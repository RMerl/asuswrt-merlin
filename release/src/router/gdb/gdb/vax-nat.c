/* Native-dependent code for VAX UNIXen (including older BSD's).

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "inferior.h"

#include "gdb_assert.h"
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/user.h>

#ifdef HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>
#endif

#ifndef PT_READ_U
#define PT_READ_U 3
#endif

#ifdef SYS_REG_H
/* UNIX 32V and derivatives (including 3BSD).  */
#include <sys/reg.h>
#else
/* 4.2BSD and derivatives.  */
#include <machine/reg.h>
#endif

#include "vax-tdep.h"
#include "inf-ptrace.h"

/* Address of the user structure.  This is the the value for 32V; 3BSD
   uses a different value, but hey, who's still using those systems?  */
static CORE_ADDR vax_kernel_u_addr = 0x80020000;

/* Location of the user's stored registers; usage is `u.u_ar0[XX]'.
   For 4.2BSD and ULTRIX these are negative!  See <machine/reg.h>.  */
static int vax_register_index[] =
{
  R0, R1, R2, R3, R4, R5,
  R6, R7, R8, R9, R10, R11,
  AP, FP, SP, PC, PS
};

static CORE_ADDR
vax_register_u_addr (CORE_ADDR u_ar0, int regnum)
{
  gdb_assert (regnum >= 0 && regnum < ARRAY_SIZE (vax_register_index));

  /* Type is `int *u_ar0'.  See <sys/user.h>.  */
  return u_ar0 + vax_register_index[regnum - VAX_R0_REGNUM] * 4;
}

static CORE_ADDR
vax_register_u_offset (struct gdbarch *gdbarch, int regnum, int store_p)
{
  size_t u_ar0_offset = offsetof (struct user, u_ar0);
  CORE_ADDR u_ar0;
  int pid;

  errno = 0;
  pid = PIDGET (inferior_ptid);
  u_ar0 = ptrace (PT_READ_U, pid, u_ar0_offset, 0);
  if (errno)
    perror_with_name (_("Unable to determine location of registers"));

  return vax_register_u_addr (u_ar0, regnum) - vax_kernel_u_addr;
}


#include <nlist.h>

#ifndef _PATH_UNIX
#define _PATH_UNIX "/vmunix"
#endif

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_vax_nat (void);

void
_initialize_vax_nat (void)
{
  struct nlist names[2];

  names[0].n_name = "_u";
  names[1].n_name = NULL;
  if (nlist (_PATH_UNIX, names) == 0)
    vax_kernel_u_addr = names[0].n_value;

  add_target (inf_ptrace_trad_target (vax_register_u_offset));
}
