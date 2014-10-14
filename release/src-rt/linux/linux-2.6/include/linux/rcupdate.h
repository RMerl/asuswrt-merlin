/*
 * Read-Copy Update mechanism for mutual exclusion
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright IBM Corporation, 2001
 *
 * Author: Dipankar Sarma <dipankar@in.ibm.com>
 *
 * Based on the original work by Paul McKenney <paulmck@us.ibm.com>
 * and inputs from Rusty Russell, Andrea Arcangeli and Andi Kleen.
 * Papers:
 * http://www.rdrop.com/users/paulmck/paper/rclockpdcsproof.pdf
 * http://lse.sourceforge.net/locking/rclock_OLS.2001.05.01c.sc.pdf (OLS2001)
 *
 * For detailed explanation of Read-Copy Update mechanism see -
 *		http://lse.sourceforge.net/locking/rcupdate.html
 *
 */

#ifndef __LINUX_RCUPDATE_H
#define __LINUX_RCUPDATE_H

#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/seqlock.h>
#include <linux/lockdep.h>
#include <linux/completion.h>
#include <linux/debugobjects.h>
#include <linux/compiler.h>

#ifdef CONFIG_RCU_TORTURE_TEST
extern int rcutorture_runnable; /* for sysctl */
#endif /* #ifdef CONFIG_RCU_TORTURE_TEST */

#define UINT_CMP_GE(a, b)	(UINT_MAX / 2 >= (a) - (b))
#define UINT_CMP_LT(a, b)	(UINT_MAX / 2 < (a) - (b))
#define ULONG_CMP_GE(a, b)	(ULONG_MAX / 2 >= (a) - (b))
#define ULONG_CMP_LT(a, b)	(ULONG_MAX / 2 < (a) - (b))

/**
 * struct rcu_head - callback structure for use with RCU
 * @next: next update requests in a list
 * @func: actual update function to call after the grace period.
 */
struct rcu_head {
	struct rcu_head *next;
	void (*func)(struct rcu_head *head);
};

/* Exported common interfaces */
extern void call_rcu_sched(struct rcu_head *head,
			   void (*func)(struct rcu_head *rcu));
extern void synchronize_sched(void);
extern void rcu_barrier_bh(void);
extern void rcu_barrier_sched(void);
extern int sched_expedited_torture_stats(char *page);

static inline void __rcu_read_lock_bh(void)
{
	local_bh_disable();
}

static inline void __rcu_read_unlock_bh(void)
{
	local_bh_enable();
}

#ifdef CONFIG_PREEMPT_RCU

extern void __rcu_read_lock(void);
extern void __rcu_read_unlock(void);
void synchronize_rcu(void);

/*
 * Defined as a macro as it is a very low level header included from
 * areas that don't even know about current.  This gives the rcu_read_lock()
 * nesting depth, but makes sense only if CONFIG_PREEMPT_RCU -- in other
 * types of kernel builds, the rcu_read_lock() nesting depth is unknowable.
 */
#define rcu_preempt_depth() (current->rcu_read_lock_nesting)

#else /* #ifdef CONFIG_PREEMPT_RCU */

static inline void __rcu_read_lock(void)
{
	preempt_disable();
}

static inline void __rcu_read_unlock(void)
{
	preempt_enable();
}

static inline void synchronize_rcu(void)
{
	synchronize_sched();
}

static inline int rcu_preempt_depth(void)
{
	return 0;
}

#endif /* #else #ifdef CONFIG_PREEMPT_RCU */

/* Internal to kernel */
extern void rcu_sched_qs(int cpu);
extern void rcu_bh_qs(int cpu);
extern void rcu_check_callbacks(int cpu, int user);
struct notifier_block;

#ifdef CONFIG_NO_HZ

extern void rcu_enter_nohz(void);
extern void rcu_exit_nohz(void);

#else /* #ifdef CONFIG_NO_HZ */

static inline void rcu_enter_nohz(void)
{
}

static inline void rcu_exit_nohz(void)
{
}

#endif /* #else #ifdef CONFIG_NO_HZ */

#if defined(CONFIG_TREE_RCU) || defined(CONFIG_TREE_PREEMPT_RCU)
#include <linux/rcutree.h>
#elif defined(CONFIG_TINY_RCU) || defined(CONFIG_TINY_PREEMPT_RCU)
#include <linux/rcutiny.h>
#else
#error "Unknown RCU implementation specified to kernel configuration"
#endif

/*
 * init_rcu_head_on_stack()/destroy_rcu_head_on_stack() are needed for dynamic
 * initialization and destruction of rcu_head on the stack. rcu_head structures
 * allocated dynamically in the heap or defined statically don't need any
 * initialization.
 */
#ifdef CONFIG_DEBUG_OBJECTS_RCU_HEAD
extern void init_rcu_head_on_stack(struct rcu_head *head);
extern void destroy_rcu_head_on_stack(struct rcu_head *head);
#else /* !CONFIG_DEBUG_OBJECTS_RCU_HEAD */
static inline void init_rcu_head_on_stack(struct rcu_head *head)
{
}

static inline void destroy_rcu_head_on_stack(struct rcu_head *head)
{
}
#endif	/* #else !CONFIG_DEBUG_OBJECTS_RCU_HEAD */

#ifdef CONFIG_DEBUG_LOCK_ALLOC

extern struct lockdep_map rcu_lock_map;
# define rcu_read_acquire() \
		lock_acquire(&rcu_lock_map, 0, 0, 2, 1, NULL, _THIS_IP_)
# define rcu_read_release()	lock_release(&rcu_lock_map, 1, _THIS_IP_)

extern struct lockdep_map rcu_bh_lock_map;
# define rcu_read_acquire_bh() \
		lock_acquire(&rcu_bh_lock_map, 0, 0, 2, 1, NULL, _THIS_IP_)
# define rcu_read_release_bh()	lock_release(&rcu_bh_lock_map, 1, _THIS_IP_)

extern struct lockdep_map rcu_sched_lock_map;
# define rcu_read_acquire_sched() \
		lock_acquire(&rcu_sched_lock_map, 0, 0, 2, 1, NULL, _THIS_IP_)
# define rcu_read_release_sched() \
		lock_release(&rcu_sched_lock_map, 1, _THIS_IP_)

extern int debug_lockdep_rcu_enabled(void);

/**
 * rcu_read_lock_held() - might we be in RCU read-side critical section?
 *
 * If CONFIG_DEBUG_LOCK_ALLOC is selected, returns nonzero iff in an RCU
 * read-side critical section.  In absence of CONFIG_DEBUG_LOCK_ALLOC,
 * this assumes we are in an RCU read-side critical section unless it can
 * prove otherwise.  This is useful for debug checks in functions that
 * require that they be called within an RCU read-side critical section.
 *
 * Checks debug_lockdep_rcu_enabled() to prevent false positives during boot
 * and while lockdep is disabled.
 */
static inline int rcu_read_lock_held(void)
{
	if (!debug_lockdep_rcu_enabled())
		return 1;
	return lock_is_held(&rcu_lock_map);
}

/*
 * rcu_read_lock_bh_held() is defined out of line to avoid #include-file
 * hell.
 */
extern int rcu_read_lock_bh_held(void);

/**
 * rcu_read_lock_sched_held() - might we be in RCU-sched read-side critical section?
 *
 * If CONFIG_DEBUG_LOCK_ALLOC is selected, returns nonzero iff in an
 * RCU-sched read-side critical section.  In absence of
 * CONFIG_DEBUG_LOCK_ALLOC, this assumes we are in an RCU-sched read-side
 * critical section unless it can prove otherwise.  Note that disabling
 * of preemption (including disabling irqs) counts as an RCU-sched
 * read-side critical section.  This is useful for debug checks in functions
 * that required that they be called within an RCU-sched read-side
 * critical section.
 *
 * Check debug_lockdep_rcu_enabled() to prevent false positives during boot
 * and while lockdep is disabled.
 */
#ifdef CONFIG_PREEMPT
static inline int rcu_read_lock_sched_held(void)
{
	int lockdep_opinion = 0;

	if (!debug_lockdep_rcu_enabled())
		return 1;
	if (debug_locks)
		lockdep_opinion = lock_is_held(&rcu_sched_lock_map);
	return lockdep_opinion || preempt_count() != 0 || irqs_disabled();
}
#else /* #ifdef CONFIG_PREEMPT */
static inline int rcu_read_lock_sched_held(void)
{
	return 1;
}
#endif /* #else #ifdef CONFIG_PREEMPT */

#else /* #ifdef CONFIG_DEBUG_LOCK_ALLOC */

# define rcu_read_acquire()		do { } while (0)
# define rcu_read_release()		do { } while (0)
# define rcu_read_acquire_bh()		do { } while (0)
# define rcu_read_release_bh()		do { } while (0)
# define rcu_read_acquire_sched()	do { } while (0)
# define rcu_read_release_sched()	do { } while (0)

static inline int rcu_read_lock_held(void)
{
	return 1;
}

static inline int rcu_read_lock_bh_held(void)
{
	return 1;
}

#ifdef CONFIG_PREEMPT
static inline int rcu_read_lock_sched_held(void)
{
	return preempt_count() != 0 || irqs_disabled();
}
#else /* #ifdef CONFIG_PREEMPT */
static inline int rcu_read_lock_sched_held(void)
{
	return 1;
}
#endif /* #else #ifdef CONFIG_PREEMPT */

#endif /* #else #ifdef CONFIG_DEBUG_LOCK_ALLOC */

#ifdef CONFIG_PROVE_RCU

extern int rcu_my_thread_group_empty(void);

/**
 * rcu_lockdep_assert - emit lockdep splat if specified condition not met
 * @c: condition to check
 */
#define rcu_lockdep_assert(c)						\
	do {								\
		static bool __warned;					\
		if (debug_lockdep_rcu_enabled() && !__warned && !(c)) {	\
			__warned = true;				\
			lockdep_rcu_dereference(__FILE__, __LINE__);	\
		}							\
	} while (0)

#else /* #ifdef CONFIG_PROVE_RCU */

#define rcu_lockdep_assert(c) do { } while (0)

#endif /* #else #ifdef CONFIG_PROVE_RCU */

/*
 * Helper functions for rcu_dereference_check(), rcu_dereference_protected()
 * and rcu_assign_pointer().  Some of these could be folded into their
 * callers, but they are left separate in order to ease introduction of
 * multiple flavors of pointers to match the multiple flavors of RCU
 * (e.g., __rcu_bh, * __rcu_sched, and __srcu), should this make sense in
 * the future.
 */

#ifdef __CHECKER__
#define rcu_dereference_sparse(p, space) \
	((void)(((typeof(*p) space *)p) == p))
#else /* #ifdef __CHECKER__ */
#define rcu_dereference_sparse(p, space)
#endif /* #else #ifdef __CHECKER__ */

#define __rcu_access_pointer(p, space) \
	({ \
		typeof(*p) *_________p1 = (typeof(*p)*__force )ACCESS_ONCE(p); \
		rcu_dereference_sparse(p, space); \
		((typeof(*p) __force __kernel *)(_________p1)); \
	})
#define __rcu_dereference_check(p, c, space) \
	({ \
		typeof(*p) *_________p1 = (typeof(*p)*__force )ACCESS_ONCE(p); \
		rcu_lockdep_assert(c); \
		rcu_dereference_sparse(p, space); \
		smp_read_barrier_depends(); \
		((typeof(*p) __force __kernel *)(_________p1)); \
	})
#define __rcu_dereference_protected(p, c, space) \
	({ \
		rcu_lockdep_assert(c); \
		rcu_dereference_sparse(p, space); \
		((typeof(*p) __force __kernel *)(p)); \
	})

#define __rcu_access_index(p, space) \
	({ \
		typeof(p) _________p1 = ACCESS_ONCE(p); \
		rcu_dereference_sparse(p, space); \
		(_________p1); \
	})
#define __rcu_dereference_index_check(p, c) \
	({ \
		typeof(p) _________p1 = ACCESS_ONCE(p); \
		rcu_lockdep_assert(c); \
		smp_read_barrier_depends(); \
		(_________p1); \
	})
#define __rcu_assign_pointer(p, v, space) \
	({ \
		if (!__builtin_constant_p(v) || \
		    ((v) != NULL)) \
			smp_wmb(); \
		(p) = (typeof(*v) __force space *)(v); \
	})


/**
 * rcu_access_pointer() - fetch RCU pointer with no dereferencing
 * @p: The pointer to read
 *
 * Return the value of the specified RCU-protected pointer, but omit the
 * smp_read_barrier_depends() and keep the ACCESS_ONCE().  This is useful
 * when the value of this pointer is accessed, but the pointer is not
 * dereferenced, for example, when testing an RCU-protected pointer against
 * NULL.  Although rcu_access_pointer() may also be used in cases where
 * update-side locks prevent the value of the pointer from changing, you
 * should instead use rcu_dereference_protected() for this use case.
 */
#define rcu_access_pointer(p) __rcu_access_pointer((p), __rcu)

/**
 * rcu_dereference_check() - rcu_dereference with debug checking
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * Do an rcu_dereference(), but check that the conditions under which the
 * dereference will take place are correct.  Typically the conditions
 * indicate the various locking conditions that should be held at that
 * point.  The check should return true if the conditions are satisfied.
 * An implicit check for being in an RCU read-side critical section
 * (rcu_read_lock()) is included.
 *
 * For example:
 *
 *	bar = rcu_dereference_check(foo->bar, lockdep_is_held(&foo->lock));
 *
 * could be used to indicate to lockdep that foo->bar may only be dereferenced
 * if either rcu_read_lock() is held, or that the lock required to replace
 * the bar struct at foo->bar is held.
 *
 * Note that the list of conditions may also include indications of when a lock
 * need not be held, for example during initialisation or destruction of the
 * target struct:
 *
 *	bar = rcu_dereference_check(foo->bar, lockdep_is_held(&foo->lock) ||
 *					      atomic_read(&foo->usage) == 0);
 *
 * Inserts memory barriers on architectures that require them
 * (currently only the Alpha), prevents the compiler from refetching
 * (and from merging fetches), and, more importantly, documents exactly
 * which pointers are protected by RCU and checks that the pointer is
 * annotated as __rcu.
 */
#define rcu_dereference_check(p, c) \
	__rcu_dereference_check((p), rcu_read_lock_held() || (c), __rcu)

/**
 * rcu_dereference_bh_check() - rcu_dereference_bh with debug checking
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * This is the RCU-bh counterpart to rcu_dereference_check().
 */
#define rcu_dereference_bh_check(p, c) \
	__rcu_dereference_check((p), rcu_read_lock_bh_held() || (c), __rcu)

/**
 * rcu_dereference_sched_check() - rcu_dereference_sched with debug checking
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * This is the RCU-sched counterpart to rcu_dereference_check().
 */
#define rcu_dereference_sched_check(p, c) \
	__rcu_dereference_check((p), rcu_read_lock_sched_held() || (c), \
				__rcu)

#define rcu_dereference_raw(p) rcu_dereference_check(p, 1) /*@@@ needed? @@@*/

/**
 * rcu_access_index() - fetch RCU index with no dereferencing
 * @p: The index to read
 *
 * Return the value of the specified RCU-protected index, but omit the
 * smp_read_barrier_depends() and keep the ACCESS_ONCE().  This is useful
 * when the value of this index is accessed, but the index is not
 * dereferenced, for example, when testing an RCU-protected index against
 * -1.  Although rcu_access_index() may also be used in cases where
 * update-side locks prevent the value of the index from changing, you
 * should instead use rcu_dereference_index_protected() for this use case.
 */
#define rcu_access_index(p) __rcu_access_index((p), __rcu)

/**
 * rcu_dereference_index_check() - rcu_dereference for indices with debug checking
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * Similar to rcu_dereference_check(), but omits the sparse checking.
 * This allows rcu_dereference_index_check() to be used on integers,
 * which can then be used as array indices.  Attempting to use
 * rcu_dereference_check() on an integer will give compiler warnings
 * because the sparse address-space mechanism relies on dereferencing
 * the RCU-protected pointer.  Dereferencing integers is not something
 * that even gcc will put up with.
 *
 * Note that this function does not implicitly check for RCU read-side
 * critical sections.  If this function gains lots of uses, it might
 * make sense to provide versions for each flavor of RCU, but it does
 * not make sense as of early 2010.
 */
#define rcu_dereference_index_check(p, c) \
	__rcu_dereference_index_check((p), (c))

/**
 * rcu_dereference_protected() - fetch RCU pointer when updates prevented
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * Return the value of the specified RCU-protected pointer, but omit
 * both the smp_read_barrier_depends() and the ACCESS_ONCE().  This
 * is useful in cases where update-side locks prevent the value of the
 * pointer from changing.  Please note that this primitive does -not-
 * prevent the compiler from repeating this reference or combining it
 * with other references, so it should not be used without protection
 * of appropriate locks.
 *
 * This function is only for update-side use.  Using this function
 * when protected only by rcu_read_lock() will result in infrequent
 * but very ugly failures.
 */
#define rcu_dereference_protected(p, c) \
	__rcu_dereference_protected((p), (c), __rcu)

/**
 * rcu_dereference_bh_protected() - fetch RCU-bh pointer when updates prevented
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * This is the RCU-bh counterpart to rcu_dereference_protected().
 */
#define rcu_dereference_bh_protected(p, c) \
	__rcu_dereference_protected((p), (c), __rcu)

/**
 * rcu_dereference_sched_protected() - fetch RCU-sched pointer when updates prevented
 * @p: The pointer to read, prior to dereferencing
 * @c: The conditions under which the dereference will take place
 *
 * This is the RCU-sched counterpart to rcu_dereference_protected().
 */
#define rcu_dereference_sched_protected(p, c) \
	__rcu_dereference_protected((p), (c), __rcu)


/**
 * rcu_dereference() - fetch RCU-protected pointer for dereferencing
 * @p: The pointer to read, prior to dereferencing
 *
 * This is a simple wrapper around rcu_dereference_check().
 */
#define rcu_dereference(p) rcu_dereference_check(p, 0)

/**
 * rcu_dereference_bh() - fetch an RCU-bh-protected pointer for dereferencing
 * @p: The pointer to read, prior to dereferencing
 *
 * Makes rcu_dereference_check() do the dirty work.
 */
#define rcu_dereference_bh(p) rcu_dereference_bh_check(p, 0)

/**
 * rcu_dereference_sched() - fetch RCU-sched-protected pointer for dereferencing
 * @p: The pointer to read, prior to dereferencing
 *
 * Makes rcu_dereference_check() do the dirty work.
 */
#define rcu_dereference_sched(p) rcu_dereference_sched_check(p, 0)

/**
 * rcu_read_lock() - mark the beginning of an RCU read-side critical section
 *
 * When synchronize_rcu() is invoked on one CPU while other CPUs
 * are within RCU read-side critical sections, then the
 * synchronize_rcu() is guaranteed to block until after all the other
 * CPUs exit their critical sections.  Similarly, if call_rcu() is invoked
 * on one CPU while other CPUs are within RCU read-side critical
 * sections, invocation of the corresponding RCU callback is deferred
 * until after the all the other CPUs exit their critical sections.
 *
 * Note, however, that RCU callbacks are permitted to run concurrently
 * with new RCU read-side critical sections.  One way that this can happen
 * is via the following sequence of events: (1) CPU 0 enters an RCU
 * read-side critical section, (2) CPU 1 invokes call_rcu() to register
 * an RCU callback, (3) CPU 0 exits the RCU read-side critical section,
 * (4) CPU 2 enters a RCU read-side critical section, (5) the RCU
 * callback is invoked.  This is legal, because the RCU read-side critical
 * section that was running concurrently with the call_rcu() (and which
 * therefore might be referencing something that the corresponding RCU
 * callback would free up) has completed before the corresponding
 * RCU callback is invoked.
 *
 * RCU read-side critical sections may be nested.  Any deferred actions
 * will be deferred until the outermost RCU read-side critical section
 * completes.
 *
 * You can avoid reading and understanding the next paragraph by
 * following this rule: don't put anything in an rcu_read_lock() RCU
 * read-side critical section that would block in a !PREEMPT kernel.
 * But if you want the full story, read on!
 *
 * In non-preemptible RCU implementations (TREE_RCU and TINY_RCU), it
 * is illegal to block while in an RCU read-side critical section.  In
 * preemptible RCU implementations (TREE_PREEMPT_RCU and TINY_PREEMPT_RCU)
 * in CONFIG_PREEMPT kernel builds, RCU read-side critical sections may
 * be preempted, but explicit blocking is illegal.  Finally, in preemptible
 * RCU implementations in real-time (CONFIG_PREEMPT_RT) kernel builds,
 * RCU read-side critical sections may be preempted and they may also
 * block, but only when acquiring spinlocks that are subject to priority
 * inheritance.
 */
static inline void rcu_read_lock(void)
{
	__rcu_read_lock();
	__acquire(RCU);
	rcu_read_acquire();
}

/*
 * So where is rcu_write_lock()?  It does not exist, as there is no
 * way for writers to lock out RCU readers.  This is a feature, not
 * a bug -- this property is what provides RCU's performance benefits.
 * Of course, writers must coordinate with each other.  The normal
 * spinlock primitives work well for this, but any other technique may be
 * used as well.  RCU does not care how the writers keep out of each
 * others' way, as long as they do so.
 */

/**
 * rcu_read_unlock() - marks the end of an RCU read-side critical section.
 *
 * See rcu_read_lock() for more information.
 */
static inline void rcu_read_unlock(void)
{
	rcu_read_release();
	__release(RCU);
	__rcu_read_unlock();
}

/**
 * rcu_read_lock_bh() - mark the beginning of an RCU-bh critical section
 *
 * This is equivalent of rcu_read_lock(), but to be used when updates
 * are being done using call_rcu_bh() or synchronize_rcu_bh(). Since
 * both call_rcu_bh() and synchronize_rcu_bh() consider completion of a
 * softirq handler to be a quiescent state, a process in RCU read-side
 * critical section must be protected by disabling softirqs. Read-side
 * critical sections in interrupt context can use just rcu_read_lock(),
 * though this should at least be commented to avoid confusing people
 * reading the code.
 */
static inline void rcu_read_lock_bh(void)
{
	__rcu_read_lock_bh();
	__acquire(RCU_BH);
	rcu_read_acquire_bh();
}

/*
 * rcu_read_unlock_bh - marks the end of a softirq-only RCU critical section
 *
 * See rcu_read_lock_bh() for more information.
 */
static inline void rcu_read_unlock_bh(void)
{
	rcu_read_release_bh();
	__release(RCU_BH);
	__rcu_read_unlock_bh();
}

/**
 * rcu_read_lock_sched() - mark the beginning of a RCU-sched critical section
 *
 * This is equivalent of rcu_read_lock(), but to be used when updates
 * are being done using call_rcu_sched() or synchronize_rcu_sched().
 * Read-side critical sections can also be introduced by anything that
 * disables preemption, including local_irq_disable() and friends.
 */
static inline void rcu_read_lock_sched(void)
{
	preempt_disable();
	__acquire(RCU_SCHED);
	rcu_read_acquire_sched();
}

/* Used by lockdep and tracing: cannot be traced, cannot call lockdep. */
static inline notrace void rcu_read_lock_sched_notrace(void)
{
	preempt_disable_notrace();
	__acquire(RCU_SCHED);
}

/*
 * rcu_read_unlock_sched - marks the end of a RCU-classic critical section
 *
 * See rcu_read_lock_sched for more information.
 */
static inline void rcu_read_unlock_sched(void)
{
	rcu_read_release_sched();
	__release(RCU_SCHED);
	preempt_enable();
}

/* Used by lockdep and tracing: cannot be traced, cannot call lockdep. */
static inline notrace void rcu_read_unlock_sched_notrace(void)
{
	__release(RCU_SCHED);
	preempt_enable_notrace();
}

/**
 * rcu_assign_pointer() - assign to RCU-protected pointer
 * @p: pointer to assign to
 * @v: value to assign (publish)
 *
 * Assigns the specified value to the specified RCU-protected
 * pointer, ensuring that any concurrent RCU readers will see
 * any prior initialization.  Returns the value assigned.
 *
 * Inserts memory barriers on architectures that require them
 * (pretty much all of them other than x86), and also prevents
 * the compiler from reordering the code that initializes the
 * structure after the pointer assignment.  More importantly, this
 * call documents which pointers will be dereferenced by RCU read-side
 * code.
 */
#define rcu_assign_pointer(p, v) \
	__rcu_assign_pointer((p), (v), __rcu)

/**
 * RCU_INIT_POINTER() - initialize an RCU protected pointer
 *
 * Initialize an RCU-protected pointer in such a way to avoid RCU-lockdep
 * splats.
 */
#define RCU_INIT_POINTER(p, v) \
		p = (typeof(*v) __force __rcu *)(v)

/* Infrastructure to implement the synchronize_() primitives. */

struct rcu_synchronize {
	struct rcu_head head;
	struct completion completion;
};

extern void wakeme_after_rcu(struct rcu_head  *head);

#ifdef CONFIG_PREEMPT_RCU

/**
 * call_rcu() - Queue an RCU callback for invocation after a grace period.
 * @head: structure to be used for queueing the RCU updates.
 * @func: actual callback function to be invoked after the grace period
 *
 * The callback function will be invoked some time after a full grace
 * period elapses, in other words after all pre-existing RCU read-side
 * critical sections have completed.  However, the callback function
 * might well execute concurrently with RCU read-side critical sections
 * that started after call_rcu() was invoked.  RCU read-side critical
 * sections are delimited by rcu_read_lock() and rcu_read_unlock(),
 * and may be nested.
 */
extern void call_rcu(struct rcu_head *head,
			      void (*func)(struct rcu_head *head));

#else /* #ifdef CONFIG_PREEMPT_RCU */

/* In classic RCU, call_rcu() is just call_rcu_sched(). */
#define	call_rcu	call_rcu_sched

#endif /* #else #ifdef CONFIG_PREEMPT_RCU */

/**
 * call_rcu_bh() - Queue an RCU for invocation after a quicker grace period.
 * @head: structure to be used for queueing the RCU updates.
 * @func: actual callback function to be invoked after the grace period
 *
 * The callback function will be invoked some time after a full grace
 * period elapses, in other words after all currently executing RCU
 * read-side critical sections have completed. call_rcu_bh() assumes
 * that the read-side critical sections end on completion of a softirq
 * handler. This means that read-side critical sections in process
 * context must not be interrupted by softirqs. This interface is to be
 * used when most of the read-side critical sections are in softirq context.
 * RCU read-side critical sections are delimited by :
 *  - rcu_read_lock() and  rcu_read_unlock(), if in interrupt context.
 *  OR
 *  - rcu_read_lock_bh() and rcu_read_unlock_bh(), if in process context.
 *  These may be nested.
 */
extern void call_rcu_bh(struct rcu_head *head,
			void (*func)(struct rcu_head *head));

/*
 * debug_rcu_head_queue()/debug_rcu_head_unqueue() are used internally
 * by call_rcu() and rcu callback execution, and are therefore not part of the
 * RCU API. Leaving in rcupdate.h because they are used by all RCU flavors.
 */

#ifdef CONFIG_DEBUG_OBJECTS_RCU_HEAD
# define STATE_RCU_HEAD_READY	0
# define STATE_RCU_HEAD_QUEUED	1

extern struct debug_obj_descr rcuhead_debug_descr;

static inline void debug_rcu_head_queue(struct rcu_head *head)
{
	debug_object_activate(head, &rcuhead_debug_descr);
	debug_object_active_state(head, &rcuhead_debug_descr,
				  STATE_RCU_HEAD_READY,
				  STATE_RCU_HEAD_QUEUED);
}

static inline void debug_rcu_head_unqueue(struct rcu_head *head)
{
	debug_object_active_state(head, &rcuhead_debug_descr,
				  STATE_RCU_HEAD_QUEUED,
				  STATE_RCU_HEAD_READY);
	debug_object_deactivate(head, &rcuhead_debug_descr);
}
#else	/* !CONFIG_DEBUG_OBJECTS_RCU_HEAD */
static inline void debug_rcu_head_queue(struct rcu_head *head)
{
}

static inline void debug_rcu_head_unqueue(struct rcu_head *head)
{
}
#endif	/* #else !CONFIG_DEBUG_OBJECTS_RCU_HEAD */

#endif /* __LINUX_RCUPDATE_H */
