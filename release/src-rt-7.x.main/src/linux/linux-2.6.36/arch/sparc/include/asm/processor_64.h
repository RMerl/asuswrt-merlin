/*
 * include/asm/processor.h
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __ASM_SPARC64_PROCESSOR_H
#define __ASM_SPARC64_PROCESSOR_H

/*
 * Sparc64 implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; __asm__("rd %%pc, %0" : "=r" (pc)); pc; })

#include <asm/asi.h>
#include <asm/pstate.h>
#include <asm/ptrace.h>
#include <asm/page.h>

/* The sparc has no problems with write protection */
#define wp_works_ok 1
#define wp_works_ok__is_a_macro /* for versions in ksyms.c */

#define VA_BITS		44
#ifndef __ASSEMBLY__
#define VPTE_SIZE	(1UL << (VA_BITS - PAGE_SHIFT + 3))
#else
#define VPTE_SIZE	(1 << (VA_BITS - PAGE_SHIFT + 3))
#endif

#define TASK_SIZE_OF(tsk) \
	(test_tsk_thread_flag(tsk,TIF_32BIT) ? \
	 (1UL << 32UL) : ((unsigned long)-VPTE_SIZE))
#define TASK_SIZE	TASK_SIZE_OF(current)
#ifdef __KERNEL__

#define STACK_TOP32	((1UL << 32UL) - PAGE_SIZE)
#define STACK_TOP64	(0x0000080000000000UL - (1UL << 32UL))

#define STACK_TOP	(test_thread_flag(TIF_32BIT) ? \
			 STACK_TOP32 : STACK_TOP64)

#define STACK_TOP_MAX	STACK_TOP64

#endif

#ifndef __ASSEMBLY__

typedef struct {
	unsigned char seg;
} mm_segment_t;

/* The Sparc processor specific thread struct. */
struct thread_struct {
#ifdef CONFIG_DEBUG_SPINLOCK
	/* How many spinlocks held by this thread.
	 * Used with spin lock debugging to catch tasks
	 * sleeping illegally with locks held.
	 */
	int smp_lock_count;
	unsigned int smp_lock_pc;
#else
	int dummy; /* f'in gcc bug... */
#endif
};

#endif /* !(__ASSEMBLY__) */

#ifndef CONFIG_DEBUG_SPINLOCK
#define INIT_THREAD  {			\
	0,				\
}
#else /* CONFIG_DEBUG_SPINLOCK */
#define INIT_THREAD  {					\
/* smp_lock_count, smp_lock_pc, */			\
   0,		   0,					\
}
#endif /* !(CONFIG_DEBUG_SPINLOCK) */

#ifndef __ASSEMBLY__

#include <linux/types.h>

/* Return saved PC of a blocked thread. */
struct task_struct;
extern unsigned long thread_saved_pc(struct task_struct *);

/* On Uniprocessor, even in RMO processes see TSO semantics */
#ifdef CONFIG_SMP
#define TSTATE_INITIAL_MM	TSTATE_TSO
#else
#define TSTATE_INITIAL_MM	TSTATE_RMO
#endif

/* Do necessary setup to start up a newly executed thread. */
#define start_thread(regs, pc, sp) \
do { \
	unsigned long __asi = ASI_PNF; \
	regs->tstate = (regs->tstate & (TSTATE_CWP)) | (TSTATE_INITIAL_MM|TSTATE_IE) | (__asi << 24UL); \
	regs->tpc = ((pc & (~3)) - 4); \
	regs->tnpc = regs->tpc + 4; \
	regs->y = 0; \
	set_thread_wstate(1 << 3); \
	if (current_thread_info()->utraps) { \
		if (*(current_thread_info()->utraps) < 2) \
			kfree(current_thread_info()->utraps); \
		else \
			(*(current_thread_info()->utraps))--; \
		current_thread_info()->utraps = NULL; \
	} \
	__asm__ __volatile__( \
	"stx		%%g0, [%0 + %2 + 0x00]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x08]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x10]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x18]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x20]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x28]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x30]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x38]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x40]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x48]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x50]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x58]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x60]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x68]\n\t" \
	"stx		%1,   [%0 + %2 + 0x70]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x78]\n\t" \
	"wrpr		%%g0, (1 << 3), %%wstate\n\t" \
	: \
	: "r" (regs), "r" (sp - sizeof(struct reg_window) - STACK_BIAS), \
	  "i" ((const unsigned long)(&((struct pt_regs *)0)->u_regs[0]))); \
} while (0)

#define start_thread32(regs, pc, sp) \
do { \
	unsigned long __asi = ASI_PNF; \
	pc &= 0x00000000ffffffffUL; \
	sp &= 0x00000000ffffffffUL; \
	regs->tstate = (regs->tstate & (TSTATE_CWP))|(TSTATE_INITIAL_MM|TSTATE_IE|TSTATE_AM) | (__asi << 24UL); \
	regs->tpc = ((pc & (~3)) - 4); \
	regs->tnpc = regs->tpc + 4; \
	regs->y = 0; \
	set_thread_wstate(2 << 3); \
	if (current_thread_info()->utraps) { \
		if (*(current_thread_info()->utraps) < 2) \
			kfree(current_thread_info()->utraps); \
		else \
			(*(current_thread_info()->utraps))--; \
		current_thread_info()->utraps = NULL; \
	} \
	__asm__ __volatile__( \
	"stx		%%g0, [%0 + %2 + 0x00]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x08]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x10]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x18]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x20]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x28]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x30]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x38]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x40]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x48]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x50]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x58]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x60]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x68]\n\t" \
	"stx		%1,   [%0 + %2 + 0x70]\n\t" \
	"stx		%%g0, [%0 + %2 + 0x78]\n\t" \
	"wrpr		%%g0, (2 << 3), %%wstate\n\t" \
	: \
	: "r" (regs), "r" (sp - sizeof(struct reg_window32)), \
	  "i" ((const unsigned long)(&((struct pt_regs *)0)->u_regs[0]))); \
} while (0)

/* Free all resources held by a thread. */
#define release_thread(tsk)		do { } while (0)

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)	do { } while (0)

extern pid_t kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

extern unsigned long get_wchan(struct task_struct *task);

#define task_pt_regs(tsk) (task_thread_info(tsk)->kregs)
#define KSTK_EIP(tsk)  (task_pt_regs(tsk)->tpc)
#define KSTK_ESP(tsk)  (task_pt_regs(tsk)->u_regs[UREG_FP])

#define cpu_relax()	barrier()

/* Prefetch support.  This is tuned for UltraSPARC-III and later.
 * UltraSPARC-I will treat these as nops, and UltraSPARC-II has
 * a shallower prefetch queue than later chips.
 */
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

static inline void prefetch(const void *x)
{
	/* We do not use the read prefetch mnemonic because that
	 * prefetches into the prefetch-cache which only is accessible
	 * by floating point operations in UltraSPARC-III and later.
	 * By contrast, "#one_write" prefetches into the L2 cache
	 * in shared state.
	 */
	__asm__ __volatile__("prefetch [%0], #one_write"
			     : /* no outputs */
			     : "r" (x));
}

static inline void prefetchw(const void *x)
{
	/* The most optimal prefetch to use for writes is
	 * "#n_writes".  This brings the cacheline into the
	 * L2 cache in "owned" state.
	 */
	__asm__ __volatile__("prefetch [%0], #n_writes"
			     : /* no outputs */
			     : "r" (x));
}

#define spin_lock_prefetch(x)	prefetchw(x)

#define HAVE_ARCH_PICK_MMAP_LAYOUT

#endif /* !(__ASSEMBLY__) */

#endif /* !(__ASM_SPARC64_PROCESSOR_H) */
