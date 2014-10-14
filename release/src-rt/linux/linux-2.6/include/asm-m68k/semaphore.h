#ifndef _M68K_SEMAPHORE_H
#define _M68K_SEMAPHORE_H

#define RW_LOCK_BIAS		 0x01000000

#ifndef __ASSEMBLY__

#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/stringify.h>

#include <asm/system.h>
#include <asm/atomic.h>

/*
 * Interrupt-safe semaphores..
 *
 * (C) Copyright 1996 Linus Torvalds
 *
 * m68k version by Andreas Schwab
 */


struct semaphore {
	atomic_t count;
	atomic_t waking;
	wait_queue_head_t wait;
};

#define __SEMAPHORE_INITIALIZER(name, n)				\
{									\
	.count		= ATOMIC_INIT(n),				\
	.waking		= ATOMIC_INIT(0),				\
	.wait		= __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
}

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init(struct semaphore *sem, int val)
{
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER(*sem, val);
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

asmlinkage void __down_failed(void /* special register calling convention */);
asmlinkage int  __down_failed_interruptible(void  /* params in registers */);
asmlinkage int  __down_failed_trylock(void  /* params in registers */);
asmlinkage void __up_wakeup(void /* special register calling convention */);

asmlinkage void __down(struct semaphore * sem);
asmlinkage int  __down_interruptible(struct semaphore * sem);
asmlinkage int  __down_trylock(struct semaphore * sem);
asmlinkage void __up(struct semaphore * sem);

/*
 * This is ugly, but we want the default case to fall through.
 * "down_failed" is a special asm handler that calls the C
 * routine that actually waits. See arch/m68k/lib/semaphore.S
 */
static inline void down(struct semaphore *sem)
{
	register struct semaphore *sem1 __asm__ ("%a1") = sem;

	might_sleep();
	__asm__ __volatile__(
		"| atomic down operation\n\t"
		"subql #1,%0@\n\t"
		"jmi 2f\n\t"
		"1:\n"
		LOCK_SECTION_START(".even\n\t")
		"2:\tpea 1b\n\t"
		"jbra __down_failed\n"
		LOCK_SECTION_END
		: /* no outputs */
		: "a" (sem1)
		: "memory");
}

static inline int down_interruptible(struct semaphore *sem)
{
	register struct semaphore *sem1 __asm__ ("%a1") = sem;
	register int result __asm__ ("%d0");

	might_sleep();
	__asm__ __volatile__(
		"| atomic interruptible down operation\n\t"
		"subql #1,%1@\n\t"
		"jmi 2f\n\t"
		"clrl %0\n"
		"1:\n"
		LOCK_SECTION_START(".even\n\t")
		"2:\tpea 1b\n\t"
		"jbra __down_failed_interruptible\n"
		LOCK_SECTION_END
		: "=d" (result)
		: "a" (sem1)
		: "memory");
	return result;
}

static inline int down_trylock(struct semaphore *sem)
{
	register struct semaphore *sem1 __asm__ ("%a1") = sem;
	register int result __asm__ ("%d0");

	__asm__ __volatile__(
		"| atomic down trylock operation\n\t"
		"subql #1,%1@\n\t"
		"jmi 2f\n\t"
		"clrl %0\n"
		"1:\n"
		LOCK_SECTION_START(".even\n\t")
		"2:\tpea 1b\n\t"
		"jbra __down_failed_trylock\n"
		LOCK_SECTION_END
		: "=d" (result)
		: "a" (sem1)
		: "memory");
	return result;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
static inline void up(struct semaphore *sem)
{
	register struct semaphore *sem1 __asm__ ("%a1") = sem;

	__asm__ __volatile__(
		"| atomic up operation\n\t"
		"addql #1,%0@\n\t"
		"jle 2f\n"
		"1:\n"
		LOCK_SECTION_START(".even\n\t")
		"2:\t"
		"pea 1b\n\t"
		"jbra __up_wakeup\n"
		LOCK_SECTION_END
		: /* no outputs */
		: "a" (sem1)
		: "memory");
}

#endif /* __ASSEMBLY__ */

#endif
