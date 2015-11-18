/* der2dsa.c

   Decoding of DSA keys in OpenSSL and X.509.1 format.

   Copyright (C) 2005, 2009 Niels Möller, Magnus Holmgren
   Copyright (C) 2014 Niels Möller

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

#include "dsa.h"

#include "bignum.h"
#include "asn1.h"

#define GET(i, x, l)					\
(asn1_der_iterator_next((i)) == ASN1_ITERATOR_PRIMITIVE	\
 && (i)->type == ASN1_INTEGER				\
 && asn1_der_get_bignum((i), (x), (l))			\
 && mpz_sgn((x)) > 0)

/* If q_bits > 0, q is required to be of exactly this size. */
int
dsa_params_from_der_iterator(struct dsa_params *params,
			     unsigned max_bits, unsigned q_bits,
			     struct asn1_der_iterator *i)
{
  /* Dss-Parms ::= SEQUENCE {
	 p  INTEGER,
	 q  INTEGER,
	 g  INTEGER
     }
  */
  if (i->type == ASN1_INTEGER
      && asn1_der_get_bignum(i, params->p, max_bits)
      && mpz_sgn(params->p) > 0)
    {
      unsigned p_bits = mpz_sizeinbase (params->p, 2);
      return (GET(i, params->q, q_bits ? q_bits : p_bits)
	      && (q_bits == 0 || mpz_sizeinbase(params->q, 2) == q_bits)
	      && mpz_cmp (params->q, params->p) < 0
	      && GET(i, params->g, p_bits)
	      && mpz_cmp (params->g, params->p) < 0
	      && asn1_der_iterator_next(i) == ASN1_ITERATOR_END);
    }
  else
    return 0;
}

int
dsa_public_key_from_der_iterator(const struct dsa_params *params,
				 mpz_t pub,
				 struct asn1_der_iterator *i)
{
  /* DSAPublicKey ::= INTEGER
  */

  return (i->type == ASN1_INTEGER
	  && asn1_der_get_bignum(i, pub,
				 mpz_sizeinbase (params->p, 2))
	  && mpz_sgn(pub) > 0
	  && mpz_cmp(pub, params->p) < 0);
}

int
dsa_openssl_private_key_from_der_iterator(struct dsa_params *params,
					  mpz_t pub,
					  mpz_t priv,
					  unsigned p_max_bits,
					  struct asn1_der_iterator *i)
{
  /* DSAPrivateKey ::= SEQUENCE {
         version           Version,
	 p                 INTEGER,
	 q                 INTEGER,
	 g                 INTEGER,
	 pub_key           INTEGER,  -- y
	 priv_key          INTEGER,  -- x
    }
  */

  uint32_t version;

  if (i->type == ASN1_SEQUENCE
	  && asn1_der_decode_constructed_last(i) == ASN1_ITERATOR_PRIMITIVE
	  && i->type == ASN1_INTEGER
	  && asn1_der_get_uint32(i, &version)
	  && version == 0
      && GET(i, params->p, p_max_bits))
    {
      unsigned p_bits = mpz_sizeinbase (params->p, 2);
      return (GET(i, params->q, DSA_SHA1_Q_BITS)
	      && GET(i, params->g, p_bits)
	      && mpz_cmp (params->g, params->p) < 0
	      && GET(i, pub, p_bits)
	      && mpz_cmp (pub, params->p) < 0
	      && GET(i, priv, DSA_SHA1_Q_BITS)
	      && asn1_der_iterator_next(i) == ASN1_ITERATOR_END);
    }
  else
    return 0;
}

int
dsa_openssl_private_key_from_der(struct dsa_params *params,
				 mpz_t pub,
				 mpz_t priv,
				 unsigned p_max_bits,
				 size_t length, const uint8_t *data)
{
  struct asn1_der_iterator i;
  enum asn1_iterator_result res;

  res = asn1_der_iterator_first(&i, length, data);

  return (res == ASN1_ITERATOR_CONSTRUCTED
	  && dsa_openssl_private_key_from_der_iterator(params, pub, priv,
						       p_max_bits, &i));
}
