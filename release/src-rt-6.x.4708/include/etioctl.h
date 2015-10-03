/*
 * BCM44XX Ethernet Windows device driver custom OID definitions.
 *
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
 * $Id: etioctl.h 429710 2013-10-15 21:00:58Z $
 */

#ifndef _etioctl_h_
#define	_etioctl_h_

/*
 * Minor kludge alert:
 * Duplicate a few definitions that irelay requires from epiioctl.h here
 * so caller doesn't have to include this file and epiioctl.h .
 * If this grows any more, it would be time to move these irelay-specific
 * definitions out of the epiioctl.h and into a separate driver common file.
 */
#ifndef EPICTRL_COOKIE
#define EPICTRL_COOKIE		0xABADCEDE
#endif

/* common ioctl definitions */
#define	ETCUP		0
#define	ETCDOWN		1
#define ETCLOOP		2
#define ETCDUMP		3
#define ETCSETMSGLEVEL	4
#define	ETCPROMISC	5
#define ETCVAR		6
#define	ETCSPEED	7
#define ETCPHYRD	9
#define ETCPHYWR	10
#define	ETCQOS		11
#define ETCPHYRD2	12
#define ETCPHYWR2	13
#define ETCROBORD	14
#define ETCROBOWR	15

/*
 * A set of iovars defined for ET set/get
 */
#define IOV_ET_POWER_SAVE_MODE	1
#define IOV_ET_CLEAR_DUMP	2
#define IOV_ET_ROBO_DEVID	3
#define IOV_PKTC		4
#define IOV_PKTCBND		5
#define IOV_COUNTERS		6
#define IOV_DUMP_CTF		7
#define IOV_DUMP_CTRACE		8
#define IOV_DUMP		9
#define IOV_FA_DUMP		10
#define IOV_DMA_RX_THRESH	11
#define IOV_DMA_RX_POLICY	12
#define IOV_PORTSTATS		13
#define IOV_SW_MCTBL		14
#define IOV_FA_REV		15
#define IOV_DUMP_OOPS		16

#if defined(linux) || defined(__ECOS)
#define SIOCSETCUP		(SIOCDEVPRIVATE + ETCUP)
#define SIOCSETCDOWN		(SIOCDEVPRIVATE + ETCDOWN)
#define SIOCSETCLOOP		(SIOCDEVPRIVATE + ETCLOOP)
#define SIOCGETCDUMP		(SIOCDEVPRIVATE + ETCDUMP)
#define SIOCSETCSETMSGLEVEL	(SIOCDEVPRIVATE + ETCSETMSGLEVEL)
#define SIOCSETCPROMISC		(SIOCDEVPRIVATE + ETCPROMISC)
#define SIOCSETGETVAR		(SIOCDEVPRIVATE + ETCVAR)
#define SIOCSETCSPEED		(SIOCDEVPRIVATE + ETCSPEED)
#define SIOCTXGEN		(SIOCDEVPRIVATE + 8)
#define SIOCGETCPHYRD		(SIOCDEVPRIVATE + ETCPHYRD)
#define SIOCSETCPHYWR		(SIOCDEVPRIVATE + ETCPHYWR)
#define SIOCSETCQOS		(SIOCDEVPRIVATE + ETCQOS)
#define SIOCGETCPHYRD2		(SIOCDEVPRIVATE + ETCPHYRD2)
#define SIOCSETCPHYWR2		(SIOCDEVPRIVATE + ETCPHYWR2)
#define SIOCGETCROBORD		(SIOCDEVPRIVATE + ETCROBORD)
#define SIOCSETCROBOWR		(SIOCDEVPRIVATE + ETCROBOWR)

/* structure to send a generic var set/get */
typedef struct et_var_s {
	uint cmd;
	uint set;
	void *buf;
	uint len;
} et_var_t;

/* arg to SIOCTXGEN */
struct txg {
	uint32 num;		/* number of frames to send */
	uint32 delay;		/* delay in microseconds between sending each */
	uint32 size;		/* size of ether frame to send */
	uchar buf[1514];	/* starting ether frame data */
};
#endif /* linux */


#if defined(__NetBSD__)
#define SIOCSETCUP		 _IOW('e',  0, struct ifreq)
#define SIOCSETCDOWN		 _IOW('e',  1, struct ifreq)
#define SIOCSETCLOOP		 _IOW('e',  2, struct ifreq)
#define SIOCGETCDUMP		_IOWR('e',  3, struct ifreq)
#define SIOCSETCSETMSGLEVEL	 _IOW('e',  4, struct ifreq)
#define SIOCSETCPROMISC		 _IOW('e',  5, struct ifreq)
#define SIOCSETCTXDOWN		 _IOW('e',  6, struct ifreq)	/* obsolete */
#define SIOCSETCSPEED		 _IOW('e',  7, struct ifreq)
#define SIOCTXGEN		 _IOW('e',  8, struct ifreq)
#define SIOCGETCPHYRD		_IOWR('e',  9, struct ifreq)
#define SIOCSETCPHYWR		 _IOW('e', 10, struct ifreq)
#define SIOCSETCQOS		 _IOW('e', 11, struct ifreq)
#define SIOCGETCPHYRD2		_IOWR('e', 12, struct ifreq)
#define SIOCSETCPHYWR2		 _IOW('e', 13, struct ifreq)
#define SIOCGETCROBORD		_IOWR('e', 14, struct ifreq)
#define SIOCSETCROBOWR		 _IOW('e', 15, struct ifreq)

/* arg to SIOCTXGEN */
struct txg {
	uint32 num;		/* number of frames to send */
	uint32 delay;		/* delay in microseconds between sending each */
	uint32 size;		/* size of ether frame to send */
	uchar buf[1514];	/* starting ether frame data */
};
#endif	/* __NetBSD__ */

/*
 * custom OID support
 *
 * 0xFF - implementation specific OID
 * 0xE4 - first byte of Broadcom PCI vendor ID
 * 0x14 - second byte of Broadcom PCI vendor ID
 * 0xXX - the custom OID number
 */
#define ET_OID_BASE		0xFFE41400 /* OID Base for ET */

#define	OID_ET_UP				(ET_OID_BASE + ETCUP)
#define	OID_ET_DOWN				(ET_OID_BASE + ETCDOWN)
#define	OID_ET_LOOP				(ET_OID_BASE + ETCLOOP)
#define	OID_ET_DUMP				(ET_OID_BASE + ETCDUMP)
#define	OID_ET_SETMSGLEVEL			(ET_OID_BASE + ETCSETMSGLEVEL)
#define	OID_ET_PROMISC				(ET_OID_BASE + ETCPROMISC)
#define	OID_ET_TXDOWN				(ET_OID_BASE + 6)
#define	OID_ET_SPEED				(ET_OID_BASE + ETCSPEED)
#define	OID_ET_GETINSTANCE			(ET_OID_BASE + 8)
#define	OID_ET_SETCALLBACK			(ET_OID_BASE + 9)
#define	OID_ET_UNSETCALLBACK			(ET_OID_BASE + 10)

#define IS_ET_OID(oid) (((oid) & 0xFFFFFF00) == 0xFFE41400)

#define	ET_ISQUERYOID(oid)	((oid == OID_ET_DUMP) || (oid == OID_ET_GETINSTANCE))

/* OID_ET_SETCALLBACK data type */
typedef struct et_cb {
	void (*fn)(void *, int);	/* Callback function */
	void *context;				/* Passed to callback function */
} et_cb_t;

/* FA mode values */
#define CTF_FA_DISABLED		0
#define CTF_FA_BYPASS		1
#define CTF_FA_NORMAL		2
#define CTF_FA_SW_ACC		3
#define FA_ON(mode) 		(mode == CTF_FA_BYPASS || mode == CTF_FA_NORMAL)

/* FA callback commands */
#define FA_CB_ADD_NAPT		1
#define FA_CB_DEL_NAPT		2
#define FA_CB_GET_LIVE		3
#define FA_CB_CONNTRACK		4

#endif /* _etioctl_h_ */
