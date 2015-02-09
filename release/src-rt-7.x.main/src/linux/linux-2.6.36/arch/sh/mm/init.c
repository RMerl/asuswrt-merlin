/*
 * linux/arch/sh/mm/init.c
 *
 *  Copyright (C) 1999  Niibe Yutaka
 *  Copyright (C) 2002 - 2010  Paul Mundt
 *
 *  Based on linux/arch/i386/mm/init.c:
 *   Copyright (C) 1995  Linus Torvalds
 */
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/bootmem.h>
#include <linux/proc_fs.h>
#include <linux/pagemap.h>
#include <linux/percpu.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <asm/mmu_context.h>
#include <asm/mmzone.h>
#include <asm/kexec.h>
#include <asm/tlb.h>
#include <asm/cacheflush.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/cache.h>
#include <asm/sizes.h>

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);
pgd_t swapper_pg_dir[PTRS_PER_PGD];

void __init generic_mem_init(void)
{
	memblock_add(__MEMORY_START, __MEMORY_SIZE);
}

void __init __weak plat_mem_setup(void)
{
	/* Nothing to see here, move along. */
}

#ifdef CONFIG_MMU
static pte_t *__get_pte_phys(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd)) {
		pgd_ERROR(*pgd);
		return NULL;
	}

	pud = pud_alloc(NULL, pgd, addr);
	if (unlikely(!pud)) {
		pud_ERROR(*pud);
		return NULL;
	}

	pmd = pmd_alloc(NULL, pud, addr);
	if (unlikely(!pmd)) {
		pmd_ERROR(*pmd);
		return NULL;
	}

	pte = pte_offset_kernel(pmd, addr);
	return pte;
}

static void set_pte_phys(unsigned long addr, unsigned long phys, pgprot_t prot)
{
	pte_t *pte;

	pte = __get_pte_phys(addr);
	if (!pte_none(*pte)) {
		pte_ERROR(*pte);
		return;
	}

	set_pte(pte, pfn_pte(phys >> PAGE_SHIFT, prot));
	local_flush_tlb_one(get_asid(), addr);

	if (pgprot_val(prot) & _PAGE_WIRED)
		tlb_wire_entry(NULL, addr, *pte);
}

static void clear_pte_phys(unsigned long addr, pgprot_t prot)
{
	pte_t *pte;

	pte = __get_pte_phys(addr);

	if (pgprot_val(prot) & _PAGE_WIRED)
		tlb_unwire_entry();

	set_pte(pte, pfn_pte(0, __pgprot(0)));
	local_flush_tlb_one(get_asid(), addr);
}

void __set_fixmap(enum fixed_addresses idx, unsigned long phys, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}

	set_pte_phys(address, phys, prot);
}

void __clear_fixmap(enum fixed_addresses idx, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}

	clear_pte_phys(address, prot);
}

void __init page_table_range_init(unsigned long start, unsigned long end,
					 pgd_t *pgd_base)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int i, j, k;
	unsigned long vaddr;

	vaddr = start;
	i = __pgd_offset(vaddr);
	j = __pud_offset(vaddr);
	k = __pmd_offset(vaddr);
	pgd = pgd_base + i;

	for ( ; (i < PTRS_PER_PGD) && (vaddr != end); pgd++, i++) {
		pud = (pud_t *)pgd;
		for ( ; (j < PTRS_PER_PUD) && (vaddr != end); pud++, j++) {
#ifdef __PAGETABLE_PMD_FOLDED
			pmd = (pmd_t *)pud;
#else
			pmd = (pmd_t *)alloc_bootmem_low_pages(PAGE_SIZE);
			pud_populate(&init_mm, pud, pmd);
			pmd += k;
#endif
			for (; (k < PTRS_PER_PMD) && (vaddr != end); pmd++, k++) {
				if (pmd_none(*pmd)) {
					pte = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
					pmd_populate_kernel(&init_mm, pmd, pte);
					BUG_ON(pte != pte_offset_kernel(pmd, 0));
				}
				vaddr += PMD_SIZE;
			}
			k = 0;
		}
		j = 0;
	}
}
#endif	/* CONFIG_MMU */

void __init allocate_pgdat(unsigned int nid)
{
	unsigned long start_pfn, end_pfn;
#ifdef CONFIG_NEED_MULTIPLE_NODES
	unsigned long phys;
#endif

	get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);

#ifdef CONFIG_NEED_MULTIPLE_NODES
	phys = __memblock_alloc_base(sizeof(struct pglist_data),
				SMP_CACHE_BYTES, end_pfn << PAGE_SHIFT);
	/* Retry with all of system memory */
	if (!phys)
		phys = __memblock_alloc_base(sizeof(struct pglist_data),
					SMP_CACHE_BYTES, memblock_end_of_DRAM());
	if (!phys)
		panic("Can't allocate pgdat for node %d\n", nid);

	NODE_DATA(nid) = __va(phys);
	memset(NODE_DATA(nid), 0, sizeof(struct pglist_data));

	NODE_DATA(nid)->bdata = &bootmem_node_data[nid];
#endif

	NODE_DATA(nid)->node_start_pfn = start_pfn;
	NODE_DATA(nid)->node_spanned_pages = end_pfn - start_pfn;
}

static void __init bootmem_init_one_node(unsigned int nid)
{
	unsigned long total_pages, paddr;
	unsigned long end_pfn;
	struct pglist_data *p;
	int i;

	p = NODE_DATA(nid);

	/* Nothing to do.. */
	if (!p->node_spanned_pages)
		return;

	end_pfn = p->node_start_pfn + p->node_spanned_pages;

	total_pages = bootmem_bootmap_pages(p->node_spanned_pages);

	paddr = memblock_alloc(total_pages << PAGE_SHIFT, PAGE_SIZE);
	if (!paddr)
		panic("Can't allocate bootmap for nid[%d]\n", nid);

	init_bootmem_node(p, paddr >> PAGE_SHIFT, p->node_start_pfn, end_pfn);

	free_bootmem_with_active_regions(nid, end_pfn);

	if (nid == 0) {
		/* Reserve the sections we're already using. */
		for (i = 0; i < memblock.reserved.cnt; i++)
			reserve_bootmem(memblock.reserved.region[i].base,
					memblock_size_bytes(&memblock.reserved, i),
					BOOTMEM_DEFAULT);
	}

	sparse_memory_present_with_active_regions(nid);
}

static void __init do_init_bootmem(void)
{
	int i;

	/* Add active regions with valid PFNs. */
	for (i = 0; i < memblock.memory.cnt; i++) {
		unsigned long start_pfn, end_pfn;
		start_pfn = memblock.memory.region[i].base >> PAGE_SHIFT;
		end_pfn = start_pfn + memblock_size_pages(&memblock.memory, i);
		__add_active_range(0, start_pfn, end_pfn);
	}

	/* All of system RAM sits in node 0 for the non-NUMA case */
	allocate_pgdat(0);
	node_set_online(0);

	plat_mem_setup();

	for_each_online_node(i)
		bootmem_init_one_node(i);

	sparse_init();
}

static void __init early_reserve_mem(void)
{
	unsigned long start_pfn;

	/*
	 * Partially used pages are not usable - thus
	 * we are rounding upwards:
	 */
	start_pfn = PFN_UP(__pa(_end));

	/*
	 * Reserve the kernel text and Reserve the bootmem bitmap. We do
	 * this in two steps (first step was init_bootmem()), because
	 * this catches the (definitely buggy) case of us accidentally
	 * initializing the bootmem allocator with an invalid RAM area.
	 */
	memblock_reserve(__MEMORY_START + CONFIG_ZERO_PAGE_OFFSET,
		    (PFN_PHYS(start_pfn) + PAGE_SIZE - 1) -
		    (__MEMORY_START + CONFIG_ZERO_PAGE_OFFSET));

	/*
	 * Reserve physical pages below CONFIG_ZERO_PAGE_OFFSET.
	 */
	if (CONFIG_ZERO_PAGE_OFFSET != 0)
		memblock_reserve(__MEMORY_START, CONFIG_ZERO_PAGE_OFFSET);

	/*
	 * Handle additional early reservations
	 */
	check_for_initrd();
	reserve_crashkernel();
}

void __init paging_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];
	unsigned long vaddr, end;
	int nid;

	memblock_init();

	sh_mv.mv_mem_init();

	early_reserve_mem();

	memblock_enforce_memory_limit(memory_limit);
	memblock_analyze();

	memblock_dump_all();

	/*
	 * Determine low and high memory ranges:
	 */
	max_low_pfn = max_pfn = memblock_end_of_DRAM() >> PAGE_SHIFT;
	min_low_pfn = __MEMORY_START >> PAGE_SHIFT;

	nodes_clear(node_online_map);

	memory_start = (unsigned long)__va(__MEMORY_START);
	memory_end = memory_start + (memory_limit ?: memblock_phys_mem_size());

	uncached_init();
	pmb_init();
	do_init_bootmem();
	ioremap_fixed_init();

	/* We don't need to map the kernel through the TLB, as
	 * it is permanatly mapped using P1. So clear the
	 * entire pgd. */
	memset(swapper_pg_dir, 0, sizeof(swapper_pg_dir));

	/* Set an initial value for the MMU.TTB so we don't have to
	 * check for a null value. */
	set_TTB(swapper_pg_dir);

	/*
	 * Populate the relevant portions of swapper_pg_dir so that
	 * we can use the fixmap entries without calling kmalloc.
	 * pte's will be filled in by __set_fixmap().
	 */
	vaddr = __fix_to_virt(__end_of_fixed_addresses - 1) & PMD_MASK;
	end = (FIXADDR_TOP + PMD_SIZE - 1) & PMD_MASK;
	page_table_range_init(vaddr, end, swapper_pg_dir);

	kmap_coherent_init();

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		unsigned long low, start_pfn;

		start_pfn = pgdat->bdata->node_min_pfn;
		low = pgdat->bdata->node_low_pfn;

		if (max_zone_pfns[ZONE_NORMAL] < low)
			max_zone_pfns[ZONE_NORMAL] = low;

		printk("Node %u: start_pfn = 0x%lx, low = 0x%lx\n",
		       nid, start_pfn, low);
	}

	free_area_init_nodes(max_zone_pfns);
}

/*
 * Early initialization for any I/O MMUs we might have.
 */
static void __init iommu_init(void)
{
	no_iommu_init();
}

unsigned int mem_init_done = 0;

void __init mem_init(void)
{
	int codesize, datasize, initsize;
	int nid;

	iommu_init();

	num_physpages = 0;
	high_memory = NULL;

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		unsigned long node_pages = 0;
		void *node_high_memory;

		num_physpages += pgdat->node_present_pages;

		if (pgdat->node_spanned_pages)
			node_pages = free_all_bootmem_node(pgdat);

		totalram_pages += node_pages;

		node_high_memory = (void *)__va((pgdat->node_start_pfn +
						 pgdat->node_spanned_pages) <<
						 PAGE_SHIFT);
		if (node_high_memory > high_memory)
			high_memory = node_high_memory;
	}

	/* Set this up early, so we can take care of the zero page */
	cpu_cache_init();

	/* clear the zero-page */
	memset(empty_zero_page, 0, PAGE_SIZE);
	__flush_wback_region(empty_zero_page, PAGE_SIZE);

	vsyscall_init();

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk(KERN_INFO "Memory: %luk/%luk available (%dk kernel code, "
	       "%dk data, %dk init)\n",
		nr_free_pages() << (PAGE_SHIFT-10),
		num_physpages << (PAGE_SHIFT-10),
		codesize >> 10,
		datasize >> 10,
		initsize >> 10);

	printk(KERN_INFO "virtual kernel memory layout:\n"
		"    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#ifdef CONFIG_HIGHMEM
		"    pkmap   : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#endif
		"    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n"
		"    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB) (cached)\n"
#ifdef CONFIG_UNCACHED_MAPPING
		"            : 0x%08lx - 0x%08lx   (%4ld MB) (uncached)\n"
#endif
		"      .init : 0x%08lx - 0x%08lx   (%4ld kB)\n"
		"      .data : 0x%08lx - 0x%08lx   (%4ld kB)\n"
		"      .text : 0x%08lx - 0x%08lx   (%4ld kB)\n",
		FIXADDR_START, FIXADDR_TOP,
		(FIXADDR_TOP - FIXADDR_START) >> 10,

#ifdef CONFIG_HIGHMEM
		PKMAP_BASE, PKMAP_BASE+LAST_PKMAP*PAGE_SIZE,
		(LAST_PKMAP*PAGE_SIZE) >> 10,
#endif

		(unsigned long)VMALLOC_START, VMALLOC_END,
		(VMALLOC_END - VMALLOC_START) >> 20,

		(unsigned long)memory_start, (unsigned long)high_memory,
		((unsigned long)high_memory - (unsigned long)memory_start) >> 20,

#ifdef CONFIG_UNCACHED_MAPPING
		uncached_start, uncached_end, uncached_size >> 20,
#endif

		(unsigned long)&__init_begin, (unsigned long)&__init_end,
		((unsigned long)&__init_end -
		 (unsigned long)&__init_begin) >> 10,

		(unsigned long)&_etext, (unsigned long)&_edata,
		((unsigned long)&_edata - (unsigned long)&_etext) >> 10,

		(unsigned long)&_text, (unsigned long)&_etext,
		((unsigned long)&_etext - (unsigned long)&_text) >> 10);

	mem_init_done = 1;
}

void free_initmem(void)
{
	unsigned long addr;

	addr = (unsigned long)(&__init_begin);
	for (; addr < (unsigned long)(&__init_end); addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		free_page(addr);
		totalram_pages++;
	}
	printk("Freeing unused kernel memory: %ldk freed\n",
	       ((unsigned long)&__init_end -
	        (unsigned long)&__init_begin) >> 10);
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	unsigned long p;
	for (p = start; p < end; p += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(p));
		init_page_count(virt_to_page(p));
		free_page(p);
		totalram_pages++;
	}
	printk("Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
}
#endif

#ifdef CONFIG_MEMORY_HOTPLUG
int arch_add_memory(int nid, u64 start, u64 size)
{
	pg_data_t *pgdat;
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	int ret;

	pgdat = NODE_DATA(nid);

	/* We only have ZONE_NORMAL, so this is easy.. */
	ret = __add_pages(nid, pgdat->node_zones + ZONE_NORMAL,
				start_pfn, nr_pages);
	if (unlikely(ret))
		printk("%s: Failed, __add_pages() == %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(arch_add_memory);

#ifdef CONFIG_NUMA
int memory_add_physaddr_to_nid(u64 addr)
{
	/* Node 0 for now.. */
	return 0;
}
EXPORT_SYMBOL_GPL(memory_add_physaddr_to_nid);
#endif

#endif /* CONFIG_MEMORY_HOTPLUG */
