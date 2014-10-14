/*
 * Video capture interface for Linux version 2
 *
 * A generic framework to process V4L2 ioctl commands.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Authors:	Alan Cox, <alan@lxorguk.ukuu.org.uk> (version 1)
 *              Mauro Carvalho Chehab <mchehab@infradead.org> (version 2)
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/videodev2.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#define dbgarg(cmd, fmt, arg...) \
		do {							\
		    if (vfd->debug & V4L2_DEBUG_IOCTL_ARG) {		\
			printk(KERN_DEBUG "%s: ",  vfd->name);		\
			v4l_printk_ioctl(cmd);				\
			printk(" " fmt,  ## arg);			\
		    }							\
		} while (0)

#define dbgarg2(fmt, arg...) \
		do {							\
		    if (vfd->debug & V4L2_DEBUG_IOCTL_ARG)		\
			printk(KERN_DEBUG "%s: " fmt, vfd->name, ## arg);\
		} while (0)

#define dbgarg3(fmt, arg...) \
		do {							\
		    if (vfd->debug & V4L2_DEBUG_IOCTL_ARG)		\
			printk(KERN_CONT "%s: " fmt, vfd->name, ## arg);\
		} while (0)

/* Zero out the end of the struct pointed to by p.  Everything after, but
 * not including, the specified field is cleared. */
#define CLEAR_AFTER_FIELD(p, field) \
	memset((u8 *)(p) + offsetof(typeof(*(p)), field) + sizeof((p)->field), \
	0, sizeof(*(p)) - offsetof(typeof(*(p)), field) - sizeof((p)->field))

struct std_descr {
	v4l2_std_id std;
	const char *descr;
};

static const struct std_descr standards[] = {
	{ V4L2_STD_NTSC, 	"NTSC"      },
	{ V4L2_STD_NTSC_M, 	"NTSC-M"    },
	{ V4L2_STD_NTSC_M_JP, 	"NTSC-M-JP" },
	{ V4L2_STD_NTSC_M_KR,	"NTSC-M-KR" },
	{ V4L2_STD_NTSC_443, 	"NTSC-443"  },
	{ V4L2_STD_PAL, 	"PAL"       },
	{ V4L2_STD_PAL_BG, 	"PAL-BG"    },
	{ V4L2_STD_PAL_B, 	"PAL-B"     },
	{ V4L2_STD_PAL_B1, 	"PAL-B1"    },
	{ V4L2_STD_PAL_G, 	"PAL-G"     },
	{ V4L2_STD_PAL_H, 	"PAL-H"     },
	{ V4L2_STD_PAL_I, 	"PAL-I"     },
	{ V4L2_STD_PAL_DK, 	"PAL-DK"    },
	{ V4L2_STD_PAL_D, 	"PAL-D"     },
	{ V4L2_STD_PAL_D1, 	"PAL-D1"    },
	{ V4L2_STD_PAL_K, 	"PAL-K"     },
	{ V4L2_STD_PAL_M, 	"PAL-M"     },
	{ V4L2_STD_PAL_N, 	"PAL-N"     },
	{ V4L2_STD_PAL_Nc, 	"PAL-Nc"    },
	{ V4L2_STD_PAL_60, 	"PAL-60"    },
	{ V4L2_STD_SECAM, 	"SECAM"     },
	{ V4L2_STD_SECAM_B, 	"SECAM-B"   },
	{ V4L2_STD_SECAM_G, 	"SECAM-G"   },
	{ V4L2_STD_SECAM_H, 	"SECAM-H"   },
	{ V4L2_STD_SECAM_DK, 	"SECAM-DK"  },
	{ V4L2_STD_SECAM_D, 	"SECAM-D"   },
	{ V4L2_STD_SECAM_K, 	"SECAM-K"   },
	{ V4L2_STD_SECAM_K1, 	"SECAM-K1"  },
	{ V4L2_STD_SECAM_L, 	"SECAM-L"   },
	{ V4L2_STD_SECAM_LC, 	"SECAM-Lc"  },
	{ 0, 			"Unknown"   }
};

/* video4linux standard ID conversion to standard name
 */
const char *v4l2_norm_to_name(v4l2_std_id id)
{
	u32 myid = id;
	int i;

	/* HACK: ppc32 architecture doesn't have __ucmpdi2 function to handle
	   64 bit comparations. So, on that architecture, with some gcc
	   variants, compilation fails. Currently, the max value is 30bit wide.
	 */
	BUG_ON(myid != id);

	for (i = 0; standards[i].std; i++)
		if (myid == standards[i].std)
			break;
	return standards[i].descr;
}
EXPORT_SYMBOL(v4l2_norm_to_name);

/* Returns frame period for the given standard */
void v4l2_video_std_frame_period(int id, struct v4l2_fract *frameperiod)
{
	if (id & V4L2_STD_525_60) {
		frameperiod->numerator = 1001;
		frameperiod->denominator = 30000;
	} else {
		frameperiod->numerator = 1;
		frameperiod->denominator = 25;
	}
}
EXPORT_SYMBOL(v4l2_video_std_frame_period);

/* Fill in the fields of a v4l2_standard structure according to the
   'id' and 'transmission' parameters.  Returns negative on error.  */
int v4l2_video_std_construct(struct v4l2_standard *vs,
			     int id, const char *name)
{
	vs->id = id;
	v4l2_video_std_frame_period(id, &vs->frameperiod);
	vs->framelines = (id & V4L2_STD_525_60) ? 525 : 625;
	strlcpy(vs->name, name, sizeof(vs->name));
	return 0;
}
EXPORT_SYMBOL(v4l2_video_std_construct);

/* ----------------------------------------------------------------- */
/* some arrays for pretty-printing debug messages of enum types      */

const char *v4l2_field_names[] = {
	[V4L2_FIELD_ANY]        = "any",
	[V4L2_FIELD_NONE]       = "none",
	[V4L2_FIELD_TOP]        = "top",
	[V4L2_FIELD_BOTTOM]     = "bottom",
	[V4L2_FIELD_INTERLACED] = "interlaced",
	[V4L2_FIELD_SEQ_TB]     = "seq-tb",
	[V4L2_FIELD_SEQ_BT]     = "seq-bt",
	[V4L2_FIELD_ALTERNATE]  = "alternate",
	[V4L2_FIELD_INTERLACED_TB] = "interlaced-tb",
	[V4L2_FIELD_INTERLACED_BT] = "interlaced-bt",
};
EXPORT_SYMBOL(v4l2_field_names);

const char *v4l2_type_names[] = {
	[V4L2_BUF_TYPE_VIDEO_CAPTURE]      = "vid-cap",
	[V4L2_BUF_TYPE_VIDEO_OVERLAY]      = "vid-overlay",
	[V4L2_BUF_TYPE_VIDEO_OUTPUT]       = "vid-out",
	[V4L2_BUF_TYPE_VBI_CAPTURE]        = "vbi-cap",
	[V4L2_BUF_TYPE_VBI_OUTPUT]         = "vbi-out",
	[V4L2_BUF_TYPE_SLICED_VBI_CAPTURE] = "sliced-vbi-cap",
	[V4L2_BUF_TYPE_SLICED_VBI_OUTPUT]  = "sliced-vbi-out",
	[V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY] = "vid-out-overlay",
	[V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE] = "vid-cap-mplane",
	[V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE] = "vid-out-mplane",
};
EXPORT_SYMBOL(v4l2_type_names);

static const char *v4l2_memory_names[] = {
	[V4L2_MEMORY_MMAP]    = "mmap",
	[V4L2_MEMORY_USERPTR] = "userptr",
	[V4L2_MEMORY_OVERLAY] = "overlay",
};

#define prt_names(a, arr) ((((a) >= 0) && ((a) < ARRAY_SIZE(arr))) ? \
			   arr[a] : "unknown")

/* ------------------------------------------------------------------ */
/* debug help functions                                               */
static const char *v4l2_ioctls[] = {
	[_IOC_NR(VIDIOC_QUERYCAP)]         = "VIDIOC_QUERYCAP",
	[_IOC_NR(VIDIOC_RESERVED)]         = "VIDIOC_RESERVED",
	[_IOC_NR(VIDIOC_ENUM_FMT)]         = "VIDIOC_ENUM_FMT",
	[_IOC_NR(VIDIOC_G_FMT)]            = "VIDIOC_G_FMT",
	[_IOC_NR(VIDIOC_S_FMT)]            = "VIDIOC_S_FMT",
	[_IOC_NR(VIDIOC_REQBUFS)]          = "VIDIOC_REQBUFS",
	[_IOC_NR(VIDIOC_QUERYBUF)]         = "VIDIOC_QUERYBUF",
	[_IOC_NR(VIDIOC_G_FBUF)]           = "VIDIOC_G_FBUF",
	[_IOC_NR(VIDIOC_S_FBUF)]           = "VIDIOC_S_FBUF",
	[_IOC_NR(VIDIOC_OVERLAY)]          = "VIDIOC_OVERLAY",
	[_IOC_NR(VIDIOC_QBUF)]             = "VIDIOC_QBUF",
	[_IOC_NR(VIDIOC_DQBUF)]            = "VIDIOC_DQBUF",
	[_IOC_NR(VIDIOC_STREAMON)]         = "VIDIOC_STREAMON",
	[_IOC_NR(VIDIOC_STREAMOFF)]        = "VIDIOC_STREAMOFF",
	[_IOC_NR(VIDIOC_G_PARM)]           = "VIDIOC_G_PARM",
	[_IOC_NR(VIDIOC_S_PARM)]           = "VIDIOC_S_PARM",
	[_IOC_NR(VIDIOC_G_STD)]            = "VIDIOC_G_STD",
	[_IOC_NR(VIDIOC_S_STD)]            = "VIDIOC_S_STD",
	[_IOC_NR(VIDIOC_ENUMSTD)]          = "VIDIOC_ENUMSTD",
	[_IOC_NR(VIDIOC_ENUMINPUT)]        = "VIDIOC_ENUMINPUT",
	[_IOC_NR(VIDIOC_G_CTRL)]           = "VIDIOC_G_CTRL",
	[_IOC_NR(VIDIOC_S_CTRL)]           = "VIDIOC_S_CTRL",
	[_IOC_NR(VIDIOC_G_TUNER)]          = "VIDIOC_G_TUNER",
	[_IOC_NR(VIDIOC_S_TUNER)]          = "VIDIOC_S_TUNER",
	[_IOC_NR(VIDIOC_G_AUDIO)]          = "VIDIOC_G_AUDIO",
	[_IOC_NR(VIDIOC_S_AUDIO)]          = "VIDIOC_S_AUDIO",
	[_IOC_NR(VIDIOC_QUERYCTRL)]        = "VIDIOC_QUERYCTRL",
	[_IOC_NR(VIDIOC_QUERYMENU)]        = "VIDIOC_QUERYMENU",
	[_IOC_NR(VIDIOC_G_INPUT)]          = "VIDIOC_G_INPUT",
	[_IOC_NR(VIDIOC_S_INPUT)]          = "VIDIOC_S_INPUT",
	[_IOC_NR(VIDIOC_G_OUTPUT)]         = "VIDIOC_G_OUTPUT",
	[_IOC_NR(VIDIOC_S_OUTPUT)]         = "VIDIOC_S_OUTPUT",
	[_IOC_NR(VIDIOC_ENUMOUTPUT)]       = "VIDIOC_ENUMOUTPUT",
	[_IOC_NR(VIDIOC_G_AUDOUT)]         = "VIDIOC_G_AUDOUT",
	[_IOC_NR(VIDIOC_S_AUDOUT)]         = "VIDIOC_S_AUDOUT",
	[_IOC_NR(VIDIOC_G_MODULATOR)]      = "VIDIOC_G_MODULATOR",
	[_IOC_NR(VIDIOC_S_MODULATOR)]      = "VIDIOC_S_MODULATOR",
	[_IOC_NR(VIDIOC_G_FREQUENCY)]      = "VIDIOC_G_FREQUENCY",
	[_IOC_NR(VIDIOC_S_FREQUENCY)]      = "VIDIOC_S_FREQUENCY",
	[_IOC_NR(VIDIOC_CROPCAP)]          = "VIDIOC_CROPCAP",
	[_IOC_NR(VIDIOC_G_CROP)]           = "VIDIOC_G_CROP",
	[_IOC_NR(VIDIOC_S_CROP)]           = "VIDIOC_S_CROP",
	[_IOC_NR(VIDIOC_G_JPEGCOMP)]       = "VIDIOC_G_JPEGCOMP",
	[_IOC_NR(VIDIOC_S_JPEGCOMP)]       = "VIDIOC_S_JPEGCOMP",
	[_IOC_NR(VIDIOC_QUERYSTD)]         = "VIDIOC_QUERYSTD",
	[_IOC_NR(VIDIOC_TRY_FMT)]          = "VIDIOC_TRY_FMT",
	[_IOC_NR(VIDIOC_ENUMAUDIO)]        = "VIDIOC_ENUMAUDIO",
	[_IOC_NR(VIDIOC_ENUMAUDOUT)]       = "VIDIOC_ENUMAUDOUT",
	[_IOC_NR(VIDIOC_G_PRIORITY)]       = "VIDIOC_G_PRIORITY",
	[_IOC_NR(VIDIOC_S_PRIORITY)]       = "VIDIOC_S_PRIORITY",
	[_IOC_NR(VIDIOC_G_SLICED_VBI_CAP)] = "VIDIOC_G_SLICED_VBI_CAP",
	[_IOC_NR(VIDIOC_LOG_STATUS)]       = "VIDIOC_LOG_STATUS",
	[_IOC_NR(VIDIOC_G_EXT_CTRLS)]      = "VIDIOC_G_EXT_CTRLS",
	[_IOC_NR(VIDIOC_S_EXT_CTRLS)]      = "VIDIOC_S_EXT_CTRLS",
	[_IOC_NR(VIDIOC_TRY_EXT_CTRLS)]    = "VIDIOC_TRY_EXT_CTRLS",
#if 1
	[_IOC_NR(VIDIOC_ENUM_FRAMESIZES)]  = "VIDIOC_ENUM_FRAMESIZES",
	[_IOC_NR(VIDIOC_ENUM_FRAMEINTERVALS)] = "VIDIOC_ENUM_FRAMEINTERVALS",
	[_IOC_NR(VIDIOC_G_ENC_INDEX)] 	   = "VIDIOC_G_ENC_INDEX",
	[_IOC_NR(VIDIOC_ENCODER_CMD)] 	   = "VIDIOC_ENCODER_CMD",
	[_IOC_NR(VIDIOC_TRY_ENCODER_CMD)]  = "VIDIOC_TRY_ENCODER_CMD",

	[_IOC_NR(VIDIOC_DBG_S_REGISTER)]   = "VIDIOC_DBG_S_REGISTER",
	[_IOC_NR(VIDIOC_DBG_G_REGISTER)]   = "VIDIOC_DBG_G_REGISTER",

	[_IOC_NR(VIDIOC_DBG_G_CHIP_IDENT)] = "VIDIOC_DBG_G_CHIP_IDENT",
	[_IOC_NR(VIDIOC_S_HW_FREQ_SEEK)]   = "VIDIOC_S_HW_FREQ_SEEK",
#endif
	[_IOC_NR(VIDIOC_ENUM_DV_PRESETS)]  = "VIDIOC_ENUM_DV_PRESETS",
	[_IOC_NR(VIDIOC_S_DV_PRESET)]	   = "VIDIOC_S_DV_PRESET",
	[_IOC_NR(VIDIOC_G_DV_PRESET)]	   = "VIDIOC_G_DV_PRESET",
	[_IOC_NR(VIDIOC_QUERY_DV_PRESET)]  = "VIDIOC_QUERY_DV_PRESET",
	[_IOC_NR(VIDIOC_S_DV_TIMINGS)]     = "VIDIOC_S_DV_TIMINGS",
	[_IOC_NR(VIDIOC_G_DV_TIMINGS)]     = "VIDIOC_G_DV_TIMINGS",
	[_IOC_NR(VIDIOC_DQEVENT)]	   = "VIDIOC_DQEVENT",
	[_IOC_NR(VIDIOC_SUBSCRIBE_EVENT)]  = "VIDIOC_SUBSCRIBE_EVENT",
	[_IOC_NR(VIDIOC_UNSUBSCRIBE_EVENT)] = "VIDIOC_UNSUBSCRIBE_EVENT",
};
#define V4L2_IOCTLS ARRAY_SIZE(v4l2_ioctls)

/* Common ioctl debug function. This function can be used by
   external ioctl messages as well as internal V4L ioctl */
void v4l_printk_ioctl(unsigned int cmd)
{
	char *dir, *type;

	switch (_IOC_TYPE(cmd)) {
	case 'd':
		type = "v4l2_int";
		break;
	case 'V':
		if (_IOC_NR(cmd) >= V4L2_IOCTLS) {
			type = "v4l2";
			break;
		}
		printk("%s", v4l2_ioctls[_IOC_NR(cmd)]);
		return;
	default:
		type = "unknown";
	}

	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:              dir = "--"; break;
	case _IOC_READ:              dir = "r-"; break;
	case _IOC_WRITE:             dir = "-w"; break;
	case _IOC_READ | _IOC_WRITE: dir = "rw"; break;
	default:                     dir = "*ERR*"; break;
	}
	printk("%s ioctl '%c', dir=%s, #%d (0x%08x)",
		type, _IOC_TYPE(cmd), dir, _IOC_NR(cmd), cmd);
}
EXPORT_SYMBOL(v4l_printk_ioctl);

static void dbgbuf(unsigned int cmd, struct video_device *vfd,
					struct v4l2_buffer *p)
{
	struct v4l2_timecode *tc = &p->timecode;
	struct v4l2_plane *plane;
	int i;

	dbgarg(cmd, "%02ld:%02d:%02d.%08ld index=%d, type=%s, "
		"flags=0x%08d, field=%0d, sequence=%d, memory=%s\n",
			p->timestamp.tv_sec / 3600,
			(int)(p->timestamp.tv_sec / 60) % 60,
			(int)(p->timestamp.tv_sec % 60),
			(long)p->timestamp.tv_usec,
			p->index,
			prt_names(p->type, v4l2_type_names),
			p->flags, p->field, p->sequence,
			prt_names(p->memory, v4l2_memory_names));

	if (V4L2_TYPE_IS_MULTIPLANAR(p->type) && p->m.planes) {
		for (i = 0; i < p->length; ++i) {
			plane = &p->m.planes[i];
			dbgarg2("plane %d: bytesused=%d, data_offset=0x%08x "
				"offset/userptr=0x%08lx, length=%d\n",
				i, plane->bytesused, plane->data_offset,
				plane->m.userptr, plane->length);
		}
	} else {
		dbgarg2("bytesused=%d, offset/userptr=0x%08lx, length=%d\n",
			p->bytesused, p->m.userptr, p->length);
	}

	dbgarg2("timecode=%02d:%02d:%02d type=%d, "
		"flags=0x%08d, frames=%d, userbits=0x%08x\n",
			tc->hours, tc->minutes, tc->seconds,
			tc->type, tc->flags, tc->frames, *(__u32 *)tc->userbits);
}

static inline void dbgrect(struct video_device *vfd, char *s,
							struct v4l2_rect *r)
{
	dbgarg2("%sRect start at %dx%d, size=%dx%d\n", s, r->left, r->top,
						r->width, r->height);
};

static inline void v4l_print_pix_fmt(struct video_device *vfd,
						struct v4l2_pix_format *fmt)
{
	dbgarg2("width=%d, height=%d, format=%c%c%c%c, field=%s, "
		"bytesperline=%d sizeimage=%d, colorspace=%d\n",
		fmt->width, fmt->height,
		(fmt->pixelformat & 0xff),
		(fmt->pixelformat >>  8) & 0xff,
		(fmt->pixelformat >> 16) & 0xff,
		(fmt->pixelformat >> 24) & 0xff,
		prt_names(fmt->field, v4l2_field_names),
		fmt->bytesperline, fmt->sizeimage, fmt->colorspace);
};

static inline void v4l_print_pix_fmt_mplane(struct video_device *vfd,
					    struct v4l2_pix_format_mplane *fmt)
{
	int i;

	dbgarg2("width=%d, height=%d, format=%c%c%c%c, field=%s, "
		"colorspace=%d, num_planes=%d\n",
		fmt->width, fmt->height,
		(fmt->pixelformat & 0xff),
		(fmt->pixelformat >>  8) & 0xff,
		(fmt->pixelformat >> 16) & 0xff,
		(fmt->pixelformat >> 24) & 0xff,
		prt_names(fmt->field, v4l2_field_names),
		fmt->colorspace, fmt->num_planes);

	for (i = 0; i < fmt->num_planes; ++i)
		dbgarg2("plane %d: bytesperline=%d sizeimage=%d\n", i,
			fmt->plane_fmt[i].bytesperline,
			fmt->plane_fmt[i].sizeimage);
}

static inline void v4l_print_ext_ctrls(unsigned int cmd,
	struct video_device *vfd, struct v4l2_ext_controls *c, int show_vals)
{
	__u32 i;

	if (!(vfd->debug & V4L2_DEBUG_IOCTL_ARG))
		return;
	dbgarg(cmd, "");
	printk(KERN_CONT "class=0x%x", c->ctrl_class);
	for (i = 0; i < c->count; i++) {
		if (show_vals && !c->controls[i].size)
			printk(KERN_CONT " id/val=0x%x/0x%x",
				c->controls[i].id, c->controls[i].value);
		else
			printk(KERN_CONT " id=0x%x,size=%u",
				c->controls[i].id, c->controls[i].size);
	}
	printk(KERN_CONT "\n");
};

static inline int check_ext_ctrls(struct v4l2_ext_controls *c, int allow_priv)
{
	__u32 i;

	/* zero the reserved fields */
	c->reserved[0] = c->reserved[1] = 0;
	for (i = 0; i < c->count; i++)
		c->controls[i].reserved2[0] = 0;

	/* V4L2_CID_PRIVATE_BASE cannot be used as control class
	   when using extended controls.
	   Only when passed in through VIDIOC_G_CTRL and VIDIOC_S_CTRL
	   is it allowed for backwards compatibility.
	 */
	if (!allow_priv && c->ctrl_class == V4L2_CID_PRIVATE_BASE)
		return 0;
	/* Check that all controls are from the same control class. */
	for (i = 0; i < c->count; i++) {
		if (V4L2_CTRL_ID2CLASS(c->controls[i].id) != c->ctrl_class) {
			c->error_idx = i;
			return 0;
		}
	}
	return 1;
}

static int check_fmt(const struct v4l2_ioctl_ops *ops, enum v4l2_buf_type type)
{
	if (ops == NULL)
		return -EINVAL;

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (ops->vidioc_g_fmt_vid_cap ||
				ops->vidioc_g_fmt_vid_cap_mplane)
			return 0;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (ops->vidioc_g_fmt_vid_cap_mplane)
			return 0;
		break;
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		if (ops->vidioc_g_fmt_vid_overlay)
			return 0;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (ops->vidioc_g_fmt_vid_out ||
				ops->vidioc_g_fmt_vid_out_mplane)
			return 0;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (ops->vidioc_g_fmt_vid_out_mplane)
			return 0;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		if (ops->vidioc_g_fmt_vid_out_overlay)
			return 0;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		if (ops->vidioc_g_fmt_vbi_cap)
			return 0;
		break;
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		if (ops->vidioc_g_fmt_vbi_out)
			return 0;
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		if (ops->vidioc_g_fmt_sliced_vbi_cap)
			return 0;
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		if (ops->vidioc_g_fmt_sliced_vbi_out)
			return 0;
		break;
	case V4L2_BUF_TYPE_PRIVATE:
		if (ops->vidioc_g_fmt_type_private)
			return 0;
		break;
	}
	return -EINVAL;
}

/**
 * fmt_sp_to_mp() - Convert a single-plane format to its multi-planar 1-plane
 * equivalent
 */
static int fmt_sp_to_mp(const struct v4l2_format *f_sp,
			struct v4l2_format *f_mp)
{
	struct v4l2_pix_format_mplane *pix_mp = &f_mp->fmt.pix_mp;
	const struct v4l2_pix_format *pix = &f_sp->fmt.pix;

	if (f_sp->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		f_mp->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else if (f_sp->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		f_mp->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		return -EINVAL;

	pix_mp->width = pix->width;
	pix_mp->height = pix->height;
	pix_mp->pixelformat = pix->pixelformat;
	pix_mp->field = pix->field;
	pix_mp->colorspace = pix->colorspace;
	pix_mp->num_planes = 1;
	pix_mp->plane_fmt[0].sizeimage = pix->sizeimage;
	pix_mp->plane_fmt[0].bytesperline = pix->bytesperline;

	return 0;
}

/**
 * fmt_mp_to_sp() - Convert a multi-planar 1-plane format to its single-planar
 * equivalent
 */
static int fmt_mp_to_sp(const struct v4l2_format *f_mp,
			struct v4l2_format *f_sp)
{
	const struct v4l2_pix_format_mplane *pix_mp = &f_mp->fmt.pix_mp;
	struct v4l2_pix_format *pix = &f_sp->fmt.pix;

	if (f_mp->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		f_sp->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else if (f_mp->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		f_sp->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		return -EINVAL;

	pix->width = pix_mp->width;
	pix->height = pix_mp->height;
	pix->pixelformat = pix_mp->pixelformat;
	pix->field = pix_mp->field;
	pix->colorspace = pix_mp->colorspace;
	pix->sizeimage = pix_mp->plane_fmt[0].sizeimage;
	pix->bytesperline = pix_mp->plane_fmt[0].bytesperline;

	return 0;
}

static long __video_do_ioctl(struct file *file,
		unsigned int cmd, void *arg)
{
	struct video_device *vfd = video_devdata(file);
	const struct v4l2_ioctl_ops *ops = vfd->ioctl_ops;
	void *fh = file->private_data;
	struct v4l2_fh *vfh = NULL;
	struct v4l2_format f_copy;
	int use_fh_prio = 0;
	long ret = -EINVAL;

	if (ops == NULL) {
		printk(KERN_WARNING "videodev: \"%s\" has no ioctl_ops.\n",
				vfd->name);
		return -EINVAL;
	}

	if ((vfd->debug & V4L2_DEBUG_IOCTL) &&
				!(vfd->debug & V4L2_DEBUG_IOCTL_ARG)) {
		v4l_print_ioctl(vfd->name, cmd);
		printk(KERN_CONT "\n");
	}

	if (test_bit(V4L2_FL_USES_V4L2_FH, &vfd->flags)) {
		vfh = file->private_data;
		use_fh_prio = test_bit(V4L2_FL_USE_FH_PRIO, &vfd->flags);
	}

	if (use_fh_prio) {
		switch (cmd) {
		case VIDIOC_S_CTRL:
		case VIDIOC_S_STD:
		case VIDIOC_S_INPUT:
		case VIDIOC_S_OUTPUT:
		case VIDIOC_S_TUNER:
		case VIDIOC_S_FREQUENCY:
		case VIDIOC_S_FMT:
		case VIDIOC_S_CROP:
		case VIDIOC_S_AUDIO:
		case VIDIOC_S_AUDOUT:
		case VIDIOC_S_EXT_CTRLS:
		case VIDIOC_S_FBUF:
		case VIDIOC_S_PRIORITY:
		case VIDIOC_S_DV_PRESET:
		case VIDIOC_S_DV_TIMINGS:
		case VIDIOC_S_JPEGCOMP:
		case VIDIOC_S_MODULATOR:
		case VIDIOC_S_PARM:
		case VIDIOC_S_HW_FREQ_SEEK:
		case VIDIOC_ENCODER_CMD:
		case VIDIOC_OVERLAY:
		case VIDIOC_REQBUFS:
		case VIDIOC_STREAMON:
		case VIDIOC_STREAMOFF:
			ret = v4l2_prio_check(vfd->prio, vfh->prio);
			if (ret)
				goto exit_prio;
			ret = -EINVAL;
			break;
		}
	}

	switch (cmd) {

	/* --- capabilities ------------------------------------------ */
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability *cap = (struct v4l2_capability *)arg;

		if (!ops->vidioc_querycap)
			break;

		ret = ops->vidioc_querycap(file, fh, cap);
		if (!ret)
			dbgarg(cmd, "driver=%s, card=%s, bus=%s, "
					"version=0x%08x, "
					"capabilities=0x%08x\n",
					cap->driver, cap->card, cap->bus_info,
					cap->version,
					cap->capabilities);
		break;
	}

	/* --- priority ------------------------------------------ */
	case VIDIOC_G_PRIORITY:
	{
		enum v4l2_priority *p = arg;

		if (ops->vidioc_g_priority) {
			ret = ops->vidioc_g_priority(file, fh, p);
		} else if (use_fh_prio) {
			*p = v4l2_prio_max(&vfd->v4l2_dev->prio);
			ret = 0;
		}
		if (!ret)
			dbgarg(cmd, "priority is %d\n", *p);
		break;
	}
	case VIDIOC_S_PRIORITY:
	{
		enum v4l2_priority *p = arg;

		if (!ops->vidioc_s_priority && !use_fh_prio)
				break;
		dbgarg(cmd, "setting priority to %d\n", *p);
		if (ops->vidioc_s_priority)
			ret = ops->vidioc_s_priority(file, fh, *p);
		else
			ret = v4l2_prio_change(&vfd->v4l2_dev->prio, &vfh->prio, *p);
		break;
	}

	/* --- capture ioctls ---------------------------------------- */
	case VIDIOC_ENUM_FMT:
	{
		struct v4l2_fmtdesc *f = arg;

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			if (ops->vidioc_enum_fmt_vid_cap)
				ret = ops->vidioc_enum_fmt_vid_cap(file, fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (ops->vidioc_enum_fmt_vid_cap_mplane)
				ret = ops->vidioc_enum_fmt_vid_cap_mplane(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			if (ops->vidioc_enum_fmt_vid_overlay)
				ret = ops->vidioc_enum_fmt_vid_overlay(file,
					fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			if (ops->vidioc_enum_fmt_vid_out)
				ret = ops->vidioc_enum_fmt_vid_out(file, fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (ops->vidioc_enum_fmt_vid_out_mplane)
				ret = ops->vidioc_enum_fmt_vid_out_mplane(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_PRIVATE:
			if (ops->vidioc_enum_fmt_type_private)
				ret = ops->vidioc_enum_fmt_type_private(file,
								fh, f);
			break;
		default:
			break;
		}
		if (!ret)
			dbgarg(cmd, "index=%d, type=%d, flags=%d, "
				"pixelformat=%c%c%c%c, description='%s'\n",
				f->index, f->type, f->flags,
				(f->pixelformat & 0xff),
				(f->pixelformat >>  8) & 0xff,
				(f->pixelformat >> 16) & 0xff,
				(f->pixelformat >> 24) & 0xff,
				f->description);
		break;
	}
	case VIDIOC_G_FMT:
	{
		struct v4l2_format *f = (struct v4l2_format *)arg;

		/* FIXME: Should be one dump per type */
		dbgarg(cmd, "type=%s\n", prt_names(f->type, v4l2_type_names));

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			if (ops->vidioc_g_fmt_vid_cap) {
				ret = ops->vidioc_g_fmt_vid_cap(file, fh, f);
			} else if (ops->vidioc_g_fmt_vid_cap_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_g_fmt_vid_cap_mplane(file, fh,
								       &f_copy);
				if (ret)
					break;

				/* Driver is currently in multi-planar format,
				 * we can't return it in single-planar API*/
				if (f_copy.fmt.pix_mp.num_planes > 1) {
					ret = -EBUSY;
					break;
				}

				ret = fmt_mp_to_sp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt(vfd, &f->fmt.pix);
			break;
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (ops->vidioc_g_fmt_vid_cap_mplane) {
				ret = ops->vidioc_g_fmt_vid_cap_mplane(file,
									fh, f);
			} else if (ops->vidioc_g_fmt_vid_cap) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_g_fmt_vid_cap(file,
								fh, &f_copy);
				if (ret)
					break;

				ret = fmt_sp_to_mp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			break;
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			if (ops->vidioc_g_fmt_vid_overlay)
				ret = ops->vidioc_g_fmt_vid_overlay(file,
								    fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			if (ops->vidioc_g_fmt_vid_out) {
				ret = ops->vidioc_g_fmt_vid_out(file, fh, f);
			} else if (ops->vidioc_g_fmt_vid_out_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_g_fmt_vid_out_mplane(file, fh,
									&f_copy);
				if (ret)
					break;

				/* Driver is currently in multi-planar format,
				 * we can't return it in single-planar API*/
				if (f_copy.fmt.pix_mp.num_planes > 1) {
					ret = -EBUSY;
					break;
				}

				ret = fmt_mp_to_sp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt(vfd, &f->fmt.pix);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (ops->vidioc_g_fmt_vid_out_mplane) {
				ret = ops->vidioc_g_fmt_vid_out_mplane(file,
									fh, f);
			} else if (ops->vidioc_g_fmt_vid_out) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_g_fmt_vid_out(file,
								fh, &f_copy);
				if (ret)
					break;

				ret = fmt_sp_to_mp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
			if (ops->vidioc_g_fmt_vid_out_overlay)
				ret = ops->vidioc_g_fmt_vid_out_overlay(file,
				       fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_CAPTURE:
			if (ops->vidioc_g_fmt_vbi_cap)
				ret = ops->vidioc_g_fmt_vbi_cap(file, fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_OUTPUT:
			if (ops->vidioc_g_fmt_vbi_out)
				ret = ops->vidioc_g_fmt_vbi_out(file, fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
			if (ops->vidioc_g_fmt_sliced_vbi_cap)
				ret = ops->vidioc_g_fmt_sliced_vbi_cap(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
			if (ops->vidioc_g_fmt_sliced_vbi_out)
				ret = ops->vidioc_g_fmt_sliced_vbi_out(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_PRIVATE:
			if (ops->vidioc_g_fmt_type_private)
				ret = ops->vidioc_g_fmt_type_private(file,
								fh, f);
			break;
		}

		break;
	}
	case VIDIOC_S_FMT:
	{
		struct v4l2_format *f = (struct v4l2_format *)arg;

		/* FIXME: Should be one dump per type */
		dbgarg(cmd, "type=%s\n", prt_names(f->type, v4l2_type_names));

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.pix);
			v4l_print_pix_fmt(vfd, &f->fmt.pix);
			if (ops->vidioc_s_fmt_vid_cap) {
				ret = ops->vidioc_s_fmt_vid_cap(file, fh, f);
			} else if (ops->vidioc_s_fmt_vid_cap_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_s_fmt_vid_cap_mplane(file, fh,
									&f_copy);
				if (ret)
					break;

				if (f_copy.fmt.pix_mp.num_planes > 1) {
					/* Drivers shouldn't adjust from 1-plane
					 * to more than 1-plane formats */
					ret = -EBUSY;
					WARN_ON(1);
					break;
				}

				ret = fmt_mp_to_sp(&f_copy, f);
			}
			break;
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			CLEAR_AFTER_FIELD(f, fmt.pix_mp);
			v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			if (ops->vidioc_s_fmt_vid_cap_mplane) {
				ret = ops->vidioc_s_fmt_vid_cap_mplane(file,
									fh, f);
			} else if (ops->vidioc_s_fmt_vid_cap &&
					f->fmt.pix_mp.num_planes == 1) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_s_fmt_vid_cap(file,
								fh, &f_copy);
				if (ret)
					break;

				ret = fmt_sp_to_mp(&f_copy, f);
			}
			break;
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			CLEAR_AFTER_FIELD(f, fmt.win);
			if (ops->vidioc_s_fmt_vid_overlay)
				ret = ops->vidioc_s_fmt_vid_overlay(file,
								    fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.pix);
			v4l_print_pix_fmt(vfd, &f->fmt.pix);
			if (ops->vidioc_s_fmt_vid_out) {
				ret = ops->vidioc_s_fmt_vid_out(file, fh, f);
			} else if (ops->vidioc_s_fmt_vid_out_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_s_fmt_vid_out_mplane(file, fh,
									&f_copy);
				if (ret)
					break;

				if (f_copy.fmt.pix_mp.num_planes > 1) {
					/* Drivers shouldn't adjust from 1-plane
					 * to more than 1-plane formats */
					ret = -EBUSY;
					WARN_ON(1);
					break;
				}

				ret = fmt_mp_to_sp(&f_copy, f);
			}
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			CLEAR_AFTER_FIELD(f, fmt.pix_mp);
			v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			if (ops->vidioc_s_fmt_vid_out_mplane) {
				ret = ops->vidioc_s_fmt_vid_out_mplane(file,
									fh, f);
			} else if (ops->vidioc_s_fmt_vid_out &&
					f->fmt.pix_mp.num_planes == 1) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_s_fmt_vid_out(file,
								fh, &f_copy);
				if (ret)
					break;

				ret = fmt_mp_to_sp(&f_copy, f);
			}
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
			CLEAR_AFTER_FIELD(f, fmt.win);
			if (ops->vidioc_s_fmt_vid_out_overlay)
				ret = ops->vidioc_s_fmt_vid_out_overlay(file,
					fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.vbi);
			if (ops->vidioc_s_fmt_vbi_cap)
				ret = ops->vidioc_s_fmt_vbi_cap(file, fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.vbi);
			if (ops->vidioc_s_fmt_vbi_out)
				ret = ops->vidioc_s_fmt_vbi_out(file, fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.sliced);
			if (ops->vidioc_s_fmt_sliced_vbi_cap)
				ret = ops->vidioc_s_fmt_sliced_vbi_cap(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.sliced);
			if (ops->vidioc_s_fmt_sliced_vbi_out)
				ret = ops->vidioc_s_fmt_sliced_vbi_out(file,
									fh, f);
			break;
		case V4L2_BUF_TYPE_PRIVATE:
			/* CLEAR_AFTER_FIELD(f, fmt.raw_data); <- does nothing */
			if (ops->vidioc_s_fmt_type_private)
				ret = ops->vidioc_s_fmt_type_private(file,
								fh, f);
			break;
		}
		break;
	}
	case VIDIOC_TRY_FMT:
	{
		struct v4l2_format *f = (struct v4l2_format *)arg;

		/* FIXME: Should be one dump per type */
		dbgarg(cmd, "type=%s\n", prt_names(f->type,
						v4l2_type_names));
		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.pix);
			if (ops->vidioc_try_fmt_vid_cap) {
				ret = ops->vidioc_try_fmt_vid_cap(file, fh, f);
			} else if (ops->vidioc_try_fmt_vid_cap_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_try_fmt_vid_cap_mplane(file,
								fh, &f_copy);
				if (ret)
					break;

				if (f_copy.fmt.pix_mp.num_planes > 1) {
					/* Drivers shouldn't adjust from 1-plane
					 * to more than 1-plane formats */
					ret = -EBUSY;
					WARN_ON(1);
					break;
				}
				ret = fmt_mp_to_sp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt(vfd, &f->fmt.pix);
			break;
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			CLEAR_AFTER_FIELD(f, fmt.pix_mp);
			if (ops->vidioc_try_fmt_vid_cap_mplane) {
				ret = ops->vidioc_try_fmt_vid_cap_mplane(file,
									 fh, f);
			} else if (ops->vidioc_try_fmt_vid_cap &&
					f->fmt.pix_mp.num_planes == 1) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_try_fmt_vid_cap(file,
								  fh, &f_copy);
				if (ret)
					break;

				ret = fmt_sp_to_mp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			break;
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			CLEAR_AFTER_FIELD(f, fmt.win);
			if (ops->vidioc_try_fmt_vid_overlay)
				ret = ops->vidioc_try_fmt_vid_overlay(file,
					fh, f);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.pix);
			if (ops->vidioc_try_fmt_vid_out) {
				ret = ops->vidioc_try_fmt_vid_out(file, fh, f);
			} else if (ops->vidioc_try_fmt_vid_out_mplane) {
				if (fmt_sp_to_mp(f, &f_copy))
					break;
				ret = ops->vidioc_try_fmt_vid_out_mplane(file,
								fh, &f_copy);
				if (ret)
					break;

				if (f_copy.fmt.pix_mp.num_planes > 1) {
					/* Drivers shouldn't adjust from 1-plane
					 * to more than 1-plane formats */
					ret = -EBUSY;
					WARN_ON(1);
					break;
				}
				ret = fmt_mp_to_sp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt(vfd, &f->fmt.pix);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			CLEAR_AFTER_FIELD(f, fmt.pix_mp);
			if (ops->vidioc_try_fmt_vid_out_mplane) {
				ret = ops->vidioc_try_fmt_vid_out_mplane(file,
									 fh, f);
			} else if (ops->vidioc_try_fmt_vid_out &&
					f->fmt.pix_mp.num_planes == 1) {
				if (fmt_mp_to_sp(f, &f_copy))
					break;
				ret = ops->vidioc_try_fmt_vid_out(file,
								  fh, &f_copy);
				if (ret)
					break;

				ret = fmt_sp_to_mp(&f_copy, f);
			}
			if (!ret)
				v4l_print_pix_fmt_mplane(vfd, &f->fmt.pix_mp);
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
			CLEAR_AFTER_FIELD(f, fmt.win);
			if (ops->vidioc_try_fmt_vid_out_overlay)
				ret = ops->vidioc_try_fmt_vid_out_overlay(file,
				       fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.vbi);
			if (ops->vidioc_try_fmt_vbi_cap)
				ret = ops->vidioc_try_fmt_vbi_cap(file, fh, f);
			break;
		case V4L2_BUF_TYPE_VBI_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.vbi);
			if (ops->vidioc_try_fmt_vbi_out)
				ret = ops->vidioc_try_fmt_vbi_out(file, fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
			CLEAR_AFTER_FIELD(f, fmt.sliced);
			if (ops->vidioc_try_fmt_sliced_vbi_cap)
				ret = ops->vidioc_try_fmt_sliced_vbi_cap(file,
								fh, f);
			break;
		case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
			CLEAR_AFTER_FIELD(f, fmt.sliced);
			if (ops->vidioc_try_fmt_sliced_vbi_out)
				ret = ops->vidioc_try_fmt_sliced_vbi_out(file,
								fh, f);
			break;
		case V4L2_BUF_TYPE_PRIVATE:
			/* CLEAR_AFTER_FIELD(f, fmt.raw_data); <- does nothing */
			if (ops->vidioc_try_fmt_type_private)
				ret = ops->vidioc_try_fmt_type_private(file,
								fh, f);
			break;
		}

		break;
	}
	/* FIXME: Those buf reqs could be handled here,
	   with some changes on videobuf to allow its header to be included at
	   videodev2.h or being merged at videodev2.
	 */
	case VIDIOC_REQBUFS:
	{
		struct v4l2_requestbuffers *p = arg;

		if (!ops->vidioc_reqbufs)
			break;
		ret = check_fmt(ops, p->type);
		if (ret)
			break;

		if (p->type < V4L2_BUF_TYPE_PRIVATE)
			CLEAR_AFTER_FIELD(p, memory);

		ret = ops->vidioc_reqbufs(file, fh, p);
		dbgarg(cmd, "count=%d, type=%s, memory=%s\n",
				p->count,
				prt_names(p->type, v4l2_type_names),
				prt_names(p->memory, v4l2_memory_names));
		break;
	}
	case VIDIOC_QUERYBUF:
	{
		struct v4l2_buffer *p = arg;

		if (!ops->vidioc_querybuf)
			break;
		ret = check_fmt(ops, p->type);
		if (ret)
			break;

		ret = ops->vidioc_querybuf(file, fh, p);
		if (!ret)
			dbgbuf(cmd, vfd, p);
		break;
	}
	case VIDIOC_QBUF:
	{
		struct v4l2_buffer *p = arg;

		if (!ops->vidioc_qbuf)
			break;
		ret = check_fmt(ops, p->type);
		if (ret)
			break;

		ret = ops->vidioc_qbuf(file, fh, p);
		if (!ret)
			dbgbuf(cmd, vfd, p);
		break;
	}
	case VIDIOC_DQBUF:
	{
		struct v4l2_buffer *p = arg;

		if (!ops->vidioc_dqbuf)
			break;
		ret = check_fmt(ops, p->type);
		if (ret)
			break;

		ret = ops->vidioc_dqbuf(file, fh, p);
		if (!ret)
			dbgbuf(cmd, vfd, p);
		break;
	}
	case VIDIOC_OVERLAY:
	{
		int *i = arg;

		if (!ops->vidioc_overlay)
			break;
		dbgarg(cmd, "value=%d\n", *i);
		ret = ops->vidioc_overlay(file, fh, *i);
		break;
	}
	case VIDIOC_G_FBUF:
	{
		struct v4l2_framebuffer *p = arg;

		if (!ops->vidioc_g_fbuf)
			break;
		ret = ops->vidioc_g_fbuf(file, fh, arg);
		if (!ret) {
			dbgarg(cmd, "capability=0x%x, flags=%d, base=0x%08lx\n",
					p->capability, p->flags,
					(unsigned long)p->base);
			v4l_print_pix_fmt(vfd, &p->fmt);
		}
		break;
	}
	case VIDIOC_S_FBUF:
	{
		struct v4l2_framebuffer *p = arg;

		if (!ops->vidioc_s_fbuf)
			break;
		dbgarg(cmd, "capability=0x%x, flags=%d, base=0x%08lx\n",
			p->capability, p->flags, (unsigned long)p->base);
		v4l_print_pix_fmt(vfd, &p->fmt);
		ret = ops->vidioc_s_fbuf(file, fh, arg);
		break;
	}
	case VIDIOC_STREAMON:
	{
		enum v4l2_buf_type i = *(int *)arg;

		if (!ops->vidioc_streamon)
			break;
		dbgarg(cmd, "type=%s\n", prt_names(i, v4l2_type_names));
		ret = ops->vidioc_streamon(file, fh, i);
		break;
	}
	case VIDIOC_STREAMOFF:
	{
		enum v4l2_buf_type i = *(int *)arg;

		if (!ops->vidioc_streamoff)
			break;
		dbgarg(cmd, "type=%s\n", prt_names(i, v4l2_type_names));
		ret = ops->vidioc_streamoff(file, fh, i);
		break;
	}
	/* ---------- tv norms ---------- */
	case VIDIOC_ENUMSTD:
	{
		struct v4l2_standard *p = arg;
		v4l2_std_id id = vfd->tvnorms, curr_id = 0;
		unsigned int index = p->index, i, j = 0;
		const char *descr = "";

		/* Return norm array in a canonical way */
		for (i = 0; i <= index && id; i++) {
			/* last std value in the standards array is 0, so this
			   while always ends there since (id & 0) == 0. */
			while ((id & standards[j].std) != standards[j].std)
				j++;
			curr_id = standards[j].std;
			descr = standards[j].descr;
			j++;
			if (curr_id == 0)
				break;
			if (curr_id != V4L2_STD_PAL &&
			    curr_id != V4L2_STD_SECAM &&
			    curr_id != V4L2_STD_NTSC)
				id &= ~curr_id;
		}
		if (i <= index)
			break;

		v4l2_video_std_construct(p, curr_id, descr);

		dbgarg(cmd, "index=%d, id=0x%Lx, name=%s, fps=%d/%d, "
				"framelines=%d\n", p->index,
				(unsigned long long)p->id, p->name,
				p->frameperiod.numerator,
				p->frameperiod.denominator,
				p->framelines);

		ret = 0;
		break;
	}
	case VIDIOC_G_STD:
	{
		v4l2_std_id *id = arg;

		ret = 0;
		/* Calls the specific handler */
		if (ops->vidioc_g_std)
			ret = ops->vidioc_g_std(file, fh, id);
		else if (vfd->current_norm)
			*id = vfd->current_norm;
		else
			ret = -EINVAL;

		if (!ret)
			dbgarg(cmd, "std=0x%08Lx\n", (long long unsigned)*id);
		break;
	}
	case VIDIOC_S_STD:
	{
		v4l2_std_id *id = arg, norm;

		dbgarg(cmd, "std=%08Lx\n", (long long unsigned)*id);

		norm = (*id) & vfd->tvnorms;
		if (vfd->tvnorms && !norm)	/* Check if std is supported */
			break;

		/* Calls the specific handler */
		if (ops->vidioc_s_std)
			ret = ops->vidioc_s_std(file, fh, &norm);
		else
			ret = -EINVAL;

		/* Updates standard information */
		if (ret >= 0)
			vfd->current_norm = norm;
		break;
	}
	case VIDIOC_QUERYSTD:
	{
		v4l2_std_id *p = arg;

		if (!ops->vidioc_querystd)
			break;
		ret = ops->vidioc_querystd(file, fh, arg);
		if (!ret)
			dbgarg(cmd, "detected std=%08Lx\n",
						(unsigned long long)*p);
		break;
	}
	/* ------ input switching ---------- */
	/* FIXME: Inputs can be handled inside videodev2 */
	case VIDIOC_ENUMINPUT:
	{
		struct v4l2_input *p = arg;

		/*
		 * We set the flags for CAP_PRESETS, CAP_CUSTOM_TIMINGS &
		 * CAP_STD here based on ioctl handler provided by the
		 * driver. If the driver doesn't support these
		 * for a specific input, it must override these flags.
		 */
		if (ops->vidioc_s_std)
			p->capabilities |= V4L2_IN_CAP_STD;
		if (ops->vidioc_s_dv_preset)
			p->capabilities |= V4L2_IN_CAP_PRESETS;
		if (ops->vidioc_s_dv_timings)
			p->capabilities |= V4L2_IN_CAP_CUSTOM_TIMINGS;

		if (!ops->vidioc_enum_input)
			break;

		ret = ops->vidioc_enum_input(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, type=%d, "
				"audioset=%d, "
				"tuner=%d, std=%08Lx, status=%d\n",
				p->index, p->name, p->type, p->audioset,
				p->tuner,
				(unsigned long long)p->std,
				p->status);
		break;
	}
	case VIDIOC_G_INPUT:
	{
		unsigned int *i = arg;

		if (!ops->vidioc_g_input)
			break;
		ret = ops->vidioc_g_input(file, fh, i);
		if (!ret)
			dbgarg(cmd, "value=%d\n", *i);
		break;
	}
	case VIDIOC_S_INPUT:
	{
		unsigned int *i = arg;

		if (!ops->vidioc_s_input)
			break;
		dbgarg(cmd, "value=%d\n", *i);
		ret = ops->vidioc_s_input(file, fh, *i);
		break;
	}

	/* ------ output switching ---------- */
	case VIDIOC_ENUMOUTPUT:
	{
		struct v4l2_output *p = arg;

		if (!ops->vidioc_enum_output)
			break;

		/*
		 * We set the flags for CAP_PRESETS, CAP_CUSTOM_TIMINGS &
		 * CAP_STD here based on ioctl handler provided by the
		 * driver. If the driver doesn't support these
		 * for a specific output, it must override these flags.
		 */
		if (ops->vidioc_s_std)
			p->capabilities |= V4L2_OUT_CAP_STD;
		if (ops->vidioc_s_dv_preset)
			p->capabilities |= V4L2_OUT_CAP_PRESETS;
		if (ops->vidioc_s_dv_timings)
			p->capabilities |= V4L2_OUT_CAP_CUSTOM_TIMINGS;

		ret = ops->vidioc_enum_output(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, type=%d, "
				"audioset=0x%x, "
				"modulator=%d, std=0x%08Lx\n",
				p->index, p->name, p->type, p->audioset,
				p->modulator, (unsigned long long)p->std);
		break;
	}
	case VIDIOC_G_OUTPUT:
	{
		unsigned int *i = arg;

		if (!ops->vidioc_g_output)
			break;
		ret = ops->vidioc_g_output(file, fh, i);
		if (!ret)
			dbgarg(cmd, "value=%d\n", *i);
		break;
	}
	case VIDIOC_S_OUTPUT:
	{
		unsigned int *i = arg;

		if (!ops->vidioc_s_output)
			break;
		dbgarg(cmd, "value=%d\n", *i);
		ret = ops->vidioc_s_output(file, fh, *i);
		break;
	}

	/* --- controls ---------------------------------------------- */
	case VIDIOC_QUERYCTRL:
	{
		struct v4l2_queryctrl *p = arg;

		if (vfd->ctrl_handler)
			ret = v4l2_queryctrl(vfd->ctrl_handler, p);
		else if (ops->vidioc_queryctrl)
			ret = ops->vidioc_queryctrl(file, fh, p);
		else
			break;
		if (!ret)
			dbgarg(cmd, "id=0x%x, type=%d, name=%s, min/max=%d/%d, "
					"step=%d, default=%d, flags=0x%08x\n",
					p->id, p->type, p->name,
					p->minimum, p->maximum,
					p->step, p->default_value, p->flags);
		else
			dbgarg(cmd, "id=0x%x\n", p->id);
		break;
	}
	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *p = arg;

		if (vfd->ctrl_handler)
			ret = v4l2_g_ctrl(vfd->ctrl_handler, p);
		else if (ops->vidioc_g_ctrl)
			ret = ops->vidioc_g_ctrl(file, fh, p);
		else if (ops->vidioc_g_ext_ctrls) {
			struct v4l2_ext_controls ctrls;
			struct v4l2_ext_control ctrl;

			ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(p->id);
			ctrls.count = 1;
			ctrls.controls = &ctrl;
			ctrl.id = p->id;
			ctrl.value = p->value;
			if (check_ext_ctrls(&ctrls, 1)) {
				ret = ops->vidioc_g_ext_ctrls(file, fh, &ctrls);
				if (ret == 0)
					p->value = ctrl.value;
			}
		} else
			break;
		if (!ret)
			dbgarg(cmd, "id=0x%x, value=%d\n", p->id, p->value);
		else
			dbgarg(cmd, "id=0x%x\n", p->id);
		break;
	}
	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *p = arg;
		struct v4l2_ext_controls ctrls;
		struct v4l2_ext_control ctrl;

		if (!vfd->ctrl_handler &&
			!ops->vidioc_s_ctrl && !ops->vidioc_s_ext_ctrls)
			break;

		dbgarg(cmd, "id=0x%x, value=%d\n", p->id, p->value);

		if (vfd->ctrl_handler) {
			ret = v4l2_s_ctrl(vfd->ctrl_handler, p);
			break;
		}
		if (ops->vidioc_s_ctrl) {
			ret = ops->vidioc_s_ctrl(file, fh, p);
			break;
		}
		if (!ops->vidioc_s_ext_ctrls)
			break;

		ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(p->id);
		ctrls.count = 1;
		ctrls.controls = &ctrl;
		ctrl.id = p->id;
		ctrl.value = p->value;
		if (check_ext_ctrls(&ctrls, 1))
			ret = ops->vidioc_s_ext_ctrls(file, fh, &ctrls);
		break;
	}
	case VIDIOC_G_EXT_CTRLS:
	{
		struct v4l2_ext_controls *p = arg;

		p->error_idx = p->count;
		if (vfd->ctrl_handler)
			ret = v4l2_g_ext_ctrls(vfd->ctrl_handler, p);
		else if (ops->vidioc_g_ext_ctrls && check_ext_ctrls(p, 0))
			ret = ops->vidioc_g_ext_ctrls(file, fh, p);
		else
			break;
		v4l_print_ext_ctrls(cmd, vfd, p, !ret);
		break;
	}
	case VIDIOC_S_EXT_CTRLS:
	{
		struct v4l2_ext_controls *p = arg;

		p->error_idx = p->count;
		if (!vfd->ctrl_handler && !ops->vidioc_s_ext_ctrls)
			break;
		v4l_print_ext_ctrls(cmd, vfd, p, 1);
		if (vfd->ctrl_handler)
			ret = v4l2_s_ext_ctrls(vfd->ctrl_handler, p);
		else if (check_ext_ctrls(p, 0))
			ret = ops->vidioc_s_ext_ctrls(file, fh, p);
		break;
	}
	case VIDIOC_TRY_EXT_CTRLS:
	{
		struct v4l2_ext_controls *p = arg;

		p->error_idx = p->count;
		if (!vfd->ctrl_handler && !ops->vidioc_try_ext_ctrls)
			break;
		v4l_print_ext_ctrls(cmd, vfd, p, 1);
		if (vfd->ctrl_handler)
			ret = v4l2_try_ext_ctrls(vfd->ctrl_handler, p);
		else if (check_ext_ctrls(p, 0))
			ret = ops->vidioc_try_ext_ctrls(file, fh, p);
		break;
	}
	case VIDIOC_QUERYMENU:
	{
		struct v4l2_querymenu *p = arg;

		if (vfd->ctrl_handler)
			ret = v4l2_querymenu(vfd->ctrl_handler, p);
		else if (ops->vidioc_querymenu)
			ret = ops->vidioc_querymenu(file, fh, p);
		else
			break;
		if (!ret)
			dbgarg(cmd, "id=0x%x, index=%d, name=%s\n",
				p->id, p->index, p->name);
		else
			dbgarg(cmd, "id=0x%x, index=%d\n",
				p->id, p->index);
		break;
	}
	/* --- audio ---------------------------------------------- */
	case VIDIOC_ENUMAUDIO:
	{
		struct v4l2_audio *p = arg;

		if (!ops->vidioc_enumaudio)
			break;
		ret = ops->vidioc_enumaudio(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, capability=0x%x, "
					"mode=0x%x\n", p->index, p->name,
					p->capability, p->mode);
		else
			dbgarg(cmd, "index=%d\n", p->index);
		break;
	}
	case VIDIOC_G_AUDIO:
	{
		struct v4l2_audio *p = arg;

		if (!ops->vidioc_g_audio)
			break;

		ret = ops->vidioc_g_audio(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, capability=0x%x, "
					"mode=0x%x\n", p->index,
					p->name, p->capability, p->mode);
		else
			dbgarg(cmd, "index=%d\n", p->index);
		break;
	}
	case VIDIOC_S_AUDIO:
	{
		struct v4l2_audio *p = arg;

		if (!ops->vidioc_s_audio)
			break;
		dbgarg(cmd, "index=%d, name=%s, capability=0x%x, "
					"mode=0x%x\n", p->index, p->name,
					p->capability, p->mode);
		ret = ops->vidioc_s_audio(file, fh, p);
		break;
	}
	case VIDIOC_ENUMAUDOUT:
	{
		struct v4l2_audioout *p = arg;

		if (!ops->vidioc_enumaudout)
			break;
		dbgarg(cmd, "Enum for index=%d\n", p->index);
		ret = ops->vidioc_enumaudout(file, fh, p);
		if (!ret)
			dbgarg2("index=%d, name=%s, capability=%d, "
					"mode=%d\n", p->index, p->name,
					p->capability, p->mode);
		break;
	}
	case VIDIOC_G_AUDOUT:
	{
		struct v4l2_audioout *p = arg;

		if (!ops->vidioc_g_audout)
			break;

		ret = ops->vidioc_g_audout(file, fh, p);
		if (!ret)
			dbgarg2("index=%d, name=%s, capability=%d, "
					"mode=%d\n", p->index, p->name,
					p->capability, p->mode);
		break;
	}
	case VIDIOC_S_AUDOUT:
	{
		struct v4l2_audioout *p = arg;

		if (!ops->vidioc_s_audout)
			break;
		dbgarg(cmd, "index=%d, name=%s, capability=%d, "
					"mode=%d\n", p->index, p->name,
					p->capability, p->mode);

		ret = ops->vidioc_s_audout(file, fh, p);
		break;
	}
	case VIDIOC_G_MODULATOR:
	{
		struct v4l2_modulator *p = arg;

		if (!ops->vidioc_g_modulator)
			break;
		ret = ops->vidioc_g_modulator(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, "
					"capability=%d, rangelow=%d,"
					" rangehigh=%d, txsubchans=%d\n",
					p->index, p->name, p->capability,
					p->rangelow, p->rangehigh,
					p->txsubchans);
		break;
	}
	case VIDIOC_S_MODULATOR:
	{
		struct v4l2_modulator *p = arg;

		if (!ops->vidioc_s_modulator)
			break;
		dbgarg(cmd, "index=%d, name=%s, capability=%d, "
				"rangelow=%d, rangehigh=%d, txsubchans=%d\n",
				p->index, p->name, p->capability, p->rangelow,
				p->rangehigh, p->txsubchans);
			ret = ops->vidioc_s_modulator(file, fh, p);
		break;
	}
	case VIDIOC_G_CROP:
	{
		struct v4l2_crop *p = arg;

		if (!ops->vidioc_g_crop)
			break;

		dbgarg(cmd, "type=%s\n", prt_names(p->type, v4l2_type_names));
		ret = ops->vidioc_g_crop(file, fh, p);
		if (!ret)
			dbgrect(vfd, "", &p->c);
		break;
	}
	case VIDIOC_S_CROP:
	{
		struct v4l2_crop *p = arg;

		if (!ops->vidioc_s_crop)
			break;
		dbgarg(cmd, "type=%s\n", prt_names(p->type, v4l2_type_names));
		dbgrect(vfd, "", &p->c);
		ret = ops->vidioc_s_crop(file, fh, p);
		break;
	}
	case VIDIOC_CROPCAP:
	{
		struct v4l2_cropcap *p = arg;

		/*FIXME: Should also show v4l2_fract pixelaspect */
		if (!ops->vidioc_cropcap)
			break;

		dbgarg(cmd, "type=%s\n", prt_names(p->type, v4l2_type_names));
		ret = ops->vidioc_cropcap(file, fh, p);
		if (!ret) {
			dbgrect(vfd, "bounds ", &p->bounds);
			dbgrect(vfd, "defrect ", &p->defrect);
		}
		break;
	}
	case VIDIOC_G_JPEGCOMP:
	{
		struct v4l2_jpegcompression *p = arg;

		if (!ops->vidioc_g_jpegcomp)
			break;

		ret = ops->vidioc_g_jpegcomp(file, fh, p);
		if (!ret)
			dbgarg(cmd, "quality=%d, APPn=%d, "
					"APP_len=%d, COM_len=%d, "
					"jpeg_markers=%d\n",
					p->quality, p->APPn, p->APP_len,
					p->COM_len, p->jpeg_markers);
		break;
	}
	case VIDIOC_S_JPEGCOMP:
	{
		struct v4l2_jpegcompression *p = arg;

		if (!ops->vidioc_g_jpegcomp)
			break;
		dbgarg(cmd, "quality=%d, APPn=%d, APP_len=%d, "
					"COM_len=%d, jpeg_markers=%d\n",
					p->quality, p->APPn, p->APP_len,
					p->COM_len, p->jpeg_markers);
			ret = ops->vidioc_s_jpegcomp(file, fh, p);
		break;
	}
	case VIDIOC_G_ENC_INDEX:
	{
		struct v4l2_enc_idx *p = arg;

		if (!ops->vidioc_g_enc_index)
			break;
		ret = ops->vidioc_g_enc_index(file, fh, p);
		if (!ret)
			dbgarg(cmd, "entries=%d, entries_cap=%d\n",
					p->entries, p->entries_cap);
		break;
	}
	case VIDIOC_ENCODER_CMD:
	{
		struct v4l2_encoder_cmd *p = arg;

		if (!ops->vidioc_encoder_cmd)
			break;
		ret = ops->vidioc_encoder_cmd(file, fh, p);
		if (!ret)
			dbgarg(cmd, "cmd=%d, flags=%x\n", p->cmd, p->flags);
		break;
	}
	case VIDIOC_TRY_ENCODER_CMD:
	{
		struct v4l2_encoder_cmd *p = arg;

		if (!ops->vidioc_try_encoder_cmd)
			break;
		ret = ops->vidioc_try_encoder_cmd(file, fh, p);
		if (!ret)
			dbgarg(cmd, "cmd=%d, flags=%x\n", p->cmd, p->flags);
		break;
	}
	case VIDIOC_G_PARM:
	{
		struct v4l2_streamparm *p = arg;

		if (ops->vidioc_g_parm) {
			ret = check_fmt(ops, p->type);
			if (ret)
				break;
			ret = ops->vidioc_g_parm(file, fh, p);
		} else {
			v4l2_std_id std = vfd->current_norm;

			if (p->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
				break;

			ret = 0;
			if (ops->vidioc_g_std)
				ret = ops->vidioc_g_std(file, fh, &std);
			else if (std == 0)
				ret = -EINVAL;
			if (ret == 0)
				v4l2_video_std_frame_period(std,
						    &p->parm.capture.timeperframe);
		}

		dbgarg(cmd, "type=%d\n", p->type);
		break;
	}
	case VIDIOC_S_PARM:
	{
		struct v4l2_streamparm *p = arg;

		if (!ops->vidioc_s_parm)
			break;
		ret = check_fmt(ops, p->type);
		if (ret)
			break;

		dbgarg(cmd, "type=%d\n", p->type);
		ret = ops->vidioc_s_parm(file, fh, p);
		break;
	}
	case VIDIOC_G_TUNER:
	{
		struct v4l2_tuner *p = arg;

		if (!ops->vidioc_g_tuner)
			break;

		p->type = (vfd->vfl_type == VFL_TYPE_RADIO) ?
			V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
		ret = ops->vidioc_g_tuner(file, fh, p);
		if (!ret)
			dbgarg(cmd, "index=%d, name=%s, type=%d, "
					"capability=0x%x, rangelow=%d, "
					"rangehigh=%d, signal=%d, afc=%d, "
					"rxsubchans=0x%x, audmode=%d\n",
					p->index, p->name, p->type,
					p->capability, p->rangelow,
					p->rangehigh, p->signal, p->afc,
					p->rxsubchans, p->audmode);
		break;
	}
	case VIDIOC_S_TUNER:
	{
		struct v4l2_tuner *p = arg;

		if (!ops->vidioc_s_tuner)
			break;
		p->type = (vfd->vfl_type == VFL_TYPE_RADIO) ?
			V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
		dbgarg(cmd, "index=%d, name=%s, type=%d, "
				"capability=0x%x, rangelow=%d, "
				"rangehigh=%d, signal=%d, afc=%d, "
				"rxsubchans=0x%x, audmode=%d\n",
				p->index, p->name, p->type,
				p->capability, p->rangelow,
				p->rangehigh, p->signal, p->afc,
				p->rxsubchans, p->audmode);
		ret = ops->vidioc_s_tuner(file, fh, p);
		break;
	}
	case VIDIOC_G_FREQUENCY:
	{
		struct v4l2_frequency *p = arg;

		if (!ops->vidioc_g_frequency)
			break;

		p->type = (vfd->vfl_type == VFL_TYPE_RADIO) ?
			V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
		ret = ops->vidioc_g_frequency(file, fh, p);
		if (!ret)
			dbgarg(cmd, "tuner=%d, type=%d, frequency=%d\n",
					p->tuner, p->type, p->frequency);
		break;
	}
	case VIDIOC_S_FREQUENCY:
	{
		struct v4l2_frequency *p = arg;

		if (!ops->vidioc_s_frequency)
			break;
		dbgarg(cmd, "tuner=%d, type=%d, frequency=%d\n",
				p->tuner, p->type, p->frequency);
		ret = ops->vidioc_s_frequency(file, fh, p);
		break;
	}
	case VIDIOC_G_SLICED_VBI_CAP:
	{
		struct v4l2_sliced_vbi_cap *p = arg;

		if (!ops->vidioc_g_sliced_vbi_cap)
			break;

		/* Clear up to type, everything after type is zerod already */
		memset(p, 0, offsetof(struct v4l2_sliced_vbi_cap, type));

		dbgarg(cmd, "type=%s\n", prt_names(p->type, v4l2_type_names));
		ret = ops->vidioc_g_sliced_vbi_cap(file, fh, p);
		if (!ret)
			dbgarg2("service_set=%d\n", p->service_set);
		break;
	}
	case VIDIOC_LOG_STATUS:
	{
		if (!ops->vidioc_log_status)
			break;
		ret = ops->vidioc_log_status(file, fh);
		break;
	}
#ifdef CONFIG_VIDEO_ADV_DEBUG
	case VIDIOC_DBG_G_REGISTER:
	{
		struct v4l2_dbg_register *p = arg;

		if (ops->vidioc_g_register) {
			if (!capable(CAP_SYS_ADMIN))
				ret = -EPERM;
			else
				ret = ops->vidioc_g_register(file, fh, p);
		}
		break;
	}
	case VIDIOC_DBG_S_REGISTER:
	{
		struct v4l2_dbg_register *p = arg;

		if (ops->vidioc_s_register) {
			if (!capable(CAP_SYS_ADMIN))
				ret = -EPERM;
			else
				ret = ops->vidioc_s_register(file, fh, p);
		}
		break;
	}
#endif
	case VIDIOC_DBG_G_CHIP_IDENT:
	{
		struct v4l2_dbg_chip_ident *p = arg;

		if (!ops->vidioc_g_chip_ident)
			break;
		p->ident = V4L2_IDENT_NONE;
		p->revision = 0;
		ret = ops->vidioc_g_chip_ident(file, fh, p);
		if (!ret)
			dbgarg(cmd, "chip_ident=%u, revision=0x%x\n", p->ident, p->revision);
		break;
	}
	case VIDIOC_S_HW_FREQ_SEEK:
	{
		struct v4l2_hw_freq_seek *p = arg;

		if (!ops->vidioc_s_hw_freq_seek)
			break;
		dbgarg(cmd,
			"tuner=%d, type=%d, seek_upward=%d, wrap_around=%d\n",
			p->tuner, p->type, p->seek_upward, p->wrap_around);
		ret = ops->vidioc_s_hw_freq_seek(file, fh, p);
		break;
	}
	case VIDIOC_ENUM_FRAMESIZES:
	{
		struct v4l2_frmsizeenum *p = arg;

		if (!ops->vidioc_enum_framesizes)
			break;

		ret = ops->vidioc_enum_framesizes(file, fh, p);
		dbgarg(cmd,
			"index=%d, pixelformat=%c%c%c%c, type=%d ",
			p->index,
			(p->pixel_format & 0xff),
			(p->pixel_format >>  8) & 0xff,
			(p->pixel_format >> 16) & 0xff,
			(p->pixel_format >> 24) & 0xff,
			p->type);
		switch (p->type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
			dbgarg3("width = %d, height=%d\n",
				p->discrete.width, p->discrete.height);
			break;
		case V4L2_FRMSIZE_TYPE_STEPWISE:
			dbgarg3("min %dx%d, max %dx%d, step %dx%d\n",
				p->stepwise.min_width,  p->stepwise.min_height,
				p->stepwise.step_width, p->stepwise.step_height,
				p->stepwise.max_width,  p->stepwise.max_height);
			break;
		case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			dbgarg3("continuous\n");
			break;
		default:
			dbgarg3("- Unknown type!\n");
		}

		break;
	}
	case VIDIOC_ENUM_FRAMEINTERVALS:
	{
		struct v4l2_frmivalenum *p = arg;

		if (!ops->vidioc_enum_frameintervals)
			break;

		ret = ops->vidioc_enum_frameintervals(file, fh, p);
		dbgarg(cmd,
			"index=%d, pixelformat=%d, width=%d, height=%d, type=%d ",
			p->index, p->pixel_format,
			p->width, p->height, p->type);
		switch (p->type) {
		case V4L2_FRMIVAL_TYPE_DISCRETE:
			dbgarg2("fps=%d/%d\n",
				p->discrete.numerator,
				p->discrete.denominator);
			break;
		case V4L2_FRMIVAL_TYPE_STEPWISE:
			dbgarg2("min=%d/%d, max=%d/%d, step=%d/%d\n",
				p->stepwise.min.numerator,
				p->stepwise.min.denominator,
				p->stepwise.max.numerator,
				p->stepwise.max.denominator,
				p->stepwise.step.numerator,
				p->stepwise.step.denominator);
			break;
		case V4L2_FRMIVAL_TYPE_CONTINUOUS:
			dbgarg2("continuous\n");
			break;
		default:
			dbgarg2("- Unknown type!\n");
		}
		break;
	}
	case VIDIOC_ENUM_DV_PRESETS:
	{
		struct v4l2_dv_enum_preset *p = arg;

		if (!ops->vidioc_enum_dv_presets)
			break;

		ret = ops->vidioc_enum_dv_presets(file, fh, p);
		if (!ret)
			dbgarg(cmd,
				"index=%d, preset=%d, name=%s, width=%d,"
				" height=%d ",
				p->index, p->preset, p->name, p->width,
				p->height);
		break;
	}
	case VIDIOC_S_DV_PRESET:
	{
		struct v4l2_dv_preset *p = arg;

		if (!ops->vidioc_s_dv_preset)
			break;

		dbgarg(cmd, "preset=%d\n", p->preset);
		ret = ops->vidioc_s_dv_preset(file, fh, p);
		break;
	}
	case VIDIOC_G_DV_PRESET:
	{
		struct v4l2_dv_preset *p = arg;

		if (!ops->vidioc_g_dv_preset)
			break;

		ret = ops->vidioc_g_dv_preset(file, fh, p);
		if (!ret)
			dbgarg(cmd, "preset=%d\n", p->preset);
		break;
	}
	case VIDIOC_QUERY_DV_PRESET:
	{
		struct v4l2_dv_preset *p = arg;

		if (!ops->vidioc_query_dv_preset)
			break;

		ret = ops->vidioc_query_dv_preset(file, fh, p);
		if (!ret)
			dbgarg(cmd, "preset=%d\n", p->preset);
		break;
	}
	case VIDIOC_S_DV_TIMINGS:
	{
		struct v4l2_dv_timings *p = arg;

		if (!ops->vidioc_s_dv_timings)
			break;

		switch (p->type) {
		case V4L2_DV_BT_656_1120:
			dbgarg2("bt-656/1120:interlaced=%d, pixelclock=%lld,"
				" width=%d, height=%d, polarities=%x,"
				" hfrontporch=%d, hsync=%d, hbackporch=%d,"
				" vfrontporch=%d, vsync=%d, vbackporch=%d,"
				" il_vfrontporch=%d, il_vsync=%d,"
				" il_vbackporch=%d\n",
				p->bt.interlaced, p->bt.pixelclock,
				p->bt.width, p->bt.height, p->bt.polarities,
				p->bt.hfrontporch, p->bt.hsync,
				p->bt.hbackporch, p->bt.vfrontporch,
				p->bt.vsync, p->bt.vbackporch,
				p->bt.il_vfrontporch, p->bt.il_vsync,
				p->bt.il_vbackporch);
			ret = ops->vidioc_s_dv_timings(file, fh, p);
			break;
		default:
			dbgarg2("Unknown type %d!\n", p->type);
			break;
		}
		break;
	}
	case VIDIOC_G_DV_TIMINGS:
	{
		struct v4l2_dv_timings *p = arg;

		if (!ops->vidioc_g_dv_timings)
			break;

		ret = ops->vidioc_g_dv_timings(file, fh, p);
		if (!ret) {
			switch (p->type) {
			case V4L2_DV_BT_656_1120:
				dbgarg2("bt-656/1120:interlaced=%d,"
					" pixelclock=%lld,"
					" width=%d, height=%d, polarities=%x,"
					" hfrontporch=%d, hsync=%d,"
					" hbackporch=%d, vfrontporch=%d,"
					" vsync=%d, vbackporch=%d,"
					" il_vfrontporch=%d, il_vsync=%d,"
					" il_vbackporch=%d\n",
					p->bt.interlaced, p->bt.pixelclock,
					p->bt.width, p->bt.height,
					p->bt.polarities, p->bt.hfrontporch,
					p->bt.hsync, p->bt.hbackporch,
					p->bt.vfrontporch, p->bt.vsync,
					p->bt.vbackporch, p->bt.il_vfrontporch,
					p->bt.il_vsync, p->bt.il_vbackporch);
				break;
			default:
				dbgarg2("Unknown type %d!\n", p->type);
				break;
			}
		}
		break;
	}
	case VIDIOC_DQEVENT:
	{
		struct v4l2_event *ev = arg;

		if (!ops->vidioc_subscribe_event)
			break;

		ret = v4l2_event_dequeue(fh, ev, file->f_flags & O_NONBLOCK);
		if (ret < 0) {
			dbgarg(cmd, "no pending events?");
			break;
		}
		dbgarg(cmd,
		       "pending=%d, type=0x%8.8x, sequence=%d, "
		       "timestamp=%lu.%9.9lu ",
		       ev->pending, ev->type, ev->sequence,
		       ev->timestamp.tv_sec, ev->timestamp.tv_nsec);
		break;
	}
	case VIDIOC_SUBSCRIBE_EVENT:
	{
		struct v4l2_event_subscription *sub = arg;

		if (!ops->vidioc_subscribe_event)
			break;

		ret = ops->vidioc_subscribe_event(fh, sub);
		if (ret < 0) {
			dbgarg(cmd, "failed, ret=%ld", ret);
			break;
		}
		dbgarg(cmd, "type=0x%8.8x", sub->type);
		break;
	}
	case VIDIOC_UNSUBSCRIBE_EVENT:
	{
		struct v4l2_event_subscription *sub = arg;

		if (!ops->vidioc_unsubscribe_event)
			break;

		ret = ops->vidioc_unsubscribe_event(fh, sub);
		if (ret < 0) {
			dbgarg(cmd, "failed, ret=%ld", ret);
			break;
		}
		dbgarg(cmd, "type=0x%8.8x", sub->type);
		break;
	}
	default:
	{
		bool valid_prio = true;

		if (!ops->vidioc_default)
			break;
		if (use_fh_prio)
			valid_prio = v4l2_prio_check(vfd->prio, vfh->prio) >= 0;
		ret = ops->vidioc_default(file, fh, valid_prio, cmd, arg);
		break;
	}
	} /* switch */

exit_prio:
	if (vfd->debug & V4L2_DEBUG_IOCTL_ARG) {
		if (ret < 0) {
			v4l_print_ioctl(vfd->name, cmd);
			printk(KERN_CONT " error %ld\n", ret);
		}
	}

	return ret;
}

/* In some cases, only a few fields are used as input, i.e. when the app sets
 * "index" and then the driver fills in the rest of the structure for the thing
 * with that index.  We only need to copy up the first non-input field.  */
static unsigned long cmd_input_size(unsigned int cmd)
{
	/* Size of structure up to and including 'field' */
#define CMDINSIZE(cmd, type, field) 				\
	case VIDIOC_##cmd: 					\
		return offsetof(struct v4l2_##type, field) + 	\
			sizeof(((struct v4l2_##type *)0)->field);

	switch (cmd) {
		CMDINSIZE(ENUM_FMT,		fmtdesc,	type);
		CMDINSIZE(G_FMT,		format,		type);
		CMDINSIZE(QUERYBUF,		buffer,		length);
		CMDINSIZE(G_PARM,		streamparm,	type);
		CMDINSIZE(ENUMSTD,		standard,	index);
		CMDINSIZE(ENUMINPUT,		input,		index);
		CMDINSIZE(G_CTRL,		control,	id);
		CMDINSIZE(G_TUNER,		tuner,		index);
		CMDINSIZE(QUERYCTRL,		queryctrl,	id);
		CMDINSIZE(QUERYMENU,		querymenu,	index);
		CMDINSIZE(ENUMOUTPUT,		output,		index);
		CMDINSIZE(G_MODULATOR,		modulator,	index);
		CMDINSIZE(G_FREQUENCY,		frequency,	tuner);
		CMDINSIZE(CROPCAP,		cropcap,	type);
		CMDINSIZE(G_CROP,		crop,		type);
		CMDINSIZE(ENUMAUDIO,		audio, 		index);
		CMDINSIZE(ENUMAUDOUT,		audioout, 	index);
		CMDINSIZE(ENCODER_CMD,		encoder_cmd,	flags);
		CMDINSIZE(TRY_ENCODER_CMD,	encoder_cmd,	flags);
		CMDINSIZE(G_SLICED_VBI_CAP,	sliced_vbi_cap,	type);
		CMDINSIZE(ENUM_FRAMESIZES,	frmsizeenum,	pixel_format);
		CMDINSIZE(ENUM_FRAMEINTERVALS,	frmivalenum,	height);
	default:
		return _IOC_SIZE(cmd);
	}
}

static int check_array_args(unsigned int cmd, void *parg, size_t *array_size,
			    void * __user *user_ptr, void ***kernel_ptr)
{
	int ret = 0;

	switch (cmd) {
	case VIDIOC_QUERYBUF:
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF: {
		struct v4l2_buffer *buf = parg;

		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type) && buf->length > 0) {
			if (buf->length > VIDEO_MAX_PLANES) {
				ret = -EINVAL;
				break;
			}
			*user_ptr = (void __user *)buf->m.planes;
			*kernel_ptr = (void **)&buf->m.planes;
			*array_size = sizeof(struct v4l2_plane) * buf->length;
			ret = 1;
		}
		break;
	}

	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS: {
		struct v4l2_ext_controls *ctrls = parg;

		if (ctrls->count != 0) {
			*user_ptr = (void __user *)ctrls->controls;
			*kernel_ptr = (void **)&ctrls->controls;
			*array_size = sizeof(struct v4l2_ext_control)
				    * ctrls->count;
			ret = 1;
		}
		break;
	}
	}

	return ret;
}

long
video_usercopy(struct file *file, unsigned int cmd, unsigned long arg,
	       v4l2_kioctl func)
{
	char	sbuf[128];
	void    *mbuf = NULL;
	void	*parg = (void *)arg;
	long	err  = -EINVAL;
	bool	has_array_args;
	size_t  array_size = 0;
	void __user *user_ptr = NULL;
	void	**kernel_ptr = NULL;

	/*  Copy arguments into temp kernel buffer  */
	if (_IOC_DIR(cmd) != _IOC_NONE) {
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			unsigned long n = cmd_input_size(cmd);

			if (copy_from_user(parg, (void __user *)arg, n))
				goto out;

			/* zero out anything we don't copy from userspace */
			if (n < _IOC_SIZE(cmd))
				memset((u8 *)parg + n, 0, _IOC_SIZE(cmd) - n);
		} else {
			/* read-only ioctl */
			memset(parg, 0, _IOC_SIZE(cmd));
		}
	}

	err = check_array_args(cmd, parg, &array_size, &user_ptr, &kernel_ptr);
	if (err < 0)
		goto out;
	has_array_args = err;

	if (has_array_args) {
		/*
		 * When adding new types of array args, make sure that the
		 * parent argument to ioctl (which contains the pointer to the
		 * array) fits into sbuf (so that mbuf will still remain
		 * unused up to here).
		 */
		mbuf = kmalloc(array_size, GFP_KERNEL);
		err = -ENOMEM;
		if (NULL == mbuf)
			goto out_array_args;
		err = -EFAULT;
		if (copy_from_user(mbuf, user_ptr, array_size))
			goto out_array_args;
		*kernel_ptr = mbuf;
	}

	/* Handles IOCTL */
	err = func(file, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -EINVAL;

	if (has_array_args) {
		*kernel_ptr = user_ptr;
		if (copy_to_user(user_ptr, mbuf, array_size))
			err = -EFAULT;
		goto out_array_args;
	}
	if (err < 0)
		goto out;

out_array_args:
	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void __user *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	kfree(mbuf);
	return err;
}
EXPORT_SYMBOL(video_usercopy);

long video_ioctl2(struct file *file,
	       unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, __video_do_ioctl);
}
EXPORT_SYMBOL(video_ioctl2);
