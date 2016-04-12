/* Added to ref10 for Tor. We place this in the public domain.  Alternatively,
 * you may have it under the Creative Commons 0 "CC0" license. */
//#include "fe.h"
#include "ge.h"
#include "sc.h"
#include "crypto_hash_sha512.h"
#include "ed25519_ref10.h"

#include <string.h>
#include "crypto.h"

static void
gettweak(unsigned char *out, const unsigned char *param)
{
  const char str[] = "Derive temporary signing key";
  crypto_hash_sha512_2(out, (const unsigned char*)str, strlen(str), param, 32);
  out[0] &= 248;  /* Is this necessary necessary ? */
  out[31] &= 63;
  out[31] |= 64;
}

int ed25519_ref10_blind_secret_key(unsigned char *out,
                              const unsigned char *inp,
                              const unsigned char *param)
{
  const char str[] = "Derive temporary signing key hash input";
  unsigned char tweak[64];
  unsigned char zero[32];
  gettweak(tweak, param);

  memset(zero, 0, 32);
  sc_muladd(out, inp, tweak, zero);

  crypto_hash_sha512_2(tweak, (const unsigned char *)str, strlen(str),
                       inp+32, 32);
  memcpy(out+32, tweak, 32);

  memwipe(tweak, 0, sizeof(tweak));

  return 0;
}

int ed25519_ref10_blind_public_key(unsigned char *out,
                              const unsigned char *inp,
                              const unsigned char *param)
{
  unsigned char tweak[64];
  unsigned char zero[32];
  unsigned char pkcopy[32];
  ge_p3 A;
  ge_p2 Aprime;

  gettweak(tweak, param);

  memset(zero, 0, sizeof(zero));
  /* Not the greatest implementation of all of this.  I wish I had
   * better-suited primitives to work with here... (but I don't wish that so
   * strongly that I'm about to code my own ge_scalarmult_vartime). */

  /* We negate the public key first, so that we can pass it to
   * frombytes_negate_vartime, which negates it again. If there were a
   * "ge_frombytes", we'd use that, but there isn't. */
  memcpy(pkcopy, inp, 32);
  pkcopy[31] ^= (1<<7);
  ge_frombytes_negate_vartime(&A, pkcopy);
  /* There isn't a regular ge_scalarmult -- we have to do tweak*A + zero*B. */
  ge_double_scalarmult_vartime(&Aprime, tweak, &A, zero);
  ge_tobytes(out, &Aprime);

  memwipe(tweak, 0, sizeof(tweak));
  memwipe(&A, 0, sizeof(A));
  memwipe(&Aprime, 0, sizeof(Aprime));
  memwipe(pkcopy, 0, sizeof(pkcopy));

  return 0;
}
