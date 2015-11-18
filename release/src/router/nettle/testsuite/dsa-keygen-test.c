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
  struct dsa_public_key pub;
  struct dsa_private_key key;
  struct dsa_params *params;

  struct knuth_lfib_ctx lfib;
  
  dsa_private_key_init(&key);
  dsa_public_key_init(&pub);

  knuth_lfib_init(&lfib, 13);

  params = (struct dsa_params *) &pub;
  ASSERT (dsa_compat_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       1024, 160));

  test_dsa_key(params, pub.y, key.x, 160);
  test_dsa160(&pub, &key, NULL);

  ASSERT (dsa_compat_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       2048, 256));

  test_dsa_key(params, pub.y, key.x, 256);
  test_dsa256(&pub, &key, NULL);
  
  ASSERT (dsa_compat_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       2048, 224));

  test_dsa_key(params, pub.y, key.x, 224);
  test_dsa256(&pub, &key, NULL);

  /* Test with large q */
  if (!dsa_generate_params (params,
			    &lfib,
			    (nettle_random_func *) knuth_lfib_random,
			    NULL, verbose ? progress : NULL,
			    1024, 768))
    FAIL();

  dsa_generate_keypair (params, pub.y, key.x,
			&lfib,
			(nettle_random_func *) knuth_lfib_random);
  test_dsa_key(params, pub.y, key.x, 768);
  test_dsa256(&pub, &key, NULL);
  
  dsa_public_key_clear(&pub);
  dsa_private_key_clear(&key);
}
