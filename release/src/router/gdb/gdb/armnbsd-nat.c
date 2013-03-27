/* Native-dependent code for BSD Unix running on ARM's, for GDB.

   Copyright (C) 1988, 1989, 1991, 1992, 1994, 1996, 1999, 2002, 2004, 2007
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
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "target.h"

#include "gdb_string.h"
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>

#include "arm-tdep.h"
#include "inf-ptrace.h"

extern int arm_apcs_32;

static void
arm_supply_gregset (struct regcache *regcache, struct reg *gregset)
{
  int regno;
  CORE_ADDR r_pc;

  /* Integer registers.  */
  for (regno = ARM_A1_REGNUM; regno < ARM_SP_REGNUM; regno++)
    regcache_raw_supply (regcache, regno, (char *) &gregset->r[regno]);

  regcache_raw_supply (regcache, ARM_SP_REGNUM,
		       (char *) &gregset->r_sp);
  regcache_raw_supply (regcache, ARM_LR_REGNUM,
		       (char *) &gregset->r_lr);
  /* This is ok: we're running native...  */
  r_pc = gdbarch_addr_bits_remove (current_gdbarch, gregset->r_pc);
  regcache_raw_supply (regcache, ARM_PC_REGNUM, (char *) &r_pc);

  if (arm_apcs_32)
    regcache_raw_supply (regcache, ARM_PS_REGNUM,
			 (char *) &gregset->r_cpsr);
  else
    regcache_raw_supply (regcache, ARM_PS_REGNUM,
			 (char *) &gregset->r_pc);
}

static void
arm_supply_fparegset (struct regcache *regcache, struct fpreg *fparegset)
{
  int regno;

  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    regcache_raw_supply (regcache, regno,
			 (char *) &fparegset->fpr[regno - ARM_F0_REGNUM]);

  regcache_raw_supply (regcache, ARM_FPS_REGNUM,
		       (char *) &fparegset->fpr_fpsr);
}

static void
fetch_register (struct regcache *regcache, int regno)
{
  struct reg inferior_registers;
  int ret;

  ret = ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch general register"));
      return;
    }

  switch (regno)
    {
    case ARM_SP_REGNUM:
      regcache_raw_supply (regcache, ARM_SP_REGNUM,
			   (char *) &inferior_registers.r_sp);
      break;

    case ARM_LR_REGNUM:
      regcache_raw_supply (regcache, ARM_LR_REGNUM,
			   (char *) &inferior_registers.r_lr);
      break;

    case ARM_PC_REGNUM:
      /* This is ok: we're running native... */
      inferior_registers.r_pc = gdbarch_addr_bits_remove
				  (current_gdbarch, inferior_registers.r_pc);
      regcache_raw_supply (regcache, ARM_PC_REGNUM,
			   (char *) &inferior_registers.r_pc);
      break;

    case ARM_PS_REGNUM:
      if (arm_apcs_32)
	regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     (char *) &inferior_registers.r_cpsr);
      else
	regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     (char *) &inferior_registers.r_pc);
      break;

    default:
      regcache_raw_supply (regcache, regno,
			   (char *) &inferior_registers.r[regno]);
      break;
    }
}

static void
fetch_regs (struct regcache *regcache)
{
  struct reg inferior_registers;
  int ret;
  int regno;

  ret = ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch general registers"));
      return;
    }

  arm_supply_gregset (regcache, &inferior_registers);
}

static void
fetch_fp_register (struct regcache *regcache, int regno)
{
  struct fpreg inferior_fp_registers;
  int ret;

  ret = ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_fp_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch floating-point register"));
      return;
    }

  switch (regno)
    {
    case ARM_FPS_REGNUM:
      regcache_raw_supply (regcache, ARM_FPS_REGNUM,
			   (char *) &inferior_fp_registers.fpr_fpsr);
      break;

    default:
      regcache_raw_supply (regcache, regno,
			   (char *) &inferior_fp_registers.fpr[regno - ARM_F0_REGNUM]);
      break;
    }
}

static void
fetch_fp_regs (struct regcache *regcache)
{
  struct fpreg inferior_fp_registers;
  int ret;
  int regno;

  ret = ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_fp_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch general registers"));
      return;
    }

  arm_supply_fparegset (regcache, &inferior_fp_registers);
}

static void
armnbsd_fetch_registers (struct regcache *regcache, int regno)
{
  if (regno >= 0)
    {
      if (regno < ARM_F0_REGNUM || regno > ARM_FPS_REGNUM)
	fetch_register (regcache, regno);
      else
	fetch_fp_register (regcache, regno);
    }
  else
    {
      fetch_regs (regcache);
      fetch_fp_regs (regcache);
    }
}


static void
store_register (const struct regcache *regcache, int regno)
{
  struct reg inferior_registers;
  int ret;

  ret = ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch general registers"));
      return;
    }

  switch (regno)
    {
    case ARM_SP_REGNUM:
      regcache_raw_collect (regcache, ARM_SP_REGNUM,
			    (char *) &inferior_registers.r_sp);
      break;

    case ARM_LR_REGNUM:
      regcache_raw_collect (regcache, ARM_LR_REGNUM,
			    (char *) &inferior_registers.r_lr);
      break;

    case ARM_PC_REGNUM:
      if (arm_apcs_32)
	regcache_raw_collect (regcache, ARM_PC_REGNUM,
			      (char *) &inferior_registers.r_pc);
      else
	{
	  unsigned pc_val;

	  regcache_raw_collect (regcache, ARM_PC_REGNUM,
				(char *) &pc_val);
	  
	  pc_val = gdbarch_addr_bits_remove (current_gdbarch, pc_val);
	  inferior_registers.r_pc ^= gdbarch_addr_bits_remove
				       (current_gdbarch,
					inferior_registers.r_pc);
	  inferior_registers.r_pc |= pc_val;
	}
      break;

    case ARM_PS_REGNUM:
      if (arm_apcs_32)
	regcache_raw_collect (regcache, ARM_PS_REGNUM,
			      (char *) &inferior_registers.r_cpsr);
      else
	{
	  unsigned psr_val;

	  regcache_raw_collect (regcache, ARM_PS_REGNUM,
				(char *) &psr_val);

	  psr_val ^= gdbarch_addr_bits_remove (current_gdbarch, psr_val);
	  inferior_registers.r_pc = gdbarch_addr_bits_remove
				      (current_gdbarch,
				       inferior_registers.r_pc);
	  inferior_registers.r_pc |= psr_val;
	}
      break;

    default:
      regcache_raw_collect (regcache, regno,
			    (char *) &inferior_registers.r[regno]);
      break;
    }

  ret = ptrace (PT_SETREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_registers, 0);

  if (ret < 0)
    warning (_("unable to write register %d to inferior"), regno);
}

static void
store_regs (const struct regcache *regcache)
{
  struct reg inferior_registers;
  int ret;
  int regno;


  for (regno = ARM_A1_REGNUM; regno < ARM_SP_REGNUM; regno++)
    regcache_raw_collect (regcache, regno,
			  (char *) &inferior_registers.r[regno]);

  regcache_raw_collect (regcache, ARM_SP_REGNUM,
			(char *) &inferior_registers.r_sp);
  regcache_raw_collect (regcache, ARM_LR_REGNUM,
			(char *) &inferior_registers.r_lr);

  if (arm_apcs_32)
    {
      regcache_raw_collect (regcache, ARM_PC_REGNUM,
			    (char *) &inferior_registers.r_pc);
      regcache_raw_collect (regcache, ARM_PS_REGNUM,
			    (char *) &inferior_registers.r_cpsr);
    }
  else
    {
      unsigned pc_val;
      unsigned psr_val;

      regcache_raw_collect (regcache, ARM_PC_REGNUM,
			    (char *) &pc_val);
      regcache_raw_collect (regcache, ARM_PS_REGNUM,
			    (char *) &psr_val);
	  
      pc_val = gdbarch_addr_bits_remove (current_gdbarch, pc_val);
      psr_val ^= gdbarch_addr_bits_remove (current_gdbarch, psr_val);

      inferior_registers.r_pc = pc_val | psr_val;
    }

  ret = ptrace (PT_SETREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_registers, 0);

  if (ret < 0)
    warning (_("unable to store general registers"));
}

static void
store_fp_register (const struct regcache *regcache, int regno)
{
  struct fpreg inferior_fp_registers;
  int ret;

  ret = ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_fp_registers, 0);

  if (ret < 0)
    {
      warning (_("unable to fetch floating-point registers"));
      return;
    }

  switch (regno)
    {
    case ARM_FPS_REGNUM:
      regcache_raw_collect (regcache, ARM_FPS_REGNUM,
			    (char *) &inferior_fp_registers.fpr_fpsr);
      break;

    default:
      regcache_raw_collect (regcache, regno,
			    (char *) &inferior_fp_registers.fpr[regno - ARM_F0_REGNUM]);
      break;
    }

  ret = ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_fp_registers, 0);

  if (ret < 0)
    warning (_("unable to write register %d to inferior"), regno);
}

static void
store_fp_regs (const struct regcache *regcache)
{
  struct fpreg inferior_fp_registers;
  int ret;
  int regno;


  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    regcache_raw_collect (regcache, regno,
			  (char *) &inferior_fp_registers.fpr[regno - ARM_F0_REGNUM]);

  regcache_raw_collect (regcache, ARM_FPS_REGNUM,
			(char *) &inferior_fp_registers.fpr_fpsr);

  ret = ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		(PTRACE_TYPE_ARG3) &inferior_fp_registers, 0);

  if (ret < 0)
    warning (_("unable to store floating-point registers"));
}

static void
armnbsd_store_registers (struct regcache *regcache, int regno)
{
  if (regno >= 0)
    {
      if (regno < ARM_F0_REGNUM || regno > ARM_FPS_REGNUM)
	store_register (regcache, regno);
      else
	store_fp_register (regcache, regno);
    }
  else
    {
      store_regs (regcache);
      store_fp_regs (regcache);
    }
}

struct md_core
{
  struct reg intreg;
  struct fpreg freg;
};

static void
fetch_core_registers (struct regcache *regcache,
		      char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR ignore)
{
  struct md_core *core_reg = (struct md_core *) core_reg_sect;
  int regno;
  CORE_ADDR r_pc;

  arm_supply_gregset (regcache, &core_reg->intreg);
  arm_supply_fparegset (regcache, &core_reg->freg);
}

static void
fetch_elfcore_registers (struct regcache *regcache,
			 char *core_reg_sect, unsigned core_reg_size,
			 int which, CORE_ADDR ignore)
{
  struct reg gregset;
  struct fpreg fparegset;

  switch (which)
    {
    case 0:	/* Integer registers.  */
      if (core_reg_size != sizeof (struct reg))
	warning (_("wrong size of register set in core file"));
      else
	{
	  /* The memcpy may be unnecessary, but we can't really be sure
	     of the alignment of the data in the core file.  */
	  memcpy (&gregset, core_reg_sect, sizeof (gregset));
	  arm_supply_gregset (regcache, &gregset);
	}
      break;

    case 2:
      if (core_reg_size != sizeof (struct fpreg))
	warning (_("wrong size of FPA register set in core file"));
      else
	{
	  /* The memcpy may be unnecessary, but we can't really be sure
	     of the alignment of the data in the core file.  */
	  memcpy (&fparegset, core_reg_sect, sizeof (fparegset));
	  arm_supply_fparegset (regcache, &fparegset);
	}
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

static struct core_fns arm_netbsd_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flovour.  */
  default_check_format,			/* check_format.  */
  default_core_sniffer,			/* core_sniffer.  */
  fetch_core_registers,			/* core_read_registers.  */
  NULL
};

static struct core_fns arm_netbsd_elfcore_fns =
{
  bfd_target_elf_flavour,		/* core_flovour.  */
  default_check_format,			/* check_format.  */
  default_core_sniffer,			/* core_sniffer.  */
  fetch_elfcore_registers,		/* core_read_registers.  */
  NULL
};

void
_initialize_arm_netbsd_nat (void)
{
  struct target_ops *t;

  t = inf_ptrace_target ();
  t->to_fetch_registers = armnbsd_fetch_registers;
  t->to_store_registers = armnbsd_store_registers;
  add_target (t);

  deprecated_add_core_fns (&arm_netbsd_core_fns);
  deprecated_add_core_fns (&arm_netbsd_elfcore_fns);
}
