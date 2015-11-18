#include "testutils.h"
#include "md5-compat.h"

void
test_main(void)
{
  MD5_CTX ctx;
  unsigned char digest[MD5_DIGEST_SIZE];

  MD5Init(&ctx);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("D41D8CD98F00B204 E9800998ECF8427E")));

  MD5Init(&ctx);
  MD5Update(&ctx, "a", 1);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("0CC175B9C0F1B6A8 31C399E269772661")));

  MD5Init(&ctx);
  MD5Update(&ctx, "abc", 3);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("900150983cd24fb0 D6963F7D28E17F72")));

  MD5Init(&ctx);
  MD5Update(&ctx, "message digest", 14);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("F96B697D7CB7938D 525A2F31AAF161D0")));

  MD5Init(&ctx);
  MD5Update(&ctx, "abcdefghijklmnopqrstuvwxyz", 26);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("C3FCD3D76192E400 7DFB496CCA67E13B")));

  MD5Init(&ctx);
  MD5Update(&ctx, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("D174AB98D277D9F5 A5611C2C9F419D9F")));

  MD5Init(&ctx);
  MD5Update(&ctx, "1234567890123456789012345678901234567890"
	    "1234567890123456789012345678901234567890",
	    80);
  MD5Final(digest, &ctx);
  ASSERT(MEMEQ(MD5_DIGEST_SIZE, digest,
	       H("57EDF4A22BE3C955 AC49DA2E2107B67A")));
}
