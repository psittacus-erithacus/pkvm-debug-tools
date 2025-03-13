// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <asm/kvm_pkvm_module.h>
#include "config.h"
#include "hyp_debug.h"
#include "ramlog.h"

#define hyp_pfn_to_phys(pfn)	((phys_addr_t)((pfn) << PAGE_SHIFT))

struct shared_buffer *dbg_buffer;
u64 scmd;

int init_dbg(u64 pfn, u64 size)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64) ops->hyp_va(host_addr);

	hyp_print("shared_mem %llx size %llx\n", hyp_addr, size);
	ret = ops->pin_shared_mem((void *) hyp_addr, (void *)(hyp_addr + size));
	if (ret)
		return ret;

	dbg_buffer = (struct shared_buffer *) hyp_addr;
	scmd = 0;
	return 0;
}

/**
 *  Uninitialise shared RAM for hyp-debugger
 *  @param pfn the start page of shared memory
 *  @return int 0
 */
int deinit_dbg(void)
{
	u64 to = (u64) dbg_buffer + dbg_buffer->size;

	ops->unpin_shared_mem(dbg_buffer, (void *)to);
	dbg_buffer = 0;
	scmd = 0;
	return 0;
}

int read_buffer(void)
{
	int ret;

	if (!dbg_buffer)
		return 0;

	switch (scmd) {
	case HYP_DBG_CALL_PRINT_S2:
		ret = print_mappings(0, 0, 0, true);
		break;
	case HYP_DBG_CALL_COUNT_SHARED:
		ret = count_shared(0, 0, 0, true);
		break;

	case HYP_DBG_CALL_RAMLOG:
		ret = output_rlog(0, true);
		break;
	default:
		ret = 0;
		break;
	}
	if (ret == 0)
		scmd = 0;
	return ret;
}

u64 hyp_dbg(u64 cmd, u64 param1, u64 param2, u64 param3, u64 param4)
{

	u64 ret = 0;

	hyp_print("hyp_cmd (%llx) %llx %llx %llx %llx\n", cmd,
			param1, param2, param3, param4);

	switch (cmd) {
	case HYP_DBG_CALL_INIT:
		init_dbg(param1, param2);
		break;
	case HYP_DBG_CALL_DEINIT:
		deinit_dbg();
		break;
	case HYP_DBG_CALL_READ_BUFFER:
		ret = read_buffer();
		break;
	case HYP_DBG_CALL_PRINT_S2:
		ret = print_mappings(param1, param2, param3, false);
		if (ret == 1)
			scmd = cmd;
		break;
	case HYP_DBG_CALL_COUNT_SHARED:
		ret = count_shared(param1, param2, param3, false);
		if (ret == 1)
			scmd = cmd;
		break;
	case HYP_DBG_CALL_RAMLOG:
		output_rlog(param1, false);
		if (ret == 1)
			scmd = cmd;
		break;
	}
	return ret;
}

