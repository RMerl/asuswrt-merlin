#include "testutils.h"

void
test_main (void)
{
  unsigned i;

  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      mp_limb_t *g = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *p = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *scratch = xalloc_limbs (ECC_DUP_EH_ITCH(ecc->p.size));;

      if (ecc->p.bit_size == 255)
	{
	  mp_limb_t *z = xalloc_limbs (ecc_size_j (ecc));
	  /* Zero point has x = 0, y = 1, z = 1 */
	  mpn_zero (z, 3*ecc->p.size);
	  z[ecc->p.size] = z[2*ecc->p.size] = 1;
	  
	  ecc_a_to_j (ecc, g, ecc->g);

	  ecc_dup_eh (ecc, p, z, scratch);
	  test_ecc_mul_h (i, 0, p);

	  ecc_dup_eh (ecc, p, g, scratch);
	  test_ecc_mul_h (i, 2, p);

	  ecc_dup_eh (ecc, p, p, scratch);
	  test_ecc_mul_h (i, 4, p);
	  free (z);
	}
      else
	{
	  ecc_a_to_j (ecc, g, ecc->g);

	  ecc_dup_jj (ecc, p, g, scratch);
	  test_ecc_mul_h (i, 2, p);

	  ecc_dup_jj (ecc, p, p, scratch);
	  test_ecc_mul_h (i, 4, p);
	}
      free (p);
      free (g);
      free (scratch);
    }
}
