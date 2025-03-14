
pkvm-dbg-tools:
	make -C pkvm-dbg-tools KERNEL_DIR=$(KERNEL_DIR) pkvm-dbg-tools

hypdbg:
	make -C utils/hypdbg hypdbg HYPDRV_PATH=$(CURDIR)/pkvm-dbg-tools
	
hypdbgrs:
	make -C utils/hypdbgrs all

initramfs: pkvm-dbg-tools
	make -C pkvm-dbg-tools  MOD_PATH=$(CURDIR)/initramfs/root install
	make -C initramfs UBUNTU_DIR=$(UBUNTU_DIR) KERNEL_DIR=$(KERNEL_DIR) initramfs

install:
	make -C initramfs install OUT_IMAGE=$(OUT_IMAGE)

clean:
	make -C pkvm-dbg-tools KERNEL_DIR=$(KERNEL_DIR) clean
	make -C initramfs KERNEL_DIR=$(KERNEL_DIR) clean
	make -C utils/hypdbg clean
	make -C utils/hypdbgrs clean

.PHONY: pkvm-dbg-tools hypdbg hypdbgrs initramfs install clean
