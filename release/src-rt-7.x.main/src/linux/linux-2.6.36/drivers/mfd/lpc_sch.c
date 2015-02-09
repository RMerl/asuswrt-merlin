/*
 *  lpc_sch.c - LPC interface for Intel Poulsbo SCH
 *
 *  LPC bridge function of the Intel SCH contains many other
 *  functional units, such as Interrupt controllers, Timers,
 *  Power Management, System Management, GPIO, RTC, and LPC
 *  Configuration Registers.
 *
 *  Copyright (c) 2010 CompuLab Ltd
 *  Author: Denis Turischev <denis@compulab.co.il>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/acpi.h>
#include <linux/pci.h>
#include <linux/mfd/core.h>

#define SMBASE		0x40
#define SMBUS_IO_SIZE	64

#define GPIOBASE	0x44
#define GPIO_IO_SIZE	64

static struct resource smbus_sch_resource = {
		.flags = IORESOURCE_IO,
};


static struct resource gpio_sch_resource = {
		.flags = IORESOURCE_IO,
};

static struct mfd_cell lpc_sch_cells[] = {
	{
		.name = "isch_smbus",
		.num_resources = 1,
		.resources = &smbus_sch_resource,
	},
	{
		.name = "sch_gpio",
		.num_resources = 1,
		.resources = &gpio_sch_resource,
	},
};

static struct pci_device_id lpc_sch_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SCH_LPC) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, lpc_sch_ids);

static int __devinit lpc_sch_probe(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	unsigned int base_addr_cfg;
	unsigned short base_addr;

	pci_read_config_dword(dev, SMBASE, &base_addr_cfg);
	if (!(base_addr_cfg & (1 << 31))) {
		dev_err(&dev->dev, "Decode of the SMBus I/O range disabled\n");
		return -ENODEV;
	}
	base_addr = (unsigned short)base_addr_cfg;
	if (base_addr == 0) {
		dev_err(&dev->dev, "I/O space for SMBus uninitialized\n");
		return -ENODEV;
	}

	smbus_sch_resource.start = base_addr;
	smbus_sch_resource.end = base_addr + SMBUS_IO_SIZE - 1;

	pci_read_config_dword(dev, GPIOBASE, &base_addr_cfg);
	if (!(base_addr_cfg & (1 << 31))) {
		dev_err(&dev->dev, "Decode of the GPIO I/O range disabled\n");
		return -ENODEV;
	}
	base_addr = (unsigned short)base_addr_cfg;
	if (base_addr == 0) {
		dev_err(&dev->dev, "I/O space for GPIO uninitialized\n");
		return -ENODEV;
	}

	gpio_sch_resource.start = base_addr;
	gpio_sch_resource.end = base_addr + GPIO_IO_SIZE - 1;

	return mfd_add_devices(&dev->dev, -1,
			lpc_sch_cells, ARRAY_SIZE(lpc_sch_cells), NULL, 0);
}

static void __devexit lpc_sch_remove(struct pci_dev *dev)
{
	mfd_remove_devices(&dev->dev);
}

static struct pci_driver lpc_sch_driver = {
	.name		= "lpc_sch",
	.id_table	= lpc_sch_ids,
	.probe		= lpc_sch_probe,
	.remove		= __devexit_p(lpc_sch_remove),
};

static int __init lpc_sch_init(void)
{
	return pci_register_driver(&lpc_sch_driver);
}

static void __exit lpc_sch_exit(void)
{
	pci_unregister_driver(&lpc_sch_driver);
}

module_init(lpc_sch_init);
module_exit(lpc_sch_exit);

MODULE_AUTHOR("Denis Turischev <denis@compulab.co.il>");
MODULE_DESCRIPTION("LPC interface for Intel Poulsbo SCH");
MODULE_LICENSE("GPL");
