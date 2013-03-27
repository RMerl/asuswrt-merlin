/* Target-dependent code for NetBSD/sh.

   Copyright (C) 2002, 2003, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "regcache.h"
#include "regset.h"
#include "value.h"
#include "osabi.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "sh-tdep.h"
#include "shnbsd-tdep.h"
#include "solib-svr4.h"

/* Convert an r0-r15 register number into an offset into a ptrace
   register structure.  */
static const int regmap[] =
{
  (20 * 4),	/* r0 */
  (19 * 4),	/* r1 */
  (18 * 4),	/* r2 */ 
  (17 * 4),	/* r3 */ 
  (16 * 4),	/* r4 */
  (15 * 4),	/* r5 */
  (14 * 4),	/* r6 */
  (13 * 4),	/* r7 */
  (12 * 4),	/* r8 */ 
  (11 * 4),	/* r9 */
  (10 * 4),	/* r10 */
  ( 9 * 4),	/* r11 */
  ( 8 * 4),	/* r12 */
  ( 7 * 4),	/* r13 */
  ( 6 * 4),	/* r14 */
  ( 5 * 4),	/* r15 */
};

/* Sizeof `struct reg' in <machine/reg.h>.  */
#define SHNBSD_SIZEOF_GREGS	(21 * 4)

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
shnbsd_supply_gregset (const struct regset *regset,
		       struct regcache *regcache,
		       int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  gdb_assert (len >= SHNBSD_SIZEOF_GREGS);

  if (regnum == gdbarch_pc_regnum (current_gdbarch) || regnum == -1)
    regcache_raw_supply (regcache,
			 gdbarch_pc_regnum (current_gdbarch),
			 regs + (0 * 4));

  if (regnum == SR_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, SR_REGNUM, regs + (1 * 4));

  if (regnum == PR_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, PR_REGNUM, regs + (2 * 4));

  if (regnum == MACH_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, MACH_REGNUM, regs + (3 * 4));

  if (regnum == MACL_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, MACL_REGNUM, regs + (4 * 4));

  for (i = R0_REGNUM; i <= (R0_REGNUM + 15); i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + regmap[i - R0_REGNUM]);
    }
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

static void
shnbsd_collect_gregset (const struct regset *regset,
			const struct regcache *regcache,
			int regnum, void *gregs, size_t len)
{
  gdb_byte *regs = gregs;
  int i;

  gdb_assert (len >= SHNBSD_SIZEOF_GREGS);

  if (regnum == gdbarch_pc_regnum (current_gdbarch) || regnum == -1)
    regcache_raw_collect (regcache, gdbarch_pc_regnum (current_gdbarch),
			  regs + (0 * 4));

  if (regnum == SR_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, SR_REGNUM, regs + (1 * 4));

  if (regnum == PR_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, PR_REGNUM, regs + (2 * 4));

  if (regnum == MACH_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, MACH_REGNUM, regs + (3 * 4));

  if (regnum == MACL_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, MACL_REGNUM, regs + (4 * 4));

  for (i = R0_REGNUM; i <= (R0_REGNUM + 15); i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_collect (regcache, i, regs + regmap[i - R0_REGNUM]);
    }
}

/* SH register sets.  */

static struct regset shnbsd_gregset =
{
  NULL,
  shnbsd_supply_gregset,
  shnbsd_collect_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

const struct regset *
shnbsd_regset_from_core_section (struct gdbarch *gdbarch,
				 const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= SHNBSD_SIZEOF_GREGS)
    return &shnbsd_gregset;

  return NULL;
}

void
shnbsd_supply_reg (struct regcache *regcache, const char *regs, int regnum)
{
  shnbsd_supply_gregset (&shnbsd_gregset, regcache, regnum,
			 regs, SHNBSD_SIZEOF_GREGS);
}

void
shnbsd_fill_reg (const struct regcache *regcache, char *regs, int regnum)
{
  shnbsd_collect_gregset (&shnbsd_gregset, regcache, regnum,
			  regs, SHNBSD_SIZEOF_GREGS);
}


static void
shnbsd_init_abi (struct gdbarch_info info,
                  struct gdbarch *gdbarch)
{
  set_gdbarch_regset_from_core_section
    (gdbarch, shnbsd_regset_from_core_section);

  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


/* OpenBSD uses uses the traditional NetBSD core file format, even for
   ports that use ELF.  */
#define GDB_OSABI_NETBSD_CORE GDB_OSABI_OPENBSD_ELF

static enum gdb_osabi
shnbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_CORE;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_shnbsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_sh, bfd_target_unknown_flavour,
                                  shnbsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_sh, 0, GDB_OSABI_NETBSD_ELF,
			  shnbsd_init_abi);
  gdbarch_register_osabi (bfd_arch_sh, 0, GDB_OSABI_OPENBSD_ELF,
			  shnbsd_init_abi);
}
