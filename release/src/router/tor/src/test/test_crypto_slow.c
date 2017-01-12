/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define CRYPTO_S2K_PRIVATE
#include "or.h"
#include "test.h"
#include "crypto_s2k.h"
#include "crypto_pwbox.h"

#if defined(HAVE_LIBSCRYPT_H) && defined(HAVE_LIBSCRYPT_SCRYPT)
#define HAVE_LIBSCRYPT
#include <libscrypt.h>
#endif

#include <openssl/evp.h>

/** Run unit tests for our secret-to-key passphrase hashing functionality. */
static void
test_crypto_s2k_rfc2440(void *arg)
{
  char buf[29];
  char buf2[29];
  char *buf3 = NULL;
  int i;

  (void)arg;
  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));
  buf3 = tor_malloc(65536);
  memset(buf3, 0, 65536);

  secret_to_key_rfc2440(buf+9, 20, "", 0, buf);
  crypto_digest(buf2+9, buf3, 1024);
  tt_mem_op(buf,OP_EQ, buf2, 29);

  memcpy(buf,"vrbacrda",8);
  memcpy(buf2,"vrbacrda",8);
  buf[8] = 96;
  buf2[8] = 96;
  secret_to_key_rfc2440(buf+9, 20, "12345678", 8, buf);
  for (i = 0; i < 65536; i += 16) {
    memcpy(buf3+i, "vrbacrda12345678", 16);
  }
  crypto_digest(buf2+9, buf3, 65536);
  tt_mem_op(buf,OP_EQ, buf2, 29);

 done:
  tor_free(buf3);
}

static void
run_s2k_tests(const unsigned flags, const unsigned type,
              int speclen, const int keylen, int legacy)
{
  uint8_t buf[S2K_MAXLEN], buf2[S2K_MAXLEN], buf3[S2K_MAXLEN];
  int r;
  size_t sz;
  const char pw1[] = "You can't come in here unless you say swordfish!";
  const char pw2[] = "Now, I give you one more guess.";

  r = secret_to_key_new(buf, sizeof(buf), &sz,
                        pw1, strlen(pw1), flags);
  tt_int_op(r, OP_EQ, S2K_OKAY);
  tt_int_op(buf[0], OP_EQ, type);

  tt_int_op(sz, OP_EQ, keylen + speclen);

  if (legacy) {
    memmove(buf, buf+1, sz-1);
    --sz;
    --speclen;
  }

  tt_int_op(S2K_OKAY, OP_EQ,
            secret_to_key_check(buf, sz, pw1, strlen(pw1)));

  tt_int_op(S2K_BAD_SECRET, OP_EQ,
            secret_to_key_check(buf, sz, pw2, strlen(pw2)));

  /* Move key to buf2, and clear it. */
  memset(buf3, 0, sizeof(buf3));
  memcpy(buf2, buf+speclen, keylen);
  memset(buf+speclen, 0, sz - speclen);

  /* Derivekey should produce the same results. */
  tt_int_op(S2K_OKAY, OP_EQ,
      secret_to_key_derivekey(buf3, keylen, buf, speclen, pw1, strlen(pw1)));

  tt_mem_op(buf2, OP_EQ, buf3, keylen);

  /* Derivekey with a longer output should fill the output. */
  memset(buf2, 0, sizeof(buf2));
  tt_int_op(S2K_OKAY, OP_EQ,
   secret_to_key_derivekey(buf2, sizeof(buf2), buf, speclen,
                           pw1, strlen(pw1)));

  tt_mem_op(buf2, OP_NE, buf3, sizeof(buf2));

  memset(buf3, 0, sizeof(buf3));
  tt_int_op(S2K_OKAY, OP_EQ,
            secret_to_key_derivekey(buf3, sizeof(buf3), buf, speclen,
                                    pw1, strlen(pw1)));
  tt_mem_op(buf2, OP_EQ, buf3, sizeof(buf3));
  tt_assert(!tor_mem_is_zero((char*)buf2+keylen, sizeof(buf2)-keylen));

 done:
  ;
}

static void
test_crypto_s2k_general(void *arg)
{
  const char *which = arg;

  if (!strcmp(which, "scrypt")) {
    run_s2k_tests(0, 2, 19, 32, 0);
  } else if (!strcmp(which, "scrypt-low")) {
    run_s2k_tests(S2K_FLAG_LOW_MEM, 2, 19, 32, 0);
  } else if (!strcmp(which, "pbkdf2")) {
    run_s2k_tests(S2K_FLAG_USE_PBKDF2, 1, 18, 20, 0);
  } else if (!strcmp(which, "rfc2440")) {
    run_s2k_tests(S2K_FLAG_NO_SCRYPT, 0, 10, 20, 0);
  } else if (!strcmp(which, "rfc2440-legacy")) {
    run_s2k_tests(S2K_FLAG_NO_SCRYPT, 0, 10, 20, 1);
  } else {
    tt_fail();
  }
}

#if defined(HAVE_LIBSCRYPT) && defined(HAVE_EVP_PBE_SCRYPT)
static void
test_libscrypt_eq_openssl(void *arg)
{
  uint8_t buf1[64];
  uint8_t buf2[64];

  uint64_t N, r, p;
  uint64_t maxmem = 0; // --> SCRYPT_MAX_MEM in OpenSSL.

  int libscrypt_retval, openssl_retval;

  size_t dk_len = 64;

  (void)arg;

  memset(buf1,0,64);
  memset(buf2,0,64);

  /* NOTE: we're using N,r the way OpenSSL and libscrypt define them,
   * not the way draft-josefsson-scrypt-kdf-00.txt define them.
   */
  N = 16;
  r = 1;
  p = 1;

  libscrypt_retval =
  libscrypt_scrypt((const uint8_t *)"", 0, (const uint8_t *)"", 0,
                   N, r, p, buf1, dk_len);
  openssl_retval =
  EVP_PBE_scrypt((const char *)"", 0, (const unsigned char *)"", 0,
                  N, r, p, maxmem, buf2, dk_len);

  tt_int_op(libscrypt_retval, ==, 0);
  tt_int_op(openssl_retval, ==, 1);

  tt_mem_op(buf1, ==, buf2, 64);

  memset(buf1,0,64);
  memset(buf2,0,64);

  N = 1024;
  r = 8;
  p = 16;

  libscrypt_retval =
  libscrypt_scrypt((const uint8_t *)"password", strlen("password"),
                   (const uint8_t *)"NaCl", strlen("NaCl"),
                   N, r, p, buf1, dk_len);
  openssl_retval =
  EVP_PBE_scrypt((const char *)"password", strlen("password"),
                 (const unsigned char *)"NaCl", strlen("NaCl"),
                 N, r, p, maxmem, buf2, dk_len);

  tt_int_op(libscrypt_retval, ==, 0);
  tt_int_op(openssl_retval, ==, 1);

  tt_mem_op(buf1, ==, buf2, 64);

  memset(buf1,0,64);
  memset(buf2,0,64);

  N = 16384;
  r = 8;
  p = 1;

  libscrypt_retval =
  libscrypt_scrypt((const uint8_t *)"pleaseletmein",
                   strlen("pleaseletmein"),
                   (const uint8_t *)"SodiumChloride",
                   strlen("SodiumChloride"),
                   N, r, p, buf1, dk_len);
  openssl_retval =
  EVP_PBE_scrypt((const char *)"pleaseletmein",
                 strlen("pleaseletmein"),
                 (const unsigned char *)"SodiumChloride",
                 strlen("SodiumChloride"),
                 N, r, p, maxmem, buf2, dk_len);

  tt_int_op(libscrypt_retval, ==, 0);
  tt_int_op(openssl_retval, ==, 1);

  tt_mem_op(buf1, ==, buf2, 64);

  memset(buf1,0,64);
  memset(buf2,0,64);

  N = 1048576;
  maxmem = 2 * 1024 * 1024 * (uint64_t)1024; // 2 GB

  libscrypt_retval =
  libscrypt_scrypt((const uint8_t *)"pleaseletmein",
                   strlen("pleaseletmein"),
                   (const uint8_t *)"SodiumChloride",
                   strlen("SodiumChloride"),
                   N, r, p, buf1, dk_len);
  openssl_retval =
  EVP_PBE_scrypt((const char *)"pleaseletmein",
                 strlen("pleaseletmein"),
                 (const unsigned char *)"SodiumChloride",
                 strlen("SodiumChloride"),
                 N, r, p, maxmem, buf2, dk_len);

  tt_int_op(libscrypt_retval, ==, 0);
  tt_int_op(openssl_retval, ==, 1);

  tt_mem_op(buf1, ==, buf2, 64);

  done:
  return;
}
#endif

static void
test_crypto_s2k_errors(void *arg)
{
  uint8_t buf[S2K_MAXLEN], buf2[S2K_MAXLEN];
  size_t sz;

  (void)arg;

  /* Bogus specifiers: simple */
  tt_int_op(S2K_BAD_LEN, OP_EQ,
            secret_to_key_derivekey(buf, sizeof(buf),
                                    (const uint8_t*)"", 0, "ABC", 3));
  tt_int_op(S2K_BAD_ALGORITHM, OP_EQ,
            secret_to_key_derivekey(buf, sizeof(buf),
                                    (const uint8_t*)"\x10", 1, "ABC", 3));
  tt_int_op(S2K_BAD_LEN, OP_EQ,
            secret_to_key_derivekey(buf, sizeof(buf),
                                    (const uint8_t*)"\x01\x02", 2, "ABC", 3));

  tt_int_op(S2K_BAD_LEN, OP_EQ,
            secret_to_key_check((const uint8_t*)"", 0, "ABC", 3));
  tt_int_op(S2K_BAD_ALGORITHM, OP_EQ,
            secret_to_key_check((const uint8_t*)"\x10", 1, "ABC", 3));
  tt_int_op(S2K_BAD_LEN, OP_EQ,
            secret_to_key_check((const uint8_t*)"\x01\x02", 2, "ABC", 3));

  /* too long gets "BAD_LEN" too */
  memset(buf, 0, sizeof(buf));
  buf[0] = 2;
  tt_int_op(S2K_BAD_LEN, OP_EQ,
            secret_to_key_derivekey(buf2, sizeof(buf2),
                                    buf, sizeof(buf), "ABC", 3));

  /* Truncated output */
#ifdef HAVE_LIBSCRYPT
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_new(buf, 50, &sz,
                                                 "ABC", 3, 0));
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_new(buf, 50, &sz,
                                                 "ABC", 3, S2K_FLAG_LOW_MEM));
#endif
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_new(buf, 37, &sz,
                                              "ABC", 3, S2K_FLAG_USE_PBKDF2));
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_new(buf, 29, &sz,
                                              "ABC", 3, S2K_FLAG_NO_SCRYPT));

#ifdef HAVE_LIBSCRYPT
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_make_specifier(buf, 18, 0));
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_make_specifier(buf, 18,
                                                 S2K_FLAG_LOW_MEM));
#endif
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_make_specifier(buf, 17,
                                                 S2K_FLAG_USE_PBKDF2));
  tt_int_op(S2K_TRUNCATED, OP_EQ, secret_to_key_make_specifier(buf, 9,
                                                 S2K_FLAG_NO_SCRYPT));

  /* Now try using type-specific bogus specifiers. */

  /* It's a bad pbkdf2 buffer if it has an iteration count that would overflow
   * int32_t. */
  memset(buf, 0, sizeof(buf));
  buf[0] = 1; /* pbkdf2 */
  buf[17] = 100; /* 1<<100 is much bigger than INT32_MAX */
  tt_int_op(S2K_BAD_PARAMS, OP_EQ,
            secret_to_key_derivekey(buf2, sizeof(buf2),
                                    buf, 18, "ABC", 3));

#ifdef HAVE_LIBSCRYPT
  /* It's a bad scrypt buffer if N would overflow uint64 */
  memset(buf, 0, sizeof(buf));
  buf[0] = 2; /* scrypt */
  buf[17] = 100; /* 1<<100 is much bigger than UINT64_MAX */
  tt_int_op(S2K_BAD_PARAMS, OP_EQ,
            secret_to_key_derivekey(buf2, sizeof(buf2),
                                    buf, 19, "ABC", 3));
#endif

 done:
  ;
}

static void
test_crypto_scrypt_vectors(void *arg)
{
  char *mem_op_hex_tmp = NULL;
  uint8_t spec[64], out[64];

  (void)arg;
#ifndef HAVE_LIBSCRYPT
  if (1)
    tt_skip();
#endif

  /* Test vectors from
     http://tools.ietf.org/html/draft-josefsson-scrypt-kdf-00 section 11.

     Note that the names of 'r' and 'N' are switched in that section. Or
     possibly in libscrypt.
  */

  base16_decode((char*)spec, sizeof(spec),
                "0400", 4);
  memset(out, 0x00, sizeof(out));
  tt_int_op(64, OP_EQ,
            secret_to_key_compute_key(out, 64, spec, 2, "", 0, 2));
  test_memeq_hex(out,
                 "77d6576238657b203b19ca42c18a0497"
                 "f16b4844e3074ae8dfdffa3fede21442"
                 "fcd0069ded0948f8326a753a0fc81f17"
                 "e8d3e0fb2e0d3628cf35e20c38d18906");

  base16_decode((char*)spec, sizeof(spec),
                "4e61436c" "0A34", 12);
  memset(out, 0x00, sizeof(out));
  tt_int_op(64, OP_EQ,
            secret_to_key_compute_key(out, 64, spec, 6, "password", 8, 2));
  test_memeq_hex(out,
                 "fdbabe1c9d3472007856e7190d01e9fe"
                 "7c6ad7cbc8237830e77376634b373162"
                 "2eaf30d92e22a3886ff109279d9830da"
                 "c727afb94a83ee6d8360cbdfa2cc0640");

  base16_decode((char*)spec, sizeof(spec),
                "536f6469756d43686c6f72696465" "0e30", 32);
  memset(out, 0x00, sizeof(out));
  tt_int_op(64, OP_EQ,
            secret_to_key_compute_key(out, 64, spec, 16,
                                      "pleaseletmein", 13, 2));
  test_memeq_hex(out,
                 "7023bdcb3afd7348461c06cd81fd38eb"
                 "fda8fbba904f8e3ea9b543f6545da1f2"
                 "d5432955613f0fcf62d49705242a9af9"
                 "e61e85dc0d651e40dfcf017b45575887");

  base16_decode((char*)spec, sizeof(spec),
                "536f6469756d43686c6f72696465" "1430", 32);
  memset(out, 0x00, sizeof(out));
  tt_int_op(64, OP_EQ,
            secret_to_key_compute_key(out, 64, spec, 16,
                                      "pleaseletmein", 13, 2));
  test_memeq_hex(out,
                 "2101cb9b6a511aaeaddbbe09cf70f881"
                 "ec568d574a2ffd4dabe5ee9820adaa47"
                 "8e56fd8f4ba5d09ffa1c6d927c40f4c3"
                 "37304049e8a952fbcbf45c6fa77a41a4");

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_pbkdf2_vectors(void *arg)
{
  char *mem_op_hex_tmp = NULL;
  uint8_t spec[64], out[64];
  (void)arg;

  /* Test vectors from RFC6070, section 2 */
  base16_decode((char*)spec, sizeof(spec),
                "73616c74" "00" , 10);
  memset(out, 0x00, sizeof(out));
  tt_int_op(20, OP_EQ,
            secret_to_key_compute_key(out, 20, spec, 5, "password", 8, 1));
  test_memeq_hex(out, "0c60c80f961f0e71f3a9b524af6012062fe037a6");

  base16_decode((char*)spec, sizeof(spec),
                "73616c74" "01" , 10);
  memset(out, 0x00, sizeof(out));
  tt_int_op(20, OP_EQ,
            secret_to_key_compute_key(out, 20, spec, 5, "password", 8, 1));
  test_memeq_hex(out, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957");

  base16_decode((char*)spec, sizeof(spec),
                "73616c74" "0C" , 10);
  memset(out, 0x00, sizeof(out));
  tt_int_op(20, OP_EQ,
            secret_to_key_compute_key(out, 20, spec, 5, "password", 8, 1));
  test_memeq_hex(out, "4b007901b765489abead49d926f721d065a429c1");

  /* This is the very slow one here.  When enabled, it accounts for roughly
   * half the time spent in test-slow. */
  /*
  base16_decode((char*)spec, sizeof(spec),
                "73616c74" "18" , 10);
  memset(out, 0x00, sizeof(out));
  tt_int_op(20, OP_EQ,
            secret_to_key_compute_key(out, 20, spec, 5, "password", 8, 1));
  test_memeq_hex(out, "eefe3d61cd4da4e4e9945b3d6ba2158c2634e984");
  */

  base16_decode((char*)spec, sizeof(spec),
                "73616c7453414c5473616c7453414c5473616c745"
                "3414c5473616c7453414c5473616c74" "0C" , 74);
  memset(out, 0x00, sizeof(out));
  tt_int_op(25, OP_EQ,
            secret_to_key_compute_key(out, 25, spec, 37,
                                      "passwordPASSWORDpassword", 24, 1));
  test_memeq_hex(out, "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038");

  base16_decode((char*)spec, sizeof(spec),
                "7361006c74" "0c" , 12);
  memset(out, 0x00, sizeof(out));
  tt_int_op(16, OP_EQ,
            secret_to_key_compute_key(out, 16, spec, 6, "pass\0word", 9, 1));
  test_memeq_hex(out, "56fa6aa75548099dcc37d7f03425e0c3");

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_crypto_pwbox(void *arg)
{
  uint8_t *boxed=NULL, *decoded=NULL;
  size_t len, dlen;
  unsigned i;
  const char msg[] = "This bunny reminds you that you still have a "
    "salamander in your sylladex. She is holding the bunny Dave got you. "
    "It’s sort of uncanny how similar they are, aside from the knitted "
    "enhancements. Seriously, what are the odds?? So weird.";
  const char pw[] = "I'm a night owl and a wise bird too";

  const unsigned flags[] = { 0,
                             S2K_FLAG_NO_SCRYPT,
                             S2K_FLAG_LOW_MEM,
                             S2K_FLAG_NO_SCRYPT|S2K_FLAG_LOW_MEM,
                             S2K_FLAG_USE_PBKDF2 };
  (void)arg;

  for (i = 0; i < ARRAY_LENGTH(flags); ++i) {
    tt_int_op(0, OP_EQ, crypto_pwbox(&boxed, &len,
                                  (const uint8_t*)msg, strlen(msg),
                                  pw, strlen(pw), flags[i]));
    tt_assert(boxed);
    tt_assert(len > 128+32);

    tt_int_op(0, OP_EQ, crypto_unpwbox(&decoded, &dlen, boxed, len,
                                    pw, strlen(pw)));

    tt_assert(decoded);
    tt_uint_op(dlen, OP_EQ, strlen(msg));
    tt_mem_op(decoded, OP_EQ, msg, dlen);

    tor_free(decoded);

    tt_int_op(UNPWBOX_BAD_SECRET, OP_EQ, crypto_unpwbox(&decoded, &dlen,
                                                     boxed, len,
                                                     pw, strlen(pw)-1));
    boxed[len-1] ^= 1;
    tt_int_op(UNPWBOX_BAD_SECRET, OP_EQ, crypto_unpwbox(&decoded, &dlen,
                                                     boxed, len,
                                                     pw, strlen(pw)));
    boxed[0] = 255;
    tt_int_op(UNPWBOX_CORRUPTED, OP_EQ, crypto_unpwbox(&decoded, &dlen,
                                                    boxed, len,
                                                    pw, strlen(pw)));

    tor_free(boxed);
  }

 done:
  tor_free(boxed);
  tor_free(decoded);
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

#define CRYPTO_LEGACY(name)                                            \
  { #name, test_crypto_ ## name , 0, NULL, NULL }

#define ED25519_TEST_ONE(name, fl, which)                               \
  { #name "/ed25519_" which, test_crypto_ed25519_ ## name, (fl),        \
    &ed25519_test_setup, (void*)which }

#define ED25519_TEST(name, fl)                  \
  ED25519_TEST_ONE(name, (fl), "donna"),        \
  ED25519_TEST_ONE(name, (fl), "ref10")

struct testcase_t slow_crypto_tests[] = {
  CRYPTO_LEGACY(s2k_rfc2440),
#ifdef HAVE_LIBSCRYPT
  { "s2k_scrypt", test_crypto_s2k_general, 0, &passthrough_setup,
    (void*)"scrypt" },
  { "s2k_scrypt_low", test_crypto_s2k_general, 0, &passthrough_setup,
    (void*)"scrypt-low" },
#ifdef HAVE_EVP_PBE_SCRYPT
  { "libscrypt_eq_openssl", test_libscrypt_eq_openssl, 0, NULL, NULL },
#endif
#endif
  { "s2k_pbkdf2", test_crypto_s2k_general, 0, &passthrough_setup,
    (void*)"pbkdf2" },
  { "s2k_rfc2440_general", test_crypto_s2k_general, 0, &passthrough_setup,
    (void*)"rfc2440" },
  { "s2k_rfc2440_legacy", test_crypto_s2k_general, 0, &passthrough_setup,
    (void*)"rfc2440-legacy" },
  { "s2k_errors", test_crypto_s2k_errors, 0, NULL, NULL },
  { "scrypt_vectors", test_crypto_scrypt_vectors, 0, NULL, NULL },
  { "pbkdf2_vectors", test_crypto_pbkdf2_vectors, 0, NULL, NULL },
  { "pwbox", test_crypto_pwbox, 0, NULL, NULL },
  ED25519_TEST(fuzz_donna, TT_FORK),
  END_OF_TESTCASES
};

