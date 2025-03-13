// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "hypdrv_ioctl.h"
#include "hyp/hyp_calls.h"
#include <asm/kvm_pkvm_module.h>

MODULE_DESCRIPTION("Pkvm debug tools module");
MODULE_LICENSE("GPL v2");

static int major;
static int dopen;
static struct shared_buffer *buffer;
extern int pkvm_mod_dbg_tools;

int __kvm_nvhe_pkvm_driver_hyp_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_pkvm_driver_hyp_hvc(struct user_pt_regs *regs);

int pkvm_mod_dbg_tools;

static int device_open(struct inode *inode, struct file *filp)
{

	int ret;
	int i;

	u8 *p = (u8 *) buffer;
	if (dopen)
		return -EBUSY;

	dopen = 1;
	for (i = 0; i < DBG_BUFF_SIZE / 0x1000; i++) {
		ret = kvm_call_hyp_nvhe(__pkvm_host_share_hyp,
				virt_to_pfn(&p[i * 0x1000]), 1);
		/* TODO; make add cleanup if the call fails */
	}

	buffer->ri = 0;
	buffer->wi = 0;
	buffer->size = DBG_BUFF_SIZE - sizeof(struct shared_buffer);;

	if (ret)
		return ret;
	ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools, HYP_DBG_CALL_INIT,
				virt_to_pfn(p), DBG_BUFF_SIZE, 0, 0);
	return ret;
}

static int device_release(struct inode *inode, struct file *filp)
{

	int ret;
	int i;
	u8 *p = (u8 *) buffer;

	ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools, HYP_DBG_CALL_DEINIT, 0, 0, 0, 0);
	for (i = 0; i < DBG_BUFF_SIZE / 0x1000; i++) {
		ret = kvm_call_hyp_nvhe(__pkvm_host_unshare_hyp,
				virt_to_pfn(&p[i * 0x1000]), 1);
	}
	dopen = 0;
	return ret;
}


static ssize_t
device_read(struct file *filp, char *obuf, size_t length, loff_t *off)
{
	struct shared_buffer *p = buffer;
	int ret;
	int rdy = 0;
	u32 icnt;
	u32 ocnt;
	u32 cnt = 0;
	u32 ri = p->ri;
	u32 wi = p->wi;
	//u32 tmp;

	//printk("read %lx %x\n",length, rdy);
	while (length && !rdy) {
		icnt = (wi + p->size - ri) % p->size;
		if (icnt < (p->size >> 4)) {
			/* update ring buffer content */
			ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools,
						HYP_DBG_CALL_READ_BUFFER,
						0, 0, 0, 0);
			if (ret != 1)
				rdy = 1;

			ri = p->ri;
			wi = p->wi;
			icnt = (wi + p->size - ri) % p->size;
		}
		if (icnt == 0)
			break;

		ocnt = icnt > length ? length : icnt;

		if (ri + ocnt > p->size) {
			u32 tmp =  p->size - ri;

			ret = copy_to_user(obuf, &p->data[ri], tmp);
			ret = copy_to_user(&obuf[tmp],
					   &p->data[(ri + tmp) % p->size],
					   ocnt - tmp);
		} else
			ret = copy_to_user(obuf, &p->data[ri], ocnt);

		ri = (ri + ocnt) % p->size;
		p->ri = ri;
		length -= ocnt;
		obuf += ocnt;
		cnt += ocnt;
	}

	return cnt;
}

static long
device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	struct  ioctl_params *p = 0;
	int ret = -ENOTSUPP;

	p = kmalloc(sizeof(struct ioctl_params), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	ret = copy_from_user(p, argp, sizeof(struct ioctl_params));
	if (ret) {
		ret = -EIO;
		goto err;
	}

	switch (cmd) {
	case HYPDBG_PRINT_S2_MAPPING:
		ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools, HYP_DBG_CALL_COUNT_SHARED,
					p->id, p->addr, p->size, 0);
		break;

	case HYPDBG_COUNT_SHARED_S2_MAPPING:
		ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools, HYP_DBG_CALL_PRINT_S2,
				    p->id, p->size, p->lock, 0);
		break;

	case HYPDBG_PRINT_RAMLOG:
		ret = pkvm_el2_mod_call(pkvm_mod_dbg_tools, HYP_DBG_CALL_RAMLOG,
					1, 0, 0, 0);
		break;
	default:
		WARN(1, "HYPDRV: unknown ioctl: 0x%x\n", cmd);
	}

	if (ret == 1) {
		/* No errors */
		ret = 0;
	}
err:
	if (p)
		kfree(p);

	return ret;
}


static const struct file_operations fops = {
	.read = device_read,
	.open = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
};

static int __init pkvm_driver_init(void)
{
	unsigned long token;
	int ret;

	pr_info("HYPDBG hypervisor debugger driver\n");

	ret = pkvm_load_el2_module(__kvm_nvhe_pkvm_driver_hyp_init, &token);
	if (ret)
		return ret;
	ret = pkvm_register_el2_mod_call(__kvm_nvhe_pkvm_driver_hyp_hvc, token);
	if (ret < 0)
		return ret;

	pkvm_mod_dbg_tools = ret;
	major = register_chrdev(0, DEVICE_NAME, &fops);

	if (major < 0) {
		pr_err("HYPDBG: register_chrdev failed with %d\n", major);
		return major;
	}

	pr_info("HYPDBG mknod /dev/%s c %d 0\n", DEVICE_NAME, major);
	buffer = alloc_pages_exact(DBG_BUFF_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	return 0;

}

void cleanup_module(void)
{
	pr_info("hyp cleanup\n");
	if (buffer)
		free_pages_exact(buffer, DBG_BUFF_SIZE);

	buffer = 0;
	if (major > 0)
		unregister_chrdev(major, DEVICE_NAME);
	major = 0;
}
module_init(pkvm_driver_init);
