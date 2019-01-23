/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat_time.h
 *
 * \brief Functions and types for monotonic times.
 *
 * monotime_* functions try to provide a high-resolution monotonic timer with
 * something the best resolution the system provides.  monotime_coarse_*
 * functions run faster (if the operating system gives us a way to do that)
 * but produce a less accurate timer: accuracy will probably be on the order
 * of tens of milliseconds.
 */

#ifndef TOR_COMPAT_TIME_H
#define TOR_COMPAT_TIME_H

#include "orconfig.h"
#ifdef _WIN32
#undef HAVE_CLOCK_GETTIME
#endif

#if defined(HAVE_CLOCK_GETTIME)
/* to ensure definition of CLOCK_MONOTONIC_COARSE if it's there */
#include <time.h>
#endif

#if !defined(HAVE_GETTIMEOFDAY) && !defined(HAVE_STRUCT_TIMEVAL_TV_SEC)
/** Implementation of timeval for platforms that don't have it. */
struct timeval {
  time_t tv_sec;
  unsigned int tv_usec;
};
#endif

/** Represents a monotonic timer in a platform-dependent way. */
typedef struct monotime_t {
#ifdef __APPLE__
  /* On apple, there is a 64-bit counter whose precision we must look up. */
  uint64_t abstime_;
#elif defined(HAVE_CLOCK_GETTIME)
  /* It sure would be nice to use clock_gettime(). Posix is a nice thing. */
  struct timespec ts_;
#elif defined (_WIN32)
  /* On Windows, there is a 64-bit counter whose precision we must look up. */
  int64_t pcount_;
#else
#define MONOTIME_USING_GETTIMEOFDAY
  /* Otherwise, we will be stuck using gettimeofday. */
  struct timeval tv_;
#endif
} monotime_t;

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC_COARSE)
#define MONOTIME_COARSE_FN_IS_DIFFERENT
#define monotime_coarse_t monotime_t
#elif defined(_WIN32)
#define MONOTIME_COARSE_FN_IS_DIFFERENT
#define MONOTIME_COARSE_TYPE_IS_DIFFERENT
/** Represents a coarse monotonic time in a platform-independent way. */
typedef struct monotime_coarse_t {
  uint64_t tick_count_;
} monotime_coarse_t;
#else
#define monotime_coarse_t monotime_t
#endif

/**
 * Initialize the timing subsystem. This function is idempotent.
 */
void monotime_init(void);
/**
 * Set <b>out</b> to the current time.
 */
void monotime_get(monotime_t *out);
/**
 * Return the number of nanoseconds between <b>start</b> and <b>end</b>.
 */
int64_t monotime_diff_nsec(const monotime_t *start, const monotime_t *end);
/**
 * Return the number of microseconds between <b>start</b> and <b>end</b>.
 */
int64_t monotime_diff_usec(const monotime_t *start, const monotime_t *end);
/**
 * Return the number of milliseconds between <b>start</b> and <b>end</b>.
 */
int64_t monotime_diff_msec(const monotime_t *start, const monotime_t *end);
/**
 * Return the number of nanoseconds since the timer system was initialized.
 */
uint64_t monotime_absolute_nsec(void);
/**
 * Return the number of microseconds since the timer system was initialized.
 */
uint64_t monotime_absolute_usec(void);
/**
 * Return the number of milliseconds since the timer system was initialized.
 */
uint64_t monotime_absolute_msec(void);

#if defined(MONOTIME_COARSE_FN_IS_DIFFERENT)
/**
 * Set <b>out</b> to the current coarse time.
 */
void monotime_coarse_get(monotime_coarse_t *out);
uint64_t monotime_coarse_absolute_nsec(void);
uint64_t monotime_coarse_absolute_usec(void);
uint64_t monotime_coarse_absolute_msec(void);
#else
#define monotime_coarse_get monotime_get
#define monotime_coarse_absolute_nsec monotime_absolute_nsec
#define monotime_coarse_absolute_usec monotime_absolute_usec
#define monotime_coarse_absolute_msec monotime_absolute_msec
#endif

#if defined(MONOTIME_COARSE_TYPE_IS_DIFFERENT)
int64_t monotime_coarse_diff_nsec(const monotime_coarse_t *start,
    const monotime_coarse_t *end);
int64_t monotime_coarse_diff_usec(const monotime_coarse_t *start,
    const monotime_coarse_t *end);
int64_t monotime_coarse_diff_msec(const monotime_coarse_t *start,
    const monotime_coarse_t *end);
#else
#define monotime_coarse_diff_nsec monotime_diff_nsec
#define monotime_coarse_diff_usec monotime_diff_usec
#define monotime_coarse_diff_msec monotime_diff_msec
#endif

void tor_gettimeofday(struct timeval *timeval);

#ifdef TOR_UNIT_TESTS
void tor_sleep_msec(int msec);

void monotime_enable_test_mocking(void);
void monotime_disable_test_mocking(void);
void monotime_set_mock_time_nsec(int64_t);
#if defined(MONOTIME_COARSE_FN_IS_DIFFERENT)
void monotime_coarse_set_mock_time_nsec(int64_t);
#else
#define monotime_coarse_set_mock_time_nsec monotime_set_mock_time_nsec
#endif
#endif

#ifdef COMPAT_TIME_PRIVATE
#if defined(_WIN32) || defined(TOR_UNIT_TESTS)
STATIC int64_t ratchet_performance_counter(int64_t count_raw);
STATIC int64_t ratchet_coarse_performance_counter(int64_t count_raw);
#endif
#if defined(MONOTIME_USING_GETTIMEOFDAY) || defined(TOR_UNIT_TESTS)
STATIC void ratchet_timeval(const struct timeval *timeval_raw,
                            struct timeval *out);
#endif
#ifdef TOR_UNIT_TESTS
void monotime_reset_ratchets_for_testing(void);
#endif
#endif

#endif

