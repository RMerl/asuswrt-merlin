/* Target-dependent code for OpenBSD/arm.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

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
#include "trad-frame.h"
#include "tramp-frame.h"

#include "gdb_string.h"

#include "obsd-tdep.h"
#include "arm-tdep.h"
#include "solib-svr4.h"

/* Signal trampolines.  */

static void
armobsd_sigframe_init (const struct tramp_frame *self,
		       struct frame_info *next_frame,
		       struct trad_frame_cache *cache,
		       CORE_ADDR func)
{
  CORE_ADDR sp, sigcontext_addr, addr;
  int regnum;

  /* We find the appropriate instance of `struct sigcontext' at a
     fixed offset in the signal frame.  */
  sp = frame_unwind_register_signed (next_frame, ARM_SP_REGNUM);
  sigcontext_addr = sp + 16;

  /* PC.  */
  trad_frame_set_reg_addr (cache, ARM_PC_REGNUM, sigcontext_addr + 76);

  /* GPRs.  */
  for (regnum = ARM_A1_REGNUM, addr = sigcontext_addr + 12;
       regnum <= ARM_LR_REGNUM; regnum++, addr += 4)
    trad_frame_set_reg_addr (cache, regnum, addr);

  trad_frame_set_id (cache, frame_id_build (sp, func));
}

static const struct tramp_frame armobsd_sigframe =
{
  SIGTRAMP_FRAME,
  4,
  {
    { 0xe28d0010, -1 },		/* add     r0, sp, #16 */
    { 0xef000067, -1 },		/* swi     SYS_sigreturn */
    { 0xef000001, -1 },		/* swi     SYS_exit */
    { 0xeafffffc, -1 },		/* b       . - 8 */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  armobsd_sigframe_init
};


/* Override default thumb breakpoints.  */
static const char arm_obsd_thumb_le_breakpoint[] = {0xfe, 0xdf};
static const char arm_obsd_thumb_be_breakpoint[] = {0xdf, 0xfe};

static void
armobsd_init_abi (struct gdbarch_info info,
		  struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (tdep->fp_model == ARM_FLOAT_AUTO)
    tdep->fp_model = ARM_FLOAT_SOFT_VFP;

  tramp_frame_prepend_unwinder (gdbarch, &armobsd_sigframe);

  /* OpenBSD/arm uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
  set_gdbarch_skip_solib_resolver (gdbarch, obsd_skip_solib_resolver);

  tdep->jb_pc = 24;
  tdep->jb_elt_size = 4;

  set_gdbarch_regset_from_core_section
    (gdbarch, armbsd_regset_from_core_section);

  /* OpenBSD/arm uses -fpcc-struct-return by default.  */
  tdep->struct_return = pcc_struct_return;

  /* Single stepping.  */
  set_gdbarch_software_single_step (gdbarch, arm_software_single_step);

  /* Breakpoints.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      tdep->thumb_breakpoint = arm_obsd_thumb_be_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_obsd_thumb_be_breakpoint);
      break;

    case BFD_ENDIAN_LITTLE:
      tdep->thumb_breakpoint = arm_obsd_thumb_le_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_obsd_thumb_le_breakpoint);
      break;
    }
}


static enum gdb_osabi
armobsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_OPENBSD_ELF;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_armobsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_arm, bfd_target_unknown_flavour,
                                  armobsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_arm, 0, GDB_OSABI_OPENBSD_ELF,
			  armobsd_init_abi);
}
