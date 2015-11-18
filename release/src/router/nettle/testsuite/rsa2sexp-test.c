#include "testutils.h"

#include "buffer.h"

void
test_main(void)
{
  struct rsa_public_key pub;
  struct rsa_private_key priv;

  struct nettle_buffer buffer;
  
  rsa_public_key_init(&pub);
  rsa_private_key_init(&priv);

  mpz_set_str(pub.n,
	      "085c3408989acae4faec3cbbad91c90d34c1d259cd74121a"
	      "36f38b0b51424a9b2be514a04377113a6cdafe79dd7d5f2e"
	      "cc8b5e9661189b86a7b22239907c25", 16);
  mpz_set_str(pub.e, "36ad4b1d", 16);

  ASSERT(rsa_public_key_prepare(&pub));
  
  mpz_set_str(priv.d,
	      "06ee6d4ff3c239e408150daf8117abfa36a40ad4455d9059"
	      "a86d52f33a2de07418a0a699594588c64810248c9412d554"
	      "f74af947c73c32007e87c92f0937ed", 16);

  mpz_set_str(priv.p,
	      "03259879b24315e9cf14254824c7935d807cdb6990f414a0"
	      "f65e6065130a611f", 16);

  mpz_set_str(priv.q,
	      "02a81ba73bad45fc73b36deffce52d1b73e0747f4d8a8264"
	      "8cecd310448ea63b", 16);

  mpz_set_str(priv.a,
	      "026cbdad5dd0046e093f060ecd5b4ac918e098b0278bb752"
	      "b7cadd6a8944f0b9", 16);

  mpz_set_str(priv.b,
	      "0148751e622d6d58e3bb094afd6edacf7370351d068e2ce9"
	      "f565c5528c4a7473", 16);

  mpz_set_str(priv.c,
	      "f8a458ea73a018dc6fa56863e3bc6de405f364f77dee6f09"
	      "62679ea1a8282e", 16);

  ASSERT(rsa_private_key_prepare(&priv));
  
  nettle_buffer_init(&buffer);
  ASSERT(rsa_keypair_to_sexp(&buffer, "rsa", &pub, &priv));

  if (verbose)
    {
      printf("private:");
      print_hex(buffer.size, buffer.contents);  
    }
  
  ASSERT(MEMEQ(buffer.size, buffer.contents,
	       H("2831313a707269766174652d6b657928"
		 "333a72736128313a6e36333a085c3408"
		 "989acae4faec3cbbad91c90d34c1d259"
		 "cd74121a36f38b0b51424a9b2be514a0"
		 "4377113a6cdafe79dd7d5f2ecc8b5e96"
		 "61189b86a7b22239907c252928313a65"
		 "343a36ad4b1d2928313a6436333a06ee"
		 "6d4ff3c239e408150daf8117abfa36a4"
		 "0ad4455d9059a86d52f33a2de07418a0"
		 "a699594588c64810248c9412d554f74a"
		 "f947c73c32007e87c92f0937ed292831"
		 "3a7033323a03259879b24315e9cf1425"
		 "4824c7935d807cdb6990f414a0f65e60"
		 "65130a611f2928313a7133323a02a81b"
		 "a73bad45fc73b36deffce52d1b73e074"
		 "7f4d8a82648cecd310448ea63b292831"
		 "3a6133323a026cbdad5dd0046e093f06"
		 "0ecd5b4ac918e098b0278bb752b7cadd"
		 "6a8944f0b92928313a6233323a014875"
		 "1e622d6d58e3bb094afd6edacf737035"
		 "1d068e2ce9f565c5528c4a7473292831"
		 "3a6333323a00f8a458ea73a018dc6fa5"
		 "6863e3bc6de405f364f77dee6f096267"
		 "9ea1a8282e292929")));

  nettle_buffer_clear(&buffer);
  ASSERT(rsa_keypair_to_sexp(&buffer, NULL, &pub, NULL));

  if (verbose)
    {
      printf("public:");
      print_hex(buffer.size, buffer.contents);  
    }
  ASSERT(MEMEQ(buffer.size, buffer.contents,
	       H("2831303a7075626c69632d6b65792839"
		 "3a7273612d706b63733128313a6e3633"
		 "3a085c3408989acae4faec3cbbad91c9"
		 "0d34c1d259cd74121a36f38b0b51424a"
		 "9b2be514a04377113a6cdafe79dd7d5f"
		 "2ecc8b5e9661189b86a7b22239907c25"
		 "2928313a65343a36ad4b1d292929")));

  nettle_buffer_clear(&buffer);
  rsa_public_key_clear(&pub);
  rsa_private_key_clear(&priv);
}
