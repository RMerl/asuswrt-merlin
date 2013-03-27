/* Manage register sets.

   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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
#include "regset.h"

#include "gdb_assert.h"

/* Allocate a fresh 'struct regset' whose supply_regset function is
   SUPPLY_REGSET, and whose collect_regset function is COLLECT_REGSET.
   If the regset has no collect_regset function, pass NULL for
   COLLECT_REGSET.

   The object returned is allocated on ARCH's obstack.  */

struct regset *
regset_alloc (struct gdbarch *arch,
              supply_regset_ftype *supply_regset,
              collect_regset_ftype *collect_regset)
{
  struct regset *regset = GDBARCH_OBSTACK_ZALLOC (arch, struct regset);

  regset->arch = arch;
  regset->supply_regset = supply_regset;
  regset->collect_regset = collect_regset;

  return regset;
}
