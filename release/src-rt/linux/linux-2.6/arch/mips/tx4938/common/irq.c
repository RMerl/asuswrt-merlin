/*
 * linux/arch/mips/tx4938/common/irq.c
 *
 * Common tx4938 irq handler
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 * 2003-2005 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * Support for TX4938 in 2.6 - Manish Lachwani (mlachwani@mvista.com)
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/irq.h>
#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/wbflush.h>
#include <asm/tx4938/rbtx4938.h>

/**********************************************************************************/
/* Forwad definitions for all pic's                                               */
/**********************************************************************************/

static void tx4938_irq_cp0_enable(unsigned int irq);
static void tx4938_irq_cp0_disable(unsigned int irq);

static void tx4938_irq_pic_enable(unsigned int irq);
static void tx4938_irq_pic_disable(unsigned int irq);

/**********************************************************************************/
/* Kernel structs for all pic's                                                   */
/**********************************************************************************/

#define TX4938_CP0_NAME "TX4938-CP0"
static struct irq_chip tx4938_irq_cp0_type = {
	.name = TX4938_CP0_NAME,
	.ack = tx4938_irq_cp0_disable,
	.mask = tx4938_irq_cp0_disable,
	.mask_ack = tx4938_irq_cp0_disable,
	.unmask = tx4938_irq_cp0_enable,
};

#define TX4938_PIC_NAME "TX4938-PIC"
static struct irq_chip tx4938_irq_pic_type = {
	.name = TX4938_PIC_NAME,
	.ack = tx4938_irq_pic_disable,
	.mask = tx4938_irq_pic_disable,
	.mask_ack = tx4938_irq_pic_disable,
	.unmask = tx4938_irq_pic_enable,
};

static struct irqaction tx4938_irq_pic_action = {
	.handler = no_action,
	.flags = 0,
	.mask = CPU_MASK_NONE,
	.name = TX4938_PIC_NAME
};

/**********************************************************************************/
/* Functions for cp0                                                              */
/**********************************************************************************/

#define tx4938_irq_cp0_mask(irq) ( 1 << ( irq-TX4938_IRQ_CP0_BEG+8 ) )

static void __init
tx4938_irq_cp0_init(void)
{
	int i;

	for (i = TX4938_IRQ_CP0_BEG; i <= TX4938_IRQ_CP0_END; i++)
		set_irq_chip_and_handler(i, &tx4938_irq_cp0_type,
					 handle_level_irq);
}

static void
tx4938_irq_cp0_enable(unsigned int irq)
{
	set_c0_status(tx4938_irq_cp0_mask(irq));
}

static void
tx4938_irq_cp0_disable(unsigned int irq)
{
	clear_c0_status(tx4938_irq_cp0_mask(irq));
}

/**********************************************************************************/
/* Functions for pic                                                              */
/**********************************************************************************/

u32
tx4938_irq_pic_addr(int irq)
{
	/* MVMCP -- need to formulize this */
	irq -= TX4938_IRQ_PIC_BEG;

	switch (irq) {
	case 17:
	case 16:
	case 1:
	case 0:{
			return (TX4938_MKA(TX4938_IRC_IRLVL0));
		}
	case 19:
	case 18:
	case 3:
	case 2:{
			return (TX4938_MKA(TX4938_IRC_IRLVL1));
		}
	case 21:
	case 20:
	case 5:
	case 4:{
			return (TX4938_MKA(TX4938_IRC_IRLVL2));
		}
	case 23:
	case 22:
	case 7:
	case 6:{
			return (TX4938_MKA(TX4938_IRC_IRLVL3));
		}
	case 25:
	case 24:
	case 9:
	case 8:{
			return (TX4938_MKA(TX4938_IRC_IRLVL4));
		}
	case 27:
	case 26:
	case 11:
	case 10:{
			return (TX4938_MKA(TX4938_IRC_IRLVL5));
		}
	case 29:
	case 28:
	case 13:
	case 12:{
			return (TX4938_MKA(TX4938_IRC_IRLVL6));
		}
	case 31:
	case 30:
	case 15:
	case 14:{
			return (TX4938_MKA(TX4938_IRC_IRLVL7));
		}
	}

	return 0;
}

u32
tx4938_irq_pic_mask(int irq)
{
	/* MVMCP -- need to formulize this */
	irq -= TX4938_IRQ_PIC_BEG;

	switch (irq) {
	case 31:
	case 29:
	case 27:
	case 25:
	case 23:
	case 21:
	case 19:
	case 17:{
			return (0x07000000);
		}
	case 30:
	case 28:
	case 26:
	case 24:
	case 22:
	case 20:
	case 18:
	case 16:{
			return (0x00070000);
		}
	case 15:
	case 13:
	case 11:
	case 9:
	case 7:
	case 5:
	case 3:
	case 1:{
			return (0x00000700);
		}
	case 14:
	case 12:
	case 10:
	case 8:
	case 6:
	case 4:
	case 2:
	case 0:{
			return (0x00000007);
		}
	}
	return 0x00000000;
}

static void
tx4938_irq_pic_modify(unsigned pic_reg, unsigned clr_bits, unsigned set_bits)
{
	unsigned long val = 0;

	val = TX4938_RD(pic_reg);
	val &= (~clr_bits);
	val |= (set_bits);
	TX4938_WR(pic_reg, val);
	mmiowb();
	TX4938_RD(pic_reg);
}

static void __init
tx4938_irq_pic_init(void)
{
	int i;

	for (i = TX4938_IRQ_PIC_BEG; i <= TX4938_IRQ_PIC_END; i++)
		set_irq_chip_and_handler(i, &tx4938_irq_pic_type,
					 handle_level_irq);

	setup_irq(TX4938_IRQ_NEST_PIC_ON_CP0, &tx4938_irq_pic_action);

	TX4938_WR(0xff1ff640, 0x6);	/* irq level mask -- only accept hightest */
	TX4938_WR(0xff1ff600, TX4938_RD(0xff1ff600) | 0x1);	/* irq enable */
}

static void
tx4938_irq_pic_enable(unsigned int irq)
{
	tx4938_irq_pic_modify(tx4938_irq_pic_addr(irq), 0,
			      tx4938_irq_pic_mask(irq));
}

static void
tx4938_irq_pic_disable(unsigned int irq)
{
	tx4938_irq_pic_modify(tx4938_irq_pic_addr(irq),
			      tx4938_irq_pic_mask(irq), 0);
}

/**********************************************************************************/
/* Main init functions                                                            */
/**********************************************************************************/

void __init
tx4938_irq_init(void)
{
	tx4938_irq_cp0_init();
	tx4938_irq_pic_init();
}

int
tx4938_irq_nested(void)
{
	int sw_irq = 0;
	u32 level2;

	level2 = TX4938_RD(0xff1ff6a0);
	if ((level2 & 0x10000) == 0) {
		level2 &= 0x1f;
		sw_irq = TX4938_IRQ_PIC_BEG + level2;
		if (sw_irq == 26) {
			{
				extern int toshiba_rbtx4938_irq_nested(int sw_irq);
				sw_irq = toshiba_rbtx4938_irq_nested(sw_irq);
			}
		}
	}

	wbflush();
	return sw_irq;
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_cause() & read_c0_status();

	if (pending & STATUSF_IP7)
		do_IRQ(TX4938_IRQ_CPU_TIMER);
	else if (pending & STATUSF_IP2) {
		int irq = tx4938_irq_nested();
		if (irq)
			do_IRQ(irq);
		else
			spurious_interrupt();
	} else if (pending & STATUSF_IP1)
		do_IRQ(TX4938_IRQ_USER1);
	else if (pending & STATUSF_IP0)
		do_IRQ(TX4938_IRQ_USER0);
}
