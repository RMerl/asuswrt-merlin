/* Low-level child interface to ptrace.

   Copyright (C) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998,
   1999, 2000, 2001, 2002, 2004, 2005, 2006, 2007
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

#include "defs.h"
#include "command.h"
#include "inferior.h"
#include "inflow.h"
#include "gdbcore.h"
#include "regcache.h"

#include "gdb_stdint.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "gdb_ptrace.h"
#include "gdb_wait.h"
#include <signal.h>

#include "inf-child.h"

/* HACK: Save the ptrace ops returned by inf_ptrace_target.  */
static struct target_ops *ptrace_ops_hack;


#ifdef PT_GET_PROCESS_STATE

static int
inf_ptrace_follow_fork (struct target_ops *ops, int follow_child)
{
  pid_t pid, fpid;
  ptrace_state_t pe;

  /* FIXME: kettenis/20050720: This stuff should really be passed as
     an argument by our caller.  */
  {
    ptid_t ptid;
    struct target_waitstatus status;

    get_last_target_status (&ptid, &status);
    gdb_assert (status.kind == TARGET_WAITKIND_FORKED);

    pid = ptid_get_pid (ptid);
  }

  if (ptrace (PT_GET_PROCESS_STATE, pid,
	       (PTRACE_TYPE_ARG3)&pe, sizeof pe) == -1)
    perror_with_name (("ptrace"));

  gdb_assert (pe.pe_report_event == PTRACE_FORK);
  fpid = pe.pe_other_pid;

  if (follow_child)
    {
      inferior_ptid = pid_to_ptid (fpid);
      detach_breakpoints (pid);

      /* Reset breakpoints in the child as appropriate.  */
      follow_inferior_reset_breakpoints ();

      if (ptrace (PT_DETACH, pid, (PTRACE_TYPE_ARG3)1, 0) == -1)
	perror_with_name (("ptrace"));
    }
  else
    {
      inferior_ptid = pid_to_ptid (pid);
      detach_breakpoints (fpid);

      if (ptrace (PT_DETACH, fpid, (PTRACE_TYPE_ARG3)1, 0) == -1)
	perror_with_name (("ptrace"));
    }

  return 0;
}

#endif /* PT_GET_PROCESS_STATE */


/* Prepare to be traced.  */

static void
inf_ptrace_me (void)
{
  /* "Trace me, Dr. Memory!"  */
  ptrace (PT_TRACE_ME, 0, (PTRACE_TYPE_ARG3)0, 0);
}

/* Start tracing PID.  */

static void
inf_ptrace_him (int pid)
{
  push_target (ptrace_ops_hack);

  /* On some targets, there must be some explicit synchronization
     between the parent and child processes after the debugger
     forks, and before the child execs the debuggee program.  This
     call basically gives permission for the child to exec.  */

  target_acknowledge_created_inferior (pid);

  /* START_INFERIOR_TRAPS_EXPECTED is defined in inferior.h, and will
     be 1 or 2 depending on whether we're starting without or with a
     shell.  */
  startup_inferior (START_INFERIOR_TRAPS_EXPECTED);

  /* On some targets, there must be some explicit actions taken after
     the inferior has been started up.  */
  target_post_startup_inferior (pid_to_ptid (pid));
}

/* Start a new inferior Unix child process.  EXEC_FILE is the file to
   run, ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  If FROM_TTY is non-zero, be
   chatty about it.  */

static void
inf_ptrace_create_inferior (char *exec_file, char *allargs, char **env,
			    int from_tty)
{
  fork_inferior (exec_file, allargs, env, inf_ptrace_me, inf_ptrace_him,
		 NULL, NULL);
}

#ifdef PT_GET_PROCESS_STATE

static void
inf_ptrace_post_startup_inferior (ptid_t pid)
{
  ptrace_event_t pe;

  /* Set the initial event mask.  */
  memset (&pe, 0, sizeof pe);
  pe.pe_set_event |= PTRACE_FORK;
  if (ptrace (PT_SET_EVENT_MASK, ptid_get_pid (pid),
	      (PTRACE_TYPE_ARG3)&pe, sizeof pe) == -1)
    perror_with_name (("ptrace"));
}

#endif

/* Clean up a rotting corpse of an inferior after it died.  */

static void
inf_ptrace_mourn_inferior (void)
{
  int status;

  /* Wait just one more time to collect the inferior's exit status.
     Do not check whether this succeeds though, since we may be
     dealing with a process that we attached to.  Such a process will
     only report its exit status to its original parent.  */
  waitpid (ptid_get_pid (inferior_ptid), &status, 0);

  unpush_target (ptrace_ops_hack);
  generic_mourn_inferior ();
}

/* Attach to the process specified by ARGS.  If FROM_TTY is non-zero,
   be chatty about it.  */

static void
inf_ptrace_attach (char *args, int from_tty)
{
  char *exec_file;
  pid_t pid;
  char *dummy;

  if (!args)
    error_no_arg (_("process-id to attach"));

  dummy = args;
  pid = strtol (args, &dummy, 0);
  /* Some targets don't set errno on errors, grrr!  */
  if (pid == 0 && args == dummy)
    error (_("Illegal process-id: %s."), args);

  if (pid == getpid ())		/* Trying to masturbate?  */
    error (_("I refuse to debug myself!"));

  if (from_tty)
    {
      exec_file = get_exec_file (0);

      if (exec_file)
	printf_unfiltered (_("Attaching to program: %s, %s\n"), exec_file,
			   target_pid_to_str (pid_to_ptid (pid)));
      else
	printf_unfiltered (_("Attaching to %s\n"),
			   target_pid_to_str (pid_to_ptid (pid)));

      gdb_flush (gdb_stdout);
    }

#ifdef PT_ATTACH
  errno = 0;
  ptrace (PT_ATTACH, pid, (PTRACE_TYPE_ARG3)0, 0);
  if (errno != 0)
    perror_with_name (("ptrace"));
  attach_flag = 1;
#else
  error (_("This system does not support attaching to a process"));
#endif

  inferior_ptid = pid_to_ptid (pid);
  push_target (ptrace_ops_hack);
}

#ifdef PT_GET_PROCESS_STATE

void
inf_ptrace_post_attach (int pid)
{
  ptrace_event_t pe;

  /* Set the initial event mask.  */
  memset (&pe, 0, sizeof pe);
  pe.pe_set_event |= PTRACE_FORK;
  if (ptrace (PT_SET_EVENT_MASK, pid,
	      (PTRACE_TYPE_ARG3)&pe, sizeof pe) == -1)
    perror_with_name (("ptrace"));
}

#endif

/* Detach from the inferior, optionally passing it the signal
   specified by ARGS.  If FROM_TTY is non-zero, be chatty about it.  */

static void
inf_ptrace_detach (char *args, int from_tty)
{
  pid_t pid = ptid_get_pid (inferior_ptid);
  int sig = 0;

  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered (_("Detaching from program: %s, %s\n"), exec_file,
			 target_pid_to_str (pid_to_ptid (pid)));
      gdb_flush (gdb_stdout);
    }
  if (args)
    sig = atoi (args);

#ifdef PT_DETACH
  /* We'd better not have left any breakpoints in the program or it'll
     die when it hits one.  Also note that this may only work if we
     previously attached to the inferior.  It *might* work if we
     started the process ourselves.  */
  errno = 0;
  ptrace (PT_DETACH, pid, (PTRACE_TYPE_ARG3)1, sig);
  if (errno != 0)
    perror_with_name (("ptrace"));
  attach_flag = 0;
#else
  error (_("This system does not support detaching from a process"));
#endif

  inferior_ptid = null_ptid;
  unpush_target (ptrace_ops_hack);
}

/* Kill the inferior.  */

static void
inf_ptrace_kill (void)
{
  pid_t pid = ptid_get_pid (inferior_ptid);
  int status;

  if (pid == 0)
    return;

  ptrace (PT_KILL, pid, (PTRACE_TYPE_ARG3)0, 0);
  waitpid (pid, &status, 0);

  target_mourn_inferior ();
}

/* Stop the inferior.  */

static void
inf_ptrace_stop (void)
{
  /* Send a SIGINT to the process group.  This acts just like the user
     typed a ^C on the controlling terminal.  Note that using a
     negative process number in kill() is a System V-ism.  The proper
     BSD interface is killpg().  However, all modern BSDs support the
     System V interface too.  */
  kill (-inferior_process_group, SIGINT);
}

/* Resume execution of thread PTID, or all threads if PTID is -1.  If
   STEP is nonzero, single-step it.  If SIGNAL is nonzero, give it
   that signal.  */

static void
inf_ptrace_resume (ptid_t ptid, int step, enum target_signal signal)
{
  pid_t pid = ptid_get_pid (ptid);
  int request = PT_CONTINUE;

  if (pid == -1)
    /* Resume all threads.  Traditionally ptrace() only supports
       single-threaded processes, so simply resume the inferior.  */
    pid = ptid_get_pid (inferior_ptid);

  if (step)
    {
      /* If this system does not support PT_STEP, a higher level
         function will have called single_step() to transmute the step
         request into a continue request (by setting breakpoints on
         all possible successor instructions), so we don't have to
         worry about that here.  */
      request = PT_STEP;
    }

  /* An address of (PTRACE_TYPE_ARG3)1 tells ptrace to continue from
     where it was.  If GDB wanted it to start some other way, we have
     already written a new program counter value to the child.  */
  errno = 0;
  ptrace (request, pid, (PTRACE_TYPE_ARG3)1, target_signal_to_host (signal));
  if (errno != 0)
    perror_with_name (("ptrace"));
}

/* Wait for the child specified by PTID to do something.  Return the
   process ID of the child, or MINUS_ONE_PTID in case of error; store
   the status in *OURSTATUS.  */

static ptid_t
inf_ptrace_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  pid_t pid;
  int status, save_errno;

  do
    {
      set_sigint_trap ();
      set_sigio_trap ();

      do
	{
	  pid = waitpid (ptid_get_pid (ptid), &status, 0);
	  save_errno = errno;
	}
      while (pid == -1 && errno == EINTR);

      clear_sigio_trap ();
      clear_sigint_trap ();

      if (pid == -1)
	{
	  fprintf_unfiltered (gdb_stderr,
			      _("Child process unexpectedly missing: %s.\n"),
			      safe_strerror (save_errno));

	  /* Claim it exited with unknown signal.  */
	  ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
	  ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
	  return minus_one_ptid;
	}

      /* Ignore terminated detached child processes.  */
      if (!WIFSTOPPED (status) && pid != ptid_get_pid (inferior_ptid))
	pid = -1;
    }
  while (pid == -1);

#ifdef PT_GET_PROCESS_STATE
  if (WIFSTOPPED (status))
    {
      ptrace_state_t pe;
      pid_t fpid;

      if (ptrace (PT_GET_PROCESS_STATE, pid,
		  (PTRACE_TYPE_ARG3)&pe, sizeof pe) == -1)
	perror_with_name (("ptrace"));

      switch (pe.pe_report_event)
	{
	case PTRACE_FORK:
	  ourstatus->kind = TARGET_WAITKIND_FORKED;
	  ourstatus->value.related_pid = pe.pe_other_pid;

	  /* Make sure the other end of the fork is stopped too.  */
	  fpid = waitpid (pe.pe_other_pid, &status, 0);
	  if (fpid == -1)
	    perror_with_name (("waitpid"));

	  if (ptrace (PT_GET_PROCESS_STATE, fpid,
		      (PTRACE_TYPE_ARG3)&pe, sizeof pe) == -1)
	    perror_with_name (("ptrace"));

	  gdb_assert (pe.pe_report_event == PTRACE_FORK);
	  gdb_assert (pe.pe_other_pid == pid);
	  if (fpid == ptid_get_pid (inferior_ptid))
	    {
	      ourstatus->value.related_pid = pe.pe_other_pid;
	      return pid_to_ptid (fpid);
	    }

	  return pid_to_ptid (pid);
	}
    }
#endif

  store_waitstatus (ourstatus, status);
  return pid_to_ptid (pid);
}

/* Attempt a transfer all LEN bytes starting at OFFSET between the
   inferior's OBJECT:ANNEX space and GDB's READBUF/WRITEBUF buffer.
   Return the number of bytes actually transferred.  */

static LONGEST
inf_ptrace_xfer_partial (struct target_ops *ops, enum target_object object,
			 const char *annex, gdb_byte *readbuf,
			 const gdb_byte *writebuf,
			 ULONGEST offset, LONGEST len)
{
  pid_t pid = ptid_get_pid (inferior_ptid);

  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
#ifdef PT_IO
      /* OpenBSD 3.1, NetBSD 1.6 and FreeBSD 5.0 have a new PT_IO
	 request that promises to be much more efficient in reading
	 and writing data in the traced process's address space.  */
      {
	struct ptrace_io_desc piod;

	/* NOTE: We assume that there are no distinct address spaces
	   for instruction and data.  However, on OpenBSD 3.9 and
	   later, PIOD_WRITE_D doesn't allow changing memory that's
	   mapped read-only.  Since most code segments will be
	   read-only, using PIOD_WRITE_D will prevent us from
	   inserting breakpoints, so we use PIOD_WRITE_I instead.  */
	piod.piod_op = writebuf ? PIOD_WRITE_I : PIOD_READ_D;
	piod.piod_addr = writebuf ? (void *) writebuf : readbuf;
	piod.piod_offs = (void *) (long) offset;
	piod.piod_len = len;

	errno = 0;
	if (ptrace (PT_IO, pid, (caddr_t)&piod, 0) == 0)
	  /* Return the actual number of bytes read or written.  */
	  return piod.piod_len;
	/* If the PT_IO request is somehow not supported, fallback on
	   using PT_WRITE_D/PT_READ_D.  Otherwise we will return zero
	   to indicate failure.  */
	if (errno != EINVAL)
	  return 0;
      }
#endif
      {
	union
	{
	  PTRACE_TYPE_RET word;
	  gdb_byte byte[sizeof (PTRACE_TYPE_RET)];
	} buffer;
	ULONGEST rounded_offset;
	LONGEST partial_len;

	/* Round the start offset down to the next long word
	   boundary.  */
	rounded_offset = offset & -(ULONGEST) sizeof (PTRACE_TYPE_RET);

	/* Since ptrace will transfer a single word starting at that
	   rounded_offset the partial_len needs to be adjusted down to
	   that (remember this function only does a single transfer).
	   Should the required length be even less, adjust it down
	   again.  */
	partial_len = (rounded_offset + sizeof (PTRACE_TYPE_RET)) - offset;
	if (partial_len > len)
	  partial_len = len;

	if (writebuf)
	  {
	    /* If OFFSET:PARTIAL_LEN is smaller than
	       ROUNDED_OFFSET:WORDSIZE then a read/modify write will
	       be needed.  Read in the entire word.  */
	    if (rounded_offset < offset
		|| (offset + partial_len
		    < rounded_offset + sizeof (PTRACE_TYPE_RET)))
	      /* Need part of initial word -- fetch it.  */
	      buffer.word = ptrace (PT_READ_I, pid,
				    (PTRACE_TYPE_ARG3)(uintptr_t)
				    rounded_offset, 0);

	    /* Copy data to be written over corresponding part of
	       buffer.  */
	    memcpy (buffer.byte + (offset - rounded_offset),
		    writebuf, partial_len);

	    errno = 0;
	    ptrace (PT_WRITE_D, pid,
		    (PTRACE_TYPE_ARG3)(uintptr_t)rounded_offset,
		    buffer.word);
	    if (errno)
	      {
		/* Using the appropriate one (I or D) is necessary for
		   Gould NP1, at least.  */
		errno = 0;
		ptrace (PT_WRITE_I, pid,
			(PTRACE_TYPE_ARG3)(uintptr_t)rounded_offset,
			buffer.word);
		if (errno)
		  return 0;
	      }
	  }

	if (readbuf)
	  {
	    errno = 0;
	    buffer.word = ptrace (PT_READ_I, pid,
				  (PTRACE_TYPE_ARG3)(uintptr_t)rounded_offset,
				  0);
	    if (errno)
	      return 0;
	    /* Copy appropriate bytes out of the buffer.  */
	    memcpy (readbuf, buffer.byte + (offset - rounded_offset),
		    partial_len);
	  }

	return partial_len;
      }

    case TARGET_OBJECT_UNWIND_TABLE:
      return -1;

    case TARGET_OBJECT_AUXV:
      return -1;

    case TARGET_OBJECT_WCOOKIE:
      return -1;

    default:
      return -1;
    }
}

/* Return non-zero if the thread specified by PTID is alive.  */

static int
inf_ptrace_thread_alive (ptid_t ptid)
{
  /* ??? Is kill the right way to do this?  */
  return (kill (ptid_get_pid (ptid), 0) != -1);
}

/* Print status information about what we're accessing.  */

static void
inf_ptrace_files_info (struct target_ops *ignore)
{
  printf_filtered (_("\tUsing the running image of %s %s.\n"),
		   attach_flag ? "attached" : "child",
		   target_pid_to_str (inferior_ptid));
}

/* Create a prototype ptrace target.  The client can override it with
   local methods.  */

struct target_ops *
inf_ptrace_target (void)
{
  struct target_ops *t = inf_child_target ();

  t->to_attach = inf_ptrace_attach;
  t->to_detach = inf_ptrace_detach;
  t->to_resume = inf_ptrace_resume;
  t->to_wait = inf_ptrace_wait;
  t->to_files_info = inf_ptrace_files_info;
  t->to_kill = inf_ptrace_kill;
  t->to_create_inferior = inf_ptrace_create_inferior;
#ifdef PT_GET_PROCESS_STATE
  t->to_follow_fork = inf_ptrace_follow_fork;
  t->to_post_startup_inferior = inf_ptrace_post_startup_inferior;
  t->to_post_attach = inf_ptrace_post_attach;
#endif
  t->to_mourn_inferior = inf_ptrace_mourn_inferior;
  t->to_thread_alive = inf_ptrace_thread_alive;
  t->to_pid_to_str = normal_pid_to_str;
  t->to_stop = inf_ptrace_stop;
  t->to_xfer_partial = inf_ptrace_xfer_partial;

  ptrace_ops_hack = t;
  return t;
}


/* Pointer to a function that returns the offset within the user area
   where a particular register is stored.  */
static CORE_ADDR (*inf_ptrace_register_u_offset)(struct gdbarch *, int, int);

/* Fetch register REGNUM from the inferior.  */

static void
inf_ptrace_fetch_register (struct regcache *regcache, int regnum)
{
  CORE_ADDR addr;
  size_t size;
  PTRACE_TYPE_RET *buf;
  int pid, i;

  /* This isn't really an address, but ptrace thinks of it as one.  */
  addr = inf_ptrace_register_u_offset (current_gdbarch, regnum, 0);
  if (addr == (CORE_ADDR)-1
      || gdbarch_cannot_fetch_register (current_gdbarch, regnum))
    {
      regcache_raw_supply (regcache, regnum, NULL);
      return;
    }

  /* Cater for systems like GNU/Linux, that implement threads as
     separate processes.  */
  pid = ptid_get_lwp (inferior_ptid);
  if (pid == 0)
    pid = ptid_get_pid (inferior_ptid);

  size = register_size (current_gdbarch, regnum);
  gdb_assert ((size % sizeof (PTRACE_TYPE_RET)) == 0);
  buf = alloca (size);

  /* Read the register contents from the inferior a chunk at a time.  */
  for (i = 0; i < size / sizeof (PTRACE_TYPE_RET); i++)
    {
      errno = 0;
      buf[i] = ptrace (PT_READ_U, pid, (PTRACE_TYPE_ARG3)(uintptr_t)addr, 0);
      if (errno != 0)
	error (_("Couldn't read register %s (#%d): %s."),
	       gdbarch_register_name (current_gdbarch, regnum),
	       regnum, safe_strerror (errno));

      addr += sizeof (PTRACE_TYPE_RET);
    }
  regcache_raw_supply (regcache, regnum, buf);
}

/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
inf_ptrace_fetch_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1)
    for (regnum = 0; regnum < gdbarch_num_regs (current_gdbarch); regnum++)
      inf_ptrace_fetch_register (regcache, regnum);
  else
    inf_ptrace_fetch_register (regcache, regnum);
}

/* Store register REGNUM into the inferior.  */

static void
inf_ptrace_store_register (const struct regcache *regcache, int regnum)
{
  CORE_ADDR addr;
  size_t size;
  PTRACE_TYPE_RET *buf;
  int pid, i;

  /* This isn't really an address, but ptrace thinks of it as one.  */
  addr = inf_ptrace_register_u_offset (current_gdbarch, regnum, 1);
  if (addr == (CORE_ADDR)-1 
      || gdbarch_cannot_store_register (current_gdbarch, regnum))
    return;

  /* Cater for systems like GNU/Linux, that implement threads as
     separate processes.  */
  pid = ptid_get_lwp (inferior_ptid);
  if (pid == 0)
    pid = ptid_get_pid (inferior_ptid);

  size = register_size (current_gdbarch, regnum);
  gdb_assert ((size % sizeof (PTRACE_TYPE_RET)) == 0);
  buf = alloca (size);

  /* Write the register contents into the inferior a chunk at a time.  */
  regcache_raw_collect (regcache, regnum, buf);
  for (i = 0; i < size / sizeof (PTRACE_TYPE_RET); i++)
    {
      errno = 0;
      ptrace (PT_WRITE_U, pid, (PTRACE_TYPE_ARG3)(uintptr_t)addr, buf[i]);
      if (errno != 0)
	error (_("Couldn't write register %s (#%d): %s."),
	       gdbarch_register_name (current_gdbarch, regnum),
	       regnum, safe_strerror (errno));

      addr += sizeof (PTRACE_TYPE_RET);
    }
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

void
inf_ptrace_store_registers (struct regcache *regcache, int regnum)
{
  if (regnum == -1)
    for (regnum = 0; regnum < gdbarch_num_regs (current_gdbarch); regnum++)
      inf_ptrace_store_register (regcache, regnum);
  else
    inf_ptrace_store_register (regcache, regnum);
}

/* Create a "traditional" ptrace target.  REGISTER_U_OFFSET should be
   a function returning the offset within the user area where a
   particular register is stored.  */

struct target_ops *
inf_ptrace_trad_target (CORE_ADDR (*register_u_offset)
					(struct gdbarch *, int, int))
{
  struct target_ops *t = inf_ptrace_target();

  gdb_assert (register_u_offset);
  inf_ptrace_register_u_offset = register_u_offset;
  t->to_fetch_registers = inf_ptrace_fetch_registers;
  t->to_store_registers = inf_ptrace_store_registers;

  return t;
}
