/* Copyright (C) 2002, 2004-2005, 2007, 2009-2017 Free Software Foundation,
   Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.  */

#ifndef _LIBC
# include <config.h>
#endif

#include <time.h>

#include <assert.h>
#include <ctype.h>
#ifdef _LIBC
# include <langinfo.h>
#endif
#include <limits.h>
#include <string.h>
#include <stdbool.h>

#ifdef _LIBC
# include "../locale/localeinfo.h"
#endif

#ifndef _LIBC
enum ptime_locale_status { not, loc, raw };
#endif



#define match_char(ch1, ch2) if (ch1 != ch2) return NULL
#if defined _LIBC && defined __GNUC__ && __GNUC__ >= 2
# define match_string(cs1, s2) \
  ({ size_t len = strlen (cs1);                                               \
     int result = __strncasecmp_l ((cs1), (s2), len, locale) == 0;            \
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
# define get_alt_number(from, to, n) \
  ({                                                                          \
     __label__ do_normal;                                                     \
                                                                              \
     if (*decided != raw)                                                     \
       {                                                                      \
         val = _nl_parse_alt_digit (&rp HELPER_LOCALE_ARG);                   \
         if (val == -1 && *decided != loc)                                    \
           {                                                                  \
             *decided = loc;                                                  \
             goto do_normal;                                                  \
           }                                                                  \
        if (val < from || val > to)                                           \
          return NULL;                                                        \
       }                                                                      \
     else                                                                     \
       {                                                                      \
       do_normal:                                                             \
         get_number (from, to, n);                                            \
       }                                                                      \
    0;                                                                        \
  })
#else
# define get_alt_number(from, to, n) \
  /* We don't have the alternate representation.  */                          \
  get_number(from, to, n)
#endif
#define recursive(new_fmt) \
  (*(new_fmt) != '\0'                                                         \
   && (rp = __strptime_internal (rp, (new_fmt), tm,                           \
                                 decided, era_cnt LOCALE_ARG)) != NULL)


#ifdef _LIBC
/* This is defined in locale/C-time.c in the GNU libc.  */
extern const struct locale_data _nl_C_LC_TIME attribute_hidden;

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

static const unsigned short int __mon_yday[2][13] =
  {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };
#endif

#if defined _LIBC
/* We use this code also for the extended locale handling where the
   function gets as an additional argument the locale which has to be
   used.  To access the values we have to redefine the _NL_CURRENT
   macro.  */
# define strptime               __strptime_l
# undef _NL_CURRENT
# define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)
# undef _NL_CURRENT_WORD
# define _NL_CURRENT_WORD(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].word)
# define LOCALE_PARAM , locale
# define LOCALE_ARG , locale
# define LOCALE_PARAM_PROTO , __locale_t locale
# define LOCALE_PARAM_DECL __locale_t locale;
# define HELPER_LOCALE_ARG , current
# define ISSPACE(Ch) __isspace_l (Ch, locale)
#else
# define LOCALE_PARAM
# define LOCALE_ARG
# define LOCALE_PARAM_DECL
# define LOCALE_PARAM_PROTO
# define HELPER_LOCALE_ARG
# define ISSPACE(Ch) isspace (Ch)
#endif




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


#ifdef _LIBC
char *
internal_function
#else
static char *
#endif
__strptime_internal (rp, fmt, tm, decided, era_cnt LOCALE_PARAM)
     const char *rp;
     const char *fmt;
     struct tm *tm;
     enum ptime_locale_status *decided;
     int era_cnt;
     LOCALE_PARAM_DECL
{
#ifdef _LIBC
  struct locale_data *const current = locale->__locales[LC_TIME];
#endif

  int cnt;
  size_t val;
  int have_I, is_pm;
  int century, want_century;
  int want_era;
  int have_wday, want_xday;
  int have_yday;
  int have_mon, have_mday;
  int have_uweek, have_wweek;
  int week_no;
#ifdef _NL_CURRENT
  size_t num_eras;
  struct era_entry *era = NULL;
  const char *rp_backup;
#endif

  have_I = is_pm = 0;
  century = -1;
  want_century = 0;
  want_era = 0;
  week_no = 0;

  have_wday = want_xday = have_yday = have_mon = have_mday = have_uweek = 0;
  have_wweek = 0;

  while (*fmt != '\0')
    {
      /* A white space in the format string matches 0 more or white
         space in the input string.  */
      if (ISSPACE (*fmt))
        {
          while (ISSPACE (*rp))
            ++rp;
          ++fmt;
          continue;
        }

      /* Any character but '%' must be matched by the same character
         in the iput string.  */
      if (*fmt != '%')
        {
          match_char (*fmt++, *rp++);
          continue;
        }

      ++fmt;
#ifndef _NL_CURRENT
      /* We need this for handling the 'E' modifier.  */
    start_over:
#else
      /* Make back up of current processing pointer.  */
      rp_backup = rp;
#endif

      switch (*fmt++)
        {
        case '%':
          /* Match the '%' character itself.  */
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
#ifdef _NL_CURRENT
        match_century:
#endif
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
        case 'l':
          /* Match hour in 12-hour clock.  GNU extension.  */
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
          while (ISSPACE (*rp))
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
        case 'q':
          /* Match quarter of year.  GNU extension.  */
          get_number (1, 4, 1);
          tm->tm_mon = (val - 1) * 3;
          tm->tm_mday = 1;
          have_mon = 1;
          have_mday = 1;
          want_xday = 1;
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
               the 'get_number' macro.  Instead read the number
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
          get_number (0, 53, 2);
          week_no = val;
          have_uweek = 1;
          break;
        case 'W':
          get_number (0, 53, 2);
          week_no = val;
          have_wweek = 1;
          break;
        case 'V':
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
#ifdef _NL_CURRENT
        match_year_in_century:
#endif
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
        case 'z':
          /* We recognize two formats: if two digits are given, these
             specify hours.  If fours digits are used, minutes are
             also specified.  */
          {
            bool neg _GL_UNUSED;
            int n;

            val = 0;
            while (*rp == ' ')
              ++rp;
            if (*rp != '+' && *rp != '-')
              return NULL;
            neg = *rp++ == '-';
            n = 0;
            while (n < 4 && *rp >= '0' && *rp <= '9')
              {
                val = val * 10 + *rp++ - '0';
                ++n;
              }
            if (n == 2)
              val *= 100;
            else if (n != 4)
              /* Only two or four digits recognized.  */
              return NULL;
            else
              {
                /* We have to convert the minutes into decimal.  */
                if (val % 100 >= 60)
                  return NULL;
                val = (val / 100) * 100 + ((val % 100) * 50) / 30;
              }
            if (val > 1200)
              return NULL;
#if defined _LIBC || HAVE_TM_GMTOFF
            tm->tm_gmtoff = (val * 3600) / 100;
            if (neg)
              tm->tm_gmtoff = -tm->tm_gmtoff;
#endif
          }
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
              if (*decided != raw)
                {
                  if (era_cnt >= 0)
                    {
                      era = _nl_select_era_entry (era_cnt HELPER_LOCALE_ARG);
                      if (era != NULL && match_string (era->era_name, rp))
                        {
                          *decided = loc;
                          break;
                        }
                      else
                        return NULL;
                    }

                  num_eras = _NL_CURRENT_WORD (LC_TIME,
                                               _NL_TIME_ERA_NUM_ENTRIES);
                  for (era_cnt = 0; era_cnt < (int) num_eras;
                       ++era_cnt, rp = rp_backup)
                    {
                      era = _nl_select_era_entry (era_cnt
                                                  HELPER_LOCALE_ARG);
                      if (era != NULL && match_string (era->era_name, rp))
                        {
                          *decided = loc;
                          break;
                        }
                    }
                  if (era_cnt != (int) num_eras)
                    break;

                  era_cnt = -1;
                  if (*decided == loc)
                    return NULL;

                  *decided = raw;
                }
              /* The C locale has no era information, so use the
                 normal representation.  */
              goto match_century;
            case 'y':
              if (*decided != raw)
                {
                  get_number(0, 9999, 4);
                  tm->tm_year = val;
                  want_era = 1;
                  want_xday = 1;
                  want_century = 1;

                  if (era_cnt >= 0)
                    {
                      assert (*decided == loc);

                      era = _nl_select_era_entry (era_cnt HELPER_LOCALE_ARG);
                      bool match = false;
                      if (era != NULL)
                        {
                          int delta = ((tm->tm_year - era->offset)
                                       * era->absolute_direction);
                          match = (delta >= 0
                                   && delta < (((int64_t) era->stop_date[0]
                                                - (int64_t) era->start_date[0])
                                               * era->absolute_direction));
                        }
                      if (! match)
                        return NULL;

                      break;
                    }

                  num_eras = _NL_CURRENT_WORD (LC_TIME,
                                               _NL_TIME_ERA_NUM_ENTRIES);
                  for (era_cnt = 0; era_cnt < (int) num_eras; ++era_cnt)
                    {
                      era = _nl_select_era_entry (era_cnt
                                                  HELPER_LOCALE_ARG);
                      if (era != NULL)
                        {
                          int delta = ((tm->tm_year - era->offset)
                                       * era->absolute_direction);
                          if (delta >= 0
                              && delta < (((int64_t) era->stop_date[0]
                                           - (int64_t) era->start_date[0])
                                          * era->absolute_direction))
                            {
                              *decided = loc;
                              break;
                            }
                        }
                    }
                  if (era_cnt != (int) num_eras)
                    break;

                  era_cnt = -1;
                  if (*decided == loc)
                    return NULL;

                  *decided = raw;
                }

              goto match_year_in_century;
            case 'Y':
              if (*decided != raw)
                {
                  num_eras = _NL_CURRENT_WORD (LC_TIME,
                                               _NL_TIME_ERA_NUM_ENTRIES);
                  for (era_cnt = 0; era_cnt < (int) num_eras;
                       ++era_cnt, rp = rp_backup)
                    {
                      era = _nl_select_era_entry (era_cnt HELPER_LOCALE_ARG);
                      if (era != NULL && recursive (era->era_format))
                        break;
                    }
                  if (era_cnt == (int) num_eras)
                    {
                      era_cnt = -1;
                      if (*decided == loc)
                        return NULL;
                      else
                        rp = rp_backup;
                    }
                  else
                    {
                      *decided = loc;
                      era_cnt = -1;
                      break;
                    }

                  *decided = raw;
                }
              get_number (0, 9999, 4);
              tm->tm_year = val - 1900;
              want_century = 0;
              want_xday = 1;
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
              tm->tm_hour = val % 12;
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
            case 'q':
              /* Match quarter using alternate numeric symbols.  */
              get_alt_number (1, 4, 1);
              tm->tm_mon = (val - 1) * 3;
              tm->tm_mday = 1;
              have_mon = 1;
              have_mday = 1;
              want_xday = 1;
              break;
            case 'S':
              /* Match seconds using alternate numeric symbols.  */
              get_alt_number (0, 61, 2);
              tm->tm_sec = val;
              break;
            case 'U':
              get_alt_number (0, 53, 2);
              week_no = val;
              have_uweek = 1;
              break;
            case 'W':
              get_alt_number (0, 53, 2);
              week_no = val;
              have_wweek = 1;
              break;
            case 'V':
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

  if (era_cnt != -1)
    {
#ifdef _NL_CURRENT
      era = _nl_select_era_entry (era_cnt HELPER_LOCALE_ARG);
      if (era == NULL)
        return NULL;
      if (want_era)
        tm->tm_year = (era->start_date[0]
                       + ((tm->tm_year - era->offset)
                          * era->absolute_direction));
      else
        /* Era start year assumed.  */
        tm->tm_year = era->start_date[0];
#endif
    }
  else
    if (want_era)
      {
        /* No era found but we have seen an E modifier.  Rectify some
           values.  */
        if (want_century && century == -1 && tm->tm_year < 69)
          tm->tm_year += 100;
      }

  if (want_xday && !have_wday)
    {
      if ( !(have_mon && have_mday) && have_yday)
        {
          /* We don't have tm_mon and/or tm_mday, compute them.  */
          int t_mon = 0;
          while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon] <= tm->tm_yday)
              t_mon++;
          if (!have_mon)
              tm->tm_mon = t_mon - 1;
          if (!have_mday)
              tm->tm_mday =
                (tm->tm_yday
                 - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1);
        }
      day_of_the_week (tm);
    }

  if (want_xday && !have_yday)
    day_of_the_year (tm);

  if ((have_uweek || have_wweek) && have_wday)
    {
      int save_wday = tm->tm_wday;
      int save_mday = tm->tm_mday;
      int save_mon = tm->tm_mon;
      int w_offset = have_uweek ? 0 : 1;

      tm->tm_mday = 1;
      tm->tm_mon = 0;
      day_of_the_week (tm);
      if (have_mday)
        tm->tm_mday = save_mday;
      if (have_mon)
        tm->tm_mon = save_mon;

      if (!have_yday)
        tm->tm_yday = ((7 - (tm->tm_wday - w_offset)) % 7
                       + (week_no - 1) *7
                       + save_wday - w_offset);

      if (!have_mday || !have_mon)
        {
          int t_mon = 0;
          while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon]
                 <= tm->tm_yday)
            t_mon++;
          if (!have_mon)
            tm->tm_mon = t_mon - 1;
          if (!have_mday)
              tm->tm_mday =
                (tm->tm_yday
                 - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1);
        }

      tm->tm_wday = save_wday;
    }

  return (char *) rp;
}


char *
strptime (buf, format, tm LOCALE_PARAM)
     const char *restrict buf;
     const char *restrict format;
     struct tm *restrict tm;
     LOCALE_PARAM_DECL
{
  enum ptime_locale_status decided;

#ifdef _NL_CURRENT
  decided = not;
#else
  decided = raw;
#endif
  return __strptime_internal (buf, format, tm, &decided, -1 LOCALE_ARG);
}

#ifdef _LIBC
weak_alias (__strptime_l, strptime_l)
#endif
