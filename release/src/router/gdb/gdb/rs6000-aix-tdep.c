/* Native support code for PPC AIX, for GDB the GNU debugger.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "rs6000-tdep.h"
#include "ppc-tdep.h"


/* Core file support.  */

static struct ppc_reg_offsets rs6000_aix32_reg_offsets =
{
  /* General-purpose registers.  */
  208, /* r0_offset */
  4,  /* gpr_size */
  4,  /* xr_size */
  24, /* pc_offset */
  28, /* ps_offset */
  32, /* cr_offset */
  36, /* lr_offset */
  40, /* ctr_offset */
  44, /* xer_offset */
  48, /* mq_offset */

  /* Floating-point registers.  */
  336, /* f0_offset */
  56, /* fpscr_offset */
  4,  /* fpscr_size */

  /* AltiVec registers.  */
  -1, /* vr0_offset */
  -1, /* vscr_offset */
  -1 /* vrsave_offset */
};

static struct ppc_reg_offsets rs6000_aix64_reg_offsets =
{
  /* General-purpose registers.  */
  0, /* r0_offset */
  8,  /* gpr_size */
  4,  /* xr_size */
  264, /* pc_offset */
  256, /* ps_offset */
  288, /* cr_offset */
  272, /* lr_offset */
  280, /* ctr_offset */
  292, /* xer_offset */
  -1, /* mq_offset */

  /* Floating-point registers.  */
  312, /* f0_offset */
  296, /* fpscr_offset */
  4,  /* fpscr_size */

  /* AltiVec registers.  */
  -1, /* vr0_offset */
  -1, /* vscr_offset */
  -1 /* vrsave_offset */
};


/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
rs6000_aix_supply_regset (const struct regset *regset,
			  struct regcache *regcache, int regnum,
			  const void *gregs, size_t len)
{
  ppc_supply_gregset (regset, regcache, regnum, gregs, len);
  ppc_supply_fpregset (regset, regcache, regnum, gregs, len);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

static void
rs6000_aix_collect_regset (const struct regset *regset,
			   const struct regcache *regcache, int regnum,
			   void *gregs, size_t len)
{
  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
  ppc_collect_fpregset (regset, regcache, regnum, gregs, len);
}

/* AIX register set.  */

static struct regset rs6000_aix32_regset =
{
  &rs6000_aix32_reg_offsets,
  rs6000_aix_supply_regset,
  rs6000_aix_collect_regset,
};

static struct regset rs6000_aix64_regset =
{
  &rs6000_aix64_reg_offsets,
  rs6000_aix_supply_regset,
  rs6000_aix_collect_regset,
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
rs6000_aix_regset_from_core_section (struct gdbarch *gdbarch,
				     const char *sect_name, size_t sect_size)
{
  if (gdbarch_tdep (gdbarch)->wordsize == 4)
    {
      if (strcmp (sect_name, ".reg") == 0 && sect_size >= 592)
        return &rs6000_aix32_regset;
    }
  else
    {
      if (strcmp (sect_name, ".reg") == 0 && sect_size >= 576)
        return &rs6000_aix64_regset;
    }

  return NULL;
}


static enum gdb_osabi
rs6000_aix_osabi_sniffer (bfd *abfd)
{
  
  if (bfd_get_flavour (abfd) == bfd_target_xcoff_flavour);
    return GDB_OSABI_AIX;

  return GDB_OSABI_UNKNOWN;
}

static void
rs6000_aix_init_osabi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* RS6000/AIX does not support PT_STEP.  Has to be simulated.  */
  set_gdbarch_software_single_step (gdbarch, rs6000_software_single_step);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section
    (gdbarch, rs6000_aix_regset_from_core_section);

  /* Minimum possible text address in AIX.  */
  gdbarch_tdep (gdbarch)->text_segment_base = 0x10000000;
}

void
_initialize_rs6000_aix_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_rs6000,
                                  bfd_target_xcoff_flavour,
                                  rs6000_aix_osabi_sniffer);
  gdbarch_register_osabi_sniffer (bfd_arch_powerpc,
                                  bfd_target_xcoff_flavour,
                                  rs6000_aix_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_AIX,
                          rs6000_aix_init_osabi);
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_AIX,
                          rs6000_aix_init_osabi);
}

