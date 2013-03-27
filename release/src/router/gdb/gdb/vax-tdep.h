/* Target-dependent code for the VAX.

   Copyright (C) 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

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

#ifndef VAX_TDEP_H
#define VAX_TDEP_H

/* Register numbers of various important registers.  */

enum vax_regnum
{
  VAX_R0_REGNUM,
  VAX_R1_REGNUM,
  VAX_AP_REGNUM = 12,		/* Argument pointer on user stack.  */
  VAX_FP_REGNUM,		/* Address of executing stack frame.  */
  VAX_SP_REGNUM,		/* Address of top of stack.  */
  VAX_PC_REGNUM,		/* Program counter.  */
  VAX_PS_REGNUM			/* Processor status.  */
};

/* Number of machine registers.  */
#define VAX_NUM_REGS 17

#endif /* vax-tdep.h */
