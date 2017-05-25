/* Provide gettimeofday for systems that don't have it or for which it's broken.

   Copyright (C) 2001-2003, 2005-2007, 2009-2017 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

/* written by Jim Meyering */

#include <config.h>

/* Specification.  */
#include <sys/time.h>

#include <time.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
# define WINDOWS_NATIVE
# include <windows.h>
#endif

#include "localtime-buffer.h"

#ifdef WINDOWS_NATIVE

/* GetSystemTimePreciseAsFileTime was introduced only in Windows 8.  */
typedef void (WINAPI * GetSystemTimePreciseAsFileTimeFuncType) (FILETIME *lpTime);
static GetSystemTimePreciseAsFileTimeFuncType GetSystemTimePreciseAsFileTimeFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE kernel32 = LoadLibrary ("kernel32.dll");
  if (kernel32 != NULL)
    {
      GetSystemTimePreciseAsFileTimeFunc =
	(GetSystemTimePreciseAsFileTimeFuncType) GetProcAddress (kernel32, "GetSystemTimePreciseAsFileTime");
    }
  initialized = TRUE;
}

#endif

/* This is a wrapper for gettimeofday.  It is used only on systems
   that lack this function, or whose implementation of this function
   causes problems.
   Work around the bug in some systems whereby gettimeofday clobbers
   the static buffer that localtime uses for its return value.  The
   gettimeofday function from Mac OS X 10.0.4 (i.e., Darwin 1.3.7) has
   this problem.  */

int
gettimeofday (struct timeval *restrict tv, void *restrict tz)
{
#undef gettimeofday
#ifdef WINDOWS_NATIVE

  /* On native Windows, there are two ways to get the current time:
     GetSystemTimeAsFileTime
     <https://msdn.microsoft.com/en-us/library/ms724397.aspx>
     or
     GetSystemTimePreciseAsFileTime
     <https://msdn.microsoft.com/en-us/library/hh706895.aspx>.
     GetSystemTimeAsFileTime produces values that jump by increments of
     15.627 milliseconds (!) on average.
     Whereas GetSystemTimePreciseAsFileTime values usually jump by 1 or 2
     microseconds.
     More discussion on this topic:
     <http://www.windowstimestamp.com/description>.  */
  FILETIME current_time;

  if (!initialized)
    initialize ();
  if (GetSystemTimePreciseAsFileTimeFunc != NULL)
    GetSystemTimePreciseAsFileTimeFunc (&current_time);
  else
    GetSystemTimeAsFileTime (&current_time);

  /* Convert from FILETIME to 'struct timeval'.  */
  /* FILETIME: <https://msdn.microsoft.com/en-us/library/ms724284.aspx> */
  ULONGLONG since_1601 =
    ((ULONGLONG) current_time.dwHighDateTime << 32)
    | (ULONGLONG) current_time.dwLowDateTime;
  /* Between 1601-01-01 and 1970-01-01 there were 280 normal years and 89 leap
     years, in total 134774 days.  */
  ULONGLONG since_1970 =
    since_1601 - (ULONGLONG) 134774 * (ULONGLONG) 86400 * (ULONGLONG) 10000000;
  ULONGLONG microseconds_since_1970 = since_1970 / (ULONGLONG) 10;
  tv->tv_sec = microseconds_since_1970 / (ULONGLONG) 1000000;
  tv->tv_usec = microseconds_since_1970 % (ULONGLONG) 1000000;

  return 0;

#else

# if HAVE_GETTIMEOFDAY
#  if GETTIMEOFDAY_CLOBBERS_LOCALTIME
  /* Save and restore the contents of the buffer used for localtime's
     result around the call to gettimeofday.  */
  struct tm save = *localtime_buffer_addr;
#  endif

#  if defined timeval /* 'struct timeval' overridden by gnulib?  */
#   undef timeval
  struct timeval otv;
  int result = gettimeofday (&otv, (struct timezone *) tz);
  if (result == 0)
    {
      tv->tv_sec = otv.tv_sec;
      tv->tv_usec = otv.tv_usec;
    }
#  else
  int result = gettimeofday (tv, (struct timezone *) tz);
#  endif

#  if GETTIMEOFDAY_CLOBBERS_LOCALTIME
  *localtime_buffer_addr = save;
#  endif

  return result;

# else

#  if !defined OK_TO_USE_1S_CLOCK
#   error "Only 1-second nominal clock resolution found.  Is that intended?" \
          "If so, compile with the -DOK_TO_USE_1S_CLOCK option."
#  endif
  tv->tv_sec = time (NULL);
  tv->tv_usec = 0;

  return 0;

# endif
#endif
}
