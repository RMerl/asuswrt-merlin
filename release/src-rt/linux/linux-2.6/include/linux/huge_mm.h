#ifndef _LINUX_HUGE_MM_H
#define _LINUX_HUGE_MM_H

extern int do_huge_pmd_anonymous_page(struct mm_struct *mm,
				      struct vm_area_struct *vma,
				      unsigned long address, pmd_t *pmd,
				      unsigned int flags);
extern int copy_huge_pmd(struct mm_struct *dst_mm, struct mm_struct *src_mm,
			 pmd_t *dst_pmd, pmd_t *src_pmd, unsigned long addr,
			 struct vm_area_struct *vma);
extern int do_huge_pmd_wp_page(struct mm_struct *mm, struct vm_area_struct *vma,
			       unsigned long address, pmd_t *pmd,
			       pmd_t orig_pmd);
extern pgtable_t get_pmd_huge_pte(struct mm_struct *mm);
extern struct page *follow_trans_huge_pmd(struct mm_struct *mm,
					  unsigned long addr,
					  pmd_t *pmd,
					  unsigned int flags);
extern int zap_huge_pmd(struct mmu_gather *tlb,
			struct vm_area_struct *vma,
			pmd_t *pmd);
extern int mincore_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
			unsigned long addr, unsigned long end,
			unsigned char *vec);
extern int change_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
			unsigned long addr, pgprot_t newprot);

enum transparent_hugepage_flag {
	TRANSPARENT_HUGEPAGE_FLAG,
	TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_REQ_MADV_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_KHUGEPAGED_FLAG,
#ifdef CONFIG_DEBUG_VM
	TRANSPARENT_HUGEPAGE_DEBUG_COW_FLAG,
#endif
};

enum page_check_address_pmd_flag {
	PAGE_CHECK_ADDRESS_PMD_FLAG,
	PAGE_CHECK_ADDRESS_PMD_NOTSPLITTING_FLAG,
	PAGE_CHECK_ADDRESS_PMD_SPLITTING_FLAG,
};
extern pmd_t *page_check_address_pmd(struct page *page,
				     struct mm_struct *mm,
				     unsigned long address,
				     enum page_check_address_pmd_flag flag);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#define HPAGE_PMD_SHIFT HPAGE_SHIFT
#define HPAGE_PMD_MASK HPAGE_MASK
#define HPAGE_PMD_SIZE HPAGE_SIZE

#define transparent_hugepage_enabled(__vma)				\
	((transparent_hugepage_flags &					\
	  (1<<TRANSPARENT_HUGEPAGE_FLAG) ||				\
	  (transparent_hugepage_flags &					\
	   (1<<TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG) &&			\
	   ((__vma)->vm_flags & VM_HUGEPAGE))) &&			\
	 !((__vma)->vm_flags & VM_NOHUGEPAGE) &&			\
	 !is_vma_temporary_stack(__vma))
#define transparent_hugepage_defrag(__vma)				\
	((transparent_hugepage_flags &					\
	  (1<<TRANSPARENT_HUGEPAGE_DEFRAG_FLAG)) ||			\
	 (transparent_hugepage_flags &					\
	  (1<<TRANSPARENT_HUGEPAGE_DEFRAG_REQ_MADV_FLAG) &&		\
	  (__vma)->vm_flags & VM_HUGEPAGE))
#ifdef CONFIG_DEBUG_VM
#define transparent_hugepage_debug_cow()				\
	(transparent_hugepage_flags &					\
	 (1<<TRANSPARENT_HUGEPAGE_DEBUG_COW_FLAG))
#else /* CONFIG_DEBUG_VM */
#define transparent_hugepage_debug_cow() 0
#endif /* CONFIG_DEBUG_VM */

extern unsigned long transparent_hugepage_flags;
extern int copy_pte_range(struct mm_struct *dst_mm, struct mm_struct *src_mm,
			  pmd_t *dst_pmd, pmd_t *src_pmd,
			  struct vm_area_struct *vma,
			  unsigned long addr, unsigned long end);
extern int handle_pte_fault(struct mm_struct *mm,
			    struct vm_area_struct *vma, unsigned long address,
			    pte_t *pte, pmd_t *pmd, unsigned int flags);
extern int split_huge_page(struct page *page);
extern void __split_huge_page_pmd(struct mm_struct *mm, pmd_t *pmd);
#define split_huge_page_pmd(__mm, __pmd)				\
	do {								\
		pmd_t *____pmd = (__pmd);				\
		if (unlikely(pmd_trans_huge(*____pmd)))			\
			__split_huge_page_pmd(__mm, ____pmd);		\
	}  while (0)
#define wait_split_huge_page(__anon_vma, __pmd)				\
	do {								\
		pmd_t *____pmd = (__pmd);				\
		spin_unlock_wait(&(__anon_vma)->root->lock);		\
		/*							\
		 * spin_unlock_wait() is just a loop in C and so the	\
		 * CPU can reorder anything around it.			\
		 */							\
		smp_mb();						\
		BUG_ON(pmd_trans_splitting(*____pmd) ||			\
		       pmd_trans_huge(*____pmd));			\
	} while (0)
#define HPAGE_PMD_ORDER (HPAGE_PMD_SHIFT-PAGE_SHIFT)
#define HPAGE_PMD_NR (1<<HPAGE_PMD_ORDER)
#if HPAGE_PMD_ORDER > MAX_ORDER
#error "hugepages can't be allocated by the buddy allocator"
#endif
extern int hugepage_madvise(struct vm_area_struct *vma,
			    unsigned long *vm_flags, int advice);
extern void __vma_adjust_trans_huge(struct vm_area_struct *vma,
				    unsigned long start,
				    unsigned long end,
				    long adjust_next);
static inline void vma_adjust_trans_huge(struct vm_area_struct *vma,
					 unsigned long start,
					 unsigned long end,
					 long adjust_next)
{
	if (!vma->anon_vma || vma->vm_ops)
		return;
	__vma_adjust_trans_huge(vma, start, end, adjust_next);
}
static inline int hpage_nr_pages(struct page *page)
{
	if (unlikely(PageTransHuge(page)))
		return HPAGE_PMD_NR;
	return 1;
}
static inline struct page *compound_trans_head(struct page *page)
{
	if (PageTail(page)) {
		struct page *head;
		head = page->first_page;
		smp_rmb();
		/*
		 * head may be a dangling pointer.
		 * __split_huge_page_refcount clears PageTail before
		 * overwriting first_page, so if PageTail is still
		 * there it means the head pointer isn't dangling.
		 */
		if (PageTail(page))
			return head;
	}
	return page;
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
#define HPAGE_PMD_SHIFT ({ BUG(); 0; })
#define HPAGE_PMD_MASK ({ BUG(); 0; })
#define HPAGE_PMD_SIZE ({ BUG(); 0; })

#define hpage_nr_pages(x) 1

#define transparent_hugepage_enabled(__vma) 0

#define transparent_hugepage_flags 0UL
static inline int split_huge_page(struct page *page)
{
	return 0;
}
#define split_huge_page_pmd(__mm, __pmd)	\
	do { } while (0)
#define wait_split_huge_page(__anon_vma, __pmd)	\
	do { } while (0)
#define compound_trans_head(page) compound_head(page)
static inline int hugepage_madvise(struct vm_area_struct *vma,
				   unsigned long *vm_flags, int advice)
{
	BUG();
	return 0;
}
static inline void vma_adjust_trans_huge(struct vm_area_struct *vma,
					 unsigned long start,
					 unsigned long end,
					 long adjust_next)
{
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#endif /* _LINUX_HUGE_MM_H */
