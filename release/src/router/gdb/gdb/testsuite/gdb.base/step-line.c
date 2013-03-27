/* Test step/next in presence of #line directives.
   Copyright 2001, 2007
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



void dummy (int, int);
int f1 (int);
int f2 (int);

int
main (int argc, char **argv)
{
  int i;
  i = f1 (4);
  i = f1 (i);
  dummy (0, i);
  return 0;
}

int
f1 (int i)
{
#line 40 "step-line.c"
  dummy (1, i);
#line 24 "step-line.inp"
  i = f2 (i);
#line 44 "step-line.c"
  dummy (2, i);
#line 25 "step-line.inp"
  i = f2 (i);
#line 48 "step-line.c"
  dummy (3, i);
#line 26 "step-line.inp"
  return i;
#line 52 "step-line.c"
}

int
f2 (int i)
{
#line 31 "step-line.inp"
  int j;
#line 60 "step-line.c"
  dummy (4, i);
#line 32 "step-line.inp"
  j = i;
#line 64 "step-line.c"
  dummy (5, i);
  dummy (6, j);
#line 33 "step-line.inp"
  j = j + 1;
#line 69 "step-line.c"
  dummy (7, i);
  dummy (8, j);
#line 34 "step-line.inp"
  j = j - i;
#line 74 "step-line.c"
  dummy (9, i);
  dummy (10, j);
#line 35 "step-line.inp"
  return i;
#line 79 "step-line.c"
}

void
dummy (int num, int i)
{
}
