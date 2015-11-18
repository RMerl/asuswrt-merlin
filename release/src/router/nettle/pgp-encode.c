/* pgp-encode.c

   PGP related functions.

   Copyright (C) 2001, 2002 Niels Möller

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
#include <string.h>

#include "pgp.h"

#include "base64.h"
#include "buffer.h"
#include "macros.h"
#include "rsa.h"

int
pgp_put_uint32(struct nettle_buffer *buffer, uint32_t i)
{
  uint8_t *p = nettle_buffer_space(buffer, 4);
  if (!p)
    return 0;
  
  WRITE_UINT32(p, i);
  return 1;
}

int
pgp_put_uint16(struct nettle_buffer *buffer, unsigned i)
{
  uint8_t *p = nettle_buffer_space(buffer, 2);
  if (!p)
    return 0;
  
  WRITE_UINT16(p, i);
  return 1;
}

int
pgp_put_mpi(struct nettle_buffer *buffer, const mpz_t x)
{
  unsigned bits = mpz_sizeinbase(x, 2);
  unsigned octets = (bits + 7) / 8;

  uint8_t *p;

  /* FIXME: What's the correct representation of zero? */
  if (!pgp_put_uint16(buffer, bits))
    return 0;
  
  p = nettle_buffer_space(buffer, octets);

  if (!p)
    return 0;
  
  nettle_mpz_get_str_256(octets, p, x);

  return 1;
}

int
pgp_put_string(struct nettle_buffer *buffer,
	       unsigned length,
	       const uint8_t *s)
{
  return nettle_buffer_write(buffer, length, s);
}

#if 0
static unsigned
length_field(unsigned length)
{
  if (length < PGP_LENGTH_TWO_OCTET)
    return 1;
  else if (length < PGP_LENGTH_FOUR_OCTETS)
    return 2;
  else return 4;
}
#endif

/*   bodyLen = ((1st_octet - 192) << 8) + (2nd_octet) + 192
 *   ==> bodyLen - 192 + 192 << 8 = (1st_octet << 8) + (2nd_octet) 
 */

#define LENGTH_TWO_OFFSET (192 * 255)

int
pgp_put_length(struct nettle_buffer *buffer,
	       unsigned length)
{
  if (length < PGP_LENGTH_TWO_OCTETS)
    return NETTLE_BUFFER_PUTC(buffer, length);

  else if (length < PGP_LENGTH_FOUR_OCTETS)
    return pgp_put_uint16(buffer, length + LENGTH_TWO_OFFSET);
  else
    return NETTLE_BUFFER_PUTC(buffer, 0xff) && pgp_put_uint32(buffer, length);
}

/* Uses the "new" packet format */
int
pgp_put_header(struct nettle_buffer *buffer,
	       unsigned tag, unsigned length)
{
  assert(tag < 0x40);

  return (NETTLE_BUFFER_PUTC(buffer, 0xC0 | tag)
	  && pgp_put_length(buffer, length));  
}

/* FIXME: Should we abort or return error if the length and the field
 * size don't match? */
void
pgp_put_header_length(struct nettle_buffer *buffer,
		      /* start of the header */
		      unsigned start,
		      unsigned field_size)
{
  unsigned length;
  switch (field_size)
    {
    case 1:
      length = buffer->size - (start + 2);
      assert(length < PGP_LENGTH_TWO_OCTETS);
      buffer->contents[start + 1] = length;
      break;
    case 2:
      length = buffer->size - (start + 3);
      assert(length < PGP_LENGTH_FOUR_OCTETS
	     && length >= PGP_LENGTH_TWO_OCTETS);
      WRITE_UINT16(buffer->contents + start + 1, length + LENGTH_TWO_OFFSET);
      break;
    case 4:
      length = buffer->size - (start + 5);
      WRITE_UINT32(buffer->contents + start + 2, length);
      break;
    default:
      abort();
    }
}

int
pgp_put_userid(struct nettle_buffer *buffer,
	       unsigned length,
	       const uint8_t *name)
{
  return (pgp_put_header(buffer, PGP_TAG_USERID, length)
	  && pgp_put_string(buffer, length, name));
}

unsigned
pgp_sub_packet_start(struct nettle_buffer *buffer)
{
  return nettle_buffer_space(buffer, 2) ? buffer->size : 0;
}

int
pgp_put_sub_packet(struct nettle_buffer *buffer,
		   unsigned type,
		   unsigned length,
		   const uint8_t *data)
{
  return (pgp_put_length(buffer, length + 1)
	  && NETTLE_BUFFER_PUTC(buffer, type)
	  && pgp_put_string(buffer, length, data));
}

void
pgp_sub_packet_end(struct nettle_buffer *buffer, unsigned start)
{
  unsigned length;
  
  assert(start >= 2);
  assert(start <= buffer->size);

  length = buffer->size - start;
  WRITE_UINT32(buffer->contents + start - 2, length);
}

int
pgp_put_public_rsa_key(struct nettle_buffer *buffer,
		       const struct rsa_public_key *pub,
		       time_t timestamp)
{
  /* Public key packet, version 4 */
  unsigned start;
  unsigned length;

  /* Size of packet is 16 + the size of e and n */
  length = (4 * 4
	  + nettle_mpz_sizeinbase_256_u(pub->n)
	  + nettle_mpz_sizeinbase_256_u(pub->e));

  if (!pgp_put_header(buffer, PGP_TAG_PUBLIC_KEY, length))
    return 0;

  start = buffer->size;
  
  if (! (pgp_put_header(buffer, PGP_TAG_PUBLIC_KEY,
			/* Assume that we need two octets */
			PGP_LENGTH_TWO_OCTETS)
	 && pgp_put_uint32(buffer, 4)        /* Version */  
	 && pgp_put_uint32(buffer, timestamp)/* Time stamp */
	 && pgp_put_uint32(buffer, PGP_RSA)  /* Algorithm */
	 && pgp_put_mpi(buffer, pub->n)
	 && pgp_put_mpi(buffer, pub->e)) )
    return 0;

  assert(buffer->size == start + length);

  return 1;
}

int
pgp_put_rsa_sha1_signature(struct nettle_buffer *buffer,
			   const struct rsa_private_key *key,
			   const uint8_t *keyid,
			   unsigned type,
			   struct sha1_ctx *hash)
{
  unsigned signature_start = buffer->size;
  unsigned hash_end;
  unsigned sub_packet_start;
  uint8_t trailer[6];
  mpz_t s;
  
  /* Signature packet. The packet could reasonably be both smaller and
   * larger than 192, so for simplicity we use the 4 octet header
   * form. */

  if (! (pgp_put_header(buffer, PGP_TAG_SIGNATURE, PGP_LENGTH_FOUR_OCTETS)
	 && NETTLE_BUFFER_PUTC(buffer, 4)  /* Version */
	 && NETTLE_BUFFER_PUTC(buffer, type)
	 /* Could also be PGP_RSA_SIGN */
	 && NETTLE_BUFFER_PUTC(buffer, PGP_RSA)
	 && NETTLE_BUFFER_PUTC(buffer, PGP_SHA1)
	 && pgp_put_uint16(buffer, 0)))  /* Hashed subpacket length */
    return 0;

  hash_end = buffer->size;

  sha1_update(hash,
	      hash_end - signature_start,
	      buffer->contents + signature_start);

  trailer[0] = 4; trailer[1] = 0xff;
  WRITE_UINT32(trailer + 2, buffer->size - signature_start);

  sha1_update(hash, sizeof(trailer), trailer);

  {
    struct sha1_ctx hcopy = *hash;
    uint8_t *p = nettle_buffer_space(buffer, 2);
    if (!p)
      return 0;
    
    sha1_digest(&hcopy, 2, p);
  }

  /* One "sub-packet" field with the issuer keyid */
  sub_packet_start = pgp_sub_packet_start(buffer);
  if (!sub_packet_start)
    return 0;

  if (pgp_put_sub_packet(buffer, PGP_SUBPACKET_ISSUER_KEY_ID, 8, keyid))
    {
      pgp_sub_packet_end(buffer, sub_packet_start);
      return 0;
    }
    
  mpz_init(s);
  if (!(rsa_sha1_sign(key, hash, s)
	&& pgp_put_mpi(buffer, s)))
    {
      mpz_clear(s);
      return 0;
    }

  mpz_clear(s);
  pgp_put_header_length(buffer, signature_start, 4);

  return 1;
}

#define CRC24_INIT 0x0b704ceL
#define CRC24_POLY 0x1864cfbL

uint32_t
pgp_crc24(unsigned length, const uint8_t *data)
{
  uint32_t crc = CRC24_INIT;

  unsigned i;
  for (i = 0; i<length; i++)
    {
      unsigned j;
      crc ^= ((unsigned) (data[i]) << 16);
      for (j = 0; j<8; j++)
	{
	  crc <<= 1;
	  if (crc & 0x1000000)
	    crc ^= CRC24_POLY;
	}
    }
  assert(crc < 0x1000000);
  return crc;
}


#define WRITE(buffer, s) (nettle_buffer_write(buffer, strlen((s)), (s)))

/* 15 base 64 groups data per line */
#define BINARY_PER_LINE 45
#define TEXT_PER_LINE BASE64_ENCODE_LENGTH(BINARY_PER_LINE)

int
pgp_armor(struct nettle_buffer *buffer,
	  const char *tag,
	  unsigned length,
	  const uint8_t *data)
{
  struct base64_encode_ctx ctx;
  
  unsigned crc = pgp_crc24(length, data);

  base64_encode_init(&ctx);
  
  if (! (WRITE(buffer, "BEGIN PGP ")
	 && WRITE(buffer, tag)
	 && WRITE(buffer, "\nComment: Nettle\n\n")))
    return 0;

  for (;
       length >= BINARY_PER_LINE;
       length -= BINARY_PER_LINE, data += BINARY_PER_LINE)
    {
      unsigned done;
      uint8_t *p
	= nettle_buffer_space(buffer, TEXT_PER_LINE);
      
      if (!p)
	return 0;

      done = base64_encode_update(&ctx, p, BINARY_PER_LINE, data);
      assert(done <= TEXT_PER_LINE);

      /* FIXME: Create some official way to do this */
      buffer->size -= (TEXT_PER_LINE - done);
      
      if (!NETTLE_BUFFER_PUTC(buffer, '\n'))
	return 0;
    }

  if (length)
    {
      unsigned text_size = BASE64_ENCODE_LENGTH(length)
	+ BASE64_ENCODE_FINAL_LENGTH;
      unsigned done;
      
      uint8_t *p
	= nettle_buffer_space(buffer, text_size);
      if (!p)
	return 0;

      done = base64_encode_update(&ctx, p, length, data);
      done += base64_encode_final(&ctx, p + done);

      /* FIXME: Create some official way to do this */
      buffer->size -= (text_size - done);
      
      if (!NETTLE_BUFFER_PUTC(buffer, '\n'))
	return 0;
    }
  /* Checksum */
  if (!NETTLE_BUFFER_PUTC(buffer, '='))
    return 0;

  {
    uint8_t *p = nettle_buffer_space(buffer, 4);
    if (!p)
      return 0;
    base64_encode_group(p, crc);
  }
  
  return (WRITE(buffer, "\nBEGIN PGP ")
	  && WRITE(buffer, tag)
	  && NETTLE_BUFFER_PUTC(buffer, '\n'));
}
