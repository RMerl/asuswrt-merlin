/* Native-dependent code for PA-RISC HP-UX.

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
#include "regcache.h"
#include "target.h"

#include "gdb_assert.h"
#include <sys/ptrace.h>
#include <machine/save_state.h>

#ifdef HAVE_TTRACE
#include <sys/ttrace.h>
#endif

#include "hppa-tdep.h"
#include "inf-ptrace.h"
#include "inf-ttrace.h"

/* Non-zero if we should pretend not to be a runnable target.  */
int child_suppress_run = 0;

/* Return the offset of register REGNUM within `struct save_state'.
   The offset returns depends on the flags in the "flags" register and
   the register size (32-bit or 64-bit).  These are taken from
   REGCACHE.  */

LONGEST
hppa_hpux_save_state_offset (struct regcache *regcache, int regnum)
{
  LONGEST offset;

  if (regnum == HPPA_FLAGS_REGNUM)
    return ssoff (ss_flags);

  if (HPPA_R0_REGNUM < regnum && regnum < HPPA_FP0_REGNUM)
    {
      struct gdbarch *arch = get_regcache_arch (regcache);
      size_t size = register_size (arch, HPPA_R1_REGNUM);
      ULONGEST flags;

      gdb_assert (size == 4 || size == 8);

      regcache_cooked_read_unsigned (regcache, HPPA_FLAGS_REGNUM, &flags);
      if (flags & SS_WIDEREGS)
	offset = ssoff (ss_wide) + (8 - size) + (regnum - HPPA_R0_REGNUM) * 8;
      else
	offset = ssoff (ss_narrow) + (regnum - HPPA_R1_REGNUM) * 4;
    }
  else
    {
      struct gdbarch *arch = get_regcache_arch (regcache);
      size_t size = register_size (arch, HPPA_FP0_REGNUM);

      gdb_assert (size == 4 || size == 8);
      gdb_assert (regnum >= HPPA_FP0_REGNUM);
      offset = ssoff(ss_fpblock) + (regnum - HPPA_FP0_REGNUM) * size;
    }

  gdb_assert (offset < sizeof (save_state_t));
  return offset;
}

/* Just in case a future version of PA-RISC HP-UX won't have ptrace(2)
   at all.  */
#ifndef PTRACE_TYPE_RET
#define PTRACE_TYPE_RET void
#endif

static void
hppa_hpux_fetch_register (struct regcache *regcache, int regnum)
{
  CORE_ADDR addr;
  size_t size;
  PTRACE_TYPE_RET *buf;
  pid_t pid;
  int i;

  pid = ptid_get_pid (inferior_ptid);

  /* This isn't really an address, but ptrace thinks of it as one.  */
  addr = hppa_hpux_save_state_offset (regcache, regnum);
  size = register_size (current_gdbarch, regnum);

  gdb_assert (size == 4 || size == 8);
  buf = alloca (size);

#ifdef HAVE_TTRACE
  {
    lwpid_t lwp = ptid_get_lwp (inferior_ptid);

    if (ttrace (TT_LWP_RUREGS, pid, lwp, addr, size, (uintptr_t)buf) == -1)
      error (_("Couldn't read register %s (#%d): %s"),
	     gdbarch_register_name (current_gdbarch, regnum),
	     regnum, safe_strerror (errno));
  }
#else
  {
    int i;

    /* Read the register contents from the inferior a chuck at the time.  */
    for (i = 0; i < size / sizeof (PTRACE_TYPE_RET); i++)
      {
	errno = 0;
	buf[i] = ptrace (PT_RUREGS, pid, (PTRACE_TYPE_ARG3) addr, 0, 0);
	if (errno != 0)
	  error (_("Couldn't read register %s (#%d): %s"),
		 gdbarch_register_name (current_gdbarch, regnum),
		 regnum, safe_strerror (errno));

	addr += sizeof (PTRACE_TYPE_RET);
      }
  }
#endif

  /* Take care with the "flags" register.  It's stored as an `int' in
     `struct save_state', even for 64-bit code.  */
  if (regnum == HPPA_FLAGS_REGNUM && size == 8)
    {
      ULONGEST flags = extract_unsigned_integer ((gdb_byte *)buf, 4);
      store_unsigned_integer ((gdb_byte *)buf, 8, flags);
    }

  regcache_raw_supply (regcache, regnum, buf);
}

static void
hppa_hpux_fetch_inferior_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1)
    for (regnum = 0; regnum < gdbarch_num_regs (current_gdbarch); regnum++)
      hppa_hpux_fetch_register (regcache, regnum);
  else
    hppa_hpux_fetch_register (regcache, regnum);
}

/* Store register REGNUM into the inferior.  */

static void
hppa_hpux_store_register (struct regcache *regcache, int regnum)
{
  CORE_ADDR addr;
  size_t size;
  PTRACE_TYPE_RET *buf;
  pid_t pid;

  pid = ptid_get_pid (inferior_ptid);

  /* This isn't really an address, but ptrace thinks of it as one.  */
  addr = hppa_hpux_save_state_offset (regcache, regnum);
  size = register_size (current_gdbarch, regnum);

  gdb_assert (size == 4 || size == 8);
  buf = alloca (size);

  regcache_raw_collect (regcache, regnum, buf);

  /* Take care with the "flags" register.  It's stored as an `int' in
     `struct save_state', even for 64-bit code.  */
  if (regnum == HPPA_FLAGS_REGNUM && size == 8)
    {
      ULONGEST flags = extract_unsigned_integer ((gdb_byte *)buf, 8);
      store_unsigned_integer ((gdb_byte *)buf, 4, flags);
      size = 4;
    }

#ifdef HAVE_TTRACE
  {
    lwpid_t lwp = ptid_get_lwp (inferior_ptid);

    if (ttrace (TT_LWP_WUREGS, pid, lwp, addr, size, (uintptr_t)buf) == -1)
      error (_("Couldn't write register %s (#%d): %s"),
	     gdbarch_register_name (current_gdbarch, regnum),
	     regnum, safe_strerror (errno));
  }
#else
  {
    int i;

    /* Write the register contents into the inferior a chunk at the time.  */
    for (i = 0; i < size / sizeof (PTRACE_TYPE_RET); i++)
      {
	errno = 0;
	ptrace (PT_WUREGS, pid, (PTRACE_TYPE_ARG3) addr, buf[i], 0);
	if (errno != 0)
	  error (_("Couldn't write register %s (#%d): %s"),
		 gdbarch_register_name (current_gdbarch, regnum),
		 regnum, safe_strerror (errno));

	addr += sizeof (PTRACE_TYPE_RET);
      }
  }
#endif
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating point registers).  */

static void
hppa_hpux_store_inferior_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1)
    for (regnum = 0; regnum < gdbarch_num_regs (current_gdbarch); regnum++)
      hppa_hpux_store_register (regcache, regnum);
  else
    hppa_hpux_store_register (regcache, regnum);
}

static int
hppa_hpux_child_can_run (void)
{
  /* This variable is controlled by modules that layer their own
     process structure atop that provided here.  The code in
     hpux-thread.c does this to support the HP-UX user-mode DCE
     threads.  */
  return !child_suppress_run;
}


/* Prevent warning from -Wmissing-prototypes.  */
void _initialize_hppa_hpux_nat (void);

void
_initialize_hppa_hpux_nat (void)
{
  struct target_ops *t;

#ifdef HAVE_TTRACE
  t = inf_ttrace_target ();
#else
  t = inf_ptrace_target ();
#endif

  t->to_fetch_registers = hppa_hpux_fetch_inferior_registers;
  t->to_store_registers = hppa_hpux_store_inferior_registers;
  t->to_can_run = hppa_hpux_child_can_run;

  add_target (t);
}
