#include "testutils.h"
#include "nettle-internal.h"
#include "des.h"

static void
test_des(const struct tstring *key, int expected_parity,
	 const struct tstring *cleartext,
	 const struct tstring *ciphertext)
{
  struct des_ctx ctx;
  uint8_t *data;
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (key->length == DES_KEY_SIZE);
  
  data = xalloc(length);

  ASSERT (des_check_parity(8, key->data) == expected_parity);

  ASSERT (des_set_key(&ctx, key->data));

  des_encrypt(&ctx, length, data, cleartext->data);

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

  des_decrypt(&ctx, length, data, data);

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

static void
test_weak(const struct tstring *key)
{
  struct des_ctx ctx;

  ASSERT (key->length == DES_KEY_SIZE);
  ASSERT (des_set_key(&ctx, key->data) == 0);
}

void
test_main(void)
{
  /* From Applied Cryptography */
  test_des(SHEX("01234567 89ABCDEF"), 1,
	   SHEX("01234567 89ABCDE7"),
	   SHEX("C9574425 6A5ED31D"));

  test_des(SHEX("01 01 01 01 01 01 01 80"), 1,
	   SHEX("00 00 00 00 00 00 00 00"),
	   SHEX("9C C6 2D F4 3B 6E ED 74"));

  test_des(SHEX("80 01 01 01 01 01 01 01"), 1,
	   SHEX("00 00 00 00 00 00 00 40"),
	   SHEX("A3 80 E0 2A 6B E5 46 96"));

  test_des(SHEX("08 19 2A 3B 4C 5D 6E 7F"), 1,
	   SHEX("00 00 00 00 00 00 00 00"),
	   SHEX("25 DD AC 3E 96 17 64 67"));

  test_des(SHEX("01 23 45 67 89 AB CD EF"), 1,
	   SDATA("Now is t"),
	   SHEX("3F A4 0E 8A 98 4D 48 15"));

  /* Same key, but with one bad parity bit, */
  test_des(SHEX("01 23 45 66 89 AB CD EF"), 0,
	   SDATA("Now is t"),
	   SHEX("3F A4 0E 8A 98 4D 48 15"));

  /* Parity check */
  {
    const struct tstring *s = SHEX("01 01 01 01 01 01 01 00");
    ASSERT (des_check_parity(s->length, s->data) == 0);
  }

  /* The four weak keys */
  test_weak(SHEX("01 01 01 01 01 01 01 01"));  
  test_weak(SHEX("FE FE FE FE FE FE FE FE"));
  test_weak(SHEX("1F 1F 1F 1F 0E 0E 0E 0E"));
  test_weak(SHEX("E0 E0 E0 E0 F1 F1 F1 F1"));

  /* Same weak key, but different parity. */
  test_weak(SHEX("E0 E0 E0 E0 F0 F1 F1 F1"));

  /* The six pairs of semiweak keys */
  test_weak(SHEX("01 FE 01 FE 01 FE 01 FE"));
  test_weak(SHEX("FE 01 FE 01 FE 01 FE 01"));

  test_weak(SHEX("1F E0 1F E0 0E F1 0E F1"));
  test_weak(SHEX("E0 1F E0 1F F1 0E F1 0E"));

  test_weak(SHEX("01 E0 01 E0 01 F1 01 F1"));
  test_weak(SHEX("E0 01 E0 01 F1 01 F1 01"));

  test_weak(SHEX("1F FE 1F FE 0E FE 0E FE"));
  test_weak(SHEX("FE 1F FE 1F FE 0E FE 0E"));

  test_weak(SHEX("01 1F 01 1F 01 0E 01 0E"));
  test_weak(SHEX("1F 01 1F 01 0E 01 0E 01"));

  test_weak(SHEX("E0 FE E0 FE F1 FE F1 FE"));
  test_weak(SHEX("FE E0 FE E0 FE F1 FE F1"));
}
