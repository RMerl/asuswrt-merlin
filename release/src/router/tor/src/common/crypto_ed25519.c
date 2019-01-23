/* Copyright (c) 2013-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file crypto_ed25519.c
 *
 * \brief Wrapper code for an ed25519 implementation.
 *
 * Ed25519 is a Schnorr signature on a Twisted Edwards curve, defined
 * by Dan Bernstein. For more information, see https://ed25519.cr.yp.to/
 *
 * This module wraps our choice of Ed25519 backend, and provides a few
 * convenience functions for checking and generating signatures.  It also
 * provides Tor-specific tools for key blinding and for converting Ed25519
 * keys to and from the corresponding Curve25519 keys.
 */

#include "orconfig.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "crypto.h"

#include "crypto_curve25519.h"
#include "crypto_ed25519.h"
#include "crypto_format.h"
#include "torlog.h"
#include "util.h"

#include "ed25519/ref10/ed25519_ref10.h"
#include "ed25519/donna/ed25519_donna_tor.h"

#include <openssl/sha.h>

static void pick_ed25519_impl(void);
static int ed25519_impl_spot_check(void);

/** An Ed25519 implementation, as a set of function pointers. */
typedef struct {
  int (*selftest)(void);

  int (*seckey)(unsigned char *);
  int (*seckey_expand)(unsigned char *, const unsigned char *);
  int (*pubkey)(unsigned char *, const unsigned char *);
  int (*keygen)(unsigned char *, unsigned char *);

  int (*open)(const unsigned char *, const unsigned char *, size_t, const
              unsigned char *);
  int (*sign)(unsigned char *, const unsigned char *, size_t,
              const unsigned char *, const unsigned char *);
  int (*open_batch)(const unsigned char **, size_t *, const unsigned char **,
                    const unsigned char **, size_t, int *);

  int (*blind_secret_key)(unsigned char *, const unsigned char *,
                          const unsigned char *);
  int (*blind_public_key)(unsigned char *, const unsigned char *,
                          const unsigned char *);

  int (*pubkey_from_curve25519_pubkey)(unsigned char *, const unsigned char *,
                                       int);
} ed25519_impl_t;

/** The Ref10 Ed25519 implementation. This one is pure C and lightly
 * optimized. */
static const ed25519_impl_t impl_ref10 = {
  NULL,

  ed25519_ref10_seckey,
  ed25519_ref10_seckey_expand,
  ed25519_ref10_pubkey,
  ed25519_ref10_keygen,

  ed25519_ref10_open,
  ed25519_ref10_sign,
  NULL,

  ed25519_ref10_blind_secret_key,
  ed25519_ref10_blind_public_key,

  ed25519_ref10_pubkey_from_curve25519_pubkey,
};

/** The Ref10 Ed25519 implementation. This one is heavily optimized, but still
 * mostly C. The C still tends to be heavily platform-specific. */
static const ed25519_impl_t impl_donna = {
  ed25519_donna_selftest,

  ed25519_donna_seckey,
  ed25519_donna_seckey_expand,
  ed25519_donna_pubkey,
  ed25519_donna_keygen,

  ed25519_donna_open,
  ed25519_donna_sign,
  ed25519_sign_open_batch_donna,

  ed25519_donna_blind_secret_key,
  ed25519_donna_blind_public_key,

  ed25519_donna_pubkey_from_curve25519_pubkey,
};

/** Which Ed25519 implementation are we using?  NULL if we haven't decided
 * yet. */
static const ed25519_impl_t *ed25519_impl = NULL;

/** Helper: Return our chosen Ed25519 implementation.
 *
 * This should only be called after we've picked an implementation, but
 * it _does_ recover if you forget this.
 **/
static inline const ed25519_impl_t *
get_ed_impl(void)
{
  if (BUG(ed25519_impl == NULL)) {
    pick_ed25519_impl(); // LCOV_EXCL_LINE - We always call ed25519_init().
  }
  return ed25519_impl;
}

#ifdef TOR_UNIT_TESTS
/** For testing: used to remember our actual choice of Ed25519
 * implementation */
static const ed25519_impl_t *saved_ed25519_impl = NULL;
/** For testing: Use the Ed25519 implementation called <b>name</b> until
 * crypto_ed25519_testing_restore_impl is called.  Recognized names are
 * "donna" and "ref10". */
void
crypto_ed25519_testing_force_impl(const char *name)
{
  tor_assert(saved_ed25519_impl == NULL);
  saved_ed25519_impl = ed25519_impl;
  if (! strcmp(name, "donna")) {
    ed25519_impl = &impl_donna;
  } else {
    tor_assert(!strcmp(name, "ref10"));
    ed25519_impl = &impl_ref10;
  }
}
/** For testing: go back to whatever Ed25519 implementation we had picked
 * before crypto_ed25519_testing_force_impl was called.
 */
void
crypto_ed25519_testing_restore_impl(void)
{
  ed25519_impl = saved_ed25519_impl;
  saved_ed25519_impl = NULL;
}
#endif

/**
 * Initialize a new ed25519 secret key in <b>seckey_out</b>.  If
 * <b>extra_strong</b>, take the RNG inputs directly from the operating
 * system.  Return 0 on success, -1 on failure.
 */
int
ed25519_secret_key_generate(ed25519_secret_key_t *seckey_out,
                        int extra_strong)
{
  int r;
  uint8_t seed[32];
  if (extra_strong)
    crypto_strongest_rand(seed, sizeof(seed));
 else
    crypto_rand((char*)seed, sizeof(seed));

  r = get_ed_impl()->seckey_expand(seckey_out->seckey, seed);
  memwipe(seed, 0, sizeof(seed));

  return r < 0 ? -1 : 0;
}

/**
 * Given a 32-byte random seed in <b>seed</b>, expand it into an ed25519
 * secret key in <b>seckey_out</b>.  Return 0 on success, -1 on failure.
 */
int
ed25519_secret_key_from_seed(ed25519_secret_key_t *seckey_out,
                             const uint8_t *seed)
{
  if (get_ed_impl()->seckey_expand(seckey_out->seckey, seed) < 0)
     return -1;
  return 0;
}

/**
 * Given a secret key in <b>seckey</b>, expand it into an
 * ed25519 public key.  Return 0 on success, -1 on failure.
 */
int
ed25519_public_key_generate(ed25519_public_key_t *pubkey_out,
                        const ed25519_secret_key_t *seckey)
{
  if (get_ed_impl()->pubkey(pubkey_out->pubkey, seckey->seckey) < 0)
    return -1;
  return 0;
}

/** Generate a new ed25519 keypair in <b>keypair_out</b>.  If
 * <b>extra_strong</b> is set, try to mix some system entropy into the key
 * generation process. Return 0 on success, -1 on failure. */
int
ed25519_keypair_generate(ed25519_keypair_t *keypair_out, int extra_strong)
{
  if (ed25519_secret_key_generate(&keypair_out->seckey, extra_strong) < 0)
    return -1;
  if (ed25519_public_key_generate(&keypair_out->pubkey,
                                  &keypair_out->seckey)<0)
    return -1;
  return 0;
}

/* Return a heap-allocated array that contains <b>msg</b> prefixed by the
 * string <b>prefix_str</b>. Set <b>final_msg_len_out</b> to the size of the
 * final array. If an error occured, return NULL. It's the resonsibility of the
 * caller to free the returned array. */
static uint8_t *
get_prefixed_msg(const uint8_t *msg, size_t msg_len,
                 const char *prefix_str,
                 size_t *final_msg_len_out)
{
  size_t prefixed_msg_len, prefix_len;
  uint8_t *prefixed_msg;

  tor_assert(prefix_str);
  tor_assert(final_msg_len_out);

  prefix_len = strlen(prefix_str);

  /* msg_len + strlen(prefix_str) must not overflow. */
  if (msg_len > SIZE_T_CEILING - prefix_len) {
    return NULL;
  }

  prefixed_msg_len = msg_len + prefix_len;
  prefixed_msg = tor_malloc_zero(prefixed_msg_len);

  memcpy(prefixed_msg, prefix_str, prefix_len);
  memcpy(prefixed_msg + prefix_len, msg, msg_len);

  *final_msg_len_out = prefixed_msg_len;
  return prefixed_msg;
}

/**
 * Set <b>signature_out</b> to a signature of the <b>len</b>-byte message
 * <b>msg</b>, using the secret and public key in <b>keypair</b>.
 *
 * Return 0 if we successfuly signed the message, otherwise return -1.
 */
int
ed25519_sign(ed25519_signature_t *signature_out,
             const uint8_t *msg, size_t len,
             const ed25519_keypair_t *keypair)
{
  if (get_ed_impl()->sign(signature_out->sig, msg, len,
                          keypair->seckey.seckey,
                          keypair->pubkey.pubkey) < 0) {
    return -1;
  }

  return 0;
}

/**
 * Like ed25519_sign(), but also prefix <b>msg</b> with <b>prefix_str</b>
 * before signing. <b>prefix_str</b> must be a NUL-terminated string.
 */
int
ed25519_sign_prefixed(ed25519_signature_t *signature_out,
                      const uint8_t *msg, size_t msg_len,
                      const char *prefix_str,
                      const ed25519_keypair_t *keypair)
{
  int retval;
  size_t prefixed_msg_len;
  uint8_t *prefixed_msg;

  tor_assert(prefix_str);

  prefixed_msg = get_prefixed_msg(msg, msg_len, prefix_str,
                                  &prefixed_msg_len);
  if (!prefixed_msg) {
    log_warn(LD_GENERAL, "Failed to get prefixed msg.");
    return -1;
  }

  retval = ed25519_sign(signature_out,
                        prefixed_msg, prefixed_msg_len,
                        keypair);
  tor_free(prefixed_msg);

  return retval;
}

/**
 * Check whether if <b>signature</b> is a valid signature for the
 * <b>len</b>-byte message in <b>msg</b> made with the key <b>pubkey</b>.
 *
 * Return 0 if the signature is valid; -1 if it isn't.
 */
int
ed25519_checksig(const ed25519_signature_t *signature,
                 const uint8_t *msg, size_t len,
                 const ed25519_public_key_t *pubkey)
{
  return
    get_ed_impl()->open(signature->sig, msg, len, pubkey->pubkey) < 0 ? -1 : 0;
}

/**
 * Like ed2519_checksig(), but also prefix <b>msg</b> with <b>prefix_str</b>
 * before verifying signature. <b>prefix_str</b> must be a NUL-terminated
 * string.
 */
int
ed25519_checksig_prefixed(const ed25519_signature_t *signature,
                          const uint8_t *msg, size_t msg_len,
                          const char *prefix_str,
                          const ed25519_public_key_t *pubkey)
{
  int retval;
  size_t prefixed_msg_len;
  uint8_t *prefixed_msg;

  prefixed_msg = get_prefixed_msg(msg, msg_len, prefix_str,
                                  &prefixed_msg_len);
  if (!prefixed_msg) {
    log_warn(LD_GENERAL, "Failed to get prefixed msg.");
    return -1;
  }

  retval = ed25519_checksig(signature,
                            prefixed_msg, prefixed_msg_len,
                            pubkey);
  tor_free(prefixed_msg);

  return retval;
}

/** Validate every signature among those in <b>checkable</b>, which contains
 * exactly <b>n_checkable</b> elements.  If <b>okay_out</b> is non-NULL, set
 * the i'th element of <b>okay_out</b> to 1 if the i'th element of
 * <b>checkable</b> is valid, and to 0 otherwise.  Return 0 if every signature
 * was valid. Otherwise return -N, where N is the number of invalid
 * signatures.
 */
int
ed25519_checksig_batch(int *okay_out,
                       const ed25519_checkable_t *checkable,
                       int n_checkable)
{
  int i, res;
  const ed25519_impl_t *impl = get_ed_impl();

  if (impl->open_batch == NULL) {
    /* No batch verification implementation available, fake it by checking the
     * each signature individually.
     */
    res = 0;
    for (i = 0; i < n_checkable; ++i) {
      const ed25519_checkable_t *ch = &checkable[i];
      int r = ed25519_checksig(&ch->signature, ch->msg, ch->len, ch->pubkey);
      if (r < 0)
        --res;
      if (okay_out)
        okay_out[i] = (r == 0);
    }
  } else {
    /* ed25519-donna style batch verification available.
     *
     * Theoretically, this should only be called if n_checkable >= 3, since
     * that's the threshold where the batch verification actually kicks in,
     * but the only difference is a few mallocs/frees.
     */
    const uint8_t **ms;
    size_t *lens;
    const uint8_t **pks;
    const uint8_t **sigs;
    int *oks;
    int all_ok;

    ms = tor_calloc(n_checkable, sizeof(uint8_t*));
    lens = tor_calloc(n_checkable, sizeof(size_t));
    pks = tor_calloc(n_checkable, sizeof(uint8_t*));
    sigs = tor_calloc(n_checkable, sizeof(uint8_t*));
    oks = okay_out ? okay_out : tor_calloc(n_checkable, sizeof(int));

    for (i = 0; i < n_checkable; ++i) {
      ms[i] = checkable[i].msg;
      lens[i] = checkable[i].len;
      pks[i] = checkable[i].pubkey->pubkey;
      sigs[i] = checkable[i].signature.sig;
      oks[i] = 0;
    }

    res = 0;
    all_ok = impl->open_batch(ms, lens, pks, sigs, n_checkable, oks);
    for (i = 0; i < n_checkable; ++i) {
      if (!oks[i])
        --res;
    }
    /* XXX: For now sanity check oks with the return value.  Once we have
     * more confidence in the code, if `all_ok == 0` we can skip iterating
     * over oks since all the signatures were found to be valid.
     */
    tor_assert(((res == 0) && !all_ok) || ((res < 0) && all_ok));

    tor_free(ms);
    tor_free(lens);
    tor_free(pks);
    tor_free(sigs);
    if (! okay_out)
      tor_free(oks);
  }

  return res;
}

/**
 * Given a curve25519 keypair in <b>inp</b>, generate a corresponding
 * ed25519 keypair in <b>out</b>, and set <b>signbit_out</b> to the
 * sign bit of the X coordinate of the ed25519 key.
 *
 * NOTE THAT IT IS PROBABLY NOT SAFE TO USE THE GENERATED KEY FOR ANYTHING
 * OUTSIDE OF WHAT'S PRESENTED IN PROPOSAL 228.  In particular, it's probably
 * not a great idea to use it to sign attacker-supplied anything.
 */
int
ed25519_keypair_from_curve25519_keypair(ed25519_keypair_t *out,
                                        int *signbit_out,
                                        const curve25519_keypair_t *inp)
{
  const char string[] = "Derive high part of ed25519 key from curve25519 key";
  ed25519_public_key_t pubkey_check;
  SHA512_CTX ctx;
  uint8_t sha512_output[64];

  memcpy(out->seckey.seckey, inp->seckey.secret_key, 32);
  SHA512_Init(&ctx);
  SHA512_Update(&ctx, out->seckey.seckey, 32);
  SHA512_Update(&ctx, string, sizeof(string));
  SHA512_Final(sha512_output, &ctx);
  memcpy(out->seckey.seckey + 32, sha512_output, 32);

  ed25519_public_key_generate(&out->pubkey, &out->seckey);

  *signbit_out = out->pubkey.pubkey[31] >> 7;

  ed25519_public_key_from_curve25519_public_key(&pubkey_check, &inp->pubkey,
                                                *signbit_out);

  tor_assert(fast_memeq(pubkey_check.pubkey, out->pubkey.pubkey, 32));

  memwipe(&pubkey_check, 0, sizeof(pubkey_check));
  memwipe(&ctx, 0, sizeof(ctx));
  memwipe(sha512_output, 0, sizeof(sha512_output));

  return 0;
}

/**
 * Given a curve25519 public key and sign bit of X coordinate of the ed25519
 * public key, generate the corresponding ed25519 public key.
 */
int
ed25519_public_key_from_curve25519_public_key(ed25519_public_key_t *pubkey,
                                     const curve25519_public_key_t *pubkey_in,
                                     int signbit)
{
  return get_ed_impl()->pubkey_from_curve25519_pubkey(pubkey->pubkey,
                                                      pubkey_in->public_key,
                                                      signbit);
}

/**
 * Given an ed25519 keypair in <b>inp</b>, generate a corresponding
 * ed25519 keypair in <b>out</b>, blinded by the corresponding 32-byte input
 * in 'param'.
 *
 * Tor uses key blinding for the "next-generation" hidden services design:
 * service descriptors are encrypted with a key derived from the service's
 * long-term public key, and then signed with (and stored at a position
 * indexed by) a short-term key derived by blinding the long-term keys.
 */
int
ed25519_keypair_blind(ed25519_keypair_t *out,
                      const ed25519_keypair_t *inp,
                      const uint8_t *param)
{
  ed25519_public_key_t pubkey_check;

  get_ed_impl()->blind_secret_key(out->seckey.seckey,
                                  inp->seckey.seckey, param);

  ed25519_public_blind(&pubkey_check, &inp->pubkey, param);
  ed25519_public_key_generate(&out->pubkey, &out->seckey);

  tor_assert(fast_memeq(pubkey_check.pubkey, out->pubkey.pubkey, 32));

  memwipe(&pubkey_check, 0, sizeof(pubkey_check));

  return 0;
}

/**
 * Given an ed25519 public key in <b>inp</b>, generate a corresponding blinded
 * public key in <b>out</b>, blinded with the 32-byte parameter in
 * <b>param</b>.  Return 0 on sucess, -1 on railure.
 */
int
ed25519_public_blind(ed25519_public_key_t *out,
                     const ed25519_public_key_t *inp,
                     const uint8_t *param)
{
  get_ed_impl()->blind_public_key(out->pubkey, inp->pubkey, param);
  return 0;
}

/**
 * Store seckey unencrypted to <b>filename</b>, marking it with <b>tag</b>.
 * Return 0 on success, -1 on failure.
 */
int
ed25519_seckey_write_to_file(const ed25519_secret_key_t *seckey,
                             const char *filename,
                             const char *tag)
{
  return crypto_write_tagged_contents_to_file(filename,
                                              "ed25519v1-secret",
                                              tag,
                                              seckey->seckey,
                                              sizeof(seckey->seckey));
}

/**
 * Read seckey unencrypted from <b>filename</b>, storing it into
 * <b>seckey_out</b>.  Set *<b>tag_out</b> to the tag it was marked with.
 * Return 0 on success, -1 on failure.
 */
int
ed25519_seckey_read_from_file(ed25519_secret_key_t *seckey_out,
                              char **tag_out,
                              const char *filename)
{
  ssize_t len;

  len = crypto_read_tagged_contents_from_file(filename, "ed25519v1-secret",
                                              tag_out, seckey_out->seckey,
                                              sizeof(seckey_out->seckey));
  if (len == sizeof(seckey_out->seckey)) {
    return 0;
  } else if (len >= 0) {
    errno = EINVAL;
  }

  tor_free(*tag_out);
  return -1;
}

/**
 * Store pubkey unencrypted to <b>filename</b>, marking it with <b>tag</b>.
 * Return 0 on success, -1 on failure.
 */
int
ed25519_pubkey_write_to_file(const ed25519_public_key_t *pubkey,
                             const char *filename,
                             const char *tag)
{
  return crypto_write_tagged_contents_to_file(filename,
                                              "ed25519v1-public",
                                              tag,
                                              pubkey->pubkey,
                                              sizeof(pubkey->pubkey));
}

/**
 * Store pubkey unencrypted to <b>filename</b>, marking it with <b>tag</b>.
 * Return 0 on success, -1 on failure.
 */
int
ed25519_pubkey_read_from_file(ed25519_public_key_t *pubkey_out,
                              char **tag_out,
                              const char *filename)
{
  ssize_t len;

  len = crypto_read_tagged_contents_from_file(filename, "ed25519v1-public",
                                              tag_out, pubkey_out->pubkey,
                                              sizeof(pubkey_out->pubkey));
  if (len == sizeof(pubkey_out->pubkey)) {
    return 0;
  } else if (len >= 0) {
    errno = EINVAL;
  }

  tor_free(*tag_out);
  return -1;
}

/** Release all storage held for <b>kp</b>. */
void
ed25519_keypair_free(ed25519_keypair_t *kp)
{
  if (! kp)
    return;

  memwipe(kp, 0, sizeof(*kp));
  tor_free(kp);
}

/** Return true iff <b>key1</b> and <b>key2</b> are the same public key. */
int
ed25519_pubkey_eq(const ed25519_public_key_t *key1,
                  const ed25519_public_key_t *key2)
{
  tor_assert(key1);
  tor_assert(key2);
  return tor_memeq(key1->pubkey, key2->pubkey, ED25519_PUBKEY_LEN);
}

/** Check whether the given Ed25519 implementation seems to be working.
 * If so, return 0; otherwise return -1. */
static int
ed25519_impl_spot_check(void)
{
  static const uint8_t alicesk[32] = {
    0xc5,0xaa,0x8d,0xf4,0x3f,0x9f,0x83,0x7b,
    0xed,0xb7,0x44,0x2f,0x31,0xdc,0xb7,0xb1,
    0x66,0xd3,0x85,0x35,0x07,0x6f,0x09,0x4b,
    0x85,0xce,0x3a,0x2e,0x0b,0x44,0x58,0xf7
  };
  static const uint8_t alicepk[32] = {
    0xfc,0x51,0xcd,0x8e,0x62,0x18,0xa1,0xa3,
    0x8d,0xa4,0x7e,0xd0,0x02,0x30,0xf0,0x58,
    0x08,0x16,0xed,0x13,0xba,0x33,0x03,0xac,
    0x5d,0xeb,0x91,0x15,0x48,0x90,0x80,0x25
  };
  static const uint8_t alicemsg[2] = { 0xaf, 0x82 };
  static const uint8_t alicesig[64] = {
    0x62,0x91,0xd6,0x57,0xde,0xec,0x24,0x02,
    0x48,0x27,0xe6,0x9c,0x3a,0xbe,0x01,0xa3,
    0x0c,0xe5,0x48,0xa2,0x84,0x74,0x3a,0x44,
    0x5e,0x36,0x80,0xd7,0xdb,0x5a,0xc3,0xac,
    0x18,0xff,0x9b,0x53,0x8d,0x16,0xf2,0x90,
    0xae,0x67,0xf7,0x60,0x98,0x4d,0xc6,0x59,
    0x4a,0x7c,0x15,0xe9,0x71,0x6e,0xd2,0x8d,
    0xc0,0x27,0xbe,0xce,0xea,0x1e,0xc4,0x0a
  };
  const ed25519_impl_t *impl = get_ed_impl();
  uint8_t sk[ED25519_SECKEY_LEN];
  uint8_t pk[ED25519_PUBKEY_LEN];
  uint8_t sig[ED25519_SIG_LEN];
  int r = 0;

  /* Some implementations (eg: The modified Ed25519-donna) have handy self-test
   * code that sanity-checks the internals.  If present, use that to screen out
   * catastrophic errors like massive compiler failure.
   */
  if (impl->selftest && impl->selftest() != 0)
    goto fail;

  /* Validate results versus known answer tests.  People really should be
   * running "make test" instead of relying on this, but it's better than
   * nothing.
   *
   * Test vectors taken from "EdDSA & Ed25519 - 6. Test Vectors for Ed25519
   * (TEST3)" (draft-josefsson-eddsa-ed25519-03).
   */

  /* Key expansion, public key derivation. */
  if (impl->seckey_expand(sk, alicesk) < 0)
    goto fail;
  if (impl->pubkey(pk, sk) < 0)
    goto fail;
  if (fast_memneq(pk, alicepk, ED25519_PUBKEY_LEN))
    goto fail;

  /* Signing, verification. */
  if (impl->sign(sig, alicemsg, sizeof(alicemsg), sk, pk) < 0)
    return -1;
  if (fast_memneq(sig, alicesig, ED25519_SIG_LEN))
    return -1;
  if (impl->open(sig, alicemsg, sizeof(alicemsg), pk) < 0)
    return -1;

  /* XXX/yawning: Someone that's more paranoid than I am, can write "Assume
   * ref0 is cannonical, and fuzz impl against it" if they want, but I doubt
   * that will catch anything that the known answer tests won't.
   */
  goto end;

 fail:
  r = -1;
 end:
  return r;
}

/** Force the Ed25519 implementation to a given one, without sanity checking
 * the output.  Used for testing.
 */
void
ed25519_set_impl_params(int use_donna)
{
  if (use_donna)
    ed25519_impl = &impl_donna;
  else
    ed25519_impl = &impl_ref10;
}

/** Choose whether to use the Ed25519-donna implementation. */
static void
pick_ed25519_impl(void)
{
  ed25519_impl = &impl_donna;

  if (ed25519_impl_spot_check() == 0)
    return;

  /* LCOV_EXCL_START
   * unreachable unless ed25519_donna is broken */
  log_warn(LD_CRYPTO, "The Ed25519-donna implementation seems broken; using "
           "the ref10 implementation.");
  ed25519_impl = &impl_ref10;
  /* LCOV_EXCL_STOP */
}

/* Initialize the Ed25519 implementation. This is neccessary if you're
 * going to use them in a multithreaded setting, and not otherwise. */
void
ed25519_init(void)
{
  pick_ed25519_impl();
}

