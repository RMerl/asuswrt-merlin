/* Builtin frame register, for GDB, the GNU debugger.

   Copyright (C) 2002, 2005, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat.

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
#include "user-regs.h"
#include "frame.h"
#include "gdbtypes.h"
#include "value.h"
#include "gdb_string.h"


static struct value *
value_of_builtin_frame_fp_reg (struct frame_info *frame, const void *baton)
{
  if (gdbarch_deprecated_fp_regnum (current_gdbarch) >= 0)
    /* NOTE: cagney/2003-04-24: Since the mere presence of "fp" in the
       register name table overrides this built-in $fp register, there
       is no real reason for this gdbarch_deprecated_fp_regnum trickery here.
       An architecture wanting to implement "$fp" as alias for a raw
       register can do so by adding "fp" to register name table (mind
       you, doing this is probably a dangerous thing).  */
    return value_of_register (gdbarch_deprecated_fp_regnum (current_gdbarch),
			      frame);
  else
    {
      struct value *val = allocate_value (builtin_type_void_data_ptr);
      gdb_byte *buf = value_contents_raw (val);
      if (frame == NULL)
	memset (buf, 0, TYPE_LENGTH (value_type (val)));
      else
	gdbarch_address_to_pointer (current_gdbarch, builtin_type_void_data_ptr,
				    buf, get_frame_base_address (frame));
      return val;
    }
}

static struct value *
value_of_builtin_frame_pc_reg (struct frame_info *frame, const void *baton)
{
  if (gdbarch_pc_regnum (current_gdbarch) >= 0)
    return value_of_register (gdbarch_pc_regnum (current_gdbarch), frame);
  else
    {
      struct value *val = allocate_value (builtin_type_void_data_ptr);
      gdb_byte *buf = value_contents_raw (val);
      if (frame == NULL)
	memset (buf, 0, TYPE_LENGTH (value_type (val)));
      else
	gdbarch_address_to_pointer (current_gdbarch, builtin_type_void_data_ptr,
				     buf, get_frame_pc (frame));
      return val;
    }
}

static struct value *
value_of_builtin_frame_sp_reg (struct frame_info *frame, const void *baton)
{
  if (gdbarch_sp_regnum (current_gdbarch) >= 0)
    return value_of_register (gdbarch_sp_regnum (current_gdbarch), frame);
  error (_("Standard register ``$sp'' is not available for this target"));
}

static struct value *
value_of_builtin_frame_ps_reg (struct frame_info *frame, const void *baton)
{
  if (gdbarch_ps_regnum (current_gdbarch) >= 0)
    return value_of_register (gdbarch_ps_regnum (current_gdbarch), frame);
  error (_("Standard register ``$ps'' is not available for this target"));
}

extern initialize_file_ftype _initialize_frame_reg; /* -Wmissing-prototypes */

void
_initialize_frame_reg (void)
{
  /* Frame based $fp, $pc, $sp and $ps.  These only come into play
     when the target does not define its own version of these
     registers.  */
  user_reg_add_builtin ("fp", value_of_builtin_frame_fp_reg, NULL);
  user_reg_add_builtin ("pc", value_of_builtin_frame_pc_reg, NULL);
  user_reg_add_builtin ("sp", value_of_builtin_frame_sp_reg, NULL);
  user_reg_add_builtin ("ps", value_of_builtin_frame_ps_reg, NULL);
}
