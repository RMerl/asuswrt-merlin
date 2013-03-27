/* Target-dependent code for the Motorola 88000 series.

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

#ifndef M88K_TDEP_H
#define M88K_TDEP_H

/* Register numbers of various important registers.  */

enum m88k_regnum
{
  M88K_R0_REGNUM,
  M88K_R1_REGNUM,		/* Return address (hardware).  */
  M88K_R2_REGNUM,
  M88K_R3_REGNUM,
  M88K_R12_REGNUM = 12,
  M88K_R30_REGNUM = 30,		/* Frame pointer.  */
  M88K_R31_REGNUM,		/* Stack pointer. */
  M88K_EPSR_REGNUM,
  M88K_FPSR_REGNUM,
  M88K_FPCR_REGNUM,
  M88K_SXIP_REGNUM,
  M88K_SNIP_REGNUM,
  M88K_SFIP_REGNUM,
  M88K_NUM_REGS			/* Number of machine registers.  */

};

/* Instruction size.  */
#define M88K_INSN_SIZE 4

#endif /* m88k-tdep.h */
