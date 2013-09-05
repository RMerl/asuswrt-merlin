/*
 * Flow Accelerator
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: $
 */

#ifndef _BCM_FA_H_
#define _BCM_FA_H_

#include <bcmutils.h>
#include <siutils.h>
#include <proto/bcmip.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>


/* FA arguments */
#define FA_IOV_ADD_NAPT		1
#define FA_IOV_DEL_NAPT		2
#define FA_IOV_GET_LIVE		3
#define FA_IOV_CONNTRACK	4

#ifndef IPADDR_U32_SZ
#define IPADDR_U32_SZ		(IPV6_ADDR_LEN / sizeof(uint32))
#endif

#define ETFA_NH_OP_CTAG		1
#define ETFA_NH_OP_NOTAG	2
#define ETFA_NP_INTERNAL	0
#define ETFA_NP_EXTERNAL	1

typedef struct fa_napt_ioctl	fa_napt_ioctl_t;

struct fa_napt_ioctl {
	uint32			sip[IPADDR_U32_SZ];
	uint32			dip[IPADDR_U32_SZ];
	uint16			sp;
	uint16			dp;
	uint8			proto;
	void			*txif;		/* Target interface to send */
	void			*rxif;		/* Receive interface */
	struct	ether_addr	shost;		/* Source MAC address */
	struct	ether_addr	dhost;		/* Destination MAC address */
	uint8			tag_op;
	uint8			external;
	uint8			tos;		/* IPv4 tos or IPv6 traff class excl ECN */
	uint16			vid;		/* VLAN id to use on txif */
	uint32			nat_ip[IPADDR_U32_SZ];
	uint16			nat_port;
	void			*pkt;
	bool			v6;
	bool			live;		/* out value */
};

#endif /* _BCM_FA_H_ */
