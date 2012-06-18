/*
 * HND Run Time Environment OS Abstraction Layer.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hndrte_osl.h,v 13.98.14.1 2010-05-23 18:19:25 Exp $
 */

#ifndef _hndrte_osl_h_
#define _hndrte_osl_h_

#include <hndrte.h>
#include <hndrte_lbuf.h>

struct osl_info {
	uint pktalloced;	/* Number of allocated packet buffers */
	void *dev;		/* Device handle */
	pktfree_cb_fn_t tx_fn;	/* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
};

struct pktpool;
struct bcmstrbuf;

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	ASSERT(0)
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	ASSERT(0)

/* PCI configuration space access macros */
#ifdef	SBPCI
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
		osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
		osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
#else	/* SBPCI */
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	(offset == 8 ? 0 : 0xffffffff)
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	do {} while (0)

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	(0)
#define OSL_PCI_SLOT(osh)	(0)
#endif	/* SBPCI */

/* register access macros */
#define	R_REG(osh, r) \
	(sizeof(*(r)) == sizeof(uint32) ? rreg32((volatile uint32 *)(void *)(r)) : \
	 sizeof(*(r)) == sizeof(uint16) ? rreg16((volatile uint16 *)(void *)(r)) : \
					  rreg8((volatile uint8 *)(void *)(r)))
#define	W_REG(osh, r, v) \
	do { \
		if (sizeof(*(r)) == sizeof(uint32)) \
			wreg32((volatile uint32 *)(void *)(r), (uint32)(v)); \
		else if (sizeof(*(r)) == sizeof(uint16)) \
			wreg16((volatile uint16 *)(void *)(r), (uint16)(v)); \
		else \
			wreg8((volatile uint8 *)(void *)(r), (uint8)(v)); \
	} while (0)

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* OSL initialization */
extern osl_t *osl_attach(void *pdev);
extern void osl_detach(osl_t *osh);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx) \
	do { \
	   osh->tx_fn = _tx_fn; \
	   osh->tx_ctx = _tx_ctx; \
	} while (0)

/* general purpose memory allocation */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#define	MALLOC(osh, size)	osl_malloc((osh), (size), __FILE__, __LINE__)
extern void *osl_malloc(osl_t *osh, uint size, char *file, int line);
#else
#define	MALLOC(osh, size)	osl_malloc((osh), (size))
extern void *osl_malloc(osl_t *osh, uint size);
#endif /* BCMDBG_MEM */

#define	MFREE(osh, addr, size)	osl_mfree((osh), (addr), (size))
#define	MALLOCED(osh)		osl_malloced((osh))
#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
#define	MALLOC_DUMP(osh, b)
extern int osl_mfree(osl_t *osh, void *addr, uint size);
extern uint osl_malloced(osl_t *osh);
extern uint osl_malloc_failed(osl_t *osh);
extern int osl_error(int bcmerror);

/* microsecond delay */
#define	OSL_DELAY(usec)		hndrte_delay(usec)

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)

/* uncached/cached virtual address */
#define	OSL_UNCACHED(va)	hndrte_uncached(va)
#define	OSL_CACHED(va)		hndrte_cached(va)

#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (uint32)(addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* allocate/free shared (dma-able) consistent (uncached) memory */
#define DMA_CONSISTENT_ALIGN_BITS	2
#define	DMA_CONSISTENT_ALIGN	(1 << DMA_CONSISTENT_ALIGN_BITS)

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	hndrte_dma_alloc_consistent(size, align, (tot), (void *)(pap), __FILE__, __LINE__)
#else
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	hndrte_dma_alloc_consistent(size, align, (tot), (void *)(pap))
#endif
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	hndrte_dma_free_consistent((void*)(va))

/* map/unmap direction */
#define	DMA_TX			1	/* TX direction for DMA */
#define	DMA_RX			2	/* RX direction for DMA */

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) do {} while (0)

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)		hndrte_reg_map(pa, size)
#define	REG_UNMAP(va)			hndrte_reg_unmap(va)

/* map/unmap shared (dma-able) memory */
#define	DMA_MAP(osh, va, size, direction, lb, dmah)	((dmaaddr_t)hndrte_dma_map(va, size))
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah)	hndrte_dma_unmap((uint32)pa, size)

/* shared (dma-able) memory access macros */
#define	R_SM(r)				*(r)
#define	W_SM(r, v)			(*(r) = (v))
#define	BZERO_SM(r, len)		memset((r), '\0', (len))

/* assert & debugging */
#define assfail	hndrte_assfail

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ			(MAXPKTBUFSZ - LBUFSZ)

/* packet primitives */
#define PKTGET(osh, len, send)		(void *)osl_pktget((osh), (len))
#define PKTFREE(osh, p, send)		osl_pktfree((osh), (p), (send))
#define	PKTDATA(osh, lb)		LBP(lb)->data
#define	PKTLEN(osh, lb)			LBP(lb)->len
#define	PKTHEADROOM(osh, lb)		(LBP(lb)->data - LBP(lb)->head)
#define	PKTTAILROOM(osh, lb)		(LBP(lb)->end - (LBP(lb)->data + LBP(lb)->len))
#define	PKTNEXT(osh, lb)		(LBP(lb)->next)
#define	PKTSETNEXT(osh, lb, x)		(LBP(lb)->next = LBP(x))
#define	PKTSETLEN(osh, lb, len)		lb_setlen(LBP(lb), (len))
#define	PKTPUSH(osh, lb, bytes)		lb_push(LBP(lb), (bytes))
#define	PKTPULL(osh, lb, bytes)		lb_pull(LBP(lb), (bytes))
#define PKTDUP(osh, p)			osl_pktdup((osh), (p))
#define	PKTTAG(lb)			((void *)((LBP(lb))->pkttag))
#define	PKTLINK(lb)			(LBP(lb)->link)
#define	PKTSETLINK(lb, x)		(LBP(lb)->link = LBP(x))
#define	PKTPRIO(lb)			lb_pri(LBP(lb))
#define	PKTSETPRIO(lb, x)		lb_setpri(LBP(lb), (x))
#define PKTSHARED(lb)                   (lb_isclone(LBP(lb)) || LBP(lb)->refcnt > 1)
#define PKTALLOCED(osh)			((osl_t *)osh)->pktalloced
#define PKTSUMNEEDED(lb)		lb_sumneeded(LBP(lb))
#define PKTSETSUMNEEDED(lb, x)		lb_setsumneeded(LBP(lb), (x))
#define PKTSUMGOOD(lb)			lb_sumgood(LBP(lb))
#define PKTSETSUMGOOD(lb, x)		lb_setsumgood(LBP(lb), (x))
#define PKTMSGTRACE(lb)			lb_msgtrace(LBP(lb))
#define PKTSETMSGTRACE(lb, x)		lb_setmsgtrace(LBP(lb), (x))
#define PKTDATAOFFSET(lb)		lb_dataoff(LBP(lb))
#define PKTSETDATAOFFSET(lb, dataOff)	lb_setdataoff(LBP(lb), dataOff)
#define PKTSETPOOL(osh, lb, x, y)	lb_setpool(LBP(lb), (x), (y))
#define PKTPOOL(osh, lb)		lb_pool(LBP(lb))
#ifdef BCMDBG_POOL
#define PKTPOOLSTATE(lb)		lb_poolstate(LBP(lb))
#define PKTPOOLSETSTATE(lb, s)		lb_setpoolstate(LBP(lb), s)
#endif

#define BCM_DMAPAD
#define PKTDMAPAD(osh, lb)		(LBP(lb)->dmapad)
#define PKTSETDMAPAD(osh, lb, pad)	(LBP(lb)->dmapad = pad)


#ifdef BCMDBG_PKT     /* pkt logging for debugging */
#define PKTLIST_DUMP(osh, buf)		(void)buf
#else /* BCMDBG_PKT */
#define PKTLIST_DUMP(osh, buf)
#endif /* BCMDBG_PKT */

#define PKTFRMNATIVE(osh, lb)		((void *)(osl_pktfrmnative((osh), (lb))))
#define PKTTONATIVE(osh, p)		((struct lbuf *)(osl_pkttonative((osh), (p))))


extern void * osl_pktfrmnative(osl_t *osh, struct lbuf *lb);
extern struct lbuf * osl_pkttonative(osl_t *osh, void *p);
extern void * osl_pktget(osl_t *osh, uint len);
extern void osl_pktfree(osl_t *osh, void *p, bool send);
extern void * osl_pktdup(osl_t *osh, void *p);
extern void * osl_pktclone(osl_t *osh, void *p, int offset, int len);

/* get system up time in milliseconds */
#define OSL_SYSUPTIME()		(hndrte_time())

/* Kernel: File Operations: start */
extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
/* Kernel: File Operations: end */

/* free memory available in pool */ 
#define OSL_MEM_AVAIL()         (hndrte_memavail())

#endif	/* _hndrte_osl_h_ */
