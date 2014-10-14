#ifndef _ASM_IA64_ATOMIC_H
#define _ASM_IA64_ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 *
 * NOTE: don't mess with the types below!  The "unsigned long" and
 * "int" types were carefully placed so as to ensure proper operation
 * of the macros.
 *
 * Copyright (C) 1998, 1999, 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/types.h>

#include <asm/intrinsics.h>
#include <asm/system.h>


#define ATOMIC_INIT(i)		((atomic_t) { (i) })
#define ATOMIC64_INIT(i)	((atomic64_t) { (i) })

#define atomic_read(v)		(*(volatile int *)&(v)->counter)
#define atomic64_read(v)	(*(volatile long *)&(v)->counter)

#define atomic_set(v,i)		(((v)->counter) = (i))
#define atomic64_set(v,i)	(((v)->counter) = (i))

static __inline__ int
ia64_atomic_add (int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic_t)) != old);
	return new;
}

static __inline__ long
ia64_atomic64_add (__s64 i, atomic64_t *v)
{
	__s64 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic64_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic64_t)) != old);
	return new;
}

static __inline__ int
ia64_atomic_sub (int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic_t)) != old);
	return new;
}

static __inline__ long
ia64_atomic64_sub (__s64 i, atomic64_t *v)
{
	__s64 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic64_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic64_t)) != old);
	return new;
}

#define atomic_cmpxchg(v, old, new) (cmpxchg(&((v)->counter), old, new))
#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

#define atomic64_cmpxchg(v, old, new) \
	(cmpxchg(&((v)->counter), old, new))
#define atomic64_xchg(v, new) (xchg(&((v)->counter), new))

static __inline__ int atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c != (u);
}

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

static __inline__ long atomic64_add_unless(atomic64_t *v, long a, long u)
{
	long c, old;
	c = atomic64_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic64_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c != (u);
}

#define atomic64_inc_not_zero(v) atomic64_add_unless((v), 1, 0)

#define atomic_add_return(i,v)						\
({									\
	int __ia64_aar_i = (i);						\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->counter)	\
		: ia64_atomic_add(__ia64_aar_i, v);			\
})

#define atomic64_add_return(i,v)					\
({									\
	long __ia64_aar_i = (i);					\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->counter)	\
		: ia64_atomic64_add(__ia64_aar_i, v);			\
})

/*
 * Atomically add I to V and return TRUE if the resulting value is
 * negative.
 */
static __inline__ int
atomic_add_negative (int i, atomic_t *v)
{
	return atomic_add_return(i, v) < 0;
}

static __inline__ long
atomic64_add_negative (__s64 i, atomic64_t *v)
{
	return atomic64_add_return(i, v) < 0;
}

#define atomic_sub_return(i,v)						\
({									\
	int __ia64_asr_i = (i);						\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->counter)	\
		: ia64_atomic_sub(__ia64_asr_i, v);			\
})

#define atomic64_sub_return(i,v)					\
({									\
	long __ia64_asr_i = (i);					\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->counter)	\
		: ia64_atomic64_sub(__ia64_asr_i, v);			\
})

#define atomic_dec_return(v)		atomic_sub_return(1, (v))
#define atomic_inc_return(v)		atomic_add_return(1, (v))
#define atomic64_dec_return(v)		atomic64_sub_return(1, (v))
#define atomic64_inc_return(v)		atomic64_add_return(1, (v))

#define atomic_sub_and_test(i,v)	(atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_sub_return(1, (v)) == 0)
#define atomic_inc_and_test(v)		(atomic_add_return(1, (v)) == 0)
#define atomic64_sub_and_test(i,v)	(atomic64_sub_return((i), (v)) == 0)
#define atomic64_dec_and_test(v)	(atomic64_sub_return(1, (v)) == 0)
#define atomic64_inc_and_test(v)	(atomic64_add_return(1, (v)) == 0)

#define atomic_add(i,v)			atomic_add_return((i), (v))
#define atomic_sub(i,v)			atomic_sub_return((i), (v))
#define atomic_inc(v)			atomic_add(1, (v))
#define atomic_dec(v)			atomic_sub(1, (v))

#define atomic64_add(i,v)		atomic64_add_return((i), (v))
#define atomic64_sub(i,v)		atomic64_sub_return((i), (v))
#define atomic64_inc(v)			atomic64_add(1, (v))
#define atomic64_dec(v)			atomic64_sub(1, (v))

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#include <asm-generic/atomic-long.h>
#endif /* _ASM_IA64_ATOMIC_H */
