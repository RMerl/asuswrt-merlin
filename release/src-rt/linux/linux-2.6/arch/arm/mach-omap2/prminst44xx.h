/*
 * OMAP4 Power/Reset Management (PRM) function prototypes
 *
 * Copyright (C) 2010 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_ASM_MACH_OMAP2_PRMINST44XX_H
#define __ARCH_ASM_MACH_OMAP2_PRMINST44XX_H

/*
 * In an ideal world, we would not export these low-level functions,
 * but this will probably take some time to fix properly
 */
extern u32 omap4_prminst_read_inst_reg(u8 part, s16 inst, u16 idx);
extern void omap4_prminst_write_inst_reg(u32 val, u8 part, s16 inst, u16 idx);
extern u32 omap4_prminst_rmw_inst_reg_bits(u32 mask, u32 bits, u8 part,
					   s16 inst, s16 idx);

extern void omap4_prm_global_warm_sw_reset(void);

#endif
