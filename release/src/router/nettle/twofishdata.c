/* twofishdata.c

   Generates the permutations q0 and q1 for twofish.

   Copyright (C) 1999 Ruud de Rooij <ruud@debian.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02111-1301  USA.
 */

#include <stdio.h>

#define ror4(x) (((x) >> 1) | (((x) & 1) << 3))

static unsigned char q0(unsigned char x)
{
    static const unsigned char t0[16] = {
      0x8, 0x1, 0x7, 0xD, 0x6, 0xF, 0x3, 0x2,
      0x0, 0xB, 0x5, 0x9, 0xE, 0xC, 0xA, 0x4
    };
    static const unsigned char t1[16] = {
      0xE, 0xC, 0xB, 0x8, 0x1, 0x2, 0x3, 0x5,
      0xF, 0x4, 0xA, 0x6, 0x7, 0x0, 0x9, 0xD
    };
    static const unsigned char t2[16] = {
      0xB, 0xA, 0x5, 0xE, 0x6, 0xD, 0x9, 0x0,
      0xC, 0x8, 0xF, 0x3, 0x2, 0x4, 0x7, 0x1
    };
    static const unsigned char t3[16] = {
      0xD, 0x7, 0xF, 0x4, 0x1, 0x2, 0x6, 0xE,
      0x9, 0xB, 0x3, 0x0, 0x8, 0x5, 0xC, 0xA
    };

    unsigned char a0 = x / 16;
    unsigned char b0 = x % 16;

    unsigned char a1 = a0 ^ b0;
    unsigned char b1 = a0 ^ ror4(b0) ^ ((8*a0) % 16);

    unsigned char a2 = t0[a1];
    unsigned char b2 = t1[b1];

    unsigned char a3 = a2 ^ b2;
    unsigned char b3 = a2 ^ ror4(b2) ^ ((8*a2) % 16);

    unsigned char a4 = t2[a3];
    unsigned char b4 = t3[b3];

    unsigned char y = 16*b4 + a4;

    return y;
}

static unsigned char q1(unsigned char x)
{
  static const unsigned char t0[16] = {
    0x2, 0x8, 0xB, 0xD, 0xF, 0x7, 0x6, 0xE,
    0x3, 0x1, 0x9, 0x4, 0x0, 0xA, 0xC, 0x5
  };
  static const unsigned char t1[16] = {
    0x1, 0xE, 0x2, 0xB, 0x4, 0xC, 0x3, 0x7,
    0x6, 0xD, 0xA, 0x5, 0xF, 0x9, 0x0, 0x8
  };
  static const unsigned char t2[16] = {
    0x4, 0xC, 0x7, 0x5, 0x1, 0x6, 0x9, 0xA,
    0x0, 0xE, 0xD, 0x8, 0x2, 0xB, 0x3, 0xF
  };
  static const unsigned char t3[16] = {
    0xB, 0x9, 0x5, 0x1, 0xC, 0x3, 0xD, 0xE,
    0x6, 0x4, 0x7, 0xF, 0x2, 0x0, 0x8, 0xA
  };

  unsigned char a0 = x / 16;
  unsigned char b0 = x % 16;

  unsigned char a1 = a0 ^ b0;
  unsigned char b1 = a0 ^ ror4(b0) ^ ((8*a0) % 16);

  unsigned char a2 = t0[a1];
  unsigned char b2 = t1[b1];

  unsigned char a3 = a2 ^ b2;
  unsigned char b3 = a2 ^ ror4(b2) ^ ((8*a2) % 16);

  unsigned char a4 = t2[a3];
  unsigned char b4 = t3[b3];

  unsigned char y = 16*b4 + a4;

  return y;
}

int
main(void)
{
  unsigned i;

  printf("static const uint8_t q0[256] = {");
  for (i = 0; i < 256; i++) {
    if ( (i % 8) == 0)
      printf("\n  ");
    printf("0x%02X,", q0(i));
  }
  printf("\n};\n\n");

  printf("static const uint8_t q1[256] = {");
  for (i = 0; i < 256; i++) {
    if ( (i % 8) == 0)
      printf("\n  ");
    printf("0x%02X,", q1(i));
  }
  printf("\n};\n");

  return 0;
}
