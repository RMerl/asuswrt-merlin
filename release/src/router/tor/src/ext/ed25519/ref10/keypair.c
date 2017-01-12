/* Modified for Tor: new API, 64-byte secret keys. */

#include "randombytes.h"
#include <string.h>
#include "crypto_sign.h"
#include "crypto_hash_sha512.h"
#include "ge.h"

int
crypto_sign_seckey(unsigned char *sk)
{
  unsigned char seed[32];

  if (randombytes(seed,32) < 0)
    return -1;

  crypto_sign_seckey_expand(sk, seed);

  memwipe(seed, 0, 32);

  return 0;
}

int crypto_sign_seckey_expand(unsigned char *sk, const unsigned char *skseed)
{
  crypto_hash_sha512(sk,skseed,32);
  sk[0] &= 248;
  sk[31] &= 63;
  sk[31] |= 64;

  return 0;
}

int crypto_sign_pubkey(unsigned char *pk,const unsigned char *sk)
{
  ge_p3 A;

  ge_scalarmult_base(&A,sk);
  ge_p3_tobytes(pk,&A);

  return 0;
}


int crypto_sign_keypair(unsigned char *pk,unsigned char *sk)
{
  crypto_sign_seckey(sk);
  crypto_sign_pubkey(pk, sk);

  return 0;
}

