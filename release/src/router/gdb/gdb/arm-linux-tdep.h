/* GNU/Linux on ARM target support, prototypes.

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

struct regset;
struct regcache;

#define		ARM_CPSR_REGNUM		16

#define ARM_LINUX_SIZEOF_NWFPE (8 * FP_REGISTER_SIZE \
				+ 2 * INT_REGISTER_SIZE \
				+ 8 + INT_REGISTER_SIZE)

/* Support for register format used by the NWFPE FPA emulator.  Each
   register takes three words, where either the first one, two, or
   three hold a single, double, or extended precision value (depending
   on the corresponding tag).  The register set is eight registers,
   followed by the fpsr and fpcr, followed by eight tag bytes, and a
   final word flag which indicates whether NWFPE has been
   initialized.  */

#define NWFPE_FPSR_OFFSET (8 * FP_REGISTER_SIZE)
#define NWFPE_FPCR_OFFSET (NWFPE_FPSR_OFFSET + INT_REGISTER_SIZE)
#define NWFPE_TAGS_OFFSET (NWFPE_FPCR_OFFSET + INT_REGISTER_SIZE)
#define NWFPE_INITFLAG_OFFSET (NWFPE_TAGS_OFFSET + 8)

void arm_linux_supply_gregset (const struct regset *regset,
			       struct regcache *regcache,
			       int regnum, const void *gregs_buf, size_t len);
void arm_linux_collect_gregset (const struct regset *regset,
				const struct regcache *regcache,
				int regnum, void *gregs_buf, size_t len);

void supply_nwfpe_register (struct regcache *regcache, int regno,
			    const gdb_byte *regs);
void collect_nwfpe_register (const struct regcache *regcache, int regno,
			     gdb_byte *regs);

void arm_linux_supply_nwfpe (const struct regset *regset,
			     struct regcache *regcache,
			     int regnum, const void *regs_buf, size_t len);
void arm_linux_collect_nwfpe (const struct regset *regset,
			      const struct regcache *regcache,
			      int regnum, void *regs_buf, size_t len);
