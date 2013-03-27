/* Target-dependent code for Windows CE running on ARM processors,
   for GDB.

   Copyright (C) 2007 Free Software Foundation, Inc.

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
#include "osabi.h"
#include "gdbcore.h"
#include "target.h"

#include "gdb_string.h"

#include "arm-tdep.h"

static const char arm_wince_le_breakpoint[] = { 0x10, 0x00, 0x00, 0xe6 };
static const char arm_wince_thumb_le_breakpoint[] = { 0xfe, 0xdf };

/* Description of the longjmp buffer.  */
#define ARM_WINCE_JB_ELEMENT_SIZE	INT_REGISTER_SIZE
#define ARM_WINCE_JB_PC			10

static CORE_ADDR
arm_pe_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  ULONGEST indirect;
  struct minimal_symbol *indsym;
  char *symname;
  CORE_ADDR next_pc;

  /* The format of an ARM DLL trampoline is:
       ldr  ip, [pc]
       ldr  pc, [ip]
       .dw __imp_<func>  */

  if (pc == 0
      || read_memory_unsigned_integer (pc + 0, 4) != 0xe59fc000
      || read_memory_unsigned_integer (pc + 4, 4) != 0xe59cf000)
    return 0;

  indirect = read_memory_unsigned_integer (pc + 8, 4);
  if (indirect == 0)
    return 0;

  indsym = lookup_minimal_symbol_by_pc (indirect);
  if (indsym == NULL)
    return 0;

  symname = SYMBOL_LINKAGE_NAME (indsym);
  if (symname == NULL || strncmp (symname, "__imp_", 6) != 0)
    return 0;

  next_pc = read_memory_unsigned_integer (indirect, 4);
  if (next_pc != 0)
    return next_pc;

  /* Check with the default arm gdbarch_skip_trampoline.  */
  return arm_skip_stub (frame, pc);
}

static void
arm_wince_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->arm_breakpoint = arm_wince_le_breakpoint;
  tdep->arm_breakpoint_size = sizeof (arm_wince_le_breakpoint);
  tdep->thumb_breakpoint = arm_wince_thumb_le_breakpoint;
  tdep->thumb_breakpoint_size = sizeof (arm_wince_thumb_le_breakpoint);
  tdep->struct_return = pcc_struct_return;

  tdep->fp_model = ARM_FLOAT_SOFT_VFP;

  tdep->jb_pc = ARM_WINCE_JB_PC;
  tdep->jb_elt_size = ARM_WINCE_JB_ELEMENT_SIZE;

  /* On ARM WinCE char defaults to signed.  */
  set_gdbarch_char_signed (gdbarch, 1);

  /* Shared library handling.  */
  set_gdbarch_skip_trampoline_code (gdbarch, arm_pe_skip_trampoline_code);

  /* Single stepping.  */
  set_gdbarch_software_single_step (gdbarch, arm_software_single_step);
}

static enum gdb_osabi
arm_wince_osabi_sniffer (bfd *abfd)
{
  const char *target_name = bfd_get_target (abfd);

  if (strcmp (target_name, "pei-arm-wince-little") == 0)
    return GDB_OSABI_WINCE;

  return GDB_OSABI_UNKNOWN;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_arm_wince_tdep (void);

void
_initialize_arm_wince_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_arm, bfd_target_coff_flavour,
                                  arm_wince_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_arm, 0, GDB_OSABI_WINCE,
                          arm_wince_init_abi);
}
