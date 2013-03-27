/* GNU/Linux/x86-64 specific low level interface, for the remote server
   for GDB.
   Copyright (C) 2002, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "i387-fp.h"

#include "gdb_proc_service.h"

#include <sys/reg.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

/* This definition comes from prctl.h, but some kernels may not have it.  */
#ifndef PTRACE_ARCH_PRCTL
#define PTRACE_ARCH_PRCTL      30
#endif

/* The following definitions come from prctl.h, but may be absent
   for certain configurations.  */
#ifndef ARCH_GET_FS
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004
#endif

static int x86_64_regmap[] = {
  RAX * 8, RBX * 8, RCX * 8, RDX * 8,
  RSI * 8, RDI * 8, RBP * 8, RSP * 8,
  R8 * 8, R9 * 8, R10 * 8, R11 * 8,
  R12 * 8, R13 * 8, R14 * 8, R15 * 8,
  RIP * 8, EFLAGS * 8, CS * 8, SS * 8, 
  DS * 8, ES * 8, FS * 8, GS * 8,
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1,
  ORIG_RAX * 8
};

#define X86_64_NUM_GREGS (sizeof(x86_64_regmap)/sizeof(int))

/* Called by libthread_db.  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
  switch (idx)
    {
    case FS:
      if (ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_FS) == 0)
	return PS_OK;
      break;
    case GS:
      if (ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_GS) == 0)
	return PS_OK;
      break;
    default:
      return PS_BADADDR;
    }
  return PS_ERR;
}

static void
x86_64_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < X86_64_NUM_GREGS; i++)
    if (x86_64_regmap[i] != -1)
      collect_register (i, ((char *) buf) + x86_64_regmap[i]);
}

static void
x86_64_store_gregset (const void *buf)
{
  int i;

  for (i = 0; i < X86_64_NUM_GREGS; i++)
    if (x86_64_regmap[i] != -1)
      supply_register (i, ((char *) buf) + x86_64_regmap[i]);
}

static void
x86_64_fill_fpregset (void *buf)
{
  i387_cache_to_fxsave (buf);
}

static void
x86_64_store_fpregset (const void *buf)
{
  i387_fxsave_to_cache (buf);
}

struct regset_info target_regsets[] = {
  { PTRACE_GETREGS, PTRACE_SETREGS, sizeof (elf_gregset_t),
    GENERAL_REGS,
    x86_64_fill_gregset, x86_64_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, sizeof (elf_fpregset_t),
    FP_REGS,
    x86_64_fill_fpregset, x86_64_store_fpregset },
  { 0, 0, -1, -1, NULL, NULL }
};

static const unsigned char x86_64_breakpoint[] = { 0xCC };
#define x86_64_breakpoint_len 1
                
extern int debug_threads;

static CORE_ADDR
x86_64_get_pc ()
{
  unsigned long pc;

  collect_register_by_name ("rip", &pc);

  if (debug_threads)
    fprintf (stderr, "stop pc (before any decrement) is %08lx\n", pc);
  return pc;
}

static void
x86_64_set_pc (CORE_ADDR newpc)
{
  if (debug_threads)
    fprintf (stderr, "set pc to %08lx\n", (long) newpc);
  supply_register_by_name ("rip", &newpc);
}

static int
x86_64_breakpoint_at (CORE_ADDR pc)
{
  unsigned char c;

  read_inferior_memory (pc, &c, 1);
  if (c == 0xCC)
    return 1;

  return 0;
}

struct linux_target_ops the_low_target = {
  -1,
  NULL,
  NULL,
  NULL,
  x86_64_get_pc,
  x86_64_set_pc,
  x86_64_breakpoint,  
  x86_64_breakpoint_len,
  NULL,                                 
  1,
  x86_64_breakpoint_at,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  "i386:x86-64",
};
