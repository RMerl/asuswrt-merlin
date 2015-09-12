/*
 * HND Run Time Environment for standalone ARM programs.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: arminc.h 425360 2013-09-24 00:06:24Z $
 */

#ifndef	_ARMINC_H
#define _ARMINC_H


/* ARM defines */

#ifdef	_LANGUAGE_ASSEMBLY

/*
 * LEAF - declare leaf routine
 */
#define LEAF(function)				\
		.section .text.function, "ax";	\
		.global	function;		\
		.func	function;		\
function:

#define THUMBLEAF(function)			\
		.section .text.function, "ax";	\
		.global	function;		\
		.func	function;		\
		.thumb;				\
		.thumb_func;			\
function:

/*
 * END - mark end of function
 */
#define END(function)				\
		.ltorg;				\
		.endfunc;			\
		.size	function, . - function

#define	DW(var, val)			\
	.global	var;			\
	.type	var, %object;		\
	.size	var, 4;			\
	.align	2;			\
var:	.word	val


#define _ULCAST_

#else

/*
 * The following macros are especially useful for __asm__
 * inline assembler.
 */
#ifndef __STR
#define __STR(x) #x
#endif
#ifndef STR
#define STR(x) __STR(x)
#endif

#define _ULCAST_ (unsigned long)

#endif	/* _LANGUAGE_ASSEMBLY */

/*
 * Macro to count leading zeroes
 *
 */
#if defined(__GNUC__)
#define CLZ(x) __builtin_clzl(x)
#elif defined(__arm__)
#define CLZ(x) __clz(x)
#else
#ifndef WLOFFLD
#error "No buitlin CLZ known on this compiler platform"
#endif /* WLOFFLD */
#endif /* __GNUC__ */


#if defined(__ARM_ARCH_7M__)	/* Cortex-M3 */

/* Data Watchpoint and Trigger */
#define CM3_DWT_CTRL		0xe0001000
#define CM3_DWT_CYCCNT		0xe0001004
#define CM3_DWT_CPICNT		0xe0001008
#define CM3_DWT_EXCCNT		0xe000100c
#define CM3_DWT_SLEEPCNT	0xe0001010
#define CM3_DWT_LSUCNT		0xe0001014
#define CM3_DWT_FOLDCNT		0xe0001018
#define CM3_DWT_COMP0		0xe0001020
#define CM3_DWT_MASK0		0xe0001024
#define CM3_DWT_FUNCTION0	0xe0001028
#define CM3_DWT_COMP1		0xe0001030
#define CM3_DWT_MASK1		0xe0001034
#define CM3_DWT_FUNCTION1	0xe0001038
#define CM3_DWT_COMP2		0xe0001040
#define CM3_DWT_MASK2		0xe0001044
#define CM3_DWT_FUNCTION2	0xe0001048
#define CM3_DWT_COMP3		0xe0001050
#define CM3_DWT_MASK3		0xe0001054
#define CM3_DWT_FUNCTION3	0xe0001058

#define CM3_DWT_FUNCTION_DISAB		0
#define CM3_DWT_FUNCTION_WP_PCMATCH	4
#define CM3_DWT_FUNCTION_WP_READ	5
#define CM3_DWT_FUNCTION_WP_WRITE	6
#define CM3_DWT_FUNCTION_WP_RDWR	7

#define CM3_NVIC_IC_TYPE	0xe000e004	/* Interrupt Control Type Reg */
#define CM3_NVIC_TICK_CSR	0xe000e010	/* SysTick Control and Status Reg */
#define CM3_NVIC_TICK_CSR_COUNTFLAG		0x10000
#define CM3_NVIC_TICK_CSR_CLKSOURCE		0x4	/* Set for core clock, 0 for ext ref */
#define CM3_NVIC_TICK_CSR_TICKINT		0x2	/* Set for intr on count going 1 => 0 */
#define CM3_NVIC_TICK_CSR_ENABLE		0x1
#define CM3_NVIC_TICK_RLDVAL	0xe000e014	/* SysTick Reload Value Reg */
#define CM3_NVIC_TICK_CURVAL	0xe000e018	/* SysTick Current Value Reg */
#define CM3_NVIC_TICK_CALVAL	0xe000e01c	/* SysTick Calibration Value Reg */

/* Interrupt enable/disable register */
#define CM3_NVIC_IRQ_SET_EN0	0xe000e100	/* Irq 0 to 31 Set Enable Reg */
#define CM3_NVIC_IRQ_SET_EN(n)	(0xe000e100 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

#define CM3_NVIC_IRQ_CLR_EN0	0xe000e180	/* Irq 0 to 31 Clear Enable Reg [...] */
#define CM3_NVIC_IRQ_CLR_EN(n)	(0xe000e180 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

#define CM3_NVIC_IRQ_SET_PND0	0xe000e200	/* Irq 0 to 31 Set Pending Reg [...] */
#define CM3_NVIC_IRQ_SET_PND(n)	(0xe000e200 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

#define CM3_NVIC_IRQ_CLR_PND0	0xe000e280	/* Irq 0 to 31 Clear Pending Reg [...] */
#define CM3_NVIC_IRQ_CLR_PND(n)	(0xe000e280 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

#define CM3_NVIC_IRQ_ACT_BIT0	0xe000e300	/* Irq 0 to 31 Active Bit Reg [...] */
#define CM3_NVIC_IRQ_ACT_BIT(n)	(0xe000e300 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

#define CM3_NVIC_IRQ_PRIO0	0xe000e400	/* Irq 0 to 31 Priority Reg [...] */
#define CM3_NVIC_IRQ_PRIO(n)	(0xe000e400 + (n) * 4)	/* Irq 0-31, 32-63, ..., 224-239 */

/* CPU control */
#define	CM3_CPUID		0xe000ed00
#define	CM3_INTCTLSTATE		0xe000ed04
#define	CM3_VTOFF		0xe000ed08	/* Vector Table Offset */
#define	CM3_SYSCTRL		0xe000ed10
#define	CM3_CFGCTRL		0xe000ed14
#define	CM3_CFGCTRL_UNALIGN_TRP		0x8
#define	CM3_CFGCTRL_DIV_0_TRP		0x10
#define	CM3_CFGCTRL_STKALIGN		0x200

#define	CM3_PFR0		0xe000ed40
#define	CM3_PFR1		0xe000ed44
#define	CM3_DFR0		0xe000ed48
#define	CM3_AFR0		0xe000ed4c
#define	CM3_MMFR0		0xe000ed50
#define	CM3_MMFR1		0xe000ed54
#define	CM3_MMFR2		0xe000ed58
#define	CM3_MMFR3		0xe000ed5c
#define	CM3_ISAR0		0xe000ed60
#define	CM3_ISAR1		0xe000ed64
#define	CM3_ISAR2		0xe000ed68
#define	CM3_ISAR3		0xe000ed6c
#define	CM3_ISAR4		0xe000ed70
#define	CM3_ISAR5		0xe000ed74

#define	CM3_MPUTYPE		0xe000ed90
#define	CM3_MPUCTRL		0xe000ed94
#define	CM3_REGNUM		0xe000ed98
#define	CM3_REGBAR		0xe000ed9c
#define	CM3_REGASZ		0xe000eda0
#define	CM3_AL1BAR		0xe000eda4
#define	CM3_AL1ASZ		0xe000eda8
#define	CM3_AL2BAR		0xe000edac
#define	CM3_AL2ASZ		0xe000edb0
#define	CM3_AL3BAR		0xe000edb4
#define	CM3_AL3ASZ		0xe000edb8

#define CM3_DBG_HCSR		0xe000edf0	/* Debug Halting Control and Status Reg */
#define CM3_DBG_CRSR		0xe000edf4	/* Debug Core Register Selector Reg */
#define CM3_DBG_CRDR		0xe000edf8	/* Debug Core Register Data Reg */
#define CM3_DBG_EMCR		0xe000edfc	/* Debug Exception and Monitor Control Reg */
#define CM3_DBG_EMCR_TRCENA		(1U << 24)
#define CM3_DBG_EMCR_MON_EN		(1U << 16)

/* Trap types */
#define TR_RST		1			/* Reset */
#define TR_NMI		2			/* NMI */
#define TR_FAULT	3			/* Hard Fault */
#define TR_MM		4			/* Memory Management */
#define TR_BUS		5			/* Bus Fault */
#define TR_USAGE	6			/* Usage Fault */
#define TR_SVC		11			/* SVCall */
#define TR_DMON		12			/* Debug Monitor */
#define TR_PENDSV	14			/* PendSV */
#define TR_SYSTICK	15			/* SysTick */
#define TR_ISR		16			/* External Interrupts start here */

#define	TR_BAD		256			/* Bad trap: Not used by CM3 */

/* Offsets of automatically saved registers from sp upon trap */
#define CM3_TROFF_R0	0
#define CM3_TROFF_R1	4
#define CM3_TROFF_R2	8
#define CM3_TROFF_R3	12
#define CM3_TROFF_R12	16
#define CM3_TROFF_LR	20
#define CM3_TROFF_PC	24
#define CM3_TROFF_xPSR	28

#elif defined(__ARM_ARCH_7A__)	/* Cortex-A9 */
/* Fields in cpsr */
#define	PS_USR		0x00000010		/* Mode: User */
#define	PS_FIQ		0x00000011		/* Mode: FIQ */
#define	PS_IRQ		0x00000012		/* Mode: IRQ */
#define	PS_SVC		0x00000013		/* Mode: Supervisor */
#define	PS_ABT		0x00000017		/* Mode: Abort */
#define	PS_UND		0x0000001b		/* Mode: Undefined */
#define	PS_SYS		0x0000001f		/* Mode: System */
#define	PS_MM		0x0000001f		/* Mode bits mask */
#define	PS_T		0x00000020		/* Thumb mode */
#define	PS_F		0x00000040		/* FIQ disable */
#define	PS_I		0x00000080		/* IRQ disable */
#define	PS_A		0x00000100		/* Imprecise abort */
#define	PS_E		0x00000200		/* Endianess */
#define	PS_IT72		0x0000fc00		/* IT[7:2] */
#define	PS_GE		0x000f0000		/* IT[7:2] */
#define	PS_J		0x01000000		/* Java state */
#define	PS_IT10		0x06000000		/* IT[1:0] */
#define	PS_Q		0x08000000		/* Sticky overflow */
#define	PS_V		0x10000000		/* Overflow cc */
#define	PS_C		0x20000000		/* Carry cc */
#define	PS_Z		0x40000000		/* Zero cc */
#define	PS_N		0x80000000		/* Negative cc */

/* Trap types */
#define	TR_RST		0			/* Reset trap */
#define	TR_UND		1			/* Indefined instruction trap */
#define	TR_SWI		2			/* Software intrrupt */
#define	TR_IAB		3			/* Instruction fetch abort */
#define	TR_DAB		4			/* Data access abort */
#define	TR_BAD		5			/* Bad trap: Not used by ARM */
#define	TR_IRQ		6			/* Interrupt */
#define	TR_FIQ		7			/* Fast interrupt */

/*
 * Memory segments (32bit kernel mode addresses)
 */
#define PHYSADDR_MASK	0xffffffff

/*
 * Map an address to a certain kernel segment
 */
#undef PHYSADDR
#define PHYSADDR(a)	(_ULCAST_(a) & PHYSADDR_MASK)
#else	/* !__ARM_ARCH_7M__ */

/* Fields in cpsr */
#define	PS_USR		0x00000010		/* Mode: User */
#define	PS_FIQ		0x00000011		/* Mode: FIQ */
#define	PS_IRQ		0x00000012		/* Mode: IRQ */
#define	PS_SVC		0x00000013		/* Mode: Supervisor */
#define	PS_ABT		0x00000017		/* Mode: Abort */
#define	PS_UND		0x0000001b		/* Mode: Undefined */
#define	PS_SYS		0x0000001f		/* Mode: System */
#define	PS_MM		0x0000001f		/* Mode bits mask */
#define	PS_T		0x00000020		/* Thumb mode */
#define	PS_F		0x00000040		/* FIQ disable */
#define	PS_I		0x00000080		/* IRQ disable */
#define	PS_A		0x00000100		/* Imprecise abort */
#define	PS_E		0x00000200		/* Endianess */
#define	PS_IT72		0x0000fc00		/* IT[7:2] */
#define	PS_GE		0x000f0000		/* IT[7:2] */
#define	PS_J		0x01000000		/* Java state */
#define	PS_IT10		0x06000000		/* IT[1:0] */
#define	PS_Q		0x08000000		/* Sticky overflow */
#define	PS_V		0x10000000		/* Overflow cc */
#define	PS_C		0x20000000		/* Carry cc */
#define	PS_Z		0x40000000		/* Zero cc */
#define	PS_N		0x80000000		/* Negative cc */

/* Trap types */
#define	TR_RST		0			/* Reset trap */
#define	TR_UND		1			/* Indefined instruction trap */
#define	TR_SWI		2			/* Software intrrupt */
#define	TR_IAB		3			/* Instruction fetch abort */
#define	TR_DAB		4			/* Data access abort */
#define	TR_BAD		5			/* Bad trap: Not used by ARM */
#define	TR_IRQ		6			/* Interrupt */
#define	TR_FIQ		7			/* Fast interrupt */

#ifdef BCMDBG_ARMRST
#define	TR_ARMRST	0xF			/* Debug facility to trap Arm reset */
#endif

/* used to fill an overlay region with nop's */
#define NOP_UINT32	0x46c046c0


#define	mrc(cp, a, b, n)						\
({									\
	int __res;							\
	__asm__ __volatile__("\tmrc\tp"STR(cp)", 0, %0, c"STR(a)", c"STR(b)", "STR(n) \
				:"=r" (__res));				\
	__res;								\
})


#endif	/* !__ARM_ARCH_7M__ */

/* Pieces of a CPU Id */
#define CID_IMPL	0xff000000		/* Implementor: 0x41 for ARM Ltd. */
#define CID_VARIANT	0x00f00000
#define CID_ARCH	0x000f0000
#define CID_PART	0x0000fff0
#define CID_REV		0x0000000f
#define CID_MASK	(CID_IMPL | CID_ARCH | CID_PART)

#endif	/* _ARMINC_H */
