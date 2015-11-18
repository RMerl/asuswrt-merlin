#include "testutils.h"
#include "nettle-internal.h"
#include "gcm.h"

static void
test_gcm_hash (const struct tstring *msg, const struct tstring *ref)
{
  struct gcm_aes128_ctx ctx;
  const uint8_t z16[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
  uint8_t digest[16];

  ASSERT (ref->length == sizeof(digest));
  gcm_aes128_set_key (&ctx, z16);
  gcm_aes128_set_iv (&ctx, 16, z16);
  gcm_aes128_update (&ctx, msg->length, msg->data);
  gcm_aes128_digest (&ctx, sizeof(digest), digest);
  if (!MEMEQ (ref->length, ref->data, digest))
    {
      fprintf (stderr, "gcm_hash failed, msg: %s\nOutput: ", msg->data);
      print_hex (16, digest);
      fprintf(stderr, "Expected:");
      tstring_print_hex(ref);
      fprintf(stderr, "\n");
      FAIL();
    }
}

static nettle_set_key_func gcm_unified_aes128_set_key;
static nettle_set_key_func gcm_unified_aes128_set_iv;
static void
gcm_unified_aes128_set_key (void *ctx, const uint8_t *key)
{
  gcm_aes_set_key (ctx, AES128_KEY_SIZE, key);
}
static void
gcm_unified_aes128_set_iv (void *ctx, const uint8_t *iv)
{
  gcm_aes_set_iv (ctx, GCM_IV_SIZE, iv);
}
static const struct nettle_aead
nettle_gcm_unified_aes128 = {
  "gcm-aes128",
  sizeof (struct gcm_aes_ctx),
  GCM_BLOCK_SIZE, AES128_KEY_SIZE,
  GCM_IV_SIZE, GCM_DIGEST_SIZE,
  (nettle_set_key_func *) gcm_unified_aes128_set_key,
  (nettle_set_key_func *) gcm_unified_aes128_set_key,
  (nettle_set_key_func *) gcm_unified_aes128_set_iv,
  (nettle_hash_update_func *) gcm_aes_update,
  (nettle_crypt_func *) gcm_aes_encrypt,
  (nettle_crypt_func *) gcm_aes_decrypt,
  (nettle_hash_digest_func *) gcm_aes_digest
};
    

void
test_main(void)
{
  /* 
   * GCM-AES Test Vectors from
   * http://www.cryptobarn.com/papers/gcm-spec.pdf
   */

  /* Test case 1 */
  test_aead(&nettle_gcm_aes128, NULL,
	    SHEX("00000000000000000000000000000000"),	/* key */
	    SHEX(""),					/* auth data */ 
	    SHEX(""),					/* plaintext */
	    SHEX(""),					/* ciphertext*/
	    SHEX("000000000000000000000000"),		/* IV */
	    SHEX("58e2fccefa7e3061367f1d57a4e7455a"));	/* tag */

  /* Test case 2 */
  test_aead(&nettle_gcm_aes128, NULL,
	    SHEX("00000000000000000000000000000000"),
	    SHEX(""),
	    SHEX("00000000000000000000000000000000"),
	    SHEX("0388dace60b6a392f328c2b971b2fe78"),
	    SHEX("000000000000000000000000"),
	    SHEX("ab6e47d42cec13bdf53a67b21257bddf"));

  /* Test case 3 */
  test_aead(&nettle_gcm_aes128, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"),
	    SHEX(""),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b391aafd255"),
	    SHEX("42831ec2217774244b7221b784d0d49c"
		 "e3aa212f2c02a4e035c17e2329aca12e"
		 "21d514b25466931c7d8f6a5aac84aa05"
		 "1ba30b396a0aac973d58e091473f5985"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("4d5c2af327cd64a62cf35abd2ba6fab4"));

  /* Test case 4 */
  test_aead(&nettle_gcm_aes128, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("42831ec2217774244b7221b784d0d49c"
		 "e3aa212f2c02a4e035c17e2329aca12e"
		 "21d514b25466931c7d8f6a5aac84aa05"
		 "1ba30b396a0aac973d58e091"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("5bc94fbc3221a5db94fae95ae7121a47"));

  /* Test case 5 */
  test_aead(&nettle_gcm_aes128,
	    (nettle_hash_update_func *) gcm_aes128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("61353b4c2806934a777ff51fa22a4755"
		 "699b2a714fcdc6f83766e5f97b6c7423"
		 "73806900e49f24b22b097544d4896b42"
		 "4989b5e1ebac0f07c23f4598"),
	    SHEX("cafebabefacedbad"),
	    SHEX("3612d2e79e3b0785561be14aaca2fccb"));

  /* Test case 6 */
  test_aead(&nettle_gcm_aes128,
	    (nettle_hash_update_func *) gcm_aes128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("8ce24998625615b603a033aca13fb894"
		 "be9112a5c3a211a8ba262a3cca7e2ca7"
		 "01e4a9a4fba43c90ccdcb281d48c7c6f"
		 "d62875d2aca417034c34aee5"),
	    SHEX("9313225df88406e555909c5aff5269aa"
		 "6a7a9538534f7da1e4c303d2a318a728"
		 "c3c0c95156809539fcf0e2429a6b5254"
		 "16aedbf5a0de6a57a637b39b"),
	    SHEX("619cc5aefffe0bfa462af43c1699d050"));

  /* Same test, but with old gcm_aes interface */
  test_aead(&nettle_gcm_unified_aes128,
	    (nettle_hash_update_func *) gcm_aes_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("8ce24998625615b603a033aca13fb894"
		 "be9112a5c3a211a8ba262a3cca7e2ca7"
		 "01e4a9a4fba43c90ccdcb281d48c7c6f"
		 "d62875d2aca417034c34aee5"),
	    SHEX("9313225df88406e555909c5aff5269aa"
		 "6a7a9538534f7da1e4c303d2a318a728"
		 "c3c0c95156809539fcf0e2429a6b5254"
		 "16aedbf5a0de6a57a637b39b"),
	    SHEX("619cc5aefffe0bfa462af43c1699d050"));

  /* Test case 7 */
  test_aead(&nettle_gcm_aes192, NULL,
	    SHEX("00000000000000000000000000000000"
		 "0000000000000000"),
	    SHEX(""),
	    SHEX(""),
	    SHEX(""),
	    SHEX("000000000000000000000000"),
	    SHEX("cd33b28ac773f74ba00ed1f312572435"));

  /* Test case 8 */
  test_aead(&nettle_gcm_aes192, NULL,
	    SHEX("00000000000000000000000000000000"
		 "0000000000000000"),
	    SHEX(""),
	    SHEX("00000000000000000000000000000000"),
	    SHEX("98e7247c07f0fe411c267e4384b0f600"),
	    SHEX("000000000000000000000000"),
	    SHEX("2ff58d80033927ab8ef4d4587514f0fb"));

  /* Test case 9 */
  test_aead(&nettle_gcm_aes192, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c"),
	    SHEX(""),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b391aafd255"),
	    SHEX("3980ca0b3c00e841eb06fac4872a2757"
		 "859e1ceaa6efd984628593b40ca1e19c"
		 "7d773d00c144c525ac619d18c84a3f47"
		 "18e2448b2fe324d9ccda2710acade256"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("9924a7c8587336bfb118024db8674a14"));

  /* Test case 10 */
  test_aead(&nettle_gcm_aes192, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("3980ca0b3c00e841eb06fac4872a2757"
		 "859e1ceaa6efd984628593b40ca1e19c"
		 "7d773d00c144c525ac619d18c84a3f47"
		 "18e2448b2fe324d9ccda2710"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("2519498e80f1478f37ba55bd6d27618c"));

  /* Test case 11 */
  test_aead(&nettle_gcm_aes192,
	    (nettle_hash_update_func *) gcm_aes192_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("0f10f599ae14a154ed24b36e25324db8"
		 "c566632ef2bbb34f8347280fc4507057"
		 "fddc29df9a471f75c66541d4d4dad1c9"
		 "e93a19a58e8b473fa0f062f7"),
	    SHEX("cafebabefacedbad"),
	    SHEX("65dcc57fcf623a24094fcca40d3533f8"));

  /* Test case 12 */
  test_aead(&nettle_gcm_aes192,
	    (nettle_hash_update_func *) gcm_aes192_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("d27e88681ce3243c4830165a8fdcf9ff"
		 "1de9a1d8e6b447ef6ef7b79828666e45"
		 "81e79012af34ddd9e2f037589b292db3"
		 "e67c036745fa22e7e9b7373b"),
	    SHEX("9313225df88406e555909c5aff5269aa"
		 "6a7a9538534f7da1e4c303d2a318a728"
		 "c3c0c95156809539fcf0e2429a6b5254"
		 "16aedbf5a0de6a57a637b39b"),
	    SHEX("dcf566ff291c25bbb8568fc3d376a6d9"));

  /* Test case 13 */
  test_aead(&nettle_gcm_aes256, NULL,
	    SHEX("00000000000000000000000000000000"
		 "00000000000000000000000000000000"),
	    SHEX(""),
	    SHEX(""),
	    SHEX(""),
	    SHEX("000000000000000000000000"),
	    SHEX("530f8afbc74536b9a963b4f1c4cb738b"));

  /* Test case 14 */
  test_aead(&nettle_gcm_aes256, NULL,
	    SHEX("00000000000000000000000000000000"
		 "00000000000000000000000000000000"),
	    SHEX(""),
	    SHEX("00000000000000000000000000000000"),
	    SHEX("cea7403d4d606b6e074ec5d3baf39d18"),
	    SHEX("000000000000000000000000"),
	    SHEX("d0d1c8a799996bf0265b98b5d48ab919"));

  /* Test case 15 */
  test_aead(&nettle_gcm_aes256, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c6d6a8f9467308308"),
	    SHEX(""),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b391aafd255"),
	    SHEX("522dc1f099567d07f47f37a32a84427d"
		 "643a8cdcbfe5c0c97598a2bd2555d1aa"
		 "8cb08e48590dbb3da7b08b1056828838"
		 "c5f61e6393ba7a0abcc9f662898015ad"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("b094dac5d93471bdec1a502270e3cc6c"));

  /* Test case 16 */
  test_aead(&nettle_gcm_aes256, NULL,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("522dc1f099567d07f47f37a32a84427d"
		 "643a8cdcbfe5c0c97598a2bd2555d1aa"
		 "8cb08e48590dbb3da7b08b1056828838"
		 "c5f61e6393ba7a0abcc9f662"),
	    SHEX("cafebabefacedbaddecaf888"),
	    SHEX("76fc6ece0f4e1768cddf8853bb2d551b"));

  /* Test case 17 */
  test_aead(&nettle_gcm_aes256,
	    (nettle_hash_update_func *) gcm_aes256_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("c3762df1ca787d32ae47c13bf19844cb"
		 "af1ae14d0b976afac52ff7d79bba9de0"
		 "feb582d33934a4f0954cc2363bc73f78"
		 "62ac430e64abe499f47c9b1f"),
	    SHEX("cafebabefacedbad"),
	    SHEX("3a337dbf46a792c45e454913fe2ea8f2"));

  /* Test case 18 */
  test_aead(&nettle_gcm_aes256,
	    (nettle_hash_update_func *) gcm_aes256_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"
		 "feffe9928665731c6d6a8f9467308308"),
	    SHEX("feedfacedeadbeeffeedfacedeadbeef"
		 "abaddad2"),
	    SHEX("d9313225f88406e5a55909c5aff5269a"
		 "86a7a9531534f7da2e4c303d8a318a72"
		 "1c3c0c95956809532fcf0e2449a6b525"
		 "b16aedf5aa0de657ba637b39"),
	    SHEX("5a8def2f0c9e53f1f75d7853659e2a20"
		 "eeb2b22aafde6419a058ab4f6f746bf4"
		 "0fc0c3b780f244452da3ebf1c5d82cde"
		 "a2418997200ef82e44ae7e3f"),
	    SHEX("9313225df88406e555909c5aff5269aa"
		 "6a7a9538534f7da1e4c303d2a318a728"
		 "c3c0c95156809539fcf0e2429a6b5254"
		 "16aedbf5a0de6a57a637b39b"),
	    SHEX("a44a8266ee1c8eb0c8b5d4cf5ae9f19a"));



  /* 
   * GCM-Camellia Test Vectors obtained from the authors
   */

  /* Test case 1 */
  test_aead(&nettle_gcm_camellia128,
	    (nettle_hash_update_func *) gcm_camellia128_set_iv,
	    SHEX("00000000000000000000000000000000"),	/* key */
	    SHEX(""),					/* auth data */ 
	    SHEX(""),					/* plaintext */
	    SHEX(""),					/* ciphertext*/
	    SHEX("000000000000000000000000"),		/* IV */
	    SHEX("f5574acc3148dfcb9015200631024df9"));	/* tag */

  /* Test case 3 */
  test_aead(&nettle_gcm_camellia128,
	    (nettle_hash_update_func *) gcm_camellia128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),	/* key */
	    SHEX(""),					/* auth data */ 
	    SHEX("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
	         "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255"),					/* plaintext */
	    SHEX("d0d94a13b632f337a0cc9955b94fa020c815f903aab12f1efaf2fe9d90f729a6"
	         "cccbfa986ef2ff2c33de418d9a2529091cf18fe652c1cfde13f8260614bab815"),					/* ciphertext*/
	    SHEX("cafebabefacedbaddecaf888"),		/* IV */
	    SHEX("86e318012dd8329dc9dae6a170f61b24"));	/* tag */

  /* Test case 4 */
  test_aead(&nettle_gcm_camellia128,
	    (nettle_hash_update_func *) gcm_camellia128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeeffeedfacedeadbeefabaddad2"),					/* auth data */ 
	    SHEX("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
	         "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39"),					/* plaintext */
	    SHEX("d0d94a13b632f337a0cc9955b94fa020c815f903aab12f1efaf2fe9d90f729a6"
	         "cccbfa986ef2ff2c33de418d9a2529091cf18fe652c1cfde13f82606"),					/* ciphertext*/
	    SHEX("cafebabefacedbaddecaf888"),		/* IV */
	    SHEX("9f458869431576ea6a095456ec6b8101"));	/* tag */

  /* Test case 5 */
  test_aead(&nettle_gcm_camellia128,
	    (nettle_hash_update_func *) gcm_camellia128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeeffeedfacedeadbeefabaddad2"),					/* auth data */ 
	    SHEX("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
	         "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39"),					/* plaintext */
	    SHEX("28fd7434d5cd424a5353818fc21a982460d20cf632eb1e6c4fbfca17d5abcf6a"
	         "52111086162fe9570e7774c7a912aca3dfa10067ddaad40688645bdd"),					/* ciphertext*/
	    SHEX("cafebabefacedbad"),		/* IV */
	    SHEX("e86f8f2e730c49d536f00fb5225d28b1"));	/* tag */

  /* Test case 6 */
  test_aead(&nettle_gcm_camellia128,
	    (nettle_hash_update_func *) gcm_camellia128_set_iv,
	    SHEX("feffe9928665731c6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeeffeedfacedeadbeefabaddad2"),					/* auth data */ 
	    SHEX("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
	         "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39"),					/* plaintext */
	    SHEX("2e582b8417c93f2ff4f6f7ee3c361e4496e710ee12433baa964987d02f42953e"
	         "402e6f4af407fe08cd2f35123696014c34db19128df4056faebcd647"),					/* ciphertext*/
	    SHEX("9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728"
	         "c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b"),		/* IV */
	    SHEX("ceae5569b2af8641572622731aed3e53"));	/* tag */

  /* gcm-camellia256 */

  /* Test case 13 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("0000000000000000 0000000000000000"
		 "0000000000000000 0000000000000000"),	/* key */
	    SHEX(""),	/* auth data */
	    SHEX(""),	/* plaintext */
	    SHEX(""),	/* ciphertext */
	    SHEX("000000000000000000000000"),	/* iv */
	    SHEX("9cdb269b5d293bc5db9c55b057d9b591"));	/* tag */

  /* Test case 14 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("0000000000000000 0000000000000000"
		 "0000000000000000 0000000000000000"),	/* key */
	    SHEX(""),	/* auth data */
	    SHEX("0000000000000000 0000000000000000"),	/* plaintext */
	    SHEX("3d4b2cde666761ba 5dfb305178e667fb"),	/* ciphertext */
	    SHEX("000000000000000000000000"),	/* iv */
	    SHEX("284b63bb143c40ce100fb4dea6bb617b"));	/* tag */

  /* Test case 15 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("feffe9928665731c 6d6a8f9467308308"
		 "feffe9928665731c 6d6a8f9467308308"),	/* key */
	    SHEX(""),	/* auth data */
	    SHEX("d9313225f88406e5 a55909c5aff5269a"
		 "86a7a9531534f7da 2e4c303d8a318a72"
		 "1c3c0c9595680953 2fcf0e2449a6b525"
		 "b16aedf5aa0de657 ba637b391aafd255"),	/* plaintext */
	    SHEX("ad142c11579dd95e 41f3c1f324dabc25"
		 "5864d920f1b65759 d8f560d4948d4477"
		 "58dfdcf77aa9f625 81c7ff572a037f81"
		 "0cb1a9c4b3ca6ed6 38179b776549e092"),	/* ciphertext */
	    SHEX("cafebabefacedbaddecaf888"),	/* iv */
	    SHEX("c912686270a2b9966415fca3be75c468"));	/* tag */

  /* Test case 16 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("feffe9928665731c 6d6a8f9467308308"
		 "feffe9928665731c 6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeef feedfacedeadbeef"
		 "abaddad2"),	/* auth data */
	    SHEX("d9313225f88406e5 a55909c5aff5269a"
		 "86a7a9531534f7da 2e4c303d8a318a72"
		 "1c3c0c9595680953 2fcf0e2449a6b525"
		 "b16aedf5aa0de657 ba637b39"),	/* plaintext */
	    SHEX("ad142c11579dd95e 41f3c1f324dabc25"
		 "5864d920f1b65759 d8f560d4948d4477"
		 "58dfdcf77aa9f625 81c7ff572a037f81"
		 "0cb1a9c4b3ca6ed6 38179b77"),	/* ciphertext */
	    SHEX("cafebabefacedbaddecaf888"),	/* iv */
	    SHEX("4e4b178d8fe26fdc95e2e7246dd94bec"));	/* tag */

  /* Test case 17 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("feffe9928665731c 6d6a8f9467308308"
		 "feffe9928665731c 6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeef feedfacedeadbeef"
		 "abaddad2"),	/* auth data */
	    SHEX("d9313225f88406e5 a55909c5aff5269a"
		 "86a7a9531534f7da 2e4c303d8a318a72"
		 "1c3c0c9595680953 2fcf0e2449a6b525"
		 "b16aedf5aa0de657 ba637b39"),	/* plaintext */
	    SHEX("6ca95fbb7d16577a 9ef2fded94dc85b5"
		 "d40c629f6bef2c64 9888e3cbb0ededc7"
		 "810c04b12c2983bb bbc482e16e45c921"
		 "5ae12c15c55f2f48 09d06652"),	/* ciphertext */
	    SHEX("cafebabefacedbad"),	/* iv */
	    SHEX("e6472b8ebd331bfcc7c0fa63ce094461"));	/* tag */

  /* Test case 18 */
  test_aead(&nettle_gcm_camellia256,
	    (nettle_hash_update_func *) gcm_camellia256_set_iv,
	    SHEX("feffe9928665731c 6d6a8f9467308308"
		 "feffe9928665731c 6d6a8f9467308308"),	/* key */
	    SHEX("feedfacedeadbeef feedfacedeadbeef"
		 "abaddad2"),	/* auth data */
	    SHEX("d9313225f88406e5 a55909c5aff5269a"
		 "86a7a9531534f7da 2e4c303d8a318a72"
		 "1c3c0c9595680953 2fcf0e2449a6b525"
		 "b16aedf5aa0de657 ba637b39"),	/* plaintext */
	    SHEX("e0cddd7564d09c4d c522dd65949262bb"
		 "f9dcdb07421cf67f 3032becb7253c284"
		 "a16e5bf0f556a308 043f53fab9eebb52"
		 "6be7f7ad33d697ac 77c67862"),	/* ciphertext */
	    SHEX("9313225df88406e5 55909c5aff5269aa"
		 "6a7a9538534f7da1 e4c303d2a318a728"
		 "c3c0c95156809539 fcf0e2429a6b5254"
		 "16aedbf5a0de6a57 a637b39b"),	/* iv */
	    SHEX("5791883f822013f8bd136fc36fb9946b"));	/* tag */

  /* Test gcm_hash, with varying message size, keys and iv all zero.
     Not compared to any other implementation. */
  test_gcm_hash (SDATA("a"),
		 SHEX("1521c9a442bbf63b 2293a21d4874a5fd"));
  test_gcm_hash (SDATA("ab"),
		 SHEX("afb4592d2c7c1687 37f27271ee30412a"));
  test_gcm_hash (SDATA("abc"), 
		 SHEX("9543ca3e1662ba03 9a921ec2a20769be"));
  test_gcm_hash (SDATA("abcd"),
		 SHEX("8f041cc12bcb7e1b 0257a6da22ee1185"));
  test_gcm_hash (SDATA("abcde"),
		 SHEX("0b2376e5fed58ffb 717b520c27cd5c35"));
  test_gcm_hash (SDATA("abcdef"), 
		 SHEX("9679497a1eafa161 4942963380c1a76f"));
  test_gcm_hash (SDATA("abcdefg"),
		 SHEX("83862e40339536bc 723d9817f7df8282"));
  test_gcm_hash (SDATA("abcdefgh"), 
		 SHEX("b73bcc4d6815c4dc d7424a04e61b87c5"));
  test_gcm_hash (SDATA("abcdefghi"), 
		 SHEX("8e7846a383f0b3b2 07b01160a5ef993d"));
  test_gcm_hash (SDATA("abcdefghij"),
		 SHEX("37651643b6f8ecac 4ea1b320e6ea308c"));
  test_gcm_hash (SDATA("abcdefghijk"), 
		 SHEX("c1ce10106ee23286 f00513f55e2226b0"));
  test_gcm_hash (SDATA("abcdefghijkl"),
		 SHEX("c6a3e32a90196cdf b2c7a415d637e6ca"));
  test_gcm_hash (SDATA("abcdefghijklm"), 
		 SHEX("6cca29389d4444fa 3d20e65497088fd8"));
  test_gcm_hash (SDATA("abcdefghijklmn"),
		 SHEX("19476a997ec0a824 2022db0f0e8455ce"));
  test_gcm_hash (SDATA("abcdefghijklmno"), 
		 SHEX("f66931cee7eadcbb d42753c3ac3c4c16"));
  test_gcm_hash (SDATA("abcdefghijklmnop"),
		 SHEX("a79699ce8bed61f9 b8b1b4c5abb1712e"));
  test_gcm_hash (SDATA("abcdefghijklmnopq"), 
		 SHEX("65f8245330febf15 6fd95e324304c258"));
  test_gcm_hash (SDATA("abcdefghijklmnopqr"),
		 SHEX("d07259e85d4fc998 5a662eed41c8ed1d"));
}

