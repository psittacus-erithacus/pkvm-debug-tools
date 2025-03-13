/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KVM_ARM64_DUMP_REGS_H
#define _KVM_ARM64_DUMP_REGS_H

#include <linux/kernel.h>
#include "config.h"
#include "ramlog.h"

#if HYP_DEBUG_RAMLOG
void debug_dump_csrs(void);
#else
static inline void debug_dump_csrs(void) { }
#endif

#endif /* _KVM_ARM64_DUMP_REGS_H */
