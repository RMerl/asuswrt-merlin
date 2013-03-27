/* Definitions for targets without shared libraries for GDB, the GNU Debugger.

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
#include "solist.h"

static struct so_list *
null_current_sos (void)
{
  return NULL;
}

static void
null_special_symbol_handling (void)
{
}

static void
null_solib_create_inferior_hook (void)
{
}

static void
null_clear_solib (void)
{
}

static void
null_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}


static void
null_relocate_section_addresses (struct so_list *so,
                                 struct section_table *sec)
{
}

static int
null_open_symbol_file_object (void *from_ttyp)
{
  return 0;
}

static int
null_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return 0;
}

static struct target_so_ops null_so_ops;

extern initialize_file_ftype _initialize_null_solib; /* -Wmissing-prototypes */

void
_initialize_null_solib (void)
{
  null_so_ops.relocate_section_addresses = null_relocate_section_addresses;
  null_so_ops.free_so = null_free_so;
  null_so_ops.clear_solib = null_clear_solib;
  null_so_ops.solib_create_inferior_hook = null_solib_create_inferior_hook;
  null_so_ops.special_symbol_handling = null_special_symbol_handling;
  null_so_ops.current_sos = null_current_sos;
  null_so_ops.open_symbol_file_object = null_open_symbol_file_object;
  null_so_ops.in_dynsym_resolve_code = null_in_dynsym_resolve_code;

  /* Set current_target_so_ops to null_so_ops if not already set.  */
  if (current_target_so_ops == 0)
    current_target_so_ops = &null_so_ops;
}
