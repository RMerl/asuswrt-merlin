/* Abstraction of GNU v2 abi.

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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

#ifndef GNU_V2_ABI_H
#define GNU_V2_ABI_H

extern int gnuv2_baseclass_offset (struct type *type, int index,
				   const bfd_byte *valaddr,
				   CORE_ADDR address);

#endif
