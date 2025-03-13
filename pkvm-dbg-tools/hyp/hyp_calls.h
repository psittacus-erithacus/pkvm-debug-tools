/* SPDX-License-Identifier:q GPL-2.0-only */

#ifndef __MOD_ARM64_KVM_HYP_HYP_CALLS_H__
#define __MOD_ARM64_KVM_HYP_HYP_CALLS_H__

#define HYP_DBG_CALL_INIT		0x0
#define HYP_DBG_CALL_DEINIT		0x1
#define HYP_DBG_CALL_READ_BUFFER	0x2

#define HYP_DBG_CALL_PRINT_S2 		0x11
#define HYP_DBG_CALL_COUNT_SHARED	0x12
#define HYP_DBG_CALL_RAMLOG		0x13

struct shared_buffer {
	u32 size;
	u32 ri;
	u32 wi;
	u8 data[];
};

#endif
