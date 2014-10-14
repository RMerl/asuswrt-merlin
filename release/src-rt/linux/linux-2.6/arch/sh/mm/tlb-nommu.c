/*
 * arch/sh/mm/tlb-nommu.c
 *
 * TLB Operations for MMUless SH.
 *
 * Copyright (C) 2002 Paul Mundt
 *
 * Released under the terms of the GNU GPL v2.0.
 */
#include <linux/kernel.h>
#include <linux/mm.h>

/*
 * Nothing too terribly exciting here ..
 */
void local_flush_tlb_all(void)
{
	BUG();
}

void local_flush_tlb_mm(struct mm_struct *mm)
{
	BUG();
}

void local_flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
			    unsigned long end)
{
	BUG();
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	BUG();
}

void local_flush_tlb_one(unsigned long asid, unsigned long page)
{
	BUG();
}

void local_flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
	BUG();
}

void update_mmu_cache(struct vm_area_struct * vma,
		      unsigned long address, pte_t pte)
{
	BUG();
}
