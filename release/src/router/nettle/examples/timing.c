/* timing.c

   Copyright (C) 2013 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "timing.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if HAVE_CLOCK_GETTIME && defined CLOCK_PROCESS_CPUTIME_ID
#define TRY_CLOCK_GETTIME 1
struct timespec cgt_start;

static void NORETURN PRINTF_STYLE(1,2)
die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(EXIT_FAILURE);
}

static int
cgt_works_p(void)
{
  struct timespec now;
  return clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now) == 0;
}

static void
cgt_time_start(void)
{
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cgt_start) < 0)
    die("clock_gettime failed: %s\n", strerror(errno));
}

static double
cgt_time_end(void)
{
  struct timespec end;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) < 0)
    die("clock_gettime failed: %s\n", strerror(errno));

  return end.tv_sec - cgt_start.tv_sec
    + 1e-9 * (end.tv_nsec - cgt_start.tv_nsec);
}
#endif /* !HAVE_CLOCK_GETTIME */

static clock_t clock_start;

static void
clock_time_start(void)
{
  clock_start = clock();
}

static double
clock_time_end(void)
{
  return (double) (clock() - (clock_start)) / CLOCKS_PER_SEC;
}

void (*time_start)(void) = clock_time_start;
double (*time_end)(void) = clock_time_end;

void time_init(void)
{
  /* Choose timing function */
#if TRY_CLOCK_GETTIME
  if (cgt_works_p())
    {
      time_start = cgt_time_start;
      time_end = cgt_time_end;
    }
  else
    {
      fprintf(stderr, "clock_gettime not working, falling back to clock\n");
      time_start = clock_time_start;
      time_end = clock_time_end;
    }
#endif
}
