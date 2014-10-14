/*
 * arch/arm/plat-omap/include/mach/prcm.h
 *
 * Access definations for use in OMAP24XX clock and power management
 *
 * Copyright (C) 2005 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * XXX This file is deprecated.  The PRCM is an OMAP2+-only subsystem,
 * so this file doesn't belong in plat-omap/include/plat.  Please
 * do not add anything new to this file.
 */

#ifndef __ASM_ARM_ARCH_OMAP_PRCM_H
#define __ASM_ARM_ARCH_OMAP_PRCM_H

u32 omap_prcm_get_reset_sources(void);
int omap2_cm_wait_idlest(void __iomem *reg, u32 mask, u8 idlest,
			 const char *name);

#endif



