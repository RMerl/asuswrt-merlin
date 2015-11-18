#include "testutils.h"

static int
ref_modinv (mp_limb_t *rp, const mp_limb_t *ap, const mp_limb_t *mp, mp_size_t mn)
{
  mpz_t g, s, a, m;
  int res;

  mpz_init (g);
  mpz_init (s);
  mpz_roinit_n (a, ap, mn);
  mpz_roinit_n (m, mp, mn);
  
  mpz_gcdext (g, s, NULL, a, m);
  if (mpz_cmp_ui (g, 1) == 0)
    {
      if (mpz_sgn (s) < 0)
	{
	  mpz_add (s, s, m);
	  ASSERT (mpz_sgn (s) > 0);
	}
      mpz_limbs_copy (rp, s, mn);
      res = 1;
    }
  else
    res = 0;

  mpz_clear (g);
  mpz_clear (s);
  return res;
}

static int
zero_p (const struct ecc_modulo *m, const mp_limb_t *xp)
{
  return mpn_zero_p (xp, m->size)
    || mpn_cmp (xp, m->m, m->size) == 0;
}

#define MAX_ECC_SIZE (1 + 521 / GMP_NUMB_BITS)
#define COUNT 500

static void
test_modulo (gmp_randstate_t rands, const char *name,
	     const struct ecc_modulo *m)
{
  mp_limb_t *a;
  mp_limb_t *ai;
  mp_limb_t *ref;
  mp_limb_t *scratch;
  unsigned j;
  mpz_t r;

  mpz_init (r);

  a = xalloc_limbs (m->size);
  ai = xalloc_limbs (2*m->size);
  ref = xalloc_limbs (m->size);;
  scratch = xalloc_limbs (m->invert_itch);

  /* Check behaviour for zero input */
  mpn_zero (a, m->size);
  memset (ai, 17, m->size * sizeof(*ai));
  m->invert (m, ai, a, scratch);
  if (!zero_p (m, ai))
    {
      fprintf (stderr, "%s->invert failed for zero input (bit size %u):\n",
	       name, m->bit_size);
      fprintf (stderr, "p = ");
      mpn_out_str (stderr, 16, m->m, m->size);
      fprintf (stderr, "\nt = ");
      mpn_out_str (stderr, 16, ai, m->size);
      fprintf (stderr, " (bad)\n");
      abort ();
    }
	  
  /* Check behaviour for a = m */
  memset (ai, 17, m->size * sizeof(*ai));
  m->invert (m, ai, m->m, scratch);
  if (!zero_p (m, ai))
    {
      fprintf (stderr, "%s->invert failed for a = p input (bit size %u):\n",
	       name, m->bit_size);
      
      fprintf (stderr, "p = ");
      mpn_out_str (stderr, 16, m->m, m->size);
      fprintf (stderr, "\nt = ");
      mpn_out_str (stderr, 16, ai, m->size);
      fprintf (stderr, " (bad)\n");
      abort ();
    }
	
  for (j = 0; j < COUNT; j++)
    {
      if (j & 1)
	mpz_rrandomb (r, rands, m->size * GMP_NUMB_BITS);
      else
	mpz_urandomb (r, rands, m->size * GMP_NUMB_BITS);

      mpz_limbs_copy (a, r, m->size);

      if (!ref_modinv (ref, a, m->m, m->size))
	{
	  if (verbose)
	    fprintf (stderr, "Test %u (bit size %u) not invertible mod %s.\n",
		     j, m->bit_size, name);
	  continue;
	}
      m->invert (m, ai, a, scratch);
      if (mpn_cmp (ref, ai, m->size))
	{
	  fprintf (stderr, "%s->invert failed (test %u, bit size %u):\n",
		   name, j, m->bit_size);
	  fprintf (stderr, "a = ");
	  mpz_out_str (stderr, 16, r);
	  fprintf (stderr, "\np = ");
	  mpn_out_str (stderr, 16, m->m, m->size);
	  fprintf (stderr, "\nt = ");
	  mpn_out_str (stderr, 16, ai, m->size);
	  fprintf (stderr, " (bad)\nr = ");
	  mpn_out_str (stderr, 16, ref, m->size);

	  abort ();
	}
	  
    }
  mpz_clear (r);
  free (a);
  free (ai);
  free (ref);
  free (scratch);
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
