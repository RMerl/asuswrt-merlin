/* twofish.c

   The twofish block cipher.

   Copyright (C) 2001, 2014 Niels Möller
   Copyright (C) 1999 Ruud de Rooij <ruud@debian.org>

   Modifications for lsh, integrated testing
   Copyright (C) 1999 J.H.M. Dassen (Ray) <jdassen@wi.LeidenUniv.nl>

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
#include <string.h>

#include "twofish.h"

#include "macros.h"

/* Bitwise rotations on 32-bit words.  These are defined as macros that
 * evaluate their argument twice, so do not apply to any expressions with
 * side effects.
 */

#define rol1(x) (((x) << 1) | (((x) & 0x80000000) >> 31))
#define rol8(x) (((x) << 8) | (((x) & 0xFF000000) >> 24))
#define rol9(x) (((x) << 9) | (((x) & 0xFF800000) >> 23))
#define ror1(x) (((x) >> 1) | (((x) & 0x00000001) << 31))

/* ------------------------------------------------------------------------- */

/* The permutations q0 and q1.  These are fixed permutations on 8-bit values.
 * The permutations have been computed using the program twofish-data,
 * which is distributed along with this file.
 */

static const uint8_t q0[256] = {
  0xA9,0x67,0xB3,0xE8,0x04,0xFD,0xA3,0x76,
  0x9A,0x92,0x80,0x78,0xE4,0xDD,0xD1,0x38,
  0x0D,0xC6,0x35,0x98,0x18,0xF7,0xEC,0x6C,
  0x43,0x75,0x37,0x26,0xFA,0x13,0x94,0x48,
  0xF2,0xD0,0x8B,0x30,0x84,0x54,0xDF,0x23,
  0x19,0x5B,0x3D,0x59,0xF3,0xAE,0xA2,0x82,
  0x63,0x01,0x83,0x2E,0xD9,0x51,0x9B,0x7C,
  0xA6,0xEB,0xA5,0xBE,0x16,0x0C,0xE3,0x61,
  0xC0,0x8C,0x3A,0xF5,0x73,0x2C,0x25,0x0B,
  0xBB,0x4E,0x89,0x6B,0x53,0x6A,0xB4,0xF1,
  0xE1,0xE6,0xBD,0x45,0xE2,0xF4,0xB6,0x66,
  0xCC,0x95,0x03,0x56,0xD4,0x1C,0x1E,0xD7,
  0xFB,0xC3,0x8E,0xB5,0xE9,0xCF,0xBF,0xBA,
  0xEA,0x77,0x39,0xAF,0x33,0xC9,0x62,0x71,
  0x81,0x79,0x09,0xAD,0x24,0xCD,0xF9,0xD8,
  0xE5,0xC5,0xB9,0x4D,0x44,0x08,0x86,0xE7,
  0xA1,0x1D,0xAA,0xED,0x06,0x70,0xB2,0xD2,
  0x41,0x7B,0xA0,0x11,0x31,0xC2,0x27,0x90,
  0x20,0xF6,0x60,0xFF,0x96,0x5C,0xB1,0xAB,
  0x9E,0x9C,0x52,0x1B,0x5F,0x93,0x0A,0xEF,
  0x91,0x85,0x49,0xEE,0x2D,0x4F,0x8F,0x3B,
  0x47,0x87,0x6D,0x46,0xD6,0x3E,0x69,0x64,
  0x2A,0xCE,0xCB,0x2F,0xFC,0x97,0x05,0x7A,
  0xAC,0x7F,0xD5,0x1A,0x4B,0x0E,0xA7,0x5A,
  0x28,0x14,0x3F,0x29,0x88,0x3C,0x4C,0x02,
  0xB8,0xDA,0xB0,0x17,0x55,0x1F,0x8A,0x7D,
  0x57,0xC7,0x8D,0x74,0xB7,0xC4,0x9F,0x72,
  0x7E,0x15,0x22,0x12,0x58,0x07,0x99,0x34,
  0x6E,0x50,0xDE,0x68,0x65,0xBC,0xDB,0xF8,
  0xC8,0xA8,0x2B,0x40,0xDC,0xFE,0x32,0xA4,
  0xCA,0x10,0x21,0xF0,0xD3,0x5D,0x0F,0x00,
  0x6F,0x9D,0x36,0x42,0x4A,0x5E,0xC1,0xE0,
};

static const uint8_t q1[256] = {
  0x75,0xF3,0xC6,0xF4,0xDB,0x7B,0xFB,0xC8,
  0x4A,0xD3,0xE6,0x6B,0x45,0x7D,0xE8,0x4B,
  0xD6,0x32,0xD8,0xFD,0x37,0x71,0xF1,0xE1,
  0x30,0x0F,0xF8,0x1B,0x87,0xFA,0x06,0x3F,
  0x5E,0xBA,0xAE,0x5B,0x8A,0x00,0xBC,0x9D,
  0x6D,0xC1,0xB1,0x0E,0x80,0x5D,0xD2,0xD5,
  0xA0,0x84,0x07,0x14,0xB5,0x90,0x2C,0xA3,
  0xB2,0x73,0x4C,0x54,0x92,0x74,0x36,0x51,
  0x38,0xB0,0xBD,0x5A,0xFC,0x60,0x62,0x96,
  0x6C,0x42,0xF7,0x10,0x7C,0x28,0x27,0x8C,
  0x13,0x95,0x9C,0xC7,0x24,0x46,0x3B,0x70,
  0xCA,0xE3,0x85,0xCB,0x11,0xD0,0x93,0xB8,
  0xA6,0x83,0x20,0xFF,0x9F,0x77,0xC3,0xCC,
  0x03,0x6F,0x08,0xBF,0x40,0xE7,0x2B,0xE2,
  0x79,0x0C,0xAA,0x82,0x41,0x3A,0xEA,0xB9,
  0xE4,0x9A,0xA4,0x97,0x7E,0xDA,0x7A,0x17,
  0x66,0x94,0xA1,0x1D,0x3D,0xF0,0xDE,0xB3,
  0x0B,0x72,0xA7,0x1C,0xEF,0xD1,0x53,0x3E,
  0x8F,0x33,0x26,0x5F,0xEC,0x76,0x2A,0x49,
  0x81,0x88,0xEE,0x21,0xC4,0x1A,0xEB,0xD9,
  0xC5,0x39,0x99,0xCD,0xAD,0x31,0x8B,0x01,
  0x18,0x23,0xDD,0x1F,0x4E,0x2D,0xF9,0x48,
  0x4F,0xF2,0x65,0x8E,0x78,0x5C,0x58,0x19,
  0x8D,0xE5,0x98,0x57,0x67,0x7F,0x05,0x64,
  0xAF,0x63,0xB6,0xFE,0xF5,0xB7,0x3C,0xA5,
  0xCE,0xE9,0x68,0x44,0xE0,0x4D,0x43,0x69,
  0x29,0x2E,0xAC,0x15,0x59,0xA8,0x0A,0x9E,
  0x6E,0x47,0xDF,0x34,0x35,0x6A,0xCF,0xDC,
  0x22,0xC9,0xC0,0x9B,0x89,0xD4,0xED,0xAB,
  0x12,0xA2,0x0D,0x52,0xBB,0x02,0x2F,0xA9,
  0xD7,0x61,0x1E,0xB4,0x50,0x04,0xF6,0xC2,
  0x16,0x25,0x86,0x56,0x55,0x09,0xBE,0x91,
};

/* ------------------------------------------------------------------------- */

/* uint8_t gf_multiply(uint8_t p, uint8_t a, uint8_t b)
 *
 * Multiplication in GF(2^8).
 *
 * This function multiplies a times b in the Galois Field GF(2^8) with
 * primitive polynomial p.
 * The representation of the polynomials a, b, and p uses bits with
 * values 2^i to represent the terms x^i.  The polynomial p contains an
 * implicit term x^8.
 *
 * Note that addition and subtraction in GF(2^8) is simply the XOR
 * operation.
 */

static uint8_t
gf_multiply(uint8_t p, uint8_t a, uint8_t b)
{
  uint32_t shift  = b;
  uint8_t result = 0;
  while (a)
    {
      if (a & 1) result ^= shift;
      a = a >> 1;
      shift = shift << 1;
      if (shift & 0x100) shift ^= p;
    }
  return result;
}

/* ------------------------------------------------------------------------- */

/* The matrix RS as specified in section 4.3 the twofish paper. */

static const uint8_t rs_matrix[4][8] = {
    { 0x01, 0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E },
    { 0xA4, 0x56, 0x82, 0xF3, 0x1E, 0xC6, 0x68, 0xE5 },
    { 0x02, 0xA1, 0xFC, 0xC1, 0x47, 0xAE, 0x3D, 0x19 },
    { 0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E, 0x03 } };

/* uint32_t compute_s(uint32_t m1, uint32_t m2);
 *
 * Computes the value RS * M, where M is a byte vector composed of the
 * bytes of m1 and m2.  Arithmetic is done in GF(2^8) with primitive
 * polynomial x^8 + x^6 + x^3 + x^2 + 1.
 *
 * This function is used to compute the sub-keys S which are in turn used
 * to generate the S-boxes.
 */

static uint32_t
compute_s(uint32_t m1, uint32_t m2)
{
  uint32_t s = 0;
  int i;
  for (i = 0; i < 4; i++)
    s |=  ((  gf_multiply(0x4D, m1,       rs_matrix[i][0])
	    ^ gf_multiply(0x4D, m1 >> 8,  rs_matrix[i][1])
	    ^ gf_multiply(0x4D, m1 >> 16, rs_matrix[i][2])
	    ^ gf_multiply(0x4D, m1 >> 24, rs_matrix[i][3])
	    ^ gf_multiply(0x4D, m2,       rs_matrix[i][4])
	    ^ gf_multiply(0x4D, m2 >> 8,  rs_matrix[i][5])
	    ^ gf_multiply(0x4D, m2 >> 16, rs_matrix[i][6])
	    ^ gf_multiply(0x4D, m2 >> 24, rs_matrix[i][7])) << (i*8));
  return s;
}

/* ------------------------------------------------------------------------- */

/* This table describes which q S-boxes are used for each byte in each stage
 * of the function h, cf. figure 2 of the twofish paper.
 */

static const uint8_t * const q_table[4][5] =
  { { q1, q1, q0, q0, q1 },
    { q0, q1, q1, q0, q0 },
    { q0, q0, q0, q1, q1 },
    { q1, q0, q1, q1, q0 } };

/* The matrix MDS as specified in section 4.3.2 of the twofish paper. */

static const uint8_t mds_matrix[4][4] = { { 0x01, 0xEF, 0x5B, 0x5B },
				 { 0x5B, 0xEF, 0xEF, 0x01 },
				 { 0xEF, 0x5B, 0x01, 0xEF },
				 { 0xEF, 0x01, 0xEF, 0x5B } };

/* uint32_t h_uint8_t(int k, int i, uint8_t x, uint8_t l0, uint8_t l1, uint8_t l2, uint8_t l3);
 *
 * Perform the h function (section 4.3.2) on one byte.  It consists of
 * repeated applications of the q permutation, followed by a XOR with
 * part of a sub-key.  Finally, the value is multiplied by one column of
 * the MDS matrix.  To obtain the result for a full word, the results of
 * h for the individual bytes are XORed.
 *
 * k is the key size (/ 64 bits), i is the byte number (0 = LSB), x is the
 * actual byte to apply the function to; l0, l1, l2, and l3 are the
 * appropriate bytes from the subkey.  Note that only l0..l(k-1) are used.
 */

static uint32_t
h_byte(int k, int i, uint8_t x, uint8_t l0, uint8_t l1, uint8_t l2, uint8_t l3)
{
  uint8_t y = q_table[i][4][l0 ^
            q_table[i][3][l1 ^
              q_table[i][2][k == 2 ? x : l2 ^
                q_table[i][1][k == 3 ? x : l3 ^ q_table[i][0][x]]]]];

  return ( ((uint32_t)gf_multiply(0x69, mds_matrix[0][i], y))
	   | ((uint32_t)gf_multiply(0x69, mds_matrix[1][i], y) << 8)
	   | ((uint32_t)gf_multiply(0x69, mds_matrix[2][i], y) << 16)
	   | ((uint32_t)gf_multiply(0x69, mds_matrix[3][i], y) << 24) );
}

/* uint32_t h(int k, uint8_t x, uint32_t l0, uint32_t l1, uint32_t l2, uint32_t l3);
 *
 * Perform the function h on a word.  See the description of h_byte() above.
 */

static uint32_t
h(int k, uint8_t x, uint32_t l0, uint32_t l1, uint32_t l2, uint32_t l3)
{
  return (  h_byte(k, 0, x, l0,       l1,       l2,       l3)
	  ^ h_byte(k, 1, x, l0 >> 8,  l1 >> 8,  l2 >> 8,  l3 >> 8)
	  ^ h_byte(k, 2, x, l0 >> 16, l1 >> 16, l2 >> 16, l3 >> 16)
	  ^ h_byte(k, 3, x, l0 >> 24, l1 >> 24, l2 >> 24, l3 >> 24) );
}


/* ------------------------------------------------------------------------- */

/* API */

/* Structure which contains the tables containing the subkeys and the
 * key-dependent s-boxes.
 */


/* Set up internal tables required for twofish encryption and decryption.
 *
 * The key size is specified in bytes.  Key sizes up to 32 bytes are
 * supported.  Larger key sizes are silently truncated.  
 */

void
twofish_set_key(struct twofish_ctx *context,
		size_t keysize, const uint8_t *key)
{
  uint8_t key_copy[32];
  uint32_t m[8], s[4], t;
  int i, j, k;

  /* Extend key as necessary */

  assert(keysize <= 32);

  /* We do a little more copying than necessary, but that doesn't
   * really matter. */
  memset(key_copy, 0, 32);
  memcpy(key_copy, key, keysize);

  for (i = 0; i<8; i++)
    m[i] = LE_READ_UINT32(key_copy + i*4);
  
  if (keysize <= 16)
    k = 2;
  else if (keysize <= 24)
    k = 3;
  else
    k = 4;

  /* Compute sub-keys */

  for (i = 0; i < 20; i++)
    {
      t = h(k, 2*i+1, m[1], m[3], m[5], m[7]);
      t = rol8(t);
      t += (context->keys[2*i] =
	    t + h(k, 2*i, m[0], m[2], m[4], m[6]));
      t = rol9(t);
      context->keys[2*i+1] = t;
    }

  /* Compute key-dependent S-boxes */

  for (i = 0; i < k; i++)
    s[k-1-i] = compute_s(m[2*i], m[2*i+1]);

  for (i = 0; i < 4; i++)
    for (j = 0; j < 256; j++)
      context->s_box[i][j] = h_byte(k, i, j,
				    s[0] >> (i*8),
				    s[1] >> (i*8),
				    s[2] >> (i*8),
				    s[3] >> (i*8));
}

void
twofish128_set_key(struct twofish_ctx *context, const uint8_t *key)
{
  twofish_set_key (context, TWOFISH128_KEY_SIZE, key);
}
void
twofish192_set_key(struct twofish_ctx *context, const uint8_t *key)
{
  twofish_set_key (context, TWOFISH192_KEY_SIZE, key);
}
void
twofish256_set_key(struct twofish_ctx *context, const uint8_t *key)
{
  twofish_set_key (context, TWOFISH256_KEY_SIZE, key);
}

/* Encrypt blocks of 16 bytes of data with the twofish algorithm.
 *
 * Before this function can be used, twofish_set_key() must be used in order to
 * set up various tables required for the encryption algorithm.
 * 
 * This function always encrypts 16 bytes of plaintext to 16 bytes of
 * ciphertext.  The memory areas of the plaintext and the ciphertext can
 * overlap.
 */

void
twofish_encrypt(const struct twofish_ctx *context,
		size_t length,
		uint8_t *ciphertext,
		const uint8_t *plaintext)
{
  const uint32_t * keys        = context->keys;
  const uint32_t (*s_box)[256] = context->s_box;

  assert( !(length % TWOFISH_BLOCK_SIZE) );
  for ( ; length; length -= TWOFISH_BLOCK_SIZE)
    {  
      uint32_t words[4];
      uint32_t r0, r1, r2, r3, t0, t1;
      int i;

      for (i = 0; i<4; i++, plaintext += 4)
	words[i] = LE_READ_UINT32(plaintext);

      r0 = words[0] ^ keys[0];
      r1 = words[1] ^ keys[1];
      r2 = words[2] ^ keys[2];
      r3 = words[3] ^ keys[3];
  
      for (i = 0; i < 8; i++) {
	t1 = (  s_box[1][r1 & 0xFF]
		^ s_box[2][(r1 >> 8) & 0xFF]
		^ s_box[3][(r1 >> 16) & 0xFF]
		^ s_box[0][(r1 >> 24) & 0xFF]);
	t0 = (  s_box[0][r0 & 0xFF]
		^ s_box[1][(r0 >> 8) & 0xFF]
		^ s_box[2][(r0 >> 16) & 0xFF]
		^ s_box[3][(r0 >> 24) & 0xFF]) + t1;
	r3 = (t1 + t0 + keys[4*i+9]) ^ rol1(r3);
	r2 = (t0 + keys[4*i+8]) ^ r2;
	r2 = ror1(r2);

	t1 = (  s_box[1][r3 & 0xFF]
		^ s_box[2][(r3 >> 8) & 0xFF]
		^ s_box[3][(r3 >> 16) & 0xFF]
		^ s_box[0][(r3 >> 24) & 0xFF]);
	t0 = (  s_box[0][r2 & 0xFF]
		^ s_box[1][(r2 >> 8) & 0xFF]
		^ s_box[2][(r2 >> 16) & 0xFF]
		^ s_box[3][(r2 >> 24) & 0xFF]) + t1;
	r1 = (t1 + t0 + keys[4*i+11]) ^ rol1(r1);
	r0 = (t0 + keys[4*i+10]) ^ r0;
	r0 = ror1(r0);
      }

      words[0] = r2 ^ keys[4];
      words[1] = r3 ^ keys[5];
      words[2] = r0 ^ keys[6];
      words[3] = r1 ^ keys[7];

      for (i = 0; i<4; i++, ciphertext += 4)
	LE_WRITE_UINT32(ciphertext, words[i]);
    }
}

/* Decrypt blocks of 16 bytes of data with the twofish algorithm.
 *
 * Before this function can be used, twofish_set_key() must be used in order to
 * set up various tables required for the decryption algorithm.
 * 
 * This function always decrypts 16 bytes of ciphertext to 16 bytes of
 * plaintext.  The memory areas of the plaintext and the ciphertext can
 * overlap.
 */

void
twofish_decrypt(const struct twofish_ctx *context,
		size_t length,
		uint8_t *plaintext,
		const uint8_t *ciphertext)

{
  const uint32_t *keys  = context->keys;
  const uint32_t (*s_box)[256] = context->s_box;

  assert( !(length % TWOFISH_BLOCK_SIZE) );
  for ( ; length; length -= TWOFISH_BLOCK_SIZE)
    {  
      uint32_t words[4];
      uint32_t r0, r1, r2, r3, t0, t1;
      int i;

      for (i = 0; i<4; i++, ciphertext += 4)
	words[i] = LE_READ_UINT32(ciphertext);

      r0 = words[2] ^ keys[6];
      r1 = words[3] ^ keys[7];
      r2 = words[0] ^ keys[4];
      r3 = words[1] ^ keys[5];

      for (i = 0; i < 8; i++) {
	t1 = (  s_box[1][r3 & 0xFF]
		^ s_box[2][(r3 >> 8) & 0xFF]
		^ s_box[3][(r3 >> 16) & 0xFF]
		^ s_box[0][(r3 >> 24) & 0xFF]);
	t0 = (  s_box[0][r2 & 0xFF]
		^ s_box[1][(r2 >> 8) & 0xFF]
		^ s_box[2][(r2 >> 16) & 0xFF]
		^ s_box[3][(r2 >> 24) & 0xFF]) + t1;
	r1 = (t1 + t0 + keys[39-4*i]) ^ r1;
	r1 = ror1(r1);
	r0 = (t0 + keys[38-4*i]) ^ rol1(r0);

	t1 = (  s_box[1][r1 & 0xFF]
		^ s_box[2][(r1 >> 8) & 0xFF]
		^ s_box[3][(r1 >> 16) & 0xFF]
		^ s_box[0][(r1 >> 24) & 0xFF]);
	t0 = (  s_box[0][r0 & 0xFF]
		^ s_box[1][(r0 >> 8) & 0xFF]
		^ s_box[2][(r0 >> 16) & 0xFF]
		^ s_box[3][(r0 >> 24) & 0xFF]) + t1;
	r3 = (t1 + t0 + keys[37-4*i]) ^ r3;
	r3 = ror1(r3);
	r2 = (t0 + keys[36-4*i]) ^ rol1(r2);
      }

      words[0] = r0 ^ keys[0];
      words[1] = r1 ^ keys[1];
      words[2] = r2 ^ keys[2];
      words[3] = r3 ^ keys[3];

      for (i = 0; i<4; i++, plaintext += 4)
	LE_WRITE_UINT32(plaintext, words[i]);
    }
}
