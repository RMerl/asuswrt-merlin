/* curve25519-dh-test.c

   Copyright (C) 2014 Niels Möller

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

#include "testutils.h"

#include "curve25519.h"

static void
test_g (const uint8_t *s, const uint8_t *r)
{
  uint8_t p[CURVE25519_SIZE];
  curve25519_mul_g (p, s);
  if (!MEMEQ (CURVE25519_SIZE, p, r))
    {
      printf ("curve25519_mul_g failure:\ns = ");
      print_hex (CURVE25519_SIZE, s);
      printf ("\np = ");
      print_hex (CURVE25519_SIZE, p);
      printf (" (bad)\nr = ");
      print_hex (CURVE25519_SIZE, r);
      printf (" (expected)\n");
      abort ();
    }
}

static void
test_a (const uint8_t *s, const uint8_t *b, const uint8_t *r)
{
  uint8_t p[CURVE25519_SIZE];
  curve25519_mul (p, s, b);
    
  if (!MEMEQ (CURVE25519_SIZE, p, r))
    {
      printf ("curve25519_mul failure:\ns = ");
      print_hex (CURVE25519_SIZE, s);
      printf ("\nb = ");
      print_hex (CURVE25519_SIZE, b);
      printf ("\np = ");
      print_hex (CURVE25519_SIZE, p);
      printf (" (bad)\nr = ");
      print_hex (CURVE25519_SIZE, r);
      printf (" (expected)\n");
      abort ();
    }
}

void
test_main (void)
{
  /* From draft-turner-thecurve25519function-00 (same also in
     draft-josefsson-tls-curve25519-05, but the latter uses different
     endianness). */
  test_g (H("77076d0a7318a57d3c16c17251b26645"
	    "df4c2f87ebc0992ab177fba51db92c2a"),
	  H("8520f0098930a754748b7ddcb43ef75a"
	    "0dbf3a0d26381af4eba4a98eaa9b4e6a"));
  test_g (H("5dab087e624a8a4b79e17f8b83800ee6"
	    "6f3bb1292618b6fd1c2f8b27ff88e0eb"),
	  H("de9edb7d7b7dc1b4d35b61c2ece43537"
	    "3f8343c85b78674dadfc7e146f882b4f"));

  test_a (H("77076d0a7318a57d3c16c17251b26645"
	    "df4c2f87ebc0992ab177fba51db92c2a"),
	  H("de9edb7d7b7dc1b4d35b61c2ece43537"
	    "3f8343c85b78674dadfc7e146f882b4f"),
	  H("4a5d9d5ba4ce2de1728e3bf480350f25"
	    "e07e21c947d19e3376f09b3c1e161742"));

  test_a (H("5dab087e624a8a4b79e17f8b83800ee6"
	    "6f3bb1292618b6fd1c2f8b27ff88e0eb"),
	  H("8520f0098930a754748b7ddcb43ef75a"
	    "0dbf3a0d26381af4eba4a98eaa9b4e6a"),
	  H("4a5d9d5ba4ce2de1728e3bf480350f25"
	    "e07e21c947d19e3376f09b3c1e161742"));
}
