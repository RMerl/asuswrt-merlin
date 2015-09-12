/*
 * CFE boot loader OS Abstraction Layer.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: cfe_osl.h 466958 2014-04-02 02:15:47Z $
 */

#ifndef _cfe_osl_h_
#define _cfe_osl_h_

#include <lib_types.h>
#include <lib_string.h>
#include <lib_printf.h>
#include <lib_malloc.h>
#include <cpu_config.h>
#include <cfe_timer.h>
#include <cfe_iocb.h>
#include <cfe_devfuncs.h>
#include <addrspace.h>

#include <typedefs.h>

/* pick up osl required snprintf/vsnprintf */
#include <bcmstdlib.h>

/* assert and panic */
#define	ASSERT(exp)		do {} while (0)

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size)  ({BCM_REFERENCE(osh); bzero(buf, size);})
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) ({BCM_REFERENCE(osh); BCM_REFERENCE(buf);})

/* PCI configuration space access macros */
#define	OSL_PCI_READ_CONFIG(loc, offset, size) \
	(offset == 8 ? 0 : 0xffffffff)
#define	OSL_PCI_WRITE_CONFIG(loc, offset, size, val) \
	do {(void)(loc); (void)(offset); (void)(size);(void)(val);} while (0)

/* PCI device bus # and slot # */
#define	OSL_PCI_BUS(osh)	({BCM_REFERENCE(osh); 0;})
#define	OSL_PCI_SLOT(osh)	({BCM_REFERENCE(osh); 0;})
#define	OSL_PCIE_DOMAIN(osh)	({BCM_REFERENCE(osh); 0;})
#define	OSL_PCIE_BUS(osh)	({BCM_REFERENCE(osh); 0;})

/* register access macros */
#ifdef IL_BIGENDIAN
#ifdef	BCMHND74K
#define wreg32(r, v)		(*(volatile uint32 *)((ulong)(r) ^ 4) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32 *)((ulong)(r) ^ 4))
#define wreg16(r, v)		(*(volatile uint16 *)((ulong)(r) ^ 6) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16 *)((ulong)(r) ^ 6))
#define wreg8(r, v)		(*(volatile uint8 *)((ulong)(r) ^ 7) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8 *)((ulong)(r) ^ 7))
#else	/* !74K, bcm33xx */
#define wreg32(r, v)		(*(volatile uint32*)(r) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32*)(r))
#define wreg16(r, v)		(*(volatile uint16*)((ulong)(r) ^ 2) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16*)((ulong)(r) ^ 2))
#define wreg8(r, v)		(*(volatile uint8*)((ulong)(r) ^ 3) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8*)((ulong)(r) ^ 3))
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
	BCM_REFERENCE(osh); \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	__osl_v = rreg8((void *)(r)); break; \
	case sizeof(uint16):	__osl_v = rreg16((void *)(r)); break; \
	case sizeof(uint32):	__osl_v = rreg32((void *)(r)); break; \
	} \
	__osl_v; \
})
#define W_REG(osh, r, v) do { \
	BCM_REFERENCE(osh); \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	wreg8((void *)(r), (v)); break; \
	case sizeof(uint16):	wreg16((void *)(r), (v)); break; \
	case sizeof(uint32):	wreg32((void *)(r), (v)); break; \
	} \
} while (0)
#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* bcopy, bcmp, and bzero */
#define bcmp(b1, b2, len)	lib_memcmp((b1), (b2), (len))
#define	memmove(dest, src, n)	lib_memcpy((dest), (src), (n))

struct osl_info {
	void *pdev;
	pktfree_cb_fn_t tx_fn;
	void *tx_ctx;
};

extern osl_t *osl_attach(void *pdev);
extern void osl_detach(osl_t *osh);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx) \
	do { \
	   osh->tx_fn = _tx_fn; \
	   osh->tx_ctx = _tx_ctx; \
	} while (0)

/* general purpose memory allocation */
#define	MALLOC(osh, size)	({BCM_REFERENCE(osh); KMALLOC((size), 0);})
#define	MALLOCZ(osh, size) \
	({void *_ptr; \
	_ptr = MALLOC(osh, size); \
	if (_ptr != NULL) {bzero(_ptr, (size));} \
	_ptr; })
#define	MFREE(osh, addr, size)	({BCM_REFERENCE(osh); BCM_REFERENCE(size); KFREE((addr));})
#define	MALLOCED(osh)		({BCM_REFERENCE(osh); 0;})
#define	MALLOC_DUMP(osh, b)	({BCM_REFERENCE(osh); BCM_REFERENCE(b);})
#define	MALLOC_FAILED(osh)	({BCM_REFERENCE(osh); 0;})

/* uncached/cached virtual address */
#ifdef	__mips__
#define	OSL_UNCACHED(a)		((void *)UNCADDR(PHYSADDR((ulong)(a))))
#define	OSL_CACHED(a)		((void *)KERNADDR(PHYSADDR((ulong)(a))))
#else
#define OSL_UNCACHED(a)		((void *)UNCADDR(PHYSADDR((ulong)(a))))
#define OSL_CACHED(a)		((void *)KERNADDR(PHYSADDR((ulong)(a))))
/* ARM NorthStar */
#ifndef CFG_UNCACHED
extern void _cfe_flushcache_rang(uint, uint);
#define OSL_CACHE_FLUSH(va, len)	_cfe_flushcache_rang(va, len)
#else
#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
#endif
#endif /* __mips__ */

#ifdef __mips__
#define OSL_PREF_RANGE_LD(va, sz) prefetch_range_PREF_LOAD_RETAINED(va, sz)
#define OSL_PREF_RANGE_ST(va, sz) prefetch_range_PREF_STORE_RETAINED(va, sz)
#else /* __mips__ */
#define OSL_PREF_RANGE_LD(va, sz)	BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz)	BCM_REFERENCE(va)
#endif /* __mips__ */

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = 0)

/* microsecond delay */
#define	OSL_DELAY(usec)		cfe_usleep((cfe_cpu_speed/CPUCFG_CYCLESPERCPUTICK/1000000*(usec)))

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)	((void*)UNCADDR(PHYSADDR((ulong)(pa))))
#define	REG_UNMAP(va)		BCM_REFERENCE(va)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (uint32)(addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* allocate/free shared (dma-able) consistent (uncached) memory */
#define	DMA_CONSISTENT_ALIGN	4096		/* 4k alignment */
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(dmah); \
	 osl_dma_alloc_consistent((size), (align), (tot), (pap)); \
	 })
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(size); \
	 BCM_REFERENCE(pa); \
	 BCM_REFERENCE(dmah); \
	 osl_dma_free_consistent((void*)(va)); \
	 })
extern void *osl_dma_alloc_consistent(uint size, uint16 align_bits, uint *alloced, ulong *pap);
extern void osl_dma_free_consistent(void *va);

/* map/unmap direction */
#define	DMA_TX			1	/* TX direction for DMA */
#define	DMA_RX			2	/* RX direction for DMA */

/* map/unmap shared (dma-able) memory */
#define	DMA_MAP(osh, va, size, direction, lb, dmah) ({ \
	BCM_REFERENCE(osh); \
	BCM_REFERENCE(va); \
	BCM_REFERENCE(size); \
	BCM_REFERENCE(direction); \
	BCM_REFERENCE(lb); \
	BCM_REFERENCE(dmah); \
	cfe_flushcache(CFE_CACHE_FLUSH_D); \
	PHYSADDR((ulong)(va)); \
})
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) ({ \
	BCM_REFERENCE(osh); \
	BCM_REFERENCE(pa); \
	BCM_REFERENCE(size); \
	BCM_REFERENCE(direction); \
	BCM_REFERENCE(p); \
	BCM_REFERENCE(dmah); \
})

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) ({ \
	BCM_REFERENCE(osh); \
	BCM_REFERENCE(addrwidth); \
})

/* shared (dma-able) memory access macros */
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
#define	R_SM(r)			(*(uint32 *)((ulong)(r)^4))
#define	W_SM(r, v)		(*(uint32 *)((ulong)(r)^4) = (uint32)(v))
#else	/* !IL_BIGENDIAN || !BCMHND74K */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#endif	/* IL_BIGENDIAN && BCMHND74K */
#define	BZERO_SM(r, len)	lib_memset((r), '\0', (len))

/* generic packet structure */
#define LBUFSZ		4096		/* Size of Lbuf - 4k */
#define LBDATASZ	(LBUFSZ - sizeof(struct lbuf))
struct lbuf {
	struct lbuf	*next;		/* pointer to next lbuf if in a chain */
	struct lbuf	*link;		/* pointer to next lbuf if in a list */
	uchar		*head;		/* start of buffer */
	uchar		*end;		/* end of buffer */
	uchar		*data;		/* start of data */
	uchar		*tail;		/* end of data */
	uint		len;		/* nbytes of data */
	uchar		pkttag[OSL_PKTTAG_SZ]; /* pkttag area */
};

#define OSH_NULL   NULL

#define	PKTBUFSZ	2048	/* largest reasonable packet buffer, driver uses for ethernet MTU */

/* packet primitives */
#define	PKTGET(osh, len, send) \
	({ \
	BCM_REFERENCE(osh); \
	BCM_REFERENCE(send); \
	(void*)osl_pktget((len)); \
	})
#define	PKTFREE(osh, lb, send)		osl_pktfree((osh), (struct lbuf*)(lb), (send))
#define	PKTDATA(osh, lb)		({BCM_REFERENCE(osh); ((struct lbuf*)(lb))->data;})
#define	PKTLEN(osh, lb)			({BCM_REFERENCE(osh); ((struct lbuf*)(lb))->len;})
#define PKTHEADROOM(osh, lb)		(PKTDATA(osh, lb) - (((struct lbuf*)(lb))->head))
#define PKTTAILROOM(osh, lb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct lbuf*)(lb))->end)-(((struct lbuf*)(lb))->tail); \
	 })
#define	PKTNEXT(osh, lb)		({BCM_REFERENCE(osh); ((struct lbuf*)(lb))->next;})
#define	PKTSETNEXT(osh, lb, x)		(((struct lbuf*)(lb))->next = (struct lbuf*)(x))
#define	PKTSETLEN(osh, lb, len) \
	({ \
	 BCM_REFERENCE(osh); \
	 osl_pktsetlen((struct lbuf*)(lb), (len)); \
	 })
#define	PKTPUSH(osh, lb, bytes) \
	({ \
	 BCM_REFERENCE(osh); \
	 osl_pktpush((struct lbuf*)(lb), (bytes)); \
	 })
#define	PKTPULL(osh, lb, bytes) \
	({ \
	 BCM_REFERENCE(osh); \
	 osl_pktpull((struct lbuf*)(lb), (bytes)); \
	 })
#define	PKTDUP(osh, lb)			({BCM_REFERENCE(osh); osl_pktdup((struct lbuf*)(lb));})
#define	PKTTAG(lb)			(((void *) ((struct lbuf *)(lb))->pkttag))
#define	PKTLINK(lb)			(((struct lbuf*)(lb))->link)
#define	PKTSETLINK(lb, x)		(((struct lbuf*)(lb))->link = (struct lbuf*)(x))
#define	PKTPRIO(lb)			(0)
#define	PKTSETPRIO(lb, x)		do {} while (0)
#define	PKTFRMNATIVE(buffer, lb)	osl_pkt_frmnative((buffer), (struct lbuf *)(lb))
#define	PKTTONATIVE(lb, buffer)		osl_pkt_tonative((lb), (buffer))
#define	PKTSHARED(lb)			({BCM_REFERENCE(lb); 0;})
#define	PKTALLOCED(osh)			({BCM_REFERENCE(osh); 0;})
#define PKTSETPOOL(osh, lb, x, y) \
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(lb); \
	 BCM_REFERENCE(x); \
	 BCM_REFERENCE(y); \
	 })
#define	PKTPOOL(osh, lb)		({BCM_REFERENCE(osh); BCM_REFERENCE(lb); FALSE;})
#define PKTFREELIST(lb)         PKTLINK(lb)
#define PKTSETFREELIST(lb, x)   PKTSETLINK((lb), (x))
#define PKTPTR(lb)              (lb)
#define PKTID(lb)               ({BCM_REFERENCE(lb); 0;})
#define PKTSETID(lb, id)        ({BCM_REFERENCE(lb); BCM_REFERENCE(id);})
#define PKTLIST_DUMP(osh, buf)		({BCM_REFERENCE(osh); BCM_REFERENCE(buf);})
#define PKTSHRINK(osh, m)		({BCM_REFERENCE(osh); m;})

extern void osl_pkt_frmnative(iocb_buffer_t *buffer, struct lbuf *lb);
extern void osl_pkt_tonative(struct lbuf* lb, iocb_buffer_t *buffer);
extern struct lbuf *osl_pktget(uint len);
extern void osl_pktfree(osl_t *osh, struct lbuf *lb, bool send);
extern void osl_pktsetlen(struct lbuf *lb, uint len);
extern uchar *osl_pktpush(struct lbuf *lb, uint bytes);
extern uchar *osl_pktpull(struct lbuf *lb, uint bytes);
extern struct lbuf *osl_pktdup(struct lbuf *lb);
extern int osl_error(int bcmerror);

/* Global ASSERT type */
extern uint32 g_assert_type;

#endif	/* _cfe_osl_h_ */
