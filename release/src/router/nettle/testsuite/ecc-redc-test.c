#include "testutils.h"

static void
ref_redc (mp_limb_t *rp, const mp_limb_t *ap, const mp_limb_t *mp, mp_size_t mn)
{
  mpz_t t;
  mpz_t m, a;
  mp_size_t an;

  mpz_init (t);
  mpz_setbit (t, mn * GMP_NUMB_BITS);

  mpz_roinit_n (m, mp, mn);

  an = 2*mn;
  while (an > 0 && ap[an-1] == 0)
    an--;

  mpz_roinit_n (a, ap, an);
  
  mpz_invert (t, t, m);
  mpz_mul (t, t, a);
  mpz_mod (t, t, m);

  mpz_limbs_copy (rp, t, mn);

  mpz_clear (t);
}

#define MAX_ECC_SIZE (1 + 521 / GMP_NUMB_BITS)
#define MAX_SIZE (2*MAX_ECC_SIZE)
#define COUNT 50000

void
test_main (void)
{
  gmp_randstate_t rands;
  mp_limb_t a[MAX_SIZE];
  mp_limb_t m[MAX_SIZE];
  mp_limb_t ref[MAX_SIZE];
  unsigned i;
  mpz_t r;

  gmp_randinit_default (rands);
  
  mpz_init (r);
  
  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      unsigned j;

      for (j = 0; j < COUNT; j++)
	{
	  if (j & 1)
	    mpz_rrandomb (r, rands, 2*ecc->p.size * GMP_NUMB_BITS);
	  else
	    mpz_urandomb (r, rands, 2*ecc->p.size * GMP_NUMB_BITS);

	  mpz_limbs_copy (a, r, 2*ecc->p.size);

	  ref_redc (ref, a, ecc->p.m, ecc->p.size);

	  if (ecc->p.reduce != ecc->p.mod)
	    {
	      mpn_copyi (m, a, 2*ecc->p.size);
	      ecc->p.reduce (&ecc->p, m);
	      if (mpn_cmp (m, ecc->p.m, ecc->p.size) >= 0)
		mpn_sub_n (m, m, ecc->p.m, ecc->p.size);

	      if (mpn_cmp (m, ref, ecc->p.size))
		{
		  fprintf (stderr, "ecc->p.reduce failed: bit_size = %u\n",
			   ecc->p.bit_size);
		  fprintf (stderr, "a   = ");
		  mpn_out_str (stderr, 16, a, 2*ecc->p.size);
		  fprintf (stderr, "\nm   = ");
		  mpn_out_str (stderr, 16, m, ecc->p.size);
		  fprintf (stderr, " (bad)\nref   = ");
		  mpn_out_str (stderr, 16, ref, ecc->p.size);
		  fprintf (stderr, "\n");
		  abort ();
		}
	    }
	  if (ecc->p.redc_size != 0)
	    {	  
	      mpn_copyi (m, a, 2*ecc->p.size);
	      if (ecc->p.m[0] == 1)
		ecc_pm1_redc (&ecc->p, m);
	      else
		ecc_pp1_redc (&ecc->p, m);

	      if (mpn_cmp (m, ecc->p.m, ecc->p.size) >= 0)
		mpn_sub_n (m, m, ecc->p.m, ecc->p.size);

	      if (mpn_cmp (m, ref, ecc->p.size))
		{
		  fprintf (stderr, "ecc_p%c1_redc failed: bit_size = %u\n",
			   (ecc->p.m[0] == 1) ? 'm' : 'p', ecc->p.bit_size);
		  fprintf (stderr, "a   = ");
		  mpn_out_str (stderr, 16, a, 2*ecc->p.size);
		  fprintf (stderr, "\nm   = ");
		  mpn_out_str (stderr, 16, m, ecc->p.size);
		  fprintf (stderr, " (bad)\nref = ");
		  mpn_out_str (stderr, 16, ref, ecc->p.size);
		  fprintf (stderr, "\n");
		  abort ();
		}
	    }
	}
    }

  mpz_clear (r);
  gmp_randclear (rands);
}
