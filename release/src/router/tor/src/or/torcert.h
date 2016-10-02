/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TORCERT_H_INCLUDED
#define TORCERT_H_INCLUDED

#include "crypto_ed25519.h"

#define SIGNED_KEY_TYPE_ED25519 0x01

#define CERT_TYPE_ID_SIGNING    0x04
#define CERT_TYPE_SIGNING_LINK  0x05
#define CERT_TYPE_SIGNING_AUTH  0x06
#define CERT_TYPE_ONION_ID      0x0A

#define CERT_FLAG_INCLUDE_SIGNING_KEY 0x1

/** An ed25519-signed certificate as used throughout the Tor protocol.
 **/
typedef struct tor_cert_st {
  /** The key authenticated by this certificate */
  ed25519_public_key_t signed_key;
  /** The key that signed this certificate. This value may be unset if the
   * certificate has never been checked, and didn't include its own key. */
  ed25519_public_key_t signing_key;
  /** A time after which this certificate will no longer be valid. */
  time_t valid_until;

  /** The encoded representation of this certificate */
  uint8_t *encoded;
  /** The length of <b>encoded</b> */
  size_t encoded_len;

  /** One of CERT_TYPE_... */
  uint8_t cert_type;
  /** True iff we received a signing key embedded in this certificate */
  unsigned signing_key_included : 1;
  /** True iff we checked the signature and found it bad */
  unsigned sig_bad : 1;
  /** True iff we checked the signature and found it correct */
  unsigned sig_ok : 1;
  /** True iff we checked the signature and first found that the cert
   * had expired */
  unsigned cert_expired : 1;
  /** True iff we checked the signature and found the whole cert valid */
  unsigned cert_valid : 1;
} tor_cert_t;

tor_cert_t *tor_cert_create(const ed25519_keypair_t *signing_key,
                            uint8_t cert_type,
                            const ed25519_public_key_t *signed_key,
                            time_t now, time_t lifetime,
                            uint32_t flags);

tor_cert_t *tor_cert_parse(const uint8_t *cert, size_t certlen);

void tor_cert_free(tor_cert_t *cert);

int tor_cert_get_checkable_sig(ed25519_checkable_t *checkable_out,
                                 const tor_cert_t *out,
                                 const ed25519_public_key_t *pubkey);

int tor_cert_checksig(tor_cert_t *cert,
                      const ed25519_public_key_t *pubkey, time_t now);

tor_cert_t *tor_cert_dup(const tor_cert_t *cert);
int tor_cert_eq(const tor_cert_t *cert1, const tor_cert_t *cert2);
int tor_cert_opt_eq(const tor_cert_t *cert1, const tor_cert_t *cert2);

ssize_t tor_make_rsa_ed25519_crosscert(const ed25519_public_key_t *ed_key,
                                       const crypto_pk_t *rsa_key,
                                       time_t expires,
                                       uint8_t **cert);

#endif

