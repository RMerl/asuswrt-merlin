/*
 *      uvc_v4l2.c  --  USB Video Class driver - V4L2 API
 *
 *      Copyright (C) 2005-2009
 *          Laurent Pinchart (laurent.pinchart@skynet.be)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <asm/atomic.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#include "uvcvideo.h"

/* ------------------------------------------------------------------------
 * UVC ioctls
 */
static int uvc_ioctl_ctrl_map(struct uvc_xu_control_mapping *xmap, int old)
{
	struct uvc_control_mapping *map;
	unsigned int size;
	int ret;

	map = kzalloc(sizeof *map, GFP_KERNEL);
	if (map == NULL)
		return -ENOMEM;

	map->id = xmap->id;
	memcpy(map->name, xmap->name, sizeof map->name);
	memcpy(map->entity, xmap->entity, sizeof map->entity);
	map->selector = xmap->selector;
	map->size = xmap->size;
	map->offset = xmap->offset;
	map->v4l2_type = xmap->v4l2_type;
	map->data_type = xmap->data_type;

	switch (xmap->v4l2_type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_BUTTON:
		break;

	case V4L2_CTRL_TYPE_MENU:
		if (old) {
			ret = -EINVAL;
			goto done;
		}

		size = xmap->menu_count * sizeof(*map->menu_info);
		map->menu_info = kmalloc(size, GFP_KERNEL);
		if (map->menu_info == NULL) {
			ret = -ENOMEM;
			goto done;
		}

		if (copy_from_user(map->menu_info, xmap->menu_info, size)) {
			ret = -EFAULT;
			goto done;
		}

		map->menu_count = xmap->menu_count;
		break;

	default:
		ret = -EINVAL;
		goto done;
	}

	ret = uvc_ctrl_add_mapping(map);

done:
	if (ret < 0) {
		kfree(map->menu_info);
		kfree(map);
	}

	return ret;
}

/* ------------------------------------------------------------------------
 * V4L2 interface
 */

/*
 * Mapping V4L2 controls to UVC controls can be straighforward if done well.
 * Most of the UVC controls exist in V4L2, and can be mapped directly. Some
 * must be grouped (for instance the Red Balance, Blue Balance and Do White
 * Balance V4L2 controls use the White Balance Component UVC control) or
 * otherwise translated. The approach we take here is to use a translation
 * table for the controls that can be mapped directly, and handle the others
 * manually.
 */
static int uvc_v4l2_query_menu(struct uvc_video_chain *chain,
	struct v4l2_querymenu *query_menu)
{
	struct uvc_menu_info *menu_info;
	struct uvc_control_mapping *mapping;
	struct uvc_control *ctrl;
	u32 index = query_menu->index;
	u32 id = query_menu->id;

	ctrl = uvc_find_control(chain, query_menu->id, &mapping);
	if (ctrl == NULL || mapping->v4l2_type != V4L2_CTRL_TYPE_MENU)
		return -EINVAL;

	if (query_menu->index >= mapping->menu_count)
		return -EINVAL;

	memset(query_menu, 0, sizeof(*query_menu));
	query_menu->id = id;
	query_menu->index = index;

	menu_info = &mapping->menu_info[query_menu->index];
	strlcpy(query_menu->name, menu_info->name, sizeof query_menu->name);
	return 0;
}

/*
 * Find the frame interval closest to the requested frame interval for the
 * given frame format and size. This should be done by the device as part of
 * the Video Probe and Commit negotiation, but some hardware don't implement
 * that feature.
 */
static __u32 uvc_try_frame_interval(struct uvc_frame *frame, __u32 interval)
{
	unsigned int i;

	if (frame->bFrameIntervalType) {
		__u32 best = -1, dist;

		for (i = 0; i < frame->bFrameIntervalType; ++i) {
			dist = interval > frame->dwFrameInterval[i]
			     ? interval - frame->dwFrameInterval[i]
			     : frame->dwFrameInterval[i] - interval;

			if (dist > best)
				break;

			best = dist;
		}

		interval = frame->dwFrameInterval[i-1];
	} else {
		const __u32 min = frame->dwFrameInterval[0];
		const __u32 max = frame->dwFrameInterval[1];
		const __u32 step = frame->dwFrameInterval[2];

		interval = min + (interval - min + step/2) / step * step;
		if (interval > max)
			interval = max;
	}

	return interval;
}

static int uvc_v4l2_try_format(struct uvc_streaming *stream,
	struct v4l2_format *fmt, struct uvc_streaming_control *probe,
	struct uvc_format **uvc_format, struct uvc_frame **uvc_frame)
{
	struct uvc_format *format = NULL;
	struct uvc_frame *frame = NULL;
	__u16 rw, rh;
	unsigned int d, maxd;
	unsigned int i;
	__u32 interval;
	int ret = 0;
	__u8 *fcc;

	if (fmt->type != stream->type)
		return -EINVAL;

	fcc = (__u8 *)&fmt->fmt.pix.pixelformat;
	uvc_trace(UVC_TRACE_FORMAT, "Trying format 0x%08x (%c%c%c%c): %ux%u.\n",
			fmt->fmt.pix.pixelformat,
			fcc[0], fcc[1], fcc[2], fcc[3],
			fmt->fmt.pix.width, fmt->fmt.pix.height);

	/* Check if the hardware supports the requested format. */
	for (i = 0; i < stream->nformats; ++i) {
		format = &stream->format[i];
		if (format->fcc == fmt->fmt.pix.pixelformat)
			break;
	}

	if (format == NULL || format->fcc != fmt->fmt.pix.pixelformat) {
		uvc_trace(UVC_TRACE_FORMAT, "Unsupported format 0x%08x.\n",
				fmt->fmt.pix.pixelformat);
		return -EINVAL;
	}

	/* Find the closest image size. The distance between image sizes is
	 * the size in pixels of the non-overlapping regions between the
	 * requested size and the frame-specified size.
	 */
	rw = fmt->fmt.pix.width;
	rh = fmt->fmt.pix.height;
	maxd = (unsigned int)-1;

	for (i = 0; i < format->nframes; ++i) {
		__u16 w = format->frame[i].wWidth;
		__u16 h = format->frame[i].wHeight;

		d = min(w, rw) * min(h, rh);
		d = w*h + rw*rh - 2*d;
		if (d < maxd) {
			maxd = d;
			frame = &format->frame[i];
		}

		if (maxd == 0)
			break;
	}

	if (frame == NULL) {
		uvc_trace(UVC_TRACE_FORMAT, "Unsupported size %ux%u.\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);
		return -EINVAL;
	}

	/* Use the default frame interval. */
	interval = frame->dwDefaultFrameInterval;
	uvc_trace(UVC_TRACE_FORMAT, "Using default frame interval %u.%u us "
		"(%u.%u fps).\n", interval/10, interval%10, 10000000/interval,
		(100000000/interval)%10);

	/* Set the format index, frame index and frame interval. */
	memset(probe, 0, sizeof *probe);
	probe->bmHint = 1;	/* dwFrameInterval */
	probe->bFormatIndex = format->index;
	probe->bFrameIndex = frame->bFrameIndex;
	probe->dwFrameInterval = uvc_try_frame_interval(frame, interval);
	if (stream->dev->quirks & UVC_QUIRK_PROBE_EXTRAFIELDS)
		probe->dwMaxVideoFrameSize =
			stream->ctrl.dwMaxVideoFrameSize;

	/* Probe the device. */
	ret = uvc_probe_video(stream, probe);
	if (ret < 0)
		goto done;

	fmt->fmt.pix.width = frame->wWidth;
	fmt->fmt.pix.height = frame->wHeight;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline = format->bpp * frame->wWidth / 8;
	fmt->fmt.pix.sizeimage = probe->dwMaxVideoFrameSize;
	fmt->fmt.pix.colorspace = format->colorspace;
	fmt->fmt.pix.priv = 0;

	if (uvc_format != NULL)
		*uvc_format = format;
	if (uvc_frame != NULL)
		*uvc_frame = frame;

done:
	return ret;
}

static int uvc_v4l2_get_format(struct uvc_streaming *stream,
	struct v4l2_format *fmt)
{
	struct uvc_format *format = stream->cur_format;
	struct uvc_frame *frame = stream->cur_frame;

	if (fmt->type != stream->type)
		return -EINVAL;

	if (format == NULL || frame == NULL)
		return -EINVAL;

	fmt->fmt.pix.pixelformat = format->fcc;
	fmt->fmt.pix.width = frame->wWidth;
	fmt->fmt.pix.height = frame->wHeight;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline = format->bpp * frame->wWidth / 8;
	fmt->fmt.pix.sizeimage = stream->ctrl.dwMaxVideoFrameSize;
	fmt->fmt.pix.colorspace = format->colorspace;
	fmt->fmt.pix.priv = 0;

	return 0;
}

static int uvc_v4l2_set_format(struct uvc_streaming *stream,
	struct v4l2_format *fmt)
{
	struct uvc_streaming_control probe;
	struct uvc_format *format;
	struct uvc_frame *frame;
	int ret;

	if (fmt->type != stream->type)
		return -EINVAL;

	if (uvc_queue_allocated(&stream->queue))
		return -EBUSY;

	ret = uvc_v4l2_try_format(stream, fmt, &probe, &format, &frame);
	if (ret < 0)
		return ret;

	memcpy(&stream->ctrl, &probe, sizeof probe);
	stream->cur_format = format;
	stream->cur_frame = frame;

	return 0;
}

static int uvc_v4l2_get_streamparm(struct uvc_streaming *stream,
		struct v4l2_streamparm *parm)
{
	uint32_t numerator, denominator;

	if (parm->type != stream->type)
		return -EINVAL;

	numerator = stream->ctrl.dwFrameInterval;
	denominator = 10000000;
	uvc_simplify_fraction(&numerator, &denominator, 8, 333);

	memset(parm, 0, sizeof *parm);
	parm->type = stream->type;

	if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		parm->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		parm->parm.capture.capturemode = 0;
		parm->parm.capture.timeperframe.numerator = numerator;
		parm->parm.capture.timeperframe.denominator = denominator;
		parm->parm.capture.extendedmode = 0;
		parm->parm.capture.readbuffers = 0;
	} else {
		parm->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
		parm->parm.output.outputmode = 0;
		parm->parm.output.timeperframe.numerator = numerator;
		parm->parm.output.timeperframe.denominator = denominator;
	}

	return 0;
}

static int uvc_v4l2_set_streamparm(struct uvc_streaming *stream,
		struct v4l2_streamparm *parm)
{
	struct uvc_frame *frame = stream->cur_frame;
	struct uvc_streaming_control probe;
	struct v4l2_fract timeperframe;
	uint32_t interval;
	int ret;

	if (parm->type != stream->type)
		return -EINVAL;

	if (uvc_queue_streaming(&stream->queue))
		return -EBUSY;

	if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		timeperframe = parm->parm.capture.timeperframe;
	else
		timeperframe = parm->parm.output.timeperframe;

	memcpy(&probe, &stream->ctrl, sizeof probe);
	interval = uvc_fraction_to_interval(timeperframe.numerator,
		timeperframe.denominator);

	uvc_trace(UVC_TRACE_FORMAT, "Setting frame interval to %u/%u (%u).\n",
		timeperframe.numerator, timeperframe.denominator, interval);
	probe.dwFrameInterval = uvc_try_frame_interval(frame, interval);

	/* Probe the device with the new settings. */
	ret = uvc_probe_video(stream, &probe);
	if (ret < 0)
		return ret;

	memcpy(&stream->ctrl, &probe, sizeof probe);

	/* Return the actual frame period. */
	timeperframe.numerator = probe.dwFrameInterval;
	timeperframe.denominator = 10000000;
	uvc_simplify_fraction(&timeperframe.numerator,
		&timeperframe.denominator, 8, 333);

	if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		parm->parm.capture.timeperframe = timeperframe;
	else
		parm->parm.output.timeperframe = timeperframe;

	return 0;
}

/* ------------------------------------------------------------------------
 * Privilege management
 */

/*
 * Privilege management is the multiple-open implementation basis. The current
 * implementation is completely transparent for the end-user and doesn't
 * require explicit use of the VIDIOC_G_PRIORITY and VIDIOC_S_PRIORITY ioctls.
 * Those ioctls enable finer control on the device (by making possible for a
 * user to request exclusive access to a device), but are not mature yet.
 * Switching to the V4L2 priority mechanism might be considered in the future
 * if this situation changes.
 *
 * Each open instance of a UVC device can either be in a privileged or
 * unprivileged state. Only a single instance can be in a privileged state at
 * a given time. Trying to perform an operation that requires privileges will
 * automatically acquire the required privileges if possible, or return -EBUSY
 * otherwise. Privileges are dismissed when closing the instance or when
 * freeing the video buffers using VIDIOC_REQBUFS.
 *
 * Operations that require privileges are:
 *
 * - VIDIOC_S_INPUT
 * - VIDIOC_S_PARM
 * - VIDIOC_S_FMT
 * - VIDIOC_REQBUFS
 */
static int uvc_acquire_privileges(struct uvc_fh *handle)
{
	/* Always succeed if the handle is already privileged. */
	if (handle->state == UVC_HANDLE_ACTIVE)
		return 0;

	/* Check if the device already has a privileged handle. */
	if (atomic_inc_return(&handle->stream->active) != 1) {
		atomic_dec(&handle->stream->active);
		return -EBUSY;
	}

	handle->state = UVC_HANDLE_ACTIVE;
	return 0;
}

static void uvc_dismiss_privileges(struct uvc_fh *handle)
{
	if (handle->state == UVC_HANDLE_ACTIVE)
		atomic_dec(&handle->stream->active);

	handle->state = UVC_HANDLE_PASSIVE;
}

static int uvc_has_privileges(struct uvc_fh *handle)
{
	return handle->state == UVC_HANDLE_ACTIVE;
}

/* ------------------------------------------------------------------------
 * V4L2 file operations
 */

static int uvc_v4l2_open(struct file *file)
{
	struct uvc_streaming *stream;
	struct uvc_fh *handle;
	int ret = 0;

	uvc_trace(UVC_TRACE_CALLS, "uvc_v4l2_open\n");
	stream = video_drvdata(file);

	if (stream->dev->state & UVC_DEV_DISCONNECTED)
		return -ENODEV;

	ret = usb_autopm_get_interface(stream->dev->intf);
	if (ret < 0)
		return ret;

	/* Create the device handle. */
	handle = kzalloc(sizeof *handle, GFP_KERNEL);
	if (handle == NULL) {
		usb_autopm_put_interface(stream->dev->intf);
		return -ENOMEM;
	}

	if (atomic_inc_return(&stream->dev->users) == 1) {
		ret = uvc_status_start(stream->dev);
		if (ret < 0) {
			usb_autopm_put_interface(stream->dev->intf);
			atomic_dec(&stream->dev->users);
			kfree(handle);
			return ret;
		}
	}

	handle->chain = stream->chain;
	handle->stream = stream;
	handle->state = UVC_HANDLE_PASSIVE;
	file->private_data = handle;

	return 0;
}

static int uvc_v4l2_release(struct file *file)
{
	struct uvc_fh *handle = file->private_data;
	struct uvc_streaming *stream = handle->stream;

	uvc_trace(UVC_TRACE_CALLS, "uvc_v4l2_release\n");

	/* Only free resources if this is a privileged handle. */
	if (uvc_has_privileges(handle)) {
		uvc_video_enable(stream, 0);

		mutex_lock(&stream->queue.mutex);
		if (uvc_free_buffers(&stream->queue) < 0)
			uvc_printk(KERN_ERR, "uvc_v4l2_release: Unable to "
					"free buffers.\n");
		mutex_unlock(&stream->queue.mutex);
	}

	/* Release the file handle. */
	uvc_dismiss_privileges(handle);
	kfree(handle);
	file->private_data = NULL;

	if (atomic_dec_return(&stream->dev->users) == 0)
		uvc_status_stop(stream->dev);

	usb_autopm_put_interface(stream->dev->intf);
	return 0;
}

static long uvc_v4l2_do_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct uvc_fh *handle = file->private_data;
	struct uvc_video_chain *chain = handle->chain;
	struct uvc_streaming *stream = handle->stream;
	long ret = 0;

	switch (cmd) {
	/* Query capabilities */
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability *cap = arg;

		memset(cap, 0, sizeof *cap);
		strlcpy(cap->driver, "uvcvideo", sizeof cap->driver);
		strlcpy(cap->card, vdev->name, sizeof cap->card);
		usb_make_path(stream->dev->udev,
			      cap->bus_info, sizeof(cap->bus_info));
		cap->version = DRIVER_VERSION_NUMBER;
		if (stream->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
			cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
					  | V4L2_CAP_STREAMING;
		else
			cap->capabilities = V4L2_CAP_VIDEO_OUTPUT
					  | V4L2_CAP_STREAMING;
		break;
	}

	/* Get, Set & Query control */
	case VIDIOC_QUERYCTRL:
		return uvc_query_v4l2_ctrl(chain, arg);

	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		struct v4l2_ext_control xctrl;

		memset(&xctrl, 0, sizeof xctrl);
		xctrl.id = ctrl->id;

		ret = uvc_ctrl_begin(chain);
		if (ret < 0)
			return ret;

		ret = uvc_ctrl_get(chain, &xctrl);
		uvc_ctrl_rollback(chain);
		if (ret >= 0)
			ctrl->value = xctrl.value;
		break;
	}

	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		struct v4l2_ext_control xctrl;

		memset(&xctrl, 0, sizeof xctrl);
		xctrl.id = ctrl->id;
		xctrl.value = ctrl->value;

		ret = uvc_ctrl_begin(chain);
		if (ret < 0)
			return ret;

		ret = uvc_ctrl_set(chain, &xctrl);
		if (ret < 0) {
			uvc_ctrl_rollback(chain);
			return ret;
		}
		ret = uvc_ctrl_commit(chain);
		if (ret == 0)
			ctrl->value = xctrl.value;
		break;
	}

	case VIDIOC_QUERYMENU:
		return uvc_v4l2_query_menu(chain, arg);

	case VIDIOC_G_EXT_CTRLS:
	{
		struct v4l2_ext_controls *ctrls = arg;
		struct v4l2_ext_control *ctrl = ctrls->controls;
		unsigned int i;

		ret = uvc_ctrl_begin(chain);
		if (ret < 0)
			return ret;

		for (i = 0; i < ctrls->count; ++ctrl, ++i) {
			ret = uvc_ctrl_get(chain, ctrl);
			if (ret < 0) {
				uvc_ctrl_rollback(chain);
				ctrls->error_idx = i;
				return ret;
			}
		}
		ctrls->error_idx = 0;
		ret = uvc_ctrl_rollback(chain);
		break;
	}

	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
	{
		struct v4l2_ext_controls *ctrls = arg;
		struct v4l2_ext_control *ctrl = ctrls->controls;
		unsigned int i;

		ret = uvc_ctrl_begin(chain);
		if (ret < 0)
			return ret;

		for (i = 0; i < ctrls->count; ++ctrl, ++i) {
			ret = uvc_ctrl_set(chain, ctrl);
			if (ret < 0) {
				uvc_ctrl_rollback(chain);
				ctrls->error_idx = i;
				return ret;
			}
		}

		ctrls->error_idx = 0;

		if (cmd == VIDIOC_S_EXT_CTRLS)
			ret = uvc_ctrl_commit(chain);
		else
			ret = uvc_ctrl_rollback(chain);
		break;
	}

	/* Get, Set & Enum input */
	case VIDIOC_ENUMINPUT:
	{
		const struct uvc_entity *selector = chain->selector;
		struct v4l2_input *input = arg;
		struct uvc_entity *iterm = NULL;
		u32 index = input->index;
		int pin = 0;

		if (selector == NULL ||
		    (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)) {
			if (index != 0)
				return -EINVAL;
			list_for_each_entry(iterm, &chain->entities, chain) {
				if (UVC_ENTITY_IS_ITERM(iterm))
					break;
			}
			pin = iterm->id;
		} else if (pin < selector->bNrInPins) {
			pin = selector->baSourceID[index];
			list_for_each_entry(iterm, &chain->entities, chain) {
				if (!UVC_ENTITY_IS_ITERM(iterm))
					continue;
				if (iterm->id == pin)
					break;
			}
		}

		if (iterm == NULL || iterm->id != pin)
			return -EINVAL;

		memset(input, 0, sizeof *input);
		input->index = index;
		strlcpy(input->name, iterm->name, sizeof input->name);
		if (UVC_ENTITY_TYPE(iterm) == UVC_ITT_CAMERA)
			input->type = V4L2_INPUT_TYPE_CAMERA;
		break;
	}

	case VIDIOC_G_INPUT:
	{
		u8 input;

		if (chain->selector == NULL ||
		    (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)) {
			*(int *)arg = 0;
			break;
		}

		ret = uvc_query_ctrl(chain->dev, UVC_GET_CUR,
			chain->selector->id, chain->dev->intfnum,
			UVC_SU_INPUT_SELECT_CONTROL, &input, 1);
		if (ret < 0)
			return ret;

		*(int *)arg = input - 1;
		break;
	}

	case VIDIOC_S_INPUT:
	{
		u32 input = *(u32 *)arg + 1;

		if ((ret = uvc_acquire_privileges(handle)) < 0)
			return ret;

		if (chain->selector == NULL ||
		    (chain->dev->quirks & UVC_QUIRK_IGNORE_SELECTOR_UNIT)) {
			if (input != 1)
				return -EINVAL;
			break;
		}

		if (input == 0 || input > chain->selector->bNrInPins)
			return -EINVAL;

		return uvc_query_ctrl(chain->dev, UVC_SET_CUR,
			chain->selector->id, chain->dev->intfnum,
			UVC_SU_INPUT_SELECT_CONTROL, &input, 1);
	}

	/* Try, Get, Set & Enum format */
	case VIDIOC_ENUM_FMT:
	{
		struct v4l2_fmtdesc *fmt = arg;
		struct uvc_format *format;
		enum v4l2_buf_type type = fmt->type;
		__u32 index = fmt->index;

		if (fmt->type != stream->type ||
		    fmt->index >= stream->nformats)
			return -EINVAL;

		memset(fmt, 0, sizeof(*fmt));
		fmt->index = index;
		fmt->type = type;

		format = &stream->format[fmt->index];
		fmt->flags = 0;
		if (format->flags & UVC_FMT_FLAG_COMPRESSED)
			fmt->flags |= V4L2_FMT_FLAG_COMPRESSED;
		strlcpy(fmt->description, format->name,
			sizeof fmt->description);
		fmt->description[sizeof fmt->description - 1] = 0;
		fmt->pixelformat = format->fcc;
		break;
	}

	case VIDIOC_TRY_FMT:
	{
		struct uvc_streaming_control probe;

		return uvc_v4l2_try_format(stream, arg, &probe, NULL, NULL);
	}

	case VIDIOC_S_FMT:
		if ((ret = uvc_acquire_privileges(handle)) < 0)
			return ret;

		return uvc_v4l2_set_format(stream, arg);

	case VIDIOC_G_FMT:
		return uvc_v4l2_get_format(stream, arg);

	/* Frame size enumeration */
	case VIDIOC_ENUM_FRAMESIZES:
	{
		struct v4l2_frmsizeenum *fsize = arg;
		struct uvc_format *format = NULL;
		struct uvc_frame *frame;
		int i;

		/* Look for the given pixel format */
		for (i = 0; i < stream->nformats; i++) {
			if (stream->format[i].fcc ==
					fsize->pixel_format) {
				format = &stream->format[i];
				break;
			}
		}
		if (format == NULL)
			return -EINVAL;

		if (fsize->index >= format->nframes)
			return -EINVAL;

		frame = &format->frame[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frame->wWidth;
		fsize->discrete.height = frame->wHeight;
		break;
	}

	/* Frame interval enumeration */
	case VIDIOC_ENUM_FRAMEINTERVALS:
	{
		struct v4l2_frmivalenum *fival = arg;
		struct uvc_format *format = NULL;
		struct uvc_frame *frame = NULL;
		int i;

		/* Look for the given pixel format and frame size */
		for (i = 0; i < stream->nformats; i++) {
			if (stream->format[i].fcc ==
					fival->pixel_format) {
				format = &stream->format[i];
				break;
			}
		}
		if (format == NULL)
			return -EINVAL;

		for (i = 0; i < format->nframes; i++) {
			if (format->frame[i].wWidth == fival->width &&
			    format->frame[i].wHeight == fival->height) {
				frame = &format->frame[i];
				break;
			}
		}
		if (frame == NULL)
			return -EINVAL;

		if (frame->bFrameIntervalType) {
			if (fival->index >= frame->bFrameIntervalType)
				return -EINVAL;

			fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
			fival->discrete.numerator =
				frame->dwFrameInterval[fival->index];
			fival->discrete.denominator = 10000000;
			uvc_simplify_fraction(&fival->discrete.numerator,
				&fival->discrete.denominator, 8, 333);
		} else {
			fival->type = V4L2_FRMIVAL_TYPE_STEPWISE;
			fival->stepwise.min.numerator =
				frame->dwFrameInterval[0];
			fival->stepwise.min.denominator = 10000000;
			fival->stepwise.max.numerator =
				frame->dwFrameInterval[1];
			fival->stepwise.max.denominator = 10000000;
			fival->stepwise.step.numerator =
				frame->dwFrameInterval[2];
			fival->stepwise.step.denominator = 10000000;
			uvc_simplify_fraction(&fival->stepwise.min.numerator,
				&fival->stepwise.min.denominator, 8, 333);
			uvc_simplify_fraction(&fival->stepwise.max.numerator,
				&fival->stepwise.max.denominator, 8, 333);
			uvc_simplify_fraction(&fival->stepwise.step.numerator,
				&fival->stepwise.step.denominator, 8, 333);
		}
		break;
	}

	/* Get & Set streaming parameters */
	case VIDIOC_G_PARM:
		return uvc_v4l2_get_streamparm(stream, arg);

	case VIDIOC_S_PARM:
		if ((ret = uvc_acquire_privileges(handle)) < 0)
			return ret;

		return uvc_v4l2_set_streamparm(stream, arg);

	/* Cropping and scaling */
	case VIDIOC_CROPCAP:
	{
		struct v4l2_cropcap *ccap = arg;
		struct uvc_frame *frame = stream->cur_frame;

		if (ccap->type != stream->type)
			return -EINVAL;

		ccap->bounds.left = 0;
		ccap->bounds.top = 0;
		ccap->bounds.width = frame->wWidth;
		ccap->bounds.height = frame->wHeight;

		ccap->defrect = ccap->bounds;

		ccap->pixelaspect.numerator = 1;
		ccap->pixelaspect.denominator = 1;
		break;
	}

	case VIDIOC_G_CROP:
	case VIDIOC_S_CROP:
		return -EINVAL;

	/* Buffers & streaming */
	case VIDIOC_REQBUFS:
	{
		struct v4l2_requestbuffers *rb = arg;
		unsigned int bufsize =
			stream->ctrl.dwMaxVideoFrameSize;

		if (rb->type != stream->type ||
		    rb->memory != V4L2_MEMORY_MMAP)
			return -EINVAL;

		if ((ret = uvc_acquire_privileges(handle)) < 0)
			return ret;

		ret = uvc_alloc_buffers(&stream->queue, rb->count, bufsize);
		if (ret < 0)
			return ret;

		if (ret == 0)
			uvc_dismiss_privileges(handle);

		rb->count = ret;
		ret = 0;
		break;
	}

	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer *buf = arg;

		if (buf->type != stream->type)
			return -EINVAL;

		if (!uvc_has_privileges(handle))
			return -EBUSY;

		return uvc_query_buffer(&stream->queue, buf);
	}

	case VIDIOC_QBUF:
		if (!uvc_has_privileges(handle))
			return -EBUSY;

		return uvc_queue_buffer(&stream->queue, arg);

	case VIDIOC_DQBUF:
		if (!uvc_has_privileges(handle))
			return -EBUSY;

		return uvc_dequeue_buffer(&stream->queue, arg,
			file->f_flags & O_NONBLOCK);

	case VIDIOC_STREAMON:
	{
		int *type = arg;

		if (*type != stream->type)
			return -EINVAL;

		if (!uvc_has_privileges(handle))
			return -EBUSY;

		ret = uvc_video_enable(stream, 1);
		if (ret < 0)
			return ret;
		break;
	}

	case VIDIOC_STREAMOFF:
	{
		int *type = arg;

		if (*type != stream->type)
			return -EINVAL;

		if (!uvc_has_privileges(handle))
			return -EBUSY;

		return uvc_video_enable(stream, 0);
	}

	/* Analog video standards make no sense for digital cameras. */
	case VIDIOC_ENUMSTD:
	case VIDIOC_QUERYSTD:
	case VIDIOC_G_STD:
	case VIDIOC_S_STD:

	case VIDIOC_OVERLAY:

	case VIDIOC_ENUMAUDIO:
	case VIDIOC_ENUMAUDOUT:

	case VIDIOC_ENUMOUTPUT:
		uvc_trace(UVC_TRACE_IOCTL, "Unsupported ioctl 0x%08x\n", cmd);
		return -EINVAL;

	/* Dynamic controls. */
	case UVCIOC_CTRL_ADD:
	{
		struct uvc_xu_control_info *xinfo = arg;
		struct uvc_control_info *info;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (xinfo->size == 0)
			return -EINVAL;

		info = kzalloc(sizeof *info, GFP_KERNEL);
		if (info == NULL)
			return -ENOMEM;

		memcpy(info->entity, xinfo->entity, sizeof info->entity);
		info->index = xinfo->index;
		info->selector = xinfo->selector;
		info->size = xinfo->size;
		info->flags = xinfo->flags;

		info->flags |= UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX |
			       UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF |
			       UVC_CONTROL_EXTENSION;

		ret = uvc_ctrl_add_info(info);
		if (ret < 0)
			kfree(info);
		break;
	}

	case UVCIOC_CTRL_MAP_OLD:
	case UVCIOC_CTRL_MAP:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		return uvc_ioctl_ctrl_map(arg, cmd == UVCIOC_CTRL_MAP_OLD);

	case UVCIOC_CTRL_GET:
		return uvc_xu_ctrl_query(chain, arg, 0);

	case UVCIOC_CTRL_SET:
		return uvc_xu_ctrl_query(chain, arg, 1);

	default:
		if ((ret = v4l_compat_translate_ioctl(file, cmd, arg,
			uvc_v4l2_do_ioctl)) == -ENOIOCTLCMD)
			uvc_trace(UVC_TRACE_IOCTL, "Unknown ioctl 0x%08x\n",
				  cmd);
		return ret;
	}

	return ret;
}

static long uvc_v4l2_ioctl(struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	if (uvc_trace_param & UVC_TRACE_IOCTL) {
		uvc_printk(KERN_DEBUG, "uvc_v4l2_ioctl(");
		v4l_printk_ioctl(cmd);
		printk(")\n");
	}

	return video_usercopy(file, cmd, arg, uvc_v4l2_do_ioctl);
}

static ssize_t uvc_v4l2_read(struct file *file, char __user *data,
		    size_t count, loff_t *ppos)
{
	uvc_trace(UVC_TRACE_CALLS, "uvc_v4l2_read: not implemented.\n");
	return -EINVAL;
}

/*
 * VMA operations.
 */
static void uvc_vm_open(struct vm_area_struct *vma)
{
	struct uvc_buffer *buffer = vma->vm_private_data;
	buffer->vma_use_count++;
}

static void uvc_vm_close(struct vm_area_struct *vma)
{
	struct uvc_buffer *buffer = vma->vm_private_data;
	buffer->vma_use_count--;
}

static const struct vm_operations_struct uvc_vm_ops = {
	.open		= uvc_vm_open,
	.close		= uvc_vm_close,
};

static int uvc_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct uvc_fh *handle = file->private_data;
	struct uvc_streaming *stream = handle->stream;
	struct uvc_video_queue *queue = &stream->queue;
	struct uvc_buffer *uninitialized_var(buffer);
	struct page *page;
	unsigned long addr, start, size;
	unsigned int i;
	int ret = 0;

	uvc_trace(UVC_TRACE_CALLS, "uvc_v4l2_mmap\n");

	start = vma->vm_start;
	size = vma->vm_end - vma->vm_start;

	mutex_lock(&queue->mutex);

	for (i = 0; i < queue->count; ++i) {
		buffer = &queue->buffer[i];
		if ((buffer->buf.m.offset >> PAGE_SHIFT) == vma->vm_pgoff)
			break;
	}

	if (i == queue->count || size != queue->buf_size) {
		ret = -EINVAL;
		goto done;
	}

	/*
	 * VM_IO marks the area as being an mmaped region for I/O to a
	 * device. It also prevents the region from being core dumped.
	 */
	vma->vm_flags |= VM_IO;

	addr = (unsigned long)queue->mem + buffer->buf.m.offset;
	while (size > 0) {
		page = vmalloc_to_page((void *)addr);
		if ((ret = vm_insert_page(vma, start, page)) < 0)
			goto done;

		start += PAGE_SIZE;
		addr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	vma->vm_ops = &uvc_vm_ops;
	vma->vm_private_data = buffer;
	uvc_vm_open(vma);

done:
	mutex_unlock(&queue->mutex);
	return ret;
}

static unsigned int uvc_v4l2_poll(struct file *file, poll_table *wait)
{
	struct uvc_fh *handle = file->private_data;
	struct uvc_streaming *stream = handle->stream;

	uvc_trace(UVC_TRACE_CALLS, "uvc_v4l2_poll\n");

	return uvc_queue_poll(&stream->queue, file, wait);
}

const struct v4l2_file_operations uvc_fops = {
	.owner		= THIS_MODULE,
	.open		= uvc_v4l2_open,
	.release	= uvc_v4l2_release,
	.ioctl		= uvc_v4l2_ioctl,
	.read		= uvc_v4l2_read,
	.mmap		= uvc_v4l2_mmap,
	.poll		= uvc_v4l2_poll,
};
