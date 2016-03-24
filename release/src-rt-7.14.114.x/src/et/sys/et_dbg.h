/*
 * Minimal debug/trace/assert driver definitions for
 * Broadcom Home Networking Division 10/100 Mbit/s Ethernet
 * Device Driver.
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
 * $Id: et_dbg.h 404499 2013-05-28 01:06:37Z $
 */

#ifndef _et_dbg_
#define _et_dbg_

#ifdef	BCMDBG
struct ether_header;
extern void etc_prhdr(char *msg, struct ether_header *eh, uint len, int unit);
extern void etc_prhex(char *msg, uchar *buf, uint nbytes, int unit);
/*
 * et_msg_level is a bitvector:
 *	0	errors
 *	1	function-level tracing
 *	2	one-line frame tx/rx summary
 *	3	complex frame tx/rx in hex
 */
#define	ET_ERROR(args)	if (!(et_msg_level & 1)) ; else printf args
#define	ET_TRACE(args)	if (!(et_msg_level & 2)) ; else printf args
#define	ET_PRHDR(msg, eh, len, unit)	if (!(et_msg_level & 4)) ; else etc_prhdr(msg, eh, len, unit)
#define	ET_PRPKT(msg, buf, len, unit)	if (!(et_msg_level & 8)) ; else etc_prhex(msg, buf, len, unit)
#else	/* BCMDBG */
#define	ET_ERROR(args)
#define	ET_TRACE(args)
#define	ET_PRHDR(msg, eh, len, unit)
#define	ET_PRPKT(msg, buf, len, unit)
#endif	/* BCMDBG */

extern uint32 et_msg_level;

#define	ET_LOG(fmt, a1, a2)

/* include port-specific tunables */
#ifdef NDIS
#include <et_ndis.h>
#elif defined(__ECOS)
#include <et_ecos.h>
#elif defined(linux)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <linux/config.h>
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) */
#include <et_linux.h>
#elif defined(PMON)
#include <et_pmon.h>
#elif defined(_CFE_)
#include <et_cfe.h>
#elif defined(__NetBSD__)
#include <et_bsd.h>
#else
#error
#endif

#endif /* _et_dbg_ */
