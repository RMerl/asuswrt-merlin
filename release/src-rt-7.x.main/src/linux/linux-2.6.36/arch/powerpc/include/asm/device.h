/*
 * Arch specific extensions to struct device
 *
 * This file is released under the GPLv2
 */
#ifndef _ASM_POWERPC_DEVICE_H
#define _ASM_POWERPC_DEVICE_H

struct dma_map_ops;
struct device_node;

struct dev_archdata {
	/* DMA operations on that device */
	struct dma_map_ops	*dma_ops;

	/*
	 * When an iommu is in use, dma_data is used as a ptr to the base of the
	 * iommu_table.  Otherwise, it is a simple numerical offset.
	 */
	union {
		dma_addr_t	dma_offset;
		void		*iommu_table_base;
	} dma_data;

#ifdef CONFIG_SWIOTLB
	dma_addr_t		max_direct_dma_addr;
#endif
};

struct pdev_archdata {
	u64 dma_mask;
};

#endif /* _ASM_POWERPC_DEVICE_H */
