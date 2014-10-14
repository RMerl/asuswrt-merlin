#ifndef __ARCH_I386_ATOMIC__
#define __ARCH_I386_ATOMIC__

#include <linux/compiler.h>
#include <asm/processor.h>
#include <asm/cmpxchg.h>

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { int counter; } atomic_t;

#define ATOMIC_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically reads the value of @v.
 */ 
#define atomic_read(v)		((v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 * 
 * Atomically sets the value of @v to @i.
 */ 
#define atomic_set(v,i)		(((v)->counter) = (i))

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 * 
 * Atomically adds @i to @v.
 */
static __inline__ void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addl %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

/**
 * atomic_sub - subtract integer from atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v.
 */
static __inline__ void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subl %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline__ int atomic_sub_and_test(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subl %2,%0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1.
 */ 
static __inline__ void atomic_inc(atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incl %0"
		:"+m" (v->counter));
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.
 */ 
static __inline__ void atomic_dec(atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		:"+m" (v->counter));
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */ 
static __inline__ int atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * atomic_inc_and_test - increment and test 
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */ 
static __inline__ int atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 * 
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */ 
static __inline__ int atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addl %2,%0; sets %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

/**
 * atomic_add_return - add integer and return
 * @v: pointer of type atomic_t
 * @i: integer value to add
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __inline__ int atomic_add_return(int i, atomic_t *v)
{
	int __i;
#ifdef CONFIG_M386
	unsigned long flags;
	if(unlikely(boot_cpu_data.x86 <= 3))
		goto no_xadd;
#endif
	/* Modern 486+ processor */
	__i = i;
	__asm__ __volatile__(
		LOCK_PREFIX "xaddl %0, %1"
		:"+r" (i), "+m" (v->counter)
		: : "memory");
	return i + __i;

#ifdef CONFIG_M386
no_xadd: /* Legacy 386 processor */
	local_irq_save(flags);
	__i = atomic_read(v);
	atomic_set(v, i + __i);
	local_irq_restore(flags);
	return i + __i;
#endif
}

/**
 * atomic_sub_return - subtract integer and return
 * @v: pointer of type atomic_t
 * @i: integer value to subtract
 *
 * Atomically subtracts @i from @v and returns @v - @i
 */
static __inline__ int atomic_sub_return(int i, atomic_t *v)
{
	return atomic_add_return(-i,v);
}

#define atomic_cmpxchg(v, old, new) (cmpxchg(&((v)->counter), (old), (new)))
#define atomic_xchg(v, new) (xchg(&((v)->counter), (new)))

/**
 * atomic_add_unless - add unless the number is already a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as @v was not already @u.
 * Returns non-zero if @v was not @u, and zero otherwise.
 */
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

#define atomic_inc_return(v)  (atomic_add_return(1,v))
#define atomic_dec_return(v)  (atomic_sub_return(1,v))

/* These are x86-specific, used by some header files */
#define atomic_clear_mask(mask, addr) \
__asm__ __volatile__(LOCK_PREFIX "andl %0,%1" \
: : "r" (~(mask)),"m" (*addr) : "memory")

#define atomic_set_mask(mask, addr) \
__asm__ __volatile__(LOCK_PREFIX "orl %0,%1" \
: : "r" (mask),"m" (*(addr)) : "memory")

/* Atomic operations are already serializing on x86 */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#include <asm-generic/atomic.h>
#endif
