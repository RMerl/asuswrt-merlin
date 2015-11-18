/* der-iterator.c

   Parsing of ASN.1 DER encoded objects.

   Copyright (C) 2005 Niels Möller

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

#include "bignum.h"

#include "asn1.h"

#include "macros.h"

/* Basic DER syntax: (reference: A Layman's Guide to a Subset of ASN.1, BER, and DER,
   http://luca.ntop.org/Teaching/Appunti/asn1.html)

   The DER header contains a tag and a length. First, the tag. cls is
   the class number, c is one if the object is "constructed" and zero
   if it is primitive. The tag is represented either using a single
   byte,

     7 6   5   4 3 2 1 0
    _____________________
   |_cls_|_c_|_______tag_|   0 <= tag <= 30

   or multiple bytes

     7 6   5   4 3 2 1 0
    _____________________
   |_cls_|_c_|_1_1_1_1_1_|

   followed by the real tag number, in base 128, with all but the
   final byte having the most significant bit set. The tag must be
   represented with as few bytes as possible. High tag numbers are
   currently *not* supported.
   
   Next, the length, either a single byte with the most significant bit clear, or

     7 6 5 4 3 2 1 0
    _________________
   |_1_|___________k_|

   followed by k additional bytes that give the length, in network
   byte order. The length must be encoded using as few bytes as
   possible, and k = 0 is reserved for the "indefinite length form"
   which is not supported.

   After the length comes the contets. For primitive objects (c == 0),
   it's depends on the type. For constructed objects, it's a
   concatenation of the DER encodings of zero or more other objects.
*/

enum {
  TAG_MASK = 0x1f,
  CLASS_MASK = 0xc0,
  CONSTRUCTED_MASK = 0x20,
};

/* Initializes the iterator, but one has to call next to get to the
 * first element. */
static void
asn1_der_iterator_init(struct asn1_der_iterator *iterator,
		       size_t length, const uint8_t *input)
{
  iterator->buffer_length = length;
  iterator->buffer = input;
  iterator->pos = 0;
  iterator->type = 0;
  iterator->length = 0;
  iterator->data = NULL;
}

#define LEFT(i) ((i)->buffer_length - (i)->pos)
#define NEXT(i) ((i)->buffer[(i)->pos++])

/* Gets type and length of the next object. */
enum asn1_iterator_result
asn1_der_iterator_next(struct asn1_der_iterator *i)
{
  uint8_t tag;
  
  if (!LEFT(i))
    return ASN1_ITERATOR_END;

  tag = NEXT(i);
  if (!LEFT(i))
    return ASN1_ITERATOR_ERROR;

  if ( (tag & TAG_MASK) == TAG_MASK)
    {
      /* FIXME: Long tags not supported */
      return ASN1_ITERATOR_ERROR;
    }

  i->length = NEXT(i);
  if (i->length & 0x80)
    {
      unsigned k = i->length & 0x7f;
      unsigned j;
      const uint8_t *data = i->buffer + i->pos;
      
      if (k == 0)
	/* Indefinite encoding. Not supported. */
	return ASN1_ITERATOR_ERROR;

      if (LEFT(i) < k)
	return ASN1_ITERATOR_ERROR;

      if (k > sizeof(i->length))
	return ASN1_ITERATOR_ERROR;

      i->pos += k;
      i->length = data[0];
      if (i->length == 0
	  || (k == 1 && i->length < 0x80))
	return ASN1_ITERATOR_ERROR;

      for (j = 1; j < k; j++)
	i->length = (i->length << 8) | data[j];
    }
  if (LEFT(i) < i->length)
    return ASN1_ITERATOR_ERROR;

  i->data = i->buffer + i->pos;
  i->pos += i->length;

  i->type = tag & TAG_MASK;
  i->type |= (tag & CLASS_MASK) << (ASN1_CLASS_SHIFT - 6);
  if (tag & CONSTRUCTED_MASK)
    {
      i->type |= ASN1_TYPE_CONSTRUCTED;
      return ASN1_ITERATOR_CONSTRUCTED;
    }
  else
    return ASN1_ITERATOR_PRIMITIVE;
}

enum asn1_iterator_result
asn1_der_iterator_first(struct asn1_der_iterator *i,
			size_t length, const uint8_t *input)
{
  asn1_der_iterator_init(i, length, input);
  return asn1_der_iterator_next(i);
}

enum asn1_iterator_result
asn1_der_decode_constructed(struct asn1_der_iterator *i,
			    struct asn1_der_iterator *contents)
{
  assert(i->type & ASN1_TYPE_CONSTRUCTED);
  return asn1_der_iterator_first(contents, i->length, i->data);
}

enum asn1_iterator_result
asn1_der_decode_constructed_last(struct asn1_der_iterator *i)
{
  if (LEFT(i) > 0)
    return ASN1_ITERATOR_ERROR;

  return asn1_der_decode_constructed(i, i);
}

/* Decoding a DER object which is wrapped in a bit string. */
enum asn1_iterator_result
asn1_der_decode_bitstring(struct asn1_der_iterator *i,
			  struct asn1_der_iterator *contents)
{
  assert(i->type == ASN1_BITSTRING);
  /* First byte is the number of padding bits, which must be zero. */
  if (i->length == 0  || i->data[0] != 0)
    return ASN1_ITERATOR_ERROR;

  return asn1_der_iterator_first(contents, i->length - 1, i->data + 1);
}

enum asn1_iterator_result
asn1_der_decode_bitstring_last(struct asn1_der_iterator *i)
{
  if (LEFT(i) > 0)
    return ASN1_ITERATOR_ERROR;

  return asn1_der_decode_bitstring(i, i);
}

int
asn1_der_get_uint32(struct asn1_der_iterator *i,
		    uint32_t *x)
{
  /* Big endian, two's complement, minimum number of octets (except 0,
     which is encoded as a single octet */
  uint32_t value = 0;
  size_t length = i->length;
  unsigned k;

  if (!length || length > 5)
    return 0;

  if (i->data[length - 1] >= 0x80)
    /* Signed number */
    return 0;

  if (length > 1
      && i->data[length -1] == 0
      && i->data[length -2] < 0x80)
    /* Non-minimal number of digits */
    return 0;

  if (length == 5)
    {
      if (i->data[4])
	return 0;
      length--;
    }

  for (value = k = 0; k < length; k++)
    value = (value << 8) | i->data[k];

  *x = value;
  return 1;
}

/* NOTE: This is the only function in this file which needs bignums.
   One could split this file in two, one in libnettle and one in
   libhogweed. */
int
asn1_der_get_bignum(struct asn1_der_iterator *i,
		    mpz_t x, unsigned max_bits)
{
  if (i->length > 1
      && ((i->data[0] == 0 && i->data[1] < 0x80)
	  || (i->data[0] == 0xff && i->data[1] >= 0x80)))
    /* Non-minimal number of digits */
    return 0;

  /* Allow some extra here, for leading sign octets. */
  if (max_bits && (8 * i->length > (16 + max_bits)))
    return 0;

  nettle_mpz_set_str_256_s(x, i->length, i->data);

  /* FIXME: How to interpret a max_bits for negative numbers? */
  if (max_bits && mpz_sizeinbase(x, 2) > max_bits)
    return 0;

  return 1;
}
