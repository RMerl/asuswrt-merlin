/*
    comedi/drivers/adl_pci8164.c

    Hardware comedi driver fot PCI-8164 Adlink card
    Copyright (C) 2004 Michel Lachine <mike@mikelachaine.ca>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
/*
Driver: adl_pci8164
Description: Driver for the Adlink PCI-8164 4 Axes Motion Control board
Devices: [ADLink] PCI-8164 (adl_pci8164)
Author: Michel Lachaine <mike@mikelachaine.ca>
Status: experimental
Updated: Mon, 14 Apr 2008 15:10:32 +0100

Configuration Options:
  [0] - PCI bus of device (optional)
  [1] - PCI slot of device (optional)
  If bus/slot is not specified, the first supported
  PCI device found will be used.
*/

#include "../comedidev.h"
#include <linux/kernel.h>
#include <linux/delay.h>
#include "comedi_fc.h"
#include "comedi_pci.h"
#include "8253.h"

#define PCI8164_AXIS_X  0x00
#define PCI8164_AXIS_Y  0x08
#define PCI8164_AXIS_Z  0x10
#define PCI8164_AXIS_U  0x18

#define PCI8164_MSTS	0x00
#define PCI8164_SSTS    0x02
#define PCI8164_BUF0    0x04
#define PCI8164_BUF1    0x06

#define PCI8164_CMD     0x00
#define PCI8164_OTP     0x02

#define PCI_DEVICE_ID_PCI8164 0x8164

static DEFINE_PCI_DEVICE_TABLE(adl_pci8164_pci_table) = {
	{
	PCI_VENDOR_ID_ADLINK, PCI_DEVICE_ID_PCI8164, PCI_ANY_ID,
		    PCI_ANY_ID, 0, 0, 0}, {
	0}
};

MODULE_DEVICE_TABLE(pci, adl_pci8164_pci_table);

struct adl_pci8164_private {
	int data;
	struct pci_dev *pci_dev;
};

#define devpriv ((struct adl_pci8164_private *)dev->private)

static int adl_pci8164_attach(struct comedi_device *dev,
			      struct comedi_devconfig *it);
static int adl_pci8164_detach(struct comedi_device *dev);
static struct comedi_driver driver_adl_pci8164 = {
	.driver_name = "adl_pci8164",
	.module = THIS_MODULE,
	.attach = adl_pci8164_attach,
	.detach = adl_pci8164_detach,
};

static int adl_pci8164_insn_read_msts(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_read_ssts(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_read_buf0(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_read_buf1(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_write_cmd(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_write_otp(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data);

static int adl_pci8164_insn_write_buf0(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data);

static int adl_pci8164_insn_write_buf1(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data);

static int adl_pci8164_attach(struct comedi_device *dev,
			      struct comedi_devconfig *it)
{
	struct pci_dev *pcidev = NULL;
	struct comedi_subdevice *s;
	int bus, slot;

	printk(KERN_INFO "comedi: attempt to attach...\n");
	printk(KERN_INFO "comedi%d: adl_pci8164\n", dev->minor);

	dev->board_name = "pci8164";
	bus = it->options[0];
	slot = it->options[1];

	if (alloc_private(dev, sizeof(struct adl_pci8164_private)) < 0)
		return -ENOMEM;

	if (alloc_subdevices(dev, 4) < 0)
		return -ENOMEM;

	for_each_pci_dev(pcidev) {
		if (pcidev->vendor == PCI_VENDOR_ID_ADLINK &&
		    pcidev->device == PCI_DEVICE_ID_PCI8164) {
			if (bus || slot) {
				/* requested particular bus/slot */
				if (pcidev->bus->number != bus
					|| PCI_SLOT(pcidev->devfn) != slot)
					continue;
			}
			devpriv->pci_dev = pcidev;
			if (comedi_pci_enable(pcidev, "adl_pci8164") < 0) {
				printk(KERN_ERR "comedi%d: Failed to enable "
				"PCI device and request regions\n", dev->minor);
				return -EIO;
			}
			dev->iobase = pci_resource_start(pcidev, 2);
			printk(KERN_DEBUG "comedi: base addr %4lx\n",
				   dev->iobase);

			s = dev->subdevices + 0;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			/* s->range_table = &range_axis; */
			s->insn_read = adl_pci8164_insn_read_msts;
			s->insn_write = adl_pci8164_insn_write_cmd;

			s = dev->subdevices + 1;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			/* s->range_table = &range_axis; */
			s->insn_read = adl_pci8164_insn_read_ssts;
			s->insn_write = adl_pci8164_insn_write_otp;

			s = dev->subdevices + 2;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			/* s->range_table = &range_axis; */
			s->insn_read = adl_pci8164_insn_read_buf0;
			s->insn_write = adl_pci8164_insn_write_buf0;

			s = dev->subdevices + 3;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			/* s->range_table = &range_axis; */
			s->insn_read = adl_pci8164_insn_read_buf1;
			s->insn_write = adl_pci8164_insn_write_buf1;

			printk(KERN_INFO "comedi: attached\n");

			return 1;
		}
	}

	printk(KERN_ERR "comedi%d: no supported board found!"
		   "(req. bus/slot : %d/%d)\n", dev->minor, bus, slot);
	return -EIO;
}

static int adl_pci8164_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: pci8164: remove\n", dev->minor);

	if (devpriv && devpriv->pci_dev) {
		if (dev->iobase)
			comedi_pci_disable(devpriv->pci_dev);
		pci_dev_put(devpriv->pci_dev);
	}

	return 0;
}

/*
 all the read commands are the same except for the addition a constant
 * const to the data for inw()
 */
static void adl_pci8164_insn_read(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data,
				  char *action, unsigned short offset)
{
	int axis, axis_reg;
	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis) {
	case 0:
		axis_reg = PCI8164_AXIS_X;
		axisname = "X";
		break;
	case 1:
		axis_reg = PCI8164_AXIS_Y;
		axisname = "Y";
		break;
	case 2:
		axis_reg = PCI8164_AXIS_Z;
		axisname = "Z";
		break;
	case 3:
		axis_reg = PCI8164_AXIS_U;
		axisname = "U";
		break;
	default:
		axis_reg = PCI8164_AXIS_X;
		axisname = "X";
	}

	data[0] = inw(dev->iobase + axis_reg + offset);
	printk(KERN_DEBUG "comedi: pci8164 %s read -> "
						  "%04X:%04X on axis %s\n",
				action, data[0], data[1], axisname);
}

static int adl_pci8164_insn_read_msts(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_read(dev, s, insn, data, "MSTS", PCI8164_MSTS);
	return 2;
}

static int adl_pci8164_insn_read_ssts(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_read(dev, s, insn, data, "SSTS", PCI8164_SSTS);
	return 2;
}

static int adl_pci8164_insn_read_buf0(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_read(dev, s, insn, data, "BUF0", PCI8164_BUF0);
	return 2;
}

static int adl_pci8164_insn_read_buf1(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_read(dev, s, insn, data, "BUF1", PCI8164_BUF1);
	return 2;
}

/*
 all the write commands are the same except for the addition a constant
 * const to the data for outw()
 */
static void adl_pci8164_insn_out(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data,
				 char *action, unsigned short offset)
{
	unsigned int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis) {
	case 0:
		axis_reg = PCI8164_AXIS_X;
		axisname = "X";
		break;
	case 1:
		axis_reg = PCI8164_AXIS_Y;
		axisname = "Y";
		break;
	case 2:
		axis_reg = PCI8164_AXIS_Z;
		axisname = "Z";
		break;
	case 3:
		axis_reg = PCI8164_AXIS_U;
		axisname = "U";
		break;
	default:
		axis_reg = PCI8164_AXIS_X;
		axisname = "X";
	}

	outw(data[0], dev->iobase + axis_reg + offset);

	printk(KERN_DEBUG "comedi: pci8164 %s write -> "
						"%04X:%04X on axis %s\n",
				action, data[0], data[1], axisname);

}

static int adl_pci8164_insn_write_cmd(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_out(dev, s, insn, data, "CMD", PCI8164_CMD);
	return 2;
}

static int adl_pci8164_insn_write_otp(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	adl_pci8164_insn_out(dev, s, insn, data, "OTP", PCI8164_OTP);
	return 2;
}

static int adl_pci8164_insn_write_buf0(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data)
{
	adl_pci8164_insn_out(dev, s, insn, data, "BUF0", PCI8164_BUF0);
	return 2;
}

static int adl_pci8164_insn_write_buf1(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data)
{
	adl_pci8164_insn_out(dev, s, insn, data, "BUF1", PCI8164_BUF1);
	return 2;
}

static int __devinit driver_adl_pci8164_pci_probe(struct pci_dev *dev,
						  const struct pci_device_id
						  *ent)
{
	return comedi_pci_auto_config(dev, driver_adl_pci8164.driver_name);
}

static void __devexit driver_adl_pci8164_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_adl_pci8164_pci_driver = {
	.id_table = adl_pci8164_pci_table,
	.probe = &driver_adl_pci8164_pci_probe,
	.remove = __devexit_p(&driver_adl_pci8164_pci_remove)
};

static int __init driver_adl_pci8164_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_adl_pci8164);
	if (retval < 0)
		return retval;

	driver_adl_pci8164_pci_driver.name =
	    (char *)driver_adl_pci8164.driver_name;
	return pci_register_driver(&driver_adl_pci8164_pci_driver);
}

static void __exit driver_adl_pci8164_cleanup_module(void)
{
	pci_unregister_driver(&driver_adl_pci8164_pci_driver);
	comedi_driver_unregister(&driver_adl_pci8164);
}

module_init(driver_adl_pci8164_init_module);
module_exit(driver_adl_pci8164_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
