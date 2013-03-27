/* Target-dependent code for OSF/1 on Alpha.
   Copyright (C) 2002, 2003, 2007 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "value.h"
#include "osabi.h"
#include "gdb_string.h"
#include "objfiles.h"

#include "alpha-tdep.h"

static int
alpha_osf1_pc_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  return (func_name != NULL && strcmp ("__sigtramp", func_name) == 0);
}

static CORE_ADDR
alpha_osf1_sigcontext_addr (struct frame_info *next_frame)
{
  const struct frame_id next_id = get_frame_id (next_frame);

  return (read_memory_integer (next_id.stack_addr, 8));
}

static void
alpha_osf1_init_abi (struct gdbarch_info info,
                     struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Hook into the MDEBUG frame unwinder.  */
  alpha_mdebug_init_abi (info, gdbarch);

  /* The next/step support via procfs on OSF1 is broken when running
     on multi-processor machines. We need to use software single stepping
     instead.  */
  set_gdbarch_software_single_step (gdbarch, alpha_software_single_step);

  tdep->sigcontext_addr = alpha_osf1_sigcontext_addr;
  tdep->pc_in_sigtramp = alpha_osf1_pc_in_sigtramp;

  tdep->jb_pc = 2;
  tdep->jb_elt_size = 8;
}

void
_initialize_alpha_osf1_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_alpha, 0, GDB_OSABI_OSF1,
			  alpha_osf1_init_abi);
}
