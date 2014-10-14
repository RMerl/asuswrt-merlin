/*
 * Access to user system call parameters and results
 *
 * Copyright (C) 2008 Intel Corp.  Shaohua Li <shaohua.li@intel.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * See asm-generic/syscall.h for descriptions of what we must do here.
 */

#ifndef _ASM_SYSCALL_H
#define _ASM_SYSCALL_H	1

#include <linux/sched.h>
#include <linux/err.h>

static inline long syscall_get_nr(struct task_struct *task,
				  struct pt_regs *regs)
{
	if ((long)regs->cr_ifs < 0) /* Not a syscall */
		return -1;

	return regs->r15;
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
	/* do nothing */
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	return regs->r10 == -1 ? regs->r8:0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->r8;
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	if (error) {
		/* error < 0, but ia64 uses > 0 return value */
		regs->r8 = -error;
		regs->r10 = -1;
	} else {
		regs->r8 = val;
		regs->r10 = 0;
	}
}

extern void ia64_syscall_get_set_arguments(struct task_struct *task,
	struct pt_regs *regs, unsigned int i, unsigned int n,
	unsigned long *args, int rw);
static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 unsigned long *args)
{
	BUG_ON(i + n > 6);

	ia64_syscall_get_set_arguments(task, regs, i, n, args, 0);
}

static inline void syscall_set_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 unsigned long *args)
{
	BUG_ON(i + n > 6);

	ia64_syscall_get_set_arguments(task, regs, i, n, args, 1);
}
#endif	/* _ASM_SYSCALL_H */
