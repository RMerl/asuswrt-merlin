/*
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * Fundamental constants relating to ARP Protocol
 *
 * $Id: bcmarp.h 384540 2013-02-12 04:28:58Z $
 */

#ifndef _bcmarp_h_
#define _bcmarp_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif
#include <proto/bcmip.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>


#define ARP_OPC_OFFSET		6		/* option code offset */
#define ARP_SRC_ETH_OFFSET	8		/* src h/w address offset */
#define ARP_SRC_IP_OFFSET	14		/* src IP address offset */
#define ARP_TGT_ETH_OFFSET	18		/* target h/w address offset */
#define ARP_TGT_IP_OFFSET	24		/* target IP address offset */

#define ARP_OPC_REQUEST		1		/* ARP request */
#define ARP_OPC_REPLY		2		/* ARP reply */

#define ARP_DATA_LEN		28		/* ARP data length */

BWL_PRE_PACKED_STRUCT struct bcmarp {
	uint16	htype;				/* Header type (1 = ethernet) */
	uint16	ptype;				/* Protocol type (0x800 = IP) */
	uint8	hlen;				/* Hardware address length (Eth = 6) */
	uint8	plen;				/* Protocol address length (IP = 4) */
	uint16	oper;				/* ARP_OPC_... */
	uint8	src_eth[ETHER_ADDR_LEN];	/* Source hardware address */
	uint8	src_ip[IPV4_ADDR_LEN];		/* Source protocol address (not aligned) */
	uint8	dst_eth[ETHER_ADDR_LEN];	/* Destination hardware address */
	uint8	dst_ip[IPV4_ADDR_LEN];		/* Destination protocol address */
} BWL_POST_PACKED_STRUCT;

/* Ethernet header + Arp message */
BWL_PRE_PACKED_STRUCT struct bcmetharp {
	struct ether_header	eh;
	struct bcmarp	arp;
} BWL_POST_PACKED_STRUCT;

/* IPv6 Neighbor Advertisement */
#define NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET	8		/* src IPv6 address offset */
#define NEIGHBOR_ADVERTISE_TYPE_OFFSET		40		/* type offset */
#define NEIGHBOR_ADVERTISE_CHECKSUM_OFFSET	42		/* check sum offset */
#define NEIGHBOR_ADVERTISE_FLAGS_OFFSET		44		/* R,S and O flags offset */
#define NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET	48		/* target IPv6 address offset */
#define NEIGHBOR_ADVERTISE_OPTION_OFFSET	64		/* options offset */
#define NEIGHBOR_ADVERTISE_TYPE		136
#define NEIGHBOR_SOLICITATION_TYPE	135

#define OPT_TYPE_SRC_LINK_ADDR		1
#define OPT_TYPE_TGT_LINK_ADDR		2

#define NEIGHBOR_ADVERTISE_DATA_LEN	72	/* neighbor advertisement data length */
#define NEIGHBOR_ADVERTISE_FLAGS_VALUE	0x60	/* R=0, S=1 and O=1 */

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/* !defined(_bcmarp_h_) */
