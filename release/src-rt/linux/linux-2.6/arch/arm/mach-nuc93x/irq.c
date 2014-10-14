/*
 * linux/arch/arm/mach-nuc93x/irq.c
 *
 * Copyright (c) 2008 Nuvoton technology corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/ptrace.h>
#include <linux/sysdev.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/regs-irq.h>

static void nuc93x_irq_mask(struct irq_data *d)
{
	__raw_writel(1 << d->irq, REG_AIC_MDCR);
}

/*
 * By the w90p910 spec,any irq,only write 1
 * to REG_AIC_EOSCR for ACK
 */

static void nuc93x_irq_ack(struct irq_data *d)
{
	__raw_writel(0x01, REG_AIC_EOSCR);
}

static void nuc93x_irq_unmask(struct irq_data *d)
{
	__raw_writel(1 << d->irq, REG_AIC_MECR);

}

static struct irq_chip nuc93x_irq_chip = {
	.irq_ack	= nuc93x_irq_ack,
	.irq_mask	= nuc93x_irq_mask,
	.irq_unmask	= nuc93x_irq_unmask,
};

void __init nuc93x_init_irq(void)
{
	int irqno;

	__raw_writel(0xFFFFFFFE, REG_AIC_MDCR);

	for (irqno = IRQ_WDT; irqno <= NR_IRQS; irqno++) {
		irq_set_chip_and_handler(irqno, &nuc93x_irq_chip,
					 handle_level_irq);
		set_irq_flags(irqno, IRQF_VALID);
	}
}
