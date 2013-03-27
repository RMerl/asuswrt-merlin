/* This testcase is part of GDB, the GNU debugger.

   Copyright 1998, 1999, 2004, 2007 Free Software Foundation, Inc.

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

template<class T> T add(T v1, T v2)
{
   T v3;
   v3 = v1;
   v3 += v2;
   return v3;
 }

int main()
{
  unsigned char c;
  int i;
  float f;
  extern void add1();
  extern void subr2();
  extern void subr3();
  
  c = 'a';
  i = 2;
  f = 4.5;

  c = add(c, c);
  i = add(i, i);
  f = add(f, f);

  // marker add1
  add1();
  subr2();
  subr3();
}
