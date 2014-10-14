/*
 * USB USBVISION Video device driver 0.9.10
 *
 *
 *
 * Copyright (c) 1999-2005 Joerg Heckenbach <joerg@heckenbach-aw.de>
 *
 * This module is part of usbvision driver project.
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
 *
 * Let's call the version 0.... until compression decoding is completely
 * implemented.
 *
 * This driver is written by Jose Ignacio Gijon and Joerg Heckenbach.
 * It was based on USB CPiA driver written by Peter Pregler,
 * Scott J. Bertin and Johannes Erdfelt
 * Ideas are taken from bttv driver by Ralph Metzler, Marcus Metzler &
 * Gerd Knorr and zoran 36120/36125 driver by Pauline Middelink
 * Updates to driver completed by Dwaine P. Garden
 *
 *
 * TODO:
 *     - use submit_urb for all setup packets
 *     - Fix memory settings for nt1004. It is 4 times as big as the
 *       nt1003 memory.
 *     - Add audio on endpoint 3 for nt1004 chip.
 *         Seems impossible, needs a codec interface.  Which one?
 *     - Clean up the driver.
 *     - optimization for performance.
 *     - Add Videotext capability (VBI).  Working on it.....
 *     - Check audio for other devices
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/slab.h>
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
#include <media/v4l2-ioctl.h>
#include <media/tuner.h>

#include <linux/workqueue.h>

#include "usbvision.h"
#include "usbvision-cards.h"

#define DRIVER_AUTHOR "Joerg Heckenbach <joerg@heckenbach-aw.de>, \
Dwaine Garden <DwaineGarden@rogers.com>"
#define DRIVER_NAME "usbvision"
#define DRIVER_ALIAS "USBVision"
#define DRIVER_DESC "USBVision USB Video Device Driver for Linux"
#define DRIVER_LICENSE "GPL"
#define USBVISION_DRIVER_VERSION_MAJOR 0
#define USBVISION_DRIVER_VERSION_MINOR 9
#define USBVISION_DRIVER_VERSION_PATCHLEVEL 10
#define USBVISION_DRIVER_VERSION KERNEL_VERSION(USBVISION_DRIVER_VERSION_MAJOR,\
USBVISION_DRIVER_VERSION_MINOR,\
USBVISION_DRIVER_VERSION_PATCHLEVEL)
#define USBVISION_VERSION_STRING __stringify(USBVISION_DRIVER_VERSION_MAJOR) \
"." __stringify(USBVISION_DRIVER_VERSION_MINOR) \
"." __stringify(USBVISION_DRIVER_VERSION_PATCHLEVEL)

#define	ENABLE_HEXDUMP	0	/* Enable if you need it */


#ifdef USBVISION_DEBUG
	#define PDEBUG(level, fmt, args...) { \
		if (video_debug & (level)) \
			printk(KERN_INFO KBUILD_MODNAME ":[%s:%d] " fmt, \
				__func__, __LINE__ , ## args); \
	}
#else
	#define PDEBUG(level, fmt, args...) do {} while (0)
#endif

#define DBG_IO		(1 << 1)
#define DBG_PROBE	(1 << 2)
#define DBG_MMAP	(1 << 3)

/* String operations */
#define rmspace(str)	while (*str == ' ') str++;
#define goto2next(str)	while (*str != ' ') str++; while (*str == ' ') str++;


/* sequential number of usbvision device */
static int usbvision_nr;

static struct usbvision_v4l2_format_st usbvision_v4l2_format[] = {
	{ 1, 1,  8, V4L2_PIX_FMT_GREY    , "GREY" },
	{ 1, 2, 16, V4L2_PIX_FMT_RGB565  , "RGB565" },
	{ 1, 3, 24, V4L2_PIX_FMT_RGB24   , "RGB24" },
	{ 1, 4, 32, V4L2_PIX_FMT_RGB32   , "RGB32" },
	{ 1, 2, 16, V4L2_PIX_FMT_RGB555  , "RGB555" },
	{ 1, 2, 16, V4L2_PIX_FMT_YUYV    , "YUV422" },
	{ 1, 2, 12, V4L2_PIX_FMT_YVU420  , "YUV420P" }, /* 1.5 ! */
	{ 1, 2, 16, V4L2_PIX_FMT_YUV422P , "YUV422P" }
};

/* Function prototypes */
static void usbvision_release(struct usb_usbvision *usbvision);

/* Default initialization of device driver parameters */
/* Set the default format for ISOC endpoint */
static int isoc_mode = ISOC_MODE_COMPRESS;
/* Set the default Debug Mode of the device driver */
static int video_debug;
/* Set the default device to power on at startup */
static int power_on_at_open = 1;
/* Sequential Number of Video Device */
static int video_nr = -1;
/* Sequential Number of Radio Device */
static int radio_nr = -1;

/* Grab parameters for the device driver */

/* Showing parameters under SYSFS */
module_param(isoc_mode, int, 0444);
module_param(video_debug, int, 0444);
module_param(power_on_at_open, int, 0444);
module_param(video_nr, int, 0444);
module_param(radio_nr, int, 0444);

MODULE_PARM_DESC(isoc_mode, " Set the default format for ISOC endpoint.  Default: 0x60 (Compression On)");
MODULE_PARM_DESC(video_debug, " Set the default Debug Mode of the device driver.  Default: 0 (Off)");
MODULE_PARM_DESC(power_on_at_open, " Set the default device to power on when device is opened.  Default: 1 (On)");
MODULE_PARM_DESC(video_nr, "Set video device number (/dev/videoX).  Default: -1 (autodetect)");
MODULE_PARM_DESC(radio_nr, "Set radio device number (/dev/radioX).  Default: -1 (autodetect)");


/* Misc stuff */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_VERSION(USBVISION_VERSION_STRING);
MODULE_ALIAS(DRIVER_ALIAS);


/*****************************************************************************/
/* SYSFS Code - Copied from the stv680.c usb module.			     */
/* Device information is located at /sys/class/video4linux/video0            */
/* Device parameters information is located at /sys/module/usbvision         */
/* Device USB Information is located at                                      */
/*   /sys/bus/usb/drivers/USBVision Video Grabber                            */
/*****************************************************************************/

#define YES_NO(x) ((x) ? "Yes" : "No")

static inline struct usb_usbvision *cd_to_usbvision(struct device *cd)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	return video_get_drvdata(vdev);
}

static ssize_t show_version(struct device *cd,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", USBVISION_VERSION_STRING);
}
static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

static ssize_t show_model(struct device *cd,
			  struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	return sprintf(buf, "%s\n",
		       usbvision_device_data[usbvision->dev_model].model_string);
}
static DEVICE_ATTR(model, S_IRUGO, show_model, NULL);

static ssize_t show_hue(struct device *cd,
			struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_HUE;
	ctrl.value = 0;
	if (usbvision->user)
		call_all(usbvision, core, g_ctrl, &ctrl);
	return sprintf(buf, "%d\n", ctrl.value);
}
static DEVICE_ATTR(hue, S_IRUGO, show_hue, NULL);

static ssize_t show_contrast(struct device *cd,
			     struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_CONTRAST;
	ctrl.value = 0;
	if (usbvision->user)
		call_all(usbvision, core, g_ctrl, &ctrl);
	return sprintf(buf, "%d\n", ctrl.value);
}
static DEVICE_ATTR(contrast, S_IRUGO, show_contrast, NULL);

static ssize_t show_brightness(struct device *cd,
			       struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_BRIGHTNESS;
	ctrl.value = 0;
	if (usbvision->user)
		call_all(usbvision, core, g_ctrl, &ctrl);
	return sprintf(buf, "%d\n", ctrl.value);
}
static DEVICE_ATTR(brightness, S_IRUGO, show_brightness, NULL);

static ssize_t show_saturation(struct device *cd,
			       struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_SATURATION;
	ctrl.value = 0;
	if (usbvision->user)
		call_all(usbvision, core, g_ctrl, &ctrl);
	return sprintf(buf, "%d\n", ctrl.value);
}
static DEVICE_ATTR(saturation, S_IRUGO, show_saturation, NULL);

static ssize_t show_streaming(struct device *cd,
			      struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	return sprintf(buf, "%s\n",
		       YES_NO(usbvision->streaming == stream_on ? 1 : 0));
}
static DEVICE_ATTR(streaming, S_IRUGO, show_streaming, NULL);

static ssize_t show_compression(struct device *cd,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	return sprintf(buf, "%s\n",
		       YES_NO(usbvision->isoc_mode == ISOC_MODE_COMPRESS));
}
static DEVICE_ATTR(compression, S_IRUGO, show_compression, NULL);

static ssize_t show_device_bridge(struct device *cd,
				  struct device_attribute *attr, char *buf)
{
	struct video_device *vdev =
		container_of(cd, struct video_device, dev);
	struct usb_usbvision *usbvision = video_get_drvdata(vdev);
	return sprintf(buf, "%d\n", usbvision->bridge_type);
}
static DEVICE_ATTR(bridge, S_IRUGO, show_device_bridge, NULL);

static void usbvision_create_sysfs(struct video_device *vdev)
{
	int res;

	if (!vdev)
		return;
	do {
		res = device_create_file(&vdev->dev, &dev_attr_version);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_model);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_hue);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_contrast);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_brightness);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_saturation);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_streaming);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_compression);
		if (res < 0)
			break;
		res = device_create_file(&vdev->dev, &dev_attr_bridge);
		if (res >= 0)
			return;
	} while (0);

	dev_err(&vdev->dev, "%s error: %d\n", __func__, res);
}

static void usbvision_remove_sysfs(struct video_device *vdev)
{
	if (vdev) {
		device_remove_file(&vdev->dev, &dev_attr_version);
		device_remove_file(&vdev->dev, &dev_attr_model);
		device_remove_file(&vdev->dev, &dev_attr_hue);
		device_remove_file(&vdev->dev, &dev_attr_contrast);
		device_remove_file(&vdev->dev, &dev_attr_brightness);
		device_remove_file(&vdev->dev, &dev_attr_saturation);
		device_remove_file(&vdev->dev, &dev_attr_streaming);
		device_remove_file(&vdev->dev, &dev_attr_compression);
		device_remove_file(&vdev->dev, &dev_attr_bridge);
	}
}

/*
 * usbvision_open()
 *
 * This is part of Video 4 Linux API. The driver can be opened by one
 * client only (checks internal counter 'usbvision->user'). The procedure
 * then allocates buffers needed for video processing.
 *
 */
static int usbvision_v4l2_open(struct file *file)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int err_code = 0;

	PDEBUG(DBG_IO, "open");

	usbvision_reset_power_off_timer(usbvision);

	if (usbvision->user)
		err_code = -EBUSY;
	else {
		/* Allocate memory for the scratch ring buffer */
		err_code = usbvision_scratch_alloc(usbvision);
		if (isoc_mode == ISOC_MODE_COMPRESS) {
			/* Allocate intermediate decompression buffers
			   only if needed */
			err_code = usbvision_decompress_alloc(usbvision);
		}
		if (err_code) {
			/* Deallocate all buffers if trouble */
			usbvision_scratch_free(usbvision);
			usbvision_decompress_free(usbvision);
		}
	}

	/* If so far no errors then we shall start the camera */
	if (!err_code) {
		if (usbvision->power == 0) {
			usbvision_power_on(usbvision);
			usbvision_i2c_register(usbvision);
		}

		/* Send init sequence only once, it's large! */
		if (!usbvision->initialized) {
			int setup_ok = 0;
			setup_ok = usbvision_setup(usbvision, isoc_mode);
			if (setup_ok)
				usbvision->initialized = 1;
			else
				err_code = -EBUSY;
		}

		if (!err_code) {
			usbvision_begin_streaming(usbvision);
			err_code = usbvision_init_isoc(usbvision);
			/* device must be initialized before isoc transfer */
			usbvision_muxsel(usbvision, 0);
			usbvision->user++;
		} else {
			if (power_on_at_open) {
				usbvision_i2c_unregister(usbvision);
				usbvision_power_off(usbvision);
				usbvision->initialized = 0;
			}
		}
	}

	/* prepare queues */
	usbvision_empty_framequeues(usbvision);

	PDEBUG(DBG_IO, "success");
	return err_code;
}

/*
 * usbvision_v4l2_close()
 *
 * This is part of Video 4 Linux API. The procedure
 * stops streaming and deallocates all buffers that were earlier
 * allocated in usbvision_v4l2_open().
 *
 */
static int usbvision_v4l2_close(struct file *file)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	PDEBUG(DBG_IO, "close");

	usbvision_audio_off(usbvision);
	usbvision_restart_isoc(usbvision);
	usbvision_stop_isoc(usbvision);

	usbvision_decompress_free(usbvision);
	usbvision_frames_free(usbvision);
	usbvision_empty_framequeues(usbvision);
	usbvision_scratch_free(usbvision);

	usbvision->user--;

	if (power_on_at_open) {
		/* power off in a little while
		   to avoid off/on every close/open short sequences */
		usbvision_set_power_off_timer(usbvision);
		usbvision->initialized = 0;
	}

	if (usbvision->remove_pending) {
		printk(KERN_INFO "%s: Final disconnect\n", __func__);
		usbvision_release(usbvision);
	}

	PDEBUG(DBG_IO, "success");
	return 0;
}


/*
 * usbvision_ioctl()
 *
 * This is part of Video 4 Linux API. The procedure handles ioctl() calls.
 *
 */
#ifdef CONFIG_VIDEO_ADV_DEBUG
static int vidioc_g_register(struct file *file, void *priv,
				struct v4l2_dbg_register *reg)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int err_code;

	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;
	/* NT100x has a 8-bit register space */
	err_code = usbvision_read_reg(usbvision, reg->reg&0xff);
	if (err_code < 0) {
		dev_err(&usbvision->vdev->dev,
			"%s: VIDIOC_DBG_G_REGISTER failed: error %d\n",
				__func__, err_code);
		return err_code;
	}
	reg->val = err_code;
	reg->size = 1;
	return 0;
}

static int vidioc_s_register(struct file *file, void *priv,
				struct v4l2_dbg_register *reg)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int err_code;

	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;
	/* NT100x has a 8-bit register space */
	err_code = usbvision_write_reg(usbvision, reg->reg & 0xff, reg->val);
	if (err_code < 0) {
		dev_err(&usbvision->vdev->dev,
			"%s: VIDIOC_DBG_S_REGISTER failed: error %d\n",
				__func__, err_code);
		return err_code;
	}
	return 0;
}
#endif

static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *vc)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	strlcpy(vc->driver, "USBVision", sizeof(vc->driver));
	strlcpy(vc->card,
		usbvision_device_data[usbvision->dev_model].model_string,
		sizeof(vc->card));
	usb_make_path(usbvision->dev, vc->bus_info, sizeof(vc->bus_info));
	vc->version = USBVISION_DRIVER_VERSION;
	vc->capabilities = V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_AUDIO |
		V4L2_CAP_READWRITE |
		V4L2_CAP_STREAMING |
		(usbvision->have_tuner ? V4L2_CAP_TUNER : 0);
	return 0;
}

static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *vi)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int chan;

	if (vi->index >= usbvision->video_inputs)
		return -EINVAL;
	if (usbvision->have_tuner)
		chan = vi->index;
	else
		chan = vi->index + 1; /* skip Television string*/

	/* Determine the requested input characteristics
	   specific for each usbvision card model */
	switch (chan) {
	case 0:
		if (usbvision_device_data[usbvision->dev_model].video_channels == 4) {
			strcpy(vi->name, "White Video Input");
		} else {
			strcpy(vi->name, "Television");
			vi->type = V4L2_INPUT_TYPE_TUNER;
			vi->audioset = 1;
			vi->tuner = chan;
			vi->std = USBVISION_NORMS;
		}
		break;
	case 1:
		vi->type = V4L2_INPUT_TYPE_CAMERA;
		if (usbvision_device_data[usbvision->dev_model].video_channels == 4)
			strcpy(vi->name, "Green Video Input");
		else
			strcpy(vi->name, "Composite Video Input");
		vi->std = V4L2_STD_PAL;
		break;
	case 2:
		vi->type = V4L2_INPUT_TYPE_CAMERA;
		if (usbvision_device_data[usbvision->dev_model].video_channels == 4)
			strcpy(vi->name, "Yellow Video Input");
		else
			strcpy(vi->name, "S-Video Input");
		vi->std = V4L2_STD_PAL;
		break;
	case 3:
		vi->type = V4L2_INPUT_TYPE_CAMERA;
		strcpy(vi->name, "Red Video Input");
		vi->std = V4L2_STD_PAL;
		break;
	}
	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *input)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	*input = usbvision->ctl_input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int input)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	if (input >= usbvision->video_inputs)
		return -EINVAL;

	usbvision_muxsel(usbvision, input);
	usbvision_set_input(usbvision);
	usbvision_set_output(usbvision,
			     usbvision->curwidth,
			     usbvision->curheight);
	return 0;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *id)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	usbvision->tvnorm_id = *id;

	call_all(usbvision, core, s_std, usbvision->tvnorm_id);
	/* propagate the change to the decoder */
	usbvision_muxsel(usbvision, usbvision->ctl_input);

	return 0;
}

static int vidioc_g_tuner(struct file *file, void *priv,
				struct v4l2_tuner *vt)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	if (!usbvision->have_tuner || vt->index)	/* Only tuner 0 */
		return -EINVAL;
	if (usbvision->radio) {
		strcpy(vt->name, "Radio");
		vt->type = V4L2_TUNER_RADIO;
	} else {
		strcpy(vt->name, "Television");
	}
	/* Let clients fill in the remainder of this struct */
	call_all(usbvision, tuner, g_tuner, vt);

	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv,
				struct v4l2_tuner *vt)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	/* Only no or one tuner for now */
	if (!usbvision->have_tuner || vt->index)
		return -EINVAL;
	/* let clients handle this */
	call_all(usbvision, tuner, s_tuner, vt);

	return 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
				struct v4l2_frequency *freq)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	freq->tuner = 0; /* Only one tuner */
	if (usbvision->radio)
		freq->type = V4L2_TUNER_RADIO;
	else
		freq->type = V4L2_TUNER_ANALOG_TV;
	freq->frequency = usbvision->freq;

	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
				struct v4l2_frequency *freq)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	/* Only no or one tuner for now */
	if (!usbvision->have_tuner || freq->tuner)
		return -EINVAL;

	usbvision->freq = freq->frequency;
	call_all(usbvision, tuner, s_frequency, freq);

	return 0;
}

static int vidioc_g_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	if (usbvision->radio)
		strcpy(a->name, "Radio");
	else
		strcpy(a->name, "TV");

	return 0;
}

static int vidioc_s_audio(struct file *file, void *fh,
			  struct v4l2_audio *a)
{
	if (a->index)
		return -EINVAL;
	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *ctrl)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	call_all(usbvision, core, queryctrl, ctrl);

	if (!ctrl->type)
		return -EINVAL;

	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	call_all(usbvision, core, g_ctrl, ctrl);
	return 0;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	call_all(usbvision, core, s_ctrl, ctrl);
	return 0;
}

static int vidioc_reqbufs(struct file *file,
			   void *priv, struct v4l2_requestbuffers *vr)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int ret;

	RESTRICT_TO_RANGE(vr->count, 1, USBVISION_NUMFRAMES);

	/* Check input validity:
	   the user must do a VIDEO CAPTURE and MMAP method. */
	if (vr->memory != V4L2_MEMORY_MMAP)
		return -EINVAL;

	if (usbvision->streaming == stream_on) {
		ret = usbvision_stream_interrupt(usbvision);
		if (ret)
			return ret;
	}

	usbvision_frames_free(usbvision);
	usbvision_empty_framequeues(usbvision);
	vr->count = usbvision_frames_alloc(usbvision, vr->count);

	usbvision->cur_frame = NULL;

	return 0;
}

static int vidioc_querybuf(struct file *file,
			    void *priv, struct v4l2_buffer *vb)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	struct usbvision_frame *frame;

	/* FIXME : must control
	   that buffers are mapped (VIDIOC_REQBUFS has been called) */
	if (vb->index >= usbvision->num_frames)
		return -EINVAL;
	/* Updating the corresponding frame state */
	vb->flags = 0;
	frame = &usbvision->frame[vb->index];
	if (frame->grabstate >= frame_state_ready)
		vb->flags |= V4L2_BUF_FLAG_QUEUED;
	if (frame->grabstate >= frame_state_done)
		vb->flags |= V4L2_BUF_FLAG_DONE;
	if (frame->grabstate == frame_state_unused)
		vb->flags |= V4L2_BUF_FLAG_MAPPED;
	vb->memory = V4L2_MEMORY_MMAP;

	vb->m.offset = vb->index * PAGE_ALIGN(usbvision->max_frame_size);

	vb->memory = V4L2_MEMORY_MMAP;
	vb->field = V4L2_FIELD_NONE;
	vb->length = usbvision->curwidth *
		usbvision->curheight *
		usbvision->palette.bytes_per_pixel;
	vb->timestamp = usbvision->frame[vb->index].timestamp;
	vb->sequence = usbvision->frame[vb->index].sequence;
	return 0;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *vb)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	struct usbvision_frame *frame;
	unsigned long lock_flags;

	/* FIXME : works only on VIDEO_CAPTURE MODE, MMAP. */
	if (vb->index >= usbvision->num_frames)
		return -EINVAL;

	frame = &usbvision->frame[vb->index];

	if (frame->grabstate != frame_state_unused)
		return -EAGAIN;

	/* Mark it as ready and enqueue frame */
	frame->grabstate = frame_state_ready;
	frame->scanstate = scan_state_scanning;
	frame->scanlength = 0;	/* Accumulated in usbvision_parse_data() */

	vb->flags &= ~V4L2_BUF_FLAG_DONE;

	/* set v4l2_format index */
	frame->v4l2_format = usbvision->palette;

	spin_lock_irqsave(&usbvision->queue_lock, lock_flags);
	list_add_tail(&usbvision->frame[vb->index].frame, &usbvision->inqueue);
	spin_unlock_irqrestore(&usbvision->queue_lock, lock_flags);

	return 0;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *vb)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int ret;
	struct usbvision_frame *f;
	unsigned long lock_flags;

	if (list_empty(&(usbvision->outqueue))) {
		if (usbvision->streaming == stream_idle)
			return -EINVAL;
		ret = wait_event_interruptible
			(usbvision->wait_frame,
			 !list_empty(&(usbvision->outqueue)));
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&usbvision->queue_lock, lock_flags);
	f = list_entry(usbvision->outqueue.next,
		       struct usbvision_frame, frame);
	list_del(usbvision->outqueue.next);
	spin_unlock_irqrestore(&usbvision->queue_lock, lock_flags);

	f->grabstate = frame_state_unused;

	vb->memory = V4L2_MEMORY_MMAP;
	vb->flags = V4L2_BUF_FLAG_MAPPED |
		V4L2_BUF_FLAG_QUEUED |
		V4L2_BUF_FLAG_DONE;
	vb->index = f->index;
	vb->sequence = f->sequence;
	vb->timestamp = f->timestamp;
	vb->field = V4L2_FIELD_NONE;
	vb->bytesused = f->scanlength;

	return 0;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	usbvision->streaming = stream_on;
	call_all(usbvision, video, s_stream, 1);

	return 0;
}

static int vidioc_streamoff(struct file *file,
			    void *priv, enum v4l2_buf_type type)
{
	struct usb_usbvision *usbvision = video_drvdata(file);

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (usbvision->streaming == stream_on) {
		usbvision_stream_interrupt(usbvision);
		/* Stop all video streamings */
		call_all(usbvision, video, s_stream, 0);
	}
	usbvision_empty_framequeues(usbvision);

	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *vfd)
{
	if (vfd->index >= USBVISION_SUPPORTED_PALETTES - 1)
		return -EINVAL;
	strcpy(vfd->description, usbvision_v4l2_format[vfd->index].desc);
	vfd->pixelformat = usbvision_v4l2_format[vfd->index].format;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *vf)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	vf->fmt.pix.width = usbvision->curwidth;
	vf->fmt.pix.height = usbvision->curheight;
	vf->fmt.pix.pixelformat = usbvision->palette.format;
	vf->fmt.pix.bytesperline =
		usbvision->curwidth * usbvision->palette.bytes_per_pixel;
	vf->fmt.pix.sizeimage = vf->fmt.pix.bytesperline * usbvision->curheight;
	vf->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	vf->fmt.pix.field = V4L2_FIELD_NONE; /* Always progressive image */

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			       struct v4l2_format *vf)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int format_idx;

	/* Find requested format in available ones */
	for (format_idx = 0; format_idx < USBVISION_SUPPORTED_PALETTES; format_idx++) {
		if (vf->fmt.pix.pixelformat ==
		   usbvision_v4l2_format[format_idx].format) {
			usbvision->palette = usbvision_v4l2_format[format_idx];
			break;
		}
	}
	/* robustness */
	if (format_idx == USBVISION_SUPPORTED_PALETTES)
		return -EINVAL;
	RESTRICT_TO_RANGE(vf->fmt.pix.width, MIN_FRAME_WIDTH, MAX_FRAME_WIDTH);
	RESTRICT_TO_RANGE(vf->fmt.pix.height, MIN_FRAME_HEIGHT, MAX_FRAME_HEIGHT);

	vf->fmt.pix.bytesperline = vf->fmt.pix.width*
		usbvision->palette.bytes_per_pixel;
	vf->fmt.pix.sizeimage = vf->fmt.pix.bytesperline*vf->fmt.pix.height;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
			       struct v4l2_format *vf)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int ret;

	ret = vidioc_try_fmt_vid_cap(file, priv, vf);
	if (ret)
		return ret;

	/* stop io in case it is already in progress */
	if (usbvision->streaming == stream_on) {
		ret = usbvision_stream_interrupt(usbvision);
		if (ret)
			return ret;
	}
	usbvision_frames_free(usbvision);
	usbvision_empty_framequeues(usbvision);

	usbvision->cur_frame = NULL;

	/* by now we are committed to the new data... */
	usbvision_set_output(usbvision, vf->fmt.pix.width, vf->fmt.pix.height);

	return 0;
}

static ssize_t usbvision_v4l2_read(struct file *file, char __user *buf,
		      size_t count, loff_t *ppos)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int noblock = file->f_flags & O_NONBLOCK;
	unsigned long lock_flags;
	int ret, i;
	struct usbvision_frame *frame;

	PDEBUG(DBG_IO, "%s: %ld bytes, noblock=%d", __func__,
	       (unsigned long)count, noblock);

	if (!USBVISION_IS_OPERATIONAL(usbvision) || (buf == NULL))
		return -EFAULT;

	/* This entry point is compatible with the mmap routines
	   so that a user can do either VIDIOC_QBUF/VIDIOC_DQBUF
	   to get frames or call read on the device. */
	if (!usbvision->num_frames) {
		/* First, allocate some frames to work with
		   if this has not been done with VIDIOC_REQBUF */
		usbvision_frames_free(usbvision);
		usbvision_empty_framequeues(usbvision);
		usbvision_frames_alloc(usbvision, USBVISION_NUMFRAMES);
	}

	if (usbvision->streaming != stream_on) {
		/* no stream is running, make it running ! */
		usbvision->streaming = stream_on;
		call_all(usbvision, video, s_stream, 1);
	}

	/* Then, enqueue as many frames as possible
	   (like a user of VIDIOC_QBUF would do) */
	for (i = 0; i < usbvision->num_frames; i++) {
		frame = &usbvision->frame[i];
		if (frame->grabstate == frame_state_unused) {
			/* Mark it as ready and enqueue frame */
			frame->grabstate = frame_state_ready;
			frame->scanstate = scan_state_scanning;
			/* Accumulated in usbvision_parse_data() */
			frame->scanlength = 0;

			/* set v4l2_format index */
			frame->v4l2_format = usbvision->palette;

			spin_lock_irqsave(&usbvision->queue_lock, lock_flags);
			list_add_tail(&frame->frame, &usbvision->inqueue);
			spin_unlock_irqrestore(&usbvision->queue_lock,
					       lock_flags);
		}
	}

	/* Then try to steal a frame (like a VIDIOC_DQBUF would do) */
	if (list_empty(&(usbvision->outqueue))) {
		if (noblock)
			return -EAGAIN;

		ret = wait_event_interruptible
			(usbvision->wait_frame,
			 !list_empty(&(usbvision->outqueue)));
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&usbvision->queue_lock, lock_flags);
	frame = list_entry(usbvision->outqueue.next,
			   struct usbvision_frame, frame);
	list_del(usbvision->outqueue.next);
	spin_unlock_irqrestore(&usbvision->queue_lock, lock_flags);

	/* An error returns an empty frame */
	if (frame->grabstate == frame_state_error) {
		frame->bytes_read = 0;
		return 0;
	}

	PDEBUG(DBG_IO, "%s: frmx=%d, bytes_read=%ld, scanlength=%ld",
	       __func__,
	       frame->index, frame->bytes_read, frame->scanlength);

	/* copy bytes to user space; we allow for partials reads */
	if ((count + frame->bytes_read) > (unsigned long)frame->scanlength)
		count = frame->scanlength - frame->bytes_read;

	if (copy_to_user(buf, frame->data + frame->bytes_read, count))
		return -EFAULT;

	frame->bytes_read += count;
	PDEBUG(DBG_IO, "%s: {copy} count used=%ld, new bytes_read=%ld",
	       __func__,
	       (unsigned long)count, frame->bytes_read);

	/* For now, forget the frame if it has not been read in one shot. */
/*	if (frame->bytes_read >= frame->scanlength) {*/ /* All data has been read */
		frame->bytes_read = 0;

		/* Mark it as available to be used again. */
		frame->grabstate = frame_state_unused;
/*	} */

	return count;
}

static int usbvision_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start,
		start = vma->vm_start;
	void *pos;
	u32 i;
	struct usb_usbvision *usbvision = video_drvdata(file);

	PDEBUG(DBG_MMAP, "mmap");

	if (!USBVISION_IS_OPERATIONAL(usbvision))
		return -EFAULT;

	if (!(vma->vm_flags & VM_WRITE) ||
	    size != PAGE_ALIGN(usbvision->max_frame_size)) {
		return -EINVAL;
	}

	for (i = 0; i < usbvision->num_frames; i++) {
		if (((PAGE_ALIGN(usbvision->max_frame_size)*i) >> PAGE_SHIFT) ==
		    vma->vm_pgoff)
			break;
	}
	if (i == usbvision->num_frames) {
		PDEBUG(DBG_MMAP,
		       "mmap: user supplied mapping address is out of range");
		return -EINVAL;
	}

	/* VM_IO is eventually going to replace PageReserved altogether */
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;	/* avoid to swap out this VMA */

	pos = usbvision->frame[i].data;
	while (size > 0) {
		if (vm_insert_page(vma, start, vmalloc_to_page(pos))) {
			PDEBUG(DBG_MMAP, "mmap: vm_insert_page failed");
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return 0;
}


/*
 * Here comes the stuff for radio on usbvision based devices
 *
 */
static int usbvision_radio_open(struct file *file)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int err_code = 0;

	PDEBUG(DBG_IO, "%s:", __func__);

	if (usbvision->user) {
		dev_err(&usbvision->rdev->dev,
			"%s: Someone tried to open an already opened USBVision Radio!\n",
				__func__);
		err_code = -EBUSY;
	} else {
		if (power_on_at_open) {
			usbvision_reset_power_off_timer(usbvision);
			if (usbvision->power == 0) {
				usbvision_power_on(usbvision);
				usbvision_i2c_register(usbvision);
			}
		}

		/* Alternate interface 1 is is the biggest frame size */
		err_code = usbvision_set_alternate(usbvision);
		if (err_code < 0) {
			usbvision->last_error = err_code;
			err_code = -EBUSY;
			goto out;
		}

		/* If so far no errors then we shall start the radio */
		usbvision->radio = 1;
		call_all(usbvision, tuner, s_radio);
		usbvision_set_audio(usbvision, USBVISION_AUDIO_RADIO);
		usbvision->user++;
	}

	if (err_code) {
		if (power_on_at_open) {
			usbvision_i2c_unregister(usbvision);
			usbvision_power_off(usbvision);
			usbvision->initialized = 0;
		}
	}
out:
	return err_code;
}


static int usbvision_radio_close(struct file *file)
{
	struct usb_usbvision *usbvision = video_drvdata(file);
	int err_code = 0;

	PDEBUG(DBG_IO, "");

	/* Set packet size to 0 */
	usbvision->iface_alt = 0;
	err_code = usb_set_interface(usbvision->dev, usbvision->iface,
				    usbvision->iface_alt);

	usbvision_audio_off(usbvision);
	usbvision->radio = 0;
	usbvision->user--;

	if (power_on_at_open) {
		usbvision_set_power_off_timer(usbvision);
		usbvision->initialized = 0;
	}

	if (usbvision->remove_pending) {
		printk(KERN_INFO "%s: Final disconnect\n", __func__);
		usbvision_release(usbvision);
	}

	PDEBUG(DBG_IO, "success");
	return err_code;
}

/* Video registration stuff */

/* Video template */
static const struct v4l2_file_operations usbvision_fops = {
	.owner             = THIS_MODULE,
	.open		= usbvision_v4l2_open,
	.release	= usbvision_v4l2_close,
	.read		= usbvision_v4l2_read,
	.mmap		= usbvision_v4l2_mmap,
	.unlocked_ioctl	= video_ioctl2,
/*	.poll		= video_poll, */
};

static const struct v4l2_ioctl_ops usbvision_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_audio       = vidioc_g_audio,
	.vidioc_s_audio       = vidioc_s_audio,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_g_tuner       = vidioc_g_tuner,
	.vidioc_s_tuner       = vidioc_s_tuner,
	.vidioc_g_frequency   = vidioc_g_frequency,
	.vidioc_s_frequency   = vidioc_s_frequency,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register    = vidioc_g_register,
	.vidioc_s_register    = vidioc_s_register,
#endif
};

static struct video_device usbvision_video_template = {
	.fops		= &usbvision_fops,
	.ioctl_ops	= &usbvision_ioctl_ops,
	.name           = "usbvision-video",
	.release	= video_device_release,
	.tvnorms        = USBVISION_NORMS,
	.current_norm   = V4L2_STD_PAL
};


/* Radio template */
static const struct v4l2_file_operations usbvision_radio_fops = {
	.owner             = THIS_MODULE,
	.open		= usbvision_radio_open,
	.release	= usbvision_radio_close,
	.unlocked_ioctl	= video_ioctl2,
};

static const struct v4l2_ioctl_ops usbvision_radio_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_audio       = vidioc_g_audio,
	.vidioc_s_audio       = vidioc_s_audio,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_g_tuner       = vidioc_g_tuner,
	.vidioc_s_tuner       = vidioc_s_tuner,
	.vidioc_g_frequency   = vidioc_g_frequency,
	.vidioc_s_frequency   = vidioc_s_frequency,
};

static struct video_device usbvision_radio_template = {
	.fops		= &usbvision_radio_fops,
	.name		= "usbvision-radio",
	.release	= video_device_release,
	.ioctl_ops	= &usbvision_radio_ioctl_ops,

	.tvnorms              = USBVISION_NORMS,
	.current_norm         = V4L2_STD_PAL
};


static struct video_device *usbvision_vdev_init(struct usb_usbvision *usbvision,
					struct video_device *vdev_template,
					char *name)
{
	struct usb_device *usb_dev = usbvision->dev;
	struct video_device *vdev;

	if (usb_dev == NULL) {
		dev_err(&usbvision->dev->dev,
			"%s: usbvision->dev is not set\n", __func__);
		return NULL;
	}

	vdev = video_device_alloc();
	if (NULL == vdev)
		return NULL;
	*vdev = *vdev_template;
	vdev->lock = &usbvision->v4l2_lock;
	vdev->v4l2_dev = &usbvision->v4l2_dev;
	snprintf(vdev->name, sizeof(vdev->name), "%s", name);
	video_set_drvdata(vdev, usbvision);
	return vdev;
}

/* unregister video4linux devices */
static void usbvision_unregister_video(struct usb_usbvision *usbvision)
{
	/* Radio Device: */
	if (usbvision->rdev) {
		PDEBUG(DBG_PROBE, "unregister %s [v4l2]",
		       video_device_node_name(usbvision->rdev));
		if (video_is_registered(usbvision->rdev))
			video_unregister_device(usbvision->rdev);
		else
			video_device_release(usbvision->rdev);
		usbvision->rdev = NULL;
	}

	/* Video Device: */
	if (usbvision->vdev) {
		PDEBUG(DBG_PROBE, "unregister %s [v4l2]",
		       video_device_node_name(usbvision->vdev));
		if (video_is_registered(usbvision->vdev))
			video_unregister_device(usbvision->vdev);
		else
			video_device_release(usbvision->vdev);
		usbvision->vdev = NULL;
	}
}

/* register video4linux devices */
static int __devinit usbvision_register_video(struct usb_usbvision *usbvision)
{
	/* Video Device: */
	usbvision->vdev = usbvision_vdev_init(usbvision,
					      &usbvision_video_template,
					      "USBVision Video");
	if (usbvision->vdev == NULL)
		goto err_exit;
	if (video_register_device(usbvision->vdev, VFL_TYPE_GRABBER, video_nr) < 0)
		goto err_exit;
	printk(KERN_INFO "USBVision[%d]: registered USBVision Video device %s [v4l2]\n",
	       usbvision->nr, video_device_node_name(usbvision->vdev));

	/* Radio Device: */
	if (usbvision_device_data[usbvision->dev_model].radio) {
		/* usbvision has radio */
		usbvision->rdev = usbvision_vdev_init(usbvision,
						      &usbvision_radio_template,
						      "USBVision Radio");
		if (usbvision->rdev == NULL)
			goto err_exit;
		if (video_register_device(usbvision->rdev, VFL_TYPE_RADIO, radio_nr) < 0)
			goto err_exit;
		printk(KERN_INFO "USBVision[%d]: registered USBVision Radio device %s [v4l2]\n",
		       usbvision->nr, video_device_node_name(usbvision->rdev));
	}
	/* all done */
	return 0;

 err_exit:
	dev_err(&usbvision->dev->dev,
		"USBVision[%d]: video_register_device() failed\n",
			usbvision->nr);
	usbvision_unregister_video(usbvision);
	return -1;
}

/*
 * usbvision_alloc()
 *
 * This code allocates the struct usb_usbvision.
 * It is filled with default values.
 *
 * Returns NULL on error, a pointer to usb_usbvision else.
 *
 */
static struct usb_usbvision *usbvision_alloc(struct usb_device *dev,
					     struct usb_interface *intf)
{
	struct usb_usbvision *usbvision;

	usbvision = kzalloc(sizeof(struct usb_usbvision), GFP_KERNEL);
	if (usbvision == NULL)
		return NULL;

	usbvision->dev = dev;
	if (v4l2_device_register(&intf->dev, &usbvision->v4l2_dev))
		goto err_free;

	mutex_init(&usbvision->v4l2_lock);

	/* prepare control urb for control messages during interrupts */
	usbvision->ctrl_urb = usb_alloc_urb(USBVISION_URB_FRAMES, GFP_KERNEL);
	if (usbvision->ctrl_urb == NULL)
		goto err_unreg;
	init_waitqueue_head(&usbvision->ctrl_urb_wq);

	usbvision_init_power_off_timer(usbvision);

	return usbvision;

err_unreg:
	v4l2_device_unregister(&usbvision->v4l2_dev);
err_free:
	kfree(usbvision);
	return NULL;
}

/*
 * usbvision_release()
 *
 * This code does final release of struct usb_usbvision. This happens
 * after the device is disconnected -and- all clients closed their files.
 *
 */
static void usbvision_release(struct usb_usbvision *usbvision)
{
	PDEBUG(DBG_PROBE, "");

	usbvision_reset_power_off_timer(usbvision);

	usbvision->initialized = 0;

	usbvision_remove_sysfs(usbvision->vdev);
	usbvision_unregister_video(usbvision);

	usb_free_urb(usbvision->ctrl_urb);

	v4l2_device_unregister(&usbvision->v4l2_dev);
	kfree(usbvision);

	PDEBUG(DBG_PROBE, "success");
}


/*********************** usb interface **********************************/

static void usbvision_configure_video(struct usb_usbvision *usbvision)
{
	int model;

	if (usbvision == NULL)
		return;

	model = usbvision->dev_model;
	usbvision->palette = usbvision_v4l2_format[2]; /* V4L2_PIX_FMT_RGB24; */

	if (usbvision_device_data[usbvision->dev_model].vin_reg2_override) {
		usbvision->vin_reg2_preset =
			usbvision_device_data[usbvision->dev_model].vin_reg2;
	} else {
		usbvision->vin_reg2_preset = 0;
	}

	usbvision->tvnorm_id = usbvision_device_data[model].video_norm;

	usbvision->video_inputs = usbvision_device_data[model].video_channels;
	usbvision->ctl_input = 0;

	/* This should be here to make i2c clients to be able to register */
	/* first switch off audio */
	usbvision_audio_off(usbvision);
	if (!power_on_at_open) {
		/* and then power up the noisy tuner */
		usbvision_power_on(usbvision);
		usbvision_i2c_register(usbvision);
	}
}

/*
 * usbvision_probe()
 *
 * This procedure queries device descriptor and accepts the interface
 * if it looks like USBVISION video device
 *
 */
static int __devinit usbvision_probe(struct usb_interface *intf,
				     const struct usb_device_id *devid)
{
	struct usb_device *dev = usb_get_dev(interface_to_usbdev(intf));
	struct usb_interface *uif;
	__u8 ifnum = intf->altsetting->desc.bInterfaceNumber;
	const struct usb_host_interface *interface;
	struct usb_usbvision *usbvision = NULL;
	const struct usb_endpoint_descriptor *endpoint;
	int model, i;

	PDEBUG(DBG_PROBE, "VID=%#04x, PID=%#04x, ifnum=%u",
				dev->descriptor.idVendor,
				dev->descriptor.idProduct, ifnum);

	model = devid->driver_info;
	if (model < 0 || model >= usbvision_device_data_size) {
		PDEBUG(DBG_PROBE, "model out of bounds %d", model);
		return -ENODEV;
	}
	printk(KERN_INFO "%s: %s found\n", __func__,
				usbvision_device_data[model].model_string);

	if (usbvision_device_data[model].interface >= 0)
		interface = &dev->actconfig->interface[usbvision_device_data[model].interface]->altsetting[0];
	else
		interface = &dev->actconfig->interface[ifnum]->altsetting[0];
	endpoint = &interface->endpoint[1].desc;
	if (!usb_endpoint_xfer_isoc(endpoint)) {
		dev_err(&intf->dev, "%s: interface %d. has non-ISO endpoint!\n",
		    __func__, ifnum);
		dev_err(&intf->dev, "%s: Endpoint attributes %d",
		    __func__, endpoint->bmAttributes);
		return -ENODEV;
	}
	if (usb_endpoint_dir_out(endpoint)) {
		dev_err(&intf->dev, "%s: interface %d. has ISO OUT endpoint!\n",
		    __func__, ifnum);
		return -ENODEV;
	}

	usbvision = usbvision_alloc(dev, intf);
	if (usbvision == NULL) {
		dev_err(&intf->dev, "%s: couldn't allocate USBVision struct\n", __func__);
		return -ENOMEM;
	}

	if (dev->descriptor.bNumConfigurations > 1)
		usbvision->bridge_type = BRIDGE_NT1004;
	else if (model == DAZZLE_DVC_90_REV_1_SECAM)
		usbvision->bridge_type = BRIDGE_NT1005;
	else
		usbvision->bridge_type = BRIDGE_NT1003;
	PDEBUG(DBG_PROBE, "bridge_type %d", usbvision->bridge_type);

	/* compute alternate max packet sizes */
	uif = dev->actconfig->interface[0];

	usbvision->num_alt = uif->num_altsetting;
	PDEBUG(DBG_PROBE, "Alternate settings: %i", usbvision->num_alt);
	usbvision->alt_max_pkt_size = kmalloc(32 * usbvision->num_alt, GFP_KERNEL);
	if (usbvision->alt_max_pkt_size == NULL) {
		dev_err(&intf->dev, "usbvision: out of memory!\n");
		return -ENOMEM;
	}

	for (i = 0; i < usbvision->num_alt; i++) {
		u16 tmp = le16_to_cpu(uif->altsetting[i].endpoint[1].desc.
				      wMaxPacketSize);
		usbvision->alt_max_pkt_size[i] =
			(tmp & 0x07ff) * (((tmp & 0x1800) >> 11) + 1);
		PDEBUG(DBG_PROBE, "Alternate setting %i, max size= %i", i,
		       usbvision->alt_max_pkt_size[i]);
	}


	usbvision->nr = usbvision_nr++;

	usbvision->have_tuner = usbvision_device_data[model].tuner;
	if (usbvision->have_tuner)
		usbvision->tuner_type = usbvision_device_data[model].tuner_type;

	usbvision->dev_model = model;
	usbvision->remove_pending = 0;
	usbvision->iface = ifnum;
	usbvision->iface_alt = 0;
	usbvision->video_endp = endpoint->bEndpointAddress;
	usbvision->isoc_packet_size = 0;
	usbvision->usb_bandwidth = 0;
	usbvision->user = 0;
	usbvision->streaming = stream_off;
	usbvision_configure_video(usbvision);
	usbvision_register_video(usbvision);

	usbvision_create_sysfs(usbvision->vdev);

	PDEBUG(DBG_PROBE, "success");
	return 0;
}


/*
 * usbvision_disconnect()
 *
 * This procedure stops all driver activity, deallocates interface-private
 * structure (pointed by 'ptr') and after that driver should be removable
 * with no ill consequences.
 *
 */
static void __devexit usbvision_disconnect(struct usb_interface *intf)
{
	struct usb_usbvision *usbvision = to_usbvision(usb_get_intfdata(intf));

	PDEBUG(DBG_PROBE, "");

	if (usbvision == NULL) {
		pr_err("%s: usb_get_intfdata() failed\n", __func__);
		return;
	}

	mutex_lock(&usbvision->v4l2_lock);

	/* At this time we ask to cancel outstanding URBs */
	usbvision_stop_isoc(usbvision);

	v4l2_device_disconnect(&usbvision->v4l2_dev);

	if (usbvision->power) {
		usbvision_i2c_unregister(usbvision);
		usbvision_power_off(usbvision);
	}
	usbvision->remove_pending = 1;	/* Now all ISO data will be ignored */

	usb_put_dev(usbvision->dev);
	usbvision->dev = NULL;	/* USB device is no more */

	mutex_unlock(&usbvision->v4l2_lock);

	if (usbvision->user) {
		printk(KERN_INFO "%s: In use, disconnect pending\n",
		       __func__);
		wake_up_interruptible(&usbvision->wait_frame);
		wake_up_interruptible(&usbvision->wait_stream);
	} else {
		usbvision_release(usbvision);
	}

	PDEBUG(DBG_PROBE, "success");
}

static struct usb_driver usbvision_driver = {
	.name		= "usbvision",
	.id_table	= usbvision_table,
	.probe		= usbvision_probe,
	.disconnect	= __devexit_p(usbvision_disconnect),
};

/*
 * usbvision_init()
 *
 * This code is run to initialize the driver.
 *
 */
static int __init usbvision_init(void)
{
	int err_code;

	PDEBUG(DBG_PROBE, "");

	PDEBUG(DBG_IO,  "IO      debugging is enabled [video]");
	PDEBUG(DBG_PROBE, "PROBE   debugging is enabled [video]");
	PDEBUG(DBG_MMAP, "MMAP    debugging is enabled [video]");

	/* disable planar mode support unless compression enabled */
	if (isoc_mode != ISOC_MODE_COMPRESS) {
		/* FIXME : not the right way to set supported flag */
		usbvision_v4l2_format[6].supported = 0; /* V4L2_PIX_FMT_YVU420 */
		usbvision_v4l2_format[7].supported = 0; /* V4L2_PIX_FMT_YUV422P */
	}

	err_code = usb_register(&usbvision_driver);

	if (err_code == 0) {
		printk(KERN_INFO DRIVER_DESC " : " USBVISION_VERSION_STRING "\n");
		PDEBUG(DBG_PROBE, "success");
	}
	return err_code;
}

static void __exit usbvision_exit(void)
{
	PDEBUG(DBG_PROBE, "");

	usb_deregister(&usbvision_driver);
	PDEBUG(DBG_PROBE, "success");
}

module_init(usbvision_init);
module_exit(usbvision_exit);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
