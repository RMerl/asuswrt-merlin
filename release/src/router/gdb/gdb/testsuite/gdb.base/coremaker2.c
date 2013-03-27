/* Copyright 1992, 1993, 1994, 1995, 1996, 1999, 2007
   Free Software Foundation, Inc.

   This file is part of GDB.

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

/* Simple little program that just generates a core dump from inside some
   nested function calls.  Keep this as self contained as possible, I.E.
   use no environment resources other than possibly abort(). */

#ifndef __STDC__
#define	const	/**/
#endif

#ifndef HAVE_ABORT
#define HAVE_ABORT 1
#endif

#if HAVE_ABORT
#define ABORT abort()
#else
#define ABORT {char *invalid = 0; *invalid = 0xFF;}
#endif

/* Don't make these automatic vars or we will have to walk back up the
   stack to access them. */

char *buf1;
char *buf2;

int coremaker_data = 1;	/* In Data section */
int coremaker_bss;	/* In BSS section */

const int coremaker_ro = 201;	/* In Read-Only Data section */

void
func2 (int x)
{
  int coremaker_local[5];
  int i;
  static int y;

  /* Make sure that coremaker_local doesn't get optimized away. */
  for (i = 0; i < 5; i++)
    coremaker_local[i] = i;
  coremaker_bss = 0;
  for (i = 0; i < 5; i++)
    coremaker_bss += coremaker_local[i];
  coremaker_data = coremaker_ro + 1;
  y = 10 * x;
  ABORT;
}

void
func1 (int x)
{
  func2 (x * 2);
}

int main ()
{
  func1 (10);
  return 0;
}
