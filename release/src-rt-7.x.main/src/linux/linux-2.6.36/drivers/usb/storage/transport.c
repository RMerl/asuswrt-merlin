/* Driver for USB Mass Storage compliant devices
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2000 Stephen J. Gowdy (SGowdy@lbl.gov)
 *   (c) 2002 Alan Stern <stern@rowland.org>
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/sched.h>
#include <linux/gfp.h>
#include <linux/errno.h>

#include <linux/usb/quirks.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>

#include "usb.h"
#include "transport.h"
#include "protocol.h"
#include "scsiglue.h"
#include "debug.h"

#include <linux/blkdev.h>
#include "../../scsi/sd.h"

#ifdef CONFIG_BCM47XX
extern int csw_retry;
#endif /* CONFIG_BCM47XX */
/***********************************************************************
 * Data transfer routines
 ***********************************************************************/

/*
 * This is subtle, so pay attention:
 * ---------------------------------
 * We're very concerned about races with a command abort.  Hanging this code
 * is a sure fire way to hang the kernel.  (Note that this discussion applies
 * only to transactions resulting from a scsi queued-command, since only
 * these transactions are subject to a scsi abort.  Other transactions, such
 * as those occurring during device-specific initialization, must be handled
 * by a separate code path.)
 *
 * The abort function (usb_storage_command_abort() in scsiglue.c) first
 * sets the machine state and the ABORTING bit in us->dflags to prevent
 * new URBs from being submitted.  It then calls usb_stor_stop_transport()
 * below, which atomically tests-and-clears the URB_ACTIVE bit in us->dflags
 * to see if the current_urb needs to be stopped.  Likewise, the SG_ACTIVE
 * bit is tested to see if the current_sg scatter-gather request needs to be
 * stopped.  The timeout callback routine does much the same thing.
 *
 * When a disconnect occurs, the DISCONNECTING bit in us->dflags is set to
 * prevent new URBs from being submitted, and usb_stor_stop_transport() is
 * called to stop any ongoing requests.
 *
 * The submit function first verifies that the submitting is allowed
 * (neither ABORTING nor DISCONNECTING bits are set) and that the submit
 * completes without errors, and only then sets the URB_ACTIVE bit.  This
 * prevents the stop_transport() function from trying to cancel the URB
 * while the submit call is underway.  Next, the submit function must test
 * the flags to see if an abort or disconnect occurred during the submission
 * or before the URB_ACTIVE bit was set.  If so, it's essential to cancel
 * the URB if it hasn't been cancelled already (i.e., if the URB_ACTIVE bit
 * is still set).  Either way, the function must then wait for the URB to
 * finish.  Note that the URB can still be in progress even after a call to
 * usb_unlink_urb() returns.
 *
 * The idea is that (1) once the ABORTING or DISCONNECTING bit is set,
 * either the stop_transport() function or the submitting function
 * is guaranteed to call usb_unlink_urb() for an active URB,
 * and (2) test_and_clear_bit() prevents usb_unlink_urb() from being
 * called more than once or from being called during usb_submit_urb().
 */

/* This is the completion handler which will wake us up when an URB
 * completes.
 */
static void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = urb->context;

	complete(urb_done_ptr);
}

/* This is the common part of the URB message submission code
 *
 * All URBs from the usb-storage driver involved in handling a queued scsi
 * command _must_ pass through this function (or something like it) for the
 * abort mechanisms to work properly.
 */
static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	struct completion urb_done;
	long timeleft;
	int status;

	/* don't submit URBs during abort processing */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return -EIO;

	/* set up data structures for the wakeup system */
	init_completion(&urb_done);

	/* fill the common fields in the URB */
	us->current_urb->context = &urb_done;
	us->current_urb->transfer_flags = 0;

	/* we assume that if transfer_buffer isn't us->iobuf then it
	 * hasn't been mapped for DMA.  Yes, this is clunky, but it's
	 * easier than always having the caller tell us whether the
	 * transfer buffer has already been mapped. */
	if (us->current_urb->transfer_buffer == us->iobuf)
		us->current_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	us->current_urb->transfer_dma = us->iobuf_dma;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb, GFP_NOIO);
	if (status) {
		/* something went wrong */
		return status;
	}

	/* since the URB has been submitted successfully, it's now okay
	 * to cancel it */
	set_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	/* did an abort occur during the submission? */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {

		/* cancel the URB, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
			US_DEBUGP("-- cancelling URB\n");
			usb_unlink_urb(us->current_urb);
		}
	}
 
	/* wait for the completion of the URB */
	timeleft = wait_for_completion_interruptible_timeout(
			&urb_done, timeout ? : MAX_SCHEDULE_TIMEOUT);
 
	clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	if (timeleft <= 0) {
		US_DEBUGP("%s -- cancelling URB\n",
			  timeleft == 0 ? "Timeout" : "Signal");
		usb_kill_urb(us->current_urb);
	}

	/* return the URB status */
	return us->current_urb->status;
}

/*
 * Transfer one control message, with timeouts, and allowing early
 * termination.  Return codes are usual -Exxx, *not* USB_STOR_XFER_xxx.
 */
int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index, 
		 void *data, u16 size, int timeout)
{
	int status;

	US_DEBUGP("%s: rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
			__func__, request, requesttype,
			value, index, size);

	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	/* return the actual length of the data transferred if no error */
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}
EXPORT_SYMBOL_GPL(usb_stor_control_msg);

/* This is a version of usb_clear_halt() that allows early termination and
 * doesn't read the status from the device -- this is because some devices
 * crash their internal firmware when the status is requested after a halt.
 *
 * A definitive list of these 'bad' devices is too difficult to maintain or
 * make complete enough to be useful.  This problem was first observed on the
 * Hagiwara FlashGate DUAL unit.  However, bus traces reveal that neither
 * MacOS nor Windows checks the status after clearing a halt.
 *
 * Since many vendors in this space limit their testing to interoperability
 * with these two OSes, specification violations like this one are common.
 */
int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	if (usb_pipein (pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
		USB_ENDPOINT_HALT, endp,
		NULL, 0, 3*HZ);

	if (result >= 0)
		usb_reset_endpoint(us->pusb_dev, endp);

	US_DEBUGP("%s: result = %d\n", __func__, result);
	return result;
}
EXPORT_SYMBOL_GPL(usb_stor_clear_halt);


/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	US_DEBUGP("Status code %d; transferred %u/%u\n",
			result, partial, length);
	switch (result) {

	/* no error code; did we send all the data? */
	case 0:
		if (partial != length) {
			US_DEBUGP("-- short transfer\n");
			return USB_STOR_XFER_SHORT;
		}

		US_DEBUGP("-- transfer complete\n");
		return USB_STOR_XFER_GOOD;

	/* stalled */
	case -EPIPE:
		/* for control endpoints, (used by CB[I]) a stall indicates
		 * a failed command */
		if (usb_pipecontrol(pipe)) {
			US_DEBUGP("-- stall on control pipe\n");
			return USB_STOR_XFER_STALLED;
		}

		/* for other sorts of endpoint, clear the stall */
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);
		if (usb_stor_clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;

	/* babble - the device tried to send more than we wanted to read */
	case -EOVERFLOW:
		US_DEBUGP("-- babble\n");
		return USB_STOR_XFER_LONG;

	/* the transfer was cancelled by abort, disconnect, or timeout */
	case -ECONNRESET:
		US_DEBUGP("-- transfer cancelled\n");
		return USB_STOR_XFER_ERROR;

	/* short scatter-gather read transfer */
	case -EREMOTEIO:
		US_DEBUGP("-- short read transfer\n");
		return USB_STOR_XFER_SHORT;

	/* abort or disconnect in progress */
	case -EIO:
		US_DEBUGP("-- abort or disconnect in progress\n");
		return USB_STOR_XFER_ERROR;

	/* the catch-all error case */
	default:
		US_DEBUGP("-- unknown error\n");
		return USB_STOR_XFER_ERROR;
	}
}

/*
 * Transfer one control message, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.
 */
int usb_stor_ctrl_transfer(struct us_data *us, unsigned int pipe,
		u8 request, u8 requesttype, u16 value, u16 index,
		void *data, u16 size)
{
	int result;

	US_DEBUGP("%s: rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
			__func__, request, requesttype,
			value, index, size);

	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, size, result,
			us->current_urb->actual_length);
}
EXPORT_SYMBOL_GPL(usb_stor_ctrl_transfer);

/*
 * Receive one interrupt buffer, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.
 *
 * This routine always uses us->recv_intr_pipe as the pipe and
 * us->ep_bInterval as the interrupt interval.
 */
static int usb_stor_intr_transfer(struct us_data *us, void *buf,
				  unsigned int length)
{
	int result;
	unsigned int pipe = us->recv_intr_pipe;
	unsigned int maxp;

	US_DEBUGP("%s: xfer %u bytes\n", __func__, length);

	/* calculate the max packet size */
	maxp = usb_maxpacket(us->pusb_dev, pipe, usb_pipeout(pipe));
	if (maxp > length)
		maxp = length;

	/* fill and submit the URB */
	usb_fill_int_urb(us->current_urb, us->pusb_dev, pipe, buf,
			maxp, usb_stor_blocking_completion, NULL,
			us->ep_bInterval);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, length, result,
			us->current_urb->actual_length);
}

/*
 * Transfer one buffer via bulk pipe, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.  If the bulk pipe
 * stalls during the transfer, the halt is automatically cleared.
 */
int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;

	US_DEBUGP("%s: xfer %u bytes\n", __func__, length);

	/* fill and submit the URB */
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf, length,
		      usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	/* store the actual length of the data transferred */
	if (act_len)
		*act_len = us->current_urb->actual_length;
	return interpret_urb_result(us, pipe, length, result, 
			us->current_urb->actual_length);
}
EXPORT_SYMBOL_GPL(usb_stor_bulk_transfer_buf);

/*
 * Transfer a scatter-gather list via bulk transfer
 *
 * This function does basically the same thing as usb_stor_bulk_transfer_buf()
 * above, but it uses the usbcore scatter-gather library.
 */
static int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len)
{
	int result;

	/* don't submit s-g requests during abort processing */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return USB_STOR_XFER_ERROR;

	/* initialize the scatter-gather request block */
	US_DEBUGP("%s: xfer %u bytes, %d entries\n", __func__,
			length, num_sg);
	result = usb_sg_init(&us->current_sg, us->pusb_dev, pipe, 0,
			sg, num_sg, length, GFP_NOIO);
	if (result) {
		US_DEBUGP("usb_sg_init returned %d\n", result);
		return USB_STOR_XFER_ERROR;
	}

	/* since the block has been initialized successfully, it's now
	 * okay to cancel it */
	set_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	/* did an abort occur during the submission? */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {

		/* cancel the request, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
			US_DEBUGP("-- cancelling sg request\n");
			usb_sg_cancel(&us->current_sg);
		}
	}

	/* wait for the completion of the transfer */
	usb_sg_wait(&us->current_sg);
	clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	result = us->current_sg.status;
	if (act_len)
		*act_len = us->current_sg.bytes;
	return interpret_urb_result(us, pipe, length, result,
			us->current_sg.bytes);
}

/*
 * Common used function. Transfer a complete command
 * via usb_stor_bulk_transfer_sglist() above. Set cmnd resid
 */
int usb_stor_bulk_srb(struct us_data* us, unsigned int pipe,
		      struct scsi_cmnd* srb)
{
	unsigned int partial;
	int result = usb_stor_bulk_transfer_sglist(us, pipe, scsi_sglist(srb),
				      scsi_sg_count(srb), scsi_bufflen(srb),
				      &partial);

	scsi_set_resid(srb, scsi_bufflen(srb) - partial);
	return result;
}
EXPORT_SYMBOL_GPL(usb_stor_bulk_srb);

/*
 * Transfer an entire SCSI command's worth of data payload over the bulk
 * pipe.
 *
 * Note that this uses usb_stor_bulk_transfer_buf() and
 * usb_stor_bulk_transfer_sglist() to achieve its goals --
 * this function simply determines whether we're going to use
 * scatter-gather or not, and acts appropriately.
 */
int usb_stor_bulk_transfer_sg(struct us_data* us, unsigned int pipe,
		void *buf, unsigned int length_left, int use_sg, int *residual)
{
	int result;
	unsigned int partial;

	/* are we scatter-gathering? */
	if (use_sg) {
		/* use the usb core scatter-gather primitives */
		result = usb_stor_bulk_transfer_sglist(us, pipe,
				(struct scatterlist *) buf, use_sg,
				length_left, &partial);
		length_left -= partial;
	} else {
		/* no scatter-gather, just make the request */
		result = usb_stor_bulk_transfer_buf(us, pipe, buf, 
				length_left, &partial);
		length_left -= partial;
	}

	/* store the residual and return the error code */
	if (residual)
		*residual = length_left;
	return result;
}
EXPORT_SYMBOL_GPL(usb_stor_bulk_transfer_sg);

/***********************************************************************
 * Transport routines
 ***********************************************************************/

/* There are so many devices that report the capacity incorrectly,
 * this routine was written to counteract some of the resulting
 * problems.
 */
static void last_sector_hacks(struct us_data *us, struct scsi_cmnd *srb)
{
	struct gendisk *disk;
	struct scsi_disk *sdkp;
	u32 sector;

	/* To Report "Medium Error: Record Not Found */
	static unsigned char record_not_found[18] = {
		[0]	= 0x70,			/* current error */
		[2]	= MEDIUM_ERROR,		/* = 0x03 */
		[7]	= 0x0a,			/* additional length */
		[12]	= 0x14			/* Record Not Found */
	};

	/* If last-sector problems can't occur, whether because the
	 * capacity was already decremented or because the device is
	 * known to report the correct capacity, then we don't need
	 * to do anything.
	 */
	if (!us->use_last_sector_hacks)
		return;

	/* Was this command a READ(10) or a WRITE(10)? */
	if (srb->cmnd[0] != READ_10 && srb->cmnd[0] != WRITE_10)
		goto done;

	/* Did this command access the last sector? */
	sector = (srb->cmnd[2] << 24) | (srb->cmnd[3] << 16) |
			(srb->cmnd[4] << 8) | (srb->cmnd[5]);
	disk = srb->request->rq_disk;
	if (!disk)
		goto done;
	sdkp = scsi_disk(disk);
	if (!sdkp)
		goto done;
	if (sector + 1 != sdkp->capacity)
		goto done;

	if (srb->result == SAM_STAT_GOOD && scsi_get_resid(srb) == 0) {

		/* The command succeeded.  We know this device doesn't
		 * have the last-sector bug, so stop checking it.
		 */
		us->use_last_sector_hacks = 0;

	} else {
		/* The command failed.  Allow up to 3 retries in case this
		 * is some normal sort of failure.  After that, assume the
		 * capacity is wrong and we're trying to access the sector
		 * beyond the end.  Replace the result code and sense data
		 * with values that will cause the SCSI core to fail the
		 * command immediately, instead of going into an infinite
		 * (or even just a very long) retry loop.
		 */
		if (++us->last_sector_retries < 3)
			return;
		srb->result = SAM_STAT_CHECK_CONDITION;
		memcpy(srb->sense_buffer, record_not_found,
				sizeof(record_not_found));
	}

 done:
	/* Don't reset the retry counter for TEST UNIT READY commands,
	 * because they get issued after device resets which might be
	 * caused by a failed last-sector access.
	 */
	if (srb->cmnd[0] != TEST_UNIT_READY)
		us->last_sector_retries = 0;
}

/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	/* send the command to the transport layer */
	scsi_set_resid(srb, 0);
	result = us->transport(srb, us);

	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
		US_DEBUGP("-- command was aborted\n");
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		US_DEBUGP("-- transport indicates error, resetting\n");
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	/* if the transport provided its own sense data, don't auto-sense */
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		last_sector_hacks(us, srb);
		return;
	}

	srb->result = SAM_STAT_GOOD;

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;

	/*
	 * If we're running the CB transport, which is incapable
	 * of determining status on its own, we will auto-sense
	 * unless the operation involved a data-in transfer.  Devices
	 * can signal most data-in errors by stalling the bulk-in pipe.
	 */
	if ((us->protocol == US_PR_CB || us->protocol == US_PR_DPCM_USB) &&
			srb->sc_data_direction != DMA_FROM_DEVICE) {
		US_DEBUGP("-- CB transport device requiring auto-sense\n");
		need_auto_sense = 1;
	}

	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
		US_DEBUGP("-- transport indicates command failure\n");
		need_auto_sense = 1;
	}

	/*
	 * Determine if this device is SAT by seeing if the
	 * command executed successfully.  Otherwise we'll have
	 * to wait for at least one CHECK_CONDITION to determine
	 * SANE_SENSE support
	 */
	if (unlikely((srb->cmnd[0] == ATA_16 || srb->cmnd[0] == ATA_12) &&
	    result == USB_STOR_TRANSPORT_GOOD &&
	    !(us->fflags & US_FL_SANE_SENSE) &&
	    !(us->fflags & US_FL_BAD_SENSE) &&
	    !(srb->cmnd[2] & 0x20))) {
		US_DEBUGP("-- SAT supported, increasing auto-sense\n");
		us->fflags |= US_FL_SANE_SENSE;
	}

	/*
	 * A short transfer on a command where we don't expect it
	 * is unusual, but it doesn't mean we need to auto-sense.
	 */
	if ((scsi_get_resid(srb) > 0) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
		US_DEBUGP("-- unexpectedly short transfer\n");
	}

	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		struct scsi_eh_save ses;
		int sense_size = US_SENSE_SIZE;

		/* device supports and needs bigger sense buffer */
		if (us->fflags & US_FL_SANE_SENSE)
			sense_size = ~0;
Retry_Sense:
		US_DEBUGP("Issuing auto-REQUEST_SENSE\n");

		scsi_eh_prep_cmnd(srb, &ses, NULL, 0, sense_size);

		if (us->subclass == US_SC_RBC || us->subclass == US_SC_SCSI ||
				us->subclass == US_SC_CYP_ATACB)
			srb->cmd_len = 6;
		else
			srb->cmd_len = 12;

		/* issue the auto-sense command */
		scsi_set_resid(srb, 0);
		temp_result = us->transport(us->srb, us);

		/* let's clean up right away */
		scsi_eh_restore_cmnd(srb, &ses);

		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			US_DEBUGP("-- auto-sense aborted\n");
			srb->result = DID_ABORT << 16;

			/* If SANE_SENSE caused this problem, disable it */
			if (sense_size != US_SENSE_SIZE) {
				us->fflags &= ~US_FL_SANE_SENSE;
				us->fflags |= US_FL_BAD_SENSE;
			}
			goto Handle_Errors;
		}

		/* Some devices claim to support larger sense but fail when
		 * trying to request it. When a transport failure happens
		 * using US_FS_SANE_SENSE, we always retry with a standard
		 * (small) sense request. This fixes some USB GSM modems
		 */
		if (temp_result == USB_STOR_TRANSPORT_FAILED &&
				sense_size != US_SENSE_SIZE) {
			US_DEBUGP("-- auto-sense failure, retry small sense\n");
			sense_size = US_SENSE_SIZE;
			us->fflags &= ~US_FL_SANE_SENSE;
			us->fflags |= US_FL_BAD_SENSE;
			goto Retry_Sense;
		}

		/* Other failures */
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			US_DEBUGP("-- auto-sense failure\n");

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			srb->result = DID_ERROR << 16;
			if (!(us->fflags & US_FL_SCM_MULT_TARG))
				goto Handle_Errors;
			return;
		}

		/* If the sense data returned is larger than 18-bytes then we
		 * assume this device supports requesting more in the future.
		 * The response code must be 70h through 73h inclusive.
		 */
		if (srb->sense_buffer[7] > (US_SENSE_SIZE - 8) &&
		    !(us->fflags & US_FL_SANE_SENSE) &&
		    !(us->fflags & US_FL_BAD_SENSE) &&
		    (srb->sense_buffer[0] & 0x7C) == 0x70) {
			US_DEBUGP("-- SANE_SENSE support enabled\n");
			us->fflags |= US_FL_SANE_SENSE;

			/* Indicate to the user that we truncated their sense
			 * because we didn't know it supported larger sense.
			 */
			US_DEBUGP("-- Sense data truncated to %i from %i\n",
			          US_SENSE_SIZE,
			          srb->sense_buffer[7] + 8);
			srb->sense_buffer[7] = (US_SENSE_SIZE - 8);
		}

		US_DEBUGP("-- Result from auto-sense is %d\n", temp_result);
		US_DEBUGP("-- code: 0x%x, key: 0x%x, ASC: 0x%x, ASCQ: 0x%x\n",
			  srb->sense_buffer[0],
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#ifdef CONFIG_USB_STORAGE_DEBUG
		usb_stor_show_sense(
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#endif

		/* set the result so the higher layers expect this data */
		srb->result = SAM_STAT_CHECK_CONDITION;

		/* We often get empty sense data.  This could indicate that
		 * everything worked or that there was an unspecified
		 * problem.  We have to decide which.
		 */
		if (	/* Filemark 0, ignore EOM, ILI 0, no sense */
				(srb->sense_buffer[2] & 0xaf) == 0 &&
			/* No ASC or ASCQ */
				srb->sense_buffer[12] == 0 &&
				srb->sense_buffer[13] == 0) {

			/* If things are really okay, then let's show that.
			 * Zero out the sense buffer so the higher layers
			 * won't realize we did an unsolicited auto-sense.
			 */
			if (result == USB_STOR_TRANSPORT_GOOD) {
				srb->result = SAM_STAT_GOOD;
				srb->sense_buffer[0] = 0x0;

			/* If there was a problem, report an unspecified
			 * hardware error to prevent the higher layers from
			 * entering an infinite retry loop.
			 */
			} else {
				srb->result = DID_ERROR << 16;
				srb->sense_buffer[2] = HARDWARE_ERROR;
			}
		}
	}

	/* Did we transfer less than the minimum amount required? */
	if ((srb->result == SAM_STAT_GOOD || srb->sense_buffer[2] == 0) &&
			scsi_bufflen(srb) - scsi_get_resid(srb) < srb->underflow)
		srb->result = DID_ERROR << 16;

	last_sector_hacks(us, srb);
	return;

	/* Error and abort processing: try to resynchronize with the device
	 * by issuing a port reset.  If that fails, try a class-specific
	 * device reset. */
  Handle_Errors:

	/* Set the RESETTING bit, and clear the ABORTING bit so that
	 * the reset may proceed. */
	scsi_lock(us_to_host(us));
	set_bit(US_FLIDX_RESETTING, &us->dflags);
	clear_bit(US_FLIDX_ABORTING, &us->dflags);
	scsi_unlock(us_to_host(us));

	/* We must release the device lock because the pre_reset routine
	 * will want to acquire it. */
	mutex_unlock(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	mutex_lock(&us->dev_mutex);

	if (result < 0) {
		scsi_lock(us_to_host(us));
		usb_stor_report_device_reset(us);
		scsi_unlock(us_to_host(us));
		us->transport_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->dflags);
	last_sector_hacks(us, srb);
}

/* Stop the current URB transfer */
void usb_stor_stop_transport(struct us_data *us)
{
	US_DEBUGP("%s called\n", __func__);

	/* If the state machine is blocked waiting for an URB,
	 * let's wake it up.  The test_and_clear_bit() call
	 * guarantees that if a URB has just been submitted,
	 * it won't be cancelled more than once. */
	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
		US_DEBUGP("-- cancelling URB\n");
		usb_unlink_urb(us->current_urb);
	}

	/* If we are waiting for a scatter-gather operation, cancel it. */
	if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
		US_DEBUGP("-- cancelling sg request\n");
		usb_sg_cancel(&us->current_sg);
	}
}

/*
 * Control/Bulk and Control/Bulk/Interrupt transport
 */

int usb_stor_CB_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int pipe = 0;
	int result;

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_ctrl_transfer(us, us->send_ctrl_pipe,
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	US_DEBUGP("Call to usb_stor_ctrl_transfer() returned %d\n", result);

	/* if we stalled the command, it means command failed */
	if (result == USB_STOR_XFER_STALLED) {
		return USB_STOR_TRANSPORT_FAILED;
	}

	/* Uh oh... serious problem here */
	if (result != USB_STOR_XFER_GOOD) {
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (transfer_length) {
		pipe = srb->sc_data_direction == DMA_FROM_DEVICE ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_srb(us, pipe, srb);
		US_DEBUGP("CBI data stage result is 0x%x\n", result);

		/* if we stalled the data transfer it means command failed */
		if (result == USB_STOR_XFER_STALLED)
			return USB_STOR_TRANSPORT_FAILED;
		if (result > USB_STOR_XFER_STALLED)
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* STATUS STAGE */

	/* NOTE: CB does not have a status stage.  Silly, I know.  So
	 * we have to catch this at a higher level.
	 */
	if (us->protocol != US_PR_CBI)
		return USB_STOR_TRANSPORT_GOOD;

	result = usb_stor_intr_transfer(us, us->iobuf, 2);
	US_DEBUGP("Got interrupt data (0x%x, 0x%x)\n", 
			us->iobuf[0], us->iobuf[1]);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* UFI gives us ASC and ASCQ, like a request sense
	 *
	 * REQUEST_SENSE and INQUIRY don't affect the sense data on UFI
	 * devices, so we ignore the information for those commands.  Note
	 * that this means we could be ignoring a real error on these
	 * commands, but that can't be helped.
	 */
	if (us->subclass == US_SC_UFI) {
		if (srb->cmnd[0] == REQUEST_SENSE ||
		    srb->cmnd[0] == INQUIRY)
			return USB_STOR_TRANSPORT_GOOD;
		if (us->iobuf[0])
			goto Failed;
		return USB_STOR_TRANSPORT_GOOD;
	}

	/* If not UFI, we interpret the data as a result code 
	 * The first byte should always be a 0x0.
	 *
	 * Some bogus devices don't follow that rule.  They stuff the ASC
	 * into the first byte -- so if it's non-zero, call it a failure.
	 */
	if (us->iobuf[0]) {
		US_DEBUGP("CBI IRQ data showed reserved bType 0x%x\n",
				us->iobuf[0]);
		goto Failed;

	}

	/* The second byte & 0x0F should be 0x0 for good, otherwise error */
	switch (us->iobuf[1] & 0x0F) {
		case 0x00: 
			return USB_STOR_TRANSPORT_GOOD;
		case 0x01: 
			goto Failed;
	}
	return USB_STOR_TRANSPORT_ERROR;

	/* the CBI spec requires that the bulk pipe must be cleared
	 * following any data-in/out command failure (section 2.4.3.1.3)
	 */
  Failed:
	if (pipe)
		usb_stor_clear_halt(us, pipe);
	return USB_STOR_TRANSPORT_FAILED;
}
EXPORT_SYMBOL_GPL(usb_stor_CB_transport);

/*
 * Bulk only transport
 */

/* Determine what the maximum LUN supported is */
int usb_stor_Bulk_max_lun(struct us_data *us)
{
	int result;

	/* issue the command */
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, 10*HZ);

	US_DEBUGP("GetMaxLUN command result is %d, data is %d\n", 
		  result, us->iobuf[0]);

	/* if we have a successful request, return the result */
	if (result > 0)
		return us->iobuf[0];

	/*
	 * Some devices don't like GetMaxLUN.  They may STALL the control
	 * pipe, they may return a zero-length result, they may do nothing at
	 * all and timeout, or they may fail in even more bizarrely creative
	 * ways.  In these cases the best approach is to use the default
	 * value: only one LUN.
	 */
	return 0;
}

int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int residue;
	int result;
	int fake_sense = 0;
	unsigned int cswlen;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;
#ifdef CONFIG_BCM47XX
	int retry = csw_retry;
#endif /* CONFIG_BCM47XX */

	/* Take care of BULK32 devices; set extra byte to 0 */
	if (unlikely(us->fflags & US_FL_BULK32)) {
		cbwlen = 32;
		us->iobuf[31] = 0;
	}

	/* set up the command wrapper */
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	bcb->Flags = srb->sc_data_direction == DMA_FROM_DEVICE ? 1 << 7 : 0;
	bcb->Tag = ++us->tag;
	bcb->Lun = srb->device->lun;
	if (us->fflags & US_FL_SCM_MULT_TARG)
		bcb->Lun |= srb->device->id << 4;
	bcb->Length = srb->cmd_len;

	/* copy the command payload */
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	/* send it to out endpoint */
	US_DEBUGP("Bulk Command S 0x%x T 0x%x L %d F %d Trg %d LUN %d CL %d\n",
			le32_to_cpu(bcb->Signature), bcb->Tag,
			le32_to_cpu(bcb->DataTransferLength), bcb->Flags,
			(bcb->Lun >> 4), (bcb->Lun & 0x0F), 
			bcb->Length);
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				bcb, cbwlen, NULL);
	US_DEBUGP("Bulk command transfer result=%d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* DATA STAGE */
	/* send/receive data payload, if there is any */

	/* Some USB-IDE converter chips need a 100us delay between the
	 * command phase and the data phase.  Some devices need a little
	 * more than that, probably because of clock rate inaccuracies. */
	if (unlikely(us->fflags & US_FL_GO_SLOW))
		udelay(125);

	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == DMA_FROM_DEVICE ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_srb(us, pipe, srb);
		US_DEBUGP("Bulk data transfer result 0x%x\n", result);
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;

		/* If the device tried to send back more data than the
		 * amount requested, the spec requires us to transfer
		 * the CSW anyway.  Since there's no point retrying the
		 * the command, we'll return fake sense data indicating
		 * Illegal Request, Invalid Field in CDB.
		 */
		if (result == USB_STOR_XFER_LONG)
			fake_sense = 1;
	}

	/* See flow chart on pg 15 of the Bulk Only Transport spec for
	 * an explanation of how this code works.
	 */

#ifdef CONFIG_BCM47XX
	memset(bcs, 0, sizeof(struct bulk_cs_wrap));
#endif /* CONFIG_BCM47XX */

	/* get CSW for device status */
	US_DEBUGP("Attempting to get CSW...\n");
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);

	/* Some broken devices add unnecessary zero-length packets to the
	 * end of their data transfers.  Such packets show up as 0-length
	 * CSWs.  If we encounter such a thing, try to read the CSW again.
	 */
	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		US_DEBUGP("Received 0-length CSW; retrying...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);
	}

	/* did the attempt to read the CSW fail? */
	if (result == USB_STOR_XFER_STALLED) {

		/* get the status again */
		US_DEBUGP("Attempting to get CSW (2nd try)...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	/* if we still have a failure at this point, we're in trouble */
	US_DEBUGP("Bulk status result = %d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

#ifdef CONFIG_BCM47XX
	while (retry && (bcs->Signature == 0) && (cswlen == US_BULK_CS_WRAP_LEN)) {
		retry--;
		mdelay(1);
	}

	if (retry != csw_retry) {
		US_DEBUGP("retry = %d\n", retry);

		if (retry == 0) {
			US_DEBUGP("CSW: us->S 0x%x us->T 0x%x S 0x%x T 0x%x R %u Stat 0x%x\n",
				le32_to_cpu(us->bcs_signature), us->tag,
				le32_to_cpu(bcs->Signature), bcs->Tag,
				residue, bcs->Status);
			return USB_STOR_TRANSPORT_GOOD;
		}
	}
#endif /* CONFIG_BCM47XX */

	/* check bulk status */
	residue = le32_to_cpu(bcs->Residue);
	US_DEBUGP("Bulk Status S 0x%x T 0x%x R %u Stat 0x%x\n",
			le32_to_cpu(bcs->Signature), bcs->Tag, 
			residue, bcs->Status);
	if (!(bcs->Tag == us->tag || (us->fflags & US_FL_BULK_IGNORE_TAG)) ||
		bcs->Status > US_BULK_STAT_PHASE) {
		US_DEBUGP("Bulk logical error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Some broken devices report odd signatures, so we do not check them
	 * for validity against the spec. We store the first one we see,
	 * and check subsequent transfers for validity against this signature.
	 */
	if (!us->bcs_signature) {
		us->bcs_signature = bcs->Signature;
		if (us->bcs_signature != cpu_to_le32(US_BULK_CS_SIGN))
			US_DEBUGP("Learnt BCS signature 0x%08X\n",
					le32_to_cpu(us->bcs_signature));
	} else if (bcs->Signature != us->bcs_signature) {
		US_DEBUGP("Signature mismatch: got %08X, expecting %08X\n",
			  le32_to_cpu(bcs->Signature),
			  le32_to_cpu(us->bcs_signature));
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* try to compute the actual residue, based on how much data
	 * was really transferred and what the device tells us */
	if (residue && !(us->fflags & US_FL_IGNORE_RESIDUE)) {

		/* Heuristically detect devices that generate bogus residues
		 * by seeing what happens with INQUIRY and READ CAPACITY
		 * commands.
		 */
		if (bcs->Status == US_BULK_STAT_OK &&
				scsi_get_resid(srb) == 0 &&
					((srb->cmnd[0] == INQUIRY &&
						transfer_length == 36) ||
					(srb->cmnd[0] == READ_CAPACITY &&
						transfer_length == 8))) {
			us->fflags |= US_FL_IGNORE_RESIDUE;

		} else {
			residue = min(residue, transfer_length);
			scsi_set_resid(srb, max(scsi_get_resid(srb),
			                                       (int) residue));
		}
	}

	/* based on the status code, we report good or bad */
	switch (bcs->Status) {
		case US_BULK_STAT_OK:
			/* device babbled -- return fake sense data */
			if (fake_sense) {
				memcpy(srb->sense_buffer, 
				       usb_stor_sense_invalidCDB, 
				       sizeof(usb_stor_sense_invalidCDB));
				return USB_STOR_TRANSPORT_NO_SENSE;
			}

			/* command good -- note that data could be short */
			return USB_STOR_TRANSPORT_GOOD;

		case US_BULK_STAT_FAIL:
			/* command failed */
			return USB_STOR_TRANSPORT_FAILED;

		case US_BULK_STAT_PHASE:
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}
EXPORT_SYMBOL_GPL(usb_stor_Bulk_transport);

/***********************************************************************
 * Reset routines
 ***********************************************************************/

/* This is the common part of the device reset code.
 *
 * It's handy that every transport mechanism uses the control endpoint for
 * resets.
 *
 * Basically, we send a reset with a 5-second timeout, so we don't get
 * jammed attempting to do the reset.
 */
static int usb_stor_reset_common(struct us_data *us,
		u8 request, u8 requesttype,
		u16 value, u16 index, void *data, u16 size)
{
	int result;
	int result2;

	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		US_DEBUGP("No reset during disconnect\n");
		return -EIO;
	}

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			request, requesttype, value, index, data, size,
			5*HZ);
	if (result < 0) {
		US_DEBUGP("Soft reset failed: %d\n", result);
		return result;
	}

	/* Give the device some time to recover from the reset,
	 * but don't delay disconnect processing. */
	wait_event_interruptible_timeout(us->delay_wait,
			test_bit(US_FLIDX_DISCONNECTING, &us->dflags),
			HZ*6);
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		US_DEBUGP("Reset interrupted by disconnect\n");
		return -EIO;
	}

	US_DEBUGP("Soft reset: clearing bulk-in endpoint halt\n");
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	US_DEBUGP("Soft reset: clearing bulk-out endpoint halt\n");
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	/* return a result code based on the result of the clear-halts */
	if (result >= 0)
		result = result2;
	if (result < 0)
		US_DEBUGP("Soft reset failed\n");
	else
		US_DEBUGP("Soft reset done\n");
	return result;
}

/* This issues a CB[I] Reset to the device in question
 */
#define CB_RESET_CMD_SIZE	12

int usb_stor_CB_reset(struct us_data *us)
{
	US_DEBUGP("%s called\n", __func__);

	memset(us->iobuf, 0xFF, CB_RESET_CMD_SIZE);
	us->iobuf[0] = SEND_DIAGNOSTIC;
	us->iobuf[1] = 4;
	return usb_stor_reset_common(us, US_CBI_ADSC, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, CB_RESET_CMD_SIZE);
}
EXPORT_SYMBOL_GPL(usb_stor_CB_reset);

/* This issues a Bulk-only Reset to the device in question, including
 * clearing the subsequent endpoint halts that may occur.
 */
int usb_stor_Bulk_reset(struct us_data *us)
{
	US_DEBUGP("%s called\n", __func__);

	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}
EXPORT_SYMBOL_GPL(usb_stor_Bulk_reset);

/* Issue a USB port reset to the device.  The caller must not hold
 * us->dev_mutex.
 */
int usb_stor_port_reset(struct us_data *us)
{
	int result;

	/*for these devices we must use the class specific method */
	if (us->pusb_dev->quirks & USB_QUIRK_RESET_MORPHS)
		return -EPERM;

	result = usb_lock_device_for_reset(us->pusb_dev, us->pusb_intf);
	if (result < 0)
		US_DEBUGP("unable to lock device for reset: %d\n", result);
	else {
		/* Were we disconnected while waiting for the lock? */
		if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
			result = -EIO;
			US_DEBUGP("No reset during disconnect\n");
		} else {
			result = usb_reset_device(us->pusb_dev);
			US_DEBUGP("usb_reset_device returns %d\n",
					result);
		}
		usb_unlock_device(us->pusb_dev);
	}
	return result;
}
