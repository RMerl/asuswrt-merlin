/* Simulator instruction decoder for sh64_compact.

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

#define WANT_CPU sh64
#define WANT_CPU_SH64

#include "sim-main.h"
#include "sim-assert.h"

/* The instruction descriptor array.
   This is computed at runtime.  Space for it is not malloc'd to save a
   teensy bit of cpu in the decoder.  Moving it to malloc space is trivial
   but won't be done until necessary (we don't currently support the runtime
   addition of instructions nor an SMP machine with different cpus).  */
static IDESC sh64_compact_insn_data[SH64_COMPACT_INSN__MAX];

/* Commas between elements are contained in the macros.
   Some of these are conditionally compiled out.  */

static const struct insn_sem sh64_compact_insn_sem[] =
{
  { VIRTUAL_INSN_X_INVALID, SH64_COMPACT_INSN_X_INVALID, SH64_COMPACT_SFMT_EMPTY },
  { VIRTUAL_INSN_X_AFTER, SH64_COMPACT_INSN_X_AFTER, SH64_COMPACT_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEFORE, SH64_COMPACT_INSN_X_BEFORE, SH64_COMPACT_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CTI_CHAIN, SH64_COMPACT_INSN_X_CTI_CHAIN, SH64_COMPACT_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CHAIN, SH64_COMPACT_INSN_X_CHAIN, SH64_COMPACT_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEGIN, SH64_COMPACT_INSN_X_BEGIN, SH64_COMPACT_SFMT_EMPTY },
  { SH_INSN_ADD_COMPACT, SH64_COMPACT_INSN_ADD_COMPACT, SH64_COMPACT_SFMT_ADD_COMPACT },
  { SH_INSN_ADDI_COMPACT, SH64_COMPACT_INSN_ADDI_COMPACT, SH64_COMPACT_SFMT_ADDI_COMPACT },
  { SH_INSN_ADDC_COMPACT, SH64_COMPACT_INSN_ADDC_COMPACT, SH64_COMPACT_SFMT_ADDC_COMPACT },
  { SH_INSN_ADDV_COMPACT, SH64_COMPACT_INSN_ADDV_COMPACT, SH64_COMPACT_SFMT_ADDV_COMPACT },
  { SH_INSN_AND_COMPACT, SH64_COMPACT_INSN_AND_COMPACT, SH64_COMPACT_SFMT_AND_COMPACT },
  { SH_INSN_ANDI_COMPACT, SH64_COMPACT_INSN_ANDI_COMPACT, SH64_COMPACT_SFMT_ANDI_COMPACT },
  { SH_INSN_ANDB_COMPACT, SH64_COMPACT_INSN_ANDB_COMPACT, SH64_COMPACT_SFMT_ANDB_COMPACT },
  { SH_INSN_BF_COMPACT, SH64_COMPACT_INSN_BF_COMPACT, SH64_COMPACT_SFMT_BF_COMPACT },
  { SH_INSN_BFS_COMPACT, SH64_COMPACT_INSN_BFS_COMPACT, SH64_COMPACT_SFMT_BFS_COMPACT },
  { SH_INSN_BRA_COMPACT, SH64_COMPACT_INSN_BRA_COMPACT, SH64_COMPACT_SFMT_BRA_COMPACT },
  { SH_INSN_BRAF_COMPACT, SH64_COMPACT_INSN_BRAF_COMPACT, SH64_COMPACT_SFMT_BRAF_COMPACT },
  { SH_INSN_BRK_COMPACT, SH64_COMPACT_INSN_BRK_COMPACT, SH64_COMPACT_SFMT_BRK_COMPACT },
  { SH_INSN_BSR_COMPACT, SH64_COMPACT_INSN_BSR_COMPACT, SH64_COMPACT_SFMT_BSR_COMPACT },
  { SH_INSN_BSRF_COMPACT, SH64_COMPACT_INSN_BSRF_COMPACT, SH64_COMPACT_SFMT_BSRF_COMPACT },
  { SH_INSN_BT_COMPACT, SH64_COMPACT_INSN_BT_COMPACT, SH64_COMPACT_SFMT_BF_COMPACT },
  { SH_INSN_BTS_COMPACT, SH64_COMPACT_INSN_BTS_COMPACT, SH64_COMPACT_SFMT_BFS_COMPACT },
  { SH_INSN_CLRMAC_COMPACT, SH64_COMPACT_INSN_CLRMAC_COMPACT, SH64_COMPACT_SFMT_CLRMAC_COMPACT },
  { SH_INSN_CLRS_COMPACT, SH64_COMPACT_INSN_CLRS_COMPACT, SH64_COMPACT_SFMT_CLRS_COMPACT },
  { SH_INSN_CLRT_COMPACT, SH64_COMPACT_INSN_CLRT_COMPACT, SH64_COMPACT_SFMT_CLRT_COMPACT },
  { SH_INSN_CMPEQ_COMPACT, SH64_COMPACT_INSN_CMPEQ_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_CMPEQI_COMPACT, SH64_COMPACT_INSN_CMPEQI_COMPACT, SH64_COMPACT_SFMT_CMPEQI_COMPACT },
  { SH_INSN_CMPGE_COMPACT, SH64_COMPACT_INSN_CMPGE_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_CMPGT_COMPACT, SH64_COMPACT_INSN_CMPGT_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_CMPHI_COMPACT, SH64_COMPACT_INSN_CMPHI_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_CMPHS_COMPACT, SH64_COMPACT_INSN_CMPHS_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_CMPPL_COMPACT, SH64_COMPACT_INSN_CMPPL_COMPACT, SH64_COMPACT_SFMT_CMPPL_COMPACT },
  { SH_INSN_CMPPZ_COMPACT, SH64_COMPACT_INSN_CMPPZ_COMPACT, SH64_COMPACT_SFMT_CMPPL_COMPACT },
  { SH_INSN_CMPSTR_COMPACT, SH64_COMPACT_INSN_CMPSTR_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_DIV0S_COMPACT, SH64_COMPACT_INSN_DIV0S_COMPACT, SH64_COMPACT_SFMT_DIV0S_COMPACT },
  { SH_INSN_DIV0U_COMPACT, SH64_COMPACT_INSN_DIV0U_COMPACT, SH64_COMPACT_SFMT_DIV0U_COMPACT },
  { SH_INSN_DIV1_COMPACT, SH64_COMPACT_INSN_DIV1_COMPACT, SH64_COMPACT_SFMT_DIV1_COMPACT },
  { SH_INSN_DIVU_COMPACT, SH64_COMPACT_INSN_DIVU_COMPACT, SH64_COMPACT_SFMT_DIVU_COMPACT },
  { SH_INSN_MULR_COMPACT, SH64_COMPACT_INSN_MULR_COMPACT, SH64_COMPACT_SFMT_DIVU_COMPACT },
  { SH_INSN_DMULSL_COMPACT, SH64_COMPACT_INSN_DMULSL_COMPACT, SH64_COMPACT_SFMT_DMULSL_COMPACT },
  { SH_INSN_DMULUL_COMPACT, SH64_COMPACT_INSN_DMULUL_COMPACT, SH64_COMPACT_SFMT_DMULSL_COMPACT },
  { SH_INSN_DT_COMPACT, SH64_COMPACT_INSN_DT_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_EXTSB_COMPACT, SH64_COMPACT_INSN_EXTSB_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_EXTSW_COMPACT, SH64_COMPACT_INSN_EXTSW_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_EXTUB_COMPACT, SH64_COMPACT_INSN_EXTUB_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_EXTUW_COMPACT, SH64_COMPACT_INSN_EXTUW_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_FABS_COMPACT, SH64_COMPACT_INSN_FABS_COMPACT, SH64_COMPACT_SFMT_FABS_COMPACT },
  { SH_INSN_FADD_COMPACT, SH64_COMPACT_INSN_FADD_COMPACT, SH64_COMPACT_SFMT_FADD_COMPACT },
  { SH_INSN_FCMPEQ_COMPACT, SH64_COMPACT_INSN_FCMPEQ_COMPACT, SH64_COMPACT_SFMT_FCMPEQ_COMPACT },
  { SH_INSN_FCMPGT_COMPACT, SH64_COMPACT_INSN_FCMPGT_COMPACT, SH64_COMPACT_SFMT_FCMPEQ_COMPACT },
  { SH_INSN_FCNVDS_COMPACT, SH64_COMPACT_INSN_FCNVDS_COMPACT, SH64_COMPACT_SFMT_FCNVDS_COMPACT },
  { SH_INSN_FCNVSD_COMPACT, SH64_COMPACT_INSN_FCNVSD_COMPACT, SH64_COMPACT_SFMT_FCNVSD_COMPACT },
  { SH_INSN_FDIV_COMPACT, SH64_COMPACT_INSN_FDIV_COMPACT, SH64_COMPACT_SFMT_FADD_COMPACT },
  { SH_INSN_FIPR_COMPACT, SH64_COMPACT_INSN_FIPR_COMPACT, SH64_COMPACT_SFMT_FIPR_COMPACT },
  { SH_INSN_FLDS_COMPACT, SH64_COMPACT_INSN_FLDS_COMPACT, SH64_COMPACT_SFMT_FLDS_COMPACT },
  { SH_INSN_FLDI0_COMPACT, SH64_COMPACT_INSN_FLDI0_COMPACT, SH64_COMPACT_SFMT_FLDI0_COMPACT },
  { SH_INSN_FLDI1_COMPACT, SH64_COMPACT_INSN_FLDI1_COMPACT, SH64_COMPACT_SFMT_FLDI0_COMPACT },
  { SH_INSN_FLOAT_COMPACT, SH64_COMPACT_INSN_FLOAT_COMPACT, SH64_COMPACT_SFMT_FLOAT_COMPACT },
  { SH_INSN_FMAC_COMPACT, SH64_COMPACT_INSN_FMAC_COMPACT, SH64_COMPACT_SFMT_FMAC_COMPACT },
  { SH_INSN_FMOV1_COMPACT, SH64_COMPACT_INSN_FMOV1_COMPACT, SH64_COMPACT_SFMT_FMOV1_COMPACT },
  { SH_INSN_FMOV2_COMPACT, SH64_COMPACT_INSN_FMOV2_COMPACT, SH64_COMPACT_SFMT_FMOV2_COMPACT },
  { SH_INSN_FMOV3_COMPACT, SH64_COMPACT_INSN_FMOV3_COMPACT, SH64_COMPACT_SFMT_FMOV3_COMPACT },
  { SH_INSN_FMOV4_COMPACT, SH64_COMPACT_INSN_FMOV4_COMPACT, SH64_COMPACT_SFMT_FMOV4_COMPACT },
  { SH_INSN_FMOV5_COMPACT, SH64_COMPACT_INSN_FMOV5_COMPACT, SH64_COMPACT_SFMT_FMOV5_COMPACT },
  { SH_INSN_FMOV6_COMPACT, SH64_COMPACT_INSN_FMOV6_COMPACT, SH64_COMPACT_SFMT_FMOV6_COMPACT },
  { SH_INSN_FMOV7_COMPACT, SH64_COMPACT_INSN_FMOV7_COMPACT, SH64_COMPACT_SFMT_FMOV7_COMPACT },
  { SH_INSN_FMOV8_COMPACT, SH64_COMPACT_INSN_FMOV8_COMPACT, SH64_COMPACT_SFMT_FMOV8_COMPACT },
  { SH_INSN_FMOV9_COMPACT, SH64_COMPACT_INSN_FMOV9_COMPACT, SH64_COMPACT_SFMT_FMOV9_COMPACT },
  { SH_INSN_FMUL_COMPACT, SH64_COMPACT_INSN_FMUL_COMPACT, SH64_COMPACT_SFMT_FADD_COMPACT },
  { SH_INSN_FNEG_COMPACT, SH64_COMPACT_INSN_FNEG_COMPACT, SH64_COMPACT_SFMT_FABS_COMPACT },
  { SH_INSN_FRCHG_COMPACT, SH64_COMPACT_INSN_FRCHG_COMPACT, SH64_COMPACT_SFMT_FRCHG_COMPACT },
  { SH_INSN_FSCHG_COMPACT, SH64_COMPACT_INSN_FSCHG_COMPACT, SH64_COMPACT_SFMT_FSCHG_COMPACT },
  { SH_INSN_FSQRT_COMPACT, SH64_COMPACT_INSN_FSQRT_COMPACT, SH64_COMPACT_SFMT_FABS_COMPACT },
  { SH_INSN_FSTS_COMPACT, SH64_COMPACT_INSN_FSTS_COMPACT, SH64_COMPACT_SFMT_FSTS_COMPACT },
  { SH_INSN_FSUB_COMPACT, SH64_COMPACT_INSN_FSUB_COMPACT, SH64_COMPACT_SFMT_FADD_COMPACT },
  { SH_INSN_FTRC_COMPACT, SH64_COMPACT_INSN_FTRC_COMPACT, SH64_COMPACT_SFMT_FTRC_COMPACT },
  { SH_INSN_FTRV_COMPACT, SH64_COMPACT_INSN_FTRV_COMPACT, SH64_COMPACT_SFMT_FTRV_COMPACT },
  { SH_INSN_JMP_COMPACT, SH64_COMPACT_INSN_JMP_COMPACT, SH64_COMPACT_SFMT_BRAF_COMPACT },
  { SH_INSN_JSR_COMPACT, SH64_COMPACT_INSN_JSR_COMPACT, SH64_COMPACT_SFMT_BSRF_COMPACT },
  { SH_INSN_LDC_GBR_COMPACT, SH64_COMPACT_INSN_LDC_GBR_COMPACT, SH64_COMPACT_SFMT_LDC_GBR_COMPACT },
  { SH_INSN_LDC_VBR_COMPACT, SH64_COMPACT_INSN_LDC_VBR_COMPACT, SH64_COMPACT_SFMT_LDC_VBR_COMPACT },
  { SH_INSN_LDC_SR_COMPACT, SH64_COMPACT_INSN_LDC_SR_COMPACT, SH64_COMPACT_SFMT_LDC_SR_COMPACT },
  { SH_INSN_LDCL_GBR_COMPACT, SH64_COMPACT_INSN_LDCL_GBR_COMPACT, SH64_COMPACT_SFMT_LDCL_GBR_COMPACT },
  { SH_INSN_LDCL_VBR_COMPACT, SH64_COMPACT_INSN_LDCL_VBR_COMPACT, SH64_COMPACT_SFMT_LDCL_VBR_COMPACT },
  { SH_INSN_LDS_FPSCR_COMPACT, SH64_COMPACT_INSN_LDS_FPSCR_COMPACT, SH64_COMPACT_SFMT_LDS_FPSCR_COMPACT },
  { SH_INSN_LDSL_FPSCR_COMPACT, SH64_COMPACT_INSN_LDSL_FPSCR_COMPACT, SH64_COMPACT_SFMT_LDSL_FPSCR_COMPACT },
  { SH_INSN_LDS_FPUL_COMPACT, SH64_COMPACT_INSN_LDS_FPUL_COMPACT, SH64_COMPACT_SFMT_LDS_FPUL_COMPACT },
  { SH_INSN_LDSL_FPUL_COMPACT, SH64_COMPACT_INSN_LDSL_FPUL_COMPACT, SH64_COMPACT_SFMT_LDSL_FPUL_COMPACT },
  { SH_INSN_LDS_MACH_COMPACT, SH64_COMPACT_INSN_LDS_MACH_COMPACT, SH64_COMPACT_SFMT_LDS_MACH_COMPACT },
  { SH_INSN_LDSL_MACH_COMPACT, SH64_COMPACT_INSN_LDSL_MACH_COMPACT, SH64_COMPACT_SFMT_LDSL_MACH_COMPACT },
  { SH_INSN_LDS_MACL_COMPACT, SH64_COMPACT_INSN_LDS_MACL_COMPACT, SH64_COMPACT_SFMT_LDS_MACL_COMPACT },
  { SH_INSN_LDSL_MACL_COMPACT, SH64_COMPACT_INSN_LDSL_MACL_COMPACT, SH64_COMPACT_SFMT_LDSL_MACL_COMPACT },
  { SH_INSN_LDS_PR_COMPACT, SH64_COMPACT_INSN_LDS_PR_COMPACT, SH64_COMPACT_SFMT_LDS_PR_COMPACT },
  { SH_INSN_LDSL_PR_COMPACT, SH64_COMPACT_INSN_LDSL_PR_COMPACT, SH64_COMPACT_SFMT_LDSL_PR_COMPACT },
  { SH_INSN_MACL_COMPACT, SH64_COMPACT_INSN_MACL_COMPACT, SH64_COMPACT_SFMT_MACL_COMPACT },
  { SH_INSN_MACW_COMPACT, SH64_COMPACT_INSN_MACW_COMPACT, SH64_COMPACT_SFMT_MACW_COMPACT },
  { SH_INSN_MOV_COMPACT, SH64_COMPACT_INSN_MOV_COMPACT, SH64_COMPACT_SFMT_MOV_COMPACT },
  { SH_INSN_MOVI_COMPACT, SH64_COMPACT_INSN_MOVI_COMPACT, SH64_COMPACT_SFMT_MOVI_COMPACT },
  { SH_INSN_MOVI20_COMPACT, SH64_COMPACT_INSN_MOVI20_COMPACT, SH64_COMPACT_SFMT_MOVI20_COMPACT },
  { SH_INSN_MOVB1_COMPACT, SH64_COMPACT_INSN_MOVB1_COMPACT, SH64_COMPACT_SFMT_MOVB1_COMPACT },
  { SH_INSN_MOVB2_COMPACT, SH64_COMPACT_INSN_MOVB2_COMPACT, SH64_COMPACT_SFMT_MOVB2_COMPACT },
  { SH_INSN_MOVB3_COMPACT, SH64_COMPACT_INSN_MOVB3_COMPACT, SH64_COMPACT_SFMT_MOVB3_COMPACT },
  { SH_INSN_MOVB4_COMPACT, SH64_COMPACT_INSN_MOVB4_COMPACT, SH64_COMPACT_SFMT_MOVB4_COMPACT },
  { SH_INSN_MOVB5_COMPACT, SH64_COMPACT_INSN_MOVB5_COMPACT, SH64_COMPACT_SFMT_MOVB5_COMPACT },
  { SH_INSN_MOVB6_COMPACT, SH64_COMPACT_INSN_MOVB6_COMPACT, SH64_COMPACT_SFMT_MOVB6_COMPACT },
  { SH_INSN_MOVB7_COMPACT, SH64_COMPACT_INSN_MOVB7_COMPACT, SH64_COMPACT_SFMT_MOVB7_COMPACT },
  { SH_INSN_MOVB8_COMPACT, SH64_COMPACT_INSN_MOVB8_COMPACT, SH64_COMPACT_SFMT_MOVB8_COMPACT },
  { SH_INSN_MOVB9_COMPACT, SH64_COMPACT_INSN_MOVB9_COMPACT, SH64_COMPACT_SFMT_MOVB9_COMPACT },
  { SH_INSN_MOVB10_COMPACT, SH64_COMPACT_INSN_MOVB10_COMPACT, SH64_COMPACT_SFMT_MOVB10_COMPACT },
  { SH_INSN_MOVL1_COMPACT, SH64_COMPACT_INSN_MOVL1_COMPACT, SH64_COMPACT_SFMT_MOVL1_COMPACT },
  { SH_INSN_MOVL2_COMPACT, SH64_COMPACT_INSN_MOVL2_COMPACT, SH64_COMPACT_SFMT_MOVL2_COMPACT },
  { SH_INSN_MOVL3_COMPACT, SH64_COMPACT_INSN_MOVL3_COMPACT, SH64_COMPACT_SFMT_MOVL3_COMPACT },
  { SH_INSN_MOVL4_COMPACT, SH64_COMPACT_INSN_MOVL4_COMPACT, SH64_COMPACT_SFMT_MOVL4_COMPACT },
  { SH_INSN_MOVL5_COMPACT, SH64_COMPACT_INSN_MOVL5_COMPACT, SH64_COMPACT_SFMT_MOVL5_COMPACT },
  { SH_INSN_MOVL6_COMPACT, SH64_COMPACT_INSN_MOVL6_COMPACT, SH64_COMPACT_SFMT_MOVL6_COMPACT },
  { SH_INSN_MOVL7_COMPACT, SH64_COMPACT_INSN_MOVL7_COMPACT, SH64_COMPACT_SFMT_MOVL7_COMPACT },
  { SH_INSN_MOVL8_COMPACT, SH64_COMPACT_INSN_MOVL8_COMPACT, SH64_COMPACT_SFMT_MOVL8_COMPACT },
  { SH_INSN_MOVL9_COMPACT, SH64_COMPACT_INSN_MOVL9_COMPACT, SH64_COMPACT_SFMT_MOVL9_COMPACT },
  { SH_INSN_MOVL10_COMPACT, SH64_COMPACT_INSN_MOVL10_COMPACT, SH64_COMPACT_SFMT_MOVL10_COMPACT },
  { SH_INSN_MOVL11_COMPACT, SH64_COMPACT_INSN_MOVL11_COMPACT, SH64_COMPACT_SFMT_MOVL11_COMPACT },
  { SH_INSN_MOVL12_COMPACT, SH64_COMPACT_INSN_MOVL12_COMPACT, SH64_COMPACT_SFMT_MOVL12_COMPACT },
  { SH_INSN_MOVL13_COMPACT, SH64_COMPACT_INSN_MOVL13_COMPACT, SH64_COMPACT_SFMT_MOVL13_COMPACT },
  { SH_INSN_MOVW1_COMPACT, SH64_COMPACT_INSN_MOVW1_COMPACT, SH64_COMPACT_SFMT_MOVW1_COMPACT },
  { SH_INSN_MOVW2_COMPACT, SH64_COMPACT_INSN_MOVW2_COMPACT, SH64_COMPACT_SFMT_MOVW2_COMPACT },
  { SH_INSN_MOVW3_COMPACT, SH64_COMPACT_INSN_MOVW3_COMPACT, SH64_COMPACT_SFMT_MOVW3_COMPACT },
  { SH_INSN_MOVW4_COMPACT, SH64_COMPACT_INSN_MOVW4_COMPACT, SH64_COMPACT_SFMT_MOVW4_COMPACT },
  { SH_INSN_MOVW5_COMPACT, SH64_COMPACT_INSN_MOVW5_COMPACT, SH64_COMPACT_SFMT_MOVW5_COMPACT },
  { SH_INSN_MOVW6_COMPACT, SH64_COMPACT_INSN_MOVW6_COMPACT, SH64_COMPACT_SFMT_MOVW6_COMPACT },
  { SH_INSN_MOVW7_COMPACT, SH64_COMPACT_INSN_MOVW7_COMPACT, SH64_COMPACT_SFMT_MOVW7_COMPACT },
  { SH_INSN_MOVW8_COMPACT, SH64_COMPACT_INSN_MOVW8_COMPACT, SH64_COMPACT_SFMT_MOVW8_COMPACT },
  { SH_INSN_MOVW9_COMPACT, SH64_COMPACT_INSN_MOVW9_COMPACT, SH64_COMPACT_SFMT_MOVW9_COMPACT },
  { SH_INSN_MOVW10_COMPACT, SH64_COMPACT_INSN_MOVW10_COMPACT, SH64_COMPACT_SFMT_MOVW10_COMPACT },
  { SH_INSN_MOVW11_COMPACT, SH64_COMPACT_INSN_MOVW11_COMPACT, SH64_COMPACT_SFMT_MOVW11_COMPACT },
  { SH_INSN_MOVA_COMPACT, SH64_COMPACT_INSN_MOVA_COMPACT, SH64_COMPACT_SFMT_MOVA_COMPACT },
  { SH_INSN_MOVCAL_COMPACT, SH64_COMPACT_INSN_MOVCAL_COMPACT, SH64_COMPACT_SFMT_MOVCAL_COMPACT },
  { SH_INSN_MOVCOL_COMPACT, SH64_COMPACT_INSN_MOVCOL_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_MOVT_COMPACT, SH64_COMPACT_INSN_MOVT_COMPACT, SH64_COMPACT_SFMT_MOVT_COMPACT },
  { SH_INSN_MOVUAL_COMPACT, SH64_COMPACT_INSN_MOVUAL_COMPACT, SH64_COMPACT_SFMT_MOVUAL_COMPACT },
  { SH_INSN_MOVUAL2_COMPACT, SH64_COMPACT_INSN_MOVUAL2_COMPACT, SH64_COMPACT_SFMT_MOVUAL2_COMPACT },
  { SH_INSN_MULL_COMPACT, SH64_COMPACT_INSN_MULL_COMPACT, SH64_COMPACT_SFMT_MULL_COMPACT },
  { SH_INSN_MULSW_COMPACT, SH64_COMPACT_INSN_MULSW_COMPACT, SH64_COMPACT_SFMT_MULL_COMPACT },
  { SH_INSN_MULUW_COMPACT, SH64_COMPACT_INSN_MULUW_COMPACT, SH64_COMPACT_SFMT_MULL_COMPACT },
  { SH_INSN_NEG_COMPACT, SH64_COMPACT_INSN_NEG_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_NEGC_COMPACT, SH64_COMPACT_INSN_NEGC_COMPACT, SH64_COMPACT_SFMT_NEGC_COMPACT },
  { SH_INSN_NOP_COMPACT, SH64_COMPACT_INSN_NOP_COMPACT, SH64_COMPACT_SFMT_NOP_COMPACT },
  { SH_INSN_NOT_COMPACT, SH64_COMPACT_INSN_NOT_COMPACT, SH64_COMPACT_SFMT_MOV_COMPACT },
  { SH_INSN_OCBI_COMPACT, SH64_COMPACT_INSN_OCBI_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_OCBP_COMPACT, SH64_COMPACT_INSN_OCBP_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_OCBWB_COMPACT, SH64_COMPACT_INSN_OCBWB_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_OR_COMPACT, SH64_COMPACT_INSN_OR_COMPACT, SH64_COMPACT_SFMT_AND_COMPACT },
  { SH_INSN_ORI_COMPACT, SH64_COMPACT_INSN_ORI_COMPACT, SH64_COMPACT_SFMT_ANDI_COMPACT },
  { SH_INSN_ORB_COMPACT, SH64_COMPACT_INSN_ORB_COMPACT, SH64_COMPACT_SFMT_ANDB_COMPACT },
  { SH_INSN_PREF_COMPACT, SH64_COMPACT_INSN_PREF_COMPACT, SH64_COMPACT_SFMT_PREF_COMPACT },
  { SH_INSN_ROTCL_COMPACT, SH64_COMPACT_INSN_ROTCL_COMPACT, SH64_COMPACT_SFMT_ROTCL_COMPACT },
  { SH_INSN_ROTCR_COMPACT, SH64_COMPACT_INSN_ROTCR_COMPACT, SH64_COMPACT_SFMT_ROTCL_COMPACT },
  { SH_INSN_ROTL_COMPACT, SH64_COMPACT_INSN_ROTL_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_ROTR_COMPACT, SH64_COMPACT_INSN_ROTR_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_RTS_COMPACT, SH64_COMPACT_INSN_RTS_COMPACT, SH64_COMPACT_SFMT_RTS_COMPACT },
  { SH_INSN_SETS_COMPACT, SH64_COMPACT_INSN_SETS_COMPACT, SH64_COMPACT_SFMT_CLRS_COMPACT },
  { SH_INSN_SETT_COMPACT, SH64_COMPACT_INSN_SETT_COMPACT, SH64_COMPACT_SFMT_CLRT_COMPACT },
  { SH_INSN_SHAD_COMPACT, SH64_COMPACT_INSN_SHAD_COMPACT, SH64_COMPACT_SFMT_SHAD_COMPACT },
  { SH_INSN_SHAL_COMPACT, SH64_COMPACT_INSN_SHAL_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_SHAR_COMPACT, SH64_COMPACT_INSN_SHAR_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_SHLD_COMPACT, SH64_COMPACT_INSN_SHLD_COMPACT, SH64_COMPACT_SFMT_SHAD_COMPACT },
  { SH_INSN_SHLL_COMPACT, SH64_COMPACT_INSN_SHLL_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_SHLL2_COMPACT, SH64_COMPACT_INSN_SHLL2_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_SHLL8_COMPACT, SH64_COMPACT_INSN_SHLL8_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_SHLL16_COMPACT, SH64_COMPACT_INSN_SHLL16_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_SHLR_COMPACT, SH64_COMPACT_INSN_SHLR_COMPACT, SH64_COMPACT_SFMT_DT_COMPACT },
  { SH_INSN_SHLR2_COMPACT, SH64_COMPACT_INSN_SHLR2_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_SHLR8_COMPACT, SH64_COMPACT_INSN_SHLR8_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_SHLR16_COMPACT, SH64_COMPACT_INSN_SHLR16_COMPACT, SH64_COMPACT_SFMT_MOVCOL_COMPACT },
  { SH_INSN_STC_GBR_COMPACT, SH64_COMPACT_INSN_STC_GBR_COMPACT, SH64_COMPACT_SFMT_STC_GBR_COMPACT },
  { SH_INSN_STC_VBR_COMPACT, SH64_COMPACT_INSN_STC_VBR_COMPACT, SH64_COMPACT_SFMT_STC_VBR_COMPACT },
  { SH_INSN_STCL_GBR_COMPACT, SH64_COMPACT_INSN_STCL_GBR_COMPACT, SH64_COMPACT_SFMT_STCL_GBR_COMPACT },
  { SH_INSN_STCL_VBR_COMPACT, SH64_COMPACT_INSN_STCL_VBR_COMPACT, SH64_COMPACT_SFMT_STCL_VBR_COMPACT },
  { SH_INSN_STS_FPSCR_COMPACT, SH64_COMPACT_INSN_STS_FPSCR_COMPACT, SH64_COMPACT_SFMT_STS_FPSCR_COMPACT },
  { SH_INSN_STSL_FPSCR_COMPACT, SH64_COMPACT_INSN_STSL_FPSCR_COMPACT, SH64_COMPACT_SFMT_STSL_FPSCR_COMPACT },
  { SH_INSN_STS_FPUL_COMPACT, SH64_COMPACT_INSN_STS_FPUL_COMPACT, SH64_COMPACT_SFMT_STS_FPUL_COMPACT },
  { SH_INSN_STSL_FPUL_COMPACT, SH64_COMPACT_INSN_STSL_FPUL_COMPACT, SH64_COMPACT_SFMT_STSL_FPUL_COMPACT },
  { SH_INSN_STS_MACH_COMPACT, SH64_COMPACT_INSN_STS_MACH_COMPACT, SH64_COMPACT_SFMT_STS_MACH_COMPACT },
  { SH_INSN_STSL_MACH_COMPACT, SH64_COMPACT_INSN_STSL_MACH_COMPACT, SH64_COMPACT_SFMT_STSL_MACH_COMPACT },
  { SH_INSN_STS_MACL_COMPACT, SH64_COMPACT_INSN_STS_MACL_COMPACT, SH64_COMPACT_SFMT_STS_MACL_COMPACT },
  { SH_INSN_STSL_MACL_COMPACT, SH64_COMPACT_INSN_STSL_MACL_COMPACT, SH64_COMPACT_SFMT_STSL_MACL_COMPACT },
  { SH_INSN_STS_PR_COMPACT, SH64_COMPACT_INSN_STS_PR_COMPACT, SH64_COMPACT_SFMT_STS_PR_COMPACT },
  { SH_INSN_STSL_PR_COMPACT, SH64_COMPACT_INSN_STSL_PR_COMPACT, SH64_COMPACT_SFMT_STSL_PR_COMPACT },
  { SH_INSN_SUB_COMPACT, SH64_COMPACT_INSN_SUB_COMPACT, SH64_COMPACT_SFMT_ADD_COMPACT },
  { SH_INSN_SUBC_COMPACT, SH64_COMPACT_INSN_SUBC_COMPACT, SH64_COMPACT_SFMT_ADDC_COMPACT },
  { SH_INSN_SUBV_COMPACT, SH64_COMPACT_INSN_SUBV_COMPACT, SH64_COMPACT_SFMT_ADDV_COMPACT },
  { SH_INSN_SWAPB_COMPACT, SH64_COMPACT_INSN_SWAPB_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_SWAPW_COMPACT, SH64_COMPACT_INSN_SWAPW_COMPACT, SH64_COMPACT_SFMT_EXTSB_COMPACT },
  { SH_INSN_TASB_COMPACT, SH64_COMPACT_INSN_TASB_COMPACT, SH64_COMPACT_SFMT_TASB_COMPACT },
  { SH_INSN_TRAPA_COMPACT, SH64_COMPACT_INSN_TRAPA_COMPACT, SH64_COMPACT_SFMT_TRAPA_COMPACT },
  { SH_INSN_TST_COMPACT, SH64_COMPACT_INSN_TST_COMPACT, SH64_COMPACT_SFMT_CMPEQ_COMPACT },
  { SH_INSN_TSTI_COMPACT, SH64_COMPACT_INSN_TSTI_COMPACT, SH64_COMPACT_SFMT_TSTI_COMPACT },
  { SH_INSN_TSTB_COMPACT, SH64_COMPACT_INSN_TSTB_COMPACT, SH64_COMPACT_SFMT_TSTB_COMPACT },
  { SH_INSN_XOR_COMPACT, SH64_COMPACT_INSN_XOR_COMPACT, SH64_COMPACT_SFMT_AND_COMPACT },
  { SH_INSN_XORI_COMPACT, SH64_COMPACT_INSN_XORI_COMPACT, SH64_COMPACT_SFMT_ANDI_COMPACT },
  { SH_INSN_XORB_COMPACT, SH64_COMPACT_INSN_XORB_COMPACT, SH64_COMPACT_SFMT_ANDB_COMPACT },
  { SH_INSN_XTRCT_COMPACT, SH64_COMPACT_INSN_XTRCT_COMPACT, SH64_COMPACT_SFMT_ADD_COMPACT },
};

static const struct insn_sem sh64_compact_insn_sem_invalid = {
  VIRTUAL_INSN_X_INVALID, SH64_COMPACT_INSN_X_INVALID, SH64_COMPACT_SFMT_EMPTY
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
sh64_compact_init_idesc_table (SIM_CPU *cpu)
{
  IDESC *id,*tabend;
  const struct insn_sem *t,*tend;
  int tabsize = SH64_COMPACT_INSN__MAX;
  IDESC *table = sh64_compact_insn_data;

  memset (table, 0, tabsize * sizeof (IDESC));

  /* First set all entries to the `invalid insn'.  */
  t = & sh64_compact_insn_sem_invalid;
  for (id = table, tabend = table + tabsize; id < tabend; ++id)
    init_idesc (cpu, id, t);

  /* Now fill in the values for the chosen cpu.  */
  for (t = sh64_compact_insn_sem, tend = t + sizeof (sh64_compact_insn_sem) / sizeof (*t);
       t != tend; ++t)
    {
      init_idesc (cpu, & table[t->index], t);
    }

  /* Link the IDESC table into the cpu.  */
  CPU_IDESC (cpu) = table;
}

/* Given an instruction, return a pointer to its IDESC entry.  */

const IDESC *
sh64_compact_decode (SIM_CPU *current_cpu, IADDR pc,
              CGEN_INSN_INT base_insn, CGEN_INSN_INT entire_insn,
              ARGBUF *abuf)
{
  /* Result of decoder.  */
  SH64_COMPACT_INSN_TYPE itype;

  {
    CGEN_INSN_INT insn = base_insn;

    {
      unsigned int val = (((insn >> 5) & (15 << 7)) | ((insn >> 0) & (127 << 0)));
      switch (val)
      {
      case 0 : /* fall through */
      case 16 : /* fall through */
      case 32 : /* fall through */
      case 48 : /* fall through */
      case 64 : /* fall through */
      case 80 : /* fall through */
      case 96 : /* fall through */
      case 112 :
        if ((entire_insn & 0xf00f0000) == 0x0)
          { itype = SH64_COMPACT_INSN_MOVI20_COMPACT; goto extract_sfmt_movi20_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 3 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0x3)
              { itype = SH64_COMPACT_INSN_BSRF_COMPACT; goto extract_sfmt_bsrf_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0x83)
              { itype = SH64_COMPACT_INSN_PREF_COMPACT; goto extract_sfmt_pref_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 4 : /* fall through */
      case 20 : /* fall through */
      case 36 : /* fall through */
      case 52 : /* fall through */
      case 68 : /* fall through */
      case 84 : /* fall through */
      case 100 : /* fall through */
      case 116 :
        if ((entire_insn & 0xf00f) == 0x4)
          { itype = SH64_COMPACT_INSN_MOVB3_COMPACT; goto extract_sfmt_movb3_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 5 : /* fall through */
      case 21 : /* fall through */
      case 37 : /* fall through */
      case 53 : /* fall through */
      case 69 : /* fall through */
      case 85 : /* fall through */
      case 101 : /* fall through */
      case 117 :
        if ((entire_insn & 0xf00f) == 0x5)
          { itype = SH64_COMPACT_INSN_MOVW3_COMPACT; goto extract_sfmt_movw3_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 6 : /* fall through */
      case 22 : /* fall through */
      case 38 : /* fall through */
      case 54 : /* fall through */
      case 70 : /* fall through */
      case 86 : /* fall through */
      case 102 : /* fall through */
      case 118 :
        if ((entire_insn & 0xf00f) == 0x6)
          { itype = SH64_COMPACT_INSN_MOVL3_COMPACT; goto extract_sfmt_movl3_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 7 : /* fall through */
      case 23 : /* fall through */
      case 39 : /* fall through */
      case 55 : /* fall through */
      case 71 : /* fall through */
      case 87 : /* fall through */
      case 103 : /* fall through */
      case 119 :
        if ((entire_insn & 0xf00f) == 0x7)
          { itype = SH64_COMPACT_INSN_MULL_COMPACT; goto extract_sfmt_mull_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 8 :
        if ((entire_insn & 0xffff) == 0x8)
          { itype = SH64_COMPACT_INSN_CLRT_COMPACT; goto extract_sfmt_clrt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 9 :
        if ((entire_insn & 0xffff) == 0x9)
          { itype = SH64_COMPACT_INSN_NOP_COMPACT; goto extract_sfmt_nop_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 10 :
        if ((entire_insn & 0xf0ff) == 0xa)
          { itype = SH64_COMPACT_INSN_STS_MACH_COMPACT; goto extract_sfmt_sts_mach_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 11 :
        if ((entire_insn & 0xffff) == 0xb)
          { itype = SH64_COMPACT_INSN_RTS_COMPACT; goto extract_sfmt_rts_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 12 : /* fall through */
      case 28 : /* fall through */
      case 44 : /* fall through */
      case 60 : /* fall through */
      case 76 : /* fall through */
      case 92 : /* fall through */
      case 108 : /* fall through */
      case 124 :
        if ((entire_insn & 0xf00f) == 0xc)
          { itype = SH64_COMPACT_INSN_MOVB8_COMPACT; goto extract_sfmt_movb8_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 13 : /* fall through */
      case 29 : /* fall through */
      case 45 : /* fall through */
      case 61 : /* fall through */
      case 77 : /* fall through */
      case 93 : /* fall through */
      case 109 : /* fall through */
      case 125 :
        if ((entire_insn & 0xf00f) == 0xd)
          { itype = SH64_COMPACT_INSN_MOVW8_COMPACT; goto extract_sfmt_movw8_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 14 : /* fall through */
      case 30 : /* fall through */
      case 46 : /* fall through */
      case 62 : /* fall through */
      case 78 : /* fall through */
      case 94 : /* fall through */
      case 110 : /* fall through */
      case 126 :
        if ((entire_insn & 0xf00f) == 0xe)
          { itype = SH64_COMPACT_INSN_MOVL8_COMPACT; goto extract_sfmt_movl8_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 15 : /* fall through */
      case 31 : /* fall through */
      case 47 : /* fall through */
      case 63 : /* fall through */
      case 79 : /* fall through */
      case 95 : /* fall through */
      case 111 : /* fall through */
      case 127 :
        if ((entire_insn & 0xf00f) == 0xf)
          { itype = SH64_COMPACT_INSN_MACL_COMPACT; goto extract_sfmt_macl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 18 :
        if ((entire_insn & 0xf0ff) == 0x12)
          { itype = SH64_COMPACT_INSN_STC_GBR_COMPACT; goto extract_sfmt_stc_gbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 19 :
        if ((entire_insn & 0xf0ff) == 0x93)
          { itype = SH64_COMPACT_INSN_OCBI_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 24 :
        if ((entire_insn & 0xffff) == 0x18)
          { itype = SH64_COMPACT_INSN_SETT_COMPACT; goto extract_sfmt_clrt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 25 :
        if ((entire_insn & 0xffff) == 0x19)
          { itype = SH64_COMPACT_INSN_DIV0U_COMPACT; goto extract_sfmt_div0u_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 26 :
        if ((entire_insn & 0xf0ff) == 0x1a)
          { itype = SH64_COMPACT_INSN_STS_MACL_COMPACT; goto extract_sfmt_sts_macl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 34 :
        if ((entire_insn & 0xf0ff) == 0x22)
          { itype = SH64_COMPACT_INSN_STC_VBR_COMPACT; goto extract_sfmt_stc_vbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 35 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0x23)
              { itype = SH64_COMPACT_INSN_BRAF_COMPACT; goto extract_sfmt_braf_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0xa3)
              { itype = SH64_COMPACT_INSN_OCBP_COMPACT; goto extract_sfmt_movcol_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 40 :
        if ((entire_insn & 0xffff) == 0x28)
          { itype = SH64_COMPACT_INSN_CLRMAC_COMPACT; goto extract_sfmt_clrmac_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 41 :
        if ((entire_insn & 0xf0ff) == 0x29)
          { itype = SH64_COMPACT_INSN_MOVT_COMPACT; goto extract_sfmt_movt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 42 :
        if ((entire_insn & 0xf0ff) == 0x2a)
          { itype = SH64_COMPACT_INSN_STS_PR_COMPACT; goto extract_sfmt_sts_pr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 51 :
        if ((entire_insn & 0xf0ff) == 0xb3)
          { itype = SH64_COMPACT_INSN_OCBWB_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 59 :
        if ((entire_insn & 0xffff) == 0x3b)
          { itype = SH64_COMPACT_INSN_BRK_COMPACT; goto extract_sfmt_brk_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 67 :
        if ((entire_insn & 0xf0ff) == 0xc3)
          { itype = SH64_COMPACT_INSN_MOVCAL_COMPACT; goto extract_sfmt_movcal_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 72 :
        if ((entire_insn & 0xffff) == 0x48)
          { itype = SH64_COMPACT_INSN_CLRS_COMPACT; goto extract_sfmt_clrs_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 88 :
        if ((entire_insn & 0xffff) == 0x58)
          { itype = SH64_COMPACT_INSN_SETS_COMPACT; goto extract_sfmt_clrs_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 90 :
        if ((entire_insn & 0xf0ff) == 0x5a)
          { itype = SH64_COMPACT_INSN_STS_FPUL_COMPACT; goto extract_sfmt_sts_fpul_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 106 :
        if ((entire_insn & 0xf0ff) == 0x6a)
          { itype = SH64_COMPACT_INSN_STS_FPSCR_COMPACT; goto extract_sfmt_sts_fpscr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 115 :
        if ((entire_insn & 0xf0ff) == 0x73)
          { itype = SH64_COMPACT_INSN_MOVCOL_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 128 : /* fall through */
      case 129 : /* fall through */
      case 130 : /* fall through */
      case 131 : /* fall through */
      case 132 : /* fall through */
      case 133 : /* fall through */
      case 134 : /* fall through */
      case 135 : /* fall through */
      case 136 : /* fall through */
      case 137 : /* fall through */
      case 138 : /* fall through */
      case 139 : /* fall through */
      case 140 : /* fall through */
      case 141 : /* fall through */
      case 142 : /* fall through */
      case 143 : /* fall through */
      case 144 : /* fall through */
      case 145 : /* fall through */
      case 146 : /* fall through */
      case 147 : /* fall through */
      case 148 : /* fall through */
      case 149 : /* fall through */
      case 150 : /* fall through */
      case 151 : /* fall through */
      case 152 : /* fall through */
      case 153 : /* fall through */
      case 154 : /* fall through */
      case 155 : /* fall through */
      case 156 : /* fall through */
      case 157 : /* fall through */
      case 158 : /* fall through */
      case 159 : /* fall through */
      case 160 : /* fall through */
      case 161 : /* fall through */
      case 162 : /* fall through */
      case 163 : /* fall through */
      case 164 : /* fall through */
      case 165 : /* fall through */
      case 166 : /* fall through */
      case 167 : /* fall through */
      case 168 : /* fall through */
      case 169 : /* fall through */
      case 170 : /* fall through */
      case 171 : /* fall through */
      case 172 : /* fall through */
      case 173 : /* fall through */
      case 174 : /* fall through */
      case 175 : /* fall through */
      case 176 : /* fall through */
      case 177 : /* fall through */
      case 178 : /* fall through */
      case 179 : /* fall through */
      case 180 : /* fall through */
      case 181 : /* fall through */
      case 182 : /* fall through */
      case 183 : /* fall through */
      case 184 : /* fall through */
      case 185 : /* fall through */
      case 186 : /* fall through */
      case 187 : /* fall through */
      case 188 : /* fall through */
      case 189 : /* fall through */
      case 190 : /* fall through */
      case 191 : /* fall through */
      case 192 : /* fall through */
      case 193 : /* fall through */
      case 194 : /* fall through */
      case 195 : /* fall through */
      case 196 : /* fall through */
      case 197 : /* fall through */
      case 198 : /* fall through */
      case 199 : /* fall through */
      case 200 : /* fall through */
      case 201 : /* fall through */
      case 202 : /* fall through */
      case 203 : /* fall through */
      case 204 : /* fall through */
      case 205 : /* fall through */
      case 206 : /* fall through */
      case 207 : /* fall through */
      case 208 : /* fall through */
      case 209 : /* fall through */
      case 210 : /* fall through */
      case 211 : /* fall through */
      case 212 : /* fall through */
      case 213 : /* fall through */
      case 214 : /* fall through */
      case 215 : /* fall through */
      case 216 : /* fall through */
      case 217 : /* fall through */
      case 218 : /* fall through */
      case 219 : /* fall through */
      case 220 : /* fall through */
      case 221 : /* fall through */
      case 222 : /* fall through */
      case 223 : /* fall through */
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
      case 239 : /* fall through */
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
        if ((entire_insn & 0xf000) == 0x1000)
          { itype = SH64_COMPACT_INSN_MOVL5_COMPACT; goto extract_sfmt_movl5_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 256 : /* fall through */
      case 272 : /* fall through */
      case 288 : /* fall through */
      case 304 : /* fall through */
      case 320 : /* fall through */
      case 336 : /* fall through */
      case 352 : /* fall through */
      case 368 :
        if ((entire_insn & 0xf00f) == 0x2000)
          { itype = SH64_COMPACT_INSN_MOVB1_COMPACT; goto extract_sfmt_movb1_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 257 : /* fall through */
      case 273 : /* fall through */
      case 289 : /* fall through */
      case 305 : /* fall through */
      case 321 : /* fall through */
      case 337 : /* fall through */
      case 353 : /* fall through */
      case 369 :
        if ((entire_insn & 0xf00f) == 0x2001)
          { itype = SH64_COMPACT_INSN_MOVW1_COMPACT; goto extract_sfmt_movw1_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 258 : /* fall through */
      case 274 : /* fall through */
      case 290 : /* fall through */
      case 306 : /* fall through */
      case 322 : /* fall through */
      case 338 : /* fall through */
      case 354 : /* fall through */
      case 370 :
        if ((entire_insn & 0xf00f) == 0x2002)
          { itype = SH64_COMPACT_INSN_MOVL1_COMPACT; goto extract_sfmt_movl1_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 260 : /* fall through */
      case 276 : /* fall through */
      case 292 : /* fall through */
      case 308 : /* fall through */
      case 324 : /* fall through */
      case 340 : /* fall through */
      case 356 : /* fall through */
      case 372 :
        if ((entire_insn & 0xf00f) == 0x2004)
          { itype = SH64_COMPACT_INSN_MOVB2_COMPACT; goto extract_sfmt_movb2_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 261 : /* fall through */
      case 277 : /* fall through */
      case 293 : /* fall through */
      case 309 : /* fall through */
      case 325 : /* fall through */
      case 341 : /* fall through */
      case 357 : /* fall through */
      case 373 :
        if ((entire_insn & 0xf00f) == 0x2005)
          { itype = SH64_COMPACT_INSN_MOVW2_COMPACT; goto extract_sfmt_movw2_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 262 : /* fall through */
      case 278 : /* fall through */
      case 294 : /* fall through */
      case 310 : /* fall through */
      case 326 : /* fall through */
      case 342 : /* fall through */
      case 358 : /* fall through */
      case 374 :
        if ((entire_insn & 0xf00f) == 0x2006)
          { itype = SH64_COMPACT_INSN_MOVL2_COMPACT; goto extract_sfmt_movl2_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 263 : /* fall through */
      case 279 : /* fall through */
      case 295 : /* fall through */
      case 311 : /* fall through */
      case 327 : /* fall through */
      case 343 : /* fall through */
      case 359 : /* fall through */
      case 375 :
        if ((entire_insn & 0xf00f) == 0x2007)
          { itype = SH64_COMPACT_INSN_DIV0S_COMPACT; goto extract_sfmt_div0s_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 264 : /* fall through */
      case 280 : /* fall through */
      case 296 : /* fall through */
      case 312 : /* fall through */
      case 328 : /* fall through */
      case 344 : /* fall through */
      case 360 : /* fall through */
      case 376 :
        if ((entire_insn & 0xf00f) == 0x2008)
          { itype = SH64_COMPACT_INSN_TST_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 265 : /* fall through */
      case 281 : /* fall through */
      case 297 : /* fall through */
      case 313 : /* fall through */
      case 329 : /* fall through */
      case 345 : /* fall through */
      case 361 : /* fall through */
      case 377 :
        if ((entire_insn & 0xf00f) == 0x2009)
          { itype = SH64_COMPACT_INSN_AND_COMPACT; goto extract_sfmt_and_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 266 : /* fall through */
      case 282 : /* fall through */
      case 298 : /* fall through */
      case 314 : /* fall through */
      case 330 : /* fall through */
      case 346 : /* fall through */
      case 362 : /* fall through */
      case 378 :
        if ((entire_insn & 0xf00f) == 0x200a)
          { itype = SH64_COMPACT_INSN_XOR_COMPACT; goto extract_sfmt_and_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 267 : /* fall through */
      case 283 : /* fall through */
      case 299 : /* fall through */
      case 315 : /* fall through */
      case 331 : /* fall through */
      case 347 : /* fall through */
      case 363 : /* fall through */
      case 379 :
        if ((entire_insn & 0xf00f) == 0x200b)
          { itype = SH64_COMPACT_INSN_OR_COMPACT; goto extract_sfmt_and_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 268 : /* fall through */
      case 284 : /* fall through */
      case 300 : /* fall through */
      case 316 : /* fall through */
      case 332 : /* fall through */
      case 348 : /* fall through */
      case 364 : /* fall through */
      case 380 :
        if ((entire_insn & 0xf00f) == 0x200c)
          { itype = SH64_COMPACT_INSN_CMPSTR_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 269 : /* fall through */
      case 285 : /* fall through */
      case 301 : /* fall through */
      case 317 : /* fall through */
      case 333 : /* fall through */
      case 349 : /* fall through */
      case 365 : /* fall through */
      case 381 :
        if ((entire_insn & 0xf00f) == 0x200d)
          { itype = SH64_COMPACT_INSN_XTRCT_COMPACT; goto extract_sfmt_add_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 270 : /* fall through */
      case 286 : /* fall through */
      case 302 : /* fall through */
      case 318 : /* fall through */
      case 334 : /* fall through */
      case 350 : /* fall through */
      case 366 : /* fall through */
      case 382 :
        if ((entire_insn & 0xf00f) == 0x200e)
          { itype = SH64_COMPACT_INSN_MULUW_COMPACT; goto extract_sfmt_mull_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 271 : /* fall through */
      case 287 : /* fall through */
      case 303 : /* fall through */
      case 319 : /* fall through */
      case 335 : /* fall through */
      case 351 : /* fall through */
      case 367 : /* fall through */
      case 383 :
        if ((entire_insn & 0xf00f) == 0x200f)
          { itype = SH64_COMPACT_INSN_MULSW_COMPACT; goto extract_sfmt_mull_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 384 : /* fall through */
      case 400 : /* fall through */
      case 416 : /* fall through */
      case 432 : /* fall through */
      case 448 : /* fall through */
      case 464 : /* fall through */
      case 480 : /* fall through */
      case 496 :
        if ((entire_insn & 0xf00f) == 0x3000)
          { itype = SH64_COMPACT_INSN_CMPEQ_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 385 : /* fall through */
      case 417 : /* fall through */
      case 449 : /* fall through */
      case 481 :
        {
          unsigned int val = (((insn >> -3) & (1 << 1)) | ((insn >> -4) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf00ff000) == 0x30012000)
              { itype = SH64_COMPACT_INSN_MOVL13_COMPACT; goto extract_sfmt_movl13_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf01ff000) == 0x30013000)
              { itype = SH64_COMPACT_INSN_FMOV9_COMPACT; goto extract_sfmt_fmov9_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 2 :
            if ((entire_insn & 0xf00ff000) == 0x30016000)
              { itype = SH64_COMPACT_INSN_MOVL12_COMPACT; goto extract_sfmt_movl12_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 3 :
            if ((entire_insn & 0xf10ff000) == 0x30017000)
              { itype = SH64_COMPACT_INSN_FMOV8_COMPACT; goto extract_sfmt_fmov8_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 386 : /* fall through */
      case 402 : /* fall through */
      case 418 : /* fall through */
      case 434 : /* fall through */
      case 450 : /* fall through */
      case 466 : /* fall through */
      case 482 : /* fall through */
      case 498 :
        if ((entire_insn & 0xf00f) == 0x3002)
          { itype = SH64_COMPACT_INSN_CMPHS_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 387 : /* fall through */
      case 403 : /* fall through */
      case 419 : /* fall through */
      case 435 : /* fall through */
      case 451 : /* fall through */
      case 467 : /* fall through */
      case 483 : /* fall through */
      case 499 :
        if ((entire_insn & 0xf00f) == 0x3003)
          { itype = SH64_COMPACT_INSN_CMPGE_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 388 : /* fall through */
      case 404 : /* fall through */
      case 420 : /* fall through */
      case 436 : /* fall through */
      case 452 : /* fall through */
      case 468 : /* fall through */
      case 484 : /* fall through */
      case 500 :
        if ((entire_insn & 0xf00f) == 0x3004)
          { itype = SH64_COMPACT_INSN_DIV1_COMPACT; goto extract_sfmt_div1_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 389 : /* fall through */
      case 405 : /* fall through */
      case 421 : /* fall through */
      case 437 : /* fall through */
      case 453 : /* fall through */
      case 469 : /* fall through */
      case 485 : /* fall through */
      case 501 :
        if ((entire_insn & 0xf00f) == 0x3005)
          { itype = SH64_COMPACT_INSN_DMULUL_COMPACT; goto extract_sfmt_dmulsl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 390 : /* fall through */
      case 406 : /* fall through */
      case 422 : /* fall through */
      case 438 : /* fall through */
      case 454 : /* fall through */
      case 470 : /* fall through */
      case 486 : /* fall through */
      case 502 :
        if ((entire_insn & 0xf00f) == 0x3006)
          { itype = SH64_COMPACT_INSN_CMPHI_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 391 : /* fall through */
      case 407 : /* fall through */
      case 423 : /* fall through */
      case 439 : /* fall through */
      case 455 : /* fall through */
      case 471 : /* fall through */
      case 487 : /* fall through */
      case 503 :
        if ((entire_insn & 0xf00f) == 0x3007)
          { itype = SH64_COMPACT_INSN_CMPGT_COMPACT; goto extract_sfmt_cmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 392 : /* fall through */
      case 408 : /* fall through */
      case 424 : /* fall through */
      case 440 : /* fall through */
      case 456 : /* fall through */
      case 472 : /* fall through */
      case 488 : /* fall through */
      case 504 :
        if ((entire_insn & 0xf00f) == 0x3008)
          { itype = SH64_COMPACT_INSN_SUB_COMPACT; goto extract_sfmt_add_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 394 : /* fall through */
      case 410 : /* fall through */
      case 426 : /* fall through */
      case 442 : /* fall through */
      case 458 : /* fall through */
      case 474 : /* fall through */
      case 490 : /* fall through */
      case 506 :
        if ((entire_insn & 0xf00f) == 0x300a)
          { itype = SH64_COMPACT_INSN_SUBC_COMPACT; goto extract_sfmt_addc_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 395 : /* fall through */
      case 411 : /* fall through */
      case 427 : /* fall through */
      case 443 : /* fall through */
      case 459 : /* fall through */
      case 475 : /* fall through */
      case 491 : /* fall through */
      case 507 :
        if ((entire_insn & 0xf00f) == 0x300b)
          { itype = SH64_COMPACT_INSN_SUBV_COMPACT; goto extract_sfmt_addv_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 396 : /* fall through */
      case 412 : /* fall through */
      case 428 : /* fall through */
      case 444 : /* fall through */
      case 460 : /* fall through */
      case 476 : /* fall through */
      case 492 : /* fall through */
      case 508 :
        if ((entire_insn & 0xf00f) == 0x300c)
          { itype = SH64_COMPACT_INSN_ADD_COMPACT; goto extract_sfmt_add_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 397 : /* fall through */
      case 413 : /* fall through */
      case 429 : /* fall through */
      case 445 : /* fall through */
      case 461 : /* fall through */
      case 477 : /* fall through */
      case 493 : /* fall through */
      case 509 :
        if ((entire_insn & 0xf00f) == 0x300d)
          { itype = SH64_COMPACT_INSN_DMULSL_COMPACT; goto extract_sfmt_dmulsl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 398 : /* fall through */
      case 414 : /* fall through */
      case 430 : /* fall through */
      case 446 : /* fall through */
      case 462 : /* fall through */
      case 478 : /* fall through */
      case 494 : /* fall through */
      case 510 :
        if ((entire_insn & 0xf00f) == 0x300e)
          { itype = SH64_COMPACT_INSN_ADDC_COMPACT; goto extract_sfmt_addc_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 399 : /* fall through */
      case 415 : /* fall through */
      case 431 : /* fall through */
      case 447 : /* fall through */
      case 463 : /* fall through */
      case 479 : /* fall through */
      case 495 : /* fall through */
      case 511 :
        if ((entire_insn & 0xf00f) == 0x300f)
          { itype = SH64_COMPACT_INSN_ADDV_COMPACT; goto extract_sfmt_addv_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 401 : /* fall through */
      case 433 : /* fall through */
      case 465 : /* fall through */
      case 497 :
        {
          unsigned int val = (((insn >> -3) & (1 << 1)) | ((insn >> -4) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf00ff000) == 0x30012000)
              { itype = SH64_COMPACT_INSN_MOVL13_COMPACT; goto extract_sfmt_movl13_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 2 :
            if ((entire_insn & 0xf00ff000) == 0x30016000)
              { itype = SH64_COMPACT_INSN_MOVL12_COMPACT; goto extract_sfmt_movl12_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 3 :
            if ((entire_insn & 0xf10ff000) == 0x30017000)
              { itype = SH64_COMPACT_INSN_FMOV8_COMPACT; goto extract_sfmt_fmov8_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 512 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0x4000)
              { itype = SH64_COMPACT_INSN_SHLL_COMPACT; goto extract_sfmt_dt_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0x4080)
              { itype = SH64_COMPACT_INSN_MULR_COMPACT; goto extract_sfmt_divu_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 513 :
        if ((entire_insn & 0xf0ff) == 0x4001)
          { itype = SH64_COMPACT_INSN_SHLR_COMPACT; goto extract_sfmt_dt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 514 :
        if ((entire_insn & 0xf0ff) == 0x4002)
          { itype = SH64_COMPACT_INSN_STSL_MACH_COMPACT; goto extract_sfmt_stsl_mach_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 516 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0x4004)
              { itype = SH64_COMPACT_INSN_ROTL_COMPACT; goto extract_sfmt_dt_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0x4084)
              { itype = SH64_COMPACT_INSN_DIVU_COMPACT; goto extract_sfmt_divu_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 517 :
        if ((entire_insn & 0xf0ff) == 0x4005)
          { itype = SH64_COMPACT_INSN_ROTR_COMPACT; goto extract_sfmt_dt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 518 :
        if ((entire_insn & 0xf0ff) == 0x4006)
          { itype = SH64_COMPACT_INSN_LDSL_MACH_COMPACT; goto extract_sfmt_ldsl_mach_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 520 :
        if ((entire_insn & 0xf0ff) == 0x4008)
          { itype = SH64_COMPACT_INSN_SHLL2_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 521 :
        if ((entire_insn & 0xf0ff) == 0x4009)
          { itype = SH64_COMPACT_INSN_SHLR2_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 522 :
        if ((entire_insn & 0xf0ff) == 0x400a)
          { itype = SH64_COMPACT_INSN_LDS_MACH_COMPACT; goto extract_sfmt_lds_mach_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 523 :
        if ((entire_insn & 0xf0ff) == 0x400b)
          { itype = SH64_COMPACT_INSN_JSR_COMPACT; goto extract_sfmt_bsrf_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 524 : /* fall through */
      case 540 : /* fall through */
      case 556 : /* fall through */
      case 572 : /* fall through */
      case 588 : /* fall through */
      case 604 : /* fall through */
      case 620 : /* fall through */
      case 636 :
        if ((entire_insn & 0xf00f) == 0x400c)
          { itype = SH64_COMPACT_INSN_SHAD_COMPACT; goto extract_sfmt_shad_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 525 : /* fall through */
      case 541 : /* fall through */
      case 557 : /* fall through */
      case 573 : /* fall through */
      case 589 : /* fall through */
      case 605 : /* fall through */
      case 621 : /* fall through */
      case 637 :
        if ((entire_insn & 0xf00f) == 0x400d)
          { itype = SH64_COMPACT_INSN_SHLD_COMPACT; goto extract_sfmt_shad_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 526 :
        if ((entire_insn & 0xf0ff) == 0x400e)
          { itype = SH64_COMPACT_INSN_LDC_SR_COMPACT; goto extract_sfmt_ldc_sr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 527 : /* fall through */
      case 543 : /* fall through */
      case 559 : /* fall through */
      case 575 : /* fall through */
      case 591 : /* fall through */
      case 607 : /* fall through */
      case 623 : /* fall through */
      case 639 :
        if ((entire_insn & 0xf00f) == 0x400f)
          { itype = SH64_COMPACT_INSN_MACW_COMPACT; goto extract_sfmt_macw_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 528 :
        if ((entire_insn & 0xf0ff) == 0x4010)
          { itype = SH64_COMPACT_INSN_DT_COMPACT; goto extract_sfmt_dt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 529 :
        if ((entire_insn & 0xf0ff) == 0x4011)
          { itype = SH64_COMPACT_INSN_CMPPZ_COMPACT; goto extract_sfmt_cmppl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 530 :
        if ((entire_insn & 0xf0ff) == 0x4012)
          { itype = SH64_COMPACT_INSN_STSL_MACL_COMPACT; goto extract_sfmt_stsl_macl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 531 :
        if ((entire_insn & 0xf0ff) == 0x4013)
          { itype = SH64_COMPACT_INSN_STCL_GBR_COMPACT; goto extract_sfmt_stcl_gbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 533 :
        if ((entire_insn & 0xf0ff) == 0x4015)
          { itype = SH64_COMPACT_INSN_CMPPL_COMPACT; goto extract_sfmt_cmppl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 534 :
        if ((entire_insn & 0xf0ff) == 0x4016)
          { itype = SH64_COMPACT_INSN_LDSL_MACL_COMPACT; goto extract_sfmt_ldsl_macl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 535 :
        if ((entire_insn & 0xf0ff) == 0x4017)
          { itype = SH64_COMPACT_INSN_LDCL_GBR_COMPACT; goto extract_sfmt_ldcl_gbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 536 :
        if ((entire_insn & 0xf0ff) == 0x4018)
          { itype = SH64_COMPACT_INSN_SHLL8_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 537 :
        if ((entire_insn & 0xf0ff) == 0x4019)
          { itype = SH64_COMPACT_INSN_SHLR8_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 538 :
        if ((entire_insn & 0xf0ff) == 0x401a)
          { itype = SH64_COMPACT_INSN_LDS_MACL_COMPACT; goto extract_sfmt_lds_macl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 539 :
        if ((entire_insn & 0xf0ff) == 0x401b)
          { itype = SH64_COMPACT_INSN_TASB_COMPACT; goto extract_sfmt_tasb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 542 :
        if ((entire_insn & 0xf0ff) == 0x401e)
          { itype = SH64_COMPACT_INSN_LDC_GBR_COMPACT; goto extract_sfmt_ldc_gbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 544 :
        if ((entire_insn & 0xf0ff) == 0x4020)
          { itype = SH64_COMPACT_INSN_SHAL_COMPACT; goto extract_sfmt_dt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 545 :
        if ((entire_insn & 0xf0ff) == 0x4021)
          { itype = SH64_COMPACT_INSN_SHAR_COMPACT; goto extract_sfmt_dt_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 546 :
        if ((entire_insn & 0xf0ff) == 0x4022)
          { itype = SH64_COMPACT_INSN_STSL_PR_COMPACT; goto extract_sfmt_stsl_pr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 547 :
        if ((entire_insn & 0xf0ff) == 0x4023)
          { itype = SH64_COMPACT_INSN_STCL_VBR_COMPACT; goto extract_sfmt_stcl_vbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 548 :
        if ((entire_insn & 0xf0ff) == 0x4024)
          { itype = SH64_COMPACT_INSN_ROTCL_COMPACT; goto extract_sfmt_rotcl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 549 :
        if ((entire_insn & 0xf0ff) == 0x4025)
          { itype = SH64_COMPACT_INSN_ROTCR_COMPACT; goto extract_sfmt_rotcl_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 550 :
        if ((entire_insn & 0xf0ff) == 0x4026)
          { itype = SH64_COMPACT_INSN_LDSL_PR_COMPACT; goto extract_sfmt_ldsl_pr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 551 :
        if ((entire_insn & 0xf0ff) == 0x4027)
          { itype = SH64_COMPACT_INSN_LDCL_VBR_COMPACT; goto extract_sfmt_ldcl_vbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 552 :
        if ((entire_insn & 0xf0ff) == 0x4028)
          { itype = SH64_COMPACT_INSN_SHLL16_COMPACT; goto extract_sfmt_movcol_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 553 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0x4029)
              { itype = SH64_COMPACT_INSN_SHLR16_COMPACT; goto extract_sfmt_movcol_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0x40a9)
              { itype = SH64_COMPACT_INSN_MOVUAL_COMPACT; goto extract_sfmt_movual_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 554 :
        if ((entire_insn & 0xf0ff) == 0x402a)
          { itype = SH64_COMPACT_INSN_LDS_PR_COMPACT; goto extract_sfmt_lds_pr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 555 :
        if ((entire_insn & 0xf0ff) == 0x402b)
          { itype = SH64_COMPACT_INSN_JMP_COMPACT; goto extract_sfmt_braf_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 558 :
        if ((entire_insn & 0xf0ff) == 0x402e)
          { itype = SH64_COMPACT_INSN_LDC_VBR_COMPACT; goto extract_sfmt_ldc_vbr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 594 :
        if ((entire_insn & 0xf0ff) == 0x4052)
          { itype = SH64_COMPACT_INSN_STSL_FPUL_COMPACT; goto extract_sfmt_stsl_fpul_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 598 :
        if ((entire_insn & 0xf0ff) == 0x4056)
          { itype = SH64_COMPACT_INSN_LDSL_FPUL_COMPACT; goto extract_sfmt_ldsl_fpul_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 602 :
        if ((entire_insn & 0xf0ff) == 0x405a)
          { itype = SH64_COMPACT_INSN_LDS_FPUL_COMPACT; goto extract_sfmt_lds_fpul_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 610 :
        if ((entire_insn & 0xf0ff) == 0x4062)
          { itype = SH64_COMPACT_INSN_STSL_FPSCR_COMPACT; goto extract_sfmt_stsl_fpscr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 614 :
        if ((entire_insn & 0xf0ff) == 0x4066)
          { itype = SH64_COMPACT_INSN_LDSL_FPSCR_COMPACT; goto extract_sfmt_ldsl_fpscr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 617 :
        if ((entire_insn & 0xf0ff) == 0x40e9)
          { itype = SH64_COMPACT_INSN_MOVUAL2_COMPACT; goto extract_sfmt_movual2_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 618 :
        if ((entire_insn & 0xf0ff) == 0x406a)
          { itype = SH64_COMPACT_INSN_LDS_FPSCR_COMPACT; goto extract_sfmt_lds_fpscr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 640 : /* fall through */
      case 641 : /* fall through */
      case 642 : /* fall through */
      case 643 : /* fall through */
      case 644 : /* fall through */
      case 645 : /* fall through */
      case 646 : /* fall through */
      case 647 : /* fall through */
      case 648 : /* fall through */
      case 649 : /* fall through */
      case 650 : /* fall through */
      case 651 : /* fall through */
      case 652 : /* fall through */
      case 653 : /* fall through */
      case 654 : /* fall through */
      case 655 : /* fall through */
      case 656 : /* fall through */
      case 657 : /* fall through */
      case 658 : /* fall through */
      case 659 : /* fall through */
      case 660 : /* fall through */
      case 661 : /* fall through */
      case 662 : /* fall through */
      case 663 : /* fall through */
      case 664 : /* fall through */
      case 665 : /* fall through */
      case 666 : /* fall through */
      case 667 : /* fall through */
      case 668 : /* fall through */
      case 669 : /* fall through */
      case 670 : /* fall through */
      case 671 : /* fall through */
      case 672 : /* fall through */
      case 673 : /* fall through */
      case 674 : /* fall through */
      case 675 : /* fall through */
      case 676 : /* fall through */
      case 677 : /* fall through */
      case 678 : /* fall through */
      case 679 : /* fall through */
      case 680 : /* fall through */
      case 681 : /* fall through */
      case 682 : /* fall through */
      case 683 : /* fall through */
      case 684 : /* fall through */
      case 685 : /* fall through */
      case 686 : /* fall through */
      case 687 : /* fall through */
      case 688 : /* fall through */
      case 689 : /* fall through */
      case 690 : /* fall through */
      case 691 : /* fall through */
      case 692 : /* fall through */
      case 693 : /* fall through */
      case 694 : /* fall through */
      case 695 : /* fall through */
      case 696 : /* fall through */
      case 697 : /* fall through */
      case 698 : /* fall through */
      case 699 : /* fall through */
      case 700 : /* fall through */
      case 701 : /* fall through */
      case 702 : /* fall through */
      case 703 : /* fall through */
      case 704 : /* fall through */
      case 705 : /* fall through */
      case 706 : /* fall through */
      case 707 : /* fall through */
      case 708 : /* fall through */
      case 709 : /* fall through */
      case 710 : /* fall through */
      case 711 : /* fall through */
      case 712 : /* fall through */
      case 713 : /* fall through */
      case 714 : /* fall through */
      case 715 : /* fall through */
      case 716 : /* fall through */
      case 717 : /* fall through */
      case 718 : /* fall through */
      case 719 : /* fall through */
      case 720 : /* fall through */
      case 721 : /* fall through */
      case 722 : /* fall through */
      case 723 : /* fall through */
      case 724 : /* fall through */
      case 725 : /* fall through */
      case 726 : /* fall through */
      case 727 : /* fall through */
      case 728 : /* fall through */
      case 729 : /* fall through */
      case 730 : /* fall through */
      case 731 : /* fall through */
      case 732 : /* fall through */
      case 733 : /* fall through */
      case 734 : /* fall through */
      case 735 : /* fall through */
      case 736 : /* fall through */
      case 737 : /* fall through */
      case 738 : /* fall through */
      case 739 : /* fall through */
      case 740 : /* fall through */
      case 741 : /* fall through */
      case 742 : /* fall through */
      case 743 : /* fall through */
      case 744 : /* fall through */
      case 745 : /* fall through */
      case 746 : /* fall through */
      case 747 : /* fall through */
      case 748 : /* fall through */
      case 749 : /* fall through */
      case 750 : /* fall through */
      case 751 : /* fall through */
      case 752 : /* fall through */
      case 753 : /* fall through */
      case 754 : /* fall through */
      case 755 : /* fall through */
      case 756 : /* fall through */
      case 757 : /* fall through */
      case 758 : /* fall through */
      case 759 : /* fall through */
      case 760 : /* fall through */
      case 761 : /* fall through */
      case 762 : /* fall through */
      case 763 : /* fall through */
      case 764 : /* fall through */
      case 765 : /* fall through */
      case 766 : /* fall through */
      case 767 :
        if ((entire_insn & 0xf000) == 0x5000)
          { itype = SH64_COMPACT_INSN_MOVL11_COMPACT; goto extract_sfmt_movl11_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 768 : /* fall through */
      case 784 : /* fall through */
      case 800 : /* fall through */
      case 816 : /* fall through */
      case 832 : /* fall through */
      case 848 : /* fall through */
      case 864 : /* fall through */
      case 880 :
        if ((entire_insn & 0xf00f) == 0x6000)
          { itype = SH64_COMPACT_INSN_MOVB6_COMPACT; goto extract_sfmt_movb6_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 769 : /* fall through */
      case 785 : /* fall through */
      case 801 : /* fall through */
      case 817 : /* fall through */
      case 833 : /* fall through */
      case 849 : /* fall through */
      case 865 : /* fall through */
      case 881 :
        if ((entire_insn & 0xf00f) == 0x6001)
          { itype = SH64_COMPACT_INSN_MOVW6_COMPACT; goto extract_sfmt_movw6_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 770 : /* fall through */
      case 786 : /* fall through */
      case 802 : /* fall through */
      case 818 : /* fall through */
      case 834 : /* fall through */
      case 850 : /* fall through */
      case 866 : /* fall through */
      case 882 :
        if ((entire_insn & 0xf00f) == 0x6002)
          { itype = SH64_COMPACT_INSN_MOVL6_COMPACT; goto extract_sfmt_movl6_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 771 : /* fall through */
      case 787 : /* fall through */
      case 803 : /* fall through */
      case 819 : /* fall through */
      case 835 : /* fall through */
      case 851 : /* fall through */
      case 867 : /* fall through */
      case 883 :
        if ((entire_insn & 0xf00f) == 0x6003)
          { itype = SH64_COMPACT_INSN_MOV_COMPACT; goto extract_sfmt_mov_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 772 : /* fall through */
      case 788 : /* fall through */
      case 804 : /* fall through */
      case 820 : /* fall through */
      case 836 : /* fall through */
      case 852 : /* fall through */
      case 868 : /* fall through */
      case 884 :
        if ((entire_insn & 0xf00f) == 0x6004)
          { itype = SH64_COMPACT_INSN_MOVB7_COMPACT; goto extract_sfmt_movb7_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 773 : /* fall through */
      case 789 : /* fall through */
      case 805 : /* fall through */
      case 821 : /* fall through */
      case 837 : /* fall through */
      case 853 : /* fall through */
      case 869 : /* fall through */
      case 885 :
        if ((entire_insn & 0xf00f) == 0x6005)
          { itype = SH64_COMPACT_INSN_MOVW7_COMPACT; goto extract_sfmt_movw7_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 774 : /* fall through */
      case 790 : /* fall through */
      case 806 : /* fall through */
      case 822 : /* fall through */
      case 838 : /* fall through */
      case 854 : /* fall through */
      case 870 : /* fall through */
      case 886 :
        if ((entire_insn & 0xf00f) == 0x6006)
          { itype = SH64_COMPACT_INSN_MOVL7_COMPACT; goto extract_sfmt_movl7_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 775 : /* fall through */
      case 791 : /* fall through */
      case 807 : /* fall through */
      case 823 : /* fall through */
      case 839 : /* fall through */
      case 855 : /* fall through */
      case 871 : /* fall through */
      case 887 :
        if ((entire_insn & 0xf00f) == 0x6007)
          { itype = SH64_COMPACT_INSN_NOT_COMPACT; goto extract_sfmt_mov_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 776 : /* fall through */
      case 792 : /* fall through */
      case 808 : /* fall through */
      case 824 : /* fall through */
      case 840 : /* fall through */
      case 856 : /* fall through */
      case 872 : /* fall through */
      case 888 :
        if ((entire_insn & 0xf00f) == 0x6008)
          { itype = SH64_COMPACT_INSN_SWAPB_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 777 : /* fall through */
      case 793 : /* fall through */
      case 809 : /* fall through */
      case 825 : /* fall through */
      case 841 : /* fall through */
      case 857 : /* fall through */
      case 873 : /* fall through */
      case 889 :
        if ((entire_insn & 0xf00f) == 0x6009)
          { itype = SH64_COMPACT_INSN_SWAPW_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 778 : /* fall through */
      case 794 : /* fall through */
      case 810 : /* fall through */
      case 826 : /* fall through */
      case 842 : /* fall through */
      case 858 : /* fall through */
      case 874 : /* fall through */
      case 890 :
        if ((entire_insn & 0xf00f) == 0x600a)
          { itype = SH64_COMPACT_INSN_NEGC_COMPACT; goto extract_sfmt_negc_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 779 : /* fall through */
      case 795 : /* fall through */
      case 811 : /* fall through */
      case 827 : /* fall through */
      case 843 : /* fall through */
      case 859 : /* fall through */
      case 875 : /* fall through */
      case 891 :
        if ((entire_insn & 0xf00f) == 0x600b)
          { itype = SH64_COMPACT_INSN_NEG_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 780 : /* fall through */
      case 796 : /* fall through */
      case 812 : /* fall through */
      case 828 : /* fall through */
      case 844 : /* fall through */
      case 860 : /* fall through */
      case 876 : /* fall through */
      case 892 :
        if ((entire_insn & 0xf00f) == 0x600c)
          { itype = SH64_COMPACT_INSN_EXTUB_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 781 : /* fall through */
      case 797 : /* fall through */
      case 813 : /* fall through */
      case 829 : /* fall through */
      case 845 : /* fall through */
      case 861 : /* fall through */
      case 877 : /* fall through */
      case 893 :
        if ((entire_insn & 0xf00f) == 0x600d)
          { itype = SH64_COMPACT_INSN_EXTUW_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 782 : /* fall through */
      case 798 : /* fall through */
      case 814 : /* fall through */
      case 830 : /* fall through */
      case 846 : /* fall through */
      case 862 : /* fall through */
      case 878 : /* fall through */
      case 894 :
        if ((entire_insn & 0xf00f) == 0x600e)
          { itype = SH64_COMPACT_INSN_EXTSB_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 783 : /* fall through */
      case 799 : /* fall through */
      case 815 : /* fall through */
      case 831 : /* fall through */
      case 847 : /* fall through */
      case 863 : /* fall through */
      case 879 : /* fall through */
      case 895 :
        if ((entire_insn & 0xf00f) == 0x600f)
          { itype = SH64_COMPACT_INSN_EXTSW_COMPACT; goto extract_sfmt_extsb_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 896 : /* fall through */
      case 897 : /* fall through */
      case 898 : /* fall through */
      case 899 : /* fall through */
      case 900 : /* fall through */
      case 901 : /* fall through */
      case 902 : /* fall through */
      case 903 : /* fall through */
      case 904 : /* fall through */
      case 905 : /* fall through */
      case 906 : /* fall through */
      case 907 : /* fall through */
      case 908 : /* fall through */
      case 909 : /* fall through */
      case 910 : /* fall through */
      case 911 : /* fall through */
      case 912 : /* fall through */
      case 913 : /* fall through */
      case 914 : /* fall through */
      case 915 : /* fall through */
      case 916 : /* fall through */
      case 917 : /* fall through */
      case 918 : /* fall through */
      case 919 : /* fall through */
      case 920 : /* fall through */
      case 921 : /* fall through */
      case 922 : /* fall through */
      case 923 : /* fall through */
      case 924 : /* fall through */
      case 925 : /* fall through */
      case 926 : /* fall through */
      case 927 : /* fall through */
      case 928 : /* fall through */
      case 929 : /* fall through */
      case 930 : /* fall through */
      case 931 : /* fall through */
      case 932 : /* fall through */
      case 933 : /* fall through */
      case 934 : /* fall through */
      case 935 : /* fall through */
      case 936 : /* fall through */
      case 937 : /* fall through */
      case 938 : /* fall through */
      case 939 : /* fall through */
      case 940 : /* fall through */
      case 941 : /* fall through */
      case 942 : /* fall through */
      case 943 : /* fall through */
      case 944 : /* fall through */
      case 945 : /* fall through */
      case 946 : /* fall through */
      case 947 : /* fall through */
      case 948 : /* fall through */
      case 949 : /* fall through */
      case 950 : /* fall through */
      case 951 : /* fall through */
      case 952 : /* fall through */
      case 953 : /* fall through */
      case 954 : /* fall through */
      case 955 : /* fall through */
      case 956 : /* fall through */
      case 957 : /* fall through */
      case 958 : /* fall through */
      case 959 : /* fall through */
      case 960 : /* fall through */
      case 961 : /* fall through */
      case 962 : /* fall through */
      case 963 : /* fall through */
      case 964 : /* fall through */
      case 965 : /* fall through */
      case 966 : /* fall through */
      case 967 : /* fall through */
      case 968 : /* fall through */
      case 969 : /* fall through */
      case 970 : /* fall through */
      case 971 : /* fall through */
      case 972 : /* fall through */
      case 973 : /* fall through */
      case 974 : /* fall through */
      case 975 : /* fall through */
      case 976 : /* fall through */
      case 977 : /* fall through */
      case 978 : /* fall through */
      case 979 : /* fall through */
      case 980 : /* fall through */
      case 981 : /* fall through */
      case 982 : /* fall through */
      case 983 : /* fall through */
      case 984 : /* fall through */
      case 985 : /* fall through */
      case 986 : /* fall through */
      case 987 : /* fall through */
      case 988 : /* fall through */
      case 989 : /* fall through */
      case 990 : /* fall through */
      case 991 : /* fall through */
      case 992 : /* fall through */
      case 993 : /* fall through */
      case 994 : /* fall through */
      case 995 : /* fall through */
      case 996 : /* fall through */
      case 997 : /* fall through */
      case 998 : /* fall through */
      case 999 : /* fall through */
      case 1000 : /* fall through */
      case 1001 : /* fall through */
      case 1002 : /* fall through */
      case 1003 : /* fall through */
      case 1004 : /* fall through */
      case 1005 : /* fall through */
      case 1006 : /* fall through */
      case 1007 : /* fall through */
      case 1008 : /* fall through */
      case 1009 : /* fall through */
      case 1010 : /* fall through */
      case 1011 : /* fall through */
      case 1012 : /* fall through */
      case 1013 : /* fall through */
      case 1014 : /* fall through */
      case 1015 : /* fall through */
      case 1016 : /* fall through */
      case 1017 : /* fall through */
      case 1018 : /* fall through */
      case 1019 : /* fall through */
      case 1020 : /* fall through */
      case 1021 : /* fall through */
      case 1022 : /* fall through */
      case 1023 :
        if ((entire_insn & 0xf000) == 0x7000)
          { itype = SH64_COMPACT_INSN_ADDI_COMPACT; goto extract_sfmt_addi_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1024 : /* fall through */
      case 1025 : /* fall through */
      case 1026 : /* fall through */
      case 1027 : /* fall through */
      case 1028 : /* fall through */
      case 1029 : /* fall through */
      case 1030 : /* fall through */
      case 1031 : /* fall through */
      case 1032 : /* fall through */
      case 1033 : /* fall through */
      case 1034 : /* fall through */
      case 1035 : /* fall through */
      case 1036 : /* fall through */
      case 1037 : /* fall through */
      case 1038 : /* fall through */
      case 1039 : /* fall through */
      case 1040 : /* fall through */
      case 1041 : /* fall through */
      case 1042 : /* fall through */
      case 1043 : /* fall through */
      case 1044 : /* fall through */
      case 1045 : /* fall through */
      case 1046 : /* fall through */
      case 1047 : /* fall through */
      case 1048 : /* fall through */
      case 1049 : /* fall through */
      case 1050 : /* fall through */
      case 1051 : /* fall through */
      case 1052 : /* fall through */
      case 1053 : /* fall through */
      case 1054 : /* fall through */
      case 1055 : /* fall through */
      case 1056 : /* fall through */
      case 1057 : /* fall through */
      case 1058 : /* fall through */
      case 1059 : /* fall through */
      case 1060 : /* fall through */
      case 1061 : /* fall through */
      case 1062 : /* fall through */
      case 1063 : /* fall through */
      case 1064 : /* fall through */
      case 1065 : /* fall through */
      case 1066 : /* fall through */
      case 1067 : /* fall through */
      case 1068 : /* fall through */
      case 1069 : /* fall through */
      case 1070 : /* fall through */
      case 1071 : /* fall through */
      case 1072 : /* fall through */
      case 1073 : /* fall through */
      case 1074 : /* fall through */
      case 1075 : /* fall through */
      case 1076 : /* fall through */
      case 1077 : /* fall through */
      case 1078 : /* fall through */
      case 1079 : /* fall through */
      case 1080 : /* fall through */
      case 1081 : /* fall through */
      case 1082 : /* fall through */
      case 1083 : /* fall through */
      case 1084 : /* fall through */
      case 1085 : /* fall through */
      case 1086 : /* fall through */
      case 1087 : /* fall through */
      case 1088 : /* fall through */
      case 1089 : /* fall through */
      case 1090 : /* fall through */
      case 1091 : /* fall through */
      case 1092 : /* fall through */
      case 1093 : /* fall through */
      case 1094 : /* fall through */
      case 1095 : /* fall through */
      case 1096 : /* fall through */
      case 1097 : /* fall through */
      case 1098 : /* fall through */
      case 1099 : /* fall through */
      case 1100 : /* fall through */
      case 1101 : /* fall through */
      case 1102 : /* fall through */
      case 1103 : /* fall through */
      case 1104 : /* fall through */
      case 1105 : /* fall through */
      case 1106 : /* fall through */
      case 1107 : /* fall through */
      case 1108 : /* fall through */
      case 1109 : /* fall through */
      case 1110 : /* fall through */
      case 1111 : /* fall through */
      case 1112 : /* fall through */
      case 1113 : /* fall through */
      case 1114 : /* fall through */
      case 1115 : /* fall through */
      case 1116 : /* fall through */
      case 1117 : /* fall through */
      case 1118 : /* fall through */
      case 1119 : /* fall through */
      case 1120 : /* fall through */
      case 1121 : /* fall through */
      case 1122 : /* fall through */
      case 1123 : /* fall through */
      case 1124 : /* fall through */
      case 1125 : /* fall through */
      case 1126 : /* fall through */
      case 1127 : /* fall through */
      case 1128 : /* fall through */
      case 1129 : /* fall through */
      case 1130 : /* fall through */
      case 1131 : /* fall through */
      case 1132 : /* fall through */
      case 1133 : /* fall through */
      case 1134 : /* fall through */
      case 1135 : /* fall through */
      case 1136 : /* fall through */
      case 1137 : /* fall through */
      case 1138 : /* fall through */
      case 1139 : /* fall through */
      case 1140 : /* fall through */
      case 1141 : /* fall through */
      case 1142 : /* fall through */
      case 1143 : /* fall through */
      case 1144 : /* fall through */
      case 1145 : /* fall through */
      case 1146 : /* fall through */
      case 1147 : /* fall through */
      case 1148 : /* fall through */
      case 1149 : /* fall through */
      case 1150 : /* fall through */
      case 1151 :
        {
          unsigned int val = (((insn >> 8) & (15 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xff00) == 0x8000)
              { itype = SH64_COMPACT_INSN_MOVB5_COMPACT; goto extract_sfmt_movb5_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xff00) == 0x8100)
              { itype = SH64_COMPACT_INSN_MOVW5_COMPACT; goto extract_sfmt_movw5_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 4 :
            if ((entire_insn & 0xff00) == 0x8400)
              { itype = SH64_COMPACT_INSN_MOVB10_COMPACT; goto extract_sfmt_movb10_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 5 :
            if ((entire_insn & 0xff00) == 0x8500)
              { itype = SH64_COMPACT_INSN_MOVW11_COMPACT; goto extract_sfmt_movw11_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 8 :
            if ((entire_insn & 0xff00) == 0x8800)
              { itype = SH64_COMPACT_INSN_CMPEQI_COMPACT; goto extract_sfmt_cmpeqi_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 9 :
            if ((entire_insn & 0xff00) == 0x8900)
              { itype = SH64_COMPACT_INSN_BT_COMPACT; goto extract_sfmt_bf_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 11 :
            if ((entire_insn & 0xff00) == 0x8b00)
              { itype = SH64_COMPACT_INSN_BF_COMPACT; goto extract_sfmt_bf_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 13 :
            if ((entire_insn & 0xff00) == 0x8d00)
              { itype = SH64_COMPACT_INSN_BTS_COMPACT; goto extract_sfmt_bfs_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((entire_insn & 0xff00) == 0x8f00)
              { itype = SH64_COMPACT_INSN_BFS_COMPACT; goto extract_sfmt_bfs_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1152 : /* fall through */
      case 1153 : /* fall through */
      case 1154 : /* fall through */
      case 1155 : /* fall through */
      case 1156 : /* fall through */
      case 1157 : /* fall through */
      case 1158 : /* fall through */
      case 1159 : /* fall through */
      case 1160 : /* fall through */
      case 1161 : /* fall through */
      case 1162 : /* fall through */
      case 1163 : /* fall through */
      case 1164 : /* fall through */
      case 1165 : /* fall through */
      case 1166 : /* fall through */
      case 1167 : /* fall through */
      case 1168 : /* fall through */
      case 1169 : /* fall through */
      case 1170 : /* fall through */
      case 1171 : /* fall through */
      case 1172 : /* fall through */
      case 1173 : /* fall through */
      case 1174 : /* fall through */
      case 1175 : /* fall through */
      case 1176 : /* fall through */
      case 1177 : /* fall through */
      case 1178 : /* fall through */
      case 1179 : /* fall through */
      case 1180 : /* fall through */
      case 1181 : /* fall through */
      case 1182 : /* fall through */
      case 1183 : /* fall through */
      case 1184 : /* fall through */
      case 1185 : /* fall through */
      case 1186 : /* fall through */
      case 1187 : /* fall through */
      case 1188 : /* fall through */
      case 1189 : /* fall through */
      case 1190 : /* fall through */
      case 1191 : /* fall through */
      case 1192 : /* fall through */
      case 1193 : /* fall through */
      case 1194 : /* fall through */
      case 1195 : /* fall through */
      case 1196 : /* fall through */
      case 1197 : /* fall through */
      case 1198 : /* fall through */
      case 1199 : /* fall through */
      case 1200 : /* fall through */
      case 1201 : /* fall through */
      case 1202 : /* fall through */
      case 1203 : /* fall through */
      case 1204 : /* fall through */
      case 1205 : /* fall through */
      case 1206 : /* fall through */
      case 1207 : /* fall through */
      case 1208 : /* fall through */
      case 1209 : /* fall through */
      case 1210 : /* fall through */
      case 1211 : /* fall through */
      case 1212 : /* fall through */
      case 1213 : /* fall through */
      case 1214 : /* fall through */
      case 1215 : /* fall through */
      case 1216 : /* fall through */
      case 1217 : /* fall through */
      case 1218 : /* fall through */
      case 1219 : /* fall through */
      case 1220 : /* fall through */
      case 1221 : /* fall through */
      case 1222 : /* fall through */
      case 1223 : /* fall through */
      case 1224 : /* fall through */
      case 1225 : /* fall through */
      case 1226 : /* fall through */
      case 1227 : /* fall through */
      case 1228 : /* fall through */
      case 1229 : /* fall through */
      case 1230 : /* fall through */
      case 1231 : /* fall through */
      case 1232 : /* fall through */
      case 1233 : /* fall through */
      case 1234 : /* fall through */
      case 1235 : /* fall through */
      case 1236 : /* fall through */
      case 1237 : /* fall through */
      case 1238 : /* fall through */
      case 1239 : /* fall through */
      case 1240 : /* fall through */
      case 1241 : /* fall through */
      case 1242 : /* fall through */
      case 1243 : /* fall through */
      case 1244 : /* fall through */
      case 1245 : /* fall through */
      case 1246 : /* fall through */
      case 1247 : /* fall through */
      case 1248 : /* fall through */
      case 1249 : /* fall through */
      case 1250 : /* fall through */
      case 1251 : /* fall through */
      case 1252 : /* fall through */
      case 1253 : /* fall through */
      case 1254 : /* fall through */
      case 1255 : /* fall through */
      case 1256 : /* fall through */
      case 1257 : /* fall through */
      case 1258 : /* fall through */
      case 1259 : /* fall through */
      case 1260 : /* fall through */
      case 1261 : /* fall through */
      case 1262 : /* fall through */
      case 1263 : /* fall through */
      case 1264 : /* fall through */
      case 1265 : /* fall through */
      case 1266 : /* fall through */
      case 1267 : /* fall through */
      case 1268 : /* fall through */
      case 1269 : /* fall through */
      case 1270 : /* fall through */
      case 1271 : /* fall through */
      case 1272 : /* fall through */
      case 1273 : /* fall through */
      case 1274 : /* fall through */
      case 1275 : /* fall through */
      case 1276 : /* fall through */
      case 1277 : /* fall through */
      case 1278 : /* fall through */
      case 1279 :
        if ((entire_insn & 0xf000) == 0x9000)
          { itype = SH64_COMPACT_INSN_MOVW10_COMPACT; goto extract_sfmt_movw10_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1280 : /* fall through */
      case 1281 : /* fall through */
      case 1282 : /* fall through */
      case 1283 : /* fall through */
      case 1284 : /* fall through */
      case 1285 : /* fall through */
      case 1286 : /* fall through */
      case 1287 : /* fall through */
      case 1288 : /* fall through */
      case 1289 : /* fall through */
      case 1290 : /* fall through */
      case 1291 : /* fall through */
      case 1292 : /* fall through */
      case 1293 : /* fall through */
      case 1294 : /* fall through */
      case 1295 : /* fall through */
      case 1296 : /* fall through */
      case 1297 : /* fall through */
      case 1298 : /* fall through */
      case 1299 : /* fall through */
      case 1300 : /* fall through */
      case 1301 : /* fall through */
      case 1302 : /* fall through */
      case 1303 : /* fall through */
      case 1304 : /* fall through */
      case 1305 : /* fall through */
      case 1306 : /* fall through */
      case 1307 : /* fall through */
      case 1308 : /* fall through */
      case 1309 : /* fall through */
      case 1310 : /* fall through */
      case 1311 : /* fall through */
      case 1312 : /* fall through */
      case 1313 : /* fall through */
      case 1314 : /* fall through */
      case 1315 : /* fall through */
      case 1316 : /* fall through */
      case 1317 : /* fall through */
      case 1318 : /* fall through */
      case 1319 : /* fall through */
      case 1320 : /* fall through */
      case 1321 : /* fall through */
      case 1322 : /* fall through */
      case 1323 : /* fall through */
      case 1324 : /* fall through */
      case 1325 : /* fall through */
      case 1326 : /* fall through */
      case 1327 : /* fall through */
      case 1328 : /* fall through */
      case 1329 : /* fall through */
      case 1330 : /* fall through */
      case 1331 : /* fall through */
      case 1332 : /* fall through */
      case 1333 : /* fall through */
      case 1334 : /* fall through */
      case 1335 : /* fall through */
      case 1336 : /* fall through */
      case 1337 : /* fall through */
      case 1338 : /* fall through */
      case 1339 : /* fall through */
      case 1340 : /* fall through */
      case 1341 : /* fall through */
      case 1342 : /* fall through */
      case 1343 : /* fall through */
      case 1344 : /* fall through */
      case 1345 : /* fall through */
      case 1346 : /* fall through */
      case 1347 : /* fall through */
      case 1348 : /* fall through */
      case 1349 : /* fall through */
      case 1350 : /* fall through */
      case 1351 : /* fall through */
      case 1352 : /* fall through */
      case 1353 : /* fall through */
      case 1354 : /* fall through */
      case 1355 : /* fall through */
      case 1356 : /* fall through */
      case 1357 : /* fall through */
      case 1358 : /* fall through */
      case 1359 : /* fall through */
      case 1360 : /* fall through */
      case 1361 : /* fall through */
      case 1362 : /* fall through */
      case 1363 : /* fall through */
      case 1364 : /* fall through */
      case 1365 : /* fall through */
      case 1366 : /* fall through */
      case 1367 : /* fall through */
      case 1368 : /* fall through */
      case 1369 : /* fall through */
      case 1370 : /* fall through */
      case 1371 : /* fall through */
      case 1372 : /* fall through */
      case 1373 : /* fall through */
      case 1374 : /* fall through */
      case 1375 : /* fall through */
      case 1376 : /* fall through */
      case 1377 : /* fall through */
      case 1378 : /* fall through */
      case 1379 : /* fall through */
      case 1380 : /* fall through */
      case 1381 : /* fall through */
      case 1382 : /* fall through */
      case 1383 : /* fall through */
      case 1384 : /* fall through */
      case 1385 : /* fall through */
      case 1386 : /* fall through */
      case 1387 : /* fall through */
      case 1388 : /* fall through */
      case 1389 : /* fall through */
      case 1390 : /* fall through */
      case 1391 : /* fall through */
      case 1392 : /* fall through */
      case 1393 : /* fall through */
      case 1394 : /* fall through */
      case 1395 : /* fall through */
      case 1396 : /* fall through */
      case 1397 : /* fall through */
      case 1398 : /* fall through */
      case 1399 : /* fall through */
      case 1400 : /* fall through */
      case 1401 : /* fall through */
      case 1402 : /* fall through */
      case 1403 : /* fall through */
      case 1404 : /* fall through */
      case 1405 : /* fall through */
      case 1406 : /* fall through */
      case 1407 :
        if ((entire_insn & 0xf000) == 0xa000)
          { itype = SH64_COMPACT_INSN_BRA_COMPACT; goto extract_sfmt_bra_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1408 : /* fall through */
      case 1409 : /* fall through */
      case 1410 : /* fall through */
      case 1411 : /* fall through */
      case 1412 : /* fall through */
      case 1413 : /* fall through */
      case 1414 : /* fall through */
      case 1415 : /* fall through */
      case 1416 : /* fall through */
      case 1417 : /* fall through */
      case 1418 : /* fall through */
      case 1419 : /* fall through */
      case 1420 : /* fall through */
      case 1421 : /* fall through */
      case 1422 : /* fall through */
      case 1423 : /* fall through */
      case 1424 : /* fall through */
      case 1425 : /* fall through */
      case 1426 : /* fall through */
      case 1427 : /* fall through */
      case 1428 : /* fall through */
      case 1429 : /* fall through */
      case 1430 : /* fall through */
      case 1431 : /* fall through */
      case 1432 : /* fall through */
      case 1433 : /* fall through */
      case 1434 : /* fall through */
      case 1435 : /* fall through */
      case 1436 : /* fall through */
      case 1437 : /* fall through */
      case 1438 : /* fall through */
      case 1439 : /* fall through */
      case 1440 : /* fall through */
      case 1441 : /* fall through */
      case 1442 : /* fall through */
      case 1443 : /* fall through */
      case 1444 : /* fall through */
      case 1445 : /* fall through */
      case 1446 : /* fall through */
      case 1447 : /* fall through */
      case 1448 : /* fall through */
      case 1449 : /* fall through */
      case 1450 : /* fall through */
      case 1451 : /* fall through */
      case 1452 : /* fall through */
      case 1453 : /* fall through */
      case 1454 : /* fall through */
      case 1455 : /* fall through */
      case 1456 : /* fall through */
      case 1457 : /* fall through */
      case 1458 : /* fall through */
      case 1459 : /* fall through */
      case 1460 : /* fall through */
      case 1461 : /* fall through */
      case 1462 : /* fall through */
      case 1463 : /* fall through */
      case 1464 : /* fall through */
      case 1465 : /* fall through */
      case 1466 : /* fall through */
      case 1467 : /* fall through */
      case 1468 : /* fall through */
      case 1469 : /* fall through */
      case 1470 : /* fall through */
      case 1471 : /* fall through */
      case 1472 : /* fall through */
      case 1473 : /* fall through */
      case 1474 : /* fall through */
      case 1475 : /* fall through */
      case 1476 : /* fall through */
      case 1477 : /* fall through */
      case 1478 : /* fall through */
      case 1479 : /* fall through */
      case 1480 : /* fall through */
      case 1481 : /* fall through */
      case 1482 : /* fall through */
      case 1483 : /* fall through */
      case 1484 : /* fall through */
      case 1485 : /* fall through */
      case 1486 : /* fall through */
      case 1487 : /* fall through */
      case 1488 : /* fall through */
      case 1489 : /* fall through */
      case 1490 : /* fall through */
      case 1491 : /* fall through */
      case 1492 : /* fall through */
      case 1493 : /* fall through */
      case 1494 : /* fall through */
      case 1495 : /* fall through */
      case 1496 : /* fall through */
      case 1497 : /* fall through */
      case 1498 : /* fall through */
      case 1499 : /* fall through */
      case 1500 : /* fall through */
      case 1501 : /* fall through */
      case 1502 : /* fall through */
      case 1503 : /* fall through */
      case 1504 : /* fall through */
      case 1505 : /* fall through */
      case 1506 : /* fall through */
      case 1507 : /* fall through */
      case 1508 : /* fall through */
      case 1509 : /* fall through */
      case 1510 : /* fall through */
      case 1511 : /* fall through */
      case 1512 : /* fall through */
      case 1513 : /* fall through */
      case 1514 : /* fall through */
      case 1515 : /* fall through */
      case 1516 : /* fall through */
      case 1517 : /* fall through */
      case 1518 : /* fall through */
      case 1519 : /* fall through */
      case 1520 : /* fall through */
      case 1521 : /* fall through */
      case 1522 : /* fall through */
      case 1523 : /* fall through */
      case 1524 : /* fall through */
      case 1525 : /* fall through */
      case 1526 : /* fall through */
      case 1527 : /* fall through */
      case 1528 : /* fall through */
      case 1529 : /* fall through */
      case 1530 : /* fall through */
      case 1531 : /* fall through */
      case 1532 : /* fall through */
      case 1533 : /* fall through */
      case 1534 : /* fall through */
      case 1535 :
        if ((entire_insn & 0xf000) == 0xb000)
          { itype = SH64_COMPACT_INSN_BSR_COMPACT; goto extract_sfmt_bsr_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1536 : /* fall through */
      case 1537 : /* fall through */
      case 1538 : /* fall through */
      case 1539 : /* fall through */
      case 1540 : /* fall through */
      case 1541 : /* fall through */
      case 1542 : /* fall through */
      case 1543 : /* fall through */
      case 1544 : /* fall through */
      case 1545 : /* fall through */
      case 1546 : /* fall through */
      case 1547 : /* fall through */
      case 1548 : /* fall through */
      case 1549 : /* fall through */
      case 1550 : /* fall through */
      case 1551 : /* fall through */
      case 1552 : /* fall through */
      case 1553 : /* fall through */
      case 1554 : /* fall through */
      case 1555 : /* fall through */
      case 1556 : /* fall through */
      case 1557 : /* fall through */
      case 1558 : /* fall through */
      case 1559 : /* fall through */
      case 1560 : /* fall through */
      case 1561 : /* fall through */
      case 1562 : /* fall through */
      case 1563 : /* fall through */
      case 1564 : /* fall through */
      case 1565 : /* fall through */
      case 1566 : /* fall through */
      case 1567 : /* fall through */
      case 1568 : /* fall through */
      case 1569 : /* fall through */
      case 1570 : /* fall through */
      case 1571 : /* fall through */
      case 1572 : /* fall through */
      case 1573 : /* fall through */
      case 1574 : /* fall through */
      case 1575 : /* fall through */
      case 1576 : /* fall through */
      case 1577 : /* fall through */
      case 1578 : /* fall through */
      case 1579 : /* fall through */
      case 1580 : /* fall through */
      case 1581 : /* fall through */
      case 1582 : /* fall through */
      case 1583 : /* fall through */
      case 1584 : /* fall through */
      case 1585 : /* fall through */
      case 1586 : /* fall through */
      case 1587 : /* fall through */
      case 1588 : /* fall through */
      case 1589 : /* fall through */
      case 1590 : /* fall through */
      case 1591 : /* fall through */
      case 1592 : /* fall through */
      case 1593 : /* fall through */
      case 1594 : /* fall through */
      case 1595 : /* fall through */
      case 1596 : /* fall through */
      case 1597 : /* fall through */
      case 1598 : /* fall through */
      case 1599 : /* fall through */
      case 1600 : /* fall through */
      case 1601 : /* fall through */
      case 1602 : /* fall through */
      case 1603 : /* fall through */
      case 1604 : /* fall through */
      case 1605 : /* fall through */
      case 1606 : /* fall through */
      case 1607 : /* fall through */
      case 1608 : /* fall through */
      case 1609 : /* fall through */
      case 1610 : /* fall through */
      case 1611 : /* fall through */
      case 1612 : /* fall through */
      case 1613 : /* fall through */
      case 1614 : /* fall through */
      case 1615 : /* fall through */
      case 1616 : /* fall through */
      case 1617 : /* fall through */
      case 1618 : /* fall through */
      case 1619 : /* fall through */
      case 1620 : /* fall through */
      case 1621 : /* fall through */
      case 1622 : /* fall through */
      case 1623 : /* fall through */
      case 1624 : /* fall through */
      case 1625 : /* fall through */
      case 1626 : /* fall through */
      case 1627 : /* fall through */
      case 1628 : /* fall through */
      case 1629 : /* fall through */
      case 1630 : /* fall through */
      case 1631 : /* fall through */
      case 1632 : /* fall through */
      case 1633 : /* fall through */
      case 1634 : /* fall through */
      case 1635 : /* fall through */
      case 1636 : /* fall through */
      case 1637 : /* fall through */
      case 1638 : /* fall through */
      case 1639 : /* fall through */
      case 1640 : /* fall through */
      case 1641 : /* fall through */
      case 1642 : /* fall through */
      case 1643 : /* fall through */
      case 1644 : /* fall through */
      case 1645 : /* fall through */
      case 1646 : /* fall through */
      case 1647 : /* fall through */
      case 1648 : /* fall through */
      case 1649 : /* fall through */
      case 1650 : /* fall through */
      case 1651 : /* fall through */
      case 1652 : /* fall through */
      case 1653 : /* fall through */
      case 1654 : /* fall through */
      case 1655 : /* fall through */
      case 1656 : /* fall through */
      case 1657 : /* fall through */
      case 1658 : /* fall through */
      case 1659 : /* fall through */
      case 1660 : /* fall through */
      case 1661 : /* fall through */
      case 1662 : /* fall through */
      case 1663 :
        {
          unsigned int val = (((insn >> 8) & (15 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xff00) == 0xc000)
              { itype = SH64_COMPACT_INSN_MOVB4_COMPACT; goto extract_sfmt_movb4_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xff00) == 0xc100)
              { itype = SH64_COMPACT_INSN_MOVW4_COMPACT; goto extract_sfmt_movw4_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 2 :
            if ((entire_insn & 0xff00) == 0xc200)
              { itype = SH64_COMPACT_INSN_MOVL4_COMPACT; goto extract_sfmt_movl4_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 3 :
            if ((entire_insn & 0xff00) == 0xc300)
              { itype = SH64_COMPACT_INSN_TRAPA_COMPACT; goto extract_sfmt_trapa_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 4 :
            if ((entire_insn & 0xff00) == 0xc400)
              { itype = SH64_COMPACT_INSN_MOVB9_COMPACT; goto extract_sfmt_movb9_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 5 :
            if ((entire_insn & 0xff00) == 0xc500)
              { itype = SH64_COMPACT_INSN_MOVW9_COMPACT; goto extract_sfmt_movw9_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 6 :
            if ((entire_insn & 0xff00) == 0xc600)
              { itype = SH64_COMPACT_INSN_MOVL9_COMPACT; goto extract_sfmt_movl9_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 7 :
            if ((entire_insn & 0xff00) == 0xc700)
              { itype = SH64_COMPACT_INSN_MOVA_COMPACT; goto extract_sfmt_mova_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 8 :
            if ((entire_insn & 0xff00) == 0xc800)
              { itype = SH64_COMPACT_INSN_TSTI_COMPACT; goto extract_sfmt_tsti_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 9 :
            if ((entire_insn & 0xff00) == 0xc900)
              { itype = SH64_COMPACT_INSN_ANDI_COMPACT; goto extract_sfmt_andi_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 10 :
            if ((entire_insn & 0xff00) == 0xca00)
              { itype = SH64_COMPACT_INSN_XORI_COMPACT; goto extract_sfmt_andi_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 11 :
            if ((entire_insn & 0xff00) == 0xcb00)
              { itype = SH64_COMPACT_INSN_ORI_COMPACT; goto extract_sfmt_andi_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 12 :
            if ((entire_insn & 0xff00) == 0xcc00)
              { itype = SH64_COMPACT_INSN_TSTB_COMPACT; goto extract_sfmt_tstb_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 13 :
            if ((entire_insn & 0xff00) == 0xcd00)
              { itype = SH64_COMPACT_INSN_ANDB_COMPACT; goto extract_sfmt_andb_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 14 :
            if ((entire_insn & 0xff00) == 0xce00)
              { itype = SH64_COMPACT_INSN_XORB_COMPACT; goto extract_sfmt_andb_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 15 :
            if ((entire_insn & 0xff00) == 0xcf00)
              { itype = SH64_COMPACT_INSN_ORB_COMPACT; goto extract_sfmt_andb_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1664 : /* fall through */
      case 1665 : /* fall through */
      case 1666 : /* fall through */
      case 1667 : /* fall through */
      case 1668 : /* fall through */
      case 1669 : /* fall through */
      case 1670 : /* fall through */
      case 1671 : /* fall through */
      case 1672 : /* fall through */
      case 1673 : /* fall through */
      case 1674 : /* fall through */
      case 1675 : /* fall through */
      case 1676 : /* fall through */
      case 1677 : /* fall through */
      case 1678 : /* fall through */
      case 1679 : /* fall through */
      case 1680 : /* fall through */
      case 1681 : /* fall through */
      case 1682 : /* fall through */
      case 1683 : /* fall through */
      case 1684 : /* fall through */
      case 1685 : /* fall through */
      case 1686 : /* fall through */
      case 1687 : /* fall through */
      case 1688 : /* fall through */
      case 1689 : /* fall through */
      case 1690 : /* fall through */
      case 1691 : /* fall through */
      case 1692 : /* fall through */
      case 1693 : /* fall through */
      case 1694 : /* fall through */
      case 1695 : /* fall through */
      case 1696 : /* fall through */
      case 1697 : /* fall through */
      case 1698 : /* fall through */
      case 1699 : /* fall through */
      case 1700 : /* fall through */
      case 1701 : /* fall through */
      case 1702 : /* fall through */
      case 1703 : /* fall through */
      case 1704 : /* fall through */
      case 1705 : /* fall through */
      case 1706 : /* fall through */
      case 1707 : /* fall through */
      case 1708 : /* fall through */
      case 1709 : /* fall through */
      case 1710 : /* fall through */
      case 1711 : /* fall through */
      case 1712 : /* fall through */
      case 1713 : /* fall through */
      case 1714 : /* fall through */
      case 1715 : /* fall through */
      case 1716 : /* fall through */
      case 1717 : /* fall through */
      case 1718 : /* fall through */
      case 1719 : /* fall through */
      case 1720 : /* fall through */
      case 1721 : /* fall through */
      case 1722 : /* fall through */
      case 1723 : /* fall through */
      case 1724 : /* fall through */
      case 1725 : /* fall through */
      case 1726 : /* fall through */
      case 1727 : /* fall through */
      case 1728 : /* fall through */
      case 1729 : /* fall through */
      case 1730 : /* fall through */
      case 1731 : /* fall through */
      case 1732 : /* fall through */
      case 1733 : /* fall through */
      case 1734 : /* fall through */
      case 1735 : /* fall through */
      case 1736 : /* fall through */
      case 1737 : /* fall through */
      case 1738 : /* fall through */
      case 1739 : /* fall through */
      case 1740 : /* fall through */
      case 1741 : /* fall through */
      case 1742 : /* fall through */
      case 1743 : /* fall through */
      case 1744 : /* fall through */
      case 1745 : /* fall through */
      case 1746 : /* fall through */
      case 1747 : /* fall through */
      case 1748 : /* fall through */
      case 1749 : /* fall through */
      case 1750 : /* fall through */
      case 1751 : /* fall through */
      case 1752 : /* fall through */
      case 1753 : /* fall through */
      case 1754 : /* fall through */
      case 1755 : /* fall through */
      case 1756 : /* fall through */
      case 1757 : /* fall through */
      case 1758 : /* fall through */
      case 1759 : /* fall through */
      case 1760 : /* fall through */
      case 1761 : /* fall through */
      case 1762 : /* fall through */
      case 1763 : /* fall through */
      case 1764 : /* fall through */
      case 1765 : /* fall through */
      case 1766 : /* fall through */
      case 1767 : /* fall through */
      case 1768 : /* fall through */
      case 1769 : /* fall through */
      case 1770 : /* fall through */
      case 1771 : /* fall through */
      case 1772 : /* fall through */
      case 1773 : /* fall through */
      case 1774 : /* fall through */
      case 1775 : /* fall through */
      case 1776 : /* fall through */
      case 1777 : /* fall through */
      case 1778 : /* fall through */
      case 1779 : /* fall through */
      case 1780 : /* fall through */
      case 1781 : /* fall through */
      case 1782 : /* fall through */
      case 1783 : /* fall through */
      case 1784 : /* fall through */
      case 1785 : /* fall through */
      case 1786 : /* fall through */
      case 1787 : /* fall through */
      case 1788 : /* fall through */
      case 1789 : /* fall through */
      case 1790 : /* fall through */
      case 1791 :
        if ((entire_insn & 0xf000) == 0xd000)
          { itype = SH64_COMPACT_INSN_MOVL10_COMPACT; goto extract_sfmt_movl10_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1792 : /* fall through */
      case 1793 : /* fall through */
      case 1794 : /* fall through */
      case 1795 : /* fall through */
      case 1796 : /* fall through */
      case 1797 : /* fall through */
      case 1798 : /* fall through */
      case 1799 : /* fall through */
      case 1800 : /* fall through */
      case 1801 : /* fall through */
      case 1802 : /* fall through */
      case 1803 : /* fall through */
      case 1804 : /* fall through */
      case 1805 : /* fall through */
      case 1806 : /* fall through */
      case 1807 : /* fall through */
      case 1808 : /* fall through */
      case 1809 : /* fall through */
      case 1810 : /* fall through */
      case 1811 : /* fall through */
      case 1812 : /* fall through */
      case 1813 : /* fall through */
      case 1814 : /* fall through */
      case 1815 : /* fall through */
      case 1816 : /* fall through */
      case 1817 : /* fall through */
      case 1818 : /* fall through */
      case 1819 : /* fall through */
      case 1820 : /* fall through */
      case 1821 : /* fall through */
      case 1822 : /* fall through */
      case 1823 : /* fall through */
      case 1824 : /* fall through */
      case 1825 : /* fall through */
      case 1826 : /* fall through */
      case 1827 : /* fall through */
      case 1828 : /* fall through */
      case 1829 : /* fall through */
      case 1830 : /* fall through */
      case 1831 : /* fall through */
      case 1832 : /* fall through */
      case 1833 : /* fall through */
      case 1834 : /* fall through */
      case 1835 : /* fall through */
      case 1836 : /* fall through */
      case 1837 : /* fall through */
      case 1838 : /* fall through */
      case 1839 : /* fall through */
      case 1840 : /* fall through */
      case 1841 : /* fall through */
      case 1842 : /* fall through */
      case 1843 : /* fall through */
      case 1844 : /* fall through */
      case 1845 : /* fall through */
      case 1846 : /* fall through */
      case 1847 : /* fall through */
      case 1848 : /* fall through */
      case 1849 : /* fall through */
      case 1850 : /* fall through */
      case 1851 : /* fall through */
      case 1852 : /* fall through */
      case 1853 : /* fall through */
      case 1854 : /* fall through */
      case 1855 : /* fall through */
      case 1856 : /* fall through */
      case 1857 : /* fall through */
      case 1858 : /* fall through */
      case 1859 : /* fall through */
      case 1860 : /* fall through */
      case 1861 : /* fall through */
      case 1862 : /* fall through */
      case 1863 : /* fall through */
      case 1864 : /* fall through */
      case 1865 : /* fall through */
      case 1866 : /* fall through */
      case 1867 : /* fall through */
      case 1868 : /* fall through */
      case 1869 : /* fall through */
      case 1870 : /* fall through */
      case 1871 : /* fall through */
      case 1872 : /* fall through */
      case 1873 : /* fall through */
      case 1874 : /* fall through */
      case 1875 : /* fall through */
      case 1876 : /* fall through */
      case 1877 : /* fall through */
      case 1878 : /* fall through */
      case 1879 : /* fall through */
      case 1880 : /* fall through */
      case 1881 : /* fall through */
      case 1882 : /* fall through */
      case 1883 : /* fall through */
      case 1884 : /* fall through */
      case 1885 : /* fall through */
      case 1886 : /* fall through */
      case 1887 : /* fall through */
      case 1888 : /* fall through */
      case 1889 : /* fall through */
      case 1890 : /* fall through */
      case 1891 : /* fall through */
      case 1892 : /* fall through */
      case 1893 : /* fall through */
      case 1894 : /* fall through */
      case 1895 : /* fall through */
      case 1896 : /* fall through */
      case 1897 : /* fall through */
      case 1898 : /* fall through */
      case 1899 : /* fall through */
      case 1900 : /* fall through */
      case 1901 : /* fall through */
      case 1902 : /* fall through */
      case 1903 : /* fall through */
      case 1904 : /* fall through */
      case 1905 : /* fall through */
      case 1906 : /* fall through */
      case 1907 : /* fall through */
      case 1908 : /* fall through */
      case 1909 : /* fall through */
      case 1910 : /* fall through */
      case 1911 : /* fall through */
      case 1912 : /* fall through */
      case 1913 : /* fall through */
      case 1914 : /* fall through */
      case 1915 : /* fall through */
      case 1916 : /* fall through */
      case 1917 : /* fall through */
      case 1918 : /* fall through */
      case 1919 :
        if ((entire_insn & 0xf000) == 0xe000)
          { itype = SH64_COMPACT_INSN_MOVI_COMPACT; goto extract_sfmt_movi_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1920 : /* fall through */
      case 1936 : /* fall through */
      case 1952 : /* fall through */
      case 1968 : /* fall through */
      case 1984 : /* fall through */
      case 2000 : /* fall through */
      case 2016 : /* fall through */
      case 2032 :
        if ((entire_insn & 0xf00f) == 0xf000)
          { itype = SH64_COMPACT_INSN_FADD_COMPACT; goto extract_sfmt_fadd_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1921 : /* fall through */
      case 1937 : /* fall through */
      case 1953 : /* fall through */
      case 1969 : /* fall through */
      case 1985 : /* fall through */
      case 2001 : /* fall through */
      case 2017 : /* fall through */
      case 2033 :
        if ((entire_insn & 0xf00f) == 0xf001)
          { itype = SH64_COMPACT_INSN_FSUB_COMPACT; goto extract_sfmt_fadd_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1922 : /* fall through */
      case 1938 : /* fall through */
      case 1954 : /* fall through */
      case 1970 : /* fall through */
      case 1986 : /* fall through */
      case 2002 : /* fall through */
      case 2018 : /* fall through */
      case 2034 :
        if ((entire_insn & 0xf00f) == 0xf002)
          { itype = SH64_COMPACT_INSN_FMUL_COMPACT; goto extract_sfmt_fadd_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1923 : /* fall through */
      case 1939 : /* fall through */
      case 1955 : /* fall through */
      case 1971 : /* fall through */
      case 1987 : /* fall through */
      case 2003 : /* fall through */
      case 2019 : /* fall through */
      case 2035 :
        if ((entire_insn & 0xf00f) == 0xf003)
          { itype = SH64_COMPACT_INSN_FDIV_COMPACT; goto extract_sfmt_fadd_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1924 : /* fall through */
      case 1940 : /* fall through */
      case 1956 : /* fall through */
      case 1972 : /* fall through */
      case 1988 : /* fall through */
      case 2004 : /* fall through */
      case 2020 : /* fall through */
      case 2036 :
        if ((entire_insn & 0xf00f) == 0xf004)
          { itype = SH64_COMPACT_INSN_FCMPEQ_COMPACT; goto extract_sfmt_fcmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1925 : /* fall through */
      case 1941 : /* fall through */
      case 1957 : /* fall through */
      case 1973 : /* fall through */
      case 1989 : /* fall through */
      case 2005 : /* fall through */
      case 2021 : /* fall through */
      case 2037 :
        if ((entire_insn & 0xf00f) == 0xf005)
          { itype = SH64_COMPACT_INSN_FCMPGT_COMPACT; goto extract_sfmt_fcmpeq_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1926 : /* fall through */
      case 1942 : /* fall through */
      case 1958 : /* fall through */
      case 1974 : /* fall through */
      case 1990 : /* fall through */
      case 2006 : /* fall through */
      case 2022 : /* fall through */
      case 2038 :
        if ((entire_insn & 0xf00f) == 0xf006)
          { itype = SH64_COMPACT_INSN_FMOV4_COMPACT; goto extract_sfmt_fmov4_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1927 : /* fall through */
      case 1943 : /* fall through */
      case 1959 : /* fall through */
      case 1975 : /* fall through */
      case 1991 : /* fall through */
      case 2007 : /* fall through */
      case 2023 : /* fall through */
      case 2039 :
        if ((entire_insn & 0xf00f) == 0xf007)
          { itype = SH64_COMPACT_INSN_FMOV7_COMPACT; goto extract_sfmt_fmov7_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1928 : /* fall through */
      case 1944 : /* fall through */
      case 1960 : /* fall through */
      case 1976 : /* fall through */
      case 1992 : /* fall through */
      case 2008 : /* fall through */
      case 2024 : /* fall through */
      case 2040 :
        if ((entire_insn & 0xf00f) == 0xf008)
          { itype = SH64_COMPACT_INSN_FMOV2_COMPACT; goto extract_sfmt_fmov2_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1929 : /* fall through */
      case 1945 : /* fall through */
      case 1961 : /* fall through */
      case 1977 : /* fall through */
      case 1993 : /* fall through */
      case 2009 : /* fall through */
      case 2025 : /* fall through */
      case 2041 :
        if ((entire_insn & 0xf00f) == 0xf009)
          { itype = SH64_COMPACT_INSN_FMOV3_COMPACT; goto extract_sfmt_fmov3_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1930 : /* fall through */
      case 1946 : /* fall through */
      case 1962 : /* fall through */
      case 1978 : /* fall through */
      case 1994 : /* fall through */
      case 2010 : /* fall through */
      case 2026 : /* fall through */
      case 2042 :
        if ((entire_insn & 0xf00f) == 0xf00a)
          { itype = SH64_COMPACT_INSN_FMOV5_COMPACT; goto extract_sfmt_fmov5_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1931 : /* fall through */
      case 1947 : /* fall through */
      case 1963 : /* fall through */
      case 1979 : /* fall through */
      case 1995 : /* fall through */
      case 2011 : /* fall through */
      case 2027 : /* fall through */
      case 2043 :
        if ((entire_insn & 0xf00f) == 0xf00b)
          { itype = SH64_COMPACT_INSN_FMOV6_COMPACT; goto extract_sfmt_fmov6_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1932 : /* fall through */
      case 1948 : /* fall through */
      case 1964 : /* fall through */
      case 1980 : /* fall through */
      case 1996 : /* fall through */
      case 2012 : /* fall through */
      case 2028 : /* fall through */
      case 2044 :
        if ((entire_insn & 0xf00f) == 0xf00c)
          { itype = SH64_COMPACT_INSN_FMOV1_COMPACT; goto extract_sfmt_fmov1_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1933 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0xf00d)
              { itype = SH64_COMPACT_INSN_FSTS_COMPACT; goto extract_sfmt_fsts_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0xf08d)
              { itype = SH64_COMPACT_INSN_FLDI0_COMPACT; goto extract_sfmt_fldi0_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1934 : /* fall through */
      case 1950 : /* fall through */
      case 1966 : /* fall through */
      case 1982 : /* fall through */
      case 1998 : /* fall through */
      case 2014 : /* fall through */
      case 2030 : /* fall through */
      case 2046 :
        if ((entire_insn & 0xf00f) == 0xf00e)
          { itype = SH64_COMPACT_INSN_FMAC_COMPACT; goto extract_sfmt_fmac_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 1949 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0xf01d)
              { itype = SH64_COMPACT_INSN_FLDS_COMPACT; goto extract_sfmt_flds_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0xf09d)
              { itype = SH64_COMPACT_INSN_FLDI1_COMPACT; goto extract_sfmt_fldi0_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1965 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0xf02d)
              { itype = SH64_COMPACT_INSN_FLOAT_COMPACT; goto extract_sfmt_float_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf1ff) == 0xf0ad)
              { itype = SH64_COMPACT_INSN_FCNVSD_COMPACT; goto extract_sfmt_fcnvsd_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1981 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0xf03d)
              { itype = SH64_COMPACT_INSN_FTRC_COMPACT; goto extract_sfmt_ftrc_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf1ff) == 0xf0bd)
              { itype = SH64_COMPACT_INSN_FCNVDS_COMPACT; goto extract_sfmt_fcnvds_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1997 :
        if ((entire_insn & 0xf0ff) == 0xf04d)
          { itype = SH64_COMPACT_INSN_FNEG_COMPACT; goto extract_sfmt_fabs_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 2013 :
        if ((entire_insn & 0xf0ff) == 0xf05d)
          { itype = SH64_COMPACT_INSN_FABS_COMPACT; goto extract_sfmt_fabs_compact; }
        itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      case 2029 :
        {
          unsigned int val = (((insn >> 7) & (1 << 0)));
          switch (val)
          {
          case 0 :
            if ((entire_insn & 0xf0ff) == 0xf06d)
              { itype = SH64_COMPACT_INSN_FSQRT_COMPACT; goto extract_sfmt_fabs_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xf0ff) == 0xf0ed)
              { itype = SH64_COMPACT_INSN_FIPR_COMPACT; goto extract_sfmt_fipr_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 2045 :
        {
          unsigned int val = (((insn >> 10) & (1 << 1)) | ((insn >> 9) & (1 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 2 :
            if ((entire_insn & 0xf3ff) == 0xf1fd)
              { itype = SH64_COMPACT_INSN_FTRV_COMPACT; goto extract_sfmt_ftrv_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 1 :
            if ((entire_insn & 0xffff) == 0xf3fd)
              { itype = SH64_COMPACT_INSN_FSCHG_COMPACT; goto extract_sfmt_fschg_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          case 3 :
            if ((entire_insn & 0xffff) == 0xfbfd)
              { itype = SH64_COMPACT_INSN_FRCHG_COMPACT; goto extract_sfmt_frchg_compact; }
            itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      default : itype = SH64_COMPACT_INSN_X_INVALID; goto extract_sfmt_empty;
      }
    }
  }

  /* The instruction has been decoded, now extract the fields.  */

 extract_sfmt_empty:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_empty", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_add_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addi_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_rn;
    UINT f_imm8;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi_compact", "f_imm8 0x%x", 'x', f_imm8, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addc_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addc_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addv_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addv_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_and_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_and_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm64) = f_rm;
      FLD (in_rn64) = f_rn;
      FLD (out_rn64) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andi_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andi_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_andb_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andb_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bf_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bf_compact.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (1))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bf_compact", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bfs_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bf_compact.f
    SI f_disp8;

    f_disp8 = ((((EXTRACT_MSB0_INT (insn, 16, 8, 8)) << (1))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp8) = f_disp8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bfs_compact", "disp8 0x%x", 'x', f_disp8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bra_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bra_compact.f
    SI f_disp12;

    f_disp12 = ((((EXTRACT_MSB0_INT (insn, 16, 4, 12)) << (1))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp12) = f_disp12;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bra_compact", "disp12 0x%x", 'x', f_disp12, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_braf_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_braf_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_brk_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_brk_compact", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bsr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bra_compact.f
    SI f_disp12;

    f_disp12 = ((((EXTRACT_MSB0_INT (insn, 16, 4, 12)) << (1))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (i_disp12) = f_disp12;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bsr_compact", "disp12 0x%x", 'x', f_disp12, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bsrf_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bsrf_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_clrmac_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_clrmac_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_clrs_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_clrs_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_clrt_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_clrt_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_cmpeq_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpeq_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmpeqi_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmpeqi_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmppl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmppl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_div0s_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_div0s_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_div0u_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_div0u_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_div1_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_div1_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_divu_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_divu_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_dmulsl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_dmulsl_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_dt_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_dt_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_extsb_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_extsb_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fabs_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fabs_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fsdn) = f_rn;
      FLD (out_fsdn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fadd_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fadd_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fsdm) = f_rm;
      FLD (in_fsdn) = f_rn;
      FLD (out_fsdn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcmpeq_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcmpeq_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fsdm) = f_rm;
      FLD (in_fsdn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcnvds_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
    SI f_dn;

    f_dn = ((EXTRACT_MSB0_UINT (insn, 16, 4, 3)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_dn) = f_dn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcnvds_compact", "f_dn 0x%x", 'x', f_dn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drn) = f_dn;
      FLD (out_fpul) = 32;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcnvsd_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
    SI f_dn;

    f_dn = ((EXTRACT_MSB0_UINT (insn, 16, 4, 3)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_dn) = f_dn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcnvsd_compact", "f_dn 0x%x", 'x', f_dn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpul) = 32;
      FLD (out_drn) = f_dn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fipr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
    SI f_vn;
    SI f_vm;

    f_vn = ((EXTRACT_MSB0_UINT (insn, 16, 4, 2)) << (2));
    f_vm = ((EXTRACT_MSB0_UINT (insn, 16, 6, 2)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_vm) = f_vm;
  FLD (f_vn) = f_vn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fipr_compact", "f_vm 0x%x", 'x', f_vm, "f_vn 0x%x", 'x', f_vn, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_flds_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_flds_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frn) = f_rn;
      FLD (out_fpul) = 32;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldi0_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldi0_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_frn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_float_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_float_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpul) = 32;
      FLD (out_fsdn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmac_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmac_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fr0) = 0;
      FLD (in_frm) = f_rm;
      FLD (in_frn) = f_rn;
      FLD (out_frn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov1_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov1_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fmovm) = f_rm;
      FLD (out_fmovn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov2_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov2_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_fmovn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov3_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov3_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_fmovn) = f_rn;
      FLD (out_rm) = f_rm;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov4_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov4_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (out_fmovn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov5_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov5_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fmovm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov6_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov6_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fmovm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov7_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov7_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fmovm) = f_rm;
      FLD (in_r0) = 0;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov8_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
    SI f_dn;
    UINT f_rm;
    SI f_imm12x8;

    f_dn = ((EXTRACT_MSB0_UINT (insn, 32, 4, 3)) << (1));
    f_rm = EXTRACT_MSB0_UINT (insn, 32, 8, 4);
    f_imm12x8 = ((EXTRACT_MSB0_INT (insn, 32, 20, 12)) << (3));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm12x8) = f_imm12x8;
  FLD (f_rm) = f_rm;
  FLD (f_dn) = f_dn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov8_compact", "f_imm12x8 0x%x", 'x', f_imm12x8, "f_rm 0x%x", 'x', f_rm, "f_dn 0x%x", 'x', f_dn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_drn) = f_dn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmov9_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fmov9_compact.f
    UINT f_rn;
    SI f_dm;
    SI f_imm12x8;

    f_rn = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_dm = ((EXTRACT_MSB0_UINT (insn, 32, 8, 3)) << (1));
    f_imm12x8 = ((EXTRACT_MSB0_INT (insn, 32, 20, 12)) << (3));

  /* Record the fields for the semantic handler.  */
  FLD (f_dm) = f_dm;
  FLD (f_imm12x8) = f_imm12x8;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmov9_compact", "f_dm 0x%x", 'x', f_dm, "f_imm12x8 0x%x", 'x', f_imm12x8, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drm) = f_dm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_frchg_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_frchg_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_fschg_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fschg_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_fsts_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fsts_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpul) = 32;
      FLD (out_frn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ftrc_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ftrc_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fsdn) = f_rn;
      FLD (out_fpul) = 32;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ftrv_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
    SI f_vn;

    f_vn = ((EXTRACT_MSB0_UINT (insn, 16, 4, 2)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_vn) = f_vn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ftrv_compact", "f_vn 0x%x", 'x', f_vn, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_ldc_gbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldc_gbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldc_vbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldc_vbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldc_sr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldc_sr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldcl_gbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldcl_gbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldcl_vbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldcl_vbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lds_fpscr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lds_fpscr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldsl_fpscr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldsl_fpscr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lds_fpul_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lds_fpul_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_fpul) = 32;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldsl_fpul_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldsl_fpul_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_fpul) = 32;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lds_mach_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lds_mach_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldsl_mach_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldsl_mach_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lds_macl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lds_macl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldsl_macl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldsl_macl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lds_pr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lds_pr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldsl_pr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldsl_pr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_macl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_macl_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_macw_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_macw_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mov_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mov_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm64) = f_rm;
      FLD (out_rn64) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movi_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_rn;
    UINT f_imm8;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movi_compact", "f_imm8 0x%x", 'x', f_imm8, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movi20_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movi20_compact.f
    UINT f_rn;
    INT f_imm20_hi;
    UINT f_imm20_lo;
    INT f_imm20;

    f_rn = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_imm20_hi = EXTRACT_MSB0_INT (insn, 32, 8, 4);
    f_imm20_lo = EXTRACT_MSB0_UINT (insn, 32, 16, 16);
  f_imm20 = ((((f_imm20_hi) << (16))) | (f_imm20_lo));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm20) = f_imm20;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movi20_compact", "f_imm20 0x%x", 'x', f_imm20, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb1_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb1_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb2_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb2_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb3_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb3_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb4_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb4_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb5_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
    UINT f_rm;
    UINT f_imm4;

    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4) = f_imm4;
  FLD (f_rm) = f_rm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb5_compact", "f_imm4 0x%x", 'x', f_imm4, "f_rm 0x%x", 'x', f_rm, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb6_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb6_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb7_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb7_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb8_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb8_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb9_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb9_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movb10_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
    UINT f_rm;
    UINT f_imm4;

    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4 = EXTRACT_MSB0_UINT (insn, 16, 12, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4) = f_imm4;
  FLD (f_rm) = f_rm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movb10_compact", "f_imm4 0x%x", 'x', f_imm4, "f_rm 0x%x", 'x', f_rm, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl1_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl1_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl2_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl2_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl3_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl3_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl4_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
    SI f_imm8x4;

    f_imm8x4 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x4) = f_imm8x4;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl4_compact", "f_imm8x4 0x%x", 'x', f_imm8x4, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl5_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
    UINT f_rn;
    UINT f_rm;
    SI f_imm4x4;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4x4 = ((EXTRACT_MSB0_UINT (insn, 16, 12, 4)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4x4) = f_imm4x4;
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl5_compact", "f_imm4x4 0x%x", 'x', f_imm4x4, "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl6_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl6_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl7_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl7_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl8_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl8_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl9_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
    SI f_imm8x4;

    f_imm8x4 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x4) = f_imm8x4;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl9_compact", "f_imm8x4 0x%x", 'x', f_imm8x4, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl10_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
    UINT f_rn;
    SI f_imm8x4;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_imm8x4 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x4) = f_imm8x4;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl10_compact", "f_imm8x4 0x%x", 'x', f_imm8x4, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl11_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
    UINT f_rn;
    UINT f_rm;
    SI f_imm4x4;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4x4 = ((EXTRACT_MSB0_UINT (insn, 16, 12, 4)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4x4) = f_imm4x4;
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl11_compact", "f_imm4x4 0x%x", 'x', f_imm4x4, "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl12_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;
    SI f_imm12x4;

    f_rn = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 32, 8, 4);
    f_imm12x4 = ((EXTRACT_MSB0_INT (insn, 32, 20, 12)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm12x4) = f_imm12x4;
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl12_compact", "f_imm12x4 0x%x", 'x', f_imm12x4, "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movl13_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;
    SI f_imm12x4;

    f_rn = EXTRACT_MSB0_UINT (insn, 32, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 32, 8, 4);
    f_imm12x4 = ((EXTRACT_MSB0_INT (insn, 32, 20, 12)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm12x4) = f_imm12x4;
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movl13_compact", "f_imm12x4 0x%x", 'x', f_imm12x4, "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw1_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw1_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw2_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw2_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw3_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw3_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw4_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    SI f_imm8x2;

    f_imm8x2 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x2) = f_imm8x2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw4_compact", "f_imm8x2 0x%x", 'x', f_imm8x2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw5_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
    UINT f_rm;
    SI f_imm4x2;

    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4x2 = ((EXTRACT_MSB0_UINT (insn, 16, 12, 4)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4x2) = f_imm4x2;
  FLD (f_rm) = f_rm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw5_compact", "f_imm4x2 0x%x", 'x', f_imm4x2, "f_rm 0x%x", 'x', f_rm, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw6_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw6_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw7_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw7_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw8_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw8_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw9_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    SI f_imm8x2;

    f_imm8x2 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x2) = f_imm8x2;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw9_compact", "f_imm8x2 0x%x", 'x', f_imm8x2, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw10_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;
    SI f_imm8x2;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_imm8x2 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x2) = f_imm8x2;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw10_compact", "f_imm8x2 0x%x", 'x', f_imm8x2, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movw11_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
    UINT f_rm;
    SI f_imm4x2;

    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);
    f_imm4x2 = ((EXTRACT_MSB0_UINT (insn, 16, 12, 4)) << (1));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm4x2) = f_imm4x2;
  FLD (f_rm) = f_rm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movw11_compact", "f_imm4x2 0x%x", 'x', f_imm4x2, "f_rm 0x%x", 'x', f_rm, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mova_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
    SI f_imm8x4;

    f_imm8x4 = ((EXTRACT_MSB0_UINT (insn, 16, 8, 8)) << (2));

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8x4) = f_imm8x4;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mova_compact", "f_imm8x4 0x%x", 'x', f_imm8x4, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movcal_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movcal_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movcol_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movcol_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movt_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movt_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movual_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movual_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movual2_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movual2_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_r0) = 0;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mull_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mull_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_negc_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_negc_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_nop_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_nop_compact", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_pref_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_pref_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_rotcl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_rotcl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_rts_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_rts_compact", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_shad_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
    UINT f_rn;
    UINT f_rm;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);
    f_rm = EXTRACT_MSB0_UINT (insn, 16, 8, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rm) = f_rm;
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_shad_compact", "f_rm 0x%x", 'x', f_rm, "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_rm;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stc_gbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stc_gbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stc_vbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stc_vbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stcl_gbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stcl_gbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stcl_vbr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stcl_vbr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sts_fpscr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sts_fpscr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stsl_fpscr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stsl_fpscr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sts_fpul_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sts_fpul_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpul) = 32;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stsl_fpul_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stsl_fpul_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpul) = 32;
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sts_mach_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sts_mach_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stsl_mach_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stsl_mach_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sts_macl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sts_macl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stsl_macl_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stsl_macl_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sts_pr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sts_pr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stsl_pr_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stsl_pr_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
      FLD (out_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_tasb_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
    UINT f_rn;

    f_rn = EXTRACT_MSB0_UINT (insn, 16, 4, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_rn) = f_rn;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_tasb_compact", "f_rn 0x%x", 'x', f_rn, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_rn;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_trapa_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_trapa_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_tsti_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_tsti_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_tstb_compact:
  {
    const IDESC *idesc = &sh64_compact_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi_compact.f
    UINT f_imm8;

    f_imm8 = EXTRACT_MSB0_UINT (insn, 16, 8, 8);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm8) = f_imm8;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_tstb_compact", "f_imm8 0x%x", 'x', f_imm8, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_r0) = 0;
    }
#endif
#undef FLD
    return idesc;
  }

}
