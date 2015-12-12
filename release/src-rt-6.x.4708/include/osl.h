/*
 * OS Abstraction Layer
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
 *
 * $Id: osl.h 419594 2013-08-21 22:47:07Z $
 */

#ifndef _osl_h_
#define _osl_h_

/* osl handle type forward declaration */
typedef struct osl_info osl_t;
typedef struct osl_dmainfo osldma_t;

#ifdef MACOSX
#define OSL_PKTTAG_SZ	56
#else
#define OSL_PKTTAG_SZ	32 /* Size of PktTag */
#endif

/* Drivers use PKTFREESETCB to register a callback function when a packet is freed by OSL */
typedef void (*pktfree_cb_fn_t)(void *ctx, void *pkt, unsigned int status);

/* Drivers use REGOPSSET() to register register read/write funcitons */
typedef unsigned int (*osl_rreg_fn_t)(void *ctx, volatile void *reg, unsigned int size);
typedef void  (*osl_wreg_fn_t)(void *ctx, volatile void *reg, unsigned int val, unsigned int size);

#ifdef __mips__
#define PREF_LOAD		0
#define PREF_STORE		1
#define PREF_LOAD_STREAMED	4
#define PREF_STORE_STREAMED	5
#define PREF_LOAD_RETAINED	6
#define PREF_STORE_RETAINED	7
#define PREF_WBACK_INV		25
#define PREF_PREPARE4STORE	30


#define MAKE_PREFETCH_FN(hint) \
static inline void prefetch_##hint(const void *addr) \
{ \
	__asm__ __volatile__(\
	"       .set    mips4           \n" \
	"       pref    %0, (%1)        \n" \
	"       .set    mips0           \n" \
	: \
	: "i" (hint), "r" (addr)); \
}

#define MAKE_PREFETCH_RANGE_FN(hint) \
static inline void prefetch_range_##hint(const void *addr, int len) \
{ \
	int size = len; \
	while (size > 0) { \
		prefetch_##hint(addr); \
		size -= 32; \
	} \
}

MAKE_PREFETCH_FN(PREF_LOAD)
MAKE_PREFETCH_RANGE_FN(PREF_LOAD)
MAKE_PREFETCH_FN(PREF_STORE)
MAKE_PREFETCH_RANGE_FN(PREF_STORE)
MAKE_PREFETCH_FN(PREF_LOAD_STREAMED)
MAKE_PREFETCH_RANGE_FN(PREF_LOAD_STREAMED)
MAKE_PREFETCH_FN(PREF_STORE_STREAMED)
MAKE_PREFETCH_RANGE_FN(PREF_STORE_STREAMED)
MAKE_PREFETCH_FN(PREF_LOAD_RETAINED)
MAKE_PREFETCH_RANGE_FN(PREF_LOAD_RETAINED)
MAKE_PREFETCH_FN(PREF_STORE_RETAINED)
MAKE_PREFETCH_RANGE_FN(PREF_STORE_RETAINED)
#endif /* __mips__ */

#if defined(__ECOS)
#include <ecos_osl.h>
#elif  defined(DOS)
#include <dos_osl.h>
#elif defined(PCBIOS)
#include <pcbios_osl.h>
#elif defined(linux)
#include <linux_osl.h>
#elif defined(NDIS)
#include <ndis_osl.h>
#elif defined(_CFE_)
#include <cfe_osl.h>
#elif defined(_MINOSL_)
#include <min_osl.h>
#elif defined(MACOSX)
#include <macosx_osl.h>
#elif defined(__NetBSD__)
#include <bsd_osl.h>
#elif defined(EFI)
#include <efi_osl.h>
#elif defined(TARGETOS_nucleus)
#include <nucleus_osl.h>
#elif defined(TARGETOS_symbian)
#include <symbian_osl.h>
#else
#error "Unsupported OSL requested"
#endif 

#ifndef PKTDBG_TRACE
#define PKTDBG_TRACE(osh, pkt, bit)
#endif

#ifndef PKTCTFMAP
#define PKTCTFMAP(osh, p)
#endif /* PKTCTFMAP */

/* --------------------------------------------------------------------------
** Register manipulation macros.
*/

#define	SET_REG(osh, r, mask, val)	W_REG((osh), (r), ((R_REG((osh), r) & ~(mask)) | (val)))

#ifndef AND_REG
#define AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#endif   /* !AND_REG */

#ifndef OR_REG
#define OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))
#endif   /* !OR_REG */

#if !defined(OSL_SYSUPTIME)
#define OSL_SYSUPTIME() (0)
#define OSL_SYSUPTIME_SUPPORT FALSE
#else
#define OSL_SYSUPTIME_SUPPORT TRUE
#endif /* OSL_SYSUPTIME */

#if !(defined(linux) && defined(PKTC)) && !defined(PKTC_DONGLE) && \
	!(defined(__NetBSD__) && defined(PKTC))
#define	PKTCGETATTR(s)		(0)
#define	PKTCSETATTR(skb, f, p, b)
#define	PKTCCLRATTR(skb)
#define	PKTCCNT(skb)		(1)
#define	PKTCLEN(skb)		PKTLEN(NULL, skb)
#define	PKTCGETFLAGS(skb)	(0)
#define	PKTCSETFLAGS(skb, f)
#define	PKTCCLRFLAGS(skb)
#define	PKTCFLAGS(skb)		(0)
#define	PKTCSETCNT(skb, c)
#define	PKTCINCRCNT(skb)
#define	PKTCADDCNT(skb, c)
#define	PKTCSETLEN(skb, l)
#define	PKTCADDLEN(skb, l)
#define	PKTCSETFLAG(skb, fb)
#define	PKTCCLRFLAG(skb, fb)
#define	PKTCLINK(skb)		NULL
#define	PKTSETCLINK(skb, x)
#define FOREACH_CHAINED_PKT(skb, nskb) \
	for ((nskb) = NULL; (skb) != NULL; (skb) = (nskb))
#define	PKTCFREE		PKTFREE
#endif /* !linux || !PKTC */

#if !defined(HNDCTF) && !(defined(__NetBSD__) && defined(PKTC))
#define PKTSETCHAINED(osh, skb)
#define PKTCLRCHAINED(osh, skb)
#define PKTISCHAINED(skb)	(FALSE)
#endif

#endif	/* _osl_h_ */
