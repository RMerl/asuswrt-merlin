/* Copyright 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <event2/event.h>

#include "compat.h"
#include "compat_libevent.h"
#include "crypto.h"
#include "timers.h"
#include "util.h"

#define N_TIMERS 1000
#define MAX_DURATION 30
#define N_DISABLE 5

static struct timeval fire_at[N_TIMERS] = { {0,0} };
static int is_disabled[N_TIMERS] = {0};
static int fired[N_TIMERS] = {0};
static struct timeval difference[N_TIMERS] = { {0,0} };
static tor_timer_t *timers[N_TIMERS] = {NULL};

static int n_active_timers = 0;
static int n_fired = 0;

static monotime_t started_at;
static int64_t delay_usec[N_TIMERS];
static int64_t diffs_mono_usec[N_TIMERS];

static void
timer_cb(tor_timer_t *t, void *arg, const monotime_t *now_mono)
{
  struct timeval now;

  tor_gettimeofday(&now);
  tor_timer_t **t_ptr = arg;
  tor_assert(*t_ptr == t);
  int idx = (int) (t_ptr - timers);
  ++fired[idx];
  timersub(&now, &fire_at[idx], &difference[idx]);
  diffs_mono_usec[idx] =
    monotime_diff_usec(&started_at, now_mono) -
    delay_usec[idx];
  ++n_fired;

  // printf("%d / %d\n",n_fired, N_TIMERS);
  if (n_fired == n_active_timers) {
    event_base_loopbreak(tor_libevent_get_base());
  }
}

int
main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  tor_libevent_cfg cfg;
  memset(&cfg, 0, sizeof(cfg));
  tor_libevent_initialize(&cfg);
  timers_initialize();

  int i;
  int ret;
  struct timeval now;
  tor_gettimeofday(&now);
  monotime_get(&started_at);
  for (i = 0; i < N_TIMERS; ++i) {
    struct timeval delay;
    delay.tv_sec = crypto_rand_int_range(0,MAX_DURATION);
    delay.tv_usec = crypto_rand_int_range(0,1000000);
    delay_usec[i] = delay.tv_sec * 1000000 + delay.tv_usec;
    timeradd(&now, &delay, &fire_at[i]);
    timers[i] = timer_new(timer_cb, &timers[i]);
    timer_schedule(timers[i], &delay);
    ++n_active_timers;
  }

  /* Disable some; we'll make sure they don't trigger. */
  for (i = 0; i < N_DISABLE; ++i) {
    int idx = crypto_rand_int_range(0, N_TIMERS);
    if (is_disabled[idx])
      continue;
    is_disabled[idx] = 1;
    timer_disable(timers[idx]);
    --n_active_timers;
  }

  event_base_loop(tor_libevent_get_base(), 0);

  int64_t total_difference = 0;
  uint64_t total_square_difference = 0;
  tor_assert(n_fired == n_active_timers);
  for (i = 0; i < N_TIMERS; ++i) {
    if (is_disabled[i]) {
      tor_assert(fired[i] == 0);
      continue;
    }
    tor_assert(fired[i] == 1);
    //int64_t diff = difference[i].tv_usec + difference[i].tv_sec * 1000000;
    int64_t diff = diffs_mono_usec[i];
    total_difference += diff;
    total_square_difference += diff*diff;
  }
  const int64_t mean_diff = total_difference / n_active_timers;
  printf("mean difference: "I64_FORMAT" usec\n",
         I64_PRINTF_ARG(mean_diff));

  const double mean_sq = ((double)total_square_difference)/ n_active_timers;
  const double sq_mean = mean_diff * mean_diff;
  const double stddev = sqrt(mean_sq - sq_mean);
  printf("standard deviation: %lf usec\n", stddev);

#define MAX_DIFF_USEC (500*1000)
#define MAX_STDDEV_USEC (500*1000)
#define ODD_DIFF_USEC (2000)
#define ODD_STDDEV_USEC (2000)

  if (mean_diff < 0 || mean_diff > MAX_DIFF_USEC || stddev > MAX_STDDEV_USEC) {
    printf("Either your system is under ridiculous load, or the "
           "timer backend is broken.\n");
    ret = 1;
  } else if (mean_diff > ODD_DIFF_USEC || stddev > ODD_STDDEV_USEC) {
    printf("Either your system is a bit slow or the "
           "timer backend is odd.\n");
    ret = 0;
  } else {
    printf("Looks good enough.\n");
    ret = 0;
  }

  timer_free(NULL);

  for (i = 0; i < N_TIMERS; ++i) {
    timer_free(timers[i]);
  }
  timers_shutdown();
  return ret;
}

