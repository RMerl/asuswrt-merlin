#include "testutils.h"
#include "knuth-lfib.h"

/* Check if y^2 = x^3 - 3x + b */
static int
ecc_valid_p (struct ecc_point *pub)
{
  mpz_t t, x, y;
  mpz_t lhs, rhs;
  int res;
  mp_size_t size;

  size = pub->ecc->p.size;

  /* First check range */
  if (mpn_cmp (pub->p, pub->ecc->p.m, size) >= 0
      || mpn_cmp (pub->p + size, pub->ecc->p.m, size) >= 0)
    return 0;

  mpz_init (lhs);
  mpz_init (rhs);

  mpz_roinit_n (x, pub->p, size);
  mpz_roinit_n (y, pub->p + size, size);

  mpz_mul (lhs, y, y);
  
  if (pub->ecc->p.bit_size == 255)
    {
      /* Check that
	 121666 (1 + x^2 - y^2) = 121665 x^2 y^2 */
      mpz_t x2;
      mpz_init (x2);
      mpz_mul (x2, x, x); /* x^2 */
      mpz_mul (rhs, x2, lhs); /* x^2 y^2 */
      mpz_sub (lhs, x2, lhs); /* x^2 - y^2 */
      mpz_add_ui (lhs, lhs, 1); /* 1 + x^2 - y^2 */
      mpz_mul_ui (lhs, lhs, 121666);
      mpz_mul_ui (rhs, rhs, 121665);

      mpz_clear (x2);
    }
  else
    {
      /* Check y^2 = x^3 - 3 x + b */
      mpz_mul (rhs, x, x);
      mpz_sub_ui (rhs, rhs, 3);
      mpz_mul (rhs, rhs, x);
      mpz_add (rhs, rhs, mpz_roinit_n (t, pub->ecc->b, size));
    }
  res = mpz_congruent_p (lhs, rhs, mpz_roinit_n (t, pub->ecc->p.m, size));
  
  mpz_clear (lhs);
  mpz_clear (rhs);

  return res;
}

void
test_main (void)
{
  unsigned i;
  struct knuth_lfib_ctx rctx;
  struct dsa_signature signature;

  struct tstring *digest;

  knuth_lfib_init (&rctx, 4711);
  dsa_signature_init (&signature);

  digest = SHEX (/* sha256("abc") */
		 "BA7816BF 8F01CFEA 414140DE 5DAE2223"
		 "B00361A3 96177A9C B410FF61 F20015AD");

  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      struct ecc_point pub;
      struct ecc_scalar key;

      if (verbose)
	fprintf (stderr, "Curve %d\n", ecc->p.bit_size);

      ecc_point_init (&pub, ecc);
      ecc_scalar_init (&key, ecc);

      ecdsa_generate_keypair (&pub, &key,
			      &rctx,
			      (nettle_random_func *) knuth_lfib_random);

      if (verbose)
	{
	  fprintf (stderr, "Public key:\nx = ");
	  write_mpn (stderr, 16, pub.p, ecc->p.size);
	  fprintf (stderr, "\ny = ");
	  write_mpn (stderr, 16, pub.p + ecc->p.size, ecc->p.size);
	  fprintf (stderr, "\nPrivate key: ");
	  write_mpn (stderr, 16, key.p, ecc->p.size);
	  fprintf (stderr, "\n");
	}
      if (!ecc_valid_p (&pub))
	die ("ecdsa_generate_keypair produced an invalid point.\n");

      ecdsa_sign (&key,
		  &rctx, (nettle_random_func *) knuth_lfib_random,
		  digest->length, digest->data,
		  &signature);

      if (!ecdsa_verify (&pub, digest->length, digest->data,
			  &signature))
	die ("ecdsa_verify failed.\n");

      digest->data[3] ^= 17;
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid digest.\n");
      digest->data[3] ^= 17;

      mpz_combit (signature.r, 117);
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid signature.r.\n");

      mpz_combit (signature.r, 117);
      mpz_combit (signature.s, 93);
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid signature.s.\n");

      ecc_point_clear (&pub);
      ecc_scalar_clear (&key);
    }
  dsa_signature_clear (&signature);
}
