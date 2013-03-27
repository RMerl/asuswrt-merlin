/* Simulator instruction decoder for crisv10f.

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

#define WANT_CPU crisv10f
#define WANT_CPU_CRISV10F

#include "sim-main.h"
#include "sim-assert.h"
#include "cgen-ops.h"

/* The instruction descriptor array.
   This is computed at runtime.  Space for it is not malloc'd to save a
   teensy bit of cpu in the decoder.  Moving it to malloc space is trivial
   but won't be done until necessary (we don't currently support the runtime
   addition of instructions nor an SMP machine with different cpus).  */
static IDESC crisv10f_insn_data[CRISV10F_INSN__MAX];

/* Commas between elements are contained in the macros.
   Some of these are conditionally compiled out.  */

static const struct insn_sem crisv10f_insn_sem[] =
{
  { VIRTUAL_INSN_X_INVALID, CRISV10F_INSN_X_INVALID, CRISV10F_SFMT_EMPTY },
  { VIRTUAL_INSN_X_AFTER, CRISV10F_INSN_X_AFTER, CRISV10F_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEFORE, CRISV10F_INSN_X_BEFORE, CRISV10F_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CTI_CHAIN, CRISV10F_INSN_X_CTI_CHAIN, CRISV10F_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CHAIN, CRISV10F_INSN_X_CHAIN, CRISV10F_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEGIN, CRISV10F_INSN_X_BEGIN, CRISV10F_SFMT_EMPTY },
  { CRIS_INSN_NOP, CRISV10F_INSN_NOP, CRISV10F_SFMT_NOP },
  { CRIS_INSN_MOVE_B_R, CRISV10F_INSN_MOVE_B_R, CRISV10F_SFMT_MOVE_B_R },
  { CRIS_INSN_MOVE_W_R, CRISV10F_INSN_MOVE_W_R, CRISV10F_SFMT_MOVE_B_R },
  { CRIS_INSN_MOVE_D_R, CRISV10F_INSN_MOVE_D_R, CRISV10F_SFMT_MOVE_D_R },
  { CRIS_INSN_MOVEPCR, CRISV10F_INSN_MOVEPCR, CRISV10F_SFMT_MOVEPCR },
  { CRIS_INSN_MOVEQ, CRISV10F_INSN_MOVEQ, CRISV10F_SFMT_MOVEQ },
  { CRIS_INSN_MOVS_B_R, CRISV10F_INSN_MOVS_B_R, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_MOVS_W_R, CRISV10F_INSN_MOVS_W_R, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_MOVU_B_R, CRISV10F_INSN_MOVU_B_R, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_MOVU_W_R, CRISV10F_INSN_MOVU_W_R, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_MOVECBR, CRISV10F_INSN_MOVECBR, CRISV10F_SFMT_MOVECBR },
  { CRIS_INSN_MOVECWR, CRISV10F_INSN_MOVECWR, CRISV10F_SFMT_MOVECWR },
  { CRIS_INSN_MOVECDR, CRISV10F_INSN_MOVECDR, CRISV10F_SFMT_MOVECDR },
  { CRIS_INSN_MOVSCBR, CRISV10F_INSN_MOVSCBR, CRISV10F_SFMT_MOVSCBR },
  { CRIS_INSN_MOVSCWR, CRISV10F_INSN_MOVSCWR, CRISV10F_SFMT_MOVSCWR },
  { CRIS_INSN_MOVUCBR, CRISV10F_INSN_MOVUCBR, CRISV10F_SFMT_MOVUCBR },
  { CRIS_INSN_MOVUCWR, CRISV10F_INSN_MOVUCWR, CRISV10F_SFMT_MOVUCWR },
  { CRIS_INSN_ADDQ, CRISV10F_INSN_ADDQ, CRISV10F_SFMT_ADDQ },
  { CRIS_INSN_SUBQ, CRISV10F_INSN_SUBQ, CRISV10F_SFMT_ADDQ },
  { CRIS_INSN_CMP_R_B_R, CRISV10F_INSN_CMP_R_B_R, CRISV10F_SFMT_CMP_R_B_R },
  { CRIS_INSN_CMP_R_W_R, CRISV10F_INSN_CMP_R_W_R, CRISV10F_SFMT_CMP_R_B_R },
  { CRIS_INSN_CMP_R_D_R, CRISV10F_INSN_CMP_R_D_R, CRISV10F_SFMT_CMP_R_B_R },
  { CRIS_INSN_CMP_M_B_M, CRISV10F_INSN_CMP_M_B_M, CRISV10F_SFMT_CMP_M_B_M },
  { CRIS_INSN_CMP_M_W_M, CRISV10F_INSN_CMP_M_W_M, CRISV10F_SFMT_CMP_M_W_M },
  { CRIS_INSN_CMP_M_D_M, CRISV10F_INSN_CMP_M_D_M, CRISV10F_SFMT_CMP_M_D_M },
  { CRIS_INSN_CMPCBR, CRISV10F_INSN_CMPCBR, CRISV10F_SFMT_CMPCBR },
  { CRIS_INSN_CMPCWR, CRISV10F_INSN_CMPCWR, CRISV10F_SFMT_CMPCWR },
  { CRIS_INSN_CMPCDR, CRISV10F_INSN_CMPCDR, CRISV10F_SFMT_CMPCDR },
  { CRIS_INSN_CMPQ, CRISV10F_INSN_CMPQ, CRISV10F_SFMT_CMPQ },
  { CRIS_INSN_CMPS_M_B_M, CRISV10F_INSN_CMPS_M_B_M, CRISV10F_SFMT_CMP_M_B_M },
  { CRIS_INSN_CMPS_M_W_M, CRISV10F_INSN_CMPS_M_W_M, CRISV10F_SFMT_CMP_M_W_M },
  { CRIS_INSN_CMPSCBR, CRISV10F_INSN_CMPSCBR, CRISV10F_SFMT_CMPCBR },
  { CRIS_INSN_CMPSCWR, CRISV10F_INSN_CMPSCWR, CRISV10F_SFMT_CMPCWR },
  { CRIS_INSN_CMPU_M_B_M, CRISV10F_INSN_CMPU_M_B_M, CRISV10F_SFMT_CMP_M_B_M },
  { CRIS_INSN_CMPU_M_W_M, CRISV10F_INSN_CMPU_M_W_M, CRISV10F_SFMT_CMP_M_W_M },
  { CRIS_INSN_CMPUCBR, CRISV10F_INSN_CMPUCBR, CRISV10F_SFMT_CMPUCBR },
  { CRIS_INSN_CMPUCWR, CRISV10F_INSN_CMPUCWR, CRISV10F_SFMT_CMPUCWR },
  { CRIS_INSN_MOVE_M_B_M, CRISV10F_INSN_MOVE_M_B_M, CRISV10F_SFMT_MOVE_M_B_M },
  { CRIS_INSN_MOVE_M_W_M, CRISV10F_INSN_MOVE_M_W_M, CRISV10F_SFMT_MOVE_M_W_M },
  { CRIS_INSN_MOVE_M_D_M, CRISV10F_INSN_MOVE_M_D_M, CRISV10F_SFMT_MOVE_M_D_M },
  { CRIS_INSN_MOVS_M_B_M, CRISV10F_INSN_MOVS_M_B_M, CRISV10F_SFMT_MOVS_M_B_M },
  { CRIS_INSN_MOVS_M_W_M, CRISV10F_INSN_MOVS_M_W_M, CRISV10F_SFMT_MOVS_M_W_M },
  { CRIS_INSN_MOVU_M_B_M, CRISV10F_INSN_MOVU_M_B_M, CRISV10F_SFMT_MOVS_M_B_M },
  { CRIS_INSN_MOVU_M_W_M, CRISV10F_INSN_MOVU_M_W_M, CRISV10F_SFMT_MOVS_M_W_M },
  { CRIS_INSN_MOVE_R_SPRV10, CRISV10F_INSN_MOVE_R_SPRV10, CRISV10F_SFMT_MOVE_R_SPRV10 },
  { CRIS_INSN_MOVE_SPR_RV10, CRISV10F_INSN_MOVE_SPR_RV10, CRISV10F_SFMT_MOVE_SPR_RV10 },
  { CRIS_INSN_RET_TYPE, CRISV10F_INSN_RET_TYPE, CRISV10F_SFMT_RET_TYPE },
  { CRIS_INSN_MOVE_M_SPRV10, CRISV10F_INSN_MOVE_M_SPRV10, CRISV10F_SFMT_MOVE_M_SPRV10 },
  { CRIS_INSN_MOVE_C_SPRV10_P5, CRISV10F_INSN_MOVE_C_SPRV10_P5, CRISV10F_SFMT_MOVE_C_SPRV10_P5 },
  { CRIS_INSN_MOVE_C_SPRV10_P9, CRISV10F_INSN_MOVE_C_SPRV10_P9, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P10, CRISV10F_INSN_MOVE_C_SPRV10_P10, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P11, CRISV10F_INSN_MOVE_C_SPRV10_P11, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P12, CRISV10F_INSN_MOVE_C_SPRV10_P12, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P13, CRISV10F_INSN_MOVE_C_SPRV10_P13, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P7, CRISV10F_INSN_MOVE_C_SPRV10_P7, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P14, CRISV10F_INSN_MOVE_C_SPRV10_P14, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_C_SPRV10_P15, CRISV10F_INSN_MOVE_C_SPRV10_P15, CRISV10F_SFMT_MOVE_C_SPRV10_P9 },
  { CRIS_INSN_MOVE_SPR_MV10, CRISV10F_INSN_MOVE_SPR_MV10, CRISV10F_SFMT_MOVE_SPR_MV10 },
  { CRIS_INSN_SBFS, CRISV10F_INSN_SBFS, CRISV10F_SFMT_SBFS },
  { CRIS_INSN_MOVEM_R_M, CRISV10F_INSN_MOVEM_R_M, CRISV10F_SFMT_MOVEM_R_M },
  { CRIS_INSN_MOVEM_M_R, CRISV10F_INSN_MOVEM_M_R, CRISV10F_SFMT_MOVEM_M_R },
  { CRIS_INSN_MOVEM_M_PC, CRISV10F_INSN_MOVEM_M_PC, CRISV10F_SFMT_MOVEM_M_PC },
  { CRIS_INSN_ADD_B_R, CRISV10F_INSN_ADD_B_R, CRISV10F_SFMT_ADD_B_R },
  { CRIS_INSN_ADD_W_R, CRISV10F_INSN_ADD_W_R, CRISV10F_SFMT_ADD_B_R },
  { CRIS_INSN_ADD_D_R, CRISV10F_INSN_ADD_D_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_ADD_M_B_M, CRISV10F_INSN_ADD_M_B_M, CRISV10F_SFMT_ADD_M_B_M },
  { CRIS_INSN_ADD_M_W_M, CRISV10F_INSN_ADD_M_W_M, CRISV10F_SFMT_ADD_M_W_M },
  { CRIS_INSN_ADD_M_D_M, CRISV10F_INSN_ADD_M_D_M, CRISV10F_SFMT_ADD_M_D_M },
  { CRIS_INSN_ADDCBR, CRISV10F_INSN_ADDCBR, CRISV10F_SFMT_ADDCBR },
  { CRIS_INSN_ADDCWR, CRISV10F_INSN_ADDCWR, CRISV10F_SFMT_ADDCWR },
  { CRIS_INSN_ADDCDR, CRISV10F_INSN_ADDCDR, CRISV10F_SFMT_ADDCDR },
  { CRIS_INSN_ADDCPC, CRISV10F_INSN_ADDCPC, CRISV10F_SFMT_ADDCPC },
  { CRIS_INSN_ADDS_B_R, CRISV10F_INSN_ADDS_B_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_ADDS_W_R, CRISV10F_INSN_ADDS_W_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_ADDS_M_B_M, CRISV10F_INSN_ADDS_M_B_M, CRISV10F_SFMT_ADDS_M_B_M },
  { CRIS_INSN_ADDS_M_W_M, CRISV10F_INSN_ADDS_M_W_M, CRISV10F_SFMT_ADDS_M_W_M },
  { CRIS_INSN_ADDSCBR, CRISV10F_INSN_ADDSCBR, CRISV10F_SFMT_ADDSCBR },
  { CRIS_INSN_ADDSCWR, CRISV10F_INSN_ADDSCWR, CRISV10F_SFMT_ADDSCWR },
  { CRIS_INSN_ADDSPCPC, CRISV10F_INSN_ADDSPCPC, CRISV10F_SFMT_ADDSPCPC },
  { CRIS_INSN_ADDU_B_R, CRISV10F_INSN_ADDU_B_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_ADDU_W_R, CRISV10F_INSN_ADDU_W_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_ADDU_M_B_M, CRISV10F_INSN_ADDU_M_B_M, CRISV10F_SFMT_ADDS_M_B_M },
  { CRIS_INSN_ADDU_M_W_M, CRISV10F_INSN_ADDU_M_W_M, CRISV10F_SFMT_ADDS_M_W_M },
  { CRIS_INSN_ADDUCBR, CRISV10F_INSN_ADDUCBR, CRISV10F_SFMT_ADDSCBR },
  { CRIS_INSN_ADDUCWR, CRISV10F_INSN_ADDUCWR, CRISV10F_SFMT_ADDSCWR },
  { CRIS_INSN_SUB_B_R, CRISV10F_INSN_SUB_B_R, CRISV10F_SFMT_ADD_B_R },
  { CRIS_INSN_SUB_W_R, CRISV10F_INSN_SUB_W_R, CRISV10F_SFMT_ADD_B_R },
  { CRIS_INSN_SUB_D_R, CRISV10F_INSN_SUB_D_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_SUB_M_B_M, CRISV10F_INSN_SUB_M_B_M, CRISV10F_SFMT_ADD_M_B_M },
  { CRIS_INSN_SUB_M_W_M, CRISV10F_INSN_SUB_M_W_M, CRISV10F_SFMT_ADD_M_W_M },
  { CRIS_INSN_SUB_M_D_M, CRISV10F_INSN_SUB_M_D_M, CRISV10F_SFMT_ADD_M_D_M },
  { CRIS_INSN_SUBCBR, CRISV10F_INSN_SUBCBR, CRISV10F_SFMT_ADDCBR },
  { CRIS_INSN_SUBCWR, CRISV10F_INSN_SUBCWR, CRISV10F_SFMT_ADDCWR },
  { CRIS_INSN_SUBCDR, CRISV10F_INSN_SUBCDR, CRISV10F_SFMT_ADDCDR },
  { CRIS_INSN_SUBS_B_R, CRISV10F_INSN_SUBS_B_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_SUBS_W_R, CRISV10F_INSN_SUBS_W_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_SUBS_M_B_M, CRISV10F_INSN_SUBS_M_B_M, CRISV10F_SFMT_ADDS_M_B_M },
  { CRIS_INSN_SUBS_M_W_M, CRISV10F_INSN_SUBS_M_W_M, CRISV10F_SFMT_ADDS_M_W_M },
  { CRIS_INSN_SUBSCBR, CRISV10F_INSN_SUBSCBR, CRISV10F_SFMT_ADDSCBR },
  { CRIS_INSN_SUBSCWR, CRISV10F_INSN_SUBSCWR, CRISV10F_SFMT_ADDSCWR },
  { CRIS_INSN_SUBU_B_R, CRISV10F_INSN_SUBU_B_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_SUBU_W_R, CRISV10F_INSN_SUBU_W_R, CRISV10F_SFMT_ADD_D_R },
  { CRIS_INSN_SUBU_M_B_M, CRISV10F_INSN_SUBU_M_B_M, CRISV10F_SFMT_ADDS_M_B_M },
  { CRIS_INSN_SUBU_M_W_M, CRISV10F_INSN_SUBU_M_W_M, CRISV10F_SFMT_ADDS_M_W_M },
  { CRIS_INSN_SUBUCBR, CRISV10F_INSN_SUBUCBR, CRISV10F_SFMT_ADDSCBR },
  { CRIS_INSN_SUBUCWR, CRISV10F_INSN_SUBUCWR, CRISV10F_SFMT_ADDSCWR },
  { CRIS_INSN_ADDI_B_R, CRISV10F_INSN_ADDI_B_R, CRISV10F_SFMT_ADDI_B_R },
  { CRIS_INSN_ADDI_W_R, CRISV10F_INSN_ADDI_W_R, CRISV10F_SFMT_ADDI_B_R },
  { CRIS_INSN_ADDI_D_R, CRISV10F_INSN_ADDI_D_R, CRISV10F_SFMT_ADDI_B_R },
  { CRIS_INSN_NEG_B_R, CRISV10F_INSN_NEG_B_R, CRISV10F_SFMT_NEG_B_R },
  { CRIS_INSN_NEG_W_R, CRISV10F_INSN_NEG_W_R, CRISV10F_SFMT_NEG_B_R },
  { CRIS_INSN_NEG_D_R, CRISV10F_INSN_NEG_D_R, CRISV10F_SFMT_NEG_D_R },
  { CRIS_INSN_TEST_M_B_M, CRISV10F_INSN_TEST_M_B_M, CRISV10F_SFMT_TEST_M_B_M },
  { CRIS_INSN_TEST_M_W_M, CRISV10F_INSN_TEST_M_W_M, CRISV10F_SFMT_TEST_M_W_M },
  { CRIS_INSN_TEST_M_D_M, CRISV10F_INSN_TEST_M_D_M, CRISV10F_SFMT_TEST_M_D_M },
  { CRIS_INSN_MOVE_R_M_B_M, CRISV10F_INSN_MOVE_R_M_B_M, CRISV10F_SFMT_MOVE_R_M_B_M },
  { CRIS_INSN_MOVE_R_M_W_M, CRISV10F_INSN_MOVE_R_M_W_M, CRISV10F_SFMT_MOVE_R_M_W_M },
  { CRIS_INSN_MOVE_R_M_D_M, CRISV10F_INSN_MOVE_R_M_D_M, CRISV10F_SFMT_MOVE_R_M_D_M },
  { CRIS_INSN_MULS_B, CRISV10F_INSN_MULS_B, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MULS_W, CRISV10F_INSN_MULS_W, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MULS_D, CRISV10F_INSN_MULS_D, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MULU_B, CRISV10F_INSN_MULU_B, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MULU_W, CRISV10F_INSN_MULU_W, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MULU_D, CRISV10F_INSN_MULU_D, CRISV10F_SFMT_MULS_B },
  { CRIS_INSN_MSTEP, CRISV10F_INSN_MSTEP, CRISV10F_SFMT_MSTEP },
  { CRIS_INSN_DSTEP, CRISV10F_INSN_DSTEP, CRISV10F_SFMT_DSTEP },
  { CRIS_INSN_ABS, CRISV10F_INSN_ABS, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_AND_B_R, CRISV10F_INSN_AND_B_R, CRISV10F_SFMT_AND_B_R },
  { CRIS_INSN_AND_W_R, CRISV10F_INSN_AND_W_R, CRISV10F_SFMT_AND_W_R },
  { CRIS_INSN_AND_D_R, CRISV10F_INSN_AND_D_R, CRISV10F_SFMT_AND_D_R },
  { CRIS_INSN_AND_M_B_M, CRISV10F_INSN_AND_M_B_M, CRISV10F_SFMT_AND_M_B_M },
  { CRIS_INSN_AND_M_W_M, CRISV10F_INSN_AND_M_W_M, CRISV10F_SFMT_AND_M_W_M },
  { CRIS_INSN_AND_M_D_M, CRISV10F_INSN_AND_M_D_M, CRISV10F_SFMT_AND_M_D_M },
  { CRIS_INSN_ANDCBR, CRISV10F_INSN_ANDCBR, CRISV10F_SFMT_ANDCBR },
  { CRIS_INSN_ANDCWR, CRISV10F_INSN_ANDCWR, CRISV10F_SFMT_ANDCWR },
  { CRIS_INSN_ANDCDR, CRISV10F_INSN_ANDCDR, CRISV10F_SFMT_ANDCDR },
  { CRIS_INSN_ANDQ, CRISV10F_INSN_ANDQ, CRISV10F_SFMT_ANDQ },
  { CRIS_INSN_ORR_B_R, CRISV10F_INSN_ORR_B_R, CRISV10F_SFMT_AND_B_R },
  { CRIS_INSN_ORR_W_R, CRISV10F_INSN_ORR_W_R, CRISV10F_SFMT_AND_W_R },
  { CRIS_INSN_ORR_D_R, CRISV10F_INSN_ORR_D_R, CRISV10F_SFMT_AND_D_R },
  { CRIS_INSN_OR_M_B_M, CRISV10F_INSN_OR_M_B_M, CRISV10F_SFMT_AND_M_B_M },
  { CRIS_INSN_OR_M_W_M, CRISV10F_INSN_OR_M_W_M, CRISV10F_SFMT_AND_M_W_M },
  { CRIS_INSN_OR_M_D_M, CRISV10F_INSN_OR_M_D_M, CRISV10F_SFMT_AND_M_D_M },
  { CRIS_INSN_ORCBR, CRISV10F_INSN_ORCBR, CRISV10F_SFMT_ANDCBR },
  { CRIS_INSN_ORCWR, CRISV10F_INSN_ORCWR, CRISV10F_SFMT_ANDCWR },
  { CRIS_INSN_ORCDR, CRISV10F_INSN_ORCDR, CRISV10F_SFMT_ANDCDR },
  { CRIS_INSN_ORQ, CRISV10F_INSN_ORQ, CRISV10F_SFMT_ANDQ },
  { CRIS_INSN_XOR, CRISV10F_INSN_XOR, CRISV10F_SFMT_DSTEP },
  { CRIS_INSN_SWAP, CRISV10F_INSN_SWAP, CRISV10F_SFMT_SWAP },
  { CRIS_INSN_ASRR_B_R, CRISV10F_INSN_ASRR_B_R, CRISV10F_SFMT_ASRR_B_R },
  { CRIS_INSN_ASRR_W_R, CRISV10F_INSN_ASRR_W_R, CRISV10F_SFMT_ASRR_B_R },
  { CRIS_INSN_ASRR_D_R, CRISV10F_INSN_ASRR_D_R, CRISV10F_SFMT_AND_D_R },
  { CRIS_INSN_ASRQ, CRISV10F_INSN_ASRQ, CRISV10F_SFMT_ASRQ },
  { CRIS_INSN_LSRR_B_R, CRISV10F_INSN_LSRR_B_R, CRISV10F_SFMT_LSRR_B_R },
  { CRIS_INSN_LSRR_W_R, CRISV10F_INSN_LSRR_W_R, CRISV10F_SFMT_LSRR_B_R },
  { CRIS_INSN_LSRR_D_R, CRISV10F_INSN_LSRR_D_R, CRISV10F_SFMT_LSRR_D_R },
  { CRIS_INSN_LSRQ, CRISV10F_INSN_LSRQ, CRISV10F_SFMT_ASRQ },
  { CRIS_INSN_LSLR_B_R, CRISV10F_INSN_LSLR_B_R, CRISV10F_SFMT_LSRR_B_R },
  { CRIS_INSN_LSLR_W_R, CRISV10F_INSN_LSLR_W_R, CRISV10F_SFMT_LSRR_B_R },
  { CRIS_INSN_LSLR_D_R, CRISV10F_INSN_LSLR_D_R, CRISV10F_SFMT_LSRR_D_R },
  { CRIS_INSN_LSLQ, CRISV10F_INSN_LSLQ, CRISV10F_SFMT_ASRQ },
  { CRIS_INSN_BTST, CRISV10F_INSN_BTST, CRISV10F_SFMT_BTST },
  { CRIS_INSN_BTSTQ, CRISV10F_INSN_BTSTQ, CRISV10F_SFMT_BTSTQ },
  { CRIS_INSN_SETF, CRISV10F_INSN_SETF, CRISV10F_SFMT_SETF },
  { CRIS_INSN_CLEARF, CRISV10F_INSN_CLEARF, CRISV10F_SFMT_SETF },
  { CRIS_INSN_BCC_B, CRISV10F_INSN_BCC_B, CRISV10F_SFMT_BCC_B },
  { CRIS_INSN_BA_B, CRISV10F_INSN_BA_B, CRISV10F_SFMT_BA_B },
  { CRIS_INSN_BCC_W, CRISV10F_INSN_BCC_W, CRISV10F_SFMT_BCC_W },
  { CRIS_INSN_BA_W, CRISV10F_INSN_BA_W, CRISV10F_SFMT_BA_W },
  { CRIS_INSN_JUMP_R, CRISV10F_INSN_JUMP_R, CRISV10F_SFMT_JUMP_R },
  { CRIS_INSN_JUMP_M, CRISV10F_INSN_JUMP_M, CRISV10F_SFMT_JUMP_M },
  { CRIS_INSN_JUMP_C, CRISV10F_INSN_JUMP_C, CRISV10F_SFMT_JUMP_C },
  { CRIS_INSN_BREAK, CRISV10F_INSN_BREAK, CRISV10F_SFMT_BREAK },
  { CRIS_INSN_BOUND_R_B_R, CRISV10F_INSN_BOUND_R_B_R, CRISV10F_SFMT_DSTEP },
  { CRIS_INSN_BOUND_R_W_R, CRISV10F_INSN_BOUND_R_W_R, CRISV10F_SFMT_DSTEP },
  { CRIS_INSN_BOUND_R_D_R, CRISV10F_INSN_BOUND_R_D_R, CRISV10F_SFMT_DSTEP },
  { CRIS_INSN_BOUND_M_B_M, CRISV10F_INSN_BOUND_M_B_M, CRISV10F_SFMT_BOUND_M_B_M },
  { CRIS_INSN_BOUND_M_W_M, CRISV10F_INSN_BOUND_M_W_M, CRISV10F_SFMT_BOUND_M_W_M },
  { CRIS_INSN_BOUND_M_D_M, CRISV10F_INSN_BOUND_M_D_M, CRISV10F_SFMT_BOUND_M_D_M },
  { CRIS_INSN_BOUND_CB, CRISV10F_INSN_BOUND_CB, CRISV10F_SFMT_BOUND_CB },
  { CRIS_INSN_BOUND_CW, CRISV10F_INSN_BOUND_CW, CRISV10F_SFMT_BOUND_CW },
  { CRIS_INSN_BOUND_CD, CRISV10F_INSN_BOUND_CD, CRISV10F_SFMT_BOUND_CD },
  { CRIS_INSN_SCC, CRISV10F_INSN_SCC, CRISV10F_SFMT_SCC },
  { CRIS_INSN_LZ, CRISV10F_INSN_LZ, CRISV10F_SFMT_MOVS_B_R },
  { CRIS_INSN_ADDOQ, CRISV10F_INSN_ADDOQ, CRISV10F_SFMT_ADDOQ },
  { CRIS_INSN_BDAPQPC, CRISV10F_INSN_BDAPQPC, CRISV10F_SFMT_BDAPQPC },
  { CRIS_INSN_BDAP_32_PC, CRISV10F_INSN_BDAP_32_PC, CRISV10F_SFMT_BDAP_32_PC },
  { CRIS_INSN_MOVE_M_PCPLUS_P0, CRISV10F_INSN_MOVE_M_PCPLUS_P0, CRISV10F_SFMT_MOVE_M_PCPLUS_P0 },
  { CRIS_INSN_MOVE_M_SPPLUS_P8, CRISV10F_INSN_MOVE_M_SPPLUS_P8, CRISV10F_SFMT_MOVE_M_SPPLUS_P8 },
  { CRIS_INSN_ADDO_M_B_M, CRISV10F_INSN_ADDO_M_B_M, CRISV10F_SFMT_ADDO_M_B_M },
  { CRIS_INSN_ADDO_M_W_M, CRISV10F_INSN_ADDO_M_W_M, CRISV10F_SFMT_ADDO_M_W_M },
  { CRIS_INSN_ADDO_M_D_M, CRISV10F_INSN_ADDO_M_D_M, CRISV10F_SFMT_ADDO_M_D_M },
  { CRIS_INSN_ADDO_CB, CRISV10F_INSN_ADDO_CB, CRISV10F_SFMT_ADDO_CB },
  { CRIS_INSN_ADDO_CW, CRISV10F_INSN_ADDO_CW, CRISV10F_SFMT_ADDO_CW },
  { CRIS_INSN_ADDO_CD, CRISV10F_INSN_ADDO_CD, CRISV10F_SFMT_ADDO_CD },
  { CRIS_INSN_DIP_M, CRISV10F_INSN_DIP_M, CRISV10F_SFMT_DIP_M },
  { CRIS_INSN_DIP_C, CRISV10F_INSN_DIP_C, CRISV10F_SFMT_DIP_C },
  { CRIS_INSN_ADDI_ACR_B_R, CRISV10F_INSN_ADDI_ACR_B_R, CRISV10F_SFMT_ADDI_ACR_B_R },
  { CRIS_INSN_ADDI_ACR_W_R, CRISV10F_INSN_ADDI_ACR_W_R, CRISV10F_SFMT_ADDI_ACR_B_R },
  { CRIS_INSN_ADDI_ACR_D_R, CRISV10F_INSN_ADDI_ACR_D_R, CRISV10F_SFMT_ADDI_ACR_B_R },
  { CRIS_INSN_BIAP_PC_B_R, CRISV10F_INSN_BIAP_PC_B_R, CRISV10F_SFMT_BIAP_PC_B_R },
  { CRIS_INSN_BIAP_PC_W_R, CRISV10F_INSN_BIAP_PC_W_R, CRISV10F_SFMT_BIAP_PC_B_R },
  { CRIS_INSN_BIAP_PC_D_R, CRISV10F_INSN_BIAP_PC_D_R, CRISV10F_SFMT_BIAP_PC_B_R },
};

static const struct insn_sem crisv10f_insn_sem_invalid = {
  VIRTUAL_INSN_X_INVALID, CRISV10F_INSN_X_INVALID, CRISV10F_SFMT_EMPTY
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
crisv10f_init_idesc_table (SIM_CPU *cpu)
{
  IDESC *id,*tabend;
  const struct insn_sem *t,*tend;
  int tabsize = CRISV10F_INSN__MAX;
  IDESC *table = crisv10f_insn_data;

  memset (table, 0, tabsize * sizeof (IDESC));

  /* First set all entries to the `invalid insn'.  */
  t = & crisv10f_insn_sem_invalid;
  for (id = table, tabend = table + tabsize; id < tabend; ++id)
    init_idesc (cpu, id, t);

  /* Now fill in the values for the chosen cpu.  */
  for (t = crisv10f_insn_sem, tend = t + sizeof (crisv10f_insn_sem) / sizeof (*t);
       t != tend; ++t)
    {
      init_idesc (cpu, & table[t->index], t);
    }

  /* Link the IDESC table into the cpu.  */
  CPU_IDESC (cpu) = table;
}

/* Given an instruction, return a pointer to its IDESC entry.  */

const IDESC *
crisv10f_decode (SIM_CPU *current_cpu, IADDR pc,
              CGEN_INSN_INT base_insn,
              ARGBUF *abuf)
{
  /* Result of decoder.  */
  CRISV10F_INSN_TYPE itype;

  {
    CGEN_INSN_INT insn = base_insn;

    {
      unsigned int val = (((insn >> 4) & (255 << 0)));
      switch (val)
      {
      case 0 : /* fall through */
      case 1 : /* fall through */
      case 2 : /* fall through */
      case 3 : /* fall through */
      case 4 : /* fall through */
      case 5 : /* fall through */
      case 6 : /* fall through */
      case 7 : /* fall through */
      case 8 : /* fall through */
      case 9 : /* fall through */
      case 10 : /* fall through */
      case 11 : /* fall through */
      case 12 : /* fall through */
      case 13 : /* fall through */
      case 14 : /* fall through */
      case 15 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 15 :
            if ((base_insn & 0xf00) == 0x0)
              { itype = CRISV10F_INSN_BCC_B; goto extract_sfmt_bcc_b; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 14 :
            if ((base_insn & 0xff00) == 0xe000)
              { itype = CRISV10F_INSN_BA_B; goto extract_sfmt_ba_b; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 16 : /* fall through */
      case 17 : /* fall through */
      case 18 : /* fall through */
      case 19 : /* fall through */
      case 20 : /* fall through */
      case 21 : /* fall through */
      case 22 : /* fall through */
      case 23 : /* fall through */
      case 24 : /* fall through */
      case 25 : /* fall through */
      case 26 : /* fall through */
      case 27 : /* fall through */
      case 28 : /* fall through */
      case 29 : /* fall through */
      case 30 : /* fall through */
      case 31 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xf00) == 0x100)
              { itype = CRISV10F_INSN_ADDOQ; goto extract_sfmt_addoq; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xff00) == 0xf100)
              { itype = CRISV10F_INSN_BDAPQPC; goto extract_sfmt_bdapqpc; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 32 : /* fall through */
      case 33 : /* fall through */
      case 34 : /* fall through */
      case 35 :
        if ((base_insn & 0xfc0) == 0x200)
          { itype = CRISV10F_INSN_ADDQ; goto extract_sfmt_addq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 36 : /* fall through */
      case 37 : /* fall through */
      case 38 : /* fall through */
      case 39 :
        if ((base_insn & 0xfc0) == 0x240)
          { itype = CRISV10F_INSN_MOVEQ; goto extract_sfmt_moveq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 40 : /* fall through */
      case 41 : /* fall through */
      case 42 : /* fall through */
      case 43 :
        if ((base_insn & 0xfc0) == 0x280)
          { itype = CRISV10F_INSN_SUBQ; goto extract_sfmt_addq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 44 : /* fall through */
      case 45 : /* fall through */
      case 46 : /* fall through */
      case 47 :
        if ((base_insn & 0xfc0) == 0x2c0)
          { itype = CRISV10F_INSN_CMPQ; goto extract_sfmt_cmpq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 48 : /* fall through */
      case 49 : /* fall through */
      case 50 : /* fall through */
      case 51 :
        if ((base_insn & 0xfc0) == 0x300)
          { itype = CRISV10F_INSN_ANDQ; goto extract_sfmt_andq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 52 : /* fall through */
      case 53 : /* fall through */
      case 54 : /* fall through */
      case 55 :
        if ((base_insn & 0xfc0) == 0x340)
          { itype = CRISV10F_INSN_ORQ; goto extract_sfmt_andq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 56 : /* fall through */
      case 57 :
        if ((base_insn & 0xfe0) == 0x380)
          { itype = CRISV10F_INSN_BTSTQ; goto extract_sfmt_btstq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 58 : /* fall through */
      case 59 :
        if ((base_insn & 0xfe0) == 0x3a0)
          { itype = CRISV10F_INSN_ASRQ; goto extract_sfmt_asrq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 60 : /* fall through */
      case 61 :
        if ((base_insn & 0xfe0) == 0x3c0)
          { itype = CRISV10F_INSN_LSLQ; goto extract_sfmt_asrq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 62 : /* fall through */
      case 63 :
        if ((base_insn & 0xfe0) == 0x3e0)
          { itype = CRISV10F_INSN_LSRQ; goto extract_sfmt_asrq; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 64 :
        if ((base_insn & 0xff0) == 0x400)
          { itype = CRISV10F_INSN_ADDU_B_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 65 :
        if ((base_insn & 0xff0) == 0x410)
          { itype = CRISV10F_INSN_ADDU_W_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 66 :
        if ((base_insn & 0xff0) == 0x420)
          { itype = CRISV10F_INSN_ADDS_B_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 67 :
        if ((base_insn & 0xff0) == 0x430)
          { itype = CRISV10F_INSN_ADDS_W_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 68 :
        if ((base_insn & 0xff0) == 0x440)
          { itype = CRISV10F_INSN_MOVU_B_R; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 69 :
        if ((base_insn & 0xff0) == 0x450)
          { itype = CRISV10F_INSN_MOVU_W_R; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 70 :
        if ((base_insn & 0xff0) == 0x460)
          { itype = CRISV10F_INSN_MOVS_B_R; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 71 :
        if ((base_insn & 0xff0) == 0x470)
          { itype = CRISV10F_INSN_MOVS_W_R; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 72 :
        if ((base_insn & 0xff0) == 0x480)
          { itype = CRISV10F_INSN_SUBU_B_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 73 :
        if ((base_insn & 0xff0) == 0x490)
          { itype = CRISV10F_INSN_SUBU_W_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 74 :
        if ((base_insn & 0xff0) == 0x4a0)
          { itype = CRISV10F_INSN_SUBS_B_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 75 :
        if ((base_insn & 0xff0) == 0x4b0)
          { itype = CRISV10F_INSN_SUBS_W_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 76 :
        if ((base_insn & 0xff0) == 0x4c0)
          { itype = CRISV10F_INSN_LSLR_B_R; goto extract_sfmt_lsrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 77 :
        if ((base_insn & 0xff0) == 0x4d0)
          { itype = CRISV10F_INSN_LSLR_W_R; goto extract_sfmt_lsrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 78 :
        if ((base_insn & 0xff0) == 0x4e0)
          { itype = CRISV10F_INSN_LSLR_D_R; goto extract_sfmt_lsrr_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 79 :
        if ((base_insn & 0xff0) == 0x4f0)
          { itype = CRISV10F_INSN_BTST; goto extract_sfmt_btst; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 80 :
        {
          unsigned int val = (((insn >> 8) & (7 << 4)) | ((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 : /* fall through */
          case 16 : /* fall through */
          case 17 : /* fall through */
          case 18 : /* fall through */
          case 19 : /* fall through */
          case 20 : /* fall through */
          case 21 : /* fall through */
          case 22 : /* fall through */
          case 23 : /* fall through */
          case 24 : /* fall through */
          case 25 : /* fall through */
          case 26 : /* fall through */
          case 27 : /* fall through */
          case 28 : /* fall through */
          case 29 : /* fall through */
          case 30 : /* fall through */
          case 31 : /* fall through */
          case 32 : /* fall through */
          case 33 : /* fall through */
          case 34 : /* fall through */
          case 35 : /* fall through */
          case 36 : /* fall through */
          case 37 : /* fall through */
          case 38 : /* fall through */
          case 39 : /* fall through */
          case 40 : /* fall through */
          case 41 : /* fall through */
          case 42 : /* fall through */
          case 43 : /* fall through */
          case 44 : /* fall through */
          case 45 : /* fall through */
          case 46 : /* fall through */
          case 47 : /* fall through */
          case 48 : /* fall through */
          case 49 : /* fall through */
          case 50 : /* fall through */
          case 51 : /* fall through */
          case 52 : /* fall through */
          case 53 : /* fall through */
          case 54 : /* fall through */
          case 55 : /* fall through */
          case 56 : /* fall through */
          case 57 : /* fall through */
          case 58 : /* fall through */
          case 59 : /* fall through */
          case 60 : /* fall through */
          case 61 : /* fall through */
          case 62 : /* fall through */
          case 63 : /* fall through */
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
          case 79 : /* fall through */
          case 80 : /* fall through */
          case 81 : /* fall through */
          case 82 : /* fall through */
          case 83 : /* fall through */
          case 84 : /* fall through */
          case 85 : /* fall through */
          case 86 : /* fall through */
          case 87 : /* fall through */
          case 88 : /* fall through */
          case 89 : /* fall through */
          case 90 : /* fall through */
          case 91 : /* fall through */
          case 92 : /* fall through */
          case 93 : /* fall through */
          case 94 : /* fall through */
          case 95 : /* fall through */
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
          case 111 : /* fall through */
          case 112 : /* fall through */
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
            if ((base_insn & 0xff0) == 0x500)
              { itype = CRISV10F_INSN_ADDI_B_R; goto extract_sfmt_addi_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            {
              unsigned int val = (((insn >> 15) & (1 << 0)));
              switch (val)
              {
              case 0 :
                if ((base_insn & 0xffff) == 0x50f)
                  { itype = CRISV10F_INSN_NOP; goto extract_sfmt_nop; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 1 :
                if ((base_insn & 0xff0) == 0x500)
                  { itype = CRISV10F_INSN_ADDI_B_R; goto extract_sfmt_addi_b_r; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 81 :
        if ((base_insn & 0xff0) == 0x510)
          { itype = CRISV10F_INSN_ADDI_W_R; goto extract_sfmt_addi_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 82 :
        if ((base_insn & 0xff0) == 0x520)
          { itype = CRISV10F_INSN_ADDI_D_R; goto extract_sfmt_addi_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 83 :
        if ((base_insn & 0xff0) == 0x530)
          { itype = CRISV10F_INSN_SCC; goto extract_sfmt_scc; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 84 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xff0) == 0x540)
              { itype = CRISV10F_INSN_ADDI_ACR_B_R; goto extract_sfmt_addi_acr_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0x54f)
              { itype = CRISV10F_INSN_BIAP_PC_B_R; goto extract_sfmt_biap_pc_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 85 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xff0) == 0x550)
              { itype = CRISV10F_INSN_ADDI_ACR_W_R; goto extract_sfmt_addi_acr_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0x55f)
              { itype = CRISV10F_INSN_BIAP_PC_W_R; goto extract_sfmt_biap_pc_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 86 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xff0) == 0x560)
              { itype = CRISV10F_INSN_ADDI_ACR_D_R; goto extract_sfmt_addi_acr_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0x56f)
              { itype = CRISV10F_INSN_BIAP_PC_D_R; goto extract_sfmt_biap_pc_b_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 88 :
        if ((base_insn & 0xff0) == 0x580)
          { itype = CRISV10F_INSN_NEG_B_R; goto extract_sfmt_neg_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 89 :
        if ((base_insn & 0xff0) == 0x590)
          { itype = CRISV10F_INSN_NEG_W_R; goto extract_sfmt_neg_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 90 :
        if ((base_insn & 0xff0) == 0x5a0)
          { itype = CRISV10F_INSN_NEG_D_R; goto extract_sfmt_neg_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 91 :
        if ((base_insn & 0xff0) == 0x5b0)
          { itype = CRISV10F_INSN_SETF; goto extract_sfmt_setf; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 92 :
        if ((base_insn & 0xff0) == 0x5c0)
          { itype = CRISV10F_INSN_BOUND_R_B_R; goto extract_sfmt_dstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 93 :
        if ((base_insn & 0xff0) == 0x5d0)
          { itype = CRISV10F_INSN_BOUND_R_W_R; goto extract_sfmt_dstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 94 :
        if ((base_insn & 0xff0) == 0x5e0)
          { itype = CRISV10F_INSN_BOUND_R_D_R; goto extract_sfmt_dstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 95 :
        if ((base_insn & 0xff0) == 0x5f0)
          { itype = CRISV10F_INSN_CLEARF; goto extract_sfmt_setf; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 96 :
        if ((base_insn & 0xff0) == 0x600)
          { itype = CRISV10F_INSN_ADD_B_R; goto extract_sfmt_add_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 97 :
        if ((base_insn & 0xff0) == 0x610)
          { itype = CRISV10F_INSN_ADD_W_R; goto extract_sfmt_add_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 98 :
        if ((base_insn & 0xff0) == 0x620)
          { itype = CRISV10F_INSN_ADD_D_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 99 :
        if ((base_insn & 0xff0) == 0x630)
          { itype = CRISV10F_INSN_MOVE_R_SPRV10; goto extract_sfmt_move_r_sprv10; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 100 :
        if ((base_insn & 0xff0) == 0x640)
          { itype = CRISV10F_INSN_MOVE_B_R; goto extract_sfmt_move_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 101 :
        if ((base_insn & 0xff0) == 0x650)
          { itype = CRISV10F_INSN_MOVE_W_R; goto extract_sfmt_move_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 102 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xff0) == 0x660)
              { itype = CRISV10F_INSN_MOVE_D_R; goto extract_sfmt_move_d_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0x66f)
              { itype = CRISV10F_INSN_MOVEPCR; goto extract_sfmt_movepcr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 103 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xff0) == 0x670)
              { itype = CRISV10F_INSN_MOVE_SPR_RV10; goto extract_sfmt_move_spr_rv10; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0x67f)
              { itype = CRISV10F_INSN_RET_TYPE; goto extract_sfmt_ret_type; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 104 :
        if ((base_insn & 0xff0) == 0x680)
          { itype = CRISV10F_INSN_SUB_B_R; goto extract_sfmt_add_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 105 :
        if ((base_insn & 0xff0) == 0x690)
          { itype = CRISV10F_INSN_SUB_W_R; goto extract_sfmt_add_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 106 :
        if ((base_insn & 0xff0) == 0x6a0)
          { itype = CRISV10F_INSN_SUB_D_R; goto extract_sfmt_add_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 107 :
        if ((base_insn & 0xff0) == 0x6b0)
          { itype = CRISV10F_INSN_ABS; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 108 :
        if ((base_insn & 0xff0) == 0x6c0)
          { itype = CRISV10F_INSN_CMP_R_B_R; goto extract_sfmt_cmp_r_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 109 :
        if ((base_insn & 0xff0) == 0x6d0)
          { itype = CRISV10F_INSN_CMP_R_W_R; goto extract_sfmt_cmp_r_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 110 :
        if ((base_insn & 0xff0) == 0x6e0)
          { itype = CRISV10F_INSN_CMP_R_D_R; goto extract_sfmt_cmp_r_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 111 :
        if ((base_insn & 0xff0) == 0x6f0)
          { itype = CRISV10F_INSN_DSTEP; goto extract_sfmt_dstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 112 :
        if ((base_insn & 0xff0) == 0x700)
          { itype = CRISV10F_INSN_AND_B_R; goto extract_sfmt_and_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 113 :
        if ((base_insn & 0xff0) == 0x710)
          { itype = CRISV10F_INSN_AND_W_R; goto extract_sfmt_and_w_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 114 :
        if ((base_insn & 0xff0) == 0x720)
          { itype = CRISV10F_INSN_AND_D_R; goto extract_sfmt_and_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 115 :
        if ((base_insn & 0xff0) == 0x730)
          { itype = CRISV10F_INSN_LZ; goto extract_sfmt_movs_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 116 :
        if ((base_insn & 0xff0) == 0x740)
          { itype = CRISV10F_INSN_ORR_B_R; goto extract_sfmt_and_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 117 :
        if ((base_insn & 0xff0) == 0x750)
          { itype = CRISV10F_INSN_ORR_W_R; goto extract_sfmt_and_w_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 118 :
        if ((base_insn & 0xff0) == 0x760)
          { itype = CRISV10F_INSN_ORR_D_R; goto extract_sfmt_and_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 119 :
        if ((base_insn & 0xff0) == 0x770)
          { itype = CRISV10F_INSN_SWAP; goto extract_sfmt_swap; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 120 :
        if ((base_insn & 0xff0) == 0x780)
          { itype = CRISV10F_INSN_ASRR_B_R; goto extract_sfmt_asrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 121 :
        if ((base_insn & 0xff0) == 0x790)
          { itype = CRISV10F_INSN_ASRR_W_R; goto extract_sfmt_asrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 122 :
        if ((base_insn & 0xff0) == 0x7a0)
          { itype = CRISV10F_INSN_ASRR_D_R; goto extract_sfmt_and_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 123 :
        if ((base_insn & 0xff0) == 0x7b0)
          { itype = CRISV10F_INSN_XOR; goto extract_sfmt_dstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 124 :
        if ((base_insn & 0xff0) == 0x7c0)
          { itype = CRISV10F_INSN_LSRR_B_R; goto extract_sfmt_lsrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 125 :
        if ((base_insn & 0xff0) == 0x7d0)
          { itype = CRISV10F_INSN_LSRR_W_R; goto extract_sfmt_lsrr_b_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 126 :
        if ((base_insn & 0xff0) == 0x7e0)
          { itype = CRISV10F_INSN_LSRR_D_R; goto extract_sfmt_lsrr_d_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 127 :
        if ((base_insn & 0xff0) == 0x7f0)
          { itype = CRISV10F_INSN_MSTEP; goto extract_sfmt_mstep; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 128 :
        if ((base_insn & 0xbf0) == 0x800)
          { itype = CRISV10F_INSN_ADDU_M_B_M; goto extract_sfmt_adds_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 129 :
        if ((base_insn & 0xbf0) == 0x810)
          { itype = CRISV10F_INSN_ADDU_M_W_M; goto extract_sfmt_adds_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 130 :
        if ((base_insn & 0xbf0) == 0x820)
          { itype = CRISV10F_INSN_ADDS_M_B_M; goto extract_sfmt_adds_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 131 :
        {
          unsigned int val = (((insn >> 8) & (7 << 4)) | ((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 : /* fall through */
          case 15 : /* fall through */
          case 16 : /* fall through */
          case 17 : /* fall through */
          case 18 : /* fall through */
          case 19 : /* fall through */
          case 20 : /* fall through */
          case 21 : /* fall through */
          case 22 : /* fall through */
          case 23 : /* fall through */
          case 24 : /* fall through */
          case 25 : /* fall through */
          case 26 : /* fall through */
          case 27 : /* fall through */
          case 28 : /* fall through */
          case 29 : /* fall through */
          case 30 : /* fall through */
          case 31 : /* fall through */
          case 32 : /* fall through */
          case 33 : /* fall through */
          case 34 : /* fall through */
          case 35 : /* fall through */
          case 36 : /* fall through */
          case 37 : /* fall through */
          case 38 : /* fall through */
          case 39 : /* fall through */
          case 40 : /* fall through */
          case 41 : /* fall through */
          case 42 : /* fall through */
          case 43 : /* fall through */
          case 44 : /* fall through */
          case 45 : /* fall through */
          case 46 : /* fall through */
          case 47 : /* fall through */
          case 48 : /* fall through */
          case 49 : /* fall through */
          case 50 : /* fall through */
          case 51 : /* fall through */
          case 52 : /* fall through */
          case 53 : /* fall through */
          case 54 : /* fall through */
          case 55 : /* fall through */
          case 56 : /* fall through */
          case 57 : /* fall through */
          case 58 : /* fall through */
          case 59 : /* fall through */
          case 60 : /* fall through */
          case 61 : /* fall through */
          case 62 : /* fall through */
          case 63 : /* fall through */
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
          case 79 : /* fall through */
          case 80 : /* fall through */
          case 81 : /* fall through */
          case 82 : /* fall through */
          case 83 : /* fall through */
          case 84 : /* fall through */
          case 85 : /* fall through */
          case 86 : /* fall through */
          case 87 : /* fall through */
          case 88 : /* fall through */
          case 89 : /* fall through */
          case 90 : /* fall through */
          case 91 : /* fall through */
          case 92 : /* fall through */
          case 93 : /* fall through */
          case 94 : /* fall through */
          case 95 : /* fall through */
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
          case 111 : /* fall through */
          case 112 : /* fall through */
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
          case 126 :
            if ((base_insn & 0xbf0) == 0x830)
              { itype = CRISV10F_INSN_ADDS_M_W_M; goto extract_sfmt_adds_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 127 :
            {
              unsigned int val = (((insn >> 15) & (1 << 0)));
              switch (val)
              {
              case 0 :
                if ((base_insn & 0xbf0) == 0x830)
                  { itype = CRISV10F_INSN_ADDS_M_W_M; goto extract_sfmt_adds_m_w_m; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 1 :
                if ((base_insn & 0xffff) == 0xf83f)
                  { itype = CRISV10F_INSN_ADDSPCPC; goto extract_sfmt_addspcpc; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 132 :
        if ((base_insn & 0xbf0) == 0x840)
          { itype = CRISV10F_INSN_MOVU_M_B_M; goto extract_sfmt_movs_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 133 :
        if ((base_insn & 0xbf0) == 0x850)
          { itype = CRISV10F_INSN_MOVU_M_W_M; goto extract_sfmt_movs_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 134 :
        if ((base_insn & 0xbf0) == 0x860)
          { itype = CRISV10F_INSN_MOVS_M_B_M; goto extract_sfmt_movs_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 135 :
        if ((base_insn & 0xbf0) == 0x870)
          { itype = CRISV10F_INSN_MOVS_M_W_M; goto extract_sfmt_movs_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 136 :
        if ((base_insn & 0xbf0) == 0x880)
          { itype = CRISV10F_INSN_SUBU_M_B_M; goto extract_sfmt_adds_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 137 :
        if ((base_insn & 0xbf0) == 0x890)
          { itype = CRISV10F_INSN_SUBU_M_W_M; goto extract_sfmt_adds_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 138 :
        if ((base_insn & 0xbf0) == 0x8a0)
          { itype = CRISV10F_INSN_SUBS_M_B_M; goto extract_sfmt_adds_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 139 :
        if ((base_insn & 0xbf0) == 0x8b0)
          { itype = CRISV10F_INSN_SUBS_M_W_M; goto extract_sfmt_adds_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 140 :
        if ((base_insn & 0xbf0) == 0x8c0)
          { itype = CRISV10F_INSN_CMPU_M_B_M; goto extract_sfmt_cmp_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 141 :
        if ((base_insn & 0xbf0) == 0x8d0)
          { itype = CRISV10F_INSN_CMPU_M_W_M; goto extract_sfmt_cmp_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 142 :
        if ((base_insn & 0xbf0) == 0x8e0)
          { itype = CRISV10F_INSN_CMPS_M_B_M; goto extract_sfmt_cmp_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 143 :
        if ((base_insn & 0xbf0) == 0x8f0)
          { itype = CRISV10F_INSN_CMPS_M_W_M; goto extract_sfmt_cmp_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 144 :
        if ((base_insn & 0xff0) == 0x900)
          { itype = CRISV10F_INSN_MULU_B; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 145 :
        if ((base_insn & 0xff0) == 0x910)
          { itype = CRISV10F_INSN_MULU_W; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 146 :
        if ((base_insn & 0xff0) == 0x920)
          { itype = CRISV10F_INSN_MULU_D; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 147 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 15 :
            if ((base_insn & 0xbf0) == 0x930)
              { itype = CRISV10F_INSN_JUMP_M; goto extract_sfmt_jump_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 14 :
            if ((base_insn & 0xfff0) == 0xe930)
              { itype = CRISV10F_INSN_BREAK; goto extract_sfmt_break; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 148 :
        if ((base_insn & 0xbf0) == 0x940)
          { itype = CRISV10F_INSN_ADDO_M_B_M; goto extract_sfmt_addo_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 149 :
        if ((base_insn & 0xbf0) == 0x950)
          { itype = CRISV10F_INSN_ADDO_M_W_M; goto extract_sfmt_addo_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 150 :
        if ((base_insn & 0xbf0) == 0x960)
          { itype = CRISV10F_INSN_ADDO_M_D_M; goto extract_sfmt_addo_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 151 :
        if ((base_insn & 0xfbf0) == 0x970)
          { itype = CRISV10F_INSN_DIP_M; goto extract_sfmt_dip_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 155 :
        if ((base_insn & 0xff0) == 0x9b0)
          { itype = CRISV10F_INSN_JUMP_R; goto extract_sfmt_jump_r; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 156 :
        if ((base_insn & 0xbf0) == 0x9c0)
          { itype = CRISV10F_INSN_BOUND_M_B_M; goto extract_sfmt_bound_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 157 :
        if ((base_insn & 0xbf0) == 0x9d0)
          { itype = CRISV10F_INSN_BOUND_M_W_M; goto extract_sfmt_bound_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 158 :
        if ((base_insn & 0xbf0) == 0x9e0)
          { itype = CRISV10F_INSN_BOUND_M_D_M; goto extract_sfmt_bound_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 160 :
        if ((base_insn & 0xbf0) == 0xa00)
          { itype = CRISV10F_INSN_ADD_M_B_M; goto extract_sfmt_add_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 161 :
        if ((base_insn & 0xbf0) == 0xa10)
          { itype = CRISV10F_INSN_ADD_M_W_M; goto extract_sfmt_add_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 162 :
        if ((base_insn & 0xbf0) == 0xa20)
          { itype = CRISV10F_INSN_ADD_M_D_M; goto extract_sfmt_add_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 163 :
        if ((base_insn & 0xbf0) == 0xa30)
          { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 164 :
        if ((base_insn & 0xbf0) == 0xa40)
          { itype = CRISV10F_INSN_MOVE_M_B_M; goto extract_sfmt_move_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 165 :
        if ((base_insn & 0xbf0) == 0xa50)
          { itype = CRISV10F_INSN_MOVE_M_W_M; goto extract_sfmt_move_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 166 :
        if ((base_insn & 0xbf0) == 0xa60)
          { itype = CRISV10F_INSN_MOVE_M_D_M; goto extract_sfmt_move_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 167 : /* fall through */
      case 231 :
        if ((base_insn & 0xbf0) == 0xa70)
          { itype = CRISV10F_INSN_MOVE_SPR_MV10; goto extract_sfmt_move_spr_mv10; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 168 :
        if ((base_insn & 0xbf0) == 0xa80)
          { itype = CRISV10F_INSN_SUB_M_B_M; goto extract_sfmt_add_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 169 :
        if ((base_insn & 0xbf0) == 0xa90)
          { itype = CRISV10F_INSN_SUB_M_W_M; goto extract_sfmt_add_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 170 :
        if ((base_insn & 0xbf0) == 0xaa0)
          { itype = CRISV10F_INSN_SUB_M_D_M; goto extract_sfmt_add_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 172 :
        if ((base_insn & 0xbf0) == 0xac0)
          { itype = CRISV10F_INSN_CMP_M_B_M; goto extract_sfmt_cmp_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 173 :
        if ((base_insn & 0xbf0) == 0xad0)
          { itype = CRISV10F_INSN_CMP_M_W_M; goto extract_sfmt_cmp_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 174 :
        if ((base_insn & 0xbf0) == 0xae0)
          { itype = CRISV10F_INSN_CMP_M_D_M; goto extract_sfmt_cmp_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 176 :
        if ((base_insn & 0xbf0) == 0xb00)
          { itype = CRISV10F_INSN_AND_M_B_M; goto extract_sfmt_and_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 177 :
        if ((base_insn & 0xbf0) == 0xb10)
          { itype = CRISV10F_INSN_AND_M_W_M; goto extract_sfmt_and_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 178 :
        if ((base_insn & 0xbf0) == 0xb20)
          { itype = CRISV10F_INSN_AND_M_D_M; goto extract_sfmt_and_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 180 :
        if ((base_insn & 0xbf0) == 0xb40)
          { itype = CRISV10F_INSN_OR_M_B_M; goto extract_sfmt_and_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 181 :
        if ((base_insn & 0xbf0) == 0xb50)
          { itype = CRISV10F_INSN_OR_M_W_M; goto extract_sfmt_and_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 182 :
        if ((base_insn & 0xbf0) == 0xb60)
          { itype = CRISV10F_INSN_OR_M_D_M; goto extract_sfmt_and_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 183 : /* fall through */
      case 247 :
        if ((base_insn & 0xfbf0) == 0x3b70)
          { itype = CRISV10F_INSN_SBFS; goto extract_sfmt_sbfs; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 184 : /* fall through */
      case 248 :
        if ((base_insn & 0xfbf0) == 0xb80)
          { itype = CRISV10F_INSN_TEST_M_B_M; goto extract_sfmt_test_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 185 : /* fall through */
      case 249 :
        if ((base_insn & 0xfbf0) == 0xb90)
          { itype = CRISV10F_INSN_TEST_M_W_M; goto extract_sfmt_test_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 186 : /* fall through */
      case 250 :
        if ((base_insn & 0xfbf0) == 0xba0)
          { itype = CRISV10F_INSN_TEST_M_D_M; goto extract_sfmt_test_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 187 : /* fall through */
      case 251 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xbb0)
              { itype = CRISV10F_INSN_MOVEM_M_R; goto extract_sfmt_movem_m_r; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfbf0) == 0xfbb0)
              { itype = CRISV10F_INSN_MOVEM_M_PC; goto extract_sfmt_movem_m_pc; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 188 : /* fall through */
      case 252 :
        if ((base_insn & 0xbf0) == 0xbc0)
          { itype = CRISV10F_INSN_MOVE_R_M_B_M; goto extract_sfmt_move_r_m_b_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 189 : /* fall through */
      case 253 :
        if ((base_insn & 0xbf0) == 0xbd0)
          { itype = CRISV10F_INSN_MOVE_R_M_W_M; goto extract_sfmt_move_r_m_w_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 190 : /* fall through */
      case 254 :
        if ((base_insn & 0xbf0) == 0xbe0)
          { itype = CRISV10F_INSN_MOVE_R_M_D_M; goto extract_sfmt_move_r_m_d_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 191 : /* fall through */
      case 255 :
        if ((base_insn & 0xbf0) == 0xbf0)
          { itype = CRISV10F_INSN_MOVEM_R_M; goto extract_sfmt_movem_r_m; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 192 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x800)
              { itype = CRISV10F_INSN_ADDU_M_B_M; goto extract_sfmt_adds_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc0f)
              { itype = CRISV10F_INSN_ADDUCBR; goto extract_sfmt_addscbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 193 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x810)
              { itype = CRISV10F_INSN_ADDU_M_W_M; goto extract_sfmt_adds_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc1f)
              { itype = CRISV10F_INSN_ADDUCWR; goto extract_sfmt_addscwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 194 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x820)
              { itype = CRISV10F_INSN_ADDS_M_B_M; goto extract_sfmt_adds_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc2f)
              { itype = CRISV10F_INSN_ADDSCBR; goto extract_sfmt_addscbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 195 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x830)
              { itype = CRISV10F_INSN_ADDS_M_W_M; goto extract_sfmt_adds_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc3f)
              { itype = CRISV10F_INSN_ADDSCWR; goto extract_sfmt_addscwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 196 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x840)
              { itype = CRISV10F_INSN_MOVU_M_B_M; goto extract_sfmt_movs_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc4f)
              { itype = CRISV10F_INSN_MOVUCBR; goto extract_sfmt_movucbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 197 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x850)
              { itype = CRISV10F_INSN_MOVU_M_W_M; goto extract_sfmt_movs_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc5f)
              { itype = CRISV10F_INSN_MOVUCWR; goto extract_sfmt_movucwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 198 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x860)
              { itype = CRISV10F_INSN_MOVS_M_B_M; goto extract_sfmt_movs_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc6f)
              { itype = CRISV10F_INSN_MOVSCBR; goto extract_sfmt_movscbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 199 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x870)
              { itype = CRISV10F_INSN_MOVS_M_W_M; goto extract_sfmt_movs_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc7f)
              { itype = CRISV10F_INSN_MOVSCWR; goto extract_sfmt_movscwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 200 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x880)
              { itype = CRISV10F_INSN_SUBU_M_B_M; goto extract_sfmt_adds_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc8f)
              { itype = CRISV10F_INSN_SUBUCBR; goto extract_sfmt_addscbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 201 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x890)
              { itype = CRISV10F_INSN_SUBU_M_W_M; goto extract_sfmt_adds_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xc9f)
              { itype = CRISV10F_INSN_SUBUCWR; goto extract_sfmt_addscwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 202 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8a0)
              { itype = CRISV10F_INSN_SUBS_M_B_M; goto extract_sfmt_adds_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xcaf)
              { itype = CRISV10F_INSN_SUBSCBR; goto extract_sfmt_addscbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 203 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8b0)
              { itype = CRISV10F_INSN_SUBS_M_W_M; goto extract_sfmt_adds_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xcbf)
              { itype = CRISV10F_INSN_SUBSCWR; goto extract_sfmt_addscwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 204 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8c0)
              { itype = CRISV10F_INSN_CMPU_M_B_M; goto extract_sfmt_cmp_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xccf)
              { itype = CRISV10F_INSN_CMPUCBR; goto extract_sfmt_cmpucbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 205 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8d0)
              { itype = CRISV10F_INSN_CMPU_M_W_M; goto extract_sfmt_cmp_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xcdf)
              { itype = CRISV10F_INSN_CMPUCWR; goto extract_sfmt_cmpucwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 206 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8e0)
              { itype = CRISV10F_INSN_CMPS_M_B_M; goto extract_sfmt_cmp_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xcef)
              { itype = CRISV10F_INSN_CMPSCBR; goto extract_sfmt_cmpcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 207 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x8f0)
              { itype = CRISV10F_INSN_CMPS_M_W_M; goto extract_sfmt_cmp_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xcff)
              { itype = CRISV10F_INSN_CMPSCWR; goto extract_sfmt_cmpcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 208 :
        if ((base_insn & 0xff0) == 0xd00)
          { itype = CRISV10F_INSN_MULS_B; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 209 :
        if ((base_insn & 0xff0) == 0xd10)
          { itype = CRISV10F_INSN_MULS_W; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 210 :
        if ((base_insn & 0xff0) == 0xd20)
          { itype = CRISV10F_INSN_MULS_D; goto extract_sfmt_muls_b; }
        itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      case 211 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x930)
              { itype = CRISV10F_INSN_JUMP_M; goto extract_sfmt_jump_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xd3f)
              { itype = CRISV10F_INSN_JUMP_C; goto extract_sfmt_jump_c; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 212 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x940)
              { itype = CRISV10F_INSN_ADDO_M_B_M; goto extract_sfmt_addo_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xd4f)
              { itype = CRISV10F_INSN_ADDO_CB; goto extract_sfmt_addo_cb; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 213 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x950)
              { itype = CRISV10F_INSN_ADDO_M_W_M; goto extract_sfmt_addo_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xd5f)
              { itype = CRISV10F_INSN_ADDO_CW; goto extract_sfmt_addo_cw; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 214 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            {
              unsigned int val = (((insn >> 0) & (15 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 : /* fall through */
              case 7 : /* fall through */
              case 8 : /* fall through */
              case 9 : /* fall through */
              case 10 : /* fall through */
              case 11 : /* fall through */
              case 12 : /* fall through */
              case 13 : /* fall through */
              case 14 :
                if ((base_insn & 0xbf0) == 0x960)
                  { itype = CRISV10F_INSN_ADDO_M_D_M; goto extract_sfmt_addo_m_d_m; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 15 :
                if ((base_insn & 0xfff) == 0xd6f)
                  { itype = CRISV10F_INSN_ADDO_CD; goto extract_sfmt_addo_cd; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 15 :
            {
              unsigned int val = (((insn >> 0) & (15 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 : /* fall through */
              case 7 : /* fall through */
              case 8 : /* fall through */
              case 9 : /* fall through */
              case 10 : /* fall through */
              case 11 : /* fall through */
              case 12 : /* fall through */
              case 13 : /* fall through */
              case 14 :
                if ((base_insn & 0xbf0) == 0x960)
                  { itype = CRISV10F_INSN_ADDO_M_D_M; goto extract_sfmt_addo_m_d_m; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 15 :
                if ((base_insn & 0xffff) == 0xfd6f)
                  { itype = CRISV10F_INSN_BDAP_32_PC; goto extract_sfmt_bdap_32_pc; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 215 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xfbf0) == 0x970)
              { itype = CRISV10F_INSN_DIP_M; goto extract_sfmt_dip_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xffff) == 0xd7f)
              { itype = CRISV10F_INSN_DIP_C; goto extract_sfmt_dip_c; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 220 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x9c0)
              { itype = CRISV10F_INSN_BOUND_M_B_M; goto extract_sfmt_bound_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xdcf)
              { itype = CRISV10F_INSN_BOUND_CB; goto extract_sfmt_bound_cb; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 221 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x9d0)
              { itype = CRISV10F_INSN_BOUND_M_W_M; goto extract_sfmt_bound_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xddf)
              { itype = CRISV10F_INSN_BOUND_CW; goto extract_sfmt_bound_cw; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 222 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0x9e0)
              { itype = CRISV10F_INSN_BOUND_M_D_M; goto extract_sfmt_bound_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xdef)
              { itype = CRISV10F_INSN_BOUND_CD; goto extract_sfmt_bound_cd; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 223 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 15 :
            if ((base_insn & 0xfff) == 0xdff)
              { itype = CRISV10F_INSN_BCC_W; goto extract_sfmt_bcc_w; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 14 :
            if ((base_insn & 0xffff) == 0xedff)
              { itype = CRISV10F_INSN_BA_W; goto extract_sfmt_ba_w; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 224 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa00)
              { itype = CRISV10F_INSN_ADD_M_B_M; goto extract_sfmt_add_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe0f)
              { itype = CRISV10F_INSN_ADDCBR; goto extract_sfmt_addcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 225 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa10)
              { itype = CRISV10F_INSN_ADD_M_W_M; goto extract_sfmt_add_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe1f)
              { itype = CRISV10F_INSN_ADDCWR; goto extract_sfmt_addcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 226 :
        {
          unsigned int val = (((insn >> 12) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            {
              unsigned int val = (((insn >> 0) & (15 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 : /* fall through */
              case 7 : /* fall through */
              case 8 : /* fall through */
              case 9 : /* fall through */
              case 10 : /* fall through */
              case 11 : /* fall through */
              case 12 : /* fall through */
              case 13 : /* fall through */
              case 14 :
                if ((base_insn & 0xbf0) == 0xa20)
                  { itype = CRISV10F_INSN_ADD_M_D_M; goto extract_sfmt_add_m_d_m; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 15 :
                if ((base_insn & 0xfff) == 0xe2f)
                  { itype = CRISV10F_INSN_ADDCDR; goto extract_sfmt_addcdr; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 15 :
            {
              unsigned int val = (((insn >> 0) & (15 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 : /* fall through */
              case 7 : /* fall through */
              case 8 : /* fall through */
              case 9 : /* fall through */
              case 10 : /* fall through */
              case 11 : /* fall through */
              case 12 : /* fall through */
              case 13 : /* fall through */
              case 14 :
                if ((base_insn & 0xbf0) == 0xa20)
                  { itype = CRISV10F_INSN_ADD_M_D_M; goto extract_sfmt_add_m_d_m; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 15 :
                if ((base_insn & 0xffff) == 0xfe2f)
                  { itype = CRISV10F_INSN_ADDCPC; goto extract_sfmt_addcpc; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 227 :
        {
          unsigned int val = (((insn >> 11) & (15 << 1)) | ((insn >> 0) & (1 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 : /* fall through */
          case 17 : /* fall through */
          case 18 : /* fall through */
          case 20 : /* fall through */
          case 22 : /* fall through */
          case 24 : /* fall through */
          case 26 : /* fall through */
          case 28 : /* fall through */
          case 30 :
            if ((base_insn & 0xbf0) == 0xa30)
              { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xe3f)
                  { itype = CRISV10F_INSN_MOVE_M_PCPLUS_P0; goto extract_sfmt_move_m_pcplus_p0; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 11 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0x5e3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P5; goto extract_sfmt_move_c_sprv10_p5; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 15 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0x7e3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P7; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 16 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0x8e3e)
                  { itype = CRISV10F_INSN_MOVE_M_SPPLUS_P8; goto extract_sfmt_move_m_spplus_p8; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 19 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0x9e3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P9; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 21 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xae3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P10; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 23 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xbe3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P11; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 25 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xce3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P12; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 27 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xde3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P13; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 29 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xee3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P14; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 31 :
            {
              unsigned int val = (((insn >> 1) & (7 << 0)));
              switch (val)
              {
              case 0 : /* fall through */
              case 1 : /* fall through */
              case 2 : /* fall through */
              case 3 : /* fall through */
              case 4 : /* fall through */
              case 5 : /* fall through */
              case 6 :
                if ((base_insn & 0xbf0) == 0xa30)
                  { itype = CRISV10F_INSN_MOVE_M_SPRV10; goto extract_sfmt_move_m_sprv10; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              case 7 :
                if ((base_insn & 0xffff) == 0xfe3f)
                  { itype = CRISV10F_INSN_MOVE_C_SPRV10_P15; goto extract_sfmt_move_c_sprv10_p9; }
                itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 228 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa40)
              { itype = CRISV10F_INSN_MOVE_M_B_M; goto extract_sfmt_move_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe4f)
              { itype = CRISV10F_INSN_MOVECBR; goto extract_sfmt_movecbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 229 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa50)
              { itype = CRISV10F_INSN_MOVE_M_W_M; goto extract_sfmt_move_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe5f)
              { itype = CRISV10F_INSN_MOVECWR; goto extract_sfmt_movecwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 230 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa60)
              { itype = CRISV10F_INSN_MOVE_M_D_M; goto extract_sfmt_move_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe6f)
              { itype = CRISV10F_INSN_MOVECDR; goto extract_sfmt_movecdr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 232 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa80)
              { itype = CRISV10F_INSN_SUB_M_B_M; goto extract_sfmt_add_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe8f)
              { itype = CRISV10F_INSN_SUBCBR; goto extract_sfmt_addcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 233 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xa90)
              { itype = CRISV10F_INSN_SUB_M_W_M; goto extract_sfmt_add_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xe9f)
              { itype = CRISV10F_INSN_SUBCWR; goto extract_sfmt_addcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 234 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xaa0)
              { itype = CRISV10F_INSN_SUB_M_D_M; goto extract_sfmt_add_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xeaf)
              { itype = CRISV10F_INSN_SUBCDR; goto extract_sfmt_addcdr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 236 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xac0)
              { itype = CRISV10F_INSN_CMP_M_B_M; goto extract_sfmt_cmp_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xecf)
              { itype = CRISV10F_INSN_CMPCBR; goto extract_sfmt_cmpcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 237 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xad0)
              { itype = CRISV10F_INSN_CMP_M_W_M; goto extract_sfmt_cmp_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xedf)
              { itype = CRISV10F_INSN_CMPCWR; goto extract_sfmt_cmpcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 238 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xae0)
              { itype = CRISV10F_INSN_CMP_M_D_M; goto extract_sfmt_cmp_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xeef)
              { itype = CRISV10F_INSN_CMPCDR; goto extract_sfmt_cmpcdr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 240 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb00)
              { itype = CRISV10F_INSN_AND_M_B_M; goto extract_sfmt_and_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf0f)
              { itype = CRISV10F_INSN_ANDCBR; goto extract_sfmt_andcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 241 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb10)
              { itype = CRISV10F_INSN_AND_M_W_M; goto extract_sfmt_and_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf1f)
              { itype = CRISV10F_INSN_ANDCWR; goto extract_sfmt_andcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 242 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb20)
              { itype = CRISV10F_INSN_AND_M_D_M; goto extract_sfmt_and_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf2f)
              { itype = CRISV10F_INSN_ANDCDR; goto extract_sfmt_andcdr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 244 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb40)
              { itype = CRISV10F_INSN_OR_M_B_M; goto extract_sfmt_and_m_b_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf4f)
              { itype = CRISV10F_INSN_ORCBR; goto extract_sfmt_andcbr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 245 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb50)
              { itype = CRISV10F_INSN_OR_M_W_M; goto extract_sfmt_and_m_w_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf5f)
              { itype = CRISV10F_INSN_ORCWR; goto extract_sfmt_andcwr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 246 :
        {
          unsigned int val = (((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 1 : /* fall through */
          case 2 : /* fall through */
          case 3 : /* fall through */
          case 4 : /* fall through */
          case 5 : /* fall through */
          case 6 : /* fall through */
          case 7 : /* fall through */
          case 8 : /* fall through */
          case 9 : /* fall through */
          case 10 : /* fall through */
          case 11 : /* fall through */
          case 12 : /* fall through */
          case 13 : /* fall through */
          case 14 :
            if ((base_insn & 0xbf0) == 0xb60)
              { itype = CRISV10F_INSN_OR_M_D_M; goto extract_sfmt_and_m_d_m; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((base_insn & 0xfff) == 0xf6f)
              { itype = CRISV10F_INSN_ORCDR; goto extract_sfmt_andcdr; }
            itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      default : itype = CRISV10F_INSN_X_INVALID; goto extract_sfmt_empty;
      }
    }
  }

  /* The instruction has been decoded, now extract the fields.  */

 extract_sfmt_empty:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_empty", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_nop:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_nop", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_move_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_b_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_d_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_d_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movepcr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_moveq.f
    UINT f_operand2;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movepcr", "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_moveq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_moveq.f
    UINT f_operand2;
    INT f_s6;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_s6 = EXTRACT_LSB0_INT (insn, 16, 5, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_s6) = f_s6;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_moveq", "f_s6 0x%x", 'x', f_s6, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movs_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_muls_b.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movs_b_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movecbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcbr.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movecbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movecwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcwr.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movecwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movecdr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cd.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movecdr", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movscbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    UINT f_operand2;
    INT f_indir_pc__byte;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movscbr", "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movscwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    UINT f_operand2;
    INT f_indir_pc__word;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__word) = f_indir_pc__word;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movscwr", "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movucbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    UINT f_operand2;
    INT f_indir_pc__byte;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movucbr", "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movucwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    UINT f_operand2;
    INT f_indir_pc__word;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__word) = f_indir_pc__word;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movucwr", "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addq.f
    UINT f_operand2;
    UINT f_u6;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_u6 = EXTRACT_LSB0_UINT (insn, 16, 5, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_u6) = f_u6;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addq", "f_operand2 0x%x", 'x', f_operand2, "f_u6 0x%x", 'x', f_u6, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmp_r_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmp_r_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmp_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmp_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmp_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmp_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmp_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmp_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpcbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpcbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpcwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpcwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpcdr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cd.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpcdr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_andq.f
    UINT f_operand2;
    INT f_s6;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_s6 = EXTRACT_LSB0_INT (insn, 16, 5, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_s6) = f_s6;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpq", "f_operand2 0x%x", 'x', f_operand2, "f_s6 0x%x", 'x', f_s6, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpucbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpucbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpucwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpucwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_b_m", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_w_m", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_d_m", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movs_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movs_m_b_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movs_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movs_m_w_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_r_sprv10:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_r_sprv10", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Pd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_spr_rv10:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_rv10.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_spr_rv10", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Ps) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rs) = FLD (f_operand1);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ret_type:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_rv10.f
    UINT f_operand2;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ret_type", "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Ps) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_sprv10:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_sprv10", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Pd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_c_sprv10_p5:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p5.f
    UINT f_operand2;
    INT f_indir_pc__word;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__word) = f_indir_pc__word;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_c_sprv10_p5", "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Pd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_c_sprv10_p9:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_c_sprv10_p9", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Pd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_spr_mv10:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_spr_mv10", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Ps) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sbfs:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sbfs", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_movem_r_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_movem_r_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movem_r_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (in_h_gr_SI_0) = 0;
      FLD (in_h_gr_SI_1) = 1;
      FLD (in_h_gr_SI_10) = 10;
      FLD (in_h_gr_SI_11) = 11;
      FLD (in_h_gr_SI_12) = 12;
      FLD (in_h_gr_SI_13) = 13;
      FLD (in_h_gr_SI_14) = 14;
      FLD (in_h_gr_SI_15) = 15;
      FLD (in_h_gr_SI_2) = 2;
      FLD (in_h_gr_SI_3) = 3;
      FLD (in_h_gr_SI_4) = 4;
      FLD (in_h_gr_SI_5) = 5;
      FLD (in_h_gr_SI_6) = 6;
      FLD (in_h_gr_SI_7) = 7;
      FLD (in_h_gr_SI_8) = 8;
      FLD (in_h_gr_SI_9) = 9;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movem_m_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_movem_m_r.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movem_m_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_0) = 0;
      FLD (out_h_gr_SI_1) = 1;
      FLD (out_h_gr_SI_10) = 10;
      FLD (out_h_gr_SI_11) = 11;
      FLD (out_h_gr_SI_12) = 12;
      FLD (out_h_gr_SI_13) = 13;
      FLD (out_h_gr_SI_14) = 14;
      FLD (out_h_gr_SI_2) = 2;
      FLD (out_h_gr_SI_3) = 3;
      FLD (out_h_gr_SI_4) = 4;
      FLD (out_h_gr_SI_5) = 5;
      FLD (out_h_gr_SI_6) = 6;
      FLD (out_h_gr_SI_7) = 7;
      FLD (out_h_gr_SI_8) = 8;
      FLD (out_h_gr_SI_9) = 9;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movem_m_pc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_movem_m_r.f
    UINT f_memmode;
    UINT f_operand1;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movem_m_pc", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_0) = 0;
      FLD (out_h_gr_SI_1) = 1;
      FLD (out_h_gr_SI_10) = 10;
      FLD (out_h_gr_SI_11) = 11;
      FLD (out_h_gr_SI_12) = 12;
      FLD (out_h_gr_SI_13) = 13;
      FLD (out_h_gr_SI_14) = 14;
      FLD (out_h_gr_SI_2) = 2;
      FLD (out_h_gr_SI_3) = 3;
      FLD (out_h_gr_SI_4) = 4;
      FLD (out_h_gr_SI_5) = 5;
      FLD (out_h_gr_SI_6) = 6;
      FLD (out_h_gr_SI_7) = 7;
      FLD (out_h_gr_SI_8) = 8;
      FLD (out_h_gr_SI_9) = 9;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add_d_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_d_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_add_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addcbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcbr.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addcbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addcwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcwr.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addcwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addcdr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcdr.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addcdr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addcpc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
    INT f_indir_pc__dword;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addcpc", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_adds_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_adds_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_adds_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_adds_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addscbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcbr.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addscbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addscwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcwr.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addscwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addspcpc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addspcpc", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addi_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_neg_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_neg_b_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_neg_d_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_neg_d_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_test_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_memmode;
    UINT f_operand1;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_test_m_b_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_test_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_memmode;
    UINT f_operand1;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_test_m_w_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_test_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_memmode;
    UINT f_operand1;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_test_m_d_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_r_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_r_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_r_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_r_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_r_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_r_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_muls_b:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_muls_b.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_muls_b", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_h_sr_SI_7) = 7;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mstep:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_muls_b.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mstep", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_dstep:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_muls_b.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_dstep", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_w_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_w_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_d_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_d_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
      FLD (out_h_gr_SI_if__SI_andif__DFLT_prefix_set_not__DFLT_inc_index_of__DFLT_Rs_index_of__DFLT_Rd) = ((ANDIF (GET_H_INSN_PREFIXED_P (), (! (FLD (f_memmode))))) ? (FLD (f_operand1)) : (FLD (f_operand2)));
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andcbr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcbr.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andcbr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andcwr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcwr.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andcwr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andcdr:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addcdr.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andcdr", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_andq.f
    UINT f_operand2;
    INT f_s6;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_s6 = EXTRACT_LSB0_INT (insn, 16, 5, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_s6) = f_s6;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andq", "f_operand2 0x%x", 'x', f_operand2, "f_s6 0x%x", 'x', f_s6, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_swap:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_swap", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_asrr_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_asrr_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_asrq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_asrq.f
    UINT f_operand2;
    UINT f_u5;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_u5 = EXTRACT_LSB0_UINT (insn, 16, 4, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_u5) = f_u5;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_asrq", "f_operand2 0x%x", 'x', f_operand2, "f_u5 0x%x", 'x', f_u5, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lsrr_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lsrr_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lsrr_d_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lsrr_d_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_h_gr_SI_index_of__DFLT_Rd) = FLD (f_operand2);
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_btst:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_btst", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_btstq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_asrq.f
    UINT f_operand2;
    UINT f_u5;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_u5 = EXTRACT_LSB0_UINT (insn, 16, 4, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_u5) = f_u5;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_btstq", "f_operand2 0x%x", 'x', f_operand2, "f_u5 0x%x", 'x', f_u5, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_setf:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_setf.f
    UINT f_operand2;
    UINT f_operand1;
    UINT f_dstsrc;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);
  f_dstsrc = ((((f_operand1) | (((f_operand2) << (4))))) & (255));

  /* Record the fields for the semantic handler.  */
  FLD (f_dstsrc) = f_dstsrc;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_setf", "f_dstsrc 0x%x", 'x', f_dstsrc, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_bcc_b:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bcc_b.f
    UINT f_operand2;
    UINT f_disp9_lo;
    INT f_disp9_hi;
    INT f_disp9;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_disp9_lo = EXTRACT_LSB0_UINT (insn, 16, 7, 7);
    f_disp9_hi = EXTRACT_LSB0_INT (insn, 16, 0, 1);
{
  SI tmp_abslo;
  SI tmp_absval;
  tmp_abslo = ((f_disp9_lo) << (1));
  tmp_absval = ((((((f_disp9_hi) != (0))) ? ((~ (255))) : (0))) | (tmp_abslo));
  f_disp9 = ((((pc) + (tmp_absval))) + (((GET_H_V32_NON_V32 ()) ? (0) : (2))));
}

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (i_o_pcrel) = f_disp9;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bcc_b", "f_operand2 0x%x", 'x', f_operand2, "o_pcrel 0x%x", 'x', f_disp9, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ba_b:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bcc_b.f
    UINT f_disp9_lo;
    INT f_disp9_hi;
    INT f_disp9;

    f_disp9_lo = EXTRACT_LSB0_UINT (insn, 16, 7, 7);
    f_disp9_hi = EXTRACT_LSB0_INT (insn, 16, 0, 1);
{
  SI tmp_abslo;
  SI tmp_absval;
  tmp_abslo = ((f_disp9_lo) << (1));
  tmp_absval = ((((((f_disp9_hi) != (0))) ? ((~ (255))) : (0))) | (tmp_abslo));
  f_disp9 = ((((pc) + (tmp_absval))) + (((GET_H_V32_NON_V32 ()) ? (0) : (2))));
}

  /* Record the fields for the semantic handler.  */
  FLD (i_o_pcrel) = f_disp9;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ba_b", "o_pcrel 0x%x", 'x', f_disp9, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bcc_w:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bcc_w.f
    SI f_indir_pc__word_pcrel;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word_pcrel = ((EXTHISI (((HI) (UINT) ((0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)))))) + (((pc) + (((GET_H_V32_NON_V32 ()) ? (0) : (4))))));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (i_o_word_pcrel) = f_indir_pc__word_pcrel;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bcc_w", "f_operand2 0x%x", 'x', f_operand2, "o_word_pcrel 0x%x", 'x', f_indir_pc__word_pcrel, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ba_w:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bcc_w.f
    SI f_indir_pc__word_pcrel;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word_pcrel = ((EXTHISI (((HI) (UINT) ((0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0)))))) + (((pc) + (((GET_H_V32_NON_V32 ()) ? (0) : (4))))));

  /* Record the fields for the semantic handler.  */
  FLD (i_o_word_pcrel) = f_indir_pc__word_pcrel;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ba_w", "o_word_pcrel 0x%x", 'x', f_indir_pc__word_pcrel, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jump_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jump_r", "f_operand1 0x%x", 'x', f_operand1, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Pd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jump_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jump_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Pd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jump_c:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jump_c", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Pd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_break:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_break.f
    UINT f_u4;

    f_u4 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_u4) = f_u4;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_break", "f_u4 0x%x", 'x', f_u4, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rd) = f_operand2;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_cb:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_cb", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_cw:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_cw", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bound_cd:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cd.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bound_cd", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (out_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_scc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_scc", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addoq:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addoq.f
    UINT f_operand2;
    INT f_s8;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_s8 = EXTRACT_LSB0_INT (insn, 16, 7, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_s8) = f_s8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addoq", "f_operand2 0x%x", 'x', f_operand2, "f_s8 0x%x", 'x', f_s8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bdapqpc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addoq.f
    INT f_s8;

    f_s8 = EXTRACT_LSB0_INT (insn, 16, 7, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_s8) = f_s8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bdapqpc", "f_s8 0x%x", 'x', f_s8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bdap_32_pc:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
    INT f_indir_pc__dword;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bdap_32_pc", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_pcplus_p0:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_spplus_p8.f
    UINT f_memmode;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);

  /* Record the fields for the semantic handler.  */
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_pcplus_p0", "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_move_m_spplus_p8:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_m_spplus_p8.f
    UINT f_memmode;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);

  /* Record the fields for the semantic handler.  */
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_move_m_spplus_p8", "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_h_gr_SI_14) = 14;
      FLD (out_h_gr_SI_14) = 14;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_m_b_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_m_b_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_m_w_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_m_w_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_m_d_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
    UINT f_operand2;
    UINT f_memmode;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_m_d_m", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_cb:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cb.f
    INT f_indir_pc__byte;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__byte = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__byte) = f_indir_pc__byte;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_cb", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__byte 0x%x", 'x', f_indir_pc__byte, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_cw:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cw.f
    INT f_indir_pc__word;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__word = (0|(EXTRACT_LSB0_UINT (word_1, 32, 15, 16) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__word) = f_indir_pc__word;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_cw", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__word 0x%x", 'x', f_indir_pc__word, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addo_cd:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_bound_cd.f
    INT f_indir_pc__dword;
    UINT f_operand2;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));
    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addo_cd", "f_operand2 0x%x", 'x', f_operand2, "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_dip_m:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
    UINT f_memmode;
    UINT f_operand1;

    f_memmode = EXTRACT_LSB0_UINT (insn, 16, 10, 1);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand1) = f_operand1;
  FLD (f_memmode) = f_memmode;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_dip_m", "f_operand1 0x%x", 'x', f_operand1, "f_memmode 0x%x", 'x', f_memmode, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rs) = f_operand1;
      FLD (out_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_dip_c:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
    INT f_indir_pc__dword;
    /* Contents of trailing part of insn.  */
    UINT word_1;

  word_1 = GETIMEMUSI (current_cpu, pc + 2);
    f_indir_pc__dword = (0|(EXTRACT_LSB0_UINT (word_1, 32, 31, 32) << 0));

  /* Record the fields for the semantic handler.  */
  FLD (f_indir_pc__dword) = f_indir_pc__dword;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_dip_c", "f_indir_pc__dword 0x%x", 'x', f_indir_pc__dword, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_addi_acr_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_add_b_r.f
    UINT f_operand2;
    UINT f_operand1;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);
    f_operand1 = EXTRACT_LSB0_UINT (insn, 16, 3, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  FLD (f_operand1) = f_operand1;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi_acr_b_r", "f_operand2 0x%x", 'x', f_operand2, "f_operand1 0x%x", 'x', f_operand1, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
      FLD (in_Rs) = f_operand1;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_biap_pc_b_r:
  {
    const IDESC *idesc = &crisv10f_insn_data[itype];
    CGEN_INSN_INT insn = base_insn;
#define FLD(f) abuf->fields.sfmt_addoq.f
    UINT f_operand2;

    f_operand2 = EXTRACT_LSB0_UINT (insn, 16, 15, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_operand2) = f_operand2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_biap_pc_b_r", "f_operand2 0x%x", 'x', f_operand2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_Rd) = f_operand2;
    }
#endif
#undef FLD
    return idesc;
  }

}
