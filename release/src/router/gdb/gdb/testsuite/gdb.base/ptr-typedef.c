/* This testcase is part of GDB, the GNU debugger.

   Copyright 2005, 2007 Free Software Foundation, Inc.

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

struct foo {
  int x;
};

typedef struct foo *foz;

struct foo *foo_ptr;
foz foz_ptr;

int
marker1 (void)
{
  return foo_ptr == foz_ptr;
}

int
main (void)
{
  struct foo the_foo;
  foo_ptr = &the_foo;
  foz_ptr = &the_foo;

  return marker1 ();
}
