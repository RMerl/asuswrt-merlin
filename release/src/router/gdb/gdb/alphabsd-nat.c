/* Native-dependent code for Alpha BSD's.

   Copyright (C) 2000, 2001, 2002, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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
#include "regcache.h"

#include "alpha-tdep.h"
#include "alphabsd-tdep.h"
#include "inf-ptrace.h"

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#ifdef HAVE_SYS_PROCFS_H
#include <sys/procfs.h>
#endif

#ifndef HAVE_GREGSET_T
typedef struct reg gregset_t;
#endif

#ifndef HAVE_FPREGSET_T 
typedef struct fpreg fpregset_t; 
#endif 

#include "gregset.h"

/* Provide *regset() wrappers around the generic Alpha BSD register
   supply/fill routines.  */

void
supply_gregset (struct regcache *regcache, const gregset_t *gregsetp)
{
  alphabsd_supply_reg (regcache, (const char *) gregsetp, -1);
}

void
fill_gregset (const struct regcache *regcache, gregset_t *gregsetp, int regno)
{
  alphabsd_fill_reg (regcache, (char *) gregsetp, regno);
}

void
supply_fpregset (struct regcache *regcache, const fpregset_t *fpregsetp)
{
  alphabsd_supply_fpreg (regcache, (const char *) fpregsetp, -1);
}

void
fill_fpregset (const struct regcache *regcache, fpregset_t *fpregsetp, int regno)
{
  alphabsd_fill_fpreg (regcache, (char *) fpregsetp, regno);
}

/* Determine if PT_GETREGS fetches this register.  */

static int
getregs_supplies (int regno)
{
  return ((regno >= ALPHA_V0_REGNUM && regno <= ALPHA_ZERO_REGNUM)
	  || regno >= ALPHA_PC_REGNUM);
}

/* Fetch register REGNO from the inferior.  If REGNO is -1, do this
   for all registers (including the floating point registers).  */

static void
alphabsd_fetch_inferior_registers (struct regcache *regcache, int regno)
{
  if (regno == -1 || getregs_supplies (regno))
    {
      struct reg gregs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &gregs, 0) == -1)
	perror_with_name (_("Couldn't get registers"));

      alphabsd_supply_reg (regcache, (char *) &gregs, regno);
      if (regno != -1)
	return;
    }

  if (regno == -1 || regno >= gdbarch_fp0_regnum (current_gdbarch))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      alphabsd_supply_fpreg (regcache, (char *) &fpregs, regno);
    }
}

/* Store register REGNO back into the inferior.  If REGNO is -1, do
   this for all registers (including the floating point registers).  */

static void
alphabsd_store_inferior_registers (struct regcache *regcache, int regno)
{
  if (regno == -1 || getregs_supplies (regno))
    {
      struct reg gregs;
      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
                  (PTRACE_TYPE_ARG3) &gregs, 0) == -1)
        perror_with_name (_("Couldn't get registers"));

      alphabsd_fill_reg (regcache, (char *) &gregs, regno);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
                  (PTRACE_TYPE_ARG3) &gregs, 0) == -1)
        perror_with_name (_("Couldn't write registers"));

      if (regno != -1)
	return;
    }

  if (regno == -1 || regno >= gdbarch_fp0_regnum (current_gdbarch))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      alphabsd_fill_fpreg (regcache, (char *) &fpregs, regno);

      if (ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't write floating point status"));
    }
}


/* Support for debugging kernel virtual memory images.  */

#include <sys/types.h>
#include <sys/signal.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

static int
alphabsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  int regnum;

  /* The following is true for OpenBSD 3.9:

     The pcb contains the register state at the context switch inside
     cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_hw.apcb_ksp == 0)
    return 0;

  regcache_raw_supply (regcache, ALPHA_SP_REGNUM, &pcb->pcb_hw.apcb_ksp);

  for (regnum = ALPHA_S0_REGNUM; regnum < ALPHA_A0_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum,
			 &pcb->pcb_context[regnum - ALPHA_S0_REGNUM]);
  regcache_raw_supply (regcache, ALPHA_RA_REGNUM, &pcb->pcb_context[7]);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_alphabsd_nat (void);

void
_initialize_alphabsd_nat (void)
{
  struct target_ops *t;

  t = inf_ptrace_target ();
  t->to_fetch_registers = alphabsd_fetch_inferior_registers;
  t->to_store_registers = alphabsd_store_inferior_registers;
  add_target (t);

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (alphabsd_supply_pcb);
}
