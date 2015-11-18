/* ecc-a-to-j.c

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
ecc_a_to_j (const struct ecc_curve *ecc,
	    mp_limb_t *r, const mp_limb_t *p)
{
  if (ecc->use_redc)
    {
      mpn_copyd (r + ecc->p.size, p, 2*ecc->p.size);

      mpn_zero (r, ecc->p.size);
      ecc->p.mod (&ecc->p, r);

      mpn_zero (r + ecc->p.size, ecc->p.size);
      ecc->p.mod (&ecc->p, r + ecc->p.size);
    }
  else if (r != p)
    mpn_copyi (r, p, 2*ecc->p.size);

  mpn_copyi (r + 2*ecc->p.size, ecc->unit, ecc->p.size);
}
