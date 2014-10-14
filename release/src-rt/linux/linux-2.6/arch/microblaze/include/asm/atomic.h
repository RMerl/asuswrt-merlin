#ifndef _ASM_MICROBLAZE_ATOMIC_H
#define _ASM_MICROBLAZE_ATOMIC_H

#include <asm-generic/atomic.h>

/*
 * Atomically test *v and decrement if it is greater than 0.
 * The function returns the old value of *v minus 1.
 */
static inline int atomic_dec_if_positive(atomic_t *v)
{
	unsigned long flags;
	int res;

	local_irq_save(flags);
	res = v->counter - 1;
	if (res >= 0)
		v->counter = res;
	local_irq_restore(flags);

	return res;
}

#endif /* _ASM_MICROBLAZE_ATOMIC_H */
