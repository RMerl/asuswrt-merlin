/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 MIPS Technologies, Inc.
 *   written by Ralf Baechle (ralf@linux-mips.org)
 *
 * Probe driver for the SEAD3 network device
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <irq.h>


struct resource i2c_resources[] = {
	{
		.start			= 0x805200,
		.end			= 0x8053FF,
		.flags			= IORESOURCE_MEM
	},
};

static struct platform_device i2c_device = {
	.name			= "i2c_pic32",
	.id			= 2,
	.num_resources		= ARRAY_SIZE(i2c_resources),
	.resource		= i2c_resources
};

static int __init i2c_init(void)
{
	return platform_device_register(&i2c_device);
}

module_init(i2c_init);

MODULE_AUTHOR("Chris Dearman <chris@mips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C probe driver for SEAD3");
