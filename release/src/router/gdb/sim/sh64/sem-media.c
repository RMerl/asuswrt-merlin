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

#define WANT_CPU sh64
#define WANT_CPU_SH64

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
SEM_FN_NAME (sh64_media,x_invalid) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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
SEM_FN_NAME (sh64_media,x_after) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_MEDIA
    sh64_media_pbb_after (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-before: --before-- */

static SEM_PC
SEM_FN_NAME (sh64_media,x_before) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_MEDIA
    sh64_media_pbb_before (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-cti-chain: --cti-chain-- */

static SEM_PC
SEM_FN_NAME (sh64_media,x_cti_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_MEDIA
#ifdef DEFINE_SWITCH
    vpc = sh64_media_pbb_cti_chain (current_cpu, sem_arg,
			       pbb_br_type, pbb_br_npc);
    BREAK (sem);
#else
    /* FIXME: Allow provision of explicit ifmt spec in insn spec.  */
    vpc = sh64_media_pbb_cti_chain (current_cpu, sem_arg,
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
SEM_FN_NAME (sh64_media,x_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_MEDIA
    vpc = sh64_media_pbb_chain (current_cpu, sem_arg);
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
SEM_FN_NAME (sh64_media,x_begin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_MEDIA
#if defined DEFINE_SWITCH || defined FAST_P
    /* In the switch case FAST_P is a constant, allowing several optimizations
       in any called inline functions.  */
    vpc = sh64_media_pbb_begin (current_cpu, FAST_P);
#else
#if 0 /* cgen engine can't handle dynamic fast/full switching yet.  */
    vpc = sh64_media_pbb_begin (current_cpu, STATE_RUN_FAST_P (CPU_STATE (current_cpu)));
#else
    vpc = sh64_media_pbb_begin (current_cpu, 0);
#endif
#endif
#endif
  }

  return vpc;
#undef FLD
}

/* add: add $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,add) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* addl: add.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,addl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* addi: addi $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,addi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* addil: addi.l $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,addil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (ADDSI (EXTSISI (FLD (f_disp10)), SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* addzl: addz.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,addzl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTSIDI (ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* alloco: alloco $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,alloco) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* and: and $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,and) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* andc: andc $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,andc) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), INVDI (GET_H_GR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* andi: andi $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,andi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* beq: beq$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,beq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (EQDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* beqi: beqi$likely $rm, $imm6, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,beqi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beqi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (EQDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm6)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bge: bge$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bge) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (GEDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bgeu: bgeu$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bgeu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (GEUDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bgt: bgt$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (GTDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bgtu: bgtu$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bgtu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (GTUDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* blink: blink $trb, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,blink) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_blink.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = ORDI (ADDDI (pc, 4), 1);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
  {
    UDI opval = CPU (h_tr[FLD (f_trb)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
if (EQSI (FLD (f_dest), 63)) {
((void) 0); /*nop*/
} else {
((void) 0); /*nop*/
}
}

  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bne: bne$likely $rm, $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (NEDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* bnei: bnei$likely $rm, $imm6, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,bnei) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beqi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
if (NEDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm6)))) {
  {
    UDI opval = CPU (h_tr[FLD (f_tra)]);
    SEM_BRANCH_VIA_ADDR (current_cpu, sem_arg, opval, vpc);
    written |= (1 << 3);
    TRACE_RESULT (current_cpu, abuf, "pc", 'D', opval);
  }
}
}

  abuf->written = written;
  SEM_BRANCH_FINI (vpc);
  return vpc;
#undef FLD
}

/* brk: brk */

static SEM_PC
SEM_FN_NAME (sh64_media,brk) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

sh64_break (current_cpu, pc);

  return vpc;
#undef FLD
}

/* byterev: byterev $rm, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,byterev) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_source;
  DI tmp_result;
  tmp_source = GET_H_GR (FLD (f_left));
  tmp_result = 0;
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
{
  tmp_result = ORDI (SLLDI (tmp_result, 8), ANDDI (tmp_source, 255));
  tmp_source = SRLDI (tmp_source, 8);
}
  {
    DI opval = tmp_result;
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* cmpeq: cmpeq $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,cmpeq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((EQDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* cmpgt: cmpgt $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,cmpgt) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((GTDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* cmpgtu: cmpgtu $rm,$rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,cmpgtu) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((GTUDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* cmveq: cmveq $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,cmveq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQDI (GET_H_GR (FLD (f_left)), 0)) {
  {
    DI opval = GET_H_GR (FLD (f_right));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* cmvne: cmvne $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,cmvne) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NEDI (GET_H_GR (FLD (f_left)), 0)) {
  {
    DI opval = GET_H_GR (FLD (f_right));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* fabsd: fabs.d $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fabsd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fabsd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fabss: fabs.s $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fabss) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fabss (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* faddd: fadd.d $drg, $drh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,faddd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_faddd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fadds: fadd.s $frg, $frh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fadds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fadds (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpeqd: fcmpeq.d $drg, $drh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpeqd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpeqd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpeqs: fcmpeq.s $frg, $frh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpeqs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpeqs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpged: fcmpge.d $drg, $drh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpged) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpged (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpges: fcmpge.s $frg, $frh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpges) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpges (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpgtd: fcmpgt.d $drg, $drh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpgtd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpgtd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpgts: fcmpgt.s $frg, $frh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpgts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpgts (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpund: fcmpun.d $drg, $drh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpund) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpund (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcmpuns: fcmpun.s $frg, $frh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fcmpuns) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpuns (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fcnvds: fcnv.ds $drgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fcnvds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fcnvds (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fcnvsd: fcnv.sd $frgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fcnvsd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fcnvsd (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdivd: fdiv.d $drg, $drh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fdivd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fdivd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdivs: fdiv.s $frg, $frh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fdivs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fdivs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fgetscr: fgetscr $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fgetscr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = SUBWORDSISF (CPU (h_fpscr));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fiprs: fipr.s $fvg, $fvh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fiprs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FV (FLD (f_left));
    SET_H_FV (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "fv", 'f', opval);
  }
  {
    SF opval = GET_H_FV (FLD (f_right));
    SET_H_FV (FLD (f_right), opval);
    TRACE_RESULT (current_cpu, abuf, "fv", 'f', opval);
  }
  {
    SF opval = sh64_fiprs (current_cpu, FLD (f_left), FLD (f_right));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }
}

  return vpc;
#undef FLD
}

/* fldd: fld.d $rm, $disp10x8, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fldd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x8)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fldp: fld.p $rm, $disp10x8, $fpf */

static SEM_PC
SEM_FN_NAME (sh64_media,fldp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fldp (current_cpu, pc, GET_H_GR (FLD (f_left)), FLD (f_disp10x8), FLD (f_dest));
}

  return vpc;
#undef FLD
}

/* flds: fld.s $rm, $disp10x4, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,flds) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_flds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = GETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x4)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fldxd: fldx.d $rm, $rn, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fldxd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fldxp: fldx.p $rm, $rn, $fpf */

static SEM_PC
SEM_FN_NAME (sh64_media,fldxp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fldp (current_cpu, pc, GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)), FLD (f_dest));
}

  return vpc;
#undef FLD
}

/* fldxs: fldx.s $rm, $rn, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fldxs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = GETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* floatld: float.ld $frgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,floatld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_floatld (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* floatls: float.ls $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,floatls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_floatls (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* floatqd: float.qd $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,floatqd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_floatqd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* floatqs: float.qs $drgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,floatqs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_floatqs (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmacs: fmac.s $frg, $frh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmacs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fadds (current_cpu, CPU (h_fr[FLD (f_dest)]), sh64_fmuls (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovd: fmov.d $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_left_right));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovdq: fmov.dq $drgh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovdq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SUBWORDDFDI (GET_H_DR (FLD (f_left_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fmovls: fmov.ls $rm, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = SUBWORDSISF (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovqd: fmov.qd $rm, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovqd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = SUBWORDDIDF (GET_H_GR (FLD (f_left)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovs: fmov.s $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_left_right)]);
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmovsl: fmov.sl $frgh, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,fmovsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SUBWORDSFSI (CPU (h_fr[FLD (f_left_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* fmuld: fmul.d $drg, $drh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmuld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fmuld (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmuls: fmul.s $frg, $frh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fmuls) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fmuls (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fnegd: fneg.d $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fnegd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fnegd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fnegs: fneg.s $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fnegs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fnegs (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fputscr: fputscr $frgh */

static SEM_PC
SEM_FN_NAME (sh64_media,fputscr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBWORDSFSI (CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fpscr) = opval;
    TRACE_RESULT (current_cpu, abuf, "fpscr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fsqrtd: fsqrt.d $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fsqrtd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fsqrtd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsqrts: fsqrt.s $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fsqrts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fsqrts (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fstd: fst.d $rm, $disp10x8, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fstd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_dest));
    SETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x8)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fstp: fst.p $rm, $disp10x8, $fpf */

static SEM_PC
SEM_FN_NAME (sh64_media,fstp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fstp (current_cpu, pc, GET_H_GR (FLD (f_left)), FLD (f_disp10x8), FLD (f_dest));
}

  return vpc;
#undef FLD
}

/* fsts: fst.s $rm, $disp10x4, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fsts) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_flds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_dest)]);
    SETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fstxd: fstx.d $rm, $rn, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fstxd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_dest));
    SETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fstxp: fstx.p $rm, $rn, $fpf */

static SEM_PC
SEM_FN_NAME (sh64_media,fstxp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fstp (current_cpu, pc, GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)), FLD (f_dest));
}

  return vpc;
#undef FLD
}

/* fstxs: fstx.s $rm, $rn, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fstxs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_dest)]);
    SETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsubd: fsub.d $drg, $drh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,fsubd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fsubd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsubs: fsub.s $frg, $frh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,fsubs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fsubs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrcdl: ftrc.dl $drgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,ftrcdl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_ftrcdl (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrcsl: ftrc.sl $frgh, $frf */

static SEM_PC
SEM_FN_NAME (sh64_media,ftrcsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_ftrcsl (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrcdq: ftrc.dq $drgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,ftrcdq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_ftrcdq (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrcsq: ftrc.sq $frgh, $drf */

static SEM_PC
SEM_FN_NAME (sh64_media,ftrcsq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fabsd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_ftrcsq (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrvs: ftrv.s $mtrxg, $fvh, $fvf */

static SEM_PC
SEM_FN_NAME (sh64_media,ftrvs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FMTX (FLD (f_left));
    SET_H_FMTX (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "fmtx", 'f', opval);
  }
  {
    SF opval = GET_H_FV (FLD (f_right));
    SET_H_FV (FLD (f_right), opval);
    TRACE_RESULT (current_cpu, abuf, "fv", 'f', opval);
  }
  {
    SF opval = GET_H_FV (FLD (f_dest));
    SET_H_FV (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fv", 'f', opval);
  }
sh64_ftrvs (current_cpu, FLD (f_left), FLD (f_right), FLD (f_dest));
}

  return vpc;
#undef FLD
}

/* getcfg: getcfg $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,getcfg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
  tmp_address = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
((void) 0); /*nop*/
  {
    DI opval = GETMEMSI (current_cpu, pc, tmp_address);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* getcon: getcon $crk, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,getcon) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_CR (FLD (f_left));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* gettr: gettr $trb, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,gettr) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_blink.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = CPU (h_tr[FLD (f_trb)]);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* icbi: icbi $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,icbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* ldb: ld.b $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldl: ld.l $rm, $disp10x4, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_flds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x4)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldq: ld.q $rm, $disp10x8, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x8))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldub: ld.ub $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* lduw: ld.uw $rm, $disp10x2, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,lduw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lduw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldw: ld.w $rm, $disp10x2, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lduw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldhil: ldhi.l $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldhil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  SI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = ADDDI (ANDDI (tmp_addr, 3), 1);
  tmp_val = 0;
if (ANDQI (tmp_bytecount, 4)) {
  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -4)));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4))));
}
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
  {
    DI opval = EXTSIDI (tmp_val);
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
} else {
{
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4))));
}
  {
    DI opval = EXTSIDI (SLLSI (tmp_val, SUBSI (32, MULSI (8, tmp_bytecount))));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldhiq: ldhi.q $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldhiq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = ADDDI (ANDDI (tmp_addr, 7), 1);
  tmp_val = 0;
if (ANDQI (tmp_bytecount, 8)) {
  {
    DI opval = GETMEMDI (current_cpu, pc, ANDDI (tmp_addr, -8));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
if (ANDQI (tmp_bytecount, 4)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 32), ZEXTSIDI (GETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -8))));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4))));
}
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
  {
    DI opval = tmp_val;
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
} else {
{
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4))));
}
if (ANDQI (tmp_bytecount, 4)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 32), ZEXTSIDI (GETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -8))));
}
  {
    DI opval = SLLDI (tmp_val, SUBSI (64, MULSI (8, tmp_bytecount)));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldlol: ldlo.l $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldlol) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  SI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = SUBSI (4, ANDDI (tmp_addr, 3));
  tmp_val = 0;
if (ANDQI (tmp_bytecount, 4)) {
  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, tmp_addr));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2))));
}
  {
    DI opval = EXTSIDI (SLLSI (tmp_val, SUBSI (32, MULSI (8, tmp_bytecount))));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
} else {
{
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2))));
}
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDSI (SLLSI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
  {
    DI opval = EXTSIDI (tmp_val);
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldloq: ldlo.q $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldloq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = SUBSI (8, ANDDI (tmp_addr, 7));
  tmp_val = 0;
if (ANDQI (tmp_bytecount, 8)) {
  {
    DI opval = GETMEMDI (current_cpu, pc, tmp_addr);
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2))));
}
if (ANDQI (tmp_bytecount, 4)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 32), ZEXTSIDI (GETMEMSI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 3), -4))));
}
  {
    DI opval = SLLDI (tmp_val, SUBSI (64, MULSI (8, tmp_bytecount)));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
} else {
{
if (ANDQI (tmp_bytecount, 4)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 32), ZEXTSIDI (GETMEMSI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 3), -4))));
}
if (ANDQI (tmp_bytecount, 2)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 16), ZEXTHIDI (GETMEMHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2))));
}
if (ANDQI (tmp_bytecount, 1)) {
  tmp_val = ADDDI (SLLDI (tmp_val, 8), ZEXTQIDI (GETMEMQI (current_cpu, pc, tmp_addr)));
}
  {
    DI opval = tmp_val;
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}
}
}
}

  abuf->written = written;
  return vpc;
#undef FLD
}

/* ldxb: ldx.b $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldxl: ldx.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldxq: ldx.q $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldxub: ldx.ub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTQIDI (GETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldxuw: ldx.uw $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxuw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTHIDI (GETMEMUHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ldxw: ldx.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ldxw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mabsl: mabs.l $rm, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mabsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ABSSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1));
  tmp_result1 = ABSSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mabsw: mabs.w $rm, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mabsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ABSHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3));
  tmp_result1 = ABSHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2));
  tmp_result2 = ABSHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1));
  tmp_result3 = ABSHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* maddl: madd.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,maddl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1));
  tmp_result1 = ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SUBWORDDISI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* maddw: madd.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,maddw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ADDHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3));
  tmp_result1 = ADDHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2));
  tmp_result2 = ADDHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1));
  tmp_result3 = ADDHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* maddsl: madds.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,maddsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ((LTDI (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, SUBSI (32, 1)))) ? (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result1 = ((LTDI (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, SUBSI (32, 1)))) ? (ADDDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0)))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* maddsub: madds.ub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,maddsub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result1 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result2 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result3 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result4 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result5 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result6 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result7 = ((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), MAKEDI (0, 0))) ? (0) : (((LTDI (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, 8))) ? (ADDDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0)))) : (SUBQI (SLLQI (1, 8), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* maddsw: madds.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,maddsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3))), SLLDI (1, SUBSI (16, 1)))) ? (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result1 = ((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2))), SLLDI (1, SUBSI (16, 1)))) ? (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result2 = ((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, SUBSI (16, 1)))) ? (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result3 = ((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, SUBSI (16, 1)))) ? (ADDDI (EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), EXTHIDI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpeqb: mcmpeq.b $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpeqb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))) ? (INVQI (0)) : (0));
  tmp_result1 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))) ? (INVQI (0)) : (0));
  tmp_result2 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))) ? (INVQI (0)) : (0));
  tmp_result3 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))) ? (INVQI (0)) : (0));
  tmp_result4 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))) ? (INVQI (0)) : (0));
  tmp_result5 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))) ? (INVQI (0)) : (0));
  tmp_result6 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))) ? (INVQI (0)) : (0));
  tmp_result7 = ((EQQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))) ? (INVQI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpeql: mcmpeq.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpeql) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ((EQSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))) ? (INVSI (0)) : (0));
  tmp_result1 = ((EQSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))) ? (INVSI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpeqw: mcmpeq.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpeqw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ((EQHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3))) ? (INVHI (0)) : (0));
  tmp_result1 = ((EQHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2))) ? (INVHI (0)) : (0));
  tmp_result2 = ((EQHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1))) ? (INVHI (0)) : (0));
  tmp_result3 = ((EQHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0))) ? (INVHI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpgtl: mcmpgt.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpgtl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ((GTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))) ? (INVSI (0)) : (0));
  tmp_result1 = ((GTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))) ? (INVSI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpgtub: mcmpgt.ub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpgtub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))) ? (INVQI (0)) : (0));
  tmp_result1 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))) ? (INVQI (0)) : (0));
  tmp_result2 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))) ? (INVQI (0)) : (0));
  tmp_result3 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))) ? (INVQI (0)) : (0));
  tmp_result4 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))) ? (INVQI (0)) : (0));
  tmp_result5 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))) ? (INVQI (0)) : (0));
  tmp_result6 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))) ? (INVQI (0)) : (0));
  tmp_result7 = ((GTUQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))) ? (INVQI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmpgtw: mcmpgt.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmpgtw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ((GTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3))) ? (INVHI (0)) : (0));
  tmp_result1 = ((GTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2))) ? (INVHI (0)) : (0));
  tmp_result2 = ((GTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1))) ? (INVHI (0)) : (0));
  tmp_result3 = ((GTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0))) ? (INVHI (0)) : (0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcmv: mcmv $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcmv) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (ANDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), ANDDI (GET_H_GR (FLD (f_dest)), INVDI (GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mcnvslw: mcnvs.lw $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcnvslw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SLLDI (1, SUBSI (16, 1)))) ? (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result1 = ((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SLLDI (1, SUBSI (16, 1)))) ? (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result2 = ((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1), SLLDI (1, SUBSI (16, 1)))) ? (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result3 = ((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0), SLLDI (1, SUBSI (16, 1)))) ? (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcnvswb: mcnvs.wb $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcnvswb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result1 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result2 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result3 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result4 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result5 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result6 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  tmp_result7 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0), NEGDI (SLLDI (1, SUBSI (8, 1))))) ? (NEGQI (SLLQI (1, SUBSI (8, 1)))) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0), SLLDI (1, SUBSI (8, 1)))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)) : (SUBQI (SLLQI (1, SUBSI (8, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mcnvswub: mcnvs.wub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mcnvswub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result1 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result2 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result3 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result4 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result5 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result6 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result7 = ((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0), MAKEDI (0, 0))) ? (0) : (((LTHI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0), SLLDI (1, 8))) ? (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)) : (SUBQI (SLLQI (1, 8), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr1: mextr1 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr1) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 1);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 1));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr2: mextr2 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr2) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 2);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 2));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr3: mextr3 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr3) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 3);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 3));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr4: mextr4 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr4) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 4);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 4));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr5: mextr5 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr5) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 5);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 5));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr6: mextr6 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr6) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 6);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 6));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mextr7: mextr7 $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mextr7) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_count;
  DI tmp_mask;
  DI tmp_rhs;
  tmp_count = MULQI (8, 7);
  tmp_mask = SLLDI (INVSI (0), tmp_count);
  tmp_rhs = SRLDI (ANDDI (GET_H_GR (FLD (f_left)), tmp_mask), tmp_count);
  tmp_count = MULQI (8, SUBQI (8, 7));
  tmp_mask = SRLDI (INVSI (0), tmp_count);
  {
    DI opval = ORDI (tmp_rhs, SLLDI (ANDDI (GET_H_GR (FLD (f_right)), tmp_mask), tmp_count));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmacfxwl: mmacfx.wl $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmacfxwl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_temp;
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 1);
  tmp_result1 = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 0);
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)));
  tmp_temp = ((LTDI (SLLDI (tmp_temp, 1), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (tmp_temp, 1), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (tmp_temp, 1)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result0 = ((LTDI (ADDDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (ADDDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp)), SLLDI (1, SUBSI (32, 1)))) ? (ADDDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)));
  tmp_temp = ((LTDI (SLLDI (tmp_temp, 1), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (tmp_temp, 1), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (tmp_temp, 1)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result1 = ((LTDI (ADDDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (ADDDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp)), SLLDI (1, SUBSI (32, 1)))) ? (ADDDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmacnfx.wl: mmacnfx.wl $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmacnfx_wl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_temp;
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 1);
  tmp_result1 = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 0);
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)));
  tmp_temp = ((LTDI (SLLDI (tmp_temp, 1), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (tmp_temp, 1), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (tmp_temp, 1)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result0 = ((LTDI (SUBDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SUBDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp)), SLLDI (1, SUBSI (32, 1)))) ? (SUBDI (EXTSIDI (tmp_result0), EXTSIDI (tmp_temp))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)));
  tmp_temp = ((LTDI (SLLDI (tmp_temp, 1), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (tmp_temp, 1), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (tmp_temp, 1)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result1 = ((LTDI (SUBDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SUBDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp)), SLLDI (1, SUBSI (32, 1)))) ? (SUBDI (EXTSIDI (tmp_result1), EXTSIDI (tmp_temp))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmull: mmul.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmull) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = MULSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1));
  tmp_result1 = MULSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SUBWORDDISI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulw: mmul.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = MULHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3));
  tmp_result1 = MULHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2));
  tmp_result2 = MULHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1));
  tmp_result3 = MULHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulfxl: mmulfx.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulfxl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_temp;
  SI tmp_result0;
  SI tmp_result1;
  tmp_temp = MULDI (ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
  tmp_result0 = ((LTDI (SRADI (tmp_temp, 31), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SRADI (tmp_temp, 31), SLLDI (1, SUBSI (32, 1)))) ? (SRADI (tmp_temp, 31)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_temp = MULDI (ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0)));
  tmp_result1 = ((LTDI (SRADI (tmp_temp, 31), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SRADI (tmp_temp, 31), SLLDI (1, SUBSI (32, 1)))) ? (SRADI (tmp_temp, 31)) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulfxw: mmulfx.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulfxw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_temp;
  HI tmp_result0;
  HI tmp_result1;
  HI tmp_result2;
  HI tmp_result3;
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)));
  tmp_result0 = ((LTSI (SRASI (tmp_temp, 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (tmp_temp, 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (tmp_temp, 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)));
  tmp_result1 = ((LTSI (SRASI (tmp_temp, 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (tmp_temp, 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (tmp_temp, 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)));
  tmp_result2 = ((LTSI (SRASI (tmp_temp, 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (tmp_temp, 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (tmp_temp, 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)));
  tmp_result3 = ((LTSI (SRASI (tmp_temp, 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (tmp_temp, 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (tmp_temp, 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulfxrpw: mmulfxrp.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulfxrpw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_temp;
  HI tmp_result0;
  HI tmp_result1;
  HI tmp_result2;
  HI tmp_result3;
  HI tmp_c;
  tmp_c = SLLSI (1, 14);
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)));
  tmp_result0 = ((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (ADDSI (tmp_temp, tmp_c), 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)));
  tmp_result1 = ((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (ADDSI (tmp_temp, tmp_c), 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)));
  tmp_result2 = ((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (ADDSI (tmp_temp, tmp_c), 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_temp = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)));
  tmp_result3 = ((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTSI (SRASI (ADDSI (tmp_temp, tmp_c), 15), SLLDI (1, SUBSI (16, 1)))) ? (SRASI (ADDSI (tmp_temp, tmp_c), 15)) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulhiwl: mmulhi.wl $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulhiwl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1)));
  tmp_result1 = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmullowl: mmullo.wl $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmullowl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3)));
  tmp_result1 = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2)));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mmulsumwq: mmulsum.wq $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mmulsumwq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_acc;
  tmp_acc = MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0)));
  tmp_acc = ADDDI (tmp_acc, MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1))));
  tmp_acc = ADDDI (tmp_acc, MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2))));
  tmp_acc = ADDDI (tmp_acc, MULSI (ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3)), ZEXTHISI (SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3))));
  {
    DI opval = ADDDI (GET_H_GR (FLD (f_dest)), tmp_acc);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* movi: movi $imm16, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,movi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (FLD (f_imm16));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mpermw: mperm.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mpermw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_control;
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_control = ANDQI (GET_H_GR (FLD (f_right)), 255);
  tmp_result0 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), SUBSI (3, ANDQI (tmp_control, 3)));
  tmp_result1 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), SUBSI (3, ANDQI (SRLQI (tmp_control, 2), 3)));
  tmp_result2 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), SUBSI (3, ANDQI (SRLQI (tmp_control, 4), 3)));
  tmp_result3 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), SUBSI (3, ANDQI (SRLQI (tmp_control, 6), 3)));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msadubq: msad.ubq $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msadubq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_acc;
  tmp_acc = ABSDI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0)));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))));
  tmp_acc = ADDDI (tmp_acc, ABSQI (SUBQI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7), SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))));
  {
    DI opval = ADDDI (GET_H_GR (FLD (f_dest)), tmp_acc);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshaldsl: mshalds.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshaldsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ((LTDI (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31)), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result1 = ((LTDI (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31)), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31)), SLLDI (1, SUBSI (32, 1)))) ? (SLLDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshaldsw: mshalds.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshaldsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = ((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15)), SLLDI (1, SUBSI (16, 1)))) ? (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result1 = ((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15)), SLLDI (1, SUBSI (16, 1)))) ? (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result2 = ((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15)), SLLDI (1, SUBSI (16, 1)))) ? (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result3 = ((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15)), SLLDI (1, SUBSI (16, 1)))) ? (SLLDI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshardl: mshard.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshardl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31));
  tmp_result1 = SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshardw: mshard.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshardw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SRAHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result1 = SRAHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result2 = SRAHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result3 = SRAHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshardsq: mshards.q $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshardsq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((LTDI (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGDI (SLLDI (1, SUBSI (16, 1)))) : (((LTDI (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63)), SLLDI (1, SUBSI (16, 1)))) ? (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63))) : (SUBDI (SLLDI (1, SUBSI (16, 1)), 1)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mshfhib: mshfhi.b $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshfhib) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3);
  tmp_result1 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3);
  tmp_result2 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2);
  tmp_result3 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2);
  tmp_result4 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1);
  tmp_result5 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1);
  tmp_result6 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0);
  tmp_result7 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0);
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshfhil: mshfhi.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshfhil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SUBWORDDISI (GET_H_GR (FLD (f_left)), 0);
  tmp_result1 = SUBWORDDISI (GET_H_GR (FLD (f_right)), 0);
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshfhiw: mshfhi.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshfhiw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1);
  tmp_result1 = SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1);
  tmp_result2 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0);
  tmp_result3 = SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0);
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshflob: mshflo.b $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshflob) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7);
  tmp_result1 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7);
  tmp_result2 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6);
  tmp_result3 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6);
  tmp_result4 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5);
  tmp_result5 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5);
  tmp_result6 = SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4);
  tmp_result7 = SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4);
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshflol: mshflo.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshflol) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SUBWORDDISI (GET_H_GR (FLD (f_left)), 1);
  tmp_result1 = SUBWORDDISI (GET_H_GR (FLD (f_right)), 1);
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshflow: mshflo.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshflow) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3);
  tmp_result1 = SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3);
  tmp_result2 = SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2);
  tmp_result3 = SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2);
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshlldl: mshlld.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshlldl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31));
  tmp_result1 = SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshlldw: mshlld.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshlldw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SLLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result1 = SLLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result2 = SLLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result3 = SLLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshlrdl: mshlrd.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshlrdl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 31));
  tmp_result1 = SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 31));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mshlrdw: mshlrd.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mshlrdw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SRLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result1 = SRLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result2 = SRLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 15));
  tmp_result3 = SRLHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), ANDDI (GET_H_GR (FLD (f_right)), 15));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msubl: msub.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msubl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = SUBSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1));
  tmp_result1 = SUBSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0), SUBWORDDISI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msubw: msub.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msubw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  HI tmp_result3;
  HI tmp_result2;
  HI tmp_result1;
  HI tmp_result0;
  tmp_result0 = SUBHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 3), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 3));
  tmp_result1 = SUBHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 2), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 2));
  tmp_result2 = SUBHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 1), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 1));
  tmp_result3 = SUBHI (SUBWORDDIHI (GET_H_GR (FLD (f_left)), 0), SUBWORDDIHI (GET_H_GR (FLD (f_right)), 0));
  {
    DI opval = ORDI (SLLDI (ZEXTHIDI (tmp_result3), 48), ORDI (SLLDI (ZEXTHIDI (tmp_result2), 32), ORDI (SLLDI (ZEXTHIDI (tmp_result1), 16), ZEXTHIDI (tmp_result0))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msubsl: msubs.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msubsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_result1;
  SI tmp_result0;
  tmp_result0 = ((LTDI (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, SUBSI (32, 1)))) ? (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  tmp_result1 = ((LTDI (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))), NEGDI (SLLDI (1, SUBSI (32, 1))))) ? (NEGSI (SLLSI (1, SUBSI (32, 1)))) : (((LTDI (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, SUBSI (32, 1)))) ? (SUBDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 0)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 0)))) : (SUBSI (SLLSI (1, SUBSI (32, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTSIDI (tmp_result1), 32), ZEXTSIDI (tmp_result0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msubsub: msubs.ub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msubsub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result1 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result2 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result3 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result4 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result5 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result6 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1)))) : (SUBQI (SLLQI (1, 8), 1)))));
  tmp_result7 = ((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), MAKEDI (0, 0))) ? (0) : (((LTDI (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, 8))) ? (SUBDI (ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), ZEXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0)))) : (SUBQI (SLLQI (1, 8), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* msubsw: msubs.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,msubsw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  QI tmp_result7;
  QI tmp_result6;
  QI tmp_result5;
  QI tmp_result4;
  QI tmp_result3;
  QI tmp_result2;
  QI tmp_result1;
  QI tmp_result0;
  tmp_result0 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 7)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 7)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result1 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 6)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 6)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result2 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 5)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 5)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result3 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 4)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 4)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result4 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 3)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 3)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result5 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 2)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 2)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result6 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 1)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 1)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  tmp_result7 = ((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGHI (SLLHI (1, SUBSI (16, 1)))) : (((LTDI (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0))), SLLDI (1, SUBSI (16, 1)))) ? (SUBDI (EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_left)), 0)), EXTQIDI (SUBWORDDIQI (GET_H_GR (FLD (f_right)), 0)))) : (SUBHI (SLLHI (1, SUBSI (16, 1)), 1)))));
  {
    DI opval = ORDI (SLLDI (ZEXTQIDI (tmp_result7), 56), ORDI (SLLDI (ZEXTQIDI (tmp_result6), 48), ORDI (SLLDI (ZEXTQIDI (tmp_result5), 40), ORDI (SLLDI (ZEXTQIDI (tmp_result4), 32), ORDI (SLLDI (ZEXTQIDI (tmp_result3), 24), ORDI (SLLDI (ZEXTQIDI (tmp_result2), 16), ORDI (SLLDI (ZEXTQIDI (tmp_result1), 8), ZEXTQIDI (tmp_result0))))))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* mulsl: muls.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mulsl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* mulul: mulu.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,mulul) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* nop: nop */

static SEM_PC
SEM_FN_NAME (sh64_media,nop) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* nsb: nsb $rm, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,nsb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = sh64_nsb (current_cpu, GET_H_GR (FLD (f_left)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ocbi: ocbi $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,ocbi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* ocbp: ocbp $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,ocbp) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* ocbwb: ocbwb $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,ocbwb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* or: or $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,or) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ori: ori $rm, $imm10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,ori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* prefi: prefi $rm, $disp6x32 */

static SEM_PC
SEM_FN_NAME (sh64_media,prefi) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* pta: pta$likely $disp16, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,pta) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_pta.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = ADDSI (FLD (f_disp16), 1);
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* ptabs: ptabs$likely $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,ptabs) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = GET_H_GR (FLD (f_right));
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* ptb: ptb$likely $disp16, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,ptb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_pta.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = FLD (f_disp16);
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* ptrel: ptrel$likely $rn, $tra */

static SEM_PC
SEM_FN_NAME (sh64_media,ptrel) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = ADDDI (pc, GET_H_GR (FLD (f_right)));
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* putcfg: putcfg $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,putcfg) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  SI tmp_address;
  tmp_address = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
((void) 0); /*nop*/
  {
    SI opval = GET_H_GR (FLD (f_dest));
    SETMEMSI (current_cpu, pc, tmp_address, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* putcon: putcon $rm, $crj */

static SEM_PC
SEM_FN_NAME (sh64_media,putcon) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_CR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* rte: rte */

static SEM_PC
SEM_FN_NAME (sh64_media,rte) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* shard: shard $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shard) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shardl: shard.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shardl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shari: shari $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shari) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRADI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* sharil: shari.l $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,sharil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlld: shlld $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlld) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SLLDI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlldl: shlld.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlldl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlli: shlli $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlli) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SLLDI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shllil: shlli.l $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shllil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlrd: shlrd $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlrd) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRLDI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlrdl: shlrd.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlrdl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlri: shlri $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlri) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRLDI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shlril: shlri.l $rm, $uimm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shlril) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shari.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* shori: shori $uimm16, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,shori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_shori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (SLLDI (GET_H_GR (FLD (f_dest)), 16), ZEXTSIDI (FLD (f_uimm16)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* sleep: sleep */

static SEM_PC
SEM_FN_NAME (sh64_media,sleep) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* stb: st.b $rm, $disp10, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = ANDQI (GET_H_GR (FLD (f_dest)), 255);
    SETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stl: st.l $rm, $disp10x4, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_flds.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_dest)), 0xffffffff);
    SETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x4))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stq: st.q $rm, $disp10x8, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fldd.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x8))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* stw: st.w $rm, $disp10x2, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lduw.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = ANDHI (GET_H_GR (FLD (f_dest)), 65535);
    SETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sthil: sthi.l $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,sthil) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = ADDDI (ANDDI (tmp_addr, 3), 1);
if (ANDQI (tmp_bytecount, 4)) {
  {
    SI opval = GET_H_GR (FLD (f_dest));
    SETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -4), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
  tmp_val = GET_H_GR (FLD (f_dest));
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    HI opval = ANDHI (tmp_val, 65535);
    SETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
}
} else {
{
  tmp_val = SRLDI (GET_H_GR (FLD (f_dest)), SUBSI (32, MULSI (8, tmp_bytecount)));
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    HI opval = ANDHI (tmp_val, 65535);
    SETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
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

/* sthiq: sthi.q $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,sthiq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = ADDDI (ANDDI (tmp_addr, 7), 1);
if (ANDQI (tmp_bytecount, 8)) {
  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, ANDDI (tmp_addr, -8), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
  tmp_val = GET_H_GR (FLD (f_dest));
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    HI opval = ANDHI (tmp_val, 65535);
    SETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 4)) {
{
  {
    SI opval = ANDSI (tmp_val, 0xffffffff);
    SETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -8), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 32);
}
}
}
} else {
{
  tmp_val = SRLDI (GET_H_GR (FLD (f_dest)), SUBSI (64, MULSI (8, tmp_bytecount)));
if (ANDQI (tmp_bytecount, 4)) {
{
  {
    SI opval = ANDSI (tmp_val, 0xffffffff);
    SETMEMSI (current_cpu, pc, ANDDI (tmp_addr, -8), opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 32);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    HI opval = ANDHI (tmp_val, 65535);
    SETMEMHI (current_cpu, pc, ANDDI (tmp_addr, -4), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
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

/* stlol: stlo.l $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stlol) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = SUBSI (4, ANDDI (tmp_addr, 3));
if (ANDQI (tmp_bytecount, 4)) {
  {
    USI opval = GET_H_GR (FLD (f_dest));
    SETMEMUSI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
  tmp_val = SRLDI (GET_H_GR (FLD (f_dest)), SUBSI (32, MULSI (8, tmp_bytecount)));
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    UHI opval = ANDHI (tmp_val, 65535);
    SETMEMUHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
}
} else {
{
  tmp_val = GET_H_GR (FLD (f_dest));
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    UHI opval = ANDHI (tmp_val, 65535);
    SETMEMUHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2), opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
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

/* stloq: stlo.q $rm, $disp6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stloq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_getcfg.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  QI tmp_bytecount;
  DI tmp_val;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp6));
  tmp_bytecount = SUBSI (8, ANDDI (tmp_addr, 7));
if (ANDQI (tmp_bytecount, 8)) {
  {
    UDI opval = GET_H_GR (FLD (f_dest));
    SETMEMUDI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 4);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }
} else {
if (GET_H_ENDIAN ()) {
{
  tmp_val = SRLDI (GET_H_GR (FLD (f_dest)), SUBSI (64, MULSI (8, tmp_bytecount)));
if (ANDQI (tmp_bytecount, 4)) {
{
  {
    USI opval = ANDSI (tmp_val, 0xffffffff);
    SETMEMUSI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 3), -4), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 32);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    UHI opval = ANDHI (tmp_val, 65535);
    SETMEMUHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
}
} else {
{
  tmp_val = GET_H_GR (FLD (f_dest));
if (ANDQI (tmp_bytecount, 1)) {
{
  {
    UQI opval = ANDQI (tmp_val, 255);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    written |= (1 << 6);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 8);
}
}
if (ANDQI (tmp_bytecount, 2)) {
{
  {
    UHI opval = ANDHI (tmp_val, 65535);
    SETMEMUHI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 1), -2), opval);
    written |= (1 << 5);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 16);
}
}
if (ANDQI (tmp_bytecount, 4)) {
{
  {
    USI opval = ANDSI (tmp_val, 0xffffffff);
    SETMEMUSI (current_cpu, pc, ANDDI (ADDDI (tmp_addr, 3), -4), opval);
    written |= (1 << 7);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
  tmp_val = SRLDI (tmp_val, 32);
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

/* stxb: stx.b $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stxb) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = SUBWORDDIQI (GET_H_GR (FLD (f_dest)), 7);
    SETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stxl: stx.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stxl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 1);
    SETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stxq: stx.q $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stxq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* stxw: stx.w $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,stxw) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = SUBWORDDIHI (GET_H_GR (FLD (f_dest)), 3);
    SETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sub: sub $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,sub) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SUBDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* subl: sub.l $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,subl) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SUBSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* swapq: swap.q $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,swapq) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  DI tmp_addr;
  DI tmp_temp;
  tmp_addr = ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
  tmp_temp = GETMEMDI (current_cpu, pc, tmp_addr);
  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }
  {
    DI opval = tmp_temp;
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  return vpc;
#undef FLD
}

/* synci: synci */

static SEM_PC
SEM_FN_NAME (sh64_media,synci) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* synco: synco */

static SEM_PC
SEM_FN_NAME (sh64_media,synco) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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

/* trapa: trapa $rm */

static SEM_PC
SEM_FN_NAME (sh64_media,trapa) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

sh64_trapa (current_cpu, GET_H_GR (FLD (f_left)), pc);

  return vpc;
#undef FLD
}

/* xor: xor $rm, $rn, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,xor) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* xori: xori $rm, $imm6, $rd */

static SEM_PC
SEM_FN_NAME (sh64_media,xori) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_xori.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm6)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* Table of all semantic fns.  */

static const struct sem_fn_desc sem_fns[] = {
  { SH64_MEDIA_INSN_X_INVALID, SEM_FN_NAME (sh64_media,x_invalid) },
  { SH64_MEDIA_INSN_X_AFTER, SEM_FN_NAME (sh64_media,x_after) },
  { SH64_MEDIA_INSN_X_BEFORE, SEM_FN_NAME (sh64_media,x_before) },
  { SH64_MEDIA_INSN_X_CTI_CHAIN, SEM_FN_NAME (sh64_media,x_cti_chain) },
  { SH64_MEDIA_INSN_X_CHAIN, SEM_FN_NAME (sh64_media,x_chain) },
  { SH64_MEDIA_INSN_X_BEGIN, SEM_FN_NAME (sh64_media,x_begin) },
  { SH64_MEDIA_INSN_ADD, SEM_FN_NAME (sh64_media,add) },
  { SH64_MEDIA_INSN_ADDL, SEM_FN_NAME (sh64_media,addl) },
  { SH64_MEDIA_INSN_ADDI, SEM_FN_NAME (sh64_media,addi) },
  { SH64_MEDIA_INSN_ADDIL, SEM_FN_NAME (sh64_media,addil) },
  { SH64_MEDIA_INSN_ADDZL, SEM_FN_NAME (sh64_media,addzl) },
  { SH64_MEDIA_INSN_ALLOCO, SEM_FN_NAME (sh64_media,alloco) },
  { SH64_MEDIA_INSN_AND, SEM_FN_NAME (sh64_media,and) },
  { SH64_MEDIA_INSN_ANDC, SEM_FN_NAME (sh64_media,andc) },
  { SH64_MEDIA_INSN_ANDI, SEM_FN_NAME (sh64_media,andi) },
  { SH64_MEDIA_INSN_BEQ, SEM_FN_NAME (sh64_media,beq) },
  { SH64_MEDIA_INSN_BEQI, SEM_FN_NAME (sh64_media,beqi) },
  { SH64_MEDIA_INSN_BGE, SEM_FN_NAME (sh64_media,bge) },
  { SH64_MEDIA_INSN_BGEU, SEM_FN_NAME (sh64_media,bgeu) },
  { SH64_MEDIA_INSN_BGT, SEM_FN_NAME (sh64_media,bgt) },
  { SH64_MEDIA_INSN_BGTU, SEM_FN_NAME (sh64_media,bgtu) },
  { SH64_MEDIA_INSN_BLINK, SEM_FN_NAME (sh64_media,blink) },
  { SH64_MEDIA_INSN_BNE, SEM_FN_NAME (sh64_media,bne) },
  { SH64_MEDIA_INSN_BNEI, SEM_FN_NAME (sh64_media,bnei) },
  { SH64_MEDIA_INSN_BRK, SEM_FN_NAME (sh64_media,brk) },
  { SH64_MEDIA_INSN_BYTEREV, SEM_FN_NAME (sh64_media,byterev) },
  { SH64_MEDIA_INSN_CMPEQ, SEM_FN_NAME (sh64_media,cmpeq) },
  { SH64_MEDIA_INSN_CMPGT, SEM_FN_NAME (sh64_media,cmpgt) },
  { SH64_MEDIA_INSN_CMPGTU, SEM_FN_NAME (sh64_media,cmpgtu) },
  { SH64_MEDIA_INSN_CMVEQ, SEM_FN_NAME (sh64_media,cmveq) },
  { SH64_MEDIA_INSN_CMVNE, SEM_FN_NAME (sh64_media,cmvne) },
  { SH64_MEDIA_INSN_FABSD, SEM_FN_NAME (sh64_media,fabsd) },
  { SH64_MEDIA_INSN_FABSS, SEM_FN_NAME (sh64_media,fabss) },
  { SH64_MEDIA_INSN_FADDD, SEM_FN_NAME (sh64_media,faddd) },
  { SH64_MEDIA_INSN_FADDS, SEM_FN_NAME (sh64_media,fadds) },
  { SH64_MEDIA_INSN_FCMPEQD, SEM_FN_NAME (sh64_media,fcmpeqd) },
  { SH64_MEDIA_INSN_FCMPEQS, SEM_FN_NAME (sh64_media,fcmpeqs) },
  { SH64_MEDIA_INSN_FCMPGED, SEM_FN_NAME (sh64_media,fcmpged) },
  { SH64_MEDIA_INSN_FCMPGES, SEM_FN_NAME (sh64_media,fcmpges) },
  { SH64_MEDIA_INSN_FCMPGTD, SEM_FN_NAME (sh64_media,fcmpgtd) },
  { SH64_MEDIA_INSN_FCMPGTS, SEM_FN_NAME (sh64_media,fcmpgts) },
  { SH64_MEDIA_INSN_FCMPUND, SEM_FN_NAME (sh64_media,fcmpund) },
  { SH64_MEDIA_INSN_FCMPUNS, SEM_FN_NAME (sh64_media,fcmpuns) },
  { SH64_MEDIA_INSN_FCNVDS, SEM_FN_NAME (sh64_media,fcnvds) },
  { SH64_MEDIA_INSN_FCNVSD, SEM_FN_NAME (sh64_media,fcnvsd) },
  { SH64_MEDIA_INSN_FDIVD, SEM_FN_NAME (sh64_media,fdivd) },
  { SH64_MEDIA_INSN_FDIVS, SEM_FN_NAME (sh64_media,fdivs) },
  { SH64_MEDIA_INSN_FGETSCR, SEM_FN_NAME (sh64_media,fgetscr) },
  { SH64_MEDIA_INSN_FIPRS, SEM_FN_NAME (sh64_media,fiprs) },
  { SH64_MEDIA_INSN_FLDD, SEM_FN_NAME (sh64_media,fldd) },
  { SH64_MEDIA_INSN_FLDP, SEM_FN_NAME (sh64_media,fldp) },
  { SH64_MEDIA_INSN_FLDS, SEM_FN_NAME (sh64_media,flds) },
  { SH64_MEDIA_INSN_FLDXD, SEM_FN_NAME (sh64_media,fldxd) },
  { SH64_MEDIA_INSN_FLDXP, SEM_FN_NAME (sh64_media,fldxp) },
  { SH64_MEDIA_INSN_FLDXS, SEM_FN_NAME (sh64_media,fldxs) },
  { SH64_MEDIA_INSN_FLOATLD, SEM_FN_NAME (sh64_media,floatld) },
  { SH64_MEDIA_INSN_FLOATLS, SEM_FN_NAME (sh64_media,floatls) },
  { SH64_MEDIA_INSN_FLOATQD, SEM_FN_NAME (sh64_media,floatqd) },
  { SH64_MEDIA_INSN_FLOATQS, SEM_FN_NAME (sh64_media,floatqs) },
  { SH64_MEDIA_INSN_FMACS, SEM_FN_NAME (sh64_media,fmacs) },
  { SH64_MEDIA_INSN_FMOVD, SEM_FN_NAME (sh64_media,fmovd) },
  { SH64_MEDIA_INSN_FMOVDQ, SEM_FN_NAME (sh64_media,fmovdq) },
  { SH64_MEDIA_INSN_FMOVLS, SEM_FN_NAME (sh64_media,fmovls) },
  { SH64_MEDIA_INSN_FMOVQD, SEM_FN_NAME (sh64_media,fmovqd) },
  { SH64_MEDIA_INSN_FMOVS, SEM_FN_NAME (sh64_media,fmovs) },
  { SH64_MEDIA_INSN_FMOVSL, SEM_FN_NAME (sh64_media,fmovsl) },
  { SH64_MEDIA_INSN_FMULD, SEM_FN_NAME (sh64_media,fmuld) },
  { SH64_MEDIA_INSN_FMULS, SEM_FN_NAME (sh64_media,fmuls) },
  { SH64_MEDIA_INSN_FNEGD, SEM_FN_NAME (sh64_media,fnegd) },
  { SH64_MEDIA_INSN_FNEGS, SEM_FN_NAME (sh64_media,fnegs) },
  { SH64_MEDIA_INSN_FPUTSCR, SEM_FN_NAME (sh64_media,fputscr) },
  { SH64_MEDIA_INSN_FSQRTD, SEM_FN_NAME (sh64_media,fsqrtd) },
  { SH64_MEDIA_INSN_FSQRTS, SEM_FN_NAME (sh64_media,fsqrts) },
  { SH64_MEDIA_INSN_FSTD, SEM_FN_NAME (sh64_media,fstd) },
  { SH64_MEDIA_INSN_FSTP, SEM_FN_NAME (sh64_media,fstp) },
  { SH64_MEDIA_INSN_FSTS, SEM_FN_NAME (sh64_media,fsts) },
  { SH64_MEDIA_INSN_FSTXD, SEM_FN_NAME (sh64_media,fstxd) },
  { SH64_MEDIA_INSN_FSTXP, SEM_FN_NAME (sh64_media,fstxp) },
  { SH64_MEDIA_INSN_FSTXS, SEM_FN_NAME (sh64_media,fstxs) },
  { SH64_MEDIA_INSN_FSUBD, SEM_FN_NAME (sh64_media,fsubd) },
  { SH64_MEDIA_INSN_FSUBS, SEM_FN_NAME (sh64_media,fsubs) },
  { SH64_MEDIA_INSN_FTRCDL, SEM_FN_NAME (sh64_media,ftrcdl) },
  { SH64_MEDIA_INSN_FTRCSL, SEM_FN_NAME (sh64_media,ftrcsl) },
  { SH64_MEDIA_INSN_FTRCDQ, SEM_FN_NAME (sh64_media,ftrcdq) },
  { SH64_MEDIA_INSN_FTRCSQ, SEM_FN_NAME (sh64_media,ftrcsq) },
  { SH64_MEDIA_INSN_FTRVS, SEM_FN_NAME (sh64_media,ftrvs) },
  { SH64_MEDIA_INSN_GETCFG, SEM_FN_NAME (sh64_media,getcfg) },
  { SH64_MEDIA_INSN_GETCON, SEM_FN_NAME (sh64_media,getcon) },
  { SH64_MEDIA_INSN_GETTR, SEM_FN_NAME (sh64_media,gettr) },
  { SH64_MEDIA_INSN_ICBI, SEM_FN_NAME (sh64_media,icbi) },
  { SH64_MEDIA_INSN_LDB, SEM_FN_NAME (sh64_media,ldb) },
  { SH64_MEDIA_INSN_LDL, SEM_FN_NAME (sh64_media,ldl) },
  { SH64_MEDIA_INSN_LDQ, SEM_FN_NAME (sh64_media,ldq) },
  { SH64_MEDIA_INSN_LDUB, SEM_FN_NAME (sh64_media,ldub) },
  { SH64_MEDIA_INSN_LDUW, SEM_FN_NAME (sh64_media,lduw) },
  { SH64_MEDIA_INSN_LDW, SEM_FN_NAME (sh64_media,ldw) },
  { SH64_MEDIA_INSN_LDHIL, SEM_FN_NAME (sh64_media,ldhil) },
  { SH64_MEDIA_INSN_LDHIQ, SEM_FN_NAME (sh64_media,ldhiq) },
  { SH64_MEDIA_INSN_LDLOL, SEM_FN_NAME (sh64_media,ldlol) },
  { SH64_MEDIA_INSN_LDLOQ, SEM_FN_NAME (sh64_media,ldloq) },
  { SH64_MEDIA_INSN_LDXB, SEM_FN_NAME (sh64_media,ldxb) },
  { SH64_MEDIA_INSN_LDXL, SEM_FN_NAME (sh64_media,ldxl) },
  { SH64_MEDIA_INSN_LDXQ, SEM_FN_NAME (sh64_media,ldxq) },
  { SH64_MEDIA_INSN_LDXUB, SEM_FN_NAME (sh64_media,ldxub) },
  { SH64_MEDIA_INSN_LDXUW, SEM_FN_NAME (sh64_media,ldxuw) },
  { SH64_MEDIA_INSN_LDXW, SEM_FN_NAME (sh64_media,ldxw) },
  { SH64_MEDIA_INSN_MABSL, SEM_FN_NAME (sh64_media,mabsl) },
  { SH64_MEDIA_INSN_MABSW, SEM_FN_NAME (sh64_media,mabsw) },
  { SH64_MEDIA_INSN_MADDL, SEM_FN_NAME (sh64_media,maddl) },
  { SH64_MEDIA_INSN_MADDW, SEM_FN_NAME (sh64_media,maddw) },
  { SH64_MEDIA_INSN_MADDSL, SEM_FN_NAME (sh64_media,maddsl) },
  { SH64_MEDIA_INSN_MADDSUB, SEM_FN_NAME (sh64_media,maddsub) },
  { SH64_MEDIA_INSN_MADDSW, SEM_FN_NAME (sh64_media,maddsw) },
  { SH64_MEDIA_INSN_MCMPEQB, SEM_FN_NAME (sh64_media,mcmpeqb) },
  { SH64_MEDIA_INSN_MCMPEQL, SEM_FN_NAME (sh64_media,mcmpeql) },
  { SH64_MEDIA_INSN_MCMPEQW, SEM_FN_NAME (sh64_media,mcmpeqw) },
  { SH64_MEDIA_INSN_MCMPGTL, SEM_FN_NAME (sh64_media,mcmpgtl) },
  { SH64_MEDIA_INSN_MCMPGTUB, SEM_FN_NAME (sh64_media,mcmpgtub) },
  { SH64_MEDIA_INSN_MCMPGTW, SEM_FN_NAME (sh64_media,mcmpgtw) },
  { SH64_MEDIA_INSN_MCMV, SEM_FN_NAME (sh64_media,mcmv) },
  { SH64_MEDIA_INSN_MCNVSLW, SEM_FN_NAME (sh64_media,mcnvslw) },
  { SH64_MEDIA_INSN_MCNVSWB, SEM_FN_NAME (sh64_media,mcnvswb) },
  { SH64_MEDIA_INSN_MCNVSWUB, SEM_FN_NAME (sh64_media,mcnvswub) },
  { SH64_MEDIA_INSN_MEXTR1, SEM_FN_NAME (sh64_media,mextr1) },
  { SH64_MEDIA_INSN_MEXTR2, SEM_FN_NAME (sh64_media,mextr2) },
  { SH64_MEDIA_INSN_MEXTR3, SEM_FN_NAME (sh64_media,mextr3) },
  { SH64_MEDIA_INSN_MEXTR4, SEM_FN_NAME (sh64_media,mextr4) },
  { SH64_MEDIA_INSN_MEXTR5, SEM_FN_NAME (sh64_media,mextr5) },
  { SH64_MEDIA_INSN_MEXTR6, SEM_FN_NAME (sh64_media,mextr6) },
  { SH64_MEDIA_INSN_MEXTR7, SEM_FN_NAME (sh64_media,mextr7) },
  { SH64_MEDIA_INSN_MMACFXWL, SEM_FN_NAME (sh64_media,mmacfxwl) },
  { SH64_MEDIA_INSN_MMACNFX_WL, SEM_FN_NAME (sh64_media,mmacnfx_wl) },
  { SH64_MEDIA_INSN_MMULL, SEM_FN_NAME (sh64_media,mmull) },
  { SH64_MEDIA_INSN_MMULW, SEM_FN_NAME (sh64_media,mmulw) },
  { SH64_MEDIA_INSN_MMULFXL, SEM_FN_NAME (sh64_media,mmulfxl) },
  { SH64_MEDIA_INSN_MMULFXW, SEM_FN_NAME (sh64_media,mmulfxw) },
  { SH64_MEDIA_INSN_MMULFXRPW, SEM_FN_NAME (sh64_media,mmulfxrpw) },
  { SH64_MEDIA_INSN_MMULHIWL, SEM_FN_NAME (sh64_media,mmulhiwl) },
  { SH64_MEDIA_INSN_MMULLOWL, SEM_FN_NAME (sh64_media,mmullowl) },
  { SH64_MEDIA_INSN_MMULSUMWQ, SEM_FN_NAME (sh64_media,mmulsumwq) },
  { SH64_MEDIA_INSN_MOVI, SEM_FN_NAME (sh64_media,movi) },
  { SH64_MEDIA_INSN_MPERMW, SEM_FN_NAME (sh64_media,mpermw) },
  { SH64_MEDIA_INSN_MSADUBQ, SEM_FN_NAME (sh64_media,msadubq) },
  { SH64_MEDIA_INSN_MSHALDSL, SEM_FN_NAME (sh64_media,mshaldsl) },
  { SH64_MEDIA_INSN_MSHALDSW, SEM_FN_NAME (sh64_media,mshaldsw) },
  { SH64_MEDIA_INSN_MSHARDL, SEM_FN_NAME (sh64_media,mshardl) },
  { SH64_MEDIA_INSN_MSHARDW, SEM_FN_NAME (sh64_media,mshardw) },
  { SH64_MEDIA_INSN_MSHARDSQ, SEM_FN_NAME (sh64_media,mshardsq) },
  { SH64_MEDIA_INSN_MSHFHIB, SEM_FN_NAME (sh64_media,mshfhib) },
  { SH64_MEDIA_INSN_MSHFHIL, SEM_FN_NAME (sh64_media,mshfhil) },
  { SH64_MEDIA_INSN_MSHFHIW, SEM_FN_NAME (sh64_media,mshfhiw) },
  { SH64_MEDIA_INSN_MSHFLOB, SEM_FN_NAME (sh64_media,mshflob) },
  { SH64_MEDIA_INSN_MSHFLOL, SEM_FN_NAME (sh64_media,mshflol) },
  { SH64_MEDIA_INSN_MSHFLOW, SEM_FN_NAME (sh64_media,mshflow) },
  { SH64_MEDIA_INSN_MSHLLDL, SEM_FN_NAME (sh64_media,mshlldl) },
  { SH64_MEDIA_INSN_MSHLLDW, SEM_FN_NAME (sh64_media,mshlldw) },
  { SH64_MEDIA_INSN_MSHLRDL, SEM_FN_NAME (sh64_media,mshlrdl) },
  { SH64_MEDIA_INSN_MSHLRDW, SEM_FN_NAME (sh64_media,mshlrdw) },
  { SH64_MEDIA_INSN_MSUBL, SEM_FN_NAME (sh64_media,msubl) },
  { SH64_MEDIA_INSN_MSUBW, SEM_FN_NAME (sh64_media,msubw) },
  { SH64_MEDIA_INSN_MSUBSL, SEM_FN_NAME (sh64_media,msubsl) },
  { SH64_MEDIA_INSN_MSUBSUB, SEM_FN_NAME (sh64_media,msubsub) },
  { SH64_MEDIA_INSN_MSUBSW, SEM_FN_NAME (sh64_media,msubsw) },
  { SH64_MEDIA_INSN_MULSL, SEM_FN_NAME (sh64_media,mulsl) },
  { SH64_MEDIA_INSN_MULUL, SEM_FN_NAME (sh64_media,mulul) },
  { SH64_MEDIA_INSN_NOP, SEM_FN_NAME (sh64_media,nop) },
  { SH64_MEDIA_INSN_NSB, SEM_FN_NAME (sh64_media,nsb) },
  { SH64_MEDIA_INSN_OCBI, SEM_FN_NAME (sh64_media,ocbi) },
  { SH64_MEDIA_INSN_OCBP, SEM_FN_NAME (sh64_media,ocbp) },
  { SH64_MEDIA_INSN_OCBWB, SEM_FN_NAME (sh64_media,ocbwb) },
  { SH64_MEDIA_INSN_OR, SEM_FN_NAME (sh64_media,or) },
  { SH64_MEDIA_INSN_ORI, SEM_FN_NAME (sh64_media,ori) },
  { SH64_MEDIA_INSN_PREFI, SEM_FN_NAME (sh64_media,prefi) },
  { SH64_MEDIA_INSN_PTA, SEM_FN_NAME (sh64_media,pta) },
  { SH64_MEDIA_INSN_PTABS, SEM_FN_NAME (sh64_media,ptabs) },
  { SH64_MEDIA_INSN_PTB, SEM_FN_NAME (sh64_media,ptb) },
  { SH64_MEDIA_INSN_PTREL, SEM_FN_NAME (sh64_media,ptrel) },
  { SH64_MEDIA_INSN_PUTCFG, SEM_FN_NAME (sh64_media,putcfg) },
  { SH64_MEDIA_INSN_PUTCON, SEM_FN_NAME (sh64_media,putcon) },
  { SH64_MEDIA_INSN_RTE, SEM_FN_NAME (sh64_media,rte) },
  { SH64_MEDIA_INSN_SHARD, SEM_FN_NAME (sh64_media,shard) },
  { SH64_MEDIA_INSN_SHARDL, SEM_FN_NAME (sh64_media,shardl) },
  { SH64_MEDIA_INSN_SHARI, SEM_FN_NAME (sh64_media,shari) },
  { SH64_MEDIA_INSN_SHARIL, SEM_FN_NAME (sh64_media,sharil) },
  { SH64_MEDIA_INSN_SHLLD, SEM_FN_NAME (sh64_media,shlld) },
  { SH64_MEDIA_INSN_SHLLDL, SEM_FN_NAME (sh64_media,shlldl) },
  { SH64_MEDIA_INSN_SHLLI, SEM_FN_NAME (sh64_media,shlli) },
  { SH64_MEDIA_INSN_SHLLIL, SEM_FN_NAME (sh64_media,shllil) },
  { SH64_MEDIA_INSN_SHLRD, SEM_FN_NAME (sh64_media,shlrd) },
  { SH64_MEDIA_INSN_SHLRDL, SEM_FN_NAME (sh64_media,shlrdl) },
  { SH64_MEDIA_INSN_SHLRI, SEM_FN_NAME (sh64_media,shlri) },
  { SH64_MEDIA_INSN_SHLRIL, SEM_FN_NAME (sh64_media,shlril) },
  { SH64_MEDIA_INSN_SHORI, SEM_FN_NAME (sh64_media,shori) },
  { SH64_MEDIA_INSN_SLEEP, SEM_FN_NAME (sh64_media,sleep) },
  { SH64_MEDIA_INSN_STB, SEM_FN_NAME (sh64_media,stb) },
  { SH64_MEDIA_INSN_STL, SEM_FN_NAME (sh64_media,stl) },
  { SH64_MEDIA_INSN_STQ, SEM_FN_NAME (sh64_media,stq) },
  { SH64_MEDIA_INSN_STW, SEM_FN_NAME (sh64_media,stw) },
  { SH64_MEDIA_INSN_STHIL, SEM_FN_NAME (sh64_media,sthil) },
  { SH64_MEDIA_INSN_STHIQ, SEM_FN_NAME (sh64_media,sthiq) },
  { SH64_MEDIA_INSN_STLOL, SEM_FN_NAME (sh64_media,stlol) },
  { SH64_MEDIA_INSN_STLOQ, SEM_FN_NAME (sh64_media,stloq) },
  { SH64_MEDIA_INSN_STXB, SEM_FN_NAME (sh64_media,stxb) },
  { SH64_MEDIA_INSN_STXL, SEM_FN_NAME (sh64_media,stxl) },
  { SH64_MEDIA_INSN_STXQ, SEM_FN_NAME (sh64_media,stxq) },
  { SH64_MEDIA_INSN_STXW, SEM_FN_NAME (sh64_media,stxw) },
  { SH64_MEDIA_INSN_SUB, SEM_FN_NAME (sh64_media,sub) },
  { SH64_MEDIA_INSN_SUBL, SEM_FN_NAME (sh64_media,subl) },
  { SH64_MEDIA_INSN_SWAPQ, SEM_FN_NAME (sh64_media,swapq) },
  { SH64_MEDIA_INSN_SYNCI, SEM_FN_NAME (sh64_media,synci) },
  { SH64_MEDIA_INSN_SYNCO, SEM_FN_NAME (sh64_media,synco) },
  { SH64_MEDIA_INSN_TRAPA, SEM_FN_NAME (sh64_media,trapa) },
  { SH64_MEDIA_INSN_XOR, SEM_FN_NAME (sh64_media,xor) },
  { SH64_MEDIA_INSN_XORI, SEM_FN_NAME (sh64_media,xori) },
  { 0, 0 }
};

/* Add the semantic fns to IDESC_TABLE.  */

void
SEM_FN_NAME (sh64_media,init_idesc_table) (SIM_CPU *current_cpu)
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
	idesc_table[sf->index].sem_fast = SEM_FN_NAME (sh64_media,x_invalid);
#else
      if (valid_p)
	idesc_table[sf->index].sem_full = sf->fn;
      else
	idesc_table[sf->index].sem_full = SEM_FN_NAME (sh64_media,x_invalid);
#endif
    }
}

