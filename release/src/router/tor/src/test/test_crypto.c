/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define CRYPTO_CURVE25519_PRIVATE
#define CRYPTO_PRIVATE
#include "or.h"
#include "test.h"
#include "aes.h"
#include "util.h"
#include "siphash.h"
#include "crypto_curve25519.h"
#include "crypto_ed25519.h"
#include "ed25519_vectors.inc"

#include <openssl/evp.h>
#include <openssl/rand.h>

/** Run unit tests for Diffie-Hellman functionality. */
static void
test_crypto_dh(void *arg)
{
  crypto_dh_t *dh1 = crypto_dh_new(DH_TYPE_CIRCUIT);
  crypto_dh_t *dh1_dup = NULL;
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

  tt_int_op(-1, OP_EQ, crypto_dh_get_public(dh1, p1, 6)); /* too short */

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

  /* test dh_dup; make sure it works the same. */
  dh1_dup = crypto_dh_dup(dh1);
  s1len = crypto_dh_compute_secret(LOG_WARN, dh1_dup, p2, DH_BYTES, s1, 50);
  tt_mem_op(s1,OP_EQ, s2, s1len);

  {
    /* Now fabricate some bad values and make sure they get caught. */

    /* 1 and 0 should both fail. */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, "\x01", 1, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, "\x00", 1, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    memset(p1, 0, DH_BYTES); /* 0 with padding. */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    p1[DH_BYTES-1] = 1; /* 1 with padding*/
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    /* 2 is okay, though weird. */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, "\x02", 1, s1, 50);
    tt_int_op(50, OP_EQ, s1len);

    const char P[] =
      "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
      "8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
      "302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
      "A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
      "49286651ECE65381FFFFFFFFFFFFFFFF";

    /* p-1, p, and so on are not okay. */
    base16_decode(p1, sizeof(p1), P, strlen(P));

    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    p1[DH_BYTES-1] = 0xFE; /* p-1 */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    p1[DH_BYTES-1] = 0xFD; /* p-2 works fine */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(50, OP_EQ, s1len);

    const char P_plus_one[] =
      "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
      "8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
      "302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
      "A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
      "49286651ECE653820000000000000000";

    base16_decode(p1, sizeof(p1), P_plus_one, strlen(P_plus_one));

    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    p1[DH_BYTES-1] = 0x01; /* p+2 */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    p1[DH_BYTES-1] = 0xff; /* p+256 */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);

    memset(p1, 0xff, DH_BYTES), /* 2^1024-1 */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh1, p1, DH_BYTES, s1, 50);
    tt_int_op(-1, OP_EQ, s1len);
  }

  {
    /* provoke an error in the openssl DH_compute_key function; make sure we
     * survive. */
    tt_assert(! crypto_dh_get_public(dh1, p1, DH_BYTES));

    crypto_dh_free(dh2);
    dh2= crypto_dh_new(DH_TYPE_CIRCUIT); /* no private key set */
    s1len = crypto_dh_compute_secret(LOG_WARN, dh2,
                                     p1, DH_BYTES,
                                     s1, 50);
    tt_int_op(s1len, OP_EQ, -1);
  }

 done:
  crypto_dh_free(dh1);
  crypto_dh_free(dh2);
  crypto_dh_free(dh1_dup);
}

static void
test_crypto_openssl_version(void *arg)
{
  (void)arg;
  const char *version = crypto_openssl_get_version_str();
  const char *h_version = crypto_openssl_get_header_version_str();
  tt_assert(version);
  tt_assert(h_version);
  tt_assert(!strcmpstart(version, h_version)); /* "-fips" suffix, etc */
  tt_assert(!strstr(version, "OpenSSL"));
  int a=-1,b=-1,c=-1;
  if (!strcmpstart(version, "LibreSSL") || !strcmpstart(version, "BoringSSL"))
    return;
  int r = tor_sscanf(version, "%d.%d.%d", &a,&b,&c);
  tt_int_op(r, OP_EQ, 3);
  tt_int_op(a, OP_GE, 0);
  tt_int_op(b, OP_GE, 0);
  tt_int_op(c, OP_GE, 0);

 done:
  ;
}

/** Run unit tests for our random number generation function and its wrappers.
 */
static void
test_crypto_rng(void *arg)
{
  int i, j, allok;
  char data1[100], data2[100];
  double d;
  char *h=NULL;

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

  /* Make sure crypto_random_hostname clips its inputs properly. */
  h = crypto_random_hostname(20000, 9000, "www.", ".onion");
  tt_assert(! strcmpstart(h,"www."));
  tt_assert(! strcmpend(h,".onion"));
  tt_int_op(63+4+6, OP_EQ, strlen(h));

  tt_assert(allok);
 done:
  tor_free(h);
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

  got_smallest = got_largest = 0;
  const uint64_t ten_billion = 10 * ((uint64_t)1000000000000);
  for (i = 0; i < 1000; ++i) {
    uint64_t x = crypto_rand_uint64_range(ten_billion, ten_billion+10);
    tt_u64_op(x, OP_GE, ten_billion);
    tt_u64_op(x, OP_LT, ten_billion+10);
    if (x == ten_billion)
      got_smallest = 1;
    if (x == ten_billion+9)
      got_largest = 1;
  }

  tt_assert(got_smallest);
  tt_assert(got_largest);

  const time_t now = time(NULL);
  for (i = 0; i < 2000; ++i) {
    time_t x = crypto_rand_time_range(now, now+60);
    tt_i64_op(x, OP_GE, now);
    tt_i64_op(x, OP_LT, now+60);
    if (x == now)
      got_smallest = 1;
    if (x == now+59)
      got_largest = 1;
  }

  tt_assert(got_smallest);
  tt_assert(got_largest);
 done:
  ;
}

static void
test_crypto_rng_strongest(void *arg)
{
  const char *how = arg;
  int broken = 0;

  if (how == NULL) {
    ;
  } else if (!strcmp(how, "nosyscall")) {
    break_strongest_rng_syscall = 1;
  } else if (!strcmp(how, "nofallback")) {
    break_strongest_rng_fallback = 1;
  } else if (!strcmp(how, "broken")) {
    broken = break_strongest_rng_syscall = break_strongest_rng_fallback = 1;
  }

#define N 128
  uint8_t combine_and[N];
  uint8_t combine_or[N];
  int i, j;

  memset(combine_and, 0xff, N);
  memset(combine_or, 0, N);

  for (i = 0; i < 100; ++i) { /* 2^-100 chances just don't happen. */
    uint8_t output[N];
    memset(output, 0, N);
    if (how == NULL) {
      /* this one can't fail. */
      crypto_strongest_rand(output, sizeof(output));
    } else {
      int r = crypto_strongest_rand_raw(output, sizeof(output));
      if (r == -1) {
        if (broken) {
          goto done; /* we're fine. */
        }
        /* This function is allowed to break, but only if it always breaks. */
        tt_int_op(i, OP_EQ, 0);
        tt_skip();
      } else {
        tt_assert(! broken);
      }
    }
    for (j = 0; j < N; ++j) {
      combine_and[j] &= output[j];
      combine_or[j] |= output[j];
    }
  }

  for (j = 0; j < N; ++j) {
    tt_int_op(combine_and[j], OP_EQ, 0);
    tt_int_op(combine_or[j], OP_EQ, 0xff);
  }
 done:
  ;
#undef N
}

/* Test for rectifying openssl RAND engine. */
static void
test_crypto_rng_engine(void *arg)
{
  (void)arg;
  RAND_METHOD dummy_method;
  memset(&dummy_method, 0, sizeof(dummy_method));

  /* We should be a no-op if we're already on RAND_OpenSSL */
  tt_int_op(0, ==, crypto_force_rand_ssleay());
  tt_assert(RAND_get_rand_method() == RAND_OpenSSL());

  /* We should correct the method if it's a dummy. */
  RAND_set_rand_method(&dummy_method);
#ifdef LIBRESSL_VERSION_NUMBER
  /* On libressl, you can't override the RNG. */
  tt_assert(RAND_get_rand_method() == RAND_OpenSSL());
  tt_int_op(0, ==, crypto_force_rand_ssleay());
#else
  tt_assert(RAND_get_rand_method() == &dummy_method);
  tt_int_op(1, ==, crypto_force_rand_ssleay());
#endif
  tt_assert(RAND_get_rand_method() == RAND_OpenSSL());

  /* Make sure we aren't calling dummy_method */
  crypto_rand((void *) &dummy_method, sizeof(dummy_method));
  crypto_rand((void *) &dummy_method, sizeof(dummy_method));

 done:
  ;
}

/** Run unit tests for our AES128 functionality */
static void
test_crypto_aes128(void *arg)
{
  char *data1 = NULL, *data2 = NULL, *data3 = NULL;
  crypto_cipher_t *env1 = NULL, *env2 = NULL;
  int i, j;
  char *mem_op_hex_tmp=NULL;
  char key[CIPHER_KEY_LEN];
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
  crypto_rand(key, sizeof(key));
  env1 = crypto_cipher_new(key);
  tt_ptr_op(env1, OP_NE, NULL);
  env2 = crypto_cipher_new(key);
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
  env2 = crypto_cipher_new(key);
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

static void
test_crypto_aes_ctr_testvec(void *arg)
{
  const char *bitstr = arg;
  char *mem_op_hex_tmp=NULL;
  crypto_cipher_t *c=NULL;

  /* from NIST SP800-38a, section F.5 */
  const char ctr16[] = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
  const char plaintext16[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
  const char *ciphertext16;
  const char *key16;
  int bits;

  if (!strcmp(bitstr, "128")) {
    ciphertext16 = /* section F.5.1 */
      "874d6191b620e3261bef6864990db6ce"
      "9806f66b7970fdff8617187bb9fffdff"
      "5ae4df3edbd5d35e5b4f09020db03eab"
      "1e031dda2fbe03d1792170a0f3009cee";
    key16 = "2b7e151628aed2a6abf7158809cf4f3c";
    bits = 128;
  } else if (!strcmp(bitstr, "192")) {
    ciphertext16 = /* section F.5.3 */
      "1abc932417521ca24f2b0459fe7e6e0b"
      "090339ec0aa6faefd5ccc2c6f4ce8e94"
      "1e36b26bd1ebc670d1bd1d665620abf7"
      "4f78a7f6d29809585a97daec58c6b050";
    key16 = "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b";
    bits = 192;
  } else if (!strcmp(bitstr, "256")) {
    ciphertext16 = /* section F.5.5 */
      "601ec313775789a5b7a7f504bbf3d228"
      "f443e3ca4d62b59aca84e990cacaf5c5"
      "2b0930daa23de94ce87017ba2d84988d"
      "dfc9c58db67aada613c2dd08457941a6";
    key16 =
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4";
    bits = 256;
  } else {
    tt_abort_msg("AES doesn't support this number of bits.");
  }

  char key[32];
  char iv[16];
  char plaintext[16*4];
  memset(key, 0xf9, sizeof(key)); /* poison extra bytes */
  base16_decode(key, sizeof(key), key16, strlen(key16));
  base16_decode(iv, sizeof(iv), ctr16, strlen(ctr16));
  base16_decode(plaintext, sizeof(plaintext),
                plaintext16, strlen(plaintext16));

  c = crypto_cipher_new_with_iv_and_bits((uint8_t*)key, (uint8_t*)iv, bits);
  crypto_cipher_crypt_inplace(c, plaintext, sizeof(plaintext));
  test_memeq_hex(plaintext, ciphertext16);

 done:
  tor_free(mem_op_hex_tmp);
  crypto_cipher_free(c);
}

/** Run unit tests for our SHA-1 functionality */
static void
test_crypto_sha(void *arg)
{
  crypto_digest_t *d1 = NULL, *d2 = NULL;
  int i;
#define RFC_4231_MAX_KEY_SIZE 131
  char key[RFC_4231_MAX_KEY_SIZE];
  char digest[DIGEST256_LEN];
  char data[DIGEST512_LEN];
  char d_out1[DIGEST512_LEN], d_out2[DIGEST512_LEN];
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

  /* Test SHA-512 with a test vector from the specification. */
  i = crypto_digest512(data, "abc", 3, DIGEST_SHA512);
  test_memeq_hex(data, "ddaf35a193617abacc417349ae20413112e6fa4e89a97"
                       "ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3"
                       "feebbd454d4423643ce80e2a9ac94fa54ca49f");
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
  crypto_digest_get_digest(d2, d_out1, DIGEST_LEN);
  crypto_digest(d_out2, "abcdefghijkl", 12);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, DIGEST_LEN);
  crypto_digest(d_out2, "abcdefmno", 9);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST_LEN);
  crypto_digest_get_digest(d1, d_out1, DIGEST_LEN);
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
  crypto_digest_get_digest(d2, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdefghijkl", 12, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdefmno", 9, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_get_digest(d1, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdef", 6, DIGEST_SHA256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_free(d1);
  crypto_digest_free(d2);

  /* Incremental digest code with sha512 */
  d1 = crypto_digest512_new(DIGEST_SHA512);
  tt_assert(d1);
  crypto_digest_add_bytes(d1, "abcdef", 6);
  d2 = crypto_digest_dup(d1);
  tt_assert(d2);
  crypto_digest_add_bytes(d2, "ghijkl", 6);
  crypto_digest_get_digest(d2, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdefghijkl", 12, DIGEST_SHA512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdefmno", 9, DIGEST_SHA512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);
  crypto_digest_get_digest(d1, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdef", 6, DIGEST_SHA512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);

 done:
  if (d1)
    crypto_digest_free(d1);
  if (d2)
    crypto_digest_free(d2);
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_sha3(void *arg)
{
  crypto_digest_t *d1 = NULL, *d2 = NULL;
  int i;
  char data[DIGEST512_LEN];
  char d_out1[DIGEST512_LEN], d_out2[DIGEST512_LEN];
  char *mem_op_hex_tmp=NULL;
  char *large = NULL;

  (void)arg;

  /* Test SHA3-[256,512] with a test vectors from the Keccak Code Package.
   *
   * NB: The code package's test vectors have length expressed in bits.
   */

  /* Len = 8, Msg = CC */
  const uint8_t keccak_kat_msg8[] = { 0xcc };
  i = crypto_digest256(data, (const char*)keccak_kat_msg8, 1, DIGEST_SHA3_256);
  test_memeq_hex(data, "677035391CD3701293D385F037BA3279"
                       "6252BB7CE180B00B582DD9B20AAAD7F0");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, (const char*)keccak_kat_msg8, 1, DIGEST_SHA3_512);
  test_memeq_hex(data, "3939FCC8B57B63612542DA31A834E5DC"
                       "C36E2EE0F652AC72E02624FA2E5ADEEC"
                       "C7DD6BB3580224B4D6138706FC6E8059"
                       "7B528051230B00621CC2B22999EAA205");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 24, Msg = 1F877C */
  const uint8_t keccak_kat_msg24[] = { 0x1f, 0x87, 0x7c };
  i = crypto_digest256(data, (const char*)keccak_kat_msg24, 3,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "BC22345E4BD3F792A341CF18AC0789F1"
                       "C9C966712A501B19D1B6632CCD408EC5");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, (const char*)keccak_kat_msg24, 3,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "CB20DCF54955F8091111688BECCEF48C"
                       "1A2F0D0608C3A575163751F002DB30F4"
                       "0F2F671834B22D208591CFAF1F5ECFE4"
                       "3C49863A53B3225BDFD7C6591BA7658B");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1080, Msg = B771D5CEF... ...C35AC81B5 (SHA3-256 rate - 1) */
  const uint8_t keccak_kat_msg1080[] = {
    0xB7, 0x71, 0xD5, 0xCE, 0xF5, 0xD1, 0xA4, 0x1A, 0x93, 0xD1,
    0x56, 0x43, 0xD7, 0x18, 0x1D, 0x2A, 0x2E, 0xF0, 0xA8, 0xE8,
    0x4D, 0x91, 0x81, 0x2F, 0x20, 0xED, 0x21, 0xF1, 0x47, 0xBE,
    0xF7, 0x32, 0xBF, 0x3A, 0x60, 0xEF, 0x40, 0x67, 0xC3, 0x73,
    0x4B, 0x85, 0xBC, 0x8C, 0xD4, 0x71, 0x78, 0x0F, 0x10, 0xDC,
    0x9E, 0x82, 0x91, 0xB5, 0x83, 0x39, 0xA6, 0x77, 0xB9, 0x60,
    0x21, 0x8F, 0x71, 0xE7, 0x93, 0xF2, 0x79, 0x7A, 0xEA, 0x34,
    0x94, 0x06, 0x51, 0x28, 0x29, 0x06, 0x5D, 0x37, 0xBB, 0x55,
    0xEA, 0x79, 0x6F, 0xA4, 0xF5, 0x6F, 0xD8, 0x89, 0x6B, 0x49,
    0xB2, 0xCD, 0x19, 0xB4, 0x32, 0x15, 0xAD, 0x96, 0x7C, 0x71,
    0x2B, 0x24, 0xE5, 0x03, 0x2D, 0x06, 0x52, 0x32, 0xE0, 0x2C,
    0x12, 0x74, 0x09, 0xD2, 0xED, 0x41, 0x46, 0xB9, 0xD7, 0x5D,
    0x76, 0x3D, 0x52, 0xDB, 0x98, 0xD9, 0x49, 0xD3, 0xB0, 0xFE,
    0xD6, 0xA8, 0x05, 0x2F, 0xBB,
  };
  i = crypto_digest256(data, (const char*)keccak_kat_msg1080, 135,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "A19EEE92BB2097B64E823D597798AA18"
                       "BE9B7C736B8059ABFD6779AC35AC81B5");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, (const char*)keccak_kat_msg1080, 135,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "7575A1FB4FC9A8F9C0466BD5FCA496D1"
                       "CB78696773A212A5F62D02D14E3259D1"
                       "92A87EBA4407DD83893527331407B6DA"
                       "DAAD920DBC46489B677493CE5F20B595");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1088, Msg = B32D95B0... ...8E380C04 (SHA3-256 rate) */
  const uint8_t keccak_kat_msg1088[] = {
    0xB3, 0x2D, 0x95, 0xB0, 0xB9, 0xAA, 0xD2, 0xA8, 0x81, 0x6D,
    0xE6, 0xD0, 0x6D, 0x1F, 0x86, 0x00, 0x85, 0x05, 0xBD, 0x8C,
    0x14, 0x12, 0x4F, 0x6E, 0x9A, 0x16, 0x3B, 0x5A, 0x2A, 0xDE,
    0x55, 0xF8, 0x35, 0xD0, 0xEC, 0x38, 0x80, 0xEF, 0x50, 0x70,
    0x0D, 0x3B, 0x25, 0xE4, 0x2C, 0xC0, 0xAF, 0x05, 0x0C, 0xCD,
    0x1B, 0xE5, 0xE5, 0x55, 0xB2, 0x30, 0x87, 0xE0, 0x4D, 0x7B,
    0xF9, 0x81, 0x36, 0x22, 0x78, 0x0C, 0x73, 0x13, 0xA1, 0x95,
    0x4F, 0x87, 0x40, 0xB6, 0xEE, 0x2D, 0x3F, 0x71, 0xF7, 0x68,
    0xDD, 0x41, 0x7F, 0x52, 0x04, 0x82, 0xBD, 0x3A, 0x08, 0xD4,
    0xF2, 0x22, 0xB4, 0xEE, 0x9D, 0xBD, 0x01, 0x54, 0x47, 0xB3,
    0x35, 0x07, 0xDD, 0x50, 0xF3, 0xAB, 0x42, 0x47, 0xC5, 0xDE,
    0x9A, 0x8A, 0xBD, 0x62, 0xA8, 0xDE, 0xCE, 0xA0, 0x1E, 0x3B,
    0x87, 0xC8, 0xB9, 0x27, 0xF5, 0xB0, 0x8B, 0xEB, 0x37, 0x67,
    0x4C, 0x6F, 0x8E, 0x38, 0x0C, 0x04,
  };
  i = crypto_digest256(data, (const char*)keccak_kat_msg1088, 136,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "DF673F4105379FF6B755EEAB20CEB0DC"
                       "77B5286364FE16C59CC8A907AFF07732");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, (const char*)keccak_kat_msg1088, 136,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "2E293765022D48996CE8EFF0BE54E87E"
                       "FB94A14C72DE5ACD10D0EB5ECE029CAD"
                       "FA3BA17A40B2FFA2163991B17786E51C"
                       "ABA79E5E0FFD34CF085E2A098BE8BACB");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1096, Msg = 04410E310... ...601016A0D (SHA3-256 rate + 1) */
  const uint8_t keccak_kat_msg1096[] = {
    0x04, 0x41, 0x0E, 0x31, 0x08, 0x2A, 0x47, 0x58, 0x4B, 0x40,
    0x6F, 0x05, 0x13, 0x98, 0xA6, 0xAB, 0xE7, 0x4E, 0x4D, 0xA5,
    0x9B, 0xB6, 0xF8, 0x5E, 0x6B, 0x49, 0xE8, 0xA1, 0xF7, 0xF2,
    0xCA, 0x00, 0xDF, 0xBA, 0x54, 0x62, 0xC2, 0xCD, 0x2B, 0xFD,
    0xE8, 0xB6, 0x4F, 0xB2, 0x1D, 0x70, 0xC0, 0x83, 0xF1, 0x13,
    0x18, 0xB5, 0x6A, 0x52, 0xD0, 0x3B, 0x81, 0xCA, 0xC5, 0xEE,
    0xC2, 0x9E, 0xB3, 0x1B, 0xD0, 0x07, 0x8B, 0x61, 0x56, 0x78,
    0x6D, 0xA3, 0xD6, 0xD8, 0xC3, 0x30, 0x98, 0xC5, 0xC4, 0x7B,
    0xB6, 0x7A, 0xC6, 0x4D, 0xB1, 0x41, 0x65, 0xAF, 0x65, 0xB4,
    0x45, 0x44, 0xD8, 0x06, 0xDD, 0xE5, 0xF4, 0x87, 0xD5, 0x37,
    0x3C, 0x7F, 0x97, 0x92, 0xC2, 0x99, 0xE9, 0x68, 0x6B, 0x7E,
    0x58, 0x21, 0xE7, 0xC8, 0xE2, 0x45, 0x83, 0x15, 0xB9, 0x96,
    0xB5, 0x67, 0x7D, 0x92, 0x6D, 0xAC, 0x57, 0xB3, 0xF2, 0x2D,
    0xA8, 0x73, 0xC6, 0x01, 0x01, 0x6A, 0x0D,
  };
  i = crypto_digest256(data, (const char*)keccak_kat_msg1096, 137,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "D52432CF3B6B4B949AA848E058DCD62D"
                       "735E0177279222E7AC0AF8504762FAA0");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, (const char*)keccak_kat_msg1096, 137,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "BE8E14B6757FFE53C9B75F6DDE9A7B6C"
                       "40474041DE83D4A60645A826D7AF1ABE"
                       "1EEFCB7B74B62CA6A514E5F2697D585B"
                       "FECECE12931BBE1D4ED7EBF7B0BE660E");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1144, Msg = EA40E83C...  ...66DFAFEC (SHA3-512 rate *2 - 1) */
  const uint8_t keccak_kat_msg1144[] = {
    0xEA, 0x40, 0xE8, 0x3C, 0xB1, 0x8B, 0x3A, 0x24, 0x2C, 0x1E,
    0xCC, 0x6C, 0xCD, 0x0B, 0x78, 0x53, 0xA4, 0x39, 0xDA, 0xB2,
    0xC5, 0x69, 0xCF, 0xC6, 0xDC, 0x38, 0xA1, 0x9F, 0x5C, 0x90,
    0xAC, 0xBF, 0x76, 0xAE, 0xF9, 0xEA, 0x37, 0x42, 0xFF, 0x3B,
    0x54, 0xEF, 0x7D, 0x36, 0xEB, 0x7C, 0xE4, 0xFF, 0x1C, 0x9A,
    0xB3, 0xBC, 0x11, 0x9C, 0xFF, 0x6B, 0xE9, 0x3C, 0x03, 0xE2,
    0x08, 0x78, 0x33, 0x35, 0xC0, 0xAB, 0x81, 0x37, 0xBE, 0x5B,
    0x10, 0xCD, 0xC6, 0x6F, 0xF3, 0xF8, 0x9A, 0x1B, 0xDD, 0xC6,
    0xA1, 0xEE, 0xD7, 0x4F, 0x50, 0x4C, 0xBE, 0x72, 0x90, 0x69,
    0x0B, 0xB2, 0x95, 0xA8, 0x72, 0xB9, 0xE3, 0xFE, 0x2C, 0xEE,
    0x9E, 0x6C, 0x67, 0xC4, 0x1D, 0xB8, 0xEF, 0xD7, 0xD8, 0x63,
    0xCF, 0x10, 0xF8, 0x40, 0xFE, 0x61, 0x8E, 0x79, 0x36, 0xDA,
    0x3D, 0xCA, 0x5C, 0xA6, 0xDF, 0x93, 0x3F, 0x24, 0xF6, 0x95,
    0x4B, 0xA0, 0x80, 0x1A, 0x12, 0x94, 0xCD, 0x8D, 0x7E, 0x66,
    0xDF, 0xAF, 0xEC,
  };
  i = crypto_digest512(data, (const char*)keccak_kat_msg1144, 143,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "3A8E938C45F3F177991296B24565D9A6"
                       "605516615D96A062C8BE53A0D6C5A648"
                       "7BE35D2A8F3CF6620D0C2DBA2C560D68"
                       "295F284BE7F82F3B92919033C9CE5D80");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest256(data, (const char*)keccak_kat_msg1144, 143,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "E58A947E98D6DD7E932D2FE02D9992E6"
                       "118C0C2C606BDCDA06E7943D2C95E0E5");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1152, Msg = 157D5B7E... ...79EE00C63 (SHA3-512 rate * 2) */
  const uint8_t keccak_kat_msg1152[] = {
    0x15, 0x7D, 0x5B, 0x7E, 0x45, 0x07, 0xF6, 0x6D, 0x9A, 0x26,
    0x74, 0x76, 0xD3, 0x38, 0x31, 0xE7, 0xBB, 0x76, 0x8D, 0x4D,
    0x04, 0xCC, 0x34, 0x38, 0xDA, 0x12, 0xF9, 0x01, 0x02, 0x63,
    0xEA, 0x5F, 0xCA, 0xFB, 0xDE, 0x25, 0x79, 0xDB, 0x2F, 0x6B,
    0x58, 0xF9, 0x11, 0xD5, 0x93, 0xD5, 0xF7, 0x9F, 0xB0, 0x5F,
    0xE3, 0x59, 0x6E, 0x3F, 0xA8, 0x0F, 0xF2, 0xF7, 0x61, 0xD1,
    0xB0, 0xE5, 0x70, 0x80, 0x05, 0x5C, 0x11, 0x8C, 0x53, 0xE5,
    0x3C, 0xDB, 0x63, 0x05, 0x52, 0x61, 0xD7, 0xC9, 0xB2, 0xB3,
    0x9B, 0xD9, 0x0A, 0xCC, 0x32, 0x52, 0x0C, 0xBB, 0xDB, 0xDA,
    0x2C, 0x4F, 0xD8, 0x85, 0x6D, 0xBC, 0xEE, 0x17, 0x31, 0x32,
    0xA2, 0x67, 0x91, 0x98, 0xDA, 0xF8, 0x30, 0x07, 0xA9, 0xB5,
    0xC5, 0x15, 0x11, 0xAE, 0x49, 0x76, 0x6C, 0x79, 0x2A, 0x29,
    0x52, 0x03, 0x88, 0x44, 0x4E, 0xBE, 0xFE, 0x28, 0x25, 0x6F,
    0xB3, 0x3D, 0x42, 0x60, 0x43, 0x9C, 0xBA, 0x73, 0xA9, 0x47,
    0x9E, 0xE0, 0x0C, 0x63,
  };
  i = crypto_digest512(data, (const char*)keccak_kat_msg1152, 144,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "FE45289874879720CE2A844AE34BB735"
                       "22775DCB6019DCD22B8885994672A088"
                       "9C69E8115C641DC8B83E39F7311815A1"
                       "64DC46E0BA2FCA344D86D4BC2EF2532C");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest256(data, (const char*)keccak_kat_msg1152, 144,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "A936FB9AF87FB67857B3EAD5C76226AD"
                       "84DA47678F3C2FFE5A39FDB5F7E63FFB");
  tt_int_op(i, OP_EQ, 0);

  /* Len = 1160, Msg = 836B34B5... ...11044C53 (SHA3-512 rate * 2 + 1) */
  const uint8_t keccak_kat_msg1160[] = {
    0x83, 0x6B, 0x34, 0xB5, 0x15, 0x47, 0x6F, 0x61, 0x3F, 0xE4,
    0x47, 0xA4, 0xE0, 0xC3, 0xF3, 0xB8, 0xF2, 0x09, 0x10, 0xAC,
    0x89, 0xA3, 0x97, 0x70, 0x55, 0xC9, 0x60, 0xD2, 0xD5, 0xD2,
    0xB7, 0x2B, 0xD8, 0xAC, 0xC7, 0x15, 0xA9, 0x03, 0x53, 0x21,
    0xB8, 0x67, 0x03, 0xA4, 0x11, 0xDD, 0xE0, 0x46, 0x6D, 0x58,
    0xA5, 0x97, 0x69, 0x67, 0x2A, 0xA6, 0x0A, 0xD5, 0x87, 0xB8,
    0x48, 0x1D, 0xE4, 0xBB, 0xA5, 0x52, 0xA1, 0x64, 0x57, 0x79,
    0x78, 0x95, 0x01, 0xEC, 0x53, 0xD5, 0x40, 0xB9, 0x04, 0x82,
    0x1F, 0x32, 0xB0, 0xBD, 0x18, 0x55, 0xB0, 0x4E, 0x48, 0x48,
    0xF9, 0xF8, 0xCF, 0xE9, 0xEB, 0xD8, 0x91, 0x1B, 0xE9, 0x57,
    0x81, 0xA7, 0x59, 0xD7, 0xAD, 0x97, 0x24, 0xA7, 0x10, 0x2D,
    0xBE, 0x57, 0x67, 0x76, 0xB7, 0xC6, 0x32, 0xBC, 0x39, 0xB9,
    0xB5, 0xE1, 0x90, 0x57, 0xE2, 0x26, 0x55, 0x2A, 0x59, 0x94,
    0xC1, 0xDB, 0xB3, 0xB5, 0xC7, 0x87, 0x1A, 0x11, 0xF5, 0x53,
    0x70, 0x11, 0x04, 0x4C, 0x53,
  };
  i = crypto_digest512(data, (const char*)keccak_kat_msg1160, 145,
                       DIGEST_SHA3_512);
  test_memeq_hex(data, "AFF61C6E11B98E55AC213B1A0BC7DE04"
                       "05221AC5EFB1229842E4614F4A029C9B"
                       "D14A0ED7FD99AF3681429F3F309FDB53"
                       "166AA9A3CD9F1F1223D04B4A9015E94A");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest256(data, (const char*)keccak_kat_msg1160, 145,
                       DIGEST_SHA3_256);
  test_memeq_hex(data, "3A654B88F88086C2751EDAE6D3924814"
                       "3CF6235C6B0B7969342C45A35194B67E");
  tt_int_op(i, OP_EQ, 0);

  /* SHA3-[256,512] Empty case (wikipedia) */
  i = crypto_digest256(data, "", 0, DIGEST_SHA3_256);
  test_memeq_hex(data, "a7ffc6f8bf1ed76651c14756a061d662"
                       "f580ff4de43b49fa82d80a4b80f8434a");
  tt_int_op(i, OP_EQ, 0);
  i = crypto_digest512(data, "", 0, DIGEST_SHA3_512);
  test_memeq_hex(data, "a69f73cca23a9ac5c8b567dc185a756e"
                       "97c982164fe25859e0d1dcc1475c80a6"
                       "15b2123af1f5f94c11e3e9402c3ac558"
                       "f500199d95b6d3e301758586281dcd26");
  tt_int_op(i, OP_EQ, 0);

  /* Incremental digest code with SHA3-256 */
  d1 = crypto_digest256_new(DIGEST_SHA3_256);
  tt_assert(d1);
  crypto_digest_add_bytes(d1, "abcdef", 6);
  d2 = crypto_digest_dup(d1);
  tt_assert(d2);
  crypto_digest_add_bytes(d2, "ghijkl", 6);
  crypto_digest_get_digest(d2, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdefghijkl", 12, DIGEST_SHA3_256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdefmno", 9, DIGEST_SHA3_256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_get_digest(d1, d_out1, DIGEST256_LEN);
  crypto_digest256(d_out2, "abcdef", 6, DIGEST_SHA3_256);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST256_LEN);
  crypto_digest_free(d1);
  crypto_digest_free(d2);

  /* Incremental digest code with SHA3-512 */
  d1 = crypto_digest512_new(DIGEST_SHA3_512);
  tt_assert(d1);
  crypto_digest_add_bytes(d1, "abcdef", 6);
  d2 = crypto_digest_dup(d1);
  tt_assert(d2);
  crypto_digest_add_bytes(d2, "ghijkl", 6);
  crypto_digest_get_digest(d2, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdefghijkl", 12, DIGEST_SHA3_512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);
  crypto_digest_assign(d2, d1);
  crypto_digest_add_bytes(d2, "mno", 3);
  crypto_digest_get_digest(d2, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdefmno", 9, DIGEST_SHA3_512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);
  crypto_digest_get_digest(d1, d_out1, DIGEST512_LEN);
  crypto_digest512(d_out2, "abcdef", 6, DIGEST_SHA3_512);
  tt_mem_op(d_out1,OP_EQ, d_out2, DIGEST512_LEN);
  crypto_digest_free(d1);

  /* Attempt to exercise the incremental hashing code by creating a randomized
   * 30 KiB buffer, and hashing rand[1, 5 * Rate] bytes at a time.  SHA3-512
   * is used because it has a lowest rate of the family (the code is common,
   * but the slower rate exercises more of it).
   */
  const size_t bufsz = 30 * 1024;
  size_t j = 0;
  large = tor_malloc(bufsz);
  crypto_rand(large, bufsz);
  d1 = crypto_digest512_new(DIGEST_SHA3_512); /* Running digest. */
  while (j < bufsz) {
    /* Pick how much data to add to the running digest. */
    size_t incr = (size_t)crypto_rand_int_range(1, 72 * 5);
    incr = MIN(bufsz - j, incr);

    /* Add the data, and calculate the hash. */
    crypto_digest_add_bytes(d1, large + j, incr);
    crypto_digest_get_digest(d1, d_out1, DIGEST512_LEN);

    /* One-shot hash the buffer up to the data that was just added,
     * and ensure that the values match up.
     *
     * XXX/yawning: If this actually fails, it'll be rather difficult to
     * reproduce.  Improvements welcome.
     */
    i = crypto_digest512(d_out2, large, j + incr, DIGEST_SHA3_512);
    tt_int_op(i, OP_EQ, 0);
    tt_mem_op(d_out1, OP_EQ, d_out2, DIGEST512_LEN);

    j += incr;
  }

 done:
  if (d1)
    crypto_digest_free(d1);
  if (d2)
    crypto_digest_free(d2);
  tor_free(large);
  tor_free(mem_op_hex_tmp);
}

/** Run unit tests for our XOF. */
static void
test_crypto_sha3_xof(void *arg)
{
  uint8_t msg[255];
  uint8_t out[512];
  crypto_xof_t *xof;
  char *mem_op_hex_tmp=NULL;

  (void)arg;

  /* SHAKE256 test vector (Len = 2040) from the Keccak Code Package. */
  base16_decode((char *)msg, 255,
                "3A3A819C48EFDE2AD914FBF00E18AB6BC4F14513AB27D0C178A188B61431"
                "E7F5623CB66B23346775D386B50E982C493ADBBFC54B9A3CD383382336A1"
                "A0B2150A15358F336D03AE18F666C7573D55C4FD181C29E6CCFDE63EA35F"
                "0ADF5885CFC0A3D84A2B2E4DD24496DB789E663170CEF74798AA1BBCD457"
                "4EA0BBA40489D764B2F83AADC66B148B4A0CD95246C127D5871C4F114186"
                "90A5DDF01246A0C80A43C70088B6183639DCFDA4125BD113A8F49EE23ED3"
                "06FAAC576C3FB0C1E256671D817FC2534A52F5B439F72E424DE376F4C565"
                "CCA82307DD9EF76DA5B7C4EB7E085172E328807C02D011FFBF33785378D7"
                "9DC266F6A5BE6BB0E4A92ECEEBAEB1", 510);
  const char *squeezed_hex =
                "8A5199B4A7E133E264A86202720655894D48CFF344A928CF8347F48379CE"
                "F347DFC5BCFFAB99B27B1F89AA2735E23D30088FFA03B9EDB02B9635470A"
                "B9F1038985D55F9CA774572DD006470EA65145469609F9FA0831BF1FFD84"
                "2DC24ACADE27BD9816E3B5BF2876CB112232A0EB4475F1DFF9F5C713D9FF"
                "D4CCB89AE5607FE35731DF06317949EEF646E9591CF3BE53ADD6B7DD2B60"
                "96E2B3FB06E662EC8B2D77422DAAD9463CD155204ACDBD38E319613F39F9"
                "9B6DFB35CA9365160066DB19835888C2241FF9A731A4ACBB5663727AAC34"
                "A401247FBAA7499E7D5EE5B69D31025E63D04C35C798BCA1262D5673A9CF"
                "0930B5AD89BD485599DC184528DA4790F088EBD170B635D9581632D2FF90"
                "DB79665CED430089AF13C9F21F6D443A818064F17AEC9E9C5457001FA8DC"
                "6AFBADBE3138F388D89D0E6F22F66671255B210754ED63D81DCE75CE8F18"
                "9B534E6D6B3539AA51E837C42DF9DF59C71E6171CD4902FE1BDC73FB1775"
                "B5C754A1ED4EA7F3105FC543EE0418DAD256F3F6118EA77114A16C15355B"
                "42877A1DB2A7DF0E155AE1D8670ABCEC3450F4E2EEC9838F895423EF63D2"
                "61138BAAF5D9F104CB5A957AEA06C0B9B8C78B0D441796DC0350DDEABB78"
                "A33B6F1F9E68EDE3D1805C7B7E2CFD54E0FAD62F0D8CA67A775DC4546AF9"
                "096F2EDB221DB42843D65327861282DC946A0BA01A11863AB2D1DFD16E39"
                "73D4";

  /* Test oneshot absorb/squeeze. */
  xof = crypto_xof_new();
  tt_assert(xof);
  crypto_xof_add_bytes(xof, msg, sizeof(msg));
  crypto_xof_squeeze_bytes(xof, out, sizeof(out));
  test_memeq_hex(out, squeezed_hex);
  crypto_xof_free(xof);
  memset(out, 0, sizeof(out));

  /* Test incremental absorb/squeeze. */
  xof = crypto_xof_new();
  tt_assert(xof);
  for (size_t i = 0; i < sizeof(msg); i++)
    crypto_xof_add_bytes(xof, msg + i, 1);
  for (size_t i = 0; i < sizeof(out); i++)
    crypto_xof_squeeze_bytes(xof, out + i, 1);
  test_memeq_hex(out, squeezed_hex);

 done:
  if (xof)
    crypto_xof_free(xof);
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

#ifdef HAVE_TRUNCATE
#define do_truncate truncate
#else
static int
do_truncate(const char *fname, size_t len)
{
  struct stat st;
  char *bytes;

  bytes = read_file_to_str(fname, RFTS_BIN, &st);
  if (!bytes)
    return -1;
  /* This cast isn't so great, but it should be safe given the actual files
   * and lengths we're using. */
  if (st.st_size < (off_t)len)
    len = MIN(len, (size_t)st.st_size);

  int r = write_bytes_to_file(fname, bytes, len, 1);
  tor_free(bytes);
  return r;
}
#endif

/** Sanity check for crypto pk digests  */
static void
test_crypto_digests(void *arg)
{
  crypto_pk_t *k = NULL;
  ssize_t r;
  common_digests_t pkey_digests;
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

  r = crypto_pk_get_common_digests(k, &pkey_digests);

  tt_mem_op(hex_str(pkey_digests.d[DIGEST_SHA1], DIGEST_LEN),OP_EQ,
             AUTHORITY_SIGNKEY_A_DIGEST, HEX_DIGEST_LEN);
  tt_mem_op(hex_str(pkey_digests.d[DIGEST_SHA256], DIGEST256_LEN),OP_EQ,
             AUTHORITY_SIGNKEY_A_DIGEST256, HEX_DIGEST256_LEN);
 done:
  crypto_pk_free(k);
}

static void
test_crypto_digest_names(void *arg)
{
  static const struct {
    int a; const char *n;
  } names[] = {
    { DIGEST_SHA1, "sha1" },
    { DIGEST_SHA256, "sha256" },
    { DIGEST_SHA512, "sha512" },
    { DIGEST_SHA3_256, "sha3-256" },
    { DIGEST_SHA3_512, "sha3-512" },
    { -1, NULL }
  };
  (void)arg;

  int i;
  for (i = 0; names[i].n; ++i) {
    tt_str_op(names[i].n, OP_EQ,crypto_digest_algorithm_get_name(names[i].a));
    tt_int_op(names[i].a,
              OP_EQ,crypto_digest_algorithm_parse_name(names[i].n));
  }
  tt_int_op(-1, OP_EQ,
            crypto_digest_algorithm_parse_name("TimeCubeHash-4444"));
 done:
  ;
}

#ifndef OPENSSL_1_1_API
#define EVP_ENCODE_CTX_new() tor_malloc_zero(sizeof(EVP_ENCODE_CTX))
#define EVP_ENCODE_CTX_free(ctx) tor_free(ctx)
#endif

/** Encode src into dest with OpenSSL's EVP Encode interface, returning the
 * length of the encoded data in bytes.
 */
static int
base64_encode_evp(char *dest, char *src, size_t srclen)
{
  const unsigned char *s = (unsigned char*)src;
  EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
  int len, ret;

  EVP_EncodeInit(ctx);
  EVP_EncodeUpdate(ctx, (unsigned char *)dest, &len, s, (int)srclen);
  EVP_EncodeFinal(ctx, (unsigned char *)(dest + len), &ret);
  EVP_ENCODE_CTX_free(ctx);
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
  tt_int_op(i,OP_EQ, 8);
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
test_crypto_hkdf_sha256_testvecs(void *arg)
{
  (void) arg;
  /* Test vectors from RFC5869, sections A.1 through A.3 */
  const struct {
    const char *ikm16, *salt16, *info16;
    int L;
    const char *okm16;
  } vecs[] = {
    { /* from A.1 */
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "000102030405060708090a0b0c",
      "f0f1f2f3f4f5f6f7f8f9",
      42,
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
        "34007208d5b887185865"
    },
    { /* from A.2 */
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        "404142434445464748494a4b4c4d4e4f",
      "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef"
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
      82,
      "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
      "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
      "cc30c58179ec3e87c14c01d5c1f3434f1d87"
    },
    { /* from A.3 */
      "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      "",
      "",
      42,
      "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
        "9d201395faa4b61a96c8",
    },
    { NULL, NULL, NULL, -1, NULL }
  };

  int i;
  char *ikm = NULL;
  char *salt = NULL;
  char *info = NULL;
  char *okm = NULL;
  char *mem_op_hex_tmp = NULL;

  for (i = 0; vecs[i].ikm16; ++i) {
    size_t ikm_len = strlen(vecs[i].ikm16)/2;
    size_t salt_len = strlen(vecs[i].salt16)/2;
    size_t info_len = strlen(vecs[i].info16)/2;
    size_t okm_len = vecs[i].L;

    ikm = tor_malloc(ikm_len);
    salt = tor_malloc(salt_len);
    info = tor_malloc(info_len);
    okm = tor_malloc(okm_len);

    base16_decode(ikm, ikm_len, vecs[i].ikm16, strlen(vecs[i].ikm16));
    base16_decode(salt, salt_len, vecs[i].salt16, strlen(vecs[i].salt16));
    base16_decode(info, info_len, vecs[i].info16, strlen(vecs[i].info16));

    int r = crypto_expand_key_material_rfc5869_sha256(
              (const uint8_t*)ikm, ikm_len,
              (const uint8_t*)salt, salt_len,
              (const uint8_t*)info, info_len,
              (uint8_t*)okm, okm_len);
    tt_int_op(r, OP_EQ, 0);
    test_memeq_hex(okm, vecs[i].okm16);
    tor_free(ikm);
    tor_free(salt);
    tor_free(info);
    tor_free(okm);
  }
 done:
  tor_free(ikm);
  tor_free(salt);
  tor_free(info);
  tor_free(okm);
  tor_free(mem_op_hex_tmp);
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
test_crypto_curve25519_testvec(void *arg)
{
  (void)arg;
  char *mem_op_hex_tmp = NULL;

  /* From RFC 7748, section 6.1 */
  /* Alice's private key, a: */
  const char a16[] =
    "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a";
  /* Alice's public key, X25519(a, 9): */
  const char a_pub16[] =
    "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a";
  /* Bob's private key, b: */
  const char b16[] =
    "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb";
  /* Bob's public key, X25519(b, 9): */
  const char b_pub16[] =
    "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f";
  /* Their shared secret, K: */
  const char k16[] =
    "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742";

  uint8_t a[32], b[32], a_pub[32], b_pub[32], k1[32], k2[32];
  base16_decode((char*)a, sizeof(a), a16, strlen(a16));
  base16_decode((char*)b, sizeof(b), b16, strlen(b16));
  curve25519_basepoint_impl(a_pub, a);
  curve25519_basepoint_impl(b_pub, b);
  curve25519_impl(k1, a, b_pub);
  curve25519_impl(k2, b, a_pub);

  test_memeq_hex(a, a16);
  test_memeq_hex(b, b16);
  test_memeq_hex(a_pub, a_pub16);
  test_memeq_hex(b_pub, b_pub16);
  test_memeq_hex(k1, k16);
  test_memeq_hex(k2, k16);
 done:
  tor_free(mem_op_hex_tmp);
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

  /* Test the string-prefixed sign/checksig functions */
  {
    ed25519_signature_t manual_sig;
    char *prefixed_msg;

    /* Generate a signature with a prefixed msg. */
    tt_int_op(0, OP_EQ, ed25519_sign_prefixed(&sig1, msg, msg_len,
                                              "always in the mood",
                                              &kp1));

    /* First, check that ed25519_sign_prefixed() returns the exact same sig as
       if we had manually prefixed the msg ourselves. */
    tor_asprintf(&prefixed_msg, "%s%s", "always in the mood", msg);
    tt_int_op(0, OP_EQ, ed25519_sign(&manual_sig, (uint8_t *)prefixed_msg,
                                     strlen(prefixed_msg), &kp1));
    tor_free(prefixed_msg);
    tt_assert(fast_memeq(sig1.sig, manual_sig.sig, sizeof(sig1.sig)));

    /* Test that prefixed checksig verifies it properly. */
    tt_int_op(0, OP_EQ, ed25519_checksig_prefixed(&sig1, msg, msg_len,
                                                  "always in the mood",
                                                  &pub1));

    /* Test that checksig with wrong prefix fails. */
    tt_int_op(-1, OP_EQ, ed25519_checksig_prefixed(&sig1, msg, msg_len,
                                                   "always in the moo",
                                                   &pub1));
    tt_int_op(-1, OP_EQ, ed25519_checksig_prefixed(&sig1, msg, msg_len,
                                                   "always in the moon",
                                                   &pub1));
    tt_int_op(-1, OP_EQ, ed25519_checksig_prefixed(&sig1, msg, msg_len,
                                                   "always in the mood!",
                                                   &pub1));
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
    /* These come from draft-irtf-cfrg-eddsa-05 section 7.1 */
    {
      "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
      ""
    },
    {
      "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c",
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00",
      "72"
    },
    {
      "f5e5767cf153319517630f226876b86c8160cc583bc013744c6bf255f5cc0ee5",
      "278117fc144c72340f67d0f2316e8386ceffbf2b2428c9c51fef7c597f1d426e",
      "0aab4c900501b3e24d7cdf4663326a3a87df5e4843b2cbdb67cbf6e460fec350"
        "aa5371b1508f9f4528ecea23c436d94b5e8fcd4f681e30a6ac00a9704a188a03",
      "08b8b2b733424243760fe426a4b54908632110a66c2f6591eabd3345e3e4eb98"
        "fa6e264bf09efe12ee50f8f54e9f77b1e355f6c50544e23fb1433ddf73be84d8"
        "79de7c0046dc4996d9e773f4bc9efe5738829adb26c81b37c93a1b270b20329d"
        "658675fc6ea534e0810a4432826bf58c941efb65d57a338bbd2e26640f89ffbc"
        "1a858efcb8550ee3a5e1998bd177e93a7363c344fe6b199ee5d02e82d522c4fe"
        "ba15452f80288a821a579116ec6dad2b3b310da903401aa62100ab5d1a36553e"
        "06203b33890cc9b832f79ef80560ccb9a39ce767967ed628c6ad573cb116dbef"
        "efd75499da96bd68a8a97b928a8bbc103b6621fcde2beca1231d206be6cd9ec7"
        "aff6f6c94fcd7204ed3455c68c83f4a41da4af2b74ef5c53f1d8ac70bdcb7ed1"
        "85ce81bd84359d44254d95629e9855a94a7c1958d1f8ada5d0532ed8a5aa3fb2"
        "d17ba70eb6248e594e1a2297acbbb39d502f1a8c6eb6f1ce22b3de1a1f40cc24"
        "554119a831a9aad6079cad88425de6bde1a9187ebb6092cf67bf2b13fd65f270"
        "88d78b7e883c8759d2c4f5c65adb7553878ad575f9fad878e80a0c9ba63bcbcc"
        "2732e69485bbc9c90bfbd62481d9089beccf80cfe2df16a2cf65bd92dd597b07"
        "07e0917af48bbb75fed413d238f5555a7a569d80c3414a8d0859dc65a46128ba"
        "b27af87a71314f318c782b23ebfe808b82b0ce26401d2e22f04d83d1255dc51a"
        "ddd3b75a2b1ae0784504df543af8969be3ea7082ff7fc9888c144da2af58429e"
        "c96031dbcad3dad9af0dcbaaaf268cb8fcffead94f3c7ca495e056a9b47acdb7"
        "51fb73e666c6c655ade8297297d07ad1ba5e43f1bca32301651339e22904cc8c"
        "42f58c30c04aafdb038dda0847dd988dcda6f3bfd15c4b4c4525004aa06eeff8"
        "ca61783aacec57fb3d1f92b0fe2fd1a85f6724517b65e614ad6808d6f6ee34df"
        "f7310fdc82aebfd904b01e1dc54b2927094b2db68d6f903b68401adebf5a7e08"
        "d78ff4ef5d63653a65040cf9bfd4aca7984a74d37145986780fc0b16ac451649"
        "de6188a7dbdf191f64b5fc5e2ab47b57f7f7276cd419c17a3ca8e1b939ae49e4"
        "88acba6b965610b5480109c8b17b80e1b7b750dfc7598d5d5011fd2dcc5600a3"
        "2ef5b52a1ecc820e308aa342721aac0943bf6686b64b2579376504ccc493d97e"
        "6aed3fb0f9cd71a43dd497f01f17c0e2cb3797aa2a2f256656168e6c496afc5f"
        "b93246f6b1116398a346f1a641f3b041e989f7914f90cc2c7fff357876e506b5"
        "0d334ba77c225bc307ba537152f3f1610e4eafe595f6d9d90d11faa933a15ef1"
        "369546868a7f3a45a96768d40fd9d03412c091c6315cf4fde7cb68606937380d"
        "b2eaaa707b4c4185c32eddcdd306705e4dc1ffc872eeee475a64dfac86aba41c"
        "0618983f8741c5ef68d3a101e8a3b8cac60c905c15fc910840b94c00a0b9d0"
    },
    {
      "833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
      "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf",
      "dc2a4459e7369633a52b1bf277839a00201009a3efbf3ecb69bea2186c26b589"
        "09351fc9ac90b3ecfdfbc7c66431e0303dca179c138ac17ad9bef1177331a704",
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
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

    tt_int_op(sizeof(sk), OP_EQ, DECODE(sk, ED25519_SECRET_KEYS[i]));
    tt_int_op(sizeof(blinding_param), OP_EQ, DECODE(blinding_param,
              ED25519_BLINDING_PARAMS[i]));

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
test_crypto_ed25519_storage(void *arg)
{
  (void)arg;
  ed25519_keypair_t *keypair = NULL;
  ed25519_public_key_t pub;
  ed25519_secret_key_t sec;
  char *fname_1 = tor_strdup(get_fname("ed_seckey_1"));
  char *fname_2 = tor_strdup(get_fname("ed_pubkey_2"));
  char *contents = NULL;
  char *tag = NULL;

  keypair = tor_malloc_zero(sizeof(ed25519_keypair_t));
  tt_int_op(0,OP_EQ,ed25519_keypair_generate(keypair, 0));
  tt_int_op(0,OP_EQ,
            ed25519_seckey_write_to_file(&keypair->seckey, fname_1, "foo"));
  tt_int_op(0,OP_EQ,
            ed25519_pubkey_write_to_file(&keypair->pubkey, fname_2, "bar"));

  tt_int_op(-1, OP_EQ, ed25519_pubkey_read_from_file(&pub, &tag, fname_1));
  tt_ptr_op(tag, OP_EQ, NULL);
  tt_int_op(-1, OP_EQ, ed25519_seckey_read_from_file(&sec, &tag, fname_2));
  tt_ptr_op(tag, OP_EQ, NULL);

  tt_int_op(0, OP_EQ, ed25519_pubkey_read_from_file(&pub, &tag, fname_2));
  tt_str_op(tag, OP_EQ, "bar");
  tor_free(tag);
  tt_int_op(0, OP_EQ, ed25519_seckey_read_from_file(&sec, &tag, fname_1));
  tt_str_op(tag, OP_EQ, "foo");
  tor_free(tag);

  /* whitebox test: truncated keys. */
  tt_int_op(0, ==, do_truncate(fname_1, 40));
  tt_int_op(0, ==, do_truncate(fname_2, 40));
  tt_int_op(-1, OP_EQ, ed25519_pubkey_read_from_file(&pub, &tag, fname_2));
  tt_ptr_op(tag, OP_EQ, NULL);
  tor_free(tag);
  tt_int_op(-1, OP_EQ, ed25519_seckey_read_from_file(&sec, &tag, fname_1));
  tt_ptr_op(tag, OP_EQ, NULL);

 done:
  tor_free(fname_1);
  tor_free(fname_2);
  tor_free(contents);
  tor_free(tag);
  ed25519_keypair_free(keypair);
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

/* We want the likelihood that the random buffer exhibits any regular pattern
 * to be far less than the memory bit error rate in the int return value.
 * Using 2048 bits provides a failure rate of 1/(3 * 10^616), and we call
 * 3 functions, leading to an overall error rate of 1/10^616.
 * This is comparable with the 1/10^603 failure rate of test_crypto_rng_range.
 */
#define FAILURE_MODE_BUFFER_SIZE (2048/8)

/** Check crypto_rand for a failure mode where it does nothing to the buffer,
 * or it sets the buffer to all zeroes. Return 0 when the check passes,
 * or -1 when it fails. */
static int
crypto_rand_check_failure_mode_zero(void)
{
  char buf[FAILURE_MODE_BUFFER_SIZE];

  memset(buf, 0, FAILURE_MODE_BUFFER_SIZE);
  crypto_rand(buf, FAILURE_MODE_BUFFER_SIZE);

  for (size_t i = 0; i < FAILURE_MODE_BUFFER_SIZE; i++) {
    if (buf[i] != 0) {
      return 0;
    }
  }

  return -1;
}

/** Check crypto_rand for a failure mode where every int64_t in the buffer is
 * the same. Return 0 when the check passes, or -1 when it fails. */
static int
crypto_rand_check_failure_mode_identical(void)
{
  /* just in case the buffer size isn't a multiple of sizeof(int64_t) */
#define FAILURE_MODE_BUFFER_SIZE_I64 \
  (FAILURE_MODE_BUFFER_SIZE/SIZEOF_INT64_T)
#define FAILURE_MODE_BUFFER_SIZE_I64_BYTES \
  (FAILURE_MODE_BUFFER_SIZE_I64*SIZEOF_INT64_T)

#if FAILURE_MODE_BUFFER_SIZE_I64 < 2
#error FAILURE_MODE_BUFFER_SIZE needs to be at least 2*SIZEOF_INT64_T
#endif

  int64_t buf[FAILURE_MODE_BUFFER_SIZE_I64];

  memset(buf, 0, FAILURE_MODE_BUFFER_SIZE_I64_BYTES);
  crypto_rand((char *)buf, FAILURE_MODE_BUFFER_SIZE_I64_BYTES);

  for (size_t i = 1; i < FAILURE_MODE_BUFFER_SIZE_I64; i++) {
    if (buf[i] != buf[i-1]) {
      return 0;
    }
  }

  return -1;
}

/** Check crypto_rand for a failure mode where it increments the "random"
 * value by 1 for every byte in the buffer. (This is OpenSSL's PREDICT mode.)
 * Return 0 when the check passes, or -1 when it fails. */
static int
crypto_rand_check_failure_mode_predict(void)
{
  unsigned char buf[FAILURE_MODE_BUFFER_SIZE];

  memset(buf, 0, FAILURE_MODE_BUFFER_SIZE);
  crypto_rand((char *)buf, FAILURE_MODE_BUFFER_SIZE);

  for (size_t i = 1; i < FAILURE_MODE_BUFFER_SIZE; i++) {
    /* check if the last byte was incremented by 1, including integer
     * wrapping */
    if (buf[i] - buf[i-1] != 1 && buf[i-1] - buf[i] != 255) {
      return 0;
    }
  }

  return -1;
}

#undef FAILURE_MODE_BUFFER_SIZE

static void
test_crypto_failure_modes(void *arg)
{
  int rv = 0;
  (void)arg;

  rv = crypto_early_init();
  tt_assert(rv == 0);

  /* Check random works */
  rv = crypto_rand_check_failure_mode_zero();
  tt_assert(rv == 0);

  rv = crypto_rand_check_failure_mode_identical();
  tt_assert(rv == 0);

  rv = crypto_rand_check_failure_mode_predict();
  tt_assert(rv == 0);

 done:
  ;
}

#define CRYPTO_LEGACY(name)                                            \
  { #name, test_crypto_ ## name , 0, NULL, NULL }

#define ED25519_TEST_ONE(name, fl, which)                               \
  { #name "/ed25519_" which, test_crypto_ed25519_ ## name, (fl),        \
    &ed25519_test_setup, (void*)which }

#define ED25519_TEST(name, fl)                  \
  ED25519_TEST_ONE(name, (fl), "donna"),        \
  ED25519_TEST_ONE(name, (fl), "ref10")

struct testcase_t crypto_tests[] = {
  CRYPTO_LEGACY(formats),
  CRYPTO_LEGACY(rng),
  { "rng_range", test_crypto_rng_range, 0, NULL, NULL },
  { "rng_engine", test_crypto_rng_engine, TT_FORK, NULL, NULL },
  { "rng_strongest", test_crypto_rng_strongest, TT_FORK, NULL, NULL },
  { "rng_strongest_nosyscall", test_crypto_rng_strongest, TT_FORK,
    &passthrough_setup, (void*)"nosyscall" },
  { "rng_strongest_nofallback", test_crypto_rng_strongest, TT_FORK,
    &passthrough_setup, (void*)"nofallback" },
  { "rng_strongest_broken", test_crypto_rng_strongest, TT_FORK,
    &passthrough_setup, (void*)"broken" },
  { "openssl_version", test_crypto_openssl_version, TT_FORK, NULL, NULL },
  { "aes_AES", test_crypto_aes128, TT_FORK, &passthrough_setup, (void*)"aes" },
  { "aes_EVP", test_crypto_aes128, TT_FORK, &passthrough_setup, (void*)"evp" },
  { "aes128_ctr_testvec", test_crypto_aes_ctr_testvec, 0,
    &passthrough_setup, (void*)"128" },
  { "aes192_ctr_testvec", test_crypto_aes_ctr_testvec, 0,
    &passthrough_setup, (void*)"192" },
  { "aes256_ctr_testvec", test_crypto_aes_ctr_testvec, 0,
    &passthrough_setup, (void*)"256" },
  CRYPTO_LEGACY(sha),
  CRYPTO_LEGACY(pk),
  { "pk_fingerprints", test_crypto_pk_fingerprints, TT_FORK, NULL, NULL },
  { "pk_base64", test_crypto_pk_base64, TT_FORK, NULL, NULL },
  CRYPTO_LEGACY(digests),
  { "digest_names", test_crypto_digest_names, 0, NULL, NULL },
  { "sha3", test_crypto_sha3, TT_FORK, NULL, NULL},
  { "sha3_xof", test_crypto_sha3_xof, TT_FORK, NULL, NULL},
  CRYPTO_LEGACY(dh),
  { "aes_iv_AES", test_crypto_aes_iv, TT_FORK, &passthrough_setup,
    (void*)"aes" },
  { "aes_iv_EVP", test_crypto_aes_iv, TT_FORK, &passthrough_setup,
    (void*)"evp" },
  CRYPTO_LEGACY(base32_decode),
  { "kdf_TAP", test_crypto_kdf_TAP, 0, NULL, NULL },
  { "hkdf_sha256", test_crypto_hkdf_sha256, 0, NULL, NULL },
  { "hkdf_sha256_testvecs", test_crypto_hkdf_sha256_testvecs, 0, NULL, NULL },
  { "curve25519_impl", test_crypto_curve25519_impl, 0, NULL, NULL },
  { "curve25519_impl_hibit", test_crypto_curve25519_impl, 0, NULL, (void*)"y"},
  { "curve25516_testvec", test_crypto_curve25519_testvec, 0, NULL, NULL },
  { "curve25519_basepoint",
    test_crypto_curve25519_basepoint, TT_FORK, NULL, NULL },
  { "curve25519_wrappers", test_crypto_curve25519_wrappers, 0, NULL, NULL },
  { "curve25519_encode", test_crypto_curve25519_encode, 0, NULL, NULL },
  { "curve25519_persist", test_crypto_curve25519_persist, 0, NULL, NULL },
  ED25519_TEST(simple, 0),
  ED25519_TEST(test_vectors, 0),
  ED25519_TEST(encode, 0),
  ED25519_TEST(convert, 0),
  ED25519_TEST(blinding, 0),
  ED25519_TEST(testvectors, 0),
  { "ed25519_storage", test_crypto_ed25519_storage, 0, NULL, NULL },
  { "siphash", test_crypto_siphash, 0, NULL, NULL },
  { "failure_modes", test_crypto_failure_modes, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

