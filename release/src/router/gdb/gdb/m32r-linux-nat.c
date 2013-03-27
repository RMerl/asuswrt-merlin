/* Native-dependent code for GNU/Linux m32r.

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
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "linux-nat.h"
#include "target.h"

#include "gdb_assert.h"
#include "gdb_string.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/procfs.h>

/* Prototypes for supply_gregset etc.  */
#include "gregset.h"

#include "m32r-tdep.h"




/* Since EVB register is not available for native debug, we reduce
   the number of registers.  */
#define M32R_LINUX_NUM_REGS (M32R_NUM_REGS - 1)

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register array layout.  */
static int regmap[] = {
  4, 5, 6, 7, 0, 1, 2, 8,
  9, 10, 11, 12, 13, 24, 25, 23,
  19, 19, 26, 23, 22, 20, 16, 15
};

#define PSW_REGMAP 19
#define BBPSW_REGMAP 21
#define SPU_REGMAP 23
#define SPI_REGMAP 26

/* Doee apply to the corresponding SET requests as well.  */
#define GETREGS_SUPPLIES(regno) (0 <= (regno) && (regno) <= M32R_LINUX_NUM_REGS)



/* Transfering the general-purpose registers between GDB, inferiors
   and core files.  */

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (struct regcache *regcache, const elf_gregset_t * gregsetp)
{
  const elf_greg_t *regp = (const elf_greg_t *) gregsetp;
  int i;
  unsigned long psw, bbpsw;

  psw = *(regp + PSW_REGMAP);
  bbpsw = *(regp + BBPSW_REGMAP);

  for (i = 0; i < M32R_LINUX_NUM_REGS; i++)
    {
      elf_greg_t regval;

      switch (i)
	{
	case PSW_REGNUM:
	  regval = ((0x00c1 & bbpsw) << 8) | ((0xc100 & psw) >> 8);
	  break;
	case CBR_REGNUM:
	  regval = ((psw >> 8) & 1);
	  break;
	default:
	  regval = *(regp + regmap[i]);
	  break;
	}

      if (i != M32R_SP_REGNUM)
	regcache_raw_supply (regcache, i, &regval);
      else if (psw & 0x8000)
	regcache_raw_supply (regcache, i, regp + SPU_REGMAP);
      else
	regcache_raw_supply (regcache, i, regp + SPI_REGMAP);
    }
}

/* Fetch all general-purpose registers from process/thread TID and
   store their values in GDB's register array.  */

static void
fetch_regs (struct regcache *regcache, int tid)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't get registers"));

  supply_gregset (regcache, (const elf_gregset_t *) &regs);
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_gregset (const struct regcache *regcache,
	      elf_gregset_t * gregsetp, int regno)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;
  unsigned long psw, bbpsw, tmp;

  psw = *(regp + PSW_REGMAP);
  bbpsw = *(regp + BBPSW_REGMAP);

  for (i = 0; i < M32R_LINUX_NUM_REGS; i++)
    {
      if (regno != -1 && regno != i)
	continue;

      if (i == CBR_REGNUM || i == PSW_REGNUM)
	continue;

      if (i == SPU_REGNUM || i == SPI_REGNUM)
	continue;

      if (i != M32R_SP_REGNUM)
	regcache_raw_collect (regcache, i, regp + regmap[i]);
      else if (psw & 0x8000)
	regcache_raw_collect (regcache, i, regp + SPU_REGMAP);
      else
	regcache_raw_collect (regcache, i, regp + SPI_REGMAP);
    }
}

/* Store all valid general-purpose registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_regs (const struct regcache *regcache, int tid, int regno)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't get registers"));

  fill_gregset (regcache, &regs, regno);

  if (ptrace (PTRACE_SETREGS, tid, 0, (int) &regs) < 0)
    perror_with_name (_("Couldn't write registers"));
}



/* Transfering floating-point registers between GDB, inferiors and cores.  
   Since M32R has no floating-point registers, these functions do nothing.  */

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t *fpregs)
{
}

void
fill_fpregset (const struct regcache *regcache,
	       gdb_fpregset_t *fpregs, int regno)
{
}



/* Transferring arbitrary registers between GDB and inferior.  */

/* Fetch register REGNO from the child process.  If REGNO is -1, do
   this for all registers (including the floating point and SSE
   registers).  */

static void
m32r_linux_fetch_inferior_registers (struct regcache *regcache, int regno)
{
  int tid;

  /* GNU/Linux LWP ID's are process ID's.  */
  tid = TIDGET (inferior_ptid);
  if (tid == 0)
    tid = PIDGET (inferior_ptid);	/* Not a threaded program.  */

  /* Use the PTRACE_GETREGS request whenever possible, since it
     transfers more registers in one system call, and we'll cache the
     results.  */
  if (regno == -1 || GETREGS_SUPPLIES (regno))
    {
      fetch_regs (regcache, tid);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request for bad register number %d."), regno);
}

/* Store register REGNO back into the child process.  If REGNO is -1,
   do this for all registers (including the floating point and SSE
   registers).  */
static void
m32r_linux_store_inferior_registers (struct regcache *regcache, int regno)
{
  int tid;

  /* GNU/Linux LWP ID's are process ID's.  */
  if ((tid = TIDGET (inferior_ptid)) == 0)
    tid = PIDGET (inferior_ptid);	/* Not a threaded program.  */

  /* Use the PTRACE_SETREGS request whenever possible, since it
     transfers more registers in one system call.  */
  if (regno == -1 || GETREGS_SUPPLIES (regno))
    {
      store_regs (regcache, tid, regno);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  _("Got request to store bad register number %d."), regno);
}

void _initialize_m32r_linux_nat (void);

void
_initialize_m32r_linux_nat (void)
{
  struct target_ops *t;

  /* Fill in the generic GNU/Linux methods.  */
  t = linux_target ();

  /* Add our register access methods.  */
  t->to_fetch_registers = m32r_linux_fetch_inferior_registers;
  t->to_store_registers = m32r_linux_store_inferior_registers;

  /* Register the target.  */
  linux_nat_add_target (t);
}
