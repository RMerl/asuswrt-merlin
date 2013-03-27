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

struct struct_libtype_opaque
{ 
  int libfield_opaque;
};
struct struct_libtype_opaque struct_libtype_opaque_use;

struct struct_libtype_empty
{ 
  int libfield_empty;
};
struct struct_libtype_empty struct_libtype_empty_use;

struct struct_libtype_filled
{ 
  int libfield_filled;
};
struct struct_libtype_filled struct_libtype_filled_use;

union union_libtype_opaque
{ 
  int libfield_opaque;
};
union union_libtype_opaque union_libtype_opaque_use;

union union_libtype_empty
{ 
  int libfield_empty;
};
union union_libtype_empty union_libtype_empty_use;

union union_libtype_filled
{ 
  int libfield_filled;
};
union union_libtype_filled union_libtype_filled_use;
