/* spinlock.h: 32-bit Sparc spinlock support.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC_SPINLOCK_H
#define __SPARC_SPINLOCK_H

#ifndef __ASSEMBLY__

#include <asm/psr.h>

#define arch_spin_is_locked(lock) (*((volatile unsigned char *)(lock)) != 0)

#define arch_spin_unlock_wait(lock) \
	do { while (arch_spin_is_locked(lock)) cpu_relax(); } while (0)

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	__asm__ __volatile__(
	"\n1:\n\t"
	"ldstub	[%0], %%g2\n\t"
	"orcc	%%g2, 0x0, %%g0\n\t"
	"bne,a	2f\n\t"
	" ldub	[%0], %%g2\n\t"
	".subsection	2\n"
	"2:\n\t"
	"orcc	%%g2, 0x0, %%g0\n\t"
	"bne,a	2b\n\t"
	" ldub	[%0], %%g2\n\t"
	"b,a	1b\n\t"
	".previous\n"
	: /* no outputs */
	: "r" (lock)
	: "g2", "memory", "cc");
}

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int result;
	__asm__ __volatile__("ldstub [%1], %0"
			     : "=r" (result)
			     : "r" (lock)
			     : "memory");
	return (result == 0);
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	__asm__ __volatile__("stb %%g0, [%0]" : : "r" (lock) : "memory");
}

static inline void __arch_read_lock(arch_rwlock_t *rw)
{
	register arch_rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_read_enter\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
}

#define arch_read_lock(lock) \
do {	unsigned long flags; \
	local_irq_save(flags); \
	__arch_read_lock(lock); \
	local_irq_restore(flags); \
} while(0)

static inline void __arch_read_unlock(arch_rwlock_t *rw)
{
	register arch_rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_read_exit\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
}

#define arch_read_unlock(lock) \
do {	unsigned long flags; \
	local_irq_save(flags); \
	__arch_read_unlock(lock); \
	local_irq_restore(flags); \
} while(0)

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	register arch_rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_write_enter\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
	*(volatile __u32 *)&lp->lock = ~0U;
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned int val;

	__asm__ __volatile__("ldstub [%1 + 3], %0"
			     : "=r" (val)
			     : "r" (&rw->lock)
			     : "memory");

	if (val == 0) {
		val = rw->lock & ~0xff;
		if (val)
			((volatile u8*)&rw->lock)[3] = 0;
		else
			*(volatile u32*)&rw->lock = ~0U;
	}

	return (val == 0);
}

static inline int __arch_read_trylock(arch_rwlock_t *rw)
{
	register arch_rwlock_t *lp asm("g1");
	register int res asm("o0");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_read_try\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: "=r" (res)
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
	return res;
}

#define arch_read_trylock(lock) \
({	unsigned long flags; \
	int res; \
	local_irq_save(flags); \
	res = __arch_read_trylock(lock); \
	local_irq_restore(flags); \
	res; \
})

#define arch_write_unlock(rw)	do { (rw)->lock = 0; } while(0)

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)
#define arch_read_lock_flags(rw, flags)   arch_read_lock(rw)
#define arch_write_lock_flags(rw, flags)  arch_write_lock(rw)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#define arch_read_can_lock(rw) (!((rw)->lock & 0xff))
#define arch_write_can_lock(rw) (!(rw)->lock)

#endif /* !(__ASSEMBLY__) */

#endif /* __SPARC_SPINLOCK_H */
