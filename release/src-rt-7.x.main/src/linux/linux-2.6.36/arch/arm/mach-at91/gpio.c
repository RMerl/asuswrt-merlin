/*
 * linux/arch/arm/mach-at91/gpio.c
 *
 * Copyright (C) 2005 HP Labs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/at91_pio.h>
#include <mach/gpio.h>

#include <asm/gpio.h>

#include "generic.h"

struct at91_gpio_chip {
	struct gpio_chip	chip;
	struct at91_gpio_chip	*next;		/* Bank sharing same clock */
	struct at91_gpio_bank	*bank;		/* Bank definition */
	void __iomem		*regbase;	/* Base of register bank */
};

#define to_at91_gpio_chip(c) container_of(c, struct at91_gpio_chip, chip)

static void at91_gpiolib_dbg_show(struct seq_file *s, struct gpio_chip *chip);
static void at91_gpiolib_set(struct gpio_chip *chip, unsigned offset, int val);
static int at91_gpiolib_get(struct gpio_chip *chip, unsigned offset);
static int at91_gpiolib_direction_output(struct gpio_chip *chip,
					 unsigned offset, int val);
static int at91_gpiolib_direction_input(struct gpio_chip *chip,
					unsigned offset);

#define AT91_GPIO_CHIP(name, base_gpio, nr_gpio)			\
	{								\
		.chip = {						\
			.label		  = name,			\
			.direction_input  = at91_gpiolib_direction_input, \
			.direction_output = at91_gpiolib_direction_output, \
			.get		  = at91_gpiolib_get,		\
			.set		  = at91_gpiolib_set,		\
			.dbg_show	  = at91_gpiolib_dbg_show,	\
			.base		  = base_gpio,			\
			.ngpio		  = nr_gpio,			\
		},							\
	}

static struct at91_gpio_chip gpio_chip[] = {
	AT91_GPIO_CHIP("A", 0x00 + PIN_BASE, 32),
	AT91_GPIO_CHIP("B", 0x20 + PIN_BASE, 32),
	AT91_GPIO_CHIP("C", 0x40 + PIN_BASE, 32),
	AT91_GPIO_CHIP("D", 0x60 + PIN_BASE, 32),
	AT91_GPIO_CHIP("E", 0x80 + PIN_BASE, 32),
};

static int gpio_banks;

static inline void __iomem *pin_to_controller(unsigned pin)
{
	pin -= PIN_BASE;
	pin /= 32;
	if (likely(pin < gpio_banks))
		return gpio_chip[pin].regbase;

	return NULL;
}

static inline unsigned pin_to_mask(unsigned pin)
{
	pin -= PIN_BASE;
	return 1 << (pin % 32);
}


/*--------------------------------------------------------------------------*/

/* Not all hardware capabilities are exposed through these calls; they
 * only encapsulate the most common features and modes.  (So if you
 * want to change signals in groups, do it directly.)
 *
 * Bootloaders will usually handle some of the pin multiplexing setup.
 * The intent is certainly that by the time Linux is fully booted, all
 * pins should have been fully initialized.  These setup calls should
 * only be used by board setup routines, or possibly in driver probe().
 *
 * For bootloaders doing all that setup, these calls could be inlined
 * as NOPs so Linux won't duplicate any setup code
 */


/*
 * mux the pin to the "GPIO" peripheral role.
 */
int __init_or_module at91_set_GPIO_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;
	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_GPIO_periph);


/*
 * mux the pin to the "A" internal peripheral role.
 */
int __init_or_module at91_set_A_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_ASR);
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_A_periph);


/*
 * mux the pin to the "B" internal peripheral role.
 */
int __init_or_module at91_set_B_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_BSR);
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_B_periph);


/*
 * mux the pin to the gpio controller (instead of "A" or "B" peripheral), and
 * configure it for an input.
 */
int __init_or_module at91_set_gpio_input(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_ODR);
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_input);


/*
 * mux the pin to the gpio controller (instead of "A" or "B" peripheral),
 * and configure it for an output.
 */
int __init_or_module at91_set_gpio_output(unsigned pin, int value)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + PIO_PUDR);
	__raw_writel(mask, pio + (value ? PIO_SODR : PIO_CODR));
	__raw_writel(mask, pio + PIO_OER);
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_output);


/*
 * enable/disable the glitch filter; mostly used with IRQ handling.
 */
int __init_or_module at91_set_deglitch(unsigned pin, int is_on)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;
	__raw_writel(mask, pio + (is_on ? PIO_IFER : PIO_IFDR));
	return 0;
}
EXPORT_SYMBOL(at91_set_deglitch);

/*
 * enable/disable the multi-driver; This is only valid for output and
 * allows the output pin to run as an open collector output.
 */
int __init_or_module at91_set_multi_drive(unsigned pin, int is_on)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + (is_on ? PIO_MDER : PIO_MDDR));
	return 0;
}
EXPORT_SYMBOL(at91_set_multi_drive);

/*
 * assuming the pin is muxed as a gpio output, set its value.
 */
int at91_set_gpio_value(unsigned pin, int value)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;
	__raw_writel(mask, pio + (value ? PIO_SODR : PIO_CODR));
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_value);


/*
 * read the pin's value (works even if it's not muxed as a gpio).
 */
int at91_get_gpio_value(unsigned pin)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);
	u32		pdsr;

	if (!pio)
		return -EINVAL;
	pdsr = __raw_readl(pio + PIO_PDSR);
	return (pdsr & mask) != 0;
}
EXPORT_SYMBOL(at91_get_gpio_value);

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_PM

static u32 wakeups[MAX_GPIO_BANKS];
static u32 backups[MAX_GPIO_BANKS];

static int gpio_irq_set_wake(unsigned pin, unsigned state)
{
	unsigned	mask = pin_to_mask(pin);
	unsigned	bank = (pin - PIN_BASE) / 32;

	if (unlikely(bank >= MAX_GPIO_BANKS))
		return -EINVAL;

	if (state)
		wakeups[bank] |= mask;
	else
		wakeups[bank] &= ~mask;

	set_irq_wake(gpio_chip[bank].bank->id, state);

	return 0;
}

void at91_gpio_suspend(void)
{
	int i;

	for (i = 0; i < gpio_banks; i++) {
		void __iomem	*pio = gpio_chip[i].regbase;

		backups[i] = __raw_readl(pio + PIO_IMR);
		__raw_writel(backups[i], pio + PIO_IDR);
		__raw_writel(wakeups[i], pio + PIO_IER);

		if (!wakeups[i])
			clk_disable(gpio_chip[i].bank->clock);
		else {
#ifdef CONFIG_PM_DEBUG
			printk(KERN_DEBUG "GPIO-%c may wake for %08x\n", 'A'+i, wakeups[i]);
#endif
		}
	}
}

void at91_gpio_resume(void)
{
	int i;

	for (i = 0; i < gpio_banks; i++) {
		void __iomem	*pio = gpio_chip[i].regbase;

		if (!wakeups[i])
			clk_enable(gpio_chip[i].bank->clock);

		__raw_writel(wakeups[i], pio + PIO_IDR);
		__raw_writel(backups[i], pio + PIO_IER);
	}
}

#else
#define gpio_irq_set_wake	NULL
#endif


/* Several AIC controller irqs are dispatched through this GPIO handler.
 * To use any AT91_PIN_* as an externally triggered IRQ, first call
 * at91_set_gpio_input() then maybe enable its glitch filter.
 * Then just request_irq() with the pin ID; it works like any ARM IRQ
 * handler, though it always triggers on rising and falling edges.
 *
 * Alternatively, certain pins may be used directly as IRQ0..IRQ6 after
 * configuring them with at91_set_a_periph() or at91_set_b_periph().
 * IRQ0..IRQ6 should be configurable, e.g. level vs edge triggering.
 */

static void gpio_irq_mask(unsigned pin)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (pio)
		__raw_writel(mask, pio + PIO_IDR);
}

static void gpio_irq_unmask(unsigned pin)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (pio)
		__raw_writel(mask, pio + PIO_IER);
}

static int gpio_irq_type(unsigned pin, unsigned type)
{
	switch (type) {
	case IRQ_TYPE_NONE:
	case IRQ_TYPE_EDGE_BOTH:
		return 0;
	default:
		return -EINVAL;
	}
}

static struct irq_chip gpio_irqchip = {
	.name		= "GPIO",
	.mask		= gpio_irq_mask,
	.unmask		= gpio_irq_unmask,
	.set_type	= gpio_irq_type,
	.set_wake	= gpio_irq_set_wake,
};

static void gpio_irq_handler(unsigned irq, struct irq_desc *desc)
{
	unsigned	pin;
	struct irq_desc	*gpio;
	struct at91_gpio_chip *at91_gpio;
	void __iomem	*pio;
	u32		isr;

	at91_gpio = get_irq_chip_data(irq);
	pio = at91_gpio->regbase;

	/* temporarily mask (level sensitive) parent IRQ */
	desc->chip->ack(irq);
	for (;;) {
		/* Reading ISR acks pending (edge triggered) GPIO interrupts.
		 * When there none are pending, we're finished unless we need
		 * to process multiple banks (like ID_PIOCDE on sam9263).
		 */
		isr = __raw_readl(pio + PIO_ISR) & __raw_readl(pio + PIO_IMR);
		if (!isr) {
			if (!at91_gpio->next)
				break;
			at91_gpio = at91_gpio->next;
			pio = at91_gpio->regbase;
			continue;
		}

		pin = at91_gpio->chip.base;
		gpio = &irq_desc[pin];

		while (isr) {
			if (isr & 1) {
				if (unlikely(gpio->depth)) {
					/*
					 * The core ARM interrupt handler lazily disables IRQs so
					 * another IRQ must be generated before it actually gets
					 * here to be disabled on the GPIO controller.
					 */
					gpio_irq_mask(pin);
				}
				else
					generic_handle_irq(pin);
			}
			pin++;
			gpio++;
			isr >>= 1;
		}
	}
	desc->chip->unmask(irq);
	/* now it may re-trigger */
}

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_DEBUG_FS

static int at91_gpio_show(struct seq_file *s, void *unused)
{
	int bank, j;

	/* print heading */
	seq_printf(s, "Pin\t");
	for (bank = 0; bank < gpio_banks; bank++) {
		seq_printf(s, "PIO%c\t", 'A' + bank);
	};
	seq_printf(s, "\n\n");

	/* print pin status */
	for (j = 0; j < 32; j++) {
		seq_printf(s, "%i:\t", j);

		for (bank = 0; bank < gpio_banks; bank++) {
			unsigned	pin  = PIN_BASE + (32 * bank) + j;
			void __iomem	*pio = pin_to_controller(pin);
			unsigned	mask = pin_to_mask(pin);

			if (__raw_readl(pio + PIO_PSR) & mask)
				seq_printf(s, "GPIO:%s", __raw_readl(pio + PIO_PDSR) & mask ? "1" : "0");
			else
				seq_printf(s, "%s", __raw_readl(pio + PIO_ABSR) & mask ? "B" : "A");

			seq_printf(s, "\t");
		}

		seq_printf(s, "\n");
	}

	return 0;
}

static int at91_gpio_open(struct inode *inode, struct file *file)
{
	return single_open(file, at91_gpio_show, NULL);
}

static const struct file_operations at91_gpio_operations = {
	.open		= at91_gpio_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init at91_gpio_debugfs_init(void)
{
	/* /sys/kernel/debug/at91_gpio */
	(void) debugfs_create_file("at91_gpio", S_IFREG | S_IRUGO, NULL, NULL, &at91_gpio_operations);
	return 0;
}
postcore_initcall(at91_gpio_debugfs_init);

#endif

/*--------------------------------------------------------------------------*/

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;

/*
 * Called from the processor-specific init to enable GPIO interrupt support.
 */
void __init at91_gpio_irq_setup(void)
{
	unsigned		pioc, pin;
	struct at91_gpio_chip	*this, *prev;

	for (pioc = 0, pin = PIN_BASE, this = gpio_chip, prev = NULL;
			pioc++ < gpio_banks;
			prev = this, this++) {
		unsigned	id = this->bank->id;
		unsigned	i;

		__raw_writel(~0, this->regbase + PIO_IDR);

		for (i = 0, pin = this->chip.base; i < 32; i++, pin++) {
			lockdep_set_class(&irq_desc[pin].lock, &gpio_lock_class);

			/*
			 * Can use the "simple" and not "edge" handler since it's
			 * shorter, and the AIC handles interrupts sanely.
			 */
			set_irq_chip(pin, &gpio_irqchip);
			set_irq_handler(pin, handle_simple_irq);
			set_irq_flags(pin, IRQF_VALID);
		}

		/* The toplevel handler handles one bank of GPIOs, except
		 * AT91SAM9263_ID_PIOCDE handles three... PIOC is first in
		 * the list, so we only set up that handler.
		 */
		if (prev && prev->next == this)
			continue;

		set_irq_chip_data(id, this);
		set_irq_chained_handler(id, gpio_irq_handler);
	}
	pr_info("AT91: %d gpio irqs in %d banks\n", pin - PIN_BASE, gpio_banks);
}

/* gpiolib support */
static int at91_gpiolib_direction_input(struct gpio_chip *chip,
					unsigned offset)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + PIO_ODR);
	return 0;
}

static int at91_gpiolib_direction_output(struct gpio_chip *chip,
					 unsigned offset, int val)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + (val ? PIO_SODR : PIO_CODR));
	__raw_writel(mask, pio + PIO_OER);
	return 0;
}

static int at91_gpiolib_get(struct gpio_chip *chip, unsigned offset)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;
	u32 pdsr;

	pdsr = __raw_readl(pio + PIO_PDSR);
	return (pdsr & mask) != 0;
}

static void at91_gpiolib_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + (val ? PIO_SODR : PIO_CODR));
}

static void at91_gpiolib_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	int i;

	for (i = 0; i < chip->ngpio; i++) {
		unsigned pin = chip->base + i;
		void __iomem *pio = pin_to_controller(pin);
		unsigned mask = pin_to_mask(pin);
		const char *gpio_label;

		gpio_label = gpiochip_is_requested(chip, i);
		if (gpio_label) {
			seq_printf(s, "[%s] GPIO%s%d: ",
				   gpio_label, chip->label, i);
			if (__raw_readl(pio + PIO_PSR) & mask)
				seq_printf(s, "[gpio] %s\n",
					   at91_get_gpio_value(pin) ?
					   "set" : "clear");
			else
				seq_printf(s, "[periph %s]\n",
					   __raw_readl(pio + PIO_ABSR) &
					   mask ? "B" : "A");
		}
	}
}

/*
 * Called from the processor-specific init to enable GPIO pin support.
 */
void __init at91_gpio_init(struct at91_gpio_bank *data, int nr_banks)
{
	unsigned		i;
	struct at91_gpio_chip *at91_gpio, *last = NULL;

	BUG_ON(nr_banks > MAX_GPIO_BANKS);

	gpio_banks = nr_banks;

	for (i = 0; i < nr_banks; i++) {
		at91_gpio = &gpio_chip[i];

		at91_gpio->bank = &data[i];
		at91_gpio->chip.base = PIN_BASE + i * 32;
		at91_gpio->regbase = at91_gpio->bank->offset +
			(void __iomem *)AT91_VA_BASE_SYS;

		/* enable PIO controller's clock */
		clk_enable(at91_gpio->bank->clock);

		/* AT91SAM9263_ID_PIOCDE groups PIOC, PIOD, PIOE */
		if (last && last->bank->id == at91_gpio->bank->id)
			last->next = at91_gpio;
		last = at91_gpio;

		gpiochip_add(&at91_gpio->chip);
	}
}
