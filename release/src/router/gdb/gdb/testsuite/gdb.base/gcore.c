/* Copyright 2002, 2004, 2007 Free Software Foundation, Inc.

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
 * Test GDB's ability to save and reload a corefile.
 */

#include <stdlib.h>
#include <string.h>

int extern_array[4] = {1, 2, 3, 4};
static int static_array[4] = {5, 6, 7, 8};
static int un_initialized_array[4];
static char *heap_string;

void 
terminal_func ()
{
  return;
}

void
array_func ()
{
  int local_array[4];
  int i;

  heap_string = (char *) malloc (80);
  strcpy (heap_string, "I'm a little teapot, short and stout...");
  for (i = 0; i < 4; i++)
    {
      un_initialized_array[i] = extern_array[i] + 8;
      local_array[i] = extern_array[i] + 12;
    }
  terminal_func ();
}

#ifdef PROTOTYPES
int factorial_func (int value)
#else
int factorial_func (value)
     int value;
#endif
{
  if (value > 1) {
    value *= factorial_func (value - 1);
  }
  array_func ();
  return (value);
}

main()
{
  factorial_func (6);
  return 0;
}
