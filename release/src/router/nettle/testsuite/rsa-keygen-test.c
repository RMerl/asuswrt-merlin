#include "testutils.h"

#include "knuth-lfib.h"

static void
progress(void *ctx UNUSED, int c)
{
  fputc(c, stderr);
}

void
test_main(void)
{
  struct rsa_public_key pub;
  struct rsa_private_key key;
  
  struct knuth_lfib_ctx lfib;

  mpz_t expected;
  
  mpz_init(expected);
  
  rsa_private_key_init(&key);
  rsa_public_key_init(&pub);

  /* Generate a 1024 bit key with random e */
  knuth_lfib_init(&lfib, 13);

  ASSERT (rsa_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       1024, 50));

  test_rsa_key(&pub, &key);

  mpz_set_str(expected,
	      "31ee088024b66f28" "e182fa07e60f7864" "636eea21cf74c804"
	      "02a9c29ebd00933d" "8fae3ddf029d18e0" "1b5498c70a4b68fd"
	      "d3135748424e8caf" "43ee86068f250c92" "8da001d09f68c433"
	      "96f2c2a42eaed7e5" "8934a052faa38b2c" "f2ac04cc91bd0d15"
	      "4f60b16dc9045b08" "2ea1372717fd7f9c" "1e9cc383b7d5b909"
	      "72e3126df060ef79" , 16);

  test_rsa_md5(&pub, &key, expected);

  /* Generate a 2000 bit key with fixed e */
  knuth_lfib_init(&lfib, 17);

  mpz_set_ui(pub.e, 17);
  ASSERT (rsa_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       2000, 0));

  test_rsa_key(&pub, &key);

  mpz_set_str(expected,
	      "892ef7cda3b0b501" "85de20b93340316e" "b35ac9c193f1f5a3"
	      "9e6c1c327b9c36b8" "d4f1d41653b48fbd" "0c49e48bbdc46ced"
	      "13b3f6426e8a1780" "55b9077ba59ce748" "a325563c3b4bdf78"
	      "acdcdd556f5de3cf" "70257c3b334ba360" "5625ebda869c8058"
	      "b95b40c1e75eb91b" "776e83c0224757c6" "b61266cb1739df1a"
	      "c7fcc09194c575b5" "af4f3eb8e3aa3900" "22b72fb6e950c355"
	      "88743bab32c3a214" "ea5865e2f5c41d67" "12e745496890fc9d"
	      "7944a759f39d7b57" "e365d8d3f6ac2dd4" "052b6a2e58a6af82"
	      "b0d67e7fe09045d9" "bc965e260cf3c9a9" "3bfaa09bdd076dc2"
	      "c0ab48ce5b67105c" "cad90dcfc11cd713" "e64478d2d7ea42dd"
	      "fd040793c487588d" "6218" , 16);

  test_rsa_sha1(&pub, &key, expected);
  
  rsa_private_key_clear(&key);
  rsa_public_key_clear(&pub);
  mpz_clear(expected);
}
