/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/homecache.h>

#define K(x) ((x) << (PAGE_SHIFT-10))

/*
 * The normal show_free_areas() is too verbose on Tile, with dozens
 * of processors and often four NUMA zones each with high and lowmem.
 */
void show_mem(void)
{
	struct zone *zone;

	pr_err("Active:%lu inactive:%lu dirty:%lu writeback:%lu unstable:%lu"
	       " free:%lu\n slab:%lu mapped:%lu pagetables:%lu bounce:%lu"
	       " pagecache:%lu swap:%lu\n",
	       (global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE)),
	       (global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE)),
	       global_page_state(NR_FILE_DIRTY),
	       global_page_state(NR_WRITEBACK),
	       global_page_state(NR_UNSTABLE_NFS),
	       global_page_state(NR_FREE_PAGES),
	       (global_page_state(NR_SLAB_RECLAIMABLE) +
		global_page_state(NR_SLAB_UNRECLAIMABLE)),
	       global_page_state(NR_FILE_MAPPED),
	       global_page_state(NR_PAGETABLE),
	       global_page_state(NR_BOUNCE),
	       global_page_state(NR_FILE_PAGES),
	       nr_swap_pages);

	for_each_zone(zone) {
		unsigned long flags, order, total = 0, largest_order = -1;

		if (!populated_zone(zone))
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			int nr = zone->free_area[order].nr_free;
			total += nr << order;
			if (nr)
				largest_order = order;
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		pr_err("Node %d %7s: %lukB (largest %luKb)\n",
		       zone_to_nid(zone), zone->name,
		       K(total), largest_order ? K(1UL) << largest_order : 0);
	}
}

/*
 * Associate a virtual page frame with a given physical page frame
 * and protection flags for that frame.
 */
static void set_pte_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	/* <pfn,flags> stored as-is, to permit clearing entries */
	set_pte(pte, pfn_pte(pfn, flags));

	/*
	 * It's enough to flush this one mapping.
	 * This appears conservative since it is only called
	 * from __set_fixmap.
	 */
	local_flush_tlb_page(NULL, vaddr, PAGE_SIZE);
}

void __set_fixmap(enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_pfn(address, phys >> PAGE_SHIFT, flags);
}

#if defined(CONFIG_HIGHPTE)
pte_t *_pte_offset_map(pmd_t *dir, unsigned long address, enum km_type type)
{
	pte_t *pte = kmap_atomic(pmd_page(*dir), type) +
		(pmd_ptfn(*dir) << HV_LOG2_PAGE_TABLE_ALIGN) & ~PAGE_MASK;
	return &pte[pte_index(address)];
}
#endif

/*
 * List of all pgd's needed so it can invalidate entries in both cached
 * and uncached pgd's. This is essentially codepath-based locking
 * against pageattr.c; it is the unique case in which a valid change
 * of kernel pagetables can't be lazily synchronized by vmalloc faults.
 * vmalloc faults work because attached pagetables are never freed.
 * The locking scheme was chosen on the basis of manfred's
 * recommendations and having no core impact whatsoever.
 * -- wli
 */
DEFINE_SPINLOCK(pgd_lock);
LIST_HEAD(pgd_list);

static inline void pgd_list_add(pgd_t *pgd)
{
	list_add(pgd_to_list(pgd), &pgd_list);
}

static inline void pgd_list_del(pgd_t *pgd)
{
	list_del(pgd_to_list(pgd));
}

#define KERNEL_PGD_INDEX_START pgd_index(PAGE_OFFSET)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD - KERNEL_PGD_INDEX_START)

static void pgd_ctor(pgd_t *pgd)
{
	unsigned long flags;

	memset(pgd, 0, KERNEL_PGD_INDEX_START*sizeof(pgd_t));
	spin_lock_irqsave(&pgd_lock, flags);

#ifndef __tilegx__
	/*
	 * Check that the user interrupt vector has no L2.
	 * It never should for the swapper, and new page tables
	 * should always start with an empty user interrupt vector.
	 */
	BUG_ON(((u64 *)swapper_pg_dir)[pgd_index(MEM_USER_INTRPT)] != 0);
#endif

	clone_pgd_range(pgd + KERNEL_PGD_INDEX_START,
			swapper_pg_dir + KERNEL_PGD_INDEX_START,
			KERNEL_PGD_PTRS);

	pgd_list_add(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

static void pgd_dtor(pgd_t *pgd)
{
	unsigned long flags; /* can be called from interrupt context */

	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = kmem_cache_alloc(pgd_cache, GFP_KERNEL);
	if (pgd)
		pgd_ctor(pgd);
	return pgd;
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	pgd_dtor(pgd);
	kmem_cache_free(pgd_cache, pgd);
}


#define L2_USER_PGTABLE_PAGES (1 << L2_USER_PGTABLE_ORDER)

struct page *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	gfp_t flags = GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO|__GFP_COMP;
	struct page *p;

#ifdef CONFIG_HIGHPTE
	flags |= __GFP_HIGHMEM;
#endif

	p = alloc_pages(flags, L2_USER_PGTABLE_ORDER);
	if (p == NULL)
		return NULL;

	pgtable_page_ctor(p);
	return p;
}

/*
 * Free page immediately (used in __pte_alloc if we raced with another
 * process).  We have to correct whatever pte_alloc_one() did before
 * returning the pages to the allocator.
 */
void pte_free(struct mm_struct *mm, struct page *p)
{
	pgtable_page_dtor(p);
	__free_pages(p, L2_USER_PGTABLE_ORDER);
}

void __pte_free_tlb(struct mmu_gather *tlb, struct page *pte,
		    unsigned long address)
{
	int i;

	pgtable_page_dtor(pte);
	tlb->need_flush = 1;
	if (tlb_fast_mode(tlb)) {
		struct page *pte_pages[L2_USER_PGTABLE_PAGES];
		for (i = 0; i < L2_USER_PGTABLE_PAGES; ++i)
			pte_pages[i] = pte + i;
		free_pages_and_swap_cache(pte_pages, L2_USER_PGTABLE_PAGES);
		return;
	}
	for (i = 0; i < L2_USER_PGTABLE_PAGES; ++i) {
		tlb->pages[tlb->nr++] = pte + i;
		if (tlb->nr >= FREE_PTE_NR)
			tlb_flush_mmu(tlb, 0, 0);
	}
}

#ifndef __tilegx__

int ptep_test_and_clear_young(struct vm_area_struct *vma,
			      unsigned long addr, pte_t *ptep)
{
#if HV_PTE_INDEX_ACCESSED < 8 || HV_PTE_INDEX_ACCESSED >= 16
# error Code assumes HV_PTE "accessed" bit in second byte
#endif
	u8 *tmp = (u8 *)ptep;
	u8 second_byte = tmp[1];
	if (!(second_byte & (1 << (HV_PTE_INDEX_ACCESSED - 8))))
		return 0;
	tmp[1] = second_byte & ~(1 << (HV_PTE_INDEX_ACCESSED - 8));
	return 1;
}

/*
 * This implementation is atomic vs hypervisor writes, since the hypervisor
 * always writes the low word (where "accessed" and "dirty" are) and this
 * routine only writes the high word.
 */
void ptep_set_wrprotect(struct mm_struct *mm,
			unsigned long addr, pte_t *ptep)
{
#if HV_PTE_INDEX_WRITABLE < 32
# error Code assumes HV_PTE "writable" bit in high word
#endif
	u32 *tmp = (u32 *)ptep;
	tmp[1] = tmp[1] & ~(1 << (HV_PTE_INDEX_WRITABLE - 32));
}

#endif

pte_t *virt_to_pte(struct mm_struct* mm, unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	if (pgd_addr_invalid(addr))
		return NULL;

	pgd = mm ? pgd_offset(mm, addr) : swapper_pg_dir + pgd_index(addr);
	pud = pud_offset(pgd, addr);
	if (!pud_present(*pud))
		return NULL;
	pmd = pmd_offset(pud, addr);
	if (pmd_huge_page(*pmd))
		return (pte_t *)pmd;
	if (!pmd_present(*pmd))
		return NULL;
	return pte_offset_kernel(pmd, addr);
}

pgprot_t set_remote_cache_cpu(pgprot_t prot, int cpu)
{
	unsigned int width = smp_width;
	int x = cpu % width;
	int y = cpu / width;
	BUG_ON(y >= smp_height);
	BUG_ON(hv_pte_get_mode(prot) != HV_PTE_MODE_CACHE_TILE_L3);
	BUG_ON(cpu < 0 || cpu >= NR_CPUS);
	BUG_ON(!cpu_is_valid_lotar(cpu));
	return hv_pte_set_lotar(prot, HV_XY_TO_LOTAR(x, y));
}

int get_remote_cache_cpu(pgprot_t prot)
{
	HV_LOTAR lotar = hv_pte_get_lotar(prot);
	int x = HV_LOTAR_X(lotar);
	int y = HV_LOTAR_Y(lotar);
	BUG_ON(hv_pte_get_mode(prot) != HV_PTE_MODE_CACHE_TILE_L3);
	return x + y * smp_width;
}

void set_pte_order(pte_t *ptep, pte_t pte, int order)
{
	unsigned long pfn = pte_pfn(pte);
	struct page *page = pfn_to_page(pfn);

	/* Update the home of a PTE if necessary */
	pte = pte_set_home(pte, page_home(page));

#ifdef __tilegx__
	*ptep = pte;
#else
	/*
	 * When setting a PTE, write the high bits first, then write
	 * the low bits.  This sets the "present" bit only after the
	 * other bits are in place.  If a particular PTE update
	 * involves transitioning from one valid PTE to another, it
	 * may be necessary to call set_pte_order() more than once,
	 * transitioning via a suitable intermediate state.
	 * Note that this sequence also means that if we are transitioning
	 * from any migrating PTE to a non-migrating one, we will not
	 * see a half-updated PTE with the migrating bit off.
	 */
#if HV_PTE_INDEX_PRESENT >= 32 || HV_PTE_INDEX_MIGRATING >= 32
# error Must write the present and migrating bits last
#endif
	((u32 *)ptep)[1] = (u32)(pte_val(pte) >> 32);
	barrier();
	((u32 *)ptep)[0] = (u32)(pte_val(pte));
#endif
}

/* Can this mm load a PTE with cached_priority set? */
static inline int mm_is_priority_cached(struct mm_struct *mm)
{
	return mm->context.priority_cached;
}

/*
 * Add a priority mapping to an mm_context and
 * notify the hypervisor if this is the first one.
 */
void start_mm_caching(struct mm_struct *mm)
{
	if (!mm_is_priority_cached(mm)) {
		mm->context.priority_cached = -1U;
		hv_set_caching(-1U);
	}
}

/*
 * Validate and return the priority_cached flag.  We know if it's zero
 * that we don't need to scan, since we immediately set it non-zero
 * when we first consider a MAP_CACHE_PRIORITY mapping.
 *
 * We only _try_ to acquire the mmap_sem semaphore; if we can't acquire it,
 * since we're in an interrupt context (servicing switch_mm) we don't
 * worry about it and don't unset the "priority_cached" field.
 * Presumably we'll come back later and have more luck and clear
 * the value then; for now we'll just keep the cache marked for priority.
 */
static unsigned int update_priority_cached(struct mm_struct *mm)
{
	if (mm->context.priority_cached && down_write_trylock(&mm->mmap_sem)) {
		struct vm_area_struct *vm;
		for (vm = mm->mmap; vm; vm = vm->vm_next) {
			if (hv_pte_get_cached_priority(vm->vm_page_prot))
				break;
		}
		if (vm == NULL)
			mm->context.priority_cached = 0;
		up_write(&mm->mmap_sem);
	}
	return mm->context.priority_cached;
}

/* Set caching correctly for an mm that we are switching to. */
void check_mm_caching(struct mm_struct *prev, struct mm_struct *next)
{
	if (!mm_is_priority_cached(next)) {
		/*
		 * If the new mm doesn't use priority caching, just see if we
		 * need the hv_set_caching(), or can assume it's already zero.
		 */
		if (mm_is_priority_cached(prev))
			hv_set_caching(0);
	} else {
		hv_set_caching(update_priority_cached(next));
	}
}

#if CHIP_HAS_MMIO()

/* Map an arbitrary MMIO address, homed according to pgprot, into VA space. */
void __iomem *ioremap_prot(resource_size_t phys_addr, unsigned long size,
			   pgprot_t home)
{
	void *addr;
	struct vm_struct *area;
	unsigned long offset, last_addr;
	pgprot_t pgprot;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	/* Create a read/write, MMIO VA mapping homed at the requested shim. */
	pgprot = PAGE_KERNEL;
	pgprot = hv_pte_set_mode(pgprot, HV_PTE_MODE_MMIO);
	pgprot = hv_pte_set_lotar(pgprot, hv_pte_get_lotar(home));

	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

	/*
	 * Ok, go for it..
	 */
	area = get_vm_area(size, VM_IOREMAP /* | other flags? */);
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = area->addr;
	if (ioremap_page_range((unsigned long)addr, (unsigned long)addr + size,
			       phys_addr, pgprot)) {
		remove_vm_area((void *)(PAGE_MASK & (unsigned long) addr));
		return NULL;
	}
	return (__force void __iomem *) (offset + (char *)addr);
}
EXPORT_SYMBOL(ioremap_prot);

/* Map a PCI MMIO bus address into VA space. */
void __iomem *ioremap(resource_size_t phys_addr, unsigned long size)
{
	panic("ioremap for PCI MMIO is not supported");
}
EXPORT_SYMBOL(ioremap);

/* Unmap an MMIO VA mapping. */
void iounmap(volatile void __iomem *addr_in)
{
	volatile void __iomem *addr = (volatile void __iomem *)
		(PAGE_MASK & (unsigned long __force)addr_in);
	vunmap((void * __force)addr);
}
EXPORT_SYMBOL(iounmap);

#endif /* CHIP_HAS_MMIO() */
