/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util_process.c
 * \brief utility functions for launching processes and checking their
 *    status. These functions are kept separately from procmon so that they
 *    won't require linking against libevent.
 **/

#include "orconfig.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "compat.h"
#include "util.h"
#include "torlog.h"
#include "util_process.h"
#include "ht.h"

/* ================================================== */
/* Convenience structures for handlers for waitpid().
 *
 * The tor_process_monitor*() code above doesn't use them, since it is for
 * monitoring a non-child process.
 */

#ifndef _WIN32

/** Mapping from a PID to a userfn/userdata pair. */
struct waitpid_callback_t {
  HT_ENTRY(waitpid_callback_t) node;
  pid_t pid;

  void (*userfn)(int, void *userdata);
  void *userdata;

  unsigned running;
};

static INLINE unsigned int
process_map_entry_hash_(const waitpid_callback_t *ent)
{
  return (unsigned) ent->pid;
}

static INLINE unsigned int
process_map_entries_eq_(const waitpid_callback_t *a,
                        const waitpid_callback_t *b)
{
  return a->pid == b->pid;
}

static HT_HEAD(process_map, waitpid_callback_t) process_map = HT_INITIALIZER();

HT_PROTOTYPE(process_map, waitpid_callback_t, node, process_map_entry_hash_,
             process_map_entries_eq_);
HT_GENERATE2(process_map, waitpid_callback_t, node, process_map_entry_hash_,
             process_map_entries_eq_, 0.6, tor_reallocarray_, tor_free_);

/**
 * Begin monitoring the child pid <b>pid</b> to see if we get a SIGCHLD for
 * it.  If we eventually do, call <b>fn</b>, passing it the exit status (as
 * yielded by waitpid) and the pointer <b>arg</b>.
 *
 * To cancel this, or clean up after it has triggered, call
 * clear_waitpid_callback().
 */
waitpid_callback_t *
set_waitpid_callback(pid_t pid, void (*fn)(int, void *), void *arg)
{
  waitpid_callback_t *old_ent;
  waitpid_callback_t *ent = tor_malloc_zero(sizeof(waitpid_callback_t));
  ent->pid = pid;
  ent->userfn = fn;
  ent->userdata = arg;
  ent->running = 1;

  old_ent = HT_REPLACE(process_map, &process_map, ent);
  if (old_ent) {
    log_warn(LD_BUG, "Replaced a waitpid monitor on pid %u. That should be "
             "impossible.", (unsigned) pid);
    old_ent->running = 0;
  }

  return ent;
}

/**
 * Cancel a waitpid_callback_t, or clean up after one has triggered. Releases
 * all storage held by <b>ent</b>.
 */
void
clear_waitpid_callback(waitpid_callback_t *ent)
{
  waitpid_callback_t *old_ent;
  if (ent == NULL)
    return;

  if (ent->running) {
    old_ent = HT_REMOVE(process_map, &process_map, ent);
    if (old_ent != ent) {
      log_warn(LD_BUG, "Couldn't remove waitpid monitor for pid %u.",
               (unsigned) ent->pid);
      return;
    }
  }

  tor_free(ent);
}

/** Helper: find the callack for <b>pid</b>; if there is one, run it,
 * reporting the exit status as <b>status</b>. */
static void
notify_waitpid_callback_by_pid(pid_t pid, int status)
{
  waitpid_callback_t search, *ent;

  search.pid = pid;
  ent = HT_REMOVE(process_map, &process_map, &search);
  if (!ent || !ent->running) {
    log_info(LD_GENERAL, "Child process %u has exited; no callback was "
             "registered", (unsigned)pid);
    return;
  }

  log_info(LD_GENERAL, "Child process %u has exited; running callback.",
           (unsigned)pid);

  ent->running = 0;
  ent->userfn(status, ent->userdata);
}

/** Use waitpid() to wait for all children that have exited, and invoke any
 * callbacks registered for them. */
void
notify_pending_waitpid_callbacks(void)
{
  /* I was going to call this function reap_zombie_children(), but
   * that makes it sound way more exciting than it really is. */
  pid_t child;
  int status = 0;

  while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
    notify_waitpid_callback_by_pid(child, status);
    status = 0; /* should be needless */
  }
}

#endif

