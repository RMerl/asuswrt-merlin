/* Definition of `struct sigcontext' for Linux/ARM
   Copyright (C) 1996, 1997, 1998, 1999, 2000 Free Software Foundation, Inc.
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

/* The format of struct sigcontext changed between 2.0 and 2.1 kernels.
   Fortunately 2.0 puts a magic number in the first word and this is not
   a legal value for `trap_no', so we can tell them apart.  */

/* Early 2.2 and 2.3 kernels do not have the `fault_address' member in
   the sigcontext structure.  Unfortunately there is no reliable way
   to test for its presence and this word will contain garbage for too-old
   kernels.  Versions 2.2.14 and 2.3.35 (plus later versions) are known to
   include this element.  */

#ifndef __ARMSIGCTX_H
#define __ARMSIGCTX_H	1

#include <asm/ptrace.h>

union k_sigcontext
  {
    struct
      {
	unsigned long int trap_no;
	unsigned long int error_code;
	unsigned long int oldmask;
	unsigned long int arm_r0;
	unsigned long int arm_r1;
	unsigned long int arm_r2;
	unsigned long int arm_r3;
	unsigned long int arm_r4;
	unsigned long int arm_r5;
	unsigned long int arm_r6;
	unsigned long int arm_r7;
	unsigned long int arm_r8;
	unsigned long int arm_r9;
	unsigned long int arm_r10;
	unsigned long int arm_fp;
	unsigned long int arm_ip;
	unsigned long int arm_sp;
	unsigned long int arm_lr;
	unsigned long int arm_pc;
	unsigned long int arm_cpsr;
	unsigned long fault_address;
      } v21;
    struct
      {
	unsigned long int magic;
	struct pt_regs reg;
	unsigned long int trap_no;
	unsigned long int error_code;
	unsigned long int oldmask;
      } v20;
};

#define SIGCONTEXT_2_0_MAGIC	0x4B534154

#endif	/* bits/armsigctx.h */
