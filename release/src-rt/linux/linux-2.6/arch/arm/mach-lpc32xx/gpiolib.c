/*
 * arch/arm/mach-lpc32xx/gpiolib.c
 *
 * Author: Kevin Wells <kevin.wells@nxp.com>
 *
 * Copyright (C) 2010 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/gpio.h>

#include <mach/hardware.h>
#include <mach/platform.h>
#include "common.h"

#define LPC32XX_GPIO_P3_INP_STATE		_GPREG(0x000)
#define LPC32XX_GPIO_P3_OUTP_SET		_GPREG(0x004)
#define LPC32XX_GPIO_P3_OUTP_CLR		_GPREG(0x008)
#define LPC32XX_GPIO_P3_OUTP_STATE		_GPREG(0x00C)
#define LPC32XX_GPIO_P2_DIR_SET			_GPREG(0x010)
#define LPC32XX_GPIO_P2_DIR_CLR			_GPREG(0x014)
#define LPC32XX_GPIO_P2_DIR_STATE		_GPREG(0x018)
#define LPC32XX_GPIO_P2_INP_STATE		_GPREG(0x01C)
#define LPC32XX_GPIO_P2_OUTP_SET		_GPREG(0x020)
#define LPC32XX_GPIO_P2_OUTP_CLR		_GPREG(0x024)
#define LPC32XX_GPIO_P2_MUX_SET			_GPREG(0x028)
#define LPC32XX_GPIO_P2_MUX_CLR			_GPREG(0x02C)
#define LPC32XX_GPIO_P2_MUX_STATE		_GPREG(0x030)
#define LPC32XX_GPIO_P0_INP_STATE		_GPREG(0x040)
#define LPC32XX_GPIO_P0_OUTP_SET		_GPREG(0x044)
#define LPC32XX_GPIO_P0_OUTP_CLR		_GPREG(0x048)
#define LPC32XX_GPIO_P0_OUTP_STATE		_GPREG(0x04C)
#define LPC32XX_GPIO_P0_DIR_SET			_GPREG(0x050)
#define LPC32XX_GPIO_P0_DIR_CLR			_GPREG(0x054)
#define LPC32XX_GPIO_P0_DIR_STATE		_GPREG(0x058)
#define LPC32XX_GPIO_P1_INP_STATE		_GPREG(0x060)
#define LPC32XX_GPIO_P1_OUTP_SET		_GPREG(0x064)
#define LPC32XX_GPIO_P1_OUTP_CLR		_GPREG(0x068)
#define LPC32XX_GPIO_P1_OUTP_STATE		_GPREG(0x06C)
#define LPC32XX_GPIO_P1_DIR_SET			_GPREG(0x070)
#define LPC32XX_GPIO_P1_DIR_CLR			_GPREG(0x074)
#define LPC32XX_GPIO_P1_DIR_STATE		_GPREG(0x078)

#define GPIO012_PIN_TO_BIT(x)			(1 << (x))
#define GPIO3_PIN_TO_BIT(x)			(1 << ((x) + 25))
#define GPO3_PIN_TO_BIT(x)			(1 << (x))
#define GPIO012_PIN_IN_SEL(x, y)		(((x) >> (y)) & 1)
#define GPIO3_PIN_IN_SHIFT(x)			((x) == 5 ? 24 : 10 + (x))
#define GPIO3_PIN_IN_SEL(x, y)			((x) >> GPIO3_PIN_IN_SHIFT(y))
#define GPIO3_PIN5_IN_SEL(x)			(((x) >> 24) & 1)
#define GPI3_PIN_IN_SEL(x, y)			(((x) >> (y)) & 1)

struct gpio_regs {
	void __iomem *inp_state;
	void __iomem *outp_set;
	void __iomem *outp_clr;
	void __iomem *dir_set;
	void __iomem *dir_clr;
};

/*
 * GPIO names
 */
static const char *gpio_p0_names[LPC32XX_GPIO_P0_MAX] = {
	"p0.0", "p0.1", "p0.2", "p0.3",
	"p0.4", "p0.5", "p0.6", "p0.7"
};

static const char *gpio_p1_names[LPC32XX_GPIO_P1_MAX] = {
	"p1.0", "p1.1", "p1.2", "p1.3",
	"p1.4", "p1.5", "p1.6", "p1.7",
	"p1.8", "p1.9", "p1.10", "p1.11",
	"p1.12", "p1.13", "p1.14", "p1.15",
	"p1.16", "p1.17", "p1.18", "p1.19",
	"p1.20", "p1.21", "p1.22", "p1.23",
};

static const char *gpio_p2_names[LPC32XX_GPIO_P2_MAX] = {
	"p2.0", "p2.1", "p2.2", "p2.3",
	"p2.4", "p2.5", "p2.6", "p2.7",
	"p2.8", "p2.9", "p2.10", "p2.11",
	"p2.12"
};

static const char *gpio_p3_names[LPC32XX_GPIO_P3_MAX] = {
	"gpi000", "gpio01", "gpio02", "gpio03",
	"gpio04", "gpio05"
};

static const char *gpi_p3_names[LPC32XX_GPI_P3_MAX] = {
	"gpi00", "gpi01", "gpi02", "gpi03",
	"gpi04", "gpi05", "gpi06", "gpi07",
	"gpi08", "gpi09",  NULL,    NULL,
	 NULL,    NULL,    NULL,   "gpi15",
	"gpi16", "gpi17", "gpi18", "gpi19",
	"gpi20", "gpi21", "gpi22", "gpi23",
	"gpi24", "gpi25", "gpi26", "gpi27"
};

static const char *gpo_p3_names[LPC32XX_GPO_P3_MAX] = {
	"gpo00", "gpo01", "gpo02", "gpo03",
	"gpo04", "gpo05", "gpo06", "gpo07",
	"gpo08", "gpo09", "gpo10", "gpo11",
	"gpo12", "gpo13", "gpo14", "gpo15",
	"gpo16", "gpo17", "gpo18", "gpo19",
	"gpo20", "gpo21", "gpo22", "gpo23"
};

static struct gpio_regs gpio_grp_regs_p0 = {
	.inp_state	= LPC32XX_GPIO_P0_INP_STATE,
	.outp_set	= LPC32XX_GPIO_P0_OUTP_SET,
	.outp_clr	= LPC32XX_GPIO_P0_OUTP_CLR,
	.dir_set	= LPC32XX_GPIO_P0_DIR_SET,
	.dir_clr	= LPC32XX_GPIO_P0_DIR_CLR,
};

static struct gpio_regs gpio_grp_regs_p1 = {
	.inp_state	= LPC32XX_GPIO_P1_INP_STATE,
	.outp_set	= LPC32XX_GPIO_P1_OUTP_SET,
	.outp_clr	= LPC32XX_GPIO_P1_OUTP_CLR,
	.dir_set	= LPC32XX_GPIO_P1_DIR_SET,
	.dir_clr	= LPC32XX_GPIO_P1_DIR_CLR,
};

static struct gpio_regs gpio_grp_regs_p2 = {
	.inp_state	= LPC32XX_GPIO_P2_INP_STATE,
	.outp_set	= LPC32XX_GPIO_P2_OUTP_SET,
	.outp_clr	= LPC32XX_GPIO_P2_OUTP_CLR,
	.dir_set	= LPC32XX_GPIO_P2_DIR_SET,
	.dir_clr	= LPC32XX_GPIO_P2_DIR_CLR,
};

static struct gpio_regs gpio_grp_regs_p3 = {
	.inp_state	= LPC32XX_GPIO_P3_INP_STATE,
	.outp_set	= LPC32XX_GPIO_P3_OUTP_SET,
	.outp_clr	= LPC32XX_GPIO_P3_OUTP_CLR,
	.dir_set	= LPC32XX_GPIO_P2_DIR_SET,
	.dir_clr	= LPC32XX_GPIO_P2_DIR_CLR,
};

struct lpc32xx_gpio_chip {
	struct gpio_chip	chip;
	struct gpio_regs	*gpio_grp;
};

static inline struct lpc32xx_gpio_chip *to_lpc32xx_gpio(
	struct gpio_chip *gpc)
{
	return container_of(gpc, struct lpc32xx_gpio_chip, chip);
}

static void __set_gpio_dir_p012(struct lpc32xx_gpio_chip *group,
	unsigned pin, int input)
{
	if (input)
		__raw_writel(GPIO012_PIN_TO_BIT(pin),
			group->gpio_grp->dir_clr);
	else
		__raw_writel(GPIO012_PIN_TO_BIT(pin),
			group->gpio_grp->dir_set);
}

static void __set_gpio_dir_p3(struct lpc32xx_gpio_chip *group,
	unsigned pin, int input)
{
	u32 u = GPIO3_PIN_TO_BIT(pin);

	if (input)
		__raw_writel(u, group->gpio_grp->dir_clr);
	else
		__raw_writel(u, group->gpio_grp->dir_set);
}

static void __set_gpio_level_p012(struct lpc32xx_gpio_chip *group,
	unsigned pin, int high)
{
	if (high)
		__raw_writel(GPIO012_PIN_TO_BIT(pin),
			group->gpio_grp->outp_set);
	else
		__raw_writel(GPIO012_PIN_TO_BIT(pin),
			group->gpio_grp->outp_clr);
}

static void __set_gpio_level_p3(struct lpc32xx_gpio_chip *group,
	unsigned pin, int high)
{
	u32 u = GPIO3_PIN_TO_BIT(pin);

	if (high)
		__raw_writel(u, group->gpio_grp->outp_set);
	else
		__raw_writel(u, group->gpio_grp->outp_clr);
}

static void __set_gpo_level_p3(struct lpc32xx_gpio_chip *group,
	unsigned pin, int high)
{
	if (high)
		__raw_writel(GPO3_PIN_TO_BIT(pin), group->gpio_grp->outp_set);
	else
		__raw_writel(GPO3_PIN_TO_BIT(pin), group->gpio_grp->outp_clr);
}

static int __get_gpio_state_p012(struct lpc32xx_gpio_chip *group,
	unsigned pin)
{
	return GPIO012_PIN_IN_SEL(__raw_readl(group->gpio_grp->inp_state),
		pin);
}

static int __get_gpio_state_p3(struct lpc32xx_gpio_chip *group,
	unsigned pin)
{
	int state = __raw_readl(group->gpio_grp->inp_state);

	/*
	 * P3 GPIO pin input mapping is not contiguous, GPIOP3-0..4 is mapped
	 * to bits 10..14, while GPIOP3-5 is mapped to bit 24.
	 */
	return GPIO3_PIN_IN_SEL(state, pin);
}

static int __get_gpi_state_p3(struct lpc32xx_gpio_chip *group,
	unsigned pin)
{
	return GPI3_PIN_IN_SEL(__raw_readl(group->gpio_grp->inp_state), pin);
}

/*
 * GENERIC_GPIO primitives.
 */
static int lpc32xx_gpio_dir_input_p012(struct gpio_chip *chip,
	unsigned pin)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_dir_p012(group, pin, 1);

	return 0;
}

static int lpc32xx_gpio_dir_input_p3(struct gpio_chip *chip,
	unsigned pin)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_dir_p3(group, pin, 1);

	return 0;
}

static int lpc32xx_gpio_dir_in_always(struct gpio_chip *chip,
	unsigned pin)
{
	return 0;
}

static int lpc32xx_gpio_get_value_p012(struct gpio_chip *chip, unsigned pin)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	return __get_gpio_state_p012(group, pin);
}

static int lpc32xx_gpio_get_value_p3(struct gpio_chip *chip, unsigned pin)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	return __get_gpio_state_p3(group, pin);
}

static int lpc32xx_gpi_get_value(struct gpio_chip *chip, unsigned pin)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	return __get_gpi_state_p3(group, pin);
}

static int lpc32xx_gpio_dir_output_p012(struct gpio_chip *chip, unsigned pin,
	int value)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_dir_p012(group, pin, 0);

	return 0;
}

static int lpc32xx_gpio_dir_output_p3(struct gpio_chip *chip, unsigned pin,
	int value)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_dir_p3(group, pin, 0);

	return 0;
}

static int lpc32xx_gpio_dir_out_always(struct gpio_chip *chip, unsigned pin,
	int value)
{
	return 0;
}

static void lpc32xx_gpio_set_value_p012(struct gpio_chip *chip, unsigned pin,
	int value)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_level_p012(group, pin, value);
}

static void lpc32xx_gpio_set_value_p3(struct gpio_chip *chip, unsigned pin,
	int value)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpio_level_p3(group, pin, value);
}

static void lpc32xx_gpo_set_value(struct gpio_chip *chip, unsigned pin,
	int value)
{
	struct lpc32xx_gpio_chip *group = to_lpc32xx_gpio(chip);

	__set_gpo_level_p3(group, pin, value);
}

static int lpc32xx_gpio_request(struct gpio_chip *chip, unsigned pin)
{
	if (pin < chip->ngpio)
		return 0;

	return -EINVAL;
}

static struct lpc32xx_gpio_chip lpc32xx_gpiochip[] = {
	{
		.chip = {
			.label			= "gpio_p0",
			.direction_input	= lpc32xx_gpio_dir_input_p012,
			.get			= lpc32xx_gpio_get_value_p012,
			.direction_output	= lpc32xx_gpio_dir_output_p012,
			.set			= lpc32xx_gpio_set_value_p012,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPIO_P0_GRP,
			.ngpio			= LPC32XX_GPIO_P0_MAX,
			.names			= gpio_p0_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p0,
	},
	{
		.chip = {
			.label			= "gpio_p1",
			.direction_input	= lpc32xx_gpio_dir_input_p012,
			.get			= lpc32xx_gpio_get_value_p012,
			.direction_output	= lpc32xx_gpio_dir_output_p012,
			.set			= lpc32xx_gpio_set_value_p012,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPIO_P1_GRP,
			.ngpio			= LPC32XX_GPIO_P1_MAX,
			.names			= gpio_p1_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p1,
	},
	{
		.chip = {
			.label			= "gpio_p2",
			.direction_input	= lpc32xx_gpio_dir_input_p012,
			.get			= lpc32xx_gpio_get_value_p012,
			.direction_output	= lpc32xx_gpio_dir_output_p012,
			.set			= lpc32xx_gpio_set_value_p012,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPIO_P2_GRP,
			.ngpio			= LPC32XX_GPIO_P2_MAX,
			.names			= gpio_p2_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p2,
	},
	{
		.chip = {
			.label			= "gpio_p3",
			.direction_input	= lpc32xx_gpio_dir_input_p3,
			.get			= lpc32xx_gpio_get_value_p3,
			.direction_output	= lpc32xx_gpio_dir_output_p3,
			.set			= lpc32xx_gpio_set_value_p3,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPIO_P3_GRP,
			.ngpio			= LPC32XX_GPIO_P3_MAX,
			.names			= gpio_p3_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p3,
	},
	{
		.chip = {
			.label			= "gpi_p3",
			.direction_input	= lpc32xx_gpio_dir_in_always,
			.get			= lpc32xx_gpi_get_value,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPI_P3_GRP,
			.ngpio			= LPC32XX_GPI_P3_MAX,
			.names			= gpi_p3_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p3,
	},
	{
		.chip = {
			.label			= "gpo_p3",
			.direction_output	= lpc32xx_gpio_dir_out_always,
			.set			= lpc32xx_gpo_set_value,
			.request		= lpc32xx_gpio_request,
			.base			= LPC32XX_GPO_P3_GRP,
			.ngpio			= LPC32XX_GPO_P3_MAX,
			.names			= gpo_p3_names,
			.can_sleep		= 0,
		},
		.gpio_grp = &gpio_grp_regs_p3,
	},
};

void __init lpc32xx_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lpc32xx_gpiochip); i++)
		gpiochip_add(&lpc32xx_gpiochip[i].chip);
}
