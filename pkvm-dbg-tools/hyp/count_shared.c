// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <asm/kvm_mmu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/pkvm.h>
#include <nvhe/mm.h>
#include <asm/kvm_host.h>
#include "config.h"
#include "hyp_debug.h"

#define S2_XN_SHIFT	53
#define S2_XN_MASK	(0x3UL << S2_XN_SHIFT)
#define S2AP_SHIFT	6
#define S2AP_MASK	(0x3UL << S2AP_SHIFT)
#define S2_EXEC_NONE	(0x2UL << S2_XN_SHIFT)
#define S2AP_READ	(1UL << S2AP_SHIFT)

#define tlbi_el1_ipa(va)                                   \
	do {                                                   \
		__asm__ __volatile__("mov	x20, %[vaddr]\n"       \
				     "lsr	%[vaddr], %[vaddr], #12\n"     \
				     "tlbi	ipas2e1is, %[vaddr]\n"         \
				     "mov	%[vaddr], x20\n"               \
				     :                                     \
				     : [vaddr] "r"(va)                     \
				     : "memory", "x20");                   \
	} while (0)

//struct pkvm_hyp_vm *get_vm_by_handle(pkvm_handle_t handle);

struct data_buf {
	u64 size;
	u64 start_ipa;
	u64 start_phys;
	u64 attr;
	u64 host_attr;
	u32 level;
};

struct count_shares_data {
	struct kvm_pgtable *host_pgt;
	struct data_buf data;
	u64 fail_addr;
	u32 share_count;
	bool lock;
	bool hyp;

};

struct static_data {
	pkvm_handle_t handle;
	u64 addr;
	u64 size;
	u32 share_count;
	bool lock;
};

struct host_data {
	u64 pte;
};

struct walker_data {
	u64 *ptep;
};

static struct static_data sdata;

char *parse_attrs(char *p, uint64_t attrs, uint64_t stage);

static int host_walker(const struct kvm_pgtable_visit_ctx *ctx,
		       enum kvm_pgtable_walk_flags visit)
{
	struct host_data *data = ctx->arg;

	data->pte = *ctx->ptep;
	return 1;
}

static int bit_shift(u32 level)
{
	int shift;

	switch (level) {
	case 0:
		shift = 39;
		break;
	case 1:
		shift = 30;
		break;
	case 2:
		shift = 21;
		break;
	case 3:
		shift = 12;
		break;
	default:
		shift = 0;
	}
	return shift;
}

static void clean_dbuf(struct data_buf *data)
{
	memset(data, 0, sizeof(struct data_buf));
}

static void init_dbuf(struct data_buf *data, u64 addr, u32 level,
		      kvm_pte_t *ptep, u64 host_attr)
{
	data->size = 1;
	data->level = level;
	data->start_ipa = addr;
	data->attr = (*ptep) & ~KVM_PTE_ADDR_MASK;
	data->host_attr = host_attr;
	data->start_phys = (*ptep) & KVM_PTE_ADDR_MASK;
}

static int update_dbuf(struct data_buf *data, u64 addr, u32 level,
		       kvm_pte_t *ptep, u64 host_attr)
{
	if (data->size == 0) {
		init_dbuf(data, addr, level, ptep, host_attr);
		return 1;
	}
	if ((data->attr == ((*ptep) & ~KVM_PTE_ADDR_MASK)) &&
	    (data->level == level)  &&
	    ((*ptep) & KVM_PTE_ADDR_MASK) == data->start_phys +
	     data->size * (1UL << bit_shift(level))) {
		data->size++;
		return 1;
	}
	return 0;
}
static void print_header(void)
{
	hyp_dbg_print("Count     IPA             Phys           page attributes");
	hyp_dbg_print("                host page attributes\n");
	hyp_dbg_print("                                         prv unp type\n");
}

static int print_dbuf(struct data_buf *data, bool hyp)
{
	char buff[128];
	int ret;

	ret = hyp_dbg_print("%3d pages %#015llx -> %#012llx ", data->size,
			data->start_ipa, data->start_phys);
	if (ret == -1)
		return 1;
	ret = hyp_dbg_print("%s ", parse_attrs(buff, data->attr, hyp ? 1 : 2));
	if (ret == -1)
		return 1;
	ret = hyp_dbg_print(": %s\n", parse_attrs(buff, data->host_attr, 2));
	if (ret == -1)
		return 1;
	return 0;
}

static int count_shares_walker(const struct kvm_pgtable_visit_ctx *ctx,
			       enum kvm_pgtable_walk_flags visit)

{
	u64 phys = (*ctx->ptep) & KVM_PTE_ADDR_MASK;
	struct host_data hwalker_data;
	struct count_shares_data *data = ctx->arg;
	struct kvm_pgtable_walker walker = {
		.cb	= host_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &hwalker_data,
	};

	if (phys == 0)
		return 0;

	memset(&hwalker_data, 0, sizeof(hwalker_data));
	dops->kvm_pgtable_walk(data->host_pgt, phys, 0x01000, &walker);

	if (hwalker_data.pte & KVM_PTE_ADDR_MASK) {
		data->share_count++;
		if (data->lock) {
			*ctx->ptep &= ~(S2_XN_MASK & S2AP_MASK);
			*ctx->ptep |= (S2_EXEC_NONE | S2AP_READ);

			dsb(ish);
			isb();
			tlbi_el1_ipa(phys);
			dsb(ish);
			isb();
		}

		if (update_dbuf(&data->data, ctx->addr, ctx->level, ctx->ptep,
				hwalker_data.pte))
			return 0;

		if (print_dbuf(&data->data, data->hyp)) {
			data->fail_addr = data->data.start_ipa;
			return 1;
		}
		init_dbuf(&data->data, ctx->addr, ctx->level, ctx->ptep,
			 hwalker_data.pte);
	} else {
		if (data->data.size) {
			if (print_dbuf(&data->data, data->hyp)) {
				data->fail_addr = data->data.start_ipa;
				return 1;
			}
			clean_dbuf(&data->data);
		}
	}

	return 0;
}

int count_shared(u32 id, u64 size, bool lck, bool cont)
{
	int ret;
	struct kvm_pgtable *guest_pgt;
	struct pkvm_hyp_vm *vm = NULL;
	struct count_shares_data walker_data;
	struct kvm_pgtable_walker walker = {
		.cb	= count_shares_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &walker_data,
	};

	memset(&walker_data, 0, sizeof(walker_data));
	if (!cont) {
		hyp_print("count shared size %llx\n", size);
		memset(&sdata, 0, sizeof(struct static_data));
		sdata.handle = id;
	} else {
		walker_data.share_count = sdata.share_count;
		hyp_print("continue print_mappings addr %llx, size %llx\n",
			   sdata.addr, sdata.size);
	}

	walker_data.host_pgt = dops->host_mmu->arch.mmu.pgt;

	if (sdata.handle) {
		vm = dops->pkvm_get_hyp_vm(sdata.handle);

		//vm = get_vm_by_handle(sdata.handle);
		if (!vm) {
			hyp_print("no handle\n");
			return -EINVAL;
		};
		walker_data.hyp = false;
		guest_pgt = &vm->pgt;
	} else {
		/* hyperpevisor mapping */
		walker_data.hyp = true;
		guest_pgt = dops->pkvm_pgtable;
	}

	if (!cont) {
		if (size == 0) {
			/* prints the entire memory area */
			sdata.size = (1UL << guest_pgt->ia_bits) - 1;
		} else
			sdata.size = size;
		sdata.addr = 0;
		walker_data.lock = (bool) lck;
		print_header();
	}

	ret = dops->kvm_pgtable_walk(guest_pgt, sdata.addr, sdata.size, &walker);
	if (vm)
		dops->pkvm_put_hyp_vm(vm);

	if (ret == 1) {
		/* save the input parameters for continuation */
		u64 cnt =  walker_data.fail_addr - sdata.addr;

		sdata.addr = walker_data.fail_addr;
		sdata.share_count = walker_data.share_count;
		sdata.size -= cnt;

		return 1;
	}

	ret = hyp_dbg_print("shared count %d (%d MBytes)\n",
			      walker_data.share_count,
			      walker_data.share_count / 256);

	if (ret == 1) {
		sdata.addr = walker_data.fail_addr;
		sdata.share_count = walker_data.share_count;
		sdata.size = 0;
		return 1;
	}
	return 0;
}
