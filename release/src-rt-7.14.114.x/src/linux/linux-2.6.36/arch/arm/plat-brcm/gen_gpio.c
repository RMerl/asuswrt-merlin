/*
 * Generic GPIO
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: gen_gpio.c,v 1.1 2009-10-30 20:51:47 $
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>

#define BCM947XX_GENGPIO_DEBUG 0
#if BCM947XX_GENGPIO_DEBUG
//#define DBG(x...) printk(KERN_DEBUG x)
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif

/* Global SB handle */
extern si_t *bcm947xx_sih;

/* Convenience */
#define gpio_sih bcm947xx_sih
//static si_t *gpio_sih;
int mask;

#ifndef GPIO_COMMON_SPINLOCK_NAME
#define GPIO_COMMON_SPINLOCK_NAME lock
static DEFINE_SPINLOCK(GPIO_COMMON_SPINLOCK_NAME);
#else
DEFINE_SPINLOCK(GPIO_COMMON_SPINLOCK_NAME);
EXPORT_SYMBOL(GPIO_COMMON_SPINLOCK_NAME);
#endif

#ifdef CONFIG_GPIOLIB
# define BCM5301X_GPIO_FUNCNAME(func)	bcm5301x_##func
# define BCM5301X_GPIO_FUNCDECL(func)	static BCM5301X_GPIO_FUNCNAME(func)
# define BCM5301X_GPIO_EXPORT(func)
#else
# define BCM5301X_GPIO_FUNCDECL(func)	func
# define BCM5301X_GPIO_EXPORT(func)	EXPORT_SYMBOL(func)
#endif

int BCM5301X_GPIO_FUNCDECL(gpio_direction_input)(unsigned gpio)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);
	ret = si_gpioouten(gpio_sih, (1<<gpio), 0, GPIO_APP_PRIORITY);
	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);

	DBG("%s: gpio %d - input 0x%x\n", __FUNCTION__, gpio, ret);
	return 0;
}
BCM5301X_GPIO_EXPORT(gpio_direction_input);

int BCM5301X_GPIO_FUNCDECL(gpio_direction_output)(unsigned gpio, int value)
{
	int out, outen;
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);
	outen = si_gpioouten(gpio_sih, (1<<gpio), (1<<gpio), GPIO_APP_PRIORITY);
	out = si_gpioout(gpio_sih, (1<<gpio), (value ? (1<<gpio) : 0), GPIO_APP_PRIORITY);
	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);

	DBG("%s: gpio %d, value %d - out 0x%x outen 0x%x\n", __FUNCTION__, gpio, value, out, outen);
	return 0;
}
BCM5301X_GPIO_EXPORT(gpio_direction_output);

int gpio_get_value(unsigned int gpio)
{
	uint32 get;
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);
	get = si_gpioin(gpio_sih);
	get &= (1<<gpio);
	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);

	return (get ? 1 : 0);
}
EXPORT_SYMBOL(gpio_get_value);

void gpio_set_value(unsigned int gpio, int value)
{
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);
	si_gpioout(gpio_sih, (1<<gpio), (value ? (1<<gpio) : 0), GPIO_APP_PRIORITY);
	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);
}
EXPORT_SYMBOL(gpio_set_value);

int BCM5301X_GPIO_FUNCDECL(gpio_request)(unsigned int gpio, const char *label)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);

	mask |= (1<<gpio);

	ret = si_gpioreserve(gpio_sih, (1<<gpio), GPIO_APP_PRIORITY);
	DBG("%s: gpio %d label %s mask 0x%x reserve 0x%x\n", __FUNCTION__, gpio,
	       label, mask, ret);

	ret = si_gpiocontrol(gpio_sih, (1<<gpio), 0, GPIO_APP_PRIORITY);
	DBG("%s: si_gpiocontrol 0x%x\n", __FUNCTION__, ret);

	/* clear pulldown */
	ret = si_gpiopull(gpio_sih, GPIO_PULLDN, (1<<gpio), 0);
	DBG("%s: si_gpiopull (down) 0x%x\n", __FUNCTION__, ret);
	/* Set pullup */
	ret = si_gpiopull(gpio_sih, GPIO_PULLUP, (1<<gpio), (1<<gpio));
	DBG("%s: si_gpiopull (up) 0x%x\n", __FUNCTION__, ret);

	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);

	return 0;
}
BCM5301X_GPIO_EXPORT(gpio_request);

void BCM5301X_GPIO_FUNCDECL(gpio_free)(unsigned int gpio)
{
	unsigned long flags;

	spin_lock_irqsave(&GPIO_COMMON_SPINLOCK_NAME, flags);

	mask &= ~(1<<gpio);

	/* clear pullup */
	si_gpiopull(gpio_sih, GPIO_PULLUP, (1<<gpio), 0);
	si_gpiorelease(gpio_sih, (1<<gpio), GPIO_APP_PRIORITY);

	spin_unlock_irqrestore(&GPIO_COMMON_SPINLOCK_NAME, flags);

	DBG("%s: gpio %d mask 0x%x\n", __FUNCTION__, gpio, mask);
}
BCM5301X_GPIO_EXPORT(gpio_free);

#ifdef CONFIG_GPIOLIB

static int
gpiolib_request(struct gpio_chip *chip, unsigned int offset)
{
	return BCM5301X_GPIO_FUNCNAME(gpio_request)(chip->base + offset, "gpiolib");
}

static void
gpiolib_free(struct gpio_chip *chip, unsigned int offset)
{
	BCM5301X_GPIO_FUNCNAME(gpio_free)(chip->base + offset);
}

static int
gpiolib_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	return BCM5301X_GPIO_FUNCNAME(gpio_direction_input)(chip->base + offset);
}

static int
gpiolib_get_value(struct gpio_chip *chip, unsigned int offset)
{
	return gpio_get_value(chip->base + offset);
}

static int
gpiolib_direction_output(struct gpio_chip *chip, unsigned int offset, int value)
{
	return BCM5301X_GPIO_FUNCNAME(gpio_direction_output)(chip->base + offset, value);
}

static void
gpiolib_set_value(struct gpio_chip *chip, unsigned int offset, int value)
{
	 gpio_set_value(chip->base + offset, value);
}

static struct gpio_chip gc = {
	.label			= "bcm5301x_gpio",
	.owner			= THIS_MODULE,
	.request		= gpiolib_request,
	.free			= gpiolib_free,
	.direction_input	= gpiolib_direction_input,
	.get			= gpiolib_get_value,
	.direction_output	= gpiolib_direction_output,
	.set			= gpiolib_set_value,
	.base			= 0,
	.ngpio			= ARCH_NR_GPIOS,
};
#endif /* CONFIG_GPIOLIB */

static int
gen_gpio_init(void)
{

#ifdef CONFIG_GPIOLIB
	gpiochip_add(&gc);
#endif

	return 0;
}

static void
gen_gpio_exit(void)
{
#ifdef CONFIG_GPIOLIB
	(void)gpiochip_remove(&gc);
#endif

}

module_init(gen_gpio_init);
module_exit(gen_gpio_exit);
