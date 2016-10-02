/* Copyright (c) 2012-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CRYPTO_ED25519_H
#define TOR_CRYPTO_ED25519_H

#include "testsupport.h"
#include "torint.h"
#include "crypto_curve25519.h"

#define ED25519_PUBKEY_LEN 32
#define ED25519_SECKEY_LEN 64
#define ED25519_SECKEY_SEED_LEN 32
#define ED25519_SIG_LEN 64

/** An Ed25519 signature. */
typedef struct {
  uint8_t sig[ED25519_SIG_LEN];
} ed25519_signature_t;

/** An Ed25519 public key */
typedef struct {
  uint8_t pubkey[ED25519_PUBKEY_LEN];
} ed25519_public_key_t;

/** An Ed25519 secret key */
typedef struct {
  /** Note that we store secret keys in an expanded format that doesn't match
   * the format from standard ed25519.  Ed25519 stores a 32-byte value k and
   * expands it into a 64-byte H(k), using the first 32 bytes for a multiplier
   * of the base point, and second 32 bytes as an input to a hash function
   * for deriving r.  But because we implement key blinding, we need to store
   * keys in the 64-byte expanded form. */
  uint8_t seckey[ED25519_SECKEY_LEN];
} ed25519_secret_key_t;

/** An Ed25519 keypair. */
typedef struct {
  ed25519_public_key_t pubkey;
  ed25519_secret_key_t seckey;
} ed25519_keypair_t;

int ed25519_secret_key_generate(ed25519_secret_key_t *seckey_out,
                            int extra_strong);
int ed25519_secret_key_from_seed(ed25519_secret_key_t *seckey_out,
                                 const uint8_t *seed);

int ed25519_public_key_generate(ed25519_public_key_t *pubkey_out,
                            const ed25519_secret_key_t *seckey);
int ed25519_keypair_generate(ed25519_keypair_t *keypair_out, int extra_strong);
int ed25519_sign(ed25519_signature_t *signature_out,
                 const uint8_t *msg, size_t len,
                 const ed25519_keypair_t *key);
int ed25519_checksig(const ed25519_signature_t *signature,
                     const uint8_t *msg, size_t len,
                     const ed25519_public_key_t *pubkey);

/**
 * A collection of information necessary to check an Ed25519 signature. Used
 * for batch verification.
 */
typedef struct {
  /** The public key that supposedly generated the signature. */
  const ed25519_public_key_t *pubkey;
  /** The signature to check. */
  ed25519_signature_t signature;
  /** The message that the signature is supposed to have been applied to. */
  const uint8_t *msg;
  /** The length of the message. */
  size_t len;
} ed25519_checkable_t;

int ed25519_checksig_batch(int *okay_out,
                           const ed25519_checkable_t *checkable,
                           int n_checkable);

int ed25519_keypair_from_curve25519_keypair(ed25519_keypair_t *out,
                                            int *signbit_out,
                                            const curve25519_keypair_t *inp);

int ed25519_public_key_from_curve25519_public_key(ed25519_public_key_t *pubkey,
                                     const curve25519_public_key_t *pubkey_in,
                                     int signbit);
int ed25519_keypair_blind(ed25519_keypair_t *out,
                          const ed25519_keypair_t *inp,
                          const uint8_t *param);
int ed25519_public_blind(ed25519_public_key_t *out,
                         const ed25519_public_key_t *inp,
                         const uint8_t *param);

/* XXXX read encrypted, write encrypted. */

int ed25519_seckey_write_to_file(const ed25519_secret_key_t *seckey,
                                 const char *filename,
                                 const char *tag);
int ed25519_seckey_read_from_file(ed25519_secret_key_t *seckey_out,
                                  char **tag_out,
                                  const char *filename);
int ed25519_pubkey_write_to_file(const ed25519_public_key_t *pubkey,
                                 const char *filename,
                                 const char *tag);
int ed25519_pubkey_read_from_file(ed25519_public_key_t *pubkey_out,
                                  char **tag_out,
                                  const char *filename);

void ed25519_keypair_free(ed25519_keypair_t *kp);

int ed25519_pubkey_eq(const ed25519_public_key_t *key1,
                      const ed25519_public_key_t *key2);

void ed25519_set_impl_params(int use_donna);
void ed25519_init(void);

#ifdef TOR_UNIT_TESTS
void crypto_ed25519_testing_force_impl(const char *name);
void crypto_ed25519_testing_restore_impl(void);
#endif

#endif

