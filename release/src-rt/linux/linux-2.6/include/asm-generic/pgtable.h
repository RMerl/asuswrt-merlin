#ifndef _ASM_GENERIC_PGTABLE_H
#define _ASM_GENERIC_PGTABLE_H

#ifndef __ASSEMBLY__
#ifdef CONFIG_MMU

#include <linux/mm_types.h>

#ifndef __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS
extern int ptep_set_access_flags(struct vm_area_struct *vma,
				 unsigned long address, pte_t *ptep,
				 pte_t entry, int dirty);
#endif

#ifndef __HAVE_ARCH_PMDP_SET_ACCESS_FLAGS
extern int pmdp_set_access_flags(struct vm_area_struct *vma,
				 unsigned long address, pmd_t *pmdp,
				 pmd_t entry, int dirty);
#endif

#ifndef __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
static inline int ptep_test_and_clear_young(struct vm_area_struct *vma,
					    unsigned long address,
					    pte_t *ptep)
{
	pte_t pte = *ptep;
	int r = 1;
	if (!pte_young(pte))
		r = 0;
	else
		set_pte_at(vma->vm_mm, address, ptep, pte_mkold(pte));
	return r;
}
#endif

#ifndef __HAVE_ARCH_PMDP_TEST_AND_CLEAR_YOUNG
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline int pmdp_test_and_clear_young(struct vm_area_struct *vma,
					    unsigned long address,
					    pmd_t *pmdp)
{
	pmd_t pmd = *pmdp;
	int r = 1;
	if (!pmd_young(pmd))
		r = 0;
	else
		set_pmd_at(vma->vm_mm, address, pmdp, pmd_mkold(pmd));
	return r;
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline int pmdp_test_and_clear_young(struct vm_area_struct *vma,
					    unsigned long address,
					    pmd_t *pmdp)
{
	BUG();
	return 0;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */
#endif

#ifndef __HAVE_ARCH_PTEP_CLEAR_YOUNG_FLUSH
int ptep_clear_flush_young(struct vm_area_struct *vma,
			   unsigned long address, pte_t *ptep);
#endif

#ifndef __HAVE_ARCH_PMDP_CLEAR_YOUNG_FLUSH
int pmdp_clear_flush_young(struct vm_area_struct *vma,
			   unsigned long address, pmd_t *pmdp);
#endif

#ifndef __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(struct mm_struct *mm,
				       unsigned long address,
				       pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(mm, address, ptep);
	return pte;
}
#endif

#ifndef __HAVE_ARCH_PMDP_GET_AND_CLEAR
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline pmd_t pmdp_get_and_clear(struct mm_struct *mm,
				       unsigned long address,
				       pmd_t *pmdp)
{
	pmd_t pmd = *pmdp;
	pmd_clear(mm, address, pmdp);
	return pmd;
})
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */
#endif

#ifndef __HAVE_ARCH_PTEP_GET_AND_CLEAR_FULL
static inline pte_t ptep_get_and_clear_full(struct mm_struct *mm,
					    unsigned long address, pte_t *ptep,
					    int full)
{
	pte_t pte;
	pte = ptep_get_and_clear(mm, address, ptep);
	return pte;
}
#endif

/*
 * Some architectures may be able to avoid expensive synchronization
 * primitives when modifications are made to PTE's which are already
 * not present, or in the process of an address space destruction.
 */
#ifndef __HAVE_ARCH_PTE_CLEAR_NOT_PRESENT_FULL
static inline void pte_clear_not_present_full(struct mm_struct *mm,
					      unsigned long address,
					      pte_t *ptep,
					      int full)
{
	pte_clear(mm, address, ptep);
}
#endif

#ifndef __HAVE_ARCH_PTEP_CLEAR_FLUSH
extern pte_t ptep_clear_flush(struct vm_area_struct *vma,
			      unsigned long address,
			      pte_t *ptep);
#endif

#ifndef __HAVE_ARCH_PMDP_CLEAR_FLUSH
extern pmd_t pmdp_clear_flush(struct vm_area_struct *vma,
			      unsigned long address,
			      pmd_t *pmdp);
#endif

#ifndef __HAVE_ARCH_PTEP_SET_WRPROTECT
struct mm_struct;
static inline void ptep_set_wrprotect(struct mm_struct *mm, unsigned long address, pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte_at(mm, address, ptep, pte_wrprotect(old_pte));
}
#endif

#ifndef __HAVE_ARCH_PMDP_SET_WRPROTECT
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline void pmdp_set_wrprotect(struct mm_struct *mm,
				      unsigned long address, pmd_t *pmdp)
{
	pmd_t old_pmd = *pmdp;
	set_pmd_at(mm, address, pmdp, pmd_wrprotect(old_pmd));
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline void pmdp_set_wrprotect(struct mm_struct *mm,
				      unsigned long address, pmd_t *pmdp)
{
	BUG();
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */
#endif

#ifndef __HAVE_ARCH_PMDP_SPLITTING_FLUSH
extern pmd_t pmdp_splitting_flush(struct vm_area_struct *vma,
				  unsigned long address,
				  pmd_t *pmdp);
#endif

#ifndef __HAVE_ARCH_PTE_SAME
static inline int pte_same(pte_t pte_a, pte_t pte_b)
{
	return pte_val(pte_a) == pte_val(pte_b);
}
#endif

#ifndef __HAVE_ARCH_PMD_SAME
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline int pmd_same(pmd_t pmd_a, pmd_t pmd_b)
{
	return pmd_val(pmd_a) == pmd_val(pmd_b);
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline int pmd_same(pmd_t pmd_a, pmd_t pmd_b)
{
	BUG();
	return 0;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */
#endif

#ifndef __HAVE_ARCH_PAGE_TEST_DIRTY
#define page_test_dirty(page)		(0)
#endif

#ifndef __HAVE_ARCH_PAGE_CLEAR_DIRTY
#define page_clear_dirty(page, mapped)	do { } while (0)
#endif

#ifndef __HAVE_ARCH_PAGE_TEST_DIRTY
#define pte_maybe_dirty(pte)		pte_dirty(pte)
#else
#define pte_maybe_dirty(pte)		(1)
#endif

#ifndef __HAVE_ARCH_PAGE_TEST_AND_CLEAR_YOUNG
#define page_test_and_clear_young(page) (0)
#endif

#ifndef __HAVE_ARCH_PGD_OFFSET_GATE
#define pgd_offset_gate(mm, addr)	pgd_offset(mm, addr)
#endif

#ifndef __HAVE_ARCH_MOVE_PTE
#define move_pte(pte, prot, old_addr, new_addr)	(pte)
#endif

#ifndef flush_tlb_fix_spurious_fault
#define flush_tlb_fix_spurious_fault(vma, address) flush_tlb_page(vma, address)
#endif

#ifndef pgprot_noncached
#define pgprot_noncached(prot)	(prot)
#endif

#ifndef pgprot_writecombine
#define pgprot_writecombine pgprot_noncached
#endif

/*
 * When walking page tables, get the address of the next boundary,
 * or the end address of the range if that comes earlier.  Although no
 * vma end wraps to 0, rounded up __boundary may wrap to 0 throughout.
 */

#define pgd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

#ifndef pud_addr_end
#define pud_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PUD_SIZE) & PUD_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})
#endif

#ifndef pmd_addr_end
#define pmd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PMD_SIZE) & PMD_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})
#endif

/*
 * When walking page tables, we usually want to skip any p?d_none entries;
 * and any p?d_bad entries - reporting the error before resetting to none.
 * Do the tests inline, but report and clear the bad entry in mm/memory.c.
 */
void pgd_clear_bad(pgd_t *);
void pud_clear_bad(pud_t *);
void pmd_clear_bad(pmd_t *);

static inline int pgd_none_or_clear_bad(pgd_t *pgd)
{
	if (pgd_none(*pgd))
		return 1;
	if (unlikely(pgd_bad(*pgd))) {
		pgd_clear_bad(pgd);
		return 1;
	}
	return 0;
}

static inline int pud_none_or_clear_bad(pud_t *pud)
{
	if (pud_none(*pud))
		return 1;
	if (unlikely(pud_bad(*pud))) {
		pud_clear_bad(pud);
		return 1;
	}
	return 0;
}

static inline int pmd_none_or_clear_bad(pmd_t *pmd)
{
	if (pmd_none(*pmd))
		return 1;
	if (unlikely(pmd_bad(*pmd))) {
		pmd_clear_bad(pmd);
		return 1;
	}
	return 0;
}

static inline pte_t __ptep_modify_prot_start(struct mm_struct *mm,
					     unsigned long addr,
					     pte_t *ptep)
{
	/*
	 * Get the current pte state, but zero it out to make it
	 * non-present, preventing the hardware from asynchronously
	 * updating it.
	 */
	return ptep_get_and_clear(mm, addr, ptep);
}

static inline void __ptep_modify_prot_commit(struct mm_struct *mm,
					     unsigned long addr,
					     pte_t *ptep, pte_t pte)
{
	/*
	 * The pte is non-present, so there's no hardware state to
	 * preserve.
	 */
	set_pte_at(mm, addr, ptep, pte);
}

#ifndef __HAVE_ARCH_PTEP_MODIFY_PROT_TRANSACTION
/*
 * Start a pte protection read-modify-write transaction, which
 * protects against asynchronous hardware modifications to the pte.
 * The intention is not to prevent the hardware from making pte
 * updates, but to prevent any updates it may make from being lost.
 *
 * This does not protect against other software modifications of the
 * pte; the appropriate pte lock must be held over the transation.
 *
 * Note that this interface is intended to be batchable, meaning that
 * ptep_modify_prot_commit may not actually update the pte, but merely
 * queue the update to be done at some later time.  The update must be
 * actually committed before the pte lock is released, however.
 */
static inline pte_t ptep_modify_prot_start(struct mm_struct *mm,
					   unsigned long addr,
					   pte_t *ptep)
{
	return __ptep_modify_prot_start(mm, addr, ptep);
}

/*
 * Commit an update to a pte, leaving any hardware-controlled bits in
 * the PTE unmodified.
 */
static inline void ptep_modify_prot_commit(struct mm_struct *mm,
					   unsigned long addr,
					   pte_t *ptep, pte_t pte)
{
	__ptep_modify_prot_commit(mm, addr, ptep, pte);
}
#endif /* __HAVE_ARCH_PTEP_MODIFY_PROT_TRANSACTION */
#endif /* CONFIG_MMU */

/*
 * A facility to provide lazy MMU batching.  This allows PTE updates and
 * page invalidations to be delayed until a call to leave lazy MMU mode
 * is issued.  Some architectures may benefit from doing this, and it is
 * beneficial for both shadow and direct mode hypervisors, which may batch
 * the PTE updates which happen during this window.  Note that using this
 * interface requires that read hazards be removed from the code.  A read
 * hazard could result in the direct mode hypervisor case, since the actual
 * write to the page tables may not yet have taken place, so reads though
 * a raw PTE pointer after it has been modified are not guaranteed to be
 * up to date.  This mode can only be entered and left under the protection of
 * the page table locks for all page tables which may be modified.  In the UP
 * case, this is required so that preemption is disabled, and in the SMP case,
 * it must synchronize the delayed page table writes properly on other CPUs.
 */
#ifndef __HAVE_ARCH_ENTER_LAZY_MMU_MODE
#define arch_enter_lazy_mmu_mode()	do {} while (0)
#define arch_leave_lazy_mmu_mode()	do {} while (0)
#define arch_flush_lazy_mmu_mode()	do {} while (0)
#endif

/*
 * A facility to provide batching of the reload of page tables and
 * other process state with the actual context switch code for
 * paravirtualized guests.  By convention, only one of the batched
 * update (lazy) modes (CPU, MMU) should be active at any given time,
 * entry should never be nested, and entry and exits should always be
 * paired.  This is for sanity of maintaining and reasoning about the
 * kernel code.  In this case, the exit (end of the context switch) is
 * in architecture-specific code, and so doesn't need a generic
 * definition.
 */
#ifndef __HAVE_ARCH_START_CONTEXT_SWITCH
#define arch_start_context_switch(prev)	do {} while (0)
#endif

#ifndef __HAVE_PFNMAP_TRACKING
/*
 * Interface that can be used by architecture code to keep track of
 * memory type of pfn mappings (remap_pfn_range, vm_insert_pfn)
 *
 * track_pfn_vma_new is called when a _new_ pfn mapping is being established
 * for physical range indicated by pfn and size.
 */
static inline int track_pfn_vma_new(struct vm_area_struct *vma, pgprot_t *prot,
					unsigned long pfn, unsigned long size)
{
	return 0;
}

/*
 * Interface that can be used by architecture code to keep track of
 * memory type of pfn mappings (remap_pfn_range, vm_insert_pfn)
 *
 * track_pfn_vma_copy is called when vma that is covering the pfnmap gets
 * copied through copy_page_range().
 */
static inline int track_pfn_vma_copy(struct vm_area_struct *vma)
{
	return 0;
}

/*
 * Interface that can be used by architecture code to keep track of
 * memory type of pfn mappings (remap_pfn_range, vm_insert_pfn)
 *
 * untrack_pfn_vma is called while unmapping a pfnmap for a region.
 * untrack can be called for a specific region indicated by pfn and size or
 * can be for the entire vma (in which case size can be zero).
 */
static inline void untrack_pfn_vma(struct vm_area_struct *vma,
					unsigned long pfn, unsigned long size)
{
}
#else
extern int track_pfn_vma_new(struct vm_area_struct *vma, pgprot_t *prot,
				unsigned long pfn, unsigned long size);
extern int track_pfn_vma_copy(struct vm_area_struct *vma);
extern void untrack_pfn_vma(struct vm_area_struct *vma, unsigned long pfn,
				unsigned long size);
#endif

#ifndef CONFIG_TRANSPARENT_HUGEPAGE
static inline int pmd_trans_huge(pmd_t pmd)
{
	return 0;
}
static inline int pmd_trans_splitting(pmd_t pmd)
{
	return 0;
}
#ifndef __HAVE_ARCH_PMD_WRITE
static inline int pmd_write(pmd_t pmd)
{
	BUG();
	return 0;
}
#endif /* __HAVE_ARCH_PMD_WRITE */
#endif

#endif /* !__ASSEMBLY__ */

#endif /* _ASM_GENERIC_PGTABLE_H */
