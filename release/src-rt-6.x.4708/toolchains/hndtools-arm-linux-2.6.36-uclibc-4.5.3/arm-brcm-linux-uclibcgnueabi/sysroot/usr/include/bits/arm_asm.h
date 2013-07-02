/* Various definitons used the the ARM uClibc assembly code.  */
#ifndef _ARM_ASM_H
#define _ARM_ASM_H

#ifdef __thumb2__
.thumb
.syntax unified
#define IT(t, cond) i##t cond
#else
/* XXX: This can be removed if/when we require an assembler that supports
   unified assembly syntax.  */
#define IT(t, cond)
/* Code to return from a thumb function stub.  */
#ifdef __ARM_ARCH_4T__
#define POP_RET pop	{r2, pc}
#else
#define POP_RET pop	{r2, r3}; bx	r3
#endif
#endif

#if defined(__ARM_ARCH_6M__)
/* Force arm mode to flush out errors on M profile cores.  */
#undef IT
#define THUMB1_ONLY 1
#endif

#endif /* _ARM_ASM_H */

