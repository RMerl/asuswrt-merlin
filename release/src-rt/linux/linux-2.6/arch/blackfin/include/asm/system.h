/*
 * Copyright 2004-2009 Analog Devices Inc.
 *               Tony Kou (tonyko@lineo.ca)
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _BLACKFIN_SYSTEM_H
#define _BLACKFIN_SYSTEM_H

#include <linux/linkage.h>
#include <linux/irqflags.h>
#include <mach/anomaly.h>
#include <asm/cache.h>
#include <asm/pda.h>
#include <asm/irq.h>

/*
 * Force strict CPU ordering.
 */
#define nop()  __asm__ __volatile__ ("nop;\n\t" : : )
#define smp_mb()  mb()
#define smp_rmb() rmb()
#define smp_wmb() wmb()
#define set_mb(var, value) do { var = value; mb(); } while (0)
#define smp_read_barrier_depends()	read_barrier_depends()

#ifdef CONFIG_SMP
asmlinkage unsigned long __raw_xchg_1_asm(volatile void *ptr, unsigned long value);
asmlinkage unsigned long __raw_xchg_2_asm(volatile void *ptr, unsigned long value);
asmlinkage unsigned long __raw_xchg_4_asm(volatile void *ptr, unsigned long value);
asmlinkage unsigned long __raw_cmpxchg_1_asm(volatile void *ptr,
					unsigned long new, unsigned long old);
asmlinkage unsigned long __raw_cmpxchg_2_asm(volatile void *ptr,
					unsigned long new, unsigned long old);
asmlinkage unsigned long __raw_cmpxchg_4_asm(volatile void *ptr,
					unsigned long new, unsigned long old);

#ifdef __ARCH_SYNC_CORE_DCACHE
/* Force Core data cache coherence */
# define mb()	do { barrier(); smp_check_barrier(); smp_mark_barrier(); } while (0)
# define rmb()	do { barrier(); smp_check_barrier(); } while (0)
# define wmb()	do { barrier(); smp_mark_barrier(); } while (0)
# define read_barrier_depends()	do { barrier(); smp_check_barrier(); } while (0)
#else
# define mb()	barrier()
# define rmb()	barrier()
# define wmb()	barrier()
# define read_barrier_depends()	do { } while (0)
#endif

static inline unsigned long __xchg(unsigned long x, volatile void *ptr,
				   int size)
{
	unsigned long tmp;

	switch (size) {
	case 1:
		tmp = __raw_xchg_1_asm(ptr, x);
		break;
	case 2:
		tmp = __raw_xchg_2_asm(ptr, x);
		break;
	case 4:
		tmp = __raw_xchg_4_asm(ptr, x);
		break;
	}

	return tmp;
}

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long tmp;

	switch (size) {
	case 1:
		tmp = __raw_cmpxchg_1_asm(ptr, new, old);
		break;
	case 2:
		tmp = __raw_cmpxchg_2_asm(ptr, new, old);
		break;
	case 4:
		tmp = __raw_cmpxchg_4_asm(ptr, new, old);
		break;
	}

	return tmp;
}
#define cmpxchg(ptr, o, n) \
	((__typeof__(*(ptr)))__cmpxchg((ptr), (unsigned long)(o), \
		(unsigned long)(n), sizeof(*(ptr))))

#else /* !CONFIG_SMP */

#define mb()	barrier()
#define rmb()	barrier()
#define wmb()	barrier()
#define read_barrier_depends()	do { } while (0)

struct __xchg_dummy {
	unsigned long a[100];
};
#define __xg(x) ((volatile struct __xchg_dummy *)(x))

#include <mach/blackfin.h>

static inline unsigned long __xchg(unsigned long x, volatile void *ptr,
				   int size)
{
	unsigned long tmp = 0;
	unsigned long flags;

	flags = hard_local_irq_save();

	switch (size) {
	case 1:
		__asm__ __volatile__
			("%0 = b%2 (z);\n\t"
			 "b%2 = %1;\n\t"
			 : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	case 2:
		__asm__ __volatile__
			("%0 = w%2 (z);\n\t"
			 "w%2 = %1;\n\t"
			 : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	case 4:
		__asm__ __volatile__
			("%0 = %2;\n\t"
			 "%2 = %1;\n\t"
			 : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	}
	hard_local_irq_restore(flags);
	return tmp;
}

#include <asm-generic/cmpxchg-local.h>

/*
 * cmpxchg_local and cmpxchg64_local are atomic wrt current CPU. Always make
 * them available.
 */
#define cmpxchg_local(ptr, o, n)				  	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#include <asm-generic/cmpxchg.h>

#endif /* !CONFIG_SMP */

#define xchg(ptr, x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x), (ptr), sizeof(*(ptr))))
#define tas(ptr) ((void)xchg((ptr), 1))

#define prepare_to_switch()     do { } while(0)

/*
 * switch_to(n) should switch tasks to task ptr, first checking that
 * ptr isn't the current task, in which case it does nothing.
 */

#include <asm/l1layout.h>
#include <asm/mem_map.h>

asmlinkage struct task_struct *resume(struct task_struct *prev, struct task_struct *next);

#ifndef CONFIG_SMP
#define switch_to(prev,next,last) \
do {    \
	memcpy (&task_thread_info(prev)->l1_task_info, L1_SCRATCH_TASK_INFO, \
		sizeof *L1_SCRATCH_TASK_INFO); \
	memcpy (L1_SCRATCH_TASK_INFO, &task_thread_info(next)->l1_task_info, \
		sizeof *L1_SCRATCH_TASK_INFO); \
	(last) = resume (prev, next);   \
} while (0)
#else
#define switch_to(prev, next, last) \
do {    \
	(last) = resume(prev, next);   \
} while (0)
#endif

#endif	/* _BLACKFIN_SYSTEM_H */
