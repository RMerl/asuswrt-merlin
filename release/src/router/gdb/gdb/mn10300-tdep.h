/* Target-dependent interface for Matsushita MN10300 for GDB, the GNU debugger.

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

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

enum {
  E_D0_REGNUM = 0,
  E_D1_REGNUM = 1,
  E_D2_REGNUM = 2,
  E_D3_REGNUM = 3,
  E_A0_REGNUM = 4,
  E_A1_REGNUM = 5,
  E_A2_REGNUM = 6,
  E_A3_REGNUM = 7,
  E_SP_REGNUM = 8,
  E_PC_REGNUM = 9,
  E_MDR_REGNUM = 10,
  E_PSW_REGNUM = 11,
  E_LIR_REGNUM = 12,
  E_LAR_REGNUM = 13,
  E_MDRQ_REGNUM = 14,
  E_E0_REGNUM = 15,
  E_E1_REGNUM = 16,
  E_E2_REGNUM = 17,
  E_E3_REGNUM = 18,
  E_E4_REGNUM = 19,
  E_E5_REGNUM = 20,
  E_E6_REGNUM = 21,
  E_E7_REGNUM = 22,
  E_E8_REGNUM = 23,
  E_E9_REGNUM = 24,
  E_E10_REGNUM = 25,
  E_MCRH_REGNUM = 26,
  E_MCRL_REGNUM = 27,
  E_MCVF_REGNUM = 28,
  E_FPCR_REGNUM = 29,
  E_FS0_REGNUM = 32
};

enum movm_register_bits {
  movm_exother_bit = 0x01,
  movm_exreg1_bit  = 0x02,
  movm_exreg0_bit  = 0x04,
  movm_other_bit   = 0x08,
  movm_a3_bit      = 0x10,
  movm_a2_bit      = 0x20,
  movm_d3_bit      = 0x40,
  movm_d2_bit      = 0x80
};

/* Values for frame_info.status */

enum frame_kind {
  MY_FRAME_IN_SP = 0x1,
  MY_FRAME_IN_FP = 0x2,
  NO_MORE_FRAMES = 0x4
};

/* mn10300 private data */
struct gdbarch_tdep
{
  int am33_mode;
};

#define AM33_MODE (gdbarch_tdep (current_gdbarch)->am33_mode)
