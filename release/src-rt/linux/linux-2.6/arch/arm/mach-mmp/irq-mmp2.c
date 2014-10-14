/*
 *  linux/arch/arm/mach-mmp/irq-mmp2.c
 *
 *  Generic IRQ handling, GPIO IRQ demultiplexing, etc.
 *
 *  Author:	Haojian Zhuang <haojian.zhuang@marvell.com>
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
#include <mach/mmp2.h>

#include "common.h"

static void icu_mask_irq(struct irq_data *d)
{
	uint32_t r = __raw_readl(ICU_INT_CONF(d->irq));

	r &= ~ICU_INT_ROUTE_PJ4_IRQ;
	__raw_writel(r, ICU_INT_CONF(d->irq));
}

static void icu_unmask_irq(struct irq_data *d)
{
	uint32_t r = __raw_readl(ICU_INT_CONF(d->irq));

	r |= ICU_INT_ROUTE_PJ4_IRQ;
	__raw_writel(r, ICU_INT_CONF(d->irq));
}

static struct irq_chip icu_irq_chip = {
	.name		= "icu_irq",
	.irq_mask	= icu_mask_irq,
	.irq_mask_ack	= icu_mask_irq,
	.irq_unmask	= icu_unmask_irq,
};

static void pmic_irq_ack(struct irq_data *d)
{
	if (d->irq == IRQ_MMP2_PMIC)
		mmp2_clear_pmic_int();
}

#define SECOND_IRQ_MASK(_name_, irq_base, prefix)			\
static void _name_##_mask_irq(struct irq_data *d)			\
{									\
	uint32_t r;							\
	r = __raw_readl(prefix##_MASK) | (1 << (d->irq - irq_base));	\
	__raw_writel(r, prefix##_MASK);					\
}

#define SECOND_IRQ_UNMASK(_name_, irq_base, prefix)			\
static void _name_##_unmask_irq(struct irq_data *d)			\
{									\
	uint32_t r;							\
	r = __raw_readl(prefix##_MASK) & ~(1 << (d->irq - irq_base));	\
	__raw_writel(r, prefix##_MASK);					\
}

#define SECOND_IRQ_DEMUX(_name_, irq_base, prefix)			\
static void _name_##_irq_demux(unsigned int irq, struct irq_desc *desc)	\
{									\
	unsigned long status, mask, n;					\
	mask = __raw_readl(prefix##_MASK);				\
	while (1) {							\
		status = __raw_readl(prefix##_STATUS) & ~mask;		\
		if (status == 0)					\
			break;						\
		n = find_first_bit(&status, BITS_PER_LONG);		\
		while (n < BITS_PER_LONG) {				\
			generic_handle_irq(irq_base + n);		\
			n = find_next_bit(&status, BITS_PER_LONG, n+1);	\
		}							\
	}								\
}

#define SECOND_IRQ_CHIP(_name_, irq_base, prefix)			\
SECOND_IRQ_MASK(_name_, irq_base, prefix)				\
SECOND_IRQ_UNMASK(_name_, irq_base, prefix)				\
SECOND_IRQ_DEMUX(_name_, irq_base, prefix)				\
static struct irq_chip _name_##_irq_chip = {				\
	.name		= #_name_,					\
	.irq_mask	= _name_##_mask_irq,				\
	.irq_unmask	= _name_##_unmask_irq,				\
}

SECOND_IRQ_CHIP(pmic, IRQ_MMP2_PMIC_BASE, MMP2_ICU_INT4);
SECOND_IRQ_CHIP(rtc,  IRQ_MMP2_RTC_BASE,  MMP2_ICU_INT5);
SECOND_IRQ_CHIP(twsi, IRQ_MMP2_TWSI_BASE, MMP2_ICU_INT17);
SECOND_IRQ_CHIP(misc, IRQ_MMP2_MISC_BASE, MMP2_ICU_INT35);
SECOND_IRQ_CHIP(ssp,  IRQ_MMP2_SSP_BASE,  MMP2_ICU_INT51);

static void init_mux_irq(struct irq_chip *chip, int start, int num)
{
	int irq;

	for (irq = start; num > 0; irq++, num--) {
		struct irq_data *d = irq_get_irq_data(irq);

		/* mask and clear the IRQ */
		chip->irq_mask(d);
		if (chip->irq_ack)
			chip->irq_ack(d);

		irq_set_chip(irq, chip);
		set_irq_flags(irq, IRQF_VALID);
		irq_set_handler(irq, handle_level_irq);
	}
}

void __init mmp2_init_icu(void)
{
	int irq;

	for (irq = 0; irq < IRQ_MMP2_MUX_BASE; irq++) {
		icu_mask_irq(irq_get_irq_data(irq));
		irq_set_chip(irq, &icu_irq_chip);
		set_irq_flags(irq, IRQF_VALID);

		switch (irq) {
		case IRQ_MMP2_PMIC_MUX:
		case IRQ_MMP2_RTC_MUX:
		case IRQ_MMP2_TWSI_MUX:
		case IRQ_MMP2_MISC_MUX:
		case IRQ_MMP2_SSP_MUX:
			break;
		default:
			irq_set_handler(irq, handle_level_irq);
			break;
		}
	}

	/* NOTE: IRQ_MMP2_PMIC requires the PMIC MFPR register
	 * to be written to clear the interrupt
	 */
	pmic_irq_chip.irq_ack = pmic_irq_ack;

	init_mux_irq(&pmic_irq_chip, IRQ_MMP2_PMIC_BASE, 2);
	init_mux_irq(&rtc_irq_chip, IRQ_MMP2_RTC_BASE, 2);
	init_mux_irq(&twsi_irq_chip, IRQ_MMP2_TWSI_BASE, 5);
	init_mux_irq(&misc_irq_chip, IRQ_MMP2_MISC_BASE, 15);
	init_mux_irq(&ssp_irq_chip, IRQ_MMP2_SSP_BASE, 2);

	irq_set_chained_handler(IRQ_MMP2_PMIC_MUX, pmic_irq_demux);
	irq_set_chained_handler(IRQ_MMP2_RTC_MUX, rtc_irq_demux);
	irq_set_chained_handler(IRQ_MMP2_TWSI_MUX, twsi_irq_demux);
	irq_set_chained_handler(IRQ_MMP2_MISC_MUX, misc_irq_demux);
	irq_set_chained_handler(IRQ_MMP2_SSP_MUX, ssp_irq_demux);
}
