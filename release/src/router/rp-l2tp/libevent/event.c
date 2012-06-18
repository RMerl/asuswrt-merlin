/***********************************************************************
*
* event.c
*
* Abstraction of select call into "event-handling" to make programming
* easier.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: event.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "event.h"
#include <stdlib.h>
#include <errno.h>

static void DestroySelector(EventSelector *es);
static void DestroyHandler(EventHandler *eh);
static void DoPendingChanges(EventSelector *es);

/**********************************************************************
* %FUNCTION: Event_CreateSelector
* %ARGUMENTS:
*  None
* %RETURNS:
*  A newly-allocated EventSelector, or NULL if out of memory.
* %DESCRIPTION:
*  Creates a new EventSelector.
***********************************************************************/
EventSelector *
Event_CreateSelector(void)
{
    EventSelector *es = malloc(sizeof(EventSelector));
    if (!es) return NULL;
    es->handlers = NULL;
    es->nestLevel = 0;
    es->destroyPending = 0;
    es->opsPending = 0;
    EVENT_DEBUG(("CreateSelector() -> %p\n", (void *) es));
    return es;
}

/**********************************************************************
* %FUNCTION: Event_DestroySelector
* %ARGUMENTS:
*  es -- EventSelector to destroy
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Destroys an EventSelector.  Destruction may be delayed if we
*  are in the HandleEvent function.
***********************************************************************/
void
Event_DestroySelector(EventSelector *es)
{
    if (es->nestLevel) {
	es->destroyPending = 1;
	es->opsPending = 1;
	return;
    }
    DestroySelector(es);
}

/**********************************************************************
* %FUNCTION: Event_HandleEvent
* %ARGUMENTS:
*  es -- EventSelector
* %RETURNS:
*  0 if OK, non-zero on error.  errno is set appropriately.
* %DESCRIPTION:
*  Handles a single event (uses select() to wait for an event.)
***********************************************************************/
int
Event_HandleEvent(EventSelector *es)
{
    fd_set readfds, writefds;
    fd_set *rd, *wr;
    unsigned int flags;

    struct timeval abs_timeout, now;
    struct timeval timeout;
    struct timeval *tm;
    EventHandler *eh;

    int r = 0;
    int errno_save = 0;
    int foundTimeoutEvent = 0;
    int foundReadEvent = 0;
    int foundWriteEvent = 0;
    int maxfd = -1;
    int pastDue;

    EVENT_DEBUG(("Enter Event_HandleEvent(es=%p)\n", (void *) es));

    /* Build the select sets */
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    eh = es->handlers;
    for (eh=es->handlers; eh; eh=eh->next) {
	if (eh->flags & EVENT_FLAG_DELETED) continue;
	if (eh->flags & EVENT_FLAG_READABLE) {
	    foundReadEvent = 1;
	    FD_SET(eh->fd, &readfds);
	    if (eh->fd > maxfd) maxfd = eh->fd;
	}
	if (eh->flags & EVENT_FLAG_WRITEABLE) {
	    foundWriteEvent = 1;
	    FD_SET(eh->fd, &writefds);
	    if (eh->fd > maxfd) maxfd = eh->fd;
	}
	if (eh->flags & EVENT_TIMER_BITS) {
	    if (!foundTimeoutEvent) {
		abs_timeout = eh->tmout;
		foundTimeoutEvent = 1;
	    } else {
		if (eh->tmout.tv_sec < abs_timeout.tv_sec ||
		    (eh->tmout.tv_sec == abs_timeout.tv_sec &&
		     eh->tmout.tv_usec < abs_timeout.tv_usec)) {
		    abs_timeout = eh->tmout;
		}
	    }
	}
    }
    if (foundReadEvent) {
	rd = &readfds;
    } else {
	rd = NULL;
    }
    if (foundWriteEvent) {
	wr = &writefds;
    } else {
	wr = NULL;
    }

    if (foundTimeoutEvent) {
	gettimeofday(&now, NULL);
	/* Convert absolute timeout to relative timeout for select */
	timeout.tv_usec = abs_timeout.tv_usec - now.tv_usec;
	timeout.tv_sec = abs_timeout.tv_sec - now.tv_sec;
	if (timeout.tv_usec < 0) {
	    timeout.tv_usec += 1000000;
	    timeout.tv_sec--;
	}
	if (timeout.tv_sec < 0 ||
	    (timeout.tv_sec == 0 && timeout.tv_usec < 0)) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	}
	tm = &timeout;
    } else {
	tm = NULL;
    }

    if (foundReadEvent || foundWriteEvent || foundTimeoutEvent) {
	for(;;) {
	    r = select(maxfd+1, rd, wr, NULL, tm);
	    if (r < 0) {
		if (errno == EINTR) continue;
	    }
	    break;
	}
    }

    if (foundTimeoutEvent) gettimeofday(&now, NULL);
    errno_save = errno;
    es->nestLevel++;

    if (r >= 0) {
	/* Call handlers */
	for (eh=es->handlers; eh; eh=eh->next) {

	    /* Pending delete for this handler?  Ignore it */
	    if (eh->flags & EVENT_FLAG_DELETED) continue;

	    flags = 0;
	    if ((eh->flags & EVENT_FLAG_READABLE) &&
		FD_ISSET(eh->fd, &readfds)) {
		flags |= EVENT_FLAG_READABLE;
	    }
	    if ((eh->flags & EVENT_FLAG_WRITEABLE) &&
		FD_ISSET(eh->fd, &writefds)) {
		flags |= EVENT_FLAG_WRITEABLE;
	    }
	    if (eh->flags & EVENT_TIMER_BITS) {
		pastDue = (eh->tmout.tv_sec < now.tv_sec ||
			   (eh->tmout.tv_sec == now.tv_sec &&
			    eh->tmout.tv_usec <= now.tv_usec));
		if (pastDue) {
		    flags |= EVENT_TIMER_BITS;
		    if (eh->flags & EVENT_FLAG_TIMER) {
			/* Timer events are only called once */
			es->opsPending = 1;
			eh->flags |= EVENT_FLAG_DELETED;
		    }
		}
	    }
	    /* Do callback */
	    if (flags) {
		EVENT_DEBUG(("Enter callback: eh=%p flags=%u\n", eh, flags));
		eh->fn(es, eh->fd, flags, eh->data);
		EVENT_DEBUG(("Leave callback: eh=%p flags=%u\n", eh, flags));
	    }
	}
    }

    es->nestLevel--;

    if (!es->nestLevel && es->opsPending) {
	DoPendingChanges(es);
    }
    errno = errno_save;
    return r;
}

/**********************************************************************
* %FUNCTION: Event_AddHandler
* %ARGUMENTS:
*  es -- event selector
*  fd -- file descriptor to watch
*  flags -- combination of EVENT_FLAG_READABLE and EVENT_FLAG_WRITEABLE
*  fn -- callback function to call when event is triggered
*  data -- extra data to pass to callback function
* %RETURNS:
*  A newly-allocated EventHandler, or NULL.
***********************************************************************/
EventHandler *
Event_AddHandler(EventSelector *es,
		 int fd,
		 unsigned int flags,
		 EventCallbackFunc fn,
		 void *data)
{
    EventHandler *eh;

    /* Specifically disable timer and deleted flags */
    flags &= (~(EVENT_TIMER_BITS | EVENT_FLAG_DELETED));

    /* Bad file descriptor */
    if (fd < 0) {
	errno = EBADF;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;
    eh->fd = fd;
    eh->flags = flags;
    eh->tmout.tv_usec = 0;
    eh->tmout.tv_sec = 0;
    eh->fn = fn;
    eh->data = data;

    /* Add immediately.  This is safe even if we are in a handler. */
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddHandler(es=%p, fd=%d, flags=%u) -> %p\n", es, fd, flags, eh));
    return eh;
}

/**********************************************************************
* %FUNCTION: Event_AddHandlerWithTimeout
* %ARGUMENTS:
*  es -- event selector
*  fd -- file descriptor to watch
*  flags -- combination of EVENT_FLAG_READABLE and EVENT_FLAG_WRITEABLE
*  t -- Timeout after which to call handler, even if not readable/writable.
*       If t.tv_sec < 0, calls normal Event_AddHandler with no timeout.
*  fn -- callback function to call when event is triggered
*  data -- extra data to pass to callback function
* %RETURNS:
*  A newly-allocated EventHandler, or NULL.
***********************************************************************/
EventHandler *
Event_AddHandlerWithTimeout(EventSelector *es,
			    int fd,
			    unsigned int flags,
			    struct timeval t,
			    EventCallbackFunc fn,
			    void *data)
{
    EventHandler *eh;
    struct timeval now;

    /* If timeout is negative, just do normal non-timing-out event */
    if (t.tv_sec < 0 || t.tv_usec < 0) {
	return Event_AddHandler(es, fd, flags, fn, data);
    }

    /* Specifically disable timer and deleted flags */
    flags &= (~(EVENT_FLAG_TIMER | EVENT_FLAG_DELETED));
    flags |= EVENT_FLAG_TIMEOUT;

    /* Bad file descriptor? */
    if (fd < 0) {
	errno = EBADF;
	return NULL;
    }

    /* Bad timeout? */
    if (t.tv_usec >= 1000000) {
	errno = EINVAL;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;

    /* Convert time interval to absolute time */
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    eh->fd = fd;
    eh->flags = flags;
    eh->tmout = t;
    eh->fn = fn;
    eh->data = data;

    /* Add immediately.  This is safe even if we are in a handler. */
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddHandlerWithTimeout(es=%p, fd=%d, flags=%u, t=%d/%d) -> %p\n", es, fd, flags, t.tv_sec, t.tv_usec, eh));
    return eh;
}


/**********************************************************************
* %FUNCTION: Event_AddTimerHandler
* %ARGUMENTS:
*  es -- event selector
*  t -- time interval after which to trigger event
*  fn -- callback function to call when event is triggered
*  data -- extra data to pass to callback function
* %RETURNS:
*  A newly-allocated EventHandler, or NULL.
***********************************************************************/
EventHandler *
Event_AddTimerHandler(EventSelector *es,
		      struct timeval t,
		      EventCallbackFunc fn,
		      void *data)
{
    EventHandler *eh;
    struct timeval now;

    /* Check time interval for validity */
    if (t.tv_sec < 0 || t.tv_usec < 0 || t.tv_usec >= 1000000) {
	errno = EINVAL;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;

    /* Convert time interval to absolute time */
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    eh->fd = -1;
    eh->flags = EVENT_FLAG_TIMER;
    eh->tmout = t;
    eh->fn = fn;
    eh->data = data;

    /* Add immediately.  This is safe even if we are in a handler. */
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddTimerHandler(es=%p, t=%d/%d) -> %p\n", es, t.tv_sec,t.tv_usec, eh));
    return eh;
}

/**********************************************************************
* %FUNCTION: Event_DelHandler
* %ARGUMENTS:
*  es -- event selector
*  eh -- event handler
* %RETURNS:
*  0 if OK, non-zero if there is an error
* %DESCRIPTION:
*  Deletes the event handler eh
***********************************************************************/
int
Event_DelHandler(EventSelector *es,
		 EventHandler *eh)
{
    /* Scan the handlers list */
    EventHandler *cur, *prev;
    EVENT_DEBUG(("Event_DelHandler(es=%p, eh=%p)\n", es, eh));
    for (cur=es->handlers, prev=NULL; cur; prev=cur, cur=cur->next) {
	if (cur == eh) {
	    if (es->nestLevel) {
		eh->flags |= EVENT_FLAG_DELETED;
		es->opsPending = 1;
		return 0;
	    } else {
		if (prev) prev->next = cur->next;
		else      es->handlers = cur->next;

		DestroyHandler(cur);
		return 0;
	    }
	}
    }

    /* Handler not found */
    return 1;
}

/**********************************************************************
* %FUNCTION: DestroySelector
* %ARGUMENTS:
*  es -- an event selector
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Destroys selector and all associated handles.
***********************************************************************/
void
DestroySelector(EventSelector *es)
{
    EventHandler *cur, *next;
    for (cur=es->handlers; cur; cur=next) {
	next = cur->next;
	DestroyHandler(cur);
    }

    free(es);
}

/**********************************************************************
* %FUNCTION: DestroyHandler
* %ARGUMENTS:
*  eh -- an event handler
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Destroys handler
***********************************************************************/
void
DestroyHandler(EventHandler *eh)
{
    EVENT_DEBUG(("DestroyHandler(eh=%p)\n", eh));
    free(eh);
}

/**********************************************************************
* %FUNCTION: DoPendingChanges
* %ARGUMENTS:
*  es -- an event selector
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Makes all pending insertions and deletions happen.
***********************************************************************/
void
DoPendingChanges(EventSelector *es)
{
    EventHandler *cur, *prev, *next;

    es->opsPending = 0;

    /* If selector is to be deleted, do it and skip everything else */
    if (es->destroyPending) {
	DestroySelector(es);
	return;
    }

    /* Do deletions */
    cur = es->handlers;
    prev = NULL;
    while(cur) {
	if (!(cur->flags & EVENT_FLAG_DELETED)) {
	    prev = cur;
	    cur = cur->next;
	    continue;
	}

	/* Unlink from list */
	if (prev) {
	    prev->next = cur->next;
	} else {
	    es->handlers = cur->next;
	}
	next = cur->next;
	DestroyHandler(cur);
	cur = next;
    }
}

/**********************************************************************
* %FUNCTION: Event_GetCallback
* %ARGUMENTS:
*  eh -- the event handler
* %RETURNS:
*  The callback function
***********************************************************************/
EventCallbackFunc
Event_GetCallback(EventHandler *eh)
{
    return eh->fn;
}

/**********************************************************************
* %FUNCTION: Event_GetData
* %ARGUMENTS:
*  eh -- the event handler
* %RETURNS:
*  The "data" field.
***********************************************************************/
void *
Event_GetData(EventHandler *eh)
{
    return eh->data;
}

/**********************************************************************
* %FUNCTION: Event_SetCallbackAndData
* %ARGUMENTS:
*  eh -- the event handler
*  fn -- new callback function
*  data -- new data value
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sets the callback function and data fields.
***********************************************************************/
void
Event_SetCallbackAndData(EventHandler *eh,
			 EventCallbackFunc fn,
			 void *data)
{
    eh->fn = fn;
    eh->data = data;
}

#ifdef DEBUG_EVENT
#include <stdarg.h>
#include <stdio.h>
FILE *Event_DebugFP = NULL;
/**********************************************************************
* %FUNCTION: Event_DebugMsg
* %ARGUMENTS:
*  fmt, ... -- format string
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Writes a debug message to the debug file.
***********************************************************************/
void
Event_DebugMsg(char const *fmt, ...)
{
    va_list ap;
    struct timeval now;

    if (!Event_DebugFP) return;

    gettimeofday(&now, NULL);

    fprintf(Event_DebugFP, "%03d.%03d ", (int) now.tv_sec % 1000,
	    (int) now.tv_usec / 1000);

    va_start(ap, fmt);
    vfprintf(Event_DebugFP, fmt, ap);
    va_end(ap);
    fflush(Event_DebugFP);
}

#endif

/**********************************************************************
* %FUNCTION: Event_EnableDebugging
* %ARGUMENTS:
*  fname -- name of file to log debug messages to
* %RETURNS:
*  1 if debugging was enabled; 0 otherwise.
***********************************************************************/
int
Event_EnableDebugging(char const *fname)
{
#ifndef DEBUG_EVENT
    return 0;
#else
    Event_DebugFP = fopen(fname, "w");
    return (Event_DebugFP != NULL);
#endif
}

/**********************************************************************
* %FUNCTION: Event_ChangeTimeout
* %ARGUMENTS:
*  h -- event handler
*  t -- new timeout
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Changes timeout of event handler to be "t" seconds in the future.
***********************************************************************/
void
Event_ChangeTimeout(EventHandler *h, struct timeval t)
{
    struct timeval now;

    /* Check time interval for validity */
    if (t.tv_sec < 0 || t.tv_usec < 0 || t.tv_usec >= 1000000) {
	return;
    }
    /* Convert time interval to absolute time */
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    h->tmout = t;
}
