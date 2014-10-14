/*
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <loongson.h>
/*
 * the first level int-handler will jump here if it is a bonito irq
 */
void bonito_irqdispatch(void)
{
	u32 int_status;
	int i;

	/* workaround the IO dma problem: let cpu looping to allow DMA finish */
	int_status = LOONGSON_INTISR;
	while (int_status & (1 << 10)) {
		udelay(1);
		int_status = LOONGSON_INTISR;
	}

	/* Get pending sources, masked by current enables */
	int_status = LOONGSON_INTISR & LOONGSON_INTEN;

	if (int_status) {
		i = __ffs(int_status);
		do_IRQ(LOONGSON_IRQ_BASE + i);
	}
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending;

	pending = read_c0_cause() & read_c0_status() & ST0_IM;

	/* machine-specific plat_irq_dispatch */
	mach_irq_dispatch(pending);
}

void __init arch_init_irq(void)
{
	/*
	 * Clear all of the interrupts while we change the able around a bit.
	 * int-handler is not on bootstrap
	 */
	clear_c0_status(ST0_IM | ST0_BEV);

	/* no steer */
	LOONGSON_INTSTEER = 0;

	/*
	 * Mask out all interrupt by writing "1" to all bit position in
	 * the interrupt reset reg.
	 */
	LOONGSON_INTENCLR = ~0;

	/* machine specific irq init */
	mach_init_irq();
}
