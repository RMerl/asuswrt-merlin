/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file torcert.c
 *
 * \brief Implementation for ed25519-signed certificates as used in the Tor
 * protocol.
 */

#include "crypto.h"
#include "torcert.h"
#include "ed25519_cert.h"
#include "torlog.h"
#include "util.h"
#include "compat.h"
#include "link_handshake.h"

/** Helper for tor_cert_create(): signs any 32 bytes, not just an ed25519
 * key.
 */
static tor_cert_t *
tor_cert_sign_impl(const ed25519_keypair_t *signing_key,
                      uint8_t cert_type,
                      uint8_t signed_key_type,
                      const uint8_t signed_key_info[32],
                      time_t now, time_t lifetime,
                      uint32_t flags)
{
  tor_cert_t *torcert = NULL;

  ed25519_cert_t *cert = ed25519_cert_new();
  cert->cert_type = cert_type;
  cert->exp_field = (uint32_t) CEIL_DIV(now + lifetime, 3600);
  cert->cert_key_type = signed_key_type;
  memcpy(cert->certified_key, signed_key_info, 32);

  if (flags & CERT_FLAG_INCLUDE_SIGNING_KEY) {
    ed25519_cert_extension_t *ext = ed25519_cert_extension_new();
    ext->ext_type = CERTEXT_SIGNED_WITH_KEY;
    memcpy(ext->un_signing_key, signing_key->pubkey.pubkey, 32);
    ed25519_cert_add_ext(cert, ext);
    ++cert->n_extensions;
  }

  const ssize_t alloc_len = ed25519_cert_encoded_len(cert);
  tor_assert(alloc_len > 0);
  uint8_t *encoded = tor_malloc(alloc_len);
  const ssize_t real_len = ed25519_cert_encode(encoded, alloc_len, cert);
  if (real_len < 0)
    goto err;
  tor_assert(real_len == alloc_len);
  tor_assert(real_len > ED25519_SIG_LEN);
  uint8_t *sig = encoded + (real_len - ED25519_SIG_LEN);
  tor_assert(tor_mem_is_zero((char*)sig, ED25519_SIG_LEN));

  ed25519_signature_t signature;
  if (ed25519_sign(&signature, encoded,
                   real_len-ED25519_SIG_LEN, signing_key)<0) {
    log_warn(LD_BUG, "Can't sign certificate");
    goto err;
  }
  memcpy(sig, signature.sig, ED25519_SIG_LEN);

  torcert = tor_cert_parse(encoded, real_len);
  if (! torcert) {
    log_warn(LD_BUG, "Generated a certificate we cannot parse");
    goto err;
  }

  if (tor_cert_checksig(torcert, &signing_key->pubkey, now) < 0) {
    log_warn(LD_BUG, "Generated a certificate whose signature we can't check");
    goto err;
  }

  tor_free(encoded);

  goto done;

 err:
  tor_cert_free(torcert);
  torcert = NULL;
 done:
  ed25519_cert_free(cert);
  tor_free(encoded);
  return torcert;
}

/**
 * Create and return a new new certificate of type <b>cert_type</b> to
 * authenticate <b>signed_key</b> using the key <b>signing_key</b>.  The
 * certificate should remain valid for at least <b>lifetime</b> seconds after
 * <b>now</b>.
 *
 * If CERT_FLAG_INCLUDE_SIGNING_KEY is set in <b>flags</b>, embed
 * the public part of <b>signing_key</b> in the certificate.
 */
tor_cert_t *
tor_cert_create(const ed25519_keypair_t *signing_key,
                uint8_t cert_type,
                const ed25519_public_key_t *signed_key,
                time_t now, time_t lifetime,
                uint32_t flags)
{
  return tor_cert_sign_impl(signing_key, cert_type,
                            SIGNED_KEY_TYPE_ED25519, signed_key->pubkey,
                            now, lifetime, flags);
}

/** Release all storage held for <b>cert</b>. */
void
tor_cert_free(tor_cert_t *cert)
{
  if (! cert)
    return;

  if (cert->encoded)
    memwipe(cert->encoded, 0, cert->encoded_len);
  tor_free(cert->encoded);

  memwipe(cert, 0, sizeof(tor_cert_t));
  tor_free(cert);
}

/** Parse a certificate encoded with <b>len</b> bytes in <b>encoded</b>. */
tor_cert_t *
tor_cert_parse(const uint8_t *encoded, const size_t len)
{
  tor_cert_t *cert = NULL;
  ed25519_cert_t *parsed = NULL;
  ssize_t got_len = ed25519_cert_parse(&parsed, encoded, len);
  if (got_len < 0 || (size_t) got_len != len)
    goto err;

  cert = tor_malloc_zero(sizeof(tor_cert_t));
  cert->encoded = tor_memdup(encoded, len);
  cert->encoded_len = len;

  memcpy(cert->signed_key.pubkey, parsed->certified_key, 32);
  cert->valid_until = parsed->exp_field * 3600;
  cert->cert_type = parsed->cert_type;

  for (unsigned i = 0; i < ed25519_cert_getlen_ext(parsed); ++i) {
    ed25519_cert_extension_t *ext = ed25519_cert_get_ext(parsed, i);
    if (ext->ext_type == CERTEXT_SIGNED_WITH_KEY) {
      if (cert->signing_key_included)
        goto err;

      cert->signing_key_included = 1;
      memcpy(cert->signing_key.pubkey, ext->un_signing_key, 32);
    } else if (ext->ext_flags & CERTEXT_FLAG_AFFECTS_VALIDATION) {
      /* Unrecognized extension with affects_validation set */
      goto err;
    }
  }

  goto done;
 err:
  tor_cert_free(cert);
  cert = NULL;
 done:
  ed25519_cert_free(parsed);
  return cert;
}

/** Fill in <b>checkable_out</b> with the information needed to check
 * the signature on <b>cert</b> with <b>pubkey</b>. */
int
tor_cert_get_checkable_sig(ed25519_checkable_t *checkable_out,
                           const tor_cert_t *cert,
                           const ed25519_public_key_t *pubkey)
{
  if (! pubkey) {
    if (cert->signing_key_included)
      pubkey = &cert->signing_key;
    else
      return -1;
  }

  checkable_out->msg = cert->encoded;
  checkable_out->pubkey = pubkey;
  tor_assert(cert->encoded_len > ED25519_SIG_LEN);
  const size_t signed_len = cert->encoded_len - ED25519_SIG_LEN;
  checkable_out->len = signed_len;
  memcpy(checkable_out->signature.sig,
         cert->encoded + signed_len, ED25519_SIG_LEN);

  return 0;
}

/** Validates the signature on <b>cert</b> with <b>pubkey</b> relative to the
 * current time <b>now</b>.  (If <b>now</b> is 0, do not check the expiration
 * time.) Return 0 on success, -1 on failure.  Sets flags in <b>cert</b> as
 * appropriate.
 */
int
tor_cert_checksig(tor_cert_t *cert,
                  const ed25519_public_key_t *pubkey, time_t now)
{
  ed25519_checkable_t checkable;
  int okay;

  if (now && now > cert->valid_until) {
    cert->cert_expired = 1;
    return -1;
  }

  if (tor_cert_get_checkable_sig(&checkable, cert, pubkey) < 0)
    return -1;

  if (ed25519_checksig_batch(&okay, &checkable, 1) < 0) {
    cert->sig_bad = 1;
    return -1;
  } else {
    cert->sig_ok = 1;
    /* Only copy the checkable public key when it is different from the signing
     * key of the certificate to avoid undefined behavior. */
    if (cert->signing_key.pubkey != checkable.pubkey->pubkey) {
      memcpy(cert->signing_key.pubkey, checkable.pubkey->pubkey, 32);
    }
    cert->cert_valid = 1;
    return 0;
  }
}

/** Return a new copy of <b>cert</b> */
tor_cert_t *
tor_cert_dup(const tor_cert_t *cert)
{
  tor_cert_t *newcert = tor_memdup(cert, sizeof(tor_cert_t));
  if (cert->encoded)
    newcert->encoded = tor_memdup(cert->encoded, cert->encoded_len);
  return newcert;
}

/** Return true iff cert1 and cert2 are the same cert. */
int
tor_cert_eq(const tor_cert_t *cert1, const tor_cert_t *cert2)
{
  tor_assert(cert1);
  tor_assert(cert2);
  return cert1->encoded_len == cert2->encoded_len &&
    tor_memeq(cert1->encoded, cert2->encoded, cert1->encoded_len);
}

/** Return true iff cert1 and cert2 are the same cert, or if they are both
 * NULL. */
int
tor_cert_opt_eq(const tor_cert_t *cert1, const tor_cert_t *cert2)
{
  if (cert1 == NULL && cert2 == NULL)
    return 1;
  if (!cert1 || !cert2)
    return 0;
  return tor_cert_eq(cert1, cert2);
}

/** Create new cross-certification object to certify <b>ed_key</b> as the
 * master ed25519 identity key for the RSA identity key <b>rsa_key</b>.
 * Allocates and stores the encoded certificate in *<b>cert</b>, and returns
 * the number of bytes stored. Returns negative on error.*/
ssize_t
tor_make_rsa_ed25519_crosscert(const ed25519_public_key_t *ed_key,
                               const crypto_pk_t *rsa_key,
                               time_t expires,
                               uint8_t **cert)
{
  uint8_t *res;

  rsa_ed_crosscert_t *cc = rsa_ed_crosscert_new();
  memcpy(cc->ed_key, ed_key->pubkey, ED25519_PUBKEY_LEN);
  cc->expiration = (uint32_t) CEIL_DIV(expires, 3600);
  cc->sig_len = crypto_pk_keysize(rsa_key);
  rsa_ed_crosscert_setlen_sig(cc, crypto_pk_keysize(rsa_key));

  ssize_t alloc_sz = rsa_ed_crosscert_encoded_len(cc);
  tor_assert(alloc_sz > 0);
  res = tor_malloc_zero(alloc_sz);
  ssize_t sz = rsa_ed_crosscert_encode(res, alloc_sz, cc);
  tor_assert(sz > 0 && sz <= alloc_sz);

  const int signed_part_len = 32 + 4;
  int siglen = crypto_pk_private_sign(rsa_key,
                                      (char*)rsa_ed_crosscert_getarray_sig(cc),
                                      rsa_ed_crosscert_getlen_sig(cc),
                                      (char*)res, signed_part_len);
  tor_assert(siglen > 0 && siglen <= (int)crypto_pk_keysize(rsa_key));
  tor_assert(siglen <= UINT8_MAX);
  cc->sig_len = siglen;
  rsa_ed_crosscert_setlen_sig(cc, siglen);

  sz = rsa_ed_crosscert_encode(res, alloc_sz, cc);
  rsa_ed_crosscert_free(cc);
  *cert = res;
  return sz;
}

