/* GNU/Linux native-dependent code for debugging multiple forks.

   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdbcmd.h"
#include "infcall.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "linux-fork.h"
#include "linux-nat.h"

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <dirent.h>
#include <ctype.h>

struct fork_info *fork_list;
static int highest_fork_num;

/* Prevent warning from -Wmissing-prototypes.  */
extern void _initialize_linux_fork (void);

int detach_fork = 1;		/* Default behavior is to detach
				   newly forked processes (legacy).  */

/* Fork list data structure:  */
struct fork_info
{
  struct fork_info *next;
  ptid_t ptid;
  int num;			/* Convenient handle (GDB fork id) */
  struct regcache *savedregs;	/* Convenient for info fork, saves 
				   having to actually switch contexts.  */
  int clobber_regs;		/* True if we should restore saved regs.  */
  ULONGEST pc;			/* PC for info fork.  */
  off_t *filepos;		/* Set of open file descriptors' offsets.  */
  int maxfd;
};

/* Fork list methods:  */

extern int
forks_exist_p (void)
{
  return (fork_list != NULL);
}

/* Add a fork to internal fork list.
   Called from linux child_follow_fork.  */

extern struct fork_info *
add_fork (pid_t pid)
{
  struct fork_info *fp;

  if (fork_list == NULL && pid != PIDGET (inferior_ptid))
    {
      /* Special case -- if this is the first fork in the list
	 (the list is hitherto empty), and if this new fork is
	 NOT the current inferior_ptid, then add inferior_ptid
	 first, as a special zeroeth fork id.  */
      highest_fork_num = -1;
      add_fork (PIDGET (inferior_ptid));	/* safe recursion */
    }

  fp = XZALLOC (struct fork_info);
  fp->ptid = ptid_build (pid, pid, 0);
  fp->num = ++highest_fork_num;
  fp->next = fork_list;
  fork_list = fp;
  return fp;
}

static void
free_fork (struct fork_info *fp)
{
  /* Notes on step-resume breakpoints: since this is a concern for
     threads, let's convince ourselves that it's not a concern for
     forks.  There are two ways for a fork_info to be created.  First,
     by the checkpoint command, in which case we're at a gdb prompt
     and there can't be any step-resume breakpoint.  Second, by a fork
     in the user program, in which case we *may* have stepped into the
     fork call, but regardless of whether we follow the parent or the
     child, we will return to the same place and the step-resume
     breakpoint, if any, will take care of itself as usual.  And
     unlike threads, we do not save a private copy of the step-resume
     breakpoint -- so we're OK.  */

  if (fp)
    {
      if (fp->savedregs)
	regcache_xfree (fp->savedregs);
      if (fp->filepos)
	xfree (fp->filepos);
      xfree (fp);
    }
}

static void
delete_fork (ptid_t ptid)
{
  struct fork_info *fp, *fpprev;

  fpprev = NULL;

  for (fp = fork_list; fp; fpprev = fp, fp = fp->next)
    if (ptid_equal (fp->ptid, ptid))
      break;

  if (!fp)
    return;

  if (fpprev)
    fpprev->next = fp->next;
  else
    fork_list = fp->next;

  free_fork (fp);

  /* Special case: if there is now only one process in the list, 
     and if it is (hopefully!) the current inferior_ptid, then
     remove it, leaving the list empty -- we're now down to the
     default case of debugging a single process.  */
  if (fork_list != NULL && fork_list->next == NULL &&
      ptid_equal (fork_list->ptid, inferior_ptid))
    {
      /* Last fork -- delete from list and handle as solo process
	 (should be a safe recursion).  */
      delete_fork (inferior_ptid);
    }
}

/* Find a fork_info by matching PTID.  */
static struct fork_info *
find_fork_ptid (ptid_t ptid)
{
  struct fork_info *fp;

  for (fp = fork_list; fp; fp = fp->next)
    if (ptid_equal (fp->ptid, ptid))
      return fp;

  return NULL;
}

/* Find a fork_info by matching ID.  */
static struct fork_info *
find_fork_id (int num)
{
  struct fork_info *fp;

  for (fp = fork_list; fp; fp = fp->next)
    if (fp->num == num)
      return fp;

  return NULL;
}

/* Find a fork_info by matching pid.  */
extern struct fork_info *
find_fork_pid (pid_t pid)
{
  struct fork_info *fp;

  for (fp = fork_list; fp; fp = fp->next)
    if (pid == ptid_get_pid (fp->ptid))
      return fp;

  return NULL;
}

static ptid_t
fork_id_to_ptid (int num)
{
  struct fork_info *fork = find_fork_id (num);
  if (fork)
    return fork->ptid;
  else
    return pid_to_ptid (-1);
}

static void
init_fork_list (void)
{
  struct fork_info *fp, *fpnext;

  if (!fork_list)
    return;

  for (fp = fork_list; fp; fp = fpnext)
    {
      fpnext = fp->next;
      free_fork (fp);
    }

  fork_list = NULL;
}

/* Fork list <-> gdb interface.  */

/* Utility function for fork_load/fork_save.  
   Calls lseek in the (current) inferior process.  */

static off_t
call_lseek (int fd, off_t offset, int whence)
{
  char exp[80];

  snprintf (&exp[0], sizeof (exp), "lseek (%d, %ld, %d)",
	    fd, (long) offset, whence);
  return (off_t) parse_and_eval_long (&exp[0]);
}

/* Load infrun state for the fork PTID.  */

static void
fork_load_infrun_state (struct fork_info *fp)
{
  extern void nullify_last_target_wait_ptid ();
  int i;

  inferior_ptid = fp->ptid;

  linux_nat_switch_fork (inferior_ptid);

  if (fp->savedregs && fp->clobber_regs)
    regcache_cpy (get_current_regcache (), fp->savedregs);

  registers_changed ();
  reinit_frame_cache ();

  stop_pc = read_pc ();
  nullify_last_target_wait_ptid ();

  /* Now restore the file positions of open file descriptors.  */
  if (fp->filepos)
    {
      for (i = 0; i <= fp->maxfd; i++)
	if (fp->filepos[i] != (off_t) -1)
	  call_lseek (i, fp->filepos[i], SEEK_SET);
      /* NOTE: I can get away with using SEEK_SET and SEEK_CUR because
	 this is native-only.  If it ever has to be cross, we'll have
	 to rethink this.  */
    }
}

/* Save infrun state for the fork PTID.
   Exported for use by linux child_follow_fork.  */

extern void
fork_save_infrun_state (struct fork_info *fp, int clobber_regs)
{
  char path[MAXPATHLEN];
  struct dirent *de;
  DIR *d;

  if (fp->savedregs)
    regcache_xfree (fp->savedregs);

  fp->savedregs = regcache_dup (get_current_regcache ());
  fp->clobber_regs = clobber_regs;
  fp->pc = read_pc ();

  if (clobber_regs)
    {
      /* Now save the 'state' (file position) of all open file descriptors.
	 Unfortunately fork does not take care of that for us...  */
      snprintf (path, MAXPATHLEN, "/proc/%ld/fd", (long) PIDGET (fp->ptid));
      if ((d = opendir (path)) != NULL)
	{
	  long tmp;

	  fp->maxfd = 0;
	  while ((de = readdir (d)) != NULL)
	    {
	      /* Count open file descriptors (actually find highest
		 numbered).  */
	      tmp = strtol (&de->d_name[0], NULL, 10);
	      if (fp->maxfd < tmp)
		fp->maxfd = tmp;
	    }
	  /* Allocate array of file positions.  */
	  fp->filepos = xrealloc (fp->filepos, 
				  (fp->maxfd + 1) * sizeof (*fp->filepos));

	  /* Initialize to -1 (invalid).  */
	  for (tmp = 0; tmp <= fp->maxfd; tmp++)
	    fp->filepos[tmp] = -1;

	  /* Now find actual file positions.  */
	  rewinddir (d);
	  while ((de = readdir (d)) != NULL)
	    if (isdigit (de->d_name[0]))
	      {
		tmp = strtol (&de->d_name[0], NULL, 10);
		fp->filepos[tmp] = call_lseek (tmp, 0, SEEK_CUR);
	      }
	  closedir (d);
	}
    }
}

/* Kill 'em all, let God sort 'em out...  */

extern void
linux_fork_killall (void)
{
  /* Walk list and kill every pid.  No need to treat the
     current inferior_ptid as special (we do not return a
     status for it) -- however any process may be a child
     or a parent, so may get a SIGCHLD from a previously
     killed child.  Wait them all out.  */
  struct fork_info *fp;
  pid_t pid, ret;
  int status;

  for (fp = fork_list; fp; fp = fp->next)
    {
      pid = PIDGET (fp->ptid);
      do {
	ptrace (PT_KILL, pid, 0, 0);
	ret = waitpid (pid, &status, 0);
	/* We might get a SIGCHLD instead of an exit status.  This is
	 aggravated by the first kill above - a child has just
	 died.  MVS comment cut-and-pasted from linux-nat.  */
      } while (ret == pid && WIFSTOPPED (status));
    }
  init_fork_list ();	/* Clear list, prepare to start fresh.  */
}

/* The current inferior_ptid has exited, but there are other viable
   forks to debug.  Delete the exiting one and context-switch to the
   first available.  */

extern void
linux_fork_mourn_inferior (void)
{
  /* Wait just one more time to collect the inferior's exit status.
     Do not check whether this succeeds though, since we may be
     dealing with a process that we attached to.  Such a process will
     only report its exit status to its original parent.  */
  int status;

  waitpid (ptid_get_pid (inferior_ptid), &status, 0);

  /* OK, presumably inferior_ptid is the one who has exited.
     We need to delete that one from the fork_list, and switch
     to the next available fork.  */
  delete_fork (inferior_ptid);

  /* There should still be a fork - if there's only one left,
     delete_fork won't remove it, because we haven't updated
     inferior_ptid yet.  */
  gdb_assert (fork_list);

  fork_load_infrun_state (fork_list);
  printf_filtered (_("[Switching to %s]\n"),
		   target_pid_to_str (inferior_ptid));

  /* If there's only one fork, switch back to non-fork mode.  */
  if (fork_list->next == NULL)
    delete_fork (inferior_ptid);
}

/* Fork list <-> user interface.  */

static void
delete_fork_command (char *args, int from_tty)
{
  ptid_t ptid;

  if (!args || !*args)
    error (_("Requires argument (fork/checkpoint id to delete)"));

  ptid = fork_id_to_ptid (parse_and_eval_long (args));
  if (ptid_equal (ptid, minus_one_ptid))
    error (_("No such fork/checkpoint id, %s"), args);

  if (ptid_equal (ptid, inferior_ptid))
    error (_("Please switch to another fork/checkpoint before deleting the current one"));

  if (ptrace (PTRACE_KILL, PIDGET (ptid), 0, 0))
    error (_("Unable to kill pid %s"), target_tid_to_str (ptid));

  if (from_tty)
    printf_filtered (_("Killed %s\n"), target_pid_to_str (ptid));

  delete_fork (ptid);
}

static void
detach_fork_command (char *args, int from_tty)
{
  ptid_t ptid;

  if (!args || !*args)
    error (_("Requires argument (fork id to detach)"));

  ptid = fork_id_to_ptid (parse_and_eval_long (args));
  if (ptid_equal (ptid, minus_one_ptid))
    error (_("No such fork id, %s"), args);

  if (ptid_equal (ptid, inferior_ptid))
    error (_("Please switch to another fork before detaching the current one"));

  if (ptrace (PTRACE_DETACH, PIDGET (ptid), 0, 0))
    error (_("Unable to detach %s"), target_pid_to_str (ptid));

  if (from_tty)
    printf_filtered (_("Detached %s\n"), target_pid_to_str (ptid));

  delete_fork (ptid);
}

/* Print information about currently known forks.  */

static void
info_forks_command (char *arg, int from_tty)
{
  struct frame_info *cur_frame;
  struct symtab_and_line sal;
  struct symtab *cur_symtab;
  struct fork_info *fp;
  int cur_line;
  ULONGEST pc;
  int requested = -1;
  struct fork_info *printed = NULL;

  if (arg && *arg)
    requested = (int) parse_and_eval_long (arg);

  for (fp = fork_list; fp; fp = fp->next)
    {
      if (requested > 0 && fp->num != requested)
	continue;

      printed = fp;
      if (ptid_equal (fp->ptid, inferior_ptid))
	{
	  printf_filtered ("* ");
	  pc = read_pc ();
	}
      else
	{
	  printf_filtered ("  ");
	  pc = fp->pc;
	}
      printf_filtered ("%d %s", fp->num, target_pid_to_str (fp->ptid));
      if (fp->num == 0)
	printf_filtered (_(" (main process)"));
      printf_filtered (_(" at "));
      deprecated_print_address_numeric (pc, 1, gdb_stdout);

      sal = find_pc_line (pc, 0);
      if (sal.symtab)
	{
	  char *tmp = strrchr (sal.symtab->filename, '/');

	  if (tmp)
	    printf_filtered (_(", file %s"), tmp + 1);
	  else
	    printf_filtered (_(", file %s"), sal.symtab->filename);
	}
      if (sal.line)
	printf_filtered (_(", line %d"), sal.line);
      if (!sal.symtab && !sal.line)
	{
	  struct minimal_symbol *msym;

	  msym = lookup_minimal_symbol_by_pc (pc);
	  if (msym)
	    printf_filtered (", <%s>", SYMBOL_LINKAGE_NAME (msym));
	}

      putchar_filtered ('\n');
    }
  if (printed == NULL)
    {
      if (requested > 0)
	printf_filtered (_("No fork number %d.\n"), requested);
      else
	printf_filtered (_("No forks.\n"));
    }
}

/* Save/restore mode variable 'detach_fork':
   We need to temporarily take over this mode variable, while
   preserving the user-specified state, and make sure that it 
   gets restored in case of error.

   The int pointer that we use comes from the caller, so we can
   be called more than once (even though currently we don't need to).  */

static void 
restore_detach_fork (void *arg)
{
  detach_fork = *(int *) arg;
}

static struct cleanup *
save_detach_fork (int *saved_val)
{
  *saved_val = detach_fork;
  return make_cleanup (restore_detach_fork, (void *) saved_val);
}

static void
checkpoint_command (char *args, int from_tty)
{
  struct target_waitstatus last_target_waitstatus;
  ptid_t last_target_ptid;
  struct value *fork_fn = NULL, *ret;
  struct fork_info *fp;
  pid_t retpid;
  struct cleanup *old_chain;
  long i;
  /* Make this temp var static, 'cause it's used in the error context.  */
  static int temp_detach_fork;

  /* Make the inferior fork, record its (and gdb's) state.  */

  if (lookup_minimal_symbol ("fork", NULL, NULL) != NULL)
    fork_fn = find_function_in_inferior ("fork");
  if (!fork_fn)
    if (lookup_minimal_symbol ("_fork", NULL, NULL) != NULL)
      fork_fn = find_function_in_inferior ("fork");
  if (!fork_fn)
    error (_("checkpoint: can't find fork function in inferior."));

  ret = value_from_longest (builtin_type_int, 0);
  old_chain = save_detach_fork (&temp_detach_fork);
  detach_fork = 0;
  ret = call_function_by_hand (fork_fn, 0, &ret);
  do_cleanups (old_chain);
  if (!ret)	/* Probably can't happen.  */
    error (_("checkpoint: call_function_by_hand returned null."));

  retpid = value_as_long (ret);
  get_last_target_status (&last_target_ptid, &last_target_waitstatus);
  if (from_tty)
    {
      int parent_pid;

      printf_filtered (_("checkpoint: fork returned pid %ld.\n"), 
		       (long) retpid);
      if (info_verbose)
	{
	  parent_pid = ptid_get_lwp (last_target_ptid);
	  if (parent_pid == 0)
	    parent_pid = ptid_get_pid (last_target_ptid);
	  printf_filtered (_("   gdb says parent = %ld.\n"), 
			   (long) parent_pid);
	}
    }

  fp = find_fork_pid (retpid);
  if (!fp)
    error (_("Failed to find new fork"));
  fork_save_infrun_state (fp, 1);
}

static void
linux_fork_context (struct fork_info *newfp, int from_tty)
{
  /* Now we attempt to switch processes.  */
  struct fork_info *oldfp = find_fork_ptid (inferior_ptid);
  ptid_t ptid;
  int id, i;

  if (!newfp)
    error (_("No such fork/process"));

  if (!oldfp)
    oldfp = add_fork (ptid_get_pid (inferior_ptid));

  fork_save_infrun_state (oldfp, 1);
  fork_load_infrun_state (newfp);

  printf_filtered (_("Switching to %s\n"), 
		   target_pid_to_str (inferior_ptid));

  print_stack_frame (get_selected_frame (NULL), 1, SRC_AND_LOC);
}

/* Switch inferior process (fork) context, by process id.  */
static void
process_command (char *args, int from_tty)
{
  struct fork_info *fp;

  if (!args || !*args)
    error (_("Requires argument (process id to switch to)"));

  if ((fp = find_fork_pid (parse_and_eval_long (args))) == NULL)
    error (_("Not found: process id %s"), args);

  linux_fork_context (fp, from_tty);
}

/* Switch inferior process (fork) context, by fork id.  */
static void
fork_command (char *args, int from_tty)
{
  struct fork_info *fp;

  if (!args || !*args)
    error (_("Requires argument (fork id to switch to)"));

  if ((fp = find_fork_id (parse_and_eval_long (args))) == NULL)
    error (_("Not found: fork id %s"), args);

  linux_fork_context (fp, from_tty);
}

/* Switch inferior process (fork) context, by checkpoint id.  */
static void
restart_command (char *args, int from_tty)
{
  struct fork_info *fp;

  if (!args || !*args)
    error (_("Requires argument (checkpoint id to restart)"));

  if ((fp = find_fork_id (parse_and_eval_long (args))) == NULL)
    error (_("Not found: checkpoint id %s"), args);

  linux_fork_context (fp, from_tty);
}

void
_initialize_linux_fork (void)
{
  init_fork_list ();

  /* Set/show detach-on-fork: user-settable mode.  */

  add_setshow_boolean_cmd ("detach-on-fork", class_obscure, &detach_fork, _("\
Set whether gdb will detach the child of a fork."), _("\
Show whether gdb will detach the child of a fork."), _("\
Tells gdb whether to detach the child of a fork."), 
			   NULL, NULL, &setlist, &showlist);

  /* Set/show restart-auto-finish: user-settable count.  Causes the
     first "restart" of a fork to do some number of "finish" commands
     before returning to user.

     Useful because otherwise the virgin fork process will be stopped
     somewhere in the un-interesting fork system call.  */

  /* Checkpoint command: create a fork of the inferior process
     and set it aside for later debugging.  */

  add_com ("checkpoint", class_obscure, checkpoint_command, _("\
Fork a duplicate process (experimental)."));

  /* Restart command: restore the context of a specified fork
     process.  May be used for "program forks" as well as for
     "debugger forks" (checkpoints).  */

  add_com ("restart", class_obscure, restart_command, _("\
restart <n>: restore program context from a checkpoint.\n\
Argument 'n' is checkpoint ID, as displayed by 'info checkpoints'."));

  /* Delete checkpoint command: kill the process and remove it from
     fork list.  */

  add_cmd ("checkpoint", class_obscure, delete_fork_command, _("\
Delete a fork/checkpoint (experimental)."),
	   &deletelist);

  /* Detach checkpoint command: release the process to run independently, 
     and remove it from the fork list.  */

  add_cmd ("checkpoint", class_obscure, detach_fork_command, _("\
Detach from a fork/checkpoint (experimental)."),
	   &detachlist);

  /* Info checkpoints command: list all forks/checkpoints 
     currently under gdb's control.  */

  add_info ("checkpoints", info_forks_command,
	    _("IDs of currently known forks/checkpoints."));

  /* Command aliases (let "fork" and "checkpoint" be used 
     interchangeably).  */

  add_alias_cmd ("fork", "checkpoint", class_obscure, 1, &deletelist);
  add_alias_cmd ("fork", "checkpoint", class_obscure, 1, &detachlist);
  add_info_alias ("forks", "checkpoints", 0);

  /* "fork <n>" (by analogy to "thread <n>").  */
  add_com ("fork", class_obscure, fork_command, _("\
fork <n>: Switch between forked processes.\n\
Argument 'n' is fork ID, as displayed by 'info forks'."));

  /* "process <proc id>" as opposed to "fork <fork id>".  */
  add_com ("process", class_obscure, process_command, _("\
process <pid>: Switch between forked processes.\n\
Argument 'pid' is process ID, as displayed by 'info forks' or 'shell ps'."));
}
