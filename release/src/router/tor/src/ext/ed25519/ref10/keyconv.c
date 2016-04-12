/* Added to ref10 for Tor. We place this in the public domain.  Alternatively,
 * you may have it under the Creative Commons 0 "CC0" license. */
#include "fe.h"
#include "ed25519_ref10.h"

int ed25519_ref10_pubkey_from_curve25519_pubkey(unsigned char *out,
                                                const unsigned char *inp,
                                                int signbit)
{
  fe u;
  fe one;
  fe y;
  fe uplus1;
  fe uminus1;
  fe inv_uplus1;

  /* From prop228:

   Given a curve25519 x-coordinate (u), we can get the y coordinate
   of the ed25519 key using

         y = (u-1)/(u+1)
  */
  fe_frombytes(u, inp);
  fe_1(one);
  fe_sub(uminus1, u, one);
  fe_add(uplus1, u, one);
  fe_invert(inv_uplus1, uplus1);
  fe_mul(y, uminus1, inv_uplus1);

  fe_tobytes(out, y);

  /* propagate sign. */
  out[31] |= (!!signbit) << 7;

  return 0;
}
