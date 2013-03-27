/* Simulator instruction semantics for m32rbf.

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
    { M32RBF_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { M32RBF_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { M32RBF_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { M32RBF_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { M32RBF_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { M32RBF_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { M32RBF_INSN_ADD, && case_sem_INSN_ADD },
    { M32RBF_INSN_ADD3, && case_sem_INSN_ADD3 },
    { M32RBF_INSN_AND, && case_sem_INSN_AND },
    { M32RBF_INSN_AND3, && case_sem_INSN_AND3 },
    { M32RBF_INSN_OR, && case_sem_INSN_OR },
    { M32RBF_INSN_OR3, && case_sem_INSN_OR3 },
    { M32RBF_INSN_XOR, && case_sem_INSN_XOR },
    { M32RBF_INSN_XOR3, && case_sem_INSN_XOR3 },
    { M32RBF_INSN_ADDI, && case_sem_INSN_ADDI },
    { M32RBF_INSN_ADDV, && case_sem_INSN_ADDV },
    { M32RBF_INSN_ADDV3, && case_sem_INSN_ADDV3 },
    { M32RBF_INSN_ADDX, && case_sem_INSN_ADDX },
    { M32RBF_INSN_BC8, && case_sem_INSN_BC8 },
    { M32RBF_INSN_BC24, && case_sem_INSN_BC24 },
    { M32RBF_INSN_BEQ, && case_sem_INSN_BEQ },
    { M32RBF_INSN_BEQZ, && case_sem_INSN_BEQZ },
    { M32RBF_INSN_BGEZ, && case_sem_INSN_BGEZ },
    { M32RBF_INSN_BGTZ, && case_sem_INSN_BGTZ },
    { M32RBF_INSN_BLEZ, && case_sem_INSN_BLEZ },
    { M32RBF_INSN_BLTZ, && case_sem_INSN_BLTZ },
    { M32RBF_INSN_BNEZ, && case_sem_INSN_BNEZ },
    { M32RBF_INSN_BL8, && case_sem_INSN_BL8 },
    { M32RBF_INSN_BL24, && case_sem_INSN_BL24 },
    { M32RBF_INSN_BNC8, && case_sem_INSN_BNC8 },
    { M32RBF_INSN_BNC24, && case_sem_INSN_BNC24 },
    { M32RBF_INSN_BNE, && case_sem_INSN_BNE },
    { M32RBF_INSN_BRA8, && case_sem_INSN_BRA8 },
    { M32RBF_INSN_BRA24, && case_sem_INSN_BRA24 },
    { M32RBF_INSN_CMP, && case_sem_INSN_CMP },
    { M32RBF_INSN_CMPI, && case_sem_INSN_CMPI },
    { M32RBF_INSN_CMPU, && case_sem_INSN_CMPU },
    { M32RBF_INSN_CMPUI, && case_sem_INSN_CMPUI },
    { M32RBF_INSN_DIV, && case_sem_INSN_DIV },
    { M32RBF_INSN_DIVU, && case_sem_INSN_DIVU },
    { M32RBF_INSN_REM, && case_sem_INSN_REM },
    { M32RBF_INSN_REMU, && case_sem_INSN_REMU },
    { M32RBF_INSN_JL, && case_sem_INSN_JL },
    { M32RBF_INSN_JMP, && case_sem_INSN_JMP },
    { M32RBF_INSN_LD, && case_sem_INSN_LD },
    { M32RBF_INSN_LD_D, && case_sem_INSN_LD_D },
    { M32RBF_INSN_LDB, && case_sem_INSN_LDB },
    { M32RBF_INSN_LDB_D, && case_sem_INSN_LDB_D },
    { M32RBF_INSN_LDH, && case_sem_INSN_LDH },
    { M32RBF_INSN_LDH_D, && case_sem_INSN_LDH_D },
    { M32RBF_INSN_LDUB, && case_sem_INSN_LDUB },
    { M32RBF_INSN_LDUB_D, && case_sem_INSN_LDUB_D },
    { M32RBF_INSN_LDUH, && case_sem_INSN_LDUH },
    { M32RBF_INSN_LDUH_D, && case_sem_INSN_LDUH_D },
    { M32RBF_INSN_LD_PLUS, && case_sem_INSN_LD_PLUS },
    { M32RBF_INSN_LD24, && case_sem_INSN_LD24 },
    { M32RBF_INSN_LDI8, && case_sem_INSN_LDI8 },
    { M32RBF_INSN_LDI16, && case_sem_INSN_LDI16 },
    { M32RBF_INSN_LOCK, && case_sem_INSN_LOCK },
    { M32RBF_INSN_MACHI, && case_sem_INSN_MACHI },
    { M32RBF_INSN_MACLO, && case_sem_INSN_MACLO },
    { M32RBF_INSN_MACWHI, && case_sem_INSN_MACWHI },
    { M32RBF_INSN_MACWLO, && case_sem_INSN_MACWLO },
    { M32RBF_INSN_MUL, && case_sem_INSN_MUL },
    { M32RBF_INSN_MULHI, && case_sem_INSN_MULHI },
    { M32RBF_INSN_MULLO, && case_sem_INSN_MULLO },
    { M32RBF_INSN_MULWHI, && case_sem_INSN_MULWHI },
    { M32RBF_INSN_MULWLO, && case_sem_INSN_MULWLO },
    { M32RBF_INSN_MV, && case_sem_INSN_MV },
    { M32RBF_INSN_MVFACHI, && case_sem_INSN_MVFACHI },
    { M32RBF_INSN_MVFACLO, && case_sem_INSN_MVFACLO },
    { M32RBF_INSN_MVFACMI, && case_sem_INSN_MVFACMI },
    { M32RBF_INSN_MVFC, && case_sem_INSN_MVFC },
    { M32RBF_INSN_MVTACHI, && case_sem_INSN_MVTACHI },
    { M32RBF_INSN_MVTACLO, && case_sem_INSN_MVTACLO },
    { M32RBF_INSN_MVTC, && case_sem_INSN_MVTC },
    { M32RBF_INSN_NEG, && case_sem_INSN_NEG },
    { M32RBF_INSN_NOP, && case_sem_INSN_NOP },
    { M32RBF_INSN_NOT, && case_sem_INSN_NOT },
    { M32RBF_INSN_RAC, && case_sem_INSN_RAC },
    { M32RBF_INSN_RACH, && case_sem_INSN_RACH },
    { M32RBF_INSN_RTE, && case_sem_INSN_RTE },
    { M32RBF_INSN_SETH, && case_sem_INSN_SETH },
    { M32RBF_INSN_SLL, && case_sem_INSN_SLL },
    { M32RBF_INSN_SLL3, && case_sem_INSN_SLL3 },
    { M32RBF_INSN_SLLI, && case_sem_INSN_SLLI },
    { M32RBF_INSN_SRA, && case_sem_INSN_SRA },
    { M32RBF_INSN_SRA3, && case_sem_INSN_SRA3 },
    { M32RBF_INSN_SRAI, && case_sem_INSN_SRAI },
    { M32RBF_INSN_SRL, && case_sem_INSN_SRL },
    { M32RBF_INSN_SRL3, && case_sem_INSN_SRL3 },
    { M32RBF_INSN_SRLI, && case_sem_INSN_SRLI },
    { M32RBF_INSN_ST, && case_sem_INSN_ST },
    { M32RBF_INSN_ST_D, && case_sem_INSN_ST_D },
    { M32RBF_INSN_STB, && case_sem_INSN_STB },
    { M32RBF_INSN_STB_D, && case_sem_INSN_STB_D },
    { M32RBF_INSN_STH, && case_sem_INSN_STH },
    { M32RBF_INSN_STH_D, && case_sem_INSN_STH_D },
    { M32RBF_INSN_ST_PLUS, && case_sem_INSN_ST_PLUS },
    { M32RBF_INSN_ST_MINUS, && case_sem_INSN_ST_MINUS },
    { M32RBF_INSN_SUB, && case_sem_INSN_SUB },
    { M32RBF_INSN_SUBV, && case_sem_INSN_SUBV },
    { M32RBF_INSN_SUBX, && case_sem_INSN_SUBX },
    { M32RBF_INSN_TRAP, && case_sem_INSN_TRAP },
    { M32RBF_INSN_UNLOCK, && case_sem_INSN_UNLOCK },
    { M32RBF_INSN_CLRPSW, && case_sem_INSN_CLRPSW },
    { M32RBF_INSN_SETPSW, && case_sem_INSN_SETPSW },
    { M32RBF_INSN_BSET, && case_sem_INSN_BSET },
    { M32RBF_INSN_BCLR, && case_sem_INSN_BCLR },
    { M32RBF_INSN_BTST, && case_sem_INSN_BTST },
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
#if WITH_SCACHE_PBB_M32RBF
    m32rbf_pbb_after (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_M32RBF
    m32rbf_pbb_before (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_M32RBF
#ifdef DEFINE_SWITCH
    vpc = m32rbf_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = m32rbf_pbb_cti_chain (current_cpu, sem_arg,
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
#if WITH_SCACHE_PBB_M32RBF
    vpc = m32rbf_pbb_chain (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_M32RBF
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = m32rbf_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = m32rbf_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = m32rbf_pbb_begin (current_cpu, 0);
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

  CASE (sem, INSN_MACHI) : /* machi $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACLO) : /* maclo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACWHI) : /* macwhi $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MACWLO) : /* macwlo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
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

  CASE (sem, INSN_MULHI) : /* mulhi $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 16), 16);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULLO) : /* mullo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 16), 16);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULWHI) : /* mulwhi $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULWLO) : /* mulwlo $src1,$src2 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
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

  CASE (sem, INSN_MVFACHI) : /* mvfachi $dr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_seth.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUM (), 32));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFACLO) : /* mvfaclo $dr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_seth.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (GET_H_ACCUM ());
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVFACMI) : /* mvfacmi $dr */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_seth.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUM (), 16));
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

  CASE (sem, INSN_MVTACHI) : /* mvtachi $src1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUM (), MAKEDI (0, 0xffffffff)), SLLDI (EXTSIDI (* FLD (i_src1)), 32));
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MVTACLO) : /* mvtaclo $src1 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_st_plus.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUM (), MAKEDI (0xffffffff, 0)), ZEXTSIDI (* FLD (i_src1)));
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
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

  CASE (sem, INSN_RAC) : /* rac */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = SLLDI (GET_H_ACCUM (), 1);
  tmp_tmp1 = ADDDI (tmp_tmp1, MAKEDI (0, 32768));
  {
    DI opval = (GTDI (tmp_tmp1, MAKEDI (32767, 0xffff0000))) ? (MAKEDI (32767, 0xffff0000)) : (LTDI (tmp_tmp1, MAKEDI (0xffff8000, 0))) ? (MAKEDI (0xffff8000, 0)) : (ANDDI (tmp_tmp1, MAKEDI (0xffffffff, 0xffff0000)));
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_RACH) : /* rach */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_tmp1;
  tmp_tmp1 = ANDDI (GET_H_ACCUM (), MAKEDI (16777215, 0xffffffff));
if (ANDIF (GEDI (tmp_tmp1, MAKEDI (16383, 0x80000000)), LEDI (tmp_tmp1, MAKEDI (8388607, 0xffffffff)))) {
  tmp_tmp1 = MAKEDI (16383, 0x80000000);
} else {
if (ANDIF (GEDI (tmp_tmp1, MAKEDI (8388608, 0)), LEDI (tmp_tmp1, MAKEDI (16760832, 0)))) {
  tmp_tmp1 = MAKEDI (16760832, 0);
} else {
  tmp_tmp1 = ANDDI (ADDDI (GET_H_ACCUM (), MAKEDI (0, 1073741824)), MAKEDI (0xffffffff, 0x80000000));
}
}
  tmp_tmp1 = SLLDI (tmp_tmp1, 1);
  {
    DI opval = SRADI (SLLDI (tmp_tmp1, 7), 7);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
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


    }
  ENDSWITCH (sem) /* End of semantic switch.  */

  /* At this point `vpc' contains the next insn to execute.  */
}

#undef DEFINE_SWITCH
#endif /* DEFINE_SWITCH */
