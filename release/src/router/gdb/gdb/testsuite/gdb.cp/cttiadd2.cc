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

template<class T> T add2(T v1, T v2)
{
   T v3;
   v3 = v1;
   v3 += v2;
   return v3;
}

void subr2()
{
  unsigned char c;
  int i;
  float f;
  
  c = 'b';
  i = 3;
  f = 6.5;

  c = add2(c, c);
  i = add2(i, i);
  f = add2(f, f);
}
