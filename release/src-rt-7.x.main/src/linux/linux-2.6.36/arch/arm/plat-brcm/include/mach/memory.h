/*
 * Platform memory layout definitions
 *
 * Note: due to dependencies in common architecture code
 * some mappings go in other header files.
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PADDR_ACP_AX		(0x80000000)
#define PADDR_ACP_BX		(0x40000000)

/*
 * Main memory base address and size
 * are defined from the board-level configuration file
 */

#ifndef PHYS_OFFSET
#if !defined(__ASSEMBLY__)
extern unsigned int ddr_phys_offset_va;
#define PHYS_OFFSET	((unsigned long)ddr_phys_offset_va)
#define ACP_WAR_ENAB()	(PHYS_OFFSET == PADDR_ACP_AX ? 1 : 0)
#define DDR_PADDR_ACP	(PHYS_OFFSET == PADDR_ACP_AX ? PADDR_ACP_AX : PADDR_ACP_BX)
extern unsigned int ns_acp_win_size;
#define ACP_WIN_SIZE			(ns_acp_win_size)
#define ACP_WIN_LIMIT			(PHYS_OFFSET + ACP_WIN_SIZE)
#else
#define PHYS_OFFSET             UL(CONFIG_DRAM_BASE)
#endif	/* !__ASSEMBLY__ */
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
#define PHYS_OFFSET2		0xa8000000

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

#define __phys_to_virt(phys)								\
	(((PHYS_OFFSET) == DDR_PADDR_ACP) ? ((phys) - DDR_PADDR_ACP + PAGE_OFFSET) :	\
	(((phys) >= PHYS_OFFSET2) ? ((phys) - PHYS_OFFSET2 + PAGE_OFFSET1) :		\
	((phys) - CONFIG_DRAM_BASE + PAGE_OFFSET)))

#define __virt_to_phys(virt)								\
	(((PHYS_OFFSET) == DDR_PADDR_ACP) ? ((virt) - PAGE_OFFSET + DDR_PADDR_ACP) :	\
	(((virt) >= PAGE_OFFSET1) ? ((virt) - PAGE_OFFSET1 + PHYS_OFFSET2) :		\
	((virt) - PAGE_OFFSET + CONFIG_DRAM_BASE)))
#else /* !CONFIG_SPARSEMEM */
#define __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)
#endif /* CONFIG_SPARSEMEM */

#endif
