/*
 * DMA Pool allocator
 *
 * Copyright 2001 David Brownell
 * Copyright 2007 Intel Corporation
 *   Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * This software may be redistributed and/or modified under the terms of
 * the GNU General Public License ("GPL") version 2 as published by the
 * Free Software Foundation.
 *
 * This allocator returns small blocks of a given size which are DMA-able by
 * the given device.  It uses the dma_alloc_coherent page allocator to get
 * new pages, then splits them up into blocks of the required size.
 * Many older drivers still have their own code to do this.
 *
 * The current design of this allocator is fairly simple.  The pool is
 * represented by the 'struct dma_pool' which keeps a doubly-linked list of
 * allocated pages.  Each page in the page_list is split into blocks of at
 * least 'size' bytes.  Free blocks are tracked in an unsorted singly-linked
 * list of free blocks within the page.  Used blocks aren't tracked, but we
 * keep a count of how many are currently allocated from each page.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wait.h>

#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_SLUB_DEBUG_ON)
#define DMAPOOL_DEBUG 1
#endif

struct dma_pool {		/* the pool */
	struct list_head page_list;
	spinlock_t lock;
	size_t size;
	struct device *dev;
	size_t allocation;
	size_t boundary;
	char name[32];
	wait_queue_head_t waitq;
	struct list_head pools;
};

struct dma_page {		/* cacheable header for 'allocation' bytes */
	struct list_head page_list;
	void *vaddr;
	dma_addr_t dma;
	unsigned int in_use;
	unsigned int offset;
};

#define	POOL_TIMEOUT_JIFFIES	((100 /* msec */ * HZ) / 1000)

static DEFINE_MUTEX(pools_lock);

static ssize_t
show_pools(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned temp;
	unsigned size;
	char *next;
	struct dma_page *page;
	struct dma_pool *pool;

	next = buf;
	size = PAGE_SIZE;

	temp = scnprintf(next, size, "poolinfo - 0.1\n");
	size -= temp;
	next += temp;

	mutex_lock(&pools_lock);
	list_for_each_entry(pool, &dev->dma_pools, pools) {
		unsigned pages = 0;
		unsigned blocks = 0;

		spin_lock_irq(&pool->lock);
		list_for_each_entry(page, &pool->page_list, page_list) {
			pages++;
			blocks += page->in_use;
		}
		spin_unlock_irq(&pool->lock);

		/* per-pool info, no real statistics yet */
		temp = scnprintf(next, size, "%-16s %4u %4Zu %4Zu %2u\n",
				 pool->name, blocks,
				 pages * (pool->allocation / pool->size),
				 pool->size, pages);
		size -= temp;
		next += temp;
	}
	mutex_unlock(&pools_lock);

	return PAGE_SIZE - size;
}

static DEVICE_ATTR(pools, S_IRUGO, show_pools, NULL);

/**
 * dma_pool_create - Creates a pool of consistent memory blocks, for dma.
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @boundary: returned blocks won't cross this power of two boundary
 * Context: !in_interrupt()
 *
 * Returns a dma allocation pool with the requested characteristics, or
 * null if one can't be created.  Given one of these pools, dma_pool_alloc()
 * may be used to allocate memory.  Such memory will all have "consistent"
 * DMA mappings, accessible by the device and its driver without using
 * cache flushing primitives.  The actual size of blocks allocated may be
 * larger than requested because of alignment.
 *
 * If @boundary is nonzero, objects returned from dma_pool_alloc() won't
 * cross that size boundary.  This is useful for devices which have
 * addressing restrictions on individual DMA transfers, such as not crossing
 * boundaries of 4KBytes.
 */
struct dma_pool *dma_pool_create(const char *name, struct device *dev,
				 size_t size, size_t align, size_t boundary)
{
	struct dma_pool *retval;
	size_t allocation;

	if (align == 0) {
		align = 1;
	} else if (align & (align - 1)) {
		return NULL;
	}

	if (size == 0) {
		return NULL;
	} else if (size < 4) {
		size = 4;
	}

	if ((size % align) != 0)
		size = ALIGN(size, align);

	allocation = max_t(size_t, size, PAGE_SIZE);

	if (!boundary) {
		boundary = allocation;
	} else if ((boundary < size) || (boundary & (boundary - 1))) {
		return NULL;
	}

	retval = kmalloc_node(sizeof(*retval), GFP_KERNEL, dev_to_node(dev));
	if (!retval)
		return retval;

	strlcpy(retval->name, name, sizeof(retval->name));

	retval->dev = dev;

	INIT_LIST_HEAD(&retval->page_list);
	spin_lock_init(&retval->lock);
	retval->size = size;
	retval->boundary = boundary;
	retval->allocation = allocation;
	init_waitqueue_head(&retval->waitq);

	if (dev) {
		int ret;

		mutex_lock(&pools_lock);
		if (list_empty(&dev->dma_pools))
			ret = device_create_file(dev, &dev_attr_pools);
		else
			ret = 0;
		/* note:  not currently insisting "name" be unique */
		if (!ret)
			list_add(&retval->pools, &dev->dma_pools);
		else {
			kfree(retval);
			retval = NULL;
		}
		mutex_unlock(&pools_lock);
	} else
		INIT_LIST_HEAD(&retval->pools);

	return retval;
}
EXPORT_SYMBOL(dma_pool_create);

static void pool_initialise_page(struct dma_pool *pool, struct dma_page *page)
{
	unsigned int offset = 0;
	unsigned int next_boundary = pool->boundary;

	do {
		unsigned int next = offset + pool->size;
		if (unlikely((next + pool->size) >= next_boundary)) {
			next = next_boundary;
			next_boundary += pool->boundary;
		}
		*(int *)(page->vaddr + offset) = next;
		offset = next;
	} while (offset < pool->allocation);
}

static struct dma_page *pool_alloc_page(struct dma_pool *pool, gfp_t mem_flags)
{
	struct dma_page *page;

	page = kmalloc(sizeof(*page), mem_flags);
	if (!page)
		return NULL;
	page->vaddr = dma_alloc_coherent(pool->dev, pool->allocation,
					 &page->dma, mem_flags);
	if (page->vaddr) {
#ifdef	DMAPOOL_DEBUG
		memset(page->vaddr, POOL_POISON_FREED, pool->allocation);
#endif
		pool_initialise_page(pool, page);
		list_add(&page->page_list, &pool->page_list);
		page->in_use = 0;
		page->offset = 0;
	} else {
		kfree(page);
		page = NULL;
	}
	return page;
}

static inline int is_page_busy(struct dma_page *page)
{
	return page->in_use != 0;
}

static void pool_free_page(struct dma_pool *pool, struct dma_page *page)
{
	dma_addr_t dma = page->dma;

#ifdef	DMAPOOL_DEBUG
	memset(page->vaddr, POOL_POISON_FREED, pool->allocation);
#endif
	dma_free_coherent(pool->dev, pool->allocation, page->vaddr, dma);
	list_del(&page->page_list);
	kfree(page);
}

/**
 * dma_pool_destroy - destroys a pool of dma memory blocks.
 * @pool: dma pool that will be destroyed
 * Context: !in_interrupt()
 *
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void dma_pool_destroy(struct dma_pool *pool)
{
	mutex_lock(&pools_lock);
	list_del(&pool->pools);
	if (pool->dev && list_empty(&pool->dev->dma_pools))
		device_remove_file(pool->dev, &dev_attr_pools);
	mutex_unlock(&pools_lock);

	while (!list_empty(&pool->page_list)) {
		struct dma_page *page;
		page = list_entry(pool->page_list.next,
				  struct dma_page, page_list);
		if (is_page_busy(page)) {
			if (pool->dev)
				dev_err(pool->dev,
					"dma_pool_destroy %s, %p busy\n",
					pool->name, page->vaddr);
			else
				printk(KERN_ERR
				       "dma_pool_destroy %s, %p busy\n",
				       pool->name, page->vaddr);
			/* leak the still-in-use consistent memory */
			list_del(&page->page_list);
			kfree(page);
		} else
			pool_free_page(pool, page);
	}

	kfree(pool);
}
EXPORT_SYMBOL(dma_pool_destroy);

/**
 * dma_pool_alloc - get a block of consistent memory
 * @pool: dma pool that will produce the block
 * @mem_flags: GFP_* bitmask
 * @handle: pointer to dma address of block
 *
 * This returns the kernel virtual address of a currently unused block,
 * and reports its dma address through the handle.
 * If such a memory block can't be allocated, %NULL is returned.
 */
void *dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
		     dma_addr_t *handle)
{
	unsigned long flags;
	struct dma_page *page;
	size_t offset;
	void *retval;

	spin_lock_irqsave(&pool->lock, flags);
 restart:
	list_for_each_entry(page, &pool->page_list, page_list) {
		if (page->offset < pool->allocation)
			goto ready;
	}
	page = pool_alloc_page(pool, GFP_ATOMIC);
	if (!page) {
		if (mem_flags & __GFP_WAIT) {
			DECLARE_WAITQUEUE(wait, current);

			__set_current_state(TASK_INTERRUPTIBLE);
			__add_wait_queue(&pool->waitq, &wait);
			spin_unlock_irqrestore(&pool->lock, flags);

			schedule_timeout(POOL_TIMEOUT_JIFFIES);

			spin_lock_irqsave(&pool->lock, flags);
			__remove_wait_queue(&pool->waitq, &wait);
			goto restart;
		}
		retval = NULL;
		goto done;
	}

 ready:
	page->in_use++;
	offset = page->offset;
	page->offset = *(int *)(page->vaddr + offset);
	retval = offset + page->vaddr;
	*handle = offset + page->dma;
#ifdef	DMAPOOL_DEBUG
	memset(retval, POOL_POISON_ALLOCATED, pool->size);
#endif
 done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return retval;
}
EXPORT_SYMBOL(dma_pool_alloc);

static struct dma_page *pool_find_page(struct dma_pool *pool, dma_addr_t dma)
{
	unsigned long flags;
	struct dma_page *page;

	spin_lock_irqsave(&pool->lock, flags);
	list_for_each_entry(page, &pool->page_list, page_list) {
		if (dma < page->dma)
			continue;
		if (dma < (page->dma + pool->allocation))
			goto done;
	}
	page = NULL;
 done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return page;
}

/**
 * dma_pool_free - put block back into dma pool
 * @pool: the dma pool holding the block
 * @vaddr: virtual address of block
 * @dma: dma address of block
 *
 * Caller promises neither device nor driver will again touch this block
 * unless it is first re-allocated.
 */
void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct dma_page *page;
	unsigned long flags;
	unsigned int offset;

	page = pool_find_page(pool, dma);
	if (!page) {
		if (pool->dev)
			dev_err(pool->dev,
				"dma_pool_free %s, %p/%lx (bad dma)\n",
				pool->name, vaddr, (unsigned long)dma);
		else
			printk(KERN_ERR "dma_pool_free %s, %p/%lx (bad dma)\n",
			       pool->name, vaddr, (unsigned long)dma);
		return;
	}

	offset = vaddr - page->vaddr;
#ifdef	DMAPOOL_DEBUG
	if ((dma - page->dma) != offset) {
		if (pool->dev)
			dev_err(pool->dev,
				"dma_pool_free %s, %p (bad vaddr)/%Lx\n",
				pool->name, vaddr, (unsigned long long)dma);
		else
			printk(KERN_ERR
			       "dma_pool_free %s, %p (bad vaddr)/%Lx\n",
			       pool->name, vaddr, (unsigned long long)dma);
		return;
	}
	{
		unsigned int chain = page->offset;
		while (chain < pool->allocation) {
			if (chain != offset) {
				chain = *(int *)(page->vaddr + chain);
				continue;
			}
			if (pool->dev)
				dev_err(pool->dev, "dma_pool_free %s, dma %Lx "
					"already free\n", pool->name,
					(unsigned long long)dma);
			else
				printk(KERN_ERR "dma_pool_free %s, dma %Lx "
					"already free\n", pool->name,
					(unsigned long long)dma);
			return;
		}
	}
	memset(vaddr, POOL_POISON_FREED, pool->size);
#endif

	spin_lock_irqsave(&pool->lock, flags);
	page->in_use--;
	*(int *)vaddr = page->offset;
	page->offset = offset;
	if (waitqueue_active(&pool->waitq))
		wake_up_locked(&pool->waitq);
	/*
	 * Resist a temptation to do
	 *    if (!is_page_busy(page)) pool_free_page(pool, page);
	 * Better have a few empty pages hang around.
	 */
	spin_unlock_irqrestore(&pool->lock, flags);
}
EXPORT_SYMBOL(dma_pool_free);

/*
 * Managed DMA pool
 */
static void dmam_pool_release(struct device *dev, void *res)
{
	struct dma_pool *pool = *(struct dma_pool **)res;

	dma_pool_destroy(pool);
}

static int dmam_pool_match(struct device *dev, void *res, void *match_data)
{
	return *(struct dma_pool **)res == match_data;
}

/**
 * dmam_pool_create - Managed dma_pool_create()
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @allocation: returned blocks won't cross this boundary (or zero)
 *
 * Managed dma_pool_create().  DMA pool created with this function is
 * automatically destroyed on driver detach.
 */
struct dma_pool *dmam_pool_create(const char *name, struct device *dev,
				  size_t size, size_t align, size_t allocation)
{
	struct dma_pool **ptr, *pool;

	ptr = devres_alloc(dmam_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	pool = *ptr = dma_pool_create(name, dev, size, align, allocation);
	if (pool)
		devres_add(dev, ptr);
	else
		devres_free(ptr);

	return pool;
}
EXPORT_SYMBOL(dmam_pool_create);

/**
 * dmam_pool_destroy - Managed dma_pool_destroy()
 * @pool: dma pool that will be destroyed
 *
 * Managed dma_pool_destroy().
 */
void dmam_pool_destroy(struct dma_pool *pool)
{
	struct device *dev = pool->dev;

	dma_pool_destroy(pool);
	WARN_ON(devres_destroy(dev, dmam_pool_release, dmam_pool_match, pool));
}
EXPORT_SYMBOL(dmam_pool_destroy);
