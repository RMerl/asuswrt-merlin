/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TIMERS_H
#define TOR_TIMERS_H

#include "orconfig.h"
#include "testsupport.h"

struct monotime_t;
typedef struct timeout tor_timer_t;
typedef void (*timer_cb_fn_t)(tor_timer_t *, void *,
                              const struct monotime_t *);
tor_timer_t *timer_new(timer_cb_fn_t cb, void *arg);
void timer_set_cb(tor_timer_t *t, timer_cb_fn_t cb, void *arg);
void timer_schedule(tor_timer_t *t, const struct timeval *delay);
void timer_disable(tor_timer_t *t);
void timer_free(tor_timer_t *t);

void timers_initialize(void);
void timers_shutdown(void);

#endif

