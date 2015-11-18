/* ecc-mul-a.c

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

/* Binary algorithm needs 6*ecc->p.size + scratch for ecc_add_jja.
   Current total is 12 ecc->p.size, at most 864 bytes.

   Window algorithm needs (3<<w) * ecc->p.size for the table,
   3*ecc->p.size for a temporary point, and scratch for
   ecc_add_jjj. */

#if ECC_MUL_A_WBITS == 0
void
ecc_mul_a (const struct ecc_curve *ecc,
	   mp_limb_t *r,
	   const mp_limb_t *np, const mp_limb_t *p,
	   mp_limb_t *scratch)
{
#define tp scratch
#define pj (scratch + 3*ecc->p.size)
#define scratch_out (scratch + 6*ecc->p.size)

  int is_zero;

  unsigned i;

  ecc_a_to_j (ecc, pj, p);
  mpn_zero (r, 3*ecc->p.size);
  
  for (i = ecc->p.size, is_zero = 1; i-- > 0; )
    {
      mp_limb_t w = np[i];
      mp_limb_t bit;

      for (bit = (mp_limb_t) 1 << (GMP_NUMB_BITS - 1);
	   bit > 0;
	   bit >>= 1)
	{
	  int digit;

	  ecc_dup_jj (ecc, r, r, scratch_out);
	  ecc_add_jja (ecc, tp, r, pj, scratch_out);

	  digit = (w & bit) > 0;
	  /* If is_zero is set, r is the zero point,
	     and ecc_add_jja produced garbage. */
	  cnd_copy (is_zero, tp, pj, 3*ecc->p.size);
	  is_zero &= ~digit;
	  /* If we had a one-bit, use the sum. */
	  cnd_copy (digit, r, tp, 3*ecc->p.size);
	}
    }
}
#else /* ECC_MUL_A_WBITS > 1 */

#define TABLE_SIZE (1U << ECC_MUL_A_WBITS)
#define TABLE_MASK (TABLE_SIZE - 1)

#define TABLE(j) (table + (j) * 3*ecc->p.size)

static void
table_init (const struct ecc_curve *ecc,
	    mp_limb_t *table, unsigned bits,
	    const mp_limb_t *p,
	    mp_limb_t *scratch)
{
  unsigned size = 1 << bits;
  unsigned j;

  mpn_zero (TABLE(0), 3*ecc->p.size);
  ecc_a_to_j (ecc, TABLE(1), p);

  for (j = 2; j < size; j += 2)
    {
      ecc_dup_jj (ecc, TABLE(j), TABLE(j/2), scratch);
      ecc_add_jja (ecc, TABLE(j+1), TABLE(j), TABLE(1), scratch);
    }  
}

void
ecc_mul_a (const struct ecc_curve *ecc,
	   mp_limb_t *r,
	   const mp_limb_t *np, const mp_limb_t *p,
	   mp_limb_t *scratch)
{
#define tp scratch
#define table (scratch + 3*ecc->p.size)
  mp_limb_t *scratch_out = table + (3*ecc->p.size << ECC_MUL_A_WBITS);
  int is_zero = 0;

  /* Avoid the mp_bitcnt_t type for compatibility with older GMP
     versions. */
  unsigned blocks = (ecc->p.bit_size + ECC_MUL_A_WBITS - 1) / ECC_MUL_A_WBITS;
  unsigned bit_index = (blocks-1) * ECC_MUL_A_WBITS;

  mp_size_t limb_index = bit_index / GMP_NUMB_BITS;
  unsigned shift = bit_index % GMP_NUMB_BITS;
  mp_limb_t w, bits;

  table_init (ecc, table, ECC_MUL_A_WBITS, p, scratch_out);

  w = np[limb_index];
  bits = w >> shift;
  if (limb_index < ecc->p.size - 1)
    bits |= np[limb_index + 1] << (GMP_NUMB_BITS - shift);

  assert (bits < TABLE_SIZE);

  sec_tabselect (r, 3*ecc->p.size, table, TABLE_SIZE, bits);
  is_zero = (bits == 0);

  for (;;)
    {
      unsigned j;
      if (shift >= ECC_MUL_A_WBITS)
	{
	  shift -= ECC_MUL_A_WBITS;
	  bits = w >> shift;
	}
      else
	{
	  if (limb_index == 0)
	    {
	      assert (shift == 0);
	      break;
	    }
	  bits = w << (ECC_MUL_A_WBITS - shift);
	  w = np[--limb_index];
	  shift = shift + GMP_NUMB_BITS - ECC_MUL_A_WBITS;
	  bits |= w >> shift;
	}
      for (j = 0; j < ECC_MUL_A_WBITS; j++)
	ecc_dup_jj (ecc, r, r, scratch_out);

      bits &= TABLE_MASK;
      sec_tabselect (tp, 3*ecc->p.size, table, TABLE_SIZE, bits);
      cnd_copy (is_zero, r, tp, 3*ecc->p.size);
      ecc_add_jjj (ecc, tp, tp, r, scratch_out);

      /* Use the sum when valid. ecc_add_jja produced garbage if
	 is_zero != 0 or bits == 0, . */	  
      cnd_copy (bits & (is_zero - 1), r, tp, 3*ecc->p.size);
      is_zero &= (bits == 0);
    }
#undef table
#undef tp
}

#endif /* ECC_MUL_A_WBITS > 1 */
