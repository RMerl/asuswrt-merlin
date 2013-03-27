/* CPU family header for iq2000bf.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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

#ifndef CPU_IQ2000BF_H
#define CPU_IQ2000BF_H

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
#define GET_H_PC() get_h_pc (current_cpu)
#define SET_H_PC(x) \
do { \
set_h_pc (current_cpu, (x));\
;} while (0)
  /* General purpose registers */
  SI h_gr[32];
#define GET_H_GR(index) (((index) == (0))) ? (0) : (CPU (h_gr[index]))
#define SET_H_GR(index, x) \
do { \
if ((((index)) == (0))) {\
((void) 0); /*nop*/\
}\
 else {\
CPU (h_gr[(index)]) = (x);\
}\
;} while (0)
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} IQ2000BF_CPU_DATA;

/* Cover fns for register access.  */
USI iq2000bf_h_pc_get (SIM_CPU *);
void iq2000bf_h_pc_set (SIM_CPU *, USI);
SI iq2000bf_h_gr_get (SIM_CPU *, UINT);
void iq2000bf_h_gr_set (SIM_CPU *, UINT, SI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN iq2000bf_fetch_register;
extern CPUREG_STORE_FN iq2000bf_store_register;

typedef struct {
  int empty;
} MODEL_IQ2000_DATA;

/* Instruction argument buffer.  */

union sem_fields {
  struct { /* no operands */
    int empty;
  } fmt_empty;
  struct { /*  */
    IADDR i_jmptarg;
  } sfmt_j;
  struct { /*  */
    IADDR i_offset;
    UINT f_rs;
    UINT f_rt;
  } sfmt_bbi;
  struct { /*  */
    UINT f_imm;
    UINT f_rs;
    UINT f_rt;
  } sfmt_addi;
  struct { /*  */
    UINT f_mask;
    UINT f_rd;
    UINT f_rs;
    UINT f_rt;
  } sfmt_mrgb;
  struct { /*  */
    UINT f_maskl;
    UINT f_rd;
    UINT f_rs;
    UINT f_rt;
    UINT f_shamt;
  } sfmt_ram;
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
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_ADD_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ADDI_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_imm; \
  unsigned int length;
#define EXTRACT_IFMT_ADDI_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_RAM_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_5; \
  UINT f_maskl; \
  unsigned int length;
#define EXTRACT_IFMT_RAM_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_5 = EXTRACT_LSB0_UINT (insn, 32, 5, 1); \
  f_maskl = EXTRACT_LSB0_UINT (insn, 32, 4, 5); \

#define EXTRACT_IFMT_SLL_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_SLL_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SLMV_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_SLMV_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SLTI_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_imm; \
  unsigned int length;
#define EXTRACT_IFMT_SLTI_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_BBI_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  SI f_offset; \
  unsigned int length;
#define EXTRACT_IFMT_BBI_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4)))); \

#define EXTRACT_IFMT_BBV_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  SI f_offset; \
  unsigned int length;
#define EXTRACT_IFMT_BBV_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4)))); \

#define EXTRACT_IFMT_BGEZ_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  SI f_offset; \
  unsigned int length;
#define EXTRACT_IFMT_BGEZ_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4)))); \

#define EXTRACT_IFMT_JALR_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_JALR_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_JR_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_JR_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LB_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_imm; \
  unsigned int length;
#define EXTRACT_IFMT_LB_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_LUI_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_imm; \
  unsigned int length;
#define EXTRACT_IFMT_LUI_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_BREAK_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_BREAK_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SYSCALL_VARS \
  UINT f_opcode; \
  UINT f_excode; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_SYSCALL_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_excode = EXTRACT_LSB0_UINT (insn, 32, 25, 20); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ANDOUI_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_imm; \
  unsigned int length;
#define EXTRACT_IFMT_ANDOUI_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_MRGB_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_10; \
  UINT f_mask; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_MRGB_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_10 = EXTRACT_LSB0_UINT (insn, 32, 10, 1); \
  f_mask = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_BC0F_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  SI f_offset; \
  unsigned int length;
#define EXTRACT_IFMT_BC0F_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4)))); \

#define EXTRACT_IFMT_CFC0_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_10_11; \
  unsigned int length;
#define EXTRACT_IFMT_CFC0_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_10_11 = EXTRACT_LSB0_UINT (insn, 32, 10, 11); \

#define EXTRACT_IFMT_CHKHDR_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_CHKHDR_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LULCK_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_rd; \
  UINT f_shamt; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_LULCK_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5); \
  f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_PKRLR1_VARS \
  UINT f_opcode; \
  UINT f_rs; \
  UINT f_rt; \
  UINT f_count; \
  UINT f_index; \
  unsigned int length;
#define EXTRACT_IFMT_PKRLR1_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5); \
  f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5); \
  f_count = EXTRACT_LSB0_UINT (insn, 32, 15, 7); \
  f_index = EXTRACT_LSB0_UINT (insn, 32, 8, 9); \

#define EXTRACT_IFMT_RFE_VARS \
  UINT f_opcode; \
  UINT f_25; \
  UINT f_24_19; \
  UINT f_func; \
  unsigned int length;
#define EXTRACT_IFMT_RFE_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_25 = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_24_19 = EXTRACT_LSB0_UINT (insn, 32, 24, 19); \
  f_func = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_J_VARS \
  UINT f_opcode; \
  UINT f_rsrvd; \
  USI f_jtarg; \
  unsigned int length;
#define EXTRACT_IFMT_J_CODE \
  length = 4; \
  f_opcode = EXTRACT_LSB0_UINT (insn, 32, 31, 6); \
  f_rsrvd = EXTRACT_LSB0_UINT (insn, 32, 25, 10); \
  f_jtarg = ((((pc) & (0xf0000000))) | (((EXTRACT_LSB0_UINT (insn, 32, 15, 16)) << (2)))); \

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_IQ2000BF_H */
