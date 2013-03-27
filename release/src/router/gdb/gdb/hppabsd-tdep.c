/* Target-dependent code for HP PA-RISC BSD's.

   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdbtypes.h"
#include "symtab.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "target.h"
#include "value.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "elf/common.h"

#include "hppa-tdep.h"
#include "solib-svr4.h"

/* Core file support.  */

/* Sizeof `struct reg' in <machine/reg.h>.  */
#define HPPABSD_SIZEOF_GREGS	(34 * 4)

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
hppabsd_supply_gregset (const struct regset *regset, struct regcache *regcache,
		     int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  size_t offset;
  int i;

  gdb_assert (len >= HPPABSD_SIZEOF_GREGS);

  for (i = HPPA_R1_REGNUM, offset = 4; i <= HPPA_R31_REGNUM; i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_supply (regcache, i, regs + offset);
    }

  if (regnum == -1 || regnum == HPPA_SAR_REGNUM)
    regcache_raw_supply (regcache, HPPA_SAR_REGNUM, regs);
  if (regnum == -1 || regnum == HPPA_PCOQ_HEAD_REGNUM)
    regcache_raw_supply (regcache, HPPA_PCOQ_HEAD_REGNUM, regs + 32 * 4);
  if (regnum == -1 || regnum == HPPA_PCOQ_TAIL_REGNUM)
    regcache_raw_supply (regcache, HPPA_PCOQ_TAIL_REGNUM, regs + 33 * 4);
}

/* OpenBSD/hppa register set.  */

static struct regset hppabsd_gregset =
{
  NULL,
  hppabsd_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
hppabsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= HPPABSD_SIZEOF_GREGS)
    return &hppabsd_gregset;

  return NULL;
}


CORE_ADDR
hppabsd_find_global_pointer (struct value *function)
{
  CORE_ADDR faddr = value_as_address (function);
  struct obj_section *faddr_sec;
  gdb_byte buf[4];

  /* Is this a plabel? If so, dereference it to get the Global Pointer
     value.  */
  if (faddr & 2)
    {
      if (target_read_memory ((faddr & ~3) + 4, buf, sizeof buf) == 0)
	return extract_unsigned_integer (buf, sizeof buf);
    }

  /* If the address is in the .plt section, then the real function
     hasn't yet been fixed up by the linker so we cannot determine the
     Global Pointer for that function.  */
  if (in_plt_section (faddr, NULL))
    return 0;

  faddr_sec = find_pc_section (faddr);
  if (faddr_sec != NULL)
    {
      struct obj_section *sec;

      ALL_OBJFILE_OSECTIONS (faddr_sec->objfile, sec)
	{
	  if (strcmp (sec->the_bfd_section->name, ".dynamic") == 0)
	    break;
	}

      if (sec < faddr_sec->objfile->sections_end)
	{
	  CORE_ADDR addr = sec->addr;

	  while (addr < sec->endaddr)
	    {
	      gdb_byte buf[4];
	      LONGEST tag;

	      if (target_read_memory (addr, buf, sizeof buf) != 0)
		break;

	      tag = extract_signed_integer (buf, sizeof buf);
	      if (tag == DT_PLTGOT)
		{
		  CORE_ADDR pltgot;

		  if (target_read_memory (addr + 4, buf, sizeof buf) != 0)
		    break;

		  /* The OpenBSD ld.so doesn't relocate DT_PLTGOT, so
		     we have to do it ourselves.  */
		  pltgot = extract_unsigned_integer (buf, sizeof buf);
		  pltgot += ANOFFSET (sec->objfile->section_offsets,
				      SECT_OFF_TEXT (sec->objfile));
		  return pltgot;
		}

	      if (tag == DT_NULL)
		break;

	      addr += 8;
	    }
	}
    }

  return 0;
}


static void
hppabsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* OpenBSD and NetBSD have a 64-bit 'long double'.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section
    (gdbarch, hppabsd_regset_from_core_section);

  /* OpenBSD and NetBSD use ELF.  */
  tdep->is_elf = 1;
  tdep->find_global_pointer = hppabsd_find_global_pointer;
  tdep->in_solib_call_trampoline = hppa_in_solib_call_trampoline;
  set_gdbarch_skip_trampoline_code (gdbarch, hppa_skip_trampoline_code);

  /* OpenBSD and NetBSD use SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


/* OpenBSD uses uses the traditional NetBSD core file format, even for
   ports that use ELF.  */
#define GDB_OSABI_NETBSD_CORE GDB_OSABI_OPENBSD_ELF

static enum gdb_osabi
hppabsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_CORE;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_hppabsd_tdep (void);

void
_initialize_hppabsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_hppa, bfd_target_unknown_flavour,
				  hppabsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_NETBSD_ELF,
			  hppabsd_init_abi);
  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_OPENBSD_ELF,
			  hppabsd_init_abi);
}
