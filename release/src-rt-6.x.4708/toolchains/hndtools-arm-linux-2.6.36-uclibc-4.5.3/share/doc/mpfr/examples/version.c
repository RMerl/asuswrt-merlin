/*
 * Output various information about GMP and MPFR.
 */

/*
Copyright 2010, 2011 Free Software Foundation, Inc.
Contributed by the Arenaire and Cacao projects, INRIA.

This file is part of the GNU MPFR Library.

The GNU MPFR Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

The GNU MPFR Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MPFR Library; see the file COPYING.LESSER.  If not, see
http://www.gnu.org/licenses/ or write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <stdio.h>
#include <limits.h>
#include <gmp.h>
#include <mpfr.h>

/* The following failure can occur if GMP has been rebuilt with
 * a different ABI, e.g.
 *   1. GMP built with ABI=mode32.
 *   2. MPFR built against this GMP version.
 *   3. GMP rebuilt with ABI=32.
 */
static void failure_test (void)
{
  mpfr_t x;

  mpfr_init2 (x, 128);
  mpfr_set_str (x, "17", 0, GMP_RNDN);
  if (mpfr_cmp_ui (x, 17) != 0)
    printf ("\nFailure in mpfr_set_str! Probably an unmatched ABI!\n");
  mpfr_clear (x);
}

int main (void)
{
  unsigned long c;
  mp_limb_t t[4] = { -1, -1, -1, -1 };

#if defined(__cplusplus)
  printf ("A C++ compiler is used.\n");
#endif

  printf ("GMP .....  Library: %-12s  Header: %d.%d.%d\n",
          gmp_version, __GNU_MP_VERSION, __GNU_MP_VERSION_MINOR,
          __GNU_MP_VERSION_PATCHLEVEL);

  printf ("MPFR ....  Library: %-12s  Header: %s (based on %d.%d.%d)\n",
          mpfr_get_version (), MPFR_VERSION_STRING, MPFR_VERSION_MAJOR,
          MPFR_VERSION_MINOR, MPFR_VERSION_PATCHLEVEL);
  printf ("MPFR patches: %s\n\n", mpfr_get_patches ());

#ifdef __GMP_CC
  printf ("__GMP_CC = \"%s\"\n", __GMP_CC);
#endif
#ifdef __GMP_CFLAGS
  printf ("__GMP_CFLAGS = \"%s\"\n", __GMP_CFLAGS);
#endif
  printf ("GMP_LIMB_BITS     = %d\n", (int) GMP_LIMB_BITS);
  printf ("GMP_NAIL_BITS     = %d\n", (int) GMP_NAIL_BITS);
  printf ("GMP_NUMB_BITS     = %d\n", (int) GMP_NUMB_BITS);
  printf ("mp_bits_per_limb  = %d\n", (int) mp_bits_per_limb);
  printf ("sizeof(mp_limb_t) = %d\n", (int) sizeof(mp_limb_t));
  if (mp_bits_per_limb != GMP_LIMB_BITS)
    printf ("Warning! mp_bits_per_limb != GMP_LIMB_BITS\n");
  if (GMP_LIMB_BITS != sizeof(mp_limb_t) * CHAR_BIT)
    printf ("Warning! GMP_LIMB_BITS != sizeof(mp_limb_t) * CHAR_BIT\n");

  c = mpn_popcount (t, 1);
  printf ("The GMP library expects %lu bits in a mp_limb_t.\n", c);
  if (c != GMP_LIMB_BITS)
    printf ("Warning! This is different from GMP_LIMB_BITS!\n"
            "Different ABI caused by a GMP library upgrade?\n");

  failure_test ();

  return 0;
}
