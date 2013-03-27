/* CPU family header for crisv10f.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

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

#ifndef CPU_CRISV10F_H
#define CPU_CRISV10F_H

/* Maximum number of instructions that are fetched at a time.
   This is for LIW type instructions sets (e.g. m32r).  */
#define MAX_LIW_INSNS 1

/* Maximum number of instructions that can be executed in parallel.  */
#define MAX_PARALLEL_INSNS 1

/* CPU state information.  */
typedef struct {
  /* Hardware elements.  */
  struct {
  /* program counter */
  USI h_pc;
#define GET_H_PC() CPU (h_pc)
#define SET_H_PC(x) \
do { \
CPU (h_pc) = ANDSI ((x), (~ (1)));\
;} while (0)
  /* General purpose registers */
  SI h_gr_real_pc[16];
#define GET_H_GR_REAL_PC(a1) CPU (h_gr_real_pc)[a1]
#define SET_H_GR_REAL_PC(a1, x) (CPU (h_gr_real_pc)[a1] = (x))
  /* Special registers for v10 */
  SI h_sr_v10[16];
#define GET_H_SR_V10(index) (ORIF (ORIF (((index) == (((UINT) 0))), ((index) == (((UINT) 4)))), ((index) == (((UINT) 8))))) ? (0) : (((index) == (((UINT) 1)))) ? (10) : (ORIF (((index) == (((UINT) 5))), ((index) == (((UINT) 13))))) ? (ORSI (ANDSI (CPU (h_sr_v10[((UINT) 5)]), 0xffffff00), ORSI (ZEXTBISI (CPU (h_cbit)), ORSI (SLLSI (ZEXTBISI (CPU (h_vbit)), 1), ORSI (SLLSI (ZEXTBISI (CPU (h_zbit)), 2), ORSI (SLLSI (ZEXTBISI (CPU (h_nbit)), 3), ORSI (SLLSI (ZEXTBISI (CPU (h_xbit)), 4), ORSI (SLLSI (ZEXTBISI (GET_H_IBIT ()), 5), ORSI (SLLSI (ZEXTBISI (GET_H_UBIT ()), 6), ORSI (SLLSI (ZEXTBISI (CPU (h_pbit)), 7), 0)))))))))) : (CPU (h_sr_v10[index]))
#define SET_H_SR_V10(index, x) \
do { \
if (ORIF (ORIF ((((index)) == (((UINT) 0))), (((index)) == (((UINT) 4)))), ORIF ((((index)) == (((UINT) 8))), (((index)) == (((UINT) 1)))))) {\
((void) 0); /*nop*/\
}\
 else if (ORIF ((((index)) == (((UINT) 5))), (((index)) == (((UINT) 13))))) {\
{\
CPU (h_cbit) = ((NESI (ANDSI ((x), ((1) << (0))), 0)) ? (1) : (0));\
CPU (h_vbit) = ((NESI (ANDSI ((x), ((1) << (1))), 0)) ? (1) : (0));\
CPU (h_zbit) = ((NESI (ANDSI ((x), ((1) << (2))), 0)) ? (1) : (0));\
CPU (h_nbit) = ((NESI (ANDSI ((x), ((1) << (3))), 0)) ? (1) : (0));\
CPU (h_xbit) = ((NESI (ANDSI ((x), ((1) << (4))), 0)) ? (1) : (0));\
SET_H_IBIT (((NESI (ANDSI ((x), ((1) << (5))), 0)) ? (1) : (0)));\
SET_H_UBIT (((NESI (ANDSI ((x), ((1) << (6))), 0)) ? (1) : (0)));\
CPU (h_pbit) = ((NESI (ANDSI ((x), ((1) << (7))), 0)) ? (1) : (0));\
CPU (h_sr_v10[((UINT) 5)]) = (x);\
CPU (h_sr_v10[((UINT) 13)]) = (x);\
}\
}\
 else {\
CPU (h_sr_v10[(index)]) = (x);\
}\
;} while (0)
  /* carry bit */
  BI h_cbit;
#define GET_H_CBIT() CPU (h_cbit)
#define SET_H_CBIT(x) (CPU (h_cbit) = (x))
  /* overflow bit */
  BI h_vbit;
#define GET_H_VBIT() CPU (h_vbit)
#define SET_H_VBIT(x) (CPU (h_vbit) = (x))
  /* zero bit */
  BI h_zbit;
#define GET_H_ZBIT() CPU (h_zbit)
#define SET_H_ZBIT(x) (CPU (h_zbit) = (x))
  /* sign bit */
  BI h_nbit;
#define GET_H_NBIT() CPU (h_nbit)
#define SET_H_NBIT(x) (CPU (h_nbit) = (x))
  /* extended-arithmetic bit */
  BI h_xbit;
#define GET_H_XBIT() CPU (h_xbit)
#define SET_H_XBIT(x) (CPU (h_xbit) = (x))
  /* interrupt-enable bit */
  BI h_ibit_pre_v32;
#define GET_H_IBIT_PRE_V32() CPU (h_ibit_pre_v32)
#define SET_H_IBIT_PRE_V32(x) (CPU (h_ibit_pre_v32) = (x))
  /* sequence-broken bit */
  BI h_pbit;
#define GET_H_PBIT() CPU (h_pbit)
#define SET_H_PBIT(x) (CPU (h_pbit) = (x))
  /* user mode bit */
  BI h_ubit_pre_v32;
#define GET_H_UBIT_PRE_V32() CPU (h_ubit_pre_v32)
#define SET_H_UBIT_PRE_V32(x) (CPU (h_ubit_pre_v32) = (x))
  /* instruction-is-prefixed bit */
  BI h_insn_prefixed_p_pre_v32;
#define GET_H_INSN_PREFIXED_P_PRE_V32() CPU (h_insn_prefixed_p_pre_v32)
#define SET_H_INSN_PREFIXED_P_PRE_V32(x) (CPU (h_insn_prefixed_p_pre_v32) = (x))
  /* Prefix-address register */
  SI h_prefixreg_pre_v32;
#define GET_H_PREFIXREG_PRE_V32() CPU (h_prefixreg_pre_v32)
#define SET_H_PREFIXREG_PRE_V32(x) (CPU (h_prefixreg_pre_v32) = (x))
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} CRISV10F_CPU_DATA;

/* Virtual regs.  */

#define GET_H_V32_NON_V32() 0
#define SET_H_V32_NON_V32(x) \
do { \
cgen_rtx_error (current_cpu, "Can't set h-v32");\
;} while (0)
#define GET_H_GR(index) GET_H_GR_PC (index)
#define SET_H_GR(index, x) \
do { \
SET_H_GR_PC ((index), (x));\
;} while (0)
#define GET_H_GR_PC(index) ((((index) == (15))) ? ((cgen_rtx_error (current_cpu, "General register read of PC is not implemented."), 0)) : (CPU (h_gr_real_pc[index])))
#define SET_H_GR_PC(index, x) \
do { \
{\
if ((((index)) == (15))) {\
cgen_rtx_error (current_cpu, "General register write to PC is not implemented.");\
}\
CPU (h_gr_real_pc[(index)]) = (x);\
}\
;} while (0)
#define GET_H_RAW_GR_PC(index) CPU (h_gr_real_pc[index])
#define SET_H_RAW_GR_PC(index, x) \
do { \
CPU (h_gr_real_pc[(index)]) = (x);\
;} while (0)
#define GET_H_SR(index) GET_H_SR_V10 (index)
#define SET_H_SR(index, x) \
do { \
SET_H_SR_V10 ((index), (x));\
;} while (0)
#define GET_H_CBIT_MOVE() GET_H_CBIT_MOVE_PRE_V32 ()
#define SET_H_CBIT_MOVE(x) \
do { \
SET_H_CBIT_MOVE_PRE_V32 ((x));\
;} while (0)
#define GET_H_CBIT_MOVE_PRE_V32() CPU (h_cbit)
#define SET_H_CBIT_MOVE_PRE_V32(x) \
do { \
CPU (h_cbit) = (x);\
;} while (0)
#define GET_H_VBIT_MOVE() GET_H_VBIT_MOVE_PRE_V32 ()
#define SET_H_VBIT_MOVE(x) \
do { \
SET_H_VBIT_MOVE_PRE_V32 ((x));\
;} while (0)
#define GET_H_VBIT_MOVE_PRE_V32() CPU (h_vbit)
#define SET_H_VBIT_MOVE_PRE_V32(x) \
do { \
CPU (h_vbit) = (x);\
;} while (0)
#define GET_H_ZBIT_MOVE() GET_H_ZBIT_MOVE_PRE_V32 ()
#define SET_H_ZBIT_MOVE(x) \
do { \
SET_H_ZBIT_MOVE_PRE_V32 ((x));\
;} while (0)
#define GET_H_ZBIT_MOVE_PRE_V32() CPU (h_zbit)
#define SET_H_ZBIT_MOVE_PRE_V32(x) \
do { \
CPU (h_zbit) = (x);\
;} while (0)
#define GET_H_NBIT_MOVE() GET_H_NBIT_MOVE_PRE_V32 ()
#define SET_H_NBIT_MOVE(x) \
do { \
SET_H_NBIT_MOVE_PRE_V32 ((x));\
;} while (0)
#define GET_H_NBIT_MOVE_PRE_V32() CPU (h_nbit)
#define SET_H_NBIT_MOVE_PRE_V32(x) \
do { \
CPU (h_nbit) = (x);\
;} while (0)
#define GET_H_IBIT() CPU (h_ibit_pre_v32)
#define SET_H_IBIT(x) \
do { \
CPU (h_ibit_pre_v32) = (x);\
;} while (0)
#define GET_H_UBIT() CPU (h_ubit_pre_v32)
#define SET_H_UBIT(x) \
do { \
CPU (h_ubit_pre_v32) = (x);\
;} while (0)
#define GET_H_INSN_PREFIXED_P() CPU (h_insn_prefixed_p_pre_v32)
#define SET_H_INSN_PREFIXED_P(x) \
do { \
CPU (h_insn_prefixed_p_pre_v32) = (x);\
;} while (0)

/* Cover fns for register access.  */
BI crisv10f_h_v32_non_v32_get (SIM_CPU *);
void crisv10f_h_v32_non_v32_set (SIM_CPU *, BI);
USI crisv10f_h_pc_get (SIM_CPU *);
void crisv10f_h_pc_set (SIM_CPU *, USI);
SI crisv10f_h_gr_get (SIM_CPU *, UINT);
void crisv10f_h_gr_set (SIM_CPU *, UINT, SI);
SI crisv10f_h_gr_pc_get (SIM_CPU *, UINT);
void crisv10f_h_gr_pc_set (SIM_CPU *, UINT, SI);
SI crisv10f_h_gr_real_pc_get (SIM_CPU *, UINT);
void crisv10f_h_gr_real_pc_set (SIM_CPU *, UINT, SI);
SI crisv10f_h_raw_gr_pc_get (SIM_CPU *, UINT);
void crisv10f_h_raw_gr_pc_set (SIM_CPU *, UINT, SI);
SI crisv10f_h_sr_get (SIM_CPU *, UINT);
void crisv10f_h_sr_set (SIM_CPU *, UINT, SI);
SI crisv10f_h_sr_v10_get (SIM_CPU *, UINT);
void crisv10f_h_sr_v10_set (SIM_CPU *, UINT, SI);
BI crisv10f_h_cbit_get (SIM_CPU *);
void crisv10f_h_cbit_set (SIM_CPU *, BI);
BI crisv10f_h_cbit_move_get (SIM_CPU *);
void crisv10f_h_cbit_move_set (SIM_CPU *, BI);
BI crisv10f_h_cbit_move_pre_v32_get (SIM_CPU *);
void crisv10f_h_cbit_move_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_vbit_get (SIM_CPU *);
void crisv10f_h_vbit_set (SIM_CPU *, BI);
BI crisv10f_h_vbit_move_get (SIM_CPU *);
void crisv10f_h_vbit_move_set (SIM_CPU *, BI);
BI crisv10f_h_vbit_move_pre_v32_get (SIM_CPU *);
void crisv10f_h_vbit_move_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_zbit_get (SIM_CPU *);
void crisv10f_h_zbit_set (SIM_CPU *, BI);
BI crisv10f_h_zbit_move_get (SIM_CPU *);
void crisv10f_h_zbit_move_set (SIM_CPU *, BI);
BI crisv10f_h_zbit_move_pre_v32_get (SIM_CPU *);
void crisv10f_h_zbit_move_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_nbit_get (SIM_CPU *);
void crisv10f_h_nbit_set (SIM_CPU *, BI);
BI crisv10f_h_nbit_move_get (SIM_CPU *);
void crisv10f_h_nbit_move_set (SIM_CPU *, BI);
BI crisv10f_h_nbit_move_pre_v32_get (SIM_CPU *);
void crisv10f_h_nbit_move_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_xbit_get (SIM_CPU *);
void crisv10f_h_xbit_set (SIM_CPU *, BI);
BI crisv10f_h_ibit_get (SIM_CPU *);
void crisv10f_h_ibit_set (SIM_CPU *, BI);
BI crisv10f_h_ibit_pre_v32_get (SIM_CPU *);
void crisv10f_h_ibit_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_pbit_get (SIM_CPU *);
void crisv10f_h_pbit_set (SIM_CPU *, BI);
BI crisv10f_h_ubit_get (SIM_CPU *);
void crisv10f_h_ubit_set (SIM_CPU *, BI);
BI crisv10f_h_ubit_pre_v32_get (SIM_CPU *);
void crisv10f_h_ubit_pre_v32_set (SIM_CPU *, BI);
BI crisv10f_h_insn_prefixed_p_get (SIM_CPU *);
void crisv10f_h_insn_prefixed_p_set (SIM_CPU *, BI);
BI crisv10f_h_insn_prefixed_p_pre_v32_get (SIM_CPU *);
void crisv10f_h_insn_prefixed_p_pre_v32_set (SIM_CPU *, BI);
SI crisv10f_h_prefixreg_pre_v32_get (SIM_CPU *);
void crisv10f_h_prefixreg_pre_v32_set (SIM_CPU *, SI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN crisv10f_fetch_register;
extern CPUREG_STORE_FN crisv10f_store_register;

typedef struct {
  int empty;
} MODEL_CRISV10_DATA;

/* Instruction argument buffer.  */

union sem_fields {
  struct { /* no operands */
    int empty;
  } fmt_empty;
  struct { /*  */
    UINT f_u4;
  } sfmt_break;
  struct { /*  */
    UINT f_dstsrc;
  } sfmt_setf;
  struct { /*  */
    IADDR i_o_word_pcrel;
    UINT f_operand2;
  } sfmt_bcc_w;
  struct { /*  */
    IADDR i_o_pcrel;
    UINT f_operand2;
  } sfmt_bcc_b;
  struct { /*  */
    UINT f_memmode;
    unsigned char in_h_gr_SI_14;
    unsigned char out_h_gr_SI_14;
  } sfmt_move_m_spplus_p8;
  struct { /*  */
    INT f_s8;
    UINT f_operand2;
    unsigned char in_Rd;
  } sfmt_addoq;
  struct { /*  */
    INT f_indir_pc__dword;
    UINT f_operand2;
    unsigned char out_Pd;
  } sfmt_move_c_sprv10_p9;
  struct { /*  */
    INT f_indir_pc__word;
    UINT f_operand2;
    unsigned char out_Pd;
  } sfmt_move_c_sprv10_p5;
  struct { /*  */
    INT f_s6;
    UINT f_operand2;
    unsigned char out_Rd;
  } sfmt_moveq;
  struct { /*  */
    INT f_indir_pc__dword;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_Rd;
  } sfmt_bound_cd;
  struct { /*  */
    INT f_indir_pc__word;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_Rd;
  } sfmt_bound_cw;
  struct { /*  */
    INT f_indir_pc__byte;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_Rd;
  } sfmt_bound_cb;
  struct { /*  */
    UINT f_operand2;
    UINT f_u5;
    unsigned char in_Rd;
    unsigned char out_Rd;
  } sfmt_asrq;
  struct { /*  */
    INT f_s6;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_andq;
  struct { /*  */
    INT f_indir_pc__dword;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_addcdr;
  struct { /*  */
    INT f_indir_pc__word;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_addcwr;
  struct { /*  */
    INT f_indir_pc__byte;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_addcbr;
  struct { /*  */
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Ps;
    unsigned char out_h_gr_SI_index_of__DFLT_Rs;
  } sfmt_move_spr_rv10;
  struct { /*  */
    UINT f_operand2;
    UINT f_u6;
    unsigned char in_Rd;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_addq;
  struct { /*  */
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char out_h_gr_SI_index_of__DFLT_Rd;
  } sfmt_add_b_r;
  struct { /*  */
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char out_Rd;
    unsigned char out_h_sr_SI_7;
  } sfmt_muls_b;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Ps;
    unsigned char in_Rs;
    unsigned char out_Rs;
  } sfmt_move_spr_mv10;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rs;
    unsigned char out_Pd;
    unsigned char out_Rs;
  } sfmt_move_m_sprv10;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char out_Rd;
    unsigned char out_Rs;
  } sfmt_bound_m_b_m;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char out_Rs;
    unsigned char out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd;
  } sfmt_add_m_b_m;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char out_Rs;
    unsigned char out_h_gr_SI_0;
    unsigned char out_h_gr_SI_1;
    unsigned char out_h_gr_SI_10;
    unsigned char out_h_gr_SI_11;
    unsigned char out_h_gr_SI_12;
    unsigned char out_h_gr_SI_13;
    unsigned char out_h_gr_SI_14;
    unsigned char out_h_gr_SI_2;
    unsigned char out_h_gr_SI_3;
    unsigned char out_h_gr_SI_4;
    unsigned char out_h_gr_SI_5;
    unsigned char out_h_gr_SI_6;
    unsigned char out_h_gr_SI_7;
    unsigned char out_h_gr_SI_8;
    unsigned char out_h_gr_SI_9;
  } sfmt_movem_m_r;
  struct { /*  */
    UINT f_memmode;
    UINT f_operand1;
    UINT f_operand2;
    unsigned char in_Rd;
    unsigned char in_Rs;
    unsigned char in_h_gr_SI_0;
    unsigned char in_h_gr_SI_1;
    unsigned char in_h_gr_SI_10;
    unsigned char in_h_gr_SI_11;
    unsigned char in_h_gr_SI_12;
    unsigned char in_h_gr_SI_13;
    unsigned char in_h_gr_SI_14;
    unsigned char in_h_gr_SI_15;
    unsigned char in_h_gr_SI_2;
    unsigned char in_h_gr_SI_3;
    unsigned char in_h_gr_SI_4;
    unsigned char in_h_gr_SI_5;
    unsigned char in_h_gr_SI_6;
    unsigned char in_h_gr_SI_7;
    unsigned char in_h_gr_SI_8;
    unsigned char in_h_gr_SI_9;
    unsigned char out_Rs;
  } sfmt_movem_r_m;
#if WITH_SCACHE_PBB
  /* Writeback handler.  */
  struct {
    /* Pointer to argbuf entry for insn whose results need writing back.  */
    const struct argbuf *abuf;
  } write;
  /* x-before handler */
  struct {
    /*const SCACHE *insns[MAX_PARALLEL_INSNS];*/
    int first_p;
  } before;
  /* x-after handler */
  struct {
    int empty;
  } after;
  /* This entry is used to terminate each pbb.  */
  struct {
    /* Number of insns in pbb.  */
    int insn_count;
    /* Next pbb to execute.  */
    SCACHE *next;
    SCACHE *branch_target;
  } chain;
#endif
};

/* The ARGBUF struct.  */
struct argbuf {
  /* These are the baseclass definitions.  */
  IADDR addr;
  const IDESC *idesc;
  char trace_p;
  char profile_p;
  /* ??? Temporary hack for skip insns.  */
  char skip_count;
  char unused;
  /* cpu specific data follows */
  union sem semantic;
  int written;
  union sem_fields fields;
};

/* A cached insn.

   ??? SCACHE used to contain more than just argbuf.  We could delete the
   type entirely and always just use ARGBUF, but for future concerns and as
   a level of abstraction it is left in.  */

struct scache {
  struct argbuf argbuf;
};

/* Macros to simplify extraction, reading and semantic code.
   These define and assign the local vars that contain the insn's fields.  */

#define EXTRACT_IFMT_EMPTY_VARS \
  unsigned int length;
#define EXTRACT_IFMT_EMPTY_CODE \
  length = 0; \

#define EXTRACT_IFMT_NOP_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_NOP_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_B_R_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_B_R_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVEPCR_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVEPCR_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVEQ_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  INT f_s6; \
  unsigned int length;
#define EXTRACT_IFMT_MOVEQ_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_s6 = EXTRACT_LSB0_INT (insn, 16, 5, 6); \

#define EXTRACT_IFMT_MOVECBR_VARS \
  UINT f_operand2; \
  INT f_indir_pc__byte; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVECBR_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVECWR_VARS \
  UINT f_operand2; \
  INT f_indir_pc__word; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVECWR_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVECDR_VARS \
  INT f_indir_pc__dword; \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVECDR_CODE \
  length = 6; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0)); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVUCBR_VARS \
  UINT f_operand2; \
  INT f_indir_pc__byte; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVUCBR_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVUCWR_VARS \
  UINT f_operand2; \
  INT f_indir_pc__word; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVUCWR_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_ADDQ_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_ADDQ_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 16, 5, 6); \

#define EXTRACT_IFMT_CMP_M_B_M_VARS \
  UINT f_operand2; \
  UINT f_membit; \
  UINT f_memmode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_CMP_M_B_M_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_membit = EXTRACT_LSB0_UINT (insn, 16, 11, 1); \
  f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_R_SPRV10_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_R_SPRV10_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_SPR_RV10_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_SPR_RV10_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_RET_TYPE_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_RET_TYPE_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_M_SPRV10_VARS \
  UINT f_operand2; \
  UINT f_membit; \
  UINT f_memmode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_M_SPRV10_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_membit = EXTRACT_LSB0_UINT (insn, 16, 11, 1); \
  f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_C_SPRV10_P5_VARS \
  UINT f_operand2; \
  INT f_indir_pc__word; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_C_SPRV10_P5_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_C_SPRV10_P9_VARS \
  INT f_indir_pc__dword; \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_C_SPRV10_P9_CODE \
  length = 6; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0)); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_MOVE_SPR_MV10_VARS \
  UINT f_operand2; \
  UINT f_membit; \
  UINT f_memmode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_MOVE_SPR_MV10_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_membit = EXTRACT_LSB0_UINT (insn, 16, 11, 1); \
  f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_SBFS_VARS \
  UINT f_operand2; \
  UINT f_membit; \
  UINT f_memmode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_SBFS_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_membit = EXTRACT_LSB0_UINT (insn, 16, 11, 1); \
  f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_SWAP_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_SWAP_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_ASRQ_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_b5; \
  UINT f_u5; \
  unsigned int length;
#define EXTRACT_IFMT_ASRQ_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_b5 = EXTRACT_LSB0_UINT (insn, 16, 5, 1); \
  f_u5 = EXTRACT_LSB0_UINT (insn, 16, 4, 5); \

#define EXTRACT_IFMT_SETF_VARS \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand2; \
  UINT f_operand1; \
  UINT f_dstsrc; \
  unsigned int length;
#define EXTRACT_IFMT_SETF_CODE \
  length = 2; \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \
  f_dstsrc = ((((f_operand1) | (((f_operand2) << (4))))) & (255));\

#define EXTRACT_IFMT_BCC_B_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode_hi; \
  INT f_disp9_hi; \
  UINT f_disp9_lo; \
  INT f_disp9; \
  unsigned int length;
#define EXTRACT_IFMT_BCC_B_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode_hi = EXTRACT_LSB0_UINT (insn, 16, 9, 2); \
  f_disp9_hi = EXTRACT_LSB0_INT (insn, 16, 0, 1); \
  f_disp9_lo = EXTRACT_LSB0_UINT (insn, 16, 7, 7); \
{\
  SI tmp_abslo;\
  SI tmp_absval;\
  tmp_abslo = ((f_disp9_lo) << (1));\
  tmp_absval = ((((((f_disp9_hi) != (0))) ? ((~ (255))) : (0))) | (tmp_abslo));\
  f_disp9 = ((((pc) + (tmp_absval))) + (((GET_H_V32_NON_V32 ()) ? (0) : (2))));\
}\

#define EXTRACT_IFMT_BA_B_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode_hi; \
  INT f_disp9_hi; \
  UINT f_disp9_lo; \
  INT f_disp9; \
  unsigned int length;
#define EXTRACT_IFMT_BA_B_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode_hi = EXTRACT_LSB0_UINT (insn, 16, 9, 2); \
  f_disp9_hi = EXTRACT_LSB0_INT (insn, 16, 0, 1); \
  f_disp9_lo = EXTRACT_LSB0_UINT (insn, 16, 7, 7); \
{\
  SI tmp_abslo;\
  SI tmp_absval;\
  tmp_abslo = ((f_disp9_lo) << (1));\
  tmp_absval = ((((((f_disp9_hi) != (0))) ? ((~ (255))) : (0))) | (tmp_abslo));\
  f_disp9 = ((((pc) + (tmp_absval))) + (((GET_H_V32_NON_V32 ()) ? (0) : (2))));\
}\

#define EXTRACT_IFMT_BCC_W_VARS \
  UINT f_operand2; \
  SI f_indir_pc__word_pcrel; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_BCC_W_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__word_pcrel = ((EXTHISI (((HI) (UINT) ((0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)))))) + (((pc) + (((GET_H_V32_NON_V32 ()) ? (0) : (4)))))); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_BA_W_VARS \
  UINT f_operand2; \
  SI f_indir_pc__word_pcrel; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_BA_W_CODE \
  length = 4; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_indir_pc__word_pcrel = ((EXTHISI (((HI) (UINT) ((0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)))))) + (((pc) + (((GET_H_V32_NON_V32 ()) ? (0) : (4)))))); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_JUMP_C_VARS \
  INT f_indir_pc__dword; \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  /* Contents of trailing part of insn.  */ \
  UINT word_1; \
  unsigned int length;
#define EXTRACT_IFMT_JUMP_C_CODE \
  length = 6; \
  word_1 = GETIMEMUSI (current_cpu, pc + 2); \
  f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0)); \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_BREAK_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_u4; \
  unsigned int length;
#define EXTRACT_IFMT_BREAK_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_u4 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_SCC_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode; \
  UINT f_size; \
  UINT f_operand1; \
  unsigned int length;
#define EXTRACT_IFMT_SCC_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode = EXTRACT_LSB0_UINT (insn, 16, 9, 4); \
  f_size = EXTRACT_LSB0_UINT (insn, 16, 5, 2); \
  f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4); \

#define EXTRACT_IFMT_ADDOQ_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode_hi; \
  INT f_s8; \
  unsigned int length;
#define EXTRACT_IFMT_ADDOQ_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode_hi = EXTRACT_LSB0_UINT (insn, 16, 9, 2); \
  f_s8 = EXTRACT_LSB0_INT (insn, 16, 7, 8); \

#define EXTRACT_IFMT_BDAPQPC_VARS \
  UINT f_operand2; \
  UINT f_mode; \
  UINT f_opcode_hi; \
  INT f_s8; \
  unsigned int length;
#define EXTRACT_IFMT_BDAPQPC_CODE \
  length = 2; \
  f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4); \
  f_mode = EXTRACT_LSB0_UINT (insn, 16, 11, 2); \
  f_opcode_hi = EXTRACT_LSB0_UINT (insn, 16, 9, 2); \
  f_s8 = EXTRACT_LSB0_INT (insn, 16, 7, 8); \

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_CRISV10F_H */
