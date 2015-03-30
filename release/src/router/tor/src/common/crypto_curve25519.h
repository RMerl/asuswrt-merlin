/* Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CRYPTO_CURVE25519_H
#define TOR_CRYPTO_CURVE25519_H

#include "testsupport.h"
#include "torint.h"

/** Length of a curve25519 public key when encoded. */
#define CURVE25519_PUBKEY_LEN 32
/** Length of a curve25519 secret key when encoded. */
#define CURVE25519_SECKEY_LEN 32
/** Length of the result of a curve25519 handshake. */
#define CURVE25519_OUTPUT_LEN 32

/** Wrapper type for a curve25519 public key */
typedef struct curve25519_public_key_t {
  uint8_t public_key[CURVE25519_PUBKEY_LEN];
} curve25519_public_key_t;

/** Wrapper type for a curve25519 secret key */
typedef struct curve25519_secret_key_t {
  uint8_t secret_key[CURVE25519_SECKEY_LEN];
} curve25519_secret_key_t;

/** A paired public and private key for curve25519. **/
typedef struct curve25519_keypair_t {
  curve25519_public_key_t pubkey;
  curve25519_secret_key_t seckey;
} curve25519_keypair_t;

#ifdef CURVE25519_ENABLED
/* These functions require that we actually know how to use curve25519 keys.
 * The other data structures and functions in this header let us parse them,
 * store them, and move them around.
 */

int curve25519_public_key_is_ok(const curve25519_public_key_t *);

int curve25519_secret_key_generate(curve25519_secret_key_t *key_out,
                                   int extra_strong);
void curve25519_public_key_generate(curve25519_public_key_t *key_out,
                                    const curve25519_secret_key_t *seckey);
int curve25519_keypair_generate(curve25519_keypair_t *keypair_out,
                                int extra_strong);

void curve25519_handshake(uint8_t *output,
                          const curve25519_secret_key_t *,
                          const curve25519_public_key_t *);

int curve25519_keypair_write_to_file(const curve25519_keypair_t *keypair,
                                     const char *fname,
                                     const char *tag);

int curve25519_keypair_read_from_file(curve25519_keypair_t *keypair_out,
                                      char **tag_out,
                                      const char *fname);

#ifdef CRYPTO_CURVE25519_PRIVATE
STATIC int curve25519_impl(uint8_t *output, const uint8_t *secret,
                           const uint8_t *basepoint);
#endif
#endif

#define CURVE25519_BASE64_PADDED_LEN 44

int curve25519_public_from_base64(curve25519_public_key_t *pkey,
                                  const char *input);
int curve25519_public_to_base64(char *output,
                                const curve25519_public_key_t *pkey);

#endif

