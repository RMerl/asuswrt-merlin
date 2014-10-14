/*
 *  USB ATI Remote support
 *
 *  Version 2.2.0 Copyright (c) 2004 Torrey Hoffman <thoffman@arnor.net>
 *  Version 2.1.1 Copyright (c) 2002 Vladimir Dergachev
 *
 *  This 2.2.0 version is a rewrite / cleanup of the 2.1.1 driver, including
 *  porting to the 2.6 kernel interfaces, along with other modification
 *  to better match the style of the existing usb/input drivers.  However, the
 *  protocol and hardware handling is essentially unchanged from 2.1.1.
 *
 *  The 2.1.1 driver was derived from the usbati_remote and usbkbd drivers by
 *  Vojtech Pavlik.
 *
 *  Changes:
 *
 *  Feb 2004: Torrey Hoffman <thoffman@arnor.net>
 *            Version 2.2.0
 *  Jun 2004: Torrey Hoffman <thoffman@arnor.net>
 *            Version 2.2.1
 *            Added key repeat support contributed by:
 *                Vincent Vanackere <vanackere@lif.univ-mrs.fr>
 *            Added support for the "Lola" remote contributed by:
 *                Seth Cohn <sethcohn@yahoo.com>
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Hardware & software notes
 *
 * These remote controls are distributed by ATI as part of their
 * "All-In-Wonder" video card packages.  The receiver self-identifies as a
 * "USB Receiver" with manufacturer "X10 Wireless Technology Inc".
 *
 * The "Lola" remote is available from X10.  See:
 *    http://www.x10.com/products/lola_sg1.htm
 * The Lola is similar to the ATI remote but has no mouse support, and slightly
 * different keys.
 *
 * It is possible to use multiple receivers and remotes on multiple computers
 * simultaneously by configuring them to use specific channels.
 *
 * The RF protocol used by the remote supports 16 distinct channels, 1 to 16.
 * Actually, it may even support more, at least in some revisions of the
 * hardware.
 *
 * Each remote can be configured to transmit on one channel as follows:
 *   - Press and hold the "hand icon" button.
 *   - When the red LED starts to blink, let go of the "hand icon" button.
 *   - When it stops blinking, input the channel code as two digits, from 01
 *     to 16, and press the hand icon again.
 *
 * The timing can be a little tricky.  Try loading the module with debug=1
 * to have the kernel print out messages about the remote control number
 * and mask.  Note: debugging prints remote numbers as zero-based hexadecimal.
 *
 * The driver has a "channel_mask" parameter. This bitmask specifies which
 * channels will be ignored by the module.  To mask out channels, just add
 * all the 2^channel_number values together.
 *
 * For instance, set channel_mask = 2^4 = 16 (binary 10000) to make ati_remote
 * ignore signals coming from remote controls transmitting on channel 4, but
 * accept all other channels.
 *
 * Or, set channel_mask = 65533, (0xFFFD), and all channels except 1 will be
 * ignored.
 *
 * The default is 0 (respond to all channels). Bit 0 and bits 17-32 of this
 * parameter are unused.
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb/input.h>
#include <linux/wait.h>
#include <linux/jiffies.h>

/*
 * Module and Version Information, Module Parameters
 */

#define ATI_REMOTE_VENDOR_ID		0x0bc7
#define LOLA_REMOTE_PRODUCT_ID		0x0002
#define LOLA2_REMOTE_PRODUCT_ID		0x0003
#define ATI_REMOTE_PRODUCT_ID		0x0004
#define NVIDIA_REMOTE_PRODUCT_ID	0x0005
#define MEDION_REMOTE_PRODUCT_ID	0x0006

#define DRIVER_VERSION	        "2.2.1"
#define DRIVER_AUTHOR           "Torrey Hoffman <thoffman@arnor.net>"
#define DRIVER_DESC             "ATI/X10 RF USB Remote Control"

#define NAME_BUFSIZE      80    /* size of product name, path buffers */
#define DATA_BUFSIZE      63    /* size of URB data buffers */

/*
 * Duplicate event filtering time.
 * Sequential, identical KIND_FILTERED inputs with less than
 * FILTER_TIME milliseconds between them are considered as repeat
 * events. The hardware generates 5 events for the first keypress
 * and we have to take this into account for an accurate repeat
 * behaviour.
 */
#define FILTER_TIME	60 /* msec */
#define REPEAT_DELAY	500 /* msec */

static unsigned long channel_mask;
module_param(channel_mask, ulong, 0644);
MODULE_PARM_DESC(channel_mask, "Bitmask of remote control channels to ignore");

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable extra debug messages and information");

static int repeat_filter = FILTER_TIME;
module_param(repeat_filter, int, 0644);
MODULE_PARM_DESC(repeat_filter, "Repeat filter time, default = 60 msec");

static int repeat_delay = REPEAT_DELAY;
module_param(repeat_delay, int, 0644);
MODULE_PARM_DESC(repeat_delay, "Delay before sending repeats, default = 500 msec");

#define dbginfo(dev, format, arg...) do { if (debug) dev_info(dev , format , ## arg); } while (0)
#undef err
#define err(format, arg...) printk(KERN_ERR format , ## arg)

static struct usb_device_id ati_remote_table[] = {
	{ USB_DEVICE(ATI_REMOTE_VENDOR_ID, LOLA_REMOTE_PRODUCT_ID) },
	{ USB_DEVICE(ATI_REMOTE_VENDOR_ID, LOLA2_REMOTE_PRODUCT_ID) },
	{ USB_DEVICE(ATI_REMOTE_VENDOR_ID, ATI_REMOTE_PRODUCT_ID) },
	{ USB_DEVICE(ATI_REMOTE_VENDOR_ID, NVIDIA_REMOTE_PRODUCT_ID) },
	{ USB_DEVICE(ATI_REMOTE_VENDOR_ID, MEDION_REMOTE_PRODUCT_ID) },
	{}	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, ati_remote_table);

/* Get hi and low bytes of a 16-bits int */
#define HI(a)	((unsigned char)((a) >> 8))
#define LO(a)	((unsigned char)((a) & 0xff))

#define SEND_FLAG_IN_PROGRESS	1
#define SEND_FLAG_COMPLETE	2

/* Device initialization strings */
static char init1[] = { 0x01, 0x00, 0x20, 0x14 };
static char init2[] = { 0x01, 0x00, 0x20, 0x14, 0x20, 0x20, 0x20 };

struct ati_remote {
	struct input_dev *idev;
	struct usb_device *udev;
	struct usb_interface *interface;

	struct urb *irq_urb;
	struct urb *out_urb;
	struct usb_endpoint_descriptor *endpoint_in;
	struct usb_endpoint_descriptor *endpoint_out;
	unsigned char *inbuf;
	unsigned char *outbuf;
	dma_addr_t inbuf_dma;
	dma_addr_t outbuf_dma;

	unsigned char old_data[2];  /* Detect duplicate events */
	unsigned long old_jiffies;
	unsigned long acc_jiffies;  /* handle acceleration */
	unsigned long first_jiffies;

	unsigned int repeat_count;

	char name[NAME_BUFSIZE];
	char phys[NAME_BUFSIZE];

	wait_queue_head_t wait;
	int send_flags;
};

/* "Kinds" of messages sent from the hardware to the driver. */
#define KIND_END        0
#define KIND_LITERAL    1   /* Simply pass to input system */
#define KIND_FILTERED   2   /* Add artificial key-up events, drop keyrepeats */
#define KIND_LU         3   /* Directional keypad diagonals - left up, */
#define KIND_RU         4   /*   right up,  */
#define KIND_LD         5   /*   left down, */
#define KIND_RD         6   /*   right down */
#define KIND_ACCEL      7   /* Directional keypad - left, right, up, down.*/

/* Translation table from hardware messages to input events. */
static const struct {
	short kind;
	unsigned char data1, data2;
	int type;
	unsigned int code;
	int value;
}  ati_remote_tbl[] = {
	/* Directional control pad axes */
	{KIND_ACCEL,   0x35, 0x70, EV_REL, REL_X, -1},	 /* left */
	{KIND_ACCEL,   0x36, 0x71, EV_REL, REL_X, 1},    /* right */
	{KIND_ACCEL,   0x37, 0x72, EV_REL, REL_Y, -1},	 /* up */
	{KIND_ACCEL,   0x38, 0x73, EV_REL, REL_Y, 1},    /* down */
	/* Directional control pad diagonals */
	{KIND_LU,      0x39, 0x74, EV_REL, 0, 0},        /* left up */
	{KIND_RU,      0x3a, 0x75, EV_REL, 0, 0},        /* right up */
	{KIND_LD,      0x3c, 0x77, EV_REL, 0, 0},        /* left down */
	{KIND_RD,      0x3b, 0x76, EV_REL, 0, 0},        /* right down */

	/* "Mouse button" buttons */
	{KIND_LITERAL, 0x3d, 0x78, EV_KEY, BTN_LEFT, 1}, /* left btn down */
	{KIND_LITERAL, 0x3e, 0x79, EV_KEY, BTN_LEFT, 0}, /* left btn up */
	{KIND_LITERAL, 0x41, 0x7c, EV_KEY, BTN_RIGHT, 1},/* right btn down */
	{KIND_LITERAL, 0x42, 0x7d, EV_KEY, BTN_RIGHT, 0},/* right btn up */

	/* Artificial "doubleclick" events are generated by the hardware.
	 * They are mapped to the "side" and "extra" mouse buttons here. */
	{KIND_FILTERED, 0x3f, 0x7a, EV_KEY, BTN_SIDE, 1}, /* left dblclick */
	{KIND_FILTERED, 0x43, 0x7e, EV_KEY, BTN_EXTRA, 1},/* right dblclick */

	/* keyboard. */
	{KIND_FILTERED, 0xd2, 0x0d, EV_KEY, KEY_1, 1},
	{KIND_FILTERED, 0xd3, 0x0e, EV_KEY, KEY_2, 1},
	{KIND_FILTERED, 0xd4, 0x0f, EV_KEY, KEY_3, 1},
	{KIND_FILTERED, 0xd5, 0x10, EV_KEY, KEY_4, 1},
	{KIND_FILTERED, 0xd6, 0x11, EV_KEY, KEY_5, 1},
	{KIND_FILTERED, 0xd7, 0x12, EV_KEY, KEY_6, 1},
	{KIND_FILTERED, 0xd8, 0x13, EV_KEY, KEY_7, 1},
	{KIND_FILTERED, 0xd9, 0x14, EV_KEY, KEY_8, 1},
	{KIND_FILTERED, 0xda, 0x15, EV_KEY, KEY_9, 1},
	{KIND_FILTERED, 0xdc, 0x17, EV_KEY, KEY_0, 1},
	{KIND_FILTERED, 0xc5, 0x00, EV_KEY, KEY_A, 1},
	{KIND_FILTERED, 0xc6, 0x01, EV_KEY, KEY_B, 1},
	{KIND_FILTERED, 0xde, 0x19, EV_KEY, KEY_C, 1},
	{KIND_FILTERED, 0xe0, 0x1b, EV_KEY, KEY_D, 1},
	{KIND_FILTERED, 0xe6, 0x21, EV_KEY, KEY_E, 1},
	{KIND_FILTERED, 0xe8, 0x23, EV_KEY, KEY_F, 1},

	/* "special" keys */
	{KIND_FILTERED, 0xdd, 0x18, EV_KEY, KEY_KPENTER, 1},    /* "check" */
	{KIND_FILTERED, 0xdb, 0x16, EV_KEY, KEY_MENU, 1},       /* "menu" */
	{KIND_FILTERED, 0xc7, 0x02, EV_KEY, KEY_POWER, 1},      /* Power */
	{KIND_FILTERED, 0xc8, 0x03, EV_KEY, KEY_TV, 1},         /* TV */
	{KIND_FILTERED, 0xc9, 0x04, EV_KEY, KEY_DVD, 1},        /* DVD */
	{KIND_FILTERED, 0xca, 0x05, EV_KEY, KEY_WWW, 1},        /* WEB */
	{KIND_FILTERED, 0xcb, 0x06, EV_KEY, KEY_BOOKMARKS, 1},  /* "book" */
	{KIND_FILTERED, 0xcc, 0x07, EV_KEY, KEY_EDIT, 1},       /* "hand" */
	{KIND_FILTERED, 0xe1, 0x1c, EV_KEY, KEY_COFFEE, 1},     /* "timer" */
	{KIND_FILTERED, 0xe5, 0x20, EV_KEY, KEY_FRONT, 1},      /* "max" */
	{KIND_FILTERED, 0xe2, 0x1d, EV_KEY, KEY_LEFT, 1},       /* left */
	{KIND_FILTERED, 0xe4, 0x1f, EV_KEY, KEY_RIGHT, 1},      /* right */
	{KIND_FILTERED, 0xe7, 0x22, EV_KEY, KEY_DOWN, 1},       /* down */
	{KIND_FILTERED, 0xdf, 0x1a, EV_KEY, KEY_UP, 1},         /* up */
	{KIND_FILTERED, 0xe3, 0x1e, EV_KEY, KEY_OK, 1},         /* "OK" */
	{KIND_FILTERED, 0xce, 0x09, EV_KEY, KEY_VOLUMEDOWN, 1}, /* VOL + */
	{KIND_FILTERED, 0xcd, 0x08, EV_KEY, KEY_VOLUMEUP, 1},   /* VOL - */
	{KIND_FILTERED, 0xcf, 0x0a, EV_KEY, KEY_MUTE, 1},       /* MUTE  */
	{KIND_FILTERED, 0xd0, 0x0b, EV_KEY, KEY_CHANNELUP, 1},  /* CH + */
	{KIND_FILTERED, 0xd1, 0x0c, EV_KEY, KEY_CHANNELDOWN, 1},/* CH - */
	{KIND_FILTERED, 0xec, 0x27, EV_KEY, KEY_RECORD, 1},     /* ( o) red */
	{KIND_FILTERED, 0xea, 0x25, EV_KEY, KEY_PLAY, 1},       /* ( >) */
	{KIND_FILTERED, 0xe9, 0x24, EV_KEY, KEY_REWIND, 1},     /* (<<) */
	{KIND_FILTERED, 0xeb, 0x26, EV_KEY, KEY_FORWARD, 1},    /* (>>) */
	{KIND_FILTERED, 0xed, 0x28, EV_KEY, KEY_STOP, 1},       /* ([]) */
	{KIND_FILTERED, 0xee, 0x29, EV_KEY, KEY_PAUSE, 1},      /* ('') */
	{KIND_FILTERED, 0xf0, 0x2b, EV_KEY, KEY_PREVIOUS, 1},   /* (<-) */
	{KIND_FILTERED, 0xef, 0x2a, EV_KEY, KEY_NEXT, 1},       /* (>+) */
	{KIND_FILTERED, 0xf2, 0x2D, EV_KEY, KEY_INFO, 1},       /* PLAYING */
	{KIND_FILTERED, 0xf3, 0x2E, EV_KEY, KEY_HOME, 1},       /* TOP */
	{KIND_FILTERED, 0xf4, 0x2F, EV_KEY, KEY_END, 1},        /* END */
	{KIND_FILTERED, 0xf5, 0x30, EV_KEY, KEY_SELECT, 1},     /* SELECT */

	{KIND_END, 0x00, 0x00, EV_MAX + 1, 0, 0}
};

/* Local function prototypes */
static int ati_remote_open		(struct input_dev *inputdev);
static void ati_remote_close		(struct input_dev *inputdev);
static int ati_remote_sendpacket	(struct ati_remote *ati_remote, u16 cmd, unsigned char *data);
static void ati_remote_irq_out		(struct urb *urb);
static void ati_remote_irq_in		(struct urb *urb);
static void ati_remote_input_report	(struct urb *urb);
static int ati_remote_initialize	(struct ati_remote *ati_remote);
static int ati_remote_probe		(struct usb_interface *interface, const struct usb_device_id *id);
static void ati_remote_disconnect	(struct usb_interface *interface);

/* usb specific object to register with the usb subsystem */
static struct usb_driver ati_remote_driver = {
	.name         = "ati_remote",
	.probe        = ati_remote_probe,
	.disconnect   = ati_remote_disconnect,
	.id_table     = ati_remote_table,
};

/*
 *	ati_remote_dump_input
 */
static void ati_remote_dump(struct device *dev, unsigned char *data,
			    unsigned int len)
{
	if ((len == 1) && (data[0] != (unsigned char)0xff) && (data[0] != 0x00))
		dev_warn(dev, "Weird byte 0x%02x\n", data[0]);
	else if (len == 4)
		dev_warn(dev, "Weird key %02x %02x %02x %02x\n",
		     data[0], data[1], data[2], data[3]);
	else
		dev_warn(dev, "Weird data, len=%d %02x %02x %02x %02x %02x %02x ...\n",
		     len, data[0], data[1], data[2], data[3], data[4], data[5]);
}

/*
 *	ati_remote_open
 */
static int ati_remote_open(struct input_dev *inputdev)
{
	struct ati_remote *ati_remote = input_get_drvdata(inputdev);

	/* On first open, submit the read urb which was set up previously. */
	ati_remote->irq_urb->dev = ati_remote->udev;
	if (usb_submit_urb(ati_remote->irq_urb, GFP_KERNEL)) {
		dev_err(&ati_remote->interface->dev,
			"%s: usb_submit_urb failed!\n", __func__);
		return -EIO;
	}

	return 0;
}

/*
 *	ati_remote_close
 */
static void ati_remote_close(struct input_dev *inputdev)
{
	struct ati_remote *ati_remote = input_get_drvdata(inputdev);

	usb_kill_urb(ati_remote->irq_urb);
}

/*
 *		ati_remote_irq_out
 */
static void ati_remote_irq_out(struct urb *urb)
{
	struct ati_remote *ati_remote = urb->context;

	if (urb->status) {
		dev_dbg(&ati_remote->interface->dev, "%s: status %d\n",
			__func__, urb->status);
		return;
	}

	ati_remote->send_flags |= SEND_FLAG_COMPLETE;
	wmb();
	wake_up(&ati_remote->wait);
}

/*
 *	ati_remote_sendpacket
 *
 *	Used to send device initialization strings
 */
static int ati_remote_sendpacket(struct ati_remote *ati_remote, u16 cmd, unsigned char *data)
{
	int retval = 0;

	/* Set up out_urb */
	memcpy(ati_remote->out_urb->transfer_buffer + 1, data, LO(cmd));
	((char *) ati_remote->out_urb->transfer_buffer)[0] = HI(cmd);

	ati_remote->out_urb->transfer_buffer_length = LO(cmd) + 1;
	ati_remote->out_urb->dev = ati_remote->udev;
	ati_remote->send_flags = SEND_FLAG_IN_PROGRESS;

	retval = usb_submit_urb(ati_remote->out_urb, GFP_ATOMIC);
	if (retval) {
		dev_dbg(&ati_remote->interface->dev,
			 "sendpacket: usb_submit_urb failed: %d\n", retval);
		return retval;
	}

	wait_event_timeout(ati_remote->wait,
		((ati_remote->out_urb->status != -EINPROGRESS) ||
			(ati_remote->send_flags & SEND_FLAG_COMPLETE)),
		HZ);
	usb_kill_urb(ati_remote->out_urb);

	return retval;
}

/*
 *	ati_remote_event_lookup
 */
static int ati_remote_event_lookup(int rem, unsigned char d1, unsigned char d2)
{
	int i;

	for (i = 0; ati_remote_tbl[i].kind != KIND_END; i++) {
		/*
		 * Decide if the table entry matches the remote input.
		 */
		if ((((ati_remote_tbl[i].data1 & 0x0f) == (d1 & 0x0f))) &&
		    ((((ati_remote_tbl[i].data1 >> 4) -
		       (d1 >> 4) + rem) & 0x0f) == 0x0f) &&
		    (ati_remote_tbl[i].data2 == d2))
			return i;

	}
	return -1;
}

/*
 *	ati_remote_compute_accel
 *
 * Implements acceleration curve for directional control pad
 * If elapsed time since last event is > 1/4 second, user "stopped",
 * so reset acceleration. Otherwise, user is probably holding the control
 * pad down, so we increase acceleration, ramping up over two seconds to
 * a maximum speed.
 */
static int ati_remote_compute_accel(struct ati_remote *ati_remote)
{
	static const char accel[] = { 1, 2, 4, 6, 9, 13, 20 };
	unsigned long now = jiffies;
	int acc;

	if (time_after(now, ati_remote->old_jiffies + msecs_to_jiffies(250))) {
		acc = 1;
		ati_remote->acc_jiffies = now;
	}
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(125)))
		acc = accel[0];
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(250)))
		acc = accel[1];
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(500)))
		acc = accel[2];
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(1000)))
		acc = accel[3];
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(1500)))
		acc = accel[4];
	else if (time_before(now, ati_remote->acc_jiffies + msecs_to_jiffies(2000)))
		acc = accel[5];
	else
		acc = accel[6];

	return acc;
}

/*
 *	ati_remote_report_input
 */
static void ati_remote_input_report(struct urb *urb)
{
	struct ati_remote *ati_remote = urb->context;
	unsigned char *data= ati_remote->inbuf;
	struct input_dev *dev = ati_remote->idev;
	int index, acc;
	int remote_num;

	/* Deal with strange looking inputs */
	if ( (urb->actual_length != 4) || (data[0] != 0x14) ||
		((data[3] & 0x0f) != 0x00) ) {
		ati_remote_dump(&urb->dev->dev, data, urb->actual_length);
		return;
	}

	/* Mask unwanted remote channels.  */
	/* note: remote_num is 0-based, channel 1 on remote == 0 here */
	remote_num = (data[3] >> 4) & 0x0f;
        if (channel_mask & (1 << (remote_num + 1))) {
		dbginfo(&ati_remote->interface->dev,
			"Masked input from channel 0x%02x: data %02x,%02x, mask= 0x%02lx\n",
			remote_num, data[1], data[2], channel_mask);
		return;
	}

	/* Look up event code index in translation table */
	index = ati_remote_event_lookup(remote_num, data[1], data[2]);
	if (index < 0) {
		dev_warn(&ati_remote->interface->dev,
			 "Unknown input from channel 0x%02x: data %02x,%02x\n",
			 remote_num, data[1], data[2]);
		return;
	}
	dbginfo(&ati_remote->interface->dev,
		"channel 0x%02x; data %02x,%02x; index %d; keycode %d\n",
		remote_num, data[1], data[2], index, ati_remote_tbl[index].code);

	if (ati_remote_tbl[index].kind == KIND_LITERAL) {
		input_event(dev, ati_remote_tbl[index].type,
			ati_remote_tbl[index].code,
			ati_remote_tbl[index].value);
		input_sync(dev);

		ati_remote->old_jiffies = jiffies;
		return;
	}

	if (ati_remote_tbl[index].kind == KIND_FILTERED) {
		unsigned long now = jiffies;

		/* Filter duplicate events which happen "too close" together. */
		if (ati_remote->old_data[0] == data[1] &&
		    ati_remote->old_data[1] == data[2] &&
		    time_before(now, ati_remote->old_jiffies +
				     msecs_to_jiffies(repeat_filter))) {
			ati_remote->repeat_count++;
		} else {
			ati_remote->repeat_count = 0;
			ati_remote->first_jiffies = now;
		}

		ati_remote->old_data[0] = data[1];
		ati_remote->old_data[1] = data[2];
		ati_remote->old_jiffies = now;

		/* Ensure we skip at least the 4 first duplicate events (generated
		 * by a single keypress), and continue skipping until repeat_delay
		 * msecs have passed
		 */
		if (ati_remote->repeat_count > 0 &&
		    (ati_remote->repeat_count < 5 ||
		     time_before(now, ati_remote->first_jiffies +
				      msecs_to_jiffies(repeat_delay))))
			return;


		input_event(dev, ati_remote_tbl[index].type,
			ati_remote_tbl[index].code, 1);
		input_sync(dev);
		input_event(dev, ati_remote_tbl[index].type,
			ati_remote_tbl[index].code, 0);
		input_sync(dev);

	} else {

		/*
		 * Other event kinds are from the directional control pad, and have an
		 * acceleration factor applied to them.  Without this acceleration, the
		 * control pad is mostly unusable.
		 */
		acc = ati_remote_compute_accel(ati_remote);

		switch (ati_remote_tbl[index].kind) {
		case KIND_ACCEL:
			input_event(dev, ati_remote_tbl[index].type,
				ati_remote_tbl[index].code,
				ati_remote_tbl[index].value * acc);
			break;
		case KIND_LU:
			input_report_rel(dev, REL_X, -acc);
			input_report_rel(dev, REL_Y, -acc);
			break;
		case KIND_RU:
			input_report_rel(dev, REL_X, acc);
			input_report_rel(dev, REL_Y, -acc);
			break;
		case KIND_LD:
			input_report_rel(dev, REL_X, -acc);
			input_report_rel(dev, REL_Y, acc);
			break;
		case KIND_RD:
			input_report_rel(dev, REL_X, acc);
			input_report_rel(dev, REL_Y, acc);
			break;
		default:
			dev_dbg(&ati_remote->interface->dev, "ati_remote kind=%d\n",
				ati_remote_tbl[index].kind);
		}
		input_sync(dev);

		ati_remote->old_jiffies = jiffies;
		ati_remote->old_data[0] = data[1];
		ati_remote->old_data[1] = data[2];
	}
}

/*
 *	ati_remote_irq_in
 */
static void ati_remote_irq_in(struct urb *urb)
{
	struct ati_remote *ati_remote = urb->context;
	int retval;

	switch (urb->status) {
	case 0:			/* success */
		ati_remote_input_report(urb);
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		dev_dbg(&ati_remote->interface->dev, "%s: urb error status, unlink? \n",
			__func__);
		return;
	default:		/* error */
		dev_dbg(&ati_remote->interface->dev, "%s: Nonzero urb status %d\n",
			__func__, urb->status);
	}

	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		dev_err(&ati_remote->interface->dev, "%s: usb_submit_urb()=%d\n",
			__func__, retval);
}

/*
 *	ati_remote_alloc_buffers
 */
static int ati_remote_alloc_buffers(struct usb_device *udev,
				    struct ati_remote *ati_remote)
{
	ati_remote->inbuf = usb_alloc_coherent(udev, DATA_BUFSIZE, GFP_ATOMIC,
					       &ati_remote->inbuf_dma);
	if (!ati_remote->inbuf)
		return -1;

	ati_remote->outbuf = usb_alloc_coherent(udev, DATA_BUFSIZE, GFP_ATOMIC,
					        &ati_remote->outbuf_dma);
	if (!ati_remote->outbuf)
		return -1;

	ati_remote->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ati_remote->irq_urb)
		return -1;

	ati_remote->out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ati_remote->out_urb)
		return -1;

	return 0;
}

/*
 *	ati_remote_free_buffers
 */
static void ati_remote_free_buffers(struct ati_remote *ati_remote)
{
	usb_free_urb(ati_remote->irq_urb);
	usb_free_urb(ati_remote->out_urb);

	usb_free_coherent(ati_remote->udev, DATA_BUFSIZE,
		ati_remote->inbuf, ati_remote->inbuf_dma);

	usb_free_coherent(ati_remote->udev, DATA_BUFSIZE,
		ati_remote->outbuf, ati_remote->outbuf_dma);
}

static void ati_remote_input_init(struct ati_remote *ati_remote)
{
	struct input_dev *idev = ati_remote->idev;
	int i;

	idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	idev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
		BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_SIDE) | BIT_MASK(BTN_EXTRA);
	idev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	for (i = 0; ati_remote_tbl[i].kind != KIND_END; i++)
		if (ati_remote_tbl[i].type == EV_KEY)
			set_bit(ati_remote_tbl[i].code, idev->keybit);

	input_set_drvdata(idev, ati_remote);

	idev->open = ati_remote_open;
	idev->close = ati_remote_close;

	idev->name = ati_remote->name;
	idev->phys = ati_remote->phys;

	usb_to_input_id(ati_remote->udev, &idev->id);
	idev->dev.parent = &ati_remote->udev->dev;
}

static int ati_remote_initialize(struct ati_remote *ati_remote)
{
	struct usb_device *udev = ati_remote->udev;
	int pipe, maxp;

	init_waitqueue_head(&ati_remote->wait);

	/* Set up irq_urb */
	pipe = usb_rcvintpipe(udev, ati_remote->endpoint_in->bEndpointAddress);
	maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));
	maxp = (maxp > DATA_BUFSIZE) ? DATA_BUFSIZE : maxp;

	usb_fill_int_urb(ati_remote->irq_urb, udev, pipe, ati_remote->inbuf,
			 maxp, ati_remote_irq_in, ati_remote,
			 ati_remote->endpoint_in->bInterval);
	ati_remote->irq_urb->transfer_dma = ati_remote->inbuf_dma;
	ati_remote->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* Set up out_urb */
	pipe = usb_sndintpipe(udev, ati_remote->endpoint_out->bEndpointAddress);
	maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));
	maxp = (maxp > DATA_BUFSIZE) ? DATA_BUFSIZE : maxp;

	usb_fill_int_urb(ati_remote->out_urb, udev, pipe, ati_remote->outbuf,
			 maxp, ati_remote_irq_out, ati_remote,
			 ati_remote->endpoint_out->bInterval);
	ati_remote->out_urb->transfer_dma = ati_remote->outbuf_dma;
	ati_remote->out_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* send initialization strings */
	if ((ati_remote_sendpacket(ati_remote, 0x8004, init1)) ||
	    (ati_remote_sendpacket(ati_remote, 0x8007, init2))) {
		dev_err(&ati_remote->interface->dev,
			 "Initializing ati_remote hardware failed.\n");
		return -EIO;
	}

	return 0;
}

/*
 *	ati_remote_probe
 */
static int ati_remote_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_host_interface *iface_host = interface->cur_altsetting;
	struct usb_endpoint_descriptor *endpoint_in, *endpoint_out;
	struct ati_remote *ati_remote;
	struct input_dev *input_dev;
	int err = -ENOMEM;

	if (iface_host->desc.bNumEndpoints != 2) {
		err("%s: Unexpected desc.bNumEndpoints\n", __func__);
		return -ENODEV;
	}

	endpoint_in = &iface_host->endpoint[0].desc;
	endpoint_out = &iface_host->endpoint[1].desc;

	if (!usb_endpoint_is_int_in(endpoint_in)) {
		err("%s: Unexpected endpoint_in\n", __func__);
		return -ENODEV;
	}
	if (le16_to_cpu(endpoint_in->wMaxPacketSize) == 0) {
		err("%s: endpoint_in message size==0? \n", __func__);
		return -ENODEV;
	}

	ati_remote = kzalloc(sizeof (struct ati_remote), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ati_remote || !input_dev)
		goto fail1;

	/* Allocate URB buffers, URBs */
	if (ati_remote_alloc_buffers(udev, ati_remote))
		goto fail2;

	ati_remote->endpoint_in = endpoint_in;
	ati_remote->endpoint_out = endpoint_out;
	ati_remote->udev = udev;
	ati_remote->idev = input_dev;
	ati_remote->interface = interface;

	usb_make_path(udev, ati_remote->phys, sizeof(ati_remote->phys));
	strlcat(ati_remote->phys, "/input0", sizeof(ati_remote->phys));

	if (udev->manufacturer)
		strlcpy(ati_remote->name, udev->manufacturer, sizeof(ati_remote->name));

	if (udev->product)
		snprintf(ati_remote->name, sizeof(ati_remote->name),
			 "%s %s", ati_remote->name, udev->product);

	if (!strlen(ati_remote->name))
		snprintf(ati_remote->name, sizeof(ati_remote->name),
			DRIVER_DESC "(%04x,%04x)",
			le16_to_cpu(ati_remote->udev->descriptor.idVendor),
			le16_to_cpu(ati_remote->udev->descriptor.idProduct));

	ati_remote_input_init(ati_remote);

	/* Device Hardware Initialization - fills in ati_remote->idev from udev. */
	err = ati_remote_initialize(ati_remote);
	if (err)
		goto fail3;

	/* Set up and register input device */
	err = input_register_device(ati_remote->idev);
	if (err)
		goto fail3;

	usb_set_intfdata(interface, ati_remote);
	return 0;

 fail3:	usb_kill_urb(ati_remote->irq_urb);
	usb_kill_urb(ati_remote->out_urb);
 fail2:	ati_remote_free_buffers(ati_remote);
 fail1:	input_free_device(input_dev);
	kfree(ati_remote);
	return err;
}

/*
 *	ati_remote_disconnect
 */
static void ati_remote_disconnect(struct usb_interface *interface)
{
	struct ati_remote *ati_remote;

	ati_remote = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);
	if (!ati_remote) {
		dev_warn(&interface->dev, "%s - null device?\n", __func__);
		return;
	}

	usb_kill_urb(ati_remote->irq_urb);
	usb_kill_urb(ati_remote->out_urb);
	input_unregister_device(ati_remote->idev);
	ati_remote_free_buffers(ati_remote);
	kfree(ati_remote);
}

/*
 *	ati_remote_init
 */
static int __init ati_remote_init(void)
{
	int result;

	result = usb_register(&ati_remote_driver);
	if (result)
		printk(KERN_ERR KBUILD_MODNAME
		       ": usb_register error #%d\n", result);
	else
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
		       DRIVER_DESC "\n");

	return result;
}

/*
 *	ati_remote_exit
 */
static void __exit ati_remote_exit(void)
{
	usb_deregister(&ati_remote_driver);
}

/*
 *	module specification
 */

module_init(ati_remote_init);
module_exit(ati_remote_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
