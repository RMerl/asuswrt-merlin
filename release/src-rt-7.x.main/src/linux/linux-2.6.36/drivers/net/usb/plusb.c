/*
 * PL-2301/2302 USB host-to-host link cables
 * Copyright (C) 2000-2005 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// #define	DEBUG			// error path messages, extra info
// #define	VERBOSE			// more; success messages

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>


/*
 * Prolific PL-2301/PL-2302 driver ... http://www.prolifictech.com
 *
 * The protocol and handshaking used here should be bug-compatible
 * with the Linux 2.2 "plusb" driver, by Deti Fliegl.
 *
 * HEADS UP:  this handshaking isn't all that robust.  This driver
 * gets confused easily if you unplug one end of the cable then
 * try to connect it again; you'll need to restart both ends. The
 * "naplink" software (used by some PlayStation/2 deveopers) does
 * the handshaking much better!   Also, sometimes this hardware
 * seems to get wedged under load.  Prolific docs are weak, and
 * don't identify differences between PL2301 and PL2302, much less
 * anything to explain the different PL2302 versions observed.
 */

/*
 * Bits 0-4 can be used for software handshaking; they're set from
 * one end, cleared from the other, "read" with the interrupt byte.
 */
#define	PL_S_EN		(1<<7)		/* (feature only) suspend enable */
/* reserved bit -- rx ready (6) ? */
#define	PL_TX_READY	(1<<5)		/* (interrupt only) transmit ready */
#define	PL_RESET_OUT	(1<<4)		/* reset output pipe */
#define	PL_RESET_IN	(1<<3)		/* reset input pipe */
#define	PL_TX_C		(1<<2)		/* transmission complete */
#define	PL_TX_REQ	(1<<1)		/* transmission received */
#define	PL_PEER_E	(1<<0)		/* peer exists */

static inline int
pl_vendor_req(struct usbnet *dev, u8 req, u8 val, u8 index)
{
	return usb_control_msg(dev->udev,
		usb_rcvctrlpipe(dev->udev, 0),
		req,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		val, index,
		NULL, 0,
		USB_CTRL_GET_TIMEOUT);
}

static inline int
pl_clear_QuickLink_features(struct usbnet *dev, int val)
{
	return pl_vendor_req(dev, 1, (u8) val, 0);
}

static inline int
pl_set_QuickLink_features(struct usbnet *dev, int val)
{
	return pl_vendor_req(dev, 3, (u8) val, 0);
}

static int pl_reset(struct usbnet *dev)
{
	(void) pl_set_QuickLink_features(dev,
		PL_S_EN|PL_RESET_OUT|PL_RESET_IN|PL_PEER_E);
	return 0;
}

static const struct driver_info	prolific_info = {
	.description =	"Prolific PL-2301/PL-2302",
	.flags =	FLAG_NO_SETINT,
		/* some PL-2302 versions seem to fail usb_set_interface() */
	.reset =	pl_reset,
};


/*-------------------------------------------------------------------------*/

/*
 * Proilific's name won't normally be on the cables, and
 * may not be on the device.
 */

static const struct usb_device_id	products [] = {

{
	USB_DEVICE(0x067b, 0x0000),	// PL-2301
	.driver_info =	(unsigned long) &prolific_info,
}, {
	USB_DEVICE(0x067b, 0x0001),	// PL-2302
	.driver_info =	(unsigned long) &prolific_info,
},

	{ },		// END
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver plusb_driver = {
	.name =		"plusb",
	.id_table =	products,
	.probe =	usbnet_probe,
	.disconnect =	usbnet_disconnect,
	.suspend =	usbnet_suspend,
	.resume =	usbnet_resume,
};

static int __init plusb_init(void)
{
 	return usb_register(&plusb_driver);
}
module_init(plusb_init);

static void __exit plusb_exit(void)
{
 	usb_deregister(&plusb_driver);
}
module_exit(plusb_exit);

MODULE_AUTHOR("David Brownell");
MODULE_DESCRIPTION("Prolific PL-2301/2302 USB Host to Host Link Driver");
MODULE_LICENSE("GPL");
