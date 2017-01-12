/* Copyright (c) 2009-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_COMPAT_LIBEVENT_H
#define TOR_COMPAT_LIBEVENT_H

#include "orconfig.h"
#include "testsupport.h"

#include <event2/event.h>

void configure_libevent_logging(void);
void suppress_libevent_log_msg(const char *msg);

#define tor_event_new     event_new
#define tor_evtimer_new   evtimer_new
#define tor_evsignal_new  evsignal_new
#define tor_evdns_add_server_port(sock, tcp, cb, data) \
  evdns_add_server_port_with_base(tor_libevent_get_base(), \
  (sock),(tcp),(cb),(data));

void tor_event_free(struct event *ev);

typedef struct periodic_timer_t periodic_timer_t;

periodic_timer_t *periodic_timer_new(struct event_base *base,
             const struct timeval *tv,
             void (*cb)(periodic_timer_t *timer, void *data),
             void *data);
void periodic_timer_free(periodic_timer_t *);

#define tor_event_base_loopexit event_base_loopexit

/** Defines a configuration for using libevent with Tor: passed as an argument
 * to tor_libevent_initialize() to describe how we want to set up. */
typedef struct tor_libevent_cfg {
  /** How many CPUs should we use (not currently useful). */
  int num_cpus;
  /** How many milliseconds should we allow between updating bandwidth limits?
   * (Not currently useful). */
  int msec_per_tick;
} tor_libevent_cfg;

void tor_libevent_initialize(tor_libevent_cfg *cfg);
MOCK_DECL(struct event_base *, tor_libevent_get_base, (void));
const char *tor_libevent_get_method(void);
void tor_check_libevent_header_compatibility(void);
const char *tor_libevent_get_version_str(void);
const char *tor_libevent_get_header_version_str(void);

int tor_init_libevent_rng(void);

void tor_gettimeofday_cached(struct timeval *tv);
void tor_gettimeofday_cache_clear(void);
#ifdef TOR_UNIT_TESTS
void tor_gettimeofday_cache_set(const struct timeval *tv);
#endif

#ifdef COMPAT_LIBEVENT_PRIVATE

/** Macro: returns the number of a Libevent version as a 4-byte number,
    with the first three bytes representing the major, minor, and patchlevel
    respectively of the library.  The fourth byte is unused.

    This is equivalent to the format of LIBEVENT_VERSION_NUMBER on Libevent
    2.0.1 or later. */
#define V(major, minor, patch) \
  (((major) << 24) | ((minor) << 16) | ((patch) << 8))

STATIC void
libevent_logging_callback(int severity, const char *msg);
#endif

#endif

