/* GNU/Linux/SH specific low level interface, for the remote server for GDB.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2007
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

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#include <asm/ptrace.h>

#define sh_num_regs 41

/* Currently, don't check/send MQ.  */
static int sh_regmap[] = {
 0,	4,	8,	12,	16,	20,	24,	28,
 32,	36,	40,	44,	48,	52,	56,	60,

 REG_PC*4,   REG_PR*4,   REG_GBR*4,  -1,
 REG_MACH*4, REG_MACL*4, REG_SR*4,
 REG_FPUL*4, REG_FPSCR*4,

 REG_FPREG0*4+0,   REG_FPREG0*4+4,   REG_FPREG0*4+8,   REG_FPREG0*4+12,
 REG_FPREG0*4+16,  REG_FPREG0*4+20,  REG_FPREG0*4+24,  REG_FPREG0*4+28,
 REG_FPREG0*4+32,  REG_FPREG0*4+36,  REG_FPREG0*4+40,  REG_FPREG0*4+44,
 REG_FPREG0*4+48,  REG_FPREG0*4+52,  REG_FPREG0*4+56,  REG_FPREG0*4+60,
};

static int
sh_cannot_store_register (int regno)
{
  return 0;
}

static int
sh_cannot_fetch_register (int regno)
{
  return 0;
}

static CORE_ADDR
sh_get_pc ()
{
  unsigned long pc;
  collect_register_by_name ("pc", &pc);
  return pc;
}

static void
sh_set_pc (CORE_ADDR pc)
{
  unsigned long newpc = pc;
  supply_register_by_name ("pc", &newpc);
}

/* Correct in either endianness, obviously.  */
static const unsigned short sh_breakpoint = 0xc3c3;
#define sh_breakpoint_len 2

static int
sh_breakpoint_at (CORE_ADDR where)
{
  unsigned short insn;

  (*the_target->read_memory) (where, (unsigned char *) &insn, 2);
  if (insn == sh_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

/* Provide only a fill function for the general register set.  ps_lgetregs
   will use this for NPTL support.  */

static void sh_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < 23; i++)
    if (sh_regmap[i] != -1)
      collect_register (i, (char *) buf + sh_regmap[i]);
}

struct regset_info target_regsets[] = {
  { 0, 0, 0, GENERAL_REGS, sh_fill_gregset, NULL },
  { 0, 0, -1, -1, NULL, NULL }
};

struct linux_target_ops the_low_target = {
  sh_num_regs,
  sh_regmap,
  sh_cannot_fetch_register,
  sh_cannot_store_register,
  sh_get_pc,
  sh_set_pc,
  (const unsigned char *) &sh_breakpoint,
  sh_breakpoint_len,
  NULL,
  0,
  sh_breakpoint_at,
};
