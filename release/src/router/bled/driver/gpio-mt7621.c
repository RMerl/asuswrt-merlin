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

/* copy from rt_mmap.h, ralink_gpio.h */
#define RALINK_PIO_BASE			0xBE000600
#define RALINK_PRGIO_ADDR		RALINK_PIO_BASE // Programmable I/O
#define RALINK_REG_PIODATA		(RALINK_PRGIO_ADDR + 0x20)
#define RALINK_REG_PIOSET		(RALINK_PRGIO_ADDR + 0x30)
#define RALINK_REG_PIORESET		(RALINK_PRGIO_ADDR + 0x40)

/* ralink_gpio.h RALINK_GPIO_HAS_7224 */
#define RALINK_REG_PIO6332DATA          (RALINK_PRGIO_ADDR + 0x24)
#define RALINK_REG_PIO6332SET           (RALINK_PRGIO_ADDR + 0x34)
#define RALINK_REG_PIO6332RESET         (RALINK_PRGIO_ADDR + 0x44)
#define RALINK_REG_PIO9564DATA          (RALINK_PRGIO_ADDR + 0x28)
#define RALINK_REG_PIO9564SET           (RALINK_PRGIO_ADDR + 0x38)
#define RALINK_REG_PIO9564RESET         (RALINK_PRGIO_ADDR + 0x48)


static struct mt7621_gpio_reg_s {
	int start;
	int end;
	unsigned int reg_data;
	unsigned int reg_set;
	unsigned int reg_clear;
} mt7621_gpio_reg_tbl[] = {
	{ 0,	31, RALINK_REG_PIODATA, RALINK_REG_PIOSET, RALINK_REG_PIORESET },
	{ 32,	63, RALINK_REG_PIO6332DATA, RALINK_REG_PIO6332SET, RALINK_REG_PIO6332RESET },
	{ 64,	95, RALINK_REG_PIO9564DATA, RALINK_REG_PIO9564SET, RALINK_REG_PIO9564RESET },
	{ -1, -1, 0, 0 },
};

/**
 * Get set/clear register base on GPIO# and value.
 * @gpio_nr:
 * @value:
 * 	> 0:	return set register
 * 	= 0:	return clear register
 * 	< 0:	return data register
 * @return:
 * 	0:	success.
 *  otherwise:	not found
 */
static int get_gpio_register(int gpio_nr, int value, u32 *reg, u32 *mask)
{
	int ret = -2;
	struct mt7621_gpio_reg_s *p;

	if (!reg || !mask)
		return -1;
	for (p = &mt7621_gpio_reg_tbl[0]; p->start >= 0; ++p) {
		if (gpio_nr < p->start || gpio_nr > p->end)
			continue;

		if (value > 0)
			*reg = p->reg_set;
		else if (value == 0)
			*reg = p->reg_clear;
		else
			*reg = p->reg_data;

		*mask = 1U << (gpio_nr - p->start);
		
		ret = 0;
	}

	return ret;
}

static void mt7621_gpio_set(int gpio_nr, int value)
{
	u32 reg = 0, mask = 0;
	if (get_gpio_register(gpio_nr, value, &reg, &mask)) {
		return;
	}
	*(volatile u32*)reg = mask;
}

static int mt7621_gpio_get(int gpio_nr)
{
	u32 reg = 0, mask = 0;
	if (get_gpio_register(gpio_nr, -1, &reg, &mask)) {
		return 0;
	}
	return !!(*(volatile u32*)reg & mask);
}

struct gpio_api_s gpio_api_tbl[GPIO_API_MAX] = {
	[GPIO_API_PLATFORM] =
	{
		.gpio_set = mt7621_gpio_set,
		.gpio_get = mt7621_gpio_get
	},
};
