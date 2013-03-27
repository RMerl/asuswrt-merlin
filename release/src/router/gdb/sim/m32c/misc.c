/* misc.c --- miscellaneous utility functions for M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <stdio.h>

#include "cpu.h"
#include "misc.h"

int
bcd2int (int bcd, int w)
{
  int v = 0, m = 1, i;
  for (i = 0; i < (w ? 4 : 2); i++)
    {
      v += (bcd % 16) * m;
      m *= 10;
      bcd /= 16;
    }
  return v;
}

int
int2bcd (int v, int w)
{
  int bcd = 0, m = 1, i;
  for (i = 0; i < (w ? 4 : 2); i++)
    {
      bcd += (v % 10) * m;
      m *= 16;
      v /= 10;
    }
  return bcd;
}

char *
comma (unsigned int u)
{
  static char buf[5][20];
  static int bi = 0;
  int comma = 0;
  char *bp;

  bi = (bi + 1) % 5;
  bp = buf[bi] + 19;
  *--bp = 0;
  do
    {
      if (comma == 3)
	{
	  *--bp = ',';
	  comma = 0;
	}
      comma++;
      *--bp = '0' + (u % 10);
      u /= 10;
    }
  while (u);
  return bp;
}
