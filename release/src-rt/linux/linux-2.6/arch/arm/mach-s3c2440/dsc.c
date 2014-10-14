/* linux/arch/arm/mach-s3c2440/dsc.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *   Ben Dooks <ben@simtec.co.uk>
 *
 * Samsung S3C2440 Drive Strength Control support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <asm/irq.h>

#include <mach/regs-gpio.h>
#include <mach/regs-dsc.h>

#include <plat/cpu.h>
#include <plat/s3c244x.h>

int s3c2440_set_dsc(unsigned int pin, unsigned int value)
{
	void __iomem *base;
	unsigned long val;
	unsigned long flags;
	unsigned long mask;

	base = (pin & S3C2440_SELECT_DSC1) ? S3C2440_DSC1 : S3C2440_DSC0;
	mask = 3 << S3C2440_DSC_GETSHIFT(pin);

	local_irq_save(flags);

	val = __raw_readl(base);
	val &= ~mask;
	val |= value & mask;
	__raw_writel(val, base);

	local_irq_restore(flags);
	return 0;
}

EXPORT_SYMBOL(s3c2440_set_dsc);
