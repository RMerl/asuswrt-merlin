/* This test script is part of GDB, the GNU debugger.

   Copyright 2002, 2004, 2007 Free Software Foundation, Inc.

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

/*
  An attempt to replicate PR gdb/574 with a shorter program.

  Printing out *theB failed if the program was compiled with GCC 2.95.
*/

class A {
public:
  virtual void foo() {};		// Stick in a virtual function.
  int a;				// Stick in a data member.
};

class B : public A {
  static int b;				// Stick in a static data member.
};

int main()
{
  B *theB = new B;

  return 0;				// breakpoint: constructs-done
}
