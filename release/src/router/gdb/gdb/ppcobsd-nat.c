/* Native-dependent code for OpenBSD/powerpc.

   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include <stddef.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include "ppc-tdep.h"
#include "ppcobsd-tdep.h"
#include "inf-ptrace.h"
#include "bsd-kvm.h"

/* OpenBSD/powerpc didn't have PT_GETFPREGS/PT_SETFPREGS until release
   4.0.  On older releases the floating-point registers are handled by
   PT_GETREGS/PT_SETREGS, but fpscr wasn't available..  */

#ifdef PT_GETFPREGS

/* Returns true if PT_GETFPREGS fetches this register.  */

static int
getfpregs_supplies (int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the NetBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  return ((regnum >= tdep->ppc_fp0_regnum
           && regnum < tdep->ppc_fp0_regnum + ppc_num_fprs)
	  || regnum == tdep->ppc_fpscr_regnum);
}

#endif /* PT_GETFPREGS */

/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
ppcobsd_fetch_registers (struct regcache *regcache, int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  ppc_supply_gregset (&ppcobsd_gregset, regcache, -1,
		      &regs, sizeof regs);
#ifndef PT_GETFPREGS
  ppc_supply_fpregset (&ppcobsd_gregset, regcache, -1,
		       &regs, sizeof regs);
#endif

#ifdef PT_GETFPREGS
  if (regnum == -1 || getfpregs_supplies (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      ppc_supply_fpregset (&ppcobsd_fpregset, regcache, -1,
			   &fpregs, sizeof fpregs);
    }
#endif
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

static void
ppcobsd_store_registers (struct regcache *regcache, int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  ppc_collect_gregset (&ppcobsd_gregset, regcache,
		       regnum, &regs, sizeof regs);
#ifndef PT_GETFPREGS
  ppc_collect_fpregset (&ppcobsd_gregset, regcache,
			regnum, &regs, sizeof regs);
#endif

  if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't write registers"));

#ifdef PT_GETFPREGS
  if (regnum == -1 || getfpregs_supplies (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      ppc_collect_fpregset (&ppcobsd_fpregset, regcache,
			    regnum, &fpregs, sizeof fpregs);

      if (ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't write floating point status"));
    }
#endif
}


static int
ppcobsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_regcache_arch (regcache));
  struct switchframe sf;
  struct callframe cf;
  int i, regnum;

  /* The following is true for OpenBSD 3.7:

     The pcb contains %r1 (the stack pointer) at the point of the
     context switch in cpu_switch().  At that point we have a stack
     frame as described by `struct switchframe', and below that a call
     frame as described by `struct callframe'.  From this information
     we reconstruct the register state as it would look when we are in
     cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_sp == 0)
    return 0;

  read_memory (pcb->pcb_sp, (gdb_byte *)&sf, sizeof sf);
  regcache_raw_supply (regcache, gdbarch_sp_regnum (current_gdbarch), &sf.sp);
  regcache_raw_supply (regcache, tdep->ppc_cr_regnum, &sf.cr);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 2, &sf.fixreg2);
  for (i = 0, regnum = tdep->ppc_gp0_regnum + 13; i < 19; i++, regnum++)
    regcache_raw_supply (regcache, regnum, &sf.fixreg[i]);

  read_memory (sf.sp, (gdb_byte *)&cf, sizeof cf);
  regcache_raw_supply (regcache, gdbarch_pc_regnum (current_gdbarch), &cf.lr);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 30, &cf.r30);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 31, &cf.r31);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcobsd_nat (void);

void
_initialize_ppcobsd_nat (void)
{
  struct target_ops *t;

  /* Add in local overrides.  */
  t = inf_ptrace_target ();
  t->to_fetch_registers = ppcobsd_fetch_registers;
  t->to_store_registers = ppcobsd_store_registers;
  add_target (t);

  /* General-purpose registers.  */
  ppcobsd_reg_offsets.r0_offset = offsetof (struct reg, gpr);
  ppcobsd_reg_offsets.gpr_size = 4;
  ppcobsd_reg_offsets.xr_size = 4;
  ppcobsd_reg_offsets.pc_offset = offsetof (struct reg, pc);
  ppcobsd_reg_offsets.ps_offset = offsetof (struct reg, ps);
  ppcobsd_reg_offsets.cr_offset = offsetof (struct reg, cnd);
  ppcobsd_reg_offsets.lr_offset = offsetof (struct reg, lr);
  ppcobsd_reg_offsets.ctr_offset = offsetof (struct reg, cnt);
  ppcobsd_reg_offsets.xer_offset = offsetof (struct reg, xer);
  ppcobsd_reg_offsets.mq_offset = offsetof (struct reg, mq);

  /* Floating-point registers.  */
  ppcobsd_reg_offsets.f0_offset = offsetof (struct reg, fpr);
  ppcobsd_reg_offsets.fpscr_offset = -1;
#ifdef PT_GETFPREGS
  ppcobsd_fpreg_offsets.f0_offset = offsetof (struct fpreg, fpr);
  ppcobsd_fpreg_offsets.fpscr_offset = offsetof (struct fpreg, fpscr);
  ppcobsd_fpreg_offsets.fpscr_size = 4;
#endif

  /* AltiVec registers.  */
  ppcobsd_reg_offsets.vr0_offset = offsetof (struct vreg, vreg);
  ppcobsd_reg_offsets.vscr_offset = offsetof (struct vreg, vscr);
  ppcobsd_reg_offsets.vrsave_offset = offsetof (struct vreg, vrsave);

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (ppcobsd_supply_pcb);
}
