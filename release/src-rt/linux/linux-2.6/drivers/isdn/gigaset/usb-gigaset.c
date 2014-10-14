/*
 * USB driver for Gigaset 307x directly or using M105 Data.
 *
 * Copyright (c) 2001 by Stefan Eilers
 *                   and Hansjoerg Lipp <hjlipp@web.de>.
 *
 * This driver was derived from the USB skeleton driver by
 * Greg Kroah-Hartman <greg@kroah.com>
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 */

#include "gigaset.h"
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* Version Information */
#define DRIVER_AUTHOR "Hansjoerg Lipp <hjlipp@web.de>, Stefan Eilers"
#define DRIVER_DESC "USB Driver for Gigaset 307x using M105"

/* Module parameters */

static int startmode = SM_ISDN;
static int cidmode = 1;

module_param(startmode, int, S_IRUGO);
module_param(cidmode, int, S_IRUGO);
MODULE_PARM_DESC(startmode, "start in isdn4linux mode");
MODULE_PARM_DESC(cidmode, "Call-ID mode");

#define GIGASET_MINORS     1
#define GIGASET_MINOR      8
#define GIGASET_MODULENAME "usb_gigaset"
#define GIGASET_DEVNAME    "ttyGU"

/* length limit according to Siemens 3070usb-protokoll.doc ch. 2.1 */
#define IF_WRITEBUF 264

/* Values for the Gigaset M105 Data */
#define USB_M105_VENDOR_ID	0x0681
#define USB_M105_PRODUCT_ID	0x0009

/* table of devices that work with this driver */
static const struct usb_device_id gigaset_table[] = {
	{ USB_DEVICE(USB_M105_VENDOR_ID, USB_M105_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, gigaset_table);

/*
 * Control requests (empty fields: 00)
 *
 *       RT|RQ|VALUE|INDEX|LEN  |DATA
 * In:
 *       C1 08             01
 *            Get flags (1 byte). Bits: 0=dtr,1=rts,3-7:?
 *       C1 0F             ll ll
 *            Get device information/status (llll: 0x200 and 0x40 seen).
 *            Real size: I only saw MIN(llll,0x64).
 *            Contents: seems to be always the same...
 *              offset 0x00: Length of this structure (0x64) (len: 1,2,3 bytes)
 *              offset 0x3c: String (16 bit chars): "MCCI USB Serial V2.0"
 *              rest:        ?
 * Out:
 *       41 11
 *            Initialize/reset device ?
 *       41 00 xx 00
 *            ? (xx=00 or 01; 01 on start, 00 on close)
 *       41 07 vv mm
 *            Set/clear flags vv=value, mm=mask (see RQ 08)
 *       41 12 xx
 *            Used before the following configuration requests are issued
 *            (with xx=0x0f). I've seen other values<0xf, though.
 *       41 01 xx xx
 *            Set baud rate. xxxx=ceil(0x384000/rate)=trunc(0x383fff/rate)+1.
 *       41 03 ps bb
 *            Set byte size and parity. p:  0x20=even,0x10=odd,0x00=no parity
 *                                     [    0x30: m, 0x40: s           ]
 *                                     [s:  0: 1 stop bit; 1: 1.5; 2: 2]
 *                                      bb: bits/byte (seen 7 and 8)
 *       41 13 -- -- -- -- 10 00 ww 00 00 00 xx 00 00 00 yy 00 00 00 zz 00 00 00
 *            ??
 *            Initialization: 01, 40, 00, 00
 *            Open device:    00  40, 00, 00
 *            yy and zz seem to be equal, either 0x00 or 0x0a
 *            (ww,xx) pairs seen: (00,00), (00,40), (01,40), (09,80), (19,80)
 *       41 19 -- -- -- -- 06 00 00 00 00 xx 11 13
 *            Used after every "configuration sequence" (RQ 12, RQs 01/03/13).
 *            xx is usually 0x00 but was 0x7e before starting data transfer
 *            in unimodem mode. So, this might be an array of characters that
 *            need special treatment ("commit all bufferd data"?), 11=^Q, 13=^S.
 *
 * Unimodem mode: use "modprobe ppp_async flag_time=0" as the device _needs_ two
 * flags per packet.
 */

/* functions called if a device of this driver is connected/disconnected */
static int gigaset_probe(struct usb_interface *interface,
			 const struct usb_device_id *id);
static void gigaset_disconnect(struct usb_interface *interface);

/* functions called before/after suspend */
static int gigaset_suspend(struct usb_interface *intf, pm_message_t message);
static int gigaset_resume(struct usb_interface *intf);
static int gigaset_pre_reset(struct usb_interface *intf);

static struct gigaset_driver *driver;

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver gigaset_usb_driver = {
	.name =		GIGASET_MODULENAME,
	.probe =	gigaset_probe,
	.disconnect =	gigaset_disconnect,
	.id_table =	gigaset_table,
	.suspend =	gigaset_suspend,
	.resume =	gigaset_resume,
	.reset_resume =	gigaset_resume,
	.pre_reset =	gigaset_pre_reset,
	.post_reset =	gigaset_resume,
};

struct usb_cardstate {
	struct usb_device	*udev;		/* usb device pointer */
	struct usb_interface	*interface;	/* interface for this device */
	int			busy;		/* bulk output in progress */

	/* Output buffer */
	unsigned char		*bulk_out_buffer;
	int			bulk_out_size;
	__u8			bulk_out_endpointAddr;
	struct urb		*bulk_out_urb;

	/* Input buffer */
	unsigned char		*rcvbuf;
	int			rcvbuf_size;
	struct urb		*read_urb;
	__u8			int_in_endpointAddr;

	char			bchars[6];		/* for request 0x19 */
};

static inline unsigned tiocm_to_gigaset(unsigned state)
{
	return ((state & TIOCM_DTR) ? 1 : 0) | ((state & TIOCM_RTS) ? 2 : 0);
}

static int gigaset_set_modem_ctrl(struct cardstate *cs, unsigned old_state,
				  unsigned new_state)
{
	struct usb_device *udev = cs->hw.usb->udev;
	unsigned mask, val;
	int r;

	mask = tiocm_to_gigaset(old_state ^ new_state);
	val = tiocm_to_gigaset(new_state);

	gig_dbg(DEBUG_USBREQ, "set flags 0x%02x with mask 0x%02x", val, mask);
	r = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 7, 0x41,
			    (val & 0xff) | ((mask & 0xff) << 8), 0,
			    NULL, 0, 2000 /* timeout? */);
	if (r < 0)
		return r;
	return 0;
}

/*
 * Set M105 configuration value
 * using undocumented device commands reverse engineered from USB traces
 * of the Siemens Windows driver
 */
static int set_value(struct cardstate *cs, u8 req, u16 val)
{
	struct usb_device *udev = cs->hw.usb->udev;
	int r, r2;

	gig_dbg(DEBUG_USBREQ, "request %02x (%04x)",
		(unsigned)req, (unsigned)val);
	r = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x12, 0x41,
			    0xf /*?*/, 0, NULL, 0, 2000 /*?*/);
			    /* no idea what this does */
	if (r < 0) {
		dev_err(&udev->dev, "error %d on request 0x12\n", -r);
		return r;
	}

	r = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), req, 0x41,
			    val, 0, NULL, 0, 2000 /*?*/);
	if (r < 0)
		dev_err(&udev->dev, "error %d on request 0x%02x\n",
			-r, (unsigned)req);

	r2 = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x19, 0x41,
			     0, 0, cs->hw.usb->bchars, 6, 2000 /*?*/);
	if (r2 < 0)
		dev_err(&udev->dev, "error %d on request 0x19\n", -r2);

	return r < 0 ? r : (r2 < 0 ? r2 : 0);
}

/*
 * set the baud rate on the internal serial adapter
 * using the undocumented parameter setting command
 */
static int gigaset_baud_rate(struct cardstate *cs, unsigned cflag)
{
	u16 val;
	u32 rate;

	cflag &= CBAUD;

	switch (cflag) {
	case    B300: rate =     300; break;
	case    B600: rate =     600; break;
	case   B1200: rate =    1200; break;
	case   B2400: rate =    2400; break;
	case   B4800: rate =    4800; break;
	case   B9600: rate =    9600; break;
	case  B19200: rate =   19200; break;
	case  B38400: rate =   38400; break;
	case  B57600: rate =   57600; break;
	case B115200: rate =  115200; break;
	default:
		rate =  9600;
		dev_err(cs->dev, "unsupported baudrate request 0x%x,"
			" using default of B9600\n", cflag);
	}

	val = 0x383fff / rate + 1;

	return set_value(cs, 1, val);
}

/*
 * set the line format on the internal serial adapter
 * using the undocumented parameter setting command
 */
static int gigaset_set_line_ctrl(struct cardstate *cs, unsigned cflag)
{
	u16 val = 0;

	/* set the parity */
	if (cflag & PARENB)
		val |= (cflag & PARODD) ? 0x10 : 0x20;

	/* set the number of data bits */
	switch (cflag & CSIZE) {
	case CS5:
		val |= 5 << 8; break;
	case CS6:
		val |= 6 << 8; break;
	case CS7:
		val |= 7 << 8; break;
	case CS8:
		val |= 8 << 8; break;
	default:
		dev_err(cs->dev, "CSIZE was not CS5-CS8, using default of 8\n");
		val |= 8 << 8;
		break;
	}

	/* set the number of stop bits */
	if (cflag & CSTOPB) {
		if ((cflag & CSIZE) == CS5)
			val |= 1; /* 1.5 stop bits */
		else
			val |= 2; /* 2 stop bits */
	}

	return set_value(cs, 3, val);
}


/*============================================================================*/
static int gigaset_init_bchannel(struct bc_state *bcs)
{
	/* nothing to do for M10x */
	gigaset_bchannel_up(bcs);
	return 0;
}

static int gigaset_close_bchannel(struct bc_state *bcs)
{
	/* nothing to do for M10x */
	gigaset_bchannel_down(bcs);
	return 0;
}

static int write_modem(struct cardstate *cs);
static int send_cb(struct cardstate *cs, struct cmdbuf_t *cb);


/* Write tasklet handler: Continue sending current skb, or send command, or
 * start sending an skb from the send queue.
 */
static void gigaset_modem_fill(unsigned long data)
{
	struct cardstate *cs = (struct cardstate *) data;
	struct bc_state *bcs = &cs->bcs[0]; /* only one channel */
	struct cmdbuf_t *cb;
	int again;

	gig_dbg(DEBUG_OUTPUT, "modem_fill");

	if (cs->hw.usb->busy) {
		gig_dbg(DEBUG_OUTPUT, "modem_fill: busy");
		return;
	}

	do {
		again = 0;
		if (!bcs->tx_skb) { /* no skb is being sent */
			cb = cs->cmdbuf;
			if (cb) { /* commands to send? */
				gig_dbg(DEBUG_OUTPUT, "modem_fill: cb");
				if (send_cb(cs, cb) < 0) {
					gig_dbg(DEBUG_OUTPUT,
						"modem_fill: send_cb failed");
					again = 1; /* no callback will be
						      called! */
				}
			} else { /* skbs to send? */
				bcs->tx_skb = skb_dequeue(&bcs->squeue);
				if (bcs->tx_skb)
					gig_dbg(DEBUG_INTR,
						"Dequeued skb (Adr: %lx)!",
						(unsigned long) bcs->tx_skb);
			}
		}

		if (bcs->tx_skb) {
			gig_dbg(DEBUG_OUTPUT, "modem_fill: tx_skb");
			if (write_modem(cs) < 0) {
				gig_dbg(DEBUG_OUTPUT,
					"modem_fill: write_modem failed");
				again = 1; /* no callback will be called! */
			}
		}
	} while (again);
}

/*
 * Interrupt Input URB completion routine
 */
static void gigaset_read_int_callback(struct urb *urb)
{
	struct cardstate *cs = urb->context;
	struct inbuf_t *inbuf = cs->inbuf;
	int status = urb->status;
	int r;
	unsigned numbytes;
	unsigned char *src;
	unsigned long flags;

	if (!status) {
		numbytes = urb->actual_length;

		if (numbytes) {
			src = cs->hw.usb->rcvbuf;
			if (unlikely(*src))
				dev_warn(cs->dev,
				    "%s: There was no leading 0, but 0x%02x!\n",
					 __func__, (unsigned) *src);
			++src; /* skip leading 0x00 */
			--numbytes;
			if (gigaset_fill_inbuf(inbuf, src, numbytes)) {
				gig_dbg(DEBUG_INTR, "%s-->BH", __func__);
				gigaset_schedule_event(inbuf->cs);
			}
		} else
			gig_dbg(DEBUG_INTR, "Received zero block length");
	} else {
		/* The urb might have been killed. */
		gig_dbg(DEBUG_ANY, "%s - nonzero status received: %d",
			__func__, status);
		if (status == -ENOENT || status == -ESHUTDOWN)
			/* killed or endpoint shutdown: don't resubmit */
			return;
	}

	/* resubmit URB */
	spin_lock_irqsave(&cs->lock, flags);
	if (!cs->connected) {
		spin_unlock_irqrestore(&cs->lock, flags);
		pr_err("%s: disconnected\n", __func__);
		return;
	}
	r = usb_submit_urb(urb, GFP_ATOMIC);
	spin_unlock_irqrestore(&cs->lock, flags);
	if (r)
		dev_err(cs->dev, "error %d resubmitting URB\n", -r);
}


/* This callback routine is called when data was transmitted to the device. */
static void gigaset_write_bulk_callback(struct urb *urb)
{
	struct cardstate *cs = urb->context;
	int status = urb->status;
	unsigned long flags;

	switch (status) {
	case 0:			/* normal completion */
		break;
	case -ENOENT:		/* killed */
		gig_dbg(DEBUG_ANY, "%s: killed", __func__);
		cs->hw.usb->busy = 0;
		return;
	default:
		dev_err(cs->dev, "bulk transfer failed (status %d)\n",
			-status);
		/* That's all we can do. Communication problems
		   are handled by timeouts or network protocols. */
	}

	spin_lock_irqsave(&cs->lock, flags);
	if (!cs->connected) {
		pr_err("%s: disconnected\n", __func__);
	} else {
		cs->hw.usb->busy = 0;
		tasklet_schedule(&cs->write_tasklet);
	}
	spin_unlock_irqrestore(&cs->lock, flags);
}

static int send_cb(struct cardstate *cs, struct cmdbuf_t *cb)
{
	struct cmdbuf_t *tcb;
	unsigned long flags;
	int count;
	int status = -ENOENT;
	struct usb_cardstate *ucs = cs->hw.usb;

	do {
		if (!cb->len) {
			tcb = cb;

			spin_lock_irqsave(&cs->cmdlock, flags);
			cs->cmdbytes -= cs->curlen;
			gig_dbg(DEBUG_OUTPUT, "send_cb: sent %u bytes, %u left",
				cs->curlen, cs->cmdbytes);
			cs->cmdbuf = cb = cb->next;
			if (cb) {
				cb->prev = NULL;
				cs->curlen = cb->len;
			} else {
				cs->lastcmdbuf = NULL;
				cs->curlen = 0;
			}
			spin_unlock_irqrestore(&cs->cmdlock, flags);

			if (tcb->wake_tasklet)
				tasklet_schedule(tcb->wake_tasklet);
			kfree(tcb);
		}
		if (cb) {
			count = min(cb->len, ucs->bulk_out_size);
			gig_dbg(DEBUG_OUTPUT, "send_cb: send %d bytes", count);

			usb_fill_bulk_urb(ucs->bulk_out_urb, ucs->udev,
					  usb_sndbulkpipe(ucs->udev,
					     ucs->bulk_out_endpointAddr & 0x0f),
					  cb->buf + cb->offset, count,
					  gigaset_write_bulk_callback, cs);

			cb->offset += count;
			cb->len -= count;
			ucs->busy = 1;

			spin_lock_irqsave(&cs->lock, flags);
			status = cs->connected ?
				usb_submit_urb(ucs->bulk_out_urb, GFP_ATOMIC) :
				-ENODEV;
			spin_unlock_irqrestore(&cs->lock, flags);

			if (status) {
				ucs->busy = 0;
				dev_err(cs->dev,
					"could not submit urb (error %d)\n",
					-status);
				cb->len = 0; /* skip urb => remove cb+wakeup
						in next loop cycle */
			}
		}
	} while (cb && status); /* next command on error */

	return status;
}

/* Send command to device. */
static int gigaset_write_cmd(struct cardstate *cs, struct cmdbuf_t *cb)
{
	unsigned long flags;

	gigaset_dbg_buffer(cs->mstate != MS_LOCKED ?
			     DEBUG_TRANSCMD : DEBUG_LOCKCMD,
			   "CMD Transmit", cb->len, cb->buf);

	spin_lock_irqsave(&cs->cmdlock, flags);
	cb->prev = cs->lastcmdbuf;
	if (cs->lastcmdbuf)
		cs->lastcmdbuf->next = cb;
	else {
		cs->cmdbuf = cb;
		cs->curlen = cb->len;
	}
	cs->cmdbytes += cb->len;
	cs->lastcmdbuf = cb;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	spin_lock_irqsave(&cs->lock, flags);
	if (cs->connected)
		tasklet_schedule(&cs->write_tasklet);
	spin_unlock_irqrestore(&cs->lock, flags);
	return cb->len;
}

static int gigaset_write_room(struct cardstate *cs)
{
	unsigned bytes;

	bytes = cs->cmdbytes;
	return bytes < IF_WRITEBUF ? IF_WRITEBUF - bytes : 0;
}

static int gigaset_chars_in_buffer(struct cardstate *cs)
{
	return cs->cmdbytes;
}

/*
 * set the break characters on the internal serial adapter
 * using undocumented device commands reverse engineered from USB traces
 * of the Siemens Windows driver
 */
static int gigaset_brkchars(struct cardstate *cs, const unsigned char buf[6])
{
	struct usb_device *udev = cs->hw.usb->udev;

	gigaset_dbg_buffer(DEBUG_USBREQ, "brkchars", 6, buf);
	memcpy(cs->hw.usb->bchars, buf, 6);
	return usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x19, 0x41,
			       0, 0, &buf, 6, 2000);
}

static int gigaset_freebcshw(struct bc_state *bcs)
{
	/* unused */
	return 1;
}

/* Initialize the b-channel structure */
static int gigaset_initbcshw(struct bc_state *bcs)
{
	/* unused */
	bcs->hw.usb = NULL;
	return 1;
}

static void gigaset_reinitbcshw(struct bc_state *bcs)
{
	/* nothing to do for M10x */
}

static void gigaset_freecshw(struct cardstate *cs)
{
	tasklet_kill(&cs->write_tasklet);
	kfree(cs->hw.usb);
}

static int gigaset_initcshw(struct cardstate *cs)
{
	struct usb_cardstate *ucs;

	cs->hw.usb = ucs =
		kmalloc(sizeof(struct usb_cardstate), GFP_KERNEL);
	if (!ucs) {
		pr_err("out of memory\n");
		return 0;
	}

	ucs->bchars[0] = 0;
	ucs->bchars[1] = 0;
	ucs->bchars[2] = 0;
	ucs->bchars[3] = 0;
	ucs->bchars[4] = 0x11;
	ucs->bchars[5] = 0x13;
	ucs->bulk_out_buffer = NULL;
	ucs->bulk_out_urb = NULL;
	ucs->read_urb = NULL;
	tasklet_init(&cs->write_tasklet,
		     gigaset_modem_fill, (unsigned long) cs);

	return 1;
}

/* Send data from current skb to the device. */
static int write_modem(struct cardstate *cs)
{
	int ret = 0;
	int count;
	struct bc_state *bcs = &cs->bcs[0]; /* only one channel */
	struct usb_cardstate *ucs = cs->hw.usb;
	unsigned long flags;

	gig_dbg(DEBUG_OUTPUT, "len: %d...", bcs->tx_skb->len);

	if (!bcs->tx_skb->len) {
		dev_kfree_skb_any(bcs->tx_skb);
		bcs->tx_skb = NULL;
		return -EINVAL;
	}

	/* Copy data to bulk out buffer and transmit data */
	count = min(bcs->tx_skb->len, (unsigned) ucs->bulk_out_size);
	skb_copy_from_linear_data(bcs->tx_skb, ucs->bulk_out_buffer, count);
	skb_pull(bcs->tx_skb, count);
	ucs->busy = 1;
	gig_dbg(DEBUG_OUTPUT, "write_modem: send %d bytes", count);

	spin_lock_irqsave(&cs->lock, flags);
	if (cs->connected) {
		usb_fill_bulk_urb(ucs->bulk_out_urb, ucs->udev,
				  usb_sndbulkpipe(ucs->udev,
						  ucs->bulk_out_endpointAddr &
						  0x0f),
				  ucs->bulk_out_buffer, count,
				  gigaset_write_bulk_callback, cs);
		ret = usb_submit_urb(ucs->bulk_out_urb, GFP_ATOMIC);
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(&cs->lock, flags);

	if (ret) {
		dev_err(cs->dev, "could not submit urb (error %d)\n", -ret);
		ucs->busy = 0;
	}

	if (!bcs->tx_skb->len) {
		/* skb sent completely */
		gigaset_skb_sent(bcs, bcs->tx_skb);

		gig_dbg(DEBUG_INTR, "kfree skb (Adr: %lx)!",
			(unsigned long) bcs->tx_skb);
		dev_kfree_skb_any(bcs->tx_skb);
		bcs->tx_skb = NULL;
	}

	return ret;
}

static int gigaset_probe(struct usb_interface *interface,
			 const struct usb_device_id *id)
{
	int retval;
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_host_interface *hostif = interface->cur_altsetting;
	struct cardstate *cs = NULL;
	struct usb_cardstate *ucs = NULL;
	struct usb_endpoint_descriptor *endpoint;
	int buffer_size;

	gig_dbg(DEBUG_ANY, "%s: Check if device matches ...", __func__);

	/* See if the device offered us matches what we can accept */
	if ((le16_to_cpu(udev->descriptor.idVendor)  != USB_M105_VENDOR_ID) ||
	    (le16_to_cpu(udev->descriptor.idProduct) != USB_M105_PRODUCT_ID)) {
		gig_dbg(DEBUG_ANY, "device ID (0x%x, 0x%x) not for me - skip",
			le16_to_cpu(udev->descriptor.idVendor),
			le16_to_cpu(udev->descriptor.idProduct));
		return -ENODEV;
	}
	if (hostif->desc.bInterfaceNumber != 0) {
		gig_dbg(DEBUG_ANY, "interface %d not for me - skip",
			hostif->desc.bInterfaceNumber);
		return -ENODEV;
	}
	if (hostif->desc.bAlternateSetting != 0) {
		dev_notice(&udev->dev, "unsupported altsetting %d - skip",
			   hostif->desc.bAlternateSetting);
		return -ENODEV;
	}
	if (hostif->desc.bInterfaceClass != 255) {
		dev_notice(&udev->dev, "unsupported interface class %d - skip",
			   hostif->desc.bInterfaceClass);
		return -ENODEV;
	}

	dev_info(&udev->dev, "%s: Device matched ... !\n", __func__);

	/* allocate memory for our device state and initialize it */
	cs = gigaset_initcs(driver, 1, 1, 0, cidmode, GIGASET_MODULENAME);
	if (!cs)
		return -ENODEV;
	ucs = cs->hw.usb;

	/* save off device structure ptrs for later use */
	usb_get_dev(udev);
	ucs->udev = udev;
	ucs->interface = interface;
	cs->dev = &interface->dev;

	/* save address of controller structure */
	usb_set_intfdata(interface, cs);

	endpoint = &hostif->endpoint[0].desc;

	buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
	ucs->bulk_out_size = buffer_size;
	ucs->bulk_out_endpointAddr = endpoint->bEndpointAddress;
	ucs->bulk_out_buffer = kmalloc(buffer_size, GFP_KERNEL);
	if (!ucs->bulk_out_buffer) {
		dev_err(cs->dev, "Couldn't allocate bulk_out_buffer\n");
		retval = -ENOMEM;
		goto error;
	}

	ucs->bulk_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ucs->bulk_out_urb) {
		dev_err(cs->dev, "Couldn't allocate bulk_out_urb\n");
		retval = -ENOMEM;
		goto error;
	}

	endpoint = &hostif->endpoint[1].desc;

	ucs->busy = 0;

	ucs->read_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ucs->read_urb) {
		dev_err(cs->dev, "No free urbs available\n");
		retval = -ENOMEM;
		goto error;
	}
	buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
	ucs->rcvbuf_size = buffer_size;
	ucs->int_in_endpointAddr = endpoint->bEndpointAddress;
	ucs->rcvbuf = kmalloc(buffer_size, GFP_KERNEL);
	if (!ucs->rcvbuf) {
		dev_err(cs->dev, "Couldn't allocate rcvbuf\n");
		retval = -ENOMEM;
		goto error;
	}
	/* Fill the interrupt urb and send it to the core */
	usb_fill_int_urb(ucs->read_urb, udev,
			 usb_rcvintpipe(udev,
					endpoint->bEndpointAddress & 0x0f),
			 ucs->rcvbuf, buffer_size,
			 gigaset_read_int_callback,
			 cs, endpoint->bInterval);

	retval = usb_submit_urb(ucs->read_urb, GFP_KERNEL);
	if (retval) {
		dev_err(cs->dev, "Could not submit URB (error %d)\n", -retval);
		goto error;
	}

	/* tell common part that the device is ready */
	if (startmode == SM_LOCKED)
		cs->mstate = MS_LOCKED;

	if (!gigaset_start(cs)) {
		tasklet_kill(&cs->write_tasklet);
		retval = -ENODEV;
		goto error;
	}
	return 0;

error:
	usb_kill_urb(ucs->read_urb);
	kfree(ucs->bulk_out_buffer);
	usb_free_urb(ucs->bulk_out_urb);
	kfree(ucs->rcvbuf);
	usb_free_urb(ucs->read_urb);
	usb_set_intfdata(interface, NULL);
	ucs->read_urb = ucs->bulk_out_urb = NULL;
	ucs->rcvbuf = ucs->bulk_out_buffer = NULL;
	usb_put_dev(ucs->udev);
	ucs->udev = NULL;
	ucs->interface = NULL;
	gigaset_freecs(cs);
	return retval;
}

static void gigaset_disconnect(struct usb_interface *interface)
{
	struct cardstate *cs;
	struct usb_cardstate *ucs;

	cs = usb_get_intfdata(interface);
	ucs = cs->hw.usb;

	dev_info(cs->dev, "disconnecting Gigaset USB adapter\n");

	usb_kill_urb(ucs->read_urb);

	gigaset_stop(cs);

	usb_set_intfdata(interface, NULL);
	tasklet_kill(&cs->write_tasklet);

	usb_kill_urb(ucs->bulk_out_urb);

	kfree(ucs->bulk_out_buffer);
	usb_free_urb(ucs->bulk_out_urb);
	kfree(ucs->rcvbuf);
	usb_free_urb(ucs->read_urb);
	ucs->read_urb = ucs->bulk_out_urb = NULL;
	ucs->rcvbuf = ucs->bulk_out_buffer = NULL;

	usb_put_dev(ucs->udev);
	ucs->interface = NULL;
	ucs->udev = NULL;
	cs->dev = NULL;
	gigaset_freecs(cs);
}

/* gigaset_suspend
 * This function is called before the USB connection is suspended or reset.
 */
static int gigaset_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct cardstate *cs = usb_get_intfdata(intf);

	/* stop activity */
	cs->connected = 0;	/* prevent rescheduling */
	usb_kill_urb(cs->hw.usb->read_urb);
	tasklet_kill(&cs->write_tasklet);
	usb_kill_urb(cs->hw.usb->bulk_out_urb);

	gig_dbg(DEBUG_SUSPEND, "suspend complete");
	return 0;
}

/* gigaset_resume
 * This function is called after the USB connection has been resumed or reset.
 */
static int gigaset_resume(struct usb_interface *intf)
{
	struct cardstate *cs = usb_get_intfdata(intf);
	int rc;

	/* resubmit interrupt URB */
	cs->connected = 1;
	rc = usb_submit_urb(cs->hw.usb->read_urb, GFP_KERNEL);
	if (rc) {
		dev_err(cs->dev, "Could not submit read URB (error %d)\n", -rc);
		return rc;
	}

	gig_dbg(DEBUG_SUSPEND, "resume complete");
	return 0;
}

/* gigaset_pre_reset
 * This function is called before the USB connection is reset.
 */
static int gigaset_pre_reset(struct usb_interface *intf)
{
	/* same as suspend */
	return gigaset_suspend(intf, PMSG_ON);
}

static const struct gigaset_ops ops = {
	gigaset_write_cmd,
	gigaset_write_room,
	gigaset_chars_in_buffer,
	gigaset_brkchars,
	gigaset_init_bchannel,
	gigaset_close_bchannel,
	gigaset_initbcshw,
	gigaset_freebcshw,
	gigaset_reinitbcshw,
	gigaset_initcshw,
	gigaset_freecshw,
	gigaset_set_modem_ctrl,
	gigaset_baud_rate,
	gigaset_set_line_ctrl,
	gigaset_m10x_send_skb,
	gigaset_m10x_input,
};

/*
 * This function is called while kernel-module is loaded
 */
static int __init usb_gigaset_init(void)
{
	int result;

	/* allocate memory for our driver state and initialize it */
	driver = gigaset_initdriver(GIGASET_MINOR, GIGASET_MINORS,
				    GIGASET_MODULENAME, GIGASET_DEVNAME,
				    &ops, THIS_MODULE);
	if (driver == NULL)
		goto error;

	/* register this driver with the USB subsystem */
	result = usb_register(&gigaset_usb_driver);
	if (result < 0) {
		pr_err("error %d registering USB driver\n", -result);
		goto error;
	}

	pr_info(DRIVER_DESC "\n");
	return 0;

error:
	if (driver)
		gigaset_freedriver(driver);
	driver = NULL;
	return -1;
}

/*
 * This function is called while unloading the kernel-module
 */
static void __exit usb_gigaset_exit(void)
{
	int i;

	gigaset_blockdriver(driver); /* => probe will fail
				      * => no gigaset_start any more
				      */

	/* stop all connected devices */
	for (i = 0; i < driver->minors; i++)
		gigaset_shutdown(driver->cs + i);

	/* from now on, no isdn callback should be possible */

	/* deregister this driver with the USB subsystem */
	usb_deregister(&gigaset_usb_driver);
	/* this will call the disconnect-callback */
	/* from now on, no disconnect/probe callback should be running */

	gigaset_freedriver(driver);
	driver = NULL;
}


module_init(usb_gigaset_init);
module_exit(usb_gigaset_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

MODULE_LICENSE("GPL");
