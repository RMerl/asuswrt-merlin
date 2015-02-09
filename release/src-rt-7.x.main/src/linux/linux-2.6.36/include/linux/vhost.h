#ifndef _LINUX_VHOST_H
#define _LINUX_VHOST_H
/* Userspace interface for in-kernel virtio accelerators. */

/* vhost is used to reduce the number of system calls involved in virtio.
 *
 * Existing virtio net code is used in the guest without modification.
 *
 * This header includes interface used by userspace hypervisor for
 * device configuration.
 */

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/ioctl.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>

struct vhost_vring_state {
	unsigned int index;
	unsigned int num;
};

struct vhost_vring_file {
	unsigned int index;
	int fd; /* Pass -1 to unbind from file. */

};

struct vhost_vring_addr {
	unsigned int index;
	/* Option flags. */
	unsigned int flags;
	/* Flag values: */
	/* Whether log address is valid. If set enables logging. */
#define VHOST_VRING_F_LOG 0

	/* Start of array of descriptors (virtually contiguous) */
	__u64 desc_user_addr;
	/* Used structure address. Must be 32 bit aligned */
	__u64 used_user_addr;
	/* Available structure address. Must be 16 bit aligned */
	__u64 avail_user_addr;
	/* Logging support. */
	/* Log writes to used structure, at offset calculated from specified
	 * address. Address must be 32 bit aligned. */
	__u64 log_guest_addr;
};

struct vhost_memory_region {
	__u64 guest_phys_addr;
	__u64 memory_size; /* bytes */
	__u64 userspace_addr;
	__u64 flags_padding; /* No flags are currently specified. */
};

/* All region addresses and sizes must be 4K aligned. */
#define VHOST_PAGE_SIZE 0x1000

struct vhost_memory {
	__u32 nregions;
	__u32 padding;
	struct vhost_memory_region regions[0];
};

/* ioctls */

#define VHOST_VIRTIO 0xAF

/* Features bitmask for forward compatibility.  Transport bits are used for
 * vhost specific features. */
#define VHOST_GET_FEATURES	_IOR(VHOST_VIRTIO, 0x00, __u64)
#define VHOST_SET_FEATURES	_IOW(VHOST_VIRTIO, 0x00, __u64)

/* Set current process as the (exclusive) owner of this file descriptor.  This
 * must be called before any other vhost command.  Further calls to
 * VHOST_OWNER_SET fail until VHOST_OWNER_RESET is called. */
#define VHOST_SET_OWNER _IO(VHOST_VIRTIO, 0x01)
/* Give up ownership, and reset the device to default values.
 * Allows subsequent call to VHOST_OWNER_SET to succeed. */
#define VHOST_RESET_OWNER _IO(VHOST_VIRTIO, 0x02)

/* Set up/modify memory layout */
#define VHOST_SET_MEM_TABLE	_IOW(VHOST_VIRTIO, 0x03, struct vhost_memory)

/* Write logging setup. */
/* Memory writes can optionally be logged by setting bit at an offset
 * (calculated from the physical address) from specified log base.
 * The bit is set using an atomic 32 bit operation. */
/* Set base address for logging. */
#define VHOST_SET_LOG_BASE _IOW(VHOST_VIRTIO, 0x04, __u64)
/* Specify an eventfd file descriptor to signal on log write. */
#define VHOST_SET_LOG_FD _IOW(VHOST_VIRTIO, 0x07, int)

/* Ring setup. */
/* Set number of descriptors in ring. This parameter can not
 * be modified while ring is running (bound to a device). */
#define VHOST_SET_VRING_NUM _IOW(VHOST_VIRTIO, 0x10, struct vhost_vring_state)
/* Set addresses for the ring. */
#define VHOST_SET_VRING_ADDR _IOW(VHOST_VIRTIO, 0x11, struct vhost_vring_addr)
/* Base value where queue looks for available descriptors */
#define VHOST_SET_VRING_BASE _IOW(VHOST_VIRTIO, 0x12, struct vhost_vring_state)
/* Get accessor: reads index, writes value in num */
#define VHOST_GET_VRING_BASE _IOWR(VHOST_VIRTIO, 0x12, struct vhost_vring_state)

/* The following ioctls use eventfd file descriptors to signal and poll
 * for events. */

/* Set eventfd to poll for added buffers */
#define VHOST_SET_VRING_KICK _IOW(VHOST_VIRTIO, 0x20, struct vhost_vring_file)
/* Set eventfd to signal when buffers have beed used */
#define VHOST_SET_VRING_CALL _IOW(VHOST_VIRTIO, 0x21, struct vhost_vring_file)
/* Set eventfd to signal an error */
#define VHOST_SET_VRING_ERR _IOW(VHOST_VIRTIO, 0x22, struct vhost_vring_file)

/* VHOST_NET specific defines */

/* Attach virtio net ring to a raw socket, or tap device.
 * The socket must be already bound to an ethernet device, this device will be
 * used for transmit.  Pass fd -1 to unbind from the socket and the transmit
 * device.  This can be used to stop the ring (e.g. for migration). */
#define VHOST_NET_SET_BACKEND _IOW(VHOST_VIRTIO, 0x30, struct vhost_vring_file)

/* Feature bits */
/* Log all write descriptors. Can be changed while device is active. */
#define VHOST_F_LOG_ALL 26
/* vhost-net should add virtio_net_hdr for RX, and strip for TX packets. */
#define VHOST_NET_F_VIRTIO_NET_HDR 27

#endif
