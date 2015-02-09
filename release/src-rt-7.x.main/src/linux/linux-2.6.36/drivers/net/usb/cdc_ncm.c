/*
 * cdc_ncm.c
 *
 * Copyright (C) ST-Ericsson 2010-2012
 * Contact: Alexey Orishko <alexey.orishko@stericsson.com>
 * Original author: Hans Petter Selasky <hans.petter.selasky@stericsson.com>
 *
 * USB Host Driver for Network Control Model (NCM)
 * http://www.usb.org/developers/devclass_docs/NCM10.zip
 *
 * The NCM encoding, decoding and initialization logic
 * derives from FreeBSD 8.x. if_cdce.c and if_cdcereg.h
 *
 * This software is available to you under a choice of one of two
 * licenses. You may choose this file to be licensed under the terms
 * of the GNU General Public License (GPL) Version 2 or the 2-clause
 * BSD license listed below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/ctype.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/usb.h>
#include <linux/hrtimer.h>
#include <asm/uaccess.h>

#include <linux/usb/usbnet.h>
#include <linux/usb/cdc.h>
#include <linux/usb/cdc_ncm.h>

#define	DRIVER_VERSION				"14-Mar-2012"

#ifdef CONFIG_USB_NET_CDC_MBIM
static int prefer_mbim = true;
#else
static int prefer_mbim = false;
#endif
module_param(prefer_mbim, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(prefer_mbim, "Prefer MBIM setting on dual NCM/MBIM functions");

static void cdc_ncm_txpath_bh(unsigned long param);
static void cdc_ncm_tx_timeout_start(struct cdc_ncm_ctx *ctx);
static enum hrtimer_restart cdc_ncm_tx_timer_cb(struct hrtimer *hr_timer);
static struct usb_driver cdc_ncm_driver;

static void
cdc_ncm_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *info)
{
	struct usbnet *dev = netdev_priv(net);

	strncpy(info->driver, dev->driver_name, sizeof(info->driver));
	strncpy(info->version, DRIVER_VERSION, sizeof(info->version));
	strncpy(info->fw_version, dev->driver_info->description,
		sizeof(info->fw_version));
	usb_make_path(dev->udev, info->bus_info, sizeof(info->bus_info));
}

static u8 cdc_ncm_setup(struct cdc_ncm_ctx *ctx)
{
	u32 val;
	u8 flags;
	u8 iface_no;
	int err;
	int eth_hlen;
	u16 ntb_fmt_supported;
	u32 min_dgram_size;
	u32 min_hdr_size;

	iface_no = ctx->control->cur_altsetting->desc.bInterfaceNumber;

	err = usb_control_msg(ctx->udev,
				usb_rcvctrlpipe(ctx->udev, 0),
				USB_CDC_GET_NTB_PARAMETERS,
				USB_TYPE_CLASS | USB_DIR_IN
				 | USB_RECIP_INTERFACE,
				0, iface_no, &ctx->ncm_parm,
				sizeof(ctx->ncm_parm), 10000);
	if (err < 0) {
		pr_debug("failed GET_NTB_PARAMETERS\n");
		return 1;
	}

	/* read correct set of parameters according to device mode */
	ctx->rx_max = le32_to_cpu(ctx->ncm_parm.dwNtbInMaxSize);
	ctx->tx_max = le32_to_cpu(ctx->ncm_parm.dwNtbOutMaxSize);
	ctx->tx_remainder = le16_to_cpu(ctx->ncm_parm.wNdpOutPayloadRemainder);
	ctx->tx_modulus = le16_to_cpu(ctx->ncm_parm.wNdpOutDivisor);
	ctx->tx_ndp_modulus = le16_to_cpu(ctx->ncm_parm.wNdpOutAlignment);
	/* devices prior to NCM Errata shall set this field to zero */
	ctx->tx_max_datagrams = le16_to_cpu(ctx->ncm_parm.wNtbOutMaxDatagrams);
	ntb_fmt_supported = le16_to_cpu(ctx->ncm_parm.bmNtbFormatsSupported);

	eth_hlen = ETH_HLEN;
	min_dgram_size = CDC_NCM_MIN_DATAGRAM_SIZE;
	min_hdr_size = CDC_NCM_MIN_HDR_SIZE;
	if (ctx->mbim_desc != NULL) {
		flags = ctx->mbim_desc->bmNetworkCapabilities;
		eth_hlen = 0;
		min_dgram_size = CDC_MBIM_MIN_DATAGRAM_SIZE;
		min_hdr_size = 0;
	} else if (ctx->func_desc != NULL) {
		flags = ctx->func_desc->bmNetworkCapabilities;
	} else {
		flags = 0;
	}

	pr_debug("dwNtbInMaxSize=%u dwNtbOutMaxSize=%u "
		 "wNdpOutPayloadRemainder=%u wNdpOutDivisor=%u "
		 "wNdpOutAlignment=%u wNtbOutMaxDatagrams=%u flags=0x%x\n",
		 ctx->rx_max, ctx->tx_max, ctx->tx_remainder, ctx->tx_modulus,
		 ctx->tx_ndp_modulus, ctx->tx_max_datagrams, flags);

	/* max count of tx datagrams */
	if ((ctx->tx_max_datagrams == 0) ||
			(ctx->tx_max_datagrams > CDC_NCM_DPT_DATAGRAMS_MAX))
		ctx->tx_max_datagrams = CDC_NCM_DPT_DATAGRAMS_MAX;

	/* verify maximum size of received NTB in bytes */
	if (ctx->rx_max < USB_CDC_NCM_NTB_MIN_IN_SIZE) {
		pr_debug("Using min receive length=%d\n",
						USB_CDC_NCM_NTB_MIN_IN_SIZE);
		ctx->rx_max = USB_CDC_NCM_NTB_MIN_IN_SIZE;
	}

	if (ctx->rx_max > CDC_NCM_NTB_MAX_SIZE_RX) {
		pr_debug("Using default maximum receive length=%d\n",
						CDC_NCM_NTB_MAX_SIZE_RX);
		ctx->rx_max = CDC_NCM_NTB_MAX_SIZE_RX;
	}

	/* inform device about NTB input size changes */
	if (ctx->rx_max != le32_to_cpu(ctx->ncm_parm.dwNtbInMaxSize)) {
		__le32 *dwNtbInMaxSize;

		dwNtbInMaxSize = kzalloc(sizeof(*dwNtbInMaxSize), GFP_KERNEL);
		if (!dwNtbInMaxSize) {
			err = -ENOMEM;
			goto size_err;
		}
		*dwNtbInMaxSize = cpu_to_le32(ctx->rx_max);
		err = usb_control_msg(ctx->udev,
				      usb_sndctrlpipe(ctx->udev, 0),
				      USB_CDC_SET_NTB_INPUT_SIZE,
				      USB_TYPE_CLASS | USB_DIR_OUT
				      | USB_RECIP_INTERFACE,
				      0, iface_no, dwNtbInMaxSize, 4, 1000);
		kfree(dwNtbInMaxSize);
size_err:
		if (err < 0)
			pr_debug("Setting NTB Input Size failed\n");
	}

	/* verify maximum size of transmitted NTB in bytes */
	if ((ctx->tx_max <
	    (min_hdr_size + min_dgram_size)) ||
	    (ctx->tx_max > CDC_NCM_NTB_MAX_SIZE_TX)) {
		pr_debug("Using default maximum transmit length=%d\n",
						CDC_NCM_NTB_MAX_SIZE_TX);
		ctx->tx_max = CDC_NCM_NTB_MAX_SIZE_TX;
	}

	/*
	 * verify that the structure alignment is:
	 * - power of two
	 * - not greater than the maximum transmit length
	 * - not less than four bytes
	 */
	val = ctx->tx_ndp_modulus;

	if ((val < USB_CDC_NCM_NDP_ALIGN_MIN_SIZE) ||
	    (val != ((-val) & val)) || (val >= ctx->tx_max)) {
		pr_debug("Using default alignment: 4 bytes\n");
		ctx->tx_ndp_modulus = USB_CDC_NCM_NDP_ALIGN_MIN_SIZE;
	}

	/*
	 * verify that the payload alignment is:
	 * - power of two
	 * - not greater than the maximum transmit length
	 * - not less than four bytes
	 */
	val = ctx->tx_modulus;

	if ((val < USB_CDC_NCM_NDP_ALIGN_MIN_SIZE) ||
	    (val != ((-val) & val)) || (val >= ctx->tx_max)) {
		pr_debug("Using default transmit modulus: 4 bytes\n");
		ctx->tx_modulus = USB_CDC_NCM_NDP_ALIGN_MIN_SIZE;
	}

	/* verify the payload remainder */
	if (ctx->tx_remainder >= ctx->tx_modulus) {
		pr_debug("Using default transmit remainder: 0 bytes\n");
		ctx->tx_remainder = 0;
	}

	/* adjust TX-remainder according to NCM specification. */
	ctx->tx_remainder = ((ctx->tx_remainder - eth_hlen) &
			     (ctx->tx_modulus - 1));

	/* additional configuration */

	/* set CRC Mode */
	if (flags & USB_CDC_NCM_NCAP_CRC_MODE) {
		err = usb_control_msg(ctx->udev, usb_sndctrlpipe(ctx->udev, 0),
				USB_CDC_SET_CRC_MODE,
				USB_TYPE_CLASS | USB_DIR_OUT
				 | USB_RECIP_INTERFACE,
				USB_CDC_NCM_CRC_NOT_APPENDED,
				iface_no, NULL, 0, 1000);
		if (err < 0)
			pr_debug("Setting CRC mode off failed\n");
	}

	/* set NTB format, if both formats are supported */
	if (ntb_fmt_supported & USB_CDC_NCM_NTH32_SIGN) {
		err = usb_control_msg(ctx->udev, usb_sndctrlpipe(ctx->udev, 0),
				USB_CDC_SET_NTB_FORMAT, USB_TYPE_CLASS
				 | USB_DIR_OUT | USB_RECIP_INTERFACE,
				USB_CDC_NCM_NTB16_FORMAT,
				iface_no, NULL, 0, 1000);
		if (err < 0)
			pr_debug("Setting NTB format to 16-bit failed\n");
	}

	ctx->max_datagram_size = min_dgram_size;

	/* set Max Datagram Size (MTU) */
	if (flags & USB_CDC_NCM_NCAP_MAX_DATAGRAM_SIZE) {
		__le16 *max_datagram_size;
		u16 eth_max_sz;
		if (ctx->ether_desc != NULL)
			eth_max_sz = le16_to_cpu(ctx->ether_desc->wMaxSegmentSize);
		else if (ctx->mbim_desc != NULL)
			eth_max_sz = le16_to_cpu(ctx->mbim_desc->wMaxSegmentSize);
		else
			goto max_dgram_err;

		max_datagram_size = kzalloc(sizeof(*max_datagram_size),
				GFP_KERNEL);
		if (!max_datagram_size) {
			err = -ENOMEM;
			goto max_dgram_err;
		}

		err = usb_control_msg(ctx->udev, usb_rcvctrlpipe(ctx->udev, 0),
				USB_CDC_GET_MAX_DATAGRAM_SIZE,
				USB_TYPE_CLASS | USB_DIR_IN
				 | USB_RECIP_INTERFACE,
				0, iface_no, max_datagram_size,
				2, 1000);
		if (err < 0) {
			pr_debug("GET_MAX_DATAGRAM_SIZE failed, use size=%u\n",
				 min_dgram_size);
		} else {
			ctx->max_datagram_size =
				le16_to_cpu(*max_datagram_size);
			/* Check Eth descriptor value */
			if (ctx->max_datagram_size > eth_max_sz)
					ctx->max_datagram_size = eth_max_sz;

			if (ctx->max_datagram_size > CDC_NCM_MAX_DATAGRAM_SIZE)
				ctx->max_datagram_size = CDC_NCM_MAX_DATAGRAM_SIZE;

			if (ctx->max_datagram_size < min_dgram_size)
				ctx->max_datagram_size = min_dgram_size;

			/* if value changed, update device */
			if (ctx->max_datagram_size !=
					le16_to_cpu(*max_datagram_size)) {
				err = usb_control_msg(ctx->udev,
						usb_sndctrlpipe(ctx->udev, 0),
						USB_CDC_SET_MAX_DATAGRAM_SIZE,
						USB_TYPE_CLASS | USB_DIR_OUT
						 | USB_RECIP_INTERFACE,
						0,
						iface_no, max_datagram_size,
						2, 1000);
				if (err < 0)
					pr_debug("SET_MAX_DGRAM_SIZE failed\n");
			}
		}
		kfree(max_datagram_size);
	}

max_dgram_err:
	if (ctx->netdev->mtu != (ctx->max_datagram_size - eth_hlen))
		ctx->netdev->mtu = ctx->max_datagram_size - eth_hlen;

	return 0;
}

static void
cdc_ncm_find_endpoints(struct cdc_ncm_ctx *ctx, struct usb_interface *intf)
{
	struct usb_host_endpoint *e;
	u8 ep;

	for (ep = 0; ep < intf->cur_altsetting->desc.bNumEndpoints; ep++) {

		e = intf->cur_altsetting->endpoint + ep;
		switch (e->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
		case USB_ENDPOINT_XFER_INT:
			if (usb_endpoint_dir_in(&e->desc)) {
				if (ctx->status_ep == NULL)
					ctx->status_ep = e;
			}
			break;

		case USB_ENDPOINT_XFER_BULK:
			if (usb_endpoint_dir_in(&e->desc)) {
				if (ctx->in_ep == NULL)
					ctx->in_ep = e;
			} else {
				if (ctx->out_ep == NULL)
					ctx->out_ep = e;
			}
			break;

		default:
			break;
		}
	}
}

static void cdc_ncm_free(struct cdc_ncm_ctx *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->tx_rem_skb != NULL) {
		dev_kfree_skb_any(ctx->tx_rem_skb);
		ctx->tx_rem_skb = NULL;
	}

	if (ctx->tx_curr_skb != NULL) {
		dev_kfree_skb_any(ctx->tx_curr_skb);
		ctx->tx_curr_skb = NULL;
	}

	kfree(ctx);
}

static const struct ethtool_ops cdc_ncm_ethtool_ops = {
	.get_drvinfo = cdc_ncm_get_drvinfo,
#ifdef	HAVE_MII
	.get_settings = usbnet_get_settings,
	.set_settings = usbnet_set_settings,
	.nway_reset = usbnet_nway_reset,
	.get_link = usbnet_get_link,
#endif
	.get_msglevel = usbnet_get_msglevel,
	.set_msglevel = usbnet_set_msglevel,
};

int cdc_ncm_bind_common(struct usbnet *dev, struct usb_interface *intf, u8 data_altsetting)
{
	struct cdc_ncm_ctx *ctx;
	struct usb_driver *driver;
	u8 *buf;
	int len;
	int temp;
	u8 iface_no;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	hrtimer_init(&ctx->tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ctx->tx_timer.function = &cdc_ncm_tx_timer_cb;
	ctx->bh.data = (unsigned long)ctx;
	ctx->bh.func = cdc_ncm_txpath_bh;
	atomic_set(&ctx->stop, 0);
	spin_lock_init(&ctx->mtx);
	ctx->netdev = dev->net;

	/* store ctx pointer in device data field */
	dev->data[0] = (unsigned long)ctx;

	/* get some pointers */
	driver = driver_of(intf);
	buf = intf->cur_altsetting->extra;
	len = intf->cur_altsetting->extralen;

	ctx->udev = dev->udev;
	ctx->intf = intf;

	/* parse through descriptors associated with control interface */
	while ((len > 0) && (buf[0] > 2) && (buf[0] <= len)) {

		if (buf[1] != USB_DT_CS_INTERFACE)
			goto advance;

		switch (buf[2]) {
		case USB_CDC_UNION_TYPE:
			if (buf[0] < sizeof(*(ctx->union_desc)))
				break;

			ctx->union_desc =
					(const struct usb_cdc_union_desc *)buf;

			ctx->control = usb_ifnum_to_if(dev->udev,
					ctx->union_desc->bMasterInterface0);
			ctx->data = usb_ifnum_to_if(dev->udev,
					ctx->union_desc->bSlaveInterface0);
			break;

		case USB_CDC_ETHERNET_TYPE:
			if (buf[0] < sizeof(*(ctx->ether_desc)))
				break;

			ctx->ether_desc =
					(const struct usb_cdc_ether_desc *)buf;
			dev->hard_mtu =
				le16_to_cpu(ctx->ether_desc->wMaxSegmentSize);

			if (dev->hard_mtu < CDC_NCM_MIN_DATAGRAM_SIZE)
				dev->hard_mtu =	CDC_NCM_MIN_DATAGRAM_SIZE;
			else if (dev->hard_mtu > CDC_NCM_MAX_DATAGRAM_SIZE)
				dev->hard_mtu =	CDC_NCM_MAX_DATAGRAM_SIZE;
			break;

		case USB_CDC_NCM_TYPE:
			if (buf[0] < sizeof(*(ctx->func_desc)))
				break;

			ctx->func_desc = (const struct usb_cdc_ncm_desc *)buf;
			break;

		case USB_CDC_MBIM_TYPE:
			if (buf[0] < sizeof(*(ctx->mbim_desc)))
				break;

			ctx->mbim_desc = (const struct usb_cdc_mbim_desc *)buf;
			break;

		default:
			break;
		}
advance:
		/* advance to next descriptor */
		temp = buf[0];
		buf += temp;
		len -= temp;
	}

	/* some buggy devices have an IAD but no CDC Union */
	if (!ctx->union_desc && intf->intf_assoc && intf->intf_assoc->bInterfaceCount == 2) {
		ctx->control = intf;
		ctx->data = usb_ifnum_to_if(dev->udev, intf->cur_altsetting->desc.bInterfaceNumber + 1);
		dev_dbg(&intf->dev, "CDC Union missing - got slave from IAD\n");
	}

	/* check if we got everything */
	if ((ctx->control == NULL) || (ctx->data == NULL) ||
	    ((!ctx->mbim_desc) && ((ctx->ether_desc == NULL) || (ctx->control != intf))))
		goto error;

	/* claim data interface, if different from control */
	if (ctx->data != ctx->control) {
		temp = usb_driver_claim_interface(driver, ctx->data, dev);
		if (temp)
			goto error;
	}

	iface_no = ctx->data->cur_altsetting->desc.bInterfaceNumber;

	/* reset data interface */
	temp = usb_set_interface(dev->udev, iface_no, 0);
	if (temp)
		goto error2;

	/* initialize data interface */
	if (cdc_ncm_setup(ctx))
		goto error2;

	/* configure data interface */
	temp = usb_set_interface(dev->udev, iface_no, data_altsetting);
	if (temp)
		goto error2;

	cdc_ncm_find_endpoints(ctx, ctx->data);
	cdc_ncm_find_endpoints(ctx, ctx->control);

	if ((ctx->in_ep == NULL) || (ctx->out_ep == NULL) ||
	    (ctx->status_ep == NULL))
		goto error2;

	dev->net->ethtool_ops = &cdc_ncm_ethtool_ops;

	usb_set_intfdata(ctx->data, dev);
	usb_set_intfdata(ctx->control, dev);
	usb_set_intfdata(ctx->intf, dev);

	if (ctx->ether_desc) {
		temp = usbnet_get_ethernet_addr(dev, ctx->ether_desc->iMACAddress);
		if (temp)
			goto error2;
		dev_info(&dev->udev->dev, "MAC-Address: %pM\n", dev->net->dev_addr);
	}


	dev->in = usb_rcvbulkpipe(dev->udev,
		ctx->in_ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->out = usb_sndbulkpipe(dev->udev,
		ctx->out_ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->status = ctx->status_ep;
	dev->rx_urb_size = ctx->rx_max;

	ctx->tx_speed = ctx->rx_speed = 0;
	return 0;

error2:
	usb_set_intfdata(ctx->control, NULL);
	usb_set_intfdata(ctx->data, NULL);
	if (ctx->data != ctx->control)
		usb_driver_release_interface(driver, ctx->data);
error:
	cdc_ncm_free((struct cdc_ncm_ctx *)dev->data[0]);
	dev->data[0] = 0;
	dev_info(&dev->udev->dev, "bind() failure\n");
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(cdc_ncm_bind_common);

void cdc_ncm_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	struct cdc_ncm_ctx *ctx = (struct cdc_ncm_ctx *)dev->data[0];
	struct usb_driver *driver = driver_of(intf);

	if (ctx == NULL)
		return;		/* no setup */

	atomic_set(&ctx->stop, 1);

	if (hrtimer_active(&ctx->tx_timer))
		hrtimer_cancel(&ctx->tx_timer);

	tasklet_kill(&ctx->bh);

	/* handle devices with combined control and data interface */
	if (ctx->control == ctx->data)
		ctx->data = NULL;

	/* disconnect master --> disconnect slave */
	if (intf == ctx->control && ctx->data) {
		usb_set_intfdata(ctx->data, NULL);
		usb_driver_release_interface(driver, ctx->data);
		ctx->data = NULL;

	} else if (intf == ctx->data && ctx->control) {
		usb_set_intfdata(ctx->control, NULL);
		usb_driver_release_interface(driver, ctx->control);
		ctx->control = NULL;
	}

	usb_set_intfdata(ctx->intf, NULL);
	cdc_ncm_free(ctx);
}
EXPORT_SYMBOL_GPL(cdc_ncm_unbind);

/* Select the MBIM altsetting iff it is preferred and available,
 * returning the number of the corresponding data interface altsetting
 */
u8 cdc_ncm_select_altsetting(struct usbnet *dev, struct usb_interface *intf)
{
	struct usb_host_interface *alt;

	/* The MBIM spec defines a NCM compatible default altsetting,
	 * which we may have matched:
	 *
	 *  "Functions that implement both NCM 1.0 and MBIM (an
	 *   “NCM/MBIM function”) according to this recommendation
	 *   shall provide two alternate settings for the
	 *   Communication Interface.  Alternate setting 0, and the
	 *   associated class and endpoint descriptors, shall be
	 *   constructed according to the rules given for the
	 *   Communication Interface in section 5 of [USBNCM10].
	 *   Alternate setting 1, and the associated class and
	 *   endpoint descriptors, shall be constructed according to
	 *   the rules given in section 6 (USB Device Model) of this
	 *   specification."
	 */
	if (prefer_mbim && intf->num_altsetting == 2) {
		alt = usb_altnum_to_altsetting(intf, CDC_NCM_COMM_ALTSETTING_MBIM);
		if (alt && cdc_ncm_comm_intf_is_mbim(alt) &&
		    !usb_set_interface(dev->udev,
				       intf->cur_altsetting->desc.bInterfaceNumber,
				       CDC_NCM_COMM_ALTSETTING_MBIM))
			return CDC_NCM_DATA_ALTSETTING_MBIM;
	}
	return CDC_NCM_DATA_ALTSETTING_NCM;
}
EXPORT_SYMBOL_GPL(cdc_ncm_select_altsetting);

static int cdc_ncm_bind(struct usbnet *dev, struct usb_interface *intf)
{
	int ret;

	/* MBIM backwards compatible function? */
	cdc_ncm_select_altsetting(dev, intf);
	if (cdc_ncm_comm_intf_is_mbim(intf->cur_altsetting))
		return -ENODEV;

	/* NCM data altsetting is always 1 */
	ret = cdc_ncm_bind_common(dev, intf, 1);

	/*
	 * We should get an event when network connection is "connected" or
	 * "disconnected". Set network connection in "disconnected" state
	 * (carrier is OFF) during attach, so the IP network stack does not
	 * start IPv6 negotiation and more.
	 */
	netif_carrier_off(dev->net);
	return ret;
}

static void cdc_ncm_align_tail(struct sk_buff *skb, size_t modulus, size_t remainder, size_t max)
{
	size_t align = ALIGN(skb->len, modulus) - skb->len + remainder;

	if (skb->len + align > max)
		align = max - skb->len;
	if (align && skb_tailroom(skb) >= align)
		memset(skb_put(skb, align), 0, align);
}

/* return a pointer to a valid struct usb_cdc_ncm_ndp16 of type sign, possibly
 * allocating a new one within skb
 */
static struct usb_cdc_ncm_ndp16 *cdc_ncm_ndp(struct cdc_ncm_ctx *ctx, struct sk_buff *skb, __le32 sign, size_t reserve)
{
	struct usb_cdc_ncm_ndp16 *ndp16 = NULL;
	struct usb_cdc_ncm_nth16 *nth16 = (void *)skb->data;
	size_t ndpoffset = le16_to_cpu(nth16->wNdpIndex);

	/* follow the chain of NDPs, looking for a match */
	while (ndpoffset) {
		ndp16 = (struct usb_cdc_ncm_ndp16 *)(skb->data + ndpoffset);
		if  (ndp16->dwSignature == sign)
			return ndp16;
		ndpoffset = le16_to_cpu(ndp16->wNextNdpIndex);
	}

	/* align new NDP */
	cdc_ncm_align_tail(skb, ctx->tx_ndp_modulus, 0, ctx->tx_max);

	/* verify that there is room for the NDP and the datagram (reserve) */
	if ((ctx->tx_max - skb->len - reserve) < CDC_NCM_NDP_SIZE)
		return NULL;

	/* link to it */
	if (ndp16)
		ndp16->wNextNdpIndex = cpu_to_le16(skb->len);
	else
		nth16->wNdpIndex = cpu_to_le16(skb->len);

	/* push a new empty NDP */
	ndp16 = (struct usb_cdc_ncm_ndp16 *)memset(skb_put(skb, CDC_NCM_NDP_SIZE), 0, CDC_NCM_NDP_SIZE);
	ndp16->dwSignature = sign;
	ndp16->wLength = cpu_to_le16(sizeof(struct usb_cdc_ncm_ndp16) + sizeof(struct usb_cdc_ncm_dpe16));
	return ndp16;
}

struct sk_buff *
cdc_ncm_fill_tx_frame(struct cdc_ncm_ctx *ctx, struct sk_buff *skb, __le32 sign)
{
	struct usb_cdc_ncm_nth16 *nth16;
	struct usb_cdc_ncm_ndp16 *ndp16;
	struct sk_buff *skb_out;
	u16 n = 0, index, ndplen;
	u8 ready2send = 0;

	/* if there is a remaining skb, it gets priority */
	if (skb != NULL) {
		swap(skb, ctx->tx_rem_skb);
		swap(sign, ctx->tx_rem_sign);
	} else {
		ready2send = 1;
	}

	/* check if we are resuming an OUT skb */
	skb_out = ctx->tx_curr_skb;

	/* allocate a new OUT skb */
	if (!skb_out) {
		skb_out = alloc_skb((ctx->tx_max + 1), GFP_ATOMIC);
		if (skb_out == NULL) {
			if (skb != NULL) {
				dev_kfree_skb_any(skb);
				ctx->netdev->stats.tx_dropped++;
			}
			goto exit_no_skb;
		}
		/* fill out the initial 16-bit NTB header */
		nth16 = (struct usb_cdc_ncm_nth16 *)memset(skb_put(skb_out, sizeof(struct usb_cdc_ncm_nth16)), 0, sizeof(struct usb_cdc_ncm_nth16));
		nth16->dwSignature = cpu_to_le32(USB_CDC_NCM_NTH16_SIGN);
		nth16->wHeaderLength = cpu_to_le16(sizeof(struct usb_cdc_ncm_nth16));
		nth16->wSequence = cpu_to_le16(ctx->tx_seq++);

		/* count total number of frames in this NTB */
		ctx->tx_curr_frame_num = 0;
	}

	for (n = ctx->tx_curr_frame_num; n < ctx->tx_max_datagrams; n++) {
		/* send any remaining skb first */
		if (skb == NULL) {
			skb = ctx->tx_rem_skb;
			sign = ctx->tx_rem_sign;
			ctx->tx_rem_skb = NULL;

			/* check for end of skb */
			if (skb == NULL)
				break;
		}

		/* get the appropriate NDP for this skb */
		ndp16 = cdc_ncm_ndp(ctx, skb_out, sign, skb->len + ctx->tx_modulus + ctx->tx_remainder);

		/* align beginning of next frame */
		cdc_ncm_align_tail(skb_out,  ctx->tx_modulus, ctx->tx_remainder, ctx->tx_max);

		/* check if we had enough room left for both NDP and frame */
		if (!ndp16 || skb_out->len + skb->len > ctx->tx_max) {
			if (n == 0) {
				/* won't fit, MTU problem? */
				dev_kfree_skb_any(skb);
				skb = NULL;
				ctx->netdev->stats.tx_dropped++;
			} else {
				/* no room for skb - store for later */
				if (ctx->tx_rem_skb != NULL) {
					dev_kfree_skb_any(ctx->tx_rem_skb);
					ctx->netdev->stats.tx_dropped++;
				}
				ctx->tx_rem_skb = skb;
				ctx->tx_rem_sign = sign;
				skb = NULL;
				ready2send = 1;
			}
			break;
		}

		/* calculate frame number withing this NDP */
		ndplen = le16_to_cpu(ndp16->wLength);
		index = (ndplen - sizeof(struct usb_cdc_ncm_ndp16)) / sizeof(struct usb_cdc_ncm_dpe16) - 1;

		/* OK, add this skb */
		ndp16->dpe16[index].wDatagramLength = cpu_to_le16(skb->len);
		ndp16->dpe16[index].wDatagramIndex = cpu_to_le16(skb_out->len);
		ndp16->wLength = cpu_to_le16(ndplen + sizeof(struct usb_cdc_ncm_dpe16));
		memcpy(skb_put(skb_out, skb->len), skb->data, skb->len);
		dev_kfree_skb_any(skb);
		skb = NULL;

		/* send now if this NDP is full */
		if (index >= CDC_NCM_DPT_DATAGRAMS_MAX) {
			ready2send = 1;
			break;
		}
	}

	/* free up any dangling skb */
	if (skb != NULL) {
		dev_kfree_skb_any(skb);
		skb = NULL;
		ctx->netdev->stats.tx_dropped++;
	}

	ctx->tx_curr_frame_num = n;

	if (n == 0) {
		/* wait for more frames */
		/* push variables */
		ctx->tx_curr_skb = skb_out;
		goto exit_no_skb;

	} else if ((n < ctx->tx_max_datagrams) && (ready2send == 0)) {
		/* wait for more frames */
		/* push variables */
		ctx->tx_curr_skb = skb_out;
		/* set the pending count */
		if (n < CDC_NCM_RESTART_TIMER_DATAGRAM_CNT)
			ctx->tx_timer_pending = CDC_NCM_TIMER_PENDING_CNT;
		goto exit_no_skb;

	} else {
		/* frame goes out */
		/* variables will be reset at next call */
	}

	/*
	 * If collected data size is less or equal CDC_NCM_MIN_TX_PKT bytes,
	 * we send buffers as it is. If we get more data, it would be more
	 * efficient for USB HS mobile device with DMA engine to receive a full
	 * size NTB, than canceling DMA transfer and receiving a short packet.
	 */
	if (skb_out->len > CDC_NCM_MIN_TX_PKT)
		/* final zero padding */
		memset(skb_put(skb_out, ctx->tx_max - skb_out->len), 0, ctx->tx_max - skb_out->len);

	/* do we need to prevent a ZLP? */
	if (((skb_out->len % le16_to_cpu(ctx->out_ep->desc.wMaxPacketSize)) == 0) &&
	    (skb_out->len < le32_to_cpu(ctx->ncm_parm.dwNtbOutMaxSize)) && skb_tailroom(skb_out))
		*skb_put(skb_out, 1) = 0;	/* force short packet */

	/* set final frame length */
	nth16 = (struct usb_cdc_ncm_nth16 *)skb_out->data;
	nth16->wBlockLength = cpu_to_le16(skb_out->len);

	/* return skb */
	ctx->tx_curr_skb = NULL;
	ctx->netdev->stats.tx_packets += ctx->tx_curr_frame_num;
	return skb_out;

exit_no_skb:
	/* Start timer, if there is a remaining skb */
	if (ctx->tx_curr_skb != NULL)
		cdc_ncm_tx_timeout_start(ctx);
	return NULL;
}
EXPORT_SYMBOL_GPL(cdc_ncm_fill_tx_frame);

static void cdc_ncm_tx_timeout_start(struct cdc_ncm_ctx *ctx)
{
	/* start timer, if not already started */
	if (!(hrtimer_active(&ctx->tx_timer) || atomic_read(&ctx->stop)))
		hrtimer_start(&ctx->tx_timer,
				ktime_set(0, CDC_NCM_TIMER_INTERVAL),
				HRTIMER_MODE_REL);
}

static enum hrtimer_restart cdc_ncm_tx_timer_cb(struct hrtimer *timer)
{
	struct cdc_ncm_ctx *ctx =
			container_of(timer, struct cdc_ncm_ctx, tx_timer);

	if (!atomic_read(&ctx->stop))
		tasklet_schedule(&ctx->bh);
	return HRTIMER_NORESTART;
}

static void cdc_ncm_txpath_bh(unsigned long param)
{
	struct cdc_ncm_ctx *ctx = (struct cdc_ncm_ctx *)param;

	spin_lock_bh(&ctx->mtx);
	if (ctx->tx_timer_pending != 0) {
		ctx->tx_timer_pending--;
		cdc_ncm_tx_timeout_start(ctx);
		spin_unlock_bh(&ctx->mtx);
	} else if (ctx->netdev != NULL) {
		spin_unlock_bh(&ctx->mtx);
		netif_tx_lock_bh(ctx->netdev);
		usbnet_start_xmit(NULL, ctx->netdev);
		netif_tx_unlock_bh(ctx->netdev);
	} else {
		spin_unlock_bh(&ctx->mtx);
	}
}

static struct sk_buff *
cdc_ncm_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags)
{
	struct sk_buff *skb_out;
	struct cdc_ncm_ctx *ctx = (struct cdc_ncm_ctx *)dev->data[0];

	/*
	 * The Ethernet API we are using does not support transmitting
	 * multiple Ethernet frames in a single call. This driver will
	 * accumulate multiple Ethernet frames and send out a larger
	 * USB frame when the USB buffer is full or when a single jiffies
	 * timeout happens.
	 */
	if (ctx == NULL)
		goto error;

	spin_lock_bh(&ctx->mtx);
	skb_out = cdc_ncm_fill_tx_frame(ctx, skb, cpu_to_le32(USB_CDC_NCM_NDP16_NOCRC_SIGN));
	spin_unlock_bh(&ctx->mtx);
	return skb_out;

error:
	if (skb != NULL)
		dev_kfree_skb_any(skb);

	return NULL;
}

/* verify NTB header and return offset of first NDP, or negative error */
int cdc_ncm_rx_verify_nth16(struct cdc_ncm_ctx *ctx, struct sk_buff *skb_in)
{
	struct usb_cdc_ncm_nth16 *nth16;
	int len;
	int ret = -EINVAL;

	if (ctx == NULL)
		goto error;

	if (skb_in->len < (sizeof(struct usb_cdc_ncm_nth16) +
					sizeof(struct usb_cdc_ncm_ndp16))) {
		pr_debug("frame too short\n");
		goto error;
	}

	nth16 = (struct usb_cdc_ncm_nth16 *)skb_in->data;

	if (le32_to_cpu(nth16->dwSignature) != USB_CDC_NCM_NTH16_SIGN) {
		pr_debug("invalid NTH16 signature <%u>\n",
					le32_to_cpu(nth16->dwSignature));
		goto error;
	}

	len = le16_to_cpu(nth16->wBlockLength);
	if (len > ctx->rx_max) {
		pr_debug("unsupported NTB block length %u/%u\n", len,
								ctx->rx_max);
		goto error;
	}

	if ((ctx->rx_seq + 1) != le16_to_cpu(nth16->wSequence) &&
		(ctx->rx_seq || le16_to_cpu(nth16->wSequence)) &&
		!((ctx->rx_seq == 0xffff) && !le16_to_cpu(nth16->wSequence))) {
		pr_debug("sequence number glitch prev=%d curr=%d\n",
				ctx->rx_seq, le16_to_cpu(nth16->wSequence));
	}
	ctx->rx_seq = le16_to_cpu(nth16->wSequence);

	ret = le16_to_cpu(nth16->wNdpIndex);
error:
	return ret;
}
EXPORT_SYMBOL_GPL(cdc_ncm_rx_verify_nth16);

/* verify NDP header and return number of datagrams, or negative error */
int cdc_ncm_rx_verify_ndp16(struct sk_buff *skb_in, int ndpoffset)
{
	struct usb_cdc_ncm_ndp16 *ndp16;
	int ret = -EINVAL;

	if ((ndpoffset + sizeof(struct usb_cdc_ncm_ndp16)) > skb_in->len) {
		pr_debug("invalid NDP offset  <%u>\n", ndpoffset);
		goto error;
	}
	ndp16 = (struct usb_cdc_ncm_ndp16 *)(skb_in->data + ndpoffset);

	if (le16_to_cpu(ndp16->wLength) < USB_CDC_NCM_NDP16_LENGTH_MIN) {
		pr_debug("invalid DPT16 length <%u>\n",
					le32_to_cpu(ndp16->dwSignature));
		goto error;
	}

	ret = ((le16_to_cpu(ndp16->wLength) -
					sizeof(struct usb_cdc_ncm_ndp16)) /
					sizeof(struct usb_cdc_ncm_dpe16));
	ret--; /* we process NDP entries except for the last one */

	if ((sizeof(struct usb_cdc_ncm_ndp16) + ret * (sizeof(struct usb_cdc_ncm_dpe16))) >
								skb_in->len) {
		pr_debug("Invalid nframes = %d\n", ret);
		ret = -EINVAL;
	}

error:
	return ret;
}
EXPORT_SYMBOL_GPL(cdc_ncm_rx_verify_ndp16);

static int cdc_ncm_rx_fixup(struct usbnet *dev, struct sk_buff *skb_in)
{
	struct sk_buff *skb;
	struct cdc_ncm_ctx *ctx = (struct cdc_ncm_ctx *)dev->data[0];
	int len;
	int nframes;
	int x;
	int offset;
	struct usb_cdc_ncm_ndp16 *ndp16;
	struct usb_cdc_ncm_dpe16 *dpe16;
	int ndpoffset;
	int loopcount = 50; /* arbitrary max preventing infinite loop */

	ndpoffset = cdc_ncm_rx_verify_nth16(ctx, skb_in);
	if (ndpoffset < 0)
		goto error;

next_ndp:
	nframes = cdc_ncm_rx_verify_ndp16(skb_in, ndpoffset);
	if (nframes < 0)
		goto error;

	ndp16 = (struct usb_cdc_ncm_ndp16 *)(skb_in->data + ndpoffset);

	if (le32_to_cpu(ndp16->dwSignature) != USB_CDC_NCM_NDP16_NOCRC_SIGN) {
		pr_debug("invalid DPT16 signature <%u>\n",
			 le32_to_cpu(ndp16->dwSignature));
		goto err_ndp;
	}
	dpe16 = ndp16->dpe16;

	for (x = 0; x < nframes; x++, dpe16++) {
		offset = le16_to_cpu(dpe16->wDatagramIndex);
		len = le16_to_cpu(dpe16->wDatagramLength);

		/*
		 * CDC NCM ch. 3.7
		 * All entries after first NULL entry are to be ignored
		 */
		if ((offset == 0) || (len == 0)) {
			if (!x)
				goto err_ndp; /* empty NTB */
			break;
		}

		/* sanity checking */
		if (((offset + len) > skb_in->len) ||
				(len > ctx->rx_max) || (len < ETH_HLEN)) {
			pr_debug("invalid frame detected (ignored)"
					"offset[%u]=%u, length=%u, skb=%p\n",
					x, offset, len, skb_in);
			if (!x)
				goto err_ndp;
			break;

		} else {
			skb = skb_clone(skb_in, GFP_ATOMIC);
			if (!skb)
				goto error;
			skb->len = len;
			skb->data = ((u8 *)skb_in->data) + offset;
			skb_set_tail_pointer(skb, len);
			usbnet_skb_return(dev, skb);
		}
	}
err_ndp:
	/* are there more NDPs to process? */
	ndpoffset = le16_to_cpu(ndp16->wNextNdpIndex);
	if (ndpoffset && loopcount--)
		goto next_ndp;

	return 1;
error:
	return 0;
}

static void
cdc_ncm_speed_change(struct cdc_ncm_ctx *ctx,
		     struct usb_cdc_speed_change *data)
{
	uint32_t rx_speed = le32_to_cpu(data->DLBitRRate);
	uint32_t tx_speed = le32_to_cpu(data->ULBitRate);

	/*
	 * Currently the USB-NET API does not support reporting the actual
	 * device speed. Do print it instead.
	 */
	if ((tx_speed != ctx->tx_speed) || (rx_speed != ctx->rx_speed)) {
		ctx->tx_speed = tx_speed;
		ctx->rx_speed = rx_speed;

		if ((tx_speed > 1000000) && (rx_speed > 1000000)) {
			printk(KERN_INFO KBUILD_MODNAME
				": %s: %u mbit/s downlink "
				"%u mbit/s uplink\n",
				ctx->netdev->name,
				(unsigned int)(rx_speed / 1000000U),
				(unsigned int)(tx_speed / 1000000U));
		} else {
			printk(KERN_INFO KBUILD_MODNAME
				": %s: %u kbit/s downlink "
				"%u kbit/s uplink\n",
				ctx->netdev->name,
				(unsigned int)(rx_speed / 1000U),
				(unsigned int)(tx_speed / 1000U));
		}
	}
}

static void cdc_ncm_status(struct usbnet *dev, struct urb *urb)
{
	struct cdc_ncm_ctx *ctx;
	struct usb_cdc_notification *event;

	ctx = (struct cdc_ncm_ctx *)dev->data[0];

	if (urb->actual_length < sizeof(*event))
		return;

	/* test for split data in 8-byte chunks */
	if (test_and_clear_bit(EVENT_STS_SPLIT, &dev->flags)) {
		cdc_ncm_speed_change(ctx,
		      (struct usb_cdc_speed_change *)urb->transfer_buffer);
		return;
	}

	event = urb->transfer_buffer;

	switch (event->bNotificationType) {
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
		/*
		 * According to the CDC NCM specification ch.7.1
		 * USB_CDC_NOTIFY_NETWORK_CONNECTION notification shall be
		 * sent by device after USB_CDC_NOTIFY_SPEED_CHANGE.
		 */
		ctx->connected = le16_to_cpu(event->wValue);

		printk(KERN_INFO KBUILD_MODNAME ": %s: network connection:"
			" %sconnected\n",
			ctx->netdev->name, ctx->connected ? "" : "dis");

		if (ctx->connected)
			netif_carrier_on(dev->net);
		else {
			netif_carrier_off(dev->net);
			ctx->tx_speed = ctx->rx_speed = 0;
		}
		break;

	case USB_CDC_NOTIFY_RESPONSE_AVAILABLE:
		/* Not implemented yet */
		dev_dbg(&dev->udev->dev, "NCM: ResponseAvailable - Value=%d, Index=%d\n",
			le16_to_cpu(event->wValue), le16_to_cpu(event->wIndex));
		break;

	case USB_CDC_NOTIFY_SPEED_CHANGE:
		if (urb->actual_length < (sizeof(*event) +
					sizeof(struct usb_cdc_speed_change)))
			set_bit(EVENT_STS_SPLIT, &dev->flags);
		else
			cdc_ncm_speed_change(ctx,
				(struct usb_cdc_speed_change *) &event[1]);
		break;

	default:
		dev_err(&dev->udev->dev, "NCM: unexpected "
			"notification 0x%02x!\n", event->bNotificationType);
		break;
	}
}

static int cdc_ncm_check_connect(struct usbnet *dev)
{
	struct cdc_ncm_ctx *ctx;

	ctx = (struct cdc_ncm_ctx *)dev->data[0];
	if (ctx == NULL)
		return 1;	/* disconnected */

	return !ctx->connected;
}

static int
cdc_ncm_probe(struct usb_interface *udev, const struct usb_device_id *prod)
{
	return usbnet_probe(udev, prod);
}

static void cdc_ncm_disconnect(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);

	if (dev == NULL)
		return;		/* already disconnected */

	usbnet_disconnect(intf);
}

#if 0
static int cdc_ncm_manage_power(struct usbnet *dev, int status)
{
	dev->intf->needs_remote_wakeup = status;
	return 0;
}
#endif

static const struct driver_info cdc_ncm_info = {
	.description = "CDC NCM",
	.flags = FLAG_NO_SETINT | FLAG_MULTI_PACKET,
	.bind = cdc_ncm_bind,
	.unbind = cdc_ncm_unbind,
	.check_connect = cdc_ncm_check_connect,
//	.manage_power = cdc_ncm_manage_power,
	.status = cdc_ncm_status,
	.rx_fixup = cdc_ncm_rx_fixup,
	.tx_fixup = cdc_ncm_tx_fixup,
};

/* Same as cdc_ncm_info, but with FLAG_WWAN */
static const struct driver_info wwan_info = {
	.description = "Mobile Broadband Network Device",
	.flags = FLAG_NO_SETINT | FLAG_MULTI_PACKET,
	.bind = cdc_ncm_bind,
	.unbind = cdc_ncm_unbind,
	.check_connect = cdc_ncm_check_connect,
//	.manage_power = cdc_ncm_manage_power,
	.status = cdc_ncm_status,
	.rx_fixup = cdc_ncm_rx_fixup,
	.tx_fixup = cdc_ncm_tx_fixup,
};

/* Same as wwan_info, but with FLAG_NOARP  */
static const struct driver_info wwan_noarp_info = {
	.description = "Mobile Broadband Network Device (NO ARP)",
	.flags = FLAG_NO_SETINT | FLAG_MULTI_PACKET | FLAG_NOARP,
	.bind = cdc_ncm_bind,
	.unbind = cdc_ncm_unbind,
	.check_connect = cdc_ncm_check_connect,
//	.manage_power = cdc_ncm_manage_power,
	.status = cdc_ncm_status,
	.rx_fixup = cdc_ncm_rx_fixup,
	.tx_fixup = cdc_ncm_tx_fixup,
};

static const struct usb_device_id cdc_devs[] = {
	/* Ericsson MBM devices like F5521gw */
	{ .match_flags = USB_DEVICE_ID_MATCH_INT_INFO
		| USB_DEVICE_ID_MATCH_VENDOR,
	  .idVendor = 0x0bdb,
	  .bInterfaceClass = USB_CLASS_COMM,
	  .bInterfaceSubClass = USB_CDC_SUBCLASS_NCM,
	  .bInterfaceProtocol = USB_CDC_PROTO_NONE,
	  .driver_info = (unsigned long) &wwan_info,
	},

	/* Dell branded MBM devices like DW5550 */
	{ .match_flags = USB_DEVICE_ID_MATCH_INT_INFO
		| USB_DEVICE_ID_MATCH_VENDOR,
	  .idVendor = 0x413c,
	  .bInterfaceClass = USB_CLASS_COMM,
	  .bInterfaceSubClass = USB_CDC_SUBCLASS_NCM,
	  .bInterfaceProtocol = USB_CDC_PROTO_NONE,
	  .driver_info = (unsigned long) &wwan_info,
	},

	/* Toshiba branded MBM devices */
	{ .match_flags = USB_DEVICE_ID_MATCH_INT_INFO
		| USB_DEVICE_ID_MATCH_VENDOR,
	  .idVendor = 0x0930,
	  .bInterfaceClass = USB_CLASS_COMM,
	  .bInterfaceSubClass = USB_CDC_SUBCLASS_NCM,
	  .bInterfaceProtocol = USB_CDC_PROTO_NONE,
	  .driver_info = (unsigned long) &wwan_info,
	},

	/* tag Huawei devices as wwan */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x12d1,
					USB_CLASS_COMM,
					USB_CDC_SUBCLASS_NCM,
					USB_CDC_PROTO_NONE),
	  .driver_info = (unsigned long)&wwan_info,
	},

	/* Infineon(now Intel) HSPA Modem platform */
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1519, 0x0443,
		USB_CLASS_COMM,
		USB_CDC_SUBCLASS_NCM, USB_CDC_PROTO_NONE),
	  .driver_info = (unsigned long)&wwan_noarp_info,
	},

	/* Generic CDC-NCM devices */
	{ USB_INTERFACE_INFO(USB_CLASS_COMM,
		USB_CDC_SUBCLASS_NCM, USB_CDC_PROTO_NONE),
		.driver_info = (unsigned long)&cdc_ncm_info,
	},
	{
	},
};
MODULE_DEVICE_TABLE(usb, cdc_devs);

static struct usb_driver cdc_ncm_driver = {
	.name = "cdc_ncm",
	.id_table = cdc_devs,
	.probe = cdc_ncm_probe,
	.disconnect = cdc_ncm_disconnect,
	.suspend = usbnet_suspend,
	.resume = usbnet_resume,
	.reset_resume =	usbnet_resume,
	.supports_autosuspend = 1,
};

static int __init cdc_ncm_init(void)
{
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION "\n");
	return usb_register(&cdc_ncm_driver);
}

module_init(cdc_ncm_init);

static void __exit cdc_ncm_exit(void)
{
	usb_deregister(&cdc_ncm_driver);
}

module_exit(cdc_ncm_exit);

MODULE_AUTHOR("Hans Petter Selasky");
MODULE_DESCRIPTION("USB CDC NCM host driver");
MODULE_LICENSE("Dual BSD/GPL");
