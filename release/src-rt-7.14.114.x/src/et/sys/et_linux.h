/*
 * Linux device driver tunables for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver
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
 * $Id: et_linux.h 575708 2015-07-30 20:27:43Z $
 */

#ifndef _et_linux_h_
#define _et_linux_h_

/* tunables */
#define	NTXD		512		/* # tx dma ring descriptors (must be ^2) */
#define	NRXD		512		/* # rx dma ring descriptors (must be ^2) */
#if defined(CONFIG_RAM_SIZE) && ((CONFIG_RAM_SIZE > 0) && (CONFIG_RAM_SIZE <= 16))
#define NRXBUFPOST      256             /* try to keep this # rbufs posted to the chip */
#else
#ifdef ET_INGRESS_QOS
#define NRXBUFPOST      511             /* try to keep this # rbufs posted to the chip */
#else
#define NRXBUFPOST      320             /* try to keep this # rbufs posted to the chip */
#endif /* ET_INGRESS_QOS */
#endif /* CONFIG_RAM_SIZE ... */

#if defined(BCM_GMAC3)
#undef NRXBUFPOST
#define NRXBUFPOST 511
/*
 * To ensure that a 2K slab is used,                                  2048
 * Linux Add-ons: skb_shared_info=264, NET_SKB_PAD=32, align=32        352
 * BCMEXTRAHDROOM= 32                                                   32
 * RXBUFSZ = Buffer to hold ethernet frame = 2048 - 352 - 32        = 1664
 * RXBUFSZ includes space for 30B HWRXOFF + 1514B 802.3 + VLAN|BRCM Tag
 */
#define BUFSZ       (1696)
#define RXBUFSZ     (BUFSZ - BCMEXTRAHDROOM)
#else /* ! BCM_GMAC3 */
#define	BUFSZ		2048		/* packet data buffer size */
#define	RXBUFSZ		(BUFSZ - 256)	/* receive buffer size */
#endif /* ! BCM_GMAC3 */

#ifndef RXBND
#define RXBND		48		/* max # rx frames to process in dpc */
#endif

#if defined(ILSIM) || defined(__arch_um__)
#undef	NTXD
#define	NTXD		16
#undef	NRXD
#define	NRXD		16
#undef	NRXBUFPOST
#define	NRXBUFPOST	2
#endif

#define	PKTCBND		48		/* See also: IOV_PKTCBND */

/* 80pkts per millisec @900Mbps */
#define ET_RXLAZY_TIMEOUT   (1000U)  /* microseconds */
#define ET_RXLAZY_FRAMECNT  (32U)    /* interrupt coalescing over frames */

#if defined(CONFIG_RAM_SIZE) && ((CONFIG_RAM_SIZE > 0) && (CONFIG_RAM_SIZE <= 16))
#define CTFPOOLSZ	512
#else
#ifdef __ARM_ARCH_7A__
#define CTFPOOLSZ	1024
#else  /* ! __ARM_ARCH_7A__ */
#define CTFPOOLSZ	768
#endif /* ! __ARM_ARCH_7A__ */
#endif /* CONFIG_RAM_SIZE */

#define	PREFSZ			96
#ifdef PKTC
#define ETPREFHDRS(h, sz)
#else
#define ETPREFHDRS(h, sz)	OSL_PREF_RANGE_ST((h), (sz))
#endif

/* dma tunables */
#ifndef TXMR
#define TXMR			2	/* number of outstanding reads */
#endif

#ifndef TXPREFTHRESH
#define TXPREFTHRESH		8	/* prefetch threshold */
#endif

#ifndef TXPREFCTL
#define TXPREFCTL		16	/* max descr allowed in prefetch request */
#endif

#ifndef TXBURSTLEN
#define TXBURSTLEN		128	/* burst length for dma reads */
#endif

#ifndef RXPREFTHRESH
#define RXPREFTHRESH		1	/* prefetch threshold */
#endif

#ifndef RXPREFCTL
#define RXPREFCTL		8	/* max descr allowed in prefetch request */
#endif

#ifndef RXBURSTLEN
#define RXBURSTLEN		128	/* burst length for dma writes */
#endif

#define ET_IOCTL_MAXLEN		(127*1024)	/* max length ioctl buffer required;
						 * must be at least ET_DUMP_BUF_LEN
						 */
#endif	/* _et_linux_h_ */
