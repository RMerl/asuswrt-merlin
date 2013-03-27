/* Simulator instruction semantics for iq2000bf.

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

#ifdef DEFINE_LABELS

  /* The labels have the case they have because the enum of insn types
     is all uppercase and in the non-stdc case the insn symbol is built
     into the enum name.  */

  static struct {
    int index;
    void *label;
  } labels[] = {
    { IQ2000BF_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { IQ2000BF_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { IQ2000BF_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { IQ2000BF_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { IQ2000BF_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { IQ2000BF_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { IQ2000BF_INSN_ADD, && case_sem_INSN_ADD },
    { IQ2000BF_INSN_ADDI, && case_sem_INSN_ADDI },
    { IQ2000BF_INSN_ADDIU, && case_sem_INSN_ADDIU },
    { IQ2000BF_INSN_ADDU, && case_sem_INSN_ADDU },
    { IQ2000BF_INSN_ADO16, && case_sem_INSN_ADO16 },
    { IQ2000BF_INSN_AND, && case_sem_INSN_AND },
    { IQ2000BF_INSN_ANDI, && case_sem_INSN_ANDI },
    { IQ2000BF_INSN_ANDOI, && case_sem_INSN_ANDOI },
    { IQ2000BF_INSN_NOR, && case_sem_INSN_NOR },
    { IQ2000BF_INSN_OR, && case_sem_INSN_OR },
    { IQ2000BF_INSN_ORI, && case_sem_INSN_ORI },
    { IQ2000BF_INSN_RAM, && case_sem_INSN_RAM },
    { IQ2000BF_INSN_SLL, && case_sem_INSN_SLL },
    { IQ2000BF_INSN_SLLV, && case_sem_INSN_SLLV },
    { IQ2000BF_INSN_SLMV, && case_sem_INSN_SLMV },
    { IQ2000BF_INSN_SLT, && case_sem_INSN_SLT },
    { IQ2000BF_INSN_SLTI, && case_sem_INSN_SLTI },
    { IQ2000BF_INSN_SLTIU, && case_sem_INSN_SLTIU },
    { IQ2000BF_INSN_SLTU, && case_sem_INSN_SLTU },
    { IQ2000BF_INSN_SRA, && case_sem_INSN_SRA },
    { IQ2000BF_INSN_SRAV, && case_sem_INSN_SRAV },
    { IQ2000BF_INSN_SRL, && case_sem_INSN_SRL },
    { IQ2000BF_INSN_SRLV, && case_sem_INSN_SRLV },
    { IQ2000BF_INSN_SRMV, && case_sem_INSN_SRMV },
    { IQ2000BF_INSN_SUB, && case_sem_INSN_SUB },
    { IQ2000BF_INSN_SUBU, && case_sem_INSN_SUBU },
    { IQ2000BF_INSN_XOR, && case_sem_INSN_XOR },
    { IQ2000BF_INSN_XORI, && case_sem_INSN_XORI },
    { IQ2000BF_INSN_BBI, && case_sem_INSN_BBI },
    { IQ2000BF_INSN_BBIN, && case_sem_INSN_BBIN },
    { IQ2000BF_INSN_BBV, && case_sem_INSN_BBV },
    { IQ2000BF_INSN_BBVN, && case_sem_INSN_BBVN },
    { IQ2000BF_INSN_BEQ, && case_sem_INSN_BEQ },
    { IQ2000BF_INSN_BEQL, && case_sem_INSN_BEQL },
    { IQ2000BF_INSN_BGEZ, && case_sem_INSN_BGEZ },
    { IQ2000BF_INSN_BGEZAL, && case_sem_INSN_BGEZAL },
    { IQ2000BF_INSN_BGEZALL, && case_sem_INSN_BGEZALL },
    { IQ2000BF_INSN_BGEZL, && case_sem_INSN_BGEZL },
    { IQ2000BF_INSN_BLTZ, && case_sem_INSN_BLTZ },
    { IQ2000BF_INSN_BLTZL, && case_sem_INSN_BLTZL },
    { IQ2000BF_INSN_BLTZAL, && case_sem_INSN_BLTZAL },
    { IQ2000BF_INSN_BLTZALL, && case_sem_INSN_BLTZALL },
    { IQ2000BF_INSN_BMB0, && case_sem_INSN_BMB0 },
    { IQ2000BF_INSN_BMB1, && case_sem_INSN_BMB1 },
    { IQ2000BF_INSN_BMB2, && case_sem_INSN_BMB2 },
    { IQ2000BF_INSN_BMB3, && case_sem_INSN_BMB3 },
    { IQ2000BF_INSN_BNE, && case_sem_INSN_BNE },
    { IQ2000BF_INSN_BNEL, && case_sem_INSN_BNEL },
    { IQ2000BF_INSN_JALR, && case_sem_INSN_JALR },
    { IQ2000BF_INSN_JR, && case_sem_INSN_JR },
    { IQ2000BF_INSN_LB, && case_sem_INSN_LB },
    { IQ2000BF_INSN_LBU, && case_sem_INSN_LBU },
    { IQ2000BF_INSN_LH, && case_sem_INSN_LH },
    { IQ2000BF_INSN_LHU, && case_sem_INSN_LHU },
    { IQ2000BF_INSN_LUI, && case_sem_INSN_LUI },
    { IQ2000BF_INSN_LW, && case_sem_INSN_LW },
    { IQ2000BF_INSN_SB, && case_sem_INSN_SB },
    { IQ2000BF_INSN_SH, && case_sem_INSN_SH },
    { IQ2000BF_INSN_SW, && case_sem_INSN_SW },
    { IQ2000BF_INSN_BREAK, && case_sem_INSN_BREAK },
    { IQ2000BF_INSN_SYSCALL, && case_sem_INSN_SYSCALL },
    { IQ2000BF_INSN_ANDOUI, && case_sem_INSN_ANDOUI },
    { IQ2000BF_INSN_ORUI, && case_sem_INSN_ORUI },
    { IQ2000BF_INSN_BGTZ, && case_sem_INSN_BGTZ },
    { IQ2000BF_INSN_BGTZL, && case_sem_INSN_BGTZL },
    { IQ2000BF_INSN_BLEZ, && case_sem_INSN_BLEZ },
    { IQ2000BF_INSN_BLEZL, && case_sem_INSN_BLEZL },
    { IQ2000BF_INSN_MRGB, && case_sem_INSN_MRGB },
    { IQ2000BF_INSN_BCTXT, && case_sem_INSN_BCTXT },
    { IQ2000BF_INSN_BC0F, && case_sem_INSN_BC0F },
    { IQ2000BF_INSN_BC0FL, && case_sem_INSN_BC0FL },
    { IQ2000BF_INSN_BC3F, && case_sem_INSN_BC3F },
    { IQ2000BF_INSN_BC3FL, && case_sem_INSN_BC3FL },
    { IQ2000BF_INSN_BC0T, && case_sem_INSN_BC0T },
    { IQ2000BF_INSN_BC0TL, && case_sem_INSN_BC0TL },
    { IQ2000BF_INSN_BC3T, && case_sem_INSN_BC3T },
    { IQ2000BF_INSN_BC3TL, && case_sem_INSN_BC3TL },
    { IQ2000BF_INSN_CFC0, && case_sem_INSN_CFC0 },
    { IQ2000BF_INSN_CFC1, && case_sem_INSN_CFC1 },
    { IQ2000BF_INSN_CFC2, && case_sem_INSN_CFC2 },
    { IQ2000BF_INSN_CFC3, && case_sem_INSN_CFC3 },
    { IQ2000BF_INSN_CHKHDR, && case_sem_INSN_CHKHDR },
    { IQ2000BF_INSN_CTC0, && case_sem_INSN_CTC0 },
    { IQ2000BF_INSN_CTC1, && case_sem_INSN_CTC1 },
    { IQ2000BF_INSN_CTC2, && case_sem_INSN_CTC2 },
    { IQ2000BF_INSN_CTC3, && case_sem_INSN_CTC3 },
    { IQ2000BF_INSN_JCR, && case_sem_INSN_JCR },
    { IQ2000BF_INSN_LUC32, && case_sem_INSN_LUC32 },
    { IQ2000BF_INSN_LUC32L, && case_sem_INSN_LUC32L },
    { IQ2000BF_INSN_LUC64, && case_sem_INSN_LUC64 },
    { IQ2000BF_INSN_LUC64L, && case_sem_INSN_LUC64L },
    { IQ2000BF_INSN_LUK, && case_sem_INSN_LUK },
    { IQ2000BF_INSN_LULCK, && case_sem_INSN_LULCK },
    { IQ2000BF_INSN_LUM32, && case_sem_INSN_LUM32 },
    { IQ2000BF_INSN_LUM32L, && case_sem_INSN_LUM32L },
    { IQ2000BF_INSN_LUM64, && case_sem_INSN_LUM64 },
    { IQ2000BF_INSN_LUM64L, && case_sem_INSN_LUM64L },
    { IQ2000BF_INSN_LUR, && case_sem_INSN_LUR },
    { IQ2000BF_INSN_LURL, && case_sem_INSN_LURL },
    { IQ2000BF_INSN_LUULCK, && case_sem_INSN_LUULCK },
    { IQ2000BF_INSN_MFC0, && case_sem_INSN_MFC0 },
    { IQ2000BF_INSN_MFC1, && case_sem_INSN_MFC1 },
    { IQ2000BF_INSN_MFC2, && case_sem_INSN_MFC2 },
    { IQ2000BF_INSN_MFC3, && case_sem_INSN_MFC3 },
    { IQ2000BF_INSN_MTC0, && case_sem_INSN_MTC0 },
    { IQ2000BF_INSN_MTC1, && case_sem_INSN_MTC1 },
    { IQ2000BF_INSN_MTC2, && case_sem_INSN_MTC2 },
    { IQ2000BF_INSN_MTC3, && case_sem_INSN_MTC3 },
    { IQ2000BF_INSN_PKRL, && case_sem_INSN_PKRL },
    { IQ2000BF_INSN_PKRLR1, && case_sem_INSN_PKRLR1 },
    { IQ2000BF_INSN_PKRLR30, && case_sem_INSN_PKRLR30 },
    { IQ2000BF_INSN_RB, && case_sem_INSN_RB },
    { IQ2000BF_INSN_RBR1, && case_sem_INSN_RBR1 },
    { IQ2000BF_INSN_RBR30, && case_sem_INSN_RBR30 },
    { IQ2000BF_INSN_RFE, && case_sem_INSN_RFE },
    { IQ2000BF_INSN_RX, && case_sem_INSN_RX },
    { IQ2000BF_INSN_RXR1, && case_sem_INSN_RXR1 },
    { IQ2000BF_INSN_RXR30, && case_sem_INSN_RXR30 },
    { IQ2000BF_INSN_SLEEP, && case_sem_INSN_SLEEP },
    { IQ2000BF_INSN_SRRD, && case_sem_INSN_SRRD },
    { IQ2000BF_INSN_SRRDL, && case_sem_INSN_SRRDL },
    { IQ2000BF_INSN_SRULCK, && case_sem_INSN_SRULCK },
    { IQ2000BF_INSN_SRWR, && case_sem_INSN_SRWR },
    { IQ2000BF_INSN_SRWRU, && case_sem_INSN_SRWRU },
    { IQ2000BF_INSN_TRAPQFL, && case_sem_INSN_TRAPQFL },
    { IQ2000BF_INSN_TRAPQNE, && case_sem_INSN_TRAPQNE },
    { IQ2000BF_INSN_TRAPREL, && case_sem_INSN_TRAPREL },
    { IQ2000BF_INSN_WB, && case_sem_INSN_WB },
    { IQ2000BF_INSN_WBU, && case_sem_INSN_WBU },
    { IQ2000BF_INSN_WBR1, && case_sem_INSN_WBR1 },
    { IQ2000BF_INSN_WBR1U, && case_sem_INSN_WBR1U },
    { IQ2000BF_INSN_WBR30, && case_sem_INSN_WBR30 },
    { IQ2000BF_INSN_WBR30U, && case_sem_INSN_WBR30U },
    { IQ2000BF_INSN_WX, && case_sem_INSN_WX },
    { IQ2000BF_INSN_WXU, && case_sem_INSN_WXU },
    { IQ2000BF_INSN_WXR1, && case_sem_INSN_WXR1 },
    { IQ2000BF_INSN_WXR1U, && case_sem_INSN_WXR1U },
    { IQ2000BF_INSN_WXR30, && case_sem_INSN_WXR30 },
    { IQ2000BF_INSN_WXR30U, && case_sem_INSN_WXR30U },
    { IQ2000BF_INSN_LDW, && case_sem_INSN_LDW },
    { IQ2000BF_INSN_SDW, && case_sem_INSN_SDW },
    { IQ2000BF_INSN_J, && case_sem_INSN_J },
    { IQ2000BF_INSN_JAL, && case_sem_INSN_JAL },
    { IQ2000BF_INSN_BMB, && case_sem_INSN_BMB },
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
#if WITH_SCACHE_PBB_IQ2000BF
    iq2000bf_pbb_after (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_IQ2000BF
    iq2000bf_pbb_before (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_IQ2000BF
#ifdef DEFINE_SWITCH
    vpc = iq2000bf_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = iq2000bf_pbb_cti_chain (current_cpu, sem_arg,
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
#if WITH_SCACHE_PBB_IQ2000BF
    vpc = iq2000bf_pbb_chain (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_IQ2000BF
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = iq2000bf_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = iq2000bf_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = iq2000bf_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD) : /* add $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI) : /* addi $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDIU) : /* addiu $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDU) : /* addu $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADO16) : /* ado16 $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_high;
  HI tmp_low;
  tmp_low = ADDHI (ANDHI (GET_H_GR (FLD (f_rs)), 65535), ANDHI (GET_H_GR (FLD (f_rt)), 65535));
  tmp_high = ADDHI (SRLSI (GET_H_GR (FLD (f_rs)), 16), SRLSI (GET_H_GR (FLD (f_rt)), 16));
  {
    SI opval = ORSI (SLLSI (tmp_high, 16), tmp_low);
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND) : /* and $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDI) : /* andi $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDOI) : /* andoi $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ORSI (0xffff0000, EXTHISI (TRUNCSIHI (FLD (f_imm)))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NOR) : /* nor $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = INVSI (ORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR) : /* or $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORI) : /* ori $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RAM) : /* ram $rd,$rt,$shamt,$maskl,$maskr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = RORSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rd)), SRLSI (0xffffffff, FLD (f_maskl)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rd)), SLLSI (0xffffffff, FLD (f_rs)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLL) : /* sll $rd,$rt,$shamt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLLV) : /* sllv $rd,$rt,$rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLMV) : /* slmv $rd,$rt,$rs,$shamt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (SLLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt)), SRLSI (0xffffffff, GET_H_GR (FLD (f_rs))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLT) : /* slt $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
  {
    SI opval = 1;
    SET_H_GR (FLD (f_rd), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GR (FLD (f_rd), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLTI) : /* slti $rt,$rs,$imm */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))) {
  {
    SI opval = 1;
    SET_H_GR (FLD (f_rt), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GR (FLD (f_rt), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLTIU) : /* sltiu $rt,$rs,$imm */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTUSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))) {
  {
    SI opval = 1;
    SET_H_GR (FLD (f_rt), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GR (FLD (f_rt), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLTU) : /* sltu $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTUSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
  {
    SI opval = 1;
    SET_H_GR (FLD (f_rd), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
} else {
  {
    SI opval = 0;
    SET_H_GR (FLD (f_rd), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRA) : /* sra $rd,$rt,$shamt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRAV) : /* srav $rd,$rt,$rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRL) : /* srl $rd,$rt,$shamt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRLV) : /* srlv $rd,$rt,$rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRMV) : /* srmv $rd,$rt,$rs,$shamt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ram.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (SRLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt)), SLLSI (0xffffffff, GET_H_GR (FLD (f_rs))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB) : /* sub $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBU) : /* subu $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR) : /* xor $rd,$rs,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XORI) : /* xori $rt,$rs,$lo16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BBI) : /* bbi $rs($bitnum),$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (GET_H_GR (FLD (f_rs)), SLLSI (1, FLD (f_rt)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BBIN) : /* bbin $rs($bitnum),$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTSI (ANDSI (GET_H_GR (FLD (f_rs)), SLLSI (1, FLD (f_rt))))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BBV) : /* bbv $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (GET_H_GR (FLD (f_rs)), SLLSI (1, ANDSI (GET_H_GR (FLD (f_rt)), 31)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BBVN) : /* bbvn $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTSI (ANDSI (GET_H_GR (FLD (f_rs)), SLLSI (1, ANDSI (GET_H_GR (FLD (f_rt)), 31))))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQ) : /* beq $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQL) : /* beql $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGEZ) : /* bgez $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGEZAL) : /* bgezal $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (((UINT) 31), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
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

  CASE (sem, INSN_BGEZALL) : /* bgezall $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (((UINT) 31), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGEZL) : /* bgezl $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLTZ) : /* bltz $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLTZL) : /* bltzl $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLTZAL) : /* bltzal $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (((UINT) 31), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
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

  CASE (sem, INSN_BLTZALL) : /* bltzall $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (((UINT) 31), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BMB0) : /* bmb0 $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 255), ANDSI (GET_H_GR (FLD (f_rt)), 255))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BMB1) : /* bmb1 $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 65280), ANDSI (GET_H_GR (FLD (f_rt)), 65280))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BMB2) : /* bmb2 $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 16711680), ANDSI (GET_H_GR (FLD (f_rt)), 16711680))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BMB3) : /* bmb3 $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 0xff000000), ANDSI (GET_H_GR (FLD (f_rt)), 0xff000000))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNE) : /* bne $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNEL) : /* bnel $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)))) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JALR) : /* jalr $rd,$rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_GR (FLD (f_rs));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JR) : /* jr $rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    USI opval = GET_H_GR (FLD (f_rs));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LB) : /* lb $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LBU) : /* lbu $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LH) : /* lh $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LHU) : /* lhu $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUI) : /* lui $rt,$hi16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (FLD (f_imm), 16);
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LW) : /* lw $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SB) : /* sb $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ANDQI (GET_H_GR (FLD (f_rt)), 255);
    SETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SH) : /* sh $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = ANDHI (GET_H_GR (FLD (f_rt)), 65535);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SW) : /* sw $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_GR (FLD (f_rt));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BREAK) : /* break */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

do_break (current_cpu, pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SYSCALL) : /* syscall */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

do_syscall (current_cpu);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDOUI) : /* andoui $rt,$rs,$hi16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ORSI (SLLSI (FLD (f_imm), 16), 65535));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORUI) : /* orui $rt,$rs,$hi16 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), SLLSI (FLD (f_imm), 16));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGTZ) : /* bgtz $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGTZL) : /* bgtzl $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (GTSI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLEZ) : /* blez $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLEZL) : /* blezl $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (LESI (GET_H_GR (FLD (f_rs)), 0)) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (1)
  SEM_SKIP_INSN (current_cpu, sem_arg, vpc);
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MRGB) : /* mrgb $rd,$rs,$rt,$mask */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_mrgb.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_temp;
if (NOTSI (ANDSI (FLD (f_mask), SLLSI (1, 0)))) {
  tmp_temp = ANDSI (GET_H_GR (FLD (f_rs)), 255);
} else {
  tmp_temp = ANDSI (GET_H_GR (FLD (f_rt)), 255);
}
if (NOTSI (ANDSI (FLD (f_mask), SLLSI (1, 1)))) {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rs)), 65280));
} else {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rt)), 65280));
}
if (NOTSI (ANDSI (FLD (f_mask), SLLSI (1, 2)))) {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rs)), 16711680));
} else {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rt)), 16711680));
}
if (NOTSI (ANDSI (FLD (f_mask), SLLSI (1, 3)))) {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rs)), 0xff000000));
} else {
  tmp_temp = ORSI (tmp_temp, ANDSI (GET_H_GR (FLD (f_rt)), 0xff000000));
}
  {
    SI opval = tmp_temp;
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BCTXT) : /* bctxt $rs,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC0F) : /* bc0f $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC0FL) : /* bc0fl $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC3F) : /* bc3f $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC3FL) : /* bc3fl $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC0T) : /* bc0t $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC0TL) : /* bc0tl $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC3T) : /* bc3t $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BC3TL) : /* bc3tl $offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CFC0) : /* cfc0 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CFC1) : /* cfc1 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CFC2) : /* cfc2 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CFC3) : /* cfc3 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CHKHDR) : /* chkhdr $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CTC0) : /* ctc0 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CTC1) : /* ctc1 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CTC2) : /* ctc2 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CTC3) : /* ctc3 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JCR) : /* jcr $rs */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUC32) : /* luc32 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUC32L) : /* luc32l $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUC64) : /* luc64 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUC64L) : /* luc64l $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUK) : /* luk $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LULCK) : /* lulck $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUM32) : /* lum32 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUM32L) : /* lum32l $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUM64) : /* lum64 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUM64L) : /* lum64l $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUR) : /* lur $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LURL) : /* lurl $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LUULCK) : /* luulck $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MFC0) : /* mfc0 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MFC1) : /* mfc1 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MFC2) : /* mfc2 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MFC3) : /* mfc3 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MTC0) : /* mtc0 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MTC1) : /* mtc1 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MTC2) : /* mtc2 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MTC3) : /* mtc3 $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PKRL) : /* pkrl $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PKRLR1) : /* pkrlr1 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PKRLR30) : /* pkrlr30 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RB) : /* rb $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RBR1) : /* rbr1 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RBR30) : /* rbr30 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RFE) : /* rfe */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RX) : /* rx $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RXR1) : /* rxr1 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RXR30) : /* rxr30 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SLEEP) : /* sleep */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRRD) : /* srrd $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRRDL) : /* srrdl $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRULCK) : /* srulck $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRWR) : /* srwr $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SRWRU) : /* srwru $rt,$rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TRAPQFL) : /* trapqfl */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TRAPQNE) : /* trapqne */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_TRAPREL) : /* traprel $rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WB) : /* wb $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WBU) : /* wbu $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WBR1) : /* wbr1 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WBR1U) : /* wbr1u $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WBR30) : /* wbr30 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WBR30U) : /* wbr30u $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WX) : /* wx $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WXU) : /* wxu $rd,$rt */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WXR1) : /* wxr1 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WXR1U) : /* wxr1u $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WXR30) : /* wxr30 $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_WXR30U) : /* wxr30u $rt,$index,$count */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDW) : /* ldw $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_addr;
  tmp_addr = ANDSI (ADDSI (GET_H_GR (FLD (f_rs)), FLD (f_imm)), INVSI (3));
  {
    SI opval = GETMEMSI (current_cpu, pc, tmp_addr);
    SET_H_GR (ADDSI (FLD (f_rt), 1), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (tmp_addr, 4));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SDW) : /* sdw $rt,$lo16($base) */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_addr;
  tmp_addr = ANDSI (ADDSI (GET_H_GR (FLD (f_rs)), FLD (f_imm)), INVSI (3));
  {
    SI opval = GET_H_GR (FLD (f_rt));
    SETMEMSI (current_cpu, pc, ADDSI (tmp_addr, 4), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  {
    SI opval = GET_H_GR (ADDSI (FLD (f_rt), 1));
    SETMEMSI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_J) : /* j $jmptarg */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_j.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    USI opval = FLD (i_jmptarg);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_JAL) : /* jal $jmptarg */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_j.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
{
  {
    SI opval = ADDSI (pc, 8);
    SET_H_GR (((UINT) 31), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = FLD (i_jmptarg);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  SEM_BRANCH_FINI (vpc);
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BMB) : /* bmb $rs,$rt,$offset */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_bbi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_branch_;
  tmp_branch_ = 0;
if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 255), ANDSI (GET_H_GR (FLD (f_rt)), 255))) {
  tmp_branch_ = 1;
}
if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 65280), ANDSI (GET_H_GR (FLD (f_rt)), 65280))) {
  tmp_branch_ = 1;
}
if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 16711680), ANDSI (GET_H_GR (FLD (f_rt)), 16711680))) {
  tmp_branch_ = 1;
}
if (EQSI (ANDSI (GET_H_GR (FLD (f_rs)), 0xff000000), ANDSI (GET_H_GR (FLD (f_rt)), 0xff000000))) {
  tmp_branch_ = 1;
}
if (tmp_branch_) {
{
  {
    USI opval = FLD (i_offset);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
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


    }
  ENDSWITCH (sem) /* End of semantic switch.  */
     ;

  /* At this point `vpc' contains the next insn to execute.  */
}

#undef DEFINE_SWITCH
#endif /* DEFINE_SWITCH */
