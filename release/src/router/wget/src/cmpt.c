/* Replacements for routines missing on some systems.
   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software
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

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <errno.h>

/* Some systems lack certain functions normally taken for granted.
   For example, Windows doesn't have strptime, and some systems don't
   have a usable fnmatch.  This file should contain fallback
   implementations of such missing functions.  It should *not* define
   new Wget-specific interfaces -- those should be placed in utils.c
   or elsewhere.  */

/* strcasecmp and strncasecmp apparently originated with BSD 4.4.
   SUSv3 seems to be the only standard out there (that I can find)
   that requires their existence, so in theory there might be systems
   still in use that lack them.  Note that these don't get defined
   under Windows because mswindows.h defines them to the equivalent
   Windows functions stricmp and strnicmp.  */

#ifndef HAVE_STRCASECMP
/* From GNU libc.  */
/* Compare S1 and S2, ignoring case, returning less than, equal to or
   greater than zero if S1 is lexiographically less than,
   equal to or greater than S2.  */
int
strcasecmp (const char *s1, const char *s2)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = c_tolower (*p1++);
      c2 = c_tolower (*p2++);
      if (c1 == '\0')
        break;
    }
  while (c1 == c2);

  return c1 - c2;
}
#endif /* not HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
/* From GNU libc.  */
/* Compare no more than N characters of S1 and S2,
   ignoring case, returning less than, equal to or
   greater than zero if S1 is lexicographically less
   than, equal to or greater than S2.  */
int
strncasecmp (const char *s1, const char *s2, size_t n)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = c_tolower (*p1++);
      c2 = c_tolower (*p2++);
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
    } while (--n > 0);

  return c1 - c2;
}
#endif /* not HAVE_STRNCASECMP */

#ifndef HAVE_MEMRCHR
/* memrchr is a GNU extension.  It is like the memchr function, except
   that it searches backwards from the end of the n bytes pointed to
   by s instead of forwards from the front.  */

void *
memrchr (const void *s, int c, size_t n)
{
  const char *b = s;
  const char *e = b + n;
  while (e > b)
    if (*--e == c)
      return (void *) e;
  return NULL;
}
#endif

/* strptime is required by POSIX, but it is missing from Windows,
   which means we must keep a fallback implementation.  It is
   reportedly missing or broken on many older Unix systems as well, so
   it's good to have around.  */

#ifndef HAVE_STRPTIME
/* From GNU libc 2.1.3.  */
/* Ulrich, thanks for helping me out with this!  --hniksic  */

/* strptime - Convert a string representation of time to a time value.
   Copyright (C) 1996, 1997, 1998, 1999, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1996.  */

/* XXX This version of the implementation is not really complete.
   Some of the fields cannot add information alone.  But if seeing
   some of them in the same format (such as year, week and weekday)
   this is enough information for determining the date.  */

#ifndef __P
# define __P(args) args
#endif /* not __P */

#if ! HAVE_LOCALTIME_R && ! defined localtime_r
# ifdef _LIBC
#  define localtime_r __localtime_r
# else
/* Approximate localtime_r as best we can in its absence.  */
#  define localtime_r my_localtime_r
static struct tm *localtime_r __P ((const time_t *, struct tm *));
static struct tm *
localtime_r (t, tp)
     const time_t *t;
     struct tm *tp;
{
  struct tm *l = localtime (t);
  if (! l)
    return 0;
  *tp = *l;
  return tp;
}
# endif /* ! _LIBC */
#endif /* ! HAVE_LOCALTIME_R && ! defined (localtime_r) */


#define match_char(ch1, ch2) if (ch1 != ch2) return NULL
#if defined __GNUC__ && __GNUC__ >= 2
# define match_string(cs1, s2) \
  ({ size_t len = strlen (cs1);                                               \
     int result = strncasecmp ((cs1), (s2), len) == 0;                        \
     if (result) (s2) += len;                                                 \
     result; })
#else
/* Oh come on.  Get a reasonable compiler.  */
# define match_string(cs1, s2) \
  (strncasecmp ((cs1), (s2), strlen (cs1)) ? 0 : ((s2) += strlen (cs1), 1))
#endif
/* We intentionally do not use isdigit() for testing because this will
   lead to problems with the wide character version.  */
#define get_number(from, to, n) \
  do {                                                                        \
    int __n = n;                                                              \
    val = 0;                                                                  \
    while (*rp == ' ')                                                        \
      ++rp;                                                                   \
    if (*rp < '0' || *rp > '9')                                               \
      return NULL;                                                            \
    do {                                                                      \
      val *= 10;                                                              \
      val += *rp++ - '0';                                                     \
    } while (--__n > 0 && val * 10 <= to && *rp >= '0' && *rp <= '9');        \
    if (val < from || val > to)                                               \
      return NULL;                                                            \
  } while (0)
#ifdef _NL_CURRENT
/* Added check for __GNUC__ extensions here for Wget. --abbotti */
# if defined __GNUC__ && __GNUC__ >= 2
#  define get_alt_number(from, to, n) \
  ({                                                                          \
    __label__ do_normal;                                                      \
    if (*decided != raw)                                                      \
      {                                                                       \
        const char *alts = _NL_CURRENT (LC_TIME, ALT_DIGITS);                 \
        int __n = n;                                                          \
        int any = 0;                                                          \
        while (*rp == ' ')                                                    \
          ++rp;                                                               \
        val = 0;                                                              \
        do {                                                                  \
          val *= 10;                                                          \
          while (*alts != '\0')                                               \
            {                                                                 \
              size_t len = strlen (alts);                                     \
              if (strncasecmp (alts, rp, len) == 0)                           \
                break;                                                        \
              alts += len + 1;                                                \
              ++val;                                                          \
            }                                                                 \
          if (*alts == '\0')                                                  \
            {                                                                 \
              if (*decided == not && ! any)                                   \
                goto do_normal;                                               \
              /* If we haven't read anything it's an error.  */               \
              if (! any)                                                      \
                return NULL;                                                  \
              /* Correct the premature multiplication.  */                    \
              val /= 10;                                                      \
              break;                                                          \
            }                                                                 \
          else                                                                \
            *decided = loc;                                                   \
        } while (--__n > 0 && val * 10 <= to);                                \
        if (val < from || val > to)                                           \
          return NULL;                                                        \
      }                                                                       \
    else                                                                      \
      {                                                                       \
       do_normal:                                                             \
        get_number (from, to, n);                                             \
      }                                                                       \
    0;                                                                        \
  })
# else
#  define get_alt_number(from, to, n) \
  do {
    if (*decided != raw)                                                      \
      {                                                                       \
        const char *alts = _NL_CURRENT (LC_TIME, ALT_DIGITS);                 \
        int __n = n;                                                          \
        int any = 0;                                                          \
        while (*rp == ' ')                                                    \
          ++rp;                                                               \
        val = 0;                                                              \
        do {                                                                  \
          val *= 10;                                                          \
          while (*alts != '\0')                                               \
            {                                                                 \
              size_t len = strlen (alts);                                     \
              if (strncasecmp (alts, rp, len) == 0)                           \
                break;                                                        \
              alts += len + 1;                                                \
              ++val;                                                          \
            }                                                                 \
          if (*alts == '\0')                                                  \
            {                                                                 \
              if (*decided == not && ! any)                                   \
                goto do_normal;                                               \
              /* If we haven't read anything it's an error.  */               \
              if (! any)                                                      \
                return NULL;                                                  \
              /* Correct the premature multiplication.  */                    \
              val /= 10;                                                      \
              break;                                                          \
            }                                                                 \
          else                                                                \
            *decided = loc;                                                   \
        } while (--__n > 0 && val * 10 <= to);                                \
        if (val < from || val > to)                                           \
          return NULL;                                                        \
      }                                                                       \
    else                                                                      \
      {                                                                       \
       do_normal:                                                             \
        get_number (from, to, n);                                             \
      }                                                                       \
  } while (0)
# endif /* defined __GNUC__ && __GNUC__ >= 2 */
#else
# define get_alt_number(from, to, n) \
  /* We don't have the alternate representation.  */                          \
  get_number(from, to, n)
#endif
#define recursive(new_fmt) \
  (*(new_fmt) != '\0'                                                         \
   && (rp = strptime_internal (rp, (new_fmt), tm, decided)) != NULL)


#ifdef _LIBC
/* This is defined in locale/C-time.c in the GNU libc.  */
extern const struct locale_data _nl_C_LC_TIME;
extern const unsigned short int __mon_yday[2][13];

# define weekday_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (DAY_1)].string)
# define ab_weekday_name \
  (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (ABDAY_1)].string)
# define month_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (MON_1)].string)
# define ab_month_name (&_nl_C_LC_TIME.values[_NL_ITEM_INDEX (ABMON_1)].string)
# define HERE_D_T_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (D_T_FMT)].string)
# define HERE_D_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (D_FMT)].string)
# define HERE_AM_STR (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (AM_STR)].string)
# define HERE_PM_STR (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (PM_STR)].string)
# define HERE_T_FMT_AMPM \
  (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (T_FMT_AMPM)].string)
# define HERE_T_FMT (_nl_C_LC_TIME.values[_NL_ITEM_INDEX (T_FMT)].string)

# define strncasecmp(s1, s2, n) __strncasecmp (s1, s2, n)
#else
static char const weekday_name[][10] =
  {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };
static char const ab_weekday_name[][4] =
  {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
static char const month_name[][10] =
  {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
static char const ab_month_name[][4] =
  {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
# define HERE_D_T_FMT "%a %b %e %H:%M:%S %Y"
# define HERE_D_FMT "%m/%d/%y"
# define HERE_AM_STR "AM"
# define HERE_PM_STR "PM"
# define HERE_T_FMT_AMPM "%I:%M:%S %p"
# define HERE_T_FMT "%H:%M:%S"

const unsigned short int __mon_yday[2][13];
# ifndef NEED_MON_YDAY
#  define NEED_MON_YDAY
# endif
#endif

/* Status of lookup: do we use the locale data or the raw data?  */
enum locale_status { not, loc, raw };


#ifndef __isleap
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year) \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

/* Compute the day of the week.  */
static void
day_of_the_week (struct tm *tm)
{
  /* We know that January 1st 1970 was a Thursday (= 4).  Compute the
     difference between this data in the one on TM and so determine
     the weekday.  */
  int corr_year = 1900 + tm->tm_year - (tm->tm_mon < 2);
  int wday = (-473
              + (365 * (tm->tm_year - 70))
              + (corr_year / 4)
              - ((corr_year / 4) / 25) + ((corr_year / 4) % 25 < 0)
              + (((corr_year / 4) / 25) / 4)
              + __mon_yday[0][tm->tm_mon]
              + tm->tm_mday - 1);
  tm->tm_wday = ((wday % 7) + 7) % 7;
}

/* Compute the day of the year.  */
static void
day_of_the_year (struct tm *tm)
{
  tm->tm_yday = (__mon_yday[__isleap (1900 + tm->tm_year)][tm->tm_mon]
                 + (tm->tm_mday - 1));
}

static char *
#ifdef _LIBC
internal_function
#endif
strptime_internal __P ((const char *buf, const char *format, struct tm *tm,
                        enum locale_status *decided));

static char *
#ifdef _LIBC
internal_function
#endif
strptime_internal (rp, fmt, tm, decided)
     const char *rp;
     const char *fmt;
     struct tm *tm;
     enum locale_status *decided;
{
#ifdef _NL_CURRENT
  const char *rp_backup;
#endif
  int cnt;
  size_t val;
  int have_I, is_pm;
  int century, want_century;
  int have_wday, want_xday;
  int have_yday;
  int have_mon, have_mday;

  have_I = is_pm = 0;
  century = -1;
  want_century = 0;
  have_wday = want_xday = have_yday = have_mon = have_mday = 0;

  while (*fmt != '\0')
    {
      /* A white space in the format string matches 0 more or white
         space in the input string.  */
      if (c_isspace (*fmt))
        {
          while (c_isspace (*rp))
            ++rp;
          ++fmt;
          continue;
        }

      /* Any character but `%' must be matched by the same character
         in the iput string.  */
      if (*fmt != '%')
        {
          match_char (*fmt++, *rp++);
          continue;
        }

      ++fmt;
#ifndef _NL_CURRENT
      /* We need this for handling the `E' modifier.  */
    start_over:
#endif

#ifdef _NL_CURRENT
      /* Make back up of current processing pointer.  */
      rp_backup = rp;
#endif

      switch (*fmt++)
        {
        case '%':
          /* Match the `%' character itself.  */
          match_char ('%', *rp++);
          break;
        case 'a':
        case 'A':
          /* Match day of week.  */
          for (cnt = 0; cnt < 7; ++cnt)
            {
#ifdef _NL_CURRENT
              if (*decided !=raw)
                {
                  if (match_string (_NL_CURRENT (LC_TIME, DAY_1 + cnt), rp))
                    {
                      if (*decided == not
                          && strcmp (_NL_CURRENT (LC_TIME, DAY_1 + cnt),
                                     weekday_name[cnt]))
                        *decided = loc;
                      break;
                    }
                  if (match_string (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt), rp))
                    {
                      if (*decided == not
                          && strcmp (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt),
                                     ab_weekday_name[cnt]))
                        *decided = loc;
                      break;
                    }
                }
#endif
              if (*decided != loc
                  && (match_string (weekday_name[cnt], rp)
                      || match_string (ab_weekday_name[cnt], rp)))
                {
                  *decided = raw;
                  break;
                }
            }
          if (cnt == 7)
            /* Does not match a weekday name.  */
            return NULL;
          tm->tm_wday = cnt;
          have_wday = 1;
          break;
        case 'b':
        case 'B':
        case 'h':
          /* Match month name.  */
          for (cnt = 0; cnt < 12; ++cnt)
            {
#ifdef _NL_CURRENT
              if (*decided !=raw)
                {
                  if (match_string (_NL_CURRENT (LC_TIME, MON_1 + cnt), rp))
                    {
                      if (*decided == not
                          && strcmp (_NL_CURRENT (LC_TIME, MON_1 + cnt),
                                     month_name[cnt]))
                        *decided = loc;
                      break;
                    }
                  if (match_string (_NL_CURRENT (LC_TIME, ABMON_1 + cnt), rp))
                    {
                      if (*decided == not
                          && strcmp (_NL_CURRENT (LC_TIME, ABMON_1 + cnt),
                                     ab_month_name[cnt]))
                        *decided = loc;
                      break;
                    }
                }
#endif
              if (match_string (month_name[cnt], rp)
                  || match_string (ab_month_name[cnt], rp))
                {
                  *decided = raw;
                  break;
                }
            }
          if (cnt == 12)
            /* Does not match a month name.  */
            return NULL;
          tm->tm_mon = cnt;
          want_xday = 1;
          break;
        case 'c':
          /* Match locale's date and time format.  */
#ifdef _NL_CURRENT
          if (*decided != raw)
            {
              if (!recursive (_NL_CURRENT (LC_TIME, D_T_FMT)))
                {
                  if (*decided == loc)
                    return NULL;
                  else
                    rp = rp_backup;
                }
              else
                {
                  if (*decided == not &&
                      strcmp (_NL_CURRENT (LC_TIME, D_T_FMT), HERE_D_T_FMT))
                    *decided = loc;
                  want_xday = 1;
                  break;
                }
              *decided = raw;
            }
#endif
          if (!recursive (HERE_D_T_FMT))
            return NULL;
          want_xday = 1;
          break;
        case 'C':
          /* Match century number.  */
          get_number (0, 99, 2);
          century = val;
          want_xday = 1;
          break;
        case 'd':
        case 'e':
          /* Match day of month.  */
          get_number (1, 31, 2);
          tm->tm_mday = val;
          have_mday = 1;
          want_xday = 1;
          break;
        case 'F':
          if (!recursive ("%Y-%m-%d"))
            return NULL;
          want_xday = 1;
          break;
        case 'x':
#ifdef _NL_CURRENT
          if (*decided != raw)
            {
              if (!recursive (_NL_CURRENT (LC_TIME, D_FMT)))
                {
                  if (*decided == loc)
                    return NULL;
                  else
                    rp = rp_backup;
                }
              else
                {
                  if (*decided == not
                      && strcmp (_NL_CURRENT (LC_TIME, D_FMT), HERE_D_FMT))
                    *decided = loc;
                  want_xday = 1;
                  break;
                }
              *decided = raw;
            }
#endif
          /* Fall through.  */
        case 'D':
          /* Match standard day format.  */
          if (!recursive (HERE_D_FMT))
            return NULL;
          want_xday = 1;
          break;
        case 'k':
        case 'H':
          /* Match hour in 24-hour clock.  */
          get_number (0, 23, 2);
          tm->tm_hour = val;
          have_I = 0;
          break;
        case 'I':
          /* Match hour in 12-hour clock.  */
          get_number (1, 12, 2);
          tm->tm_hour = val % 12;
          have_I = 1;
          break;
        case 'j':
          /* Match day number of year.  */
          get_number (1, 366, 3);
          tm->tm_yday = val - 1;
          have_yday = 1;
          break;
        case 'm':
          /* Match number of month.  */
          get_number (1, 12, 2);
          tm->tm_mon = val - 1;
          have_mon = 1;
          want_xday = 1;
          break;
        case 'M':
          /* Match minute.  */
          get_number (0, 59, 2);
          tm->tm_min = val;
          break;
        case 'n':
        case 't':
          /* Match any white space.  */
          while (c_isspace (*rp))
            ++rp;
          break;
        case 'p':
          /* Match locale's equivalent of AM/PM.  */
#ifdef _NL_CURRENT
          if (*decided != raw)
            {
              if (match_string (_NL_CURRENT (LC_TIME, AM_STR), rp))
                {
                  if (strcmp (_NL_CURRENT (LC_TIME, AM_STR), HERE_AM_STR))
                    *decided = loc;
                  break;
                }
              if (match_string (_NL_CURRENT (LC_TIME, PM_STR), rp))
                {
                  if (strcmp (_NL_CURRENT (LC_TIME, PM_STR), HERE_PM_STR))
                    *decided = loc;
                  is_pm = 1;
                  break;
                }
              *decided = raw;
            }
#endif
          if (!match_string (HERE_AM_STR, rp))
            {
              if (match_string (HERE_PM_STR, rp))
                is_pm = 1;
              else
                return NULL;
            }
          break;
        case 'r':
#ifdef _NL_CURRENT
          if (*decided != raw)
            {
              if (!recursive (_NL_CURRENT (LC_TIME, T_FMT_AMPM)))
                {
                  if (*decided == loc)
                    return NULL;
                  else
                    rp = rp_backup;
                }
              else
                {
                  if (*decided == not &&
                      strcmp (_NL_CURRENT (LC_TIME, T_FMT_AMPM),
                              HERE_T_FMT_AMPM))
                    *decided = loc;
                  break;
                }
              *decided = raw;
            }
#endif
          if (!recursive (HERE_T_FMT_AMPM))
            return NULL;
          break;
        case 'R':
          if (!recursive ("%H:%M"))
            return NULL;
          break;
        case 's':
          {
            /* The number of seconds may be very high so we cannot use
               the `get_number' macro.  Instead read the number
               character for character and construct the result while
               doing this.  */
            time_t secs = 0;
            if (*rp < '0' || *rp > '9')
              /* We need at least one digit.  */
              return NULL;

            do
              {
                secs *= 10;
                secs += *rp++ - '0';
              }
            while (*rp >= '0' && *rp <= '9');

            if (localtime_r (&secs, tm) == NULL)
              /* Error in function.  */
              return NULL;
          }
          break;
        case 'S':
          get_number (0, 61, 2);
          tm->tm_sec = val;
          break;
        case 'X':
#ifdef _NL_CURRENT
          if (*decided != raw)
            {
              if (!recursive (_NL_CURRENT (LC_TIME, T_FMT)))
                {
                  if (*decided == loc)
                    return NULL;
                  else
                    rp = rp_backup;
                }
              else
                {
                  if (strcmp (_NL_CURRENT (LC_TIME, T_FMT), HERE_T_FMT))
                    *decided = loc;
                  break;
                }
              *decided = raw;
            }
#endif
          /* Fall through.  */
        case 'T':
          if (!recursive (HERE_T_FMT))
            return NULL;
          break;
        case 'u':
          get_number (1, 7, 1);
          tm->tm_wday = val % 7;
          have_wday = 1;
          break;
        case 'g':
          get_number (0, 99, 2);
          /* XXX This cannot determine any field in TM.  */
          break;
        case 'G':
          if (*rp < '0' || *rp > '9')
            return NULL;
          /* XXX Ignore the number since we would need some more
             information to compute a real date.  */
          do
            ++rp;
          while (*rp >= '0' && *rp <= '9');
          break;
        case 'U':
        case 'V':
        case 'W':
          get_number (0, 53, 2);
          /* XXX This cannot determine any field in TM without some
             information.  */
          break;
        case 'w':
          /* Match number of weekday.  */
          get_number (0, 6, 1);
          tm->tm_wday = val;
          have_wday = 1;
          break;
        case 'y':
          /* Match year within century.  */
          get_number (0, 99, 2);
          /* The "Year 2000: The Millennium Rollover" paper suggests that
             values in the range 69-99 refer to the twentieth century.  */
          tm->tm_year = val >= 69 ? val : val + 100;
          /* Indicate that we want to use the century, if specified.  */
          want_century = 1;
          want_xday = 1;
          break;
        case 'Y':
          /* Match year including century number.  */
          get_number (0, 9999, 4);
          tm->tm_year = val - 1900;
          want_century = 0;
          want_xday = 1;
          break;
        case 'Z':
          /* XXX How to handle this?  */
          break;
        case 'E':
#ifdef _NL_CURRENT
          switch (*fmt++)
            {
            case 'c':
              /* Match locale's alternate date and time format.  */
              if (*decided != raw)
                {
                  const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_T_FMT);

                  if (*fmt == '\0')
                    fmt = _NL_CURRENT (LC_TIME, D_T_FMT);

                  if (!recursive (fmt))
                    {
                      if (*decided == loc)
                        return NULL;
                      else
                        rp = rp_backup;
                    }
                  else
                    {
                      if (strcmp (fmt, HERE_D_T_FMT))
                        *decided = loc;
                      want_xday = 1;
                      break;
                    }
                  *decided = raw;
                }
              /* The C locale has no era information, so use the
                 normal representation.  */
              if (!recursive (HERE_D_T_FMT))
                return NULL;
              want_xday = 1;
              break;
            case 'C':
            case 'y':
            case 'Y':
              /* Match name of base year in locale's alternate
                 representation.  */
              /* XXX This is currently not implemented.  It should
                 use the value _NL_CURRENT (LC_TIME, ERA).  */
              break;
            case 'x':
              if (*decided != raw)
                {
                  const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_FMT);

                  if (*fmt == '\0')
                    fmt = _NL_CURRENT (LC_TIME, D_FMT);

                  if (!recursive (fmt))
                    {
                      if (*decided == loc)
                        return NULL;
                      else
                        rp = rp_backup;
                    }
                  else
                    {
                      if (strcmp (fmt, HERE_D_FMT))
                        *decided = loc;
                      break;
                    }
                  *decided = raw;
                }
              if (!recursive (HERE_D_FMT))
                return NULL;
              break;
            case 'X':
              if (*decided != raw)
                {
                  const char *fmt = _NL_CURRENT (LC_TIME, ERA_T_FMT);

                  if (*fmt == '\0')
                    fmt = _NL_CURRENT (LC_TIME, T_FMT);

                  if (!recursive (fmt))
                    {
                      if (*decided == loc)
                        return NULL;
                      else
                        rp = rp_backup;
                    }
                  else
                    {
                      if (strcmp (fmt, HERE_T_FMT))
                        *decided = loc;
                      break;
                    }
                  *decided = raw;
                }
              if (!recursive (HERE_T_FMT))
                return NULL;
              break;
            default:
              return NULL;
            }
          break;
#else
          /* We have no information about the era format.  Just use
             the normal format.  */
          if (*fmt != 'c' && *fmt != 'C' && *fmt != 'y' && *fmt != 'Y'
              && *fmt != 'x' && *fmt != 'X')
            /* This is an illegal format.  */
            return NULL;

          goto start_over;
#endif
        case 'O':
          switch (*fmt++)
            {
            case 'd':
            case 'e':
              /* Match day of month using alternate numeric symbols.  */
              get_alt_number (1, 31, 2);
              tm->tm_mday = val;
              have_mday = 1;
              want_xday = 1;
              break;
            case 'H':
              /* Match hour in 24-hour clock using alternate numeric
                 symbols.  */
              get_alt_number (0, 23, 2);
              tm->tm_hour = val;
              have_I = 0;
              break;
            case 'I':
              /* Match hour in 12-hour clock using alternate numeric
                 symbols.  */
              get_alt_number (1, 12, 2);
              tm->tm_hour = val - 1;
              have_I = 1;
              break;
            case 'm':
              /* Match month using alternate numeric symbols.  */
              get_alt_number (1, 12, 2);
              tm->tm_mon = val - 1;
              have_mon = 1;
              want_xday = 1;
              break;
            case 'M':
              /* Match minutes using alternate numeric symbols.  */
              get_alt_number (0, 59, 2);
              tm->tm_min = val;
              break;
            case 'S':
              /* Match seconds using alternate numeric symbols.  */
              get_alt_number (0, 61, 2);
              tm->tm_sec = val;
              break;
            case 'U':
            case 'V':
            case 'W':
              get_alt_number (0, 53, 2);
              /* XXX This cannot determine any field in TM without
                 further information.  */
              break;
            case 'w':
              /* Match number of weekday using alternate numeric symbols.  */
              get_alt_number (0, 6, 1);
              tm->tm_wday = val;
              have_wday = 1;
              break;
            case 'y':
              /* Match year within century using alternate numeric symbols.  */
              get_alt_number (0, 99, 2);
              tm->tm_year = val >= 69 ? val : val + 100;
              want_xday = 1;
              break;
            default:
              return NULL;
            }
          break;
        default:
          return NULL;
        }
    }

  if (have_I && is_pm)
    tm->tm_hour += 12;

  if (century != -1)
    {
      if (want_century)
        tm->tm_year = tm->tm_year % 100 + (century - 19) * 100;
      else
        /* Only the century, but not the year.  Strange, but so be it.  */
        tm->tm_year = (century - 19) * 100;
    }

  if (want_xday && !have_wday) {
      if ( !(have_mon && have_mday) && have_yday)  {
          /* we don't have tm_mon and/or tm_mday, compute them */
          int t_mon = 0;
          while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon] <= tm->tm_yday)
              t_mon++;
          if (!have_mon)
              tm->tm_mon = t_mon - 1;
          if (!have_mday)
              tm->tm_mday = tm->tm_yday - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1;
      }
      day_of_the_week (tm);
  }
  if (want_xday && !have_yday)
    day_of_the_year (tm);

  return (char *) rp;
}


char *
strptime (buf, format, tm)
     const char *buf;
     const char *format;
     struct tm *tm;
{
  enum locale_status decided;
#ifdef _NL_CURRENT
  decided = not;
#else
  decided = raw;
#endif
  return strptime_internal (buf, format, tm, &decided);
}
#endif /* not HAVE_STRPTIME */

#ifdef NEED_MON_YDAY
const unsigned short int __mon_yday[2][13] =
  {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };
#endif

/* fnmatch is required by POSIX, but we include an implementation for
   the sake of systems that don't have it, most notably Windows.  Some
   systems do have fnmatch, but Apache's installation process installs
   its own fnmatch.h (incompatible with the system one!) in a system
   include directory, effectively rendering fnmatch unusable.  This
   has been fixed with Apache 2, where fnmatch has been moved to apr
   and given a prefix, but many systems out there are still (as of
   this writing in 2005) broken and we must cater to them.

   Additionally, according to some conventional wisdom, many
   historical implementations of fnmatch are buggy and unreliable.  If
   yours is such, undefine SYSTEM_FNMATCH in sysdep.h and tell us
   about it.  */

#ifndef SYSTEM_FNMATCH

#define __FNM_FLAGS     (FNM_PATHNAME | FNM_NOESCAPE | FNM_PERIOD)

/* Match STRING against the filename pattern PATTERN, returning zero
   if it matches, FNM_NOMATCH if not.  This implementation comes from
   an earlier version of GNU Bash.  (It doesn't make sense to update
   it with a newer version because those versions add a lot of
   features Wget doesn't use or care about.)  */

int
fnmatch (const char *pattern, const char *string, int flags)
{
  register const char *p = pattern, *n = string;
  register char c;

  if ((flags & ~__FNM_FLAGS) != 0)
    {
      errno = EINVAL;
      return (-1);
    }

  while ((c = *p++) != '\0')
    {
      switch (c)
        {
        case '?':
          if (*n == '\0')
            return (FNM_NOMATCH);
          else if ((flags & FNM_PATHNAME) && *n == '/')
            return (FNM_NOMATCH);
          else if ((flags & FNM_PERIOD) && *n == '.' &&
                   (n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
            return (FNM_NOMATCH);
          break;

        case '\\':
          if (!(flags & FNM_NOESCAPE))
            c = *p++;
          if (*n != c)
            return (FNM_NOMATCH);
          break;

        case '*':
          if ((flags & FNM_PERIOD) && *n == '.' &&
              (n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
            return (FNM_NOMATCH);

          for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
            if (((flags & FNM_PATHNAME) && *n == '/') ||
                (c == '?' && *n == '\0'))
              return (FNM_NOMATCH);

          if (c == '\0')
            return (0);

          {
            char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
            for (--p; *n != '\0'; ++n)
              if ((c == '[' || *n == c1) &&
                  fnmatch (p, n, flags & ~FNM_PERIOD) == 0)
                return (0);
            return (FNM_NOMATCH);
          }

        case '[':
          {
            /* Nonzero if the sense of the character class is
               inverted.  */
            register int not;

            if (*n == '\0')
              return (FNM_NOMATCH);

            if ((flags & FNM_PERIOD) && *n == '.' &&
                (n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
              return (FNM_NOMATCH);

            /* Make sure there is a closing `]'.  If there isn't,
               the `[' is just a character to be matched.  */
            {
              register const char *np;

              for (np = p; np && *np && *np != ']'; np++)
                ;

              if (np && !*np)
                {
                  if (*n != '[')
                    return (FNM_NOMATCH);
                  goto next_char;
                }
            }

            not = (*p == '!' || *p == '^');
            if (not)
              ++p;

            c = *p++;
            while (1)
              {
                register char cstart = c, cend = c;

                if (!(flags & FNM_NOESCAPE) && c == '\\')
                  cstart = cend = *p++;

                if (c == '\0')
                  /* [ (unterminated) loses.  */
                  return (FNM_NOMATCH);

                c = *p++;

                if ((flags & FNM_PATHNAME) && c == '/')
                  /* [/] can never match.  */
                  return (FNM_NOMATCH);

                if (c == '-' && *p != ']')
                  {
                    cend = *p++;
                    if (!(flags & FNM_NOESCAPE) && cend == '\\')
                      cend = *p++;
                    if (cend == '\0')
                      return (FNM_NOMATCH);
                    c = *p++;
                  }

                if (*n >= cstart && *n <= cend)
                  goto matched;

                if (c == ']')
                  break;
              }
            if (!not)
              return (FNM_NOMATCH);

          next_char:
            break;

          matched:
            /* Skip the rest of the [...] that already matched.  */
            while (c != ']')
              {
                if (c == '\0')
                  /* [... (unterminated) loses.  */
                  return (FNM_NOMATCH);

                c = *p++;
                if (!(flags & FNM_NOESCAPE) && c == '\\')
                  /* 1003.2d11 is unclear if this is right.  %%% */
                  ++p;
              }
            if (not)
              return (FNM_NOMATCH);
          }
          break;

        default:
          if (c != *n)
            return (FNM_NOMATCH);
        }

      ++n;
    }

  if (*n == '\0')
    return (0);

  return (FNM_NOMATCH);
}

#endif /* not SYSTEM_FNMATCH */

#ifndef HAVE_TIMEGM
/* timegm is a GNU extension, but lately also available on *BSD
   systems and possibly elsewhere. */

/* True if YEAR is a leap year. */
#define ISLEAP(year)                                            \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

/* Number of leap years in the range [y1, y2). */
#define LEAPYEARS(y1, y2)                                               \
  ((y2-1)/4 - (y1-1)/4) - ((y2-1)/100 - (y1-1)/100) + ((y2-1)/400 - (y1-1)/400)

/* Inverse of gmtime: converts struct tm to time_t, assuming the data
   in tm is UTC rather than local timezone.  This implementation
   returns the number of seconds elapsed since midnight 1970-01-01,
   converted to time_t.  */

time_t
timegm (struct tm *t)
{
  static const unsigned short int month_to_days[][13] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 }, /* normal */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }  /* leap */
  };
  const int year = 1900 + t->tm_year;
  unsigned long secs;  /* until 2106-02-07 for 32-bit unsigned long */
  int days;

  if (year < 1970)
    return (time_t) -1;

  days = 365 * (year - 1970);
  /* Take into account leap years between 1970 and YEAR, not counting
     YEAR itself.  */
  days += LEAPYEARS (1970, year);
  if (t->tm_mon < 0 || t->tm_mon >= 12)
    return (time_t) -1;
  days += month_to_days[ISLEAP (year)][t->tm_mon];
  days += t->tm_mday - 1;

  secs = days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
  return (time_t) secs;
}
#endif /* HAVE_TIMEGM */

#ifdef NEED_STRTOLL
/* strtoll is required by C99 and used by Wget only on systems with
   LFS.  Unfortunately, some systems have LFS, but no strtoll or
   equivalent.  These include HPUX 11.0 and Windows.

   We use #ifdef NEED_STRTOLL instead of #ifndef HAVE_STRTOLL because
   of the systems which have a suitable replacement (e.g. _strtoi64 on
   Windows), on which Wget's str_to_wgint is instructed to use that
   instead.  */

static inline int
char_value (char c, int base)
{
  int value;
  if (c < '0')
    return -1;
  if ('0' <= c && c <= '9')
    value = c - '0';
  else if ('a' <= c && c <= 'z')
    value = c - 'a' + 10;
  else if ('A' <= c && c <= 'Z')
    value = c - 'A' + 10;
  else
    return -1;
  if (value >= base)
    return -1;
  return value;
}

#define STRTOLL_MAX TYPE_MAXIMUM (strtoll_type)
/* This definition assumes two's complement arithmetic */
#define STRTOLL_MIN (-STRTOLL_MAX - 1)

/* Like a%b, but always returns a positive number when A is negative.
   (C doesn't guarantee the sign of the result.)  */
#define MOD(a, b) ((strtoll_type) -1 % 2 == 1 ? (a) % (b) : - ((a) % (b)))

/* A strtoll-like replacement for systems that have an integral type
   larger than long but don't supply strtoll.  This implementation
   makes no assumptions about the size of strtoll_type.  */

strtoll_type
strtoll (const char *nptr, char **endptr, int base)
{
  strtoll_type result = 0;
  bool negative;

  if (base != 0 && (base < 2 || base > 36))
    {
      errno = EINVAL;
      return 0;
    }

  while (*nptr == ' ' || *nptr == '\t')
    ++nptr;
  if (*nptr == '-')
    {
      negative = true;
      ++nptr;
    }
  else if (*nptr == '+')
    {
      negative = false;
      ++nptr;
    }
  else
    negative = false;

  /* If BASE is 0, determine the real base based on the beginning on
     the number; octal numbers begin with "0", hexadecimal with "0x",
     and the others are considered octal.  */
  if (*nptr == '0')
    {
      if ((base == 0 || base == 16)
          &&
          (*(nptr + 1) == 'x' || *(nptr + 1) == 'X'))
        {
          base = 16;
          nptr += 2;
          /* "0x" must be followed by at least one hex char.  If not,
             return 0 and place ENDPTR on 'x'. */
          if (!c_isxdigit (*nptr))
            {
              --nptr;
              goto out;
            }
        }
      else if (base == 0)
        base = 8;
    }
  else if (base == 0)
    base = 10;

  if (!negative)
    {
      /* Parse positive number, checking for overflow. */
      int digit;
      /* Overflow watermark.  If RESULT exceeds it, overflow occurs on
         this digit.  If result==WATERMARK, current digit may not
         exceed the last digit of maximum value. */
      const strtoll_type WATERMARK = STRTOLL_MAX / base;
      for (; (digit = char_value (*nptr, base)) != -1; ++nptr)
        {
          if (result > WATERMARK
              || (result == WATERMARK && digit > STRTOLL_MAX % base))
            {
              result = STRTOLL_MAX;
              errno = ERANGE;
              break;
            }
          result = base * result + digit;
        }
    }
  else
    {
      /* Parse negative number, checking for underflow. */
      int digit;
      const strtoll_type WATERMARK = STRTOLL_MIN / base;
      for (; (digit = char_value (*nptr, base)) != -1; ++nptr)
        {
          if (result < WATERMARK
              || (result == WATERMARK && digit > MOD (STRTOLL_MIN, base)))
            {
              result = STRTOLL_MIN;
              errno = ERANGE;
              break;
            }
          result = base * result - digit;
        }
    }
 out:
  if (endptr)
    *endptr = (char *) nptr;
  return result;
}

#undef STRTOLL_MAX
#undef STRTOLL_MIN
#undef ABS

#endif  /* NEED_STRTOLL */
