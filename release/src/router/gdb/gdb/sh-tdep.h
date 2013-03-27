/* Target-specific definition for a Renesas Super-H.
   Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2007 Free Software Foundation, Inc.

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

#ifndef SH_TDEP_H
#define SH_TDEP_H

/* Contributed by Steve Chamberlain sac@cygnus.com */

/* Registers for all SH variants.  Used also by sh3-rom.c. */
enum
  {
    R0_REGNUM = 0,
    STRUCT_RETURN_REGNUM = 2,
    ARG0_REGNUM = 4,
    ARGLAST_REGNUM = 7,
    FP_REGNUM = 14,
    PR_REGNUM = 17,
    GBR_REGNUM = 18,
    VBR_REGNUM = 19,
    MACH_REGNUM = 20,
    MACL_REGNUM = 21,
    SR_REGNUM = 22,
    FPUL_REGNUM = 23,
    /* Floating point registers */
    FPSCR_REGNUM = 24,
    FR0_REGNUM = 25,
    FLOAT_ARG0_REGNUM = 29,
    FLOAT_ARGLAST_REGNUM = 36,
    FP_LAST_REGNUM = 40,
    /* sh3,sh4 registers */
    SSR_REGNUM = 41,
    SPC_REGNUM = 42,
    /* DSP registers */
    DSR_REGNUM = 24,
    A0G_REGNUM = 25,
    A0_REGNUM = 26,
    A1G_REGNUM = 27,
    A1_REGNUM = 28,
    M0_REGNUM = 29,
    M1_REGNUM = 30,
    X0_REGNUM = 31,
    X1_REGNUM = 32,
    Y0_REGNUM = 33,
    Y1_REGNUM = 34,
    MOD_REGNUM = 40,
    RS_REGNUM = 43,
    RE_REGNUM = 44,
    DSP_R0_BANK_REGNUM = 51,
    DSP_R7_BANK_REGNUM = 58,
    /* sh2a register */
    R0_BANK0_REGNUM = 43,
    MACHB_REGNUM = 58,
    IVNB_REGNUM = 59,
    PRB_REGNUM = 60,
    GBRB_REGNUM = 61,
    MACLB_REGNUM = 62,
    BANK_REGNUM = 63,
    IBCR_REGNUM = 64,
    IBNR_REGNUM = 65,
    TBR_REGNUM = 66,
    PSEUDO_BANK_REGNUM = 67,
    /* Floating point pseudo registers */
    DR0_REGNUM = 68,
    DR_LAST_REGNUM = 75,
    FV0_REGNUM = 76,
    FV_LAST_REGNUM = 79
  };

extern gdbarch_init_ftype sh64_gdbarch_init;
extern void sh64_show_regs (struct frame_info *);

#endif /* SH_TDEP_H */
