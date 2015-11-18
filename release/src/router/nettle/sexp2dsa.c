/* sexp2dsa.c

   Copyright (C) 2002 Niels Möller

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

#include <string.h>

#include "dsa.h"

#include "bignum.h"
#include "sexp.h"

#define GET(x, l, v)				\
do {						\
  if (!nettle_mpz_set_sexp((x), (l), (v))	\
      || mpz_sgn(x) <= 0)			\
    return 0;					\
} while(0)

/* Iterator should point past the algorithm tag, e.g.
 *
 *   (public-key (dsa (p |xxxx|) ...)
 *                    ^ here
 */

int
dsa_keypair_from_sexp_alist(struct dsa_params *params,
			    mpz_t pub,
			    mpz_t priv,
			    unsigned p_max_bits,
			    unsigned q_bits,
			    struct sexp_iterator *i)
{
  static const uint8_t * const names[5]
    = { "p", "q", "g", "y", "x" };
  struct sexp_iterator values[5];
  unsigned nvalues = priv ? 5 : 4;
  unsigned p_bits;

  if (!sexp_iterator_assoc(i, nvalues, names, values))
    return 0;

  GET(params->p, p_max_bits, &values[0]);
  p_bits = mpz_sizeinbase (params->p, 2);
  GET(params->q, q_bits ? q_bits : p_bits, &values[1]);
  if (q_bits > 0 && mpz_sizeinbase(params->q, 2) != q_bits)
    return 0;
  if (mpz_cmp (params->q, params->p) >= 0)
    return 0;
  GET(params->g, p_bits, &values[2]);
  if (mpz_cmp (params->g, params->p) >= 0)
    return 0;
  GET(pub, p_bits, &values[3]);
  if (mpz_cmp (pub, params->p) >= 0)
    return 0;

  if (priv)
    {
      GET(priv, mpz_sizeinbase (params->q, 2), &values[4]);
      if (mpz_cmp (priv, params->q) >= 0)
	return 0;
    }

  return 1;
}

int
dsa_sha1_keypair_from_sexp(struct dsa_params *params,
			   mpz_t pub,
			   mpz_t priv,
			   unsigned p_max_bits, 
			   size_t length, const uint8_t *expr)
{
  struct sexp_iterator i;

  return sexp_iterator_first(&i, length, expr)
    && sexp_iterator_check_type(&i, priv ? "private-key" : "public-key")
    && sexp_iterator_check_type(&i, "dsa")
    && dsa_keypair_from_sexp_alist(params, pub, priv,
				   p_max_bits, DSA_SHA1_Q_BITS, &i);
}

int
dsa_sha256_keypair_from_sexp(struct dsa_params *params,
			     mpz_t pub,
			     mpz_t priv,
			     unsigned p_max_bits, 
			     size_t length, const uint8_t *expr)
{
  struct sexp_iterator i;

  return sexp_iterator_first(&i, length, expr)
    && sexp_iterator_check_type(&i, priv ? "private-key" : "public-key")
    && sexp_iterator_check_type(&i, "dsa-sha256")
    && dsa_keypair_from_sexp_alist(params, pub, priv,
				   p_max_bits, DSA_SHA256_Q_BITS, &i);
}

int
dsa_signature_from_sexp(struct dsa_signature *rs,
			struct sexp_iterator *i,
			unsigned q_bits)
{
  static const uint8_t * const names[2] = { "r", "s" };
  struct sexp_iterator values[2];

  if (!sexp_iterator_assoc(i, 2, names, values))
    return 0;

  GET(rs->r, q_bits, &values[0]);
  GET(rs->s, q_bits, &values[1]);

  return 1;
}
