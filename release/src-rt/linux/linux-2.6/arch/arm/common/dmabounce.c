/*
 *  arch/arm/common/dmabounce.c
 *
 *  Special dma_{map/unmap/dma_sync}_* routines for systems that have
 *  limited DMA windows. These functions utilize bounce buffers to
 *  copy data to/from buffers located outside the DMA region. This
 *  only works for systems in which DMA memory is at the bottom of
 *  RAM, the remainder of memory is at the top and the DMA memory
 *  can be marked as ZONE_DMA. Anything beyond that such as discontiguous
 *  DMA windows will require custom implementations that reserve memory
 *  areas at early bootup.
 *
 *  Original version by Brad Parker (brad@heeltoe.com)
 *  Re-written by Christopher Hoover <ch@murgatroid.com>
 *  Made generic by Deepak Saxena <dsaxena@plexity.net>
 *
 *  Copyright (C) 2002 Hewlett Packard Company.
 *  Copyright (C) 2004 MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/page-flags.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/scatterlist.h>

#include <asm/cacheflush.h>

#undef STATS

#ifdef STATS
#define DO_STATS(X) do { X ; } while (0)
#else
#define DO_STATS(X) do { } while (0)
#endif

/* ************************************************** */

struct safe_buffer {
	struct list_head node;

	/* original request */
	void		*ptr;
	size_t		size;
	int		direction;

	/* safe buffer info */
	struct dmabounce_pool *pool;
	void		*safe;
	dma_addr_t	safe_dma_addr;
};

struct dmabounce_pool {
	unsigned long	size;
	struct dma_pool	*pool;
#ifdef STATS
	unsigned long	allocs;
#endif
};

struct dmabounce_device_info {
	struct device *dev;
	struct list_head safe_buffers;
#ifdef STATS
	unsigned long total_allocs;
	unsigned long map_op_count;
	unsigned long bounce_count;
	int attr_res;
#endif
	struct dmabounce_pool	small;
	struct dmabounce_pool	large;

	rwlock_t lock;
};

#ifdef STATS
static ssize_t dmabounce_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct dmabounce_device_info *device_info = dev->archdata.dmabounce;
	return sprintf(buf, "%lu %lu %lu %lu %lu %lu\n",
		device_info->small.allocs,
		device_info->large.allocs,
		device_info->total_allocs - device_info->small.allocs -
			device_info->large.allocs,
		device_info->total_allocs,
		device_info->map_op_count,
		device_info->bounce_count);
}

static DEVICE_ATTR(dmabounce_stats, 0400, dmabounce_show, NULL);
#endif


/* allocate a 'safe' buffer and keep track of it */
static inline struct safe_buffer *
alloc_safe_buffer(struct dmabounce_device_info *device_info, void *ptr,
		  size_t size, enum dma_data_direction dir)
{
	struct safe_buffer *buf;
	struct dmabounce_pool *pool;
	struct device *dev = device_info->dev;
	unsigned long flags;

	dev_dbg(dev, "%s(ptr=%p, size=%d, dir=%d)\n",
		__func__, ptr, size, dir);

	if (size <= device_info->small.size) {
		pool = &device_info->small;
	} else if (size <= device_info->large.size) {
		pool = &device_info->large;
	} else {
		pool = NULL;
	}

	buf = kmalloc(sizeof(struct safe_buffer), GFP_ATOMIC);
	if (buf == NULL) {
		dev_warn(dev, "%s: kmalloc failed\n", __func__);
		return NULL;
	}

	buf->ptr = ptr;
	buf->size = size;
	buf->direction = dir;
	buf->pool = pool;

	if (pool) {
		buf->safe = dma_pool_alloc(pool->pool, GFP_ATOMIC,
					   &buf->safe_dma_addr);
	} else {
		buf->safe = dma_alloc_coherent(dev, size, &buf->safe_dma_addr,
					       GFP_ATOMIC);
	}

	if (buf->safe == NULL) {
		dev_warn(dev,
			 "%s: could not alloc dma memory (size=%d)\n",
			 __func__, size);
		kfree(buf);
		return NULL;
	}

#ifdef STATS
	if (pool)
		pool->allocs++;
	device_info->total_allocs++;
#endif

	write_lock_irqsave(&device_info->lock, flags);
	list_add(&buf->node, &device_info->safe_buffers);
	write_unlock_irqrestore(&device_info->lock, flags);

	return buf;
}

/* determine if a buffer is from our "safe" pool */
static inline struct safe_buffer *
find_safe_buffer(struct dmabounce_device_info *device_info, dma_addr_t safe_dma_addr)
{
	struct safe_buffer *b, *rb = NULL;
	unsigned long flags;

	read_lock_irqsave(&device_info->lock, flags);

	list_for_each_entry(b, &device_info->safe_buffers, node)
		if (b->safe_dma_addr == safe_dma_addr) {
			rb = b;
			break;
		}

	read_unlock_irqrestore(&device_info->lock, flags);
	return rb;
}

static inline void
free_safe_buffer(struct dmabounce_device_info *device_info, struct safe_buffer *buf)
{
	unsigned long flags;

	dev_dbg(device_info->dev, "%s(buf=%p)\n", __func__, buf);

	write_lock_irqsave(&device_info->lock, flags);

	list_del(&buf->node);

	write_unlock_irqrestore(&device_info->lock, flags);

	if (buf->pool)
		dma_pool_free(buf->pool->pool, buf->safe, buf->safe_dma_addr);
	else
		dma_free_coherent(device_info->dev, buf->size, buf->safe,
				    buf->safe_dma_addr);

	kfree(buf);
}

/* ************************************************** */

static struct safe_buffer *find_safe_buffer_dev(struct device *dev,
		dma_addr_t dma_addr, const char *where)
{
	if (!dev || !dev->archdata.dmabounce)
		return NULL;
	if (dma_mapping_error(dev, dma_addr)) {
		if (dev)
			dev_err(dev, "Trying to %s invalid mapping\n", where);
		else
			pr_err("unknown device: Trying to %s invalid mapping\n", where);
		return NULL;
	}
	return find_safe_buffer(dev->archdata.dmabounce, dma_addr);
}

static inline dma_addr_t map_single(struct device *dev, void *ptr, size_t size,
		enum dma_data_direction dir)
{
	struct dmabounce_device_info *device_info = dev->archdata.dmabounce;
	dma_addr_t dma_addr;
	int needs_bounce = 0;

	if (device_info)
		DO_STATS ( device_info->map_op_count++ );

	dma_addr = virt_to_dma(dev, ptr);

	if (dev->dma_mask) {
		unsigned long mask = *dev->dma_mask;
		unsigned long limit;

		limit = (mask + 1) & ~mask;
		if (limit && size > limit) {
			dev_err(dev, "DMA mapping too big (requested %#x "
				"mask %#Lx)\n", size, *dev->dma_mask);
			return ~0;
		}

		/*
		 * Figure out if we need to bounce from the DMA mask.
		 */
		needs_bounce = (dma_addr | (dma_addr + size - 1)) & ~mask;
	}

	if (device_info && (needs_bounce || dma_needs_bounce(dev, dma_addr, size))) {
		struct safe_buffer *buf;

		buf = alloc_safe_buffer(device_info, ptr, size, dir);
		if (buf == 0) {
			dev_err(dev, "%s: unable to map unsafe buffer %p!\n",
			       __func__, ptr);
			return 0;
		}

		dev_dbg(dev,
			"%s: unsafe buffer %p (dma=%#x) mapped to %p (dma=%#x)\n",
			__func__, buf->ptr, virt_to_dma(dev, buf->ptr),
			buf->safe, buf->safe_dma_addr);

		if ((dir == DMA_TO_DEVICE) ||
		    (dir == DMA_BIDIRECTIONAL)) {
			dev_dbg(dev, "%s: copy unsafe %p to safe %p, size %d\n",
				__func__, ptr, buf->safe, size);
			memcpy(buf->safe, ptr, size);
		}
		ptr = buf->safe;

		dma_addr = buf->safe_dma_addr;
	} else {
		/*
		 * We don't need to sync the DMA buffer since
		 * it was allocated via the coherent allocators.
		 */
		__dma_single_cpu_to_dev(ptr, size, dir);
	}

	return dma_addr;
}

static inline void unmap_single(struct device *dev, dma_addr_t dma_addr,
		size_t size, enum dma_data_direction dir)
{
	struct safe_buffer *buf = find_safe_buffer_dev(dev, dma_addr, "unmap");

	if (buf) {
		BUG_ON(buf->size != size);
		BUG_ON(buf->direction != dir);

		dev_dbg(dev,
			"%s: unsafe buffer %p (dma=%#x) mapped to %p (dma=%#x)\n",
			__func__, buf->ptr, virt_to_dma(dev, buf->ptr),
			buf->safe, buf->safe_dma_addr);

		DO_STATS(dev->archdata.dmabounce->bounce_count++);

		if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
			void *ptr = buf->ptr;

			dev_dbg(dev,
				"%s: copy back safe %p to unsafe %p size %d\n",
				__func__, buf->safe, ptr, size);
			memcpy(ptr, buf->safe, size);

			/*
			 * Since we may have written to a page cache page,
			 * we need to ensure that the data will be coherent
			 * with user mappings.
			 */
			__cpuc_flush_dcache_area(ptr, size);
		}
		free_safe_buffer(dev->archdata.dmabounce, buf);
	} else {
		__dma_single_dev_to_cpu(dma_to_virt(dev, dma_addr), size, dir);
	}
}

/* ************************************************** */

/*
 * see if a buffer address is in an 'unsafe' range.  if it is
 * allocate a 'safe' buffer and copy the unsafe buffer into it.
 * substitute the safe buffer for the unsafe one.
 * (basically move the buffer from an unsafe area to a safe one)
 */
dma_addr_t __dma_map_single(struct device *dev, void *ptr, size_t size,
		enum dma_data_direction dir)
{
	dev_dbg(dev, "%s(ptr=%p,size=%d,dir=%x)\n",
		__func__, ptr, size, dir);

	BUG_ON(!valid_dma_direction(dir));

	return map_single(dev, ptr, size, dir);
}
EXPORT_SYMBOL(__dma_map_single);

/*
 * see if a mapped address was really a "safe" buffer and if so, copy
 * the data from the safe buffer back to the unsafe buffer and free up
 * the safe buffer.  (basically return things back to the way they
 * should be)
 */
void __dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size,
		enum dma_data_direction dir)
{
	dev_dbg(dev, "%s(ptr=%p,size=%d,dir=%x)\n",
		__func__, (void *) dma_addr, size, dir);

	unmap_single(dev, dma_addr, size, dir);
}
EXPORT_SYMBOL(__dma_unmap_single);

dma_addr_t __dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir)
{
	dev_dbg(dev, "%s(page=%p,off=%#lx,size=%zx,dir=%x)\n",
		__func__, page, offset, size, dir);

	BUG_ON(!valid_dma_direction(dir));

	if (PageHighMem(page)) {
		dev_err(dev, "DMA buffer bouncing of HIGHMEM pages "
			     "is not supported\n");
		return ~0;
	}

	return map_single(dev, page_address(page) + offset, size, dir);
}
EXPORT_SYMBOL(__dma_map_page);

/*
 * see if a mapped address was really a "safe" buffer and if so, copy
 * the data from the safe buffer back to the unsafe buffer and free up
 * the safe buffer.  (basically return things back to the way they
 * should be)
 */
void __dma_unmap_page(struct device *dev, dma_addr_t dma_addr, size_t size,
		enum dma_data_direction dir)
{
	dev_dbg(dev, "%s(ptr=%p,size=%d,dir=%x)\n",
		__func__, (void *) dma_addr, size, dir);

	unmap_single(dev, dma_addr, size, dir);
}
EXPORT_SYMBOL(__dma_unmap_page);

int dmabounce_sync_for_cpu(struct device *dev, dma_addr_t addr,
		unsigned long off, size_t sz, enum dma_data_direction dir)
{
	struct safe_buffer *buf;

	dev_dbg(dev, "%s(dma=%#x,off=%#lx,sz=%zx,dir=%x)\n",
		__func__, addr, off, sz, dir);

	buf = find_safe_buffer_dev(dev, addr, __func__);
	if (!buf)
		return 1;

	BUG_ON(buf->direction != dir);

	dev_dbg(dev, "%s: unsafe buffer %p (dma=%#x) mapped to %p (dma=%#x)\n",
		__func__, buf->ptr, virt_to_dma(dev, buf->ptr),
		buf->safe, buf->safe_dma_addr);

	DO_STATS(dev->archdata.dmabounce->bounce_count++);

	if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
		dev_dbg(dev, "%s: copy back safe %p to unsafe %p size %d\n",
			__func__, buf->safe + off, buf->ptr + off, sz);
		memcpy(buf->ptr + off, buf->safe + off, sz);
	}
	return 0;
}
EXPORT_SYMBOL(dmabounce_sync_for_cpu);

int dmabounce_sync_for_device(struct device *dev, dma_addr_t addr,
		unsigned long off, size_t sz, enum dma_data_direction dir)
{
	struct safe_buffer *buf;

	dev_dbg(dev, "%s(dma=%#x,off=%#lx,sz=%zx,dir=%x)\n",
		__func__, addr, off, sz, dir);

	buf = find_safe_buffer_dev(dev, addr, __func__);
	if (!buf)
		return 1;

	BUG_ON(buf->direction != dir);

	dev_dbg(dev, "%s: unsafe buffer %p (dma=%#x) mapped to %p (dma=%#x)\n",
		__func__, buf->ptr, virt_to_dma(dev, buf->ptr),
		buf->safe, buf->safe_dma_addr);

	DO_STATS(dev->archdata.dmabounce->bounce_count++);

	if (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL) {
		dev_dbg(dev, "%s: copy out unsafe %p to safe %p, size %d\n",
			__func__,buf->ptr + off, buf->safe + off, sz);
		memcpy(buf->safe + off, buf->ptr + off, sz);
	}
	return 0;
}
EXPORT_SYMBOL(dmabounce_sync_for_device);

static int dmabounce_init_pool(struct dmabounce_pool *pool, struct device *dev,
		const char *name, unsigned long size)
{
	pool->size = size;
	DO_STATS(pool->allocs = 0);
	pool->pool = dma_pool_create(name, dev, size,
				     0 /* byte alignment */,
				     0 /* no page-crossing issues */);

	return pool->pool ? 0 : -ENOMEM;
}

int dmabounce_register_dev(struct device *dev, unsigned long small_buffer_size,
		unsigned long large_buffer_size)
{
	struct dmabounce_device_info *device_info;
	int ret;

	device_info = kmalloc(sizeof(struct dmabounce_device_info), GFP_ATOMIC);
	if (!device_info) {
		dev_err(dev,
			"Could not allocated dmabounce_device_info\n");
		return -ENOMEM;
	}

	ret = dmabounce_init_pool(&device_info->small, dev,
				  "small_dmabounce_pool", small_buffer_size);
	if (ret) {
		dev_err(dev,
			"dmabounce: could not allocate DMA pool for %ld byte objects\n",
			small_buffer_size);
		goto err_free;
	}

	if (large_buffer_size) {
		ret = dmabounce_init_pool(&device_info->large, dev,
					  "large_dmabounce_pool",
					  large_buffer_size);
		if (ret) {
			dev_err(dev,
				"dmabounce: could not allocate DMA pool for %ld byte objects\n",
				large_buffer_size);
			goto err_destroy;
		}
	}

	device_info->dev = dev;
	INIT_LIST_HEAD(&device_info->safe_buffers);
	rwlock_init(&device_info->lock);

#ifdef STATS
	device_info->total_allocs = 0;
	device_info->map_op_count = 0;
	device_info->bounce_count = 0;
	device_info->attr_res = device_create_file(dev, &dev_attr_dmabounce_stats);
#endif

	dev->archdata.dmabounce = device_info;

	dev_info(dev, "dmabounce: registered device\n");

	return 0;

 err_destroy:
	dma_pool_destroy(device_info->small.pool);
 err_free:
	kfree(device_info);
	return ret;
}
EXPORT_SYMBOL(dmabounce_register_dev);

void dmabounce_unregister_dev(struct device *dev)
{
	struct dmabounce_device_info *device_info = dev->archdata.dmabounce;

	dev->archdata.dmabounce = NULL;

	if (!device_info) {
		dev_warn(dev,
			 "Never registered with dmabounce but attempting"
			 "to unregister!\n");
		return;
	}

	if (!list_empty(&device_info->safe_buffers)) {
		dev_err(dev,
			"Removing from dmabounce with pending buffers!\n");
		BUG();
	}

	if (device_info->small.pool)
		dma_pool_destroy(device_info->small.pool);
	if (device_info->large.pool)
		dma_pool_destroy(device_info->large.pool);

#ifdef STATS
	if (device_info->attr_res == 0)
		device_remove_file(dev, &dev_attr_dmabounce_stats);
#endif

	kfree(device_info);

	dev_info(dev, "dmabounce: device unregistered\n");
}
EXPORT_SYMBOL(dmabounce_unregister_dev);

MODULE_AUTHOR("Christopher Hoover <ch@hpl.hp.com>, Deepak Saxena <dsaxena@plexity.net>");
MODULE_DESCRIPTION("Special dma_{map/unmap/dma_sync}_* routines for systems with limited DMA windows");
MODULE_LICENSE("GPL");
