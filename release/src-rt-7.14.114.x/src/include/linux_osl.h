/*
 * Linux OS Independent Layer
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
 * $Id: linux_osl.h 573045 2015-07-21 20:17:41Z $
 */

#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>

#define DECLSPEC_ALIGN(x)	__attribute__ ((aligned(x)))

/* Linux Kernel: File Operations: start */
extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
extern int osl_os_image_size(void *image);
/* Linux Kernel: File Operations: end */

#ifdef BCMDRIVER

/* OSL initialization */
#ifdef SHARED_OSL_CMN
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag, void **osh_cmn);
#else
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);
#endif /* SHARED_OSL_CMN */

extern void osl_detach(osl_t *osh);
extern int osl_static_mem_init(osl_t *osh, void *adapter);
extern int osl_static_mem_deinit(osl_t *osh, void *adapter);
extern void osl_set_bus_handle(osl_t *osh, void *bus_handle);
extern void* osl_get_bus_handle(osl_t *osh);

/* Global ASSERT type */
extern uint32 g_assert_type;

/* ASSERT */
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

/* bcm_prefetch_32B */
static inline void bcm_prefetch_32B(const uint8 *addr, const int cachelines_32B)
{
#if defined(BCM47XX_CA9) && (__LINUX_ARM_ARCH__ >= 5)
	switch (cachelines_32B) {
		case 4: __asm__ __volatile__("pld\t%a0" :: "p"(addr + 96) : "cc");
		case 3: __asm__ __volatile__("pld\t%a0" :: "p"(addr + 64) : "cc");
		case 2: __asm__ __volatile__("pld\t%a0" :: "p"(addr + 32) : "cc");
		case 1: __asm__ __volatile__("pld\t%a0" :: "p"(addr +  0) : "cc");
	}
#elif defined(__mips__)
	/* Hint Pref_Load = 0 */
	switch (cachelines_32B) {
		case 4: __asm__ __volatile__("pref %0, (%1)" :: "i"(0), "r"(addr + 96));
		case 3: __asm__ __volatile__("pref %0, (%1)" :: "i"(0), "r"(addr + 64));
		case 2: __asm__ __volatile__("pref %0, (%1)" :: "i"(0), "r"(addr + 32));
		case 1: __asm__ __volatile__("pref %0, (%1)" :: "i"(0), "r"(addr +  0));
	}
#endif /* BCM47XX_CA9, __mips__ */
}

/* microsecond delay */
#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#define OSL_SLEEP(ms)			osl_sleep(ms)
extern void osl_sleep(uint ms);

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
#define OSL_PCIE_DOMAIN(osh)	osl_pcie_domain(osh)
#define OSL_PCIE_BUS(osh)	osl_pcie_bus(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
extern uint osl_pcie_domain(osl_t *osh);
extern uint osl_pcie_bus(osl_t *osh);
extern struct pci_dev *osl_pci_device(osl_t *osh);

/* Flags that can be used to handle OSL specifcs */
#define OSL_PHYS_MEM_LESS_THAN_16MB	(1<<0L)
#define OSL_ACP_COHERENCE		(1<<1L)

/* Pkttag flag should be part of public information */
typedef struct {
	bool pkttag;
	bool mmbus;		/* Bus supports memory-mapped register accesses */
	pktfree_cb_fn_t tx_fn;  /* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
#ifdef OSLREGOPS
	osl_rreg_fn_t rreg_fn;	/* Read Register function */
	osl_wreg_fn_t wreg_fn;	/* Write Register function */
	void *reg_ctx;		/* Context to the reg callback functions */
#else
	void	*unused[3];
#endif
} osl_pubinfo_t;

extern void osl_flag_set(osl_t *osh, uint32 mask);
extern bool osl_is_flag_set(osl_t *osh, uint32 mask);

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

	#define MALLOC(osh, size)	osl_malloc((osh), (size))
	#define MALLOCZ(osh, size)	osl_mallocz((osh), (size))
	#define MFREE(osh, addr, size)	osl_mfree((osh), (addr), (size))
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MEMORY_LEFTOVER(osh) osl_check_memleak(osh)
	extern void *osl_malloc(osl_t *osh, uint size);
	extern void *osl_mallocz(osl_t *osh, uint size);
	extern void osl_mfree(osl_t *osh, void *addr, uint size);
	extern uint osl_malloced(osl_t *osh);
	extern uint osl_check_memleak(osl_t *osh);

#define	NATIVE_MALLOC(osh, size)	({BCM_REFERENCE(osh); kmalloc(size, GFP_ATOMIC);})
#define	NATIVE_MFREE(osh, addr, size)	({BCM_REFERENCE(osh); BCM_REFERENCE(size); kfree(addr);})

#ifdef USBAP
#include <linux/vmalloc.h>
#define	VMALLOC(osh, size)	({BCM_REFERENCE(osh); vmalloc(size);})
#define	VFREE(osh, addr, size)	({BCM_REFERENCE(osh); BCM_REFERENCE(size); vfree(addr);})
#endif /* USBAP */

#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);

/* allocate/free shared (dma-able) consistent memory */
#define	DMA_CONSISTENT_ALIGN	osl_dma_consistent_align()
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

#define	DMA_ALLOC_CONSISTENT_FORCE32(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT_FORCE32(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

extern uint osl_dma_consistent_align(void);
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align,
	uint *tot, dmaaddr_t *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, dmaaddr_t pa);

/* map/unmap direction */
#define	DMA_TX	1	/* TX direction for DMA */
#define	DMA_RX	2	/* RX direction for DMA */

/* map/unmap shared (dma-able) memory */
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
#define	SECURE_DMA_UNMAP(osh, pa, size, direction, p, dmah, pcma, offset) \
	osl_sec_dma_unmap((osh), (pa), (size), (direction), (p), (dmah), (pcma), (offset))

extern dmaaddr_t osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern void osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction);

#define PHYS_TO_PAGE(pa) pfn_to_page(PFN_DOWN((u32)pa))

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) ({BCM_REFERENCE(osh); BCM_REFERENCE(addrwidth);})

/* API for CPU relax */
extern void osl_cpu_relax(void);
#define OSL_CPU_RELAX() osl_cpu_relax()

#if defined(__mips__) || defined(__ARM_ARCH_7A__)
	extern void osl_cache_flush(void *va, uint size);
	extern void osl_cache_inv(void *va, uint size);
	extern void osl_prefetch(const void *ptr);
	#define OSL_CACHE_FLUSH(va, len)	osl_cache_flush((void *) va, len)
	#define OSL_CACHE_INV(va, len)		osl_cache_inv((void *) va, len)
	#define OSL_PREFETCH(ptr)			osl_prefetch(ptr)
#ifdef __ARM_ARCH_7A__
	extern int osl_arch_is_coherent(void);
	#define OSL_ARCH_IS_COHERENT()		osl_arch_is_coherent()
	extern int osl_acp_war_enab(void);
	#define OSL_ACP_WAR_ENAB()			osl_acp_war_enab()
#else
	#define OSL_ARCH_IS_COHERENT()		NULL
	#define OSL_ACP_WAR_ENAB()			NULL
#endif /* __ARM_ARCH_7A__ */
#else
	#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
	#define OSL_CACHE_INV(va, len)		BCM_REFERENCE(va)
	#define OSL_PREFETCH(ptr)			prefetch(ptr)

	#define OSL_ARCH_IS_COHERENT()		NULL
	#define OSL_ACP_WAR_ENAB()			NULL
#endif /* mips */

/* register access macros */
#if defined(BCMJTAG)
	#include <bcmjtag.h>
	#define OSL_WRITE_REG(osh, r, v) \
		({ \
		 BCM_REFERENCE(osh); \
		 bcmjtag_write(NULL, (uintptr)(r), (v), sizeof(*(r))); \
		 })
	#define OSL_READ_REG(osh, r) \
		({ \
		 BCM_REFERENCE(osh); \
		 bcmjtag_read(NULL, (uintptr)(r), sizeof(*(r)));
		 })
#elif defined(BCM47XX_CA9)
extern void osl_pcie_rreg(osl_t *osh, ulong addr, void *v, uint size);

#define OSL_READ_REG(osh, r) \
	({\
		__typeof(*(r)) __osl_v; \
		osl_pcie_rreg(osh, (uintptr)(r), (void *)&__osl_v, sizeof(*(r))); \
		__osl_v; \
	})
#endif 

#if defined(BCM47XX_CA9)
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); bus_op;})
#else /* !BCM47XX_CA9 */
#if defined(BCMJTAG)
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) if (((osl_pubinfo_t*)(osh))->mmbus) \
		mmap_op else bus_op
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) (((osl_pubinfo_t*)(osh))->mmbus) ? \
		mmap_op : bus_op
#else
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
#endif 
#endif /* BCM47XX_CA9 */

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ	2048   /* largest reasonable packet buffer, driver uses for ethernet MTU */

#define OSH_NULL   NULL

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 * Macros expand to calls to functions defined in linux_osl.c .
 */
#ifndef BINOSL
#include <linuxver.h>           /* use current 2.4.x calling conventions */
#include <linux/kernel.h>       /* for vsn/printf's */
#include <linux/string.h>       /* for mem*, str* */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 29)
#define OSL_SYSUPTIME()		((uint32)jiffies_to_msecs(jiffies))
#else
#define OSL_SYSUPTIME()		((uint32)jiffies * (1000 / HZ))
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 29) */
#define	printf(fmt, args...)	printk(fmt , ## args)
#include <linux/kernel.h>	/* for vsn/printf's */
#include <linux/string.h>	/* for mem*, str* */
/* bcopy's: Linux kernel doesn't provide these (anymore) */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

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

extern uint8 osl_readb(osl_t *osh, volatile uint8 *r);
extern uint16 osl_readw(osl_t *osh, volatile uint16 *r);
extern uint32 osl_readl(osl_t *osh, volatile uint32 *r);
extern void osl_writeb(osl_t *osh, volatile uint8 *r, uint8 v);
extern void osl_writew(osl_t *osh, volatile uint16 *r, uint16 v);
extern void osl_writel(osl_t *osh, volatile uint32 *r, uint32 v);

#else /* OSLREGOPS */

#ifndef IL_BIGENDIAN
#ifndef __mips__
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
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
	SELECT_BUS_WRITE(osh, \
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
	SELECT_BUS_WRITE(osh, \
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
#define OSL_PREF_RANGE_LD(va, sz) BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz) BCM_REFERENCE(va)
#endif /* __mips__ */

/* get processor cycle count */
#if defined(mips)
#define	OSL_GETCYCLES(x)	((x) = read_c0_count() * 2)
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

/* Because the non BINOSL implemenation of the PKT OSL routines are macros (for
 * performance reasons),  we need the Linux headers.
 */
#include <linuxver.h>		/* use current 2.4.x calling conventions */

/* packet primitives */
#ifdef BCMDBG_CTRACE
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len), __LINE__, __FILE__)
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb), __LINE__, __FILE__)
#else
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#endif /* BCMDBG_CTRACE */
#define PKTLIST_DUMP(osh, buf)		BCM_REFERENCE(osh)
#define PKTDBG_TRACE(osh, pkt, bit)	BCM_REFERENCE(osh)
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#ifdef CONFIG_DHD_USE_STATIC_BUF
#define	PKTGET_STATIC(osh, len, send)		osl_pktget_static((osh), (len))
#define	PKTFREE_STATIC(osh, skb, send)		osl_pktfree_static((osh), (skb), (send))
#else
#define	PKTGET_STATIC	PKTGET
#define	PKTFREE_STATIC	PKTFREE
#endif /* CONFIG_DHD_USE_STATIC_BUF */
#define	PKTDATA(osh, skb)		({BCM_REFERENCE(osh); (((struct sk_buff*)(skb))->data);})
#define	PKTLEN(osh, skb)		({BCM_REFERENCE(osh); (((struct sk_buff*)(skb))->len);})
#define PKTHEADROOM(osh, skb)		(PKTDATA(osh, skb)-(((struct sk_buff*)(skb))->head))
#define PKTEXPHEADROOM(osh, skb, b)	\
	({ \
	 BCM_REFERENCE(osh); \
	 skb_realloc_headroom((struct sk_buff*)(skb), (b)); \
	 })
#define PKTTAILROOM(osh, skb)		\
	({ \
	 BCM_REFERENCE(osh); \
	 skb_tailroom((struct sk_buff*)(skb)); \
	 })
#define PKTPADTAILROOM(osh, skb, padlen) \
	({ \
	 BCM_REFERENCE(osh); \
	 skb_pad((struct sk_buff*)(skb), (padlen)); \
	 })
#define	PKTNEXT(osh, skb)		({BCM_REFERENCE(osh); (((struct sk_buff*)(skb))->next);})
#define	PKTSETNEXT(osh, skb, x)		\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->next = (struct sk_buff*)(x)); \
	 })
#define	PKTSETLEN(osh, skb, len)	\
	({ \
	 BCM_REFERENCE(osh); \
	 __skb_trim((struct sk_buff*)(skb), (len)); \
	 })
#define	PKTPUSH(osh, skb, bytes)	\
	({ \
	 BCM_REFERENCE(osh); \
	 skb_push((struct sk_buff*)(skb), (bytes)); \
	 })
#define	PKTPULL(osh, skb, bytes)	\
	({ \
	 BCM_REFERENCE(osh); \
	 skb_pull((struct sk_buff*)(skb), (bytes)); \
	 })
#define	PKTTAG(skb)			((void*)(((struct sk_buff*)(skb))->cb))
#define PKTSETPOOL(osh, skb, x, y)	BCM_REFERENCE(osh)
#define	PKTPOOL(osh, skb)		({BCM_REFERENCE(osh); BCM_REFERENCE(skb); FALSE;})
#define PKTFREELIST(skb)        PKTLINK(skb)
#define PKTSETFREELIST(skb, x)  PKTSETLINK((skb), (x))
#define PKTPTR(skb)             (skb)
#define PKTID(skb)              ({BCM_REFERENCE(skb); 0;})
#define PKTSETID(skb, id)       ({BCM_REFERENCE(skb); BCM_REFERENCE(id);})
#define PKTSHRINK(osh, m)		({BCM_REFERENCE(osh); m;})

#ifdef BCMDBG_CTRACE
#define	DEL_CTRACE(zosh, zskb) { \
	unsigned long zflags; \
	spin_lock_irqsave(&(zosh)->ctrace_lock, zflags); \
	list_del(&(zskb)->ctrace_list); \
	(zosh)->ctrace_num--; \
	(zskb)->ctrace_start = 0; \
	(zskb)->ctrace_count = 0; \
	spin_unlock_irqrestore(&(zosh)->ctrace_lock, zflags); \
}

#define	UPDATE_CTRACE(zskb, zfile, zline) { \
	struct sk_buff *_zskb = (struct sk_buff *)(zskb); \
	if (_zskb->ctrace_count < CTRACE_NUM) { \
		_zskb->func[_zskb->ctrace_count] = zfile; \
		_zskb->line[_zskb->ctrace_count] = zline; \
		_zskb->ctrace_count++; \
	} \
	else { \
		_zskb->func[_zskb->ctrace_start] = zfile; \
		_zskb->line[_zskb->ctrace_start] = zline; \
		_zskb->ctrace_start++; \
		if (_zskb->ctrace_start >= CTRACE_NUM) \
			_zskb->ctrace_start = 0; \
	} \
}

#define	ADD_CTRACE(zosh, zskb, zfile, zline) { \
	unsigned long zflags; \
	spin_lock_irqsave(&(zosh)->ctrace_lock, zflags); \
	list_add(&(zskb)->ctrace_list, &(zosh)->ctrace_list); \
	(zosh)->ctrace_num++; \
	UPDATE_CTRACE(zskb, zfile, zline); \
	spin_unlock_irqrestore(&(zosh)->ctrace_lock, zflags); \
}

#define PKTCALLER(zskb)	UPDATE_CTRACE((struct sk_buff *)zskb, (char *)__FUNCTION__, __LINE__)
#endif /* BCMDBG_CTRACE */

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define	FASTBUF	(1 << 0)
#define	PKTSETFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) |= FASTBUF); \
	 })
#define	PKTCLRFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) &= (~FASTBUF)); \
	 })
#define	PKTISFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) & FASTBUF); \
	 })
#define	PKTFAST(osh, skb)	(((struct sk_buff*)(skb))->pktc_flags)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	FASTBUF	(1 << 16)
#define	PKTSETFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) |= FASTBUF); \
	 })
#define	PKTCLRFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) &= (~FASTBUF)); \
	 })
#define	PKTISFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) & FASTBUF); \
	 })
#define	PKTFAST(osh, skb)	(((struct sk_buff*)(skb))->mac_len)
#else
#define	FASTBUF	(1 << 0)
#define	PKTSETFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) |= FASTBUF); \
	 })
#define	PKTCLRFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) &= (~FASTBUF)); \
	 })
#define	PKTISFAST(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) & FASTBUF); \
	 })
#define	PKTFAST(osh, skb)	(((struct sk_buff*)(skb))->__unused)
#endif /* 2.6.22 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	CTFPOOLPTR(osh, skb)	(((struct sk_buff*)(skb))->ctfpool)
#define	CTFPOOLHEAD(osh, skb)	(((ctfpool_t *)((struct sk_buff*)(skb))->ctfpool)->head)
#else
#define	CTFPOOLPTR(osh, skb)	(((struct sk_buff*)(skb))->sk)
#define	CTFPOOLHEAD(osh, skb)	(((ctfpool_t *)((struct sk_buff*)(skb))->sk)->head)
#endif

extern void *osl_ctfpool_add(osl_t *osh);
extern void osl_ctfpool_replenish(osl_t *osh, uint thresh);
extern int32 osl_ctfpool_init(osl_t *osh, uint numobj, uint size);
extern void osl_ctfpool_cleanup(osl_t *osh);
extern void osl_ctfpool_stats(osl_t *osh, void *b);
#else /* CTFPOOL */
#define	PKTSETFAST(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRFAST(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTISFAST(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb); FALSE;})
#endif /* CTFPOOL */

#ifdef CTFMAP
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define	CTFBUF	(1 << 1)
#define	PKTSETCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) |= CTFBUF); \
	 })
#define	PKTCLRCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) &= (~CTFBUF)); \
	 })
#define	PKTISCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->pktc_flags) & CTFBUF); \
	 })
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	CTFBUF	(1 << 17)
#define	PKTSETCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) |= CTFBUF); \
	 })
#define	PKTCLRCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) &= (~CTFBUF)); \
	 })
#define	PKTISCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->mac_len) & CTFBUF); \
	 })
#else
#define	CTFBUF	(1 << 1)
#define	PKTSETCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) |= CTFBUF); \
	 })
#define	PKTCLRCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) &= (~CTFBUF)); \
	 })
#define	PKTISCTF(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 ((((struct sk_buff*)(skb))->__unused) & CTFBUF); \
	 })
#endif /* 2.6.22 */

#if defined(__mips__)
#define CACHE_LINE_SIZE		32
#elif defined(__ARM_ARCH_7A__)
#define CACHE_LINE_SIZE		32
#else
#error "CACHE_LINE_SIZE define needed!"
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define CTFMAPPTR(osh, skb)     (((struct sk_buff*)(skb))->ctfmap)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#define CTFMAPPTR(osh, skb)	(((struct sk_buff*)(skb))->sp)
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
			sz = (sz + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1); \
			_DMA_MAP(osh, (void *)CTFMAPPTR(osh, p), \
			         sz, DMA_RX, p, NULL); \
		} \
		/* clear ctf buf flag */ \
		PKTCLRCTF(osh, p); \
		CTFMAPPTR(osh, p) = NULL; \
	} \
} while (0)
#else /* CTFMAP */
#define	PKTSETCTF(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRCTF(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTISCTF(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb); FALSE;})
#endif /* CTFMAP */

#ifdef HNDCTF

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define	SKIPCT	(1 << 2)
#define	CHAINED	(1 << 3)
#define	PKTSETSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags |= SKIPCT); \
	 })
#define	PKTCLRSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags &= (~SKIPCT)); \
	 })
#define	PKTSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags & SKIPCT); \
	 })
#define	PKTSETCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags |= CHAINED); \
	 })
#define	PKTCLRCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags &= (~CHAINED)); \
	 })
#define	PKTISCHAINED(skb)	(((struct sk_buff*)(skb))->pktc_flags & CHAINED)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	SKIPCT	(1 << 18)
#define	CHAINED	(1 << 19)
#define	PKTSETSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len |= SKIPCT); \
	 })
#define	PKTCLRSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len &= (~SKIPCT)); \
	 })
#define	PKTSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len & SKIPCT); \
	 })
#define	PKTSETCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len |= CHAINED); \
	 })
#define	PKTCLRCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len &= (~CHAINED)); \
	 })
#define	PKTISCHAINED(skb)	(((struct sk_buff*)(skb))->mac_len & CHAINED)
#else /* 2.6.22 */
#define	SKIPCT	(1 << 2)
#define	CHAINED	(1 << 3)
#define	PKTSETSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused |= SKIPCT); \
	 })
#define	PKTCLRSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused &= (~SKIPCT)); \
	 })
#define	PKTSKIPCT(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused & SKIPCT); \
	 })
#define	PKTSETCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused |= CHAINED); \
	 })
#define	PKTCLRCHAINED(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused &= (~CHAINED)); \
	 })
#define	PKTISCHAINED(skb)	(((struct sk_buff*)(skb))->__unused & CHAINED)
#endif /* 2.6.22 */
typedef struct ctf_mark {
	uint32	value;
}	ctf_mark_t;
#define CTF_MARK(m)				(m.value)
#else /* HNDCTF */
#define	PKTSETSKIPCT(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRSKIPCT(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTSKIPCT(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define CTF_MARK(m)		({BCM_REFERENCE(m); 0;})
#endif /* HNDCTF */

#if defined(BCM_GMAC3)

/** pktalloced accounting in devices using GMAC Bulk Forwarding to DHD */

/* Account for packets delivered to downstream forwarder by GMAC interface. */
extern void osl_pkt_tofwder(osl_t *osh, void *skbs, int skb_cnt);
#define PKTTOFWDER(osh, skbs, skb_cnt)  \
	osl_pkt_tofwder(((osl_t *)osh), (void *)(skbs), (skb_cnt))

/* Account for packets received from downstream forwarder. */
#if defined(BCMDBG_CTRACE) /* pkt logging */
extern void osl_pkt_frmfwder(osl_t *osh, void *skbs, int skb_cnt,
                             int line, char *file);
#define PKTFRMFWDER(osh, skbs, skb_cnt) \
	osl_pkt_frmfwder(((osl_t *)osh), (void *)(skbs), (skb_cnt), \
	                 __LINE__, __FILE__)
#else  /* ! (BCMDBG_PKT || BCMDBG_CTRACE) */
extern void osl_pkt_frmfwder(osl_t *osh, void *skbs, int skb_cnt);
#define PKTFRMFWDER(osh, skbs, skb_cnt) \
	osl_pkt_frmfwder(((osl_t *)osh), (void *)(skbs), (skb_cnt))
#endif 


/** GMAC Forwarded packet tagging for reduced cache flush/invalidate.
 * In FWDERBUF tagged packet, only FWDER_PKTMAPSZ amount of data would have
 * been accessed in the GMAC forwarder. This may be used to limit the number of
 * cachelines that need to be flushed or invalidated.
 * Packets sent to the DHD from a GMAC forwarder will be tagged w/ FWDERBUF.
 * DHD may clear the FWDERBUF tag, if more than FWDER_PKTMAPSZ was accessed.
 * Likewise, a debug print of a packet payload in say the ethernet driver needs
 * to be accompanied with a clear of the FWDERBUF tag.
 */

/** Forwarded packets, have a GMAC_FWDER_HWRXOFF sized rx header (etc.h) */
#define FWDER_HWRXOFF       (18)

/** Maximum amount of a pktadat that a downstream forwarder (GMAC) may have
 * read into the L1 cache (not dirty). This may be used in reduced cache ops.
 *
 * Max 44: ET HWRXOFF[18] + BRCMHdr[4] + EtherHdr[14] + VlanHdr[4] + IP[4]
 * Min 32: GMAC_FWDER_HWRXOFF[18] + EtherHdr[14]
 */
#define FWDER_MINMAPSZ      (FWDER_HWRXOFF + 14)
#define FWDER_MAXMAPSZ      (FWDER_HWRXOFF + 4 + 14 + 4 + 4)
#define FWDER_PKTMAPSZ      (FWDER_MAXMAPSZ)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)

#define FWDERBUF            (1 << 4)
#define PKTSETFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags |= FWDERBUF); \
	 })
#define PKTCLRFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags &= (~FWDERBUF)); \
	 })
#define PKTISFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags & FWDERBUF); \
	 })

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)

#define FWDERBUF	        (1 << 20)
#define PKTSETFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len |= FWDERBUF); \
	 })
#define PKTCLRFWDERBUF(osh, skb)  \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len &= (~FWDERBUF)); \
	 })
#define PKTISFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->mac_len & FWDERBUF); \
	 })

#else /* 2.6.22 */

#define FWDERBUF            (1 << 4)
#define PKTSETFWDERBUF(osh, skb)  \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused |= FWDERBUF); \
	 })
#define PKTCLRFWDERBUF(osh, skb)  \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused &= (~FWDERBUF)); \
	 })
#define PKTISFWDERBUF(osh, skb) \
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->__unused & FWDERBUF); \
	 })

#endif /* 2.6.22 */

#else  /* ! BCM_GMAC3 */

#define PKTSETFWDERBUF(osh, skb)  ({ BCM_REFERENCE(osh); BCM_REFERENCE(skb); })
#define PKTCLRFWDERBUF(osh, skb)  ({ BCM_REFERENCE(osh); BCM_REFERENCE(skb); })
#define PKTISFWDERBUF(osh, skb)   ({ BCM_REFERENCE(osh); BCM_REFERENCE(skb); FALSE;})

#endif /* ! BCM_GMAC3 */


#ifdef HNDCTF
/* For broadstream iqos */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define	TOBR		(1 << 5)
#define	PKTSETTOBR(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags |= TOBR); \
	 })
#define	PKTCLRTOBR(osh, skb)	\
	({ \
	 BCM_REFERENCE(osh); \
	 (((struct sk_buff*)(skb))->pktc_flags &= (~TOBR)); \
	 })
#define	PKTISTOBR(skb)	(((struct sk_buff*)(skb))->pktc_flags & TOBR)
#define	PKTSETCTFIPCTXIF(skb, ifp)	(((struct sk_buff*)(skb))->ctf_ipc_txif = ifp)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define	PKTSETTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTISTOBR(skb)	({BCM_REFERENCE(skb); FALSE;})
#define	PKTSETCTFIPCTXIF(skb, ifp)	({BCM_REFERENCE(skb); BCM_REFERENCE(ifp);})
#else /* 2.6.22 */
#define	PKTSETTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTISTOBR(skb)	({BCM_REFERENCE(skb); FALSE;})
#define	PKTSETCTFIPCTXIF(skb, ifp)	({BCM_REFERENCE(skb); BCM_REFERENCE(ifp);})
#endif /* 2.6.22 */
#else /* HNDCTF */
#define	PKTSETTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTCLRTOBR(osh, skb)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTISTOBR(skb)	({BCM_REFERENCE(skb); FALSE;})
#endif /* HNDCTF */

#ifdef HNDCTF
/* Indicate if the packet is forwarding by CTF */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
#define		CTFFWDING               (1 << 6)
#define		PKTSETCTFFWDING(osh, skb)       \
	({ \
	BCM_REFERENCE(osh); \
	(((struct sk_buff*)(skb))->pktc_flags |= CTFFWDING); \
	})
#define        PKTCLRCTFFWDING(osh, skb)       \
	({ \
	BCM_REFERENCE(osh); \
	(((struct sk_buff*)(skb))->pktc_flags &= (~CTFFWDING)); \
	})
#define         PKTISCTFFWDING(skb)     (((struct sk_buff*)(skb))->pktc_flags & CTFFWDING)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define		PKTSETCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTCLRCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTISCTFFWDING(skb)     ({BCM_REFERENCE(skb); FALSE;})
#else /* 2.6.22 */
#define		PKTSETCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTCLRCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTISCTFFWDING(skb)     ({BCM_REFERENCE(skb); FALSE;})
#endif /* 2.6.22 */
#else /* HNDCTF */
#define		PKTSETCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTCLRCTFFWDING(osh, skb)       ({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define		PKTISCTFFWDING(skb)     ({BCM_REFERENCE(skb); FALSE;})
#endif /* HNDCTF */

#ifdef BCMFA
#ifdef BCMFA_HW_HASH
#define PKTSETFAHIDX(skb, idx)	(((struct sk_buff*)(skb))->napt_idx = idx)
#else
#define PKTSETFAHIDX(skb, idx)	({BCM_REFERENCE(skb); BCM_REFERENCE(idx);})
#endif /* BCMFA_SW_HASH */
#define PKTGETFAHIDX(skb)	(((struct sk_buff*)(skb))->napt_idx)
#define PKTSETFADEV(skb, imp)	(((struct sk_buff*)(skb))->dev = imp)
#define PKTSETRXDEV(skb)	(((struct sk_buff*)(skb))->rxdev = ((struct sk_buff*)(skb))->dev)

#define	AUX_TCP_FIN_RST	(1 << 0)
#define	AUX_FREED	(1 << 1)
#define PKTSETFAAUX(skb)	(((struct sk_buff*)(skb))->napt_flags |= AUX_TCP_FIN_RST)
#define	PKTCLRFAAUX(skb)	(((struct sk_buff*)(skb))->napt_flags &= (~AUX_TCP_FIN_RST))
#define	PKTISFAAUX(skb)		(((struct sk_buff*)(skb))->napt_flags & AUX_TCP_FIN_RST)
#define PKTSETFAFREED(skb)	(((struct sk_buff*)(skb))->napt_flags |= AUX_FREED)
#define	PKTCLRFAFREED(skb)	(((struct sk_buff*)(skb))->napt_flags &= (~AUX_FREED))
#define	PKTISFAFREED(skb)	(((struct sk_buff*)(skb))->napt_flags & AUX_FREED)
#define	PKTISFABRIDGED(skb)	PKTISFAAUX(skb)
#else
#define	PKTISFAAUX(skb)		({BCM_REFERENCE(skb); FALSE;})
#define	PKTISFABRIDGED(skb)	({BCM_REFERENCE(skb); FALSE;})
#define	PKTISFAFREED(skb)	({BCM_REFERENCE(skb); FALSE;})

#define	PKTCLRFAAUX(skb)	BCM_REFERENCE(skb)
#define PKTSETFAFREED(skb)	BCM_REFERENCE(skb)
#define	PKTCLRFAFREED(skb)	BCM_REFERENCE(skb)
#endif /* BCMFA */

extern void osl_pktfree(osl_t *osh, void *skb, bool send);
extern void *osl_pktget_static(osl_t *osh, uint len);
extern void osl_pktfree_static(osl_t *osh, void *skb, bool send);

#ifdef BCMDBG_CTRACE
#define PKT_CTRACE_DUMP(osh, b)	osl_ctrace_dump((osh), (b))
extern void *osl_pktget(osl_t *osh, uint len, int line, char *file);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb, int line, char *file);
extern int osl_pkt_is_frmnative(osl_t *osh, struct sk_buff *pkt);
extern void *osl_pktdup(osl_t *osh, void *skb, int line, char *file);
struct bcmstrbuf;
extern void osl_ctrace_dump(osl_t *osh, struct bcmstrbuf *b);
#else
extern void *osl_pkt_frmnative(osl_t *osh, void *skb);
extern void *osl_pktget(osl_t *osh, uint len);
extern void *osl_pktdup(osl_t *osh, void *skb);
#endif /* BCMDBG_CTRACE */
extern struct sk_buff *osl_pkt_tonative(osl_t *osh, void *pkt);
#ifdef BCMDBG_CTRACE
#define PKTFRMNATIVE(osh, skb)  osl_pkt_frmnative(((osl_t *)osh), \
				(struct sk_buff*)(skb), __LINE__, __FILE__)
#define	PKTISFRMNATIVE(osh, skb) osl_pkt_is_frmnative((osl_t *)(osh), (struct sk_buff *)(skb))
#else
#define PKTFRMNATIVE(osh, skb)	osl_pkt_frmnative(((osl_t *)osh), (struct sk_buff*)(skb))
#endif /* BCMDBG_CTRACE */
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

#ifdef CONFIG_NF_CONNTRACK_MARK
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#define PKTMARK(p)                     (((struct sk_buff *)(p))->mark)
#define PKTSETMARK(p, m)               ((struct sk_buff *)(p))->mark = (m)
#else /* !2.6.0 */
#define PKTMARK(p)                     (((struct sk_buff *)(p))->nfmark)
#define PKTSETMARK(p, m)               ((struct sk_buff *)(p))->nfmark = (m)
#endif /* 2.6.0 */
#else /* CONFIG_NF_CONNTRACK_MARK */
#define PKTMARK(p)                     0
#define PKTSETMARK(p, m)
#endif /* CONFIG_NF_CONNTRACK_MARK */

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
#define R_REG(osh, r) \
	({ \
	 BCM_REFERENCE(osh); \
	 sizeof(*(r)) == sizeof(uint8) ? osl_readb((volatile uint8*)(r)) : \
	 sizeof(*(r)) == sizeof(uint16) ? osl_readw((volatile uint16*)(r)) : \
	 osl_readl((volatile uint32*)(r)); \
	 })
#define W_REG(osh, r, v) do { \
	BCM_REFERENCE(osh); \
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
#ifdef BCMDBG_CTRACE
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len), __LINE__, __FILE__)
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb), __LINE__, __FILE__)
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (skb), __LINE__, __FILE__)
#else
#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (skb))
#endif /* BCMDBG_CTRACE */
#define PKTLIST_DUMP(osh, buf)		({BCM_REFERENCE(osh); BCM_REFERENCE(buf);})
#define PKTDBG_TRACE(osh, pkt, bit)	({BCM_REFERENCE(osh); BCM_REFERENCE(pkt);})
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#define	PKTDATA(osh, skb)		osl_pktdata((osh), (skb))
#define	PKTLEN(osh, skb)		osl_pktlen((osh), (skb))
#define PKTHEADROOM(osh, skb)		osl_pktheadroom((osh), (skb))
#define PKTTAILROOM(osh, skb)		osl_pkttailroom((osh), (skb))
#define	PKTNEXT(osh, skb)		osl_pktnext((osh), (skb))
#define	PKTSETNEXT(osh, skb, x)		({BCM_REFERENCE(osh); osl_pktsetnext((skb), (x));})
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
#define PKTSETPOOL(osh, skb, x, y)	({BCM_REFERENCE(osh); BCM_REFERENCE(skb);})
#define	PKTPOOL(osh, skb)		({BCM_REFERENCE(osh); BCM_REFERENCE(skb); FALSE;})
#define PKTFREELIST(skb)        PKTLINK(skb)
#define PKTSETFREELIST(skb, x)  PKTSETLINK((skb), (x))
#define PKTPTR(skb)             (skb)
#define PKTID(skb)              ({BCM_REFERENCE(skb); 0;})
#define PKTSETID(skb, id)       ({BCM_REFERENCE(skb); BCM_REFERENCE(id);})

extern void *osl_pktget(osl_t *osh, uint len);
extern void *osl_pktdup(osl_t *osh, void *skb);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb);
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


#endif	/* BINOSL */

#define PKTALLOCED(osh)		osl_pktalloced(osh)
extern uint osl_pktalloced(osl_t *osh);

#define OSL_RAND()		osl_rand()
extern uint32 osl_rand(void);

#ifdef CTFMAP
#include <ctf/hndctf.h>
#define	CTFMAPSZ	320
#define	DMA_MAP(osh, va, size, direction, p, dmah) \
({ \
	typeof(size) sz = (size); \
	if (p && PKTISCTF((osh), (p))) { \
		sz = CTFMAPSZ; \
		CTFMAPPTR((osh), (p)) = (void *)(((uint8 *)(va)) + CTFMAPSZ); \
	} \
	osl_dma_map((osh), (va), sz, (direction), (p), (dmah)); \
})
#define	SECURE_DMA_MAP(osh, va, size, direction, p, dmah, pcma, offset) \
	osl_sec_dma_map((osh), (va), (size), (direction), (p), (dmah), (pcma), (offset))
#define	SECURE_DMA_DD_MAP(osh, va, size, direction, p, dmah) \
	osl_sec_dma_dd_map((osh), (va), (size), (direction), (p), (dmah))
#if defined(__mips__)
#define	_DMA_MAP(osh, va, size, direction, p, dmah) \
	({ \
	 BCM_REFERENCE(osh); \
	 BCM_REFERENCE(direction); \
	 BCM_REFERENCE(p); \
	 BCM_REFERENCE(dmah); \
	 dma_cache_inv((uint)(va), (size)); \
	 })
#elif defined(__ARM_ARCH_7A__)
#include <asm/cacheflush.h>
#define	_DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction), (p), (dmah))
#else
#define	_DMA_MAP(osh, va, size, direction, p, dmah)	BCM_REFERENCE(osh)
#endif

#else /* CTFMAP */
#define	DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction), (p), (dmah))
#define	SECURE_DMA_MAP(osh, va, size, direction, p, dmah, pcma, offset) \
	osl_sec_dma_map((osh), (va), (size), (direction), (p), (dmah), (pcma), (offset))
#define	SECURE_DMA_DD_MAP(osh, va, size, direction, p, dmah) \
	osl_sec_dma_dd_map((osh), (va), (size), (direction), (p), (dmah))
#endif /* CTFMAP */

#ifdef PKTC
/* Use 8 bytes of skb tstamp field to store below info */
struct chain_node {
	struct sk_buff	*link;
	unsigned int	flags:3, pkts:9, bytes:20;
};

#define CHAIN_NODE(skb)		((struct chain_node*)(((struct sk_buff*)skb)->pktc_cb))

#define	PKTCSETATTR(s, f, p, b)	({CHAIN_NODE(s)->flags = (f); CHAIN_NODE(s)->pkts = (p); \
	                         CHAIN_NODE(s)->bytes = (b);})
#define	PKTCCLRATTR(s)		({CHAIN_NODE(s)->flags = CHAIN_NODE(s)->pkts = \
	                         CHAIN_NODE(s)->bytes = 0;})
#define	PKTCGETATTR(s)		(CHAIN_NODE(s)->flags << 29 | CHAIN_NODE(s)->pkts << 20 | \
	                         CHAIN_NODE(s)->bytes)
#define	PKTCCNT(skb)		(CHAIN_NODE(skb)->pkts)
#define	PKTCLEN(skb)		(CHAIN_NODE(skb)->bytes)
#define	PKTCGETFLAGS(skb)	(CHAIN_NODE(skb)->flags)
#define	PKTCSETFLAGS(skb, f)	(CHAIN_NODE(skb)->flags = (f))
#define	PKTCCLRFLAGS(skb)	(CHAIN_NODE(skb)->flags = 0)
#define	PKTCFLAGS(skb)		(CHAIN_NODE(skb)->flags)
#define	PKTCSETCNT(skb, c)	(CHAIN_NODE(skb)->pkts = (c))
#define	PKTCINCRCNT(skb)	(CHAIN_NODE(skb)->pkts++)
#define	PKTCADDCNT(skb, c)	(CHAIN_NODE(skb)->pkts += (c))
#define	PKTCSETLEN(skb, l)	(CHAIN_NODE(skb)->bytes = (l))
#define	PKTCADDLEN(skb, l)	(CHAIN_NODE(skb)->bytes += (l))
#define	PKTCSETFLAG(skb, fb)	(CHAIN_NODE(skb)->flags |= (fb))
#define	PKTCCLRFLAG(skb, fb)	(CHAIN_NODE(skb)->flags &= ~(fb))
#define	PKTCLINK(skb)		(CHAIN_NODE(skb)->link)
#define	PKTSETCLINK(skb, x)	(CHAIN_NODE(skb)->link = (struct sk_buff*)(x))
#define FOREACH_CHAINED_PKT(skb, nskb) \
	for (; (skb) != NULL; (skb) = (nskb)) \
		if ((nskb) = (PKTISCHAINED(skb) ? PKTCLINK(skb) : NULL), \
		    PKTSETCLINK((skb), NULL), 1)
#define	PKTCFREE(osh, skb, send) \
do { \
	void *nskb; \
	ASSERT((skb) != NULL); \
	FOREACH_CHAINED_PKT((skb), nskb) { \
		PKTCLRCHAINED((osh), (skb)); \
		PKTCCLRFLAGS((skb)); \
		PKTFREE((osh), (skb), (send)); \
	} \
} while (0)
#define PKTCENQTAIL(h, t, p) \
do { \
	if ((t) == NULL) { \
		(h) = (t) = (p); \
	} else { \
		PKTSETCLINK((t), (p)); \
		(t) = (p); \
	} \
} while (0)
#endif /* PKTC */

#else /* ! BCMDRIVER */


/* ASSERT */
	#define ASSERT(exp)	do {} while (0)

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

/* Current STB 7445D1 doesn't use ACP and it is non-coherrent.
 * Adding these dummy values for build apss only
 * When we revisit need to change these.
 */
#if defined(STBLINUX)
#if defined(__ARM_ARCH_7A__)
#define ACP_WAR_ENAB() 0
#define ACP_WIN_LIMIT 1
#define arch_is_coherent() 0
#endif /* __ARM_ARCH_7A__ */
#endif /* STBLINUX */

#ifdef BCM_SECURE_DMA

#define CMA_BUFSIZE_4K	4096
#define CMA_BUFSIZE_2K	2048
#define CMA_BUFSIZE_512	512

#define	CMA_BUFNUM		2048
#define CMA_DMA_DESC_MEMBLOCK	(0x80000)	/* 512K bytes */
#define CMA_DMA_DATA_MEMBLOCK	(CMA_BUFSIZE_4K*CMA_BUFNUM)		/* 2048 page size buffers */
#define	CMA_MEMBLOCK		(CMA_DMA_DESC_MEMBLOCK + CMA_DMA_DATA_MEMBLOCK)
#if defined(__ARM_ARCH_7A__)
#define CONT_ARMREGION	0x02		/* Region CMA */
#else
#define CONT_MIPREGION	0x00		/* To access the MIPs mem, Not yet... */
#endif

#define SEC_DMA_ALIGN	(1<<16)
typedef struct sec_mem_elem {
	size_t			size;
	int			direction;
	phys_addr_t		pa_cma; /* physical  address */
	void			*va; /* virtual address of driver pkt */
	dma_addr_t		dma_handle; /* bus address assign by linux */
	void			*vac;       /* virtual address of cma buffer */
	struct page *pa_cma_page;	/* phys to page address */
	struct	sec_mem_elem	*next;
} sec_mem_elem_t;

typedef struct sec_cma_info {
	struct sec_mem_elem *sec_alloc_list;
	struct sec_mem_elem *sec_alloc_list_tail;
} sec_cma_info_t;

extern void osl_sec_dma_setup_contig_mem(osl_t *osh, unsigned long memsize, int regn);
extern int osl_sec_dma_alloc_contig_mem(osl_t *osh, unsigned long memsize, int regn);
extern void osl_sec_dma_init_elem_mem_block(osl_t *osh, size_t mbsize, int max,
	sec_mem_elem_t **list);
extern sec_mem_elem_t *osl_sec_dma_alloc_mem_elem(osl_t *osh, void *va, uint size,
	int direction, struct sec_cma_info *ptr_cma_info, uint offset);
extern void osl_sec_dma_free_mem_elem(osl_t *osh, sec_mem_elem_t *sec_mem_elem);
extern sec_mem_elem_t *osl_sec_dma_find_elem(osl_t *osh, struct sec_cma_info *ptr_cma_info,
	void *va);
extern sec_mem_elem_t *osl_sec_dma_find_rem_elem(osl_t *osh, struct sec_cma_info *ptr_cma_info,
	dma_addr_t dma_handle);
extern dma_addr_t osl_sec_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *dmah, void *ptr_cma_info, uint offset);
extern dma_addr_t osl_sec_dma_dd_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *dmah);
extern void osl_sec_dma_unmap(osl_t *osh, dma_addr_t dma_handle, uint size, int direction,
	void *p, hnddma_seg_map_t *map, void *ptr_cma_info, uint offset);
extern void *osl_sec_dma_ioremap(osl_t *osh, struct page *page, size_t size, bool iscache,
	bool isdecr);
extern void osl_sec_dma_deinit_elem_mem_block(osl_t *osh, size_t mbsize, int max,
	void *sec_list_base);
extern void osl_sec_dma_free_contig_mem(osl_t *osh, u32 memsize, int regn);
extern void osl_sec_dma_iounmap(osl_t *osh, void *contig_base_va, size_t size);
extern void *osl_sec_dma_alloc_consistent(osl_t *osh, uint size, uint16 align_bits, ulong *pap);
extern void osl_sec_cma_baseaddr_memsize(osl_t *osh, dma_addr_t *cma_baseaddr, size_t *cma_memsize);

#endif /* BCM_SECURE_DMA */
#endif	/* _linux_osl_h_ */
