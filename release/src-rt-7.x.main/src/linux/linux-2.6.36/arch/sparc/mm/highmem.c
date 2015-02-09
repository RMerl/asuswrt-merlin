/*
 *  highmem.c: virtual kernel memory mappings for high memory
 *
 *  Provides kernel-static versions of atomic kmap functions originally
 *  found as inlines in include/asm-sparc/highmem.h.  These became
 *  needed as kmap_atomic() and kunmap_atomic() started getting
 *  called from within modules.
 *  -- Tomas Szepe <szepe@pinerecords.com>, September 2002
 *
 *  But kmap_atomic() and kunmap_atomic() cannot be inlined in
 *  modules because they are loaded with btfixup-ped functions.
 */

#include <linux/mm.h>
#include <linux/highmem.h>
#include <asm/pgalloc.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/fixmap.h>

void *kmap_atomic(struct page *page, enum km_type type)
{
	unsigned long idx;
	unsigned long vaddr;

	/* even !CONFIG_PREEMPT needs this, for in_atomic in do_page_fault */
	pagefault_disable();
	if (!PageHighMem(page))
		return page_address(page);

	debug_kmap_atomic(type);
	idx = type + KM_TYPE_NR*smp_processor_id();
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);

	flush_cache_all();

#ifdef CONFIG_DEBUG_HIGHMEM
	BUG_ON(!pte_none(*(kmap_pte-idx)));
#endif
	set_pte(kmap_pte-idx, mk_pte(page, kmap_prot));
	flush_tlb_all();

	return (void*) vaddr;
}
EXPORT_SYMBOL(kmap_atomic);

void kunmap_atomic_notypecheck(void *kvaddr, enum km_type type)
{
#ifdef CONFIG_DEBUG_HIGHMEM
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;
	unsigned long idx = type + KM_TYPE_NR*smp_processor_id();

	if (vaddr < FIXADDR_START) {
		pagefault_enable();
		return;
	}

	BUG_ON(vaddr != __fix_to_virt(FIX_KMAP_BEGIN+idx));

	flush_cache_all();

	/*
	 * force other mappings to Oops if they'll try to access
	 * this pte without first remap it
	 */
	pte_clear(&init_mm, vaddr, kmap_pte-idx);
	flush_tlb_all();
#endif

	pagefault_enable();
}
EXPORT_SYMBOL(kunmap_atomic_notypecheck);

/* We may be fed a pagetable here by ptep_to_xxx and others. */
struct page *kmap_atomic_to_page(void *ptr)
{
	unsigned long idx, vaddr = (unsigned long)ptr;
	pte_t *pte;

	if (vaddr < SRMMU_NOCACHE_VADDR)
		return virt_to_page(ptr);
	if (vaddr < PKMAP_BASE)
		return pfn_to_page(__nocache_pa(vaddr) >> PAGE_SHIFT);
	BUG_ON(vaddr < FIXADDR_START);
	BUG_ON(vaddr > FIXADDR_TOP);

	idx = virt_to_fix(vaddr);
	pte = kmap_pte - (idx - FIX_KMAP_BEGIN);
	return pte_page(*pte);
}
