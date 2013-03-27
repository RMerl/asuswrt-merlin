/* Simulator instruction decoder for m32r2f.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#define WANT_CPU m32r2f
#define WANT_CPU_M32R2F

#include "sim-main.h"
#include "sim-assert.h"

/* Insn can't be executed in parallel.
   Or is that "do NOt Pass to Air defense Radar"? :-) */
#define NOPAR (-1)

/* The instruction descriptor array.
   This is computed at runtime.  Space for it is not malloc'd to save a
   teensy bit of cpu in the decoder.  Moving it to malloc space is trivial
   but won't be done until necessary (we don't currently support the runtime
   addition of instructions nor an SMP machine with different cpus).  */
static IDESC m32r2f_insn_data[M32R2F_INSN__MAX];

/* Commas between elements are contained in the macros.
   Some of these are conditionally compiled out.  */

static const struct insn_sem m32r2f_insn_sem[] =
{
  { VIRTUAL_INSN_X_INVALID, M32R2F_INSN_X_INVALID, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { VIRTUAL_INSN_X_AFTER, M32R2F_INSN_X_AFTER, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { VIRTUAL_INSN_X_BEFORE, M32R2F_INSN_X_BEFORE, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { VIRTUAL_INSN_X_CTI_CHAIN, M32R2F_INSN_X_CTI_CHAIN, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { VIRTUAL_INSN_X_CHAIN, M32R2F_INSN_X_CHAIN, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { VIRTUAL_INSN_X_BEGIN, M32R2F_INSN_X_BEGIN, M32R2F_SFMT_EMPTY, NOPAR, NOPAR  },
  { M32R_INSN_ADD, M32R2F_INSN_ADD, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_ADD, M32R2F_INSN_WRITE_ADD },
  { M32R_INSN_ADD3, M32R2F_INSN_ADD3, M32R2F_SFMT_ADD3, NOPAR, NOPAR  },
  { M32R_INSN_AND, M32R2F_INSN_AND, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_AND, M32R2F_INSN_WRITE_AND },
  { M32R_INSN_AND3, M32R2F_INSN_AND3, M32R2F_SFMT_AND3, NOPAR, NOPAR  },
  { M32R_INSN_OR, M32R2F_INSN_OR, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_OR, M32R2F_INSN_WRITE_OR },
  { M32R_INSN_OR3, M32R2F_INSN_OR3, M32R2F_SFMT_OR3, NOPAR, NOPAR  },
  { M32R_INSN_XOR, M32R2F_INSN_XOR, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_XOR, M32R2F_INSN_WRITE_XOR },
  { M32R_INSN_XOR3, M32R2F_INSN_XOR3, M32R2F_SFMT_AND3, NOPAR, NOPAR  },
  { M32R_INSN_ADDI, M32R2F_INSN_ADDI, M32R2F_SFMT_ADDI, M32R2F_INSN_PAR_ADDI, M32R2F_INSN_WRITE_ADDI },
  { M32R_INSN_ADDV, M32R2F_INSN_ADDV, M32R2F_SFMT_ADDV, M32R2F_INSN_PAR_ADDV, M32R2F_INSN_WRITE_ADDV },
  { M32R_INSN_ADDV3, M32R2F_INSN_ADDV3, M32R2F_SFMT_ADDV3, NOPAR, NOPAR  },
  { M32R_INSN_ADDX, M32R2F_INSN_ADDX, M32R2F_SFMT_ADDX, M32R2F_INSN_PAR_ADDX, M32R2F_INSN_WRITE_ADDX },
  { M32R_INSN_BC8, M32R2F_INSN_BC8, M32R2F_SFMT_BC8, M32R2F_INSN_PAR_BC8, M32R2F_INSN_WRITE_BC8 },
  { M32R_INSN_BC24, M32R2F_INSN_BC24, M32R2F_SFMT_BC24, NOPAR, NOPAR  },
  { M32R_INSN_BEQ, M32R2F_INSN_BEQ, M32R2F_SFMT_BEQ, NOPAR, NOPAR  },
  { M32R_INSN_BEQZ, M32R2F_INSN_BEQZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BGEZ, M32R2F_INSN_BGEZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BGTZ, M32R2F_INSN_BGTZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BLEZ, M32R2F_INSN_BLEZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BLTZ, M32R2F_INSN_BLTZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BNEZ, M32R2F_INSN_BNEZ, M32R2F_SFMT_BEQZ, NOPAR, NOPAR  },
  { M32R_INSN_BL8, M32R2F_INSN_BL8, M32R2F_SFMT_BL8, M32R2F_INSN_PAR_BL8, M32R2F_INSN_WRITE_BL8 },
  { M32R_INSN_BL24, M32R2F_INSN_BL24, M32R2F_SFMT_BL24, NOPAR, NOPAR  },
  { M32R_INSN_BCL8, M32R2F_INSN_BCL8, M32R2F_SFMT_BCL8, M32R2F_INSN_PAR_BCL8, M32R2F_INSN_WRITE_BCL8 },
  { M32R_INSN_BCL24, M32R2F_INSN_BCL24, M32R2F_SFMT_BCL24, NOPAR, NOPAR  },
  { M32R_INSN_BNC8, M32R2F_INSN_BNC8, M32R2F_SFMT_BC8, M32R2F_INSN_PAR_BNC8, M32R2F_INSN_WRITE_BNC8 },
  { M32R_INSN_BNC24, M32R2F_INSN_BNC24, M32R2F_SFMT_BC24, NOPAR, NOPAR  },
  { M32R_INSN_BNE, M32R2F_INSN_BNE, M32R2F_SFMT_BEQ, NOPAR, NOPAR  },
  { M32R_INSN_BRA8, M32R2F_INSN_BRA8, M32R2F_SFMT_BRA8, M32R2F_INSN_PAR_BRA8, M32R2F_INSN_WRITE_BRA8 },
  { M32R_INSN_BRA24, M32R2F_INSN_BRA24, M32R2F_SFMT_BRA24, NOPAR, NOPAR  },
  { M32R_INSN_BNCL8, M32R2F_INSN_BNCL8, M32R2F_SFMT_BCL8, M32R2F_INSN_PAR_BNCL8, M32R2F_INSN_WRITE_BNCL8 },
  { M32R_INSN_BNCL24, M32R2F_INSN_BNCL24, M32R2F_SFMT_BCL24, NOPAR, NOPAR  },
  { M32R_INSN_CMP, M32R2F_INSN_CMP, M32R2F_SFMT_CMP, M32R2F_INSN_PAR_CMP, M32R2F_INSN_WRITE_CMP },
  { M32R_INSN_CMPI, M32R2F_INSN_CMPI, M32R2F_SFMT_CMPI, NOPAR, NOPAR  },
  { M32R_INSN_CMPU, M32R2F_INSN_CMPU, M32R2F_SFMT_CMP, M32R2F_INSN_PAR_CMPU, M32R2F_INSN_WRITE_CMPU },
  { M32R_INSN_CMPUI, M32R2F_INSN_CMPUI, M32R2F_SFMT_CMPI, NOPAR, NOPAR  },
  { M32R_INSN_CMPEQ, M32R2F_INSN_CMPEQ, M32R2F_SFMT_CMP, M32R2F_INSN_PAR_CMPEQ, M32R2F_INSN_WRITE_CMPEQ },
  { M32R_INSN_CMPZ, M32R2F_INSN_CMPZ, M32R2F_SFMT_CMPZ, M32R2F_INSN_PAR_CMPZ, M32R2F_INSN_WRITE_CMPZ },
  { M32R_INSN_DIV, M32R2F_INSN_DIV, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_DIVU, M32R2F_INSN_DIVU, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REM, M32R2F_INSN_REM, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REMU, M32R2F_INSN_REMU, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REMH, M32R2F_INSN_REMH, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REMUH, M32R2F_INSN_REMUH, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REMB, M32R2F_INSN_REMB, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_REMUB, M32R2F_INSN_REMUB, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_DIVUH, M32R2F_INSN_DIVUH, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_DIVB, M32R2F_INSN_DIVB, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_DIVUB, M32R2F_INSN_DIVUB, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_DIVH, M32R2F_INSN_DIVH, M32R2F_SFMT_DIV, NOPAR, NOPAR  },
  { M32R_INSN_JC, M32R2F_INSN_JC, M32R2F_SFMT_JC, M32R2F_INSN_PAR_JC, M32R2F_INSN_WRITE_JC },
  { M32R_INSN_JNC, M32R2F_INSN_JNC, M32R2F_SFMT_JC, M32R2F_INSN_PAR_JNC, M32R2F_INSN_WRITE_JNC },
  { M32R_INSN_JL, M32R2F_INSN_JL, M32R2F_SFMT_JL, M32R2F_INSN_PAR_JL, M32R2F_INSN_WRITE_JL },
  { M32R_INSN_JMP, M32R2F_INSN_JMP, M32R2F_SFMT_JMP, M32R2F_INSN_PAR_JMP, M32R2F_INSN_WRITE_JMP },
  { M32R_INSN_LD, M32R2F_INSN_LD, M32R2F_SFMT_LD, M32R2F_INSN_PAR_LD, M32R2F_INSN_WRITE_LD },
  { M32R_INSN_LD_D, M32R2F_INSN_LD_D, M32R2F_SFMT_LD_D, NOPAR, NOPAR  },
  { M32R_INSN_LDB, M32R2F_INSN_LDB, M32R2F_SFMT_LDB, M32R2F_INSN_PAR_LDB, M32R2F_INSN_WRITE_LDB },
  { M32R_INSN_LDB_D, M32R2F_INSN_LDB_D, M32R2F_SFMT_LDB_D, NOPAR, NOPAR  },
  { M32R_INSN_LDH, M32R2F_INSN_LDH, M32R2F_SFMT_LDH, M32R2F_INSN_PAR_LDH, M32R2F_INSN_WRITE_LDH },
  { M32R_INSN_LDH_D, M32R2F_INSN_LDH_D, M32R2F_SFMT_LDH_D, NOPAR, NOPAR  },
  { M32R_INSN_LDUB, M32R2F_INSN_LDUB, M32R2F_SFMT_LDB, M32R2F_INSN_PAR_LDUB, M32R2F_INSN_WRITE_LDUB },
  { M32R_INSN_LDUB_D, M32R2F_INSN_LDUB_D, M32R2F_SFMT_LDB_D, NOPAR, NOPAR  },
  { M32R_INSN_LDUH, M32R2F_INSN_LDUH, M32R2F_SFMT_LDH, M32R2F_INSN_PAR_LDUH, M32R2F_INSN_WRITE_LDUH },
  { M32R_INSN_LDUH_D, M32R2F_INSN_LDUH_D, M32R2F_SFMT_LDH_D, NOPAR, NOPAR  },
  { M32R_INSN_LD_PLUS, M32R2F_INSN_LD_PLUS, M32R2F_SFMT_LD_PLUS, M32R2F_INSN_PAR_LD_PLUS, M32R2F_INSN_WRITE_LD_PLUS },
  { M32R_INSN_LD24, M32R2F_INSN_LD24, M32R2F_SFMT_LD24, NOPAR, NOPAR  },
  { M32R_INSN_LDI8, M32R2F_INSN_LDI8, M32R2F_SFMT_LDI8, M32R2F_INSN_PAR_LDI8, M32R2F_INSN_WRITE_LDI8 },
  { M32R_INSN_LDI16, M32R2F_INSN_LDI16, M32R2F_SFMT_LDI16, NOPAR, NOPAR  },
  { M32R_INSN_LOCK, M32R2F_INSN_LOCK, M32R2F_SFMT_LOCK, M32R2F_INSN_PAR_LOCK, M32R2F_INSN_WRITE_LOCK },
  { M32R_INSN_MACHI_A, M32R2F_INSN_MACHI_A, M32R2F_SFMT_MACHI_A, M32R2F_INSN_PAR_MACHI_A, M32R2F_INSN_WRITE_MACHI_A },
  { M32R_INSN_MACLO_A, M32R2F_INSN_MACLO_A, M32R2F_SFMT_MACHI_A, M32R2F_INSN_PAR_MACLO_A, M32R2F_INSN_WRITE_MACLO_A },
  { M32R_INSN_MACWHI_A, M32R2F_INSN_MACWHI_A, M32R2F_SFMT_MACHI_A, M32R2F_INSN_PAR_MACWHI_A, M32R2F_INSN_WRITE_MACWHI_A },
  { M32R_INSN_MACWLO_A, M32R2F_INSN_MACWLO_A, M32R2F_SFMT_MACHI_A, M32R2F_INSN_PAR_MACWLO_A, M32R2F_INSN_WRITE_MACWLO_A },
  { M32R_INSN_MUL, M32R2F_INSN_MUL, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_MUL, M32R2F_INSN_WRITE_MUL },
  { M32R_INSN_MULHI_A, M32R2F_INSN_MULHI_A, M32R2F_SFMT_MULHI_A, M32R2F_INSN_PAR_MULHI_A, M32R2F_INSN_WRITE_MULHI_A },
  { M32R_INSN_MULLO_A, M32R2F_INSN_MULLO_A, M32R2F_SFMT_MULHI_A, M32R2F_INSN_PAR_MULLO_A, M32R2F_INSN_WRITE_MULLO_A },
  { M32R_INSN_MULWHI_A, M32R2F_INSN_MULWHI_A, M32R2F_SFMT_MULHI_A, M32R2F_INSN_PAR_MULWHI_A, M32R2F_INSN_WRITE_MULWHI_A },
  { M32R_INSN_MULWLO_A, M32R2F_INSN_MULWLO_A, M32R2F_SFMT_MULHI_A, M32R2F_INSN_PAR_MULWLO_A, M32R2F_INSN_WRITE_MULWLO_A },
  { M32R_INSN_MV, M32R2F_INSN_MV, M32R2F_SFMT_MV, M32R2F_INSN_PAR_MV, M32R2F_INSN_WRITE_MV },
  { M32R_INSN_MVFACHI_A, M32R2F_INSN_MVFACHI_A, M32R2F_SFMT_MVFACHI_A, M32R2F_INSN_PAR_MVFACHI_A, M32R2F_INSN_WRITE_MVFACHI_A },
  { M32R_INSN_MVFACLO_A, M32R2F_INSN_MVFACLO_A, M32R2F_SFMT_MVFACHI_A, M32R2F_INSN_PAR_MVFACLO_A, M32R2F_INSN_WRITE_MVFACLO_A },
  { M32R_INSN_MVFACMI_A, M32R2F_INSN_MVFACMI_A, M32R2F_SFMT_MVFACHI_A, M32R2F_INSN_PAR_MVFACMI_A, M32R2F_INSN_WRITE_MVFACMI_A },
  { M32R_INSN_MVFC, M32R2F_INSN_MVFC, M32R2F_SFMT_MVFC, M32R2F_INSN_PAR_MVFC, M32R2F_INSN_WRITE_MVFC },
  { M32R_INSN_MVTACHI_A, M32R2F_INSN_MVTACHI_A, M32R2F_SFMT_MVTACHI_A, M32R2F_INSN_PAR_MVTACHI_A, M32R2F_INSN_WRITE_MVTACHI_A },
  { M32R_INSN_MVTACLO_A, M32R2F_INSN_MVTACLO_A, M32R2F_SFMT_MVTACHI_A, M32R2F_INSN_PAR_MVTACLO_A, M32R2F_INSN_WRITE_MVTACLO_A },
  { M32R_INSN_MVTC, M32R2F_INSN_MVTC, M32R2F_SFMT_MVTC, M32R2F_INSN_PAR_MVTC, M32R2F_INSN_WRITE_MVTC },
  { M32R_INSN_NEG, M32R2F_INSN_NEG, M32R2F_SFMT_MV, M32R2F_INSN_PAR_NEG, M32R2F_INSN_WRITE_NEG },
  { M32R_INSN_NOP, M32R2F_INSN_NOP, M32R2F_SFMT_NOP, M32R2F_INSN_PAR_NOP, M32R2F_INSN_WRITE_NOP },
  { M32R_INSN_NOT, M32R2F_INSN_NOT, M32R2F_SFMT_MV, M32R2F_INSN_PAR_NOT, M32R2F_INSN_WRITE_NOT },
  { M32R_INSN_RAC_DSI, M32R2F_INSN_RAC_DSI, M32R2F_SFMT_RAC_DSI, M32R2F_INSN_PAR_RAC_DSI, M32R2F_INSN_WRITE_RAC_DSI },
  { M32R_INSN_RACH_DSI, M32R2F_INSN_RACH_DSI, M32R2F_SFMT_RAC_DSI, M32R2F_INSN_PAR_RACH_DSI, M32R2F_INSN_WRITE_RACH_DSI },
  { M32R_INSN_RTE, M32R2F_INSN_RTE, M32R2F_SFMT_RTE, M32R2F_INSN_PAR_RTE, M32R2F_INSN_WRITE_RTE },
  { M32R_INSN_SETH, M32R2F_INSN_SETH, M32R2F_SFMT_SETH, NOPAR, NOPAR  },
  { M32R_INSN_SLL, M32R2F_INSN_SLL, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_SLL, M32R2F_INSN_WRITE_SLL },
  { M32R_INSN_SLL3, M32R2F_INSN_SLL3, M32R2F_SFMT_SLL3, NOPAR, NOPAR  },
  { M32R_INSN_SLLI, M32R2F_INSN_SLLI, M32R2F_SFMT_SLLI, M32R2F_INSN_PAR_SLLI, M32R2F_INSN_WRITE_SLLI },
  { M32R_INSN_SRA, M32R2F_INSN_SRA, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_SRA, M32R2F_INSN_WRITE_SRA },
  { M32R_INSN_SRA3, M32R2F_INSN_SRA3, M32R2F_SFMT_SLL3, NOPAR, NOPAR  },
  { M32R_INSN_SRAI, M32R2F_INSN_SRAI, M32R2F_SFMT_SLLI, M32R2F_INSN_PAR_SRAI, M32R2F_INSN_WRITE_SRAI },
  { M32R_INSN_SRL, M32R2F_INSN_SRL, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_SRL, M32R2F_INSN_WRITE_SRL },
  { M32R_INSN_SRL3, M32R2F_INSN_SRL3, M32R2F_SFMT_SLL3, NOPAR, NOPAR  },
  { M32R_INSN_SRLI, M32R2F_INSN_SRLI, M32R2F_SFMT_SLLI, M32R2F_INSN_PAR_SRLI, M32R2F_INSN_WRITE_SRLI },
  { M32R_INSN_ST, M32R2F_INSN_ST, M32R2F_SFMT_ST, M32R2F_INSN_PAR_ST, M32R2F_INSN_WRITE_ST },
  { M32R_INSN_ST_D, M32R2F_INSN_ST_D, M32R2F_SFMT_ST_D, NOPAR, NOPAR  },
  { M32R_INSN_STB, M32R2F_INSN_STB, M32R2F_SFMT_STB, M32R2F_INSN_PAR_STB, M32R2F_INSN_WRITE_STB },
  { M32R_INSN_STB_D, M32R2F_INSN_STB_D, M32R2F_SFMT_STB_D, NOPAR, NOPAR  },
  { M32R_INSN_STH, M32R2F_INSN_STH, M32R2F_SFMT_STH, M32R2F_INSN_PAR_STH, M32R2F_INSN_WRITE_STH },
  { M32R_INSN_STH_D, M32R2F_INSN_STH_D, M32R2F_SFMT_STH_D, NOPAR, NOPAR  },
  { M32R_INSN_ST_PLUS, M32R2F_INSN_ST_PLUS, M32R2F_SFMT_ST_PLUS, M32R2F_INSN_PAR_ST_PLUS, M32R2F_INSN_WRITE_ST_PLUS },
  { M32R_INSN_STH_PLUS, M32R2F_INSN_STH_PLUS, M32R2F_SFMT_STH_PLUS, M32R2F_INSN_PAR_STH_PLUS, M32R2F_INSN_WRITE_STH_PLUS },
  { M32R_INSN_STB_PLUS, M32R2F_INSN_STB_PLUS, M32R2F_SFMT_STB_PLUS, M32R2F_INSN_PAR_STB_PLUS, M32R2F_INSN_WRITE_STB_PLUS },
  { M32R_INSN_ST_MINUS, M32R2F_INSN_ST_MINUS, M32R2F_SFMT_ST_PLUS, M32R2F_INSN_PAR_ST_MINUS, M32R2F_INSN_WRITE_ST_MINUS },
  { M32R_INSN_SUB, M32R2F_INSN_SUB, M32R2F_SFMT_ADD, M32R2F_INSN_PAR_SUB, M32R2F_INSN_WRITE_SUB },
  { M32R_INSN_SUBV, M32R2F_INSN_SUBV, M32R2F_SFMT_ADDV, M32R2F_INSN_PAR_SUBV, M32R2F_INSN_WRITE_SUBV },
  { M32R_INSN_SUBX, M32R2F_INSN_SUBX, M32R2F_SFMT_ADDX, M32R2F_INSN_PAR_SUBX, M32R2F_INSN_WRITE_SUBX },
  { M32R_INSN_TRAP, M32R2F_INSN_TRAP, M32R2F_SFMT_TRAP, M32R2F_INSN_PAR_TRAP, M32R2F_INSN_WRITE_TRAP },
  { M32R_INSN_UNLOCK, M32R2F_INSN_UNLOCK, M32R2F_SFMT_UNLOCK, M32R2F_INSN_PAR_UNLOCK, M32R2F_INSN_WRITE_UNLOCK },
  { M32R_INSN_SATB, M32R2F_INSN_SATB, M32R2F_SFMT_SATB, NOPAR, NOPAR  },
  { M32R_INSN_SATH, M32R2F_INSN_SATH, M32R2F_SFMT_SATB, NOPAR, NOPAR  },
  { M32R_INSN_SAT, M32R2F_INSN_SAT, M32R2F_SFMT_SAT, NOPAR, NOPAR  },
  { M32R_INSN_PCMPBZ, M32R2F_INSN_PCMPBZ, M32R2F_SFMT_CMPZ, M32R2F_INSN_PAR_PCMPBZ, M32R2F_INSN_WRITE_PCMPBZ },
  { M32R_INSN_SADD, M32R2F_INSN_SADD, M32R2F_SFMT_SADD, M32R2F_INSN_PAR_SADD, M32R2F_INSN_WRITE_SADD },
  { M32R_INSN_MACWU1, M32R2F_INSN_MACWU1, M32R2F_SFMT_MACWU1, M32R2F_INSN_PAR_MACWU1, M32R2F_INSN_WRITE_MACWU1 },
  { M32R_INSN_MSBLO, M32R2F_INSN_MSBLO, M32R2F_SFMT_MSBLO, M32R2F_INSN_PAR_MSBLO, M32R2F_INSN_WRITE_MSBLO },
  { M32R_INSN_MULWU1, M32R2F_INSN_MULWU1, M32R2F_SFMT_MULWU1, M32R2F_INSN_PAR_MULWU1, M32R2F_INSN_WRITE_MULWU1 },
  { M32R_INSN_MACLH1, M32R2F_INSN_MACLH1, M32R2F_SFMT_MACWU1, M32R2F_INSN_PAR_MACLH1, M32R2F_INSN_WRITE_MACLH1 },
  { M32R_INSN_SC, M32R2F_INSN_SC, M32R2F_SFMT_SC, M32R2F_INSN_PAR_SC, M32R2F_INSN_WRITE_SC },
  { M32R_INSN_SNC, M32R2F_INSN_SNC, M32R2F_SFMT_SC, M32R2F_INSN_PAR_SNC, M32R2F_INSN_WRITE_SNC },
  { M32R_INSN_CLRPSW, M32R2F_INSN_CLRPSW, M32R2F_SFMT_CLRPSW, M32R2F_INSN_PAR_CLRPSW, M32R2F_INSN_WRITE_CLRPSW },
  { M32R_INSN_SETPSW, M32R2F_INSN_SETPSW, M32R2F_SFMT_SETPSW, M32R2F_INSN_PAR_SETPSW, M32R2F_INSN_WRITE_SETPSW },
  { M32R_INSN_BSET, M32R2F_INSN_BSET, M32R2F_SFMT_BSET, NOPAR, NOPAR  },
  { M32R_INSN_BCLR, M32R2F_INSN_BCLR, M32R2F_SFMT_BSET, NOPAR, NOPAR  },
  { M32R_INSN_BTST, M32R2F_INSN_BTST, M32R2F_SFMT_BTST, M32R2F_INSN_PAR_BTST, M32R2F_INSN_WRITE_BTST },
};

static const struct insn_sem m32r2f_insn_sem_invalid = {
  VIRTUAL_INSN_X_INVALID, M32R2F_INSN_X_INVALID, M32R2F_SFMT_EMPTY, NOPAR, NOPAR
};

/* Initialize an IDESC from the compile-time computable parts.  */

static INLINE void
init_idesc (SIM_CPU *cpu, IDESC *id, const struct insn_sem *t)
{
  const CGEN_INSN *insn_table = CGEN_CPU_INSN_TABLE (CPU_CPU_DESC (cpu))->init_entries;

  id->num = t->index;
  id->sfmt = t->sfmt;
  if ((int) t->type <= 0)
    id->idata = & cgen_virtual_insn_table[- (int) t->type];
  else
    id->idata = & insn_table[t->type];
  id->attrs = CGEN_INSN_ATTRS (id->idata);
  /* Oh my god, a magic number.  */
  id->length = CGEN_INSN_BITSIZE (id->idata) / 8;

#if WITH_PROFILE_MODEL_P
  id->timing = & MODEL_TIMING (CPU_MODEL (cpu)) [t->index];
  {
    SIM_DESC sd = CPU_STATE (cpu);
    SIM_ASSERT (t->index == id->timing->num);
  }
#endif

  /* Semantic pointers are initialized elsewhere.  */
}

/* Initialize the instruction descriptor table.  */

void
m32r2f_init_idesc_table (SIM_CPU *cpu)
{
  IDESC *id,*tabend;
  const struct insn_sem *t,*tend;
  int tabsize = M32R2F_INSN__MAX;
  IDESC *table = m32r2f_insn_data;

  memset (table, 0, tabsize * sizeof (IDESC));

  /* First set all entries to the `invalid insn'.  */
  t = & m32r2f_insn_sem_invalid;
  for (id = table, tabend = table + tabsize; id < tabend; ++id)
    init_idesc (cpu, id, t);

  /* Now fill in the values for the chosen cpu.  */
  for (t = m32r2f_insn_sem, tend = t + sizeof (m32r2f_insn_sem) / sizeof (*t);
       t != tend; ++t)
    {
      init_idesc (cpu, & table[t->index], t);
      if (t->par_index != NOPAR)
	{
	  init_idesc (cpu, &table[t->par_index], t);
	  table[t->index].par_idesc = &table[t->par_index];
	}
      if (t->par_index != NOPAR)
	{
	  init_idesc (cpu, &table[t->write_index], t);
	  table[t->par_index].par_idesc = &table[t->write_index];
	}
    }

  /* Link the IDESC table into the cpu.  */
  CPU_IDESC (cpu) = table;
}

/* Given an instruction, return a pointer to its IDESC entry.  */

const IDESC *
m32r2f_decode (SIM_CPU *current_cpu, IADDR pc,
              CGEN_INSN_INT base_insn, CGEN_INSN_INT entire_insn,
              ARGBUF *abuf)
{
  /* Result of decoder.  */
  M32R2F_INSN_TYPE itype;

  {
    CGEN_INSN_INT insn = base_insn;

    {
      unsigned int val = (((insn >> 8) & (15 << 4)) | ((insn >> 4) & (15 << 0)));
      switch (val)
      {
      case 0 : itype = M32R2F_INSN_SUBV; goto extract_sfmt_addv;
      case 1 : itype = M32R2F_INSN_SUBX; goto extract_sfmt_addx;
      case 2 : itype = M32R2F_INSN_SUB; goto extract_sfmt_add;
      case 3 : itype = M32R2F_INSN_NEG; goto extract_sfmt_mv;
      case 4 : itype = M32R2F_INSN_CMP; goto extract_sfmt_cmp;
      case 5 : itype = M32R2F_INSN_CMPU; goto extract_sfmt_cmp;
      case 6 : itype = M32R2F_INSN_CMPEQ; goto extract_sfmt_cmp;
      case 7 :
        {
          unsigned int val = (((insn >> 8) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_CMPZ; goto extract_sfmt_cmpz;
          case 3 : itype = M32R2F_INSN_PCMPBZ; goto extract_sfmt_cmpz;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 8 : itype = M32R2F_INSN_ADDV; goto extract_sfmt_addv;
      case 9 : itype = M32R2F_INSN_ADDX; goto extract_sfmt_addx;
      case 10 : itype = M32R2F_INSN_ADD; goto extract_sfmt_add;
      case 11 : itype = M32R2F_INSN_NOT; goto extract_sfmt_mv;
      case 12 : itype = M32R2F_INSN_AND; goto extract_sfmt_add;
      case 13 : itype = M32R2F_INSN_XOR; goto extract_sfmt_add;
      case 14 : itype = M32R2F_INSN_OR; goto extract_sfmt_add;
      case 15 : itype = M32R2F_INSN_BTST; goto extract_sfmt_btst;
      case 16 : itype = M32R2F_INSN_SRL; goto extract_sfmt_add;
      case 18 : itype = M32R2F_INSN_SRA; goto extract_sfmt_add;
      case 20 : itype = M32R2F_INSN_SLL; goto extract_sfmt_add;
      case 22 : itype = M32R2F_INSN_MUL; goto extract_sfmt_add;
      case 24 : itype = M32R2F_INSN_MV; goto extract_sfmt_mv;
      case 25 : itype = M32R2F_INSN_MVFC; goto extract_sfmt_mvfc;
      case 26 : itype = M32R2F_INSN_MVTC; goto extract_sfmt_mvtc;
      case 28 :
        {
          unsigned int val = (((insn >> 8) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_JC; goto extract_sfmt_jc;
          case 1 : itype = M32R2F_INSN_JNC; goto extract_sfmt_jc;
          case 2 : itype = M32R2F_INSN_JL; goto extract_sfmt_jl;
          case 3 : itype = M32R2F_INSN_JMP; goto extract_sfmt_jmp;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 29 : itype = M32R2F_INSN_RTE; goto extract_sfmt_rte;
      case 31 : itype = M32R2F_INSN_TRAP; goto extract_sfmt_trap;
      case 32 : itype = M32R2F_INSN_STB; goto extract_sfmt_stb;
      case 33 : itype = M32R2F_INSN_STB_PLUS; goto extract_sfmt_stb_plus;
      case 34 : itype = M32R2F_INSN_STH; goto extract_sfmt_sth;
      case 35 : itype = M32R2F_INSN_STH_PLUS; goto extract_sfmt_sth_plus;
      case 36 : itype = M32R2F_INSN_ST; goto extract_sfmt_st;
      case 37 : itype = M32R2F_INSN_UNLOCK; goto extract_sfmt_unlock;
      case 38 : itype = M32R2F_INSN_ST_PLUS; goto extract_sfmt_st_plus;
      case 39 : itype = M32R2F_INSN_ST_MINUS; goto extract_sfmt_st_plus;
      case 40 : itype = M32R2F_INSN_LDB; goto extract_sfmt_ldb;
      case 41 : itype = M32R2F_INSN_LDUB; goto extract_sfmt_ldb;
      case 42 : itype = M32R2F_INSN_LDH; goto extract_sfmt_ldh;
      case 43 : itype = M32R2F_INSN_LDUH; goto extract_sfmt_ldh;
      case 44 : itype = M32R2F_INSN_LD; goto extract_sfmt_ld;
      case 45 : itype = M32R2F_INSN_LOCK; goto extract_sfmt_lock;
      case 46 : itype = M32R2F_INSN_LD_PLUS; goto extract_sfmt_ld_plus;
      case 48 : /* fall through */
      case 56 : itype = M32R2F_INSN_MULHI_A; goto extract_sfmt_mulhi_a;
      case 49 : /* fall through */
      case 57 : itype = M32R2F_INSN_MULLO_A; goto extract_sfmt_mulhi_a;
      case 50 : /* fall through */
      case 58 : itype = M32R2F_INSN_MULWHI_A; goto extract_sfmt_mulhi_a;
      case 51 : /* fall through */
      case 59 : itype = M32R2F_INSN_MULWLO_A; goto extract_sfmt_mulhi_a;
      case 52 : /* fall through */
      case 60 : itype = M32R2F_INSN_MACHI_A; goto extract_sfmt_machi_a;
      case 53 : /* fall through */
      case 61 : itype = M32R2F_INSN_MACLO_A; goto extract_sfmt_machi_a;
      case 54 : /* fall through */
      case 62 : itype = M32R2F_INSN_MACWHI_A; goto extract_sfmt_machi_a;
      case 55 : /* fall through */
      case 63 : itype = M32R2F_INSN_MACWLO_A; goto extract_sfmt_machi_a;
      case 64 : /* fall through */
      case 65 : /* fall through */
      case 66 : /* fall through */
      case 67 : /* fall through */
      case 68 : /* fall through */
      case 69 : /* fall through */
      case 70 : /* fall through */
      case 71 : /* fall through */
      case 72 : /* fall through */
      case 73 : /* fall through */
      case 74 : /* fall through */
      case 75 : /* fall through */
      case 76 : /* fall through */
      case 77 : /* fall through */
      case 78 : /* fall through */
      case 79 : itype = M32R2F_INSN_ADDI; goto extract_sfmt_addi;
      case 80 : /* fall through */
      case 81 : itype = M32R2F_INSN_SRLI; goto extract_sfmt_slli;
      case 82 : /* fall through */
      case 83 : itype = M32R2F_INSN_SRAI; goto extract_sfmt_slli;
      case 84 : /* fall through */
      case 85 : itype = M32R2F_INSN_SLLI; goto extract_sfmt_slli;
      case 87 :
        {
          unsigned int val = (((insn >> 0) & (1 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_MVTACHI_A; goto extract_sfmt_mvtachi_a;
          case 1 : itype = M32R2F_INSN_MVTACLO_A; goto extract_sfmt_mvtachi_a;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 88 : itype = M32R2F_INSN_RACH_DSI; goto extract_sfmt_rac_dsi;
      case 89 : itype = M32R2F_INSN_RAC_DSI; goto extract_sfmt_rac_dsi;
      case 90 : itype = M32R2F_INSN_MULWU1; goto extract_sfmt_mulwu1;
      case 91 : itype = M32R2F_INSN_MACWU1; goto extract_sfmt_macwu1;
      case 92 : itype = M32R2F_INSN_MACLH1; goto extract_sfmt_macwu1;
      case 93 : itype = M32R2F_INSN_MSBLO; goto extract_sfmt_msblo;
      case 94 : itype = M32R2F_INSN_SADD; goto extract_sfmt_sadd;
      case 95 :
        {
          unsigned int val = (((insn >> 0) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_MVFACHI_A; goto extract_sfmt_mvfachi_a;
          case 1 : itype = M32R2F_INSN_MVFACLO_A; goto extract_sfmt_mvfachi_a;
          case 2 : itype = M32R2F_INSN_MVFACMI_A; goto extract_sfmt_mvfachi_a;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 96 : /* fall through */
      case 97 : /* fall through */
      case 98 : /* fall through */
      case 99 : /* fall through */
      case 100 : /* fall through */
      case 101 : /* fall through */
      case 102 : /* fall through */
      case 103 : /* fall through */
      case 104 : /* fall through */
      case 105 : /* fall through */
      case 106 : /* fall through */
      case 107 : /* fall through */
      case 108 : /* fall through */
      case 109 : /* fall through */
      case 110 : /* fall through */
      case 111 : itype = M32R2F_INSN_LDI8; goto extract_sfmt_ldi8;
      case 112 :
        {
          unsigned int val = (((insn >> 7) & (15 << 1)) | ((insn >> 0) & (1 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_NOP; goto extract_sfmt_nop;
          case 2 : /* fall through */
          case 3 : itype = M32R2F_INSN_SETPSW; goto extract_sfmt_setpsw;
          case 4 : /* fall through */
          case 5 : itype = M32R2F_INSN_CLRPSW; goto extract_sfmt_clrpsw;
          case 9 : itype = M32R2F_INSN_SC; goto extract_sfmt_sc;
          case 11 : itype = M32R2F_INSN_SNC; goto extract_sfmt_sc;
          case 16 : /* fall through */
          case 17 : itype = M32R2F_INSN_BCL8; goto extract_sfmt_bcl8;
          case 18 : /* fall through */
          case 19 : itype = M32R2F_INSN_BNCL8; goto extract_sfmt_bcl8;
          case 24 : /* fall through */
          case 25 : itype = M32R2F_INSN_BC8; goto extract_sfmt_bc8;
          case 26 : /* fall through */
          case 27 : itype = M32R2F_INSN_BNC8; goto extract_sfmt_bc8;
          case 28 : /* fall through */
          case 29 : itype = M32R2F_INSN_BL8; goto extract_sfmt_bl8;
          case 30 : /* fall through */
          case 31 : itype = M32R2F_INSN_BRA8; goto extract_sfmt_bra8;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 113 : /* fall through */
      case 114 : /* fall through */
      case 115 : /* fall through */
      case 116 : /* fall through */
      case 117 : /* fall through */
      case 118 : /* fall through */
      case 119 : /* fall through */
      case 120 : /* fall through */
      case 121 : /* fall through */
      case 122 : /* fall through */
      case 123 : /* fall through */
      case 124 : /* fall through */
      case 125 : /* fall through */
      case 126 : /* fall through */
      case 127 :
        {
          unsigned int val = (((insn >> 8) & (15 << 0)));
          switch (val)
          {
          case 1 : itype = M32R2F_INSN_SETPSW; goto extract_sfmt_setpsw;
          case 2 : itype = M32R2F_INSN_CLRPSW; goto extract_sfmt_clrpsw;
          case 8 : itype = M32R2F_INSN_BCL8; goto extract_sfmt_bcl8;
          case 9 : itype = M32R2F_INSN_BNCL8; goto extract_sfmt_bcl8;
          case 12 : itype = M32R2F_INSN_BC8; goto extract_sfmt_bc8;
          case 13 : itype = M32R2F_INSN_BNC8; goto extract_sfmt_bc8;
          case 14 : itype = M32R2F_INSN_BL8; goto extract_sfmt_bl8;
          case 15 : itype = M32R2F_INSN_BRA8; goto extract_sfmt_bra8;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 132 : itype = M32R2F_INSN_CMPI; goto extract_sfmt_cmpi;
      case 133 : itype = M32R2F_INSN_CMPUI; goto extract_sfmt_cmpi;
      case 134 :
        {
          unsigned int val = (((insn >> -8) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_SAT; goto extract_sfmt_sat;
          case 2 : itype = M32R2F_INSN_SATH; goto extract_sfmt_satb;
          case 3 : itype = M32R2F_INSN_SATB; goto extract_sfmt_satb;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 136 : itype = M32R2F_INSN_ADDV3; goto extract_sfmt_addv3;
      case 138 : itype = M32R2F_INSN_ADD3; goto extract_sfmt_add3;
      case 140 : itype = M32R2F_INSN_AND3; goto extract_sfmt_and3;
      case 141 : itype = M32R2F_INSN_XOR3; goto extract_sfmt_and3;
      case 142 : itype = M32R2F_INSN_OR3; goto extract_sfmt_or3;
      case 144 :
        {
          unsigned int val = (((insn >> -13) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_DIV; goto extract_sfmt_div;
          case 2 : itype = M32R2F_INSN_DIVH; goto extract_sfmt_div;
          case 3 : itype = M32R2F_INSN_DIVB; goto extract_sfmt_div;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 145 :
        {
          unsigned int val = (((insn >> -13) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_DIVU; goto extract_sfmt_div;
          case 2 : itype = M32R2F_INSN_DIVUH; goto extract_sfmt_div;
          case 3 : itype = M32R2F_INSN_DIVUB; goto extract_sfmt_div;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 146 :
        {
          unsigned int val = (((insn >> -13) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_REM; goto extract_sfmt_div;
          case 2 : itype = M32R2F_INSN_REMH; goto extract_sfmt_div;
          case 3 : itype = M32R2F_INSN_REMB; goto extract_sfmt_div;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 147 :
        {
          unsigned int val = (((insn >> -13) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_REMU; goto extract_sfmt_div;
          case 2 : itype = M32R2F_INSN_REMUH; goto extract_sfmt_div;
          case 3 : itype = M32R2F_INSN_REMUB; goto extract_sfmt_div;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 152 : itype = M32R2F_INSN_SRL3; goto extract_sfmt_sll3;
      case 154 : itype = M32R2F_INSN_SRA3; goto extract_sfmt_sll3;
      case 156 : itype = M32R2F_INSN_SLL3; goto extract_sfmt_sll3;
      case 159 : itype = M32R2F_INSN_LDI16; goto extract_sfmt_ldi16;
      case 160 : itype = M32R2F_INSN_STB_D; goto extract_sfmt_stb_d;
      case 162 : itype = M32R2F_INSN_STH_D; goto extract_sfmt_sth_d;
      case 164 : itype = M32R2F_INSN_ST_D; goto extract_sfmt_st_d;
      case 166 : itype = M32R2F_INSN_BSET; goto extract_sfmt_bset;
      case 167 : itype = M32R2F_INSN_BCLR; goto extract_sfmt_bset;
      case 168 : itype = M32R2F_INSN_LDB_D; goto extract_sfmt_ldb_d;
      case 169 : itype = M32R2F_INSN_LDUB_D; goto extract_sfmt_ldb_d;
      case 170 : itype = M32R2F_INSN_LDH_D; goto extract_sfmt_ldh_d;
      case 171 : itype = M32R2F_INSN_LDUH_D; goto extract_sfmt_ldh_d;
      case 172 : itype = M32R2F_INSN_LD_D; goto extract_sfmt_ld_d;
      case 176 : itype = M32R2F_INSN_BEQ; goto extract_sfmt_beq;
      case 177 : itype = M32R2F_INSN_BNE; goto extract_sfmt_beq;
      case 184 : itype = M32R2F_INSN_BEQZ; goto extract_sfmt_beqz;
      case 185 : itype = M32R2F_INSN_BNEZ; goto extract_sfmt_beqz;
      case 186 : itype = M32R2F_INSN_BLTZ; goto extract_sfmt_beqz;
      case 187 : itype = M32R2F_INSN_BGEZ; goto extract_sfmt_beqz;
      case 188 : itype = M32R2F_INSN_BLEZ; goto extract_sfmt_beqz;
      case 189 : itype = M32R2F_INSN_BGTZ; goto extract_sfmt_beqz;
      case 220 : itype = M32R2F_INSN_SETH; goto extract_sfmt_seth;
      case 224 : /* fall through */
      case 225 : /* fall through */
      case 226 : /* fall through */
      case 227 : /* fall through */
      case 228 : /* fall through */
      case 229 : /* fall through */
      case 230 : /* fall through */
      case 231 : /* fall through */
      case 232 : /* fall through */
      case 233 : /* fall through */
      case 234 : /* fall through */
      case 235 : /* fall through */
      case 236 : /* fall through */
      case 237 : /* fall through */
      case 238 : /* fall through */
      case 239 : itype = M32R2F_INSN_LD24; goto extract_sfmt_ld24;
      case 240 : /* fall through */
      case 241 : /* fall through */
      case 242 : /* fall through */
      case 243 : /* fall through */
      case 244 : /* fall through */
      case 245 : /* fall through */
      case 246 : /* fall through */
      case 247 : /* fall through */
      case 248 : /* fall through */
      case 249 : /* fall through */
      case 250 : /* fall through */
      case 251 : /* fall through */
      case 252 : /* fall through */
      case 253 : /* fall through */
      case 254 : /* fall through */
      case 255 :
        {
          unsigned int val = (((insn >> 8) & (7 << 0)));
          switch (val)
          {
          case 0 : itype = M32R2F_INSN_BCL24; goto extract_sfmt_bcl24;
          case 1 : itype = M32R2F_INSN_BNCL24; goto extract_sfmt_bcl24;
          case 4 : itype = M32R2F_INSN_BC24; goto extract_sfmt_bc24;
          case 5 : itype = M32R2F_INSN_BNC24; goto extract_sfmt_bc24;
          case 6 : itype = M32R2F_INSN_BL24; goto extract_sfmt_bl24;
          case 7 : itype = M32R2F_INSN_BRA24; goto extract_sfmt_bra24;
          default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      default : itype = M32R2F_INSN_X_INVALID; goto extract_sfmt_empty;
      }
    }
  }

  /* The instruction has been decoded, now extract the fields.  */

 extract_sfmt_empty:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_empty", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_add:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add3:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add3", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and3:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_and3.f
    UINT f_r1;
    UINT f_r2;
    UINT f_uimm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_uimm16) = f_uimm16;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and3", "f_r2 0x%x", 'x', f_r2, "f_uimm16 0x%x", 'x', f_uimm16, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_or3:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_and3.f
    UINT f_r1;
    UINT f_r2;
    UINT f_uimm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_uimm16) = f_uimm16;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_or3", "f_r2 0x%x", 'x', f_r2, "f_uimm16 0x%x", 'x', f_uimm16, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addi:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_r1;
    INT f_simm8;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_simm8 = EXTRACT_MSB0_INT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_simm8) = f_simm8;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi", "f_r1 0x%x", 'x', f_r1, "f_simm8 0x%x", 'x', f_simm8, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addv:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addv", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addv3:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addv3", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addx:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addx", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bc8:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl8.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bc8", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bc24:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl24.f
    SI f_disp24;

    f_disp24 = ((((EXTRACT_MSB0_INT (insn, 32, 8, 24)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp24) = f_disp24;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bc24", "disp24 0x%x", 'x', f_disp24, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_beq:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beq.f
    UINT f_r1;
    UINT f_r2;
    SI f_disp16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 16, 16)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_disp16) = f_disp16;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_beq", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "disp16 0x%x", 'x', f_disp16, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_beqz:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beq.f
    UINT f_r2;
    SI f_disp16;

    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 16, 16)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (i_disp16) = f_disp16;
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_beqz", "f_r2 0x%x", 'x', f_r2, "disp16 0x%x", 'x', f_disp16, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bl8:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl8.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bl8", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bl24:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl24.f
    SI f_disp24;

    f_disp24 = ((((EXTRACT_MSB0_INT (insn, 32, 8, 24)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp24) = f_disp24;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bl24", "disp24 0x%x", 'x', f_disp24, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bcl8:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl8.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bcl8", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bcl24:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl24.f
    SI f_disp24;

    f_disp24 = ((((EXTRACT_MSB0_INT (insn, 32, 8, 24)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp24) = f_disp24;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bcl24", "disp24 0x%x", 'x', f_disp24, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bra8:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl8.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bra8", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bra24:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bl24.f
    SI f_disp24;

    f_disp24 = ((((EXTRACT_MSB0_INT (insn, 32, 8, 24)) << (2))) + (pc));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp24) = f_disp24;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bra24", "disp24 0x%x", 'x', f_disp24, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmp:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmp", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpi:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_d.f
    UINT f_r2;
    INT f_simm16;

    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpi", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpz:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r2;

    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpz", "f_r2 0x%x", 'x', f_r2, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_div:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_div", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jc:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_jl.f
    UINT f_r2;

    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jc", "f_r2 0x%x", 'x', f_r2, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jl:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_jl.f
    UINT f_r2;

    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jl", "f_r2 0x%x", 'x', f_r2, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jmp:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_jl.f
    UINT f_r2;

    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jmp", "f_r2 0x%x", 'x', f_r2, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ld:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ld", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ld_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ld_d", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldb:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldb", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldb_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldb_d", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldh:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldh", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldh_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldh_d", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ld_plus:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ld_plus", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
      FLD (out_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ld24:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld24.f
    UINT f_r1;
    UINT f_uimm24;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_uimm24 = EXTRACT_MSB0_UINT (insn, 32, 8, 24);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (i_uimm24) = f_uimm24;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ld24", "f_r1 0x%x", 'x', f_r1, "uimm24 0x%x", 'x', f_uimm24, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldi8:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_r1;
    INT f_simm8;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_simm8 = EXTRACT_MSB0_INT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm8) = f_simm8;
  FLD (f_r1) = f_r1;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldi8", "f_simm8 0x%x", 'x', f_simm8, "f_r1 0x%x", 'x', f_r1, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldi16:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r1) = f_r1;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldi16", "f_simm16 0x%x", 'x', f_simm16, "f_r1 0x%x", 'x', f_r1, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lock:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lock", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_machi_a:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_machi_a.f
    UINT f_r1;
    UINT f_acc;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_acc = EXTRACT_MSB0_UINT (insn, 16, 8, 1);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_acc) = f_acc;
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_machi_a", "f_acc 0x%x", 'x', f_acc, "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mulhi_a:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_machi_a.f
    UINT f_r1;
    UINT f_acc;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_acc = EXTRACT_MSB0_UINT (insn, 16, 8, 1);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (f_acc) = f_acc;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mulhi_a", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "f_acc 0x%x", 'x', f_acc, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mv:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mv", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mvfachi_a:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
    UINT f_r1;
    UINT f_accs;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2);

  /* Record the fields for the semantic handler.  */
  FLD (f_accs) = f_accs;
  FLD (f_r1) = f_r1;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mvfachi_a", "f_accs 0x%x", 'x', f_accs, "f_r1 0x%x", 'x', f_r1, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mvfc:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mvfc", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mvtachi_a:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
    UINT f_r1;
    UINT f_accs;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2);

  /* Record the fields for the semantic handler.  */
  FLD (f_accs) = f_accs;
  FLD (f_r1) = f_r1;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mvtachi_a", "f_accs 0x%x", 'x', f_accs, "f_r1 0x%x", 'x', f_r1, "src1 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mvtc:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mvtc", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_nop:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_nop", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_rac_dsi:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
    UINT f_accd;
    UINT f_accs;
    SI f_imm1;

    f_accd = EXTRACT_MSB0_UINT (insn, 16, 4, 2);
    f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2);
    f_imm1 = ((EXTRACT_MSB0_UINT (insn, 16, 15, 1)) + (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_accs) = f_accs;
  FLD (f_imm1) = f_imm1;
  FLD (f_accd) = f_accd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_rac_dsi", "f_accs 0x%x", 'x', f_accs, "f_imm1 0x%x", 'x', f_imm1, "f_accd 0x%x", 'x', f_accd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_rte:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_rte", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_seth:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_seth.f
    UINT f_r1;
    UINT f_hi16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_hi16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_hi16) = f_hi16;
  FLD (f_r1) = f_r1;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_seth", "f_hi16 0x%x", 'x', f_hi16, "f_r1 0x%x", 'x', f_r1, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sll3:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add3.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sll3", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_slli:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_slli.f
    UINT f_r1;
    UINT f_uimm5;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_uimm5 = EXTRACT_MSB0_UINT (insn, 16, 11, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_uimm5) = f_uimm5;
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_slli", "f_r1 0x%x", 'x', f_r1, "f_uimm5 0x%x", 'x', f_uimm5, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_dr) = f_r1;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_st:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_st", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_st_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_d.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_st_d", "f_simm16 0x%x", 'x', f_simm16, "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stb:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stb", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stb_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_d.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stb_d", "f_simm16 0x%x", 'x', f_simm16, "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sth:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sth", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sth_d:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_d.f
    UINT f_r1;
    UINT f_r2;
    INT f_simm16;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sth_d", "f_simm16 0x%x", 'x', f_simm16, "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_st_plus:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_st_plus", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
      FLD (out_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sth_plus:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sth_plus", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
      FLD (out_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stb_plus:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stb_plus", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
      FLD (out_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_trap:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_trap.f
    UINT f_uimm4;

    f_uimm4 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_uimm4) = f_uimm4;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_trap", "f_uimm4 0x%x", 'x', f_uimm4, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_unlock:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_unlock", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_satb:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_satb", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sat:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_r1) = f_r1;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  FLD (i_dr) = & CPU (h_gr)[f_r1];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sat", "f_r2 0x%x", 'x', f_r2, "f_r1 0x%x", 'x', f_r1, "sr 0x%x", 'x', f_r2, "dr 0x%x", 'x', f_r1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
      FLD (out_dr) = f_r1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sadd:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sadd", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_macwu1:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_macwu1", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_msblo:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_msblo", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mulwu1:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_st_plus.f
    UINT f_r1;
    UINT f_r2;

    f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r1) = f_r1;
  FLD (f_r2) = f_r2;
  FLD (i_src1) = & CPU (h_gr)[f_r1];
  FLD (i_src2) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mulwu1", "f_r1 0x%x", 'x', f_r1, "f_r2 0x%x", 'x', f_r2, "src1 0x%x", 'x', f_r1, "src2 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_src1) = f_r1;
      FLD (in_src2) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sc:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sc", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_clrpsw:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_clrpsw.f
    UINT f_uimm8;

    f_uimm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_uimm8) = f_uimm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_clrpsw", "f_uimm8 0x%x", 'x', f_uimm8, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_setpsw:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_clrpsw.f
    UINT f_uimm8;

    f_uimm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_uimm8) = f_uimm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_setpsw", "f_uimm8 0x%x", 'x', f_uimm8, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_bset:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bset.f
    UINT f_uimm3;
    UINT f_r2;
    INT f_simm16;

    f_uimm3 = EXTRACT_MSB0_UINT (insn, 32, 5, 3);
    f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4);
    f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_simm16) = f_simm16;
  FLD (f_r2) = f_r2;
  FLD (f_uimm3) = f_uimm3;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bset", "f_simm16 0x%x", 'x', f_simm16, "f_r2 0x%x", 'x', f_r2, "f_uimm3 0x%x", 'x', f_uimm3, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_btst:
  {
    const IDESC *idesc = &m32r2f_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bset.f
    UINT f_uimm3;
    UINT f_r2;

    f_uimm3 = EXTRACT_MSB0_UINT (insn, 16, 5, 3);
    f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_r2) = f_r2;
  FLD (f_uimm3) = f_uimm3;
  FLD (i_sr) = & CPU (h_gr)[f_r2];
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_btst", "f_r2 0x%x", 'x', f_r2, "f_uimm3 0x%x", 'x', f_uimm3, "sr 0x%x", 'x', f_r2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_sr) = f_r2;
    }
#endif
#undef FLD
    return idesc;
  }

}
