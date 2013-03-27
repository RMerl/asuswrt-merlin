/* GNU/Linux/CRIS specific low level interface, for the remote server for GDB.
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

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

/* CRISv10 */
#define cris_num_regs 32

/* Locations need to match <include/asm/arch/ptrace.h>.  */
static int cris_regmap[] = {
  15*4, 14*4, 13*4, 12*4,
  11*4, 10*4, 9*4, 8*4,
  7*4, 6*4, 5*4, 4*4,
  3*4, 2*4, 23*4, 19*4,

  -1, -1, -1, -1,
  -1, 17*4, -1, 16*4,
  -1, -1, -1, 18*4,
  -1, 17*4, -1, -1
  
};

static int
cris_cannot_store_register (int regno)
{
  if (cris_regmap[regno] == -1)
    return 1;
  
  return (regno >= cris_num_regs);
}

static int
cris_cannot_fetch_register (int regno)
{
  if (cris_regmap[regno] == -1)
    return 1;

  return (regno >= cris_num_regs);
}

extern int debug_threads;

static CORE_ADDR
cris_get_pc (void)
{
  unsigned long pc;
  collect_register_by_name ("pc", &pc);
  if (debug_threads)
    fprintf (stderr, "stop pc is %08lx\n", pc);
  return pc;
}

static void
cris_set_pc (CORE_ADDR pc)
{
  unsigned long newpc = pc;
  supply_register_by_name ("pc", &newpc);
}

static const unsigned short cris_breakpoint = 0xe938;
#define cris_breakpoint_len 2

static int
cris_breakpoint_at (CORE_ADDR where)
{
  unsigned short insn;

  (*the_target->read_memory) (where, (unsigned char *) &insn,
			      cris_breakpoint_len);
  if (insn == cris_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

/* We only place breakpoints in empty marker functions, and thread locking
   is outside of the function.  So rather than importing software single-step,
   we can just run until exit.  */
static CORE_ADDR
cris_reinsert_addr (void)
{
  unsigned long pc;
  collect_register_by_name ("srp", &pc);
  return pc;
}

struct linux_target_ops the_low_target = {
  cris_num_regs,
  cris_regmap,
  cris_cannot_fetch_register,
  cris_cannot_store_register,
  cris_get_pc,
  cris_set_pc,
  (const unsigned char *) &cris_breakpoint,
  cris_breakpoint_len,
  cris_reinsert_addr,
  0,
  cris_breakpoint_at,
  0,
  0,
  0,
  0,
};
