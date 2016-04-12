/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CRYPTO_S2K_PRIVATE

#include "crypto.h"
#include "util.h"
#include "compat.h"
#include "crypto_s2k.h"

#include <openssl/evp.h>

#ifdef HAVE_LIBSCRYPT_H
#define HAVE_SCRYPT
#include <libscrypt.h>
#endif

/* Encoded secrets take the form:

     u8 type;
     u8 salt_and_parameters[depends on type];
     u8 key[depends on type];

   As a special case, if the encoded secret is exactly 29 bytes long,
   type 0 is understood.

   Recognized types are:
       00 -- RFC2440. salt_and_parameters is 9 bytes. key is 20 bytes.
                salt_and_parameters is 8 bytes random salt,
                1 byte iteration info.
       01 -- PKBDF2_SHA1. salt_and_parameters is 17 bytes. key is 20 bytes.
                salt_and_parameters is 16 bytes random salt,
                1 byte iteration info.
       02 -- SCRYPT_SALSA208_SHA256. salt_and_parameters is 18 bytes. key is
             32 bytes.
                salt_and_parameters is 18 bytes random salt, 2 bytes iteration
                info.
*/

#define S2K_TYPE_RFC2440 0
#define S2K_TYPE_PBKDF2  1
#define S2K_TYPE_SCRYPT  2

#define PBKDF2_SPEC_LEN 17
#define PBKDF2_KEY_LEN 20

#define SCRYPT_SPEC_LEN 18
#define SCRYPT_KEY_LEN 32

/** Given an algorithm ID (one of S2K_TYPE_*), return the length of the
 * specifier part of it, without the prefix type byte. */
static int
secret_to_key_spec_len(uint8_t type)
{
  switch (type) {
    case S2K_TYPE_RFC2440:
      return S2K_RFC2440_SPECIFIER_LEN;
    case S2K_TYPE_PBKDF2:
      return PBKDF2_SPEC_LEN;
    case S2K_TYPE_SCRYPT:
      return SCRYPT_SPEC_LEN;
    default:
      return -1;
  }
}

/** Given an algorithm ID (one of S2K_TYPE_*), return the length of the
 * its preferred output. */
static int
secret_to_key_key_len(uint8_t type)
{
  switch (type) {
    case S2K_TYPE_RFC2440:
      return DIGEST_LEN;
    case S2K_TYPE_PBKDF2:
      return DIGEST_LEN;
    case S2K_TYPE_SCRYPT:
      return DIGEST256_LEN;
    default:
      return -1;
  }
}

/** Given a specifier in <b>spec_and_key</b> of length
 * <b>spec_and_key_len</b>, along with its prefix algorithm ID byte, and along
 * with a key if <b>key_included</b> is true, check whether the whole
 * specifier-and-key is of valid length, and return the algorithm type if it
 * is.  Set *<b>legacy_out</b> to 1 iff this is a legacy password hash or
 * legacy specifier.  Return an error code on failure.
 */
static int
secret_to_key_get_type(const uint8_t *spec_and_key, size_t spec_and_key_len,
                       int key_included, int *legacy_out)
{
  size_t legacy_len = S2K_RFC2440_SPECIFIER_LEN;
  uint8_t type;
  int total_len;

  if (key_included)
    legacy_len += DIGEST_LEN;

  if (spec_and_key_len == legacy_len) {
    *legacy_out = 1;
    return S2K_TYPE_RFC2440;
  }

  *legacy_out = 0;
  if (spec_and_key_len == 0)
    return S2K_BAD_LEN;

  type = spec_and_key[0];
  total_len = secret_to_key_spec_len(type);
  if (total_len < 0)
    return S2K_BAD_ALGORITHM;
  if (key_included) {
    int keylen = secret_to_key_key_len(type);
    if (keylen < 0)
      return S2K_BAD_ALGORITHM;
    total_len += keylen;
  }

  if ((size_t)total_len + 1 == spec_and_key_len)
    return type;
  else
    return S2K_BAD_LEN;
}

/**
 * Write a new random s2k specifier of type <b>type</b>, without prefixing
 * type byte, to <b>spec_out</b>, which must have enough room.  May adjust
 * parameter choice based on <b>flags</b>.
 */
static int
make_specifier(uint8_t *spec_out, uint8_t type, unsigned flags)
{
  int speclen = secret_to_key_spec_len(type);
  if (speclen < 0)
      return S2K_BAD_ALGORITHM;

  crypto_rand((char*)spec_out, speclen);
  switch (type) {
    case S2K_TYPE_RFC2440:
      /* Hash 64 k of data. */
      spec_out[S2K_RFC2440_SPECIFIER_LEN-1] = 96;
      break;
    case S2K_TYPE_PBKDF2:
      /* 131 K iterations */
      spec_out[PBKDF2_SPEC_LEN-1] = 17;
      break;
    case S2K_TYPE_SCRYPT:
      if (flags & S2K_FLAG_LOW_MEM) {
        /* N = 1<<12 */
        spec_out[SCRYPT_SPEC_LEN-2] = 12;
      } else {
        /* N = 1<<15 */
        spec_out[SCRYPT_SPEC_LEN-2] = 15;
      }
      /* r = 8; p = 2. */
      spec_out[SCRYPT_SPEC_LEN-1] = (3u << 4) | (1u << 0);
      break;
    default:
      tor_fragile_assert();
      return S2K_BAD_ALGORITHM;
  }

  return speclen;
}

/** Implement RFC2440-style iterated-salted S2K conversion: convert the
 * <b>secret_len</b>-byte <b>secret</b> into a <b>key_out_len</b> byte
 * <b>key_out</b>.  As in RFC2440, the first 8 bytes of s2k_specifier
 * are a salt; the 9th byte describes how much iteration to do.
 * If <b>key_out_len</b> &gt; DIGEST_LEN, use HDKF to expand the result.
 */
void
secret_to_key_rfc2440(char *key_out, size_t key_out_len, const char *secret,
              size_t secret_len, const char *s2k_specifier)
{
  crypto_digest_t *d;
  uint8_t c;
  size_t count, tmplen;
  char *tmp;
  uint8_t buf[DIGEST_LEN];
  tor_assert(key_out_len < SIZE_T_CEILING);

#define EXPBIAS 6
  c = s2k_specifier[8];
  count = ((uint32_t)16 + (c & 15)) << ((c >> 4) + EXPBIAS);
#undef EXPBIAS

  d = crypto_digest_new();
  tmplen = 8+secret_len;
  tmp = tor_malloc(tmplen);
  memcpy(tmp,s2k_specifier,8);
  memcpy(tmp+8,secret,secret_len);
  secret_len += 8;
  while (count) {
    if (count >= secret_len) {
      crypto_digest_add_bytes(d, tmp, secret_len);
      count -= secret_len;
    } else {
      crypto_digest_add_bytes(d, tmp, count);
      count = 0;
    }
  }
  crypto_digest_get_digest(d, (char*)buf, sizeof(buf));

  if (key_out_len <= sizeof(buf)) {
    memcpy(key_out, buf, key_out_len);
  } else {
    crypto_expand_key_material_rfc5869_sha256(buf, DIGEST_LEN,
                                           (const uint8_t*)s2k_specifier, 8,
                                           (const uint8_t*)"EXPAND", 6,
                                           (uint8_t*)key_out, key_out_len);
  }
  memwipe(tmp, 0, tmplen);
  memwipe(buf, 0, sizeof(buf));
  tor_free(tmp);
  crypto_digest_free(d);
}

/**
 * Helper: given a valid specifier without prefix type byte in <b>spec</b>,
 * whose length must be correct, and given a secret passphrase <b>secret</b>
 * of length <b>secret_len</b>, compute the key and store it into
 * <b>key_out</b>, which must have enough room for secret_to_key_key_len(type)
 * bytes.  Return the number of bytes written on success and an error code
 * on failure.
 */
STATIC int
secret_to_key_compute_key(uint8_t *key_out, size_t key_out_len,
                          const uint8_t *spec, size_t spec_len,
                          const char *secret, size_t secret_len,
                          int type)
{
  int rv;
  if (key_out_len > INT_MAX)
    return S2K_BAD_LEN;

  switch (type) {
    case S2K_TYPE_RFC2440:
      secret_to_key_rfc2440((char*)key_out, key_out_len, secret, secret_len,
                            (const char*)spec);
      return (int)key_out_len;

    case S2K_TYPE_PBKDF2: {
      uint8_t log_iters;
      if (spec_len < 1 || secret_len > INT_MAX || spec_len > INT_MAX)
        return S2K_BAD_LEN;
      log_iters = spec[spec_len-1];
      if (log_iters > 31)
        return S2K_BAD_PARAMS;
      rv = PKCS5_PBKDF2_HMAC_SHA1(secret, (int)secret_len,
                                  spec, (int)spec_len-1,
                                  (1<<log_iters),
                                  (int)key_out_len, key_out);
      if (rv < 0)
        return S2K_FAILED;
      return (int)key_out_len;
    }

    case S2K_TYPE_SCRYPT: {
#ifdef HAVE_SCRYPT
      uint8_t log_N, log_r, log_p;
      uint64_t N;
      uint32_t r, p;
      if (spec_len < 2)
        return S2K_BAD_LEN;
      log_N = spec[spec_len-2];
      log_r = (spec[spec_len-1]) >> 4;
      log_p = (spec[spec_len-1]) & 15;
      if (log_N > 63)
        return S2K_BAD_PARAMS;
      N = ((uint64_t)1) << log_N;
      r = 1u << log_r;
      p = 1u << log_p;
      rv = libscrypt_scrypt((const uint8_t*)secret, secret_len,
                            spec, spec_len-2, N, r, p, key_out, key_out_len);
      if (rv != 0)
        return S2K_FAILED;
      return (int)key_out_len;
#else
      return S2K_NO_SCRYPT_SUPPORT;
#endif
    }
    default:
      return S2K_BAD_ALGORITHM;
  }
}

/**
 * Given a specifier previously constructed with secret_to_key_make_specifier
 * in <b>spec</b> of length <b>spec_len</b>, and a secret password in
 * <b>secret</b> of length <b>secret_len</b>, generate <b>key_out_len</b>
 * bytes of cryptographic material in <b>key_out</b>.  The native output of
 * the secret-to-key function will be truncated if key_out_len is short, and
 * expanded with HKDF if key_out_len is long.  Returns S2K_OKAY on success,
 * and an error code on failure.
 */
int
secret_to_key_derivekey(uint8_t *key_out, size_t key_out_len,
                        const uint8_t *spec, size_t spec_len,
                        const char *secret, size_t secret_len)
{
  int legacy_format = 0;
  int type = secret_to_key_get_type(spec, spec_len, 0, &legacy_format);
  int r;

  if (type < 0)
    return type;
#ifndef HAVE_SCRYPT
  if (type == S2K_TYPE_SCRYPT)
    return S2K_NO_SCRYPT_SUPPORT;
 #endif

  if (! legacy_format) {
    ++spec;
    --spec_len;
  }

  r = secret_to_key_compute_key(key_out, key_out_len, spec, spec_len,
                                secret, secret_len, type);
  if (r < 0)
    return r;
  else
    return S2K_OKAY;
}

/**
 * Construct a new s2k algorithm specifier and salt in <b>buf</b>, according
 * to the bitwise-or of some S2K_FLAG_* options in <b>flags</b>.  Up to
 * <b>buf_len</b> bytes of storage may be used in <b>buf</b>.  Return the
 * number of bytes used on success and an error code on failure.
 */
int
secret_to_key_make_specifier(uint8_t *buf, size_t buf_len, unsigned flags)
{
  int rv;
  int spec_len;
#ifdef HAVE_SCRYPT
  uint8_t type = S2K_TYPE_SCRYPT;
#else
  uint8_t type = S2K_TYPE_RFC2440;
#endif

  if (flags & S2K_FLAG_NO_SCRYPT)
    type = S2K_TYPE_RFC2440;
  if (flags & S2K_FLAG_USE_PBKDF2)
    type = S2K_TYPE_PBKDF2;

  spec_len = secret_to_key_spec_len(type);

  if ((int)buf_len < spec_len + 1)
    return S2K_TRUNCATED;

  buf[0] = type;
  rv = make_specifier(buf+1, type, flags);
  if (rv < 0)
    return rv;
  else
    return rv + 1;
}

/**
 * Hash a passphrase from <b>secret</b> of length <b>secret_len</b>, according
 * to the bitwise-or of some S2K_FLAG_* options in <b>flags</b>, and store the
 * hash along with salt and hashing parameters into <b>buf</b>.  Up to
 * <b>buf_len</b> bytes of storage may be used in <b>buf</b>.  Set
 * *<b>len_out</b> to the number of bytes used and return S2K_OKAY on success;
 * and return an error code on failure.
 */
int
secret_to_key_new(uint8_t *buf,
                  size_t buf_len,
                  size_t *len_out,
                  const char *secret, size_t secret_len,
                  unsigned flags)
{
  int key_len;
  int spec_len;
  int type;
  int rv;

  spec_len = secret_to_key_make_specifier(buf, buf_len, flags);

  if (spec_len < 0)
    return spec_len;

  type = buf[0];
  key_len = secret_to_key_key_len(type);

  if (key_len < 0)
    return key_len;

  if ((int)buf_len < key_len + spec_len)
    return S2K_TRUNCATED;

  rv = secret_to_key_compute_key(buf + spec_len, key_len,
                                 buf + 1, spec_len-1,
                                 secret, secret_len, type);
  if (rv < 0)
    return rv;

  *len_out = spec_len + key_len;

  return S2K_OKAY;
}

/**
 * Given a hashed passphrase in <b>spec_and_key</b> of length
 * <b>spec_and_key_len</b> as generated by secret_to_key_new(), verify whether
 * it is a hash of the passphrase <b>secret</b> of length <b>secret_len</b>.
 * Return S2K_OKAY on a match, S2K_BAD_SECRET on a well-formed hash that
 * doesn't match this secret, and another error code on other errors.
 */
int
secret_to_key_check(const uint8_t *spec_and_key, size_t spec_and_key_len,
                    const char *secret, size_t secret_len)
{
  int is_legacy = 0;
  int type = secret_to_key_get_type(spec_and_key, spec_and_key_len,
                                    1, &is_legacy);
  uint8_t buf[32];
  int spec_len;
  int key_len;
  int rv;

  if (type < 0)
    return type;

  if (! is_legacy) {
    spec_and_key++;
    spec_and_key_len--;
  }

  spec_len = secret_to_key_spec_len(type);
  key_len = secret_to_key_key_len(type);
  tor_assert(spec_len > 0);
  tor_assert(key_len > 0);
  tor_assert(key_len <= (int) sizeof(buf));
  tor_assert((int)spec_and_key_len == spec_len + key_len);
  rv = secret_to_key_compute_key(buf, key_len,
                                 spec_and_key, spec_len,
                                 secret, secret_len, type);
  if (rv < 0)
    goto done;

  if (tor_memeq(buf, spec_and_key + spec_len, key_len))
    rv = S2K_OKAY;
  else
    rv = S2K_BAD_SECRET;

 done:
  memwipe(buf, 0, sizeof(buf));
  return rv;
}

