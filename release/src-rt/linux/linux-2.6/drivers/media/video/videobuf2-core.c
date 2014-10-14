/*
 * videobuf2-core.c - V4L2 driver helper framework
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *	   Marek Szyprowski <m.szyprowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <media/videobuf2-core.h>

static int debug;
module_param(debug, int, 0644);

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "vb2: " fmt, ## arg);		\
	} while (0)

#define call_memop(q, plane, op, args...)				\
	(((q)->mem_ops->op) ?						\
		((q)->mem_ops->op(args)) : 0)

#define call_qop(q, op, args...)					\
	(((q)->ops->op) ? ((q)->ops->op(args)) : 0)

#define V4L2_BUFFER_STATE_FLAGS	(V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED | \
				 V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR)

/**
 * __vb2_buf_mem_alloc() - allocate video memory for the given buffer
 */
static int __vb2_buf_mem_alloc(struct vb2_buffer *vb,
				unsigned long *plane_sizes)
{
	struct vb2_queue *q = vb->vb2_queue;
	void *mem_priv;
	int plane;

	/* Allocate memory for all planes in this buffer */
	for (plane = 0; plane < vb->num_planes; ++plane) {
		mem_priv = call_memop(q, plane, alloc, q->alloc_ctx[plane],
					plane_sizes[plane]);
		if (IS_ERR_OR_NULL(mem_priv))
			goto free;

		/* Associate allocator private data with this plane */
		vb->planes[plane].mem_priv = mem_priv;
		vb->v4l2_planes[plane].length = plane_sizes[plane];
	}

	return 0;
free:
	/* Free already allocated memory if one of the allocations failed */
	for (; plane > 0; --plane)
		call_memop(q, plane, put, vb->planes[plane - 1].mem_priv);

	return -ENOMEM;
}

/**
 * __vb2_buf_mem_free() - free memory of the given buffer
 */
static void __vb2_buf_mem_free(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		call_memop(q, plane, put, vb->planes[plane].mem_priv);
		vb->planes[plane].mem_priv = NULL;
		dprintk(3, "Freed plane %d of buffer %d\n",
				plane, vb->v4l2_buf.index);
	}
}

/**
 * __vb2_buf_userptr_put() - release userspace memory associated with
 * a USERPTR buffer
 */
static void __vb2_buf_userptr_put(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		void *mem_priv = vb->planes[plane].mem_priv;

		if (mem_priv) {
			call_memop(q, plane, put_userptr, mem_priv);
			vb->planes[plane].mem_priv = NULL;
		}
	}
}

/**
 * __setup_offsets() - setup unique offsets ("cookies") for every plane in
 * every buffer on the queue
 */
static void __setup_offsets(struct vb2_queue *q)
{
	unsigned int buffer, plane;
	struct vb2_buffer *vb;
	unsigned long off = 0;

	for (buffer = 0; buffer < q->num_buffers; ++buffer) {
		vb = q->bufs[buffer];
		if (!vb)
			continue;

		for (plane = 0; plane < vb->num_planes; ++plane) {
			vb->v4l2_planes[plane].m.mem_offset = off;

			dprintk(3, "Buffer %d, plane %d offset 0x%08lx\n",
					buffer, plane, off);

			off += vb->v4l2_planes[plane].length;
			off = PAGE_ALIGN(off);
		}
	}
}

/**
 * __vb2_queue_alloc() - allocate videobuf buffer structures and (for MMAP type)
 * video buffer memory for all buffers/planes on the queue and initializes the
 * queue
 *
 * Returns the number of buffers successfully allocated.
 */
static int __vb2_queue_alloc(struct vb2_queue *q, enum v4l2_memory memory,
			     unsigned int num_buffers, unsigned int num_planes,
			     unsigned long plane_sizes[])
{
	unsigned int buffer;
	struct vb2_buffer *vb;
	int ret;

	for (buffer = 0; buffer < num_buffers; ++buffer) {
		/* Allocate videobuf buffer structures */
		vb = kzalloc(q->buf_struct_size, GFP_KERNEL);
		if (!vb) {
			dprintk(1, "Memory alloc for buffer struct failed\n");
			break;
		}

		/* Length stores number of planes for multiplanar buffers */
		if (V4L2_TYPE_IS_MULTIPLANAR(q->type))
			vb->v4l2_buf.length = num_planes;

		vb->state = VB2_BUF_STATE_DEQUEUED;
		vb->vb2_queue = q;
		vb->num_planes = num_planes;
		vb->v4l2_buf.index = buffer;
		vb->v4l2_buf.type = q->type;
		vb->v4l2_buf.memory = memory;

		/* Allocate video buffer memory for the MMAP type */
		if (memory == V4L2_MEMORY_MMAP) {
			ret = __vb2_buf_mem_alloc(vb, plane_sizes);
			if (ret) {
				dprintk(1, "Failed allocating memory for "
						"buffer %d\n", buffer);
				kfree(vb);
				break;
			}
			/*
			 * Call the driver-provided buffer initialization
			 * callback, if given. An error in initialization
			 * results in queue setup failure.
			 */
			ret = call_qop(q, buf_init, vb);
			if (ret) {
				dprintk(1, "Buffer %d %p initialization"
					" failed\n", buffer, vb);
				__vb2_buf_mem_free(vb);
				kfree(vb);
				break;
			}
		}

		q->bufs[buffer] = vb;
	}

	q->num_buffers = buffer;

	__setup_offsets(q);

	dprintk(1, "Allocated %d buffers, %d plane(s) each\n",
			q->num_buffers, num_planes);

	return buffer;
}

/**
 * __vb2_free_mem() - release all video buffer memory for a given queue
 */
static void __vb2_free_mem(struct vb2_queue *q)
{
	unsigned int buffer;
	struct vb2_buffer *vb;

	for (buffer = 0; buffer < q->num_buffers; ++buffer) {
		vb = q->bufs[buffer];
		if (!vb)
			continue;

		/* Free MMAP buffers or release USERPTR buffers */
		if (q->memory == V4L2_MEMORY_MMAP)
			__vb2_buf_mem_free(vb);
		else
			__vb2_buf_userptr_put(vb);
	}
}

/**
 * __vb2_queue_free() - free the queue - video memory and related information
 * and return the queue to an uninitialized state. Might be called even if the
 * queue has already been freed.
 */
static void __vb2_queue_free(struct vb2_queue *q)
{
	unsigned int buffer;

	/* Call driver-provided cleanup function for each buffer, if provided */
	if (q->ops->buf_cleanup) {
		for (buffer = 0; buffer < q->num_buffers; ++buffer) {
			if (NULL == q->bufs[buffer])
				continue;
			q->ops->buf_cleanup(q->bufs[buffer]);
		}
	}

	/* Release video buffer memory */
	__vb2_free_mem(q);

	/* Free videobuf buffers */
	for (buffer = 0; buffer < q->num_buffers; ++buffer) {
		kfree(q->bufs[buffer]);
		q->bufs[buffer] = NULL;
	}

	q->num_buffers = 0;
	q->memory = 0;
}

/**
 * __verify_planes_array() - verify that the planes array passed in struct
 * v4l2_buffer from userspace can be safely used
 */
static int __verify_planes_array(struct vb2_buffer *vb, struct v4l2_buffer *b)
{
	/* Is memory for copying plane information present? */
	if (NULL == b->m.planes) {
		dprintk(1, "Multi-planar buffer passed but "
			   "planes array not provided\n");
		return -EINVAL;
	}

	if (b->length < vb->num_planes || b->length > VIDEO_MAX_PLANES) {
		dprintk(1, "Incorrect planes array length, "
			   "expected %d, got %d\n", vb->num_planes, b->length);
		return -EINVAL;
	}

	return 0;
}

/**
 * __fill_v4l2_buffer() - fill in a struct v4l2_buffer with information to be
 * returned to userspace
 */
static int __fill_v4l2_buffer(struct vb2_buffer *vb, struct v4l2_buffer *b)
{
	struct vb2_queue *q = vb->vb2_queue;
	int ret = 0;

	/* Copy back data such as timestamp, flags, input, etc. */
	memcpy(b, &vb->v4l2_buf, offsetof(struct v4l2_buffer, m));
	b->input = vb->v4l2_buf.input;
	b->reserved = vb->v4l2_buf.reserved;

	if (V4L2_TYPE_IS_MULTIPLANAR(q->type)) {
		ret = __verify_planes_array(vb, b);
		if (ret)
			return ret;

		/*
		 * Fill in plane-related data if userspace provided an array
		 * for it. The memory and size is verified above.
		 */
		memcpy(b->m.planes, vb->v4l2_planes,
			b->length * sizeof(struct v4l2_plane));
	} else {
		/*
		 * We use length and offset in v4l2_planes array even for
		 * single-planar buffers, but userspace does not.
		 */
		b->length = vb->v4l2_planes[0].length;
		b->bytesused = vb->v4l2_planes[0].bytesused;
		if (q->memory == V4L2_MEMORY_MMAP)
			b->m.offset = vb->v4l2_planes[0].m.mem_offset;
		else if (q->memory == V4L2_MEMORY_USERPTR)
			b->m.userptr = vb->v4l2_planes[0].m.userptr;
	}

	/*
	 * Clear any buffer state related flags.
	 */
	b->flags &= ~V4L2_BUFFER_STATE_FLAGS;

	switch (vb->state) {
	case VB2_BUF_STATE_QUEUED:
	case VB2_BUF_STATE_ACTIVE:
		b->flags |= V4L2_BUF_FLAG_QUEUED;
		break;
	case VB2_BUF_STATE_ERROR:
		b->flags |= V4L2_BUF_FLAG_ERROR;
		/* fall through */
	case VB2_BUF_STATE_DONE:
		b->flags |= V4L2_BUF_FLAG_DONE;
		break;
	case VB2_BUF_STATE_DEQUEUED:
		/* nothing */
		break;
	}

	if (vb->num_planes_mapped == vb->num_planes)
		b->flags |= V4L2_BUF_FLAG_MAPPED;

	return ret;
}

/**
 * vb2_querybuf() - query video buffer information
 * @q:		videobuf queue
 * @b:		buffer struct passed from userspace to vidioc_querybuf handler
 *		in driver
 *
 * Should be called from vidioc_querybuf ioctl handler in driver.
 * This function will verify the passed v4l2_buffer structure and fill the
 * relevant information for the userspace.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_querybuf handler in driver.
 */
int vb2_querybuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;

	if (b->type != q->type) {
		dprintk(1, "querybuf: wrong buffer type\n");
		return -EINVAL;
	}

	if (b->index >= q->num_buffers) {
		dprintk(1, "querybuf: buffer index out of range\n");
		return -EINVAL;
	}
	vb = q->bufs[b->index];

	return __fill_v4l2_buffer(vb, b);
}
EXPORT_SYMBOL(vb2_querybuf);

/**
 * __verify_userptr_ops() - verify that all memory operations required for
 * USERPTR queue type have been provided
 */
static int __verify_userptr_ops(struct vb2_queue *q)
{
	if (!(q->io_modes & VB2_USERPTR) || !q->mem_ops->get_userptr ||
	    !q->mem_ops->put_userptr)
		return -EINVAL;

	return 0;
}

/**
 * __verify_mmap_ops() - verify that all memory operations required for
 * MMAP queue type have been provided
 */
static int __verify_mmap_ops(struct vb2_queue *q)
{
	if (!(q->io_modes & VB2_MMAP) || !q->mem_ops->alloc ||
	    !q->mem_ops->put || !q->mem_ops->mmap)
		return -EINVAL;

	return 0;
}

/**
 * __buffers_in_use() - return true if any buffers on the queue are in use and
 * the queue cannot be freed (by the means of REQBUFS(0)) call
 */
static bool __buffers_in_use(struct vb2_queue *q)
{
	unsigned int buffer, plane;
	struct vb2_buffer *vb;

	for (buffer = 0; buffer < q->num_buffers; ++buffer) {
		vb = q->bufs[buffer];
		for (plane = 0; plane < vb->num_planes; ++plane) {
			/*
			 * If num_users() has not been provided, call_memop
			 * will return 0, apparently nobody cares about this
			 * case anyway. If num_users() returns more than 1,
			 * we are not the only user of the plane's memory.
			 */
			if (call_memop(q, plane, num_users,
					vb->planes[plane].mem_priv) > 1)
				return true;
		}
	}

	return false;
}

/**
 * vb2_reqbufs() - Initiate streaming
 * @q:		videobuf2 queue
 * @req:	struct passed from userspace to vidioc_reqbufs handler in driver
 *
 * Should be called from vidioc_reqbufs ioctl handler of a driver.
 * This function:
 * 1) verifies streaming parameters passed from the userspace,
 * 2) sets up the queue,
 * 3) negotiates number of buffers and planes per buffer with the driver
 *    to be used during streaming,
 * 4) allocates internal buffer structures (struct vb2_buffer), according to
 *    the agreed parameters,
 * 5) for MMAP memory type, allocates actual video memory, using the
 *    memory handling/allocation routines provided during queue initialization
 *
 * If req->count is 0, all the memory will be freed instead.
 * If the queue has been allocated previously (by a previous vb2_reqbufs) call
 * and the queue is not busy, memory will be reallocated.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_reqbufs handler in driver.
 */
int vb2_reqbufs(struct vb2_queue *q, struct v4l2_requestbuffers *req)
{
	unsigned int num_buffers, num_planes;
	unsigned long plane_sizes[VIDEO_MAX_PLANES];
	int ret = 0;

	if (q->fileio) {
		dprintk(1, "reqbufs: file io in progress\n");
		return -EBUSY;
	}

	if (req->memory != V4L2_MEMORY_MMAP
			&& req->memory != V4L2_MEMORY_USERPTR) {
		dprintk(1, "reqbufs: unsupported memory type\n");
		return -EINVAL;
	}

	if (req->type != q->type) {
		dprintk(1, "reqbufs: requested type is incorrect\n");
		return -EINVAL;
	}

	if (q->streaming) {
		dprintk(1, "reqbufs: streaming active\n");
		return -EBUSY;
	}

	/*
	 * Make sure all the required memory ops for given memory type
	 * are available.
	 */
	if (req->memory == V4L2_MEMORY_MMAP && __verify_mmap_ops(q)) {
		dprintk(1, "reqbufs: MMAP for current setup unsupported\n");
		return -EINVAL;
	}

	if (req->memory == V4L2_MEMORY_USERPTR && __verify_userptr_ops(q)) {
		dprintk(1, "reqbufs: USERPTR for current setup unsupported\n");
		return -EINVAL;
	}

	/*
	 * If the same number of buffers and memory access method is requested
	 * then return immediately.
	 */
	if (q->memory == req->memory && req->count == q->num_buffers)
		return 0;

	if (req->count == 0 || q->num_buffers != 0 || q->memory != req->memory) {
		/*
		 * We already have buffers allocated, so first check if they
		 * are not in use and can be freed.
		 */
		if (q->memory == V4L2_MEMORY_MMAP && __buffers_in_use(q)) {
			dprintk(1, "reqbufs: memory in use, cannot free\n");
			return -EBUSY;
		}

		__vb2_queue_free(q);

		/*
		 * In case of REQBUFS(0) return immediately without calling
		 * driver's queue_setup() callback and allocating resources.
		 */
		if (req->count == 0)
			return 0;
	}

	/*
	 * Make sure the requested values and current defaults are sane.
	 */
	num_buffers = min_t(unsigned int, req->count, VIDEO_MAX_FRAME);
	memset(plane_sizes, 0, sizeof(plane_sizes));
	memset(q->alloc_ctx, 0, sizeof(q->alloc_ctx));
	q->memory = req->memory;

	/*
	 * Ask the driver how many buffers and planes per buffer it requires.
	 * Driver also sets the size and allocator context for each plane.
	 */
	ret = call_qop(q, queue_setup, q, &num_buffers, &num_planes,
		       plane_sizes, q->alloc_ctx);
	if (ret)
		return ret;

	/* Finally, allocate buffers and video memory */
	ret = __vb2_queue_alloc(q, req->memory, num_buffers, num_planes,
				plane_sizes);
	if (ret < 0) {
		dprintk(1, "Memory allocation failed with error: %d\n", ret);
		return ret;
	}

	/*
	 * Check if driver can handle the allocated number of buffers.
	 */
	if (ret < num_buffers) {
		unsigned int orig_num_buffers;

		orig_num_buffers = num_buffers = ret;
		ret = call_qop(q, queue_setup, q, &num_buffers, &num_planes,
			       plane_sizes, q->alloc_ctx);
		if (ret)
			goto free_mem;

		if (orig_num_buffers < num_buffers) {
			ret = -ENOMEM;
			goto free_mem;
		}

		/*
		 * Ok, driver accepted smaller number of buffers.
		 */
		ret = num_buffers;
	}

	/*
	 * Return the number of successfully allocated buffers
	 * to the userspace.
	 */
	req->count = ret;

	return 0;

free_mem:
	__vb2_queue_free(q);
	return ret;
}
EXPORT_SYMBOL_GPL(vb2_reqbufs);

/**
 * vb2_plane_vaddr() - Return a kernel virtual address of a given plane
 * @vb:		vb2_buffer to which the plane in question belongs to
 * @plane_no:	plane number for which the address is to be returned
 *
 * This function returns a kernel virtual address of a given plane if
 * such a mapping exist, NULL otherwise.
 */
void *vb2_plane_vaddr(struct vb2_buffer *vb, unsigned int plane_no)
{
	struct vb2_queue *q = vb->vb2_queue;

	if (plane_no > vb->num_planes)
		return NULL;

	return call_memop(q, plane_no, vaddr, vb->planes[plane_no].mem_priv);

}
EXPORT_SYMBOL_GPL(vb2_plane_vaddr);

/**
 * vb2_plane_cookie() - Return allocator specific cookie for the given plane
 * @vb:		vb2_buffer to which the plane in question belongs to
 * @plane_no:	plane number for which the cookie is to be returned
 *
 * This function returns an allocator specific cookie for a given plane if
 * available, NULL otherwise. The allocator should provide some simple static
 * inline function, which would convert this cookie to the allocator specific
 * type that can be used directly by the driver to access the buffer. This can
 * be for example physical address, pointer to scatter list or IOMMU mapping.
 */
void *vb2_plane_cookie(struct vb2_buffer *vb, unsigned int plane_no)
{
	struct vb2_queue *q = vb->vb2_queue;

	if (plane_no > vb->num_planes)
		return NULL;

	return call_memop(q, plane_no, cookie, vb->planes[plane_no].mem_priv);
}
EXPORT_SYMBOL_GPL(vb2_plane_cookie);

/**
 * vb2_buffer_done() - inform videobuf that an operation on a buffer is finished
 * @vb:		vb2_buffer returned from the driver
 * @state:	either VB2_BUF_STATE_DONE if the operation finished successfully
 *		or VB2_BUF_STATE_ERROR if the operation finished with an error
 *
 * This function should be called by the driver after a hardware operation on
 * a buffer is finished and the buffer may be returned to userspace. The driver
 * cannot use this buffer anymore until it is queued back to it by videobuf
 * by the means of buf_queue callback. Only buffers previously queued to the
 * driver by buf_queue can be passed to this function.
 */
void vb2_buffer_done(struct vb2_buffer *vb, enum vb2_buffer_state state)
{
	struct vb2_queue *q = vb->vb2_queue;
	unsigned long flags;

	if (vb->state != VB2_BUF_STATE_ACTIVE)
		return;

	if (state != VB2_BUF_STATE_DONE && state != VB2_BUF_STATE_ERROR)
		return;

	dprintk(4, "Done processing on buffer %d, state: %d\n",
			vb->v4l2_buf.index, vb->state);

	/* Add the buffer to the done buffers list */
	spin_lock_irqsave(&q->done_lock, flags);
	vb->state = state;
	list_add_tail(&vb->done_entry, &q->done_list);
	atomic_dec(&q->queued_count);
	spin_unlock_irqrestore(&q->done_lock, flags);

	/* Inform any processes that may be waiting for buffers */
	wake_up(&q->done_wq);
}
EXPORT_SYMBOL_GPL(vb2_buffer_done);

/**
 * __fill_vb2_buffer() - fill a vb2_buffer with information provided in
 * a v4l2_buffer by the userspace
 */
static int __fill_vb2_buffer(struct vb2_buffer *vb, struct v4l2_buffer *b,
				struct v4l2_plane *v4l2_planes)
{
	unsigned int plane;
	int ret;

	if (V4L2_TYPE_IS_MULTIPLANAR(b->type)) {
		/*
		 * Verify that the userspace gave us a valid array for
		 * plane information.
		 */
		ret = __verify_planes_array(vb, b);
		if (ret)
			return ret;

		/* Fill in driver-provided information for OUTPUT types */
		if (V4L2_TYPE_IS_OUTPUT(b->type)) {
			/*
			 * Will have to go up to b->length when API starts
			 * accepting variable number of planes.
			 */
			for (plane = 0; plane < vb->num_planes; ++plane) {
				v4l2_planes[plane].bytesused =
					b->m.planes[plane].bytesused;
				v4l2_planes[plane].data_offset =
					b->m.planes[plane].data_offset;
			}
		}

		if (b->memory == V4L2_MEMORY_USERPTR) {
			for (plane = 0; plane < vb->num_planes; ++plane) {
				v4l2_planes[plane].m.userptr =
					b->m.planes[plane].m.userptr;
				v4l2_planes[plane].length =
					b->m.planes[plane].length;
			}
		}
	} else {
		/*
		 * Single-planar buffers do not use planes array,
		 * so fill in relevant v4l2_buffer struct fields instead.
		 * In videobuf we use our internal V4l2_planes struct for
		 * single-planar buffers as well, for simplicity.
		 */
		if (V4L2_TYPE_IS_OUTPUT(b->type))
			v4l2_planes[0].bytesused = b->bytesused;

		if (b->memory == V4L2_MEMORY_USERPTR) {
			v4l2_planes[0].m.userptr = b->m.userptr;
			v4l2_planes[0].length = b->length;
		}
	}

	vb->v4l2_buf.field = b->field;
	vb->v4l2_buf.timestamp = b->timestamp;
	vb->v4l2_buf.input = b->input;
	vb->v4l2_buf.flags = b->flags & ~V4L2_BUFFER_STATE_FLAGS;

	return 0;
}

/**
 * __qbuf_userptr() - handle qbuf of a USERPTR buffer
 */
static int __qbuf_userptr(struct vb2_buffer *vb, struct v4l2_buffer *b)
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct vb2_queue *q = vb->vb2_queue;
	void *mem_priv;
	unsigned int plane;
	int ret;
	int write = !V4L2_TYPE_IS_OUTPUT(q->type);

	/* Verify and copy relevant information provided by the userspace */
	ret = __fill_vb2_buffer(vb, b, planes);
	if (ret)
		return ret;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		/* Skip the plane if already verified */
		if (vb->v4l2_planes[plane].m.userptr == planes[plane].m.userptr
		    && vb->v4l2_planes[plane].length == planes[plane].length)
			continue;

		dprintk(3, "qbuf: userspace address for plane %d changed, "
				"reacquiring memory\n", plane);

		/* Release previously acquired memory if present */
		if (vb->planes[plane].mem_priv)
			call_memop(q, plane, put_userptr,
					vb->planes[plane].mem_priv);

		vb->planes[plane].mem_priv = NULL;

		/* Acquire each plane's memory */
		if (q->mem_ops->get_userptr) {
			mem_priv = q->mem_ops->get_userptr(q->alloc_ctx[plane],
							planes[plane].m.userptr,
							planes[plane].length,
							write);
			if (IS_ERR(mem_priv)) {
				dprintk(1, "qbuf: failed acquiring userspace "
						"memory for plane %d\n", plane);
				ret = PTR_ERR(mem_priv);
				goto err;
			}
			vb->planes[plane].mem_priv = mem_priv;
		}
	}

	/*
	 * Call driver-specific initialization on the newly acquired buffer,
	 * if provided.
	 */
	ret = call_qop(q, buf_init, vb);
	if (ret) {
		dprintk(1, "qbuf: buffer initialization failed\n");
		goto err;
	}

	/*
	 * Now that everything is in order, copy relevant information
	 * provided by userspace.
	 */
	for (plane = 0; plane < vb->num_planes; ++plane)
		vb->v4l2_planes[plane] = planes[plane];

	return 0;
err:
	/* In case of errors, release planes that were already acquired */
	for (; plane > 0; --plane) {
		call_memop(q, plane, put_userptr,
				vb->planes[plane - 1].mem_priv);
		vb->planes[plane - 1].mem_priv = NULL;
	}

	return ret;
}

/**
 * __qbuf_mmap() - handle qbuf of an MMAP buffer
 */
static int __qbuf_mmap(struct vb2_buffer *vb, struct v4l2_buffer *b)
{
	return __fill_vb2_buffer(vb, b, vb->v4l2_planes);
}

/**
 * __enqueue_in_driver() - enqueue a vb2_buffer in driver for processing
 */
static void __enqueue_in_driver(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;

	vb->state = VB2_BUF_STATE_ACTIVE;
	atomic_inc(&q->queued_count);
	q->ops->buf_queue(vb);
}

/**
 * vb2_qbuf() - Queue a buffer from userspace
 * @q:		videobuf2 queue
 * @b:		buffer structure passed from userspace to vidioc_qbuf handler
 *		in driver
 *
 * Should be called from vidioc_qbuf ioctl handler of a driver.
 * This function:
 * 1) verifies the passed buffer,
 * 2) calls buf_prepare callback in the driver (if provided), in which
 *    driver-specific buffer initialization can be performed,
 * 3) if streaming is on, queues the buffer in driver by the means of buf_queue
 *    callback for processing.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_qbuf handler in driver.
 */
int vb2_qbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;
	int ret = 0;

	if (q->fileio) {
		dprintk(1, "qbuf: file io in progress\n");
		return -EBUSY;
	}

	if (b->type != q->type) {
		dprintk(1, "qbuf: invalid buffer type\n");
		return -EINVAL;
	}

	if (b->index >= q->num_buffers) {
		dprintk(1, "qbuf: buffer index out of range\n");
		return -EINVAL;
	}

	vb = q->bufs[b->index];
	if (NULL == vb) {
		/* Should never happen */
		dprintk(1, "qbuf: buffer is NULL\n");
		return -EINVAL;
	}

	if (b->memory != q->memory) {
		dprintk(1, "qbuf: invalid memory type\n");
		return -EINVAL;
	}

	if (vb->state != VB2_BUF_STATE_DEQUEUED) {
		dprintk(1, "qbuf: buffer already in use\n");
		return -EINVAL;
	}

	if (q->memory == V4L2_MEMORY_MMAP)
		ret = __qbuf_mmap(vb, b);
	else if (q->memory == V4L2_MEMORY_USERPTR)
		ret = __qbuf_userptr(vb, b);
	else {
		WARN(1, "Invalid queue type\n");
		return -EINVAL;
	}

	if (ret)
		return ret;

	ret = call_qop(q, buf_prepare, vb);
	if (ret) {
		dprintk(1, "qbuf: buffer preparation failed\n");
		return ret;
	}

	/*
	 * Add to the queued buffers list, a buffer will stay on it until
	 * dequeued in dqbuf.
	 */
	list_add_tail(&vb->queued_entry, &q->queued_list);
	vb->state = VB2_BUF_STATE_QUEUED;

	/*
	 * If already streaming, give the buffer to driver for processing.
	 * If not, the buffer will be given to driver on next streamon.
	 */
	if (q->streaming)
		__enqueue_in_driver(vb);

	dprintk(1, "qbuf of buffer %d succeeded\n", vb->v4l2_buf.index);
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_qbuf);

/**
 * __vb2_wait_for_done_vb() - wait for a buffer to become available
 * for dequeuing
 *
 * Will sleep if required for nonblocking == false.
 */
static int __vb2_wait_for_done_vb(struct vb2_queue *q, int nonblocking)
{
	/*
	 * All operations on vb_done_list are performed under done_lock
	 * spinlock protection. However, buffers may be removed from
	 * it and returned to userspace only while holding both driver's
	 * lock and the done_lock spinlock. Thus we can be sure that as
	 * long as we hold the driver's lock, the list will remain not
	 * empty if list_empty() check succeeds.
	 */

	for (;;) {
		int ret;

		if (!q->streaming) {
			dprintk(1, "Streaming off, will not wait for buffers\n");
			return -EINVAL;
		}

		if (!list_empty(&q->done_list)) {
			/*
			 * Found a buffer that we were waiting for.
			 */
			break;
		}

		if (nonblocking) {
			dprintk(1, "Nonblocking and no buffers to dequeue, "
								"will not wait\n");
			return -EAGAIN;
		}

		/*
		 * We are streaming and blocking, wait for another buffer to
		 * become ready or for streamoff. Driver's lock is released to
		 * allow streamoff or qbuf to be called while waiting.
		 */
		call_qop(q, wait_prepare, q);

		/*
		 * All locks have been released, it is safe to sleep now.
		 */
		dprintk(3, "Will sleep waiting for buffers\n");
		ret = wait_event_interruptible(q->done_wq,
				!list_empty(&q->done_list) || !q->streaming);

		/*
		 * We need to reevaluate both conditions again after reacquiring
		 * the locks or return an error if one occurred.
		 */
		call_qop(q, wait_finish, q);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * __vb2_get_done_vb() - get a buffer ready for dequeuing
 *
 * Will sleep if required for nonblocking == false.
 */
static int __vb2_get_done_vb(struct vb2_queue *q, struct vb2_buffer **vb,
				int nonblocking)
{
	unsigned long flags;
	int ret;

	/*
	 * Wait for at least one buffer to become available on the done_list.
	 */
	ret = __vb2_wait_for_done_vb(q, nonblocking);
	if (ret)
		return ret;

	/*
	 * Driver's lock has been held since we last verified that done_list
	 * is not empty, so no need for another list_empty(done_list) check.
	 */
	spin_lock_irqsave(&q->done_lock, flags);
	*vb = list_first_entry(&q->done_list, struct vb2_buffer, done_entry);
	list_del(&(*vb)->done_entry);
	spin_unlock_irqrestore(&q->done_lock, flags);

	return 0;
}

/**
 * vb2_wait_for_all_buffers() - wait until all buffers are given back to vb2
 * @q:		videobuf2 queue
 *
 * This function will wait until all buffers that have been given to the driver
 * by buf_queue() are given back to vb2 with vb2_buffer_done(). It doesn't call
 * wait_prepare, wait_finish pair. It is intended to be called with all locks
 * taken, for example from stop_streaming() callback.
 */
int vb2_wait_for_all_buffers(struct vb2_queue *q)
{
	if (!q->streaming) {
		dprintk(1, "Streaming off, will not wait for buffers\n");
		return -EINVAL;
	}

	wait_event(q->done_wq, !atomic_read(&q->queued_count));
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_wait_for_all_buffers);

/**
 * vb2_dqbuf() - Dequeue a buffer to the userspace
 * @q:		videobuf2 queue
 * @b:		buffer structure passed from userspace to vidioc_dqbuf handler
 *		in driver
 * @nonblocking: if true, this call will not sleep waiting for a buffer if no
 *		 buffers ready for dequeuing are present. Normally the driver
 *		 would be passing (file->f_flags & O_NONBLOCK) here
 *
 * Should be called from vidioc_dqbuf ioctl handler of a driver.
 * This function:
 * 1) verifies the passed buffer,
 * 2) calls buf_finish callback in the driver (if provided), in which
 *    driver can perform any additional operations that may be required before
 *    returning the buffer to userspace, such as cache sync,
 * 3) the buffer struct members are filled with relevant information for
 *    the userspace.
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_dqbuf handler in driver.
 */
int vb2_dqbuf(struct vb2_queue *q, struct v4l2_buffer *b, bool nonblocking)
{
	struct vb2_buffer *vb = NULL;
	int ret;

	if (q->fileio) {
		dprintk(1, "dqbuf: file io in progress\n");
		return -EBUSY;
	}

	if (b->type != q->type) {
		dprintk(1, "dqbuf: invalid buffer type\n");
		return -EINVAL;
	}

	ret = __vb2_get_done_vb(q, &vb, nonblocking);
	if (ret < 0) {
		dprintk(1, "dqbuf: error getting next done buffer\n");
		return ret;
	}

	ret = call_qop(q, buf_finish, vb);
	if (ret) {
		dprintk(1, "dqbuf: buffer finish failed\n");
		return ret;
	}

	switch (vb->state) {
	case VB2_BUF_STATE_DONE:
		dprintk(3, "dqbuf: Returning done buffer\n");
		break;
	case VB2_BUF_STATE_ERROR:
		dprintk(3, "dqbuf: Returning done buffer with errors\n");
		break;
	default:
		dprintk(1, "dqbuf: Invalid buffer state\n");
		return -EINVAL;
	}

	/* Fill buffer information for the userspace */
	__fill_v4l2_buffer(vb, b);
	/* Remove from videobuf queue */
	list_del(&vb->queued_entry);

	dprintk(1, "dqbuf of buffer %d, with state %d\n",
			vb->v4l2_buf.index, vb->state);

	vb->state = VB2_BUF_STATE_DEQUEUED;
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_dqbuf);

/**
 * vb2_streamon - start streaming
 * @q:		videobuf2 queue
 * @type:	type argument passed from userspace to vidioc_streamon handler
 *
 * Should be called from vidioc_streamon handler of a driver.
 * This function:
 * 1) verifies current state
 * 2) starts streaming and passes any previously queued buffers to the driver
 *
 * The return values from this function are intended to be directly returned
 * from vidioc_streamon handler in the driver.
 */
int vb2_streamon(struct vb2_queue *q, enum v4l2_buf_type type)
{
	struct vb2_buffer *vb;
	int ret;

	if (q->fileio) {
		dprintk(1, "streamon: file io in progress\n");
		return -EBUSY;
	}

	if (type != q->type) {
		dprintk(1, "streamon: invalid stream type\n");
		return -EINVAL;
	}

	if (q->streaming) {
		dprintk(1, "streamon: already streaming\n");
		return -EBUSY;
	}

	/*
	 * Cannot start streaming on an OUTPUT device if no buffers have
	 * been queued yet.
	 */
	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (list_empty(&q->queued_list)) {
			dprintk(1, "streamon: no output buffers queued\n");
			return -EINVAL;
		}
	}

	/*
	 * Let driver notice that streaming state has been enabled.
	 */
	ret = call_qop(q, start_streaming, q);
	if (ret) {
		dprintk(1, "streamon: driver refused to start streaming\n");
		return ret;
	}

	q->streaming = 1;

	/*
	 * If any buffers were queued before streamon,
	 * we can now pass them to driver for processing.
	 */
	list_for_each_entry(vb, &q->queued_list, queued_entry)
		__enqueue_in_driver(vb);

	dprintk(3, "Streamon successful\n");
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_streamon);

/**
 * __vb2_queue_cancel() - cancel and stop (pause) streaming
 *
 * Removes all queued buffers from driver's queue and all buffers queued by
 * userspace from videobuf's queue. Returns to state after reqbufs.
 */
static void __vb2_queue_cancel(struct vb2_queue *q)
{
	unsigned int i;

	/*
	 * Tell driver to stop all transactions and release all queued
	 * buffers.
	 */
	if (q->streaming)
		call_qop(q, stop_streaming, q);
	q->streaming = 0;

	/*
	 * Remove all buffers from videobuf's list...
	 */
	INIT_LIST_HEAD(&q->queued_list);
	/*
	 * ...and done list; userspace will not receive any buffers it
	 * has not already dequeued before initiating cancel.
	 */
	INIT_LIST_HEAD(&q->done_list);
	wake_up_all(&q->done_wq);

	/*
	 * Reinitialize all buffers for next use.
	 */
	for (i = 0; i < q->num_buffers; ++i)
		q->bufs[i]->state = VB2_BUF_STATE_DEQUEUED;
}

/**
 * vb2_streamoff - stop streaming
 * @q:		videobuf2 queue
 * @type:	type argument passed from userspace to vidioc_streamoff handler
 *
 * Should be called from vidioc_streamoff handler of a driver.
 * This function:
 * 1) verifies current state,
 * 2) stop streaming and dequeues any queued buffers, including those previously
 *    passed to the driver (after waiting for the driver to finish).
 *
 * This call can be used for pausing playback.
 * The return values from this function are intended to be directly returned
 * from vidioc_streamoff handler in the driver
 */
int vb2_streamoff(struct vb2_queue *q, enum v4l2_buf_type type)
{
	if (q->fileio) {
		dprintk(1, "streamoff: file io in progress\n");
		return -EBUSY;
	}

	if (type != q->type) {
		dprintk(1, "streamoff: invalid stream type\n");
		return -EINVAL;
	}

	if (!q->streaming) {
		dprintk(1, "streamoff: not streaming\n");
		return -EINVAL;
	}

	/*
	 * Cancel will pause streaming and remove all buffers from the driver
	 * and videobuf, effectively returning control over them to userspace.
	 */
	__vb2_queue_cancel(q);

	dprintk(3, "Streamoff successful\n");
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_streamoff);

/**
 * __find_plane_by_offset() - find plane associated with the given offset off
 */
static int __find_plane_by_offset(struct vb2_queue *q, unsigned long off,
			unsigned int *_buffer, unsigned int *_plane)
{
	struct vb2_buffer *vb;
	unsigned int buffer, plane;

	/*
	 * Go over all buffers and their planes, comparing the given offset
	 * with an offset assigned to each plane. If a match is found,
	 * return its buffer and plane numbers.
	 */
	for (buffer = 0; buffer < q->num_buffers; ++buffer) {
		vb = q->bufs[buffer];

		for (plane = 0; plane < vb->num_planes; ++plane) {
			if (vb->v4l2_planes[plane].m.mem_offset == off) {
				*_buffer = buffer;
				*_plane = plane;
				return 0;
			}
		}
	}

	return -EINVAL;
}

/**
 * vb2_mmap() - map video buffers into application address space
 * @q:		videobuf2 queue
 * @vma:	vma passed to the mmap file operation handler in the driver
 *
 * Should be called from mmap file operation handler of a driver.
 * This function maps one plane of one of the available video buffers to
 * userspace. To map whole video memory allocated on reqbufs, this function
 * has to be called once per each plane per each buffer previously allocated.
 *
 * When the userspace application calls mmap, it passes to it an offset returned
 * to it earlier by the means of vidioc_querybuf handler. That offset acts as
 * a "cookie", which is then used to identify the plane to be mapped.
 * This function finds a plane with a matching offset and a mapping is performed
 * by the means of a provided memory operation.
 *
 * The return values from this function are intended to be directly returned
 * from the mmap handler in driver.
 */
int vb2_mmap(struct vb2_queue *q, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	struct vb2_plane *vb_plane;
	struct vb2_buffer *vb;
	unsigned int buffer, plane;
	int ret;

	if (q->memory != V4L2_MEMORY_MMAP) {
		dprintk(1, "Queue is not currently set up for mmap\n");
		return -EINVAL;
	}

	/*
	 * Check memory area access mode.
	 */
	if (!(vma->vm_flags & VM_SHARED)) {
		dprintk(1, "Invalid vma flags, VM_SHARED needed\n");
		return -EINVAL;
	}
	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (!(vma->vm_flags & VM_WRITE)) {
			dprintk(1, "Invalid vma flags, VM_WRITE needed\n");
			return -EINVAL;
		}
	} else {
		if (!(vma->vm_flags & VM_READ)) {
			dprintk(1, "Invalid vma flags, VM_READ needed\n");
			return -EINVAL;
		}
	}

	/*
	 * Find the plane corresponding to the offset passed by userspace.
	 */
	ret = __find_plane_by_offset(q, off, &buffer, &plane);
	if (ret)
		return ret;

	vb = q->bufs[buffer];
	vb_plane = &vb->planes[plane];

	ret = q->mem_ops->mmap(vb_plane->mem_priv, vma);
	if (ret)
		return ret;

	vb_plane->mapped = 1;
	vb->num_planes_mapped++;

	dprintk(3, "Buffer %d, plane %d successfully mapped\n", buffer, plane);
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_mmap);

static int __vb2_init_fileio(struct vb2_queue *q, int read);
static int __vb2_cleanup_fileio(struct vb2_queue *q);

/**
 * vb2_poll() - implements poll userspace operation
 * @q:		videobuf2 queue
 * @file:	file argument passed to the poll file operation handler
 * @wait:	wait argument passed to the poll file operation handler
 *
 * This function implements poll file operation handler for a driver.
 * For CAPTURE queues, if a buffer is ready to be dequeued, the userspace will
 * be informed that the file descriptor of a video device is available for
 * reading.
 * For OUTPUT queues, if a buffer is ready to be dequeued, the file descriptor
 * will be reported as available for writing.
 *
 * The return values from this function are intended to be directly returned
 * from poll handler in driver.
 */
unsigned int vb2_poll(struct vb2_queue *q, struct file *file, poll_table *wait)
{
	unsigned long flags;
	unsigned int ret;
	struct vb2_buffer *vb = NULL;

	/*
	 * Start file I/O emulator only if streaming API has not been used yet.
	 */
	if (q->num_buffers == 0 && q->fileio == NULL) {
		if (!V4L2_TYPE_IS_OUTPUT(q->type) && (q->io_modes & VB2_READ)) {
			ret = __vb2_init_fileio(q, 1);
			if (ret)
				return POLLERR;
		}
		if (V4L2_TYPE_IS_OUTPUT(q->type) && (q->io_modes & VB2_WRITE)) {
			ret = __vb2_init_fileio(q, 0);
			if (ret)
				return POLLERR;
			/*
			 * Write to OUTPUT queue can be done immediately.
			 */
			return POLLOUT | POLLWRNORM;
		}
	}

	/*
	 * There is nothing to wait for if no buffers have already been queued.
	 */
	if (list_empty(&q->queued_list))
		return POLLERR;

	poll_wait(file, &q->done_wq, wait);

	/*
	 * Take first buffer available for dequeuing.
	 */
	spin_lock_irqsave(&q->done_lock, flags);
	if (!list_empty(&q->done_list))
		vb = list_first_entry(&q->done_list, struct vb2_buffer,
					done_entry);
	spin_unlock_irqrestore(&q->done_lock, flags);

	if (vb && (vb->state == VB2_BUF_STATE_DONE
			|| vb->state == VB2_BUF_STATE_ERROR)) {
		return (V4L2_TYPE_IS_OUTPUT(q->type)) ? POLLOUT | POLLWRNORM :
			POLLIN | POLLRDNORM;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(vb2_poll);

/**
 * vb2_queue_init() - initialize a videobuf2 queue
 * @q:		videobuf2 queue; this structure should be allocated in driver
 *
 * The vb2_queue structure should be allocated by the driver. The driver is
 * responsible of clearing it's content and setting initial values for some
 * required entries before calling this function.
 * q->ops, q->mem_ops, q->type and q->io_modes are mandatory. Please refer
 * to the struct vb2_queue description in include/media/videobuf2-core.h
 * for more information.
 */
int vb2_queue_init(struct vb2_queue *q)
{
	BUG_ON(!q);
	BUG_ON(!q->ops);
	BUG_ON(!q->mem_ops);
	BUG_ON(!q->type);
	BUG_ON(!q->io_modes);

	BUG_ON(!q->ops->queue_setup);
	BUG_ON(!q->ops->buf_queue);

	INIT_LIST_HEAD(&q->queued_list);
	INIT_LIST_HEAD(&q->done_list);
	spin_lock_init(&q->done_lock);
	init_waitqueue_head(&q->done_wq);

	if (q->buf_struct_size == 0)
		q->buf_struct_size = sizeof(struct vb2_buffer);

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_queue_init);

/**
 * vb2_queue_release() - stop streaming, release the queue and free memory
 * @q:		videobuf2 queue
 *
 * This function stops streaming and performs necessary clean ups, including
 * freeing video buffer memory. The driver is responsible for freeing
 * the vb2_queue structure itself.
 */
void vb2_queue_release(struct vb2_queue *q)
{
	__vb2_cleanup_fileio(q);
	__vb2_queue_cancel(q);
	__vb2_queue_free(q);
}
EXPORT_SYMBOL_GPL(vb2_queue_release);

/**
 * struct vb2_fileio_buf - buffer context used by file io emulator
 *
 * vb2 provides a compatibility layer and emulator of file io (read and
 * write) calls on top of streaming API. This structure is used for
 * tracking context related to the buffers.
 */
struct vb2_fileio_buf {
	void *vaddr;
	unsigned int size;
	unsigned int pos;
	unsigned int queued:1;
};

/**
 * struct vb2_fileio_data - queue context used by file io emulator
 *
 * vb2 provides a compatibility layer and emulator of file io (read and
 * write) calls on top of streaming API. For proper operation it required
 * this structure to save the driver state between each call of the read
 * or write function.
 */
struct vb2_fileio_data {
	struct v4l2_requestbuffers req;
	struct v4l2_buffer b;
	struct vb2_fileio_buf bufs[VIDEO_MAX_FRAME];
	unsigned int index;
	unsigned int q_count;
	unsigned int dq_count;
	unsigned int flags;
};

/**
 * __vb2_init_fileio() - initialize file io emulator
 * @q:		videobuf2 queue
 * @read:	mode selector (1 means read, 0 means write)
 */
static int __vb2_init_fileio(struct vb2_queue *q, int read)
{
	struct vb2_fileio_data *fileio;
	int i, ret;
	unsigned int count = 0;

	/*
	 * Sanity check
	 */
	if ((read && !(q->io_modes & VB2_READ)) ||
	   (!read && !(q->io_modes & VB2_WRITE)))
		BUG();

	/*
	 * Check if device supports mapping buffers to kernel virtual space.
	 */
	if (!q->mem_ops->vaddr)
		return -EBUSY;

	/*
	 * Check if streaming api has not been already activated.
	 */
	if (q->streaming || q->num_buffers > 0)
		return -EBUSY;

	/*
	 * Start with count 1, driver can increase it in queue_setup()
	 */
	count = 1;

	dprintk(3, "setting up file io: mode %s, count %d, flags %08x\n",
		(read) ? "read" : "write", count, q->io_flags);

	fileio = kzalloc(sizeof(struct vb2_fileio_data), GFP_KERNEL);
	if (fileio == NULL)
		return -ENOMEM;

	fileio->flags = q->io_flags;

	/*
	 * Request buffers and use MMAP type to force driver
	 * to allocate buffers by itself.
	 */
	fileio->req.count = count;
	fileio->req.memory = V4L2_MEMORY_MMAP;
	fileio->req.type = q->type;
	ret = vb2_reqbufs(q, &fileio->req);
	if (ret)
		goto err_kfree;

	/*
	 * Check if plane_count is correct
	 * (multiplane buffers are not supported).
	 */
	if (q->bufs[0]->num_planes != 1) {
		fileio->req.count = 0;
		ret = -EBUSY;
		goto err_reqbufs;
	}

	/*
	 * Get kernel address of each buffer.
	 */
	for (i = 0; i < q->num_buffers; i++) {
		fileio->bufs[i].vaddr = vb2_plane_vaddr(q->bufs[i], 0);
		if (fileio->bufs[i].vaddr == NULL)
			goto err_reqbufs;
		fileio->bufs[i].size = vb2_plane_size(q->bufs[i], 0);
	}

	/*
	 * Read mode requires pre queuing of all buffers.
	 */
	if (read) {
		/*
		 * Queue all buffers.
		 */
		for (i = 0; i < q->num_buffers; i++) {
			struct v4l2_buffer *b = &fileio->b;
			memset(b, 0, sizeof(*b));
			b->type = q->type;
			b->memory = q->memory;
			b->index = i;
			ret = vb2_qbuf(q, b);
			if (ret)
				goto err_reqbufs;
			fileio->bufs[i].queued = 1;
		}

		/*
		 * Start streaming.
		 */
		ret = vb2_streamon(q, q->type);
		if (ret)
			goto err_reqbufs;
	}

	q->fileio = fileio;

	return ret;

err_reqbufs:
	vb2_reqbufs(q, &fileio->req);

err_kfree:
	kfree(fileio);
	return ret;
}

/**
 * __vb2_cleanup_fileio() - free resourced used by file io emulator
 * @q:		videobuf2 queue
 */
static int __vb2_cleanup_fileio(struct vb2_queue *q)
{
	struct vb2_fileio_data *fileio = q->fileio;

	if (fileio) {
		/*
		 * Hack fileio context to enable direct calls to vb2 ioctl
		 * interface.
		 */
		q->fileio = NULL;

		vb2_streamoff(q, q->type);
		fileio->req.count = 0;
		vb2_reqbufs(q, &fileio->req);
		kfree(fileio);
		dprintk(3, "file io emulator closed\n");
	}
	return 0;
}

/**
 * __vb2_perform_fileio() - perform a single file io (read or write) operation
 * @q:		videobuf2 queue
 * @data:	pointed to target userspace buffer
 * @count:	number of bytes to read or write
 * @ppos:	file handle position tracking pointer
 * @nonblock:	mode selector (1 means blocking calls, 0 means nonblocking)
 * @read:	access mode selector (1 means read, 0 means write)
 */
static size_t __vb2_perform_fileio(struct vb2_queue *q, char __user *data, size_t count,
		loff_t *ppos, int nonblock, int read)
{
	struct vb2_fileio_data *fileio;
	struct vb2_fileio_buf *buf;
	int ret, index;

	dprintk(3, "file io: mode %s, offset %ld, count %zd, %sblocking\n",
		read ? "read" : "write", (long)*ppos, count,
		nonblock ? "non" : "");

	if (!data)
		return -EINVAL;

	/*
	 * Initialize emulator on first call.
	 */
	if (!q->fileio) {
		ret = __vb2_init_fileio(q, read);
		dprintk(3, "file io: vb2_init_fileio result: %d\n", ret);
		if (ret)
			return ret;
	}
	fileio = q->fileio;

	/*
	 * Hack fileio context to enable direct calls to vb2 ioctl interface.
	 * The pointer will be restored before returning from this function.
	 */
	q->fileio = NULL;

	index = fileio->index;
	buf = &fileio->bufs[index];

	/*
	 * Check if we need to dequeue the buffer.
	 */
	if (buf->queued) {
		struct vb2_buffer *vb;

		/*
		 * Call vb2_dqbuf to get buffer back.
		 */
		memset(&fileio->b, 0, sizeof(fileio->b));
		fileio->b.type = q->type;
		fileio->b.memory = q->memory;
		fileio->b.index = index;
		ret = vb2_dqbuf(q, &fileio->b, nonblock);
		dprintk(5, "file io: vb2_dqbuf result: %d\n", ret);
		if (ret)
			goto end;
		fileio->dq_count += 1;

		/*
		 * Get number of bytes filled by the driver
		 */
		vb = q->bufs[index];
		buf->size = vb2_get_plane_payload(vb, 0);
		buf->queued = 0;
	}

	/*
	 * Limit count on last few bytes of the buffer.
	 */
	if (buf->pos + count > buf->size) {
		count = buf->size - buf->pos;
		dprintk(5, "reducing read count: %zd\n", count);
	}

	/*
	 * Transfer data to userspace.
	 */
	dprintk(3, "file io: copying %zd bytes - buffer %d, offset %u\n",
		count, index, buf->pos);
	if (read)
		ret = copy_to_user(data, buf->vaddr + buf->pos, count);
	else
		ret = copy_from_user(buf->vaddr + buf->pos, data, count);
	if (ret) {
		dprintk(3, "file io: error copying data\n");
		ret = -EFAULT;
		goto end;
	}

	/*
	 * Update counters.
	 */
	buf->pos += count;
	*ppos += count;

	/*
	 * Queue next buffer if required.
	 */
	if (buf->pos == buf->size ||
	   (!read && (fileio->flags & VB2_FILEIO_WRITE_IMMEDIATELY))) {
		/*
		 * Check if this is the last buffer to read.
		 */
		if (read && (fileio->flags & VB2_FILEIO_READ_ONCE) &&
		    fileio->dq_count == 1) {
			dprintk(3, "file io: read limit reached\n");
			/*
			 * Restore fileio pointer and release the context.
			 */
			q->fileio = fileio;
			return __vb2_cleanup_fileio(q);
		}

		/*
		 * Call vb2_qbuf and give buffer to the driver.
		 */
		memset(&fileio->b, 0, sizeof(fileio->b));
		fileio->b.type = q->type;
		fileio->b.memory = q->memory;
		fileio->b.index = index;
		fileio->b.bytesused = buf->pos;
		ret = vb2_qbuf(q, &fileio->b);
		dprintk(5, "file io: vb2_dbuf result: %d\n", ret);
		if (ret)
			goto end;

		/*
		 * Buffer has been queued, update the status
		 */
		buf->pos = 0;
		buf->queued = 1;
		buf->size = q->bufs[0]->v4l2_planes[0].length;
		fileio->q_count += 1;

		/*
		 * Switch to the next buffer
		 */
		fileio->index = (index + 1) % q->num_buffers;

		/*
		 * Start streaming if required.
		 */
		if (!read && !q->streaming) {
			ret = vb2_streamon(q, q->type);
			if (ret)
				goto end;
		}
	}

	/*
	 * Return proper number of bytes processed.
	 */
	if (ret == 0)
		ret = count;
end:
	/*
	 * Restore the fileio context and block vb2 ioctl interface.
	 */
	q->fileio = fileio;
	return ret;
}

size_t vb2_read(struct vb2_queue *q, char __user *data, size_t count,
		loff_t *ppos, int nonblocking)
{
	return __vb2_perform_fileio(q, data, count, ppos, nonblocking, 1);
}
EXPORT_SYMBOL_GPL(vb2_read);

size_t vb2_write(struct vb2_queue *q, char __user *data, size_t count,
		loff_t *ppos, int nonblocking)
{
	return __vb2_perform_fileio(q, data, count, ppos, nonblocking, 0);
}
EXPORT_SYMBOL_GPL(vb2_write);

MODULE_DESCRIPTION("Driver helper framework for Video for Linux 2");
MODULE_AUTHOR("Pawel Osciak <pawel@osciak.com>, Marek Szyprowski");
MODULE_LICENSE("GPL");
