/* Copyright (C) 1997, 1998 Free Software Foundation, Inc.
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

/* Define the machine-dependent type `jmp_buf'.  ARM version. */
#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H	1

#if !defined _SETJMP_H && !defined _PTHREAD_H
# error "Never include <bits/setjmp.h> directly; use <setjmp.h> instead."
#endif

#ifndef _ASM
/* Jump buffer contains v1-v6, sl, fp, sp and pc.  Other registers are not
   saved.  */
#ifdef __ARM_EABI__
/* The exact set of registers saved may depend on the particular core
   in use, as some coprocessor registers may need to be saved.  The C
   Library ABI requires that the buffer be 8-byte aligned, and
   recommends that the buffer contain 64 words.  The first 28 words
   are occupied by v1-v6, sl, fp, sp, pc, d8-d15, and fpscr.  (Note
   that d8-15 require 17 words, due to the use of fstmx.)  */
typedef int __jmp_buf[64] __attribute__((aligned (8)));
#elif defined __MAVERICK__ || defined __IWMMXT__
typedef int __jmp_buf[34];
#else
typedef int __jmp_buf[22];
#endif
#endif

#define __JMP_BUF_SP		8

/* Test if longjmp to JMPBUF would unwind the frame
   containing a local variable at ADDRESS.  */
#define _JMPBUF_UNWINDS(jmpbuf, address) \
  ((void *) (address) < (void *) (jmpbuf[__JMP_BUF_SP]))

#endif	/* bits/setjmp.h */
