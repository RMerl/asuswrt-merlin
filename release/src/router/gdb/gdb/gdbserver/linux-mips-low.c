/* GNU/Linux/MIPS specific low level interface, for the remote server for GDB.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2005, 2006, 2007
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

#include "server.h"
#include "linux-low.h"

#include <sys/ptrace.h>
#include <endian.h>

#include "gdb_proc_service.h"

#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA 25
#endif

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#define mips_num_regs 73

#include <asm/ptrace.h>

union mips_register
{
  unsigned char buf[8];

  /* Deliberately signed, for proper sign extension.  */
  int reg32;
  long long reg64;
};

/* Return the ptrace ``address'' of register REGNO. */

static int mips_regmap[] = {
  -1,  1,  2,  3,  4,  5,  6,  7,
  8,  9,  10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,

  -1, MMLO, MMHI, BADVADDR, CAUSE, PC,

  FPR_BASE,      FPR_BASE + 1,  FPR_BASE + 2,  FPR_BASE + 3,
  FPR_BASE + 4,  FPR_BASE + 5,  FPR_BASE + 6,  FPR_BASE + 7,
  FPR_BASE + 8,  FPR_BASE + 8,  FPR_BASE + 10, FPR_BASE + 11,
  FPR_BASE + 12, FPR_BASE + 13, FPR_BASE + 14, FPR_BASE + 15,
  FPR_BASE + 16, FPR_BASE + 17, FPR_BASE + 18, FPR_BASE + 19,
  FPR_BASE + 20, FPR_BASE + 21, FPR_BASE + 22, FPR_BASE + 23,
  FPR_BASE + 24, FPR_BASE + 25, FPR_BASE + 26, FPR_BASE + 27,
  FPR_BASE + 28, FPR_BASE + 29, FPR_BASE + 30, FPR_BASE + 31,
  FPC_CSR, FPC_EIR,

  0
};

/* From mips-linux-nat.c.  */

/* Pseudo registers can not be read.  ptrace does not provide a way to
   read (or set) PS_REGNUM, and there's no point in reading or setting
   ZERO_REGNUM.  We also can not set BADVADDR, CAUSE, or FCRIR via
   ptrace().  */

static int
mips_cannot_fetch_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("r0") == regno)
    return 1;

  return 0;
}

static int
mips_cannot_store_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("r0") == regno)
    return 1;

  if (find_regno ("cause") == regno)
    return 1;

  if (find_regno ("badvaddr") == regno)
    return 1;

  if (find_regno ("fir") == regno)
    return 1;

  return 0;
}

static CORE_ADDR
mips_get_pc ()
{
  union mips_register pc;
  collect_register_by_name ("pc", pc.buf);
  return register_size (0) == 4 ? pc.reg32 : pc.reg64;
}

static void
mips_set_pc (CORE_ADDR pc)
{
  union mips_register newpc;
  if (register_size (0) == 4)
    newpc.reg32 = pc;
  else
    newpc.reg64 = pc;

  supply_register_by_name ("pc", newpc.buf);
}

/* Correct in either endianness.  */
static const unsigned int mips_breakpoint = 0x0005000d;
#define mips_breakpoint_len 4

/* We only place breakpoints in empty marker functions, and thread locking
   is outside of the function.  So rather than importing software single-step,
   we can just run until exit.  */
static CORE_ADDR
mips_reinsert_addr ()
{
  union mips_register ra;
  collect_register_by_name ("r31", ra.buf);
  return register_size (0) == 4 ? ra.reg32 : ra.reg64;
}

static int
mips_breakpoint_at (CORE_ADDR where)
{
  unsigned int insn;

  (*the_target->read_memory) (where, (unsigned char *) &insn, 4);
  if (insn == mips_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

/* Fetch the thread-local storage pointer for libthread_db.  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
  if (ptrace (PTRACE_GET_THREAD_AREA, lwpid, NULL, base) != 0)
    return PS_ERR;

  /* IDX is the bias from the thread pointer to the beginning of the
     thread descriptor.  It has to be subtracted due to implementation
     quirks in libthread_db.  */
  *base = (void *) ((char *)*base - idx);

  return PS_OK;
}

#ifdef HAVE_PTRACE_GETREGS

static void
mips_collect_register (int use_64bit, int regno, union mips_register *reg)
{
  union mips_register tmp_reg;

  if (use_64bit)
    {
      collect_register (regno, &tmp_reg.reg64);
      *reg = tmp_reg;
    }
  else
    {
      collect_register (regno, &tmp_reg.reg32);
      reg->reg64 = tmp_reg.reg32;
    }
}

static void
mips_supply_register (int use_64bit, int regno, const union mips_register *reg)
{
  int offset = 0;

  /* For big-endian 32-bit targets, ignore the high four bytes of each
     eight-byte slot.  */
  if (__BYTE_ORDER == __BIG_ENDIAN && !use_64bit)
    offset = 4;

  supply_register (regno, reg->buf + offset);
}

static void
mips_collect_register_32bit (int use_64bit, int regno, unsigned char *buf)
{
  union mips_register tmp_reg;
  int reg32;

  mips_collect_register (use_64bit, regno, &tmp_reg);
  reg32 = tmp_reg.reg64;
  memcpy (buf, &reg32, 4);
}

static void
mips_supply_register_32bit (int use_64bit, int regno, const unsigned char *buf)
{
  union mips_register tmp_reg;
  int reg32;

  memcpy (&reg32, buf, 4);
  tmp_reg.reg64 = reg32;
  mips_supply_register (use_64bit, regno, &tmp_reg);
}

static void
mips_fill_gregset (void *buf)
{
  union mips_register *regset = buf;
  int i, use_64bit;

  use_64bit = (register_size (0) == 8);

  for (i = 1; i < 32; i++)
    mips_collect_register (use_64bit, i, regset + i);

  mips_collect_register (use_64bit, find_regno ("lo"), regset + 32);
  mips_collect_register (use_64bit, find_regno ("hi"), regset + 33);
  mips_collect_register (use_64bit, find_regno ("pc"), regset + 34);
  mips_collect_register (use_64bit, find_regno ("badvaddr"), regset + 35);
  mips_collect_register (use_64bit, find_regno ("status"), regset + 36);
  mips_collect_register (use_64bit, find_regno ("cause"), regset + 37);

  mips_collect_register (use_64bit, find_regno ("restart"), regset + 0);
}

static void
mips_store_gregset (const void *buf)
{
  const union mips_register *regset = buf;
  int i, use_64bit;

  use_64bit = (register_size (0) == 8);

  for (i = 0; i < 32; i++)
    mips_supply_register (use_64bit, i, regset + i);

  mips_supply_register (use_64bit, find_regno ("lo"), regset + 32);
  mips_supply_register (use_64bit, find_regno ("hi"), regset + 33);
  mips_supply_register (use_64bit, find_regno ("pc"), regset + 34);
  mips_supply_register (use_64bit, find_regno ("badvaddr"), regset + 35);
  mips_supply_register (use_64bit, find_regno ("status"), regset + 36);
  mips_supply_register (use_64bit, find_regno ("cause"), regset + 37);

  mips_supply_register (use_64bit, find_regno ("restart"), regset + 0);
}

static void
mips_fill_fpregset (void *buf)
{
  union mips_register *regset = buf;
  int i, use_64bit, first_fp, big_endian;

  use_64bit = (register_size (0) == 8);
  first_fp = find_regno ("f0");
  big_endian = (__BYTE_ORDER == __BIG_ENDIAN);

  /* See GDB for a discussion of this peculiar layout.  */
  for (i = 0; i < 32; i++)
    if (use_64bit)
      collect_register (first_fp + i, regset[i].buf);
    else
      collect_register (first_fp + i,
			regset[i & ~1].buf + 4 * (big_endian != (i & 1)));

  mips_collect_register_32bit (use_64bit, find_regno ("fcsr"), regset[32].buf);
  mips_collect_register_32bit (use_64bit, find_regno ("fir"),
			       regset[32].buf + 4);
}

static void
mips_store_fpregset (const void *buf)
{
  const union mips_register *regset = buf;
  int i, use_64bit, first_fp, big_endian;

  use_64bit = (register_size (0) == 8);
  first_fp = find_regno ("f0");
  big_endian = (__BYTE_ORDER == __BIG_ENDIAN);

  /* See GDB for a discussion of this peculiar layout.  */
  for (i = 0; i < 32; i++)
    if (use_64bit)
      supply_register (first_fp + i, regset[i].buf);
    else
      supply_register (first_fp + i,
		       regset[i & ~1].buf + 4 * (big_endian != (i & 1)));

  mips_supply_register_32bit (use_64bit, find_regno ("fcsr"), regset[32].buf);
  mips_supply_register_32bit (use_64bit, find_regno ("fir"),
			      regset[32].buf + 4);
}
#endif /* HAVE_PTRACE_GETREGS */

struct regset_info target_regsets[] = {
#ifdef HAVE_PTRACE_GETREGS
  { PTRACE_GETREGS, PTRACE_SETREGS, 38 * 8, GENERAL_REGS,
    mips_fill_gregset, mips_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, 33 * 8, FP_REGS,
    mips_fill_fpregset, mips_store_fpregset },
#endif /* HAVE_PTRACE_GETREGS */
  { 0, 0, -1, -1, NULL, NULL }
};

struct linux_target_ops the_low_target = {
  mips_num_regs,
  mips_regmap,
  mips_cannot_fetch_register,
  mips_cannot_store_register,
  mips_get_pc,
  mips_set_pc,
  (const unsigned char *) &mips_breakpoint,
  mips_breakpoint_len,
  mips_reinsert_addr,
  0,
  mips_breakpoint_at,
};
