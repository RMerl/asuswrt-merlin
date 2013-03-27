/* Low level Alpha GNU/Linux interface, for GDB when running native.
   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "target.h"
#include "regcache.h"
#include "linux-nat.h"

#include "alpha-tdep.h"

#include <sys/ptrace.h>
#include <alpha/ptrace.h>

#include <sys/procfs.h>
#include "gregset.h"

/* The address of UNIQUE for ptrace.  */
#define ALPHA_UNIQUE_PTRACE_ADDR 65


/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 */

void
supply_gregset (struct regcache *regcache, const gdb_gregset_t *gregsetp)
{
  const long *regp = (const long *)gregsetp;

  /* PC is in slot 32, UNIQUE is in slot 33.  */
  alpha_supply_int_regs (regcache, -1, regp, regp + 31, regp + 32);
}

void
fill_gregset (const struct regcache *regcache,
	      gdb_gregset_t *gregsetp, int regno)
{
  long *regp = (long *)gregsetp;

  /* PC is in slot 32, UNIQUE is in slot 33.  */
  alpha_fill_int_regs (regcache, regno, regp, regp + 31, regp + 32);
}

/*
 * Now we do the same thing for floating-point registers.
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t *fpregsetp)
{
  const long *regp = (const long *)fpregsetp;

  /* FPCR is in slot 32.  */
  alpha_supply_fp_regs (regcache, -1, regp, regp + 31);
}

void
fill_fpregset (const struct regcache *regcache,
	       gdb_fpregset_t *fpregsetp, int regno)
{
  long *regp = (long *)fpregsetp;

  /* FPCR is in slot 32.  */
  alpha_fill_fp_regs (regcache, regno, regp, regp + 31);
}


static CORE_ADDR
alpha_linux_register_u_offset (struct gdbarch *gdbarch, int regno, int store_p)
{
  if (regno == gdbarch_pc_regnum (current_gdbarch))
    return PC;
  if (regno == ALPHA_UNIQUE_REGNUM)
    return ALPHA_UNIQUE_PTRACE_ADDR;
  if (regno < gdbarch_fp0_regnum (current_gdbarch))
    return GPR_BASE + regno;
  else
    return FPR_BASE + regno - gdbarch_fp0_regnum (current_gdbarch);
}

void _initialialize_alpha_linux_nat (void);

void
_initialize_alpha_linux_nat (void)
{
  linux_nat_add_target (linux_trad_target (alpha_linux_register_u_offset));
}
