/* FPU control word definitions.  ARM version.
   Copyright (C) 1996, 1997, 1998, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _FPU_CONTROL_H
#define _FPU_CONTROL_H

#ifdef __VFP_FP__

/* masking of interrupts */
#define _FPU_MASK_IM	0x00000100	/* invalid operation */
#define _FPU_MASK_ZM	0x00000200	/* divide by zero */
#define _FPU_MASK_OM	0x00000400	/* overflow */
#define _FPU_MASK_UM	0x00000800	/* underflow */
#define _FPU_MASK_PM	0x00001000	/* inexact */

/* Some bits in the FPSCR are not yet defined.  They must be preserved when
   modifying the contents.  */
#define _FPU_RESERVED	0x0e08e0e0
#define _FPU_DEFAULT    0x00000000
/* Default + exceptions enabled. */
#define _FPU_IEEE	(_FPU_DEFAULT | 0x00001f00)

/* Type of the control word.  */
typedef unsigned int fpu_control_t;

/* Macros for accessing the hardware control word.  */
/* This is fmrx %0, fpscr.  */
#define _FPU_GETCW(cw) \
  __asm__ __volatile__ ("mrc p10, 7, %0, cr1, cr0, 0" : "=r" (cw))
/* This is fmxr fpscr, %0.  */
#define _FPU_SETCW(cw) \
  __asm__ __volatile__ ("mcr p10, 7, %0, cr1, cr0, 0" : : "r" (cw))

#elif defined __MAVERICK__

/* DSPSC register: (from EP9312 User's Guide)
 *
 * bits 31..29	- DAID
 * bits 28..26	- HVID
 * bits 25..24	- RSVD
 * bit  23	- ISAT
 * bit  22	- UI
 * bit  21	- INT
 * bit  20	- AEXC
 * bits 19..18	- SAT
 * bits 17..16	- FCC
 * bit  15	- V
 * bit  14	- FWDEN
 * bit  13	- Invalid
 * bit	12	- Denorm
 * bits 11..10	- RM
 * bits 9..5	- IXE, UFE, OFE, RSVD, IOE
 * bits 4..0	- IX, UF, OF, RSVD, IO
 */

/* masking of interrupts */
#define _FPU_MASK_IM	(1 << 5)	/* invalid operation */
#define _FPU_MASK_ZM	0		/* divide by zero */
#define _FPU_MASK_OM	(1 << 7)	/* overflow */
#define _FPU_MASK_UM	(1 << 8)	/* underflow */
#define _FPU_MASK_PM	(1 << 9)	/* inexact */
#define _FPU_MASK_DM	0		/* denormalized operation */

#define _FPU_RESERVED	0xfffff000	/* These bits are reserved.  */

#define _FPU_DEFAULT	0x00b00000	/* Default value.  */
#define _FPU_IEEE	0x00b003a0	/* Default + exceptions enabled. */

/* Type of the control word.  */
typedef unsigned int fpu_control_t;

/* Macros for accessing the hardware control word.  */
#define _FPU_GETCW(cw) ({			\
	register int __t1, __t2;		\
						\
	__asm__ __volatile__ (			\
	"cfmvr64l	%1, mvdx0\n\t"		\
	"cfmvr64h	%2, mvdx0\n\t"		\
	"cfmv32sc	mvdx0, dspsc\n\t"	\
	"cfmvr64l	%0, mvdx0\n\t"		\
	"cfmv64lr	mvdx0, %1\n\t"		\
	"cfmv64hr	mvdx0, %2"		\
	: "=r" (cw), "=r" (__t1), "=r" (__t2)	\
	);					\
})

#define _FPU_SETCW(cw) ({			\
	register int __t0, __t1, __t2;		\
						\
	__asm__ __volatile__ (			\
	"cfmvr64l	%1, mvdx0\n\t"		\
	"cfmvr64h	%2, mvdx0\n\t"		\
	"cfmv64lr	mvdx0, %0\n\t"		\
	"cfmvsc32	dspsc, mvdx0\n\t"	\
	"cfmv64lr	mvdx0, %1\n\t"		\
	"cfmv64hr	mvdx0, %2"		\
	: "=r" (__t0), "=r" (__t1), "=r" (__t2)	\
	: "0" (cw)				\
	);					\
})

#else /* !__MAVERICK__ */

/* We have a slight terminology confusion here.  On the ARM, the register
 * we're interested in is actually the FPU status word - the FPU control
 * word is something different (which is implementation-defined and only
 * accessible from supervisor mode.)
 *
 * The FPSR looks like this:
 *
 *     31-24        23-16          15-8              7-0
 * | system ID | trap enable | system control | exception flags |
 *
 * We ignore the system ID bits; for interest's sake they are:
 *
 *  0000	"old" FPE
 *  1000	FPPC hardware
 *  0001	FPE 400
 *  1001	FPA hardware
 *
 * The trap enable and exception flags are both structured like this:
 *
 *     7 - 5     4     3     2     1     0
 * | reserved | INX | UFL | OFL | DVZ | IVO |
 *
 * where a `1' bit in the enable byte means that the trap can occur, and
 * a `1' bit in the flags byte means the exception has occurred.
 *
 * The exceptions are:
 *
 *  IVO - invalid operation
 *  DVZ - divide by zero
 *  OFL - overflow
 *  UFL - underflow
 *  INX - inexact (do not use; implementations differ)
 *
 * The system control byte looks like this:
 *
 *     7-5      4    3    2    1    0
 * | reserved | AC | EP | SO | NE | ND |
 *
 * where the bits mean
 *
 *  ND - no denormalised numbers (force them all to zero)
 *  NE - enable NaN exceptions
 *  SO - synchronous operation
 *  EP - use expanded packed-decimal format
 *  AC - use alternate definition for C flag on compare operations
 */

/* masking of interrupts */
#define _FPU_MASK_IM	0x00010000	/* invalid operation */
#define _FPU_MASK_ZM	0x00020000	/* divide by zero */
#define _FPU_MASK_OM	0x00040000	/* overflow */
#define _FPU_MASK_UM	0x00080000	/* underflow */
#define _FPU_MASK_PM	0x00100000	/* inexact */
#define _FPU_MASK_DM	0x00000000	/* denormalized operation */

/* The system id bytes cannot be changed.
   Only the bottom 5 bits in the trap enable byte can be changed.
   Only the bottom 5 bits in the system control byte can be changed.
   Only the bottom 5 bits in the exception flags are used.
   The exception flags are set by the fpu, but can be zeroed by the user. */
#define _FPU_RESERVED	0xffe0e0e0	/* These bits are reserved.  */

/* The fdlibm code requires strict IEEE double precision arithmetic,
   no interrupts for exceptions, rounding to nearest.  Changing the
   rounding mode will break long double I/O.  Turn on the AC bit,
   the compiler generates code that assumes it is on.  */
#define _FPU_DEFAULT	0x00001000	/* Default value.  */
#define _FPU_IEEE	0x001f1000	/* Default + exceptions enabled. */

/* Type of the control word.  */
typedef unsigned int fpu_control_t;

/* Macros for accessing the hardware control word.  */
#define _FPU_GETCW(cw) __asm__ ("rfs %0" : "=r" (cw))
#define _FPU_SETCW(cw) __asm__ ("wfs %0" : : "r" (cw))

#endif /* __MAVERICK__ */

#if 0
/* Default control word set at startup.  */
extern fpu_control_t __fpu_control;
#endif

#endif /* _FPU_CONTROL_H */
