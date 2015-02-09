/*
   comedi/drivers/dt282x.c
   Hardware driver for Data Translation DT2821 series

   COMEDI - Linux Control and Measurement Device Interface
   Copyright (C) 1997-8 David A. Schleef <ds@schleef.org>

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
Driver: dt282x
Description: Data Translation DT2821 series (including DT-EZ)
Author: ds
Devices: [Data Translation] DT2821 (dt2821),
  DT2821-F-16SE (dt2821-f), DT2821-F-8DI (dt2821-f),
  DT2821-G-16SE (dt2821-f), DT2821-G-8DI (dt2821-g),
  DT2823 (dt2823),
  DT2824-PGH (dt2824-pgh), DT2824-PGL (dt2824-pgl), DT2825 (dt2825),
  DT2827 (dt2827), DT2828 (dt2828), DT21-EZ (dt21-ez), DT23-EZ (dt23-ez),
  DT24-EZ (dt24-ez), DT24-EZ-PGL (dt24-ez-pgl)
Status: complete
Updated: Wed, 22 Aug 2001 17:11:34 -0700

Configuration options:
  [0] - I/O port base address
  [1] - IRQ
  [2] - DMA 1
  [3] - DMA 2
  [4] - AI jumpered for 0=single ended, 1=differential
  [5] - AI jumpered for 0=straight binary, 1=2's complement
  [6] - AO 0 jumpered for 0=straight binary, 1=2's complement
  [7] - AO 1 jumpered for 0=straight binary, 1=2's complement
  [8] - AI jumpered for 0=[-10,10]V, 1=[0,10], 2=[-5,5], 3=[0,5]
  [9] - AO 0 jumpered for 0=[-10,10]V, 1=[0,10], 2=[-5,5], 3=[0,5],
	4=[-2.5,2.5]
  [10]- A0 1 jumpered for 0=[-10,10]V, 1=[0,10], 2=[-5,5], 3=[0,5],
	4=[-2.5,2.5]

Notes:
  - AO commands might be broken.
  - If you try to run a command on both the AI and AO subdevices
    simultaneously, bad things will happen.  The driver needs to
    be fixed to check for this situation and return an error.
*/

#include "../comedidev.h"

#include <linux/gfp.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <asm/dma.h>
#include "comedi_fc.h"

#define DEBUG

#define DT2821_TIMEOUT		100	/* 500 us */
#define DT2821_SIZE 0x10

/*
 *    Registers in the DT282x
 */

#define DT2821_ADCSR	0x00	/* A/D Control/Status             */
#define DT2821_CHANCSR	0x02	/* Channel Control/Status */
#define DT2821_ADDAT	0x04	/* A/D data                       */
#define DT2821_DACSR	0x06	/* D/A Control/Status             */
#define DT2821_DADAT	0x08	/* D/A data                       */
#define DT2821_DIODAT	0x0a	/* digital data                   */
#define DT2821_SUPCSR	0x0c	/* Supervisor Control/Status      */
#define DT2821_TMRCTR	0x0e	/* Timer/Counter          */

/*
 *  At power up, some registers are in a well-known state.  The
 *  masks and values are as follows:
 */

#define DT2821_ADCSR_MASK 0xfff0
#define DT2821_ADCSR_VAL 0x7c00

#define DT2821_CHANCSR_MASK 0xf0f0
#define DT2821_CHANCSR_VAL 0x70f0

#define DT2821_DACSR_MASK 0x7c93
#define DT2821_DACSR_VAL 0x7c90

#define DT2821_SUPCSR_MASK 0xf8ff
#define DT2821_SUPCSR_VAL 0x0000

#define DT2821_TMRCTR_MASK 0xff00
#define DT2821_TMRCTR_VAL 0xf000

/*
 *    Bit fields of each register
 */

/* ADCSR */

#define DT2821_ADERR	0x8000	/* (R)   1 for A/D error  */
#define DT2821_ADCLK	0x0200	/* (R/W) A/D clock enable */
		/*      0x7c00           read as 1's            */
#define DT2821_MUXBUSY	0x0100	/* (R)   multiplexer busy */
#define DT2821_ADDONE	0x0080	/* (R)   A/D done         */
#define DT2821_IADDONE	0x0040	/* (R/W) interrupt on A/D done    */
		/*      0x0030           gain select            */
		/*      0x000f           channel select         */

/* CHANCSR */

#define DT2821_LLE	0x8000	/* (R/W) Load List Enable */
		/*      0x7000           read as 1's            */
		/*      0x0f00     (R)   present address        */
		/*      0x00f0           read as 1's            */
		/*      0x000f     (R)   number of entries - 1  */

/* DACSR */

#define DT2821_DAERR	0x8000	/* (R)   D/A error                */
#define DT2821_YSEL	0x0200	/* (R/W) DAC 1 select             */
#define DT2821_SSEL	0x0100	/* (R/W) single channel select    */
#define DT2821_DACRDY	0x0080	/* (R)   DAC ready                */
#define DT2821_IDARDY	0x0040	/* (R/W) interrupt on DAC ready   */
#define DT2821_DACLK	0x0020	/* (R/W) D/A clock enable */
#define DT2821_HBOE	0x0002	/* (R/W) DIO high byte output enable      */
#define DT2821_LBOE	0x0001	/* (R/W) DIO low byte output enable       */

/* SUPCSR */

#define DT2821_DMAD	0x8000	/* (R)   DMA done                 */
#define DT2821_ERRINTEN	0x4000	/* (R/W) interrupt on error               */
#define DT2821_CLRDMADNE 0x2000	/* (W)   clear DMA done                   */
#define DT2821_DDMA	0x1000	/* (R/W) dual DMA                 */
#define DT2821_DS1	0x0800	/* (R/W) DMA select 1                     */
#define DT2821_DS0	0x0400	/* (R/W) DMA select 0                     */
#define DT2821_BUFFB	0x0200	/* (R/W) buffer B selected                */
#define DT2821_SCDN	0x0100	/* (R)   scan done                        */
#define DT2821_DACON	0x0080	/* (W)   DAC single conversion            */
#define DT2821_ADCINIT	0x0040	/* (W)   A/D initialize                   */
#define DT2821_DACINIT	0x0020	/* (W)   D/A initialize                   */
#define DT2821_PRLD	0x0010	/* (W)   preload multiplexer              */
#define DT2821_STRIG	0x0008	/* (W)   software trigger         */
#define DT2821_XTRIG	0x0004	/* (R/W) external trigger enable  */
#define DT2821_XCLK	0x0002	/* (R/W) external clock enable            */
#define DT2821_BDINIT	0x0001	/* (W)   initialize board         */

static const struct comedi_lrange range_dt282x_ai_lo_bipolar = {
	4, {
		RANGE(-10, 10),
		RANGE(-5, 5),
		RANGE(-2.5, 2.5),
		RANGE(-1.25, 1.25)
	}
};

static const struct comedi_lrange range_dt282x_ai_lo_unipolar = {
	4, {
		RANGE(0, 10),
		RANGE(0, 5),
		RANGE(0, 2.5),
		RANGE(0, 1.25)
	}
};

static const struct comedi_lrange range_dt282x_ai_5_bipolar = {
	4, {
		RANGE(-5, 5),
		RANGE(-2.5, 2.5),
		RANGE(-1.25, 1.25),
		RANGE(-0.625, 0.625)
	}
};

static const struct comedi_lrange range_dt282x_ai_5_unipolar = {
	4, {
		RANGE(0, 5),
		RANGE(0, 2.5),
		RANGE(0, 1.25),
		RANGE(0, 0.625),
	}
};

static const struct comedi_lrange range_dt282x_ai_hi_bipolar = {
	4, {
		RANGE(-10, 10),
		RANGE(-1, 1),
		RANGE(-0.1, 0.1),
		RANGE(-0.02, 0.02)
	}
};

static const struct comedi_lrange range_dt282x_ai_hi_unipolar = {
	4, {
		RANGE(0, 10),
		RANGE(0, 1),
		RANGE(0, 0.1),
		RANGE(0, 0.02)
	}
};

struct dt282x_board {
	const char *name;
	int adbits;
	int adchan_se;
	int adchan_di;
	int ai_speed;
	int ispgl;
	int dachan;
	int dabits;
};

static const struct dt282x_board boardtypes[] = {
	{.name = "dt2821",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 20000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2821-f",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 6500,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2821-g",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 4000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2823",
	 .adbits = 16,
	 .adchan_se = 0,
	 .adchan_di = 4,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 16,
	 },
	{.name = "dt2824-pgh",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 20000,
	 .ispgl = 0,
	 .dachan = 0,
	 .dabits = 0,
	 },
	{.name = "dt2824-pgl",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 20000,
	 .ispgl = 1,
	 .dachan = 0,
	 .dabits = 0,
	 },
	{.name = "dt2825",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 20000,
	 .ispgl = 1,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2827",
	 .adbits = 16,
	 .adchan_se = 0,
	 .adchan_di = 4,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2828",
	 .adbits = 12,
	 .adchan_se = 4,
	 .adchan_di = 0,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt2829",
	 .adbits = 16,
	 .adchan_se = 8,
	 .adchan_di = 0,
	 .ai_speed = 33250,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 16,
	 },
	{.name = "dt21-ez",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 2,
	 .dabits = 12,
	 },
	{.name = "dt23-ez",
	 .adbits = 16,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 0,
	 .dabits = 0,
	 },
	{.name = "dt24-ez",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 10000,
	 .ispgl = 0,
	 .dachan = 0,
	 .dabits = 0,
	 },
	{.name = "dt24-ez-pgl",
	 .adbits = 12,
	 .adchan_se = 16,
	 .adchan_di = 8,
	 .ai_speed = 10000,
	 .ispgl = 1,
	 .dachan = 0,
	 .dabits = 0,
	 },
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct dt282x_board))
#define this_board ((const struct dt282x_board *)dev->board_ptr)

struct dt282x_private {
	int ad_2scomp;		/* we have 2's comp jumper set  */
	int da0_2scomp;		/* same, for DAC0               */
	int da1_2scomp;		/* same, for DAC1               */

	const struct comedi_lrange *darangelist[2];

	short ao[2];

	volatile int dacsr;	/* software copies of registers */
	volatile int adcsr;
	volatile int supcsr;

	volatile int ntrig;
	volatile int nread;

	struct {
		int chan;
		short *buf;	/* DMA buffer */
		volatile int size;	/* size of current transfer */
	} dma[2];
	int dma_maxsize;	/* max size of DMA transfer (in bytes) */
	int usedma;		/* driver uses DMA              */
	volatile int current_dma_index;
	int dma_dir;
};

#define devpriv ((struct dt282x_private *)dev->private)
#define boardtype (*(const struct dt282x_board *)dev->board_ptr)

/*
 *    Some useless abstractions
 */
#define chan_to_DAC(a)	((a)&1)
#define update_dacsr(a)	outw(devpriv->dacsr|(a), dev->iobase+DT2821_DACSR)
#define update_adcsr(a)	outw(devpriv->adcsr|(a), dev->iobase+DT2821_ADCSR)
#define mux_busy() (inw(dev->iobase+DT2821_ADCSR)&DT2821_MUXBUSY)
#define ad_done() (inw(dev->iobase+DT2821_ADCSR)&DT2821_ADDONE)
#define update_supcsr(a) outw(devpriv->supcsr|(a), dev->iobase+DT2821_SUPCSR)

/*
 *    danger! macro abuse... a is the expression to wait on, and b is
 *      the statement(s) to execute if it doesn't happen.
 */
#define wait_for(a, b)						\
	do {							\
		int _i;						\
		for (_i = 0; _i < DT2821_TIMEOUT; _i++) {	\
			if (a) {				\
				_i = 0;				\
				break;				\
			}					\
			udelay(5);				\
		}						\
		if (_i)						\
			b					\
	} while (0)

static int dt282x_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int dt282x_detach(struct comedi_device *dev);
static struct comedi_driver driver_dt282x = {
	.driver_name = "dt282x",
	.module = THIS_MODULE,
	.attach = dt282x_attach,
	.detach = dt282x_detach,
	.board_name = &boardtypes[0].name,
	.num_names = n_boardtypes,
	.offset = sizeof(struct dt282x_board),
};

static int __init driver_dt282x_init_module(void)
{
	return comedi_driver_register(&driver_dt282x);
}

static void __exit driver_dt282x_cleanup_module(void)
{
	comedi_driver_unregister(&driver_dt282x);
}

module_init(driver_dt282x_init_module);
module_exit(driver_dt282x_cleanup_module);

static void free_resources(struct comedi_device *dev);
static int prep_ai_dma(struct comedi_device *dev, int chan, int size);
static int prep_ao_dma(struct comedi_device *dev, int chan, int size);
static int dt282x_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s);
static int dt282x_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s);
static int dt282x_ns_to_timer(int *nanosec, int round_mode);
static void dt282x_disable_dma(struct comedi_device *dev);

static int dt282x_grab_dma(struct comedi_device *dev, int dma1, int dma2);

static void dt282x_munge(struct comedi_device *dev, short *buf,
			 unsigned int nbytes)
{
	unsigned int i;
	unsigned short mask = (1 << boardtype.adbits) - 1;
	unsigned short sign = 1 << (boardtype.adbits - 1);
	int n;

	if (devpriv->ad_2scomp)
		sign = 1 << (boardtype.adbits - 1);
	else
		sign = 0;

	if (nbytes % 2)
		comedi_error(dev, "bug! odd number of bytes from dma xfer");
	n = nbytes / 2;
	for (i = 0; i < n; i++)
		buf[i] = (buf[i] & mask) ^ sign;
}

static void dt282x_ao_dma_interrupt(struct comedi_device *dev)
{
	void *ptr;
	int size;
	int i;
	struct comedi_subdevice *s = dev->subdevices + 1;

	update_supcsr(DT2821_CLRDMADNE);

	if (!s->async->prealloc_buf) {
		printk(KERN_ERR "async->data disappeared.  dang!\n");
		return;
	}

	i = devpriv->current_dma_index;
	ptr = devpriv->dma[i].buf;

	disable_dma(devpriv->dma[i].chan);

	devpriv->current_dma_index = 1 - i;

	size = cfc_read_array_from_buffer(s, ptr, devpriv->dma_maxsize);
	if (size == 0) {
		printk(KERN_ERR "dt282x: AO underrun\n");
		dt282x_ao_cancel(dev, s);
		s->async->events |= COMEDI_CB_OVERFLOW;
		return;
	}
	prep_ao_dma(dev, i, size);
	return;
}

static void dt282x_ai_dma_interrupt(struct comedi_device *dev)
{
	void *ptr;
	int size;
	int i;
	int ret;
	struct comedi_subdevice *s = dev->subdevices;

	update_supcsr(DT2821_CLRDMADNE);

	if (!s->async->prealloc_buf) {
		printk(KERN_ERR "async->data disappeared.  dang!\n");
		return;
	}

	i = devpriv->current_dma_index;
	ptr = devpriv->dma[i].buf;
	size = devpriv->dma[i].size;

	disable_dma(devpriv->dma[i].chan);

	devpriv->current_dma_index = 1 - i;

	dt282x_munge(dev, ptr, size);
	ret = cfc_write_array_to_buffer(s, ptr, size);
	if (ret != size) {
		dt282x_ai_cancel(dev, s);
		return;
	}
	devpriv->nread -= size / 2;

	if (devpriv->nread < 0) {
		printk(KERN_INFO "dt282x: off by one\n");
		devpriv->nread = 0;
	}
	if (!devpriv->nread) {
		dt282x_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA;
		return;
	}
	/* restart the channel */
	prep_ai_dma(dev, i, 0);
}

static int prep_ai_dma(struct comedi_device *dev, int dma_index, int n)
{
	int dma_chan;
	unsigned long dma_ptr;
	unsigned long flags;

	if (!devpriv->ntrig)
		return 0;

	if (n == 0)
		n = devpriv->dma_maxsize;
	if (n > devpriv->ntrig * 2)
		n = devpriv->ntrig * 2;
	devpriv->ntrig -= n / 2;

	devpriv->dma[dma_index].size = n;
	dma_chan = devpriv->dma[dma_index].chan;
	dma_ptr = virt_to_bus(devpriv->dma[dma_index].buf);

	set_dma_mode(dma_chan, DMA_MODE_READ);
	flags = claim_dma_lock();
	clear_dma_ff(dma_chan);
	set_dma_addr(dma_chan, dma_ptr);
	set_dma_count(dma_chan, n);
	release_dma_lock(flags);

	enable_dma(dma_chan);

	return n;
}

static int prep_ao_dma(struct comedi_device *dev, int dma_index, int n)
{
	int dma_chan;
	unsigned long dma_ptr;
	unsigned long flags;

	devpriv->dma[dma_index].size = n;
	dma_chan = devpriv->dma[dma_index].chan;
	dma_ptr = virt_to_bus(devpriv->dma[dma_index].buf);

	set_dma_mode(dma_chan, DMA_MODE_WRITE);
	flags = claim_dma_lock();
	clear_dma_ff(dma_chan);
	set_dma_addr(dma_chan, dma_ptr);
	set_dma_count(dma_chan, n);
	release_dma_lock(flags);

	enable_dma(dma_chan);

	return n;
}

static irqreturn_t dt282x_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s;
	struct comedi_subdevice *s_ao;
	unsigned int supcsr, adcsr, dacsr;
	int handled = 0;

	if (!dev->attached) {
		comedi_error(dev, "spurious interrupt");
		return IRQ_HANDLED;
	}

	s = dev->subdevices + 0;
	s_ao = dev->subdevices + 1;
	adcsr = inw(dev->iobase + DT2821_ADCSR);
	dacsr = inw(dev->iobase + DT2821_DACSR);
	supcsr = inw(dev->iobase + DT2821_SUPCSR);
	if (supcsr & DT2821_DMAD) {
		if (devpriv->dma_dir == DMA_MODE_READ)
			dt282x_ai_dma_interrupt(dev);
		else
			dt282x_ao_dma_interrupt(dev);
		handled = 1;
	}
	if (adcsr & DT2821_ADERR) {
		if (devpriv->nread != 0) {
			comedi_error(dev, "A/D error");
			dt282x_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_ERROR;
		}
		handled = 1;
	}
	if (dacsr & DT2821_DAERR) {
		comedi_error(dev, "D/A error");
		dt282x_ao_cancel(dev, s_ao);
		s->async->events |= COMEDI_CB_ERROR;
		handled = 1;
	}
	comedi_event(dev, s);
	/* printk("adcsr=0x%02x dacsr-0x%02x supcsr=0x%02x\n",
		adcsr, dacsr, supcsr); */
	return IRQ_RETVAL(handled);
}

static void dt282x_load_changain(struct comedi_device *dev, int n,
				 unsigned int *chanlist)
{
	unsigned int i;
	unsigned int chan, range;

	outw(DT2821_LLE | (n - 1), dev->iobase + DT2821_CHANCSR);
	for (i = 0; i < n; i++) {
		chan = CR_CHAN(chanlist[i]);
		range = CR_RANGE(chanlist[i]);
		update_adcsr((range << 4) | (chan));
	}
	outw(n - 1, dev->iobase + DT2821_CHANCSR);
}

/*
 *    Performs a single A/D conversion.
 *      - Put channel/gain into channel-gain list
 *      - preload multiplexer
 *      - trigger conversion and wait for it to finish
 */
static int dt282x_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int i;

	devpriv->adcsr = DT2821_ADCLK;
	update_adcsr(0);

	dt282x_load_changain(dev, 1, &insn->chanspec);

	update_supcsr(DT2821_PRLD);
	wait_for(!mux_busy(), comedi_error(dev, "timeout\n"); return -ETIME;);

	for (i = 0; i < insn->n; i++) {
		update_supcsr(DT2821_STRIG);
		wait_for(ad_done(), comedi_error(dev, "timeout\n");
			 return -ETIME;);

		data[i] =
		    inw(dev->iobase +
			DT2821_ADDAT) & ((1 << boardtype.adbits) - 1);
		if (devpriv->ad_2scomp)
			data[i] ^= (1 << (boardtype.adbits - 1));
	}

	return i;
}

static int dt282x_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_FOLLOW | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/*
	 * step 2: make sure trigger sources are unique
	 * and mutually compatible
	 */

	/* note that mutual compatibility is not an issue here */
	if (cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		/* internal trigger */
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	} else {
		/* external trigger */
		/* should be level/edge, hi/lo specification here */
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}
	if (cmd->convert_arg < 4000) {
		cmd->convert_arg = 4000;
		err++;
	}
#define SLOWEST_TIMER	(250*(1<<15)*255)
	if (cmd->convert_arg > SLOWEST_TIMER) {
		cmd->convert_arg = SLOWEST_TIMER;
		err++;
	}
	if (cmd->convert_arg < this_board->ai_speed) {
		cmd->convert_arg = this_board->ai_speed;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		/* any count is allowed */
	} else {
		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	tmp = cmd->convert_arg;
	dt282x_ns_to_timer(&cmd->convert_arg, cmd->flags & TRIG_ROUND_MASK);
	if (tmp != cmd->convert_arg)
		err++;

	if (err)
		return 4;

	return 0;
}

static int dt282x_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int timer;

	if (devpriv->usedma == 0) {
		comedi_error(dev,
			     "driver requires 2 dma channels"
						" to execute command");
		return -EIO;
	}

	dt282x_disable_dma(dev);

	if (cmd->convert_arg < this_board->ai_speed)
		cmd->convert_arg = this_board->ai_speed;
	timer = dt282x_ns_to_timer(&cmd->convert_arg, TRIG_ROUND_NEAREST);
	outw(timer, dev->iobase + DT2821_TMRCTR);

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		/* internal trigger */
		devpriv->supcsr = DT2821_ERRINTEN | DT2821_DS0;
	} else {
		/* external trigger */
		devpriv->supcsr = DT2821_ERRINTEN | DT2821_DS0 | DT2821_DS1;
	}
	update_supcsr(DT2821_CLRDMADNE | DT2821_BUFFB | DT2821_ADCINIT);

	devpriv->ntrig = cmd->stop_arg * cmd->scan_end_arg;
	devpriv->nread = devpriv->ntrig;

	devpriv->dma_dir = DMA_MODE_READ;
	devpriv->current_dma_index = 0;
	prep_ai_dma(dev, 0, 0);
	if (devpriv->ntrig) {
		prep_ai_dma(dev, 1, 0);
		devpriv->supcsr |= DT2821_DDMA;
		update_supcsr(0);
	}

	devpriv->adcsr = 0;

	dt282x_load_changain(dev, cmd->chanlist_len, cmd->chanlist);

	devpriv->adcsr = DT2821_ADCLK | DT2821_IADDONE;
	update_adcsr(0);

	update_supcsr(DT2821_PRLD);
	wait_for(!mux_busy(), comedi_error(dev, "timeout\n"); return -ETIME;);

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		update_supcsr(DT2821_STRIG);
	} else {
		devpriv->supcsr |= DT2821_XTRIG;
		update_supcsr(0);
	}

	return 0;
}

static void dt282x_disable_dma(struct comedi_device *dev)
{
	if (devpriv->usedma) {
		disable_dma(devpriv->dma[0].chan);
		disable_dma(devpriv->dma[1].chan);
	}
}

static int dt282x_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	dt282x_disable_dma(dev);

	devpriv->adcsr = 0;
	update_adcsr(0);

	devpriv->supcsr = 0;
	update_supcsr(DT2821_ADCINIT);

	return 0;
}

static int dt282x_ns_to_timer(int *nanosec, int round_mode)
{
	int prescale, base, divider;

	for (prescale = 0; prescale < 16; prescale++) {
		if (prescale == 1)
			continue;
		base = 250 * (1 << prescale);
		switch (round_mode) {
		case TRIG_ROUND_NEAREST:
		default:
			divider = (*nanosec + base / 2) / base;
			break;
		case TRIG_ROUND_DOWN:
			divider = (*nanosec) / base;
			break;
		case TRIG_ROUND_UP:
			divider = (*nanosec + base - 1) / base;
			break;
		}
		if (divider < 256) {
			*nanosec = divider * base;
			return (prescale << 8) | (255 - divider);
		}
	}
	base = 250 * (1 << 15);
	divider = 255;
	*nanosec = divider * base;
	return (15 << 8) | (255 - divider);
}

/*
 *    Analog output routine.  Selects single channel conversion,
 *      selects correct channel, converts from 2's compliment to
 *      offset binary if necessary, loads the data into the DAC
 *      data register, and performs the conversion.
 */
static int dt282x_ao_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	data[0] = devpriv->ao[CR_CHAN(insn->chanspec)];

	return 1;
}

static int dt282x_ao_insn_write(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	short d;
	unsigned int chan;

	chan = CR_CHAN(insn->chanspec);
	d = data[0];
	d &= (1 << boardtype.dabits) - 1;
	devpriv->ao[chan] = d;

	devpriv->dacsr |= DT2821_SSEL;

	if (chan) {
		/* select channel */
		devpriv->dacsr |= DT2821_YSEL;
		if (devpriv->da0_2scomp)
			d ^= (1 << (boardtype.dabits - 1));
	} else {
		devpriv->dacsr &= ~DT2821_YSEL;
		if (devpriv->da1_2scomp)
			d ^= (1 << (boardtype.dabits - 1));
	}

	update_dacsr(0);

	outw(d, dev->iobase + DT2821_DADAT);

	update_supcsr(DT2821_DACON);

	return 1;
}

static int dt282x_ao_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	/*
	 * step 2: make sure trigger sources are unique
	 * and mutually compatible
	 */

	/* note that mutual compatibility is not an issue here */
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
	if (cmd->scan_begin_arg < 5000) {
		cmd->scan_begin_arg = 5000;
		err++;
	}
	if (cmd->convert_arg != 0) {
		cmd->convert_arg = 0;
		err++;
	}
	if (cmd->scan_end_arg > 2) {
		cmd->scan_end_arg = 2;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		/* any count is allowed */
	} else {
		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	tmp = cmd->scan_begin_arg;
	dt282x_ns_to_timer(&cmd->scan_begin_arg, cmd->flags & TRIG_ROUND_MASK);
	if (tmp != cmd->scan_begin_arg)
		err++;

	if (err)
		return 4;

	return 0;

}

static int dt282x_ao_inttrig(struct comedi_device *dev,
			     struct comedi_subdevice *s, unsigned int x)
{
	int size;

	if (x != 0)
		return -EINVAL;

	size = cfc_read_array_from_buffer(s, devpriv->dma[0].buf,
					  devpriv->dma_maxsize);
	if (size == 0) {
		printk(KERN_ERR "dt282x: AO underrun\n");
		return -EPIPE;
	}
	prep_ao_dma(dev, 0, size);

	size = cfc_read_array_from_buffer(s, devpriv->dma[1].buf,
					  devpriv->dma_maxsize);
	if (size == 0) {
		printk(KERN_ERR "dt282x: AO underrun\n");
		return -EPIPE;
	}
	prep_ao_dma(dev, 1, size);

	update_supcsr(DT2821_STRIG);
	s->async->inttrig = NULL;

	return 1;
}

static int dt282x_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	int timer;
	struct comedi_cmd *cmd = &s->async->cmd;

	if (devpriv->usedma == 0) {
		comedi_error(dev,
			     "driver requires 2 dma channels"
						" to execute command");
		return -EIO;
	}

	dt282x_disable_dma(dev);

	devpriv->supcsr = DT2821_ERRINTEN | DT2821_DS1 | DT2821_DDMA;
	update_supcsr(DT2821_CLRDMADNE | DT2821_BUFFB | DT2821_DACINIT);

	devpriv->ntrig = cmd->stop_arg * cmd->chanlist_len;
	devpriv->nread = devpriv->ntrig;

	devpriv->dma_dir = DMA_MODE_WRITE;
	devpriv->current_dma_index = 0;

	timer = dt282x_ns_to_timer(&cmd->scan_begin_arg, TRIG_ROUND_NEAREST);
	outw(timer, dev->iobase + DT2821_TMRCTR);

	devpriv->dacsr = DT2821_SSEL | DT2821_DACLK | DT2821_IDARDY;
	update_dacsr(0);

	s->async->inttrig = dt282x_ao_inttrig;

	return 0;
}

static int dt282x_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	dt282x_disable_dma(dev);

	devpriv->dacsr = 0;
	update_dacsr(0);

	devpriv->supcsr = 0;
	update_supcsr(DT2821_DACINIT);

	return 0;
}

static int dt282x_dio_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);

		outw(s->state, dev->iobase + DT2821_DIODAT);
	}
	data[1] = inw(dev->iobase + DT2821_DIODAT);

	return 2;
}

static int dt282x_dio_insn_config(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int mask;

	mask = (CR_CHAN(insn->chanspec) < 8) ? 0x00ff : 0xff00;
	if (data[0])
		s->io_bits |= mask;
	else
		s->io_bits &= ~mask;

	if (s->io_bits & 0x00ff)
		devpriv->dacsr |= DT2821_LBOE;
	else
		devpriv->dacsr &= ~DT2821_LBOE;
	if (s->io_bits & 0xff00)
		devpriv->dacsr |= DT2821_HBOE;
	else
		devpriv->dacsr &= ~DT2821_HBOE;

	outw(devpriv->dacsr, dev->iobase + DT2821_DACSR);

	return 1;
}

static const struct comedi_lrange *const ai_range_table[] = {
	&range_dt282x_ai_lo_bipolar,
	&range_dt282x_ai_lo_unipolar,
	&range_dt282x_ai_5_bipolar,
	&range_dt282x_ai_5_unipolar
};

static const struct comedi_lrange *const ai_range_pgl_table[] = {
	&range_dt282x_ai_hi_bipolar,
	&range_dt282x_ai_hi_unipolar
};

static const struct comedi_lrange *opt_ai_range_lkup(int ispgl, int x)
{
	if (ispgl) {
		if (x < 0 || x >= 2)
			x = 0;
		return ai_range_pgl_table[x];
	} else {
		if (x < 0 || x >= 4)
			x = 0;
		return ai_range_table[x];
	}
}

static const struct comedi_lrange *const ao_range_table[] = {
	&range_bipolar10,
	&range_unipolar10,
	&range_bipolar5,
	&range_unipolar5,
	&range_bipolar2_5
};

static const struct comedi_lrange *opt_ao_range_lkup(int x)
{
	if (x < 0 || x >= 5)
		x = 0;
	return ao_range_table[x];
}

enum {  /* i/o base, irq, dma channels */
	opt_iobase = 0, opt_irq, opt_dma1, opt_dma2,
	opt_diff,		/* differential */
	opt_ai_twos, opt_ao0_twos, opt_ao1_twos,	/* twos comp */
	opt_ai_range, opt_ao0_range, opt_ao1_range,	/* range */
};

/*
   options:
   0	i/o base
   1	irq
   2	dma1
   3	dma2
   4	0=single ended, 1=differential
   5	ai 0=straight binary, 1=2's comp
   6	ao0 0=straight binary, 1=2's comp
   7	ao1 0=straight binary, 1=2's comp
   8	ai 0=±10 V, 1=0-10 V, 2=±5 V, 3=0-5 V
   9	ao0 0=±10 V, 1=0-10 V, 2=±5 V, 3=0-5 V, 4=±2.5 V
   10	ao1 0=±10 V, 1=0-10 V, 2=±5 V, 3=0-5 V, 4=±2.5 V
 */
static int dt282x_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int i, irq;
	int ret;
	struct comedi_subdevice *s;
	unsigned long iobase;

	dev->board_name = this_board->name;

	iobase = it->options[opt_iobase];
	if (!iobase)
		iobase = 0x240;

	printk(KERN_INFO "comedi%d: dt282x: 0x%04lx", dev->minor, iobase);
	if (!request_region(iobase, DT2821_SIZE, "dt282x")) {
		printk(KERN_INFO " I/O port conflict\n");
		return -EBUSY;
	}
	dev->iobase = iobase;

	outw(DT2821_BDINIT, dev->iobase + DT2821_SUPCSR);
	i = inw(dev->iobase + DT2821_ADCSR);
#ifdef DEBUG
	printk(KERN_DEBUG " fingerprint=%x,%x,%x,%x,%x",
	       inw(dev->iobase + DT2821_ADCSR),
	       inw(dev->iobase + DT2821_CHANCSR),
	       inw(dev->iobase + DT2821_DACSR),
	       inw(dev->iobase + DT2821_SUPCSR),
	       inw(dev->iobase + DT2821_TMRCTR));
#endif

	if (((inw(dev->iobase + DT2821_ADCSR) & DT2821_ADCSR_MASK)
	     != DT2821_ADCSR_VAL) ||
	    ((inw(dev->iobase + DT2821_CHANCSR) & DT2821_CHANCSR_MASK)
	     != DT2821_CHANCSR_VAL) ||
	    ((inw(dev->iobase + DT2821_DACSR) & DT2821_DACSR_MASK)
	     != DT2821_DACSR_VAL) ||
	    ((inw(dev->iobase + DT2821_SUPCSR) & DT2821_SUPCSR_MASK)
	     != DT2821_SUPCSR_VAL) ||
	    ((inw(dev->iobase + DT2821_TMRCTR) & DT2821_TMRCTR_MASK)
	     != DT2821_TMRCTR_VAL)) {
		printk(KERN_ERR " board not found");
		return -EIO;
	}
	/* should do board test */

	irq = it->options[opt_irq];
	if (irq > 0) {
		printk(KERN_INFO " ( irq = %d )", irq);
		ret = request_irq(irq, dt282x_interrupt, 0, "dt282x", dev);
		if (ret < 0) {
			printk(KERN_ERR " failed to get irq\n");
			return -EIO;
		}
		dev->irq = irq;
	} else if (irq == 0) {
		printk(KERN_INFO " (no irq)");
	} else {
		printk(KERN_INFO " (irq probe not implemented)");
	}

	ret = alloc_private(dev, sizeof(struct dt282x_private));
	if (ret < 0)
		return ret;

	ret = dt282x_grab_dma(dev, it->options[opt_dma1],
			      it->options[opt_dma2]);
	if (ret < 0)
		return ret;

	ret = alloc_subdevices(dev, 3);
	if (ret < 0)
		return ret;

	s = dev->subdevices + 0;

	dev->read_subdev = s;
	/* ai subdevice */
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_CMD_READ |
	    ((it->options[opt_diff]) ? SDF_DIFF : SDF_COMMON);
	s->n_chan =
	    (it->options[opt_diff]) ? boardtype.adchan_di : boardtype.adchan_se;
	s->insn_read = dt282x_ai_insn_read;
	s->do_cmdtest = dt282x_ai_cmdtest;
	s->do_cmd = dt282x_ai_cmd;
	s->cancel = dt282x_ai_cancel;
	s->maxdata = (1 << boardtype.adbits) - 1;
	s->len_chanlist = 16;
	s->range_table =
	    opt_ai_range_lkup(boardtype.ispgl, it->options[opt_ai_range]);
	devpriv->ad_2scomp = it->options[opt_ai_twos];

	s++;

	s->n_chan = boardtype.dachan;
	if (s->n_chan) {
		/* ao subsystem */
		s->type = COMEDI_SUBD_AO;
		dev->write_subdev = s;
		s->subdev_flags = SDF_WRITABLE | SDF_CMD_WRITE;
		s->insn_read = dt282x_ao_insn_read;
		s->insn_write = dt282x_ao_insn_write;
		s->do_cmdtest = dt282x_ao_cmdtest;
		s->do_cmd = dt282x_ao_cmd;
		s->cancel = dt282x_ao_cancel;
		s->maxdata = (1 << boardtype.dabits) - 1;
		s->len_chanlist = 2;
		s->range_table_list = devpriv->darangelist;
		devpriv->darangelist[0] =
		    opt_ao_range_lkup(it->options[opt_ao0_range]);
		devpriv->darangelist[1] =
		    opt_ao_range_lkup(it->options[opt_ao1_range]);
		devpriv->da0_2scomp = it->options[opt_ao0_twos];
		devpriv->da1_2scomp = it->options[opt_ao1_twos];
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s++;
	/* dio subsystem */
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 16;
	s->insn_bits = dt282x_dio_insn_bits;
	s->insn_config = dt282x_dio_insn_config;
	s->maxdata = 1;
	s->range_table = &range_digital;

	printk(KERN_INFO "\n");

	return 0;
}

static void free_resources(struct comedi_device *dev)
{
	if (dev->irq)
		free_irq(dev->irq, dev);
	if (dev->iobase)
		release_region(dev->iobase, DT2821_SIZE);
	if (dev->private) {
		if (devpriv->dma[0].chan)
			free_dma(devpriv->dma[0].chan);
		if (devpriv->dma[1].chan)
			free_dma(devpriv->dma[1].chan);
		if (devpriv->dma[0].buf)
			free_page((unsigned long)devpriv->dma[0].buf);
		if (devpriv->dma[1].buf)
			free_page((unsigned long)devpriv->dma[1].buf);
	}
}

static int dt282x_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: dt282x: remove\n", dev->minor);

	free_resources(dev);

	return 0;
}

static int dt282x_grab_dma(struct comedi_device *dev, int dma1, int dma2)
{
	int ret;

	devpriv->usedma = 0;

	if (!dma1 && !dma2) {
		printk(KERN_ERR " (no dma)");
		return 0;
	}

	if (dma1 == dma2 || dma1 < 5 || dma2 < 5 || dma1 > 7 || dma2 > 7)
		return -EINVAL;

	if (dma2 < dma1) {
		int i;
		i = dma1;
		dma1 = dma2;
		dma2 = i;
	}

	ret = request_dma(dma1, "dt282x A");
	if (ret)
		return -EBUSY;
	devpriv->dma[0].chan = dma1;

	ret = request_dma(dma2, "dt282x B");
	if (ret)
		return -EBUSY;
	devpriv->dma[1].chan = dma2;

	devpriv->dma_maxsize = PAGE_SIZE;
	devpriv->dma[0].buf = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
	devpriv->dma[1].buf = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
	if (!devpriv->dma[0].buf || !devpriv->dma[1].buf) {
		printk(KERN_ERR " can't get DMA memory");
		return -ENOMEM;
	}

	printk(KERN_INFO " (dma=%d,%d)", dma1, dma2);

	devpriv->usedma = 1;

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
