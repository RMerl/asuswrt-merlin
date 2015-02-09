/*
   cx231xx-video.c - driver for Conexant Cx23100/101/102
		     USB video capture devices

   Copyright (C) 2008 <srinivasa.deevi at conexant dot com>
	Based on em28xx driver
	Based on cx23885 driver
	Based on cx88 driver

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

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/usb.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-chip-ident.h>
#include <media/msp3400.h>
#include <media/tuner.h>

#include "dvb_frontend.h"

#include "cx231xx.h"
#include "cx231xx-vbi.h"

#define CX231XX_VERSION_CODE            KERNEL_VERSION(0, 0, 1)

#define DRIVER_AUTHOR   "Srinivasa Deevi <srinivasa.deevi@conexant.com>"
#define DRIVER_DESC     "Conexant cx231xx based USB video device driver"

#define cx231xx_videodbg(fmt, arg...) do {\
	if (video_debug) \
		printk(KERN_INFO "%s %s :"fmt, \
			 dev->name, __func__ , ##arg); } while (0)

static unsigned int isoc_debug;
module_param(isoc_debug, int, 0644);
MODULE_PARM_DESC(isoc_debug, "enable debug messages [isoc transfers]");

#define cx231xx_isocdbg(fmt, arg...) \
do {\
	if (isoc_debug) { \
		printk(KERN_INFO "%s %s :"fmt, \
			 dev->name, __func__ , ##arg); \
	} \
  } while (0)

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static unsigned int card[]     = {[0 ... (CX231XX_MAXBOARDS - 1)] = UNSET };
static unsigned int video_nr[] = {[0 ... (CX231XX_MAXBOARDS - 1)] = UNSET };
static unsigned int vbi_nr[]   = {[0 ... (CX231XX_MAXBOARDS - 1)] = UNSET };
static unsigned int radio_nr[] = {[0 ... (CX231XX_MAXBOARDS - 1)] = UNSET };

module_param_array(card, int, NULL, 0444);
module_param_array(video_nr, int, NULL, 0444);
module_param_array(vbi_nr, int, NULL, 0444);
module_param_array(radio_nr, int, NULL, 0444);

MODULE_PARM_DESC(card, "card type");
MODULE_PARM_DESC(video_nr, "video device numbers");
MODULE_PARM_DESC(vbi_nr, "vbi device numbers");
MODULE_PARM_DESC(radio_nr, "radio device numbers");

static unsigned int video_debug;
module_param(video_debug, int, 0644);
MODULE_PARM_DESC(video_debug, "enable debug messages [video]");

/* supported video standards */
static struct cx231xx_fmt format[] = {
	{
	 .name = "16bpp YUY2, 4:2:2, packed",
	 .fourcc = V4L2_PIX_FMT_YUYV,
	 .depth = 16,
	 .reg = 0,
	 },
};

/* supported controls */
/* Common to all boards */

/* ------------------------------------------------------------------- */

static const struct v4l2_queryctrl no_ctl = {
	.name = "42",
	.flags = V4L2_CTRL_FLAG_DISABLED,
};

static struct cx231xx_ctrl cx231xx_ctls[] = {
	/* --- video --- */
	{
		.v = {
			.id = V4L2_CID_BRIGHTNESS,
			.name = "Brightness",
			.minimum = 0x00,
			.maximum = 0xff,
			.step = 1,
			.default_value = 0x7f,
			.type = V4L2_CTRL_TYPE_INTEGER,
		},
		.off = 128,
		.reg = LUMA_CTRL,
		.mask = 0x00ff,
		.shift = 0,
	}, {
		.v = {
			.id = V4L2_CID_CONTRAST,
			.name = "Contrast",
			.minimum = 0,
			.maximum = 0xff,
			.step = 1,
			.default_value = 0x3f,
			.type = V4L2_CTRL_TYPE_INTEGER,
		},
		.off = 0,
		.reg = LUMA_CTRL,
		.mask = 0xff00,
		.shift = 8,
	}, {
		.v = {
			.id = V4L2_CID_HUE,
			.name = "Hue",
			.minimum = 0,
			.maximum = 0xff,
			.step = 1,
			.default_value = 0x7f,
			.type = V4L2_CTRL_TYPE_INTEGER,
		},
		.off = 128,
		.reg = CHROMA_CTRL,
		.mask = 0xff0000,
		.shift = 16,
	}, {
	/* strictly, this only describes only U saturation.
	* V saturation is handled specially through code.
	*/
		.v = {
			.id = V4L2_CID_SATURATION,
			.name = "Saturation",
			.minimum = 0,
			.maximum = 0xff,
			.step = 1,
			.default_value = 0x7f,
			.type = V4L2_CTRL_TYPE_INTEGER,
		},
		.off = 0,
		.reg = CHROMA_CTRL,
		.mask = 0x00ff,
		.shift = 0,
	}, {
		/* --- audio --- */
		.v = {
			.id = V4L2_CID_AUDIO_MUTE,
			.name = "Mute",
			.minimum = 0,
			.maximum = 1,
			.default_value = 1,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
		},
		.reg = PATH1_CTL1,
		.mask = (0x1f << 24),
		.shift = 24,
	}, {
		.v = {
			.id = V4L2_CID_AUDIO_VOLUME,
			.name = "Volume",
			.minimum = 0,
			.maximum = 0x3f,
			.step = 1,
			.default_value = 0x3f,
			.type = V4L2_CTRL_TYPE_INTEGER,
		},
		.reg = PATH1_VOL_CTL,
		.mask = 0xff,
		.shift = 0,
	}
};
static const int CX231XX_CTLS = ARRAY_SIZE(cx231xx_ctls);

static const u32 cx231xx_user_ctrls[] = {
	V4L2_CID_USER_CLASS,
	V4L2_CID_BRIGHTNESS,
	V4L2_CID_CONTRAST,
	V4L2_CID_SATURATION,
	V4L2_CID_HUE,
	V4L2_CID_AUDIO_VOLUME,
	V4L2_CID_AUDIO_MUTE,
	0
};

static const u32 *ctrl_classes[] = {
	cx231xx_user_ctrls,
	NULL
};

/* ------------------------------------------------------------------
	Video buffer and parser functions
   ------------------------------------------------------------------*/

/*
 * Announces that a buffer were filled and request the next
 */
static inline void buffer_filled(struct cx231xx *dev,
				 struct cx231xx_dmaqueue *dma_q,
				 struct cx231xx_buffer *buf)
{
	/* Advice that buffer was filled */
	cx231xx_isocdbg("[%p/%d] wakeup\n", buf, buf->vb.i);
	buf->vb.state = VIDEOBUF_DONE;
	buf->vb.field_count++;
	do_gettimeofday(&buf->vb.ts);

	dev->video_mode.isoc_ctl.buf = NULL;

	list_del(&buf->vb.queue);
	wake_up(&buf->vb.done);
}

static inline void print_err_status(struct cx231xx *dev, int packet, int status)
{
	char *errmsg = "Unknown";

	switch (status) {
	case -ENOENT:
		errmsg = "unlinked synchronuously";
		break;
	case -ECONNRESET:
		errmsg = "unlinked asynchronuously";
		break;
	case -ENOSR:
		errmsg = "Buffer error (overrun)";
		break;
	case -EPIPE:
		errmsg = "Stalled (device not responding)";
		break;
	case -EOVERFLOW:
		errmsg = "Babble (bad cable?)";
		break;
	case -EPROTO:
		errmsg = "Bit-stuff error (bad cable?)";
		break;
	case -EILSEQ:
		errmsg = "CRC/Timeout (could be anything)";
		break;
	case -ETIME:
		errmsg = "Device does not respond";
		break;
	}
	if (packet < 0) {
		cx231xx_isocdbg("URB status %d [%s].\n", status, errmsg);
	} else {
		cx231xx_isocdbg("URB packet %d, status %d [%s].\n",
				packet, status, errmsg);
	}
}

/*
 * video-buf generic routine to get the next available buffer
 */
static inline void get_next_buf(struct cx231xx_dmaqueue *dma_q,
				struct cx231xx_buffer **buf)
{
	struct cx231xx_video_mode *vmode =
	    container_of(dma_q, struct cx231xx_video_mode, vidq);
	struct cx231xx *dev = container_of(vmode, struct cx231xx, video_mode);

	char *outp;

	if (list_empty(&dma_q->active)) {
		cx231xx_isocdbg("No active queue to serve\n");
		dev->video_mode.isoc_ctl.buf = NULL;
		*buf = NULL;
		return;
	}

	/* Get the next buffer */
	*buf = list_entry(dma_q->active.next, struct cx231xx_buffer, vb.queue);

	/* Cleans up buffer - Usefull for testing for frame/URB loss */
	outp = videobuf_to_vmalloc(&(*buf)->vb);
	memset(outp, 0, (*buf)->vb.size);

	dev->video_mode.isoc_ctl.buf = *buf;

	return;
}

/*
 * Controls the isoc copy of each urb packet
 */
static inline int cx231xx_isoc_copy(struct cx231xx *dev, struct urb *urb)
{
	struct cx231xx_buffer *buf;
	struct cx231xx_dmaqueue *dma_q = urb->context;
	unsigned char *outp = NULL;
	int i, rc = 1;
	unsigned char *p_buffer;
	u32 bytes_parsed = 0, buffer_size = 0;
	u8 sav_eav = 0;

	if (!dev)
		return 0;

	if ((dev->state & DEV_DISCONNECTED) || (dev->state & DEV_MISCONFIGURED))
		return 0;

	if (urb->status < 0) {
		print_err_status(dev, -1, urb->status);
		if (urb->status == -ENOENT)
			return 0;
	}

	buf = dev->video_mode.isoc_ctl.buf;
	if (buf != NULL)
		outp = videobuf_to_vmalloc(&buf->vb);

	for (i = 0; i < urb->number_of_packets; i++) {
		int status = urb->iso_frame_desc[i].status;

		if (status < 0) {
			print_err_status(dev, i, status);
			if (urb->iso_frame_desc[i].status != -EPROTO)
				continue;
		}

		if (urb->iso_frame_desc[i].actual_length <= 0) {
			/* cx231xx_isocdbg("packet %d is empty",i); - spammy */
			continue;
		}
		if (urb->iso_frame_desc[i].actual_length >
		    dev->video_mode.max_pkt_size) {
			cx231xx_isocdbg("packet bigger than packet size");
			continue;
		}

		/*  get buffer pointer and length */
		p_buffer = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		buffer_size = urb->iso_frame_desc[i].actual_length;
		bytes_parsed = 0;

		if (dma_q->is_partial_line) {
			/* Handle the case of a partial line */
			sav_eav = dma_q->last_sav;
		} else {
			/* Check for a SAV/EAV overlapping
				the buffer boundary */
			sav_eav =
			    cx231xx_find_boundary_SAV_EAV(p_buffer,
							  dma_q->partial_buf,
							  &bytes_parsed);
		}

		sav_eav &= 0xF0;
		/* Get the first line if we have some portion of an SAV/EAV from
		   the last buffer or a partial line  */
		if (sav_eav) {
			bytes_parsed += cx231xx_get_video_line(dev, dma_q,
				sav_eav,	/* SAV/EAV */
				p_buffer + bytes_parsed,	/* p_buffer */
				buffer_size - bytes_parsed);/* buf size */
		}

		/* Now parse data that is completely in this buffer */
		/* dma_q->is_partial_line = 0;  */

		while (bytes_parsed < buffer_size) {
			u32 bytes_used = 0;

			sav_eav = cx231xx_find_next_SAV_EAV(
				p_buffer + bytes_parsed,	/* p_buffer */
				buffer_size - bytes_parsed,	/* buf size */
				&bytes_used);/* bytes used to get SAV/EAV */

			bytes_parsed += bytes_used;

			sav_eav &= 0xF0;
			if (sav_eav && (bytes_parsed < buffer_size)) {
				bytes_parsed += cx231xx_get_video_line(dev,
					dma_q, sav_eav,	/* SAV/EAV */
					p_buffer + bytes_parsed,/* p_buffer */
					buffer_size - bytes_parsed);/*buf size*/
			}
		}

		/* Save the last four bytes of the buffer so we can check the
		   buffer boundary condition next time */
		memcpy(dma_q->partial_buf, p_buffer + buffer_size - 4, 4);
		bytes_parsed = 0;

	}
	return rc;
}

u8 cx231xx_find_boundary_SAV_EAV(u8 *p_buffer, u8 *partial_buf,
				 u32 *p_bytes_used)
{
	u32 bytes_used;
	u8 boundary_bytes[8];
	u8 sav_eav = 0;

	*p_bytes_used = 0;

	/* Create an array of the last 4 bytes of the last buffer and the first
	   4 bytes of the current buffer. */

	memcpy(boundary_bytes, partial_buf, 4);
	memcpy(boundary_bytes + 4, p_buffer, 4);

	/* Check for the SAV/EAV in the boundary buffer */
	sav_eav = cx231xx_find_next_SAV_EAV((u8 *)&boundary_bytes, 8,
					    &bytes_used);

	if (sav_eav) {
		/* found a boundary SAV/EAV.  Updates the bytes used to reflect
		   only those used in the new buffer */
		*p_bytes_used = bytes_used - 4;
	}

	return sav_eav;
}

u8 cx231xx_find_next_SAV_EAV(u8 *p_buffer, u32 buffer_size, u32 *p_bytes_used)
{
	u32 i;
	u8 sav_eav = 0;

	/*
	 * Don't search if the buffer size is less than 4.  It causes a page
	 * fault since buffer_size - 4 evaluates to a large number in that
	 * case.
	 */
	if (buffer_size < 4) {
		*p_bytes_used = buffer_size;
		return 0;
	}

	for (i = 0; i < (buffer_size - 3); i++) {

		if ((p_buffer[i] == 0xFF) &&
		    (p_buffer[i + 1] == 0x00) && (p_buffer[i + 2] == 0x00)) {

			*p_bytes_used = i + 4;
			sav_eav = p_buffer[i + 3];
			return sav_eav;
		}
	}

	*p_bytes_used = buffer_size;
	return 0;
}

u32 cx231xx_get_video_line(struct cx231xx *dev,
			   struct cx231xx_dmaqueue *dma_q, u8 sav_eav,
			   u8 *p_buffer, u32 buffer_size)
{
	u32 bytes_copied = 0;
	int current_field = -1;

	switch (sav_eav) {
	case SAV_ACTIVE_VIDEO_FIELD1:
		/* looking for skipped line which occurred in PAL 720x480 mode.
		   In this case, there will be no active data contained
		   between the SAV and EAV */
		if ((buffer_size > 3) && (p_buffer[0] == 0xFF) &&
		    (p_buffer[1] == 0x00) && (p_buffer[2] == 0x00) &&
		    ((p_buffer[3] == EAV_ACTIVE_VIDEO_FIELD1) ||
		     (p_buffer[3] == EAV_ACTIVE_VIDEO_FIELD2) ||
		     (p_buffer[3] == EAV_VBLANK_FIELD1) ||
		     (p_buffer[3] == EAV_VBLANK_FIELD2)))
			return bytes_copied;
		current_field = 1;
		break;

	case SAV_ACTIVE_VIDEO_FIELD2:
		/* looking for skipped line which occurred in PAL 720x480 mode.
		   In this case, there will be no active data contained between
		   the SAV and EAV */
		if ((buffer_size > 3) && (p_buffer[0] == 0xFF) &&
		    (p_buffer[1] == 0x00) && (p_buffer[2] == 0x00) &&
		    ((p_buffer[3] == EAV_ACTIVE_VIDEO_FIELD1) ||
		     (p_buffer[3] == EAV_ACTIVE_VIDEO_FIELD2) ||
		     (p_buffer[3] == EAV_VBLANK_FIELD1)       ||
		     (p_buffer[3] == EAV_VBLANK_FIELD2)))
			return bytes_copied;
		current_field = 2;
		break;
	}

	dma_q->last_sav = sav_eav;

	bytes_copied = cx231xx_copy_video_line(dev, dma_q, p_buffer,
					       buffer_size, current_field);

	return bytes_copied;
}

u32 cx231xx_copy_video_line(struct cx231xx *dev,
			    struct cx231xx_dmaqueue *dma_q, u8 *p_line,
			    u32 length, int field_number)
{
	u32 bytes_to_copy;
	struct cx231xx_buffer *buf;
	u32 _line_size = dev->width * 2;

	if (dma_q->current_field != field_number)
		cx231xx_reset_video_buffer(dev, dma_q);

	/* get the buffer pointer */
	buf = dev->video_mode.isoc_ctl.buf;

	/* Remember the field number for next time */
	dma_q->current_field = field_number;

	bytes_to_copy = dma_q->bytes_left_in_line;
	if (bytes_to_copy > length)
		bytes_to_copy = length;

	if (dma_q->lines_completed >= dma_q->lines_per_field) {
		dma_q->bytes_left_in_line -= bytes_to_copy;
		dma_q->is_partial_line = (dma_q->bytes_left_in_line == 0) ?
					  0 : 1;
		return 0;
	}

	dma_q->is_partial_line = 1;

	/* If we don't have a buffer, just return the number of bytes we would
	   have copied if we had a buffer. */
	if (!buf) {
		dma_q->bytes_left_in_line -= bytes_to_copy;
		dma_q->is_partial_line = (dma_q->bytes_left_in_line == 0)
					 ? 0 : 1;
		return bytes_to_copy;
	}

	/* copy the data to video buffer */
	cx231xx_do_copy(dev, dma_q, p_line, bytes_to_copy);

	dma_q->pos += bytes_to_copy;
	dma_q->bytes_left_in_line -= bytes_to_copy;

	if (dma_q->bytes_left_in_line == 0) {
		dma_q->bytes_left_in_line = _line_size;
		dma_q->lines_completed++;
		dma_q->is_partial_line = 0;

		if (cx231xx_is_buffer_done(dev, dma_q) && buf) {
			buffer_filled(dev, dma_q, buf);

			dma_q->pos = 0;
			buf = NULL;
			dma_q->lines_completed = 0;
		}
	}

	return bytes_to_copy;
}

void cx231xx_reset_video_buffer(struct cx231xx *dev,
				struct cx231xx_dmaqueue *dma_q)
{
	struct cx231xx_buffer *buf;

	/* handle the switch from field 1 to field 2 */
	if (dma_q->current_field == 1) {
		if (dma_q->lines_completed >= dma_q->lines_per_field)
			dma_q->field1_done = 1;
		else
			dma_q->field1_done = 0;
	}

	buf = dev->video_mode.isoc_ctl.buf;

	if (buf == NULL) {
		u8 *outp = NULL;
		/* first try to get the buffer */
		get_next_buf(dma_q, &buf);

		if (buf)
			outp = videobuf_to_vmalloc(&buf->vb);

		dma_q->pos = 0;
		dma_q->field1_done = 0;
		dma_q->current_field = -1;
	}

	/* reset the counters */
	dma_q->bytes_left_in_line = dev->width << 1;
	dma_q->lines_completed = 0;
}

int cx231xx_do_copy(struct cx231xx *dev, struct cx231xx_dmaqueue *dma_q,
		    u8 *p_buffer, u32 bytes_to_copy)
{
	u8 *p_out_buffer = NULL;
	u32 current_line_bytes_copied = 0;
	struct cx231xx_buffer *buf;
	u32 _line_size = dev->width << 1;
	void *startwrite;
	int offset, lencopy;

	buf = dev->video_mode.isoc_ctl.buf;

	if (buf == NULL)
		return -1;

	p_out_buffer = videobuf_to_vmalloc(&buf->vb);

	current_line_bytes_copied = _line_size - dma_q->bytes_left_in_line;

	/* Offset field 2 one line from the top of the buffer */
	offset = (dma_q->current_field == 1) ? 0 : _line_size;

	/* Offset for field 2 */
	startwrite = p_out_buffer + offset;

	/* lines already completed in the current field */
	startwrite += (dma_q->lines_completed * _line_size * 2);

	/* bytes already completed in the current line */
	startwrite += current_line_bytes_copied;

	lencopy = dma_q->bytes_left_in_line > bytes_to_copy ?
		  bytes_to_copy : dma_q->bytes_left_in_line;

	if ((u8 *)(startwrite + lencopy) > (u8 *)(p_out_buffer + buf->vb.size))
		return 0;

	/* The below copies the UYVY data straight into video buffer */
	cx231xx_swab((u16 *) p_buffer, (u16 *) startwrite, (u16) lencopy);

	return 0;
}

void cx231xx_swab(u16 *from, u16 *to, u16 len)
{
	u16 i;

	if (len <= 0)
		return;

	for (i = 0; i < len / 2; i++)
		to[i] = (from[i] << 8) | (from[i] >> 8);
}

u8 cx231xx_is_buffer_done(struct cx231xx *dev, struct cx231xx_dmaqueue *dma_q)
{
	u8 buffer_complete = 0;

	/* Dual field stream */
	buffer_complete = ((dma_q->current_field == 2) &&
			   (dma_q->lines_completed >= dma_q->lines_per_field) &&
			    dma_q->field1_done);

	return buffer_complete;
}

/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/

static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct cx231xx_fh *fh = vq->priv_data;
	struct cx231xx *dev = fh->dev;
	struct v4l2_frequency f;

	*size = (fh->dev->width * fh->dev->height * dev->format->depth + 7)>>3;
	if (0 == *count)
		*count = CX231XX_DEF_BUF;

	if (*count < CX231XX_MIN_BUF)
		*count = CX231XX_MIN_BUF;

	/* Ask tuner to go to analog mode */
	memset(&f, 0, sizeof(f));
	f.frequency = dev->ctl_freq;
	f.type = fh->radio ? V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;

	call_all(dev, tuner, s_frequency, &f);

	return 0;
}

/* This is called *without* dev->slock held; please keep it that way */
static void free_buffer(struct videobuf_queue *vq, struct cx231xx_buffer *buf)
{
	struct cx231xx_fh *fh = vq->priv_data;
	struct cx231xx *dev = fh->dev;
	unsigned long flags = 0;

	if (in_interrupt())
		BUG();

	/* We used to wait for the buffer to finish here, but this didn't work
	   because, as we were keeping the state as VIDEOBUF_QUEUED,
	   videobuf_queue_cancel marked it as finished for us.
	   (Also, it could wedge forever if the hardware was misconfigured.)

	   This should be safe; by the time we get here, the buffer isn't
	   queued anymore. If we ever start marking the buffers as
	   VIDEOBUF_ACTIVE, it won't be, though.
	 */
	spin_lock_irqsave(&dev->video_mode.slock, flags);
	if (dev->video_mode.isoc_ctl.buf == buf)
		dev->video_mode.isoc_ctl.buf = NULL;
	spin_unlock_irqrestore(&dev->video_mode.slock, flags);

	videobuf_vmalloc_free(&buf->vb);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
	       enum v4l2_field field)
{
	struct cx231xx_fh *fh = vq->priv_data;
	struct cx231xx_buffer *buf =
	    container_of(vb, struct cx231xx_buffer, vb);
	struct cx231xx *dev = fh->dev;
	int rc = 0, urb_init = 0;

	/* The only currently supported format is 16 bits/pixel */
	buf->vb.size = (fh->dev->width * fh->dev->height * dev->format->depth
			+ 7) >> 3;
	if (0 != buf->vb.baddr && buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	buf->vb.width = dev->width;
	buf->vb.height = dev->height;
	buf->vb.field = field;

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	if (!dev->video_mode.isoc_ctl.num_bufs)
		urb_init = 1;

	if (urb_init) {
		rc = cx231xx_init_isoc(dev, CX231XX_NUM_PACKETS,
				       CX231XX_NUM_BUFS,
				       dev->video_mode.max_pkt_size,
				       cx231xx_isoc_copy);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct cx231xx_buffer *buf =
	    container_of(vb, struct cx231xx_buffer, vb);
	struct cx231xx_fh *fh = vq->priv_data;
	struct cx231xx *dev = fh->dev;
	struct cx231xx_dmaqueue *vidq = &dev->video_mode.vidq;

	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);

}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct cx231xx_buffer *buf =
	    container_of(vb, struct cx231xx_buffer, vb);
	struct cx231xx_fh *fh = vq->priv_data;
	struct cx231xx *dev = (struct cx231xx *)fh->dev;

	cx231xx_isocdbg("cx231xx: called buffer_release\n");

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops cx231xx_video_qops = {
	.buf_setup = buffer_setup,
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.buf_release = buffer_release,
};

/*********************  v4l2 interface  **************************************/

void video_mux(struct cx231xx *dev, int index)
{
	dev->video_input = index;
	dev->ctl_ainput = INPUT(index)->amux;

	cx231xx_set_video_input_mux(dev, index);

	cx25840_call(dev, video, s_routing, INPUT(index)->vmux, 0, 0);

	cx231xx_set_audio_input(dev, dev->ctl_ainput);

	cx231xx_info("video_mux : %d\n", index);

	/* do mode control overrides if required */
	cx231xx_do_mode_ctrl_overrides(dev);
}

/* Usage lock check functions */
static int res_get(struct cx231xx_fh *fh)
{
	struct cx231xx *dev = fh->dev;
	int rc = 0;

	/* This instance already has stream_on */
	if (fh->stream_on)
		return rc;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		if (dev->stream_on)
			return -EBUSY;
		dev->stream_on = 1;
	} else if (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE) {
		if (dev->vbi_stream_on)
			return -EBUSY;
		dev->vbi_stream_on = 1;
	} else
		return -EINVAL;

	fh->stream_on = 1;

	return rc;
}

static int res_check(struct cx231xx_fh *fh)
{
	return fh->stream_on;
}

static void res_free(struct cx231xx_fh *fh)
{
	struct cx231xx *dev = fh->dev;

	fh->stream_on = 0;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		dev->stream_on = 0;
	if (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE)
		dev->vbi_stream_on = 0;
}

static int check_dev(struct cx231xx *dev)
{
	if (dev->state & DEV_DISCONNECTED) {
		cx231xx_errdev("v4l2 ioctl: device not present\n");
		return -ENODEV;
	}

	if (dev->state & DEV_MISCONFIGURED) {
		cx231xx_errdev("v4l2 ioctl: device is misconfigured; "
			       "close and open it again\n");
		return -EIO;
	}
	return 0;
}

static void get_scale(struct cx231xx *dev,
		      unsigned int width, unsigned int height,
		      unsigned int *hscale, unsigned int *vscale)
{
	unsigned int maxw = norm_maxw(dev);
	unsigned int maxh = norm_maxh(dev);

	*hscale = (((unsigned long)maxw) << 12) / width - 4096L;
	if (*hscale >= 0x4000)
		*hscale = 0x3fff;

	*vscale = (((unsigned long)maxh) << 12) / height - 4096L;
	if (*vscale >= 0x4000)
		*vscale = 0x3fff;
}

/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	mutex_lock(&dev->lock);

	f->fmt.pix.width = dev->width;
	f->fmt.pix.height = dev->height;
	f->fmt.pix.pixelformat = dev->format->fourcc;
	f->fmt.pix.bytesperline = (dev->width * dev->format->depth + 7) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * dev->height;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;

	mutex_unlock(&dev->lock);

	return 0;
}

static struct cx231xx_fmt *format_by_fourcc(unsigned int fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(format); i++)
		if (format[i].fourcc == fourcc)
			return &format[i];

	return NULL;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	unsigned int width = f->fmt.pix.width;
	unsigned int height = f->fmt.pix.height;
	unsigned int maxw = norm_maxw(dev);
	unsigned int maxh = norm_maxh(dev);
	unsigned int hscale, vscale;
	struct cx231xx_fmt *fmt;

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	if (!fmt) {
		cx231xx_videodbg("Fourcc format (%08x) invalid.\n",
				 f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	/* width must even because of the YUYV format
	   height must be even because of interlacing */
	v4l_bound_align_image(&width, 48, maxw, 1, &height, 32, maxh, 1, 0);

	get_scale(dev, width, height, &hscale, &vscale);

	width = (((unsigned long)maxw) << 12) / (hscale + 4096L);
	height = (((unsigned long)maxh) << 12) / (vscale + 4096L);

	f->fmt.pix.width = width;
	f->fmt.pix.height = height;
	f->fmt.pix.pixelformat = fmt->fourcc;
	f->fmt.pix.bytesperline = (dev->width * fmt->depth + 7) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * height;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	f->fmt.pix.field = V4L2_FIELD_INTERLACED;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;
	struct cx231xx_fmt *fmt;
	struct v4l2_mbus_framefmt mbus_fmt;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);

	vidioc_try_fmt_vid_cap(file, priv, f);

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	if (!fmt) {
		rc = -EINVAL;
		goto out;
	}

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		cx231xx_errdev("%s queue busy\n", __func__);
		rc = -EBUSY;
		goto out;
	}

	if (dev->stream_on && !fh->stream_on) {
		cx231xx_errdev("%s device in use by another fh\n", __func__);
		rc = -EBUSY;
		goto out;
	}

	/* set new image size */
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	dev->format = fmt;
	get_scale(dev, dev->width, dev->height, &dev->hscale, &dev->vscale);

	v4l2_fill_mbus_format(&mbus_fmt, &f->fmt.pix, V4L2_MBUS_FMT_FIXED);
	call_all(dev, video, s_mbus_fmt, &mbus_fmt);
	v4l2_fill_pix_format(&f->fmt.pix, &mbus_fmt);

	/* Set the correct alternate setting for this resolution */
	cx231xx_resolution_set(dev);

out:
	mutex_unlock(&dev->lock);
	return rc;
}

static int vidioc_g_std(struct file *file, void *priv, v4l2_std_id * id)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	*id = dev->norm;
	return 0;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *norm)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	struct v4l2_format f;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	cx231xx_info("vidioc_s_std : 0x%x\n", (unsigned int)*norm);

	mutex_lock(&dev->lock);
	dev->norm = *norm;

	/* Adjusts width/height, if needed */
	f.fmt.pix.width = dev->width;
	f.fmt.pix.height = dev->height;
	vidioc_try_fmt_vid_cap(file, priv, &f);

	/* set new image size */
	dev->width = f.fmt.pix.width;
	dev->height = f.fmt.pix.height;
	get_scale(dev, dev->width, dev->height, &dev->hscale, &dev->vscale);

	call_all(dev, core, s_std, dev->norm);

	mutex_unlock(&dev->lock);

	cx231xx_resolution_set(dev);

	/* do mode control overrides */
	cx231xx_do_mode_ctrl_overrides(dev);

	return 0;
}

static const char *iname[] = {
	[CX231XX_VMUX_COMPOSITE1] = "Composite1",
	[CX231XX_VMUX_SVIDEO]     = "S-Video",
	[CX231XX_VMUX_TELEVISION] = "Television",
	[CX231XX_VMUX_CABLE]      = "Cable TV",
	[CX231XX_VMUX_DVB]        = "DVB",
	[CX231XX_VMUX_DEBUG]      = "for debug only",
};

static int vidioc_enum_input(struct file *file, void *priv,
			     struct v4l2_input *i)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	unsigned int n;

	n = i->index;
	if (n >= MAX_CX231XX_INPUT)
		return -EINVAL;
	if (0 == INPUT(n)->type)
		return -EINVAL;

	i->index = n;
	i->type = V4L2_INPUT_TYPE_CAMERA;

	strcpy(i->name, iname[INPUT(n)->type]);

	if ((CX231XX_VMUX_TELEVISION == INPUT(n)->type) ||
	    (CX231XX_VMUX_CABLE == INPUT(n)->type))
		i->type = V4L2_INPUT_TYPE_TUNER;

	i->std = dev->vdev->tvnorms;

	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	*i = dev->video_input;

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if (i >= MAX_CX231XX_INPUT)
		return -EINVAL;
	if (0 == INPUT(i)->type)
		return -EINVAL;

	mutex_lock(&dev->lock);

	video_mux(dev, i);

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_g_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	switch (a->index) {
	case CX231XX_AMUX_VIDEO:
		strcpy(a->name, "Television");
		break;
	case CX231XX_AMUX_LINE_IN:
		strcpy(a->name, "Line In");
		break;
	default:
		return -EINVAL;
	}

	a->index = dev->ctl_ainput;
	a->capability = V4L2_AUDCAP_STEREO;

	return 0;
}

static int vidioc_s_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int status = 0;

	/* Doesn't allow manual routing */
	if (a->index != dev->ctl_ainput)
		return -EINVAL;

	dev->ctl_ainput = INPUT(a->index)->amux;
	status = cx231xx_set_audio_input(dev, dev->ctl_ainput);

	return status;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int id = qc->id;
	int i;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	qc->id = v4l2_ctrl_next(ctrl_classes, qc->id);
	if (unlikely(qc->id == 0))
		return -EINVAL;

	memset(qc, 0, sizeof(*qc));

	qc->id = id;

	if (qc->id < V4L2_CID_BASE || qc->id >= V4L2_CID_LASTP1)
		return -EINVAL;

	for (i = 0; i < CX231XX_CTLS; i++)
		if (cx231xx_ctls[i].v.id == qc->id)
			break;

	if (i == CX231XX_CTLS) {
		*qc = no_ctl;
		return 0;
	}
	*qc = cx231xx_ctls[i].v;

	mutex_lock(&dev->lock);
	call_all(dev, core, queryctrl, qc);
	mutex_unlock(&dev->lock);

	if (qc->type)
		return 0;
	else
		return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	call_all(dev, core, g_ctrl, ctrl);
	mutex_unlock(&dev->lock);
	return rc;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	call_all(dev, core, s_ctrl, ctrl);
	mutex_unlock(&dev->lock);
	return rc;
}

static int vidioc_g_tuner(struct file *file, void *priv, struct v4l2_tuner *t)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if (0 != t->index)
		return -EINVAL;

	strcpy(t->name, "Tuner");

	t->type = V4L2_TUNER_ANALOG_TV;
	t->capability = V4L2_TUNER_CAP_NORM;
	t->rangehigh = 0xffffffffUL;
	t->signal = 0xffff;	/* LOCKED */

	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv, struct v4l2_tuner *t)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if (0 != t->index)
		return -EINVAL;
	return 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
			      struct v4l2_frequency *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	mutex_lock(&dev->lock);
	f->type = fh->radio ? V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
	f->frequency = dev->ctl_freq;

	call_all(dev, tuner, g_frequency, f);

	mutex_unlock(&dev->lock);

	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
			      struct v4l2_frequency *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if (0 != f->tuner)
		return -EINVAL;

	if (unlikely(0 == fh->radio && f->type != V4L2_TUNER_ANALOG_TV))
		return -EINVAL;
	if (unlikely(1 == fh->radio && f->type != V4L2_TUNER_RADIO))
		return -EINVAL;

	/* set pre channel change settings in DIF first */
	rc = cx231xx_tuner_pre_channel_change(dev);

	mutex_lock(&dev->lock);

	dev->ctl_freq = f->frequency;

	if (dev->tuner_type == TUNER_XC5000) {
		if (dev->cx231xx_set_analog_freq != NULL)
			dev->cx231xx_set_analog_freq(dev, f->frequency);
	} else
		call_all(dev, tuner, s_frequency, f);

	mutex_unlock(&dev->lock);

	/* set post channel change settings in DIF first */
	rc = cx231xx_tuner_post_channel_change(dev);

	cx231xx_info("Set New FREQUENCY to %d\n", f->frequency);

	return rc;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG

/*
  -R, --list-registers=type=<host/i2cdrv/i2caddr>,
				chip=<chip>[,min=<addr>,max=<addr>]
		     dump registers from <min> to <max> [VIDIOC_DBG_G_REGISTER]
  -r, --set-register=type=<host/i2cdrv/i2caddr>,
				chip=<chip>,reg=<addr>,val=<val>
		     set the register [VIDIOC_DBG_S_REGISTER]

  if type == host, then <chip> is the hosts chip ID (default 0)
  if type == i2cdrv (default), then <chip> is the I2C driver name or ID
  if type == i2caddr, then <chip> is the 7-bit I2C address
*/

static int vidioc_g_register(struct file *file, void *priv,
			     struct v4l2_dbg_register *reg)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int ret = 0;
	u8 value[4] = { 0, 0, 0, 0 };
	u32 data = 0;

	switch (reg->match.type) {
	case V4L2_CHIP_MATCH_HOST:
		switch (reg->match.addr) {
		case 0:	/* Cx231xx - internal registers */
			ret = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER,
						  (u16)reg->reg, value, 4);
			reg->val = value[0] | value[1] << 8 |
				   value[2] << 16 | value[3] << 24;
			break;
		case 1:	/* AFE - read byte */
			ret = cx231xx_read_i2c_data(dev, AFE_DEVICE_ADDRESS,
						  (u16)reg->reg, 2, &data, 1);
			reg->val = le32_to_cpu(data & 0xff);
			break;
		case 14: /* AFE - read dword */
			ret = cx231xx_read_i2c_data(dev, AFE_DEVICE_ADDRESS,
						  (u16)reg->reg, 2, &data, 4);
			reg->val = le32_to_cpu(data);
			break;
		case 2:	/* Video Block - read byte */
			ret = cx231xx_read_i2c_data(dev, VID_BLK_I2C_ADDRESS,
						  (u16)reg->reg, 2, &data, 1);
			reg->val = le32_to_cpu(data & 0xff);
			break;
		case 24: /* Video Block - read dword */
			ret = cx231xx_read_i2c_data(dev, VID_BLK_I2C_ADDRESS,
						  (u16)reg->reg, 2, &data, 4);
			reg->val = le32_to_cpu(data);
			break;
		case 3:	/* I2S block - read byte */
			ret = cx231xx_read_i2c_data(dev,
						    I2S_BLK_DEVICE_ADDRESS,
						    (u16)reg->reg, 1,
						    &data, 1);
			reg->val = le32_to_cpu(data & 0xff);
			break;
		case 34: /* I2S Block - read dword */
			ret =
			    cx231xx_read_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
						  (u16)reg->reg, 1, &data, 4);
			reg->val = le32_to_cpu(data);
			break;
		}
		return ret < 0 ? ret : 0;

	case V4L2_CHIP_MATCH_I2C_DRIVER:
		call_all(dev, core, g_register, reg);
		return 0;
	case V4L2_CHIP_MATCH_I2C_ADDR:
		/* Not supported yet */
		return -EINVAL;
	default:
		if (!v4l2_chip_match_host(&reg->match))
			return -EINVAL;
	}

	mutex_lock(&dev->lock);
	call_all(dev, core, g_register, reg);
	mutex_unlock(&dev->lock);

	return ret;
}

static int vidioc_s_register(struct file *file, void *priv,
			     struct v4l2_dbg_register *reg)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int ret = 0;
	__le64 buf;
	u32 value;
	u8 data[4] = { 0, 0, 0, 0 };

	buf = cpu_to_le64(reg->val);

	switch (reg->match.type) {
	case V4L2_CHIP_MATCH_HOST:
		{
			value = (u32) buf & 0xffffffff;

			switch (reg->match.addr) {
			case 0:	/* cx231xx internal registers */
				data[0] = (u8) value;
				data[1] = (u8) (value >> 8);
				data[2] = (u8) (value >> 16);
				data[3] = (u8) (value >> 24);
				ret = cx231xx_write_ctrl_reg(dev,
							   VRT_SET_REGISTER,
							   (u16)reg->reg, data,
							   4);
				break;
			case 1:	/* AFE - read byte */
				ret = cx231xx_write_i2c_data(dev,
							AFE_DEVICE_ADDRESS,
							(u16)reg->reg, 2,
							value, 1);
				break;
			case 14: /* AFE - read dword */
				ret = cx231xx_write_i2c_data(dev,
							AFE_DEVICE_ADDRESS,
							(u16)reg->reg, 2,
							value, 4);
				break;
			case 2:	/* Video Block - read byte */
				ret =
				    cx231xx_write_i2c_data(dev,
							VID_BLK_I2C_ADDRESS,
							(u16)reg->reg, 2,
							value, 1);
				break;
			case 24: /* Video Block - read dword */
				ret =
				    cx231xx_write_i2c_data(dev,
							VID_BLK_I2C_ADDRESS,
							(u16)reg->reg, 2,
							value, 4);
				break;
			case 3:	/* I2S block - read byte */
				ret =
				    cx231xx_write_i2c_data(dev,
							I2S_BLK_DEVICE_ADDRESS,
							(u16)reg->reg, 1,
							value, 1);
				break;
			case 34: /* I2S block - read dword */
				ret =
				    cx231xx_write_i2c_data(dev,
							I2S_BLK_DEVICE_ADDRESS,
							(u16)reg->reg, 1,
							value, 4);
				break;
			}
		}
		return ret < 0 ? ret : 0;

	default:
		break;
	}

	mutex_lock(&dev->lock);
	call_all(dev, core, s_register, reg);
	mutex_unlock(&dev->lock);

	return ret;
}
#endif

static int vidioc_cropcap(struct file *file, void *priv,
			  struct v4l2_cropcap *cc)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	if (cc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	cc->bounds.left = 0;
	cc->bounds.top = 0;
	cc->bounds.width = dev->width;
	cc->bounds.height = dev->height;
	cc->defrect = cc->bounds;
	cc->pixelaspect.numerator = 54;
	cc->pixelaspect.denominator = 59;

	return 0;
}

static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	rc = res_get(fh);

	if (likely(rc >= 0))
		rc = videobuf_streamon(&fh->vb_vidq);

	call_all(dev, video, s_stream, 1);

	mutex_unlock(&dev->lock);

	return rc;
}

static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if ((fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) &&
	    (fh->type != V4L2_BUF_TYPE_VBI_CAPTURE))
		return -EINVAL;
	if (type != fh->type)
		return -EINVAL;

	mutex_lock(&dev->lock);

	cx25840_call(dev, video, s_stream, 0);

	videobuf_streamoff(&fh->vb_vidq);
	res_free(fh);

	mutex_unlock(&dev->lock);

	return 0;
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	strlcpy(cap->driver, "cx231xx", sizeof(cap->driver));
	strlcpy(cap->card, cx231xx_boards[dev->model].name, sizeof(cap->card));
	usb_make_path(dev->udev, cap->bus_info, sizeof(cap->bus_info));

	cap->version = CX231XX_VERSION_CODE;

	cap->capabilities = V4L2_CAP_VBI_CAPTURE |
		V4L2_CAP_VIDEO_CAPTURE	|
		V4L2_CAP_AUDIO		|
		V4L2_CAP_READWRITE	|
		V4L2_CAP_STREAMING;

	if (dev->tuner_type != TUNER_ABSENT)
		cap->capabilities |= V4L2_CAP_TUNER;

	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	if (unlikely(f->index >= ARRAY_SIZE(format)))
		return -EINVAL;

	strlcpy(f->description, format[f->index].name, sizeof(f->description));
	f->pixelformat = format[f->index].fourcc;

	return 0;
}

/* Sliced VBI ioctls */
static int vidioc_g_fmt_sliced_vbi_cap(struct file *file, void *priv,
				       struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);

	f->fmt.sliced.service_set = 0;

	call_all(dev, vbi, g_sliced_fmt, &f->fmt.sliced);

	if (f->fmt.sliced.service_set == 0)
		rc = -EINVAL;

	mutex_unlock(&dev->lock);
	return rc;
}

static int vidioc_try_set_sliced_vbi_cap(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	call_all(dev, vbi, g_sliced_fmt, &f->fmt.sliced);
	mutex_unlock(&dev->lock);

	if (f->fmt.sliced.service_set == 0)
		return -EINVAL;

	return 0;
}

/* RAW VBI ioctls */

static int vidioc_g_fmt_vbi_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	f->fmt.vbi.sampling_rate = (dev->norm & V4L2_STD_625_50) ?
	    35468950 : 28636363;
	f->fmt.vbi.samples_per_line = VBI_LINE_LENGTH;
	f->fmt.vbi.sample_format = V4L2_PIX_FMT_GREY;
	f->fmt.vbi.offset = 64 * 4;
	f->fmt.vbi.start[0] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_START_LINE : NTSC_VBI_START_LINE;
	f->fmt.vbi.count[0] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_LINES : NTSC_VBI_LINES;
	f->fmt.vbi.start[1] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_START_LINE + 312 : NTSC_VBI_START_LINE + 263;
	f->fmt.vbi.count[1] = f->fmt.vbi.count[0];

	return 0;

}

static int vidioc_try_fmt_vbi_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;

	if (dev->vbi_stream_on && !fh->stream_on) {
		cx231xx_errdev("%s device in use by another fh\n", __func__);
		return -EBUSY;
	}

	f->type = V4L2_BUF_TYPE_VBI_CAPTURE;
	f->fmt.vbi.sampling_rate = (dev->norm & V4L2_STD_625_50) ?
	    35468950 : 28636363;
	f->fmt.vbi.samples_per_line = VBI_LINE_LENGTH;
	f->fmt.vbi.sample_format = V4L2_PIX_FMT_GREY;
	f->fmt.vbi.offset = 244;
	f->fmt.vbi.flags = 0;
	f->fmt.vbi.start[0] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_START_LINE : NTSC_VBI_START_LINE;
	f->fmt.vbi.count[0] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_LINES : NTSC_VBI_LINES;
	f->fmt.vbi.start[1] = (dev->norm & V4L2_STD_625_50) ?
	    PAL_VBI_START_LINE + 312 : NTSC_VBI_START_LINE + 263;
	f->fmt.vbi.count[1] = f->fmt.vbi.count[0];

	return 0;

}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *rb)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	return videobuf_reqbufs(&fh->vb_vidq, rb);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	return videobuf_querybuf(&fh->vb_vidq, b);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	return videobuf_qbuf(&fh->vb_vidq, b);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct cx231xx_fh *fh = priv;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	return videobuf_dqbuf(&fh->vb_vidq, b, file->f_flags & O_NONBLOCK);
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct cx231xx_fh *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

/* ----------------------------------------------------------- */
/* RADIO ESPECIFIC IOCTLS                                      */
/* ----------------------------------------------------------- */

static int radio_querycap(struct file *file, void *priv,
			  struct v4l2_capability *cap)
{
	struct cx231xx *dev = ((struct cx231xx_fh *)priv)->dev;

	strlcpy(cap->driver, "cx231xx", sizeof(cap->driver));
	strlcpy(cap->card, cx231xx_boards[dev->model].name, sizeof(cap->card));
	usb_make_path(dev->udev, cap->bus_info, sizeof(cap->bus_info));

	cap->version = CX231XX_VERSION_CODE;
	cap->capabilities = V4L2_CAP_TUNER;
	return 0;
}

static int radio_g_tuner(struct file *file, void *priv, struct v4l2_tuner *t)
{
	struct cx231xx *dev = ((struct cx231xx_fh *)priv)->dev;

	if (unlikely(t->index > 0))
		return -EINVAL;

	strcpy(t->name, "Radio");
	t->type = V4L2_TUNER_RADIO;

	mutex_lock(&dev->lock);
	call_all(dev, tuner, s_tuner, t);
	mutex_unlock(&dev->lock);

	return 0;
}

static int radio_enum_input(struct file *file, void *priv, struct v4l2_input *i)
{
	if (i->index != 0)
		return -EINVAL;
	strcpy(i->name, "Radio");
	i->type = V4L2_INPUT_TYPE_TUNER;

	return 0;
}

static int radio_g_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	if (unlikely(a->index))
		return -EINVAL;

	strcpy(a->name, "Radio");
	return 0;
}

static int radio_s_tuner(struct file *file, void *priv, struct v4l2_tuner *t)
{
	struct cx231xx *dev = ((struct cx231xx_fh *)priv)->dev;

	if (0 != t->index)
		return -EINVAL;

	mutex_lock(&dev->lock);
	call_all(dev, tuner, s_tuner, t);
	mutex_unlock(&dev->lock);

	return 0;
}

static int radio_s_audio(struct file *file, void *fh, struct v4l2_audio *a)
{
	return 0;
}

static int radio_s_input(struct file *file, void *fh, unsigned int i)
{
	return 0;
}

static int radio_queryctrl(struct file *file, void *priv,
			   struct v4l2_queryctrl *c)
{
	int i;

	if (c->id < V4L2_CID_BASE || c->id >= V4L2_CID_LASTP1)
		return -EINVAL;
	if (c->id == V4L2_CID_AUDIO_MUTE) {
		for (i = 0; i < CX231XX_CTLS; i++) {
			if (cx231xx_ctls[i].v.id == c->id)
				break;
		}
		if (i == CX231XX_CTLS)
			return -EINVAL;
		*c = cx231xx_ctls[i].v;
	} else
		*c = no_ctl;
	return 0;
}

/*
 * cx231xx_v4l2_open()
 * inits the device and starts isoc transfer
 */
static int cx231xx_v4l2_open(struct file *filp)
{
	int errCode = 0, radio = 0;
	struct video_device *vdev = video_devdata(filp);
	struct cx231xx *dev = video_drvdata(filp);
	struct cx231xx_fh *fh;
	enum v4l2_buf_type fh_type = 0;

	switch (vdev->vfl_type) {
	case VFL_TYPE_GRABBER:
		fh_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		break;
	case VFL_TYPE_VBI:
		fh_type = V4L2_BUF_TYPE_VBI_CAPTURE;
		break;
	case VFL_TYPE_RADIO:
		radio = 1;
		break;
	}

	mutex_lock(&dev->lock);

	cx231xx_videodbg("open dev=%s type=%s users=%d\n",
			 video_device_node_name(vdev), v4l2_type_names[fh_type],
			 dev->users);


	fh = kzalloc(sizeof(struct cx231xx_fh), GFP_KERNEL);
	if (!fh) {
		cx231xx_errdev("cx231xx-video.c: Out of memory?!\n");
		mutex_unlock(&dev->lock);
		return -ENOMEM;
	}
	fh->dev = dev;
	fh->radio = radio;
	fh->type = fh_type;
	filp->private_data = fh;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE && dev->users == 0) {
		dev->width = norm_maxw(dev);
		dev->height = norm_maxh(dev);
		dev->hscale = 0;
		dev->vscale = 0;

		/* Power up in Analog TV mode */
		cx231xx_set_power_mode(dev, POLARIS_AVMODE_ANALOGT_TV);

		cx231xx_resolution_set(dev);

		/* set video alternate setting */
		cx231xx_set_video_alternate(dev);

		/* Needed, since GPIO might have disabled power of
		   some i2c device */
		cx231xx_config_i2c(dev);

		/* device needs to be initialized before isoc transfer */
		dev->video_input = dev->video_input > 2 ? 2 : dev->video_input;
		video_mux(dev, dev->video_input);

	}
	if (fh->radio) {
		cx231xx_videodbg("video_open: setting radio device\n");

		/* cx231xx_start_radio(dev); */

		call_all(dev, tuner, s_radio);
	}

	dev->users++;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		videobuf_queue_vmalloc_init(&fh->vb_vidq, &cx231xx_video_qops,
					    NULL, &dev->video_mode.slock,
					    fh->type, V4L2_FIELD_INTERLACED,
					    sizeof(struct cx231xx_buffer), fh);
	if (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE) {
		/* Set the required alternate setting  VBI interface works in
		   Bulk mode only */
		cx231xx_set_alt_setting(dev, INDEX_VANC, 0);

		videobuf_queue_vmalloc_init(&fh->vb_vidq, &cx231xx_vbi_qops,
					    NULL, &dev->vbi_mode.slock,
					    fh->type, V4L2_FIELD_SEQ_TB,
					    sizeof(struct cx231xx_buffer), fh);
	}

	mutex_unlock(&dev->lock);

	return errCode;
}

/*
 * cx231xx_realease_resources()
 * unregisters the v4l2,i2c and usb devices
 * called when the device gets disconected or at module unload
*/
void cx231xx_release_analog_resources(struct cx231xx *dev)
{


	if (dev->radio_dev) {
		if (video_is_registered(dev->radio_dev))
			video_unregister_device(dev->radio_dev);
		else
			video_device_release(dev->radio_dev);
		dev->radio_dev = NULL;
	}
	if (dev->vbi_dev) {
		cx231xx_info("V4L2 device %s deregistered\n",
			     video_device_node_name(dev->vbi_dev));
		if (video_is_registered(dev->vbi_dev))
			video_unregister_device(dev->vbi_dev);
		else
			video_device_release(dev->vbi_dev);
		dev->vbi_dev = NULL;
	}
	if (dev->vdev) {
		cx231xx_info("V4L2 device %s deregistered\n",
			     video_device_node_name(dev->vdev));
		if (video_is_registered(dev->vdev))
			video_unregister_device(dev->vdev);
		else
			video_device_release(dev->vdev);
		dev->vdev = NULL;
	}
}

/*
 * cx231xx_v4l2_close()
 * stops streaming and deallocates all resources allocated by the v4l2
 * calls and ioctls
 */
static int cx231xx_v4l2_close(struct file *filp)
{
	struct cx231xx_fh *fh = filp->private_data;
	struct cx231xx *dev = fh->dev;

	cx231xx_videodbg("users=%d\n", dev->users);

	mutex_lock(&dev->lock);

	if (res_check(fh))
		res_free(fh);

	if (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE) {
		videobuf_stop(&fh->vb_vidq);
		videobuf_mmap_free(&fh->vb_vidq);

		/* the device is already disconnect,
		   free the remaining resources */
		if (dev->state & DEV_DISCONNECTED) {
			cx231xx_release_resources(dev);
			mutex_unlock(&dev->lock);
			kfree(dev);
			return 0;
		}

		/* do this before setting alternate! */
		cx231xx_uninit_vbi_isoc(dev);

		/* set alternate 0 */
		if (!dev->vbi_or_sliced_cc_mode)
			cx231xx_set_alt_setting(dev, INDEX_VANC, 0);
		else
			cx231xx_set_alt_setting(dev, INDEX_HANC, 0);

		kfree(fh);
		dev->users--;
		wake_up_interruptible_nr(&dev->open, 1);
		mutex_unlock(&dev->lock);
		return 0;
	}

	if (dev->users == 1) {
		videobuf_stop(&fh->vb_vidq);
		videobuf_mmap_free(&fh->vb_vidq);

		/* the device is already disconnect,
		   free the remaining resources */
		if (dev->state & DEV_DISCONNECTED) {
			cx231xx_release_resources(dev);
			mutex_unlock(&dev->lock);
			kfree(dev);
			return 0;
		}

		/* Save some power by putting tuner to sleep */
		call_all(dev, core, s_power, 0);

		/* do this before setting alternate! */
		cx231xx_uninit_isoc(dev);
		cx231xx_set_mode(dev, CX231XX_SUSPEND);

		/* set alternate 0 */
		cx231xx_set_alt_setting(dev, INDEX_VIDEO, 0);
	}
	kfree(fh);
	dev->users--;
	wake_up_interruptible_nr(&dev->open, 1);
	mutex_unlock(&dev->lock);
	return 0;
}

/*
 * cx231xx_v4l2_read()
 * will allocate buffers when called for the first time
 */
static ssize_t
cx231xx_v4l2_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *pos)
{
	struct cx231xx_fh *fh = filp->private_data;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	if ((fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
	    (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE)) {
		mutex_lock(&dev->lock);
		rc = res_get(fh);
		mutex_unlock(&dev->lock);

		if (unlikely(rc < 0))
			return rc;

		return videobuf_read_stream(&fh->vb_vidq, buf, count, pos, 0,
					    filp->f_flags & O_NONBLOCK);
	}
	return 0;
}

/*
 * cx231xx_v4l2_poll()
 * will allocate buffers when called for the first time
 */
static unsigned int cx231xx_v4l2_poll(struct file *filp, poll_table * wait)
{
	struct cx231xx_fh *fh = filp->private_data;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	rc = res_get(fh);
	mutex_unlock(&dev->lock);

	if (unlikely(rc < 0))
		return POLLERR;

	if ((V4L2_BUF_TYPE_VIDEO_CAPTURE == fh->type) ||
	    (V4L2_BUF_TYPE_VBI_CAPTURE == fh->type))
		return videobuf_poll_stream(filp, &fh->vb_vidq, wait);
	else
		return POLLERR;
}

/*
 * cx231xx_v4l2_mmap()
 */
static int cx231xx_v4l2_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct cx231xx_fh *fh = filp->private_data;
	struct cx231xx *dev = fh->dev;
	int rc;

	rc = check_dev(dev);
	if (rc < 0)
		return rc;

	mutex_lock(&dev->lock);
	rc = res_get(fh);
	mutex_unlock(&dev->lock);

	if (unlikely(rc < 0))
		return rc;

	rc = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	cx231xx_videodbg("vma start=0x%08lx, size=%ld, ret=%d\n",
			 (unsigned long)vma->vm_start,
			 (unsigned long)vma->vm_end -
			 (unsigned long)vma->vm_start, rc);

	return rc;
}

static const struct v4l2_file_operations cx231xx_v4l_fops = {
	.owner   = THIS_MODULE,
	.open    = cx231xx_v4l2_open,
	.release = cx231xx_v4l2_close,
	.read    = cx231xx_v4l2_read,
	.poll    = cx231xx_v4l2_poll,
	.mmap    = cx231xx_v4l2_mmap,
	.ioctl   = video_ioctl2,
};

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap               = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap       = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap          = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap        = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap          = vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_vbi_cap          = vidioc_g_fmt_vbi_cap,
	.vidioc_try_fmt_vbi_cap        = vidioc_try_fmt_vbi_cap,
	.vidioc_s_fmt_vbi_cap          = vidioc_try_fmt_vbi_cap,
	.vidioc_g_audio                =  vidioc_g_audio,
	.vidioc_s_audio                = vidioc_s_audio,
	.vidioc_cropcap                = vidioc_cropcap,
	.vidioc_g_fmt_sliced_vbi_cap   = vidioc_g_fmt_sliced_vbi_cap,
	.vidioc_try_fmt_sliced_vbi_cap = vidioc_try_set_sliced_vbi_cap,
	.vidioc_reqbufs                = vidioc_reqbufs,
	.vidioc_querybuf               = vidioc_querybuf,
	.vidioc_qbuf                   = vidioc_qbuf,
	.vidioc_dqbuf                  = vidioc_dqbuf,
	.vidioc_s_std                  = vidioc_s_std,
	.vidioc_g_std                  = vidioc_g_std,
	.vidioc_enum_input             = vidioc_enum_input,
	.vidioc_g_input                = vidioc_g_input,
	.vidioc_s_input                = vidioc_s_input,
	.vidioc_queryctrl              = vidioc_queryctrl,
	.vidioc_g_ctrl                 = vidioc_g_ctrl,
	.vidioc_s_ctrl                 = vidioc_s_ctrl,
	.vidioc_streamon               = vidioc_streamon,
	.vidioc_streamoff              = vidioc_streamoff,
	.vidioc_g_tuner                = vidioc_g_tuner,
	.vidioc_s_tuner                = vidioc_s_tuner,
	.vidioc_g_frequency            = vidioc_g_frequency,
	.vidioc_s_frequency            = vidioc_s_frequency,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register             = vidioc_g_register,
	.vidioc_s_register             = vidioc_s_register,
#endif
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf                   = vidiocgmbuf,
#endif
};

static struct video_device cx231xx_vbi_template;

static const struct video_device cx231xx_video_template = {
	.fops         = &cx231xx_v4l_fops,
	.release      = video_device_release,
	.ioctl_ops    = &video_ioctl_ops,
	.tvnorms      = V4L2_STD_ALL,
	.current_norm = V4L2_STD_PAL,
};

static const struct v4l2_file_operations radio_fops = {
	.owner   = THIS_MODULE,
	.open   = cx231xx_v4l2_open,
	.release = cx231xx_v4l2_close,
	.ioctl   = video_ioctl2,
};

static const struct v4l2_ioctl_ops radio_ioctl_ops = {
	.vidioc_querycap    = radio_querycap,
	.vidioc_g_tuner     = radio_g_tuner,
	.vidioc_enum_input  = radio_enum_input,
	.vidioc_g_audio     = radio_g_audio,
	.vidioc_s_tuner     = radio_s_tuner,
	.vidioc_s_audio     = radio_s_audio,
	.vidioc_s_input     = radio_s_input,
	.vidioc_queryctrl   = radio_queryctrl,
	.vidioc_g_ctrl      = vidioc_g_ctrl,
	.vidioc_s_ctrl      = vidioc_s_ctrl,
	.vidioc_g_frequency = vidioc_g_frequency,
	.vidioc_s_frequency = vidioc_s_frequency,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register  = vidioc_g_register,
	.vidioc_s_register  = vidioc_s_register,
#endif
};

static struct video_device cx231xx_radio_template = {
	.name      = "cx231xx-radio",
	.fops      = &radio_fops,
	.ioctl_ops = &radio_ioctl_ops,
};

/******************************** usb interface ******************************/

static struct video_device *cx231xx_vdev_init(struct cx231xx *dev,
		const struct video_device
		*template, const char *type_name)
{
	struct video_device *vfd;

	vfd = video_device_alloc();
	if (NULL == vfd)
		return NULL;

	*vfd = *template;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->release = video_device_release;
	vfd->debug = video_debug;

	snprintf(vfd->name, sizeof(vfd->name), "%s %s", dev->name, type_name);

	video_set_drvdata(vfd, dev);
	return vfd;
}

int cx231xx_register_analog_devices(struct cx231xx *dev)
{
	int ret;

	cx231xx_info("%s: v4l2 driver version %d.%d.%d\n",
		     dev->name,
		     (CX231XX_VERSION_CODE >> 16) & 0xff,
		     (CX231XX_VERSION_CODE >> 8) & 0xff,
		     CX231XX_VERSION_CODE & 0xff);

	/* set default norm */
	/*dev->norm = cx231xx_video_template.current_norm; */
	dev->width = norm_maxw(dev);
	dev->height = norm_maxh(dev);
	dev->interlaced = 0;
	dev->hscale = 0;
	dev->vscale = 0;

	/* Analog specific initialization */
	dev->format = &format[0];
	/* video_mux(dev, dev->video_input); */

	/* Audio defaults */
	dev->mute = 1;
	dev->volume = 0x1f;

	/* enable vbi capturing */
	/* write code here...  */

	/* allocate and fill video video_device struct */
	dev->vdev = cx231xx_vdev_init(dev, &cx231xx_video_template, "video");
	if (!dev->vdev) {
		cx231xx_errdev("cannot allocate video_device.\n");
		return -ENODEV;
	}

	/* register v4l2 video video_device */
	ret = video_register_device(dev->vdev, VFL_TYPE_GRABBER,
				    video_nr[dev->devno]);
	if (ret) {
		cx231xx_errdev("unable to register video device (error=%i).\n",
			       ret);
		return ret;
	}

	cx231xx_info("%s/0: registered device %s [v4l2]\n",
		     dev->name, video_device_node_name(dev->vdev));

	/* Initialize VBI template */
	memcpy(&cx231xx_vbi_template, &cx231xx_video_template,
	       sizeof(cx231xx_vbi_template));
	strcpy(cx231xx_vbi_template.name, "cx231xx-vbi");

	/* Allocate and fill vbi video_device struct */
	dev->vbi_dev = cx231xx_vdev_init(dev, &cx231xx_vbi_template, "vbi");

	/* register v4l2 vbi video_device */
	ret = video_register_device(dev->vbi_dev, VFL_TYPE_VBI,
				    vbi_nr[dev->devno]);
	if (ret < 0) {
		cx231xx_errdev("unable to register vbi device\n");
		return ret;
	}

	cx231xx_info("%s/0: registered device %s\n",
		     dev->name, video_device_node_name(dev->vbi_dev));

	if (cx231xx_boards[dev->model].radio.type == CX231XX_RADIO) {
		dev->radio_dev = cx231xx_vdev_init(dev, &cx231xx_radio_template,
						   "radio");
		if (!dev->radio_dev) {
			cx231xx_errdev("cannot allocate video_device.\n");
			return -ENODEV;
		}
		ret = video_register_device(dev->radio_dev, VFL_TYPE_RADIO,
					    radio_nr[dev->devno]);
		if (ret < 0) {
			cx231xx_errdev("can't register radio device\n");
			return ret;
		}
		cx231xx_info("Registered radio device as %s\n",
			     video_device_node_name(dev->radio_dev));
	}

	cx231xx_info("V4L2 device registered as %s and %s\n",
		     video_device_node_name(dev->vdev),
		     video_device_node_name(dev->vbi_dev));

	return 0;
}
