/* serpent-decrypt.c

   The serpent block cipher.

   For more details on this algorithm, see the Serpent website at
   http://www.cl.cam.ac.uk/~rja14/serpent.html

   Copyright (C) 2011  Niels Möller
   Copyright (C) 2010, 2011  Simon Josefsson
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.

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

/* This file is derived from cipher/serpent.c in Libgcrypt v1.4.6.
   The adaption to Nettle was made by Simon Josefsson on 2010-12-07
   with final touches on 2011-05-30.  Changes include replacing
   libgcrypt with nettle in the license template, renaming
   serpent_context to serpent_ctx, renaming u32 to uint32_t, removing
   libgcrypt stubs and selftests, modifying entry function prototypes,
   using FOR_BLOCKS to iterate through data in encrypt/decrypt, using
   LE_READ_UINT32 and LE_WRITE_UINT32 to access data in
   encrypt/decrypt, and running indent on the code. */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <limits.h>

#include "serpent.h"

#include "macros.h"
#include "serpent-internal.h"

/* These are the S-Boxes of Serpent.  They are copied from Serpents
   reference implementation (the optimized one, contained in
   `floppy2') and are therefore:

     Copyright (C) 1998 Ross Anderson, Eli Biham, Lars Knudsen.

  To quote the Serpent homepage
  (http://www.cl.cam.ac.uk/~rja14/serpent.html):

  "Serpent is now completely in the public domain, and we impose no
   restrictions on its use.  This was announced on the 21st August at
   the First AES Candidate Conference. The optimised implementations
   in the submission package are now under the GNU PUBLIC LICENSE
   (GPL), although some comments in the code still say otherwise. You
   are welcome to use Serpent for any application."  */

/* S0 inverse:  13  3 11  0 10  6  5 12  1 14  4  7 15  9  8  2 */
/* Original single-assignment form:

     t01 = x2  ^ x3;
     t02 = x0  | x1;
     t03 = x1  | x2;
     t04 = x2  & t01;
     t05 = t02 ^ t01;
     t06 = x0  | t04;
     y2  =     ~ t05;
     t08 = x1  ^ x3;
     t09 = t03 & t08;
     t10 = x3  | y2;
     y1  = t09 ^ t06;
     t12 = x0  | t05;
     t13 = y1  ^ t12;
     t14 = t03 ^ t10;
     t15 = x0  ^ x2;
     y3  = t14 ^ t13;
     t17 = t05 & t13;
     t18 = t14 | t17;
     y0  = t15 ^ t18;
*/
#define SBOX0_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)		\
  do {								\
    y0  = x0 ^ x2;						\
    y2  = x0 | x1;						\
    y1  = x2 ^ x3;						\
    y2 ^= y1;							\
    y1 &= x2;							\
    x2 |= x1;							\
    x1 ^= x3;							\
    y1 |= x0;							\
    x1 &= x2;							\
    y1 ^= x1;							\
    x0 |= y2;							\
    x0 ^= y1;							\
    x1  = y2 & x0;						\
    y2  = ~ y2;							\
    x3 |= y2;							\
    x3 ^= x2;							\
    y3  = x3 ^ x0;						\
    x1 |= x3;							\
    y0 ^= x1;							\
  } while (0)

/* S1 inverse:   5  8  2 14 15  6 12  3 11  4  7  9  1 13 10  0 */
/* Original single-assignment form:
     t01 = x0  ^ x1;
     t02 = x1  | x3;
     t03 = x0  & x2;
     t04 = x2  ^ t02;
     t05 = x0  | t04;
     t06 = t01 & t05;
     t07 = x3  | t03;
     t08 = x1  ^ t06;
     t09 = t07 ^ t06;
     t10 = t04 | t03;
     t11 = x3  & t08;
     y2  =     ~ t09;
     y1  = t10 ^ t11;
     t14 = x0  | y2;
     t15 = t06 ^ y1;
     y3  = t01 ^ t04;
     t17 = x2  ^ t15;
     y0  = t14 ^ t17;
*/
#define SBOX1_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y1  = x1 | x3;					    \
    y1 ^= x2;						    \
    y3  = x0 ^ x1;					    \
    y0  = x0 | y1;					    \
    y0 &= y3;						    \
    x1 ^= y0;						    \
    y3 ^= y1;						    \
    x1 &= x3;						    \
    y2  = x0 & x2;					    \
    y1 |= y2;						    \
    y2 |= x3;						    \
    y2 ^= y0;						    \
    y2  = ~ y2;						    \
    y1 ^= x1;						    \
    y0 ^= y1;						    \
    y0 ^= x2;						    \
    x0 |= y2;						    \
    y0 ^= x0;						    \
  } while (0)

/* S2 inverse:  12  9 15  4 11 14  1  2  0  3  6 13  5  8 10  7 */
/* Original single-assignment form:
     t01 = x0  ^ x3;
     t02 = x2  ^ x3;
     t03 = x0  & x2;
     t04 = x1  | t02;
     y0  = t01 ^ t04;
     t06 = x0  | x2;
     t07 = x3  | y0;
     t08 =     ~ x3;
     t09 = x1  & t06;
     t10 = t08 | t03;
     t11 = x1  & t07;
     t12 = t06 & t02;
     y3  = t09 ^ t10;
     y1  = t12 ^ t11;
     t15 = x2  & y3;
     t16 = y0  ^ y1;
     t17 = t10 ^ t15;
     y2  = t16 ^ t17;
*/
#define SBOX2_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)		\
  do {								\
    y0  = x0 ^ x3;						\
    y2  = x2 ^ x3;						\
    y1  = x1 | y2;						\
    y0 ^= y1;							\
    y1  = x3 | y0;						\
    y1 &= x1;							\
    x3  = ~ x3;							\
    y3  = x0 | x2;						\
    y2 &= y3;							\
    y1 ^= y2;							\
    y3 &= x1;							\
    x0 &= x2;							\
    x0 |= x3;							\
    y3 ^= x0;							\
    x2 &= y3;							\
    x2 ^= x0;							\
    y2  = y0 ^ y1;						\
    y2 ^= x2;							\
  } while (0)

/* S3 inverse:   0  9 10  7 11 14  6 13  3  5 12  2  4  8 15  1 */
/* Original single-assignment form:
     t01 = x2  | x3;
     t02 = x0  | x3;
     t03 = x2  ^ t02;
     t04 = x1  ^ t02;
     t05 = x0  ^ x3;
     t06 = t04 & t03;
     t07 = x1  & t01;
     y2  = t05 ^ t06;
     t09 = x0  ^ t03;
     y0  = t07 ^ t03;
     t11 = y0  | t05;
     t12 = t09 & t11;
     t13 = x0  & y2;
     t14 = t01 ^ t05;
     y1  = x1  ^ t12;
     t16 = x1  | t13;
     y3  = t14 ^ t16;
*/
#define SBOX3_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y3  = x2 | x3;					    \
    y0  = x1 & y3;					    \
    y2  = x0 | x3;					    \
    y1  = x2 ^ y2;					    \
    y0 ^= y1;						    \
    x3 ^= x0;						    \
    y3 ^= x3;						    \
    y2 ^= x1;						    \
    y2 &= y1;						    \
    y2 ^= x3;						    \
    y1 ^= x0;						    \
    x3 |= y0;						    \
    y1 &= x3;						    \
    y1 ^= x1;						    \
    x0 &= y2;						    \
    x0 |= x1;						    \
    y3 ^= x0;						    \
  } while (0)

/* S4 inverse:   5  0  8  3 10  9  7 14  2 12 11  6  4 15 13  1 */
/* Original single-assignment form:
     t01 = x1  | x3;
     t02 = x2  | x3;
     t03 = x0  & t01;
     t04 = x1  ^ t02;
     t05 = x2  ^ x3;
     t06 =     ~ t03;
     t07 = x0  & t04;
     y1  = t05 ^ t07;
     t09 = y1  | t06;
     t10 = x0  ^ t07;
     t11 = t01 ^ t09;
     t12 = x3  ^ t04;
     t13 = x2  | t10;
     y3  = t03 ^ t12;
     t15 = x0  ^ t04;
     y2  = t11 ^ t13;
     y0  = t15 ^ t09;
*/
#define SBOX4_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y1  = x2 ^ x3;					    \
    y2  = x2 | x3;					    \
    y2 ^= x1;						    \
    x1 |= x3;						    \
    y0  = x0 ^ y2;					    \
    x3 ^= y2;						    \
    y2 &= x0;						    \
    y1 ^= y2;						    \
    y2 ^= x0;						    \
    y2 |= x2;						    \
    x0 &=  x1;						    \
    y3  = x0 ^ x3;					    \
    x0  = ~ x0;						    \
    x0 |= y1;						    \
    y0 ^= x0;						    \
    x0 ^= x1;						    \
    y2 ^= x0;						    \
  } while (0)

/* S5 inverse:   8 15  2  9  4  1 13 14 11  6  5  3  7 12 10  0 */
/* Original single-assignment form:
     t01 = x0  & x3;
     t02 = x2  ^ t01;
     t03 = x0  ^ x3;
     t04 = x1  & t02;
     t05 = x0  & x2;
     y0  = t03 ^ t04;
     t07 = x0  & y0;
     t08 = t01 ^ y0;
     t09 = x1  | t05;
     t10 =     ~ x1;
     y1  = t08 ^ t09;
     t12 = t10 | t07;
     t13 = y0  | y1;
     y3  = t02 ^ t12;
     t15 = t02 ^ t13;
     t16 = x1  ^ x3;
     y2  = t16 ^ t15;
*/
#define SBOX5_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y1  = x0 & x3;					    \
    y3  = x2 ^ y1;					    \
    y0  = x1 & y3;					    \
    y2  = x0 ^ x3;					    \
    x3 ^= x1;						    \
    y0 ^= y2;						    \
    x2 &= x0;						    \
    x0 &= y0;						    \
    x2 |= x1;						    \
    y1 ^= y0;						    \
    y1 ^= x2;						    \
    y2  = y0 | y1;					    \
    y2 ^= y3;						    \
    y2 ^= x3;						    \
    x1  = ~ x1;						    \
    x1 |= x0;						    \
    y3 ^= x1;						    \
  } while (0)

/* S6 inverse:  15 10  1 13  5  3  6  0  4  9 14  7  2 12  8 11 */
/* Original single-assignment form:
     t01 = x0  ^ x2;
     t02 =     ~ x2;
     t03 = x1  & t01;
     t04 = x1  | t02;
     t05 = x3  | t03;
     t06 = x1  ^ x3;
     t07 = x0  & t04;
     t08 = x0  | t02;
     t09 = t07 ^ t05;
     y1  = t06 ^ t08;
     y0  =     ~ t09;
     t12 = x1  & y0;
     t13 = t01 & t05;
     t14 = t01 ^ t12;
     t15 = t07 ^ t13;
     t16 = x3  | t02;
     t17 = x0  ^ y1;
     y3  = t17 ^ t15;
     y2  = t16 ^ t14;
 */
#define SBOX6_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y2  = x0 ^ x2;					    \
    x2  = ~ x2;						    \
    y0  = x1 ^ x3;					    \
    y1  = x0 | x2;					    \
    y1 ^= y0;						    \
    y3  = x1 & y2;					    \
    y3 |= x3;						    \
    x3 |= x2;						    \
    x2 |= x1;						    \
    x2 &= x0;						    \
    y0  = x2 ^ y3;					    \
    y0  = ~ y0;						    \
    y3 &= y2;						    \
    y3 ^= x2;						    \
    x0 ^= y1;						    \
    y3 ^= x0;						    \
    x1 &= y0;						    \
    y2 ^= x1;						    \
    y2 ^= x3;						    \
  } while (0)

/* S7 inverse:   3  0  6 13  9 14 15  8  5 12 11  7 10  1  4  2 */
/* Original single-assignment form:
     t01 = x0  & x1;
     t02 = x0  | x1;
     t03 = x2  | t01;
     t04 = x3  & t02;
     y3  = t03 ^ t04;
     t06 = x1  ^ t04;
     t07 = x3  ^ y3;
     t08 =     ~ t07;
     t09 = t06 | t08;
     t10 = x1  ^ x3;
     t11 = x0  | x3;
     y1  = x0  ^ t09;
     t13 = x2  ^ t06;
     t14 = x2  & t11;
     t15 = x3  | y1;
     t16 = t01 | t10;
     y0  = t13 ^ t15;
     y2  = t14 ^ t16;
*/
#define SBOX7_INVERSE(x0, x1, x2, x3, y0, y1, y2, y3)	    \
  do {							    \
    y3  = x0 & x1;					    \
    y2  = x1 ^ x3;					    \
    y2 |= y3;						    \
    y1  = x0 | x3;					    \
    y1 &= x2;						    \
    y2 ^= y1;						    \
    y3 |= x2;						    \
    y0  = x0 | x1;					    \
    y0 &= x3;						    \
    y3 ^= y0;						    \
    y0 ^= x1;						    \
    y1  = x3 ^ y3;					    \
    y1  = ~ y1;						    \
    y1 |= y0;						    \
    y0 ^= x2;						    \
    y1 ^= x0;						    \
    x3 |= y1;						    \
    y0 ^= x3;						    \
  } while (0)

/* In-place inverse linear transformation.  */
#define LINEAR_TRANSFORMATION_INVERSE(x0,x1,x2,x3)	 \
  do {                                                   \
    x2 = ROTL32 (10, x2);                    \
    x0 = ROTL32 (27, x0);                    \
    x2 = x2 ^ x3 ^ (x1 << 7); \
    x0 = x0 ^ x1 ^ x3;        \
    x3 = ROTL32 (25, x3);                     \
    x1 = ROTL32 (31, x1);                     \
    x3 = x3 ^ x2 ^ (x0 << 3); \
    x1 = x1 ^ x0 ^ x2;        \
    x2 = ROTL32 (29, x2);                     \
    x0 = ROTL32 (19, x0);                    \
  } while (0)

/* Round inputs are x0,x1,x2,x3 (destroyed), and round outputs are
   y0,y1,y2,y3. */
#define ROUND_INVERSE(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {							       \
    LINEAR_TRANSFORMATION_INVERSE (x0,x1,x2,x3);	       \
    SBOX##which##_INVERSE(x0,x1,x2,x3, y0,y1,y2,y3);	       \
    KEYXOR(y0,y1,y2,y3, subkey);			       \
  } while (0)

#if HAVE_NATIVE_64_BIT

/* In-place inverse linear transformation.  */
#define LINEAR_TRANSFORMATION64_INVERSE(x0,x1,x2,x3)	 \
  do {                                                   \
    x2 = DROTL32 (10, x2);                    \
    x0 = DROTL32 (27, x0);                    \
    x2 = x2 ^ x3 ^ DRSHIFT32(7, x1); \
    x0 = x0 ^ x1 ^ x3;        \
    x3 = DROTL32 (25, x3);                     \
    x1 = DROTL32 (31, x1);                     \
    x3 = x3 ^ x2 ^ DRSHIFT32(3, x0); \
    x1 = x1 ^ x0 ^ x2;        \
    x2 = DROTL32 (29, x2);                     \
    x0 = DROTL32 (19, x0);                    \
  } while (0)

#define ROUND64_INVERSE(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {							       \
    LINEAR_TRANSFORMATION64_INVERSE (x0,x1,x2,x3);	       \
    SBOX##which##_INVERSE(x0,x1,x2,x3, y0,y1,y2,y3);	       \
    KEYXOR64(y0,y1,y2,y3, subkey);			       \
  } while (0)

#endif /* HAVE_NATIVE_64_BIT */

void
serpent_decrypt (const struct serpent_ctx *ctx,
		 size_t length, uint8_t * dst, const uint8_t * src)
{
  assert( !(length % SERPENT_BLOCK_SIZE));

#if HAVE_NATIVE_64_BIT
  if (length & SERPENT_BLOCK_SIZE)
#else
  while (length >= SERPENT_BLOCK_SIZE)
#endif
    {
      uint32_t x0,x1,x2,x3, y0,y1,y2,y3;
      unsigned k;

      x0 = LE_READ_UINT32 (src);
      x1 = LE_READ_UINT32 (src + 4);
      x2 = LE_READ_UINT32 (src + 8);
      x3 = LE_READ_UINT32 (src + 12);

      /* Inverse of special round */
      KEYXOR (x0,x1,x2,x3, ctx->keys[32]);
      SBOX7_INVERSE (x0,x1,x2,x3, y0,y1,y2,y3);
      KEYXOR (y0,y1,y2,y3, ctx->keys[31]);

      k = 24;
      goto start32;
      while (k > 0)
	{
	  k -= 8;
	  ROUND_INVERSE (7, ctx->keys[k+7], x0,x1,x2,x3, y0,y1,y2,y3);
	start32:
	  ROUND_INVERSE (6, ctx->keys[k+6], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND_INVERSE (5, ctx->keys[k+5], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND_INVERSE (4, ctx->keys[k+4], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND_INVERSE (3, ctx->keys[k+3], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND_INVERSE (2, ctx->keys[k+2], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND_INVERSE (1, ctx->keys[k+1], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND_INVERSE (0, ctx->keys[k], y0,y1,y2,y3, x0,x1,x2,x3);
	}
      
      LE_WRITE_UINT32 (dst, x0);
      LE_WRITE_UINT32 (dst + 4, x1);
      LE_WRITE_UINT32 (dst + 8, x2);
      LE_WRITE_UINT32 (dst + 12, x3);

      src += SERPENT_BLOCK_SIZE;
      dst += SERPENT_BLOCK_SIZE;
      length -= SERPENT_BLOCK_SIZE;
    }
#if HAVE_NATIVE_64_BIT
  FOR_BLOCKS(length, dst, src, 2*SERPENT_BLOCK_SIZE)
    {
      uint64_t x0,x1,x2,x3, y0,y1,y2,y3;
      unsigned k;

      x0 = LE_READ_UINT32 (src);
      x1 = LE_READ_UINT32 (src + 4);
      x2 = LE_READ_UINT32 (src + 8);
      x3 = LE_READ_UINT32 (src + 12);

      x0 <<= 32; x0 |= LE_READ_UINT32 (src + 16);
      x1 <<= 32; x1 |= LE_READ_UINT32 (src + 20);
      x2 <<= 32; x2 |= LE_READ_UINT32 (src + 24);
      x3 <<= 32; x3 |= LE_READ_UINT32 (src + 28);

      /* Inverse of special round */
      KEYXOR64 (x0,x1,x2,x3, ctx->keys[32]);
      SBOX7_INVERSE (x0,x1,x2,x3, y0,y1,y2,y3);
      KEYXOR64 (y0,y1,y2,y3, ctx->keys[31]);

      k = 24;
      goto start64;
      while (k > 0)
	{
	  k -= 8;
	  ROUND64_INVERSE (7, ctx->keys[k+7], x0,x1,x2,x3, y0,y1,y2,y3);
	start64:
	  ROUND64_INVERSE (6, ctx->keys[k+6], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64_INVERSE (5, ctx->keys[k+5], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64_INVERSE (4, ctx->keys[k+4], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64_INVERSE (3, ctx->keys[k+3], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64_INVERSE (2, ctx->keys[k+2], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64_INVERSE (1, ctx->keys[k+1], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64_INVERSE (0, ctx->keys[k], y0,y1,y2,y3, x0,x1,x2,x3);
	}
    
      LE_WRITE_UINT32 (dst + 16, x0);
      LE_WRITE_UINT32 (dst + 20, x1);
      LE_WRITE_UINT32 (dst + 24, x2);
      LE_WRITE_UINT32 (dst + 28, x3);
      x0 >>= 32; LE_WRITE_UINT32 (dst, x0);
      x1 >>= 32; LE_WRITE_UINT32 (dst + 4, x1);
      x2 >>= 32; LE_WRITE_UINT32 (dst + 8, x2);
      x3 >>= 32; LE_WRITE_UINT32 (dst + 12, x3);
    }
#endif /* HAVE_NATIVE_64_BIT */  
}
