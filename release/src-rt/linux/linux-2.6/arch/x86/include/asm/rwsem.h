/* rwsem.h: R/W semaphores implemented using XADD/CMPXCHG for i486+
 *
 * Written by David Howells (dhowells@redhat.com).
 *
 * Derived from asm-x86/semaphore.h
 *
 *
 * The MSW of the count is the negated number of active writers and waiting
 * lockers, and the LSW is the total number of active locks
 *
 * The lock count is initialized to 0 (no active and no waiting lockers).
 *
 * When a writer subtracts WRITE_BIAS, it'll get 0xffff0001 for the case of an
 * uncontended lock. This can be determined because XADD returns the old value.
 * Readers increment by 1 and see a positive value when uncontended, negative
 * if there are writers (and maybe) readers waiting (in which case it goes to
 * sleep).
 *
 * The value of WAITING_BIAS supports up to 32766 waiting processes. This can
 * be extended to 65534 by manually checking the whole MSW rather than relying
 * on the S flag.
 *
 * The value of ACTIVE_BIAS supports up to 65535 active processes.
 *
 * This should be totally fair - if anything is waiting, a process that wants a
 * lock will go to the back of the queue. When the currently active lock is
 * released, if there's a writer at the front of the queue, then that and only
 * that will be woken up; if there's a bunch of consequtive readers at the
 * front, then they'll all be woken up, but no other readers will be.
 */

#ifndef _ASM_X86_RWSEM_H
#define _ASM_X86_RWSEM_H

#ifndef _LINUX_RWSEM_H
#error "please don't include asm/rwsem.h directly, use linux/rwsem.h instead"
#endif

#ifdef __KERNEL__
#include <asm/asm.h>

/*
 * The bias values and the counter type limits the number of
 * potential readers/writers to 32767 for 32 bits and 2147483647
 * for 64 bits.
 */

#ifdef CONFIG_X86_64
# define RWSEM_ACTIVE_MASK		0xffffffffL
#else
# define RWSEM_ACTIVE_MASK		0x0000ffffL
#endif

#define RWSEM_UNLOCKED_VALUE		0x00000000L
#define RWSEM_ACTIVE_BIAS		0x00000001L
#define RWSEM_WAITING_BIAS		(-RWSEM_ACTIVE_MASK-1)
#define RWSEM_ACTIVE_READ_BIAS		RWSEM_ACTIVE_BIAS
#define RWSEM_ACTIVE_WRITE_BIAS		(RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)

/*
 * lock for reading
 */
static inline void __down_read(struct rw_semaphore *sem)
{
	asm volatile("# beginning down_read\n\t"
		     LOCK_PREFIX _ASM_INC "(%1)\n\t"
		     /* adds 0x00000001 */
		     "  jns        1f\n"
		     "  call call_rwsem_down_read_failed\n"
		     "1:\n\t"
		     "# ending down_read\n\t"
		     : "+m" (sem->count)
		     : "a" (sem)
		     : "memory", "cc");
}

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
static inline int __down_read_trylock(struct rw_semaphore *sem)
{
	long result, tmp;
	asm volatile("# beginning __down_read_trylock\n\t"
		     "  mov          %0,%1\n\t"
		     "1:\n\t"
		     "  mov          %1,%2\n\t"
		     "  add          %3,%2\n\t"
		     "  jle	     2f\n\t"
		     LOCK_PREFIX "  cmpxchg  %2,%0\n\t"
		     "  jnz	     1b\n\t"
		     "2:\n\t"
		     "# ending __down_read_trylock\n\t"
		     : "+m" (sem->count), "=&a" (result), "=&r" (tmp)
		     : "i" (RWSEM_ACTIVE_READ_BIAS)
		     : "memory", "cc");
	return result >= 0 ? 1 : 0;
}

/*
 * lock for writing
 */
static inline void __down_write_nested(struct rw_semaphore *sem, int subclass)
{
	long tmp;
	asm volatile("# beginning down_write\n\t"
		     LOCK_PREFIX "  xadd      %1,(%2)\n\t"
		     /* adds 0xffff0001, returns the old value */
		     "  test      %1,%1\n\t"
		     /* was the count 0 before? */
		     "  jz        1f\n"
		     "  call call_rwsem_down_write_failed\n"
		     "1:\n"
		     "# ending down_write"
		     : "+m" (sem->count), "=d" (tmp)
		     : "a" (sem), "1" (RWSEM_ACTIVE_WRITE_BIAS)
		     : "memory", "cc");
}

static inline void __down_write(struct rw_semaphore *sem)
{
	__down_write_nested(sem, 0);
}

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
static inline int __down_write_trylock(struct rw_semaphore *sem)
{
	long ret = cmpxchg(&sem->count, RWSEM_UNLOCKED_VALUE,
			   RWSEM_ACTIVE_WRITE_BIAS);
	if (ret == RWSEM_UNLOCKED_VALUE)
		return 1;
	return 0;
}

/*
 * unlock after reading
 */
static inline void __up_read(struct rw_semaphore *sem)
{
	long tmp;
	asm volatile("# beginning __up_read\n\t"
		     LOCK_PREFIX "  xadd      %1,(%2)\n\t"
		     /* subtracts 1, returns the old value */
		     "  jns        1f\n\t"
		     "  call call_rwsem_wake\n" /* expects old value in %edx */
		     "1:\n"
		     "# ending __up_read\n"
		     : "+m" (sem->count), "=d" (tmp)
		     : "a" (sem), "1" (-RWSEM_ACTIVE_READ_BIAS)
		     : "memory", "cc");
}

/*
 * unlock after writing
 */
static inline void __up_write(struct rw_semaphore *sem)
{
	long tmp;
	asm volatile("# beginning __up_write\n\t"
		     LOCK_PREFIX "  xadd      %1,(%2)\n\t"
		     /* subtracts 0xffff0001, returns the old value */
		     "  jns        1f\n\t"
		     "  call call_rwsem_wake\n" /* expects old value in %edx */
		     "1:\n\t"
		     "# ending __up_write\n"
		     : "+m" (sem->count), "=d" (tmp)
		     : "a" (sem), "1" (-RWSEM_ACTIVE_WRITE_BIAS)
		     : "memory", "cc");
}

/*
 * downgrade write lock to read lock
 */
static inline void __downgrade_write(struct rw_semaphore *sem)
{
	asm volatile("# beginning __downgrade_write\n\t"
		     LOCK_PREFIX _ASM_ADD "%2,(%1)\n\t"
		     /*
		      * transitions 0xZZZZ0001 -> 0xYYYY0001 (i386)
		      *     0xZZZZZZZZ00000001 -> 0xYYYYYYYY00000001 (x86_64)
		      */
		     "  jns       1f\n\t"
		     "  call call_rwsem_downgrade_wake\n"
		     "1:\n\t"
		     "# ending __downgrade_write\n"
		     : "+m" (sem->count)
		     : "a" (sem), "er" (-RWSEM_WAITING_BIAS)
		     : "memory", "cc");
}

/*
 * implement atomic add functionality
 */
static inline void rwsem_atomic_add(long delta, struct rw_semaphore *sem)
{
	asm volatile(LOCK_PREFIX _ASM_ADD "%1,%0"
		     : "+m" (sem->count)
		     : "er" (delta));
}

/*
 * implement exchange and add functionality
 */
static inline long rwsem_atomic_update(long delta, struct rw_semaphore *sem)
{
	long tmp = delta;

	asm volatile(LOCK_PREFIX "xadd %0,%1"
		     : "+r" (tmp), "+m" (sem->count)
		     : : "memory");

	return tmp + delta;
}

#endif /* __KERNEL__ */
#endif /* _ASM_X86_RWSEM_H */
