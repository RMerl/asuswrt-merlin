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

#define WANT_CPU m32rbf
#define WANT_CPU_M32RBF

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-ops.h"

#undef GET_ATTR
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define GET_ATTR(cpu, num, attr) CGEN_ATTR_VALUE (NULL, abuf->idesc->attrs, CGEN_INSN_##attr)
#else
#define GET_ATTR(cpu, num, attr) CGEN_ATTR_VALUE (NULL, abuf->idesc->attrs, CGEN_INSN_/**/attr)
#endif

/* This is used so that we can compile two copies of the semantic code,
   one with full feature support and one without that runs fast(er).
   FAST_P, when desired, is defined on the command line, -DFAST_P=1.  */
#if FAST_P
#define SEM_FN_NAME(cpu,fn) XCONCAT3 (cpu,_semf_,fn)
#undef TRACE_RESULT
#define TRACE_RESULT(cpu, abuf, name, type, val)
#else
#define SEM_FN_NAME(cpu,fn) XCONCAT3 (cpu,_sem_,fn)
#endif

/* x-invalid: --invalid-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_invalid) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* x-after: --after-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_after) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32RBF
    m32rbf_pbb_after (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-before: --before-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_before) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32RBF
    m32rbf_pbb_before (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-cti-chain: --cti-chain-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_cti_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* x-chain: --chain-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_M32RBF
    vpc = m32rbf_pbb_chain (current_cpu, sem_arg);
#ifdef DEFINE_SWITCH
    BREAK (sem);
#endif
#endif
  }

  return vpc;
#undef FLD
}

/* x-begin: --begin-- */

static SEM_PC
SEM_FN_NAME (m32rbf,x_begin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* add: add $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,add) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* add3: add3 $dr,$sr,$hash$slo16 */

static SEM_PC
SEM_FN_NAME (m32rbf,add3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (* FLD (i_sr), FLD (f_simm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* and: and $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,and) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* and3: and3 $dr,$sr,$uimm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,and3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* or: or $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,or) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* or3: or3 $dr,$sr,$hash$ulo16 */

static SEM_PC
SEM_FN_NAME (m32rbf,or3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xor: xor $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,xor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = XORSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xor3: xor3 $dr,$sr,$uimm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,xor3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (* FLD (i_sr), FLD (f_uimm16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addi: addi $dr,$simm8 */

static SEM_PC
SEM_FN_NAME (m32rbf,addi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (* FLD (i_dr), FLD (f_simm8));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addv: addv $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,addv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* addv3: addv3 $dr,$sr,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,addv3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* addx: addx $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,addx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* bc8: bc.s $disp8 */

static SEM_PC
SEM_FN_NAME (m32rbf,bc8) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bc24: bc.l $disp24 */

static SEM_PC
SEM_FN_NAME (m32rbf,bc24) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* beq: beq $src1,$src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,beq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* beqz: beqz $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,beqz) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgez: bgez $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,bgez) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgtz: bgtz $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,bgtz) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* blez: blez $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,blez) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bltz: bltz $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,bltz) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bnez: bnez $src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,bnez) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bl8: bl.s $disp8 */

static SEM_PC
SEM_FN_NAME (m32rbf,bl8) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bl24: bl.l $disp24 */

static SEM_PC
SEM_FN_NAME (m32rbf,bl24) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bnc8: bnc.s $disp8 */

static SEM_PC
SEM_FN_NAME (m32rbf,bnc8) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bnc24: bnc.l $disp24 */

static SEM_PC
SEM_FN_NAME (m32rbf,bnc24) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bne: bne $src1,$src2,$disp16 */

static SEM_PC
SEM_FN_NAME (m32rbf,bne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bra8: bra.s $disp8 */

static SEM_PC
SEM_FN_NAME (m32rbf,bra8) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = FLD (i_disp8);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bra24: bra.l $disp24 */

static SEM_PC
SEM_FN_NAME (m32rbf,bra24) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    USI opval = FLD (i_disp24);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* cmp: cmp $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,cmp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTSI (* FLD (i_src1), * FLD (i_src2));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpi: cmpi $src2,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,cmpi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    BI opval = LTSI (* FLD (i_src2), FLD (f_simm16));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpu: cmpu $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,cmpu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = LTUSI (* FLD (i_src1), * FLD (i_src2));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpui: cmpui $src2,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,cmpui) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    BI opval = LTUSI (* FLD (i_src2), FLD (f_simm16));
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* div: div $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,div) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = DIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* divu: divu $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,divu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UDIVSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* rem: rem $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,rem) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = MODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* remu: remu $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,remu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (* FLD (i_sr), 0)) {
  {
    SI opval = UMODSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* jl: jl $sr */

static SEM_PC
SEM_FN_NAME (m32rbf,jl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* jmp: jmp $sr */

static SEM_PC
SEM_FN_NAME (m32rbf,jmp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = ANDSI (* FLD (i_sr), -4);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* ld: ld $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,ld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ld-d: ld $dr,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,ld_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldb: ldb $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,ldb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldb-d: ldb $dr,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,ldb_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldh: ldh $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,ldh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldh-d: ldh $dr,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,ldh_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldub: ldub $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,ldub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldub-d: ldub $dr,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,ldub_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lduh: lduh $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,lduh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, * FLD (i_sr)));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lduh-d: lduh $dr,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,lduh_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ld-plus: ld $dr,@$sr+ */

static SEM_PC
SEM_FN_NAME (m32rbf,ld_plus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* ld24: ld24 $dr,$uimm24 */

static SEM_PC
SEM_FN_NAME (m32rbf,ld24) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld24.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (i_uimm24);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldi8: ldi8 $dr,$simm8 */

static SEM_PC
SEM_FN_NAME (m32rbf,ldi8) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_simm8);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldi16: ldi16 $dr,$hash$slo16 */

static SEM_PC
SEM_FN_NAME (m32rbf,ldi16) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (f_simm16);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lock: lock $dr,@$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,lock) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* machi: machi $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,machi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* maclo: maclo $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,maclo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* macwhi: macwhi $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,macwhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* macwlo: macwlo $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,macwlo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (ADDDI (GET_H_ACCUM (), MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2))))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mul: mul $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,mul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mulhi: mulhi $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,mulhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (ANDSI (* FLD (i_src1), 0xffff0000)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 16), 16);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mullo: mullo $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,mullo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (SLLSI (* FLD (i_src1), 16)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 16), 16);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mulwhi: mulwhi $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,mulwhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (SRASI (* FLD (i_src2), 16)))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mulwlo: mulwlo $src1,$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,mulwlo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = SRADI (SLLDI (MULDI (EXTSIDI (* FLD (i_src1)), EXTHIDI (TRUNCSIHI (* FLD (i_src2)))), 8), 8);
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mv: mv $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,mv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_sr);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mvfachi: mvfachi $dr */

static SEM_PC
SEM_FN_NAME (m32rbf,mvfachi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUM (), 32));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mvfaclo: mvfaclo $dr */

static SEM_PC
SEM_FN_NAME (m32rbf,mvfaclo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (GET_H_ACCUM ());
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mvfacmi: mvfacmi $dr */

static SEM_PC
SEM_FN_NAME (m32rbf,mvfacmi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = TRUNCDISI (SRADI (GET_H_ACCUM (), 16));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mvfc: mvfc $dr,$scr */

static SEM_PC
SEM_FN_NAME (m32rbf,mvfc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_CR (FLD (f_r2));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mvtachi: mvtachi $src1 */

static SEM_PC
SEM_FN_NAME (m32rbf,mvtachi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUM (), MAKEDI (0, 0xffffffff)), SLLDI (EXTSIDI (* FLD (i_src1)), 32));
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mvtaclo: mvtaclo $src1 */

static SEM_PC
SEM_FN_NAME (m32rbf,mvtaclo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (ANDDI (GET_H_ACCUM (), MAKEDI (0xffffffff, 0)), ZEXTSIDI (* FLD (i_src1)));
    SET_H_ACCUM (opval);
    TRACE_RESULT (current_cpu, abuf, "accum", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mvtc: mvtc $sr,$dcr */

static SEM_PC
SEM_FN_NAME (m32rbf,mvtc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    USI opval = * FLD (i_sr);
    SET_H_CR (FLD (f_r1), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* neg: neg $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,neg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = NEGSI (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nop: nop */

static SEM_PC
SEM_FN_NAME (m32rbf,nop) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

PROFILE_COUNT_FILLNOPS (current_cpu, abuf->addr);

  return vpc;
#undef FLD
}

/* not: not $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,not) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = INVSI (* FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* rac: rac */

static SEM_PC
SEM_FN_NAME (m32rbf,rac) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rach: rach */

static SEM_PC
SEM_FN_NAME (m32rbf,rach) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rte: rte */

static SEM_PC
SEM_FN_NAME (m32rbf,rte) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* seth: seth $dr,$hash$hi16 */

static SEM_PC
SEM_FN_NAME (m32rbf,seth) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (FLD (f_hi16), 16);
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sll: sll $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,sll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sll3: sll3 $dr,$sr,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,sll3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* slli: slli $dr,$uimm5 */

static SEM_PC
SEM_FN_NAME (m32rbf,slli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sra: sra $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,sra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sra3: sra3 $dr,$sr,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,sra3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srai: srai $dr,$uimm5 */

static SEM_PC
SEM_FN_NAME (m32rbf,srai) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRASI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srl: srl $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,srl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), ANDSI (* FLD (i_sr), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srl3: srl3 $dr,$sr,$simm16 */

static SEM_PC
SEM_FN_NAME (m32rbf,srl3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (* FLD (i_sr), ANDSI (FLD (f_simm16), 31));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srli: srli $dr,$uimm5 */

static SEM_PC
SEM_FN_NAME (m32rbf,srli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (* FLD (i_dr), FLD (f_uimm5));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* st: st $src1,@$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,st) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* st-d: st $src1,@($slo16,$src2) */

static SEM_PC
SEM_FN_NAME (m32rbf,st_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = * FLD (i_src1);
    SETMEMSI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stb: stb $src1,@$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,stb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    QI opval = * FLD (i_src1);
    SETMEMQI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stb-d: stb $src1,@($slo16,$src2) */

static SEM_PC
SEM_FN_NAME (m32rbf,stb_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = * FLD (i_src1);
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sth: sth $src1,@$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,sth) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = * FLD (i_src1);
    SETMEMHI (current_cpu, pc, * FLD (i_src2), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sth-d: sth $src1,@($slo16,$src2) */

static SEM_PC
SEM_FN_NAME (m32rbf,sth_d) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = * FLD (i_src1);
    SETMEMHI (current_cpu, pc, ADDSI (* FLD (i_src2), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* st-plus: st $src1,@+$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,st_plus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* st-minus: st $src1,@-$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,st_minus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sub: sub $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,sub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBSI (* FLD (i_dr), * FLD (i_sr));
    * FLD (i_dr) = opval;
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subv: subv $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,subv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* subx: subx $dr,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,subx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* trap: trap $uimm4 */

static SEM_PC
SEM_FN_NAME (m32rbf,trap) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_trap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* unlock: unlock $src1,@$src2 */

static SEM_PC
SEM_FN_NAME (m32rbf,unlock) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* clrpsw: clrpsw $uimm8 */

static SEM_PC
SEM_FN_NAME (m32rbf,clrpsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (GET_H_CR (((UINT) 0)), ORSI (INVBI (FLD (f_uimm8)), 65280));
    SET_H_CR (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* setpsw: setpsw $uimm8 */

static SEM_PC
SEM_FN_NAME (m32rbf,setpsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = FLD (f_uimm8);
    SET_H_CR (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* bset: bset $uimm3,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,bset) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ORQI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))), SLLSI (1, SUBSI (7, FLD (f_uimm3))));
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* bclr: bclr $uimm3,@($slo16,$sr) */

static SEM_PC
SEM_FN_NAME (m32rbf,bclr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ANDQI (GETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16))), INVQI (SLLSI (1, SUBSI (7, FLD (f_uimm3)))));
    SETMEMQI (current_cpu, pc, ADDSI (* FLD (i_sr), FLD (f_simm16)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* btst: btst $uimm3,$sr */

static SEM_PC
SEM_FN_NAME (m32rbf,btst) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ANDQI (SRLSI (* FLD (i_sr), SUBSI (7, FLD (f_uimm3))), 1);
    CPU (h_cond) = opval;
    TRACE_RESULT (current_cpu, abuf, "cond", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* Table of all semantic fns.  */

static const struct sem_fn_desc sem_fns[] = {
  { M32RBF_INSN_X_INVALID, SEM_FN_NAME (m32rbf,x_invalid) },
  { M32RBF_INSN_X_AFTER, SEM_FN_NAME (m32rbf,x_after) },
  { M32RBF_INSN_X_BEFORE, SEM_FN_NAME (m32rbf,x_before) },
  { M32RBF_INSN_X_CTI_CHAIN, SEM_FN_NAME (m32rbf,x_cti_chain) },
  { M32RBF_INSN_X_CHAIN, SEM_FN_NAME (m32rbf,x_chain) },
  { M32RBF_INSN_X_BEGIN, SEM_FN_NAME (m32rbf,x_begin) },
  { M32RBF_INSN_ADD, SEM_FN_NAME (m32rbf,add) },
  { M32RBF_INSN_ADD3, SEM_FN_NAME (m32rbf,add3) },
  { M32RBF_INSN_AND, SEM_FN_NAME (m32rbf,and) },
  { M32RBF_INSN_AND3, SEM_FN_NAME (m32rbf,and3) },
  { M32RBF_INSN_OR, SEM_FN_NAME (m32rbf,or) },
  { M32RBF_INSN_OR3, SEM_FN_NAME (m32rbf,or3) },
  { M32RBF_INSN_XOR, SEM_FN_NAME (m32rbf,xor) },
  { M32RBF_INSN_XOR3, SEM_FN_NAME (m32rbf,xor3) },
  { M32RBF_INSN_ADDI, SEM_FN_NAME (m32rbf,addi) },
  { M32RBF_INSN_ADDV, SEM_FN_NAME (m32rbf,addv) },
  { M32RBF_INSN_ADDV3, SEM_FN_NAME (m32rbf,addv3) },
  { M32RBF_INSN_ADDX, SEM_FN_NAME (m32rbf,addx) },
  { M32RBF_INSN_BC8, SEM_FN_NAME (m32rbf,bc8) },
  { M32RBF_INSN_BC24, SEM_FN_NAME (m32rbf,bc24) },
  { M32RBF_INSN_BEQ, SEM_FN_NAME (m32rbf,beq) },
  { M32RBF_INSN_BEQZ, SEM_FN_NAME (m32rbf,beqz) },
  { M32RBF_INSN_BGEZ, SEM_FN_NAME (m32rbf,bgez) },
  { M32RBF_INSN_BGTZ, SEM_FN_NAME (m32rbf,bgtz) },
  { M32RBF_INSN_BLEZ, SEM_FN_NAME (m32rbf,blez) },
  { M32RBF_INSN_BLTZ, SEM_FN_NAME (m32rbf,bltz) },
  { M32RBF_INSN_BNEZ, SEM_FN_NAME (m32rbf,bnez) },
  { M32RBF_INSN_BL8, SEM_FN_NAME (m32rbf,bl8) },
  { M32RBF_INSN_BL24, SEM_FN_NAME (m32rbf,bl24) },
  { M32RBF_INSN_BNC8, SEM_FN_NAME (m32rbf,bnc8) },
  { M32RBF_INSN_BNC24, SEM_FN_NAME (m32rbf,bnc24) },
  { M32RBF_INSN_BNE, SEM_FN_NAME (m32rbf,bne) },
  { M32RBF_INSN_BRA8, SEM_FN_NAME (m32rbf,bra8) },
  { M32RBF_INSN_BRA24, SEM_FN_NAME (m32rbf,bra24) },
  { M32RBF_INSN_CMP, SEM_FN_NAME (m32rbf,cmp) },
  { M32RBF_INSN_CMPI, SEM_FN_NAME (m32rbf,cmpi) },
  { M32RBF_INSN_CMPU, SEM_FN_NAME (m32rbf,cmpu) },
  { M32RBF_INSN_CMPUI, SEM_FN_NAME (m32rbf,cmpui) },
  { M32RBF_INSN_DIV, SEM_FN_NAME (m32rbf,div) },
  { M32RBF_INSN_DIVU, SEM_FN_NAME (m32rbf,divu) },
  { M32RBF_INSN_REM, SEM_FN_NAME (m32rbf,rem) },
  { M32RBF_INSN_REMU, SEM_FN_NAME (m32rbf,remu) },
  { M32RBF_INSN_JL, SEM_FN_NAME (m32rbf,jl) },
  { M32RBF_INSN_JMP, SEM_FN_NAME (m32rbf,jmp) },
  { M32RBF_INSN_LD, SEM_FN_NAME (m32rbf,ld) },
  { M32RBF_INSN_LD_D, SEM_FN_NAME (m32rbf,ld_d) },
  { M32RBF_INSN_LDB, SEM_FN_NAME (m32rbf,ldb) },
  { M32RBF_INSN_LDB_D, SEM_FN_NAME (m32rbf,ldb_d) },
  { M32RBF_INSN_LDH, SEM_FN_NAME (m32rbf,ldh) },
  { M32RBF_INSN_LDH_D, SEM_FN_NAME (m32rbf,ldh_d) },
  { M32RBF_INSN_LDUB, SEM_FN_NAME (m32rbf,ldub) },
  { M32RBF_INSN_LDUB_D, SEM_FN_NAME (m32rbf,ldub_d) },
  { M32RBF_INSN_LDUH, SEM_FN_NAME (m32rbf,lduh) },
  { M32RBF_INSN_LDUH_D, SEM_FN_NAME (m32rbf,lduh_d) },
  { M32RBF_INSN_LD_PLUS, SEM_FN_NAME (m32rbf,ld_plus) },
  { M32RBF_INSN_LD24, SEM_FN_NAME (m32rbf,ld24) },
  { M32RBF_INSN_LDI8, SEM_FN_NAME (m32rbf,ldi8) },
  { M32RBF_INSN_LDI16, SEM_FN_NAME (m32rbf,ldi16) },
  { M32RBF_INSN_LOCK, SEM_FN_NAME (m32rbf,lock) },
  { M32RBF_INSN_MACHI, SEM_FN_NAME (m32rbf,machi) },
  { M32RBF_INSN_MACLO, SEM_FN_NAME (m32rbf,maclo) },
  { M32RBF_INSN_MACWHI, SEM_FN_NAME (m32rbf,macwhi) },
  { M32RBF_INSN_MACWLO, SEM_FN_NAME (m32rbf,macwlo) },
  { M32RBF_INSN_MUL, SEM_FN_NAME (m32rbf,mul) },
  { M32RBF_INSN_MULHI, SEM_FN_NAME (m32rbf,mulhi) },
  { M32RBF_INSN_MULLO, SEM_FN_NAME (m32rbf,mullo) },
  { M32RBF_INSN_MULWHI, SEM_FN_NAME (m32rbf,mulwhi) },
  { M32RBF_INSN_MULWLO, SEM_FN_NAME (m32rbf,mulwlo) },
  { M32RBF_INSN_MV, SEM_FN_NAME (m32rbf,mv) },
  { M32RBF_INSN_MVFACHI, SEM_FN_NAME (m32rbf,mvfachi) },
  { M32RBF_INSN_MVFACLO, SEM_FN_NAME (m32rbf,mvfaclo) },
  { M32RBF_INSN_MVFACMI, SEM_FN_NAME (m32rbf,mvfacmi) },
  { M32RBF_INSN_MVFC, SEM_FN_NAME (m32rbf,mvfc) },
  { M32RBF_INSN_MVTACHI, SEM_FN_NAME (m32rbf,mvtachi) },
  { M32RBF_INSN_MVTACLO, SEM_FN_NAME (m32rbf,mvtaclo) },
  { M32RBF_INSN_MVTC, SEM_FN_NAME (m32rbf,mvtc) },
  { M32RBF_INSN_NEG, SEM_FN_NAME (m32rbf,neg) },
  { M32RBF_INSN_NOP, SEM_FN_NAME (m32rbf,nop) },
  { M32RBF_INSN_NOT, SEM_FN_NAME (m32rbf,not) },
  { M32RBF_INSN_RAC, SEM_FN_NAME (m32rbf,rac) },
  { M32RBF_INSN_RACH, SEM_FN_NAME (m32rbf,rach) },
  { M32RBF_INSN_RTE, SEM_FN_NAME (m32rbf,rte) },
  { M32RBF_INSN_SETH, SEM_FN_NAME (m32rbf,seth) },
  { M32RBF_INSN_SLL, SEM_FN_NAME (m32rbf,sll) },
  { M32RBF_INSN_SLL3, SEM_FN_NAME (m32rbf,sll3) },
  { M32RBF_INSN_SLLI, SEM_FN_NAME (m32rbf,slli) },
  { M32RBF_INSN_SRA, SEM_FN_NAME (m32rbf,sra) },
  { M32RBF_INSN_SRA3, SEM_FN_NAME (m32rbf,sra3) },
  { M32RBF_INSN_SRAI, SEM_FN_NAME (m32rbf,srai) },
  { M32RBF_INSN_SRL, SEM_FN_NAME (m32rbf,srl) },
  { M32RBF_INSN_SRL3, SEM_FN_NAME (m32rbf,srl3) },
  { M32RBF_INSN_SRLI, SEM_FN_NAME (m32rbf,srli) },
  { M32RBF_INSN_ST, SEM_FN_NAME (m32rbf,st) },
  { M32RBF_INSN_ST_D, SEM_FN_NAME (m32rbf,st_d) },
  { M32RBF_INSN_STB, SEM_FN_NAME (m32rbf,stb) },
  { M32RBF_INSN_STB_D, SEM_FN_NAME (m32rbf,stb_d) },
  { M32RBF_INSN_STH, SEM_FN_NAME (m32rbf,sth) },
  { M32RBF_INSN_STH_D, SEM_FN_NAME (m32rbf,sth_d) },
  { M32RBF_INSN_ST_PLUS, SEM_FN_NAME (m32rbf,st_plus) },
  { M32RBF_INSN_ST_MINUS, SEM_FN_NAME (m32rbf,st_minus) },
  { M32RBF_INSN_SUB, SEM_FN_NAME (m32rbf,sub) },
  { M32RBF_INSN_SUBV, SEM_FN_NAME (m32rbf,subv) },
  { M32RBF_INSN_SUBX, SEM_FN_NAME (m32rbf,subx) },
  { M32RBF_INSN_TRAP, SEM_FN_NAME (m32rbf,trap) },
  { M32RBF_INSN_UNLOCK, SEM_FN_NAME (m32rbf,unlock) },
  { M32RBF_INSN_CLRPSW, SEM_FN_NAME (m32rbf,clrpsw) },
  { M32RBF_INSN_SETPSW, SEM_FN_NAME (m32rbf,setpsw) },
  { M32RBF_INSN_BSET, SEM_FN_NAME (m32rbf,bset) },
  { M32RBF_INSN_BCLR, SEM_FN_NAME (m32rbf,bclr) },
  { M32RBF_INSN_BTST, SEM_FN_NAME (m32rbf,btst) },
  { 0, 0 }
};

/* Add the semantic fns to IDESC_TABLE.  */

void
SEM_FN_NAME (m32rbf,init_idesc_table) (SIM_CPU *current_cpu)
{
  IDESC *idesc_table = CPU_IDESC (current_cpu);
  const struct sem_fn_desc *sf;
  int mach_num = MACH_NUM (CPU_MACH (current_cpu));

  for (sf = &sem_fns[0]; sf->fn != 0; ++sf)
    {
      const CGEN_INSN *insn = idesc_table[sf->index].idata;
      int valid_p = (CGEN_INSN_VIRTUAL_P (insn)
		     || CGEN_INSN_MACH_HAS_P (insn, mach_num));
#if FAST_P
      if (valid_p)
	idesc_table[sf->index].sem_fast = sf->fn;
      else
	idesc_table[sf->index].sem_fast = SEM_FN_NAME (m32rbf,x_invalid);
#else
      if (valid_p)
	idesc_table[sf->index].sem_full = sf->fn;
      else
	idesc_table[sf->index].sem_full = SEM_FN_NAME (m32rbf,x_invalid);
#endif
    }
}

