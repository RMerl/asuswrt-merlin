/* Test program for multi-register variable.
   Copyright 2003, 2007 Free Software Foundation, Inc.

   This file is part of the gdb testsuite.

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
 
   This file was written by Michael Elizabeth Chastain (mec@shout.net).  */

struct s_2_by_4
{
  int field_0;
  int field_1;
};

void marker (struct s_2_by_4 s_whatever)
{
  s_whatever = s_whatever;
  return;
}

void foo ()
{
  /* I want this variable in a register but I can't really force it */
  register struct s_2_by_4 s24;
  s24.field_0 = 1170;
  s24.field_1 = 64701;
  marker (s24);
  return;
}

int main ()
{
  foo ();
}
