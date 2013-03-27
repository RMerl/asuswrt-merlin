/* CPU family header for m32r2f.

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

#ifndef CPU_M32R2F_H
#define CPU_M32R2F_H

/* Maximum number of instructions that are fetched at a time.
   This is for LIW type instructions sets (e.g. m32r).  */
#define MAX_LIW_INSNS 2

/* Maximum number of instructions that can be executed in parallel.  */
#define MAX_PARALLEL_INSNS 2

/* CPU state information.  */
typedef struct {
  /* Hardware elements.  */
  struct {
  /* program counter */
  USI h_pc;
#define GET_H_PC() CPU (h_pc)
#define SET_H_PC(x) (CPU (h_pc) = (x))
  /* general registers */
  SI h_gr[16];
#define GET_H_GR(a1) CPU (h_gr)[a1]
#define SET_H_GR(a1, x) (CPU (h_gr)[a1] = (x))
  /* control registers */
  USI h_cr[16];
#define GET_H_CR(index) m32r2f_h_cr_get_handler (current_cpu, index)
#define SET_H_CR(index, x) \
do { \
m32r2f_h_cr_set_handler (current_cpu, (index), (x));\
;} while (0)
  /* accumulator */
  DI h_accum;
#define GET_H_ACCUM() m32r2f_h_accum_get_handler (current_cpu)
#define SET_H_ACCUM(x) \
do { \
m32r2f_h_accum_set_handler (current_cpu, (x));\
;} while (0)
  /* accumulators */
  DI h_accums[2];
#define GET_H_ACCUMS(index) m32r2f_h_accums_get_handler (current_cpu, index)
#define SET_H_ACCUMS(index, x) \
do { \
m32r2f_h_accums_set_handler (current_cpu, (index), (x));\
;} while (0)
  /* condition bit */
  BI h_cond;
#define GET_H_COND() CPU (h_cond)
#define SET_H_COND(x) (CPU (h_cond) = (x))
  /* psw part of psw */
  UQI h_psw;
#define GET_H_PSW() m32r2f_h_psw_get_handler (current_cpu)
#define SET_H_PSW(x) \
do { \
m32r2f_h_psw_set_handler (current_cpu, (x));\
;} while (0)
  /* backup psw */
  UQI h_bpsw;
#define GET_H_BPSW() CPU (h_bpsw)
#define SET_H_BPSW(x) (CPU (h_bpsw) = (x))
  /* backup bpsw */
  UQI h_bbpsw;
#define GET_H_BBPSW() CPU (h_bbpsw)
#define SET_H_BBPSW(x) (CPU (h_bbpsw) = (x))
  /* lock */
  BI h_lock;
#define GET_H_LOCK() CPU (h_lock)
#define SET_H_LOCK(x) (CPU (h_lock) = (x))
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} M32R2F_CPU_DATA;

/* Cover fns for register access.  */
USI m32r2f_h_pc_get (SIM_CPU *);
void m32r2f_h_pc_set (SIM_CPU *, USI);
SI m32r2f_h_gr_get (SIM_CPU *, UINT);
void m32r2f_h_gr_set (SIM_CPU *, UINT, SI);
USI m32r2f_h_cr_get (SIM_CPU *, UINT);
void m32r2f_h_cr_set (SIM_CPU *, UINT, USI);
DI m32r2f_h_accum_get (SIM_CPU *);
void m32r2f_h_accum_set (SIM_CPU *, DI);
DI m32r2f_h_accums_get (SIM_CPU *, UINT);
void m32r2f_h_accums_set (SIM_CPU *, UINT, DI);
BI m32r2f_h_cond_get (SIM_CPU *);
void m32r2f_h_cond_set (SIM_CPU *, BI);
UQI m32r2f_h_psw_get (SIM_CPU *);
void m32r2f_h_psw_set (SIM_CPU *, UQI);
UQI m32r2f_h_bpsw_get (SIM_CPU *);
void m32r2f_h_bpsw_set (SIM_CPU *, UQI);
UQI m32r2f_h_bbpsw_get (SIM_CPU *);
void m32r2f_h_bbpsw_set (SIM_CPU *, UQI);
BI m32r2f_h_lock_get (SIM_CPU *);
void m32r2f_h_lock_set (SIM_CPU *, BI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN m32r2f_fetch_register;
extern CPUREG_STORE_FN m32r2f_store_register;

typedef struct {
  int empty;
} MODEL_M32R2_DATA;

/* Instruction argument buffer.  */

union sem_fields {
  struct { /* no operands */
    int empty;
  } fmt_empty;
  struct { /*  */
    UINT f_uimm8;
  } sfmt_clrpsw;
  struct { /*  */
    UINT f_uimm4;
  } sfmt_trap;
  struct { /*  */
    IADDR i_disp24;
    unsigned char out_h_gr_SI_14;
  } sfmt_bl24;
  struct { /*  */
    IADDR i_disp8;
    unsigned char out_h_gr_SI_14;
  } sfmt_bl8;
  struct { /*  */
    SI f_imm1;
    UINT f_accd;
    UINT f_accs;
  } sfmt_rac_dsi;
  struct { /*  */
    SI* i_dr;
    UINT f_hi16;
    UINT f_r1;
    unsigned char out_dr;
  } sfmt_seth;
  struct { /*  */
    SI* i_src1;
    UINT f_accs;
    UINT f_r1;
    unsigned char in_src1;
  } sfmt_mvtachi_a;
  struct { /*  */
    SI* i_dr;
    UINT f_accs;
    UINT f_r1;
    unsigned char out_dr;
  } sfmt_mvfachi_a;
  struct { /*  */
    ADDR i_uimm24;
    SI* i_dr;
    UINT f_r1;
    unsigned char out_dr;
  } sfmt_ld24;
  struct { /*  */
    SI* i_sr;
    UINT f_r2;
    unsigned char in_sr;
    unsigned char out_h_gr_SI_14;
  } sfmt_jl;
  struct { /*  */
    SI* i_sr;
    INT f_simm16;
    UINT f_r2;
    UINT f_uimm3;
    unsigned char in_sr;
  } sfmt_bset;
  struct { /*  */
    SI* i_dr;
    UINT f_r1;
    UINT f_uimm5;
    unsigned char in_dr;
    unsigned char out_dr;
  } sfmt_slli;
  struct { /*  */
    SI* i_dr;
    INT f_simm8;
    UINT f_r1;
    unsigned char in_dr;
    unsigned char out_dr;
  } sfmt_addi;
  struct { /*  */
    SI* i_src1;
    SI* i_src2;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_src1;
    unsigned char in_src2;
    unsigned char out_src2;
  } sfmt_st_plus;
  struct { /*  */
    SI* i_src1;
    SI* i_src2;
    INT f_simm16;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_src1;
    unsigned char in_src2;
  } sfmt_st_d;
  struct { /*  */
    SI* i_src1;
    SI* i_src2;
    UINT f_acc;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_src1;
    unsigned char in_src2;
  } sfmt_machi_a;
  struct { /*  */
    SI* i_dr;
    SI* i_sr;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_sr;
    unsigned char out_dr;
    unsigned char out_sr;
  } sfmt_ld_plus;
  struct { /*  */
    IADDR i_disp16;
    SI* i_src1;
    SI* i_src2;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_src1;
    unsigned char in_src2;
  } sfmt_beq;
  struct { /*  */
    SI* i_dr;
    SI* i_sr;
    UINT f_r1;
    UINT f_r2;
    UINT f_uimm16;
    unsigned char in_sr;
    unsigned char out_dr;
  } sfmt_and3;
  struct { /*  */
    SI* i_dr;
    SI* i_sr;
    INT f_simm16;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_sr;
    unsigned char out_dr;
  } sfmt_add3;
  struct { /*  */
    SI* i_dr;
    SI* i_sr;
    UINT f_r1;
    UINT f_r2;
    unsigned char in_dr;
    unsigned char in_sr;
    unsigned char out_dr;
  } sfmt_add;
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

#define EXTRACT_IFMT_ADD_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_ADD_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_ADD3_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ADD3_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_AND3_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_AND3_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_OR3_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_OR3_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_ADDI_VARS \
  UINT f_op1; \
  UINT f_r1; \
  INT f_simm8; \
  unsigned int length;
#define EXTRACT_IFMT_ADDI_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_simm8 = EXTRACT_MSB0_INT (insn, 16, 8, 8); \

#define EXTRACT_IFMT_ADDV3_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ADDV3_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_BC8_VARS \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp8; \
  unsigned int length;
#define EXTRACT_IFMT_BC8_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4)))); \

#define EXTRACT_IFMT_BC24_VARS \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp24; \
  unsigned int length;
#define EXTRACT_IFMT_BC24_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_disp24 = ((((EXTRACT_MSB0_INT (insn, 32, 8, 24)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BEQ_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  SI f_disp16; \
  unsigned int length;
#define EXTRACT_IFMT_BEQ_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 16, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BEQZ_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  SI f_disp16; \
  unsigned int length;
#define EXTRACT_IFMT_BEQZ_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 16, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_CMP_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_CMP_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_CMPI_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_CMPI_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_CMPZ_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_CMPZ_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_DIV_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_DIV_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_JC_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_JC_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_LD24_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_uimm24; \
  unsigned int length;
#define EXTRACT_IFMT_LD24_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_uimm24 = EXTRACT_MSB0_UINT (insn, 32, 8, 24); \

#define EXTRACT_IFMT_LDI16_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_LDI16_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_MACHI_A_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_acc; \
  UINT f_op23; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_acc = EXTRACT_MSB0_UINT (insn, 16, 8, 1); \
  f_op23 = EXTRACT_MSB0_UINT (insn, 16, 9, 3); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_MVFACHI_A_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_op3; \
  unsigned int length;
#define EXTRACT_IFMT_MVFACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2); \
  f_op3 = EXTRACT_MSB0_UINT (insn, 16, 14, 2); \

#define EXTRACT_IFMT_MVFC_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MVFC_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_MVTACHI_A_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_op3; \
  unsigned int length;
#define EXTRACT_IFMT_MVTACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2); \
  f_op3 = EXTRACT_MSB0_UINT (insn, 16, 14, 2); \

#define EXTRACT_IFMT_MVTC_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MVTC_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_NOP_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_NOP_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_RAC_DSI_VARS \
  UINT f_op1; \
  UINT f_accd; \
  UINT f_bits67; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_bit14; \
  SI f_imm1; \
  unsigned int length;
#define EXTRACT_IFMT_RAC_DSI_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_accd = EXTRACT_MSB0_UINT (insn, 16, 4, 2); \
  f_bits67 = EXTRACT_MSB0_UINT (insn, 16, 6, 2); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_MSB0_UINT (insn, 16, 12, 2); \
  f_bit14 = EXTRACT_MSB0_UINT (insn, 16, 14, 1); \
  f_imm1 = ((EXTRACT_MSB0_UINT (insn, 16, 15, 1)) + (1)); \

#define EXTRACT_IFMT_SETH_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_hi16; \
  unsigned int length;
#define EXTRACT_IFMT_SETH_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_hi16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_SLLI_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_shift_op2; \
  UINT f_uimm5; \
  unsigned int length;
#define EXTRACT_IFMT_SLLI_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_shift_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 3); \
  f_uimm5 = EXTRACT_MSB0_UINT (insn, 16, 11, 5); \

#define EXTRACT_IFMT_ST_D_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ST_D_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_TRAP_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_uimm4; \
  unsigned int length;
#define EXTRACT_IFMT_TRAP_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_uimm4 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_SATB_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_SATB_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_CLRPSW_VARS \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_uimm8; \
  unsigned int length;
#define EXTRACT_IFMT_CLRPSW_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_MSB0_UINT (insn, 16, 4, 4); \
  f_uimm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8); \

#define EXTRACT_IFMT_BSET_VARS \
  UINT f_op1; \
  UINT f_bit4; \
  UINT f_uimm3; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_BSET_CODE \
  length = 4; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 32, 0, 4); \
  f_bit4 = EXTRACT_MSB0_UINT (insn, 32, 4, 1); \
  f_uimm3 = EXTRACT_MSB0_UINT (insn, 32, 5, 3); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_MSB0_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_BTST_VARS \
  UINT f_op1; \
  UINT f_bit4; \
  UINT f_uimm3; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_BTST_CODE \
  length = 2; \
  f_op1 = EXTRACT_MSB0_UINT (insn, 16, 0, 4); \
  f_bit4 = EXTRACT_MSB0_UINT (insn, 16, 4, 1); \
  f_uimm3 = EXTRACT_MSB0_UINT (insn, 16, 5, 3); \
  f_op2 = EXTRACT_MSB0_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_MSB0_UINT (insn, 16, 12, 4); \

/* Queued output values of an instruction.  */

struct parexec {
  union {
    struct { /* empty sformat for unspecified field list */
      int empty;
    } sfmt_empty;
    struct { /* e.g. add $dr,$sr */
      SI dr;
    } sfmt_add;
    struct { /* e.g. add3 $dr,$sr,$hash$slo16 */
      SI dr;
    } sfmt_add3;
    struct { /* e.g. and3 $dr,$sr,$uimm16 */
      SI dr;
    } sfmt_and3;
    struct { /* e.g. or3 $dr,$sr,$hash$ulo16 */
      SI dr;
    } sfmt_or3;
    struct { /* e.g. addi $dr,$simm8 */
      SI dr;
    } sfmt_addi;
    struct { /* e.g. addv $dr,$sr */
      BI condbit;
      SI dr;
    } sfmt_addv;
    struct { /* e.g. addv3 $dr,$sr,$simm16 */
      BI condbit;
      SI dr;
    } sfmt_addv3;
    struct { /* e.g. addx $dr,$sr */
      BI condbit;
      SI dr;
    } sfmt_addx;
    struct { /* e.g. bc.s $disp8 */
      USI pc;
    } sfmt_bc8;
    struct { /* e.g. bc.l $disp24 */
      USI pc;
    } sfmt_bc24;
    struct { /* e.g. beq $src1,$src2,$disp16 */
      USI pc;
    } sfmt_beq;
    struct { /* e.g. beqz $src2,$disp16 */
      USI pc;
    } sfmt_beqz;
    struct { /* e.g. bl.s $disp8 */
      SI h_gr_SI_14;
      USI pc;
    } sfmt_bl8;
    struct { /* e.g. bl.l $disp24 */
      SI h_gr_SI_14;
      USI pc;
    } sfmt_bl24;
    struct { /* e.g. bcl.s $disp8 */
      SI h_gr_SI_14;
      USI pc;
    } sfmt_bcl8;
    struct { /* e.g. bcl.l $disp24 */
      SI h_gr_SI_14;
      USI pc;
    } sfmt_bcl24;
    struct { /* e.g. bra.s $disp8 */
      USI pc;
    } sfmt_bra8;
    struct { /* e.g. bra.l $disp24 */
      USI pc;
    } sfmt_bra24;
    struct { /* e.g. cmp $src1,$src2 */
      BI condbit;
    } sfmt_cmp;
    struct { /* e.g. cmpi $src2,$simm16 */
      BI condbit;
    } sfmt_cmpi;
    struct { /* e.g. cmpz $src2 */
      BI condbit;
    } sfmt_cmpz;
    struct { /* e.g. div $dr,$sr */
      SI dr;
    } sfmt_div;
    struct { /* e.g. jc $sr */
      USI pc;
    } sfmt_jc;
    struct { /* e.g. jl $sr */
      SI h_gr_SI_14;
      USI pc;
    } sfmt_jl;
    struct { /* e.g. jmp $sr */
      USI pc;
    } sfmt_jmp;
    struct { /* e.g. ld $dr,@$sr */
      SI dr;
    } sfmt_ld;
    struct { /* e.g. ld $dr,@($slo16,$sr) */
      SI dr;
    } sfmt_ld_d;
    struct { /* e.g. ldb $dr,@$sr */
      SI dr;
    } sfmt_ldb;
    struct { /* e.g. ldb $dr,@($slo16,$sr) */
      SI dr;
    } sfmt_ldb_d;
    struct { /* e.g. ldh $dr,@$sr */
      SI dr;
    } sfmt_ldh;
    struct { /* e.g. ldh $dr,@($slo16,$sr) */
      SI dr;
    } sfmt_ldh_d;
    struct { /* e.g. ld $dr,@$sr+ */
      SI dr;
      SI sr;
    } sfmt_ld_plus;
    struct { /* e.g. ld24 $dr,$uimm24 */
      SI dr;
    } sfmt_ld24;
    struct { /* e.g. ldi8 $dr,$simm8 */
      SI dr;
    } sfmt_ldi8;
    struct { /* e.g. ldi16 $dr,$hash$slo16 */
      SI dr;
    } sfmt_ldi16;
    struct { /* e.g. lock $dr,@$sr */
      SI dr;
      BI h_lock_BI;
    } sfmt_lock;
    struct { /* e.g. machi $src1,$src2,$acc */
      DI acc;
    } sfmt_machi_a;
    struct { /* e.g. mulhi $src1,$src2,$acc */
      DI acc;
    } sfmt_mulhi_a;
    struct { /* e.g. mv $dr,$sr */
      SI dr;
    } sfmt_mv;
    struct { /* e.g. mvfachi $dr,$accs */
      SI dr;
    } sfmt_mvfachi_a;
    struct { /* e.g. mvfc $dr,$scr */
      SI dr;
    } sfmt_mvfc;
    struct { /* e.g. mvtachi $src1,$accs */
      DI accs;
    } sfmt_mvtachi_a;
    struct { /* e.g. mvtc $sr,$dcr */
      USI dcr;
    } sfmt_mvtc;
    struct { /* e.g. nop */
      int empty;
    } sfmt_nop;
    struct { /* e.g. rac $accd,$accs,$imm1 */
      DI accd;
    } sfmt_rac_dsi;
    struct { /* e.g. rte */
      UQI h_bpsw_UQI;
      USI h_cr_USI_6;
      UQI h_psw_UQI;
      USI pc;
    } sfmt_rte;
    struct { /* e.g. seth $dr,$hash$hi16 */
      SI dr;
    } sfmt_seth;
    struct { /* e.g. sll3 $dr,$sr,$simm16 */
      SI dr;
    } sfmt_sll3;
    struct { /* e.g. slli $dr,$uimm5 */
      SI dr;
    } sfmt_slli;
    struct { /* e.g. st $src1,@$src2 */
      SI h_memory_SI_src2;
      USI h_memory_SI_src2_idx;
    } sfmt_st;
    struct { /* e.g. st $src1,@($slo16,$src2) */
      SI h_memory_SI_add__DFLT_src2_slo16;
      USI h_memory_SI_add__DFLT_src2_slo16_idx;
    } sfmt_st_d;
    struct { /* e.g. stb $src1,@$src2 */
      QI h_memory_QI_src2;
      USI h_memory_QI_src2_idx;
    } sfmt_stb;
    struct { /* e.g. stb $src1,@($slo16,$src2) */
      QI h_memory_QI_add__DFLT_src2_slo16;
      USI h_memory_QI_add__DFLT_src2_slo16_idx;
    } sfmt_stb_d;
    struct { /* e.g. sth $src1,@$src2 */
      HI h_memory_HI_src2;
      USI h_memory_HI_src2_idx;
    } sfmt_sth;
    struct { /* e.g. sth $src1,@($slo16,$src2) */
      HI h_memory_HI_add__DFLT_src2_slo16;
      USI h_memory_HI_add__DFLT_src2_slo16_idx;
    } sfmt_sth_d;
    struct { /* e.g. st $src1,@+$src2 */
      SI h_memory_SI_new_src2;
      USI h_memory_SI_new_src2_idx;
      SI src2;
    } sfmt_st_plus;
    struct { /* e.g. sth $src1,@$src2+ */
      HI h_memory_HI_new_src2;
      USI h_memory_HI_new_src2_idx;
      SI src2;
    } sfmt_sth_plus;
    struct { /* e.g. stb $src1,@$src2+ */
      QI h_memory_QI_new_src2;
      USI h_memory_QI_new_src2_idx;
      SI src2;
    } sfmt_stb_plus;
    struct { /* e.g. trap $uimm4 */
      UQI h_bbpsw_UQI;
      UQI h_bpsw_UQI;
      USI h_cr_USI_14;
      USI h_cr_USI_6;
      UQI h_psw_UQI;
      SI pc;
    } sfmt_trap;
    struct { /* e.g. unlock $src1,@$src2 */
      BI h_lock_BI;
      SI h_memory_SI_src2;
      USI h_memory_SI_src2_idx;
    } sfmt_unlock;
    struct { /* e.g. satb $dr,$sr */
      SI dr;
    } sfmt_satb;
    struct { /* e.g. sat $dr,$sr */
      SI dr;
    } sfmt_sat;
    struct { /* e.g. sadd */
      DI h_accums_DI_0;
    } sfmt_sadd;
    struct { /* e.g. macwu1 $src1,$src2 */
      DI h_accums_DI_1;
    } sfmt_macwu1;
    struct { /* e.g. msblo $src1,$src2 */
      DI accum;
    } sfmt_msblo;
    struct { /* e.g. mulwu1 $src1,$src2 */
      DI h_accums_DI_1;
    } sfmt_mulwu1;
    struct { /* e.g. sc */
      int empty;
    } sfmt_sc;
    struct { /* e.g. clrpsw $uimm8 */
      USI h_cr_USI_0;
    } sfmt_clrpsw;
    struct { /* e.g. setpsw $uimm8 */
      USI h_cr_USI_0;
    } sfmt_setpsw;
    struct { /* e.g. bset $uimm3,@($slo16,$sr) */
      QI h_memory_QI_add__DFLT_sr_slo16;
      USI h_memory_QI_add__DFLT_sr_slo16_idx;
    } sfmt_bset;
    struct { /* e.g. btst $uimm3,$sr */
      BI condbit;
    } sfmt_btst;
  } operands;
  /* For conditionally written operands, bitmask of which ones were.  */
  int written;
};

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_M32R2F_H */
