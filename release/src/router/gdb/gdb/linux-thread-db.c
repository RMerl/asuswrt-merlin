/* libthread_db assisted debugging support, generic parts.

   Copyright (C) 1999, 2000, 2001, 2003, 2004, 2005, 2006, 2007
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

#include "gdb_assert.h"
#include <dlfcn.h>
#include "gdb_proc_service.h"
#include "gdb_thread_db.h"

#include "bfd.h"
#include "exceptions.h"
#include "gdbthread.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "regcache.h"
#include "solib-svr4.h"
#include "gdbcore.h"
#include "observer.h"
#include "linux-nat.h"

#include <signal.h>

#ifdef HAVE_GNU_LIBC_VERSION_H
#include <gnu/libc-version.h>
#endif

#ifndef LIBTHREAD_DB_SO
#define LIBTHREAD_DB_SO "libthread_db.so.1"
#endif

/* If we're running on GNU/Linux, we must explicitly attach to any new
   threads.  */

/* This module's target vector.  */
static struct target_ops thread_db_ops;

/* The target vector that we call for things this module can't handle.  */
static struct target_ops *target_beneath;

/* Non-zero if we're using this module's target vector.  */
static int using_thread_db;

/* Non-zero if we have determined the signals used by the threads
   library.  */
static int thread_signals;
static sigset_t thread_stop_set;
static sigset_t thread_print_set;

/* Structure that identifies the child process for the
   <proc_service.h> interface.  */
static struct ps_prochandle proc_handle;

/* Connection to the libthread_db library.  */
static td_thragent_t *thread_agent;

/* Pointers to the libthread_db functions.  */

static td_err_e (*td_init_p) (void);

static td_err_e (*td_ta_new_p) (struct ps_prochandle * ps,
				td_thragent_t **ta);
static td_err_e (*td_ta_map_id2thr_p) (const td_thragent_t *ta, thread_t pt,
				       td_thrhandle_t *__th);
static td_err_e (*td_ta_map_lwp2thr_p) (const td_thragent_t *ta,
					lwpid_t lwpid, td_thrhandle_t *th);
static td_err_e (*td_ta_thr_iter_p) (const td_thragent_t *ta,
				     td_thr_iter_f *callback, void *cbdata_p,
				     td_thr_state_e state, int ti_pri,
				     sigset_t *ti_sigmask_p,
				     unsigned int ti_user_flags);
static td_err_e (*td_ta_event_addr_p) (const td_thragent_t *ta,
				       td_event_e event, td_notify_t *ptr);
static td_err_e (*td_ta_set_event_p) (const td_thragent_t *ta,
				      td_thr_events_t *event);
static td_err_e (*td_ta_event_getmsg_p) (const td_thragent_t *ta,
					 td_event_msg_t *msg);

static td_err_e (*td_thr_validate_p) (const td_thrhandle_t *th);
static td_err_e (*td_thr_get_info_p) (const td_thrhandle_t *th,
				      td_thrinfo_t *infop);
static td_err_e (*td_thr_event_enable_p) (const td_thrhandle_t *th,
					  int event);

static td_err_e (*td_thr_tls_get_addr_p) (const td_thrhandle_t *th,
					  void *map_address,
					  size_t offset, void **address);

/* Location of the thread creation event breakpoint.  The code at this
   location in the child process will be called by the pthread library
   whenever a new thread is created.  By setting a special breakpoint
   at this location, GDB can detect when a new thread is created.  We
   obtain this location via the td_ta_event_addr call.  */
static CORE_ADDR td_create_bp_addr;

/* Location of the thread death event breakpoint.  */
static CORE_ADDR td_death_bp_addr;

/* Prototypes for local functions.  */
static void thread_db_find_new_threads (void);
static void attach_thread (ptid_t ptid, const td_thrhandle_t *th_p,
			   const td_thrinfo_t *ti_p, int verbose);
static void detach_thread (ptid_t ptid, int verbose);


/* Building process ids.  */

#define GET_PID(ptid)		ptid_get_pid (ptid)
#define GET_LWP(ptid)		ptid_get_lwp (ptid)
#define GET_THREAD(ptid)	ptid_get_tid (ptid)

#define is_lwp(ptid)		(GET_LWP (ptid) != 0)
#define is_thread(ptid)		(GET_THREAD (ptid) != 0)

#define BUILD_LWP(lwp, pid)	ptid_build (pid, lwp, 0)


/* Use "struct private_thread_info" to cache thread state.  This is
   a substantial optimization.  */

struct private_thread_info
{
  /* Flag set when we see a TD_DEATH event for this thread.  */
  unsigned int dying:1;

  /* Cached thread state.  */
  unsigned int th_valid:1;
  unsigned int ti_valid:1;

  td_thrhandle_t th;
  td_thrinfo_t ti;
};


static char *
thread_db_err_str (td_err_e err)
{
  static char buf[64];

  switch (err)
    {
    case TD_OK:
      return "generic 'call succeeded'";
    case TD_ERR:
      return "generic error";
    case TD_NOTHR:
      return "no thread to satisfy query";
    case TD_NOSV:
      return "no sync handle to satisfy query";
    case TD_NOLWP:
      return "no LWP to satisfy query";
    case TD_BADPH:
      return "invalid process handle";
    case TD_BADTH:
      return "invalid thread handle";
    case TD_BADSH:
      return "invalid synchronization handle";
    case TD_BADTA:
      return "invalid thread agent";
    case TD_BADKEY:
      return "invalid key";
    case TD_NOMSG:
      return "no event message for getmsg";
    case TD_NOFPREGS:
      return "FPU register set not available";
    case TD_NOLIBTHREAD:
      return "application not linked with libthread";
    case TD_NOEVENT:
      return "requested event is not supported";
    case TD_NOCAPAB:
      return "capability not available";
    case TD_DBERR:
      return "debugger service failed";
    case TD_NOAPLIC:
      return "operation not applicable to";
    case TD_NOTSD:
      return "no thread-specific data for this thread";
    case TD_MALLOC:
      return "malloc failed";
    case TD_PARTIALREG:
      return "only part of register set was written/read";
    case TD_NOXREGS:
      return "X register set not available for this thread";
#ifdef THREAD_DB_HAS_TD_NOTALLOC
    case TD_NOTALLOC:
      return "thread has not yet allocated TLS for given module";
#endif
#ifdef THREAD_DB_HAS_TD_VERSION
    case TD_VERSION:
      return "versions of libpthread and libthread_db do not match";
#endif
#ifdef THREAD_DB_HAS_TD_NOTLS
    case TD_NOTLS:
      return "there is no TLS segment in the given module";
#endif
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db error '%d'", err);
      return buf;
    }
}

/* Return 1 if any threads have been registered.  There may be none if
   the threading library is not fully initialized yet.  */

static int
have_threads_callback (struct thread_info *thread, void *dummy)
{
  return 1;
}

static int
have_threads (void)
{
  return iterate_over_threads (have_threads_callback, NULL) != NULL;
}

/* A callback function for td_ta_thr_iter, which we use to map all
   threads to LWPs.

   THP is a handle to the current thread; if INFOP is not NULL, the
   struct thread_info associated with this thread is returned in
   *INFOP.

   If the thread is a zombie, TD_THR_ZOMBIE is returned.  Otherwise,
   zero is returned to indicate success.  */

static int
thread_get_info_callback (const td_thrhandle_t *thp, void *infop)
{
  td_thrinfo_t ti;
  td_err_e err;
  struct thread_info *thread_info;
  ptid_t thread_ptid;

  err = td_thr_get_info_p (thp, &ti);
  if (err != TD_OK)
    error (_("thread_get_info_callback: cannot get thread info: %s"),
	   thread_db_err_str (err));

  /* Fill the cache.  */
  thread_ptid = ptid_build (GET_PID (inferior_ptid), ti.ti_lid, ti.ti_tid);
  thread_info = find_thread_pid (thread_ptid);

  /* In the case of a zombie thread, don't continue.  We don't want to
     attach to it thinking it is a new thread.  */
  if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
    {
      if (infop != NULL)
        *(struct thread_info **) infop = thread_info;
      if (thread_info != NULL)
	{
	  memcpy (&thread_info->private->th, thp, sizeof (*thp));
	  thread_info->private->th_valid = 1;
	  memcpy (&thread_info->private->ti, &ti, sizeof (ti));
	  thread_info->private->ti_valid = 1;
	}
      return TD_THR_ZOMBIE;
    }

  if (thread_info == NULL)
    {
      /* New thread.  Attach to it now (why wait?).  */
      attach_thread (thread_ptid, thp, &ti, 1);
      thread_info = find_thread_pid (thread_ptid);
      gdb_assert (thread_info != NULL);
    }

  memcpy (&thread_info->private->th, thp, sizeof (*thp));
  thread_info->private->th_valid = 1;
  memcpy (&thread_info->private->ti, &ti, sizeof (ti));
  thread_info->private->ti_valid = 1;

  if (infop != NULL)
    *(struct thread_info **) infop = thread_info;

  return 0;
}

/* Accessor functions for the thread_db information, with caching.  */

static void
thread_db_map_id2thr (struct thread_info *thread_info, int fatal)
{
  td_err_e err;

  if (thread_info->private->th_valid)
    return;

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (thread_info->ptid),
			    &thread_info->private->th);
  if (err != TD_OK)
    {
      if (fatal)
	error (_("Cannot find thread %ld: %s"),
	       (long) GET_THREAD (thread_info->ptid),
	       thread_db_err_str (err));
    }
  else
    thread_info->private->th_valid = 1;
}

/* Convert between user-level thread ids and LWP ids.  */

static ptid_t
thread_from_lwp (ptid_t ptid)
{
  td_thrhandle_t th;
  td_err_e err;
  struct thread_info *thread_info;
  ptid_t thread_ptid;

  if (GET_LWP (ptid) == 0)
    ptid = BUILD_LWP (GET_PID (ptid), GET_PID (ptid));

  gdb_assert (is_lwp (ptid));

  err = td_ta_map_lwp2thr_p (thread_agent, GET_LWP (ptid), &th);
  if (err != TD_OK)
    error (_("Cannot find user-level thread for LWP %ld: %s"),
	   GET_LWP (ptid), thread_db_err_str (err));

  thread_info = NULL;

  /* Fetch the thread info.  If we get back TD_THR_ZOMBIE, then the
     event thread has already died.  If another gdb interface has called
     thread_alive() previously, the thread won't be found on the thread list
     anymore.  In that case, we don't want to process this ptid anymore
     to avoid the possibility of later treating it as a newly
     discovered thread id that we should add to the list.  Thus,
     we return a -1 ptid which is also how the thread list marks a
     dead thread.  */
  if (thread_get_info_callback (&th, &thread_info) == TD_THR_ZOMBIE
      && thread_info == NULL)
    return pid_to_ptid (-1);

  gdb_assert (thread_info && thread_info->private->ti_valid);

  return ptid_build (GET_PID (ptid), GET_LWP (ptid),
		     thread_info->private->ti.ti_tid);
}

static ptid_t
lwp_from_thread (ptid_t ptid)
{
  return BUILD_LWP (GET_LWP (ptid), GET_PID (ptid));
}


void
thread_db_init (struct target_ops *target)
{
  target_beneath = target;
}

static void *
verbose_dlsym (void *handle, const char *name)
{
  void *sym = dlsym (handle, name);
  if (sym == NULL)
    warning (_("Symbol \"%s\" not found in libthread_db: %s"), name, dlerror ());
  return sym;
}

static int
thread_db_load (void)
{
  void *handle;
  td_err_e err;

  handle = dlopen (LIBTHREAD_DB_SO, RTLD_NOW);
  if (handle == NULL)
    {
      fprintf_filtered (gdb_stderr, "\n\ndlopen failed on '%s' - %s\n",
			LIBTHREAD_DB_SO, dlerror ());
      fprintf_filtered (gdb_stderr,
			"GDB will not be able to debug pthreads.\n\n");
      return 0;
    }

  /* Initialize pointers to the dynamic library functions we will use.
     Essential functions first.  */

  td_init_p = verbose_dlsym (handle, "td_init");
  if (td_init_p == NULL)
    return 0;

  td_ta_new_p = verbose_dlsym (handle, "td_ta_new");
  if (td_ta_new_p == NULL)
    return 0;

  td_ta_map_id2thr_p = verbose_dlsym (handle, "td_ta_map_id2thr");
  if (td_ta_map_id2thr_p == NULL)
    return 0;

  td_ta_map_lwp2thr_p = verbose_dlsym (handle, "td_ta_map_lwp2thr");
  if (td_ta_map_lwp2thr_p == NULL)
    return 0;

  td_ta_thr_iter_p = verbose_dlsym (handle, "td_ta_thr_iter");
  if (td_ta_thr_iter_p == NULL)
    return 0;

  td_thr_validate_p = verbose_dlsym (handle, "td_thr_validate");
  if (td_thr_validate_p == NULL)
    return 0;

  td_thr_get_info_p = verbose_dlsym (handle, "td_thr_get_info");
  if (td_thr_get_info_p == NULL)
    return 0;

  /* Initialize the library.  */
  err = td_init_p ();
  if (err != TD_OK)
    {
      warning (_("Cannot initialize libthread_db: %s"), thread_db_err_str (err));
      return 0;
    }

  /* These are not essential.  */
  td_ta_event_addr_p = dlsym (handle, "td_ta_event_addr");
  td_ta_set_event_p = dlsym (handle, "td_ta_set_event");
  td_ta_event_getmsg_p = dlsym (handle, "td_ta_event_getmsg");
  td_thr_event_enable_p = dlsym (handle, "td_thr_event_enable");
  td_thr_tls_get_addr_p = dlsym (handle, "td_thr_tls_get_addr");

  return 1;
}

static td_err_e
enable_thread_event (td_thragent_t *thread_agent, int event, CORE_ADDR *bp)
{
  td_notify_t notify;
  td_err_e err;

  /* Get the breakpoint address for thread EVENT.  */
  err = td_ta_event_addr_p (thread_agent, event, &notify);
  if (err != TD_OK)
    return err;

  /* Set up the breakpoint.  */
  gdb_assert (exec_bfd);
  (*bp) = (gdbarch_convert_from_func_ptr_addr
	   (current_gdbarch,
	    /* Do proper sign extension for the target.  */
	    (bfd_get_sign_extend_vma (exec_bfd) > 0
	     ? (CORE_ADDR) (intptr_t) notify.u.bptaddr
	     : (CORE_ADDR) (uintptr_t) notify.u.bptaddr),
	    &current_target));
  create_thread_event_breakpoint ((*bp));

  return TD_OK;
}

static void
enable_thread_event_reporting (void)
{
  td_thr_events_t events;
  td_notify_t notify;
  td_err_e err;
#ifdef HAVE_GNU_LIBC_VERSION_H
  const char *libc_version;
  int libc_major, libc_minor;
#endif

  /* We cannot use the thread event reporting facility if these
     functions aren't available.  */
  if (td_ta_event_addr_p == NULL || td_ta_set_event_p == NULL
      || td_ta_event_getmsg_p == NULL || td_thr_event_enable_p == NULL)
    return;

  /* Set the process wide mask saying which events we're interested in.  */
  td_event_emptyset (&events);
  td_event_addset (&events, TD_CREATE);

#ifdef HAVE_GNU_LIBC_VERSION_H
  /* The event reporting facility is broken for TD_DEATH events in
     glibc 2.1.3, so don't enable it if we have glibc but a lower
     version.  */
  libc_version = gnu_get_libc_version ();
  if (sscanf (libc_version, "%d.%d", &libc_major, &libc_minor) == 2
      && (libc_major > 2 || (libc_major == 2 && libc_minor > 1)))
#endif
    td_event_addset (&events, TD_DEATH);

  err = td_ta_set_event_p (thread_agent, &events);
  if (err != TD_OK)
    {
      warning (_("Unable to set global thread event mask: %s"),
	       thread_db_err_str (err));
      return;
    }

  /* Delete previous thread event breakpoints, if any.  */
  remove_thread_event_breakpoints ();
  td_create_bp_addr = 0;
  td_death_bp_addr = 0;

  /* Set up the thread creation event.  */
  err = enable_thread_event (thread_agent, TD_CREATE, &td_create_bp_addr);
  if (err != TD_OK)
    {
      warning (_("Unable to get location for thread creation breakpoint: %s"),
	       thread_db_err_str (err));
      return;
    }

  /* Set up the thread death event.  */
  err = enable_thread_event (thread_agent, TD_DEATH, &td_death_bp_addr);
  if (err != TD_OK)
    {
      warning (_("Unable to get location for thread death breakpoint: %s"),
	       thread_db_err_str (err));
      return;
    }
}

static void
disable_thread_event_reporting (void)
{
  td_thr_events_t events;

  /* Set the process wide mask saying we aren't interested in any
     events anymore.  */
  td_event_emptyset (&events);
  td_ta_set_event_p (thread_agent, &events);

  /* Delete thread event breakpoints, if any.  */
  remove_thread_event_breakpoints ();
  td_create_bp_addr = 0;
  td_death_bp_addr = 0;
}

static void
check_thread_signals (void)
{
#ifdef GET_THREAD_SIGNALS
  if (!thread_signals)
    {
      sigset_t mask;
      int i;

      GET_THREAD_SIGNALS (&mask);
      sigemptyset (&thread_stop_set);
      sigemptyset (&thread_print_set);

      for (i = 1; i < NSIG; i++)
	{
	  if (sigismember (&mask, i))
	    {
	      if (signal_stop_update (target_signal_from_host (i), 0))
		sigaddset (&thread_stop_set, i);
	      if (signal_print_update (target_signal_from_host (i), 0))
		sigaddset (&thread_print_set, i);
	      thread_signals = 1;
	    }
	}
    }
#endif
}

/* Check whether thread_db is usable.  This function is called when
   an inferior is created (or otherwise acquired, e.g. attached to)
   and when new shared libraries are loaded into a running process.  */

void
check_for_thread_db (void)
{
  td_err_e err;
  static int already_loaded;

  /* Do nothing if we couldn't load libthread_db.so.1.  */
  if (td_ta_new_p == NULL)
    return;

  /* First time through, report that libthread_db was successfuly
     loaded.  Can't print this in in thread_db_load as, at that stage,
     the interpreter and it's console haven't started.  */

  if (!already_loaded)
    {
      Dl_info info;
      const char *library = NULL;
      if (dladdr ((*td_ta_new_p), &info) != 0)
	library = info.dli_fname;

      /* Try dlinfo?  */

      if (library == NULL)
	/* Paranoid - don't let a NULL path slip through.  */
	library = LIBTHREAD_DB_SO;

      printf_unfiltered (_("Using host libthread_db library \"%s\".\n"),
			 library);
      already_loaded = 1;
    }

  if (using_thread_db)
    /* Nothing to do.  The thread library was already detected and the
       target vector was already activated.  */
    return;

  /* Don't attempt to use thread_db on targets which can not run
     (executables not running yet, core files) for now.  */
  if (!target_has_execution)
    return;

  /* Don't attempt to use thread_db for remote targets.  */
  if (!target_can_run (&current_target))
    return;

  /* Initialize the structure that identifies the child process.  */
  proc_handle.pid = GET_PID (inferior_ptid);

  /* Now attempt to open a connection to the thread library.  */
  err = td_ta_new_p (&proc_handle, &thread_agent);
  switch (err)
    {
    case TD_NOLIBTHREAD:
      /* No thread library was detected.  */
      break;

    case TD_OK:
      printf_unfiltered (_("[Thread debugging using libthread_db enabled]\n"));

      /* The thread library was detected.  Activate the thread_db target.  */
      push_target (&thread_db_ops);
      using_thread_db = 1;

      enable_thread_event_reporting ();
      thread_db_find_new_threads ();
      break;

    default:
      warning (_("Cannot initialize thread debugging library: %s"),
	       thread_db_err_str (err));
      break;
    }
}

static void
thread_db_new_objfile (struct objfile *objfile)
{
  if (objfile != NULL)
    check_for_thread_db ();
}

/* Attach to a new thread.  This function is called when we receive a
   TD_CREATE event or when we iterate over all threads and find one
   that wasn't already in our list.  */

static void
attach_thread (ptid_t ptid, const td_thrhandle_t *th_p,
	       const td_thrinfo_t *ti_p, int verbose)
{
  struct thread_info *tp;
  td_err_e err;

  /* If we're being called after a TD_CREATE event, we may already
     know about this thread.  There are two ways this can happen.  We
     may have iterated over all threads between the thread creation
     and the TD_CREATE event, for instance when the user has issued
     the `info threads' command before the SIGTRAP for hitting the
     thread creation breakpoint was reported.  Alternatively, the
     thread may have exited and a new one been created with the same
     thread ID.  In the first case we don't need to do anything; in
     the second case we should discard information about the dead
     thread and attach to the new one.  */
  if (in_thread_list (ptid))
    {
      tp = find_thread_pid (ptid);
      gdb_assert (tp != NULL);

      if (!tp->private->dying)
        return;

      delete_thread (ptid);
    }

  check_thread_signals ();

  if (ti_p->ti_state == TD_THR_UNKNOWN || ti_p->ti_state == TD_THR_ZOMBIE)
    return;			/* A zombie thread -- do not attach.  */

  /* Under GNU/Linux, we have to attach to each and every thread.  */
  if (lin_lwp_attach_lwp (BUILD_LWP (ti_p->ti_lid, GET_PID (ptid)), 0) < 0)
    return;

  /* Add the thread to GDB's thread list.  */
  tp = add_thread (ptid);
  tp->private = xmalloc (sizeof (struct private_thread_info));
  memset (tp->private, 0, sizeof (struct private_thread_info));

  if (verbose)
    printf_unfiltered (_("[New %s]\n"), target_pid_to_str (ptid));

  /* Enable thread event reporting for this thread.  */
  err = td_thr_event_enable_p (th_p, 1);
  if (err != TD_OK)
    error (_("Cannot enable thread event reporting for %s: %s"),
	   target_pid_to_str (ptid), thread_db_err_str (err));
}

static void
detach_thread (ptid_t ptid, int verbose)
{
  struct thread_info *thread_info;

  if (verbose)
    printf_unfiltered (_("[%s exited]\n"), target_pid_to_str (ptid));

  /* Don't delete the thread now, because it still reports as active
     until it has executed a few instructions after the event
     breakpoint - if we deleted it now, "info threads" would cause us
     to re-attach to it.  Just mark it as having had a TD_DEATH
     event.  This means that we won't delete it from our thread list
     until we notice that it's dead (via prune_threads), or until
     something re-uses its thread ID.  */
  thread_info = find_thread_pid (ptid);
  gdb_assert (thread_info != NULL);
  thread_info->private->dying = 1;
}

static void
thread_db_detach (char *args, int from_tty)
{
  disable_thread_event_reporting ();

  /* There's no need to save & restore inferior_ptid here, since the
     inferior is not supposed to survive this function call.  */
  inferior_ptid = lwp_from_thread (inferior_ptid);

  target_beneath->to_detach (args, from_tty);

  /* Should this be done by detach_command?  */
  target_mourn_inferior ();
}

static int
clear_lwpid_callback (struct thread_info *thread, void *dummy)
{
  /* If we know that our thread implementation is 1-to-1, we could save
     a certain amount of information; it's not clear how much, so we
     are always conservative.  */

  thread->private->th_valid = 0;
  thread->private->ti_valid = 0;

  return 0;
}

static void
thread_db_resume (ptid_t ptid, int step, enum target_signal signo)
{
  struct cleanup *old_chain = save_inferior_ptid ();

  if (GET_PID (ptid) == -1)
    inferior_ptid = lwp_from_thread (inferior_ptid);
  else if (is_thread (ptid))
    ptid = lwp_from_thread (ptid);

  /* Clear cached data which may not be valid after the resume.  */
  iterate_over_threads (clear_lwpid_callback, NULL);

  target_beneath->to_resume (ptid, step, signo);

  do_cleanups (old_chain);
}

/* Check if PID is currently stopped at the location of a thread event
   breakpoint location.  If it is, read the event message and act upon
   the event.  */

static void
check_event (ptid_t ptid)
{
  td_event_msg_t msg;
  td_thrinfo_t ti;
  td_err_e err;
  CORE_ADDR stop_pc;
  int loop = 0;

  /* Bail out early if we're not at a thread event breakpoint.  */
  stop_pc = read_pc_pid (ptid) - gdbarch_decr_pc_after_break (current_gdbarch);
  if (stop_pc != td_create_bp_addr && stop_pc != td_death_bp_addr)
    return;

  /* If we are at a create breakpoint, we do not know what new lwp
     was created and cannot specifically locate the event message for it.
     We have to call td_ta_event_getmsg() to get
     the latest message.  Since we have no way of correlating whether
     the event message we get back corresponds to our breakpoint, we must
     loop and read all event messages, processing them appropriately.
     This guarantees we will process the correct message before continuing
     from the breakpoint.

     Currently, death events are not enabled.  If they are enabled,
     the death event can use the td_thr_event_getmsg() interface to
     get the message specifically for that lwp and avoid looping
     below.  */

  loop = 1;

  do
    {
      err = td_ta_event_getmsg_p (thread_agent, &msg);
      if (err != TD_OK)
	{
	  if (err == TD_NOMSG)
	    return;

	  error (_("Cannot get thread event message: %s"),
		 thread_db_err_str (err));
	}

      err = td_thr_get_info_p (msg.th_p, &ti);
      if (err != TD_OK)
	error (_("Cannot get thread info: %s"), thread_db_err_str (err));

      ptid = ptid_build (GET_PID (ptid), ti.ti_lid, ti.ti_tid);

      switch (msg.event)
	{
	case TD_CREATE:
	  /* Call attach_thread whether or not we already know about a
	     thread with this thread ID.  */
	  attach_thread (ptid, msg.th_p, &ti, 1);

	  break;

	case TD_DEATH:

	  if (!in_thread_list (ptid))
	    error (_("Spurious thread death event."));

	  detach_thread (ptid, 1);

	  break;

	default:
	  error (_("Spurious thread event."));
	}
    }
  while (loop);
}

static ptid_t
thread_db_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  extern ptid_t trap_ptid;

  if (GET_PID (ptid) != -1 && is_thread (ptid))
    ptid = lwp_from_thread (ptid);

  ptid = target_beneath->to_wait (ptid, ourstatus);

  if (ourstatus->kind == TARGET_WAITKIND_EXITED
    || ourstatus->kind == TARGET_WAITKIND_SIGNALLED)
    return pid_to_ptid (-1);

  if (ourstatus->kind == TARGET_WAITKIND_EXECD)
    {
      remove_thread_event_breakpoints ();
      unpush_target (&thread_db_ops);
      using_thread_db = 0;

      return pid_to_ptid (GET_PID (ptid));
    }

  /* If we do not know about the main thread yet, this would be a good time to
     find it.  */
  if (ourstatus->kind == TARGET_WAITKIND_STOPPED && !have_threads ())
    thread_db_find_new_threads ();

  if (ourstatus->kind == TARGET_WAITKIND_STOPPED
      && ourstatus->value.sig == TARGET_SIGNAL_TRAP)
    /* Check for a thread event.  */
    check_event (ptid);

  if (have_threads ())
    {
      /* Change ptids back into the higher level PID + TID format.  If
	 the thread is dead and no longer on the thread list, we will
	 get back a dead ptid.  This can occur if the thread death
	 event gets postponed by other simultaneous events.  In such a
	 case, we want to just ignore the event and continue on.  */

      if (!ptid_equal (trap_ptid, null_ptid))
	trap_ptid = thread_from_lwp (trap_ptid);

      ptid = thread_from_lwp (ptid);
      if (GET_PID (ptid) == -1)
	ourstatus->kind = TARGET_WAITKIND_SPURIOUS;
    }

  return ptid;
}

static void
thread_db_kill (void)
{
  /* There's no need to save & restore inferior_ptid here, since the
     inferior isn't supposed to survive this function call.  */
  inferior_ptid = lwp_from_thread (inferior_ptid);
  target_beneath->to_kill ();
}

static void
thread_db_mourn_inferior (void)
{
  /* Forget about the child's process ID.  We shouldn't need it
     anymore.  */
  proc_handle.pid = 0;

  target_beneath->to_mourn_inferior ();

  /* Delete the old thread event breakpoints.  Do this after mourning
     the inferior, so that we don't try to uninsert them.  */
  remove_thread_event_breakpoints ();

  /* Detach thread_db target ops.  */
  unpush_target (&thread_db_ops);
  using_thread_db = 0;
}

static int
find_new_threads_callback (const td_thrhandle_t *th_p, void *data)
{
  td_thrinfo_t ti;
  td_err_e err;
  ptid_t ptid;

  err = td_thr_get_info_p (th_p, &ti);
  if (err != TD_OK)
    error (_("find_new_threads_callback: cannot get thread info: %s"),
	   thread_db_err_str (err));

  if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
    return 0;			/* A zombie -- ignore.  */

  ptid = ptid_build (GET_PID (inferior_ptid), ti.ti_lid, ti.ti_tid);

  if (ti.ti_tid == 0)
    {
      /* A thread ID of zero means that this is the main thread, but
	 glibc has not yet initialized thread-local storage and the
	 pthread library.  We do not know what the thread's TID will
	 be yet.  Just enable event reporting and otherwise ignore
	 it.  */

      err = td_thr_event_enable_p (th_p, 1);
      if (err != TD_OK)
	error (_("Cannot enable thread event reporting for %s: %s"),
	       target_pid_to_str (ptid), thread_db_err_str (err));

      return 0;
    }

  if (!in_thread_list (ptid))
    attach_thread (ptid, th_p, &ti, 1);

  return 0;
}

static void
thread_db_find_new_threads (void)
{
  td_err_e err;

  /* Iterate over all user-space threads to discover new threads.  */
  err = td_ta_thr_iter_p (thread_agent, find_new_threads_callback, NULL,
			  TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
			  TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
  if (err != TD_OK)
    error (_("Cannot find new threads: %s"), thread_db_err_str (err));
}

static char *
thread_db_pid_to_str (ptid_t ptid)
{
  if (is_thread (ptid))
    {
      static char buf[64];
      struct thread_info *thread_info;

      thread_info = find_thread_pid (ptid);
      if (thread_info == NULL)
	snprintf (buf, sizeof (buf), "Thread 0x%lx (LWP %ld) (Missing)",
		  GET_THREAD (ptid), GET_LWP (ptid));
      else
	snprintf (buf, sizeof (buf), "Thread 0x%lx (LWP %ld)",
		  GET_THREAD (ptid), GET_LWP (ptid));

      return buf;
    }

  if (target_beneath->to_pid_to_str (ptid))
    return target_beneath->to_pid_to_str (ptid);

  return normal_pid_to_str (ptid);
}

/* Return a string describing the state of the thread specified by
   INFO.  */

static char *
thread_db_extra_thread_info (struct thread_info *info)
{
  if (info->private->dying)
    return "Exiting";

  return NULL;
}

/* Return 1 if this thread has the same LWP as the passed PTID.  */

static int
same_ptid_callback (struct thread_info *thread, void *arg)
{
  ptid_t *ptid_p = arg;

  return GET_LWP (thread->ptid) == GET_LWP (*ptid_p);
}

/* Get the address of the thread local variable in load module LM which
   is stored at OFFSET within the thread local storage for thread PTID.  */

static CORE_ADDR
thread_db_get_thread_local_address (ptid_t ptid,
				    CORE_ADDR lm,
				    CORE_ADDR offset)
{
  /* If we have not discovered any threads yet, check now.  */
  if (!is_thread (ptid) && !have_threads ())
    thread_db_find_new_threads ();

  /* Try to find a matching thread if we still have the LWP ID instead
     of the thread ID.  */
  if (!is_thread (ptid))
    {
      struct thread_info *thread;

      thread = iterate_over_threads (same_ptid_callback, &ptid);
      if (thread != NULL)
	ptid = thread->ptid;
    }

  if (is_thread (ptid))
    {
      td_err_e err;
      void *address;
      struct thread_info *thread_info;

      /* glibc doesn't provide the needed interface.  */
      if (!td_thr_tls_get_addr_p)
	throw_error (TLS_NO_LIBRARY_SUPPORT_ERROR,
		     _("No TLS library support"));

      /* Caller should have verified that lm != 0.  */
      gdb_assert (lm != 0);

      /* Get info about the thread.  */
      thread_info = find_thread_pid (ptid);
      gdb_assert (thread_info);
      thread_db_map_id2thr (thread_info, 1);

      /* Finally, get the address of the variable.  */
      err = td_thr_tls_get_addr_p (&thread_info->private->th,
				   (void *)(size_t) lm,
				   offset, &address);

#ifdef THREAD_DB_HAS_TD_NOTALLOC
      /* The memory hasn't been allocated, yet.  */
      if (err == TD_NOTALLOC)
	  /* Now, if libthread_db provided the initialization image's
	     address, we *could* try to build a non-lvalue value from
	     the initialization image.  */
        throw_error (TLS_NOT_ALLOCATED_YET_ERROR,
                     _("TLS not allocated yet"));
#endif

      /* Something else went wrong.  */
      if (err != TD_OK)
        throw_error (TLS_GENERIC_ERROR,
                     (("%s")), thread_db_err_str (err));

      /* Cast assuming host == target.  Joy.  */
      /* Do proper sign extension for the target.  */
      gdb_assert (exec_bfd);
      return (bfd_get_sign_extend_vma (exec_bfd) > 0
	      ? (CORE_ADDR) (intptr_t) address
	      : (CORE_ADDR) (uintptr_t) address);
    }

  if (target_beneath->to_get_thread_local_address)
    return target_beneath->to_get_thread_local_address (ptid, lm, offset);
  else
    throw_error (TLS_GENERIC_ERROR,
	         _("TLS not supported on this target"));
}

static void
init_thread_db_ops (void)
{
  thread_db_ops.to_shortname = "multi-thread";
  thread_db_ops.to_longname = "multi-threaded child process.";
  thread_db_ops.to_doc = "Threads and pthreads support.";
  thread_db_ops.to_detach = thread_db_detach;
  thread_db_ops.to_resume = thread_db_resume;
  thread_db_ops.to_wait = thread_db_wait;
  thread_db_ops.to_kill = thread_db_kill;
  thread_db_ops.to_mourn_inferior = thread_db_mourn_inferior;
  thread_db_ops.to_find_new_threads = thread_db_find_new_threads;
  thread_db_ops.to_pid_to_str = thread_db_pid_to_str;
  thread_db_ops.to_stratum = thread_stratum;
  thread_db_ops.to_has_thread_control = tc_schedlock;
  thread_db_ops.to_get_thread_local_address
    = thread_db_get_thread_local_address;
  thread_db_ops.to_extra_thread_info = thread_db_extra_thread_info;
  thread_db_ops.to_magic = OPS_MAGIC;
}

void
_initialize_thread_db (void)
{
  /* Only initialize the module if we can load libthread_db.  */
  if (thread_db_load ())
    {
      init_thread_db_ops ();
      add_target (&thread_db_ops);

      /* Add ourselves to objfile event chain.  */
      observer_attach_new_objfile (thread_db_new_objfile);
    }
}
