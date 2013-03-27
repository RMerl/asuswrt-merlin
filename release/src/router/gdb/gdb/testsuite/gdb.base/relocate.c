/* This testcase is part of GDB, the GNU debugger.

   Copyright 2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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

static int static_foo = 1;
static int static_bar = 2;

int global_foo = 3;
int global_bar = 4;

int
function_foo ()
{
  return 5;
}

int
function_bar ()
{
  return 6;
}
