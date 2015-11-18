/* serpent-encrypt.c

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

/* S0:  3  8 15  1 10  6  5 11 14 13  4  2  7  0  9 12 */
/* Could easily let y0, y1 overlap with x0, x1, and possibly also x2 and y2 */
#define SBOX0(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y3  = x1 ^ x2;					\
    y0  = x0 | x3;					\
    y1  = x0 ^ x1;					\
    y3 ^= y0;						\
    y2  = x2 | y3;					\
    x0 ^= x3;						\
    y2 &= x3;						\
    x3 ^= x2;						\
    x2 |= x1;						\
    y0  = y1 & x2;					\
    y2 ^= y0;						\
    y0 &= y2;						\
    y0 ^= x2;						\
    x1 &= x0;						\
    y0 ^= x0;						\
    y0  = ~ y0;						\
    y1  = y0 ^ x1;					\
    y1 ^= x3;						\
  } while (0)

/* FIXME: Arrange for some overlap between inputs and outputs? */
/* S1: 15 12  2  7  9  0  5 10  1 11 14  8  6 13  3  4 */
/* Original single-assignment form:
   
     t01 = x0  | x3;
     t02 = x2  ^ x3;
     t03 =     ~ x1;
     t04 = x0  ^ x2;
     t05 = x0  | t03;
     t06 = x3  & t04;
     t07 = t01 & t02;
     t08 = x1  | t06;
     y2  = t02 ^ t05;
     t10 = t07 ^ t08;
     t11 = t01 ^ t10;
     t12 = y2  ^ t11;
     t13 = x1  & x3;
     y3  =     ~ t10;
     y1  = t13 ^ t12;
     t16 = t10 | y1;
     t17 = t05 & t16;
     y0  = x2  ^ t17;
*/
#define SBOX1(x0, x1, x2, x3, y0, y1, y2, y3)		\
  do {							\
    y1  = x0 | x3;					\
    y2  = x2 ^ x3;					\
    y0  = ~ x1;						\
    y3  = x0 ^ x2;					\
    y0 |= x0;						\
    y3 &= x3;						\
    x0 = y1 & y2;					\
    y3 |= x1;						\
    y2 ^= y0;						\
    y3 ^= x0;						\
    x0  = y1 ^ y3;					\
    x0 ^= y2;						\
    y1  = x1 & x3;					\
    y1 ^= x0;						\
    x3  = y1 | y3;					\
    y3  = ~ y3;						\
    y0 &= x3;						\
    y0 ^= x2;						\
  } while (0)

/* FIXME: Arrange for some overlap between inputs and outputs? */
/* S2:  8  6  7  9  3 12 10 15 13  1 14  4  0 11  5  2 */
#define SBOX2(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y2  = x0 | x2;					\
    y1  = x0 ^ x1;					\
    y3  = x3 ^ y2;					\
    y0  = y1 ^ y3;					\
    x3 |= x0;						\
    x2 ^= y0;						\
    x0  = x1 ^ x2;					\
    x2 |= x1;						\
    x0 &= y2;						\
    y3 ^= x2;						\
    y1 |= y3;						\
    y1 ^= x0;						\
    y2  = y3 ^ y1;					\
    y2 ^= x1;						\
    y3  = ~ y3;						\
    y2 ^= x3;						\
  } while (0)

/* S3:  0 15 11  8 12  9  6  3 13  1  2  4 10  7  5 14 */
/* Original single-assignment form:
   
     t01 = x0  ^ x2;
     t02 = x0  | x3;
     t03 = x0  & x3;
     t04 = t01 & t02;
     t05 = x1  | t03;
     t06 = x0  & x1;
     t07 = x3  ^ t04;
     t08 = x2  | t06;
     t09 = x1  ^ t07;
     t10 = x3  & t05;
     t11 = t02 ^ t10;
     y3  = t08 ^ t09;
     t13 = x3  | y3;
     t14 = x0  | t07;
     t15 = x1  & t13;
     y2  = t08 ^ t11;
     y0  = t14 ^ t15;
     y1  = t05 ^ t04;
*/
#define SBOX3(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y1  = x0 ^ x2;					\
    y0  = x0 | x3;					\
    y3  = x0 & x3;					\
    y1 &= y0;						\
    y3 |= x1;						\
    y2  = x0 & x1;					\
    y2 |= x2;						\
    x2  = x3 ^ y1;					\
    y1 ^= y3;						\
    x0 |= x2;						\
    x2 ^= x1;						\
    y3 &= x3;						\
    y0 ^= y3;						\
    y3  = y2 ^ x2;					\
    y2 ^= y0;						\
    x3 |= y3;						\
    x1 &= x3;						\
    y0  = x0 ^ x1;					\
  } while (0)


/* S4:  1 15  8  3 12  0 11  6  2  5  4 10  9 14  7 13 */
/* Original single-assignment form:
    t01 = x0  | x1;
    t02 = x1  | x2;
    t03 = x0  ^ t02;
    t04 = x1  ^ x3;
    t05 = x3  | t03;
    t06 = x3  & t01;
    y3  = t03 ^ t06;
    t08 = y3  & t04;
    t09 = t04 & t05;
    t10 = x2  ^ t06;
    t11 = x1  & x2;
    t12 = t04 ^ t08;
    t13 = t11 | t03;
    t14 = t10 ^ t09;
    t15 = x0  & t05;
    t16 = t11 | t12;
    y2  = t13 ^ t08;
    y1  = t15 ^ t16;
    y0  =     ~ t14;
*/
#define SBOX4(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y3  = x0 | x1;					\
    y2  = x1 | x2;					\
    y2 ^= x0;						\
    y3 &= x3;						\
    y0  = x1 ^ x3;					\
    x3 |= y2;						\
    x0 &= x3;						\
    x1 &= x2;						\
    x2 ^= y3;						\
    y3 ^= y2;						\
    y2 |= x1;						\
    y1  = y3 & y0;					\
    y2 ^= y1;						\
    y1 ^= y0;						\
    y1 |= x1;						\
    y1 ^= x0;						\
    y0 &= x3;						\
    y0 ^= x2;						\
    y0  = ~y0;						\
  } while (0)

/* S5: 15  5  2 11  4 10  9 12  0  3 14  8 13  6  7  1 */
/* Original single-assignment form:
    t01 = x1  ^ x3;
    t02 = x1  | x3;
    t03 = x0  & t01;
    t04 = x2  ^ t02;
    t05 = t03 ^ t04;
    y0  =     ~ t05;
    t07 = x0  ^ t01;
    t08 = x3  | y0;
    t09 = x1  | t05;
    t10 = x3  ^ t08;
    t11 = x1  | t07;
    t12 = t03 | y0;
    t13 = t07 | t10;
    t14 = t01 ^ t11;
    y2  = t09 ^ t13;
    y1  = t07 ^ t08;
    y3  = t12 ^ t14;
*/
#define SBOX5(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y0  = x1 | x3;					\
    y0 ^= x2;						\
    x2  = x1 ^ x3;					\
    y2  = x0 ^ x2;					\
    x0 &= x2;						\
    y0 ^= x0;						\
    y3  = x1 | y2;					\
    x1 |= y0;						\
    y0  = ~y0;						\
    x0 |= y0;						\
    y3 ^= x2;						\
    y3 ^= x0;						\
    y1  = x3 | y0;					\
    x3 ^= y1;						\
    y1 ^= y2;						\
    y2 |= x3;						\
    y2 ^= x1;						\
  } while (0)

/* S6:  7  2 12  5  8  4  6 11 14  9  1 15 13  3 10  0 */
/* Original single-assignment form:
    t01 = x0  & x3;
    t02 = x1  ^ x2;
    t03 = x0  ^ x3;
    t04 = t01 ^ t02;
    t05 = x1  | x2;
    y1  =     ~ t04;
    t07 = t03 & t05;
    t08 = x1  & y1;
    t09 = x0  | x2;
    t10 = t07 ^ t08;
    t11 = x1  | x3;
    t12 = x2  ^ t11;
    t13 = t09 ^ t10;
    y2  =     ~ t13;
    t15 = y1  & t03;
    y3  = t12 ^ t07;
    t17 = x0  ^ x1;
    t18 = y2  ^ t15;
    y0  = t17 ^ t18;
*/
#define SBOX6(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y0  = x0 ^ x3;					\
    y1  = x0 & x3;					\
    y2  = x0 | x2;					\
    x3 |= x1;						\
    x3 ^= x2;						\
    x0 ^= x1;						\
    y3  = x1 | x2;					\
    x2 ^= x1;						\
    y3 &= y0;						\
    y1 ^= x2;						\
    y1  = ~y1;						\
    y0 &= y1;						\
    x1 &= y1;						\
    x1 ^= y3;						\
    y3 ^= x3;						\
    y2 ^= x1;						\
    y2  = ~y2;						\
    y0 ^= y2;						\
    y0 ^= x0;						\
  } while (0)

/* S7:  1 13 15  0 14  8  2 11  7  4 12 10  9  3  5  6 */
/* Original single-assignment form:
    t01 = x0  & x2;
    t02 =     ~ x3;
    t03 = x0  & t02;
    t04 = x1  | t01;
    t05 = x0  & x1;
    t06 = x2  ^ t04;
    y3  = t03 ^ t06;
    t08 = x2  | y3;
    t09 = x3  | t05;
    t10 = x0  ^ t08;
    t11 = t04 & y3;
    y1  = t09 ^ t10;
    t13 = x1  ^ y1;
    t14 = t01 ^ y1;
    t15 = x2  ^ t05;
    t16 = t11 | t13;
    t17 = t02 | t14;
    y0  = t15 ^ t17;
    y2  = x0  ^ t16;
*/
/* It appears impossible to do this with only 8 registers. We
   recompute t02, and t04 (if we have spare registers, hopefully the
   compiler can recognize them as common subexpressions). */
#define SBOX7(x0, x1, x2, x3, y0, y1, y2, y3)	\
  do {							\
    y0  = x0 & x2;					\
    y3  = x1 | y0; /* t04 */				\
    y3 ^= x2;						\
    y1  = ~x3;     /* t02 */				\
    y1 &= x0;						\
    y3 ^= y1;						\
    y1  = x2 | y3;					\
    y1 ^= x0;						\
    y2  = x0 & x1;					\
    x2 ^= y2;						\
    y2 |= x3;						\
    y1 ^= y2;						\
    y2  = x1 | y0; /* t04 */				\
    y2 &= y3;						\
    x1 ^= y1;						\
    y2 |= x1;						\
    y2 ^= x0;						\
    y0 ^= y1;						\
    x3  = ~x3;     /* t02 */				\
    y0 |= x3;						\
    y0 ^= x2;						\
  } while (0)

/* In-place linear transformation.  */
#define LINEAR_TRANSFORMATION(x0,x1,x2,x3)		 \
  do {                                                   \
    x0 = ROTL32 (13, x0);                    \
    x2 = ROTL32 (3, x2);                     \
    x1 = x1 ^ x0 ^ x2;        \
    x3 = x3 ^ x2 ^ (x0 << 3); \
    x1 = ROTL32 (1, x1);                     \
    x3 = ROTL32 (7, x3);                     \
    x0 = x0 ^ x1 ^ x3;        \
    x2 = x2 ^ x3 ^ (x1 << 7); \
    x0 = ROTL32 (5, x0);                     \
    x2 = ROTL32 (22, x2);                    \
  } while (0)

/* Round inputs are x0,x1,x2,x3 (destroyed), and round outputs are
   y0,y1,y2,y3. */
#define ROUND(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {						       \
    KEYXOR(x0,x1,x2,x3, subkey);		       \
    SBOX##which(x0,x1,x2,x3, y0,y1,y2,y3);	       \
    LINEAR_TRANSFORMATION(y0,y1,y2,y3);		       \
  } while (0)

#if HAVE_NATIVE_64_BIT

#define LINEAR_TRANSFORMATION64(x0,x1,x2,x3)		 \
  do {                                                   \
    x0 = DROTL32 (13, x0);                    \
    x2 = DROTL32 (3, x2);                     \
    x1 = x1 ^ x0 ^ x2;        \
    x3 = x3 ^ x2 ^ DRSHIFT32(3, x0);	    \
    x1 = DROTL32 (1, x1);                     \
    x3 = DROTL32 (7, x3);                     \
    x0 = x0 ^ x1 ^ x3;        \
    x2 = x2 ^ x3 ^ DRSHIFT32(7, x1);	    \
    x0 = DROTL32 (5, x0);                     \
    x2 = DROTL32 (22, x2);                    \
  } while (0)

#define ROUND64(which, subkey, x0,x1,x2,x3, y0,y1,y2,y3) \
  do {						       \
    KEYXOR64(x0,x1,x2,x3, subkey);		       \
    SBOX##which(x0,x1,x2,x3, y0,y1,y2,y3);	       \
    LINEAR_TRANSFORMATION64(y0,y1,y2,y3);		       \
  } while (0)

#endif /* HAVE_NATIVE_64_BIT */

void
serpent_encrypt (const struct serpent_ctx *ctx,
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

      for (k = 0; ; k += 8)
	{
	  ROUND (0, ctx->keys[k+0], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (1, ctx->keys[k+1], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (2, ctx->keys[k+2], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (3, ctx->keys[k+3], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (4, ctx->keys[k+4], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND (5, ctx->keys[k+5], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND (6, ctx->keys[k+6], x0,x1,x2,x3, y0,y1,y2,y3);
	  if (k == 24)
	    break;
	  ROUND (7, ctx->keys[k+7], y0,y1,y2,y3, x0,x1,x2,x3);
	}

      /* Special final round, using two subkeys. */
      KEYXOR (y0,y1,y2,y3, ctx->keys[31]);
      SBOX7 (y0,y1,y2,y3, x0,x1,x2,x3);
      KEYXOR (x0,x1,x2,x3, ctx->keys[32]);
    
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

      for (k = 0; ; k += 8)
	{
	  ROUND64 (0, ctx->keys[k+0], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (1, ctx->keys[k+1], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (2, ctx->keys[k+2], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (3, ctx->keys[k+3], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (4, ctx->keys[k+4], x0,x1,x2,x3, y0,y1,y2,y3);
	  ROUND64 (5, ctx->keys[k+5], y0,y1,y2,y3, x0,x1,x2,x3);
	  ROUND64 (6, ctx->keys[k+6], x0,x1,x2,x3, y0,y1,y2,y3);
	  if (k == 24)
	    break;
	  ROUND64 (7, ctx->keys[k+7], y0,y1,y2,y3, x0,x1,x2,x3);
	}

      /* Special final round, using two subkeys. */
      KEYXOR64 (y0,y1,y2,y3, ctx->keys[31]);
      SBOX7 (y0,y1,y2,y3, x0,x1,x2,x3);
      KEYXOR64 (x0,x1,x2,x3, ctx->keys[32]);
    
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
