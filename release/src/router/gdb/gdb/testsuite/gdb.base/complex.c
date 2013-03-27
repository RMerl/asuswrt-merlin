/* Copyright 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

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

/* Test taken from GCC.  Verify that we can print a structure containing
   a complex number.  */

#include <stdlib.h>

typedef __complex__ float cf;
struct x { char c; cf f; } __attribute__ ((__packed__));
struct unpacked_x { char c; cf f; };
extern void f4 (struct unpacked_x*);
extern void f3 (void);
extern void f2 (struct x*);
extern void f1 (void);
int
main (void)
{
  f1 ();
  f3 ();
  exit (0);
}

void
f1 (void)
{
  struct x s;
  s.f = 1;
  s.c = 42;
  f2 (&s);
}

void
f2 (struct x *y)
{
  if (y->f != 1 || y->c != 42)
    abort ();
}

void
f3 (void)
{
  struct unpacked_x s;
  s.f = 1;
  s.c = 42;
  f4 (&s);
}

void
f4 (struct unpacked_x *y)
{
  if (y->f != 1 || y->c != 42)
    abort ();
}
