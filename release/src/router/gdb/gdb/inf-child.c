/* Default child (native) target interface, for GDB when running under
   Unix.

   Copyright (C) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998,
   1999, 2000, 2001, 2002, 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "regcache.h"
#include "memattr.h"
#include "symtab.h"
#include "target.h"
#include "inferior.h"
#include "gdb_string.h"

/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
inf_child_fetch_inferior_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1)
    {
      for (regnum = 0; regnum < gdbarch_num_regs (current_gdbarch); regnum++)
	regcache_raw_supply (regcache, regnum, NULL);
    }
  else
    regcache_raw_supply (regcache, regnum, NULL);
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating point registers).  */

static void
inf_child_store_inferior_registers (struct regcache *regcache, int regnum)
{
}

static void
inf_child_post_attach (int pid)
{
  /* This version of Unix doesn't require a meaningful "post attach"
     operation by a debugger.  */
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On
   machines which store all the registers in one fell swoop, this
   makes sure that registers contains all the registers from the
   program being debugged.  */

static void
inf_child_prepare_to_store (struct regcache *regcache)
{
}

static void
inf_child_open (char *arg, int from_tty)
{
  error (_("Use the \"run\" command to start a Unix child process."));
}

static void
inf_child_post_startup_inferior (ptid_t ptid)
{
  /* This version of Unix doesn't require a meaningful "post startup
     inferior" operation by a debugger.  */
}

static void
inf_child_acknowledge_created_inferior (int pid)
{
  /* This version of Unix doesn't require a meaningful "acknowledge
     created inferior" operation by a debugger.  */
}

static void
inf_child_insert_fork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of fork
     events.  */
}

static int
inf_child_remove_fork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of fork
     events.  */
  return 0;
}

static void
inf_child_insert_vfork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of vfork
     events.  */
}

static int
inf_child_remove_vfork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of vfork
     events.  */
  return 0;
}

static int
inf_child_follow_fork (struct target_ops *ops, int follow_child)
{
  /* This version of Unix doesn't support following fork or vfork
     events.  */
  return 0;
}

static void
inf_child_insert_exec_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of exec
     events.  */
}

static int
inf_child_remove_exec_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of exec
     events.  */
  return 0;
}

static int
inf_child_reported_exec_events_per_exec_call (void)
{
  /* This version of Unix doesn't support notification of exec
     events.  */
  return 1;
}

static int
inf_child_can_run (void)
{
  return 1;
}

static struct symtab_and_line *
inf_child_enable_exception_callback (enum exception_event_kind kind,
				     int enable)
{
  return (struct symtab_and_line *) NULL;
}

static struct exception_event_record *
inf_child_get_current_exception_event (void)
{
  return (struct exception_event_record *) NULL;
}

static char *
inf_child_pid_to_exec_file (int pid)
{
  /* This version of Unix doesn't support translation of a process ID
     to the filename of the executable file.  */
  return NULL;
}

struct target_ops *
inf_child_target (void)
{
  struct target_ops *t = XZALLOC (struct target_ops);
  t->to_shortname = "child";
  t->to_longname = "Unix child process";
  t->to_doc = "Unix child process (started by the \"run\" command).";
  t->to_open = inf_child_open;
  t->to_post_attach = inf_child_post_attach;
  t->to_fetch_registers = inf_child_fetch_inferior_registers;
  t->to_store_registers = inf_child_store_inferior_registers;
  t->to_prepare_to_store = inf_child_prepare_to_store;
  t->to_insert_breakpoint = memory_insert_breakpoint;
  t->to_remove_breakpoint = memory_remove_breakpoint;
  t->to_terminal_init = terminal_init_inferior;
  t->to_terminal_inferior = terminal_inferior;
  t->to_terminal_ours_for_output = terminal_ours_for_output;
  t->to_terminal_save_ours = terminal_save_ours;
  t->to_terminal_ours = terminal_ours;
  t->to_terminal_info = child_terminal_info;
  t->to_post_startup_inferior = inf_child_post_startup_inferior;
  t->to_acknowledge_created_inferior = inf_child_acknowledge_created_inferior;
  t->to_insert_fork_catchpoint = inf_child_insert_fork_catchpoint;
  t->to_remove_fork_catchpoint = inf_child_remove_fork_catchpoint;
  t->to_insert_vfork_catchpoint = inf_child_insert_vfork_catchpoint;
  t->to_remove_vfork_catchpoint = inf_child_remove_vfork_catchpoint;
  t->to_follow_fork = inf_child_follow_fork;
  t->to_insert_exec_catchpoint = inf_child_insert_exec_catchpoint;
  t->to_remove_exec_catchpoint = inf_child_remove_exec_catchpoint;
  t->to_reported_exec_events_per_exec_call =
    inf_child_reported_exec_events_per_exec_call;
  t->to_can_run = inf_child_can_run;
  t->to_enable_exception_callback = inf_child_enable_exception_callback;
  t->to_get_current_exception_event = inf_child_get_current_exception_event;
  t->to_pid_to_exec_file = inf_child_pid_to_exec_file;
  t->to_stratum = process_stratum;
  t->to_has_all_memory = 1;
  t->to_has_memory = 1;
  t->to_has_stack = 1;
  t->to_has_registers = 1;
  t->to_has_execution = 1;
  t->to_magic = OPS_MAGIC;
  return t;
}
