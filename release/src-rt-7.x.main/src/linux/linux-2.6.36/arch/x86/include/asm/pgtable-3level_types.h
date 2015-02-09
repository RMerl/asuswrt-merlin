#ifndef _ASM_X86_PGTABLE_3LEVEL_DEFS_H
#define _ASM_X86_PGTABLE_3LEVEL_DEFS_H

#ifndef __ASSEMBLY__
#include <linux/types.h>

typedef u64	pteval_t;
typedef u64	pmdval_t;
typedef u64	pudval_t;
typedef u64	pgdval_t;
typedef u64	pgprotval_t;

typedef union {
	struct {
		unsigned long pte_low, pte_high;
	};
	pteval_t pte;
} pte_t;
#endif	/* !__ASSEMBLY__ */

#ifdef CONFIG_PARAVIRT
#define SHARED_KERNEL_PMD	(pv_info.shared_kernel_pmd)
#else
#define SHARED_KERNEL_PMD	1
#endif

#define PAGETABLE_LEVELS	3

/*
 * PGDIR_SHIFT determines what a top-level page table entry can map
 */
#define PGDIR_SHIFT	30
#define PTRS_PER_PGD	4

/*
 * PMD_SHIFT determines the size of the area a middle-level
 * page table can map
 */
#define PMD_SHIFT	21
#define PTRS_PER_PMD	512

/*
 * entries per page directory level
 */
#define PTRS_PER_PTE	512


#endif /* _ASM_X86_PGTABLE_3LEVEL_DEFS_H */
