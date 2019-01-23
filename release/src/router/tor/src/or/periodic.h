/* Copyright (c) 2015-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_PERIODIC_H
#define TOR_PERIODIC_H

#define PERIODIC_EVENT_NO_UPDATE (-1)

/** Callback function for a periodic event to take action.  The return value
* influences the next time the function will get called.  Return
* PERIODIC_EVENT_NO_UPDATE to not update <b>last_action_time</b> and be polled
* again in the next second. If a positive value is returned it will update the
* interval time. */
typedef int (*periodic_event_helper_t)(time_t now,
                                      const or_options_t *options);

struct event;

/** A single item for the periodic-events-function table. */
typedef struct periodic_event_item_t {
  periodic_event_helper_t fn; /**< The function to run the event */
  time_t last_action_time; /**< The last time the function did something */
  struct event *ev; /**< Libevent callback we're using to implement this */
  const char *name; /**< Name of the function -- for debug */
} periodic_event_item_t;

/** events will get their interval from first execution */
#define PERIODIC_EVENT(fn) { fn##_callback, 0, NULL, #fn }
#define END_OF_PERIODIC_EVENTS { NULL, 0, NULL, NULL }

void periodic_event_launch(periodic_event_item_t *event);
void periodic_event_setup(periodic_event_item_t *event);
void periodic_event_destroy(periodic_event_item_t *event);
void periodic_event_reschedule(periodic_event_item_t *event);

#endif

