/* linux/arch/arm/mach-exynos4/include/mach/smp.h
 *
 * Cloned from arch/arm/mach-realview/include/mach/smp.h
*/

#ifndef ASM_ARCH_SMP_H
#define ASM_ARCH_SMP_H __FILE__

#include <asm/hardware/gic.h>

/*
 * We use IRQ1 as the IPI
 */
static inline void smp_cross_call(const struct cpumask *mask, int ipi)
{
	gic_raise_softirq(mask, ipi);
}

#endif
