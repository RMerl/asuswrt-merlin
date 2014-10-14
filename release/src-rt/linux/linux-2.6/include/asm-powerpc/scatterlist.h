#ifndef _ASM_POWERPC_SCATTERLIST_H
#define _ASM_POWERPC_SCATTERLIST_H
/*
 * Copyright (C) 2001 PPC64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifdef __KERNEL__
#include <linux/types.h>
#include <asm/dma.h>

struct scatterlist {
	struct page *page;
	unsigned int offset;
	unsigned int length;

	/* For TCE support */
	dma_addr_t dma_address;
	u32 dma_length;
};

/*
 * These macros should be used after a dma_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns, or alternatively stop on the first sg_dma_len(sg) which
 * is 0.
 */
#define sg_dma_address(sg)	((sg)->dma_address)
#ifdef __powerpc64__
#define sg_dma_len(sg)		((sg)->dma_length)
#else
#define sg_dma_len(sg)		((sg)->length)
#endif

#ifdef __powerpc64__
#define ISA_DMA_THRESHOLD	(~0UL)
#endif

#endif /* __KERNEL__ */
#endif /* _ASM_POWERPC_SCATTERLIST_H */
