/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test that posixtime works as required.
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

/* Written by Jim Meyering.  */

#include <config.h>

#include "strftime.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "macros.h"
#define STREQ(a, b) (strcmp (a, b) == 0)

struct posixtm_test
{
  time_t in;
  int in_ns;
  char const *fmt;
  char const *exp;
};

static struct posixtm_test const T[] =
  {
    { 1300000000, 0,            "%F", "2011-03-13" },
    { 0,          0,            NULL, NULL }
  };

int
main (void)
{
  int fail = 0;
  unsigned int i;

  for (i = 0; T[i].fmt; i++)
    {
      char buf[1000];
      time_t t = T[i].in;
      struct tm *tm = gmtime (&t);
      size_t n;
      int utc = 1;

      ASSERT (tm);

      n = nstrftime (buf, sizeof buf, T[i].fmt, tm, utc, T[i].in_ns);
      if (n == 0)
        {
          fail = 1;
          printf ("nstrftime failed with format %s\n", T[i].fmt);
        }

      if (! STREQ (buf, T[i].exp))
        {
          fail = 1;
          printf ("%s: result mismatch: got %s, expected %s\n",
                  T[i].fmt, buf, T[i].exp);
        }
    }

  return fail;
}

/*
Local Variables:
indent-tabs-mode: nil
End:
*/
