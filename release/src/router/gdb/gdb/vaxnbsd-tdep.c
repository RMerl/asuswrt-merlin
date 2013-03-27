/* Target-dependent code for NetBSD/vax.

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
#include "arch-utils.h"
#include "osabi.h"

#include "vax-tdep.h"
#include "solib-svr4.h"

#include "gdb_string.h"

/* NetBSD a.out.  */

static void
vaxnbsd_aout_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
}

/* NetBSD ELF.  */

static void
vaxnbsd_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* NetBSD ELF uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_vaxnbsd_tdep (void);

void
_initialize_vaxnbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_NETBSD_AOUT,
			  vaxnbsd_aout_init_abi);
  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_NETBSD_ELF,
			  vaxnbsd_elf_init_abi);
}
