#ifndef __ASM_MACH_MEMORY_H
#define __ASM_MACH_MEMORY_H

#define PHYS_OFFSET	UL(CONFIG_MEMORY_START)
#define MEM_SIZE	UL(CONFIG_MEMORY_SIZE)

/* DMA memory at 0xf6000000 - 0xffdfffff */
#define CONSISTENT_DMA_SIZE (158 << 20)

#endif /* __ASM_MACH_MEMORY_H */
