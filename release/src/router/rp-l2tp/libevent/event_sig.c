/***********************************************************************
*
* event_sig.c
*
* Code for handling signals nicely (synchronously) and for dealing
* with reaping child processes.
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: event_sig.c 3323 2011-09-21 18:45:48Z lly.dev $";

#define _POSIX_SOURCE 1 /* For sigaction defines */
#define _BSD_SOURCE   1 /* For SA_RESTART */

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stddef.h>

#include "event.h"
#include "hash.h"

/* Kludge for figuring out NSIG */
#ifdef NSIG
#define MAX_SIGNALS NSIG
#elif defined(_NSIG)
#define MAX_SIGNALS _NSIG
#else
#define MAX_SIGNALS 256  /* Should be safe... */
#endif

/* A structure for a "synchronous" signal handler */
struct SynchronousSignalHandler {
    int fired;			/* Have we received this signal? */
    void (*handler)(int sig);	/* Handler function              */
};

/* A structure for calling back when a child dies */
struct ChildEntry {
    hash_bucket hash;
    void (*handler)(pid_t pid, int status, void *data);
    pid_t pid;
    void *data;
};

static struct SynchronousSignalHandler SignalHandlers[MAX_SIGNALS];
static int Pipe[2] = {-1, -1};
static EventHandler *PipeHandler = NULL;
static sig_atomic_t PipeFull = 0;
static hash_table child_process_table;

static unsigned int child_hash(void *data)
{
    return (unsigned int) ((struct ChildEntry *) data)->pid;
}

static int child_compare(void *d1, void *d2)
{
    return ((struct ChildEntry *)d1)->pid != ((struct ChildEntry *)d2)->pid;
}

/**********************************************************************
* %FUNCTION: DoPipe
* %ARGUMENTS:
*  es -- event selector
*  fd -- readable file descriptor
*  flags -- flags from event system
*  data -- ignored
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Called when an async signal handler wants attention.  This function
*  fires all "synchronous" signal handlers.
***********************************************************************/
static void
DoPipe(EventSelector *es,
       int fd,
       unsigned int flags,
       void *data)
{
    char buf[64];
    int i;

    /* Clear buffer */
    read(fd, buf, 64);
    PipeFull = 0;

    /* Fire handlers */
    for (i=0; i<MAX_SIGNALS; i++) {
	if (SignalHandlers[i].fired &&
	    SignalHandlers[i].handler) {
	    SignalHandlers[i].handler(i);
	}
	SignalHandlers[i].fired = 0;
    }
}

/**********************************************************************
* %FUNCTION: sig_handler
* %ARGUMENTS:
*  sig -- signal number
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Marks a signal as having "fired"; fills IPC pipe.
***********************************************************************/
static void
sig_handler(int sig)
{
    if (sig <0 || sig > MAX_SIGNALS) {
	/* Ooops... */
	return;
    }
    SignalHandlers[sig].fired = 1;
    if (!PipeFull) {
	write(Pipe[1], &sig, 1);
	PipeFull = 1;
    }
}

/**********************************************************************
* %FUNCTION: child_handler
* %ARGUMENTS:
*  sig -- signal number (whoop-dee-doo)
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Called *SYNCHRONOUSLY* to reap dead children.
***********************************************************************/
static void
child_handler(int sig)
{
    int status;
    int pid;
    struct ChildEntry *ce;
    struct ChildEntry candidate;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	candidate.pid = (pid_t) pid;
	ce = hash_find(&child_process_table, &candidate);
	if (ce) {
	    if (ce->handler) {
		ce->handler(pid, status, ce->data);
	    }
	    hash_remove(&child_process_table, ce);
	    free(ce);
	}
    }
}

/**********************************************************************
* %FUNCTION: SetupPipes (static)
* %ARGUMENTS:
*  es -- event selector
* %RETURNS:
*  0 on success; -1 on failure
* %DESCRIPTION:
*  Sets up pipes with an event handler to handle IPC from a signal handler
***********************************************************************/
static int
SetupPipes(EventSelector *es)
{
    /* If already done, do nothing */
    if (PipeHandler) return 0;

    /* Initialize the child-process hash table */
    hash_init(&child_process_table,
	      offsetof(struct ChildEntry, hash),
	      child_hash,
	      child_compare);

    /* Open pipe to self */
    if (pipe(Pipe) < 0) {
	return -1;
    }

    PipeHandler = Event_AddHandler(es, Pipe[0],
				   EVENT_FLAG_READABLE, DoPipe, NULL);
    if (!PipeHandler) {
	int old_errno = errno;
	close(Pipe[0]);
	close(Pipe[1]);
	errno = old_errno;
	return -1;
    }
    return 0;
}

/**********************************************************************
* %FUNCTION: Event_HandleSignal
* %ARGUMENTS:
*  es -- event selector
*  sig -- signal number
*  handler -- handler to call when signal is raised.  Handler is called
*             "synchronously" as events are processed by event loop.
* %RETURNS:
*  0 on success, -1 on error.
* %DESCRIPTION:
*  Sets up a "synchronous" signal handler.
***********************************************************************/
int
Event_HandleSignal(EventSelector *es,
		   int sig,
		   void (*handler)(int sig))
{
    struct sigaction act;

    if (SetupPipes(es) < 0) return -1;

    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif
    if (sig == SIGCHLD) {
	act.sa_flags |= SA_NOCLDSTOP;
    }
    if (sigaction(sig, &act, NULL) < 0) return -1;

    SignalHandlers[sig].handler = handler;

    return 0;
}

/**********************************************************************
* %FUNCTION: Event_HandleChildExit
* %ARGUMENTS:
*  es -- event selector
*  pid -- process-ID of child to wait for
*  handler -- function to call when child exits
*  data -- data to pass to handler when child exits
* %RETURNS:
*  0 on success, -1 on failure.
* %DESCRIPTION:
*  Sets things up so that when a child exits, handler() will be called
*  with the pid of the child and "data" as arguments.  The call will
*  be synchronous (part of the normal event loop on es).
***********************************************************************/
int
Event_HandleChildExit(EventSelector *es,
		      pid_t pid,
		      void (*handler)(pid_t, int, void *),
		      void *data)
{
    struct ChildEntry *ce;

    if (Event_HandleSignal(es, SIGCHLD, child_handler) < 0) return -1;
    ce = malloc(sizeof(struct ChildEntry));
    if (!ce) return -1;
    ce->pid = pid;
    ce->data = data;
    ce->handler = handler;
    hash_insert(&child_process_table, ce);
    return 0;
}
