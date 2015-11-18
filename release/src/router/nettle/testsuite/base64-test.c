#include "testutils.h"
#include "base64.h"
#include "knuth-lfib.h"

static void
test_fuzz_once(struct base64_encode_ctx *encode,
	       struct base64_decode_ctx *decode,
	       size_t size, const uint8_t *input)
{
  size_t base64_len = BASE64_ENCODE_RAW_LENGTH (size);
  size_t out_len;
  uint8_t *base64 = xalloc (base64_len + 2);
  uint8_t *decoded = xalloc (size + 2);

  *base64++ = 0x12;
  base64[base64_len] = 0x34;

  *decoded++ = 0x56;
  decoded[size] = 0x78;

  out_len = base64_encode_update(encode, base64, size, input);
  ASSERT (out_len <= base64_len);
  out_len += base64_encode_final(encode, base64 + out_len);
  ASSERT (out_len == base64_len);
  ASSERT (base64[-1] == 0x12);
  ASSERT (base64[base64_len] == 0x34);

  ASSERT(base64_decode_update(decode, &out_len, decoded,
			      base64_len, base64));
  ASSERT(base64_decode_final(decode));
  ASSERT (out_len == size);
  ASSERT (decoded[-1] == 0x56);
  ASSERT (decoded[size] == 0x78);
  
  ASSERT(MEMEQ(size, input, decoded));
  free (base64 - 1);
  free (decoded - 1);
}

static void
test_fuzz(void)
{
  /* Fuzz a round-trip through both encoder and decoder */
  struct base64_encode_ctx encode;
  struct base64_decode_ctx decode;
  unsigned i;
  size_t length;
  uint8_t input[1024];

  struct knuth_lfib_ctx rand_ctx;
  knuth_lfib_init(&rand_ctx, 39854);

  for (i = 0; i < 10000; i++)
    {
      length = i % sizeof(input);
      /* length could be 0, which is fine we need to test that case too */
      knuth_lfib_random(&rand_ctx, length, input);

      base64_encode_init(&encode);
      base64_decode_init(&decode);
      test_fuzz_once(&encode, &decode, length, input);

      base64url_encode_init(&encode);
      base64url_decode_init(&decode);
      test_fuzz_once(&encode, &decode, length, input);
    }
}

void
test_main(void)
{
  ASSERT(BASE64_ENCODE_LENGTH(0) == 0);   /* At most   4 bits */
  ASSERT(BASE64_ENCODE_LENGTH(1) == 2);   /* At most  12 bits */
  ASSERT(BASE64_ENCODE_LENGTH(2) == 3);   /* At most  20 bits */
  ASSERT(BASE64_ENCODE_LENGTH(3) == 4);   /* At most  28 bits */
  ASSERT(BASE64_ENCODE_LENGTH(4) == 6);   /* At most  36 bits */
  ASSERT(BASE64_ENCODE_LENGTH(5) == 7);   /* At most  44 bits */
  ASSERT(BASE64_ENCODE_LENGTH(12) == 16); /* At most 100 bits */
  ASSERT(BASE64_ENCODE_LENGTH(13) == 18); /* At most 108 bits */

  ASSERT(BASE64_DECODE_LENGTH(0) == 0); /* At most  6 bits */
  ASSERT(BASE64_DECODE_LENGTH(1) == 1); /* At most 12 bits */
  ASSERT(BASE64_DECODE_LENGTH(2) == 2); /* At most 18 bits */
  ASSERT(BASE64_DECODE_LENGTH(3) == 3); /* At most 24 bits */
  ASSERT(BASE64_DECODE_LENGTH(4) == 3); /* At most 30 bits */
  
  test_armor(&nettle_base64, 0, "", "");
  test_armor(&nettle_base64, 1, "H", "SA==");
  test_armor(&nettle_base64, 2, "He", "SGU=");
  test_armor(&nettle_base64, 3, "Hel", "SGVs");
  test_armor(&nettle_base64, 4, "Hell", "SGVsbA==");
  test_armor(&nettle_base64, 5, "Hello", "SGVsbG8=");
  test_armor(&nettle_base64, 6, "Hello", "SGVsbG8A");
  test_armor(&nettle_base64, 9, "Hello?>>>", "SGVsbG8/Pj4+");
  test_armor(&nettle_base64, 4, "\xff\xff\xff\xff", "/////w==");

  test_armor(&nettle_base64url, 0, "", "");
  test_armor(&nettle_base64url, 1, "H", "SA==");
  test_armor(&nettle_base64url, 2, "He", "SGU=");
  test_armor(&nettle_base64url, 3, "Hel", "SGVs");
  test_armor(&nettle_base64url, 4, "Hell", "SGVsbA==");
  test_armor(&nettle_base64url, 5, "Hello", "SGVsbG8=");
  test_armor(&nettle_base64url, 6, "Hello", "SGVsbG8A");
  test_armor(&nettle_base64url, 9, "Hello?>>>", "SGVsbG8_Pj4-");
  test_armor(&nettle_base64url, 4, "\xff\xff\xff\xff", "_____w==");

  {
    /* Test overlapping areas */
    uint8_t buffer[] = "Helloxxxx";
    struct base64_decode_ctx ctx;
    size_t dst_length;
    
    ASSERT(BASE64_ENCODE_RAW_LENGTH(5) == 8);
    base64_encode_raw(buffer, 5, buffer);
    ASSERT(MEMEQ(9, buffer, "SGVsbG8=x"));

    base64_decode_init(&ctx);
    dst_length = 0; /* Output parameter only. */
    ASSERT(base64_decode_update(&ctx, &dst_length, buffer, 8, buffer));
    ASSERT(dst_length == 5);
    
    ASSERT(MEMEQ(9, buffer, "HelloG8=x"));
  }
  test_fuzz ();
}
