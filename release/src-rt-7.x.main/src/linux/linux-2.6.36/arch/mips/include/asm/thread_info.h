/* thread_info.h: MIPS low-level thread information
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__


#ifndef __ASSEMBLY__

#include <asm/processor.h>

/*
 * low level task data that entry.S needs immediate access to
 * - this struct should fit entirely inside of one cache line
 * - this struct shares the supervisor stack pages
 * - if the contents of this structure are changed, the assembly constants
 *   must also be changed
 */
struct thread_info {
	struct task_struct	*task;		/* main task structure */
	struct exec_domain	*exec_domain;	/* execution domain */
	unsigned long		flags;		/* low level flags */
	unsigned long		tp_value;	/* thread pointer */
	__u32			cpu;		/* current CPU */
	int			preempt_count;	/* 0 => preemptable, <0 => BUG */

	mm_segment_t		addr_limit;	/* thread address space:
						   0-0xBFFFFFFF for user-thead
						   0-0xFFFFFFFF for kernel-thread
						*/
	struct restart_block	restart_block;
	struct pt_regs		*regs;
};

/*
 * macros/functions for gaining access to the thread information structure
 */
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= _TIF_FIXADE,		\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.addr_limit	= KERNEL_DS,		\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

/* How to get the thread information struct from C.  */
register struct thread_info *__current_thread_info __asm__("$28");
#define current_thread_info()  __current_thread_info

/* thread information allocation */
#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_32BIT)
#define THREAD_SIZE_ORDER (1)
#endif
#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_64BIT)
#define THREAD_SIZE_ORDER (2)
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define THREAD_SIZE_ORDER (1)
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_32KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define THREAD_SIZE_ORDER (0)
#endif

#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_MASK (THREAD_SIZE - 1UL)

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#ifdef CONFIG_DEBUG_STACK_USAGE
#define alloc_thread_info(tsk) kzalloc(THREAD_SIZE, GFP_KERNEL)
#else
#define alloc_thread_info(tsk) kmalloc(THREAD_SIZE, GFP_KERNEL)
#endif

#define free_thread_info(info) kfree(info)

#endif /* !__ASSEMBLY__ */

#define PREEMPT_ACTIVE		0x10000000

/*
 * thread information flags
 * - these are process state flags that various assembly files may need to
 *   access
 * - pending work-to-be-done flags are in LSW
 * - other flags in MSW
 */
#define TIF_SIGPENDING		1	/* signal pending */
#define TIF_NEED_RESCHED	2	/* rescheduling necessary */
#define TIF_SYSCALL_AUDIT	3	/* syscall auditing active */
#define TIF_SECCOMP		4	/* secure computing */
#define TIF_NOTIFY_RESUME	5	/* callback before returning to user */
#define TIF_RESTORE_SIGMASK	9	/* restore signal mask in do_signal() */
#define TIF_USEDFPU		16	/* FPU was used by this task this quantum (SMP) */
#define TIF_POLLING_NRFLAG	17	/* true if poll_idle() is polling TIF_NEED_RESCHED */
#define TIF_MEMDIE		18	/* is terminating due to OOM killer */
#define TIF_FREEZE		19
#define TIF_FIXADE		20	/* Fix address errors in software */
#define TIF_LOGADE		21	/* Log address errors to syslog */
#define TIF_32BIT_REGS		22	/* also implies 16/32 fprs */
#define TIF_32BIT_ADDR		23	/* 32-bit address space (o32/n32) */
#define TIF_FPUBOUND		24	/* thread bound to FPU-full CPU set */
#define TIF_LOAD_WATCH		25	/* If set, load watch registers */
#define TIF_SYSCALL_TRACE	31	/* syscall trace active */

#ifdef CONFIG_MIPS32_O32
#define TIF_32BIT TIF_32BIT_REGS
#elif defined(CONFIG_MIPS32_N32)
#define TIF_32BIT _TIF_32BIT_ADDR
#endif /* CONFIG_MIPS32_O32 */

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_USEDFPU		(1<<TIF_USEDFPU)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_FREEZE		(1<<TIF_FREEZE)
#define _TIF_FIXADE		(1<<TIF_FIXADE)
#define _TIF_LOGADE		(1<<TIF_LOGADE)
#define _TIF_32BIT_REGS		(1<<TIF_32BIT_REGS)
#define _TIF_32BIT_ADDR		(1<<TIF_32BIT_ADDR)
#define _TIF_FPUBOUND		(1<<TIF_FPUBOUND)
#define _TIF_LOAD_WATCH		(1<<TIF_LOAD_WATCH)

/* work to do on interrupt/exception return */
#define _TIF_WORK_MASK		(0x0000ffef &				\
					~(_TIF_SECCOMP | _TIF_SYSCALL_AUDIT))
/* work to do on any return to u-space */
#define _TIF_ALLWORK_MASK	(0x8000ffff & ~_TIF_SECCOMP)

#endif /* __KERNEL__ */

#endif /* _ASM_THREAD_INFO_H */
