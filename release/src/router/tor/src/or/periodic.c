/* Copyright (c) 2015-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file periodic.c
 *
 * \brief Generic backend for handling periodic events.
 */

#include "or.h"
#include "compat_libevent.h"
#include "config.h"
#include "periodic.h"

#ifdef HAVE_EVENT2_EVENT_H
#include <event2/event.h>
#else
#include <event.h>
#endif

/** We disable any interval greater than this number of seconds, on the
 * grounds that it is probably an absolute time mistakenly passed in as a
 * relative time.
 */
static const int MAX_INTERVAL = 10 * 365 * 86400;

/** Set the event <b>event</b> to run in <b>next_interval</b> seconds from
 * now. */
static void
periodic_event_set_interval(periodic_event_item_t *event,
                            time_t next_interval)
{
  tor_assert(next_interval < MAX_INTERVAL);
  struct timeval tv;
  tv.tv_sec = next_interval;
  tv.tv_usec = 0;
  event_add(event->ev, &tv);
}

/** Wraps dispatches for periodic events, <b>data</b> will be a pointer to the
 * event that needs to be called */
static void
periodic_event_dispatch(evutil_socket_t fd, short what, void *data)
{
  (void)fd;
  (void)what;
  periodic_event_item_t *event = data;

  time_t now = time(NULL);
  const or_options_t *options = get_options();
//  log_debug(LD_GENERAL, "Dispatching %s", event->name);
  int r = event->fn(now, options);
  int next_interval = 0;

  /* update the last run time if action was taken */
  if (r==0) {
    log_err(LD_BUG, "Invalid return value for periodic event from %s.",
                      event->name);
    tor_assert(r != 0);
  } else if (r > 0) {
    event->last_action_time = now;
    /* If the event is meant to happen after ten years, that's likely
     * a bug, and somebody gave an absolute time rather than an interval.
     */
    tor_assert(r < MAX_INTERVAL);
    next_interval = r;
  } else {
    /* no action was taken, it is likely a precondition failed,
     * we should reschedule for next second incase the precondition
     * passes then */
    next_interval = 1;
  }

//  log_debug(LD_GENERAL, "Scheduling %s for %d seconds", event->name,
//           next_interval);
  struct timeval tv = { next_interval , 0 };
  event_add(event->ev, &tv);
}

/** Schedules <b>event</b> to run as soon as possible from now. */
void
periodic_event_reschedule(periodic_event_item_t *event)
{
  periodic_event_set_interval(event, 1);
}

/** Initializes the libevent backend for a periodic event. */
void
periodic_event_setup(periodic_event_item_t *event)
{
  if (event->ev) { /* Already setup? This is a bug */
    log_err(LD_BUG, "Initial dispatch should only be done once.");
    tor_assert(0);
  }

  event->ev = tor_event_new(tor_libevent_get_base(),
                            -1, 0,
                            periodic_event_dispatch,
                            event);
  tor_assert(event->ev);
}

/** Handles initial dispatch for periodic events. It should happen 1 second
 * after the events are created to mimic behaviour before #3199's refactor */
void
periodic_event_launch(periodic_event_item_t *event)
{
  if (! event->ev) { /* Not setup? This is a bug */
    log_err(LD_BUG, "periodic_event_launch without periodic_event_setup");
    tor_assert(0);
  }

  // Initial dispatch
  periodic_event_dispatch(-1, EV_TIMEOUT, event);
}

/** Release all storage associated with <b>event</b> */
void
periodic_event_destroy(periodic_event_item_t *event)
{
  if (!event)
    return;
  tor_event_free(event->ev);
  event->last_action_time = 0;
}

