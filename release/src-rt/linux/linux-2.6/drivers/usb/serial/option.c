/*
  USB Driver for GSM modems

  Copyright (C) 2005  Matthias Urlichs <smurf@smurf.noris.de>

  This driver is free software; you can redistribute it and/or modify
  it under the terms of Version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  Portions copied from the Keyspan driver by Hugh Blemings <hugh@blemings.org>

  History: see the git log.

  Work sponsored by: Sigos GmbH, Germany <info@sigos.de>

  This driver exists because the "normal" serial driver doesn't work too well
  with GSM modems. Issues:
  - data loss -- one single Receive URB is not nearly enough
  - nonstandard flow (Option devices) control
  - controlling the baud rate doesn't make sense

  This driver is named "option" because the most common device it's
  used for is a PC-Card (with an internal OHCI-USB interface, behind
  which the GSM interface sits), made by Option Inc.

  Some of the "one port" devices actually exhibit multiple USB instances
  on the USB bus. This is not a bug, these ports are used for different
  device features.
*/

#define DRIVER_VERSION "v0.7.2"
#define DRIVER_AUTHOR "Matthias Urlichs <smurf@smurf.noris.de>"
#define DRIVER_DESC "USB Driver for GSM modems"

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

/* Function prototypes */
static int  option_probe(struct usb_serial *serial,
			const struct usb_device_id *id);
static int  option_open(struct usb_serial_port *port, struct file *filp);
static void option_close(struct usb_serial_port *port, struct file *filp);
static int  option_startup(struct usb_serial *serial);
static void option_shutdown(struct usb_serial *serial);
static void option_rx_throttle(struct usb_serial_port *port);
static void option_rx_unthrottle(struct usb_serial_port *port);
static int  option_write_room(struct usb_serial_port *port);

static void option_instat_callback(struct urb *urb);

static int option_write(struct usb_serial_port *port,
			const unsigned char *buf, int count);

static int  option_chars_in_buffer(struct usb_serial_port *port);
static int  option_ioctl(struct usb_serial_port *port, struct file *file,
			unsigned int cmd, unsigned long arg);
static void option_set_termios(struct usb_serial_port *port,
				struct ktermios *old);
static void option_break_ctl(struct usb_serial_port *port, int break_state);
static int  option_tiocmget(struct usb_serial_port *port, struct file *file);
static int  option_tiocmset(struct usb_serial_port *port, struct file *file,
				unsigned int set, unsigned int clear);
static int  option_send_setup(struct usb_serial_port *port);


/* some devices interfaces need special handling due to a number of reasons */
enum option_blacklist_reason {
	OPTION_BLACKLIST_NONE = 0,
	OPTION_BLACKLIST_SENDSETUP = 1,
	OPTION_BLACKLIST_RESERVED_IF = 2
};

#include "option_vid_pid_list.h"
MODULE_DEVICE_TABLE(usb, option_ids);

static struct usb_driver option_driver = {
	.name       = "option",
	.probe      = usb_serial_probe,
	.disconnect = usb_serial_disconnect,
	.id_table   = option_ids,
	.no_dynamic_id = 	1,
};

/* The card has three separate interfaces, which the serial driver
 * recognizes separately, thus num_port=1.
 */

static struct usb_serial_driver option_1port_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"option1",
	},
	.description       = "GSM modem (1-port)",
	.usb_driver        = &option_driver,
	.id_table          = option_ids,
	.num_interrupt_in  = NUM_DONT_CARE,
	.num_bulk_in       = NUM_DONT_CARE,
	.num_bulk_out      = NUM_DONT_CARE,
	.num_ports         = 1,
	.probe             = option_probe,
	.open              = option_open,
	.close             = option_close,
	.write             = option_write,
	.write_room        = option_write_room,
	.chars_in_buffer   = option_chars_in_buffer,
	.throttle          = option_rx_throttle,
	.unthrottle        = option_rx_unthrottle,
	.ioctl             = option_ioctl,
	.set_termios       = option_set_termios,
	.break_ctl         = option_break_ctl,
	.tiocmget          = option_tiocmget,
	.tiocmset          = option_tiocmset,
	.attach            = option_startup,
	.shutdown          = option_shutdown,
	.read_int_callback = option_instat_callback,
};

static int debug;
static __u16 vendor  = 0;
static __u16 product = 0;

/* per port private data */

#define N_IN_URB 4
#define N_OUT_URB 4
#define IN_BUFLEN 4096
#define OUT_BUFLEN 4096

struct option_port_private {
	/* Input endpoints and buffer for this port */
	struct urb *in_urbs[N_IN_URB];
	u8 *in_buffer[N_IN_URB];
	/* Output endpoints and buffer for this port */
	struct urb *out_urbs[N_OUT_URB];
	u8 *out_buffer[N_OUT_URB];
	unsigned long out_busy;		/* Bit vector of URBs in use */

	/* Settings for the port */
	int rts_state;	/* Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;	/* Handshaking pins (inputs) */
	int dsr_state;
	int dcd_state;
	int ri_state;

	unsigned long tx_start_time[N_OUT_URB];
};

/* Functions used by new usb-serial code. */
static int __init option_init(void)
{
	int retval;

	/* Add user specified VID/PID to reserved element of table. */
	if (vendor > 0 && product > 0) {
		int i = sizeof(option_ids)/sizeof(option_ids[0]) - 2;
		option_ids[i].idVendor = vendor;
		option_ids[i].idProduct = product;
		option_ids[i].match_flags = USB_DEVICE_ID_MATCH_DEVICE;
	}

	retval = usb_serial_register(&option_1port_device);
	if (retval)
		goto failed_1port_device_register;
	retval = usb_register(&option_driver);
	if (retval)
		goto failed_driver_register;

	info(DRIVER_DESC ": " DRIVER_VERSION);

	return 0;

failed_driver_register:
	usb_serial_deregister (&option_1port_device);
failed_1port_device_register:
	return retval;
}

static void __exit option_exit(void)
{
	usb_deregister (&option_driver);
	usb_serial_deregister (&option_1port_device);
}

module_init(option_init);
module_exit(option_exit);

static int option_probe(struct usb_serial *serial,
			const struct usb_device_id *id)
{
	/* D-Link DWM 652 still exposes CD-Rom emulation interface in modem mode */
	if (serial->dev->descriptor.idVendor == DLINK_VENDOR_ID &&
		serial->dev->descriptor.idProduct == DLINK_PRODUCT_DWM_652 &&
		serial->interface->cur_altsetting->desc.bInterfaceClass == 0x8)
		return -ENODEV;

	/* Bandrich modem and AT command interface is 0xff */
	if ((serial->dev->descriptor.idVendor == BANDRICH_VENDOR_ID ||
		serial->dev->descriptor.idVendor == PIRELLI_VENDOR_ID) &&
		serial->interface->cur_altsetting->desc.bInterfaceClass != 0xff)
		return -ENODEV;

	/* Don't bind network interfaces on Huawei K3765 & K4505 */
	if (serial->dev->descriptor.idVendor == HUAWEI_VENDOR_ID &&
		(serial->dev->descriptor.idProduct == HUAWEI_PRODUCT_K3765 ||
			serial->dev->descriptor.idProduct == HUAWEI_PRODUCT_K4505) &&
		serial->interface->cur_altsetting->desc.bInterfaceNumber == 1)
		return -ENODEV;

	/* Don't bind network interface on Samsung GT-B3730, it is handled by a separate module */
	if (serial->dev->descriptor.idVendor == SAMSUNG_VENDOR_ID &&
                (serial->dev->descriptor.idProduct == SAMSUNG_PRODUCT_GT_B3730 ||
                serial->dev->descriptor.idProduct == SAMSUNG_PRODUCT_GT_B1110) &&
		serial->interface->cur_altsetting->desc.bInterfaceClass != USB_CLASS_CDC_DATA)
		return -ENODEV;

	serial->private = (void *)id->driver_info;

	return 0;
}

static enum option_blacklist_reason is_blacklisted(const u8 ifnum,
				const struct option_blacklist_info *blacklist)
{
	const u8  *info;
	int i;

	if (blacklist) {
		info = blacklist->ifaceinfo;

		for (i = 0; i < blacklist->infolen; i++) {
			if (info[i] == ifnum)
				return blacklist->reason;
		}
	}
	return OPTION_BLACKLIST_NONE;
}

static void option_rx_throttle(struct usb_serial_port *port)
{
	dbg("%s", __FUNCTION__);
}

static void option_rx_unthrottle(struct usb_serial_port *port)
{
	dbg("%s", __FUNCTION__);
}

static void option_break_ctl(struct usb_serial_port *port, int break_state)
{
	/* Unfortunately, I don't know how to send a break */
	dbg("%s", __FUNCTION__);
}

static void option_set_termios(struct usb_serial_port *port,
			struct ktermios *old_termios)
{
	dbg("%s", __FUNCTION__);
	/* Doesn't support option setting */
	tty_termios_copy_hw(port->tty->termios, old_termios);
	option_send_setup(port);
}

static int option_tiocmget(struct usb_serial_port *port, struct file *file)
{
	unsigned int value;
	struct option_port_private *portdata;

	portdata = usb_get_serial_port_data(port);

	value = ((portdata->rts_state) ? TIOCM_RTS : 0) |
		((portdata->dtr_state) ? TIOCM_DTR : 0) |
		((portdata->cts_state) ? TIOCM_CTS : 0) |
		((portdata->dsr_state) ? TIOCM_DSR : 0) |
		((portdata->dcd_state) ? TIOCM_CAR : 0) |
		((portdata->ri_state) ? TIOCM_RNG : 0);

	return value;
}

static int option_tiocmset(struct usb_serial_port *port, struct file *file,
			unsigned int set, unsigned int clear)
{
	struct option_port_private *portdata;

	portdata = usb_get_serial_port_data(port);

	/* FIXME: what locks portdata fields ? */
	if (set & TIOCM_RTS)
		portdata->rts_state = 1;
	if (set & TIOCM_DTR)
		portdata->dtr_state = 1;

	if (clear & TIOCM_RTS)
		portdata->rts_state = 0;
	if (clear & TIOCM_DTR)
		portdata->dtr_state = 0;
	return option_send_setup(port);
}

static int option_ioctl(struct usb_serial_port *port, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}

/* Write */
static int option_write(struct usb_serial_port *port,
			const unsigned char *buf, int count)
{
	struct option_port_private *portdata;
	int i;
	int left, todo;
	struct urb *this_urb = NULL; /* spurious */
	int err;

	portdata = usb_get_serial_port_data(port);

	dbg("%s: write (%d chars)", __FUNCTION__, count);

	i = 0;
	left = count;
	for (i=0; left > 0 && i < N_OUT_URB; i++) {
		todo = left;
		if (todo > OUT_BUFLEN)
			todo = OUT_BUFLEN;

		this_urb = portdata->out_urbs[i];
		if (test_and_set_bit(i, &portdata->out_busy)) {
			if (time_before(jiffies,
					portdata->tx_start_time[i] + 10 * HZ))
				continue;
			usb_unlink_urb(this_urb);
			continue;
		}
		dbg("%s: endpoint %d buf %d", __FUNCTION__,
			usb_pipeendpoint(this_urb->pipe), i);

		/* send the data */
		memcpy (this_urb->transfer_buffer, buf, todo);
		this_urb->transfer_buffer_length = todo;

		err = usb_submit_urb(this_urb, GFP_ATOMIC);
		if (err) {
			dbg("usb_submit_urb %p (write bulk) failed "
				"(%d)", this_urb, err);
			clear_bit(i, &portdata->out_busy);
			continue;
		}
		portdata->tx_start_time[i] = jiffies;
		buf += todo;
		left -= todo;
	}

	count -= left;
	dbg("%s: wrote (did %d)", __FUNCTION__, count);
	return count;
}

static void option_indat_callback(struct urb *urb)
{
	int err;
	int endpoint;
	struct usb_serial_port *port;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int status = urb->status;

	dbg("%s: %p", __FUNCTION__, urb);

	endpoint = usb_pipeendpoint(urb->pipe);
	port = urb->context;

	switch (status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s: urb shutting down with status: %d on endpoint %02x.",
		    __FUNCTION__, status, endpoint);
		return;
	default:
		dbg("%s: nonzero status: %d on endpoint %02x.",
		    __FUNCTION__, status, endpoint);
		goto exit;
	}

	if (urb->actual_length) {
		tty = port->tty;
		tty_insert_flip_string(tty, data, urb->actual_length);
		tty_flip_buffer_push(tty);
	} else
		dbg("%s: empty read urb received", __FUNCTION__);

exit:
	/* Resubmit urb so we continue receiving */
	if (port->open_count) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err)
			printk(KERN_ERR "%s: resubmit read urb failed. "
				"(%d)", __FUNCTION__, err);
	}
	return;
}

static void option_outdat_callback(struct urb *urb)
{
	struct usb_serial_port *port;
	struct option_port_private *portdata;
	int i;

	dbg("%s", __FUNCTION__);

	port =  urb->context;

	usb_serial_port_softint(port);

	portdata = usb_get_serial_port_data(port);
	for (i = 0; i < N_OUT_URB; ++i) {
		if (portdata->out_urbs[i] == urb) {
			smp_mb__before_clear_bit();
			clear_bit(i, &portdata->out_busy);
			break;
		}
	}
}

static void option_instat_callback(struct urb *urb)
{
	int err;
	int status = urb->status;
	struct usb_serial_port *port =  urb->context;
	struct option_port_private *portdata = usb_get_serial_port_data(port);

	dbg("%s", __FUNCTION__);
	dbg("%s: urb %p port %p has data %p", __FUNCTION__,urb,port,portdata);

	if (status == 0) {
		struct usb_ctrlrequest *req_pkt =
				(struct usb_ctrlrequest *)urb->transfer_buffer;

		if (!req_pkt) {
			dbg("%s: NULL req_pkt\n", __FUNCTION__);
			return;
		}
		if ((req_pkt->bRequestType == 0xA1) &&
				(req_pkt->bRequest == 0x20)) {
			int old_dcd_state;
			unsigned char signals = *((unsigned char *)
					urb->transfer_buffer +
					sizeof(struct usb_ctrlrequest));

			dbg("%s: signal x%x", __FUNCTION__, signals);

			old_dcd_state = portdata->dcd_state;
			portdata->cts_state = 1;
			portdata->dcd_state = ((signals & 0x01) ? 1 : 0);
			portdata->dsr_state = ((signals & 0x02) ? 1 : 0);
			portdata->ri_state = ((signals & 0x08) ? 1 : 0);

			if (port->tty && !C_CLOCAL(port->tty) &&
					old_dcd_state && !portdata->dcd_state)
				tty_hangup(port->tty);
		} else {
			dbg("%s: type %x req %x", __FUNCTION__,
				req_pkt->bRequestType,req_pkt->bRequest);
		}
	} else
		dbg("%s: error %d", __FUNCTION__, status);

	/* Resubmit urb so we continue receiving IRQ data */
	if (status != -ESHUTDOWN && status != -ENOENT) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err)
			dbg("%s: resubmit intr urb failed. (%d)",
				__FUNCTION__, err);
	}
}

static int option_write_room(struct usb_serial_port *port)
{
	struct option_port_private *portdata;
	int i;
	int data_len = 0;
	struct urb *this_urb;

	portdata = usb_get_serial_port_data(port);

	for (i=0; i < N_OUT_URB; i++) {
		this_urb = portdata->out_urbs[i];
		if (this_urb && !test_bit(i, &portdata->out_busy))
			data_len += OUT_BUFLEN;
	}

	dbg("%s: %d", __FUNCTION__, data_len);
	return data_len;
}

static int option_chars_in_buffer(struct usb_serial_port *port)
{
	struct option_port_private *portdata;
	int i;
	int data_len = 0;
	struct urb *this_urb;

	portdata = usb_get_serial_port_data(port);


	for (i=0; i < N_OUT_URB; i++) {
		this_urb = portdata->out_urbs[i];
		/* FIXME: This locking is insufficient as this_urb may
		   go unused during the test */
		if (this_urb && test_bit(i, &portdata->out_busy))
			data_len += this_urb->transfer_buffer_length;
	}
	dbg("%s: %d", __FUNCTION__, data_len);
	return data_len;
}

static int option_open(struct usb_serial_port *port, struct file *filp)
{
	struct option_port_private *portdata;
	//struct usb_serial *serial = port->serial;
	int i, err;
	struct urb *urb;

	portdata = usb_get_serial_port_data(port);

	dbg("%s", __FUNCTION__);

	/* Set some sane defaults */
	portdata->rts_state = 1;
	portdata->dtr_state = 1;

	/* Start reading from the IN endpoint */
	for (i = 0; i < N_IN_URB; i++) {
		urb = portdata->in_urbs[i];
		if (! urb)
			continue;
		err = usb_submit_urb(urb, GFP_KERNEL);
		if (err) {
			dbg("%s: submit urb %d failed (%d) %d",
				__FUNCTION__, i, err,
				urb->transfer_buffer_length);
		}
	}

	option_send_setup(port);

	return (0);
}

static void option_close(struct usb_serial_port *port, struct file *filp)
{
	int i;
	struct usb_serial *serial = port->serial;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);
	portdata = usb_get_serial_port_data(port);

	portdata->rts_state = 0;
	portdata->dtr_state = 0;

	if (serial->dev) {
		option_send_setup(port);
		/* Stop reading/writing urbs */
		for (i = 0; i < N_IN_URB; i++)
			usb_kill_urb(portdata->in_urbs[i]);
		for (i = 0; i < N_OUT_URB; i++)
			usb_kill_urb(portdata->out_urbs[i]);
	}
	port->tty = NULL;
}

/* Helper functions used by option_setup_urbs */
static struct urb *option_setup_urb(struct usb_serial *serial, int endpoint,
		int dir, void *ctx, char *buf, int len,
		void (*callback)(struct urb *))
{
	struct urb *urb;

	if (endpoint == -1)
		return NULL;		/* endpoint not needed */

	urb = usb_alloc_urb(0, GFP_KERNEL);		/* No ISO */
	if (urb == NULL) {
		dbg("%s: alloc for endpoint %d failed.", __FUNCTION__, endpoint);
		return NULL;
	}

		/* Fill URB using supplied data. */
	usb_fill_bulk_urb(urb, serial->dev,
		      usb_sndbulkpipe(serial->dev, endpoint) | dir,
		      buf, len, callback, ctx);

	return urb;
}

/* Setup urbs */
static void option_setup_urbs(struct usb_serial *serial)
{
	int i,j;
	struct usb_serial_port *port;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);

	/* Do indat endpoints first */
		for (j = 0; j < N_IN_URB; ++j) {
			portdata->in_urbs[j] = option_setup_urb (serial,
                  	port->bulk_in_endpointAddress, USB_DIR_IN, port,
                  	portdata->in_buffer[j], IN_BUFLEN, option_indat_callback);
		}

		/* outdat endpoints */
		for (j = 0; j < N_OUT_URB; ++j) {
			portdata->out_urbs[j] = option_setup_urb (serial,
                  	port->bulk_out_endpointAddress, USB_DIR_OUT, port,
                  	portdata->out_buffer[j], OUT_BUFLEN, option_outdat_callback);
		}
	}
}


/** send RTS/DTR state to the port.
 *
 * This is exactly the same as SET_CONTROL_LINE_STATE from the PSTN
 * CDC.
*/
static int option_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct option_port_private *portdata;
	int ifNum = serial->interface->cur_altsetting->desc.bInterfaceNumber;
	dbg("%s", __FUNCTION__);

	if (is_blacklisted(ifNum,
			   (struct option_blacklist_info *) serial->private)
	    == OPTION_BLACKLIST_SENDSETUP) {
		dbg("No send_setup on blacklisted interface #%d\n", ifNum);
		return -EIO;
	}

	portdata = usb_get_serial_port_data(port);

	if (port->tty) {
		int val = 0;
		if (portdata->dtr_state)
			val |= 0x01;
		if (portdata->rts_state)
			val |= 0x02;

		return usb_control_msg(serial->dev,
				usb_rcvctrlpipe(serial->dev, 0),
				0x22,0x21,val,ifNum,NULL,0,USB_CTRL_SET_TIMEOUT);
	}

	return 0;
}

static int option_startup(struct usb_serial *serial)
{
	int i, j, err;
	struct usb_serial_port *port;
	struct option_port_private *portdata;
	u8 *buffer;

	dbg("%s", __FUNCTION__);

	/* Now setup per port private data */
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = kzalloc(sizeof(*portdata), GFP_KERNEL);
		if (!portdata) {
			dbg("%s: kmalloc for option_port_private (%d) failed!.",
					__FUNCTION__, i);
			return (1);
		}

		for (j = 0; j < N_IN_URB; j++) {
			buffer = (u8 *)__get_free_page(GFP_KERNEL);
			if (!buffer)
				goto bail_out_error;
			portdata->in_buffer[j] = buffer;
		}

		for (j = 0; j < N_OUT_URB; j++) {
			buffer = kmalloc(OUT_BUFLEN, GFP_KERNEL);
			if (!buffer)
				goto bail_out_error2;
			portdata->out_buffer[j] = buffer;
		}

		usb_set_serial_port_data(port, portdata);

		if (! port->interrupt_in_urb)
			continue;
		err = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
		if (err)
			dbg("%s: submit irq_in urb failed %d",
				__FUNCTION__, err);
	}

	option_setup_urbs(serial);

	return (0);

bail_out_error2:
	for (j = 0; j < N_OUT_URB; j++)
		kfree(portdata->out_buffer[j]);
bail_out_error:
	for (j = 0; j < N_IN_URB; j++)
		if (portdata->in_buffer[j])
			free_page((unsigned long)portdata->in_buffer[j]);
	kfree(portdata);
	return 1;
}

static void option_shutdown(struct usb_serial *serial)
{
	int i, j;
	struct usb_serial_port *port;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	/* Stop reading/writing urbs */
	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);
		for (j = 0; j < N_IN_URB; j++)
			usb_kill_urb(portdata->in_urbs[j]);
		for (j = 0; j < N_OUT_URB; j++)
			usb_kill_urb(portdata->out_urbs[j]);
	}

	/* Now free them */
	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);

		for (j = 0; j < N_IN_URB; j++) {
			if (portdata->in_urbs[j]) {
				usb_free_urb(portdata->in_urbs[j]);
				free_page((unsigned long)portdata->in_buffer[j]);
				portdata->in_urbs[j] = NULL;
			}
		}
		for (j = 0; j < N_OUT_URB; j++) {
			if (portdata->out_urbs[j]) {
				usb_free_urb(portdata->out_urbs[j]);
				kfree(portdata->out_buffer[j]);
				portdata->out_urbs[j] = NULL;
			}
		}
	}

	/* Now free per port private data */
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		kfree(usb_get_serial_port_data(port));
	}
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages");
module_param(vendor, ushort, 0);
MODULE_PARM_DESC(vendor, "User specified USB idVendor");
module_param(product, ushort, 0);
MODULE_PARM_DESC(product, "User specified USB idProduct");


