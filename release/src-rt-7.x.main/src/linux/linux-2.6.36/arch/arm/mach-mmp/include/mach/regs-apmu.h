/*
 * linux/arch/arm/mach-mmp/include/mach/regs-apmu.h
 *
 *   Application Subsystem Power Management Unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_REGS_APMU_H
#define __ASM_MACH_REGS_APMU_H

#include <mach/addr-map.h>

#define APMU_VIRT_BASE	(AXI_VIRT_BASE + 0x82800)
#define APMU_REG(x)	(APMU_VIRT_BASE + (x))

/* Clock Reset Control */
#define APMU_IRE	APMU_REG(0x048)
#define APMU_LCD	APMU_REG(0x04c)
#define APMU_CCIC	APMU_REG(0x050)
#define APMU_SDH0	APMU_REG(0x054)
#define APMU_SDH1	APMU_REG(0x058)
#define APMU_USB	APMU_REG(0x05c)
#define APMU_NAND	APMU_REG(0x060)
#define APMU_DMA	APMU_REG(0x064)
#define APMU_GEU	APMU_REG(0x068)
#define APMU_BUS	APMU_REG(0x06c)

#define APMU_FNCLK_EN	(1 << 4)
#define APMU_AXICLK_EN	(1 << 3)
#define APMU_FNRST_DIS	(1 << 1)
#define APMU_AXIRST_DIS	(1 << 0)

#endif /* __ASM_MACH_REGS_APMU_H */
