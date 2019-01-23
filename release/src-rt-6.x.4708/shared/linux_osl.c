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
 * $Id: linux_osl.c 532344 2015-02-05 19:21:37Z $
 */

#define LINUX_PORT

#include <typedefs.h>
#include <bcmendian.h>
#include <linuxver.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <linux/delay.h>
#ifdef mips
#include <asm/paccess.h>
#endif /* mips */
#include <pcicfg.h>


#ifdef BCM_SECURE_DMA
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>
#if !defined(__ARM_ARCH_7A__)
#include <linux/highmem.h>
#endif
#include <linux/dma-mapping.h>
#if defined(__ARM_ARCH_7A__)
#include <asm/memory.h>
#include <linux/brcmstb/cma_driver.h>
#else
#include <linux/brcmstb/brcmapi.h>
#include <linux/dmaengine.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#endif /* __ARM_ARCH_7A__ */
#endif /* BCM_SECURE_DMA */

#include <linux/fs.h>

#define PCI_CFG_RETRY 		10

#define OS_HANDLE_MAGIC		0x1234abcd	/* Magic # to recognize osh */
#define BCM_MEM_FILENAME_LEN 	24		/* Mem. filename length */

#ifdef DHD_USE_STATIC_BUF
#define STATIC_BUF_MAX_NUM	16
#define STATIC_BUF_SIZE	(PAGE_SIZE*2)
#define STATIC_BUF_TOTAL_LEN	(STATIC_BUF_MAX_NUM * STATIC_BUF_SIZE)

typedef struct bcm_static_buf {
	struct semaphore static_sem;
	unsigned char *buf_ptr;
	unsigned char buf_use[STATIC_BUF_MAX_NUM];
} bcm_static_buf_t;

static bcm_static_buf_t *bcm_static_buf = 0;

#define STATIC_PKT_MAX_NUM	8

typedef struct bcm_static_pkt {
	struct sk_buff *skb_4k[STATIC_PKT_MAX_NUM];
	struct sk_buff *skb_8k[STATIC_PKT_MAX_NUM];
	struct semaphore osl_pkt_sem;
	unsigned char pkt_use[STATIC_PKT_MAX_NUM * 2];
} bcm_static_pkt_t;

static bcm_static_pkt_t *bcm_static_skb = 0;
#endif /* DHD_USE_STATIC_BUF */

typedef struct bcm_mem_link {
	struct bcm_mem_link *prev;
	struct bcm_mem_link *next;
	uint	size;
	int	line;
	void 	*osh;
	char	file[BCM_MEM_FILENAME_LEN];
} bcm_mem_link_t;

struct osl_info {
	osl_pubinfo_t pub;
#ifdef CTFPOOL
	ctfpool_t *ctfpool;
#endif /* CTFPOOL */
	uint magic;
	void *pdev;
	atomic_t malloced;
	atomic_t pktalloced; 	/* Number of allocated packet buffers */
	uint failed;
	uint bustype;
	bcm_mem_link_t *dbgmem_list;
#if defined(DSLCPE_DELAY)
	shared_osl_t *oshsh; /* osh shared */
#endif
	spinlock_t dbgmem_lock;
#ifdef BCMDBG_CTRACE
	spinlock_t ctrace_lock;
	struct list_head ctrace_list;
	int ctrace_num;
#endif /* BCMDBG_CTRACE */
	spinlock_t pktalloc_lock;
#ifdef	BCM_SECURE_DMA
	struct sec_mem_elem *sec_list_512;
	struct sec_mem_elem *sec_list_base_512;
	struct sec_mem_elem *sec_list_2048;
	struct sec_mem_elem *sec_list_base_2048;
	struct sec_mem_elem *sec_list_4096;
	struct sec_mem_elem *sec_list_base_4096;
	u32  contig_base_alloc;
	u32   contig_base;
	void *contig_base_alloc_va;
	void *contig_base_va;
#if defined(__ARM_ARCH_7A__)
	struct cma_dev *cma;
#endif
	struct device *dev;
	u32 contig_base_alloc_coherent;
	void *coherent_base_alloc_va;
	void *coherent_base_va;
	void *contig_delta_va_pa;
#endif /* BCM_SECURE_DMA */
};

#define OSL_PKTTAG_CLEAR(p) \
do { \
	struct sk_buff *s = (struct sk_buff *)(p); \
	ASSERT(OSL_PKTTAG_SZ == 32); \
	*(uint32 *)(&s->cb[0]) = 0; *(uint32 *)(&s->cb[4]) = 0; \
	*(uint32 *)(&s->cb[8]) = 0; *(uint32 *)(&s->cb[12]) = 0; \
	*(uint32 *)(&s->cb[16]) = 0; *(uint32 *)(&s->cb[20]) = 0; \
	*(uint32 *)(&s->cb[24]) = 0; *(uint32 *)(&s->cb[28]) = 0; \
} while (0)

/* PCMCIA attribute space access macros */
#if defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)
struct pcmcia_dev {
	dev_link_t link;	/* PCMCIA device pointer */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	dev_node_t node;	/* PCMCIA node structure */
#endif
	void *base;		/* Mapped attribute memory window */
	size_t size;		/* Size of window */
	void *drv;		/* Driver data */
};
#endif /* defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE) */

/* Global ASSERT type flag */
uint32 g_assert_type = FALSE;

static int16 linuxbcmerrormap[] =
{	0, 			/* 0 */
	-EINVAL,		/* BCME_ERROR */
	-EINVAL,		/* BCME_BADARG */
	-EINVAL,		/* BCME_BADOPTION */
	-EINVAL,		/* BCME_NOTUP */
	-EINVAL,		/* BCME_NOTDOWN */
	-EINVAL,		/* BCME_NOTAP */
	-EINVAL,		/* BCME_NOTSTA */
	-EINVAL,		/* BCME_BADKEYIDX */
	-EINVAL,		/* BCME_RADIOOFF */
	-EINVAL,		/* BCME_NOTBANDLOCKED */
	-EINVAL, 		/* BCME_NOCLK */
	-EINVAL, 		/* BCME_BADRATESET */
	-EINVAL, 		/* BCME_BADBAND */
	-E2BIG,			/* BCME_BUFTOOSHORT */
	-E2BIG,			/* BCME_BUFTOOLONG */
	-EBUSY, 		/* BCME_BUSY */
	-EINVAL, 		/* BCME_NOTASSOCIATED */
	-EINVAL, 		/* BCME_BADSSIDLEN */
	-EINVAL, 		/* BCME_OUTOFRANGECHAN */
	-EINVAL, 		/* BCME_BADCHAN */
	-EFAULT, 		/* BCME_BADADDR */
	-ENOMEM, 		/* BCME_NORESOURCE */
	-EOPNOTSUPP,		/* BCME_UNSUPPORTED */
	-EMSGSIZE,		/* BCME_BADLENGTH */
	-EINVAL,		/* BCME_NOTREADY */
	-EPERM,			/* BCME_EPERM */
	-ENOMEM, 		/* BCME_NOMEM */
	-EINVAL, 		/* BCME_ASSOCIATED */
	-ERANGE, 		/* BCME_RANGE */
	-EINVAL, 		/* BCME_NOTFOUND */
	-EINVAL, 		/* BCME_WME_NOT_ENABLED */
	-EINVAL, 		/* BCME_TSPEC_NOTFOUND */
	-EINVAL, 		/* BCME_ACM_NOTSUPPORTED */
	-EINVAL,		/* BCME_NOT_WME_ASSOCIATION */
	-EIO,			/* BCME_SDIO_ERROR */
	-ENODEV,		/* BCME_DONGLE_DOWN */
	-EINVAL,		/* BCME_VERSION */
	-EIO,			/* BCME_TXFAIL */
	-EIO,			/* BCME_RXFAIL */
	-ENODEV,		/* BCME_NODEVICE */
	-EINVAL,		/* BCME_NMODE_DISABLED */
	-ENODATA,		/* BCME_NONRESIDENT */
	-EINVAL,		/* BCME_SCANREJECT */
	-EINVAL,		/* unused */
	-EINVAL,		/* unused */
	-EINVAL,		/* unused */
	-EOPNOTSUPP,		/* BCME_DISABLED */

/* When an new error code is added to bcmutils.h, add os
 * specific error translation here as well
 */
/* check if BCME_LAST changed since the last time this function was updated */
#if BCME_LAST != -47
#error "You need to add a OS error translation in the linuxbcmerrormap \
	for new error code defined in bcmutils.h"
#endif
};

/* translate bcmerrors into linux errors */
int
osl_error(int bcmerror)
{
	if (bcmerror > 0)
		bcmerror = 0;
	else if (bcmerror < BCME_LAST)
		bcmerror = BCME_ERROR;

	/* Array bounds covered by ASSERT in osl_attach */
	return linuxbcmerrormap[-bcmerror];
}

extern uint8* dhd_os_prealloc(void *osh, int section, int size);

osl_t *
osl_attach(void *pdev, uint bustype, bool pkttag)
{
	osl_t *osh;

	osh = kmalloc(sizeof(osl_t), GFP_ATOMIC);
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));

	/* Check that error map has the right number of entries in it */
	ASSERT(ABS(BCME_LAST) == (ARRAYSIZE(linuxbcmerrormap) - 1));

	osh->magic = OS_HANDLE_MAGIC;
	atomic_set(&osh->malloced, 0);
	osh->failed = 0;
	osh->dbgmem_list = NULL;
	spin_lock_init(&(osh->dbgmem_lock));
	osh->pdev = pdev;
	osh->pub.pkttag = pkttag;
	osh->bustype = bustype;

#ifdef BCM_SECURE_DMA
#if defined(__ARM_ARCH_7A__)
	osl_sec_dma_setup_contig_mem(osh, CMA_MEMBLOCK, CONT_ARMREGION);
#else
	osl_sec_dma_alloc_mips_contig_mem(osh, CMA_MEMBLOCK, CONT_MIPREGION);
#endif

#ifdef BCM47XX_CA9
	osh->coherent_base_alloc_va = osl_sec_dma_ioremap(osh,
		PHYS_TO_PAGE(osh->contig_base_alloc),
		CMA_DMA_DESC_MEMBLOCK, TRUE, TRUE);
#else
	osh->coherent_base_alloc_va = osl_sec_dma_ioremap(osh,
		PHYS_TO_PAGE(osh->contig_base_alloc),
		CMA_DMA_DESC_MEMBLOCK, FALSE, TRUE);

#endif /* BCM47XX_CA9 */

	osh->coherent_base_va = osh->coherent_base_alloc_va;
	osh->contig_base_alloc_coherent = osh->contig_base_alloc;

	osh->contig_base_alloc += CMA_DMA_DESC_MEMBLOCK;

#if defined(__ARM_ARCH_7A__)
	osh->contig_base_alloc_va = osl_sec_dma_ioremap(osh,
		PHYS_TO_PAGE(osh->contig_base_alloc), CMA_DMA_DATA_MEMBLOCK, TRUE, FALSE);
#else
	osh->contig_base_alloc_va = osl_sec_dma_ioremap(osh,
		PHYS_TO_PAGE(osh->contig_base_alloc), CMA_DMA_DATA_MEMBLOCK, TRUE, FALSE);

#endif

	osh->contig_base_va = osh->contig_base_alloc_va;

	/*
	* osl_sec_dma_init_elem_mem_block(osh, CMA_BUFSIZE_512, CMA_BUFNUM, &osh->sec_list_512);
	* osh->sec_list_base_512 = osh->sec_list_512;
	* osl_sec_dma_init_elem_mem_block(osh, CMA_BUFSIZE_2K, CMA_BUFNUM, &osh->sec_list_2048);
	* osh->sec_list_base_2048 = osh->sec_list_2048;
	*/
	osl_sec_dma_init_elem_mem_block(osh, CMA_BUFSIZE_4K, CMA_BUFNUM, &osh->sec_list_4096);
	osh->sec_list_base_4096 = osh->sec_list_4096;

#endif /* BCM_SECURE_DMA */

	switch (bustype) {
		case PCI_BUS:
		case SI_BUS:
		case PCMCIA_BUS:
			osh->pub.mmbus = TRUE;
			break;
		case JTAG_BUS:
		case SDIO_BUS:
		case USB_BUS:
		case SPI_BUS:
		case RPC_BUS:
			osh->pub.mmbus = FALSE;
			break;
		default:
			ASSERT(FALSE);
			break;
	}

#if defined(DHD_USE_STATIC_BUF)
	if (!bcm_static_buf) {
		if (!(bcm_static_buf = (bcm_static_buf_t *)dhd_os_prealloc(osh, 3, STATIC_BUF_SIZE+
			STATIC_BUF_TOTAL_LEN))) {
			printk("can not alloc static buf!\n");
		}
		else
			printk("alloc static buf at %x!\n", (unsigned int)bcm_static_buf);


		sema_init(&bcm_static_buf->static_sem, 1);

		bcm_static_buf->buf_ptr = (unsigned char *)bcm_static_buf + STATIC_BUF_SIZE;
	}

	if (!bcm_static_skb) {
		int i;
		void *skb_buff_ptr = 0;
		bcm_static_skb = (bcm_static_pkt_t *)((char *)bcm_static_buf + 2048);
		skb_buff_ptr = dhd_os_prealloc(osh, 4, 0);

		bcopy(skb_buff_ptr, bcm_static_skb, sizeof(struct sk_buff *)*16);
		for (i = 0; i < STATIC_PKT_MAX_NUM * 2; i++)
			bcm_static_skb->pkt_use[i] = 0;

		sema_init(&bcm_static_skb->osl_pkt_sem, 1);
	}
#endif /* DHD_USE_STATIC_BUF */

#ifdef BCMDBG_CTRACE
	spin_lock_init(&osh->ctrace_lock);
	INIT_LIST_HEAD(&osh->ctrace_list);
	osh->ctrace_num = 0;
#endif /* BCMDBG_CTRACE */

	spin_lock_init(&(osh->pktalloc_lock));

#ifdef BCMDBG
	if (pkttag) {
		struct sk_buff *skb;
		BCM_REFERENCE(skb);
		ASSERT(OSL_PKTTAG_SZ <= sizeof(skb->cb));
	}
#endif
	return osh;
}

void
osl_detach(osl_t *osh)
{

	if (osh == NULL)
		return;

#ifdef BCM_SECURE_DMA
#if defined(__ARM_ARCH_7A__)
	/* applicable for ARM as bootmem for MIPS */
	osl_sec_dma_free_contig_mem(osh, CMA_MEMBLOCK, CONT_ARMREGION);
#endif
	osl_sec_dma_deinit_elem_mem_block(osh, CMA_BUFSIZE_512, CMA_BUFNUM, osh->sec_list_base_512);
	osl_sec_dma_deinit_elem_mem_block(osh, CMA_BUFSIZE_2K, CMA_BUFNUM, osh->sec_list_base_2048);
	osl_sec_dma_deinit_elem_mem_block(osh, CMA_BUFSIZE_4K, CMA_BUFNUM, osh->sec_list_base_4096);
	osl_sec_dma_iounmap(osh, osh->contig_base_va, CMA_MEMBLOCK);
#endif /* BCM_SECURE_DMA */

#ifdef DHD_USE_STATIC_BUF
		if (bcm_static_buf) {
			bcm_static_buf = 0;
		}
		if (bcm_static_skb) {
			bcm_static_skb = 0;
		}
#endif

	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	kfree(osh);
}

static struct sk_buff *osl_alloc_skb(unsigned int len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = GFP_ATOMIC;
	struct sk_buff *skb;

#if defined(CONFIG_SPARSEMEM) && defined(CONFIG_ZONE_DMA)
	flags |= GFP_DMA;
#endif
	skb = __dev_alloc_skb(len, flags);
#ifdef CTFMAP
	if (skb) {
		skb->data = skb->head + NET_SKB_PAD;
		skb->tail = skb->data;
	}
#endif /* CTFMAP */
	return skb;
#else
	return dev_alloc_skb(len);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25) */
}

#ifdef CTFPOOL

#ifdef CTFPOOL_SPINLOCK
#define CTFPOOL_LOCK(ctfpool, flags)	spin_lock_irqsave(&(ctfpool)->lock, flags)
#define CTFPOOL_UNLOCK(ctfpool, flags)	spin_unlock_irqrestore(&(ctfpool)->lock, flags)
#else
#define CTFPOOL_LOCK(ctfpool, flags)	spin_lock_bh(&(ctfpool)->lock)
#define CTFPOOL_UNLOCK(ctfpool, flags)	spin_unlock_bh(&(ctfpool)->lock)
#endif /* CTFPOOL_SPINLOCK */

#if defined(CTF_PPTP) || defined(CTF_L2TP)
static int ctfpool_tail __read_mostly = 0;

void osl_ctfpool_direction(int tail)
{
	static struct {
		spinlock_t lock;
	} dummy = { .lock = __SPIN_LOCK_UNLOCKED(dummy.lock) };
	static atomic_t tail_users = ATOMIC_INIT(0);
#ifdef CTFPOOL_SPINLOCK
	unsigned long flags;
#endif /* CTFPOOL_SPINLOCK */
	int changed;

	changed = tail ?
		atomic_add_unless(&tail_users, 1, 0) :
		atomic_add_unless(&tail_users, -1, 1);
	if (changed)
		return;

	CTFPOOL_LOCK(&dummy, flags);

	ctfpool_tail = tail ?
		!atomic_inc_and_test(&tail_users) :
		!atomic_dec_and_test(&tail_users);
	printk("ctfpool: set %s direction\n", ctfpool_tail ? "tail" : "head");

	CTFPOOL_UNLOCK(&dummy, flags);
}
#endif

/*
 * Allocate and add an object to packet pool.
 */
void *
osl_ctfpool_add(osl_t *osh)
{
	struct sk_buff *skb;
#ifdef CTFPOOL_SPINLOCK
	unsigned long flags;
#endif /* CTFPOOL_SPINLOCK */

	if ((osh == NULL) || (osh->ctfpool == NULL))
		return NULL;

	CTFPOOL_LOCK(osh->ctfpool, flags);
	ASSERT(osh->ctfpool->curr_obj <= osh->ctfpool->max_obj);

	/* No need to allocate more objects */
	if (osh->ctfpool->curr_obj == osh->ctfpool->max_obj) {
		CTFPOOL_UNLOCK(osh->ctfpool, flags);
		return NULL;
	}

	/* Allocate a new skb and add it to the ctfpool */
	skb = osl_alloc_skb(osh->ctfpool->obj_size);
	if (skb == NULL) {
		printf("%s: skb alloc of len %d failed\n", __FUNCTION__,
		       osh->ctfpool->obj_size);
		CTFPOOL_UNLOCK(osh->ctfpool, flags);
		return NULL;
	}

	/* Add to ctfpool */
#if defined(CTF_PPTP) || defined(CTF_L2TP)
	if (osh->ctfpool->head == NULL) {
		osh->ctfpool->head = skb;
		osh->ctfpool->tail = skb;
		skb->next = NULL;  /* tail end */
	}
	else if (!ctfpool_tail) {
		skb->next = (struct sk_buff *)osh->ctfpool->head;
		osh->ctfpool->head = skb;
	}
	else {
		((struct sk_buff *)osh->ctfpool->tail)->next = skb;
		osh->ctfpool->tail = skb;
		skb->next = NULL;  /* tail end */
	}
#else
	skb->next = (struct sk_buff *)osh->ctfpool->head;
	osh->ctfpool->head = skb;
#endif

	osh->ctfpool->fast_frees++;
	osh->ctfpool->curr_obj++;

	/* Hijack a skb member to store ptr to ctfpool */
	CTFPOOLPTR(osh, skb) = (void *)osh->ctfpool;

	/* Use bit flag to indicate skb from fast ctfpool */
	PKTFAST(osh, skb) = FASTBUF;

	CTFPOOL_UNLOCK(osh->ctfpool, flags);

	return skb;
}

/*
 * Add new objects to the pool.
 */
void
osl_ctfpool_replenish(osl_t *osh, uint thresh)
{
	if ((osh == NULL) || (osh->ctfpool == NULL))
		return;

	/* Do nothing if no refills are required */
	while ((osh->ctfpool->refills > 0) && (thresh--)) {
		osl_ctfpool_add(osh);
		osh->ctfpool->refills--;
	}
}

/*
 * Initialize the packet pool with specified number of objects.
 */
int32
osl_ctfpool_init(osl_t *osh, uint numobj, uint size)
{
	osh->ctfpool = kmalloc(sizeof(ctfpool_t), GFP_ATOMIC);
	ASSERT(osh->ctfpool);
	bzero(osh->ctfpool, sizeof(ctfpool_t));

	osh->ctfpool->max_obj = numobj;
	osh->ctfpool->obj_size = size;

	spin_lock_init(&osh->ctfpool->lock);

	while (numobj--) {
		if (!osl_ctfpool_add(osh))
			return -1;
		osh->ctfpool->fast_frees--;
	}

	return 0;
}

/*
 * Cleanup the packet pool objects.
 */
void
osl_ctfpool_cleanup(osl_t *osh)
{
	struct sk_buff *skb, *nskb;
#ifdef CTFPOOL_SPINLOCK
	unsigned long flags;
#endif /* CTFPOOL_SPINLOCK */

	if ((osh == NULL) || (osh->ctfpool == NULL))
		return;

	CTFPOOL_LOCK(osh->ctfpool, flags);

	skb = osh->ctfpool->head;

	while (skb != NULL) {
		nskb = skb->next;
		dev_kfree_skb(skb);
		skb = nskb;
		osh->ctfpool->curr_obj--;
	}

	ASSERT(osh->ctfpool->curr_obj == 0);
	osh->ctfpool->head = NULL;
#if defined(CTF_PPTP) || defined(CTF_L2TP)
	osh->ctfpool->tail = NULL;
#endif
	CTFPOOL_UNLOCK(osh->ctfpool, flags);

	kfree(osh->ctfpool);
	osh->ctfpool = NULL;
}

void
osl_ctfpool_stats(osl_t *osh, void *b)
{
	struct bcmstrbuf *bb;

	if ((osh == NULL) || (osh->ctfpool == NULL))
		return;

#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf) {
		bcm_static_buf = 0;
	}
	if (bcm_static_skb) {
		bcm_static_skb = 0;
	}
#endif /* DHD_USE_STATIC_BUF */

	bb = b;

	ASSERT((osh != NULL) && (bb != NULL));

	bcm_bprintf(bb, "max_obj %d obj_size %d curr_obj %d refills %d\n",
	            osh->ctfpool->max_obj, osh->ctfpool->obj_size,
	            osh->ctfpool->curr_obj, osh->ctfpool->refills);
	bcm_bprintf(bb, "fast_allocs %d fast_frees %d slow_allocs %d\n",
	            osh->ctfpool->fast_allocs, osh->ctfpool->fast_frees,
	            osh->ctfpool->slow_allocs);
}

static inline struct sk_buff *
osl_pktfastget(osl_t *osh, uint len)
{
	struct sk_buff *skb;
#ifdef CTFPOOL_SPINLOCK
	unsigned long flags;
#endif /* CTFPOOL_SPINLOCK */

	/* Try to do fast allocate. Return null if ctfpool is not in use
	 * or if there are no items in the ctfpool.
	 */
	if (osh->ctfpool == NULL)
		return NULL;

	CTFPOOL_LOCK(osh->ctfpool, flags);
	if (osh->ctfpool->head == NULL) {
		ASSERT(osh->ctfpool->curr_obj == 0);
		osh->ctfpool->slow_allocs++;
		CTFPOOL_UNLOCK(osh->ctfpool, flags);
		return NULL;
	}

	ASSERT(len <= osh->ctfpool->obj_size);
	if (len > osh->ctfpool->obj_size) {
		CTFPOOL_UNLOCK(osh->ctfpool, flags);
		return NULL;
	}

	/* Get an object from ctfpool */
	skb = (struct sk_buff *)osh->ctfpool->head;
	osh->ctfpool->head = (void *)skb->next;

	osh->ctfpool->fast_allocs++;
	osh->ctfpool->curr_obj--;
	ASSERT(CTFPOOLHEAD(osh, skb) == (struct sock *)osh->ctfpool->head);
	CTFPOOL_UNLOCK(osh->ctfpool, flags);

	/* Init skb struct */
	skb->next = skb->prev = NULL;
	skb->data = skb->head + NET_SKB_PAD;
	skb->tail = skb->data;

	skb->len = 0;
	skb->cloned = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
	skb->list = NULL;
#endif
	atomic_set(&skb->users, 1);

	PKTSETCLINK(skb, NULL);
	PKTCCLRATTR(skb);
	PKTFAST(osh, skb) &= ~(CTFBUF | SKIPCT | CHAINED);

	return skb;
}
#endif /* CTFPOOL */
/* Convert a driver packet to native(OS) packet
 * In the process, packettag is zeroed out before sending up
 * IP code depends on skb->cb to be setup correctly with various options
 * In our case, that means it should be 0
 */
struct sk_buff * BCMFASTPATH
osl_pkt_tonative(osl_t *osh, void *pkt)
{
	struct sk_buff *nskb;
#ifdef BCMDBG_CTRACE
	struct sk_buff *nskb1, *nskb2;
#endif

	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(pkt);

	/* Decrement the packet counter */
	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
		atomic_sub(PKTISCHAINED(nskb) ? PKTCCNT(nskb) : 1, &osh->pktalloced);

#ifdef BCMDBG_CTRACE
		for (nskb1 = nskb; nskb1 != NULL; nskb1 = nskb2) {
			if (PKTISCHAINED(nskb1)) {
				nskb2 = PKTCLINK(nskb1);
			}
			else
				nskb2 = NULL;

			DEL_CTRACE(osh, nskb1);
		}
#endif /* BCMDBG_CTRACE */
	}
	return (struct sk_buff *)pkt;
}

/* Convert a native(OS) packet to driver packet.
 * In the process, native packet is destroyed, there is no copying
 * Also, a packettag is zeroed out
 */
#ifdef BCMDBG_CTRACE
void * BCMFASTPATH
osl_pkt_frmnative(osl_t *osh, void *pkt, int line, char *file)
#else
void * BCMFASTPATH
osl_pkt_frmnative(osl_t *osh, void *pkt)
#endif /* BCMDBG_CTRACE */
{
	struct sk_buff *nskb;
#ifdef BCMDBG_CTRACE
	struct sk_buff *nskb1, *nskb2;
#endif

	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(pkt);

	/* Increment the packet counter */
	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
		atomic_add(PKTISCHAINED(nskb) ? PKTCCNT(nskb) : 1, &osh->pktalloced);

#ifdef BCMDBG_CTRACE
		for (nskb1 = nskb; nskb1 != NULL; nskb1 = nskb2) {
			if (PKTISCHAINED(nskb1)) {
				nskb2 = PKTCLINK(nskb1);
			}
			else
				nskb2 = NULL;

			ADD_CTRACE(osh, nskb1, file, line);
		}
#endif /* BCMDBG_CTRACE */
	}
	return (void *)pkt;
}

/* Return a new packet. zero out pkttag */
#ifdef BCMDBG_CTRACE
void * BCMFASTPATH
osl_pktget(osl_t *osh, uint len, int line, char *file)
#else
void * BCMFASTPATH
osl_pktget(osl_t *osh, uint len)
#endif /* BCMDBG_CTRACE */
{
	struct sk_buff *skb;

#ifdef CTFPOOL
	/* Allocate from local pool */
	skb = osl_pktfastget(osh, len);
	if ((skb != NULL) || ((skb = osl_alloc_skb(len)) != NULL)) {
#else /* CTFPOOL */
	if ((skb = osl_alloc_skb(len))) {
#endif /* CTFPOOL */
#ifdef BCMDBG
		skb_put(skb, len);
#else
		skb->tail += len;
		skb->len  += len;
#endif
		skb->priority = 0;

#ifdef BCMDBG_CTRACE
		ADD_CTRACE(osh, skb, file, line);
#endif
		atomic_inc(&osh->pktalloced);
	}

	return ((void*) skb);
}

#ifdef CTFPOOL
static inline void
osl_pktfastfree(osl_t *osh, struct sk_buff *skb)
{
	ctfpool_t *ctfpool;
#ifdef CTFPOOL_SPINLOCK
	unsigned long flags;
#endif /* CTFPOOL_SPINLOCK */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	skb->tstamp.tv.sec = 0;
#else
	skb->stamp.tv_sec = 0;
#endif

	/* We only need to init the fields that we change */
	skb->dev = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	skb->dst = NULL;
#endif
	OSL_PKTTAG_CLEAR(skb);
	skb->ip_summed = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	skb_orphan(skb);
#else
	skb->destructor = NULL;
#endif

	ctfpool = (ctfpool_t *)CTFPOOLPTR(osh, skb);
#if 0
	ASSERT(ctfpool != NULL);
#else
	if (ctfpool == NULL) {
		printk("%s: free skb for NULL ctfpool\n", __FUNCTION__);
		dev_kfree_skb(skb);
		return;
	}
#endif

	/* Add object to the ctfpool */
	CTFPOOL_LOCK(ctfpool, flags);
#if defined(CTF_PPTP) || defined(CTF_L2TP)
	if (ctfpool->head == NULL) {
		ctfpool->head = skb;
		ctfpool->tail = skb;
		skb->next = NULL;  /* tail end */
	}
	else if (!ctfpool_tail) {
		skb->next = (struct sk_buff *)ctfpool->head;
		ctfpool->head = skb;
	}
	else {
		((struct sk_buff *)ctfpool->tail)->next = skb;
		ctfpool->tail = skb;
		skb->next = NULL;  /* tail end */
	}
#else
	skb->next = (struct sk_buff *)ctfpool->head;
	ctfpool->head = (void *)skb;
#endif

	ctfpool->fast_frees++;
	ctfpool->curr_obj++;

	ASSERT(ctfpool->curr_obj <= ctfpool->max_obj);
	CTFPOOL_UNLOCK(ctfpool, flags);
}
#endif /* CTFPOOL */

/* Free the driver packet. Free the tag if present */
void BCMFASTPATH
osl_pktfree(osl_t *osh, void *p, bool send)
{
	struct sk_buff *skb, *nskb;

	skb = (struct sk_buff*) p;

	if (send && osh->pub.tx_fn)
		osh->pub.tx_fn(osh->pub.tx_ctx, p, 0);

	PKTDBG_TRACE(osh, (void *) skb, PKTLIST_PKTFREE);

	/* perversion: we use skb->next to chain multi-skb packets */
	while (skb) {
		nskb = skb->next;
		skb->next = NULL;

#ifdef BCMDBG_CTRACE
		DEL_CTRACE(osh, skb);
#endif

#ifdef CTFMAP
		/* Clear the map ptr before freeing */
		PKTCLRCTF(osh, skb);
		CTFMAPPTR(osh, skb) = NULL;
#endif

#ifdef CTFPOOL
		if (PKTISFAST(osh, skb)) {
			if (atomic_read(&skb->users) == 1)
				smp_rmb();
			else if (!atomic_dec_and_test(&skb->users))
				goto next_skb;
			osl_pktfastfree(osh, skb);
		} else
#endif
		{
			if (skb->destructor)
				/* cannot kfree_skb() on hard IRQ (net/core/skbuff.c) if
				 * destructor exists
				 */
				dev_kfree_skb_any(skb);
			else
				/* can free immediately (even in_irq()) if destructor
				 * does not exist
				 */
				dev_kfree_skb(skb);
		}
#ifdef CTFPOOL
next_skb:
#endif
		atomic_dec(&osh->pktalloced);
		skb = nskb;
	}
}

#ifdef DHD_USE_STATIC_BUF
void*
osl_pktget_static(osl_t *osh, uint len)
{
	int i = 0;
	struct sk_buff *skb;

	if (len > (PAGE_SIZE*2)) {
		printk("%s: attempt to allocate huge packet (0x%x)\n", __FUNCTION__, len);
		return osl_pktget(osh, len);
	}

	down(&bcm_static_skb->osl_pkt_sem);

	if (len <= PAGE_SIZE) {
		for (i = 0; i < STATIC_PKT_MAX_NUM; i++) {
			if (bcm_static_skb->pkt_use[i] == 0)
				break;
		}

		if (i != STATIC_PKT_MAX_NUM) {
			bcm_static_skb->pkt_use[i] = 1;
			up(&bcm_static_skb->osl_pkt_sem);
			skb = bcm_static_skb->skb_4k[i];
			skb->tail = skb->data + len;
			skb->len = len;
			return skb;
		}
	}


	for (i = 0; i < STATIC_PKT_MAX_NUM; i++) {
		if (bcm_static_skb->pkt_use[i+STATIC_PKT_MAX_NUM] == 0)
			break;
	}

	if (i != STATIC_PKT_MAX_NUM) {
		bcm_static_skb->pkt_use[i+STATIC_PKT_MAX_NUM] = 1;
		up(&bcm_static_skb->osl_pkt_sem);
		skb = bcm_static_skb->skb_8k[i];
		skb->tail = skb->data + len;
		skb->len = len;
		return skb;
	}

	up(&bcm_static_skb->osl_pkt_sem);
	printk("%s: all static pkt in use!\n", __FUNCTION__);
	return osl_pktget(osh, len);
}

void
osl_pktfree_static(osl_t *osh, void *p, bool send)
{
	int i;

	for (i = 0; i < STATIC_PKT_MAX_NUM; i++) {
		if (p == bcm_static_skb->skb_4k[i]) {
			down(&bcm_static_skb->osl_pkt_sem);
			bcm_static_skb->pkt_use[i] = 0;
			up(&bcm_static_skb->osl_pkt_sem);
			return;
		}
	}

	for (i = 0; i < STATIC_PKT_MAX_NUM; i++) {
		if (p == bcm_static_skb->skb_8k[i]) {
			down(&bcm_static_skb->osl_pkt_sem);
			bcm_static_skb->pkt_use[i + STATIC_PKT_MAX_NUM] = 0;
			up(&bcm_static_skb->osl_pkt_sem);
			return;
		}
	}

	return osl_pktfree(osh, p, send);
}
#endif /* DHD_USE_STATIC_BUF */

uint32
osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	uint val = 0;
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	/* only 4byte access supported */
	ASSERT(size == 4);

	do {
		pci_read_config_dword(osh->pdev, offset, &val);
		if (val != 0xffffffff)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG READ access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif /* BCMDBG */

	return (val);
}

void
osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	/* only 4byte access supported */
	ASSERT(size == 4);

	do {
		pci_write_config_dword(osh->pdev, offset, val);
		if (offset != PCI_BAR0_WIN)
			break;
		if (osl_pci_read_config(osh, offset, size) == val)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG WRITE access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif /* BCMDBG */
}

/* return bus # for the pci device pointed by osh->pdev */
uint
osl_pci_bus(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

#if defined(__ARM_ARCH_7A__) && LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
	return pci_domain_nr(((struct pci_dev *)osh->pdev)->bus);
#else
	return ((struct pci_dev *)osh->pdev)->bus->number;
#endif
}

/* return slot # for the pci device pointed by osh->pdev */
uint
osl_pci_slot(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

#if defined(__ARM_ARCH_7A__) && LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn) + 1;
#else
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn);
#endif
}

/* return domain # for the pci device pointed by osh->pdev */
uint
osl_pcie_domain(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return pci_domain_nr(((struct pci_dev *)osh->pdev)->bus);
}

/* return bus # for the pci device pointed by osh->pdev */
uint
osl_pcie_bus(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return ((struct pci_dev *)osh->pdev)->bus->number;
}

/* return the pci device pointed by osh->pdev */
struct pci_dev *
osl_pci_device(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return osh->pdev;
}

static void
osl_pcmcia_attr(osl_t *osh, uint offset, char *buf, int size, bool write)
{
}

void
osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, FALSE);
}

void
osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, TRUE);
}

void *
osl_malloc(osl_t *osh, uint size)
{
	void *addr;

	/* only ASSERT if osh is defined */
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf)
	{
		int i = 0;
		if ((size >= PAGE_SIZE)&&(size <= STATIC_BUF_SIZE))
		{
			down(&bcm_static_buf->static_sem);

			for (i = 0; i < STATIC_BUF_MAX_NUM; i++)
			{
				if (bcm_static_buf->buf_use[i] == 0)
					break;
			}

			if (i == STATIC_BUF_MAX_NUM)
			{
				up(&bcm_static_buf->static_sem);
				printk("all static buff in use!\n");
				goto original;
			}

			bcm_static_buf->buf_use[i] = 1;
			up(&bcm_static_buf->static_sem);

			bzero(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i, size);
			if (osh)
				atomic_add(size, &osh->malloced);

			return ((void *)(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i));
		}
	}
original:
#endif /* DHD_USE_STATIC_BUF */

	if ((addr = kmalloc(size, GFP_ATOMIC)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh)
		atomic_add(size, &osh->malloced);

	return (addr);
}

void
osl_mfree(osl_t *osh, void *addr, uint size)
{
#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf)
	{
		if ((addr > (void *)bcm_static_buf) && ((unsigned char *)addr
			<= ((unsigned char *)bcm_static_buf + STATIC_BUF_TOTAL_LEN)))
		{
			int buf_idx = 0;

			buf_idx = ((unsigned char *)addr - bcm_static_buf->buf_ptr)/STATIC_BUF_SIZE;

			down(&bcm_static_buf->static_sem);
			bcm_static_buf->buf_use[buf_idx] = 0;
			up(&bcm_static_buf->static_sem);

			if (osh) {
				ASSERT(osh->magic == OS_HANDLE_MAGIC);
				atomic_sub(size, &osh->malloced);
			}
			return;
		}
	}
#endif /* DHD_USE_STATIC_BUF */
	if (osh) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
		atomic_sub(size, &osh->malloced);
	}
	kfree(addr);
}

uint
osl_malloced(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (atomic_read(&osh->malloced));
}

uint
osl_malloc_failed(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->failed);
}


uint
osl_dma_consistent_align(void)
{
	return (PAGE_SIZE);
}

void*
osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align_bits, uint *alloced, ulong *pap)
{
	void *va;
	uint16 align = (1 << align_bits);
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	if (!ISALIGNED(DMA_CONSISTENT_ALIGN, align))
		size += align;
	*alloced = size;
#ifdef __ARM_ARCH_7A__
#ifndef	BCM_SECURE_DMA
	va = kmalloc(size, GFP_ATOMIC | __GFP_ZERO);
	if (va)
		*pap = (ulong)__virt_to_phys((ulong)va);
#else
	va = osl_sec_dma_alloc_consistent(osh, size, align_bits, pap);
#endif /* __ARM_ARCH_7A__ */
#else
	va = pci_alloc_consistent(osh->pdev, size, (dma_addr_t*)pap);
#endif /* BCM_SECURE_DMA */
	return va;
}

void
osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

#ifdef __ARM_ARCH_7A__
#ifndef BCM_SECURE_DMA
	kfree(va);
#endif /* BCM_SECURE_DMA */
#else
	pci_free_consistent(osh->pdev, size, va, (dma_addr_t)pa);
#endif /* __ARM_ARCH_7A__ */
}

uint BCMFASTPATH
osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p, hnddma_seg_map_t *dmah)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;

#if defined(__ARM_ARCH_7A__) && defined(BCMDMASGLISTOSL)
	if (dmah != NULL) {
		int32 nsegs, i, totsegs = 0, totlen = 0;
		struct scatterlist *sg, _sg[MAX_DMA_SEGS * 2];
		struct sk_buff *skb;
		for (skb = (struct sk_buff *)p; skb != NULL; skb = PKTNEXT(osh, skb)) {
			sg = &_sg[totsegs];
			if (skb_is_nonlinear(skb)) {
				nsegs = skb_to_sgvec(skb, sg, 0, PKTLEN(osh, skb));
				ASSERT((nsegs > 0) && (totsegs + nsegs <= MAX_DMA_SEGS));
				pci_map_sg(osh->pdev, sg, nsegs, dir);
			} else {
				nsegs = 1;
				ASSERT(totsegs + nsegs <= MAX_DMA_SEGS);
				sg->page_link = 0;
				sg_set_buf(sg, PKTDATA(osh, skb), PKTLEN(osh, skb));
#ifdef	CTFMAP
				/* Map size bytes (not skb->len) for ctf bufs */
				pci_map_single(osh->pdev, PKTDATA(osh, skb),
				    PKTISCTF(osh, skb) ? CTFMAPSZ : PKTLEN(osh, skb), dir);
#else
				pci_map_single(osh->pdev, PKTDATA(osh, skb), PKTLEN(osh, skb), dir);
#endif
			}
			totsegs += nsegs;
			totlen += PKTLEN(osh, skb);
		}
		dmah->nsegs = totsegs;
		dmah->origsize = totlen;
		for (i = 0, sg = _sg; i < totsegs; i++, sg++) {
			dmah->segs[i].addr = sg_phys(sg);
			dmah->segs[i].length = sg->length;
		}
		return dmah->segs[0].addr;
	}
#endif /* __ARM_ARCH_7A__ && BCMDMASGLISTOSL */

	return (pci_map_single(osh->pdev, va, size, dir));
}

void BCMFASTPATH
osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;
	pci_unmap_single(osh->pdev, (uint32)pa, size, dir);
}


void
osl_delay(uint usec)
{
	uint d;

	while (usec > 0) {
		d = MIN(usec, 1000);
		udelay(d);
		usec -= d;
	}
}

#if defined(DSLCPE_DELAY)

void
osl_oshsh_init(osl_t *osh, shared_osl_t* oshsh)
{
	extern unsigned long loops_per_jiffy;
	osh->oshsh = oshsh;
	osh->oshsh->MIPS = loops_per_jiffy / (500000/HZ);
}

int
in_long_delay(osl_t *osh)
{
	return osh->oshsh->long_delay;
}

void
osl_long_delay(osl_t *osh, uint usec, bool yield)
{
	uint d;
	bool yielded = TRUE;
	int usec_to_delay = usec;
	unsigned long tick1, tick2, tick_diff = 0;

	/* delay at least requested usec */
	while (usec_to_delay > 0) {
		if (!yield || !yielded) {
			d = MIN(usec_to_delay, 10);
			udelay(d);
			usec_to_delay -= d;
		}
		if (usec_to_delay > 0) {
			osh->oshsh->long_delay++;
			OSL_GETCYCLES(tick1);
			spin_unlock_bh(osh->oshsh->lock);
			if (usec_to_delay > 0 && !in_irq() && !in_softirq() && !in_interrupt()) {
				schedule();
				yielded = TRUE;
			} else {
				yielded = FALSE;
			}
			spin_lock_bh(osh->oshsh->lock);
			OSL_GETCYCLES(tick2);

			if (yielded) {
				tick_diff = TICKDIFF(tick2, tick1);
				tick_diff = (tick_diff * 2)/(osh->oshsh->MIPS);
				if (tick_diff) {
					usec_to_delay -= tick_diff;
				} else
					yielded = 0;
			}
			osh->oshsh->long_delay--;
			ASSERT(osh->oshsh->long_delay >= 0);
		}
	}
}
#endif /* DSLCPE_DELAY */

/* Clone a packet.
 * The pkttag contents are NOT cloned.
 */
#ifdef BCMDBG_CTRACE
void *
osl_pktdup(osl_t *osh, void *skb, int line, char *file)
#else
void *
osl_pktdup(osl_t *osh, void *skb)
#endif /* BCMDBG_CTRACE */
{
	void * p;

	ASSERT(!PKTISCHAINED(skb));

	/* clear the CTFBUF flag if set and map the rest of the buffer
	 * before cloning.
	 */
	PKTCTFMAP(osh, skb);

	if ((p = skb_clone((struct sk_buff *)skb, GFP_ATOMIC)) == NULL)
		return NULL;

#ifdef CTFPOOL
	if (PKTISFAST(osh, skb)) {
		ctfpool_t *ctfpool;

		/* if the buffer allocated from ctfpool is cloned then
		 * we can't be sure when it will be freed. since there
		 * is a chance that we will be losing a buffer
		 * from our pool, we increment the refill count for the
		 * object to be alloced later.
		 */
		ctfpool = (ctfpool_t *)CTFPOOLPTR(osh, skb);
		ASSERT(ctfpool != NULL);
		PKTCLRFAST(osh, p);
		PKTCLRFAST(osh, skb);
		ctfpool->refills++;
	}
#endif /* CTFPOOL */

	/* Clear PKTC  context */
	PKTSETCLINK(p, NULL);
	PKTCCLRFLAGS(p);
	PKTCSETCNT(p, 1);
	PKTCSETLEN(p, PKTLEN(osh, skb));

	/* skb_clone copies skb->cb.. we don't want that */
	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(p);

	/* Increment the packet counter */
	atomic_inc(&osh->pktalloced);
#ifdef BCMDBG_CTRACE
	ADD_CTRACE(osh, (struct sk_buff *)p, file, line);
#endif
	return (p);
}

#ifdef BCMDBG_CTRACE
int osl_pkt_is_frmnative(osl_t *osh, struct sk_buff *pkt)
{
	unsigned long flags;
	struct sk_buff *skb;
	int ck = FALSE;

	spin_lock_irqsave(&osh->ctrace_lock, flags);

	list_for_each_entry(skb, &osh->ctrace_list, ctrace_list) {
		if (pkt == skb) {
			ck = TRUE;
			break;
		}
	}

	spin_unlock_irqrestore(&osh->ctrace_lock, flags);
	return ck;
}

void osl_ctrace_dump(osl_t *osh, struct bcmstrbuf *b)
{
	unsigned long flags;
	struct sk_buff *skb;
	int idx = 0;
	int i, j;

	spin_lock_irqsave(&osh->ctrace_lock, flags);

	if (b != NULL)
		bcm_bprintf(b, " Total %d sbk not free\n", osh->ctrace_num);
	else
		printk(" Total %d sbk not free\n", osh->ctrace_num);

	list_for_each_entry(skb, &osh->ctrace_list, ctrace_list) {
		if (b != NULL)
			bcm_bprintf(b, "[%d] skb %p:\n", ++idx, skb);
		else
			printk("[%d] skb %p:\n", ++idx, skb);

		for (i = 0; i < skb->ctrace_count; i++) {
			j = (skb->ctrace_start + i) % CTRACE_NUM;
			if (b != NULL)
				bcm_bprintf(b, "    [%s(%d)]\n", skb->func[j], skb->line[j]);
			else
				printk("    [%s(%d)]\n", skb->func[j], skb->line[j]);
		}
		if (b != NULL)
			bcm_bprintf(b, "\n");
		else
			printk("\n");
	}

	spin_unlock_irqrestore(&osh->ctrace_lock, flags);

	return;
}
#endif /* BCMDBG_CTRACE */


/*
 * OSLREGOPS specifies the use of osl_XXX routines to be used for register access
 */
#ifdef OSLREGOPS
uint8
osl_readb(osl_t *osh, volatile uint8 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint8)((rreg)(ctx, (void*)r, sizeof(uint8)));
}


uint16
osl_readw(osl_t *osh, volatile uint16 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint16)((rreg)(ctx, (void*)r, sizeof(uint16)));
}

uint32
osl_readl(osl_t *osh, volatile uint32 *r)
{
	osl_rreg_fn_t rreg	= ((osl_pubinfo_t*)osh)->rreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	return (uint32)((rreg)(ctx, (void*)r, sizeof(uint32)));
}

void
osl_writeb(osl_t *osh, volatile uint8 *r, uint8 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (void*)r, v, sizeof(uint8)));
}


void
osl_writew(osl_t *osh, volatile uint16 *r, uint16 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (void*)r, v, sizeof(uint16)));
}

void
osl_writel(osl_t *osh, volatile uint32 *r, uint32 v)
{
	osl_wreg_fn_t wreg	= ((osl_pubinfo_t*)osh)->wreg_fn;
	void *ctx		= ((osl_pubinfo_t*)osh)->reg_ctx;

	((wreg)(ctx, (void*)r, v, sizeof(uint32)));
}
#endif /* OSLREGOPS */

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 */
#ifdef BINOSL

uint32
osl_sysuptime(void)
{
	return ((uint32)jiffies * (1000 / HZ));
}

int
osl_printf(const char *format, ...)
{
	va_list args;
	static char printbuf[1024];
	int len;

	/* sprintf into a local buffer because there *is* no "vprintk()".. */
	va_start(args, format);
	len = vsnprintf(printbuf, 1024, format, args);
	va_end(args);

	if (len > sizeof(printbuf)) {
		printk("osl_printf: buffer overrun\n");
		return (0);
	}

	return (printk("%s", printbuf));
}

int
osl_sprintf(char *buf, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsprintf(buf, format, args);
	va_end(args);
	return (rc);
}

int
osl_snprintf(char *buf, size_t n, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsnprintf(buf, n, format, args);
	va_end(args);
	return (rc);
}

int
osl_vsprintf(char *buf, const char *format, va_list ap)
{
	return (vsprintf(buf, format, ap));
}

int
osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap)
{
	return (vsnprintf(buf, n, format, ap));
}

int
osl_strcmp(const char *s1, const char *s2)
{
	return (strcmp(s1, s2));
}

int
osl_strncmp(const char *s1, const char *s2, uint n)
{
	return (strncmp(s1, s2, n));
}

int
osl_strlen(const char *s)
{
	return (strlen(s));
}

char*
osl_strcpy(char *d, const char *s)
{
	return (strcpy(d, s));
}

char*
osl_strncpy(char *d, const char *s, uint n)
{
	return (strncpy(d, s, n));
}

char*
osl_strchr(const char *s, int c)
{
	return (strchr(s, c));
}

char*
osl_strrchr(const char *s, int c)
{
	return (strrchr(s, c));
}

void*
osl_memset(void *d, int c, size_t n)
{
	return memset(d, c, n);
}

void*
osl_memcpy(void *d, const void *s, size_t n)
{
	return memcpy(d, s, n);
}

void*
osl_memmove(void *d, const void *s, size_t n)
{
	return memmove(d, s, n);
}

int
osl_memcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

uint32
osl_readl(volatile uint32 *r)
{
	return (readl(r));
}

uint16
osl_readw(volatile uint16 *r)
{
	return (readw(r));
}

uint8
osl_readb(volatile uint8 *r)
{
	return (readb(r));
}

void
osl_writel(uint32 v, volatile uint32 *r)
{
	writel(v, r);
}

void
osl_writew(uint16 v, volatile uint16 *r)
{
	writew(v, r);
}

void
osl_writeb(uint8 v, volatile uint8 *r)
{
	writeb(v, r);
}

void *
osl_uncached(void *va)
{
#ifdef mips
	return ((void*)KSEG1ADDR(va));
#else
	return ((void*)va);
#endif /* mips */
}

void *
osl_cached(void *va)
{
#ifdef mips
	return ((void*)KSEG0ADDR(va));
#else
	return ((void*)va);
#endif /* mips */
}

uint
osl_getcycles(void)
{
	uint cycles;

#if defined(mips)
	cycles = read_c0_count() * 2;
#elif defined(__i386__)
	rdtscl(cycles);
#else
	cycles = 0;
#endif /* defined(mips) */
	return cycles;
}

void *
osl_reg_map(uint32 pa, uint size)
{
	return (ioremap_nocache((unsigned long)pa, (unsigned long)size));
}

void
osl_reg_unmap(void *va)
{
	iounmap(va);
}

int
osl_busprobe(uint32 *val, uint32 addr)
{
#ifdef mips
	return get_dbe(*val, (uint32 *)addr);
#else
	*val = readl((uint32 *)(uintptr)addr);
	return 0;
#endif /* mips */
}

bool
osl_pktshared(void *skb)
{
	return (((struct sk_buff*)skb)->cloned);
}

uchar*
osl_pktdata(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->data);
}

uint
osl_pktlen(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->len);
}

uint
osl_pktheadroom(osl_t *osh, void *skb)
{
	return (uint) skb_headroom((struct sk_buff *) skb);
}

uint
osl_pkttailroom(osl_t *osh, void *skb)
{
	return (uint) skb_tailroom((struct sk_buff *) skb);
}

void*
osl_pktnext(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->next);
}

void
osl_pktsetnext(void *skb, void *x)
{
	((struct sk_buff*)skb)->next = (struct sk_buff*)x;
}

void
osl_pktsetlen(osl_t *osh, void *skb, uint len)
{
	__skb_trim((struct sk_buff*)skb, len);
}

uchar*
osl_pktpush(osl_t *osh, void *skb, int bytes)
{
	return (skb_push((struct sk_buff*)skb, bytes));
}

uchar*
osl_pktpull(osl_t *osh, void *skb, int bytes)
{
	return (skb_pull((struct sk_buff*)skb, bytes));
}

void*
osl_pkttag(void *skb)
{
	return ((void*)(((struct sk_buff*)skb)->cb));
}

void*
osl_pktlink(void *skb)
{
	return (((struct sk_buff*)skb)->prev);
}

void
osl_pktsetlink(void *skb, void *x)
{
	((struct sk_buff*)skb)->prev = (struct sk_buff*)x;
}

uint
osl_pktprio(void *skb)
{
	return (((struct sk_buff*)skb)->priority);
}

void
osl_pktsetprio(void *skb, uint x)
{
	((struct sk_buff*)skb)->priority = x;
}
#endif	/* BINOSL */

uint
osl_pktalloced(osl_t *osh)
{
	return (atomic_read(&osh->pktalloced));
}

/* Linux Kernel: File Operations: start */
void *
osl_os_open_image(char *filename)
{
	struct file *fp;

	fp = filp_open(filename, O_RDONLY, 0);
	/*
	 * 2.6.11 (FC4) supports filp_open() but later revs don't?
	 * Alternative:
	 * fp = open_namei(AT_FDCWD, filename, O_RD, 0);
	 * ???
	 */
	 if (IS_ERR(fp))
		 fp = NULL;

	 return fp;
}

int
osl_os_get_image_block(char *buf, int len, void *image)
{
	struct file *fp = (struct file *)image;
	int rdlen;

	if (!image)
		return 0;

	rdlen = kernel_read(fp, fp->f_pos, buf, len);
	if (rdlen > 0)
		fp->f_pos += rdlen;

	return rdlen;
}

void
osl_os_close_image(void *image)
{
	if (image)
		filp_close((struct file *)image, NULL);
}

int
osl_os_image_size(void *image)
{
	int len = 0, curroffset;

	if (image) {
		/* store the current offset */
		curroffset = generic_file_llseek(image, 0, 1);
		/* goto end of file to get length */
		len = generic_file_llseek(image, 0, 2);
		/* restore back the offset */
		generic_file_llseek(image, curroffset, 0);
	}
	return len;
}
/* Linux Kernel: File Operations: end */

#ifdef BCM_SECURE_DMA

void
osl_sec_dma_setup_contig_mem(osl_t *osh, unsigned long memsize, int regn)
{
	int ret;

#if defined(__ARM_ARCH_7A__)
	if (regn == CONT_ARMREGION) {
		ret = osl_sec_dma_alloc_contig_mem(osh, memsize, regn);
		if (ret != BCME_OK)
			printk("linux_osl.c: CMA memory access failed\n");
	}
#else
	if (regn == CONT_MIPREGION) {
		ret = osl_sec_dma_alloc_mips_contig_mem(osh, memsize, regn);
		if (ret != BCME_OK)
			printk("linux_osl.c: MIPS contig memory access failed\n");
	}
#endif
	/* implement the MIPS Here */

}

#if !defined(__ARM_ARCH_7A__)
int
osl_sec_dma_alloc_mips_contig_mem(osl_t *osh, unsigned long memsize, int regn)
{

	unsigned long addr;

	if (regn < 0) {
		printk("osl_sec_dma_alloc_mips_contig_mem:missing contig_region parameter\n");
		return BCME_ERROR;
	}

	osh->dev = &(((struct pci_dev *)osh->pdev)->dev);

	if (bmem_region_info(regn, &addr, &memsize) < 0) {
		printk("osl_sec_dma_alloc_mips_contig_mem:index is invalid %d \n", regn);
		return BCME_ERROR;
	}
	if (memsize < 0) {
		printk("osl_sec_dma_alloc_mips_contig_mem:bmem region is too small \n");
		return BCME_ERROR;
	}

	osh->contig_base_alloc = addr;
	osh->contig_base = (u32)osh->contig_base_alloc;
	printk("contig base alloc=%x \n", osh->contig_base_alloc);

	return BCME_OK;

}

#else
int
osl_sec_dma_alloc_contig_mem(osl_t *osh, unsigned long memsize, int regn)
{
	u64 addr;

	printk("linux_osl.c: The value of cma mem block size = %ld\n", memsize);
	osh->cma = cma_dev_get_cma_dev(regn);
	printk("The value of cma = %p\n", osh->cma);
	if (!osh->cma) {
		printk("linux_osl.c:contig_region index is invalid\n");
		return BCME_ERROR;
	}
	if (cma_dev_get_mem(osh->cma, &addr, (u32)memsize, SEC_DMA_ALIGN) < 0) {
		printk("linux_osl.c: contiguous memory block allocation failure\n");
		return BCME_ERROR;
	}
	osh->dev = osh->cma->dev;
	osh->contig_base_alloc = (u32)addr;
	osh->contig_base = (u32)osh->contig_base_alloc;
	printk("contig base alloc=%x \n", osh->contig_base_alloc);

	return BCME_OK;

}
#endif /* !defined(__ARM_ARCH_7A__) */

#if defined(__ARM_ARCH_7A__)
void
osl_sec_dma_free_contig_mem(osl_t *osh, u32 memsize, int regn)
{
	int ret;

	ret = cma_dev_put_mem(osh->cma, (u64)osh->contig_base, memsize);
	if (ret)
	printf("%s contig base free failed\n", __FUNCTION__);

}
#endif /* defined(__ARM_ARCH_7A__) */


void *
osl_sec_dma_ioremap(osl_t *osh, struct page *page, size_t size, bool iscache, bool isdecr)
{

	struct page **map;
	int order, i;
	void *addr = NULL;

	size = PAGE_ALIGN(size);
	order = get_order(size);

	map = kmalloc(sizeof(struct page *) << order, GFP_ATOMIC);

	for (i = 0; i < (size >> PAGE_SHIFT); i++)
		map[i] = page + i;

	if (iscache) {
#if defined(__ARM_ARCH_7A__)
		addr = vmap(map, size >> PAGE_SHIFT, VM_MAP, __pgprot(PAGE_KERNEL));
#else
		addr = vmap(map, size >> PAGE_SHIFT, VM_MAP, PAGE_KERNEL);
#endif
		if (isdecr)
			osh->contig_delta_va_pa = (addr - page_to_phys(page));
	} else {

#if defined(__ARM_ARCH_7A__)
		addr = vmap(map, size >> PAGE_SHIFT, VM_MAP,
		pgprot_noncached(__pgprot(PAGE_KERNEL)));
#else
		addr = vmap(map, size >> PAGE_SHIFT, VM_MAP,  PAGE_KERNEL_UNCACHED);
#endif
		if (isdecr)
			osh->contig_delta_va_pa = (addr - page_to_phys(page));
	}

	kfree(map);
	return (void *)addr;
}

void
osl_sec_dma_iounmap(osl_t *osh, void *contig_base_va, size_t size)
{
	vunmap(contig_base_va);
}

void
osl_sec_dma_deinit_elem_mem_block(osl_t *osh, size_t mbsize, int max, void *sec_list_base)
{

	if (sec_list_base)
		kfree(sec_list_base);

}

void
osl_sec_dma_init_elem_mem_block(osl_t *osh, size_t mbsize, int max, sec_mem_elem_t **list)
{
	int i;
	sec_mem_elem_t *sec_mem_elem;

	if ((sec_mem_elem = kmalloc(sizeof(sec_mem_elem_t)*(max), GFP_ATOMIC)) != NULL) {

		*list = sec_mem_elem;

		for (i = 0; i < max-1; i++) {
			sec_mem_elem->next = (sec_mem_elem + 1);
			sec_mem_elem->size = mbsize;
			sec_mem_elem->pa_cma = (u32)osh->contig_base_alloc;
			sec_mem_elem->vac = osh->contig_base_alloc_va;

			osh->contig_base_alloc += mbsize;
			osh->contig_base_alloc_va += mbsize;

			sec_mem_elem = sec_mem_elem + 1;
		}
		sec_mem_elem->next = NULL;
		sec_mem_elem->size = mbsize;
		sec_mem_elem->pa_cma = (u32)osh->contig_base_alloc;
		sec_mem_elem->vac = osh->contig_base_alloc_va;

		osh->contig_base_alloc += mbsize;
		osh->contig_base_alloc_va += mbsize;

	}
	else
		printf("%s sec mem elem kmalloc failed\n", __FUNCTION__);

}


sec_mem_elem_t * BCMFASTPATH
osl_sec_dma_alloc_mem_elem(osl_t *osh, void *va, uint size, int direction,
	struct sec_cma_info *ptr_cma_info, uint offset)
{
	sec_mem_elem_t *sec_mem_elem = NULL;

	if (size <= 512 && osh->sec_list_512) {
		sec_mem_elem = osh->sec_list_512;
		osh->sec_list_512 = sec_mem_elem->next;
	}
	else if (size <= 2048 && osh->sec_list_2048) {
		sec_mem_elem = osh->sec_list_2048;
		osh->sec_list_2048 = sec_mem_elem->next;
	}
	else if (osh->sec_list_4096) {
		sec_mem_elem = osh->sec_list_4096;
		osh->sec_list_4096 = sec_mem_elem->next;
	} else {
		printf("%s No matching Pool available size=%d\n", __FUNCTION__, size);
		return NULL;
	}

	if (ptr_cma_info->sec_alloc_list_tail != NULL) {
		ptr_cma_info->sec_alloc_list_tail->next = sec_mem_elem;
	}

	if (sec_mem_elem != NULL) {
		sec_mem_elem->next = NULL;

	if (ptr_cma_info->sec_alloc_list_tail) {
		ptr_cma_info->sec_alloc_list_tail->next = sec_mem_elem;
	}

	ptr_cma_info->sec_alloc_list_tail = sec_mem_elem;
	if (ptr_cma_info->sec_alloc_list == NULL)
		ptr_cma_info->sec_alloc_list = sec_mem_elem;
	}
	return sec_mem_elem;
}


void BCMFASTPATH
osl_sec_dma_free_mem_elem(osl_t *osh, sec_mem_elem_t *sec_mem_elem)
{
	sec_mem_elem->dma_handle = 0x0;
	sec_mem_elem->va = NULL;

	if (sec_mem_elem->size == 512) {
		sec_mem_elem->next = osh->sec_list_512;
		osh->sec_list_512 = sec_mem_elem;
	}
	else if (sec_mem_elem->size == 2048) {
		sec_mem_elem->next = osh->sec_list_2048;
		osh->sec_list_2048 = sec_mem_elem;
	}
	else if (sec_mem_elem->size == 4096) {
		sec_mem_elem->next = osh->sec_list_4096;
		osh->sec_list_4096 = sec_mem_elem;
	}
	else
	printf("%s free failed size=%d\n", __FUNCTION__, sec_mem_elem->size);
}


sec_mem_elem_t * BCMFASTPATH
osl_sec_dma_find_elem(osl_t *osh, struct sec_cma_info *ptr_cma_info, void *va)
{
	sec_mem_elem_t *sec_mem_elem = ptr_cma_info->sec_alloc_list;

	while (sec_mem_elem != NULL)
	{
		if (sec_mem_elem->va == va)
			return sec_mem_elem;

		sec_mem_elem = sec_mem_elem->next;
	}
	return NULL;
}


sec_mem_elem_t * BCMFASTPATH
osl_sec_dma_find_rem_elem(osl_t *osh, struct sec_cma_info *ptr_cma_info, dma_addr_t dma_handle)
{
	sec_mem_elem_t *sec_mem_elem = ptr_cma_info->sec_alloc_list;
	sec_mem_elem_t *sec_prv_elem = ptr_cma_info->sec_alloc_list;

	if (sec_mem_elem->dma_handle == dma_handle) {

		ptr_cma_info->sec_alloc_list = sec_mem_elem->next;

		if (sec_mem_elem == ptr_cma_info->sec_alloc_list_tail)
			ptr_cma_info->sec_alloc_list_tail = NULL;

		return sec_mem_elem;
	}

	while (sec_mem_elem != NULL) {

		if (sec_mem_elem->dma_handle == dma_handle) {

			sec_prv_elem->next = sec_mem_elem->next;
			if (sec_mem_elem == ptr_cma_info->sec_alloc_list_tail)
				ptr_cma_info->sec_alloc_list_tail = sec_prv_elem;

			return sec_mem_elem;
		}
		sec_prv_elem = sec_mem_elem;
		sec_mem_elem = sec_mem_elem->next;
	}
	return NULL;
}


dma_addr_t BCMFASTPATH
osl_sec_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *dmah, void *ptr_cma_info, uint offset)
{

	sec_mem_elem_t *sec_mem_elem;
	struct page *pa_cma_page;
	void *pa_cma_kmap_va = NULL;
	int *fragva;
	uint buflen = 0;
	struct sk_buff *skb;
	dma_addr_t dma_handle = 0x0;
	uint loffset;
	int i = 0;
	BCM_REFERENCE(fragva);
	BCM_REFERENCE(i);

	sec_mem_elem = osl_sec_dma_alloc_mem_elem(osh, va, size, direction, ptr_cma_info, offset);

	if (sec_mem_elem == NULL) {
		printk("linux_osl.c: osl_sec_dma_map - cma allocation failed\n");
		return 0;
	}
	sec_mem_elem->va = va;
	sec_mem_elem->direction = direction;

	pa_cma_page = PHYS_TO_PAGE(sec_mem_elem->pa_cma);
	loffset = sec_mem_elem->pa_cma -(sec_mem_elem->pa_cma & ~(PAGE_SIZE-1));
	/* pa_cma_kmap_va = kmap_atomic(pa_cma_page);
	* pa_cma_kmap_va += loffset;
	*/

	pa_cma_kmap_va = sec_mem_elem->vac;

	if (direction == DMA_TX) {

		if (p == NULL) {

			memcpy(pa_cma_kmap_va+offset, va, size);
			buflen = size;
			/* prhex("Txpkt",pa_cma_kmap_va, size); */
		}
#ifdef NOT_YET
		else {
			for (skb = (struct sk_buff *)p; skb != NULL; skb = PKTNEXT(osh, skb)) {
				if (skb_is_nonlinear(skb)) {


					for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
						skb_frag_t *f = &skb_shinfo(skb)->frags[i];
						fragva = kmap_atomic(skb_frag_page(f));
						memcpy((pa_cma_kmap_va+buflen),
						(fragva + f->page_offset), skb_frag_size(f));
						kunmap_atomic(fragva);
						buflen += skb_frag_size(f);
					}
				}
				else {
					memcpy((pa_cma_kmap_va+buflen), skb->data, skb->len);
					buflen += skb->len;
				}
			}

		}
#endif /* NOT_YET */

		if (dmah) {
			dmah->nsegs = 1;
			dmah->origsize = buflen;
		}
	}

	else if (direction == DMA_RX)
	{
		buflen = size;
		if ((p != NULL) && (dmah != NULL)) {
			dmah->nsegs = 1;
			dmah->origsize = buflen;
		}
	}

	if (direction == DMA_RX) {


		*(uint32 *)(pa_cma_kmap_va) = 0x0;
		dma_handle = dma_map_page(osh->dev, pa_cma_page, loffset,
		sizeof(int), DMA_TO_DEVICE);
	}

	if (direction == DMA_RX || direction == DMA_TX) {

		dma_handle = dma_map_page(osh->dev, pa_cma_page, loffset+offset, buflen,
			(direction == DMA_TX ? DMA_TO_DEVICE:DMA_FROM_DEVICE));

	}
	if (dmah) {
		dmah->segs[0].addr = dma_handle;
		dmah->segs[0].length = buflen;
	}
	sec_mem_elem->dma_handle = dma_handle;
	/* kunmap_atomic(pa_cma_kmap_va-loffset); */
	return dma_handle;
}

dma_addr_t BCMFASTPATH
osl_sec_dma_dd_map(osl_t *osh, void *va, uint size, int direction, void *p, hnddma_seg_map_t *map)
{

	struct page *pa_cma_page;
	phys_addr_t pa_cma;
	dma_addr_t dma_handle = 0x0;
	uint loffset;

	pa_cma = (va - osh->contig_delta_va_pa);

	pa_cma_page = PHYS_TO_PAGE(pa_cma);

	loffset = pa_cma -(pa_cma & ~(PAGE_SIZE-1));

	dma_handle = dma_map_page(osh->dev, pa_cma_page, loffset, size,
		(direction == DMA_TX ? DMA_TO_DEVICE:DMA_FROM_DEVICE));

	return dma_handle;

}


void BCMFASTPATH
osl_sec_dma_unmap(osl_t *osh, dma_addr_t dma_handle, uint size, int direction,
void *p, hnddma_seg_map_t *map,	void *ptr_cma_info, uint offset)
{
	sec_mem_elem_t *sec_mem_elem;
	struct page *pa_cma_page;
	void *pa_cma_kmap_va = NULL;
	uint buflen = 0;
	dma_addr_t pa_cma;
	void *va;
	uint loffset = 0;
	int read_count = 0;

	sec_mem_elem = osl_sec_dma_find_rem_elem(osh, ptr_cma_info, dma_handle);
	if (sec_mem_elem == NULL) {
		printf("%s sec_mem_elem is NULL and dma_handle =0x%lx\n", __FUNCTION__,
			(ulong)dma_handle);
		return;
	}

	va = sec_mem_elem->va;
	va -= offset;
	pa_cma = sec_mem_elem->pa_cma;

	pa_cma_page = PHYS_TO_PAGE(pa_cma);

	loffset = sec_mem_elem->pa_cma -(sec_mem_elem->pa_cma & ~(PAGE_SIZE-1));

	if (direction == DMA_RX) {

		if (p == NULL) {

			/* pa_cma_kmap_va = kmap_atomic(pa_cma_page);
			* pa_cma_kmap_va += loffset;
			*/

			pa_cma_kmap_va = sec_mem_elem->vac;

			for (read_count = 200; read_count; read_count--) {

				dma_map_page(osh->dev, pa_cma_page, loffset,
				sizeof(int), DMA_FROM_DEVICE);

				buflen = *(uint *)(pa_cma_kmap_va);
				if (buflen)
					break;

				OSL_DELAY(1);
			}
			dma_unmap_page(osh->dev, pa_cma, size+offset, DMA_FROM_DEVICE);
			memcpy(va, pa_cma_kmap_va, size+offset);
			/* prhex("rxpkt",pa_cma_kmap_va, 64); */
			/* kunmap_atomic(pa_cma_kmap_va); */
		}
#ifdef NOT_YET
		else {
			buflen = 0;
			for (skb = (struct sk_buff *)p; (buflen < size) &&
				(skb != NULL); skb = skb->next) {
				if (skb_is_nonlinear(skb)) {
					pa_cma_kmap_va = kmap_atomic(pa_cma_page);
					for (i = 0; (buflen < size) &&
						(i < skb_shinfo(skb)->nr_frags); i++) {
						skb_frag_t *f = &skb_shinfo(skb)->frags[i];
						cpuaddr = kmap_atomic(skb_frag_page(f));
						memcpy((cpuaddr + f->page_offset),
							(pa_cma_kmap_va+buflen), skb_frag_size(f));
						kunmap_atomic(cpuaddr);
						buflen += skb_frag_size(f);
					}
						kunmap_atomic(pa_cma_kmap_va);
				}
				else {
					pa_cma_kmap_va = kmap_atomic(pa_cma_page);
					memcpy(skb->data, (pa_cma_kmap_va + buflen), skb->len);
					kunmap_atomic(pa_cma_kmap_va);
					buflen += skb->len;
				}

			}

		}
#endif /* NOT YET */
	} else {
		dma_unmap_page(osh->dev, pa_cma, size+offset, DMA_TO_DEVICE);
	}

	osl_sec_dma_free_mem_elem(osh, sec_mem_elem);
}

void *
osl_sec_dma_alloc_consistent(osl_t *osh, uint size, uint16 align_bits, ulong *pap)
{
	/* Allocate 64k of bytes of CMA for every request, aligned with 64k boundary */
	/* Unlike system coherent memory cma is big */

	void *temp_va;
	unsigned long temp_pa;

	if ((osh->contig_base_alloc_coherent + size) > (osh->contig_base + CMA_DMA_DESC_MEMBLOCK)) {
		printf("%s No more coherent memory\n", __FUNCTION__);
		return NULL;
	}
	temp_va = osh->coherent_base_alloc_va;
	temp_pa = osh->contig_base_alloc_coherent;
	osh->coherent_base_alloc_va += SEC_DMA_ALIGN;
	osh->contig_base_alloc_coherent += SEC_DMA_ALIGN;

	*pap = (unsigned long)temp_pa;
	return temp_va;
}

void osl_sec_cma_baseaddr_memsize(osl_t *osh, dma_addr_t *cma_baseaddr, uint32 *cma_memsize)
{
	*cma_baseaddr = osh->contig_base;
	*cma_memsize = CMA_MEMBLOCK;
}

#endif /* BCM_SECURE_DMA */
