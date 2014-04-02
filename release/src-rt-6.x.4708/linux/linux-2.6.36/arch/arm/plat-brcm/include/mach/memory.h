/*
 * Platform memory layout definitions
 *
 * Note: due to dependencies in common architecture code
 * some mappings go in other header files.
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * Main memory base address and size
 * are defined from the board-level configuration file
 */

#ifndef PHYS_OFFSET
#define PHYS_OFFSET             UL(CONFIG_DRAM_BASE)
#endif



/*
 * DMA memory
 *
 * The PCIe-to-AXI mapping (PAX) has a window of 128 MB alighed at 1MB
 * we should make the DMA-able DRAM at least this large.
 * Will need to use CONSISTENT_BASE and CONSISTENT_SIZE macros
 * to program the PAX inbound mapping registers.
 */
#define CONSISTENT_DMA_SIZE     SZ_128M

/* 2nd physical memory window */
#define PHYS_OFFSET2		0x98000000

#if !defined(__ASSEMBLY__) && defined(CONFIG_ZONE_DMA)
extern void bcm47xx_adjust_zones(unsigned long *size, unsigned long *hole);
#define arch_adjust_zones(size, hole) \
	bcm47xx_adjust_zones(size, hole)

#define ISA_DMA_THRESHOLD	(PHYS_OFFSET + SZ_128M - 1)
#define MAX_DMA_ADDRESS		(PAGE_OFFSET + SZ_128M)
#endif

#ifdef CONFIG_SPARSEMEM

#define MAX_PHYSMEM_BITS	32
#define SECTION_SIZE_BITS	27

/* bank page offsets */
#define PAGE_OFFSET1	(PAGE_OFFSET + SZ_128M)

#define __phys_to_virt(phys)						\
	((phys) >= PHYS_OFFSET2 ? (phys) - PHYS_OFFSET2 + PAGE_OFFSET1 :	\
	 (phys) - PHYS_OFFSET + PAGE_OFFSET)

#define __virt_to_phys(virt)						\
	 ((virt) >= PAGE_OFFSET1 ? (virt) - PAGE_OFFSET1 + PHYS_OFFSET2 :	\
	  (virt) - PAGE_OFFSET + PHYS_OFFSET)

#else
#define __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)
#endif


#endif
