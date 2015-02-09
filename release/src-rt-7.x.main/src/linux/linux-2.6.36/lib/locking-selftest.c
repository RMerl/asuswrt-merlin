/*
 * lib/locking-selftest.c
 *
 * Testsuite for various locking APIs: spinlocks, rwlocks,
 * mutexes and rw-semaphores.
 *
 * It is checking both false positives and false negatives.
 *
 * Started by Ingo Molnar:
 *
 *  Copyright (C) 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/lockdep.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/irqflags.h>

/*
 * Change this to 1 if you want to see the failure printouts:
 */
static unsigned int debug_locks_verbose;

static int __init setup_debug_locks_verbose(char *str)
{
	get_option(&str, &debug_locks_verbose);

	return 1;
}

__setup("debug_locks_verbose=", setup_debug_locks_verbose);

#define FAILURE		0
#define SUCCESS		1

#define LOCKTYPE_SPIN	0x1
#define LOCKTYPE_RWLOCK	0x2
#define LOCKTYPE_MUTEX	0x4
#define LOCKTYPE_RWSEM	0x8

/*
 * Normal standalone locks, for the circular and irq-context
 * dependency tests:
 */
static DEFINE_SPINLOCK(lock_A);
static DEFINE_SPINLOCK(lock_B);
static DEFINE_SPINLOCK(lock_C);
static DEFINE_SPINLOCK(lock_D);

static DEFINE_RWLOCK(rwlock_A);
static DEFINE_RWLOCK(rwlock_B);
static DEFINE_RWLOCK(rwlock_C);
static DEFINE_RWLOCK(rwlock_D);

static DEFINE_MUTEX(mutex_A);
static DEFINE_MUTEX(mutex_B);
static DEFINE_MUTEX(mutex_C);
static DEFINE_MUTEX(mutex_D);

static DECLARE_RWSEM(rwsem_A);
static DECLARE_RWSEM(rwsem_B);
static DECLARE_RWSEM(rwsem_C);
static DECLARE_RWSEM(rwsem_D);

/*
 * Locks that we initialize dynamically as well so that
 * e.g. X1 and X2 becomes two instances of the same class,
 * but X* and Y* are different classes. We do this so that
 * we do not trigger a real lockup:
 */
static DEFINE_SPINLOCK(lock_X1);
static DEFINE_SPINLOCK(lock_X2);
static DEFINE_SPINLOCK(lock_Y1);
static DEFINE_SPINLOCK(lock_Y2);
static DEFINE_SPINLOCK(lock_Z1);
static DEFINE_SPINLOCK(lock_Z2);

static DEFINE_RWLOCK(rwlock_X1);
static DEFINE_RWLOCK(rwlock_X2);
static DEFINE_RWLOCK(rwlock_Y1);
static DEFINE_RWLOCK(rwlock_Y2);
static DEFINE_RWLOCK(rwlock_Z1);
static DEFINE_RWLOCK(rwlock_Z2);

static DEFINE_MUTEX(mutex_X1);
static DEFINE_MUTEX(mutex_X2);
static DEFINE_MUTEX(mutex_Y1);
static DEFINE_MUTEX(mutex_Y2);
static DEFINE_MUTEX(mutex_Z1);
static DEFINE_MUTEX(mutex_Z2);

static DECLARE_RWSEM(rwsem_X1);
static DECLARE_RWSEM(rwsem_X2);
static DECLARE_RWSEM(rwsem_Y1);
static DECLARE_RWSEM(rwsem_Y2);
static DECLARE_RWSEM(rwsem_Z1);
static DECLARE_RWSEM(rwsem_Z2);

/*
 * non-inlined runtime initializers, to let separate locks share
 * the same lock-class:
 */
#define INIT_CLASS_FUNC(class) 				\
static noinline void					\
init_class_##class(spinlock_t *lock, rwlock_t *rwlock, struct mutex *mutex, \
		 struct rw_semaphore *rwsem)		\
{							\
	spin_lock_init(lock);				\
	rwlock_init(rwlock);				\
	mutex_init(mutex);				\
	init_rwsem(rwsem);				\
}

INIT_CLASS_FUNC(X)
INIT_CLASS_FUNC(Y)
INIT_CLASS_FUNC(Z)

static void init_shared_classes(void)
{
	init_class_X(&lock_X1, &rwlock_X1, &mutex_X1, &rwsem_X1);
	init_class_X(&lock_X2, &rwlock_X2, &mutex_X2, &rwsem_X2);

	init_class_Y(&lock_Y1, &rwlock_Y1, &mutex_Y1, &rwsem_Y1);
	init_class_Y(&lock_Y2, &rwlock_Y2, &mutex_Y2, &rwsem_Y2);

	init_class_Z(&lock_Z1, &rwlock_Z1, &mutex_Z1, &rwsem_Z1);
	init_class_Z(&lock_Z2, &rwlock_Z2, &mutex_Z2, &rwsem_Z2);
}

/*
 * For spinlocks and rwlocks we also do hardirq-safe / softirq-safe tests.
 * The following functions use a lock from a simulated hardirq/softirq
 * context, causing the locks to be marked as hardirq-safe/softirq-safe:
 */

#define HARDIRQ_DISABLE		local_irq_disable
#define HARDIRQ_ENABLE		local_irq_enable

#define HARDIRQ_ENTER()				\
	local_irq_disable();			\
	irq_enter();				\
	WARN_ON(!in_irq());

#define HARDIRQ_EXIT()				\
	__irq_exit();				\
	local_irq_enable();

#define SOFTIRQ_DISABLE		local_bh_disable
#define SOFTIRQ_ENABLE		local_bh_enable

#define SOFTIRQ_ENTER()				\
		local_bh_disable();		\
		local_irq_disable();		\
		lockdep_softirq_enter();	\
		WARN_ON(!in_softirq());

#define SOFTIRQ_EXIT()				\
		lockdep_softirq_exit();		\
		local_irq_enable();		\
		local_bh_enable();

/*
 * Shortcuts for lock/unlock API variants, to keep
 * the testcases compact:
 */
#define L(x)			spin_lock(&lock_##x)
#define U(x)			spin_unlock(&lock_##x)
#define LU(x)			L(x); U(x)
#define SI(x)			spin_lock_init(&lock_##x)

#define WL(x)			write_lock(&rwlock_##x)
#define WU(x)			write_unlock(&rwlock_##x)
#define WLU(x)			WL(x); WU(x)

#define RL(x)			read_lock(&rwlock_##x)
#define RU(x)			read_unlock(&rwlock_##x)
#define RLU(x)			RL(x); RU(x)
#define RWI(x)			rwlock_init(&rwlock_##x)

#define ML(x)			mutex_lock(&mutex_##x)
#define MU(x)			mutex_unlock(&mutex_##x)
#define MI(x)			mutex_init(&mutex_##x)

#define WSL(x)			down_write(&rwsem_##x)
#define WSU(x)			up_write(&rwsem_##x)

#define RSL(x)			down_read(&rwsem_##x)
#define RSU(x)			up_read(&rwsem_##x)
#define RWSI(x)			init_rwsem(&rwsem_##x)

#define LOCK_UNLOCK_2(x,y)	LOCK(x); LOCK(y); UNLOCK(y); UNLOCK(x)

/*
 * Generate different permutations of the same testcase, using
 * the same basic lock-dependency/state events:
 */

#define GENERATE_TESTCASE(name)			\
						\
static void name(void) { E(); }

#define GENERATE_PERMUTATIONS_2_EVENTS(name)	\
						\
static void name##_12(void) { E1(); E2(); }	\
static void name##_21(void) { E2(); E1(); }

#define GENERATE_PERMUTATIONS_3_EVENTS(name)		\
							\
static void name##_123(void) { E1(); E2(); E3(); }	\
static void name##_132(void) { E1(); E3(); E2(); }	\
static void name##_213(void) { E2(); E1(); E3(); }	\
static void name##_231(void) { E2(); E3(); E1(); }	\
static void name##_312(void) { E3(); E1(); E2(); }	\
static void name##_321(void) { E3(); E2(); E1(); }

/*
 * AA deadlock:
 */

#define E()					\
						\
	LOCK(X1);				\
	LOCK(X2); /* this one should fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(AA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(AA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(AA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(AA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(AA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(AA_rsem)

#undef E

/*
 * Special-case for read-locking, they are
 * allowed to recurse on the same lock class:
 */
static void rlock_AA1(void)
{
	RL(X1);
	RL(X1); // this one should NOT fail
}

static void rlock_AA1B(void)
{
	RL(X1);
	RL(X2); // this one should NOT fail
}

static void rsem_AA1(void)
{
	RSL(X1);
	RSL(X1); // this one should fail
}

static void rsem_AA1B(void)
{
	RSL(X1);
	RSL(X2); // this one should fail
}
/*
 * The mixing of read and write locks is not allowed:
 */
static void rlock_AA2(void)
{
	RL(X1);
	WL(X2); // this one should fail
}

static void rsem_AA2(void)
{
	RSL(X1);
	WSL(X2); // this one should fail
}

static void rlock_AA3(void)
{
	WL(X1);
	RL(X2); // this one should fail
}

static void rsem_AA3(void)
{
	WSL(X1);
	RSL(X2); // this one should fail
}

/*
 * ABBA deadlock:
 */

#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(B, A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABBA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABBA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABBA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABBA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABBA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABBA_rsem)

#undef E

/*
 * AB BC CA deadlock:
 */

#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(B, C);			\
	LOCK_UNLOCK_2(C, A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABBCCA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABBCCA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABBCCA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABBCCA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABBCCA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABBCCA_rsem)

#undef E

/*
 * AB CA BC deadlock:
 */

#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(C, A);			\
	LOCK_UNLOCK_2(B, C); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABCABC_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABCABC_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABCABC_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABCABC_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABCABC_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABCABC_rsem)

#undef E

/*
 * AB BC CD DA deadlock:
 */

#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(B, C);			\
	LOCK_UNLOCK_2(C, D);			\
	LOCK_UNLOCK_2(D, A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABBCCDDA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABBCCDDA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABBCCDDA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABBCCDDA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABBCCDDA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABBCCDDA_rsem)

#undef E

/*
 * AB CD BD DA deadlock:
 */
#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(C, D);			\
	LOCK_UNLOCK_2(B, D);			\
	LOCK_UNLOCK_2(D, A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABCDBDDA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABCDBDDA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABCDBDDA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABCDBDDA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABCDBDDA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABCDBDDA_rsem)

#undef E

/*
 * AB CD BC DA deadlock:
 */
#define E()					\
						\
	LOCK_UNLOCK_2(A, B);			\
	LOCK_UNLOCK_2(C, D);			\
	LOCK_UNLOCK_2(B, C);			\
	LOCK_UNLOCK_2(D, A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(ABCDBCDA_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(ABCDBCDA_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(ABCDBCDA_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(ABCDBCDA_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(ABCDBCDA_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(ABCDBCDA_rsem)

#undef E

/*
 * Double unlock:
 */
#define E()					\
						\
	LOCK(A);				\
	UNLOCK(A);				\
	UNLOCK(A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(double_unlock_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(double_unlock_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(double_unlock_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(double_unlock_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(double_unlock_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(double_unlock_rsem)

#undef E

/*
 * Bad unlock ordering:
 */
#define E()					\
						\
	LOCK(A);				\
	LOCK(B);				\
	UNLOCK(A); /* fail */			\
	UNLOCK(B);

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(bad_unlock_order_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(bad_unlock_order_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(bad_unlock_order_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(bad_unlock_order_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(bad_unlock_order_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(bad_unlock_order_rsem)

#undef E

/*
 * initializing a held lock:
 */
#define E()					\
						\
	LOCK(A);				\
	INIT(A); /* fail */

/*
 * 6 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_TESTCASE(init_held_spin)
#include "locking-selftest-wlock.h"
GENERATE_TESTCASE(init_held_wlock)
#include "locking-selftest-rlock.h"
GENERATE_TESTCASE(init_held_rlock)
#include "locking-selftest-mutex.h"
GENERATE_TESTCASE(init_held_mutex)
#include "locking-selftest-wsem.h"
GENERATE_TESTCASE(init_held_wsem)
#include "locking-selftest-rsem.h"
GENERATE_TESTCASE(init_held_rsem)

#undef E

/*
 * locking an irq-safe lock with irqs enabled:
 */
#define E1()				\
					\
	IRQ_ENTER();			\
	LOCK(A);			\
	UNLOCK(A);			\
	IRQ_EXIT();

#define E2()				\
					\
	LOCK(A);			\
	UNLOCK(A);

/*
 * Generate 24 testcases:
 */
#include "locking-selftest-spin-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_hard_spin)

#include "locking-selftest-rlock-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_hard_rlock)

#include "locking-selftest-wlock-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_hard_wlock)

#include "locking-selftest-spin-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_soft_spin)

#include "locking-selftest-rlock-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_soft_rlock)

#include "locking-selftest-wlock-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe1_soft_wlock)

#undef E1
#undef E2

/*
 * Enabling hardirqs with a softirq-safe lock held:
 */
#define E1()				\
					\
	SOFTIRQ_ENTER();		\
	LOCK(A);			\
	UNLOCK(A);			\
	SOFTIRQ_EXIT();

#define E2()				\
					\
	HARDIRQ_DISABLE();		\
	LOCK(A);			\
	HARDIRQ_ENABLE();		\
	UNLOCK(A);

/*
 * Generate 12 testcases:
 */
#include "locking-selftest-spin.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2A_spin)

#include "locking-selftest-wlock.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2A_wlock)

#include "locking-selftest-rlock.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2A_rlock)

#undef E1
#undef E2

/*
 * Enabling irqs with an irq-safe lock held:
 */
#define E1()				\
					\
	IRQ_ENTER();			\
	LOCK(A);			\
	UNLOCK(A);			\
	IRQ_EXIT();

#define E2()				\
					\
	IRQ_DISABLE();			\
	LOCK(A);			\
	IRQ_ENABLE();			\
	UNLOCK(A);

/*
 * Generate 24 testcases:
 */
#include "locking-selftest-spin-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_hard_spin)

#include "locking-selftest-rlock-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_hard_rlock)

#include "locking-selftest-wlock-hardirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_hard_wlock)

#include "locking-selftest-spin-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_soft_spin)

#include "locking-selftest-rlock-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_soft_rlock)

#include "locking-selftest-wlock-softirq.h"
GENERATE_PERMUTATIONS_2_EVENTS(irqsafe2B_soft_wlock)

#undef E1
#undef E2

/*
 * Acquiring a irq-unsafe lock while holding an irq-safe-lock:
 */
#define E1()				\
					\
	LOCK(A);			\
	LOCK(B);			\
	UNLOCK(B);			\
	UNLOCK(A);			\

#define E2()				\
					\
	LOCK(B);			\
	UNLOCK(B);

#define E3()				\
					\
	IRQ_ENTER();			\
	LOCK(A);			\
	UNLOCK(A);			\
	IRQ_EXIT();

/*
 * Generate 36 testcases:
 */
#include "locking-selftest-spin-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_hard_spin)

#include "locking-selftest-rlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_hard_rlock)

#include "locking-selftest-wlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_hard_wlock)

#include "locking-selftest-spin-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_soft_spin)

#include "locking-selftest-rlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_soft_rlock)

#include "locking-selftest-wlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe3_soft_wlock)

#undef E1
#undef E2
#undef E3

/*
 * If a lock turns into softirq-safe, but earlier it took
 * a softirq-unsafe lock:
 */

#define E1()				\
	IRQ_DISABLE();			\
	LOCK(A);			\
	LOCK(B);			\
	UNLOCK(B);			\
	UNLOCK(A);			\
	IRQ_ENABLE();

#define E2()				\
	LOCK(B);			\
	UNLOCK(B);

#define E3()				\
	IRQ_ENTER();			\
	LOCK(A);			\
	UNLOCK(A);			\
	IRQ_EXIT();

/*
 * Generate 36 testcases:
 */
#include "locking-selftest-spin-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_hard_spin)

#include "locking-selftest-rlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_hard_rlock)

#include "locking-selftest-wlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_hard_wlock)

#include "locking-selftest-spin-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_soft_spin)

#include "locking-selftest-rlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_soft_rlock)

#include "locking-selftest-wlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irqsafe4_soft_wlock)

#undef E1
#undef E2
#undef E3

/*
 * read-lock / write-lock irq inversion.
 *
 * Deadlock scenario:
 *
 * CPU#1 is at #1, i.e. it has write-locked A, but has not
 * taken B yet.
 *
 * CPU#2 is at #2, i.e. it has locked B.
 *
 * Hardirq hits CPU#2 at point #2 and is trying to read-lock A.
 *
 * The deadlock occurs because CPU#1 will spin on B, and CPU#2
 * will spin on A.
 */

#define E1()				\
					\
	IRQ_DISABLE();			\
	WL(A);				\
	LOCK(B);			\
	UNLOCK(B);			\
	WU(A);				\
	IRQ_ENABLE();

#define E2()				\
					\
	LOCK(B);			\
	UNLOCK(B);

#define E3()				\
					\
	IRQ_ENTER();			\
	RL(A);				\
	RU(A);				\
	IRQ_EXIT();

/*
 * Generate 36 testcases:
 */
#include "locking-selftest-spin-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_hard_spin)

#include "locking-selftest-rlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_hard_rlock)

#include "locking-selftest-wlock-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_hard_wlock)

#include "locking-selftest-spin-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_soft_spin)

#include "locking-selftest-rlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_soft_rlock)

#include "locking-selftest-wlock-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_inversion_soft_wlock)

#undef E1
#undef E2
#undef E3

/*
 * read-lock / write-lock recursion that is actually safe.
 */

#define E1()				\
					\
	IRQ_DISABLE();			\
	WL(A);				\
	WU(A);				\
	IRQ_ENABLE();

#define E2()				\
					\
	RL(A);				\
	RU(A);				\

#define E3()				\
					\
	IRQ_ENTER();			\
	RL(A);				\
	L(B);				\
	U(B);				\
	RU(A);				\
	IRQ_EXIT();

/*
 * Generate 12 testcases:
 */
#include "locking-selftest-hardirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_read_recursion_hard)

#include "locking-selftest-softirq.h"
GENERATE_PERMUTATIONS_3_EVENTS(irq_read_recursion_soft)

#undef E1
#undef E2
#undef E3

/*
 * read-lock / write-lock recursion that is unsafe.
 */

#define E1()				\
					\
	IRQ_DISABLE();			\
	L(B);				\
	WL(A);				\
	WU(A);				\
	U(B);				\
	IRQ_ENABLE();

#define E2()				\
					\
	RL(A);				\
	RU(A);				\

#define E3()				\
					\
	IRQ_ENTER();			\
	L(B);				\
	U(B);				\
	IRQ_EXIT();

/*
 * Generate 12 testcases:
 */
#include "locking-selftest-hardirq.h"
// GENERATE_PERMUTATIONS_3_EVENTS(irq_read_recursion2_hard)

#include "locking-selftest-softirq.h"
// GENERATE_PERMUTATIONS_3_EVENTS(irq_read_recursion2_soft)

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define I_SPINLOCK(x)	lockdep_reset_lock(&lock_##x.dep_map)
# define I_RWLOCK(x)	lockdep_reset_lock(&rwlock_##x.dep_map)
# define I_MUTEX(x)	lockdep_reset_lock(&mutex_##x.dep_map)
# define I_RWSEM(x)	lockdep_reset_lock(&rwsem_##x.dep_map)
#else
# define I_SPINLOCK(x)
# define I_RWLOCK(x)
# define I_MUTEX(x)
# define I_RWSEM(x)
#endif

#define I1(x)					\
	do {					\
		I_SPINLOCK(x);			\
		I_RWLOCK(x);			\
		I_MUTEX(x);			\
		I_RWSEM(x);			\
	} while (0)

#define I2(x)					\
	do {					\
		spin_lock_init(&lock_##x);	\
		rwlock_init(&rwlock_##x);	\
		mutex_init(&mutex_##x);		\
		init_rwsem(&rwsem_##x);		\
	} while (0)

static void reset_locks(void)
{
	local_irq_disable();
	I1(A); I1(B); I1(C); I1(D);
	I1(X1); I1(X2); I1(Y1); I1(Y2); I1(Z1); I1(Z2);
	lockdep_reset();
	I2(A); I2(B); I2(C); I2(D);
	init_shared_classes();
	local_irq_enable();
}

#undef I

static int testcase_total;
static int testcase_successes;
static int expected_testcase_failures;
static int unexpected_testcase_failures;

static void dotest(void (*testcase_fn)(void), int expected, int lockclass_mask)
{
	unsigned long saved_preempt_count = preempt_count();
	int expected_failure = 0;

	WARN_ON(irqs_disabled());

	testcase_fn();
	/*
	 * Filter out expected failures:
	 */
#ifndef CONFIG_PROVE_LOCKING
	if ((lockclass_mask & LOCKTYPE_SPIN) && debug_locks != expected)
		expected_failure = 1;
	if ((lockclass_mask & LOCKTYPE_RWLOCK) && debug_locks != expected)
		expected_failure = 1;
	if ((lockclass_mask & LOCKTYPE_MUTEX) && debug_locks != expected)
		expected_failure = 1;
	if ((lockclass_mask & LOCKTYPE_RWSEM) && debug_locks != expected)
		expected_failure = 1;
#endif
	if (debug_locks != expected) {
		if (expected_failure) {
			expected_testcase_failures++;
			printk("failed|");
		} else {
			unexpected_testcase_failures++;

			printk("FAILED|");
			dump_stack();
		}
	} else {
		testcase_successes++;
		printk("  ok  |");
	}
	testcase_total++;

	if (debug_locks_verbose)
		printk(" lockclass mask: %x, debug_locks: %d, expected: %d\n",
			lockclass_mask, debug_locks, expected);
	/*
	 * Some tests (e.g. double-unlock) might corrupt the preemption
	 * count, so restore it:
	 */
	preempt_count() = saved_preempt_count;
#ifdef CONFIG_TRACE_IRQFLAGS
	if (softirq_count())
		current->softirqs_enabled = 0;
	else
		current->softirqs_enabled = 1;
#endif

	reset_locks();
}

static inline void print_testname(const char *testname)
{
	printk("%33s:", testname);
}

#define DO_TESTCASE_1(desc, name, nr)				\
	print_testname(desc"/"#nr);				\
	dotest(name##_##nr, SUCCESS, LOCKTYPE_RWLOCK);		\
	printk("\n");

#define DO_TESTCASE_1B(desc, name, nr)				\
	print_testname(desc"/"#nr);				\
	dotest(name##_##nr, FAILURE, LOCKTYPE_RWLOCK);		\
	printk("\n");

#define DO_TESTCASE_3(desc, name, nr)				\
	print_testname(desc"/"#nr);				\
	dotest(name##_spin_##nr, FAILURE, LOCKTYPE_SPIN);	\
	dotest(name##_wlock_##nr, FAILURE, LOCKTYPE_RWLOCK);	\
	dotest(name##_rlock_##nr, SUCCESS, LOCKTYPE_RWLOCK);	\
	printk("\n");

#define DO_TESTCASE_3RW(desc, name, nr)				\
	print_testname(desc"/"#nr);				\
	dotest(name##_spin_##nr, FAILURE, LOCKTYPE_SPIN|LOCKTYPE_RWLOCK);\
	dotest(name##_wlock_##nr, FAILURE, LOCKTYPE_RWLOCK);	\
	dotest(name##_rlock_##nr, SUCCESS, LOCKTYPE_RWLOCK);	\
	printk("\n");

#define DO_TESTCASE_6(desc, name)				\
	print_testname(desc);					\
	dotest(name##_spin, FAILURE, LOCKTYPE_SPIN);		\
	dotest(name##_wlock, FAILURE, LOCKTYPE_RWLOCK);		\
	dotest(name##_rlock, FAILURE, LOCKTYPE_RWLOCK);		\
	dotest(name##_mutex, FAILURE, LOCKTYPE_MUTEX);		\
	dotest(name##_wsem, FAILURE, LOCKTYPE_RWSEM);		\
	dotest(name##_rsem, FAILURE, LOCKTYPE_RWSEM);		\
	printk("\n");

#define DO_TESTCASE_6_SUCCESS(desc, name)			\
	print_testname(desc);					\
	dotest(name##_spin, SUCCESS, LOCKTYPE_SPIN);		\
	dotest(name##_wlock, SUCCESS, LOCKTYPE_RWLOCK);		\
	dotest(name##_rlock, SUCCESS, LOCKTYPE_RWLOCK);		\
	dotest(name##_mutex, SUCCESS, LOCKTYPE_MUTEX);		\
	dotest(name##_wsem, SUCCESS, LOCKTYPE_RWSEM);		\
	dotest(name##_rsem, SUCCESS, LOCKTYPE_RWSEM);		\
	printk("\n");

/*
 * 'read' variant: rlocks must not trigger.
 */
#define DO_TESTCASE_6R(desc, name)				\
	print_testname(desc);					\
	dotest(name##_spin, FAILURE, LOCKTYPE_SPIN);		\
	dotest(name##_wlock, FAILURE, LOCKTYPE_RWLOCK);		\
	dotest(name##_rlock, SUCCESS, LOCKTYPE_RWLOCK);		\
	dotest(name##_mutex, FAILURE, LOCKTYPE_MUTEX);		\
	dotest(name##_wsem, FAILURE, LOCKTYPE_RWSEM);		\
	dotest(name##_rsem, FAILURE, LOCKTYPE_RWSEM);		\
	printk("\n");

#define DO_TESTCASE_2I(desc, name, nr)				\
	DO_TESTCASE_1("hard-"desc, name##_hard, nr);		\
	DO_TESTCASE_1("soft-"desc, name##_soft, nr);

#define DO_TESTCASE_2IB(desc, name, nr)				\
	DO_TESTCASE_1B("hard-"desc, name##_hard, nr);		\
	DO_TESTCASE_1B("soft-"desc, name##_soft, nr);

#define DO_TESTCASE_6I(desc, name, nr)				\
	DO_TESTCASE_3("hard-"desc, name##_hard, nr);		\
	DO_TESTCASE_3("soft-"desc, name##_soft, nr);

#define DO_TESTCASE_6IRW(desc, name, nr)			\
	DO_TESTCASE_3RW("hard-"desc, name##_hard, nr);		\
	DO_TESTCASE_3RW("soft-"desc, name##_soft, nr);

#define DO_TESTCASE_2x3(desc, name)				\
	DO_TESTCASE_3(desc, name, 12);				\
	DO_TESTCASE_3(desc, name, 21);

#define DO_TESTCASE_2x6(desc, name)				\
	DO_TESTCASE_6I(desc, name, 12);				\
	DO_TESTCASE_6I(desc, name, 21);

#define DO_TESTCASE_6x2(desc, name)				\
	DO_TESTCASE_2I(desc, name, 123);			\
	DO_TESTCASE_2I(desc, name, 132);			\
	DO_TESTCASE_2I(desc, name, 213);			\
	DO_TESTCASE_2I(desc, name, 231);			\
	DO_TESTCASE_2I(desc, name, 312);			\
	DO_TESTCASE_2I(desc, name, 321);

#define DO_TESTCASE_6x2B(desc, name)				\
	DO_TESTCASE_2IB(desc, name, 123);			\
	DO_TESTCASE_2IB(desc, name, 132);			\
	DO_TESTCASE_2IB(desc, name, 213);			\
	DO_TESTCASE_2IB(desc, name, 231);			\
	DO_TESTCASE_2IB(desc, name, 312);			\
	DO_TESTCASE_2IB(desc, name, 321);

#define DO_TESTCASE_6x6(desc, name)				\
	DO_TESTCASE_6I(desc, name, 123);			\
	DO_TESTCASE_6I(desc, name, 132);			\
	DO_TESTCASE_6I(desc, name, 213);			\
	DO_TESTCASE_6I(desc, name, 231);			\
	DO_TESTCASE_6I(desc, name, 312);			\
	DO_TESTCASE_6I(desc, name, 321);

#define DO_TESTCASE_6x6RW(desc, name)				\
	DO_TESTCASE_6IRW(desc, name, 123);			\
	DO_TESTCASE_6IRW(desc, name, 132);			\
	DO_TESTCASE_6IRW(desc, name, 213);			\
	DO_TESTCASE_6IRW(desc, name, 231);			\
	DO_TESTCASE_6IRW(desc, name, 312);			\
	DO_TESTCASE_6IRW(desc, name, 321);


void locking_selftest(void)
{
	/*
	 * Got a locking failure before the selftest ran?
	 */
	if (!debug_locks) {
		printk("----------------------------------\n");
		printk("| Locking API testsuite disabled |\n");
		printk("----------------------------------\n");
		return;
	}

	/*
	 * Run the testsuite:
	 */
	printk("------------------------\n");
	printk("| Locking API testsuite:\n");
	printk("----------------------------------------------------------------------------\n");
	printk("                                 | spin |wlock |rlock |mutex | wsem | rsem |\n");
	printk("  --------------------------------------------------------------------------\n");

	init_shared_classes();
	debug_locks_silent = !debug_locks_verbose;

	DO_TESTCASE_6R("A-A deadlock", AA);
	DO_TESTCASE_6R("A-B-B-A deadlock", ABBA);
	DO_TESTCASE_6R("A-B-B-C-C-A deadlock", ABBCCA);
	DO_TESTCASE_6R("A-B-C-A-B-C deadlock", ABCABC);
	DO_TESTCASE_6R("A-B-B-C-C-D-D-A deadlock", ABBCCDDA);
	DO_TESTCASE_6R("A-B-C-D-B-D-D-A deadlock", ABCDBDDA);
	DO_TESTCASE_6R("A-B-C-D-B-C-D-A deadlock", ABCDBCDA);
	DO_TESTCASE_6("double unlock", double_unlock);
	DO_TESTCASE_6("initialize held", init_held);
	DO_TESTCASE_6_SUCCESS("bad unlock order", bad_unlock_order);

	printk("  --------------------------------------------------------------------------\n");
	print_testname("recursive read-lock");
	printk("             |");
	dotest(rlock_AA1, SUCCESS, LOCKTYPE_RWLOCK);
	printk("             |");
	dotest(rsem_AA1, FAILURE, LOCKTYPE_RWSEM);
	printk("\n");

	print_testname("recursive read-lock #2");
	printk("             |");
	dotest(rlock_AA1B, SUCCESS, LOCKTYPE_RWLOCK);
	printk("             |");
	dotest(rsem_AA1B, FAILURE, LOCKTYPE_RWSEM);
	printk("\n");

	print_testname("mixed read-write-lock");
	printk("             |");
	dotest(rlock_AA2, FAILURE, LOCKTYPE_RWLOCK);
	printk("             |");
	dotest(rsem_AA2, FAILURE, LOCKTYPE_RWSEM);
	printk("\n");

	print_testname("mixed write-read-lock");
	printk("             |");
	dotest(rlock_AA3, FAILURE, LOCKTYPE_RWLOCK);
	printk("             |");
	dotest(rsem_AA3, FAILURE, LOCKTYPE_RWSEM);
	printk("\n");

	printk("  --------------------------------------------------------------------------\n");

	/*
	 * irq-context testcases:
	 */
	DO_TESTCASE_2x6("irqs-on + irq-safe-A", irqsafe1);
	DO_TESTCASE_2x3("sirq-safe-A => hirqs-on", irqsafe2A);
	DO_TESTCASE_2x6("safe-A + irqs-on", irqsafe2B);
	DO_TESTCASE_6x6("safe-A + unsafe-B #1", irqsafe3);
	DO_TESTCASE_6x6("safe-A + unsafe-B #2", irqsafe4);
	DO_TESTCASE_6x6RW("irq lock-inversion", irq_inversion);

	DO_TESTCASE_6x2("irq read-recursion", irq_read_recursion);
//	DO_TESTCASE_6x2B("irq read-recursion #2", irq_read_recursion2);

	if (unexpected_testcase_failures) {
		printk("-----------------------------------------------------------------\n");
		debug_locks = 0;
		printk("BUG: %3d unexpected failures (out of %3d) - debugging disabled! |\n",
			unexpected_testcase_failures, testcase_total);
		printk("-----------------------------------------------------------------\n");
	} else if (expected_testcase_failures && testcase_successes) {
		printk("--------------------------------------------------------\n");
		printk("%3d out of %3d testcases failed, as expected. |\n",
			expected_testcase_failures, testcase_total);
		printk("----------------------------------------------------\n");
		debug_locks = 1;
	} else if (expected_testcase_failures && !testcase_successes) {
		printk("--------------------------------------------------------\n");
		printk("All %3d testcases failed, as expected. |\n",
			expected_testcase_failures);
		printk("----------------------------------------\n");
		debug_locks = 1;
	} else {
		printk("-------------------------------------------------------\n");
		printk("Good, all %3d testcases passed! |\n",
			testcase_successes);
		printk("---------------------------------\n");
		debug_locks = 1;
	}
	debug_locks_silent = 0;
}
