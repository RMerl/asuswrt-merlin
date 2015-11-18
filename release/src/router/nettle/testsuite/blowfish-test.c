#include "testutils.h"
#include "nettle-internal.h"
#include "blowfish.h"

static void
test_blowfish(const struct tstring *key,
	      const struct tstring *cleartext,
	      const struct tstring *ciphertext)
{
  struct blowfish_ctx ctx;
  uint8_t *data = xalloc(cleartext->length);
  size_t length;
  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  blowfish_set_key(&ctx, key->length, key->data);
  blowfish_encrypt(&ctx, length, data, cleartext->data);

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
  blowfish_set_key(&ctx, key->length, key->data);
  blowfish_decrypt(&ctx, length, data, data);

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
  /* 208 bit key. Test from GNUPG. */
  test_blowfish(SDATA("abcdefghijklmnopqrstuvwxyz"),
		SDATA("BLOWFISH"),
		SHEX("32 4E D0 FE F4 13 A2 03"));
}
