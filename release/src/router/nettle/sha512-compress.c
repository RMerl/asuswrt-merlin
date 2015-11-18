/* sha512-compress.c

   The compression function of the sha512 hash function.

   Copyright (C) 2001, 2010 Niels Möller

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

#ifndef SHA512_DEBUG
# define SHA512_DEBUG 0
#endif

#if SHA512_DEBUG
# include <stdio.h>
# define DEBUG(i) \
  fprintf(stderr, "%2d: %8lx %8lx %8lx %8lx\n    %8lx %8lx %8lx %8lx\n", \
	  i, A, B, C, D ,E, F, G, H)
#else
# define DEBUG(i)
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sha2.h"

#include "macros.h"

/* A block, treated as a sequence of 64-bit words. */
#define SHA512_DATA_LENGTH 16

/* For fat builds */
#if HAVE_NATIVE_sha512_compress
void
_nettle_sha512_compress_c (uint64_t *state, const uint8_t *input, const uint64_t *k);
#define _nettle_sha512_compress _nettle_sha512_compress_c
#endif

/* The SHA512 functions. The Choice function is the same as the SHA1
   function f1, and the majority function is the same as the SHA1 f3
   function, and the same as for SHA256. */

#define Choice(x,y,z)   ( (z) ^ ( (x) & ( (y) ^ (z) ) ) ) 
#define Majority(x,y,z) ( ((x) & (y)) ^ ((z) & ((x) ^ (y))) )

#define S0(x) (ROTL64(36,(x)) ^ ROTL64(30,(x)) ^ ROTL64(25,(x))) 
#define S1(x) (ROTL64(50,(x)) ^ ROTL64(46,(x)) ^ ROTL64(23,(x)))

#define s0(x) (ROTL64(63,(x)) ^ ROTL64(56,(x)) ^ ((x) >> 7))
#define s1(x) (ROTL64(45,(x)) ^ ROTL64(3,(x)) ^ ((x) >> 6))

/* The initial expanding function. The hash function is defined over
   an 64-word expanded input array W, where the first 16 are copies of
   the input data, and the remaining 64 are defined by

        W[ t ] = s1(W[t-2]) + W[t-7] + s0(W[i-15]) + W[i-16]

   This implementation generates these values on the fly in a circular
   buffer.
*/

#define EXPAND(W,i) \
( W[(i) & 15 ] += (s1(W[((i)-2) & 15]) + W[((i)-7) & 15] + s0(W[((i)-15) & 15])) )

/* The prototype SHA sub-round.  The fundamental sub-round is:

        T1 = h + S1(e) + Choice(e,f,g) + K[t] + W[t]
	T2 = S0(a) + Majority(a,b,c)
	a' = T1+T2
	b' = a
	c' = b
	d' = c
	e' = d + T1
	f' = e
	g' = f
	h' = g

   but this is implemented by unrolling the loop 8 times and renaming
   the variables
   ( h, a, b, c, d, e, f, g ) = ( a, b, c, d, e, f, g, h ) each
   iteration. This code is then replicated 8, using the next 8 values
   from the W[] array each time */

/* It's crucial that DATA is only used once, as that argument will
 * have side effects. */
#define ROUND(a,b,c,d,e,f,g,h,k,data) do {	\
  h += S1(e) + Choice(e,f,g) + k + data;	\
  d += h;					\
  h += S0(a) + Majority(a,b,c);			\
} while (0)

void
_nettle_sha512_compress(uint64_t *state, const uint8_t *input, const uint64_t *k)
{
  uint64_t data[SHA512_DATA_LENGTH];
  uint64_t A, B, C, D, E, F, G, H;     /* Local vars */
  unsigned i;
  uint64_t *d;

  for (i = 0; i < SHA512_DATA_LENGTH; i++, input += 8)
    {
      data[i] = READ_UINT64(input);
    }

  /* Set up first buffer and local data buffer */
  A = state[0];
  B = state[1];
  C = state[2];
  D = state[3];
  E = state[4];
  F = state[5];
  G = state[6];
  H = state[7];
  
  /* Heavy mangling */
  /* First 16 subrounds that act on the original data */

  DEBUG(-1);
  for (i = 0, d = data; i<16; i+=8, k += 8, d+= 8)
    {
      ROUND(A, B, C, D, E, F, G, H, k[0], d[0]); DEBUG(i);
      ROUND(H, A, B, C, D, E, F, G, k[1], d[1]); DEBUG(i+1);
      ROUND(G, H, A, B, C, D, E, F, k[2], d[2]);
      ROUND(F, G, H, A, B, C, D, E, k[3], d[3]);
      ROUND(E, F, G, H, A, B, C, D, k[4], d[4]);
      ROUND(D, E, F, G, H, A, B, C, k[5], d[5]);
      ROUND(C, D, E, F, G, H, A, B, k[6], d[6]); DEBUG(i+6);
      ROUND(B, C, D, E, F, G, H, A, k[7], d[7]); DEBUG(i+7);
    }
  
  for (; i<80; i += 16, k+= 16)
    {
      ROUND(A, B, C, D, E, F, G, H, k[ 0], EXPAND(data,  0)); DEBUG(i);
      ROUND(H, A, B, C, D, E, F, G, k[ 1], EXPAND(data,  1)); DEBUG(i+1);
      ROUND(G, H, A, B, C, D, E, F, k[ 2], EXPAND(data,  2)); DEBUG(i+2);
      ROUND(F, G, H, A, B, C, D, E, k[ 3], EXPAND(data,  3));
      ROUND(E, F, G, H, A, B, C, D, k[ 4], EXPAND(data,  4));
      ROUND(D, E, F, G, H, A, B, C, k[ 5], EXPAND(data,  5));
      ROUND(C, D, E, F, G, H, A, B, k[ 6], EXPAND(data,  6));
      ROUND(B, C, D, E, F, G, H, A, k[ 7], EXPAND(data,  7));
      ROUND(A, B, C, D, E, F, G, H, k[ 8], EXPAND(data,  8));
      ROUND(H, A, B, C, D, E, F, G, k[ 9], EXPAND(data,  9));
      ROUND(G, H, A, B, C, D, E, F, k[10], EXPAND(data, 10));
      ROUND(F, G, H, A, B, C, D, E, k[11], EXPAND(data, 11));
      ROUND(E, F, G, H, A, B, C, D, k[12], EXPAND(data, 12));
      ROUND(D, E, F, G, H, A, B, C, k[13], EXPAND(data, 13));
      ROUND(C, D, E, F, G, H, A, B, k[14], EXPAND(data, 14)); DEBUG(i+14);
      ROUND(B, C, D, E, F, G, H, A, k[15], EXPAND(data, 15)); DEBUG(i+15);
    }

  /* Update state */
  state[0] += A;
  state[1] += B;
  state[2] += C;
  state[3] += D;
  state[4] += E;
  state[5] += F;
  state[6] += G;
  state[7] += H;
#if SHA512_DEBUG
  fprintf(stderr, "99: %8lx %8lx %8lx %8lx\n    %8lx %8lx %8lx %8lx\n",
	  state[0], state[1], state[2], state[3],
	  state[4], state[5], state[6], state[7]);
#endif
}
