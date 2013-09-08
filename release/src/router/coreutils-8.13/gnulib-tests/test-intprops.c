/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test intprops.h.
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

/* Written by Paul Eggert.  */

/* Tell gcc not to warn about the many (X < 0) expressions that
   the overflow macros expand to.  */
#if (__GNUC__ == 4 && 3 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#include <config.h>

#include "intprops.h"
#include "verify.h"

#include <stdbool.h>
#include <inttypes.h>

#include "macros.h"

/* VERIFY (X) uses a static assertion for compilers that are known to work,
   and falls back on a dynamic assertion for other compilers.
   These tests should be checkable via 'verify' rather than 'ASSERT', but
   using 'verify' would run into a bug with HP-UX 11.23 cc; see
   <http://lists.gnu.org/archive/html/bug-gnulib/2011-05/msg00401.html>.  */
#if __GNUC__ || __SUNPRO_C
# define VERIFY(x) do { verify (x); } while (0)
#else
# define VERIFY(x) ASSERT (x)
#endif

int
main (void)
{
  /* Use VERIFY for tests that must be integer constant expressions,
     ASSERT otherwise.  */

  /* TYPE_IS_INTEGER.  */
  ASSERT (TYPE_IS_INTEGER (bool));
  ASSERT (TYPE_IS_INTEGER (char));
  ASSERT (TYPE_IS_INTEGER (signed char));
  ASSERT (TYPE_IS_INTEGER (unsigned char));
  ASSERT (TYPE_IS_INTEGER (short int));
  ASSERT (TYPE_IS_INTEGER (unsigned short int));
  ASSERT (TYPE_IS_INTEGER (int));
  ASSERT (TYPE_IS_INTEGER (unsigned int));
  ASSERT (TYPE_IS_INTEGER (long int));
  ASSERT (TYPE_IS_INTEGER (unsigned long int));
  ASSERT (TYPE_IS_INTEGER (intmax_t));
  ASSERT (TYPE_IS_INTEGER (uintmax_t));
  ASSERT (! TYPE_IS_INTEGER (float));
  ASSERT (! TYPE_IS_INTEGER (double));
  ASSERT (! TYPE_IS_INTEGER (long double));

  /* TYPE_SIGNED.  */
  /* VERIFY (! TYPE_SIGNED (bool)); // not guaranteed by gnulib substitute */
  VERIFY (TYPE_SIGNED (signed char));
  VERIFY (! TYPE_SIGNED (unsigned char));
  VERIFY (TYPE_SIGNED (short int));
  VERIFY (! TYPE_SIGNED (unsigned short int));
  VERIFY (TYPE_SIGNED (int));
  VERIFY (! TYPE_SIGNED (unsigned int));
  VERIFY (TYPE_SIGNED (long int));
  VERIFY (! TYPE_SIGNED (unsigned long int));
  VERIFY (TYPE_SIGNED (intmax_t));
  VERIFY (! TYPE_SIGNED (uintmax_t));
  ASSERT (TYPE_SIGNED (float));
  ASSERT (TYPE_SIGNED (double));
  ASSERT (TYPE_SIGNED (long double));

  /* Integer representation.  */
  VERIFY (INT_MIN + INT_MAX < 0
          ? (TYPE_TWOS_COMPLEMENT (int)
             && ! TYPE_ONES_COMPLEMENT (int) && ! TYPE_SIGNED_MAGNITUDE (int))
          : (! TYPE_TWOS_COMPLEMENT (int)
             && (TYPE_ONES_COMPLEMENT (int) || TYPE_SIGNED_MAGNITUDE (int))));

  /* TYPE_MINIMUM, TYPE_MAXIMUM.  */
  VERIFY (TYPE_MINIMUM (char) == CHAR_MIN);
  VERIFY (TYPE_MAXIMUM (char) == CHAR_MAX);
  VERIFY (TYPE_MINIMUM (unsigned char) == 0);
  VERIFY (TYPE_MAXIMUM (unsigned char) == UCHAR_MAX);
  VERIFY (TYPE_MINIMUM (signed char) == SCHAR_MIN);
  VERIFY (TYPE_MAXIMUM (signed char) == SCHAR_MAX);
  VERIFY (TYPE_MINIMUM (short int) == SHRT_MIN);
  VERIFY (TYPE_MAXIMUM (short int) == SHRT_MAX);
  VERIFY (TYPE_MINIMUM (unsigned short int) == 0);
  VERIFY (TYPE_MAXIMUM (unsigned short int) == USHRT_MAX);
  VERIFY (TYPE_MINIMUM (int) == INT_MIN);
  VERIFY (TYPE_MAXIMUM (int) == INT_MAX);
  VERIFY (TYPE_MINIMUM (unsigned int) == 0);
  VERIFY (TYPE_MAXIMUM (unsigned int) == UINT_MAX);
  VERIFY (TYPE_MINIMUM (long int) == LONG_MIN);
  VERIFY (TYPE_MAXIMUM (long int) == LONG_MAX);
  VERIFY (TYPE_MINIMUM (unsigned long int) == 0);
  VERIFY (TYPE_MAXIMUM (unsigned long int) == ULONG_MAX);
  VERIFY (TYPE_MINIMUM (intmax_t) == INTMAX_MIN);
  VERIFY (TYPE_MAXIMUM (intmax_t) == INTMAX_MAX);
  VERIFY (TYPE_MINIMUM (uintmax_t) == 0);
  VERIFY (TYPE_MAXIMUM (uintmax_t) == UINTMAX_MAX);

  /* INT_BITS_STRLEN_BOUND.  */
  VERIFY (INT_BITS_STRLEN_BOUND (1) == 1);
  VERIFY (INT_BITS_STRLEN_BOUND (2620) == 789);

  /* INT_STRLEN_BOUND, INT_BUFSIZE_BOUND.  */
  #ifdef INT32_MAX /* POSIX guarantees int32_t; this ports to non-POSIX.  */
  VERIFY (INT_STRLEN_BOUND (int32_t) == sizeof ("-2147483648") - 1);
  VERIFY (INT_BUFSIZE_BOUND (int32_t) == sizeof ("-2147483648"));
  #endif
  #ifdef INT64_MAX
  VERIFY (INT_STRLEN_BOUND (int64_t) == sizeof ("-9223372036854775808") - 1);
  VERIFY (INT_BUFSIZE_BOUND (int64_t) == sizeof ("-9223372036854775808"));
  #endif

  /* All the INT_<op>_RANGE_OVERFLOW tests are equally valid as
     INT_<op>_OVERFLOW tests, so define a single macro to do both.  */
  #define CHECK_BINOP(op, a, b, min, max, overflow)                      \
    (INT_##op##_RANGE_OVERFLOW (a, b, min, max) == (overflow)            \
     && INT_##op##_OVERFLOW (a, b) == (overflow))
  #define CHECK_UNOP(op, a, min, max, overflow)                          \
    (INT_##op##_RANGE_OVERFLOW (a, min, max) == (overflow)               \
     && INT_##op##_OVERFLOW (a) == (overflow))

  /* INT_<op>_RANGE_OVERFLOW, INT_<op>_OVERFLOW.  */
  VERIFY (INT_ADD_RANGE_OVERFLOW (INT_MAX, 1, INT_MIN, INT_MAX));
  VERIFY (INT_ADD_OVERFLOW (INT_MAX, 1));
  VERIFY (CHECK_BINOP (ADD, INT_MAX, 1, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (ADD, INT_MAX, -1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (ADD, INT_MIN, 1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (ADD, INT_MIN, -1, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (ADD, UINT_MAX, 1u, 0u, UINT_MAX, true));
  VERIFY (CHECK_BINOP (ADD, 0u, 1u, 0u, UINT_MAX, false));

  VERIFY (CHECK_BINOP (SUBTRACT, INT_MAX, 1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (SUBTRACT, INT_MAX, -1, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (SUBTRACT, INT_MIN, 1, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (SUBTRACT, INT_MIN, -1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (SUBTRACT, UINT_MAX, 1u, 0u, UINT_MAX, false));
  VERIFY (CHECK_BINOP (SUBTRACT, 0u, 1u, 0u, UINT_MAX, true));

  VERIFY (CHECK_UNOP (NEGATE, INT_MIN, INT_MIN, INT_MAX,
                      TYPE_TWOS_COMPLEMENT (int)));
  VERIFY (CHECK_UNOP (NEGATE, 0, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_UNOP (NEGATE, INT_MAX, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_UNOP (NEGATE, 0u, 0u, UINT_MAX, false));
  VERIFY (CHECK_UNOP (NEGATE, 1u, 0u, UINT_MAX, true));
  VERIFY (CHECK_UNOP (NEGATE, UINT_MAX, 0u, UINT_MAX, true));

  VERIFY (CHECK_BINOP (MULTIPLY, INT_MAX, INT_MAX, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (MULTIPLY, INT_MAX, INT_MIN, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (MULTIPLY, INT_MIN, INT_MAX, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (MULTIPLY, INT_MIN, INT_MIN, INT_MIN, INT_MAX, true));
  VERIFY (CHECK_BINOP (MULTIPLY, -1, INT_MIN, INT_MIN, INT_MAX,
                       INT_NEGATE_OVERFLOW (INT_MIN)));
  VERIFY (CHECK_BINOP (MULTIPLY, LONG_MIN / INT_MAX, (long int) INT_MAX,
                       LONG_MIN, LONG_MIN, false));

  VERIFY (CHECK_BINOP (DIVIDE, INT_MIN, -1, INT_MIN, INT_MAX,
                       INT_NEGATE_OVERFLOW (INT_MIN)));
  VERIFY (CHECK_BINOP (DIVIDE, INT_MAX, 1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (DIVIDE, (unsigned int) INT_MIN,
                       -1u, 0u, UINT_MAX, false));

  VERIFY (CHECK_BINOP (REMAINDER, INT_MIN, -1, INT_MIN, INT_MAX,
                       INT_NEGATE_OVERFLOW (INT_MIN)));
  VERIFY (CHECK_BINOP (REMAINDER, INT_MAX, 1, INT_MIN, INT_MAX, false));
  VERIFY (CHECK_BINOP (REMAINDER, (unsigned int) INT_MIN,
                       -1u, 0u, UINT_MAX, false));

  VERIFY (CHECK_BINOP (LEFT_SHIFT, UINT_MAX, 1, 0u, UINT_MAX, true));
  VERIFY (CHECK_BINOP (LEFT_SHIFT, UINT_MAX / 2 + 1, 1, 0u, UINT_MAX, true));
  VERIFY (CHECK_BINOP (LEFT_SHIFT, UINT_MAX / 2, 1, 0u, UINT_MAX, false));

  /* INT_<op>_OVERFLOW with mixed types.  */
  #define CHECK_SUM(a, b, overflow)                       \
    VERIFY (INT_ADD_OVERFLOW (a, b) == (overflow));       \
    VERIFY (INT_ADD_OVERFLOW (b, a) == (overflow))
  CHECK_SUM (-1, LONG_MIN, true);
  CHECK_SUM (-1, UINT_MAX, false);
  CHECK_SUM (-1L, INT_MIN, INT_MIN == LONG_MIN);
  CHECK_SUM (0u, -1, true);
  CHECK_SUM (0u, 0, false);
  CHECK_SUM (0u, 1, false);
  CHECK_SUM (1, LONG_MAX, true);
  CHECK_SUM (1, UINT_MAX, true);
  CHECK_SUM (1L, INT_MAX, INT_MAX == LONG_MAX);
  CHECK_SUM (1u, INT_MAX, INT_MAX == UINT_MAX);
  CHECK_SUM (1u, INT_MIN, true);

  VERIFY (! INT_SUBTRACT_OVERFLOW (INT_MAX, 1u));
  VERIFY (! INT_SUBTRACT_OVERFLOW (UINT_MAX, 1));
  VERIFY (! INT_SUBTRACT_OVERFLOW (0u, -1));
  VERIFY (INT_SUBTRACT_OVERFLOW (UINT_MAX, -1));
  VERIFY (INT_SUBTRACT_OVERFLOW (INT_MIN, 1u));
  VERIFY (INT_SUBTRACT_OVERFLOW (-1, 0u));

  #define CHECK_PRODUCT(a, b, overflow)                   \
    VERIFY (INT_MULTIPLY_OVERFLOW (a, b) == (overflow));   \
    VERIFY (INT_MULTIPLY_OVERFLOW (b, a) == (overflow))

  CHECK_PRODUCT (-1, 1u, true);
  CHECK_PRODUCT (-1, INT_MIN, INT_NEGATE_OVERFLOW (INT_MIN));
  CHECK_PRODUCT (-1, UINT_MAX, true);
  CHECK_PRODUCT (-12345, LONG_MAX / -12345 - 1, true);
  CHECK_PRODUCT (-12345, LONG_MAX / -12345, false);
  CHECK_PRODUCT (0, -1, false);
  CHECK_PRODUCT (0, 0, false);
  CHECK_PRODUCT (0, 0u, false);
  CHECK_PRODUCT (0, 1, false);
  CHECK_PRODUCT (0, INT_MAX, false);
  CHECK_PRODUCT (0, INT_MIN, false);
  CHECK_PRODUCT (0, UINT_MAX, false);
  CHECK_PRODUCT (0u, -1, false);
  CHECK_PRODUCT (0u, 0, false);
  CHECK_PRODUCT (0u, 0u, false);
  CHECK_PRODUCT (0u, 1, false);
  CHECK_PRODUCT (0u, INT_MAX, false);
  CHECK_PRODUCT (0u, INT_MIN, false);
  CHECK_PRODUCT (0u, UINT_MAX, false);
  CHECK_PRODUCT (1, INT_MAX, false);
  CHECK_PRODUCT (1, INT_MIN, false);
  CHECK_PRODUCT (1, UINT_MAX, false);
  CHECK_PRODUCT (1u, INT_MIN, true);
  CHECK_PRODUCT (1u, INT_MAX, UINT_MAX < INT_MAX);
  CHECK_PRODUCT (INT_MAX, UINT_MAX, true);
  CHECK_PRODUCT (INT_MAX, ULONG_MAX, true);
  CHECK_PRODUCT (INT_MIN, LONG_MAX / INT_MIN - 1, true);
  CHECK_PRODUCT (INT_MIN, LONG_MAX / INT_MIN, false);
  CHECK_PRODUCT (INT_MIN, UINT_MAX, true);
  CHECK_PRODUCT (INT_MIN, ULONG_MAX, true);

  VERIFY (INT_DIVIDE_OVERFLOW (INT_MIN, -1L)
          == (TYPE_TWOS_COMPLEMENT (long int) && INT_MIN == LONG_MIN));
  VERIFY (! INT_DIVIDE_OVERFLOW (INT_MIN, UINT_MAX));
  VERIFY (! INT_DIVIDE_OVERFLOW (INTMAX_MIN, UINTMAX_MAX));
  VERIFY (! INT_DIVIDE_OVERFLOW (INTMAX_MIN, UINT_MAX));
  VERIFY (INT_DIVIDE_OVERFLOW (-11, 10u));
  VERIFY (INT_DIVIDE_OVERFLOW (-10, 10u));
  VERIFY (! INT_DIVIDE_OVERFLOW (-9, 10u));
  VERIFY (INT_DIVIDE_OVERFLOW (11u, -10));
  VERIFY (INT_DIVIDE_OVERFLOW (10u, -10));
  VERIFY (! INT_DIVIDE_OVERFLOW (9u, -10));

  VERIFY (INT_REMAINDER_OVERFLOW (INT_MIN, -1L)
          == (TYPE_TWOS_COMPLEMENT (long int) && INT_MIN == LONG_MIN));
  VERIFY (INT_REMAINDER_OVERFLOW (-1, UINT_MAX));
  VERIFY (INT_REMAINDER_OVERFLOW ((intmax_t) -1, UINTMAX_MAX));
  VERIFY (INT_REMAINDER_OVERFLOW (INTMAX_MIN, UINT_MAX)
          == (INTMAX_MAX < UINT_MAX
              && - (unsigned int) INTMAX_MIN % UINT_MAX != 0));
  VERIFY (INT_REMAINDER_OVERFLOW (INT_MIN, ULONG_MAX)
          == (INT_MIN % ULONG_MAX != 1));
  VERIFY (! INT_REMAINDER_OVERFLOW (1u, -1));
  VERIFY (! INT_REMAINDER_OVERFLOW (37*39u, -39));
  VERIFY (INT_REMAINDER_OVERFLOW (37*39u + 1, -39));
  VERIFY (INT_REMAINDER_OVERFLOW (37*39u - 1, -39));
  VERIFY (! INT_REMAINDER_OVERFLOW (LONG_MAX, -INT_MAX));

  return 0;
}
