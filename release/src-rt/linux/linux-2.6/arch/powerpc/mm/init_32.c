/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  PPC44x/36-bit changes by Matt Porter (mporter@mvista.com)
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/initrd.h>
#include <linux/pagemap.h>
#include <linux/memblock.h>
#include <linux/gfp.h>

#include <asm/pgalloc.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/smp.h>
#include <asm/machdep.h>
#include <asm/btext.h>
#include <asm/tlb.h>
#include <asm/sections.h>
#include <asm/system.h>

#include "mmu_decl.h"

#if defined(CONFIG_KERNEL_START_BOOL) || defined(CONFIG_LOWMEM_SIZE_BOOL)
/* The amount of lowmem must be within 0xF0000000 - KERNELBASE. */
#if (CONFIG_LOWMEM_SIZE > (0xF0000000 - PAGE_OFFSET))
#error "You must adjust CONFIG_LOWMEM_SIZE or CONFIG_START_KERNEL"
#endif
#endif
#define MAX_LOW_MEM	CONFIG_LOWMEM_SIZE

phys_addr_t total_memory;
phys_addr_t total_lowmem;

phys_addr_t memstart_addr = (phys_addr_t)~0ull;
EXPORT_SYMBOL(memstart_addr);
phys_addr_t kernstart_addr;
EXPORT_SYMBOL(kernstart_addr);
phys_addr_t lowmem_end_addr;

int boot_mapsize;
#ifdef CONFIG_PPC_PMAC
unsigned long agp_special_page;
EXPORT_SYMBOL(agp_special_page);
#endif

void MMU_init(void);

/* XXX should be in current.h  -- paulus */
extern struct task_struct *current_set[NR_CPUS];

/*
 * this tells the system to map all of ram with the segregs
 * (i.e. page tables) instead of the bats.
 * -- Cort
 */
int __map_without_bats;
int __map_without_ltlbs;

/*
 * This tells the system to allow ioremapping memory marked as reserved.
 */
int __allow_ioremap_reserved;

/* max amount of low RAM to map in */
unsigned long __max_low_memory = MAX_LOW_MEM;

/*
 * Check for command-line options that affect what MMU_init will do.
 */
void MMU_setup(void)
{
	/* Check for nobats option (used in mapin_ram). */
	if (strstr(cmd_line, "nobats")) {
		__map_without_bats = 1;
	}

	if (strstr(cmd_line, "noltlbs")) {
		__map_without_ltlbs = 1;
	}
#ifdef CONFIG_DEBUG_PAGEALLOC
	__map_without_bats = 1;
	__map_without_ltlbs = 1;
#endif
}

/*
 * MMU_init sets up the basic memory mappings for the kernel,
 * including both RAM and possibly some I/O regions,
 * and sets up the page tables and the MMU hardware ready to go.
 */
void __init MMU_init(void)
{
	if (ppc_md.progress)
		ppc_md.progress("MMU:enter", 0x111);

	/* parse args from command line */
	MMU_setup();

	if (memblock.memory.cnt > 1) {
#ifndef CONFIG_WII
		memblock.memory.cnt = 1;
		memblock_analyze();
		printk(KERN_WARNING "Only using first contiguous memory region");
#else
		wii_memory_fixups();
#endif
	}

	total_lowmem = total_memory = memblock_end_of_DRAM() - memstart_addr;
	lowmem_end_addr = memstart_addr + total_lowmem;

#ifdef CONFIG_FSL_BOOKE
	/* Freescale Book-E parts expect lowmem to be mapped by fixed TLB
	 * entries, so we need to adjust lowmem to match the amount we can map
	 * in the fixed entries */
	adjust_total_lowmem();
#endif /* CONFIG_FSL_BOOKE */

	if (total_lowmem > __max_low_memory) {
		total_lowmem = __max_low_memory;
		lowmem_end_addr = memstart_addr + total_lowmem;
#ifndef CONFIG_HIGHMEM
		total_memory = total_lowmem;
		memblock_enforce_memory_limit(total_lowmem);
		memblock_analyze();
#endif /* CONFIG_HIGHMEM */
	}

	/* Initialize the MMU hardware */
	if (ppc_md.progress)
		ppc_md.progress("MMU:hw init", 0x300);
	MMU_init_hw();

	/* Map in all of RAM starting at KERNELBASE */
	if (ppc_md.progress)
		ppc_md.progress("MMU:mapin", 0x301);
	mapin_ram();

	/* Initialize early top-down ioremap allocator */
	ioremap_bot = IOREMAP_TOP;

	/* Map in I/O resources */
	if (ppc_md.progress)
		ppc_md.progress("MMU:setio", 0x302);

	if (ppc_md.progress)
		ppc_md.progress("MMU:exit", 0x211);

	/* From now on, btext is no longer BAT mapped if it was at all */
#ifdef CONFIG_BOOTX_TEXT
	btext_unmap();
#endif

	/* Shortly after that, the entire linear mapping will be available */
	memblock_set_current_limit(lowmem_end_addr);
}

/* This is only called until mem_init is done. */
void __init *early_get_page(void)
{
	if (init_bootmem_done)
		return alloc_bootmem_pages(PAGE_SIZE);
	else
		return __va(memblock_alloc(PAGE_SIZE, PAGE_SIZE));
}

/* Free up now-unused memory */
static void free_sec(unsigned long start, unsigned long end, const char *name)
{
	unsigned long cnt = 0;

	while (start < end) {
		ClearPageReserved(virt_to_page(start));
		init_page_count(virt_to_page(start));
		free_page(start);
		cnt++;
		start += PAGE_SIZE;
 	}
	if (cnt) {
		printk(" %ldk %s", cnt << (PAGE_SHIFT - 10), name);
		totalram_pages += cnt;
	}
}

void free_initmem(void)
{
#define FREESEC(TYPE) \
	free_sec((unsigned long)(&__ ## TYPE ## _begin), \
		 (unsigned long)(&__ ## TYPE ## _end), \
		 #TYPE);

	printk ("Freeing unused kernel memory:");
	FREESEC(init);
 	printk("\n");
	ppc_md.progress = NULL;
#undef FREESEC
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (start < end)
		printk ("Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		init_page_count(virt_to_page(start));
		free_page(start);
		totalram_pages++;
	}
}
#endif


#ifdef CONFIG_8xx /* No 8xx specific .c file to put that in ... */
void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				phys_addr_t first_memblock_size)
{
	/* We don't currently support the first MEMBLOCK not mapping 0
	 * physical on those processors
	 */
	BUG_ON(first_memblock_base != 0);

	/* 8xx can only access 8MB at the moment */
	memblock_set_current_limit(min_t(u64, first_memblock_size, 0x00800000));
}
#endif /* CONFIG_8xx */
