/* Target-dependent code for OpenBSD/powerpc.

   Copyright (C) 2004, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef PPCOBSD_TDEP_H
#define PPCOBSD_TDEP_H

#include <stddef.h>

struct regset;
struct regcache;

/* Register offsets for OpenBSD/powerpc.  */
extern struct ppc_reg_offsets ppcobsd_reg_offsets;
extern struct ppc_reg_offsets ppcobsd_fpreg_offsets;

/* Register sets for OpenBSD/powerpc.  */
extern struct regset ppcobsd_gregset;
extern struct regset ppcobsd_fpregset;


/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

extern void ppcobsd_supply_gregset (const struct regset *regset,
				    struct regcache *regcache, int regnum,
				    const void *gregs, size_t len);

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

extern void ppcobsd_collect_gregset (const struct regset *regset,
				     const struct regcache *regcache,
				     int regnum, void *gregs, size_t len);

#endif /* ppcobsd-tdep.h */
