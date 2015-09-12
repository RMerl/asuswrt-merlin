/*
 * Early initialization code for BCM94710 boards
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: prom.c,v 1.8 2010-07-09 06:00:16 $
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
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
#ifdef	CONFIG_CFE
#include <asm/fw/cfe/cfe_api.h>
#endif

#include "bcm947xx.h"

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
#ifdef	CONFIG_CFE
static int cfe_cons_handle;
#endif

#if defined(CONFIG_CFE) && defined(CONFIG_EARLY_PRINTK)
void prom_putchar(char c)
{
	while (cfe_write(cfe_cons_handle, &c, 1) == 0)
		;
}
#endif

#ifdef	CONFIG_CFE

static __init void prom_init_cfe(void)
{
	uint32_t cfe_ept;
	uint32_t cfe_handle;
	uint32_t cfe_eptseal;
	int argc = fw_arg0;
	char **envp = (char **) fw_arg2;
	int *prom_vec = (int *) fw_arg3;

	/*
	 * Check if a loader was used; if NOT, the 4 arguments are
	 * what CFE gives us (handle, 0, EPT and EPTSEAL)
	 */
	if (argc < 0) {
		cfe_handle = (uint32_t)argc;
		cfe_ept = (uint32_t)envp;
		cfe_eptseal = (uint32_t)prom_vec;
	} else {
		if ((int)prom_vec < 0) {
			/*
			 * Old loader; all it gives us is the handle,
			 * so use the "known" entrypoint and assume
			 * the seal.
			 */
			cfe_handle = (uint32_t)prom_vec;
			cfe_ept = 0xBFC00500;
			cfe_eptseal = CFE_EPTSEAL;
		} else {
			/*
			 * Newer loaders bundle the handle/ept/eptseal
			 * Note: prom_vec is in the loader's useg
			 * which is still alive in the TLB.
			 */
			cfe_handle = prom_vec[0];
			cfe_ept = prom_vec[2];
			cfe_eptseal = prom_vec[3];
		}
	}

	if (cfe_eptseal != CFE_EPTSEAL) {
		/* too early for panic to do any good */
		printk(KERN_ERR "CFE's entrypoint seal doesn't match.");
		while (1) ;
	}

	cfe_init(cfe_handle, cfe_ept);
}

static __init void prom_init_console(void)
{
	/* Initialize CFE console */
	cfe_cons_handle = cfe_getstdhandle(CFE_STDHANDLE_CONSOLE);
}

static __init void prom_init_cmdline(void)
{
	static char buf[COMMAND_LINE_SIZE] __initdata;

	/* Get the kernel command line from CFE */
	if (cfe_getenv("LINUX_CMDLINE", buf, COMMAND_LINE_SIZE) >= 0) {
		buf[COMMAND_LINE_SIZE - 1] = 0;
		strcpy(arcs_cmdline, buf);
	}

	/* Force a console handover by adding a console= argument if needed,
	 * as CFE is not available anymore later in the boot process. */
	if ((strstr(arcs_cmdline, "console=")) == NULL) {
		/* Try to read the default serial port used by CFE */
		if ((cfe_getenv("BOOT_CONSOLE", buf, COMMAND_LINE_SIZE) < 0)
		    || (strncmp("uart", buf, 4)))
			/* Default to uart0 */
			strcpy(buf, "uart0");

		/* Compute the new command line */
		snprintf(arcs_cmdline, COMMAND_LINE_SIZE, "%s console=ttyS%c,115200",
			 arcs_cmdline, buf[4]);
	}
}
#endif	/* CONFIG_CFE */

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	/* These are not really being used anywhere - LR */
	mips_machgroup = MACH_GROUP_BRCM;
	mips_machtype = MACH_BCM947XX;
#endif

#ifdef	CONFIG_CFE
	prom_init_cfe();
	prom_init_console();
	prom_init_cmdline();
#endif

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
	}
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
