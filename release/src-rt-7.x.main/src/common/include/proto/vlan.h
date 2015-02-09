/*
 * 802.1Q VLAN protocol definitions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: vlan.h 382883 2013-02-04 23:26:09Z $
 */

#ifndef _vlan_h_
#define _vlan_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#ifndef	 VLAN_VID_MASK
#define VLAN_VID_MASK		0xfff	/* low 12 bits are vlan id */
#endif

#define	VLAN_CFI_SHIFT		12	/* canonical format indicator bit */
#define VLAN_PRI_SHIFT		13	/* user priority */

#define VLAN_PRI_MASK		7	/* 3 bits of priority */

#define	VLAN_TPID_OFFSET	12	/* offset of tag protocol id field */
#define	VLAN_TCI_OFFSET		14	/* offset of tag ctrl info field */

#define	VLAN_TAG_LEN		4
#define	VLAN_TAG_OFFSET		(2 * ETHER_ADDR_LEN)	/* offset in Ethernet II packet only */

#define VLAN_TPID		0x8100	/* VLAN ethertype/Tag Protocol ID */

struct vlan_header {
	uint16	vlan_type;		/* 0x8100 */
	uint16	vlan_tag;		/* priority, cfi and vid */
};

struct ethervlan_header {
	uint8	ether_dhost[ETHER_ADDR_LEN];
	uint8	ether_shost[ETHER_ADDR_LEN];
	uint16	vlan_type;		/* 0x8100 */
	uint16	vlan_tag;		/* priority, cfi and vid */
	uint16	ether_type;
};

struct dot3_mac_llc_snapvlan_header {
	uint8	ether_dhost[ETHER_ADDR_LEN];	/* dest mac */
	uint8	ether_shost[ETHER_ADDR_LEN];	/* src mac */
	uint16	length;				/* frame length incl header */
	uint8	dsap;				/* always 0xAA */
	uint8	ssap;				/* always 0xAA */
	uint8	ctl;				/* always 0x03 */
	uint8	oui[3];				/* RFC1042: 0x00 0x00 0x00
						 * Bridge-Tunnel: 0x00 0x00 0xF8
						 */
	uint16	vlan_type;			/* 0x8100 */
	uint16	vlan_tag;			/* priority, cfi and vid */
	uint16	ether_type;			/* ethertype */
};

#define	ETHERVLAN_HDR_LEN	(ETHER_HDR_LEN + VLAN_TAG_LEN)


/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#define ETHERVLAN_MOVE_HDR(d, s) \
do { \
	struct ethervlan_header t; \
	t = *(struct ethervlan_header *)(s); \
	*(struct ethervlan_header *)(d) = t; \
} while (0)

#endif /* _vlan_h_ */
