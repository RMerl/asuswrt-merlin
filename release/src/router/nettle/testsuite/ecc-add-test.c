#include "testutils.h"

void
test_main (void)
{
  unsigned i;

  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      mp_limb_t *g = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *g2 = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *g3 = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *p = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *scratch = xalloc_limbs (ECC_ADD_JJJ_ITCH(ecc->p.size));

      if (ecc->p.bit_size == 255)
	{
	  mp_limb_t *z = xalloc_limbs (ecc_size_j (ecc));
	  /* Zero point has x = 0, y = 1, z = 1 */
	  mpn_zero (z, 3*ecc->p.size);
	  z[ecc->p.size] = z[2*ecc->p.size] = 1;
	  
	  ecc_a_to_j (ecc, g, ecc->g);

	  ecc_add_ehh (ecc, p, z, z, scratch);
	  test_ecc_mul_h (i, 0, p);

	  ecc_add_eh (ecc, p, z, z, scratch);
	  test_ecc_mul_h (i, 0, p);

	  ecc_add_ehh (ecc, p, g, p, scratch);
	  test_ecc_mul_h (i, 1, p);

	  ecc_add_eh (ecc, p, z, g, scratch);
	  test_ecc_mul_h (i, 1, p);

	  ecc_add_ehh (ecc, g2, g, p, scratch);
	  test_ecc_mul_h (i, 2, g2);

	  ecc_add_eh (ecc, g2, g, g, scratch);
	  test_ecc_mul_h (i, 2, g2);

	  ecc_add_ehh (ecc, g3, g, g2, scratch);
	  test_ecc_mul_h (i, 3, g3);

	  ecc_add_eh (ecc, g3, g2, g, scratch);
	  test_ecc_mul_h (i, 3, g3);

	  ecc_add_ehh (ecc, p, g, g3, scratch);
	  test_ecc_mul_h (i, 4, p);

	  ecc_add_eh (ecc, p, g3, g, scratch);
	  test_ecc_mul_h (i, 4, p);

	  ecc_add_ehh (ecc, p, g2, g2, scratch);
	  test_ecc_mul_h (i, 4, p);

	  free (z);
	}
      else
	{
	  ecc_a_to_j (ecc, g, ecc->g);

	  ecc_dup_jj (ecc, g2, g, scratch);
	  test_ecc_mul_h (i, 2, g2);

	  ecc_add_jjj (ecc, g3, g, g2, scratch);
	  test_ecc_mul_h (i, 3, g3);

	  ecc_add_jjj (ecc, g3, g2, g, scratch);
	  test_ecc_mul_h (i, 3, g3);

	  ecc_add_jjj (ecc, p, g, g3, scratch);
	  test_ecc_mul_h (i, 4, p);

	  ecc_add_jjj (ecc, p, g3, g, scratch);
	  test_ecc_mul_h (i, 4, p);

	  ecc_dup_jj (ecc, p, g2, scratch);
	  test_ecc_mul_h (i, 4, p);
	}
      free (g);
      free (g2);
      free (g3);
      free (p);
      free (scratch);
    }
}
