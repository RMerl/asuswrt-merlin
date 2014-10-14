/*
 *
 * Copyright (C) 2005 Embedded Alley Solutions, Inc
 * Ported to 2.6.
 *
 * Per Hallsmark, per.hallsmark@mvista.com
 * Copyright (C) 2000, 2001 MIPS Technologies, Inc.
 * Copyright (C) 2001 Ralf Baechle
 *
 * Cleaned up and bug fixing: Pete Popov, ppopov@embeddedalley.com
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/random.h>
#include <linux/module.h>

#include <asm/io.h>
#include <int.h>
#include <uart.h>

/* default prio for interrupts */
/* first one is a no-no so therefore always prio 0 (disabled) */
static char gic_prio[PNX8550_INT_GIC_TOTINT] = {
	0, 1, 1, 1, 1, 15, 1, 1, 1, 1,	//   0 -  9
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	//  10 - 19
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	//  20 - 29
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	//  30 - 39
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	//  40 - 49
	1, 1, 1, 1, 1, 1, 1, 1, 2, 1,	//  50 - 59
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	//  60 - 69
	1			//  70
};

static void hw0_irqdispatch(int irq)
{
	/* find out which interrupt */
	irq = PNX8550_GIC_VECTOR_0 >> 3;

	if (irq == 0) {
		printk("hw0_irqdispatch: irq 0, spurious interrupt?\n");
		return;
	}
	do_IRQ(PNX8550_INT_GIC_MIN + irq);
}


static void timer_irqdispatch(int irq)
{
	irq = (0x01c0 & read_c0_config7()) >> 6;

	if (unlikely(irq == 0)) {
		printk("timer_irqdispatch: irq 0, spurious interrupt?\n");
		return;
	}

	if (irq & 0x1)
		do_IRQ(PNX8550_INT_TIMER1);
	if (irq & 0x2)
		do_IRQ(PNX8550_INT_TIMER2);
	if (irq & 0x4)
		do_IRQ(PNX8550_INT_TIMER3);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause() & ST0_IM;

	if (pending & STATUSF_IP2)
		hw0_irqdispatch(2);
	else if (pending & STATUSF_IP7) {
		if (read_c0_config7() & 0x01c0)
			timer_irqdispatch(7);
	} else
		spurious_interrupt();
}

static inline void modify_cp0_intmask(unsigned clr_mask, unsigned set_mask)
{
	unsigned long status = read_c0_status();

	status &= ~((clr_mask & 0xFF) << 8);
	status |= (set_mask & 0xFF) << 8;

	write_c0_status(status);
}

static inline void mask_gic_int(unsigned int irq_nr)
{
	/* interrupt disabled, bit 26(WE_ENABLE)=1 and bit 16(enable)=0 */
	PNX8550_GIC_REQ(irq_nr) = 1<<28; /* set priority to 0 */
}

static inline void unmask_gic_int(unsigned int irq_nr)
{
	/* set prio mask to lower four bits and enable interrupt */
	PNX8550_GIC_REQ(irq_nr) = (1<<26 | 1<<16) | (1<<28) | gic_prio[irq_nr];
}

static inline void mask_irq(struct irq_data *d)
{
	unsigned int irq_nr = d->irq;

	if ((PNX8550_INT_CP0_MIN <= irq_nr) && (irq_nr <= PNX8550_INT_CP0_MAX)) {
		modify_cp0_intmask(1 << irq_nr, 0);
	} else if ((PNX8550_INT_GIC_MIN <= irq_nr) &&
		(irq_nr <= PNX8550_INT_GIC_MAX)) {
		mask_gic_int(irq_nr - PNX8550_INT_GIC_MIN);
	} else if ((PNX8550_INT_TIMER_MIN <= irq_nr) &&
		(irq_nr <= PNX8550_INT_TIMER_MAX)) {
		modify_cp0_intmask(1 << 7, 0);
	} else {
		printk("mask_irq: irq %d doesn't exist!\n", irq_nr);
	}
}

static inline void unmask_irq(struct irq_data *d)
{
	unsigned int irq_nr = d->irq;

	if ((PNX8550_INT_CP0_MIN <= irq_nr) && (irq_nr <= PNX8550_INT_CP0_MAX)) {
		modify_cp0_intmask(0, 1 << irq_nr);
	} else if ((PNX8550_INT_GIC_MIN <= irq_nr) &&
		(irq_nr <= PNX8550_INT_GIC_MAX)) {
		unmask_gic_int(irq_nr - PNX8550_INT_GIC_MIN);
	} else if ((PNX8550_INT_TIMER_MIN <= irq_nr) &&
		(irq_nr <= PNX8550_INT_TIMER_MAX)) {
		modify_cp0_intmask(0, 1 << 7);
	} else {
		printk("mask_irq: irq %d doesn't exist!\n", irq_nr);
	}
}

int pnx8550_set_gic_priority(int irq, int priority)
{
	int gic_irq = irq-PNX8550_INT_GIC_MIN;
	int prev_priority = PNX8550_GIC_REQ(gic_irq) & 0xf;

        gic_prio[gic_irq] = priority;
	PNX8550_GIC_REQ(gic_irq) |= (0x10000000 | gic_prio[gic_irq]);

	return prev_priority;
}

static struct irq_chip level_irq_type = {
	.name =		"PNX Level IRQ",
	.irq_mask =	mask_irq,
	.irq_unmask =	unmask_irq,
};

static struct irqaction gic_action = {
	.handler =	no_action,
	.flags =	IRQF_DISABLED,
	.name =		"GIC",
};

static struct irqaction timer_action = {
	.handler =	no_action,
	.flags =	IRQF_DISABLED | IRQF_TIMER,
	.name =		"Timer",
};

void __init arch_init_irq(void)
{
	int i;
	int configPR;

	for (i = 0; i < PNX8550_INT_CP0_TOTINT; i++)
		irq_set_chip_and_handler(i, &level_irq_type, handle_level_irq);

	/* init of GIC/IPC interrupts */
	/* should be done before cp0 since cp0 init enables the GIC int */
	for (i = PNX8550_INT_GIC_MIN; i <= PNX8550_INT_GIC_MAX; i++) {
		int gic_int_line = i - PNX8550_INT_GIC_MIN;
		if (gic_int_line == 0 )
			continue;	// don't fiddle with int 0
		/*
		 * enable change of TARGET, ENABLE and ACTIVE_LOW bits
		 * set TARGET        0 to route through hw0 interrupt
		 * set ACTIVE_LOW    0 active high  (correct?)
		 *
		 * We really should setup an interrupt description table
		 * to do this nicely.
		 * Note, PCI INTA is active low on the bus, but inverted
		 * in the GIC, so to us it's active high.
		 */
		PNX8550_GIC_REQ(i - PNX8550_INT_GIC_MIN) = 0x1E000000;

		/* mask/priority is still 0 so we will not get any
		 * interrupts until it is unmasked */

		irq_set_chip_and_handler(i, &level_irq_type, handle_level_irq);
	}

	/* Priority level 0 */
	PNX8550_GIC_PRIMASK_0 = PNX8550_GIC_PRIMASK_1 = 0;

	/* Set int vector table address */
	PNX8550_GIC_VECTOR_0 = PNX8550_GIC_VECTOR_1 = 0;

	irq_set_chip_and_handler(MIPS_CPU_GIC_IRQ, &level_irq_type,
				 handle_level_irq);
	setup_irq(MIPS_CPU_GIC_IRQ, &gic_action);

	/* init of Timer interrupts */
	for (i = PNX8550_INT_TIMER_MIN; i <= PNX8550_INT_TIMER_MAX; i++)
		irq_set_chip_and_handler(i, &level_irq_type, handle_level_irq);

	/* Stop Timer 1-3 */
	configPR = read_c0_config7();
	configPR |= 0x00000038;
	write_c0_config7(configPR);

	irq_set_chip_and_handler(MIPS_CPU_TIMER_IRQ, &level_irq_type,
				 handle_level_irq);
	setup_irq(MIPS_CPU_TIMER_IRQ, &timer_action);
}

EXPORT_SYMBOL(pnx8550_set_gic_priority);
