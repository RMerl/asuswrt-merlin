/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <zebra.h>
#include "log.h"
#include "thread.h"

#include "pim_time.h"

static int gettime_monotonic(struct timeval *tv)
{
  int result;

  result = gettimeofday(tv, 0);
  if (result) {
    zlog_err("%s: gettimeofday() failure: errno=%d: %s",
	     __PRETTY_FUNCTION__,
	     errno, safe_strerror(errno));
  }

  return result;
}

/*
  pim_time_monotonic_sec():
  number of seconds since some unspecified starting point
*/
int64_t pim_time_monotonic_sec()
{
  struct timeval now_tv;

  if (gettime_monotonic(&now_tv)) {
    zlog_err("%s: gettime_monotonic() failure: errno=%d: %s",
	     __PRETTY_FUNCTION__,
	     errno, safe_strerror(errno));
    return -1;
  }

  return now_tv.tv_sec;
}

/*
  pim_time_monotonic_dsec():
  number of deciseconds since some unspecified starting point
*/
int64_t pim_time_monotonic_dsec()
{
  struct timeval now_tv;
  int64_t        now_dsec;

  if (gettime_monotonic(&now_tv)) {
    zlog_err("%s: gettime_monotonic() failure: errno=%d: %s",
	     __PRETTY_FUNCTION__,
	     errno, safe_strerror(errno));
    return -1;
  }

  now_dsec = ((int64_t) now_tv.tv_sec) * 10 + ((int64_t) now_tv.tv_usec) / 100000;

  return now_dsec;
}

int pim_time_mmss(char *buf, int buf_size, long sec)
{
  long mm;
  int wr;

  zassert(buf_size >= 5);

  mm = sec / 60;
  sec %= 60;
  
  wr = snprintf(buf, buf_size, "%02ld:%02ld", mm, sec);

  return wr != 8;
}

static int pim_time_hhmmss(char *buf, int buf_size, long sec)
{
  long hh;
  long mm;
  int wr;

  zassert(buf_size >= 8);

  hh = sec / 3600;
  sec %= 3600;
  mm = sec / 60;
  sec %= 60;
  
  wr = snprintf(buf, buf_size, "%02ld:%02ld:%02ld", hh, mm, sec);

  return wr != 8;
}

void pim_time_timer_to_mmss(char *buf, int buf_size, struct thread *t_timer)
{
  if (t_timer) {
    pim_time_mmss(buf, buf_size,
		  thread_timer_remain_second(t_timer));
  }
  else {
    snprintf(buf, buf_size, "--:--");
  }
}

void pim_time_timer_to_hhmmss(char *buf, int buf_size, struct thread *t_timer)
{
  if (t_timer) {
    pim_time_hhmmss(buf, buf_size,
		    thread_timer_remain_second(t_timer));
  }
  else {
    snprintf(buf, buf_size, "--:--:--");
  }
}

void pim_time_uptime(char *buf, int buf_size, int64_t uptime_sec)
{
  zassert(buf_size >= 8);

  pim_time_hhmmss(buf, buf_size, uptime_sec);
}

void pim_time_uptime_begin(char *buf, int buf_size, int64_t now, int64_t begin)
{
  if (begin > 0)
    pim_time_uptime(buf, buf_size, now - begin);
  else
    snprintf(buf, buf_size, "--:--:--");
}

long pim_time_timer_remain_msec(struct thread *t_timer)
{
  /* FIXME: Actually fetch msec resolution from thread */

  /* no timer thread running means timer has expired: return 0 */

  return t_timer ?
    1000 * thread_timer_remain_second(t_timer) :
    0;
}
