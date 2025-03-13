#ifndef __HYPDRV_IOCTL__
#define __HYPDRV_IOCTL__

#define DBG_BUFF_SIZE (0x4000)
#define DEVICE_NAME "hypdbg"

struct ioctl_params {
	u32 dlen;
	u32 id;
	u64 addr;
	u64 size;
	u8  lock;
	u8  dump;
};


#define IOCTL_PRINT_S2_MAPPING		1
#define IOCTL_COUNT_SHARED		2
#define IOCTL_RAMLOG			3

#define MAGIC 0xDE
#define HYPDBG_COUNT_SHARED_S2_MAPPING \
	_IOW(MAGIC, IOCTL_COUNT_SHARED, struct ioctl_params)
#define HYPDBG_PRINT_S2_MAPPING \
	_IOW(MAGIC, IOCTL_PRINT_S2_MAPPING, struct ioctl_params)
#define HYPDBG_PRINT_RAMLOG \
		_IOW(MAGIC, IOCTL_RAMLOG, struct ioctl_params)
#endif // __HYPDRV_IOCTÖL__
