/* Test resolving of an opaque type from the loaded shared library.

   Copyright 2007, Free Software Foundation, Inc.

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

struct struct_libtype_opaque;

struct struct_libtype_empty
  {
  };    

struct struct_libtype_filled
  {
    long mainfield_filled;
  };    

union union_libtype_opaque;

union union_libtype_empty
  {
  };    

union union_libtype_filled
  {
    long mainfield_filled;
  };    

int main (void)
{
  volatile struct struct_libtype_opaque *pointer_struct_opaque = (void *) 0;
  volatile struct struct_libtype_empty *pointer_struct_empty = (void *) 0;
  volatile struct struct_libtype_filled *pointer_struct_filled = (void *) 0;
  volatile union union_libtype_opaque *pointer_union_opaque = (void *) 0;
  volatile union union_libtype_empty *pointer_union_empty = (void *) 0;
  volatile union union_libtype_filled *pointer_union_filled = (void *) 0;

  return (long) pointer_struct_opaque + (long) pointer_struct_empty
	 + (long) pointer_struct_filled + (long) pointer_union_opaque
	 + (long) pointer_union_empty + (long) pointer_union_filled;
}
