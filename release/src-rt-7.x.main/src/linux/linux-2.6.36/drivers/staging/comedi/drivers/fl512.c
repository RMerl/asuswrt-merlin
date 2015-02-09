/*
    comedi/drivers/fl512.c
    Anders Gnistrup <ex18@kalman.iau.dtu.dk>
*/

/*
Driver: fl512
Description: unknown
Author: Anders Gnistrup <ex18@kalman.iau.dtu.dk>
Devices: [unknown] FL512 (fl512)
Status: unknown

Digital I/O is not supported.

Configuration options:
  [0] - I/O port base address
*/

#define DEBUG 0

#include "../comedidev.h"

#include <linux/delay.h>
#include <linux/ioport.h>

#define FL512_SIZE 16		/* the size of the used memory */
struct fl512_private {

	short ao_readback[2];
};

#define devpriv ((struct fl512_private *) dev->private)

static const struct comedi_lrange range_fl512 = { 4, {
						      BIP_RANGE(0.5),
						      BIP_RANGE(1),
						      BIP_RANGE(5),
						      BIP_RANGE(10),
						      UNI_RANGE(1),
						      UNI_RANGE(5),
						      UNI_RANGE(10),
						      }
};

static int fl512_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int fl512_detach(struct comedi_device *dev);

static struct comedi_driver driver_fl512 = {
	.driver_name = "fl512",
	.module = THIS_MODULE,
	.attach = fl512_attach,
	.detach = fl512_detach,
};

static int __init driver_fl512_init_module(void)
{
	return comedi_driver_register(&driver_fl512);
}

static void __exit driver_fl512_cleanup_module(void)
{
	comedi_driver_unregister(&driver_fl512);
}

module_init(driver_fl512_init_module);
module_exit(driver_fl512_cleanup_module);

static int fl512_ai_insn(struct comedi_device *dev,
			 struct comedi_subdevice *s, struct comedi_insn *insn,
			 unsigned int *data);
static int fl512_ao_insn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int fl512_ao_insn_readback(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);

/*
 * fl512_ai_insn : this is the analog input function
 */
static int fl512_ai_insn(struct comedi_device *dev,
			 struct comedi_subdevice *s, struct comedi_insn *insn,
			 unsigned int *data)
{
	int n;
	unsigned int lo_byte, hi_byte;
	char chan = CR_CHAN(insn->chanspec);
	unsigned long iobase = dev->iobase;

	for (n = 0; n < insn->n; n++) {	/* sample n times on selected channel */
		outb(chan, iobase + 2);	/* select chan */
		outb(0, iobase + 3);	/* start conversion */
		udelay(30);	/* sleep 30 usec */
		lo_byte = inb(iobase + 2);	/* low 8 byte */
		hi_byte = inb(iobase + 3) & 0xf; /* high 4 bit and mask */
		data[n] = lo_byte + (hi_byte << 8);
	}
	return n;
}

/*
 * fl512_ao_insn : used to write to a DA port n times
 */
static int fl512_ao_insn(struct comedi_device *dev,
			 struct comedi_subdevice *s, struct comedi_insn *insn,
			 unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);	/* get chan to write */
	unsigned long iobase = dev->iobase;	/* get base address  */

	for (n = 0; n < insn->n; n++) {	/* write n data set */
		/* write low byte   */
		outb(data[n] & 0x0ff, iobase + 4 + 2 * chan);
		/* write high byte  */
		outb((data[n] & 0xf00) >> 8, iobase + 4 + 2 * chan);
		inb(iobase + 4 + 2 * chan);	/* trig */

		devpriv->ao_readback[chan] = data[n];
	}
	return n;
}

/*
 * fl512_ao_insn_readback : used to read previous values written to
 * DA port
 */
static int fl512_ao_insn_readback(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++)
		data[n] = devpriv->ao_readback[chan];

	return n;
}

/*
 * start attach
 */
static int fl512_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	unsigned long iobase;

	/* pointer to the subdevice: Analog in, Analog out,
	   (not made ->and Digital IO) */
	struct comedi_subdevice *s;

	iobase = it->options[0];
	printk(KERN_INFO "comedi:%d fl512: 0x%04lx", dev->minor, iobase);
	if (!request_region(iobase, FL512_SIZE, "fl512")) {
		printk(KERN_WARNING " I/O port conflict\n");
		return -EIO;
	}
	dev->iobase = iobase;
	dev->board_name = "fl512";
	if (alloc_private(dev, sizeof(struct fl512_private)) < 0)
		return -ENOMEM;

#if DEBUG
	printk(KERN_DEBUG "malloc ok\n");
#endif

	if (alloc_subdevices(dev, 2) < 0)
		return -ENOMEM;

	/*
	 * this if the definitions of the supdevices, 2 have been defined
	 */
	/* Analog indput */
	s = dev->subdevices + 0;
	/* define subdevice as Analog In */
	s->type = COMEDI_SUBD_AI;
	/* you can read it from userspace */
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	/* Number of Analog input channels */
	s->n_chan = 16;
	/* accept only 12 bits of data */
	s->maxdata = 0x0fff;
	/* device use one of the ranges */
	s->range_table = &range_fl512;
	/* function to call when read AD */
	s->insn_read = fl512_ai_insn;
	printk(KERN_INFO "comedi: fl512: subdevice 0 initialized\n");

	/* Analog output */
	s = dev->subdevices + 1;
	/* define subdevice as Analog OUT */
	s->type = COMEDI_SUBD_AO;
	/* you can write it from userspace */
	s->subdev_flags = SDF_WRITABLE;
	/* Number of Analog output channels */
	s->n_chan = 2;
	/* accept only 12 bits of data */
	s->maxdata = 0x0fff;
	/* device use one of the ranges */
	s->range_table = &range_fl512;
	/* function to call when write DA */
	s->insn_write = fl512_ao_insn;
	/* function to call when reading DA */
	s->insn_read = fl512_ao_insn_readback;
	printk(KERN_INFO "comedi: fl512: subdevice 1 initialized\n");

	return 1;
}

static int fl512_detach(struct comedi_device *dev)
{
	if (dev->iobase)
		release_region(dev->iobase, FL512_SIZE);
	printk(KERN_INFO "comedi%d: fl512: dummy i detach\n", dev->minor);
	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
