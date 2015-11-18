/* bignum-random.c

   Generating big random numbers

   Copyright (C) 2002, 2013 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "bignum.h"
#include "gmp-glue.h"

void
nettle_mpz_random_size(mpz_t x,
		       void *ctx, nettle_random_func *random,
		       unsigned bits)
{
  unsigned length = (bits + 7) / 8;
  TMP_GMP_DECL(data, uint8_t);

  TMP_GMP_ALLOC(data, length);

  random(ctx, length, data);
  nettle_mpz_set_str_256_u(x, length, data);

  if (bits % 8)
    mpz_fdiv_r_2exp(x, x, bits);
  
  TMP_GMP_FREE(data);
}

/* Returns a random number x, 0 <= x < n */
void
nettle_mpz_random(mpz_t x,
		  void *ctx, nettle_random_func *random,
		  const mpz_t n)
{
  /* NOTE: This leaves some bias, which may be bad for DSA. A better
   * way might be to generate a random number of mpz_sizeinbase(n, 2)
   * bits, and loop until one smaller than n is found. */

  /* From Daniel Bleichenbacher (via coderpunks):
   *
   * There is still a theoretical attack possible with 8 extra bits.
   * But, the attack would need about 2^66 signatures 2^66 memory and
   * 2^66 time (if I remember that correctly). Compare that to DSA,
   * where the attack requires 2^22 signatures 2^40 memory and 2^64
   * time. And of course, the numbers above are not a real threat for
   * PGP. Using 16 extra bits (i.e. generating a 176 bit random number
   * and reducing it modulo q) will defeat even this theoretical
   * attack.
   * 
   * More generally log_2(q)/8 extra bits are enough to defeat my
   * attack. NIST also plans to update the standard.
   */

  /* Add a few bits extra, to decrease the bias from the final modulo
   * operation. NIST FIPS 186-3 specifies 64 extra bits, for use with
   * DSA. */

  nettle_mpz_random_size(x, 
			 ctx, random,
			 mpz_sizeinbase(n, 2) + 64);
  
  mpz_fdiv_r(x, x, n);
}
