/*
 * OMAP2/3 CM module functions
 *
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include <asm/atomic.h>

#include <plat/common.h>

#include "cm.h"
#include "cm-regbits-24xx.h"
#include "cm-regbits-34xx.h"

static const u8 cm_idlest_offs[] = {
	CM_IDLEST1, CM_IDLEST2, OMAP2430_CM_IDLEST3
};

int omap2_cm_wait_module_ready(s16 prcm_mod, u8 idlest_id, u8 idlest_shift)
{
	int ena = 0, i = 0;
	u8 cm_idlest_reg;
	u32 mask;

	if (!idlest_id || (idlest_id > ARRAY_SIZE(cm_idlest_offs)))
		return -EINVAL;

	cm_idlest_reg = cm_idlest_offs[idlest_id - 1];

	mask = 1 << idlest_shift;

	if (cpu_is_omap24xx())
		ena = mask;
	else if (cpu_is_omap34xx())
		ena = 0;
	else
		BUG();

	omap_test_timeout(((cm_read_mod_reg(prcm_mod, cm_idlest_reg) & mask) == ena),
			  MAX_MODULE_READY_TIME, i);

	return (i < MAX_MODULE_READY_TIME) ? 0 : -EBUSY;
}
