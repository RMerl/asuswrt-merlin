/*
 * (C) Copyright Linus Torvalds 1999
 * (C) Copyright Johannes Erdfelt 1999-2001
 * (C) Copyright Andreas Gal 1999
 * (C) Copyright Gregory P. Smith 1999
 * (C) Copyright Deti Fliegl 1999
 * (C) Copyright Randy Dunlap 2000
 * (C) Copyright David Brownell 2000-2002
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/utsname.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/scatterlist.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <asm/irq.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <linux/usb.h>

#include "usb.h"
#include "hcd.h"
#include "hub.h"


/*-------------------------------------------------------------------------*/

/*
 * USB Host Controller Driver framework
 *
 * Plugs into usbcore (usb_bus) and lets HCDs share code, minimizing
 * HCD-specific behaviors/bugs.
 *
 * This does error checks, tracks devices and urbs, and delegates to a
 * "hc_driver" only for code (and data) that really needs to know about
 * hardware differences.  That includes root hub registers, i/o queues,
 * and so on ... but as little else as possible.
 *
 * Shared code includes most of the "root hub" code (these are emulated,
 * though each HC's hardware works differently) and PCI glue, plus request
 * tracking overhead.  The HCD code should only block on spinlocks or on
 * hardware handshaking; blocking on software events (such as other kernel
 * threads releasing resources, or completing actions) is all generic.
 *
 * Happens the USB 2.0 spec says this would be invisible inside the "USBD",
 * and includes mostly a "HCDI" (HCD Interface) along with some APIs used
 * only by the hub driver ... and that neither should be seen or used by
 * usb client device drivers.
 *
 * Contributors of ideas or unattributed patches include: David Brownell,
 * Roman Weissgaerber, Rory Bolt, Greg Kroah-Hartman, ...
 *
 * HISTORY:
 * 2002-02-21	Pull in most of the usb_bus support from usb.c; some
 *		associated cleanup.  "usb_hcd" still != "usb_bus".
 * 2001-12-12	Initial patch version for Linux 2.5.1 kernel.
 */

/*-------------------------------------------------------------------------*/

/* Keep track of which host controller drivers are loaded */
unsigned long usb_hcds_loaded;
EXPORT_SYMBOL_GPL(usb_hcds_loaded);

/* host controllers we manage */
LIST_HEAD (usb_bus_list);
EXPORT_SYMBOL_GPL (usb_bus_list);

/* used when allocating bus numbers */
#define USB_MAXBUS		64
struct usb_busmap {
	unsigned long busmap [USB_MAXBUS / (8*sizeof (unsigned long))];
};
static struct usb_busmap busmap;

/* used when updating list of hcds */
DEFINE_MUTEX(usb_bus_list_lock);	/* exported only for usbfs */
EXPORT_SYMBOL_GPL (usb_bus_list_lock);

/* used for controlling access to virtual root hubs */
static DEFINE_SPINLOCK(hcd_root_hub_lock);

/* used when updating an endpoint's URB list */
static DEFINE_SPINLOCK(hcd_urb_list_lock);

/* used to protect against unlinking URBs after the device is gone */
static DEFINE_SPINLOCK(hcd_urb_unlink_lock);

/* wait queue for synchronous unlinks */
DECLARE_WAIT_QUEUE_HEAD(usb_kill_urb_queue);

static inline int is_root_hub(struct usb_device *udev)
{
	return (udev->parent == NULL);
}

/*-------------------------------------------------------------------------*/

/*
 * Sharable chunks of root hub code.
 */

/*-------------------------------------------------------------------------*/

#define KERNEL_REL	((LINUX_VERSION_CODE >> 16) & 0x0ff)
#define KERNEL_VER	((LINUX_VERSION_CODE >> 8) & 0x0ff)

/* usb 2.0 root hub device descriptor */
static const u8 usb2_rh_dev_descriptor [18] = {
	0x12,       /*  __u8  bLength; */
	0x01,       /*  __u8  bDescriptorType; Device */
	0x00, 0x02, /*  __le16 bcdUSB; v2.0 */

	0x09,	    /*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,	    /*  __u8  bDeviceSubClass; */
	0x00,       /*  __u8  bDeviceProtocol; [ usb 2.0 no TT ] */
	0x40,       /*  __u8  bMaxPacketSize0; 64 Bytes */

	0x6b, 0x1d, /*  __le16 idVendor; Linux Foundation */
	0x02, 0x00, /*  __le16 idProduct; device 0x0002 */
	KERNEL_VER, KERNEL_REL, /*  __le16 bcdDevice */

	0x03,       /*  __u8  iManufacturer; */
	0x02,       /*  __u8  iProduct; */
	0x01,       /*  __u8  iSerialNumber; */
	0x01        /*  __u8  bNumConfigurations; */
};

/* no usb 2.0 root hub "device qualifier" descriptor: one speed only */

/* usb 1.1 root hub device descriptor */
static const u8 usb11_rh_dev_descriptor [18] = {
	0x12,       /*  __u8  bLength; */
	0x01,       /*  __u8  bDescriptorType; Device */
	0x10, 0x01, /*  __le16 bcdUSB; v1.1 */

	0x09,	    /*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,	    /*  __u8  bDeviceSubClass; */
	0x00,       /*  __u8  bDeviceProtocol; [ low/full speeds only ] */
	0x40,       /*  __u8  bMaxPacketSize0; 64 Bytes */

	0x6b, 0x1d, /*  __le16 idVendor; Linux Foundation */
	0x01, 0x00, /*  __le16 idProduct; device 0x0001 */
	KERNEL_VER, KERNEL_REL, /*  __le16 bcdDevice */

	0x03,       /*  __u8  iManufacturer; */
	0x02,       /*  __u8  iProduct; */
	0x01,       /*  __u8  iSerialNumber; */
	0x01        /*  __u8  bNumConfigurations; */
};


/*-------------------------------------------------------------------------*/

/* Configuration descriptors for our root hubs */

static const u8 fs_rh_config_descriptor [] = {

	/* one configuration */
	0x09,       /*  __u8  bLength; */
	0x02,       /*  __u8  bDescriptorType; Configuration */
	0x19, 0x00, /*  __le16 wTotalLength; */
	0x01,       /*  __u8  bNumInterfaces; (1) */
	0x01,       /*  __u8  bConfigurationValue; */
	0x00,       /*  __u8  iConfiguration; */
	0xc0,       /*  __u8  bmAttributes; 
				 Bit 7: must be set,
				     6: Self-powered,
				     5: Remote wakeup,
				     4..0: resvd */
	0x00,       /*  __u8  MaxPower; */
      
	/* USB 1.1:
	 * USB 2.0, single TT organization (mandatory):
	 *	one interface, protocol 0
	 *
	 * USB 2.0, multiple TT organization (optional):
	 *	two interfaces, protocols 1 (like single TT)
	 *	and 2 (multiple TT mode) ... config is
	 *	sometimes settable
	 *	NOT IMPLEMENTED
	 */

	/* one interface */
	0x09,       /*  __u8  if_bLength; */
	0x04,       /*  __u8  if_bDescriptorType; Interface */
	0x00,       /*  __u8  if_bInterfaceNumber; */
	0x00,       /*  __u8  if_bAlternateSetting; */
	0x01,       /*  __u8  if_bNumEndpoints; */
	0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,       /*  __u8  if_bInterfaceSubClass; */
	0x00,       /*  __u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
	0x00,       /*  __u8  if_iInterface; */
     
	/* one endpoint (status change endpoint) */
	0x07,       /*  __u8  ep_bLength; */
	0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  __u8  ep_bmAttributes; Interrupt */
 	0x02, 0x00, /*  __le16 ep_wMaxPacketSize; 1 + (MAX_ROOT_PORTS / 8) */
	0xff        /*  __u8  ep_bInterval; (255ms -- usb 2.0 spec) */
};

static const u8 hs_rh_config_descriptor [] = {

	/* one configuration */
	0x09,       /*  __u8  bLength; */
	0x02,       /*  __u8  bDescriptorType; Configuration */
	0x19, 0x00, /*  __le16 wTotalLength; */
	0x01,       /*  __u8  bNumInterfaces; (1) */
	0x01,       /*  __u8  bConfigurationValue; */
	0x00,       /*  __u8  iConfiguration; */
	0xc0,       /*  __u8  bmAttributes; 
				 Bit 7: must be set,
				     6: Self-powered,
				     5: Remote wakeup,
				     4..0: resvd */
	0x00,       /*  __u8  MaxPower; */
      
	/* USB 1.1:
	 * USB 2.0, single TT organization (mandatory):
	 *	one interface, protocol 0
	 *
	 * USB 2.0, multiple TT organization (optional):
	 *	two interfaces, protocols 1 (like single TT)
	 *	and 2 (multiple TT mode) ... config is
	 *	sometimes settable
	 *	NOT IMPLEMENTED
	 */

	/* one interface */
	0x09,       /*  __u8  if_bLength; */
	0x04,       /*  __u8  if_bDescriptorType; Interface */
	0x00,       /*  __u8  if_bInterfaceNumber; */
	0x00,       /*  __u8  if_bAlternateSetting; */
	0x01,       /*  __u8  if_bNumEndpoints; */
	0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,       /*  __u8  if_bInterfaceSubClass; */
	0x00,       /*  __u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
	0x00,       /*  __u8  if_iInterface; */
     
	/* one endpoint (status change endpoint) */
	0x07,       /*  __u8  ep_bLength; */
	0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  __u8  ep_bmAttributes; Interrupt */
		    /* __le16 ep_wMaxPacketSize; 1 + (MAX_ROOT_PORTS / 8)
		     * see hub.c:hub_configure() for details. */
	(USB_MAXCHILDREN + 1 + 7) / 8, 0x00,
	0x0c        /*  __u8  ep_bInterval; (256ms -- usb 2.0 spec) */
};

/*-------------------------------------------------------------------------*/

/*
 * helper routine for returning string descriptors in UTF-16LE
 * input can actually be ISO-8859-1; ASCII is its 7-bit subset
 */
static unsigned ascii2utf(char *s, u8 *utf, int utfmax)
{
	unsigned retval;

	for (retval = 0; *s && utfmax > 1; utfmax -= 2, retval += 2) {
		*utf++ = *s++;
		*utf++ = 0;
	}
	if (utfmax > 0) {
		*utf = *s;
		++retval;
	}
	return retval;
}

/*
 * rh_string - provides manufacturer, product and serial strings for root hub
 * @id: the string ID number (1: serial number, 2: product, 3: vendor)
 * @hcd: the host controller for this root hub
 * @data: return packet in UTF-16 LE
 * @len: length of the return packet
 *
 * Produces either a manufacturer, product or serial number string for the
 * virtual root hub device.
 */
static unsigned rh_string(int id, struct usb_hcd *hcd, u8 *data, unsigned len)
{
	char buf [100];

	// language ids
	if (id == 0) {
		buf[0] = 4;    buf[1] = 3;	/* 4 bytes string data */
		buf[2] = 0x09; buf[3] = 0x04;	/* MSFT-speak for "en-us" */
		len = min_t(unsigned, len, 4);
		memcpy (data, buf, len);
		return len;

	// serial number
	} else if (id == 1) {
		strlcpy (buf, hcd->self.bus_name, sizeof buf);

	// product description
	} else if (id == 2) {
		strlcpy (buf, hcd->product_desc, sizeof buf);

 	// id 3 == vendor description
	} else if (id == 3) {
		snprintf (buf, sizeof buf, "%s %s %s", init_utsname()->sysname,
			init_utsname()->release, hcd->driver->description);
	}

	switch (len) {		/* All cases fall through */
	default:
		len = 2 + ascii2utf (buf, data + 2, len - 2);
	case 2:
		data [1] = 3;	/* type == string */
	case 1:
		data [0] = 2 * (strlen (buf) + 1);
	case 0:
		;		/* Compiler wants a statement here */
	}
	return len;
}


/* Root hub control transfers execute synchronously */
static int rh_call_control (struct usb_hcd *hcd, struct urb *urb)
{
	struct usb_ctrlrequest *cmd;
 	u16		typeReq, wValue, wIndex, wLength;
	u8		*ubuf = urb->transfer_buffer;
	u8		tbuf [sizeof (struct usb_hub_descriptor)]
		__attribute__((aligned(4)));
	const u8	*bufp = tbuf;
	unsigned	len = 0;
	int		status;
	u8		patch_wakeup = 0;
	u8		patch_protocol = 0;

	might_sleep();

	spin_lock_irq(&hcd_root_hub_lock);
	status = usb_hcd_link_urb_to_ep(hcd, urb);
	spin_unlock_irq(&hcd_root_hub_lock);
	if (status)
		return status;
	urb->hcpriv = hcd;	/* Indicate it's queued */

	cmd = (struct usb_ctrlrequest *) urb->setup_packet;
	typeReq  = (cmd->bRequestType << 8) | cmd->bRequest;
	wValue   = le16_to_cpu (cmd->wValue);
	wIndex   = le16_to_cpu (cmd->wIndex);
	wLength  = le16_to_cpu (cmd->wLength);

	if (wLength > urb->transfer_buffer_length)
		goto error;

	urb->actual_length = 0;
	switch (typeReq) {

	/* DEVICE REQUESTS */

	/* The root hub's remote wakeup enable bit is implemented using
	 * driver model wakeup flags.  If this system supports wakeup
	 * through USB, userspace may change the default "allow wakeup"
	 * policy through sysfs or these calls.
	 *
	 * Most root hubs support wakeup from downstream devices, for
	 * runtime power management (disabling USB clocks and reducing
	 * VBUS power usage).  However, not all of them do so; silicon,
	 * board, and BIOS bugs here are not uncommon, so these can't
	 * be treated quite like external hubs.
	 *
	 * Likewise, not all root hubs will pass wakeup events upstream,
	 * to wake up the whole system.  So don't assume root hub and
	 * controller capabilities are identical.
	 */

	case DeviceRequest | USB_REQ_GET_STATUS:
		tbuf [0] = (device_may_wakeup(&hcd->self.root_hub->dev)
					<< USB_DEVICE_REMOTE_WAKEUP)
				| (1 << USB_DEVICE_SELF_POWERED);
		tbuf [1] = 0;
		len = 2;
		break;
	case DeviceOutRequest | USB_REQ_CLEAR_FEATURE:
		if (wValue == USB_DEVICE_REMOTE_WAKEUP)
			device_set_wakeup_enable(&hcd->self.root_hub->dev, 0);
		else
			goto error;
		break;
	case DeviceOutRequest | USB_REQ_SET_FEATURE:
		if (device_can_wakeup(&hcd->self.root_hub->dev)
				&& wValue == USB_DEVICE_REMOTE_WAKEUP)
			device_set_wakeup_enable(&hcd->self.root_hub->dev, 1);
		else
			goto error;
		break;
	case DeviceRequest | USB_REQ_GET_CONFIGURATION:
		tbuf [0] = 1;
		len = 1;
			/* FALLTHROUGH */
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		break;
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		switch (wValue & 0xff00) {
		case USB_DT_DEVICE << 8:
			if (hcd->driver->flags & HCD_USB2)
				bufp = usb2_rh_dev_descriptor;
			else if (hcd->driver->flags & HCD_USB11)
				bufp = usb11_rh_dev_descriptor;
			else
				goto error;
			len = 18;
			if (hcd->has_tt)
				patch_protocol = 1;
			break;
		case USB_DT_CONFIG << 8:
			if (hcd->driver->flags & HCD_USB2) {
				bufp = hs_rh_config_descriptor;
				len = sizeof hs_rh_config_descriptor;
			} else {
				bufp = fs_rh_config_descriptor;
				len = sizeof fs_rh_config_descriptor;
			}
			if (device_can_wakeup(&hcd->self.root_hub->dev))
				patch_wakeup = 1;
			break;
		case USB_DT_STRING << 8:
			if ((wValue & 0xff) < 4)
				urb->actual_length = rh_string(wValue & 0xff,
						hcd, ubuf, wLength);
			else /* unsupported IDs --> "protocol stall" */
				goto error;
			break;
		default:
			goto error;
		}
		break;
	case DeviceRequest | USB_REQ_GET_INTERFACE:
		tbuf [0] = 0;
		len = 1;
			/* FALLTHROUGH */
	case DeviceOutRequest | USB_REQ_SET_INTERFACE:
		break;
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		// wValue == urb->dev->devaddr
		dev_dbg (hcd->self.controller, "root hub device address %d\n",
			wValue);
		break;

	/* INTERFACE REQUESTS (no defined feature/status flags) */

	/* ENDPOINT REQUESTS */

	case EndpointRequest | USB_REQ_GET_STATUS:
		// ENDPOINT_HALT flag
		tbuf [0] = 0;
		tbuf [1] = 0;
		len = 2;
			/* FALLTHROUGH */
	case EndpointOutRequest | USB_REQ_CLEAR_FEATURE:
	case EndpointOutRequest | USB_REQ_SET_FEATURE:
		dev_dbg (hcd->self.controller, "no endpoint features yet\n");
		break;

	/* CLASS REQUESTS (and errors) */

	default:
		/* non-generic request */
		switch (typeReq) {
		case GetHubStatus:
		case GetPortStatus:
			len = 4;
			break;
		case GetHubDescriptor:
			len = sizeof (struct usb_hub_descriptor);
			break;
		}
		status = hcd->driver->hub_control (hcd,
			typeReq, wValue, wIndex,
			tbuf, wLength);
		break;
error:
		/* "protocol stall" on error */
		status = -EPIPE;
	}

	if (status) {
		len = 0;
		if (status != -EPIPE) {
			dev_dbg (hcd->self.controller,
				"CTRL: TypeReq=0x%x val=0x%x "
				"idx=0x%x len=%d ==> %d\n",
				typeReq, wValue, wIndex,
				wLength, status);
		}
	}
	if (len) {
		if (urb->transfer_buffer_length < len)
			len = urb->transfer_buffer_length;
		urb->actual_length = len;
		// always USB_DIR_IN, toward host
		memcpy (ubuf, bufp, len);

		/* report whether RH hardware supports remote wakeup */
		if (patch_wakeup &&
				len > offsetof (struct usb_config_descriptor,
						bmAttributes))
			((struct usb_config_descriptor *)ubuf)->bmAttributes
				|= USB_CONFIG_ATT_WAKEUP;

		/* report whether RH hardware has an integrated TT */
		if (patch_protocol &&
				len > offsetof(struct usb_device_descriptor,
						bDeviceProtocol))
			((struct usb_device_descriptor *) ubuf)->
					bDeviceProtocol = 1;
	}

	/* any errors get returned through the urb completion */
	spin_lock_irq(&hcd_root_hub_lock);
	usb_hcd_unlink_urb_from_ep(hcd, urb);

	/* This peculiar use of spinlocks echoes what real HC drivers do.
	 * Avoiding calls to local_irq_disable/enable makes the code
	 * RT-friendly.
	 */
	spin_unlock(&hcd_root_hub_lock);
	usb_hcd_giveback_urb(hcd, urb, status);
	spin_lock(&hcd_root_hub_lock);

	spin_unlock_irq(&hcd_root_hub_lock);
	return 0;
}

/*-------------------------------------------------------------------------*/

/*
 * Root Hub interrupt transfers are polled using a timer if the
 * driver requests it; otherwise the driver is responsible for
 * calling usb_hcd_poll_rh_status() when an event occurs.
 *
 * Completions are called in_interrupt(), but they may or may not
 * be in_irq().
 */
void usb_hcd_poll_rh_status(struct usb_hcd *hcd)
{
	struct urb	*urb;
	int		length;
	unsigned long	flags;
	char		buffer[4];	/* Any root hubs with > 31 ports? */

	if (unlikely(!hcd->rh_pollable))
		return;
	if (!hcd->uses_new_polling && !hcd->status_urb)
		return;

	length = hcd->driver->hub_status_data(hcd, buffer);
	if (length > 0) {

		/* try to complete the status urb */
		spin_lock_irqsave(&hcd_root_hub_lock, flags);
		urb = hcd->status_urb;
		if (urb) {
			hcd->poll_pending = 0;
			hcd->status_urb = NULL;
			urb->actual_length = length;
			memcpy(urb->transfer_buffer, buffer, length);

			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock(&hcd_root_hub_lock);
			usb_hcd_giveback_urb(hcd, urb, 0);
			spin_lock(&hcd_root_hub_lock);
		} else {
			length = 0;
			hcd->poll_pending = 1;
		}
		spin_unlock_irqrestore(&hcd_root_hub_lock, flags);
	}

	/* The USB 2.0 spec says 256 ms.  This is close enough and won't
	 * exceed that limit if HZ is 100. The math is more clunky than
	 * maybe expected, this is to make sure that all timers for USB devices
	 * fire at the same time to give the CPU a break inbetween */
	if (hcd->uses_new_polling ? hcd->poll_rh :
			(length == 0 && hcd->status_urb != NULL))
		mod_timer (&hcd->rh_timer, (jiffies/(HZ/4) + 1) * (HZ/4));
}
EXPORT_SYMBOL_GPL(usb_hcd_poll_rh_status);

/* timer callback */
static void rh_timer_func (unsigned long _hcd)
{
	usb_hcd_poll_rh_status((struct usb_hcd *) _hcd);
}

/*-------------------------------------------------------------------------*/

static int rh_queue_status (struct usb_hcd *hcd, struct urb *urb)
{
	int		retval;
	unsigned long	flags;
	unsigned	len = 1 + (urb->dev->maxchild / 8);

	spin_lock_irqsave (&hcd_root_hub_lock, flags);
	if (hcd->status_urb || urb->transfer_buffer_length < len) {
		dev_dbg (hcd->self.controller, "not queuing rh status urb\n");
		retval = -EINVAL;
		goto done;
	}

	retval = usb_hcd_link_urb_to_ep(hcd, urb);
	if (retval)
		goto done;

	hcd->status_urb = urb;
	urb->hcpriv = hcd;	/* indicate it's queued */
	if (!hcd->uses_new_polling)
		mod_timer(&hcd->rh_timer, (jiffies/(HZ/4) + 1) * (HZ/4));

	/* If a status change has already occurred, report it ASAP */
	else if (hcd->poll_pending)
		mod_timer(&hcd->rh_timer, jiffies);
	retval = 0;
 done:
	spin_unlock_irqrestore (&hcd_root_hub_lock, flags);
	return retval;
}

static int rh_urb_enqueue (struct usb_hcd *hcd, struct urb *urb)
{
	if (usb_endpoint_xfer_int(&urb->ep->desc))
		return rh_queue_status (hcd, urb);
	if (usb_endpoint_xfer_control(&urb->ep->desc))
		return rh_call_control (hcd, urb);
	return -EINVAL;
}

/*-------------------------------------------------------------------------*/

/* Unlinks of root-hub control URBs are legal, but they don't do anything
 * since these URBs always execute synchronously.
 */
static int usb_rh_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	unsigned long	flags;
	int		rc;

	spin_lock_irqsave(&hcd_root_hub_lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc)
		goto done;

	if (usb_endpoint_num(&urb->ep->desc) == 0) {	/* Control URB */
		;	/* Do nothing */

	} else {				/* Status URB */
		if (!hcd->uses_new_polling)
			del_timer (&hcd->rh_timer);
		if (urb == hcd->status_urb) {
			hcd->status_urb = NULL;
			usb_hcd_unlink_urb_from_ep(hcd, urb);

			spin_unlock(&hcd_root_hub_lock);
			usb_hcd_giveback_urb(hcd, urb, status);
			spin_lock(&hcd_root_hub_lock);
		}
	}
 done:
	spin_unlock_irqrestore(&hcd_root_hub_lock, flags);
	return rc;
}

/*-------------------------------------------------------------------------*/

static struct class *usb_host_class;

int usb_host_init(void)
{
	int retval = 0;

	usb_host_class = class_create(THIS_MODULE, "usb_host");
	if (IS_ERR(usb_host_class))
		retval = PTR_ERR(usb_host_class);
	return retval;
}

void usb_host_cleanup(void)
{
	class_destroy(usb_host_class);
}

/**
 * usb_bus_init - shared initialization code
 * @bus: the bus structure being initialized
 *
 * This code is used to initialize a usb_bus structure, memory for which is
 * separately managed.
 */
static void usb_bus_init (struct usb_bus *bus)
{
	memset (&bus->devmap, 0, sizeof(struct usb_devmap));

	bus->devnum_next = 1;

	bus->root_hub = NULL;
	bus->busnum = -1;
	bus->bandwidth_allocated = 0;
	bus->bandwidth_int_reqs  = 0;
	bus->bandwidth_isoc_reqs = 0;

	INIT_LIST_HEAD (&bus->bus_list);
}

/*-------------------------------------------------------------------------*/

/**
 * usb_register_bus - registers the USB host controller with the usb core
 * @bus: pointer to the bus to register
 * Context: !in_interrupt()
 *
 * Assigns a bus number, and links the controller into usbcore data
 * structures so that it can be seen by scanning the bus list.
 */
static int usb_register_bus(struct usb_bus *bus)
{
	int result = -E2BIG;
	int busnum;

	mutex_lock(&usb_bus_list_lock);
	busnum = find_next_zero_bit (busmap.busmap, USB_MAXBUS, 1);
	if (busnum >= USB_MAXBUS) {
		printk (KERN_ERR "%s: too many buses\n", usbcore_name);
		goto error_find_busnum;
	}
	set_bit (busnum, busmap.busmap);
	bus->busnum = busnum;

	bus->dev = device_create(usb_host_class, bus->controller, MKDEV(0, 0),
				 "usb_host%d", busnum);
	result = PTR_ERR(bus->dev);
	if (IS_ERR(bus->dev))
		goto error_create_class_dev;
	dev_set_drvdata(bus->dev, bus);

	/* Add it to the local list of buses */
	list_add (&bus->bus_list, &usb_bus_list);
	mutex_unlock(&usb_bus_list_lock);

	usb_notify_add_bus(bus);

	dev_info (bus->controller, "new USB bus registered, assigned bus "
		  "number %d\n", bus->busnum);
	return 0;

error_create_class_dev:
	clear_bit(busnum, busmap.busmap);
error_find_busnum:
	mutex_unlock(&usb_bus_list_lock);
	return result;
}

/**
 * usb_deregister_bus - deregisters the USB host controller
 * @bus: pointer to the bus to deregister
 * Context: !in_interrupt()
 *
 * Recycles the bus number, and unlinks the controller from usbcore data
 * structures so that it won't be seen by scanning the bus list.
 */
static void usb_deregister_bus (struct usb_bus *bus)
{
	dev_info (bus->controller, "USB bus %d deregistered\n", bus->busnum);

	/*
	 * NOTE: make sure that all the devices are removed by the
	 * controller code, as well as having it call this when cleaning
	 * itself up
	 */
	mutex_lock(&usb_bus_list_lock);
	list_del (&bus->bus_list);
	mutex_unlock(&usb_bus_list_lock);

	usb_notify_remove_bus(bus);

	clear_bit (bus->busnum, busmap.busmap);

	device_unregister(bus->dev);
}

/**
 * register_root_hub - called by usb_add_hcd() to register a root hub
 * @hcd: host controller for this root hub
 *
 * This function registers the root hub with the USB subsystem.  It sets up
 * the device properly in the device tree and then calls usb_new_device()
 * to register the usb device.  It also assigns the root hub's USB address
 * (always 1).
 */
static int register_root_hub(struct usb_hcd *hcd)
{
	struct device *parent_dev = hcd->self.controller;
	struct usb_device *usb_dev = hcd->self.root_hub;
	const int devnum = 1;
	int retval;

	usb_dev->devnum = devnum;
	usb_dev->bus->devnum_next = devnum + 1;
	memset (&usb_dev->bus->devmap.devicemap, 0,
			sizeof usb_dev->bus->devmap.devicemap);
	set_bit (devnum, usb_dev->bus->devmap.devicemap);
	usb_set_device_state(usb_dev, USB_STATE_ADDRESS);

	mutex_lock(&usb_bus_list_lock);

	usb_dev->ep0.desc.wMaxPacketSize = __constant_cpu_to_le16(64);
	retval = usb_get_device_descriptor(usb_dev, USB_DT_DEVICE_SIZE);
	if (retval != sizeof usb_dev->descriptor) {
		mutex_unlock(&usb_bus_list_lock);
		dev_dbg (parent_dev, "can't read %s device descriptor %d\n",
				usb_dev->dev.bus_id, retval);
		return (retval < 0) ? retval : -EMSGSIZE;
	}

	retval = usb_new_device (usb_dev);
	if (retval) {
		dev_err (parent_dev, "can't register root hub for %s, %d\n",
				usb_dev->dev.bus_id, retval);
	}
	mutex_unlock(&usb_bus_list_lock);

	if (retval == 0) {
		spin_lock_irq (&hcd_root_hub_lock);
		hcd->rh_registered = 1;
		spin_unlock_irq (&hcd_root_hub_lock);

		/* Did the HC die before the root hub was registered? */
		if (hcd->state == HC_STATE_HALT)
			usb_hc_died (hcd);	/* This time clean up */
	}

	return retval;
}

void usb_enable_root_hub_irq (struct usb_bus *bus)
{
	struct usb_hcd *hcd;

	hcd = container_of (bus, struct usb_hcd, self);
	if (hcd->driver->hub_irq_enable && hcd->state != HC_STATE_HALT)
		hcd->driver->hub_irq_enable (hcd);
}


/*-------------------------------------------------------------------------*/

/**
 * usb_calc_bus_time - approximate periodic transaction time in nanoseconds
 * @speed: from dev->speed; USB_SPEED_{LOW,FULL,HIGH}
 * @is_input: true iff the transaction sends data to the host
 * @isoc: true for isochronous transactions, false for interrupt ones
 * @bytecount: how many bytes in the transaction.
 *
 * Returns approximate bus time in nanoseconds for a periodic transaction.
 * See USB 2.0 spec section 5.11.3; only periodic transfers need to be
 * scheduled in software, this function is only used for such scheduling.
 */
long usb_calc_bus_time (int speed, int is_input, int isoc, int bytecount)
{
	unsigned long	tmp;

	switch (speed) {
	case USB_SPEED_LOW: 	/* INTR only */
		if (is_input) {
			tmp = (67667L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (64060L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		} else {
			tmp = (66700L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (64107L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		}
	case USB_SPEED_FULL:	/* ISOC or INTR */
		if (isoc) {
			tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (((is_input) ? 7268L : 6265L) + BW_HOST_DELAY + tmp);
		} else {
			tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (9107L + BW_HOST_DELAY + tmp);
		}
	case USB_SPEED_HIGH:	/* ISOC or INTR */
		// FIXME adjust for input vs output
		if (isoc)
			tmp = HS_NSECS_ISO (bytecount);
		else
			tmp = HS_NSECS (bytecount);
		return tmp;
	default:
		pr_debug ("%s: bogus device speed!\n", usbcore_name);
		return -1;
	}
}
EXPORT_SYMBOL_GPL(usb_calc_bus_time);


/*-------------------------------------------------------------------------*/

/*
 * Generic HC operations.
 */

/*-------------------------------------------------------------------------*/

/**
 * usb_hcd_link_urb_to_ep - add an URB to its endpoint queue
 * @hcd: host controller to which @urb was submitted
 * @urb: URB being submitted
 *
 * Host controller drivers should call this routine in their enqueue()
 * method.  The HCD's private spinlock must be held and interrupts must
 * be disabled.  The actions carried out here are required for URB
 * submission, as well as for endpoint shutdown and for usb_kill_urb.
 *
 * Returns 0 for no error, otherwise a negative error code (in which case
 * the enqueue() method must fail).  If no error occurs but enqueue() fails
 * anyway, it must call usb_hcd_unlink_urb_from_ep() before releasing
 * the private spinlock and returning.
 */
int usb_hcd_link_urb_to_ep(struct usb_hcd *hcd, struct urb *urb)
{
	int		rc = 0;

	spin_lock(&hcd_urb_list_lock);

	/* Check that the URB isn't being killed */
	if (unlikely(atomic_read(&urb->reject))) {
		rc = -EPERM;
		goto done;
	}

	if (unlikely(!urb->ep->enabled)) {
		rc = -ENOENT;
		goto done;
	}

	if (unlikely(!urb->dev->can_submit)) {
		rc = -EHOSTUNREACH;
		goto done;
	}

	/*
	 * Check the host controller's state and add the URB to the
	 * endpoint's queue.
	 */
	switch (hcd->state) {
	case HC_STATE_RUNNING:
	case HC_STATE_RESUMING:
		urb->unlinked = 0;
		list_add_tail(&urb->urb_list, &urb->ep->urb_list);
		break;
	default:
		rc = -ESHUTDOWN;
		goto done;
	}
 done:
	spin_unlock(&hcd_urb_list_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(usb_hcd_link_urb_to_ep);

/**
 * usb_hcd_check_unlink_urb - check whether an URB may be unlinked
 * @hcd: host controller to which @urb was submitted
 * @urb: URB being checked for unlinkability
 * @status: error code to store in @urb if the unlink succeeds
 *
 * Host controller drivers should call this routine in their dequeue()
 * method.  The HCD's private spinlock must be held and interrupts must
 * be disabled.  The actions carried out here are required for making
 * sure than an unlink is valid.
 *
 * Returns 0 for no error, otherwise a negative error code (in which case
 * the dequeue() method must fail).  The possible error codes are:
 *
 *	-EIDRM: @urb was not submitted or has already completed.
 *		The completion function may not have been called yet.
 *
 *	-EBUSY: @urb has already been unlinked.
 */
int usb_hcd_check_unlink_urb(struct usb_hcd *hcd, struct urb *urb,
		int status)
{
	struct list_head	*tmp;

	/* insist the urb is still queued */
	list_for_each(tmp, &urb->ep->urb_list) {
		if (tmp == &urb->urb_list)
			break;
	}
	if (tmp != &urb->urb_list)
		return -EIDRM;

	/* Any status except -EINPROGRESS means something already started to
	 * unlink this URB from the hardware.  So there's no more work to do.
	 */
	if (urb->unlinked)
		return -EBUSY;
	urb->unlinked = status;

	/* IRQ setup can easily be broken so that USB controllers
	 * never get completion IRQs ... maybe even the ones we need to
	 * finish unlinking the initial failed usb_set_address()
	 * or device descriptor fetch.
	 */
	if (!test_bit(HCD_FLAG_SAW_IRQ, &hcd->flags) &&
			!is_root_hub(urb->dev)) {
		dev_warn(hcd->self.controller, "Unlink after no-IRQ?  "
			"Controller is probably using the wrong IRQ.\n");
		set_bit(HCD_FLAG_SAW_IRQ, &hcd->flags);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(usb_hcd_check_unlink_urb);

/**
 * usb_hcd_unlink_urb_from_ep - remove an URB from its endpoint queue
 * @hcd: host controller to which @urb was submitted
 * @urb: URB being unlinked
 *
 * Host controller drivers should call this routine before calling
 * usb_hcd_giveback_urb().  The HCD's private spinlock must be held and
 * interrupts must be disabled.  The actions carried out here are required
 * for URB completion.
 */
void usb_hcd_unlink_urb_from_ep(struct usb_hcd *hcd, struct urb *urb)
{
	/* clear all state linking urb to this dev (and hcd) */
	spin_lock(&hcd_urb_list_lock);
	list_del_init(&urb->urb_list);
	spin_unlock(&hcd_urb_list_lock);
}
EXPORT_SYMBOL_GPL(usb_hcd_unlink_urb_from_ep);

/*
 * Some usb host controllers can only perform dma using a small SRAM area.
 * The usb core itself is however optimized for host controllers that can dma
 * using regular system memory - like pci devices doing bus mastering.
 *
 * To support host controllers with limited dma capabilites we provide dma
 * bounce buffers. This feature can be enabled using the HCD_LOCAL_MEM flag.
 * For this to work properly the host controller code must first use the
 * function dma_declare_coherent_memory() to point out which memory area
 * that should be used for dma allocations.
 *
 * The HCD_LOCAL_MEM flag then tells the usb code to allocate all data for
 * dma using dma_alloc_coherent() which in turn allocates from the memory
 * area pointed out with dma_declare_coherent_memory().
 *
 * So, to summarize...
 *
 * - We need "local" memory, canonical example being
 *   a small SRAM on a discrete controller being the
 *   only memory that the controller can read ...
 *   (a) "normal" kernel memory is no good, and
 *   (b) there's not enough to share
 *
 * - The only *portable* hook for such stuff in the
 *   DMA framework is dma_declare_coherent_memory()
 *
 * - So we use that, even though the primary requirement
 *   is that the memory be "local" (hence addressible
 *   by that device), not "coherent".
 *
 */

static int hcd_alloc_coherent(struct usb_bus *bus,
			      gfp_t mem_flags, dma_addr_t *dma_handle,
			      void **vaddr_handle, size_t size,
			      enum dma_data_direction dir)
{
	unsigned char *vaddr;

	vaddr = hcd_buffer_alloc(bus, size + sizeof(vaddr),
				 mem_flags, dma_handle);
	if (!vaddr)
		return -ENOMEM;

	/*
	 * Store the virtual address of the buffer at the end
	 * of the allocated dma buffer. The size of the buffer
	 * may be uneven so use unaligned functions instead
	 * of just rounding up. It makes sense to optimize for
	 * memory footprint over access speed since the amount
	 * of memory available for dma may be limited.
	 */
	put_unaligned((unsigned long)*vaddr_handle,
		      (unsigned long *)(vaddr + size));

	if (dir == DMA_TO_DEVICE)
		memcpy(vaddr, *vaddr_handle, size);

	*vaddr_handle = vaddr;
	return 0;
}

static void hcd_free_coherent(struct usb_bus *bus, dma_addr_t *dma_handle,
			      void **vaddr_handle, size_t size,
			      enum dma_data_direction dir)
{
	unsigned char *vaddr = *vaddr_handle;

	vaddr = (void *)get_unaligned((unsigned long *)(vaddr + size));

	if (dir == DMA_FROM_DEVICE)
		memcpy(vaddr, *vaddr_handle, size);

	hcd_buffer_free(bus, size + sizeof(vaddr), *vaddr_handle, *dma_handle);

	*vaddr_handle = vaddr;
	*dma_handle = 0;
}

static int map_urb_for_dma(struct usb_hcd *hcd, struct urb *urb,
			   gfp_t mem_flags)
{
	if (hcd->driver->map_urb_for_dma)
		return hcd->driver->map_urb_for_dma(hcd, urb, mem_flags);
	else
		return usb_hcd_map_urb_for_dma(hcd, urb, mem_flags);
}

int usb_hcd_map_urb_for_dma(struct usb_hcd *hcd, struct urb *urb,
			    gfp_t mem_flags)
{
	enum dma_data_direction dir;
	int ret = 0;

	/* Map the URB's buffers for DMA access.
	 * Lower level HCD code should use *_dma exclusively,
	 * unless it uses pio or talks to another transport.
	 */
	if (is_root_hub(urb->dev))
		return 0;

	if (usb_endpoint_xfer_control(&urb->ep->desc)
	    && !(urb->transfer_flags & URB_NO_SETUP_DMA_MAP)) {
		if (hcd->self.uses_dma)
			urb->setup_dma = dma_map_single(
					hcd->self.controller,
					urb->setup_packet,
					sizeof(struct usb_ctrlrequest),
					DMA_TO_DEVICE);
		else if (hcd->driver->flags & HCD_LOCAL_MEM)
			ret = hcd_alloc_coherent(
					urb->dev->bus, mem_flags,
					&urb->setup_dma,
					(void **)&urb->setup_packet,
					sizeof(struct usb_ctrlrequest),
					DMA_TO_DEVICE);
	}

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	if (ret == 0 && urb->transfer_buffer_length != 0
	    && !(urb->transfer_flags & URB_NO_TRANSFER_DMA_MAP)) {
		if (hcd->self.uses_dma)
			urb->transfer_dma = dma_map_single (
					hcd->self.controller,
					urb->transfer_buffer,
					urb->transfer_buffer_length,
					dir);
		else if (hcd->driver->flags & HCD_LOCAL_MEM) {
			ret = hcd_alloc_coherent(
					urb->dev->bus, mem_flags,
					&urb->transfer_dma,
					&urb->transfer_buffer,
					urb->transfer_buffer_length,
					dir);

			if (ret && usb_endpoint_xfer_control(&urb->ep->desc)
			    && !(urb->transfer_flags & URB_NO_SETUP_DMA_MAP))
				hcd_free_coherent(urb->dev->bus,
					&urb->setup_dma,
					(void **)&urb->setup_packet,
					sizeof(struct usb_ctrlrequest),
					DMA_TO_DEVICE);
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(usb_hcd_map_urb_for_dma);

static void unmap_urb_for_dma(struct usb_hcd *hcd, struct urb *urb)
{
	if (hcd->driver->unmap_urb_for_dma)
		hcd->driver->unmap_urb_for_dma(hcd, urb);
	else
		usb_hcd_unmap_urb_for_dma(hcd, urb);
}

void usb_hcd_unmap_urb_for_dma(struct usb_hcd *hcd, struct urb *urb)
{
	enum dma_data_direction dir;

	if (is_root_hub(urb->dev))
		return;

	if (usb_endpoint_xfer_control(&urb->ep->desc)
	    && !(urb->transfer_flags & URB_NO_SETUP_DMA_MAP)) {
		if (hcd->self.uses_dma)
			dma_unmap_single(hcd->self.controller, urb->setup_dma,
					sizeof(struct usb_ctrlrequest),
					DMA_TO_DEVICE);
		else if (hcd->driver->flags & HCD_LOCAL_MEM)
			hcd_free_coherent(urb->dev->bus, &urb->setup_dma,
					(void **)&urb->setup_packet,
					sizeof(struct usb_ctrlrequest),
					DMA_TO_DEVICE);
	}

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	if (urb->transfer_buffer_length != 0
	    && !(urb->transfer_flags & URB_NO_TRANSFER_DMA_MAP)) {
		if (hcd->self.uses_dma)
			dma_unmap_single(hcd->self.controller,
					urb->transfer_dma,
					urb->transfer_buffer_length,
					dir);
		else if (hcd->driver->flags & HCD_LOCAL_MEM)
			hcd_free_coherent(urb->dev->bus, &urb->transfer_dma,
					&urb->transfer_buffer,
					urb->transfer_buffer_length,
					dir);
	}
}
EXPORT_SYMBOL_GPL(usb_hcd_unmap_urb_for_dma);

/*-------------------------------------------------------------------------*/

/* may be called in any context with a valid urb->dev usecount
 * caller surrenders "ownership" of urb
 * expects usb_submit_urb() to have sanity checked and conditioned all
 * inputs in the urb
 */
int usb_hcd_submit_urb (struct urb *urb, gfp_t mem_flags)
{
	int			status;
	struct usb_hcd		*hcd = bus_to_hcd(urb->dev->bus);

	/* increment urb's reference count as part of giving it to the HCD
	 * (which will control it).  HCD guarantees that it either returns
	 * an error or calls giveback(), but not both.
	 */
	usb_get_urb(urb);
	atomic_inc(&urb->use_count);
	atomic_inc(&urb->dev->urbnum);
	usbmon_urb_submit(&hcd->self, urb);

	/* NOTE requirements on root-hub callers (usbfs and the hub
	 * driver, for now):  URBs' urb->transfer_buffer must be
	 * valid and usb_buffer_{sync,unmap}() not be needed, since
	 * they could clobber root hub response data.  Also, control
	 * URBs must be submitted in process context with interrupts
	 * enabled.
	 */
	status = map_urb_for_dma(hcd, urb, mem_flags);
	if (unlikely(status)) {
		usbmon_urb_submit_error(&hcd->self, urb, status);
		goto error;
	}

	if (is_root_hub(urb->dev))
		status = rh_urb_enqueue(hcd, urb);
	else
		status = hcd->driver->urb_enqueue(hcd, urb, mem_flags);

	if (unlikely(status)) {
		usbmon_urb_submit_error(&hcd->self, urb, status);
		unmap_urb_for_dma(hcd, urb);
 error:
		urb->hcpriv = NULL;
		INIT_LIST_HEAD(&urb->urb_list);
		atomic_dec(&urb->use_count);
		atomic_dec(&urb->dev->urbnum);
		if (atomic_read(&urb->reject))
			wake_up(&usb_kill_urb_queue);
		usb_put_urb(urb);
	}
	return status;
}

/*-------------------------------------------------------------------------*/

/* this makes the hcd giveback() the urb more quickly, by kicking it
 * off hardware queues (which may take a while) and returning it as
 * soon as practical.  we've already set up the urb's return status,
 * but we can't know if the callback completed already.
 */
static int unlink1(struct usb_hcd *hcd, struct urb *urb, int status)
{
	int		value;

	if (is_root_hub(urb->dev))
		value = usb_rh_urb_dequeue(hcd, urb, status);
	else {

		/* The only reason an HCD might fail this call is if
		 * it has not yet fully queued the urb to begin with.
		 * Such failures should be harmless. */
		value = hcd->driver->urb_dequeue(hcd, urb, status);
	}
	return value;
}

/*
 * called in any context
 *
 * caller guarantees urb won't be recycled till both unlink()
 * and the urb's completion function return
 */
int usb_hcd_unlink_urb (struct urb *urb, int status)
{
	struct usb_hcd		*hcd;
	int			retval = -EIDRM;
	unsigned long		flags;

	/* Prevent the device and bus from going away while
	 * the unlink is carried out.  If they are already gone
	 * then urb->use_count must be 0, since disconnected
	 * devices can't have any active URBs.
	 */
	spin_lock_irqsave(&hcd_urb_unlink_lock, flags);
	if (atomic_read(&urb->use_count) > 0) {
		retval = 0;
		usb_get_dev(urb->dev);
	}
	spin_unlock_irqrestore(&hcd_urb_unlink_lock, flags);
	if (retval == 0) {
		hcd = bus_to_hcd(urb->dev->bus);
		retval = unlink1(hcd, urb, status);
		usb_put_dev(urb->dev);
	}

	if (retval == 0)
		retval = -EINPROGRESS;
	else if (retval != -EIDRM && retval != -EBUSY)
		dev_dbg(&urb->dev->dev, "hcd_unlink_urb %p fail %d\n",
				urb, retval);
	return retval;
}

/*-------------------------------------------------------------------------*/

/**
 * usb_hcd_giveback_urb - return URB from HCD to device driver
 * @hcd: host controller returning the URB
 * @urb: urb being returned to the USB device driver.
 * @status: completion status code for the URB.
 * Context: in_interrupt()
 *
 * This hands the URB from HCD to its USB device driver, using its
 * completion function.  The HCD has freed all per-urb resources
 * (and is done using urb->hcpriv).  It also released all HCD locks;
 * the device driver won't cause problems if it frees, modifies,
 * or resubmits this URB.
 *
 * If @urb was unlinked, the value of @status will be overridden by
 * @urb->unlinked.  Erroneous short transfers are detected in case
 * the HCD hasn't checked for them.
 */
void usb_hcd_giveback_urb(struct usb_hcd *hcd, struct urb *urb, int status)
{
	urb->hcpriv = NULL;
	if (unlikely(urb->unlinked))
		status = urb->unlinked;
	else if (unlikely((urb->transfer_flags & URB_SHORT_NOT_OK) &&
			urb->actual_length < urb->transfer_buffer_length &&
			!status))
		status = -EREMOTEIO;

	unmap_urb_for_dma(hcd, urb);
	usbmon_urb_complete(&hcd->self, urb, status);
	usb_unanchor_urb(urb);

	/* pass ownership to the completion handler */
	urb->status = status;
	urb->complete (urb);
	atomic_dec (&urb->use_count);
	if (unlikely(atomic_read(&urb->reject)))
		wake_up (&usb_kill_urb_queue);
	usb_put_urb (urb);
}
EXPORT_SYMBOL_GPL(usb_hcd_giveback_urb);

/*-------------------------------------------------------------------------*/

/* Cancel all URBs pending on this endpoint and wait for the endpoint's
 * queue to drain completely.  The caller must first insure that no more
 * URBs can be submitted for this endpoint.
 */
void usb_hcd_flush_endpoint(struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	struct usb_hcd		*hcd;
	struct urb		*urb;

	if (!ep)
		return;
	might_sleep();
	hcd = bus_to_hcd(udev->bus);

 	/* No more submits can occur */
	spin_lock_irq(&hcd_urb_list_lock);
rescan:
	list_for_each_entry (urb, &ep->urb_list, urb_list) {
		int	is_in;

		if (urb->unlinked)
			continue;
		usb_get_urb (urb);
		is_in = usb_urb_dir_in(urb);
		spin_unlock(&hcd_urb_list_lock);

		/* kick hcd */
		unlink1(hcd, urb, -ESHUTDOWN);
		dev_dbg (hcd->self.controller,
			"shutdown urb %p ep%d%s%s\n",
			urb, usb_endpoint_num(&ep->desc),
			is_in ? "in" : "out",
			({	char *s;

				 switch (usb_endpoint_type(&ep->desc)) {
				 case USB_ENDPOINT_XFER_CONTROL:
					s = ""; break;
				 case USB_ENDPOINT_XFER_BULK:
					s = "-bulk"; break;
				 case USB_ENDPOINT_XFER_INT:
					s = "-intr"; break;
				 default:
			 		s = "-iso"; break;
				};
				s;
			}));
		usb_put_urb (urb);

		/* list contents may have changed */
		spin_lock(&hcd_urb_list_lock);
		goto rescan;
	}
	spin_unlock_irq(&hcd_urb_list_lock);

	/* Wait until the endpoint queue is completely empty */
	while (!list_empty (&ep->urb_list)) {
		spin_lock_irq(&hcd_urb_list_lock);

		/* The list may have changed while we acquired the spinlock */
		urb = NULL;
		if (!list_empty (&ep->urb_list)) {
			urb = list_entry (ep->urb_list.prev, struct urb,
					urb_list);
			usb_get_urb (urb);
		}
		spin_unlock_irq(&hcd_urb_list_lock);

		if (urb) {
			usb_kill_urb (urb);
			usb_put_urb (urb);
		}
	}
}

/* Disables the endpoint: synchronizes with the hcd to make sure all
 * endpoint state is gone from hardware.  usb_hcd_flush_endpoint() must
 * have been called previously.  Use for set_configuration, set_interface,
 * driver removal, physical disconnect.
 *
 * example:  a qh stored in ep->hcpriv, holding state related to endpoint
 * type, maxpacket size, toggle, halt status, and scheduling.
 */
void usb_hcd_disable_endpoint(struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	struct usb_hcd		*hcd;

	might_sleep();
	hcd = bus_to_hcd(udev->bus);
	if (hcd->driver->endpoint_disable)
		hcd->driver->endpoint_disable(hcd, ep);
}

/* Protect against drivers that try to unlink URBs after the device
 * is gone, by waiting until all unlinks for @udev are finished.
 * Since we don't currently track URBs by device, simply wait until
 * nothing is running in the locked region of usb_hcd_unlink_urb().
 */
void usb_hcd_synchronize_unlinks(struct usb_device *udev)
{
	spin_lock_irq(&hcd_urb_unlink_lock);
	spin_unlock_irq(&hcd_urb_unlink_lock);
}

/*-------------------------------------------------------------------------*/

/* called in any context */
int usb_hcd_get_frame_number (struct usb_device *udev)
{
	struct usb_hcd	*hcd = bus_to_hcd(udev->bus);

	if (!HC_IS_RUNNING (hcd->state))
		return -ESHUTDOWN;
	return hcd->driver->get_frame_number (hcd);
}

/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_PM

int hcd_bus_suspend(struct usb_device *rhdev)
{
	struct usb_hcd	*hcd = container_of(rhdev->bus, struct usb_hcd, self);
	int		status;
	int		old_state = hcd->state;

	dev_dbg(&rhdev->dev, "bus %s%s\n",
			rhdev->auto_pm ? "auto-" : "", "suspend");
	if (!hcd->driver->bus_suspend) {
		status = -ENOENT;
	} else {
		hcd->state = HC_STATE_QUIESCING;
		status = hcd->driver->bus_suspend(hcd);
	}
	if (status == 0) {
		usb_set_device_state(rhdev, USB_STATE_SUSPENDED);
		hcd->state = HC_STATE_SUSPENDED;
	} else {
		hcd->state = old_state;
		dev_dbg(&rhdev->dev, "bus %s fail, err %d\n",
				"suspend", status);
	}
	return status;
}

int hcd_bus_resume(struct usb_device *rhdev)
{
	struct usb_hcd	*hcd = container_of(rhdev->bus, struct usb_hcd, self);
	int		status;
	int		old_state = hcd->state;

	dev_dbg(&rhdev->dev, "usb %s%s\n",
			rhdev->auto_pm ? "auto-" : "", "resume");
	if (!hcd->driver->bus_resume)
		return -ENOENT;
	if (hcd->state == HC_STATE_RUNNING)
		return 0;

	hcd->state = HC_STATE_RESUMING;
	status = hcd->driver->bus_resume(hcd);
	if (status == 0) {
		/* TRSMRCY = 10 msec */
		msleep(10);
		usb_set_device_state(rhdev, rhdev->actconfig
				? USB_STATE_CONFIGURED
				: USB_STATE_ADDRESS);
		hcd->state = HC_STATE_RUNNING;
	} else {
		hcd->state = old_state;
		dev_dbg(&rhdev->dev, "bus %s fail, err %d\n",
				"resume", status);
		if (status != -ESHUTDOWN)
			usb_hc_died(hcd);
	}
	return status;
}

/* Workqueue routine for root-hub remote wakeup */
static void hcd_resume_work(struct work_struct *work)
{
	struct usb_hcd *hcd = container_of(work, struct usb_hcd, wakeup_work);
	struct usb_device *udev = hcd->self.root_hub;

	usb_lock_device(udev);
	usb_mark_last_busy(udev);
	usb_external_resume_device(udev);
	usb_unlock_device(udev);
}

/**
 * usb_hcd_resume_root_hub - called by HCD to resume its root hub 
 * @hcd: host controller for this root hub
 *
 * The USB host controller calls this function when its root hub is
 * suspended (with the remote wakeup feature enabled) and a remote
 * wakeup request is received.  The routine submits a workqueue request
 * to resume the root hub (that is, manage its downstream ports again).
 */
void usb_hcd_resume_root_hub (struct usb_hcd *hcd)
{
	unsigned long flags;

	spin_lock_irqsave (&hcd_root_hub_lock, flags);
	if (hcd->rh_registered)
		queue_work(ksuspend_usb_wq, &hcd->wakeup_work);
	spin_unlock_irqrestore (&hcd_root_hub_lock, flags);
}
EXPORT_SYMBOL_GPL(usb_hcd_resume_root_hub);

#endif

/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_USB_OTG

/**
 * usb_bus_start_enum - start immediate enumeration (for OTG)
 * @bus: the bus (must use hcd framework)
 * @port_num: 1-based number of port; usually bus->otg_port
 * Context: in_interrupt()
 *
 * Starts enumeration, with an immediate reset followed later by
 * khubd identifying and possibly configuring the device.
 * This is needed by OTG controller drivers, where it helps meet
 * HNP protocol timing requirements for starting a port reset.
 */
int usb_bus_start_enum(struct usb_bus *bus, unsigned port_num)
{
	struct usb_hcd		*hcd;
	int			status = -EOPNOTSUPP;

	/* NOTE: since HNP can't start by grabbing the bus's address0_sem,
	 * boards with root hubs hooked up to internal devices (instead of
	 * just the OTG port) may need more attention to resetting...
	 */
	hcd = container_of (bus, struct usb_hcd, self);
	if (port_num && hcd->driver->start_port_reset)
		status = hcd->driver->start_port_reset(hcd, port_num);

	/* run khubd shortly after (first) root port reset finishes;
	 * it may issue others, until at least 50 msecs have passed.
	 */
	if (status == 0)
		mod_timer(&hcd->rh_timer, jiffies + msecs_to_jiffies(10));
	return status;
}
EXPORT_SYMBOL_GPL(usb_bus_start_enum);

#endif

/*-------------------------------------------------------------------------*/

/**
 * usb_hcd_irq - hook IRQs to HCD framework (bus glue)
 * @irq: the IRQ being raised
 * @__hcd: pointer to the HCD whose IRQ is being signaled
 *
 * If the controller isn't HALTed, calls the driver's irq handler.
 * Checks whether the controller is now dead.
 */
irqreturn_t usb_hcd_irq (int irq, void *__hcd)
{
	struct usb_hcd		*hcd = __hcd;
	unsigned long		flags;
	irqreturn_t		rc;

	/* IRQF_DISABLED doesn't work correctly with shared IRQs
	 * when the first handler doesn't use it.  So let's just
	 * assume it's never used.
	 */
	local_irq_save(flags);

	if (unlikely(hcd->state == HC_STATE_HALT ||
		     !test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags))) {
		rc = IRQ_NONE;
	} else if (hcd->driver->irq(hcd) == IRQ_NONE) {
		rc = IRQ_NONE;
	} else {
		set_bit(HCD_FLAG_SAW_IRQ, &hcd->flags);

		if (unlikely(hcd->state == HC_STATE_HALT))
			usb_hc_died(hcd);
		rc = IRQ_HANDLED;
	}

	local_irq_restore(flags);
	return rc;
}

/*-------------------------------------------------------------------------*/

/**
 * usb_hc_died - report abnormal shutdown of a host controller (bus glue)
 * @hcd: pointer to the HCD representing the controller
 *
 * This is called by bus glue to report a USB host controller that died
 * while operations may still have been pending.  It's called automatically
 * by the PCI glue, so only glue for non-PCI busses should need to call it. 
 */
void usb_hc_died (struct usb_hcd *hcd)
{
	unsigned long flags;

	dev_err (hcd->self.controller, "HC died; cleaning up\n");

	spin_lock_irqsave (&hcd_root_hub_lock, flags);
	if (hcd->rh_registered) {
		hcd->poll_rh = 0;

		/* make khubd clean up old urbs and devices */
		usb_set_device_state (hcd->self.root_hub,
				USB_STATE_NOTATTACHED);
		usb_kick_khubd (hcd->self.root_hub);
	}
	spin_unlock_irqrestore (&hcd_root_hub_lock, flags);
}
EXPORT_SYMBOL_GPL (usb_hc_died);

/*-------------------------------------------------------------------------*/

/**
 * usb_create_hcd - create and initialize an HCD structure
 * @driver: HC driver that will use this hcd
 * @dev: device for this HC, stored in hcd->self.controller
 * @bus_name: value to store in hcd->self.bus_name
 * Context: !in_interrupt()
 *
 * Allocate a struct usb_hcd, with extra space at the end for the
 * HC driver's private data.  Initialize the generic members of the
 * hcd structure.
 *
 * If memory is unavailable, returns NULL.
 */
struct usb_hcd *usb_create_hcd (const struct hc_driver *driver,
		struct device *dev, char *bus_name)
{
	struct usb_hcd *hcd;

	hcd = kzalloc(sizeof(*hcd) + driver->hcd_priv_size, GFP_KERNEL);
	if (!hcd) {
		dev_dbg (dev, "hcd alloc failed\n");
		return NULL;
	}
	dev_set_drvdata(dev, hcd);
	kref_init(&hcd->kref);

	usb_bus_init(&hcd->self);
	hcd->self.controller = dev;
	hcd->self.bus_name = bus_name;
	hcd->self.uses_dma = (dev->dma_mask != NULL);

	init_timer(&hcd->rh_timer);
	hcd->rh_timer.function = rh_timer_func;
	hcd->rh_timer.data = (unsigned long) hcd;
#ifdef CONFIG_PM
	INIT_WORK(&hcd->wakeup_work, hcd_resume_work);
#endif

	hcd->driver = driver;
	hcd->product_desc = (driver->product_desc) ? driver->product_desc :
			"USB Host Controller";

	return hcd;
}
EXPORT_SYMBOL_GPL(usb_create_hcd);

static void hcd_release (struct kref *kref)
{
	struct usb_hcd *hcd = container_of (kref, struct usb_hcd, kref);

	kfree(hcd);
}

struct usb_hcd *usb_get_hcd (struct usb_hcd *hcd)
{
	if (hcd)
		kref_get (&hcd->kref);
	return hcd;
}
EXPORT_SYMBOL_GPL(usb_get_hcd);

void usb_put_hcd (struct usb_hcd *hcd)
{
	if (hcd)
		kref_put (&hcd->kref, hcd_release);
}
EXPORT_SYMBOL_GPL(usb_put_hcd);

/**
 * usb_add_hcd - finish generic HCD structure initialization and register
 * @hcd: the usb_hcd structure to initialize
 * @irqnum: Interrupt line to allocate
 * @irqflags: Interrupt type flags
 *
 * Finish the remaining parts of generic HCD initialization: allocate the
 * buffers of consistent memory, register the bus, request the IRQ line,
 * and call the driver's reset() and start() routines.
 */
int usb_add_hcd(struct usb_hcd *hcd,
		unsigned int irqnum, unsigned long irqflags)
{
	int retval;
	struct usb_device *rhdev;

	dev_info(hcd->self.controller, "%s\n", hcd->product_desc);

	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	/* HC is in reset state, but accessible.  Now do the one-time init,
	 * bottom up so that hcds can customize the root hubs before khubd
	 * starts talking to them.  (Note, bus id is assigned early too.)
	 */
	if ((retval = hcd_buffer_create(hcd)) != 0) {
		dev_dbg(hcd->self.controller, "pool alloc failed\n");
		return retval;
	}

	if ((retval = usb_register_bus(&hcd->self)) < 0)
		goto err_register_bus;

	if ((rhdev = usb_alloc_dev(NULL, &hcd->self, 0)) == NULL) {
		dev_err(hcd->self.controller, "unable to allocate root hub\n");
		retval = -ENOMEM;
		goto err_allocate_root_hub;
	}
	rhdev->speed = (hcd->driver->flags & HCD_USB2) ? USB_SPEED_HIGH :
			USB_SPEED_FULL;
	hcd->self.root_hub = rhdev;

	/* wakeup flag init defaults to "everything works" for root hubs,
	 * but drivers can override it in reset() if needed, along with
	 * recording the overall controller's system wakeup capability.
	 */
	device_init_wakeup(&rhdev->dev, 1);

	/* "reset" is misnamed; its role is now one-time init. the controller
	 * should already have been reset (and boot firmware kicked off etc).
	 */
	if (hcd->driver->reset && (retval = hcd->driver->reset(hcd)) < 0) {
		dev_err(hcd->self.controller, "can't setup\n");
		goto err_hcd_driver_setup;
	}
	hcd->rh_pollable = 1;

	/* NOTE: root hub and controller capabilities may not be the same */
	if (device_can_wakeup(hcd->self.controller)
			&& device_can_wakeup(&hcd->self.root_hub->dev))
		dev_dbg(hcd->self.controller, "supports USB remote wakeup\n");

	/* enable irqs just before we start the controller */
	if (hcd->driver->irq) {

		/* IRQF_DISABLED doesn't work as advertised when used together
		 * with IRQF_SHARED. As usb_hcd_irq() will always disable
		 * interrupts we can remove it here.
		 */
		if (irqflags & IRQF_SHARED)
			irqflags &= ~IRQF_DISABLED;

		snprintf(hcd->irq_descr, sizeof(hcd->irq_descr), "%s:usb%d",
				hcd->driver->description, hcd->self.busnum);
		if ((retval = request_irq(irqnum, &usb_hcd_irq, irqflags,
				hcd->irq_descr, hcd)) != 0) {
			dev_err(hcd->self.controller,
					"request interrupt %d failed\n", irqnum);
			goto err_request_irq;
		}
		hcd->irq = irqnum;
		dev_info(hcd->self.controller, "irq %d, %s 0x%08llx\n", irqnum,
				(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
	} else {
		hcd->irq = -1;
		if (hcd->rsrc_start)
			dev_info(hcd->self.controller, "%s 0x%08llx\n",
					(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
	}

	if ((retval = hcd->driver->start(hcd)) < 0) {
		dev_err(hcd->self.controller, "startup error %d\n", retval);
		goto err_hcd_driver_start;
	}

	/* starting here, usbcore will pay attention to this root hub */
	rhdev->bus_mA = min(500u, hcd->power_budget);
	if ((retval = register_root_hub(hcd)) != 0)
		goto err_register_root_hub;

	if (hcd->uses_new_polling && hcd->poll_rh)
		usb_hcd_poll_rh_status(hcd);
	return retval;

err_register_root_hub:
	hcd->rh_pollable = 0;
	hcd->poll_rh = 0;
	del_timer_sync(&hcd->rh_timer);
	hcd->driver->stop(hcd);
err_hcd_driver_start:
	if (hcd->irq >= 0)
		free_irq(irqnum, hcd);
err_request_irq:
err_hcd_driver_setup:
	usb_put_dev(hcd->self.root_hub);
err_allocate_root_hub:
	usb_deregister_bus(&hcd->self);
err_register_bus:
	hcd_buffer_destroy(hcd);
	return retval;
} 
EXPORT_SYMBOL_GPL(usb_add_hcd);

/**
 * usb_remove_hcd - shutdown processing for generic HCDs
 * @hcd: the usb_hcd structure to remove
 * Context: !in_interrupt()
 *
 * Disconnects the root hub, then reverses the effects of usb_add_hcd(),
 * invoking the HCD's stop() method.
 */
void usb_remove_hcd(struct usb_hcd *hcd)
{
	struct usb_device *rhdev = hcd->self.root_hub;

	dev_info(hcd->self.controller, "remove, state %x\n", hcd->state);
	usb_get_dev(rhdev);

	if (HC_IS_RUNNING (hcd->state))
		hcd->state = HC_STATE_QUIESCING;

	dev_dbg(hcd->self.controller, "roothub graceful disconnect\n");
	spin_lock_irq (&hcd_root_hub_lock);
	hcd->rh_registered = 0;
	spin_unlock_irq (&hcd_root_hub_lock);

#ifdef CONFIG_PM
	cancel_work_sync(&hcd->wakeup_work);
#endif

	mutex_lock(&usb_bus_list_lock);
	usb_disconnect(&rhdev);		/* Sets rhdev to NULL */
	mutex_unlock(&usb_bus_list_lock);

	/* Prevent any more root-hub status calls from the timer.
	 * The HCD might still restart the timer (if a port status change
	 * interrupt occurs), but usb_hcd_poll_rh_status() won't invoke
	 * the hub_status_data() callback.
	 */
	hcd->rh_pollable = 0;
	hcd->poll_rh = 0;
	del_timer_sync(&hcd->rh_timer);

	hcd->driver->stop(hcd);
	hcd->state = HC_STATE_HALT;

	/* In case the HCD restarted the timer, stop it again. */
	hcd->poll_rh = 0;
	del_timer_sync(&hcd->rh_timer);

	if (hcd->irq >= 0)
		free_irq(hcd->irq, hcd);

	usb_put_dev(hcd->self.root_hub);
	usb_deregister_bus(&hcd->self);
	hcd_buffer_destroy(hcd);
}
EXPORT_SYMBOL_GPL(usb_remove_hcd);

void
usb_hcd_platform_shutdown(struct platform_device* dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}
EXPORT_SYMBOL_GPL(usb_hcd_platform_shutdown);

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_USB_MON) || defined(CONFIG_USB_MON_MODULE)

struct usb_mon_operations *mon_ops;

/*
 * The registration is unlocked.
 * We do it this way because we do not want to lock in hot paths.
 *
 * Notice that the code is minimally error-proof. Because usbmon needs
 * symbols from usbcore, usbcore gets referenced and cannot be unloaded first.
 */
 
int usb_mon_register (struct usb_mon_operations *ops)
{

	if (mon_ops)
		return -EBUSY;

	mon_ops = ops;
	mb();
	return 0;
}
EXPORT_SYMBOL_GPL (usb_mon_register);

void usb_mon_deregister (void)
{

	if (mon_ops == NULL) {
		printk(KERN_ERR "USB: monitor was not registered\n");
		return;
	}
	mon_ops = NULL;
	mb();
}
EXPORT_SYMBOL_GPL (usb_mon_deregister);

#endif /* CONFIG_USB_MON || CONFIG_USB_MON_MODULE */
