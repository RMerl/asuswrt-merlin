/*
 * usbvision-core.c - driver for NT100x USB video capture devices
 *
 *
 * Copyright (c) 1999-2005 Joerg Heckenbach <joerg@heckenbach-aw.de>
 *                         Dwaine Garden <dwainegarden@rogers.com>
 *
 * This module is part of usbvision driver project.
 * Updates to driver completed by Dwaine P. Garden
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>

#include <media/saa7115.h>
#include <media/v4l2-common.h>
#include <media/tuner.h>

#include <linux/workqueue.h>

#include "usbvision.h"

static unsigned int core_debug;
module_param(core_debug, int, 0644);
MODULE_PARM_DESC(core_debug, "enable debug messages [core]");

static unsigned int force_testpattern;
module_param(force_testpattern, int, 0644);
MODULE_PARM_DESC(force_testpattern, "enable test pattern display [core]");

static int adjust_compression = 1;	/* Set the compression to be adaptive */
module_param(adjust_compression, int, 0444);
MODULE_PARM_DESC(adjust_compression, " Set the ADPCM compression for the device.  Default: 1 (On)");

/* To help people with Black and White output with using s-video input.
 * Some cables and input device are wired differently. */
static int switch_svideo_input;
module_param(switch_svideo_input, int, 0444);
MODULE_PARM_DESC(switch_svideo_input, " Set the S-Video input.  Some cables and input device are wired differently. Default: 0 (Off)");

static unsigned int adjust_x_offset = -1;
module_param(adjust_x_offset, int, 0644);
MODULE_PARM_DESC(adjust_x_offset, "adjust X offset display [core]");

static unsigned int adjust_y_offset = -1;
module_param(adjust_y_offset, int, 0644);
MODULE_PARM_DESC(adjust_y_offset, "adjust Y offset display [core]");


#define	ENABLE_HEXDUMP	0	/* Enable if you need it */


#ifdef USBVISION_DEBUG
	#define PDEBUG(level, fmt, args...) { \
		if (core_debug & (level)) \
			printk(KERN_INFO KBUILD_MODNAME ":[%s:%d] " fmt, \
				__func__, __LINE__ , ## args); \
	}
#else
	#define PDEBUG(level, fmt, args...) do {} while (0)
#endif

#define DBG_HEADER	(1 << 0)
#define DBG_IRQ		(1 << 1)
#define DBG_ISOC	(1 << 2)
#define DBG_PARSE	(1 << 3)
#define DBG_SCRATCH	(1 << 4)
#define DBG_FUNC	(1 << 5)

static const int max_imgwidth = MAX_FRAME_WIDTH;
static const int max_imgheight = MAX_FRAME_HEIGHT;
static const int min_imgwidth = MIN_FRAME_WIDTH;
static const int min_imgheight = MIN_FRAME_HEIGHT;

/* The value of 'scratch_buf_size' affects quality of the picture
 * in many ways. Shorter buffers may cause loss of data when client
 * is too slow. Larger buffers are memory-consuming and take longer
 * to work with. This setting can be adjusted, but the default value
 * should be OK for most desktop users.
 */
#define DEFAULT_SCRATCH_BUF_SIZE	(0x20000)		/* 128kB memory scratch buffer */
static const int scratch_buf_size = DEFAULT_SCRATCH_BUF_SIZE;

/* Function prototypes */
static int usbvision_request_intra(struct usb_usbvision *usbvision);
static int usbvision_unrequest_intra(struct usb_usbvision *usbvision);
static int usbvision_adjust_compression(struct usb_usbvision *usbvision);
static int usbvision_measure_bandwidth(struct usb_usbvision *usbvision);

/*******************************/
/* Memory management functions */
/*******************************/

/*
 * Here we want the physical address of the memory.
 * This is used when initializing the contents of the area.
 */

static void *usbvision_rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return mem;
}

static void usbvision_rvfree(void *mem, unsigned long size)
{
	unsigned long adr;

	if (!mem)
		return;

	size = PAGE_ALIGN(size);

	adr = (unsigned long) mem;
	while ((long) size > 0) {
		ClearPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	vfree(mem);
}


#if ENABLE_HEXDUMP
static void usbvision_hexdump(const unsigned char *data, int len)
{
	char tmp[80];
	int i, k;

	for (i = k = 0; len > 0; i++, len--) {
		if (i > 0 && (i % 16 == 0)) {
			printk("%s\n", tmp);
			k = 0;
		}
		k += sprintf(&tmp[k], "%02x ", data[i]);
	}
	if (k > 0)
		printk(KERN_CONT "%s\n", tmp);
}
#endif

/********************************
 * scratch ring buffer handling
 ********************************/
static int scratch_len(struct usb_usbvision *usbvision)    /* This returns the amount of data actually in the buffer */
{
	int len = usbvision->scratch_write_ptr - usbvision->scratch_read_ptr;

	if (len < 0)
		len += scratch_buf_size;
	PDEBUG(DBG_SCRATCH, "scratch_len() = %d\n", len);

	return len;
}


/* This returns the free space left in the buffer */
static int scratch_free(struct usb_usbvision *usbvision)
{
	int free = usbvision->scratch_read_ptr - usbvision->scratch_write_ptr;
	if (free <= 0)
		free += scratch_buf_size;
	if (free) {
		free -= 1;							/* at least one byte in the buffer must */
										/* left blank, otherwise there is no chance to differ between full and empty */
	}
	PDEBUG(DBG_SCRATCH, "return %d\n", free);

	return free;
}


/* This puts data into the buffer */
static int scratch_put(struct usb_usbvision *usbvision, unsigned char *data,
		       int len)
{
	int len_part;

	if (usbvision->scratch_write_ptr + len < scratch_buf_size) {
		memcpy(usbvision->scratch + usbvision->scratch_write_ptr, data, len);
		usbvision->scratch_write_ptr += len;
	} else {
		len_part = scratch_buf_size - usbvision->scratch_write_ptr;
		memcpy(usbvision->scratch + usbvision->scratch_write_ptr, data, len_part);
		if (len == len_part) {
			usbvision->scratch_write_ptr = 0;			/* just set write_ptr to zero */
		} else {
			memcpy(usbvision->scratch, data + len_part, len - len_part);
			usbvision->scratch_write_ptr = len - len_part;
		}
	}

	PDEBUG(DBG_SCRATCH, "len=%d, new write_ptr=%d\n", len, usbvision->scratch_write_ptr);

	return len;
}

/* This marks the write_ptr as position of new frame header */
static void scratch_mark_header(struct usb_usbvision *usbvision)
{
	PDEBUG(DBG_SCRATCH, "header at write_ptr=%d\n", usbvision->scratch_headermarker_write_ptr);

	usbvision->scratch_headermarker[usbvision->scratch_headermarker_write_ptr] =
				usbvision->scratch_write_ptr;
	usbvision->scratch_headermarker_write_ptr += 1;
	usbvision->scratch_headermarker_write_ptr %= USBVISION_NUM_HEADERMARKER;
}

/* This gets data from the buffer at the given "ptr" position */
static int scratch_get_extra(struct usb_usbvision *usbvision,
			     unsigned char *data, int *ptr, int len)
{
	int len_part;

	if (*ptr + len < scratch_buf_size) {
		memcpy(data, usbvision->scratch + *ptr, len);
		*ptr += len;
	} else {
		len_part = scratch_buf_size - *ptr;
		memcpy(data, usbvision->scratch + *ptr, len_part);
		if (len == len_part) {
			*ptr = 0;							/* just set the y_ptr to zero */
		} else {
			memcpy(data + len_part, usbvision->scratch, len - len_part);
			*ptr = len - len_part;
		}
	}

	PDEBUG(DBG_SCRATCH, "len=%d, new ptr=%d\n", len, *ptr);

	return len;
}


/* This sets the scratch extra read pointer */
static void scratch_set_extra_ptr(struct usb_usbvision *usbvision, int *ptr,
				  int len)
{
	*ptr = (usbvision->scratch_read_ptr + len) % scratch_buf_size;

	PDEBUG(DBG_SCRATCH, "ptr=%d\n", *ptr);
}


/* This increments the scratch extra read pointer */
static void scratch_inc_extra_ptr(int *ptr, int len)
{
	*ptr = (*ptr + len) % scratch_buf_size;

	PDEBUG(DBG_SCRATCH, "ptr=%d\n", *ptr);
}


/* This gets data from the buffer */
static int scratch_get(struct usb_usbvision *usbvision, unsigned char *data,
		       int len)
{
	int len_part;

	if (usbvision->scratch_read_ptr + len < scratch_buf_size) {
		memcpy(data, usbvision->scratch + usbvision->scratch_read_ptr, len);
		usbvision->scratch_read_ptr += len;
	} else {
		len_part = scratch_buf_size - usbvision->scratch_read_ptr;
		memcpy(data, usbvision->scratch + usbvision->scratch_read_ptr, len_part);
		if (len == len_part) {
			usbvision->scratch_read_ptr = 0;				/* just set the read_ptr to zero */
		} else {
			memcpy(data + len_part, usbvision->scratch, len - len_part);
			usbvision->scratch_read_ptr = len - len_part;
		}
	}

	PDEBUG(DBG_SCRATCH, "len=%d, new read_ptr=%d\n", len, usbvision->scratch_read_ptr);

	return len;
}


/* This sets read pointer to next header and returns it */
static int scratch_get_header(struct usb_usbvision *usbvision,
			      struct usbvision_frame_header *header)
{
	int err_code = 0;

	PDEBUG(DBG_SCRATCH, "from read_ptr=%d", usbvision->scratch_headermarker_read_ptr);

	while (usbvision->scratch_headermarker_write_ptr -
		usbvision->scratch_headermarker_read_ptr != 0) {
		usbvision->scratch_read_ptr =
			usbvision->scratch_headermarker[usbvision->scratch_headermarker_read_ptr];
		usbvision->scratch_headermarker_read_ptr += 1;
		usbvision->scratch_headermarker_read_ptr %= USBVISION_NUM_HEADERMARKER;
		scratch_get(usbvision, (unsigned char *)header, USBVISION_HEADER_LENGTH);
		if ((header->magic_1 == USBVISION_MAGIC_1)
			 && (header->magic_2 == USBVISION_MAGIC_2)
			 && (header->header_length == USBVISION_HEADER_LENGTH)) {
			err_code = USBVISION_HEADER_LENGTH;
			header->frame_width  = header->frame_width_lo  + (header->frame_width_hi << 8);
			header->frame_height = header->frame_height_lo + (header->frame_height_hi << 8);
			break;
		}
	}

	return err_code;
}


/* This removes len bytes of old data from the buffer */
static void scratch_rm_old(struct usb_usbvision *usbvision, int len)
{
	usbvision->scratch_read_ptr += len;
	usbvision->scratch_read_ptr %= scratch_buf_size;
	PDEBUG(DBG_SCRATCH, "read_ptr is now %d\n", usbvision->scratch_read_ptr);
}


/* This resets the buffer - kills all data in it too */
static void scratch_reset(struct usb_usbvision *usbvision)
{
	PDEBUG(DBG_SCRATCH, "\n");

	usbvision->scratch_read_ptr = 0;
	usbvision->scratch_write_ptr = 0;
	usbvision->scratch_headermarker_read_ptr = 0;
	usbvision->scratch_headermarker_write_ptr = 0;
	usbvision->isocstate = isoc_state_no_frame;
}

int usbvision_scratch_alloc(struct usb_usbvision *usbvision)
{
	usbvision->scratch = vmalloc_32(scratch_buf_size);
	scratch_reset(usbvision);
	if (usbvision->scratch == NULL) {
		dev_err(&usbvision->dev->dev,
			"%s: unable to allocate %d bytes for scratch\n",
				__func__, scratch_buf_size);
		return -ENOMEM;
	}
	return 0;
}

void usbvision_scratch_free(struct usb_usbvision *usbvision)
{
	vfree(usbvision->scratch);
	usbvision->scratch = NULL;
}

/*
 * usbvision_testpattern()
 *
 * Procedure forms a test pattern (yellow grid on blue background).
 *
 * Parameters:
 * fullframe:   if TRUE then entire frame is filled, otherwise the procedure
 *		continues from the current scanline.
 * pmode	0: fill the frame with solid blue color (like on VCR or TV)
 *		1: Draw a colored grid
 *
 */
static void usbvision_testpattern(struct usb_usbvision *usbvision,
				  int fullframe, int pmode)
{
	static const char proc[] = "usbvision_testpattern";
	struct usbvision_frame *frame;
	unsigned char *f;
	int num_cell = 0;
	int scan_length = 0;
	static int num_pass;

	if (usbvision == NULL) {
		printk(KERN_ERR "%s: usbvision == NULL\n", proc);
		return;
	}
	if (usbvision->cur_frame == NULL) {
		printk(KERN_ERR "%s: usbvision->cur_frame is NULL.\n", proc);
		return;
	}

	/* Grab the current frame */
	frame = usbvision->cur_frame;

	/* Optionally start at the beginning */
	if (fullframe) {
		frame->curline = 0;
		frame->scanlength = 0;
	}

	/* Form every scan line */
	for (; frame->curline < frame->frmheight; frame->curline++) {
		int i;

		f = frame->data + (usbvision->curwidth * 3 * frame->curline);
		for (i = 0; i < usbvision->curwidth; i++) {
			unsigned char cb = 0x80;
			unsigned char cg = 0;
			unsigned char cr = 0;

			if (pmode == 1) {
				if (frame->curline % 32 == 0)
					cb = 0, cg = cr = 0xFF;
				else if (i % 32 == 0) {
					if (frame->curline % 32 == 1)
						num_cell++;
					cb = 0, cg = cr = 0xFF;
				} else {
					cb =
					    ((num_cell * 7) +
					     num_pass) & 0xFF;
					cg =
					    ((num_cell * 5) +
					     num_pass * 2) & 0xFF;
					cr =
					    ((num_cell * 3) +
					     num_pass * 3) & 0xFF;
				}
			} else {
				/* Just the blue screen */
			}

			*f++ = cb;
			*f++ = cg;
			*f++ = cr;
			scan_length += 3;
		}
	}

	frame->grabstate = frame_state_done;
	frame->scanlength += scan_length;
	++num_pass;
}

/*
 * usbvision_decompress_alloc()
 *
 * allocates intermediate buffer for decompression
 */
int usbvision_decompress_alloc(struct usb_usbvision *usbvision)
{
	int IFB_size = MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT * 3 / 2;

	usbvision->intra_frame_buffer = vmalloc_32(IFB_size);
	if (usbvision->intra_frame_buffer == NULL) {
		dev_err(&usbvision->dev->dev,
			"%s: unable to allocate %d for compr. frame buffer\n",
				__func__, IFB_size);
		return -ENOMEM;
	}
	return 0;
}

/*
 * usbvision_decompress_free()
 *
 * frees intermediate buffer for decompression
 */
void usbvision_decompress_free(struct usb_usbvision *usbvision)
{
	vfree(usbvision->intra_frame_buffer);
	usbvision->intra_frame_buffer = NULL;

}

/************************************************************
 * Here comes the data parsing stuff that is run as interrupt
 ************************************************************/
/*
 * usbvision_find_header()
 *
 * Locate one of supported header markers in the scratch buffer.
 */
static enum parse_state usbvision_find_header(struct usb_usbvision *usbvision)
{
	struct usbvision_frame *frame;
	int found_header = 0;

	frame = usbvision->cur_frame;

	while (scratch_get_header(usbvision, &frame->isoc_header) == USBVISION_HEADER_LENGTH) {
		/* found header in scratch */
		PDEBUG(DBG_HEADER, "found header: 0x%02x%02x %d %d %d %d %#x 0x%02x %u %u",
				frame->isoc_header.magic_2,
				frame->isoc_header.magic_1,
				frame->isoc_header.header_length,
				frame->isoc_header.frame_num,
				frame->isoc_header.frame_phase,
				frame->isoc_header.frame_latency,
				frame->isoc_header.data_format,
				frame->isoc_header.format_param,
				frame->isoc_header.frame_width,
				frame->isoc_header.frame_height);

		if (usbvision->request_intra) {
			if (frame->isoc_header.format_param & 0x80) {
				found_header = 1;
				usbvision->last_isoc_frame_num = -1; /* do not check for lost frames this time */
				usbvision_unrequest_intra(usbvision);
				break;
			}
		} else {
			found_header = 1;
			break;
		}
	}

	if (found_header) {
		frame->frmwidth = frame->isoc_header.frame_width * usbvision->stretch_width;
		frame->frmheight = frame->isoc_header.frame_height * usbvision->stretch_height;
		frame->v4l2_linesize = (frame->frmwidth * frame->v4l2_format.depth) >> 3;
	} else { /* no header found */
		PDEBUG(DBG_HEADER, "skipping scratch data, no header");
		scratch_reset(usbvision);
		return parse_state_end_parse;
	}

	/* found header */
	if (frame->isoc_header.data_format == ISOC_MODE_COMPRESS) {
		/* check isoc_header.frame_num for lost frames */
		if (usbvision->last_isoc_frame_num >= 0) {
			if (((usbvision->last_isoc_frame_num + 1) % 32) != frame->isoc_header.frame_num) {
				/* unexpected frame drop: need to request new intra frame */
				PDEBUG(DBG_HEADER, "Lost frame before %d on USB", frame->isoc_header.frame_num);
				usbvision_request_intra(usbvision);
				return parse_state_next_frame;
			}
		}
		usbvision->last_isoc_frame_num = frame->isoc_header.frame_num;
	}
	usbvision->header_count++;
	frame->scanstate = scan_state_lines;
	frame->curline = 0;

	if (force_testpattern) {
		usbvision_testpattern(usbvision, 1, 1);
		return parse_state_next_frame;
	}
	return parse_state_continue;
}

static enum parse_state usbvision_parse_lines_422(struct usb_usbvision *usbvision,
					   long *pcopylen)
{
	volatile struct usbvision_frame *frame;
	unsigned char *f;
	int len;
	int i;
	unsigned char yuyv[4] = { 180, 128, 10, 128 }; /* YUV components */
	unsigned char rv, gv, bv;	/* RGB components */
	int clipmask_index, bytes_per_pixel;
	int stretch_bytes, clipmask_add;

	frame  = usbvision->cur_frame;
	f = frame->data + (frame->v4l2_linesize * frame->curline);

	/* Make sure there's enough data for the entire line */
	len = (frame->isoc_header.frame_width * 2) + 5;
	if (scratch_len(usbvision) < len) {
		PDEBUG(DBG_PARSE, "out of data in line %d, need %u.\n", frame->curline, len);
		return parse_state_out;
	}

	if ((frame->curline + 1) >= frame->frmheight)
		return parse_state_next_frame;

	bytes_per_pixel = frame->v4l2_format.bytes_per_pixel;
	stretch_bytes = (usbvision->stretch_width - 1) * bytes_per_pixel;
	clipmask_index = frame->curline * MAX_FRAME_WIDTH;
	clipmask_add = usbvision->stretch_width;

	for (i = 0; i < frame->frmwidth; i += (2 * usbvision->stretch_width)) {
		scratch_get(usbvision, &yuyv[0], 4);

		if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
			*f++ = yuyv[0]; /* Y */
			*f++ = yuyv[3]; /* U */
		} else {
			YUV_TO_RGB_BY_THE_BOOK(yuyv[0], yuyv[1], yuyv[3], rv, gv, bv);
			switch (frame->v4l2_format.format) {
			case V4L2_PIX_FMT_RGB565:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x07 & (gv >> 3)) |
					(0xF8 &  bv);
				break;
			case V4L2_PIX_FMT_RGB24:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				break;
			case V4L2_PIX_FMT_RGB32:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				f++;
				break;
			case V4L2_PIX_FMT_RGB555:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x03 & (gv >> 3)) |
					(0x7C & (bv << 2));
				break;
			}
		}
		clipmask_index += clipmask_add;
		f += stretch_bytes;

		if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
			*f++ = yuyv[2]; /* Y */
			*f++ = yuyv[1]; /* V */
		} else {
			YUV_TO_RGB_BY_THE_BOOK(yuyv[2], yuyv[1], yuyv[3], rv, gv, bv);
			switch (frame->v4l2_format.format) {
			case V4L2_PIX_FMT_RGB565:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x07 & (gv >> 3)) |
					(0xF8 &  bv);
				break;
			case V4L2_PIX_FMT_RGB24:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				break;
			case V4L2_PIX_FMT_RGB32:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				f++;
				break;
			case V4L2_PIX_FMT_RGB555:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x03 & (gv >> 3)) |
					(0x7C & (bv << 2));
				break;
			}
		}
		clipmask_index += clipmask_add;
		f += stretch_bytes;
	}

	frame->curline += usbvision->stretch_height;
	*pcopylen += frame->v4l2_linesize * usbvision->stretch_height;

	if (frame->curline >= frame->frmheight)
		return parse_state_next_frame;
	return parse_state_continue;
}

/* The decompression routine  */
static int usbvision_decompress(struct usb_usbvision *usbvision, unsigned char *compressed,
								unsigned char *decompressed, int *start_pos,
								int *block_typestart_pos, int len)
{
	int rest_pixel, idx, max_pos, pos, extra_pos, block_len, block_type_pos, block_type_len;
	unsigned char block_byte, block_code, block_type, block_type_byte, integrator;

	integrator = 0;
	pos = *start_pos;
	block_type_pos = *block_typestart_pos;
	max_pos = 396; /* pos + len; */
	extra_pos = pos;
	block_len = 0;
	block_byte = 0;
	block_code = 0;
	block_type = 0;
	block_type_byte = 0;
	block_type_len = 0;
	rest_pixel = len;

	for (idx = 0; idx < len; idx++) {
		if (block_len == 0) {
			if (block_type_len == 0) {
				block_type_byte = compressed[block_type_pos];
				block_type_pos++;
				block_type_len = 4;
			}
			block_type = (block_type_byte & 0xC0) >> 6;

			/* statistic: */
			usbvision->compr_block_types[block_type]++;

			pos = extra_pos;
			if (block_type == 0) {
				if (rest_pixel >= 24) {
					idx += 23;
					rest_pixel -= 24;
					integrator = decompressed[idx];
				} else {
					idx += rest_pixel - 1;
					rest_pixel = 0;
				}
			} else {
				block_code = compressed[pos];
				pos++;
				if (rest_pixel >= 24)
					block_len  = 24;
				else
					block_len = rest_pixel;
				rest_pixel -= block_len;
				extra_pos = pos + (block_len / 4);
			}
			block_type_byte <<= 2;
			block_type_len -= 1;
		}
		if (block_len > 0) {
			if ((block_len % 4) == 0) {
				block_byte = compressed[pos];
				pos++;
			}
			if (block_type == 1) /* inter Block */
				integrator = decompressed[idx];
			switch (block_byte & 0xC0) {
			case 0x03 << 6:
				integrator += compressed[extra_pos];
				extra_pos++;
				break;
			case 0x02 << 6:
				integrator += block_code;
				break;
			case 0x00:
				integrator -= block_code;
				break;
			}
			decompressed[idx] = integrator;
			block_byte <<= 2;
			block_len -= 1;
		}
	}
	*start_pos = extra_pos;
	*block_typestart_pos = block_type_pos;
	return idx;
}


/*
 * usbvision_parse_compress()
 *
 * Parse compressed frame from the scratch buffer, put
 * decoded RGB value into the current frame buffer and add the written
 * number of bytes (RGB) to the *pcopylen.
 *
 */
static enum parse_state usbvision_parse_compress(struct usb_usbvision *usbvision,
					   long *pcopylen)
{
#define USBVISION_STRIP_MAGIC		0x5A
#define USBVISION_STRIP_LEN_MAX		400
#define USBVISION_STRIP_HEADER_LEN	3

	struct usbvision_frame *frame;
	unsigned char *f, *u = NULL, *v = NULL;
	unsigned char strip_data[USBVISION_STRIP_LEN_MAX];
	unsigned char strip_header[USBVISION_STRIP_HEADER_LEN];
	int idx, idx_end, strip_len, strip_ptr, startblock_pos, block_pos, block_type_pos;
	int clipmask_index, bytes_per_pixel, rc;
	int image_size;
	unsigned char rv, gv, bv;
	static unsigned char *Y, *U, *V;

	frame = usbvision->cur_frame;
	image_size = frame->frmwidth * frame->frmheight;
	if ((frame->v4l2_format.format == V4L2_PIX_FMT_YUV422P) ||
	    (frame->v4l2_format.format == V4L2_PIX_FMT_YVU420)) {       /* this is a planar format */
		/* ... v4l2_linesize not used here. */
		f = frame->data + (frame->width * frame->curline);
	} else
		f = frame->data + (frame->v4l2_linesize * frame->curline);

	if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) { /* initialise u and v pointers */
		/* get base of u and b planes add halfoffset */
		u = frame->data
			+ image_size
			+ (frame->frmwidth >> 1) * frame->curline;
		v = u + (image_size >> 1);
	} else if (frame->v4l2_format.format == V4L2_PIX_FMT_YVU420) {
		v = frame->data + image_size + ((frame->curline * (frame->width)) >> 2);
		u = v + (image_size >> 2);
	}

	if (frame->curline == 0)
		usbvision_adjust_compression(usbvision);

	if (scratch_len(usbvision) < USBVISION_STRIP_HEADER_LEN)
		return parse_state_out;

	/* get strip header without changing the scratch_read_ptr */
	scratch_set_extra_ptr(usbvision, &strip_ptr, 0);
	scratch_get_extra(usbvision, &strip_header[0], &strip_ptr,
				USBVISION_STRIP_HEADER_LEN);

	if (strip_header[0] != USBVISION_STRIP_MAGIC) {
		/* wrong strip magic */
		usbvision->strip_magic_errors++;
		return parse_state_next_frame;
	}

	if (frame->curline != (int)strip_header[2]) {
		/* line number mismatch error */
		usbvision->strip_line_number_errors++;
	}

	strip_len = 2 * (unsigned int)strip_header[1];
	if (strip_len > USBVISION_STRIP_LEN_MAX) {
		/* strip overrun */
		/* I think this never happens */
		usbvision_request_intra(usbvision);
	}

	if (scratch_len(usbvision) < strip_len) {
		/* there is not enough data for the strip */
		return parse_state_out;
	}

	if (usbvision->intra_frame_buffer) {
		Y = usbvision->intra_frame_buffer + frame->frmwidth * frame->curline;
		U = usbvision->intra_frame_buffer + image_size + (frame->frmwidth / 2) * (frame->curline / 2);
		V = usbvision->intra_frame_buffer + image_size / 4 * 5 + (frame->frmwidth / 2) * (frame->curline / 2);
	} else {
		return parse_state_next_frame;
	}

	bytes_per_pixel = frame->v4l2_format.bytes_per_pixel;
	clipmask_index = frame->curline * MAX_FRAME_WIDTH;

	scratch_get(usbvision, strip_data, strip_len);

	idx_end = frame->frmwidth;
	block_type_pos = USBVISION_STRIP_HEADER_LEN;
	startblock_pos = block_type_pos + (idx_end - 1) / 96 + (idx_end / 2 - 1) / 96 + 2;
	block_pos = startblock_pos;

	usbvision->block_pos = block_pos;

	rc = usbvision_decompress(usbvision, strip_data, Y, &block_pos, &block_type_pos, idx_end);
	if (strip_len > usbvision->max_strip_len)
		usbvision->max_strip_len = strip_len;

	if (frame->curline % 2)
		rc = usbvision_decompress(usbvision, strip_data, V, &block_pos, &block_type_pos, idx_end / 2);
	else
		rc = usbvision_decompress(usbvision, strip_data, U, &block_pos, &block_type_pos, idx_end / 2);

	if (block_pos > usbvision->comprblock_pos)
		usbvision->comprblock_pos = block_pos;
	if (block_pos > strip_len)
		usbvision->strip_len_errors++;

	for (idx = 0; idx < idx_end; idx++) {
		if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
			*f++ = Y[idx];
			*f++ = idx & 0x01 ? U[idx / 2] : V[idx / 2];
		} else if (frame->v4l2_format.format == V4L2_PIX_FMT_YUV422P) {
			*f++ = Y[idx];
			if (idx & 0x01)
				*u++ = U[idx >> 1];
			else
				*v++ = V[idx >> 1];
		} else if (frame->v4l2_format.format == V4L2_PIX_FMT_YVU420) {
			*f++ = Y[idx];
			if (!((idx & 0x01) | (frame->curline & 0x01))) {
				/* only need do this for 1 in 4 pixels */
				/* intraframe buffer is YUV420 format */
				*u++ = U[idx >> 1];
				*v++ = V[idx >> 1];
			}
		} else {
			YUV_TO_RGB_BY_THE_BOOK(Y[idx], U[idx / 2], V[idx / 2], rv, gv, bv);
			switch (frame->v4l2_format.format) {
			case V4L2_PIX_FMT_GREY:
				*f++ = Y[idx];
				break;
			case V4L2_PIX_FMT_RGB555:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x03 & (gv >> 3)) |
					(0x7C & (bv << 2));
				break;
			case V4L2_PIX_FMT_RGB565:
				*f++ = (0x1F & rv) |
					(0xE0 & (gv << 5));
				*f++ = (0x07 & (gv >> 3)) |
					(0xF8 & bv);
				break;
			case V4L2_PIX_FMT_RGB24:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				break;
			case V4L2_PIX_FMT_RGB32:
				*f++ = rv;
				*f++ = gv;
				*f++ = bv;
				f++;
				break;
			}
		}
		clipmask_index++;
	}
	/* Deal with non-integer no. of bytes for YUV420P */
	if (frame->v4l2_format.format != V4L2_PIX_FMT_YVU420)
		*pcopylen += frame->v4l2_linesize;
	else
		*pcopylen += frame->curline & 0x01 ? frame->v4l2_linesize : frame->v4l2_linesize << 1;

	frame->curline += 1;

	if (frame->curline >= frame->frmheight)
		return parse_state_next_frame;
	return parse_state_continue;

}


/*
 * usbvision_parse_lines_420()
 *
 * Parse two lines from the scratch buffer, put
 * decoded RGB value into the current frame buffer and add the written
 * number of bytes (RGB) to the *pcopylen.
 *
 */
static enum parse_state usbvision_parse_lines_420(struct usb_usbvision *usbvision,
					   long *pcopylen)
{
	struct usbvision_frame *frame;
	unsigned char *f_even = NULL, *f_odd = NULL;
	unsigned int pixel_per_line, block;
	int pixel, block_split;
	int y_ptr, u_ptr, v_ptr, y_odd_offset;
	const int y_block_size = 128;
	const int uv_block_size = 64;
	const int sub_block_size = 32;
	const int y_step[] = { 0, 0, 0, 2 }, y_step_size = 4;
	const int uv_step[] = { 0, 0, 0, 4 }, uv_step_size = 4;
	unsigned char y[2], u, v;	/* YUV components */
	int y_, u_, v_, vb, uvg, ur;
	int r_, g_, b_;			/* RGB components */
	unsigned char g;
	int clipmask_even_index, clipmask_odd_index, bytes_per_pixel;
	int clipmask_add, stretch_bytes;

	frame  = usbvision->cur_frame;
	f_even = frame->data + (frame->v4l2_linesize * frame->curline);
	f_odd  = f_even + frame->v4l2_linesize * usbvision->stretch_height;

	/* Make sure there's enough data for the entire line */
	/* In this mode usbvision transfer 3 bytes for every 2 pixels */
	/* I need two lines to decode the color */
	bytes_per_pixel = frame->v4l2_format.bytes_per_pixel;
	stretch_bytes = (usbvision->stretch_width - 1) * bytes_per_pixel;
	clipmask_even_index = frame->curline * MAX_FRAME_WIDTH;
	clipmask_odd_index  = clipmask_even_index + MAX_FRAME_WIDTH;
	clipmask_add = usbvision->stretch_width;
	pixel_per_line = frame->isoc_header.frame_width;

	if (scratch_len(usbvision) < (int)pixel_per_line * 3) {
		/* printk(KERN_DEBUG "out of data, need %d\n", len); */
		return parse_state_out;
	}

	if ((frame->curline + 1) >= frame->frmheight)
		return parse_state_next_frame;

	block_split = (pixel_per_line%y_block_size) ? 1 : 0;	/* are some blocks splitted into different lines? */

	y_odd_offset = (pixel_per_line / y_block_size) * (y_block_size + uv_block_size)
			+ block_split * uv_block_size;

	scratch_set_extra_ptr(usbvision, &y_ptr, y_odd_offset);
	scratch_set_extra_ptr(usbvision, &u_ptr, y_block_size);
	scratch_set_extra_ptr(usbvision, &v_ptr, y_odd_offset
			+ (4 - block_split) * sub_block_size);

	for (block = 0; block < (pixel_per_line / sub_block_size); block++) {
		for (pixel = 0; pixel < sub_block_size; pixel += 2) {
			scratch_get(usbvision, &y[0], 2);
			scratch_get_extra(usbvision, &u, &u_ptr, 1);
			scratch_get_extra(usbvision, &v, &v_ptr, 1);

			/* I don't use the YUV_TO_RGB macro for better performance */
			v_ = v - 128;
			u_ = u - 128;
			vb = 132252 * v_;
			uvg = -53281 * u_ - 25625 * v_;
			ur = 104595 * u_;

			if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
				*f_even++ = y[0];
				*f_even++ = v;
			} else {
				y_ = 76284 * (y[0] - 16);

				b_ = (y_ + vb) >> 16;
				g_ = (y_ + uvg) >> 16;
				r_ = (y_ + ur) >> 16;

				switch (frame->v4l2_format.format) {
				case V4L2_PIX_FMT_RGB565:
					g = LIMIT_RGB(g_);
					*f_even++ =
						(0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_even++ =
						(0x07 & (g >> 3)) |
						(0xF8 &  LIMIT_RGB(b_));
					break;
				case V4L2_PIX_FMT_RGB24:
					*f_even++ = LIMIT_RGB(r_);
					*f_even++ = LIMIT_RGB(g_);
					*f_even++ = LIMIT_RGB(b_);
					break;
				case V4L2_PIX_FMT_RGB32:
					*f_even++ = LIMIT_RGB(r_);
					*f_even++ = LIMIT_RGB(g_);
					*f_even++ = LIMIT_RGB(b_);
					f_even++;
					break;
				case V4L2_PIX_FMT_RGB555:
					g = LIMIT_RGB(g_);
					*f_even++ = (0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_even++ = (0x03 & (g >> 3)) |
						(0x7C & (LIMIT_RGB(b_) << 2));
					break;
				}
			}
			clipmask_even_index += clipmask_add;
			f_even += stretch_bytes;

			if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
				*f_even++ = y[1];
				*f_even++ = u;
			} else {
				y_ = 76284 * (y[1] - 16);

				b_ = (y_ + vb) >> 16;
				g_ = (y_ + uvg) >> 16;
				r_ = (y_ + ur) >> 16;

				switch (frame->v4l2_format.format) {
				case V4L2_PIX_FMT_RGB565:
					g = LIMIT_RGB(g_);
					*f_even++ =
						(0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_even++ =
						(0x07 & (g >> 3)) |
						(0xF8 &  LIMIT_RGB(b_));
					break;
				case V4L2_PIX_FMT_RGB24:
					*f_even++ = LIMIT_RGB(r_);
					*f_even++ = LIMIT_RGB(g_);
					*f_even++ = LIMIT_RGB(b_);
					break;
				case V4L2_PIX_FMT_RGB32:
					*f_even++ = LIMIT_RGB(r_);
					*f_even++ = LIMIT_RGB(g_);
					*f_even++ = LIMIT_RGB(b_);
					f_even++;
					break;
				case V4L2_PIX_FMT_RGB555:
					g = LIMIT_RGB(g_);
					*f_even++ = (0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_even++ = (0x03 & (g >> 3)) |
						(0x7C & (LIMIT_RGB(b_) << 2));
					break;
				}
			}
			clipmask_even_index += clipmask_add;
			f_even += stretch_bytes;

			scratch_get_extra(usbvision, &y[0], &y_ptr, 2);

			if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
				*f_odd++ = y[0];
				*f_odd++ = v;
			} else {
				y_ = 76284 * (y[0] - 16);

				b_ = (y_ + vb) >> 16;
				g_ = (y_ + uvg) >> 16;
				r_ = (y_ + ur) >> 16;

				switch (frame->v4l2_format.format) {
				case V4L2_PIX_FMT_RGB565:
					g = LIMIT_RGB(g_);
					*f_odd++ =
						(0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_odd++ =
						(0x07 & (g >> 3)) |
						(0xF8 &  LIMIT_RGB(b_));
					break;
				case V4L2_PIX_FMT_RGB24:
					*f_odd++ = LIMIT_RGB(r_);
					*f_odd++ = LIMIT_RGB(g_);
					*f_odd++ = LIMIT_RGB(b_);
					break;
				case V4L2_PIX_FMT_RGB32:
					*f_odd++ = LIMIT_RGB(r_);
					*f_odd++ = LIMIT_RGB(g_);
					*f_odd++ = LIMIT_RGB(b_);
					f_odd++;
					break;
				case V4L2_PIX_FMT_RGB555:
					g = LIMIT_RGB(g_);
					*f_odd++ = (0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_odd++ = (0x03 & (g >> 3)) |
						(0x7C & (LIMIT_RGB(b_) << 2));
					break;
				}
			}
			clipmask_odd_index += clipmask_add;
			f_odd += stretch_bytes;

			if (frame->v4l2_format.format == V4L2_PIX_FMT_YUYV) {
				*f_odd++ = y[1];
				*f_odd++ = u;
			} else {
				y_ = 76284 * (y[1] - 16);

				b_ = (y_ + vb) >> 16;
				g_ = (y_ + uvg) >> 16;
				r_ = (y_ + ur) >> 16;

				switch (frame->v4l2_format.format) {
				case V4L2_PIX_FMT_RGB565:
					g = LIMIT_RGB(g_);
					*f_odd++ =
						(0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_odd++ =
						(0x07 & (g >> 3)) |
						(0xF8 &  LIMIT_RGB(b_));
					break;
				case V4L2_PIX_FMT_RGB24:
					*f_odd++ = LIMIT_RGB(r_);
					*f_odd++ = LIMIT_RGB(g_);
					*f_odd++ = LIMIT_RGB(b_);
					break;
				case V4L2_PIX_FMT_RGB32:
					*f_odd++ = LIMIT_RGB(r_);
					*f_odd++ = LIMIT_RGB(g_);
					*f_odd++ = LIMIT_RGB(b_);
					f_odd++;
					break;
				case V4L2_PIX_FMT_RGB555:
					g = LIMIT_RGB(g_);
					*f_odd++ = (0x1F & LIMIT_RGB(r_)) |
						(0xE0 & (g << 5));
					*f_odd++ = (0x03 & (g >> 3)) |
						(0x7C & (LIMIT_RGB(b_) << 2));
					break;
				}
			}
			clipmask_odd_index += clipmask_add;
			f_odd += stretch_bytes;
		}

		scratch_rm_old(usbvision, y_step[block % y_step_size] * sub_block_size);
		scratch_inc_extra_ptr(&y_ptr, y_step[(block + 2 * block_split) % y_step_size]
				* sub_block_size);
		scratch_inc_extra_ptr(&u_ptr, uv_step[block % uv_step_size]
				* sub_block_size);
		scratch_inc_extra_ptr(&v_ptr, uv_step[(block + 2 * block_split) % uv_step_size]
				* sub_block_size);
	}

	scratch_rm_old(usbvision, pixel_per_line * 3 / 2
			+ block_split * sub_block_size);

	frame->curline += 2 * usbvision->stretch_height;
	*pcopylen += frame->v4l2_linesize * 2 * usbvision->stretch_height;

	if (frame->curline >= frame->frmheight)
		return parse_state_next_frame;
	return parse_state_continue;
}

/*
 * usbvision_parse_data()
 *
 * Generic routine to parse the scratch buffer. It employs either
 * usbvision_find_header() or usbvision_parse_lines() to do most
 * of work.
 *
 */
static void usbvision_parse_data(struct usb_usbvision *usbvision)
{
	struct usbvision_frame *frame;
	enum parse_state newstate;
	long copylen = 0;
	unsigned long lock_flags;

	frame = usbvision->cur_frame;

	PDEBUG(DBG_PARSE, "parsing len=%d\n", scratch_len(usbvision));

	while (1) {
		newstate = parse_state_out;
		if (scratch_len(usbvision)) {
			if (frame->scanstate == scan_state_scanning) {
				newstate = usbvision_find_header(usbvision);
			} else if (frame->scanstate == scan_state_lines) {
				if (usbvision->isoc_mode == ISOC_MODE_YUV420)
					newstate = usbvision_parse_lines_420(usbvision, &copylen);
				else if (usbvision->isoc_mode == ISOC_MODE_YUV422)
					newstate = usbvision_parse_lines_422(usbvision, &copylen);
				else if (usbvision->isoc_mode == ISOC_MODE_COMPRESS)
					newstate = usbvision_parse_compress(usbvision, &copylen);
			}
		}
		if (newstate == parse_state_continue)
			continue;
		if ((newstate == parse_state_next_frame) || (newstate == parse_state_out))
			break;
		return;	/* parse_state_end_parse */
	}

	if (newstate == parse_state_next_frame) {
		frame->grabstate = frame_state_done;
		do_gettimeofday(&(frame->timestamp));
		frame->sequence = usbvision->frame_num;

		spin_lock_irqsave(&usbvision->queue_lock, lock_flags);
		list_move_tail(&(frame->frame), &usbvision->outqueue);
		usbvision->cur_frame = NULL;
		spin_unlock_irqrestore(&usbvision->queue_lock, lock_flags);

		usbvision->frame_num++;

		/* This will cause the process to request another frame. */
		if (waitqueue_active(&usbvision->wait_frame)) {
			PDEBUG(DBG_PARSE, "Wake up !");
			wake_up_interruptible(&usbvision->wait_frame);
		}
	} else {
		frame->grabstate = frame_state_grabbing;
	}

	/* Update the frame's uncompressed length. */
	frame->scanlength += copylen;
}


/*
 * Make all of the blocks of data contiguous
 */
static int usbvision_compress_isochronous(struct usb_usbvision *usbvision,
					  struct urb *urb)
{
	unsigned char *packet_data;
	int i, totlen = 0;

	for (i = 0; i < urb->number_of_packets; i++) {
		int packet_len = urb->iso_frame_desc[i].actual_length;
		int packet_stat = urb->iso_frame_desc[i].status;

		packet_data = urb->transfer_buffer + urb->iso_frame_desc[i].offset;

		/* Detect and ignore errored packets */
		if (packet_stat) {	/* packet_stat != 0 ????????????? */
			PDEBUG(DBG_ISOC, "data error: [%d] len=%d, status=%X", i, packet_len, packet_stat);
			usbvision->isoc_err_count++;
			continue;
		}

		/* Detect and ignore empty packets */
		if (packet_len < 0) {
			PDEBUG(DBG_ISOC, "error packet [%d]", i);
			usbvision->isoc_skip_count++;
			continue;
		} else if (packet_len == 0) {	/* Frame end ????? */
			PDEBUG(DBG_ISOC, "null packet [%d]", i);
			usbvision->isocstate = isoc_state_no_frame;
			usbvision->isoc_skip_count++;
			continue;
		} else if (packet_len > usbvision->isoc_packet_size) {
			PDEBUG(DBG_ISOC, "packet[%d] > isoc_packet_size", i);
			usbvision->isoc_skip_count++;
			continue;
		}

		PDEBUG(DBG_ISOC, "packet ok [%d] len=%d", i, packet_len);

		if (usbvision->isocstate == isoc_state_no_frame) { /* new frame begins */
			usbvision->isocstate = isoc_state_in_frame;
			scratch_mark_header(usbvision);
			usbvision_measure_bandwidth(usbvision);
			PDEBUG(DBG_ISOC, "packet with header");
		}

		/*
		 * If usbvision continues to feed us with data but there is no
		 * consumption (if, for example, V4L client fell asleep) we
		 * may overflow the buffer. We have to move old data over to
		 * free room for new data. This is bad for old data. If we
		 * just drop new data then it's bad for new data... choose
		 * your favorite evil here.
		 */
		if (scratch_free(usbvision) < packet_len) {
			usbvision->scratch_ovf_count++;
			PDEBUG(DBG_ISOC, "scratch buf overflow! scr_len: %d, n: %d",
			       scratch_len(usbvision), packet_len);
			scratch_rm_old(usbvision, packet_len - scratch_free(usbvision));
		}

		/* Now we know that there is enough room in scratch buffer */
		scratch_put(usbvision, packet_data, packet_len);
		totlen += packet_len;
		usbvision->isoc_data_count += packet_len;
		usbvision->isoc_packet_count++;
	}
#if ENABLE_HEXDUMP
	if (totlen > 0) {
		static int foo;

		if (foo < 1) {
			printk(KERN_DEBUG "+%d.\n", usbvision->scratchlen);
			usbvision_hexdump(data0, (totlen > 64) ? 64 : totlen);
			++foo;
		}
	}
#endif
	return totlen;
}

static void usbvision_isoc_irq(struct urb *urb)
{
	int err_code = 0;
	int len;
	struct usb_usbvision *usbvision = urb->context;
	int i;
	unsigned long start_time = jiffies;
	struct usbvision_frame **f;

	/* We don't want to do anything if we are about to be removed! */
	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return;

	/* any urb with wrong status is ignored without acknowledgement */
	if (urb->status == -ENOENT)
		return;

	f = &usbvision->cur_frame;

	/* Manage streaming interruption */
	if (usbvision->streaming == stream_interrupt) {
		usbvision->streaming = stream_idle;
		if ((*f)) {
			(*f)->grabstate = frame_state_ready;
			(*f)->scanstate = scan_state_scanning;
		}
		PDEBUG(DBG_IRQ, "stream interrupted");
		wake_up_interruptible(&usbvision->wait_stream);
	}

	/* Copy the data received into our scratch buffer */
	len = usbvision_compress_isochronous(usbvision, urb);

	usbvision->isoc_urb_count++;
	usbvision->urb_length = len;

	if (usbvision->streaming == stream_on) {
		/* If we collected enough data let's parse! */
		if (scratch_len(usbvision) > USBVISION_HEADER_LENGTH &&
		    !list_empty(&(usbvision->inqueue))) {
			if (!(*f)) {
				(*f) = list_entry(usbvision->inqueue.next,
						  struct usbvision_frame,
						  frame);
			}
			usbvision_parse_data(usbvision);
		} else {
			/* If we don't have a frame
			  we're current working on, complain */
			PDEBUG(DBG_IRQ,
			       "received data, but no one needs it");
			scratch_reset(usbvision);
		}
	} else {
		PDEBUG(DBG_IRQ, "received data, but no one needs it");
		scratch_reset(usbvision);
	}

	usbvision->time_in_irq += jiffies - start_time;

	for (i = 0; i < USBVISION_URB_FRAMES; i++) {
		urb->iso_frame_desc[i].status = 0;
		urb->iso_frame_desc[i].actual_length = 0;
	}

	urb->status = 0;
	urb->dev = usbvision->dev;
	err_code = usb_submit_urb(urb, GFP_ATOMIC);

	if (err_code) {
		dev_err(&usbvision->dev->dev,
			"%s: usb_submit_urb failed: error %d\n",
				__func__, err_code);
	}

	return;
}

/*************************************/
/* Low level usbvision access functions */
/*************************************/

/*
 * usbvision_read_reg()
 *
 * return  < 0 -> Error
 *        >= 0 -> Data
 */

int usbvision_read_reg(struct usb_usbvision *usbvision, unsigned char reg)
{
	int err_code = 0;
	unsigned char buffer[1];

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return -1;

	err_code = usb_control_msg(usbvision->dev, usb_rcvctrlpipe(usbvision->dev, 1),
				USBVISION_OP_CODE,
				USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
				0, (__u16) reg, buffer, 1, HZ);

	if (err_code < 0) {
		dev_err(&usbvision->dev->dev,
			"%s: failed: error %d\n", __func__, err_code);
		return err_code;
	}
	return buffer[0];
}

/*
 * usbvision_write_reg()
 *
 * return 1 -> Reg written
 *        0 -> usbvision is not yet ready
 *       -1 -> Something went wrong
 */

int usbvision_write_reg(struct usb_usbvision *usbvision, unsigned char reg,
			    unsigned char value)
{
	int err_code = 0;

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	err_code = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
				USBVISION_OP_CODE,
				USB_DIR_OUT | USB_TYPE_VENDOR |
				USB_RECIP_ENDPOINT, 0, (__u16) reg, &value, 1, HZ);

	if (err_code < 0) {
		dev_err(&usbvision->dev->dev,
			"%s: failed: error %d\n", __func__, err_code);
	}
	return err_code;
}


static void usbvision_ctrl_urb_complete(struct urb *urb)
{
	struct usb_usbvision *usbvision = (struct usb_usbvision *)urb->context;

	PDEBUG(DBG_IRQ, "");
	usbvision->ctrl_urb_busy = 0;
	if (waitqueue_active(&usbvision->ctrl_urb_wq))
		wake_up_interruptible(&usbvision->ctrl_urb_wq);
}


static int usbvision_write_reg_irq(struct usb_usbvision *usbvision, int address,
				unsigned char *data, int len)
{
	int err_code = 0;

	PDEBUG(DBG_IRQ, "");
	if (len > 8)
		return -EFAULT;
	if (usbvision->ctrl_urb_busy)
		return -EBUSY;
	usbvision->ctrl_urb_busy = 1;

	usbvision->ctrl_urb_setup.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT;
	usbvision->ctrl_urb_setup.bRequest     = USBVISION_OP_CODE;
	usbvision->ctrl_urb_setup.wValue       = 0;
	usbvision->ctrl_urb_setup.wIndex       = cpu_to_le16(address);
	usbvision->ctrl_urb_setup.wLength      = cpu_to_le16(len);
	usb_fill_control_urb(usbvision->ctrl_urb, usbvision->dev,
							usb_sndctrlpipe(usbvision->dev, 1),
							(unsigned char *)&usbvision->ctrl_urb_setup,
							(void *)usbvision->ctrl_urb_buffer, len,
							usbvision_ctrl_urb_complete,
							(void *)usbvision);

	memcpy(usbvision->ctrl_urb_buffer, data, len);

	err_code = usb_submit_urb(usbvision->ctrl_urb, GFP_ATOMIC);
	if (err_code < 0) {
		/* error in usb_submit_urb() */
		usbvision->ctrl_urb_busy = 0;
	}
	PDEBUG(DBG_IRQ, "submit %d byte: error %d", len, err_code);
	return err_code;
}


static int usbvision_init_compression(struct usb_usbvision *usbvision)
{
	int err_code = 0;

	usbvision->last_isoc_frame_num = -1;
	usbvision->isoc_data_count = 0;
	usbvision->isoc_packet_count = 0;
	usbvision->isoc_skip_count = 0;
	usbvision->compr_level = 50;
	usbvision->last_compr_level = -1;
	usbvision->isoc_urb_count = 0;
	usbvision->request_intra = 1;
	usbvision->isoc_measure_bandwidth_count = 0;

	return err_code;
}

/* this function measures the used bandwidth since last call
 * return:    0 : no error
 * sets used_bandwidth to 1-100 : 1-100% of full bandwidth resp. to isoc_packet_size
 */
static int usbvision_measure_bandwidth(struct usb_usbvision *usbvision)
{
	int err_code = 0;

	if (usbvision->isoc_measure_bandwidth_count < 2) { /* this gives an average bandwidth of 3 frames */
		usbvision->isoc_measure_bandwidth_count++;
		return err_code;
	}
	if ((usbvision->isoc_packet_size > 0) && (usbvision->isoc_packet_count > 0)) {
		usbvision->used_bandwidth = usbvision->isoc_data_count /
					(usbvision->isoc_packet_count + usbvision->isoc_skip_count) *
					100 / usbvision->isoc_packet_size;
	}
	usbvision->isoc_measure_bandwidth_count = 0;
	usbvision->isoc_data_count = 0;
	usbvision->isoc_packet_count = 0;
	usbvision->isoc_skip_count = 0;
	return err_code;
}

static int usbvision_adjust_compression(struct usb_usbvision *usbvision)
{
	int err_code = 0;
	unsigned char buffer[6];

	PDEBUG(DBG_IRQ, "");
	if ((adjust_compression) && (usbvision->used_bandwidth > 0)) {
		usbvision->compr_level += (usbvision->used_bandwidth - 90) / 2;
		RESTRICT_TO_RANGE(usbvision->compr_level, 0, 100);
		if (usbvision->compr_level != usbvision->last_compr_level) {
			int distortion;

			if (usbvision->bridge_type == BRIDGE_NT1004 || usbvision->bridge_type == BRIDGE_NT1005) {
				buffer[0] = (unsigned char)(4 + 16 * usbvision->compr_level / 100);	/* PCM Threshold 1 */
				buffer[1] = (unsigned char)(4 + 8 * usbvision->compr_level / 100);	/* PCM Threshold 2 */
				distortion = 7 + 248 * usbvision->compr_level / 100;
				buffer[2] = (unsigned char)(distortion & 0xFF);				/* Average distortion Threshold (inter) */
				buffer[3] = (unsigned char)(distortion & 0xFF);				/* Average distortion Threshold (intra) */
				distortion = 1 + 42 * usbvision->compr_level / 100;
				buffer[4] = (unsigned char)(distortion & 0xFF);				/* Maximum distortion Threshold (inter) */
				buffer[5] = (unsigned char)(distortion & 0xFF);				/* Maximum distortion Threshold (intra) */
			} else { /* BRIDGE_NT1003 */
				buffer[0] = (unsigned char)(4 + 16 * usbvision->compr_level / 100);	/* PCM threshold 1 */
				buffer[1] = (unsigned char)(4 + 8 * usbvision->compr_level / 100);	/* PCM threshold 2 */
				distortion = 2 + 253 * usbvision->compr_level / 100;
				buffer[2] = (unsigned char)(distortion & 0xFF);				/* distortion threshold bit0-7 */
				buffer[3] = 0;	/* (unsigned char)((distortion >> 8) & 0x0F);		distortion threshold bit 8-11 */
				distortion = 0 + 43 * usbvision->compr_level / 100;
				buffer[4] = (unsigned char)(distortion & 0xFF);				/* maximum distortion bit0-7 */
				buffer[5] = 0; /* (unsigned char)((distortion >> 8) & 0x01);		maximum distortion bit 8 */
			}
			err_code = usbvision_write_reg_irq(usbvision, USBVISION_PCM_THR1, buffer, 6);
			if (err_code == 0) {
				PDEBUG(DBG_IRQ, "new compr params %#02x %#02x %#02x %#02x %#02x %#02x", buffer[0],
								buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
				usbvision->last_compr_level = usbvision->compr_level;
			}
		}
	}
	return err_code;
}

static int usbvision_request_intra(struct usb_usbvision *usbvision)
{
	int err_code = 0;
	unsigned char buffer[1];

	PDEBUG(DBG_IRQ, "");
	usbvision->request_intra = 1;
	buffer[0] = 1;
	usbvision_write_reg_irq(usbvision, USBVISION_FORCE_INTRA, buffer, 1);
	return err_code;
}

static int usbvision_unrequest_intra(struct usb_usbvision *usbvision)
{
	int err_code = 0;
	unsigned char buffer[1];

	PDEBUG(DBG_IRQ, "");
	usbvision->request_intra = 0;
	buffer[0] = 0;
	usbvision_write_reg_irq(usbvision, USBVISION_FORCE_INTRA, buffer, 1);
	return err_code;
}

/*******************************
 * usbvision utility functions
 *******************************/

int usbvision_power_off(struct usb_usbvision *usbvision)
{
	int err_code = 0;

	PDEBUG(DBG_FUNC, "");

	err_code = usbvision_write_reg(usbvision, USBVISION_PWR_REG, USBVISION_SSPND_EN);
	if (err_code == 1)
		usbvision->power = 0;
	PDEBUG(DBG_FUNC, "%s: err_code %d", (err_code != 1) ? "ERROR" : "power is off", err_code);
	return err_code;
}

/*
 * usbvision_set_video_format()
 *
 */
static int usbvision_set_video_format(struct usb_usbvision *usbvision, int format)
{
	static const char proc[] = "usbvision_set_video_format";
	int rc;
	unsigned char value[2];

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	PDEBUG(DBG_FUNC, "isoc_mode %#02x", format);

	if ((format != ISOC_MODE_YUV422)
	    && (format != ISOC_MODE_YUV420)
	    && (format != ISOC_MODE_COMPRESS)) {
		printk(KERN_ERR "usbvision: unknown video format %02x, using default YUV420",
		       format);
		format = ISOC_MODE_YUV420;
	}
	value[0] = 0x0A;  /* TODO: See the effect of the filter */
	value[1] = format; /* Sets the VO_MODE register which follows FILT_CONT */
	rc = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,
			     USB_DIR_OUT | USB_TYPE_VENDOR |
			     USB_RECIP_ENDPOINT, 0,
			     (__u16) USBVISION_FILT_CONT, value, 2, HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: ERROR=%d. USBVISION stopped - "
		       "reconnect or reload driver.\n", proc, rc);
	}
	usbvision->isoc_mode = format;
	return rc;
}

/*
 * usbvision_set_output()
 *
 */

int usbvision_set_output(struct usb_usbvision *usbvision, int width,
			 int height)
{
	int err_code = 0;
	int usb_width, usb_height;
	unsigned int frame_rate = 0, frame_drop = 0;
	unsigned char value[4];

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	if (width > MAX_USB_WIDTH) {
		usb_width = width / 2;
		usbvision->stretch_width = 2;
	} else {
		usb_width = width;
		usbvision->stretch_width = 1;
	}

	if (height > MAX_USB_HEIGHT) {
		usb_height = height / 2;
		usbvision->stretch_height = 2;
	} else {
		usb_height = height;
		usbvision->stretch_height = 1;
	}

	RESTRICT_TO_RANGE(usb_width, MIN_FRAME_WIDTH, MAX_USB_WIDTH);
	usb_width &= ~(MIN_FRAME_WIDTH-1);
	RESTRICT_TO_RANGE(usb_height, MIN_FRAME_HEIGHT, MAX_USB_HEIGHT);
	usb_height &= ~(1);

	PDEBUG(DBG_FUNC, "usb %dx%d; screen %dx%d; stretch %dx%d",
						usb_width, usb_height, width, height,
						usbvision->stretch_width, usbvision->stretch_height);

	/* I'll not rewrite the same values */
	if ((usb_width != usbvision->curwidth) || (usb_height != usbvision->curheight)) {
		value[0] = usb_width & 0xff;		/* LSB */
		value[1] = (usb_width >> 8) & 0x03;	/* MSB */
		value[2] = usb_height & 0xff;		/* LSB */
		value[3] = (usb_height >> 8) & 0x03;	/* MSB */

		err_code = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,
			     USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
				 0, (__u16) USBVISION_LXSIZE_O, value, 4, HZ);

		if (err_code < 0) {
			dev_err(&usbvision->dev->dev,
				"%s failed: error %d\n", __func__, err_code);
			return err_code;
		}
		usbvision->curwidth = usbvision->stretch_width * usb_width;
		usbvision->curheight = usbvision->stretch_height * usb_height;
	}

	if (usbvision->isoc_mode == ISOC_MODE_YUV422)
		frame_rate = (usbvision->isoc_packet_size * 1000) / (usb_width * usb_height * 2);
	else if (usbvision->isoc_mode == ISOC_MODE_YUV420)
		frame_rate = (usbvision->isoc_packet_size * 1000) / ((usb_width * usb_height * 12) / 8);
	else
		frame_rate = FRAMERATE_MAX;

	if (usbvision->tvnorm_id & V4L2_STD_625_50)
		frame_drop = frame_rate * 32 / 25 - 1;
	else if (usbvision->tvnorm_id & V4L2_STD_525_60)
		frame_drop = frame_rate * 32 / 30 - 1;

	RESTRICT_TO_RANGE(frame_drop, FRAMERATE_MIN, FRAMERATE_MAX);

	PDEBUG(DBG_FUNC, "frame_rate %d fps, frame_drop %d", frame_rate, frame_drop);

	frame_drop = FRAMERATE_MAX;	/* We can allow the maximum here, because dropping is controlled */

	/* frame_drop = 7; => frame_phase = 1, 5, 9, 13, 17, 21, 25, 0, 4, 8, ...
		=> frame_skip = 4;
		=> frame_rate = (7 + 1) * 25 / 32 = 200 / 32 = 6.25;

	   frame_drop = 9; => frame_phase = 1, 5, 8, 11, 14, 17, 21, 24, 27, 1, 4, 8, ...
	    => frame_skip = 4, 3, 3, 3, 3, 4, 3, 3, 3, 3, 4, ...
		=> frame_rate = (9 + 1) * 25 / 32 = 250 / 32 = 7.8125;
	*/
	err_code = usbvision_write_reg(usbvision, USBVISION_FRM_RATE, frame_drop);
	return err_code;
}


/*
 * usbvision_frames_alloc
 * allocate the required frames
 */
int usbvision_frames_alloc(struct usb_usbvision *usbvision, int number_of_frames)
{
	int i;

	/* needs to be page aligned cause the buffers can be mapped individually! */
	usbvision->max_frame_size = PAGE_ALIGN(usbvision->curwidth *
						usbvision->curheight *
						usbvision->palette.bytes_per_pixel);

	/* Try to do my best to allocate the frames the user want in the remaining memory */
	usbvision->num_frames = number_of_frames;
	while (usbvision->num_frames > 0) {
		usbvision->fbuf_size = usbvision->num_frames * usbvision->max_frame_size;
		usbvision->fbuf = usbvision_rvmalloc(usbvision->fbuf_size);
		if (usbvision->fbuf)
			break;
		usbvision->num_frames--;
	}

	spin_lock_init(&usbvision->queue_lock);
	init_waitqueue_head(&usbvision->wait_frame);
	init_waitqueue_head(&usbvision->wait_stream);

	/* Allocate all buffers */
	for (i = 0; i < usbvision->num_frames; i++) {
		usbvision->frame[i].index = i;
		usbvision->frame[i].grabstate = frame_state_unused;
		usbvision->frame[i].data = usbvision->fbuf +
			i * usbvision->max_frame_size;
		/*
		 * Set default sizes for read operation.
		 */
		usbvision->stretch_width = 1;
		usbvision->stretch_height = 1;
		usbvision->frame[i].width = usbvision->curwidth;
		usbvision->frame[i].height = usbvision->curheight;
		usbvision->frame[i].bytes_read = 0;
	}
	PDEBUG(DBG_FUNC, "allocated %d frames (%d bytes per frame)",
			usbvision->num_frames, usbvision->max_frame_size);
	return usbvision->num_frames;
}

/*
 * usbvision_frames_free
 * frees memory allocated for the frames
 */
void usbvision_frames_free(struct usb_usbvision *usbvision)
{
	/* Have to free all that memory */
	PDEBUG(DBG_FUNC, "free %d frames", usbvision->num_frames);

	if (usbvision->fbuf != NULL) {
		usbvision_rvfree(usbvision->fbuf, usbvision->fbuf_size);
		usbvision->fbuf = NULL;

		usbvision->num_frames = 0;
	}
}
/*
 * usbvision_empty_framequeues()
 * prepare queues for incoming and outgoing frames
 */
void usbvision_empty_framequeues(struct usb_usbvision *usbvision)
{
	u32 i;

	INIT_LIST_HEAD(&(usbvision->inqueue));
	INIT_LIST_HEAD(&(usbvision->outqueue));

	for (i = 0; i < USBVISION_NUMFRAMES; i++) {
		usbvision->frame[i].grabstate = frame_state_unused;
		usbvision->frame[i].bytes_read = 0;
	}
}

/*
 * usbvision_stream_interrupt()
 * stops streaming
 */
int usbvision_stream_interrupt(struct usb_usbvision *usbvision)
{
	int ret = 0;

	/* stop reading from the device */

	usbvision->streaming = stream_interrupt;
	ret = wait_event_timeout(usbvision->wait_stream,
				 (usbvision->streaming == stream_idle),
				 msecs_to_jiffies(USBVISION_NUMSBUF*USBVISION_URB_FRAMES));
	return ret;
}

/*
 * usbvision_set_compress_params()
 *
 */

static int usbvision_set_compress_params(struct usb_usbvision *usbvision)
{
	static const char proc[] = "usbvision_set_compresion_params: ";
	int rc;
	unsigned char value[6];

	value[0] = 0x0F;    /* Intra-Compression cycle */
	value[1] = 0x01;    /* Reg.45 one line per strip */
	value[2] = 0x00;    /* Reg.46 Force intra mode on all new frames */
	value[3] = 0x00;    /* Reg.47 FORCE_UP <- 0 normal operation (not force) */
	value[4] = 0xA2;    /* Reg.48 BUF_THR I'm not sure if this does something in not compressed mode. */
	value[5] = 0x00;    /* Reg.49 DVI_YUV This has nothing to do with compression */

	/* catched values for NT1004 */
	/* value[0] = 0xFF; Never apply intra mode automatically */
	/* value[1] = 0xF1; Use full frame height for virtual strip width; One line per strip */
	/* value[2] = 0x01; Force intra mode on all new frames */
	/* value[3] = 0x00; Strip size 400 Bytes; do not force up */
	/* value[4] = 0xA2; */
	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	rc = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,
			     USB_DIR_OUT | USB_TYPE_VENDOR |
			     USB_RECIP_ENDPOINT, 0,
			     (__u16) USBVISION_INTRA_CYC, value, 5, HZ);

	if (rc < 0) {
		printk(KERN_ERR "%sERROR=%d. USBVISION stopped - "
		       "reconnect or reload driver.\n", proc, rc);
		return rc;
	}

	if (usbvision->bridge_type == BRIDGE_NT1004) {
		value[0] =  20; /* PCM Threshold 1 */
		value[1] =  12; /* PCM Threshold 2 */
		value[2] = 255; /* Distortion Threshold inter */
		value[3] = 255; /* Distortion Threshold intra */
		value[4] =  43; /* Max Distortion inter */
		value[5] =  43; /* Max Distortion intra */
	} else {
		value[0] =  20; /* PCM Threshold 1 */
		value[1] =  12; /* PCM Threshold 2 */
		value[2] = 255; /* Distortion Threshold d7-d0 */
		value[3] =   0; /* Distortion Threshold d11-d8 */
		value[4] =  43; /* Max Distortion d7-d0 */
		value[5] =   0; /* Max Distortion d8 */
	}

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	rc = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,
			     USB_DIR_OUT | USB_TYPE_VENDOR |
			     USB_RECIP_ENDPOINT, 0,
			     (__u16) USBVISION_PCM_THR1, value, 6, HZ);

	if (rc < 0) {
		printk(KERN_ERR "%sERROR=%d. USBVISION stopped - "
		       "reconnect or reload driver.\n", proc, rc);
	}
	return rc;
}


/*
 * usbvision_set_input()
 *
 * Set the input (saa711x, ...) size x y and other misc input params
 * I've no idea if this parameters are right
 *
 */
int usbvision_set_input(struct usb_usbvision *usbvision)
{
	static const char proc[] = "usbvision_set_input: ";
	int rc;
	unsigned char value[8];
	unsigned char dvi_yuv_value;

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	/* Set input format expected from decoder*/
	if (usbvision_device_data[usbvision->dev_model].vin_reg1_override) {
		value[0] = usbvision_device_data[usbvision->dev_model].vin_reg1;
	} else if (usbvision_device_data[usbvision->dev_model].codec == CODEC_SAA7113) {
		/* SAA7113 uses 8 bit output */
		value[0] = USBVISION_8_422_SYNC;
	} else {
		/* I'm sure only about d2-d0 [010] 16 bit 4:2:2 usin sync pulses
		 * as that is how saa7111 is configured */
		value[0] = USBVISION_16_422_SYNC;
		/* | USBVISION_VSNC_POL | USBVISION_VCLK_POL);*/
	}

	rc = usbvision_write_reg(usbvision, USBVISION_VIN_REG1, value[0]);
	if (rc < 0) {
		printk(KERN_ERR "%sERROR=%d. USBVISION stopped - "
		       "reconnect or reload driver.\n", proc, rc);
		return rc;
	}


	if (usbvision->tvnorm_id & V4L2_STD_PAL) {
		value[0] = 0xC0;
		value[1] = 0x02;	/* 0x02C0 -> 704 Input video line length */
		value[2] = 0x20;
		value[3] = 0x01;	/* 0x0120 -> 288 Input video n. of lines */
		value[4] = 0x60;
		value[5] = 0x00;	/* 0x0060 -> 96 Input video h offset */
		value[6] = 0x16;
		value[7] = 0x00;	/* 0x0016 -> 22 Input video v offset */
	} else if (usbvision->tvnorm_id & V4L2_STD_SECAM) {
		value[0] = 0xC0;
		value[1] = 0x02;	/* 0x02C0 -> 704 Input video line length */
		value[2] = 0x20;
		value[3] = 0x01;	/* 0x0120 -> 288 Input video n. of lines */
		value[4] = 0x01;
		value[5] = 0x00;	/* 0x0001 -> 01 Input video h offset */
		value[6] = 0x01;
		value[7] = 0x00;	/* 0x0001 -> 01 Input video v offset */
	} else {	/* V4L2_STD_NTSC */
		value[0] = 0xD0;
		value[1] = 0x02;	/* 0x02D0 -> 720 Input video line length */
		value[2] = 0xF0;
		value[3] = 0x00;	/* 0x00F0 -> 240 Input video number of lines */
		value[4] = 0x50;
		value[5] = 0x00;	/* 0x0050 -> 80 Input video h offset */
		value[6] = 0x10;
		value[7] = 0x00;	/* 0x0010 -> 16 Input video v offset */
	}

	if (usbvision_device_data[usbvision->dev_model].x_offset >= 0) {
		value[4] = usbvision_device_data[usbvision->dev_model].x_offset & 0xff;
		value[5] = (usbvision_device_data[usbvision->dev_model].x_offset & 0x0300) >> 8;
	}

	if (adjust_x_offset != -1) {
		value[4] = adjust_x_offset & 0xff;
		value[5] = (adjust_x_offset & 0x0300) >> 8;
	}

	if (usbvision_device_data[usbvision->dev_model].y_offset >= 0) {
		value[6] = usbvision_device_data[usbvision->dev_model].y_offset & 0xff;
		value[7] = (usbvision_device_data[usbvision->dev_model].y_offset & 0x0300) >> 8;
	}

	if (adjust_y_offset != -1) {
		value[6] = adjust_y_offset & 0xff;
		value[7] = (adjust_y_offset & 0x0300) >> 8;
	}

	rc = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,	/* USBVISION specific code */
			     USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT, 0,
			     (__u16) USBVISION_LXSIZE_I, value, 8, HZ);
	if (rc < 0) {
		printk(KERN_ERR "%sERROR=%d. USBVISION stopped - "
		       "reconnect or reload driver.\n", proc, rc);
		return rc;
	}


	dvi_yuv_value = 0x00;	/* U comes after V, Ya comes after U/V, Yb comes after Yb */

	if (usbvision_device_data[usbvision->dev_model].dvi_yuv_override) {
		dvi_yuv_value = usbvision_device_data[usbvision->dev_model].dvi_yuv;
	} else if (usbvision_device_data[usbvision->dev_model].codec == CODEC_SAA7113) {
		/* This changes as the fine sync control changes. Further investigation necessary */
		dvi_yuv_value = 0x06;
	}

	return usbvision_write_reg(usbvision, USBVISION_DVI_YUV, dvi_yuv_value);
}


/*
 * usbvision_set_dram_settings()
 *
 * Set the buffer address needed by the usbvision dram to operate
 * This values has been taken with usbsnoop.
 *
 */

static int usbvision_set_dram_settings(struct usb_usbvision *usbvision)
{
	int rc;
	unsigned char value[8];

	if (usbvision->isoc_mode == ISOC_MODE_COMPRESS) {
		value[0] = 0x42;
		value[1] = 0x71;
		value[2] = 0xff;
		value[3] = 0x00;
		value[4] = 0x98;
		value[5] = 0xe0;
		value[6] = 0x71;
		value[7] = 0xff;
		/* UR:  0x0E200-0x3FFFF = 204288 Words (1 Word = 2 Byte) */
		/* FDL: 0x00000-0x0E099 =  57498 Words */
		/* VDW: 0x0E3FF-0x3FFFF */
	} else {
		value[0] = 0x42;
		value[1] = 0x00;
		value[2] = 0xff;
		value[3] = 0x00;
		value[4] = 0x00;
		value[5] = 0x00;
		value[6] = 0x00;
		value[7] = 0xff;
	}
	/* These are the values of the address of the video buffer,
	 * they have to be loaded into the USBVISION_DRM_PRM1-8
	 *
	 * Start address of video output buffer for read:	drm_prm1-2 -> 0x00000
	 * End address of video output buffer for read:		drm_prm1-3 -> 0x1ffff
	 * Start address of video frame delay buffer:		drm_prm1-4 -> 0x20000
	 *    Only used in compressed mode
	 * End address of video frame delay buffer:		drm_prm1-5-6 -> 0x3ffff
	 *    Only used in compressed mode
	 * Start address of video output buffer for write:	drm_prm1-7 -> 0x00000
	 * End address of video output buffer for write:	drm_prm1-8 -> 0x1ffff
	 */

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return 0;

	rc = usb_control_msg(usbvision->dev, usb_sndctrlpipe(usbvision->dev, 1),
			     USBVISION_OP_CODE,	/* USBVISION specific code */
			     USB_DIR_OUT | USB_TYPE_VENDOR |
			     USB_RECIP_ENDPOINT, 0,
			     (__u16) USBVISION_DRM_PRM1, value, 8, HZ);

	if (rc < 0) {
		dev_err(&usbvision->dev->dev, "%sERROR=%d\n", __func__, rc);
		return rc;
	}

	/* Restart the video buffer logic */
	rc = usbvision_write_reg(usbvision, USBVISION_DRM_CONT, USBVISION_RES_UR |
				   USBVISION_RES_FDL | USBVISION_RES_VDW);
	if (rc < 0)
		return rc;
	rc = usbvision_write_reg(usbvision, USBVISION_DRM_CONT, 0x00);

	return rc;
}

/*
 * ()
 *
 * Power on the device, enables suspend-resume logic
 * &  reset the isoc End-Point
 *
 */

int usbvision_power_on(struct usb_usbvision *usbvision)
{
	int err_code = 0;

	PDEBUG(DBG_FUNC, "");

	usbvision_write_reg(usbvision, USBVISION_PWR_REG, USBVISION_SSPND_EN);
	usbvision_write_reg(usbvision, USBVISION_PWR_REG,
			USBVISION_SSPND_EN | USBVISION_RES2);

	usbvision_write_reg(usbvision, USBVISION_PWR_REG,
			USBVISION_SSPND_EN | USBVISION_PWR_VID);
	err_code = usbvision_write_reg(usbvision, USBVISION_PWR_REG,
			USBVISION_SSPND_EN | USBVISION_PWR_VID | USBVISION_RES2);
	if (err_code == 1)
		usbvision->power = 1;
	PDEBUG(DBG_FUNC, "%s: err_code %d", (err_code < 0) ? "ERROR" : "power is on", err_code);
	return err_code;
}


/*
 * usbvision timer stuff
 */

/* to call usbvision_power_off from task queue */
static void call_usbvision_power_off(struct work_struct *work)
{
	struct usb_usbvision *usbvision = container_of(work, struct usb_usbvision, power_off_work);

	PDEBUG(DBG_FUNC, "");
	if (mutex_lock_interruptible(&usbvision->v4l2_lock))
		return;

	if (usbvision->user == 0) {
		usbvision_i2c_unregister(usbvision);

		usbvision_power_off(usbvision);
		usbvision->initialized = 0;
	}
	mutex_unlock(&usbvision->v4l2_lock);
}

static void usbvision_power_off_timer(unsigned long data)
{
	struct usb_usbvision *usbvision = (void *)data;

	PDEBUG(DBG_FUNC, "");
	del_timer(&usbvision->power_off_timer);
	INIT_WORK(&usbvision->power_off_work, call_usbvision_power_off);
	(void) schedule_work(&usbvision->power_off_work);
}

void usbvision_init_power_off_timer(struct usb_usbvision *usbvision)
{
	init_timer(&usbvision->power_off_timer);
	usbvision->power_off_timer.data = (long)usbvision;
	usbvision->power_off_timer.function = usbvision_power_off_timer;
}

void usbvision_set_power_off_timer(struct usb_usbvision *usbvision)
{
	mod_timer(&usbvision->power_off_timer, jiffies + USBVISION_POWEROFF_TIME);
}

void usbvision_reset_power_off_timer(struct usb_usbvision *usbvision)
{
	if (timer_pending(&usbvision->power_off_timer))
		del_timer(&usbvision->power_off_timer);
}

/*
 * usbvision_begin_streaming()
 * Sure you have to put bit 7 to 0, if not incoming frames are droped, but no
 * idea about the rest
 */
int usbvision_begin_streaming(struct usb_usbvision *usbvision)
{
	if (usbvision->isoc_mode == ISOC_MODE_COMPRESS)
		usbvision_init_compression(usbvision);
	return usbvision_write_reg(usbvision, USBVISION_VIN_REG2,
		USBVISION_NOHVALID | usbvision->vin_reg2_preset);
}

/*
 * usbvision_restart_isoc()
 * Not sure yet if touching here PWR_REG make loose the config
 */

int usbvision_restart_isoc(struct usb_usbvision *usbvision)
{
	int ret;

	ret = usbvision_write_reg(usbvision, USBVISION_PWR_REG,
			      USBVISION_SSPND_EN | USBVISION_PWR_VID);
	if (ret < 0)
		return ret;
	ret = usbvision_write_reg(usbvision, USBVISION_PWR_REG,
			      USBVISION_SSPND_EN | USBVISION_PWR_VID |
			      USBVISION_RES2);
	if (ret < 0)
		return ret;
	ret = usbvision_write_reg(usbvision, USBVISION_VIN_REG2,
			      USBVISION_KEEP_BLANK | USBVISION_NOHVALID |
				  usbvision->vin_reg2_preset);
	if (ret < 0)
		return ret;

	/* TODO: schedule timeout */
	while ((usbvision_read_reg(usbvision, USBVISION_STATUS_REG) & 0x01) != 1)
		;

	return 0;
}

int usbvision_audio_off(struct usb_usbvision *usbvision)
{
	if (usbvision_write_reg(usbvision, USBVISION_IOPIN_REG, USBVISION_AUDIO_MUTE) < 0) {
		printk(KERN_ERR "usbvision_audio_off: can't write reg\n");
		return -1;
	}
	usbvision->audio_mute = 0;
	usbvision->audio_channel = USBVISION_AUDIO_MUTE;
	return 0;
}

int usbvision_set_audio(struct usb_usbvision *usbvision, int audio_channel)
{
	if (!usbvision->audio_mute) {
		if (usbvision_write_reg(usbvision, USBVISION_IOPIN_REG, audio_channel) < 0) {
			printk(KERN_ERR "usbvision_set_audio: can't write iopin register for audio switching\n");
			return -1;
		}
	}
	usbvision->audio_channel = audio_channel;
	return 0;
}

int usbvision_setup(struct usb_usbvision *usbvision, int format)
{
	usbvision_set_video_format(usbvision, format);
	usbvision_set_dram_settings(usbvision);
	usbvision_set_compress_params(usbvision);
	usbvision_set_input(usbvision);
	usbvision_set_output(usbvision, MAX_USB_WIDTH, MAX_USB_HEIGHT);
	usbvision_restart_isoc(usbvision);

	/* cosas del PCM */
	return USBVISION_IS_OPERATIONAL(usbvision);
}

int usbvision_set_alternate(struct usb_usbvision *dev)
{
	int err_code, prev_alt = dev->iface_alt;
	int i;

	dev->iface_alt = 0;
	for (i = 0; i < dev->num_alt; i++)
		if (dev->alt_max_pkt_size[i] > dev->alt_max_pkt_size[dev->iface_alt])
			dev->iface_alt = i;

	if (dev->iface_alt != prev_alt) {
		dev->isoc_packet_size = dev->alt_max_pkt_size[dev->iface_alt];
		PDEBUG(DBG_FUNC, "setting alternate %d with max_packet_size=%u",
				dev->iface_alt, dev->isoc_packet_size);
		err_code = usb_set_interface(dev->dev, dev->iface, dev->iface_alt);
		if (err_code < 0) {
			dev_err(&dev->dev->dev,
				"cannot change alternate number to %d (error=%i)\n",
					dev->iface_alt, err_code);
			return err_code;
		}
	}

	PDEBUG(DBG_ISOC, "ISO Packet Length:%d", dev->isoc_packet_size);

	return 0;
}

/*
 * usbvision_init_isoc()
 *
 */
int usbvision_init_isoc(struct usb_usbvision *usbvision)
{
	struct usb_device *dev = usbvision->dev;
	int buf_idx, err_code, reg_value;
	int sb_size;

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return -EFAULT;

	usbvision->cur_frame = NULL;
	scratch_reset(usbvision);

	/* Alternate interface 1 is is the biggest frame size */
	err_code = usbvision_set_alternate(usbvision);
	if (err_code < 0) {
		usbvision->last_error = err_code;
		return -EBUSY;
	}
	sb_size = USBVISION_URB_FRAMES * usbvision->isoc_packet_size;

	reg_value = (16 - usbvision_read_reg(usbvision,
					    USBVISION_ALTER_REG)) & 0x0F;

	usbvision->usb_bandwidth = reg_value >> 1;
	PDEBUG(DBG_ISOC, "USB Bandwidth Usage: %dMbit/Sec",
	       usbvision->usb_bandwidth);



	/* We double buffer the Iso lists */

	for (buf_idx = 0; buf_idx < USBVISION_NUMSBUF; buf_idx++) {
		int j, k;
		struct urb *urb;

		urb = usb_alloc_urb(USBVISION_URB_FRAMES, GFP_KERNEL);
		if (urb == NULL) {
			dev_err(&usbvision->dev->dev,
				"%s: usb_alloc_urb() failed\n", __func__);
			return -ENOMEM;
		}
		usbvision->sbuf[buf_idx].urb = urb;
		usbvision->sbuf[buf_idx].data =
			usb_alloc_coherent(usbvision->dev,
					   sb_size,
					   GFP_KERNEL,
					   &urb->transfer_dma);
		urb->dev = dev;
		urb->context = usbvision;
		urb->pipe = usb_rcvisocpipe(dev, usbvision->video_endp);
		urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
		urb->interval = 1;
		urb->transfer_buffer = usbvision->sbuf[buf_idx].data;
		urb->complete = usbvision_isoc_irq;
		urb->number_of_packets = USBVISION_URB_FRAMES;
		urb->transfer_buffer_length =
		    usbvision->isoc_packet_size * USBVISION_URB_FRAMES;
		for (j = k = 0; j < USBVISION_URB_FRAMES; j++,
		     k += usbvision->isoc_packet_size) {
			urb->iso_frame_desc[j].offset = k;
			urb->iso_frame_desc[j].length =
				usbvision->isoc_packet_size;
		}
	}

	/* Submit all URBs */
	for (buf_idx = 0; buf_idx < USBVISION_NUMSBUF; buf_idx++) {
			err_code = usb_submit_urb(usbvision->sbuf[buf_idx].urb,
						 GFP_KERNEL);
		if (err_code) {
			dev_err(&usbvision->dev->dev,
				"%s: usb_submit_urb(%d) failed: error %d\n",
					__func__, buf_idx, err_code);
		}
	}

	usbvision->streaming = stream_idle;
	PDEBUG(DBG_ISOC, "%s: streaming=1 usbvision->video_endp=$%02x",
	       __func__,
	       usbvision->video_endp);
	return 0;
}

/*
 * usbvision_stop_isoc()
 *
 * This procedure stops streaming and deallocates URBs. Then it
 * activates zero-bandwidth alt. setting of the video interface.
 *
 */
void usbvision_stop_isoc(struct usb_usbvision *usbvision)
{
	int buf_idx, err_code, reg_value;
	int sb_size = USBVISION_URB_FRAMES * usbvision->isoc_packet_size;

	if ((usbvision->streaming == stream_off) || (usbvision->dev == NULL))
		return;

	/* Unschedule all of the iso td's */
	for (buf_idx = 0; buf_idx < USBVISION_NUMSBUF; buf_idx++) {
		usb_kill_urb(usbvision->sbuf[buf_idx].urb);
		if (usbvision->sbuf[buf_idx].data) {
			usb_free_coherent(usbvision->dev,
					  sb_size,
					  usbvision->sbuf[buf_idx].data,
					  usbvision->sbuf[buf_idx].urb->transfer_dma);
		}
		usb_free_urb(usbvision->sbuf[buf_idx].urb);
		usbvision->sbuf[buf_idx].urb = NULL;
	}

	PDEBUG(DBG_ISOC, "%s: streaming=stream_off\n", __func__);
	usbvision->streaming = stream_off;

	if (!usbvision->remove_pending) {
		/* Set packet size to 0 */
		usbvision->iface_alt = 0;
		err_code = usb_set_interface(usbvision->dev, usbvision->iface,
					    usbvision->iface_alt);
		if (err_code < 0) {
			dev_err(&usbvision->dev->dev,
				"%s: usb_set_interface() failed: error %d\n",
					__func__, err_code);
			usbvision->last_error = err_code;
		}
		reg_value = (16-usbvision_read_reg(usbvision, USBVISION_ALTER_REG)) & 0x0F;
		usbvision->isoc_packet_size =
			(reg_value == 0) ? 0 : (reg_value * 64) - 1;
		PDEBUG(DBG_ISOC, "ISO Packet Length:%d",
		       usbvision->isoc_packet_size);

		usbvision->usb_bandwidth = reg_value >> 1;
		PDEBUG(DBG_ISOC, "USB Bandwidth Usage: %dMbit/Sec",
		       usbvision->usb_bandwidth);
	}
}

int usbvision_muxsel(struct usb_usbvision *usbvision, int channel)
{
	/* inputs #0 and #3 are constant for every SAA711x. */
	/* inputs #1 and #2 are variable for SAA7111 and SAA7113 */
	int mode[4] = { SAA7115_COMPOSITE0, 0, 0, SAA7115_COMPOSITE3 };
	int audio[] = { 1, 0, 0, 0 };
	/* channel 0 is TV with audiochannel 1 (tuner mono) */
	/* channel 1 is Composite with audio channel 0 (line in) */
	/* channel 2 is S-Video with audio channel 0 (line in) */
	/* channel 3 is additional video inputs to the device with audio channel 0 (line in) */

	RESTRICT_TO_RANGE(channel, 0, usbvision->video_inputs);
	usbvision->ctl_input = channel;

	/* set the new channel */
	/* Regular USB TV Tuners -> channel: 0 = Television, 1 = Composite, 2 = S-Video */
	/* Four video input devices -> channel: 0 = Chan White, 1 = Chan Green, 2 = Chan Yellow, 3 = Chan Red */

	switch (usbvision_device_data[usbvision->dev_model].codec) {
	case CODEC_SAA7113:
		mode[1] = SAA7115_COMPOSITE2;
		if (switch_svideo_input) {
			/* To handle problems with S-Video Input for
			 * some devices.  Use switch_svideo_input
			 * parameter when loading the module.*/
			mode[2] = SAA7115_COMPOSITE1;
		} else {
			mode[2] = SAA7115_SVIDEO1;
		}
		break;
	case CODEC_SAA7111:
	default:
		/* modes for saa7111 */
		mode[1] = SAA7115_COMPOSITE1;
		mode[2] = SAA7115_SVIDEO1;
		break;
	}
	call_all(usbvision, video, s_routing, mode[channel], 0, 0);
	usbvision_set_audio(usbvision, audio[channel]);
	return 0;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
