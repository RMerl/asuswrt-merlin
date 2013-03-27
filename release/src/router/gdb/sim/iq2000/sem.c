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

#define WANT_CPU iq2000bf
#define WANT_CPU_IQ2000BF

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
SEM_FN_NAME (iq2000bf,x_invalid) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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
SEM_FN_NAME (iq2000bf,x_after) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_IQ2000BF
    iq2000bf_pbb_after (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-before: --before-- */

static SEM_PC
SEM_FN_NAME (iq2000bf,x_before) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_IQ2000BF
    iq2000bf_pbb_before (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-cti-chain: --cti-chain-- */

static SEM_PC
SEM_FN_NAME (iq2000bf,x_cti_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* x-chain: --chain-- */

static SEM_PC
SEM_FN_NAME (iq2000bf,x_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_IQ2000BF
    vpc = iq2000bf_pbb_chain (current_cpu, sem_arg);
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
SEM_FN_NAME (iq2000bf,x_begin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* add: add $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,add) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addi: addi $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,addi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addiu: addiu $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,addiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addu: addu $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,addu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ADDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ado16: ado16 $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,ado16) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* and: and $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,and) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* andi: andi $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,andi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* andoi: andoi $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,andoi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ORSI (0xffff0000, EXTHISI (TRUNCSIHI (FLD (f_imm)))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* nor: nor $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,nor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = INVSI (ORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* or: or $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,or) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ori: ori $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,ori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ram: ram $rd,$rt,$shamt,$maskl,$maskr */

static SEM_PC
SEM_FN_NAME (iq2000bf,ram) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* sll: sll $rd,$rt,$shamt */

static SEM_PC
SEM_FN_NAME (iq2000bf,sll) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sllv: sllv $rd,$rt,$rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,sllv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* slmv: slmv $rd,$rt,$rs,$shamt */

static SEM_PC
SEM_FN_NAME (iq2000bf,slmv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (SLLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt)), SRLSI (0xffffffff, GET_H_GR (FLD (f_rs))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* slt: slt $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,slt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* slti: slti $rt,$rs,$imm */

static SEM_PC
SEM_FN_NAME (iq2000bf,slti) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* sltiu: sltiu $rt,$rs,$imm */

static SEM_PC
SEM_FN_NAME (iq2000bf,sltiu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* sltu: sltu $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,sltu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* sra: sra $rd,$rt,$shamt */

static SEM_PC
SEM_FN_NAME (iq2000bf,sra) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srav: srav $rd,$rt,$rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,srav) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRASI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srl: srl $rd,$rt,$shamt */

static SEM_PC
SEM_FN_NAME (iq2000bf,srl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srlv: srlv $rd,$rt,$rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,srlv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SRLSI (GET_H_GR (FLD (f_rt)), ANDSI (GET_H_GR (FLD (f_rs)), 31));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* srmv: srmv $rd,$rt,$rs,$shamt */

static SEM_PC
SEM_FN_NAME (iq2000bf,srmv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ram.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (SRLSI (GET_H_GR (FLD (f_rt)), FLD (f_shamt)), SLLSI (0xffffffff, GET_H_GR (FLD (f_rs))));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sub: sub $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,sub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subu: subu $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,subu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xor: xor $rd,$rs,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,xor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_rs)), GET_H_GR (FLD (f_rt)));
    SET_H_GR (FLD (f_rd), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xori: xori $rt,$rs,$lo16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,xori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = XORSI (GET_H_GR (FLD (f_rs)), ZEXTSISI (FLD (f_imm)));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* bbi: bbi $rs($bitnum),$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bbin: bbin $rs($bitnum),$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bbin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bbv: bbv $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bbv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bbvn: bbvn $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bbvn) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* beq: beq $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,beq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* beql: beql $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,beql) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgez: bgez $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgez) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgezal: bgezal $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgezal) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgezall: bgezall $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgezall) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgezl: bgezl $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgezl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bltz: bltz $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bltz) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bltzl: bltzl $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bltzl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bltzal: bltzal $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bltzal) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bltzall: bltzall $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bltzall) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bmb0: bmb0 $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bmb0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bmb1: bmb1 $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bmb1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bmb2: bmb2 $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bmb2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bmb3: bmb3 $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bmb3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bne: bne $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bnel: bnel $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bnel) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* jalr: jalr $rd,$rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,jalr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* jr: jr $rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,jr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    USI opval = GET_H_GR (FLD (f_rs));
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* lb: lb $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,lb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lbu: lbu $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,lbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lh: lh $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,lh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lhu: lhu $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,lhu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ZEXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm))))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lui: lui $rt,$hi16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,lui) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SLLSI (FLD (f_imm), 16);
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* lw: lw $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,lw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sb: sb $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,sb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    QI opval = ANDQI (GET_H_GR (FLD (f_rt)), 255);
    SETMEMQI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sh: sh $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,sh) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = ANDHI (GET_H_GR (FLD (f_rt)), 65535);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sw: sw $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,sw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_GR (FLD (f_rt));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GR (FLD (f_rs)), EXTHISI (TRUNCSIHI (FLD (f_imm)))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* break: break */

static SEM_PC
SEM_FN_NAME (iq2000bf,break) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

do_break (current_cpu, pc);

  return vpc;
#undef FLD
}

/* syscall: syscall */

static SEM_PC
SEM_FN_NAME (iq2000bf,syscall) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

do_syscall (current_cpu);

  return vpc;
#undef FLD
}

/* andoui: andoui $rt,$rs,$hi16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,andoui) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_rs)), ORSI (SLLSI (FLD (f_imm), 16), 65535));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* orui: orui $rt,$rs,$hi16 */

static SEM_PC
SEM_FN_NAME (iq2000bf,orui) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ORSI (GET_H_GR (FLD (f_rs)), SLLSI (FLD (f_imm), 16));
    SET_H_GR (FLD (f_rt), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* bgtz: bgtz $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgtz) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bgtzl: bgtzl $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bgtzl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* blez: blez $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,blez) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* blezl: blezl $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,blezl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* mrgb: mrgb $rd,$rs,$rt,$mask */

static SEM_PC
SEM_FN_NAME (iq2000bf,mrgb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mrgb.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* bctxt: bctxt $rs,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bctxt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc0f: bc0f $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc0f) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc0fl: bc0fl $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc0fl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc3f: bc3f $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc3f) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc3fl: bc3fl $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc3fl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc0t: bc0t $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc0t) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc0tl: bc0tl $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc0tl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc3t: bc3t $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc3t) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bc3tl: bc3tl $offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bc3tl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* cfc0: cfc0 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,cfc0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* cfc1: cfc1 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,cfc1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* cfc2: cfc2 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,cfc2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* cfc3: cfc3 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,cfc3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* chkhdr: chkhdr $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,chkhdr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* ctc0: ctc0 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,ctc0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* ctc1: ctc1 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,ctc1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* ctc2: ctc2 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,ctc2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* ctc3: ctc3 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,ctc3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* jcr: jcr $rs */

static SEM_PC
SEM_FN_NAME (iq2000bf,jcr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* luc32: luc32 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,luc32) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* luc32l: luc32l $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,luc32l) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* luc64: luc64 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,luc64) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* luc64l: luc64l $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,luc64l) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* luk: luk $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,luk) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lulck: lulck $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,lulck) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lum32: lum32 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lum32) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lum32l: lum32l $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lum32l) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lum64: lum64 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lum64) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lum64l: lum64l $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lum64l) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lur: lur $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lur) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* lurl: lurl $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,lurl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* luulck: luulck $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,luulck) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mfc0: mfc0 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mfc0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mfc1: mfc1 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mfc1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mfc2: mfc2 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mfc2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mfc3: mfc3 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mfc3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mtc0: mtc0 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mtc0) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mtc1: mtc1 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mtc1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mtc2: mtc2 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mtc2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* mtc3: mtc3 $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,mtc3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* pkrl: pkrl $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,pkrl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* pkrlr1: pkrlr1 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,pkrlr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* pkrlr30: pkrlr30 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,pkrlr30) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rb: rb $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,rb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rbr1: rbr1 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,rbr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rbr30: rbr30 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,rbr30) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rfe: rfe */

static SEM_PC
SEM_FN_NAME (iq2000bf,rfe) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rx: rx $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,rx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rxr1: rxr1 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,rxr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* rxr30: rxr30 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,rxr30) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* sleep: sleep */

static SEM_PC
SEM_FN_NAME (iq2000bf,sleep) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* srrd: srrd $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,srrd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* srrdl: srrdl $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,srrdl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* srulck: srulck $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,srulck) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* srwr: srwr $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,srwr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* srwru: srwru $rt,$rd */

static SEM_PC
SEM_FN_NAME (iq2000bf,srwru) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* trapqfl: trapqfl */

static SEM_PC
SEM_FN_NAME (iq2000bf,trapqfl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* trapqne: trapqne */

static SEM_PC
SEM_FN_NAME (iq2000bf,trapqne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* traprel: traprel $rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,traprel) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wb: wb $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,wb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wbu: wbu $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,wbu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wbr1: wbr1 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wbr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wbr1u: wbr1u $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wbr1u) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wbr30: wbr30 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wbr30) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wbr30u: wbr30u $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wbr30u) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wx: wx $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,wx) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wxu: wxu $rd,$rt */

static SEM_PC
SEM_FN_NAME (iq2000bf,wxu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wxr1: wxr1 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wxr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wxr1u: wxr1u $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wxr1u) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wxr30: wxr30 $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wxr30) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* wxr30u: wxr30u $rt,$index,$count */

static SEM_PC
SEM_FN_NAME (iq2000bf,wxr30u) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* ldw: ldw $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,ldw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* sdw: sdw $rt,$lo16($base) */

static SEM_PC
SEM_FN_NAME (iq2000bf,sdw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

  return vpc;
#undef FLD
}

/* j: j $jmptarg */

static SEM_PC
SEM_FN_NAME (iq2000bf,j) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_j.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    USI opval = FLD (i_jmptarg);
    SEM_BRANCH_VIA_CACHE (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'x', opval);
  }
}

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* jal: jal $jmptarg */

static SEM_PC
SEM_FN_NAME (iq2000bf,jal) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_j.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* bmb: bmb $rs,$rt,$offset */

static SEM_PC
SEM_FN_NAME (iq2000bf,bmb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bbi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
  return vpc;
#undef FLD
}

/* Table of all semantic fns.  */

static const struct sem_fn_desc sem_fns[] = {
  { IQ2000BF_INSN_X_INVALID, SEM_FN_NAME (iq2000bf,x_invalid) },
  { IQ2000BF_INSN_X_AFTER, SEM_FN_NAME (iq2000bf,x_after) },
  { IQ2000BF_INSN_X_BEFORE, SEM_FN_NAME (iq2000bf,x_before) },
  { IQ2000BF_INSN_X_CTI_CHAIN, SEM_FN_NAME (iq2000bf,x_cti_chain) },
  { IQ2000BF_INSN_X_CHAIN, SEM_FN_NAME (iq2000bf,x_chain) },
  { IQ2000BF_INSN_X_BEGIN, SEM_FN_NAME (iq2000bf,x_begin) },
  { IQ2000BF_INSN_ADD, SEM_FN_NAME (iq2000bf,add) },
  { IQ2000BF_INSN_ADDI, SEM_FN_NAME (iq2000bf,addi) },
  { IQ2000BF_INSN_ADDIU, SEM_FN_NAME (iq2000bf,addiu) },
  { IQ2000BF_INSN_ADDU, SEM_FN_NAME (iq2000bf,addu) },
  { IQ2000BF_INSN_ADO16, SEM_FN_NAME (iq2000bf,ado16) },
  { IQ2000BF_INSN_AND, SEM_FN_NAME (iq2000bf,and) },
  { IQ2000BF_INSN_ANDI, SEM_FN_NAME (iq2000bf,andi) },
  { IQ2000BF_INSN_ANDOI, SEM_FN_NAME (iq2000bf,andoi) },
  { IQ2000BF_INSN_NOR, SEM_FN_NAME (iq2000bf,nor) },
  { IQ2000BF_INSN_OR, SEM_FN_NAME (iq2000bf,or) },
  { IQ2000BF_INSN_ORI, SEM_FN_NAME (iq2000bf,ori) },
  { IQ2000BF_INSN_RAM, SEM_FN_NAME (iq2000bf,ram) },
  { IQ2000BF_INSN_SLL, SEM_FN_NAME (iq2000bf,sll) },
  { IQ2000BF_INSN_SLLV, SEM_FN_NAME (iq2000bf,sllv) },
  { IQ2000BF_INSN_SLMV, SEM_FN_NAME (iq2000bf,slmv) },
  { IQ2000BF_INSN_SLT, SEM_FN_NAME (iq2000bf,slt) },
  { IQ2000BF_INSN_SLTI, SEM_FN_NAME (iq2000bf,slti) },
  { IQ2000BF_INSN_SLTIU, SEM_FN_NAME (iq2000bf,sltiu) },
  { IQ2000BF_INSN_SLTU, SEM_FN_NAME (iq2000bf,sltu) },
  { IQ2000BF_INSN_SRA, SEM_FN_NAME (iq2000bf,sra) },
  { IQ2000BF_INSN_SRAV, SEM_FN_NAME (iq2000bf,srav) },
  { IQ2000BF_INSN_SRL, SEM_FN_NAME (iq2000bf,srl) },
  { IQ2000BF_INSN_SRLV, SEM_FN_NAME (iq2000bf,srlv) },
  { IQ2000BF_INSN_SRMV, SEM_FN_NAME (iq2000bf,srmv) },
  { IQ2000BF_INSN_SUB, SEM_FN_NAME (iq2000bf,sub) },
  { IQ2000BF_INSN_SUBU, SEM_FN_NAME (iq2000bf,subu) },
  { IQ2000BF_INSN_XOR, SEM_FN_NAME (iq2000bf,xor) },
  { IQ2000BF_INSN_XORI, SEM_FN_NAME (iq2000bf,xori) },
  { IQ2000BF_INSN_BBI, SEM_FN_NAME (iq2000bf,bbi) },
  { IQ2000BF_INSN_BBIN, SEM_FN_NAME (iq2000bf,bbin) },
  { IQ2000BF_INSN_BBV, SEM_FN_NAME (iq2000bf,bbv) },
  { IQ2000BF_INSN_BBVN, SEM_FN_NAME (iq2000bf,bbvn) },
  { IQ2000BF_INSN_BEQ, SEM_FN_NAME (iq2000bf,beq) },
  { IQ2000BF_INSN_BEQL, SEM_FN_NAME (iq2000bf,beql) },
  { IQ2000BF_INSN_BGEZ, SEM_FN_NAME (iq2000bf,bgez) },
  { IQ2000BF_INSN_BGEZAL, SEM_FN_NAME (iq2000bf,bgezal) },
  { IQ2000BF_INSN_BGEZALL, SEM_FN_NAME (iq2000bf,bgezall) },
  { IQ2000BF_INSN_BGEZL, SEM_FN_NAME (iq2000bf,bgezl) },
  { IQ2000BF_INSN_BLTZ, SEM_FN_NAME (iq2000bf,bltz) },
  { IQ2000BF_INSN_BLTZL, SEM_FN_NAME (iq2000bf,bltzl) },
  { IQ2000BF_INSN_BLTZAL, SEM_FN_NAME (iq2000bf,bltzal) },
  { IQ2000BF_INSN_BLTZALL, SEM_FN_NAME (iq2000bf,bltzall) },
  { IQ2000BF_INSN_BMB0, SEM_FN_NAME (iq2000bf,bmb0) },
  { IQ2000BF_INSN_BMB1, SEM_FN_NAME (iq2000bf,bmb1) },
  { IQ2000BF_INSN_BMB2, SEM_FN_NAME (iq2000bf,bmb2) },
  { IQ2000BF_INSN_BMB3, SEM_FN_NAME (iq2000bf,bmb3) },
  { IQ2000BF_INSN_BNE, SEM_FN_NAME (iq2000bf,bne) },
  { IQ2000BF_INSN_BNEL, SEM_FN_NAME (iq2000bf,bnel) },
  { IQ2000BF_INSN_JALR, SEM_FN_NAME (iq2000bf,jalr) },
  { IQ2000BF_INSN_JR, SEM_FN_NAME (iq2000bf,jr) },
  { IQ2000BF_INSN_LB, SEM_FN_NAME (iq2000bf,lb) },
  { IQ2000BF_INSN_LBU, SEM_FN_NAME (iq2000bf,lbu) },
  { IQ2000BF_INSN_LH, SEM_FN_NAME (iq2000bf,lh) },
  { IQ2000BF_INSN_LHU, SEM_FN_NAME (iq2000bf,lhu) },
  { IQ2000BF_INSN_LUI, SEM_FN_NAME (iq2000bf,lui) },
  { IQ2000BF_INSN_LW, SEM_FN_NAME (iq2000bf,lw) },
  { IQ2000BF_INSN_SB, SEM_FN_NAME (iq2000bf,sb) },
  { IQ2000BF_INSN_SH, SEM_FN_NAME (iq2000bf,sh) },
  { IQ2000BF_INSN_SW, SEM_FN_NAME (iq2000bf,sw) },
  { IQ2000BF_INSN_BREAK, SEM_FN_NAME (iq2000bf,break) },
  { IQ2000BF_INSN_SYSCALL, SEM_FN_NAME (iq2000bf,syscall) },
  { IQ2000BF_INSN_ANDOUI, SEM_FN_NAME (iq2000bf,andoui) },
  { IQ2000BF_INSN_ORUI, SEM_FN_NAME (iq2000bf,orui) },
  { IQ2000BF_INSN_BGTZ, SEM_FN_NAME (iq2000bf,bgtz) },
  { IQ2000BF_INSN_BGTZL, SEM_FN_NAME (iq2000bf,bgtzl) },
  { IQ2000BF_INSN_BLEZ, SEM_FN_NAME (iq2000bf,blez) },
  { IQ2000BF_INSN_BLEZL, SEM_FN_NAME (iq2000bf,blezl) },
  { IQ2000BF_INSN_MRGB, SEM_FN_NAME (iq2000bf,mrgb) },
  { IQ2000BF_INSN_BCTXT, SEM_FN_NAME (iq2000bf,bctxt) },
  { IQ2000BF_INSN_BC0F, SEM_FN_NAME (iq2000bf,bc0f) },
  { IQ2000BF_INSN_BC0FL, SEM_FN_NAME (iq2000bf,bc0fl) },
  { IQ2000BF_INSN_BC3F, SEM_FN_NAME (iq2000bf,bc3f) },
  { IQ2000BF_INSN_BC3FL, SEM_FN_NAME (iq2000bf,bc3fl) },
  { IQ2000BF_INSN_BC0T, SEM_FN_NAME (iq2000bf,bc0t) },
  { IQ2000BF_INSN_BC0TL, SEM_FN_NAME (iq2000bf,bc0tl) },
  { IQ2000BF_INSN_BC3T, SEM_FN_NAME (iq2000bf,bc3t) },
  { IQ2000BF_INSN_BC3TL, SEM_FN_NAME (iq2000bf,bc3tl) },
  { IQ2000BF_INSN_CFC0, SEM_FN_NAME (iq2000bf,cfc0) },
  { IQ2000BF_INSN_CFC1, SEM_FN_NAME (iq2000bf,cfc1) },
  { IQ2000BF_INSN_CFC2, SEM_FN_NAME (iq2000bf,cfc2) },
  { IQ2000BF_INSN_CFC3, SEM_FN_NAME (iq2000bf,cfc3) },
  { IQ2000BF_INSN_CHKHDR, SEM_FN_NAME (iq2000bf,chkhdr) },
  { IQ2000BF_INSN_CTC0, SEM_FN_NAME (iq2000bf,ctc0) },
  { IQ2000BF_INSN_CTC1, SEM_FN_NAME (iq2000bf,ctc1) },
  { IQ2000BF_INSN_CTC2, SEM_FN_NAME (iq2000bf,ctc2) },
  { IQ2000BF_INSN_CTC3, SEM_FN_NAME (iq2000bf,ctc3) },
  { IQ2000BF_INSN_JCR, SEM_FN_NAME (iq2000bf,jcr) },
  { IQ2000BF_INSN_LUC32, SEM_FN_NAME (iq2000bf,luc32) },
  { IQ2000BF_INSN_LUC32L, SEM_FN_NAME (iq2000bf,luc32l) },
  { IQ2000BF_INSN_LUC64, SEM_FN_NAME (iq2000bf,luc64) },
  { IQ2000BF_INSN_LUC64L, SEM_FN_NAME (iq2000bf,luc64l) },
  { IQ2000BF_INSN_LUK, SEM_FN_NAME (iq2000bf,luk) },
  { IQ2000BF_INSN_LULCK, SEM_FN_NAME (iq2000bf,lulck) },
  { IQ2000BF_INSN_LUM32, SEM_FN_NAME (iq2000bf,lum32) },
  { IQ2000BF_INSN_LUM32L, SEM_FN_NAME (iq2000bf,lum32l) },
  { IQ2000BF_INSN_LUM64, SEM_FN_NAME (iq2000bf,lum64) },
  { IQ2000BF_INSN_LUM64L, SEM_FN_NAME (iq2000bf,lum64l) },
  { IQ2000BF_INSN_LUR, SEM_FN_NAME (iq2000bf,lur) },
  { IQ2000BF_INSN_LURL, SEM_FN_NAME (iq2000bf,lurl) },
  { IQ2000BF_INSN_LUULCK, SEM_FN_NAME (iq2000bf,luulck) },
  { IQ2000BF_INSN_MFC0, SEM_FN_NAME (iq2000bf,mfc0) },
  { IQ2000BF_INSN_MFC1, SEM_FN_NAME (iq2000bf,mfc1) },
  { IQ2000BF_INSN_MFC2, SEM_FN_NAME (iq2000bf,mfc2) },
  { IQ2000BF_INSN_MFC3, SEM_FN_NAME (iq2000bf,mfc3) },
  { IQ2000BF_INSN_MTC0, SEM_FN_NAME (iq2000bf,mtc0) },
  { IQ2000BF_INSN_MTC1, SEM_FN_NAME (iq2000bf,mtc1) },
  { IQ2000BF_INSN_MTC2, SEM_FN_NAME (iq2000bf,mtc2) },
  { IQ2000BF_INSN_MTC3, SEM_FN_NAME (iq2000bf,mtc3) },
  { IQ2000BF_INSN_PKRL, SEM_FN_NAME (iq2000bf,pkrl) },
  { IQ2000BF_INSN_PKRLR1, SEM_FN_NAME (iq2000bf,pkrlr1) },
  { IQ2000BF_INSN_PKRLR30, SEM_FN_NAME (iq2000bf,pkrlr30) },
  { IQ2000BF_INSN_RB, SEM_FN_NAME (iq2000bf,rb) },
  { IQ2000BF_INSN_RBR1, SEM_FN_NAME (iq2000bf,rbr1) },
  { IQ2000BF_INSN_RBR30, SEM_FN_NAME (iq2000bf,rbr30) },
  { IQ2000BF_INSN_RFE, SEM_FN_NAME (iq2000bf,rfe) },
  { IQ2000BF_INSN_RX, SEM_FN_NAME (iq2000bf,rx) },
  { IQ2000BF_INSN_RXR1, SEM_FN_NAME (iq2000bf,rxr1) },
  { IQ2000BF_INSN_RXR30, SEM_FN_NAME (iq2000bf,rxr30) },
  { IQ2000BF_INSN_SLEEP, SEM_FN_NAME (iq2000bf,sleep) },
  { IQ2000BF_INSN_SRRD, SEM_FN_NAME (iq2000bf,srrd) },
  { IQ2000BF_INSN_SRRDL, SEM_FN_NAME (iq2000bf,srrdl) },
  { IQ2000BF_INSN_SRULCK, SEM_FN_NAME (iq2000bf,srulck) },
  { IQ2000BF_INSN_SRWR, SEM_FN_NAME (iq2000bf,srwr) },
  { IQ2000BF_INSN_SRWRU, SEM_FN_NAME (iq2000bf,srwru) },
  { IQ2000BF_INSN_TRAPQFL, SEM_FN_NAME (iq2000bf,trapqfl) },
  { IQ2000BF_INSN_TRAPQNE, SEM_FN_NAME (iq2000bf,trapqne) },
  { IQ2000BF_INSN_TRAPREL, SEM_FN_NAME (iq2000bf,traprel) },
  { IQ2000BF_INSN_WB, SEM_FN_NAME (iq2000bf,wb) },
  { IQ2000BF_INSN_WBU, SEM_FN_NAME (iq2000bf,wbu) },
  { IQ2000BF_INSN_WBR1, SEM_FN_NAME (iq2000bf,wbr1) },
  { IQ2000BF_INSN_WBR1U, SEM_FN_NAME (iq2000bf,wbr1u) },
  { IQ2000BF_INSN_WBR30, SEM_FN_NAME (iq2000bf,wbr30) },
  { IQ2000BF_INSN_WBR30U, SEM_FN_NAME (iq2000bf,wbr30u) },
  { IQ2000BF_INSN_WX, SEM_FN_NAME (iq2000bf,wx) },
  { IQ2000BF_INSN_WXU, SEM_FN_NAME (iq2000bf,wxu) },
  { IQ2000BF_INSN_WXR1, SEM_FN_NAME (iq2000bf,wxr1) },
  { IQ2000BF_INSN_WXR1U, SEM_FN_NAME (iq2000bf,wxr1u) },
  { IQ2000BF_INSN_WXR30, SEM_FN_NAME (iq2000bf,wxr30) },
  { IQ2000BF_INSN_WXR30U, SEM_FN_NAME (iq2000bf,wxr30u) },
  { IQ2000BF_INSN_LDW, SEM_FN_NAME (iq2000bf,ldw) },
  { IQ2000BF_INSN_SDW, SEM_FN_NAME (iq2000bf,sdw) },
  { IQ2000BF_INSN_J, SEM_FN_NAME (iq2000bf,j) },
  { IQ2000BF_INSN_JAL, SEM_FN_NAME (iq2000bf,jal) },
  { IQ2000BF_INSN_BMB, SEM_FN_NAME (iq2000bf,bmb) },
  { 0, 0 }
};

/* Add the semantic fns to IDESC_TABLE.  */

void
SEM_FN_NAME (iq2000bf,init_idesc_table) (SIM_CPU *current_cpu)
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
	idesc_table[sf->index].sem_fast = SEM_FN_NAME (iq2000bf,x_invalid);
#else
      if (valid_p)
	idesc_table[sf->index].sem_full = sf->fn;
      else
	idesc_table[sf->index].sem_full = SEM_FN_NAME (iq2000bf,x_invalid);
#endif
    }
}

