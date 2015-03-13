/* Portable timers.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

/* This file implements "portable timers" (ptimers), objects that
   measure elapsed time using the primitives most appropriate for the
   underlying operating system.  The entry points are:

     ptimer_new     -- creates a timer.
     ptimer_reset   -- resets the timer's elapsed time to zero.
     ptimer_measure -- measure and return the time elapsed since
                       creation or last reset.
     ptimer_read    -- reads the last measured elapsed value.
     ptimer_destroy -- destroy the timer.
     ptimer_granularity -- returns the approximate granularity of the timers.

   Timers measure time in seconds, returning the timings as floating
   point numbers, so they can carry as much precision as the
   underlying system timer supports.  For example, to measure the time
   it takes to run a loop, you can use something like:

     ptimer *tmr = ptimer_new ();
     while (...)
       ... loop ...
     double secs = ptimer_measure ();
     printf ("The loop took %.2fs\n", secs);  */

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

/* Cygwin currently (as of 2005-04-08, Cygwin 1.5.14) lacks clock_getres,
   but still defines _POSIX_TIMERS!  Because of that we simply use the
   Windows timers under Cygwin.  */
#ifdef __CYGWIN__
# include <windows.h>
#endif

#include "utils.h"
#include "ptimer.h"

/* Depending on the OS, one and only one of PTIMER_POSIX,
   PTIMER_GETTIMEOFDAY, or PTIMER_WINDOWS will be defined.  */

#undef PTIMER_POSIX
#undef PTIMER_GETTIMEOFDAY
#undef PTIMER_WINDOWS

#if defined(WINDOWS) || defined(__CYGWIN__)
# define PTIMER_WINDOWS         /* use Windows timers */
#elif _POSIX_TIMERS - 0 > 0
# define PTIMER_POSIX           /* use POSIX timers (clock_gettime) */
#else
# define PTIMER_GETTIMEOFDAY    /* use gettimeofday */
#endif

#ifdef PTIMER_POSIX
/* Elapsed time measurement using POSIX timers: system time is held in
   struct timespec, time is retrieved using clock_gettime, and
   resolution using clock_getres.

   This method is used on Unix systems that implement POSIX
   timers.  */

typedef struct timespec ptimer_system_time;

#define IMPL_init posix_init
#define IMPL_measure posix_measure
#define IMPL_diff posix_diff
#define IMPL_resolution posix_resolution

/* clock_id to use for POSIX clocks.  This tries to use
   CLOCK_MONOTONIC where available, CLOCK_REALTIME otherwise.  */
static int posix_clock_id;

/* Resolution of the clock, initialized in posix_init. */
static double posix_clock_resolution;

/* Decide which clock_id to use.  */

static void
posix_init (void)
{
  /* List of clocks we want to support: some systems support monotonic
     clocks, Solaris has "high resolution" clock (sometimes
     unavailable except to superuser), and all should support the
     real-time clock.  */
#define NO_SYSCONF_CHECK -1
  static const struct {
    int id;
    int sysconf_name;
  } clocks[] = {
#if defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK - 0 >= 0
    { CLOCK_MONOTONIC, _SC_MONOTONIC_CLOCK },
#endif
#ifdef CLOCK_HIGHRES
    { CLOCK_HIGHRES, NO_SYSCONF_CHECK },
#endif
    { CLOCK_REALTIME, NO_SYSCONF_CHECK },
  };
  size_t i;

  /* Determine the clock we can use.  For a clock to be usable, it
     must be confirmed with sysconf (where applicable) and with
     clock_getres.  If no clock is found, CLOCK_REALTIME is used.  */

  for (i = 0; i < countof (clocks); i++)
    {
      struct timespec r;
      if (clocks[i].sysconf_name != NO_SYSCONF_CHECK)
        if (sysconf (clocks[i].sysconf_name) < 0)
          continue;             /* sysconf claims this clock is unavailable */
      if (clock_getres (clocks[i].id, &r) < 0)
        continue;               /* clock_getres doesn't work for this clock */
      posix_clock_id = clocks[i].id;
      posix_clock_resolution = (double) r.tv_sec + r.tv_nsec / 1e9;
      /* Guard against nonsense returned by a broken clock_getres.  */
      if (posix_clock_resolution == 0)
        posix_clock_resolution = 1e-3;
      break;
    }
  if (i == countof (clocks))
    {
      /* If no clock was found, it means that clock_getres failed for
         the realtime clock.  */
      logprintf (LOG_NOTQUIET, _("Cannot get REALTIME clock frequency: %s\n"),
                 strerror (errno));
      /* Use CLOCK_REALTIME, but invent a plausible resolution. */
      posix_clock_id = CLOCK_REALTIME;
      posix_clock_resolution = 1e-3;
    }
}

static inline void
posix_measure (ptimer_system_time *pst)
{
  clock_gettime (posix_clock_id, pst);
}

static inline double
posix_diff (ptimer_system_time *pst1, ptimer_system_time *pst2)
{
  return ((pst1->tv_sec - pst2->tv_sec)
          + (pst1->tv_nsec - pst2->tv_nsec) / 1e9);
}

static inline double
posix_resolution (void)
{
  return posix_clock_resolution;
}
#endif  /* PTIMER_POSIX */

#ifdef PTIMER_GETTIMEOFDAY
/* Elapsed time measurement using gettimeofday: system time is held in
   struct timeval, retrieved using gettimeofday, and resolution is
   unknown.

   This method is used Unix systems without POSIX timers.  */

typedef struct timeval ptimer_system_time;

#define IMPL_measure gettimeofday_measure
#define IMPL_diff gettimeofday_diff
#define IMPL_resolution gettimeofday_resolution

static inline void
gettimeofday_measure (ptimer_system_time *pst)
{
  gettimeofday (pst, NULL);
}

static inline double
gettimeofday_diff (ptimer_system_time *pst1, ptimer_system_time *pst2)
{
  return ((pst1->tv_sec - pst2->tv_sec)
          + (pst1->tv_usec - pst2->tv_usec) / 1e6);
}

static inline double
gettimeofday_resolution (void)
{
  /* Granularity of gettimeofday varies wildly between architectures.
     However, it appears that on modern machines it tends to be better
     than 1ms.  Assume 100 usecs.  */
  return 0.1;
}
#endif  /* PTIMER_GETTIMEOFDAY */

#ifdef PTIMER_WINDOWS
/* Elapsed time measurement on Windows: where high-resolution timers
   are available, time is stored in a LARGE_INTEGER and retrieved
   using QueryPerformanceCounter.  Otherwise, it is stored in a DWORD
   and retrieved using GetTickCount.

   This method is used on Windows.  */

typedef union {
  DWORD lores;          /* In case GetTickCount is used */
  LARGE_INTEGER hires;  /* In case high-resolution timer is used */
} ptimer_system_time;

#define IMPL_init windows_init
#define IMPL_measure windows_measure
#define IMPL_diff windows_diff
#define IMPL_resolution windows_resolution

/* Whether high-resolution timers are used.  Set by ptimer_initialize_once
   the first time ptimer_new is called. */
static bool windows_hires_timers;

/* Frequency of high-resolution timers -- number of updates per
   second.  Calculated the first time ptimer_new is called provided
   that high-resolution timers are available. */
static double windows_hires_freq;

static void
windows_init (void)
{
  LARGE_INTEGER freq;
  freq.QuadPart = 0;
  QueryPerformanceFrequency (&freq);
  if (freq.QuadPart != 0)
    {
      windows_hires_timers = true;
      windows_hires_freq = (double) freq.QuadPart;
    }
}

static inline void
windows_measure (ptimer_system_time *pst)
{
  if (windows_hires_timers)
    QueryPerformanceCounter (&pst->hires);
  else
    /* Where hires counters are not available, use GetTickCount rather
       GetSystemTime, because it is unaffected by clock skew and
       simpler to use.  Note that overflows don't affect us because we
       never use absolute values of the ticker, only the
       differences.  */
    pst->lores = GetTickCount ();
}

static inline double
windows_diff (ptimer_system_time *pst1, ptimer_system_time *pst2)
{
  if (windows_hires_timers)
    return (pst1->hires.QuadPart - pst2->hires.QuadPart) / windows_hires_freq;
  else
    return pst1->lores - pst2->lores;
}

static double
windows_resolution (void)
{
  if (windows_hires_timers)
    return 1.0 / windows_hires_freq;
  else
    return 10;                  /* according to MSDN */
}
#endif  /* PTIMER_WINDOWS */

/* The code below this point is independent of timer implementation. */

struct ptimer {
  /* The starting point in time which, subtracted from the current
     time, yields elapsed time. */
  ptimer_system_time start;

  /* The most recent elapsed time, calculated by ptimer_measure().  */
  double elapsed_last;

  /* Approximately, the time elapsed between the true start of the
     measurement and the time represented by START.  This is used for
     adjustment when clock skew is detected.  */
  double elapsed_pre_start;
};

/* Allocate a new timer and reset it.  Return the new timer. */

struct ptimer *
ptimer_new (void)
{
  struct ptimer *pt = xnew0 (struct ptimer);
#ifdef IMPL_init
  static bool init_done;
  if (!init_done)
    {
      init_done = true;
      IMPL_init ();
    }
#endif
  ptimer_reset (pt);
  return pt;
}

/* Free the resources associated with the timer.  Its further use is
   prohibited.  */

void
ptimer_destroy (struct ptimer *pt)
{
  xfree (pt);
}

/* Reset timer PT.  This establishes the starting point from which
   ptimer_measure() will return the elapsed time in seconds.  It is
   allowed to reset a previously used timer.  */

void
ptimer_reset (struct ptimer *pt)
{
  /* Set the start time to the current time. */
  IMPL_measure (&pt->start);
  pt->elapsed_last = 0;
  pt->elapsed_pre_start = 0;
}

/* Measure the elapsed time since timer creation/reset.  This causes
   the timer to internally call clock_gettime (or gettimeofday, etc.)
   to update its idea of current time.  The time is returned, but is
   also stored for later access through ptimer_read().

   This function handles clock skew, i.e. time that moves backwards is
   ignored.  */

double
ptimer_measure (struct ptimer *pt)
{
  ptimer_system_time now;
  double elapsed;

  IMPL_measure (&now);
  elapsed = pt->elapsed_pre_start + IMPL_diff (&now, &pt->start);

  /* Ideally we'd just return the difference between NOW and
     pt->start.  However, the system timer can be set back, and we
     could return a value smaller than when we were last called, even
     a negative value.  Both of these would confuse the callers, which
     expect us to return monotonically nondecreasing values.

     Therefore: if ELAPSED is smaller than its previous known value,
     we reset pt->start to the current time and effectively start
     measuring from this point.  But since we don't want the elapsed
     value to start from zero, we set elapsed_pre_start to the last
     elapsed time and increment all future calculations by that
     amount.

     This cannot happen with Windows and POSIX monotonic/highres
     timers, but the check is not expensive.  */

  if (elapsed < pt->elapsed_last)
    {
      pt->start = now;
      pt->elapsed_pre_start = pt->elapsed_last;
      elapsed = pt->elapsed_last;
    }

  pt->elapsed_last = elapsed;
  return elapsed;
}

/* Return the most recent elapsed time measured with ptimer_measure.
   If ptimer_measure has not yet been called since the timer was
   created or reset, this returns 0.  */

double
ptimer_read (const struct ptimer *pt)
{
  return pt->elapsed_last;
}

/* Return the assessed resolution of the timer implementation, in
   seconds.  This is used by code that tries to substitute a better
   value for timers that have returned zero.  */

double
ptimer_resolution (void)
{
  return IMPL_resolution ();
}
