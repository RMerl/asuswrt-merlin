/*
 * Early initialization code for BCM94710 boards
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: prom.c,v 1.5 2009/06/08 17:32:33 Exp $
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <bcmendian.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <mipsinc.h>
#include <mips74k_core.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
/* Convenience */
#define sih bcm947xx_sih
#define MB      << 20

#ifdef  CONFIG_HIGHMEM

#define EXTVBASE        0xc0000000
#define ENTRYLO(x)      ((pte_val(pfn_pte((x) >> PAGE_SHIFT, PAGE_KERNEL_UNCACHED)) >> 6) | 1)
#define UNIQUE_ENTRYHI(idx) (CKSEG0 + ((idx) << (PAGE_SHIFT + 1)))

static unsigned long tmp_tlb_ent __initdata;

/* Initialize the wired register and all tlb entries to 
 * known good state.
 */
void __init
early_tlb_init(void)
{
	unsigned long  index;
	struct cpuinfo_mips *c = &current_cpu_data;

	tmp_tlb_ent = c->tlbsize;

	/* printk(KERN_ALERT "%s: tlb size %ld\n", __FUNCTION__, c->tlbsize); */

	/*
	* initialize entire TLB to uniqe virtual addresses
	* but with the PAGE_VALID bit not set
	*/
	write_c0_wired(0);
	write_c0_pagemask(PM_DEFAULT_MASK);

	write_c0_entrylo0(0);   /* not _PAGE_VALID */
	write_c0_entrylo1(0);

	for (index = 0; index < c->tlbsize; index++) {
		/* Make sure all entries differ. */
		write_c0_entryhi(UNIQUE_ENTRYHI(index+32));
		write_c0_index(index);
		mtc0_tlbw_hazard();
		tlb_write_indexed();
	}

	tlbw_use_hazard();

}

void __init
add_tmptlb_entry(unsigned long entrylo0, unsigned long entrylo1,
		 unsigned long entryhi, unsigned long pagemask)
{
/* write one tlb entry */
	--tmp_tlb_ent;
	write_c0_index(tmp_tlb_ent);
	write_c0_pagemask(pagemask);
	write_c0_entryhi(entryhi);
	write_c0_entrylo0(entrylo0);
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();
	tlb_write_indexed();
	tlbw_use_hazard();
}
#endif  /* CONFIG_HIGHMEM */

extern char ram_nvram_buf[];
void __init
prom_init(void)
{
	unsigned long mem, extmem = 0, off, data;
	unsigned long off1, data1;
	struct nvram_header *header;

	mips_machgroup = MACH_GROUP_BRCM;
	mips_machtype = MACH_BCM947XX;

	off = (unsigned long)prom_init;
	data = *(unsigned long *)prom_init;
	off1 = off + 4;
	data1 = *(unsigned long *)off1;

	/* Figure out memory size by finding aliases */
	for (mem = (1 MB); mem < (128 MB); mem <<= 1) {
		if ((*(unsigned long *)(off + mem) == data) &&
			(*(unsigned long *)(off1 + mem) == data1))
			break;
	}

#if CONFIG_RAM_SIZE
	{
		unsigned long config_mem;
		config_mem = CONFIG_RAM_SIZE * 0x100000;
		if (config_mem < mem)
			mem = config_mem;
	}
#endif
#ifdef  CONFIG_HIGHMEM
	if (mem == 128 MB) {
#if 0 // always false for BCM4706, needs CPU check
		bool highmem_region = FALSE;
		int idx;

		sih = si_kattach(SI_OSH);
		/* save current core index */
		idx = si_coreidx(sih);
		if ((si_setcore(sih, DMEMC_CORE_ID, 0) != NULL) ||
			(si_setcore(sih, DMEMS_CORE_ID, 0) != NULL)) {
			uint32 addr, size;
			uint asidx = 0;
			do {
				si_coreaddrspaceX(sih, asidx, &addr, &size);
				if (size == 0)
					break;
				if (addr == SI_SDRAM_R2) {
					highmem_region = TRUE;
					break;
				}
				asidx++;
			} while (1);
		}
		/* switch back to previous core */
		si_setcoreidx(sih, idx);
		if (highmem_region) {
#endif
		early_tlb_init();
		/* Add one temporary TLB entries to map SDRAM Region 2.
		*      Physical        Virtual
		*      0x80000000      0xc0000000      (1st: 256MB)
		*      0x90000000      0xd0000000      (2nd: 256MB)
		*/
		add_tmptlb_entry(ENTRYLO(SI_SDRAM_R2),
				 ENTRYLO(SI_SDRAM_R2 + (256 MB)),
				 EXTVBASE, PM_256M);

		off = EXTVBASE + __pa(off);
		for (extmem = (128 MB); extmem < (512 MB); extmem <<= 1) {
			if (*(unsigned long *)(off + extmem) == data)
				break;
		}

		extmem -= mem;
		/* Keep tlb entries back in consistent state */
		early_tlb_init();
	}
#if 0
	}
#endif
#endif  /* CONFIG_HIGHMEM */
	/* Ignoring the last page when ddr size is 128M. Cached
	 * accesses to last page is causing the processor to prefetch
	 * using address above 128M stepping out of the ddr address
	 * space.
	 */
	if (MIPS74K(current_cpu_data.processor_id) && (mem == (128 MB)))
		mem -= 0x1000;
	/* CFE could have loaded nvram during netboot
	 * to top 32KB of RAM, Just check for nvram signature
	 * and copy it to nvram space embedded in linux
	 * image for later use by nvram driver.
	 */
	header = (struct nvram_header *)(KSEG0ADDR(mem - NVRAM_SPACE));
	if (ltoh32(header->magic) == NVRAM_MAGIC) {
		uint32 *src = (uint32 *)header;
		uint32 *dst = (uint32 *)ram_nvram_buf;
		uint32 i;
		printk("Copying NVRAM bytes: %d from: 0x%p To: 0x%p\n", ltoh32(header->len),
			src, dst);
		for (i = 0; i < ltoh32(header->len) && i < NVRAM_SPACE; i += 4)
			*dst++ = ltoh32(*src++);
	}
#ifdef CONFIG_BLK_DEV_RAM
	init_ramdisk(mem);
#endif
	add_memory_region(SI_SDRAM_BASE, mem, BOOT_MEM_RAM);

#ifdef  CONFIG_HIGHMEM
	if (extmem) {
		/* We should deduct 0x1000 from the second memory
		 * region, because of the fact that processor does prefetch.
		 * Now that we are deducting a page from second memory 
		 * region, we could add the earlier deducted 4KB (from first bank)
		 * to the second region (the fact that 0x80000000 -> 0x88000000
		 * shadows 0x0 -> 0x8000000)
		 */
		if (MIPS74K(current_cpu_data.processor_id) && (mem == (128 MB)))
			extmem -= 0x1000;
		add_memory_region(SI_SDRAM_R2 + (128 MB) - 0x1000, extmem, BOOT_MEM_RAM);
	}
#endif  /* CONFIG_HIGHMEM */
}

void __init
prom_free_prom_memory(void)
{
}
