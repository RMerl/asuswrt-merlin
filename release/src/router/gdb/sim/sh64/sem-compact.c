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
SEM_FN_NAME (sh64_compact,x_invalid) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
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
SEM_FN_NAME (sh64_compact,x_after) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_COMPACT
    sh64_compact_pbb_after (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-before: --before-- */

static SEM_PC
SEM_FN_NAME (sh64_compact,x_before) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_COMPACT
    sh64_compact_pbb_before (current_cpu, sem_arg);
#endif
  }

  return vpc;
#undef FLD
}

/* x-cti-chain: --cti-chain-- */

static SEM_PC
SEM_FN_NAME (sh64_compact,x_cti_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* x-chain: --chain-- */

static SEM_PC
SEM_FN_NAME (sh64_compact,x_chain) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

  {
#if WITH_SCACHE_PBB_SH64_COMPACT
    vpc = sh64_compact_pbb_chain (current_cpu, sem_arg);
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
SEM_FN_NAME (sh64_compact,x_begin) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 0);

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

  return vpc;
#undef FLD
}

/* add-compact: add $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,add_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addi-compact: add #$imm8, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,addi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDSI (GET_H_GRC (FLD (f_rn)), EXTQISI (ANDQI (FLD (f_imm8), 255)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* addc-compact: addc $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,addc_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* addv-compact: addv $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,addv_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* and-compact: and $rm64, $rn64 */

static SEM_PC
SEM_FN_NAME (sh64_compact,and_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_rm)), GET_H_GR (FLD (f_rn)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* andi-compact: and #$uimm8, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,andi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ANDSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* andb-compact: and.b #$imm8, @(r0, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,andb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* bf-compact: bf $disp8 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bf_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bfs-compact: bf/s $disp8 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bfs_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bra-compact: bra $disp12 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bra_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bra_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* braf-compact: braf $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,braf_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* brk-compact: brk */

static SEM_PC
SEM_FN_NAME (sh64_compact,brk_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_break (current_cpu, pc);

  return vpc;
#undef FLD
}

/* bsr-compact: bsr $disp12 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bsr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bra_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bsrf-compact: bsrf $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,bsrf_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bt-compact: bt $disp8 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* bts-compact: bt/s $disp8 */

static SEM_PC
SEM_FN_NAME (sh64_compact,bts_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bf_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* clrmac-compact: clrmac */

static SEM_PC
SEM_FN_NAME (sh64_compact,clrmac_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* clrs-compact: clrs */

static SEM_PC
SEM_FN_NAME (sh64_compact,clrs_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 0;
    SET_H_SBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "sbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* clrt-compact: clrt */

static SEM_PC
SEM_FN_NAME (sh64_compact,clrt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 0;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpeq-compact: cmp/eq $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmpeq_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpeqi-compact: cmp/eq #$imm8, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmpeqi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = EQSI (GET_H_GRC (((UINT) 0)), EXTQISI (ANDQI (FLD (f_imm8), 255)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpge-compact: cmp/ge $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmpge_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GESI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpgt-compact: cmp/gt $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmpgt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmphi-compact: cmp/hi $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmphi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTUSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmphs-compact: cmp/hs $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmphs_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GEUSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmppl-compact: cmp/pl $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmppl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GTSI (GET_H_GRC (FLD (f_rn)), 0);
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmppz-compact: cmp/pz $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmppz_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = GESI (GET_H_GRC (FLD (f_rn)), 0);
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* cmpstr-compact: cmp/str $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,cmpstr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* div0s-compact: div0s $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,div0s_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* div0u-compact: div0u */

static SEM_PC
SEM_FN_NAME (sh64_compact,div0u_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* div1-compact: div1 $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,div1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* divu-compact: divu r0, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,divu_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = UDIVSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (((UINT) 0)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mulr-compact: mulr r0, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,mulr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (((UINT) 0)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* dmulsl-compact: dmuls.l $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,dmulsl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* dmulul-compact: dmulu.l $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,dmulul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* dt-compact: dt $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,dt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* extsb-compact: exts.b $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,extsb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 3));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* extsw-compact: exts.w $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,extsw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* extub-compact: extu.b $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,extub_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTQISI (SUBWORDSIQI (GET_H_GRC (FLD (f_rm)), 3));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* extuw-compact: extu.w $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,extuw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fabs-compact: fabs $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fabs_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fadd-compact: fadd $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fadd_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fcmpeq-compact: fcmp/eq $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fcmpeq_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fcmpgt-compact: fcmp/gt $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fcmpgt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fcnvds-compact: fcnvds $drn, fpul */

static SEM_PC
SEM_FN_NAME (sh64_compact,fcnvds_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fcnvds (current_cpu, GET_H_DRC (FLD (f_dn)));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fcnvsd-compact: fcnvsd fpul, $drn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fcnvsd_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DF opval = sh64_fcnvsd (current_cpu, CPU (h_fr[((UINT) 32)]));
    SET_H_DRC (FLD (f_dn), opval);
    TRACE_RESULT (current_cpu, abuf, "drc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fdiv-compact: fdiv $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fdiv_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fipr-compact: fipr $fvm, $fvn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fipr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_fipr (current_cpu, FLD (f_vm), FLD (f_vn));

  return vpc;
#undef FLD
}

/* flds-compact: flds $frn, fpul */

static SEM_PC
SEM_FN_NAME (sh64_compact,flds_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = GET_H_FRC (FLD (f_rn));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fldi0-compact: fldi0 $frn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fldi0_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fldi0 (current_cpu);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fldi1-compact: fldi1 $frn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fldi1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fldi1 (current_cpu);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* float-compact: float fpul, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,float_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmac-compact: fmac fr0, $frm, $frn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmac_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = sh64_fmacs (current_cpu, GET_H_FRC (((UINT) 0)), GET_H_FRC (FLD (f_rm)), GET_H_FRC (FLD (f_rn)));
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmov1-compact: fmov $fmovm, $fmovn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DF opval = GET_H_FMOV (FLD (f_rm));
    SET_H_FMOV (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "fmov", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmov2-compact: fmov @$rm, $fmovn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov3-compact: fmov @${rm}+, fmovn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov3_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov4-compact: fmov @(r0, $rm), $fmovn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov4_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov5-compact: fmov $fmovm, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov5_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov6-compact: fmov $fmovm, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov6_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov7-compact: fmov $fmovm, @(r0, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov7_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fmov8-compact: fmov.d @($imm12x8, $rm), $drn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmov8_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm12x8)));
    SET_H_DRC (FLD (f_dn), opval);
    TRACE_RESULT (current_cpu, abuf, "drc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmov9-compact: mov.l $drm, @($imm12x8, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmov9_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fmov9_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DRC (FLD (f_dm));
    SETMEMDF (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm12x8)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fmul-compact: fmul $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fmul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fneg-compact: fneg $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fneg_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* frchg-compact: frchg */

static SEM_PC
SEM_FN_NAME (sh64_compact,frchg_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = NOTBI (GET_H_FRBIT ());
    SET_H_FRBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "frbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fschg-compact: fschg */

static SEM_PC
SEM_FN_NAME (sh64_compact,fschg_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = NOTBI (GET_H_SZBIT ());
    SET_H_SZBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "szbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* fsqrt-compact: fsqrt $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fsqrt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* fsts-compact: fsts fpul, $frn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fsts_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = CPU (h_fr[((UINT) 32)]);
    SET_H_FRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "frc", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* fsub-compact: fsub $fsdm, $fsdn */

static SEM_PC
SEM_FN_NAME (sh64_compact,fsub_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* ftrc-compact: ftrc $fsdn, fpul */

static SEM_PC
SEM_FN_NAME (sh64_compact,ftrc_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = ((GET_H_PRBIT ()) ? (sh64_ftrcdl (current_cpu, GET_H_FSD (FLD (f_rn)))) : (sh64_ftrcsl (current_cpu, GET_H_FSD (FLD (f_rn)))));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ftrv-compact: ftrv xmtrx, $fvn */

static SEM_PC
SEM_FN_NAME (sh64_compact,ftrv_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_fipr_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_ftrv (current_cpu, FLD (f_vn));

  return vpc;
#undef FLD
}

/* jmp-compact: jmp @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,jmp_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* jsr-compact: jsr @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,jsr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* ldc-gbr-compact: ldc $rn, gbr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldc_gbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GBR (opval);
    TRACE_RESULT (current_cpu, abuf, "gbr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldc-vbr-compact: ldc $rn, vbr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldc_vbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_VBR (opval);
    TRACE_RESULT (current_cpu, abuf, "vbr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldc-sr-compact: ldc $rn, sr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldc_sr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    CPU (h_sr) = opval;
    TRACE_RESULT (current_cpu, abuf, "sr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldcl-gbr-compact: ldc.l @${rn}+, gbr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldcl_gbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* ldcl-vbr-compact: ldc.l @${rn}+, vbr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldcl_vbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* lds-fpscr-compact: lds $rn, fpscr */

static SEM_PC
SEM_FN_NAME (sh64_compact,lds_fpscr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    CPU (h_fpscr) = opval;
    TRACE_RESULT (current_cpu, abuf, "fpscr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsl-fpscr-compact: lds.l @${rn}+, fpscr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldsl_fpscr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* lds-fpul-compact: lds $rn, fpul */

static SEM_PC
SEM_FN_NAME (sh64_compact,lds_fpul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SF opval = SUBWORDSISF (GET_H_GRC (FLD (f_rn)));
    CPU (h_fr[((UINT) 32)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

  return vpc;
#undef FLD
}

/* ldsl-fpul-compact: lds.l @${rn}+, fpul */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldsl_fpul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* lds-mach-compact: lds $rn, mach */

static SEM_PC
SEM_FN_NAME (sh64_compact,lds_mach_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_MACH (opval);
    TRACE_RESULT (current_cpu, abuf, "mach", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsl-mach-compact: lds.l @${rn}+, mach */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldsl_mach_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* lds-macl-compact: lds $rn, macl */

static SEM_PC
SEM_FN_NAME (sh64_compact,lds_macl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsl-macl-compact: lds.l @${rn}+, macl */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldsl_macl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* lds-pr-compact: lds $rn, pr */

static SEM_PC
SEM_FN_NAME (sh64_compact,lds_pr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_PR (opval);
    TRACE_RESULT (current_cpu, abuf, "pr", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* ldsl-pr-compact: lds.l @${rn}+, pr */

static SEM_PC
SEM_FN_NAME (sh64_compact,ldsl_pr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* macl-compact: mac.l @${rm}+, @${rn}+ */

static SEM_PC
SEM_FN_NAME (sh64_compact,macl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* macw-compact: mac.w @${rm}+, @${rn}+ */

static SEM_PC
SEM_FN_NAME (sh64_compact,macw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* mov-compact: mov $rm64, $rn64 */

static SEM_PC
SEM_FN_NAME (sh64_compact,mov_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = GET_H_GR (FLD (f_rm));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* movi-compact: mov #$imm8, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQIDI (ANDQI (FLD (f_imm8), 255));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movi20-compact: movi20 #$imm20, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movi20_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movi20_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = FLD (f_imm20);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb1-compact: mov.b $rm, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (FLD (f_rm)), 3);
    SETMEMUQI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb2-compact: mov.b $rm, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* movb3-compact: mov.b $rm, @(r0,$rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb3_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (FLD (f_rm)), 3);
    SETMEMUQI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb4-compact: mov.b r0, @($imm8, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb4_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GBR (), FLD (f_imm8));
  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (((UINT) 0)), 3);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* movb5-compact: mov.b r0, @($imm4, $rm) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb5_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4));
  {
    UQI opval = SUBWORDSIUQI (GET_H_GRC (((UINT) 0)), 3);
    SETMEMUQI (current_cpu, pc, tmp_addr, opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* movb6-compact: mov.b @$rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb6_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb7-compact: mov.b @${rm}+, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb7_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* movb8-compact: mov.b @(r0, $rm), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm)))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb9-compact: mov.b @($imm8, gbr), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb9_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movb10-compact: mov.b @($imm4, $rm), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movb10_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movb5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTQISI (GETMEMQI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl1-compact: mov.l $rm, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl2-compact: mov.l $rm, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* movl3-compact: mov.l $rm, @(r0, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl3_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl4-compact: mov.l r0, @($imm8x4, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl4_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (((UINT) 0));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl5-compact: mov.l $rm, @($imm4x4, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl5_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm4x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl6-compact: mov.l @$rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl6_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl7-compact: mov.l @${rm}+, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl7_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* movl8-compact: mov.l @(r0, $rm), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl9-compact: mov.l @($imm8x4, gbr), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl9_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x4)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl10-compact: mov.l @($imm8x4, pc), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl10_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (FLD (f_imm8x4), ANDDI (ADDDI (pc, 4), INVSI (3))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl11-compact: mov.l @($imm4x4, $rm), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl11_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x4)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl12-compact: mov.l @($imm12x4, $rm), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl12_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm12x4)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movl13-compact: mov.l $rm, @($imm12x4, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movl13_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = GET_H_GRC (FLD (f_rm));
    SETMEMSI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rn)), FLD (f_imm12x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw1-compact: mov.w $rm, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw1_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1);
    SETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw2-compact: mov.w $rm, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* movw3-compact: mov.w $rm, @(r0, $rn) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw3_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rn))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw4-compact: mov.w r0, @($imm8x2, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw4_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (((UINT) 0)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x2)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw5-compact: mov.w r0, @($imm4x2, $rm) */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw5_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    HI opval = SUBWORDSIHI (GET_H_GRC (((UINT) 0)), 1);
    SETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x2)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw6-compact: mov.w @$rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw6_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, GET_H_GRC (FLD (f_rm))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw7-compact: mov.w @${rm}+, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw7_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* movw8-compact: mov.w @(r0, $rm), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GRC (FLD (f_rm)))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw9-compact: mov.w @($imm8x2, gbr), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw9_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GBR (), FLD (f_imm8x2))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw10-compact: mov.w @($imm8x2, pc), $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw10_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDDI (ADDDI (pc, 4), FLD (f_imm8x2))));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movw11-compact: mov.w @($imm4x2, $rm), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movw11_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw5_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = EXTHISI (GETMEMHI (current_cpu, pc, ADDSI (GET_H_GRC (FLD (f_rm)), FLD (f_imm4x2))));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mova-compact: mova @($imm8x4, pc), r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,mova_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ADDDI (ANDDI (ADDDI (pc, 4), INVSI (3)), FLD (f_imm8x4));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movcal-compact: movca.l r0, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movcal_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (((UINT) 0));
    SETMEMSI (current_cpu, pc, GET_H_GRC (FLD (f_rn)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movcol-compact: movco.l r0, @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movcol_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movt-compact: movt $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,movt_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ZEXTBISI (GET_H_TBIT ());
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movual-compact: movua.l @$rn, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movual_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = sh64_movua (current_cpu, pc, GET_H_GRC (FLD (f_rn)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* movual2-compact: movua.l @$rn+, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,movual2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* mull-compact: mul.l $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,mull_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* mulsw-compact: muls.w $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,mulsw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1)), EXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rn)), 1)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* muluw-compact: mulu.w $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,muluw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = MULSI (ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rm)), 1)), ZEXTHISI (SUBWORDSIHI (GET_H_GRC (FLD (f_rn)), 1)));
    SET_H_MACL (opval);
    TRACE_RESULT (current_cpu, abuf, "macl", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* neg-compact: neg $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,neg_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = NEGSI (GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* negc-compact: negc $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,negc_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* nop-compact: nop */

static SEM_PC
SEM_FN_NAME (sh64_compact,nop_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

((void) 0); /*nop*/

  return vpc;
#undef FLD
}

/* not-compact: not $rm64, $rn64 */

static SEM_PC
SEM_FN_NAME (sh64_compact,not_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = INVDI (GET_H_GR (FLD (f_rm)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ocbi-compact: ocbi @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,ocbi_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* ocbp-compact: ocbp @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,ocbp_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* ocbwb-compact: ocbwb @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,ocbwb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  {
    SI opval = GET_H_GRC (FLD (f_rn));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }
((void) 0); /*nop*/
}

  return vpc;
#undef FLD
}

/* or-compact: or $rm64, $rn64 */

static SEM_PC
SEM_FN_NAME (sh64_compact,or_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_rm)), GET_H_GR (FLD (f_rn)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* ori-compact: or #$uimm8, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,ori_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* orb-compact: or.b #$imm8, @(r0, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,orb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* pref-compact: pref @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,pref_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_pref (current_cpu, GET_H_GRC (FLD (f_rn)));

  return vpc;
#undef FLD
}

/* rotcl-compact: rotcl $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,rotcl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rotcr-compact: rotcr $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,rotcr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rotl-compact: rotl $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,rotl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rotr-compact: rotr $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,rotr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* rts-compact: rts */

static SEM_PC
SEM_FN_NAME (sh64_compact,rts_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* sets-compact: sets */

static SEM_PC
SEM_FN_NAME (sh64_compact,sets_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 1;
    SET_H_SBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "sbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* sett-compact: sett */

static SEM_PC
SEM_FN_NAME (sh64_compact,sett_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = 1;
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shad-compact: shad $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shad_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* shal-compact: shal $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shal_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* shar-compact: shar $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shar_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* shld-compact: shld $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shld_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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
  return vpc;
#undef FLD
}

/* shll-compact: shll $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shll_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* shll2-compact: shll2 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shll2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shll8-compact: shll8 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shll8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 8);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shll16-compact: shll16 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shll16_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SLLSI (GET_H_GRC (FLD (f_rn)), 16);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shlr-compact: shlr $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shlr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* shlr2-compact: shlr2 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shlr2_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 2);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shlr8-compact: shlr8 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shlr8_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 8);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* shlr16-compact: shlr16 $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,shlr16_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SRLSI (GET_H_GRC (FLD (f_rn)), 16);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stc-gbr-compact: stc gbr, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stc_gbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_GBR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stc-vbr-compact: stc vbr, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stc_vbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_VBR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stcl-gbr-compact: stc.l gbr, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stcl_gbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* stcl-vbr-compact: stc.l vbr, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stcl_vbr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sts-fpscr-compact: sts fpscr, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sts_fpscr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = CPU (h_fpscr);
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stsl-fpscr-compact: sts.l fpscr, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stsl_fpscr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sts-fpul-compact: sts fpul, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sts_fpul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBWORDSFSI (CPU (h_fr[((UINT) 32)]));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stsl-fpul-compact: sts.l fpul, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stsl_fpul_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sts-mach-compact: sts mach, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sts_mach_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_MACH ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stsl-mach-compact: sts.l mach, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stsl_mach_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sts-macl-compact: sts macl, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sts_macl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_MACL ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stsl-macl-compact: sts.l macl, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stsl_macl_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sts-pr-compact: sts pr, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sts_pr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = GET_H_PR ();
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* stsl-pr-compact: sts.l pr, @-$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,stsl_pr_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* sub-compact: sub $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,sub_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = SUBSI (GET_H_GRC (FLD (f_rn)), GET_H_GRC (FLD (f_rm)));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* subc-compact: subc $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,subc_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* subv-compact: subv $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,subv_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* swapb-compact: swap.b $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,swapb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* swapw-compact: swap.w $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,swapw_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (SRLSI (GET_H_GRC (FLD (f_rm)), 16), SLLSI (GET_H_GRC (FLD (f_rm)), 16));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* tasb-compact: tas.b @$rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,tasb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movw10_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* trapa-compact: trapa #$uimm8 */

static SEM_PC
SEM_FN_NAME (sh64_compact,trapa_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

sh64_compact_trapa (current_cpu, FLD (f_imm8), pc);

  return vpc;
#undef FLD
}

/* tst-compact: tst $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,tst_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ((EQSI (ANDSI (GET_H_GRC (FLD (f_rm)), GET_H_GRC (FLD (f_rn))), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* tsti-compact: tst #$uimm8, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,tsti_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    BI opval = ((EQSI (ANDSI (GET_H_GRC (((UINT) 0)), ZEXTSISI (FLD (f_imm8))), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* tstb-compact: tst.b #$imm8, @(r0, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,tstb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

{
  DI tmp_addr;
  tmp_addr = ADDSI (GET_H_GRC (((UINT) 0)), GET_H_GBR ());
  {
    BI opval = ((EQQI (ANDQI (GETMEMUQI (current_cpu, pc, tmp_addr), FLD (f_imm8)), 0)) ? (1) : (0));
    SET_H_TBIT (opval);
    TRACE_RESULT (current_cpu, abuf, "tbit", 'x', opval);
  }
}

  return vpc;
#undef FLD
}

/* xor-compact: xor $rm64, $rn64 */

static SEM_PC
SEM_FN_NAME (sh64_compact,xor_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_rn)), GET_H_GR (FLD (f_rm)));
    SET_H_GR (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

  return vpc;
#undef FLD
}

/* xori-compact: xor #$uimm8, r0 */

static SEM_PC
SEM_FN_NAME (sh64_compact,xori_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = XORSI (GET_H_GRC (((UINT) 0)), ZEXTSIDI (FLD (f_imm8)));
    SET_H_GRC (((UINT) 0), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* xorb-compact: xor.b #$imm8, @(r0, gbr) */

static SEM_PC
SEM_FN_NAME (sh64_compact,xorb_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

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

  return vpc;
#undef FLD
}

/* xtrct-compact: xtrct $rm, $rn */

static SEM_PC
SEM_FN_NAME (sh64_compact,xtrct_compact) (SIM_CPU *current_cpu, SEM_ARG sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movl12_compact.f
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_PC vpc = SEM_NEXT_VPC (sem_arg, pc, 2);

  {
    SI opval = ORSI (SLLSI (GET_H_GRC (FLD (f_rm)), 16), SRLSI (GET_H_GRC (FLD (f_rn)), 16));
    SET_H_GRC (FLD (f_rn), opval);
    TRACE_RESULT (current_cpu, abuf, "grc", 'x', opval);
  }

  return vpc;
#undef FLD
}

/* Table of all semantic fns.  */

static const struct sem_fn_desc sem_fns[] = {
  { SH64_COMPACT_INSN_X_INVALID, SEM_FN_NAME (sh64_compact,x_invalid) },
  { SH64_COMPACT_INSN_X_AFTER, SEM_FN_NAME (sh64_compact,x_after) },
  { SH64_COMPACT_INSN_X_BEFORE, SEM_FN_NAME (sh64_compact,x_before) },
  { SH64_COMPACT_INSN_X_CTI_CHAIN, SEM_FN_NAME (sh64_compact,x_cti_chain) },
  { SH64_COMPACT_INSN_X_CHAIN, SEM_FN_NAME (sh64_compact,x_chain) },
  { SH64_COMPACT_INSN_X_BEGIN, SEM_FN_NAME (sh64_compact,x_begin) },
  { SH64_COMPACT_INSN_ADD_COMPACT, SEM_FN_NAME (sh64_compact,add_compact) },
  { SH64_COMPACT_INSN_ADDI_COMPACT, SEM_FN_NAME (sh64_compact,addi_compact) },
  { SH64_COMPACT_INSN_ADDC_COMPACT, SEM_FN_NAME (sh64_compact,addc_compact) },
  { SH64_COMPACT_INSN_ADDV_COMPACT, SEM_FN_NAME (sh64_compact,addv_compact) },
  { SH64_COMPACT_INSN_AND_COMPACT, SEM_FN_NAME (sh64_compact,and_compact) },
  { SH64_COMPACT_INSN_ANDI_COMPACT, SEM_FN_NAME (sh64_compact,andi_compact) },
  { SH64_COMPACT_INSN_ANDB_COMPACT, SEM_FN_NAME (sh64_compact,andb_compact) },
  { SH64_COMPACT_INSN_BF_COMPACT, SEM_FN_NAME (sh64_compact,bf_compact) },
  { SH64_COMPACT_INSN_BFS_COMPACT, SEM_FN_NAME (sh64_compact,bfs_compact) },
  { SH64_COMPACT_INSN_BRA_COMPACT, SEM_FN_NAME (sh64_compact,bra_compact) },
  { SH64_COMPACT_INSN_BRAF_COMPACT, SEM_FN_NAME (sh64_compact,braf_compact) },
  { SH64_COMPACT_INSN_BRK_COMPACT, SEM_FN_NAME (sh64_compact,brk_compact) },
  { SH64_COMPACT_INSN_BSR_COMPACT, SEM_FN_NAME (sh64_compact,bsr_compact) },
  { SH64_COMPACT_INSN_BSRF_COMPACT, SEM_FN_NAME (sh64_compact,bsrf_compact) },
  { SH64_COMPACT_INSN_BT_COMPACT, SEM_FN_NAME (sh64_compact,bt_compact) },
  { SH64_COMPACT_INSN_BTS_COMPACT, SEM_FN_NAME (sh64_compact,bts_compact) },
  { SH64_COMPACT_INSN_CLRMAC_COMPACT, SEM_FN_NAME (sh64_compact,clrmac_compact) },
  { SH64_COMPACT_INSN_CLRS_COMPACT, SEM_FN_NAME (sh64_compact,clrs_compact) },
  { SH64_COMPACT_INSN_CLRT_COMPACT, SEM_FN_NAME (sh64_compact,clrt_compact) },
  { SH64_COMPACT_INSN_CMPEQ_COMPACT, SEM_FN_NAME (sh64_compact,cmpeq_compact) },
  { SH64_COMPACT_INSN_CMPEQI_COMPACT, SEM_FN_NAME (sh64_compact,cmpeqi_compact) },
  { SH64_COMPACT_INSN_CMPGE_COMPACT, SEM_FN_NAME (sh64_compact,cmpge_compact) },
  { SH64_COMPACT_INSN_CMPGT_COMPACT, SEM_FN_NAME (sh64_compact,cmpgt_compact) },
  { SH64_COMPACT_INSN_CMPHI_COMPACT, SEM_FN_NAME (sh64_compact,cmphi_compact) },
  { SH64_COMPACT_INSN_CMPHS_COMPACT, SEM_FN_NAME (sh64_compact,cmphs_compact) },
  { SH64_COMPACT_INSN_CMPPL_COMPACT, SEM_FN_NAME (sh64_compact,cmppl_compact) },
  { SH64_COMPACT_INSN_CMPPZ_COMPACT, SEM_FN_NAME (sh64_compact,cmppz_compact) },
  { SH64_COMPACT_INSN_CMPSTR_COMPACT, SEM_FN_NAME (sh64_compact,cmpstr_compact) },
  { SH64_COMPACT_INSN_DIV0S_COMPACT, SEM_FN_NAME (sh64_compact,div0s_compact) },
  { SH64_COMPACT_INSN_DIV0U_COMPACT, SEM_FN_NAME (sh64_compact,div0u_compact) },
  { SH64_COMPACT_INSN_DIV1_COMPACT, SEM_FN_NAME (sh64_compact,div1_compact) },
  { SH64_COMPACT_INSN_DIVU_COMPACT, SEM_FN_NAME (sh64_compact,divu_compact) },
  { SH64_COMPACT_INSN_MULR_COMPACT, SEM_FN_NAME (sh64_compact,mulr_compact) },
  { SH64_COMPACT_INSN_DMULSL_COMPACT, SEM_FN_NAME (sh64_compact,dmulsl_compact) },
  { SH64_COMPACT_INSN_DMULUL_COMPACT, SEM_FN_NAME (sh64_compact,dmulul_compact) },
  { SH64_COMPACT_INSN_DT_COMPACT, SEM_FN_NAME (sh64_compact,dt_compact) },
  { SH64_COMPACT_INSN_EXTSB_COMPACT, SEM_FN_NAME (sh64_compact,extsb_compact) },
  { SH64_COMPACT_INSN_EXTSW_COMPACT, SEM_FN_NAME (sh64_compact,extsw_compact) },
  { SH64_COMPACT_INSN_EXTUB_COMPACT, SEM_FN_NAME (sh64_compact,extub_compact) },
  { SH64_COMPACT_INSN_EXTUW_COMPACT, SEM_FN_NAME (sh64_compact,extuw_compact) },
  { SH64_COMPACT_INSN_FABS_COMPACT, SEM_FN_NAME (sh64_compact,fabs_compact) },
  { SH64_COMPACT_INSN_FADD_COMPACT, SEM_FN_NAME (sh64_compact,fadd_compact) },
  { SH64_COMPACT_INSN_FCMPEQ_COMPACT, SEM_FN_NAME (sh64_compact,fcmpeq_compact) },
  { SH64_COMPACT_INSN_FCMPGT_COMPACT, SEM_FN_NAME (sh64_compact,fcmpgt_compact) },
  { SH64_COMPACT_INSN_FCNVDS_COMPACT, SEM_FN_NAME (sh64_compact,fcnvds_compact) },
  { SH64_COMPACT_INSN_FCNVSD_COMPACT, SEM_FN_NAME (sh64_compact,fcnvsd_compact) },
  { SH64_COMPACT_INSN_FDIV_COMPACT, SEM_FN_NAME (sh64_compact,fdiv_compact) },
  { SH64_COMPACT_INSN_FIPR_COMPACT, SEM_FN_NAME (sh64_compact,fipr_compact) },
  { SH64_COMPACT_INSN_FLDS_COMPACT, SEM_FN_NAME (sh64_compact,flds_compact) },
  { SH64_COMPACT_INSN_FLDI0_COMPACT, SEM_FN_NAME (sh64_compact,fldi0_compact) },
  { SH64_COMPACT_INSN_FLDI1_COMPACT, SEM_FN_NAME (sh64_compact,fldi1_compact) },
  { SH64_COMPACT_INSN_FLOAT_COMPACT, SEM_FN_NAME (sh64_compact,float_compact) },
  { SH64_COMPACT_INSN_FMAC_COMPACT, SEM_FN_NAME (sh64_compact,fmac_compact) },
  { SH64_COMPACT_INSN_FMOV1_COMPACT, SEM_FN_NAME (sh64_compact,fmov1_compact) },
  { SH64_COMPACT_INSN_FMOV2_COMPACT, SEM_FN_NAME (sh64_compact,fmov2_compact) },
  { SH64_COMPACT_INSN_FMOV3_COMPACT, SEM_FN_NAME (sh64_compact,fmov3_compact) },
  { SH64_COMPACT_INSN_FMOV4_COMPACT, SEM_FN_NAME (sh64_compact,fmov4_compact) },
  { SH64_COMPACT_INSN_FMOV5_COMPACT, SEM_FN_NAME (sh64_compact,fmov5_compact) },
  { SH64_COMPACT_INSN_FMOV6_COMPACT, SEM_FN_NAME (sh64_compact,fmov6_compact) },
  { SH64_COMPACT_INSN_FMOV7_COMPACT, SEM_FN_NAME (sh64_compact,fmov7_compact) },
  { SH64_COMPACT_INSN_FMOV8_COMPACT, SEM_FN_NAME (sh64_compact,fmov8_compact) },
  { SH64_COMPACT_INSN_FMOV9_COMPACT, SEM_FN_NAME (sh64_compact,fmov9_compact) },
  { SH64_COMPACT_INSN_FMUL_COMPACT, SEM_FN_NAME (sh64_compact,fmul_compact) },
  { SH64_COMPACT_INSN_FNEG_COMPACT, SEM_FN_NAME (sh64_compact,fneg_compact) },
  { SH64_COMPACT_INSN_FRCHG_COMPACT, SEM_FN_NAME (sh64_compact,frchg_compact) },
  { SH64_COMPACT_INSN_FSCHG_COMPACT, SEM_FN_NAME (sh64_compact,fschg_compact) },
  { SH64_COMPACT_INSN_FSQRT_COMPACT, SEM_FN_NAME (sh64_compact,fsqrt_compact) },
  { SH64_COMPACT_INSN_FSTS_COMPACT, SEM_FN_NAME (sh64_compact,fsts_compact) },
  { SH64_COMPACT_INSN_FSUB_COMPACT, SEM_FN_NAME (sh64_compact,fsub_compact) },
  { SH64_COMPACT_INSN_FTRC_COMPACT, SEM_FN_NAME (sh64_compact,ftrc_compact) },
  { SH64_COMPACT_INSN_FTRV_COMPACT, SEM_FN_NAME (sh64_compact,ftrv_compact) },
  { SH64_COMPACT_INSN_JMP_COMPACT, SEM_FN_NAME (sh64_compact,jmp_compact) },
  { SH64_COMPACT_INSN_JSR_COMPACT, SEM_FN_NAME (sh64_compact,jsr_compact) },
  { SH64_COMPACT_INSN_LDC_GBR_COMPACT, SEM_FN_NAME (sh64_compact,ldc_gbr_compact) },
  { SH64_COMPACT_INSN_LDC_VBR_COMPACT, SEM_FN_NAME (sh64_compact,ldc_vbr_compact) },
  { SH64_COMPACT_INSN_LDC_SR_COMPACT, SEM_FN_NAME (sh64_compact,ldc_sr_compact) },
  { SH64_COMPACT_INSN_LDCL_GBR_COMPACT, SEM_FN_NAME (sh64_compact,ldcl_gbr_compact) },
  { SH64_COMPACT_INSN_LDCL_VBR_COMPACT, SEM_FN_NAME (sh64_compact,ldcl_vbr_compact) },
  { SH64_COMPACT_INSN_LDS_FPSCR_COMPACT, SEM_FN_NAME (sh64_compact,lds_fpscr_compact) },
  { SH64_COMPACT_INSN_LDSL_FPSCR_COMPACT, SEM_FN_NAME (sh64_compact,ldsl_fpscr_compact) },
  { SH64_COMPACT_INSN_LDS_FPUL_COMPACT, SEM_FN_NAME (sh64_compact,lds_fpul_compact) },
  { SH64_COMPACT_INSN_LDSL_FPUL_COMPACT, SEM_FN_NAME (sh64_compact,ldsl_fpul_compact) },
  { SH64_COMPACT_INSN_LDS_MACH_COMPACT, SEM_FN_NAME (sh64_compact,lds_mach_compact) },
  { SH64_COMPACT_INSN_LDSL_MACH_COMPACT, SEM_FN_NAME (sh64_compact,ldsl_mach_compact) },
  { SH64_COMPACT_INSN_LDS_MACL_COMPACT, SEM_FN_NAME (sh64_compact,lds_macl_compact) },
  { SH64_COMPACT_INSN_LDSL_MACL_COMPACT, SEM_FN_NAME (sh64_compact,ldsl_macl_compact) },
  { SH64_COMPACT_INSN_LDS_PR_COMPACT, SEM_FN_NAME (sh64_compact,lds_pr_compact) },
  { SH64_COMPACT_INSN_LDSL_PR_COMPACT, SEM_FN_NAME (sh64_compact,ldsl_pr_compact) },
  { SH64_COMPACT_INSN_MACL_COMPACT, SEM_FN_NAME (sh64_compact,macl_compact) },
  { SH64_COMPACT_INSN_MACW_COMPACT, SEM_FN_NAME (sh64_compact,macw_compact) },
  { SH64_COMPACT_INSN_MOV_COMPACT, SEM_FN_NAME (sh64_compact,mov_compact) },
  { SH64_COMPACT_INSN_MOVI_COMPACT, SEM_FN_NAME (sh64_compact,movi_compact) },
  { SH64_COMPACT_INSN_MOVI20_COMPACT, SEM_FN_NAME (sh64_compact,movi20_compact) },
  { SH64_COMPACT_INSN_MOVB1_COMPACT, SEM_FN_NAME (sh64_compact,movb1_compact) },
  { SH64_COMPACT_INSN_MOVB2_COMPACT, SEM_FN_NAME (sh64_compact,movb2_compact) },
  { SH64_COMPACT_INSN_MOVB3_COMPACT, SEM_FN_NAME (sh64_compact,movb3_compact) },
  { SH64_COMPACT_INSN_MOVB4_COMPACT, SEM_FN_NAME (sh64_compact,movb4_compact) },
  { SH64_COMPACT_INSN_MOVB5_COMPACT, SEM_FN_NAME (sh64_compact,movb5_compact) },
  { SH64_COMPACT_INSN_MOVB6_COMPACT, SEM_FN_NAME (sh64_compact,movb6_compact) },
  { SH64_COMPACT_INSN_MOVB7_COMPACT, SEM_FN_NAME (sh64_compact,movb7_compact) },
  { SH64_COMPACT_INSN_MOVB8_COMPACT, SEM_FN_NAME (sh64_compact,movb8_compact) },
  { SH64_COMPACT_INSN_MOVB9_COMPACT, SEM_FN_NAME (sh64_compact,movb9_compact) },
  { SH64_COMPACT_INSN_MOVB10_COMPACT, SEM_FN_NAME (sh64_compact,movb10_compact) },
  { SH64_COMPACT_INSN_MOVL1_COMPACT, SEM_FN_NAME (sh64_compact,movl1_compact) },
  { SH64_COMPACT_INSN_MOVL2_COMPACT, SEM_FN_NAME (sh64_compact,movl2_compact) },
  { SH64_COMPACT_INSN_MOVL3_COMPACT, SEM_FN_NAME (sh64_compact,movl3_compact) },
  { SH64_COMPACT_INSN_MOVL4_COMPACT, SEM_FN_NAME (sh64_compact,movl4_compact) },
  { SH64_COMPACT_INSN_MOVL5_COMPACT, SEM_FN_NAME (sh64_compact,movl5_compact) },
  { SH64_COMPACT_INSN_MOVL6_COMPACT, SEM_FN_NAME (sh64_compact,movl6_compact) },
  { SH64_COMPACT_INSN_MOVL7_COMPACT, SEM_FN_NAME (sh64_compact,movl7_compact) },
  { SH64_COMPACT_INSN_MOVL8_COMPACT, SEM_FN_NAME (sh64_compact,movl8_compact) },
  { SH64_COMPACT_INSN_MOVL9_COMPACT, SEM_FN_NAME (sh64_compact,movl9_compact) },
  { SH64_COMPACT_INSN_MOVL10_COMPACT, SEM_FN_NAME (sh64_compact,movl10_compact) },
  { SH64_COMPACT_INSN_MOVL11_COMPACT, SEM_FN_NAME (sh64_compact,movl11_compact) },
  { SH64_COMPACT_INSN_MOVL12_COMPACT, SEM_FN_NAME (sh64_compact,movl12_compact) },
  { SH64_COMPACT_INSN_MOVL13_COMPACT, SEM_FN_NAME (sh64_compact,movl13_compact) },
  { SH64_COMPACT_INSN_MOVW1_COMPACT, SEM_FN_NAME (sh64_compact,movw1_compact) },
  { SH64_COMPACT_INSN_MOVW2_COMPACT, SEM_FN_NAME (sh64_compact,movw2_compact) },
  { SH64_COMPACT_INSN_MOVW3_COMPACT, SEM_FN_NAME (sh64_compact,movw3_compact) },
  { SH64_COMPACT_INSN_MOVW4_COMPACT, SEM_FN_NAME (sh64_compact,movw4_compact) },
  { SH64_COMPACT_INSN_MOVW5_COMPACT, SEM_FN_NAME (sh64_compact,movw5_compact) },
  { SH64_COMPACT_INSN_MOVW6_COMPACT, SEM_FN_NAME (sh64_compact,movw6_compact) },
  { SH64_COMPACT_INSN_MOVW7_COMPACT, SEM_FN_NAME (sh64_compact,movw7_compact) },
  { SH64_COMPACT_INSN_MOVW8_COMPACT, SEM_FN_NAME (sh64_compact,movw8_compact) },
  { SH64_COMPACT_INSN_MOVW9_COMPACT, SEM_FN_NAME (sh64_compact,movw9_compact) },
  { SH64_COMPACT_INSN_MOVW10_COMPACT, SEM_FN_NAME (sh64_compact,movw10_compact) },
  { SH64_COMPACT_INSN_MOVW11_COMPACT, SEM_FN_NAME (sh64_compact,movw11_compact) },
  { SH64_COMPACT_INSN_MOVA_COMPACT, SEM_FN_NAME (sh64_compact,mova_compact) },
  { SH64_COMPACT_INSN_MOVCAL_COMPACT, SEM_FN_NAME (sh64_compact,movcal_compact) },
  { SH64_COMPACT_INSN_MOVCOL_COMPACT, SEM_FN_NAME (sh64_compact,movcol_compact) },
  { SH64_COMPACT_INSN_MOVT_COMPACT, SEM_FN_NAME (sh64_compact,movt_compact) },
  { SH64_COMPACT_INSN_MOVUAL_COMPACT, SEM_FN_NAME (sh64_compact,movual_compact) },
  { SH64_COMPACT_INSN_MOVUAL2_COMPACT, SEM_FN_NAME (sh64_compact,movual2_compact) },
  { SH64_COMPACT_INSN_MULL_COMPACT, SEM_FN_NAME (sh64_compact,mull_compact) },
  { SH64_COMPACT_INSN_MULSW_COMPACT, SEM_FN_NAME (sh64_compact,mulsw_compact) },
  { SH64_COMPACT_INSN_MULUW_COMPACT, SEM_FN_NAME (sh64_compact,muluw_compact) },
  { SH64_COMPACT_INSN_NEG_COMPACT, SEM_FN_NAME (sh64_compact,neg_compact) },
  { SH64_COMPACT_INSN_NEGC_COMPACT, SEM_FN_NAME (sh64_compact,negc_compact) },
  { SH64_COMPACT_INSN_NOP_COMPACT, SEM_FN_NAME (sh64_compact,nop_compact) },
  { SH64_COMPACT_INSN_NOT_COMPACT, SEM_FN_NAME (sh64_compact,not_compact) },
  { SH64_COMPACT_INSN_OCBI_COMPACT, SEM_FN_NAME (sh64_compact,ocbi_compact) },
  { SH64_COMPACT_INSN_OCBP_COMPACT, SEM_FN_NAME (sh64_compact,ocbp_compact) },
  { SH64_COMPACT_INSN_OCBWB_COMPACT, SEM_FN_NAME (sh64_compact,ocbwb_compact) },
  { SH64_COMPACT_INSN_OR_COMPACT, SEM_FN_NAME (sh64_compact,or_compact) },
  { SH64_COMPACT_INSN_ORI_COMPACT, SEM_FN_NAME (sh64_compact,ori_compact) },
  { SH64_COMPACT_INSN_ORB_COMPACT, SEM_FN_NAME (sh64_compact,orb_compact) },
  { SH64_COMPACT_INSN_PREF_COMPACT, SEM_FN_NAME (sh64_compact,pref_compact) },
  { SH64_COMPACT_INSN_ROTCL_COMPACT, SEM_FN_NAME (sh64_compact,rotcl_compact) },
  { SH64_COMPACT_INSN_ROTCR_COMPACT, SEM_FN_NAME (sh64_compact,rotcr_compact) },
  { SH64_COMPACT_INSN_ROTL_COMPACT, SEM_FN_NAME (sh64_compact,rotl_compact) },
  { SH64_COMPACT_INSN_ROTR_COMPACT, SEM_FN_NAME (sh64_compact,rotr_compact) },
  { SH64_COMPACT_INSN_RTS_COMPACT, SEM_FN_NAME (sh64_compact,rts_compact) },
  { SH64_COMPACT_INSN_SETS_COMPACT, SEM_FN_NAME (sh64_compact,sets_compact) },
  { SH64_COMPACT_INSN_SETT_COMPACT, SEM_FN_NAME (sh64_compact,sett_compact) },
  { SH64_COMPACT_INSN_SHAD_COMPACT, SEM_FN_NAME (sh64_compact,shad_compact) },
  { SH64_COMPACT_INSN_SHAL_COMPACT, SEM_FN_NAME (sh64_compact,shal_compact) },
  { SH64_COMPACT_INSN_SHAR_COMPACT, SEM_FN_NAME (sh64_compact,shar_compact) },
  { SH64_COMPACT_INSN_SHLD_COMPACT, SEM_FN_NAME (sh64_compact,shld_compact) },
  { SH64_COMPACT_INSN_SHLL_COMPACT, SEM_FN_NAME (sh64_compact,shll_compact) },
  { SH64_COMPACT_INSN_SHLL2_COMPACT, SEM_FN_NAME (sh64_compact,shll2_compact) },
  { SH64_COMPACT_INSN_SHLL8_COMPACT, SEM_FN_NAME (sh64_compact,shll8_compact) },
  { SH64_COMPACT_INSN_SHLL16_COMPACT, SEM_FN_NAME (sh64_compact,shll16_compact) },
  { SH64_COMPACT_INSN_SHLR_COMPACT, SEM_FN_NAME (sh64_compact,shlr_compact) },
  { SH64_COMPACT_INSN_SHLR2_COMPACT, SEM_FN_NAME (sh64_compact,shlr2_compact) },
  { SH64_COMPACT_INSN_SHLR8_COMPACT, SEM_FN_NAME (sh64_compact,shlr8_compact) },
  { SH64_COMPACT_INSN_SHLR16_COMPACT, SEM_FN_NAME (sh64_compact,shlr16_compact) },
  { SH64_COMPACT_INSN_STC_GBR_COMPACT, SEM_FN_NAME (sh64_compact,stc_gbr_compact) },
  { SH64_COMPACT_INSN_STC_VBR_COMPACT, SEM_FN_NAME (sh64_compact,stc_vbr_compact) },
  { SH64_COMPACT_INSN_STCL_GBR_COMPACT, SEM_FN_NAME (sh64_compact,stcl_gbr_compact) },
  { SH64_COMPACT_INSN_STCL_VBR_COMPACT, SEM_FN_NAME (sh64_compact,stcl_vbr_compact) },
  { SH64_COMPACT_INSN_STS_FPSCR_COMPACT, SEM_FN_NAME (sh64_compact,sts_fpscr_compact) },
  { SH64_COMPACT_INSN_STSL_FPSCR_COMPACT, SEM_FN_NAME (sh64_compact,stsl_fpscr_compact) },
  { SH64_COMPACT_INSN_STS_FPUL_COMPACT, SEM_FN_NAME (sh64_compact,sts_fpul_compact) },
  { SH64_COMPACT_INSN_STSL_FPUL_COMPACT, SEM_FN_NAME (sh64_compact,stsl_fpul_compact) },
  { SH64_COMPACT_INSN_STS_MACH_COMPACT, SEM_FN_NAME (sh64_compact,sts_mach_compact) },
  { SH64_COMPACT_INSN_STSL_MACH_COMPACT, SEM_FN_NAME (sh64_compact,stsl_mach_compact) },
  { SH64_COMPACT_INSN_STS_MACL_COMPACT, SEM_FN_NAME (sh64_compact,sts_macl_compact) },
  { SH64_COMPACT_INSN_STSL_MACL_COMPACT, SEM_FN_NAME (sh64_compact,stsl_macl_compact) },
  { SH64_COMPACT_INSN_STS_PR_COMPACT, SEM_FN_NAME (sh64_compact,sts_pr_compact) },
  { SH64_COMPACT_INSN_STSL_PR_COMPACT, SEM_FN_NAME (sh64_compact,stsl_pr_compact) },
  { SH64_COMPACT_INSN_SUB_COMPACT, SEM_FN_NAME (sh64_compact,sub_compact) },
  { SH64_COMPACT_INSN_SUBC_COMPACT, SEM_FN_NAME (sh64_compact,subc_compact) },
  { SH64_COMPACT_INSN_SUBV_COMPACT, SEM_FN_NAME (sh64_compact,subv_compact) },
  { SH64_COMPACT_INSN_SWAPB_COMPACT, SEM_FN_NAME (sh64_compact,swapb_compact) },
  { SH64_COMPACT_INSN_SWAPW_COMPACT, SEM_FN_NAME (sh64_compact,swapw_compact) },
  { SH64_COMPACT_INSN_TASB_COMPACT, SEM_FN_NAME (sh64_compact,tasb_compact) },
  { SH64_COMPACT_INSN_TRAPA_COMPACT, SEM_FN_NAME (sh64_compact,trapa_compact) },
  { SH64_COMPACT_INSN_TST_COMPACT, SEM_FN_NAME (sh64_compact,tst_compact) },
  { SH64_COMPACT_INSN_TSTI_COMPACT, SEM_FN_NAME (sh64_compact,tsti_compact) },
  { SH64_COMPACT_INSN_TSTB_COMPACT, SEM_FN_NAME (sh64_compact,tstb_compact) },
  { SH64_COMPACT_INSN_XOR_COMPACT, SEM_FN_NAME (sh64_compact,xor_compact) },
  { SH64_COMPACT_INSN_XORI_COMPACT, SEM_FN_NAME (sh64_compact,xori_compact) },
  { SH64_COMPACT_INSN_XORB_COMPACT, SEM_FN_NAME (sh64_compact,xorb_compact) },
  { SH64_COMPACT_INSN_XTRCT_COMPACT, SEM_FN_NAME (sh64_compact,xtrct_compact) },
  { 0, 0 }
};

/* Add the semantic fns to IDESC_TABLE.  */

void
SEM_FN_NAME (sh64_compact,init_idesc_table) (SIM_CPU *current_cpu)
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
	idesc_table[sf->index].sem_fast = SEM_FN_NAME (sh64_compact,x_invalid);
#else
      if (valid_p)
	idesc_table[sf->index].sem_full = sf->fn;
      else
	idesc_table[sf->index].sem_full = SEM_FN_NAME (sh64_compact,x_invalid);
#endif
    }
}

