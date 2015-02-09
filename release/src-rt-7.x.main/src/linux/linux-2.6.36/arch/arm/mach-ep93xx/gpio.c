/*
 * linux/arch/arm/mach-ep93xx/gpio.c
 *
 * Generic EP93xx GPIO handling
 *
 * Copyright (c) 2008 Ryan Mallon <ryan@bluewatersys.com>
 *
 * Based on code originally from:
 *  linux/arch/arm/mach-ep93xx/core.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "ep93xx " KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include <mach/hardware.h>

/*************************************************************************
 * Interrupt handling for EP93xx on-chip GPIOs
 *************************************************************************/
static unsigned char gpio_int_unmasked[3];
static unsigned char gpio_int_enabled[3];
static unsigned char gpio_int_type1[3];
static unsigned char gpio_int_type2[3];
static unsigned char gpio_int_debounce[3];

/* Port ordering is: A B F */
static const u8 int_type1_register_offset[3]	= { 0x90, 0xac, 0x4c };
static const u8 int_type2_register_offset[3]	= { 0x94, 0xb0, 0x50 };
static const u8 eoi_register_offset[3]		= { 0x98, 0xb4, 0x54 };
static const u8 int_en_register_offset[3]	= { 0x9c, 0xb8, 0x58 };
static const u8 int_debounce_register_offset[3]	= { 0xa8, 0xc4, 0x64 };

static void ep93xx_gpio_update_int_params(unsigned port)
{
	BUG_ON(port > 2);

	__raw_writeb(0, EP93XX_GPIO_REG(int_en_register_offset[port]));

	__raw_writeb(gpio_int_type2[port],
		EP93XX_GPIO_REG(int_type2_register_offset[port]));

	__raw_writeb(gpio_int_type1[port],
		EP93XX_GPIO_REG(int_type1_register_offset[port]));

	__raw_writeb(gpio_int_unmasked[port] & gpio_int_enabled[port],
		EP93XX_GPIO_REG(int_en_register_offset[port]));
}

static inline void ep93xx_gpio_int_mask(unsigned line)
{
	gpio_int_unmasked[line >> 3] &= ~(1 << (line & 7));
}

void ep93xx_gpio_int_debounce(unsigned int irq, int enable)
{
	int line = irq_to_gpio(irq);
	int port = line >> 3;
	int port_mask = 1 << (line & 7);

	if (enable)
		gpio_int_debounce[port] |= port_mask;
	else
		gpio_int_debounce[port] &= ~port_mask;

	__raw_writeb(gpio_int_debounce[port],
		EP93XX_GPIO_REG(int_debounce_register_offset[port]));
}
EXPORT_SYMBOL(ep93xx_gpio_int_debounce);

static void ep93xx_gpio_ab_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned char status;
	int i;

	status = __raw_readb(EP93XX_GPIO_A_INT_STATUS);
	for (i = 0; i < 8; i++) {
		if (status & (1 << i)) {
			int gpio_irq = gpio_to_irq(EP93XX_GPIO_LINE_A(0)) + i;
			generic_handle_irq(gpio_irq);
		}
	}

	status = __raw_readb(EP93XX_GPIO_B_INT_STATUS);
	for (i = 0; i < 8; i++) {
		if (status & (1 << i)) {
			int gpio_irq = gpio_to_irq(EP93XX_GPIO_LINE_B(0)) + i;
			generic_handle_irq(gpio_irq);
		}
	}
}

static void ep93xx_gpio_f_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	/*
	 * map discontiguous hw irq range to continous sw irq range:
	 *
	 *  IRQ_EP93XX_GPIO{0..7}MUX -> gpio_to_irq(EP93XX_GPIO_LINE_F({0..7})
	 */
	int port_f_idx = ((irq + 1) & 7) ^ 4; /* {19..22,47..50} -> {0..7} */
	int gpio_irq = gpio_to_irq(EP93XX_GPIO_LINE_F(0)) + port_f_idx;

	generic_handle_irq(gpio_irq);
}

static void ep93xx_gpio_irq_ack(unsigned int irq)
{
	int line = irq_to_gpio(irq);
	int port = line >> 3;
	int port_mask = 1 << (line & 7);

	if ((irq_desc[irq].status & IRQ_TYPE_SENSE_MASK) == IRQ_TYPE_EDGE_BOTH) {
		gpio_int_type2[port] ^= port_mask; /* switch edge direction */
		ep93xx_gpio_update_int_params(port);
	}

	__raw_writeb(port_mask, EP93XX_GPIO_REG(eoi_register_offset[port]));
}

static void ep93xx_gpio_irq_mask_ack(unsigned int irq)
{
	int line = irq_to_gpio(irq);
	int port = line >> 3;
	int port_mask = 1 << (line & 7);

	if ((irq_desc[irq].status & IRQ_TYPE_SENSE_MASK) == IRQ_TYPE_EDGE_BOTH)
		gpio_int_type2[port] ^= port_mask; /* switch edge direction */

	gpio_int_unmasked[port] &= ~port_mask;
	ep93xx_gpio_update_int_params(port);

	__raw_writeb(port_mask, EP93XX_GPIO_REG(eoi_register_offset[port]));
}

static void ep93xx_gpio_irq_mask(unsigned int irq)
{
	int line = irq_to_gpio(irq);
	int port = line >> 3;

	gpio_int_unmasked[port] &= ~(1 << (line & 7));
	ep93xx_gpio_update_int_params(port);
}

static void ep93xx_gpio_irq_unmask(unsigned int irq)
{
	int line = irq_to_gpio(irq);
	int port = line >> 3;

	gpio_int_unmasked[port] |= 1 << (line & 7);
	ep93xx_gpio_update_int_params(port);
}

/*
 * gpio_int_type1 controls whether the interrupt is level (0) or
 * edge (1) triggered, while gpio_int_type2 controls whether it
 * triggers on low/falling (0) or high/rising (1).
 */
static int ep93xx_gpio_irq_type(unsigned int irq, unsigned int type)
{
	struct irq_desc *desc = irq_desc + irq;
	const int gpio = irq_to_gpio(irq);
	const int port = gpio >> 3;
	const int port_mask = 1 << (gpio & 7);

	gpio_direction_input(gpio);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		gpio_int_type1[port] |= port_mask;
		gpio_int_type2[port] |= port_mask;
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		gpio_int_type1[port] |= port_mask;
		gpio_int_type2[port] &= ~port_mask;
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		gpio_int_type1[port] &= ~port_mask;
		gpio_int_type2[port] |= port_mask;
		desc->handle_irq = handle_level_irq;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		gpio_int_type1[port] &= ~port_mask;
		gpio_int_type2[port] &= ~port_mask;
		desc->handle_irq = handle_level_irq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		gpio_int_type1[port] |= port_mask;
		/* set initial polarity based on current input level */
		if (gpio_get_value(gpio))
			gpio_int_type2[port] &= ~port_mask; /* falling */
		else
			gpio_int_type2[port] |= port_mask; /* rising */
		desc->handle_irq = handle_edge_irq;
		break;
	default:
		pr_err("failed to set irq type %d for gpio %d\n", type, gpio);
		return -EINVAL;
	}

	gpio_int_enabled[port] |= port_mask;

	desc->status &= ~IRQ_TYPE_SENSE_MASK;
	desc->status |= type & IRQ_TYPE_SENSE_MASK;

	ep93xx_gpio_update_int_params(port);

	return 0;
}

static struct irq_chip ep93xx_gpio_irq_chip = {
	.name		= "GPIO",
	.ack		= ep93xx_gpio_irq_ack,
	.mask_ack	= ep93xx_gpio_irq_mask_ack,
	.mask		= ep93xx_gpio_irq_mask,
	.unmask		= ep93xx_gpio_irq_unmask,
	.set_type	= ep93xx_gpio_irq_type,
};

void __init ep93xx_gpio_init_irq(void)
{
	int gpio_irq;

	for (gpio_irq = gpio_to_irq(0);
	     gpio_irq <= gpio_to_irq(EP93XX_GPIO_LINE_MAX_IRQ); ++gpio_irq) {
		set_irq_chip(gpio_irq, &ep93xx_gpio_irq_chip);
		set_irq_handler(gpio_irq, handle_level_irq);
		set_irq_flags(gpio_irq, IRQF_VALID);
	}

	set_irq_chained_handler(IRQ_EP93XX_GPIO_AB, ep93xx_gpio_ab_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO0MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO1MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO2MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO3MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO4MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO5MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO6MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO7MUX, ep93xx_gpio_f_irq_handler);
}


/*************************************************************************
 * gpiolib interface for EP93xx on-chip GPIOs
 *************************************************************************/
struct ep93xx_gpio_chip {
	struct gpio_chip	chip;

	void __iomem		*data_reg;
	void __iomem		*data_dir_reg;
};

#define to_ep93xx_gpio_chip(c) container_of(c, struct ep93xx_gpio_chip, chip)

static int ep93xx_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct ep93xx_gpio_chip *ep93xx_chip = to_ep93xx_gpio_chip(chip);
	unsigned long flags;
	u8 v;

	local_irq_save(flags);
	v = __raw_readb(ep93xx_chip->data_dir_reg);
	v &= ~(1 << offset);
	__raw_writeb(v, ep93xx_chip->data_dir_reg);
	local_irq_restore(flags);

	return 0;
}

static int ep93xx_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int val)
{
	struct ep93xx_gpio_chip *ep93xx_chip = to_ep93xx_gpio_chip(chip);
	unsigned long flags;
	int line;
	u8 v;

	local_irq_save(flags);

	/* Set the value */
	v = __raw_readb(ep93xx_chip->data_reg);
	if (val)
		v |= (1 << offset);
	else
		v &= ~(1 << offset);
	__raw_writeb(v, ep93xx_chip->data_reg);

	/* Drive as an output */
	line = chip->base + offset;
	if (line <= EP93XX_GPIO_LINE_MAX_IRQ) {
		/* Ports A/B/F */
		ep93xx_gpio_int_mask(line);
		ep93xx_gpio_update_int_params(line >> 3);
	}

	v = __raw_readb(ep93xx_chip->data_dir_reg);
	v |= (1 << offset);
	__raw_writeb(v, ep93xx_chip->data_dir_reg);

	local_irq_restore(flags);

	return 0;
}

static int ep93xx_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct ep93xx_gpio_chip *ep93xx_chip = to_ep93xx_gpio_chip(chip);

	return !!(__raw_readb(ep93xx_chip->data_reg) & (1 << offset));
}

static void ep93xx_gpio_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct ep93xx_gpio_chip *ep93xx_chip = to_ep93xx_gpio_chip(chip);
	unsigned long flags;
	u8 v;

	local_irq_save(flags);
	v = __raw_readb(ep93xx_chip->data_reg);
	if (val)
		v |= (1 << offset);
	else
		v &= ~(1 << offset);
	__raw_writeb(v, ep93xx_chip->data_reg);
	local_irq_restore(flags);
}

static void ep93xx_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct ep93xx_gpio_chip *ep93xx_chip = to_ep93xx_gpio_chip(chip);
	u8 data_reg, data_dir_reg;
	int gpio, i;

	data_reg = __raw_readb(ep93xx_chip->data_reg);
	data_dir_reg = __raw_readb(ep93xx_chip->data_dir_reg);

	gpio = ep93xx_chip->chip.base;
	for (i = 0; i < chip->ngpio; i++, gpio++) {
		int is_out = data_dir_reg & (1 << i);

		seq_printf(s, " %s%d gpio-%-3d (%-12s) %s %s",
				chip->label, i, gpio,
				gpiochip_is_requested(chip, i) ? : "",
				is_out ? "out" : "in ",
				(data_reg & (1 << i)) ? "hi" : "lo");

		if (!is_out) {
			int irq = gpio_to_irq(gpio);
			struct irq_desc *desc = irq_desc + irq;

			if (irq >= 0 && desc->action) {
				char *trigger;

				switch (desc->status & IRQ_TYPE_SENSE_MASK) {
				case IRQ_TYPE_NONE:
					trigger = "(default)";
					break;
				case IRQ_TYPE_EDGE_FALLING:
					trigger = "edge-falling";
					break;
				case IRQ_TYPE_EDGE_RISING:
					trigger = "edge-rising";
					break;
				case IRQ_TYPE_EDGE_BOTH:
					trigger = "edge-both";
					break;
				case IRQ_TYPE_LEVEL_HIGH:
					trigger = "level-high";
					break;
				case IRQ_TYPE_LEVEL_LOW:
					trigger = "level-low";
					break;
				default:
					trigger = "?trigger?";
					break;
				}

				seq_printf(s, " irq-%d %s%s",
						irq, trigger,
						(desc->status & IRQ_WAKEUP)
							? " wakeup" : "");
			}
		}

		seq_printf(s, "\n");
	}
}

#define EP93XX_GPIO_BANK(name, dr, ddr, base_gpio)			\
	{								\
		.chip = {						\
			.label		  = name,			\
			.direction_input  = ep93xx_gpio_direction_input, \
			.direction_output = ep93xx_gpio_direction_output, \
			.get		  = ep93xx_gpio_get,		\
			.set		  = ep93xx_gpio_set,		\
			.dbg_show	  = ep93xx_gpio_dbg_show,	\
			.base		  = base_gpio,			\
			.ngpio		  = 8,				\
		},							\
		.data_reg	= EP93XX_GPIO_REG(dr),			\
		.data_dir_reg	= EP93XX_GPIO_REG(ddr),			\
	}

static struct ep93xx_gpio_chip ep93xx_gpio_banks[] = {
	EP93XX_GPIO_BANK("A", 0x00, 0x10, 0),
	EP93XX_GPIO_BANK("B", 0x04, 0x14, 8),
	EP93XX_GPIO_BANK("C", 0x08, 0x18, 40),
	EP93XX_GPIO_BANK("D", 0x0c, 0x1c, 24),
	EP93XX_GPIO_BANK("E", 0x20, 0x24, 32),
	EP93XX_GPIO_BANK("F", 0x30, 0x34, 16),
	EP93XX_GPIO_BANK("G", 0x38, 0x3c, 48),
	EP93XX_GPIO_BANK("H", 0x40, 0x44, 56),
};

void __init ep93xx_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ep93xx_gpio_banks); i++)
		gpiochip_add(&ep93xx_gpio_banks[i].chip);
}
