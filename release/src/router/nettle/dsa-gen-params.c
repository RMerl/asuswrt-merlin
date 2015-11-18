/* dsa-gen-params.c

   Generation of DSA parameters

   Copyright (C) 2002, 2013, 2014 Niels Möller

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

#include <assert.h>
#include <stdlib.h>

#include "dsa.h"

#include "bignum.h"
#include "nettle-internal.h"


/* Valid sizes, according to FIPS 186-3 are (1024, 160), (2048, 224),
   (2048, 256), (3072, 256). */
int
dsa_generate_params(struct dsa_params *params,
		    void *random_ctx, nettle_random_func *random,
		    void *progress_ctx, nettle_progress_func *progress,
		    unsigned p_bits, unsigned q_bits)
{
  mpz_t r;
  unsigned p0_bits;
  unsigned a;

  if (q_bits < 30 || p_bits < q_bits + 30)
    return 0;

  mpz_init (r);

  nettle_random_prime (params->q, q_bits, 0, random_ctx, random,
		       progress_ctx, progress);

  if (q_bits >= (p_bits + 2)/3)
    _nettle_generate_pocklington_prime (params->p, r, p_bits, 0,
					random_ctx, random,
					params->q, NULL, params->q);
  else
    {
      mpz_t p0, p0q;
      mpz_init (p0);
      mpz_init (p0q);

      p0_bits = (p_bits + 3)/2;
  
      nettle_random_prime (p0, p0_bits, 0,
			   random_ctx, random,
			   progress_ctx, progress);

      if (progress)
	progress (progress_ctx, 'q');

      /* Generate p = 2 r q p0 + 1, such that 2^{n-1} < p < 2^n. */
      mpz_mul (p0q, p0, params->q);

      _nettle_generate_pocklington_prime (params->p, r, p_bits, 0,
					  random_ctx, random,
					  p0, params->q, p0q);

      mpz_mul (r, r, p0);

      mpz_clear (p0);
      mpz_clear (p0q);
    }
  if (progress)
    progress (progress_ctx, 'p');

  for (a = 2; ; a++)
    {
      mpz_set_ui (params->g, a);
      mpz_powm (params->g, params->g, r, params->p);
      if (mpz_cmp_ui (params->g, 1) != 0)
	break;
    }

  mpz_clear (r);
  
  if (progress)
    progress (progress_ctx, 'g');

  return 1;
}
