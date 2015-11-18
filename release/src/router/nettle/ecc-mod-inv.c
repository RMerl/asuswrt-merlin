/* ecc-mod-inv.c

   Copyright (C) 2013, 2014 Niels Möller

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

#include "ecc-internal.h"

static void
cnd_neg (int cnd, mp_limb_t *rp, const mp_limb_t *ap, mp_size_t n)
{
  mp_limb_t cy = (cnd != 0);
  mp_limb_t mask = -cy;
  mp_size_t i;

  for (i = 0; i < n; i++)
    {
      mp_limb_t r = (ap[i] ^ mask) + cy;
      cy = r < cy;
      rp[i] = r;
    }
}

/* Compute a^{-1} mod m, with running time depending only on the size.
   Returns zero if a == 0 (mod m), to be consistent with a^{phi(m)-1}.
   Also needs (m+1)/2, and m must be odd.

   Needs 2n limbs available at rp, and 2n additional scratch limbs.
*/

/* FIXME: Could use mpn_sec_invert (in GMP-6), but with a bit more
   scratch need since it doesn't precompute (m+1)/2. */
void
ecc_mod_inv (const struct ecc_modulo *m,
	     mp_limb_t *vp, const mp_limb_t *in_ap,
	     mp_limb_t *scratch)
{
#define ap scratch
#define bp (scratch + n)
#define up (vp + n)

  mp_size_t n = m->size;
  /* Avoid the mp_bitcnt_t type for compatibility with older GMP
     versions. */  
  unsigned i;

  /* Maintain

       a = u * orig_a (mod m)
       b = v * orig_a (mod m)

     and b odd at all times. Initially,

       a = a_orig, u = 1
       b = m,      v = 0
     */

  assert (ap != vp);

  up[0] = 1;
  mpn_zero (up+1, n - 1);
  mpn_copyi (bp, m->m, n);
  mpn_zero (vp, n);
  mpn_copyi (ap, in_ap, n);

  for (i = m->bit_size + GMP_NUMB_BITS * n; i-- > 0; )
    {
      mp_limb_t odd, swap, cy;
      
      /* Always maintain b odd. The logic of the iteration is as
	 follows. For a, b:

	   odd = a & 1
	   a -= odd * b
	   if (underflow from a-b)
	     {
	       b += a, assigns old a
	       a = B^n-a
	     }
	   
	   a /= 2

	 For u, v:

	   if (underflow from a - b)
	     swap u, v
	   u -= odd * v
	   if (underflow from u - v)
	     u += m

	   u /= 2
	   if (a one bit was shifted out)
	     u += (m+1)/2

	 As long as a > 0, the quantity

	   (bitsize of a) + (bitsize of b)

	 is reduced by at least one bit per iteration, hence after
         (bit_size of orig_a) + (bit_size of m) - 1 iterations we
         surely have a = 0. Then b = gcd(orig_a, m) and if b = 1 then
         also v = orig_a^{-1} (mod m)
      */

      assert (bp[0] & 1);
      odd = ap[0] & 1;

      swap = cnd_sub_n (odd, ap, bp, n);
      cnd_add_n (swap, bp, ap, n);
      cnd_neg (swap, ap, ap, n);

      cnd_swap (swap, up, vp, n);
      cy = cnd_sub_n (odd, up, vp, n);
      cy -= cnd_add_n (cy, up, m->m, n);

      cy = mpn_rshift (ap, ap, n, 1);
      assert (cy == 0);
      cy = mpn_rshift (up, up, n, 1);
      cy = cnd_add_n (cy, up, m->mp1h, n);
      assert (cy == 0);
    }
  assert ( (ap[0] | ap[n-1]) == 0);
#undef ap
#undef bp
#undef up
}
