#include "testutils.h"
#include "hmac.h"
#include "pbkdf2.h"

/* NOTE: The salt argument is expected to expand to length, data */
#define PBKDF2_TEST(ctx, update, digest, size, c, salt, expect)	\
  do {									\
    dk[expect->length] = 17;						\
    PBKDF2 (ctx, update, digest, size, c, salt, expect->length, dk); \
    ASSERT(MEMEQ (expect->length, dk, expect->data));			\
    ASSERT(dk[expect->length] == 17);					\
  } while (0)

#define PBKDF2_HMAC_TEST(f, key, c, salt, expect)			\
  do {									\
    dk[expect->length] = 17;						\
    f (key, c, salt, expect->length, dk);				\
    ASSERT(MEMEQ (expect->length, dk, expect->data));			\
    ASSERT(dk[expect->length] == 17);					\
  } while (0)

#define MAX_DKLEN SHA512_DIGEST_SIZE

void
test_main (void)
{
  uint8_t dk[MAX_DKLEN + 1];
  struct hmac_sha1_ctx sha1ctx;
  struct hmac_sha256_ctx sha256ctx;
  struct hmac_sha512_ctx sha512ctx;

  /* Test vectors for PBKDF2 from RFC 6070. */

  hmac_sha1_set_key (&sha1ctx, 8, "password");

  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       1, LDATA("salt"),
	       SHEX("0c60c80f961f0e71f3a9b524af6012062fe037a6"));

  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       2, LDATA("salt"),
	       SHEX("ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957"));

  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       4096, LDATA("salt"),
	       SHEX("4b007901b765489abead49d926f721d065a429c1"));

#if 0				/* too slow */
  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       16777216, LDATA("salt"),
	       SHEX("eefe3d61cd4da4e4e9945b3d6ba2158c2634e984"));
#endif

  hmac_sha1_set_key (&sha1ctx, 24, "passwordPASSWORDpassword");

  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       4096, LDATA("saltSALTsaltSALTsaltSALTsaltSALTsalt"),
	       SHEX("3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038"));

  hmac_sha1_set_key (&sha1ctx, 9, "pass\0word");

  PBKDF2_TEST (&sha1ctx, hmac_sha1_update, hmac_sha1_digest, SHA1_DIGEST_SIZE,
	       4096, LDATA("sa\0lt"),
	       SHEX("56fa6aa75548099dcc37d7f03425e0c3"));

  /* PBKDF2-HMAC-SHA-256 test vectors confirmed with another
     implementation.  */

  hmac_sha256_set_key (&sha256ctx, 6, "passwd");

  PBKDF2_TEST (&sha256ctx, hmac_sha256_update, hmac_sha256_digest,
	       SHA256_DIGEST_SIZE, 1, LDATA("salt"),
	       SHEX("55ac046e56e3089fec1691c22544b605"));

  hmac_sha256_set_key (&sha256ctx, 8, "Password");

  PBKDF2_TEST (&sha256ctx, hmac_sha256_update, hmac_sha256_digest,
	       SHA256_DIGEST_SIZE, 80000, LDATA("NaCl"),
	       SHEX("4ddcd8f60b98be21830cee5ef22701f9"));

  /* PBKDF2-HMAC-SHA-512 test vectors confirmed with another
     implementation (python-pbkdf2).

     >>> from pbkdf2 import PBKDF2
     >>> import hmac as HMAC
     >>> from hashlib import sha512 as SHA512
     >>> PBKDF2("password", "salt", 50, macmodule=HMAC, digestmodule=SHA512).read(64).encode('hex')
  */

  hmac_sha512_set_key (&sha512ctx, 8, "password");
  PBKDF2_TEST (&sha512ctx, hmac_sha512_update, hmac_sha512_digest,
	       SHA512_DIGEST_SIZE, 1, LDATA("NaCL"),
	       SHEX("73decfa58aa2e84f94771a75736bb88bd3c7b38270cfb50cb390ed78b305656af8148e52452b2216b2b8098b761fc6336060a09f76415e9f71ea47f9e9064306"));

  hmac_sha512_set_key (&sha512ctx, 9, "pass\0word");
  PBKDF2_TEST (&sha512ctx, hmac_sha512_update, hmac_sha512_digest,
	       SHA512_DIGEST_SIZE, 1, LDATA("sa\0lt"),
	       SHEX("71a0ec842abd5c678bcfd145f09d83522f93361560563c4d0d63b88329871090e76604a49af08fe7c9f57156c8790996b20f06bc535e5ab5440df7e878296fa7"));

  hmac_sha512_set_key (&sha512ctx, 24, "passwordPASSWORDpassword");
  PBKDF2_TEST (&sha512ctx, hmac_sha512_update, hmac_sha512_digest,
	       SHA512_DIGEST_SIZE, 50, LDATA("salt\0\0\0"),
	       SHEX("016871a4c4b75f96857fd2b9f8ca28023b30ee2a39f5adcac8c9375f9bda1ccd1b6f0b2fc3adda505412e79d890056c62e524c7d51154b1a8534575bd02dee39"));

  /* Test convenience functions. */

  PBKDF2_HMAC_TEST(pbkdf2_hmac_sha1, LDATA("password"), 1, LDATA("salt"),
		   SHEX("0c60c80f961f0e71f3a9b524af6012062fe037a6"));

  PBKDF2_HMAC_TEST(pbkdf2_hmac_sha256, LDATA("passwd"), 1, LDATA("salt"),
		   SHEX("55ac046e56e3089fec1691c22544b605"));

}
