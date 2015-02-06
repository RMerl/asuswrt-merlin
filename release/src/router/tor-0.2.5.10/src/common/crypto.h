/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file crypto.h
 *
 * \brief Headers for crypto.c
 **/

#ifndef TOR_CRYPTO_H
#define TOR_CRYPTO_H

#include <stdio.h>
#include "torint.h"
#include "testsupport.h"

/*
  Macro to create an arbitrary OpenSSL version number as used by
  OPENSSL_VERSION_NUMBER or SSLeay(), since the actual numbers are a bit hard
  to read.

  Don't use this directly, instead use one of the other OPENSSL_V macros
  below.

  The format is: 4 bits major, 8 bits minor, 8 bits fix, 8 bits patch, 4 bit
  status.
 */
#define OPENSSL_VER(a,b,c,d,e)                                \
  (((a)<<28) |                                                \
   ((b)<<20) |                                                \
   ((c)<<12) |                                                \
   ((d)<< 4) |                                                \
    (e))
/** An openssl release number.  For example, OPENSSL_V(0,9,8,'j') is the
 * version for the released version of 0.9.8j */
#define OPENSSL_V(a,b,c,d) \
  OPENSSL_VER((a),(b),(c),(d)-'a'+1,0xf)
/** An openssl release number for the first release in the series.  For
 * example, OPENSSL_V_NOPATCH(1,0,0) is the first released version of OpenSSL
 * 1.0.0. */
#define OPENSSL_V_NOPATCH(a,b,c) \
  OPENSSL_VER((a),(b),(c),0,0xf)
/** The first version that would occur for any alpha or beta in an openssl
 * series. For example, OPENSSL_V_SERIES(0,9,8) is greater than any released
 * 0.9.7, and less than any released 0.9.8. */
#define OPENSSL_V_SERIES(a,b,c) \
  OPENSSL_VER((a),(b),(c),0,0)

/** Length of the output of our message digest. */
#define DIGEST_LEN 20
/** Length of the output of our second (improved) message digests.  (For now
 * this is just sha256, but it could be any other 256-bit digest.) */
#define DIGEST256_LEN 32
/** Length of our symmetric cipher's keys. */
#define CIPHER_KEY_LEN 16
/** Length of our symmetric cipher's IV. */
#define CIPHER_IV_LEN 16
/** Length of our public keys. */
#define PK_BYTES (1024/8)
/** Length of our DH keys. */
#define DH_BYTES (1024/8)

/** Length of a sha1 message digest when encoded in base64 with trailing =
 * signs removed. */
#define BASE64_DIGEST_LEN 27
/** Length of a sha256 message digest when encoded in base64 with trailing =
 * signs removed. */
#define BASE64_DIGEST256_LEN 43

/** Constant used to indicate OAEP padding for public-key encryption */
#define PK_PKCS1_OAEP_PADDING 60002

/** Number of bytes added for PKCS1-OAEP padding. */
#define PKCS1_OAEP_PADDING_OVERHEAD 42

/** Length of encoded public key fingerprints, including space; but not
 * including terminating NUL. */
#define FINGERPRINT_LEN 49
/** Length of hex encoding of SHA1 digest, not including final NUL. */
#define HEX_DIGEST_LEN 40
/** Length of hex encoding of SHA256 digest, not including final NUL. */
#define HEX_DIGEST256_LEN 64

typedef enum {
  DIGEST_SHA1 = 0,
  DIGEST_SHA256 = 1,
} digest_algorithm_t;
#define  N_DIGEST_ALGORITHMS (DIGEST_SHA256+1)
#define digest_algorithm_bitfield_t ENUM_BF(digest_algorithm_t)

/** A set of all the digests we know how to compute, taken on a single
 * string.  Any digests that are shorter than 256 bits are right-padded
 * with 0 bits.
 *
 * Note that this representation wastes 12 bytes for the SHA1 case, so
 * don't use it for anything where we need to allocate a whole bunch at
 * once.
 **/
typedef struct {
  char d[N_DIGEST_ALGORITHMS][DIGEST256_LEN];
} digests_t;

typedef struct crypto_pk_t crypto_pk_t;
typedef struct crypto_cipher_t crypto_cipher_t;
typedef struct crypto_digest_t crypto_digest_t;
typedef struct crypto_dh_t crypto_dh_t;

/* global state */
const char * crypto_openssl_get_version_str(void);
const char * crypto_openssl_get_header_version_str(void);
int crypto_early_init(void);
int crypto_global_init(int hardwareAccel,
                       const char *accelName,
                       const char *accelPath);
void crypto_thread_cleanup(void);
int crypto_global_cleanup(void);

/* environment setup */
crypto_pk_t *crypto_pk_new(void);
void crypto_pk_free(crypto_pk_t *env);

void crypto_set_tls_dh_prime(const char *dynamic_dh_modulus_fname);

crypto_cipher_t *crypto_cipher_new(const char *key);
crypto_cipher_t *crypto_cipher_new_with_iv(const char *key, const char *iv);
void crypto_cipher_free(crypto_cipher_t *env);

/* public key crypto */
int crypto_pk_generate_key_with_bits(crypto_pk_t *env, int bits);
#define crypto_pk_generate_key(env)                     \
  crypto_pk_generate_key_with_bits((env), (PK_BYTES*8))

int crypto_pk_read_private_key_from_filename(crypto_pk_t *env,
                                             const char *keyfile);
int crypto_pk_write_public_key_to_string(crypto_pk_t *env,
                                         char **dest, size_t *len);
int crypto_pk_write_private_key_to_string(crypto_pk_t *env,
                                          char **dest, size_t *len);
int crypto_pk_read_public_key_from_string(crypto_pk_t *env,
                                          const char *src, size_t len);
int crypto_pk_read_private_key_from_string(crypto_pk_t *env,
                                           const char *s, ssize_t len);
int crypto_pk_write_private_key_to_filename(crypto_pk_t *env,
                                            const char *fname);

int crypto_pk_check_key(crypto_pk_t *env);
int crypto_pk_cmp_keys(crypto_pk_t *a, crypto_pk_t *b);
int crypto_pk_eq_keys(crypto_pk_t *a, crypto_pk_t *b);
size_t crypto_pk_keysize(crypto_pk_t *env);
int crypto_pk_num_bits(crypto_pk_t *env);
crypto_pk_t *crypto_pk_dup_key(crypto_pk_t *orig);
crypto_pk_t *crypto_pk_copy_full(crypto_pk_t *orig);
int crypto_pk_key_is_private(const crypto_pk_t *key);
int crypto_pk_public_exponent_ok(crypto_pk_t *env);

int crypto_pk_public_encrypt(crypto_pk_t *env, char *to, size_t tolen,
                             const char *from, size_t fromlen, int padding);
int crypto_pk_private_decrypt(crypto_pk_t *env, char *to, size_t tolen,
                              const char *from, size_t fromlen,
                              int padding, int warnOnFailure);
int crypto_pk_public_checksig(crypto_pk_t *env, char *to, size_t tolen,
                              const char *from, size_t fromlen);
int crypto_pk_public_checksig_digest(crypto_pk_t *env, const char *data,
                               size_t datalen, const char *sig, size_t siglen);
int crypto_pk_private_sign(crypto_pk_t *env, char *to, size_t tolen,
                           const char *from, size_t fromlen);
int crypto_pk_private_sign_digest(crypto_pk_t *env, char *to, size_t tolen,
                                  const char *from, size_t fromlen);
int crypto_pk_public_hybrid_encrypt(crypto_pk_t *env, char *to,
                                    size_t tolen,
                                    const char *from, size_t fromlen,
                                    int padding, int force);
int crypto_pk_private_hybrid_decrypt(crypto_pk_t *env, char *to,
                                     size_t tolen,
                                     const char *from, size_t fromlen,
                                     int padding, int warnOnFailure);

int crypto_pk_asn1_encode(crypto_pk_t *pk, char *dest, size_t dest_len);
crypto_pk_t *crypto_pk_asn1_decode(const char *str, size_t len);
int crypto_pk_get_digest(crypto_pk_t *pk, char *digest_out);
int crypto_pk_get_all_digests(crypto_pk_t *pk, digests_t *digests_out);
int crypto_pk_get_fingerprint(crypto_pk_t *pk, char *fp_out,int add_space);
int crypto_pk_get_hashed_fingerprint(crypto_pk_t *pk, char *fp_out);

/* symmetric crypto */
const char *crypto_cipher_get_key(crypto_cipher_t *env);

int crypto_cipher_encrypt(crypto_cipher_t *env, char *to,
                          const char *from, size_t fromlen);
int crypto_cipher_decrypt(crypto_cipher_t *env, char *to,
                          const char *from, size_t fromlen);
int crypto_cipher_crypt_inplace(crypto_cipher_t *env, char *d, size_t len);

int crypto_cipher_encrypt_with_iv(const char *key,
                                  char *to, size_t tolen,
                                  const char *from, size_t fromlen);
int crypto_cipher_decrypt_with_iv(const char *key,
                                  char *to, size_t tolen,
                                  const char *from, size_t fromlen);

/* SHA-1 and other digests. */
int crypto_digest(char *digest, const char *m, size_t len);
int crypto_digest256(char *digest, const char *m, size_t len,
                     digest_algorithm_t algorithm);
int crypto_digest_all(digests_t *ds_out, const char *m, size_t len);
struct smartlist_t;
void crypto_digest_smartlist(char *digest_out, size_t len_out,
                             const struct smartlist_t *lst, const char *append,
                             digest_algorithm_t alg);
const char *crypto_digest_algorithm_get_name(digest_algorithm_t alg);
int crypto_digest_algorithm_parse_name(const char *name);
crypto_digest_t *crypto_digest_new(void);
crypto_digest_t *crypto_digest256_new(digest_algorithm_t algorithm);
void crypto_digest_free(crypto_digest_t *digest);
void crypto_digest_add_bytes(crypto_digest_t *digest, const char *data,
                             size_t len);
void crypto_digest_get_digest(crypto_digest_t *digest,
                              char *out, size_t out_len);
crypto_digest_t *crypto_digest_dup(const crypto_digest_t *digest);
void crypto_digest_assign(crypto_digest_t *into,
                          const crypto_digest_t *from);
void crypto_hmac_sha256(char *hmac_out,
                        const char *key, size_t key_len,
                        const char *msg, size_t msg_len);

/* Key negotiation */
#define DH_TYPE_CIRCUIT 1
#define DH_TYPE_REND 2
#define DH_TYPE_TLS 3
crypto_dh_t *crypto_dh_new(int dh_type);
crypto_dh_t *crypto_dh_dup(const crypto_dh_t *dh);
int crypto_dh_get_bytes(crypto_dh_t *dh);
int crypto_dh_generate_public(crypto_dh_t *dh);
int crypto_dh_get_public(crypto_dh_t *dh, char *pubkey_out,
                         size_t pubkey_out_len);
ssize_t crypto_dh_compute_secret(int severity, crypto_dh_t *dh,
                             const char *pubkey, size_t pubkey_len,
                             char *secret_out, size_t secret_out_len);
void crypto_dh_free(crypto_dh_t *dh);

int crypto_expand_key_material_TAP(const uint8_t *key_in,
                                   size_t key_in_len,
                                   uint8_t *key_out, size_t key_out_len);
int crypto_expand_key_material_rfc5869_sha256(
                                    const uint8_t *key_in, size_t key_in_len,
                                    const uint8_t *salt_in, size_t salt_in_len,
                                    const uint8_t *info_in, size_t info_in_len,
                                    uint8_t *key_out, size_t key_out_len);

/* random numbers */
int crypto_seed_rng(int startup);
MOCK_DECL(int,crypto_rand,(char *to, size_t n));
int crypto_strongest_rand(uint8_t *out, size_t out_len);
int crypto_rand_int(unsigned int max);
uint64_t crypto_rand_uint64(uint64_t max);
double crypto_rand_double(void);
struct tor_weak_rng_t;
void crypto_seed_weak_rng(struct tor_weak_rng_t *rng);
int crypto_init_siphash_key(void);

char *crypto_random_hostname(int min_rand_len, int max_rand_len,
                             const char *prefix, const char *suffix);

struct smartlist_t;
void *smartlist_choose(const struct smartlist_t *sl);
void smartlist_shuffle(struct smartlist_t *sl);

int base64_encode(char *dest, size_t destlen, const char *src, size_t srclen);
int base64_decode(char *dest, size_t destlen, const char *src, size_t srclen);
/** Characters that can appear (case-insensitively) in a base32 encoding. */
#define BASE32_CHARS "abcdefghijklmnopqrstuvwxyz234567"
void base32_encode(char *dest, size_t destlen, const char *src, size_t srclen);
int base32_decode(char *dest, size_t destlen, const char *src, size_t srclen);

int digest_to_base64(char *d64, const char *digest);
int digest_from_base64(char *digest, const char *d64);
int digest256_to_base64(char *d64, const char *digest);
int digest256_from_base64(char *digest, const char *d64);

/** Length of RFC2440-style S2K specifier: the first 8 bytes are a salt, the
 * 9th describes how much iteration to do. */
#define S2K_SPECIFIER_LEN 9
void secret_to_key(char *key_out, size_t key_out_len, const char *secret,
                   size_t secret_len, const char *s2k_specifier);

/** OpenSSL-based utility functions. */
void memwipe(void *mem, uint8_t byte, size_t sz);

/* Prototypes for private functions only used by tortls.c, crypto.c, and the
 * unit tests. */
struct rsa_st;
struct evp_pkey_st;
struct dh_st;
struct rsa_st *crypto_pk_get_rsa_(crypto_pk_t *env);
crypto_pk_t *crypto_new_pk_from_rsa_(struct rsa_st *rsa);
struct evp_pkey_st *crypto_pk_get_evp_pkey_(crypto_pk_t *env,
                                                int private);
struct dh_st *crypto_dh_get_dh_(crypto_dh_t *dh);

void crypto_add_spaces_to_fp(char *out, size_t outlen, const char *in);

#endif

