/*
 * linux/arch/arm/mach-omap1/fpga.c
 *
 * Interrupt handler for OMAP-1510 Innovator FPGA
 *
 * Copyright (C) 2001 RidgeRun, Inc.
 * Author: Greg Lonnon <glonnon@ridgerun.com>
 *
 * Copyright (C) 2002 MontaVista Software, Inc.
 *
 * Separated FPGA interrupts from innovator1510.c and cleaned up for 2.6
 * Copyright (C) 2004 Nokia Corporation by Tony Lindrgen <tony@atomide.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <plat/fpga.h>
#include <mach/gpio.h>

static void fpga_mask_irq(unsigned int irq)
{
	irq -= OMAP_FPGA_IRQ_BASE;

	if (irq < 8)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_LO)
			      & ~(1 << irq)), OMAP1510_FPGA_IMR_LO);
	else if (irq < 16)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_HI)
			      & ~(1 << (irq - 8))), OMAP1510_FPGA_IMR_HI);
	else
		__raw_writeb((__raw_readb(INNOVATOR_FPGA_IMR2)
			      & ~(1 << (irq - 16))), INNOVATOR_FPGA_IMR2);
}


static inline u32 get_fpga_unmasked_irqs(void)
{
	return
		((__raw_readb(OMAP1510_FPGA_ISR_LO) &
		  __raw_readb(OMAP1510_FPGA_IMR_LO))) |
		((__raw_readb(OMAP1510_FPGA_ISR_HI) &
		  __raw_readb(OMAP1510_FPGA_IMR_HI)) << 8) |
		((__raw_readb(INNOVATOR_FPGA_ISR2) &
		  __raw_readb(INNOVATOR_FPGA_IMR2)) << 16);
}


static void fpga_ack_irq(unsigned int irq)
{
	/* Don't need to explicitly ACK FPGA interrupts */
}

static void fpga_unmask_irq(unsigned int irq)
{
	irq -= OMAP_FPGA_IRQ_BASE;

	if (irq < 8)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_LO) | (1 << irq)),
		     OMAP1510_FPGA_IMR_LO);
	else if (irq < 16)
		__raw_writeb((__raw_readb(OMAP1510_FPGA_IMR_HI)
			      | (1 << (irq - 8))), OMAP1510_FPGA_IMR_HI);
	else
		__raw_writeb((__raw_readb(INNOVATOR_FPGA_IMR2)
			      | (1 << (irq - 16))), INNOVATOR_FPGA_IMR2);
}

static void fpga_mask_ack_irq(unsigned int irq)
{
	fpga_mask_irq(irq);
	fpga_ack_irq(irq);
}

void innovator_fpga_IRQ_demux(unsigned int irq, struct irq_desc *desc)
{
	u32 stat;
	int fpga_irq;

	stat = get_fpga_unmasked_irqs();

	if (!stat)
		return;

	for (fpga_irq = OMAP_FPGA_IRQ_BASE;
	     (fpga_irq < OMAP_FPGA_IRQ_END) && stat;
	     fpga_irq++, stat >>= 1) {
		if (stat & 1) {
			generic_handle_irq(fpga_irq);
		}
	}
}

static struct irq_chip omap_fpga_irq_ack = {
	.name		= "FPGA-ack",
	.ack		= fpga_mask_ack_irq,
	.mask		= fpga_mask_irq,
	.unmask		= fpga_unmask_irq,
};


static struct irq_chip omap_fpga_irq = {
	.name		= "FPGA",
	.ack		= fpga_ack_irq,
	.mask		= fpga_mask_irq,
	.unmask		= fpga_unmask_irq,
};

void omap1510_fpga_init_irq(void)
{
	int i;

	__raw_writeb(0, OMAP1510_FPGA_IMR_LO);
	__raw_writeb(0, OMAP1510_FPGA_IMR_HI);
	__raw_writeb(0, INNOVATOR_FPGA_IMR2);

	for (i = OMAP_FPGA_IRQ_BASE; i < OMAP_FPGA_IRQ_END; i++) {

		if (i == OMAP1510_INT_FPGA_TS) {
			/*
			 * The touchscreen interrupt is level-sensitive, so
			 * we'll use the regular mask_ack routine for it.
			 */
			set_irq_chip(i, &omap_fpga_irq_ack);
		}
		else {
			/*
			 * All FPGA interrupts except the touchscreen are
			 * edge-sensitive, so we won't mask them.
			 */
			set_irq_chip(i, &omap_fpga_irq);
		}

		set_irq_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	/*
	 * The FPGA interrupt line is connected to GPIO13. Claim this pin for
	 * the ARM.
	 *
	 * NOTE: For general GPIO/MPUIO access and interrupts, please see
	 * gpio.[ch]
	 */
	gpio_request(13, "FPGA irq");
	gpio_direction_input(13);
	set_irq_type(gpio_to_irq(13), IRQ_TYPE_EDGE_RISING);
	set_irq_chained_handler(OMAP1510_INT_FPGA, innovator_fpga_IRQ_demux);
}

EXPORT_SYMBOL(omap1510_fpga_init_irq);
