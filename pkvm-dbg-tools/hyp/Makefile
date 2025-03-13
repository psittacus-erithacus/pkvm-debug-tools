hyp-obj-y := el2_main.o hyp-dbg.o print_mappings.o count_shared.o print.o strlen.o subr_prf.o
hyp-obj-y += parse_attr.o ramlog.o chacha.o dump_regs.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/