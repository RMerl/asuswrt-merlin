/* rwsem.h: R/W semaphores implemented using XADD/CMPXCHG for i486+
 *
 * Written by David Howells (dhowells@redhat.com).
 *
 * Derived from asm-i386/semaphore.h
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

#ifndef _I386_RWSEM_H
#define _I386_RWSEM_H

#ifndef _LINUX_RWSEM_H
#error "please don't include asm/rwsem.h directly, use linux/rwsem.h instead"
#endif

#ifdef __KERNEL__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/lockdep.h>

struct rwsem_waiter;

extern struct rw_semaphore *FASTCALL(rwsem_down_read_failed(struct rw_semaphore *sem));
extern struct rw_semaphore *FASTCALL(rwsem_down_write_failed(struct rw_semaphore *sem));
extern struct rw_semaphore *FASTCALL(rwsem_wake(struct rw_semaphore *));
extern struct rw_semaphore *FASTCALL(rwsem_downgrade_wake(struct rw_semaphore *sem));

/*
 * the semaphore definition
 */
struct rw_semaphore {
	signed long		count;
#define RWSEM_UNLOCKED_VALUE		0x00000000
#define RWSEM_ACTIVE_BIAS		0x00000001
#define RWSEM_ACTIVE_MASK		0x0000ffff
#define RWSEM_WAITING_BIAS		(-0x00010000)
#define RWSEM_ACTIVE_READ_BIAS		RWSEM_ACTIVE_BIAS
#define RWSEM_ACTIVE_WRITE_BIAS		(RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)
	spinlock_t		wait_lock;
	struct list_head	wait_list;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif
};

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define __RWSEM_DEP_MAP_INIT(lockname) , .dep_map = { .name = #lockname }
#else
# define __RWSEM_DEP_MAP_INIT(lockname)
#endif


#define __RWSEM_INITIALIZER(name) \
{ RWSEM_UNLOCKED_VALUE, __SPIN_LOCK_UNLOCKED((name).wait_lock), \
  LIST_HEAD_INIT((name).wait_list) __RWSEM_DEP_MAP_INIT(name) }

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

extern void __init_rwsem(struct rw_semaphore *sem, const char *name,
			 struct lock_class_key *key);

#define init_rwsem(sem)						\
do {								\
	static struct lock_class_key __key;			\
								\
	__init_rwsem((sem), #sem, &__key);			\
} while (0)

/*
 * lock for reading
 */
static inline void __down_read(struct rw_semaphore *sem)
{
	__asm__ __volatile__(
		"# beginning down_read\n\t"
LOCK_PREFIX	"  incl      (%%eax)\n\t" /* adds 0x00000001, returns the old value */
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
	__s32 result, tmp;
	__asm__ __volatile__(
		"# beginning __down_read_trylock\n\t"
		"  movl      %0,%1\n\t"
		"1:\n\t"
		"  movl	     %1,%2\n\t"
		"  addl      %3,%2\n\t"
		"  jle	     2f\n\t"
LOCK_PREFIX	"  cmpxchgl  %2,%0\n\t"
		"  jnz	     1b\n\t"
		"2:\n\t"
		"# ending __down_read_trylock\n\t"
		: "+m" (sem->count), "=&a" (result), "=&r" (tmp)
		: "i" (RWSEM_ACTIVE_READ_BIAS)
		: "memory", "cc");
	return result>=0 ? 1 : 0;
}

/*
 * lock for writing
 */
static inline void __down_write_nested(struct rw_semaphore *sem, int subclass)
{
	int tmp;

	tmp = RWSEM_ACTIVE_WRITE_BIAS;
	__asm__ __volatile__(
		"# beginning down_write\n\t"
LOCK_PREFIX	"  xadd      %%edx,(%%eax)\n\t" /* subtract 0x0000ffff, returns the old value */
		"  testl     %%edx,%%edx\n\t" /* was the count 0 before? */
		"  jz        1f\n"
		"  call call_rwsem_down_write_failed\n"
		"1:\n"
		"# ending down_write"
		: "+m" (sem->count), "=d" (tmp)
		: "a" (sem), "1" (tmp)
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
	signed long ret = cmpxchg(&sem->count,
				  RWSEM_UNLOCKED_VALUE, 
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
	__s32 tmp = -RWSEM_ACTIVE_READ_BIAS;
	__asm__ __volatile__(
		"# beginning __up_read\n\t"
LOCK_PREFIX	"  xadd      %%edx,(%%eax)\n\t" /* subtracts 1, returns the old value */
		"  jns        1f\n\t"
		"  call call_rwsem_wake\n"
		"1:\n"
		"# ending __up_read\n"
		: "+m" (sem->count), "=d" (tmp)
		: "a" (sem), "1" (tmp)
		: "memory", "cc");
}

/*
 * unlock after writing
 */
static inline void __up_write(struct rw_semaphore *sem)
{
	__asm__ __volatile__(
		"# beginning __up_write\n\t"
		"  movl      %2,%%edx\n\t"
LOCK_PREFIX	"  xaddl     %%edx,(%%eax)\n\t" /* tries to transition 0xffff0001 -> 0x00000000 */
		"  jz       1f\n"
		"  call call_rwsem_wake\n"
		"1:\n\t"
		"# ending __up_write\n"
		: "+m" (sem->count)
		: "a" (sem), "i" (-RWSEM_ACTIVE_WRITE_BIAS)
		: "memory", "cc", "edx");
}

/*
 * downgrade write lock to read lock
 */
static inline void __downgrade_write(struct rw_semaphore *sem)
{
	__asm__ __volatile__(
		"# beginning __downgrade_write\n\t"
LOCK_PREFIX	"  addl      %2,(%%eax)\n\t" /* transitions 0xZZZZ0001 -> 0xYYYY0001 */
		"  jns       1f\n\t"
		"  call call_rwsem_downgrade_wake\n"
		"1:\n\t"
		"# ending __downgrade_write\n"
		: "+m" (sem->count)
		: "a" (sem), "i" (-RWSEM_WAITING_BIAS)
		: "memory", "cc");
}

/*
 * implement atomic add functionality
 */
static inline void rwsem_atomic_add(int delta, struct rw_semaphore *sem)
{
	__asm__ __volatile__(
LOCK_PREFIX	"addl %1,%0"
		: "+m" (sem->count)
		: "ir" (delta));
}

/*
 * implement exchange and add functionality
 */
static inline int rwsem_atomic_update(int delta, struct rw_semaphore *sem)
{
	int tmp = delta;

	__asm__ __volatile__(
LOCK_PREFIX	"xadd %0,%1"
		: "+r" (tmp), "+m" (sem->count)
		: : "memory");

	return tmp+delta;
}

static inline int rwsem_is_locked(struct rw_semaphore *sem)
{
	return (sem->count != 0);
}

#endif /* __KERNEL__ */
#endif /* _I386_RWSEM_H */
