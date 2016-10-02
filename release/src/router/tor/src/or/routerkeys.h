/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_ROUTERKEYS_H
#define TOR_ROUTERKEYS_H

#include "crypto_ed25519.h"

#define INIT_ED_KEY_CREATE                      (1u<<0)
#define INIT_ED_KEY_REPLACE                     (1u<<1)
#define INIT_ED_KEY_SPLIT                       (1u<<2)
#define INIT_ED_KEY_MISSING_SECRET_OK           (1u<<3)
#define INIT_ED_KEY_NEEDCERT                    (1u<<4)
#define INIT_ED_KEY_EXTRA_STRONG                (1u<<5)
#define INIT_ED_KEY_INCLUDE_SIGNING_KEY_IN_CERT (1u<<6)
#define INIT_ED_KEY_OMIT_SECRET                 (1u<<7)
#define INIT_ED_KEY_TRY_ENCRYPTED               (1u<<8)
#define INIT_ED_KEY_NO_REPAIR                   (1u<<9)
#define INIT_ED_KEY_SUGGEST_KEYGEN              (1u<<10)
#define INIT_ED_KEY_OFFLINE_SECRET              (1u<<11)
#define INIT_ED_KEY_EXPLICIT_FNAME              (1u<<12)

struct tor_cert_st;
ed25519_keypair_t *ed_key_init_from_file(const char *fname, uint32_t flags,
                                         int severity,
                                         const ed25519_keypair_t *signing_key,
                                         time_t now,
                                         time_t lifetime,
                                         uint8_t cert_type,
                                         struct tor_cert_st **cert_out);
ed25519_keypair_t *ed_key_new(const ed25519_keypair_t *signing_key,
                              uint32_t flags,
                              time_t now,
                              time_t lifetime,
                              uint8_t cert_type,
                              struct tor_cert_st **cert_out);
const ed25519_public_key_t *get_master_identity_key(void);
const ed25519_keypair_t *get_master_signing_keypair(void);
const struct tor_cert_st *get_master_signing_key_cert(void);

const ed25519_keypair_t *get_current_auth_keypair(void);
const struct tor_cert_st *get_current_link_cert_cert(void);
const struct tor_cert_st *get_current_auth_key_cert(void);

void get_master_rsa_crosscert(const uint8_t **cert_out,
                              size_t *size_out);

struct tor_cert_st *make_ntor_onion_key_crosscert(
                                  const curve25519_keypair_t *onion_key,
                                  const ed25519_public_key_t *master_id_key,
                                  time_t now, time_t lifetime,
                                  int *sign_out);
uint8_t *make_tap_onion_key_crosscert(const crypto_pk_t *onion_key,
                                  const ed25519_public_key_t *master_id_key,
                                  const crypto_pk_t *rsa_id_key,
                                  int *len_out);

int check_tap_onion_key_crosscert(const uint8_t *crosscert,
                                  int crosscert_len,
                                  const crypto_pk_t *onion_pkey,
                                  const ed25519_public_key_t *master_id_pkey,
                                  const uint8_t *rsa_id_digest);

int load_ed_keys(const or_options_t *options, time_t now);
int should_make_new_ed_keys(const or_options_t *options, const time_t now);

int generate_ed_link_cert(const or_options_t *options, time_t now);

int read_encrypted_secret_key(ed25519_secret_key_t *out,
                              const char *fname);
int write_encrypted_secret_key(const ed25519_secret_key_t *out,
                               const char *fname);

void routerkeys_free_all(void);

#endif

