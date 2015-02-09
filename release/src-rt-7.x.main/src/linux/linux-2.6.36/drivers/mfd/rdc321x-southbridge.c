/*
 * RDC321x MFD southbrige driver
 *
 * Copyright (C) 2007-2010 Florian Fainelli <florian@openwrt.org>
 * Copyright (C) 2010 Bernhard Loos <bernhardloos@googlemail.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rdc321x.h>

static struct rdc321x_wdt_pdata rdc321x_wdt_pdata;

static struct resource rdc321x_wdt_resource[] = {
	{
		.name	= "wdt-reg",
		.start	= RDC321X_WDT_CTRL,
		.end	= RDC321X_WDT_CTRL + 0x3,
		.flags	= IORESOURCE_IO,
	}
};

static struct rdc321x_gpio_pdata rdc321x_gpio_pdata = {
	.max_gpios	= RDC321X_MAX_GPIO,
};

static struct resource rdc321x_gpio_resources[] = {
	{
		.name	= "gpio-reg1",
		.start	= RDC321X_GPIO_CTRL_REG1,
		.end	= RDC321X_GPIO_CTRL_REG1 + 0x7,
		.flags	= IORESOURCE_IO,
	}, {
		.name	= "gpio-reg2",
		.start	= RDC321X_GPIO_CTRL_REG2,
		.end	= RDC321X_GPIO_CTRL_REG2 + 0x7,
		.flags	= IORESOURCE_IO,
	}
};

static struct mfd_cell rdc321x_sb_cells[] = {
	{
		.name		= "rdc321x-wdt",
		.resources	= rdc321x_wdt_resource,
		.num_resources	= ARRAY_SIZE(rdc321x_wdt_resource),
		.driver_data	= &rdc321x_wdt_pdata,
	}, {
		.name		= "rdc321x-gpio",
		.resources	= rdc321x_gpio_resources,
		.num_resources	= ARRAY_SIZE(rdc321x_gpio_resources),
		.driver_data	= &rdc321x_gpio_pdata,
	},
};

static int __devinit rdc321x_sb_probe(struct pci_dev *pdev,
					const struct pci_device_id *ent)
{
	int err;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "failed to enable device\n");
		return err;
	}

	rdc321x_gpio_pdata.sb_pdev = pdev;
	rdc321x_wdt_pdata.sb_pdev = pdev;

	return mfd_add_devices(&pdev->dev, -1,
		rdc321x_sb_cells, ARRAY_SIZE(rdc321x_sb_cells), NULL, 0);
}

static void __devexit rdc321x_sb_remove(struct pci_dev *pdev)
{
	mfd_remove_devices(&pdev->dev);
}

static DEFINE_PCI_DEVICE_TABLE(rdc321x_sb_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_RDC, PCI_DEVICE_ID_RDC_R6030) },
	{}
};

static struct pci_driver rdc321x_sb_driver = {
	.name		= "RDC321x Southbridge",
	.id_table	= rdc321x_sb_table,
	.probe		= rdc321x_sb_probe,
	.remove		= __devexit_p(rdc321x_sb_remove),
};

static int __init rdc321x_sb_init(void)
{
	return pci_register_driver(&rdc321x_sb_driver);
}

static void __exit rdc321x_sb_exit(void)
{
	pci_unregister_driver(&rdc321x_sb_driver);
}

module_init(rdc321x_sb_init);
module_exit(rdc321x_sb_exit);

MODULE_AUTHOR("Florian Fainelli <florian@openwrt.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RDC R-321x MFD southbridge driver");
