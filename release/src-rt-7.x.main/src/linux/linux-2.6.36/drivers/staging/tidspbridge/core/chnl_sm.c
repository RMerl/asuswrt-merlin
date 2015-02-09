/*
 * chnl_sm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Implements upper edge functions for Bridge driver channel module.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 *      The lower edge functions must be implemented by the Bridge driver
 *      writer, and are declared in chnl_sm.h.
 *
 *      Care is taken in this code to prevent simulataneous access to channel
 *      queues from
 *      1. Threads.
 *      2. io_dpc(), scheduled from the io_isr() as an event.
 *
 *      This is done primarily by:
 *      - Semaphores.
 *      - state flags in the channel object; and
 *      - ensuring the IO_Dispatch() routine, which is called from both
 *        CHNL_AddIOReq() and the DPC(if implemented), is not re-entered.
 *
 *  Channel Invariant:
 *      There is an important invariant condition which must be maintained per
 *      channel outside of bridge_chnl_get_ioc() and IO_Dispatch(), violation of
 *      which may cause timeouts and/or failure offunction sync_wait_on_event.
 *      This invariant condition is:
 *
 *          LST_Empty(pchnl->pio_completions) ==> pchnl->sync_event is reset
 *      and
 *          !LST_Empty(pchnl->pio_completions) ==> pchnl->sync_event is set.
 */

#include <linux/types.h>

/*  ----------------------------------- OS */
#include <dspbridge/host_os.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */
#include <dspbridge/cfg.h>
#include <dspbridge/sync.h>

/*  ----------------------------------- Bridge Driver */
#include <dspbridge/dspdefs.h>
#include <dspbridge/dspchnl.h>
#include "_tiomap.h"

/*  ----------------------------------- Platform Manager */
#include <dspbridge/dev.h>

/*  ----------------------------------- Others */
#include <dspbridge/io_sm.h>

/*  ----------------------------------- Define for This */
#define USERMODE_ADDR   PAGE_OFFSET

#define MAILBOX_IRQ INT_MAIL_MPU_IRQ

/*  ----------------------------------- Function Prototypes */
static struct lst_list *create_chirp_list(u32 chirps);

static void free_chirp_list(struct lst_list *chirp_list);

static struct chnl_irp *make_new_chirp(void);

static int search_free_channel(struct chnl_mgr *chnl_mgr_obj,
				      u32 *chnl);

/*
 *  ======== bridge_chnl_add_io_req ========
 *      Enqueue an I/O request for data transfer on a channel to the DSP.
 *      The direction (mode) is specified in the channel object. Note the DSP
 *      address is specified for channels opened in direct I/O mode.
 */
int bridge_chnl_add_io_req(struct chnl_object *chnl_obj, void *host_buf,
			       u32 byte_size, u32 buf_size,
			       u32 dw_dsp_addr, u32 dw_arg)
{
	int status = 0;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;
	struct chnl_irp *chnl_packet_obj = NULL;
	struct bridge_dev_context *dev_ctxt;
	struct dev_object *dev_obj;
	u8 dw_state;
	bool is_eos;
	struct chnl_mgr *chnl_mgr_obj = pchnl->chnl_mgr_obj;
	u8 *host_sys_buf = NULL;
	bool sched_dpc = false;
	u16 mb_val = 0;

	is_eos = (byte_size == 0);

	/* Validate args */
	if (!host_buf || !pchnl) {
		status = -EFAULT;
	} else if (is_eos && CHNL_IS_INPUT(pchnl->chnl_mode)) {
		status = -EPERM;
	} else {
		/*
		 * Check the channel state: only queue chirp if channel state
		 * allows it.
		 */
		dw_state = pchnl->dw_state;
		if (dw_state != CHNL_STATEREADY) {
			if (dw_state & CHNL_STATECANCEL)
				status = -ECANCELED;
			else if ((dw_state & CHNL_STATEEOS) &&
				 CHNL_IS_OUTPUT(pchnl->chnl_mode))
				status = -EPIPE;
			else
				/* No other possible states left */
				DBC_ASSERT(0);
		}
	}

	dev_obj = dev_get_first();
	dev_get_bridge_context(dev_obj, &dev_ctxt);
	if (!dev_ctxt)
		status = -EFAULT;

	if (status)
		goto func_end;

	if (pchnl->chnl_type == CHNL_PCPY && pchnl->chnl_id > 1 && host_buf) {
		if (!(host_buf < (void *)USERMODE_ADDR)) {
			host_sys_buf = host_buf;
			goto func_cont;
		}
		/* if addr in user mode, then copy to kernel space */
		host_sys_buf = kmalloc(buf_size, GFP_KERNEL);
		if (host_sys_buf == NULL) {
			status = -ENOMEM;
			goto func_end;
		}
		if (CHNL_IS_OUTPUT(pchnl->chnl_mode)) {
			status = copy_from_user(host_sys_buf, host_buf,
						buf_size);
			if (status) {
				kfree(host_sys_buf);
				host_sys_buf = NULL;
				status = -EFAULT;
				goto func_end;
			}
		}
	}
func_cont:
	/* Mailbox IRQ is disabled to avoid race condition with DMA/ZCPY
	 * channels. DPCCS is held to avoid race conditions with PCPY channels.
	 * If DPC is scheduled in process context (iosm_schedule) and any
	 * non-mailbox interrupt occurs, that DPC will run and break CS. Hence
	 * we disable ALL DPCs. We will try to disable ONLY IO DPC later. */
	spin_lock_bh(&chnl_mgr_obj->chnl_mgr_lock);
	omap_mbox_disable_irq(dev_ctxt->mbox, IRQ_RX);
	if (pchnl->chnl_type == CHNL_PCPY) {
		/* This is a processor-copy channel. */
		if (!status && CHNL_IS_OUTPUT(pchnl->chnl_mode)) {
			/* Check buffer size on output channels for fit. */
			if (byte_size >
			    io_buf_size(pchnl->chnl_mgr_obj->hio_mgr))
				status = -EINVAL;

		}
	}
	if (!status) {
		/* Get a free chirp: */
		chnl_packet_obj =
		    (struct chnl_irp *)lst_get_head(pchnl->free_packets_list);
		if (chnl_packet_obj == NULL)
			status = -EIO;

	}
	if (!status) {
		/* Enqueue the chirp on the chnl's IORequest queue: */
		chnl_packet_obj->host_user_buf = chnl_packet_obj->host_sys_buf =
		    host_buf;
		if (pchnl->chnl_type == CHNL_PCPY && pchnl->chnl_id > 1)
			chnl_packet_obj->host_sys_buf = host_sys_buf;

		/*
		 * Note: for dma chans dw_dsp_addr contains dsp address
		 * of SM buffer.
		 */
		DBC_ASSERT(chnl_mgr_obj->word_size != 0);
		/* DSP address */
		chnl_packet_obj->dsp_tx_addr =
		    dw_dsp_addr / chnl_mgr_obj->word_size;
		chnl_packet_obj->byte_size = byte_size;
		chnl_packet_obj->buf_size = buf_size;
		/* Only valid for output channel */
		chnl_packet_obj->dw_arg = dw_arg;
		chnl_packet_obj->status = (is_eos ? CHNL_IOCSTATEOS :
					   CHNL_IOCSTATCOMPLETE);
		lst_put_tail(pchnl->pio_requests,
			     (struct list_head *)chnl_packet_obj);
		pchnl->cio_reqs++;
		DBC_ASSERT(pchnl->cio_reqs <= pchnl->chnl_packets);
		/*
		 * If end of stream, update the channel state to prevent
		 * more IOR's.
		 */
		if (is_eos)
			pchnl->dw_state |= CHNL_STATEEOS;

		/* Legacy DSM Processor-Copy */
		DBC_ASSERT(pchnl->chnl_type == CHNL_PCPY);
		/* Request IO from the DSP */
		io_request_chnl(chnl_mgr_obj->hio_mgr, pchnl,
				(CHNL_IS_INPUT(pchnl->chnl_mode) ? IO_INPUT :
				 IO_OUTPUT), &mb_val);
		sched_dpc = true;

	}
	omap_mbox_enable_irq(dev_ctxt->mbox, IRQ_RX);
	spin_unlock_bh(&chnl_mgr_obj->chnl_mgr_lock);
	if (mb_val != 0)
		sm_interrupt_dsp(dev_ctxt, mb_val);

	/* Schedule a DPC, to do the actual data transfer */
	if (sched_dpc)
		iosm_schedule(chnl_mgr_obj->hio_mgr);

func_end:
	return status;
}

/*
 *  ======== bridge_chnl_cancel_io ========
 *      Return all I/O requests to the client which have not yet been
 *      transferred.  The channel's I/O completion object is
 *      signalled, and all the I/O requests are queued as IOC's, with the
 *      status field set to CHNL_IOCSTATCANCEL.
 *      This call is typically used in abort situations, and is a prelude to
 *      chnl_close();
 */
int bridge_chnl_cancel_io(struct chnl_object *chnl_obj)
{
	int status = 0;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;
	u32 chnl_id = -1;
	s8 chnl_mode;
	struct chnl_irp *chnl_packet_obj;
	struct chnl_mgr *chnl_mgr_obj = NULL;

	/* Check args: */
	if (pchnl && pchnl->chnl_mgr_obj) {
		chnl_id = pchnl->chnl_id;
		chnl_mode = pchnl->chnl_mode;
		chnl_mgr_obj = pchnl->chnl_mgr_obj;
	} else {
		status = -EFAULT;
	}
	if (status)
		goto func_end;

	/*  Mark this channel as cancelled, to prevent further IORequests or
	 *  IORequests or dispatching. */
	spin_lock_bh(&chnl_mgr_obj->chnl_mgr_lock);
	pchnl->dw_state |= CHNL_STATECANCEL;
	if (LST_IS_EMPTY(pchnl->pio_requests))
		goto func_cont;

	if (pchnl->chnl_type == CHNL_PCPY) {
		/* Indicate we have no more buffers available for transfer: */
		if (CHNL_IS_INPUT(pchnl->chnl_mode)) {
			io_cancel_chnl(chnl_mgr_obj->hio_mgr, chnl_id);
		} else {
			/* Record that we no longer have output buffers
			 * available: */
			chnl_mgr_obj->dw_output_mask &= ~(1 << chnl_id);
		}
	}
	/* Move all IOR's to IOC queue: */
	while (!LST_IS_EMPTY(pchnl->pio_requests)) {
		chnl_packet_obj =
		    (struct chnl_irp *)lst_get_head(pchnl->pio_requests);
		if (chnl_packet_obj) {
			chnl_packet_obj->byte_size = 0;
			chnl_packet_obj->status |= CHNL_IOCSTATCANCEL;
			lst_put_tail(pchnl->pio_completions,
				     (struct list_head *)chnl_packet_obj);
			pchnl->cio_cs++;
			pchnl->cio_reqs--;
			DBC_ASSERT(pchnl->cio_reqs >= 0);
		}
	}
func_cont:
	spin_unlock_bh(&chnl_mgr_obj->chnl_mgr_lock);
func_end:
	return status;
}

/*
 *  ======== bridge_chnl_close ========
 *  Purpose:
 *      Ensures all pending I/O on this channel is cancelled, discards all
 *      queued I/O completion notifications, then frees the resources allocated
 *      for this channel, and makes the corresponding logical channel id
 *      available for subsequent use.
 */
int bridge_chnl_close(struct chnl_object *chnl_obj)
{
	int status;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;

	/* Check args: */
	if (!pchnl) {
		status = -EFAULT;
		goto func_cont;
	}
	{
		/* Cancel IO: this ensures no further IO requests or
		 * notifications. */
		status = bridge_chnl_cancel_io(chnl_obj);
	}
func_cont:
	if (!status) {
		/* Assert I/O on this channel is now cancelled: Protects
		 * from io_dpc. */
		DBC_ASSERT((pchnl->dw_state & CHNL_STATECANCEL));
		/* Invalidate channel object: Protects from
		 * CHNL_GetIOCompletion(). */
		/* Free the slot in the channel manager: */
		pchnl->chnl_mgr_obj->ap_channel[pchnl->chnl_id] = NULL;
		spin_lock_bh(&pchnl->chnl_mgr_obj->chnl_mgr_lock);
		pchnl->chnl_mgr_obj->open_channels -= 1;
		spin_unlock_bh(&pchnl->chnl_mgr_obj->chnl_mgr_lock);
		if (pchnl->ntfy_obj) {
			ntfy_delete(pchnl->ntfy_obj);
			kfree(pchnl->ntfy_obj);
			pchnl->ntfy_obj = NULL;
		}
		/* Reset channel event: (NOTE: user_event freed in user
		 * context.). */
		if (pchnl->sync_event) {
			sync_reset_event(pchnl->sync_event);
			kfree(pchnl->sync_event);
			pchnl->sync_event = NULL;
		}
		/* Free I/O request and I/O completion queues: */
		if (pchnl->pio_completions) {
			free_chirp_list(pchnl->pio_completions);
			pchnl->pio_completions = NULL;
			pchnl->cio_cs = 0;
		}
		if (pchnl->pio_requests) {
			free_chirp_list(pchnl->pio_requests);
			pchnl->pio_requests = NULL;
			pchnl->cio_reqs = 0;
		}
		if (pchnl->free_packets_list) {
			free_chirp_list(pchnl->free_packets_list);
			pchnl->free_packets_list = NULL;
		}
		/* Release channel object. */
		kfree(pchnl);
		pchnl = NULL;
	}
	DBC_ENSURE(status || !pchnl);
	return status;
}

/*
 *  ======== bridge_chnl_create ========
 *      Create a channel manager object, responsible for opening new channels
 *      and closing old ones for a given board.
 */
int bridge_chnl_create(struct chnl_mgr **channel_mgr,
			      struct dev_object *hdev_obj,
			      const struct chnl_mgrattrs *mgr_attrts)
{
	int status = 0;
	struct chnl_mgr *chnl_mgr_obj = NULL;
	u8 max_channels;

	/* Check DBC requirements: */
	DBC_REQUIRE(channel_mgr != NULL);
	DBC_REQUIRE(mgr_attrts != NULL);
	DBC_REQUIRE(mgr_attrts->max_channels > 0);
	DBC_REQUIRE(mgr_attrts->max_channels <= CHNL_MAXCHANNELS);
	DBC_REQUIRE(mgr_attrts->word_size != 0);

	/* Allocate channel manager object */
	chnl_mgr_obj = kzalloc(sizeof(struct chnl_mgr), GFP_KERNEL);
	if (chnl_mgr_obj) {
		/*
		 * The max_channels attr must equal the # of supported chnls for
		 * each transport(# chnls for PCPY = DDMA = ZCPY): i.e.
		 *      mgr_attrts->max_channels = CHNL_MAXCHANNELS =
		 *                       DDMA_MAXDDMACHNLS = DDMA_MAXZCPYCHNLS.
		 */
		DBC_ASSERT(mgr_attrts->max_channels == CHNL_MAXCHANNELS);
		max_channels = CHNL_MAXCHANNELS + CHNL_MAXCHANNELS * CHNL_PCPY;
		/* Create array of channels */
		chnl_mgr_obj->ap_channel = kzalloc(sizeof(struct chnl_object *)
						* max_channels, GFP_KERNEL);
		if (chnl_mgr_obj->ap_channel) {
			/* Initialize chnl_mgr object */
			chnl_mgr_obj->dw_type = CHNL_TYPESM;
			chnl_mgr_obj->word_size = mgr_attrts->word_size;
			/* Total # chnls supported */
			chnl_mgr_obj->max_channels = max_channels;
			chnl_mgr_obj->open_channels = 0;
			chnl_mgr_obj->dw_output_mask = 0;
			chnl_mgr_obj->dw_last_output = 0;
			chnl_mgr_obj->hdev_obj = hdev_obj;
			spin_lock_init(&chnl_mgr_obj->chnl_mgr_lock);
		} else {
			status = -ENOMEM;
		}
	} else {
		status = -ENOMEM;
	}

	if (status) {
		bridge_chnl_destroy(chnl_mgr_obj);
		*channel_mgr = NULL;
	} else {
		/* Return channel manager object to caller... */
		*channel_mgr = chnl_mgr_obj;
	}
	return status;
}

/*
 *  ======== bridge_chnl_destroy ========
 *  Purpose:
 *      Close all open channels, and destroy the channel manager.
 */
int bridge_chnl_destroy(struct chnl_mgr *hchnl_mgr)
{
	int status = 0;
	struct chnl_mgr *chnl_mgr_obj = hchnl_mgr;
	u32 chnl_id;

	if (hchnl_mgr) {
		/* Close all open channels: */
		for (chnl_id = 0; chnl_id < chnl_mgr_obj->max_channels;
		     chnl_id++) {
			status =
			    bridge_chnl_close(chnl_mgr_obj->ap_channel
					      [chnl_id]);
			if (status)
				dev_dbg(bridge, "%s: Error status 0x%x\n",
					__func__, status);
		}

		/* Free channel manager object: */
		kfree(chnl_mgr_obj->ap_channel);

		/* Set hchnl_mgr to NULL in device object. */
		dev_set_chnl_mgr(chnl_mgr_obj->hdev_obj, NULL);
		/* Free this Chnl Mgr object: */
		kfree(hchnl_mgr);
	} else {
		status = -EFAULT;
	}
	return status;
}

/*
 *  ======== bridge_chnl_flush_io ========
 *  purpose:
 *      Flushes all the outstanding data requests on a channel.
 */
int bridge_chnl_flush_io(struct chnl_object *chnl_obj, u32 timeout)
{
	int status = 0;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;
	s8 chnl_mode = -1;
	struct chnl_mgr *chnl_mgr_obj;
	struct chnl_ioc chnl_ioc_obj;
	/* Check args: */
	if (pchnl) {
		if ((timeout == CHNL_IOCNOWAIT)
		    && CHNL_IS_OUTPUT(pchnl->chnl_mode)) {
			status = -EINVAL;
		} else {
			chnl_mode = pchnl->chnl_mode;
			chnl_mgr_obj = pchnl->chnl_mgr_obj;
		}
	} else {
		status = -EFAULT;
	}
	if (!status) {
		/* Note: Currently, if another thread continues to add IO
		 * requests to this channel, this function will continue to
		 * flush all such queued IO requests. */
		if (CHNL_IS_OUTPUT(chnl_mode)
		    && (pchnl->chnl_type == CHNL_PCPY)) {
			/* Wait for IO completions, up to the specified
			 * timeout: */
			while (!LST_IS_EMPTY(pchnl->pio_requests) && !status) {
				status = bridge_chnl_get_ioc(chnl_obj,
						timeout, &chnl_ioc_obj);
				if (status)
					continue;

				if (chnl_ioc_obj.status & CHNL_IOCSTATTIMEOUT)
					status = -ETIMEDOUT;

			}
		} else {
			status = bridge_chnl_cancel_io(chnl_obj);
			/* Now, leave the channel in the ready state: */
			pchnl->dw_state &= ~CHNL_STATECANCEL;
		}
	}
	DBC_ENSURE(status || LST_IS_EMPTY(pchnl->pio_requests));
	return status;
}

/*
 *  ======== bridge_chnl_get_info ========
 *  Purpose:
 *      Retrieve information related to a channel.
 */
int bridge_chnl_get_info(struct chnl_object *chnl_obj,
			     struct chnl_info *channel_info)
{
	int status = 0;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;
	if (channel_info != NULL) {
		if (pchnl) {
			/* Return the requested information: */
			channel_info->hchnl_mgr = pchnl->chnl_mgr_obj;
			channel_info->event_obj = pchnl->user_event;
			channel_info->cnhl_id = pchnl->chnl_id;
			channel_info->dw_mode = pchnl->chnl_mode;
			channel_info->bytes_tx = pchnl->bytes_moved;
			channel_info->process = pchnl->process;
			channel_info->sync_event = pchnl->sync_event;
			channel_info->cio_cs = pchnl->cio_cs;
			channel_info->cio_reqs = pchnl->cio_reqs;
			channel_info->dw_state = pchnl->dw_state;
		} else {
			status = -EFAULT;
		}
	} else {
		status = -EFAULT;
	}
	return status;
}

/*
 *  ======== bridge_chnl_get_ioc ========
 *      Optionally wait for I/O completion on a channel.  Dequeue an I/O
 *      completion record, which contains information about the completed
 *      I/O request.
 *      Note: Ensures Channel Invariant (see notes above).
 */
int bridge_chnl_get_ioc(struct chnl_object *chnl_obj, u32 timeout,
			    struct chnl_ioc *chan_ioc)
{
	int status = 0;
	struct chnl_object *pchnl = (struct chnl_object *)chnl_obj;
	struct chnl_irp *chnl_packet_obj;
	int stat_sync;
	bool dequeue_ioc = true;
	struct chnl_ioc ioc = { NULL, 0, 0, 0, 0 };
	u8 *host_sys_buf = NULL;
	struct bridge_dev_context *dev_ctxt;
	struct dev_object *dev_obj;

	/* Check args: */
	if (!chan_ioc || !pchnl) {
		status = -EFAULT;
	} else if (timeout == CHNL_IOCNOWAIT) {
		if (LST_IS_EMPTY(pchnl->pio_completions))
			status = -EREMOTEIO;

	}

	dev_obj = dev_get_first();
	dev_get_bridge_context(dev_obj, &dev_ctxt);
	if (!dev_ctxt)
		status = -EFAULT;

	if (status)
		goto func_end;

	ioc.status = CHNL_IOCSTATCOMPLETE;
	if (timeout !=
	    CHNL_IOCNOWAIT && LST_IS_EMPTY(pchnl->pio_completions)) {
		if (timeout == CHNL_IOCINFINITE)
			timeout = SYNC_INFINITE;

		stat_sync = sync_wait_on_event(pchnl->sync_event, timeout);
		if (stat_sync == -ETIME) {
			/* No response from DSP */
			ioc.status |= CHNL_IOCSTATTIMEOUT;
			dequeue_ioc = false;
		} else if (stat_sync == -EPERM) {
			/* This can occur when the user mode thread is
			 * aborted (^C), or when _VWIN32_WaitSingleObject()
			 * fails due to unkown causes. */
			/* Even though Wait failed, there may be something in
			 * the Q: */
			if (LST_IS_EMPTY(pchnl->pio_completions)) {
				ioc.status |= CHNL_IOCSTATCANCEL;
				dequeue_ioc = false;
			}
		}
	}
	/* See comment in AddIOReq */
	spin_lock_bh(&pchnl->chnl_mgr_obj->chnl_mgr_lock);
	omap_mbox_disable_irq(dev_ctxt->mbox, IRQ_RX);
	if (dequeue_ioc) {
		/* Dequeue IOC and set chan_ioc; */
		DBC_ASSERT(!LST_IS_EMPTY(pchnl->pio_completions));
		chnl_packet_obj =
		    (struct chnl_irp *)lst_get_head(pchnl->pio_completions);
		/* Update chan_ioc from channel state and chirp: */
		if (chnl_packet_obj) {
			pchnl->cio_cs--;
			/*  If this is a zero-copy channel, then set IOC's pbuf
			 *  to the DSP's address. This DSP address will get
			 *  translated to user's virtual addr later. */
			{
				host_sys_buf = chnl_packet_obj->host_sys_buf;
				ioc.pbuf = chnl_packet_obj->host_user_buf;
			}
			ioc.byte_size = chnl_packet_obj->byte_size;
			ioc.buf_size = chnl_packet_obj->buf_size;
			ioc.dw_arg = chnl_packet_obj->dw_arg;
			ioc.status |= chnl_packet_obj->status;
			/* Place the used chirp on the free list: */
			lst_put_tail(pchnl->free_packets_list,
				     (struct list_head *)chnl_packet_obj);
		} else {
			ioc.pbuf = NULL;
			ioc.byte_size = 0;
		}
	} else {
		ioc.pbuf = NULL;
		ioc.byte_size = 0;
		ioc.dw_arg = 0;
		ioc.buf_size = 0;
	}
	/* Ensure invariant: If any IOC's are queued for this channel... */
	if (!LST_IS_EMPTY(pchnl->pio_completions)) {
		/*  Since DSPStream_Reclaim() does not take a timeout
		 *  parameter, we pass the stream's timeout value to
		 *  bridge_chnl_get_ioc. We cannot determine whether or not
		 *  we have waited in User mode. Since the stream's timeout
		 *  value may be non-zero, we still have to set the event.
		 *  Therefore, this optimization is taken out.
		 *
		 *  if (timeout == CHNL_IOCNOWAIT) {
		 *    ... ensure event is set..
		 *      sync_set_event(pchnl->sync_event);
		 *  } */
		sync_set_event(pchnl->sync_event);
	} else {
		/* else, if list is empty, ensure event is reset. */
		sync_reset_event(pchnl->sync_event);
	}
	omap_mbox_enable_irq(dev_ctxt->mbox, IRQ_RX);
	spin_unlock_bh(&pchnl->chnl_mgr_obj->chnl_mgr_lock);
	if (dequeue_ioc
	    && (pchnl->chnl_type == CHNL_PCPY && pchnl->chnl_id > 1)) {
		if (!(ioc.pbuf < (void *)USERMODE_ADDR))
			goto func_cont;

		/* If the addr is in user mode, then copy it */
		if (!host_sys_buf || !ioc.pbuf) {
			status = -EFAULT;
			goto func_cont;
		}
		if (!CHNL_IS_INPUT(pchnl->chnl_mode))
			goto func_cont1;

		/*host_user_buf */
		status = copy_to_user(ioc.pbuf, host_sys_buf, ioc.byte_size);
		if (status) {
			if (current->flags & PF_EXITING)
				status = 0;
		}
		if (status)
			status = -EFAULT;
func_cont1:
		kfree(host_sys_buf);
	}
func_cont:
	/* Update User's IOC block: */
	*chan_ioc = ioc;
func_end:
	return status;
}

/*
 *  ======== bridge_chnl_get_mgr_info ========
 *      Retrieve information related to the channel manager.
 */
int bridge_chnl_get_mgr_info(struct chnl_mgr *hchnl_mgr, u32 ch_id,
				 struct chnl_mgrinfo *mgr_info)
{
	int status = 0;
	struct chnl_mgr *chnl_mgr_obj = (struct chnl_mgr *)hchnl_mgr;

	if (mgr_info != NULL) {
		if (ch_id <= CHNL_MAXCHANNELS) {
			if (hchnl_mgr) {
				/* Return the requested information: */
				mgr_info->chnl_obj =
				    chnl_mgr_obj->ap_channel[ch_id];
				mgr_info->open_channels =
				    chnl_mgr_obj->open_channels;
				mgr_info->dw_type = chnl_mgr_obj->dw_type;
				/* total # of chnls */
				mgr_info->max_channels =
				    chnl_mgr_obj->max_channels;
			} else {
				status = -EFAULT;
			}
		} else {
			status = -ECHRNG;
		}
	} else {
		status = -EFAULT;
	}

	return status;
}

/*
 *  ======== bridge_chnl_idle ========
 *      Idles a particular channel.
 */
int bridge_chnl_idle(struct chnl_object *chnl_obj, u32 timeout,
			    bool flush_data)
{
	s8 chnl_mode;
	struct chnl_mgr *chnl_mgr_obj;
	int status = 0;

	DBC_REQUIRE(chnl_obj);

	chnl_mode = chnl_obj->chnl_mode;
	chnl_mgr_obj = chnl_obj->chnl_mgr_obj;

	if (CHNL_IS_OUTPUT(chnl_mode) && !flush_data) {
		/* Wait for IO completions, up to the specified timeout: */
		status = bridge_chnl_flush_io(chnl_obj, timeout);
	} else {
		status = bridge_chnl_cancel_io(chnl_obj);

		/* Reset the byte count and put channel back in ready state. */
		chnl_obj->bytes_moved = 0;
		chnl_obj->dw_state &= ~CHNL_STATECANCEL;
	}

	return status;
}

/*
 *  ======== bridge_chnl_open ========
 *      Open a new half-duplex channel to the DSP board.
 */
int bridge_chnl_open(struct chnl_object **chnl,
			    struct chnl_mgr *hchnl_mgr, s8 chnl_mode,
			    u32 ch_id, const struct chnl_attr *pattrs)
{
	int status = 0;
	struct chnl_mgr *chnl_mgr_obj = hchnl_mgr;
	struct chnl_object *pchnl = NULL;
	struct sync_object *sync_event = NULL;
	/* Ensure DBC requirements: */
	DBC_REQUIRE(chnl != NULL);
	DBC_REQUIRE(pattrs != NULL);
	DBC_REQUIRE(hchnl_mgr != NULL);
	*chnl = NULL;
	/* Validate Args: */
	if (pattrs->uio_reqs == 0) {
		status = -EINVAL;
	} else {
		if (!hchnl_mgr) {
			status = -EFAULT;
		} else {
			if (ch_id != CHNL_PICKFREE) {
				if (ch_id >= chnl_mgr_obj->max_channels)
					status = -ECHRNG;
				else if (chnl_mgr_obj->ap_channel[ch_id] !=
					 NULL)
					status = -EALREADY;
			} else {
				/* Check for free channel */
				status =
				    search_free_channel(chnl_mgr_obj, &ch_id);
			}
		}
	}
	if (status)
		goto func_end;

	DBC_ASSERT(ch_id < chnl_mgr_obj->max_channels);
	/* Create channel object: */
	pchnl = kzalloc(sizeof(struct chnl_object), GFP_KERNEL);
	if (!pchnl) {
		status = -ENOMEM;
		goto func_end;
	}
	/* Protect queues from io_dpc: */
	pchnl->dw_state = CHNL_STATECANCEL;
	/* Allocate initial IOR and IOC queues: */
	pchnl->free_packets_list = create_chirp_list(pattrs->uio_reqs);
	pchnl->pio_requests = create_chirp_list(0);
	pchnl->pio_completions = create_chirp_list(0);
	pchnl->chnl_packets = pattrs->uio_reqs;
	pchnl->cio_cs = 0;
	pchnl->cio_reqs = 0;
	sync_event = kzalloc(sizeof(struct sync_object), GFP_KERNEL);
	if (sync_event)
		sync_init_event(sync_event);
	else
		status = -ENOMEM;

	if (!status) {
		pchnl->ntfy_obj = kmalloc(sizeof(struct ntfy_object),
							GFP_KERNEL);
		if (pchnl->ntfy_obj)
			ntfy_init(pchnl->ntfy_obj);
		else
			status = -ENOMEM;
	}

	if (!status) {
		if (pchnl->pio_completions && pchnl->pio_requests &&
		    pchnl->free_packets_list) {
			/* Initialize CHNL object fields: */
			pchnl->chnl_mgr_obj = chnl_mgr_obj;
			pchnl->chnl_id = ch_id;
			pchnl->chnl_mode = chnl_mode;
			pchnl->user_event = sync_event;
			pchnl->sync_event = sync_event;
			/* Get the process handle */
			pchnl->process = current->tgid;
			pchnl->pcb_arg = 0;
			pchnl->bytes_moved = 0;
			/* Default to proc-copy */
			pchnl->chnl_type = CHNL_PCPY;
		} else {
			status = -ENOMEM;
		}
	}

	if (status) {
		/* Free memory */
		if (pchnl->pio_completions) {
			free_chirp_list(pchnl->pio_completions);
			pchnl->pio_completions = NULL;
			pchnl->cio_cs = 0;
		}
		if (pchnl->pio_requests) {
			free_chirp_list(pchnl->pio_requests);
			pchnl->pio_requests = NULL;
		}
		if (pchnl->free_packets_list) {
			free_chirp_list(pchnl->free_packets_list);
			pchnl->free_packets_list = NULL;
		}
		kfree(sync_event);
		sync_event = NULL;

		if (pchnl->ntfy_obj) {
			ntfy_delete(pchnl->ntfy_obj);
			kfree(pchnl->ntfy_obj);
			pchnl->ntfy_obj = NULL;
		}
		kfree(pchnl);
	} else {
		/* Insert channel object in channel manager: */
		chnl_mgr_obj->ap_channel[pchnl->chnl_id] = pchnl;
		spin_lock_bh(&chnl_mgr_obj->chnl_mgr_lock);
		chnl_mgr_obj->open_channels++;
		spin_unlock_bh(&chnl_mgr_obj->chnl_mgr_lock);
		/* Return result... */
		pchnl->dw_state = CHNL_STATEREADY;
		*chnl = pchnl;
	}
func_end:
	DBC_ENSURE((!status && pchnl) || (*chnl == NULL));
	return status;
}

/*
 *  ======== bridge_chnl_register_notify ========
 *      Registers for events on a particular channel.
 */
int bridge_chnl_register_notify(struct chnl_object *chnl_obj,
				    u32 event_mask, u32 notify_type,
				    struct dsp_notification *hnotification)
{
	int status = 0;

	DBC_ASSERT(!(event_mask & ~(DSP_STREAMDONE | DSP_STREAMIOCOMPLETION)));

	if (event_mask)
		status = ntfy_register(chnl_obj->ntfy_obj, hnotification,
						event_mask, notify_type);
	else
		status = ntfy_unregister(chnl_obj->ntfy_obj, hnotification);

	return status;
}

/*
 *  ======== create_chirp_list ========
 *  Purpose:
 *      Initialize a queue of channel I/O Request/Completion packets.
 *  Parameters:
 *      chirps:     Number of Chirps to allocate.
 *  Returns:
 *      Pointer to queue of IRPs, or NULL.
 *  Requires:
 *  Ensures:
 */
static struct lst_list *create_chirp_list(u32 chirps)
{
	struct lst_list *chirp_list;
	struct chnl_irp *chnl_packet_obj;
	u32 i;

	chirp_list = kzalloc(sizeof(struct lst_list), GFP_KERNEL);

	if (chirp_list) {
		INIT_LIST_HEAD(&chirp_list->head);
		/* Make N chirps and place on queue. */
		for (i = 0; (i < chirps)
		     && ((chnl_packet_obj = make_new_chirp()) != NULL); i++) {
			lst_put_tail(chirp_list,
				     (struct list_head *)chnl_packet_obj);
		}

		/* If we couldn't allocate all chirps, free those allocated: */
		if (i != chirps) {
			free_chirp_list(chirp_list);
			chirp_list = NULL;
		}
	}

	return chirp_list;
}

/*
 *  ======== free_chirp_list ========
 *  Purpose:
 *      Free the queue of Chirps.
 */
static void free_chirp_list(struct lst_list *chirp_list)
{
	DBC_REQUIRE(chirp_list != NULL);

	while (!LST_IS_EMPTY(chirp_list))
		kfree(lst_get_head(chirp_list));

	kfree(chirp_list);
}

/*
 *  ======== make_new_chirp ========
 *      Allocate the memory for a new channel IRP.
 */
static struct chnl_irp *make_new_chirp(void)
{
	struct chnl_irp *chnl_packet_obj;

	chnl_packet_obj = kzalloc(sizeof(struct chnl_irp), GFP_KERNEL);
	if (chnl_packet_obj != NULL) {
		/* lst_init_elem only resets the list's member values. */
		lst_init_elem(&chnl_packet_obj->link);
	}

	return chnl_packet_obj;
}

/*
 *  ======== search_free_channel ========
 *      Search for a free channel slot in the array of channel pointers.
 */
static int search_free_channel(struct chnl_mgr *chnl_mgr_obj,
				      u32 *chnl)
{
	int status = -ENOSR;
	u32 i;

	DBC_REQUIRE(chnl_mgr_obj);

	for (i = 0; i < chnl_mgr_obj->max_channels; i++) {
		if (chnl_mgr_obj->ap_channel[i] == NULL) {
			status = 0;
			*chnl = i;
			break;
		}
	}

	return status;
}
