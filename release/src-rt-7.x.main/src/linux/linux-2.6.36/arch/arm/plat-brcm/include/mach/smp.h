/*
 * derived from arch/arm/mach-realview/include/mach/smp.h
 *
 * This file is required from common architecture code,
 * in arch/arm/include/asm/smp.h
 */

#ifndef __ASM_ARCH_SMP_H
#define __ASM_ARCH_SMP_H __FILE__

#include <asm/hardware/gic.h>

extern void platform_secondary_startup(void);

/* Used in hotplug.c */
#define hard_smp_processor_id()			\
	({						\
		unsigned int cpunum;			\
		__asm__("mrc p15, 0, %0, c0, c0, 5"	\
			: "=r" (cpunum));		\
		cpunum &= 0x0F;				\
	})

/*
 * We use IRQ1 as the IPI
 */
static inline void smp_cross_call(const struct cpumask *mask)
{
	gic_raise_softirq(mask, 1);
}

#endif /* __ASM_ARCH_SMP_H */
