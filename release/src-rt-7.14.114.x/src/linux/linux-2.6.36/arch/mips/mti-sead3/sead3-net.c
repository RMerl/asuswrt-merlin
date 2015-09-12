/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007-2011 MIPS Technologies, Inc.
 *   written by Ralf Baechle (ralf@linux-mips.org)
 *
 * Probe driver for the SEAD3 network device
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/smsc911x.h>
#include <irq.h>

static struct smsc911x_platform_config net_data = {
	.irq_polarity = SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type = SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags	= SMSC911X_USE_32BIT | SMSC911X_SAVE_MAC_ADDRESS,
	.phy_interface = PHY_INTERFACE_MODE_MII,
};

struct resource net_resources[] = {
	{
		.start                  = 0x1f010000,
		.end                    = 0x1f01ffff,
		.flags			= IORESOURCE_MEM
	},
	{
		.start			= MIPS_CPU_IRQ_BASE + 6,
		.flags			= IORESOURCE_IRQ
	}
};

static struct platform_device net_device = {
	.name			= "smsc911x",
	.id			= 0,
	.dev			= {
		.platform_data	= &net_data,
	},
	.num_resources		= ARRAY_SIZE(net_resources),
	.resource		= net_resources
};

static int __init net_init(void)
{
	return platform_device_register(&net_device);
}

module_init(net_init);

MODULE_AUTHOR("Chris Dearman <chris@mips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Network probe driver for SEAD3");
