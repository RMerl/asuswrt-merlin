/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 MIPS Technologies, Inc.
 *   written by Chris Dearman (chris@mips.com)
 *
 * Probe driver for the SEAD3 LED devices
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <irq.h>


#define LEDFLAGS(bits, shift)		\
	((bits << 8) | (shift << 8))

#define LEDBITS(id, shift, bits)	\
	.name = id #shift,		\
	.flags = LEDFLAGS(bits, shift)

struct led_info led_data_info[] = {
	{ LEDBITS("bit", 0, 1) },
	{ LEDBITS("bit", 1, 1) },
	{ LEDBITS("bit", 2, 1) },
	{ LEDBITS("bit", 3, 1) },
	{ LEDBITS("bit", 4, 1) },
	{ LEDBITS("bit", 5, 1) },
	{ LEDBITS("bit", 6, 1) },
	{ LEDBITS("bit", 7, 1) },
	{ LEDBITS("all", 0, 8) },
};

static struct led_platform_data led_data = {
	.num_leds	= ARRAY_SIZE(led_data_info),
	.leds		= led_data_info
};

static struct resource pled_resources[] = {
	{
		.start			= 0x1F000210,
		.end			= 0x1F000217,
		.flags			= IORESOURCE_MEM
	}
};

static struct platform_device pled_device = {
	.name			= "sead3::pled",
	.id			= 0,
	.dev			= {
		.platform_data	= &led_data,
	},
	.num_resources		= ARRAY_SIZE(pled_resources),
	.resource		= pled_resources
};


static struct resource fled_resources[] = {
	{
		.start			= 0x1F000218,
		.end			= 0x1F00021f,
		.flags			= IORESOURCE_MEM
	}
};

static struct platform_device fled_device = {
	.name			= "sead3::fled",
	.id			= 0,
	.dev			= {
		.platform_data	= &led_data,
	},
	.num_resources		= ARRAY_SIZE(fled_resources),
	.resource		= fled_resources
};


static int __init led_init(void)
{
	platform_device_register(&pled_device);
	return platform_device_register(&fled_device);
}

module_init(led_init);

MODULE_AUTHOR("Chris Dearman <chris@mips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED probe driver for SEAD3");
