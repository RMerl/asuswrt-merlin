/* For FILE, used by gmp.h */
#include <cstdio>

#include "testutils.h"
#include "md5.h"

/* Test C++ linkage */
void
test_main(void)
{
  struct md5_ctx md5;
  uint8_t digest[MD5_DIGEST_SIZE];

  md5_init (&md5);
  md5_update (&md5, 14, reinterpret_cast<const uint8_t *> ("message digest"));
  md5_digest (&md5, MD5_DIGEST_SIZE, digest);

  ASSERT (MEMEQ (MD5_DIGEST_SIZE, digest,
		 H("F96B697D7CB7938D 525A2F31AAF161D0")));

#if WITH_PUBLIC_KEY

  struct rsa_public_key pub;
  struct rsa_private_key key;

  mpz_t expected;
  
  mpz_init(expected);
  
  rsa_private_key_init(&key);
  rsa_public_key_init(&pub);

  mpz_set_str(pub.n,
	      "69abd505285af665" "36ddc7c8f027e6f0" "ed435d6748b16088"
	      "4fd60842b3a8d7fb" "bd8a3c98f0cc50ae" "4f6a9f7dd73122cc"
	      "ec8afa3f77134406" "f53721973115fc2d" "8cfbba23b145f28d"
	      "84f81d3b6ae8ce1e" "2850580c026e809b" "cfbb52566ea3a3b3"
	      "df7edf52971872a7" "e35c1451b8636d22" "279a8fb299368238"
	      "e545fbb4cf", 16);
  mpz_set_str(pub.e, "0db2ad57", 16);

  ASSERT (rsa_public_key_prepare(&pub));

  mpz_set_str(key.p,
	      "0a66399919be4b4d" "e5a78c5ea5c85bf9" "aba8c013cb4a8732"
	      "14557a12bd67711e" "bb4073fd39ad9a86" "f4e80253ad809e5b"
	      "f2fad3bc37f6f013" "273c9552c9f489", 16);

  mpz_set_str(key.q,
	      "0a294f069f118625" "f5eae2538db9338c" "776a298eae953329"
	      "9fd1eed4eba04e82" "b2593bc98ba8db27" "de034da7daaea795"
	      "2d55b07b5f9a5875" "d1ca5f6dcab897", 16);
  
  mpz_set_str(key.a,
	      "011b6c48eb592eee" "e85d1bb35cfb6e07" "344ea0b5e5f03a28"
	      "5b405396cbc78c5c" "868e961db160ba8d" "4b984250930cf79a"
	      "1bf8a9f28963de53" "128aa7d690eb87", 16);
  
  mpz_set_str(key.b,
	      "0409ecf3d2557c88" "214f1af5e1f17853" "d8b2d63782fa5628"
	      "60cf579b0833b7ff" "5c0529f2a97c6452" "2fa1a8878a9635ab"
	      "ce56debf431bdec2" "70b308fa5bf387", 16);
  
  mpz_set_str(key.c,
	      "04e103ee925cb5e6" "6653949fa5e1a462" "c9e65e1adcd60058"
	      "e2df9607cee95fa8" "daec7a389a7d9afc" "8dd21fef9d83805a"
	      "40d46f49676a2f6b" "2926f70c572c00", 16);

  ASSERT (rsa_private_key_prepare(&key));

  ASSERT (pub.size == key.size);
  
  mpz_set_str(expected,
	      "53bf517009fa956e" "3daa6adc95e8663d" "3759002f488bbbad"
	      "e49f62792d85dbcc" "293f68e2b68ef89a" "c5bd42d98f845325"
	      "3e6c1b76fc337db5" "e0053f255c55faf3" "eb6cc568ad7f5013"
	      "5b269a64acb9eaa7" "b7f09d9bd90310e6" "4c58f6dbe673ada2"
	      "67c97a9d99e19f9d" "87960d9ce3f0d5ce" "84f401fe7e10fa24"
	      "28b9bffcf9", 16);

  mpz_t signature;
  mpz_init(signature);

  /* Create signature */
  md5_update (&md5, 39, reinterpret_cast<const uint8_t *>
	      ("The magic words are squeamish ossifrage"));
  ASSERT (rsa_md5_sign (&key, &md5, signature));

  /* Verify it */
  md5_update (&md5, 39, reinterpret_cast<const uint8_t *>
	      ("The magic words are squeamish ossifrage"));
  ASSERT (rsa_md5_verify (&pub, &md5, signature));

  /* Try bad data */
  md5_update (&md5, 39, reinterpret_cast<const uint8_t *>
	      ("The magik words are squeamish ossifrage"));
  ASSERT (!rsa_md5_verify (&pub, &md5, signature));
  
#endif /* WITH_PUBLIC_KEY */
}
