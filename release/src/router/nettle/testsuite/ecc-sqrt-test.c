/* ecc-sqrt.c

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

#define COUNT 5000

#if NETTLE_USE_MINI_GMP
/* Implements Legendre symbol only, requiring that p is an odd prime */
static int
mpz_ui_kronecker (mp_limb_t ul, const mpz_t p)
{
  mpz_t t, u;
  int r;

  mpz_init_set_ui (u, ul);
  mpz_init_set (t, p);
  mpz_sub_ui (t, t, 1);
  mpz_tdiv_q_2exp (t, t, 1);
  mpz_powm (t, u, t, p);

  r = mpz_cmp_ui (t, 1);
  if (r < 0)
    r = 0;
  else if (r == 0)
    r = 1;
  else
    {
      mpz_sub (t, p, t);
      ASSERT (mpz_cmp_ui (t, 1) == 0);
      r = -1;
    }
  mpz_clear (t);
  mpz_clear (u);

  return r;
}
#endif /* NETTLE_USE_MINI_GMP */

static void
test_modulo (gmp_randstate_t rands, const struct ecc_modulo *m)
{
  mpz_t u;
  mpz_t v;
  mpz_t p;
  mpz_t r;
  mpz_t t;

  unsigned z, i;
  mp_limb_t *up;
  mp_limb_t *vp;
  mp_limb_t *rp;
  mp_limb_t *scratch;

  mpz_init (u);
  mpz_init (v);
  mpz_init (t);

  mpz_roinit_n (p, m->m, m->size);

  up = xalloc_limbs (m->size);
  vp = xalloc_limbs (m->size);
  rp = xalloc_limbs (2*m->size);
  scratch = xalloc_limbs (m->sqrt_itch);

  /* Find a non-square */
  for (z = 2; mpz_ui_kronecker (z, p) != -1; z++)
    ;

  if (verbose)
    fprintf(stderr, "Non square: %d\n", z);

  for (i = 0; i < COUNT; i++)
    {
      if (i & 1)
	{
	  mpz_rrandomb (u, rands, m->bit_size);
	  mpz_rrandomb (v, rands, m->bit_size);
	}
      else
	{
	  mpz_urandomb (u, rands, m->bit_size);
	  mpz_urandomb (v, rands, m->bit_size);
	}
      mpz_limbs_copy (up, u, m->size);
      mpz_limbs_copy (vp, v, m->size);
      if (!m->sqrt (m, rp, up, vp, scratch))
	{
	  mpz_mul_ui (u, u, z);
	  mpz_mod (u, u, p);
	  mpz_limbs_copy (up, u, m->size);
	  if (!m->sqrt (m, rp, up, vp, scratch))
	    {
	      fprintf (stderr, "m->sqrt returned failure, bit_size = %d\n"
		       "u = 0x",
		       m->bit_size);
	      mpz_out_str (stderr, 16, u);
	      fprintf (stderr, "\nv = 0x");
	      mpz_out_str (stderr, 16, v);
	      fprintf (stderr, "\n");
	      abort ();
	    }
	}
      /* Check that r^2 v = u */
      mpz_roinit_n (r, rp, m->size);
      mpz_mul (t, r, r);
      mpz_mul (t, t, v);
      if (!mpz_congruent_p (t, u, p))
	{
	  fprintf (stderr, "m->sqrt gave incorrect result, bit_size = %d\n"
		   "u = 0x",
		   m->bit_size);
	  mpz_out_str (stderr, 16, u);
	  fprintf (stderr, "\nv = 0x");
	  mpz_out_str (stderr, 16, v);
	  fprintf (stderr, "\nr = 0x");
	  mpz_out_str (stderr, 16, r);
	  fprintf (stderr, "\n");
	  abort ();
	}
    }
  mpz_clear (u);
  mpz_clear (v);
  mpz_clear (t);
  free (up);
  free (vp);
  free (rp);
  free (scratch);
}

void
test_main (void)
{
  gmp_randstate_t rands;
  unsigned i;

  gmp_randinit_default (rands);
  for (i = 0; ecc_curves[i]; i++)
    {
      if (ecc_curves[i]->p.sqrt)
	test_modulo (rands, &ecc_curves[i]->p);
    }
  gmp_randclear (rands);
}
