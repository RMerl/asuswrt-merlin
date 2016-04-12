/* (Modified by Tor to generate detached signatures.) */
#include <string.h>
#include "crypto_sign.h"
#include "crypto_hash_sha512.h"
#include "ge.h"
#include "sc.h"

int crypto_sign(
  unsigned char *sig,
  const unsigned char *m, size_t mlen,
  const unsigned char *sk,const unsigned char *pk
)
{
  unsigned char nonce[64];
  unsigned char hram[64];
  ge_p3 R;

  crypto_hash_sha512_2(nonce, sk+32, 32, m, mlen);

  sc_reduce(nonce);
  ge_scalarmult_base(&R,nonce);
  ge_p3_tobytes(sig,&R);

  crypto_hash_sha512_3(hram, sig, 32, pk, 32, m, mlen);
  sc_reduce(hram);
  sc_muladd(sig + 32,hram,sk,nonce);

  return 0;
}
