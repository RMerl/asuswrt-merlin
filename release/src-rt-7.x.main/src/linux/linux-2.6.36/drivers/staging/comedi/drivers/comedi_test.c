/*
    comedi/drivers/comedi_test.c

    Generates fake waveform signals that can be read through
    the command interface.  It does _not_ read from any board;
    it just generates deterministic waveforms.
    Useful for various testing purposes.

    Copyright (C) 2002 Joachim Wuttke <Joachim.Wuttke@icn.siemens.de>
    Copyright (C) 2002 Frank Mori Hess <fmhess@users.sourceforge.net>

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2000 David A. Schleef <ds@schleef.org>

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

************************************************************************/
/*
Driver: comedi_test
Description: generates fake waveforms
Author: Joachim Wuttke <Joachim.Wuttke@icn.siemens.de>, Frank Mori Hess
  <fmhess@users.sourceforge.net>, ds
Devices:
Status: works
Updated: Sat, 16 Mar 2002 17:34:48 -0800

This driver is mainly for testing purposes, but can also be used to
generate sample waveforms on systems that don't have data acquisition
hardware.

Configuration options:
  [0] - Amplitude in microvolts for fake waveforms (default 1 volt)
  [1] - Period in microseconds for fake waveforms (default 0.1 sec)

Generates a sawtooth wave on channel 0, square wave on channel 1, additional
waveforms could be added to other channels (currently they return flatline
zero volts).

*/

#include "../comedidev.h"

#include <asm/div64.h>

#include "comedi_fc.h"
#include <linux/timer.h>

/* Board descriptions */
struct waveform_board {
	const char *name;
	int ai_chans;
	int ai_bits;
	int have_dio;
};

#define N_CHANS 8

static const struct waveform_board waveform_boards[] = {
	{
	 .name = "comedi_test",
	 .ai_chans = N_CHANS,
	 .ai_bits = 16,
	 .have_dio = 0,
	 },
};

#define thisboard ((const struct waveform_board *)dev->board_ptr)

/* Data unique to this driver */
struct waveform_private {
	struct timer_list timer;
	struct timeval last;	/* time at which last timer interrupt occured */
	unsigned int uvolt_amplitude;	/* waveform amplitude in microvolts */
	unsigned long usec_period;	/* waveform period in microseconds */
	unsigned long usec_current;	/* current time (modulo waveform period) */
	unsigned long usec_remainder;	/* usec since last scan; */
	unsigned long ai_count;	/* number of conversions remaining */
	unsigned int scan_period;	/* scan period in usec */
	unsigned int convert_period;	/* conversion period in usec */
	unsigned timer_running:1;
	unsigned int ao_loopbacks[N_CHANS];
};
#define devpriv ((struct waveform_private *)dev->private)

static int waveform_attach(struct comedi_device *dev,
			   struct comedi_devconfig *it);
static int waveform_detach(struct comedi_device *dev);
static struct comedi_driver driver_waveform = {
	.driver_name = "comedi_test",
	.module = THIS_MODULE,
	.attach = waveform_attach,
	.detach = waveform_detach,
	.board_name = &waveform_boards[0].name,
	.offset = sizeof(struct waveform_board),
	.num_names = ARRAY_SIZE(waveform_boards),
};

static int __init driver_waveform_init_module(void)
{
	return comedi_driver_register(&driver_waveform);
}

static void __exit driver_waveform_cleanup_module(void)
{
	comedi_driver_unregister(&driver_waveform);
}

module_init(driver_waveform_init_module);
module_exit(driver_waveform_cleanup_module);

static int waveform_ai_cmdtest(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_cmd *cmd);
static int waveform_ai_cmd(struct comedi_device *dev,
			   struct comedi_subdevice *s);
static int waveform_ai_cancel(struct comedi_device *dev,
			      struct comedi_subdevice *s);
static int waveform_ai_insn_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);
static int waveform_ao_insn_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);
static short fake_sawtooth(struct comedi_device *dev, unsigned int range,
			   unsigned long current_time);
static short fake_squarewave(struct comedi_device *dev, unsigned int range,
			     unsigned long current_time);
static short fake_flatline(struct comedi_device *dev, unsigned int range,
			   unsigned long current_time);
static short fake_waveform(struct comedi_device *dev, unsigned int channel,
			   unsigned int range, unsigned long current_time);

/* 1000 nanosec in a microsec */
static const int nano_per_micro = 1000;

/* fake analog input ranges */
static const struct comedi_lrange waveform_ai_ranges = {
	2,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 }
};

/*
   This is the background routine used to generate arbitrary data.
   It should run in the background; therefore it is scheduled by
   a timer mechanism.
*/
static void waveform_ai_interrupt(unsigned long arg)
{
	struct comedi_device *dev = (struct comedi_device *)arg;
	struct comedi_async *async = dev->read_subdev->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int i, j;
	/* all times in microsec */
	unsigned long elapsed_time;
	unsigned int num_scans;
	struct timeval now;

	do_gettimeofday(&now);

	elapsed_time =
	    1000000 * (now.tv_sec - devpriv->last.tv_sec) + now.tv_usec -
	    devpriv->last.tv_usec;
	devpriv->last = now;
	num_scans =
	    (devpriv->usec_remainder + elapsed_time) / devpriv->scan_period;
	devpriv->usec_remainder =
	    (devpriv->usec_remainder + elapsed_time) % devpriv->scan_period;
	async->events = 0;

	for (i = 0; i < num_scans; i++) {
		for (j = 0; j < cmd->chanlist_len; j++) {
			cfc_write_to_buffer(dev->read_subdev,
					    fake_waveform(dev,
							  CR_CHAN(cmd->
								  chanlist[j]),
							  CR_RANGE(cmd->
								   chanlist[j]),
							  devpriv->
							  usec_current +
							  i *
							  devpriv->scan_period +
							  j *
							  devpriv->
							  convert_period));
		}
		devpriv->ai_count++;
		if (cmd->stop_src == TRIG_COUNT
		    && devpriv->ai_count >= cmd->stop_arg) {
			async->events |= COMEDI_CB_EOA;
			break;
		}
	}

	devpriv->usec_current += elapsed_time;
	devpriv->usec_current %= devpriv->usec_period;

	if ((async->events & COMEDI_CB_EOA) == 0 && devpriv->timer_running)
		mod_timer(&devpriv->timer, jiffies + 1);
	else
		del_timer(&devpriv->timer);

	comedi_event(dev, dev->read_subdev);
}

static int waveform_attach(struct comedi_device *dev,
			   struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int amplitude = it->options[0];
	int period = it->options[1];
	int i;

	dev->board_name = thisboard->name;

	if (alloc_private(dev, sizeof(struct waveform_private)) < 0)
		return -ENOMEM;

	/* set default amplitude and period */
	if (amplitude <= 0)
		amplitude = 1000000;	/* 1 volt */
	if (period <= 0)
		period = 100000;	/* 0.1 sec */

	devpriv->uvolt_amplitude = amplitude;
	devpriv->usec_period = period;

	dev->n_subdevices = 2;
	if (alloc_subdevices(dev, dev->n_subdevices) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	dev->read_subdev = s;
	/* analog input subdevice */
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_CMD_READ;
	s->n_chan = thisboard->ai_chans;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = &waveform_ai_ranges;
	s->len_chanlist = s->n_chan * 2;
	s->insn_read = waveform_ai_insn_read;
	s->do_cmd = waveform_ai_cmd;
	s->do_cmdtest = waveform_ai_cmdtest;
	s->cancel = waveform_ai_cancel;

	s = dev->subdevices + 1;
	dev->write_subdev = s;
	/* analog output subdevice (loopback) */
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITEABLE | SDF_GROUND;
	s->n_chan = thisboard->ai_chans;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = &waveform_ai_ranges;
	s->len_chanlist = s->n_chan * 2;
	s->insn_write = waveform_ao_insn_write;
	s->do_cmd = NULL;
	s->do_cmdtest = NULL;
	s->cancel = NULL;

	/* Our default loopback value is just a 0V flatline */
	for (i = 0; i < s->n_chan; i++)
		devpriv->ao_loopbacks[i] = s->maxdata / 2;

	init_timer(&(devpriv->timer));
	devpriv->timer.function = waveform_ai_interrupt;
	devpriv->timer.data = (unsigned long)dev;

	printk(KERN_INFO "comedi%d: comedi_test: "
	       "%i microvolt, %li microsecond waveform attached\n", dev->minor,
	       devpriv->uvolt_amplitude, devpriv->usec_period);
	return 1;
}

static int waveform_detach(struct comedi_device *dev)
{
	printk("comedi%d: comedi_test: remove\n", dev->minor);

	if (dev->private)
		waveform_ai_cancel(dev, dev->read_subdev);

	return 0;
}

static int waveform_ai_cmdtest(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW | TRIG_TIMER;
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
	 * step 2: make sure trigger sources are unique and mutually compatible
	 */

	if (cmd->convert_src != TRIG_NOW && cmd->convert_src != TRIG_TIMER)
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
	if (cmd->convert_src == TRIG_NOW) {
		if (cmd->convert_arg != 0) {
			cmd->convert_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < nano_per_micro) {
			cmd->scan_begin_arg = nano_per_micro;
			err++;
		}
		if (cmd->convert_src == TRIG_TIMER &&
		    cmd->scan_begin_arg <
		    cmd->convert_arg * cmd->chanlist_len) {
			cmd->scan_begin_arg =
			    cmd->convert_arg * cmd->chanlist_len;
			err++;
		}
	}
	if (!cmd->chanlist_len) {
		cmd->chanlist_len = 1;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->stop_src == TRIG_COUNT) {
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
	} else {		/* TRIG_NONE */
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		/* round to nearest microsec */
		cmd->scan_begin_arg =
		    nano_per_micro * ((tmp +
				       (nano_per_micro / 2)) / nano_per_micro);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		/* round to nearest microsec */
		cmd->convert_arg =
		    nano_per_micro * ((tmp +
				       (nano_per_micro / 2)) / nano_per_micro);
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (err)
		return 4;

	return 0;
}

static int waveform_ai_cmd(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;

	if (cmd->flags & TRIG_RT) {
		comedi_error(dev,
			     "commands at RT priority not supported in this driver");
		return -1;
	}

	devpriv->timer_running = 1;
	devpriv->ai_count = 0;
	devpriv->scan_period = cmd->scan_begin_arg / nano_per_micro;

	if (cmd->convert_src == TRIG_NOW)
		devpriv->convert_period = 0;
	else if (cmd->convert_src == TRIG_TIMER)
		devpriv->convert_period = cmd->convert_arg / nano_per_micro;
	else {
		comedi_error(dev, "bug setting conversion period");
		return -1;
	}

	do_gettimeofday(&devpriv->last);
	devpriv->usec_current = devpriv->last.tv_usec % devpriv->usec_period;
	devpriv->usec_remainder = 0;

	devpriv->timer.expires = jiffies + 1;
	add_timer(&devpriv->timer);
	return 0;
}

static int waveform_ai_cancel(struct comedi_device *dev,
			      struct comedi_subdevice *s)
{
	devpriv->timer_running = 0;
	del_timer(&devpriv->timer);
	return 0;
}

static short fake_sawtooth(struct comedi_device *dev, unsigned int range_index,
			   unsigned long current_time)
{
	struct comedi_subdevice *s = dev->read_subdev;
	unsigned int offset = s->maxdata / 2;
	u64 value;
	const struct comedi_krange *krange =
	    &s->range_table->range[range_index];
	u64 binary_amplitude;

	binary_amplitude = s->maxdata;
	binary_amplitude *= devpriv->uvolt_amplitude;
	do_div(binary_amplitude, krange->max - krange->min);

	current_time %= devpriv->usec_period;
	value = current_time;
	value *= binary_amplitude * 2;
	do_div(value, devpriv->usec_period);
	value -= binary_amplitude;	/* get rid of sawtooth's dc offset */

	return offset + value;
}

static short fake_squarewave(struct comedi_device *dev,
			     unsigned int range_index,
			     unsigned long current_time)
{
	struct comedi_subdevice *s = dev->read_subdev;
	unsigned int offset = s->maxdata / 2;
	u64 value;
	const struct comedi_krange *krange =
	    &s->range_table->range[range_index];
	current_time %= devpriv->usec_period;

	value = s->maxdata;
	value *= devpriv->uvolt_amplitude;
	do_div(value, krange->max - krange->min);

	if (current_time < devpriv->usec_period / 2)
		value *= -1;

	return offset + value;
}

static short fake_flatline(struct comedi_device *dev, unsigned int range_index,
			   unsigned long current_time)
{
	return dev->read_subdev->maxdata / 2;
}

/* generates a different waveform depending on what channel is read */
static short fake_waveform(struct comedi_device *dev, unsigned int channel,
			   unsigned int range, unsigned long current_time)
{
	enum {
		SAWTOOTH_CHAN,
		SQUARE_CHAN,
	};
	switch (channel) {
	case SAWTOOTH_CHAN:
		return fake_sawtooth(dev, range, current_time);
		break;
	case SQUARE_CHAN:
		return fake_squarewave(dev, range, current_time);
		break;
	default:
		break;
	}

	return fake_flatline(dev, range, current_time);
}

static int waveform_ai_insn_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int i, chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_loopbacks[chan];

	return insn->n;
}

static int waveform_ao_insn_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int i, chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		devpriv->ao_loopbacks[chan] = data[i];

	return insn->n;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
