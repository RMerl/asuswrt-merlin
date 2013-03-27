/* Target-dependent code for NetBSD/powerpc.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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

#ifndef PPCNBSD_TDEP_H
#define PPCNBSD_TDEP_H

#include <stddef.h>

struct regset;

/* Register offsets for NetBSD/powerpc.  */
extern struct ppc_reg_offsets ppcnbsd_reg_offsets;

/* Register sets for NetBSD/powerpc.  */
extern struct regset ppcnbsd_gregset;
extern struct regset ppcnbsd_fpregset;

#endif /* ppcnbsd-tdep.h */
