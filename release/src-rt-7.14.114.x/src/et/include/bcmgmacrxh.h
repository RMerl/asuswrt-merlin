/*
 * Hardware-specific Receive Data Header for the
 * Broadcom Home Networking Division
 * BCM47XX GbE cores.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmgmacrxh.h 376342 2012-12-24 21:02:49Z $
 */

#ifndef _bcmgmacrxh_h_
#define	_bcmgmacrxh_h_

/*
 * The Ethernet GMAC core returns an 8-byte Receive Frame Data Header
 * with every frame consisting of
 * 16 bits of frame length, followed by
 * 16 bits of GMAC rx descriptor info, followed by 32bits of undefined.
 */
typedef volatile struct {
	uint16	len;
	uint16	flags;
	uint16	pad[12];
} bcmgmacrxh_t;

#define	RXHDR_LEN	28	/* Header length */

#define	GRXF_DT_MASK	((uint16)0xf)		/* data type */
#define	GRXF_DT_SHIFT	12
#define	GRXF_DC_MASK	((uint16)0xf)		/* (num descr to xfer the frame) - 1 */
#define	GRXF_DC_SHIFT	8
#define	GRXF_OVF	((uint16)1 << 7)	/* overflow error occured */
#define	GRXF_OVERSIZE	((uint16)1 << 4)	/* frame size > rxmaxlength */
#define	GRXF_CRC	((uint16)1 << 3)	/* crc error */
#define	GRXF_VLAN	((uint16)1 << 2)	/* vlan tag detected */
#define	GRXF_PT_MASK	((uint16)3)		/* packet type 0 - Unicast,
						 * 1 - Multicast, 2 - Broadcast
						 */

#endif	/* _bcmgmacrxh_h_ */
