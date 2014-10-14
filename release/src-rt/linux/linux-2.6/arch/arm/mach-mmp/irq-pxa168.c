/*
 *  linux/arch/arm/mach-mmp/irq.c
 *
 *  Generic IRQ handling, GPIO IRQ demultiplexing, etc.
 *
 *  Author:	Bin Yang <bin.yang@marvell.com>
 *  Created:	Sep 30, 2008
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/regs-icu.h>

#include "common.h"

#define IRQ_ROUTE_TO_AP		(ICU_INT_CONF_AP_INT | ICU_INT_CONF_IRQ)

#define PRIORITY_DEFAULT	0x1
#define PRIORITY_NONE		0x0	/* means IRQ disabled */

static void icu_mask_irq(struct irq_data *d)
{
	__raw_writel(PRIORITY_NONE, ICU_INT_CONF(d->irq));
}

static void icu_unmask_irq(struct irq_data *d)
{
	__raw_writel(IRQ_ROUTE_TO_AP | PRIORITY_DEFAULT, ICU_INT_CONF(d->irq));
}

static struct irq_chip icu_irq_chip = {
	.name		= "icu_irq",
	.irq_ack	= icu_mask_irq,
	.irq_mask	= icu_mask_irq,
	.irq_unmask	= icu_unmask_irq,
};

void __init icu_init_irq(void)
{
	int irq;

	for (irq = 0; irq < 64; irq++) {
		icu_mask_irq(irq_get_irq_data(irq));
		irq_set_chip_and_handler(irq, &icu_irq_chip, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
}
