/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __UM_THREAD_INFO_H
#define __UM_THREAD_INFO_H

#ifndef __ASSEMBLY__

#include <asm/types.h>
#include <asm/page.h>
#include <asm/uaccess.h>

struct thread_info {
	struct task_struct	*task;		/* main task structure */
	struct exec_domain	*exec_domain;	/* execution domain */
	unsigned long		flags;		/* low level flags */
	__u32			cpu;		/* current CPU */
	int			preempt_count;  /* 0 => preemptable,
						   <0 => BUG */
	mm_segment_t		addr_limit;	/* thread address space:
					 	   0-0xBFFFFFFF for user
						   0-0xFFFFFFFF for kernel */
	struct restart_block    restart_block;
	struct thread_info	*real_thread;    /* Points to non-IRQ stack */
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task =		&tsk,			\
	.exec_domain =	&default_exec_domain,	\
	.flags =		0,		\
	.cpu =		0,			\
	.preempt_count = INIT_PREEMPT_COUNT,	\
	.addr_limit =	KERNEL_DS,		\
	.restart_block =  {			\
		.fn =  do_no_restart_syscall,	\
	},					\
	.real_thread = NULL,			\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

#define THREAD_SIZE ((1 << CONFIG_KERNEL_STACK_ORDER) * PAGE_SIZE)
/* how to get the thread information struct from C */
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	unsigned long mask = THREAD_SIZE - 1;
	ti = (struct thread_info *) (((unsigned long) &ti) & ~mask);
	return ti;
}

#define THREAD_SIZE_ORDER CONFIG_KERNEL_STACK_ORDER

#endif

#define PREEMPT_ACTIVE		0x10000000

#define TIF_SYSCALL_TRACE	0	/* syscall trace active */
#define TIF_SIGPENDING		1	/* signal pending */
#define TIF_NEED_RESCHED	2	/* rescheduling necessary */
#define TIF_POLLING_NRFLAG      3       /* true if poll_idle() is polling
					 * TIF_NEED_RESCHED */
#define TIF_RESTART_BLOCK	4
#define TIF_MEMDIE		5	/* is terminating due to OOM killer */
#define TIF_SYSCALL_AUDIT	6
#define TIF_RESTORE_SIGMASK	7
#define TIF_FREEZE		16	/* is freezing for suspend */

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG     (1 << TIF_POLLING_NRFLAG)
#define _TIF_MEMDIE		(1 << TIF_MEMDIE)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#define _TIF_FREEZE		(1 << TIF_FREEZE)

#endif
