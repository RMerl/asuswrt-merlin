/*
 * ioctl32.c: Conversion between 32bit and 64bit native ioctls.
 *	Separated from fs stuff by Arnd Bergmann <arnd@arndb.de>
 *
 * Copyright (C) 1997-2000  Jakub Jelinek  (jakub@redhat.com)
 * Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 * Copyright (C) 2001,2002  Andi Kleen, SuSE Labs
 * Copyright (C) 2003       Pavel Machek (pavel@ucw.cz)
 * Copyright (C) 2005       Philippe De Muyter (phdm@macqel.be)
 * Copyright (C) 2008       Hans Verkuil <hverkuil@xs4all.nl>
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * ioctls.
 */

#include <linux/compat.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <media/v4l2-ioctl.h>

#ifdef CONFIG_COMPAT

static long native_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	if (file->f_op->unlocked_ioctl)
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);

	return ret;
}


struct v4l2_clip32 {
	struct v4l2_rect        c;
	compat_caddr_t 		next;
};

struct v4l2_window32 {
	struct v4l2_rect        w;
	enum v4l2_field  	field;
	__u32			chromakey;
	compat_caddr_t		clips; /* actually struct v4l2_clip32 * */
	__u32			clipcount;
	compat_caddr_t		bitmap;
};

static int get_v4l2_window32(struct v4l2_window *kp, struct v4l2_window32 __user *up)
{
	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_window32)) ||
		copy_from_user(&kp->w, &up->w, sizeof(up->w)) ||
		get_user(kp->field, &up->field) ||
		get_user(kp->chromakey, &up->chromakey) ||
		get_user(kp->clipcount, &up->clipcount))
			return -EFAULT;
	if (kp->clipcount > 2048)
		return -EINVAL;
	if (kp->clipcount) {
		struct v4l2_clip32 __user *uclips;
		struct v4l2_clip __user *kclips;
		int n = kp->clipcount;
		compat_caddr_t p;

		if (get_user(p, &up->clips))
			return -EFAULT;
		uclips = compat_ptr(p);
		kclips = compat_alloc_user_space(n * sizeof(struct v4l2_clip));
		kp->clips = kclips;
		while (--n >= 0) {
			if (copy_in_user(&kclips->c, &uclips->c, sizeof(uclips->c)))
				return -EFAULT;
			if (put_user(n ? kclips + 1 : NULL, &kclips->next))
				return -EFAULT;
			uclips += 1;
			kclips += 1;
		}
	} else
		kp->clips = NULL;
	return 0;
}

static int put_v4l2_window32(struct v4l2_window *kp, struct v4l2_window32 __user *up)
{
	if (copy_to_user(&up->w, &kp->w, sizeof(kp->w)) ||
		put_user(kp->field, &up->field) ||
		put_user(kp->chromakey, &up->chromakey) ||
		put_user(kp->clipcount, &up->clipcount))
			return -EFAULT;
	return 0;
}

static inline int get_v4l2_pix_format(struct v4l2_pix_format *kp, struct v4l2_pix_format __user *up)
{
	if (copy_from_user(kp, up, sizeof(struct v4l2_pix_format)))
		return -EFAULT;
	return 0;
}

static inline int get_v4l2_pix_format_mplane(struct v4l2_pix_format_mplane *kp,
				struct v4l2_pix_format_mplane __user *up)
{
	if (copy_from_user(kp, up, sizeof(struct v4l2_pix_format_mplane)))
		return -EFAULT;
	return 0;
}

static inline int put_v4l2_pix_format(struct v4l2_pix_format *kp, struct v4l2_pix_format __user *up)
{
	if (copy_to_user(up, kp, sizeof(struct v4l2_pix_format)))
		return -EFAULT;
	return 0;
}

static inline int put_v4l2_pix_format_mplane(struct v4l2_pix_format_mplane *kp,
				struct v4l2_pix_format_mplane __user *up)
{
	if (copy_to_user(up, kp, sizeof(struct v4l2_pix_format_mplane)))
		return -EFAULT;
	return 0;
}

static inline int get_v4l2_vbi_format(struct v4l2_vbi_format *kp, struct v4l2_vbi_format __user *up)
{
	if (copy_from_user(kp, up, sizeof(struct v4l2_vbi_format)))
		return -EFAULT;
	return 0;
}

static inline int put_v4l2_vbi_format(struct v4l2_vbi_format *kp, struct v4l2_vbi_format __user *up)
{
	if (copy_to_user(up, kp, sizeof(struct v4l2_vbi_format)))
		return -EFAULT;
	return 0;
}

static inline int get_v4l2_sliced_vbi_format(struct v4l2_sliced_vbi_format *kp, struct v4l2_sliced_vbi_format __user *up)
{
	if (copy_from_user(kp, up, sizeof(struct v4l2_sliced_vbi_format)))
		return -EFAULT;
	return 0;
}

static inline int put_v4l2_sliced_vbi_format(struct v4l2_sliced_vbi_format *kp, struct v4l2_sliced_vbi_format __user *up)
{
	if (copy_to_user(up, kp, sizeof(struct v4l2_sliced_vbi_format)))
		return -EFAULT;
	return 0;
}

struct v4l2_format32 {
	enum v4l2_buf_type type;
	union {
		struct v4l2_pix_format	pix;
		struct v4l2_pix_format_mplane	pix_mp;
		struct v4l2_window32	win;
		struct v4l2_vbi_format	vbi;
		struct v4l2_sliced_vbi_format	sliced;
		__u8	raw_data[200];        /* user-defined */
	} fmt;
};

static int get_v4l2_format32(struct v4l2_format *kp, struct v4l2_format32 __user *up)
{
	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_format32)) ||
			get_user(kp->type, &up->type))
			return -EFAULT;
	switch (kp->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return get_v4l2_pix_format(&kp->fmt.pix, &up->fmt.pix);
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return get_v4l2_pix_format_mplane(&kp->fmt.pix_mp,
						  &up->fmt.pix_mp);
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return get_v4l2_window32(&kp->fmt.win, &up->fmt.win);
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return get_v4l2_vbi_format(&kp->fmt.vbi, &up->fmt.vbi);
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return get_v4l2_sliced_vbi_format(&kp->fmt.sliced, &up->fmt.sliced);
	case V4L2_BUF_TYPE_PRIVATE:
		if (copy_from_user(kp, up, sizeof(kp->fmt.raw_data)))
			return -EFAULT;
		return 0;
	default:
		printk(KERN_INFO "compat_ioctl32: unexpected VIDIOC_FMT type %d\n",
								kp->type);
		return -EINVAL;
	}
}

static int put_v4l2_format32(struct v4l2_format *kp, struct v4l2_format32 __user *up)
{
	if (!access_ok(VERIFY_WRITE, up, sizeof(struct v4l2_format32)) ||
		put_user(kp->type, &up->type))
		return -EFAULT;
	switch (kp->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return put_v4l2_pix_format(&kp->fmt.pix, &up->fmt.pix);
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return put_v4l2_pix_format_mplane(&kp->fmt.pix_mp,
						  &up->fmt.pix_mp);
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return put_v4l2_window32(&kp->fmt.win, &up->fmt.win);
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return put_v4l2_vbi_format(&kp->fmt.vbi, &up->fmt.vbi);
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return put_v4l2_sliced_vbi_format(&kp->fmt.sliced, &up->fmt.sliced);
	case V4L2_BUF_TYPE_PRIVATE:
		if (copy_to_user(up, kp, sizeof(up->fmt.raw_data)))
			return -EFAULT;
		return 0;
	default:
		printk(KERN_INFO "compat_ioctl32: unexpected VIDIOC_FMT type %d\n",
								kp->type);
		return -EINVAL;
	}
}

struct v4l2_standard32 {
	__u32		     index;
	__u32		     id[2]; /* __u64 would get the alignment wrong */
	__u8		     name[24];
	struct v4l2_fract    frameperiod; /* Frames, not fields */
	__u32		     framelines;
	__u32		     reserved[4];
};

static int get_v4l2_standard32(struct v4l2_standard *kp, struct v4l2_standard32 __user *up)
{
	/* other fields are not set by the user, nor used by the driver */
	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_standard32)) ||
		get_user(kp->index, &up->index))
		return -EFAULT;
	return 0;
}

static int put_v4l2_standard32(struct v4l2_standard *kp, struct v4l2_standard32 __user *up)
{
	if (!access_ok(VERIFY_WRITE, up, sizeof(struct v4l2_standard32)) ||
		put_user(kp->index, &up->index) ||
		copy_to_user(up->id, &kp->id, sizeof(__u64)) ||
		copy_to_user(up->name, kp->name, 24) ||
		copy_to_user(&up->frameperiod, &kp->frameperiod, sizeof(kp->frameperiod)) ||
		put_user(kp->framelines, &up->framelines) ||
		copy_to_user(up->reserved, kp->reserved, 4 * sizeof(__u32)))
			return -EFAULT;
	return 0;
}

struct v4l2_plane32 {
	__u32			bytesused;
	__u32			length;
	union {
		__u32		mem_offset;
		compat_long_t	userptr;
	} m;
	__u32			data_offset;
	__u32			reserved[11];
};

struct v4l2_buffer32 {
	__u32			index;
	enum v4l2_buf_type      type;
	__u32			bytesused;
	__u32			flags;
	enum v4l2_field		field;
	struct compat_timeval	timestamp;
	struct v4l2_timecode	timecode;
	__u32			sequence;

	/* memory location */
	enum v4l2_memory        memory;
	union {
		__u32           offset;
		compat_long_t   userptr;
		compat_caddr_t  planes;
	} m;
	__u32			length;
	__u32			input;
	__u32			reserved;
};

static int get_v4l2_plane32(struct v4l2_plane *up, struct v4l2_plane32 *up32,
				enum v4l2_memory memory)
{
	void __user *up_pln;
	compat_long_t p;

	if (copy_in_user(up, up32, 2 * sizeof(__u32)) ||
		copy_in_user(&up->data_offset, &up32->data_offset,
				sizeof(__u32)))
		return -EFAULT;

	if (memory == V4L2_MEMORY_USERPTR) {
		if (get_user(p, &up32->m.userptr))
			return -EFAULT;
		up_pln = compat_ptr(p);
		if (put_user((unsigned long)up_pln, &up->m.userptr))
			return -EFAULT;
	} else {
		if (copy_in_user(&up->m.mem_offset, &up32->m.mem_offset,
					sizeof(__u32)))
			return -EFAULT;
	}

	return 0;
}

static int put_v4l2_plane32(struct v4l2_plane *up, struct v4l2_plane32 *up32,
				enum v4l2_memory memory)
{
	if (copy_in_user(up32, up, 2 * sizeof(__u32)) ||
		copy_in_user(&up32->data_offset, &up->data_offset,
				sizeof(__u32)))
		return -EFAULT;

	/* For MMAP, driver might've set up the offset, so copy it back.
	 * USERPTR stays the same (was userspace-provided), so no copying. */
	if (memory == V4L2_MEMORY_MMAP)
		if (copy_in_user(&up32->m.mem_offset, &up->m.mem_offset,
					sizeof(__u32)))
			return -EFAULT;

	return 0;
}

static int get_v4l2_buffer32(struct v4l2_buffer *kp, struct v4l2_buffer32 __user *up)
{
	struct v4l2_plane32 __user *uplane32;
	struct v4l2_plane __user *uplane;
	compat_caddr_t p;
	int num_planes;
	int ret;

	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_buffer32)) ||
		get_user(kp->index, &up->index) ||
		get_user(kp->type, &up->type) ||
		get_user(kp->flags, &up->flags) ||
		get_user(kp->memory, &up->memory) ||
		get_user(kp->input, &up->input))
			return -EFAULT;

	if (V4L2_TYPE_IS_OUTPUT(kp->type))
		if (get_user(kp->bytesused, &up->bytesused) ||
			get_user(kp->field, &up->field) ||
			get_user(kp->timestamp.tv_sec, &up->timestamp.tv_sec) ||
			get_user(kp->timestamp.tv_usec,
					&up->timestamp.tv_usec))
			return -EFAULT;

	if (V4L2_TYPE_IS_MULTIPLANAR(kp->type)) {
		if (get_user(kp->length, &up->length))
			return -EFAULT;

		num_planes = kp->length;
		if (num_planes == 0) {
			kp->m.planes = NULL;
			/* num_planes == 0 is legal, e.g. when userspace doesn't
			 * need planes array on DQBUF*/
			return 0;
		}

		if (get_user(p, &up->m.planes))
			return -EFAULT;

		uplane32 = compat_ptr(p);
		if (!access_ok(VERIFY_READ, uplane32,
				num_planes * sizeof(struct v4l2_plane32)))
			return -EFAULT;

		/* We don't really care if userspace decides to kill itself
		 * by passing a very big num_planes value */
		uplane = compat_alloc_user_space(num_planes *
						sizeof(struct v4l2_plane));
		kp->m.planes = uplane;

		while (--num_planes >= 0) {
			ret = get_v4l2_plane32(uplane, uplane32, kp->memory);
			if (ret)
				return ret;
			++uplane;
			++uplane32;
		}
	} else {
		switch (kp->memory) {
		case V4L2_MEMORY_MMAP:
			if (get_user(kp->length, &up->length) ||
				get_user(kp->m.offset, &up->m.offset))
				return -EFAULT;
			break;
		case V4L2_MEMORY_USERPTR:
			{
			compat_long_t tmp;

			if (get_user(kp->length, &up->length) ||
			    get_user(tmp, &up->m.userptr))
				return -EFAULT;

			kp->m.userptr = (unsigned long)compat_ptr(tmp);
			}
			break;
		case V4L2_MEMORY_OVERLAY:
			if (get_user(kp->m.offset, &up->m.offset))
				return -EFAULT;
			break;
		}
	}

	return 0;
}

static int put_v4l2_buffer32(struct v4l2_buffer *kp, struct v4l2_buffer32 __user *up)
{
	struct v4l2_plane32 __user *uplane32;
	struct v4l2_plane __user *uplane;
	compat_caddr_t p;
	int num_planes;
	int ret;

	if (!access_ok(VERIFY_WRITE, up, sizeof(struct v4l2_buffer32)) ||
		put_user(kp->index, &up->index) ||
		put_user(kp->type, &up->type) ||
		put_user(kp->flags, &up->flags) ||
		put_user(kp->memory, &up->memory) ||
		put_user(kp->input, &up->input))
			return -EFAULT;

	if (put_user(kp->bytesused, &up->bytesused) ||
		put_user(kp->field, &up->field) ||
		put_user(kp->timestamp.tv_sec, &up->timestamp.tv_sec) ||
		put_user(kp->timestamp.tv_usec, &up->timestamp.tv_usec) ||
		copy_to_user(&up->timecode, &kp->timecode, sizeof(struct v4l2_timecode)) ||
		put_user(kp->sequence, &up->sequence) ||
		put_user(kp->reserved, &up->reserved))
			return -EFAULT;

	if (V4L2_TYPE_IS_MULTIPLANAR(kp->type)) {
		num_planes = kp->length;
		if (num_planes == 0)
			return 0;

		uplane = kp->m.planes;
		if (get_user(p, &up->m.planes))
			return -EFAULT;
		uplane32 = compat_ptr(p);

		while (--num_planes >= 0) {
			ret = put_v4l2_plane32(uplane, uplane32, kp->memory);
			if (ret)
				return ret;
			++uplane;
			++uplane32;
		}
	} else {
		switch (kp->memory) {
		case V4L2_MEMORY_MMAP:
			if (put_user(kp->length, &up->length) ||
				put_user(kp->m.offset, &up->m.offset))
				return -EFAULT;
			break;
		case V4L2_MEMORY_USERPTR:
			if (put_user(kp->length, &up->length) ||
				put_user(kp->m.userptr, &up->m.userptr))
				return -EFAULT;
			break;
		case V4L2_MEMORY_OVERLAY:
			if (put_user(kp->m.offset, &up->m.offset))
				return -EFAULT;
			break;
		}
	}

	return 0;
}

struct v4l2_framebuffer32 {
	__u32			capability;
	__u32			flags;
	compat_caddr_t 		base;
	struct v4l2_pix_format	fmt;
};

static int get_v4l2_framebuffer32(struct v4l2_framebuffer *kp, struct v4l2_framebuffer32 __user *up)
{
	u32 tmp;

	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_framebuffer32)) ||
		get_user(tmp, &up->base) ||
		get_user(kp->capability, &up->capability) ||
		get_user(kp->flags, &up->flags))
			return -EFAULT;
	kp->base = compat_ptr(tmp);
	get_v4l2_pix_format(&kp->fmt, &up->fmt);
	return 0;
}

static int put_v4l2_framebuffer32(struct v4l2_framebuffer *kp, struct v4l2_framebuffer32 __user *up)
{
	u32 tmp = (u32)((unsigned long)kp->base);

	if (!access_ok(VERIFY_WRITE, up, sizeof(struct v4l2_framebuffer32)) ||
		put_user(tmp, &up->base) ||
		put_user(kp->capability, &up->capability) ||
		put_user(kp->flags, &up->flags))
			return -EFAULT;
	put_v4l2_pix_format(&kp->fmt, &up->fmt);
	return 0;
}

struct v4l2_input32 {
	__u32	     index;		/*  Which input */
	__u8	     name[32];		/*  Label */
	__u32	     type;		/*  Type of input */
	__u32	     audioset;		/*  Associated audios (bitfield) */
	__u32        tuner;             /*  Associated tuner */
	v4l2_std_id  std;
	__u32	     status;
	__u32	     reserved[4];
} __attribute__ ((packed));

/* The 64-bit v4l2_input struct has extra padding at the end of the struct.
   Otherwise it is identical to the 32-bit version. */
static inline int get_v4l2_input32(struct v4l2_input *kp, struct v4l2_input32 __user *up)
{
	if (copy_from_user(kp, up, sizeof(struct v4l2_input32)))
		return -EFAULT;
	return 0;
}

static inline int put_v4l2_input32(struct v4l2_input *kp, struct v4l2_input32 __user *up)
{
	if (copy_to_user(up, kp, sizeof(struct v4l2_input32)))
		return -EFAULT;
	return 0;
}

struct v4l2_ext_controls32 {
       __u32 ctrl_class;
       __u32 count;
       __u32 error_idx;
       __u32 reserved[2];
       compat_caddr_t controls; /* actually struct v4l2_ext_control32 * */
};

struct v4l2_ext_control32 {
	__u32 id;
	__u32 size;
	__u32 reserved2[1];
	union {
		__s32 value;
		__s64 value64;
		compat_caddr_t string; /* actually char * */
	};
} __attribute__ ((packed));

/* The following function really belong in v4l2-common, but that causes
   a circular dependency between modules. We need to think about this, but
   for now this will do. */

/* Return non-zero if this control is a pointer type. Currently only
   type STRING is a pointer type. */
static inline int ctrl_is_pointer(u32 id)
{
	switch (id) {
	case V4L2_CID_RDS_TX_PS_NAME:
	case V4L2_CID_RDS_TX_RADIO_TEXT:
		return 1;
	default:
		return 0;
	}
}

static int get_v4l2_ext_controls32(struct v4l2_ext_controls *kp, struct v4l2_ext_controls32 __user *up)
{
	struct v4l2_ext_control32 __user *ucontrols;
	struct v4l2_ext_control __user *kcontrols;
	int n;
	compat_caddr_t p;

	if (!access_ok(VERIFY_READ, up, sizeof(struct v4l2_ext_controls32)) ||
		get_user(kp->ctrl_class, &up->ctrl_class) ||
		get_user(kp->count, &up->count) ||
		get_user(kp->error_idx, &up->error_idx) ||
		copy_from_user(kp->reserved, up->reserved, sizeof(kp->reserved)))
			return -EFAULT;
	n = kp->count;
	if (n == 0) {
		kp->controls = NULL;
		return 0;
	}
	if (get_user(p, &up->controls))
		return -EFAULT;
	ucontrols = compat_ptr(p);
	if (!access_ok(VERIFY_READ, ucontrols,
			n * sizeof(struct v4l2_ext_control32)))
		return -EFAULT;
	kcontrols = compat_alloc_user_space(n * sizeof(struct v4l2_ext_control));
	kp->controls = kcontrols;
	while (--n >= 0) {
		if (copy_in_user(kcontrols, ucontrols, sizeof(*ucontrols)))
			return -EFAULT;
		if (ctrl_is_pointer(kcontrols->id)) {
			void __user *s;

			if (get_user(p, &ucontrols->string))
				return -EFAULT;
			s = compat_ptr(p);
			if (put_user(s, &kcontrols->string))
				return -EFAULT;
		}
		ucontrols++;
		kcontrols++;
	}
	return 0;
}

static int put_v4l2_ext_controls32(struct v4l2_ext_controls *kp, struct v4l2_ext_controls32 __user *up)
{
	struct v4l2_ext_control32 __user *ucontrols;
	struct v4l2_ext_control __user *kcontrols = kp->controls;
	int n = kp->count;
	compat_caddr_t p;

	if (!access_ok(VERIFY_WRITE, up, sizeof(struct v4l2_ext_controls32)) ||
		put_user(kp->ctrl_class, &up->ctrl_class) ||
		put_user(kp->count, &up->count) ||
		put_user(kp->error_idx, &up->error_idx) ||
		copy_to_user(up->reserved, kp->reserved, sizeof(up->reserved)))
			return -EFAULT;
	if (!kp->count)
		return 0;

	if (get_user(p, &up->controls))
		return -EFAULT;
	ucontrols = compat_ptr(p);
	if (!access_ok(VERIFY_WRITE, ucontrols,
			n * sizeof(struct v4l2_ext_control32)))
		return -EFAULT;

	while (--n >= 0) {
		unsigned size = sizeof(*ucontrols);

		/* Do not modify the pointer when copying a pointer control.
		   The contents of the pointer was changed, not the pointer
		   itself. */
		if (ctrl_is_pointer(kcontrols->id))
			size -= sizeof(ucontrols->value64);
		if (copy_in_user(ucontrols, kcontrols, size))
			return -EFAULT;
		ucontrols++;
		kcontrols++;
	}
	return 0;
}

#define VIDIOC_G_FMT32		_IOWR('V',  4, struct v4l2_format32)
#define VIDIOC_S_FMT32		_IOWR('V',  5, struct v4l2_format32)
#define VIDIOC_QUERYBUF32	_IOWR('V',  9, struct v4l2_buffer32)
#define VIDIOC_G_FBUF32		_IOR ('V', 10, struct v4l2_framebuffer32)
#define VIDIOC_S_FBUF32		_IOW ('V', 11, struct v4l2_framebuffer32)
#define VIDIOC_QBUF32		_IOWR('V', 15, struct v4l2_buffer32)
#define VIDIOC_DQBUF32		_IOWR('V', 17, struct v4l2_buffer32)
#define VIDIOC_ENUMSTD32	_IOWR('V', 25, struct v4l2_standard32)
#define VIDIOC_ENUMINPUT32	_IOWR('V', 26, struct v4l2_input32)
#define VIDIOC_TRY_FMT32      	_IOWR('V', 64, struct v4l2_format32)
#define VIDIOC_G_EXT_CTRLS32    _IOWR('V', 71, struct v4l2_ext_controls32)
#define VIDIOC_S_EXT_CTRLS32    _IOWR('V', 72, struct v4l2_ext_controls32)
#define VIDIOC_TRY_EXT_CTRLS32  _IOWR('V', 73, struct v4l2_ext_controls32)

#define VIDIOC_OVERLAY32	_IOW ('V', 14, s32)
#define VIDIOC_STREAMON32	_IOW ('V', 18, s32)
#define VIDIOC_STREAMOFF32	_IOW ('V', 19, s32)
#define VIDIOC_G_INPUT32	_IOR ('V', 38, s32)
#define VIDIOC_S_INPUT32	_IOWR('V', 39, s32)
#define VIDIOC_G_OUTPUT32	_IOR ('V', 46, s32)
#define VIDIOC_S_OUTPUT32	_IOWR('V', 47, s32)

static long do_video_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	union {
		struct v4l2_format v2f;
		struct v4l2_buffer v2b;
		struct v4l2_framebuffer v2fb;
		struct v4l2_input v2i;
		struct v4l2_standard v2s;
		struct v4l2_ext_controls v2ecs;
		unsigned long vx;
		int vi;
	} karg;
	void __user *up = compat_ptr(arg);
	int compatible_arg = 1;
	long err = 0;

	/* First, convert the command. */
	switch (cmd) {
	case VIDIOC_G_FMT32: cmd = VIDIOC_G_FMT; break;
	case VIDIOC_S_FMT32: cmd = VIDIOC_S_FMT; break;
	case VIDIOC_QUERYBUF32: cmd = VIDIOC_QUERYBUF; break;
	case VIDIOC_G_FBUF32: cmd = VIDIOC_G_FBUF; break;
	case VIDIOC_S_FBUF32: cmd = VIDIOC_S_FBUF; break;
	case VIDIOC_QBUF32: cmd = VIDIOC_QBUF; break;
	case VIDIOC_DQBUF32: cmd = VIDIOC_DQBUF; break;
	case VIDIOC_ENUMSTD32: cmd = VIDIOC_ENUMSTD; break;
	case VIDIOC_ENUMINPUT32: cmd = VIDIOC_ENUMINPUT; break;
	case VIDIOC_TRY_FMT32: cmd = VIDIOC_TRY_FMT; break;
	case VIDIOC_G_EXT_CTRLS32: cmd = VIDIOC_G_EXT_CTRLS; break;
	case VIDIOC_S_EXT_CTRLS32: cmd = VIDIOC_S_EXT_CTRLS; break;
	case VIDIOC_TRY_EXT_CTRLS32: cmd = VIDIOC_TRY_EXT_CTRLS; break;
	case VIDIOC_OVERLAY32: cmd = VIDIOC_OVERLAY; break;
	case VIDIOC_STREAMON32: cmd = VIDIOC_STREAMON; break;
	case VIDIOC_STREAMOFF32: cmd = VIDIOC_STREAMOFF; break;
	case VIDIOC_G_INPUT32: cmd = VIDIOC_G_INPUT; break;
	case VIDIOC_S_INPUT32: cmd = VIDIOC_S_INPUT; break;
	case VIDIOC_G_OUTPUT32: cmd = VIDIOC_G_OUTPUT; break;
	case VIDIOC_S_OUTPUT32: cmd = VIDIOC_S_OUTPUT; break;
	}

	switch (cmd) {
	case VIDIOC_OVERLAY:
	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	case VIDIOC_S_INPUT:
	case VIDIOC_S_OUTPUT:
		err = get_user(karg.vi, (s32 __user *)up);
		compatible_arg = 0;
		break;

	case VIDIOC_G_INPUT:
	case VIDIOC_G_OUTPUT:
		compatible_arg = 0;
		break;

	case VIDIOC_G_FMT:
	case VIDIOC_S_FMT:
	case VIDIOC_TRY_FMT:
		err = get_v4l2_format32(&karg.v2f, up);
		compatible_arg = 0;
		break;

	case VIDIOC_QUERYBUF:
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
		err = get_v4l2_buffer32(&karg.v2b, up);
		compatible_arg = 0;
		break;

	case VIDIOC_S_FBUF:
		err = get_v4l2_framebuffer32(&karg.v2fb, up);
		compatible_arg = 0;
		break;

	case VIDIOC_G_FBUF:
		compatible_arg = 0;
		break;

	case VIDIOC_ENUMSTD:
		err = get_v4l2_standard32(&karg.v2s, up);
		compatible_arg = 0;
		break;

	case VIDIOC_ENUMINPUT:
		err = get_v4l2_input32(&karg.v2i, up);
		compatible_arg = 0;
		break;

	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
		err = get_v4l2_ext_controls32(&karg.v2ecs, up);
		compatible_arg = 0;
		break;
	}
	if (err)
		return err;

	if (compatible_arg)
		err = native_ioctl(file, cmd, (unsigned long)up);
	else {
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		err = native_ioctl(file, cmd, (unsigned long)&karg);
		set_fs(old_fs);
	}

	/* Special case: even after an error we need to put the
	   results back for these ioctls since the error_idx will
	   contain information on which control failed. */
	switch (cmd) {
	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
		if (put_v4l2_ext_controls32(&karg.v2ecs, up))
			err = -EFAULT;
		break;
	}
	if (err)
		return err;

	switch (cmd) {
	case VIDIOC_S_INPUT:
	case VIDIOC_S_OUTPUT:
	case VIDIOC_G_INPUT:
	case VIDIOC_G_OUTPUT:
		err = put_user(((s32)karg.vi), (s32 __user *)up);
		break;

	case VIDIOC_G_FBUF:
		err = put_v4l2_framebuffer32(&karg.v2fb, up);
		break;

	case VIDIOC_G_FMT:
	case VIDIOC_S_FMT:
	case VIDIOC_TRY_FMT:
		err = put_v4l2_format32(&karg.v2f, up);
		break;

	case VIDIOC_QUERYBUF:
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
		err = put_v4l2_buffer32(&karg.v2b, up);
		break;

	case VIDIOC_ENUMSTD:
		err = put_v4l2_standard32(&karg.v2s, up);
		break;

	case VIDIOC_ENUMINPUT:
		err = put_v4l2_input32(&karg.v2i, up);
		break;
	}
	return err;
}

long v4l2_compat_ioctl32(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	if (!file->f_op->unlocked_ioctl)
		return ret;

	switch (cmd) {
	case VIDIOC_QUERYCAP:
	case VIDIOC_RESERVED:
	case VIDIOC_ENUM_FMT:
	case VIDIOC_G_FMT32:
	case VIDIOC_S_FMT32:
	case VIDIOC_REQBUFS:
	case VIDIOC_QUERYBUF32:
	case VIDIOC_G_FBUF32:
	case VIDIOC_S_FBUF32:
	case VIDIOC_OVERLAY32:
	case VIDIOC_QBUF32:
	case VIDIOC_DQBUF32:
	case VIDIOC_STREAMON32:
	case VIDIOC_STREAMOFF32:
	case VIDIOC_G_PARM:
	case VIDIOC_S_PARM:
	case VIDIOC_G_STD:
	case VIDIOC_S_STD:
	case VIDIOC_ENUMSTD32:
	case VIDIOC_ENUMINPUT32:
	case VIDIOC_G_CTRL:
	case VIDIOC_S_CTRL:
	case VIDIOC_G_TUNER:
	case VIDIOC_S_TUNER:
	case VIDIOC_G_AUDIO:
	case VIDIOC_S_AUDIO:
	case VIDIOC_QUERYCTRL:
	case VIDIOC_QUERYMENU:
	case VIDIOC_G_INPUT32:
	case VIDIOC_S_INPUT32:
	case VIDIOC_G_OUTPUT32:
	case VIDIOC_S_OUTPUT32:
	case VIDIOC_ENUMOUTPUT:
	case VIDIOC_G_AUDOUT:
	case VIDIOC_S_AUDOUT:
	case VIDIOC_G_MODULATOR:
	case VIDIOC_S_MODULATOR:
	case VIDIOC_S_FREQUENCY:
	case VIDIOC_G_FREQUENCY:
	case VIDIOC_CROPCAP:
	case VIDIOC_G_CROP:
	case VIDIOC_S_CROP:
	case VIDIOC_G_JPEGCOMP:
	case VIDIOC_S_JPEGCOMP:
	case VIDIOC_QUERYSTD:
	case VIDIOC_TRY_FMT32:
	case VIDIOC_ENUMAUDIO:
	case VIDIOC_ENUMAUDOUT:
	case VIDIOC_G_PRIORITY:
	case VIDIOC_S_PRIORITY:
	case VIDIOC_G_SLICED_VBI_CAP:
	case VIDIOC_LOG_STATUS:
	case VIDIOC_G_EXT_CTRLS32:
	case VIDIOC_S_EXT_CTRLS32:
	case VIDIOC_TRY_EXT_CTRLS32:
	case VIDIOC_ENUM_FRAMESIZES:
	case VIDIOC_ENUM_FRAMEINTERVALS:
	case VIDIOC_G_ENC_INDEX:
	case VIDIOC_ENCODER_CMD:
	case VIDIOC_TRY_ENCODER_CMD:
	case VIDIOC_DBG_S_REGISTER:
	case VIDIOC_DBG_G_REGISTER:
	case VIDIOC_DBG_G_CHIP_IDENT:
	case VIDIOC_S_HW_FREQ_SEEK:
	case VIDIOC_ENUM_DV_PRESETS:
	case VIDIOC_S_DV_PRESET:
	case VIDIOC_G_DV_PRESET:
	case VIDIOC_QUERY_DV_PRESET:
	case VIDIOC_S_DV_TIMINGS:
	case VIDIOC_G_DV_TIMINGS:
	case VIDIOC_DQEVENT:
	case VIDIOC_SUBSCRIBE_EVENT:
	case VIDIOC_UNSUBSCRIBE_EVENT:
		ret = do_video_ioctl(file, cmd, arg);
		break;

	default:
		printk(KERN_WARNING "compat_ioctl32: "
			"unknown ioctl '%c', dir=%d, #%d (0x%08x)\n",
			_IOC_TYPE(cmd), _IOC_DIR(cmd), _IOC_NR(cmd), cmd);
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(v4l2_compat_ioctl32);
#endif

MODULE_LICENSE("GPL");
