/*
 *  MMU context allocation for 64-bit kernels.
 *
 *  Copyright (C) 2004 Anton Blanchard, IBM Corp. <anton@samba.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/idr.h>

#include <asm/mmu_context.h>

static DEFINE_SPINLOCK(mmu_context_lock);
static DEFINE_IDR(mmu_context_idr);

int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int index;
	int err;
	int new_context = (mm->context.id == 0);

again:
	if (!idr_pre_get(&mmu_context_idr, GFP_KERNEL))
		return -ENOMEM;

	spin_lock(&mmu_context_lock);
	err = idr_get_new_above(&mmu_context_idr, NULL, 1, &index);
	spin_unlock(&mmu_context_lock);

	if (err == -EAGAIN)
		goto again;
	else if (err)
		return err;

	if (index > MAX_CONTEXT) {
		spin_lock(&mmu_context_lock);
		idr_remove(&mmu_context_idr, index);
		spin_unlock(&mmu_context_lock);
		return -ENOMEM;
	}

	mm->context.id = index;
#ifdef CONFIG_PPC_MM_SLICES
	/* The old code would re-promote on fork, we don't do that
	 * when using slices as it could cause problem promoting slices
	 * that have been forced down to 4K
	 */
	if (new_context)
		slice_set_user_psize(mm, mmu_virtual_psize);
#else
	mm->context.user_psize = mmu_virtual_psize;
	mm->context.sllp = SLB_VSID_USER |
		mmu_psize_defs[mmu_virtual_psize].sllp;
#endif

	return 0;
}

void destroy_context(struct mm_struct *mm)
{
	spin_lock(&mmu_context_lock);
	idr_remove(&mmu_context_idr, mm->context.id);
	spin_unlock(&mmu_context_lock);

	mm->context.id = NO_CONTEXT;
}
