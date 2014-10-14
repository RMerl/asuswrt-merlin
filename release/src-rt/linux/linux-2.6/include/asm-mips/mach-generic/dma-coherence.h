/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006  Ralf Baechle <ralf@linux-mips.org>
 *
 */
#ifndef __ASM_MACH_GENERIC_DMA_COHERENCE_H
#define __ASM_MACH_GENERIC_DMA_COHERENCE_H

struct device;

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr,
	size_t size)
{
	return virt_to_phys(addr);
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev,
	struct page *page)
{
	return page_to_phys(page);
}

static inline unsigned long plat_dma_addr_to_phys(dma_addr_t dma_addr)
{
	return dma_addr;
}

static inline void plat_unmap_dma_mem(dma_addr_t dma_addr)
{
}

static inline int plat_device_is_coherent(struct device *dev)
{
#ifdef CONFIG_DMA_COHERENT
	return 1;
#endif
#ifdef CONFIG_DMA_NONCOHERENT
	return 0;
#endif
}

#endif /* __ASM_MACH_GENERIC_DMA_COHERENCE_H */
