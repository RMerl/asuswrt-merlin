/*
 *   lirc_imon.c:  LIRC/VFD/LCD driver for SoundGraph iMON IR/VFD/LCD
 *		   including the iMON PAD model
 *
 *   Copyright(C) 2004  Venky Raju(dev@venky.ws)
 *   Copyright(C) 2009  Jarod Wilson <jarod@wilsonet.com>
 *
 *   lirc_imon is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#include <media/lirc.h>
#include <media/lirc_dev.h>


#define MOD_AUTHOR	"Venky Raju <dev@venky.ws>"
#define MOD_DESC	"Driver for SoundGraph iMON MultiMedia IR/Display"
#define MOD_NAME	"lirc_imon"
#define MOD_VERSION	"0.8"

#define DISPLAY_MINOR_BASE	144
#define DEVICE_NAME	"lcd%d"

#define BUF_CHUNK_SIZE	4
#define BUF_SIZE	128

#define BIT_DURATION	250	/* each bit received is 250us */

/*** P R O T O T Y P E S ***/

/* USB Callback prototypes */
static int imon_probe(struct usb_interface *interface,
		      const struct usb_device_id *id);
static void imon_disconnect(struct usb_interface *interface);
static void usb_rx_callback(struct urb *urb);
static void usb_tx_callback(struct urb *urb);

/* suspend/resume support */
static int imon_resume(struct usb_interface *intf);
static int imon_suspend(struct usb_interface *intf, pm_message_t message);

/* Display file_operations function prototypes */
static int display_open(struct inode *inode, struct file *file);
static int display_close(struct inode *inode, struct file *file);

/* VFD write operation */
static ssize_t vfd_write(struct file *file, const char *buf,
			 size_t n_bytes, loff_t *pos);

/* LIRC driver function prototypes */
static int ir_open(void *data);
static void ir_close(void *data);

/* Driver init/exit prototypes */
static int __init imon_init(void);
static void __exit imon_exit(void);

/*** G L O B A L S ***/
#define IMON_DATA_BUF_SZ	35

struct imon_context {
	struct usb_device *usbdev;
	/* Newer devices have two interfaces */
	int display;			/* not all controllers do */
	int display_isopen;		/* display port has been opened */
	int ir_isopen;			/* IR port open	*/
	int dev_present;		/* USB device presence */
	struct mutex ctx_lock;		/* to lock this object */
	wait_queue_head_t remove_ok;	/* For unexpected USB disconnects */

	int vfd_proto_6p;		/* some VFD require a 6th packet */

	struct lirc_driver *driver;
	struct usb_endpoint_descriptor *rx_endpoint;
	struct usb_endpoint_descriptor *tx_endpoint;
	struct urb *rx_urb;
	struct urb *tx_urb;
	unsigned char usb_rx_buf[8];
	unsigned char usb_tx_buf[8];

	struct rx_data {
		int count;		/* length of 0 or 1 sequence */
		int prev_bit;		/* logic level of sequence */
		int initial_space;	/* initial space flag */
	} rx;

	struct tx_t {
		unsigned char data_buf[IMON_DATA_BUF_SZ]; /* user data buffer */
		struct completion finished;	/* wait for write to finish */
		atomic_t busy;			/* write in progress */
		int status;			/* status of tx completion */
	} tx;
};

static const struct file_operations display_fops = {
	.owner		= THIS_MODULE,
	.open		= &display_open,
	.write		= &vfd_write,
	.release	= &display_close
};

/*
 * USB Device ID for iMON USB Control Boards
 *
 * The Windows drivers contain 6 different inf files, more or less one for
 * each new device until the 0x0034-0x0046 devices, which all use the same
 * driver. Some of the devices in the 34-46 range haven't been definitively
 * identified yet. Early devices have either a TriGem Computer, Inc. or a
 * Samsung vendor ID (0x0aa8 and 0x04e8 respectively), while all later
 * devices use the SoundGraph vendor ID (0x15c2).
 */
static struct usb_device_id imon_usb_id_table[] = {
	/* TriGem iMON (IR only) -- TG_iMON.inf */
	{ USB_DEVICE(0x0aa8, 0x8001) },

	/* SoundGraph iMON (IR only) -- sg_imon.inf */
	{ USB_DEVICE(0x04e8, 0xff30) },

	/* SoundGraph iMON VFD (IR & VFD) -- iMON_VFD.inf */
	{ USB_DEVICE(0x0aa8, 0xffda) },

	/* SoundGraph iMON SS (IR & VFD) -- iMON_SS.inf */
	{ USB_DEVICE(0x15c2, 0xffda) },

	{}
};

/* Some iMON VFD models requires a 6th packet for VFD writes */
static struct usb_device_id vfd_proto_6p_list[] = {
	{ USB_DEVICE(0x15c2, 0xffda) },
	{}
};

/* Some iMON devices have no lcd/vfd, don't set one up */
static struct usb_device_id ir_only_list[] = {
	{ USB_DEVICE(0x0aa8, 0x8001) },
	{ USB_DEVICE(0x04e8, 0xff30) },
	{}
};

/* USB Device data */
static struct usb_driver imon_driver = {
	.name		= MOD_NAME,
	.probe		= imon_probe,
	.disconnect	= imon_disconnect,
	.suspend	= imon_suspend,
	.resume		= imon_resume,
	.id_table	= imon_usb_id_table,
};

static struct usb_class_driver imon_class = {
	.name		= DEVICE_NAME,
	.fops		= &display_fops,
	.minor_base	= DISPLAY_MINOR_BASE,
};

/* to prevent races between open() and disconnect(), probing, etc */
static DEFINE_MUTEX(driver_lock);

static int debug;

/***  M O D U L E   C O D E ***/

MODULE_AUTHOR(MOD_AUTHOR);
MODULE_DESCRIPTION(MOD_DESC);
MODULE_VERSION(MOD_VERSION);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(usb, imon_usb_id_table);
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages: 0=no, 1=yes(default: no)");

static void free_imon_context(struct imon_context *context)
{
	struct device *dev = context->driver->dev;
	usb_free_urb(context->tx_urb);
	usb_free_urb(context->rx_urb);
	lirc_buffer_free(context->driver->rbuf);
	kfree(context->driver->rbuf);
	kfree(context->driver);
	kfree(context);

	dev_dbg(dev, "%s: iMON context freed\n", __func__);
}

static void deregister_from_lirc(struct imon_context *context)
{
	int retval;
	int minor = context->driver->minor;

	retval = lirc_unregister_driver(minor);
	if (retval)
		err("%s: unable to deregister from lirc(%d)",
			__func__, retval);
	else
		printk(KERN_INFO MOD_NAME ": Deregistered iMON driver "
		       "(minor:%d)\n", minor);

}

/**
 * Called when the Display device (e.g. /dev/lcd0)
 * is opened by the application.
 */
static int display_open(struct inode *inode, struct file *file)
{
	struct usb_interface *interface;
	struct imon_context *context = NULL;
	int subminor;
	int retval = 0;

	/* prevent races with disconnect */
	mutex_lock(&driver_lock);

	subminor = iminor(inode);
	interface = usb_find_interface(&imon_driver, subminor);
	if (!interface) {
		err("%s: could not find interface for minor %d",
		    __func__, subminor);
		retval = -ENODEV;
		goto exit;
	}
	context = usb_get_intfdata(interface);

	if (!context) {
		err("%s: no context found for minor %d",
					__func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	mutex_lock(&context->ctx_lock);

	if (!context->display) {
		err("%s: display not supported by device", __func__);
		retval = -ENODEV;
	} else if (context->display_isopen) {
		err("%s: display port is already open", __func__);
		retval = -EBUSY;
	} else {
		context->display_isopen = 1;
		file->private_data = context;
		dev_info(context->driver->dev, "display port opened\n");
	}

	mutex_unlock(&context->ctx_lock);

exit:
	mutex_unlock(&driver_lock);
	return retval;
}

/**
 * Called when the display device (e.g. /dev/lcd0)
 * is closed by the application.
 */
static int display_close(struct inode *inode, struct file *file)
{
	struct imon_context *context = NULL;
	int retval = 0;

	context = (struct imon_context *)file->private_data;

	if (!context) {
		err("%s: no context for device", __func__);
		return -ENODEV;
	}

	mutex_lock(&context->ctx_lock);

	if (!context->display) {
		err("%s: display not supported by device", __func__);
		retval = -ENODEV;
	} else if (!context->display_isopen) {
		err("%s: display is not open", __func__);
		retval = -EIO;
	} else {
		context->display_isopen = 0;
		dev_info(context->driver->dev, "display port closed\n");
		if (!context->dev_present && !context->ir_isopen) {
			/*
			 * Device disconnected before close and IR port is not
			 * open. If IR port is open, context will be deleted by
			 * ir_close.
			 */
			mutex_unlock(&context->ctx_lock);
			free_imon_context(context);
			return retval;
		}
	}

	mutex_unlock(&context->ctx_lock);
	return retval;
}

/**
 * Sends a packet to the device -- this function must be called
 * with context->ctx_lock held.
 */
static int send_packet(struct imon_context *context)
{
	unsigned int pipe;
	int interval = 0;
	int retval = 0;
	struct usb_ctrlrequest *control_req = NULL;

	/* Check if we need to use control or interrupt urb */
	pipe = usb_sndintpipe(context->usbdev,
			      context->tx_endpoint->bEndpointAddress);
	interval = context->tx_endpoint->bInterval;

	usb_fill_int_urb(context->tx_urb, context->usbdev, pipe,
			 context->usb_tx_buf,
			 sizeof(context->usb_tx_buf),
			 usb_tx_callback, context, interval);

	context->tx_urb->actual_length = 0;

	init_completion(&context->tx.finished);
	atomic_set(&(context->tx.busy), 1);

	retval = usb_submit_urb(context->tx_urb, GFP_KERNEL);
	if (retval) {
		atomic_set(&(context->tx.busy), 0);
		err("%s: error submitting urb(%d)", __func__, retval);
	} else {
		/* Wait for transmission to complete (or abort) */
		mutex_unlock(&context->ctx_lock);
		retval = wait_for_completion_interruptible(
				&context->tx.finished);
		if (retval)
			err("%s: task interrupted", __func__);
		mutex_lock(&context->ctx_lock);

		retval = context->tx.status;
		if (retval)
			err("%s: packet tx failed (%d)", __func__, retval);
	}

	kfree(control_req);

	return retval;
}

/**
 * Writes data to the VFD.  The iMON VFD is 2x16 characters
 * and requires data in 5 consecutive USB interrupt packets,
 * each packet but the last carrying 7 bytes.
 *
 * I don't know if the VFD board supports features such as
 * scrolling, clearing rows, blanking, etc. so at
 * the caller must provide a full screen of data.  If fewer
 * than 32 bytes are provided spaces will be appended to
 * generate a full screen.
 */
static ssize_t vfd_write(struct file *file, const char *buf,
			 size_t n_bytes, loff_t *pos)
{
	int i;
	int offset;
	int seq;
	int retval = 0;
	struct imon_context *context;
	const unsigned char vfd_packet6[] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
	int *data_buf;

	context = (struct imon_context *)file->private_data;
	if (!context) {
		err("%s: no context for device", __func__);
		return -ENODEV;
	}

	mutex_lock(&context->ctx_lock);

	if (!context->dev_present) {
		err("%s: no iMON device present", __func__);
		retval = -ENODEV;
		goto exit;
	}

	if (n_bytes <= 0 || n_bytes > IMON_DATA_BUF_SZ - 3) {
		err("%s: invalid payload size", __func__);
		retval = -EINVAL;
		goto exit;
	}

	data_buf = memdup_user(buf, n_bytes);
	if (IS_ERR(data_buf)) {
		retval = PTR_ERR(data_buf);
		goto exit;
	}

	memcpy(context->tx.data_buf, data_buf, n_bytes);

	/* Pad with spaces */
	for (i = n_bytes; i < IMON_DATA_BUF_SZ - 3; ++i)
		context->tx.data_buf[i] = ' ';

	for (i = IMON_DATA_BUF_SZ - 3; i < IMON_DATA_BUF_SZ; ++i)
		context->tx.data_buf[i] = 0xFF;

	offset = 0;
	seq = 0;

	do {
		memcpy(context->usb_tx_buf, context->tx.data_buf + offset, 7);
		context->usb_tx_buf[7] = (unsigned char) seq;

		retval = send_packet(context);
		if (retval) {
			err("%s: send packet failed for packet #%d",
					__func__, seq/2);
			goto exit;
		} else {
			seq += 2;
			offset += 7;
		}

	} while (offset < IMON_DATA_BUF_SZ);

	if (context->vfd_proto_6p) {
		/* Send packet #6 */
		memcpy(context->usb_tx_buf, &vfd_packet6, sizeof(vfd_packet6));
		context->usb_tx_buf[7] = (unsigned char) seq;
		retval = send_packet(context);
		if (retval)
			err("%s: send packet failed for packet #%d",
					__func__, seq/2);
	}

exit:
	mutex_unlock(&context->ctx_lock);

	return (!retval) ? n_bytes : retval;
}

/**
 * Callback function for USB core API: transmit data
 */
static void usb_tx_callback(struct urb *urb)
{
	struct imon_context *context;

	if (!urb)
		return;
	context = (struct imon_context *)urb->context;
	if (!context)
		return;

	context->tx.status = urb->status;

	/* notify waiters that write has finished */
	atomic_set(&context->tx.busy, 0);
	complete(&context->tx.finished);

	return;
}

/**
 * Called by lirc_dev when the application opens /dev/lirc
 */
static int ir_open(void *data)
{
	int retval = 0;
	struct imon_context *context;

	/* prevent races with disconnect */
	mutex_lock(&driver_lock);

	context = (struct imon_context *)data;

	/* initial IR protocol decode variables */
	context->rx.count = 0;
	context->rx.initial_space = 1;
	context->rx.prev_bit = 0;

	context->ir_isopen = 1;
	dev_info(context->driver->dev, "IR port opened\n");

	mutex_unlock(&driver_lock);
	return retval;
}

/**
 * Called by lirc_dev when the application closes /dev/lirc
 */
static void ir_close(void *data)
{
	struct imon_context *context;

	context = (struct imon_context *)data;
	if (!context) {
		err("%s: no context for device", __func__);
		return;
	}

	mutex_lock(&context->ctx_lock);

	context->ir_isopen = 0;
	dev_info(context->driver->dev, "IR port closed\n");

	if (!context->dev_present) {
		/*
		 * Device disconnected while IR port was still open. Driver
		 * was not deregistered at disconnect time, so do it now.
		 */
		deregister_from_lirc(context);

		if (!context->display_isopen) {
			mutex_unlock(&context->ctx_lock);
			free_imon_context(context);
			return;
		}
		/*
		 * If display port is open, context will be deleted by
		 * display_close
		 */
	}

	mutex_unlock(&context->ctx_lock);
	return;
}

/**
 * Convert bit count to time duration (in us) and submit
 * the value to lirc_dev.
 */
static void submit_data(struct imon_context *context)
{
	unsigned char buf[4];
	int value = context->rx.count;
	int i;

	dev_dbg(context->driver->dev, "submitting data to LIRC\n");

	value *= BIT_DURATION;
	value &= PULSE_MASK;
	if (context->rx.prev_bit)
		value |= PULSE_BIT;

	for (i = 0; i < 4; ++i)
		buf[i] = value>>(i*8);

	lirc_buffer_write(context->driver->rbuf, buf);
	wake_up(&context->driver->rbuf->wait_poll);
	return;
}

static inline int tv2int(const struct timeval *a, const struct timeval *b)
{
	int usecs = 0;
	int sec   = 0;

	if (b->tv_usec > a->tv_usec) {
		usecs = 1000000;
		sec--;
	}

	usecs += a->tv_usec - b->tv_usec;

	sec += a->tv_sec - b->tv_sec;
	sec *= 1000;
	usecs /= 1000;
	sec += usecs;

	if (sec < 0)
		sec = 1000;

	return sec;
}

/**
 * Process the incoming packet
 */
static void imon_incoming_packet(struct imon_context *context,
				 struct urb *urb, int intf)
{
	int len = urb->actual_length;
	unsigned char *buf = urb->transfer_buffer;
	struct device *dev = context->driver->dev;
	int octet, bit;
	unsigned char mask;
	int i, chunk_num;

	/*
	 * just bail out if no listening IR client
	 */
	if (!context->ir_isopen)
		return;

	if (len != 8) {
		dev_warn(dev, "imon %s: invalid incoming packet "
			 "size (len = %d, intf%d)\n", __func__, len, intf);
		return;
	}

	if (debug) {
		printk(KERN_INFO "raw packet: ");
		for (i = 0; i < len; ++i)
			printk("%02x ", buf[i]);
		printk("\n");
	}

	/*
	 * Translate received data to pulse and space lengths.
	 * Received data is active low, i.e. pulses are 0 and
	 * spaces are 1.
	 *
	 * My original algorithm was essentially similar to
	 * Changwoo Ryu's with the exception that he switched
	 * the incoming bits to active high and also fed an
	 * initial space to LIRC at the start of a new sequence
	 * if the previous bit was a pulse.
	 *
	 * I've decided to adopt his algorithm.
	 */

	if (buf[7] == 1 && context->rx.initial_space) {
		/* LIRC requires a leading space */
		context->rx.prev_bit = 0;
		context->rx.count = 4;
		submit_data(context);
		context->rx.count = 0;
	}

	for (octet = 0; octet < 5; ++octet) {
		mask = 0x80;
		for (bit = 0; bit < 8; ++bit) {
			int curr_bit = !(buf[octet] & mask);
			if (curr_bit != context->rx.prev_bit) {
				if (context->rx.count) {
					submit_data(context);
					context->rx.count = 0;
				}
				context->rx.prev_bit = curr_bit;
			}
			++context->rx.count;
			mask >>= 1;
		}
	}

	if (chunk_num == 10) {
		if (context->rx.count) {
			submit_data(context);
			context->rx.count = 0;
		}
		context->rx.initial_space = context->rx.prev_bit;
	}
}

/**
 * Callback function for USB core API: receive data
 */
static void usb_rx_callback(struct urb *urb)
{
	struct imon_context *context;
	unsigned char *buf;
	int len;
	int intfnum = 0;

	if (!urb)
		return;

	context = (struct imon_context *)urb->context;
	if (!context)
		return;

	buf = urb->transfer_buffer;
	len = urb->actual_length;

	switch (urb->status) {
	case -ENOENT:		/* usbcore unlink successful! */
		return;

	case 0:
		imon_incoming_packet(context, urb, intfnum);
		break;

	default:
		dev_warn(context->driver->dev, "imon %s: status(%d): ignored\n",
			 __func__, urb->status);
		break;
	}

	usb_submit_urb(context->rx_urb, GFP_ATOMIC);

	return;
}

/**
 * Callback function for USB core API: Probe
 */
static int imon_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_device *usbdev = NULL;
	struct usb_host_interface *iface_desc = NULL;
	struct usb_endpoint_descriptor *rx_endpoint = NULL;
	struct usb_endpoint_descriptor *tx_endpoint = NULL;
	struct urb *rx_urb = NULL;
	struct urb *tx_urb = NULL;
	struct lirc_driver *driver = NULL;
	struct lirc_buffer *rbuf = NULL;
	struct device *dev = &interface->dev;
	int ifnum;
	int lirc_minor = 0;
	int num_endpts;
	int retval = 0;
	int display_ep_found = 0;
	int ir_ep_found = 0;
	int alloc_status = 0;
	int vfd_proto_6p = 0;
	int code_length;
	struct imon_context *context = NULL;
	int i;
	u16 vendor, product;

	context = kzalloc(sizeof(struct imon_context), GFP_KERNEL);
	if (!context) {
		err("%s: kzalloc failed for context", __func__);
		alloc_status = 1;
		goto alloc_status_switch;
	}

	/*
	 * Try to auto-detect the type of display if the user hasn't set
	 * it by hand via the display_type modparam. Default is VFD.
	 */
	if (usb_match_id(interface, ir_only_list))
		context->display = 0;
	else
		context->display = 1;

	code_length = BUF_CHUNK_SIZE * 8;

	usbdev     = usb_get_dev(interface_to_usbdev(interface));
	iface_desc = interface->cur_altsetting;
	num_endpts = iface_desc->desc.bNumEndpoints;
	ifnum      = iface_desc->desc.bInterfaceNumber;
	vendor     = le16_to_cpu(usbdev->descriptor.idVendor);
	product    = le16_to_cpu(usbdev->descriptor.idProduct);

	dev_dbg(dev, "%s: found iMON device (%04x:%04x, intf%d)\n",
		__func__, vendor, product, ifnum);

	/* prevent races probing devices w/multiple interfaces */
	mutex_lock(&driver_lock);

	/*
	 * Scan the endpoint list and set:
	 *	first input endpoint = IR endpoint
	 *	first output endpoint = display endpoint
	 */
	for (i = 0; i < num_endpts && !(ir_ep_found && display_ep_found); ++i) {
		struct usb_endpoint_descriptor *ep;
		int ep_dir;
		int ep_type;
		ep = &iface_desc->endpoint[i].desc;
		ep_dir = ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		ep_type = ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

		if (!ir_ep_found &&
			ep_dir == USB_DIR_IN &&
			ep_type == USB_ENDPOINT_XFER_INT) {

			rx_endpoint = ep;
			ir_ep_found = 1;
			dev_dbg(dev, "%s: found IR endpoint\n", __func__);

		} else if (!display_ep_found && ep_dir == USB_DIR_OUT &&
			   ep_type == USB_ENDPOINT_XFER_INT) {
			tx_endpoint = ep;
			display_ep_found = 1;
			dev_dbg(dev, "%s: found display endpoint\n", __func__);
		}
	}

	/*
	 * Some iMON receivers have no display. Unfortunately, it seems
	 * that SoundGraph recycles device IDs between devices both with
	 * and without... :\
	 */
	if (context->display == 0) {
		display_ep_found = 0;
		dev_dbg(dev, "%s: device has no display\n", __func__);
	}

	/* Input endpoint is mandatory */
	if (!ir_ep_found) {
		err("%s: no valid input (IR) endpoint found.", __func__);
		retval = -ENODEV;
		alloc_status = 2;
		goto alloc_status_switch;
	}

	/* Determine if display requires 6 packets */
	if (display_ep_found) {
		if (usb_match_id(interface, vfd_proto_6p_list))
			vfd_proto_6p = 1;

		dev_dbg(dev, "%s: vfd_proto_6p: %d\n",
			__func__, vfd_proto_6p);
	}

	driver = kzalloc(sizeof(struct lirc_driver), GFP_KERNEL);
	if (!driver) {
		err("%s: kzalloc failed for lirc_driver", __func__);
		alloc_status = 2;
		goto alloc_status_switch;
	}
	rbuf = kmalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
	if (!rbuf) {
		err("%s: kmalloc failed for lirc_buffer", __func__);
		alloc_status = 3;
		goto alloc_status_switch;
	}
	if (lirc_buffer_init(rbuf, BUF_CHUNK_SIZE, BUF_SIZE)) {
		err("%s: lirc_buffer_init failed", __func__);
		alloc_status = 4;
		goto alloc_status_switch;
	}
	rx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!rx_urb) {
		err("%s: usb_alloc_urb failed for IR urb", __func__);
		alloc_status = 5;
		goto alloc_status_switch;
	}
	tx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!tx_urb) {
		err("%s: usb_alloc_urb failed for display urb",
		    __func__);
		alloc_status = 6;
		goto alloc_status_switch;
	}

	mutex_init(&context->ctx_lock);
	context->vfd_proto_6p = vfd_proto_6p;

	strcpy(driver->name, MOD_NAME);
	driver->minor = -1;
	driver->code_length = sizeof(int) * 8;
	driver->sample_rate = 0;
	driver->features = LIRC_CAN_REC_MODE2;
	driver->data = context;
	driver->rbuf = rbuf;
	driver->set_use_inc = ir_open;
	driver->set_use_dec = ir_close;
	driver->dev = &interface->dev;
	driver->owner = THIS_MODULE;

	mutex_lock(&context->ctx_lock);

	context->driver = driver;
	/* start out in keyboard mode */

	lirc_minor = lirc_register_driver(driver);
	if (lirc_minor < 0) {
		err("%s: lirc_register_driver failed", __func__);
		alloc_status = 7;
		goto alloc_status_switch;
	} else
		dev_info(dev, "Registered iMON driver "
			 "(lirc minor: %d)\n", lirc_minor);

	/* Needed while unregistering! */
	driver->minor = lirc_minor;

	context->usbdev = usbdev;
	context->dev_present = 1;
	context->rx_endpoint = rx_endpoint;
	context->rx_urb = rx_urb;

	/*
	 * tx is used to send characters to lcd/vfd, associate RF
	 * remotes, set IR protocol, and maybe more...
	 */
	context->tx_endpoint = tx_endpoint;
	context->tx_urb = tx_urb;

	if (display_ep_found)
		context->display = 1;

	usb_fill_int_urb(context->rx_urb, context->usbdev,
		usb_rcvintpipe(context->usbdev,
			context->rx_endpoint->bEndpointAddress),
		context->usb_rx_buf, sizeof(context->usb_rx_buf),
		usb_rx_callback, context,
		context->rx_endpoint->bInterval);

	retval = usb_submit_urb(context->rx_urb, GFP_KERNEL);

	if (retval) {
		err("%s: usb_submit_urb failed for intf0 (%d)",
		    __func__, retval);
		mutex_unlock(&context->ctx_lock);
		goto exit;
	}

	usb_set_intfdata(interface, context);

	if (context->display && ifnum == 0) {
		dev_dbg(dev, "%s: Registering iMON display with sysfs\n",
			__func__);

		if (usb_register_dev(interface, &imon_class)) {
			/* Not a fatal error, so ignore */
			dev_info(dev, "%s: could not get a minor number for "
				 "display\n", __func__);
		}
	}

	dev_info(dev, "iMON device (%04x:%04x, intf%d) on "
		 "usb<%d:%d> initialized\n", vendor, product, ifnum,
		 usbdev->bus->busnum, usbdev->devnum);

alloc_status_switch:
	mutex_unlock(&context->ctx_lock);

	switch (alloc_status) {
	case 7:
		usb_free_urb(tx_urb);
	case 6:
		usb_free_urb(rx_urb);
	case 5:
		if (rbuf)
			lirc_buffer_free(rbuf);
	case 4:
		kfree(rbuf);
	case 3:
		kfree(driver);
	case 2:
		kfree(context);
		context = NULL;
	case 1:
		if (retval != -ENODEV)
			retval = -ENOMEM;
		break;
	case 0:
		retval = 0;
	}

exit:
	mutex_unlock(&driver_lock);

	return retval;
}

/**
 * Callback function for USB core API: disconnect
 */
static void imon_disconnect(struct usb_interface *interface)
{
	struct imon_context *context;
	int ifnum;

	/* prevent races with ir_open()/display_open() */
	mutex_lock(&driver_lock);

	context = usb_get_intfdata(interface);
	ifnum = interface->cur_altsetting->desc.bInterfaceNumber;

	mutex_lock(&context->ctx_lock);

	usb_set_intfdata(interface, NULL);

	/* Abort ongoing write */
	if (atomic_read(&context->tx.busy)) {
		usb_kill_urb(context->tx_urb);
		complete_all(&context->tx.finished);
	}

	context->dev_present = 0;
	usb_kill_urb(context->rx_urb);
	if (context->display)
		usb_deregister_dev(interface, &imon_class);

	if (!context->ir_isopen && !context->dev_present) {
		deregister_from_lirc(context);
		mutex_unlock(&context->ctx_lock);
		if (!context->display_isopen)
			free_imon_context(context);
	} else
		mutex_unlock(&context->ctx_lock);

	mutex_unlock(&driver_lock);

	printk(KERN_INFO "%s: iMON device (intf%d) disconnected\n",
	       __func__, ifnum);
}

static int imon_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct imon_context *context = usb_get_intfdata(intf);

	usb_kill_urb(context->rx_urb);

	return 0;
}

static int imon_resume(struct usb_interface *intf)
{
	int rc = 0;
	struct imon_context *context = usb_get_intfdata(intf);

	usb_fill_int_urb(context->rx_urb, context->usbdev,
		usb_rcvintpipe(context->usbdev,
			context->rx_endpoint->bEndpointAddress),
		context->usb_rx_buf, sizeof(context->usb_rx_buf),
		usb_rx_callback, context,
		context->rx_endpoint->bInterval);

	rc = usb_submit_urb(context->rx_urb, GFP_ATOMIC);

	return rc;
}

static int __init imon_init(void)
{
	int rc;

	printk(KERN_INFO MOD_NAME ": " MOD_DESC ", v" MOD_VERSION "\n");

	rc = usb_register(&imon_driver);
	if (rc) {
		err("%s: usb register failed(%d)", __func__, rc);
		return -ENODEV;
	}

	return 0;
}

static void __exit imon_exit(void)
{
	usb_deregister(&imon_driver);
	printk(KERN_INFO MOD_NAME ": module removed. Goodbye!\n");
}

module_init(imon_init);
module_exit(imon_exit);
