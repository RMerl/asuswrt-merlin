/* Copyright (c) 2009-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat_libevent.c
 * \brief Wrappers and utility functions for Libevent.
 */

#include "orconfig.h"
#include "compat.h"
#define COMPAT_LIBEVENT_PRIVATE
#include "compat_libevent.h"

#include "crypto.h"

#include "util.h"
#include "torlog.h"

#include <event2/event.h>
#include <event2/thread.h>

/** A string which, if it appears in a libevent log, should be ignored. */
static const char *suppress_msg = NULL;
/** Callback function passed to event_set_log() so we can intercept
 * log messages from libevent. */
STATIC void
libevent_logging_callback(int severity, const char *msg)
{
  char buf[1024];
  size_t n;
  if (suppress_msg && strstr(msg, suppress_msg))
    return;
  n = strlcpy(buf, msg, sizeof(buf));
  if (n && n < sizeof(buf) && buf[n-1] == '\n') {
    buf[n-1] = '\0';
  }
  switch (severity) {
    case _EVENT_LOG_DEBUG:
      log_debug(LD_NOCB|LD_NET, "Message from libevent: %s", buf);
      break;
    case _EVENT_LOG_MSG:
      log_info(LD_NOCB|LD_NET, "Message from libevent: %s", buf);
      break;
    case _EVENT_LOG_WARN:
      log_warn(LD_NOCB|LD_GENERAL, "Warning from libevent: %s", buf);
      break;
    case _EVENT_LOG_ERR:
      log_err(LD_NOCB|LD_GENERAL, "Error from libevent: %s", buf);
      break;
    default:
      log_warn(LD_NOCB|LD_GENERAL, "Message [%d] from libevent: %s",
          severity, buf);
      break;
  }
}
/** Set hook to intercept log messages from libevent. */
void
configure_libevent_logging(void)
{
  event_set_log_callback(libevent_logging_callback);
}

/** Ignore any libevent log message that contains <b>msg</b>. */
void
suppress_libevent_log_msg(const char *msg)
{
  suppress_msg = msg;
}

/* Wrapper for event_free() that tolerates tor_event_free(NULL) */
void
tor_event_free(struct event *ev)
{
  if (ev == NULL)
    return;
  event_free(ev);
}

/** Global event base for use by the main thread. */
static struct event_base *the_event_base = NULL;

/* This is what passes for version detection on OSX.  We set
 * MACOSX_KQUEUE_IS_BROKEN to true iff we're on a version of OSX before
 * 10.4.0 (aka 1040). */
#ifdef __APPLE__
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#define MACOSX_KQUEUE_IS_BROKEN \
  (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1040)
#else
#define MACOSX_KQUEUE_IS_BROKEN 0
#endif
#endif

/** Initialize the Libevent library and set up the event base. */
void
tor_libevent_initialize(tor_libevent_cfg *torcfg)
{
  tor_assert(the_event_base == NULL);
  /* some paths below don't use torcfg, so avoid unused variable warnings */
  (void)torcfg;

  {
    int attempts = 0;
    struct event_config *cfg;

    ++attempts;
    cfg = event_config_new();
    tor_assert(cfg);

    /* Telling Libevent not to try to turn locking on can avoid a needless
     * socketpair() attempt. */
    event_config_set_flag(cfg, EVENT_BASE_FLAG_NOLOCK);

    if (torcfg->num_cpus > 0)
      event_config_set_num_cpus_hint(cfg, torcfg->num_cpus);

    /* We can enable changelist support with epoll, since we don't give
     * Libevent any dup'd fds.  This lets us avoid some syscalls. */
    event_config_set_flag(cfg, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);

    the_event_base = event_base_new_with_config(cfg);

    event_config_free(cfg);
  }

  if (!the_event_base) {
    /* LCOV_EXCL_START */
    log_err(LD_GENERAL, "Unable to initialize Libevent: cannot continue.");
    exit(1);
    /* LCOV_EXCL_STOP */
  }

  log_info(LD_GENERAL,
      "Initialized libevent version %s using method %s. Good.",
      event_get_version(), tor_libevent_get_method());
}

/** Return the current Libevent event base that we're set up to use. */
MOCK_IMPL(struct event_base *,
tor_libevent_get_base, (void))
{
  tor_assert(the_event_base != NULL);
  return the_event_base;
}

/** Return the name of the Libevent backend we're using. */
const char *
tor_libevent_get_method(void)
{
  return event_base_get_method(the_event_base);
}

/** Return a string representation of the version of the currently running
 * version of Libevent. */
const char *
tor_libevent_get_version_str(void)
{
  return event_get_version();
}

/** Return a string representation of the version of Libevent that was used
* at compilation time. */
const char *
tor_libevent_get_header_version_str(void)
{
  return LIBEVENT_VERSION;
}

/** Represents a timer that's run every N microseconds by Libevent. */
struct periodic_timer_t {
  /** Underlying event used to implement this periodic event. */
  struct event *ev;
  /** The callback we'll be invoking whenever the event triggers */
  void (*cb)(struct periodic_timer_t *, void *);
  /** User-supplied data for the callback */
  void *data;
};

/** Libevent callback to implement a periodic event. */
static void
periodic_timer_cb(evutil_socket_t fd, short what, void *arg)
{
  periodic_timer_t *timer = arg;
  (void) what;
  (void) fd;
  timer->cb(timer, timer->data);
}

/** Create and schedule a new timer that will run every <b>tv</b> in
 * the event loop of <b>base</b>.  When the timer fires, it will
 * run the timer in <b>cb</b> with the user-supplied data in <b>data</b>. */
periodic_timer_t *
periodic_timer_new(struct event_base *base,
                   const struct timeval *tv,
                   void (*cb)(periodic_timer_t *timer, void *data),
                   void *data)
{
  periodic_timer_t *timer;
  tor_assert(base);
  tor_assert(tv);
  tor_assert(cb);
  timer = tor_malloc_zero(sizeof(periodic_timer_t));
  if (!(timer->ev = tor_event_new(base, -1, EV_PERSIST,
                                  periodic_timer_cb, timer))) {
    tor_free(timer);
    return NULL;
  }
  timer->cb = cb;
  timer->data = data;
  event_add(timer->ev, (struct timeval *)tv); /*drop const for old libevent*/
  return timer;
}

/** Stop and free a periodic timer */
void
periodic_timer_free(periodic_timer_t *timer)
{
  if (!timer)
    return;
  tor_event_free(timer->ev);
  tor_free(timer);
}

int
tor_init_libevent_rng(void)
{
  int rv = 0;
  char buf[256];
  if (evutil_secure_rng_init() < 0) {
    rv = -1;
  }
  crypto_rand(buf, 32);
#ifdef HAVE_EVUTIL_SECURE_RNG_ADD_BYTES
  evutil_secure_rng_add_bytes(buf, 32);
#endif
  evutil_secure_rng_get_bytes(buf, sizeof(buf));
  return rv;
}

#if defined(LIBEVENT_VERSION_NUMBER) && LIBEVENT_VERSION_NUMBER >= V(2,1,1) \
  && !defined(TOR_UNIT_TESTS)
void
tor_gettimeofday_cached(struct timeval *tv)
{
  event_base_gettimeofday_cached(the_event_base, tv);
}
void
tor_gettimeofday_cache_clear(void)
{
  event_base_update_cache_time(the_event_base);
}
#else
/** Cache the current hi-res time; the cache gets reset when libevent
 * calls us. */
static struct timeval cached_time_hires = {0, 0};

/** Return a fairly recent view of the current time. */
void
tor_gettimeofday_cached(struct timeval *tv)
{
  if (cached_time_hires.tv_sec == 0) {
    tor_gettimeofday(&cached_time_hires);
  }
  *tv = cached_time_hires;
}

/** Reset the cached view of the current time, so that the next time we try
 * to learn it, we will get an up-to-date value. */
void
tor_gettimeofday_cache_clear(void)
{
  cached_time_hires.tv_sec = 0;
}

#ifdef TOR_UNIT_TESTS
/** For testing: force-update the cached time to a given value. */
void
tor_gettimeofday_cache_set(const struct timeval *tv)
{
  tor_assert(tv);
  memcpy(&cached_time_hires, tv, sizeof(*tv));
}
#endif
#endif

