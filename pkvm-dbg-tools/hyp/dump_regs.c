// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/bitops.h>

#include "config.h"
#include "dump_regs.h"
#include "hyp_debug.h"
#include "chacha.h"
#include <asm/barrier.h>
#include <asm/page-def.h>
#include <nvhe/mm.h>

#if HYP_DEBUG_RAMLOG
void debug_dump_csrs(void)
{
	hyp_ramlog_reg(TTBR0_EL2);
	hyp_ramlog_reg(TTBR0_EL1);
	hyp_ramlog_reg(TTBR1_EL1);
	hyp_ramlog_reg(ESR_EL2);
	hyp_ramlog_reg(HPFAR_EL2);
	hyp_ramlog_reg(FAR_EL2);
	hyp_ramlog_reg(VTTBR_EL2);
}
#endif
