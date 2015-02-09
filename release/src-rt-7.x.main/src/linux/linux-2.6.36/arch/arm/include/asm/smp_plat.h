/*
 * ARM specific SMP header, this contains our implementation
 * details.
 */
#ifndef __ASMARM_SMP_PLAT_H
#define __ASMARM_SMP_PLAT_H

#include <asm/cputype.h>

#ifdef CONFIG_BCM47XX
/*
 * Merged from Linux-2.6.37
 * Return true if we are running on a SMP platform
 */
static inline bool is_smp(void)
{
#ifndef CONFIG_SMP
	return false;
#elif defined(CONFIG_SMP_ON_UP)
	extern unsigned int smp_on_up;
	return !!smp_on_up;
#else
	return true;
#endif
}
#endif /* CONFIG_BCM47XX */

/* all SMP configurations have the extended CPUID registers */
static inline int tlb_ops_need_broadcast(void)
{
#ifdef CONFIG_BCM47XX
	/* Merged from Linux-2.6.37 */
	if (!is_smp())
		return 0;
#endif /* CONFIG_BCM47XX */
	return ((read_cpuid_ext(CPUID_EXT_MMFR3) >> 12) & 0xf) < 2;
}

#ifdef CONFIG_BCM47XX
/* Merged from Linux-2.6.37 */
#if !defined(CONFIG_SMP) || __LINUX_ARM_ARCH__ >= 7
#define cache_ops_need_broadcast()	0
#else
static inline int cache_ops_need_broadcast(void)
{
	if (!is_smp())
		return 0;

	return ((read_cpuid_ext(CPUID_EXT_MMFR3) >> 12) & 0xf) < 1;
}
#endif /* !defined(CONFIG_SMP) || __LINUX_ARM_ARCH__ >= 7 */
#else
static inline int cache_ops_need_broadcast(void)
{
	return ((read_cpuid_ext(CPUID_EXT_MMFR3) >> 12) & 0xf) < 1;
}
#endif /* CONFIG_BCM47XX */

#endif
