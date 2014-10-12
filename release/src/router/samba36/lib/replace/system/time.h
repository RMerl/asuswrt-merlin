#ifndef _system_time_h
#define _system_time_h
/* 
   Unix SMB/CIFS implementation.

   time system include wrappers

   Copyright (C) Andrew Tridgell 2004

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

*/

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#else
struct utimbuf {
	time_t actime;       /* access time */
	time_t modtime;      /* modification time */
};
#endif

#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
	time_t tv_sec;            /* Seconds.  */
	long tv_nsec;           /* Nanoseconds.  */
};
#endif

#ifndef HAVE_MKTIME
/* define is in "replace.h" */
time_t rep_mktime(struct tm *t);
#endif

#ifndef HAVE_TIMEGM
/* define is in "replace.h" */
time_t rep_timegm(struct tm *tm);
#endif

#ifndef HAVE_UTIME
/* define is in "replace.h" */
int rep_utime(const char *filename, const struct utimbuf *buf);
#endif

#ifndef HAVE_UTIMES
/* define is in "replace.h" */
int rep_utimes(const char *filename, const struct timeval tv[2]);
#endif

#ifndef HAVE_CLOCK_GETTIME
/* CLOCK_REALTIME is required by POSIX */
#define CLOCK_REALTIME 0
typedef int clockid_t;
int rep_clock_gettime(clockid_t clk_id, struct timespec *tp);
#endif
/* make sure we have a best effort CUSTOM_CLOCK_MONOTONIC we can rely on */
#if defined(CLOCK_MONOTONIC)
#define CUSTOM_CLOCK_MONOTONIC CLOCK_MONOTONIC
#elif defined(CLOCK_HIGHRES)
#define CUSTOM_CLOCK_MONOTONIC CLOCK_HIGHRES
#else
#define CUSTOM_CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#endif
