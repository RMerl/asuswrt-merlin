/*****************************************************************************
 *
 * Filename:      irda-usb.c
 * Version:       0.10
 * Description:   IrDA-USB Driver
 * Status:        Experimental 
 * Author:        Dag Brattli <dag@brattli.net>
 *
 *	Copyright (C) 2000, Roman Weissgaerber <weissg@vienna.at>
 *      Copyright (C) 2001, Dag Brattli <dag@brattli.net>
 *      Copyright (C) 2001, Jean Tourrilhes <jt@hpl.hp.com>
 *      Copyright (C) 2004, SigmaTel, Inc. <irquality@sigmatel.com>
 *      Copyright (C) 2005, Milan Beno <beno@pobox.sk>
 *      Copyright (C) 2006, Nick Fedchik <nick@fedchik.org.ua>
 *          
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

/*
 *			    IMPORTANT NOTE
 *			    --------------
 *
 * As of kernel 2.5.20, this is the state of compliance and testing of
 * this driver (irda-usb) with regards to the USB low level drivers...
 *
 * This driver has been tested SUCCESSFULLY with the following drivers :
 *	o usb-uhci-hcd	(For Intel/Via USB controllers)
 *	o uhci-hcd	(Alternate/JE driver for Intel/Via USB controllers)
 *	o ohci-hcd	(For other USB controllers)
 *
 * This driver has NOT been tested with the following drivers :
 *	o ehci-hcd	(USB 2.0 controllers)
 *
 * Note that all HCD drivers do URB_ZERO_PACKET and timeout properly,
 * so we don't have to worry about that anymore.
 * One common problem is the failure to set the address on the dongle,
 * but this happens before the driver gets loaded...
 *
 * Jean II
 */

/*------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/rtnetlink.h>
#include <linux/usb.h>
#include <linux/firmware.h>

#include "irda-usb.h"

/*------------------------------------------------------------------*/

static int qos_mtt_bits = 0;

/* These are the currently known IrDA USB dongles. Add new dongles here */
static struct usb_device_id dongles[] = {
	/* ACTiSYS Corp.,  ACT-IR2000U FIR-USB Adapter */
	{ USB_DEVICE(0x9c4, 0x011), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	/* Look like ACTiSYS, Report : IBM Corp., IBM UltraPort IrDA */
	{ USB_DEVICE(0x4428, 0x012), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	/* KC Technology Inc.,  KC-180 USB IrDA Device */
	{ USB_DEVICE(0x50f, 0x180), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	/* Extended Systems, Inc.,  XTNDAccess IrDA USB (ESI-9685) */
	{ USB_DEVICE(0x8e9, 0x100), .driver_info = IUC_SPEED_BUG | IUC_NO_WINDOW },
	/* SigmaTel STIR4210/4220/4116 USB IrDA (VFIR) Bridge */
	{ USB_DEVICE(0x66f, 0x4210), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ USB_DEVICE(0x66f, 0x4220), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ USB_DEVICE(0x66f, 0x4116), .driver_info = IUC_STIR421X | IUC_SPEED_BUG },
	{ .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
	  USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	  .bInterfaceClass = USB_CLASS_APP_SPEC,
	  .bInterfaceSubClass = USB_CLASS_IRDA,
	  .driver_info = IUC_DEFAULT, },
	{ }, /* The end */
};

/*
 * Important note :
 * Devices based on the SigmaTel chipset (0x66f, 0x4200) are not designed
 * using the "USB-IrDA specification" (yes, there exist such a thing), and
 * therefore not supported by this driver (don't add them above).
 * There is a Linux driver, stir4200, that support those USB devices.
 * Jean II
 */

MODULE_DEVICE_TABLE(usb, dongles);

/*------------------------------------------------------------------*/

static void irda_usb_init_qos(struct irda_usb_cb *self) ;
static struct irda_class_desc *irda_usb_find_class_desc(struct usb_interface *intf);
static void irda_usb_disconnect(struct usb_interface *intf);
static void irda_usb_change_speed_xbofs(struct irda_usb_cb *self);
static netdev_tx_t irda_usb_hard_xmit(struct sk_buff *skb,
					    struct net_device *dev);
static int irda_usb_open(struct irda_usb_cb *self);
static void irda_usb_close(struct irda_usb_cb *self);
static void speed_bulk_callback(struct urb *urb);
static void write_bulk_callback(struct urb *urb);
static void irda_usb_receive(struct urb *urb);
static void irda_usb_rx_defer_expired(unsigned long data);
static int irda_usb_net_open(struct net_device *dev);
static int irda_usb_net_close(struct net_device *dev);
static int irda_usb_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void irda_usb_net_timeout(struct net_device *dev);

/************************ TRANSMIT ROUTINES ************************/
/*
 * Receive packets from the IrDA stack and send them on the USB pipe.
 * Handle speed change, timeout and lot's of ugliness...
 */

/*------------------------------------------------------------------*/
/*
 * Function irda_usb_build_header(self, skb, header)
 *
 *   Builds USB-IrDA outbound header
 *
 * When we send an IrDA frame over an USB pipe, we add to it a 1 byte
 * header. This function create this header with the proper values.
 *
 * Important note : the USB-IrDA spec 1.0 say very clearly in chapter 5.4.2.2
 * that the setting of the link speed and xbof number in this outbound header
 * should be applied *AFTER* the frame has been sent.
 * Unfortunately, some devices are not compliant with that... It seems that
 * reading the spec is far too difficult...
 * Jean II
 */
static void irda_usb_build_header(struct irda_usb_cb *self,
				  __u8 *header,
				  int	force)
{
	/* Here we check if we have an STIR421x chip,
	 * and if either speed or xbofs (or both) needs
	 * to be changed.
	 */
	if (self->capability & IUC_STIR421X &&
	    ((self->new_speed != -1) || (self->new_xbofs != -1))) {

		/* With STIR421x, speed and xBOFs must be set at the same
		 * time, even if only one of them changes.
		 */
		if (self->new_speed == -1)
			self->new_speed = self->speed ;

		if (self->new_xbofs == -1)
			self->new_xbofs = self->xbofs ;
	}

	/* Set the link speed */
	if (self->new_speed != -1) {
		/* Hum... Ugly hack :-(
		 * Some device are not compliant with the spec and change
		 * parameters *before* sending the frame. - Jean II
		 */
		if ((self->capability & IUC_SPEED_BUG) &&
		    (!force) && (self->speed != -1)) {
			/* No speed and xbofs change here
			 * (we'll do it later in the write callback) */
			IRDA_DEBUG(2, "%s(), not changing speed yet\n", __func__);
			*header = 0;
			return;
		}

		IRDA_DEBUG(2, "%s(), changing speed to %d\n", __func__, self->new_speed);
		self->speed = self->new_speed;
		/* We will do ` self->new_speed = -1; ' in the completion
		 * handler just in case the current URB fail - Jean II */

		switch (self->speed) {
		case 2400:
		        *header = SPEED_2400;
			break;
		default:
		case 9600:
			*header = SPEED_9600;
			break;
		case 19200:
			*header = SPEED_19200;
			break;
		case 38400:
			*header = SPEED_38400;
			break;
		case 57600:
		        *header = SPEED_57600;
			break;
		case 115200:
		        *header = SPEED_115200;
			break;
		case 576000:
		        *header = SPEED_576000;
			break;
		case 1152000:
		        *header = SPEED_1152000;
			break;
		case 4000000:
		        *header = SPEED_4000000;
			self->new_xbofs = 0;
			break;
		case 16000000:
			*header = SPEED_16000000;
  			self->new_xbofs = 0;
  			break;
  		}
	} else
		/* No change */
		*header = 0;
	
	/* Set the negotiated additional XBOFS */
	if (self->new_xbofs != -1) {
		IRDA_DEBUG(2, "%s(), changing xbofs to %d\n", __func__, self->new_xbofs);
		self->xbofs = self->new_xbofs;
		/* We will do ` self->new_xbofs = -1; ' in the completion
		 * handler just in case the current URB fail - Jean II */

		switch (self->xbofs) {
		case 48:
			*header |= 0x10;
			break;
		case 28:
		case 24:	/* USB spec 1.0 says 24 */
			*header |= 0x20;
			break;
		default:
		case 12:
			*header |= 0x30;
			break;
		case 5: /* Bug in IrLAP spec? (should be 6) */
		case 6:
			*header |= 0x40;
			break;
		case 3:
			*header |= 0x50;
			break;
		case 2:
			*header |= 0x60;
			break;
		case 1:
			*header |= 0x70;
			break;
		case 0:
			*header |= 0x80;
			break;
		}
	}
}

/*
*   calculate turnaround time for SigmaTel header
*/
static __u8 get_turnaround_time(struct sk_buff *skb)
{
	int turnaround_time = irda_get_mtt(skb);

	if ( turnaround_time == 0 )
		return 0;
	else if ( turnaround_time <= 10 )
		return 1;
	else if ( turnaround_time <= 50 )
		return 2;
	else if ( turnaround_time <= 100 )
		return 3;
	else if ( turnaround_time <= 500 )
		return 4;
	else if ( turnaround_time <= 1000 )
		return 5;
	else if ( turnaround_time <= 5000 )
		return 6;
	else
		return 7;
}


/*------------------------------------------------------------------*/
/*
 * Send a command to change the speed of the dongle
 * Need to be called with spinlock on.
 */
static void irda_usb_change_speed_xbofs(struct irda_usb_cb *self)
{
	__u8 *frame;
	struct urb *urb;
	int ret;

	IRDA_DEBUG(2, "%s(), speed=%d, xbofs=%d\n", __func__,
		   self->new_speed, self->new_xbofs);

	/* Grab the speed URB */
	urb = self->speed_urb;
	if (urb->status != 0) {
		IRDA_WARNING("%s(), URB still in use!\n", __func__);
		return;
	}

	/* Allocate the fake frame */
	frame = self->speed_buff;

	/* Set the new speed and xbofs in this fake frame */
	irda_usb_build_header(self, frame, 1);

	if (self->capability & IUC_STIR421X) {
		if (frame[0] == 0) return ; // do nothing if no change
		frame[1] = 0; // other parameters don't change here
		frame[2] = 0;
	}

	/* Submit the 0 length IrDA frame to trigger new speed settings */
        usb_fill_bulk_urb(urb, self->usbdev,
		      usb_sndbulkpipe(self->usbdev, self->bulk_out_ep),
                      frame, IRDA_USB_SPEED_MTU,
                      speed_bulk_callback, self);
	urb->transfer_buffer_length = self->header_length;
	urb->transfer_flags = 0;

	/* Irq disabled -> GFP_ATOMIC */
	if ((ret = usb_submit_urb(urb, GFP_ATOMIC))) {
		IRDA_WARNING("%s(), failed Speed URB\n", __func__);
	}
}

/*------------------------------------------------------------------*/
/*
 * Speed URB callback
 * Now, we can only get called for the speed URB.
 */
static void speed_bulk_callback(struct urb *urb)
{
	struct irda_usb_cb *self = urb->context;
	
	IRDA_DEBUG(2, "%s()\n", __func__);

	/* We should always have a context */
	IRDA_ASSERT(self != NULL, return;);
	/* We should always be called for the speed URB */
	IRDA_ASSERT(urb == self->speed_urb, return;);

	/* Check for timeout and other USB nasties */
	if (urb->status != 0) {
		/* I get a lot of -ECONNABORTED = -103 here - Jean II */
		IRDA_DEBUG(0, "%s(), URB complete status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);

		/* Don't do anything here, that might confuse the USB layer.
		 * Instead, we will wait for irda_usb_net_timeout(), the
		 * network layer watchdog, to fix the situation.
		 * Jean II */
		/* A reset of the dongle might be welcomed here - Jean II */
		return;
	}

	/* urb is now available */
	//urb->status = 0; -> tested above

	/* New speed and xbof is now committed in hardware */
	self->new_speed = -1;
	self->new_xbofs = -1;

	/* Allow the stack to send more packets */
	netif_wake_queue(self->netdev);
}

/*------------------------------------------------------------------*/
/*
 * Send an IrDA frame to the USB dongle (for transmission)
 */
static netdev_tx_t irda_usb_hard_xmit(struct sk_buff *skb,
					    struct net_device *netdev)
{
	struct irda_usb_cb *self = netdev_priv(netdev);
	struct urb *urb = self->tx_urb;
	unsigned long flags;
	s32 speed;
	s16 xbofs;
	int res, mtt;

	IRDA_DEBUG(4, "%s() on %s\n", __func__, netdev->name);

	netif_stop_queue(netdev);

	/* Protect us from USB callbacks, net watchdog and else. */
	spin_lock_irqsave(&self->lock, flags);

	/* Check if the device is still there.
	 * We need to check self->present under the spinlock because
	 * of irda_usb_disconnect() is synchronous - Jean II */
	if (!self->present) {
		IRDA_DEBUG(0, "%s(), Device is gone...\n", __func__);
		goto drop;
	}

	/* Check if we need to change the number of xbofs */
        xbofs = irda_get_next_xbofs(skb);
        if ((xbofs != self->xbofs) && (xbofs != -1)) {
		self->new_xbofs = xbofs;
	}

        /* Check if we need to change the speed */
	speed = irda_get_next_speed(skb);
	if ((speed != self->speed) && (speed != -1)) {
		/* Set the desired speed */
		self->new_speed = speed;

		/* Check for empty frame */
		if (!skb->len) {
			/* IrLAP send us an empty frame to make us change the
			 * speed. Changing speed with the USB adapter is in
			 * fact sending an empty frame to the adapter, so we
			 * could just let the present function do its job.
			 * However, we would wait for min turn time,
			 * do an extra memcpy and increment packet counters...
			 * Jean II */
			irda_usb_change_speed_xbofs(self);
			netdev->trans_start = jiffies;
			/* Will netif_wake_queue() in callback */
			goto drop;
		}
	}

	if (urb->status != 0) {
		IRDA_WARNING("%s(), URB still in use!\n", __func__);
		goto drop;
	}

	skb_copy_from_linear_data(skb, self->tx_buff + self->header_length, skb->len);

	/* Change setting for next frame */
	if (self->capability & IUC_STIR421X) {
		__u8 turnaround_time;
		__u8* frame = self->tx_buff;
		turnaround_time = get_turnaround_time( skb );
		irda_usb_build_header(self, frame, 0);
		frame[2] = turnaround_time;
		if ((skb->len != 0) &&
		    ((skb->len % 128) == 0) &&
		    ((skb->len % 512) != 0)) {
			/* add extra byte for special SigmaTel feature */
			frame[1] = 1;
			skb_put(skb, 1);
		} else {
			frame[1] = 0;
		}
	} else {
		irda_usb_build_header(self, self->tx_buff, 0);
	}

	/* FIXME: Make macro out of this one */
	((struct irda_skb_cb *)skb->cb)->context = self;

	usb_fill_bulk_urb(urb, self->usbdev,
		      usb_sndbulkpipe(self->usbdev, self->bulk_out_ep),
                      self->tx_buff, skb->len + self->header_length,
                      write_bulk_callback, skb);

	/* This flag (URB_ZERO_PACKET) indicates that what we send is not
	 * a continuous stream of data but separate packets.
	 * In this case, the USB layer will insert an empty USB frame (TD)
	 * after each of our packets that is exact multiple of the frame size.
	 * This is how the dongle will detect the end of packet - Jean II */
	urb->transfer_flags = URB_ZERO_PACKET;

	/* Generate min turn time. FIXME: can we do better than this? */
	/* Trying to a turnaround time at this level is trying to measure
	 * processor clock cycle with a wrist-watch, approximate at best...
	 *
	 * What we know is the last time we received a frame over USB.
	 * Due to latency over USB that depend on the USB load, we don't
	 * know when this frame was received over IrDA (a few ms before ?)
	 * Then, same story for our outgoing frame...
	 *
	 * In theory, the USB dongle is supposed to handle the turnaround
	 * by itself (spec 1.0, chater 4, page 6). Who knows ??? That's
	 * why this code is enabled only for dongles that doesn't meet
	 * the spec.
	 * Jean II */
	if (self->capability & IUC_NO_TURN) {
		mtt = irda_get_mtt(skb);
		if (mtt) {
			int diff;
			do_gettimeofday(&self->now);
			diff = self->now.tv_usec - self->stamp.tv_usec;
#ifdef IU_USB_MIN_RTT
			/* Factor in USB delays -> Get rid of udelay() that
			 * would be lost in the noise - Jean II */
			diff += IU_USB_MIN_RTT;
#endif /* IU_USB_MIN_RTT */
			/* If the usec counter did wraparound, the diff will
			 * go negative (tv_usec is a long), so we need to
			 * correct it by one second. Jean II */
			if (diff < 0)
				diff += 1000000;

		        /* Check if the mtt is larger than the time we have
			 * already used by all the protocol processing
			 */
			if (mtt > diff) {
				mtt -= diff;
				if (mtt > 1000)
					mdelay(mtt/1000);
				else
					udelay(mtt);
			}
		}
	}
	
	/* Ask USB to send the packet - Irq disabled -> GFP_ATOMIC */
	if ((res = usb_submit_urb(urb, GFP_ATOMIC))) {
		IRDA_WARNING("%s(), failed Tx URB\n", __func__);
		netdev->stats.tx_errors++;
		/* Let USB recover : We will catch that in the watchdog */
		/*netif_start_queue(netdev);*/
	} else {
		/* Increment packet stats */
		netdev->stats.tx_packets++;
                netdev->stats.tx_bytes += skb->len;
		
		netdev->trans_start = jiffies;
	}
	spin_unlock_irqrestore(&self->lock, flags);
	
	return NETDEV_TX_OK;

drop:
	/* Drop silently the skb and exit */
	dev_kfree_skb(skb);
	spin_unlock_irqrestore(&self->lock, flags);
	return NETDEV_TX_OK;
}

/*------------------------------------------------------------------*/
/*
 * Note : this function will be called only for tx_urb...
 */
static void write_bulk_callback(struct urb *urb)
{
	unsigned long flags;
	struct sk_buff *skb = urb->context;
	struct irda_usb_cb *self = ((struct irda_skb_cb *) skb->cb)->context;
	
	IRDA_DEBUG(2, "%s()\n", __func__);

	/* We should always have a context */
	IRDA_ASSERT(self != NULL, return;);
	/* We should always be called for the speed URB */
	IRDA_ASSERT(urb == self->tx_urb, return;);

	/* Free up the skb */
	dev_kfree_skb_any(skb);
	urb->context = NULL;

	/* Check for timeout and other USB nasties */
	if (urb->status != 0) {
		/* I get a lot of -ECONNABORTED = -103 here - Jean II */
		IRDA_DEBUG(0, "%s(), URB complete status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);

		/* Don't do anything here, that might confuse the USB layer,
		 * and we could go in recursion and blow the kernel stack...
		 * Instead, we will wait for irda_usb_net_timeout(), the
		 * network layer watchdog, to fix the situation.
		 * Jean II */
		/* A reset of the dongle might be welcomed here - Jean II */
		return;
	}

	/* urb is now available */
	//urb->status = 0; -> tested above

	/* Make sure we read self->present properly */
	spin_lock_irqsave(&self->lock, flags);

	/* If the network is closed, stop everything */
	if ((!self->netopen) || (!self->present)) {
		IRDA_DEBUG(0, "%s(), Network is gone...\n", __func__);
		spin_unlock_irqrestore(&self->lock, flags);
		return;
	}

	/* If changes to speed or xbofs is pending... */
	if ((self->new_speed != -1) || (self->new_xbofs != -1)) {
		if ((self->new_speed != self->speed) ||
		    (self->new_xbofs != self->xbofs)) {
			/* We haven't changed speed yet (because of
			 * IUC_SPEED_BUG), so do it now - Jean II */
			IRDA_DEBUG(1, "%s(), Changing speed now...\n", __func__);
			irda_usb_change_speed_xbofs(self);
		} else {
			/* New speed and xbof is now committed in hardware */
			self->new_speed = -1;
			self->new_xbofs = -1;
			/* Done, waiting for next packet */
			netif_wake_queue(self->netdev);
		}
	} else {
		/* Otherwise, allow the stack to send more packets */
		netif_wake_queue(self->netdev);
	}
	spin_unlock_irqrestore(&self->lock, flags);
}

/*------------------------------------------------------------------*/
/*
 * Watchdog timer from the network layer.
 * After a predetermined timeout, if we don't give confirmation that
 * the packet has been sent (i.e. no call to netif_wake_queue()),
 * the network layer will call this function.
 * Note that URB that we submit have also a timeout. When the URB timeout
 * expire, the normal URB callback is called (write_bulk_callback()).
 */
static void irda_usb_net_timeout(struct net_device *netdev)
{
	unsigned long flags;
	struct irda_usb_cb *self = netdev_priv(netdev);
	struct urb *urb;
	int	done = 0;	/* If we have made any progress */

	IRDA_DEBUG(0, "%s(), Network layer thinks we timed out!\n", __func__);
	IRDA_ASSERT(self != NULL, return;);

	/* Protect us from USB callbacks, net Tx and else. */
	spin_lock_irqsave(&self->lock, flags);

	/* self->present *MUST* be read under spinlock */
	if (!self->present) {
		IRDA_WARNING("%s(), device not present!\n", __func__);
		netif_stop_queue(netdev);
		spin_unlock_irqrestore(&self->lock, flags);
		return;
	}

	/* Check speed URB */
	urb = self->speed_urb;
	if (urb->status != 0) {
		IRDA_DEBUG(0, "%s: Speed change timed out, urb->status=%d, urb->transfer_flags=0x%04X\n", netdev->name, urb->status, urb->transfer_flags);

		switch (urb->status) {
		case -EINPROGRESS:
			usb_unlink_urb(urb);
			/* Note : above will  *NOT* call netif_wake_queue()
			 * in completion handler, we will come back here.
			 * Jean II */
			done = 1;
			break;
		case -ECONNRESET:
		case -ENOENT:			/* urb unlinked by us */
		default:			/* ??? - Play safe */
			urb->status = 0;
			netif_wake_queue(self->netdev);
			done = 1;
			break;
		}
	}

	/* Check Tx URB */
	urb = self->tx_urb;
	if (urb->status != 0) {
		struct sk_buff *skb = urb->context;

		IRDA_DEBUG(0, "%s: Tx timed out, urb->status=%d, urb->transfer_flags=0x%04X\n", netdev->name, urb->status, urb->transfer_flags);

		/* Increase error count */
		netdev->stats.tx_errors++;

#ifdef IU_BUG_KICK_TIMEOUT
		/* Can't be a bad idea to reset the speed ;-) - Jean II */
		if(self->new_speed == -1)
			self->new_speed = self->speed;
		if(self->new_xbofs == -1)
			self->new_xbofs = self->xbofs;
		irda_usb_change_speed_xbofs(self);
#endif /* IU_BUG_KICK_TIMEOUT */

		switch (urb->status) {
		case -EINPROGRESS:
			usb_unlink_urb(urb);
			/* Note : above will  *NOT* call netif_wake_queue()
			 * in completion handler, because urb->status will
			 * be -ENOENT. We will fix that at the next watchdog,
			 * leaving more time to USB to recover...
			 * Jean II */
			done = 1;
			break;
		case -ECONNRESET:
		case -ENOENT:			/* urb unlinked by us */
		default:			/* ??? - Play safe */
			if(skb != NULL) {
				dev_kfree_skb_any(skb);
				urb->context = NULL;
			}
			urb->status = 0;
			netif_wake_queue(self->netdev);
			done = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&self->lock, flags);

	/* Maybe we need a reset */
	/* Note : Some drivers seem to use a usb_set_interface() when they
	 * need to reset the hardware. Hum...
	 */

	/* if(done == 0) */
}

/************************* RECEIVE ROUTINES *************************/
/*
 * Receive packets from the USB layer stack and pass them to the IrDA stack.
 * Try to work around USB failures...
 */

/*
 * Note :
 * Some of you may have noticed that most dongle have an interrupt in pipe
 * that we don't use. Here is the little secret...
 * When we hang a Rx URB on the bulk in pipe, it generates some USB traffic
 * in every USB frame. This is unnecessary overhead.
 * The interrupt in pipe will generate an event every time a packet is
 * received. Reading an interrupt pipe adds minimal overhead, but has some
 * latency (~1ms).
 * If we are connected (speed != 9600), we want to minimise latency, so
 * we just always hang the Rx URB and ignore the interrupt.
 * If we are not connected (speed == 9600), there is usually no Rx traffic,
 * and we want to minimise the USB overhead. In this case we should wait
 * on the interrupt pipe and hang the Rx URB only when an interrupt is
 * received.
 * Jean II
 *
 * Note : don't read the above as what we are currently doing, but as
 * something we could do with KC dongle. Also don't forget that the
 * interrupt pipe is not part of the original standard, so this would
 * need to be optional...
 * Jean II
 */

/*------------------------------------------------------------------*/
/*
 * Submit a Rx URB to the USB layer to handle reception of a frame
 * Mostly called by the completion callback of the previous URB.
 *
 * Jean II
 */
static void irda_usb_submit(struct irda_usb_cb *self, struct sk_buff *skb, struct urb *urb)
{
	struct irda_skb_cb *cb;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __func__);

	/* This should never happen */
	IRDA_ASSERT(skb != NULL, return;);
	IRDA_ASSERT(urb != NULL, return;);

	/* Save ourselves in the skb */
	cb = (struct irda_skb_cb *) skb->cb;
	cb->context = self;

	/* Reinitialize URB */
	usb_fill_bulk_urb(urb, self->usbdev, 
		      usb_rcvbulkpipe(self->usbdev, self->bulk_in_ep), 
		      skb->data, IRDA_SKB_MAX_MTU,
                      irda_usb_receive, skb);
	urb->status = 0;

	/* Can be called from irda_usb_receive (irq handler) -> GFP_ATOMIC */
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		/* If this ever happen, we are in deep s***.
		 * Basically, the Rx path will stop... */
		IRDA_WARNING("%s(), Failed to submit Rx URB %d\n",
			     __func__, ret);
	}
}

/*------------------------------------------------------------------*/
/*
 * Function irda_usb_receive(urb)
 *
 *     Called by the USB subsystem when a frame has been received
 *
 */
static void irda_usb_receive(struct urb *urb)
{
	struct sk_buff *skb = (struct sk_buff *) urb->context;
	struct irda_usb_cb *self; 
	struct irda_skb_cb *cb;
	struct sk_buff *newskb;
	struct sk_buff *dataskb;
	struct urb *next_urb;
	unsigned int len, docopy;

	IRDA_DEBUG(2, "%s(), len=%d\n", __func__, urb->actual_length);
	
	/* Find ourselves */
	cb = (struct irda_skb_cb *) skb->cb;
	IRDA_ASSERT(cb != NULL, return;);
	self = (struct irda_usb_cb *) cb->context;
	IRDA_ASSERT(self != NULL, return;);

	/* If the network is closed or the device gone, stop everything */
	if ((!self->netopen) || (!self->present)) {
		IRDA_DEBUG(0, "%s(), Network is gone!\n", __func__);
		/* Don't re-submit the URB : will stall the Rx path */
		return;
	}
	
	/* Check the status */
	if (urb->status != 0) {
		switch (urb->status) {
		case -EILSEQ:
			self->netdev->stats.rx_crc_errors++;
			/* Also precursor to a hot-unplug on UHCI. */
			/* Fallthrough... */
		case -ECONNRESET:
			/* Random error, if I remember correctly */
			/* uhci_cleanup_unlink() is going to kill the Rx
			 * URB just after we return. No problem, at this
			 * point the URB will be idle ;-) - Jean II */
		case -ESHUTDOWN:
			/* That's usually a hot-unplug. Submit will fail... */
		case -ETIME:
			/* Usually precursor to a hot-unplug on OHCI. */
		default:
			self->netdev->stats.rx_errors++;
			IRDA_DEBUG(0, "%s(), RX status %d, transfer_flags 0x%04X\n", __func__, urb->status, urb->transfer_flags);
			break;
		}
		/* If we received an error, we don't want to resubmit the
		 * Rx URB straight away but to give the USB layer a little
		 * bit of breathing room.
		 * We are in the USB thread context, therefore there is a
		 * danger of recursion (new URB we submit fails, we come
		 * back here).
		 * With recent USB stack (2.6.15+), I'm seeing that on
		 * hot unplug of the dongle...
		 * Lowest effective timer is 10ms...
		 * Jean II */
		self->rx_defer_timer.function = irda_usb_rx_defer_expired;
		self->rx_defer_timer.data = (unsigned long) urb;
		mod_timer(&self->rx_defer_timer, jiffies + (10 * HZ / 1000));
		return;
	}
	
	/* Check for empty frames */
	if (urb->actual_length <= self->header_length) {
		IRDA_WARNING("%s(), empty frame!\n", __func__);
		goto done;
	}

	/*  
	 * Remember the time we received this frame, so we can
	 * reduce the min turn time a bit since we will know
	 * how much time we have used for protocol processing
	 */
        do_gettimeofday(&self->stamp);

	/* Check if we need to copy the data to a new skb or not.
	 * For most frames, we use ZeroCopy and pass the already
	 * allocated skb up the stack.
	 * If the frame is small, it is more efficient to copy it
	 * to save memory (copy will be fast anyway - that's
	 * called Rx-copy-break). Jean II */
	docopy = (urb->actual_length < IRDA_RX_COPY_THRESHOLD);

	/* Allocate a new skb */
	if (self->capability & IUC_STIR421X)
		newskb = dev_alloc_skb(docopy ? urb->actual_length :
				       IRDA_SKB_MAX_MTU +
				       USB_IRDA_STIR421X_HEADER);
	else
		newskb = dev_alloc_skb(docopy ? urb->actual_length :
				       IRDA_SKB_MAX_MTU);

	if (!newskb)  {
		self->netdev->stats.rx_dropped++;
		/* We could deliver the current skb, but this would stall
		 * the Rx path. Better drop the packet... Jean II */
		goto done;  
	}

	/* Make sure IP header get aligned (IrDA header is 5 bytes) */
	/* But IrDA-USB header is 1 byte. Jean II */
	//skb_reserve(newskb, USB_IRDA_HEADER - 1);

	if(docopy) {
		/* Copy packet, so we can recycle the original */
		skb_copy_from_linear_data(skb, newskb->data, urb->actual_length);
		/* Deliver this new skb */
		dataskb = newskb;
		/* And hook the old skb to the URB
		 * Note : we don't need to "clean up" the old skb,
		 * as we never touched it. Jean II */
	} else {
		/* We are using ZeroCopy. Deliver old skb */
		dataskb = skb;
		/* And hook the new skb to the URB */
		skb = newskb;
	}

	/* Set proper length on skb & remove USB-IrDA header */
	skb_put(dataskb, urb->actual_length);
	skb_pull(dataskb, self->header_length);

	/* Ask the networking layer to queue the packet for the IrDA stack */
	dataskb->dev = self->netdev;
	skb_reset_mac_header(dataskb);
	dataskb->protocol = htons(ETH_P_IRDA);
	len = dataskb->len;
	netif_rx(dataskb);

	/* Keep stats up to date */
	self->netdev->stats.rx_bytes += len;
	self->netdev->stats.rx_packets++;

done:
	/* Note : at this point, the URB we've just received (urb)
	 * is still referenced by the USB layer. For example, if we
	 * have received a -ECONNRESET, uhci_cleanup_unlink() will
	 * continue to process it (in fact, cleaning it up).
	 * If we were to submit this URB, disaster would ensue.
	 * Therefore, we submit our idle URB, and put this URB in our
	 * idle slot....
	 * Jean II */
	/* Note : with this scheme, we could submit the idle URB before
	 * processing the Rx URB. I don't think it would buy us anything as
	 * we are running in the USB thread context. Jean II */
	next_urb = self->idle_rx_urb;

	/* Recycle Rx URB : Now, the idle URB is the present one */
	urb->context = NULL;
	self->idle_rx_urb = urb;

	/* Submit the idle URB to replace the URB we've just received.
	 * Do it last to avoid race conditions... Jean II */
	irda_usb_submit(self, skb, next_urb);
}

/*------------------------------------------------------------------*/
/*
 * In case of errors, we want the USB layer to have time to recover.
 * Now, it is time to resubmit ouur Rx URB...
 */
static void irda_usb_rx_defer_expired(unsigned long data)
{
	struct urb *urb = (struct urb *) data;
	struct sk_buff *skb = (struct sk_buff *) urb->context;
	struct irda_usb_cb *self; 
	struct irda_skb_cb *cb;
	struct urb *next_urb;

	IRDA_DEBUG(2, "%s()\n", __func__);

	/* Find ourselves */
	cb = (struct irda_skb_cb *) skb->cb;
	IRDA_ASSERT(cb != NULL, return;);
	self = (struct irda_usb_cb *) cb->context;
	IRDA_ASSERT(self != NULL, return;);

	/* Same stuff as when Rx is done, see above... */
	next_urb = self->idle_rx_urb;
	urb->context = NULL;
	self->idle_rx_urb = urb;
	irda_usb_submit(self, skb, next_urb);
}

/*------------------------------------------------------------------*/
/*
 * Callbak from IrDA layer. IrDA wants to know if we have
 * started receiving anything.
 */
static int irda_usb_is_receiving(struct irda_usb_cb *self)
{
	/* Note : because of the way UHCI works, it's almost impossible
	 * to get this info. The Controller DMA directly to memory and
	 * signal only when the whole frame is finished. To know if the
	 * first TD of the URB has been filled or not seems hard work...
	 *
	 * The other solution would be to use the "receiving" command
	 * on the default decriptor with a usb_control_msg(), but that
	 * would add USB traffic and would return result only in the
	 * next USB frame (~1ms).
	 *
	 * I've been told that current dongles send status info on their
	 * interrupt endpoint, and that's what the Windows driver uses
	 * to know this info. Unfortunately, this is not yet in the spec...
	 *
	 * Jean II
	 */

	return 0; /* For now */
}

#define STIR421X_PATCH_PRODUCT_VER     "Product Version: "
#define STIR421X_PATCH_STMP_TAG        "STMP"
#define STIR421X_PATCH_CODE_OFFSET     512 /* patch image starts before here */
/* marks end of patch file header (PC DOS text file EOF character) */
#define STIR421X_PATCH_END_OF_HDR_TAG  0x1A
#define STIR421X_PATCH_BLOCK_SIZE      1023

/*
 * Function stir421x_fwupload (struct irda_usb_cb *self,
 *                             unsigned char *patch,
 *                             const unsigned int patch_len)
 *
 *   Upload firmware code to SigmaTel 421X IRDA-USB dongle
 */
static int stir421x_fw_upload(struct irda_usb_cb *self,
			     const unsigned char *patch,
			     const unsigned int patch_len)
{
        int ret = -ENOMEM;
        int actual_len = 0;
        unsigned int i;
        unsigned int block_size = 0;
        unsigned char *patch_block;

        patch_block = kzalloc(STIR421X_PATCH_BLOCK_SIZE, GFP_KERNEL);
	if (patch_block == NULL)
		return -ENOMEM;

	/* break up patch into 1023-byte sections */
	for (i = 0; i < patch_len; i += block_size) {
		block_size = patch_len - i;

		if (block_size > STIR421X_PATCH_BLOCK_SIZE)
			block_size = STIR421X_PATCH_BLOCK_SIZE;

		/* upload the patch section */
		memcpy(patch_block, patch + i, block_size);

		ret = usb_bulk_msg(self->usbdev,
				   usb_sndbulkpipe(self->usbdev,
						   self->bulk_out_ep),
				   patch_block, block_size,
				   &actual_len, msecs_to_jiffies(500));
		IRDA_DEBUG(3,"%s(): Bulk send %u bytes, ret=%d\n",
			   __func__, actual_len, ret);

		if (ret < 0)
			break;

		mdelay(10);
	}

	kfree(patch_block);

        return ret;
 }

/*
 * Function stir421x_patch_device(struct irda_usb_cb *self)
 *
 * Get a firmware code from userspase using hotplug request_firmware() call
  */
static int stir421x_patch_device(struct irda_usb_cb *self)
{
	unsigned int i;
	int ret;
	char stir421x_fw_name[12];
	const struct firmware *fw;
	const unsigned char *fw_version_ptr; /* pointer to version string */
	unsigned long fw_version = 0;

        /*
         * Known firmware patch file names for STIR421x dongles
         * are "42101001.sb" or "42101002.sb"
         */
        sprintf(stir421x_fw_name, "4210%4X.sb",
                self->usbdev->descriptor.bcdDevice);
        ret = request_firmware(&fw, stir421x_fw_name, &self->usbdev->dev);
        if (ret < 0)
                return ret;

        /* We get a patch from userspace */
        IRDA_MESSAGE("%s(): Received firmware %s (%zu bytes)\n",
                     __func__, stir421x_fw_name, fw->size);

        ret = -EINVAL;

	/* Get the bcd product version */
        if (!memcmp(fw->data, STIR421X_PATCH_PRODUCT_VER,
                    sizeof(STIR421X_PATCH_PRODUCT_VER) - 1)) {
                fw_version_ptr = fw->data +
			sizeof(STIR421X_PATCH_PRODUCT_VER) - 1;

                /* Let's check if the product version is dotted */
                if (fw_version_ptr[3] == '.' &&
		    fw_version_ptr[7] == '.') {
			unsigned long major, minor, build;
			major = simple_strtoul(fw_version_ptr, NULL, 10);
			minor = simple_strtoul(fw_version_ptr + 4, NULL, 10);
			build = simple_strtoul(fw_version_ptr + 8, NULL, 10);

			fw_version = (major << 12)
				+ (minor << 8)
				+ ((build / 10) << 4)
				+ (build % 10);

			IRDA_DEBUG(3, "%s(): Firmware Product version %ld\n",
                                   __func__, fw_version);
                }
        }

        if (self->usbdev->descriptor.bcdDevice == cpu_to_le16(fw_version)) {
                /*
		 * If we're here, we've found a correct patch
                 * The actual image starts after the "STMP" keyword
                 * so forward to the firmware header tag
                 */
                for (i = 0; i < fw->size && fw->data[i] !=
			     STIR421X_PATCH_END_OF_HDR_TAG; i++) ;
                /* here we check for the out of buffer case */
                if (i < STIR421X_PATCH_CODE_OFFSET && i < fw->size &&
				STIR421X_PATCH_END_OF_HDR_TAG == fw->data[i]) {
                        if (!memcmp(fw->data + i + 1, STIR421X_PATCH_STMP_TAG,
                                    sizeof(STIR421X_PATCH_STMP_TAG) - 1)) {

				/* We can upload the patch to the target */
				i += sizeof(STIR421X_PATCH_STMP_TAG);
                                ret = stir421x_fw_upload(self, &fw->data[i],
							 fw->size - i);
                        }
                }
        }

        release_firmware(fw);

        return ret;
}


/********************** IRDA DEVICE CALLBACKS **********************/
/*
 * Main calls from the IrDA/Network subsystem.
 * Mostly registering a new irda-usb device and removing it....
 * We only deal with the IrDA side of the business, the USB side will
 * be dealt with below...
 */


/*------------------------------------------------------------------*/
/*
 * Function irda_usb_net_open (dev)
 *
 *    Network device is taken up. Usually this is done by "ifconfig irda0 up" 
 *   
 * Note : don't mess with self->netopen - Jean II
 */
static int irda_usb_net_open(struct net_device *netdev)
{
	struct irda_usb_cb *self;
	unsigned long flags;
	char	hwname[16];
	int i;
	
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(netdev != NULL, return -1;);
	self = netdev_priv(netdev);
	IRDA_ASSERT(self != NULL, return -1;);

	spin_lock_irqsave(&self->lock, flags);
	/* Can only open the device if it's there */
	if(!self->present) {
		spin_unlock_irqrestore(&self->lock, flags);
		IRDA_WARNING("%s(), device not present!\n", __func__);
		return -1;
	}

	if(self->needspatch) {
		spin_unlock_irqrestore(&self->lock, flags);
		IRDA_WARNING("%s(), device needs patch\n", __func__) ;
		return -EIO ;
	}

	/* Initialise default speed and xbofs value
	 * (IrLAP will change that soon) */
	self->speed = -1;
	self->xbofs = -1;
	self->new_speed = -1;
	self->new_xbofs = -1;

	/* To do *before* submitting Rx urbs and starting net Tx queue
	 * Jean II */
	self->netopen = 1;
	spin_unlock_irqrestore(&self->lock, flags);

	/* 
	 * Now that everything should be initialized properly,
	 * Open new IrLAP layer instance to take care of us...
	 * Note : will send immediately a speed change...
	 */
	sprintf(hwname, "usb#%d", self->usbdev->devnum);
	self->irlap = irlap_open(netdev, &self->qos, hwname);
	IRDA_ASSERT(self->irlap != NULL, return -1;);

	/* Allow IrLAP to send data to us */
	netif_start_queue(netdev);

	/* We submit all the Rx URB except for one that we keep idle.
	 * Need to be initialised before submitting other USBs, because
	 * in some cases as soon as we submit the URBs the USB layer
	 * will trigger a dummy receive - Jean II */
	self->idle_rx_urb = self->rx_urb[IU_MAX_ACTIVE_RX_URBS];
	self->idle_rx_urb->context = NULL;

	/* Now that we can pass data to IrLAP, allow the USB layer
	 * to send us some data... */
	for (i = 0; i < IU_MAX_ACTIVE_RX_URBS; i++) {
		struct sk_buff *skb = dev_alloc_skb(IRDA_SKB_MAX_MTU);
		if (!skb) {
			/* If this ever happen, we are in deep s***.
			 * Basically, we can't start the Rx path... */
			IRDA_WARNING("%s(), Failed to allocate Rx skb\n",
				     __func__);
			return -1;
		}
		//skb_reserve(newskb, USB_IRDA_HEADER - 1);
		irda_usb_submit(self, skb, self->rx_urb[i]);
	}

	/* Ready to play !!! */
	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Function irda_usb_net_close (self)
 *
 *    Network device is taken down. Usually this is done by 
 *    "ifconfig irda0 down" 
 */
static int irda_usb_net_close(struct net_device *netdev)
{
	struct irda_usb_cb *self;
	int	i;

	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(netdev != NULL, return -1;);
	self = netdev_priv(netdev);
	IRDA_ASSERT(self != NULL, return -1;);

	/* Clear this flag *before* unlinking the urbs and *before*
	 * stopping the network Tx queue - Jean II */
	self->netopen = 0;

	/* Stop network Tx queue */
	netif_stop_queue(netdev);

	/* Kill defered Rx URB */
	del_timer(&self->rx_defer_timer);

	/* Deallocate all the Rx path buffers (URBs and skb) */
	for (i = 0; i < self->max_rx_urb; i++) {
		struct urb *urb = self->rx_urb[i];
		struct sk_buff *skb = (struct sk_buff *) urb->context;
		/* Cancel the receive command */
		usb_kill_urb(urb);
		/* The skb is ours, free it */
		if(skb) {
			dev_kfree_skb(skb);
			urb->context = NULL;
		}
	}
	/* Cancel Tx and speed URB - need to be synchronous to avoid races */
	usb_kill_urb(self->tx_urb);
	usb_kill_urb(self->speed_urb);

	/* Stop and remove instance of IrLAP */
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * IOCTLs : Extra out-of-band network commands...
 */
static int irda_usb_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	unsigned long flags;
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct irda_usb_cb *self;
	int ret = 0;

	IRDA_ASSERT(dev != NULL, return -1;);
	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __func__, dev->name, cmd);

	switch (cmd) {
	case SIOCSBANDWIDTH: /* Set bandwidth */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* Protect us from USB callbacks, net watchdog and else. */
		spin_lock_irqsave(&self->lock, flags);
		/* Check if the device is still there */
		if(self->present) {
			/* Set the desired speed */
			self->new_speed = irq->ifr_baudrate;
			irda_usb_change_speed_xbofs(self);
		}
		spin_unlock_irqrestore(&self->lock, flags);
		break;
	case SIOCSMEDIABUSY: /* Set media busy */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* Check if the IrDA stack is still there */
		if(self->netopen)
			irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: /* Check if we are receiving right now */
		irq->ifr_receiving = irda_usb_is_receiving(self);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	
	return ret;
}

/*------------------------------------------------------------------*/

/********************* IRDA CONFIG SUBROUTINES *********************/
/*
 * Various subroutines dealing with IrDA and network stuff we use to
 * configure and initialise each irda-usb instance.
 * These functions are used below in the main calls of the driver...
 */

/*------------------------------------------------------------------*/
/*
 * Set proper values in the IrDA QOS structure
 */
static inline void irda_usb_init_qos(struct irda_usb_cb *self)
{
	struct irda_class_desc *desc;

	IRDA_DEBUG(3, "%s()\n", __func__);
	
	desc = self->irda_desc;
	
	/* Initialize QoS for this device */
	irda_init_max_qos_capabilies(&self->qos);

	/* See spec section 7.2 for meaning.
	 * Values are little endian (as most USB stuff), the IrDA stack
	 * use it in native order (see parameters.c). - Jean II */
	self->qos.baud_rate.bits       = le16_to_cpu(desc->wBaudRate);
	self->qos.min_turn_time.bits   = desc->bmMinTurnaroundTime;
	self->qos.additional_bofs.bits = desc->bmAdditionalBOFs;
	self->qos.window_size.bits     = desc->bmWindowSize;
	self->qos.data_size.bits       = desc->bmDataSize;

	IRDA_DEBUG(0, "%s(), dongle says speed=0x%X, size=0x%X, window=0x%X, bofs=0x%X, turn=0x%X\n", 
		__func__, self->qos.baud_rate.bits, self->qos.data_size.bits, self->qos.window_size.bits, self->qos.additional_bofs.bits, self->qos.min_turn_time.bits);

	/* Don't always trust what the dongle tell us */
	if(self->capability & IUC_SIR_ONLY)
		self->qos.baud_rate.bits	&= 0x00ff;
	if(self->capability & IUC_SMALL_PKT)
		self->qos.data_size.bits	 = 0x07;
	if(self->capability & IUC_NO_WINDOW)
		self->qos.window_size.bits	 = 0x01;
	if(self->capability & IUC_MAX_WINDOW)
		self->qos.window_size.bits	 = 0x7f;
	if(self->capability & IUC_MAX_XBOFS)
		self->qos.additional_bofs.bits	 = 0x01;

#if 1
	/* Module parameter can override the rx window size */
	if (qos_mtt_bits)
		self->qos.min_turn_time.bits = qos_mtt_bits;
#endif	    
	/* 
	 * Note : most of those values apply only for the receive path,
	 * the transmit path will be set differently - Jean II 
	 */
	irda_qos_bits_to_value(&self->qos);
}

/*------------------------------------------------------------------*/
static const struct net_device_ops irda_usb_netdev_ops = {
	.ndo_open       = irda_usb_net_open,
	.ndo_stop       = irda_usb_net_close,
	.ndo_do_ioctl   = irda_usb_net_ioctl,
	.ndo_start_xmit = irda_usb_hard_xmit,
	.ndo_tx_timeout	= irda_usb_net_timeout,
};

/*
 * Initialise the network side of the irda-usb instance
 * Called when a new USB instance is registered in irda_usb_probe()
 */
static inline int irda_usb_open(struct irda_usb_cb *self)
{
	struct net_device *netdev = self->netdev;

	IRDA_DEBUG(1, "%s()\n", __func__);

	netdev->netdev_ops = &irda_usb_netdev_ops;

	irda_usb_init_qos(self);

	return register_netdev(netdev);
}

/*------------------------------------------------------------------*/
/*
 * Cleanup the network side of the irda-usb instance
 * Called when a USB instance is removed in irda_usb_disconnect()
 */
static inline void irda_usb_close(struct irda_usb_cb *self)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	/* Remove netdevice */
	unregister_netdev(self->netdev);

	/* Remove the speed buffer */
	kfree(self->speed_buff);
	self->speed_buff = NULL;

	kfree(self->tx_buff);
	self->tx_buff = NULL;
}

/********************** USB CONFIG SUBROUTINES **********************/
/*
 * Various subroutines dealing with USB stuff we use to configure and
 * initialise each irda-usb instance.
 * These functions are used below in the main calls of the driver...
 */

/*------------------------------------------------------------------*/
/*
 * Function irda_usb_parse_endpoints(dev, ifnum)
 *
 *    Parse the various endpoints and find the one we need.
 *
 * The endpoint are the pipes used to communicate with the USB device.
 * The spec defines 2 endpoints of type bulk transfer, one in, and one out.
 * These are used to pass frames back and forth with the dongle.
 * Most dongle have also an interrupt endpoint, that will be probably
 * documented in the next spec...
 */
static inline int irda_usb_parse_endpoints(struct irda_usb_cb *self, struct usb_host_endpoint *endpoint, int ennum)
{
	int i;		/* Endpoint index in table */
		
	/* Init : no endpoints */
	self->bulk_in_ep = 0;
	self->bulk_out_ep = 0;
	self->bulk_int_ep = 0;

	/* Let's look at all those endpoints */
	for(i = 0; i < ennum; i++) {
		/* All those variables will get optimised by the compiler,
		 * so let's aim for clarity... - Jean II */
		__u8 ep;	/* Endpoint address */
		__u8 dir;	/* Endpoint direction */
		__u8 attr;	/* Endpoint attribute */
		__u16 psize;	/* Endpoint max packet size in bytes */

		/* Get endpoint address, direction and attribute */
		ep = endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
		dir = endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		attr = endpoint[i].desc.bmAttributes;
		psize = le16_to_cpu(endpoint[i].desc.wMaxPacketSize);

		/* Is it a bulk endpoint ??? */
		if(attr == USB_ENDPOINT_XFER_BULK) {
			/* We need to find an IN and an OUT */
			if(dir == USB_DIR_IN) {
				/* This is our Rx endpoint */
				self->bulk_in_ep = ep;
			} else {
				/* This is our Tx endpoint */
				self->bulk_out_ep = ep;
				self->bulk_out_mtu = psize;
			}
		} else {
			if((attr == USB_ENDPOINT_XFER_INT) &&
			   (dir == USB_DIR_IN)) {
				/* This is our interrupt endpoint */
				self->bulk_int_ep = ep;
			} else {
				IRDA_ERROR("%s(), Unrecognised endpoint %02X.\n", __func__, ep);
			}
		}
	}

	IRDA_DEBUG(0, "%s(), And our endpoints are : in=%02X, out=%02X (%d), int=%02X\n",
		__func__, self->bulk_in_ep, self->bulk_out_ep, self->bulk_out_mtu, self->bulk_int_ep);

	return (self->bulk_in_ep != 0) && (self->bulk_out_ep != 0);
}

#ifdef IU_DUMP_CLASS_DESC
/*------------------------------------------------------------------*/
/*
 * Function usb_irda_dump_class_desc(desc)
 *
 *    Prints out the contents of the IrDA class descriptor
 *
 */
static inline void irda_usb_dump_class_desc(struct irda_class_desc *desc)
{
	/* Values are little endian */
	printk("bLength=%x\n", desc->bLength);
	printk("bDescriptorType=%x\n", desc->bDescriptorType);
	printk("bcdSpecRevision=%x\n", le16_to_cpu(desc->bcdSpecRevision)); 
	printk("bmDataSize=%x\n", desc->bmDataSize);
	printk("bmWindowSize=%x\n", desc->bmWindowSize);
	printk("bmMinTurnaroundTime=%d\n", desc->bmMinTurnaroundTime);
	printk("wBaudRate=%x\n", le16_to_cpu(desc->wBaudRate));
	printk("bmAdditionalBOFs=%x\n", desc->bmAdditionalBOFs);
	printk("bIrdaRateSniff=%x\n", desc->bIrdaRateSniff);
	printk("bMaxUnicastList=%x\n", desc->bMaxUnicastList);
}
#endif /* IU_DUMP_CLASS_DESC */

/*------------------------------------------------------------------*/
/*
 * Function irda_usb_find_class_desc(intf)
 *
 *    Returns instance of IrDA class descriptor, or NULL if not found
 *
 * The class descriptor is some extra info that IrDA USB devices will
 * offer to us, describing their IrDA characteristics. We will use that in
 * irda_usb_init_qos()
 */
static inline struct irda_class_desc *irda_usb_find_class_desc(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev (intf);
	struct irda_class_desc *desc;
	int ret;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	/* USB-IrDA class spec 1.0:
	 *	6.1.3: Standard "Get Descriptor" Device Request is not
	 *	       appropriate to retrieve class-specific descriptor
	 *	6.2.5: Class Specific "Get Class Descriptor" Interface Request
	 *	       is mandatory and returns the USB-IrDA class descriptor
	 */

	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev,0),
		IU_REQ_GET_CLASS_DESC,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		0, intf->altsetting->desc.bInterfaceNumber, desc,
		sizeof(*desc), 500);
	
	IRDA_DEBUG(1, "%s(), ret=%d\n", __func__, ret);
	if (ret < sizeof(*desc)) {
		IRDA_WARNING("usb-irda: class_descriptor read %s (%d)\n",
			     (ret<0) ? "failed" : "too short", ret);
	}
	else if (desc->bDescriptorType != USB_DT_IRDA) {
		IRDA_WARNING("usb-irda: bad class_descriptor type\n");
	}
	else {
#ifdef IU_DUMP_CLASS_DESC
		irda_usb_dump_class_desc(desc);
#endif	/* IU_DUMP_CLASS_DESC */

		return desc;
	}
	kfree(desc);
	return NULL;
}

/*********************** USB DEVICE CALLBACKS ***********************/
/*
 * Main calls from the USB subsystem.
 * Mostly registering a new irda-usb device and removing it....
 */

/*------------------------------------------------------------------*/
/*
 * This routine is called by the USB subsystem for each new device
 * in the system. We need to check if the device is ours, and in
 * this case start handling it.
 * The USB layer protect us from reentrancy (via BKL), so we don't need
 * to spinlock in there... Jean II
 */
static int irda_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	struct net_device *net;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct irda_usb_cb *self;
	struct usb_host_interface *interface;
	struct irda_class_desc *irda_desc;
	int ret = -ENOMEM;
	int i;		/* Driver instance index / Rx URB index */

	/* Note : the probe make sure to call us only for devices that
	 * matches the list of dongle (top of the file). So, we
	 * don't need to check if the dongle is really ours.
	 * Jean II */

	IRDA_MESSAGE("IRDA-USB found at address %d, Vendor: %x, Product: %x\n",
		     dev->devnum, le16_to_cpu(dev->descriptor.idVendor),
		     le16_to_cpu(dev->descriptor.idProduct));

	net = alloc_irdadev(sizeof(*self));
	if (!net) 
		goto err_out;

	SET_NETDEV_DEV(net, &intf->dev);
	self = netdev_priv(net);
	self->netdev = net;
	spin_lock_init(&self->lock);
	init_timer(&self->rx_defer_timer);

	self->capability = id->driver_info;
	self->needspatch = ((self->capability & IUC_STIR421X) != 0);

	/* Create all of the needed urbs */
	if (self->capability & IUC_STIR421X) {
		self->max_rx_urb = IU_SIGMATEL_MAX_RX_URBS;
		self->header_length = USB_IRDA_STIR421X_HEADER;
	} else {
		self->max_rx_urb = IU_MAX_RX_URBS;
		self->header_length = USB_IRDA_HEADER;
	}

	self->rx_urb = kcalloc(self->max_rx_urb, sizeof(struct urb *),
				GFP_KERNEL);
	if (!self->rx_urb)
		goto err_free_net;

	for (i = 0; i < self->max_rx_urb; i++) {
		self->rx_urb[i] = usb_alloc_urb(0, GFP_KERNEL);
		if (!self->rx_urb[i]) {
			goto err_out_1;
		}
	}
	self->tx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!self->tx_urb) {
		goto err_out_1;
	}
	self->speed_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!self->speed_urb) {
		goto err_out_2;
	}

	/* Is this really necessary? (no, except maybe for broken devices) */
	if (usb_reset_configuration (dev) < 0) {
		err("reset_configuration failed");
		ret = -EIO;
		goto err_out_3;
	}

	/* Is this really necessary? */
	/* Note : some driver do hardcode the interface number, some others
	 * specify an alternate, but very few driver do like this.
	 * Jean II */
	ret = usb_set_interface(dev, intf->altsetting->desc.bInterfaceNumber, 0);
	IRDA_DEBUG(1, "usb-irda: set interface %d result %d\n", intf->altsetting->desc.bInterfaceNumber, ret);
	switch (ret) {
		case 0:
			break;
		case -EPIPE:		/* -EPIPE = -32 */
			/* Martin Diehl says if we get a -EPIPE we should
			 * be fine and we don't need to do a usb_clear_halt().
			 * - Jean II */
			IRDA_DEBUG(0, "%s(), Received -EPIPE, ignoring...\n", __func__);
			break;
		default:
			IRDA_DEBUG(0, "%s(), Unknown error %d\n", __func__, ret);
			ret = -EIO;
			goto err_out_3;
	}

	/* Find our endpoints */
	interface = intf->cur_altsetting;
	if(!irda_usb_parse_endpoints(self, interface->endpoint,
				     interface->desc.bNumEndpoints)) {
		IRDA_ERROR("%s(), Bogus endpoints...\n", __func__);
		ret = -EIO;
		goto err_out_3;
	}

	self->usbdev = dev;

	/* Find IrDA class descriptor */
	irda_desc = irda_usb_find_class_desc(intf);
	ret = -ENODEV;
	if (!irda_desc)
		goto err_out_3;

	if (self->needspatch) {
		ret = usb_control_msg (self->usbdev, usb_sndctrlpipe (self->usbdev, 0),
				       0x02, 0x40, 0, 0, NULL, 0, 500);
		if (ret < 0) {
			IRDA_DEBUG (0, "usb_control_msg failed %d\n", ret);
			goto err_out_3;
		} else {
			mdelay(10);
		}
	}

	self->irda_desc =  irda_desc;
	self->present = 1;
	self->netopen = 0;
	self->usbintf = intf;

	/* Allocate the buffer for speed changes */
	/* Don't change this buffer size and allocation without doing
	 * some heavy and complete testing. Don't ask why :-(
	 * Jean II */
	self->speed_buff = kzalloc(IRDA_USB_SPEED_MTU, GFP_KERNEL);
	if (!self->speed_buff)
		goto err_out_3;

	self->tx_buff = kzalloc(IRDA_SKB_MAX_MTU + self->header_length,
				GFP_KERNEL);
	if (!self->tx_buff)
		goto err_out_4;

	ret = irda_usb_open(self);
	if (ret) 
		goto err_out_5;

	IRDA_MESSAGE("IrDA: Registered device %s\n", net->name);
	usb_set_intfdata(intf, self);

	if (self->needspatch) {
		/* Now we fetch and upload the firmware patch */
		ret = stir421x_patch_device(self);
		self->needspatch = (ret < 0);
		if (self->needspatch) {
			IRDA_ERROR("STIR421X: Couldn't upload patch\n");
			goto err_out_6;
		}

		/* replace IrDA class descriptor with what patched device is now reporting */
		irda_desc = irda_usb_find_class_desc (self->usbintf);
		if (!irda_desc) {
			ret = -ENODEV;
			goto err_out_6;
		}
		kfree(self->irda_desc);
		self->irda_desc = irda_desc;
		irda_usb_init_qos(self);
	}

	return 0;
err_out_6:
	unregister_netdev(self->netdev);
err_out_5:
	kfree(self->tx_buff);
err_out_4:
	kfree(self->speed_buff);
err_out_3:
	/* Free all urbs that we may have created */
	usb_free_urb(self->speed_urb);
err_out_2:
	usb_free_urb(self->tx_urb);
err_out_1:
	for (i = 0; i < self->max_rx_urb; i++)
		usb_free_urb(self->rx_urb[i]);
	kfree(self->rx_urb);
err_free_net:
	free_netdev(net);
err_out:
	return ret;
}

/*------------------------------------------------------------------*/
/*
 * The current irda-usb device is removed, the USB layer tell us
 * to shut it down...
 * One of the constraints is that when we exit this function,
 * we cannot use the usb_device no more. Gone. Destroyed. kfree().
 * Most other subsystem allow you to destroy the instance at a time
 * when it's convenient to you, to postpone it to a later date, but
 * not the USB subsystem.
 * So, we must make bloody sure that everything gets deactivated.
 * Jean II
 */
static void irda_usb_disconnect(struct usb_interface *intf)
{
	unsigned long flags;
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	IRDA_DEBUG(1, "%s()\n", __func__);

	usb_set_intfdata(intf, NULL);
	if (!self)
		return;

	/* Make sure that the Tx path is not executing. - Jean II */
	spin_lock_irqsave(&self->lock, flags);

	/* Oups ! We are not there any more.
	 * This will stop/desactivate the Tx path. - Jean II */
	self->present = 0;

	/* Kill defered Rx URB */
	del_timer(&self->rx_defer_timer);

	/* We need to have irq enabled to unlink the URBs. That's OK,
	 * at this point the Tx path is gone - Jean II */
	spin_unlock_irqrestore(&self->lock, flags);

	/* Hum... Check if networking is still active (avoid races) */
	if((self->netopen) || (self->irlap)) {
		/* Accept no more transmissions */
		/*netif_device_detach(self->netdev);*/
		netif_stop_queue(self->netdev);
		/* Stop all the receive URBs. Must be synchronous. */
		for (i = 0; i < self->max_rx_urb; i++)
			usb_kill_urb(self->rx_urb[i]);
		/* Cancel Tx and speed URB.
		 * Make sure it's synchronous to avoid races. */
		usb_kill_urb(self->tx_urb);
		usb_kill_urb(self->speed_urb);
	}

	/* Cleanup the device stuff */
	irda_usb_close(self);
	/* No longer attached to USB bus */
	self->usbdev = NULL;
	self->usbintf = NULL;

	/* Clean up our urbs */
	for (i = 0; i < self->max_rx_urb; i++)
		usb_free_urb(self->rx_urb[i]);
	kfree(self->rx_urb);
	/* Clean up Tx and speed URB */
	usb_free_urb(self->tx_urb);
	usb_free_urb(self->speed_urb);

	/* Free self and network device */
	free_netdev(self->netdev);
	IRDA_DEBUG(0, "%s(), USB IrDA Disconnected\n", __func__);
}

#ifdef CONFIG_PM
/* USB suspend, so power off the transmitter/receiver */
static int irda_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	netif_device_detach(self->netdev);

	if (self->tx_urb != NULL)
		usb_kill_urb(self->tx_urb);
	if (self->speed_urb != NULL)
		usb_kill_urb(self->speed_urb);
	for (i = 0; i < self->max_rx_urb; i++) {
		if (self->rx_urb[i] != NULL)
			usb_kill_urb(self->rx_urb[i]);
	}
	return 0;
}

/* Coming out of suspend, so reset hardware */
static int irda_usb_resume(struct usb_interface *intf)
{
	struct irda_usb_cb *self = usb_get_intfdata(intf);
	int i;

	for (i = 0; i < self->max_rx_urb; i++) {
		if (self->rx_urb[i] != NULL)
			usb_submit_urb(self->rx_urb[i], GFP_KERNEL);
	}

	netif_device_attach(self->netdev);
	return 0;
}
#endif

/*------------------------------------------------------------------*/
/*
 * USB device callbacks
 */
static struct usb_driver irda_driver = {
	.name		= "irda-usb",
	.probe		= irda_usb_probe,
	.disconnect	= irda_usb_disconnect,
	.id_table	= dongles,
#ifdef CONFIG_PM
	.suspend	= irda_usb_suspend,
	.resume		= irda_usb_resume,
#endif
};

/************************* MODULE CALLBACKS *************************/
/*
 * Deal with module insertion/removal
 * Mostly tell USB about our existence
 */

/*------------------------------------------------------------------*/
/*
 * Module insertion
 */
static int __init usb_irda_init(void)
{
	int	ret;

	ret = usb_register(&irda_driver);
	if (ret < 0)
		return ret;

	IRDA_MESSAGE("USB IrDA support registered\n");
	return 0;
}
module_init(usb_irda_init);

/*------------------------------------------------------------------*/
/*
 * Module removal
 */
static void __exit usb_irda_cleanup(void)
{
	/* Deregister the driver and remove all pending instances */
	usb_deregister(&irda_driver);
}
module_exit(usb_irda_cleanup);

/*------------------------------------------------------------------*/
/*
 * Module parameters
 */
module_param(qos_mtt_bits, int, 0);
MODULE_PARM_DESC(qos_mtt_bits, "Minimum Turn Time");
MODULE_AUTHOR("Roman Weissgaerber <weissg@vienna.at>, Dag Brattli <dag@brattli.net>, Jean Tourrilhes <jt@hpl.hp.com> and Nick Fedchik <nick@fedchik.org.ua>");
MODULE_DESCRIPTION("IrDA-USB Dongle Driver");
MODULE_LICENSE("GPL");
