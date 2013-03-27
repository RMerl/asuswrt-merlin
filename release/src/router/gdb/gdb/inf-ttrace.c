/* Low-level child interface to ttrace.

   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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

/* The ttrace(2) system call didn't exist before HP-UX 10.30.  Don't
   try to compile this code unless we have it.  */
#ifdef HAVE_TTRACE

#include "command.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "inferior.h"
#include "target.h"

#include "gdb_assert.h"
#include "gdb_string.h"
#include <sys/mman.h>
#include <sys/ttrace.h>

#include "inf-child.h"
#include "inf-ttrace.h"

/* HACK: Save the ttrace ops returned by inf_ttrace_target.  */
static struct target_ops *ttrace_ops_hack;


/* HP-UX uses a threading model where each user-space thread
   corresponds to a kernel thread.  These kernel threads are called
   lwps.  The ttrace(2) interface gives us almost full control over
   the threads, which makes it very easy to support them in GDB.  We
   identify the threads by process ID and lwp ID.  The ttrace(2) also
   provides us with a thread's user ID (in the `tts_user_tid' member
   of `ttstate_t') but we don't use that (yet) as it isn't necessary
   to uniquely label the thread.  */

/* Number of active lwps.  */
static int inf_ttrace_num_lwps;


/* On HP-UX versions that have the ttrace(2) system call, we can
   implement "hardware" watchpoints by fiddling with the protection of
   pages in the address space that contain the variable being watched.
   In order to implement this, we keep a dictionary of pages for which
   we have changed the protection.  */

struct inf_ttrace_page
{
  CORE_ADDR addr;		/* Page address.  */
  int prot;			/* Protection.  */
  int refcount;			/* Reference count.  */
  struct inf_ttrace_page *next;
  struct inf_ttrace_page *prev;
};

struct inf_ttrace_page_dict
{
  struct inf_ttrace_page buckets[128];
  int pagesize;			/* Page size.  */
  int count;			/* Number of pages in this dictionary.  */
} inf_ttrace_page_dict;

/* Number of lwps that are currently in a system call.  */
static int inf_ttrace_num_lwps_in_syscall;

/* Flag to indicate whether we should re-enable page protections after
   the next wait.  */
static int inf_ttrace_reenable_page_protections;

/* Enable system call events for process PID.  */

static void
inf_ttrace_enable_syscall_events (pid_t pid)
{
  ttevent_t tte;
  ttstate_t tts;

  gdb_assert (inf_ttrace_num_lwps_in_syscall == 0);

  if (ttrace (TT_PROC_GET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  tte.tte_events |= (TTEVT_SYSCALL_ENTRY | TTEVT_SYSCALL_RETURN);

  if (ttrace (TT_PROC_SET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  if (ttrace (TT_PROC_GET_FIRST_LWP_STATE, pid, 0,
	      (uintptr_t)&tts, sizeof tts, 0) == -1)
    perror_with_name (("ttrace"));

  if (tts.tts_flags & TTS_INSYSCALL)
    inf_ttrace_num_lwps_in_syscall++;

  /* FIXME: Handle multiple threads.  */
}

/* Disable system call events for process PID.  */

static void
inf_ttrace_disable_syscall_events (pid_t pid)
{
  ttevent_t tte;

  gdb_assert (inf_ttrace_page_dict.count == 0);

  if (ttrace (TT_PROC_GET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  tte.tte_events &= ~(TTEVT_SYSCALL_ENTRY | TTEVT_SYSCALL_RETURN);

  if (ttrace (TT_PROC_SET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  inf_ttrace_num_lwps_in_syscall = 0;
}

/* Get information about the page at address ADDR for process PID from
   the dictionary.  */

static struct inf_ttrace_page *
inf_ttrace_get_page (pid_t pid, CORE_ADDR addr)
{
  const int num_buckets = ARRAY_SIZE (inf_ttrace_page_dict.buckets);
  const int pagesize = inf_ttrace_page_dict.pagesize;
  int bucket;
  struct inf_ttrace_page *page;

  bucket = (addr / pagesize) % num_buckets;
  page = &inf_ttrace_page_dict.buckets[bucket];
  while (page)
    {
      if (page->addr == addr)
	break;

      page = page->next;
    }

  return page;
}

/* Add the page at address ADDR for process PID to the dictionary.  */

static struct inf_ttrace_page *
inf_ttrace_add_page (pid_t pid, CORE_ADDR addr)
{
  const int num_buckets = ARRAY_SIZE (inf_ttrace_page_dict.buckets);
  const int pagesize = inf_ttrace_page_dict.pagesize;
  int bucket;
  struct inf_ttrace_page *page;
  struct inf_ttrace_page *prev = NULL;

  bucket = (addr / pagesize) % num_buckets;
  page = &inf_ttrace_page_dict.buckets[bucket];
  while (page)
    {
      if (page->addr == addr)
	break;

      prev = page;
      page = page->next;
    }
  
  if (!page)
    {
      int prot;

      if (ttrace (TT_PROC_GET_MPROTECT, pid, 0,
		  addr, 0, (uintptr_t)&prot) == -1)
	perror_with_name (("ttrace"));
      
      page = XMALLOC (struct inf_ttrace_page);
      page->addr = addr;
      page->prot = prot;
      page->refcount = 0;
      page->next = NULL;

      page->prev = prev;
      prev->next = page;

      inf_ttrace_page_dict.count++;
      if (inf_ttrace_page_dict.count == 1)
	inf_ttrace_enable_syscall_events (pid);

      if (inf_ttrace_num_lwps_in_syscall == 0)
	{
	  if (ttrace (TT_PROC_SET_MPROTECT, pid, 0,
		      addr, pagesize, prot & ~PROT_WRITE) == -1)
	    perror_with_name (("ttrace"));
	}
    }

  return page;
}

/* Insert the page at address ADDR of process PID to the dictionary.  */

static void
inf_ttrace_insert_page (pid_t pid, CORE_ADDR addr)
{
  struct inf_ttrace_page *page;

  page = inf_ttrace_get_page (pid, addr);
  if (!page)
    page = inf_ttrace_add_page (pid, addr);

  page->refcount++;
}

/* Remove the page at address ADDR of process PID from the dictionary.  */

static void
inf_ttrace_remove_page (pid_t pid, CORE_ADDR addr)
{
  const int pagesize = inf_ttrace_page_dict.pagesize;
  struct inf_ttrace_page *page;

  page = inf_ttrace_get_page (pid, addr);
  page->refcount--;

  gdb_assert (page->refcount >= 0);

  if (page->refcount == 0)
    {
      if (inf_ttrace_num_lwps_in_syscall == 0)
	{
	  if (ttrace (TT_PROC_SET_MPROTECT, pid, 0,
		      addr, pagesize, page->prot) == -1)
	    perror_with_name (("ttrace"));
	}

      inf_ttrace_page_dict.count--;
      if (inf_ttrace_page_dict.count == 0)
	inf_ttrace_disable_syscall_events (pid);

      page->prev->next = page->next;
      if (page->next)
	page->next->prev = page->prev;

      xfree (page);
    }
}

/* Mask the bits in PROT from the page protections that are currently
   in the dictionary for process PID.  */

static void
inf_ttrace_mask_page_protections (pid_t pid, int prot)
{
  const int num_buckets = ARRAY_SIZE (inf_ttrace_page_dict.buckets);
  const int pagesize = inf_ttrace_page_dict.pagesize;
  int bucket;

  for (bucket = 0; bucket < num_buckets; bucket++)
    {
      struct inf_ttrace_page *page;

      page = inf_ttrace_page_dict.buckets[bucket].next;
      while (page)
	{
	  if (ttrace (TT_PROC_SET_MPROTECT, pid, 0,
		      page->addr, pagesize, page->prot & ~prot) == -1)
	    perror_with_name (("ttrace"));

	  page = page->next;
	}
    }
}

/* Write-protect the pages in the dictionary for process PID.  */

static void
inf_ttrace_enable_page_protections (pid_t pid)
{
  inf_ttrace_mask_page_protections (pid, PROT_WRITE);
}

/* Restore the protection of the pages in the dictionary for process
   PID.  */

static void
inf_ttrace_disable_page_protections (pid_t pid)
{
  inf_ttrace_mask_page_protections (pid, 0);
}

/* Insert a "hardware" watchpoint for LEN bytes at address ADDR of
   type TYPE.  */

static int
inf_ttrace_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  const int pagesize = inf_ttrace_page_dict.pagesize;
  pid_t pid = ptid_get_pid (inferior_ptid);
  CORE_ADDR page_addr;
  int num_pages;
  int page;

  gdb_assert (type == hw_write);

  page_addr = (addr / pagesize) * pagesize;
  num_pages = (len + pagesize - 1) / pagesize;

  for (page = 0; page < num_pages; page++, page_addr += pagesize)
    inf_ttrace_insert_page (pid, page_addr);

  return 1;
}

/* Remove a "hardware" watchpoint for LEN bytes at address ADDR of
   type TYPE.  */

static int
inf_ttrace_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  const int pagesize = inf_ttrace_page_dict.pagesize;
  pid_t pid = ptid_get_pid (inferior_ptid);
  CORE_ADDR page_addr;
  int num_pages;
  int page;

  gdb_assert (type == hw_write);

  page_addr = (addr / pagesize) * pagesize;
  num_pages = (len + pagesize - 1) / pagesize;

  for (page = 0; page < num_pages; page++, page_addr += pagesize)
    inf_ttrace_remove_page (pid, page_addr);

  return 1;
}

static int
inf_ttrace_can_use_hw_breakpoint (int type, int len, int ot)
{
  return (type == bp_hardware_watchpoint);
}

static int
inf_ttrace_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len)
{
  return 1;
}

/* Return non-zero if the current inferior was (potentially) stopped
   by hitting a "hardware" watchpoint.  */

static int
inf_ttrace_stopped_by_watchpoint (void)
{
  pid_t pid = ptid_get_pid (inferior_ptid);
  lwpid_t lwpid = ptid_get_lwp (inferior_ptid);
  ttstate_t tts;

  if (inf_ttrace_page_dict.count > 0)
    {
      if (ttrace (TT_LWP_GET_STATE, pid, lwpid,
		  (uintptr_t)&tts, sizeof tts, 0) == -1)
	perror_with_name (("ttrace"));

      if (tts.tts_event == TTEVT_SIGNAL
	  && tts.tts_u.tts_signal.tts_signo == SIGBUS)
	{
	  const int pagesize = inf_ttrace_page_dict.pagesize;
	  void *addr = tts.tts_u.tts_signal.tts_siginfo.si_addr;
	  CORE_ADDR page_addr = ((uintptr_t)addr / pagesize) * pagesize;

	  if (inf_ttrace_get_page (pid, page_addr))
	    return 1;
	}
    }

  return 0;
}


/* When tracking a vfork(2), we cannot detach from the parent until
   after the child has called exec(3) or has exited.  If we are still
   attached to the parent, this variable will be set to the process ID
   of the parent.  Otherwise it will be set to zero.  */
static pid_t inf_ttrace_vfork_ppid = -1;

static int
inf_ttrace_follow_fork (struct target_ops *ops, int follow_child)
{
  pid_t pid, fpid;
  lwpid_t lwpid, flwpid;
  ttstate_t tts;

  /* FIXME: kettenis/20050720: This stuff should really be passed as
     an argument by our caller.  */
  {
    ptid_t ptid;
    struct target_waitstatus status;

    get_last_target_status (&ptid, &status);
    gdb_assert (status.kind == TARGET_WAITKIND_FORKED
		|| status.kind == TARGET_WAITKIND_VFORKED);

    pid = ptid_get_pid (ptid);
    lwpid = ptid_get_lwp (ptid);
  }

  /* Get all important details that core GDB doesn't (and shouldn't)
     know about.  */
  if (ttrace (TT_LWP_GET_STATE, pid, lwpid,
	      (uintptr_t)&tts, sizeof tts, 0) == -1)
    perror_with_name (("ttrace"));

  gdb_assert (tts.tts_event == TTEVT_FORK || tts.tts_event == TTEVT_VFORK);

  if (tts.tts_u.tts_fork.tts_isparent)
    {
      pid = tts.tts_pid;
      lwpid = tts.tts_lwpid;
      fpid = tts.tts_u.tts_fork.tts_fpid;
      flwpid = tts.tts_u.tts_fork.tts_flwpid;
    }
  else
    {
      pid = tts.tts_u.tts_fork.tts_fpid;
      lwpid = tts.tts_u.tts_fork.tts_flwpid;
      fpid = tts.tts_pid;
      flwpid = tts.tts_lwpid;
    }

  if (follow_child)
    {
      inferior_ptid = ptid_build (fpid, flwpid, 0);
      detach_breakpoints (pid);

      target_terminal_ours ();
      fprintf_unfiltered (gdb_stdlog, _("\
Attaching after fork to child process %ld.\n"), (long)fpid);
    }
  else
    {
      inferior_ptid = ptid_build (pid, lwpid, 0);
      detach_breakpoints (fpid);

      target_terminal_ours ();
      fprintf_unfiltered (gdb_stdlog, _("\
Detaching after fork from child process %ld.\n"), (long)fpid);
    }

  if (tts.tts_event == TTEVT_VFORK)
    {
      gdb_assert (!tts.tts_u.tts_fork.tts_isparent);

      if (follow_child)
	{
	  /* We can't detach from the parent yet.  */
	  inf_ttrace_vfork_ppid = pid;

	  reattach_breakpoints (fpid);
	}
      else
	{
	  if (ttrace (TT_PROC_DETACH, fpid, 0, 0, 0, 0) == -1)
	    perror_with_name (("ttrace"));

	  /* Wait till we get the TTEVT_VFORK event in the parent.
	     This indicates that the child has called exec(3) or has
	     exited and that the parent is ready to be traced again.  */
	  if (ttrace_wait (pid, lwpid, TTRACE_WAITOK, &tts, sizeof tts) == -1)
	    perror_with_name (("ttrace_wait"));
	  gdb_assert (tts.tts_event == TTEVT_VFORK);
	  gdb_assert (tts.tts_u.tts_fork.tts_isparent);

	  reattach_breakpoints (pid);
	}
    }
  else
    {
      gdb_assert (tts.tts_u.tts_fork.tts_isparent);

      if (follow_child)
	{
	  if (ttrace (TT_PROC_DETACH, pid, 0, 0, 0, 0) == -1)
	    perror_with_name (("ttrace"));
	}
      else
	{
	  if (ttrace (TT_PROC_DETACH, fpid, 0, 0, 0, 0) == -1)
	    perror_with_name (("ttrace"));
	}
    }

  if (follow_child)
    {
      /* The child will start out single-threaded.  */
      inf_ttrace_num_lwps = 0;
      inf_ttrace_num_lwps_in_syscall = 0;

      /* Reset breakpoints in the child as appropriate.  */
      follow_inferior_reset_breakpoints ();
    }

  return 0;
}


/* File descriptors for pipes used as semaphores during initial
   startup of an inferior.  */
static int inf_ttrace_pfd1[2];
static int inf_ttrace_pfd2[2];

static void
do_cleanup_pfds (void *dummy)
{
  close (inf_ttrace_pfd1[0]);
  close (inf_ttrace_pfd1[1]);
  close (inf_ttrace_pfd2[0]);
  close (inf_ttrace_pfd2[1]);
}

static void
inf_ttrace_prepare (void)
{
  if (pipe (inf_ttrace_pfd1) == -1)
    perror_with_name (("pipe"));

  if (pipe (inf_ttrace_pfd2) == -1)
    {
      close (inf_ttrace_pfd1[0]);
      close (inf_ttrace_pfd2[0]);
      perror_with_name (("pipe"));
    }
}

/* Prepare to be traced.  */

static void
inf_ttrace_me (void)
{
  struct cleanup *old_chain = make_cleanup (do_cleanup_pfds, 0);
  char c;

  /* "Trace me, Dr. Memory!"  */
  if (ttrace (TT_PROC_SETTRC, 0, 0, 0, TT_VERSION, 0) == -1)
    perror_with_name (("ttrace"));

  /* Tell our parent that we are ready to be traced.  */
  if (write (inf_ttrace_pfd1[1], &c, sizeof c) != sizeof c)
    perror_with_name (("write"));

  /* Wait until our parent has set the initial event mask.  */
  if (read (inf_ttrace_pfd2[0], &c, sizeof c) != sizeof c)
    perror_with_name (("read"));

  do_cleanups (old_chain);
}

/* Start tracing PID.  */

static void
inf_ttrace_him (int pid)
{
  struct cleanup *old_chain = make_cleanup (do_cleanup_pfds, 0);
  ttevent_t tte;
  char c;

  /* Wait until our child is ready to be traced.  */
  if (read (inf_ttrace_pfd1[0], &c, sizeof c) != sizeof c)
    perror_with_name (("read"));

  /* Set the initial event mask.  */
  memset (&tte, 0, sizeof (tte));
  tte.tte_events |= TTEVT_EXEC | TTEVT_EXIT | TTEVT_FORK | TTEVT_VFORK;
  tte.tte_events |= TTEVT_LWP_CREATE | TTEVT_LWP_EXIT | TTEVT_LWP_TERMINATE;
#ifdef TTEVT_BPT_SSTEP
  tte.tte_events |= TTEVT_BPT_SSTEP;
#endif
  tte.tte_opts |= TTEO_PROC_INHERIT;
  if (ttrace (TT_PROC_SET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  /* Tell our child that we have set the initial event mask.  */
  if (write (inf_ttrace_pfd2[1], &c, sizeof c) != sizeof c)
    perror_with_name (("write"));

  do_cleanups (old_chain);

  push_target (ttrace_ops_hack);

  /* On some targets, there must be some explicit synchronization
     between the parent and child processes after the debugger forks,
     and before the child execs the debuggee program.  This call
     basically gives permission for the child to exec.  */

  target_acknowledge_created_inferior (pid);

  /* START_INFERIOR_TRAPS_EXPECTED is defined in inferior.h, and will
     be 1 or 2 depending on whether we're starting without or with a
     shell.  */
  startup_inferior (START_INFERIOR_TRAPS_EXPECTED);

  /* On some targets, there must be some explicit actions taken after
     the inferior has been started up.  */
  target_post_startup_inferior (pid_to_ptid (pid));
}

static void
inf_ttrace_create_inferior (char *exec_file, char *allargs, char **env,
			    int from_tty)
{
  gdb_assert (inf_ttrace_num_lwps == 0);
  gdb_assert (inf_ttrace_num_lwps_in_syscall == 0);
  gdb_assert (inf_ttrace_page_dict.count == 0);
  gdb_assert (inf_ttrace_reenable_page_protections == 0);
  gdb_assert (inf_ttrace_vfork_ppid == -1);

  fork_inferior (exec_file, allargs, env, inf_ttrace_me, inf_ttrace_him,
		 inf_ttrace_prepare, NULL);
}

static void
inf_ttrace_mourn_inferior (void)
{
  const int num_buckets = ARRAY_SIZE (inf_ttrace_page_dict.buckets);
  int bucket;

  inf_ttrace_num_lwps = 0;
  inf_ttrace_num_lwps_in_syscall = 0;

  for (bucket = 0; bucket < num_buckets; bucket++)
    {
      struct inf_ttrace_page *page;
      struct inf_ttrace_page *next;

      page = inf_ttrace_page_dict.buckets[bucket].next;
      while (page)
	{
	  next = page->next;
	  xfree (page);
	  page = next;
	}
    }
  inf_ttrace_page_dict.count = 0;

  unpush_target (ttrace_ops_hack);
  generic_mourn_inferior ();
}

static void
inf_ttrace_attach (char *args, int from_tty)
{
  char *exec_file;
  pid_t pid;
  char *dummy;
  ttevent_t tte;

  if (!args)
    error_no_arg (_("process-id to attach"));

  dummy = args;
  pid = strtol (args, &dummy, 0);
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

  gdb_assert (inf_ttrace_num_lwps == 0);
  gdb_assert (inf_ttrace_num_lwps_in_syscall == 0);
  gdb_assert (inf_ttrace_vfork_ppid == -1);

  if (ttrace (TT_PROC_ATTACH, pid, 0, TT_KILL_ON_EXIT, TT_VERSION, 0) == -1)
    perror_with_name (("ttrace"));
  attach_flag = 1;

  /* Set the initial event mask.  */
  memset (&tte, 0, sizeof (tte));
  tte.tte_events |= TTEVT_EXEC | TTEVT_EXIT | TTEVT_FORK | TTEVT_VFORK;
  tte.tte_events |= TTEVT_LWP_CREATE | TTEVT_LWP_EXIT | TTEVT_LWP_TERMINATE;
#ifdef TTEVT_BPT_SSTEP
  tte.tte_events |= TTEVT_BPT_SSTEP;
#endif
  tte.tte_opts |= TTEO_PROC_INHERIT;
  if (ttrace (TT_PROC_SET_EVENT_MASK, pid, 0,
	      (uintptr_t)&tte, sizeof tte, 0) == -1)
    perror_with_name (("ttrace"));

  inferior_ptid = pid_to_ptid (pid);
  push_target (ttrace_ops_hack);
}

static void
inf_ttrace_detach (char *args, int from_tty)
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

  /* ??? The HP-UX 11.0 ttrace(2) manual page doesn't mention that we
     can pass a signal number here.  Does this really work?  */
  if (ttrace (TT_PROC_DETACH, pid, 0, 0, sig, 0) == -1)
    perror_with_name (("ttrace"));

  if (inf_ttrace_vfork_ppid != -1)
    {
      if (ttrace (TT_PROC_DETACH, inf_ttrace_vfork_ppid, 0, 0, 0, 0) == -1)
	perror_with_name (("ttrace"));
      inf_ttrace_vfork_ppid = -1;
    }

  inf_ttrace_num_lwps = 0;
  inf_ttrace_num_lwps_in_syscall = 0;

  unpush_target (ttrace_ops_hack);
  inferior_ptid = null_ptid;
}

static void
inf_ttrace_kill (void)
{
  pid_t pid = ptid_get_pid (inferior_ptid);

  if (pid == 0)
    return;

  if (ttrace (TT_PROC_EXIT, pid, 0, 0, 0, 0) == -1)
    perror_with_name (("ttrace"));
  /* ??? Is it necessary to call ttrace_wait() here?  */

  if (inf_ttrace_vfork_ppid != -1)
    {
      if (ttrace (TT_PROC_DETACH, inf_ttrace_vfork_ppid, 0, 0, 0, 0) == -1)
	perror_with_name (("ttrace"));
      inf_ttrace_vfork_ppid = -1;
    }

  target_mourn_inferior ();
}

static int
inf_ttrace_resume_callback (struct thread_info *info, void *arg)
{
  if (!ptid_equal (info->ptid, inferior_ptid))
    {
      pid_t pid = ptid_get_pid (info->ptid);
      lwpid_t lwpid = ptid_get_lwp (info->ptid);

      if (ttrace (TT_LWP_CONTINUE, pid, lwpid, TT_NOPC, 0, 0) == -1)
	perror_with_name (("ttrace"));
    }

  return 0;
}

static void
inf_ttrace_resume (ptid_t ptid, int step, enum target_signal signal)
{
  pid_t pid = ptid_get_pid (ptid);
  lwpid_t lwpid = ptid_get_lwp (ptid);
  ttreq_t request = step ? TT_LWP_SINGLE : TT_LWP_CONTINUE;
  int sig = target_signal_to_host (signal);

  if (pid == -1)
    {
      pid = ptid_get_pid (inferior_ptid);
      lwpid = ptid_get_lwp (inferior_ptid);
    }

  if (ttrace (request, pid, lwpid, TT_NOPC, sig, 0) == -1)
    perror_with_name (("ttrace"));

  if (ptid_equal (ptid, minus_one_ptid) && inf_ttrace_num_lwps > 0)
    {
      /* Let all the other threads run too.  */
      iterate_over_threads (inf_ttrace_resume_callback, NULL);
    }
}

static ptid_t
inf_ttrace_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  pid_t pid = ptid_get_pid (ptid);
  lwpid_t lwpid = ptid_get_lwp (ptid);
  ttstate_t tts;

  /* Until proven otherwise.  */
  ourstatus->kind = TARGET_WAITKIND_SPURIOUS;

  if (pid == -1)
    pid = lwpid = 0;

  gdb_assert (pid != 0 || lwpid == 0);

  do
    {
      set_sigint_trap ();
      set_sigio_trap ();

      if (ttrace_wait (pid, lwpid, TTRACE_WAITOK, &tts, sizeof tts) == -1)
	perror_with_name (("ttrace_wait"));

      if (tts.tts_event == TTEVT_VFORK && tts.tts_u.tts_fork.tts_isparent)
	{
	  if (inf_ttrace_vfork_ppid != -1)
	    {
	      gdb_assert (inf_ttrace_vfork_ppid == tts.tts_pid);

	      if (ttrace (TT_PROC_DETACH, tts.tts_pid, 0, 0, 0, 0) == -1)
		perror_with_name (("ttrace"));
	      inf_ttrace_vfork_ppid = -1;
	    }

	  tts.tts_event = TTEVT_NONE;
	}

      clear_sigio_trap ();
      clear_sigint_trap ();
    }
  while (tts.tts_event == TTEVT_NONE);

  /* Now that we've waited, we can re-enable the page protections.  */
  if (inf_ttrace_reenable_page_protections)
    {
      gdb_assert (inf_ttrace_num_lwps_in_syscall == 0);
      inf_ttrace_enable_page_protections (tts.tts_pid);
      inf_ttrace_reenable_page_protections = 0;
    }

  ptid = ptid_build (tts.tts_pid, tts.tts_lwpid, 0);

  switch (tts.tts_event)
    {
#ifdef TTEVT_BPT_SSTEP
    case TTEVT_BPT_SSTEP:
      /* Make it look like a breakpoint.  */
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
#endif

    case TTEVT_EXEC:
      /* FIXME: kettenis/20051029: GDB doesn't really know how to deal
	 with TARGET_WAITKIND_EXECD events yet.  So we make it look
	 like a SIGTRAP instead.  */
#if 0
      ourstatus->kind = TARGET_WAITKIND_EXECD;
      ourstatus->value.execd_pathname =
	xmalloc (tts.tts_u.tts_exec.tts_pathlen + 1);
      if (ttrace (TT_PROC_GET_PATHNAME, tts.tts_pid, 0,
		  (uintptr_t)ourstatus->value.execd_pathname,
		  tts.tts_u.tts_exec.tts_pathlen, 0) == -1)
	perror_with_name (("ttrace"));
      ourstatus->value.execd_pathname[tts.tts_u.tts_exec.tts_pathlen] = 0;
#else
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
#endif
      break;

    case TTEVT_EXIT:
      store_waitstatus (ourstatus, tts.tts_u.tts_exit.tts_exitcode);
      inf_ttrace_num_lwps = 0;
      break;

    case TTEVT_FORK:
      ourstatus->kind = TARGET_WAITKIND_FORKED;
      ourstatus->value.related_pid = tts.tts_u.tts_fork.tts_fpid;

      /* Make sure the other end of the fork is stopped too.  */
      if (ttrace_wait (tts.tts_u.tts_fork.tts_fpid,
		       tts.tts_u.tts_fork.tts_flwpid,
		       TTRACE_WAITOK, &tts, sizeof tts) == -1)
	perror_with_name (("ttrace_wait"));

      gdb_assert (tts.tts_event == TTEVT_FORK);
      if (tts.tts_u.tts_fork.tts_isparent)
	{
	  ptid = ptid_build (tts.tts_pid, tts.tts_lwpid, 0);
	  ourstatus->value.related_pid = tts.tts_u.tts_fork.tts_fpid;
	}
      break;

    case TTEVT_VFORK:
      gdb_assert (!tts.tts_u.tts_fork.tts_isparent);

      ourstatus->kind = TARGET_WAITKIND_VFORKED;
      ourstatus->value.related_pid = tts.tts_u.tts_fork.tts_fpid;

      /* HACK: To avoid touching the parent during the vfork, switch
	 away from it.  */
      inferior_ptid = ptid;
      break;

    case TTEVT_LWP_CREATE:
      lwpid = tts.tts_u.tts_thread.tts_target_lwpid;
      ptid = ptid_build (tts.tts_pid, lwpid, 0);
      if (inf_ttrace_num_lwps == 0)
	{
	  /* Now that we're going to be multi-threaded, add the
	     original thread to the list first.  */
	  add_thread (ptid_build (tts.tts_pid, tts.tts_lwpid, 0));
	  inf_ttrace_num_lwps++;
	}
      printf_filtered (_("[New %s]\n"), target_pid_to_str (ptid));
      add_thread (ptid);
      inf_ttrace_num_lwps++;
      ptid = ptid_build (tts.tts_pid, tts.tts_lwpid, 0);
      break;

    case TTEVT_LWP_EXIT:
      printf_filtered(_("[%s exited]\n"), target_pid_to_str (ptid));
      delete_thread (ptid);
      inf_ttrace_num_lwps--;
      /* If we don't return -1 here, core GDB will re-add the thread.  */
      ptid = minus_one_ptid;
      break;

    case TTEVT_LWP_TERMINATE:
      lwpid = tts.tts_u.tts_thread.tts_target_lwpid;
      ptid = ptid_build (tts.tts_pid, lwpid, 0);
      printf_filtered(_("[%s has been terminated]\n"), target_pid_to_str (ptid));
      delete_thread (ptid);
      inf_ttrace_num_lwps--;
      ptid = ptid_build (tts.tts_pid, tts.tts_lwpid, 0);
      break;

    case TTEVT_SIGNAL:
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig =
	target_signal_from_host (tts.tts_u.tts_signal.tts_signo);
      break;

    case TTEVT_SYSCALL_ENTRY:
      gdb_assert (inf_ttrace_reenable_page_protections == 0);
      inf_ttrace_num_lwps_in_syscall++;
      if (inf_ttrace_num_lwps_in_syscall == 1)
	{
	  /* A thread has just entered a system call.  Disable any
             page protections as the kernel can't deal with them.  */
	  inf_ttrace_disable_page_protections (tts.tts_pid);
	}
      ourstatus->kind = TARGET_WAITKIND_SYSCALL_ENTRY;
      ourstatus->value.syscall_id = tts.tts_scno;
      break;

    case TTEVT_SYSCALL_RETURN:
      if (inf_ttrace_num_lwps_in_syscall > 0)
	{
	  /* If the last thread has just left the system call, this
	     would be a logical place to re-enable the page
	     protections, but that doesn't work.  We can't re-enable
	     them until we've done another wait.  */
	  inf_ttrace_reenable_page_protections = 
	    (inf_ttrace_num_lwps_in_syscall == 1);
	  inf_ttrace_num_lwps_in_syscall--;
	}
      ourstatus->kind = TARGET_WAITKIND_SYSCALL_RETURN;
      ourstatus->value.syscall_id = tts.tts_scno;
      break;

    default:
      gdb_assert (!"Unexpected ttrace event");
      break;
    }

  /* Make sure all threads within the process are stopped.  */
  if (ttrace (TT_PROC_STOP, tts.tts_pid, 0, 0, 0, 0) == -1)
    perror_with_name (("ttrace"));

  /* HACK: Twiddle INFERIOR_PTID such that the initial thread of a
     process isn't recognized as a new thread.  */
  if (ptid_get_lwp (inferior_ptid) == 0)
    inferior_ptid = ptid;

  return ptid;
}

/* Transfer LEN bytes from ADDR in the inferior's memory into READBUF,
   and transfer LEN bytes from WRITEBUF into the inferior's memory at
   ADDR.  Either READBUF or WRITEBUF may be null, in which case the
   corresponding transfer doesn't happen.  Return the number of bytes
   actually transferred (which may be zero if an error occurs).  */

static LONGEST
inf_ttrace_xfer_memory (CORE_ADDR addr, ULONGEST len,
			void *readbuf, const void *writebuf)
{
  pid_t pid = ptid_get_pid (inferior_ptid);

  /* HP-UX treats text space and data space differently.  GDB however,
     doesn't really know the difference.  Therefore we try both.  Try
     text space before data space though because when we're writing
     into text space the instruction cache might need to be flushed.  */

  if (readbuf
      && ttrace (TT_PROC_RDTEXT, pid, 0, addr, len, (uintptr_t)readbuf) == -1
      && ttrace (TT_PROC_RDDATA, pid, 0, addr, len, (uintptr_t)readbuf) == -1)
    return 0;

  if (writebuf
      && ttrace (TT_PROC_WRTEXT, pid, 0, addr, len, (uintptr_t)writebuf) == -1
      && ttrace (TT_PROC_WRDATA, pid, 0, addr, len, (uintptr_t)writebuf) == -1)
    return 0;

  return len;
}

static LONGEST
inf_ttrace_xfer_partial (struct target_ops *ops, enum target_object object,
			 const char *annex, gdb_byte *readbuf,
			 const gdb_byte *writebuf, ULONGEST offset, LONGEST len)
{
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
      return inf_ttrace_xfer_memory (offset, len, readbuf, writebuf);

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

/* Print status information about what we're accessing.  */

static void
inf_ttrace_files_info (struct target_ops *ignore)
{
  printf_filtered (_("\tUsing the running image of %s %s.\n"),
		   attach_flag ? "attached" : "child",
		   target_pid_to_str (inferior_ptid));
}

static int
inf_ttrace_thread_alive (ptid_t ptid)
{
  return 1;
}

static char *
inf_ttrace_pid_to_str (ptid_t ptid)
{
  if (inf_ttrace_num_lwps > 0)
    {
      pid_t pid = ptid_get_pid (ptid);
      lwpid_t lwpid = ptid_get_lwp (ptid);
      static char buf[128];

      xsnprintf (buf, sizeof buf, "process %ld, lwp %ld",
		 (long)pid, (long)lwpid);
      return buf;
    }

  return normal_pid_to_str (ptid);
}


struct target_ops *
inf_ttrace_target (void)
{
  struct target_ops *t = inf_child_target ();

  t->to_attach = inf_ttrace_attach;
  t->to_detach = inf_ttrace_detach;
  t->to_resume = inf_ttrace_resume;
  t->to_wait = inf_ttrace_wait;
  t->to_files_info = inf_ttrace_files_info;
  t->to_can_use_hw_breakpoint = inf_ttrace_can_use_hw_breakpoint;
  t->to_insert_watchpoint = inf_ttrace_insert_watchpoint;
  t->to_remove_watchpoint = inf_ttrace_remove_watchpoint;
  t->to_stopped_by_watchpoint = inf_ttrace_stopped_by_watchpoint;
  t->to_region_ok_for_hw_watchpoint =
    inf_ttrace_region_ok_for_hw_watchpoint;
  t->to_kill = inf_ttrace_kill;
  t->to_create_inferior = inf_ttrace_create_inferior;
  t->to_follow_fork = inf_ttrace_follow_fork;
  t->to_mourn_inferior = inf_ttrace_mourn_inferior;
  t->to_thread_alive = inf_ttrace_thread_alive;
  t->to_pid_to_str = inf_ttrace_pid_to_str;
  t->to_xfer_partial = inf_ttrace_xfer_partial;

  ttrace_ops_hack = t;
  return t;
}
#endif


/* Prevent warning from -Wmissing-prototypes.  */
void _initialize_hppa_hpux_nat (void);

void
_initialize_inf_ttrace (void)
{
#ifdef HAVE_TTRACE
  inf_ttrace_page_dict.pagesize = getpagesize();
#endif
}
