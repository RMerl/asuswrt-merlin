/***********************************************************************
*
* event.h
*
* Abstraction of select call into "event-handling" to make programming
* easier.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* $Id: event.h 3323 2011-09-21 18:45:48Z lly.dev $
*
* LIC: GPL
*
***********************************************************************/

#define DEBUG_EVENT

#ifndef INCLUDE_EVENT_H
#define INCLUDE_EVENT_H 1

/* Solaris moans if we don't do this... */
#ifdef __sun
#define __EXTENSIONS__ 1
#endif

struct EventSelector_t;

/* Callback function */
typedef void (*EventCallbackFunc)(struct EventSelector_t *es,
				 int fd, unsigned int flags,
				 void *data);

#include "eventpriv.h"

/* Create an event selector */
extern EventSelector *Event_CreateSelector(void);

/* Destroy the event selector */
extern void Event_DestroySelector(EventSelector *es);

/* Handle one event */
extern int Event_HandleEvent(EventSelector *es);

/* Add a handler for a ready file descriptor */
extern EventHandler *Event_AddHandler(EventSelector *es,
				      int fd,
				      unsigned int flags,
				      EventCallbackFunc fn, void *data);

/* Add a handler for a ready file descriptor with associated timeout*/
extern EventHandler *Event_AddHandlerWithTimeout(EventSelector *es,
						 int fd,
						 unsigned int flags,
						 struct timeval t,
						 EventCallbackFunc fn,
						 void *data);


/* Add a timer handler */
extern EventHandler *Event_AddTimerHandler(EventSelector *es,
					   struct timeval t,
					   EventCallbackFunc fn,
					   void *data);

/* Change the timeout of a timer handler */
void Event_ChangeTimeout(EventHandler *handler, struct timeval t);

/* Delete a handler */
extern int Event_DelHandler(EventSelector *es,
			    EventHandler *eh);

/* Retrieve callback function from a handler */
extern EventCallbackFunc Event_GetCallback(EventHandler *eh);

/* Retrieve data field from a handler */
extern void *Event_GetData(EventHandler *eh);

/* Set callback and data to new values */
extern void Event_SetCallbackAndData(EventHandler *eh,
				     EventCallbackFunc fn,
				     void *data);

/* Handle a signal synchronously in event loop */
int Event_HandleSignal(EventSelector *es, int sig, void (*handler)(int sig));

/* Reap children synchronously in event loop */
int Event_HandleChildExit(EventSelector *es, pid_t pid,
			  void (*handler)(pid_t, int, void *), void *data);

extern int Event_EnableDebugging(char const *fname);

#ifdef DEBUG_EVENT
extern void Event_DebugMsg(char const *fmt, ...);
#define EVENT_DEBUG(x) Event_DebugMsg x
#else
#define EVENT_DEBUG(x) ((void) 0)
#endif

/* Flags */
#define EVENT_FLAG_READABLE 1
#define EVENT_FLAG_WRITEABLE 2
#define EVENT_FLAG_WRITABLE EVENT_FLAG_WRITEABLE

/* This is strictly a timer event */
#define EVENT_FLAG_TIMER 4

/* This is a read or write event with an associated timeout */
#define EVENT_FLAG_TIMEOUT 8

#define EVENT_TIMER_BITS (EVENT_FLAG_TIMER | EVENT_FLAG_TIMEOUT)
#endif
