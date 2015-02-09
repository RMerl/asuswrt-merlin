/*
 * Fundamental constants relating to TCP Protocol
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
 * $Id: bcmtcp.h 434054 2013-11-05 02:01:21Z $
 */

#ifndef _bcmtcp_h_
#define _bcmtcp_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>


#define TCP_SRC_PORT_OFFSET	0	/* TCP source port offset */
#define TCP_DEST_PORT_OFFSET	2	/* TCP dest port offset */
#define TCP_SEQ_NUM_OFFSET	4	/* TCP sequence number offset */
#define TCP_ACK_NUM_OFFSET	8	/* TCP acknowledgement number offset */
#define TCP_HLEN_OFFSET		12	/* HLEN and reserved bits offset */
#define TCP_FLAGS_OFFSET	13	/* FLAGS and reserved bits offset */
#define TCP_CHKSUM_OFFSET	16	/* TCP body checksum offset */

#define TCP_PORT_LEN		2	/* TCP port field length */

/* 8bit TCP flag field */
#define TCP_FLAG_URG            0x20
#define TCP_FLAG_ACK            0x10
#define TCP_FLAG_PSH            0x08
#define TCP_FLAG_RST            0x04
#define TCP_FLAG_SYN            0x02
#define TCP_FLAG_FIN            0x01

#define TCP_HLEN_MASK           0xf000
#define TCP_HLEN_SHIFT          12

/* These fields are stored in network order */
BWL_PRE_PACKED_STRUCT struct bcmtcp_hdr
{
	uint16	src_port;	/* Source Port Address */
	uint16	dst_port;	/* Destination Port Address */
	uint32	seq_num;	/* TCP Sequence Number */
	uint32	ack_num;	/* TCP Sequence Number */
	uint16	hdrlen_rsvd_flags;	/* Header length, reserved bits and flags */
	uint16	tcpwin;		/* TCP window */
	uint16	chksum;		/* Segment checksum with pseudoheader */
	uint16	urg_ptr;	/* Points to seq-num of byte following urg data */
} BWL_POST_PACKED_STRUCT;

#define TCP_MIN_HEADER_LEN 20

#define TCP_HDRLEN_MASK 0xf0
#define TCP_HDRLEN_SHIFT 4
#define TCP_HDRLEN(hdrlen) (((hdrlen) & TCP_HDRLEN_MASK) >> TCP_HDRLEN_SHIFT)

#define TCP_FLAGS_MASK  0x1f
#define TCP_FLAGS(hdrlen) ((hdrlen) & TCP_FLAGS_MASK)

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/* #ifndef _bcmtcp_h_ */
