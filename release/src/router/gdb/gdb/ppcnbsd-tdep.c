/* Target-dependent code for NetBSD/powerpc.

   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Wasabi Systems, Inc.

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
#include "gdbtypes.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "ppc-tdep.h"
#include "ppcnbsd-tdep.h"
#include "solib-svr4.h"

/* Register offsets from <machine/reg.h>.  */
struct ppc_reg_offsets ppcnbsd_reg_offsets;


/* Core file support.  */

/* NetBSD/powerpc register sets.  */

struct regset ppcnbsd_gregset =
{
  &ppcnbsd_reg_offsets,
  ppc_supply_gregset
};

struct regset ppcnbsd_fpregset =
{
  &ppcnbsd_reg_offsets,
  ppc_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
ppcnbsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= 148)
    return &ppcnbsd_gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= 264)
    return &ppcnbsd_fpregset;

  return NULL;
}


/* NetBSD is confused.  It appears that 1.5 was using the correct SVR4
   convention but, 1.6 switched to the below broken convention.  For
   the moment use the broken convention.  Ulgh!.  */

static enum return_value_convention
ppcnbsd_return_value (struct gdbarch *gdbarch, struct type *valtype,
		      struct regcache *regcache, gdb_byte *readbuf,
		      const gdb_byte *writebuf)
{
#if 0
  if ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
       || TYPE_CODE (valtype) == TYPE_CODE_UNION)
      && !((TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 8)
	    && TYPE_VECTOR (valtype))
      && !(TYPE_LENGTH (valtype) == 1
	   || TYPE_LENGTH (valtype) == 2
	   || TYPE_LENGTH (valtype) == 4
	   || TYPE_LENGTH (valtype) == 8))
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
#endif
    return ppc_sysv_abi_broken_return_value (gdbarch, valtype, regcache,
					     readbuf, writebuf);
}


/* Signal trampolines.  */

static const struct tramp_frame ppcnbsd2_sigtramp;

static void
ppcnbsd_sigtramp_cache_init (const struct tramp_frame *self,
			     struct frame_info *next_frame,
			     struct trad_frame_cache *this_cache,
			     CORE_ADDR func)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  CORE_ADDR addr, base;
  int i;

  base = frame_unwind_register_unsigned (next_frame,
					 gdbarch_sp_regnum (current_gdbarch));
  if (self == &ppcnbsd2_sigtramp)
    addr = base + 0x10 + 2 * tdep->wordsize;
  else
    addr = base + 0x18 + 2 * tdep->wordsize;
  for (i = 0; i < ppc_num_gprs; i++, addr += tdep->wordsize)
    {
      int regnum = i + tdep->ppc_gp0_regnum;
      trad_frame_set_reg_addr (this_cache, regnum, addr);
    }
  trad_frame_set_reg_addr (this_cache, tdep->ppc_lr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_cr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_xer_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_ctr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache,
			   gdbarch_pc_regnum (current_gdbarch),
			   addr); /* SRR0? */
  addr += tdep->wordsize;

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

static const struct tramp_frame ppcnbsd_sigtramp =
{
  SIGTRAMP_FRAME,
  4,
  {
    { 0x3821fff0, -1 },		/* add r1,r1,-16 */
    { 0x4e800021, -1 },		/* blrl */
    { 0x38610018, -1 },		/* addi r3,r1,24 */
    { 0x38000127, -1 },		/* li r0,295 */
    { 0x44000002, -1 },		/* sc */
    { 0x38000001, -1 },		/* li r0,1 */
    { 0x44000002, -1 },		/* sc */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  ppcnbsd_sigtramp_cache_init
};

/* NetBSD 2.0 introduced a slightly different signal trampoline.  */

static const struct tramp_frame ppcnbsd2_sigtramp =
{
  SIGTRAMP_FRAME,
  4,
  {
    { 0x3821fff0, -1 },		/* add r1,r1,-16 */
    { 0x4e800021, -1 },		/* blrl */
    { 0x38610010, -1 },		/* addi r3,r1,16 */
    { 0x38000127, -1 },		/* li r0,295 */
    { 0x44000002, -1 },		/* sc */
    { 0x38000001, -1 },		/* li r0,1 */
    { 0x44000002, -1 },		/* sc */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  ppcnbsd_sigtramp_cache_init
};


static void
ppcnbsd_init_abi (struct gdbarch_info info,
                  struct gdbarch *gdbarch)
{
  /* For NetBSD, this is an on again, off again thing.  Some systems
     do use the broken struct convention, and some don't.  */
  set_gdbarch_return_value (gdbarch, ppcnbsd_return_value);

  /* NetBSD uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  set_gdbarch_regset_from_core_section
    (gdbarch, ppcnbsd_regset_from_core_section);

  tramp_frame_prepend_unwinder (gdbarch, &ppcnbsd_sigtramp);
  tramp_frame_prepend_unwinder (gdbarch, &ppcnbsd2_sigtramp);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcnbsd_tdep (void);

void
_initialize_ppcnbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_NETBSD_ELF,
			  ppcnbsd_init_abi);

  /* Avoid initializing the register offsets again if they were
     already initailized by ppcnbsd-nat.c.  */
  if (ppcnbsd_reg_offsets.pc_offset == 0)
    {
      /* General-purpose registers.  */
      ppcnbsd_reg_offsets.r0_offset = 0;
      ppcnbsd_reg_offsets.gpr_size = 4;
      ppcnbsd_reg_offsets.xr_size = 4;
      ppcnbsd_reg_offsets.lr_offset = 128;
      ppcnbsd_reg_offsets.cr_offset = 132;
      ppcnbsd_reg_offsets.xer_offset = 136;
      ppcnbsd_reg_offsets.ctr_offset = 140;
      ppcnbsd_reg_offsets.pc_offset = 144;
      ppcnbsd_reg_offsets.ps_offset = -1;
      ppcnbsd_reg_offsets.mq_offset = -1;

      /* Floating-point registers.  */
      ppcnbsd_reg_offsets.f0_offset = 0;
      ppcnbsd_reg_offsets.fpscr_offset = 256;
      ppcnbsd_reg_offsets.fpscr_size = 4;

      /* AltiVec registers.  */
      ppcnbsd_reg_offsets.vr0_offset = 0;
      ppcnbsd_reg_offsets.vrsave_offset = 512;
      ppcnbsd_reg_offsets.vscr_offset = 524;
    }
}
