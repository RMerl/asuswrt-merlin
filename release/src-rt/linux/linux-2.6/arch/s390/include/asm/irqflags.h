/*
 *    Copyright IBM Corp. 2006,2010
 *    Author(s): Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#ifndef __ASM_IRQFLAGS_H
#define __ASM_IRQFLAGS_H

#include <linux/types.h>

/* store then OR system mask. */
#define __arch_local_irq_stosm(__or)					\
({									\
	unsigned long __mask;						\
	asm volatile(							\
		"	stosm	%0,%1"					\
		: "=Q" (__mask) : "i" (__or) : "memory");		\
	__mask;								\
})

/* store then AND system mask. */
#define __arch_local_irq_stnsm(__and)					\
({									\
	unsigned long __mask;						\
	asm volatile(							\
		"	stnsm	%0,%1"					\
		: "=Q" (__mask) : "i" (__and) : "memory");		\
	__mask;								\
})

/* set system mask. */
static inline void __arch_local_irq_ssm(unsigned long flags)
{
	asm volatile("ssm   %0" : : "Q" (flags) : "memory");
}

static inline unsigned long arch_local_save_flags(void)
{
	return __arch_local_irq_stosm(0x00);
}

static inline unsigned long arch_local_irq_save(void)
{
	return __arch_local_irq_stnsm(0xfc);
}

static inline void arch_local_irq_disable(void)
{
	arch_local_irq_save();
}

static inline void arch_local_irq_enable(void)
{
	__arch_local_irq_stosm(0x03);
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	__arch_local_irq_ssm(flags);
}

static inline bool arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & (3UL << (BITS_PER_LONG - 8)));
}

static inline bool arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

#endif /* __ASM_IRQFLAGS_H */
