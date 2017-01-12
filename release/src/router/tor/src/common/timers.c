/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file timers.c
 * \brief Wrapper around William Ahern's fast hierarchical timer wheel
 *   implementation, to tie it in with a libevent backend.
 *
 * Only use these functions from the main thread.
 *
 * The main advantage of tor_timer_t over using libevent's timers is that
 * they're way more efficient if we need to have thousands or millions of
 * them.  For more information, see
 *   http://www.25thandclement.com/~william/projects/timeout.c.html
 *
 * Periodic timers are available in the backend, but I've turned them off.
 * We can turn them back on if needed.
 */

/* Notes:
 *
 * Having a way to free all timers on shutdown would free people from the
 * need to track them.  Not sure if that's clever though.
 *
 * In an ideal world, Libevent would just switch to use this backend, and we
 * could throw this file away.  But even if Libevent does switch, we'll be
 * stuck with legacy libevents for some time.
 */

#include "orconfig.h"

#include "compat.h"
#include "compat_libevent.h"
#include "timers.h"
#include "torlog.h"
#include "util.h"

#include <event2/event.h>

struct timeout_cb {
  timer_cb_fn_t cb;
  void *arg;
};

/*
 * These definitions are for timeouts.c  and timeouts.h.
 */
#ifdef __GNUC__
/* We're not exposing any of the functions outside this file. */
#define TIMEOUT_PUBLIC __attribute__((__unused__)) static
#else
/* We're not exposing any of the functions outside this file. */
#define TIMEOUT_PUBLIC static
#endif
/* We're not using periodic events. */
#define TIMEOUT_DISABLE_INTERVALS
/* We always know the global_timeouts object, so we don't need each timeout
 * to keep a pointer to it. */
#define TIMEOUT_DISABLE_RELATIVE_ACCESS
/* We're providing our own struct timeout_cb. */
#define TIMEOUT_CB_OVERRIDE
/* We're going to support timers that are pretty far out in advance. Making
 * this big can be inefficient, but having a significant number of timers
 * above TIMEOUT_MAX can also be super-inefficent. Choosing 5 here sets
 * timeout_max to 2^30 ticks, or 29 hours with our value for USEC_PER_TICK */
#define WHEEL_NUM 5
#include "src/ext/timeouts/timeout.c"

static struct timeouts *global_timeouts = NULL;
static struct event *global_timer_event = NULL;

static monotime_t start_of_time;

/** We need to choose this value carefully.  Because we're using timer wheels,
 * it actually costs us to have extra resolution we don't use.  So for now,
 * I'm going to define our resolution as .1 msec, and hope that's good enough.
 *
 * Note that two of the most popular libevent backends (epoll without timerfd,
 * and windows select), simply can't support sub-millisecond resolution,
 * do this is optimistic for a lot of users.
 */
#define USEC_PER_TICK 100

/** One million microseconds in a second */
#define USEC_PER_SEC 1000000

/** Check at least once every N seconds. */
#define MIN_CHECK_SECONDS 3600

/** Check at least once every N ticks. */
#define MIN_CHECK_TICKS \
  (((timeout_t)MIN_CHECK_SECONDS) * (1000000 / USEC_PER_TICK))

/**
 * Convert the timeval in <b>tv</b> to a timeout_t, and return it.
 *
 * The output resolution is set by USEC_PER_TICK. Only use this to convert
 * delays to number of ticks; the time represented by 0 is undefined.
 */
static timeout_t
tv_to_timeout(const struct timeval *tv)
{
  uint64_t usec = tv->tv_usec;
  usec += ((uint64_t)USEC_PER_SEC) * tv->tv_sec;
  return usec / USEC_PER_TICK;
}

/**
 * Convert the timeout in <b>t</b> to a timeval in <b>tv_out</b>. Only
 * use this for delays, not absolute times.
 */
static void
timeout_to_tv(timeout_t t, struct timeval *tv_out)
{
  t *= USEC_PER_TICK;
  tv_out->tv_usec = (int)(t % USEC_PER_SEC);
  tv_out->tv_sec = (time_t)(t / USEC_PER_SEC);
}

/**
 * Update the timer <b>tv</b> to the current time in <b>tv</b>.
 */
static void
timer_advance_to_cur_time(const monotime_t *now)
{
  timeout_t cur_tick = CEIL_DIV(monotime_diff_usec(&start_of_time, now),
                                USEC_PER_TICK);
  timeouts_update(global_timeouts, cur_tick);
}

/**
 * Adjust the time at which the libevent timer should fire based on
 * the next-expiring time in <b>global_timeouts</b>
 */
static void
libevent_timer_reschedule(void)
{
  monotime_t now;
  monotime_get(&now);
  timer_advance_to_cur_time(&now);

  timeout_t delay = timeouts_timeout(global_timeouts);

  struct timeval d;
  if (delay > MIN_CHECK_TICKS)
    delay = MIN_CHECK_TICKS;
  timeout_to_tv(delay, &d);
  event_add(global_timer_event, &d);
}

/**
 * Invoked when the libevent timer has expired: see which tor_timer_t events
 * have fired, activate their callbacks, and reschedule the libevent timer.
 */
static void
libevent_timer_callback(evutil_socket_t fd, short what, void *arg)
{
  (void)fd;
  (void)what;
  (void)arg;

  monotime_t now;
  monotime_get(&now);
  timer_advance_to_cur_time(&now);

  tor_timer_t *t;
  while ((t = timeouts_get(global_timeouts))) {
    t->callback.cb(t, t->callback.arg, &now);
  }

  libevent_timer_reschedule();
}

/**
 * Initialize the timers subsystem.  Requires that libevent has already been
 * initialized.
 */
void
timers_initialize(void)
{
  if (BUG(global_timeouts))
    return; // LCOV_EXCL_LINE

  timeout_error_t err;
  global_timeouts = timeouts_open(0, &err);
  if (!global_timeouts) {
    // LCOV_EXCL_START -- this can only fail on malloc failure.
    log_err(LD_BUG, "Unable to open timer backend: %s", strerror(err));
    tor_assert(0);
    // LCOV_EXCL_STOP
  }

  monotime_init();
  monotime_get(&start_of_time);

  struct event *timer_event;
  timer_event = tor_event_new(tor_libevent_get_base(),
                              -1, 0, libevent_timer_callback, NULL);
  tor_assert(timer_event);
  global_timer_event = timer_event;

  libevent_timer_reschedule();
}

/**
 * Release all storage held in the timers subsystem.  Does not fire timers.
 */
void
timers_shutdown(void)
{
  if (global_timer_event) {
    tor_event_free(global_timer_event);
    global_timer_event = NULL;
  }
  if (global_timeouts) {
    timeouts_close(global_timeouts);
    global_timeouts = NULL;
  }
}

/**
 * Allocate and return a new timer, with given callback and argument.
 */
tor_timer_t *
timer_new(timer_cb_fn_t cb, void *arg)
{
  tor_timer_t *t = tor_malloc(sizeof(tor_timer_t));
  timeout_init(t, 0);
  timer_set_cb(t, cb, arg);
  return t;
}

/**
 * Release all storage held by <b>t</b>, and unschedule it if was already
 * scheduled.
 */
void
timer_free(tor_timer_t *t)
{
  if (! t)
    return;

  timeouts_del(global_timeouts, t);
  tor_free(t);
}

/**
 * Change the callback and argument associated with a timer <b>t</b>.
 */
void
timer_set_cb(tor_timer_t *t, timer_cb_fn_t cb, void *arg)
{
  t->callback.cb = cb;
  t->callback.arg = arg;
}

/**
 * Schedule the timer t to fire at the current time plus a delay of
 * <b>delay</b> microseconds.  All times are relative to monotime_get().
 */
void
timer_schedule(tor_timer_t *t, const struct timeval *tv)
{
  const timeout_t delay = tv_to_timeout(tv);

  monotime_t now;
  monotime_get(&now);
  timer_advance_to_cur_time(&now);

  /* Take the old timeout value. */
  timeout_t to = timeouts_timeout(global_timeouts);

  timeouts_add(global_timeouts, t, delay);

  /* Should we update the libevent timer? */
  if (to <= delay) {
    return; /* we're already going to fire before this timer would trigger. */
  }
  libevent_timer_reschedule();
}

/**
 * Cancel the timer <b>t</b> if it is currently scheduled.  (It's okay to call
 * this on an unscheduled timer.
 */
void
timer_disable(tor_timer_t *t)
{
  timeouts_del(global_timeouts, t);
  /* We don't reschedule the libevent timer here, since it's okay if it fires
   * early. */
}

