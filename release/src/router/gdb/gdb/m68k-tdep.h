/* Target-dependent code for the Motorola 68000 series.

   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1999, 2000, 2001,
   2003, 2004, 2007 Free Software Foundation, Inc.

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

#ifndef M68K_TDEP_H
#define M68K_TDEP_H

struct frame_info;

/* Register numbers of various important registers.  */

enum m68k_regnum
{
  M68K_D0_REGNUM = 0,
  M68K_D1_REGNUM = 1,
  M68K_D2_REGNUM = 2,
  M68K_D7_REGNUM = 7,
  M68K_A0_REGNUM = 8,
  M68K_A1_REGNUM = 9,
  M68K_A2_REGNUM = 10,
  M68K_FP_REGNUM = 14,		/* Address of executing stack frame.  */
  M68K_SP_REGNUM = 15,		/* Address of top of stack.  */
  M68K_PS_REGNUM = 16,		/* Processor status.  */
  M68K_PC_REGNUM = 17,		/* Program counter.  */
  M68K_FP0_REGNUM = 18,		/* Floating point register 0.  */
  M68K_FPC_REGNUM = 26,		/* 68881 control register.  */
  M68K_FPS_REGNUM = 27,		/* 68881 status register.  */
  M68K_FPI_REGNUM = 28
};

/* Number of machine registers.  */
#define M68K_NUM_REGS	(M68K_FPI_REGNUM + 1)

/* Size of the largest register.  */
#define M68K_MAX_REGISTER_SIZE	12

/* Convention for returning structures.  */

enum struct_return
{
  pcc_struct_return,		/* Return "short" structures in memory.  */
  reg_struct_return		/* Return "short" structures in registers.  */
};

/* Particular flavour of m68k.  */
enum m68k_flavour
  {
    m68k_no_flavour,
    m68k_coldfire_flavour,
    m68k_fido_flavour
  };

/* Target-dependent structure in gdbarch.  */

struct gdbarch_tdep
{
  /* Offset to PC value in the jump buffer.  If this is negative,
     longjmp support will be disabled.  */
  int jb_pc;
  /* The size of each entry in the jump buffer.  */
  size_t jb_elt_size;

  /* Register in which the address to store a structure value is
     passed to a function.  */
  int struct_value_regnum;

  /* Convention for returning structures.  */
  enum struct_return struct_return;

  /* Convention for returning floats.  zero in int regs, non-zero in float.  */
  int float_return;

  /* The particular flavour of m68k.  */
  enum m68k_flavour flavour;

  /* Flag set if the floating point registers are present, or assumed
     to be present.  */
  int fpregs_present;
};

/* Initialize a SVR4 architecture variant.  */
extern void m68k_svr4_init_abi (struct gdbarch_info, struct gdbarch *);


/* Functions exported from m68kbsd-tdep.c.  */

extern int m68kbsd_fpreg_offset (int regnum);

#endif /* m68k-tdep.h */
