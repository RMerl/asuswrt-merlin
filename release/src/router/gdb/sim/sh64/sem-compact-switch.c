/* Simulator instruction semantics for sh64.

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
    { SH64_COMPACT_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { SH64_COMPACT_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { SH64_COMPACT_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { SH64_COMPACT_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { SH64_COMPACT_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { SH64_COMPACT_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { SH64_COMPACT_INSN_ADD_COMPACT, && case_sem_INSN_ADD_COMPACT },
    { SH64_COMPACT_INSN_ADDI_COMPACT, && case_sem_INSN_ADDI_COMPACT },
    { SH64_COMPACT_INSN_ADDC_COMPACT, && case_sem_INSN_ADDC_COMPACT },
    { SH64_COMPACT_INSN_ADDV_COMPACT, && case_sem_INSN_ADDV_COMPACT },
    { SH64_COMPACT_INSN_AND_COMPACT, && case_sem_INSN_AND_COMPACT },
    { SH64_COMPACT_INSN_ANDI_COMPACT, && case_sem_INSN_ANDI_COMPACT },
    { SH64_COMPACT_INSN_ANDB_COMPACT, && case_sem_INSN_ANDB_COMPACT },
    { SH64_COMPACT_INSN_BF_COMPACT, && case_sem_INSN_BF_COMPACT },
    { SH64_COMPACT_INSN_BFS_COMPACT, && case_sem_INSN_BFS_COMPACT },
    { SH64_COMPACT_INSN_BRA_COMPACT, && case_sem_INSN_BRA_COMPACT },
    { SH64_COMPACT_INSN_BRAF_COMPACT, && case_sem_INSN_BRAF_COMPACT },
    { SH64_COMPACT_INSN_BRK_COMPACT, && case_sem_INSN_BRK_COMPACT },
    { SH64_COMPACT_INSN_BSR_COMPACT, && case_sem_INSN_BSR_COMPACT },
    { SH64_COMPACT_INSN_BSRF_COMPACT, && case_sem_INSN_BSRF_COMPACT },
    { SH64_COMPACT_INSN_BT_COMPACT, && case_sem_INSN_BT_COMPACT },
    { SH64_COMPACT_INSN_BTS_COMPACT, && case_sem_INSN_BTS_COMPACT },
    { SH64_COMPACT_INSN_CLRMAC_COMPACT, && case_sem_INSN_CLRMAC_COMPACT },
    { SH64_COMPACT_INSN_CLRS_COMPACT, && case_sem_INSN_CLRS_COMPACT },
    { SH64_COMPACT_INSN_CLRT_COMPACT, && case_sem_INSN_CLRT_COMPACT },
    { SH64_COMPACT_INSN_CMPEQ_COMPACT, && case_sem_INSN_CMPEQ_COMPACT },
    { SH64_COMPACT_INSN_CMPEQI_COMPACT, && case_sem_INSN_CMPEQI_COMPACT },
    { SH64_COMPACT_INSN_CMPGE_COMPACT, && case_sem_INSN_CMPGE_COMPACT },
    { SH64_COMPACT_INSN_CMPGT_COMPACT, && case_sem_INSN_CMPGT_COMPACT },
    { SH64_COMPACT_INSN_CMPHI_COMPACT, && case_sem_INSN_CMPHI_COMPACT },
    { SH64_COMPACT_INSN_CMPHS_COMPACT, && case_sem_INSN_CMPHS_COMPACT },
    { SH64_COMPACT_INSN_CMPPL_COMPACT, && case_sem_INSN_CMPPL_COMPACT },
    { SH64_COMPACT_INSN_CMPPZ_COMPACT, && case_sem_INSN_CMPPZ_COMPACT },
    { SH64_COMPACT_INSN_CMPSTR_COMPACT, && case_sem_INSN_CMPSTR_COMPACT },
    { SH64_COMPACT_INSN_DIV0S_COMPACT, && case_sem_INSN_DIV0S_COMPACT },
    { SH64_COMPACT_INSN_DIV0U_COMPACT, && case_sem_INSN_DIV0U_COMPACT },
    { SH64_COMPACT_INSN_DIV1_COMPACT, && case_sem_INSN_DIV1_COMPACT },
    { SH64_COMPACT_INSN_DIVU_COMPACT, && case_sem_INSN_DIVU_COMPACT },
    { SH64_COMPACT_INSN_MULR_COMPACT, && case_sem_INSN_MULR_COMPACT },
    { SH64_COMPACT_INSN_DMULSL_COMPACT, && case_sem_INSN_DMULSL_COMPACT },
    { SH64_COMPACT_INSN_DMULUL_COMPACT, && case_sem_INSN_DMULUL_COMPACT },
    { SH64_COMPACT_INSN_DT_COMPACT, && case_sem_INSN_DT_COMPACT },
    { SH64_COMPACT_INSN_EXTSB_COMPACT, && case_sem_INSN_EXTSB_COMPACT },
    { SH64_COMPACT_INSN_EXTSW_COMPACT, && case_sem_INSN_EXTSW_COMPACT },
    { SH64_COMPACT_INSN_EXTUB_COMPACT, && case_sem_INSN_EXTUB_COMPACT },
    { SH64_COMPACT_INSN_EXTUW_COMPACT, && case_sem_INSN_EXTUW_COMPACT },
    { SH64_COMPACT_INSN_FABS_COMPACT, && case_sem_INSN_FABS_COMPACT },
    { SH64_COMPACT_INSN_FADD_COMPACT, && case_sem_INSN_FADD_COMPACT },
    { SH64_COMPACT_INSN_FCMPEQ_COMPACT, && case_sem_INSN_FCMPEQ_COMPACT },
    { SH64_COMPACT_INSN_FCMPGT_COMPACT, && case_sem_INSN_FCMPGT_COMPACT },
    { SH64_COMPACT_INSN_FCNVDS_COMPACT, && case_sem_INSN_FCNVDS_COMPACT },
    { SH64_COMPACT_INSN_FCNVSD_COMPACT, && case_sem_INSN_FCNVSD_COMPACT },
    { SH64_COMPACT_INSN_FDIV_COMPACT, && case_sem_INSN_FDIV_COMPACT },
    { SH64_COMPACT_INSN_FIPR_COMPACT, && case_sem_INSN_FIPR_COMPACT },
    { SH64_COMPACT_INSN_FLDS_COMPACT, && case_sem_INSN_FLDS_COMPACT },
    { SH64_COMPACT_INSN_FLDI0_COMPACT, && case_sem_INSN_FLDI0_COMPACT },
    { SH64_COMPACT_INSN_FLDI1_COMPACT, && case_sem_INSN_FLDI1_COMPACT },
    { SH64_COMPACT_INSN_FLOAT_COMPACT, && case_sem_INSN_FLOAT_COMPACT },
    { SH64_COMPACT_INSN_FMAC_COMPACT, && case_sem_INSN_FMAC_COMPACT },
    { SH64_COMPACT_INSN_FMOV1_COMPACT, && case_sem_INSN_FMOV1_COMPACT },
    { SH64_COMPACT_INSN_FMOV2_COMPACT, && case_sem_INSN_FMOV2_COMPACT },
    { SH64_COMPACT_INSN_FMOV3_COMPACT, && case_sem_INSN_FMOV3_COMPACT },
    { SH64_COMPACT_INSN_FMOV4_COMPACT, && case_sem_INSN_FMOV4_COMPACT },
    { SH64_COMPACT_INSN_FMOV5_COMPACT, && case_sem_INSN_FMOV5_COMPACT },
    { SH64_COMPACT_INSN_FMOV6_COMPACT, && case_sem_INSN_FMOV6_COMPACT },
    { SH64_COMPACT_INSN_FMOV7_COMPACT, && case_sem_INSN_FMOV7_COMPACT },
    { SH64_COMPACT_INSN_FMOV8_COMPACT, && case_sem_INSN_FMOV8_COMPACT },
    { SH64_COMPACT_INSN_FMOV9_COMPACT, && case_sem_INSN_FMOV9_COMPACT },
    { SH64_COMPACT_INSN_FMUL_COMPACT, && case_sem_INSN_FMUL_COMPACT },
    { SH64_COMPACT_INSN_FNEG_COMPACT, && case_sem_INSN_FNEG_COMPACT },
    { SH64_COMPACT_INSN_FRCHG_COMPACT, && case_sem_INSN_FRCHG_COMPACT },
    { SH64_COMPACT_INSN_FSCHG_COMPACT, && case_sem_INSN_FSCHG_COMPACT },
    { SH64_COMPACT_INSN_FSQRT_COMPACT, && case_sem_INSN_FSQRT_COMPACT },
    { SH64_COMPACT_INSN_FSTS_COMPACT, && case_sem_INSN_FSTS_COMPACT },
    { SH64_COMPACT_INSN_FSUB_COMPACT, && case_sem_INSN_FSUB_COMPACT },
    { SH64_COMPACT_INSN_FTRC_COMPACT, && case_sem_INSN_FTRC_COMPACT },
    { SH64_COMPACT_INSN_FTRV_COMPACT, && case_sem_INSN_FTRV_COMPACT },
    { SH64_COMPACT_INSN_JMP_COMPACT, && case_sem_INSN_JMP_COMPACT },
    { SH64_COMPACT_INSN_JSR_COMPACT, && case_sem_INSN_JSR_COMPACT },
    { SH64_COMPACT_INSN_LDC_GBR_COMPACT, && case_sem_INSN_LDC_GBR_COMPACT },
    { SH64_COMPACT_INSN_LDC_VBR_COMPACT, && case_sem_INSN_LDC_VBR_COMPACT },
    { SH64_COMPACT_INSN_LDC_SR_COMPACT, && case_sem_INSN_LDC_SR_COMPACT },
    { SH64_COMPACT_INSN_LDCL_GBR_COMPACT, && case_sem_INSN_LDCL_GBR_COMPACT },
    { SH64_COMPACT_INSN_LDCL_VBR_COMPACT, && case_sem_INSN_LDCL_VBR_COMPACT },
    { SH64_COMPACT_INSN_LDS_FPSCR_COMPACT, && case_sem_INSN_LDS_FPSCR_COMPACT },
    { SH64_COMPACT_INSN_LDSL_FPSCR_COMPACT, && case_sem_INSN_LDSL_FPSCR_COMPACT },
    { SH64_COMPACT_INSN_LDS_FPUL_COMPACT, && case_sem_INSN_LDS_FPUL_COMPACT },
    { SH64_COMPACT_INSN_LDSL_FPUL_COMPACT, && case_sem_INSN_LDSL_FPUL_COMPACT },
    { SH64_COMPACT_INSN_LDS_MACH_COMPACT, && case_sem_INSN_LDS_MACH_COMPACT },
    { SH64_COMPACT_INSN_LDSL_MACH_COMPACT, && case_sem_INSN_LDSL_MACH_COMPACT },
    { SH64_COMPACT_INSN_LDS_MACL_COMPACT, && case_sem_INSN_LDS_MACL_COMPACT },
    { SH64_COMPACT_INSN_LDSL_MACL_COMPACT, && case_sem_INSN_LDSL_MACL_COMPACT },
    { SH64_COMPACT_INSN_LDS_PR_COMPACT, && case_sem_INSN_LDS_PR_COMPACT },
    { SH64_COMPACT_INSN_LDSL_PR_COMPACT, && case_sem_INSN_LDSL_PR_COMPACT },
    { SH64_COMPACT_INSN_MACL_COMPACT, && case_sem_INSN_MACL_COMPACT },
    { SH64_COMPACT_INSN_MACW_COMPACT, && case_sem_INSN_MACW_COMPACT },
    { SH64_COMPACT_INSN_MOV_COMPACT, && case_sem_INSN_MOV_COMPACT },
    { SH64_COMPACT_INSN_MOVI_COMPACT, && case_sem_INSN_MOVI_COMPACT },
    { SH64_COMPACT_INSN_MOVI20_COMPACT, && case_sem_INSN_MOVI20_COMPACT },
    { SH64_COMPACT_INSN_MOVB1_COMPACT, && case_sem_INSN_MOVB1_COMPACT },
    { SH64_COMPACT_INSN_MOVB2_COMPACT, && case_sem_INSN_MOVB2_COMPACT },
    { SH64_COMPACT_INSN_MOVB3_COMPACT, && case_sem_INSN_MOVB3_COMPACT },
    { SH64_COMPACT_INSN_MOVB4_COMPACT, && case_sem_INSN_MOVB4_COMPACT },
    { SH64_COMPACT_INSN_MOVB5_COMPACT, && case_sem_INSN_MOVB5_COMPACT },
    { SH64_COMPACT_INSN_MOVB6_COMPACT, && case_sem_INSN_MOVB6_COMPACT },
    { SH64_COMPACT_INSN_MOVB7_COMPACT, && case_sem_INSN_MOVB7_COMPACT },
    { SH64_COMPACT_INSN_MOVB8_COMPACT, && case_sem_INSN_MOVB8_COMPACT },
    { SH64_COMPACT_INSN_MOVB9_COMPACT, && case_sem_INSN_MOVB9_COMPACT },
    { SH64_COMPACT_INSN_MOVB10_COMPACT, && case_sem_INSN_MOVB10_COMPACT },
    { SH64_COMPACT_INSN_MOVL1_COMPACT, && case_sem_INSN_MOVL1_COMPACT },
    { SH64_COMPACT_INSN_MOVL2_COMPACT, && case_sem_INSN_MOVL2_COMPACT },
    { SH64_COMPACT_INSN_MOVL3_COMPACT, && case_sem_INSN_MOVL3_COMPACT },
    { SH64_COMPACT_INSN_MOVL4_COMPACT, && case_sem_INSN_MOVL4_COMPACT },
    { SH64_COMPACT_INSN_MOVL5_COMPACT, && case_sem_INSN_MOVL5_COMPACT },
    { SH64_COMPACT_INSN_MOVL6_COMPACT, && case_sem_INSN_MOVL6_COMPACT },
    { SH64_COMPACT_INSN_MOVL7_COMPACT, && case_sem_INSN_MOVL7_COMPACT },
    { SH64_COMPACT_INSN_MOVL8_COMPACT, && case_sem_INSN_MOVL8_COMPACT },
    { SH64_COMPACT_INSN_MOVL9_COMPACT, && case_sem_INSN_MOVL9_COMPACT },
    { SH64_COMPACT_INSN_MOVL10_COMPACT, && case_sem_INSN_MOVL10_COMPACT },
    { SH64_COMPACT_INSN_MOVL11_COMPACT, && case_sem_INSN_MOVL11_COMPACT },
    { SH64_COMPACT_INSN_MOVL12_COMPACT, && case_sem_INSN_MOVL12_COMPACT },
    { SH64_COMPACT_INSN_MOVL13_COMPACT, && case_sem_INSN_MOVL13_COMPACT },
    { SH64_COMPACT_INSN_MOVW1_COMPACT, && case_sem_INSN_MOVW1_COMPACT },
    { SH64_COMPACT_INSN_MOVW2_COMPACT, && case_sem_INSN_MOVW2_COMPACT },
    { SH64_COMPACT_INSN_MOVW3_COMPACT, && case_sem_INSN_MOVW3_COMPACT },
    { SH64_COMPACT_INSN_MOVW4_COMPACT, && case_sem_INSN_MOVW4_COMPACT },
    { SH64_COMPACT_INSN_MOVW5_COMPACT, && case_sem_INSN_MOVW5_COMPACT },
    { SH64_COMPACT_INSN_MOVW6_COMPACT, && case_sem_INSN_MOVW6_COMPACT },
    { SH64_COMPACT_INSN_MOVW7_COMPACT, && case_sem_INSN_MOVW7_COMPACT },
    { SH64_COMPACT_INSN_MOVW8_COMPACT, && case_sem_INSN_MOVW8_COMPACT },
    { SH64_COMPACT_INSN_MOVW9_COMPACT, && case_sem_INSN_MOVW9_COMPACT },
    { SH64_COMPACT_INSN_MOVW10_COMPACT, && case_sem_INSN_MOVW10_COMPACT },
    { SH64_COMPACT_INSN_MOVW11_COMPACT, && case_sem_INSN_MOVW11_COMPACT },
    { SH64_COMPACT_INSN_MOVA_COMPACT, && case_sem_INSN_MOVA_COMPACT },
    { SH64_COMPACT_INSN_MOVCAL_COMPACT, && case_sem_INSN_MOVCAL_COMPACT },
    { SH64_COMPACT_INSN_MOVCOL_COMPACT, && case_sem_INSN_MOVCOL_COMPACT },
    { SH64_COMPACT_INSN_MOVT_COMPACT, && case_sem_INSN_MOVT_COMPACT },
    { SH64_COMPACT_INSN_MOVUAL_COMPACT, && case_sem_INSN_MOVUAL_COMPACT },
    { SH64_COMPACT_INSN_MOVUAL2_COMPACT, && case_sem_INSN_MOVUAL2_COMPACT },
    { SH64_COMPACT_INSN_MULL_COMPACT, && case_sem_INSN_MULL_COMPACT },
    { SH64_COMPACT_INSN_MULSW_COMPACT, && case_sem_INSN_MULSW_COMPACT },
    { SH64_COMPACT_INSN_MULUW_COMPACT, && case_sem_INSN_MULUW_COMPACT },
    { SH64_COMPACT_INSN_NEG_COMPACT, && case_sem_INSN_NEG_COMPACT },
    { SH64_COMPACT_INSN_NEGC_COMPACT, && case_sem_INSN_NEGC_COMPACT },
    { SH64_COMPACT_INSN_NOP_COMPACT, && case_sem_INSN_NOP_COMPACT },
    { SH64_COMPACT_INSN_NOT_COMPACT, && case_sem_INSN_NOT_COMPACT },
    { SH64_COMPACT_INSN_OCBI_COMPACT, && case_sem_INSN_OCBI_COMPACT },
    { SH64_COMPACT_INSN_OCBP_COMPACT, && case_sem_INSN_OCBP_COMPACT },
    { SH64_COMPACT_INSN_OCBWB_COMPACT, && case_sem_INSN_OCBWB_COMPACT },
    { SH64_COMPACT_INSN_OR_COMPACT, && case_sem_INSN_OR_COMPACT },
    { SH64_COMPACT_INSN_ORI_COMPACT, && case_sem_INSN_ORI_COMPACT },
    { SH64_COMPACT_INSN_ORB_COMPACT, && case_sem_INSN_ORB_COMPACT },
    { SH64_COMPACT_INSN_PREF_COMPACT, && case_sem_INSN_PREF_COMPACT },
    { SH64_COMPACT_INSN_ROTCL_COMPACT, && case_sem_INSN_ROTCL_COMPACT },
    { SH64_COMPACT_INSN_ROTCR_COMPACT, && case_sem_INSN_ROTCR_COMPACT },
    { SH64_COMPACT_INSN_ROTL_COMPACT, && case_sem_INSN_ROTL_COMPACT },
    { SH64_COMPACT_INSN_ROTR_COMPACT, && case_sem_INSN_ROTR_COMPACT },
    { SH64_COMPACT_INSN_RTS_COMPACT, && case_sem_INSN_RTS_COMPACT },
    { SH64_COMPACT_INSN_SETS_COMPACT, && case_sem_INSN_SETS_COMPACT },
    { SH64_COMPACT_INSN_SETT_COMPACT, && case_sem_INSN_SETT_COMPACT },
    { SH64_COMPACT_INSN_SHAD_COMPACT, && case_sem_INSN_SHAD_COMPACT },
    { SH64_COMPACT_INSN_SHAL_COMPACT, && case_sem_INSN_SHAL_COMPACT },
    { SH64_COMPACT_INSN_SHAR_COMPACT, && case_sem_INSN_SHAR_COMPACT },
    { SH64_COMPACT_INSN_SHLD_COMPACT, && case_sem_INSN_SHLD_COMPACT },
    { SH64_COMPACT_INSN_SHLL_COMPACT, && case_sem_INSN_SHLL_COMPACT },
    { SH64_COMPACT_INSN_SHLL2_COMPACT, && case_sem_INSN_SHLL2_COMPACT },
    { SH64_COMPACT_INSN_SHLL8_COMPACT, && case_sem_INSN_SHLL8_COMPACT },
    { SH64_COMPACT_INSN_SHLL16_COMPACT, && case_sem_INSN_SHLL16_COMPACT },
    { SH64_COMPACT_INSN_SHLR_COMPACT, && case_sem_INSN_SHLR_COMPACT },
    { SH64_COMPACT_INSN_SHLR2_COMPACT, && case_sem_INSN_SHLR2_COMPACT },
    { SH64_COMPACT_INSN_SHLR8_COMPACT, && case_sem_INSN_SHLR8_COMPACT },
    { SH64_COMPACT_INSN_SHLR16_COMPACT, && case_sem_INSN_SHLR16_COMPACT },
    { SH64_COMPACT_INSN_STC_GBR_COMPACT, && case_sem_INSN_STC_GBR_COMPACT },
    { SH64_COMPACT_INSN_STC_VBR_COMPACT, && case_sem_INSN_STC_VBR_COMPACT },
    { SH64_COMPACT_INSN_STCL_GBR_COMPACT, && case_sem_INSN_STCL_GBR_COMPACT },
    { SH64_COMPACT_INSN_STCL_VBR_COMPACT, && case_sem_INSN_STCL_VBR_COMPACT },
    { SH64_COMPACT_INSN_STS_FPSCR_COMPACT, && case_sem_INSN_STS_FPSCR_COMPACT },
    { SH64_COMPACT_INSN_STSL_FPSCR_COMPACT, && case_sem_INSN_STSL_FPSCR_COMPACT },
    { SH64_COMPACT_INSN_STS_FPUL_COMPACT, && case_sem_INSN_STS_FPUL_COMPACT },
    { SH64_COMPACT_INSN_STSL_FPUL_COMPACT, && case_sem_INSN_STSL_FPUL_COMPACT },
    { SH64_COMPACT_INSN_STS_MACH_COMPACT, && case_sem_INSN_STS_MACH_COMPACT },
    { SH64_COMPACT_INSN_STSL_MACH_COMPACT, && case_sem_INSN_STSL_MACH_COMPACT },
    { SH64_COMPACT_INSN_STS_MACL_COMPACT, && case_sem_INSN_STS_MACL_COMPACT },
    { SH64_COMPACT_INSN_STSL_MACL_COMPACT, && case_sem_INSN_STSL_MACL_COMPACT },
    { SH64_COMPACT_INSN_STS_PR_COMPACT, && case_sem_INSN_STS_PR_COMPACT },
    { SH64_COMPACT_INSN_STSL_PR_COMPACT, && case_sem_INSN_STSL_PR_COMPACT },
    { SH64_COMPACT_INSN_SUB_COMPACT, && case_sem_INSN_SUB_COMPACT },
    { SH64_COMPACT_INSN_SUBC_COMPACT, && case_sem_INSN_SUBC_COMPACT },
    { SH64_COMPACT_INSN_SUBV_COMPACT, && case_sem_INSN_SUBV_COMPACT },
    { SH64_COMPACT_INSN_SWAPB_COMPACT, && case_sem_INSN_SWAPB_COMPACT },
    { SH64_COMPACT_INSN_SWAPW_COMPACT, && case_sem_INSN_SWAPW_COMPACT },
    { SH64_COMPACT_INSN_TASB_COMPACT, && case_sem_INSN_TASB_COMPACT },
    { SH64_COMPACT_INSN_TRAPA_COMPACT, && case_sem_INSN_TRAPA_COMPACT },
    { SH64_COMPACT_INSN_TST_COMPACT, && case_sem_INSN_TST_COMPACT },
    { SH64_COMPACT_INSN_TSTI_COMPACT, && case_sem_INSN_TSTI_COMPACT },
    { SH64_COMPACT_INSN_TSTB_COMPACT, && case_sem_INSN_TSTB_COMPACT },
    { SH64_COMPACT_INSN_XOR_COMPACT, && case_sem_INSN_XOR_COMPACT },
    { SH64_COMPACT_INSN_XORI_COMPACT, && case_sem_INSN_XORI_COMPACT },
    { SH64_COMPACT_INSN_XORB_COMPACT, && case_sem_INSN_XORB_COMPACT },
    { SH64_COMPACT_INSN_XTRCT_COMPACT, && case_sem_INSN_XTRCT_COMPACT },
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
#if WITH_SCACHE_PBB_SH64_COMPACT
    sh64_compact_pbb_after (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_SH64_COMPACT
    sh64_compact_pbb_before (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_SH64_COMPACT
#ifdef DEFINE_SWITCH
    vpc = sh64_compact_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = sh64_compact_pbb_cti_chain (current_cpu, sem_arg,
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
#if WITH_SCACHE_PBB_SH64_COMPACT
    vpc = sh64_compact_pbb_chain (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_SH64_COMPACT
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = sh64_compact_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = sh64_compact_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = sh64_compact_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD_COMPACT) : /* add $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI_COMPACT) : /* add #$imm8, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), EXTQISI (ANDQI (FLD (f_imm8), 255)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDC_COMPACT) : /* addc $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_flag;
  tmp_flag = ADDCFSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
  {
    SI opval = ADDCSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = tmp_flag;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDV_COMPACT) : /* addv $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = ADDOFSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), 0);
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = tmp_t;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND_COMPACT) : /* and $rm64, $rn64 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_rm)), GET_H_GR (FLD (f_rn)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDI_COMPACT) : /* and #$uimm8, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDB_COMPACT) : /* and.b #$imm8, @(r0, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  UQI tmp_data;
  tmp_addr = ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GBR ());
  tmp_data = ANDQI (GETMEMUQI (current_cpu, pc, tmp_addr), FLD (f_imm8));
  {
    UQI opval = tmp_data;
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BF_COMPACT) : /* bf $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_TBIT ())) {
  {
    UDI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BFS_COMPACT) : /* bf/s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_TBIT ())) {
{
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRA_COMPACT) : /* bra $disp12 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bra_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = FLD (i_disp12);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRAF_COMPACT) : /* braf $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = ADDDI (EXTSIDI (GET_H_GRC (FLD (f_rn))), ADDDI (pc, 4));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRK_COMPACT) : /* brk */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_break (current_cpu, pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BSR_COMPACT) : /* bsr $disp12 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bra_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    SI opval = ADDDI (pc, 4);
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }
}
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = FLD (i_disp12);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BSRF_COMPACT) : /* bsrf $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    SI opval = ADDDI (pc, 4);
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }
}
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = ADDDI (EXTSIDI (GET_H_GRC (FLD (f_rn))), ADDDI (pc, 4));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BT_COMPACT) : /* bt $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_TBIT ()) {
  {
    UDI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BTS_COMPACT) : /* bt/s $disp8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_TBIT ()) {
{
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CLRMAC_COMPACT) : /* clrmac */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = 0;
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
  {
    SI opval = 0;
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CLRS_COMPACT) : /* clrs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 0;
    SET_H_SBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "sbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CLRT_COMPACT) : /* clrt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 0;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPEQ_COMPACT) : /* cmp/eq $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPEQI_COMPACT) : /* cmp/eq #$imm8, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (GET_H_GRC (((UINT) 0)), EXTQISI (ANDQI (FLD (f_imm8), 255)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPGE_COMPACT) : /* cmp/ge $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GESI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPGT_COMPACT) : /* cmp/gt $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPHI_COMPACT) : /* cmp/hi $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTUSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPHS_COMPACT) : /* cmp/hs $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GEUSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPPL_COMPACT) : /* cmp/pl $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTSI (GET_H_GRC (FLD (f_rn)), 0);
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPPZ_COMPACT) : /* cmp/pz $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GESI (GET_H_GRC (FLD (f_rn)), 0);
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPSTR_COMPACT) : /* cmp/str $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  SI tmp_temp;
  tmp_temp = XORSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
  tmp_t = EQSI (ANDSI (tmp_temp, 0xff000000), 0);
  tmp_t = ORBI (EQSI (ANDSI (tmp_temp, 16711680), 0), tmp_t);
  tmp_t = ORBI (EQSI (ANDSI (tmp_temp, 65280), 0), tmp_t);
  tmp_t = ORBI (EQSI (ANDSI (tmp_temp, 255), 0), tmp_t);
  {
    BI opval = ((GTUBI (tmp_t, 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIV0S_COMPACT) : /* div0s $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    BI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
  {
    BI opval = SRLSI (GET_H_GRC (FLD (f_rm)), 31);
    SET_H_MBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "mbit", 'x', opval);
  }
  {
    BI opval = ((EQSI (SRLSI (GET_H_GRC (FLD (f_rm)), 31), SRLSI (GET_H_GRC (FLD (f_rn)), 31))) ? (0) : (1));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIV0U_COMPACT) : /* div0u */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    BI opval = 0;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
  {
    BI opval = 0;
    SET_H_MBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "mbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIV1_COMPACT) : /* div1 $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_oldq;
  SI tmp_tmp0;
  UQI tmp_tmp1;
  tmp_oldq = GET_H_QBIT ();
  {
    BI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
  {
    SI opval = ORSI (SLLSI (GET_H_GRC (FLD (f_rn)), 1), ZEXTBISI (GET_H_TBIT ()));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
if (NOTBI (tmp_oldq)) {
if (NOTBI (GET_H_MBIT ())) {
{
  tmp_tmp0 = GET_H_GRC (FLD (f_rn));
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmp1 = GTUSI (GET_H_GRC (FLD (f_rn)), tmp_tmp0);
if (NOTBI (GET_H_QBIT ())) {
  {
    BI opval = ((tmp_tmp1) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
} else {
  {
    BI opval = ((EQQI (tmp_tmp1, 0)) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
}
}
} else {
{
  tmp_tmp0 = GET_H_GRC (FLD (f_rn));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmp1 = LTUSI (GET_H_GRC (FLD (f_rn)), tmp_tmp0);
if (NOTBI (GET_H_QBIT ())) {
  {
    BI opval = ((EQQI (tmp_tmp1, 0)) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
} else {
  {
    BI opval = ((tmp_tmp1) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
}
}
}
} else {
if (NOTBI (GET_H_MBIT ())) {
{
  tmp_tmp0 = GET_H_GRC (FLD (f_rn));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmp1 = LTUSI (GET_H_GRC (FLD (f_rn)), tmp_tmp0);
if (NOTBI (GET_H_QBIT ())) {
  {
    BI opval = ((tmp_tmp1) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
} else {
  {
    BI opval = ((EQQI (tmp_tmp1, 0)) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
}
}
} else {
{
  tmp_tmp0 = GET_H_GRC (FLD (f_rn));
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmp1 = GTUSI (GET_H_GRC (FLD (f_rn)), tmp_tmp0);
if (NOTBI (GET_H_QBIT ())) {
  {
    BI opval = ((EQQI (tmp_tmp1, 0)) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
} else {
  {
    BI opval = ((tmp_tmp1) ? (1) : (0));
    SET_H_QBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "qbit", 'x', opval);
  }
}
}
}
}
  {
    BI opval = ((EQBI (GET_H_QBIT (), GET_H_MBIT ())) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DIVU_COMPACT) : /* divu r0, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = UDIVSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (((UINT) 0)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULR_COMPACT) : /* mulr r0, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (((UINT) 0)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DMULSL_COMPACT) : /* dmuls.l $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_result;
  tmp_result = MULDI (EXTSIDI (GET_H_GRC (FLD (f_rm))), EXTSIDI (GET_H_GRC (FLD (f_rn))));
  {
    SI opval = SUBWORDDISI (tmp_result, 0);
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
  {
    SI opval = SUBWORDDISI (tmp_result, 1);
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DMULUL_COMPACT) : /* dmulu.l $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_result;
  tmp_result = MULDI (ZEXTSIDI (GET_H_GRC (FLD (f_rm))), ZEXTSIDI (GET_H_GRC (FLD (f_rn))));
  {
    SI opval = SUBWORDDISI (tmp_result, 0);
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
  {
    SI opval = SUBWORDDISI (tmp_result, 1);
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_DT_COMPACT) : /* dt $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), 1);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = EQSI (GET_H_GRC (FLD (f_rn)), 0);
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_EXTSB_COMPACT) : /* exts.b $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 3));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_EXTSW_COMPACT) : /* exts.w $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_EXTUB_COMPACT) : /* extu.b $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTQISI (SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 3));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_EXTUW_COMPACT) : /* extu.w $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FABS_COMPACT) : /* fabs $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fabsd (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fabss (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FADD_COMPACT) : /* fadd $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_faddd (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fadds (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPEQ_COMPACT) : /* fcmp/eq $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    BI opval = sh64_fcmpeqd (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_TBIT (opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
} else {
  {
    BI opval = sh64_fcmpeqs (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_TBIT (opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPGT_COMPACT) : /* fcmp/gt $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    BI opval = sh64_fcmpgtd (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_TBIT (opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
} else {
  {
    BI opval = sh64_fcmpgts (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_TBIT (opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCNVDS_COMPACT) : /* fcnvds $drn, fpul */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fcnvds (current_cpu, GET_H_DRC (FLD (f_dn)));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCNVSD_COMPACT) : /* fcnvsd fpul, $drn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DF opval = sh64_fcnvsd (current_cpu, CPU (h_fr[((UINT) 32)]));
    SET_H_DRC (FLD (f_dn), opval);
    TRACE_RESULT (current_cpu, abuf, "drc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FDIV_COMPACT) : /* fdiv $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fdivd (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fdivs (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FIPR_COMPACT) : /* fipr $fvm, $fvn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_fipr (current_cpu, FLD (f_vm), FLD (f_vn));

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDS_COMPACT) : /* flds $frn, fpul */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = GET_H_FRC (FLD (f_rn));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDI0_COMPACT) : /* fldi0 $frn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fldi0 (current_cpu);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDI1_COMPACT) : /* fldi1 $frn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fldi1 (current_cpu);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLOAT_COMPACT) : /* float fpul, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_floatld (current_cpu, CPU (h_fr[((UINT) 32)]));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_floatls (current_cpu, CPU (h_fr[((UINT) 32)]));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMAC_COMPACT) : /* fmac fr0, $frm, $frn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fmacs (current_cpu, GET_H_FRC (((UINT) 0)), GET_H_FRC (FLD (f_rm)), GET_H_FRC (FLD (f_rn)));
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV1_COMPACT) : /* fmov $fmovm, $fmovn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DF opval = GET_H_FMOV (FLD (f_rm));
    SET_H_FMOV (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV2_COMPACT) : /* fmov @$rm, $fmovn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
  {
    DF opval = GETMEMSF (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
} else {
  {
    DF opval = GETMEMDF (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV3_COMPACT) : /* fmov @${rm}+, fmovn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
{
  {
    DF opval = GETMEMSF (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 4);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
} else {
{
  {
    DF opval = GETMEMDF (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 8);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV4_COMPACT) : /* fmov @(r0, $rm), $fmovn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
  {
    DF opval = GETMEMSF (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm))));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
} else {
  {
    DF opval = GETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm))));
    SET_H_FMOV (FLD (f_rn), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV5_COMPACT) : /* fmov $fmovm, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
  {
    SF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMSF (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
} else {
  {
    DF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMDF (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV6_COMPACT) : /* fmov $fmovm, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
{
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    SF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMSF (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
}
} else {
{
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), 8);
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    DF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMDF (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV7_COMPACT) : /* fmov $fmovm, @(r0, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (NOTBI (GET_H_SZBIT ())) {
  {
    SF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMSF (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
} else {
  {
    DF opval = GET_H_FMOV (FLD (f_rm));
    SETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV8_COMPACT) : /* fmov.d @($imm12x8, $rm), $drn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm12x8)));
    SET_H_DRC (FLD (f_dn), opval);
    TRACE_RESULT (current_cpu, abuf, "drc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOV9_COMPACT) : /* mov.l $drm, @($imm12x8, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fmov9_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DRC (FLD (f_dm));
    SETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm12x8)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMUL_COMPACT) : /* fmul $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fmuld (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fmuls (current_cpu, GET_H_FSD (FLD (f_rm)), GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FNEG_COMPACT) : /* fneg $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fnegd (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fnegs (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FRCHG_COMPACT) : /* frchg */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = NOTBI (GET_H_FRBIT ());
    SET_H_FRBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "frbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSCHG_COMPACT) : /* fschg */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = NOTBI (GET_H_SZBIT ());
    SET_H_SZBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "szbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSQRT_COMPACT) : /* fsqrt $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fsqrtd (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fsqrts (current_cpu, GET_H_FSD (FLD (f_rn)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTS_COMPACT) : /* fsts fpul, $frn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = CPU (h_fr[((UINT) 32)]);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSUB_COMPACT) : /* fsub $fsdm, $fsdn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

if (GET_H_PRBIT ()) {
  {
    DF opval = sh64_fsubd (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
} else {
  {
    DF opval = sh64_fsubs (current_cpu, GET_H_FSD (FLD (f_rn)), GET_H_FSD (FLD (f_rm)));
    SET_H_FSD (FLD (f_rn), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fsd", 'f', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRC_COMPACT) : /* ftrc $fsdn, fpul */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = ((GET_H_PRBIT ()) ? (sh64_ftrcdl (current_cpu, GET_H_FSD (FLD (f_rn)))) : (sh64_ftrcsl (current_cpu, GET_H_FSD (FLD (f_rn)))));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRV_COMPACT) : /* ftrv xmtrx, $fvn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_ftrv (current_cpu, FLD (f_vn));

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JMP_COMPACT) : /* jmp @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = GET_H_GRC (FLD (f_rn));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
((void) 0); /*nop*/
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JSR_COMPACT) : /* jsr @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
{
  {
    SI opval = ADDDI (pc, 4);
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }
}
  {
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = GET_H_GRC (FLD (f_rn));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
((void) 0); /*nop*/
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDC_GBR_COMPACT) : /* ldc $rn, gbr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GBR (opval);
    TRACE_RESULT (current_cpu, abuf, "gbr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDC_VBR_COMPACT) : /* ldc $rn, vbr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_VBR (opval);
    TRACE_RESULT (current_cpu, abuf, "vbr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDC_SR_COMPACT) : /* ldc $rn, sr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    CPU (h_sr) = opval;
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDCL_GBR_COMPACT) : /* ldc.l @${rn}+, gbr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_GBR (opval);
    TRACE_RESULT (current_cpu, abuf, "gbr", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDCL_VBR_COMPACT) : /* ldc.l @${rn}+, vbr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_VBR (opval);
    TRACE_RESULT (current_cpu, abuf, "vbr", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDS_FPSCR_COMPACT) : /* lds $rn, fpscr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    CPU (h_fpscr) = opval;
    TRACE_RESULT (current_cpu, abuf, "fpscr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDSL_FPSCR_COMPACT) : /* lds.l @${rn}+, fpscr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    CPU (h_fpscr) = opval;
    TRACE_RESULT (current_cpu, abuf, "fpscr", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDS_FPUL_COMPACT) : /* lds $rn, fpul */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = SUBWORDSISF (GET_H_GRC (FLD (f_rn)));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDSL_FPUL_COMPACT) : /* lds.l @${rn}+, fpul */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SF opval = GETMEMSF (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDS_MACH_COMPACT) : /* lds $rn, mach */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDSL_MACH_COMPACT) : /* lds.l @${rn}+, mach */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDS_MACL_COMPACT) : /* lds $rn, macl */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDSL_MACL_COMPACT) : /* lds.l @${rn}+, macl */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDS_PR_COMPACT) : /* lds $rn, pr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDSL_PR_COMPACT) : /* lds.l @${rn}+, pr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACL_COMPACT) : /* mac.l @${rm}+, @${rn}+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmpry;
  DI tmp_mac;
  DI tmp_result;
  SI tmp_x;
  SI tmp_y;
  tmp_x = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
if (EQSI (FLD (f_rn), FLD (f_rm))) {
{
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 4);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}
  tmp_y = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 4);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmpry = MULDI (ZEXTSIDI (tmp_x), ZEXTSIDI (tmp_y));
  tmp_mac = ORDI (SLLDI (ZEXTSIDI (GET_H_MACH ()), 32), ZEXTSIDI (GET_H_MACL ()));
  tmp_result = ADDDI (tmp_mac, tmp_tmpry);
{
if (GET_H_SBIT ()) {
{
  SI tmp_min;
  SI tmp_max;
  tmp_max = SRLDI (INVDI (0), 16);
  tmp_min = SRLDI (INVDI (0), 15);
if (GTDI (tmp_result, tmp_max)) {
  tmp_result = tmp_max;
} else {
if (LTDI (tmp_result, tmp_min)) {
  tmp_result = tmp_min;
}
}
}
}
  {
    SI opval = SUBWORDDISI (tmp_result, 0);
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
  {
    SI opval = SUBWORDDISI (tmp_result, 1);
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACW_COMPACT) : /* mac.w @${rm}+, @${rn}+ */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_tmpry;
  DI tmp_mac;
  DI tmp_result;
  HI tmp_x;
  HI tmp_y;
  tmp_x = GETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
if (EQSI (FLD (f_rn), FLD (f_rm))) {
{
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 2);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}
  tmp_y = GETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 2);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  tmp_tmpry = MULSI (ZEXTHISI (tmp_x), ZEXTHISI (tmp_y));
if (GET_H_SBIT ()) {
{
if (ADDOFSI (tmp_tmpry, GET_H_MACL (), 0)) {
  {
    SI opval = 1;
    SET_H_MACH (opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
}
  {
    SI opval = ADDSI (tmp_tmpry, GET_H_MACL ());
    SET_H_MACL (opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
}
} else {
{
  tmp_mac = ORDI (SLLDI (ZEXTSIDI (GET_H_MACH ()), 32), ZEXTSIDI (GET_H_MACL ()));
  tmp_result = ADDDI (tmp_mac, EXTSIDI (tmp_tmpry));
  {
    SI opval = SUBWORDDISI (tmp_result, 0);
    SET_H_MACH (opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }
  {
    SI opval = SUBWORDDISI (tmp_result, 1);
    SET_H_MACL (opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOV_COMPACT) : /* mov $rm64, $rn64 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = GET_H_GR (FLD (f_rm));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVI_COMPACT) : /* mov #$imm8, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQIDI (ANDQI (FLD (f_imm8), 255));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVI20_COMPACT) : /* movi20 #$imm20, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movi20_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (f_imm20);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB1_COMPACT) : /* mov.b $rm, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (FLD (f_rm)), 3);
    SETMEMUQI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB2_COMPACT) : /* mov.b $rm, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 1);
  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (FLD (f_rm)), 3);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB3_COMPACT) : /* mov.b $rm, @(r0,$rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (FLD (f_rm)), 3);
    SETMEMUQI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB4_COMPACT) : /* mov.b r0, @($imm8, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GBR (), FLD (f_imm8));
  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (((UINT) 0)), 3);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB5_COMPACT) : /* mov.b r0, @($imm4, $rm) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4));
  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (((UINT) 0)), 3);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB6_COMPACT) : /* mov.b @$rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB7_COMPACT) : /* mov.b @${rm}+, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  QI tmp_data;
  tmp_data = GETMEMQI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
if (EQSI (FLD (f_rm), FLD (f_rn))) {
  {
    SI opval = EXTQISI (tmp_data);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 1);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
  {
    SI opval = EXTQISI (tmp_data);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB8_COMPACT) : /* mov.b @(r0, $rm), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm)))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB9_COMPACT) : /* mov.b @($imm8, gbr), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVB10_COMPACT) : /* mov.b @($imm4, $rm), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL1_COMPACT) : /* mov.l $rm, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL2_COMPACT) : /* mov.l $rm, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL3_COMPACT) : /* mov.l $rm, @(r0, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL4_COMPACT) : /* mov.l r0, @($imm8x4, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (((UINT) 0));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL5_COMPACT) : /* mov.l $rm, @($imm4x4, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm4x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL6_COMPACT) : /* mov.l @$rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL7_COMPACT) : /* mov.l @${rm}+, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
if (EQSI (FLD (f_rm), FLD (f_rn))) {
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 4);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL8_COMPACT) : /* mov.l @(r0, $rm), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL9_COMPACT) : /* mov.l @($imm8x4, gbr), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x4)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL10_COMPACT) : /* mov.l @($imm8x4, pc), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (FLD (f_imm8x4), ANDDI (ADDDI (pc, 4), INVSI (3))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL11_COMPACT) : /* mov.l @($imm4x4, $rm), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x4)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL12_COMPACT) : /* mov.l @($imm12x4, $rm), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm12x4)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVL13_COMPACT) : /* mov.l $rm, @($imm12x4, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm12x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW1_COMPACT) : /* mov.w $rm, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1);
    SETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW2_COMPACT) : /* mov.w $rm, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 2);
  {
    HI opval = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1);
    SETMEMHI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW3_COMPACT) : /* mov.w $rm, @(r0, $rn) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW4_COMPACT) : /* mov.w r0, @($imm8x2, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (((UINT) 0)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x2)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW5_COMPACT) : /* mov.w r0, @($imm4x2, $rm) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (((UINT) 0)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x2)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW6_COMPACT) : /* mov.w @$rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW7_COMPACT) : /* mov.w @${rm}+, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  HI tmp_data;
  tmp_data = GETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
if (EQSI (FLD (f_rm), FLD (f_rn))) {
  {
    SI opval = EXTHISI (tmp_data);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rm)), 2);
    SET_H_GRC (FLD (f_rm), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
  {
    SI opval = EXTHISI (tmp_data);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW8_COMPACT) : /* mov.w @(r0, $rm), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm)))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW9_COMPACT) : /* mov.w @($imm8x2, gbr), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x2))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW10_COMPACT) : /* mov.w @($imm8x2, pc), $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDDI (ADDDI (pc, 4), FLD (f_imm8x2))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVW11_COMPACT) : /* mov.w @($imm4x2, $rm), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x2))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVA_COMPACT) : /* mova @($imm8x4, pc), r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDDI (ANDDI (ADDDI (pc, 4), INVSI (3)), FLD (f_imm8x4));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVCAL_COMPACT) : /* movca.l r0, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (((UINT) 0));
    SETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVCOL_COMPACT) : /* movco.l r0, @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVT_COMPACT) : /* movt $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTBISI (GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVUAL_COMPACT) : /* movua.l @$rn, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = sh64_movua (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVUAL2_COMPACT) : /* movua.l @$rn+, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = sh64_movua (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), 4);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULL_COMPACT) : /* mul.l $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULSW_COMPACT) : /* muls.w $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1)), EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rn)), 1)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULUW_COMPACT) : /* mulu.w $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1)), ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rn)), 1)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEG_COMPACT) : /* neg $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = NEGSI (GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NEGC_COMPACT) : /* negc $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_flag;
  tmp_flag = SUBCFSI (0, GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
  {
    SI opval = SUBCSI (0, GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = tmp_flag;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NOP_COMPACT) : /* nop */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NOT_COMPACT) : /* not $rm64, $rn64 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = INVDI (GET_H_GR (FLD (f_rm)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBI_COMPACT) : /* ocbi @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBP_COMPACT) : /* ocbp @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBWB_COMPACT) : /* ocbwb @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR_COMPACT) : /* or $rm64, $rn64 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_rm)), GET_H_GR (FLD (f_rn)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORI_COMPACT) : /* or #$uimm8, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORB_COMPACT) : /* or.b #$imm8, @(r0, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  UQI tmp_data;
  tmp_addr = ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GBR ());
  tmp_data = ORQI (GETMEMUQI (current_cpu, pc, tmp_addr), FLD (f_imm8));
  {
    UQI opval = tmp_data;
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PREF_COMPACT) : /* pref @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_pref (current_cpu, GET_H_GRC (FLD (f_rn)));

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ROTCL_COMPACT) : /* rotcl $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_temp;
  tmp_temp = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
  {
    SI opval = ORSI (SLLSI (GET_H_GRC (FLD (f_rn)), 1), GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_temp) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ROTCR_COMPACT) : /* rotcr $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_lsbit;
  SI tmp_temp;
  tmp_lsbit = ((EQSI (ANDSI (GET_H_GRC (FLD (f_rn)), 1), 0)) ? (0) : (1));
  tmp_temp = GET_H_TBIT ();
  {
    SI opval = ORSI (SRLSI (GET_H_GRC (FLD (f_rn)), 1), SLLSI (tmp_temp, 31));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_lsbit) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ROTL_COMPACT) : /* rotl $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_temp;
  tmp_temp = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
  {
    SI opval = ORSI (SLLSI (GET_H_GRC (FLD (f_rn)), 1), tmp_temp);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_temp) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ROTR_COMPACT) : /* rotr $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_lsbit;
  SI tmp_temp;
  tmp_lsbit = ((EQSI (ANDSI (GET_H_GRC (FLD (f_rn)), 1), 0)) ? (0) : (1));
  tmp_temp = tmp_lsbit;
  {
    SI opval = ORSI (SRLSI (GET_H_GRC (FLD (f_rn)), 1), SLLSI (tmp_temp, 31));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_lsbit) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RTS_COMPACT) : /* rts */
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
    UDI opval = ADDDI (pc, 2);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
((void) 0); /*nop*/
{
  {
    UDI opval = GET_H_PR ();
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
((void) 0); /*nop*/
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SETS_COMPACT) : /* sets */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 1;
    SET_H_SBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "sbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SETT_COMPACT) : /* sett */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 1;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHAD_COMPACT) : /* shad $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_shamt;
  tmp_shamt = ANDSI (GET_H_GRC (FLD (f_rm)), 31);
if (GESI (GET_H_GRC (FLD (f_rm)), 0)) {
  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), tmp_shamt);
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
if (NESI (tmp_shamt, 0)) {
  {
    SI opval = SRASI (GET_H_GRC (FLD (f_rn)), SUBSI (32, tmp_shamt));
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
if (LTSI (GET_H_GRC (FLD (f_rn)), 0)) {
  {
    SI opval = NEGSI (1);
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHAL_COMPACT) : /* shal $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 1);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_t) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHAR_COMPACT) : /* shar $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = ANDSI (GET_H_GRC (FLD (f_rn)), 1);
  {
    SI opval = SRASI (GET_H_GRC (FLD (f_rn)), 1);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_t) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLD_COMPACT) : /* shld $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  SI tmp_shamt;
  tmp_shamt = ANDSI (GET_H_GRC (FLD (f_rm)), 31);
if (GESI (GET_H_GRC (FLD (f_rm)), 0)) {
  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), tmp_shamt);
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
if (NESI (tmp_shamt, 0)) {
  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), SUBSI (32, tmp_shamt));
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GRC (FLD (f_rn), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}
}
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLL_COMPACT) : /* shll $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = SRLSI (GET_H_GRC (FLD (f_rn)), 31);
  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 1);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_t) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLL2_COMPACT) : /* shll2 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLL8_COMPACT) : /* shll8 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 8);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLL16_COMPACT) : /* shll16 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 16);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLR_COMPACT) : /* shlr $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = ANDSI (GET_H_GRC (FLD (f_rn)), 1);
  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 1);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_t) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLR2_COMPACT) : /* shlr2 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLR8_COMPACT) : /* shlr8 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 8);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLR16_COMPACT) : /* shlr16 $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 16);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STC_GBR_COMPACT) : /* stc gbr, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GBR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STC_VBR_COMPACT) : /* stc vbr, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_VBR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STCL_GBR_COMPACT) : /* stc.l gbr, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_GBR ();
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STCL_VBR_COMPACT) : /* stc.l vbr, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_VBR ();
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STS_FPSCR_COMPACT) : /* sts fpscr, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = CPU (h_fpscr);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STSL_FPSCR_COMPACT) : /* sts.l fpscr, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = CPU (h_fpscr);
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STS_FPUL_COMPACT) : /* sts fpul, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBWORDSFSI (CPU (h_fr[((UINT) 32)]));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STSL_FPUL_COMPACT) : /* sts.l fpul, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SF opval = CPU (h_fr[((UINT) 32)]);
    SETMEMSF (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STS_MACH_COMPACT) : /* sts mach, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_MACH ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STSL_MACH_COMPACT) : /* sts.l mach, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_MACH ();
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STS_MACL_COMPACT) : /* sts macl, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_MACL ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STSL_MACL_COMPACT) : /* sts.l macl, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_MACL ();
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STS_PR_COMPACT) : /* sts pr, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_PR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STSL_PR_COMPACT) : /* sts.l pr, @-$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = SUBSI (GET_H_GRC (FLD (f_rn)), 4);
  {
    SI opval = GET_H_PR ();
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = tmp_addr;
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB_COMPACT) : /* sub $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBC_COMPACT) : /* subc $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_flag;
  tmp_flag = SUBCFSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
  {
    SI opval = SUBCSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = tmp_flag;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBV_COMPACT) : /* subv $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  BI tmp_t;
  tmp_t = SUBOFSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)), 0);
  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
  {
    BI opval = ((tmp_t) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SWAPB_COMPACT) : /* swap.b $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  UHI tmp_top_half;
  UQI tmp_byte1;
  UQI tmp_byte0;
  tmp_top_half = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 0);
  tmp_byte1 = SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 2);
  tmp_byte0 = SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 3);
  {
    SI opval = ORSI (SLLSI (tmp_top_half, 16), ORSI (SLLSI (tmp_byte0, 8), tmp_byte1));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SWAPW_COMPACT) : /* swap.w $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (SRLSI (GET_H_GRC (FLD (f_rm)), 16), SLLSI (GET_H_GRC (FLD (f_rm)), 16));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TASB_COMPACT) : /* tas.b @$rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  UQI tmp_byte;
  tmp_byte = GETMEMUQI (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
  {
    BI opval = ((EQQI (tmp_byte, 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
  tmp_byte = ORQI (tmp_byte, 128);
  {
    UQI opval = tmp_byte;
    SETMEMUQI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TRAPA_COMPACT) : /* trapa #$uimm8 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_compact_trapa (current_cpu, FLD (f_imm8), pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TST_COMPACT) : /* tst $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ((EQSI (ANDSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn))), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TSTI_COMPACT) : /* tst #$uimm8, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ((EQSI (ANDSI (GET_H_GRC (((UINT) 0)), ZEXTSISI (FLD (f_imm8))), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TSTB_COMPACT) : /* tst.b #$imm8, @(r0, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GBR ());
  {
    BI opval = ((EQQI (ANDQI (GETMEMUQI (current_cpu, pc, tmp_addr), FLD (f_imm8)), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR_COMPACT) : /* xor $rm64, $rn64 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_rn)), GET_H_GR (FLD (f_rm)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XORI_COMPACT) : /* xor #$uimm8, r0 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = XORSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XORB_COMPACT) : /* xor.b #$imm8, @(r0, gbr) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  UQI tmp_data;
  tmp_addr = ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GBR ());
  tmp_data = XORQI (GETMEMUQI (current_cpu, pc, tmp_addr), FLD (f_imm8));
  {
    UQI opval = tmp_data;
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XTRCT_COMPACT) : /* xtrct $rm, $rn */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (SLLSI (GET_H_GRC (FLD (f_rm)), 16), SRLSI (GET_H_GRC (FLD (f_rn)), 16));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);


    }
  ENDSWITCH (sem) /* End of semantic switch.  */

  /* At this point `vpc' contains the next insn to execute.  */
}

#undef DEFINE_SWITCH
#endif /* DEFINE_SWITCH */
