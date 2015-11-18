/* eddsa-compress-test.c

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

#include "eddsa.h"

#define COUNT 1000

void test_main (void)
{
  const struct ecc_curve *ecc = &_nettle_curve25519;
  gmp_randstate_t rands;
  mp_size_t size, itch;
  mpz_t zp, t;
  mp_limb_t *s;
  mp_limb_t *p;
  mp_limb_t *pa1;
  mp_limb_t *pa2;
  mp_limb_t *scratch;
  size_t clen;
  uint8_t *c;
  unsigned j;

  gmp_randinit_default (rands);

  size = ecc_size (ecc);
  clen = 1 + ecc->p.bit_size / 8;

  mpz_roinit_n (zp, ecc->p.m, size);

  mpz_init (t);
  s = xalloc_limbs (size);
  p = xalloc_limbs (ecc_size_j (ecc));
  pa1 = xalloc_limbs (ecc_size_a (ecc));
  pa2 = xalloc_limbs (ecc_size_a (ecc));
  c = xalloc (clen);

  itch = _eddsa_decompress_itch (ecc);
  if (itch < ecc->mul_g_itch)
    itch = ecc->mul_g_itch;

  scratch = xalloc_limbs (itch);

  for (j = 0; j < COUNT; j++)
    {
      mpz_t x1, y1, x2, y2;

      mpz_urandomb (t, rands, ecc->q.bit_size);
      mpz_limbs_copy (s, t, ecc->q.size);
      ecc->mul_g (ecc, p, s, scratch);
      _eddsa_compress (ecc, c, p, scratch);
      ecc->h_to_a (ecc, 0, pa1, p, scratch);
      _eddsa_decompress (ecc, pa2, c, scratch);
      mpz_roinit_n (x1, pa1, size);
      mpz_roinit_n (y1, pa1 + size, size);
      mpz_roinit_n (x2, pa2, size);
      mpz_roinit_n (y2, pa2 + size, size);
      if (!(mpz_congruent_p (x1, x2, zp)
	    && mpz_congruent_p (y1, y2, zp)))
	{
	  fprintf (stderr, "eddsa compression failed:\nc = ");
	  print_hex (clen, c);
	  fprintf (stderr, "\np1 = 0x");
	  mpz_out_str (stderr, 16, x1);
	  fprintf (stderr, ",\n     0x");
	  mpz_out_str (stderr, 16, y1);
	  fprintf (stderr, "\np2 = 0x");
	  mpz_out_str (stderr, 16, x2);
	  fprintf (stderr, ",\n     0x");
	  mpz_out_str (stderr, 16, y2);
	  fprintf (stderr, "\n");
	  abort ();
	}
    }
  mpz_clear (t);
  free (s);
  free (p);
  free (c);
  free (pa1);
  free (pa2);
  free (scratch);
  gmp_randclear (rands);
}
