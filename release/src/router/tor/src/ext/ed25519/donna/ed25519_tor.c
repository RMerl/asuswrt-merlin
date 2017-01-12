/*
  Public domain by Andrew M. <liquidsun@gmail.com>

  Ed25519 reference implementation using Ed25519-donna
*/

/*
  Tor specific notes:

  This file is used by Tor instead of `ed25519.c` as the number of
  changes/additions is non-trivial.

  Tor modifications to `ed25519.c`:
   * 'Tab` -> '  '.
   * Include `ed25519_donna_tor.h` instead of `ed25519.h`.

   * The external interface has been reworked to match that provided
     by Tor's copy of the SUPERCOP `ref10` code.

   * The secret (aka private) key is now stored/used in expanded form.

   * The internal math tests from `test-internals.c` have been wrapped
     in a function and the entire file is included to allow for
     runtime validation.
 */


/* define ED25519_SUFFIX to have it appended to the end of each public function */
#if !defined(ED25519_SUFFIX)
#define ED25519_SUFFIX
#endif

#define ED25519_FN3(fn,suffix) fn##suffix
#define ED25519_FN2(fn,suffix) ED25519_FN3(fn,suffix)
#define ED25519_FN(fn)         ED25519_FN2(fn,ED25519_SUFFIX)

#include "orconfig.h"
#include "ed25519-donna.h"
#include "ed25519_donna_tor.h"
#include "ed25519-randombytes.h"
#include "ed25519-hash.h"

typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];

static void ed25519_donna_gettweak(unsigned char *out,
                                   const unsigned char *param);

static int ED25519_FN(ed25519_sign_open) (const unsigned char *m, size_t mlen,
  const ed25519_public_key pk, const ed25519_signature RS);


/*
  Generates a (extsk[0..31]) and aExt (extsk[32..63])
*/

DONNA_INLINE static void
ed25519_extsk(hash_512bits extsk, const ed25519_secret_key sk) {
  ed25519_hash(extsk, sk, 32);
  extsk[0] &= 248;
  extsk[31] &= 127;
  extsk[31] |= 64;
}

static void
ed25519_hram(hash_512bits hram, const ed25519_signature RS, const ed25519_public_key pk, const unsigned char *m, size_t mlen) {
  ed25519_hash_context ctx;
  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, RS, 32);
  ed25519_hash_update(&ctx, pk, 32);
  ed25519_hash_update(&ctx, m, mlen);
  ed25519_hash_final(&ctx, hram);
}

static int
ED25519_FN(ed25519_sign_open) (const unsigned char *m, size_t mlen, const ed25519_public_key pk, const ed25519_signature RS) {
  ge25519 ALIGN(16) R, A;
  hash_512bits hash;
  bignum256modm hram, S;
  unsigned char checkR[32];

  if ((RS[63] & 224) || !ge25519_unpack_negative_vartime(&A, pk))
    return -1;

  /* hram = H(R,A,m) */
  ed25519_hram(hash, RS, pk, m, mlen);
  expand256_modm(hram, hash, 64);

  /* S */
  expand256_modm(S, RS + 32, 32);

  /* SB - H(R,A,m)A */
  ge25519_double_scalarmult_vartime(&R, &A, hram, S);
  ge25519_pack(checkR, &R);

  /* check that R = SB - H(R,A,m)A */
  return ed25519_verify(RS, checkR, 32) ? 0 : -1;
}

#include "ed25519-donna-batchverify.h"

/*
  Fast Curve25519 basepoint scalar multiplication
*/

void
ED25519_FN(curved25519_scalarmult_basepoint) (curved25519_key pk, const curved25519_key e) {
  curved25519_key ec;
  bignum256modm s;
  bignum25519 ALIGN(16) yplusz, zminusy;
  ge25519 ALIGN(16) p;
  size_t i;

  /* clamp */
  for (i = 0; i < 32; i++) ec[i] = e[i];
  ec[0] &= 248;
  ec[31] &= 127;
  ec[31] |= 64;

  expand_raw256_modm(s, ec);

  /* scalar * basepoint */
  ge25519_scalarmult_base_niels(&p, ge25519_niels_base_multiples, s);

  /* u = (y + z) / (z - y) */
  curve25519_add(yplusz, p.y, p.z);
  curve25519_sub(zminusy, p.z, p.y);
  curve25519_recip(zminusy, zminusy);
  curve25519_mul(yplusz, yplusz, zminusy);
  curve25519_contract(pk, yplusz);
}

/*
   Tor has a specific idea of how an Ed25519 implementaion should behave.
   Implement such a beast using the ed25519-donna primitives/internals.

    * Private key generation using Tor's CSPRNG.

    * Routines that deal with the private key now use the expanded form.

    * Support for multiplicative key blinding has been added.

    * Support for converting a Curve25519 key to an Ed25519 key has been added.
 */

int
ed25519_donna_seckey(unsigned char *sk)
{
  ed25519_secret_key seed;

  crypto_strongest_rand(seed, 32);

  ed25519_extsk(sk, seed);

  memwipe(seed, 0, sizeof(seed));

  return 0;
}

int
ed25519_donna_seckey_expand(unsigned char *sk, const unsigned char *skseed)
{
  ed25519_extsk(sk, skseed);

  return 0;
}

int
ed25519_donna_pubkey(unsigned char *pk, const unsigned char *sk)
{
  bignum256modm a = {0};
  ge25519 ALIGN(16) A = {{0}, {0}, {0}, {0}};

  /* A = aB */
  expand256_modm(a, sk, 32);
  ge25519_scalarmult_base_niels(&A, ge25519_niels_base_multiples, a);
  ge25519_pack(pk, &A);

  return 0;
}

int
ed25519_donna_keygen(unsigned char *pk, unsigned char *sk)
{
  int ok;
  ok = ed25519_donna_seckey(sk);
  ed25519_donna_pubkey(pk, sk);

  return ok;
}

int
ed25519_donna_open(const unsigned char *signature, const unsigned char *m,
  size_t mlen, const unsigned char *pk)
{
  /* Wrap the ed25519-donna routine, since it is also used by the batch
   * verification code.
   */
  return ED25519_FN(ed25519_sign_open)(m, mlen, pk, signature);
}

int
ed25519_donna_sign(unsigned char *sig, const unsigned char *m, size_t mlen,
  const unsigned char *sk, const unsigned char *pk)
{
  ed25519_hash_context ctx;
  bignum256modm r = {0}, S, a;
  ge25519 ALIGN(16) R = {{0}, {0}, {0}, {0}};
  hash_512bits hashr, hram;

  /* This is equivalent to the removed `ED25519_FN(ed25519_sign)` routine,
   * except that the key expansion step is omitted as sk already is in expanded
   * form.
   */

  /* r = H(aExt[32..64], m) */
  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, sk + 32, 32);
  ed25519_hash_update(&ctx, m, mlen);
  ed25519_hash_final(&ctx, hashr);
  expand256_modm(r, hashr, 64);

  /* R = rB */
  ge25519_scalarmult_base_niels(&R, ge25519_niels_base_multiples, r);
  ge25519_pack(sig, &R);

  /* S = H(R,A,m).. */
  ed25519_hram(hram, sig, pk, m, mlen);
  expand256_modm(S, hram, 64);

  /* S = H(R,A,m)a */
  expand256_modm(a, sk, 32);
  mul256_modm(S, S, a);

  /* S = (r + H(R,A,m)a) */
  add256_modm(S, S, r);

  /* S = (r + H(R,A,m)a) mod L */
  contract256_modm(sig + 32, S);

  return 0;
}

static void
ed25519_donna_gettweak(unsigned char *out, const unsigned char *param)
{
  static const char str[] = "Derive temporary signing key";
  ed25519_hash_context ctx;

  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, (const unsigned char*)str, strlen(str));
  ed25519_hash_update(&ctx, param, 32);
  ed25519_hash_final(&ctx, out);

  out[0] &= 248;  /* Is this necessary ? */
  out[31] &= 63;
  out[31] |= 64;
}

int
ed25519_donna_blind_secret_key(unsigned char *out, const unsigned char *inp,
  const unsigned char *param)
{
  static const char str[] = "Derive temporary signing key hash input";
  unsigned char tweak[64];
  ed25519_hash_context ctx;
  bignum256modm ALIGN(16) sk, t;

  ed25519_donna_gettweak(tweak, param);
  expand256_modm(t, tweak, 32);

  expand256_modm(sk, inp, 32);
  mul256_modm(sk, sk, t);
  contract256_modm(out, sk);

  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, (const unsigned char*)str, strlen(str));
  ed25519_hash_update(&ctx, inp + 32, 32);
  ed25519_hash_final(&ctx, tweak);

  memcpy(out + 32, tweak, 32);

  memwipe(sk, 0, sizeof(sk));
  memwipe(t, 0, sizeof(t));
  memwipe(tweak, 0, sizeof(tweak));

  return 0;
}

int
ed25519_donna_blind_public_key(unsigned char *out, const unsigned char *inp,
  const unsigned char *param)
{
  static const bignum256modm zero = { 0 };
  unsigned char tweak[64];
  unsigned char pkcopy[32];
  ge25519 ALIGN(16) A, Aprime;
  bignum256modm ALIGN(16) t;

  ed25519_donna_gettweak(tweak, param);
  expand256_modm(t, tweak, 32);

  /* No "ge25519_unpack", negate the public key. */
  memcpy(pkcopy, inp, 32);
  pkcopy[31] ^= (1<<7);
  ge25519_unpack_negative_vartime(&A, pkcopy);

  /* A' = [tweak] * A + [0] * basepoint. */
  ge25519_double_scalarmult_vartime(&Aprime, &A, t, zero);
  ge25519_pack(out, &Aprime);

  memwipe(tweak, 0, sizeof(tweak));
  memwipe(pkcopy, 0, sizeof(pkcopy));
  memwipe(&A, 0, sizeof(A));
  memwipe(&Aprime, 0, sizeof(Aprime));
  memwipe(t, 0, sizeof(t));

  return 0;
}

int
ed25519_donna_pubkey_from_curve25519_pubkey(unsigned char *out,
  const unsigned char *inp, int signbit)
{
  static const bignum25519 ALIGN(16) one = { 1 };
  bignum25519 ALIGN(16) u, uminus1, uplus1, inv_uplus1, y;

  /* Prop228: y = (u-1)/(u+1) */
  curve25519_expand(u, inp);
  curve25519_sub(uminus1, u, one);
  curve25519_add(uplus1, u, one);
  curve25519_recip(inv_uplus1, uplus1);
  curve25519_mul(y, uminus1, inv_uplus1);
  curve25519_contract(out, y);

  /* Propagate sign. */
  out[31] |= (!!signbit) << 7;

  return 0;
}

#include "test-internals.c"

