/*
 * arch/sh/boards/mach-x3proto/gpio.c
 *
 * Renesas SH-X3 Prototype Baseboard GPIO Support.
 *
 * Copyright (C) 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <mach/ilsel.h>
#include <mach/hardware.h>

#define KEYCTLR	0xb81c0000
#define KEYOUTR	0xb81c0002
#define KEYDETR 0xb81c0004

static DEFINE_SPINLOCK(x3proto_gpio_lock);
static unsigned int x3proto_gpio_irq_map[NR_BASEBOARD_GPIOS] = { 0, };

static int x3proto_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	unsigned long flags;
	unsigned int data;

	spin_lock_irqsave(&x3proto_gpio_lock, flags);
	data = __raw_readw(KEYCTLR);
	data |= (1 << gpio);
	__raw_writew(data, KEYCTLR);
	spin_unlock_irqrestore(&x3proto_gpio_lock, flags);

	return 0;
}

static int x3proto_gpio_get(struct gpio_chip *chip, unsigned gpio)
{
	return !!(__raw_readw(KEYDETR) & (1 << gpio));
}

static int x3proto_gpio_to_irq(struct gpio_chip *chip, unsigned gpio)
{
	return x3proto_gpio_irq_map[gpio];
}

static void x3proto_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_data *data = irq_get_irq_data(irq);
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	unsigned long mask;
	int pin;

	chip->irq_mask_ack(data);

	mask = __raw_readw(KEYDETR);

	for_each_set_bit(pin, &mask, NR_BASEBOARD_GPIOS)
		generic_handle_irq(x3proto_gpio_to_irq(NULL, pin));

	chip->irq_unmask(data);
}

struct gpio_chip x3proto_gpio_chip = {
	.label			= "x3proto-gpio",
	.direction_input	= x3proto_gpio_direction_input,
	.get			= x3proto_gpio_get,
	.to_irq			= x3proto_gpio_to_irq,
	.base			= -1,
	.ngpio			= NR_BASEBOARD_GPIOS,
};

int __init x3proto_gpio_setup(void)
{
	int ilsel;
	int ret, i;

	ilsel = ilsel_enable(ILSEL_KEY);
	if (unlikely(ilsel < 0))
		return ilsel;

	ret = gpiochip_add(&x3proto_gpio_chip);
	if (unlikely(ret))
		goto err_gpio;

	for (i = 0; i < NR_BASEBOARD_GPIOS; i++) {
		unsigned long flags;
		int irq = create_irq();

		if (unlikely(irq < 0)) {
			ret = -EINVAL;
			goto err_irq;
		}

		spin_lock_irqsave(&x3proto_gpio_lock, flags);
		x3proto_gpio_irq_map[i] = irq;
		irq_set_chip_and_handler_name(irq, &dummy_irq_chip,
					      handle_simple_irq, "gpio");
		spin_unlock_irqrestore(&x3proto_gpio_lock, flags);
	}

	pr_info("registering '%s' support, handling GPIOs %u -> %u, "
		"bound to IRQ %u\n",
		x3proto_gpio_chip.label, x3proto_gpio_chip.base,
		x3proto_gpio_chip.base + x3proto_gpio_chip.ngpio,
		ilsel);

	irq_set_chained_handler(ilsel, x3proto_gpio_irq_handler);
	irq_set_irq_wake(ilsel, 1);

	return 0;

err_irq:
	for (; i >= 0; --i)
		if (x3proto_gpio_irq_map[i])
			destroy_irq(x3proto_gpio_irq_map[i]);

	ret = gpiochip_remove(&x3proto_gpio_chip);
	if (unlikely(ret))
		pr_err("Failed deregistering GPIO\n");

err_gpio:
	synchronize_irq(ilsel);

	ilsel_disable(ILSEL_KEY);

	return ret;
}
