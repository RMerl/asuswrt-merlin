/*
 *  Copyright (C) NEC Electronics Corporation 2004-2006
 *
 *  This file is based on the arch/mips/ddb5xxx/ddb5477/irq.c
 *
 *	Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/delay.h>

#include <asm/irq_cpu.h>
#include <asm/system.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>

#include <asm/emma/emma2rh.h>

static void emma2rh_irq_enable(unsigned int irq)
{
	u32 reg_value;
	u32 reg_bitmask;
	u32 reg_index;

	irq -= EMMA2RH_IRQ_BASE;

	reg_index = EMMA2RH_BHIF_INT_EN_0 +
		    (EMMA2RH_BHIF_INT_EN_1 - EMMA2RH_BHIF_INT_EN_0) * (irq / 32);
	reg_value = emma2rh_in32(reg_index);
	reg_bitmask = 0x1 << (irq % 32);
	emma2rh_out32(reg_index, reg_value | reg_bitmask);
}

static void emma2rh_irq_disable(unsigned int irq)
{
	u32 reg_value;
	u32 reg_bitmask;
	u32 reg_index;

	irq -= EMMA2RH_IRQ_BASE;

	reg_index = EMMA2RH_BHIF_INT_EN_0 +
		    (EMMA2RH_BHIF_INT_EN_1 - EMMA2RH_BHIF_INT_EN_0) * (irq / 32);
	reg_value = emma2rh_in32(reg_index);
	reg_bitmask = 0x1 << (irq % 32);
	emma2rh_out32(reg_index, reg_value & ~reg_bitmask);
}

struct irq_chip emma2rh_irq_controller = {
	.name = "emma2rh_irq",
	.ack = emma2rh_irq_disable,
	.mask = emma2rh_irq_disable,
	.mask_ack = emma2rh_irq_disable,
	.unmask = emma2rh_irq_enable,
};

void emma2rh_irq_init(void)
{
	u32 i;

	for (i = 0; i < NUM_EMMA2RH_IRQ; i++)
		set_irq_chip_and_handler_name(EMMA2RH_IRQ_BASE + i,
					      &emma2rh_irq_controller,
					      handle_level_irq, "level");
}

static void emma2rh_sw_irq_enable(unsigned int irq)
{
	u32 reg;

	irq -= EMMA2RH_SW_IRQ_BASE;

	reg = emma2rh_in32(EMMA2RH_BHIF_SW_INT_EN);
	reg |= 1 << irq;
	emma2rh_out32(EMMA2RH_BHIF_SW_INT_EN, reg);
}

static void emma2rh_sw_irq_disable(unsigned int irq)
{
	u32 reg;

	irq -= EMMA2RH_SW_IRQ_BASE;

	reg = emma2rh_in32(EMMA2RH_BHIF_SW_INT_EN);
	reg &= ~(1 << irq);
	emma2rh_out32(EMMA2RH_BHIF_SW_INT_EN, reg);
}

struct irq_chip emma2rh_sw_irq_controller = {
	.name = "emma2rh_sw_irq",
	.ack = emma2rh_sw_irq_disable,
	.mask = emma2rh_sw_irq_disable,
	.mask_ack = emma2rh_sw_irq_disable,
	.unmask = emma2rh_sw_irq_enable,
};

void emma2rh_sw_irq_init(void)
{
	u32 i;

	for (i = 0; i < NUM_EMMA2RH_IRQ_SW; i++)
		set_irq_chip_and_handler_name(EMMA2RH_SW_IRQ_BASE + i,
					      &emma2rh_sw_irq_controller,
					      handle_level_irq, "level");
}

static void emma2rh_gpio_irq_enable(unsigned int irq)
{
	u32 reg;

	irq -= EMMA2RH_GPIO_IRQ_BASE;

	reg = emma2rh_in32(EMMA2RH_GPIO_INT_MASK);
	reg |= 1 << irq;
	emma2rh_out32(EMMA2RH_GPIO_INT_MASK, reg);
}

static void emma2rh_gpio_irq_disable(unsigned int irq)
{
	u32 reg;

	irq -= EMMA2RH_GPIO_IRQ_BASE;

	reg = emma2rh_in32(EMMA2RH_GPIO_INT_MASK);
	reg &= ~(1 << irq);
	emma2rh_out32(EMMA2RH_GPIO_INT_MASK, reg);
}

static void emma2rh_gpio_irq_ack(unsigned int irq)
{
	irq -= EMMA2RH_GPIO_IRQ_BASE;
	emma2rh_out32(EMMA2RH_GPIO_INT_ST, ~(1 << irq));
}

static void emma2rh_gpio_irq_mask_ack(unsigned int irq)
{
	u32 reg;

	irq -= EMMA2RH_GPIO_IRQ_BASE;
	emma2rh_out32(EMMA2RH_GPIO_INT_ST, ~(1 << irq));

	reg = emma2rh_in32(EMMA2RH_GPIO_INT_MASK);
	reg &= ~(1 << irq);
	emma2rh_out32(EMMA2RH_GPIO_INT_MASK, reg);
}

struct irq_chip emma2rh_gpio_irq_controller = {
	.name = "emma2rh_gpio_irq",
	.ack = emma2rh_gpio_irq_ack,
	.mask = emma2rh_gpio_irq_disable,
	.mask_ack = emma2rh_gpio_irq_mask_ack,
	.unmask = emma2rh_gpio_irq_enable,
};

void emma2rh_gpio_irq_init(void)
{
	u32 i;

	for (i = 0; i < NUM_EMMA2RH_IRQ_GPIO; i++)
		set_irq_chip_and_handler_name(EMMA2RH_GPIO_IRQ_BASE + i,
					      &emma2rh_gpio_irq_controller,
					      handle_edge_irq, "edge");
}

static struct irqaction irq_cascade = {
	   .handler = no_action,
	   .flags = 0,
	   .name = "cascade",
	   .dev_id = NULL,
	   .next = NULL,
};

/*
 * the first level int-handler will jump here if it is a emma2rh irq
 */
void emma2rh_irq_dispatch(void)
{
	u32 intStatus;
	u32 bitmask;
	u32 i;

	intStatus = emma2rh_in32(EMMA2RH_BHIF_INT_ST_0) &
		    emma2rh_in32(EMMA2RH_BHIF_INT_EN_0);

#ifdef EMMA2RH_SW_CASCADE
	if (intStatus & (1UL << EMMA2RH_SW_CASCADE)) {
		u32 swIntStatus;
		swIntStatus = emma2rh_in32(EMMA2RH_BHIF_SW_INT)
		    & emma2rh_in32(EMMA2RH_BHIF_SW_INT_EN);
		for (i = 0, bitmask = 1; i < 32; i++, bitmask <<= 1) {
			if (swIntStatus & bitmask) {
				do_IRQ(EMMA2RH_SW_IRQ_BASE + i);
				return;
			}
		}
	}
	/* Skip S/W interrupt */
	intStatus &= ~(1UL << EMMA2RH_SW_CASCADE);
#endif

	for (i = 0, bitmask = 1; i < 32; i++, bitmask <<= 1) {
		if (intStatus & bitmask) {
			do_IRQ(EMMA2RH_IRQ_BASE + i);
			return;
		}
	}

	intStatus = emma2rh_in32(EMMA2RH_BHIF_INT_ST_1) &
		    emma2rh_in32(EMMA2RH_BHIF_INT_EN_1);

#ifdef EMMA2RH_GPIO_CASCADE
	if (intStatus & (1UL << (EMMA2RH_GPIO_CASCADE % 32))) {
		u32 gpioIntStatus;
		gpioIntStatus = emma2rh_in32(EMMA2RH_GPIO_INT_ST)
		    & emma2rh_in32(EMMA2RH_GPIO_INT_MASK);
		for (i = 0, bitmask = 1; i < 32; i++, bitmask <<= 1) {
			if (gpioIntStatus & bitmask) {
				do_IRQ(EMMA2RH_GPIO_IRQ_BASE + i);
				return;
			}
		}
	}
	/* Skip GPIO interrupt */
	intStatus &= ~(1UL << (EMMA2RH_GPIO_CASCADE % 32));
#endif

	for (i = 32, bitmask = 1; i < 64; i++, bitmask <<= 1) {
		if (intStatus & bitmask) {
			do_IRQ(EMMA2RH_IRQ_BASE + i);
			return;
		}
	}

	intStatus = emma2rh_in32(EMMA2RH_BHIF_INT_ST_2) &
		    emma2rh_in32(EMMA2RH_BHIF_INT_EN_2);

	for (i = 64, bitmask = 1; i < 96; i++, bitmask <<= 1) {
		if (intStatus & bitmask) {
			do_IRQ(EMMA2RH_IRQ_BASE + i);
			return;
		}
	}
}

void __init arch_init_irq(void)
{
	u32 reg;

	/* by default, interrupts are disabled. */
	emma2rh_out32(EMMA2RH_BHIF_INT_EN_0, 0);
	emma2rh_out32(EMMA2RH_BHIF_INT_EN_1, 0);
	emma2rh_out32(EMMA2RH_BHIF_INT_EN_2, 0);
	emma2rh_out32(EMMA2RH_BHIF_INT1_EN_0, 0);
	emma2rh_out32(EMMA2RH_BHIF_INT1_EN_1, 0);
	emma2rh_out32(EMMA2RH_BHIF_INT1_EN_2, 0);
	emma2rh_out32(EMMA2RH_BHIF_SW_INT_EN, 0);

	clear_c0_status(0xff00);
	set_c0_status(0x0400);

#define GPIO_PCI (0xf<<15)
	/* setup GPIO interrupt for PCI interface */
	/* direction input */
	reg = emma2rh_in32(EMMA2RH_GPIO_DIR);
	emma2rh_out32(EMMA2RH_GPIO_DIR, reg & ~GPIO_PCI);
	/* disable interrupt */
	reg = emma2rh_in32(EMMA2RH_GPIO_INT_MASK);
	emma2rh_out32(EMMA2RH_GPIO_INT_MASK, reg & ~GPIO_PCI);
	/* level triggerd */
	reg = emma2rh_in32(EMMA2RH_GPIO_INT_MODE);
	emma2rh_out32(EMMA2RH_GPIO_INT_MODE, reg | GPIO_PCI);
	reg = emma2rh_in32(EMMA2RH_GPIO_INT_CND_A);
	emma2rh_out32(EMMA2RH_GPIO_INT_CND_A, reg & (~GPIO_PCI));
	/* interrupt clear */
	emma2rh_out32(EMMA2RH_GPIO_INT_ST, ~GPIO_PCI);

	/* init all controllers */
	emma2rh_irq_init();
	emma2rh_sw_irq_init();
	emma2rh_gpio_irq_init();
	mips_cpu_irq_init();

	/* setup cascade interrupts */
	setup_irq(EMMA2RH_IRQ_BASE + EMMA2RH_SW_CASCADE, &irq_cascade);
	setup_irq(EMMA2RH_IRQ_BASE + EMMA2RH_GPIO_CASCADE, &irq_cascade);
	setup_irq(MIPS_CPU_IRQ_BASE + 2, &irq_cascade);
}

asmlinkage void plat_irq_dispatch(void)
{
        unsigned int pending = read_c0_status() & read_c0_cause() & ST0_IM;

	if (pending & STATUSF_IP7)
		do_IRQ(MIPS_CPU_IRQ_BASE + 7);
	else if (pending & STATUSF_IP2)
		emma2rh_irq_dispatch();
	else if (pending & STATUSF_IP1)
		do_IRQ(MIPS_CPU_IRQ_BASE + 1);
	else if (pending & STATUSF_IP0)
		do_IRQ(MIPS_CPU_IRQ_BASE + 0);
	else
		spurious_interrupt();
}
