/* Select target systems and architectures at runtime for GDB.

   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Cygnus Support.

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
#include <errno.h>
#include "gdb_string.h"
#include "target.h"
#include "gdbcmd.h"
#include "symtab.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_wait.h"
#include "dcache.h"
#include <signal.h>
#include "regcache.h"
#include "gdb_assert.h"
#include "gdbcore.h"
#include "exceptions.h"
#include "target-descriptions.h"

static void target_info (char *, int);

static void maybe_kill_then_attach (char *, int);

static void kill_or_be_killed (int);

static void default_terminal_info (char *, int);

static int default_region_ok_for_hw_watchpoint (CORE_ADDR, int);

static int nosymbol (char *, CORE_ADDR *);

static void tcomplain (void) ATTR_NORETURN;

static int nomemory (CORE_ADDR, char *, int, int, struct target_ops *);

static int return_zero (void);

static int return_one (void);

static int return_minus_one (void);

void target_ignore (void);

static void target_command (char *, int);

static struct target_ops *find_default_run_target (char *);

static void nosupport_runtime (void);

static LONGEST default_xfer_partial (struct target_ops *ops,
				     enum target_object object,
				     const char *annex, gdb_byte *readbuf,
				     const gdb_byte *writebuf,
				     ULONGEST offset, LONGEST len);

static LONGEST current_xfer_partial (struct target_ops *ops,
				     enum target_object object,
				     const char *annex, gdb_byte *readbuf,
				     const gdb_byte *writebuf,
				     ULONGEST offset, LONGEST len);

static LONGEST target_xfer_partial (struct target_ops *ops,
				    enum target_object object,
				    const char *annex,
				    void *readbuf, const void *writebuf,
				    ULONGEST offset, LONGEST len);

static void init_dummy_target (void);

static struct target_ops debug_target;

static void debug_to_open (char *, int);

static void debug_to_close (int);

static void debug_to_attach (char *, int);

static void debug_to_detach (char *, int);

static void debug_to_resume (ptid_t, int, enum target_signal);

static ptid_t debug_to_wait (ptid_t, struct target_waitstatus *);

static void debug_to_fetch_registers (struct regcache *, int);

static void debug_to_store_registers (struct regcache *, int);

static void debug_to_prepare_to_store (struct regcache *);

static void debug_to_files_info (struct target_ops *);

static int debug_to_insert_breakpoint (struct bp_target_info *);

static int debug_to_remove_breakpoint (struct bp_target_info *);

static int debug_to_can_use_hw_breakpoint (int, int, int);

static int debug_to_insert_hw_breakpoint (struct bp_target_info *);

static int debug_to_remove_hw_breakpoint (struct bp_target_info *);

static int debug_to_insert_watchpoint (CORE_ADDR, int, int);

static int debug_to_remove_watchpoint (CORE_ADDR, int, int);

static int debug_to_stopped_by_watchpoint (void);

static int debug_to_stopped_data_address (struct target_ops *, CORE_ADDR *);

static int debug_to_region_ok_for_hw_watchpoint (CORE_ADDR, int);

static void debug_to_terminal_init (void);

static void debug_to_terminal_inferior (void);

static void debug_to_terminal_ours_for_output (void);

static void debug_to_terminal_save_ours (void);

static void debug_to_terminal_ours (void);

static void debug_to_terminal_info (char *, int);

static void debug_to_kill (void);

static void debug_to_load (char *, int);

static int debug_to_lookup_symbol (char *, CORE_ADDR *);

static void debug_to_mourn_inferior (void);

static int debug_to_can_run (void);

static void debug_to_notice_signals (ptid_t);

static int debug_to_thread_alive (ptid_t);

static void debug_to_stop (void);

/* NOTE: cagney/2004-09-29: Many targets reference this variable in
   wierd and mysterious ways.  Putting the variable here lets those
   wierd and mysterious ways keep building while they are being
   converted to the inferior inheritance structure.  */
struct target_ops deprecated_child_ops;

/* Pointer to array of target architecture structures; the size of the
   array; the current index into the array; the allocated size of the
   array.  */
struct target_ops **target_structs;
unsigned target_struct_size;
unsigned target_struct_index;
unsigned target_struct_allocsize;
#define	DEFAULT_ALLOCSIZE	10

/* The initial current target, so that there is always a semi-valid
   current target.  */

static struct target_ops dummy_target;

/* Top of target stack.  */

static struct target_ops *target_stack;

/* The target structure we are currently using to talk to a process
   or file or whatever "inferior" we have.  */

struct target_ops current_target;

/* Command list for target.  */

static struct cmd_list_element *targetlist = NULL;

/* Nonzero if we are debugging an attached outside process
   rather than an inferior.  */

int attach_flag;

/* Nonzero if we should trust readonly sections from the
   executable when reading memory.  */

static int trust_readonly = 0;

/* Non-zero if we want to see trace of target level stuff.  */

static int targetdebug = 0;
static void
show_targetdebug (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Target debugging is %s.\n"), value);
}

static void setup_target_debug (void);

DCACHE *target_dcache;

/* The user just typed 'target' without the name of a target.  */

static void
target_command (char *arg, int from_tty)
{
  fputs_filtered ("Argument required (target name).  Try `help target'\n",
		  gdb_stdout);
}

/* Add a possible target architecture to the list.  */

void
add_target (struct target_ops *t)
{
  /* Provide default values for all "must have" methods.  */
  if (t->to_xfer_partial == NULL)
    t->to_xfer_partial = default_xfer_partial;

  if (!target_structs)
    {
      target_struct_allocsize = DEFAULT_ALLOCSIZE;
      target_structs = (struct target_ops **) xmalloc
	(target_struct_allocsize * sizeof (*target_structs));
    }
  if (target_struct_size >= target_struct_allocsize)
    {
      target_struct_allocsize *= 2;
      target_structs = (struct target_ops **)
	xrealloc ((char *) target_structs,
		  target_struct_allocsize * sizeof (*target_structs));
    }
  target_structs[target_struct_size++] = t;

  if (targetlist == NULL)
    add_prefix_cmd ("target", class_run, target_command, _("\
Connect to a target machine or process.\n\
The first argument is the type or protocol of the target machine.\n\
Remaining arguments are interpreted by the target protocol.  For more\n\
information on the arguments for a particular protocol, type\n\
`help target ' followed by the protocol name."),
		    &targetlist, "target ", 0, &cmdlist);
  add_cmd (t->to_shortname, no_class, t->to_open, t->to_doc, &targetlist);
}

/* Stub functions */

void
target_ignore (void)
{
}

void
target_load (char *arg, int from_tty)
{
  dcache_invalidate (target_dcache);
  (*current_target.to_load) (arg, from_tty);
}

static int
nomemory (CORE_ADDR memaddr, char *myaddr, int len, int write,
	  struct target_ops *t)
{
  errno = EIO;			/* Can't read/write this location */
  return 0;			/* No bytes handled */
}

static void
tcomplain (void)
{
  error (_("You can't do that when your target is `%s'"),
	 current_target.to_shortname);
}

void
noprocess (void)
{
  error (_("You can't do that without a process to debug."));
}

static int
nosymbol (char *name, CORE_ADDR *addrp)
{
  return 1;			/* Symbol does not exist in target env */
}

static void
nosupport_runtime (void)
{
  if (ptid_equal (inferior_ptid, null_ptid))
    noprocess ();
  else
    error (_("No run-time support for this"));
}


static void
default_terminal_info (char *args, int from_tty)
{
  printf_unfiltered (_("No saved terminal information.\n"));
}

/* This is the default target_create_inferior and target_attach function.
   If the current target is executing, it asks whether to kill it off.
   If this function returns without calling error(), it has killed off
   the target, and the operation should be attempted.  */

static void
kill_or_be_killed (int from_tty)
{
  if (target_has_execution)
    {
      printf_unfiltered (_("You are already running a program:\n"));
      target_files_info ();
      if (query ("Kill it? "))
	{
	  target_kill ();
	  if (target_has_execution)
	    error (_("Killing the program did not help."));
	  return;
	}
      else
	{
	  error (_("Program not killed."));
	}
    }
  tcomplain ();
}

static void
maybe_kill_then_attach (char *args, int from_tty)
{
  kill_or_be_killed (from_tty);
  target_attach (args, from_tty);
}

static void
maybe_kill_then_create_inferior (char *exec, char *args, char **env,
				 int from_tty)
{
  kill_or_be_killed (0);
  target_create_inferior (exec, args, env, from_tty);
}

/* Go through the target stack from top to bottom, copying over zero
   entries in current_target, then filling in still empty entries.  In
   effect, we are doing class inheritance through the pushed target
   vectors.

   NOTE: cagney/2003-10-17: The problem with this inheritance, as it
   is currently implemented, is that it discards any knowledge of
   which target an inherited method originally belonged to.
   Consequently, new new target methods should instead explicitly and
   locally search the target stack for the target that can handle the
   request.  */

static void
update_current_target (void)
{
  struct target_ops *t;

  /* First, reset current's contents.  */
  memset (&current_target, 0, sizeof (current_target));

#define INHERIT(FIELD, TARGET) \
      if (!current_target.FIELD) \
	current_target.FIELD = (TARGET)->FIELD

  for (t = target_stack; t; t = t->beneath)
    {
      INHERIT (to_shortname, t);
      INHERIT (to_longname, t);
      INHERIT (to_doc, t);
      INHERIT (to_open, t);
      INHERIT (to_close, t);
      INHERIT (to_attach, t);
      INHERIT (to_post_attach, t);
      INHERIT (to_detach, t);
      /* Do not inherit to_disconnect.  */
      INHERIT (to_resume, t);
      INHERIT (to_wait, t);
      INHERIT (to_fetch_registers, t);
      INHERIT (to_store_registers, t);
      INHERIT (to_prepare_to_store, t);
      INHERIT (deprecated_xfer_memory, t);
      INHERIT (to_files_info, t);
      INHERIT (to_insert_breakpoint, t);
      INHERIT (to_remove_breakpoint, t);
      INHERIT (to_can_use_hw_breakpoint, t);
      INHERIT (to_insert_hw_breakpoint, t);
      INHERIT (to_remove_hw_breakpoint, t);
      INHERIT (to_insert_watchpoint, t);
      INHERIT (to_remove_watchpoint, t);
      INHERIT (to_stopped_data_address, t);
      INHERIT (to_stopped_by_watchpoint, t);
      INHERIT (to_have_steppable_watchpoint, t);
      INHERIT (to_have_continuable_watchpoint, t);
      INHERIT (to_region_ok_for_hw_watchpoint, t);
      INHERIT (to_terminal_init, t);
      INHERIT (to_terminal_inferior, t);
      INHERIT (to_terminal_ours_for_output, t);
      INHERIT (to_terminal_ours, t);
      INHERIT (to_terminal_save_ours, t);
      INHERIT (to_terminal_info, t);
      INHERIT (to_kill, t);
      INHERIT (to_load, t);
      INHERIT (to_lookup_symbol, t);
      INHERIT (to_create_inferior, t);
      INHERIT (to_post_startup_inferior, t);
      INHERIT (to_acknowledge_created_inferior, t);
      INHERIT (to_insert_fork_catchpoint, t);
      INHERIT (to_remove_fork_catchpoint, t);
      INHERIT (to_insert_vfork_catchpoint, t);
      INHERIT (to_remove_vfork_catchpoint, t);
      /* Do not inherit to_follow_fork.  */
      INHERIT (to_insert_exec_catchpoint, t);
      INHERIT (to_remove_exec_catchpoint, t);
      INHERIT (to_reported_exec_events_per_exec_call, t);
      INHERIT (to_has_exited, t);
      INHERIT (to_mourn_inferior, t);
      INHERIT (to_can_run, t);
      INHERIT (to_notice_signals, t);
      INHERIT (to_thread_alive, t);
      INHERIT (to_find_new_threads, t);
      INHERIT (to_pid_to_str, t);
      INHERIT (to_extra_thread_info, t);
      INHERIT (to_stop, t);
      /* Do not inherit to_xfer_partial.  */
      INHERIT (to_rcmd, t);
      INHERIT (to_enable_exception_callback, t);
      INHERIT (to_get_current_exception_event, t);
      INHERIT (to_pid_to_exec_file, t);
      INHERIT (to_stratum, t);
      INHERIT (to_has_all_memory, t);
      INHERIT (to_has_memory, t);
      INHERIT (to_has_stack, t);
      INHERIT (to_has_registers, t);
      INHERIT (to_has_execution, t);
      INHERIT (to_has_thread_control, t);
      INHERIT (to_sections, t);
      INHERIT (to_sections_end, t);
      INHERIT (to_can_async_p, t);
      INHERIT (to_is_async_p, t);
      INHERIT (to_async, t);
      INHERIT (to_async_mask_value, t);
      INHERIT (to_find_memory_regions, t);
      INHERIT (to_make_corefile_notes, t);
      INHERIT (to_get_thread_local_address, t);
      /* Do not inherit to_read_description.  */
      INHERIT (to_magic, t);
      /* Do not inherit to_memory_map.  */
      /* Do not inherit to_flash_erase.  */
      /* Do not inherit to_flash_done.  */
    }
#undef INHERIT

  /* Clean up a target struct so it no longer has any zero pointers in
     it.  Some entries are defaulted to a method that print an error,
     others are hard-wired to a standard recursive default.  */

#define de_fault(field, value) \
  if (!current_target.field)               \
    current_target.field = value

  de_fault (to_open,
	    (void (*) (char *, int))
	    tcomplain);
  de_fault (to_close,
	    (void (*) (int))
	    target_ignore);
  de_fault (to_attach,
	    maybe_kill_then_attach);
  de_fault (to_post_attach,
	    (void (*) (int))
	    target_ignore);
  de_fault (to_detach,
	    (void (*) (char *, int))
	    target_ignore);
  de_fault (to_resume,
	    (void (*) (ptid_t, int, enum target_signal))
	    noprocess);
  de_fault (to_wait,
	    (ptid_t (*) (ptid_t, struct target_waitstatus *))
	    noprocess);
  de_fault (to_fetch_registers,
	    (void (*) (struct regcache *, int))
	    target_ignore);
  de_fault (to_store_registers,
	    (void (*) (struct regcache *, int))
	    noprocess);
  de_fault (to_prepare_to_store,
	    (void (*) (struct regcache *))
	    noprocess);
  de_fault (deprecated_xfer_memory,
	    (int (*) (CORE_ADDR, gdb_byte *, int, int, struct mem_attrib *, struct target_ops *))
	    nomemory);
  de_fault (to_files_info,
	    (void (*) (struct target_ops *))
	    target_ignore);
  de_fault (to_insert_breakpoint,
	    memory_insert_breakpoint);
  de_fault (to_remove_breakpoint,
	    memory_remove_breakpoint);
  de_fault (to_can_use_hw_breakpoint,
	    (int (*) (int, int, int))
	    return_zero);
  de_fault (to_insert_hw_breakpoint,
	    (int (*) (struct bp_target_info *))
	    return_minus_one);
  de_fault (to_remove_hw_breakpoint,
	    (int (*) (struct bp_target_info *))
	    return_minus_one);
  de_fault (to_insert_watchpoint,
	    (int (*) (CORE_ADDR, int, int))
	    return_minus_one);
  de_fault (to_remove_watchpoint,
	    (int (*) (CORE_ADDR, int, int))
	    return_minus_one);
  de_fault (to_stopped_by_watchpoint,
	    (int (*) (void))
	    return_zero);
  de_fault (to_stopped_data_address,
	    (int (*) (struct target_ops *, CORE_ADDR *))
	    return_zero);
  de_fault (to_region_ok_for_hw_watchpoint,
	    default_region_ok_for_hw_watchpoint);
  de_fault (to_terminal_init,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_terminal_inferior,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_terminal_ours_for_output,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_terminal_ours,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_terminal_save_ours,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_terminal_info,
	    default_terminal_info);
  de_fault (to_kill,
	    (void (*) (void))
	    noprocess);
  de_fault (to_load,
	    (void (*) (char *, int))
	    tcomplain);
  de_fault (to_lookup_symbol,
	    (int (*) (char *, CORE_ADDR *))
	    nosymbol);
  de_fault (to_create_inferior,
	    maybe_kill_then_create_inferior);
  de_fault (to_post_startup_inferior,
	    (void (*) (ptid_t))
	    target_ignore);
  de_fault (to_acknowledge_created_inferior,
	    (void (*) (int))
	    target_ignore);
  de_fault (to_insert_fork_catchpoint,
	    (void (*) (int))
	    tcomplain);
  de_fault (to_remove_fork_catchpoint,
	    (int (*) (int))
	    tcomplain);
  de_fault (to_insert_vfork_catchpoint,
	    (void (*) (int))
	    tcomplain);
  de_fault (to_remove_vfork_catchpoint,
	    (int (*) (int))
	    tcomplain);
  de_fault (to_insert_exec_catchpoint,
	    (void (*) (int))
	    tcomplain);
  de_fault (to_remove_exec_catchpoint,
	    (int (*) (int))
	    tcomplain);
  de_fault (to_reported_exec_events_per_exec_call,
	    (int (*) (void))
	    return_one);
  de_fault (to_has_exited,
	    (int (*) (int, int, int *))
	    return_zero);
  de_fault (to_mourn_inferior,
	    (void (*) (void))
	    noprocess);
  de_fault (to_can_run,
	    return_zero);
  de_fault (to_notice_signals,
	    (void (*) (ptid_t))
	    target_ignore);
  de_fault (to_thread_alive,
	    (int (*) (ptid_t))
	    return_zero);
  de_fault (to_find_new_threads,
	    (void (*) (void))
	    target_ignore);
  de_fault (to_extra_thread_info,
	    (char *(*) (struct thread_info *))
	    return_zero);
  de_fault (to_stop,
	    (void (*) (void))
	    target_ignore);
  current_target.to_xfer_partial = current_xfer_partial;
  de_fault (to_rcmd,
	    (void (*) (char *, struct ui_file *))
	    tcomplain);
  de_fault (to_enable_exception_callback,
	    (struct symtab_and_line * (*) (enum exception_event_kind, int))
	    nosupport_runtime);
  de_fault (to_get_current_exception_event,
	    (struct exception_event_record * (*) (void))
	    nosupport_runtime);
  de_fault (to_pid_to_exec_file,
	    (char *(*) (int))
	    return_zero);
  de_fault (to_can_async_p,
	    (int (*) (void))
	    return_zero);
  de_fault (to_is_async_p,
	    (int (*) (void))
	    return_zero);
  de_fault (to_async,
	    (void (*) (void (*) (enum inferior_event_type, void*), void*))
	    tcomplain);
  current_target.to_read_description = NULL;
#undef de_fault

  /* Finally, position the target-stack beneath the squashed
     "current_target".  That way code looking for a non-inherited
     target method can quickly and simply find it.  */
  current_target.beneath = target_stack;
}

/* Mark OPS as a running target.  This reverses the effect
   of target_mark_exited.  */

void
target_mark_running (struct target_ops *ops)
{
  struct target_ops *t;

  for (t = target_stack; t != NULL; t = t->beneath)
    if (t == ops)
      break;
  if (t == NULL)
    internal_error (__FILE__, __LINE__,
		    "Attempted to mark unpushed target \"%s\" as running",
		    ops->to_shortname);

  ops->to_has_execution = 1;
  ops->to_has_all_memory = 1;
  ops->to_has_memory = 1;
  ops->to_has_stack = 1;
  ops->to_has_registers = 1;

  update_current_target ();
}

/* Mark OPS as a non-running target.  This reverses the effect
   of target_mark_running.  */

void
target_mark_exited (struct target_ops *ops)
{
  struct target_ops *t;

  for (t = target_stack; t != NULL; t = t->beneath)
    if (t == ops)
      break;
  if (t == NULL)
    internal_error (__FILE__, __LINE__,
		    "Attempted to mark unpushed target \"%s\" as running",
		    ops->to_shortname);

  ops->to_has_execution = 0;
  ops->to_has_all_memory = 0;
  ops->to_has_memory = 0;
  ops->to_has_stack = 0;
  ops->to_has_registers = 0;

  update_current_target ();
}

/* Push a new target type into the stack of the existing target accessors,
   possibly superseding some of the existing accessors.

   Result is zero if the pushed target ended up on top of the stack,
   nonzero if at least one target is on top of it.

   Rather than allow an empty stack, we always have the dummy target at
   the bottom stratum, so we can call the function vectors without
   checking them.  */

int
push_target (struct target_ops *t)
{
  struct target_ops **cur;

  /* Check magic number.  If wrong, it probably means someone changed
     the struct definition, but not all the places that initialize one.  */
  if (t->to_magic != OPS_MAGIC)
    {
      fprintf_unfiltered (gdb_stderr,
			  "Magic number of %s target struct wrong\n",
			  t->to_shortname);
      internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
    }

  /* Find the proper stratum to install this target in.  */
  for (cur = &target_stack; (*cur) != NULL; cur = &(*cur)->beneath)
    {
      if ((int) (t->to_stratum) >= (int) (*cur)->to_stratum)
	break;
    }

  /* If there's already targets at this stratum, remove them.  */
  /* FIXME: cagney/2003-10-15: I think this should be popping all
     targets to CUR, and not just those at this stratum level.  */
  while ((*cur) != NULL && t->to_stratum == (*cur)->to_stratum)
    {
      /* There's already something at this stratum level.  Close it,
         and un-hook it from the stack.  */
      struct target_ops *tmp = (*cur);
      (*cur) = (*cur)->beneath;
      tmp->beneath = NULL;
      target_close (tmp, 0);
    }

  /* We have removed all targets in our stratum, now add the new one.  */
  t->beneath = (*cur);
  (*cur) = t;

  update_current_target ();

  if (targetdebug)
    setup_target_debug ();

  /* Not on top?  */
  return (t != target_stack);
}

/* Remove a target_ops vector from the stack, wherever it may be.
   Return how many times it was removed (0 or 1).  */

int
unpush_target (struct target_ops *t)
{
  struct target_ops **cur;
  struct target_ops *tmp;

  /* Look for the specified target.  Note that we assume that a target
     can only occur once in the target stack. */

  for (cur = &target_stack; (*cur) != NULL; cur = &(*cur)->beneath)
    {
      if ((*cur) == t)
	break;
    }

  if ((*cur) == NULL)
    return 0;			/* Didn't find target_ops, quit now */

  /* NOTE: cagney/2003-12-06: In '94 the close call was made
     unconditional by moving it to before the above check that the
     target was in the target stack (something about "Change the way
     pushing and popping of targets work to support target overlays
     and inheritance").  This doesn't make much sense - only open
     targets should be closed.  */
  target_close (t, 0);

  /* Unchain the target */
  tmp = (*cur);
  (*cur) = (*cur)->beneath;
  tmp->beneath = NULL;

  update_current_target ();

  return 1;
}

void
pop_target (void)
{
  target_close (&current_target, 0);	/* Let it clean up */
  if (unpush_target (target_stack) == 1)
    return;

  fprintf_unfiltered (gdb_stderr,
		      "pop_target couldn't find target %s\n",
		      current_target.to_shortname);
  internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
}

/* Using the objfile specified in BATON, find the address for the
   current thread's thread-local storage with offset OFFSET.  */
CORE_ADDR
target_translate_tls_address (struct objfile *objfile, CORE_ADDR offset)
{
  volatile CORE_ADDR addr = 0;

  if (target_get_thread_local_address_p ()
      && gdbarch_fetch_tls_load_module_address_p (current_gdbarch))
    {
      ptid_t ptid = inferior_ptid;
      volatile struct gdb_exception ex;

      TRY_CATCH (ex, RETURN_MASK_ALL)
	{
	  CORE_ADDR lm_addr;
	  
	  /* Fetch the load module address for this objfile.  */
	  lm_addr = gdbarch_fetch_tls_load_module_address (current_gdbarch,
	                                                   objfile);
	  /* If it's 0, throw the appropriate exception.  */
	  if (lm_addr == 0)
	    throw_error (TLS_LOAD_MODULE_NOT_FOUND_ERROR,
			 _("TLS load module not found"));

	  addr = target_get_thread_local_address (ptid, lm_addr, offset);
	}
      /* If an error occurred, print TLS related messages here.  Otherwise,
         throw the error to some higher catcher.  */
      if (ex.reason < 0)
	{
	  int objfile_is_library = (objfile->flags & OBJF_SHARED);

	  switch (ex.error)
	    {
	    case TLS_NO_LIBRARY_SUPPORT_ERROR:
	      error (_("Cannot find thread-local variables in this thread library."));
	      break;
	    case TLS_LOAD_MODULE_NOT_FOUND_ERROR:
	      if (objfile_is_library)
		error (_("Cannot find shared library `%s' in dynamic"
		         " linker's load module list"), objfile->name);
	      else
		error (_("Cannot find executable file `%s' in dynamic"
		         " linker's load module list"), objfile->name);
	      break;
	    case TLS_NOT_ALLOCATED_YET_ERROR:
	      if (objfile_is_library)
		error (_("The inferior has not yet allocated storage for"
		         " thread-local variables in\n"
		         "the shared library `%s'\n"
		         "for %s"),
		       objfile->name, target_pid_to_str (ptid));
	      else
		error (_("The inferior has not yet allocated storage for"
		         " thread-local variables in\n"
		         "the executable `%s'\n"
		         "for %s"),
		       objfile->name, target_pid_to_str (ptid));
	      break;
	    case TLS_GENERIC_ERROR:
	      if (objfile_is_library)
		error (_("Cannot find thread-local storage for %s, "
		         "shared library %s:\n%s"),
		       target_pid_to_str (ptid),
		       objfile->name, ex.message);
	      else
		error (_("Cannot find thread-local storage for %s, "
		         "executable file %s:\n%s"),
		       target_pid_to_str (ptid),
		       objfile->name, ex.message);
	      break;
	    default:
	      throw_exception (ex);
	      break;
	    }
	}
    }
  /* It wouldn't be wrong here to try a gdbarch method, too; finding
     TLS is an ABI-specific thing.  But we don't do that yet.  */
  else
    error (_("Cannot find thread-local variables on this target"));

  return addr;
}

#undef	MIN
#define MIN(A, B) (((A) <= (B)) ? (A) : (B))

/* target_read_string -- read a null terminated string, up to LEN bytes,
   from MEMADDR in target.  Set *ERRNOP to the errno code, or 0 if successful.
   Set *STRING to a pointer to malloc'd memory containing the data; the caller
   is responsible for freeing it.  Return the number of bytes successfully
   read.  */

int
target_read_string (CORE_ADDR memaddr, char **string, int len, int *errnop)
{
  int tlen, origlen, offset, i;
  gdb_byte buf[4];
  int errcode = 0;
  char *buffer;
  int buffer_allocated;
  char *bufptr;
  unsigned int nbytes_read = 0;

  gdb_assert (string);

  /* Small for testing.  */
  buffer_allocated = 4;
  buffer = xmalloc (buffer_allocated);
  bufptr = buffer;

  origlen = len;

  while (len > 0)
    {
      tlen = MIN (len, 4 - (memaddr & 3));
      offset = memaddr & 3;

      errcode = target_read_memory (memaddr & ~3, buf, sizeof buf);
      if (errcode != 0)
	{
	  /* The transfer request might have crossed the boundary to an
	     unallocated region of memory. Retry the transfer, requesting
	     a single byte.  */
	  tlen = 1;
	  offset = 0;
	  errcode = target_read_memory (memaddr, buf, 1);
	  if (errcode != 0)
	    goto done;
	}

      if (bufptr - buffer + tlen > buffer_allocated)
	{
	  unsigned int bytes;
	  bytes = bufptr - buffer;
	  buffer_allocated *= 2;
	  buffer = xrealloc (buffer, buffer_allocated);
	  bufptr = buffer + bytes;
	}

      for (i = 0; i < tlen; i++)
	{
	  *bufptr++ = buf[i + offset];
	  if (buf[i + offset] == '\000')
	    {
	      nbytes_read += i + 1;
	      goto done;
	    }
	}

      memaddr += tlen;
      len -= tlen;
      nbytes_read += tlen;
    }
done:
  *string = buffer;
  if (errnop != NULL)
    *errnop = errcode;
  return nbytes_read;
}

/* Find a section containing ADDR.  */
struct section_table *
target_section_by_addr (struct target_ops *target, CORE_ADDR addr)
{
  struct section_table *secp;
  for (secp = target->to_sections;
       secp < target->to_sections_end;
       secp++)
    {
      if (addr >= secp->addr && addr < secp->endaddr)
	return secp;
    }
  return NULL;
}

/* Perform a partial memory transfer.  The arguments and return
   value are just as for target_xfer_partial.  */

static LONGEST
memory_xfer_partial (struct target_ops *ops, void *readbuf, const void *writebuf,
		     ULONGEST memaddr, LONGEST len)
{
  LONGEST res;
  int reg_len;
  struct mem_region *region;

  /* Zero length requests are ok and require no work.  */
  if (len == 0)
    return 0;

  /* Try the executable file, if "trust-readonly-sections" is set.  */
  if (readbuf != NULL && trust_readonly)
    {
      struct section_table *secp;

      secp = target_section_by_addr (ops, memaddr);
      if (secp != NULL
	  && (bfd_get_section_flags (secp->bfd, secp->the_bfd_section)
	      & SEC_READONLY))
	return xfer_memory (memaddr, readbuf, len, 0, NULL, ops);
    }

  /* Likewise for accesses to unmapped overlay sections.  */
  if (readbuf != NULL && overlay_debugging)
    {
      asection *section = find_pc_overlay (memaddr);
      if (pc_in_unmapped_range (memaddr, section))
	return xfer_memory (memaddr, readbuf, len, 0, NULL, ops);
    }

  /* Try GDB's internal data cache.  */
  region = lookup_mem_region (memaddr);
  /* region->hi == 0 means there's no upper bound.  */
  if (memaddr + len < region->hi || region->hi == 0)
    reg_len = len;
  else
    reg_len = region->hi - memaddr;

  switch (region->attrib.mode)
    {
    case MEM_RO:
      if (writebuf != NULL)
	return -1;
      break;

    case MEM_WO:
      if (readbuf != NULL)
	return -1;
      break;

    case MEM_FLASH:
      /* We only support writing to flash during "load" for now.  */
      if (writebuf != NULL)
	error (_("Writing to flash memory forbidden in this context"));
      break;

    case MEM_NONE:
      return -1;
    }

  if (region->attrib.cache)
    {
      /* FIXME drow/2006-08-09: This call discards OPS, so the raw
	 memory request will start back at current_target.  */
      if (readbuf != NULL)
	res = dcache_xfer_memory (target_dcache, memaddr, readbuf,
				  reg_len, 0);
      else
	/* FIXME drow/2006-08-09: If we're going to preserve const
	   correctness dcache_xfer_memory should take readbuf and
	   writebuf.  */
	res = dcache_xfer_memory (target_dcache, memaddr,
				  (void *) writebuf,
				  reg_len, 1);
      if (res <= 0)
	return -1;
      else
	return res;
    }

  /* If none of those methods found the memory we wanted, fall back
     to a target partial transfer.  Normally a single call to
     to_xfer_partial is enough; if it doesn't recognize an object
     it will call the to_xfer_partial of the next target down.
     But for memory this won't do.  Memory is the only target
     object which can be read from more than one valid target.
     A core file, for instance, could have some of memory but
     delegate other bits to the target below it.  So, we must
     manually try all targets.  */

  do
    {
      res = ops->to_xfer_partial (ops, TARGET_OBJECT_MEMORY, NULL,
				  readbuf, writebuf, memaddr, reg_len);
      if (res > 0)
	return res;

      /* We want to continue past core files to executables, but not
	 past a running target's memory.  */
      if (ops->to_has_all_memory)
	return res;

      ops = ops->beneath;
    }
  while (ops != NULL);

  /* If we still haven't got anything, return the last error.  We
     give up.  */
  return res;
}

static LONGEST
target_xfer_partial (struct target_ops *ops,
		     enum target_object object, const char *annex,
		     void *readbuf, const void *writebuf,
		     ULONGEST offset, LONGEST len)
{
  LONGEST retval;

  gdb_assert (ops->to_xfer_partial != NULL);

  /* If this is a memory transfer, let the memory-specific code
     have a look at it instead.  Memory transfers are more
     complicated.  */
  if (object == TARGET_OBJECT_MEMORY)
    retval = memory_xfer_partial (ops, readbuf, writebuf, offset, len);
  else
    {
      enum target_object raw_object = object;

      /* If this is a raw memory transfer, request the normal
	 memory object from other layers.  */
      if (raw_object == TARGET_OBJECT_RAW_MEMORY)
	raw_object = TARGET_OBJECT_MEMORY;

      retval = ops->to_xfer_partial (ops, raw_object, annex, readbuf,
				     writebuf, offset, len);
    }

  if (targetdebug)
    {
      const unsigned char *myaddr = NULL;

      fprintf_unfiltered (gdb_stdlog,
			  "%s:target_xfer_partial (%d, %s, 0x%lx,  0x%lx,  0x%s, %s) = %s",
			  ops->to_shortname,
			  (int) object,
			  (annex ? annex : "(null)"),
			  (long) readbuf, (long) writebuf,
			  paddr_nz (offset), paddr_d (len), paddr_d (retval));

      if (readbuf)
	myaddr = readbuf;
      if (writebuf)
	myaddr = writebuf;
      if (retval > 0 && myaddr != NULL)
	{
	  int i;

	  fputs_unfiltered (", bytes =", gdb_stdlog);
	  for (i = 0; i < retval; i++)
	    {
	      if ((((long) &(myaddr[i])) & 0xf) == 0)
		{
		  if (targetdebug < 2 && i > 0)
		    {
		      fprintf_unfiltered (gdb_stdlog, " ...");
		      break;
		    }
		  fprintf_unfiltered (gdb_stdlog, "\n");
		}

	      fprintf_unfiltered (gdb_stdlog, " %02x", myaddr[i] & 0xff);
	    }
	}

      fputc_unfiltered ('\n', gdb_stdlog);
    }
  return retval;
}

/* Read LEN bytes of target memory at address MEMADDR, placing the results in
   GDB's memory at MYADDR.  Returns either 0 for success or an errno value
   if any error occurs.

   If an error occurs, no guarantee is made about the contents of the data at
   MYADDR.  In particular, the caller should not depend upon partial reads
   filling the buffer with good data.  There is no way for the caller to know
   how much good data might have been transfered anyway.  Callers that can
   deal with partial reads should call target_read (which will retry until
   it makes no progress, and then return how much was transferred). */

int
target_read_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len)
{
  if (target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
		   myaddr, memaddr, len) == len)
    return 0;
  else
    return EIO;
}

int
target_write_memory (CORE_ADDR memaddr, const gdb_byte *myaddr, int len)
{
  if (target_write (&current_target, TARGET_OBJECT_MEMORY, NULL,
		    myaddr, memaddr, len) == len)
    return 0;
  else
    return EIO;
}

/* Fetch the target's memory map.  */

VEC(mem_region_s) *
target_memory_map (void)
{
  VEC(mem_region_s) *result;
  struct mem_region *last_one, *this_one;
  int ix;
  struct target_ops *t;

  if (targetdebug)
    fprintf_unfiltered (gdb_stdlog, "target_memory_map ()\n");

  for (t = current_target.beneath; t != NULL; t = t->beneath)
    if (t->to_memory_map != NULL)
      break;

  if (t == NULL)
    return NULL;

  result = t->to_memory_map (t);
  if (result == NULL)
    return NULL;

  qsort (VEC_address (mem_region_s, result),
	 VEC_length (mem_region_s, result),
	 sizeof (struct mem_region), mem_region_cmp);

  /* Check that regions do not overlap.  Simultaneously assign
     a numbering for the "mem" commands to use to refer to
     each region.  */
  last_one = NULL;
  for (ix = 0; VEC_iterate (mem_region_s, result, ix, this_one); ix++)
    {
      this_one->number = ix;

      if (last_one && last_one->hi > this_one->lo)
	{
	  warning (_("Overlapping regions in memory map: ignoring"));
	  VEC_free (mem_region_s, result);
	  return NULL;
	}
      last_one = this_one;
    }

  return result;
}

void
target_flash_erase (ULONGEST address, LONGEST length)
{
  struct target_ops *t;

  for (t = current_target.beneath; t != NULL; t = t->beneath)
    if (t->to_flash_erase != NULL)
	{
	  if (targetdebug)
	    fprintf_unfiltered (gdb_stdlog, "target_flash_erase (%s, %s)\n",
                                paddr (address), phex (length, 0));
	  t->to_flash_erase (t, address, length);
	  return;
	}

  tcomplain ();
}

void
target_flash_done (void)
{
  struct target_ops *t;

  for (t = current_target.beneath; t != NULL; t = t->beneath)
    if (t->to_flash_done != NULL)
	{
	  if (targetdebug)
	    fprintf_unfiltered (gdb_stdlog, "target_flash_done\n");
	  t->to_flash_done (t);
	  return;
	}

  tcomplain ();
}

#ifndef target_stopped_data_address_p
int
target_stopped_data_address_p (struct target_ops *target)
{
  if (target->to_stopped_data_address
      == (int (*) (struct target_ops *, CORE_ADDR *)) return_zero)
    return 0;
  if (target->to_stopped_data_address == debug_to_stopped_data_address
      && (debug_target.to_stopped_data_address
	  == (int (*) (struct target_ops *, CORE_ADDR *)) return_zero))
    return 0;
  return 1;
}
#endif

static void
show_trust_readonly (struct ui_file *file, int from_tty,
		     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Mode for reading from readonly sections is %s.\n"),
		    value);
}

/* More generic transfers.  */

static LONGEST
default_xfer_partial (struct target_ops *ops, enum target_object object,
		      const char *annex, gdb_byte *readbuf,
		      const gdb_byte *writebuf, ULONGEST offset, LONGEST len)
{
  if (object == TARGET_OBJECT_MEMORY
      && ops->deprecated_xfer_memory != NULL)
    /* If available, fall back to the target's
       "deprecated_xfer_memory" method.  */
    {
      int xfered = -1;
      errno = 0;
      if (writebuf != NULL)
	{
	  void *buffer = xmalloc (len);
	  struct cleanup *cleanup = make_cleanup (xfree, buffer);
	  memcpy (buffer, writebuf, len);
	  xfered = ops->deprecated_xfer_memory (offset, buffer, len,
						1/*write*/, NULL, ops);
	  do_cleanups (cleanup);
	}
      if (readbuf != NULL)
	xfered = ops->deprecated_xfer_memory (offset, readbuf, len, 0/*read*/,
					      NULL, ops);
      if (xfered > 0)
	return xfered;
      else if (xfered == 0 && errno == 0)
	/* "deprecated_xfer_memory" uses 0, cross checked against
           ERRNO as one indication of an error.  */
	return 0;
      else
	return -1;
    }
  else if (ops->beneath != NULL)
    return ops->beneath->to_xfer_partial (ops->beneath, object, annex,
					  readbuf, writebuf, offset, len);
  else
    return -1;
}

/* The xfer_partial handler for the topmost target.  Unlike the default,
   it does not need to handle memory specially; it just passes all
   requests down the stack.  */

static LONGEST
current_xfer_partial (struct target_ops *ops, enum target_object object,
		      const char *annex, gdb_byte *readbuf,
		      const gdb_byte *writebuf, ULONGEST offset, LONGEST len)
{
  if (ops->beneath != NULL)
    return ops->beneath->to_xfer_partial (ops->beneath, object, annex,
					  readbuf, writebuf, offset, len);
  else
    return -1;
}

/* Target vector read/write partial wrapper functions.

   NOTE: cagney/2003-10-21: I wonder if having "to_xfer_partial
   (inbuf, outbuf)", instead of separate read/write methods, make life
   easier.  */

static LONGEST
target_read_partial (struct target_ops *ops,
		     enum target_object object,
		     const char *annex, gdb_byte *buf,
		     ULONGEST offset, LONGEST len)
{
  return target_xfer_partial (ops, object, annex, buf, NULL, offset, len);
}

static LONGEST
target_write_partial (struct target_ops *ops,
		      enum target_object object,
		      const char *annex, const gdb_byte *buf,
		      ULONGEST offset, LONGEST len)
{
  return target_xfer_partial (ops, object, annex, NULL, buf, offset, len);
}

/* Wrappers to perform the full transfer.  */
LONGEST
target_read (struct target_ops *ops,
	     enum target_object object,
	     const char *annex, gdb_byte *buf,
	     ULONGEST offset, LONGEST len)
{
  LONGEST xfered = 0;
  while (xfered < len)
    {
      LONGEST xfer = target_read_partial (ops, object, annex,
					  (gdb_byte *) buf + xfered,
					  offset + xfered, len - xfered);
      /* Call an observer, notifying them of the xfer progress?  */
      if (xfer == 0)
	return xfered;
      if (xfer < 0)
	return -1;
      xfered += xfer;
      QUIT;
    }
  return len;
}

/* An alternative to target_write with progress callbacks.  */

LONGEST
target_write_with_progress (struct target_ops *ops,
			    enum target_object object,
			    const char *annex, const gdb_byte *buf,
			    ULONGEST offset, LONGEST len,
			    void (*progress) (ULONGEST, void *), void *baton)
{
  LONGEST xfered = 0;

  /* Give the progress callback a chance to set up.  */
  if (progress)
    (*progress) (0, baton);

  while (xfered < len)
    {
      LONGEST xfer = target_write_partial (ops, object, annex,
					   (gdb_byte *) buf + xfered,
					   offset + xfered, len - xfered);

      if (xfer == 0)
	return xfered;
      if (xfer < 0)
	return -1;

      if (progress)
	(*progress) (xfer, baton);

      xfered += xfer;
      QUIT;
    }
  return len;
}

LONGEST
target_write (struct target_ops *ops,
	      enum target_object object,
	      const char *annex, const gdb_byte *buf,
	      ULONGEST offset, LONGEST len)
{
  return target_write_with_progress (ops, object, annex, buf, offset, len,
				     NULL, NULL);
}

/* Read OBJECT/ANNEX using OPS.  Store the result in *BUF_P and return
   the size of the transferred data.  PADDING additional bytes are
   available in *BUF_P.  This is a helper function for
   target_read_alloc; see the declaration of that function for more
   information.  */

static LONGEST
target_read_alloc_1 (struct target_ops *ops, enum target_object object,
		     const char *annex, gdb_byte **buf_p, int padding)
{
  size_t buf_alloc, buf_pos;
  gdb_byte *buf;
  LONGEST n;

  /* This function does not have a length parameter; it reads the
     entire OBJECT).  Also, it doesn't support objects fetched partly
     from one target and partly from another (in a different stratum,
     e.g. a core file and an executable).  Both reasons make it
     unsuitable for reading memory.  */
  gdb_assert (object != TARGET_OBJECT_MEMORY);

  /* Start by reading up to 4K at a time.  The target will throttle
     this number down if necessary.  */
  buf_alloc = 4096;
  buf = xmalloc (buf_alloc);
  buf_pos = 0;
  while (1)
    {
      n = target_read_partial (ops, object, annex, &buf[buf_pos],
			       buf_pos, buf_alloc - buf_pos - padding);
      if (n < 0)
	{
	  /* An error occurred.  */
	  xfree (buf);
	  return -1;
	}
      else if (n == 0)
	{
	  /* Read all there was.  */
	  if (buf_pos == 0)
	    xfree (buf);
	  else
	    *buf_p = buf;
	  return buf_pos;
	}

      buf_pos += n;

      /* If the buffer is filling up, expand it.  */
      if (buf_alloc < buf_pos * 2)
	{
	  buf_alloc *= 2;
	  buf = xrealloc (buf, buf_alloc);
	}

      QUIT;
    }
}

/* Read OBJECT/ANNEX using OPS.  Store the result in *BUF_P and return
   the size of the transferred data.  See the declaration in "target.h"
   function for more information about the return value.  */

LONGEST
target_read_alloc (struct target_ops *ops, enum target_object object,
		   const char *annex, gdb_byte **buf_p)
{
  return target_read_alloc_1 (ops, object, annex, buf_p, 0);
}

/* Read OBJECT/ANNEX using OPS.  The result is NUL-terminated and
   returned as a string, allocated using xmalloc.  If an error occurs
   or the transfer is unsupported, NULL is returned.  Empty objects
   are returned as allocated but empty strings.  A warning is issued
   if the result contains any embedded NUL bytes.  */

char *
target_read_stralloc (struct target_ops *ops, enum target_object object,
		      const char *annex)
{
  gdb_byte *buffer;
  LONGEST transferred;

  transferred = target_read_alloc_1 (ops, object, annex, &buffer, 1);

  if (transferred < 0)
    return NULL;

  if (transferred == 0)
    return xstrdup ("");

  buffer[transferred] = 0;
  if (strlen (buffer) < transferred)
    warning (_("target object %d, annex %s, "
	       "contained unexpected null characters"),
	     (int) object, annex ? annex : "(none)");

  return (char *) buffer;
}

/* Memory transfer methods.  */

void
get_target_memory (struct target_ops *ops, CORE_ADDR addr, gdb_byte *buf,
		   LONGEST len)
{
  if (target_read (ops, TARGET_OBJECT_MEMORY, NULL, buf, addr, len)
      != len)
    memory_error (EIO, addr);
}

ULONGEST
get_target_memory_unsigned (struct target_ops *ops,
			    CORE_ADDR addr, int len)
{
  gdb_byte buf[sizeof (ULONGEST)];

  gdb_assert (len <= sizeof (buf));
  get_target_memory (ops, addr, buf, len);
  return extract_unsigned_integer (buf, len);
}

static void
target_info (char *args, int from_tty)
{
  struct target_ops *t;
  int has_all_mem = 0;

  if (symfile_objfile != NULL)
    printf_unfiltered (_("Symbols from \"%s\".\n"), symfile_objfile->name);

  for (t = target_stack; t != NULL; t = t->beneath)
    {
      if (!t->to_has_memory)
	continue;

      if ((int) (t->to_stratum) <= (int) dummy_stratum)
	continue;
      if (has_all_mem)
	printf_unfiltered (_("\tWhile running this, GDB does not access memory from...\n"));
      printf_unfiltered ("%s:\n", t->to_longname);
      (t->to_files_info) (t);
      has_all_mem = t->to_has_all_memory;
    }
}

/* This function is called before any new inferior is created, e.g.
   by running a program, attaching, or connecting to a target.
   It cleans up any state from previous invocations which might
   change between runs.  This is a subset of what target_preopen
   resets (things which might change between targets).  */

void
target_pre_inferior (int from_tty)
{
  invalidate_target_mem_regions ();

  target_clear_description ();
}

/* This is to be called by the open routine before it does
   anything.  */

void
target_preopen (int from_tty)
{
  dont_repeat ();

  if (target_has_execution)
    {
      if (!from_tty
          || query (_("A program is being debugged already.  Kill it? ")))
	target_kill ();
      else
	error (_("Program not killed."));
    }

  /* Calling target_kill may remove the target from the stack.  But if
     it doesn't (which seems like a win for UDI), remove it now.  */

  if (target_has_execution)
    pop_target ();

  target_pre_inferior (from_tty);
}

/* Detach a target after doing deferred register stores.  */

void
target_detach (char *args, int from_tty)
{
  (current_target.to_detach) (args, from_tty);
}

void
target_disconnect (char *args, int from_tty)
{
  struct target_ops *t;

  for (t = current_target.beneath; t != NULL; t = t->beneath)
    if (t->to_disconnect != NULL)
	{
	  if (targetdebug)
	    fprintf_unfiltered (gdb_stdlog, "target_disconnect (%s, %d)\n",
				args, from_tty);
	  t->to_disconnect (t, args, from_tty);
	  return;
	}

  tcomplain ();
}

int
target_async_mask (int mask)
{
  int saved_async_masked_status = target_async_mask_value;
  target_async_mask_value = mask;
  return saved_async_masked_status;
}

/* Look through the list of possible targets for a target that can
   follow forks.  */

int
target_follow_fork (int follow_child)
{
  struct target_ops *t;

  for (t = current_target.beneath; t != NULL; t = t->beneath)
    {
      if (t->to_follow_fork != NULL)
	{
	  int retval = t->to_follow_fork (t, follow_child);
	  if (targetdebug)
	    fprintf_unfiltered (gdb_stdlog, "target_follow_fork (%d) = %d\n",
				follow_child, retval);
	  return retval;
	}
    }

  /* Some target returned a fork event, but did not know how to follow it.  */
  internal_error (__FILE__, __LINE__,
		  "could not find a target to follow fork");
}

/* Look for a target which can describe architectural features, starting
   from TARGET.  If we find one, return its description.  */

const struct target_desc *
target_read_description (struct target_ops *target)
{
  struct target_ops *t;

  for (t = target; t != NULL; t = t->beneath)
    if (t->to_read_description != NULL)
      {
	const struct target_desc *tdesc;

	tdesc = t->to_read_description (t);
	if (tdesc)
	  return tdesc;
      }

  return NULL;
}

/* Look through the list of possible targets for a target that can
   execute a run or attach command without any other data.  This is
   used to locate the default process stratum.

   Result is always valid (error() is called for errors).  */

static struct target_ops *
find_default_run_target (char *do_mesg)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_can_run && target_can_run (*t))
	{
	  runable = *t;
	  ++count;
	}
    }

  if (count != 1)
    error (_("Don't know how to %s.  Try \"help target\"."), do_mesg);

  return runable;
}

void
find_default_attach (char *args, int from_tty)
{
  struct target_ops *t;

  t = find_default_run_target ("attach");
  (t->to_attach) (args, from_tty);
  return;
}

void
find_default_create_inferior (char *exec_file, char *allargs, char **env,
			      int from_tty)
{
  struct target_ops *t;

  t = find_default_run_target ("run");
  (t->to_create_inferior) (exec_file, allargs, env, from_tty);
  return;
}

static int
default_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len)
{
  return (len <= TYPE_LENGTH (builtin_type_void_data_ptr));
}

static int
return_zero (void)
{
  return 0;
}

static int
return_one (void)
{
  return 1;
}

static int
return_minus_one (void)
{
  return -1;
}

/*
 * Resize the to_sections pointer.  Also make sure that anyone that
 * was holding on to an old value of it gets updated.
 * Returns the old size.
 */

int
target_resize_to_sections (struct target_ops *target, int num_added)
{
  struct target_ops **t;
  struct section_table *old_value;
  int old_count;

  old_value = target->to_sections;

  if (target->to_sections)
    {
      old_count = target->to_sections_end - target->to_sections;
      target->to_sections = (struct section_table *)
	xrealloc ((char *) target->to_sections,
		  (sizeof (struct section_table)) * (num_added + old_count));
    }
  else
    {
      old_count = 0;
      target->to_sections = (struct section_table *)
	xmalloc ((sizeof (struct section_table)) * num_added);
    }
  target->to_sections_end = target->to_sections + (num_added + old_count);

  /* Check to see if anyone else was pointing to this structure.
     If old_value was null, then no one was. */

  if (old_value)
    {
      for (t = target_structs; t < target_structs + target_struct_size;
	   ++t)
	{
	  if ((*t)->to_sections == old_value)
	    {
	      (*t)->to_sections = target->to_sections;
	      (*t)->to_sections_end = target->to_sections_end;
	    }
	}
      /* There is a flattened view of the target stack in current_target,
	 so its to_sections pointer might also need updating. */
      if (current_target.to_sections == old_value)
	{
	  current_target.to_sections = target->to_sections;
	  current_target.to_sections_end = target->to_sections_end;
	}
    }

  return old_count;

}

/* Remove all target sections taken from ABFD.

   Scan the current target stack for targets whose section tables
   refer to sections from BFD, and remove those sections.  We use this
   when we notice that the inferior has unloaded a shared object, for
   example.  */
void
remove_target_sections (bfd *abfd)
{
  struct target_ops **t;

  for (t = target_structs; t < target_structs + target_struct_size; t++)
    {
      struct section_table *src, *dest;

      dest = (*t)->to_sections;
      for (src = (*t)->to_sections; src < (*t)->to_sections_end; src++)
	if (src->bfd != abfd)
	  {
	    /* Keep this section.  */
	    if (dest < src) *dest = *src;
	    dest++;
	  }

      /* If we've dropped any sections, resize the section table.  */
      if (dest < src)
	target_resize_to_sections (*t, dest - src);
    }
}




/* Find a single runnable target in the stack and return it.  If for
   some reason there is more than one, return NULL.  */

struct target_ops *
find_run_target (void)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size; ++t)
    {
      if ((*t)->to_can_run && target_can_run (*t))
	{
	  runable = *t;
	  ++count;
	}
    }

  return (count == 1 ? runable : NULL);
}

/* Find a single core_stratum target in the list of targets and return it.
   If for some reason there is more than one, return NULL.  */

struct target_ops *
find_core_target (void)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_stratum == core_stratum)
	{
	  runable = *t;
	  ++count;
	}
    }

  return (count == 1 ? runable : NULL);
}

/*
 * Find the next target down the stack from the specified target.
 */

struct target_ops *
find_target_beneath (struct target_ops *t)
{
  return t->beneath;
}


/* The inferior process has died.  Long live the inferior!  */

void
generic_mourn_inferior (void)
{
  extern int show_breakpoint_hit_counts;

  inferior_ptid = null_ptid;
  attach_flag = 0;
  breakpoint_init_inferior (inf_exited);
  registers_changed ();

  reopen_exec_file ();
  reinit_frame_cache ();

  /* It is confusing to the user for ignore counts to stick around
     from previous runs of the inferior.  So clear them.  */
  /* However, it is more confusing for the ignore counts to disappear when
     using hit counts.  So don't clear them if we're counting hits.  */
  if (!show_breakpoint_hit_counts)
    breakpoint_clear_ignore_counts ();

  if (deprecated_detach_hook)
    deprecated_detach_hook ();
}

/* Helper function for child_wait and the derivatives of child_wait.
   HOSTSTATUS is the waitstatus from wait() or the equivalent; store our
   translation of that in OURSTATUS.  */
void
store_waitstatus (struct target_waitstatus *ourstatus, int hoststatus)
{
  if (WIFEXITED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_EXITED;
      ourstatus->value.integer = WEXITSTATUS (hoststatus);
    }
  else if (!WIFSTOPPED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
      ourstatus->value.sig = target_signal_from_host (WTERMSIG (hoststatus));
    }
  else
    {
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig = target_signal_from_host (WSTOPSIG (hoststatus));
    }
}

/* Returns zero to leave the inferior alone, one to interrupt it.  */
int (*target_activity_function) (void);
int target_activity_fd;

/* Convert a normal process ID to a string.  Returns the string in a
   static buffer.  */

char *
normal_pid_to_str (ptid_t ptid)
{
  static char buf[32];

  xsnprintf (buf, sizeof buf, "process %d", ptid_get_pid (ptid));
  return buf;
}

/* Error-catcher for target_find_memory_regions */
static int dummy_find_memory_regions (int (*ignore1) (), void *ignore2)
{
  error (_("No target."));
  return 0;
}

/* Error-catcher for target_make_corefile_notes */
static char * dummy_make_corefile_notes (bfd *ignore1, int *ignore2)
{
  error (_("No target."));
  return NULL;
}

/* Set up the handful of non-empty slots needed by the dummy target
   vector.  */

static void
init_dummy_target (void)
{
  dummy_target.to_shortname = "None";
  dummy_target.to_longname = "None";
  dummy_target.to_doc = "";
  dummy_target.to_attach = find_default_attach;
  dummy_target.to_create_inferior = find_default_create_inferior;
  dummy_target.to_pid_to_str = normal_pid_to_str;
  dummy_target.to_stratum = dummy_stratum;
  dummy_target.to_find_memory_regions = dummy_find_memory_regions;
  dummy_target.to_make_corefile_notes = dummy_make_corefile_notes;
  dummy_target.to_xfer_partial = default_xfer_partial;
  dummy_target.to_magic = OPS_MAGIC;
}

static void
debug_to_open (char *args, int from_tty)
{
  debug_target.to_open (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_open (%s, %d)\n", args, from_tty);
}

static void
debug_to_close (int quitting)
{
  target_close (&debug_target, quitting);
  fprintf_unfiltered (gdb_stdlog, "target_close (%d)\n", quitting);
}

void
target_close (struct target_ops *targ, int quitting)
{
  if (targ->to_xclose != NULL)
    targ->to_xclose (targ, quitting);
  else if (targ->to_close != NULL)
    targ->to_close (quitting);
}

static void
debug_to_attach (char *args, int from_tty)
{
  debug_target.to_attach (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_attach (%s, %d)\n", args, from_tty);
}


static void
debug_to_post_attach (int pid)
{
  debug_target.to_post_attach (pid);

  fprintf_unfiltered (gdb_stdlog, "target_post_attach (%d)\n", pid);
}

static void
debug_to_detach (char *args, int from_tty)
{
  debug_target.to_detach (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_detach (%s, %d)\n", args, from_tty);
}

static void
debug_to_resume (ptid_t ptid, int step, enum target_signal siggnal)
{
  debug_target.to_resume (ptid, step, siggnal);

  fprintf_unfiltered (gdb_stdlog, "target_resume (%d, %s, %s)\n", PIDGET (ptid),
		      step ? "step" : "continue",
		      target_signal_to_name (siggnal));
}

static ptid_t
debug_to_wait (ptid_t ptid, struct target_waitstatus *status)
{
  ptid_t retval;

  retval = debug_target.to_wait (ptid, status);

  fprintf_unfiltered (gdb_stdlog,
		      "target_wait (%d, status) = %d,   ", PIDGET (ptid),
		      PIDGET (retval));
  fprintf_unfiltered (gdb_stdlog, "status->kind = ");
  switch (status->kind)
    {
    case TARGET_WAITKIND_EXITED:
      fprintf_unfiltered (gdb_stdlog, "exited, status = %d\n",
			  status->value.integer);
      break;
    case TARGET_WAITKIND_STOPPED:
      fprintf_unfiltered (gdb_stdlog, "stopped, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_SIGNALLED:
      fprintf_unfiltered (gdb_stdlog, "signalled, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_LOADED:
      fprintf_unfiltered (gdb_stdlog, "loaded\n");
      break;
    case TARGET_WAITKIND_FORKED:
      fprintf_unfiltered (gdb_stdlog, "forked\n");
      break;
    case TARGET_WAITKIND_VFORKED:
      fprintf_unfiltered (gdb_stdlog, "vforked\n");
      break;
    case TARGET_WAITKIND_EXECD:
      fprintf_unfiltered (gdb_stdlog, "execd\n");
      break;
    case TARGET_WAITKIND_SPURIOUS:
      fprintf_unfiltered (gdb_stdlog, "spurious\n");
      break;
    default:
      fprintf_unfiltered (gdb_stdlog, "unknown???\n");
      break;
    }

  return retval;
}

static void
debug_print_register (const char * func,
		      struct regcache *regcache, int regno)
{
  fprintf_unfiltered (gdb_stdlog, "%s ", func);
  if (regno >= 0 && regno < gdbarch_num_regs (current_gdbarch)
			    + gdbarch_num_pseudo_regs (current_gdbarch)
      && gdbarch_register_name (current_gdbarch, regno) != NULL
      && gdbarch_register_name (current_gdbarch, regno)[0] != '\0')
    fprintf_unfiltered (gdb_stdlog, "(%s)", gdbarch_register_name
					      (current_gdbarch, regno));
  else
    fprintf_unfiltered (gdb_stdlog, "(%d)", regno);
  if (regno >= 0)
    {
      int i, size = register_size (current_gdbarch, regno);
      unsigned char buf[MAX_REGISTER_SIZE];
      regcache_cooked_read (regcache, regno, buf);
      fprintf_unfiltered (gdb_stdlog, " = ");
      for (i = 0; i < size; i++)
	{
	  fprintf_unfiltered (gdb_stdlog, "%02x", buf[i]);
	}
      if (size <= sizeof (LONGEST))
	{
	  ULONGEST val = extract_unsigned_integer (buf, size);
	  fprintf_unfiltered (gdb_stdlog, " 0x%s %s",
			      paddr_nz (val), paddr_d (val));
	}
    }
  fprintf_unfiltered (gdb_stdlog, "\n");
}

static void
debug_to_fetch_registers (struct regcache *regcache, int regno)
{
  debug_target.to_fetch_registers (regcache, regno);
  debug_print_register ("target_fetch_registers", regcache, regno);
}

static void
debug_to_store_registers (struct regcache *regcache, int regno)
{
  debug_target.to_store_registers (regcache, regno);
  debug_print_register ("target_store_registers", regcache, regno);
  fprintf_unfiltered (gdb_stdlog, "\n");
}

static void
debug_to_prepare_to_store (struct regcache *regcache)
{
  debug_target.to_prepare_to_store (regcache);

  fprintf_unfiltered (gdb_stdlog, "target_prepare_to_store ()\n");
}

static int
deprecated_debug_xfer_memory (CORE_ADDR memaddr, bfd_byte *myaddr, int len,
			      int write, struct mem_attrib *attrib,
			      struct target_ops *target)
{
  int retval;

  retval = debug_target.deprecated_xfer_memory (memaddr, myaddr, len, write,
						attrib, target);

  fprintf_unfiltered (gdb_stdlog,
		      "target_xfer_memory (0x%x, xxx, %d, %s, xxx) = %d",
		      (unsigned int) memaddr,	/* possable truncate long long */
		      len, write ? "write" : "read", retval);

  if (retval > 0)
    {
      int i;

      fputs_unfiltered (", bytes =", gdb_stdlog);
      for (i = 0; i < retval; i++)
	{
	  if ((((long) &(myaddr[i])) & 0xf) == 0)
	    {
	      if (targetdebug < 2 && i > 0)
		{
		  fprintf_unfiltered (gdb_stdlog, " ...");
		  break;
		}
	      fprintf_unfiltered (gdb_stdlog, "\n");
	    }

	  fprintf_unfiltered (gdb_stdlog, " %02x", myaddr[i] & 0xff);
	}
    }

  fputc_unfiltered ('\n', gdb_stdlog);

  return retval;
}

static void
debug_to_files_info (struct target_ops *target)
{
  debug_target.to_files_info (target);

  fprintf_unfiltered (gdb_stdlog, "target_files_info (xxx)\n");
}

static int
debug_to_insert_breakpoint (struct bp_target_info *bp_tgt)
{
  int retval;

  retval = debug_target.to_insert_breakpoint (bp_tgt);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) bp_tgt->placed_address,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_breakpoint (struct bp_target_info *bp_tgt)
{
  int retval;

  retval = debug_target.to_remove_breakpoint (bp_tgt);

  fprintf_unfiltered (gdb_stdlog,
		      "target_remove_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) bp_tgt->placed_address,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_can_use_hw_breakpoint (int type, int cnt, int from_tty)
{
  int retval;

  retval = debug_target.to_can_use_hw_breakpoint (type, cnt, from_tty);

  fprintf_unfiltered (gdb_stdlog,
		      "target_can_use_hw_breakpoint (%ld, %ld, %ld) = %ld\n",
		      (unsigned long) type,
		      (unsigned long) cnt,
		      (unsigned long) from_tty,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len)
{
  CORE_ADDR retval;

  retval = debug_target.to_region_ok_for_hw_watchpoint (addr, len);

  fprintf_unfiltered (gdb_stdlog,
		      "TARGET_REGION_OK_FOR_HW_WATCHPOINT (%ld, %ld) = 0x%lx\n",
		      (unsigned long) addr,
		      (unsigned long) len,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_stopped_by_watchpoint (void)
{
  int retval;

  retval = debug_target.to_stopped_by_watchpoint ();

  fprintf_unfiltered (gdb_stdlog,
		      "STOPPED_BY_WATCHPOINT () = %ld\n",
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_stopped_data_address (struct target_ops *target, CORE_ADDR *addr)
{
  int retval;

  retval = debug_target.to_stopped_data_address (target, addr);

  fprintf_unfiltered (gdb_stdlog,
		      "target_stopped_data_address ([0x%lx]) = %ld\n",
		      (unsigned long)*addr,
		      (unsigned long)retval);
  return retval;
}

static int
debug_to_insert_hw_breakpoint (struct bp_target_info *bp_tgt)
{
  int retval;

  retval = debug_target.to_insert_hw_breakpoint (bp_tgt);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_hw_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) bp_tgt->placed_address,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_hw_breakpoint (struct bp_target_info *bp_tgt)
{
  int retval;

  retval = debug_target.to_remove_hw_breakpoint (bp_tgt);

  fprintf_unfiltered (gdb_stdlog,
		      "target_remove_hw_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) bp_tgt->placed_address,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  int retval;

  retval = debug_target.to_insert_watchpoint (addr, len, type);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_watchpoint (0x%lx, %d, %d) = %ld\n",
		      (unsigned long) addr, len, type, (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  int retval;

  retval = debug_target.to_remove_watchpoint (addr, len, type);

  fprintf_unfiltered (gdb_stdlog,
		      "target_remove_watchpoint (0x%lx, %d, %d) = %ld\n",
		      (unsigned long) addr, len, type, (unsigned long) retval);
  return retval;
}

static void
debug_to_terminal_init (void)
{
  debug_target.to_terminal_init ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_init ()\n");
}

static void
debug_to_terminal_inferior (void)
{
  debug_target.to_terminal_inferior ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_inferior ()\n");
}

static void
debug_to_terminal_ours_for_output (void)
{
  debug_target.to_terminal_ours_for_output ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_ours_for_output ()\n");
}

static void
debug_to_terminal_ours (void)
{
  debug_target.to_terminal_ours ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_ours ()\n");
}

static void
debug_to_terminal_save_ours (void)
{
  debug_target.to_terminal_save_ours ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_save_ours ()\n");
}

static void
debug_to_terminal_info (char *arg, int from_tty)
{
  debug_target.to_terminal_info (arg, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_terminal_info (%s, %d)\n", arg,
		      from_tty);
}

static void
debug_to_kill (void)
{
  debug_target.to_kill ();

  fprintf_unfiltered (gdb_stdlog, "target_kill ()\n");
}

static void
debug_to_load (char *args, int from_tty)
{
  debug_target.to_load (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_load (%s, %d)\n", args, from_tty);
}

static int
debug_to_lookup_symbol (char *name, CORE_ADDR *addrp)
{
  int retval;

  retval = debug_target.to_lookup_symbol (name, addrp);

  fprintf_unfiltered (gdb_stdlog, "target_lookup_symbol (%s, xxx)\n", name);

  return retval;
}

static void
debug_to_create_inferior (char *exec_file, char *args, char **env,
			  int from_tty)
{
  debug_target.to_create_inferior (exec_file, args, env, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_create_inferior (%s, %s, xxx, %d)\n",
		      exec_file, args, from_tty);
}

static void
debug_to_post_startup_inferior (ptid_t ptid)
{
  debug_target.to_post_startup_inferior (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_post_startup_inferior (%d)\n",
		      PIDGET (ptid));
}

static void
debug_to_acknowledge_created_inferior (int pid)
{
  debug_target.to_acknowledge_created_inferior (pid);

  fprintf_unfiltered (gdb_stdlog, "target_acknowledge_created_inferior (%d)\n",
		      pid);
}

static void
debug_to_insert_fork_catchpoint (int pid)
{
  debug_target.to_insert_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_fork_catchpoint (%d)\n",
		      pid);
}

static int
debug_to_remove_fork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_fork_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static void
debug_to_insert_vfork_catchpoint (int pid)
{
  debug_target.to_insert_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_vfork_catchpoint (%d)\n",
		      pid);
}

static int
debug_to_remove_vfork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_vfork_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static void
debug_to_insert_exec_catchpoint (int pid)
{
  debug_target.to_insert_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_exec_catchpoint (%d)\n",
		      pid);
}

static int
debug_to_remove_exec_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_exec_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_reported_exec_events_per_exec_call (void)
{
  int reported_exec_events;

  reported_exec_events = debug_target.to_reported_exec_events_per_exec_call ();

  fprintf_unfiltered (gdb_stdlog,
		      "target_reported_exec_events_per_exec_call () = %d\n",
		      reported_exec_events);

  return reported_exec_events;
}

static int
debug_to_has_exited (int pid, int wait_status, int *exit_status)
{
  int has_exited;

  has_exited = debug_target.to_has_exited (pid, wait_status, exit_status);

  fprintf_unfiltered (gdb_stdlog, "target_has_exited (%d, %d, %d) = %d\n",
		      pid, wait_status, *exit_status, has_exited);

  return has_exited;
}

static void
debug_to_mourn_inferior (void)
{
  debug_target.to_mourn_inferior ();

  fprintf_unfiltered (gdb_stdlog, "target_mourn_inferior ()\n");
}

static int
debug_to_can_run (void)
{
  int retval;

  retval = debug_target.to_can_run ();

  fprintf_unfiltered (gdb_stdlog, "target_can_run () = %d\n", retval);

  return retval;
}

static void
debug_to_notice_signals (ptid_t ptid)
{
  debug_target.to_notice_signals (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_notice_signals (%d)\n",
                      PIDGET (ptid));
}

static int
debug_to_thread_alive (ptid_t ptid)
{
  int retval;

  retval = debug_target.to_thread_alive (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_thread_alive (%d) = %d\n",
		      PIDGET (ptid), retval);

  return retval;
}

static void
debug_to_find_new_threads (void)
{
  debug_target.to_find_new_threads ();

  fputs_unfiltered ("target_find_new_threads ()\n", gdb_stdlog);
}

static void
debug_to_stop (void)
{
  debug_target.to_stop ();

  fprintf_unfiltered (gdb_stdlog, "target_stop ()\n");
}

static void
debug_to_rcmd (char *command,
	       struct ui_file *outbuf)
{
  debug_target.to_rcmd (command, outbuf);
  fprintf_unfiltered (gdb_stdlog, "target_rcmd (%s, ...)\n", command);
}

static struct symtab_and_line *
debug_to_enable_exception_callback (enum exception_event_kind kind, int enable)
{
  struct symtab_and_line *result;
  result = debug_target.to_enable_exception_callback (kind, enable);
  fprintf_unfiltered (gdb_stdlog,
		      "target get_exception_callback_sal (%d, %d)\n",
		      kind, enable);
  return result;
}

static struct exception_event_record *
debug_to_get_current_exception_event (void)
{
  struct exception_event_record *result;
  result = debug_target.to_get_current_exception_event ();
  fprintf_unfiltered (gdb_stdlog, "target get_current_exception_event ()\n");
  return result;
}

static char *
debug_to_pid_to_exec_file (int pid)
{
  char *exec_file;

  exec_file = debug_target.to_pid_to_exec_file (pid);

  fprintf_unfiltered (gdb_stdlog, "target_pid_to_exec_file (%d) = %s\n",
		      pid, exec_file);

  return exec_file;
}

static void
setup_target_debug (void)
{
  memcpy (&debug_target, &current_target, sizeof debug_target);

  current_target.to_open = debug_to_open;
  current_target.to_close = debug_to_close;
  current_target.to_attach = debug_to_attach;
  current_target.to_post_attach = debug_to_post_attach;
  current_target.to_detach = debug_to_detach;
  current_target.to_resume = debug_to_resume;
  current_target.to_wait = debug_to_wait;
  current_target.to_fetch_registers = debug_to_fetch_registers;
  current_target.to_store_registers = debug_to_store_registers;
  current_target.to_prepare_to_store = debug_to_prepare_to_store;
  current_target.deprecated_xfer_memory = deprecated_debug_xfer_memory;
  current_target.to_files_info = debug_to_files_info;
  current_target.to_insert_breakpoint = debug_to_insert_breakpoint;
  current_target.to_remove_breakpoint = debug_to_remove_breakpoint;
  current_target.to_can_use_hw_breakpoint = debug_to_can_use_hw_breakpoint;
  current_target.to_insert_hw_breakpoint = debug_to_insert_hw_breakpoint;
  current_target.to_remove_hw_breakpoint = debug_to_remove_hw_breakpoint;
  current_target.to_insert_watchpoint = debug_to_insert_watchpoint;
  current_target.to_remove_watchpoint = debug_to_remove_watchpoint;
  current_target.to_stopped_by_watchpoint = debug_to_stopped_by_watchpoint;
  current_target.to_stopped_data_address = debug_to_stopped_data_address;
  current_target.to_region_ok_for_hw_watchpoint = debug_to_region_ok_for_hw_watchpoint;
  current_target.to_terminal_init = debug_to_terminal_init;
  current_target.to_terminal_inferior = debug_to_terminal_inferior;
  current_target.to_terminal_ours_for_output = debug_to_terminal_ours_for_output;
  current_target.to_terminal_ours = debug_to_terminal_ours;
  current_target.to_terminal_save_ours = debug_to_terminal_save_ours;
  current_target.to_terminal_info = debug_to_terminal_info;
  current_target.to_kill = debug_to_kill;
  current_target.to_load = debug_to_load;
  current_target.to_lookup_symbol = debug_to_lookup_symbol;
  current_target.to_create_inferior = debug_to_create_inferior;
  current_target.to_post_startup_inferior = debug_to_post_startup_inferior;
  current_target.to_acknowledge_created_inferior = debug_to_acknowledge_created_inferior;
  current_target.to_insert_fork_catchpoint = debug_to_insert_fork_catchpoint;
  current_target.to_remove_fork_catchpoint = debug_to_remove_fork_catchpoint;
  current_target.to_insert_vfork_catchpoint = debug_to_insert_vfork_catchpoint;
  current_target.to_remove_vfork_catchpoint = debug_to_remove_vfork_catchpoint;
  current_target.to_insert_exec_catchpoint = debug_to_insert_exec_catchpoint;
  current_target.to_remove_exec_catchpoint = debug_to_remove_exec_catchpoint;
  current_target.to_reported_exec_events_per_exec_call = debug_to_reported_exec_events_per_exec_call;
  current_target.to_has_exited = debug_to_has_exited;
  current_target.to_mourn_inferior = debug_to_mourn_inferior;
  current_target.to_can_run = debug_to_can_run;
  current_target.to_notice_signals = debug_to_notice_signals;
  current_target.to_thread_alive = debug_to_thread_alive;
  current_target.to_find_new_threads = debug_to_find_new_threads;
  current_target.to_stop = debug_to_stop;
  current_target.to_rcmd = debug_to_rcmd;
  current_target.to_enable_exception_callback = debug_to_enable_exception_callback;
  current_target.to_get_current_exception_event = debug_to_get_current_exception_event;
  current_target.to_pid_to_exec_file = debug_to_pid_to_exec_file;
}


static char targ_desc[] =
"Names of targets and files being debugged.\n\
Shows the entire stack of targets currently in use (including the exec-file,\n\
core-file, and process, if any), as well as the symbol file name.";

static void
do_monitor_command (char *cmd,
		 int from_tty)
{
  if ((current_target.to_rcmd
       == (void (*) (char *, struct ui_file *)) tcomplain)
      || (current_target.to_rcmd == debug_to_rcmd
	  && (debug_target.to_rcmd
	      == (void (*) (char *, struct ui_file *)) tcomplain)))
    error (_("\"monitor\" command not supported by this target."));
  target_rcmd (cmd, gdb_stdtarg);
}

/* Print the name of each layers of our target stack.  */

static void
maintenance_print_target_stack (char *cmd, int from_tty)
{
  struct target_ops *t;

  printf_filtered (_("The current target stack is:\n"));

  for (t = target_stack; t != NULL; t = t->beneath)
    {
      printf_filtered ("  - %s (%s)\n", t->to_shortname, t->to_longname);
    }
}

void
initialize_targets (void)
{
  init_dummy_target ();
  push_target (&dummy_target);

  add_info ("target", target_info, targ_desc);
  add_info ("files", target_info, targ_desc);

  add_setshow_zinteger_cmd ("target", class_maintenance, &targetdebug, _("\
Set target debugging."), _("\
Show target debugging."), _("\
When non-zero, target debugging is enabled.  Higher numbers are more\n\
verbose.  Changes do not take effect until the next \"run\" or \"target\"\n\
command."),
			    NULL,
			    show_targetdebug,
			    &setdebuglist, &showdebuglist);

  add_setshow_boolean_cmd ("trust-readonly-sections", class_support,
			   &trust_readonly, _("\
Set mode for reading from readonly sections."), _("\
Show mode for reading from readonly sections."), _("\
When this mode is on, memory reads from readonly sections (such as .text)\n\
will be read from the object file instead of from the target.  This will\n\
result in significant performance improvement for remote targets."),
			   NULL,
			   show_trust_readonly,
			   &setlist, &showlist);

  add_com ("monitor", class_obscure, do_monitor_command,
	   _("Send a command to the remote monitor (remote targets only)."));

  add_cmd ("target-stack", class_maintenance, maintenance_print_target_stack,
           _("Print the name of each layer of the internal target stack."),
           &maintenanceprintlist);

  target_dcache = dcache_init ();
}
