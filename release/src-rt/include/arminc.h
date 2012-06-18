/*
 * HND Run Time Environment for standalone ARM programs.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: arminc.h,v 13.10 2010-01-15 01:13:59 Exp $
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

#if defined(__ARM_ARCH_4T__)
/* arm7tdmi-s */
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
#define	PS_Q		0x08000000		/* DSP Ov/Sat */
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

#endif	/* __ARM_ARCH_4T__ */

#if defined(__ARM_ARCH_7M__)
/* cortex-m3 */
/* Interrupt enable/disable register */
#define INTEN_REG1	0xe000e100
#define INTDIS_REG1	0xe000e180

/* CPUID */
#define	CM3_CPUID	0xe000ed00
#define	CM3_VTOFF	0xe000ed08
#define	CM3_SYSCTRL	0xe000ed10
#define	CM3_CFGCTRL	0xe000ed14

#define	CM3_PFR0	0xe000ed40
#define	CM3_PFR1	0xe000ed44
#define	CM3_DFR0	0xe000ed48
#define	CM3_AFR0	0xe000ed4c
#define	CM3_MMFR0	0xe000ed50
#define	CM3_MMFR1	0xe000ed54
#define	CM3_MMFR2	0xe000ed58
#define	CM3_MMFR3	0xe000ed5c
#define	CM3_ISAR0	0xe000ed60
#define	CM3_ISAR1	0xe000ed64
#define	CM3_ISAR2	0xe000ed68
#define	CM3_ISAR3	0xe000ed6c
#define	CM3_ISAR4	0xe000ed70
#define	CM3_ISAR5	0xe000ed74

#define	CM3_MPUTYPE	0xe000ed90
#define	CM3_MPUCTRL	0xe000ed94
#define	CM3_REGNUM	0xe000ed98
#define	CM3_REGBAR	0xe000ed9c
#define	CM3_REGASZ	0xe000eda0
#define	CM3_AL1BAR	0xe000eda4
#define	CM3_AL1ASZ	0xe000eda8
#define	CM3_AL2BAR	0xe000edac
#define	CM3_AL2ASZ	0xe000edb0
#define	CM3_AL3BAR	0xe000edb4
#define	CM3_AL3ASZ	0xe000edb8

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

/* used to fill an overlay region with nop's */
#define NOP_UINT32	0x46c046c0

#endif	/* __ARM_ARCH_7M__ */

/* Pieces of a CPU Id */
#define CID_IMPL	0xff000000		/* Implementor: 0x41 for ARM Ltd. */
#define CID_VARIANT	0x00f00000
#define CID_ARCH	0x000f0000
#define CID_PART	0x0000fff0
#define CID_REV		0x0000000f
#define CID_MASK	(CID_IMPL | CID_ARCH | CID_PART)

#endif	/* _ARMINC_H */
