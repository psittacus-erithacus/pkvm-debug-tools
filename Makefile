
pkvm-dbg-tools:
	make -C pkvm-dbg-tools KERNEL_DIR=$(KERNEL_DIR)	pkvm-dbg-tools

pkvm-modules: pkvm-dbg-tools

initramfs: pkvm-modules
	make -C pkvm-dbg-tools  MOD_PATH=$(CURDIR)/initramfs/root install
	make -C initramfs UBUNTU_DIR=$(UBUNTU_DIR) KERNEL_DIR=$(KERNEL_DIR) initramfs

install:
	make -C initramfs install OUT_IMAGE=$(OUT_IMAGE)


clean:
	make -C pkvm-dbg-tools KERNEL_DIR=$(KERNEL_DIR) clean
	make -C initramfs KERNEL_DIR=$(KERNEL_DIR) clean

.PHONY: pkvm-modules pkvm-dbg-tools clean initramfs
