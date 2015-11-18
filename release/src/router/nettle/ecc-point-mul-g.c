/* ecc-point-mul-g.c

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

#include <assert.h>

#include "ecc.h"
#include "ecc-internal.h"
#include "nettle-internal.h"

void
ecc_point_mul_g (struct ecc_point *r, const struct ecc_scalar *n)
{
  TMP_DECL(scratch, mp_limb_t, 3*ECC_MAX_SIZE + ECC_MUL_G_ITCH (ECC_MAX_SIZE));
  const struct ecc_curve *ecc = r->ecc;
  mp_limb_t size = ecc->p.size;
  mp_size_t itch = 3*size + ecc->mul_g_itch;

  assert (n->ecc == ecc);

  TMP_ALLOC (scratch, itch);

  ecc->mul_g (ecc, scratch, n->p, scratch + 3*size);
  ecc->h_to_a (ecc, 0, r->p, scratch, scratch + 3*size);
}
