/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define CRYPTO_CURVE25519_PRIVATE
#include "or.h"
#include "test.h"
#include "aes.h"
#include "util.h"
#include "siphash.h"
#include "crypto_curve25519.h"
#include "crypto_ed25519.h"
#include "ed25519_vectors.inc"

#include <openssl/evp.h>

extern const char AUTHORITY_SIGNKEY_3[];
extern const char AUTHORITY_SIGNKEY_A_DIGEST[];
extern const char AUTHORITY_SIGNKEY_A_DIGEST256[];

/** Run unit tests for Diffie-Hellman functionality. */
static void
test_crypto_dh(void *arg)
{
  crypto_dh_t *dh1 = crypto_dh_new(DH_TYPE_CIRCUIT);
  crypto_dh_t *dh2 = crypto_dh_new(DH_TYPE_CIRCUIT);
  char p1[DH_BYTES];
  char p2[DH_BYTES];
  char s1[DH_BYTES];
  char s2[DH_BYTES];
  ssize_t s1len, s2len;

  (void)arg;
  tt_int_op(crypto_dh_get_bytes(dh1),OP_EQ, DH_BYTES);
  tt_int_op(crypto_dh_get_bytes(dh2),OP_EQ, DH_BYTES);

  memset(p1, 0, DH_BYTES);
  memset(p2, 0, DH_BYTES);
  tt_mem_op(p1,OP_EQ, p2, DH_BYTES);
  tt_assert(! crypto_dh_get_public(dh1, p1, DH_BYTES));
  tt_mem_op(p1,OP_NE, p2, DH_BYTES);
  tt_assert(! crypto_dh_get_public(dh2, p2, DH_BYTES));
  tt_mem_op(p1,OP_NE, p2, DH_BYTES);

  memset(s1, 0, DH_BYTES);
  memset(s2, 0xFF, DH_BYTES);
  s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p2, DH_BYTES, s1, 50);
  s2len = crypto_dh_compute_secret(LOG_WARN, dh2, p1, DH_BYTES, s2, 50);
  tt_assert(s1len > 0);
  tt_int_op(s1len,OP_EQ, s2len);
  tt_mem_op(s1,OP_EQ, s2, s1len);

  {
    /* XXXX Now fabricate some bad values and make sure they get caught,
     * Check 0, 1, N-1, >= N, etc.
     */
  }

 done:
  crypto_dh_free(dh1);
  crypto_dh_free(dh2);
}

/** Run unit tests for our random number generation function and its wrappers.
 */
static void
test_crypto_rng(void *arg)
{
  int i, j, allok;
  char data1[100], data2[100];
  double d;

  /* Try out RNG. */
  (void)arg;
  tt_assert(! crypto_seed_rng());
  crypto_rand(data1, 100);
  crypto_rand(data2, 100);
  tt_mem_op(data1,OP_NE, data2,100);
  allok = 1;
  for (i = 0; i < 100; ++i) {
    uint64_t big;
    char *host;
    j = crypto_rand_int(100);
    if (j < 0 || j >= 100)
      allok = 0;
    big = crypto_rand_uint64(U64_LITERAL(1)<<40);
    if (big >= (U64_LITERAL(1)<<40))
      allok = 0;
    big = crypto_rand_uint64(U64_LITERAL(5));
    if (big >= 5)
      allok = 0;
    d = crypto_rand_double();
    tt_assert(d >= 0);
    tt_assert(d < 1.0);
    host = crypto_random_hostname(3,8,"www.",".onion");
    if (strcmpstart(host,"www.") ||
        strcmpend(host,".onion") ||
        strlen(host) < 13 ||
        strlen(host) > 18)
      allok = 0;
    tor_free(host);
  }
  tt_assert(allok);
 done:
  ;
}

static void
test_crypto_rng_range(void *arg)
{
  int got_smallest = 0, got_largest = 0;
  int i;

  (void)arg;
  for (i = 0; i < 1000; ++i) {
    int x = crypto_rand_int_range(5,9);
    tt_int_op(x, OP_GE, 5);
    tt_int_op(x, OP_LT, 9);
    if (x == 5)
      got_smallest = 1;
    if (x == 8)
      got_largest = 1;
  }

  /* These fail with probability 1/10^603. */
  tt_assert(got_smallest);
  tt_assert(got_largest);
 done:
  ;
}

/** Run unit tests for our AES functionality */
static void
test_crypto_aes(void *arg)
{
  char *data1 = NULL, *data2 = NULL, *data3 = NULL;
  crypto_cipher_t *env1 = NULL, *env2 = NULL;
  int i, j;
  char *mem_op_hex_tmp=NULL;

  int use_evp = !strcmp(arg,"evp");
  evaluate_evp_for_aes(use_evp);
  evaluate_ctr_for_aes();

  data1 = tor_malloc(1024);
  data2 = tor_malloc(1024);
  data3 = tor_malloc(1024);

  /* Now, test encryption and decryption with stream cipher. */
  data1[0]='\0';
  for (i = 1023; i>0; i -= 35)
    strncat(data1, "Now is the time for all good onions", i);

  memset(data2, 0, 1024);
  memset(data3, 0, 1024);
  env1 = crypto_cipher_new(NULL);
  tt_ptr_op(env1, OP_NE, NULL);
  env2 = crypto_cipher_new(crypto_cipher_get_key(env1));
  tt_ptr_op(env2, OP_NE, NULL);

  /* Try encrypting 512 chars. */
  crypto_cipher_encrypt(env1, data2, data1, 512);
  crypto_cipher_decrypt(env2, data3, data2, 512);
  tt_mem_op(data1,OP_EQ, data3, 512);
  tt_mem_op(data1,OP_NE, data2, 512);

  /* Now encrypt 1 at a time, and get 1 at a time. */
  for (j = 512; j < 560; ++j) {
    crypto_cipher_encrypt(env1, data2+j, data1+j, 1);
  }
  for (j = 512; j < 560; ++j) {
    crypto_cipher_decrypt(env2, data3+j, data2+j, 1);
  }
  tt_mem_op(data1,OP_EQ, data3, 560);
  /* Now encrypt 3 at a time, and get 5 at a time. */
  for (j = 560; j < 1024-5; j += 3) {
    crypto_cipher_encrypt(env1, data2+j, data1+j, 3);
  }
  for (j = 560; j < 1024-5; j += 5) {
    crypto_cipher_decrypt(env2, data3+j, data2+j, 5);
  }
  tt_mem_op(data1,OP_EQ, data3, 1024-5);
  /* Now make sure that when we encrypt with different chunk sizes, we get
     the same results. */
  crypto_cipher_free(env2);
  env2 = NULL;

  memset(data3, 0, 1024);
  env2 = crypto_cipher_new(crypto_cipher_get_key(env1));
  tt_ptr_op(env2, OP_NE, NULL);
  for (j = 0; j < 1024-16; j += 17) {
    crypto_cipher_encrypt(env2, data3+j, data1+j, 17);
  }
  for (j= 0; j < 1024-16; ++j) {
    if (data2[j] != data3[j]) {
      printf("%d:  %d\t%d\n", j, (int) data2[j], (int) data3[j]);
    }
  }
  tt_mem_op(data2,OP_EQ, data3, 1024-16);
  crypto_cipher_free(env1);
  env1 = NULL;
  crypto_cipher_free(env2);
  env2 = NULL;

  /* NIST test vector for aes. */
  /* IV starts at 0 */
  env1 = crypto_cipher_new("\x80\x00\x00\x00\x00\x00\x00\x00"
                           "\x00\x00\x00\x00\x00\x00\x00\x00");
  crypto_cipher_encrypt(env1, data1,
                        "\x00\x00\x00\x00\x00\x00\x00\x00"
                        "\x00\x00\x00\x00\x00\x00\x00\x00", 16);
  test_memeq_hex(data1, "0EDD33D3C621E546455BD8BA1418BEC8");

  /* Now test rollover.  All these values are originally from a python
   * script. */
  crypto_cipher_free(env1);
  env1 = crypto_cipher_new_with_iv(
                                   "\x80\x00\x00\x00\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x00\x00\x00",
                                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                                   "\xff\xff\xff\xff\xff\xff\xff\xff");
  memset(data2, 0,  1024);
  crypto_cipher_encrypt(env1, data1, data2, 32);
  test_memeq_hex(data1, "335fe6da56f843199066c14a00a40231"
                        "cdd0b917dbc7186908a6bfb5ffd574d3");
  crypto_cipher_free(env1);
  env1 = crypto_cipher_new_with_iv(
                                   "\x80\x00\x00\x00\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x00\x00\x00",
                                   "\x00\x00\x00\x00\xff\xff\xff\xff"
                                   "\xff\xff\xff\xff\xff\xff\xff\xff");
  memset(data2, 0,  1024);
  crypto_cipher_encrypt(env1, data1, data2, 32);
  test_memeq_hex(data1, "e627c6423fa2d77832a02b2794094b73"
                        "3e63c721df790d2c6469cc1953a3ffac");
  crypto_cipher_free(env1);
  env1 = crypto_cipher_new_with_iv(
                                   "\x80\x00\x00\x00\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x00\x00\x00",
                                   "\xff\xff\xff\xff\xff\xff\xff\xff"
                                   "\xff\xff\xff\xff\xff\xff\xff\xff");
  memset(data2, 0,  1024);
  crypto_cipher_encrypt(env1, data1, data2, 32);
  test_memeq_hex(data1, "2aed2bff0de54f9328efd070bf48f70a"
                        "0EDD33D3C621E546455BD8BA1418BEC8");

  /* Now check rollover on inplace cipher. */
  crypto_cipher_free(env1);
  env1 = crypto_cipher_new_with_iv(
                                   "\x80\x00\x00\x00\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x00\x00\x00",
                                   "\xff\xff\xff\xff\xff\xff\xff\xff"
                                   "\xff\xff\xff\xff\xff\xff\xff\xff");
  crypto_cipher_crypt_inplace(env1, data2, 64);
  test_memeq_hex(data2, "2aed2bff0de54f9328efd070bf48f70a"
                        "0EDD33D3C621E546455BD8BA1418BEC8"
                        "93e2c5243d6839eac58503919192f7ae"
                        "1908e67cafa08d508816659c2e693191");
  crypto_cipher_free(env1);
  env1 = crypto_cipher_new_with_iv(
                                   "\x80\x00\x00\x00\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x00\x00\x00",
                                   "\xff\xff\xff\xff\xff\xff\xff\xff"
                                   "\xff\xff\xff\xff\xff\xff\xff\xff");
  crypto_cipher_crypt_inplace(env1, data2, 64);
  tt_assert(tor_mem_is_zero(data2, 64));

 done:
  tor_free(mem_op_hex_tmp);
  if (env1)
    crypto_cipher_free(env1);
  if (env2)
    crypto_cipher_free(env2);
  tor_free(data1);
  tor_free(data2);
  tor_free(data3);
}

/** Run unit tests for our SHA-1 functionality */
static void
test_crypto_sha(void *arg)
{
  crypto_digest_t *d1 = NULL, *d2 = NULL;
  int i;
  char key[160];
  char digest[32];
  char data[50];
  char d_out1[DIGEST_LEN], d_out2[DIGEST256_LEN];
  char *mem_op_hex_tmp=NULL;

  /* Test SHA-1 with a test vector from the specification. */
  (void)arg;
  i = crypto_digest(data, "abc", 3);
  test_memeq_hex(data, "A9993E364706816ABA3E25717850C26C9CD0D89D");
  tt_int_op(i, OP_EQ, 0);

  /* Test SHA-256 with a test vector from the specification. */
  i = crypto_digest256(data, "abc", 3, DIGEST_SHA256);
  test_memeq_hex(data, "BA7816BF8F01CFEA414140DE5DAE2223B00361A3"
                       "96177A9CB410FF61F20015AD");
  tt_int_op(i, OP_EQ, 0);

  /* Test HMAC-SHA256 with test cases from wikipedia and RFC 4231 */

  /* Case empty (wikipedia) */
  crypto_hmac_sha256(digest, "", 0, "", 0);
  tt_str_op(hex_str(digest, 32),OP_EQ,
           "B613679A0814D9EC772F95D778C35FC5FF1697C493715653C6C712144292C5AD");

  /* Case quick-brown (wikipedia) */
  crypto_hmac_sha256(digest, "key", 3,
                     "The quick brown fox jumps over the lazy dog", 43);
  tt_str_op(hex_str(digest, 32),OP_EQ,
           "F7BC83F430538424B13298E6AA6FB143EF4D59A14946175997479DBC2D1A3CD8");

  /* "Test Case 1" from RFC 4231 */
  memset(key, 0x0b, 20);
  crypto_hmac_sha256(digest, key, 20, "Hi There", 8);
  test_memeq_hex(digest,
                 "b0344c61d8db38535ca8afceaf0bf12b"
                 "881dc200c9833da726e9376c2e32cff7");

  /* "Test Case 2" from RFC 4231 */
  memset(key, 0x0b, 20);
  crypto_hmac_sha256(digest, "Jefe", 4, "what do ya want for nothing?", 28);
  test_memeq_hex(digest,
                 "5bdcc146bf60754e6a042426089575c7"
                 "5a003f089d2739839dec58b964ec3843");

  /* "Test case 3" from RFC 4231 */
  memset(key, 0xaa, 20);
  memset(data, 0xdd, 50);
  crypto_hmac_sha256(digest, key, 20, data, 50);
  test_memeq_hex(digest,
                 "773ea91e36800e46854db8ebd09181a7"
                 "2959098b3ef8c122d9635514ced565fe");

  /* "Test case 4" from RFC 4231 */
  base16_decode(key, 25,
                "0102030405060708090a0b0c0d0e0f10111213141516171819", 50);
  memset(data, 0xcd, 50);
  crypto_hmac_sha256(digest, key, 25, data, 50);
  test_memeq_hex(digest,
                 "82558a389a443c0ea4cc819899f2083a"
                 "85f0faa3e578f8077a2e3ff46729665b");

  /* "Test case 5" from RFC 4231 */
  memset(key, 0x0c, 20);
  crypto_hmac_sha256(digest, key, 20, "Test With Truncation", 20);
  test_memeq_hex(digest,
                 "a3b6167473100ee06e0c796c2955552b");

  /* "Test case 6" from RFC 4231 */
  memset(key, 0xaa, 131);
  crypto_hmac_sha256(digest, key, 131,
                     "Test Using Larger Than Block-Size Key - Hash Key First",
                     54);
  test_memeq_hex(digest,
                 "60e431591ee0b67f0d8a26aacbf5b77f"
                 "8e0bc6213728c5140546040f0ee37f54");

  /* "Test case 7" from RFC 4231 */
  memset(key, 0xaa, 131);
  crypto_hmac_sha256(digest, key, 131,
                     "This is a test using a larger than block-size key and a "
                     "larger than block-size data. The key needs to be hashed "
                     "before being used by the HMAC algorithm.", 152);
  test_memeq_hex(digest,
                 "9b09ffa71b942fcb27635fbcd5b0e944"
                 "bfdc63644f0713938a7f51535c3a35e2");

  /* Incremental digest code. */
  d1 = crypto_digest_new();
  tt_assert(d1);
  crypto_digest_add_bytes(d1, "abcdef", 6);
  d2 = crypto_digest_dup(d1);
  tt_assert(d2);
  crypto_digest_add_bytes(d2, "ghijkl", 6);
  crypto_digest_get_digest(d2, d_out1, sizeof(d_out1));
  crypto_digest(d_out2, "abcdefghijkl", 12);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, sizeof(d_out1));
  crypto_digest(d_out2, "abcdefmno", 9);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_get_digest(d1, d_out1, sizeof(d_out1));
  crypto_digest(d_out2, "abcdef", 6);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_free(d1);
  crypto_digest_free(d2);

  /* Incremental digest code with sha256 */
  d1 = crypto_digest256_new(DIGEST_SHA256);
  tt_assert(d1);
  crypto_digest_add_bytes(d1, "abcdef", 6);
  d2 = crypto_digest_dup(d1);
  tt_assert(d2);
  crypto_digest_add_bytes(d2, "ghijkl", 6);
  crypto_digest_get_digest(d2, d_out1, sizeof(d_out1));
  crypto_digest256(d_out2, "abcdefghijkl", 12, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, sizeof(d_out1));
  crypto_digest256(d_out2, "abcdefmno", 9, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_get_digest(d1, d_out1, sizeof(d_out1));
  crypto_digest256(d_out2, "abcdef", 6, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);

 done:
  if (d1)
    crypto_digest_free(d1);
  if (d2)
    crypto_digest_free(d2);
  tor_free(mem_op_hex_tmp);
}

/** Run unit tests for our public key crypto functions */
static void
test_crypto_pk(void *arg)
{
  crypto_pk_t *pk1 = NULL, *pk2 = NULL;
  char *encoded = NULL;
  char data1[1024], data2[1024], data3[1024];
  size_t size;
  int i, len;

  /* Public-key ciphers */
  (void)arg;
  pk1 = pk_generate(0);
  pk2 = crypto_pk_new();
  tt_assert(pk1 && pk2);
  tt_assert(! crypto_pk_write_public_key_to_string(pk1, &encoded, &size));
  tt_assert(! crypto_pk_read_public_key_from_string(pk2, encoded, size));
  tt_int_op(0,OP_EQ, crypto_pk_cmp_keys(pk1, pk2));

  /* comparison between keys and NULL */
  tt_int_op(crypto_pk_cmp_keys(NULL, pk1), OP_LT, 0);
  tt_int_op(crypto_pk_cmp_keys(NULL, NULL), OP_EQ, 0);
  tt_int_op(crypto_pk_cmp_keys(pk1, NULL), OP_GT, 0);

  tt_int_op(128,OP_EQ, crypto_pk_keysize(pk1));
  tt_int_op(1024,OP_EQ, crypto_pk_num_bits(pk1));
  tt_int_op(128,OP_EQ, crypto_pk_keysize(pk2));
  tt_int_op(1024,OP_EQ, crypto_pk_num_bits(pk2));

  tt_int_op(128,OP_EQ, crypto_pk_public_encrypt(pk2, data1, sizeof(data1),
                                        "Hello whirled.", 15,
                                        PK_PKCS1_OAEP_PADDING));
  tt_int_op(128,OP_EQ, crypto_pk_public_encrypt(pk1, data2, sizeof(data1),
                                        "Hello whirled.", 15,
                                        PK_PKCS1_OAEP_PADDING));
  /* oaep padding should make encryption not match */
  tt_mem_op(data1,OP_NE, data2, 128);
  tt_int_op(15,OP_EQ,
            crypto_pk_private_decrypt(pk1, data3, sizeof(data3), data1, 128,
                                        PK_PKCS1_OAEP_PADDING,1));
  tt_str_op(data3,OP_EQ, "Hello whirled.");
  memset(data3, 0, 1024);
  tt_int_op(15,OP_EQ,
            crypto_pk_private_decrypt(pk1, data3, sizeof(data3), data2, 128,
                                        PK_PKCS1_OAEP_PADDING,1));
  tt_str_op(data3,OP_EQ, "Hello whirled.");
  /* Can't decrypt with public key. */
  tt_int_op(-1,OP_EQ,
            crypto_pk_private_decrypt(pk2, data3, sizeof(data3), data2, 128,
                                        PK_PKCS1_OAEP_PADDING,1));
  /* Try again with bad padding */
  memcpy(data2+1, "XYZZY", 5);  /* This has fails ~ once-in-2^40 */
  tt_int_op(-1,OP_EQ,
            crypto_pk_private_decrypt(pk1, data3, sizeof(data3), data2, 128,
                                        PK_PKCS1_OAEP_PADDING,1));

  /* File operations: save and load private key */
  tt_assert(! crypto_pk_write_private_key_to_filename(pk1,
                                                        get_fname("pkey1")));
  /* failing case for read: can't read. */
  tt_assert(crypto_pk_read_private_key_from_filename(pk2,
                                                   get_fname("xyzzy")) < 0);
  write_str_to_file(get_fname("xyzzy"), "foobar", 6);
  /* Failing case for read: no key. */
  tt_assert(crypto_pk_read_private_key_from_filename(pk2,
                                                   get_fname("xyzzy")) < 0);
  tt_assert(! crypto_pk_read_private_key_from_filename(pk2,
                                                         get_fname("pkey1")));
  tt_int_op(15,OP_EQ,
            crypto_pk_private_decrypt(pk2, data3, sizeof(data3), data1, 128,
                                        PK_PKCS1_OAEP_PADDING,1));

  /* Now try signing. */
  strlcpy(data1, "Ossifrage", 1024);
  tt_int_op(128,OP_EQ,
            crypto_pk_private_sign(pk1, data2, sizeof(data2), data1, 10));
  tt_int_op(10,OP_EQ,
          crypto_pk_public_checksig(pk1, data3, sizeof(data3), data2, 128));
  tt_str_op(data3,OP_EQ, "Ossifrage");
  /* Try signing digests. */
  tt_int_op(128,OP_EQ, crypto_pk_private_sign_digest(pk1, data2, sizeof(data2),
                                             data1, 10));
  tt_int_op(20,OP_EQ,
          crypto_pk_public_checksig(pk1, data3, sizeof(data3), data2, 128));
  tt_int_op(0,OP_EQ,
            crypto_pk_public_checksig_digest(pk1, data1, 10, data2, 128));
  tt_int_op(-1,OP_EQ,
            crypto_pk_public_checksig_digest(pk1, data1, 11, data2, 128));

  /*XXXX test failed signing*/

  /* Try encoding */
  crypto_pk_free(pk2);
  pk2 = NULL;
  i = crypto_pk_asn1_encode(pk1, data1, 1024);
  tt_int_op(i, OP_GT, 0);
  pk2 = crypto_pk_asn1_decode(data1, i);
  tt_assert(crypto_pk_cmp_keys(pk1,pk2) == 0);

  /* Try with hybrid encryption wrappers. */
  crypto_rand(data1, 1024);
  for (i = 85; i < 140; ++i) {
    memset(data2,0,1024);
    memset(data3,0,1024);
    len = crypto_pk_public_hybrid_encrypt(pk1,data2,sizeof(data2),
                                          data1,i,PK_PKCS1_OAEP_PADDING,0);
    tt_int_op(len, OP_GE, 0);
    len = crypto_pk_private_hybrid_decrypt(pk1,data3,sizeof(data3),
                                           data2,len,PK_PKCS1_OAEP_PADDING,1);
    tt_int_op(len,OP_EQ, i);
    tt_mem_op(data1,OP_EQ, data3,i);
  }

  /* Try copy_full */
  crypto_pk_free(pk2);
  pk2 = crypto_pk_copy_full(pk1);
  tt_assert(pk2 != NULL);
  tt_ptr_op(pk1, OP_NE, pk2);
  tt_assert(crypto_pk_cmp_keys(pk1,pk2) == 0);

 done:
  if (pk1)
    crypto_pk_free(pk1);
  if (pk2)
    crypto_pk_free(pk2);
  tor_free(encoded);
}

static void
test_crypto_pk_fingerprints(void *arg)
{
  crypto_pk_t *pk = NULL;
  char encoded[512];
  char d[DIGEST_LEN], d2[DIGEST_LEN];
  char fingerprint[FINGERPRINT_LEN+1];
  int n;
  unsigned i;
  char *mem_op_hex_tmp=NULL;

  (void)arg;

  pk = pk_generate(1);
  tt_assert(pk);
  n = crypto_pk_asn1_encode(pk, encoded, sizeof(encoded));
  tt_int_op(n, OP_GT, 0);
  tt_int_op(n, OP_GT, 128);
  tt_int_op(n, OP_LT, 256);

  /* Is digest as expected? */
  crypto_digest(d, encoded, n);
  tt_int_op(0, OP_EQ, crypto_pk_get_digest(pk, d2));
  tt_mem_op(d,OP_EQ, d2, DIGEST_LEN);

  /* Is fingerprint right? */
  tt_int_op(0, OP_EQ, crypto_pk_get_fingerprint(pk, fingerprint, 0));
  tt_int_op(strlen(fingerprint), OP_EQ, DIGEST_LEN * 2);
  test_memeq_hex(d, fingerprint);

  /* Are spaces right? */
  tt_int_op(0, OP_EQ, crypto_pk_get_fingerprint(pk, fingerprint, 1));
  for (i = 4; i < strlen(fingerprint); i += 5) {
    tt_int_op(fingerprint[i], OP_EQ, ' ');
  }
  tor_strstrip(fingerprint, " ");
  tt_int_op(strlen(fingerprint), OP_EQ, DIGEST_LEN * 2);
  test_memeq_hex(d, fingerprint);

  /* Now hash again and check crypto_pk_get_hashed_fingerprint. */
  crypto_digest(d2, d, sizeof(d));
  tt_int_op(0, OP_EQ, crypto_pk_get_hashed_fingerprint(pk, fingerprint));
  tt_int_op(strlen(fingerprint), OP_EQ, DIGEST_LEN * 2);
  test_memeq_hex(d2, fingerprint);

 done:
  crypto_pk_free(pk);
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_pk_base64(void *arg)
{
  crypto_pk_t *pk1 = NULL;
  crypto_pk_t *pk2 = NULL;
  char *encoded = NULL;

  (void)arg;

  /* Test Base64 encoding a key. */
  pk1 = pk_generate(0);
  tt_assert(pk1);
  tt_int_op(0, OP_EQ, crypto_pk_base64_encode(pk1, &encoded));
  tt_assert(encoded);

  /* Test decoding a valid key. */
  pk2 = crypto_pk_base64_decode(encoded, strlen(encoded));
  tt_assert(pk2);
  tt_assert(crypto_pk_cmp_keys(pk1,pk2) == 0);
  crypto_pk_free(pk2);

  /* Test decoding a invalid key (not Base64). */
  static const char *invalid_b64 = "The key is in another castle!";
  pk2 = crypto_pk_base64_decode(invalid_b64, strlen(invalid_b64));
  tt_assert(!pk2);

  /* Test decoding a truncated Base64 blob. */
  pk2 = crypto_pk_base64_decode(encoded, strlen(encoded)/2);
  tt_assert(!pk2);

 done:
  crypto_pk_free(pk1);
  crypto_pk_free(pk2);
  tor_free(encoded);
}

/** Sanity check for crypto pk digests  */
static void
test_crypto_digests(void *arg)
{
  crypto_pk_t *k = NULL;
  ssize_t r;
  digests_t pkey_digests;
  char digest[DIGEST_LEN];

  (void)arg;
  k = crypto_pk_new();
  tt_assert(k);
  r = crypto_pk_read_private_key_from_string(k, AUTHORITY_SIGNKEY_3, -1);
  tt_assert(!r);

  r = crypto_pk_get_digest(k, digest);
  tt_assert(r == 0);
  tt_mem_op(hex_str(digest, DIGEST_LEN),OP_EQ,
             AUTHORITY_SIGNKEY_A_DIGEST, HEX_DIGEST_LEN);

  r = crypto_pk_get_all_digests(k, &pkey_digests);

  tt_mem_op(hex_str(pkey_digests.d[DIGEST_SHA1], DIGEST_LEN),OP_EQ,
             AUTHORITY_SIGNKEY_A_DIGEST, HEX_DIGEST_LEN);
  tt_mem_op(hex_str(pkey_digests.d[DIGEST_SHA256], DIGEST256_LEN),OP_EQ,
             AUTHORITY_SIGNKEY_A_DIGEST256, HEX_DIGEST256_LEN);
 done:
  crypto_pk_free(k);
}

/** Encode src into dest with OpenSSL's EVP Encode interface, returning the
 * length of the encoded data in bytes.
 */
static int
base64_encode_evp(char *dest, char *src, size_t srclen)
{
  const unsigned char *s = (unsigned char*)src;
  EVP_ENCODE_CTX ctx;
  int len, ret;

  EVP_EncodeInit(&ctx);
  EVP_EncodeUpdate(&ctx, (unsigned char *)dest, &len, s, (int)srclen);
  EVP_EncodeFinal(&ctx, (unsigned char *)(dest + len), &ret);
  return ret+ len;
}

/** Run unit tests for misc crypto formatting functionality (base64, base32,
 * fingerprints, etc) */
static void
test_crypto_formats(void *arg)
{
  char *data1 = NULL, *data2 = NULL, *data3 = NULL;
  int i, j, idx;

  (void)arg;
  data1 = tor_malloc(1024);
  data2 = tor_malloc(1024);
  data3 = tor_malloc(1024);
  tt_assert(data1 && data2 && data3);

  /* Base64 tests */
  memset(data1, 6, 1024);
  for (idx = 0; idx < 10; ++idx) {
    i = base64_encode(data2, 1024, data1, idx, 0);
    tt_int_op(i, OP_GE, 0);
    tt_int_op(i, OP_EQ, strlen(data2));
    j = base64_decode(data3, 1024, data2, i);
    tt_int_op(j,OP_EQ, idx);
    tt_mem_op(data3,OP_EQ, data1, idx);

    i = base64_encode_nopad(data2, 1024, (uint8_t*)data1, idx);
    tt_int_op(i, OP_GE, 0);
    tt_int_op(i, OP_EQ, strlen(data2));
    tt_assert(! strchr(data2, '='));
    j = base64_decode_nopad((uint8_t*)data3, 1024, data2, i);
    tt_int_op(j, OP_EQ, idx);
    tt_mem_op(data3,OP_EQ, data1, idx);
  }

  strlcpy(data1, "Test string that contains 35 chars.", 1024);
  strlcat(data1, " 2nd string that contains 35 chars.", 1024);

  i = base64_encode(data2, 1024, data1, 71, 0);
  tt_int_op(i, OP_GE, 0);
  j = base64_decode(data3, 1024, data2, i);
  tt_int_op(j,OP_EQ, 71);
  tt_str_op(data3,OP_EQ, data1);
  tt_int_op(data2[i], OP_EQ, '\0');

  crypto_rand(data1, DIGEST_LEN);
  memset(data2, 100, 1024);
  digest_to_base64(data2, data1);
  tt_int_op(BASE64_DIGEST_LEN,OP_EQ, strlen(data2));
  tt_int_op(100,OP_EQ, data2[BASE64_DIGEST_LEN+2]);
  memset(data3, 99, 1024);
  tt_int_op(digest_from_base64(data3, data2),OP_EQ, 0);
  tt_mem_op(data1,OP_EQ, data3, DIGEST_LEN);
  tt_int_op(99,OP_EQ, data3[DIGEST_LEN+1]);

  tt_assert(digest_from_base64(data3, "###") < 0);

  for (i = 0; i < 256; i++) {
    /* Test the multiline format Base64 encoder with 0 .. 256 bytes of
     * output against OpenSSL.
     */
    const size_t enclen = base64_encode_size(i, BASE64_ENCODE_MULTILINE);
    data1[i] = i;
    j = base64_encode(data2, 1024, data1, i, BASE64_ENCODE_MULTILINE);
    tt_int_op(j, OP_EQ, enclen);
    j = base64_encode_evp(data3, data1, i);
    tt_int_op(j, OP_EQ, enclen);
    tt_mem_op(data2, OP_EQ, data3, enclen);
    tt_int_op(j, OP_EQ, strlen(data2));
  }

  /* Encoding SHA256 */
  crypto_rand(data2, DIGEST256_LEN);
  memset(data2, 100, 1024);
  digest256_to_base64(data2, data1);
  tt_int_op(BASE64_DIGEST256_LEN,OP_EQ, strlen(data2));
  tt_int_op(100,OP_EQ, data2[BASE64_DIGEST256_LEN+2]);
  memset(data3, 99, 1024);
  tt_int_op(digest256_from_base64(data3, data2),OP_EQ, 0);
  tt_mem_op(data1,OP_EQ, data3, DIGEST256_LEN);
  tt_int_op(99,OP_EQ, data3[DIGEST256_LEN+1]);

  /* Base32 tests */
  strlcpy(data1, "5chrs", 1024);
  /* bit pattern is:  [35 63 68 72 73] ->
   *        [00110101 01100011 01101000 01110010 01110011]
   * By 5s: [00110 10101 10001 10110 10000 11100 10011 10011]
   */
  base32_encode(data2, 9, data1, 5);
  tt_str_op(data2,OP_EQ, "gvrwq4tt");

  strlcpy(data1, "\xFF\xF5\x6D\x44\xAE\x0D\x5C\xC9\x62\xC4", 1024);
  base32_encode(data2, 30, data1, 10);
  tt_str_op(data2,OP_EQ, "772w2rfobvomsywe");

  /* Base16 tests */
  strlcpy(data1, "6chrs\xff", 1024);
  base16_encode(data2, 13, data1, 6);
  tt_str_op(data2,OP_EQ, "3663687273FF");

  strlcpy(data1, "f0d678affc000100", 1024);
  i = base16_decode(data2, 8, data1, 16);
  tt_int_op(i,OP_EQ, 0);
  tt_mem_op(data2,OP_EQ, "\xf0\xd6\x78\xaf\xfc\x00\x01\x00",8);

  /* now try some failing base16 decodes */
  tt_int_op(-1,OP_EQ, base16_decode(data2, 8, data1, 15)); /* odd input len */
  tt_int_op(-1,OP_EQ, base16_decode(data2, 7, data1, 16)); /* dest too short */
  strlcpy(data1, "f0dz!8affc000100", 1024);
  tt_int_op(-1,OP_EQ, base16_decode(data2, 8, data1, 16));

  tor_free(data1);
  tor_free(data2);
  tor_free(data3);

  /* Add spaces to fingerprint */
  {
    data1 = tor_strdup("ABCD1234ABCD56780000ABCD1234ABCD56780000");
    tt_int_op(strlen(data1),OP_EQ, 40);
    data2 = tor_malloc(FINGERPRINT_LEN+1);
    crypto_add_spaces_to_fp(data2, FINGERPRINT_LEN+1, data1);
    tt_str_op(data2, OP_EQ,
              "ABCD 1234 ABCD 5678 0000 ABCD 1234 ABCD 5678 0000");
    tor_free(data1);
    tor_free(data2);
  }

 done:
  tor_free(data1);
  tor_free(data2);
  tor_free(data3);
}

/** Test AES-CTR encryption and decryption with IV. */
static void
test_crypto_aes_iv(void *arg)
{
  char *plain, *encrypted1, *encrypted2, *decrypted1, *decrypted2;
  char plain_1[1], plain_15[15], plain_16[16], plain_17[17];
  char key1[16], key2[16];
  ssize_t encrypted_size, decrypted_size;

  int use_evp = !strcmp(arg,"evp");
  evaluate_evp_for_aes(use_evp);

  plain = tor_malloc(4095);
  encrypted1 = tor_malloc(4095 + 1 + 16);
  encrypted2 = tor_malloc(4095 + 1 + 16);
  decrypted1 = tor_malloc(4095 + 1);
  decrypted2 = tor_malloc(4095 + 1);

  crypto_rand(plain, 4095);
  crypto_rand(key1, 16);
  crypto_rand(key2, 16);
  crypto_rand(plain_1, 1);
  crypto_rand(plain_15, 15);
  crypto_rand(plain_16, 16);
  crypto_rand(plain_17, 17);
  key1[0] = key2[0] + 128; /* Make sure that contents are different. */
  /* Encrypt and decrypt with the same key. */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted1, 16 + 4095,
                                                 plain, 4095);

  tt_int_op(encrypted_size,OP_EQ, 16 + 4095);
  tt_assert(encrypted_size > 0); /* This is obviously true, since 4111 is
                                   * greater than 0, but its truth is not
                                   * obvious to all analysis tools. */
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 4095,
                                             encrypted1, encrypted_size);

  tt_int_op(decrypted_size,OP_EQ, 4095);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain,OP_EQ, decrypted1, 4095);
  /* Encrypt a second time (with a new random initialization vector). */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted2, 16 + 4095,
                                             plain, 4095);

  tt_int_op(encrypted_size,OP_EQ, 16 + 4095);
  tt_assert(encrypted_size > 0);
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted2, 4095,
                                             encrypted2, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 4095);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain,OP_EQ, decrypted2, 4095);
  tt_mem_op(encrypted1,OP_NE, encrypted2, encrypted_size);
  /* Decrypt with the wrong key. */
  decrypted_size = crypto_cipher_decrypt_with_iv(key2, decrypted2, 4095,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 4095);
  tt_mem_op(plain,OP_NE, decrypted2, decrypted_size);
  /* Alter the initialization vector. */
  encrypted1[0] += 42;
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 4095,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 4095);
  tt_mem_op(plain,OP_NE, decrypted2, 4095);
  /* Special length case: 1. */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted1, 16 + 1,
                                             plain_1, 1);
  tt_int_op(encrypted_size,OP_EQ, 16 + 1);
  tt_assert(encrypted_size > 0);
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 1,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 1);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain_1,OP_EQ, decrypted1, 1);
  /* Special length case: 15. */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted1, 16 + 15,
                                             plain_15, 15);
  tt_int_op(encrypted_size,OP_EQ, 16 + 15);
  tt_assert(encrypted_size > 0);
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 15,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 15);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain_15,OP_EQ, decrypted1, 15);
  /* Special length case: 16. */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted1, 16 + 16,
                                             plain_16, 16);
  tt_int_op(encrypted_size,OP_EQ, 16 + 16);
  tt_assert(encrypted_size > 0);
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 16,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 16);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain_16,OP_EQ, decrypted1, 16);
  /* Special length case: 17. */
  encrypted_size = crypto_cipher_encrypt_with_iv(key1, encrypted1, 16 + 17,
                                             plain_17, 17);
  tt_int_op(encrypted_size,OP_EQ, 16 + 17);
  tt_assert(encrypted_size > 0);
  decrypted_size = crypto_cipher_decrypt_with_iv(key1, decrypted1, 17,
                                             encrypted1, encrypted_size);
  tt_int_op(decrypted_size,OP_EQ, 17);
  tt_assert(decrypted_size > 0);
  tt_mem_op(plain_17,OP_EQ, decrypted1, 17);

 done:
  /* Free memory. */
  tor_free(plain);
  tor_free(encrypted1);
  tor_free(encrypted2);
  tor_free(decrypted1);
  tor_free(decrypted2);
}

/** Test base32 decoding. */
static void
test_crypto_base32_decode(void *arg)
{
  char plain[60], encoded[96 + 1], decoded[60];
  int res;
  (void)arg;
  crypto_rand(plain, 60);
  /* Encode and decode a random string. */
  base32_encode(encoded, 96 + 1, plain, 60);
  res = base32_decode(decoded, 60, encoded, 96);
  tt_int_op(res,OP_EQ, 0);
  tt_mem_op(plain,OP_EQ, decoded, 60);
  /* Encode, uppercase, and decode a random string. */
  base32_encode(encoded, 96 + 1, plain, 60);
  tor_strupper(encoded);
  res = base32_decode(decoded, 60, encoded, 96);
  tt_int_op(res,OP_EQ, 0);
  tt_mem_op(plain,OP_EQ, decoded, 60);
  /* Change encoded string and decode. */
  if (encoded[0] == 'A' || encoded[0] == 'a')
    encoded[0] = 'B';
  else
    encoded[0] = 'A';
  res = base32_decode(decoded, 60, encoded, 96);
  tt_int_op(res,OP_EQ, 0);
  tt_mem_op(plain,OP_NE, decoded, 60);
  /* Bad encodings. */
  encoded[0] = '!';
  res = base32_decode(decoded, 60, encoded, 96);
  tt_int_op(0, OP_GT, res);

 done:
  ;
}

static void
test_crypto_kdf_TAP(void *arg)
{
  uint8_t key_material[100];
  int r;
  char *mem_op_hex_tmp = NULL;

  (void)arg;
#define EXPAND(s)                                \
  r = crypto_expand_key_material_TAP(            \
    (const uint8_t*)(s), strlen(s),              \
    key_material, 100)

  /* Test vectors generated with a little python script; feel free to write
   * your own. */
  memset(key_material, 0, sizeof(key_material));
  EXPAND("");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "5ba93c9db0cff93f52b521d7420e43f6eda2784fbf8b4530d8"
                 "d246dd74ac53a13471bba17941dff7c4ea21bb365bbeeaf5f2"
                 "c654883e56d11e43c44e9842926af7ca0a8cca12604f945414"
                 "f07b01e13da42c6cf1de3abfdea9b95f34687cbbe92b9a7383");

  EXPAND("Tor");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "776c6214fc647aaa5f683c737ee66ec44f03d0372e1cce6922"
                 "7950f236ddf1e329a7ce7c227903303f525a8c6662426e8034"
                 "870642a6dabbd41b5d97ec9bf2312ea729992f48f8ea2d0ba8"
                 "3f45dfda1a80bdc8b80de01b23e3e0ffae099b3e4ccf28dc28");

  EXPAND("AN ALARMING ITEM TO FIND ON A MONTHLY AUTO-DEBIT NOTICE");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "a340b5d126086c3ab29c2af4179196dbf95e1c72431419d331"
                 "4844bf8f6afb6098db952b95581fb6c33625709d6f4400b8e7"
                 "ace18a70579fad83c0982ef73f89395bcc39493ad53a685854"
                 "daf2ba9b78733b805d9a6824c907ee1dba5ac27a1e466d4d10");

 done:
  tor_free(mem_op_hex_tmp);

#undef EXPAND
}

static void
test_crypto_hkdf_sha256(void *arg)
{
  uint8_t key_material[100];
  const uint8_t salt[] = "ntor-curve25519-sha256-1:key_extract";
  const size_t salt_len = strlen((char*)salt);
  const uint8_t m_expand[] = "ntor-curve25519-sha256-1:key_expand";
  const size_t m_expand_len = strlen((char*)m_expand);
  int r;
  char *mem_op_hex_tmp = NULL;

  (void)arg;

#define EXPAND(s) \
  r = crypto_expand_key_material_rfc5869_sha256( \
    (const uint8_t*)(s), strlen(s),              \
    salt, salt_len,                              \
    m_expand, m_expand_len,                      \
    key_material, 100)

  /* Test vectors generated with ntor_ref.py */
  memset(key_material, 0, sizeof(key_material));
  EXPAND("");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "d3490ed48b12a48f9547861583573fe3f19aafe3f81dc7fc75"
                 "eeed96d741b3290f941576c1f9f0b2d463d1ec7ab2c6bf71cd"
                 "d7f826c6298c00dbfe6711635d7005f0269493edf6046cc7e7"
                 "dcf6abe0d20c77cf363e8ffe358927817a3d3e73712cee28d8");

  EXPAND("Tor");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "5521492a85139a8d9107a2d5c0d9c91610d0f95989975ebee6"
                 "c02a4f8d622a6cfdf9b7c7edd3832e2760ded1eac309b76f8d"
                 "66c4a3c4d6225429b3a016e3c3d45911152fc87bc2de9630c3"
                 "961be9fdb9f93197ea8e5977180801926d3321fa21513e59ac");

  EXPAND("AN ALARMING ITEM TO FIND ON YOUR CREDIT-RATING STATEMENT");
  tt_int_op(r, OP_EQ, 0);
  test_memeq_hex(key_material,
                 "a2aa9b50da7e481d30463adb8f233ff06e9571a0ca6ab6df0f"
                 "b206fa34e5bc78d063fc291501beec53b36e5a0e434561200c"
                 "5f8bd13e0f88b3459600b4dc21d69363e2895321c06184879d"
                 "94b18f078411be70b767c7fc40679a9440a0c95ea83a23efbf");

 done:
  tor_free(mem_op_hex_tmp);
#undef EXPAND
}

static void
test_crypto_curve25519_impl(void *arg)
{
  /* adapted from curve25519_donna, which adapted it from test-curve25519
     version 20050915, by D. J. Bernstein, Public domain. */

  const int randomize_high_bit = (arg != NULL);

#ifdef SLOW_CURVE25519_TEST
  const int loop_max=10000;
  const char e1_expected[]    = "4faf81190869fd742a33691b0e0824d5"
                                "7e0329f4dd2819f5f32d130f1296b500";
  const char e2k_expected[]   = "05aec13f92286f3a781ccae98995a3b9"
                                "e0544770bc7de853b38f9100489e3e79";
  const char e1e2k_expected[] = "cd6e8269104eb5aaee886bd2071fba88"
                                "bd13861475516bc2cd2b6e005e805064";
#else
  const int loop_max=200;
  const char e1_expected[]    = "bc7112cde03f97ef7008cad1bdc56be3"
                                "c6a1037d74cceb3712e9206871dcf654";
  const char e2k_expected[]   = "dd8fa254fb60bdb5142fe05b1f5de44d"
                                "8e3ee1a63c7d14274ea5d4c67f065467";
  const char e1e2k_expected[] = "7ddb98bd89025d2347776b33901b3e7e"
                                "c0ee98cb2257a4545c0cfb2ca3e1812b";
#endif

  unsigned char e1k[32];
  unsigned char e2k[32];
  unsigned char e1e2k[32];
  unsigned char e2e1k[32];
  unsigned char e1[32] = {3};
  unsigned char e2[32] = {5};
  unsigned char k[32] = {9};
  int loop, i;

  char *mem_op_hex_tmp = NULL;

  for (loop = 0; loop < loop_max; ++loop) {
    curve25519_impl(e1k,e1,k);
    curve25519_impl(e2e1k,e2,e1k);
    curve25519_impl(e2k,e2,k);
    if (randomize_high_bit) {
      /* We require that the high bit of the public key be ignored. So if
       * we're doing this variant test, we randomize the high bit of e2k, and
       * make sure that the handshake still works out the same as it would
       * otherwise. */
      uint8_t byte;
      crypto_rand((char*)&byte, 1);
      e2k[31] |= (byte & 0x80);
    }
    curve25519_impl(e1e2k,e1,e2k);
    tt_mem_op(e1e2k,OP_EQ, e2e1k, 32);
    if (loop == loop_max-1) {
      break;
    }
    for (i = 0;i < 32;++i) e1[i] ^= e2k[i];
    for (i = 0;i < 32;++i) e2[i] ^= e1k[i];
    for (i = 0;i < 32;++i) k[i] ^= e1e2k[i];
  }

  test_memeq_hex(e1, e1_expected);
  test_memeq_hex(e2k, e2k_expected);
  test_memeq_hex(e1e2k, e1e2k_expected);

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_curve25519_basepoint(void *arg)
{
  uint8_t secret[32];
  uint8_t public1[32];
  uint8_t public2[32];
  const int iters = 2048;
  int i;
  (void) arg;

  for (i = 0; i < iters; ++i) {
    crypto_rand((char*)secret, 32);
    curve25519_set_impl_params(1); /* Use optimization */
    curve25519_basepoint_impl(public1, secret);
    curve25519_set_impl_params(0); /* Disable optimization */
    curve25519_basepoint_impl(public2, secret);
    tt_mem_op(public1, OP_EQ, public2, 32);
  }

 done:
  ;
}

static void
test_crypto_curve25519_wrappers(void *arg)
{
  curve25519_public_key_t pubkey1, pubkey2;
  curve25519_secret_key_t seckey1, seckey2;

  uint8_t output1[CURVE25519_OUTPUT_LEN];
  uint8_t output2[CURVE25519_OUTPUT_LEN];
  (void)arg;

  /* Test a simple handshake, serializing and deserializing some stuff. */
  curve25519_secret_key_generate(&seckey1, 0);
  curve25519_secret_key_generate(&seckey2, 1);
  curve25519_public_key_generate(&pubkey1, &seckey1);
  curve25519_public_key_generate(&pubkey2, &seckey2);
  tt_assert(curve25519_public_key_is_ok(&pubkey1));
  tt_assert(curve25519_public_key_is_ok(&pubkey2));
  curve25519_handshake(output1, &seckey1, &pubkey2);
  curve25519_handshake(output2, &seckey2, &pubkey1);
  tt_mem_op(output1,OP_EQ, output2, sizeof(output1));

 done:
  ;
}

static void
test_crypto_curve25519_encode(void *arg)
{
  curve25519_secret_key_t seckey;
  curve25519_public_key_t key1, key2, key3;
  char buf[64];

  (void)arg;

  curve25519_secret_key_generate(&seckey, 0);
  curve25519_public_key_generate(&key1, &seckey);
  tt_int_op(0, OP_EQ, curve25519_public_to_base64(buf, &key1));
  tt_int_op(CURVE25519_BASE64_PADDED_LEN, OP_EQ, strlen(buf));

  tt_int_op(0, OP_EQ, curve25519_public_from_base64(&key2, buf));
  tt_mem_op(key1.public_key,OP_EQ, key2.public_key, CURVE25519_PUBKEY_LEN);

  buf[CURVE25519_BASE64_PADDED_LEN - 1] = '\0';
  tt_int_op(CURVE25519_BASE64_PADDED_LEN-1, OP_EQ, strlen(buf));
  tt_int_op(0, OP_EQ, curve25519_public_from_base64(&key3, buf));
  tt_mem_op(key1.public_key,OP_EQ, key3.public_key, CURVE25519_PUBKEY_LEN);

  /* Now try bogus parses. */
  strlcpy(buf, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$=", sizeof(buf));
  tt_int_op(-1, OP_EQ, curve25519_public_from_base64(&key3, buf));

  strlcpy(buf, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$", sizeof(buf));
  tt_int_op(-1, OP_EQ, curve25519_public_from_base64(&key3, buf));

  strlcpy(buf, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", sizeof(buf));
  tt_int_op(-1, OP_EQ, curve25519_public_from_base64(&key3, buf));

 done:
  ;
}

static void
test_crypto_curve25519_persist(void *arg)
{
  curve25519_keypair_t keypair, keypair2;
  char *fname = tor_strdup(get_fname("curve25519_keypair"));
  char *tag = NULL;
  char *content = NULL;
  const char *cp;
  struct stat st;
  size_t taglen;

  (void)arg;

  tt_int_op(0,OP_EQ,curve25519_keypair_generate(&keypair, 0));

  tt_int_op(0,OP_EQ,
            curve25519_keypair_write_to_file(&keypair, fname, "testing"));
  tt_int_op(0,OP_EQ,curve25519_keypair_read_from_file(&keypair2, &tag, fname));
  tt_str_op(tag,OP_EQ,"testing");
  tor_free(tag);

  tt_mem_op(keypair.pubkey.public_key,OP_EQ,
             keypair2.pubkey.public_key,
             CURVE25519_PUBKEY_LEN);
  tt_mem_op(keypair.seckey.secret_key,OP_EQ,
             keypair2.seckey.secret_key,
             CURVE25519_SECKEY_LEN);

  content = read_file_to_str(fname, RFTS_BIN, &st);
  tt_assert(content);
  taglen = strlen("== c25519v1: testing ==");
  tt_u64_op((uint64_t)st.st_size, OP_EQ,
            32+CURVE25519_PUBKEY_LEN+CURVE25519_SECKEY_LEN);
  tt_assert(fast_memeq(content, "== c25519v1: testing ==", taglen));
  tt_assert(tor_mem_is_zero(content+taglen, 32-taglen));
  cp = content + 32;
  tt_mem_op(keypair.seckey.secret_key,OP_EQ,
             cp,
             CURVE25519_SECKEY_LEN);
  cp += CURVE25519_SECKEY_LEN;
  tt_mem_op(keypair.pubkey.public_key,OP_EQ,
             cp,
             CURVE25519_SECKEY_LEN);

  tor_free(fname);
  fname = tor_strdup(get_fname("bogus_keypair"));

  tt_int_op(-1, OP_EQ,
            curve25519_keypair_read_from_file(&keypair2, &tag, fname));
  tor_free(tag);

  content[69] ^= 0xff;
  tt_int_op(0, OP_EQ,
            write_bytes_to_file(fname, content, (size_t)st.st_size, 1));
  tt_int_op(-1, OP_EQ,
            curve25519_keypair_read_from_file(&keypair2, &tag, fname));

 done:
  tor_free(fname);
  tor_free(content);
  tor_free(tag);
}

static void
test_crypto_ed25519_simple(void *arg)
{
  ed25519_keypair_t kp1, kp2;
  ed25519_public_key_t pub1, pub2;
  ed25519_secret_key_t sec1, sec2;
  ed25519_signature_t sig1, sig2;
  const uint8_t msg[] =
    "GNU will be able to run Unix programs, "
    "but will not be identical to Unix.";
  const uint8_t msg2[] =
    "Microsoft Windows extends the features of the DOS operating system, "
    "yet is compatible with most existing applications that run under DOS.";
  size_t msg_len = strlen((const char*)msg);
  size_t msg2_len = strlen((const char*)msg2);

  (void)arg;

  tt_int_op(0, OP_EQ, ed25519_secret_key_generate(&sec1, 0));
  tt_int_op(0, OP_EQ, ed25519_secret_key_generate(&sec2, 1));

  tt_int_op(0, OP_EQ, ed25519_public_key_generate(&pub1, &sec1));
  tt_int_op(0, OP_EQ, ed25519_public_key_generate(&pub2, &sec1));

  tt_mem_op(pub1.pubkey, OP_EQ, pub2.pubkey, sizeof(pub1.pubkey));
  tt_assert(ed25519_pubkey_eq(&pub1, &pub2));
  tt_assert(ed25519_pubkey_eq(&pub1, &pub1));

  memcpy(&kp1.pubkey, &pub1, sizeof(pub1));
  memcpy(&kp1.seckey, &sec1, sizeof(sec1));
  tt_int_op(0, OP_EQ, ed25519_sign(&sig1, msg, msg_len, &kp1));
  tt_int_op(0, OP_EQ, ed25519_sign(&sig2, msg, msg_len, &kp1));

  /* Ed25519 signatures are deterministic */
  tt_mem_op(sig1.sig, OP_EQ, sig2.sig, sizeof(sig1.sig));

  /* Basic signature is valid. */
  tt_int_op(0, OP_EQ, ed25519_checksig(&sig1, msg, msg_len, &pub1));

  /* Altered signature doesn't work. */
  sig1.sig[0] ^= 3;
  tt_int_op(-1, OP_EQ, ed25519_checksig(&sig1, msg, msg_len, &pub1));

  /* Wrong public key doesn't work. */
  tt_int_op(0, OP_EQ, ed25519_public_key_generate(&pub2, &sec2));
  tt_int_op(-1, OP_EQ, ed25519_checksig(&sig2, msg, msg_len, &pub2));
  tt_assert(! ed25519_pubkey_eq(&pub1, &pub2));

  /* Wrong message doesn't work. */
  tt_int_op(0, OP_EQ, ed25519_checksig(&sig2, msg, msg_len, &pub1));
  tt_int_op(-1, OP_EQ, ed25519_checksig(&sig2, msg, msg_len-1, &pub1));
  tt_int_op(-1, OP_EQ, ed25519_checksig(&sig2, msg2, msg2_len, &pub1));

  /* Batch signature checking works with some bad. */
  tt_int_op(0, OP_EQ, ed25519_keypair_generate(&kp2, 0));
  tt_int_op(0, OP_EQ, ed25519_sign(&sig1, msg, msg_len, &kp2));
  {
    ed25519_checkable_t ch[] = {
      { &pub1, sig2, msg, msg_len }, /*ok*/
      { &pub1, sig2, msg, msg_len-1 }, /*bad*/
      { &kp2.pubkey, sig2, msg2, msg2_len }, /*bad*/
      { &kp2.pubkey, sig1, msg, msg_len }, /*ok*/
    };
    int okay[4];
    tt_int_op(-2, OP_EQ, ed25519_checksig_batch(okay, ch, 4));
    tt_int_op(okay[0], OP_EQ, 1);
    tt_int_op(okay[1], OP_EQ, 0);
    tt_int_op(okay[2], OP_EQ, 0);
    tt_int_op(okay[3], OP_EQ, 1);
    tt_int_op(-2, OP_EQ, ed25519_checksig_batch(NULL, ch, 4));
  }

  /* Batch signature checking works with all good. */
  {
    ed25519_checkable_t ch[] = {
      { &pub1, sig2, msg, msg_len }, /*ok*/
      { &kp2.pubkey, sig1, msg, msg_len }, /*ok*/
    };
    int okay[2];
    tt_int_op(0, OP_EQ, ed25519_checksig_batch(okay, ch, 2));
    tt_int_op(okay[0], OP_EQ, 1);
    tt_int_op(okay[1], OP_EQ, 1);
    tt_int_op(0, OP_EQ, ed25519_checksig_batch(NULL, ch, 2));
  }

 done:
  ;
}

static void
test_crypto_ed25519_test_vectors(void *arg)
{
  char *mem_op_hex_tmp=NULL;
  int i;
  struct {
    const char *sk;
    const char *pk;
    const char *sig;
    const char *msg;
  } items[] = {
    /* These test vectors were generated with the "ref" implementation of
     * ed25519 from SUPERCOP-20130419 */
    { "4c6574277320686f706520746865726520617265206e6f206275677320696e20",
      "f3e0e493b30f56e501aeb868fc912fe0c8b76621efca47a78f6d75875193dd87",
      "b5d7fd6fd3adf643647ce1fe87a2931dedd1a4e38e6c662bedd35cdd80bfac51"
        "1b2c7d1ee6bd929ac213014e1a8dc5373854c7b25dbe15ec96bf6c94196fae06",
      "506c6561736520657863757365206d7920667269656e642e2048652069736e2774"
      "204e554c2d7465726d696e617465642e"
    },

    { "74686520696d706c656d656e746174696f6e20776869636820617265206e6f74",
      "407f0025a1e1351a4cb68e92f5c0ebaf66e7aaf93a4006a4d1a66e3ede1cfeac",
      "02884fde1c3c5944d0ecf2d133726fc820c303aae695adceabf3a1e01e95bf28"
        "da88c0966f5265e9c6f8edc77b3b96b5c91baec3ca993ccd21a3f64203600601",
      "506c6561736520657863757365206d7920667269656e642e2048652069736e2774"
      "204e554c2d7465726d696e617465642e"
    },
    { "6578706f73656420627920456e676c697368207465787420617320696e707574",
      "61681cb5fbd69f9bc5a462a21a7ab319011237b940bc781cdc47fcbe327e7706",
      "6a127d0414de7510125d4bc214994ffb9b8857a46330832d05d1355e882344ad"
        "f4137e3ca1f13eb9cc75c887ef2309b98c57528b4acd9f6376c6898889603209",
      "506c6561736520657863757365206d7920667269656e642e2048652069736e2774"
      "204e554c2d7465726d696e617465642e"
    },

    /* These come from "sign.input" in ed25519's page */
    { "5b5a619f8ce1c66d7ce26e5a2ae7b0c04febcd346d286c929e19d0d5973bfef9",
      "6fe83693d011d111131c4f3fbaaa40a9d3d76b30012ff73bb0e39ec27ab18257",
      "0f9ad9793033a2fa06614b277d37381e6d94f65ac2a5a94558d09ed6ce922258"
        "c1a567952e863ac94297aec3c0d0c8ddf71084e504860bb6ba27449b55adc40e",
      "5a8d9d0a22357e6655f9c785"
    },
    { "940c89fe40a81dafbdb2416d14ae469119869744410c3303bfaa0241dac57800",
      "a2eb8c0501e30bae0cf842d2bde8dec7386f6b7fc3981b8c57c9792bb94cf2dd",
      "d8bb64aad8c9955a115a793addd24f7f2b077648714f49c4694ec995b330d09d"
        "640df310f447fd7b6cb5c14f9fe9f490bcf8cfadbfd2169c8ac20d3b8af49a0c",
      "b87d3813e03f58cf19fd0b6395"
    },
    { "9acad959d216212d789a119252ebfe0c96512a23c73bd9f3b202292d6916a738",
      "cf3af898467a5b7a52d33d53bc037e2642a8da996903fc252217e9c033e2f291",
      "6ee3fe81e23c60eb2312b2006b3b25e6838e02106623f844c44edb8dafd66ab0"
        "671087fd195df5b8f58a1d6e52af42908053d55c7321010092748795ef94cf06",
      "55c7fa434f5ed8cdec2b7aeac173",
    },
    { "d5aeee41eeb0e9d1bf8337f939587ebe296161e6bf5209f591ec939e1440c300",
      "fd2a565723163e29f53c9de3d5e8fbe36a7ab66e1439ec4eae9c0a604af291a5",
      "f68d04847e5b249737899c014d31c805c5007a62c0a10d50bb1538c5f3550395"
        "1fbc1e08682f2cc0c92efe8f4985dec61dcbd54d4b94a22547d24451271c8b00",
      "0a688e79be24f866286d4646b5d81c"
    },

    { NULL, NULL, NULL, NULL}
  };

  (void)arg;

  for (i = 0; items[i].pk; ++i) {
    ed25519_keypair_t kp;
    ed25519_signature_t sig;
    uint8_t sk_seed[32];
    uint8_t *msg;
    size_t msg_len;
    base16_decode((char*)sk_seed, sizeof(sk_seed),
                  items[i].sk, 64);
    ed25519_secret_key_from_seed(&kp.seckey, sk_seed);
    tt_int_op(0, OP_EQ, ed25519_public_key_generate(&kp.pubkey, &kp.seckey));
    test_memeq_hex(kp.pubkey.pubkey, items[i].pk);

    msg_len = strlen(items[i].msg) / 2;
    msg = tor_malloc(msg_len);
    base16_decode((char*)msg, msg_len, items[i].msg, strlen(items[i].msg));

    tt_int_op(0, OP_EQ, ed25519_sign(&sig, msg, msg_len, &kp));
    test_memeq_hex(sig.sig, items[i].sig);

    tor_free(msg);
  }

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_ed25519_encode(void *arg)
{
  char buf[ED25519_SIG_BASE64_LEN+1];
  ed25519_keypair_t kp;
  ed25519_public_key_t pk;
  ed25519_signature_t sig1, sig2;
  char *mem_op_hex_tmp = NULL;
  (void) arg;

  /* Test roundtrip. */
  tt_int_op(0, OP_EQ, ed25519_keypair_generate(&kp, 0));
  tt_int_op(0, OP_EQ, ed25519_public_to_base64(buf, &kp.pubkey));
  tt_int_op(ED25519_BASE64_LEN, OP_EQ, strlen(buf));
  tt_int_op(0, OP_EQ, ed25519_public_from_base64(&pk, buf));
  tt_mem_op(kp.pubkey.pubkey, OP_EQ, pk.pubkey, ED25519_PUBKEY_LEN);

  tt_int_op(0, OP_EQ, ed25519_sign(&sig1, (const uint8_t*)"ABC", 3, &kp));
  tt_int_op(0, OP_EQ, ed25519_signature_to_base64(buf, &sig1));
  tt_int_op(0, OP_EQ, ed25519_signature_from_base64(&sig2, buf));
  tt_mem_op(sig1.sig, OP_EQ, sig2.sig, ED25519_SIG_LEN);

  /* Test known value. */
  tt_int_op(0, OP_EQ, ed25519_public_from_base64(&pk,
                             "lVIuIctLjbGZGU5wKMNXxXlSE3cW4kaqkqm04u6pxvM"));
  test_memeq_hex(pk.pubkey,
         "95522e21cb4b8db199194e7028c357c57952137716e246aa92a9b4e2eea9c6f3");

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_ed25519_convert(void *arg)
{
  const uint8_t msg[] =
    "The eyes are not here / There are no eyes here.";
  const int N = 30;
  int i;
  (void)arg;

  for (i = 0; i < N; ++i) {
    curve25519_keypair_t curve25519_keypair;
    ed25519_keypair_t ed25519_keypair;
    ed25519_public_key_t ed25519_pubkey;

    int bit=0;
    ed25519_signature_t sig;

    tt_int_op(0,OP_EQ,curve25519_keypair_generate(&curve25519_keypair, i&1));
    tt_int_op(0,OP_EQ,ed25519_keypair_from_curve25519_keypair(
                              &ed25519_keypair, &bit, &curve25519_keypair));
    tt_int_op(0,OP_EQ,ed25519_public_key_from_curve25519_public_key(
                        &ed25519_pubkey, &curve25519_keypair.pubkey, bit));
    tt_mem_op(ed25519_pubkey.pubkey, OP_EQ, ed25519_keypair.pubkey.pubkey, 32);

    tt_int_op(0,OP_EQ,ed25519_sign(&sig, msg, sizeof(msg), &ed25519_keypair));
    tt_int_op(0,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg),
                                    &ed25519_pubkey));

    tt_int_op(-1,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg)-1,
                                     &ed25519_pubkey));
    sig.sig[0] ^= 15;
    tt_int_op(-1,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg),
                                     &ed25519_pubkey));
  }

 done:
  ;
}

static void
test_crypto_ed25519_blinding(void *arg)
{
  const uint8_t msg[] =
    "Eyes I dare not meet in dreams / In death's dream kingdom";

  const int N = 30;
  int i;
  (void)arg;

  for (i = 0; i < N; ++i) {
    uint8_t blinding[32];
    ed25519_keypair_t ed25519_keypair;
    ed25519_keypair_t ed25519_keypair_blinded;
    ed25519_public_key_t ed25519_pubkey_blinded;

    ed25519_signature_t sig;

    crypto_rand((char*) blinding, sizeof(blinding));

    tt_int_op(0,OP_EQ,ed25519_keypair_generate(&ed25519_keypair, 0));
    tt_int_op(0,OP_EQ,ed25519_keypair_blind(&ed25519_keypair_blinded,
                                         &ed25519_keypair, blinding));

    tt_int_op(0,OP_EQ,ed25519_public_blind(&ed25519_pubkey_blinded,
                                        &ed25519_keypair.pubkey, blinding));

    tt_mem_op(ed25519_pubkey_blinded.pubkey, OP_EQ,
              ed25519_keypair_blinded.pubkey.pubkey, 32);

    tt_int_op(0,OP_EQ,ed25519_sign(&sig, msg, sizeof(msg),
                                &ed25519_keypair_blinded));

    tt_int_op(0,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg),
                                    &ed25519_pubkey_blinded));

    tt_int_op(-1,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg)-1,
                                     &ed25519_pubkey_blinded));
    sig.sig[0] ^= 15;
    tt_int_op(-1,OP_EQ,ed25519_checksig(&sig, msg, sizeof(msg),
                                     &ed25519_pubkey_blinded));
  }

 done:
  ;
}

static void
test_crypto_ed25519_testvectors(void *arg)
{
  unsigned i;
  char *mem_op_hex_tmp = NULL;
  (void)arg;

  for (i = 0; i < ARRAY_LENGTH(ED25519_SECRET_KEYS); ++i) {
    uint8_t sk[32];
    ed25519_secret_key_t esk;
    ed25519_public_key_t pk, blind_pk, pkfromcurve;
    ed25519_keypair_t keypair, blind_keypair;
    curve25519_keypair_t curvekp;
    uint8_t blinding_param[32];
    ed25519_signature_t sig;
    int sign;

#define DECODE(p,s) base16_decode((char*)(p),sizeof(p),(s),strlen(s))
#define EQ(a,h) test_memeq_hex((const char*)(a), (h))

    tt_int_op(0, OP_EQ, DECODE(sk, ED25519_SECRET_KEYS[i]));
    tt_int_op(0, OP_EQ, DECODE(blinding_param, ED25519_BLINDING_PARAMS[i]));

    tt_int_op(0, OP_EQ, ed25519_secret_key_from_seed(&esk, sk));
    EQ(esk.seckey, ED25519_EXPANDED_SECRET_KEYS[i]);

    tt_int_op(0, OP_EQ, ed25519_public_key_generate(&pk, &esk));
    EQ(pk.pubkey, ED25519_PUBLIC_KEYS[i]);

    memcpy(&curvekp.seckey.secret_key, esk.seckey, 32);
    curve25519_public_key_generate(&curvekp.pubkey, &curvekp.seckey);

    tt_int_op(0, OP_EQ,
          ed25519_keypair_from_curve25519_keypair(&keypair, &sign, &curvekp));
    tt_int_op(0, OP_EQ, ed25519_public_key_from_curve25519_public_key(
                                        &pkfromcurve, &curvekp.pubkey, sign));
    tt_mem_op(keypair.pubkey.pubkey, OP_EQ, pkfromcurve.pubkey, 32);
    EQ(curvekp.pubkey.public_key, ED25519_CURVE25519_PUBLIC_KEYS[i]);

    /* Self-signing */
    memcpy(&keypair.seckey, &esk, sizeof(esk));
    memcpy(&keypair.pubkey, &pk, sizeof(pk));

    tt_int_op(0, OP_EQ, ed25519_sign(&sig, pk.pubkey, 32, &keypair));

    EQ(sig.sig, ED25519_SELF_SIGNATURES[i]);

    /* Blinding */
    tt_int_op(0, OP_EQ,
            ed25519_keypair_blind(&blind_keypair, &keypair, blinding_param));
    tt_int_op(0, OP_EQ,
            ed25519_public_blind(&blind_pk, &pk, blinding_param));

    EQ(blind_keypair.seckey.seckey, ED25519_BLINDED_SECRET_KEYS[i]);
    EQ(blind_pk.pubkey, ED25519_BLINDED_PUBLIC_KEYS[i]);

    tt_mem_op(blind_pk.pubkey, OP_EQ, blind_keypair.pubkey.pubkey, 32);

#undef DECODE
#undef EQ
  }
 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_ed25519_fuzz_donna(void *arg)
{
  const unsigned iters = 1024;
  uint8_t msg[1024];
  unsigned i;
  (void)arg;

  tt_assert(sizeof(msg) == iters);
  crypto_rand((char*) msg, sizeof(msg));

  /* Fuzz Ed25519-donna vs ref10, alternating the implementation used to
   * generate keys/sign per iteration.
   */
  for (i = 0; i < iters; ++i) {
    const int use_donna = i & 1;
    uint8_t blinding[32];
    curve25519_keypair_t ckp;
    ed25519_keypair_t kp, kp_blind, kp_curve25519;
    ed25519_public_key_t pk, pk_blind, pk_curve25519;
    ed25519_signature_t sig, sig_blind;
    int bit = 0;

    crypto_rand((char*) blinding, sizeof(blinding));

    /* Impl. A:
     *  1. Generate a keypair.
     *  2. Blinded the keypair.
     *  3. Sign a message (unblinded).
     *  4. Sign a message (blinded).
     *  5. Generate a curve25519 keypair, and convert it to Ed25519.
     */
    ed25519_set_impl_params(use_donna);
    tt_int_op(0, OP_EQ, ed25519_keypair_generate(&kp, i&1));
    tt_int_op(0, OP_EQ, ed25519_keypair_blind(&kp_blind, &kp, blinding));
    tt_int_op(0, OP_EQ, ed25519_sign(&sig, msg, i, &kp));
    tt_int_op(0, OP_EQ, ed25519_sign(&sig_blind, msg, i, &kp_blind));

    tt_int_op(0, OP_EQ, curve25519_keypair_generate(&ckp, i&1));
    tt_int_op(0, OP_EQ, ed25519_keypair_from_curve25519_keypair(
            &kp_curve25519, &bit, &ckp));

    /* Impl. B:
     *  1. Validate the public key by rederiving it.
     *  2. Validate the blinded public key by rederiving it.
     *  3. Validate the unblinded signature (and test a invalid signature).
     *  4. Validate the blinded signature.
     *  5. Validate the public key (from Curve25519) by rederiving it.
     */
    ed25519_set_impl_params(!use_donna);
    tt_int_op(0, OP_EQ, ed25519_public_key_generate(&pk, &kp.seckey));
    tt_mem_op(pk.pubkey, OP_EQ, kp.pubkey.pubkey, 32);

    tt_int_op(0, OP_EQ, ed25519_public_blind(&pk_blind, &kp.pubkey, blinding));
    tt_mem_op(pk_blind.pubkey, OP_EQ, kp_blind.pubkey.pubkey, 32);

    tt_int_op(0, OP_EQ, ed25519_checksig(&sig, msg, i, &pk));
    sig.sig[0] ^= 15;
    tt_int_op(-1, OP_EQ, ed25519_checksig(&sig, msg, sizeof(msg), &pk));

    tt_int_op(0, OP_EQ, ed25519_checksig(&sig_blind, msg, i, &pk_blind));

    tt_int_op(0, OP_EQ, ed25519_public_key_from_curve25519_public_key(
            &pk_curve25519, &ckp.pubkey, bit));
    tt_mem_op(pk_curve25519.pubkey, OP_EQ, kp_curve25519.pubkey.pubkey, 32);
  }

 done:
  ;
}

static void
test_crypto_siphash(void *arg)
{
  /* From the reference implementation, taking
        k = 00 01 02 ... 0f
        and in = 00; 00 01; 00 01 02; ...
  */
  const uint8_t VECTORS[64][8] =
    {
      { 0x31, 0x0e, 0x0e, 0xdd, 0x47, 0xdb, 0x6f, 0x72, },
      { 0xfd, 0x67, 0xdc, 0x93, 0xc5, 0x39, 0xf8, 0x74, },
      { 0x5a, 0x4f, 0xa9, 0xd9, 0x09, 0x80, 0x6c, 0x0d, },
      { 0x2d, 0x7e, 0xfb, 0xd7, 0x96, 0x66, 0x67, 0x85, },
      { 0xb7, 0x87, 0x71, 0x27, 0xe0, 0x94, 0x27, 0xcf, },
      { 0x8d, 0xa6, 0x99, 0xcd, 0x64, 0x55, 0x76, 0x18, },
      { 0xce, 0xe3, 0xfe, 0x58, 0x6e, 0x46, 0xc9, 0xcb, },
      { 0x37, 0xd1, 0x01, 0x8b, 0xf5, 0x00, 0x02, 0xab, },
      { 0x62, 0x24, 0x93, 0x9a, 0x79, 0xf5, 0xf5, 0x93, },
      { 0xb0, 0xe4, 0xa9, 0x0b, 0xdf, 0x82, 0x00, 0x9e, },
      { 0xf3, 0xb9, 0xdd, 0x94, 0xc5, 0xbb, 0x5d, 0x7a, },
      { 0xa7, 0xad, 0x6b, 0x22, 0x46, 0x2f, 0xb3, 0xf4, },
      { 0xfb, 0xe5, 0x0e, 0x86, 0xbc, 0x8f, 0x1e, 0x75, },
      { 0x90, 0x3d, 0x84, 0xc0, 0x27, 0x56, 0xea, 0x14, },
      { 0xee, 0xf2, 0x7a, 0x8e, 0x90, 0xca, 0x23, 0xf7, },
      { 0xe5, 0x45, 0xbe, 0x49, 0x61, 0xca, 0x29, 0xa1, },
      { 0xdb, 0x9b, 0xc2, 0x57, 0x7f, 0xcc, 0x2a, 0x3f, },
      { 0x94, 0x47, 0xbe, 0x2c, 0xf5, 0xe9, 0x9a, 0x69, },
      { 0x9c, 0xd3, 0x8d, 0x96, 0xf0, 0xb3, 0xc1, 0x4b, },
      { 0xbd, 0x61, 0x79, 0xa7, 0x1d, 0xc9, 0x6d, 0xbb, },
      { 0x98, 0xee, 0xa2, 0x1a, 0xf2, 0x5c, 0xd6, 0xbe, },
      { 0xc7, 0x67, 0x3b, 0x2e, 0xb0, 0xcb, 0xf2, 0xd0, },
      { 0x88, 0x3e, 0xa3, 0xe3, 0x95, 0x67, 0x53, 0x93, },
      { 0xc8, 0xce, 0x5c, 0xcd, 0x8c, 0x03, 0x0c, 0xa8, },
      { 0x94, 0xaf, 0x49, 0xf6, 0xc6, 0x50, 0xad, 0xb8, },
      { 0xea, 0xb8, 0x85, 0x8a, 0xde, 0x92, 0xe1, 0xbc, },
      { 0xf3, 0x15, 0xbb, 0x5b, 0xb8, 0x35, 0xd8, 0x17, },
      { 0xad, 0xcf, 0x6b, 0x07, 0x63, 0x61, 0x2e, 0x2f, },
      { 0xa5, 0xc9, 0x1d, 0xa7, 0xac, 0xaa, 0x4d, 0xde, },
      { 0x71, 0x65, 0x95, 0x87, 0x66, 0x50, 0xa2, 0xa6, },
      { 0x28, 0xef, 0x49, 0x5c, 0x53, 0xa3, 0x87, 0xad, },
      { 0x42, 0xc3, 0x41, 0xd8, 0xfa, 0x92, 0xd8, 0x32, },
      { 0xce, 0x7c, 0xf2, 0x72, 0x2f, 0x51, 0x27, 0x71, },
      { 0xe3, 0x78, 0x59, 0xf9, 0x46, 0x23, 0xf3, 0xa7, },
      { 0x38, 0x12, 0x05, 0xbb, 0x1a, 0xb0, 0xe0, 0x12, },
      { 0xae, 0x97, 0xa1, 0x0f, 0xd4, 0x34, 0xe0, 0x15, },
      { 0xb4, 0xa3, 0x15, 0x08, 0xbe, 0xff, 0x4d, 0x31, },
      { 0x81, 0x39, 0x62, 0x29, 0xf0, 0x90, 0x79, 0x02, },
      { 0x4d, 0x0c, 0xf4, 0x9e, 0xe5, 0xd4, 0xdc, 0xca, },
      { 0x5c, 0x73, 0x33, 0x6a, 0x76, 0xd8, 0xbf, 0x9a, },
      { 0xd0, 0xa7, 0x04, 0x53, 0x6b, 0xa9, 0x3e, 0x0e, },
      { 0x92, 0x59, 0x58, 0xfc, 0xd6, 0x42, 0x0c, 0xad, },
      { 0xa9, 0x15, 0xc2, 0x9b, 0xc8, 0x06, 0x73, 0x18, },
      { 0x95, 0x2b, 0x79, 0xf3, 0xbc, 0x0a, 0xa6, 0xd4, },
      { 0xf2, 0x1d, 0xf2, 0xe4, 0x1d, 0x45, 0x35, 0xf9, },
      { 0x87, 0x57, 0x75, 0x19, 0x04, 0x8f, 0x53, 0xa9, },
      { 0x10, 0xa5, 0x6c, 0xf5, 0xdf, 0xcd, 0x9a, 0xdb, },
      { 0xeb, 0x75, 0x09, 0x5c, 0xcd, 0x98, 0x6c, 0xd0, },
      { 0x51, 0xa9, 0xcb, 0x9e, 0xcb, 0xa3, 0x12, 0xe6, },
      { 0x96, 0xaf, 0xad, 0xfc, 0x2c, 0xe6, 0x66, 0xc7, },
      { 0x72, 0xfe, 0x52, 0x97, 0x5a, 0x43, 0x64, 0xee, },
      { 0x5a, 0x16, 0x45, 0xb2, 0x76, 0xd5, 0x92, 0xa1, },
      { 0xb2, 0x74, 0xcb, 0x8e, 0xbf, 0x87, 0x87, 0x0a, },
      { 0x6f, 0x9b, 0xb4, 0x20, 0x3d, 0xe7, 0xb3, 0x81, },
      { 0xea, 0xec, 0xb2, 0xa3, 0x0b, 0x22, 0xa8, 0x7f, },
      { 0x99, 0x24, 0xa4, 0x3c, 0xc1, 0x31, 0x57, 0x24, },
      { 0xbd, 0x83, 0x8d, 0x3a, 0xaf, 0xbf, 0x8d, 0xb7, },
      { 0x0b, 0x1a, 0x2a, 0x32, 0x65, 0xd5, 0x1a, 0xea, },
      { 0x13, 0x50, 0x79, 0xa3, 0x23, 0x1c, 0xe6, 0x60, },
      { 0x93, 0x2b, 0x28, 0x46, 0xe4, 0xd7, 0x06, 0x66, },
      { 0xe1, 0x91, 0x5f, 0x5c, 0xb1, 0xec, 0xa4, 0x6c, },
      { 0xf3, 0x25, 0x96, 0x5c, 0xa1, 0x6d, 0x62, 0x9f, },
      { 0x57, 0x5f, 0xf2, 0x8e, 0x60, 0x38, 0x1b, 0xe5, },
      { 0x72, 0x45, 0x06, 0xeb, 0x4c, 0x32, 0x8a, 0x95, }
    };

  const struct sipkey K = { U64_LITERAL(0x0706050403020100),
                            U64_LITERAL(0x0f0e0d0c0b0a0908) };
  uint8_t input[64];
  int i, j;

  (void)arg;

  for (i = 0; i < 64; ++i)
    input[i] = i;

  for (i = 0; i < 64; ++i) {
    uint64_t r = siphash24(input, i, &K);
    for (j = 0; j < 8; ++j) {
      tt_int_op( (r >> (j*8)) & 0xff, OP_EQ, VECTORS[i][j]);
    }
  }

 done:
  ;
}

#define CRYPTO_LEGACY(name)                                            \
  { #name, test_crypto_ ## name , 0, NULL, NULL }

struct testcase_t crypto_tests[] = {
  CRYPTO_LEGACY(formats),
  CRYPTO_LEGACY(rng),
  { "rng_range", test_crypto_rng_range, 0, NULL, NULL },
  { "aes_AES", test_crypto_aes, TT_FORK, &passthrough_setup, (void*)"aes" },
  { "aes_EVP", test_crypto_aes, TT_FORK, &passthrough_setup, (void*)"evp" },
  CRYPTO_LEGACY(sha),
  CRYPTO_LEGACY(pk),
  { "pk_fingerprints", test_crypto_pk_fingerprints, TT_FORK, NULL, NULL },
  { "pk_base64", test_crypto_pk_base64, TT_FORK, NULL, NULL },
  CRYPTO_LEGACY(digests),
  CRYPTO_LEGACY(dh),
  { "aes_iv_AES", test_crypto_aes_iv, TT_FORK, &passthrough_setup,
    (void*)"aes" },
  { "aes_iv_EVP", test_crypto_aes_iv, TT_FORK, &passthrough_setup,
    (void*)"evp" },
  CRYPTO_LEGACY(base32_decode),
  { "kdf_TAP", test_crypto_kdf_TAP, 0, NULL, NULL },
  { "hkdf_sha256", test_crypto_hkdf_sha256, 0, NULL, NULL },
  { "curve25519_impl", test_crypto_curve25519_impl, 0, NULL, NULL },
  { "curve25519_impl_hibit", test_crypto_curve25519_impl, 0, NULL, (void*)"y"},
  { "curve25519_basepoint",
    test_crypto_curve25519_basepoint, TT_FORK, NULL, NULL },
  { "curve25519_wrappers", test_crypto_curve25519_wrappers, 0, NULL, NULL },
  { "curve25519_encode", test_crypto_curve25519_encode, 0, NULL, NULL },
  { "curve25519_persist", test_crypto_curve25519_persist, 0, NULL, NULL },
  { "ed25519_simple", test_crypto_ed25519_simple, 0, NULL, NULL },
  { "ed25519_test_vectors", test_crypto_ed25519_test_vectors, 0, NULL, NULL },
  { "ed25519_encode", test_crypto_ed25519_encode, 0, NULL, NULL },
  { "ed25519_convert", test_crypto_ed25519_convert, 0, NULL, NULL },
  { "ed25519_blinding", test_crypto_ed25519_blinding, 0, NULL, NULL },
  { "ed25519_testvectors", test_crypto_ed25519_testvectors, 0, NULL, NULL },
  { "ed25519_fuzz_donna", test_crypto_ed25519_fuzz_donna, TT_FORK, NULL,
    NULL },
  { "siphash", test_crypto_siphash, 0, NULL, NULL },
  END_OF_TESTCASES
};

