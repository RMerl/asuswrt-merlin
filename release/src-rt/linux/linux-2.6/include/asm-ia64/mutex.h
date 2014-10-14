/*
 * ia64 implementation of the mutex fastpath.
 *
 * Copyright (C) 2006 Ken Chen <kenneth.w.chen@intel.com>
 *
 */

#ifndef _ASM_MUTEX_H
#define _ASM_MUTEX_H

/**
 *  __mutex_fastpath_lock - try to take the lock by moving the count
 *                          from 1 to a 0 value
 *  @count: pointer of type atomic_t
 *  @fail_fn: function to call if the original value was not 1
 *
 * Change the count from 1 to a value lower than 1, and call <fail_fn> if
 * it wasn't 1 originally. This function MUST leave the value lower than
 * 1 even when the "1" assertion wasn't true.
 */
static inline void
__mutex_fastpath_lock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(ia64_fetchadd4_acq(count, -1) != 1))
		fail_fn(count);
}

/**
 *  __mutex_fastpath_lock_retval - try to take the lock by moving the count
 *                                 from 1 to a 0 value
 *  @count: pointer of type atomic_t
 *  @fail_fn: function to call if the original value was not 1
 *
 * Change the count from 1 to a value lower than 1, and call <fail_fn> if
 * it wasn't 1 originally. This function returns 0 if the fastpath succeeds,
 * or anything the slow path function returns.
 */
static inline int
__mutex_fastpath_lock_retval(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (unlikely(ia64_fetchadd4_acq(count, -1) != 1))
		return fail_fn(count);
	return 0;
}

/**
 *  __mutex_fastpath_unlock - try to promote the count from 0 to 1
 *  @count: pointer of type atomic_t
 *  @fail_fn: function to call if the original value was not 0
 *
 * Try to promote the count from 0 to 1. If it wasn't 0, call <fail_fn>.
 * In the failure case, this function is allowed to either set the value to
 * 1, or to set it to a value lower than 1.
 *
 * If the implementation sets it to a value of lower than 1, then the
 * __mutex_slowpath_needs_to_unlock() macro needs to return 1, it needs
 * to return 0 otherwise.
 */
static inline void
__mutex_fastpath_unlock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	int ret = ia64_fetchadd4_rel(count, 1);
	if (unlikely(ret < 0))
		fail_fn(count);
}

#define __mutex_slowpath_needs_to_unlock()		1

/**
 * __mutex_fastpath_trylock - try to acquire the mutex, without waiting
 *
 *  @count: pointer of type atomic_t
 *  @fail_fn: fallback function
 *
 * Change the count from 1 to a value lower than 1, and return 0 (failure)
 * if it wasn't 1 originally, or return 1 (success) otherwise. This function
 * MUST leave the value lower than 1 even when the "1" assertion wasn't true.
 * Additionally, if the value was < 0 originally, this function must not leave
 * it to 0 on failure.
 *
 * If the architecture has no effective trylock variant, it should call the
 * <fail_fn> spinlock-based trylock variant unconditionally.
 */
static inline int
__mutex_fastpath_trylock(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (cmpxchg_acq(count, 1, 0) == 1)
		return 1;
	return 0;
}

#endif
