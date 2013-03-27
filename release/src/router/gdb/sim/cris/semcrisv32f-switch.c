/* Simulator instruction semantics for crisv32f.

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

#ifdef DEFINE_LABELS

  /* The labels have the case they have because the enum of insn types
     is all uppercase and in the non-stdc case the insn symbol is built
     into the enum name.  */

  static struct {
    int index;
    void *label;
  } labels[] = {
    { CRISV32F_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { CRISV32F_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { CRISV32F_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { CRISV32F_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { CRISV32F_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { CRISV32F_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { CRISV32F_INSN_MOVE_B_R, && case_sem_INSN_MOVE_B_R },
    { CRISV32F_INSN_MOVE_W_R, && case_sem_INSN_MOVE_W_R },
    { CRISV32F_INSN_MOVE_D_R, && case_sem_INSN_MOVE_D_R },
    { CRISV32F_INSN_MOVEQ, && case_sem_INSN_MOVEQ },
    { CRISV32F_INSN_MOVS_B_R, && case_sem_INSN_MOVS_B_R },
    { CRISV32F_INSN_MOVS_W_R, && case_sem_INSN_MOVS_W_R },
    { CRISV32F_INSN_MOVU_B_R, && case_sem_INSN_MOVU_B_R },
    { CRISV32F_INSN_MOVU_W_R, && case_sem_INSN_MOVU_W_R },
    { CRISV32F_INSN_MOVECBR, && case_sem_INSN_MOVECBR },
    { CRISV32F_INSN_MOVECWR, && case_sem_INSN_MOVECWR },
    { CRISV32F_INSN_MOVECDR, && case_sem_INSN_MOVECDR },
    { CRISV32F_INSN_MOVSCBR, && case_sem_INSN_MOVSCBR },
    { CRISV32F_INSN_MOVSCWR, && case_sem_INSN_MOVSCWR },
    { CRISV32F_INSN_MOVUCBR, && case_sem_INSN_MOVUCBR },
    { CRISV32F_INSN_MOVUCWR, && case_sem_INSN_MOVUCWR },
    { CRISV32F_INSN_ADDQ, && case_sem_INSN_ADDQ },
    { CRISV32F_INSN_SUBQ, && case_sem_INSN_SUBQ },
    { CRISV32F_INSN_CMP_R_B_R, && case_sem_INSN_CMP_R_B_R },
    { CRISV32F_INSN_CMP_R_W_R, && case_sem_INSN_CMP_R_W_R },
    { CRISV32F_INSN_CMP_R_D_R, && case_sem_INSN_CMP_R_D_R },
    { CRISV32F_INSN_CMP_M_B_M, && case_sem_INSN_CMP_M_B_M },
    { CRISV32F_INSN_CMP_M_W_M, && case_sem_INSN_CMP_M_W_M },
    { CRISV32F_INSN_CMP_M_D_M, && case_sem_INSN_CMP_M_D_M },
    { CRISV32F_INSN_CMPCBR, && case_sem_INSN_CMPCBR },
    { CRISV32F_INSN_CMPCWR, && case_sem_INSN_CMPCWR },
    { CRISV32F_INSN_CMPCDR, && case_sem_INSN_CMPCDR },
    { CRISV32F_INSN_CMPQ, && case_sem_INSN_CMPQ },
    { CRISV32F_INSN_CMPS_M_B_M, && case_sem_INSN_CMPS_M_B_M },
    { CRISV32F_INSN_CMPS_M_W_M, && case_sem_INSN_CMPS_M_W_M },
    { CRISV32F_INSN_CMPSCBR, && case_sem_INSN_CMPSCBR },
    { CRISV32F_INSN_CMPSCWR, && case_sem_INSN_CMPSCWR },
    { CRISV32F_INSN_CMPU_M_B_M, && case_sem_INSN_CMPU_M_B_M },
    { CRISV32F_INSN_CMPU_M_W_M, && case_sem_INSN_CMPU_M_W_M },
    { CRISV32F_INSN_CMPUCBR, && case_sem_INSN_CMPUCBR },
    { CRISV32F_INSN_CMPUCWR, && case_sem_INSN_CMPUCWR },
    { CRISV32F_INSN_MOVE_M_B_M, && case_sem_INSN_MOVE_M_B_M },
    { CRISV32F_INSN_MOVE_M_W_M, && case_sem_INSN_MOVE_M_W_M },
    { CRISV32F_INSN_MOVE_M_D_M, && case_sem_INSN_MOVE_M_D_M },
    { CRISV32F_INSN_MOVS_M_B_M, && case_sem_INSN_MOVS_M_B_M },
    { CRISV32F_INSN_MOVS_M_W_M, && case_sem_INSN_MOVS_M_W_M },
    { CRISV32F_INSN_MOVU_M_B_M, && case_sem_INSN_MOVU_M_B_M },
    { CRISV32F_INSN_MOVU_M_W_M, && case_sem_INSN_MOVU_M_W_M },
    { CRISV32F_INSN_MOVE_R_SPRV32, && case_sem_INSN_MOVE_R_SPRV32 },
    { CRISV32F_INSN_MOVE_SPR_RV32, && case_sem_INSN_MOVE_SPR_RV32 },
    { CRISV32F_INSN_MOVE_M_SPRV32, && case_sem_INSN_MOVE_M_SPRV32 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P2, && case_sem_INSN_MOVE_C_SPRV32_P2 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P3, && case_sem_INSN_MOVE_C_SPRV32_P3 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P5, && case_sem_INSN_MOVE_C_SPRV32_P5 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P6, && case_sem_INSN_MOVE_C_SPRV32_P6 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P7, && case_sem_INSN_MOVE_C_SPRV32_P7 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P9, && case_sem_INSN_MOVE_C_SPRV32_P9 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P10, && case_sem_INSN_MOVE_C_SPRV32_P10 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P11, && case_sem_INSN_MOVE_C_SPRV32_P11 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P12, && case_sem_INSN_MOVE_C_SPRV32_P12 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P13, && case_sem_INSN_MOVE_C_SPRV32_P13 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P14, && case_sem_INSN_MOVE_C_SPRV32_P14 },
    { CRISV32F_INSN_MOVE_C_SPRV32_P15, && case_sem_INSN_MOVE_C_SPRV32_P15 },
    { CRISV32F_INSN_MOVE_SPR_MV32, && case_sem_INSN_MOVE_SPR_MV32 },
    { CRISV32F_INSN_MOVE_SS_R, && case_sem_INSN_MOVE_SS_R },
    { CRISV32F_INSN_MOVE_R_SS, && case_sem_INSN_MOVE_R_SS },
    { CRISV32F_INSN_MOVEM_R_M_V32, && case_sem_INSN_MOVEM_R_M_V32 },
    { CRISV32F_INSN_MOVEM_M_R_V32, && case_sem_INSN_MOVEM_M_R_V32 },
    { CRISV32F_INSN_ADD_B_R, && case_sem_INSN_ADD_B_R },
    { CRISV32F_INSN_ADD_W_R, && case_sem_INSN_ADD_W_R },
    { CRISV32F_INSN_ADD_D_R, && case_sem_INSN_ADD_D_R },
    { CRISV32F_INSN_ADD_M_B_M, && case_sem_INSN_ADD_M_B_M },
    { CRISV32F_INSN_ADD_M_W_M, && case_sem_INSN_ADD_M_W_M },
    { CRISV32F_INSN_ADD_M_D_M, && case_sem_INSN_ADD_M_D_M },
    { CRISV32F_INSN_ADDCBR, && case_sem_INSN_ADDCBR },
    { CRISV32F_INSN_ADDCWR, && case_sem_INSN_ADDCWR },
    { CRISV32F_INSN_ADDCDR, && case_sem_INSN_ADDCDR },
    { CRISV32F_INSN_ADDS_B_R, && case_sem_INSN_ADDS_B_R },
    { CRISV32F_INSN_ADDS_W_R, && case_sem_INSN_ADDS_W_R },
    { CRISV32F_INSN_ADDS_M_B_M, && case_sem_INSN_ADDS_M_B_M },
    { CRISV32F_INSN_ADDS_M_W_M, && case_sem_INSN_ADDS_M_W_M },
    { CRISV32F_INSN_ADDSCBR, && case_sem_INSN_ADDSCBR },
    { CRISV32F_INSN_ADDSCWR, && case_sem_INSN_ADDSCWR },
    { CRISV32F_INSN_ADDU_B_R, && case_sem_INSN_ADDU_B_R },
    { CRISV32F_INSN_ADDU_W_R, && case_sem_INSN_ADDU_W_R },
    { CRISV32F_INSN_ADDU_M_B_M, && case_sem_INSN_ADDU_M_B_M },
    { CRISV32F_INSN_ADDU_M_W_M, && case_sem_INSN_ADDU_M_W_M },
    { CRISV32F_INSN_ADDUCBR, && case_sem_INSN_ADDUCBR },
    { CRISV32F_INSN_ADDUCWR, && case_sem_INSN_ADDUCWR },
    { CRISV32F_INSN_SUB_B_R, && case_sem_INSN_SUB_B_R },
    { CRISV32F_INSN_SUB_W_R, && case_sem_INSN_SUB_W_R },
    { CRISV32F_INSN_SUB_D_R, && case_sem_INSN_SUB_D_R },
    { CRISV32F_INSN_SUB_M_B_M, && case_sem_INSN_SUB_M_B_M },
    { CRISV32F_INSN_SUB_M_W_M, && case_sem_INSN_SUB_M_W_M },
    { CRISV32F_INSN_SUB_M_D_M, && case_sem_INSN_SUB_M_D_M },
    { CRISV32F_INSN_SUBCBR, && case_sem_INSN_SUBCBR },
    { CRISV32F_INSN_SUBCWR, && case_sem_INSN_SUBCWR },
    { CRISV32F_INSN_SUBCDR, && case_sem_INSN_SUBCDR },
    { CRISV32F_INSN_SUBS_B_R, && case_sem_INSN_SUBS_B_R },
    { CRISV32F_INSN_SUBS_W_R, && case_sem_INSN_SUBS_W_R },
    { CRISV32F_INSN_SUBS_M_B_M, && case_sem_INSN_SUBS_M_B_M },
    { CRISV32F_INSN_SUBS_M_W_M, && case_sem_INSN_SUBS_M_W_M },
    { CRISV32F_INSN_SUBSCBR, && case_sem_INSN_SUBSCBR },
    { CRISV32F_INSN_SUBSCWR, && case_sem_INSN_SUBSCWR },
    { CRISV32F_INSN_SUBU_B_R, && case_sem_INSN_SUBU_B_R },
    { CRISV32F_INSN_SUBU_W_R, && case_sem_INSN_SUBU_W_R },
    { CRISV32F_INSN_SUBU_M_B_M, && case_sem_INSN_SUBU_M_B_M },
    { CRISV32F_INSN_SUBU_M_W_M, && case_sem_INSN_SUBU_M_W_M },
    { CRISV32F_INSN_SUBUCBR, && case_sem_INSN_SUBUCBR },
    { CRISV32F_INSN_SUBUCWR, && case_sem_INSN_SUBUCWR },
    { CRISV32F_INSN_ADDC_R, && case_sem_INSN_ADDC_R },
    { CRISV32F_INSN_ADDC_M, && case_sem_INSN_ADDC_M },
    { CRISV32F_INSN_ADDC_C, && case_sem_INSN_ADDC_C },
    { CRISV32F_INSN_LAPC_D, && case_sem_INSN_LAPC_D },
    { CRISV32F_INSN_LAPCQ, && case_sem_INSN_LAPCQ },
    { CRISV32F_INSN_ADDI_B_R, && case_sem_INSN_ADDI_B_R },
    { CRISV32F_INSN_ADDI_W_R, && case_sem_INSN_ADDI_W_R },
    { CRISV32F_INSN_ADDI_D_R, && case_sem_INSN_ADDI_D_R },
    { CRISV32F_INSN_NEG_B_R, && case_sem_INSN_NEG_B_R },
    { CRISV32F_INSN_NEG_W_R, && case_sem_INSN_NEG_W_R },
    { CRISV32F_INSN_NEG_D_R, && case_sem_INSN_NEG_D_R },
    { CRISV32F_INSN_TEST_M_B_M, && case_sem_INSN_TEST_M_B_M },
    { CRISV32F_INSN_TEST_M_W_M, && case_sem_INSN_TEST_M_W_M },
    { CRISV32F_INSN_TEST_M_D_M, && case_sem_INSN_TEST_M_D_M },
    { CRISV32F_INSN_MOVE_R_M_B_M, && case_sem_INSN_MOVE_R_M_B_M },
    { CRISV32F_INSN_MOVE_R_M_W_M, && case_sem_INSN_MOVE_R_M_W_M },
    { CRISV32F_INSN_MOVE_R_M_D_M, && case_sem_INSN_MOVE_R_M_D_M },
    { CRISV32F_INSN_MULS_B, && case_sem_INSN_MULS_B },
    { CRISV32F_INSN_MULS_W, && case_sem_INSN_MULS_W },
    { CRISV32F_INSN_MULS_D, && case_sem_INSN_MULS_D },
    { CRISV32F_INSN_MULU_B, && case_sem_INSN_MULU_B },
    { CRISV32F_INSN_MULU_W, && case_sem_INSN_MULU_W },
    { CRISV32F_INSN_MULU_D, && case_sem_INSN_MULU_D },
    { CRISV32F_INSN_MCP, && case_sem_INSN_MCP },
    { CRISV32F_INSN_DSTEP, && case_sem_INSN_DSTEP },
    { CRISV32F_INSN_ABS, && case_sem_INSN_ABS },
    { CRISV32F_INSN_AND_B_R, && case_sem_INSN_AND_B_R },
    { CRISV32F_INSN_AND_W_R, && case_sem_INSN_AND_W_R },
    { CRISV32F_INSN_AND_D_R, && case_sem_INSN_AND_D_R },
    { CRISV32F_INSN_AND_M_B_M, && case_sem_INSN_AND_M_B_M },
    { CRISV32F_INSN_AND_M_W_M, && case_sem_INSN_AND_M_W_M },
    { CRISV32F_INSN_AND_M_D_M, && case_sem_INSN_AND_M_D_M },
    { CRISV32F_INSN_ANDCBR, && case_sem_INSN_ANDCBR },
    { CRISV32F_INSN_ANDCWR, && case_sem_INSN_ANDCWR },
    { CRISV32F_INSN_ANDCDR, && case_sem_INSN_ANDCDR },
    { CRISV32F_INSN_ANDQ, && case_sem_INSN_ANDQ },
    { CRISV32F_INSN_ORR_B_R, && case_sem_INSN_ORR_B_R },
    { CRISV32F_INSN_ORR_W_R, && case_sem_INSN_ORR_W_R },
    { CRISV32F_INSN_ORR_D_R, && case_sem_INSN_ORR_D_R },
    { CRISV32F_INSN_OR_M_B_M, && case_sem_INSN_OR_M_B_M },
    { CRISV32F_INSN_OR_M_W_M, && case_sem_INSN_OR_M_W_M },
    { CRISV32F_INSN_OR_M_D_M, && case_sem_INSN_OR_M_D_M },
    { CRISV32F_INSN_ORCBR, && case_sem_INSN_ORCBR },
    { CRISV32F_INSN_ORCWR, && case_sem_INSN_ORCWR },
    { CRISV32F_INSN_ORCDR, && case_sem_INSN_ORCDR },
    { CRISV32F_INSN_ORQ, && case_sem_INSN_ORQ },
    { CRISV32F_INSN_XOR, && case_sem_INSN_XOR },
    { CRISV32F_INSN_SWAP, && case_sem_INSN_SWAP },
    { CRISV32F_INSN_ASRR_B_R, && case_sem_INSN_ASRR_B_R },
    { CRISV32F_INSN_ASRR_W_R, && case_sem_INSN_ASRR_W_R },
    { CRISV32F_INSN_ASRR_D_R, && case_sem_INSN_ASRR_D_R },
    { CRISV32F_INSN_ASRQ, && case_sem_INSN_ASRQ },
    { CRISV32F_INSN_LSRR_B_R, && case_sem_INSN_LSRR_B_R },
    { CRISV32F_INSN_LSRR_W_R, && case_sem_INSN_LSRR_W_R },
    { CRISV32F_INSN_LSRR_D_R, && case_sem_INSN_LSRR_D_R },
    { CRISV32F_INSN_LSRQ, && case_sem_INSN_LSRQ },
    { CRISV32F_INSN_LSLR_B_R, && case_sem_INSN_LSLR_B_R },
    { CRISV32F_INSN_LSLR_W_R, && case_sem_INSN_LSLR_W_R },
    { CRISV32F_INSN_LSLR_D_R, && case_sem_INSN_LSLR_D_R },
    { CRISV32F_INSN_LSLQ, && case_sem_INSN_LSLQ },
    { CRISV32F_INSN_BTST, && case_sem_INSN_BTST },
    { CRISV32F_INSN_BTSTQ, && case_sem_INSN_BTSTQ },
    { CRISV32F_INSN_SETF, && case_sem_INSN_SETF },
    { CRISV32F_INSN_CLEARF, && case_sem_INSN_CLEARF },
    { CRISV32F_INSN_RFE, && case_sem_INSN_RFE },
    { CRISV32F_INSN_SFE, && case_sem_INSN_SFE },
    { CRISV32F_INSN_RFG, && case_sem_INSN_RFG },
    { CRISV32F_INSN_RFN, && case_sem_INSN_RFN },
    { CRISV32F_INSN_HALT, && case_sem_INSN_HALT },
    { CRISV32F_INSN_BCC_B, && case_sem_INSN_BCC_B },
    { CRISV32F_INSN_BA_B, && case_sem_INSN_BA_B },
    { CRISV32F_INSN_BCC_W, && case_sem_INSN_BCC_W },
    { CRISV32F_INSN_BA_W, && case_sem_INSN_BA_W },
    { CRISV32F_INSN_JAS_R, && case_sem_INSN_JAS_R },
    { CRISV32F_INSN_JAS_C, && case_sem_INSN_JAS_C },
    { CRISV32F_INSN_JUMP_P, && case_sem_INSN_JUMP_P },
    { CRISV32F_INSN_BAS_C, && case_sem_INSN_BAS_C },
    { CRISV32F_INSN_JASC_R, && case_sem_INSN_JASC_R },
    { CRISV32F_INSN_JASC_C, && case_sem_INSN_JASC_C },
    { CRISV32F_INSN_BASC_C, && case_sem_INSN_BASC_C },
    { CRISV32F_INSN_BREAK, && case_sem_INSN_BREAK },
    { CRISV32F_INSN_BOUND_R_B_R, && case_sem_INSN_BOUND_R_B_R },
    { CRISV32F_INSN_BOUND_R_W_R, && case_sem_INSN_BOUND_R_W_R },
    { CRISV32F_INSN_BOUND_R_D_R, && case_sem_INSN_BOUND_R_D_R },
    { CRISV32F_INSN_BOUND_CB, && case_sem_INSN_BOUND_CB },
    { CRISV32F_INSN_BOUND_CW, && case_sem_INSN_BOUND_CW },
    { CRISV32F_INSN_BOUND_CD, && case_sem_INSN_BOUND_CD },
    { CRISV32F_INSN_SCC, && case_sem_INSN_SCC },
    { CRISV32F_INSN_LZ, && case_sem_INSN_LZ },
    { CRISV32F_INSN_ADDOQ, && case_sem_INSN_ADDOQ },
    { CRISV32F_INSN_ADDO_M_B_M, && case_sem_INSN_ADDO_M_B_M },
    { CRISV32F_INSN_ADDO_M_W_M, && case_sem_INSN_ADDO_M_W_M },
    { CRISV32F_INSN_ADDO_M_D_M, && case_sem_INSN_ADDO_M_D_M },
    { CRISV32F_INSN_ADDO_CB, && case_sem_INSN_ADDO_CB },
    { CRISV32F_INSN_ADDO_CW, && case_sem_INSN_ADDO_CW },
    { CRISV32F_INSN_ADDO_CD, && case_sem_INSN_ADDO_CD },
    { CRISV32F_INSN_ADDI_ACR_B_R, && case_sem_INSN_ADDI_ACR_B_R },
    { CRISV32F_INSN_ADDI_ACR_W_R, && case_sem_INSN_ADDI_ACR_W_R },
    { CRISV32F_INSN_ADDI_ACR_D_R, && case_sem_INSN_ADDI_ACR_D_R },
    { CRISV32F_INSN_FIDXI, && case_sem_INSN_FIDXI },
    { CRISV32F_INSN_FTAGI, && case_sem_INSN_FTAGI },
    { CRISV32F_INSN_FIDXD, && case_sem_INSN_FIDXD },
    { CRISV32F_INSN_FTAGD, && case_sem_INSN_FTAGD },
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
    vpc = SEM_NEXT_VPC (sem_arg, pc, 2);
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
#if WITH_SCACHE_PBB_CRISV32F
    crisv32f_pbb_after (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_CRISV32F
    crisv32f_pbb_before (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_CRISV32F
#ifdef DEFINE_SWITCH
    vpc = crisv32f_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = crisv32f_pbb_cti_chain (current_cpu, sem_arg,
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
#if WITH_SCACHE_PBB_CRISV32F
    vpc = crisv32f_pbb_chain (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_CRISV32F
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = crisv32f_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = crisv32f_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = crisv32f_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_B_R) : /* move.b move.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_W_R) : /* move.w move.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_D_R) : /* move.d move.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVEQ) : /* moveq $i,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_moveq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_newval;
  tmp_newval = FLD (f_s6);
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
SET_H_NBIT_MOVE (LTSI (tmp_newval, 0));
SET_H_ZBIT_MOVE (ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1))));
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVS_B_R) : /* movs.b movs.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
  {
    SI opval = EXTQISI (tmp_newval);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVS_W_R) : /* movs.w movs.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
  {
    SI opval = EXTHISI (tmp_newval);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVU_B_R) : /* movu.b movu.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
  {
    SI opval = ZEXTQISI (tmp_newval);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVU_W_R) : /* movu.w movu.m ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_newval;
  tmp_newval = GET_H_GR (FLD (f_operand1));
  {
    SI opval = ZEXTHISI (tmp_newval);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVECBR) : /* move.b ${sconst8},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_newval;
  tmp_newval = FLD (f_indir_pc__byte);
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVECWR) : /* move.w ${sconst16},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_newval;
  tmp_newval = FLD (f_indir_pc__word);
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVECDR) : /* move.d ${const32},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_newval;
  tmp_newval = FLD (f_indir_pc__dword);
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVSCBR) : /* movs.b ${sconst8},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_newval;
  tmp_newval = EXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVSCWR) : /* movs.w ${sconst16},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_newval;
  tmp_newval = EXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVUCBR) : /* movu.b ${uconst8},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_newval;
  tmp_newval = ZEXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVUCWR) : /* movu.w ${uconst16},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_newval;
  tmp_newval = ZEXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDQ) : /* addq $j,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_u6);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBQ) : /* subq $j,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_u6);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_R_B_R) : /* cmp-r.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_R_W_R) : /* cmp-r.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_R_D_R) : /* cmp-r.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_M_B_M) : /* cmp-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_M_W_M) : /* cmp-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMP_M_D_M) : /* cmp-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPCBR) : /* cmp.b $sconst8,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = TRUNCSIQI (FLD (f_indir_pc__byte));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPCWR) : /* cmp.w $sconst16,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = TRUNCSIHI (FLD (f_indir_pc__word));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPCDR) : /* cmp.d $const32,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__dword);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPQ) : /* cmpq $i,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_andq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_s6);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPS_M_B_M) : /* cmps-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPS_M_W_M) : /* cmps-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPSCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPSCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPU_M_B_M) : /* cmpu-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPU_M_W_M) : /* cmpu-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPUCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPUCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_M_B_M) : /* move-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmp, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_M_W_M) : /* move-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmp, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_M_D_M) : /* move-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  {
    SI opval = tmp_tmp;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVS_M_B_M) : /* movs-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
if (ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand2), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTSI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVS_M_W_M) : /* movs-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = EXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
if (ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand2), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTSI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVU_M_B_M) : /* movu-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = ZEXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
if (ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand2), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTSI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVU_M_W_M) : /* movu-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = ZEXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
if (ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = tmp_tmp;
    SET_H_GR (FLD (f_operand2), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTSI (tmp_tmp, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmp, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_R_SPRV32) : /* move ${Rs},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  SI tmp_rno;
  tmp_tmp = GET_H_GR (FLD (f_operand1));
  tmp_rno = FLD (f_operand2);
if (ORIF (ORIF (EQSI (tmp_rno, 0), EQSI (tmp_rno, 1)), ORIF (EQSI (tmp_rno, 4), EQSI (tmp_rno, 8)))) {
cgen_rtx_error (current_cpu, "move-r-spr: trying to set a read-only special register");
}
 else {
  {
    SI opval = tmp_tmp;
    SET_H_SR (FLD (f_operand2), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_SPR_RV32) : /* move ${Ps},${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_grno;
  SI tmp_prno;
  SI tmp_newval;
  tmp_prno = FLD (f_operand2);
  tmp_newval = GET_H_SR (FLD (f_operand2));
if (EQSI (tmp_prno, 2)) {
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand1));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
 else if (EQSI (tmp_prno, 3)) {
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand1));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
 else if (EQSI (tmp_prno, 5)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 6)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 7)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 9)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 10)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 11)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 12)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 13)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 14)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 15)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else if (EQSI (tmp_prno, 0)) {
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand1));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
 else if (EQSI (tmp_prno, 1)) {
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand1));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
 else if (EQSI (tmp_prno, 4)) {
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand1));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
 else if (EQSI (tmp_prno, 8)) {
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
 else {
cgen_rtx_error (current_cpu, "move-spr-r from unimplemented register");
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_M_SPRV32) : /* move [${Rs}${inc}],${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_rno;
  SI tmp_newval;
  tmp_rno = FLD (f_operand2);
if (EQSI (tmp_rno, 2)) {
  tmp_newval = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
}
 else if (EQSI (tmp_rno, 3)) {
  tmp_newval = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
}
 else if (EQSI (tmp_rno, 5)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 6)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 7)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 9)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 10)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 11)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 12)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 13)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 14)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else if (EQSI (tmp_rno, 15)) {
  tmp_newval = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
}
 else {
cgen_rtx_error (current_cpu, "Trying to set unimplemented special register");
}
  {
    SI opval = tmp_newval;
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P2) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P3) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P5) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P6) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P7) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P9) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P10) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P11) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P12) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P13) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P14) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_C_SPRV32_P15) : /* move ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (f_indir_pc__dword);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_SPR_MV32) : /* move ${Ps},[${Rd-sfield}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_rno;
  tmp_rno = FLD (f_operand2);
if (EQSI (tmp_rno, 2)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 3)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 5)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 6)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 7)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 9)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 10)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 11)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 12)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 13)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 14)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 15)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 0)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 1)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    QI opval = GET_H_SR (FLD (f_operand2));
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 4)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    HI opval = GET_H_SR (FLD (f_operand2));
    SETMEMHI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    HI opval = GET_H_SR (FLD (f_operand2));
    SETMEMHI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else if (EQSI (tmp_rno, 8)) {
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = GET_H_SR (FLD (f_operand2));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
}
 else {
cgen_rtx_error (current_cpu, "write from unimplemented special register");
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_SS_R) : /* move ${Ss},${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_SUPR (FLD (f_operand2));
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_R_SS) : /* move ${Rs},${Sd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GR (FLD (f_operand1));
    SET_H_SUPR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "supr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVEM_R_M_V32) : /* movem ${Rs-dfield},[${Rd-sfield}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movem_r_m_v32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
{
  SI tmp_dummy;
  tmp_dummy = GET_H_GR (FLD (f_operand2));
}
  tmp_addr = GET_H_GR (FLD (f_operand1));
{
if (GESI (FLD (f_operand2), 0)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 0));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 1)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 1));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 2)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 2));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 3)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 3));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 4)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 4));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 5)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 5));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 6)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 6));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 7)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 7));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 8)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 8));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 9)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 9));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 10)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 10));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 11)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 11));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 12)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 12));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 13)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 13));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 14)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 14));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 15)) {
{
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (((UINT) 15));
  {
    SI opval = tmp_tmp;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
}
if (NEBI (tmp_postinc, 0)) {
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVEM_M_R_V32) : /* movem [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movem_m_r_v32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = GET_H_GR (FLD (f_operand1));
{
  SI tmp_dummy;
  tmp_dummy = GET_H_GR (FLD (f_operand2));
}
{
if (GESI (FLD (f_operand2), 0)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 0), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 1)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 1), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 2)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 2), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 3)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 3), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 4)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 4), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 5)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 5), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 6)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 6), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 7)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 7), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 8)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 8), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 9)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 9), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 10)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 10), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 11)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 11), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 12)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 12), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 13)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 13), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 14)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 14), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
if (GESI (FLD (f_operand2), 15)) {
{
  SI tmp_tmp;
  tmp_tmp = GETMEMSI (current_cpu, pc, tmp_addr);
  {
    SI opval = tmp_tmp;
    SET_H_GR (((UINT) 15), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  tmp_addr = ADDSI (tmp_addr, 4);
}
}
}
if (NEBI (tmp_postinc, 0)) {
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_B_R) : /* add.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), ORIF (ANDIF (LTQI (tmp_tmpopd, 0), GEQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (GEQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_W_R) : /* add.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), ORIF (ANDIF (LTHI (tmp_tmpopd, 0), GEHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (GEHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_D_R) : /* add.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_M_B_M) : /* add-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), ORIF (ANDIF (LTQI (tmp_tmpopd, 0), GEQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (GEQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_M_W_M) : /* add-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), ORIF (ANDIF (LTHI (tmp_tmpopd, 0), GEHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (GEHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_M_D_M) : /* add-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDCBR) : /* add.b ${sconst8}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__byte);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), ORIF (ANDIF (LTQI (tmp_tmpopd, 0), GEQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (GEQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDCWR) : /* add.w ${sconst16}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__word);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), ORIF (ANDIF (LTHI (tmp_tmpopd, 0), GEHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (GEHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDCDR) : /* add.d ${const32}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcdr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__dword);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDS_B_R) : /* adds.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDS_W_R) : /* adds.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDS_M_B_M) : /* adds-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDS_M_W_M) : /* adds-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDSCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDSCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDU_B_R) : /* addu.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDU_W_R) : /* addu.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDU_M_B_M) : /* addu-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDU_M_W_M) : /* addu-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDUCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDUCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_B_R) : /* sub.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_W_R) : /* sub.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_D_R) : /* sub.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_M_B_M) : /* sub-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_M_W_M) : /* sub-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_M_D_M) : /* sub-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBCBR) : /* sub.b ${sconst8}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__byte);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBCWR) : /* sub.w ${sconst16}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__word);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBCDR) : /* sub.d ${const32}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcdr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__dword);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBS_B_R) : /* subs.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBS_W_R) : /* subs.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBS_M_B_M) : /* subs-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBS_M_W_M) : /* subs-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBSCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBSCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = EXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBU_B_R) : /* subu.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBU_W_R) : /* subu.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBU_M_B_M) : /* subu-m.b [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBU_M_W_M) : /* subu-m.w [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBUCBR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBUCWR) : /* [${Rs}${inc}],$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (FLD (f_indir_pc__word)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDC_R) : /* addc $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
CPU (h_xbit) = 1;
{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDC_M) : /* addc [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
CPU (h_xbit) = 1;
{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDC_C) : /* addc ${const32},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcdr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
CPU (h_xbit) = 1;
{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__dword);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_carry = CPU (h_cbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LAPC_D) : /* lapc.d ${const32-pcrel},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_lapc_d.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = FLD (i_const32_pcrel);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LAPCQ) : /* lapcq ${qo},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_lapcq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = FLD (i_qo);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_B_R) : /* addi.b ${Rs-dfield}.m,${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 1));
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_W_R) : /* addi.w ${Rs-dfield}.m,${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 2));
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_D_R) : /* addi.d ${Rs-dfield}.m,${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 4));
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEG_B_R) : /* neg.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = 0;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEG_W_R) : /* neg.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = 0;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_newval, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEG_D_R) : /* neg.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_GR (FLD (f_operand1));
  tmp_tmpopd = 0;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TEST_M_B_M) : /* test-m.b [${Rs}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
{
  QI tmp_tmpopd;
  QI tmp_tmpops;
  BI tmp_carry;
  QI tmp_newval;
  tmp_tmpops = 0;
  tmp_tmpopd = tmp_tmpd;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCQI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), ORIF (ANDIF (GEQI (tmp_tmpopd, 0), LTQI (tmp_newval, 0)), ANDIF (LTQI (tmp_tmpops, 0), LTQI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTQI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEQI (tmp_tmpops, 0), LTQI (tmp_tmpopd, 0)), GEQI (tmp_newval, 0)), ANDIF (ANDIF (LTQI (tmp_tmpops, 0), GEQI (tmp_tmpopd, 0)), LTQI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TEST_M_W_M) : /* test-m.w [${Rs}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
{
  HI tmp_tmpopd;
  HI tmp_tmpops;
  BI tmp_carry;
  HI tmp_newval;
  tmp_tmpops = 0;
  tmp_tmpopd = tmp_tmpd;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCHI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), ORIF (ANDIF (GEHI (tmp_tmpopd, 0), LTHI (tmp_newval, 0)), ANDIF (LTHI (tmp_tmpops, 0), LTHI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTHI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GEHI (tmp_tmpops, 0), LTHI (tmp_tmpopd, 0)), GEHI (tmp_newval, 0)), ANDIF (ANDIF (LTHI (tmp_tmpops, 0), GEHI (tmp_tmpopd, 0)), LTHI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TEST_M_D_M) : /* test-m.d [${Rs}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = 0;
  tmp_tmpopd = tmp_tmpd;
  tmp_carry = CPU (h_cbit);
  tmp_newval = SUBCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
((void) 0); /*nop*/
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), ORIF (ANDIF (GESI (tmp_tmpopd, 0), LTSI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_newval, 0))));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (GESI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_R_M_B_M) : /* move-r-m.b ${Rs-dfield},[${Rd-sfield}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = GET_H_GR (FLD (f_operand2));
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    QI opval = tmp_tmpd;
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    QI opval = tmp_tmpd;
    SETMEMQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_R_M_W_M) : /* move-r-m.w ${Rs-dfield},[${Rd-sfield}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = GET_H_GR (FLD (f_operand2));
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    HI opval = tmp_tmpd;
    SETMEMHI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    HI opval = tmp_tmpd;
    SETMEMHI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVE_R_M_D_M) : /* move-r-m.d ${Rs-dfield},[${Rd-sfield}${inc}] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = GET_H_GR (FLD (f_operand2));
{
  SI tmp_addr;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
  tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
if (ANDIF (GET_H_V32_V32 (), NEBI (CPU (h_xbit), 0))) {
if (EQBI (CPU (h_pbit), 0)) {
{
  {
    SI opval = tmp_tmpd;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    BI opval = CPU (h_pbit);
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
} else {
  {
    SI opval = tmp_tmpd;
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}
if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULS_B) : /* muls.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = EXTQIDI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = EXTQIDI (TRUNCSIQI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, EXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULS_W) : /* muls.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = EXTHIDI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = EXTHIDI (TRUNCSIHI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, EXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULS_D) : /* muls.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = EXTSIDI (TRUNCSISI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = EXTSIDI (TRUNCSISI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, EXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULU_B) : /* mulu.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = ZEXTQIDI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = ZEXTQIDI (TRUNCSIQI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, ZEXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULU_W) : /* mulu.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = ZEXTHIDI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = ZEXTHIDI (TRUNCSIHI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, ZEXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULU_D) : /* mulu.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_src1;
  DI tmp_src2;
  DI tmp_tmpr;
  tmp_src1 = ZEXTSIDI (TRUNCSISI (GET_H_GR (FLD (f_operand1))));
  tmp_src2 = ZEXTSIDI (TRUNCSISI (GET_H_GR (FLD (f_operand2))));
  tmp_tmpr = MULDI (tmp_src1, tmp_src2);
  {
    SI opval = TRUNCDISI (tmp_tmpr);
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = TRUNCDISI (SRLDI (tmp_tmpr, 32));
    SET_H_SR (((UINT) 7), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
{
  {
    BI opval = ANDIF (GET_H_V32_V32 (), CPU (h_cbit));
    CPU (h_cbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
  {
    BI opval = LTDI (tmp_tmpr, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQDI (tmp_tmpr, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = NEDI (tmp_tmpr, ZEXTSIDI (TRUNCDISI (tmp_tmpr)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCP) : /* mcp $Ps,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
CPU (h_xbit) = 1;
CPU (h_zbit) = 1;
{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  BI tmp_carry;
  SI tmp_newval;
  tmp_tmpops = GET_H_SR (FLD (f_operand2));
  tmp_tmpopd = GET_H_GR (FLD (f_operand1));
  tmp_carry = CPU (h_rbit);
  tmp_newval = ADDCSI (tmp_tmpopd, tmp_tmpops, ((EQBI (CPU (h_xbit), 0)) ? (0) : (tmp_carry)));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = ORIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), ORIF (ANDIF (LTSI (tmp_tmpopd, 0), GESI (tmp_newval, 0)), ANDIF (LTSI (tmp_tmpops, 0), GESI (tmp_newval, 0))));
    CPU (h_rbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "rbit", 'x', opval);
  }
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ORIF (CPU (h_zbit), NOTBI (CPU (h_xbit))));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
  {
    BI opval = ORIF (ANDIF (ANDIF (LTSI (tmp_tmpops, 0), LTSI (tmp_tmpopd, 0)), GESI (tmp_newval, 0)), ANDIF (ANDIF (GESI (tmp_tmpops, 0), GESI (tmp_tmpopd, 0)), LTSI (tmp_newval, 0)));
    CPU (h_vbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DSTEP) : /* dstep $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  SI tmp_tmps;
  SI tmp_tmpd;
  tmp_tmps = GET_H_GR (FLD (f_operand1));
  tmp_tmp = SLLSI (GET_H_GR (FLD (f_operand2)), 1);
  tmp_tmpd = ((GEUSI (tmp_tmp, tmp_tmps)) ? (SUBSI (tmp_tmp, tmp_tmps)) : (tmp_tmp));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ABS) : /* abs $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ABSSI (GET_H_GR (FLD (f_operand1)));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_B_R) : /* and.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = ANDQI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_W_R) : /* and.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = ANDHI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_D_R) : /* and.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ANDSI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_M_B_M) : /* and-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = ANDQI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_M_W_M) : /* and-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = ANDHI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_M_D_M) : /* and-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ANDSI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDCBR) : /* and.b ${sconst8}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_tmpd;
  tmp_tmpd = ANDQI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__byte));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDCWR) : /* and.w ${sconst16}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmpd;
  tmp_tmpd = ANDHI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__word));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDCDR) : /* and.d ${const32}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcdr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpd;
  tmp_tmpd = ANDSI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__dword));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDQ) : /* andq $i,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_andq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ANDSI (GET_H_GR (FLD (f_operand2)), FLD (f_s6));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORR_B_R) : /* orr.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = ORQI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORR_W_R) : /* orr.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = ORHI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORR_D_R) : /* orr.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ORSI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR_M_B_M) : /* or-m.b [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  tmp_tmpd = ORQI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR_M_W_M) : /* or-m.w [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  tmp_tmpd = ORHI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR_M_D_M) : /* or-m.d [${Rs}${inc}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ORSI (GET_H_GR (FLD (f_operand2)), ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; }));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (((ANDIF (GET_H_INSN_PREFIXED_P (), NOTSI (FLD (f_memmode)))) ? (FLD (f_operand1)) : (FLD (f_operand2))), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORCBR) : /* or.b ${sconst8}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcbr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_tmpd;
  tmp_tmpd = ORQI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__byte));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORCWR) : /* or.w ${sconst16}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcwr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmpd;
  tmp_tmpd = ORHI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__word));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORCDR) : /* or.d ${const32}],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addcdr.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpd;
  tmp_tmpd = ORSI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__dword));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORQ) : /* orq $i,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_andq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = ORSI (GET_H_GR (FLD (f_operand2)), FLD (f_s6));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR) : /* xor $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = XORSI (GET_H_GR (FLD (f_operand2)), GET_H_GR (FLD (f_operand1)));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SWAP) : /* swap${swapoption} ${Rs} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmps;
  SI tmp_tmpd;
  tmp_tmps = GET_H_GR (FLD (f_operand1));
  tmp_tmpd = ({   SI tmp_tmpcode;
  SI tmp_tmpval;
  SI tmp_tmpres;
  tmp_tmpcode = FLD (f_operand2);
;   tmp_tmpval = tmp_tmps;
; if (EQSI (tmp_tmpcode, 0)) {
  tmp_tmpres = (cgen_rtx_error (current_cpu, "SWAP without swap modifier isn't implemented"), 0);
}
 else if (EQSI (tmp_tmpcode, 1)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = tmp_tmpval;
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 2)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
}
 else if (EQSI (tmp_tmpcode, 3)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 4)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
}
 else if (EQSI (tmp_tmpcode, 5)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 6)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
}
 else if (EQSI (tmp_tmpcode, 7)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = ({   SI tmp_tmpb;
  tmp_tmpb = tmp_tmpval;
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 8)) {
  tmp_tmpres = INVSI (tmp_tmpval);
}
 else if (EQSI (tmp_tmpcode, 9)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = INVSI (tmp_tmpval);
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 10)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
}
 else if (EQSI (tmp_tmpcode, 11)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 12)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
}
 else if (EQSI (tmp_tmpcode, 13)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
 else if (EQSI (tmp_tmpcode, 14)) {
  tmp_tmpres = ({   SI tmp_tmpb;
  tmp_tmpb = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
}
 else if (EQSI (tmp_tmpcode, 15)) {
  tmp_tmpres = ({   SI tmp_tmpr;
  tmp_tmpr = ({   SI tmp_tmpb;
  tmp_tmpb = ({   SI tmp_tmpb;
  tmp_tmpb = INVSI (tmp_tmpval);
; ORSI (ANDSI (SLLSI (tmp_tmpb, 16), 0xffff0000), ANDSI (SRLSI (tmp_tmpb, 16), 65535)); });
; ORSI (ANDSI (SLLSI (tmp_tmpb, 8), 0xff00ff00), ANDSI (SRLSI (tmp_tmpb, 8), 16711935)); });
; ORSI (SLLSI (ANDSI (tmp_tmpr, 16843009), 7), ORSI (SLLSI (ANDSI (tmp_tmpr, 33686018), 5), ORSI (SLLSI (ANDSI (tmp_tmpr, 67372036), 3), ORSI (SLLSI (ANDSI (tmp_tmpr, 134744072), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 269488144), 1), ORSI (SRLSI (ANDSI (tmp_tmpr, 538976288), 3), ORSI (SRLSI (ANDSI (tmp_tmpr, 1077952576), 5), SRLSI (ANDSI (tmp_tmpr, 0x80808080), 7)))))))); });
}
; tmp_tmpres; });
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ASRR_B_R) : /* asrr.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmpd;
  SI tmp_cnt1;
  SI tmp_cnt2;
  tmp_cnt1 = GET_H_GR (FLD (f_operand1));
  tmp_cnt2 = ((NESI (ANDSI (tmp_cnt1, 32), 0)) ? (31) : (ANDSI (tmp_cnt1, 31)));
  tmp_tmpd = SRASI (EXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand2)))), tmp_cnt2);
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ASRR_W_R) : /* asrr.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmpd;
  SI tmp_cnt1;
  SI tmp_cnt2;
  tmp_cnt1 = GET_H_GR (FLD (f_operand1));
  tmp_cnt2 = ((NESI (ANDSI (tmp_cnt1, 32), 0)) ? (31) : (ANDSI (tmp_cnt1, 31)));
  tmp_tmpd = SRASI (EXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand2)))), tmp_cnt2);
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ASRR_D_R) : /* asrr.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt1;
  SI tmp_cnt2;
  tmp_cnt1 = GET_H_GR (FLD (f_operand1));
  tmp_cnt2 = ((NESI (ANDSI (tmp_cnt1, 32), 0)) ? (31) : (ANDSI (tmp_cnt1, 31)));
  tmp_tmpd = SRASI (EXTSISI (TRUNCSISI (GET_H_GR (FLD (f_operand2)))), tmp_cnt2);
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ASRQ) : /* asrq $c,${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_asrq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = SRASI (GET_H_GR (FLD (f_operand2)), FLD (f_u5));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSRR_B_R) : /* lsrr.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SRLSI (ZEXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSRR_W_R) : /* lsrr.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SRLSI (ZEXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSRR_D_R) : /* lsrr.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SRLSI (ZEXTSISI (TRUNCSISI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSRQ) : /* lsrq $c,${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_asrq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = SRLSI (GET_H_GR (FLD (f_operand2)), FLD (f_u5));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSLR_B_R) : /* lslr.b $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SLLSI (ZEXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 255), ANDSI (tmp_oldregval, 0xffffff00));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTQI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQQI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSLR_W_R) : /* lslr.w $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SLLSI (ZEXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
{
  SI tmp_oldregval;
  tmp_oldregval = GET_H_RAW_GR_ACR (FLD (f_operand2));
  {
    SI opval = ORSI (ANDSI (tmp_tmpd, 65535), ANDSI (tmp_oldregval, 0xffff0000));
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
{
  {
    BI opval = LTHI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQHI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSLR_D_R) : /* lslr.d $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_cnt = ANDSI (GET_H_GR (FLD (f_operand1)), 63);
  tmp_tmpd = ((NESI (ANDSI (tmp_cnt, 32), 0)) ? (0) : (SLLSI (ZEXTSISI (TRUNCSISI (GET_H_GR (FLD (f_operand2)))), ANDSI (tmp_cnt, 31))));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LSLQ) : /* lslq $c,${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_asrq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = SLLSI (GET_H_GR (FLD (f_operand2)), FLD (f_u5));
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BTST) : /* $Rs,$Rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_cnt;
  tmp_tmpd = SLLSI (GET_H_GR (FLD (f_operand2)), SUBSI (31, ANDSI (GET_H_GR (FLD (f_operand1)), 31)));
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BTSTQ) : /* btstq $c,${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_asrq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  tmp_tmpd = SLLSI (GET_H_GR (FLD (f_operand2)), SUBSI (31, FLD (f_u5)));
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SETF) : /* setf ${list-of-flags} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_setf.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = FLD (f_dstsrc);
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 0)), 0)) {
  {
    BI opval = 1;
    CPU (h_cbit) = opval;
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 1)), 0)) {
  {
    BI opval = 1;
    CPU (h_vbit) = opval;
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 2)), 0)) {
  {
    BI opval = 1;
    CPU (h_zbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 3)), 0)) {
  {
    BI opval = 1;
    CPU (h_nbit) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 4)), 0)) {
  {
    BI opval = 1;
    CPU (h_xbit) = opval;
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 5)), 0)) {
  {
    BI opval = 1;
    SET_H_IBIT (opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "ibit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 6)), 0)) {
  {
    BI opval = 1;
    SET_H_UBIT (opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "ubit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 7)), 0)) {
  {
    BI opval = 1;
    CPU (h_pbit) = opval;
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pbit", 'x', opval);
  }
}
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
if (EQSI (ANDSI (tmp_tmp, SLLSI (1, 4)), 0)) {
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CLEARF) : /* clearf ${list-of-flags} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_setf.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmp;
  tmp_tmp = FLD (f_dstsrc);
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 0)), 0)) {
  {
    BI opval = 0;
    CPU (h_cbit) = opval;
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 1)), 0)) {
  {
    BI opval = 0;
    CPU (h_vbit) = opval;
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "vbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 2)), 0)) {
  {
    BI opval = 0;
    CPU (h_zbit) = opval;
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 3)), 0)) {
  {
    BI opval = 0;
    CPU (h_nbit) = opval;
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 4)), 0)) {
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 5)), 0)) {
  {
    BI opval = 0;
    SET_H_IBIT (opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "ibit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 6)), 0)) {
  {
    BI opval = 0;
    SET_H_UBIT (opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "ubit", 'x', opval);
  }
}
if (NESI (ANDSI (tmp_tmp, SLLSI (1, 7)), 0)) {
  {
    BI opval = 0;
    CPU (h_pbit) = opval;
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pbit", 'x', opval);
  }
}
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RFE) : /* rfe */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rfe.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  USI tmp_oldccs;
  USI tmp_samebits;
  USI tmp_shiftbits;
  USI tmp_keepmask;
  BI tmp_p1;
  tmp_oldccs = GET_H_SR (((UINT) 13));
  tmp_keepmask = 0xc0000000;
  tmp_samebits = ANDSI (tmp_oldccs, tmp_keepmask);
  tmp_shiftbits = ANDSI (SRLSI (ANDSI (tmp_oldccs, 1073609728), 10), INVSI (tmp_keepmask));
  tmp_p1 = NESI (0, ANDSI (tmp_oldccs, 131072));
  {
    SI opval = ORSI (ORSI (tmp_samebits, tmp_shiftbits), ((ANDBI (CPU (h_rbit), NOTBI (tmp_p1))) ? (0) : (128)));
    SET_H_SR (((UINT) 13), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SFE) : /* sfe */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rfe.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_oldccs;
  SI tmp_savemask;
  tmp_savemask = 0xc0000000;
  tmp_oldccs = GET_H_SR (((UINT) 13));
  {
    SI opval = ORSI (ANDSI (tmp_savemask, tmp_oldccs), ANDSI (INVSI (tmp_savemask), SLLSI (tmp_oldccs, 10)));
    SET_H_SR (((UINT) 13), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RFG) : /* rfg */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

crisv32f_rfg_handler (current_cpu, pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RFN) : /* rfn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_rfe.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  USI tmp_oldccs;
  USI tmp_samebits;
  USI tmp_shiftbits;
  USI tmp_keepmask;
  BI tmp_p1;
  tmp_oldccs = GET_H_SR (((UINT) 13));
  tmp_keepmask = 0xc0000000;
  tmp_samebits = ANDSI (tmp_oldccs, tmp_keepmask);
  tmp_shiftbits = ANDSI (SRLSI (ANDSI (tmp_oldccs, 1073609728), 10), INVSI (tmp_keepmask));
  tmp_p1 = NESI (0, ANDSI (tmp_oldccs, 131072));
  {
    SI opval = ORSI (ORSI (tmp_samebits, tmp_shiftbits), ((ANDBI (CPU (h_rbit), NOTBI (tmp_p1))) ? (0) : (128)));
    SET_H_SR (((UINT) 13), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
}
  {
    BI opval = 1;
    SET_H_MBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "mbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_HALT) : /* halt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = crisv32f_halt_handler (current_cpu, pc);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCC_B) : /* b${cc} ${o-pcrel} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_truthval;
  tmp_truthval = ({   SI tmp_tmpcond;
  BI tmp_condres;
  tmp_tmpcond = FLD (f_operand2);
; if (EQSI (tmp_tmpcond, 0)) {
  tmp_condres = NOTBI (CPU (h_cbit));
}
 else if (EQSI (tmp_tmpcond, 1)) {
  tmp_condres = CPU (h_cbit);
}
 else if (EQSI (tmp_tmpcond, 2)) {
  tmp_condres = NOTBI (CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 3)) {
  tmp_condres = CPU (h_zbit);
}
 else if (EQSI (tmp_tmpcond, 4)) {
  tmp_condres = NOTBI (CPU (h_vbit));
}
 else if (EQSI (tmp_tmpcond, 5)) {
  tmp_condres = CPU (h_vbit);
}
 else if (EQSI (tmp_tmpcond, 6)) {
  tmp_condres = NOTBI (CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 7)) {
  tmp_condres = CPU (h_nbit);
}
 else if (EQSI (tmp_tmpcond, 8)) {
  tmp_condres = ORBI (CPU (h_cbit), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 9)) {
  tmp_condres = NOTBI (ORBI (CPU (h_cbit), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 10)) {
  tmp_condres = NOTBI (XORBI (CPU (h_vbit), CPU (h_nbit)));
}
 else if (EQSI (tmp_tmpcond, 11)) {
  tmp_condres = XORBI (CPU (h_vbit), CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 12)) {
  tmp_condres = NOTBI (ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 13)) {
  tmp_condres = ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 14)) {
  tmp_condres = 1;
}
 else if (EQSI (tmp_tmpcond, 15)) {
  tmp_condres = CPU (h_pbit);
}
; tmp_condres; });
crisv32f_branch_taken (current_cpu, pc, FLD (i_o_pcrel), tmp_truthval);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
if (tmp_truthval) {
{
  {
    USI opval = FLD (i_o_pcrel);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BA_B) : /* ba ${o-pcrel} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
  {
    USI opval = FLD (i_o_pcrel);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCC_W) : /* b${cc} ${o-word-pcrel} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_truthval;
  tmp_truthval = ({   SI tmp_tmpcond;
  BI tmp_condres;
  tmp_tmpcond = FLD (f_operand2);
; if (EQSI (tmp_tmpcond, 0)) {
  tmp_condres = NOTBI (CPU (h_cbit));
}
 else if (EQSI (tmp_tmpcond, 1)) {
  tmp_condres = CPU (h_cbit);
}
 else if (EQSI (tmp_tmpcond, 2)) {
  tmp_condres = NOTBI (CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 3)) {
  tmp_condres = CPU (h_zbit);
}
 else if (EQSI (tmp_tmpcond, 4)) {
  tmp_condres = NOTBI (CPU (h_vbit));
}
 else if (EQSI (tmp_tmpcond, 5)) {
  tmp_condres = CPU (h_vbit);
}
 else if (EQSI (tmp_tmpcond, 6)) {
  tmp_condres = NOTBI (CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 7)) {
  tmp_condres = CPU (h_nbit);
}
 else if (EQSI (tmp_tmpcond, 8)) {
  tmp_condres = ORBI (CPU (h_cbit), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 9)) {
  tmp_condres = NOTBI (ORBI (CPU (h_cbit), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 10)) {
  tmp_condres = NOTBI (XORBI (CPU (h_vbit), CPU (h_nbit)));
}
 else if (EQSI (tmp_tmpcond, 11)) {
  tmp_condres = XORBI (CPU (h_vbit), CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 12)) {
  tmp_condres = NOTBI (ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 13)) {
  tmp_condres = ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 14)) {
  tmp_condres = 1;
}
 else if (EQSI (tmp_tmpcond, 15)) {
  tmp_condres = CPU (h_pbit);
}
; tmp_condres; });
crisv32f_branch_taken (current_cpu, pc, FLD (i_o_word_pcrel), tmp_truthval);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
if (tmp_truthval) {
{
  {
    USI opval = FLD (i_o_word_pcrel);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BA_W) : /* ba ${o-word-pcrel} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
  {
    USI opval = FLD (i_o_word_pcrel);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JAS_R) : /* jas ${Rs},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
if (ANDIF (EQSI (FLD (f_operand1), 1), EQSI (FLD (f_operand2), 11))) {
cris_flush_simulator_decode_cache (current_cpu, pc);
}
{
{
  {
    SI opval = ADDSI (pc, 4);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = GET_H_GR (FLD (f_operand1));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JAS_C) : /* jas ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = FLD (f_indir_pc__dword);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JUMP_P) : /* jump ${Ps} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
  {
    USI opval = GET_H_SR (FLD (f_operand2));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BAS_C) : /* bas ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bas_c.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = FLD (i_const32_pcrel);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JASC_R) : /* jasc ${Rs},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = GET_H_GR (FLD (f_operand1));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JASC_C) : /* jasc ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
{
  {
    SI opval = ADDSI (pc, 12);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = FLD (f_indir_pc__dword);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BASC_C) : /* basc ${const32},${Pd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bas_c.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
{
{
  {
    SI opval = ADDSI (pc, 12);
    SET_H_SR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }
  {
    USI opval = FLD (i_const32_pcrel);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BREAK) : /* break $n */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_break.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
  {
    USI opval = crisv32f_break_handler (current_cpu, FLD (f_u4), pc);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_R_B_R) : /* bound-r.b ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_R_W_R) : /* bound-r.w ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = ZEXTHISI (TRUNCSIHI (GET_H_GR (FLD (f_operand1))));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_R_D_R) : /* bound-r.d ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = TRUNCSISI (GET_H_GR (FLD (f_operand1)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_CB) : /* bound.b [PC+],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = ZEXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte)));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_CW) : /* bound.w [PC+],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = ZEXTSISI (FLD (f_indir_pc__word));
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BOUND_CD) : /* bound.d [PC+],${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  SI tmp_tmpopd;
  SI tmp_tmpops;
  SI tmp_newval;
  tmp_tmpops = FLD (f_indir_pc__dword);
  tmp_tmpopd = GET_H_GR (FLD (f_operand2));
  tmp_newval = ((LTUSI (tmp_tmpops, tmp_tmpopd)) ? (tmp_tmpops) : (tmp_tmpopd));
  {
    SI opval = tmp_newval;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_newval, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_newval, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SCC) : /* s${cc} ${Rd-sfield} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_truthval;
  tmp_truthval = ({   SI tmp_tmpcond;
  BI tmp_condres;
  tmp_tmpcond = FLD (f_operand2);
; if (EQSI (tmp_tmpcond, 0)) {
  tmp_condres = NOTBI (CPU (h_cbit));
}
 else if (EQSI (tmp_tmpcond, 1)) {
  tmp_condres = CPU (h_cbit);
}
 else if (EQSI (tmp_tmpcond, 2)) {
  tmp_condres = NOTBI (CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 3)) {
  tmp_condres = CPU (h_zbit);
}
 else if (EQSI (tmp_tmpcond, 4)) {
  tmp_condres = NOTBI (CPU (h_vbit));
}
 else if (EQSI (tmp_tmpcond, 5)) {
  tmp_condres = CPU (h_vbit);
}
 else if (EQSI (tmp_tmpcond, 6)) {
  tmp_condres = NOTBI (CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 7)) {
  tmp_condres = CPU (h_nbit);
}
 else if (EQSI (tmp_tmpcond, 8)) {
  tmp_condres = ORBI (CPU (h_cbit), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 9)) {
  tmp_condres = NOTBI (ORBI (CPU (h_cbit), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 10)) {
  tmp_condres = NOTBI (XORBI (CPU (h_vbit), CPU (h_nbit)));
}
 else if (EQSI (tmp_tmpcond, 11)) {
  tmp_condres = XORBI (CPU (h_vbit), CPU (h_nbit));
}
 else if (EQSI (tmp_tmpcond, 12)) {
  tmp_condres = NOTBI (ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit)));
}
 else if (EQSI (tmp_tmpcond, 13)) {
  tmp_condres = ORBI (XORBI (CPU (h_vbit), CPU (h_nbit)), CPU (h_zbit));
}
 else if (EQSI (tmp_tmpcond, 14)) {
  tmp_condres = 1;
}
 else if (EQSI (tmp_tmpcond, 15)) {
  tmp_condres = CPU (h_pbit);
}
; tmp_condres; });
  {
    SI opval = ZEXTBISI (tmp_truthval);
    SET_H_GR (FLD (f_operand1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LZ) : /* lz ${Rs},${Rd} */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpd;
  SI tmp_tmp;
  tmp_tmp = GET_H_GR (FLD (f_operand1));
  tmp_tmpd = 0;
{
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
if (GESI (tmp_tmp, 0)) {
{
  tmp_tmp = SLLSI (tmp_tmp, 1);
  tmp_tmpd = ADDSI (tmp_tmpd, 1);
}
}
}
  {
    SI opval = tmp_tmpd;
    SET_H_GR (FLD (f_operand2), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    BI opval = LTSI (tmp_tmpd, 0);
    CPU (h_nbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "nbit", 'x', opval);
  }
  {
    BI opval = ANDIF (EQSI (tmp_tmpd, 0), ((CPU (h_xbit)) ? (CPU (h_zbit)) : (1)));
    CPU (h_zbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "zbit", 'x', opval);
  }
SET_H_CBIT_MOVE (0);
SET_H_VBIT_MOVE (0);
{
  {
    BI opval = 0;
    CPU (h_xbit) = opval;
    TRACE_RESULT (current_cpu, abuf, "xbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}
}
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDOQ) : /* addoq $o,$Rs,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addoq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), FLD (f_s8));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_M_B_M) : /* addo-m.b [${Rs}${inc}],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_tmps;
  tmp_tmps = ({   SI tmp_addr;
  QI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMQI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 1);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), EXTQISI (tmp_tmps));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_M_W_M) : /* addo-m.w [${Rs}${inc}],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_tmps;
  tmp_tmps = ({   SI tmp_addr;
  HI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMHI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 2);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), EXTHISI (tmp_tmps));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_M_D_M) : /* addo-m.d [${Rs}${inc}],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addc_m.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmps;
  tmp_tmps = ({   SI tmp_addr;
  SI tmp_tmp_mem;
  BI tmp_postinc;
  tmp_postinc = FLD (f_memmode);
;   tmp_addr = ((EQBI (GET_H_INSN_PREFIXED_P (), 0)) ? (GET_H_GR (FLD (f_operand1))) : (GET_H_PREFIXREG_V32 ()));
;   tmp_tmp_mem = GETMEMSI (current_cpu, pc, tmp_addr);
; if (NEBI (tmp_postinc, 0)) {
{
if (EQBI (GET_H_INSN_PREFIXED_P (), 0)) {
  tmp_addr = ADDSI (tmp_addr, 4);
}
  {
    SI opval = tmp_addr;
    SET_H_GR (FLD (f_operand1), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
; tmp_tmp_mem; });
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), tmp_tmps);
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_CB) : /* addo.b [PC+],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), EXTQISI (TRUNCSIQI (FLD (f_indir_pc__byte))));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_CW) : /* addo.w [PC+],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), EXTHISI (TRUNCSIHI (FLD (f_indir_pc__word))));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDO_CD) : /* addo.d [PC+],$Rd,ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 6);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand2)), FLD (f_indir_pc__dword));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_ACR_B_R) : /* addi-acr.b ${Rs-dfield}.m,${Rd-sfield},ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 1));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_ACR_W_R) : /* addi-acr.w ${Rs-dfield}.m,${Rd-sfield},ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 2));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_ACR_D_R) : /* addi-acr.d ${Rs-dfield}.m,${Rd-sfield},ACR */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_muls_b.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_operand1)), MULSI (GET_H_GR (FLD (f_operand2)), 4));
    SET_H_PREFIXREG_V32 (opval);
    TRACE_RESULT (current_cpu, abuf, "prefixreg", 'x', opval);
  }
  {
    BI opval = 1;
    SET_H_INSN_PREFIXED_P (opval);
    TRACE_RESULT (current_cpu, abuf, "insn-prefixed-p", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FIDXI) : /* fidxi [$Rs] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = crisv32f_fidxi_handler (current_cpu, pc, GET_H_GR (FLD (f_operand1)));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTAGI) : /* fidxi [$Rs] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = crisv32f_ftagi_handler (current_cpu, pc, GET_H_GR (FLD (f_operand1)));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FIDXD) : /* fidxd [$Rs] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = crisv32f_fidxd_handler (current_cpu, pc, GET_H_GR (FLD (f_operand1)));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTAGD) : /* ftagd [$Rs] */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mcp.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = crisv32f_ftagd_handler (current_cpu, pc, GET_H_GR (FLD (f_operand1)));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);


    }
  ENDSWITCH (sem) /* End of semantic switch.  */

  /* At this point `vpc' contains the next insn to execute.  */
}

#undef DEFINE_SWITCH
#endif /* DEFINE_SWITCH */
