/* Target-dependent code for Renesas M32R, for GDB.
 
   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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

#ifndef M32R_TDEP_H
#define M32R_TDEP_H

struct gdbarch_tdep
{
  /* gdbarch target dependent data here. Currently unused for M32R. */
};

/* m32r register names. */

enum m32r_regnum
{
  R0_REGNUM = 0,
  R3_REGNUM = 3,
  M32R_FP_REGNUM = 13,
  LR_REGNUM = 14,
  M32R_SP_REGNUM = 15,
  PSW_REGNUM = 16,
  CBR_REGNUM = 17,
  SPU_REGNUM = 18,
  SPI_REGNUM = 19,
  M32R_PC_REGNUM = 21,
  /* m32r calling convention. */
  ARG1_REGNUM = R0_REGNUM,
  ARGN_REGNUM = R3_REGNUM,
  RET1_REGNUM = R0_REGNUM,
};

#define M32R_NUM_REGS 25

#endif /* m32r-tdep.h */
