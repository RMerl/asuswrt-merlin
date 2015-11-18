#include "testutils.h"
#include "aes.h"
#include "nettle-internal.h"

static void
test_invert(const struct tstring *key,
	    const struct tstring *cleartext,
	    const struct tstring *ciphertext)
{
  struct aes_ctx encrypt;
  struct aes_ctx decrypt;
  uint8_t *data = xalloc(cleartext->length);
  size_t length;
  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  aes_set_encrypt_key (&encrypt, key->length, key->data);
  aes_encrypt (&encrypt, length, data, cleartext->data);
  
  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "test_invert: Encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }

  aes_invert_key (&decrypt, &encrypt);
  aes_decrypt (&decrypt, length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "test_invert: Decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }
  free (data);
}

/* Old, unified, interface */
static nettle_set_key_func unified_aes128_set_encrypt_key;
static nettle_set_key_func unified_aes128_set_encrypt_key;
static nettle_set_key_func unified_aes192_set_encrypt_key;
static nettle_set_key_func unified_aes192_set_encrypt_key;
static nettle_set_key_func unified_aes256_set_encrypt_key;
static nettle_set_key_func unified_aes256_set_encrypt_key;
static void
unified_aes128_set_encrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_encrypt_key (ctx, AES128_KEY_SIZE, key);
}
static void
unified_aes128_set_decrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_decrypt_key (ctx, AES128_KEY_SIZE, key);
}

static void
unified_aes192_set_encrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_encrypt_key (ctx, AES192_KEY_SIZE, key);
}
static void
unified_aes192_set_decrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_decrypt_key (ctx, AES192_KEY_SIZE, key);
}

static void
unified_aes256_set_encrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_encrypt_key (ctx, AES256_KEY_SIZE, key);
}
static void
unified_aes256_set_decrypt_key (void *ctx, const uint8_t *key)
{
  aes_set_decrypt_key (ctx, AES256_KEY_SIZE, key);
}

#define UNIFIED_AES(bits) {			\
  "unified-aes" #bits, sizeof(struct aes_ctx),	\
  AES_BLOCK_SIZE, AES ## bits ## _KEY_SIZE,	\
  unified_aes ## bits ##_set_encrypt_key,	\
  unified_aes ## bits ##_set_decrypt_key,	\
  (nettle_cipher_func *) aes_encrypt,		\
  (nettle_cipher_func *) aes_decrypt,		\
}
const struct nettle_cipher nettle_unified_aes128
= UNIFIED_AES(128);
const struct nettle_cipher nettle_unified_aes192
= UNIFIED_AES(192);
const struct nettle_cipher nettle_unified_aes256
= UNIFIED_AES(256);

static void
test_cipher2(const struct nettle_cipher *c1,
	     const struct nettle_cipher *c2,	     
	     const struct tstring *key,
	     const struct tstring *cleartext,
	     const struct tstring *ciphertext)
{
  test_cipher (c1, key, cleartext, ciphertext);
  test_cipher (c2, key, cleartext, ciphertext);
}

void
test_main(void)
{
  /* Test both the new interface and the older unified interface. */

  /* 128 bit keys */
  test_cipher2(&nettle_aes128, &nettle_unified_aes128,
	       SHEX("0001020305060708 0A0B0C0D0F101112"),
	       SHEX("506812A45F08C889 B97F5980038B8359"),
	       SHEX("D8F532538289EF7D 06B506A4FD5BE9C9"));
  
  test_cipher2(&nettle_aes128, &nettle_unified_aes128,
	       SHEX("14151617191A1B1C 1E1F202123242526"),
	       SHEX("5C6D71CA30DE8B8B 00549984D2EC7D4B"),
	       SHEX("59AB30F4D4EE6E4F F9907EF65B1FB68C"));

  test_cipher2(&nettle_aes128, &nettle_unified_aes128,
	       SHEX("28292A2B2D2E2F30 323334353738393A"),
	       SHEX("53F3F4C64F8616E4 E7C56199F48F21F6"),
	       SHEX("BF1ED2FCB2AF3FD4 1443B56D85025CB1"));
  
  test_cipher2(&nettle_aes128, &nettle_unified_aes128,
	       SHEX("A0A1A2A3A5A6A7A8 AAABACADAFB0B1B2"),
	       SHEX("F5F4F7F684878689 A6A7A0A1D2CDCCCF"),
	       SHEX("CE52AF650D088CA5 59425223F4D32694"));

  /* 192 bit keys */
  test_cipher2(&nettle_aes192, &nettle_unified_aes192, 
	       SHEX("0001020305060708 0A0B0C0D0F101112"
		    "14151617191A1B1C"),
	       SHEX("2D33EEF2C0430A8A 9EBF45E809C40BB6"),
	       SHEX("DFF4945E0336DF4C 1C56BC700EFF837F"));

  /* 256 bit keys */
  test_cipher2(&nettle_aes256, &nettle_unified_aes256,
	       SHEX("0001020305060708 0A0B0C0D0F101112"
		    "14151617191A1B1C 1E1F202123242526"),
	       SHEX("834EADFCCAC7E1B30664B1ABA44815AB"),
	       SHEX("1946DABF6A03A2A2 C3D0B05080AED6FC"));

  
  /* This test case has been problematic with the CBC test case */
  test_cipher2(&nettle_aes256, &nettle_unified_aes256,
	       SHEX("8d ae 93 ff fc 78 c9 44"
		    "2a bd 0c 1e 68 bc a6 c7"
		    "05 c7 84 e3 5a a9 11 8b"
		    "d3 16 aa 54 9b 44 08 9e"),
	       SHEX("a5 ce 55 d4 21 15 a1 c6 4a a4 0c b2 ca a6 d1 37"),
	       /* In the cbc test, I once got the bad value
		*   "b2 a0 6c d2 2f df 7d 2c  26 d2 42 88 8f 20 74 a2" */
	       SHEX("1f 94 fc 85 f2 36 21 06"
		    "4a ea e3 c9 cc 38 01 0e"));
  
  /* From draft NIST spec on AES modes.
   *
   * F.1 ECB Example Vectors
   * F.1.1 ECB-AES128-Encrypt
   */

  test_cipher2(&nettle_aes128, &nettle_unified_aes128,
	       SHEX("2b7e151628aed2a6abf7158809cf4f3c"),
	       SHEX("6bc1bee22e409f96e93d7e117393172a"
		    "ae2d8a571e03ac9c9eb76fac45af8e51"
		    "30c81c46a35ce411e5fbc1191a0a52ef"
		    "f69f2445df4f9b17ad2b417be66c3710"),
	       SHEX("3ad77bb40d7a3660a89ecaf32466ef97"
		    "f5d3d58503b9699de785895a96fdbaaf"
		    "43b1cd7f598ece23881b00e3ed030688"
		    "7b0c785e27e8ad3f8223207104725dd4"));

  /* F.1.3 ECB-AES192-Encrypt */

  test_cipher2(&nettle_aes192, &nettle_unified_aes192, 
	       SHEX("8e73b0f7da0e6452c810f32b809079e5 62f8ead2522c6b7b"),
	       SHEX("6bc1bee22e409f96e93d7e117393172a"
		    "ae2d8a571e03ac9c9eb76fac45af8e51"
		    "30c81c46a35ce411e5fbc1191a0a52ef"
		    "f69f2445df4f9b17ad2b417be66c3710"),
	       SHEX("bd334f1d6e45f25ff712a214571fa5cc"
		    "974104846d0ad3ad7734ecb3ecee4eef"
		    "ef7afd2270e2e60adce0ba2face6444e"
		    "9a4b41ba738d6c72fb16691603c18e0e"));

  /* F.1.5 ECB-AES256-Encrypt */
  test_cipher2(&nettle_aes256, &nettle_unified_aes256,
	       SHEX("603deb1015ca71be2b73aef0857d7781"
		    "1f352c073b6108d72d9810a30914dff4"),
	       SHEX("6bc1bee22e409f96e93d7e117393172a"
		    "ae2d8a571e03ac9c9eb76fac45af8e51" 
		    "30c81c46a35ce411e5fbc1191a0a52ef"
		    "f69f2445df4f9b17ad2b417be66c3710"),
	       SHEX("f3eed1bdb5d2a03c064b5a7e3db181f8"
		    "591ccb10d410ed26dc5ba74a31362870"
		    "b6ed21b99ca6f4f9f153e7b1beafed1d"
		    "23304b7a39f9f3ff067d8d8f9e24ecc7"));

  /* Test aes_invert_key with src != dst */
  test_invert(SHEX("0001020305060708 0A0B0C0D0F101112"),
	      SHEX("506812A45F08C889 B97F5980038B8359"),
	      SHEX("D8F532538289EF7D 06B506A4FD5BE9C9"));
  test_invert(SHEX("0001020305060708 0A0B0C0D0F101112"
		   "14151617191A1B1C"),
	      SHEX("2D33EEF2C0430A8A 9EBF45E809C40BB6"),
	      SHEX("DFF4945E0336DF4C 1C56BC700EFF837F"));
  test_invert(SHEX("0001020305060708 0A0B0C0D0F101112"
		   "14151617191A1B1C 1E1F202123242526"),
	      SHEX("834EADFCCAC7E1B30664B1ABA44815AB"),
	      SHEX("1946DABF6A03A2A2 C3D0B05080AED6FC"));
}

/* Internal state for the first test case:

   0: a7106950 81cf0e5a 8d5574b3 4b929b0c
   1: aa1e31c4 c19a8917  12282e4 b23e51eb
   2: 14be6dac fede8fdc 8fb98878 a27dfb5c
   3: e80a6f32 431515bb 72e8a651 7daf188b
   4: c50438c0 d464b2b6 76b875e9 b2b5f574
   5: d81ab740 746b4d89 ff033aac 44d5ffa2
   6: 52e6bb4a edadc170 24867df4 6e2ad5d5
   7: ab1c7365 64d09f00 7718d521 46a3df32
   8: f1eaad16  1aefdfb 7ba5724d d8499631
   9:  1020300  2030001  3000102    10203
  99: 5332f5d8 7def8982 a406b506 c9e95bfd

*/
