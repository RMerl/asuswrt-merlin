/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include "../bled_defs.h"
#include "gpio_api.h"

/* gpio_set_value() and gpio_get_value() are provided and exported by arch/mips/ath79/gpio.c */
extern void gpio_set_value(unsigned gpio, int value);
extern int gpio_get_value(unsigned gpio);

static void qca955x_gpio_set(int gpio_nr, int value)
{
	gpio_set_value(gpio_nr, value);
}

static int qca955x_gpio_get(int gpio_nr)
{
	return gpio_get_value(gpio_nr);
}

struct gpio_api_s gpio_api_tbl[GPIO_API_MAX] = {
	[GPIO_API_PLATFORM] =
	{
		.gpio_set = qca955x_gpio_set,
		.gpio_get = qca955x_gpio_get
	},
};
