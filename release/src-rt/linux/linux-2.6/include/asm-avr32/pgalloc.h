/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_PGALLOC_H
#define __ASM_AVR32_PGALLOC_H

#include <asm/processor.h>
#include <linux/threads.h>
#include <linux/slab.h>
#include <linux/mm.h>

#define pmd_populate_kernel(mm, pmd, pte) \
	set_pmd(pmd, __pmd(_PAGE_TABLE + __pa(pte)))

static __inline__ void pmd_populate(struct mm_struct *mm, pmd_t *pmd,
				    struct page *pte)
{
	set_pmd(pmd, __pmd(_PAGE_TABLE + page_to_phys(pte)));
}

/*
 * Allocate and free page tables
 */
static __inline__ pgd_t *pgd_alloc(struct mm_struct *mm)
{
	unsigned int pgd_size = (USER_PTRS_PER_PGD * sizeof(pgd_t));
	pgd_t *pgd = kmalloc(pgd_size, GFP_KERNEL);

	if (pgd)
		memset(pgd, 0, pgd_size);

	return pgd;
}

static inline void pgd_free(pgd_t *pgd)
{
	kfree(pgd);
}

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm,
					  unsigned long address)
{
	int count = 0;
	pte_t *pte;

	do {
		pte = (pte_t *) __get_free_page(GFP_KERNEL | __GFP_REPEAT);
		if (pte)
			clear_page(pte);
		else {
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(HZ);
		}
	} while (!pte && (count++ < 10));

	return pte;
}

static inline struct page *pte_alloc_one(struct mm_struct *mm,
					 unsigned long address)
{
	int count = 0;
	struct page *pte;

	do {
		pte = alloc_pages(GFP_KERNEL, 0);
		if (pte)
			clear_page(page_address(pte));
		else {
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(HZ);
		}
	} while (!pte && (count++ < 10));

	return pte;
}

static inline void pte_free_kernel(pte_t *pte)
{
	free_page((unsigned long)pte);
}

static inline void pte_free(struct page *pte)
{
	__free_page(pte);
}

#define __pte_free_tlb(tlb,pte) tlb_remove_page((tlb),(pte))

#define check_pgt_cache() do { } while(0)

#endif /* __ASM_AVR32_PGALLOC_H */
