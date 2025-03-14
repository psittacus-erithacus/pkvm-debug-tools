WORK_DIR=$(shell realpath ../..)

all: hypdbg

hypdbg: hypdbg.c
	echo $(WORK_DIR)
	$(CROSS_COMPILE)gcc -I$(HYPDRV_PATH)/ \
		--sysroot=$(TOOLDIR) \
		--no-sysroot-suffix  hypdbg.c -o hypdbg

clean:
	rm -f hypdbg
