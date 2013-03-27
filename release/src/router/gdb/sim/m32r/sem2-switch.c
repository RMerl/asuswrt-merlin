/* Simulator instruction semantics for m32r2f.

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

#ifdef DEFINE_LABELS

  /* The labels have the case they have because the enum of insn types
     is all uppercase and in the non-stdc case the insn symbol is built
     into the enum name.  */

  static struct {
    int index;
    void *label;
  } labels[] = {
    { M32R2F_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { M32R2F_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { M32R2F_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { M32R2F_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { M32R2F_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { M32R2F_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { M32R2F_INSN_ADD, && case_sem_INSN_ADD },
    { M32R2F_INSN_ADD3, && case_sem_INSN_ADD3 },
    { M32R2F_INSN_AND, && case_sem_INSN_AND },
    { M32R2F_INSN_AND3, && case_sem_INSN_AND3 },
    { M32R2F_INSN_OR, && case_sem_INSN_OR },
    { M32R2F_INSN_OR3, && case_sem_INSN_OR3 },
    { M32R2F_INSN_XOR, && case_sem_INSN_XOR },
    { M32R2F_INSN_XOR3, && case_sem_INSN_XOR3 },
    { M32R2F_INSN_ADDI, && case_sem_INSN_ADDI },
    { M32R2F_INSN_ADDV, && case_sem_INSN_ADDV },
    { M32R2F_INSN_ADDV3, && case_sem_INSN_ADDV3 },
    { M32R2F_INSN_ADDX, && case_sem_INSN_ADDX },
    { M32R2F_INSN_BC8, && case_sem_INSN_BC8 },
    { M32R2F_INSN_BC24, && case_sem_INSN_BC24 },
    { M32R2F_INSN_BEQ, && case_sem_INSN_BEQ },
    { M32R2F_INSN_BEQZ, && case_sem_INSN_BEQZ },
    { M32R2F_INSN_BGEZ, && case_sem_INSN_BGEZ },
    { M32R2F_INSN_BGTZ, && case_sem_INSN_BGTZ },
    { M32R2F_INSN_BLEZ, && case_sem_INSN_BLEZ },
    { M32R2F_INSN_BLTZ, && case_sem_INSN_BLTZ },
    { M32R2F_INSN_BNEZ, && case_sem_INSN_BNEZ },
    { M32R2F_INSN_BL8, && case_sem_INSN_BL8 },
    { M32R2F_INSN_BL24, && case_sem_INSN_BL24 },
    { M32R2F_INSN_BCL8, && case_sem_INSN_BCL8 },
    { M32R2F_INSN_BCL24, && case_sem_INSN_BCL24 },
    { M32R2F_INSN_BNC8, && case_sem_INSN_BNC8 },
    { M32R2F_INSN_BNC24, && case_sem_INSN_BNC24 },
    { M32R2F_INSN_BNE, && case_sem_INSN_BNE },
    { M32R2F_INSN_BRA8, && case_sem_INSN_BRA8 },
    { M32R2F_INSN_BRA24, && case_sem_INSN_BRA24 },
    { M32R2F_INSN_BNCL8, && case_sem_INSN_BNCL8 },
    { M32R2F_INSN_BNCL24, && case_sem_INSN_BNCL24 },
    { M32R2F_INSN_CMP, && case_sem_INSN_CMP },
    { M32R2F_INSN_CMPI, && case_sem_INSN_CMPI },
    { M32R2F_INSN_CMPU, && case_sem_INSN_CMPU },
    { M32R2F_INSN_CMPUI, && case_sem_INSN_CMPUI },
    { M32R2F_INSN_CMPEQ, && case_sem_INSN_CMPEQ },
    { M32R2F_INSN_CMPZ, && case_sem_INSN_CMPZ },
    { M32R2F_INSN_DIV, && case_sem_INSN_DIV },
    { M32R2F_INSN_DIVU, && case_sem_INSN_DIVU },
    { M32R2F_INSN_REM, && case_sem_INSN_REM },
    { M32R2F_INSN_REMU, && case_sem_INSN_REMU },
    { M32R2F_INSN_REMH, && case_sem_INSN_REMH },
    { M32R2F_INSN_REMUH, && case_sem_INSN_REMUH },
    { M32R2F_INSN_REMB, && case_sem_INSN_REMB },
    { M32R2F_INSN_REMUB, && case_sem_INSN_REMUB },
    { M32R2F_INSN_DIVUH, && case_sem_INSN_DIVUH },
    { M32R2F_INSN_DIVB, && case_sem_INSN_DIVB },
    { M32R2F_INSN_DIVUB, && case_sem_INSN_DIVUB },
    { M32R2F_INSN_DIVH, && case_sem_INSN_DIVH },
    { M32R2F_INSN_JC, && case_sem_INSN_JC },
    { M32R2F_INSN_JNC, && case_sem_INSN_JNC },
    { M32R2F_INSN_JL, && case_sem_INSN_JL },
    { M32R2F_INSN_JMP, && case_sem_INSN_JMP },
    { M32R2F_INSN_LD, && case_sem_INSN_LD },
    { M32R2F_INSN_LD_D, && case_sem_INSN_LD_D },
    { M32R2F_INSN_LDB, && case_sem_INSN_LDB },
    { M32R2F_INSN_LDB_D, && case_sem_INSN_LDB_D },
    { M32R2F_INSN_LDH, && case_sem_INSN_LDH },
    { M32R2F_INSN_LDH_D, && case_sem_INSN_LDH_D },
    { M32R2F_INSN_LDUB, && case_sem_INSN_LDUB },
    { M32R2F_INSN_LDUB_D, && case_sem_INSN_LDUB_D },
    { M32R2F_INSN_LDUH, && case_sem_INSN_LDUH },
    { M32R2F_INSN_LDUH_D, && case_sem_INSN_LDUH_D },
    { M32R2F_INSN_LD_PLUS, && case_sem_INSN_LD_PLUS },
    { M32R2F_INSN_LD24, && case_sem_INSN_LD24 },
    { M32R2F_INSN_LDI8, && case_sem_INSN_LDI8 },
    { M32R2F_INSN_LDI16, && case_sem_INSN_LDI16 },
    { M32R2F_INSN_LOCK, && case_sem_INSN_LOCK },
    { M32R2F_INSN_MACHI_A, && case_sem_INSN_MACHI_A },
    { M32R2F_INSN_MACLO_A, && case_sem_INSN_MACLO_A },
    { M32R2F_INSN_MACWHI_A, && case_sem_INSN_MACWHI_A },
    { M32R2F_INSN_MACWLO_A, && case_sem_INSN_MACWLO_A },
    { M32R2F_INSN_MUL, && case_sem_INSN_MUL },
    { M32R2F_INSN_MULHI_A, && case_sem_INSN_MULHI_A },
    { M32R2F_INSN_MULLO_A, && case_sem_INSN_MULLO_A },
    { M32R2F_INSN_MULWHI_A, && case_sem_INSN_MULWHI_A },
    { M32R2F_INSN_MULWLO_A, && case_sem_INSN_MULWLO_A },
    { M32R2F_INSN_MV, && case_sem_INSN_MV },
    { M32R2F_INSN_MVFACHI_A, && case_sem_INSN_MVFACHI_A },
    { M32R2F_INSN_MVFACLO_A, && case_sem_INSN_MVFACLO_A },
    { M32R2F_INSN_MVFACMI_A, && case_sem_INSN_MVFACMI_A },
    { M32R2F_INSN_MVFC, && case_sem_INSN_MVFC },
    { M32R2F_INSN_MVTACHI_A, && case_sem_INSN_MVTACHI_A },
    { M32R2F_INSN_MVTACLO_A, && case_sem_INSN_MVTACLO_A },
    { M32R2F_INSN_MVTC, && case_sem_INSN_MVTC },
    { M32R2F_INSN_NEG, && case_sem_INSN_NEG },
    { M32R2F_INSN_NOP, && case_sem_INSN_NOP },
    { M32R2F_INSN_NOT, && case_sem_INSN_NOT },
    { M32R2F_INSN_RAC_DSI, && case_sem_INSN_RAC_DSI },
    { M32R2F_INSN_RACH_DSI, && case_sem_INSN_RACH_DSI },
    { M32R2F_INSN_RTE, && case_sem_INSN_RTE },
    { M32R2F_INSN_SETH, && case_sem_INSN_SETH },
    { M32R2F_INSN_SLL, && case_sem_INSN_SLL },
    { M32R2F_INSN_SLL3, && case_sem_INSN_SLL3 },
    { M32R2F_INSN_SLLI, && case_sem_INSN_SLLI },
    { M32R2F_INSN_SRA, && case_sem_INSN_SRA },
    { M32R2F_INSN_SRA3, && case_sem_INSN_SRA3 },
    { M32R2F_INSN_SRAI, && case_sem_INSN_SRAI },
    { M32R2F_INSN_SRL, && case_sem_INSN_SRL },
    { M32R2F_INSN_SRL3, && case_sem_INSN_SRL3 },
    { M32R2F_INSN_SRLI, && case_sem_INSN_SRLI },
    { M32R2F_INSN_ST, && case_sem_INSN_ST },
    { M32R2F_INSN_ST_D, && case_sem_INSN_ST_D },
    { M32R2F_INSN_STB, && case_sem_INSN_STB },
    { M32R2F_INSN_STB_D, && case_sem_INSN_STB_D },
    { M32R2F_INSN_STH, && case_sem_INSN_STH },
    { M32R2F_INSN_STH_D, && case_sem_INSN_STH_D },
    { M32R2F_INSN_ST_PLUS, && case_sem_INSN_ST_PLUS },
    { M32R2F_INSN_STH_PLUS, && case_sem_INSN_STH_PLUS },
    { M32R2F_INSN_STB_PLUS, && case_sem_INSN_STB_PLUS },
    { M32R2F_INSN_ST_MINUS, && case_sem_INSN_ST_MINUS },
    { M32R2F_INSN_SUB, && case_sem_INSN_SUB },
    { M32R2F_INSN_SUBV, && case_sem_INSN_SUBV },
    { M32R2F_INSN_SUBX, && case_sem_INSN_SUBX },
    { M32R2F_INSN_TRAP, && case_sem_INSN_TRAP },
    { M32R2F_INSN_UNLOCK, && case_sem_INSN_UNLOCK },
    { M32R2F_INSN_SATB, && case_sem_INSN_SATB },
    { M32R2F_INSN_SATH, && case_sem_INSN_SATH },
    { M32R2F_INSN_SAT, && case_sem_INSN_SAT },
    { M32R2F_INSN_PCMPBZ, && case_sem_INSN_PCMPBZ },
    { M32R2F_INSN_SADD, && case_sem_INSN_SADD },
    { M32R2F_INSN_MACWU1, && case_sem_INSN_MACWU1 },
    { M32R2F_INSN_MSBLO, && case_sem_INSN_MSBLO },
    { M32R2F_INSN_MULWU1, && case_sem_INSN_MULWU1 },
    { M32R2F_INSN_MACLH1, && case_sem_INSN_MACLH1 },
    { M32R2F_INSN_SC, && case_sem_INSN_SC },
    { M32R2F_INSN_SNC, && case_sem_INSN_SNC },
    { M32R2F_INSN_CLRPSW, && case_sem_INSN_CLRPSW },
    { M32R2F_INSN_SETPSW, && case_sem_INSN_SETPSW },
    { M32R2F_INSN_BSET, && case_sem_INSN_BSET },
    { M32R2F_INSN_BCLR, && case_sem_INSN_BCLR },
    { M32R2F_INSN_BTST, && case_sem_INSN_BTST },
    { M32R2F_INSN_PAR_ADD, && case_sem_INSN_PAR_ADD },
    { M32R2F_INSN_WRITE_ADD, && case_sem_INSN_WRITE_ADD },
    { M32R2F_INSN_PAR_AND, && case_sem_INSN_PAR_AND },
    { M32R2F_INSN_WRITE_AND, && case_sem_INSN_WRITE_AND },
    { M32R2F_INSN_PAR_OR, && case_sem_INSN_PAR_OR },
    { M32R2F_INSN_WRITE_OR, && case_sem_INSN_WRITE_OR },
    { M32R2F_INSN_PAR_XOR, && case_sem_INSN_PAR_XOR },
    { M32R2F_INSN_WRITE_XOR, && case_sem_INSN_WRITE_XOR },
    { M32R2F_INSN_PAR_ADDI, && case_sem_INSN_PAR_ADDI },
    { M32R2F_INSN_WRITE_ADDI, && case_sem_INSN_WRITE_ADDI },
    { M32R2F_INSN_PAR_ADDV, && case_sem_INSN_PAR_ADDV },
    { M32R2F_INSN_WRITE_ADDV, && case_sem_INSN_WRITE_ADDV },
    { M32R2F_INSN_PAR_ADDX, && case_sem_INSN_PAR_ADDX },
    { M32R2F_INSN_WRITE_ADDX, && case_sem_INSN_WRITE_ADDX },
    { M32R2F_INSN_PAR_BC8, && case_sem_INSN_PAR_BC8 },
    { M32R2F_INSN_WRITE_BC8, && case_sem_INSN_WRITE_BC8 },
    { M32R2F_INSN_PAR_BL8, && case_sem_INSN_PAR_BL8 },
    { M32R2F_INSN_WRITE_BL8, && case_sem_INSN_WRITE_BL8 },
    { M32R2F_INSN_PAR_BCL8, && case_sem_INSN_PAR_BCL8 },
    { M32R2F_INSN_WRITE_BCL8, && case_sem_INSN_WRITE_BCL8 },
    { M32R2F_INSN_PAR_BNC8, && case_sem_INSN_PAR_BNC8 },
    { M32R2F_INSN_WRITE_BNC8, && case_sem_INSN_WRITE_BNC8 },
    { M32R2F_INSN_PAR_BRA8, && case_sem_INSN_PAR_BRA8 },
    { M32R2F_INSN_WRITE_BRA8, && case_sem_INSN_WRITE_BRA8 },
    { M32R2F_INSN_PAR_BNCL8, && case_sem_INSN_PAR_BNCL8 },
    { M32R2F_INSN_WRITE_BNCL8, && case_sem_INSN_WRITE_BNCL8 },
    { M32R2F_INSN_PAR_CMP, && case_sem_INSN_PAR_CMP },
    { M32R2F_INSN_WRITE_CMP, && case_sem_INSN_WRITE_CMP },
    { M32R2F_INSN_PAR_CMPU, && case_sem_INSN_PAR_CMPU },
    { M32R2F_INSN_WRITE_CMPU, && case_sem_INSN_WRITE_CMPU },
    { M32R2F_INSN_PAR_CMPEQ, && case_sem_INSN_PAR_CMPEQ },
    { M32R2F_INSN_WRITE_CMPEQ, && case_sem_INSN_WRITE_CMPEQ },
    { M32R2F_INSN_PAR_CMPZ, && case_sem_INSN_PAR_CMPZ },
    { M32R2F_INSN_WRITE_CMPZ, && case_sem_INSN_WRITE_CMPZ },
    { M32R2F_INSN_PAR_JC, && case_sem_INSN_PAR_JC },
    { M32R2F_INSN_WRITE_JC, && case_sem_INSN_WRITE_JC },
    { M32R2F_INSN_PAR_JNC, && case_sem_INSN_PAR_JNC },
    { M32R2F_INSN_WRITE_JNC, && case_sem_INSN_WRITE_JNC },
    { M32R2F_INSN_PAR_JL, && case_sem_INSN_PAR_JL },
    { M32R2F_INSN_WRITE_JL, && case_sem_INSN_WRITE_JL },
    { M32R2F_INSN_PAR_JMP, && case_sem_INSN_PAR_JMP },
    { M32R2F_INSN_WRITE_JMP, && case_sem_INSN_WRITE_JMP },
    { M32R2F_INSN_PAR_LD, && case_sem_INSN_PAR_LD },
    { M32R2F_INSN_WRITE_LD, && case_sem_INSN_WRITE_LD },
    { M32R2F_INSN_PAR_LDB, && case_sem_INSN_PAR_LDB },
    { M32R2F_INSN_WRITE_LDB, && case_sem_INSN_WRITE_LDB },
    { M32R2F_INSN_PAR_LDH, && case_sem_INSN_PAR_LDH },
    { M32R2F_INSN_WRITE_LDH, && case_sem_INSN_WRITE_LDH },
    { M32R2F_INSN_PAR_LDUB, && case_sem_INSN_PAR_LDUB },
    { M32R2F_INSN_WRITE_LDUB, && case_sem_INSN_WRITE_LDUB },
    { M32R2F_INSN_PAR_LDUH, && case_sem_INSN_PAR_LDUH },
    { M32R2F_INSN_WRITE_LDUH, && case_sem_INSN_WRITE_LDUH },
    { M32R2F_INSN_PAR_LD_PLUS, && case_sem_INSN_PAR_LD_PLUS },
    { M32R2F_INSN_WRITE_LD_PLUS, && case_sem_INSN_WRITE_LD_PLUS },
    { M32R2F_INSN_PAR_LDI8, && case_sem_INSN_PAR_LDI8 },
    { M32R2F_INSN_WRITE_LDI8, && case_sem_INSN_WRITE_LDI8 },
    { M32R2F_INSN_PAR_LOCK, && case_sem_INSN_PAR_LOCK },
    { M32R2F_INSN_WRITE_LOCK, && case_sem_INSN_WRITE_LOCK },
    { M32R2F_INSN_PAR_MACHI_A, && case_sem_INSN_PAR_MACHI_A },
    { M32R2F_INSN_WRITE_MACHI_A, && case_sem_INSN_WRITE_MACHI_A },
    { M32R2F_INSN_PAR_MACLO_A, && case_sem_INSN_PAR_MACLO_A },
    { M32R2F_INSN_WRITE_MACLO_A, && case_sem_INSN_WRITE_MACLO_A },
    { M32R2F_INSN_PAR_MACWHI_A, && case_sem_INSN_PAR_MACWHI_A },
    { M32R2F_INSN_WRITE_MACWHI_A, && case_sem_INSN_WRITE_MACWHI_A },
    { M32R2F_INSN_PAR_MACWLO_A, && case_sem_INSN_PAR_MACWLO_A },
    { M32R2F_INSN_WRITE_MACWLO_A, && case_sem_INSN_WRITE_MACWLO_A },
    { M32R2F_INSN_PAR_MUL, && case_sem_INSN_PAR_MUL },
    { M32R2F_INSN_WRITE_MUL, && case_sem_INSN_WRITE_MUL },
    { M32R2F_INSN_PAR_MULHI_A, && case_sem_INSN_PAR_MULHI_A },
    { M32R2F_INSN_WRITE_MULHI_A, && case_sem_INSN_WRITE_MULHI_A },
    { M32R2F_INSN_PAR_MULLO_A, && case_sem_INSN_PAR_MULLO_A },
    { M32R2F_INSN_WRITE_MULLO_A, && case_sem_INSN_WRITE_MULLO_A },
    { M32R2F_INSN_PAR_MULWHI_A, && case_sem_INSN_PAR_MULWHI_A },
    { M32R2F_INSN_WRITE_MULWHI_A, && case_sem_INSN_WRITE_MULWHI_A },
    { M32R2F_INSN_PAR_MULWLO_A, && case_sem_INSN_PAR_MULWLO_A },
    { M32R2F_INSN_WRITE_MULWLO_A, && case_sem_INSN_WRITE_MULWLO_A },
    { M32R2F_INSN_PAR_MV, && case_sem_INSN_PAR_MV },
    { M32R2F_INSN_WRITE_MV, && case_sem_INSN_WRITE_MV },
    { M32R2F_INSN_PAR_MVFACHI_A, && case_sem_INSN_PAR_MVFACHI_A },
    { M32R2F_INSN_WRITE_MVFACHI_A, && case_sem_INSN_WRITE_MVFACHI_A },
    { M32R2F_INSN_PAR_MVFACLO_A, && case_sem_INSN_PAR_MVFACLO_A },
    { M32R2F_INSN_WRITE_MVFACLO_A, && case_sem_INSN_WRITE_MVFACLO_A },
    { M32R2F_INSN_PAR_MVFACMI_A, && case_sem_INSN_PAR_MVFACMI_A },
    { M32R2F_INSN_WRITE_MVFACMI_A, && case_sem_INSN_WRITE_MVFACMI_A },
    { M32R2F_INSN_PAR_MVFC, && case_sem_INSN_PAR_MVFC },
    { M32R2F_INSN_WRITE_MVFC, && case_sem_INSN_WRITE_MVFC },
    { M32R2F_INSN_PAR_MVTACHI_A, && case_sem_INSN_PAR_MVTACHI_A },
    { M32R2F_INSN_WRITE_MVTACHI_A, && case_sem_INSN_WRITE_MVTACHI_A },
    { M32R2F_INSN_PAR_MVTACLO_A, && case_sem_INSN_PAR_MVTACLO_A },
    { M32R2F_INSN_WRITE_MVTACLO_A, && case_sem_INSN_WRITE_MVTACLO_A },
    { M32R2F_INSN_PAR_MVTC, && case_sem_INSN_PAR_MVTC },
    { M32R2F_INSN_WRITE_MVTC, && case_sem_INSN_WRITE_MVTC },
    { M32R2F_INSN_PAR_NEG, && case_sem_INSN_PAR_NEG },
    { M32R2F_INSN_WRITE_NEG, && case_sem_INSN_WRITE_NEG },
    { M32R2F_INSN_PAR_NOP, && case_sem_INSN_PAR_NOP },
    { M32R2F_INSN_WRITE_NOP, && case_sem_INSN_WRITE_NOP },
    { M32R2F_INSN_PAR_NOT, && case_sem_INSN_PAR_NOT },
    { M32R2F_INSN_WRITE_NOT, && case_sem_INSN_WRITE_NOT },
    { M32R2F_INSN_PAR_RAC_DSI, && case_sem_INSN_PAR_RAC_DSI },
    { M32R2F_INSN_WRITE_RAC_DSI, && case_sem_INSN_WRITE_RAC_DSI },
    { M32R2F_INSN_PAR_RACH_DSI, && case_sem_INSN_PAR_RACH_DSI },
    { M32R2F_INSN_WRITE_RACH_DSI, && case_sem_INSN_WRITE_RACH_DSI },
    { M32R2F_INSN_PAR_RTE, && case_sem_INSN_PAR_RTE },
    { M32R2F_INSN_WRITE_RTE, && case_sem_INSN_WRITE_RTE },
    { M32R2F_INSN_PAR_SLL, && case_sem_INSN_PAR_SLL },
    { M32R2F_INSN_WRITE_SLL, && case_sem_INSN_WRITE_SLL },
    { M32R2F_INSN_PAR_SLLI, && case_sem_INSN_PAR_SLLI },
    { M32R2F_INSN_WRITE_SLLI, && case_sem_INSN_WRITE_SLLI },
    { M32R2F_INSN_PAR_SRA, && case_sem_INSN_PAR_SRA },
    { M32R2F_INSN_WRITE_SRA, && case_sem_INSN_WRITE_SRA },
    { M32R2F_INSN_PAR_SRAI, && case_sem_INSN_PAR_SRAI },
    { M32R2F_INSN_WRITE_SRAI, && case_sem_INSN_WRITE_SRAI },
    { M32R2F_INSN_PAR_SRL, && case_sem_INSN_PAR_SRL },
    { M32R2F_INSN_WRITE_SRL, && case_sem_INSN_WRITE_SRL },
    { M32R2F_INSN_PAR_SRLI, && case_sem_INSN_PAR_SRLI },
    { M32R2F_INSN_WRITE_SRLI, && case_sem_INSN_WRITE_SRLI },
    { M32R2F_INSN_PAR_ST, && case_sem_INSN_PAR_ST },
    { M32R2F_INSN_WRITE_ST, && case_sem_INSN_WRITE_ST },
    { M32R2F_INSN_PAR_STB, && case_sem_INSN_PAR_STB },
    { M32R2F_INSN_WRITE_STB, && case_sem_INSN_WRITE_STB },
    { M32R2F_INSN_PAR_STH, && case_sem_INSN_PAR_STH },
    { M32R2F_INSN_WRITE_STH, && case_sem_INSN_WRITE_STH },
    { M32R2F_INSN_PAR_ST_PLUS, && case_sem_INSN_PAR_ST_PLUS },
    { M32R2F_INSN_WRITE_ST_PLUS, && case_sem_INSN_WRITE_ST_PLUS },
    { M32R2F_INSN_PAR_STH_PLUS, && case_sem_INSN_PAR_STH_PLUS },
    { M32R2F_INSN_WRITE_STH_PLUS, && case_sem_INSN_WRITE_STH_PLUS },
    { M32R2F_INSN_PAR_STB_PLUS, && case_sem_INSN_PAR_STB_PLUS },
    { M32R2F_INSN_WRITE_STB_PLUS, && case_sem_INSN_WRITE_STB_PLUS },
    { M32R2F_INSN_PAR_ST_MINUS, && case_sem_INSN_PAR_ST_MINUS },
    { M32R2F_INSN_WRITE_ST_MINUS, && case_sem_INSN_WRITE_ST_MINUS },
    { M32R2F_INSN_PAR_SUB, && case_sem_INSN_PAR_SUB },
    { M32R2F_INSN_WRITE_SUB, && case_sem_INSN_WRITE_SUB },
    { M32R2F_INSN_PAR_SUBV, && case_sem_INSN_PAR_SUBV },
    { M32R2F_INSN_WRITE_SUBV, && case_sem_INSN_WRITE_SUBV },
    { M32R2F_INSN_PAR_SUBX, && case_sem_INSN_PAR_SUBX },
    { M32R2F_INSN_WRITE_SUBX, && case_sem_INSN_WRITE_SUBX },
    { M32R2F_INSN_PAR_TRAP, && case_sem_INSN_PAR_TRAP },
    { M32R2F_INSN_WRITE_TRAP, && case_sem_INSN_WRITE_TRAP },
    { M32R2F_INSN_PAR_UNLOCK, && case_sem_INSN_PAR_UNLOCK },
    { M32R2F_INSN_WRITE_UNLOCK, && case_sem_INSN_WRITE_UNLOCK },
    { M32R2F_INSN_PAR_PCMPBZ, && case_sem_INSN_PAR_PCMPBZ },
    { M32R2F_INSN_WRITE_PCMPBZ, && case_sem_INSN_WRITE_PCMPBZ },
    { M32R2F_INSN_PAR_SADD, && case_sem_INSN_PAR_SADD },
    { M32R2F_INSN_WRITE_SADD, && case_sem_INSN_WRITE_SADD },
    { M32R2F_INSN_PAR_MACWU1, && case_sem_INSN_PAR_MACWU1 },
    { M32R2F_INSN_WRITE_MACWU1, && case_sem_INSN_WRITE_MACWU1 },
    { M32R2F_INSN_PAR_MSBLO, && case_sem_INSN_PAR_MSBLO },
    { M32R2F_INSN_WRITE_MSBLO, && case_sem_INSN_WRITE_MSBLO },
    { M32R2F_INSN_PAR_MULWU1, && case_sem_INSN_PAR_MULWU1 },
    { M32R2F_INSN_WRITE_MULWU1, && case_sem_INSN_WRITE_MULWU1 },
    { M32R2F_INSN_PAR_MACLH1, && case_sem_INSN_PAR_MACLH1 },
    { M32R2F_INSN_WRITE_MACLH1, && case_sem_INSN_WRITE_MACLH1 },
    { M32R2F_INSN_PAR_SC, && case_sem_INSN_PAR_SC },
    { M32R2F_INSN_WRITE_SC, && case_sem_INSN_WRITE_SC },
    { M32R2F_INSN_PAR_SNC, && case_sem_INSN_PAR_SNC },
    { M32R2F_INSN_WRITE_SNC, && case_sem_INSN_WRITE_SNC },
    { M32R2F_INSN_PAR_CLRPSW, && case_sem_INSN_PAR_CLRPSW },
    { M32R2F_INSN_WRITE_CLRPSW, && case_sem_INSN_WRITE_CLRPSW },
    { M32R2F_INSN_PAR_SETPSW, && case_sem_INSN_PAR_SETPSW },
    { M32R2F_INSN_WRITE_SETPSW, && case_sem_INSN_WRITE_SETPSW },
    { M32R2F_INSN_PAR_BTST, && case_sem_INSN_PAR_BTST },
    { M32R2F_INSN_WRITE_BTST, && case_sem_INSN_WRITE_BTST },
    { 0, 0 }
  };
  int i;

  for (i = 0; labels[i].label != 0; ++i)
    {
#if FAST_P
      CPU_IDESC (current_cpu) [labels[i].index].sem_fast_lab = labels[i].label;
#else
      CPU_IDESC (current_cpu) [labels[i].index].sem_full_lab = labels[i].label;
#endif
    }

#undef DEFINE_LABELS
#endif /* DEFINE_LABELS */

#ifdef DEFINE_SWITCH

/* If hyper-fast [well not unnecessarily slow] execution is selected, turn
   off frills like tracing and profiling.  */
/* FIXME: A better way would be to have TRACE_RESULT check for something
   that can cause it to be optimized out.  Another way would be to emit
   special handlers into the instruction "stream".  */

#if FAST_P
#undef TRACE_RESULT
#define TRACE_RESULT(cpu, abuf, name, type, val)
#endif

#undef GET_ATTR
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define GET_ATTR(cpu, num, attr) CGEN_ATTR_VALUE (NULL, abuf->idesc->attrs, CGEN_INSN_##attr)
#else
#define GET_ATTR(cpu, num, attr) CGEN_ATTR_VALUE (NULL, abuf->idesc->attrs, CGEN_INSN_/**/attr)
#endif

{

#if WITH_SCACHE_PBB

/* Branch to next handler without going around main loop.  */
#define NEXT(vpc) goto * SEM_ARGBUF (vpc) -> semantic.sem_case
SWITCH (sem, SEM_ARGBUF (vpc) -> semantic.sem_case)

#else /* ! WITH_SCACHE_PBB */

#define NEXT(vpc) BREAK (sem)
#ifdef __GNUC__
#if FAST_P
  SWITCH (sem, SEM_ARGBUF (sc) -> idesc->sem_fast_lab)
#else
  SWITCH (sem, SEM_ARGBUF (sc) -> idesc->sem_full_lab)
#endif
#else
  SWITCH (sem, SEM_ARGBUF (sc) -> idesc->num)
#endif

#endif /* ! WITH_SCACHE_PBB */

    {

  CASE (sem, INSN_X_INVALID) : /* --invalid-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
    /* Update the recorded pc in the cpu state struct.
       Only necessary for WITH_SCACHE case, but to avoid the
       conditional compilation ....  */
    SET_H_PC (pc);
    /* Virtual insns have zero size.  Overwrite vpc with address of next insn
       using the default-insn-bitsize spec.  When executing insns in parallel
       we may want to queue the fault and continue execution.  */
    vpc = SEM_NEXT_VPC (sem_arg, pc, 4);
    vpc = sim_engine_invalid_insn (current_cpu, pc, vpc);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_X_AFTER) : /* --after-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32R2F
    m32r2f_pbb_after (current_cpu, sem_arg);
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_X_BEFORE) : /* --before-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32R2F
    m32r2f_pbb_before (current_cpu, sem_arg);
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_X_CTI_CHAIN) : /* --cti-chain-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32R2F
#ifdef DEFINE_SWITCH
    vpc = m32r2f_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = m32r2f_pbb_cti_chain (current_cpu, sem_arg,
			       CPU_PBB_BR_TYPE (current_cpu),
			       CPU_PBB_BR_NPC (current_cpu));
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_X_CHAIN) : /* --chain-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32R2F
    vpc = m32r2f_pbb_chain (current_cpu, sem_arg);
#ifdef DEFINE_SWITCH
    BREAK (sem);
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_X_BEGIN) : /* --begin-- */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32R2F
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = m32r2f_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = m32r2f_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = m32r2f_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD) : /* add $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD3) : /* add3 $dr,$sr,$hash$slo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (* FLD (i_sr), FLD (f_simm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND) : /* and $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND3) : /* and3 $dr,$sr,$uimm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_and3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR) : /* or $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR3) : /* or3 $dr,$sr,$hash$ulo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_and3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR) : /* xor $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = XORSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR3) : /* xor3 $dr,$sr,$uimm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_and3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI) : /* addi $dr,$simm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), FLD (f_simm8));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDV) : /* addv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = ADDSI (* FLD (i_dr), * FLD (i_sr));
  temp1 = ADDOFSI (* FLD (i_dr), * FLD (i_sr), 0);
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDV3) : /* addv3 $dr,$sr,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI temp0;BI temp1;
  temp0 = ADDSI (* FLD (i_sr), FLD (f_simm16));
  temp1 = ADDOFSI (* FLD (i_sr), FLD (f_simm16), 0);
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDX) : /* addx $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = ADDCSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  temp1 = ADDCFSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC8) : /* bc.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC24) : /* bc.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (CPU (h_cond)) {
  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQ) : /* beq $src1,$src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (* FLD (i_src1), * FLD (i_src2))) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQZ) : /* beqz $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGEZ) : /* bgez $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GESI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGTZ) : /* bgtz $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GTSI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLEZ) : /* blez $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LESI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLTZ) : /* bltz $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNEZ) : /* bnez $src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_src2), 0)) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BL8) : /* bl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BL24) : /* bl.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = ADDSI (pc, 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCL8) : /* bcl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCL24) : /* bcl.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (CPU (h_cond)) {
{
  {
    SI opval = ADDSI (pc, 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNC8) : /* bnc.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNC24) : /* bnc.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (CPU (h_cond))) {
  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNE) : /* bne $src1,$src2,$disp16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_src1), * FLD (i_src2))) {
  {
    USI opval = FLD (i_disp16);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRA8) : /* bra.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRA24) : /* bra.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNCL8) : /* bncl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNCL24) : /* bncl.l $disp24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (CPU (h_cond))) {
{
  {
    SI opval = ADDSI (pc, 4);
    CPU (h_gr[((UINT) 14)]) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP) : /* cmp $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTSI (* FLD (i_src1), * FLD (i_src2));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPI) : /* cmpi $src2,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    BI opval = LTSI (* FLD (i_src2), FLD (f_simm16));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPU) : /* cmpu $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTUSI (* FLD (i_src1), * FLD (i_src2));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPUI) : /* cmpui $src2,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    BI opval = LTUSI (* FLD (i_src2), FLD (f_simm16));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPEQ) : /* cmpeq $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (* FLD (i_src1), * FLD (i_src2));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPZ) : /* cmpz $src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (* FLD (i_src2), 0);
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIV) : /* div $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = DIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVU) : /* divu $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UDIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REM) : /* rem $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = MODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REMU) : /* remu $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UMODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REMH) : /* remh $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = MODSI (EXTHISI (TRUNCSIHI (* FLD (i_dr))), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REMUH) : /* remuh $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UMODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REMB) : /* remb $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = MODSI (EXTBISI (TRUNCSIBI (* FLD (i_dr))), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_REMUB) : /* remub $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UMODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVUH) : /* divuh $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UDIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVB) : /* divb $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = DIVSI (EXTBISI (TRUNCSIBI (* FLD (i_dr))), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVUB) : /* divub $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UDIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVH) : /* divh $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = DIVSI (EXTHISI (TRUNCSIHI (* FLD (i_dr))), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JC) : /* jc $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JNC) : /* jnc $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JL) : /* jl $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;USI temp1;
  temp0 = ADDSI (ANDSI (pc, -4), 4);
  temp1 = ANDSI (* FLD (i_sr), -4);
  {
    SI opval = temp0;
    CPU (h_gr[((UINT) 14)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = temp1;
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JMP) : /* jmp $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LD) : /* ld $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LD_D) : /* ld $dr,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDB) : /* ldb $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDB_D) : /* ldb $dr,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDH) : /* ldh $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDH_D) : /* ldh $dr,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUB) : /* ldub $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUB_D) : /* ldub $dr,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUH) : /* lduh $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUH_D) : /* lduh $dr,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LD_PLUS) : /* ld $dr,@$sr+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;SI temp1;
  temp0 = GETMEMSI (current_cpu, pc, * FLD (i_sr));
  temp1 = ADDSI (* FLD (i_sr), 4);
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = temp1;
    * FLD (i_sr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LD24) : /* ld24 $dr,$uimm24 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld24.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (i_uimm24);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDI8) : /* ldi8 $dr,$simm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_simm8);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDI16) : /* ldi16 $dr,$hash$slo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (f_simm16);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LOCK) : /* lock $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    BI opval = 1;
    CPU (h_lock) = opval;
    TRACE_RESULT (current_cpu, abuf, "lock", 'x', opval);
  }
  {
    SI opval = GETMEMSI (current_cpu, pc, * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACHI_A) : /* machi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACLO_A) : /* maclo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACWHI_A) : /* macwhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))));
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACWLO_A) : /* macwlo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))));
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MUL) : /* mul $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULHI_A) : /* mulhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 16), 16);
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULLO_A) : /* mullo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 16), 16);
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULWHI_A) : /* mulwhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))));
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULWLO_A) : /* mulwlo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))));
    SET_H_ACCUMS (FLD (f_acc), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MV) : /* mv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_sr);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFACHI_A) : /* mvfachi $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUMS (FLD (f_accs)), 32));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFACLO_A) : /* mvfaclo $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (GET_H_ACCUMS (FLD (f_accs)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFACMI_A) : /* mvfacmi $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUMS (FLD (f_accs)), 16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFC) : /* mvfc $dr,$scr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_CR (FLD (f_r2));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVTACHI_A) : /* mvtachi $src1,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUMS (FLD (f_accs)), MAKEDI (0, 0xffffffff)), SLLDI (EXTSIDI (* FLD (i_src1)), 32));
    SET_H_ACCUMS (FLD (f_accs), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVTACLO_A) : /* mvtaclo $src1,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUMS (FLD (f_accs)), MAKEDI (0xffffffff, 0)), ZEXTSIDI (* FLD (i_src1)));
    SET_H_ACCUMS (FLD (f_accs), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVTC) : /* mvtc $sr,$dcr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = * FLD (i_sr);
    SET_H_CR (FLD (f_r1), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEG) : /* neg $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = NEGSI (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NOP) : /* nop */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

PROFILE_COUNT_FILLNOPS (current_cpu, abuf->addr);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NOT) : /* not $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = INVSI (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RAC_DSI) : /* rac $accd,$accs,$imm1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = SLLDI (GET_H_ACCUMS (FLD (f_accs)), FLD (f_imm1));
  tmp_tmp1 = ADDDI (tmp_tmp1, MAKEDI (0, 32768));
  {
    DI opval = (GTDI (tmp_tmp1, MAKEDI (32767, 0xffff0000))) ? (MAKEDI (32767, 0xffff0000)) : (LTDI (tmp_tmp1, MAKEDI (0xffff8000, 0))) ? (MAKEDI (0xffff8000, 0)) : (ANDDI (tmp_tmp1, MAKEDI (0xffffffff, 0xffff0000)));
    SET_H_ACCUMS (FLD (f_accd), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RACH_DSI) : /* rach $accd,$accs,$imm1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = SLLDI (GET_H_ACCUMS (FLD (f_accs)), FLD (f_imm1));
  tmp_tmp1 = ADDDI (tmp_tmp1, MAKEDI (0, 0x80000000));
  {
    DI opval = (GTDI (tmp_tmp1, MAKEDI (32767, 0))) ? (MAKEDI (32767, 0)) : (LTDI (tmp_tmp1, MAKEDI (0xffff8000, 0))) ? (MAKEDI (0xffff8000, 0)) : (ANDDI (tmp_tmp1, MAKEDI (0xffffffff, 0)));
    SET_H_ACCUMS (FLD (f_accd), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RTE) : /* rte */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    USI opval = ANDSI (GET_H_CR (((UINT) 6)), -4);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
  {
    USI opval = GET_H_CR (((UINT) 14));
    SET_H_CR (((UINT) 6), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    UQI opval = CPU (h_bpsw);
    SET_H_PSW (opval);
    TRACE_RESULT (current_cpu, abuf, "psw", 'x', opval);
  }
  {
    UQI opval = CPU (h_bbpsw);
    CPU (h_bpsw) = opval;
    TRACE_RESULT (current_cpu, abuf, "bpsw", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SETH) : /* seth $dr,$hash$hi16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_seth.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (FLD (f_hi16), 16);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLL) : /* sll $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLL3) : /* sll3 $dr,$sr,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLLI) : /* slli $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRA) : /* sra $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRA3) : /* sra3 $dr,$sr,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRAI) : /* srai $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRL) : /* srl $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRL3) : /* srl3 $dr,$sr,$simm16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add3.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRLI) : /* srli $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ST) : /* st $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ST_D) : /* st $src1,@($slo16,$src2) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STB) : /* stb $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    QI opval = * FLD (i_src1);
    SETMEMQI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STB_D) : /* stb $src1,@($slo16,$src2) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = * FLD (i_src1);
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STH) : /* sth $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = * FLD (i_src1);
    SETMEMHI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STH_D) : /* sth $src1,@($slo16,$src2) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = * FLD (i_src1);
    SETMEMHI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ST_PLUS) : /* st $src1,@+$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_new_src2;
  tmp_new_src2 = ADDSI (* FLD (i_src2), 4);
  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, tmp_new_src2, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_new_src2;
    * FLD (i_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STH_PLUS) : /* sth $src1,@$src2+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_new_src2;
  {
    HI opval = * FLD (i_src1);
    SETMEMHI (current_cpu, pc, tmp_new_src2, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_new_src2 = ADDSI (* FLD (i_src2), 2);
  {
    SI opval = tmp_new_src2;
    * FLD (i_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STB_PLUS) : /* stb $src1,@$src2+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_new_src2;
  {
    QI opval = * FLD (i_src1);
    SETMEMQI (current_cpu, pc, tmp_new_src2, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_new_src2 = ADDSI (* FLD (i_src2), 1);
  {
    SI opval = tmp_new_src2;
    * FLD (i_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ST_MINUS) : /* st $src1,@-$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_new_src2;
  tmp_new_src2 = SUBSI (* FLD (i_src2), 4);
  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, tmp_new_src2, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_new_src2;
    * FLD (i_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB) : /* sub $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBV) : /* subv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = SUBSI (* FLD (i_dr), * FLD (i_sr));
  temp1 = SUBOFSI (* FLD (i_dr), * FLD (i_sr), 0);
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBX) : /* subx $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = SUBCSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  temp1 = SUBCFSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  {
    SI opval = temp0;
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TRAP) : /* trap $uimm4 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_trap.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    USI opval = GET_H_CR (((UINT) 6));
    SET_H_CR (((UINT) 14), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    USI opval = ADDSI (pc, 4);
    SET_H_CR (((UINT) 6), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    UQI opval = CPU (h_bpsw);
    CPU (h_bbpsw) = opval;
    TRACE_RESULT (current_cpu, abuf, "bbpsw", 'x', opval);
  }
  {
    UQI opval = GET_H_PSW ();
    CPU (h_bpsw) = opval;
    TRACE_RESULT (current_cpu, abuf, "bpsw", 'x', opval);
  }
  {
    UQI opval = ANDQI (GET_H_PSW (), 128);
    SET_H_PSW (opval);
    TRACE_RESULT (current_cpu, abuf, "psw", 'x', opval);
  }
  {
    SI opval = m32r_trap (current_cpu, pc, FLD (f_uimm4));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_UNLOCK) : /* unlock $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
if (CPU (h_lock)) {
  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, * FLD (i_src2), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
  {
    BI opval = 0;
    CPU (h_lock) = opval;
    TRACE_RESULT (current_cpu, abuf, "lock", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SATB) : /* satb $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = (GESI (* FLD (i_sr), 127)) ? (127) : (LESI (* FLD (i_sr), -128)) ? (-128) : (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SATH) : /* sath $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = (GESI (* FLD (i_sr), 32767)) ? (32767) : (LESI (* FLD (i_sr), -32768)) ? (-32768) : (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SAT) : /* sat $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ((CPU (h_cond)) ? (((LTSI (* FLD (i_sr), 0)) ? (2147483647) : (0x80000000))) : (* FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PCMPBZ) : /* pcmpbz $src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = (EQSI (ANDSI (* FLD (i_src2), 255), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 65280), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 16711680), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 0xff000000), 0)) ? (1) : (0);
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SADD) : /* sadd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (SRADI (GET_H_ACCUMS (((UINT) 1)), 16), GET_H_ACCUMS (((UINT) 0)));
    SET_H_ACCUMS (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACWU1) : /* macwu1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (((UINT) 1)), MULDI (EXTSIDI (* FLD (i_src1)), EXTSIDI (ANDSI (* FLD (i_src2), 65535)))), 8), 8);
    SET_H_ACCUMS (((UINT) 1), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSBLO) : /* msblo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (SUBDI (GET_H_ACCUM (), SRADI (SLLDI (MULDI (EXTHIDI (TRUNCSIHI (* FLD (i_src1))), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 32), 16)), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULWU1) : /* mulwu1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTSIDI (ANDSI (* FLD (i_src2), 65535))), 16), 16);
    SET_H_ACCUMS (((UINT) 1), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACLH1) : /* maclh1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (((UINT) 1)), SLLDI (EXTSIDI (MULSI (EXTHISI (TRUNCSIHI (* FLD (i_src1))), SRASI (* FLD (i_src2), 16))), 16)), 8), 8);
    SET_H_ACCUMS (((UINT) 1), opval);
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SC) : /* sc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (ZEXTBISI (CPU (h_cond)))
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SNC) : /* snc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (ZEXTBISI (NOTBI (CPU (h_cond))))
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CLRPSW) : /* clrpsw $uimm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (GET_H_CR (((UINT) 0)), ORSI (INVBI (FLD (f_uimm8)), 65280));
    SET_H_CR (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SETPSW) : /* setpsw $uimm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_uimm8);
    SET_H_CR (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BSET) : /* bset $uimm3,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bset.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ORQI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))), SLLSI (1, SUBSI (7, FLD (f_uimm3))));
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCLR) : /* bclr $uimm3,@($slo16,$sr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bset.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ANDQI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))), INVQI (SLLSI (1, SUBSI (7, FLD (f_uimm3)))));
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BTST) : /* btst $uimm3,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bset.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ANDQI (SRLSI (* FLD (i_sr), SUBSI (7, FLD (f_uimm3))), 1);
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PAR_ADD) : /* add $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ADD) : /* add $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_AND) : /* and $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_AND) : /* and $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_OR) : /* or $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_OR) : /* or $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_XOR) : /* xor $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = XORSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_XOR) : /* xor $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ADDI) : /* addi $dr,$simm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
#define OPRND(f) par_exec->operands.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), FLD (f_simm8));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ADDI) : /* addi $dr,$simm8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_addi.f
#define OPRND(f) par_exec->operands.sfmt_addi.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ADDV) : /* addv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addv.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = ADDSI (* FLD (i_dr), * FLD (i_sr));
  temp1 = ADDOFSI (* FLD (i_dr), * FLD (i_sr), 0);
  {
    SI opval = temp0;
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ADDV) : /* addv $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addv.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);
  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ADDX) : /* addx $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addx.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = ADDCSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  temp1 = ADDCFSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  {
    SI opval = temp0;
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ADDX) : /* addx $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addx.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);
  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BC8) : /* bc.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bc8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BC8) : /* bc.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bc8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 2))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BL8) : /* bl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    OPRND (h_gr_SI_14) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BL8) : /* bl.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bl8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_gr[((UINT) 14)]) = OPRND (h_gr_SI_14);
  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BCL8) : /* bcl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bcl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    OPRND (h_gr_SI_14) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BCL8) : /* bcl.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bcl8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 3))
    {
      CPU (h_gr[((UINT) 14)]) = OPRND (h_gr_SI_14);
    }
  if (written & (1 << 4))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BNC8) : /* bnc.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bc8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BNC8) : /* bnc.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bc8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 2))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BRA8) : /* bra.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bra8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BRA8) : /* bra.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bra8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BNCL8) : /* bncl.s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bcl8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
{
  {
    SI opval = ADDSI (ANDSI (pc, -4), 4);
    OPRND (h_gr_SI_14) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_disp8);
    OPRND (pc) = opval;
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BNCL8) : /* bncl.s $disp8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bl8.f
#define OPRND(f) par_exec->operands.sfmt_bcl8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 3))
    {
      CPU (h_gr[((UINT) 14)]) = OPRND (h_gr_SI_14);
    }
  if (written & (1 << 4))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_CMP) : /* cmp $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTSI (* FLD (i_src1), * FLD (i_src2));
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_CMP) : /* cmp $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_CMPU) : /* cmpu $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTUSI (* FLD (i_src1), * FLD (i_src2));
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_CMPU) : /* cmpu $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_CMPEQ) : /* cmpeq $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (* FLD (i_src1), * FLD (i_src2));
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_CMPEQ) : /* cmpeq $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmp.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_CMPZ) : /* cmpz $src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmpz.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (* FLD (i_src2), 0);
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_CMPZ) : /* cmpz $src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmpz.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_JC) : /* jc $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (CPU (h_cond)) {
  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    OPRND (pc) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_JC) : /* jc $sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 2))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_JNC) : /* jnc $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (CPU (h_cond))) {
  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    OPRND (pc) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_JNC) : /* jnc $sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  if (written & (1 << 2))
    {
      SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);
    }

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_JL) : /* jl $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jl.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;USI temp1;
  temp0 = ADDSI (ANDSI (pc, -4), 4);
  temp1 = ANDSI (* FLD (i_sr), -4);
  {
    SI opval = temp0;
    OPRND (h_gr_SI_14) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = temp1;
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_JL) : /* jl $sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jl.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_gr[((UINT) 14)]) = OPRND (h_gr_SI_14);
  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_JMP) : /* jmp $sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jmp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_JMP) : /* jmp $sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_jl.f
#define OPRND(f) par_exec->operands.sfmt_jmp.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LD) : /* ld $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ld.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LD) : /* ld $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ld.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LDB) : /* ldb $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LDB) : /* ldb $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldb.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LDH) : /* ldh $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldh.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LDH) : /* ldh $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldh.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LDUB) : /* ldub $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LDUB) : /* ldub $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldb.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LDUH) : /* lduh $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldh.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LDUH) : /* lduh $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ldh.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LD_PLUS) : /* ld $dr,@$sr+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ld_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;SI temp1;
  temp0 = GETMEMSI (current_cpu, pc, * FLD (i_sr));
  temp1 = ADDSI (* FLD (i_sr), 4);
  {
    SI opval = temp0;
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = temp1;
    OPRND (sr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LD_PLUS) : /* ld $dr,@$sr+ */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_ld_plus.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);
  * FLD (i_sr) = OPRND (sr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LDI8) : /* ldi8 $dr,$simm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
#define OPRND(f) par_exec->operands.sfmt_ldi8.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_simm8);
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LDI8) : /* ldi8 $dr,$simm8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_addi.f
#define OPRND(f) par_exec->operands.sfmt_ldi8.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_LOCK) : /* lock $dr,@$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_lock.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    BI opval = 1;
    OPRND (h_lock_BI) = opval;
    TRACE_RESULT (current_cpu, abuf, "lock", 'x', opval);
  }
  {
    SI opval = GETMEMSI (current_cpu, pc, * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_LOCK) : /* lock $dr,@$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_lock.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);
  CPU (h_lock) = OPRND (h_lock_BI);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACHI_A) : /* machi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACHI_A) : /* machi $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACLO_A) : /* maclo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACLO_A) : /* maclo $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACWHI_A) : /* macwhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))));
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACWHI_A) : /* macwhi $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACWLO_A) : /* macwlo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (GET_H_ACCUMS (FLD (f_acc)), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))));
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACWLO_A) : /* macwlo $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_machi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MUL) : /* mul $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MUL) : /* mul $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MULHI_A) : /* mulhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 16), 16);
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MULHI_A) : /* mulhi $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MULLO_A) : /* mullo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 16), 16);
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MULLO_A) : /* mullo $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MULWHI_A) : /* mulwhi $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))));
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MULWHI_A) : /* mulwhi $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MULWLO_A) : /* mulwlo $src1,$src2,$acc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))));
    OPRND (acc) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MULWLO_A) : /* mulwlo $src1,$src2,$acc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_machi_a.f
#define OPRND(f) par_exec->operands.sfmt_mulhi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_acc), OPRND (acc));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MV) : /* mv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_sr);
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MV) : /* mv $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVFACHI_A) : /* mvfachi $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUMS (FLD (f_accs)), 32));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVFACHI_A) : /* mvfachi $dr,$accs */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVFACLO_A) : /* mvfaclo $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (GET_H_ACCUMS (FLD (f_accs)));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVFACLO_A) : /* mvfaclo $dr,$accs */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVFACMI_A) : /* mvfacmi $dr,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUMS (FLD (f_accs)), 16));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVFACMI_A) : /* mvfacmi $dr,$accs */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvfachi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVFC) : /* mvfc $dr,$scr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mvfc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_CR (FLD (f_r2));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVFC) : /* mvfc $dr,$scr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mvfc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVTACHI_A) : /* mvtachi $src1,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvtachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUMS (FLD (f_accs)), MAKEDI (0, 0xffffffff)), SLLDI (EXTSIDI (* FLD (i_src1)), 32));
    OPRND (accs) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVTACHI_A) : /* mvtachi $src1,$accs */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvtachi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_accs), OPRND (accs));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVTACLO_A) : /* mvtaclo $src1,$accs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvtachi_a.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUMS (FLD (f_accs)), MAKEDI (0xffffffff, 0)), ZEXTSIDI (* FLD (i_src1)));
    OPRND (accs) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVTACLO_A) : /* mvtaclo $src1,$accs */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
#define OPRND(f) par_exec->operands.sfmt_mvtachi_a.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_accs), OPRND (accs));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MVTC) : /* mvtc $sr,$dcr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mvtc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = * FLD (i_sr);
    OPRND (dcr) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MVTC) : /* mvtc $sr,$dcr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mvtc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_CR (FLD (f_r1), OPRND (dcr));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_NEG) : /* neg $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = NEGSI (* FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_NEG) : /* neg $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_NOP) : /* nop */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_nop.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

PROFILE_COUNT_FILLNOPS (current_cpu, abuf->addr);

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_NOP) : /* nop */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_nop.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);


#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_NOT) : /* not $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = INVSI (* FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_NOT) : /* not $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_ld_plus.f
#define OPRND(f) par_exec->operands.sfmt_mv.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_RAC_DSI) : /* rac $accd,$accs,$imm1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
#define OPRND(f) par_exec->operands.sfmt_rac_dsi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = SLLDI (GET_H_ACCUMS (FLD (f_accs)), FLD (f_imm1));
  tmp_tmp1 = ADDDI (tmp_tmp1, MAKEDI (0, 32768));
  {
    DI opval = (GTDI (tmp_tmp1, MAKEDI (32767, 0xffff0000))) ? (MAKEDI (32767, 0xffff0000)) : (LTDI (tmp_tmp1, MAKEDI (0xffff8000, 0))) ? (MAKEDI (0xffff8000, 0)) : (ANDDI (tmp_tmp1, MAKEDI (0xffffffff, 0xffff0000)));
    OPRND (accd) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_RAC_DSI) : /* rac $accd,$accs,$imm1 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
#define OPRND(f) par_exec->operands.sfmt_rac_dsi.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_accd), OPRND (accd));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_RACH_DSI) : /* rach $accd,$accs,$imm1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
#define OPRND(f) par_exec->operands.sfmt_rac_dsi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = SLLDI (GET_H_ACCUMS (FLD (f_accs)), FLD (f_imm1));
  tmp_tmp1 = ADDDI (tmp_tmp1, MAKEDI (0, 0x80000000));
  {
    DI opval = (GTDI (tmp_tmp1, MAKEDI (32767, 0))) ? (MAKEDI (32767, 0)) : (LTDI (tmp_tmp1, MAKEDI (0xffff8000, 0))) ? (MAKEDI (0xffff8000, 0)) : (ANDDI (tmp_tmp1, MAKEDI (0xffffffff, 0)));
    OPRND (accd) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_RACH_DSI) : /* rach $accd,$accs,$imm1 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
#define OPRND(f) par_exec->operands.sfmt_rac_dsi.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (FLD (f_accd), OPRND (accd));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_RTE) : /* rte */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_rte.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    USI opval = ANDSI (GET_H_CR (((UINT) 6)), -4);
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
  {
    USI opval = GET_H_CR (((UINT) 14));
    OPRND (h_cr_USI_6) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    UQI opval = CPU (h_bpsw);
    OPRND (h_psw_UQI) = opval;
    TRACE_RESULT (current_cpu, abuf, "psw", 'x', opval);
  }
  {
    UQI opval = CPU (h_bbpsw);
    OPRND (h_bpsw_UQI) = opval;
    TRACE_RESULT (current_cpu, abuf, "bpsw", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_RTE) : /* rte */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_rte.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_bpsw) = OPRND (h_bpsw_UQI);
  SET_H_CR (((UINT) 6), OPRND (h_cr_USI_6));
  SET_H_PSW (OPRND (h_psw_UQI));
  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SLL) : /* sll $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SLL) : /* sll $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SLLI) : /* slli $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), FLD (f_uimm5));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SLLI) : /* slli $dr,$uimm5 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SRA) : /* sra $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SRA) : /* sra $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SRAI) : /* srai $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), FLD (f_uimm5));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SRAI) : /* srai $dr,$uimm5 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SRL) : /* srl $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SRL) : /* srl $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SRLI) : /* srli $dr,$uimm5 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), FLD (f_uimm5));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SRLI) : /* srli $dr,$uimm5 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_slli.f
#define OPRND(f) par_exec->operands.sfmt_slli.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ST) : /* st $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_src1);
    OPRND (h_memory_SI_src2_idx) = * FLD (i_src2);
    OPRND (h_memory_SI_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ST) : /* st $src1,@$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMSI (current_cpu, pc, OPRND (h_memory_SI_src2_idx), OPRND (h_memory_SI_src2));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_STB) : /* stb $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_stb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    QI opval = * FLD (i_src1);
    OPRND (h_memory_QI_src2_idx) = * FLD (i_src2);
    OPRND (h_memory_QI_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_STB) : /* stb $src1,@$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_stb.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMQI (current_cpu, pc, OPRND (h_memory_QI_src2_idx), OPRND (h_memory_QI_src2));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_STH) : /* sth $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_sth.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = * FLD (i_src1);
    OPRND (h_memory_HI_src2_idx) = * FLD (i_src2);
    OPRND (h_memory_HI_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_STH) : /* sth $src1,@$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_sth.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMHI (current_cpu, pc, OPRND (h_memory_HI_src2_idx), OPRND (h_memory_HI_src2));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ST_PLUS) : /* st $src1,@+$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_new_src2;
  tmp_new_src2 = ADDSI (* FLD (i_src2), 4);
  {
    SI opval = * FLD (i_src1);
    OPRND (h_memory_SI_new_src2_idx) = tmp_new_src2;
    OPRND (h_memory_SI_new_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_new_src2;
    OPRND (src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ST_PLUS) : /* st $src1,@+$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st_plus.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMSI (current_cpu, pc, OPRND (h_memory_SI_new_src2_idx), OPRND (h_memory_SI_new_src2));
  * FLD (i_src2) = OPRND (src2);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_STH_PLUS) : /* sth $src1,@$src2+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_sth_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_new_src2;
  {
    HI opval = * FLD (i_src1);
    OPRND (h_memory_HI_new_src2_idx) = tmp_new_src2;
    OPRND (h_memory_HI_new_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_new_src2 = ADDSI (* FLD (i_src2), 2);
  {
    SI opval = tmp_new_src2;
    OPRND (src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_STH_PLUS) : /* sth $src1,@$src2+ */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_sth_plus.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMHI (current_cpu, pc, OPRND (h_memory_HI_new_src2_idx), OPRND (h_memory_HI_new_src2));
  * FLD (i_src2) = OPRND (src2);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_STB_PLUS) : /* stb $src1,@$src2+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_stb_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_new_src2;
  {
    QI opval = * FLD (i_src1);
    OPRND (h_memory_QI_new_src2_idx) = tmp_new_src2;
    OPRND (h_memory_QI_new_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_new_src2 = ADDSI (* FLD (i_src2), 1);
  {
    SI opval = tmp_new_src2;
    OPRND (src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_STB_PLUS) : /* stb $src1,@$src2+ */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_stb_plus.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMQI (current_cpu, pc, OPRND (h_memory_QI_new_src2_idx), OPRND (h_memory_QI_new_src2));
  * FLD (i_src2) = OPRND (src2);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_ST_MINUS) : /* st $src1,@-$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_new_src2;
  tmp_new_src2 = SUBSI (* FLD (i_src2), 4);
  {
    SI opval = * FLD (i_src1);
    OPRND (h_memory_SI_new_src2_idx) = tmp_new_src2;
    OPRND (h_memory_SI_new_src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_new_src2;
    OPRND (src2) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_ST_MINUS) : /* st $src1,@-$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_st_plus.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SETMEMSI (current_cpu, pc, OPRND (h_memory_SI_new_src2_idx), OPRND (h_memory_SI_new_src2));
  * FLD (i_src2) = OPRND (src2);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SUB) : /* sub $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBSI (* FLD (i_dr), * FLD (i_sr));
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SUB) : /* sub $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_add.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SUBV) : /* subv $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addv.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = SUBSI (* FLD (i_dr), * FLD (i_sr));
  temp1 = SUBOFSI (* FLD (i_dr), * FLD (i_sr), 0);
  {
    SI opval = temp0;
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SUBV) : /* subv $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addv.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);
  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SUBX) : /* subx $dr,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addx.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI temp0;BI temp1;
  temp0 = SUBCSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  temp1 = SUBCFSI (* FLD (i_dr), * FLD (i_sr), CPU (h_cond));
  {
    SI opval = temp0;
    OPRND (dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    BI opval = temp1;
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SUBX) : /* subx $dr,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_add.f
#define OPRND(f) par_exec->operands.sfmt_addx.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);
  * FLD (i_dr) = OPRND (dr);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_TRAP) : /* trap $uimm4 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_trap.f
#define OPRND(f) par_exec->operands.sfmt_trap.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    USI opval = GET_H_CR (((UINT) 6));
    OPRND (h_cr_USI_14) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    USI opval = ADDSI (pc, 4);
    OPRND (h_cr_USI_6) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }
  {
    UQI opval = CPU (h_bpsw);
    OPRND (h_bbpsw_UQI) = opval;
    TRACE_RESULT (current_cpu, abuf, "bbpsw", 'x', opval);
  }
  {
    UQI opval = GET_H_PSW ();
    OPRND (h_bpsw_UQI) = opval;
    TRACE_RESULT (current_cpu, abuf, "bpsw", 'x', opval);
  }
  {
    UQI opval = ANDQI (GET_H_PSW (), 128);
    OPRND (h_psw_UQI) = opval;
    TRACE_RESULT (current_cpu, abuf, "psw", 'x', opval);
  }
  {
    SI opval = m32r_trap (current_cpu, pc, FLD (f_uimm4));
    OPRND (pc) = opval;
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_TRAP) : /* trap $uimm4 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_trap.f
#define OPRND(f) par_exec->operands.sfmt_trap.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    SEM_BRANCH_INIT
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_bbpsw) = OPRND (h_bbpsw_UQI);
  CPU (h_bpsw) = OPRND (h_bpsw_UQI);
  SET_H_CR (((UINT) 14), OPRND (h_cr_USI_14));
  SET_H_CR (((UINT) 6), OPRND (h_cr_USI_6));
  SET_H_PSW (OPRND (h_psw_UQI));
  SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, OPRND (pc), vpc);

  SEM_BRANCH_FINI (vpc);
#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_UNLOCK) : /* unlock $src1,@$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_unlock.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
if (CPU (h_lock)) {
  {
    SI opval = * FLD (i_src1);
    OPRND (h_memory_SI_src2_idx) = * FLD (i_src2);
    OPRND (h_memory_SI_src2) = opval;
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
  {
    BI opval = 0;
    OPRND (h_lock_BI) = opval;
    TRACE_RESULT (current_cpu, abuf, "lock", 'x', opval);
  }
}

  abuf->written = written;
#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_UNLOCK) : /* unlock $src1,@$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_unlock.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_lock) = OPRND (h_lock_BI);
  if (written & (1 << 4))
    {
      SETMEMSI (current_cpu, pc, OPRND (h_memory_SI_src2_idx), OPRND (h_memory_SI_src2));
    }

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_PCMPBZ) : /* pcmpbz $src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmpz.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = (EQSI (ANDSI (* FLD (i_src2), 255), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 65280), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 16711680), 0)) ? (1) : (EQSI (ANDSI (* FLD (i_src2), 0xff000000), 0)) ? (1) : (0);
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_PCMPBZ) : /* pcmpbz $src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_cmpz.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SADD) : /* sadd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sadd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ADDDI (SRADI (GET_H_ACCUMS (((UINT) 1)), 16), GET_H_ACCUMS (((UINT) 0)));
    OPRND (h_accums_DI_0) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SADD) : /* sadd */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sadd.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (((UINT) 0), OPRND (h_accums_DI_0));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACWU1) : /* macwu1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_macwu1.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (((UINT) 1)), MULDI (EXTSIDI (* FLD (i_src1)), EXTSIDI (ANDSI (* FLD (i_src2), 65535)))), 8), 8);
    OPRND (h_accums_DI_1) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACWU1) : /* macwu1 $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_macwu1.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (((UINT) 1), OPRND (h_accums_DI_1));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MSBLO) : /* msblo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_msblo.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (SUBDI (GET_H_ACCUM (), SRADI (SLLDI (MULDI (EXTHIDI (TRUNCSIHI (* FLD (i_src1))), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 32), 16)), 8), 8);
    OPRND (accum) = opval;
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MSBLO) : /* msblo $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_msblo.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUM (OPRND (accum));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MULWU1) : /* mulwu1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_mulwu1.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTSIDI (ANDSI (* FLD (i_src2), 65535))), 16), 16);
    OPRND (h_accums_DI_1) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MULWU1) : /* mulwu1 $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_mulwu1.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (((UINT) 1), OPRND (h_accums_DI_1));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_MACLH1) : /* maclh1 $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_macwu1.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUMS (((UINT) 1)), SLLDI (EXTSIDI (MULSI (EXTHISI (TRUNCSIHI (* FLD (i_src1))), SRASI (* FLD (i_src2), 16))), 16)), 8), 8);
    OPRND (h_accums_DI_1) = opval;
    TRACE_RESULT (current_cpu, abuf, "accums", 'D', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_MACLH1) : /* maclh1 $src1,$src2 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_st_plus.f
#define OPRND(f) par_exec->operands.sfmt_macwu1.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_ACCUMS (((UINT) 1), OPRND (h_accums_DI_1));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SC) : /* sc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (ZEXTBISI (CPU (h_cond)))
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SC) : /* sc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);


#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SNC) : /* snc */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sc.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (ZEXTBISI (NOTBI (CPU (h_cond))))
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SNC) : /* snc */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.fmt_empty.f
#define OPRND(f) par_exec->operands.sfmt_sc.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);


#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_CLRPSW) : /* clrpsw $uimm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_clrpsw.f
#define OPRND(f) par_exec->operands.sfmt_clrpsw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (GET_H_CR (((UINT) 0)), ORSI (INVBI (FLD (f_uimm8)), 65280));
    OPRND (h_cr_USI_0) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_CLRPSW) : /* clrpsw $uimm8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_clrpsw.f
#define OPRND(f) par_exec->operands.sfmt_clrpsw.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_CR (((UINT) 0), OPRND (h_cr_USI_0));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_SETPSW) : /* setpsw $uimm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_clrpsw.f
#define OPRND(f) par_exec->operands.sfmt_setpsw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_uimm8);
    OPRND (h_cr_USI_0) = opval;
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_SETPSW) : /* setpsw $uimm8 */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_clrpsw.f
#define OPRND(f) par_exec->operands.sfmt_setpsw.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  SET_H_CR (((UINT) 0), OPRND (h_cr_USI_0));

#undef OPRND
#undef FLD
  }
  NEXT (vpc);

  CASE (sem, INSN_PAR_BTST) : /* btst $uimm3,$sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bset.f
#define OPRND(f) par_exec->operands.sfmt_btst.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ANDQI (SRLSI (* FLD (i_sr), SUBSI (7, FLD (f_uimm3))), 1);
    OPRND (condbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

#undef OPRND
#undef FLD
}
  NEXT (vpc);

CASE (sem, INSN_WRITE_BTST) : /* btst $uimm3,$sr */
  {
    SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
    const ARGBUF *abuf = SEM_ARGBUF (sem_arg)->fields.write.abuf;
#define FLD(f) abuf->fields.sfmt_bset.f
#define OPRND(f) par_exec->operands.sfmt_btst.f
    int UNUSED written = abuf->written;
    IADDR UNUSED pc = abuf->addr;
    vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  CPU (h_cond) = OPRND (condbit);

#undef OPRND
#undef FLD
  }
  NEXT (vpc);


    }
  ENDSWITCH (sem) /* End of semantic switch.  */

  /* At this point `vpc' contains the next insn to execute.  */
}

#undef DEFINE_SWITCH
#endif /* DEFINE_SWITCH */
