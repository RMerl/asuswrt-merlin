#ifndef __ASM_BUG_H
#define __ASM_BUG_H

#include <linux/compiler.h>
#include <asm/sgidefs.h>

#ifdef CONFIG_BUG

#include <asm/break.h>

static inline void __noreturn BUG(void)
{
#ifdef CONFIG_CPU_MICROMIPS
	__asm__ __volatile__("break %0" : : "i" (MM_BRK_BUG));
#else
	__asm__ __volatile__("break %0" : : "i" (BRK_BUG));
#endif
	unreachable();
}

#define HAVE_ARCH_BUG

#if (_MIPS_ISA > _MIPS_ISA_MIPS1)

static inline void  __BUG_ON(unsigned long condition)
{
	if (__builtin_constant_p(condition)) {
		if (condition)
			BUG();
		else
			return;
	}
	__asm__ __volatile__("tne $0, %0, %1"
#ifdef CONFIG_CPU_MICROMIPS
			     : : "r" (condition), "i" (MM_BRK_BUG));
#else
			     : : "r" (condition), "i" (BRK_BUG));
#endif
}

#define BUG_ON(C) __BUG_ON((unsigned long)(C))

#define HAVE_ARCH_BUG_ON

#endif /* _MIPS_ISA > _MIPS_ISA_MIPS1 */

#endif

#include <asm-generic/bug.h>

#endif /* __ASM_BUG_H */
