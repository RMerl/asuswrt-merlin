/* gcmdata.c

   Galois counter mode, specified by NIST,
   http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf

   Generation of fixed multiplication tables.

   Copyright (C) 2011 Niels Möller

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

#include <stdio.h>
#include <stdlib.h>

#define GHASH_POLYNOMIAL 0xE1


/* When x is shifted out over the block edge, add multiples of the
   defining polynomial to eliminate each bit. */
static unsigned
reduce(unsigned x)
{
  unsigned p = GHASH_POLYNOMIAL << 1;
  unsigned y = 0;
  for (; x; x >>= 1, p <<= 1)
    if (x & 1)
      y ^= p;
  return y;
}

int
main(int argc, char **argv)
{
  unsigned i;
  printf("4-bit table:\n");
  
  for (i = 0; i<16; i++)
    {
      unsigned x;
      if (i && !(i%8))
	printf("\n");

      x = reduce(i << 4);
      printf("W(%02x,%02x),", x >> 8, x & 0xff);
    }
  printf("\n\n");
  printf("8-bit table:\n");
  for (i = 0; i<256; i++)
    {
      unsigned x;
      if (i && !(i%8))
	printf("\n");

      x = reduce(i);
      printf("W(%02x,%02x),", x >> 8, x & 0xff);
    }
  printf("\n");
  return EXIT_SUCCESS;
}
