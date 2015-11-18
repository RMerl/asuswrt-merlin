/* ecc-scalar.c

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

/* Development of Nettle's ECC support was funded by the .SE Internet Fund. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "ecc.h"
#include "ecc-internal.h"

void
ecc_scalar_init (struct ecc_scalar *s, const struct ecc_curve *ecc)
{
  s->ecc = ecc;
  s->p = gmp_alloc_limbs (ecc->p.size);
}

void
ecc_scalar_clear (struct ecc_scalar *s)
{
  gmp_free_limbs (s->p, s->ecc->p.size);
}

int
ecc_scalar_set (struct ecc_scalar *s, const mpz_t z)
{
  mp_size_t size = s->ecc->p.size;

  if (mpz_sgn (z) <= 0 || mpz_limbs_cmp (z, s->ecc->q.m, size) >= 0)
    return 0;

  mpz_limbs_copy (s->p, z, size);
  return 1;
}

void
ecc_scalar_get (const struct ecc_scalar *s, mpz_t z)
{
  mpz_set_n (z, s->p, s->ecc->p.size);  
}
