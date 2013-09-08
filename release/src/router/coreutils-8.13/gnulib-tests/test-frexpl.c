/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of splitting a 'long double' into fraction and mantissa.
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

#include "signature.h"
SIGNATURE_CHECK (frexpl, long double, (long double, int *));

#include <float.h>

#include "fpucw.h"
#include "isnanl-nolibm.h"
#include "minus-zero.h"
#include "nan.h"
#include "macros.h"

/* Avoid some warnings from "gcc -Wshadow".
   This file doesn't use the exp() function.  */
#undef exp
#define exp exponent

/* On MIPS IRIX machines, LDBL_MIN_EXP is -1021, but the smallest reliable
   exponent for 'long double' is -964.  Similarly, on PowerPC machines,
   LDBL_MIN_EXP is -1021, but the smallest reliable exponent for 'long double'
   is -968.  For exponents below that, the precision may be truncated to the
   precision used for 'double'.  */
#ifdef __sgi
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 57)
#elif defined __ppc || defined __ppc__ || defined __powerpc || defined __powerpc__
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 53)
#else
# define MIN_NORMAL_EXP LDBL_MIN_EXP
#endif

static long double
my_ldexp (long double x, int d)
{
  for (; d > 0; d--)
    x *= 2.0L;
  for (; d < 0; d++)
    x *= 0.5L;
  return x;
}

int
main ()
{
  int i;
  long double x;
  DECL_LONG_DOUBLE_ROUNDING

  BEGIN_LONG_DOUBLE_ROUNDING ();

  { /* NaN.  */
    int exp = -9999;
    long double mantissa;
    x = NaNl ();
    mantissa = frexpl (x, &exp);
    ASSERT (isnanl (mantissa));
  }

  { /* Positive infinity.  */
    int exp = -9999;
    long double mantissa;
    x = 1.0L / 0.0L;
    mantissa = frexpl (x, &exp);
    ASSERT (mantissa == x);
  }

  { /* Negative infinity.  */
    int exp = -9999;
    long double mantissa;
    x = -1.0L / 0.0L;
    mantissa = frexpl (x, &exp);
    ASSERT (mantissa == x);
  }

  { /* Positive zero.  */
    int exp = -9999;
    long double mantissa;
    x = 0.0L;
    mantissa = frexpl (x, &exp);
    ASSERT (exp == 0);
    ASSERT (mantissa == x);
    ASSERT (!signbit (mantissa));
  }

  { /* Negative zero.  */
    int exp = -9999;
    long double mantissa;
    x = minus_zerol;
    mantissa = frexpl (x, &exp);
    ASSERT (exp == 0);
    ASSERT (mantissa == x);
    ASSERT (signbit (mantissa));
  }

  for (i = 1, x = 1.0L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.5L);
    }
  for (i = 1, x = 1.0L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.5L);
    }
  for (; i >= LDBL_MIN_EXP - 100 && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.5L);
    }

  for (i = 1, x = -1.0L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == -0.5L);
    }
  for (i = 1, x = -1.0L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == -0.5L);
    }
  for (; i >= LDBL_MIN_EXP - 100 && x < 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == -0.5L);
    }

  for (i = 1, x = 1.01L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.505L);
    }
  for (i = 1, x = 1.01L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.505L);
    }
  for (; i >= LDBL_MIN_EXP - 100 && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa >= 0.5L);
      ASSERT (mantissa < 1.0L);
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  for (i = 1, x = 1.73205L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.866025L);
    }
  for (i = 1, x = 1.73205L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == 0.866025L);
    }
  for (; i >= LDBL_MIN_EXP - 100 && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = frexpl (x, &exp);
      ASSERT (exp == i || exp == i + 1);
      ASSERT (mantissa >= 0.5L);
      ASSERT (mantissa < 1.0L);
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  return 0;
}
