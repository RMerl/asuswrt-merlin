/* Copyright 2007 Free Software Foundation, Inc.
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

   Contributed by Markus Deuling <deuling@de.ibm.com>.
*/
#include <stdio.h>

extern void foo();
void foo3();
void foo2();

int main ()
{
  printf ("in main\n");
  foo ();
  foo3 ();
  return 0;
}

void foo3()
{
  printf ("foo3 in main\n");
  return;
}

void foo2()
{
  printf ("foo2 in main\n");
  return;
}

