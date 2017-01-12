/* Copyright (c) 2012-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file crypto_curve25519.c
 *
 * \brief Wrapper code for a curve25519 implementation.
 *
 * Curve25519 is an Elliptic-Curve Diffie Hellman handshake, designed by
 * Dan Bernstein.  For more information, see https://cr.yp.to/ecdh.html
 *
 * Tor uses Curve25519 as the basis of its "ntor" circuit extension
 * handshake, and in related code.  The functions in this module are
 * used to find the most suitable available Curve25519 implementation,
 * to provide wrappers around it, and so on.
 */

#define CRYPTO_CURVE25519_PRIVATE
#include "orconfig.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "container.h"
#include "crypto.h"
#include "crypto_curve25519.h"
#include "crypto_format.h"
#include "util.h"
#include "torlog.h"

#include "ed25519/donna/ed25519_donna_tor.h"

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

static void pick_curve25519_basepoint_impl(void);

/** This is set to 1 if we have an optimized Ed25519-based
 * implementation for multiplying a value by the basepoint; to 0 if we
 * don't, and to -1 if we haven't checked. */
static int curve25519_use_ed = -1;

/**
 * Helper function: call the most appropriate backend to compute the
 * scalar "secret" times the point "point".  Store the result in
 * "output".  Return 0 on success, negative on failure.
 **/
STATIC int
curve25519_impl(uint8_t *output, const uint8_t *secret,
                const uint8_t *point)
{
  uint8_t bp[CURVE25519_PUBKEY_LEN];
  int r;
  memcpy(bp, point, CURVE25519_PUBKEY_LEN);
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

/**
 * Helper function: Multiply the scalar "secret" by the Curve25519
 * basepoint (X=9), and store the result in "output".  Return 0 on
 * success, -1 on false.
 */
STATIC int
curve25519_basepoint_impl(uint8_t *output, const uint8_t *secret)
{
  int r = 0;
  if (BUG(curve25519_use_ed == -1)) {
    /* LCOV_EXCL_START - Only reached if we forgot to call curve25519_init() */
    pick_curve25519_basepoint_impl();
    /* LCOV_EXCL_STOP */
  }

  /* TODO: Someone should benchmark curved25519_scalarmult_basepoint versus
   * an optimized NaCl build to see which should be used when compiled with
   * NaCl available.  I suspected that the ed25519 optimization always wins.
   */
  if (PREDICT_LIKELY(curve25519_use_ed == 1)) {
    curved25519_scalarmult_basepoint_donna(output, secret);
    r = 0;
  } else {
    static const uint8_t basepoint[32] = {9};
    r = curve25519_impl(output, secret, basepoint);
  }
  return r;
}

/**
 * Override the decision of whether to use the Ed25519-based basepoint
 * multiply function.  Used for testing.
 */
void
curve25519_set_impl_params(int use_ed)
{
  curve25519_use_ed = use_ed;
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

/**
 * Generate CURVE25519_SECKEY_LEN random bytes in <b>out</b>. If
 * <b>extra_strong</b> is true, this key is possibly going to get used more
 * than once, so use a better-than-usual RNG. Return 0 on success, -1 on
 * failure.
 *
 * This function does not adjust the output of the RNG at all; the will caller
 * will need to clear or set the appropriate bits to make curve25519 work.
 */
int
curve25519_rand_seckey_bytes(uint8_t *out, int extra_strong)
{
  if (extra_strong)
    crypto_strongest_rand(out, CURVE25519_SECKEY_LEN);
  else
    crypto_rand((char*)out, CURVE25519_SECKEY_LEN);

  return 0;
}

/** Generate a new keypair and return the secret key.  If <b>extra_strong</b>
 * is true, this key is possibly going to get used more than once, so
 * use a better-than-usual RNG. Return 0 on success, -1 on failure. */
int
curve25519_secret_key_generate(curve25519_secret_key_t *key_out,
                               int extra_strong)
{
  if (curve25519_rand_seckey_bytes(key_out->secret_key, extra_strong) < 0)
    return -1;

  key_out->secret_key[0] &= 248;
  key_out->secret_key[31] &= 127;
  key_out->secret_key[31] |= 64;

  return 0;
}

/**
 * Given a secret key in <b>seckey</b>, create the corresponding public
 * key in <b>key_out</b>.
 */
void
curve25519_public_key_generate(curve25519_public_key_t *key_out,
                               const curve25519_secret_key_t *seckey)
{
  curve25519_basepoint_impl(key_out->public_key, seckey->secret_key);
}

/**
 * Construct a new keypair in *<b>keypair_out</b>. If <b>extra_strong</b>
 * is true, this key is possibly going to get used more than once, so
 * use a better-than-usual RNG. Return 0 on success, -1 on failure. */
int
curve25519_keypair_generate(curve25519_keypair_t *keypair_out,
                            int extra_strong)
{
  if (curve25519_secret_key_generate(&keypair_out->seckey, extra_strong) < 0)
    return -1;
  curve25519_public_key_generate(&keypair_out->pubkey, &keypair_out->seckey);
  return 0;
}

/** Store the keypair <b>keypair</b>, including its secret and public
 * parts, to the file <b>fname</b>.  Use the string tag <b>tag</b> to
 * distinguish this from other Curve25519 keypairs. Return 0 on success,
 * -1 on failure.
 *
 * See crypto_write_tagged_contents_to_file() for more information on
 * the metaformat used for these keys.*/
int
curve25519_keypair_write_to_file(const curve25519_keypair_t *keypair,
                                 const char *fname,
                                 const char *tag)
{
  uint8_t contents[CURVE25519_SECKEY_LEN + CURVE25519_PUBKEY_LEN];
  int r;

  memcpy(contents, keypair->seckey.secret_key, CURVE25519_SECKEY_LEN);
  memcpy(contents+CURVE25519_SECKEY_LEN,
         keypair->pubkey.public_key, CURVE25519_PUBKEY_LEN);

  r = crypto_write_tagged_contents_to_file(fname,
                                           "c25519v1",
                                           tag,
                                           contents,
                                           sizeof(contents));

  memwipe(contents, 0, sizeof(contents));
  return r;
}

/** Read a curve25519 keypair from a file named <b>fname</b> created by
 * curve25519_keypair_write_to_file(). Store the keypair in
 * <b>keypair_out</b>, and the associated tag string in <b>tag_out</b>.
 * Return 0 on success, and -1 on failure. */
int
curve25519_keypair_read_from_file(curve25519_keypair_t *keypair_out,
                                  char **tag_out,
                                  const char *fname)
{
  uint8_t content[CURVE25519_SECKEY_LEN + CURVE25519_PUBKEY_LEN];
  ssize_t len;
  int r = -1;

  len = crypto_read_tagged_contents_from_file(fname, "c25519v1", tag_out,
                                              content, sizeof(content));
  if (len != sizeof(content))
    goto end;

  /* Make sure that the public key matches the secret key */
  memcpy(keypair_out->seckey.secret_key, content, CURVE25519_SECKEY_LEN);
  curve25519_public_key_generate(&keypair_out->pubkey, &keypair_out->seckey);
  if (tor_memneq(keypair_out->pubkey.public_key,
                 content + CURVE25519_SECKEY_LEN,
                 CURVE25519_PUBKEY_LEN))
    goto end;

  r = 0;

 end:
  memwipe(content, 0, sizeof(content));
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

/** Check whether the ed25519-based curve25519 basepoint optimization seems to
 * be working. If so, return 0; otherwise return -1. */
static int
curve25519_basepoint_spot_check(void)
{
  static const uint8_t alicesk[32] = {
    0x77,0x07,0x6d,0x0a,0x73,0x18,0xa5,0x7d,
    0x3c,0x16,0xc1,0x72,0x51,0xb2,0x66,0x45,
    0xdf,0x4c,0x2f,0x87,0xeb,0xc0,0x99,0x2a,
    0xb1,0x77,0xfb,0xa5,0x1d,0xb9,0x2c,0x2a
  };
  static const uint8_t alicepk[32] = {
    0x85,0x20,0xf0,0x09,0x89,0x30,0xa7,0x54,
    0x74,0x8b,0x7d,0xdc,0xb4,0x3e,0xf7,0x5a,
    0x0d,0xbf,0x3a,0x0d,0x26,0x38,0x1a,0xf4,
    0xeb,0xa4,0xa9,0x8e,0xaa,0x9b,0x4e,0x6a
  };
  const int loop_max=200;
  int save_use_ed = curve25519_use_ed;
  unsigned char e1[32] = { 5 };
  unsigned char e2[32] = { 5 };
  unsigned char x[32],y[32];
  int i;
  int r=0;

  /* Check the most basic possible sanity via the test secret/public key pair
   * used in "Cryptography in NaCl - 2. Secret keys and public keys".  This
   * may catch catastrophic failures on systems where Curve25519 is expensive,
   * without requiring a ton of key generation.
   */
  curve25519_use_ed = 1;
  r |= curve25519_basepoint_impl(x, alicesk);
  if (fast_memneq(x, alicepk, 32))
    goto fail;

  /* Ok, the optimization appears to produce passable results, try a few more
   * values, maybe there's something subtle wrong.
   */
  for (i = 0; i < loop_max; ++i) {
    curve25519_use_ed = 0;
    r |= curve25519_basepoint_impl(x, e1);
    curve25519_use_ed = 1;
    r |= curve25519_basepoint_impl(y, e2);
    if (fast_memneq(x,y,32))
      goto fail;
    memcpy(e1, x, 32);
    memcpy(e2, x, 32);
  }

  goto end;
 fail:
  r = -1;
 end:
  curve25519_use_ed = save_use_ed;
  return r;
}

/** Choose whether to use the ed25519-based curve25519-basepoint
 * implementation. */
static void
pick_curve25519_basepoint_impl(void)
{
  curve25519_use_ed = 1;

  if (curve25519_basepoint_spot_check() == 0)
    return;

  /* LCOV_EXCL_START
   * only reachable if our basepoint implementation broken */
  log_warn(LD_BUG|LD_CRYPTO, "The ed25519-based curve25519 basepoint "
           "multiplication seems broken; using the curve25519 "
           "implementation.");
  curve25519_use_ed = 0;
  /* LCOV_EXCL_STOP */
}

/** Initialize the curve25519 implementations. This is necessary if you're
 * going to use them in a multithreaded setting, and not otherwise. */
void
curve25519_init(void)
{
  pick_curve25519_basepoint_impl();
}

