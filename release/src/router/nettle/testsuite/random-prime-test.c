#include "testutils.h"

#include "knuth-lfib.h"

void
test_main(void)
{
  struct knuth_lfib_ctx lfib;
  mpz_t p;
  unsigned bits;

  knuth_lfib_init(&lfib, 17);

  mpz_init(p);
  for (bits = 6; bits < 1000; bits = bits + 1 + bits/20)
    {
      if (verbose)
	fprintf(stderr, "bits = %d\n", bits);
      
      nettle_random_prime(p, bits, 0,
			  &lfib, (nettle_random_func *) knuth_lfib_random,
			  NULL, NULL);
      ASSERT (mpz_sizeinbase (p, 2) == bits);
      ASSERT (mpz_probab_prime_p(p, 25));
    }

  mpz_clear(p);
}
