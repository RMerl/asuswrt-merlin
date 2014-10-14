/*
 * Copyright (C) 2004 IBM
 *
 * Implements the generic device dma API for powerpc.
 * the pci and vio busses
 */
#ifndef _ASM_DMA_MAPPING_H
#define _ASM_DMA_MAPPING_H
#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/cache.h>
/* need struct page definitions */
#include <linux/mm.h>
#include <asm/scatterlist.h>
#include <asm/io.h>

#define DMA_ERROR_CODE		(~(dma_addr_t)0x0)

#ifdef CONFIG_NOT_COHERENT_CACHE
/*
 * DMA-consistent mapping functions for PowerPCs that don't support
 * cache snooping.  These allocate/free a region of uncached mapped
 * memory space for use with DMA devices.  Alternatively, you could
 * allocate the space "normally" and use the cache management functions
 * to ensure it is consistent.
 */
extern void *__dma_alloc_coherent(size_t size, dma_addr_t *handle, gfp_t gfp);
extern void __dma_free_coherent(size_t size, void *vaddr);
extern void __dma_sync(void *vaddr, size_t size, int direction);
extern void __dma_sync_page(struct page *page, unsigned long offset,
				 size_t size, int direction);

#else /* ! CONFIG_NOT_COHERENT_CACHE */
/*
 * Cache coherent cores.
 */

#define __dma_alloc_coherent(gfp, size, handle)	NULL
#define __dma_free_coherent(size, addr)		((void)0)
#define __dma_sync(addr, size, rw)		((void)0)
#define __dma_sync_page(pg, off, sz, rw)	((void)0)

#endif /* ! CONFIG_NOT_COHERENT_CACHE */

#ifdef CONFIG_PPC64
/*
 * DMA operations are abstracted for G5 vs. i/pSeries, PCI vs. VIO
 */
struct dma_mapping_ops {
	void *		(*alloc_coherent)(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t flag);
	void		(*free_coherent)(struct device *dev, size_t size,
				void *vaddr, dma_addr_t dma_handle);
	dma_addr_t	(*map_single)(struct device *dev, void *ptr,
				size_t size, enum dma_data_direction direction);
	void		(*unmap_single)(struct device *dev, dma_addr_t dma_addr,
				size_t size, enum dma_data_direction direction);
	int		(*map_sg)(struct device *dev, struct scatterlist *sg,
				int nents, enum dma_data_direction direction);
	void		(*unmap_sg)(struct device *dev, struct scatterlist *sg,
				int nents, enum dma_data_direction direction);
	int		(*dma_supported)(struct device *dev, u64 mask);
	int		(*dac_dma_supported)(struct device *dev, u64 mask);
	int		(*set_dma_mask)(struct device *dev, u64 dma_mask);
};

static inline struct dma_mapping_ops *get_dma_ops(struct device *dev)
{
	/* We don't handle the NULL dev case for ISA for now. We could
	 * do it via an out of line call but it is not needed for now. The
	 * only ISA DMA device we support is the floppy and we have a hack
	 * in the floppy driver directly to get a device for us.
	 */
	if (unlikely(dev == NULL || dev->archdata.dma_ops == NULL))
		return NULL;
	return dev->archdata.dma_ops;
}

static inline int dma_supported(struct device *dev, u64 mask)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	if (unlikely(dma_ops == NULL))
		return 0;
	if (dma_ops->dma_supported == NULL)
		return 1;
	return dma_ops->dma_supported(dev, mask);
}

static inline int dma_set_mask(struct device *dev, u64 dma_mask)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	if (unlikely(dma_ops == NULL))
		return -EIO;
	if (dma_ops->set_dma_mask != NULL)
		return dma_ops->set_dma_mask(dev, dma_mask);
	if (!dev->dma_mask || !dma_supported(dev, *dev->dma_mask))
		return -EIO;
	*dev->dma_mask = dma_mask;
	return 0;
}

static inline void *dma_alloc_coherent(struct device *dev, size_t size,
				       dma_addr_t *dma_handle, gfp_t flag)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	return dma_ops->alloc_coherent(dev, size, dma_handle, flag);
}

static inline void dma_free_coherent(struct device *dev, size_t size,
				     void *cpu_addr, dma_addr_t dma_handle)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	dma_ops->free_coherent(dev, size, cpu_addr, dma_handle);
}

static inline dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
					size_t size,
					enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	return dma_ops->map_single(dev, cpu_addr, size, direction);
}

static inline void dma_unmap_single(struct device *dev, dma_addr_t dma_addr,
				    size_t size,
				    enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	dma_ops->unmap_single(dev, dma_addr, size, direction);
}

static inline dma_addr_t dma_map_page(struct device *dev, struct page *page,
				      unsigned long offset, size_t size,
				      enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	return dma_ops->map_single(dev, page_address(page) + offset, size,
			direction);
}

static inline void dma_unmap_page(struct device *dev, dma_addr_t dma_address,
				  size_t size,
				  enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	dma_ops->unmap_single(dev, dma_address, size, direction);
}

static inline int dma_map_sg(struct device *dev, struct scatterlist *sg,
			     int nents, enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	return dma_ops->map_sg(dev, sg, nents, direction);
}

static inline void dma_unmap_sg(struct device *dev, struct scatterlist *sg,
				int nhwentries,
				enum dma_data_direction direction)
{
	struct dma_mapping_ops *dma_ops = get_dma_ops(dev);

	BUG_ON(!dma_ops);
	dma_ops->unmap_sg(dev, sg, nhwentries, direction);
}


/*
 * Available generic sets of operations
 */
extern struct dma_mapping_ops dma_iommu_ops;
extern struct dma_mapping_ops dma_direct_ops;

extern unsigned long dma_direct_offset;

#else /* CONFIG_PPC64 */

#define dma_supported(dev, mask)	(1)

static inline int dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

static inline void *dma_alloc_coherent(struct device *dev, size_t size,
				       dma_addr_t * dma_handle,
				       gfp_t gfp)
{
#ifdef CONFIG_NOT_COHERENT_CACHE
	return __dma_alloc_coherent(size, dma_handle, gfp);
#else
	void *ret;
	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

	if (dev == NULL || dev->coherent_dma_mask < 0xffffffff)
		gfp |= GFP_DMA;

	ret = (void *)__get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
		memset(ret, 0, size);
		*dma_handle = virt_to_bus(ret);
	}

	return ret;
#endif
}

static inline void
dma_free_coherent(struct device *dev, size_t size, void *vaddr,
		  dma_addr_t dma_handle)
{
#ifdef CONFIG_NOT_COHERENT_CACHE
	__dma_free_coherent(size, vaddr);
#else
	free_pages((unsigned long)vaddr, get_order(size));
#endif
}

static inline dma_addr_t
dma_map_single(struct device *dev, void *ptr, size_t size,
	       enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	__dma_sync(ptr, size, direction);

	return virt_to_bus(ptr);
}

/* We do nothing. */
#define dma_unmap_single(dev, addr, size, dir)	((void)0)

static inline dma_addr_t
dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size,
	     enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	__dma_sync_page(page, offset, size, direction);

	return page_to_bus(page) + offset;
}

/* We do nothing. */
#define dma_unmap_page(dev, handle, size, dir)	((void)0)

static inline int
dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	   enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	for (i = 0; i < nents; i++, sg++) {
		BUG_ON(!sg->page);
		__dma_sync_page(sg->page, sg->offset, sg->length, direction);
		sg->dma_address = page_to_bus(sg->page) + sg->offset;
	}

	return nents;
}

/* We don't do anything here. */
#define dma_unmap_sg(dev, sg, nents, dir)	((void)0)

#endif /* CONFIG_PPC64 */

static inline void dma_sync_single_for_cpu(struct device *dev,
		dma_addr_t dma_handle, size_t size,
		enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);
	__dma_sync(bus_to_virt(dma_handle), size, direction);
}

static inline void dma_sync_single_for_device(struct device *dev,
		dma_addr_t dma_handle, size_t size,
		enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);
	__dma_sync(bus_to_virt(dma_handle), size, direction);
}

static inline void dma_sync_sg_for_cpu(struct device *dev,
		struct scatterlist *sg, int nents,
		enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	for (i = 0; i < nents; i++, sg++)
		__dma_sync_page(sg->page, sg->offset, sg->length, direction);
}

static inline void dma_sync_sg_for_device(struct device *dev,
		struct scatterlist *sg, int nents,
		enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	for (i = 0; i < nents; i++, sg++)
		__dma_sync_page(sg->page, sg->offset, sg->length, direction);
}

static inline int dma_mapping_error(dma_addr_t dma_addr)
{
#ifdef CONFIG_PPC64
	return (dma_addr == DMA_ERROR_CODE);
#else
	return 0;
#endif
}

#define dma_alloc_noncoherent(d, s, h, f) dma_alloc_coherent(d, s, h, f)
#define dma_free_noncoherent(d, s, v, h) dma_free_coherent(d, s, v, h)
#ifdef CONFIG_NOT_COHERENT_CACHE
#define dma_is_consistent(d, h)	(0)
#else
#define dma_is_consistent(d, h)	(1)
#endif

static inline int dma_get_cache_alignment(void)
{
#ifdef CONFIG_PPC64
	/* no easy way to get cache size on all processors, so return
	 * the maximum possible, to be safe */
	return (1 << INTERNODE_CACHE_SHIFT);
#else
	/*
	 * Each processor family will define its own L1_CACHE_SHIFT,
	 * L1_CACHE_BYTES wraps to this, so this is always safe.
	 */
	return L1_CACHE_BYTES;
#endif
}

static inline void dma_sync_single_range_for_cpu(struct device *dev,
		dma_addr_t dma_handle, unsigned long offset, size_t size,
		enum dma_data_direction direction)
{
	/* just sync everything for now */
	dma_sync_single_for_cpu(dev, dma_handle, offset + size, direction);
}

static inline void dma_sync_single_range_for_device(struct device *dev,
		dma_addr_t dma_handle, unsigned long offset, size_t size,
		enum dma_data_direction direction)
{
	/* just sync everything for now */
	dma_sync_single_for_device(dev, dma_handle, offset + size, direction);
}

static inline void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
		enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);
	__dma_sync(vaddr, size, (int)direction);
}

#endif /* __KERNEL__ */
#endif	/* _ASM_DMA_MAPPING_H */
