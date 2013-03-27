/* Copyright 1999, 2000, 2004, 2007 Free Software Foundation, Inc.

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

/*
 *	This simple program that passes different types of arguments
 *      on function calls.  Useful to test printing frames, stepping, etc.
 */

#include <stdio.h>

int callee4 (void)
{
  int A=1;
  int B=2;
  int C;

  C = A + B;
  return 0;
}
callee3 (char *strarg)
{
  callee4 ();
}

callee2 (int intarg, char *strarg)
{
  callee3 (strarg);
}

callee1 (int intarg, char *strarg, double fltarg)
{
  callee2 (intarg, strarg);
}

main ()
{
  callee1 (2, "A string argument.", 3.5);
  callee1 (2, "A string argument.", 3.5);

  printf ("Hello, World!");

  return 0;
}

/*
Local variables: 
change-log-default-name: "ChangeLog-mi"
End: 
*/

