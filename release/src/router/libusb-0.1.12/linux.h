#ifndef __LINUX_H__
#define __LINUX_H__

#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

struct usb_ctrltransfer {
	/* keep in sync with usbdevice_fs.h:usbdevfs_ctrltransfer */
	u_int8_t  bRequestType;
	u_int8_t  bRequest;
	u_int16_t wValue;
	u_int16_t wIndex;
	u_int16_t wLength;

	u_int32_t timeout;	/* in milliseconds */

	/* pointer to data */
	void *data;
};

struct usb_bulktransfer {
	/* keep in sync with usbdevice_fs.h:usbdevfs_bulktransfer */
	unsigned int ep;
	unsigned int len;
	unsigned int timeout;	/* in milliseconds */

	/* pointer to data */
	void *data;
};

struct usb_setinterface {
	/* keep in sync with usbdevice_fs.h:usbdevfs_setinterface */
	unsigned int interface;
	unsigned int altsetting;
};

#define USB_MAXDRIVERNAME 255

struct usb_getdriver {
	unsigned int interface;
	char driver[USB_MAXDRIVERNAME + 1];
};

#define USB_URB_DISABLE_SPD	1
#define USB_URB_ISO_ASAP	2
#define USB_URB_QUEUE_BULK	0x10

#define USB_URB_TYPE_ISO	0
#define USB_URB_TYPE_INTERRUPT	1
#define USB_URB_TYPE_CONTROL	2
#define USB_URB_TYPE_BULK	3

struct usb_iso_packet_desc {
	unsigned int length;
	unsigned int actual_length;
	unsigned int status;
};

struct usb_urb {
	unsigned char type;
	unsigned char endpoint;
	int status;
	unsigned int flags;
	void *buffer;
	int buffer_length;
	int actual_length;
	int start_frame;
	int number_of_packets;
	int error_count;
	unsigned int signr;  /* signal to be sent on error, -1 if none should be sent */
	void *usercontext;
	struct usb_iso_packet_desc iso_frame_desc[0];
};

struct usb_connectinfo {
	unsigned int devnum;
	unsigned char slow;
};

struct usb_ioctl {
	int ifno;	/* interface 0..N ; negative numbers reserved */
	int ioctl_code;	/* MUST encode size + direction of data so the
			 * macros in <asm/ioctl.h> give correct values */
	void *data;	/* param buffer (in, or out) */
};

struct usb_hub_portinfo {
	unsigned char numports;
	unsigned char port[127];	/* port to device num mapping */
};

#define IOCTL_USB_CONTROL	_IOWR('U', 0, struct usb_ctrltransfer)
#define IOCTL_USB_BULK		_IOWR('U', 2, struct usb_bulktransfer)
#define IOCTL_USB_RESETEP	_IOR('U', 3, unsigned int)
#define IOCTL_USB_SETINTF	_IOR('U', 4, struct usb_setinterface)
#define IOCTL_USB_SETCONFIG	_IOR('U', 5, unsigned int)
#define IOCTL_USB_GETDRIVER	_IOW('U', 8, struct usb_getdriver)
#define IOCTL_USB_SUBMITURB	_IOR('U', 10, struct usb_urb)
#define IOCTL_USB_DISCARDURB	_IO('U', 11)
#define IOCTL_USB_REAPURB	_IOW('U', 12, void *)
#define IOCTL_USB_REAPURBNDELAY	_IOW('U', 13, void *)
#define IOCTL_USB_CLAIMINTF	_IOR('U', 15, unsigned int)
#define IOCTL_USB_RELEASEINTF	_IOR('U', 16, unsigned int)
#define IOCTL_USB_CONNECTINFO	_IOW('U', 17, struct usb_connectinfo)
#define IOCTL_USB_IOCTL         _IOWR('U', 18, struct usb_ioctl)
#define IOCTL_USB_HUB_PORTINFO	_IOR('U', 19, struct usb_hub_portinfo)
#define IOCTL_USB_RESET		_IO('U', 20)
#define IOCTL_USB_CLEAR_HALT	_IOR('U', 21, unsigned int)
#define IOCTL_USB_DISCONNECT	_IO('U', 22)
#define IOCTL_USB_CONNECT	_IO('U', 23)

/*
 * IOCTL_USB_HUB_PORTINFO, IOCTL_USB_DISCONNECT and IOCTL_USB_CONNECT
 * all work via IOCTL_USB_IOCTL
 */

#endif

