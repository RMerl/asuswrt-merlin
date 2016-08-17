#include "testutils.h"

#include "rsa.h"
#include "knuth-lfib.h"

void
test_main(void)
{
  struct rsa_public_key pub;
  struct rsa_private_key key;
  struct knuth_lfib_ctx lfib;

  /* FIXME: How is this spelled? */
  const uint8_t *msg = "Squemish ossifrage";
  size_t msg_length;

  uint8_t *decrypted;
  size_t decrypted_length;
  uint8_t after;

  mpz_t gibberish;

  rsa_private_key_init(&key);
  rsa_public_key_init(&pub);
  mpz_init(gibberish);

  knuth_lfib_init(&lfib, 17);
  
  test_rsa_set_key_1(&pub, &key);
  msg_length = strlen(msg);

  if (verbose)
    fprintf(stderr, "msg: `%s', length = %d\n", msg, (int) msg_length);
  
  ASSERT(rsa_encrypt(&pub,
		     &lfib, (nettle_random_func *) knuth_lfib_random,
		     msg_length, msg,
		     gibberish));

  if (verbose)
    {
      fprintf(stderr, "encrypted: ");
      mpz_out_str(stderr, 10, gibberish);
    }
  
  decrypted = xalloc(msg_length + 1);

  knuth_lfib_random (&lfib, msg_length + 1, decrypted);
  after = decrypted[msg_length];
  
  decrypted_length = msg_length - 1;
  ASSERT(!rsa_decrypt(&key, &decrypted_length, decrypted, gibberish));

  decrypted_length = msg_length;
  ASSERT(rsa_decrypt(&key, &decrypted_length, decrypted, gibberish));
  ASSERT(decrypted_length == msg_length);
  ASSERT(MEMEQ(msg_length, msg, decrypted));
  ASSERT(decrypted[msg_length] == after);

  knuth_lfib_random (&lfib, msg_length + 1, decrypted);
  after = decrypted[msg_length];

  decrypted_length = key.size;
  ASSERT(rsa_decrypt(&key, &decrypted_length, decrypted, gibberish));
  ASSERT(decrypted_length == msg_length);
  ASSERT(MEMEQ(msg_length, msg, decrypted));
  ASSERT(decrypted[msg_length] == after);
  
  knuth_lfib_random (&lfib, msg_length + 1, decrypted);
  after = decrypted[msg_length];

  decrypted_length = msg_length;
  ASSERT(rsa_decrypt_tr(&pub, &key,
			&lfib, (nettle_random_func *) knuth_lfib_random,
			&decrypted_length, decrypted, gibberish));
  ASSERT(decrypted_length == msg_length);
  ASSERT(MEMEQ(msg_length, msg, decrypted));
  ASSERT(decrypted[msg_length] == after);

  /* Test invalid key. */
  mpz_add_ui (key.q, key.q, 2);
  decrypted_length = key.size;
  ASSERT(!rsa_decrypt_tr(&pub, &key,
			 &lfib, (nettle_random_func *) knuth_lfib_random,
			 &decrypted_length, decrypted, gibberish));

  rsa_private_key_clear(&key);
  rsa_public_key_clear(&pub);
  mpz_clear(gibberish);
  free(decrypted);
}
  
