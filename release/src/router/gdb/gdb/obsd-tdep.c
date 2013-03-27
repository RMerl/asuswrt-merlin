/* Target-dependent code for OpenBSD.

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

#include "defs.h"
#include "frame.h"
#include "symtab.h"

#include "obsd-tdep.h"

CORE_ADDR
obsd_skip_solib_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  struct minimal_symbol *msym;

  msym = lookup_minimal_symbol("_dl_bind", NULL, NULL);
  if (msym && SYMBOL_VALUE_ADDRESS (msym) == pc)
    return frame_pc_unwind (get_current_frame ());
  else
    return find_solib_trampoline_target (get_current_frame (), pc);
}
