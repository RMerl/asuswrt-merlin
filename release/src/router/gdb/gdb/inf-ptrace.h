/* Low level child interface to ptrace.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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

#ifndef INF_PTRACE_H
#define INF_PTRACE_H

/* Create a prototype ptrace target.  The client can override it with
   local methods.  */

extern struct target_ops *inf_ptrace_target (void);

/* Create a "traditional" ptrace target.  REGISTER_U_OFFSET should be
   a function returning the offset within the user area where a
   particular register is stored.  */

extern struct target_ops *
  inf_ptrace_trad_target (CORE_ADDR (*register_u_offset)
					(struct gdbarch *, int, int));

#endif
