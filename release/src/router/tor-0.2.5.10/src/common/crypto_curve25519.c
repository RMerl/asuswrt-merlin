/* Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Wrapper code for a curve25519 implementation. */

#define CRYPTO_CURVE25519_PRIVATE
#include "orconfig.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "crypto.h"
#include "crypto_curve25519.h"
#include "util.h"
#include "torlog.h"

/* ==============================
   Part 1: wrap a suitable curve25519 implementation as curve25519_impl
   ============================== */

#ifdef USE_CURVE25519_DONNA
int curve25519_donna(uint8_t *mypublic,
                     const uint8_t *secret, const uint8_t *basepoint);
#endif
#ifdef USE_CURVE25519_NACL
#ifdef HAVE_CRYPTO_SCALARMULT_CURVE25519_H
#include <crypto_scalarmult_curve25519.h>
#elif defined(HAVE_NACL_CRYPTO_SCALARMULT_CURVE25519_H)
#include <nacl/crypto_scalarmult_curve25519.h>
#endif
#endif

STATIC int
curve25519_impl(uint8_t *output, const uint8_t *secret,
                const uint8_t *basepoint)
{
  uint8_t bp[CURVE25519_PUBKEY_LEN];
  int r;
  memcpy(bp, basepoint, CURVE25519_PUBKEY_LEN);
  /* Clear the high bit, in case our backend foolishly looks at it. */
  bp[31] &= 0x7f;
#ifdef USE_CURVE25519_DONNA
  r = curve25519_donna(output, secret, bp);
#elif defined(USE_CURVE25519_NACL)
  r = crypto_scalarmult_curve25519(output, secret, bp);
#else
#error "No implementation of curve25519 is available."
#endif
  memwipe(bp, 0, sizeof(bp));
  return r;
}

/* ==============================
   Part 2: Wrap curve25519_impl with some convenience types and functions.
   ============================== */

/**
 * Return true iff a curve25519_public_key_t seems valid. (It's not necessary
 * to see if the point is on the curve, since the twist is also secure, but we
 * do need to make sure that it isn't the point at infinity.) */
int
curve25519_public_key_is_ok(const curve25519_public_key_t *key)
{
  return !safe_mem_is_zero(key->public_key, CURVE25519_PUBKEY_LEN);
}

/** Generate a new keypair and return the secret key.  If <b>extra_strong</b>
 * is true, this key is possibly going to get used more than once, so
 * use a better-than-usual RNG. Return 0 on success, -1 on failure. */
int
curve25519_secret_key_generate(curve25519_secret_key_t *key_out,
                               int extra_strong)
{
  uint8_t k_tmp[CURVE25519_SECKEY_LEN];

  if (crypto_rand((char*)key_out->secret_key, CURVE25519_SECKEY_LEN) < 0)
    return -1;
  if (extra_strong && !crypto_strongest_rand(k_tmp, CURVE25519_SECKEY_LEN)) {
    /* If they asked for extra-strong entropy and we have some, use it as an
     * HMAC key to improve not-so-good entropy rather than using it directly,
     * just in case the extra-strong entropy is less amazing than we hoped. */
    crypto_hmac_sha256((char *)key_out->secret_key,
                    (const char *)k_tmp, sizeof(k_tmp),
                    (const char *)key_out->secret_key, CURVE25519_SECKEY_LEN);
  }
  memwipe(k_tmp, 0, sizeof(k_tmp));
  key_out->secret_key[0] &= 248;
  key_out->secret_key[31] &= 127;
  key_out->secret_key[31] |= 64;

  return 0;
}

void
curve25519_public_key_generate(curve25519_public_key_t *key_out,
                               const curve25519_secret_key_t *seckey)
{
  static const uint8_t basepoint[32] = {9};

  curve25519_impl(key_out->public_key, seckey->secret_key, basepoint);
}

int
curve25519_keypair_generate(curve25519_keypair_t *keypair_out,
                            int extra_strong)
{
  if (curve25519_secret_key_generate(&keypair_out->seckey, extra_strong) < 0)
    return -1;
  curve25519_public_key_generate(&keypair_out->pubkey, &keypair_out->seckey);
  return 0;
}

int
curve25519_keypair_write_to_file(const curve25519_keypair_t *keypair,
                                 const char *fname,
                                 const char *tag)
{
  char contents[32 + CURVE25519_SECKEY_LEN + CURVE25519_PUBKEY_LEN];
  int r;

  memset(contents, 0, sizeof(contents));
  tor_snprintf(contents, sizeof(contents), "== c25519v1: %s ==", tag);
  tor_assert(strlen(contents) <= 32);
  memcpy(contents+32, keypair->seckey.secret_key, CURVE25519_SECKEY_LEN);
  memcpy(contents+32+CURVE25519_SECKEY_LEN,
         keypair->pubkey.public_key, CURVE25519_PUBKEY_LEN);

  r = write_bytes_to_file(fname, contents, sizeof(contents), 1);

  memwipe(contents, 0, sizeof(contents));
  return r;
}

int
curve25519_keypair_read_from_file(curve25519_keypair_t *keypair_out,
                                  char **tag_out,
                                  const char *fname)
{
  char prefix[33];
  char *content;
  struct stat st;
  int r = -1;

  *tag_out = NULL;

  st.st_size = 0;
  content = read_file_to_str(fname, RFTS_BIN|RFTS_IGNORE_MISSING, &st);
  if (! content)
    goto end;
  if (st.st_size != 32 + CURVE25519_SECKEY_LEN + CURVE25519_PUBKEY_LEN)
    goto end;

  memcpy(prefix, content, 32);
  prefix[32] = '\0';
  if (strcmpstart(prefix, "== c25519v1: ") ||
      strcmpend(prefix, " =="))
    goto end;

  *tag_out = tor_strndup(prefix+strlen("== c25519v1: "),
                         strlen(prefix) - strlen("== c25519v1:  =="));

  memcpy(keypair_out->seckey.secret_key, content+32, CURVE25519_SECKEY_LEN);
  curve25519_public_key_generate(&keypair_out->pubkey, &keypair_out->seckey);
  if (tor_memneq(keypair_out->pubkey.public_key,
                 content + 32 + CURVE25519_SECKEY_LEN,
                 CURVE25519_PUBKEY_LEN))
    goto end;

  r = 0;

 end:
  if (content) {
    memwipe(content, 0, (size_t) st.st_size);
    tor_free(content);
  }
  if (r != 0) {
    memset(keypair_out, 0, sizeof(*keypair_out));
    tor_free(*tag_out);
  }
  return r;
}

/** Perform the curve25519 ECDH handshake with <b>skey</b> and <b>pkey</b>,
 * writing CURVE25519_OUTPUT_LEN bytes of output into <b>output</b>. */
void
curve25519_handshake(uint8_t *output,
                     const curve25519_secret_key_t *skey,
                     const curve25519_public_key_t *pkey)
{
  curve25519_impl(output, skey->secret_key, pkey->public_key);
}

