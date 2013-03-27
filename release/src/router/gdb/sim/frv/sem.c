/* Simulator instruction semantics for frvbf.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

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
SEM_FN_NAME (frvbf,x_invalid) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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
SEM_FN_NAME (frvbf,x_after) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_FRVBF
    frvbf_pbb_after (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-before: --before-- */

static SEM_PC
SEM_FN_NAME (frvbf,x_before) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_FRVBF
    frvbf_pbb_before (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-cti-chain: --cti-chain-- */

static SEM_PC
SEM_FN_NAME (frvbf,x_cti_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_FRVBF
#ifdef DEFINE_SWITCH
    vpc = frvbf_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = frvbf_pbb_cti_chain (current_cpu, sem_arg,
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
SEM_FN_NAME (frvbf,x_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_FRVBF
    vpc = frvbf_pbb_chain (current_cpu, sem_arg);
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
SEM_FN_NAME (frvbf,x_begin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_FRVBF
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = frvbf_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = frvbf_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = frvbf_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

  return vpc;
#undef FLD
}

/* add: add$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,add) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sub: sub$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,sub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* and: and$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,and) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* or: or$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,or) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xor: xor$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,xor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* not: not$pack $GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,not) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_scutss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = INVSI (GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sdiv: sdiv$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,sdiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_signed_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 0);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* nsdiv: nsdiv$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nsdiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_signed_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 1);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* udiv: udiv$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,udiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_unsigned_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 0);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* nudiv: nudiv$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nudiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_unsigned_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 1);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* smul: smul$pack $GRi,$GRj,$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,smul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* umul: umul$pack $GRi,$GRj,$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,umul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (ZEXTSIDI (GET_H_GR (FLD (f_GRi))), ZEXTSIDI (GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* smu: smu$pack $GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,smu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smass.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_iacc0_set, ((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "iacc0", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* smass: smass$pack $GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,smass) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smass.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = (ANDIF (ANDIF (GTDI (MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj)))), 0), GTDI (GET_H_IACC0 (((UINT) 0)), 0)), LTDI (SUBDI (9223372036854775807, MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))), GET_H_IACC0 (((UINT) 0))))) ? (MAKEDI (2147483647, 0xffffffff)) : (ANDIF (ANDIF (LTDI (MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj)))), 0), LTDI (GET_H_IACC0 (((UINT) 0)), 0)), GTDI (SUBDI (9223372036854775808, MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))), GET_H_IACC0 (((UINT) 0))))) ? (MAKEDI (0x80000000, 0)) : (ADDDI (GET_H_IACC0 (((UINT) 0)), MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_iacc0_set, ((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "iacc0", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* smsss: smsss$pack $GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,smsss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smass.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = (ANDIF (ANDIF (LTDI (MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj)))), 0), GTDI (GET_H_IACC0 (((UINT) 0)), 0)), LTDI (ADDDI (9223372036854775807, MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))), GET_H_IACC0 (((UINT) 0))))) ? (MAKEDI (2147483647, 0xffffffff)) : (ANDIF (ANDIF (GTDI (MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj)))), 0), LTDI (GET_H_IACC0 (((UINT) 0)), 0)), GTDI (ADDDI (9223372036854775808, MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))), GET_H_IACC0 (((UINT) 0))))) ? (MAKEDI (0x80000000, 0)) : (SUBDI (GET_H_IACC0 (((UINT) 0)), MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_iacc0_set, ((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "iacc0", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* sll: sll$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,sll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srl: srl$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,srl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sra: sra$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,sra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* slass: slass$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,slass) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_shift_left_arith_saturate (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* scutss: scutss$pack $GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,scutss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_scutss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_iacc_cut (current_cpu, GET_H_IACC0 (((UINT) 0)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* scan: scan$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,scan) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp1;
  SI tmp_tmp2;
  tmp_tmp1 = GET_H_GR (FLD (f_GRi));
  tmp_tmp2 = SRASI (GET_H_GR (FLD (f_GRj)), 1);
  {
    SI opval = frvbf_scan_result (current_cpu, XORSI (tmp_tmp1, tmp_tmp2));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cadd: cadd$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cadd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csub: csub$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = SUBSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cand: cand$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cand) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = ANDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cor: cor$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = ORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cxor: cxor$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cxor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = XORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cnot: cnot$pack $GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cnot) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = INVSI (GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csmul: csmul$pack $GRi,$GRj,$GRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csmul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    DI opval = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csdiv: csdiv$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csdiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
frvbf_signed_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 0);
; /*clobber*/
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cudiv: cudiv$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cudiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
frvbf_unsigned_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_GRk), 0);
; /*clobber*/
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csll: csll$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = SLLSI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csrl: csrl$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csrl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = SRLSI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csra: csra$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = SRASI (GET_H_GR (FLD (f_GRi)), ANDSI (GET_H_GR (FLD (f_GRj)), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cscan: cscan$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cscan) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_tmp1;
  SI tmp_tmp2;
  tmp_tmp1 = GET_H_GR (FLD (f_GRi));
  tmp_tmp2 = SRASI (GET_H_GR (FLD (f_GRj)), 1);
  {
    SI opval = frvbf_scan_result (current_cpu, XORSI (tmp_tmp1, tmp_tmp2));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* addcc: addcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = ADDOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = ADDCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* subcc: subcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = SUBOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = SUBCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = SUBSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* andcc: andcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,andcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = ANDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* orcc: orcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,orcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = ORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* xorcc: xorcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,xorcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = XORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* sllcc: sllcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,sllcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_left (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SLLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* srlcc: srlcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,srlcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SRLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* sracc: sracc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,sracc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SRASI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* smulcc: smulcc$pack $GRi,$GRj,$GRdoublek,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,smulcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))));
if (EQDI (SRLDI (tmp_tmp, 63), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQDI (tmp_tmp, 0), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* umulcc: umulcc$pack $GRi,$GRj,$GRdoublek,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,umulcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = MULDI (ZEXTSIDI (GET_H_GR (FLD (f_GRi))), ZEXTSIDI (GET_H_GR (FLD (f_GRj))));
if (EQDI (SRLDI (tmp_tmp, 63), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQDI (tmp_tmp, 0), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* caddcc: caddcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,caddcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[((FLD (f_CCi)) & (3))]);
  tmp_tmp = ADDOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = ADDCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csubcc: csubcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csubcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[((FLD (f_CCi)) & (3))]);
  tmp_tmp = SUBOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = SUBCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = SUBSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csmulcc: csmulcc$pack $GRi,$GRj,$GRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csmulcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_csmulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  DI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[((FLD (f_CCi)) & (3))]);
  tmp_tmp = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (GET_H_GR (FLD (f_GRj))));
if (EQDI (SRLDI (tmp_tmp, 63), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQDI (tmp_tmp, 0), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* candcc: candcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,candcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_tmp;
  tmp_tmp = ANDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* corcc: corcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,corcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_tmp;
  tmp_tmp = ORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cxorcc: cxorcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cxorcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_tmp;
  tmp_tmp = XORSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[((FLD (f_CCi)) & (3))]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csllcc: csllcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csllcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_left (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[((FLD (f_CCi)) & (3))]));
  tmp_tmp = SLLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csrlcc: csrlcc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csrlcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[((FLD (f_CCi)) & (3))]));
  tmp_tmp = SRLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csracc: csracc$pack $GRi,$GRj,$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csracc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_caddcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (GET_H_GR (FLD (f_GRj)), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[((FLD (f_CCi)) & (3))]));
  tmp_tmp = SRASI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[((FLD (f_CCi)) & (3))]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* addx: addx$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDCSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subx: subx$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBCSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addxcc: addxcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addxcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = ADDCSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1)));
if (EQSI (ADDOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
if (EQSI (ADDCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* subxcc: subxcc$pack $GRi,$GRj,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subxcc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = SUBCSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1)));
if (EQSI (SUBOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
if (EQSI (SUBCFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* addss: addss$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,addss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (ADDOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0)) {
  {
    SI opval = (GTSI (GET_H_GR (FLD (f_GRi)), 0)) ? (2147483647) : (LTSI (GET_H_GR (FLD (f_GRi)), 0)) ? (0x80000000) : (0);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* subss: subss$pack $GRi,$GRj,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,subss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = SUBSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (SUBOFSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), 0)) {
  {
    SI opval = (GTSI (GET_H_GR (FLD (f_GRi)), 0)) ? (2147483647) : (LTSI (GET_H_GR (FLD (f_GRi)), 0)) ? (0x80000000) : (0);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* addi: addi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,addi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subi: subi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,subi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* andi: andi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,andi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ori: ori$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xori: xori$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,xori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sdivi: sdivi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,sdivi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_signed_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), FLD (f_d12), FLD (f_GRk), 0);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* nsdivi: nsdivi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nsdivi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_signed_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), FLD (f_d12), FLD (f_GRk), 1);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* udivi: udivi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,udivi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_unsigned_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), FLD (f_d12), FLD (f_GRk), 0);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* nudivi: nudivi$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nudivi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_unsigned_integer_divide (current_cpu, GET_H_GR (FLD (f_GRi)), FLD (f_d12), FLD (f_GRk), 1);
; /*clobber*/
}

  return vpc;
#undef FLD
}

/* smuli: smuli$pack $GRi,$s12,$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,smuli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smuli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (FLD (f_d12)));
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* umuli: umuli$pack $GRi,$s12,$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,umuli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smuli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (ZEXTSIDI (GET_H_GR (FLD (f_GRi))), ZEXTSIDI (FLD (f_d12)));
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* slli: slli$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,slli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_GRi)), ANDSI (FLD (f_d12), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srli: srli$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,srli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_GRi)), ANDSI (FLD (f_d12), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srai: srai$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,srai) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_GRi)), ANDSI (FLD (f_d12), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* scani: scani$pack $GRi,$s12,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,scani) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp1;
  SI tmp_tmp2;
  tmp_tmp1 = GET_H_GR (FLD (f_GRi));
  tmp_tmp2 = SRASI (FLD (f_d12), 1);
  {
    SI opval = frvbf_scan_result (current_cpu, XORSI (tmp_tmp1, tmp_tmp2));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* addicc: addicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = ADDOFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = ADDCFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* subicc: subicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_tmp;
  QI tmp_cc;
  SI tmp_result;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = SUBOFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
  tmp_tmp = SUBCFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), 0);
if (EQBI (tmp_tmp, 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  tmp_result = SUBSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10));
if (EQSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_result, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_result;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* andicc: andicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,andicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = ANDSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* oricc: oricc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,oricc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = ORSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* xoricc: xoricc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,xoricc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  tmp_tmp = XORSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10));
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 7), 4);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
if (LTSI (tmp_tmp, 0)) {
  {
    UQI opval = ORQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 11), 8);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
} else {
  {
    UQI opval = ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 3);
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* smulicc: smulicc$pack $GRi,$s10,$GRdoublek,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,smulicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = MULDI (EXTSIDI (GET_H_GR (FLD (f_GRi))), EXTSIDI (FLD (f_s10)));
if (EQDI (SRLDI (tmp_tmp, 63), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQDI (tmp_tmp, 0), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* umulicc: umulicc$pack $GRi,$s10,$GRdoublek,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,umulicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = MULDI (ZEXTSIDI (GET_H_GR (FLD (f_GRi))), ZEXTSIDI (FLD (f_s10)));
if (EQDI (SRLDI (tmp_tmp, 63), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQDI (tmp_tmp, 0), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* sllicc: sllicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,sllicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (FLD (f_s10), 31);
  tmp_cc = frvbf_set_icc_for_shift_left (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SLLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* srlicc: srlicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,srlicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (FLD (f_s10), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SRLSI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* sraicc: sraicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,sraicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_shift;
  SI tmp_tmp;
  QI tmp_cc;
  tmp_shift = ANDSI (FLD (f_s10), 31);
  tmp_cc = frvbf_set_icc_for_shift_right (current_cpu, GET_H_GR (FLD (f_GRi)), tmp_shift, CPU (h_iccr[FLD (f_ICCi_1)]));
  tmp_tmp = SRASI (GET_H_GR (FLD (f_GRi)), tmp_shift);
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* addxi: addxi$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addxi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDCSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subxi: subxi$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subxi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBCSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_1)]), 1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addxicc: addxicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,addxicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = ADDCSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1)));
if (EQSI (ADDOFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
if (EQSI (ADDCFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* subxicc: subxicc$pack $GRi,$s10,$GRk,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,subxicc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addicc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  QI tmp_cc;
  tmp_cc = CPU (h_iccr[FLD (f_ICCi_1)]);
  tmp_tmp = SUBCSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1)));
if (EQSI (SUBOFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
if (EQSI (SUBCFSI (GET_H_GR (FLD (f_GRi)), FLD (f_s10), TRUNCQIBI (ANDQI (tmp_cc, 1))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
if (EQSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 7), 4);
} else {
if (LTSI (tmp_tmp, 0)) {
  tmp_cc = ORQI (ANDQI (tmp_cc, 11), 8);
} else {
  tmp_cc = ANDQI (tmp_cc, 3);
}
}
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cmpb: cmpb$pack $GRi,$GRj,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,cmpb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_cc;
if (EQBI (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 0xff000000), ANDSI (GET_H_GR (FLD (f_GRj)), 0xff000000)), 0)) {
  tmp_cc = ANDQI (tmp_cc, 7);
} else {
  tmp_cc = ORQI (tmp_cc, 8);
}
if (EQBI (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 16711680), ANDSI (GET_H_GR (FLD (f_GRj)), 16711680)), 0)) {
  tmp_cc = ANDQI (tmp_cc, 11);
} else {
  tmp_cc = ORQI (tmp_cc, 4);
}
if (EQBI (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 65280), ANDSI (GET_H_GR (FLD (f_GRj)), 65280)), 0)) {
  tmp_cc = ANDQI (tmp_cc, 13);
} else {
  tmp_cc = ORQI (tmp_cc, 2);
}
if (EQBI (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 255), ANDSI (GET_H_GR (FLD (f_GRj)), 255)), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cmpba: cmpba$pack $GRi,$GRj,$ICCi_1 */

static SEM_PC
SEM_FN_NAME (frvbf,cmpba) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_cc;
  tmp_cc = 0;
if (EQBI (ORIF (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 0xff000000), ANDSI (GET_H_GR (FLD (f_GRj)), 0xff000000)), ORIF (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 16711680), ANDSI (GET_H_GR (FLD (f_GRj)), 16711680)), ORIF (EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 65280), ANDSI (GET_H_GR (FLD (f_GRj)), 65280)), EQSI (ANDSI (GET_H_GR (FLD (f_GRi)), 255), ANDSI (GET_H_GR (FLD (f_GRj)), 255))))), 0)) {
  tmp_cc = ANDQI (tmp_cc, 14);
} else {
  tmp_cc = ORQI (tmp_cc, 1);
}
  {
    UQI opval = tmp_cc;
    sim_queue_qi_write (current_cpu, & CPU (h_iccr[FLD (f_ICCi_1)]), opval);
    TRACE_RESULT (current_cpu, abuf, "iccr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* setlo: setlo$pack $ulo16,$GRklo */

static SEM_PC
SEM_FN_NAME (frvbf,setlo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setlo.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UHI opval = FLD (f_u16);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_gr_lo_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_lo", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sethi: sethi$pack $uhi16,$GRkhi */

static SEM_PC
SEM_FN_NAME (frvbf,sethi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_sethi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UHI opval = FLD (f_u16);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_gr_hi_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr_hi", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* setlos: setlos$pack $slo16,$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,setlos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setlos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (f_s16);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsb: ldsb$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldsb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldub: ldub$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsh: ldsh$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldsh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lduh: lduh$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,lduh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ld: ld$pack $ldann($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldbf: ldbf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldbf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldhf: ldhf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldhf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldf: ldf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldc: ldc$pack @($GRi,$GRj),$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_si_write (current_cpu, & CPU (h_cpr[FLD (f_CPRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cpr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nldsb: nldsb$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldsb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 1, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldub: nldub$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 0, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldsh: nldsh$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldsh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 3, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlduh: nlduh$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nlduh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 2, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nld: nld$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 4, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldbf: nldbf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldbf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 0, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldhf: nldhf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldhf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 2, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldf: nldf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 4, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldd: ldd$pack $lddann($GRi,$GRj),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,ldd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lddf: lddf$pack @($GRi,$GRj),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* lddc: lddc$pack @($GRi,$GRj),$CPRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lddcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_cpr_double_set, FLD (f_CPRk), opval);
    TRACE_RESULT (current_cpu, abuf, "cpr_double", 'D', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* nldd: nldd$pack @($GRi,$GRj),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nldd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 5, 0);
if (tmp_do_op) {
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlddf: nlddf$pack @($GRi,$GRj),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nlddf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 5, 1);
if (tmp_do_op) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldq: ldq$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}

  return vpc;
#undef FLD
}

/* ldqf: ldqf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}

  return vpc;
#undef FLD
}

/* ldqc: ldqc$pack @($GRi,$GRj),$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_CPR (current_cpu, pc, tmp_address, FLD (f_CPRk));
}
}

  return vpc;
#undef FLD
}

/* nldq: nldq$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 6, 0);
if (tmp_do_op) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}
}
}

  return vpc;
#undef FLD
}

/* nldqf: nldqf$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldqf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 6, 1);
if (tmp_do_op) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}
}
}

  return vpc;
#undef FLD
}

/* ldsbu: ldsbu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldsbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldubu: ldubu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldubu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldshu: ldshu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldshu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lduhu: lduhu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,lduhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldu: ldu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldsbu: nldsbu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldsbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 1, 0);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldubu: nldubu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldubu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 0, 0);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldshu: nldshu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldshu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 3, 0);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlduhu: nlduhu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nlduhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 2, 0);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldu: nldu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 4, 0);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldbfu: ldbfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldbfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* ldhfu: ldhfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldhfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* ldfu: ldfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* ldcu: ldcu$pack @($GRi,$GRj),$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_si_write (current_cpu, & CPU (h_cpr[FLD (f_CPRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cpr", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* nldbfu: nldbfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldbfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 0, 1);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldhfu: nldhfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldhfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 2, 1);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldfu: nldfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 4, 1);
if (tmp_do_op) {
{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lddu: lddu$pack @($GRi,$GRj),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlddu: nlddu$pack @($GRi,$GRj),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nlddu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 5, 0);
if (tmp_do_op) {
{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lddfu: lddfu$pack @($GRi,$GRj),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* lddcu: lddcu$pack @($GRi,$GRj),$CPRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lddcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_cpr_double_set, FLD (f_CPRk), opval);
    TRACE_RESULT (current_cpu, abuf, "cpr_double", 'D', opval);
  }
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* nlddfu: nlddfu$pack @($GRi,$GRj),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nlddfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 5, 1);
if (tmp_do_op) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldqu: ldqu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldqu: nldqu$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldqu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_GRk), 0, 6, 0);
if (tmp_do_op) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
{
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldqfu: ldqfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* ldqcu: ldqcu$pack @($GRi,$GRj),$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_CPR (current_cpu, pc, tmp_address, FLD (f_CPRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}

  return vpc;
#undef FLD
}

/* nldqfu: nldqfu$pack @($GRi,$GRj),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldqfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), FLD (f_GRj), FLD (f_FRk), 0, 6, 1);
if (tmp_do_op) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_force_update (current_cpu);
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldsbi: ldsbi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldsbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldshi: ldshi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldshi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldi: ldi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldubi: ldubi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldubi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lduhi: lduhi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,lduhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldbfi: ldbfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldbfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldhfi: ldhfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldhfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldfi: ldfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nldsbi: nldsbi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldsbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 1, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldubi: nldubi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldubi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 0, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldshi: nldshi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldshi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 3, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlduhi: nlduhi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nlduhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 2, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldi: nldi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,nldi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 4, 0);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldbfi: nldbfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldbfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_FRk), FLD (f_d12), 0, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldhfi: nldhfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldhfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_FRk), FLD (f_d12), 2, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nldfi: nldfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ldbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_FRk), FLD (f_d12), 4, 1);
if (tmp_do_op) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lddi: lddi$pack @($GRi,$d12),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smuli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* lddfi: lddfi$pack @($GRi,$d12),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,lddfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lddfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* nlddi: nlddi$pack @($GRi,$d12),$GRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nlddi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smuli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_GRk), FLD (f_d12), 5, 0);
if (tmp_do_op) {
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nlddfi: nlddfi$pack @($GRi,$d12),$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,nlddfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lddfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_FRk), FLD (f_d12), 5, 1);
if (tmp_do_op) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldqi: ldqi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}

  return vpc;
#undef FLD
}

/* ldqfi: ldqfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,ldqfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}

  return vpc;
#undef FLD
}

/* nldqfi: nldqfi$pack @($GRi,$d12),$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nldqfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  BI tmp_do_op;
  tmp_do_op = frvbf_check_non_excepting_load (current_cpu, FLD (f_GRi), -1, FLD (f_FRk), FLD (f_d12), 6, 1);
if (tmp_do_op) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_load_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}
}
}

  return vpc;
#undef FLD
}

/* stb: stb$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* sth: sth$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,sth) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* st: st$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,st) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* stbf: stbf$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stbf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* sthf: sthf$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,sthf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* stf: stf$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* stc: stc$pack $CPRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), CPU (h_cpr[FLD (f_CPRk)]));

  return vpc;
#undef FLD
}

/* std: std$pack $GRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,std) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_GR_DOUBLE (FLD (f_GRk)));
}
}

  return vpc;
#undef FLD
}

/* stdf: stdf$pack $FRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stdf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DF (current_cpu, pc, tmp_address, GET_H_FR_DOUBLE (FLD (f_FRk)));
}
}

  return vpc;
#undef FLD
}

/* stdc: stdc$pack $CPRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stdc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_CPR_DOUBLE (FLD (f_CPRk)));
}
}

  return vpc;
#undef FLD
}

/* stq: stq$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_smulcc.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}

  return vpc;
#undef FLD
}

/* stqf: stqf$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stqf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}

  return vpc;
#undef FLD
}

/* stqc: stqc$pack $CPRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stqc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_CPR (current_cpu, pc, tmp_address, FLD (f_CPRk));
}
}

  return vpc;
#undef FLD
}

/* stbu: stbu$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_QI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* sthu: sthu$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,sthu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_HI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stu: stu$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_WI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stbfu: stbfu$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stbfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_QI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* sthfu: sthfu$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,sthfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_HI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stfu: stfu$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_SI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stcu: stcu$pack $CPRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  USI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_SI (current_cpu, pc, tmp_address, CPU (h_cpr[FLD (f_CPRk)]));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stdu: stdu$pack $GRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stdu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_GR_DOUBLE (FLD (f_GRk)));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stdfu: stdfu$pack $FRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stdfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DF (current_cpu, pc, tmp_address, GET_H_FR_DOUBLE (FLD (f_FRk)));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stdcu: stdcu$pack $CPRdoublek,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stdcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_CPR_DOUBLE (FLD (f_CPRk)));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stqu: stqu$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stqu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stqfu: stqfu$pack $FRintk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stqfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* stqcu: stqcu$pack $CPRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,stqcu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdcu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_CPR (current_cpu, pc, tmp_address, FLD (f_CPRk));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cldsb: cldsb$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldsb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldub: cldub$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldsh: cldsh$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldsh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* clduh: clduh$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,clduh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cld: cld$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldbf: cldbf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldbf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldhf: cldhf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldhf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldf: cldf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldd: cldd$pack @($GRi,$GRj),$GRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* clddf: clddf$pack @($GRi,$GRj),$FRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,clddf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldq: cldq$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}
}

  return vpc;
#undef FLD
}

/* cldsbu: cldsbu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldsbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_QI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldubu: cldubu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldubu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldshu: cldshu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldshu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_HI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* clduhu: clduhu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,clduhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldu: cldu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldsbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldbfu: cldbfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldbfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UQI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldhfu: cldhfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldhfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_UHI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldfu: cldfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cldbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    SI opval = frvbf_read_mem_SI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* clddu: clddu$pack @($GRi,$GRj),$GRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,clddu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
if (NESI (FLD (f_GRk), 0)) {
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DI opval = frvbf_read_mem_DI (current_cpu, pc, tmp_address);
    sim_queue_fn_di_write (current_cpu, frvbf_h_gr_double_set, FLD (f_GRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr_double", 'D', opval);
  }
}
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* clddfu: clddfu$pack @($GRi,$GRj),$FRdoublek,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,clddfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clddfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
  {
    DF opval = frvbf_read_mem_DF (current_cpu, pc, tmp_address);
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cldqu: cldqu$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cldqu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_load_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
if (NESI (FLD (f_GRi), FLD (f_GRk))) {
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstb: cstb$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));
}

  return vpc;
#undef FLD
}

/* csth: csth$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csth) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));
}

  return vpc;
#undef FLD
}

/* cst: cst$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cst) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_GR (FLD (f_GRk)));
}

  return vpc;
#undef FLD
}

/* cstbf: cstbf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstbf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));
}

  return vpc;
#undef FLD
}

/* csthf: csthf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csthf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));
}

  return vpc;
#undef FLD
}

/* cstf: cstf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), GET_H_FR_INT (FLD (f_FRk)));
}

  return vpc;
#undef FLD
}

/* cstd: cstd$pack $GRdoublek,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_GR_DOUBLE (FLD (f_GRk)));
}
}
}

  return vpc;
#undef FLD
}

/* cstdf: cstdf$pack $FRdoublek,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstdf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DF (current_cpu, pc, tmp_address, GET_H_FR_DOUBLE (FLD (f_FRk)));
}
}
}

  return vpc;
#undef FLD
}

/* cstq: cstq$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_store_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}
}

  return vpc;
#undef FLD
}

/* cstbu: cstbu$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_QI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csthu: csthu$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csthu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_HI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstu: cstu$pack $GRk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_SI (current_cpu, pc, tmp_address, GET_H_GR (FLD (f_GRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstbfu: cstbfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstbfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_QI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* csthfu: csthfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,csthfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_HI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstfu: cstfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstbfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_SI (current_cpu, pc, tmp_address, GET_H_FR_INT (FLD (f_FRk)));
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstdu: cstdu$pack $GRdoublek,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstdu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_GR_DOUBLE (FLD (f_GRk)));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cstdfu: cstdfu$pack $FRdoublek,@($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cstdfu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cstdfu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_write_mem_DF (current_cpu, pc, tmp_address, GET_H_FR_DOUBLE (FLD (f_FRk)));
}
  {
    SI opval = tmp_address;
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* stbi: stbi$pack $GRk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* sthi: sthi$pack $GRk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,sthi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* sti: sti$pack $GRk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,sti) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_GR (FLD (f_GRk)));

  return vpc;
#undef FLD
}

/* stbfi: stbfi$pack $FRintk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stbfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_QI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* sthfi: sthfi$pack $FRintk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,sthfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_HI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* stfi: stfi$pack $FRintk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stbfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_write_mem_SI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), GET_H_FR_INT (FLD (f_FRk)));

  return vpc;
#undef FLD
}

/* stdi: stdi$pack $GRdoublek,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stdi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_write_mem_DI (current_cpu, pc, tmp_address, GET_H_GR_DOUBLE (FLD (f_GRk)));
}
}

  return vpc;
#undef FLD
}

/* stdfi: stdfi$pack $FRdoublek,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stdfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_write_mem_DF (current_cpu, pc, tmp_address, GET_H_FR_DOUBLE (FLD (f_FRk)));
}
}

  return vpc;
#undef FLD
}

/* stqi: stqi$pack $GRk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stqi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_store_quad_GR (current_cpu, pc, tmp_address, FLD (f_GRk));
}
}

  return vpc;
#undef FLD
}

/* stqfi: stqfi$pack $FRintk,@($GRi,$d12) */

static SEM_PC
SEM_FN_NAME (frvbf,stqfi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_stdfi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
{
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_store_quad_FRint (current_cpu, pc, tmp_address, FLD (f_FRk));
}
}

  return vpc;
#undef FLD
}

/* swap: swap$pack @($GRi,$GRj),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,swap) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  SI tmp_address;
  tmp_tmp = GET_H_GR (FLD (f_GRk));
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_check_swap_address (current_cpu, tmp_address);
  {
    SI opval = frvbf_read_mem_WI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_write_mem_WI (current_cpu, pc, tmp_address, tmp_tmp);
}

  return vpc;
#undef FLD
}

/* swapi: swapi$pack @($GRi,$d12),$GRk */

static SEM_PC
SEM_FN_NAME (frvbf,swapi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  SI tmp_address;
  tmp_tmp = GET_H_GR (FLD (f_GRk));
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12));
frvbf_check_swap_address (current_cpu, tmp_address);
  {
    SI opval = frvbf_read_mem_WI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_write_mem_WI (current_cpu, pc, tmp_address, tmp_tmp);
}

  return vpc;
#undef FLD
}

/* cswap: cswap$pack @($GRi,$GRj),$GRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cswap) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cswap.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  SI tmp_tmp;
  SI tmp_address;
  tmp_tmp = GET_H_GR (FLD (f_GRk));
  tmp_address = ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
frvbf_check_swap_address (current_cpu, tmp_address);
  {
    SI opval = frvbf_read_mem_WI (current_cpu, pc, tmp_address);
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
frvbf_write_mem_WI (current_cpu, pc, tmp_address, tmp_tmp);
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* movgf: movgf$pack $GRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,movgf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovgfd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movfg: movfg$pack $FRintk,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,movfg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovfgd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_FR_INT (FLD (f_FRk));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movgfd: movgfd$pack $GRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,movgfd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovgfd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (FLD (f_GRj), 0)) {
{
  {
    SI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
} else {
{
  {
    SI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = GET_H_GR (((FLD (f_GRj)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* movfgd: movfgd$pack $FRintk,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,movfgd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovfgd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (FLD (f_GRj), 0)) {
{
  {
    SI opval = GET_H_FR_INT (FLD (f_FRk));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_FR_INT (((FLD (f_FRk)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, ((FLD (f_GRj)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* movgfq: movgfq$pack $GRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,movgfq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movgfq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (FLD (f_GRj), 0)) {
{
  {
    SI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
} else {
{
  {
    SI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = GET_H_GR (((FLD (f_GRj)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = GET_H_GR (((FLD (f_GRj)) + (2)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = GET_H_GR (((FLD (f_GRj)) + (3)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* movfgq: movfgq$pack $FRintk,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,movfgq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movfgq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NESI (FLD (f_GRj), 0)) {
{
  {
    SI opval = GET_H_FR_INT (FLD (f_FRk));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_FR_INT (((FLD (f_FRk)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, ((FLD (f_GRj)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_FR_INT (((FLD (f_FRk)) + (2)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, ((FLD (f_GRj)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_FR_INT (((FLD (f_FRk)) + (3)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, ((FLD (f_GRj)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmovgf: cmovgf$pack $GRj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmovgf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovgfd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmovfg: cmovfg$pack $FRintk,$GRj,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmovfg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovfgd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = GET_H_FR_INT (FLD (f_FRk));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmovgfd: cmovgfd$pack $GRj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmovgfd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovgfd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (EQSI (FLD (f_GRj), 0)) {
{
  {
    SI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = 0;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
} else {
{
  {
    SI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = GET_H_GR (((FLD (f_GRj)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmovfgd: cmovfgd$pack $FRintk,$GRj,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmovfgd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmovfgd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDIF (NESI (FLD (f_GRj), 0), EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2)))) {
{
  {
    SI opval = GET_H_FR_INT (FLD (f_FRk));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
  {
    USI opval = GET_H_FR_INT (((FLD (f_FRk)) + (1)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, ((FLD (f_GRj)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* movgs: movgs$pack $GRj,$spr */

static SEM_PC
SEM_FN_NAME (frvbf,movgs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movgs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    USI opval = GET_H_GR (FLD (f_GRj));
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, FLD (f_spr), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movsg: movsg$pack $spr,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,movsg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movsg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_SPR (FLD (f_spr));
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, FLD (f_GRj), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* bra: bra$pack $hint_taken$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* bno: bno$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,bno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));

  return vpc;
#undef FLD
}

/* beq: beq$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,beq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bne: bne$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ble: ble$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,ble) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bgt: bgt$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* blt: blt$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,blt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bge: bge$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bls: bls$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bhi: bhi$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bc: bc$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnc: bnc$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bnc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bn: bn$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bn) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bp: bp$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bv: bv$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnv: bnv$pack $ICCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,bnv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbra: fbra$pack $hint_taken$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* fbno: fbno$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,fbno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));

  return vpc;
#undef FLD
}

/* fbne: fbne$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbeq: fbeq$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fblg: fblg$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fblg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbue: fbue$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbue) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbul: fbul$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbge: fbge$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fblt: fblt$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fblt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbuge: fbuge$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbuge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbug: fbug$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbug) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fble: fble$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fble) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbgt: fbgt$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbule: fbule$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbule) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbu: fbu$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbo: fbo$pack $FCCi_2,$hint,$label16 */

static SEM_PC
SEM_FN_NAME (frvbf,fbo) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fbne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, FLD (i_label16), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))))) {
  {
    USI opval = FLD (i_label16);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bctrlr: bctrlr$pack $ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bctrlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bralr: bralr$pack$hint_taken */

static SEM_PC
SEM_FN_NAME (frvbf,bralr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* bnolr: bnolr$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,bnolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));

  return vpc;
#undef FLD
}

/* beqlr: beqlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,beqlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnelr: bnelr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bnelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* blelr: blelr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,blelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bgtlr: bgtlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bgtlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bltlr: bltlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bltlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bgelr: bgelr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bgelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* blslr: blslr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,blslr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bhilr: bhilr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bhilr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bclr: bclr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bclr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnclr: bnclr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bnclr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnlr: bnlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bnlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bplr: bplr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bplr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bvlr: bvlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bvlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bnvlr: bnvlr$pack $ICCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bnvlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbralr: fbralr$pack$hint_taken */

static SEM_PC
SEM_FN_NAME (frvbf,fbralr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* fbnolr: fbnolr$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,fbnolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));

  return vpc;
#undef FLD
}

/* fbeqlr: fbeqlr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbeqlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbnelr: fbnelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbnelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fblglr: fblglr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fblglr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbuelr: fbuelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbuelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbullr: fbullr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbullr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbgelr: fbgelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbgelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbltlr: fbltlr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbltlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbugelr: fbugelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbugelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbuglr: fbuglr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbuglr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fblelr: fblelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fblelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbgtlr: fbgtlr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbgtlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbulelr: fbulelr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbulelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbulr: fbulr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbulr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fbolr: fbolr$pack $FCCi_2,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fbolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))))) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcralr: bcralr$pack $ccond$hint_taken */

static SEM_PC
SEM_FN_NAME (frvbf,bcralr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcnolr: bcnolr$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,bcnolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
((void) 0); /*nop*/
}
}

  return vpc;
#undef FLD
}

/* bceqlr: bceqlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bceqlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcnelr: bcnelr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcnelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bclelr: bclelr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bclelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcgtlr: bcgtlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcgtlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcltlr: bcltlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcltlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcgelr: bcgelr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcgelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bclslr: bclslr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bclslr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bchilr: bchilr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bchilr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcclr: bcclr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcclr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcnclr: bcnclr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcnclr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcnlr: bcnlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcnlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcplr: bcplr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcplr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcvlr: bcvlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcvlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* bcnvlr: bcnvlr$pack $ICCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,bcnvlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bceqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbralr: fcbralr$pack $ccond$hint_taken */

static SEM_PC
SEM_FN_NAME (frvbf,fcbralr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbnolr: fcbnolr$pack$hint_not_taken */

static SEM_PC
SEM_FN_NAME (frvbf,fcbnolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
((void) 0); /*nop*/
}
}

  return vpc;
#undef FLD
}

/* fcbeqlr: fcbeqlr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbeqlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbnelr: fcbnelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbnelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcblglr: fcblglr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcblglr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbuelr: fcbuelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbuelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbullr: fcbullr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbullr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbgelr: fcbgelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbgelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbltlr: fcbltlr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbltlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbugelr: fcbugelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbugelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbuglr: fcbuglr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbuglr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcblelr: fcblelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcblelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbgtlr: fcbgtlr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbgtlr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbulelr: fcbulelr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbulelr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbulr: fcbulr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbulr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcbolr: fcbolr$pack $FCCi_2,$ccond,$hint */

static SEM_PC
SEM_FN_NAME (frvbf,fcbolr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcbeqlr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_model_branch (current_cpu, GET_H_SPR (((UINT) 272)), FLD (f_hint));
{
  SI tmp_tmp;
  tmp_tmp = SUBSI (GET_H_SPR (((UINT) 273)), 1);
  {
    USI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, ((UINT) 273), opval);
    TRACE_RESULT (current_cpu, abuf, "spr", 'x', opval);
  }
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))))) {
if (EQSI (FLD (f_ccond), 0)) {
if (NESI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
} else {
if (EQSI (tmp_tmp, 0)) {
  {
    USI opval = GET_H_SPR (((UINT) 272));
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* jmpl: jmpl$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,jmpl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cjmpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* calll: calll$pack $callann($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,calll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cjmpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* jmpil: jmpil$pack @($GRi,$s12) */

static SEM_PC
SEM_FN_NAME (frvbf,jmpil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jmpil.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* callil: callil$pack @($GRi,$s12) */

static SEM_PC
SEM_FN_NAME (frvbf,callil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jmpil.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), FLD (f_d12)), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* call: call$pack $label24 */

static SEM_PC
SEM_FN_NAME (frvbf,call) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_call.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
  {
    USI opval = FLD (i_label24);
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* rett: rett$pack $debug */

static SEM_PC
SEM_FN_NAME (frvbf,rett) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rett.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    USI opval = frv_rett (current_cpu, pc, FLD (f_debug));
    sim_queue_pc_write (current_cpu, opval);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}

  return vpc;
#undef FLD
}

/* rei: rei$pack $eir */

static SEM_PC
SEM_FN_NAME (frvbf,rei) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* tra: tra$pack $GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tno: tno$pack */

static SEM_PC
SEM_FN_NAME (frvbf,tno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* teq: teq$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,teq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tne: tne$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tle: tle$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tgt: tgt$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tlt: tlt$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tlt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tge: tge$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tls: tls$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* thi: thi$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,thi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tc: tc$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tnc: tnc$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tnc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tn: tn$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tn) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tp: tp$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tv: tv$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tnv: tnv$pack $ICCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,tnv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_teq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftra: ftra$pack $GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftno: ftno$pack */

static SEM_PC
SEM_FN_NAME (frvbf,ftno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* ftne: ftne$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fteq: fteq$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,fteq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftlg: ftlg$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftlg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftue: ftue$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftue) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftul: ftul$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftge: ftge$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftlt: ftlt$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftlt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftuge: ftuge$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftuge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftug: ftug$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftug) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftle: ftle$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftgt: ftgt$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftule: ftule$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftule) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftu: ftu$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,ftu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fto: fto$pack $FCCi_2,$GRi,$GRj */

static SEM_PC
SEM_FN_NAME (frvbf,fto) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tira: tira$pack $GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tira) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tino: tino$pack */

static SEM_PC
SEM_FN_NAME (frvbf,tino) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* tieq: tieq$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tieq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tine: tine$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tine) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tile: tile$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tile) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tigt: tigt$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tigt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tilt: tilt$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tilt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tige: tige$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tige) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tils: tils$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tils) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tihi: tihi$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tihi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 4), 2))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tic: tic$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tic) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tinc: tinc$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tinc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tin: tin$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tip: tip$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tip) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 8), 3)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tiv: tiv$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tiv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* tinv: tinv$pack $ICCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,tinv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_tieq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftira: ftira$pack $GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftira) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftino: ftino$pack */

static SEM_PC
SEM_FN_NAME (frvbf,ftino) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* ftine: ftine$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftine) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftieq: ftieq$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftieq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftilg: ftilg$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftilg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiue: ftiue$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiue) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiul: ftiul$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftige: ftige$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftige) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftilt: ftilt$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftilt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiuge: ftiuge$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiuge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiug: ftiug$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiug) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftile: ftile$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftile) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftigt: ftigt$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftigt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiule: ftiule$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiule) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftiu: ftiu$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 1))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ftio: ftio$pack $FCCi_2,$GRi,$s12 */

static SEM_PC
SEM_FN_NAME (frvbf,ftio) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ftine.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_2)]), 2), 1))))) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
if (NEBI (CPU (h_psr_esr), 0)) {
{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
}
}
frv_itrap (current_cpu, pc, GET_H_GR (FLD (f_GRi)), FLD (f_d12));
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* break: break$pack */

static SEM_PC
SEM_FN_NAME (frvbf,break) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_break.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
; /*clobber*/
frv_break (current_cpu);
}

  return vpc;
#undef FLD
}

/* mtrap: mtrap$pack */

static SEM_PC
SEM_FN_NAME (frvbf,mtrap) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frv_mtrap (current_cpu);

  return vpc;
#undef FLD
}

/* andcr: andcr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,andcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 0, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* orcr: orcr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,orcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 1, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xorcr: xorcr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,xorcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 2, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nandcr: nandcr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,nandcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 3, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* norcr: norcr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,norcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 4, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* andncr: andncr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,andncr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 5, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* orncr: orncr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,orncr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 6, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nandncr: nandncr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,nandncr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 7, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* norncr: norncr$pack $CRi,$CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,norncr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = frvbf_cr_logic (current_cpu, 8, CPU (h_cccr[FLD (f_CRi)]), CPU (h_cccr[FLD (f_CRj)]));
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* notcr: notcr$pack $CRj,$CRk */

static SEM_PC
SEM_FN_NAME (frvbf,notcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andcr.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = XORQI (CPU (h_cccr[FLD (f_CRj)]), 1);
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRk)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ckra: ckra$pack $CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ckno: ckno$pack $CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ckeq: ckeq$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckne: ckne$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckle: ckle$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckgt: ckgt$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cklt: cklt$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,cklt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckge: ckge$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckls: ckls$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckhi: ckhi$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckc: ckc$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cknc: cknc$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,cknc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckn: ckn$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckn) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckp: ckp$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ckv: ckv$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,ckv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cknv: cknv$pack $ICCi_3,$CRj_int */

static SEM_PC
SEM_FN_NAME (frvbf,cknv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckra: fckra$pack $CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fckno: fckno$pack $CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fckne: fckne$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckeq: fckeq$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcklg: fcklg$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fcklg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckue: fckue$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckue) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckul: fckul$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckge: fckge$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcklt: fcklt$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fcklt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckuge: fckuge$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckuge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckug: fckug$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckug) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckle: fckle$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckgt: fckgt$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fckule: fckule$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fckule) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcku: fcku$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fcku) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcko: fcko$pack $FCCi_3,$CRj_float */

static SEM_PC
SEM_FN_NAME (frvbf,fcko) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 1);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckra: cckra$pack $CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckno: cckno$pack $CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckeq: cckeq$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckne: cckne$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckle: cckle$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckgt: cckgt$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)), XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ccklt: ccklt$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,ccklt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckge: cckge$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (XORBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckls: cckls$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckhi: cckhi$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (ORIF (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 4), 2))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckc: cckc$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ccknc: ccknc$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,ccknc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (TRUNCQIBI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckn: cckn$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckn) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckp: cckp$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 8), 3)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cckv: cckv$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cckv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ccknv: ccknv$pack $ICCi_3,$CRj_int,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,ccknv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cckeq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (NOTBI (TRUNCQIBI (SRLQI (ANDQI (CPU (h_iccr[FLD (f_ICCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_int)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckra: cfckra$pack $CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckno: cfckno$pack $CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckno) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckne: cfckne$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckeq: cfckeq$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfcklg: cfcklg$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfcklg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckue: cfckue$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckue) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckul: cfckul$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckge: cfckge$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfcklt: cfcklt$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfcklt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckuge: cfckuge$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckuge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckug: cfckug$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckug) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckle: cfckle$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckle) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckgt: cfckgt$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfckule: cfckule$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfckule) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfcku: cfcku$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfcku) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (TRUNCQIBI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 1))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfcko: cfcko$pack $FCCi_3,$CRj_float,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfcko) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfckne.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 8), 3)), ORIF (TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 4), 2)), TRUNCQIBI (SRLQI (ANDQI (CPU (h_fccr[FLD (f_FCCi_3)]), 2), 1))))) {
  {
    UQI opval = 3;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
} else {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}
} else {
  {
    UQI opval = 0;
    sim_queue_qi_write (current_cpu, & CPU (h_cccr[FLD (f_CRj_float)]), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "cccr", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cjmpl: cjmpl$pack @($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cjmpl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cjmpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ccalll: ccalll$pack @($GRi,$GRj),$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,ccalll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cjmpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
if (EQSI (FLD (f_LI), 1)) {
frvbf_set_write_next_vliw_addr_to_LR (current_cpu, 1);
}
  {
    USI opval = ANDSI (ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0xfffffffc);
    sim_queue_pc_write (current_cpu, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
frvbf_model_branch (current_cpu, pc, 2);
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ici: ici$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,ici) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_insn_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0);

  return vpc;
#undef FLD
}

/* dci: dci$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,dci) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_data_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0);

  return vpc;
#undef FLD
}

/* icei: icei$pack @($GRi,$GRj),$ae */

static SEM_PC
SEM_FN_NAME (frvbf,icei) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icei.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (FLD (f_ae), 0)) {
frvbf_insn_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), -1);
} else {
frvbf_insn_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), FLD (f_ae));
}

  return vpc;
#undef FLD
}

/* dcei: dcei$pack @($GRi,$GRj),$ae */

static SEM_PC
SEM_FN_NAME (frvbf,dcei) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icei.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (FLD (f_ae), 0)) {
frvbf_data_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), -1);
} else {
frvbf_data_cache_invalidate (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), FLD (f_ae));
}

  return vpc;
#undef FLD
}

/* dcf: dcf$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,dcf) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_data_cache_flush (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), 0);

  return vpc;
#undef FLD
}

/* dcef: dcef$pack @($GRi,$GRj),$ae */

static SEM_PC
SEM_FN_NAME (frvbf,dcef) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icei.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQSI (FLD (f_ae), 0)) {
frvbf_data_cache_flush (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), -1);
} else {
frvbf_data_cache_flush (current_cpu, ADDSI (GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj))), FLD (f_ae));
}

  return vpc;
#undef FLD
}

/* witlb: witlb$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,witlb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* wdtlb: wdtlb$pack $GRk,@($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,wdtlb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* itlbi: itlbi$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,itlbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* dtlbi: dtlbi$pack @($GRi,$GRj) */

static SEM_PC
SEM_FN_NAME (frvbf,dtlbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* icpl: icpl$pack $GRi,$GRj,$lock */

static SEM_PC
SEM_FN_NAME (frvbf,icpl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_insn_cache_preload (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_lock));

  return vpc;
#undef FLD
}

/* dcpl: dcpl$pack $GRi,$GRj,$lock */

static SEM_PC
SEM_FN_NAME (frvbf,dcpl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_icpl.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_data_cache_preload (current_cpu, GET_H_GR (FLD (f_GRi)), GET_H_GR (FLD (f_GRj)), FLD (f_lock));

  return vpc;
#undef FLD
}

/* icul: icul$pack $GRi */

static SEM_PC
SEM_FN_NAME (frvbf,icul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jmpil.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_insn_cache_unlock (current_cpu, GET_H_GR (FLD (f_GRi)));

  return vpc;
#undef FLD
}

/* dcul: dcul$pack $GRi */

static SEM_PC
SEM_FN_NAME (frvbf,dcul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jmpil.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_data_cache_unlock (current_cpu, GET_H_GR (FLD (f_GRi)));

  return vpc;
#undef FLD
}

/* bar: bar$pack */

static SEM_PC
SEM_FN_NAME (frvbf,bar) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* membar: membar$pack */

static SEM_PC
SEM_FN_NAME (frvbf,membar) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* lrai: lrai$pack $GRi,$GRk,$LRAE,$LRAD,$LRAS */

static SEM_PC
SEM_FN_NAME (frvbf,lrai) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* lrad: lrad$pack $GRi,$GRk,$LRAE,$LRAD,$LRAS */

static SEM_PC
SEM_FN_NAME (frvbf,lrad) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* tlbpr: tlbpr$pack $GRi,$GRj,$TLBPRopx,$TLBPRL */

static SEM_PC
SEM_FN_NAME (frvbf,tlbpr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* cop1: cop1$pack $s6_1,$CPRi,$CPRj,$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,cop1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* cop2: cop2$pack $s6_1,$CPRi,$CPRj,$CPRk */

static SEM_PC
SEM_FN_NAME (frvbf,cop2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* clrgr: clrgr$pack $GRk */

static SEM_PC
SEM_FN_NAME (frvbf,clrgr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_swapi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frv_ref_SI (GET_H_GR (FLD (f_GRk)));
frvbf_clear_ne_flags (current_cpu, FLD (f_GRk), 0);
}

  return vpc;
#undef FLD
}

/* clrfr: clrfr$pack $FRk */

static SEM_PC
SEM_FN_NAME (frvbf,clrfr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frv_ref_SI (GET_H_FR (FLD (f_FRk)));
frvbf_clear_ne_flags (current_cpu, FLD (f_FRk), 1);
}

  return vpc;
#undef FLD
}

/* clrga: clrga$pack */

static SEM_PC
SEM_FN_NAME (frvbf,clrga) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_clear_ne_flags (current_cpu, -1, 0);

  return vpc;
#undef FLD
}

/* clrfa: clrfa$pack */

static SEM_PC
SEM_FN_NAME (frvbf,clrfa) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_clear_ne_flags (current_cpu, -1, 1);

  return vpc;
#undef FLD
}

/* commitgr: commitgr$pack $GRk */

static SEM_PC
SEM_FN_NAME (frvbf,commitgr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setlos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_commit (current_cpu, FLD (f_GRk), 0);

  return vpc;
#undef FLD
}

/* commitfr: commitfr$pack $FRk */

static SEM_PC
SEM_FN_NAME (frvbf,commitfr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhsethis.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_commit (current_cpu, FLD (f_FRk), 1);

  return vpc;
#undef FLD
}

/* commitga: commitga$pack */

static SEM_PC
SEM_FN_NAME (frvbf,commitga) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_commit (current_cpu, -1, 0);

  return vpc;
#undef FLD
}

/* commitfa: commitfa$pack */

static SEM_PC
SEM_FN_NAME (frvbf,commitfa) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_commit (current_cpu, -1, 1);

  return vpc;
#undef FLD
}

/* fitos: fitos$pack $FRintj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fitos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fditos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fstoi: fstoi$pack $FRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,fstoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdstoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fitod: fitod$pack $FRintj,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fitod) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fitod.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsidf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdtoi: fdtoi$pack $FRdoublej,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,fdtoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdtoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixdfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fditos: fditos$pack $FRintj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fditos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fditos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdstoi: fdstoi$pack $FRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,fdstoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdstoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfditos: nfditos$pack $FRintj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfditos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fditos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdstoi: nfdstoi$pack $FRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdstoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdstoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    USI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfitos: cfitos$pack $FRintj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfitos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfitos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfstoi: cfstoi$pack $FRj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfstoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfstoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nfitos: nfitos$pack $FRintj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfitos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fditos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->floatsisf (CGEN_CPU_FPU (current_cpu), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfstoi: nfstoi$pack $FRj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,nfstoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdstoi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SI opval = CGEN_CPU_FPU (current_cpu)->ops->fixsfsi (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* fmovs: fmovs$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmovs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = GET_H_FR (FLD (f_FRj));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovd: fmovd$pack $FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fmovd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_FR_DOUBLE (FLD (f_FRj));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdmovs: fdmovs$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmovs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FR (FLD (f_FRj));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = GET_H_FR (((FLD (f_FRj)) + (1)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfmovs: cfmovs$pack $FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmovs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = GET_H_FR (FLD (f_FRj));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fnegs: fnegs$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fnegs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->negsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fnegd: fnegd$pack $FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fnegd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->negdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdnegs: fdnegs$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdnegs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->negsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->negsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfnegs: cfnegs$pack $FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfnegs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->negsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fabss: fabss$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fabss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->abssf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fabsd: fabsd$pack $FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fabsd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->absdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdabss: fdabss$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdabss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->abssf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->abssf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfabss: cfabss$pack $FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfabss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->abssf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fsqrts: fsqrts$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdsqrts: fdsqrts$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdsqrts: nfdsqrts$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fsqrtd: fsqrtd$pack $FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fsqrtd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* cfsqrts: cfsqrts$pack $FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nfsqrts: nfsqrts$pack $FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->sqrtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fadds: fadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsubs: fsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmuls: fmuls$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdivs: fdivs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* faddd: faddd$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,faddd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->adddf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsubd: fsubd$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fsubd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->subdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmuld: fmuld$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fmuld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->muldf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdivd: fdivd$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fdivd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->divdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* cfadds: cfadds$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfsubs: cfsubs$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfmuls: cfmuls$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfdivs: cfdivs$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfdivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nfadds: nfadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfsubs: nfsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfmuls: nfmuls$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdivs: nfdivs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fcmps: fcmps$pack $FRi,$FRj,$FCCi_2 */

static SEM_PC
SEM_FN_NAME (frvbf,fcmps) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfcmps.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fcmpd: fcmpd$pack $FRdoublei,$FRdoublej,$FCCi_2 */

static SEM_PC
SEM_FN_NAME (frvbf,fcmpd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fcmpd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (CGEN_CPU_FPU (current_cpu)->ops->gtdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltdf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj)))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfcmps: cfcmps$pack $FRi,$FRj,$FCCi_2,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfcmps) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfcmps.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fdcmps: fdcmps$pack $FRi,$FRj,$FCCi_2 */

static SEM_PC
SEM_FN_NAME (frvbf,fdcmps) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_nfdcmps.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fmadds: fmadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmsubs: fmsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmaddd: fmaddd$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fmaddd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->adddf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->muldf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj))), GET_H_FR_DOUBLE (FLD (f_FRk)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmsubd: fmsubd$pack $FRdoublei,$FRdoublej,$FRdoublek */

static SEM_PC
SEM_FN_NAME (frvbf,fmsubd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmaddd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = CGEN_CPU_FPU (current_cpu)->ops->subdf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->muldf (CGEN_CPU_FPU (current_cpu), GET_H_FR_DOUBLE (FLD (f_FRi)), GET_H_FR_DOUBLE (FLD (f_FRj))), GET_H_FR_DOUBLE (FLD (f_FRk)));
    sim_queue_fn_df_write (current_cpu, frvbf_h_fr_double_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_double", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdmadds: fdmadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1)))), GET_H_FR (((FLD (f_FRk)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdmadds: nfdmadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdmadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1)))), GET_H_FR (((FLD (f_FRk)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfmadds: cfmadds$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfmsubs: cfmsubs$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* nfmadds: nfmadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfmadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfmsubs: nfmsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfmsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj))), GET_H_FR (FLD (f_FRk)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fmas: fmas$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmas) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fmss: fmss$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdmas: fdmas$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmas) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (2))), GET_H_FR (((FLD (f_FRj)) + (2))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (2)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (3))), GET_H_FR (((FLD (f_FRj)) + (3))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (3)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdmss: fdmss$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (2))), GET_H_FR (((FLD (f_FRj)) + (2))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (2)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (3))), GET_H_FR (((FLD (f_FRj)) + (3))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (3)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdmas: nfdmas$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdmas) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 2));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 3));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (2))), GET_H_FR (((FLD (f_FRj)) + (2))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (2)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (3))), GET_H_FR (((FLD (f_FRj)) + (3))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (3)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdmss: nfdmss$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdmss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 2));
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 3));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (2))), GET_H_FR (((FLD (f_FRj)) + (2))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (2)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (3))), GET_H_FR (((FLD (f_FRj)) + (3))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (3)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* cfmas: cfmas$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmas) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cfmss: cfmss$pack $FRi,$FRj,$FRk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cfmss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cfmas.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fmad: fmad$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmad) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->muldf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi))), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->adddf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1)))), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fmsd: fmsd$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fmsd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->muldf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi))), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRj)))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->subdf (CGEN_CPU_FPU (current_cpu), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1)))), CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRj)) + (1))))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfmas: nfmas$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfmas) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfmss: nfmss$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfmss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdadds: fdadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdsubs: fdsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdmuls: fdmuls$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fddivs: fddivs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fddivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdsads: fdsads$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdsads) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fdmulcs: fdmulcs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,fdmulcs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdmulcs: nfdmulcs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdmulcs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdadds: nfdadds$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdsubs: nfdsubs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdmuls: nfdmuls$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->mulsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfddivs: nfddivs$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfddivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->divsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdsads: nfdsads$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,nfdsads) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fdmadds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->addsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
  {
    SF opval = CGEN_CPU_FPU (current_cpu)->ops->subsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))));
    sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, ((FLD (f_FRk)) + (1)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* nfdcmps: nfdcmps$pack $FRi,$FRj,$FCCi_2 */

static SEM_PC
SEM_FN_NAME (frvbf,nfdcmps) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_nfdcmps.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frvbf_set_ne_index (current_cpu, FLD (f_FRk));
if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (FLD (f_FRi)), GET_H_FR (FLD (f_FRj)))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCi_2)]), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
frvbf_set_ne_index (current_cpu, ADDSI (FLD (f_FRk), 1));
if (CGEN_CPU_FPU (current_cpu)->ops->gtsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->eqsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (CGEN_CPU_FPU (current_cpu)->ops->ltsf (CGEN_CPU_FPU (current_cpu), GET_H_FR (((FLD (f_FRi)) + (1))), GET_H_FR (((FLD (f_FRj)) + (1))))) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCi_2)) + (1))]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mhsetlos: mhsetlos$pack $u12,$FRklo */

static SEM_PC
SEM_FN_NAME (frvbf,mhsetlos) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhsetlos.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UHI opval = FLD (f_u12);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mhsethis: mhsethis$pack $u12,$FRkhi */

static SEM_PC
SEM_FN_NAME (frvbf,mhsethis) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhsethis.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UHI opval = FLD (f_u12);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mhdsets: mhdsets$pack $u12,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mhdsets) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhdsets.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = FLD (f_u12);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = FLD (f_u12);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mhsetloh: mhsetloh$pack $s5,$FRklo */

static SEM_PC
SEM_FN_NAME (frvbf,mhsetloh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhsetloh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmp;
  tmp_tmp = GET_H_FR_LO (FLD (f_FRk));
  tmp_tmp = ANDHI (tmp_tmp, 2047);
  tmp_tmp = ORHI (tmp_tmp, SLLSI (ANDSI (FLD (f_s5), 31), 11));
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mhsethih: mhsethih$pack $s5,$FRkhi */

static SEM_PC
SEM_FN_NAME (frvbf,mhsethih) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhsethih.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_tmp;
  tmp_tmp = GET_H_FR_HI (FLD (f_FRk));
  tmp_tmp = ANDHI (tmp_tmp, 2047);
  tmp_tmp = ORHI (tmp_tmp, SLLSI (ANDSI (FLD (f_s5), 31), 11));
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mhdseth: mhdseth$pack $s5,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mhdseth) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mhdseth.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  HI tmp_tmp;
  tmp_tmp = GET_H_FR_HI (((FLD (f_FRk)) + (0)));
  tmp_tmp = ANDHI (tmp_tmp, 2047);
  tmp_tmp = ORHI (tmp_tmp, SLLSI (ANDSI (FLD (f_s5), 31), 11));
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
{
  HI tmp_tmp;
  tmp_tmp = GET_H_FR_LO (((FLD (f_FRk)) + (0)));
  tmp_tmp = ANDHI (tmp_tmp, 2047);
  tmp_tmp = ORHI (tmp_tmp, SLLSI (ANDSI (FLD (f_s5), 31), 11));
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  return vpc;
#undef FLD
}

/* mand: mand$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mand) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mor: mor$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mxor: mxor$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mxor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmand: cmand$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmand) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmand.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = ANDSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmor: cmor$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmand.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = ORSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmxor: cmxor$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmxor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmand.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = XORSI (GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mnot: mnot$pack $FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mnot) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = INVSI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmnot: cmnot$pack $FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmnot) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmand.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
  {
    SI opval = INVSI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mrotli: mrotli$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mrotli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ROLSI (GET_H_FR_INT (FLD (f_FRi)), ANDSI (FLD (f_u6), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mrotri: mrotri$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mrotri) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = RORSI (GET_H_FR_INT (FLD (f_FRi)), ANDSI (FLD (f_u6), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mwcut: mwcut$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mwcut) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_cut (current_cpu, GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (((FLD (f_FRi)) + (1))), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mwcuti: mwcuti$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mwcuti) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_cut (current_cpu, GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (((FLD (f_FRi)) + (1))), FLD (f_u6));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mcut: mcut$pack $ACC40Si,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcut) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_media_cut (current_cpu, GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mcuti: mcuti$pack $ACC40Si,$s6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcuti) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_media_cut (current_cpu, GET_H_ACC40S (FLD (f_ACC40Si)), FLD (f_s6));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mcutss: mcutss$pack $ACC40Si,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcutss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_media_cut_ss (current_cpu, GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mcutssi: mcutssi$pack $ACC40Si,$s6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcutssi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_media_cut_ss (current_cpu, GET_H_ACC40S (FLD (f_ACC40Si)), FLD (f_s6));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mdcutssi: mdcutssi$pack $ACC40Si,$s6,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mdcutssi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdcutssi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_ACC40Si), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frvbf_media_cut_ss (current_cpu, GET_H_ACC40S (FLD (f_ACC40Si)), FLD (f_s6));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = frvbf_media_cut_ss (current_cpu, GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))), FLD (f_s6));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* maveh: maveh$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,maveh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcut.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = frvbf_media_average (current_cpu, GET_H_FR_INT (FLD (f_FRi)), GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* msllhi: msllhi$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msllhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_msllhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* msrlhi: msrlhi$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msrlhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_msllhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = SRLHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SRLHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* msrahi: msrahi$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msrahi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_msllhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mdrotli: mdrotli$pack $FRintieven,$s6,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mdrotli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdrotli.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = ROLSI (GET_H_FR_INT (FLD (f_FRi)), ANDSI (FLD (f_s6), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    USI opval = ROLSI (GET_H_FR_INT (((FLD (f_FRi)) + (1))), ANDSI (FLD (f_s6), 31));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcplhi: mcplhi$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcplhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcplhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_arg1;
  HI tmp_arg2;
  HI tmp_shift;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  tmp_shift = ANDSI (FLD (f_u6), 15);
  tmp_arg1 = SLLHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), tmp_shift);
if (NEHI (tmp_shift, 0)) {
{
  tmp_arg2 = GET_H_FR_HI (((FLD (f_FRi)) + (1)));
  tmp_arg2 = SRLHI (SLLHI (tmp_arg2, SUBSI (15, tmp_shift)), SUBSI (15, tmp_shift));
  tmp_arg1 = ORHI (tmp_arg1, tmp_arg2);
}
}
  {
    UHI opval = tmp_arg1;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcpli: mcpli$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mcpli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_tmp;
  SI tmp_shift;
  tmp_shift = ANDSI (FLD (f_u6), 31);
  tmp_tmp = SLLSI (GET_H_FR_INT (FLD (f_FRi)), tmp_shift);
if (NESI (tmp_shift, 0)) {
{
  SI tmp_tmp1;
  tmp_tmp1 = SRLSI (SLLSI (GET_H_FR_INT (((FLD (f_FRi)) + (1))), SUBSI (31, tmp_shift)), SUBSI (31, tmp_shift));
  tmp_tmp = ORSI (tmp_tmp, tmp_tmp1);
}
}
  {
    SI opval = tmp_tmp;
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* msaths: msaths$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msaths) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTHI (tmp_argihi, tmp_argjhi)) {
  {
    UHI opval = tmp_argjhi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
if (LTHI (tmp_argihi, INVHI (tmp_argjhi))) {
  {
    UHI opval = INVHI (tmp_argjhi);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argihi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
if (GTHI (tmp_argilo, tmp_argjlo)) {
  {
    UHI opval = tmp_argjlo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
if (LTHI (tmp_argilo, INVHI (tmp_argjlo))) {
  {
    UHI opval = INVHI (tmp_argjlo);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argilo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqsaths: mqsaths$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqsaths) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTHI (tmp_argihi, tmp_argjhi)) {
  {
    UHI opval = tmp_argjhi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
if (LTHI (tmp_argihi, INVHI (tmp_argjhi))) {
  {
    UHI opval = INVHI (tmp_argjhi);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argihi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
if (GTHI (tmp_argilo, tmp_argjlo)) {
  {
    UHI opval = tmp_argjlo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
if (LTHI (tmp_argilo, INVHI (tmp_argjlo))) {
  {
    UHI opval = INVHI (tmp_argjlo);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argilo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTHI (tmp_argihi, tmp_argjhi)) {
  {
    UHI opval = tmp_argjhi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
if (LTHI (tmp_argihi, INVHI (tmp_argjhi))) {
  {
    UHI opval = INVHI (tmp_argjhi);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argihi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
if (GTHI (tmp_argilo, tmp_argjlo)) {
  {
    UHI opval = tmp_argjlo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
if (LTHI (tmp_argilo, INVHI (tmp_argjlo))) {
  {
    UHI opval = INVHI (tmp_argjlo);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argilo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* msathu: msathu$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msathu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTUHI (tmp_argihi, tmp_argjhi)) {
  {
    UHI opval = tmp_argjhi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argihi;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
if (GTUHI (tmp_argilo, tmp_argjlo)) {
  {
    UHI opval = tmp_argjlo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
} else {
  {
    UHI opval = tmp_argilo;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcmpsh: mcmpsh$pack $FRinti,$FRintj,$FCCk */

static SEM_PC
SEM_FN_NAME (frvbf,mcmpsh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcmpsh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_FCCk), SUBSI (2, 1))) {
frvbf_media_cr_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (EQHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (LTHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
if (GTHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (EQHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (LTHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcmpuh: mcmpuh$pack $FRinti,$FRintj,$FCCk */

static SEM_PC
SEM_FN_NAME (frvbf,mcmpuh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcmpsh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_FCCk), SUBSI (2, 1))) {
frvbf_media_cr_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
if (GTUHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (EQHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (LTUHI (tmp_argihi, tmp_argjhi)) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[FLD (f_FCCk)]), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
if (GTUHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 2;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (EQHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 8;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
if (LTUHI (tmp_argilo, tmp_argjlo)) {
  {
    UQI opval = 4;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
} else {
  {
    UQI opval = 1;
    sim_queue_qi_write (current_cpu, & CPU (h_fccr[((FLD (f_FCCk)) + (1))]), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fccr", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mabshs: mabshs$pack $FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mabshs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mabshs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_arghi;
  HI tmp_arglo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  tmp_arghi = GET_H_FR_HI (((FLD (f_FRj)) + (0)));
  tmp_arglo = GET_H_FR_LO (((FLD (f_FRj)) + (0)));
if (GTDI (ABSHI (tmp_arghi), 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (ABSHI (tmp_arghi), -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = ABSHI (tmp_arghi);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
if (GTDI (ABSHI (tmp_arglo), 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (ABSHI (tmp_arglo), -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = ABSHI (tmp_arglo);
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* maddhss: maddhss$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,maddhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* maddhus: maddhus$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,maddhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* msubhss: msubhss$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msubhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* msubhus: msubhus$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,msubhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmaddhss: cmaddhss$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmaddhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmaddhus: cmaddhus$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmaddhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmsubhss: cmsubhss$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmsubhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmsubhus: cmsubhus$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmsubhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqaddhss: mqaddhss$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqaddhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqaddhus: mqaddhus$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqaddhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqsubhss: mqsubhss$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqsubhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqsubhus: mqsubhus$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqsubhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqaddhss: cmqaddhss$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqaddhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqaddhus: cmqaddhus$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqaddhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqsubhss: cmqsubhss$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqsubhss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 32767)) {
{
  {
    UHI opval = 32767;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, -32768)) {
{
  {
    UHI opval = -32768;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqsubhus: cmqsubhus$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqsubhus) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argihi, tmp_argjhi);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBHI (tmp_argilo, tmp_argjlo);
if (GTDI (tmp_tmp, 65535)) {
{
  {
    UHI opval = 65535;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, 0)) {
{
  {
    UHI opval = 0;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqlclrhs: mqlclrhs$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqlclrhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_a1;
  HI tmp_a2;
  HI tmp_a3;
  HI tmp_a4;
  HI tmp_b1;
  HI tmp_b2;
  HI tmp_b3;
  HI tmp_b4;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_a1 = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_a2 = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_b1 = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_b2 = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  tmp_a3 = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_a4 = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_b3 = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_b4 = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    UHI opval = (LEUHI (ABSHI (tmp_a1), ABSHI (tmp_b1))) ? (0) : (LEHI (0, tmp_b1)) ? (tmp_a1) : (EQHI (tmp_a1, -32768)) ? (32767) : (NEGHI (tmp_a1));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = (LEUHI (ABSHI (tmp_a2), ABSHI (tmp_b2))) ? (0) : (LEHI (0, tmp_b2)) ? (tmp_a2) : (EQHI (tmp_a2, -32768)) ? (32767) : (NEGHI (tmp_a2));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = (LEUHI (ABSHI (tmp_a3), ABSHI (tmp_b3))) ? (0) : (LEHI (0, tmp_b3)) ? (tmp_a3) : (EQHI (tmp_a3, -32768)) ? (32767) : (NEGHI (tmp_a3));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = (LEUHI (ABSHI (tmp_a4), ABSHI (tmp_b4))) ? (0) : (LEHI (0, tmp_b4)) ? (tmp_a4) : (EQHI (tmp_a4, -32768)) ? (32767) : (NEGHI (tmp_a4));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqlmths: mqlmths$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqlmths) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_a1;
  HI tmp_a2;
  HI tmp_a3;
  HI tmp_a4;
  HI tmp_b1;
  HI tmp_b2;
  HI tmp_b3;
  HI tmp_b4;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  tmp_a1 = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_a2 = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_b1 = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_b2 = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  tmp_a3 = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_a4 = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_b3 = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_b4 = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    UHI opval = (ANDIF (GTHI (tmp_b1, -32768), GEHI (tmp_a1, ABSHI (tmp_b1)))) ? (tmp_b1) : (GTHI (tmp_a1, NEGHI (ABSHI (tmp_b1)))) ? (tmp_a1) : (EQHI (tmp_b1, -32768)) ? (32767) : (NEGHI (tmp_b1));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = (ANDIF (GTHI (tmp_b2, -32768), GEHI (tmp_a2, ABSHI (tmp_b2)))) ? (tmp_b2) : (GTHI (tmp_a2, NEGHI (ABSHI (tmp_b2)))) ? (tmp_a2) : (EQHI (tmp_b2, -32768)) ? (32767) : (NEGHI (tmp_b2));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = (ANDIF (GTHI (tmp_b3, -32768), GEHI (tmp_a3, ABSHI (tmp_b3)))) ? (tmp_b3) : (GTHI (tmp_a3, NEGHI (ABSHI (tmp_b3)))) ? (tmp_a3) : (EQHI (tmp_b3, -32768)) ? (32767) : (NEGHI (tmp_b3));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = (ANDIF (GTHI (tmp_b4, -32768), GEHI (tmp_a4, ABSHI (tmp_b4)))) ? (tmp_b4) : (GTHI (tmp_a4, NEGHI (ABSHI (tmp_b4)))) ? (tmp_a4) : (EQHI (tmp_b4, -32768)) ? (32767) : (NEGHI (tmp_b4));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqsllhi: mqsllhi$pack $FRintieven,$u6,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqsllhi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mqsllhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SLLHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqsrahi: mqsrahi$pack $FRintieven,$u6,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mqsrahi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mqsllhi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = SRAHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), ANDSI (FLD (f_u6), 15));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* maddaccs: maddaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,maddaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* msubaccs: msubaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,msubaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mdaddaccs: mdaddaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mdaddaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Si)) + (2))), GET_H_ACC40S (((FLD (f_ACC40Si)) + (3))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mdsubaccs: mdsubaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mdsubaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (((FLD (f_ACC40Si)) + (2))), GET_H_ACC40S (((FLD (f_ACC40Si)) + (3))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* masaccs: masaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,masaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mdasaccs: mdasaccs$pack $ACC40Si,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mdasaccs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Si))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Si), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (FLD (f_ACC40Si)), GET_H_ACC40S (((FLD (f_ACC40Si)) + (1))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Si)) + (2))), GET_H_ACC40S (((FLD (f_ACC40Si)) + (3))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (((FLD (f_ACC40Si)) + (2))), GET_H_ACC40S (((FLD (f_ACC40Si)) + (3))));
if (GTDI (tmp_tmp, 549755813887)) {
{
  {
    DI opval = 549755813887;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, INVDI (549755813887))) {
{
  {
    DI opval = INVDI (549755813887);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmulhs: mmulhs$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmulhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmulhu: mmulhu$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmulhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmulxhs: mmulxhs$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmulxhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmulxhu: mmulxhu$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmulxhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmmulhs: cmmulhs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmmulhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmmulhu: cmmulhu$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmmulhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmulhs: mqmulhs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmulhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmulhu: mqmulhu$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmulhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmulxhs: mqmulxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmulxhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmulxhu: mqmulxhu$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmulxhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqmulhs: cmqmulhs$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqmulhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqmulhu: cmqmulhu$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqmulhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
  {
    DI opval = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmachs: mmachs$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmachs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmachu: mmachu$pack $FRinti,$FRintj,$ACC40Uk */

static SEM_PC
SEM_FN_NAME (frvbf,mmachu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Uk))) {
if (ANDSI (FLD (f_ACC40Uk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (FLD (f_ACC40Uk)), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (1))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmrdhs: mmrdhs$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mmrdhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mmrdhu: mmrdhu$pack $FRinti,$FRintj,$ACC40Uk */

static SEM_PC
SEM_FN_NAME (frvbf,mmrdhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Uk))) {
if (ANDSI (FLD (f_ACC40Uk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40U (FLD (f_ACC40Uk)), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = SUBDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (1))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmmachs: cmmachs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmmachs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmmachu: cmmachu$pack $FRinti,$FRintj,$ACC40Uk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmmachu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Uk))) {
if (ANDSI (FLD (f_ACC40Uk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (FLD (f_ACC40Uk)), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (1))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmachs: mqmachs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmachs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (2))), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (3))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmachu: mqmachu$pack $FRintieven,$FRintjeven,$ACC40Uk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmachu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Uk))) {
if (ANDSI (FLD (f_ACC40Uk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (FLD (f_ACC40Uk)), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (1))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (2))), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (3))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqmachs: cmqmachs$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqmachs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (2))), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (3))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmqmachu: cmqmachu$pack $FRintieven,$FRintjeven,$ACC40Uk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmqmachu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachu.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Uk))) {
if (ANDSI (FLD (f_ACC40Uk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (FLD (f_ACC40Uk)), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, FLD (f_ACC40Uk), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (1))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (1)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (2))), MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (2)), opval);
    written |= (1 << 21);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40U (((FLD (f_ACC40Uk)) + (3))), MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (255, 0xffffffff))) {
{
  {
    UDI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0, 0))) {
{
  {
    UDI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    UDI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40U_set, ((FLD (f_ACC40Uk)) + (3)), opval);
    written |= (1 << 22);
    TRACE_RESULT (current_cpu, abuf, "acc40U", 'D', opval);
  }
}
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqxmachs: mqxmachs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqxmachs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (2))), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (3))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqxmacxhs: mqxmacxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqxmacxhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (2))), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (3))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqmacxhs: mqmacxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqmacxhs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (4, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (1))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (2))), MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 2);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (2)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  DI tmp_tmp;
  tmp_tmp = ADDDI (GET_H_ACC40S (((FLD (f_ACC40Sk)) + (3))), MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi)));
if (GTDI (tmp_tmp, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
if (LTDI (tmp_tmp, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 1);
}
} else {
  {
    DI opval = tmp_tmp;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (3)), opval);
    written |= (1 << 20);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcpxrs: mcpxrs$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mcpxrs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcpxru: mcpxru$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mcpxru) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcpxis: mcpxis$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mcpxis) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mcpxiu: mcpxiu$pack $FRinti,$FRintj,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mcpxiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmcpxrs: cmcpxrs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmcpxrs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmcpxru: cmcpxru$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmcpxru) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmcpxis: cmcpxis$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmcpxis) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmcpxiu: cmcpxiu$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmcpxiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqcpxrs: mqcpxrs$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqcpxrs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqcpxru: mqcpxru$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqcpxru) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjhi));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjlo));
  tmp_tmp1 = SUBDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqcpxis: mqcpxis$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqcpxis) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  HI tmp_argihi;
  HI tmp_argilo;
  HI tmp_argjhi;
  HI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (EXTHIDI (tmp_argihi), EXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (EXTHIDI (tmp_argilo), EXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (127, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (127, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0xffffff80, 0))) {
{
  {
    DI opval = MAKEDI (0xffffff80, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mqcpxiu: mqcpxiu$pack $FRintieven,$FRintjeven,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mqcpxiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmqmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (frvbf_check_acc_range (current_cpu, FLD (f_ACC40Sk))) {
if (ANDSI (FLD (f_ACC40Sk), SUBSI (2, 1))) {
frvbf_media_acc_not_aligned (current_cpu);
} else {
if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRj), SUBSI (2, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_argihi;
  UHI tmp_argilo;
  UHI tmp_argjhi;
  UHI tmp_argjlo;
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (0))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 8);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
{
  tmp_argihi = ADDHI (GET_H_FR_HI (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argilo = ADDHI (GET_H_FR_LO (((FLD (f_FRi)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRi)), 0));
  tmp_argjhi = ADDHI (GET_H_FR_HI (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
  tmp_argjlo = ADDHI (GET_H_FR_LO (((FLD (f_FRj)) + (1))), MULSI (GET_H_FR_INT (FLD (f_FRj)), 0));
}
{
  DI tmp_tmp1;
  DI tmp_tmp2;
  tmp_tmp1 = MULDI (ZEXTHIDI (tmp_argihi), ZEXTHIDI (tmp_argjlo));
  tmp_tmp2 = MULDI (ZEXTHIDI (tmp_argilo), ZEXTHIDI (tmp_argjhi));
  tmp_tmp1 = ADDDI (tmp_tmp1, tmp_tmp2);
if (GTDI (tmp_tmp1, MAKEDI (255, 0xffffffff))) {
{
  {
    DI opval = MAKEDI (255, 0xffffffff);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
if (LTDI (tmp_tmp1, MAKEDI (0, 0))) {
{
  {
    DI opval = MAKEDI (0, 0);
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
frvbf_media_overflow (current_cpu, 4);
}
} else {
  {
    DI opval = tmp_tmp1;
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, ((FLD (f_ACC40Sk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }
}
}
}
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mexpdhw: mexpdhw$pack $FRinti,$u6,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mexpdhw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmexpdhw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  UHI tmp_tmp;
if (ANDSI (FLD (f_u6), 1)) {
  tmp_tmp = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
} else {
  tmp_tmp = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
}
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* cmexpdhw: cmexpdhw$pack $FRinti,$u6,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmexpdhw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmexpdhw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_tmp;
if (ANDSI (FLD (f_u6), 1)) {
  tmp_tmp = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
} else {
  tmp_tmp = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
}
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mexpdhd: mexpdhd$pack $FRinti,$u6,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mexpdhd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmexpdhd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  UHI tmp_tmp;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_u6), 1)) {
  tmp_tmp = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
} else {
  tmp_tmp = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
}
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmexpdhd: cmexpdhd$pack $FRinti,$u6,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmexpdhd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmexpdhd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  UHI tmp_tmp;
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_u6), 1)) {
  tmp_tmp = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
} else {
  tmp_tmp = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
}
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = tmp_tmp;
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mpackh: mpackh$pack $FRinti,$FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mpackh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmaddhss.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mdpackh: mdpackh$pack $FRintieven,$FRintjeven,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mdpackh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdpackh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ORIF (ANDSI (FLD (f_FRj), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (2, 1))))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
{
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* munpackh: munpackh$pack $FRinti,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,munpackh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_munpackh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (ADDSI (0, 1))), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (ADDSI (0, 1))), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mdunpackh: mdunpackh$pack $FRintieven,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mdunpackh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdunpackh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (ORIF (ANDSI (FLD (f_FRi), SUBSI (2, 1)), ANDSI (FLD (f_FRk), SUBSI (4, 1)))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRi), opval);
    written |= (1 << 8);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    written |= (1 << 9);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
{
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (ADDSI (0, 1))), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (ADDSI (0, 1))), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
{
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRi)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (ADDSI (2, 1))), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRi)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (ADDSI (2, 1))), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mbtoh: mbtoh$pack $FRintj,$FRintkeven */

static SEM_PC
SEM_FN_NAME (frvbf,mbtoh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmbtoh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmbtoh: cmbtoh$pack $FRintj,$FRintkeven,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmbtoh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmbtoh.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRk), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mhtob: mhtob$pack $FRintjeven,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mhtob) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmhtob.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRj), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_3_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_3", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_2_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_2", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRj)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_1_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_1", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_0_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_0", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmhtob: cmhtob$pack $FRintjeven,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmhtob) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmhtob.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRj), SUBSI (2, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_3_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_3", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_2_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_2", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_HI (((FLD (f_FRj)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_1_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_1", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_LO (((FLD (f_FRj)) + (1)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_0_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_0", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mbtohe: mbtohe$pack $FRintj,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mbtohe) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmbtohe.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRk), SUBSI (4, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
{
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 10);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 11);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmbtohe: cmbtohe$pack $FRintj,$FRintk,$CCi,$cond */

static SEM_PC
SEM_FN_NAME (frvbf,cmbtohe) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmbtohe.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRj)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRj), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
  {
    SI opval = frv_ref_SI (GET_H_FR_INT (FLD (f_FRk)));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }
if (ANDSI (FLD (f_FRk), SUBSI (4, 1))) {
frvbf_media_register_not_aligned (current_cpu);
} else {
if (EQQI (CPU (h_cccr[FLD (f_CCi)]), ORSI (FLD (f_cond), 2))) {
{
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 12);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_3 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (0)), opval);
    written |= (1 << 16);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 13);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_2 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (1)), opval);
    written |= (1 << 17);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 14);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_1 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (2)), opval);
    written |= (1 << 18);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_hi_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 15);
    TRACE_RESULT (current_cpu, abuf, "fr_hi", 'x', opval);
  }
  {
    UHI opval = GET_H_FR_0 (((FLD (f_FRj)) + (0)));
    sim_queue_fn_hi_write (current_cpu, frvbf_h_fr_lo_set, ((FLD (f_FRk)) + (3)), opval);
    written |= (1 << 19);
    TRACE_RESULT (current_cpu, abuf, "fr_lo", 'x', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* mnop: mnop$pack */

static SEM_PC
SEM_FN_NAME (frvbf,mnop) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* mclracc-0: mclracc$pack $ACC40Sk,$A0 */

static SEM_PC
SEM_FN_NAME (frvbf,mclracc_0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_clear_accumulators (current_cpu, FLD (f_ACC40Sk), 0);

  return vpc;
#undef FLD
}

/* mclracc-1: mclracc$pack $ACC40Sk,$A1 */

static SEM_PC
SEM_FN_NAME (frvbf,mclracc_1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mdasaccs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_clear_accumulators (current_cpu, FLD (f_ACC40Sk), 1);

  return vpc;
#undef FLD
}

/* mrdacc: mrdacc$pack $ACC40Si,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mrdacc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcuti.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_ACC40S (FLD (f_ACC40Si));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mrdaccg: mrdaccg$pack $ACCGi,$FRintk */

static SEM_PC
SEM_FN_NAME (frvbf,mrdaccg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrdaccg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_ACCG (FLD (f_ACCGi));
    sim_queue_fn_si_write (current_cpu, frvbf_h_fr_int_set, FLD (f_FRk), opval);
    TRACE_RESULT (current_cpu, abuf, "fr_int", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mwtacc: mwtacc$pack $FRinti,$ACC40Sk */

static SEM_PC
SEM_FN_NAME (frvbf,mwtacc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_cmmachs.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (ANDDI (GET_H_ACC40S (FLD (f_ACC40Sk)), MAKEDI (0xffffffff, 0)), GET_H_FR_INT (FLD (f_FRi)));
    sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, FLD (f_ACC40Sk), opval);
    TRACE_RESULT (current_cpu, abuf, "acc40S", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mwtaccg: mwtaccg$pack $FRinti,$ACCGk */

static SEM_PC
SEM_FN_NAME (frvbf,mwtaccg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mwtaccg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
frv_ref_SI (GET_H_ACCG (FLD (f_ACCGk)));
  {
    USI opval = GET_H_FR_INT (FLD (f_FRi));
    sim_queue_fn_si_write (current_cpu, frvbf_h_accg_set, FLD (f_ACCGk), opval);
    TRACE_RESULT (current_cpu, abuf, "accg", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcop1: mcop1$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,mcop1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_media_cop (current_cpu, 1);

  return vpc;
#undef FLD
}

/* mcop2: mcop2$pack $FRi,$FRj,$FRk */

static SEM_PC
SEM_FN_NAME (frvbf,mcop2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

frvbf_media_cop (current_cpu, 2);

  return vpc;
#undef FLD
}

/* fnop: fnop$pack */

static SEM_PC
SEM_FN_NAME (frvbf,fnop) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* Table of all semantic fns.  */

static const struct sem_fn_desc sem_fns[] = {
  { FRVBF_INSN_X_INVALID, SEM_FN_NAME (frvbf,x_invalid) },
  { FRVBF_INSN_X_AFTER, SEM_FN_NAME (frvbf,x_after) },
  { FRVBF_INSN_X_BEFORE, SEM_FN_NAME (frvbf,x_before) },
  { FRVBF_INSN_X_CTI_CHAIN, SEM_FN_NAME (frvbf,x_cti_chain) },
  { FRVBF_INSN_X_CHAIN, SEM_FN_NAME (frvbf,x_chain) },
  { FRVBF_INSN_X_BEGIN, SEM_FN_NAME (frvbf,x_begin) },
  { FRVBF_INSN_ADD, SEM_FN_NAME (frvbf,add) },
  { FRVBF_INSN_SUB, SEM_FN_NAME (frvbf,sub) },
  { FRVBF_INSN_AND, SEM_FN_NAME (frvbf,and) },
  { FRVBF_INSN_OR, SEM_FN_NAME (frvbf,or) },
  { FRVBF_INSN_XOR, SEM_FN_NAME (frvbf,xor) },
  { FRVBF_INSN_NOT, SEM_FN_NAME (frvbf,not) },
  { FRVBF_INSN_SDIV, SEM_FN_NAME (frvbf,sdiv) },
  { FRVBF_INSN_NSDIV, SEM_FN_NAME (frvbf,nsdiv) },
  { FRVBF_INSN_UDIV, SEM_FN_NAME (frvbf,udiv) },
  { FRVBF_INSN_NUDIV, SEM_FN_NAME (frvbf,nudiv) },
  { FRVBF_INSN_SMUL, SEM_FN_NAME (frvbf,smul) },
  { FRVBF_INSN_UMUL, SEM_FN_NAME (frvbf,umul) },
  { FRVBF_INSN_SMU, SEM_FN_NAME (frvbf,smu) },
  { FRVBF_INSN_SMASS, SEM_FN_NAME (frvbf,smass) },
  { FRVBF_INSN_SMSSS, SEM_FN_NAME (frvbf,smsss) },
  { FRVBF_INSN_SLL, SEM_FN_NAME (frvbf,sll) },
  { FRVBF_INSN_SRL, SEM_FN_NAME (frvbf,srl) },
  { FRVBF_INSN_SRA, SEM_FN_NAME (frvbf,sra) },
  { FRVBF_INSN_SLASS, SEM_FN_NAME (frvbf,slass) },
  { FRVBF_INSN_SCUTSS, SEM_FN_NAME (frvbf,scutss) },
  { FRVBF_INSN_SCAN, SEM_FN_NAME (frvbf,scan) },
  { FRVBF_INSN_CADD, SEM_FN_NAME (frvbf,cadd) },
  { FRVBF_INSN_CSUB, SEM_FN_NAME (frvbf,csub) },
  { FRVBF_INSN_CAND, SEM_FN_NAME (frvbf,cand) },
  { FRVBF_INSN_COR, SEM_FN_NAME (frvbf,cor) },
  { FRVBF_INSN_CXOR, SEM_FN_NAME (frvbf,cxor) },
  { FRVBF_INSN_CNOT, SEM_FN_NAME (frvbf,cnot) },
  { FRVBF_INSN_CSMUL, SEM_FN_NAME (frvbf,csmul) },
  { FRVBF_INSN_CSDIV, SEM_FN_NAME (frvbf,csdiv) },
  { FRVBF_INSN_CUDIV, SEM_FN_NAME (frvbf,cudiv) },
  { FRVBF_INSN_CSLL, SEM_FN_NAME (frvbf,csll) },
  { FRVBF_INSN_CSRL, SEM_FN_NAME (frvbf,csrl) },
  { FRVBF_INSN_CSRA, SEM_FN_NAME (frvbf,csra) },
  { FRVBF_INSN_CSCAN, SEM_FN_NAME (frvbf,cscan) },
  { FRVBF_INSN_ADDCC, SEM_FN_NAME (frvbf,addcc) },
  { FRVBF_INSN_SUBCC, SEM_FN_NAME (frvbf,subcc) },
  { FRVBF_INSN_ANDCC, SEM_FN_NAME (frvbf,andcc) },
  { FRVBF_INSN_ORCC, SEM_FN_NAME (frvbf,orcc) },
  { FRVBF_INSN_XORCC, SEM_FN_NAME (frvbf,xorcc) },
  { FRVBF_INSN_SLLCC, SEM_FN_NAME (frvbf,sllcc) },
  { FRVBF_INSN_SRLCC, SEM_FN_NAME (frvbf,srlcc) },
  { FRVBF_INSN_SRACC, SEM_FN_NAME (frvbf,sracc) },
  { FRVBF_INSN_SMULCC, SEM_FN_NAME (frvbf,smulcc) },
  { FRVBF_INSN_UMULCC, SEM_FN_NAME (frvbf,umulcc) },
  { FRVBF_INSN_CADDCC, SEM_FN_NAME (frvbf,caddcc) },
  { FRVBF_INSN_CSUBCC, SEM_FN_NAME (frvbf,csubcc) },
  { FRVBF_INSN_CSMULCC, SEM_FN_NAME (frvbf,csmulcc) },
  { FRVBF_INSN_CANDCC, SEM_FN_NAME (frvbf,candcc) },
  { FRVBF_INSN_CORCC, SEM_FN_NAME (frvbf,corcc) },
  { FRVBF_INSN_CXORCC, SEM_FN_NAME (frvbf,cxorcc) },
  { FRVBF_INSN_CSLLCC, SEM_FN_NAME (frvbf,csllcc) },
  { FRVBF_INSN_CSRLCC, SEM_FN_NAME (frvbf,csrlcc) },
  { FRVBF_INSN_CSRACC, SEM_FN_NAME (frvbf,csracc) },
  { FRVBF_INSN_ADDX, SEM_FN_NAME (frvbf,addx) },
  { FRVBF_INSN_SUBX, SEM_FN_NAME (frvbf,subx) },
  { FRVBF_INSN_ADDXCC, SEM_FN_NAME (frvbf,addxcc) },
  { FRVBF_INSN_SUBXCC, SEM_FN_NAME (frvbf,subxcc) },
  { FRVBF_INSN_ADDSS, SEM_FN_NAME (frvbf,addss) },
  { FRVBF_INSN_SUBSS, SEM_FN_NAME (frvbf,subss) },
  { FRVBF_INSN_ADDI, SEM_FN_NAME (frvbf,addi) },
  { FRVBF_INSN_SUBI, SEM_FN_NAME (frvbf,subi) },
  { FRVBF_INSN_ANDI, SEM_FN_NAME (frvbf,andi) },
  { FRVBF_INSN_ORI, SEM_FN_NAME (frvbf,ori) },
  { FRVBF_INSN_XORI, SEM_FN_NAME (frvbf,xori) },
  { FRVBF_INSN_SDIVI, SEM_FN_NAME (frvbf,sdivi) },
  { FRVBF_INSN_NSDIVI, SEM_FN_NAME (frvbf,nsdivi) },
  { FRVBF_INSN_UDIVI, SEM_FN_NAME (frvbf,udivi) },
  { FRVBF_INSN_NUDIVI, SEM_FN_NAME (frvbf,nudivi) },
  { FRVBF_INSN_SMULI, SEM_FN_NAME (frvbf,smuli) },
  { FRVBF_INSN_UMULI, SEM_FN_NAME (frvbf,umuli) },
  { FRVBF_INSN_SLLI, SEM_FN_NAME (frvbf,slli) },
  { FRVBF_INSN_SRLI, SEM_FN_NAME (frvbf,srli) },
  { FRVBF_INSN_SRAI, SEM_FN_NAME (frvbf,srai) },
  { FRVBF_INSN_SCANI, SEM_FN_NAME (frvbf,scani) },
  { FRVBF_INSN_ADDICC, SEM_FN_NAME (frvbf,addicc) },
  { FRVBF_INSN_SUBICC, SEM_FN_NAME (frvbf,subicc) },
  { FRVBF_INSN_ANDICC, SEM_FN_NAME (frvbf,andicc) },
  { FRVBF_INSN_ORICC, SEM_FN_NAME (frvbf,oricc) },
  { FRVBF_INSN_XORICC, SEM_FN_NAME (frvbf,xoricc) },
  { FRVBF_INSN_SMULICC, SEM_FN_NAME (frvbf,smulicc) },
  { FRVBF_INSN_UMULICC, SEM_FN_NAME (frvbf,umulicc) },
  { FRVBF_INSN_SLLICC, SEM_FN_NAME (frvbf,sllicc) },
  { FRVBF_INSN_SRLICC, SEM_FN_NAME (frvbf,srlicc) },
  { FRVBF_INSN_SRAICC, SEM_FN_NAME (frvbf,sraicc) },
  { FRVBF_INSN_ADDXI, SEM_FN_NAME (frvbf,addxi) },
  { FRVBF_INSN_SUBXI, SEM_FN_NAME (frvbf,subxi) },
  { FRVBF_INSN_ADDXICC, SEM_FN_NAME (frvbf,addxicc) },
  { FRVBF_INSN_SUBXICC, SEM_FN_NAME (frvbf,subxicc) },
  { FRVBF_INSN_CMPB, SEM_FN_NAME (frvbf,cmpb) },
  { FRVBF_INSN_CMPBA, SEM_FN_NAME (frvbf,cmpba) },
  { FRVBF_INSN_SETLO, SEM_FN_NAME (frvbf,setlo) },
  { FRVBF_INSN_SETHI, SEM_FN_NAME (frvbf,sethi) },
  { FRVBF_INSN_SETLOS, SEM_FN_NAME (frvbf,setlos) },
  { FRVBF_INSN_LDSB, SEM_FN_NAME (frvbf,ldsb) },
  { FRVBF_INSN_LDUB, SEM_FN_NAME (frvbf,ldub) },
  { FRVBF_INSN_LDSH, SEM_FN_NAME (frvbf,ldsh) },
  { FRVBF_INSN_LDUH, SEM_FN_NAME (frvbf,lduh) },
  { FRVBF_INSN_LD, SEM_FN_NAME (frvbf,ld) },
  { FRVBF_INSN_LDBF, SEM_FN_NAME (frvbf,ldbf) },
  { FRVBF_INSN_LDHF, SEM_FN_NAME (frvbf,ldhf) },
  { FRVBF_INSN_LDF, SEM_FN_NAME (frvbf,ldf) },
  { FRVBF_INSN_LDC, SEM_FN_NAME (frvbf,ldc) },
  { FRVBF_INSN_NLDSB, SEM_FN_NAME (frvbf,nldsb) },
  { FRVBF_INSN_NLDUB, SEM_FN_NAME (frvbf,nldub) },
  { FRVBF_INSN_NLDSH, SEM_FN_NAME (frvbf,nldsh) },
  { FRVBF_INSN_NLDUH, SEM_FN_NAME (frvbf,nlduh) },
  { FRVBF_INSN_NLD, SEM_FN_NAME (frvbf,nld) },
  { FRVBF_INSN_NLDBF, SEM_FN_NAME (frvbf,nldbf) },
  { FRVBF_INSN_NLDHF, SEM_FN_NAME (frvbf,nldhf) },
  { FRVBF_INSN_NLDF, SEM_FN_NAME (frvbf,nldf) },
  { FRVBF_INSN_LDD, SEM_FN_NAME (frvbf,ldd) },
  { FRVBF_INSN_LDDF, SEM_FN_NAME (frvbf,lddf) },
  { FRVBF_INSN_LDDC, SEM_FN_NAME (frvbf,lddc) },
  { FRVBF_INSN_NLDD, SEM_FN_NAME (frvbf,nldd) },
  { FRVBF_INSN_NLDDF, SEM_FN_NAME (frvbf,nlddf) },
  { FRVBF_INSN_LDQ, SEM_FN_NAME (frvbf,ldq) },
  { FRVBF_INSN_LDQF, SEM_FN_NAME (frvbf,ldqf) },
  { FRVBF_INSN_LDQC, SEM_FN_NAME (frvbf,ldqc) },
  { FRVBF_INSN_NLDQ, SEM_FN_NAME (frvbf,nldq) },
  { FRVBF_INSN_NLDQF, SEM_FN_NAME (frvbf,nldqf) },
  { FRVBF_INSN_LDSBU, SEM_FN_NAME (frvbf,ldsbu) },
  { FRVBF_INSN_LDUBU, SEM_FN_NAME (frvbf,ldubu) },
  { FRVBF_INSN_LDSHU, SEM_FN_NAME (frvbf,ldshu) },
  { FRVBF_INSN_LDUHU, SEM_FN_NAME (frvbf,lduhu) },
  { FRVBF_INSN_LDU, SEM_FN_NAME (frvbf,ldu) },
  { FRVBF_INSN_NLDSBU, SEM_FN_NAME (frvbf,nldsbu) },
  { FRVBF_INSN_NLDUBU, SEM_FN_NAME (frvbf,nldubu) },
  { FRVBF_INSN_NLDSHU, SEM_FN_NAME (frvbf,nldshu) },
  { FRVBF_INSN_NLDUHU, SEM_FN_NAME (frvbf,nlduhu) },
  { FRVBF_INSN_NLDU, SEM_FN_NAME (frvbf,nldu) },
  { FRVBF_INSN_LDBFU, SEM_FN_NAME (frvbf,ldbfu) },
  { FRVBF_INSN_LDHFU, SEM_FN_NAME (frvbf,ldhfu) },
  { FRVBF_INSN_LDFU, SEM_FN_NAME (frvbf,ldfu) },
  { FRVBF_INSN_LDCU, SEM_FN_NAME (frvbf,ldcu) },
  { FRVBF_INSN_NLDBFU, SEM_FN_NAME (frvbf,nldbfu) },
  { FRVBF_INSN_NLDHFU, SEM_FN_NAME (frvbf,nldhfu) },
  { FRVBF_INSN_NLDFU, SEM_FN_NAME (frvbf,nldfu) },
  { FRVBF_INSN_LDDU, SEM_FN_NAME (frvbf,lddu) },
  { FRVBF_INSN_NLDDU, SEM_FN_NAME (frvbf,nlddu) },
  { FRVBF_INSN_LDDFU, SEM_FN_NAME (frvbf,lddfu) },
  { FRVBF_INSN_LDDCU, SEM_FN_NAME (frvbf,lddcu) },
  { FRVBF_INSN_NLDDFU, SEM_FN_NAME (frvbf,nlddfu) },
  { FRVBF_INSN_LDQU, SEM_FN_NAME (frvbf,ldqu) },
  { FRVBF_INSN_NLDQU, SEM_FN_NAME (frvbf,nldqu) },
  { FRVBF_INSN_LDQFU, SEM_FN_NAME (frvbf,ldqfu) },
  { FRVBF_INSN_LDQCU, SEM_FN_NAME (frvbf,ldqcu) },
  { FRVBF_INSN_NLDQFU, SEM_FN_NAME (frvbf,nldqfu) },
  { FRVBF_INSN_LDSBI, SEM_FN_NAME (frvbf,ldsbi) },
  { FRVBF_INSN_LDSHI, SEM_FN_NAME (frvbf,ldshi) },
  { FRVBF_INSN_LDI, SEM_FN_NAME (frvbf,ldi) },
  { FRVBF_INSN_LDUBI, SEM_FN_NAME (frvbf,ldubi) },
  { FRVBF_INSN_LDUHI, SEM_FN_NAME (frvbf,lduhi) },
  { FRVBF_INSN_LDBFI, SEM_FN_NAME (frvbf,ldbfi) },
  { FRVBF_INSN_LDHFI, SEM_FN_NAME (frvbf,ldhfi) },
  { FRVBF_INSN_LDFI, SEM_FN_NAME (frvbf,ldfi) },
  { FRVBF_INSN_NLDSBI, SEM_FN_NAME (frvbf,nldsbi) },
  { FRVBF_INSN_NLDUBI, SEM_FN_NAME (frvbf,nldubi) },
  { FRVBF_INSN_NLDSHI, SEM_FN_NAME (frvbf,nldshi) },
  { FRVBF_INSN_NLDUHI, SEM_FN_NAME (frvbf,nlduhi) },
  { FRVBF_INSN_NLDI, SEM_FN_NAME (frvbf,nldi) },
  { FRVBF_INSN_NLDBFI, SEM_FN_NAME (frvbf,nldbfi) },
  { FRVBF_INSN_NLDHFI, SEM_FN_NAME (frvbf,nldhfi) },
  { FRVBF_INSN_NLDFI, SEM_FN_NAME (frvbf,nldfi) },
  { FRVBF_INSN_LDDI, SEM_FN_NAME (frvbf,lddi) },
  { FRVBF_INSN_LDDFI, SEM_FN_NAME (frvbf,lddfi) },
  { FRVBF_INSN_NLDDI, SEM_FN_NAME (frvbf,nlddi) },
  { FRVBF_INSN_NLDDFI, SEM_FN_NAME (frvbf,nlddfi) },
  { FRVBF_INSN_LDQI, SEM_FN_NAME (frvbf,ldqi) },
  { FRVBF_INSN_LDQFI, SEM_FN_NAME (frvbf,ldqfi) },
  { FRVBF_INSN_NLDQFI, SEM_FN_NAME (frvbf,nldqfi) },
  { FRVBF_INSN_STB, SEM_FN_NAME (frvbf,stb) },
  { FRVBF_INSN_STH, SEM_FN_NAME (frvbf,sth) },
  { FRVBF_INSN_ST, SEM_FN_NAME (frvbf,st) },
  { FRVBF_INSN_STBF, SEM_FN_NAME (frvbf,stbf) },
  { FRVBF_INSN_STHF, SEM_FN_NAME (frvbf,sthf) },
  { FRVBF_INSN_STF, SEM_FN_NAME (frvbf,stf) },
  { FRVBF_INSN_STC, SEM_FN_NAME (frvbf,stc) },
  { FRVBF_INSN_STD, SEM_FN_NAME (frvbf,std) },
  { FRVBF_INSN_STDF, SEM_FN_NAME (frvbf,stdf) },
  { FRVBF_INSN_STDC, SEM_FN_NAME (frvbf,stdc) },
  { FRVBF_INSN_STQ, SEM_FN_NAME (frvbf,stq) },
  { FRVBF_INSN_STQF, SEM_FN_NAME (frvbf,stqf) },
  { FRVBF_INSN_STQC, SEM_FN_NAME (frvbf,stqc) },
  { FRVBF_INSN_STBU, SEM_FN_NAME (frvbf,stbu) },
  { FRVBF_INSN_STHU, SEM_FN_NAME (frvbf,sthu) },
  { FRVBF_INSN_STU, SEM_FN_NAME (frvbf,stu) },
  { FRVBF_INSN_STBFU, SEM_FN_NAME (frvbf,stbfu) },
  { FRVBF_INSN_STHFU, SEM_FN_NAME (frvbf,sthfu) },
  { FRVBF_INSN_STFU, SEM_FN_NAME (frvbf,stfu) },
  { FRVBF_INSN_STCU, SEM_FN_NAME (frvbf,stcu) },
  { FRVBF_INSN_STDU, SEM_FN_NAME (frvbf,stdu) },
  { FRVBF_INSN_STDFU, SEM_FN_NAME (frvbf,stdfu) },
  { FRVBF_INSN_STDCU, SEM_FN_NAME (frvbf,stdcu) },
  { FRVBF_INSN_STQU, SEM_FN_NAME (frvbf,stqu) },
  { FRVBF_INSN_STQFU, SEM_FN_NAME (frvbf,stqfu) },
  { FRVBF_INSN_STQCU, SEM_FN_NAME (frvbf,stqcu) },
  { FRVBF_INSN_CLDSB, SEM_FN_NAME (frvbf,cldsb) },
  { FRVBF_INSN_CLDUB, SEM_FN_NAME (frvbf,cldub) },
  { FRVBF_INSN_CLDSH, SEM_FN_NAME (frvbf,cldsh) },
  { FRVBF_INSN_CLDUH, SEM_FN_NAME (frvbf,clduh) },
  { FRVBF_INSN_CLD, SEM_FN_NAME (frvbf,cld) },
  { FRVBF_INSN_CLDBF, SEM_FN_NAME (frvbf,cldbf) },
  { FRVBF_INSN_CLDHF, SEM_FN_NAME (frvbf,cldhf) },
  { FRVBF_INSN_CLDF, SEM_FN_NAME (frvbf,cldf) },
  { FRVBF_INSN_CLDD, SEM_FN_NAME (frvbf,cldd) },
  { FRVBF_INSN_CLDDF, SEM_FN_NAME (frvbf,clddf) },
  { FRVBF_INSN_CLDQ, SEM_FN_NAME (frvbf,cldq) },
  { FRVBF_INSN_CLDSBU, SEM_FN_NAME (frvbf,cldsbu) },
  { FRVBF_INSN_CLDUBU, SEM_FN_NAME (frvbf,cldubu) },
  { FRVBF_INSN_CLDSHU, SEM_FN_NAME (frvbf,cldshu) },
  { FRVBF_INSN_CLDUHU, SEM_FN_NAME (frvbf,clduhu) },
  { FRVBF_INSN_CLDU, SEM_FN_NAME (frvbf,cldu) },
  { FRVBF_INSN_CLDBFU, SEM_FN_NAME (frvbf,cldbfu) },
  { FRVBF_INSN_CLDHFU, SEM_FN_NAME (frvbf,cldhfu) },
  { FRVBF_INSN_CLDFU, SEM_FN_NAME (frvbf,cldfu) },
  { FRVBF_INSN_CLDDU, SEM_FN_NAME (frvbf,clddu) },
  { FRVBF_INSN_CLDDFU, SEM_FN_NAME (frvbf,clddfu) },
  { FRVBF_INSN_CLDQU, SEM_FN_NAME (frvbf,cldqu) },
  { FRVBF_INSN_CSTB, SEM_FN_NAME (frvbf,cstb) },
  { FRVBF_INSN_CSTH, SEM_FN_NAME (frvbf,csth) },
  { FRVBF_INSN_CST, SEM_FN_NAME (frvbf,cst) },
  { FRVBF_INSN_CSTBF, SEM_FN_NAME (frvbf,cstbf) },
  { FRVBF_INSN_CSTHF, SEM_FN_NAME (frvbf,csthf) },
  { FRVBF_INSN_CSTF, SEM_FN_NAME (frvbf,cstf) },
  { FRVBF_INSN_CSTD, SEM_FN_NAME (frvbf,cstd) },
  { FRVBF_INSN_CSTDF, SEM_FN_NAME (frvbf,cstdf) },
  { FRVBF_INSN_CSTQ, SEM_FN_NAME (frvbf,cstq) },
  { FRVBF_INSN_CSTBU, SEM_FN_NAME (frvbf,cstbu) },
  { FRVBF_INSN_CSTHU, SEM_FN_NAME (frvbf,csthu) },
  { FRVBF_INSN_CSTU, SEM_FN_NAME (frvbf,cstu) },
  { FRVBF_INSN_CSTBFU, SEM_FN_NAME (frvbf,cstbfu) },
  { FRVBF_INSN_CSTHFU, SEM_FN_NAME (frvbf,csthfu) },
  { FRVBF_INSN_CSTFU, SEM_FN_NAME (frvbf,cstfu) },
  { FRVBF_INSN_CSTDU, SEM_FN_NAME (frvbf,cstdu) },
  { FRVBF_INSN_CSTDFU, SEM_FN_NAME (frvbf,cstdfu) },
  { FRVBF_INSN_STBI, SEM_FN_NAME (frvbf,stbi) },
  { FRVBF_INSN_STHI, SEM_FN_NAME (frvbf,sthi) },
  { FRVBF_INSN_STI, SEM_FN_NAME (frvbf,sti) },
  { FRVBF_INSN_STBFI, SEM_FN_NAME (frvbf,stbfi) },
  { FRVBF_INSN_STHFI, SEM_FN_NAME (frvbf,sthfi) },
  { FRVBF_INSN_STFI, SEM_FN_NAME (frvbf,stfi) },
  { FRVBF_INSN_STDI, SEM_FN_NAME (frvbf,stdi) },
  { FRVBF_INSN_STDFI, SEM_FN_NAME (frvbf,stdfi) },
  { FRVBF_INSN_STQI, SEM_FN_NAME (frvbf,stqi) },
  { FRVBF_INSN_STQFI, SEM_FN_NAME (frvbf,stqfi) },
  { FRVBF_INSN_SWAP, SEM_FN_NAME (frvbf,swap) },
  { FRVBF_INSN_SWAPI, SEM_FN_NAME (frvbf,swapi) },
  { FRVBF_INSN_CSWAP, SEM_FN_NAME (frvbf,cswap) },
  { FRVBF_INSN_MOVGF, SEM_FN_NAME (frvbf,movgf) },
  { FRVBF_INSN_MOVFG, SEM_FN_NAME (frvbf,movfg) },
  { FRVBF_INSN_MOVGFD, SEM_FN_NAME (frvbf,movgfd) },
  { FRVBF_INSN_MOVFGD, SEM_FN_NAME (frvbf,movfgd) },
  { FRVBF_INSN_MOVGFQ, SEM_FN_NAME (frvbf,movgfq) },
  { FRVBF_INSN_MOVFGQ, SEM_FN_NAME (frvbf,movfgq) },
  { FRVBF_INSN_CMOVGF, SEM_FN_NAME (frvbf,cmovgf) },
  { FRVBF_INSN_CMOVFG, SEM_FN_NAME (frvbf,cmovfg) },
  { FRVBF_INSN_CMOVGFD, SEM_FN_NAME (frvbf,cmovgfd) },
  { FRVBF_INSN_CMOVFGD, SEM_FN_NAME (frvbf,cmovfgd) },
  { FRVBF_INSN_MOVGS, SEM_FN_NAME (frvbf,movgs) },
  { FRVBF_INSN_MOVSG, SEM_FN_NAME (frvbf,movsg) },
  { FRVBF_INSN_BRA, SEM_FN_NAME (frvbf,bra) },
  { FRVBF_INSN_BNO, SEM_FN_NAME (frvbf,bno) },
  { FRVBF_INSN_BEQ, SEM_FN_NAME (frvbf,beq) },
  { FRVBF_INSN_BNE, SEM_FN_NAME (frvbf,bne) },
  { FRVBF_INSN_BLE, SEM_FN_NAME (frvbf,ble) },
  { FRVBF_INSN_BGT, SEM_FN_NAME (frvbf,bgt) },
  { FRVBF_INSN_BLT, SEM_FN_NAME (frvbf,blt) },
  { FRVBF_INSN_BGE, SEM_FN_NAME (frvbf,bge) },
  { FRVBF_INSN_BLS, SEM_FN_NAME (frvbf,bls) },
  { FRVBF_INSN_BHI, SEM_FN_NAME (frvbf,bhi) },
  { FRVBF_INSN_BC, SEM_FN_NAME (frvbf,bc) },
  { FRVBF_INSN_BNC, SEM_FN_NAME (frvbf,bnc) },
  { FRVBF_INSN_BN, SEM_FN_NAME (frvbf,bn) },
  { FRVBF_INSN_BP, SEM_FN_NAME (frvbf,bp) },
  { FRVBF_INSN_BV, SEM_FN_NAME (frvbf,bv) },
  { FRVBF_INSN_BNV, SEM_FN_NAME (frvbf,bnv) },
  { FRVBF_INSN_FBRA, SEM_FN_NAME (frvbf,fbra) },
  { FRVBF_INSN_FBNO, SEM_FN_NAME (frvbf,fbno) },
  { FRVBF_INSN_FBNE, SEM_FN_NAME (frvbf,fbne) },
  { FRVBF_INSN_FBEQ, SEM_FN_NAME (frvbf,fbeq) },
  { FRVBF_INSN_FBLG, SEM_FN_NAME (frvbf,fblg) },
  { FRVBF_INSN_FBUE, SEM_FN_NAME (frvbf,fbue) },
  { FRVBF_INSN_FBUL, SEM_FN_NAME (frvbf,fbul) },
  { FRVBF_INSN_FBGE, SEM_FN_NAME (frvbf,fbge) },
  { FRVBF_INSN_FBLT, SEM_FN_NAME (frvbf,fblt) },
  { FRVBF_INSN_FBUGE, SEM_FN_NAME (frvbf,fbuge) },
  { FRVBF_INSN_FBUG, SEM_FN_NAME (frvbf,fbug) },
  { FRVBF_INSN_FBLE, SEM_FN_NAME (frvbf,fble) },
  { FRVBF_INSN_FBGT, SEM_FN_NAME (frvbf,fbgt) },
  { FRVBF_INSN_FBULE, SEM_FN_NAME (frvbf,fbule) },
  { FRVBF_INSN_FBU, SEM_FN_NAME (frvbf,fbu) },
  { FRVBF_INSN_FBO, SEM_FN_NAME (frvbf,fbo) },
  { FRVBF_INSN_BCTRLR, SEM_FN_NAME (frvbf,bctrlr) },
  { FRVBF_INSN_BRALR, SEM_FN_NAME (frvbf,bralr) },
  { FRVBF_INSN_BNOLR, SEM_FN_NAME (frvbf,bnolr) },
  { FRVBF_INSN_BEQLR, SEM_FN_NAME (frvbf,beqlr) },
  { FRVBF_INSN_BNELR, SEM_FN_NAME (frvbf,bnelr) },
  { FRVBF_INSN_BLELR, SEM_FN_NAME (frvbf,blelr) },
  { FRVBF_INSN_BGTLR, SEM_FN_NAME (frvbf,bgtlr) },
  { FRVBF_INSN_BLTLR, SEM_FN_NAME (frvbf,bltlr) },
  { FRVBF_INSN_BGELR, SEM_FN_NAME (frvbf,bgelr) },
  { FRVBF_INSN_BLSLR, SEM_FN_NAME (frvbf,blslr) },
  { FRVBF_INSN_BHILR, SEM_FN_NAME (frvbf,bhilr) },
  { FRVBF_INSN_BCLR, SEM_FN_NAME (frvbf,bclr) },
  { FRVBF_INSN_BNCLR, SEM_FN_NAME (frvbf,bnclr) },
  { FRVBF_INSN_BNLR, SEM_FN_NAME (frvbf,bnlr) },
  { FRVBF_INSN_BPLR, SEM_FN_NAME (frvbf,bplr) },
  { FRVBF_INSN_BVLR, SEM_FN_NAME (frvbf,bvlr) },
  { FRVBF_INSN_BNVLR, SEM_FN_NAME (frvbf,bnvlr) },
  { FRVBF_INSN_FBRALR, SEM_FN_NAME (frvbf,fbralr) },
  { FRVBF_INSN_FBNOLR, SEM_FN_NAME (frvbf,fbnolr) },
  { FRVBF_INSN_FBEQLR, SEM_FN_NAME (frvbf,fbeqlr) },
  { FRVBF_INSN_FBNELR, SEM_FN_NAME (frvbf,fbnelr) },
  { FRVBF_INSN_FBLGLR, SEM_FN_NAME (frvbf,fblglr) },
  { FRVBF_INSN_FBUELR, SEM_FN_NAME (frvbf,fbuelr) },
  { FRVBF_INSN_FBULLR, SEM_FN_NAME (frvbf,fbullr) },
  { FRVBF_INSN_FBGELR, SEM_FN_NAME (frvbf,fbgelr) },
  { FRVBF_INSN_FBLTLR, SEM_FN_NAME (frvbf,fbltlr) },
  { FRVBF_INSN_FBUGELR, SEM_FN_NAME (frvbf,fbugelr) },
  { FRVBF_INSN_FBUGLR, SEM_FN_NAME (frvbf,fbuglr) },
  { FRVBF_INSN_FBLELR, SEM_FN_NAME (frvbf,fblelr) },
  { FRVBF_INSN_FBGTLR, SEM_FN_NAME (frvbf,fbgtlr) },
  { FRVBF_INSN_FBULELR, SEM_FN_NAME (frvbf,fbulelr) },
  { FRVBF_INSN_FBULR, SEM_FN_NAME (frvbf,fbulr) },
  { FRVBF_INSN_FBOLR, SEM_FN_NAME (frvbf,fbolr) },
  { FRVBF_INSN_BCRALR, SEM_FN_NAME (frvbf,bcralr) },
  { FRVBF_INSN_BCNOLR, SEM_FN_NAME (frvbf,bcnolr) },
  { FRVBF_INSN_BCEQLR, SEM_FN_NAME (frvbf,bceqlr) },
  { FRVBF_INSN_BCNELR, SEM_FN_NAME (frvbf,bcnelr) },
  { FRVBF_INSN_BCLELR, SEM_FN_NAME (frvbf,bclelr) },
  { FRVBF_INSN_BCGTLR, SEM_FN_NAME (frvbf,bcgtlr) },
  { FRVBF_INSN_BCLTLR, SEM_FN_NAME (frvbf,bcltlr) },
  { FRVBF_INSN_BCGELR, SEM_FN_NAME (frvbf,bcgelr) },
  { FRVBF_INSN_BCLSLR, SEM_FN_NAME (frvbf,bclslr) },
  { FRVBF_INSN_BCHILR, SEM_FN_NAME (frvbf,bchilr) },
  { FRVBF_INSN_BCCLR, SEM_FN_NAME (frvbf,bcclr) },
  { FRVBF_INSN_BCNCLR, SEM_FN_NAME (frvbf,bcnclr) },
  { FRVBF_INSN_BCNLR, SEM_FN_NAME (frvbf,bcnlr) },
  { FRVBF_INSN_BCPLR, SEM_FN_NAME (frvbf,bcplr) },
  { FRVBF_INSN_BCVLR, SEM_FN_NAME (frvbf,bcvlr) },
  { FRVBF_INSN_BCNVLR, SEM_FN_NAME (frvbf,bcnvlr) },
  { FRVBF_INSN_FCBRALR, SEM_FN_NAME (frvbf,fcbralr) },
  { FRVBF_INSN_FCBNOLR, SEM_FN_NAME (frvbf,fcbnolr) },
  { FRVBF_INSN_FCBEQLR, SEM_FN_NAME (frvbf,fcbeqlr) },
  { FRVBF_INSN_FCBNELR, SEM_FN_NAME (frvbf,fcbnelr) },
  { FRVBF_INSN_FCBLGLR, SEM_FN_NAME (frvbf,fcblglr) },
  { FRVBF_INSN_FCBUELR, SEM_FN_NAME (frvbf,fcbuelr) },
  { FRVBF_INSN_FCBULLR, SEM_FN_NAME (frvbf,fcbullr) },
  { FRVBF_INSN_FCBGELR, SEM_FN_NAME (frvbf,fcbgelr) },
  { FRVBF_INSN_FCBLTLR, SEM_FN_NAME (frvbf,fcbltlr) },
  { FRVBF_INSN_FCBUGELR, SEM_FN_NAME (frvbf,fcbugelr) },
  { FRVBF_INSN_FCBUGLR, SEM_FN_NAME (frvbf,fcbuglr) },
  { FRVBF_INSN_FCBLELR, SEM_FN_NAME (frvbf,fcblelr) },
  { FRVBF_INSN_FCBGTLR, SEM_FN_NAME (frvbf,fcbgtlr) },
  { FRVBF_INSN_FCBULELR, SEM_FN_NAME (frvbf,fcbulelr) },
  { FRVBF_INSN_FCBULR, SEM_FN_NAME (frvbf,fcbulr) },
  { FRVBF_INSN_FCBOLR, SEM_FN_NAME (frvbf,fcbolr) },
  { FRVBF_INSN_JMPL, SEM_FN_NAME (frvbf,jmpl) },
  { FRVBF_INSN_CALLL, SEM_FN_NAME (frvbf,calll) },
  { FRVBF_INSN_JMPIL, SEM_FN_NAME (frvbf,jmpil) },
  { FRVBF_INSN_CALLIL, SEM_FN_NAME (frvbf,callil) },
  { FRVBF_INSN_CALL, SEM_FN_NAME (frvbf,call) },
  { FRVBF_INSN_RETT, SEM_FN_NAME (frvbf,rett) },
  { FRVBF_INSN_REI, SEM_FN_NAME (frvbf,rei) },
  { FRVBF_INSN_TRA, SEM_FN_NAME (frvbf,tra) },
  { FRVBF_INSN_TNO, SEM_FN_NAME (frvbf,tno) },
  { FRVBF_INSN_TEQ, SEM_FN_NAME (frvbf,teq) },
  { FRVBF_INSN_TNE, SEM_FN_NAME (frvbf,tne) },
  { FRVBF_INSN_TLE, SEM_FN_NAME (frvbf,tle) },
  { FRVBF_INSN_TGT, SEM_FN_NAME (frvbf,tgt) },
  { FRVBF_INSN_TLT, SEM_FN_NAME (frvbf,tlt) },
  { FRVBF_INSN_TGE, SEM_FN_NAME (frvbf,tge) },
  { FRVBF_INSN_TLS, SEM_FN_NAME (frvbf,tls) },
  { FRVBF_INSN_THI, SEM_FN_NAME (frvbf,thi) },
  { FRVBF_INSN_TC, SEM_FN_NAME (frvbf,tc) },
  { FRVBF_INSN_TNC, SEM_FN_NAME (frvbf,tnc) },
  { FRVBF_INSN_TN, SEM_FN_NAME (frvbf,tn) },
  { FRVBF_INSN_TP, SEM_FN_NAME (frvbf,tp) },
  { FRVBF_INSN_TV, SEM_FN_NAME (frvbf,tv) },
  { FRVBF_INSN_TNV, SEM_FN_NAME (frvbf,tnv) },
  { FRVBF_INSN_FTRA, SEM_FN_NAME (frvbf,ftra) },
  { FRVBF_INSN_FTNO, SEM_FN_NAME (frvbf,ftno) },
  { FRVBF_INSN_FTNE, SEM_FN_NAME (frvbf,ftne) },
  { FRVBF_INSN_FTEQ, SEM_FN_NAME (frvbf,fteq) },
  { FRVBF_INSN_FTLG, SEM_FN_NAME (frvbf,ftlg) },
  { FRVBF_INSN_FTUE, SEM_FN_NAME (frvbf,ftue) },
  { FRVBF_INSN_FTUL, SEM_FN_NAME (frvbf,ftul) },
  { FRVBF_INSN_FTGE, SEM_FN_NAME (frvbf,ftge) },
  { FRVBF_INSN_FTLT, SEM_FN_NAME (frvbf,ftlt) },
  { FRVBF_INSN_FTUGE, SEM_FN_NAME (frvbf,ftuge) },
  { FRVBF_INSN_FTUG, SEM_FN_NAME (frvbf,ftug) },
  { FRVBF_INSN_FTLE, SEM_FN_NAME (frvbf,ftle) },
  { FRVBF_INSN_FTGT, SEM_FN_NAME (frvbf,ftgt) },
  { FRVBF_INSN_FTULE, SEM_FN_NAME (frvbf,ftule) },
  { FRVBF_INSN_FTU, SEM_FN_NAME (frvbf,ftu) },
  { FRVBF_INSN_FTO, SEM_FN_NAME (frvbf,fto) },
  { FRVBF_INSN_TIRA, SEM_FN_NAME (frvbf,tira) },
  { FRVBF_INSN_TINO, SEM_FN_NAME (frvbf,tino) },
  { FRVBF_INSN_TIEQ, SEM_FN_NAME (frvbf,tieq) },
  { FRVBF_INSN_TINE, SEM_FN_NAME (frvbf,tine) },
  { FRVBF_INSN_TILE, SEM_FN_NAME (frvbf,tile) },
  { FRVBF_INSN_TIGT, SEM_FN_NAME (frvbf,tigt) },
  { FRVBF_INSN_TILT, SEM_FN_NAME (frvbf,tilt) },
  { FRVBF_INSN_TIGE, SEM_FN_NAME (frvbf,tige) },
  { FRVBF_INSN_TILS, SEM_FN_NAME (frvbf,tils) },
  { FRVBF_INSN_TIHI, SEM_FN_NAME (frvbf,tihi) },
  { FRVBF_INSN_TIC, SEM_FN_NAME (frvbf,tic) },
  { FRVBF_INSN_TINC, SEM_FN_NAME (frvbf,tinc) },
  { FRVBF_INSN_TIN, SEM_FN_NAME (frvbf,tin) },
  { FRVBF_INSN_TIP, SEM_FN_NAME (frvbf,tip) },
  { FRVBF_INSN_TIV, SEM_FN_NAME (frvbf,tiv) },
  { FRVBF_INSN_TINV, SEM_FN_NAME (frvbf,tinv) },
  { FRVBF_INSN_FTIRA, SEM_FN_NAME (frvbf,ftira) },
  { FRVBF_INSN_FTINO, SEM_FN_NAME (frvbf,ftino) },
  { FRVBF_INSN_FTINE, SEM_FN_NAME (frvbf,ftine) },
  { FRVBF_INSN_FTIEQ, SEM_FN_NAME (frvbf,ftieq) },
  { FRVBF_INSN_FTILG, SEM_FN_NAME (frvbf,ftilg) },
  { FRVBF_INSN_FTIUE, SEM_FN_NAME (frvbf,ftiue) },
  { FRVBF_INSN_FTIUL, SEM_FN_NAME (frvbf,ftiul) },
  { FRVBF_INSN_FTIGE, SEM_FN_NAME (frvbf,ftige) },
  { FRVBF_INSN_FTILT, SEM_FN_NAME (frvbf,ftilt) },
  { FRVBF_INSN_FTIUGE, SEM_FN_NAME (frvbf,ftiuge) },
  { FRVBF_INSN_FTIUG, SEM_FN_NAME (frvbf,ftiug) },
  { FRVBF_INSN_FTILE, SEM_FN_NAME (frvbf,ftile) },
  { FRVBF_INSN_FTIGT, SEM_FN_NAME (frvbf,ftigt) },
  { FRVBF_INSN_FTIULE, SEM_FN_NAME (frvbf,ftiule) },
  { FRVBF_INSN_FTIU, SEM_FN_NAME (frvbf,ftiu) },
  { FRVBF_INSN_FTIO, SEM_FN_NAME (frvbf,ftio) },
  { FRVBF_INSN_BREAK, SEM_FN_NAME (frvbf,break) },
  { FRVBF_INSN_MTRAP, SEM_FN_NAME (frvbf,mtrap) },
  { FRVBF_INSN_ANDCR, SEM_FN_NAME (frvbf,andcr) },
  { FRVBF_INSN_ORCR, SEM_FN_NAME (frvbf,orcr) },
  { FRVBF_INSN_XORCR, SEM_FN_NAME (frvbf,xorcr) },
  { FRVBF_INSN_NANDCR, SEM_FN_NAME (frvbf,nandcr) },
  { FRVBF_INSN_NORCR, SEM_FN_NAME (frvbf,norcr) },
  { FRVBF_INSN_ANDNCR, SEM_FN_NAME (frvbf,andncr) },
  { FRVBF_INSN_ORNCR, SEM_FN_NAME (frvbf,orncr) },
  { FRVBF_INSN_NANDNCR, SEM_FN_NAME (frvbf,nandncr) },
  { FRVBF_INSN_NORNCR, SEM_FN_NAME (frvbf,norncr) },
  { FRVBF_INSN_NOTCR, SEM_FN_NAME (frvbf,notcr) },
  { FRVBF_INSN_CKRA, SEM_FN_NAME (frvbf,ckra) },
  { FRVBF_INSN_CKNO, SEM_FN_NAME (frvbf,ckno) },
  { FRVBF_INSN_CKEQ, SEM_FN_NAME (frvbf,ckeq) },
  { FRVBF_INSN_CKNE, SEM_FN_NAME (frvbf,ckne) },
  { FRVBF_INSN_CKLE, SEM_FN_NAME (frvbf,ckle) },
  { FRVBF_INSN_CKGT, SEM_FN_NAME (frvbf,ckgt) },
  { FRVBF_INSN_CKLT, SEM_FN_NAME (frvbf,cklt) },
  { FRVBF_INSN_CKGE, SEM_FN_NAME (frvbf,ckge) },
  { FRVBF_INSN_CKLS, SEM_FN_NAME (frvbf,ckls) },
  { FRVBF_INSN_CKHI, SEM_FN_NAME (frvbf,ckhi) },
  { FRVBF_INSN_CKC, SEM_FN_NAME (frvbf,ckc) },
  { FRVBF_INSN_CKNC, SEM_FN_NAME (frvbf,cknc) },
  { FRVBF_INSN_CKN, SEM_FN_NAME (frvbf,ckn) },
  { FRVBF_INSN_CKP, SEM_FN_NAME (frvbf,ckp) },
  { FRVBF_INSN_CKV, SEM_FN_NAME (frvbf,ckv) },
  { FRVBF_INSN_CKNV, SEM_FN_NAME (frvbf,cknv) },
  { FRVBF_INSN_FCKRA, SEM_FN_NAME (frvbf,fckra) },
  { FRVBF_INSN_FCKNO, SEM_FN_NAME (frvbf,fckno) },
  { FRVBF_INSN_FCKNE, SEM_FN_NAME (frvbf,fckne) },
  { FRVBF_INSN_FCKEQ, SEM_FN_NAME (frvbf,fckeq) },
  { FRVBF_INSN_FCKLG, SEM_FN_NAME (frvbf,fcklg) },
  { FRVBF_INSN_FCKUE, SEM_FN_NAME (frvbf,fckue) },
  { FRVBF_INSN_FCKUL, SEM_FN_NAME (frvbf,fckul) },
  { FRVBF_INSN_FCKGE, SEM_FN_NAME (frvbf,fckge) },
  { FRVBF_INSN_FCKLT, SEM_FN_NAME (frvbf,fcklt) },
  { FRVBF_INSN_FCKUGE, SEM_FN_NAME (frvbf,fckuge) },
  { FRVBF_INSN_FCKUG, SEM_FN_NAME (frvbf,fckug) },
  { FRVBF_INSN_FCKLE, SEM_FN_NAME (frvbf,fckle) },
  { FRVBF_INSN_FCKGT, SEM_FN_NAME (frvbf,fckgt) },
  { FRVBF_INSN_FCKULE, SEM_FN_NAME (frvbf,fckule) },
  { FRVBF_INSN_FCKU, SEM_FN_NAME (frvbf,fcku) },
  { FRVBF_INSN_FCKO, SEM_FN_NAME (frvbf,fcko) },
  { FRVBF_INSN_CCKRA, SEM_FN_NAME (frvbf,cckra) },
  { FRVBF_INSN_CCKNO, SEM_FN_NAME (frvbf,cckno) },
  { FRVBF_INSN_CCKEQ, SEM_FN_NAME (frvbf,cckeq) },
  { FRVBF_INSN_CCKNE, SEM_FN_NAME (frvbf,cckne) },
  { FRVBF_INSN_CCKLE, SEM_FN_NAME (frvbf,cckle) },
  { FRVBF_INSN_CCKGT, SEM_FN_NAME (frvbf,cckgt) },
  { FRVBF_INSN_CCKLT, SEM_FN_NAME (frvbf,ccklt) },
  { FRVBF_INSN_CCKGE, SEM_FN_NAME (frvbf,cckge) },
  { FRVBF_INSN_CCKLS, SEM_FN_NAME (frvbf,cckls) },
  { FRVBF_INSN_CCKHI, SEM_FN_NAME (frvbf,cckhi) },
  { FRVBF_INSN_CCKC, SEM_FN_NAME (frvbf,cckc) },
  { FRVBF_INSN_CCKNC, SEM_FN_NAME (frvbf,ccknc) },
  { FRVBF_INSN_CCKN, SEM_FN_NAME (frvbf,cckn) },
  { FRVBF_INSN_CCKP, SEM_FN_NAME (frvbf,cckp) },
  { FRVBF_INSN_CCKV, SEM_FN_NAME (frvbf,cckv) },
  { FRVBF_INSN_CCKNV, SEM_FN_NAME (frvbf,ccknv) },
  { FRVBF_INSN_CFCKRA, SEM_FN_NAME (frvbf,cfckra) },
  { FRVBF_INSN_CFCKNO, SEM_FN_NAME (frvbf,cfckno) },
  { FRVBF_INSN_CFCKNE, SEM_FN_NAME (frvbf,cfckne) },
  { FRVBF_INSN_CFCKEQ, SEM_FN_NAME (frvbf,cfckeq) },
  { FRVBF_INSN_CFCKLG, SEM_FN_NAME (frvbf,cfcklg) },
  { FRVBF_INSN_CFCKUE, SEM_FN_NAME (frvbf,cfckue) },
  { FRVBF_INSN_CFCKUL, SEM_FN_NAME (frvbf,cfckul) },
  { FRVBF_INSN_CFCKGE, SEM_FN_NAME (frvbf,cfckge) },
  { FRVBF_INSN_CFCKLT, SEM_FN_NAME (frvbf,cfcklt) },
  { FRVBF_INSN_CFCKUGE, SEM_FN_NAME (frvbf,cfckuge) },
  { FRVBF_INSN_CFCKUG, SEM_FN_NAME (frvbf,cfckug) },
  { FRVBF_INSN_CFCKLE, SEM_FN_NAME (frvbf,cfckle) },
  { FRVBF_INSN_CFCKGT, SEM_FN_NAME (frvbf,cfckgt) },
  { FRVBF_INSN_CFCKULE, SEM_FN_NAME (frvbf,cfckule) },
  { FRVBF_INSN_CFCKU, SEM_FN_NAME (frvbf,cfcku) },
  { FRVBF_INSN_CFCKO, SEM_FN_NAME (frvbf,cfcko) },
  { FRVBF_INSN_CJMPL, SEM_FN_NAME (frvbf,cjmpl) },
  { FRVBF_INSN_CCALLL, SEM_FN_NAME (frvbf,ccalll) },
  { FRVBF_INSN_ICI, SEM_FN_NAME (frvbf,ici) },
  { FRVBF_INSN_DCI, SEM_FN_NAME (frvbf,dci) },
  { FRVBF_INSN_ICEI, SEM_FN_NAME (frvbf,icei) },
  { FRVBF_INSN_DCEI, SEM_FN_NAME (frvbf,dcei) },
  { FRVBF_INSN_DCF, SEM_FN_NAME (frvbf,dcf) },
  { FRVBF_INSN_DCEF, SEM_FN_NAME (frvbf,dcef) },
  { FRVBF_INSN_WITLB, SEM_FN_NAME (frvbf,witlb) },
  { FRVBF_INSN_WDTLB, SEM_FN_NAME (frvbf,wdtlb) },
  { FRVBF_INSN_ITLBI, SEM_FN_NAME (frvbf,itlbi) },
  { FRVBF_INSN_DTLBI, SEM_FN_NAME (frvbf,dtlbi) },
  { FRVBF_INSN_ICPL, SEM_FN_NAME (frvbf,icpl) },
  { FRVBF_INSN_DCPL, SEM_FN_NAME (frvbf,dcpl) },
  { FRVBF_INSN_ICUL, SEM_FN_NAME (frvbf,icul) },
  { FRVBF_INSN_DCUL, SEM_FN_NAME (frvbf,dcul) },
  { FRVBF_INSN_BAR, SEM_FN_NAME (frvbf,bar) },
  { FRVBF_INSN_MEMBAR, SEM_FN_NAME (frvbf,membar) },
  { FRVBF_INSN_LRAI, SEM_FN_NAME (frvbf,lrai) },
  { FRVBF_INSN_LRAD, SEM_FN_NAME (frvbf,lrad) },
  { FRVBF_INSN_TLBPR, SEM_FN_NAME (frvbf,tlbpr) },
  { FRVBF_INSN_COP1, SEM_FN_NAME (frvbf,cop1) },
  { FRVBF_INSN_COP2, SEM_FN_NAME (frvbf,cop2) },
  { FRVBF_INSN_CLRGR, SEM_FN_NAME (frvbf,clrgr) },
  { FRVBF_INSN_CLRFR, SEM_FN_NAME (frvbf,clrfr) },
  { FRVBF_INSN_CLRGA, SEM_FN_NAME (frvbf,clrga) },
  { FRVBF_INSN_CLRFA, SEM_FN_NAME (frvbf,clrfa) },
  { FRVBF_INSN_COMMITGR, SEM_FN_NAME (frvbf,commitgr) },
  { FRVBF_INSN_COMMITFR, SEM_FN_NAME (frvbf,commitfr) },
  { FRVBF_INSN_COMMITGA, SEM_FN_NAME (frvbf,commitga) },
  { FRVBF_INSN_COMMITFA, SEM_FN_NAME (frvbf,commitfa) },
  { FRVBF_INSN_FITOS, SEM_FN_NAME (frvbf,fitos) },
  { FRVBF_INSN_FSTOI, SEM_FN_NAME (frvbf,fstoi) },
  { FRVBF_INSN_FITOD, SEM_FN_NAME (frvbf,fitod) },
  { FRVBF_INSN_FDTOI, SEM_FN_NAME (frvbf,fdtoi) },
  { FRVBF_INSN_FDITOS, SEM_FN_NAME (frvbf,fditos) },
  { FRVBF_INSN_FDSTOI, SEM_FN_NAME (frvbf,fdstoi) },
  { FRVBF_INSN_NFDITOS, SEM_FN_NAME (frvbf,nfditos) },
  { FRVBF_INSN_NFDSTOI, SEM_FN_NAME (frvbf,nfdstoi) },
  { FRVBF_INSN_CFITOS, SEM_FN_NAME (frvbf,cfitos) },
  { FRVBF_INSN_CFSTOI, SEM_FN_NAME (frvbf,cfstoi) },
  { FRVBF_INSN_NFITOS, SEM_FN_NAME (frvbf,nfitos) },
  { FRVBF_INSN_NFSTOI, SEM_FN_NAME (frvbf,nfstoi) },
  { FRVBF_INSN_FMOVS, SEM_FN_NAME (frvbf,fmovs) },
  { FRVBF_INSN_FMOVD, SEM_FN_NAME (frvbf,fmovd) },
  { FRVBF_INSN_FDMOVS, SEM_FN_NAME (frvbf,fdmovs) },
  { FRVBF_INSN_CFMOVS, SEM_FN_NAME (frvbf,cfmovs) },
  { FRVBF_INSN_FNEGS, SEM_FN_NAME (frvbf,fnegs) },
  { FRVBF_INSN_FNEGD, SEM_FN_NAME (frvbf,fnegd) },
  { FRVBF_INSN_FDNEGS, SEM_FN_NAME (frvbf,fdnegs) },
  { FRVBF_INSN_CFNEGS, SEM_FN_NAME (frvbf,cfnegs) },
  { FRVBF_INSN_FABSS, SEM_FN_NAME (frvbf,fabss) },
  { FRVBF_INSN_FABSD, SEM_FN_NAME (frvbf,fabsd) },
  { FRVBF_INSN_FDABSS, SEM_FN_NAME (frvbf,fdabss) },
  { FRVBF_INSN_CFABSS, SEM_FN_NAME (frvbf,cfabss) },
  { FRVBF_INSN_FSQRTS, SEM_FN_NAME (frvbf,fsqrts) },
  { FRVBF_INSN_FDSQRTS, SEM_FN_NAME (frvbf,fdsqrts) },
  { FRVBF_INSN_NFDSQRTS, SEM_FN_NAME (frvbf,nfdsqrts) },
  { FRVBF_INSN_FSQRTD, SEM_FN_NAME (frvbf,fsqrtd) },
  { FRVBF_INSN_CFSQRTS, SEM_FN_NAME (frvbf,cfsqrts) },
  { FRVBF_INSN_NFSQRTS, SEM_FN_NAME (frvbf,nfsqrts) },
  { FRVBF_INSN_FADDS, SEM_FN_NAME (frvbf,fadds) },
  { FRVBF_INSN_FSUBS, SEM_FN_NAME (frvbf,fsubs) },
  { FRVBF_INSN_FMULS, SEM_FN_NAME (frvbf,fmuls) },
  { FRVBF_INSN_FDIVS, SEM_FN_NAME (frvbf,fdivs) },
  { FRVBF_INSN_FADDD, SEM_FN_NAME (frvbf,faddd) },
  { FRVBF_INSN_FSUBD, SEM_FN_NAME (frvbf,fsubd) },
  { FRVBF_INSN_FMULD, SEM_FN_NAME (frvbf,fmuld) },
  { FRVBF_INSN_FDIVD, SEM_FN_NAME (frvbf,fdivd) },
  { FRVBF_INSN_CFADDS, SEM_FN_NAME (frvbf,cfadds) },
  { FRVBF_INSN_CFSUBS, SEM_FN_NAME (frvbf,cfsubs) },
  { FRVBF_INSN_CFMULS, SEM_FN_NAME (frvbf,cfmuls) },
  { FRVBF_INSN_CFDIVS, SEM_FN_NAME (frvbf,cfdivs) },
  { FRVBF_INSN_NFADDS, SEM_FN_NAME (frvbf,nfadds) },
  { FRVBF_INSN_NFSUBS, SEM_FN_NAME (frvbf,nfsubs) },
  { FRVBF_INSN_NFMULS, SEM_FN_NAME (frvbf,nfmuls) },
  { FRVBF_INSN_NFDIVS, SEM_FN_NAME (frvbf,nfdivs) },
  { FRVBF_INSN_FCMPS, SEM_FN_NAME (frvbf,fcmps) },
  { FRVBF_INSN_FCMPD, SEM_FN_NAME (frvbf,fcmpd) },
  { FRVBF_INSN_CFCMPS, SEM_FN_NAME (frvbf,cfcmps) },
  { FRVBF_INSN_FDCMPS, SEM_FN_NAME (frvbf,fdcmps) },
  { FRVBF_INSN_FMADDS, SEM_FN_NAME (frvbf,fmadds) },
  { FRVBF_INSN_FMSUBS, SEM_FN_NAME (frvbf,fmsubs) },
  { FRVBF_INSN_FMADDD, SEM_FN_NAME (frvbf,fmaddd) },
  { FRVBF_INSN_FMSUBD, SEM_FN_NAME (frvbf,fmsubd) },
  { FRVBF_INSN_FDMADDS, SEM_FN_NAME (frvbf,fdmadds) },
  { FRVBF_INSN_NFDMADDS, SEM_FN_NAME (frvbf,nfdmadds) },
  { FRVBF_INSN_CFMADDS, SEM_FN_NAME (frvbf,cfmadds) },
  { FRVBF_INSN_CFMSUBS, SEM_FN_NAME (frvbf,cfmsubs) },
  { FRVBF_INSN_NFMADDS, SEM_FN_NAME (frvbf,nfmadds) },
  { FRVBF_INSN_NFMSUBS, SEM_FN_NAME (frvbf,nfmsubs) },
  { FRVBF_INSN_FMAS, SEM_FN_NAME (frvbf,fmas) },
  { FRVBF_INSN_FMSS, SEM_FN_NAME (frvbf,fmss) },
  { FRVBF_INSN_FDMAS, SEM_FN_NAME (frvbf,fdmas) },
  { FRVBF_INSN_FDMSS, SEM_FN_NAME (frvbf,fdmss) },
  { FRVBF_INSN_NFDMAS, SEM_FN_NAME (frvbf,nfdmas) },
  { FRVBF_INSN_NFDMSS, SEM_FN_NAME (frvbf,nfdmss) },
  { FRVBF_INSN_CFMAS, SEM_FN_NAME (frvbf,cfmas) },
  { FRVBF_INSN_CFMSS, SEM_FN_NAME (frvbf,cfmss) },
  { FRVBF_INSN_FMAD, SEM_FN_NAME (frvbf,fmad) },
  { FRVBF_INSN_FMSD, SEM_FN_NAME (frvbf,fmsd) },
  { FRVBF_INSN_NFMAS, SEM_FN_NAME (frvbf,nfmas) },
  { FRVBF_INSN_NFMSS, SEM_FN_NAME (frvbf,nfmss) },
  { FRVBF_INSN_FDADDS, SEM_FN_NAME (frvbf,fdadds) },
  { FRVBF_INSN_FDSUBS, SEM_FN_NAME (frvbf,fdsubs) },
  { FRVBF_INSN_FDMULS, SEM_FN_NAME (frvbf,fdmuls) },
  { FRVBF_INSN_FDDIVS, SEM_FN_NAME (frvbf,fddivs) },
  { FRVBF_INSN_FDSADS, SEM_FN_NAME (frvbf,fdsads) },
  { FRVBF_INSN_FDMULCS, SEM_FN_NAME (frvbf,fdmulcs) },
  { FRVBF_INSN_NFDMULCS, SEM_FN_NAME (frvbf,nfdmulcs) },
  { FRVBF_INSN_NFDADDS, SEM_FN_NAME (frvbf,nfdadds) },
  { FRVBF_INSN_NFDSUBS, SEM_FN_NAME (frvbf,nfdsubs) },
  { FRVBF_INSN_NFDMULS, SEM_FN_NAME (frvbf,nfdmuls) },
  { FRVBF_INSN_NFDDIVS, SEM_FN_NAME (frvbf,nfddivs) },
  { FRVBF_INSN_NFDSADS, SEM_FN_NAME (frvbf,nfdsads) },
  { FRVBF_INSN_NFDCMPS, SEM_FN_NAME (frvbf,nfdcmps) },
  { FRVBF_INSN_MHSETLOS, SEM_FN_NAME (frvbf,mhsetlos) },
  { FRVBF_INSN_MHSETHIS, SEM_FN_NAME (frvbf,mhsethis) },
  { FRVBF_INSN_MHDSETS, SEM_FN_NAME (frvbf,mhdsets) },
  { FRVBF_INSN_MHSETLOH, SEM_FN_NAME (frvbf,mhsetloh) },
  { FRVBF_INSN_MHSETHIH, SEM_FN_NAME (frvbf,mhsethih) },
  { FRVBF_INSN_MHDSETH, SEM_FN_NAME (frvbf,mhdseth) },
  { FRVBF_INSN_MAND, SEM_FN_NAME (frvbf,mand) },
  { FRVBF_INSN_MOR, SEM_FN_NAME (frvbf,mor) },
  { FRVBF_INSN_MXOR, SEM_FN_NAME (frvbf,mxor) },
  { FRVBF_INSN_CMAND, SEM_FN_NAME (frvbf,cmand) },
  { FRVBF_INSN_CMOR, SEM_FN_NAME (frvbf,cmor) },
  { FRVBF_INSN_CMXOR, SEM_FN_NAME (frvbf,cmxor) },
  { FRVBF_INSN_MNOT, SEM_FN_NAME (frvbf,mnot) },
  { FRVBF_INSN_CMNOT, SEM_FN_NAME (frvbf,cmnot) },
  { FRVBF_INSN_MROTLI, SEM_FN_NAME (frvbf,mrotli) },
  { FRVBF_INSN_MROTRI, SEM_FN_NAME (frvbf,mrotri) },
  { FRVBF_INSN_MWCUT, SEM_FN_NAME (frvbf,mwcut) },
  { FRVBF_INSN_MWCUTI, SEM_FN_NAME (frvbf,mwcuti) },
  { FRVBF_INSN_MCUT, SEM_FN_NAME (frvbf,mcut) },
  { FRVBF_INSN_MCUTI, SEM_FN_NAME (frvbf,mcuti) },
  { FRVBF_INSN_MCUTSS, SEM_FN_NAME (frvbf,mcutss) },
  { FRVBF_INSN_MCUTSSI, SEM_FN_NAME (frvbf,mcutssi) },
  { FRVBF_INSN_MDCUTSSI, SEM_FN_NAME (frvbf,mdcutssi) },
  { FRVBF_INSN_MAVEH, SEM_FN_NAME (frvbf,maveh) },
  { FRVBF_INSN_MSLLHI, SEM_FN_NAME (frvbf,msllhi) },
  { FRVBF_INSN_MSRLHI, SEM_FN_NAME (frvbf,msrlhi) },
  { FRVBF_INSN_MSRAHI, SEM_FN_NAME (frvbf,msrahi) },
  { FRVBF_INSN_MDROTLI, SEM_FN_NAME (frvbf,mdrotli) },
  { FRVBF_INSN_MCPLHI, SEM_FN_NAME (frvbf,mcplhi) },
  { FRVBF_INSN_MCPLI, SEM_FN_NAME (frvbf,mcpli) },
  { FRVBF_INSN_MSATHS, SEM_FN_NAME (frvbf,msaths) },
  { FRVBF_INSN_MQSATHS, SEM_FN_NAME (frvbf,mqsaths) },
  { FRVBF_INSN_MSATHU, SEM_FN_NAME (frvbf,msathu) },
  { FRVBF_INSN_MCMPSH, SEM_FN_NAME (frvbf,mcmpsh) },
  { FRVBF_INSN_MCMPUH, SEM_FN_NAME (frvbf,mcmpuh) },
  { FRVBF_INSN_MABSHS, SEM_FN_NAME (frvbf,mabshs) },
  { FRVBF_INSN_MADDHSS, SEM_FN_NAME (frvbf,maddhss) },
  { FRVBF_INSN_MADDHUS, SEM_FN_NAME (frvbf,maddhus) },
  { FRVBF_INSN_MSUBHSS, SEM_FN_NAME (frvbf,msubhss) },
  { FRVBF_INSN_MSUBHUS, SEM_FN_NAME (frvbf,msubhus) },
  { FRVBF_INSN_CMADDHSS, SEM_FN_NAME (frvbf,cmaddhss) },
  { FRVBF_INSN_CMADDHUS, SEM_FN_NAME (frvbf,cmaddhus) },
  { FRVBF_INSN_CMSUBHSS, SEM_FN_NAME (frvbf,cmsubhss) },
  { FRVBF_INSN_CMSUBHUS, SEM_FN_NAME (frvbf,cmsubhus) },
  { FRVBF_INSN_MQADDHSS, SEM_FN_NAME (frvbf,mqaddhss) },
  { FRVBF_INSN_MQADDHUS, SEM_FN_NAME (frvbf,mqaddhus) },
  { FRVBF_INSN_MQSUBHSS, SEM_FN_NAME (frvbf,mqsubhss) },
  { FRVBF_INSN_MQSUBHUS, SEM_FN_NAME (frvbf,mqsubhus) },
  { FRVBF_INSN_CMQADDHSS, SEM_FN_NAME (frvbf,cmqaddhss) },
  { FRVBF_INSN_CMQADDHUS, SEM_FN_NAME (frvbf,cmqaddhus) },
  { FRVBF_INSN_CMQSUBHSS, SEM_FN_NAME (frvbf,cmqsubhss) },
  { FRVBF_INSN_CMQSUBHUS, SEM_FN_NAME (frvbf,cmqsubhus) },
  { FRVBF_INSN_MQLCLRHS, SEM_FN_NAME (frvbf,mqlclrhs) },
  { FRVBF_INSN_MQLMTHS, SEM_FN_NAME (frvbf,mqlmths) },
  { FRVBF_INSN_MQSLLHI, SEM_FN_NAME (frvbf,mqsllhi) },
  { FRVBF_INSN_MQSRAHI, SEM_FN_NAME (frvbf,mqsrahi) },
  { FRVBF_INSN_MADDACCS, SEM_FN_NAME (frvbf,maddaccs) },
  { FRVBF_INSN_MSUBACCS, SEM_FN_NAME (frvbf,msubaccs) },
  { FRVBF_INSN_MDADDACCS, SEM_FN_NAME (frvbf,mdaddaccs) },
  { FRVBF_INSN_MDSUBACCS, SEM_FN_NAME (frvbf,mdsubaccs) },
  { FRVBF_INSN_MASACCS, SEM_FN_NAME (frvbf,masaccs) },
  { FRVBF_INSN_MDASACCS, SEM_FN_NAME (frvbf,mdasaccs) },
  { FRVBF_INSN_MMULHS, SEM_FN_NAME (frvbf,mmulhs) },
  { FRVBF_INSN_MMULHU, SEM_FN_NAME (frvbf,mmulhu) },
  { FRVBF_INSN_MMULXHS, SEM_FN_NAME (frvbf,mmulxhs) },
  { FRVBF_INSN_MMULXHU, SEM_FN_NAME (frvbf,mmulxhu) },
  { FRVBF_INSN_CMMULHS, SEM_FN_NAME (frvbf,cmmulhs) },
  { FRVBF_INSN_CMMULHU, SEM_FN_NAME (frvbf,cmmulhu) },
  { FRVBF_INSN_MQMULHS, SEM_FN_NAME (frvbf,mqmulhs) },
  { FRVBF_INSN_MQMULHU, SEM_FN_NAME (frvbf,mqmulhu) },
  { FRVBF_INSN_MQMULXHS, SEM_FN_NAME (frvbf,mqmulxhs) },
  { FRVBF_INSN_MQMULXHU, SEM_FN_NAME (frvbf,mqmulxhu) },
  { FRVBF_INSN_CMQMULHS, SEM_FN_NAME (frvbf,cmqmulhs) },
  { FRVBF_INSN_CMQMULHU, SEM_FN_NAME (frvbf,cmqmulhu) },
  { FRVBF_INSN_MMACHS, SEM_FN_NAME (frvbf,mmachs) },
  { FRVBF_INSN_MMACHU, SEM_FN_NAME (frvbf,mmachu) },
  { FRVBF_INSN_MMRDHS, SEM_FN_NAME (frvbf,mmrdhs) },
  { FRVBF_INSN_MMRDHU, SEM_FN_NAME (frvbf,mmrdhu) },
  { FRVBF_INSN_CMMACHS, SEM_FN_NAME (frvbf,cmmachs) },
  { FRVBF_INSN_CMMACHU, SEM_FN_NAME (frvbf,cmmachu) },
  { FRVBF_INSN_MQMACHS, SEM_FN_NAME (frvbf,mqmachs) },
  { FRVBF_INSN_MQMACHU, SEM_FN_NAME (frvbf,mqmachu) },
  { FRVBF_INSN_CMQMACHS, SEM_FN_NAME (frvbf,cmqmachs) },
  { FRVBF_INSN_CMQMACHU, SEM_FN_NAME (frvbf,cmqmachu) },
  { FRVBF_INSN_MQXMACHS, SEM_FN_NAME (frvbf,mqxmachs) },
  { FRVBF_INSN_MQXMACXHS, SEM_FN_NAME (frvbf,mqxmacxhs) },
  { FRVBF_INSN_MQMACXHS, SEM_FN_NAME (frvbf,mqmacxhs) },
  { FRVBF_INSN_MCPXRS, SEM_FN_NAME (frvbf,mcpxrs) },
  { FRVBF_INSN_MCPXRU, SEM_FN_NAME (frvbf,mcpxru) },
  { FRVBF_INSN_MCPXIS, SEM_FN_NAME (frvbf,mcpxis) },
  { FRVBF_INSN_MCPXIU, SEM_FN_NAME (frvbf,mcpxiu) },
  { FRVBF_INSN_CMCPXRS, SEM_FN_NAME (frvbf,cmcpxrs) },
  { FRVBF_INSN_CMCPXRU, SEM_FN_NAME (frvbf,cmcpxru) },
  { FRVBF_INSN_CMCPXIS, SEM_FN_NAME (frvbf,cmcpxis) },
  { FRVBF_INSN_CMCPXIU, SEM_FN_NAME (frvbf,cmcpxiu) },
  { FRVBF_INSN_MQCPXRS, SEM_FN_NAME (frvbf,mqcpxrs) },
  { FRVBF_INSN_MQCPXRU, SEM_FN_NAME (frvbf,mqcpxru) },
  { FRVBF_INSN_MQCPXIS, SEM_FN_NAME (frvbf,mqcpxis) },
  { FRVBF_INSN_MQCPXIU, SEM_FN_NAME (frvbf,mqcpxiu) },
  { FRVBF_INSN_MEXPDHW, SEM_FN_NAME (frvbf,mexpdhw) },
  { FRVBF_INSN_CMEXPDHW, SEM_FN_NAME (frvbf,cmexpdhw) },
  { FRVBF_INSN_MEXPDHD, SEM_FN_NAME (frvbf,mexpdhd) },
  { FRVBF_INSN_CMEXPDHD, SEM_FN_NAME (frvbf,cmexpdhd) },
  { FRVBF_INSN_MPACKH, SEM_FN_NAME (frvbf,mpackh) },
  { FRVBF_INSN_MDPACKH, SEM_FN_NAME (frvbf,mdpackh) },
  { FRVBF_INSN_MUNPACKH, SEM_FN_NAME (frvbf,munpackh) },
  { FRVBF_INSN_MDUNPACKH, SEM_FN_NAME (frvbf,mdunpackh) },
  { FRVBF_INSN_MBTOH, SEM_FN_NAME (frvbf,mbtoh) },
  { FRVBF_INSN_CMBTOH, SEM_FN_NAME (frvbf,cmbtoh) },
  { FRVBF_INSN_MHTOB, SEM_FN_NAME (frvbf,mhtob) },
  { FRVBF_INSN_CMHTOB, SEM_FN_NAME (frvbf,cmhtob) },
  { FRVBF_INSN_MBTOHE, SEM_FN_NAME (frvbf,mbtohe) },
  { FRVBF_INSN_CMBTOHE, SEM_FN_NAME (frvbf,cmbtohe) },
  { FRVBF_INSN_MNOP, SEM_FN_NAME (frvbf,mnop) },
  { FRVBF_INSN_MCLRACC_0, SEM_FN_NAME (frvbf,mclracc_0) },
  { FRVBF_INSN_MCLRACC_1, SEM_FN_NAME (frvbf,mclracc_1) },
  { FRVBF_INSN_MRDACC, SEM_FN_NAME (frvbf,mrdacc) },
  { FRVBF_INSN_MRDACCG, SEM_FN_NAME (frvbf,mrdaccg) },
  { FRVBF_INSN_MWTACC, SEM_FN_NAME (frvbf,mwtacc) },
  { FRVBF_INSN_MWTACCG, SEM_FN_NAME (frvbf,mwtaccg) },
  { FRVBF_INSN_MCOP1, SEM_FN_NAME (frvbf,mcop1) },
  { FRVBF_INSN_MCOP2, SEM_FN_NAME (frvbf,mcop2) },
  { FRVBF_INSN_FNOP, SEM_FN_NAME (frvbf,fnop) },
  { 0, 0 }
};

/* Add the semantic fns to IDESC_TABLE.  */

void
SEM_FN_NAME (frvbf,init_idesc_table) (SIM_CPU *current_cpu)
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
	idesc_table[sf->index].sem_fast = SEM_FN_NAME (frvbf,x_invalid);
#else
      if (valid_p)
	idesc_table[sf->index].sem_full = sf->fn;
      else
	idesc_table[sf->index].sem_full = SEM_FN_NAME (frvbf,x_invalid);
#endif
    }
}

