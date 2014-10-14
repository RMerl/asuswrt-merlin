/*
 *  cx18 ioctl system call
 *
 *  Derived from ivtv-ioctl.c
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *  Copyright (C) 2008  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA
 */

#include "cx18-driver.h"
#include "cx18-io.h"
#include "cx18-version.h"
#include "cx18-mailbox.h"
#include "cx18-i2c.h"
#include "cx18-queue.h"
#include "cx18-fileops.h"
#include "cx18-vbi.h"
#include "cx18-audio.h"
#include "cx18-video.h"
#include "cx18-streams.h"
#include "cx18-ioctl.h"
#include "cx18-gpio.h"
#include "cx18-controls.h"
#include "cx18-cards.h"
#include "cx18-av-core.h"
#include <media/tveeprom.h>
#include <media/v4l2-chip-ident.h>

u16 cx18_service2vbi(int type)
{
	switch (type) {
	case V4L2_SLICED_TELETEXT_B:
		return CX18_SLICED_TYPE_TELETEXT_B;
	case V4L2_SLICED_CAPTION_525:
		return CX18_SLICED_TYPE_CAPTION_525;
	case V4L2_SLICED_WSS_625:
		return CX18_SLICED_TYPE_WSS_625;
	case V4L2_SLICED_VPS:
		return CX18_SLICED_TYPE_VPS;
	default:
		return 0;
	}
}

/* Check if VBI services are allowed on the (field, line) for the video std */
static int valid_service_line(int field, int line, int is_pal)
{
	return (is_pal && line >= 6 &&
		((field == 0 && line <= 23) || (field == 1 && line <= 22))) ||
	       (!is_pal && line >= 10 && line < 22);
}

/*
 * For a (field, line, std) and inbound potential set of services for that line,
 * return the first valid service of those passed in the incoming set for that
 * line in priority order:
 * CC, VPS, or WSS over TELETEXT for well known lines
 * TELETEXT, before VPS, before CC, before WSS, for other lines
 */
static u16 select_service_from_set(int field, int line, u16 set, int is_pal)
{
	u16 valid_set = (is_pal ? V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525);
	int i;

	set = set & valid_set;
	if (set == 0 || !valid_service_line(field, line, is_pal))
		return 0;
	if (!is_pal) {
		if (line == 21 && (set & V4L2_SLICED_CAPTION_525))
			return V4L2_SLICED_CAPTION_525;
	} else {
		if (line == 16 && field == 0 && (set & V4L2_SLICED_VPS))
			return V4L2_SLICED_VPS;
		if (line == 23 && field == 0 && (set & V4L2_SLICED_WSS_625))
			return V4L2_SLICED_WSS_625;
		if (line == 23)
			return 0;
	}
	for (i = 0; i < 32; i++) {
		if ((1 << i) & set)
			return 1 << i;
	}
	return 0;
}

/*
 * Expand the service_set of *fmt into valid service_lines for the std,
 * and clear the passed in fmt->service_set
 */
void cx18_expand_service_set(struct v4l2_sliced_vbi_format *fmt, int is_pal)
{
	u16 set = fmt->service_set;
	int f, l;

	fmt->service_set = 0;
	for (f = 0; f < 2; f++) {
		for (l = 0; l < 24; l++)
			fmt->service_lines[f][l] = select_service_from_set(f, l, set, is_pal);
	}
}

/*
 * Sanitize the service_lines in *fmt per the video std, and return 1
 * if any service_line is left as valid after santization
 */
static int check_service_set(struct v4l2_sliced_vbi_format *fmt, int is_pal)
{
	int f, l;
	u16 set = 0;

	for (f = 0; f < 2; f++) {
		for (l = 0; l < 24; l++) {
			fmt->service_lines[f][l] = select_service_from_set(f, l, fmt->service_lines[f][l], is_pal);
			set |= fmt->service_lines[f][l];
		}
	}
	return set != 0;
}

/* Compute the service_set from the assumed valid service_lines of *fmt */
u16 cx18_get_service_set(struct v4l2_sliced_vbi_format *fmt)
{
	int f, l;
	u16 set = 0;

	for (f = 0; f < 2; f++) {
		for (l = 0; l < 24; l++)
			set |= fmt->service_lines[f][l];
	}
	return set;
}

static int cx18_g_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	struct v4l2_pix_format *pixfmt = &fmt->fmt.pix;

	pixfmt->width = cx->cxhdl.width;
	pixfmt->height = cx->cxhdl.height;
	pixfmt->colorspace = V4L2_COLORSPACE_SMPTE170M;
	pixfmt->field = V4L2_FIELD_INTERLACED;
	pixfmt->priv = 0;
	if (id->type == CX18_ENC_STREAM_TYPE_YUV) {
		pixfmt->pixelformat = V4L2_PIX_FMT_HM12;
		/* YUV size is (Y=(h*720) + UV=(h*(720/2))) */
		pixfmt->sizeimage = pixfmt->height * 720 * 3 / 2;
		pixfmt->bytesperline = 720;
	} else {
		pixfmt->pixelformat = V4L2_PIX_FMT_MPEG;
		pixfmt->sizeimage = 128 * 1024;
		pixfmt->bytesperline = 0;
	}
	return 0;
}

static int cx18_g_fmt_vbi_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	struct cx18 *cx = fh2id(fh)->cx;
	struct v4l2_vbi_format *vbifmt = &fmt->fmt.vbi;

	vbifmt->sampling_rate = 27000000;
	vbifmt->offset = 248; /* FIXME - slightly wrong for both 50 & 60 Hz */
	vbifmt->samples_per_line = vbi_active_samples - 4;
	vbifmt->sample_format = V4L2_PIX_FMT_GREY;
	vbifmt->start[0] = cx->vbi.start[0];
	vbifmt->start[1] = cx->vbi.start[1];
	vbifmt->count[0] = vbifmt->count[1] = cx->vbi.count;
	vbifmt->flags = 0;
	vbifmt->reserved[0] = 0;
	vbifmt->reserved[1] = 0;
	return 0;
}

static int cx18_g_fmt_sliced_vbi_cap(struct file *file, void *fh,
					struct v4l2_format *fmt)
{
	struct cx18 *cx = fh2id(fh)->cx;
	struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;

	/* sane, V4L2 spec compliant, defaults */
	vbifmt->reserved[0] = 0;
	vbifmt->reserved[1] = 0;
	vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
	memset(vbifmt->service_lines, 0, sizeof(vbifmt->service_lines));
	vbifmt->service_set = 0;

	/*
	 * Fetch the configured service_lines and total service_set from the
	 * digitizer/slicer.  Note, cx18_av_vbi() wipes the passed in
	 * fmt->fmt.sliced under valid calling conditions
	 */
	if (v4l2_subdev_call(cx->sd_av, vbi, g_sliced_fmt, &fmt->fmt.sliced))
		return -EINVAL;

	/* Ensure V4L2 spec compliant output */
	vbifmt->reserved[0] = 0;
	vbifmt->reserved[1] = 0;
	vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
	vbifmt->service_set = cx18_get_service_set(vbifmt);
	return 0;
}

static int cx18_try_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	int w = fmt->fmt.pix.width;
	int h = fmt->fmt.pix.height;
	int min_h = 2;

	w = min(w, 720);
	w = max(w, 2);
	if (id->type == CX18_ENC_STREAM_TYPE_YUV) {
		/* YUV height must be a multiple of 32 */
		h &= ~0x1f;
		min_h = 32;
	}
	h = min(h, cx->is_50hz ? 576 : 480);
	h = max(h, min_h);

	cx18_g_fmt_vid_cap(file, fh, fmt);
	fmt->fmt.pix.width = w;
	fmt->fmt.pix.height = h;
	return 0;
}

static int cx18_try_fmt_vbi_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	return cx18_g_fmt_vbi_cap(file, fh, fmt);
}

static int cx18_try_fmt_sliced_vbi_cap(struct file *file, void *fh,
					struct v4l2_format *fmt)
{
	struct cx18 *cx = fh2id(fh)->cx;
	struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;

	vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
	vbifmt->reserved[0] = 0;
	vbifmt->reserved[1] = 0;

	/* If given a service set, expand it validly & clear passed in set */
	if (vbifmt->service_set)
		cx18_expand_service_set(vbifmt, cx->is_50hz);
	/* Sanitize the service_lines, and compute the new set if any valid */
	if (check_service_set(vbifmt, cx->is_50hz))
		vbifmt->service_set = cx18_get_service_set(vbifmt);
	return 0;
}

static int cx18_s_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;
	int w, h;

	ret = cx18_try_fmt_vid_cap(file, fh, fmt);
	if (ret)
		return ret;
	w = fmt->fmt.pix.width;
	h = fmt->fmt.pix.height;

	if (cx->cxhdl.width == w && cx->cxhdl.height == h)
		return 0;

	if (atomic_read(&cx->ana_capturing) > 0)
		return -EBUSY;

	mbus_fmt.width = cx->cxhdl.width = w;
	mbus_fmt.height = cx->cxhdl.height = h;
	mbus_fmt.code = V4L2_MBUS_FMT_FIXED;
	v4l2_subdev_call(cx->sd_av, video, s_mbus_fmt, &mbus_fmt);
	return cx18_g_fmt_vid_cap(file, fh, fmt);
}

static int cx18_s_fmt_vbi_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	int ret;

	/*
	 * Changing the Encoder's Raw VBI parameters won't have any effect
	 * if any analog capture is ongoing
	 */
	if (!cx18_raw_vbi(cx) && atomic_read(&cx->ana_capturing) > 0)
		return -EBUSY;

	/*
	 * Set the digitizer registers for raw active VBI.
	 * Note cx18_av_vbi_wipes out a lot of the passed in fmt under valid
	 * calling conditions
	 */
	ret = v4l2_subdev_call(cx->sd_av, vbi, s_raw_fmt, &fmt->fmt.vbi);
	if (ret)
		return ret;

	/* Store our new v4l2 (non-)sliced VBI state */
	cx->vbi.sliced_in->service_set = 0;
	cx->vbi.in.type = V4L2_BUF_TYPE_VBI_CAPTURE;

	return cx18_g_fmt_vbi_cap(file, fh, fmt);
}

static int cx18_s_fmt_sliced_vbi_cap(struct file *file, void *fh,
					struct v4l2_format *fmt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	int ret;
	struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;

	cx18_try_fmt_sliced_vbi_cap(file, fh, fmt);

	/*
	 * Changing the Encoder's Raw VBI parameters won't have any effect
	 * if any analog capture is ongoing
	 */
	if (cx18_raw_vbi(cx) && atomic_read(&cx->ana_capturing) > 0)
		return -EBUSY;

	/*
	 * Set the service_lines requested in the digitizer/slicer registers.
	 * Note, cx18_av_vbi() wipes some "impossible" service lines in the
	 * passed in fmt->fmt.sliced under valid calling conditions
	 */
	ret = v4l2_subdev_call(cx->sd_av, vbi, s_sliced_fmt, &fmt->fmt.sliced);
	if (ret)
		return ret;
	/* Store our current v4l2 sliced VBI settings */
	cx->vbi.in.type =  V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
	memcpy(cx->vbi.sliced_in, vbifmt, sizeof(*cx->vbi.sliced_in));
	return 0;
}

static int cx18_g_chip_ident(struct file *file, void *fh,
				struct v4l2_dbg_chip_ident *chip)
{
	struct cx18 *cx = fh2id(fh)->cx;
	int err = 0;

	chip->ident = V4L2_IDENT_NONE;
	chip->revision = 0;
	switch (chip->match.type) {
	case V4L2_CHIP_MATCH_HOST:
		switch (chip->match.addr) {
		case 0:
			chip->ident = V4L2_IDENT_CX23418;
			chip->revision = cx18_read_reg(cx, 0xC72028);
			break;
		case 1:
			/*
			 * The A/V decoder is always present, but in the rare
			 * case that the card doesn't have analog, we don't
			 * use it.  We find it w/o using the cx->sd_av pointer
			 */
			cx18_call_hw(cx, CX18_HW_418_AV,
				     core, g_chip_ident, chip);
			break;
		default:
			/*
			 * Could return ident = V4L2_IDENT_UNKNOWN if we had
			 * other host chips at higher addresses, but we don't
			 */
			err = -EINVAL; /* per V4L2 spec */
			break;
		}
		break;
	case V4L2_CHIP_MATCH_I2C_DRIVER:
		/* If needed, returns V4L2_IDENT_AMBIGUOUS without extra work */
		cx18_call_all(cx, core, g_chip_ident, chip);
		break;
	case V4L2_CHIP_MATCH_I2C_ADDR:
		/*
		 * We could return V4L2_IDENT_UNKNOWN, but we don't do the work
		 * to look if a chip is at the address with no driver.  That's a
		 * dangerous thing to do with EEPROMs anyway.
		 */
		cx18_call_all(cx, core, g_chip_ident, chip);
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int cx18_cxc(struct cx18 *cx, unsigned int cmd, void *arg)
{
	struct v4l2_dbg_register *regs = arg;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (regs->reg >= CX18_MEM_OFFSET + CX18_MEM_SIZE)
		return -EINVAL;

	regs->size = 4;
	if (cmd == VIDIOC_DBG_S_REGISTER)
		cx18_write_enc(cx, regs->val, regs->reg);
	else
		regs->val = cx18_read_enc(cx, regs->reg);
	return 0;
}

static int cx18_g_register(struct file *file, void *fh,
				struct v4l2_dbg_register *reg)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (v4l2_chip_match_host(&reg->match))
		return cx18_cxc(cx, VIDIOC_DBG_G_REGISTER, reg);
	/* FIXME - errors shouldn't be ignored */
	cx18_call_all(cx, core, g_register, reg);
	return 0;
}

static int cx18_s_register(struct file *file, void *fh,
				struct v4l2_dbg_register *reg)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (v4l2_chip_match_host(&reg->match))
		return cx18_cxc(cx, VIDIOC_DBG_S_REGISTER, reg);
	/* FIXME - errors shouldn't be ignored */
	cx18_call_all(cx, core, s_register, reg);
	return 0;
}
#endif

static int cx18_querycap(struct file *file, void *fh,
				struct v4l2_capability *vcap)
{
	struct cx18 *cx = fh2id(fh)->cx;

	strlcpy(vcap->driver, CX18_DRIVER_NAME, sizeof(vcap->driver));
	strlcpy(vcap->card, cx->card_name, sizeof(vcap->card));
	snprintf(vcap->bus_info, sizeof(vcap->bus_info),
		 "PCI:%s", pci_name(cx->pci_dev));
	vcap->version = CX18_DRIVER_VERSION; 	    /* version */
	vcap->capabilities = cx->v4l2_cap; 	    /* capabilities */
	return 0;
}

static int cx18_enumaudio(struct file *file, void *fh, struct v4l2_audio *vin)
{
	struct cx18 *cx = fh2id(fh)->cx;

	return cx18_get_audio_input(cx, vin->index, vin);
}

static int cx18_g_audio(struct file *file, void *fh, struct v4l2_audio *vin)
{
	struct cx18 *cx = fh2id(fh)->cx;

	vin->index = cx->audio_input;
	return cx18_get_audio_input(cx, vin->index, vin);
}

static int cx18_s_audio(struct file *file, void *fh, struct v4l2_audio *vout)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (vout->index >= cx->nof_audio_inputs)
		return -EINVAL;
	cx->audio_input = vout->index;
	cx18_audio_set_io(cx);
	return 0;
}

static int cx18_enum_input(struct file *file, void *fh, struct v4l2_input *vin)
{
	struct cx18 *cx = fh2id(fh)->cx;

	/* set it to defaults from our table */
	return cx18_get_input(cx, vin->index, vin);
}

static int cx18_cropcap(struct file *file, void *fh,
			struct v4l2_cropcap *cropcap)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (cropcap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	cropcap->bounds.top = cropcap->bounds.left = 0;
	cropcap->bounds.width = 720;
	cropcap->bounds.height = cx->is_50hz ? 576 : 480;
	cropcap->pixelaspect.numerator = cx->is_50hz ? 59 : 10;
	cropcap->pixelaspect.denominator = cx->is_50hz ? 54 : 11;
	cropcap->defrect = cropcap->bounds;
	return 0;
}

static int cx18_s_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	CX18_DEBUG_WARN("VIDIOC_S_CROP not implemented\n");
	return -EINVAL;
}

static int cx18_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	CX18_DEBUG_WARN("VIDIOC_G_CROP not implemented\n");
	return -EINVAL;
}

static int cx18_enum_fmt_vid_cap(struct file *file, void *fh,
					struct v4l2_fmtdesc *fmt)
{
	static struct v4l2_fmtdesc formats[] = {
		{ 0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
		  "HM12 (YUV 4:1:1)", V4L2_PIX_FMT_HM12, { 0, 0, 0, 0 }
		},
		{ 1, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_FMT_FLAG_COMPRESSED,
		  "MPEG", V4L2_PIX_FMT_MPEG, { 0, 0, 0, 0 }
		}
	};

	if (fmt->index > 1)
		return -EINVAL;
	*fmt = formats[fmt->index];
	return 0;
}

static int cx18_g_input(struct file *file, void *fh, unsigned int *i)
{
	struct cx18 *cx = fh2id(fh)->cx;

	*i = cx->active_input;
	return 0;
}

int cx18_s_input(struct file *file, void *fh, unsigned int inp)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;

	if (inp >= cx->nof_inputs)
		return -EINVAL;

	if (inp == cx->active_input) {
		CX18_DEBUG_INFO("Input unchanged\n");
		return 0;
	}

	CX18_DEBUG_INFO("Changing input from %d to %d\n",
			cx->active_input, inp);

	cx->active_input = inp;
	/* Set the audio input to whatever is appropriate for the input type. */
	cx->audio_input = cx->card->video_inputs[inp].audio_index;

	/* prevent others from messing with the streams until
	   we're finished changing inputs. */
	cx18_mute(cx);
	cx18_video_set_io(cx);
	cx18_audio_set_io(cx);
	cx18_unmute(cx);
	return 0;
}

static int cx18_g_frequency(struct file *file, void *fh,
				struct v4l2_frequency *vf)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (vf->tuner != 0)
		return -EINVAL;

	cx18_call_all(cx, tuner, g_frequency, vf);
	return 0;
}

int cx18_s_frequency(struct file *file, void *fh, struct v4l2_frequency *vf)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;

	if (vf->tuner != 0)
		return -EINVAL;

	cx18_mute(cx);
	CX18_DEBUG_INFO("v4l2 ioctl: set frequency %d\n", vf->frequency);
	cx18_call_all(cx, tuner, s_frequency, vf);
	cx18_unmute(cx);
	return 0;
}

static int cx18_g_std(struct file *file, void *fh, v4l2_std_id *std)
{
	struct cx18 *cx = fh2id(fh)->cx;

	*std = cx->std;
	return 0;
}

int cx18_s_std(struct file *file, void *fh, v4l2_std_id *std)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;

	if ((*std & V4L2_STD_ALL) == 0)
		return -EINVAL;

	if (*std == cx->std)
		return 0;

	if (test_bit(CX18_F_I_RADIO_USER, &cx->i_flags) ||
	    atomic_read(&cx->ana_capturing) > 0) {
		/* Switching standard would turn off the radio or mess
		   with already running streams, prevent that by
		   returning EBUSY. */
		return -EBUSY;
	}

	cx->std = *std;
	cx->is_60hz = (*std & V4L2_STD_525_60) ? 1 : 0;
	cx->is_50hz = !cx->is_60hz;
	cx2341x_handler_set_50hz(&cx->cxhdl, cx->is_50hz);
	cx->cxhdl.width = 720;
	cx->cxhdl.height = cx->is_50hz ? 576 : 480;
	cx->vbi.count = cx->is_50hz ? 18 : 12;
	cx->vbi.start[0] = cx->is_50hz ? 6 : 10;
	cx->vbi.start[1] = cx->is_50hz ? 318 : 273;
	CX18_DEBUG_INFO("Switching standard to %llx.\n",
			(unsigned long long) cx->std);

	/* Tuner */
	cx18_call_all(cx, core, s_std, cx->std);
	return 0;
}

static int cx18_s_tuner(struct file *file, void *fh, struct v4l2_tuner *vt)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;

	if (vt->index != 0)
		return -EINVAL;

	cx18_call_all(cx, tuner, s_tuner, vt);
	return 0;
}

static int cx18_g_tuner(struct file *file, void *fh, struct v4l2_tuner *vt)
{
	struct cx18 *cx = fh2id(fh)->cx;

	if (vt->index != 0)
		return -EINVAL;

	cx18_call_all(cx, tuner, g_tuner, vt);

	if (test_bit(CX18_F_I_RADIO_USER, &cx->i_flags)) {
		strlcpy(vt->name, "cx18 Radio Tuner", sizeof(vt->name));
		vt->type = V4L2_TUNER_RADIO;
	} else {
		strlcpy(vt->name, "cx18 TV Tuner", sizeof(vt->name));
		vt->type = V4L2_TUNER_ANALOG_TV;
	}

	return 0;
}

static int cx18_g_sliced_vbi_cap(struct file *file, void *fh,
					struct v4l2_sliced_vbi_cap *cap)
{
	struct cx18 *cx = fh2id(fh)->cx;
	int set = cx->is_50hz ? V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;
	int f, l;

	if (cap->type != V4L2_BUF_TYPE_SLICED_VBI_CAPTURE)
		return -EINVAL;

	cap->service_set = 0;
	for (f = 0; f < 2; f++) {
		for (l = 0; l < 24; l++) {
			if (valid_service_line(f, l, cx->is_50hz)) {
				/*
				 * We can find all v4l2 supported vbi services
				 * for the standard, on a valid line for the std
				 */
				cap->service_lines[f][l] = set;
				cap->service_set |= set;
			} else
				cap->service_lines[f][l] = 0;
		}
	}
	for (f = 0; f < 3; f++)
		cap->reserved[f] = 0;
	return 0;
}

static int _cx18_process_idx_data(struct cx18_buffer *buf,
				  struct v4l2_enc_idx *idx)
{
	int consumed, remaining;
	struct v4l2_enc_idx_entry *e_idx;
	struct cx18_enc_idx_entry *e_buf;

	/* Frame type lookup: 1=I, 2=P, 4=B */
	const int mapping[8] = {
		-1, V4L2_ENC_IDX_FRAME_I, V4L2_ENC_IDX_FRAME_P,
		-1, V4L2_ENC_IDX_FRAME_B, -1, -1, -1
	};

	/*
	 * Assumption here is that a buf holds an integral number of
	 * struct cx18_enc_idx_entry objects and is properly aligned.
	 * This is enforced by the module options on IDX buffer sizes.
	 */
	remaining = buf->bytesused - buf->readpos;
	consumed = 0;
	e_idx = &idx->entry[idx->entries];
	e_buf = (struct cx18_enc_idx_entry *) &buf->buf[buf->readpos];

	while (remaining >= sizeof(struct cx18_enc_idx_entry) &&
	       idx->entries < V4L2_ENC_IDX_ENTRIES) {

		e_idx->offset = (((u64) le32_to_cpu(e_buf->offset_high)) << 32)
				| le32_to_cpu(e_buf->offset_low);

		e_idx->pts = (((u64) (le32_to_cpu(e_buf->pts_high) & 1)) << 32)
			     | le32_to_cpu(e_buf->pts_low);

		e_idx->length = le32_to_cpu(e_buf->length);

		e_idx->flags = mapping[le32_to_cpu(e_buf->flags) & 0x7];

		e_idx->reserved[0] = 0;
		e_idx->reserved[1] = 0;

		idx->entries++;
		e_idx = &idx->entry[idx->entries];
		e_buf++;

		remaining -= sizeof(struct cx18_enc_idx_entry);
		consumed += sizeof(struct cx18_enc_idx_entry);
	}

	/* Swallow any partial entries at the end, if there are any */
	if (remaining > 0 && remaining < sizeof(struct cx18_enc_idx_entry))
		consumed += remaining;

	buf->readpos += consumed;
	return consumed;
}

static int cx18_process_idx_data(struct cx18_stream *s, struct cx18_mdl *mdl,
				 struct v4l2_enc_idx *idx)
{
	if (s->type != CX18_ENC_STREAM_TYPE_IDX)
		return -EINVAL;

	if (mdl->curr_buf == NULL)
		mdl->curr_buf = list_first_entry(&mdl->buf_list,
						 struct cx18_buffer, list);

	if (list_entry_is_past_end(mdl->curr_buf, &mdl->buf_list, list)) {
		/*
		 * For some reason we've exhausted the buffers, but the MDL
		 * object still said some data was unread.
		 * Fix that and bail out.
		 */
		mdl->readpos = mdl->bytesused;
		return 0;
	}

	list_for_each_entry_from(mdl->curr_buf, &mdl->buf_list, list) {

		/* Skip any empty buffers in the MDL */
		if (mdl->curr_buf->readpos >= mdl->curr_buf->bytesused)
			continue;

		mdl->readpos += _cx18_process_idx_data(mdl->curr_buf, idx);

		/* exit when MDL drained or request satisfied */
		if (idx->entries >= V4L2_ENC_IDX_ENTRIES ||
		    mdl->curr_buf->readpos < mdl->curr_buf->bytesused ||
		    mdl->readpos >= mdl->bytesused)
			break;
	}
	return 0;
}

static int cx18_g_enc_index(struct file *file, void *fh,
				struct v4l2_enc_idx *idx)
{
	struct cx18 *cx = fh2id(fh)->cx;
	struct cx18_stream *s = &cx->streams[CX18_ENC_STREAM_TYPE_IDX];
	s32 tmp;
	struct cx18_mdl *mdl;

	if (!cx18_stream_enabled(s)) /* Module options inhibited IDX stream */
		return -EINVAL;

	/* Compute the best case number of entries we can buffer */
	tmp = s->buffers -
			  s->bufs_per_mdl * CX18_ENC_STREAM_TYPE_IDX_FW_MDL_MIN;
	if (tmp <= 0)
		tmp = 1;
	tmp = tmp * s->buf_size / sizeof(struct cx18_enc_idx_entry);

	/* Fill out the header of the return structure */
	idx->entries = 0;
	idx->entries_cap = tmp;
	memset(idx->reserved, 0, sizeof(idx->reserved));

	/* Pull IDX MDLs and buffers from q_full and populate the entries */
	do {
		mdl = cx18_dequeue(s, &s->q_full);
		if (mdl == NULL) /* No more IDX data right now */
			break;

		/* Extract the Index entry data from the MDL and buffers */
		cx18_process_idx_data(s, mdl, idx);
		if (mdl->readpos < mdl->bytesused) {
			/* We finished with data remaining, push the MDL back */
			cx18_push(s, mdl, &s->q_full);
			break;
		}

		/* We drained this MDL, schedule it to go to the firmware */
		cx18_enqueue(s, mdl, &s->q_free);

	} while (idx->entries < V4L2_ENC_IDX_ENTRIES);

	/* Tell the work handler to send free IDX MDLs to the firmware */
	cx18_stream_load_fw_queue(s);
	return 0;
}

static int cx18_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct cx18_open_id *id = fh2id(fh);
	struct cx18 *cx = id->cx;
	u32 h;

	switch (enc->cmd) {
	case V4L2_ENC_CMD_START:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_START\n");
		enc->flags = 0;
		return cx18_start_capture(id);

	case V4L2_ENC_CMD_STOP:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_STOP\n");
		enc->flags &= V4L2_ENC_CMD_STOP_AT_GOP_END;
		cx18_stop_capture(id,
				  enc->flags & V4L2_ENC_CMD_STOP_AT_GOP_END);
		break;

	case V4L2_ENC_CMD_PAUSE:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_PAUSE\n");
		enc->flags = 0;
		if (!atomic_read(&cx->ana_capturing))
			return -EPERM;
		if (test_and_set_bit(CX18_F_I_ENC_PAUSED, &cx->i_flags))
			return 0;
		h = cx18_find_handle(cx);
		if (h == CX18_INVALID_TASK_HANDLE) {
			CX18_ERR("Can't find valid task handle for "
				 "V4L2_ENC_CMD_PAUSE\n");
			return -EBADFD;
		}
		cx18_mute(cx);
		cx18_vapi(cx, CX18_CPU_CAPTURE_PAUSE, 1, h);
		break;

	case V4L2_ENC_CMD_RESUME:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_RESUME\n");
		enc->flags = 0;
		if (!atomic_read(&cx->ana_capturing))
			return -EPERM;
		if (!test_and_clear_bit(CX18_F_I_ENC_PAUSED, &cx->i_flags))
			return 0;
		h = cx18_find_handle(cx);
		if (h == CX18_INVALID_TASK_HANDLE) {
			CX18_ERR("Can't find valid task handle for "
				 "V4L2_ENC_CMD_RESUME\n");
			return -EBADFD;
		}
		cx18_vapi(cx, CX18_CPU_CAPTURE_RESUME, 1, h);
		cx18_unmute(cx);
		break;

	default:
		CX18_DEBUG_IOCTL("Unknown cmd %d\n", enc->cmd);
		return -EINVAL;
	}
	return 0;
}

static int cx18_try_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct cx18 *cx = fh2id(fh)->cx;

	switch (enc->cmd) {
	case V4L2_ENC_CMD_START:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_START\n");
		enc->flags = 0;
		break;

	case V4L2_ENC_CMD_STOP:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_STOP\n");
		enc->flags &= V4L2_ENC_CMD_STOP_AT_GOP_END;
		break;

	case V4L2_ENC_CMD_PAUSE:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_PAUSE\n");
		enc->flags = 0;
		break;

	case V4L2_ENC_CMD_RESUME:
		CX18_DEBUG_IOCTL("V4L2_ENC_CMD_RESUME\n");
		enc->flags = 0;
		break;

	default:
		CX18_DEBUG_IOCTL("Unknown cmd %d\n", enc->cmd);
		return -EINVAL;
	}
	return 0;
}

static int cx18_log_status(struct file *file, void *fh)
{
	struct cx18 *cx = fh2id(fh)->cx;
	struct v4l2_input vidin;
	struct v4l2_audio audin;
	int i;

	CX18_INFO("=================  START STATUS CARD #%d  "
		  "=================\n", cx->instance);
	CX18_INFO("Version: %s  Card: %s\n", CX18_VERSION, cx->card_name);
	if (cx->hw_flags & CX18_HW_TVEEPROM) {
		struct tveeprom tv;

		cx18_read_eeprom(cx, &tv);
	}
	cx18_call_all(cx, core, log_status);
	cx18_get_input(cx, cx->active_input, &vidin);
	cx18_get_audio_input(cx, cx->audio_input, &audin);
	CX18_INFO("Video Input: %s\n", vidin.name);
	CX18_INFO("Audio Input: %s\n", audin.name);
	mutex_lock(&cx->gpio_lock);
	CX18_INFO("GPIO:  direction 0x%08x, value 0x%08x\n",
		cx->gpio_dir, cx->gpio_val);
	mutex_unlock(&cx->gpio_lock);
	CX18_INFO("Tuner: %s\n",
		test_bit(CX18_F_I_RADIO_USER, &cx->i_flags) ?  "Radio" : "TV");
	v4l2_ctrl_handler_log_status(&cx->cxhdl.hdl, cx->v4l2_dev.name);
	CX18_INFO("Status flags: 0x%08lx\n", cx->i_flags);
	for (i = 0; i < CX18_MAX_STREAMS; i++) {
		struct cx18_stream *s = &cx->streams[i];

		if (s->video_dev == NULL || s->buffers == 0)
			continue;
		CX18_INFO("Stream %s: status 0x%04lx, %d%% of %d KiB (%d buffers) in use\n",
			  s->name, s->s_flags,
			  atomic_read(&s->q_full.depth) * s->bufs_per_mdl * 100
			   / s->buffers,
			  (s->buffers * s->buf_size) / 1024, s->buffers);
	}
	CX18_INFO("Read MPEG/VBI: %lld/%lld bytes\n",
			(long long)cx->mpg_data_received,
			(long long)cx->vbi_data_inserted);
	CX18_INFO("==================  END STATUS CARD #%d  "
		  "==================\n", cx->instance);
	return 0;
}

static long cx18_default(struct file *file, void *fh, bool valid_prio,
							int cmd, void *arg)
{
	struct cx18 *cx = fh2id(fh)->cx;

	switch (cmd) {
	case VIDIOC_INT_RESET: {
		u32 val = *(u32 *)arg;

		if ((val == 0) || (val & 0x01))
			cx18_call_hw(cx, CX18_HW_GPIO_RESET_CTRL, core, reset,
				     (u32) CX18_GPIO_RESET_Z8F0811);
		break;
	}

	default:
		return -EINVAL;
	}
	return 0;
}

long cx18_v4l2_ioctl(struct file *filp, unsigned int cmd,
		    unsigned long arg)
{
	struct video_device *vfd = video_devdata(filp);
	struct cx18_open_id *id = file2id(filp);
	struct cx18 *cx = id->cx;
	long res;

	mutex_lock(&cx->serialize_lock);

	if (cx18_debug & CX18_DBGFLG_IOCTL)
		vfd->debug = V4L2_DEBUG_IOCTL | V4L2_DEBUG_IOCTL_ARG;
	res = video_ioctl2(filp, cmd, arg);
	vfd->debug = 0;
	mutex_unlock(&cx->serialize_lock);
	return res;
}

static const struct v4l2_ioctl_ops cx18_ioctl_ops = {
	.vidioc_querycap                = cx18_querycap,
	.vidioc_s_audio                 = cx18_s_audio,
	.vidioc_g_audio                 = cx18_g_audio,
	.vidioc_enumaudio               = cx18_enumaudio,
	.vidioc_enum_input              = cx18_enum_input,
	.vidioc_cropcap                 = cx18_cropcap,
	.vidioc_s_crop                  = cx18_s_crop,
	.vidioc_g_crop                  = cx18_g_crop,
	.vidioc_g_input                 = cx18_g_input,
	.vidioc_s_input                 = cx18_s_input,
	.vidioc_g_frequency             = cx18_g_frequency,
	.vidioc_s_frequency             = cx18_s_frequency,
	.vidioc_s_tuner                 = cx18_s_tuner,
	.vidioc_g_tuner                 = cx18_g_tuner,
	.vidioc_g_enc_index             = cx18_g_enc_index,
	.vidioc_g_std                   = cx18_g_std,
	.vidioc_s_std                   = cx18_s_std,
	.vidioc_log_status              = cx18_log_status,
	.vidioc_enum_fmt_vid_cap        = cx18_enum_fmt_vid_cap,
	.vidioc_encoder_cmd             = cx18_encoder_cmd,
	.vidioc_try_encoder_cmd         = cx18_try_encoder_cmd,
	.vidioc_g_fmt_vid_cap           = cx18_g_fmt_vid_cap,
	.vidioc_g_fmt_vbi_cap           = cx18_g_fmt_vbi_cap,
	.vidioc_g_fmt_sliced_vbi_cap    = cx18_g_fmt_sliced_vbi_cap,
	.vidioc_s_fmt_vid_cap           = cx18_s_fmt_vid_cap,
	.vidioc_s_fmt_vbi_cap           = cx18_s_fmt_vbi_cap,
	.vidioc_s_fmt_sliced_vbi_cap    = cx18_s_fmt_sliced_vbi_cap,
	.vidioc_try_fmt_vid_cap         = cx18_try_fmt_vid_cap,
	.vidioc_try_fmt_vbi_cap         = cx18_try_fmt_vbi_cap,
	.vidioc_try_fmt_sliced_vbi_cap  = cx18_try_fmt_sliced_vbi_cap,
	.vidioc_g_sliced_vbi_cap        = cx18_g_sliced_vbi_cap,
	.vidioc_g_chip_ident            = cx18_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register              = cx18_g_register,
	.vidioc_s_register              = cx18_s_register,
#endif
	.vidioc_default                 = cx18_default,
};

void cx18_set_funcs(struct video_device *vdev)
{
	vdev->ioctl_ops = &cx18_ioctl_ops;
}
