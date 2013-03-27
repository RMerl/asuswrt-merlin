/* Native-dependent code for Motorola 68000 BSD's.

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
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"

#include "gdb_assert.h"
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "m68k-tdep.h"
#include "inf-ptrace.h"

static int
m68kbsd_gregset_supplies_p (int regnum)
{
  return (regnum >= M68K_D0_REGNUM && regnum <= M68K_PC_REGNUM);
}

static int
m68kbsd_fpregset_supplies_p (int regnum)
{
  return (regnum >= M68K_FP0_REGNUM && regnum <= M68K_FPI_REGNUM);
}

/* Supply the general-purpose registers stored in GREGS to REGCACHE.  */

static void
m68kbsd_supply_gregset (struct regcache *regcache, const void *gregs)
{
  const char *regs = gregs;
  int regnum;

  for (regnum = M68K_D0_REGNUM; regnum <= M68K_PC_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum, regs + regnum * 4);
}

/* Supply the floating-point registers stored in FPREGS to REGCACHE.  */

static void
m68kbsd_supply_fpregset (struct regcache *regcache, const void *fpregs)
{
  const char *regs = fpregs;
  int regnum;

  for (regnum = M68K_FP0_REGNUM; regnum <= M68K_FPI_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum,
			 regs + m68kbsd_fpreg_offset (regnum));
}

/* Collect the general-purpose registers from REGCACHE and store them
   in GREGS.  */

static void
m68kbsd_collect_gregset (const struct regcache *regcache,
			 void *gregs, int regnum)
{
  char *regs = gregs;
  int i;

  for (i = M68K_D0_REGNUM; i <= M68K_PC_REGNUM; i++)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_collect (regcache, i, regs + i * 4);
    }
}

/* Collect the floating-point registers from REGCACHE and store them
   in FPREGS.  */

static void
m68kbsd_collect_fpregset (struct regcache *regcache,
			  void *fpregs, int regnum)
{
  char *regs = fpregs;
  int i;

  for (i = M68K_FP0_REGNUM; i <= M68K_FPI_REGNUM; i++)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_collect (regcache, i, regs + m68kbsd_fpreg_offset (i));
    }
}


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers (including the floating-point registers).  */

static void
m68kbsd_fetch_inferior_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1 || m68kbsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &regs, 0) == -1)
	perror_with_name (_("Couldn't get registers"));

      m68kbsd_supply_gregset (regcache, &regs);
    }

  if (regnum == -1 || m68kbsd_fpregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      m68kbsd_supply_fpregset (regcache, &fpregs);
    }
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating-point registers).  */

static void
m68kbsd_store_inferior_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1 || m68kbsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
                  (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name (_("Couldn't get registers"));

      m68kbsd_collect_gregset (regcache, &regs, regnum);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	          (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name (_("Couldn't write registers"));
    }

  if (regnum == -1 || m68kbsd_fpregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      m68kbsd_collect_fpregset (regcache, &fpregs, regnum);

      if (ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't write floating point status"));
    }
}


/* Support for debugging kernel virtual memory images.  */

#include <sys/types.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

/* OpenBSD doesn't have these.  */
#ifndef PCB_REGS_FP
#define PCB_REGS_FP 10
#endif
#ifndef PCB_REGS_SP
#define PCB_REGS_SP 11
#endif

static int
m68kbsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  int regnum, tmp;
  int i = 0;

  /* The following is true for NetBSD 1.6.2:

     The pcb contains %d2...%d7, %a2...%a7 and %ps.  This accounts for
     all callee-saved registers.  From this information we reconstruct
     the register state as it would look when we just returned from
     cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_regs[PCB_REGS_SP] == 0)
    return 0;

  for (regnum = M68K_D2_REGNUM; regnum <= M68K_D7_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum, &pcb->pcb_regs[i++]);
  for (regnum = M68K_A2_REGNUM; regnum <= M68K_SP_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum, &pcb->pcb_regs[i++]);

  tmp = pcb->pcb_ps & 0xffff;
  regcache_raw_supply (regcache, M68K_PS_REGNUM, &tmp);

  read_memory (pcb->pcb_regs[PCB_REGS_FP] + 4, (char *) &tmp, sizeof tmp);
  regcache_raw_supply (regcache, M68K_PC_REGNUM, &tmp);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m68kbsd_nat (void);

void
_initialize_m68kbsd_nat (void)
{
  struct target_ops *t;

  t = inf_ptrace_target ();
  t->to_fetch_registers = m68kbsd_fetch_inferior_registers;
  t->to_store_registers = m68kbsd_store_inferior_registers;
  add_target (t);

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (m68kbsd_supply_pcb);
}
