/* SPU target-dependent code for GDB, the GNU debugger.
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

#ifndef SPU_TDEP_H
#define SPU_TDEP_H

/* Number of registers.  */
#define SPU_NUM_REGS         130
#define SPU_NUM_PSEUDO_REGS  6
#define SPU_NUM_GPRS	     128

/* Register numbers of various important registers.  */
enum spu_regnum
{
  /* SPU calling convention.  */
  SPU_LR_REGNUM = 0,		/* Link register.  */
  SPU_RAW_SP_REGNUM = 1,	/* Stack pointer (full register).  */
  SPU_ARG1_REGNUM = 3,		/* First argument register.  */
  SPU_ARGN_REGNUM = 74,		/* Last argument register.  */
  SPU_SAVED1_REGNUM = 80,	/* First call-saved register.  */
  SPU_SAVEDN_REGNUM = 127,	/* Last call-saved register.  */
  SPU_FP_REGNUM = 127,		/* Frame pointer.  */

  /* Special registers.  */
  SPU_ID_REGNUM = 128,		/* SPU ID register.  */
  SPU_PC_REGNUM = 129,		/* Next program counter.  */
  SPU_SP_REGNUM = 130,		/* Stack pointer (preferred slot).  */
  SPU_FPSCR_REGNUM = 131,	/* Floating point status/control register.  */
  SPU_SRR0_REGNUM = 132,	/* SRR0 register.  */
  SPU_LSLR_REGNUM = 133,	/* Local store limit register.  */
  SPU_DECR_REGNUM = 134,	/* Decrementer value.  */
  SPU_DECR_STATUS_REGNUM = 135	/* Decrementer status.  */
};

/* Local store.  */
#define SPU_LS_SIZE          0x40000

#endif
