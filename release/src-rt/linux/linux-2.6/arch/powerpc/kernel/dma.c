/*
 * Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corporation
 *
 * Provide default implementations of the DMA mapping callbacks for
 * directly mapped busses.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-debug.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <asm/bug.h>
#include <asm/abs_addr.h>
#include <asm/machdep.h>

/*
 * Generic direct DMA implementation
 *
 * This implementation supports a per-device offset that can be applied if
 * the address at which memory is visible to devices is not 0. Platform code
 * can set archdata.dma_data to an unsigned long holding the offset. By
 * default the offset is PCI_DRAM_OFFSET.
 */


void *dma_direct_alloc_coherent(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t flag)
{
	void *ret;
#ifdef CONFIG_NOT_COHERENT_CACHE
	ret = __dma_alloc_coherent(dev, size, dma_handle, flag);
	if (ret == NULL)
		return NULL;
	*dma_handle += get_dma_offset(dev);
	return ret;
#else
	struct page *page;
	int node = dev_to_node(dev);

	/* ignore region specifiers */
	flag  &= ~(__GFP_HIGHMEM);

	page = alloc_pages_node(node, flag, get_order(size));
	if (page == NULL)
		return NULL;
	ret = page_address(page);
	memset(ret, 0, size);
	*dma_handle = virt_to_abs(ret) + get_dma_offset(dev);

	return ret;
#endif
}

void dma_direct_free_coherent(struct device *dev, size_t size,
			      void *vaddr, dma_addr_t dma_handle)
{
#ifdef CONFIG_NOT_COHERENT_CACHE
	__dma_free_coherent(size, vaddr);
#else
	free_pages((unsigned long)vaddr, get_order(size));
#endif
}

static int dma_direct_map_sg(struct device *dev, struct scatterlist *sgl,
			     int nents, enum dma_data_direction direction,
			     struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = sg_phys(sg) + get_dma_offset(dev);
		sg->dma_length = sg->length;
		__dma_sync_page(sg_page(sg), sg->offset, sg->length, direction);
	}

	return nents;
}

static void dma_direct_unmap_sg(struct device *dev, struct scatterlist *sg,
				int nents, enum dma_data_direction direction,
				struct dma_attrs *attrs)
{
}

static int dma_direct_dma_supported(struct device *dev, u64 mask)
{
#ifdef CONFIG_PPC64
	/* Could be improved so platforms can set the limit in case
	 * they have limited DMA windows
	 */
	return mask >= get_dma_offset(dev) + (memblock_end_of_DRAM() - 1);
#else
	return 1;
#endif
}

static inline dma_addr_t dma_direct_map_page(struct device *dev,
					     struct page *page,
					     unsigned long offset,
					     size_t size,
					     enum dma_data_direction dir,
					     struct dma_attrs *attrs)
{
	BUG_ON(dir == DMA_NONE);
	__dma_sync_page(page, offset, size, dir);
	return page_to_phys(page) + offset + get_dma_offset(dev);
}

static inline void dma_direct_unmap_page(struct device *dev,
					 dma_addr_t dma_address,
					 size_t size,
					 enum dma_data_direction direction,
					 struct dma_attrs *attrs)
{
}

#ifdef CONFIG_NOT_COHERENT_CACHE
static inline void dma_direct_sync_sg(struct device *dev,
		struct scatterlist *sgl, int nents,
		enum dma_data_direction direction)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i)
		__dma_sync_page(sg_page(sg), sg->offset, sg->length, direction);
}

static inline void dma_direct_sync_single(struct device *dev,
					  dma_addr_t dma_handle, size_t size,
					  enum dma_data_direction direction)
{
	__dma_sync(bus_to_virt(dma_handle), size, direction);
}
#endif

struct dma_map_ops dma_direct_ops = {
	.alloc_coherent	= dma_direct_alloc_coherent,
	.free_coherent	= dma_direct_free_coherent,
	.map_sg		= dma_direct_map_sg,
	.unmap_sg	= dma_direct_unmap_sg,
	.dma_supported	= dma_direct_dma_supported,
	.map_page	= dma_direct_map_page,
	.unmap_page	= dma_direct_unmap_page,
#ifdef CONFIG_NOT_COHERENT_CACHE
	.sync_single_for_cpu 		= dma_direct_sync_single,
	.sync_single_for_device 	= dma_direct_sync_single,
	.sync_sg_for_cpu 		= dma_direct_sync_sg,
	.sync_sg_for_device 		= dma_direct_sync_sg,
#endif
};
EXPORT_SYMBOL(dma_direct_ops);

#define PREALLOC_DMA_DEBUG_ENTRIES (1 << 16)

int dma_set_mask(struct device *dev, u64 dma_mask)
{
	struct dma_map_ops *dma_ops = get_dma_ops(dev);

	if (ppc_md.dma_set_mask)
		return ppc_md.dma_set_mask(dev, dma_mask);
	if (unlikely(dma_ops == NULL))
		return -EIO;
	if (dma_ops->set_dma_mask != NULL)
		return dma_ops->set_dma_mask(dev, dma_mask);
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;
	*dev->dma_mask = dma_mask;
	return 0;
}
EXPORT_SYMBOL(dma_set_mask);

static int __init dma_init(void)
{
       dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);

       return 0;
}
fs_initcall(dma_init);

int dma_mmap_coherent(struct device *dev, struct vm_area_struct *vma,
		      void *cpu_addr, dma_addr_t handle, size_t size)
{
	unsigned long pfn;

#ifdef CONFIG_NOT_COHERENT_CACHE
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = __dma_get_coherent_pfn((unsigned long)cpu_addr);
#else
	pfn = page_to_pfn(virt_to_page(cpu_addr));
#endif
	return remap_pfn_range(vma, vma->vm_start,
			       pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}
EXPORT_SYMBOL_GPL(dma_mmap_coherent);
