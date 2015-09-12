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



static si_t *gpio_sih;
int mask;


static int
gen_gpio_init(void)
{
	if (!(gpio_sih = si_kattach(SI_OSH))) {
		DBG("%s: si_kattach failed\n", __FUNCTION__);
		return -ENODEV;
	}

	si_gpiosetcore(gpio_sih);

	return 0;
}

static void
gen_gpio_exit(void)
{
	si_detach(gpio_sih);
}

/* GENERIC_GPIO calls */
int gpio_direction_input(unsigned gpio)
{
	int ret;

	ret = si_gpioouten(gpio_sih, (1<<gpio), 0, GPIO_APP_PRIORITY);
	DBG("%s: gpio %d - input 0x%x\n", __FUNCTION__, gpio, ret);
	return 0;
}
EXPORT_SYMBOL(gpio_direction_input);


int gpio_direction_output(unsigned gpio, int value)
{
	int out, outen;

	outen = si_gpioouten(gpio_sih, (1<<gpio), (1<<gpio), GPIO_APP_PRIORITY);
	out = si_gpioout(gpio_sih, (1<<gpio), (value ? (1<<gpio) : 0), GPIO_APP_PRIORITY);
	DBG("%s: gpio %d, value %d - out 0x%x outen 0x%x\n", __FUNCTION__, gpio, value, out, outen);
	return 0;
}
EXPORT_SYMBOL(gpio_direction_output);

int gpio_get_value(unsigned int gpio)
{
	uint32 get;
	get = si_gpioin(gpio_sih);

	get &= (1<<gpio);

	return (get ? 1 : 0);
}
EXPORT_SYMBOL(gpio_get_value);

void gpio_set_value(unsigned int gpio, int value)
{
	si_gpioout(gpio_sih, (1<<gpio), (value ? (1<<gpio) : 0), GPIO_APP_PRIORITY);
	return;
}
EXPORT_SYMBOL(gpio_set_value);

int gpio_request(unsigned int gpio, const char *label)
{
	int ret;

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

	return 0;
}
EXPORT_SYMBOL(gpio_request);

void gpio_free(unsigned int gpio)
{
	mask &= ~(1<<gpio);

	/* clear pullup */
	si_gpiopull(gpio_sih, GPIO_PULLUP, (1<<gpio), 0);
	si_gpiorelease(gpio_sih, (1<<gpio), GPIO_APP_PRIORITY);

	DBG("%s: gpio %d mask 0x%x\n", __FUNCTION__, gpio, mask);
	return;
}
EXPORT_SYMBOL(gpio_free);


module_init(gen_gpio_init);
module_exit(gen_gpio_exit);
