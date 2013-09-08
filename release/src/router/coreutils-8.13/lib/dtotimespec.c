/* Convert double to timespec.

   Copyright (C) 2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* written by Paul Eggert */

/* Convert the double value SEC to a struct timespec.  Round toward
   positive infinity.  On overflow, return an extremal value.  */

#include <config.h>

#include "timespec.h"

#include "intprops.h"

struct timespec
dtotimespec (double sec)
{
  enum { BILLION = 1000 * 1000 * 1000 };
  double min_representable = TYPE_MINIMUM (time_t);
  double max_representable =
    ((TYPE_MAXIMUM (time_t) * (double) BILLION + (BILLION - 1))
     / BILLION);
  struct timespec r;

  if (! (min_representable < sec))
    {
      r.tv_sec = TYPE_MINIMUM (time_t);
      r.tv_nsec = 0;
    }
  else if (! (sec < max_representable))
    {
      r.tv_sec = TYPE_MAXIMUM (time_t);
      r.tv_nsec = BILLION - 1;
    }
  else
    {
      time_t s = sec;
      double frac = BILLION * (sec - s);
      long ns = frac;
      ns += ns < frac;
      s += ns / BILLION;
      ns %= BILLION;

      if (ns < 0)
        {
          s--;
          ns += BILLION;
        }

      r.tv_sec = s;
      r.tv_nsec = ns;
    }

  return r;
}
