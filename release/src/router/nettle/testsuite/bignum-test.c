#include "testutils.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if WITH_HOGWEED
#include "bignum.h"

static void
test_bignum(const char *hex, const struct tstring *base256)
{
  mpz_t a;
  mpz_t b;
  uint8_t *buf;
  
  mpz_init_set_str(a, hex, 16);
  nettle_mpz_init_set_str_256_s(b, base256->length, base256->data);

  ASSERT(mpz_cmp(a, b) == 0);

  buf = xalloc(base256->length + 1);
  memset(buf, 17, base256->length + 1);

  nettle_mpz_get_str_256(base256->length, buf, a);
  ASSERT(MEMEQ(base256->length, buf, base256->data));

  ASSERT(buf[base256->length] == 17);

  mpz_clear(a); mpz_clear(b);
  free(buf);
}

static void
test_size(long x, unsigned size)
{
  mpz_t t;

  mpz_init_set_si(t, x);
  ASSERT(nettle_mpz_sizeinbase_256_s(t) == size);
  mpz_clear(t);
}
#endif /* WITH_HOGWEED */


void
test_main(void)
{
#if WITH_HOGWEED
  test_size(0, 1);
  test_size(1, 1);
  test_size(0x7f, 1);
  test_size(0x80, 2);
  test_size(0x81, 2);
  test_size(0xff, 2);
  test_size(0x100, 2);
  test_size(0x101, 2);
  test_size(0x1111, 2);
  test_size(0x7fff, 2);
  test_size(0x8000, 3);
  test_size(0x8001, 3);

  test_size(-      1, 1); /*     ff */
  test_size(-   0x7f, 1); /*     81 */
  test_size(-   0x80, 1); /*     80 */
  test_size(-   0x81, 2); /*   ff7f */
  test_size(-   0xff, 2); /*   ff01 */
  test_size(-  0x100, 2); /*   ff00 */
  test_size(-  0x101, 2); /*   feff */
  test_size(- 0x1111, 2); /*   eeef */
  test_size(- 0x7fff, 2); /*   8001 */
  test_size(- 0x8000, 2); /*   8000 */
  test_size(- 0x8001, 3); /* ff7fff */

  test_bignum("0", SHEX("00"));
  test_bignum("010203040506", SHEX("010203040506"));
  test_bignum("80010203040506", SHEX("0080010203040506"));

  test_bignum(   "-1", SHEX(    "ff"));
  test_bignum(  "-7f", SHEX(    "81"));
  test_bignum(  "-80", SHEX(    "80"));
  test_bignum(  "-81", SHEX(  "ff7f"));
  test_bignum("-7fff", SHEX(  "8001"));
  test_bignum("-8000", SHEX(  "8000"));
  test_bignum("-8001", SHEX("ff7fff"));
  
#else /* !WITH_HOGWEED */
  SKIP();
#endif /* !WITH_HOGWEED */
}
