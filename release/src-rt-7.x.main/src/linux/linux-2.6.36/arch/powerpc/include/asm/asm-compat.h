#ifndef _ASM_POWERPC_ASM_COMPAT_H
#define _ASM_POWERPC_ASM_COMPAT_H

#include <asm/types.h>
#include <asm/ppc-opcode.h>

#ifdef __ASSEMBLY__
#  define stringify_in_c(...)	__VA_ARGS__
#  define ASM_CONST(x)		x
#else
/* This version of stringify will deal with commas... */
#  define __stringify_in_c(...)	#__VA_ARGS__
#  define stringify_in_c(...)	__stringify_in_c(__VA_ARGS__) " "
#  define __ASM_CONST(x)	x##UL
#  define ASM_CONST(x)		__ASM_CONST(x)
#endif


#ifdef __powerpc64__

/* operations for longs and pointers */
#define PPC_LL		stringify_in_c(ld)
#define PPC_STL		stringify_in_c(std)
#define PPC_STLU	stringify_in_c(stdu)
#define PPC_LCMPI	stringify_in_c(cmpdi)
#define PPC_LONG	stringify_in_c(.llong)
#define PPC_LONG_ALIGN	stringify_in_c(.balign 8)
#define PPC_TLNEI	stringify_in_c(tdnei)
#define PPC_LLARX(t, a, b, eh)	PPC_LDARX(t, a, b, eh)
#define PPC_STLCX	stringify_in_c(stdcx.)
#define PPC_CNTLZL	stringify_in_c(cntlzd)
#define PPC_LR_STKOFF	16
#define PPC_MIN_STKFRM	112

/* Move to CR, single-entry optimized version. Only available
 * on POWER4 and later.
 */
#ifdef CONFIG_POWER4_ONLY
#define PPC_MTOCRF	stringify_in_c(mtocrf)
#else
#define PPC_MTOCRF	stringify_in_c(mtcrf)
#endif

#else /* 32-bit */

/* operations for longs and pointers */
#define PPC_LL		stringify_in_c(lwz)
#define PPC_STL		stringify_in_c(stw)
#define PPC_STLU	stringify_in_c(stwu)
#define PPC_LCMPI	stringify_in_c(cmpwi)
#define PPC_LONG	stringify_in_c(.long)
#define PPC_LONG_ALIGN	stringify_in_c(.balign 4)
#define PPC_TLNEI	stringify_in_c(twnei)
#define PPC_LLARX(t, a, b, eh)	PPC_LWARX(t, a, b, eh)
#define PPC_STLCX	stringify_in_c(stwcx.)
#define PPC_CNTLZL	stringify_in_c(cntlzw)
#define PPC_MTOCRF	stringify_in_c(mtcrf)
#define PPC_LR_STKOFF	4
#define PPC_MIN_STKFRM	16

#endif

#ifdef __KERNEL__
#ifdef CONFIG_IBM405_ERR77
/* Erratum #77 on the 405 means we need a sync or dcbt before every
 * stwcx.  The old ATOMIC_SYNC_FIX covered some but not all of this.
 */
#define PPC405_ERR77(ra,rb)	stringify_in_c(dcbt	ra, rb;)
#define	PPC405_ERR77_SYNC	stringify_in_c(sync;)
#else
#define PPC405_ERR77(ra,rb)
#define PPC405_ERR77_SYNC
#endif
#endif

#endif /* _ASM_POWERPC_ASM_COMPAT_H */
