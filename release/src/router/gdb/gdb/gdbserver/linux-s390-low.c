/* GNU/Linux S/390 specific low level interface, for the remote server
   for GDB.
   Copyright (C) 2001, 2002, 2005, 2006, 2007 Free Software Foundation, Inc.

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

/* This file is used for both 31-bit and 64-bit S/390 systems.  */

#include "server.h"
#include "linux-low.h"

#include <asm/ptrace.h>

#define s390_num_regs 51

static int s390_regmap[] = {
  PT_PSWMASK, PT_PSWADDR,

  PT_GPR0, PT_GPR1, PT_GPR2, PT_GPR3,
  PT_GPR4, PT_GPR5, PT_GPR6, PT_GPR7,
  PT_GPR8, PT_GPR9, PT_GPR10, PT_GPR11,
  PT_GPR12, PT_GPR13, PT_GPR14, PT_GPR15,

  PT_ACR0, PT_ACR1, PT_ACR2, PT_ACR3,
  PT_ACR4, PT_ACR5, PT_ACR6, PT_ACR7,
  PT_ACR8, PT_ACR9, PT_ACR10, PT_ACR11,
  PT_ACR12, PT_ACR13, PT_ACR14, PT_ACR15,

  PT_FPC,

#ifndef __s390x__
  PT_FPR0_HI, PT_FPR1_HI, PT_FPR2_HI, PT_FPR3_HI,
  PT_FPR4_HI, PT_FPR5_HI, PT_FPR6_HI, PT_FPR7_HI,
  PT_FPR8_HI, PT_FPR9_HI, PT_FPR10_HI, PT_FPR11_HI,
  PT_FPR12_HI, PT_FPR13_HI, PT_FPR14_HI, PT_FPR15_HI,
#else
  PT_FPR0, PT_FPR1, PT_FPR2, PT_FPR3,
  PT_FPR4, PT_FPR5, PT_FPR6, PT_FPR7,
  PT_FPR8, PT_FPR9, PT_FPR10, PT_FPR11,
  PT_FPR12, PT_FPR13, PT_FPR14, PT_FPR15,
#endif
};

static int
s390_cannot_fetch_register (int regno)
{
  if (s390_regmap[regno] == -1)
    return 1;

  return 0;
}

static int
s390_cannot_store_register (int regno)
{
  if (s390_regmap[regno] == -1)
    return 1;

  return 0;
}

/* Provide only a fill function for the general register set.  ps_lgetregs
   will use this for NPTL support.  */

static void s390_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < 34; i++)
    collect_register (i, (char *) buf + s390_regmap[i]);
}

struct regset_info target_regsets[] = {
  { 0, 0, 0, GENERAL_REGS, s390_fill_gregset, NULL },
  { 0, 0, -1, -1, NULL, NULL }
};


static const unsigned char s390_breakpoint[] = { 0, 1 };
#define s390_breakpoint_len 2

static CORE_ADDR
s390_get_pc ()
{
  unsigned long pc;
  collect_register_by_name ("pswa", &pc);
#ifndef __s390x__
  pc &= 0x7fffffff;
#endif
  return pc;
}

static void
s390_set_pc (CORE_ADDR newpc)
{
  unsigned long pc = newpc;
#ifndef __s390x__
  pc |= 0x80000000;
#endif
  supply_register_by_name ("pswa", &pc);
}

static int
s390_breakpoint_at (CORE_ADDR pc)
{
  unsigned char c[s390_breakpoint_len];
  read_inferior_memory (pc, c, s390_breakpoint_len);
  return memcmp (c, s390_breakpoint, s390_breakpoint_len) == 0;
}


struct linux_target_ops the_low_target = {
  s390_num_regs,
  s390_regmap,
  s390_cannot_fetch_register,
  s390_cannot_store_register,
  s390_get_pc,
  s390_set_pc,
  s390_breakpoint,
  s390_breakpoint_len,
  NULL,
  s390_breakpoint_len,
  s390_breakpoint_at,
};

