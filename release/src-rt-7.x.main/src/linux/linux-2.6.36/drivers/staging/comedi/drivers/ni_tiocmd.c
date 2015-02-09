/*
  comedi/drivers/ni_tiocmd.c
  Command support for NI general purpose counters

  Copyright (C) 2006 Frank Mori Hess <fmhess@users.sourceforge.net>

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
Driver: ni_tiocmd
Description: National Instruments general purpose counters command support
Devices:
Author: J.P. Mellor <jpmellor@rose-hulman.edu>,
	Herman.Bruyninckx@mech.kuleuven.ac.be,
	Wim.Meeussen@mech.kuleuven.ac.be,
	Klaas.Gadeyne@mech.kuleuven.ac.be,
	Frank Mori Hess <fmhess@users.sourceforge.net>
Updated: Fri, 11 Apr 2008 12:32:35 +0100
Status: works

This module is not used directly by end-users.  Rather, it
is used by other drivers (for example ni_660x and ni_pcimio)
to provide command support for NI's general purpose counters.
It was originally split out of ni_tio.c to stop the 'ni_tio'
module depending on the 'mite' module.

References:
DAQ 660x Register-Level Programmer Manual  (NI 370505A-01)
DAQ 6601/6602 User Manual (NI 322137B-01)
340934b.pdf  DAQ-STC reference manual

*/
/*
TODO:
	Support use of both banks X and Y
*/

#include "ni_tio_internal.h"
#include "mite.h"

MODULE_AUTHOR("Comedi <comedi@comedi.org>");
MODULE_DESCRIPTION("Comedi command support for NI general-purpose counters");
MODULE_LICENSE("GPL");

static void ni_tio_configure_dma(struct ni_gpct *counter, short enable,
				 short read_not_write)
{
	struct ni_gpct_device *counter_dev = counter->counter_dev;
	unsigned input_select_bits = 0;

	if (enable) {
		if (read_not_write) {
			input_select_bits |= Gi_Read_Acknowledges_Irq;
		} else {
			input_select_bits |= Gi_Write_Acknowledges_Irq;
		}
	}
	ni_tio_set_bits(counter,
			NITIO_Gi_Input_Select_Reg(counter->counter_index),
			Gi_Read_Acknowledges_Irq | Gi_Write_Acknowledges_Irq,
			input_select_bits);
	switch (counter_dev->variant) {
	case ni_gpct_variant_e_series:
		break;
	case ni_gpct_variant_m_series:
	case ni_gpct_variant_660x:
		{
			unsigned gi_dma_config_bits = 0;

			if (enable) {
				gi_dma_config_bits |= Gi_DMA_Enable_Bit;
				gi_dma_config_bits |= Gi_DMA_Int_Bit;
			}
			if (read_not_write == 0) {
				gi_dma_config_bits |= Gi_DMA_Write_Bit;
			}
			ni_tio_set_bits(counter,
					NITIO_Gi_DMA_Config_Reg(counter->
								counter_index),
					Gi_DMA_Enable_Bit | Gi_DMA_Int_Bit |
					Gi_DMA_Write_Bit, gi_dma_config_bits);
		}
		break;
	}
}

static int ni_tio_input_inttrig(struct comedi_device *dev,
				struct comedi_subdevice *s,
				unsigned int trignum)
{
	unsigned long flags;
	int retval = 0;
	struct ni_gpct *counter = s->private;

	BUG_ON(counter == NULL);
	if (trignum != 0)
		return -EINVAL;

	spin_lock_irqsave(&counter->lock, flags);
	if (counter->mite_chan)
		mite_dma_arm(counter->mite_chan);
	else
		retval = -EIO;
	spin_unlock_irqrestore(&counter->lock, flags);
	if (retval < 0)
		return retval;
	retval = ni_tio_arm(counter, 1, NI_GPCT_ARM_IMMEDIATE);
	s->async->inttrig = NULL;

	return retval;
}

static int ni_tio_input_cmd(struct ni_gpct *counter, struct comedi_async *async)
{
	struct ni_gpct_device *counter_dev = counter->counter_dev;
	struct comedi_cmd *cmd = &async->cmd;
	int retval = 0;

	/* write alloc the entire buffer */
	comedi_buf_write_alloc(async, async->prealloc_bufsz);
	counter->mite_chan->dir = COMEDI_INPUT;
	switch (counter_dev->variant) {
	case ni_gpct_variant_m_series:
	case ni_gpct_variant_660x:
		mite_prep_dma(counter->mite_chan, 32, 32);
		break;
	case ni_gpct_variant_e_series:
		mite_prep_dma(counter->mite_chan, 16, 32);
		break;
	default:
		BUG();
		break;
	}
	ni_tio_set_bits(counter, NITIO_Gi_Command_Reg(counter->counter_index),
			Gi_Save_Trace_Bit, 0);
	ni_tio_configure_dma(counter, 1, 1);
	switch (cmd->start_src) {
	case TRIG_NOW:
		async->inttrig = NULL;
		mite_dma_arm(counter->mite_chan);
		retval = ni_tio_arm(counter, 1, NI_GPCT_ARM_IMMEDIATE);
		break;
	case TRIG_INT:
		async->inttrig = &ni_tio_input_inttrig;
		break;
	case TRIG_EXT:
		async->inttrig = NULL;
		mite_dma_arm(counter->mite_chan);
		retval = ni_tio_arm(counter, 1, cmd->start_arg);
	case TRIG_OTHER:
		async->inttrig = NULL;
		mite_dma_arm(counter->mite_chan);
		break;
	default:
		BUG();
		break;
	}
	return retval;
}

static int ni_tio_output_cmd(struct ni_gpct *counter,
			     struct comedi_async *async)
{
	printk("ni_tio: output commands not yet implemented.\n");
	return -ENOTSUPP;

	counter->mite_chan->dir = COMEDI_OUTPUT;
	mite_prep_dma(counter->mite_chan, 32, 32);
	ni_tio_configure_dma(counter, 1, 0);
	mite_dma_arm(counter->mite_chan);
	return ni_tio_arm(counter, 1, NI_GPCT_ARM_IMMEDIATE);
}

static int ni_tio_cmd_setup(struct ni_gpct *counter, struct comedi_async *async)
{
	struct comedi_cmd *cmd = &async->cmd;
	int set_gate_source = 0;
	unsigned gate_source;
	int retval = 0;

	if (cmd->scan_begin_src == TRIG_EXT) {
		set_gate_source = 1;
		gate_source = cmd->scan_begin_arg;
	} else if (cmd->convert_src == TRIG_EXT) {
		set_gate_source = 1;
		gate_source = cmd->convert_arg;
	}
	if (set_gate_source) {
		retval = ni_tio_set_gate_src(counter, 0, gate_source);
	}
	if (cmd->flags & TRIG_WAKE_EOS) {
		ni_tio_set_bits(counter,
				NITIO_Gi_Interrupt_Enable_Reg(counter->
							      counter_index),
				Gi_Gate_Interrupt_Enable_Bit(counter->
							     counter_index),
				Gi_Gate_Interrupt_Enable_Bit(counter->
							     counter_index));
	}
	return retval;
}

int ni_tio_cmd(struct ni_gpct *counter, struct comedi_async *async)
{
	struct comedi_cmd *cmd = &async->cmd;
	int retval = 0;
	unsigned long flags;

	spin_lock_irqsave(&counter->lock, flags);
	if (counter->mite_chan == NULL) {
		printk
		    ("ni_tio: commands only supported with DMA.  Interrupt-driven commands not yet implemented.\n");
		retval = -EIO;
	} else {
		retval = ni_tio_cmd_setup(counter, async);
		if (retval == 0) {
			if (cmd->flags & CMDF_WRITE) {
				retval = ni_tio_output_cmd(counter, async);
			} else {
				retval = ni_tio_input_cmd(counter, async);
			}
		}
	}
	spin_unlock_irqrestore(&counter->lock, flags);
	return retval;
}

int ni_tio_cmdtest(struct ni_gpct *counter, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	int sources;

	/* step 1: make sure trigger sources are trivially valid */

	tmp = cmd->start_src;
	sources = TRIG_NOW | TRIG_INT | TRIG_OTHER;
	if (ni_tio_counting_mode_registers_present(counter->counter_dev))
		sources |= TRIG_EXT;
	cmd->start_src &= sources;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_FOLLOW | TRIG_EXT | TRIG_OTHER;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	sources = TRIG_NOW | TRIG_EXT | TRIG_OTHER;
	cmd->convert_src &= sources;
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

	/* step 2: make sure trigger sources are unique... */

	if (cmd->start_src != TRIG_NOW &&
	    cmd->start_src != TRIG_INT &&
	    cmd->start_src != TRIG_EXT && cmd->start_src != TRIG_OTHER)
		err++;
	if (cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->scan_begin_src != TRIG_EXT &&
	    cmd->scan_begin_src != TRIG_OTHER)
		err++;
	if (cmd->convert_src != TRIG_OTHER &&
	    cmd->convert_src != TRIG_EXT && cmd->convert_src != TRIG_NOW)
		err++;
	if (cmd->stop_src != TRIG_NONE)
		err++;
	/* ... and mutually compatible */
	if (cmd->convert_src != TRIG_NOW && cmd->scan_begin_src != TRIG_FOLLOW)
		err++;

	if (err)
		return 2;

	/* step 3: make sure arguments are trivially compatible */
	if (cmd->start_src != TRIG_EXT) {
		if (cmd->start_arg != 0) {
			cmd->start_arg = 0;
			err++;
		}
	}
	if (cmd->scan_begin_src != TRIG_EXT) {
		if (cmd->scan_begin_arg) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}
	if (cmd->convert_src != TRIG_EXT) {
		if (cmd->convert_arg) {
			cmd->convert_arg = 0;
			err++;
		}
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->stop_src == TRIG_NONE) {
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	if (err)
		return 4;

	return 0;
}

int ni_tio_cancel(struct ni_gpct *counter)
{
	unsigned long flags;

	ni_tio_arm(counter, 0, 0);
	spin_lock_irqsave(&counter->lock, flags);
	if (counter->mite_chan) {
		mite_dma_disarm(counter->mite_chan);
	}
	spin_unlock_irqrestore(&counter->lock, flags);
	ni_tio_configure_dma(counter, 0, 0);

	ni_tio_set_bits(counter,
			NITIO_Gi_Interrupt_Enable_Reg(counter->counter_index),
			Gi_Gate_Interrupt_Enable_Bit(counter->counter_index),
			0x0);
	return 0;
}

	/* During buffered input counter operation for e-series, the gate interrupt is acked
	   automatically by the dma controller, due to the Gi_Read/Write_Acknowledges_IRQ bits
	   in the input select register.  */
static int should_ack_gate(struct ni_gpct *counter)
{
	unsigned long flags;
	int retval = 0;

	switch (counter->counter_dev->variant) {
	case ni_gpct_variant_m_series:
	case ni_gpct_variant_660x:	/*  not sure if 660x really supports gate interrupts (the bits are not listed in register-level manual) */
		return 1;
		break;
	case ni_gpct_variant_e_series:
		spin_lock_irqsave(&counter->lock, flags);
		{
			if (counter->mite_chan == NULL ||
			    counter->mite_chan->dir != COMEDI_INPUT ||
			    (mite_done(counter->mite_chan))) {
				retval = 1;
			}
		}
		spin_unlock_irqrestore(&counter->lock, flags);
		break;
	}
	return retval;
}

void ni_tio_acknowledge_and_confirm(struct ni_gpct *counter, int *gate_error,
				    int *tc_error, int *perm_stale_data,
				    int *stale_data)
{
	const unsigned short gxx_status = read_register(counter,
							NITIO_Gxx_Status_Reg
							(counter->
							 counter_index));
	const unsigned short gi_status = read_register(counter,
						       NITIO_Gi_Status_Reg
						       (counter->
							counter_index));
	unsigned ack = 0;

	if (gate_error)
		*gate_error = 0;
	if (tc_error)
		*tc_error = 0;
	if (perm_stale_data)
		*perm_stale_data = 0;
	if (stale_data)
		*stale_data = 0;

	if (gxx_status & Gi_Gate_Error_Bit(counter->counter_index)) {
		ack |= Gi_Gate_Error_Confirm_Bit(counter->counter_index);
		if (gate_error) {
			/*660x don't support automatic acknowledgement of gate interrupt via dma read/write
			   and report bogus gate errors */
			if (counter->counter_dev->variant !=
			    ni_gpct_variant_660x) {
				*gate_error = 1;
			}
		}
	}
	if (gxx_status & Gi_TC_Error_Bit(counter->counter_index)) {
		ack |= Gi_TC_Error_Confirm_Bit(counter->counter_index);
		if (tc_error)
			*tc_error = 1;
	}
	if (gi_status & Gi_TC_Bit) {
		ack |= Gi_TC_Interrupt_Ack_Bit;
	}
	if (gi_status & Gi_Gate_Interrupt_Bit) {
		if (should_ack_gate(counter))
			ack |= Gi_Gate_Interrupt_Ack_Bit;
	}
	if (ack)
		write_register(counter, ack,
			       NITIO_Gi_Interrupt_Acknowledge_Reg
			       (counter->counter_index));
	if (ni_tio_get_soft_copy
	    (counter,
	     NITIO_Gi_Mode_Reg(counter->counter_index)) &
	    Gi_Loading_On_Gate_Bit) {
		if (gxx_status & Gi_Stale_Data_Bit(counter->counter_index)) {
			if (stale_data)
				*stale_data = 1;
		}
		if (read_register(counter,
				  NITIO_Gxx_Joint_Status2_Reg
				  (counter->counter_index)) &
		    Gi_Permanent_Stale_Bit(counter->counter_index)) {
			printk("%s: Gi_Permanent_Stale_Data detected.\n",
			       __FUNCTION__);
			if (perm_stale_data)
				*perm_stale_data = 1;
		}
	}
}

void ni_tio_handle_interrupt(struct ni_gpct *counter,
			     struct comedi_subdevice *s)
{
	unsigned gpct_mite_status;
	unsigned long flags;
	int gate_error;
	int tc_error;
	int perm_stale_data;

	ni_tio_acknowledge_and_confirm(counter, &gate_error, &tc_error,
				       &perm_stale_data, NULL);
	if (gate_error) {
		printk("%s: Gi_Gate_Error detected.\n", __FUNCTION__);
		s->async->events |= COMEDI_CB_OVERFLOW;
	}
	if (perm_stale_data) {
		s->async->events |= COMEDI_CB_ERROR;
	}
	switch (counter->counter_dev->variant) {
	case ni_gpct_variant_m_series:
	case ni_gpct_variant_660x:
		if (read_register(counter,
				  NITIO_Gi_DMA_Status_Reg
				  (counter->counter_index)) & Gi_DRQ_Error_Bit)
		{
			printk("%s: Gi_DRQ_Error detected.\n", __FUNCTION__);
			s->async->events |= COMEDI_CB_OVERFLOW;
		}
		break;
	case ni_gpct_variant_e_series:
		break;
	}
	spin_lock_irqsave(&counter->lock, flags);
	if (counter->mite_chan == NULL) {
		spin_unlock_irqrestore(&counter->lock, flags);
		return;
	}
	gpct_mite_status = mite_get_status(counter->mite_chan);
	if (gpct_mite_status & CHSR_LINKC) {
		writel(CHOR_CLRLC,
		       counter->mite_chan->mite->mite_io_addr +
		       MITE_CHOR(counter->mite_chan->channel));
	}
	mite_sync_input_dma(counter->mite_chan, s->async);
	spin_unlock_irqrestore(&counter->lock, flags);
}

void ni_tio_set_mite_channel(struct ni_gpct *counter,
			     struct mite_channel *mite_chan)
{
	unsigned long flags;

	spin_lock_irqsave(&counter->lock, flags);
	counter->mite_chan = mite_chan;
	spin_unlock_irqrestore(&counter->lock, flags);
}

static int __init ni_tiocmd_init_module(void)
{
	return 0;
}

module_init(ni_tiocmd_init_module);

static void __exit ni_tiocmd_cleanup_module(void)
{
}

module_exit(ni_tiocmd_cleanup_module);

EXPORT_SYMBOL_GPL(ni_tio_cmd);
EXPORT_SYMBOL_GPL(ni_tio_cmdtest);
EXPORT_SYMBOL_GPL(ni_tio_cancel);
EXPORT_SYMBOL_GPL(ni_tio_handle_interrupt);
EXPORT_SYMBOL_GPL(ni_tio_set_mite_channel);
EXPORT_SYMBOL_GPL(ni_tio_acknowledge_and_confirm);
