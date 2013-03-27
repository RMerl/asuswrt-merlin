/*
 * Broadcom 802.11 Message infra (pcie<-> CR4) used for RX offloads
 *
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_ol_msg.h chandrum $
 */

#ifndef _BCM_OL_MSG_H_
#define _BCM_OL_MSG_H_

#include <epivers.h>
#include <typedefs.h>
#ifdef WLRXOE
#include <ethernet.h>
#endif
#include <bcmdevs.h>
#include <proto/bcmip.h>
#include <proto/bcmipv6.h>

#define OLMSG_RW_MAX_ENTRIES	2

/* Dongle indecies */
#define OLMSG_READ_DONGLE_INDEX		0
#define OLMSG_WRITE_DONGLE_INDEX	1

/* Host indecies */
#define OLMSG_READ_HOST_INDEX		1
#define OLMSG_WRITE_HOST_INDEX		0

#define OLMSG_BUF_SZ			0x100000 /* 1MB */
#define OLMSG_MGMT_BUFF_SZ		0x100    /* 256B */
#define OLMSG_HOST_BUF_SZ		0x40000 /* 256KB */
#define OLMSG_DGL_BUF_SZ		0x40000 /* 256KB */

#define OLMSG_MGMT_BUFF_OFFSET	0
#define OLMSG_HOST_BUFF_OFFSET	256		/* Starting at 256K */
#define OLMSG_DGL_BUFF_OFFSET	0x80000 /* Starting at 512K */

/* Maximum IE id for non vendor specific IE */
#define OLMSG_BCN_MAX_IE		222
#define MAX_VNDR_IE				50 /* Needs to be looked into */
#define MAX_IE_LENGTH_BUF			2048
#define MAX_STAT_ENTRIES	16
#define NEXT_STAT(x)    ((x + 1) & ((MAX_STAT_ENTRIES) - 1))

#ifdef WLRXOE
typedef struct rxoe_info  rxoe_info_t;
typedef struct rxoe_bcn_info rxoe_bcn_info_t;
typedef struct rxoe_arp_info rxoe_arp_info_t;
typedef struct rxoe_nd_info rxoe_nd_info_t;
#endif

enum {
	BCM_OL_BEACON_ENABLE,
	BCM_OL_BEACON_DISABLE,
	BCM_OL_ARP_ENABLE,
	BCM_OL_ARP_SETIP,
	BCM_OL_ARP_DISABLE,
	BCM_OL_ND_ENABLE,
	BCM_OL_ND_SETIP,
	BCM_OL_ND_DISABLE,
	BCM_OL_RESET,
	BCM_OL_FIFODEL,
	BCM_OL_MSG_TEST,
	BCM_OL_MSG_MAX
};


#include <packed_section_start.h>

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_stats_t {
	struct ipv4_addr src_ip;
	struct ipv4_addr dest_ip;
	uint8 suppressed;
	uint8 is_request;
} BWL_POST_PACKED_STRUCT olmsg_arp_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_stats_t {
	struct ipv6_addr dest_ip;
	uint8 suppressed;
	uint8 is_request;
} BWL_POST_PACKED_STRUCT olmsg_nd_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_dump_stats_info_t {
	uint32 rxoe_bcncount;
	uint32 rxoe_cleardefcnt;
	uint32 rxoe_bssidmiss_cnt;
	uint32 rxoe_capmiss_cnt;
	uint32 rxoe_bimiss_cnt;
	uint32 rxoe_bcnlosscnt;
	uint16 iechanged[OLMSG_BCN_MAX_IE];
	uint16 rxoe_arpcnt;
	olmsg_arp_stats arp_stats[MAX_STAT_ENTRIES];
	uint16 rxoe_ndcnt;
	olmsg_nd_stats	nd_stats[MAX_STAT_ENTRIES];
} BWL_POST_PACKED_STRUCT olmsg_dump_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_shared_info_t {
	uint32 msgbufaddr_low;
	uint32 msgbufaddr_high;
	uint32 msgbuf_sz;
	olmsg_dump_stats stats;
    uint32 console_addr;
} BWL_POST_PACKED_STRUCT olmsg_shared_info;

/* Message buffer start addreses is written at the end of
 * ARM memroy, 32 bytes additional.
 */
#define sz 		sizeof(olmsg_shared_info)
#define OLMSG_SHARED_INFO_SZ	(sz + 32 + (sz%4))


typedef BWL_PRE_PACKED_STRUCT struct vndriemask_info_t {
	union {
		struct ouidata {
		uint8	id[3];
		uint8	type;
		} b;
		uint32  mask;
	} oui;
} BWL_POST_PACKED_STRUCT vndriemask_info;

/* Read/Write Context */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_buf_info_t {
	uint32	offset;
	uint32	size;
	uint32	rx;
	uint32	wx;
} BWL_POST_PACKED_STRUCT olmsg_buf_info;

/* TBD: Should be a  packed structure */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_header_t {
	uint32 type;
	uint32 seq;
	uint32 len;
} BWL_POST_PACKED_STRUCT olmsg_header;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_test_t {
	olmsg_header hdr;
	uint32 data;
} BWL_POST_PACKED_STRUCT olmsg_test;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_reset_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_reset;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_fifodel_t {
	olmsg_header hdr;
	uint8 enable;
} BWL_POST_PACKED_STRUCT olmsg_fifodel;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_bcn_enable_t {
	olmsg_header    hdr;
	/* Deferral count to inform ucode */
	uint32	defcnt;

	/* BSSID beacon length */
	uint32	bcn_length;
	/* BSSID to support per interface */
	struct	ether_addr   BSSID;      /* BSSID (associated) */

	struct	ether_addr   cur_etheraddr;

	/* beacon interval  */
	uint16	bi; /* units are Kusec */

	/* Beacon capability */
	uint16 capability;

	/* Beacon received channel */
	uint32	rxchannel;


	/* association aid */
	uint32	aid;

	uint8 	frame_del;

	uint8	iemask[CEIL((OLMSG_BCN_MAX_IE+1), 8)];

	vndriemask_info	vndriemask[MAX_VNDR_IE];

	uint32	iedatalen;

	uint8	iedata[1];			/* Elements */
} BWL_POST_PACKED_STRUCT olmsg_bcn_enable;


typedef BWL_PRE_PACKED_STRUCT struct olmsg_bcn_disable_t {
	olmsg_header hdr;
	struct	ether_addr   BSSID;
} BWL_POST_PACKED_STRUCT olmsg_bcn_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_enable_t {
	olmsg_header		hdr;
	struct ether_addr	host_mac;
	struct ipv4_addr	host_ip;
	int8			iv_len;
} BWL_POST_PACKED_STRUCT olmsg_arp_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_disable_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_arp_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_setip_t {
	olmsg_header 		hdr;
	struct	ipv4_addr	host_ip;
} BWL_POST_PACKED_STRUCT olmsg_arp_setip;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_enable_t {
	olmsg_header 		hdr;
	struct ether_addr	host_mac;
	struct ipv6_addr	host_ip;
	int8				iv_len;
} BWL_POST_PACKED_STRUCT olmsg_nd_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_disable_t {
	olmsg_header		 hdr;
} BWL_POST_PACKED_STRUCT olmsg_nd_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_setip_t {
	olmsg_header 		hdr;
	struct ipv6_addr	host_ip;
} BWL_POST_PACKED_STRUCT olmsg_nd_setip;

#include <packed_section_end.h>

typedef struct olmsg_info_t {
	uchar	*msg_buff;
	uint32	len;
	olmsg_buf_info *write;
	olmsg_buf_info *read;
	uint32	next_seq;
} olmsg_info;

#ifdef WLRXOE
/* This needs to be moved to private place */
extern olmsg_shared_info *ppcie_shared;
#define RXOEINC(a)	(ppcie_shared->stats.a++)
#define RXOEINCIE(a, i) ((ppcie_shared->stats.a[i])++)

#define RXOEINC_N(a, n) (a = ((a + 1) & ((n) - 1)))
#define RXOEADDARPENTRY(entry) ({ uint8 i = ppcie_shared->stats.rxoe_arpcnt; \
			bcopy(&entry, &ppcie_shared->stats.arp_stats[i], sizeof(olmsg_arp_stats)); \
			RXOEINC_N(ppcie_shared->stats.rxoe_arpcnt, MAX_STAT_ENTRIES); })
#define RXOEADDNDENTRY(entry) ({ uint8 i = ppcie_shared->stats.rxoe_ndcnt; \
			bcopy(&entry, &ppcie_shared->stats.nd_stats[i], sizeof(olmsg_nd_stats)); \
			RXOEINC_N(ppcie_shared->stats.rxoe_ndcnt, MAX_STAT_ENTRIES); })
#endif
extern int
bcm_olmsg_create(uchar *buf, uint32 len);

/* Initialize message buffer */
extern int
bcm_olmsg_init(olmsg_info *ol_info, uchar *buf, uint32 len, uint8 rx, uint8 wx);

extern void
bcm_olmsg_deinit(olmsg_info *ol_info);

/* Copies the next message to be read into buf
	Updates the read pointer
	returns size of the message
	Pass NULL to retrieve the size of the message
 */
extern int
bcm_olmsg_getnext(olmsg_info *ol_info, char *buf, uint32 size);

/* same as bcm_olmsg_getnext, except that read pointer it not updated
 */
int
bcm_olmsg_peeknext(olmsg_info *ol_info, char *buf, uint32 size);


/* Writes the message to the shared buffer
	Updates the write pointer
	returns TRUE/FALSE depending on the availability of the space
	Pass NULL to retrieve the size of the message
 */
extern bool
bcm_olmsg_write(olmsg_info *ol_info, char *buf, uint32 size);

/*
 * Returns TRUE if the message buffer is full, else FALSE
 */
extern bool
bcm_olmsg_isfull(olmsg_info *ol_info);

/*
 * Returns TRUE if the message buffer is empty, else FALSE
 */
extern bool
bcm_olmsg_isempty(olmsg_info *ol_info);

/*
 * Returns free space of the message buffer
 */
extern uint32
bcm_olmsg_avail(olmsg_info *ol_info);

extern bool
bcm_olmsg_is_writebuf_full(olmsg_info *ol_info);

extern int
bcm_olmsg_writemsg(olmsg_info *ol, uchar *buf, uint16 len);

extern uint32
bcm_olmsg_bytes_to_read(olmsg_info *ol_info);

extern bool
bcm_olmsg_is_readbuf_empty(olmsg_info *ol_info);


extern uint32
bcm_olmsg_peekbytes(olmsg_info *ol, uchar *dst, uint32 len);

extern uint32
bcm_olmsg_readbytes(olmsg_info *ol, uchar *dst, uint32 len);

extern uint16
bcm_olmsg_peekmsg_len(olmsg_info *ol);

extern uint16
bcm_olmsg_readmsg(olmsg_info *ol, uchar *buf, uint16 len);

extern void
bcm_olmsg_dump_msg(olmsg_info *ol, olmsg_header *hdr);

extern void
bcm_olmsg_dump_record(olmsg_info *ol);

#endif /* _BCM_OL_MSG_H_ */
