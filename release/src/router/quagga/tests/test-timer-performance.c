/*
 * Test program which measures the time it takes to schedule and
 * remove timers.
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

#include "thread.h"
#include "pqueue.h"
#include "prng.h"

#define SCHEDULE_TIMERS 1000000
#define REMOVE_TIMERS    500000

struct thread_master *master;

static int dummy_func(struct thread *thread)
{
  return 0;
}

int main(int argc, char **argv)
{
  struct prng *prng;
  int i;
  struct thread **timers;
  struct timeval tv_start, tv_lap, tv_stop;
  unsigned long t_schedule, t_remove;

  master = thread_master_create();
  prng = prng_new(0);
  timers = calloc(SCHEDULE_TIMERS, sizeof(*timers));

  /* create thread structures so they won't be allocated during the
   * time measurement */
  for (i = 0; i < SCHEDULE_TIMERS; i++)
    timers[i] = thread_add_timer_msec(master, dummy_func, NULL, 0);
  for (i = 0; i < SCHEDULE_TIMERS; i++)
    thread_cancel(timers[i]);

  quagga_gettime(QUAGGA_CLK_MONOTONIC, &tv_start);

  for (i = 0; i < SCHEDULE_TIMERS; i++)
    {
      long interval_msec;

      interval_msec = prng_rand(prng) % (100 * SCHEDULE_TIMERS);
      timers[i] = thread_add_timer_msec(master, dummy_func,
                                        NULL, interval_msec);
    }

  quagga_gettime(QUAGGA_CLK_MONOTONIC, &tv_lap);

  for (i = 0; i < REMOVE_TIMERS; i++)
    {
      int index;

      index = prng_rand(prng) % SCHEDULE_TIMERS;
      if (timers[index])
        thread_cancel(timers[index]);
      timers[index] = NULL;
    }

  quagga_gettime(QUAGGA_CLK_MONOTONIC, &tv_stop);

  t_schedule = 1000 * (tv_lap.tv_sec - tv_start.tv_sec);
  t_schedule += (tv_lap.tv_usec - tv_start.tv_usec) / 1000;

  t_remove = 1000 * (tv_stop.tv_sec - tv_lap.tv_sec);
  t_remove += (tv_stop.tv_usec - tv_lap.tv_usec) / 1000;

  printf("Scheduling %d random timers took %ld.%03ld seconds.\n",
         SCHEDULE_TIMERS, t_schedule/1000, t_schedule%1000);
  printf("Removing %d random timers took %ld.%03ld seconds.\n",
         REMOVE_TIMERS, t_remove/1000, t_remove%1000);
  fflush(stdout);

  free(timers);
  thread_master_free(master);
  prng_free(prng);
  return 0;
}
