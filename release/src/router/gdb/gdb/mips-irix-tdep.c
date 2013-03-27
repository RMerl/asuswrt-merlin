/* Target-dependent code for the MIPS architecture running on IRIX,
   for GDB, the GNU Debugger.

   Copyright (C) 2002, 2007 Free Software Foundation, Inc.

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

#include "elf-bfd.h"

static void
mips_irix_elf_osabi_sniff_abi_tag_sections (bfd *abfd, asection *sect,
                                            void *obj)
{
  enum gdb_osabi *os_ident_ptr = obj;
  const char *name;
  unsigned int sectsize;

  name = bfd_get_section_name (abfd, sect);
  sectsize = bfd_section_size (abfd, sect);

  if (strncmp (name, ".MIPS.", 6) == 0 && sectsize > 0)
    {
      /* The presence of a section named with a ".MIPS." prefix is
         indicative of an IRIX binary.  */
      *os_ident_ptr = GDB_OSABI_IRIX;
    }
}

static enum gdb_osabi
mips_irix_elf_osabi_sniffer (bfd *abfd)
{
  unsigned int elfosabi;
  enum gdb_osabi osabi = GDB_OSABI_UNKNOWN;

  /* If the generic sniffer gets a hit, return and let other sniffers
     get a crack at it.  */
  bfd_map_over_sections (abfd,
			 generic_elf_osabi_sniff_abi_tag_sections,
			 &osabi);
  if (osabi != GDB_OSABI_UNKNOWN)
    return GDB_OSABI_UNKNOWN;

  elfosabi = elf_elfheader (abfd)->e_ident[EI_OSABI];

  if (elfosabi == ELFOSABI_NONE)
    {
      /* When elfosabi is ELFOSABI_NONE (0), then the ELF structures in the
	 file are conforming to the base specification for that machine 
	 (there are no OS-specific extensions).  In order to determine the 
	 real OS in use we must look for OS notes that have been added.  
	 
	 For IRIX, we simply look for sections named with .MIPS. as
	 prefixes.  */
      bfd_map_over_sections (abfd,
			     mips_irix_elf_osabi_sniff_abi_tag_sections, 
			     &osabi);
    }
  return osabi;
}

static void
mips_irix_init_abi (struct gdbarch_info info,
                    struct gdbarch *gdbarch)
{
}

void
_initialize_mips_irix_tdep (void)
{
  /* Register an ELF OS ABI sniffer for IRIX binaries.  */
  gdbarch_register_osabi_sniffer (bfd_arch_mips,
				  bfd_target_elf_flavour,
				  mips_irix_elf_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_mips, 0, GDB_OSABI_IRIX,
			  mips_irix_init_abi);
}
