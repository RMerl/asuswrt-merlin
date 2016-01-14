/*
 * Test program to verify that scheduled timers are executed in the
 * correct order.
 *
 * Copyright (C) 2013 by Open Source Routing.
 * Copyright (C) 2013 by Internet Systems Consortium, Inc. ("ISC")
 *
 * This file is part of Quagga
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>

#include <zebra.h>

#include "memory.h"
#include "pqueue.h"
#include "prng.h"
#include "thread.h"

#define SCHEDULE_TIMERS 800
#define REMOVE_TIMERS   200

#define TIMESTR_LEN strlen("4294967296.999999")

struct thread_master *master;

static size_t log_buf_len;
static size_t log_buf_pos;
static char *log_buf;

static size_t expected_buf_len;
static size_t expected_buf_pos;
static char *expected_buf;

static struct prng *prng;

static struct thread **timers;

static int timers_pending;

static void terminate_test(void)
{
  int exit_code;

  if (strcmp(log_buf, expected_buf))
    {
      fprintf(stderr, "Expected output and received output differ.\n");
      fprintf(stderr, "---Expected output: ---\n%s", expected_buf);
      fprintf(stderr, "---Actual output: ---\n%s", log_buf);
      exit_code = 1;
    }
  else
    {
      printf("Expected output and actual output match.\n");
      exit_code = 0;
    }

  thread_master_free(master);
  XFREE(MTYPE_TMP, log_buf);
  XFREE(MTYPE_TMP, expected_buf);
  prng_free(prng);
  XFREE(MTYPE_TMP, timers);

  exit(exit_code);
}

static int timer_func(struct thread *thread)
{
  int rv;

  rv = snprintf(log_buf + log_buf_pos, log_buf_len - log_buf_pos,
                "%s\n", (char*)thread->arg);
  assert(rv >= 0);
  log_buf_pos += rv;
  assert(log_buf_pos < log_buf_len);
  XFREE(MTYPE_TMP, thread->arg);

  timers_pending--;
  if (!timers_pending)
    terminate_test();

  return 0;
}

static int cmp_timeval(const void* a, const void *b)
{
  const struct timeval *ta = *(struct timeval * const *)a;
  const struct timeval *tb = *(struct timeval * const *)b;

  if (timercmp(ta, tb, <))
    return -1;
  if (timercmp(ta, tb, >))
    return 1;
  return 0;
}

int main(int argc, char **argv)
{
  int i, j;
  struct thread t;
  struct timeval **alarms;

  master = thread_master_create();

  log_buf_len = SCHEDULE_TIMERS * (TIMESTR_LEN + 1) + 1;
  log_buf_pos = 0;
  log_buf = XMALLOC(MTYPE_TMP, log_buf_len);

  expected_buf_len = SCHEDULE_TIMERS * (TIMESTR_LEN + 1) + 1;
  expected_buf_pos = 0;
  expected_buf = XMALLOC(MTYPE_TMP, expected_buf_len);

  prng = prng_new(0);

  timers = XMALLOC(MTYPE_TMP, SCHEDULE_TIMERS * sizeof(*timers));

  for (i = 0; i < SCHEDULE_TIMERS; i++)
    {
      long interval_msec;
      int ret;
      char *arg;

      /* Schedule timers to expire in 0..5 seconds */
      interval_msec = prng_rand(prng) % 5000;
      arg = XMALLOC(MTYPE_TMP, TIMESTR_LEN + 1);
      timers[i] = thread_add_timer_msec(master, timer_func, arg, interval_msec);
      ret = snprintf(arg, TIMESTR_LEN + 1, "%ld.%06ld",
                     timers[i]->u.sands.tv_sec, timers[i]->u.sands.tv_usec);
      assert(ret > 0);
      assert((size_t)ret < TIMESTR_LEN + 1);
      timers_pending++;
    }

  for (i = 0; i < REMOVE_TIMERS; i++)
    {
      int index;

      index = prng_rand(prng) % SCHEDULE_TIMERS;
      if (!timers[index])
        continue;

      XFREE(MTYPE_TMP, timers[index]->arg);
      thread_cancel(timers[index]);
      timers[index] = NULL;
      timers_pending--;
    }

  /* We create an array of pointers to the alarm times and sort
   * that array. That sorted array is used to generate a string
   * representing the expected "output" of the timers when they
   * are run. */
  j = 0;
  alarms = XMALLOC(MTYPE_TMP, timers_pending * sizeof(*alarms));
  for (i = 0; i < SCHEDULE_TIMERS; i++)
    {
      if (!timers[i])
        continue;
      alarms[j++] = &timers[i]->u.sands;
    }
  qsort(alarms, j, sizeof(*alarms), cmp_timeval);
  for (i = 0; i < j; i++)
    {
      int ret;

      ret = snprintf(expected_buf + expected_buf_pos,
                     expected_buf_len - expected_buf_pos,
                     "%ld.%06ld\n", alarms[i]->tv_sec, alarms[i]->tv_usec);
      assert(ret > 0);
      expected_buf_pos += ret;
      assert(expected_buf_pos < expected_buf_len);
    }
  XFREE(MTYPE_TMP, alarms);

  while (thread_fetch(master, &t))
    thread_call(&t);

  return 0;
}
