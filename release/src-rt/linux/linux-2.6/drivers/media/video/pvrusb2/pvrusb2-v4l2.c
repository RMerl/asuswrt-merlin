/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *  Copyright (C) 2004 Aurelien Alleaume <slts@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "pvrusb2-context.h"
#include "pvrusb2-hdw.h"
#include "pvrusb2.h"
#include "pvrusb2-debug.h"
#include "pvrusb2-v4l2.h"
#include "pvrusb2-ioread.h"
#include <linux/videodev2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

struct pvr2_v4l2_dev;
struct pvr2_v4l2_fh;
struct pvr2_v4l2;

struct pvr2_v4l2_dev {
	struct video_device devbase; /* MUST be first! */
	struct pvr2_v4l2 *v4lp;
	struct pvr2_context_stream *stream;
	/* Information about this device: */
	enum pvr2_config config; /* Expected stream format */
	int v4l_type; /* V4L defined type for this device node */
	enum pvr2_v4l_type minor_type; /* pvr2-understood minor device type */
};

struct pvr2_v4l2_fh {
	struct pvr2_channel channel;
	struct pvr2_v4l2_dev *pdi;
	enum v4l2_priority prio;
	struct pvr2_ioread *rhp;
	struct file *file;
	struct pvr2_v4l2 *vhead;
	struct pvr2_v4l2_fh *vnext;
	struct pvr2_v4l2_fh *vprev;
	wait_queue_head_t wait_data;
	int fw_mode_flag;
	/* Map contiguous ordinal value to input id */
	unsigned char *input_map;
	unsigned int input_cnt;
};

struct pvr2_v4l2 {
	struct pvr2_channel channel;
	struct pvr2_v4l2_fh *vfirst;
	struct pvr2_v4l2_fh *vlast;

	struct v4l2_prio_state prio;

	/* streams - Note that these must be separately, individually,
	 * allocated pointers.  This is because the v4l core is going to
	 * manage their deletion - separately, individually...  */
	struct pvr2_v4l2_dev *dev_video;
	struct pvr2_v4l2_dev *dev_radio;
};

static int video_nr[PVR_NUM] = {[0 ... PVR_NUM-1] = -1};
module_param_array(video_nr, int, NULL, 0444);
MODULE_PARM_DESC(video_nr, "Offset for device's video dev minor");
static int radio_nr[PVR_NUM] = {[0 ... PVR_NUM-1] = -1};
module_param_array(radio_nr, int, NULL, 0444);
MODULE_PARM_DESC(radio_nr, "Offset for device's radio dev minor");
static int vbi_nr[PVR_NUM] = {[0 ... PVR_NUM-1] = -1};
module_param_array(vbi_nr, int, NULL, 0444);
MODULE_PARM_DESC(vbi_nr, "Offset for device's vbi dev minor");

static struct v4l2_capability pvr_capability ={
	.driver         = "pvrusb2",
	.card           = "Hauppauge WinTV pvr-usb2",
	.bus_info       = "usb",
	.version        = KERNEL_VERSION(0, 9, 0),
	.capabilities   = (V4L2_CAP_VIDEO_CAPTURE |
			   V4L2_CAP_TUNER | V4L2_CAP_AUDIO | V4L2_CAP_RADIO |
			   V4L2_CAP_READWRITE),
	.reserved       = {0,0,0,0}
};

static struct v4l2_fmtdesc pvr_fmtdesc [] = {
	{
		.index          = 0,
		.type           = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags          = V4L2_FMT_FLAG_COMPRESSED,
		.description    = "MPEG1/2",
		// This should really be V4L2_PIX_FMT_MPEG, but xawtv
		// breaks when I do that.
		.pixelformat    = 0, // V4L2_PIX_FMT_MPEG,
		.reserved       = { 0, 0, 0, 0 }
	}
};

#define PVR_FORMAT_PIX  0
#define PVR_FORMAT_VBI  1

static struct v4l2_format pvr_format [] = {
	[PVR_FORMAT_PIX] = {
		.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt    = {
			.pix        = {
				.width          = 720,
				.height             = 576,
				// This should really be V4L2_PIX_FMT_MPEG,
				// but xawtv breaks when I do that.
				.pixelformat    = 0, // V4L2_PIX_FMT_MPEG,
				.field          = V4L2_FIELD_INTERLACED,
				.bytesperline   = 0,  // doesn't make sense
						      // here
				//FIXME : Don't know what to put here...
				.sizeimage          = (32*1024),
				.colorspace     = 0, // doesn't make sense here
				.priv           = 0
			}
		}
	},
	[PVR_FORMAT_VBI] = {
		.type   = V4L2_BUF_TYPE_VBI_CAPTURE,
		.fmt    = {
			.vbi        = {
				.sampling_rate = 27000000,
				.offset = 248,
				.samples_per_line = 1443,
				.sample_format = V4L2_PIX_FMT_GREY,
				.start = { 0, 0 },
				.count = { 0, 0 },
				.flags = 0,
				.reserved = { 0, 0 }
			}
		}
	}
};


/*
 * pvr_ioctl()
 *
 * This is part of Video 4 Linux API. The procedure handles ioctl() calls.
 *
 */
static long pvr2_v4l2_do_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct pvr2_v4l2_fh *fh = file->private_data;
	struct pvr2_v4l2 *vp = fh->vhead;
	struct pvr2_v4l2_dev *pdi = fh->pdi;
	struct pvr2_hdw *hdw = fh->channel.mc_head->hdw;
	long ret = -EINVAL;

	if (pvrusb2_debug & PVR2_TRACE_V4LIOCTL) {
		v4l_print_ioctl(pvr2_hdw_get_driver_name(hdw),cmd);
	}

	if (!pvr2_hdw_dev_ok(hdw)) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "ioctl failed - bad or no context");
		return -EFAULT;
	}

	/* check priority */
	switch (cmd) {
	case VIDIOC_S_CTRL:
	case VIDIOC_S_STD:
	case VIDIOC_S_INPUT:
	case VIDIOC_S_TUNER:
	case VIDIOC_S_FREQUENCY:
		ret = v4l2_prio_check(&vp->prio, fh->prio);
		if (ret)
			return ret;
	}

	switch (cmd) {
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability *cap = arg;

		memcpy(cap, &pvr_capability, sizeof(struct v4l2_capability));
		strlcpy(cap->bus_info,pvr2_hdw_get_bus_info(hdw),
			sizeof(cap->bus_info));
		strlcpy(cap->card,pvr2_hdw_get_desc(hdw),sizeof(cap->card));

		ret = 0;
		break;
	}

	case VIDIOC_G_PRIORITY:
	{
		enum v4l2_priority *p = arg;

		*p = v4l2_prio_max(&vp->prio);
		ret = 0;
		break;
	}

	case VIDIOC_S_PRIORITY:
	{
		enum v4l2_priority *prio = arg;

		ret = v4l2_prio_change(&vp->prio, &fh->prio, *prio);
		break;
	}

	case VIDIOC_ENUMSTD:
	{
		struct v4l2_standard *vs = (struct v4l2_standard *)arg;
		int idx = vs->index;
		ret = pvr2_hdw_get_stdenum_value(hdw,vs,idx+1);
		break;
	}

	case VIDIOC_G_STD:
	{
		int val = 0;
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_STDCUR),&val);
		*(v4l2_std_id *)arg = val;
		break;
	}

	case VIDIOC_S_STD:
	{
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_STDCUR),
			*(v4l2_std_id *)arg);
		break;
	}

	case VIDIOC_ENUMINPUT:
	{
		struct pvr2_ctrl *cptr;
		struct v4l2_input *vi = (struct v4l2_input *)arg;
		struct v4l2_input tmp;
		unsigned int cnt;
		int val;

		cptr = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_INPUT);

		memset(&tmp,0,sizeof(tmp));
		tmp.index = vi->index;
		ret = 0;
		if (vi->index >= fh->input_cnt) {
			ret = -EINVAL;
			break;
		}
		val = fh->input_map[vi->index];
		switch (val) {
		case PVR2_CVAL_INPUT_TV:
		case PVR2_CVAL_INPUT_DTV:
		case PVR2_CVAL_INPUT_RADIO:
			tmp.type = V4L2_INPUT_TYPE_TUNER;
			break;
		case PVR2_CVAL_INPUT_SVIDEO:
		case PVR2_CVAL_INPUT_COMPOSITE:
			tmp.type = V4L2_INPUT_TYPE_CAMERA;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		if (ret < 0) break;

		cnt = 0;
		pvr2_ctrl_get_valname(cptr,val,
				      tmp.name,sizeof(tmp.name)-1,&cnt);
		tmp.name[cnt] = 0;

		/* Don't bother with audioset, since this driver currently
		   always switches the audio whenever the video is
		   switched. */

		/* Handling std is a tougher problem.  It doesn't make
		   sense in cases where a device might be multi-standard.
		   We could just copy out the current value for the
		   standard, but it can change over time.  For now just
		   leave it zero. */

		memcpy(vi, &tmp, sizeof(tmp));

		ret = 0;
		break;
	}

	case VIDIOC_G_INPUT:
	{
		unsigned int idx;
		struct pvr2_ctrl *cptr;
		struct v4l2_input *vi = (struct v4l2_input *)arg;
		int val;
		cptr = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_INPUT);
		val = 0;
		ret = pvr2_ctrl_get_value(cptr,&val);
		vi->index = 0;
		for (idx = 0; idx < fh->input_cnt; idx++) {
			if (fh->input_map[idx] == val) {
				vi->index = idx;
				break;
			}
		}
		break;
	}

	case VIDIOC_S_INPUT:
	{
		struct v4l2_input *vi = (struct v4l2_input *)arg;
		if (vi->index >= fh->input_cnt) {
			ret = -ERANGE;
			break;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_INPUT),
			fh->input_map[vi->index]);
		break;
	}

	case VIDIOC_ENUMAUDIO:
	{
		/* pkt: FIXME: We are returning one "fake" input here
		   which could very well be called "whatever_we_like".
		   This is for apps that want to see an audio input
		   just to feel comfortable, as well as to test if
		   it can do stereo or sth. There is actually no guarantee
		   that the actual audio input cannot change behind the app's
		   back, but most applications should not mind that either.

		   Hopefully, mplayer people will work with us on this (this
		   whole mess is to support mplayer pvr://), or Hans will come
		   up with a more standard way to say "we have inputs but we
		   don 't want you to change them independent of video" which
		   will sort this mess.
		 */
		struct v4l2_audio *vin = arg;
		ret = -EINVAL;
		if (vin->index > 0) break;
		strncpy(vin->name, "PVRUSB2 Audio",14);
		vin->capability = V4L2_AUDCAP_STEREO;
		ret = 0;
		break;
		break;
	}

	case VIDIOC_G_AUDIO:
	{
		/* pkt: FIXME: see above comment (VIDIOC_ENUMAUDIO) */
		struct v4l2_audio *vin = arg;
		memset(vin,0,sizeof(*vin));
		vin->index = 0;
		strncpy(vin->name, "PVRUSB2 Audio",14);
		vin->capability = V4L2_AUDCAP_STEREO;
		ret = 0;
		break;
	}

	case VIDIOC_S_AUDIO:
	{
		ret = -EINVAL;
		break;
	}
	case VIDIOC_G_TUNER:
	{
		struct v4l2_tuner *vt = (struct v4l2_tuner *)arg;

		if (vt->index != 0) break; /* Only answer for the 1st tuner */

		pvr2_hdw_execute_tuner_poll(hdw);
		ret = pvr2_hdw_get_tuner_status(hdw,vt);
		break;
	}

	case VIDIOC_S_TUNER:
	{
		struct v4l2_tuner *vt=(struct v4l2_tuner *)arg;

		if (vt->index != 0)
			break;

		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_AUDIOMODE),
			vt->audmode);
		break;
	}

	case VIDIOC_S_FREQUENCY:
	{
		const struct v4l2_frequency *vf = (struct v4l2_frequency *)arg;
		unsigned long fv;
		struct v4l2_tuner vt;
		int cur_input;
		struct pvr2_ctrl *ctrlp;
		ret = pvr2_hdw_get_tuner_status(hdw,&vt);
		if (ret != 0) break;
		ctrlp = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_INPUT);
		ret = pvr2_ctrl_get_value(ctrlp,&cur_input);
		if (ret != 0) break;
		if (vf->type == V4L2_TUNER_RADIO) {
			if (cur_input != PVR2_CVAL_INPUT_RADIO) {
				pvr2_ctrl_set_value(ctrlp,
						    PVR2_CVAL_INPUT_RADIO);
			}
		} else {
			if (cur_input == PVR2_CVAL_INPUT_RADIO) {
				pvr2_ctrl_set_value(ctrlp,
						    PVR2_CVAL_INPUT_TV);
			}
		}
		fv = vf->frequency;
		if (vt.capability & V4L2_TUNER_CAP_LOW) {
			fv = (fv * 125) / 2;
		} else {
			fv = fv * 62500;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_FREQUENCY),fv);
		break;
	}

	case VIDIOC_G_FREQUENCY:
	{
		struct v4l2_frequency *vf = (struct v4l2_frequency *)arg;
		int val = 0;
		int cur_input;
		struct v4l2_tuner vt;
		ret = pvr2_hdw_get_tuner_status(hdw,&vt);
		if (ret != 0) break;
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_FREQUENCY),
			&val);
		if (ret != 0) break;
		pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_INPUT),
			&cur_input);
		if (cur_input == PVR2_CVAL_INPUT_RADIO) {
			vf->type = V4L2_TUNER_RADIO;
		} else {
			vf->type = V4L2_TUNER_ANALOG_TV;
		}
		if (vt.capability & V4L2_TUNER_CAP_LOW) {
			val = (val * 2) / 125;
		} else {
			val /= 62500;
		}
		vf->frequency = val;
		break;
	}

	case VIDIOC_ENUM_FMT:
	{
		struct v4l2_fmtdesc *fd = (struct v4l2_fmtdesc *)arg;

		/* Only one format is supported : mpeg.*/
		if (fd->index != 0)
			break;

		memcpy(fd, pvr_fmtdesc, sizeof(struct v4l2_fmtdesc));
		ret = 0;
		break;
	}

	case VIDIOC_G_FMT:
	{
		struct v4l2_format *vf = (struct v4l2_format *)arg;
		int val;
		switch(vf->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			memcpy(vf, &pvr_format[PVR_FORMAT_PIX],
			       sizeof(struct v4l2_format));
			val = 0;
			pvr2_ctrl_get_value(
				pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_HRES),
				&val);
			vf->fmt.pix.width = val;
			val = 0;
			pvr2_ctrl_get_value(
				pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_VRES),
				&val);
			vf->fmt.pix.height = val;
			ret = 0;
			break;
		case V4L2_BUF_TYPE_VBI_CAPTURE:
			// ????? Still need to figure out to do VBI correctly
			ret = -EINVAL;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	}

	case VIDIOC_TRY_FMT:
	case VIDIOC_S_FMT:
	{
		struct v4l2_format *vf = (struct v4l2_format *)arg;

		ret = 0;
		switch(vf->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE: {
			int lmin,lmax,ldef;
			struct pvr2_ctrl *hcp,*vcp;
			int h = vf->fmt.pix.height;
			int w = vf->fmt.pix.width;
			hcp = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_HRES);
			vcp = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_VRES);

			lmin = pvr2_ctrl_get_min(hcp);
			lmax = pvr2_ctrl_get_max(hcp);
			pvr2_ctrl_get_def(hcp, &ldef);
			if (w == -1) {
				w = ldef;
			} else if (w < lmin) {
				w = lmin;
			} else if (w > lmax) {
				w = lmax;
			}
			lmin = pvr2_ctrl_get_min(vcp);
			lmax = pvr2_ctrl_get_max(vcp);
			pvr2_ctrl_get_def(vcp, &ldef);
			if (h == -1) {
				h = ldef;
			} else if (h < lmin) {
				h = lmin;
			} else if (h > lmax) {
				h = lmax;
			}

			memcpy(vf, &pvr_format[PVR_FORMAT_PIX],
			       sizeof(struct v4l2_format));
			vf->fmt.pix.width = w;
			vf->fmt.pix.height = h;

			if (cmd == VIDIOC_S_FMT) {
				pvr2_ctrl_set_value(hcp,vf->fmt.pix.width);
				pvr2_ctrl_set_value(vcp,vf->fmt.pix.height);
			}
		} break;
		case V4L2_BUF_TYPE_VBI_CAPTURE:
			// ????? Still need to figure out to do VBI correctly
			ret = -EINVAL;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	}

	case VIDIOC_STREAMON:
	{
		if (!fh->pdi->stream) {
			/* No stream defined for this node.  This means
			   that we're not currently allowed to stream from
			   this node. */
			ret = -EPERM;
			break;
		}
		ret = pvr2_hdw_set_stream_type(hdw,pdi->config);
		if (ret < 0) return ret;
		ret = pvr2_hdw_set_streaming(hdw,!0);
		break;
	}

	case VIDIOC_STREAMOFF:
	{
		if (!fh->pdi->stream) {
			/* No stream defined for this node.  This means
			   that we're not currently allowed to stream from
			   this node. */
			ret = -EPERM;
			break;
		}
		ret = pvr2_hdw_set_streaming(hdw,0);
		break;
	}

	case VIDIOC_QUERYCTRL:
	{
		struct pvr2_ctrl *cptr;
		int val;
		struct v4l2_queryctrl *vc = (struct v4l2_queryctrl *)arg;
		ret = 0;
		if (vc->id & V4L2_CTRL_FLAG_NEXT_CTRL) {
			cptr = pvr2_hdw_get_ctrl_nextv4l(
				hdw,(vc->id & ~V4L2_CTRL_FLAG_NEXT_CTRL));
			if (cptr) vc->id = pvr2_ctrl_get_v4lid(cptr);
		} else {
			cptr = pvr2_hdw_get_ctrl_v4l(hdw,vc->id);
		}
		if (!cptr) {
			pvr2_trace(PVR2_TRACE_V4LIOCTL,
				   "QUERYCTRL id=0x%x not implemented here",
				   vc->id);
			ret = -EINVAL;
			break;
		}

		pvr2_trace(PVR2_TRACE_V4LIOCTL,
			   "QUERYCTRL id=0x%x mapping name=%s (%s)",
			   vc->id,pvr2_ctrl_get_name(cptr),
			   pvr2_ctrl_get_desc(cptr));
		strlcpy(vc->name,pvr2_ctrl_get_desc(cptr),sizeof(vc->name));
		vc->flags = pvr2_ctrl_get_v4lflags(cptr);
		pvr2_ctrl_get_def(cptr, &val);
		vc->default_value = val;
		switch (pvr2_ctrl_get_type(cptr)) {
		case pvr2_ctl_enum:
			vc->type = V4L2_CTRL_TYPE_MENU;
			vc->minimum = 0;
			vc->maximum = pvr2_ctrl_get_cnt(cptr) - 1;
			vc->step = 1;
			break;
		case pvr2_ctl_bool:
			vc->type = V4L2_CTRL_TYPE_BOOLEAN;
			vc->minimum = 0;
			vc->maximum = 1;
			vc->step = 1;
			break;
		case pvr2_ctl_int:
			vc->type = V4L2_CTRL_TYPE_INTEGER;
			vc->minimum = pvr2_ctrl_get_min(cptr);
			vc->maximum = pvr2_ctrl_get_max(cptr);
			vc->step = 1;
			break;
		default:
			pvr2_trace(PVR2_TRACE_V4LIOCTL,
				   "QUERYCTRL id=0x%x name=%s not mappable",
				   vc->id,pvr2_ctrl_get_name(cptr));
			ret = -EINVAL;
			break;
		}
		break;
	}

	case VIDIOC_QUERYMENU:
	{
		struct v4l2_querymenu *vm = (struct v4l2_querymenu *)arg;
		unsigned int cnt = 0;
		ret = pvr2_ctrl_get_valname(pvr2_hdw_get_ctrl_v4l(hdw,vm->id),
					    vm->index,
					    vm->name,sizeof(vm->name)-1,
					    &cnt);
		vm->name[cnt] = 0;
		break;
	}

	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *vc = (struct v4l2_control *)arg;
		int val = 0;
		ret = pvr2_ctrl_get_value(pvr2_hdw_get_ctrl_v4l(hdw,vc->id),
					  &val);
		vc->value = val;
		break;
	}

	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *vc = (struct v4l2_control *)arg;
		ret = pvr2_ctrl_set_value(pvr2_hdw_get_ctrl_v4l(hdw,vc->id),
					  vc->value);
		break;
	}

	case VIDIOC_G_EXT_CTRLS:
	{
		struct v4l2_ext_controls *ctls =
			(struct v4l2_ext_controls *)arg;
		struct v4l2_ext_control *ctrl;
		unsigned int idx;
		int val;
		ret = 0;
		for (idx = 0; idx < ctls->count; idx++) {
			ctrl = ctls->controls + idx;
			ret = pvr2_ctrl_get_value(
				pvr2_hdw_get_ctrl_v4l(hdw,ctrl->id),&val);
			if (ret) {
				ctls->error_idx = idx;
				break;
			}
			/* Ensure that if read as a 64 bit value, the user
			   will still get a hopefully sane value */
			ctrl->value64 = 0;
			ctrl->value = val;
		}
		break;
	}

	case VIDIOC_S_EXT_CTRLS:
	{
		struct v4l2_ext_controls *ctls =
			(struct v4l2_ext_controls *)arg;
		struct v4l2_ext_control *ctrl;
		unsigned int idx;
		ret = 0;
		for (idx = 0; idx < ctls->count; idx++) {
			ctrl = ctls->controls + idx;
			ret = pvr2_ctrl_set_value(
				pvr2_hdw_get_ctrl_v4l(hdw,ctrl->id),
				ctrl->value);
			if (ret) {
				ctls->error_idx = idx;
				break;
			}
		}
		break;
	}

	case VIDIOC_TRY_EXT_CTRLS:
	{
		struct v4l2_ext_controls *ctls =
			(struct v4l2_ext_controls *)arg;
		struct v4l2_ext_control *ctrl;
		struct pvr2_ctrl *pctl;
		unsigned int idx;
		/* For the moment just validate that the requested control
		   actually exists. */
		ret = 0;
		for (idx = 0; idx < ctls->count; idx++) {
			ctrl = ctls->controls + idx;
			pctl = pvr2_hdw_get_ctrl_v4l(hdw,ctrl->id);
			if (!pctl) {
				ret = -EINVAL;
				ctls->error_idx = idx;
				break;
			}
		}
		break;
	}

	case VIDIOC_CROPCAP:
	{
		struct v4l2_cropcap *cap = (struct v4l2_cropcap *)arg;
		if (cap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_hdw_get_cropcap(hdw, cap);
		cap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE; /* paranoia */
		break;
	}
	case VIDIOC_G_CROP:
	{
		struct v4l2_crop *crop = (struct v4l2_crop *)arg;
		int val = 0;
		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPL), &val);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		crop->c.left = val;
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPT), &val);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		crop->c.top = val;
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPW), &val);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		crop->c.width = val;
		ret = pvr2_ctrl_get_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPH), &val);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		crop->c.height = val;
	}
	case VIDIOC_S_CROP:
	{
		struct v4l2_crop *crop = (struct v4l2_crop *)arg;
		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPL),
			crop->c.left);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPT),
			crop->c.top);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPW),
			crop->c.width);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
		ret = pvr2_ctrl_set_value(
			pvr2_hdw_get_ctrl_by_id(hdw, PVR2_CID_CROPH),
			crop->c.height);
		if (ret != 0) {
			ret = -EINVAL;
			break;
		}
	}
	case VIDIOC_LOG_STATUS:
	{
		pvr2_hdw_trigger_module_log(hdw);
		ret = 0;
		break;
	}
#ifdef CONFIG_VIDEO_ADV_DEBUG
	case VIDIOC_DBG_S_REGISTER:
	case VIDIOC_DBG_G_REGISTER:
	{
		u64 val;
		struct v4l2_dbg_register *req = (struct v4l2_dbg_register *)arg;
		if (cmd == VIDIOC_DBG_S_REGISTER) val = req->val;
		ret = pvr2_hdw_register_access(
			hdw, &req->match, req->reg,
			cmd == VIDIOC_DBG_S_REGISTER, &val);
		if (cmd == VIDIOC_DBG_G_REGISTER) req->val = val;
		break;
	}
#endif

	default :
		ret = -EINVAL;
		break;
	}

	pvr2_hdw_commit_ctl(hdw);

	if (ret < 0) {
		if (pvrusb2_debug & PVR2_TRACE_V4LIOCTL) {
			pvr2_trace(PVR2_TRACE_V4LIOCTL,
				   "pvr2_v4l2_do_ioctl failure, ret=%ld", ret);
		} else {
			if (pvrusb2_debug & PVR2_TRACE_V4LIOCTL) {
				pvr2_trace(PVR2_TRACE_V4LIOCTL,
					   "pvr2_v4l2_do_ioctl failure, ret=%ld"
					   " command was:", ret);
				v4l_print_ioctl(pvr2_hdw_get_driver_name(hdw),
						cmd);
			}
		}
	} else {
		pvr2_trace(PVR2_TRACE_V4LIOCTL,
			   "pvr2_v4l2_do_ioctl complete, ret=%ld (0x%lx)",
			   ret, ret);
	}
	return ret;
}

static void pvr2_v4l2_dev_destroy(struct pvr2_v4l2_dev *dip)
{
	struct pvr2_hdw *hdw = dip->v4lp->channel.mc_head->hdw;
	enum pvr2_config cfg = dip->config;
	char msg[80];
	unsigned int mcnt;

	/* Construct the unregistration message *before* we actually
	   perform the unregistration step.  By doing it this way we don't
	   have to worry about potentially touching deleted resources. */
	mcnt = scnprintf(msg, sizeof(msg) - 1,
			 "pvrusb2: unregistered device %s [%s]",
			 video_device_node_name(&dip->devbase),
			 pvr2_config_get_name(cfg));
	msg[mcnt] = 0;

	pvr2_hdw_v4l_store_minor_number(hdw,dip->minor_type,-1);

	/* Paranoia */
	dip->v4lp = NULL;
	dip->stream = NULL;

	/* Actual deallocation happens later when all internal references
	   are gone. */
	video_unregister_device(&dip->devbase);

	printk(KERN_INFO "%s\n", msg);

}


static void pvr2_v4l2_dev_disassociate_parent(struct pvr2_v4l2_dev *dip)
{
	if (!dip) return;
	if (!dip->devbase.parent) return;
	dip->devbase.parent = NULL;
	device_move(&dip->devbase.dev, NULL, DPM_ORDER_NONE);
}


static void pvr2_v4l2_destroy_no_lock(struct pvr2_v4l2 *vp)
{
	if (vp->dev_video) {
		pvr2_v4l2_dev_destroy(vp->dev_video);
		vp->dev_video = NULL;
	}
	if (vp->dev_radio) {
		pvr2_v4l2_dev_destroy(vp->dev_radio);
		vp->dev_radio = NULL;
	}

	pvr2_trace(PVR2_TRACE_STRUCT,"Destroying pvr2_v4l2 id=%p",vp);
	pvr2_channel_done(&vp->channel);
	kfree(vp);
}


static void pvr2_video_device_release(struct video_device *vdev)
{
	struct pvr2_v4l2_dev *dev;
	dev = container_of(vdev,struct pvr2_v4l2_dev,devbase);
	kfree(dev);
}


static void pvr2_v4l2_internal_check(struct pvr2_channel *chp)
{
	struct pvr2_v4l2 *vp;
	vp = container_of(chp,struct pvr2_v4l2,channel);
	if (!vp->channel.mc_head->disconnect_flag) return;
	pvr2_v4l2_dev_disassociate_parent(vp->dev_video);
	pvr2_v4l2_dev_disassociate_parent(vp->dev_radio);
	if (vp->vfirst) return;
	pvr2_v4l2_destroy_no_lock(vp);
}


static long pvr2_v4l2_ioctl(struct file *file,
			   unsigned int cmd, unsigned long arg)
{

	return video_usercopy(file, cmd, arg, pvr2_v4l2_do_ioctl);
}


static int pvr2_v4l2_release(struct file *file)
{
	struct pvr2_v4l2_fh *fhp = file->private_data;
	struct pvr2_v4l2 *vp = fhp->vhead;
	struct pvr2_hdw *hdw = fhp->channel.mc_head->hdw;

	pvr2_trace(PVR2_TRACE_OPEN_CLOSE,"pvr2_v4l2_release");

	if (fhp->rhp) {
		struct pvr2_stream *sp;
		pvr2_hdw_set_streaming(hdw,0);
		sp = pvr2_ioread_get_stream(fhp->rhp);
		if (sp) pvr2_stream_set_callback(sp,NULL,NULL);
		pvr2_ioread_destroy(fhp->rhp);
		fhp->rhp = NULL;
	}

	v4l2_prio_close(&vp->prio, fhp->prio);
	file->private_data = NULL;

	if (fhp->vnext) {
		fhp->vnext->vprev = fhp->vprev;
	} else {
		vp->vlast = fhp->vprev;
	}
	if (fhp->vprev) {
		fhp->vprev->vnext = fhp->vnext;
	} else {
		vp->vfirst = fhp->vnext;
	}
	fhp->vnext = NULL;
	fhp->vprev = NULL;
	fhp->vhead = NULL;
	pvr2_channel_done(&fhp->channel);
	pvr2_trace(PVR2_TRACE_STRUCT,
		   "Destroying pvr_v4l2_fh id=%p",fhp);
	if (fhp->input_map) {
		kfree(fhp->input_map);
		fhp->input_map = NULL;
	}
	kfree(fhp);
	if (vp->channel.mc_head->disconnect_flag && !vp->vfirst) {
		pvr2_v4l2_destroy_no_lock(vp);
	}
	return 0;
}


static int pvr2_v4l2_open(struct file *file)
{
	struct pvr2_v4l2_dev *dip; /* Our own context pointer */
	struct pvr2_v4l2_fh *fhp;
	struct pvr2_v4l2 *vp;
	struct pvr2_hdw *hdw;
	unsigned int input_mask = 0;
	unsigned int input_cnt,idx;
	int ret = 0;

	dip = container_of(video_devdata(file),struct pvr2_v4l2_dev,devbase);

	vp = dip->v4lp;
	hdw = vp->channel.hdw;

	pvr2_trace(PVR2_TRACE_OPEN_CLOSE,"pvr2_v4l2_open");

	if (!pvr2_hdw_dev_ok(hdw)) {
		pvr2_trace(PVR2_TRACE_OPEN_CLOSE,
			   "pvr2_v4l2_open: hardware not ready");
		return -EIO;
	}

	fhp = kzalloc(sizeof(*fhp),GFP_KERNEL);
	if (!fhp) {
		return -ENOMEM;
	}

	init_waitqueue_head(&fhp->wait_data);
	fhp->pdi = dip;

	pvr2_trace(PVR2_TRACE_STRUCT,"Creating pvr_v4l2_fh id=%p",fhp);
	pvr2_channel_init(&fhp->channel,vp->channel.mc_head);

	if (dip->v4l_type == VFL_TYPE_RADIO) {
		/* Opening device as a radio, legal input selection subset
		   is just the radio. */
		input_mask = (1 << PVR2_CVAL_INPUT_RADIO);
	} else {
		/* Opening the main V4L device, legal input selection
		   subset includes all analog inputs. */
		input_mask = ((1 << PVR2_CVAL_INPUT_RADIO) |
			      (1 << PVR2_CVAL_INPUT_TV) |
			      (1 << PVR2_CVAL_INPUT_COMPOSITE) |
			      (1 << PVR2_CVAL_INPUT_SVIDEO));
	}
	ret = pvr2_channel_limit_inputs(&fhp->channel,input_mask);
	if (ret) {
		pvr2_channel_done(&fhp->channel);
		pvr2_trace(PVR2_TRACE_STRUCT,
			   "Destroying pvr_v4l2_fh id=%p (input mask error)",
			   fhp);

		kfree(fhp);
		return ret;
	}

	input_mask &= pvr2_hdw_get_input_available(hdw);
	input_cnt = 0;
	for (idx = 0; idx < (sizeof(input_mask) << 3); idx++) {
		if (input_mask & (1 << idx)) input_cnt++;
	}
	fhp->input_cnt = input_cnt;
	fhp->input_map = kzalloc(input_cnt,GFP_KERNEL);
	if (!fhp->input_map) {
		pvr2_channel_done(&fhp->channel);
		pvr2_trace(PVR2_TRACE_STRUCT,
			   "Destroying pvr_v4l2_fh id=%p (input map failure)",
			   fhp);
		kfree(fhp);
		return -ENOMEM;
	}
	input_cnt = 0;
	for (idx = 0; idx < (sizeof(input_mask) << 3); idx++) {
		if (!(input_mask & (1 << idx))) continue;
		fhp->input_map[input_cnt++] = idx;
	}

	fhp->vnext = NULL;
	fhp->vprev = vp->vlast;
	if (vp->vlast) {
		vp->vlast->vnext = fhp;
	} else {
		vp->vfirst = fhp;
	}
	vp->vlast = fhp;
	fhp->vhead = vp;

	fhp->file = file;
	file->private_data = fhp;
	v4l2_prio_open(&vp->prio, &fhp->prio);

	fhp->fw_mode_flag = pvr2_hdw_cpufw_get_enabled(hdw);

	return 0;
}


static void pvr2_v4l2_notify(struct pvr2_v4l2_fh *fhp)
{
	wake_up(&fhp->wait_data);
}

static int pvr2_v4l2_iosetup(struct pvr2_v4l2_fh *fh)
{
	int ret;
	struct pvr2_stream *sp;
	struct pvr2_hdw *hdw;
	if (fh->rhp) return 0;

	if (!fh->pdi->stream) {
		/* No stream defined for this node.  This means that we're
		   not currently allowed to stream from this node. */
		return -EPERM;
	}

	/* First read() attempt.  Try to claim the stream and start
	   it... */
	if ((ret = pvr2_channel_claim_stream(&fh->channel,
					     fh->pdi->stream)) != 0) {
		/* Someone else must already have it */
		return ret;
	}

	fh->rhp = pvr2_channel_create_mpeg_stream(fh->pdi->stream);
	if (!fh->rhp) {
		pvr2_channel_claim_stream(&fh->channel,NULL);
		return -ENOMEM;
	}

	hdw = fh->channel.mc_head->hdw;
	sp = fh->pdi->stream->stream;
	pvr2_stream_set_callback(sp,(pvr2_stream_callback)pvr2_v4l2_notify,fh);
	pvr2_hdw_set_stream_type(hdw,fh->pdi->config);
	if ((ret = pvr2_hdw_set_streaming(hdw,!0)) < 0) return ret;
	return pvr2_ioread_set_enabled(fh->rhp,!0);
}


static ssize_t pvr2_v4l2_read(struct file *file,
			      char __user *buff, size_t count, loff_t *ppos)
{
	struct pvr2_v4l2_fh *fh = file->private_data;
	int ret;

	if (fh->fw_mode_flag) {
		struct pvr2_hdw *hdw = fh->channel.mc_head->hdw;
		char *tbuf;
		int c1,c2;
		int tcnt = 0;
		unsigned int offs = *ppos;

		tbuf = kmalloc(PAGE_SIZE,GFP_KERNEL);
		if (!tbuf) return -ENOMEM;

		while (count) {
			c1 = count;
			if (c1 > PAGE_SIZE) c1 = PAGE_SIZE;
			c2 = pvr2_hdw_cpufw_get(hdw,offs,tbuf,c1);
			if (c2 < 0) {
				tcnt = c2;
				break;
			}
			if (!c2) break;
			if (copy_to_user(buff,tbuf,c2)) {
				tcnt = -EFAULT;
				break;
			}
			offs += c2;
			tcnt += c2;
			buff += c2;
			count -= c2;
			*ppos += c2;
		}
		kfree(tbuf);
		return tcnt;
	}

	if (!fh->rhp) {
		ret = pvr2_v4l2_iosetup(fh);
		if (ret) {
			return ret;
		}
	}

	for (;;) {
		ret = pvr2_ioread_read(fh->rhp,buff,count);
		if (ret >= 0) break;
		if (ret != -EAGAIN) break;
		if (file->f_flags & O_NONBLOCK) break;
		/* Doing blocking I/O.  Wait here. */
		ret = wait_event_interruptible(
			fh->wait_data,
			pvr2_ioread_avail(fh->rhp) >= 0);
		if (ret < 0) break;
	}

	return ret;
}


static unsigned int pvr2_v4l2_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct pvr2_v4l2_fh *fh = file->private_data;
	int ret;

	if (fh->fw_mode_flag) {
		mask |= POLLIN | POLLRDNORM;
		return mask;
	}

	if (!fh->rhp) {
		ret = pvr2_v4l2_iosetup(fh);
		if (ret) return POLLERR;
	}

	poll_wait(file,&fh->wait_data,wait);

	if (pvr2_ioread_avail(fh->rhp) >= 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}


static const struct v4l2_file_operations vdev_fops = {
	.owner      = THIS_MODULE,
	.open       = pvr2_v4l2_open,
	.release    = pvr2_v4l2_release,
	.read       = pvr2_v4l2_read,
	.ioctl      = pvr2_v4l2_ioctl,
	.poll       = pvr2_v4l2_poll,
};


static struct video_device vdev_template = {
	.fops       = &vdev_fops,
};


static void pvr2_v4l2_dev_init(struct pvr2_v4l2_dev *dip,
			       struct pvr2_v4l2 *vp,
			       int v4l_type)
{
	struct usb_device *usbdev;
	int mindevnum;
	int unit_number;
	int *nr_ptr = NULL;
	dip->v4lp = vp;

	usbdev = pvr2_hdw_get_dev(vp->channel.mc_head->hdw);
	dip->v4l_type = v4l_type;
	switch (v4l_type) {
	case VFL_TYPE_GRABBER:
		dip->stream = &vp->channel.mc_head->video_stream;
		dip->config = pvr2_config_mpeg;
		dip->minor_type = pvr2_v4l_type_video;
		nr_ptr = video_nr;
		if (!dip->stream) {
			pr_err(KBUILD_MODNAME
				": Failed to set up pvrusb2 v4l video dev"
				" due to missing stream instance\n");
			return;
		}
		break;
	case VFL_TYPE_VBI:
		dip->config = pvr2_config_vbi;
		dip->minor_type = pvr2_v4l_type_vbi;
		nr_ptr = vbi_nr;
		break;
	case VFL_TYPE_RADIO:
		dip->stream = &vp->channel.mc_head->video_stream;
		dip->config = pvr2_config_mpeg;
		dip->minor_type = pvr2_v4l_type_radio;
		nr_ptr = radio_nr;
		break;
	default:
		/* Bail out (this should be impossible) */
		pr_err(KBUILD_MODNAME ": Failed to set up pvrusb2 v4l dev"
		    " due to unrecognized config\n");
		return;
	}

	memcpy(&dip->devbase,&vdev_template,sizeof(vdev_template));
	dip->devbase.release = pvr2_video_device_release;

	mindevnum = -1;
	unit_number = pvr2_hdw_get_unit_number(vp->channel.mc_head->hdw);
	if (nr_ptr && (unit_number >= 0) && (unit_number < PVR_NUM)) {
		mindevnum = nr_ptr[unit_number];
	}
	dip->devbase.parent = &usbdev->dev;
	if ((video_register_device(&dip->devbase,
				   dip->v4l_type, mindevnum) < 0) &&
	    (video_register_device(&dip->devbase,
				   dip->v4l_type, -1) < 0)) {
		pr_err(KBUILD_MODNAME
			": Failed to register pvrusb2 v4l device\n");
	}

	printk(KERN_INFO "pvrusb2: registered device %s [%s]\n",
	       video_device_node_name(&dip->devbase),
	       pvr2_config_get_name(dip->config));

	pvr2_hdw_v4l_store_minor_number(vp->channel.mc_head->hdw,
					dip->minor_type,dip->devbase.minor);
}


struct pvr2_v4l2 *pvr2_v4l2_create(struct pvr2_context *mnp)
{
	struct pvr2_v4l2 *vp;

	vp = kzalloc(sizeof(*vp),GFP_KERNEL);
	if (!vp) return vp;
	pvr2_channel_init(&vp->channel,mnp);
	pvr2_trace(PVR2_TRACE_STRUCT,"Creating pvr2_v4l2 id=%p",vp);

	vp->channel.check_func = pvr2_v4l2_internal_check;

	/* register streams */
	vp->dev_video = kzalloc(sizeof(*vp->dev_video),GFP_KERNEL);
	if (!vp->dev_video) goto fail;
	pvr2_v4l2_dev_init(vp->dev_video,vp,VFL_TYPE_GRABBER);
	if (pvr2_hdw_get_input_available(vp->channel.mc_head->hdw) &
	    (1 << PVR2_CVAL_INPUT_RADIO)) {
		vp->dev_radio = kzalloc(sizeof(*vp->dev_radio),GFP_KERNEL);
		if (!vp->dev_radio) goto fail;
		pvr2_v4l2_dev_init(vp->dev_radio,vp,VFL_TYPE_RADIO);
	}

	return vp;
 fail:
	pvr2_trace(PVR2_TRACE_STRUCT,"Failure creating pvr2_v4l2 id=%p",vp);
	pvr2_v4l2_destroy_no_lock(vp);
	return NULL;
}

/*
  Stuff for Emacs to see, in order to encourage consistent editing style:
  *** Local Variables: ***
  *** mode: c ***
  *** fill-column: 75 ***
  *** tab-width: 8 ***
  *** c-basic-offset: 8 ***
  *** End: ***
  */
