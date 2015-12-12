/*
 * HND Minimal OS Abstraction Layer.
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
 * $Id: min_osl.h 505047 2014-09-26 07:54:55Z $
 */

#ifndef _min_osl_h_
#define _min_osl_h_

#include <typedefs.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <bcmstdlib.h>

#ifdef	mips
/* Cache support */
extern void caches_on(void);
extern void blast_dcache(void);
extern void blast_icache(void);
#elif defined(__ARM_ARCH_7A__)
extern void caches_on(void);
extern void blast_dcache(void);
extern void blast_icache(void);
#else /* !mips */
/* Cache support (or lack thereof) */
static inline void caches_on(void) { return; };
static inline void blast_dcache(void) { return; };
static inline void blast_icache(void) { return; };
#endif	/* mips */

/* assert & debugging */
#if defined(BCMDBG)
extern void assfail(char *exp, char *file, int line);
#define ASSERT(exp) \
	do { if (!(exp)) assfail(#exp, __FILE__, __LINE__); } while (0)

#ifdef __ARM_ARCH_7A__
#define	TRACE_LOC		OSL_UNCACHED(0x18000064)	/* BP access address reg in chipc */
#else
#define	TRACE_LOC		OSL_UNCACHED(0x180000d0)	/* BP access address reg in chipc */
#endif

#define	BCMDBG_TRACE(val)	do {*((uint32 *)TRACE_LOC) = val;} while (0)
#else
#define	ASSERT(exp)		do {} while (0)
#define	BCMDBG_TRACE(val)	do {} while (0)
#endif /* BCMDBG */

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	ASSERT(0)
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	ASSERT(0)

/* PCI configuration space access macros */
#define	OSL_PCI_READ_CONFIG(loc, offset, size) \
	(offset == 8 ? 0 : 0xffffffff)
#define	OSL_PCI_WRITE_CONFIG(loc, offset, size, val) \
	do {} while (0)

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	(0)
#define OSL_PCI_SLOT(osh)	(0)
#define	OSL_PCIE_DOMAIN(osh)	({BCM_REFERENCE(osh); 0;})
#define	OSL_PCIE_BUS(osh)	({BCM_REFERENCE(osh); 0;})

/* register access macros */
#ifdef	IL_BIGENDIAN
#ifdef	BCMHND74K

#define wreg32(r, v)		(*(volatile uint32 *)((uint32)(r) ^ 4) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32 *)((uint32)(r) ^ 4))
#define wreg16(r, v)		(*(volatile uint16 *)((uint32)(r) ^ 6) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16 *)((uint32)(r) ^ 6))
#define wreg8(r, v)		(*(volatile uint8 *)((uint32)(r) ^ 7) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8 *)((uint32)(r) ^ 7))

#else	/* !BCMHND74K */

#define wreg32(r, v)		(*(volatile uint32*)(r) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32*)(r))
#define wreg16(r, v)		(*(volatile uint16 *)((uint32)(r) ^ 2) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16 *)((uint32)(r) ^ 2))
#define wreg8(r, v)		(*(volatile uint8 *)((uint32)(r) ^ 3) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8 *)((uint32)(r) ^ 3))

#endif	/* BCMHND74K */

#else	/* !IL_BIGENDIAN */

#define wreg32(r, v)		(*(volatile uint32*)(r) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32*)(r))
#define wreg16(r, v)		(*(volatile uint16*)(r) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16*)(r))
#define wreg8(r, v)		(*(volatile uint8*)(r) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8*)(r))

#endif	/* IL_BIGENDIAN */

#define R_REG(osh, r) ({ \
	__typeof(*(r)) __osl_v; \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	__osl_v = rreg8((void *)(r)); break; \
	case sizeof(uint16):	__osl_v = rreg16((void *)(r)); break; \
	case sizeof(uint32):	__osl_v = rreg32((void *)(r)); break; \
	} \
	__osl_v; \
})
#define W_REG(osh, r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	wreg8((void *)(r), (v)); break; \
	case sizeof(uint16):	wreg16((void *)(r), (v)); break; \
	case sizeof(uint32):	wreg32((void *)(r), (v)); break; \
	} \
} while (0)
#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* general purpose memory allocation */
#define	MALLOC(osh, size)	malloc(size)
#define	MALLOC_ALIGN(osh, size, align_bits)	malloc_align((size), (align_bits))
#define	MFREE(osh, addr, size)	free(addr)
#define	MALLOCED(osh)		0
#define	MALLOC_FAILED(osh)	0
#define	MALLOC_DUMP(osh, b)
extern int free(void *ptr);
extern void *malloc(uint size);
extern void *malloc_align(uint size, uint align_bits);

/* uncached virtual address */
#ifdef	__mips__
#define	OSL_UNCACHED(va)	((void *)KSEG1ADDR((ulong)(va)))
#define	OSL_CACHED(va)		((void *)KSEG0ADDR((ulong)(va)))
#elif  defined(__ARM_ARCH_7A__)
#define	OSL_UNCACHED(va)	((void *)(va))
#define	OSL_CACHED(va)		((void *)(va))
/* ARM NorthStar */
#define OSL_CACHE_FLUSH(va, len)	_cfe_flushcache_rang(va, len)
#else
#define	OSL_UNCACHED(va)	((void *)(va))
#define	OSL_CACHED(va)		((void *)(va))
#define PHYSADDR_MASK		0xffffffff
#define PHYSADDR(va)		((uintptr)(va))
#endif

#ifdef __mips__
#define OSL_PREF_RANGE_LD(va, sz) prefetch_range_PREF_LOAD_RETAINED(va, sz)
#define OSL_PREF_RANGE_ST(va, sz) prefetch_range_PREF_STORE_RETAINED(va, sz)
#else /* __mips__ */
#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)
#endif /* __mips__ */

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* microsecond delay */
#define	OSL_DELAY(usec)		udelay(usec)
extern void udelay(uint32 usec);

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
extern uint32 osl_getcycles(void);

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)	(OSL_UNCACHED(pa))
#define	REG_UNMAP(va)		do {} while (0)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	(uint32 *)(addr) = (val)

/* Misc stubs */
#define osl_attach(pdev)	((osl_t*)pdev)
#define osl_detach(osh)

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx)

extern void *osl_init(void);
#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int);

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ			(MAXPKTBUFSZ - LBUFSZ)

/* packet primitives */
#define PKTGET(osh, len, send)		((void *)NULL)
#define PKTFREE(osh, p, send)
#define	PKTDATA(osh, lb)		((void *)NULL)
#define	PKTLEN(osh, lb)			0
#define	PKTHEADROOM(osh, lb)		0
#define	PKTTAILROOM(osh, lb)		0
#define	PKTNEXT(osh, lb)		((void *)NULL)
#define	PKTSETNEXT(osh, lb, x)
#define	PKTSETLEN(osh, lb, len)
#define	PKTPUSH(osh, lb, bytes)
#define	PKTPULL(osh, lb, bytes)
#define PKTDUP(osh, p)
#define	PKTTAG(lb)			((void *)NULL)
#define	PKTLINK(lb)			((void *)NULL)
#define	PKTSETLINK(lb, x)
#define	PKTPRIO(lb)			0
#define	PKTSETPRIO(lb, x)
#define PKTSHARED(lb)			1
#define PKTALLOCED(osh)			0
#define PKTLIST_DUMP(osh, buf)
#define PKTFRMNATIVE(osh, lb)		((void *)NULL)
#define PKTTONATIVE(osh, p)		((struct lbuf *)NULL)
#define PKTSETPOOL(osh, lb, x, y)	do {} while (0)
#define PKTPOOL(osh, lb)		FALSE
#define PKTSHRINK(osh, m)		(m)

/* Global ASSERT type */
extern uint32 g_assert_type;

/* Kernel: File Operations: start */
#define osl_os_open_image(filename)	NULL
#define osl_os_get_image_block(buf, len, image)	0
#define osl_os_close_image(image)	do {} while (0)
/* Kernel: File Operations: end */

#endif	/* _min_osl_h_ */
