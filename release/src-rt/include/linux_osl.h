/*
 * Linux OS Independent Layer
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: linux_osl.h,v 13.160.2.9 2010-12-08 07:33:07 Exp $
 */

#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>

/* Linux Kernel: File Operations: start */
extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
extern int osl_os_image_size(void *image);
/* Linux Kernel: File Operations: end */

#ifdef BCMDRIVER

/* OSL initialization */
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);
extern void osl_detach(osl_t *osh);

/* Global ASSERT type */
extern uint32 g_assert_type;

/* ASSERT */
#if defined(BCMDBG_ASSERT)
	#define ASSERT(exp) \
	  do { if (!(exp)) osl_assert(#exp, __FILE__, __LINE__); } while (0)
extern void osl_assert(const char *exp, const char *file, int line);
#else
	#ifdef __GNUC__
		#define GCC_VERSION \
			(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
		#if GCC_VERSION > 30100
			#define ASSERT(exp)	do {} while (0)
		#else
			/* ASSERT could cause segmentation fault on GCC3.1, use empty instead */
			#define ASSERT(exp)
		#endif /* GCC_VERSION > 30100 */
	#endif /* __GNUC__ */
#endif 

/* microsecond delay */
#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	osl_pcmcia_read_attr((osh), (offset), (buf), (size))
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	osl_pcmcia_write_attr((osh), (offset), (buf), (size))
extern void osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size);
extern void osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size);

/* PCI configuration space access macros */
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

/* Pkttag flag should be part of public information */
typedef struct {
	bool pkttag;
	uint pktalloced; 	/* Number of allocated packet buffers */
	bool mmbus;		/* Bus supports memory-mapped register accesses */
	pktfree_cb_fn_t tx_fn;  /* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
#if defined(OSLREGOPS) || (defined(WLC_HIGH) && !defined(WLC_LOW))
	osl_rreg_fn_t rreg_fn;	/* Read Register function */
	osl_wreg_fn_t wreg_fn;	/* Write Register function */
	void *reg_ctx;		/* Context to the reg callback functions */
#else
	void	*unused[3];
#endif
} osl_pubinfo_t;

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->tx_fn = _tx_fn;	\
	   ((osl_pubinfo_t*)osh)->tx_ctx = _tx_ctx;	\
	} while (0)

#ifdef OSLREGOPS
#define REGOPSSET(osh, rreg, wreg, ctx)			\
	do {						\
	   ((osl_pubinfo_t*)osh)->rreg_fn = rreg;	\
	   ((osl_pubinfo_t*)osh)->wreg_fn = wreg;	\
	   ((osl_pubinfo_t*)osh)->reg_ctx = ctx;	\
	} while (0)
#endif /* OSLREGOPS */

/* host/bus architecture-specific byte swap */
#define BUS_SWAP32(v)		(v)

#ifdef BCMDBG_MEM
	#define MALLOC(osh, size)	osl_debug_malloc((osh), (size), __LINE__, __FILE__)
	#define MFREE(osh, addr, size)	osl_debug_mfree((osh), (addr), (size), __LINE__, __FILE__)
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MALLOC_DUMP(osh, b) 	osl_debug_memdump((osh), (b))
	extern void *osl_debug_malloc(osl_t *osh, uint size, int line, char* file);
	extern void osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, char* file);
	extern uint osl_malloced(osl_t *osh);
	struct bcmstrbuf;
	extern int osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b);
#else
	#define MALLOC(osh, size)	osl_malloc((osh), (size))
	#define MFREE(osh, addr, size)	osl_mfree((osh), (addr), (size))
	#define MALLOCED(osh)		osl_malloced((osh))
	extern void *osl_malloc(osl_t *osh, uint size);
	extern void osl_mfree(osl_t *osh, void *addr, uint size);
	extern uint osl_malloced(osl_t *osh);
#endif /* BCMDBG_MEM */

#define NATIVE_MALLOC(osh, size)		kmalloc(size, GFP_ATOMIC)
#define NATIVE_MFREE(osh, addr, size)	kfree(addr)

#if defined(USBAP) || defined(WL_VMEM_NVRAM_DECOMP)
#include <linuxver.h>		/* use current 2.4.x calling conventions */
#include <linux/vmalloc.h>
#define VMALLOC(osh, size)	vmalloc(size)
#define VFREE(osh, addr, size)	vfree(addr)
#endif

#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);

/* allocate/free shared (dma-able) consistent memory */
#define	DMA_CONSISTENT_ALIGN	osl_dma_consistent_align()
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))
extern uint osl_dma_consistent_align(void);
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align, uint *tot, ulong *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa);

/* map/unmap direction */
#define	DMA_TX	1	/* TX direction for DMA */
#define	DMA_RX	2	/* RX direction for DMA */

/* map/unmap shared (dma-able) memory */
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
extern uint osl_dma_map(osl_t *osh, void *va, uint size, int direction);
extern void osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction);

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) do {} while (0)

/* register access macros */
#if defined(BCMJTAG)
	#include <bcmjtag.h>
	#define OSL_WRITE_REG(osh, r, v) (bcmjtag_write(NULL, (uintptr)(r), (v), sizeof(*(r))))
	#define OSL_READ_REG(osh, r) (bcmjtag_read(NULL, (uintptr)(r), sizeof(*(r))))
#endif 

#if defined(BCMJTAG)
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) if (((osl_pubinfo_t*)(osh))->mmbus) \
		mmap_op else bus_op
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) (((osl_pubinfo_t*)(osh))->mmbus) ? \
		mmap_op : bus_op
#else
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) mmap_op
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) mmap_op
#endif 

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ	2048   /* largest reasonable packet buffer, driver uses for ethernet MTU */

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 * Macros expand to calls to functions defined in linux_osl.c .
 */
#ifndef BINOSL

#include <linuxver.h>		/* use current 2.4.x calling conventions */
#include <linux/kernel.h>	/* for vsn/printf's */
#include <linux/string.h>	/* for mem*, str* */

#define OSL_SYSUPTIME()		((uint32)jiffies * (1000 / HZ))
#define	printf(fmt, args...)	printk(fmt , ## args)

/* bcopy's: Linux kernel doesn't provide these (anymore) */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

extern uint8 osl_readb(osl_t *osh, volatile uint8 *r);
extern uint16 osl_readw(osl_t *osh, volatile uint16 *r);
extern uint32 osl_readl(osl_t *osh, volatile uint32 *r);
extern void osl_writeb(osl_t *osh, volatile uint8 *r, uint8 v);
extern void osl_writew(osl_t *osh, volatile uint16 *r, uint16 v);
extern void osl_writel(osl_t *osh, volatile uint32 *r, uint32 v);
/* register access macros */
#if defined(OSLREGOPS)
#define R_REG(osh, r) (\
	sizeof(*(r)) == sizeof(uint8) ? osl_readb((osh), (volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? osl_readw((osh), (volatile uint16*)(r)) : \
	osl_readl((osh), (volatile uint32*)(r)) \
)
#define W_REG(osh, r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	osl_writeb((osh), (volatile uint8*)(r), (uint8)(v)); break; \
	case sizeof(uint16):	osl_writew((osh), (volatile uint16*)(r), (uint16)(v)); break; \
	case sizeof(uint32):	osl_writel((osh), (volatile uint32*)(r), (uint32)(v)); break; \
	} \
} while (0)


#else /* OSLREGOPS */

#ifndef IL_BIGENDIAN
#ifndef __mips__
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, sizeof(*(r)) == sizeof(uint8) ? readb((volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? readw((volatile uint16*)(r)) : \
	readl((volatile uint32*)(r)), OSL_READ_REG(osh, r)) \
)
#else /* __mips__ */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v; \
			__asm__ __volatile__("sync"); \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__asm__ __volatile__("sync"); \
			__osl_v; \
		}), \
		({ \
			__typeof(*(r)) __osl_v; \
			__asm__ __volatile__("sync"); \
			__osl_v = OSL_READ_REG(osh, r); \
			__asm__ __volatile__("sync"); \
			__osl_v; \
		})) \
)
#endif /* __mips__ */

#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh,  \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
			case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
			case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#else	/* IL_BIGENDIAN */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)((uintptr)(r)^3)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)((uintptr)(r)^2)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh,  \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), \
					(volatile uint8*)((uintptr)(r)^3)); break; \
			case sizeof(uint16):	writew((uint16)(v), \
					(volatile uint16*)((uintptr)(r)^2)); break; \
			case sizeof(uint32):	writel((uint32)(v), \
					(volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif /* IL_BIGENDIAN */

#endif /* OSLREGOPS */

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* bcopy, bcmp, and bzero functions */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

/* uncached/cached virtual address */
#ifdef __mips__
#include <asm/addrspace.h>
#define OSL_UNCACHED(va)	((void *)KSEG1ADDR((va)))
#define OSL_CACHED(va)		((void *)KSEG0ADDR((va)))
#else
#define OSL_UNCACHED(va)	((void *)va)
#define OSL_CACHED(va)		((void *)va)
#endif /* mips */

#ifdef __mips__
#define OSL_PREF_RANGE_LD(va, sz) prefetch_range_PREF_LOAD_RETAINED(va, sz)
#define OSL_PREF_RANGE_ST(va, sz) prefetch_range_PREF_STORE_RETAINED(va, sz)
#else /* __mips__ */
#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)
#endif /* __mips__ */

/* get processor cycle count */
#if defined(mips)
#if defined DSLCPE_DELAY
#define	OSL_GETCYCLES(x)	((x) = read_c0_count())
#define TICKDIFF(_x2_, _x1_)	\
	((_x2_ >= _x1_) ? (_x2_ - _x1_) : ((unsigned long)(-1) - _x2_ + _x1_))
#else
#define	OSL_GETCYCLES(x)	((x) = read_c0_count() * 2)
#endif
#elif defined(__i386__)
#define	OSL_GETCYCLES(x)	rdtscl((x))
#else
#define OSL_GETCYCLES(x)	((x) = 0)
#endif /* defined(mips) */

/* dereference an address that may cause a bus exception */
#ifdef mips
#if defined(MODULE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 17))
#define BUSPROBE(val, addr)	panic("get_dbe() will not fixup a bus exception when compiled into"\
					" a module")
#else
#define	BUSPROBE(val, addr)	get_dbe((val), (addr))
#include <asm/paccess.h>
#endif /* defined(MODULE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 17)) */
#else
#define	BUSPROBE(val, addr)	({ (val) = R_REG(NULL, (addr)); 0; })
#endif /* mips */

/* map/unmap physical to virtual I/O */
#if !defined(CONFIG_MMC_MSM7X00A)
#define	REG_MAP(pa, size)	ioremap_nocache((unsigned long)(pa), (unsigned long)(size))
#else
#define REG_MAP(pa, size)       (void *)(0)
#endif /* !defined(CONFIG_MMC_MSM7X00A */
#define	REG_UNMAP(va)		iounmap((va))

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	memset((r), '\0', (len))

/* packet primitives */
#ifndef BCMDBG_PKT
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define PKTLIST_DUMP(osh, buf)
#define PKTDBG_TRACE(osh, pkt, bit)
#else /* BCMDBG_PKT pkt logging for debugging */
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len), __LINE__, __FILE__)
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb), __LINE__, __FILE__)
#define PKTLIST_DUMP(osh, buf) 		osl_pktlist_dump(osh, buf)
#define BCMDBG_PTRACE
#define PKTLIST_IDX(skb)		((uint16 *)((char *)PKTTAG(skb) + \
					sizeof(((struct sk_buff*)(skb))->cb) - sizeof(uint16)))
#define PKTDBG_TRACE(osh, pkt, bit)     osl_pkttrace(osh, pkt, bit)
#endif /* BCMDBG_PKT */
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#define	PKTDATA(osh, skb)		(((struct sk_buff*)(skb))->data)
#define	PKTLEN(osh, skb)		(((struct sk_buff*)(skb))->len)
#define PKTHEADROOM(osh, skb)		(PKTDATA(osh, skb)-(((struct sk_buff*)(skb))->head))
#define PKTTAILROOM(osh, skb) ((((struct sk_buff*)(skb))->end)-(((struct sk_buff*)(skb))->tail))
#define	PKTNEXT(osh, skb)		(((struct sk_buff*)(skb))->next)
#define	PKTSETNEXT(osh, skb, x)		(((struct sk_buff*)(skb))->next = (struct sk_buff*)(x))
#define	PKTSETLEN(osh, skb, len)	__skb_trim((struct sk_buff*)(skb), (len))
#define	PKTPUSH(osh, skb, bytes)	skb_push((struct sk_buff*)(skb), (bytes))
#define	PKTPULL(osh, skb, bytes)	skb_pull((struct sk_buff*)(skb), (bytes))
#define	PKTTAG(skb)			((void*)(((struct sk_buff*)(skb))->cb))
#define PKTALLOCED(osh)			((osl_pubinfo_t *)(osh))->pktalloced
#define PKTSETPOOL(osh, skb, x, y)	do {} while (0)
#define PKTPOOL(osh, skb)		FALSE

#ifdef CTFPOOL
#define	CTFPOOL_REFILL_THRESH	3
typedef struct ctfpool {
	void		*head;
	spinlock_t	lock;
	uint		max_obj;
	uint		curr_obj;
	uint		obj_size;
	uint		refills;
	uint		fast_allocs;
	uint 		fast_frees;
	uint 		slow_allocs;
} ctfpool_t;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	FASTBUF	(1 << 4)
#define	CTFBUF	(1 << 5)
#define	PKTSETFAST(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) |= FASTBUF)
#define	PKTCLRFAST(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) &= (~FASTBUF))
#define	PKTSETCTF(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) |= CTFBUF)
#define	PKTCLRCTF(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) &= (~CTFBUF))
#define	PKTISFAST(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) & FASTBUF)
#define	PKTISCTF(osh, skb)	((((struct sk_buff*)(skb))->ctf_mac_len) & CTFBUF)
#define	PKTFAST(osh, skb)	(((struct sk_buff*)(skb))->ctf_mac_len)
#else
#define	FASTBUF	(1 << 0)
#define	CTFBUF	(1 << 1)
#define	PKTSETFAST(osh, skb)	((((struct sk_buff*)(skb))->__unused) |= FASTBUF)
#define	PKTCLRFAST(osh, skb)	((((struct sk_buff*)(skb))->__unused) &= (~FASTBUF))
#define	PKTSETCTF(osh, skb)	((((struct sk_buff*)(skb))->__unused) |= CTFBUF)
#define	PKTCLRCTF(osh, skb)	((((struct sk_buff*)(skb))->__unused) &= (~CTFBUF))
#define	PKTISFAST(osh, skb)	((((struct sk_buff*)(skb))->__unused) & FASTBUF)
#define	PKTISCTF(osh, skb)	((((struct sk_buff*)(skb))->__unused) & CTFBUF)
#define	PKTFAST(osh, skb)	(((struct sk_buff*)(skb))->__unused)
#endif /* 2.6.22 */

#define	CTFPOOLPTR(osh, skb)	(((struct sk_buff*)(skb))->sk)
#define	CTFPOOLHEAD(osh, skb)	(((ctfpool_t *)((struct sk_buff*)(skb))->sk)->head)

extern void *osl_ctfpool_add(osl_t *osh);
extern void osl_ctfpool_replenish(osl_t *osh, uint thresh);
extern int32 osl_ctfpool_init(osl_t *osh, uint numobj, uint size);
extern void osl_ctfpool_cleanup(osl_t *osh);
extern void osl_ctfpool_stats(osl_t *osh, void *b);
#endif /* CTFPOOL */

#ifdef CTFMAP
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#define CTFMAPPTR(osh, skb) 	(((struct sk_buff*)(skb))->sp)
#else /* 2.6.14 */
#define CTFMAPPTR(osh, skb)	(((struct sk_buff*)(skb))->list)
#endif /* 2.6.14 */

#define PKTCTFMAP(osh, p) \
do { \
	if (PKTISCTF(osh, p)) { \
		int32 sz; \
		sz = (uint32)(((struct sk_buff *)p)->end) - \
		     (uint32)CTFMAPPTR(osh, p); \
		/* map the remaining unmapped area */ \
		if (sz > 0) { \
			_DMA_MAP(osh, (void *)CTFMAPPTR(osh, p), \
				sz, DMA_RX, p, NULL); \
		} \
		/* clear ctf buf flag */ \
		PKTCLRCTF(osh, p); \
		CTFMAPPTR(osh, p) = NULL; \
	} \
} while (0)
#endif /* CTFMAP */

#ifdef HNDCTF
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	SKIPCT	(1 << 6)
#define	PKTSETSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->ctf_mac_len |= SKIPCT)
#define	PKTCLRSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->ctf_mac_len &= (~SKIPCT))
#define	PKTSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->ctf_mac_len & SKIPCT)
#else /* 2.6.22 */
#define	SKIPCT	(1 << 2)
#define	PKTSETSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->__unused |= SKIPCT)
#define	PKTCLRSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->__unused &= (~SKIPCT))
#define	PKTSKIPCT(osh, skb)	(((struct sk_buff*)(skb))->__unused & SKIPCT)
#endif /* 2.6.22 */
#else /* HNDCTF */
#define	PKTSETSKIPCT(osh, skb)
#define	PKTCLRSKIPCT(osh, skb)
#define	PKTSKIPCT(osh, skb)
#endif /* HNDCTF */

extern void osl_pktfree(osl_t *osh, void *skb, bool send);

#ifdef BCMDBG_PKT     /* pkt logging for debugging */
extern void *osl_pktget(osl_t *osh, uint len, int line, char *file);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb, int line, char *file);
extern void *osl_pktdup(osl_t *osh, void *skb, int line, char *file);
extern void osl_pktlist_add(osl_t *osh, void *p, int line, char *file);
extern void osl_pktlist_remove(osl_t *osh, void *p);
extern char *osl_pktlist_dump(osl_t *osh, char *buf);
#ifdef BCMDBG_PTRACE
extern void osl_pkttrace(osl_t *osh, void *pkt, uint16 bit);
#endif /* BCMDBG_PTRACE */
#else /* BCMDBG_PKT */
extern void *osl_pkt_frmnative(osl_t *osh, void *skb);
extern void *osl_pktget(osl_t *osh, uint len);
extern void *osl_pktdup(osl_t *osh, void *skb);
#endif /* BCMDBG_PKT */
extern struct sk_buff *osl_pkt_tonative(osl_t *osh, void *pkt);
#ifdef BCMDBG_PKT
#define PKTFRMNATIVE(osh, skb)  osl_pkt_frmnative(((osl_t *)osh), \
				(struct sk_buff*)(skb), __LINE__, __FILE__)
#else /* BCMDBG_PKT */
#define PKTFRMNATIVE(osh, skb)	osl_pkt_frmnative(((osl_t *)osh), (struct sk_buff*)(skb))
#endif /* BCMDBG_PKT */
#define PKTTONATIVE(osh, pkt)		osl_pkt_tonative((osl_t *)(osh), (pkt))

#define	PKTLINK(skb)			(((struct sk_buff*)(skb))->prev)
#define	PKTSETLINK(skb, x)		(((struct sk_buff*)(skb))->prev = (struct sk_buff*)(x))
#define	PKTPRIO(skb)			(((struct sk_buff*)(skb))->priority)
#define	PKTSETPRIO(skb, x)		(((struct sk_buff*)(skb))->priority = (x))
#define PKTSUMNEEDED(skb)		(((struct sk_buff*)(skb))->ip_summed == CHECKSUM_HW)
#define PKTSETSUMGOOD(skb, x)		(((struct sk_buff*)(skb))->ip_summed = \
						((x) ? CHECKSUM_UNNECESSARY : CHECKSUM_NONE))
/* PKTSETSUMNEEDED and PKTSUMGOOD are not possible because skb->ip_summed is overloaded */
#define PKTSHARED(skb)                  (((struct sk_buff*)(skb))->cloned)

#if defined(DSLCPE_DELAY)
#include <linux/spinlock.h>     /* for spinlock_t */
struct shared_osl {
	int long_delay;
	spinlock_t *lock;
	void *wl;
	unsigned long MIPS;
};
typedef struct shared_osl shared_osl_t;

#define	OSL_LONG_DELAY(osh, usec)		osl_long_delay(osh, usec, 0)
#define OSL_YIELD_EXEC(osh, usec)		osl_long_delay(osh, usec, 1)
extern void osl_long_delay(osl_t *osh, uint usec, bool yield);
extern int in_long_delay(osl_t *osh);
extern void osl_oshsh_init(osl_t *osh, shared_osl_t *oshsh);
#define IN_LONG_DELAY(osh)	in_long_delay(osh)
#endif /* DSLCPE_DELAY */

#else	/* BINOSL */

/* Where to get the declarations for mem, str, printf, bcopy's? Two basic approaches.
 *
 * First, use the Linux header files and the C standard library replacmenent versions
 * built-in to the kernel.  Use this approach when compiling non hybrid code or compling
 * the OS port files.  The second approach is to use our own defines/prototypes and
 * functions we have provided in the Linux OSL, i.e. linux_osl.c.  Use this approach when
 * compiling the files that make up the hybrid binary.  We are ensuring we
 * don't directly link to the kernel replacement routines from the hybrid binary.
 *
 * NOTE: The issue we are trying to avoid is any questioning of whether the
 * hybrid binary is derived from Linux.  The wireless common code (wlc) is designed
 * to be OS independent through the use of the OSL API and thus the hybrid binary doesn't
 * derive from the Linux kernel at all.  But since we defined our OSL API to include
 * a small collection of standard C library routines and these routines are provided in
 * the kernel we want to avoid even the appearance of deriving at all even though clearly
 * usage of a C standard library API doesn't represent a derivation from Linux.  Lastly
 * note at the time of this checkin 4 references to memcpy/memset could not be eliminated
 * from the binary because they are created internally by GCC as part of things like
 * structure assignment.  I don't think the compiler should be doing this but there is
 * no options to disable it on Intel architectures (there is for MIPS so somebody must
 * agree with me).  I may be able to even remove these references eventually with
 * a GNU binutil such as objcopy via a symbol rename (i.e. memcpy to osl_memcpy).
 */
#if !defined(LINUX_HYBRID) || defined(LINUX_PORT)
	#define	printf(fmt, args...)	printk(fmt , ## args)
	#include <linux/kernel.h>	/* for vsn/printf's */
	#include <linux/string.h>	/* for mem*, str* */
	/* bcopy's: Linux kernel doesn't provide these (anymore) */
	#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
	#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
	#define	bzero(b, len)		memset((b), '\0', (len))

	/* These are provided only because when compiling linux_osl.c there
	 * must be an explicit prototype (separate from the definition) because
	 * we are compiling with GCC option -Wstrict-prototypes.  Conversely
	 * these could be placed directly in linux_osl.c.
	 */
	extern int osl_printf(const char *format, ...);
	extern int osl_sprintf(char *buf, const char *format, ...);
	extern int osl_snprintf(char *buf, size_t n, const char *format, ...);
	extern int osl_vsprintf(char *buf, const char *format, va_list ap);
	extern int osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap);
	extern int osl_strcmp(const char *s1, const char *s2);
	extern int osl_strncmp(const char *s1, const char *s2, uint n);
	extern int osl_strlen(const char *s);
	extern char* osl_strcpy(char *d, const char *s);
	extern char* osl_strncpy(char *d, const char *s, uint n);
	extern char* osl_strchr(const char *s, int c);
	extern char* osl_strrchr(const char *s, int c);
	extern void *osl_memset(void *d, int c, size_t n);
	extern void *osl_memcpy(void *d, const void *s, size_t n);
	extern void *osl_memmove(void *d, const void *s, size_t n);
	extern int osl_memcmp(const void *s1, const void *s2, size_t n);
#else

	/* In the below defines we undefine the macro first in case it is
	 * defined.  This shouldn't happen because we are not using Linux
	 * header files but because our Linux 2.4 make includes modversions.h
	 * through a GCC -include compile option, they get defined to point
	 * at the appropriate versioned symbol name.  Note this doesn't
	 * happen with our Linux 2.6 makes.
	 */

	/* *printf functions */
	#include <stdarg.h>			/* va_list needed for v*printf */
	#include <stddef.h>			/* size_t needed for *nprintf */
	#undef printf
	#undef sprintf
	#undef snprintf
	#undef vsprintf
	#undef vsnprintf
	#define	printf(fmt, args...)		osl_printf((fmt) , ## args)
	#define sprintf(buf, fmt, args...)	osl_sprintf((buf), (fmt) , ## args)
	#define snprintf(buf, n, fmt, args...)	osl_snprintf((buf), (n), (fmt) , ## args)
	#define vsprintf(buf, fmt, ap)		osl_vsprintf((buf), (fmt), (ap))
	#define vsnprintf(buf, n, fmt, ap)	osl_vsnprintf((buf), (n), (fmt), (ap))
	extern int osl_printf(const char *format, ...);
	extern int osl_sprintf(char *buf, const char *format, ...);
	extern int osl_snprintf(char *buf, size_t n, const char *format, ...);
	extern int osl_vsprintf(char *buf, const char *format, va_list ap);
	extern int osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap);

	/* str* functions */
	#undef strcmp
	#undef strncmp
	#undef strlen
	#undef strcpy
	#undef strncpy
	#undef strchr
	#undef strrchr
	#define	strcmp(s1, s2)			osl_strcmp((s1), (s2))
	#define	strncmp(s1, s2, n)		osl_strncmp((s1), (s2), (n))
	#define strlen(s)			osl_strlen((s))
	#define	strcpy(d, s)			osl_strcpy((d), (s))
	#define	strncpy(d, s, n)		osl_strncpy((d), (s), (n))
	#define	strchr(s, c)			osl_strchr((s), (c))
	#define	strrchr(s, c)			osl_strrchr((s), (c))
	extern int osl_strcmp(const char *s1, const char *s2);
	extern int osl_strncmp(const char *s1, const char *s2, uint n);
	extern int osl_strlen(const char *s);
	extern char* osl_strcpy(char *d, const char *s);
	extern char* osl_strncpy(char *d, const char *s, uint n);
	extern char* osl_strchr(const char *s, int c);
	extern char* osl_strrchr(const char *s, int c);

	/* mem* functions */
	#undef memset
	#undef memcpy
	#undef memcmp
	#define	memset(d, c, n)		osl_memset((d), (c), (n))
	#define	memcpy(d, s, n)		osl_memcpy((d), (s), (n))
	#define	memmove(d, s, n)	osl_memmove((d), (s), (n))
	#define	memcmp(s1, s2, n)	osl_memcmp((s1), (s2), (n))
	extern void *osl_memset(void *d, int c, size_t n);
	extern void *osl_memcpy(void *d, const void *s, size_t n);
	extern void *osl_memmove(void *d, const void *s, size_t n);
	extern int osl_memcmp(const void *s1, const void *s2, size_t n);

	/* bcopy, bcmp, and bzero functions */
	#undef bcopy
	#undef bcmp
	#undef bzero
	#define	bcopy(src, dst, len)	osl_memcpy((dst), (src), (len))
	#define	bcmp(b1, b2, len)	osl_memcmp((b1), (b2), (len))
	#define	bzero(b, len)		osl_memset((b), '\0', (len))
#endif /* !defined(LINUX_HYBRID) || defined(LINUX_PORT) */

/* register access macros */
#if !defined(BCMJTAG)
#define R_REG(osh, r) (\
	sizeof(*(r)) == sizeof(uint8) ? osl_readb((volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? osl_readw((volatile uint16*)(r)) : \
	osl_readl((volatile uint32*)(r)) \
)
#define W_REG(osh, r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	osl_writeb((uint8)(v), (volatile uint8*)(r)); break; \
	case sizeof(uint16):	osl_writew((uint16)(v), (volatile uint16*)(r)); break; \
	case sizeof(uint32):	osl_writel((uint32)(v), (volatile uint32*)(r)); break; \
	} \
} while (0)

/* else added by johnvb to make sdio and jtag work with BINOSL, at least compile ... UNTESTED */
#else
#define R_REG(osh, r) OSL_READ_REG(osh, r)
#define W_REG(osh, r, v) do { OSL_WRITE_REG(osh, r, v); } while (0)
#endif 

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))
extern uint8 osl_readb(volatile uint8 *r);
extern uint16 osl_readw(volatile uint16 *r);
extern uint32 osl_readl(volatile uint32 *r);
extern void osl_writeb(uint8 v, volatile uint8 *r);
extern void osl_writew(uint16 v, volatile uint16 *r);
extern void osl_writel(uint32 v, volatile uint32 *r);

/* system up time in ms */
#define OSL_SYSUPTIME()		osl_sysuptime()
extern uint32 osl_sysuptime(void);

/* uncached/cached virtual address */
#define OSL_UNCACHED(va)	osl_uncached((va))
extern void *osl_uncached(void *va);
#define OSL_CACHED(va)		osl_cached((va))
extern void *osl_cached(void *va);

#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
extern uint osl_getcycles(void);

/* dereference an address that may target abort */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* map/unmap physical to virtual */
#define	REG_MAP(pa, size)	osl_reg_map((pa), (size))
#define	REG_UNMAP(va)		osl_reg_unmap((va))
extern void *osl_reg_map(uint32 pa, uint size);
extern void osl_reg_unmap(void *va);

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	bzero((r), (len))

/* packet primitives */
#ifdef BCMDBG_PKT
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len), __LINE__, __FILE__)
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb), __LINE__, __FILE__)
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (skb), __LINE__, __FILE__)
#define PKTLIST_DUMP(osh, buf) 		osl_pktlist_dump(osh, buf)
#define PKTDBG_TRACE(osh, pkt, bit)
#else /* BCMDBG_PKT */
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (skb))
#define PKTLIST_DUMP(osh, buf)
#define PKTDBG_TRACE(osh, pkt, bit)
#endif /* BCMDBG_PKT */
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#define	PKTDATA(osh, skb)		osl_pktdata((osh), (skb))
#define	PKTLEN(osh, skb)		osl_pktlen((osh), (skb))
#define PKTHEADROOM(osh, skb)		osl_pktheadroom((osh), (skb))
#define PKTTAILROOM(osh, skb)		osl_pkttailroom((osh), (skb))
#define	PKTNEXT(osh, skb)		osl_pktnext((osh), (skb))
#define	PKTSETNEXT(osh, skb, x)		osl_pktsetnext((skb), (x))
#define	PKTSETLEN(osh, skb, len)	osl_pktsetlen((osh), (skb), (len))
#define	PKTPUSH(osh, skb, bytes)	osl_pktpush((osh), (skb), (bytes))
#define	PKTPULL(osh, skb, bytes)	osl_pktpull((osh), (skb), (bytes))
#define PKTTAG(skb)			osl_pkttag((skb))
#define PKTTONATIVE(osh, pkt)		osl_pkt_tonative((osh), (pkt))
#define	PKTLINK(skb)			osl_pktlink((skb))
#define	PKTSETLINK(skb, x)		osl_pktsetlink((skb), (x))
#define	PKTPRIO(skb)			osl_pktprio((skb))
#define	PKTSETPRIO(skb, x)		osl_pktsetprio((skb), (x))
#define PKTSHARED(skb)                  osl_pktshared((skb))
#define PKTALLOCED(osh)			osl_pktalloced((osh))
#define PKTSETPOOL(osh, skb, x, y)	do {} while (0)
#define PKTPOOL(osh, skb)		FALSE

#ifdef BCMDBG_PKT     /* pkt logging for debugging */
extern void *osl_pktget(osl_t *osh, uint len, int line, char *file);
extern void *osl_pktdup(osl_t *osh, void *skb, int line, char *file);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb, int line, char *file);
#else /* BCMDBG_PKT */
extern void *osl_pktget(osl_t *osh, uint len);
extern void *osl_pktdup(osl_t *osh, void *skb);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb);
#endif /* BCMDBG_PKT */
extern void osl_pktfree(osl_t *osh, void *skb, bool send);
extern uchar *osl_pktdata(osl_t *osh, void *skb);
extern uint osl_pktlen(osl_t *osh, void *skb);
extern uint osl_pktheadroom(osl_t *osh, void *skb);
extern uint osl_pkttailroom(osl_t *osh, void *skb);
extern void *osl_pktnext(osl_t *osh, void *skb);
extern void osl_pktsetnext(void *skb, void *x);
extern void osl_pktsetlen(osl_t *osh, void *skb, uint len);
extern uchar *osl_pktpush(osl_t *osh, void *skb, int bytes);
extern uchar *osl_pktpull(osl_t *osh, void *skb, int bytes);
extern void *osl_pkttag(void *skb);
extern void *osl_pktlink(void *skb);
extern void osl_pktsetlink(void *skb, void *x);
extern uint osl_pktprio(void *skb);
extern void osl_pktsetprio(void *skb, uint x);
extern struct sk_buff *osl_pkt_tonative(osl_t *osh, void *pkt);
extern bool osl_pktshared(void *skb);
extern uint osl_pktalloced(osl_t *osh);

#ifdef BCMDBG_PKT     /* pkt logging for debugging */
extern char *osl_pktlist_dump(osl_t *osh, char *buf);
extern void osl_pktlist_add(osl_t *osh, void *p, int line, char *file);
extern void osl_pktlist_remove(osl_t *osh, void *p);
#endif /* BCMDBG_PKT */

#endif	/* BINOSL */

#ifdef CTFMAP
#include <ctf/hndctf.h>
#define CTFMAPSZ 	320
#define DMA_MAP(osh, va, size, direction, p, dmah) \
({ \
	typeof(size) sz = (size); \
	if (PKTISCTF((osh), (p))) { \
		sz = CTFMAPSZ; \
		CTFMAPPTR((osh), (p)) = (void *)(((uint8 *)(va)) + CTFMAPSZ); \
	} \
	osl_dma_map((osh), (va), sz, (direction)); \
})
#define _DMA_MAP(osh, va, size, direction, p, dmah) \
	dma_cache_inv((uint)(va), (size))
#else /* CTFMAP */
#define DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction))
#endif /* CTFMAP */

#else /* ! BCMDRIVER */


/* ASSERT */
#ifdef BCMDBG_ASSERT
	#include <assert.h>
	#define ASSERT assert
#else /* BCMDBG_ASSERT */
	#define ASSERT(exp)	do {} while (0)
#endif /* BCMDBG_ASSERT */

/* MALLOC and MFREE */
#define MALLOC(o, l) malloc(l)
#define MFREE(o, p, l) free(p)
#include <stdlib.h>

/* str* and mem* functions */
#include <string.h>

/* *printf functions */
#include <stdio.h>

/* bcopy, bcmp, and bzero */
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
#endif /* ! BCMDRIVER */

#endif	/* _linux_osl_h_ */
