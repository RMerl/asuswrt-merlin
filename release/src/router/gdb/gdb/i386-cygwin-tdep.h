/* Target-dependent code for Cygwin running on i386's, for GDB.

   Copyright (C) 2007
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

#ifndef I386_CYGWIN_TDEP_H
#define I386_CYGWIN_TDEP_H

struct obstack;

extern void win32_xfer_shared_library (const char* so_name,
				       CORE_ADDR load_addr,
				       struct obstack *obstack);

#endif /* I386_CYGWIN_TDEP_H */
