#include "testutils.h"

static void
ref_mod (mp_limb_t *rp, const mp_limb_t *ap, const mp_limb_t *mp, mp_size_t mn)
{
  mpz_t r, a, m;
  mpz_init (r);
  mpz_mod (r, mpz_roinit_n (a, ap, 2*mn), mpz_roinit_n (m, mp, mn));
  mpz_limbs_copy (rp, r, mn);

  mpz_clear (r);
}

#define MAX_ECC_SIZE (1 + 521 / GMP_NUMB_BITS)
#define MAX_SIZE (2*MAX_ECC_SIZE)
#define COUNT 50000

static void
test_modulo (gmp_randstate_t rands, const char *name,
	     const struct ecc_modulo *m)
{
  mp_limb_t a[MAX_SIZE];
  mp_limb_t t[MAX_SIZE];
  mp_limb_t ref[MAX_SIZE];
  mpz_t r;
  unsigned j;

  mpz_init (r);
  
  for (j = 0; j < COUNT; j++)
    {
      if (j & 1)
	mpz_rrandomb (r, rands, 2*m->size * GMP_NUMB_BITS);
      else
	mpz_urandomb (r, rands, 2*m->size * GMP_NUMB_BITS);

      mpz_limbs_copy (a, r, 2*m->size);

      ref_mod (ref, a, m->m, m->size);

      mpn_copyi (t, a, 2*m->size);
      m->mod (m, t);
      if (mpn_cmp (t, m->m, m->size) >= 0)
	mpn_sub_n (t, t, m->m, m->size);

      if (mpn_cmp (t, ref, m->size))
	{
	  fprintf (stderr, "m->mod %s failed: bit_size = %u\n",
		   name, m->bit_size);

	  fprintf (stderr, "a   = ");
	  mpn_out_str (stderr, 16, a, 2*m->size);
	  fprintf (stderr, "\nt   = ");
	  mpn_out_str (stderr, 16, t, m->size);
	  fprintf (stderr, " (bad)\nref = ");
	  mpn_out_str (stderr, 16, ref, m->size);
	  fprintf (stderr, "\n");
	  abort ();
	}

      if (m->B_size < m->size)
	{
	  mpn_copyi (t, a, 2*m->size);
	  ecc_mod (m, t);
	  if (mpn_cmp (t, m->m, m->size) >= 0)
	    mpn_sub_n (t, t, m->m, m->size);

	  if (mpn_cmp (t, ref, m->size))
	    {
	      fprintf (stderr, "ecc_mod %s failed: bit_size = %u\n",
		       name, m->bit_size);
	      fprintf (stderr, "a   = ");
	      mpn_out_str (stderr, 16, a, 2*m->size);
	      fprintf (stderr, "\nt   = ");
	      mpn_out_str (stderr, 16, t, m->size);
	      fprintf (stderr, " (bad)\nref = ");
	      mpn_out_str (stderr, 16, ref, m->size);
	      fprintf (stderr, "\n");
	      abort ();
	    }
	}
    }
  mpz_clear (r);
}

void
test_main (void)
{
  gmp_randstate_t rands;
  unsigned i;

  gmp_randinit_default (rands);
  
  for (i = 0; ecc_curves[i]; i++)
    {
      test_modulo (rands, "p", &ecc_curves[i]->p);
      test_modulo (rands, "q", &ecc_curves[i]->q);
    }
  gmp_randclear (rands);
}
