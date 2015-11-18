#include "testutils.h"
#include "cast128.h"

static void
test_cast5(const struct tstring *key,
	      const struct tstring *cleartext,
	      const struct tstring *ciphertext)
{
  struct cast128_ctx ctx;
  uint8_t *data = xalloc(cleartext->length);
  size_t length;
  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  cast5_set_key(&ctx, key->length, key->data);
  cast128_encrypt(&ctx, length, data, cleartext->data);

  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "Encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }
  cast5_set_key(&ctx, key->length, key->data);
  cast128_decrypt(&ctx, length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "Decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }

  free(data);
}

void
test_main(void)
{
  /* Test vectors from B.1. Single Plaintext-Key-Ciphertext Sets, RFC
   * 2144 */

  /* 128 bit key */
  test_cipher(&nettle_cast128,
	      SHEX("01 23 45 67 12 34 56 78"
		   "23 45 67 89 34 56 78 9A"),
	      SHEX("01 23 45 67 89 AB CD EF"),
	      SHEX("23 8B 4F E5 84 7E 44 B2"));
  
  /* 80 bit key */
  test_cast5(SHEX("01 23 45 67 12 34 56 78 23 45"),
	     SHEX("01 23 45 67 89 AB CD EF"),
	     SHEX("EB 6A 71 1A 2C 02 27 1B"));

  /* 40 bit key */
  test_cast5(SHEX("01 23 45 67 12"),
	     SHEX("01 23 45 67 89 AB CD EF"),
	     SHEX("7A C8 16 D1 6E 9B 30 2E"));
}
