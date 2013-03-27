/* Native-dependent code for Motorola 88000 BSD's.

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
#include "inferior.h"
#include "regcache.h"
#include "target.h"

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "m88k-tdep.h"
#include "inf-ptrace.h"

/* Supply the general-purpose registers stored in GREGS to REGCACHE.  */

static void
m88kbsd_supply_gregset (struct regcache *regcache, const void *gregs)
{
  const char *regs = gregs;
  int regnum;

  for (regnum = 0; regnum < M88K_NUM_REGS; regnum++)
    regcache_raw_supply (regcache, regnum, regs + regnum * 4);
}

/* Collect the general-purpose registers from REGCACHE and store them
   in GREGS.  */

static void
m88kbsd_collect_gregset (const struct regcache *regcache,
			 void *gregs, int regnum)
{
  char *regs = gregs;
  int i;

  for (i = 0; i < M88K_NUM_REGS; i++)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_collect (regcache, i, regs + i * 4);
    }
}


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
m88kbsd_fetch_inferior_registers (struct regcache *regcache, int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  m88kbsd_supply_gregset (regcache, &regs);
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

static void
m88kbsd_store_inferior_registers (struct regcache *regcache, int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  m88kbsd_collect_gregset (regcache, &regs, regnum);

  if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't write registers"));
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m88kbsd_nat (void);

void
_initialize_m88kbsd_nat (void)
{
  struct target_ops *t;

  t = inf_ptrace_target ();
  t->to_fetch_registers = m88kbsd_fetch_inferior_registers;
  t->to_store_registers = m88kbsd_store_inferior_registers;
  add_target (t);
}
