/*
 * Generic implementation of 64-bit atomics using spinlocks,
 * useful on processors that don't have 64-bit atomic instructions.
 *
 * Copyright © 2009 Paul Mackerras, IBM Corp. <paulus@au1.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/types.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/atomic.h>

/*
 * We use a hashed array of spinlocks to provide exclusive access
 * to each atomic64_t variable.  Since this is expected to used on
 * systems with small numbers of CPUs (<= 4 or so), we use a
 * relatively small array of 16 spinlocks to avoid wasting too much
 * memory on the spinlock array.
 */
#define NR_LOCKS	16

/*
 * Ensure each lock is in a separate cacheline.
 */
static union {
	spinlock_t lock;
	char pad[L1_CACHE_BYTES];
} atomic64_lock[NR_LOCKS] __cacheline_aligned_in_smp;

static inline spinlock_t *lock_addr(const atomic64_t *v)
{
	unsigned long addr = (unsigned long) v;

	addr >>= L1_CACHE_SHIFT;
	addr ^= (addr >> 8) ^ (addr >> 16);
	return &atomic64_lock[addr & (NR_LOCKS - 1)].lock;
}

long long atomic64_read(const atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_read);

void atomic64_set(atomic64_t *v, long long i)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);

	spin_lock_irqsave(lock, flags);
	v->counter = i;
	spin_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(atomic64_set);

void atomic64_add(long long a, atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);

	spin_lock_irqsave(lock, flags);
	v->counter += a;
	spin_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(atomic64_add);

long long atomic64_add_return(long long a, atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter += a;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_add_return);

void atomic64_sub(long long a, atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);

	spin_lock_irqsave(lock, flags);
	v->counter -= a;
	spin_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(atomic64_sub);

long long atomic64_sub_return(long long a, atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter -= a;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_sub_return);

long long atomic64_dec_if_positive(atomic64_t *v)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter - 1;
	if (val >= 0)
		v->counter = val;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_dec_if_positive);

long long atomic64_cmpxchg(atomic64_t *v, long long o, long long n)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter;
	if (val == o)
		v->counter = n;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_cmpxchg);

long long atomic64_xchg(atomic64_t *v, long long new)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	long long val;

	spin_lock_irqsave(lock, flags);
	val = v->counter;
	v->counter = new;
	spin_unlock_irqrestore(lock, flags);
	return val;
}
EXPORT_SYMBOL(atomic64_xchg);

int atomic64_add_unless(atomic64_t *v, long long a, long long u)
{
	unsigned long flags;
	spinlock_t *lock = lock_addr(v);
	int ret = 0;

	spin_lock_irqsave(lock, flags);
	if (v->counter != u) {
		v->counter += a;
		ret = 1;
	}
	spin_unlock_irqrestore(lock, flags);
	return ret;
}
EXPORT_SYMBOL(atomic64_add_unless);

static int init_atomic64_lock(void)
{
	int i;

	for (i = 0; i < NR_LOCKS; ++i)
		spin_lock_init(&atomic64_lock[i].lock);
	return 0;
}

pure_initcall(init_atomic64_lock);
