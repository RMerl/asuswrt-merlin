/*
    kcomedilib/kcomedilib.c
    a comedlib interface for kernel modules

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-2000 David A. Schleef <ds@schleef.org>

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

#define __NO_VERSION__
#include <linux/module.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/io.h>

#include "../comedi.h"
#include "../comedilib.h"
#include "../comedidev.h"

MODULE_AUTHOR("David Schleef <ds@schleef.org>");
MODULE_DESCRIPTION("Comedi kernel library");
MODULE_LICENSE("GPL");

struct comedi_device *comedi_open(const char *filename)
{
	struct comedi_device_file_info *dev_file_info;
	struct comedi_device *dev;
	unsigned int minor;

	if (strncmp(filename, "/dev/comedi", 11) != 0)
		return NULL;

	minor = simple_strtoul(filename + 11, NULL, 0);

	if (minor >= COMEDI_NUM_BOARD_MINORS)
		return NULL;

	dev_file_info = comedi_get_device_file_info(minor);
	if (dev_file_info == NULL)
		return NULL;
	dev = dev_file_info->device;

	if (dev == NULL || !dev->attached)
		return NULL;

	if (!try_module_get(dev->driver->module))
		return NULL;

	return dev;
}
EXPORT_SYMBOL(comedi_open);

int comedi_close(struct comedi_device *d)
{
	struct comedi_device *dev = (struct comedi_device *)d;

	module_put(dev->driver->module);

	return 0;
}
EXPORT_SYMBOL(comedi_close);

static int comedi_do_insn(struct comedi_device *dev, struct comedi_insn *insn)
{
	struct comedi_subdevice *s;
	int ret = 0;

	/* a subdevice instruction */
	if (insn->subdev >= dev->n_subdevices) {
		ret = -EINVAL;
		goto error;
	}
	s = dev->subdevices + insn->subdev;

	if (s->type == COMEDI_SUBD_UNUSED) {
		printk(KERN_ERR "%d not useable subdevice\n", insn->subdev);
		ret = -EIO;
		goto error;
	}


	ret = comedi_check_chanlist(s, 1, &insn->chanspec);
	if (ret < 0) {
		printk(KERN_ERR "bad chanspec\n");
		ret = -EINVAL;
		goto error;
	}

	if (s->busy) {
		ret = -EBUSY;
		goto error;
	}
	s->busy = dev;

	switch (insn->insn) {
	case INSN_BITS:
		ret = s->insn_bits(dev, s, insn, insn->data);
		break;
	case INSN_CONFIG:
		ret = s->insn_config(dev, s, insn, insn->data);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	s->busy = NULL;
error:

	return ret;
}

int comedi_dio_config(struct comedi_device *dev, unsigned int subdev,
		      unsigned int chan, unsigned int io)
{
	struct comedi_insn insn;

	memset(&insn, 0, sizeof(insn));
	insn.insn = INSN_CONFIG;
	insn.n = 1;
	insn.data = &io;
	insn.subdev = subdev;
	insn.chanspec = CR_PACK(chan, 0, 0);

	return comedi_do_insn(dev, &insn);
}
EXPORT_SYMBOL(comedi_dio_config);

int comedi_dio_bitfield(struct comedi_device *dev, unsigned int subdev,
			unsigned int mask, unsigned int *bits)
{
	struct comedi_insn insn;
	unsigned int data[2];
	int ret;

	memset(&insn, 0, sizeof(insn));
	insn.insn = INSN_BITS;
	insn.n = 2;
	insn.data = data;
	insn.subdev = subdev;

	data[0] = mask;
	data[1] = *bits;

	ret = comedi_do_insn(dev, &insn);

	*bits = data[1];

	return ret;
}
EXPORT_SYMBOL(comedi_dio_bitfield);

int comedi_find_subdevice_by_type(struct comedi_device *dev, int type,
				  unsigned int subd)
{
	if (subd > dev->n_subdevices)
		return -ENODEV;

	for (; subd < dev->n_subdevices; subd++) {
		if (dev->subdevices[subd].type == type)
			return subd;
	}
	return -1;
}
EXPORT_SYMBOL(comedi_find_subdevice_by_type);

int comedi_get_n_channels(struct comedi_device *dev, unsigned int subdevice)
{
	struct comedi_subdevice *s = dev->subdevices + subdevice;

	return s->n_chan;
}
EXPORT_SYMBOL(comedi_get_n_channels);
