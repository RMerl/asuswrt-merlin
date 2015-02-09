#ifndef _ASM_DMA_MAPPING_H
#define _ASM_DMA_MAPPING_H

#include <asm/scatterlist.h>
#include <asm/cache.h>
#include <asm-generic/dma-coherent.h>

void *dma_alloc_noncoherent(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t flag);

void dma_free_noncoherent(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle);

void *dma_alloc_coherent(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t flag);

void dma_free_coherent(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle);

extern dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size,
	enum dma_data_direction direction);
extern void dma_unmap_single(struct device *dev, dma_addr_t dma_addr,
	size_t size, enum dma_data_direction direction);
extern int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	enum dma_data_direction direction);
extern dma_addr_t dma_map_page(struct device *dev, struct page *page,
	unsigned long offset, size_t size, enum dma_data_direction direction);

static inline void dma_unmap_page(struct device *dev, dma_addr_t dma_address,
	size_t size, enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_address, size, direction);
}

extern void dma_unmap_sg(struct device *dev, struct scatterlist *sg,
	int nhwentries, enum dma_data_direction direction);
extern void dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
	size_t size, enum dma_data_direction direction);
extern void dma_sync_single_for_device(struct device *dev,
	dma_addr_t dma_handle, size_t size, enum dma_data_direction direction);
extern void dma_sync_single_range_for_cpu(struct device *dev,
	dma_addr_t dma_handle, unsigned long offset, size_t size,
	enum dma_data_direction direction);
extern void dma_sync_single_range_for_device(struct device *dev,
	dma_addr_t dma_handle, unsigned long offset, size_t size,
	enum dma_data_direction direction);
extern void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
	int nelems, enum dma_data_direction direction);
extern void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
	int nelems, enum dma_data_direction direction);
extern int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);
extern int dma_supported(struct device *dev, u64 mask);

static inline int
dma_set_mask(struct device *dev, u64 mask)
{
	if(!dev->dma_mask || !dma_supported(dev, mask))
		return -EIO;

	*dev->dma_mask = mask;

	return 0;
}

extern void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
	       enum dma_data_direction direction);

#endif /* _ASM_DMA_MAPPING_H */
