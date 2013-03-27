/* This test script is part of GDB, the GNU debugger.

   Copyright 1999, 2004,
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   */

/* Test long long expression; test printing in general.
 *
 * /CLO/BUILD_ENV/Exports/cc -g +e -o long_long long_long.c
 *
 * or
 *
 * cc +e +DA2.0 -g -o long_long long_long.c
 */

#include <string.h>

enum { MAX_BYTES = 16 };

void
pack (unsigned char b[MAX_BYTES], int size, int nr)
{
  static long long val[] = { 0x123456789abcdefLL, 01234567123456701234567LL, 12345678901234567890ULL};
  volatile static int e = 1;
  int i;
  for (i = 0; i < nr; i++)
    {
      int offset;
      if (*(char *)&e)
	/* Little endian.  */
	offset = sizeof (long long) - size;
      else
	/* Big endian endian.  */
	offset = 0;
      memcpy (b + size * i, (char *) val + sizeof (long long) * i + offset, size);
    }
}

unsigned char b[MAX_BYTES];
unsigned char h[MAX_BYTES];
unsigned char w[MAX_BYTES];
unsigned char g[MAX_BYTES];

unsigned char c[MAX_BYTES];
unsigned char s[MAX_BYTES];
unsigned char i[MAX_BYTES];
unsigned char l[MAX_BYTES];
unsigned char ll[MAX_BYTES];

int known_types()
{
  /* A union is used here as, hopefully it has well defined packing
     rules.  */
  struct {
    long long bin, oct, dec, hex;    
  } val;
  memset (&val, 0, sizeof val);

  /* Known values, filling the full 64 bits.  */
  val.bin = 0x123456789abcdefLL; /* 64 bits = 16 hex digits */
  val.oct = 01234567123456701234567LL; /*  = 21+ octal digits */
  val.dec = 12345678901234567890ULL;    /*  = 19+ decimal digits */

  /* Stop here and look!  */
  val.hex = val.bin - val.dec | val.oct;

  return 0;
}

int main() {

   /* Pack Byte, Half, Word and Giant arrays with byte-orderd values.
      That way "(gdb) x" gives the same output on different
      architectures.  */
   pack (b, 1, 2);
   pack (h, 2, 2);
   pack (w, 4, 2);
   pack (g, 8, 2);
   pack (c, sizeof (char), 2);
   pack (s, sizeof (short), 2);
   pack (i, sizeof (int), 2);
   pack (l, sizeof (long), 2);
   pack (ll, sizeof (long long), 2);

   known_types();
   
   return 0;
}
