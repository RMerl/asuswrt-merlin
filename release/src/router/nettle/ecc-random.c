/* ecc-random.c

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

static int
zero_p (const struct ecc_modulo *m,
	const mp_limb_t *xp)
{
  mp_limb_t t;
  mp_size_t i;

  for (i = t = 0; i < m->size; i++)
    t |= xp[i];

  return t == 0;
}

static int
ecdsa_in_range (const struct ecc_modulo *m,
		const mp_limb_t *xp, mp_limb_t *scratch)
{
  /* Check if 0 < x < q, with data independent timing. */
  return !zero_p (m, xp)
    & (mpn_sub_n (scratch, xp, m->m, m->size) != 0);
}

void
ecc_mod_random (const struct ecc_modulo *m, mp_limb_t *xp,
		void *ctx, nettle_random_func *random, mp_limb_t *scratch)
{
  uint8_t *buf = (uint8_t *) scratch;
  unsigned nbytes = (m->bit_size + 7)/8;

  /* The bytes ought to fit in the scratch area, unless we have very
     unusual limb and byte sizes. */
  assert (nbytes <= m->size * sizeof (mp_limb_t));

  do
    {
      random (ctx, nbytes, buf);
      buf[0] &= 0xff >> (nbytes * 8 - m->bit_size);

      mpn_set_base256 (xp, m->size, buf, nbytes);
    }
  while (!ecdsa_in_range (m, xp, scratch));
}

void
ecc_scalar_random (struct ecc_scalar *x,
		   void *random_ctx, nettle_random_func *random)
{
  TMP_DECL (scratch, mp_limb_t, ECC_MOD_RANDOM_ITCH (ECC_MAX_SIZE));
  TMP_ALLOC (scratch, ECC_MOD_RANDOM_ITCH (x->ecc->q.size));

  ecc_mod_random (&x->ecc->q, x->p, random_ctx, random, scratch);
}
