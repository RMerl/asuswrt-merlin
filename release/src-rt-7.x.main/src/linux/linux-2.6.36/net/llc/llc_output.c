/*
 * llc_output.c - LLC minimal output path
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License version 2 for more details.
 */

#include <linux/if_arp.h>
#include <linux/if_tr.h>
#include <linux/netdevice.h>
#include <linux/trdevice.h>
#include <linux/skbuff.h>
#include <net/llc.h>
#include <net/llc_pdu.h>

/**
 *	llc_mac_hdr_init - fills MAC header fields
 *	@skb: Address of the frame to initialize its MAC header
 *	@sa: The MAC source address
 *	@da: The MAC destination address
 *
 *	Fills MAC header fields, depending on MAC type. Returns 0, If MAC type
 *	is a valid type and initialization completes correctly 1, otherwise.
 */
int llc_mac_hdr_init(struct sk_buff *skb,
		     const unsigned char *sa, const unsigned char *da)
{
	int rc = -EINVAL;

	switch (skb->dev->type) {
	case ARPHRD_IEEE802_TR:
	case ARPHRD_ETHER:
	case ARPHRD_LOOPBACK:
		rc = dev_hard_header(skb, skb->dev, ETH_P_802_2, da, sa,
				     skb->len);
		if (rc > 0)
			rc = 0;
		break;
	default:
		WARN(1, "device type not supported: %d\n", skb->dev->type);
	}
	return rc;
}

/**
 *	llc_build_and_send_ui_pkt - unitdata request interface for upper layers
 *	@sap: sap to use
 *	@skb: packet to send
 *	@dmac: destination mac address
 *	@dsap: destination sap
 *
 *	Upper layers calls this function when upper layer wants to send data
 *	using connection-less mode communication (UI pdu).
 *
 *	Accept data frame from network layer to be sent using connection-
 *	less mode communication; timeout/retries handled by network layer;
 *	package primitive as an event and send to SAP event handler
 */
int llc_build_and_send_ui_pkt(struct llc_sap *sap, struct sk_buff *skb,
			      unsigned char *dmac, unsigned char dsap)
{
	int rc;
	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
			    dsap, LLC_PDU_CMD);
	llc_pdu_init_as_ui_cmd(skb);
	rc = llc_mac_hdr_init(skb, skb->dev->dev_addr, dmac);
	if (likely(!rc))
		rc = dev_queue_xmit(skb);
	return rc;
}

EXPORT_SYMBOL(llc_mac_hdr_init);
EXPORT_SYMBOL(llc_build_and_send_ui_pkt);
