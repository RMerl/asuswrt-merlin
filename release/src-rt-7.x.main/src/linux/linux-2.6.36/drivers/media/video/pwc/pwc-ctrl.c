/* Driver for Philips webcam
   Functions that send various control messages to the webcam, including
   video modes.
   (C) 1999-2003 Nemosoft Unv.
   (C) 2004-2006 Luc Saillard (luc@saillard.org)

   NOTE: this version of pwc is an unofficial (modified) release of pwc & pcwx
   driver and thus may have bugs that are not present in the original version.
   Please send bug reports and support requests to <luc@saillard.org>.

   NOTE: this version of pwc is an unofficial (modified) release of pwc & pcwx
   driver and thus may have bugs that are not present in the original version.
   Please send bug reports and support requests to <luc@saillard.org>.
   The decompression routines have been implemented by reverse-engineering the
   Nemosoft binary pwcx module. Caveat emptor.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
   Changes
   2001/08/03  Alvarado   Added methods for changing white balance and
			  red/green gains
 */

/* Control functions for the cam; brightness, contrast, video mode, etc. */

#ifdef __KERNEL__
#include <asm/uaccess.h>
#endif
#include <asm/errno.h>

#include "pwc.h"
#include "pwc-uncompress.h"
#include "pwc-kiara.h"
#include "pwc-timon.h"
#include "pwc-dec1.h"
#include "pwc-dec23.h"

/* Request types: video */
#define SET_LUM_CTL			0x01
#define GET_LUM_CTL			0x02
#define SET_CHROM_CTL			0x03
#define GET_CHROM_CTL			0x04
#define SET_STATUS_CTL			0x05
#define GET_STATUS_CTL			0x06
#define SET_EP_STREAM_CTL		0x07
#define GET_EP_STREAM_CTL		0x08
#define GET_XX_CTL			0x09
#define SET_XX_CTL			0x0A
#define GET_XY_CTL			0x0B
#define SET_XY_CTL			0x0C
#define SET_MPT_CTL			0x0D
#define GET_MPT_CTL			0x0E

/* Selectors for the Luminance controls [GS]ET_LUM_CTL */
#define AGC_MODE_FORMATTER			0x2000
#define PRESET_AGC_FORMATTER			0x2100
#define SHUTTER_MODE_FORMATTER			0x2200
#define PRESET_SHUTTER_FORMATTER		0x2300
#define PRESET_CONTOUR_FORMATTER		0x2400
#define AUTO_CONTOUR_FORMATTER			0x2500
#define BACK_LIGHT_COMPENSATION_FORMATTER	0x2600
#define CONTRAST_FORMATTER			0x2700
#define DYNAMIC_NOISE_CONTROL_FORMATTER		0x2800
#define FLICKERLESS_MODE_FORMATTER		0x2900
#define AE_CONTROL_SPEED			0x2A00
#define BRIGHTNESS_FORMATTER			0x2B00
#define GAMMA_FORMATTER				0x2C00

/* Selectors for the Chrominance controls [GS]ET_CHROM_CTL */
#define WB_MODE_FORMATTER			0x1000
#define AWB_CONTROL_SPEED_FORMATTER		0x1100
#define AWB_CONTROL_DELAY_FORMATTER		0x1200
#define PRESET_MANUAL_RED_GAIN_FORMATTER	0x1300
#define PRESET_MANUAL_BLUE_GAIN_FORMATTER	0x1400
#define COLOUR_MODE_FORMATTER			0x1500
#define SATURATION_MODE_FORMATTER1		0x1600
#define SATURATION_MODE_FORMATTER2		0x1700

/* Selectors for the Status controls [GS]ET_STATUS_CTL */
#define SAVE_USER_DEFAULTS_FORMATTER		0x0200
#define RESTORE_USER_DEFAULTS_FORMATTER		0x0300
#define RESTORE_FACTORY_DEFAULTS_FORMATTER	0x0400
#define READ_AGC_FORMATTER			0x0500
#define READ_SHUTTER_FORMATTER			0x0600
#define READ_RED_GAIN_FORMATTER			0x0700
#define READ_BLUE_GAIN_FORMATTER		0x0800
#define GET_STATUS_B00				0x0B00
#define SENSOR_TYPE_FORMATTER1			0x0C00
#define GET_STATUS_3000				0x3000
#define READ_RAW_Y_MEAN_FORMATTER		0x3100
#define SET_POWER_SAVE_MODE_FORMATTER		0x3200
#define MIRROR_IMAGE_FORMATTER			0x3300
#define LED_FORMATTER				0x3400
#define LOWLIGHT				0x3500
#define GET_STATUS_3600				0x3600
#define SENSOR_TYPE_FORMATTER2			0x3700
#define GET_STATUS_3800				0x3800
#define GET_STATUS_4000				0x4000
#define GET_STATUS_4100				0x4100	/* Get */
#define CTL_STATUS_4200				0x4200	/* [GS] 1 */

/* Formatters for the Video Endpoint controls [GS]ET_EP_STREAM_CTL */
#define VIDEO_OUTPUT_CONTROL_FORMATTER		0x0100

/* Formatters for the motorized pan & tilt [GS]ET_MPT_CTL */
#define PT_RELATIVE_CONTROL_FORMATTER		0x01
#define PT_RESET_CONTROL_FORMATTER		0x02
#define PT_STATUS_FORMATTER			0x03

static const char *size2name[PSZ_MAX] =
{
	"subQCIF",
	"QSIF",
	"QCIF",
	"SIF",
	"CIF",
	"VGA",
};

/********/

/* Entries for the Nala (645/646) camera; the Nala doesn't have compression
   preferences, so you either get compressed or non-compressed streams.

   An alternate value of 0 means this mode is not available at all.
 */

#define PWC_FPS_MAX_NALA 8

struct Nala_table_entry {
	char alternate;			/* USB alternate setting */
	int compressed;			/* Compressed yes/no */

	unsigned char mode[3];		/* precomputed mode table */
};

static unsigned int Nala_fps_vector[PWC_FPS_MAX_NALA] = { 4, 5, 7, 10, 12, 15, 20, 24 };

static struct Nala_table_entry Nala_table[PSZ_MAX][PWC_FPS_MAX_NALA] =
{
#include "pwc-nala.h"
};

static void pwc_set_image_buffer_size(struct pwc_device *pdev);

/****************************************************************************/

static int _send_control_msg(struct pwc_device *pdev,
	u8 request, u16 value, int index, void *buf, int buflen, int timeout)
{
	int rc;
	void *kbuf = NULL;

	if (buflen) {
		kbuf = kmalloc(buflen, GFP_KERNEL); /* not allowed on stack */
		if (kbuf == NULL)
			return -ENOMEM;
		memcpy(kbuf, buf, buflen);
	}

	rc = usb_control_msg(pdev->udev, usb_sndctrlpipe(pdev->udev, 0),
		request,
		USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		value,
		index,
		kbuf, buflen, timeout);

	kfree(kbuf);
	return rc;
}

static int recv_control_msg(struct pwc_device *pdev,
	u8 request, u16 value, void *buf, int buflen)
{
	int rc;
	void *kbuf = kmalloc(buflen, GFP_KERNEL); /* not allowed on stack */

	if (kbuf == NULL)
		return -ENOMEM;

	rc = usb_control_msg(pdev->udev, usb_rcvctrlpipe(pdev->udev, 0),
		request,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		value,
		pdev->vcinterface,
		kbuf, buflen, 500);
	memcpy(buf, kbuf, buflen);
	kfree(kbuf);
	return rc;
}

static inline int send_video_command(struct pwc_device *pdev,
	int index, void *buf, int buflen)
{
	return _send_control_msg(pdev,
		SET_EP_STREAM_CTL,
		VIDEO_OUTPUT_CONTROL_FORMATTER,
		index,
		buf, buflen, 1000);
}

static inline int send_control_msg(struct pwc_device *pdev,
	u8 request, u16 value, void *buf, int buflen)
{
	return _send_control_msg(pdev,
		request, value, pdev->vcinterface, buf, buflen, 500);
}



static int set_video_mode_Nala(struct pwc_device *pdev, int size, int frames)
{
	unsigned char buf[3];
	int ret, fps;
	struct Nala_table_entry *pEntry;
	int frames2frames[31] =
	{ /* closest match of framerate */
	   0,  0,  0,  0,  4,  /*  0-4  */
	   5,  5,  7,  7, 10,  /*  5-9  */
	  10, 10, 12, 12, 15,  /* 10-14 */
	  15, 15, 15, 20, 20,  /* 15-19 */
	  20, 20, 20, 24, 24,  /* 20-24 */
	  24, 24, 24, 24, 24,  /* 25-29 */
	  24                   /* 30    */
	};
	int frames2table[31] =
	{ 0, 0, 0, 0, 0, /*  0-4  */
	  1, 1, 1, 2, 2, /*  5-9  */
	  3, 3, 4, 4, 4, /* 10-14 */
	  5, 5, 5, 5, 5, /* 15-19 */
	  6, 6, 6, 6, 7, /* 20-24 */
	  7, 7, 7, 7, 7, /* 25-29 */
	  7              /* 30    */
	};

	if (size < 0 || size > PSZ_CIF || frames < 4 || frames > 25)
		return -EINVAL;
	frames = frames2frames[frames];
	fps = frames2table[frames];
	pEntry = &Nala_table[size][fps];
	if (pEntry->alternate == 0)
		return -EINVAL;

	memcpy(buf, pEntry->mode, 3);
	ret = send_video_command(pdev, pdev->vendpoint, buf, 3);
	if (ret < 0) {
		PWC_DEBUG_MODULE("Failed to send video command... %d\n", ret);
		return ret;
	}
	if (pEntry->compressed && pdev->vpalette != VIDEO_PALETTE_RAW)
		pwc_dec1_init(pdev->type, pdev->release, buf, pdev->decompress_data);

	pdev->cmd_len = 3;
	memcpy(pdev->cmd_buf, buf, 3);

	/* Set various parameters */
	pdev->vframes = frames;
	pdev->vsize = size;
	pdev->valternate = pEntry->alternate;
	pdev->image = pwc_image_sizes[size];
	pdev->frame_size = (pdev->image.x * pdev->image.y * 3) / 2;
	if (pEntry->compressed) {
		if (pdev->release < 5) { /* 4 fold compression */
			pdev->vbandlength = 528;
			pdev->frame_size /= 4;
		}
		else {
			pdev->vbandlength = 704;
			pdev->frame_size /= 3;
		}
	}
	else
		pdev->vbandlength = 0;
	return 0;
}


static int set_video_mode_Timon(struct pwc_device *pdev, int size, int frames, int compression, int snapshot)
{
	unsigned char buf[13];
	const struct Timon_table_entry *pChoose;
	int ret, fps;

	if (size >= PSZ_MAX || frames < 5 || frames > 30 || compression < 0 || compression > 3)
		return -EINVAL;
	if (size == PSZ_VGA && frames > 15)
		return -EINVAL;
	fps = (frames / 5) - 1;

	/* Find a supported framerate with progressively higher compression ratios
	   if the preferred ratio is not available.
	*/
	pChoose = NULL;
	while (compression <= 3) {
	   pChoose = &Timon_table[size][fps][compression];
	   if (pChoose->alternate != 0)
	     break;
	   compression++;
	}
	if (pChoose == NULL || pChoose->alternate == 0)
		return -ENOENT; /* Not supported. */

	memcpy(buf, pChoose->mode, 13);
	if (snapshot)
		buf[0] |= 0x80;
	ret = send_video_command(pdev, pdev->vendpoint, buf, 13);
	if (ret < 0)
		return ret;

	if (pChoose->bandlength > 0 && pdev->vpalette != VIDEO_PALETTE_RAW)
		pwc_dec23_init(pdev, pdev->type, buf);

	pdev->cmd_len = 13;
	memcpy(pdev->cmd_buf, buf, 13);

	/* Set various parameters */
	pdev->vframes = frames;
	pdev->vsize = size;
	pdev->vsnapshot = snapshot;
	pdev->valternate = pChoose->alternate;
	pdev->image = pwc_image_sizes[size];
	pdev->vbandlength = pChoose->bandlength;
	if (pChoose->bandlength > 0)
		pdev->frame_size = (pChoose->bandlength * pdev->image.y) / 4;
	else
		pdev->frame_size = (pdev->image.x * pdev->image.y * 12) / 8;
	return 0;
}


static int set_video_mode_Kiara(struct pwc_device *pdev, int size, int frames, int compression, int snapshot)
{
	const struct Kiara_table_entry *pChoose = NULL;
	int fps, ret;
	unsigned char buf[12];
	struct Kiara_table_entry RawEntry = {6, 773, 1272, {0xAD, 0xF4, 0x10, 0x27, 0xB6, 0x24, 0x96, 0x02, 0x30, 0x05, 0x03, 0x80}};

	if (size >= PSZ_MAX || frames < 5 || frames > 30 || compression < 0 || compression > 3)
		return -EINVAL;
	if (size == PSZ_VGA && frames > 15)
		return -EINVAL;
	fps = (frames / 5) - 1;

	/* special case: VGA @ 5 fps and snapshot is raw bayer mode */
	if (size == PSZ_VGA && frames == 5 && snapshot && pdev->vpalette == VIDEO_PALETTE_RAW)
	{
		/* Only available in case the raw palette is selected or
		   we have the decompressor available. This mode is
		   only available in compressed form
		*/
		PWC_DEBUG_SIZE("Choosing VGA/5 BAYER mode.\n");
		pChoose = &RawEntry;
	}
	else
	{
		/* Find a supported framerate with progressively higher compression ratios
		   if the preferred ratio is not available.
		   Skip this step when using RAW modes.
		*/
		snapshot = 0;
		while (compression <= 3) {
			pChoose = &Kiara_table[size][fps][compression];
			if (pChoose->alternate != 0)
				break;
			compression++;
		}
	}
	if (pChoose == NULL || pChoose->alternate == 0)
		return -ENOENT; /* Not supported. */

	PWC_TRACE("Using alternate setting %d.\n", pChoose->alternate);

	/* usb_control_msg won't take staticly allocated arrays as argument?? */
	memcpy(buf, pChoose->mode, 12);
	if (snapshot)
		buf[0] |= 0x80;

	/* Firmware bug: video endpoint is 5, but commands are sent to endpoint 4 */
	ret = send_video_command(pdev, 4 /* pdev->vendpoint */, buf, 12);
	if (ret < 0)
		return ret;

	if (pChoose->bandlength > 0 && pdev->vpalette != VIDEO_PALETTE_RAW)
		pwc_dec23_init(pdev, pdev->type, buf);

	pdev->cmd_len = 12;
	memcpy(pdev->cmd_buf, buf, 12);
	/* All set and go */
	pdev->vframes = frames;
	pdev->vsize = size;
	pdev->vsnapshot = snapshot;
	pdev->valternate = pChoose->alternate;
	pdev->image = pwc_image_sizes[size];
	pdev->vbandlength = pChoose->bandlength;
	if (pdev->vbandlength > 0)
		pdev->frame_size = (pdev->vbandlength * pdev->image.y) / 4;
	else
		pdev->frame_size = (pdev->image.x * pdev->image.y * 12) / 8;
	PWC_TRACE("frame_size=%d, vframes=%d, vsize=%d, vsnapshot=%d, vbandlength=%d\n",
	    pdev->frame_size,pdev->vframes,pdev->vsize,pdev->vsnapshot,pdev->vbandlength);
	return 0;
}



/**
   @pdev: device structure
   @width: viewport width
   @height: viewport height
   @frame: framerate, in fps
   @compression: preferred compression ratio
   @snapshot: snapshot mode or streaming
 */
int pwc_set_video_mode(struct pwc_device *pdev, int width, int height, int frames, int compression, int snapshot)
{
	int ret, size;

	PWC_DEBUG_FLOW("set_video_mode(%dx%d @ %d, palette %d).\n", width, height, frames, pdev->vpalette);
	size = pwc_decode_size(pdev, width, height);
	if (size < 0) {
		PWC_DEBUG_MODULE("Could not find suitable size.\n");
		return -ERANGE;
	}
	PWC_TRACE("decode_size = %d.\n", size);

	if (DEVICE_USE_CODEC1(pdev->type)) {
		ret = set_video_mode_Nala(pdev, size, frames);

	} else if (DEVICE_USE_CODEC3(pdev->type)) {
		ret = set_video_mode_Kiara(pdev, size, frames, compression, snapshot);

	} else {
		ret = set_video_mode_Timon(pdev, size, frames, compression, snapshot);
	}
	if (ret < 0) {
		PWC_ERROR("Failed to set video mode %s@%d fps; return code = %d\n", size2name[size], frames, ret);
		return ret;
	}
	pdev->view.x = width;
	pdev->view.y = height;
	pdev->frame_total_size = pdev->frame_size + pdev->frame_header_size + pdev->frame_trailer_size;
	pwc_set_image_buffer_size(pdev);
	PWC_DEBUG_SIZE("Set viewport to %dx%d, image size is %dx%d.\n", width, height, pwc_image_sizes[size].x, pwc_image_sizes[size].y);
	return 0;
}

static unsigned int pwc_get_fps_Nala(struct pwc_device *pdev, unsigned int index, unsigned int size)
{
	unsigned int i;

	for (i = 0; i < PWC_FPS_MAX_NALA; i++) {
		if (Nala_table[size][i].alternate) {
			if (index--==0) return Nala_fps_vector[i];
		}
	}
	return 0;
}

static unsigned int pwc_get_fps_Kiara(struct pwc_device *pdev, unsigned int index, unsigned int size)
{
	unsigned int i;

	for (i = 0; i < PWC_FPS_MAX_KIARA; i++) {
		if (Kiara_table[size][i][3].alternate) {
			if (index--==0) return Kiara_fps_vector[i];
		}
	}
	return 0;
}

static unsigned int pwc_get_fps_Timon(struct pwc_device *pdev, unsigned int index, unsigned int size)
{
	unsigned int i;

	for (i=0; i < PWC_FPS_MAX_TIMON; i++) {
		if (Timon_table[size][i][3].alternate) {
			if (index--==0) return Timon_fps_vector[i];
		}
	}
	return 0;
}

unsigned int pwc_get_fps(struct pwc_device *pdev, unsigned int index, unsigned int size)
{
	unsigned int ret;

	if (DEVICE_USE_CODEC1(pdev->type)) {
		ret = pwc_get_fps_Nala(pdev, index, size);

	} else if (DEVICE_USE_CODEC3(pdev->type)) {
		ret = pwc_get_fps_Kiara(pdev, index, size);

	} else {
		ret = pwc_get_fps_Timon(pdev, index, size);
	}

	return ret;
}

#define BLACK_Y 0
#define BLACK_U 128
#define BLACK_V 128

static void pwc_set_image_buffer_size(struct pwc_device *pdev)
{
	int i, factor = 0;

	/* for PALETTE_YUV420P */
	switch(pdev->vpalette)
	{
	case VIDEO_PALETTE_YUV420P:
		factor = 6;
		break;
	case VIDEO_PALETTE_RAW:
		factor = 6; /* can be uncompressed YUV420P */
		break;
	}

	/* Set sizes in bytes */
	pdev->image.size = pdev->image.x * pdev->image.y * factor / 4;
	pdev->view.size  = pdev->view.x  * pdev->view.y  * factor / 4;

	/* Align offset, or you'll get some very weird results in
	   YUV420 mode... x must be multiple of 4 (to get the Y's in
	   place), and y even (or you'll mixup U & V). This is less of a
	   problem for YUV420P.
	 */
	pdev->offset.x = ((pdev->view.x - pdev->image.x) / 2) & 0xFFFC;
	pdev->offset.y = ((pdev->view.y - pdev->image.y) / 2) & 0xFFFE;

	/* Fill buffers with black colors */
	for (i = 0; i < pwc_mbufs; i++) {
		unsigned char *p = pdev->image_data + pdev->images[i].offset;
		memset(p, BLACK_Y, pdev->view.x * pdev->view.y);
		p += pdev->view.x * pdev->view.y;
		memset(p, BLACK_U, pdev->view.x * pdev->view.y/4);
		p += pdev->view.x * pdev->view.y/4;
		memset(p, BLACK_V, pdev->view.x * pdev->view.y/4);
	}
}



/* BRIGHTNESS */

int pwc_get_brightness(struct pwc_device *pdev)
{
	char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, BRIGHTNESS_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	return buf;
}

int pwc_set_brightness(struct pwc_device *pdev, int value)
{
	char buf;

	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	buf = (value >> 9) & 0x7f;
	return send_control_msg(pdev,
		SET_LUM_CTL, BRIGHTNESS_FORMATTER, &buf, sizeof(buf));
}

/* CONTRAST */

int pwc_get_contrast(struct pwc_device *pdev)
{
	char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, CONTRAST_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	return buf;
}

int pwc_set_contrast(struct pwc_device *pdev, int value)
{
	char buf;

	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	buf = (value >> 10) & 0x3f;
	return send_control_msg(pdev,
		SET_LUM_CTL, CONTRAST_FORMATTER, &buf, sizeof(buf));
}

/* GAMMA */

int pwc_get_gamma(struct pwc_device *pdev)
{
	char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, GAMMA_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	return buf;
}

int pwc_set_gamma(struct pwc_device *pdev, int value)
{
	char buf;

	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	buf = (value >> 11) & 0x1f;
	return send_control_msg(pdev,
		SET_LUM_CTL, GAMMA_FORMATTER, &buf, sizeof(buf));
}


/* SATURATION */

/* return a value between [-100 , 100] */
int pwc_get_saturation(struct pwc_device *pdev, int *value)
{
	char buf;
	int ret, saturation_register;

	if (pdev->type < 675)
		return -EINVAL;
	if (pdev->type < 730)
		saturation_register = SATURATION_MODE_FORMATTER2;
	else
		saturation_register = SATURATION_MODE_FORMATTER1;
	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, saturation_register, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = (signed)buf;
	return 0;
}

/* @param value saturation color between [-100 , 100] */
int pwc_set_saturation(struct pwc_device *pdev, int value)
{
	char buf;
	int saturation_register;

	if (pdev->type < 675)
		return -EINVAL;
	if (value < -100)
		value = -100;
	if (value > 100)
		value = 100;
	if (pdev->type < 730)
		saturation_register = SATURATION_MODE_FORMATTER2;
	else
		saturation_register = SATURATION_MODE_FORMATTER1;
	return send_control_msg(pdev,
		SET_CHROM_CTL, saturation_register, &buf, sizeof(buf));
}

/* AGC */

int pwc_set_agc(struct pwc_device *pdev, int mode, int value)
{
	char buf;
	int ret;

	if (mode)
		buf = 0x0; /* auto */
	else
		buf = 0xff; /* fixed */

	ret = send_control_msg(pdev,
		SET_LUM_CTL, AGC_MODE_FORMATTER, &buf, sizeof(buf));

	if (!mode && ret >= 0) {
		if (value < 0)
			value = 0;
		if (value > 0xffff)
			value = 0xffff;
		buf = (value >> 10) & 0x3F;
		ret = send_control_msg(pdev,
			SET_LUM_CTL, PRESET_AGC_FORMATTER, &buf, sizeof(buf));
	}
	if (ret < 0)
		return ret;
	return 0;
}

int pwc_get_agc(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, AGC_MODE_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf != 0) { /* fixed */
		ret = recv_control_msg(pdev,
			GET_LUM_CTL, PRESET_AGC_FORMATTER, &buf, sizeof(buf));
		if (ret < 0)
			return ret;
		if (buf > 0x3F)
			buf = 0x3F;
		*value = (buf << 10);
	}
	else { /* auto */
		ret = recv_control_msg(pdev,
			GET_STATUS_CTL, READ_AGC_FORMATTER, &buf, sizeof(buf));
		if (ret < 0)
			return ret;
		/* Gah... this value ranges from 0x00 ... 0x9F */
		if (buf > 0x9F)
			buf = 0x9F;
		*value = -(48 + buf * 409);
	}

	return 0;
}

int pwc_set_shutter_speed(struct pwc_device *pdev, int mode, int value)
{
	char buf[2];
	int speed, ret;


	if (mode)
		buf[0] = 0x0;	/* auto */
	else
		buf[0] = 0xff; /* fixed */

	ret = send_control_msg(pdev,
		SET_LUM_CTL, SHUTTER_MODE_FORMATTER, &buf, 1);

	if (!mode && ret >= 0) {
		if (value < 0)
			value = 0;
		if (value > 0xffff)
			value = 0xffff;

		if (DEVICE_USE_CODEC2(pdev->type)) {
			/* speed ranges from 0x0 to 0x290 (656) */
			speed = (value / 100);
			buf[1] = speed >> 8;
			buf[0] = speed & 0xff;
		} else if (DEVICE_USE_CODEC3(pdev->type)) {
			/* speed seems to range from 0x0 to 0xff */
			buf[1] = 0;
			buf[0] = value >> 8;
		}

		ret = send_control_msg(pdev,
			SET_LUM_CTL, PRESET_SHUTTER_FORMATTER,
			&buf, sizeof(buf));
	}
	return ret;
}

/* This function is not exported to v4l1, so output values between 0 -> 256 */
int pwc_get_shutter_speed(struct pwc_device *pdev, int *value)
{
	unsigned char buf[2];
	int ret;

	ret = recv_control_msg(pdev,
		GET_STATUS_CTL, READ_SHUTTER_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = buf[0] + (buf[1] << 8);
	if (DEVICE_USE_CODEC2(pdev->type)) {
		/* speed ranges from 0x0 to 0x290 (656) */
		*value *= 256/656;
	} else if (DEVICE_USE_CODEC3(pdev->type)) {
		/* speed seems to range from 0x0 to 0xff */
	}
	return 0;
}


/* POWER */

int pwc_camera_power(struct pwc_device *pdev, int power)
{
	char buf;

	if (pdev->type < 675 || (pdev->type < 730 && pdev->release < 6))
		return 0;	/* Not supported by Nala or Timon < release 6 */

	if (power)
		buf = 0x00; /* active */
	else
		buf = 0xFF; /* power save */
	return send_control_msg(pdev,
		SET_STATUS_CTL, SET_POWER_SAVE_MODE_FORMATTER,
		&buf, sizeof(buf));
}



/* private calls */

int pwc_restore_user(struct pwc_device *pdev)
{
	return send_control_msg(pdev,
		SET_STATUS_CTL, RESTORE_USER_DEFAULTS_FORMATTER, NULL, 0);
}

int pwc_save_user(struct pwc_device *pdev)
{
	return send_control_msg(pdev,
		SET_STATUS_CTL, SAVE_USER_DEFAULTS_FORMATTER, NULL, 0);
}

int pwc_restore_factory(struct pwc_device *pdev)
{
	return send_control_msg(pdev,
		SET_STATUS_CTL, RESTORE_FACTORY_DEFAULTS_FORMATTER, NULL, 0);
}

 /* ************************************************* */
 /* Patch by Alvarado: (not in the original version   */

 /*
  * the camera recognizes modes from 0 to 4:
  *
  * 00: indoor (incandescant lighting)
  * 01: outdoor (sunlight)
  * 02: fluorescent lighting
  * 03: manual
  * 04: auto
  */
int pwc_set_awb(struct pwc_device *pdev, int mode)
{
	char buf;
	int ret;

	if (mode < 0)
	    mode = 0;

	if (mode > 4)
	    mode = 4;

	buf = mode & 0x07; /* just the lowest three bits */

	ret = send_control_msg(pdev,
		SET_CHROM_CTL, WB_MODE_FORMATTER, &buf, sizeof(buf));

	if (ret < 0)
		return ret;
	return 0;
}

int pwc_get_awb(struct pwc_device *pdev)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, WB_MODE_FORMATTER, &buf, sizeof(buf));

	if (ret < 0)
		return ret;
	return buf;
}

int pwc_set_red_gain(struct pwc_device *pdev, int value)
{
	unsigned char buf;

	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	/* only the msb is considered */
	buf = value >> 8;
	return send_control_msg(pdev,
		SET_CHROM_CTL, PRESET_MANUAL_RED_GAIN_FORMATTER,
		&buf, sizeof(buf));
}

int pwc_get_red_gain(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, PRESET_MANUAL_RED_GAIN_FORMATTER,
		&buf, sizeof(buf));
	if (ret < 0)
	    return ret;
	*value = buf << 8;
	return 0;
}


int pwc_set_blue_gain(struct pwc_device *pdev, int value)
{
	unsigned char buf;

	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	/* only the msb is considered */
	buf = value >> 8;
	return send_control_msg(pdev,
		SET_CHROM_CTL, PRESET_MANUAL_BLUE_GAIN_FORMATTER,
		&buf, sizeof(buf));
}

int pwc_get_blue_gain(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, PRESET_MANUAL_BLUE_GAIN_FORMATTER,
		&buf, sizeof(buf));
	if (ret < 0)
	    return ret;
	*value = buf << 8;
	return 0;
}


/* The following two functions are different, since they only read the
   internal red/blue gains, which may be different from the manual
   gains set or read above.
 */
static int pwc_read_red_gain(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_STATUS_CTL, READ_RED_GAIN_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = buf << 8;
	return 0;
}

static int pwc_read_blue_gain(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_STATUS_CTL, READ_BLUE_GAIN_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = buf << 8;
	return 0;
}


static int pwc_set_wb_speed(struct pwc_device *pdev, int speed)
{
	unsigned char buf;

	/* useful range is 0x01..0x20 */
	buf = speed / 0x7f0;
	return send_control_msg(pdev,
		SET_CHROM_CTL, AWB_CONTROL_SPEED_FORMATTER, &buf, sizeof(buf));
}

static int pwc_get_wb_speed(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, AWB_CONTROL_SPEED_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = buf * 0x7f0;
	return 0;
}


static int pwc_set_wb_delay(struct pwc_device *pdev, int delay)
{
	unsigned char buf;

	/* useful range is 0x01..0x3F */
	buf = (delay >> 10);
	return send_control_msg(pdev,
		SET_CHROM_CTL, AWB_CONTROL_DELAY_FORMATTER, &buf, sizeof(buf));
}

static int pwc_get_wb_delay(struct pwc_device *pdev, int *value)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, AWB_CONTROL_DELAY_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*value = buf << 10;
	return 0;
}


int pwc_set_leds(struct pwc_device *pdev, int on_value, int off_value)
{
	unsigned char buf[2];

	if (pdev->type < 730)
		return 0;
	on_value /= 100;
	off_value /= 100;
	if (on_value < 0)
		on_value = 0;
	if (on_value > 0xff)
		on_value = 0xff;
	if (off_value < 0)
		off_value = 0;
	if (off_value > 0xff)
		off_value = 0xff;

	buf[0] = on_value;
	buf[1] = off_value;

	return send_control_msg(pdev,
		SET_STATUS_CTL, LED_FORMATTER, &buf, sizeof(buf));
}

static int pwc_get_leds(struct pwc_device *pdev, int *on_value, int *off_value)
{
	unsigned char buf[2];
	int ret;

	if (pdev->type < 730) {
		*on_value = -1;
		*off_value = -1;
		return 0;
	}

	ret = recv_control_msg(pdev,
		GET_STATUS_CTL, LED_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*on_value = buf[0] * 100;
	*off_value = buf[1] * 100;
	return 0;
}

int pwc_set_contour(struct pwc_device *pdev, int contour)
{
	unsigned char buf;
	int ret;

	if (contour < 0)
		buf = 0xff; /* auto contour on */
	else
		buf = 0x0; /* auto contour off */
	ret = send_control_msg(pdev,
		SET_LUM_CTL, AUTO_CONTOUR_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (contour < 0)
		return 0;
	if (contour > 0xffff)
		contour = 0xffff;

	buf = (contour >> 10); /* contour preset is [0..3f] */
	ret = send_control_msg(pdev,
		SET_LUM_CTL, PRESET_CONTOUR_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	return 0;
}

int pwc_get_contour(struct pwc_device *pdev, int *contour)
{
	unsigned char buf;
	int ret;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, AUTO_CONTOUR_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf == 0) {
		/* auto mode off, query current preset value */
		ret = recv_control_msg(pdev,
			GET_LUM_CTL, PRESET_CONTOUR_FORMATTER,
			&buf, sizeof(buf));
		if (ret < 0)
			return ret;
		*contour = buf << 10;
	}
	else
		*contour = -1;
	return 0;
}


int pwc_set_backlight(struct pwc_device *pdev, int backlight)
{
	unsigned char buf;

	if (backlight)
		buf = 0xff;
	else
		buf = 0x0;
	return send_control_msg(pdev,
		SET_LUM_CTL, BACK_LIGHT_COMPENSATION_FORMATTER,
		&buf, sizeof(buf));
}

int pwc_get_backlight(struct pwc_device *pdev, int *backlight)
{
	int ret;
	unsigned char buf;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, BACK_LIGHT_COMPENSATION_FORMATTER,
		&buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*backlight = !!buf;
	return 0;
}

int pwc_set_colour_mode(struct pwc_device *pdev, int colour)
{
	unsigned char buf;

	if (colour)
		buf = 0xff;
	else
		buf = 0x0;
	return send_control_msg(pdev,
		SET_CHROM_CTL, COLOUR_MODE_FORMATTER, &buf, sizeof(buf));
}

int pwc_get_colour_mode(struct pwc_device *pdev, int *colour)
{
	int ret;
	unsigned char buf;

	ret = recv_control_msg(pdev,
		GET_CHROM_CTL, COLOUR_MODE_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*colour = !!buf;
	return 0;
}


int pwc_set_flicker(struct pwc_device *pdev, int flicker)
{
	unsigned char buf;

	if (flicker)
		buf = 0xff;
	else
		buf = 0x0;
	return send_control_msg(pdev,
		SET_LUM_CTL, FLICKERLESS_MODE_FORMATTER, &buf, sizeof(buf));
}

int pwc_get_flicker(struct pwc_device *pdev, int *flicker)
{
	int ret;
	unsigned char buf;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, FLICKERLESS_MODE_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*flicker = !!buf;
	return 0;
}

int pwc_set_dynamic_noise(struct pwc_device *pdev, int noise)
{
	unsigned char buf;

	if (noise < 0)
		noise = 0;
	if (noise > 3)
		noise = 3;
	buf = noise;
	return send_control_msg(pdev,
		SET_LUM_CTL, DYNAMIC_NOISE_CONTROL_FORMATTER,
		&buf, sizeof(buf));
}

int pwc_get_dynamic_noise(struct pwc_device *pdev, int *noise)
{
	int ret;
	unsigned char buf;

	ret = recv_control_msg(pdev,
		GET_LUM_CTL, DYNAMIC_NOISE_CONTROL_FORMATTER,
		&buf, sizeof(buf));
	if (ret < 0)
		return ret;
	*noise = buf;
	return 0;
}

static int _pwc_mpt_reset(struct pwc_device *pdev, int flags)
{
	unsigned char buf;

	buf = flags & 0x03; // only lower two bits are currently used
	return send_control_msg(pdev,
		SET_MPT_CTL, PT_RESET_CONTROL_FORMATTER, &buf, sizeof(buf));
}

int pwc_mpt_reset(struct pwc_device *pdev, int flags)
{
	int ret;
	ret = _pwc_mpt_reset(pdev, flags);
	if (ret >= 0) {
		pdev->pan_angle = 0;
		pdev->tilt_angle = 0;
	}
	return ret;
}

static int _pwc_mpt_set_angle(struct pwc_device *pdev, int pan, int tilt)
{
	unsigned char buf[4];

	/* set new relative angle; angles are expressed in degrees * 100,
	   but cam as .5 degree resolution, hence divide by 200. Also
	   the angle must be multiplied by 64 before it's send to
	   the cam (??)
	 */
	pan  =  64 * pan  / 100;
	tilt = -64 * tilt / 100; /* positive tilt is down, which is not what the user would expect */
	buf[0] = pan & 0xFF;
	buf[1] = (pan >> 8) & 0xFF;
	buf[2] = tilt & 0xFF;
	buf[3] = (tilt >> 8) & 0xFF;
	return send_control_msg(pdev,
		SET_MPT_CTL, PT_RELATIVE_CONTROL_FORMATTER, &buf, sizeof(buf));
}

int pwc_mpt_set_angle(struct pwc_device *pdev, int pan, int tilt)
{
	int ret;

	/* check absolute ranges */
	if (pan  < pdev->angle_range.pan_min  ||
	    pan  > pdev->angle_range.pan_max  ||
	    tilt < pdev->angle_range.tilt_min ||
	    tilt > pdev->angle_range.tilt_max)
		return -ERANGE;

	/* go to relative range, check again */
	pan  -= pdev->pan_angle;
	tilt -= pdev->tilt_angle;
	/* angles are specified in degrees * 100, thus the limit = 36000 */
	if (pan < -36000 || pan > 36000 || tilt < -36000 || tilt > 36000)
		return -ERANGE;

	ret = _pwc_mpt_set_angle(pdev, pan, tilt);
	if (ret >= 0) {
		pdev->pan_angle  += pan;
		pdev->tilt_angle += tilt;
	}
	if (ret == -EPIPE) /* stall -> out of range */
		ret = -ERANGE;
	return ret;
}

static int pwc_mpt_get_status(struct pwc_device *pdev, struct pwc_mpt_status *status)
{
	int ret;
	unsigned char buf[5];

	ret = recv_control_msg(pdev,
		GET_MPT_CTL, PT_STATUS_FORMATTER, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	status->status = buf[0] & 0x7; // 3 bits are used for reporting
	status->time_pan = (buf[1] << 8) + buf[2];
	status->time_tilt = (buf[3] << 8) + buf[4];
	return 0;
}


int pwc_get_cmos_sensor(struct pwc_device *pdev, int *sensor)
{
	unsigned char buf;
	int ret = -1, request;

	if (pdev->type < 675)
		request = SENSOR_TYPE_FORMATTER1;
	else if (pdev->type < 730)
		return -1; /* The Vesta series doesn't have this call */
	else
		request = SENSOR_TYPE_FORMATTER2;

	ret = recv_control_msg(pdev,
		GET_STATUS_CTL, request, &buf, sizeof(buf));
	if (ret < 0)
		return ret;
	if (pdev->type < 675)
		*sensor = buf | 0x100;
	else
		*sensor = buf;
	return 0;
}


 /* End of Add-Ons                                    */
 /* ************************************************* */

/* Linux 2.5.something and 2.6 pass direct pointers to arguments of
   ioctl() calls. With 2.4, you have to do tedious copy_from_user()
   and copy_to_user() calls. With these macros we circumvent this,
   and let me maintain only one source file. The functionality is
   exactly the same otherwise.
 */

/* define local variable for arg */
#define ARG_DEF(ARG_type, ARG_name)\
	ARG_type *ARG_name = arg;
/* copy arg to local variable */
#define ARG_IN(ARG_name) /* nothing */
/* argument itself (referenced) */
#define ARGR(ARG_name) (*ARG_name)
/* argument address */
#define ARGA(ARG_name) ARG_name
/* copy local variable to arg */
#define ARG_OUT(ARG_name) /* nothing */

long pwc_ioctl(struct pwc_device *pdev, unsigned int cmd, void *arg)
{
	long ret = 0;

	switch(cmd) {
	case VIDIOCPWCRUSER:
	{
		if (pwc_restore_user(pdev))
			ret = -EINVAL;
		break;
	}

	case VIDIOCPWCSUSER:
	{
		if (pwc_save_user(pdev))
			ret = -EINVAL;
		break;
	}

	case VIDIOCPWCFACTORY:
	{
		if (pwc_restore_factory(pdev))
			ret = -EINVAL;
		break;
	}

	case VIDIOCPWCSCQUAL:
	{
		ARG_DEF(int, qual)

		ARG_IN(qual)
		if (ARGR(qual) < 0 || ARGR(qual) > 3)
			ret = -EINVAL;
		else
			ret = pwc_try_video_mode(pdev, pdev->view.x, pdev->view.y, pdev->vframes, ARGR(qual), pdev->vsnapshot);
		if (ret >= 0)
			pdev->vcompression = ARGR(qual);
		break;
	}

	case VIDIOCPWCGCQUAL:
	{
		ARG_DEF(int, qual)

		ARGR(qual) = pdev->vcompression;
		ARG_OUT(qual)
		break;
	}

	case VIDIOCPWCPROBE:
	{
		ARG_DEF(struct pwc_probe, probe)

		strcpy(ARGR(probe).name, pdev->vdev->name);
		ARGR(probe).type = pdev->type;
		ARG_OUT(probe)
		break;
	}

	case VIDIOCPWCGSERIAL:
	{
		ARG_DEF(struct pwc_serial, serial)

		strcpy(ARGR(serial).serial, pdev->serial);
		ARG_OUT(serial)
		break;
	}

	case VIDIOCPWCSAGC:
	{
		ARG_DEF(int, agc)

		ARG_IN(agc)
		if (pwc_set_agc(pdev, ARGR(agc) < 0 ? 1 : 0, ARGR(agc)))
			ret = -EINVAL;
		break;
	}

	case VIDIOCPWCGAGC:
	{
		ARG_DEF(int, agc)

		if (pwc_get_agc(pdev, ARGA(agc)))
			ret = -EINVAL;
		ARG_OUT(agc)
		break;
	}

	case VIDIOCPWCSSHUTTER:
	{
		ARG_DEF(int, shutter_speed)

		ARG_IN(shutter_speed)
		ret = pwc_set_shutter_speed(pdev, ARGR(shutter_speed) < 0 ? 1 : 0, ARGR(shutter_speed));
		break;
	}

	case VIDIOCPWCSAWB:
	{
		ARG_DEF(struct pwc_whitebalance, wb)

		ARG_IN(wb)
		ret = pwc_set_awb(pdev, ARGR(wb).mode);
		if (ret >= 0 && ARGR(wb).mode == PWC_WB_MANUAL) {
			pwc_set_red_gain(pdev, ARGR(wb).manual_red);
			pwc_set_blue_gain(pdev, ARGR(wb).manual_blue);
		}
		break;
	}

	case VIDIOCPWCGAWB:
	{
		ARG_DEF(struct pwc_whitebalance, wb)

		memset(ARGA(wb), 0, sizeof(struct pwc_whitebalance));
		ARGR(wb).mode = pwc_get_awb(pdev);
		if (ARGR(wb).mode < 0)
			ret = -EINVAL;
		else {
			if (ARGR(wb).mode == PWC_WB_MANUAL) {
				ret = pwc_get_red_gain(pdev, &ARGR(wb).manual_red);
				if (ret < 0)
					break;
				ret = pwc_get_blue_gain(pdev, &ARGR(wb).manual_blue);
				if (ret < 0)
					break;
			}
			if (ARGR(wb).mode == PWC_WB_AUTO) {
				ret = pwc_read_red_gain(pdev, &ARGR(wb).read_red);
				if (ret < 0)
					break;
				ret = pwc_read_blue_gain(pdev, &ARGR(wb).read_blue);
				if (ret < 0)
					break;
			}
		}
		ARG_OUT(wb)
		break;
	}

	case VIDIOCPWCSAWBSPEED:
	{
		ARG_DEF(struct pwc_wb_speed, wbs)

		if (ARGR(wbs).control_speed > 0) {
			ret = pwc_set_wb_speed(pdev, ARGR(wbs).control_speed);
		}
		if (ARGR(wbs).control_delay > 0) {
			ret = pwc_set_wb_delay(pdev, ARGR(wbs).control_delay);
		}
		break;
	}

	case VIDIOCPWCGAWBSPEED:
	{
		ARG_DEF(struct pwc_wb_speed, wbs)

		ret = pwc_get_wb_speed(pdev, &ARGR(wbs).control_speed);
		if (ret < 0)
			break;
		ret = pwc_get_wb_delay(pdev, &ARGR(wbs).control_delay);
		if (ret < 0)
			break;
		ARG_OUT(wbs)
		break;
	}

	case VIDIOCPWCSLED:
	{
		ARG_DEF(struct pwc_leds, leds)

		ARG_IN(leds)
		ret = pwc_set_leds(pdev, ARGR(leds).led_on, ARGR(leds).led_off);
		break;
	}


	case VIDIOCPWCGLED:
	{
		ARG_DEF(struct pwc_leds, leds)

		ret = pwc_get_leds(pdev, &ARGR(leds).led_on, &ARGR(leds).led_off);
		ARG_OUT(leds)
		break;
	}

	case VIDIOCPWCSCONTOUR:
	{
		ARG_DEF(int, contour)

		ARG_IN(contour)
		ret = pwc_set_contour(pdev, ARGR(contour));
		break;
	}

	case VIDIOCPWCGCONTOUR:
	{
		ARG_DEF(int, contour)

		ret = pwc_get_contour(pdev, ARGA(contour));
		ARG_OUT(contour)
		break;
	}

	case VIDIOCPWCSBACKLIGHT:
	{
		ARG_DEF(int, backlight)

		ARG_IN(backlight)
		ret = pwc_set_backlight(pdev, ARGR(backlight));
		break;
	}

	case VIDIOCPWCGBACKLIGHT:
	{
		ARG_DEF(int, backlight)

		ret = pwc_get_backlight(pdev, ARGA(backlight));
		ARG_OUT(backlight)
		break;
	}

	case VIDIOCPWCSFLICKER:
	{
		ARG_DEF(int, flicker)

		ARG_IN(flicker)
		ret = pwc_set_flicker(pdev, ARGR(flicker));
		break;
	}

	case VIDIOCPWCGFLICKER:
	{
		ARG_DEF(int, flicker)

		ret = pwc_get_flicker(pdev, ARGA(flicker));
		ARG_OUT(flicker)
		break;
	}

	case VIDIOCPWCSDYNNOISE:
	{
		ARG_DEF(int, dynnoise)

		ARG_IN(dynnoise)
		ret = pwc_set_dynamic_noise(pdev, ARGR(dynnoise));
		break;
	}

	case VIDIOCPWCGDYNNOISE:
	{
		ARG_DEF(int, dynnoise)

		ret = pwc_get_dynamic_noise(pdev, ARGA(dynnoise));
		ARG_OUT(dynnoise);
		break;
	}

	case VIDIOCPWCGREALSIZE:
	{
		ARG_DEF(struct pwc_imagesize, size)

		ARGR(size).width = pdev->image.x;
		ARGR(size).height = pdev->image.y;
		ARG_OUT(size)
		break;
	}

	case VIDIOCPWCMPTRESET:
	{
		if (pdev->features & FEATURE_MOTOR_PANTILT)
		{
			ARG_DEF(int, flags)

			ARG_IN(flags)
			ret = pwc_mpt_reset(pdev, ARGR(flags));
		}
		else
		{
			ret = -ENXIO;
		}
		break;
	}

	case VIDIOCPWCMPTGRANGE:
	{
		if (pdev->features & FEATURE_MOTOR_PANTILT)
		{
			ARG_DEF(struct pwc_mpt_range, range)

			ARGR(range) = pdev->angle_range;
			ARG_OUT(range)
		}
		else
		{
			ret = -ENXIO;
		}
		break;
	}

	case VIDIOCPWCMPTSANGLE:
	{
		int new_pan, new_tilt;

		if (pdev->features & FEATURE_MOTOR_PANTILT)
		{
			ARG_DEF(struct pwc_mpt_angles, angles)

			ARG_IN(angles)
			/* The camera can only set relative angles, so
			   do some calculations when getting an absolute angle .
			 */
			if (ARGR(angles).absolute)
			{
				new_pan  = ARGR(angles).pan;
				new_tilt = ARGR(angles).tilt;
			}
			else
			{
				new_pan  = pdev->pan_angle  + ARGR(angles).pan;
				new_tilt = pdev->tilt_angle + ARGR(angles).tilt;
			}
			ret = pwc_mpt_set_angle(pdev, new_pan, new_tilt);
		}
		else
		{
			ret = -ENXIO;
		}
		break;
	}

	case VIDIOCPWCMPTGANGLE:
	{

		if (pdev->features & FEATURE_MOTOR_PANTILT)
		{
			ARG_DEF(struct pwc_mpt_angles, angles)

			ARGR(angles).absolute = 1;
			ARGR(angles).pan  = pdev->pan_angle;
			ARGR(angles).tilt = pdev->tilt_angle;
			ARG_OUT(angles)
		}
		else
		{
			ret = -ENXIO;
		}
		break;
	}

	case VIDIOCPWCMPTSTATUS:
	{
		if (pdev->features & FEATURE_MOTOR_PANTILT)
		{
			ARG_DEF(struct pwc_mpt_status, status)

			ret = pwc_mpt_get_status(pdev, ARGA(status));
			ARG_OUT(status)
		}
		else
		{
			ret = -ENXIO;
		}
		break;
	}

	case VIDIOCPWCGVIDCMD:
	{
		ARG_DEF(struct pwc_video_command, vcmd);

		ARGR(vcmd).type = pdev->type;
		ARGR(vcmd).release = pdev->release;
		ARGR(vcmd).command_len = pdev->cmd_len;
		memcpy(&ARGR(vcmd).command_buf, pdev->cmd_buf, pdev->cmd_len);
		ARGR(vcmd).bandlength = pdev->vbandlength;
		ARGR(vcmd).frame_size = pdev->frame_size;
		ARG_OUT(vcmd)
		break;
	}
	/*
	case VIDIOCPWCGVIDTABLE:
	{
		ARG_DEF(struct pwc_table_init_buffer, table);
		ARGR(table).len = pdev->cmd_len;
		memcpy(&ARGR(table).buffer, pdev->decompress_data, pdev->decompressor->table_size);
		ARG_OUT(table)
		break;
	}
	*/

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	if (ret > 0)
		return 0;
	return ret;
}


/* vim: set cinoptions= formatoptions=croql cindent shiftwidth=8 tabstop=8: */
