/* Native-dependent code for Solaris x86.

   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "regcache.h"

#include <sys/procfs.h>
#include "gregset.h"

/* This file provids the (temporary) glue between the Solaris x86
   target dependent code and the machine independent SVR4 /proc
   support.  */

/* Solaris 10 (Solaris 2.10, SunOS 5.10) and up support two process
   data models, the traditional 32-bit data model (ILP32) and the
   64-bit data model (LP64).  The format of /proc depends on the data
   model of the observer (the controlling process, GDB in our case).
   The Solaris header files conveniently define PR_MODEL_NATIVE to the
   data model of the controlling process.  If its value is
   PR_MODEL_LP64, we know that GDB is being compiled as a 64-bit
   program.

   Note that a 32-bit GDB won't be able to debug a 64-bit target
   process using /proc on Solaris.  */

#if defined (PR_MODEL_NATIVE) && (PR_MODEL_NATIVE == PR_MODEL_LP64)

#include "amd64-nat.h"
#include "amd64-tdep.h"

/* Mapping between the general-purpose registers in gregset_t format
   and GDB's register cache layout.  */

/* From <sys/regset.h>.  */
static int amd64_sol2_gregset64_reg_offset[] = {
  14 * 8,			/* %rax */
  11 * 8,			/* %rbx */
  13 * 8,			/* %rcx */
  12 * 8,			/* %rdx */
  9 * 8,			/* %rsi */
  8 * 8,			/* %rdi */
  10 * 8,			/* %rbp */
  20 * 8,			/* %rsp */
  7 * 8,			/* %r8 ... */
  6 * 8,
  5 * 8,
  4 * 8,
  3 * 8,
  2 * 8,
  1 * 8,
  0 * 8,			/* ... %r15 */
  17 * 8,			/* %rip */
  16 * 8,			/* %eflags */
  18 * 8,			/* %cs */
  21 * 8,			/* %ss */
  25 * 8,			/* %ds */
  24 * 8,			/* %es */
  22 * 8,			/* %fs */
  23 * 8			/* %gs */
};

/* 32-bit registers are provided by Solaris in 64-bit format, so just
   give a subset of the list above.  */
static int amd64_sol2_gregset32_reg_offset[] = {
  14 * 8,			/* %eax */
  13 * 8,			/* %ecx */
  12 * 8,			/* %edx */
  11 * 8,			/* %ebx */
  20 * 8,			/* %esp */
  10 * 8,			/* %ebp */
  9 * 8,			/* %esi */
  8 * 8,			/* %edi */
  17 * 8,			/* %eip */
  16 * 8,			/* %eflags */
  18 * 8,			/* %cs */
  21 * 8,			/* %ss */
  25 * 8,			/* %ds */
  24 * 8,			/* %es */
  22 * 8,			/* %fs */
  23 * 8			/* %gs */
};

void
supply_gregset (struct regcache *regcache, const prgregset_t *gregs)
{
  amd64_supply_native_gregset (regcache, gregs, -1);
}

void
supply_fpregset (struct regcache *regcache, const prfpregset_t *fpregs)
{
  amd64_supply_fxsave (regcache, -1, fpregs);
}

void
fill_gregset (const struct regcache *regcache,
	      prgregset_t *gregs, int regnum)
{
  amd64_collect_native_gregset (regcache, gregs, regnum);
}

void
fill_fpregset (const struct regcache *regcache,
	       prfpregset_t *fpregs, int regnum)
{
  amd64_collect_fxsave (regcache, regnum, fpregs);
}

#else

/* For 32-bit Solaris x86, we use the Unix SVR4 code in i386v4-nat.c.  */

#endif

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_amd64_sol2_nat (void);

void
_initialize_amd64_sol2_nat (void)
{
#if defined (PR_MODEL_NATIVE) && (PR_MODEL_NATIVE == PR_MODEL_LP64)
  amd64_native_gregset32_reg_offset = amd64_sol2_gregset32_reg_offset;
  amd64_native_gregset32_num_regs =
    ARRAY_SIZE (amd64_sol2_gregset32_reg_offset);
  amd64_native_gregset64_reg_offset = amd64_sol2_gregset64_reg_offset;
  amd64_native_gregset64_num_regs =
    ARRAY_SIZE (amd64_sol2_gregset64_reg_offset);
#endif
}
