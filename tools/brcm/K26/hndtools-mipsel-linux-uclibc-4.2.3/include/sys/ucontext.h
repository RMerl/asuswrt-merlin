/* Copyright (C) 1997, 1998, 2000, 2003, 2004, 2006 Free Software
   Foundation, Inc.  This file is part of the GNU C Library.

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

/* Don't rely on this, the interface is currently messed up and may need to
   be broken to be fixed.  */
#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H	1

#include <features.h>
#include <sgidefs.h>
#include <signal.h>

/* We need the signal context definitions even if they are not used
   included in <signal.h>.  */
#include <bits/sigcontext.h>

/* Type for general register.  Even in o32 we assume 64-bit registers,
   like the kernel.  */
__extension__ typedef unsigned long long int greg_t;

/* Number of general registers.  */
#define NGREG	32
#define NFPREG	32

/* Container for all general registers.  */
typedef greg_t gregset_t[NGREG];

/* Container for all FPU registers.  */
typedef struct fpregset {
	union {
		double	fp_dregs[NFPREG];
		struct {
			float		_fp_fregs;
			unsigned int	_fp_pad;
		} fp_fregs[NFPREG];
	} fp_r;
} fpregset_t;


/* Context to describe whole processor state.  */
#if _MIPS_SIM == _ABIO32
/* Earlier versions of glibc for mips had an entirely different
   definition of mcontext_t, that didn't even resemble the
   corresponding kernel data structure.  Since all legitimate uses of
   ucontext_t in glibc mustn't have accessed anything beyond
   uc_mcontext and, even then, taking a pointer to it, casting it to
   sigcontext_t, and accessing it as such, which is what it has always
   been, this can still be rectified.  Fortunately, makecontext,
   [gs]etcontext et all have never been implemented.  */
typedef struct
  {
    unsigned int regmask;
    unsigned int status;
    greg_t pc;
    gregset_t gregs;
    fpregset_t fpregs;
    unsigned int fp_owned;
    unsigned int fpc_csr;
    unsigned int fpc_eir;
    unsigned int used_math;
    unsigned int dsp;
    greg_t mdhi;
    greg_t mdlo;
    unsigned long hi1;
    unsigned long lo1;
    unsigned long hi2;
    unsigned long lo2;
    unsigned long hi3;
    unsigned long lo3;
  } mcontext_t;
#else
typedef struct
  {
    gregset_t gregs;
    fpregset_t fpregs;
    greg_t mdhi;
    greg_t hi1;
    greg_t hi2;
    greg_t hi3;
    greg_t mdlo;
    greg_t lo1;
    greg_t lo2;
    greg_t lo3;
    greg_t pc;
    unsigned int fpc_csr;
    unsigned int used_math;
    unsigned int dsp;
    unsigned int reserved;
  } mcontext_t;
#endif

/* Userlevel context.  */
typedef struct ucontext
  {
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
  } ucontext_t;

#endif /* sys/ucontext.h */
