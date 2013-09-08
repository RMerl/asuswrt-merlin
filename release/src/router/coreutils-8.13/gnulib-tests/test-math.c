/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of <math.h> substitute.
   Copyright (C) 2007-2011 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>, 2007.  */

#include <config.h>

#include <math.h>

#ifndef NAN
# error NAN should be defined
choke me
#endif

#if 0
/* Check that NAN expands into a constant expression.  */
static float n = NAN;
#endif

/* Compare two numbers with ==.
   This is a separate function because IRIX 6.5 "cc -O" miscompiles an
   'x == x' test.  */
static int
numeric_equal (double x, double y)
{
  return x == y;
}

int
main (void)
{
  double d = NAN;
  double zero = 0.0;
  if (numeric_equal (d, d))
    return 1;
  d = HUGE_VAL;
  if (!numeric_equal (d, 1.0 / zero))
    return 1;
  return 0;
}
