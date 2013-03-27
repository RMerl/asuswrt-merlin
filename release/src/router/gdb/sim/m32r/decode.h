/* Decode header for m32rbf.

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

#ifndef M32RBF_DECODE_H
#define M32RBF_DECODE_H

extern const IDESC *m32rbf_decode (SIM_CPU *, IADDR,
                                  CGEN_INSN_INT, CGEN_INSN_INT,
                                  ARGBUF *);
extern void m32rbf_init_idesc_table (SIM_CPU *);
extern void m32rbf_sem_init_idesc_table (SIM_CPU *);
extern void m32rbf_semf_init_idesc_table (SIM_CPU *);

/* Enum declaration for instructions in cpu family m32rbf.  */
typedef enum m32rbf_insn_type {
  M32RBF_INSN_X_INVALID, M32RBF_INSN_X_AFTER, M32RBF_INSN_X_BEFORE, M32RBF_INSN_X_CTI_CHAIN
 , M32RBF_INSN_X_CHAIN, M32RBF_INSN_X_BEGIN, M32RBF_INSN_ADD, M32RBF_INSN_ADD3
 , M32RBF_INSN_AND, M32RBF_INSN_AND3, M32RBF_INSN_OR, M32RBF_INSN_OR3
 , M32RBF_INSN_XOR, M32RBF_INSN_XOR3, M32RBF_INSN_ADDI, M32RBF_INSN_ADDV
 , M32RBF_INSN_ADDV3, M32RBF_INSN_ADDX, M32RBF_INSN_BC8, M32RBF_INSN_BC24
 , M32RBF_INSN_BEQ, M32RBF_INSN_BEQZ, M32RBF_INSN_BGEZ, M32RBF_INSN_BGTZ
 , M32RBF_INSN_BLEZ, M32RBF_INSN_BLTZ, M32RBF_INSN_BNEZ, M32RBF_INSN_BL8
 , M32RBF_INSN_BL24, M32RBF_INSN_BNC8, M32RBF_INSN_BNC24, M32RBF_INSN_BNE
 , M32RBF_INSN_BRA8, M32RBF_INSN_BRA24, M32RBF_INSN_CMP, M32RBF_INSN_CMPI
 , M32RBF_INSN_CMPU, M32RBF_INSN_CMPUI, M32RBF_INSN_DIV, M32RBF_INSN_DIVU
 , M32RBF_INSN_REM, M32RBF_INSN_REMU, M32RBF_INSN_JL, M32RBF_INSN_JMP
 , M32RBF_INSN_LD, M32RBF_INSN_LD_D, M32RBF_INSN_LDB, M32RBF_INSN_LDB_D
 , M32RBF_INSN_LDH, M32RBF_INSN_LDH_D, M32RBF_INSN_LDUB, M32RBF_INSN_LDUB_D
 , M32RBF_INSN_LDUH, M32RBF_INSN_LDUH_D, M32RBF_INSN_LD_PLUS, M32RBF_INSN_LD24
 , M32RBF_INSN_LDI8, M32RBF_INSN_LDI16, M32RBF_INSN_LOCK, M32RBF_INSN_MACHI
 , M32RBF_INSN_MACLO, M32RBF_INSN_MACWHI, M32RBF_INSN_MACWLO, M32RBF_INSN_MUL
 , M32RBF_INSN_MULHI, M32RBF_INSN_MULLO, M32RBF_INSN_MULWHI, M32RBF_INSN_MULWLO
 , M32RBF_INSN_MV, M32RBF_INSN_MVFACHI, M32RBF_INSN_MVFACLO, M32RBF_INSN_MVFACMI
 , M32RBF_INSN_MVFC, M32RBF_INSN_MVTACHI, M32RBF_INSN_MVTACLO, M32RBF_INSN_MVTC
 , M32RBF_INSN_NEG, M32RBF_INSN_NOP, M32RBF_INSN_NOT, M32RBF_INSN_RAC
 , M32RBF_INSN_RACH, M32RBF_INSN_RTE, M32RBF_INSN_SETH, M32RBF_INSN_SLL
 , M32RBF_INSN_SLL3, M32RBF_INSN_SLLI, M32RBF_INSN_SRA, M32RBF_INSN_SRA3
 , M32RBF_INSN_SRAI, M32RBF_INSN_SRL, M32RBF_INSN_SRL3, M32RBF_INSN_SRLI
 , M32RBF_INSN_ST, M32RBF_INSN_ST_D, M32RBF_INSN_STB, M32RBF_INSN_STB_D
 , M32RBF_INSN_STH, M32RBF_INSN_STH_D, M32RBF_INSN_ST_PLUS, M32RBF_INSN_ST_MINUS
 , M32RBF_INSN_SUB, M32RBF_INSN_SUBV, M32RBF_INSN_SUBX, M32RBF_INSN_TRAP
 , M32RBF_INSN_UNLOCK, M32RBF_INSN_CLRPSW, M32RBF_INSN_SETPSW, M32RBF_INSN_BSET
 , M32RBF_INSN_BCLR, M32RBF_INSN_BTST, M32RBF_INSN__MAX
} M32RBF_INSN_TYPE;

/* Enum declaration for semantic formats in cpu family m32rbf.  */
typedef enum m32rbf_sfmt_type {
  M32RBF_SFMT_EMPTY, M32RBF_SFMT_ADD, M32RBF_SFMT_ADD3, M32RBF_SFMT_AND3
 , M32RBF_SFMT_OR3, M32RBF_SFMT_ADDI, M32RBF_SFMT_ADDV, M32RBF_SFMT_ADDV3
 , M32RBF_SFMT_ADDX, M32RBF_SFMT_BC8, M32RBF_SFMT_BC24, M32RBF_SFMT_BEQ
 , M32RBF_SFMT_BEQZ, M32RBF_SFMT_BL8, M32RBF_SFMT_BL24, M32RBF_SFMT_BRA8
 , M32RBF_SFMT_BRA24, M32RBF_SFMT_CMP, M32RBF_SFMT_CMPI, M32RBF_SFMT_DIV
 , M32RBF_SFMT_JL, M32RBF_SFMT_JMP, M32RBF_SFMT_LD, M32RBF_SFMT_LD_D
 , M32RBF_SFMT_LDB, M32RBF_SFMT_LDB_D, M32RBF_SFMT_LDH, M32RBF_SFMT_LDH_D
 , M32RBF_SFMT_LD_PLUS, M32RBF_SFMT_LD24, M32RBF_SFMT_LDI8, M32RBF_SFMT_LDI16
 , M32RBF_SFMT_LOCK, M32RBF_SFMT_MACHI, M32RBF_SFMT_MULHI, M32RBF_SFMT_MV
 , M32RBF_SFMT_MVFACHI, M32RBF_SFMT_MVFC, M32RBF_SFMT_MVTACHI, M32RBF_SFMT_MVTC
 , M32RBF_SFMT_NOP, M32RBF_SFMT_RAC, M32RBF_SFMT_RTE, M32RBF_SFMT_SETH
 , M32RBF_SFMT_SLL3, M32RBF_SFMT_SLLI, M32RBF_SFMT_ST, M32RBF_SFMT_ST_D
 , M32RBF_SFMT_STB, M32RBF_SFMT_STB_D, M32RBF_SFMT_STH, M32RBF_SFMT_STH_D
 , M32RBF_SFMT_ST_PLUS, M32RBF_SFMT_TRAP, M32RBF_SFMT_UNLOCK, M32RBF_SFMT_CLRPSW
 , M32RBF_SFMT_SETPSW, M32RBF_SFMT_BSET, M32RBF_SFMT_BTST
} M32RBF_SFMT_TYPE;

/* Function unit handlers (user written).  */

extern int m32rbf_model_m32r_d_u_store (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*src1*/, INT /*src2*/);
extern int m32rbf_model_m32r_d_u_load (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*sr*/, INT /*dr*/);
extern int m32rbf_model_m32r_d_u_cti (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*sr*/);
extern int m32rbf_model_m32r_d_u_mac (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*src1*/, INT /*src2*/);
extern int m32rbf_model_m32r_d_u_cmp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*src1*/, INT /*src2*/);
extern int m32rbf_model_m32r_d_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*sr*/, INT /*dr*/, INT /*dr*/);
extern int m32rbf_model_test_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);

/* Profiling before/after handlers (user written) */

extern void m32rbf_model_insn_before (SIM_CPU *, int /*first_p*/);
extern void m32rbf_model_insn_after (SIM_CPU *, int /*last_p*/, int /*cycles*/);

#endif /* M32RBF_DECODE_H */
