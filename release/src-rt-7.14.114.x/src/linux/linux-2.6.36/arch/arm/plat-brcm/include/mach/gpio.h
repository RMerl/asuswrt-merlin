#ifndef _ASM_ARCH_GPIO_H
#define _ASM_ARCH_GPIO_H
/*
 * Generic GPIO Interface
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
 * $Id: gpio.h,v 1.1 2009-11-02 19:10:00 $
 */

#include <plat/plat-bcm5301x.h>

#ifdef CONFIG_GENERIC_GPIO
#define GPIO_COMMON_SPINLOCK_NAME gpio_common_spin_lock_name
#endif

int gpio_direction_input(unsigned pin);
int gpio_direction_output(unsigned pin, int value);

int gpio_get_value(unsigned int gpio);
void gpio_set_value(unsigned int gpio, int value);

int gpio_request(unsigned int gpio, const char *label);
void gpio_free(unsigned int gpio);

static inline int gpio_to_irq(unsigned gpio)
{
#ifdef CONFIG_PLAT_CCA_GPIO_IRQ
	if (gpio < IRQ_CCA_GPIO_N) {
		return IRQ_CCA_GPIO(gpio);
	}
#endif
	return -EINVAL;
}

static inline int irq_to_gpio(unsigned irq)
{
#ifdef CONFIG_PLAT_CCA_GPIO_IRQ
	if ((irq >= IRQ_CCA_GPIO(0)) && (irq <= IRQ_CCA_GPIO(IRQ_CCA_GPIO_N - 1))) {
		return irq - IRQ_CCA_GPIO(0);
	}
#endif
	return -EINVAL;
}

#define ARCH_NR_GPIOS IRQ_CCA_GPIO_N
#include <asm-generic/gpio.h>

#endif /* _ASM_ARCH_GPIO_H */
