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
    { SH64_MEDIA_INSN_X_INVALID, && case_sem_INSN_X_INVALID },
    { SH64_MEDIA_INSN_X_AFTER, && case_sem_INSN_X_AFTER },
    { SH64_MEDIA_INSN_X_BEFORE, && case_sem_INSN_X_BEFORE },
    { SH64_MEDIA_INSN_X_CTI_CHAIN, && case_sem_INSN_X_CTI_CHAIN },
    { SH64_MEDIA_INSN_X_CHAIN, && case_sem_INSN_X_CHAIN },
    { SH64_MEDIA_INSN_X_BEGIN, && case_sem_INSN_X_BEGIN },
    { SH64_MEDIA_INSN_ADD, && case_sem_INSN_ADD },
    { SH64_MEDIA_INSN_ADDL, && case_sem_INSN_ADDL },
    { SH64_MEDIA_INSN_ADDI, && case_sem_INSN_ADDI },
    { SH64_MEDIA_INSN_ADDIL, && case_sem_INSN_ADDIL },
    { SH64_MEDIA_INSN_ADDZL, && case_sem_INSN_ADDZL },
    { SH64_MEDIA_INSN_ALLOCO, && case_sem_INSN_ALLOCO },
    { SH64_MEDIA_INSN_AND, && case_sem_INSN_AND },
    { SH64_MEDIA_INSN_ANDC, && case_sem_INSN_ANDC },
    { SH64_MEDIA_INSN_ANDI, && case_sem_INSN_ANDI },
    { SH64_MEDIA_INSN_BEQ, && case_sem_INSN_BEQ },
    { SH64_MEDIA_INSN_BEQI, && case_sem_INSN_BEQI },
    { SH64_MEDIA_INSN_BGE, && case_sem_INSN_BGE },
    { SH64_MEDIA_INSN_BGEU, && case_sem_INSN_BGEU },
    { SH64_MEDIA_INSN_BGT, && case_sem_INSN_BGT },
    { SH64_MEDIA_INSN_BGTU, && case_sem_INSN_BGTU },
    { SH64_MEDIA_INSN_BLINK, && case_sem_INSN_BLINK },
    { SH64_MEDIA_INSN_BNE, && case_sem_INSN_BNE },
    { SH64_MEDIA_INSN_BNEI, && case_sem_INSN_BNEI },
    { SH64_MEDIA_INSN_BRK, && case_sem_INSN_BRK },
    { SH64_MEDIA_INSN_BYTEREV, && case_sem_INSN_BYTEREV },
    { SH64_MEDIA_INSN_CMPEQ, && case_sem_INSN_CMPEQ },
    { SH64_MEDIA_INSN_CMPGT, && case_sem_INSN_CMPGT },
    { SH64_MEDIA_INSN_CMPGTU, && case_sem_INSN_CMPGTU },
    { SH64_MEDIA_INSN_CMVEQ, && case_sem_INSN_CMVEQ },
    { SH64_MEDIA_INSN_CMVNE, && case_sem_INSN_CMVNE },
    { SH64_MEDIA_INSN_FABSD, && case_sem_INSN_FABSD },
    { SH64_MEDIA_INSN_FABSS, && case_sem_INSN_FABSS },
    { SH64_MEDIA_INSN_FADDD, && case_sem_INSN_FADDD },
    { SH64_MEDIA_INSN_FADDS, && case_sem_INSN_FADDS },
    { SH64_MEDIA_INSN_FCMPEQD, && case_sem_INSN_FCMPEQD },
    { SH64_MEDIA_INSN_FCMPEQS, && case_sem_INSN_FCMPEQS },
    { SH64_MEDIA_INSN_FCMPGED, && case_sem_INSN_FCMPGED },
    { SH64_MEDIA_INSN_FCMPGES, && case_sem_INSN_FCMPGES },
    { SH64_MEDIA_INSN_FCMPGTD, && case_sem_INSN_FCMPGTD },
    { SH64_MEDIA_INSN_FCMPGTS, && case_sem_INSN_FCMPGTS },
    { SH64_MEDIA_INSN_FCMPUND, && case_sem_INSN_FCMPUND },
    { SH64_MEDIA_INSN_FCMPUNS, && case_sem_INSN_FCMPUNS },
    { SH64_MEDIA_INSN_FCNVDS, && case_sem_INSN_FCNVDS },
    { SH64_MEDIA_INSN_FCNVSD, && case_sem_INSN_FCNVSD },
    { SH64_MEDIA_INSN_FDIVD, && case_sem_INSN_FDIVD },
    { SH64_MEDIA_INSN_FDIVS, && case_sem_INSN_FDIVS },
    { SH64_MEDIA_INSN_FGETSCR, && case_sem_INSN_FGETSCR },
    { SH64_MEDIA_INSN_FIPRS, && case_sem_INSN_FIPRS },
    { SH64_MEDIA_INSN_FLDD, && case_sem_INSN_FLDD },
    { SH64_MEDIA_INSN_FLDP, && case_sem_INSN_FLDP },
    { SH64_MEDIA_INSN_FLDS, && case_sem_INSN_FLDS },
    { SH64_MEDIA_INSN_FLDXD, && case_sem_INSN_FLDXD },
    { SH64_MEDIA_INSN_FLDXP, && case_sem_INSN_FLDXP },
    { SH64_MEDIA_INSN_FLDXS, && case_sem_INSN_FLDXS },
    { SH64_MEDIA_INSN_FLOATLD, && case_sem_INSN_FLOATLD },
    { SH64_MEDIA_INSN_FLOATLS, && case_sem_INSN_FLOATLS },
    { SH64_MEDIA_INSN_FLOATQD, && case_sem_INSN_FLOATQD },
    { SH64_MEDIA_INSN_FLOATQS, && case_sem_INSN_FLOATQS },
    { SH64_MEDIA_INSN_FMACS, && case_sem_INSN_FMACS },
    { SH64_MEDIA_INSN_FMOVD, && case_sem_INSN_FMOVD },
    { SH64_MEDIA_INSN_FMOVDQ, && case_sem_INSN_FMOVDQ },
    { SH64_MEDIA_INSN_FMOVLS, && case_sem_INSN_FMOVLS },
    { SH64_MEDIA_INSN_FMOVQD, && case_sem_INSN_FMOVQD },
    { SH64_MEDIA_INSN_FMOVS, && case_sem_INSN_FMOVS },
    { SH64_MEDIA_INSN_FMOVSL, && case_sem_INSN_FMOVSL },
    { SH64_MEDIA_INSN_FMULD, && case_sem_INSN_FMULD },
    { SH64_MEDIA_INSN_FMULS, && case_sem_INSN_FMULS },
    { SH64_MEDIA_INSN_FNEGD, && case_sem_INSN_FNEGD },
    { SH64_MEDIA_INSN_FNEGS, && case_sem_INSN_FNEGS },
    { SH64_MEDIA_INSN_FPUTSCR, && case_sem_INSN_FPUTSCR },
    { SH64_MEDIA_INSN_FSQRTD, && case_sem_INSN_FSQRTD },
    { SH64_MEDIA_INSN_FSQRTS, && case_sem_INSN_FSQRTS },
    { SH64_MEDIA_INSN_FSTD, && case_sem_INSN_FSTD },
    { SH64_MEDIA_INSN_FSTP, && case_sem_INSN_FSTP },
    { SH64_MEDIA_INSN_FSTS, && case_sem_INSN_FSTS },
    { SH64_MEDIA_INSN_FSTXD, && case_sem_INSN_FSTXD },
    { SH64_MEDIA_INSN_FSTXP, && case_sem_INSN_FSTXP },
    { SH64_MEDIA_INSN_FSTXS, && case_sem_INSN_FSTXS },
    { SH64_MEDIA_INSN_FSUBD, && case_sem_INSN_FSUBD },
    { SH64_MEDIA_INSN_FSUBS, && case_sem_INSN_FSUBS },
    { SH64_MEDIA_INSN_FTRCDL, && case_sem_INSN_FTRCDL },
    { SH64_MEDIA_INSN_FTRCSL, && case_sem_INSN_FTRCSL },
    { SH64_MEDIA_INSN_FTRCDQ, && case_sem_INSN_FTRCDQ },
    { SH64_MEDIA_INSN_FTRCSQ, && case_sem_INSN_FTRCSQ },
    { SH64_MEDIA_INSN_FTRVS, && case_sem_INSN_FTRVS },
    { SH64_MEDIA_INSN_GETCFG, && case_sem_INSN_GETCFG },
    { SH64_MEDIA_INSN_GETCON, && case_sem_INSN_GETCON },
    { SH64_MEDIA_INSN_GETTR, && case_sem_INSN_GETTR },
    { SH64_MEDIA_INSN_ICBI, && case_sem_INSN_ICBI },
    { SH64_MEDIA_INSN_LDB, && case_sem_INSN_LDB },
    { SH64_MEDIA_INSN_LDL, && case_sem_INSN_LDL },
    { SH64_MEDIA_INSN_LDQ, && case_sem_INSN_LDQ },
    { SH64_MEDIA_INSN_LDUB, && case_sem_INSN_LDUB },
    { SH64_MEDIA_INSN_LDUW, && case_sem_INSN_LDUW },
    { SH64_MEDIA_INSN_LDW, && case_sem_INSN_LDW },
    { SH64_MEDIA_INSN_LDHIL, && case_sem_INSN_LDHIL },
    { SH64_MEDIA_INSN_LDHIQ, && case_sem_INSN_LDHIQ },
    { SH64_MEDIA_INSN_LDLOL, && case_sem_INSN_LDLOL },
    { SH64_MEDIA_INSN_LDLOQ, && case_sem_INSN_LDLOQ },
    { SH64_MEDIA_INSN_LDXB, && case_sem_INSN_LDXB },
    { SH64_MEDIA_INSN_LDXL, && case_sem_INSN_LDXL },
    { SH64_MEDIA_INSN_LDXQ, && case_sem_INSN_LDXQ },
    { SH64_MEDIA_INSN_LDXUB, && case_sem_INSN_LDXUB },
    { SH64_MEDIA_INSN_LDXUW, && case_sem_INSN_LDXUW },
    { SH64_MEDIA_INSN_LDXW, && case_sem_INSN_LDXW },
    { SH64_MEDIA_INSN_MABSL, && case_sem_INSN_MABSL },
    { SH64_MEDIA_INSN_MABSW, && case_sem_INSN_MABSW },
    { SH64_MEDIA_INSN_MADDL, && case_sem_INSN_MADDL },
    { SH64_MEDIA_INSN_MADDW, && case_sem_INSN_MADDW },
    { SH64_MEDIA_INSN_MADDSL, && case_sem_INSN_MADDSL },
    { SH64_MEDIA_INSN_MADDSUB, && case_sem_INSN_MADDSUB },
    { SH64_MEDIA_INSN_MADDSW, && case_sem_INSN_MADDSW },
    { SH64_MEDIA_INSN_MCMPEQB, && case_sem_INSN_MCMPEQB },
    { SH64_MEDIA_INSN_MCMPEQL, && case_sem_INSN_MCMPEQL },
    { SH64_MEDIA_INSN_MCMPEQW, && case_sem_INSN_MCMPEQW },
    { SH64_MEDIA_INSN_MCMPGTL, && case_sem_INSN_MCMPGTL },
    { SH64_MEDIA_INSN_MCMPGTUB, && case_sem_INSN_MCMPGTUB },
    { SH64_MEDIA_INSN_MCMPGTW, && case_sem_INSN_MCMPGTW },
    { SH64_MEDIA_INSN_MCMV, && case_sem_INSN_MCMV },
    { SH64_MEDIA_INSN_MCNVSLW, && case_sem_INSN_MCNVSLW },
    { SH64_MEDIA_INSN_MCNVSWB, && case_sem_INSN_MCNVSWB },
    { SH64_MEDIA_INSN_MCNVSWUB, && case_sem_INSN_MCNVSWUB },
    { SH64_MEDIA_INSN_MEXTR1, && case_sem_INSN_MEXTR1 },
    { SH64_MEDIA_INSN_MEXTR2, && case_sem_INSN_MEXTR2 },
    { SH64_MEDIA_INSN_MEXTR3, && case_sem_INSN_MEXTR3 },
    { SH64_MEDIA_INSN_MEXTR4, && case_sem_INSN_MEXTR4 },
    { SH64_MEDIA_INSN_MEXTR5, && case_sem_INSN_MEXTR5 },
    { SH64_MEDIA_INSN_MEXTR6, && case_sem_INSN_MEXTR6 },
    { SH64_MEDIA_INSN_MEXTR7, && case_sem_INSN_MEXTR7 },
    { SH64_MEDIA_INSN_MMACFXWL, && case_sem_INSN_MMACFXWL },
    { SH64_MEDIA_INSN_MMACNFX_WL, && case_sem_INSN_MMACNFX_WL },
    { SH64_MEDIA_INSN_MMULL, && case_sem_INSN_MMULL },
    { SH64_MEDIA_INSN_MMULW, && case_sem_INSN_MMULW },
    { SH64_MEDIA_INSN_MMULFXL, && case_sem_INSN_MMULFXL },
    { SH64_MEDIA_INSN_MMULFXW, && case_sem_INSN_MMULFXW },
    { SH64_MEDIA_INSN_MMULFXRPW, && case_sem_INSN_MMULFXRPW },
    { SH64_MEDIA_INSN_MMULHIWL, && case_sem_INSN_MMULHIWL },
    { SH64_MEDIA_INSN_MMULLOWL, && case_sem_INSN_MMULLOWL },
    { SH64_MEDIA_INSN_MMULSUMWQ, && case_sem_INSN_MMULSUMWQ },
    { SH64_MEDIA_INSN_MOVI, && case_sem_INSN_MOVI },
    { SH64_MEDIA_INSN_MPERMW, && case_sem_INSN_MPERMW },
    { SH64_MEDIA_INSN_MSADUBQ, && case_sem_INSN_MSADUBQ },
    { SH64_MEDIA_INSN_MSHALDSL, && case_sem_INSN_MSHALDSL },
    { SH64_MEDIA_INSN_MSHALDSW, && case_sem_INSN_MSHALDSW },
    { SH64_MEDIA_INSN_MSHARDL, && case_sem_INSN_MSHARDL },
    { SH64_MEDIA_INSN_MSHARDW, && case_sem_INSN_MSHARDW },
    { SH64_MEDIA_INSN_MSHARDSQ, && case_sem_INSN_MSHARDSQ },
    { SH64_MEDIA_INSN_MSHFHIB, && case_sem_INSN_MSHFHIB },
    { SH64_MEDIA_INSN_MSHFHIL, && case_sem_INSN_MSHFHIL },
    { SH64_MEDIA_INSN_MSHFHIW, && case_sem_INSN_MSHFHIW },
    { SH64_MEDIA_INSN_MSHFLOB, && case_sem_INSN_MSHFLOB },
    { SH64_MEDIA_INSN_MSHFLOL, && case_sem_INSN_MSHFLOL },
    { SH64_MEDIA_INSN_MSHFLOW, && case_sem_INSN_MSHFLOW },
    { SH64_MEDIA_INSN_MSHLLDL, && case_sem_INSN_MSHLLDL },
    { SH64_MEDIA_INSN_MSHLLDW, && case_sem_INSN_MSHLLDW },
    { SH64_MEDIA_INSN_MSHLRDL, && case_sem_INSN_MSHLRDL },
    { SH64_MEDIA_INSN_MSHLRDW, && case_sem_INSN_MSHLRDW },
    { SH64_MEDIA_INSN_MSUBL, && case_sem_INSN_MSUBL },
    { SH64_MEDIA_INSN_MSUBW, && case_sem_INSN_MSUBW },
    { SH64_MEDIA_INSN_MSUBSL, && case_sem_INSN_MSUBSL },
    { SH64_MEDIA_INSN_MSUBSUB, && case_sem_INSN_MSUBSUB },
    { SH64_MEDIA_INSN_MSUBSW, && case_sem_INSN_MSUBSW },
    { SH64_MEDIA_INSN_MULSL, && case_sem_INSN_MULSL },
    { SH64_MEDIA_INSN_MULUL, && case_sem_INSN_MULUL },
    { SH64_MEDIA_INSN_NOP, && case_sem_INSN_NOP },
    { SH64_MEDIA_INSN_NSB, && case_sem_INSN_NSB },
    { SH64_MEDIA_INSN_OCBI, && case_sem_INSN_OCBI },
    { SH64_MEDIA_INSN_OCBP, && case_sem_INSN_OCBP },
    { SH64_MEDIA_INSN_OCBWB, && case_sem_INSN_OCBWB },
    { SH64_MEDIA_INSN_OR, && case_sem_INSN_OR },
    { SH64_MEDIA_INSN_ORI, && case_sem_INSN_ORI },
    { SH64_MEDIA_INSN_PREFI, && case_sem_INSN_PREFI },
    { SH64_MEDIA_INSN_PTA, && case_sem_INSN_PTA },
    { SH64_MEDIA_INSN_PTABS, && case_sem_INSN_PTABS },
    { SH64_MEDIA_INSN_PTB, && case_sem_INSN_PTB },
    { SH64_MEDIA_INSN_PTREL, && case_sem_INSN_PTREL },
    { SH64_MEDIA_INSN_PUTCFG, && case_sem_INSN_PUTCFG },
    { SH64_MEDIA_INSN_PUTCON, && case_sem_INSN_PUTCON },
    { SH64_MEDIA_INSN_RTE, && case_sem_INSN_RTE },
    { SH64_MEDIA_INSN_SHARD, && case_sem_INSN_SHARD },
    { SH64_MEDIA_INSN_SHARDL, && case_sem_INSN_SHARDL },
    { SH64_MEDIA_INSN_SHARI, && case_sem_INSN_SHARI },
    { SH64_MEDIA_INSN_SHARIL, && case_sem_INSN_SHARIL },
    { SH64_MEDIA_INSN_SHLLD, && case_sem_INSN_SHLLD },
    { SH64_MEDIA_INSN_SHLLDL, && case_sem_INSN_SHLLDL },
    { SH64_MEDIA_INSN_SHLLI, && case_sem_INSN_SHLLI },
    { SH64_MEDIA_INSN_SHLLIL, && case_sem_INSN_SHLLIL },
    { SH64_MEDIA_INSN_SHLRD, && case_sem_INSN_SHLRD },
    { SH64_MEDIA_INSN_SHLRDL, && case_sem_INSN_SHLRDL },
    { SH64_MEDIA_INSN_SHLRI, && case_sem_INSN_SHLRI },
    { SH64_MEDIA_INSN_SHLRIL, && case_sem_INSN_SHLRIL },
    { SH64_MEDIA_INSN_SHORI, && case_sem_INSN_SHORI },
    { SH64_MEDIA_INSN_SLEEP, && case_sem_INSN_SLEEP },
    { SH64_MEDIA_INSN_STB, && case_sem_INSN_STB },
    { SH64_MEDIA_INSN_STL, && case_sem_INSN_STL },
    { SH64_MEDIA_INSN_STQ, && case_sem_INSN_STQ },
    { SH64_MEDIA_INSN_STW, && case_sem_INSN_STW },
    { SH64_MEDIA_INSN_STHIL, && case_sem_INSN_STHIL },
    { SH64_MEDIA_INSN_STHIQ, && case_sem_INSN_STHIQ },
    { SH64_MEDIA_INSN_STLOL, && case_sem_INSN_STLOL },
    { SH64_MEDIA_INSN_STLOQ, && case_sem_INSN_STLOQ },
    { SH64_MEDIA_INSN_STXB, && case_sem_INSN_STXB },
    { SH64_MEDIA_INSN_STXL, && case_sem_INSN_STXL },
    { SH64_MEDIA_INSN_STXQ, && case_sem_INSN_STXQ },
    { SH64_MEDIA_INSN_STXW, && case_sem_INSN_STXW },
    { SH64_MEDIA_INSN_SUB, && case_sem_INSN_SUB },
    { SH64_MEDIA_INSN_SUBL, && case_sem_INSN_SUBL },
    { SH64_MEDIA_INSN_SWAPQ, && case_sem_INSN_SWAPQ },
    { SH64_MEDIA_INSN_SYNCI, && case_sem_INSN_SYNCI },
    { SH64_MEDIA_INSN_SYNCO, && case_sem_INSN_SYNCO },
    { SH64_MEDIA_INSN_TRAPA, && case_sem_INSN_TRAPA },
    { SH64_MEDIA_INSN_XOR, && case_sem_INSN_XOR },
    { SH64_MEDIA_INSN_XORI, && case_sem_INSN_XORI },
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
#if WITH_SCACHE_PBB_SH64_MEDIA
    sh64_media_pbb_after (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_SH64_MEDIA
    sh64_media_pbb_before (current_cpu, sem_arg);
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
#if WITH_SCACHE_PBB_SH64_MEDIA
    vpc = sh64_media_pbb_chain (current_cpu, sem_arg);
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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADD) : /* add $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDL) : /* add.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDI) : /* addi $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDIL) : /* addi.l $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (ADDSI (EXTSISI (FLD (f_disp10)), SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ADDZL) : /* addz.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTSIDI (ADDSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ALLOCO) : /* alloco $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_AND) : /* and $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDC) : /* andc $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), INVDI (GET_H_GR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ANDI) : /* andi $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ANDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQ) : /* beq$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BEQI) : /* beqi$likely $rm, $imm6, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beqi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGE) : /* bge$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGEU) : /* bgeu$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGT) : /* bgt$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BGTU) : /* bgtu$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BLINK) : /* blink $trb, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_blink.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNE) : /* bne$likely $rm, $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BNEI) : /* bnei$likely $rm, $imm6, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beqi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  SEM_BRANCH_INIT
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BRK) : /* brk */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.fmt_empty.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

sh64_break (current_cpu, pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_BYTEREV) : /* byterev $rm, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPEQ) : /* cmpeq $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((EQDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPGT) : /* cmpgt $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((GTDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMPGTU) : /* cmpgtu $rm,$rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((GTUDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))) ? (1) : (0));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMVEQ) : /* cmveq $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (EQDI (GET_H_GR (FLD (f_left)), 0)) {
  {
    DI opval = GET_H_GR (FLD (f_right));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_CMVNE) : /* cmvne $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

if (NEDI (GET_H_GR (FLD (f_left)), 0)) {
  {
    DI opval = GET_H_GR (FLD (f_right));
    SET_H_GR (FLD (f_dest), opval);
    written |= (1 << 2);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
}

  abuf->written = written;
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FABSD) : /* fabs.d $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fabsd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FABSS) : /* fabs.s $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fabss (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FADDD) : /* fadd.d $drg, $drh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_faddd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FADDS) : /* fadd.s $frg, $frh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fadds (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPEQD) : /* fcmpeq.d $drg, $drh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpeqd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPEQS) : /* fcmpeq.s $frg, $frh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpeqs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPGED) : /* fcmpge.d $drg, $drh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpged (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPGES) : /* fcmpge.s $frg, $frh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpges (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPGTD) : /* fcmpgt.d $drg, $drh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpgtd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPGTS) : /* fcmpgt.s $frg, $frh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpgts (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPUND) : /* fcmpun.d $drg, $drh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpund (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCMPUNS) : /* fcmpun.s $frg, $frh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTBIDI (sh64_fcmpuns (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCNVDS) : /* fcnv.ds $drgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fcnvds (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FCNVSD) : /* fcnv.sd $frgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fcnvsd (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FDIVD) : /* fdiv.d $drg, $drh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fdivd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FDIVS) : /* fdiv.s $frg, $frh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fdivs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FGETSCR) : /* fgetscr $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = SUBWORDSISF (CPU (h_fpscr));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FIPRS) : /* fipr.s $fvg, $fvh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDD) : /* fld.d $rm, $disp10x8, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x8)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDP) : /* fld.p $rm, $disp10x8, $fpf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fldp (current_cpu, pc, GET_H_GR (FLD (f_left)), FLD (f_disp10x8), FLD (f_dest));
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDS) : /* fld.s $rm, $disp10x4, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_flds.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = GETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x4)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDXD) : /* fldx.d $rm, $rn, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDXP) : /* fldx.p $rm, $rn, $fpf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fldp (current_cpu, pc, GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)), FLD (f_dest));
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLDXS) : /* fldx.s $rm, $rn, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = GETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLOATLD) : /* float.ld $frgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_floatld (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLOATLS) : /* float.ls $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_floatls (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLOATQD) : /* float.qd $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_floatqd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FLOATQS) : /* float.qs $drgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_floatqs (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMACS) : /* fmac.s $frg, $frh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fadds (current_cpu, CPU (h_fr[FLD (f_dest)]), sh64_fmuls (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)])));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVD) : /* fmov.d $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_left_right));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVDQ) : /* fmov.dq $drgh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SUBWORDDFDI (GET_H_DR (FLD (f_left_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVLS) : /* fmov.ls $rm, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = SUBWORDSISF (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVQD) : /* fmov.qd $rm, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = SUBWORDDIDF (GET_H_GR (FLD (f_left)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVS) : /* fmov.s $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_left_right)]);
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMOVSL) : /* fmov.sl $frgh, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SUBWORDSFSI (CPU (h_fr[FLD (f_left_right)])));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMULD) : /* fmul.d $drg, $drh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fmuld (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FMULS) : /* fmul.s $frg, $frh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fmuls (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FNEGD) : /* fneg.d $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fnegd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FNEGS) : /* fneg.s $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fnegs (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FPUTSCR) : /* fputscr $frgh */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBWORDSFSI (CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fpscr) = opval;
    TRACE_RESULT (current_cpu, abuf, "fpscr", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSQRTD) : /* fsqrt.d $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fsqrtd (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSQRTS) : /* fsqrt.s $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fsqrts (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTD) : /* fst.d $rm, $disp10x8, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_dest));
    SETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x8)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTP) : /* fst.p $rm, $disp10x8, $fpf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fstp (current_cpu, pc, GET_H_GR (FLD (f_left)), FLD (f_disp10x8), FLD (f_dest));
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTS) : /* fst.s $rm, $disp10x4, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_flds.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_dest)]);
    SETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), FLD (f_disp10x4)), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTXD) : /* fstx.d $rm, $rn, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = GET_H_DR (FLD (f_dest));
    SETMEMDF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTXP) : /* fstx.p $rm, $rn, $fpf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    SF opval = GET_H_FP (FLD (f_dest));
    SET_H_FP (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "fp", 'f', opval);
  }
sh64_fstp (current_cpu, pc, GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)), FLD (f_dest));
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSTXS) : /* fstx.s $rm, $rn, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = CPU (h_fr[FLD (f_dest)]);
    SETMEMSF (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSUBD) : /* fsub.d $drg, $drh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_fsubd (current_cpu, GET_H_DR (FLD (f_left)), GET_H_DR (FLD (f_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FSUBS) : /* fsub.s $frg, $frh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_fsubs (current_cpu, CPU (h_fr[FLD (f_left)]), CPU (h_fr[FLD (f_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRCDL) : /* ftrc.dl $drgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_ftrcdl (current_cpu, GET_H_DR (FLD (f_left_right)));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRCSL) : /* ftrc.sl $frgh, $frf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SF opval = sh64_ftrcsl (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    CPU (h_fr[FLD (f_dest)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "fr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRCDQ) : /* ftrc.dq $drgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_ftrcdq (current_cpu, GET_H_DR (FLD (f_left_right)));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRCSQ) : /* ftrc.sq $frgh, $drf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fabsd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DF opval = sh64_ftrcsq (current_cpu, CPU (h_fr[FLD (f_left_right)]));
    SET_H_DR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "dr", 'f', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_FTRVS) : /* ftrv.s $mtrxg, $fvh, $fvf */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_GETCFG) : /* getcfg $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_GETCON) : /* getcon $crk, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_CR (FLD (f_left));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_GETTR) : /* gettr $trb, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_blink.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = CPU (h_tr[FLD (f_trb)]);
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ICBI) : /* icbi $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDB) : /* ld.b $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDL) : /* ld.l $rm, $disp10x4, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_flds.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x4)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDQ) : /* ld.q $rm, $disp10x8, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x8))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUB) : /* ld.ub $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDUW) : /* ld.uw $rm, $disp10x2, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_lduw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDW) : /* ld.w $rm, $disp10x2, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_lduw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDHIL) : /* ldhi.l $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDHIQ) : /* ldhi.q $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDLOL) : /* ldlo.l $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDLOQ) : /* ldlo.q $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXB) : /* ldx.b $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTQIDI (GETMEMQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXL) : /* ldx.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (GETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXQ) : /* ldx.q $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXUB) : /* ldx.ub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTQIDI (GETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXUW) : /* ldx.uw $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ZEXTHIDI (GETMEMUHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_LDXW) : /* ldx.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTHIDI (GETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MABSL) : /* mabs.l $rm, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MABSW) : /* mabs.w $rm, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MADDL) : /* madd.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MADDW) : /* madd.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MADDSL) : /* madds.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MADDSUB) : /* madds.ub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MADDSW) : /* madds.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPEQB) : /* mcmpeq.b $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPEQL) : /* mcmpeq.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPEQW) : /* mcmpeq.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPGTL) : /* mcmpgt.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPGTUB) : /* mcmpgt.ub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMPGTW) : /* mcmpgt.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCMV) : /* mcmv $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (ANDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), ANDDI (GET_H_GR (FLD (f_dest)), INVDI (GET_H_GR (FLD (f_right)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCNVSLW) : /* mcnvs.lw $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCNVSWB) : /* mcnvs.wb $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MCNVSWUB) : /* mcnvs.wub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR1) : /* mextr1 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR2) : /* mextr2 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR3) : /* mextr3 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR4) : /* mextr4 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR5) : /* mextr5 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR6) : /* mextr6 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MEXTR7) : /* mextr7 $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMACFXWL) : /* mmacfx.wl $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMACNFX_WL) : /* mmacnfx.wl $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULL) : /* mmul.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULW) : /* mmul.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULFXL) : /* mmulfx.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULFXW) : /* mmulfx.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULFXRPW) : /* mmulfxrp.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULHIWL) : /* mmulhi.wl $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULLOWL) : /* mmullo.wl $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MMULSUMWQ) : /* mmulsum.wq $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MOVI) : /* movi $imm16, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_movi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (FLD (f_imm16));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MPERMW) : /* mperm.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSADUBQ) : /* msad.ubq $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHALDSL) : /* mshalds.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHALDSW) : /* mshalds.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHARDL) : /* mshard.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHARDW) : /* mshard.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHARDSQ) : /* mshards.q $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ((LTDI (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63)), NEGDI (SLLDI (1, SUBSI (16, 1))))) ? (NEGDI (SLLDI (1, SUBSI (16, 1)))) : (((LTDI (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63)), SLLDI (1, SUBSI (16, 1)))) ? (SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63))) : (SUBDI (SLLDI (1, SUBSI (16, 1)), 1)))));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFHIB) : /* mshfhi.b $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFHIL) : /* mshfhi.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFHIW) : /* mshfhi.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFLOB) : /* mshflo.b $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFLOL) : /* mshflo.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHFLOW) : /* mshflo.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHLLDL) : /* mshlld.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHLLDW) : /* mshlld.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHLRDL) : /* mshlrd.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSHLRDW) : /* mshlrd.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSUBL) : /* msub.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSUBW) : /* msub.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSUBSL) : /* msubs.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSUBSUB) : /* msubs.ub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MSUBSW) : /* msubs.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULSL) : /* muls.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), EXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_MULUL) : /* mulu.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = MULDI (ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1)), ZEXTSIDI (SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
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
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_NSB) : /* nsb $rm, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = sh64_nsb (current_cpu, GET_H_GR (FLD (f_left)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBI) : /* ocbi $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBP) : /* ocbp $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OCBWB) : /* ocbwb $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_OR) : /* or $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_ORI) : /* ori $rm, $imm10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_ori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm10)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PREFI) : /* prefi $rm, $disp6x32 */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_GR (FLD (f_left), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }
((void) 0); /*nop*/
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PTA) : /* pta$likely $disp16, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_pta.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = ADDSI (FLD (f_disp16), 1);
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PTABS) : /* ptabs$likely $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = GET_H_GR (FLD (f_right));
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PTB) : /* ptb$likely $disp16, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_pta.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = FLD (f_disp16);
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PTREL) : /* ptrel$likely $rn, $tra */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_beq.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

{
((void) 0); /*nop*/
  {
    DI opval = ADDDI (pc, GET_H_GR (FLD (f_right)));
    CPU (h_tr[FLD (f_tra)]) = opval;
    TRACE_RESULT (current_cpu, abuf, "tr", 'D', opval);
  }
}

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PUTCFG) : /* putcfg $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_PUTCON) : /* putcon $rm, $crj */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_left));
    SET_H_CR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "cr", 'D', opval);
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
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

((void) 0); /*nop*/

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHARD) : /* shard $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRADI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHARDL) : /* shard.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHARI) : /* shari $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRADI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHARIL) : /* shari.l $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRASI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLLD) : /* shlld $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SLLDI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLLDL) : /* shlld.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLLI) : /* shlli $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SLLDI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLLIL) : /* shlli.l $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SLLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLRD) : /* shlrd $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRLDI (GET_H_GR (FLD (f_left)), ANDDI (GET_H_GR (FLD (f_right)), 63));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLRDL) : /* shlrd.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDDI (GET_H_GR (FLD (f_right)), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLRI) : /* shlri $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SRLDI (GET_H_GR (FLD (f_left)), FLD (f_uimm6));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHLRIL) : /* shlri.l $rm, $uimm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shari.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SRLSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), ANDSI (FLD (f_uimm6), 63)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SHORI) : /* shori $uimm16, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_shori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = ORDI (SLLDI (GET_H_GR (FLD (f_dest)), 16), ZEXTSIDI (FLD (f_uimm16)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

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

  CASE (sem, INSN_STB) : /* st.b $rm, $disp10, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_addi.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = ANDQI (GET_H_GR (FLD (f_dest)), 255);
    SETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STL) : /* st.l $rm, $disp10x4, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_flds.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = ANDSI (GET_H_GR (FLD (f_dest)), 0xffffffff);
    SETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x4))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STQ) : /* st.q $rm, $disp10x8, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_fldd.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x8))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STW) : /* st.w $rm, $disp10x2, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_lduw.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = ANDHI (GET_H_GR (FLD (f_dest)), 65535);
    SETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_disp10x2))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STHIL) : /* sthi.l $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STHIQ) : /* sthi.q $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STLOL) : /* stlo.l $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STLOQ) : /* stlo.q $rm, $disp6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_getcfg.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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
#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STXB) : /* stx.b $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    UQI opval = SUBWORDDIQI (GET_H_GR (FLD (f_dest)), 7);
    SETMEMUQI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STXL) : /* stx.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    SI opval = SUBWORDDISI (GET_H_GR (FLD (f_dest)), 1);
    SETMEMSI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STXQ) : /* stx.q $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = GET_H_GR (FLD (f_dest));
    SETMEMDI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_STXW) : /* stx.w $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    HI opval = SUBWORDDIHI (GET_H_GR (FLD (f_dest)), 3);
    SETMEMHI (current_cpu, pc, ADDDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right))), opval);
    TRACE_RESULT (current_cpu, abuf, "memory", 'x', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUB) : /* sub $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = SUBDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SUBL) : /* sub.l $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = EXTSIDI (SUBSI (SUBWORDDISI (GET_H_GR (FLD (f_left)), 1), SUBWORDDISI (GET_H_GR (FLD (f_right)), 1)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SWAPQ) : /* swap.q $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

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

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_SYNCI) : /* synci */
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

  CASE (sem, INSN_SYNCO) : /* synco */
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

  CASE (sem, INSN_TRAPA) : /* trapa $rm */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

sh64_trapa (current_cpu, GET_H_GR (FLD (f_left)), pc);

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XOR) : /* xor $rm, $rn, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_add.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_left)), GET_H_GR (FLD (f_right)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
  }

#undef FLD
}
  NEXT (vpc);

  CASE (sem, INSN_XORI) : /* xori $rm, $imm6, $rd */
{
  SEM_ARG sem_arg = SEM_SEM_ARG (vpc, sc);
  ARGBUF *abuf = SEM_ARGBUF (sem_arg);
#define FLD(f) abuf->fields.sfmt_xori.f
  int UNUSED written = 0;
  IADDR UNUSED pc = abuf->addr;
  vpc = SEM_NEXT_VPC (sem_arg, pc, 4);

  {
    DI opval = XORDI (GET_H_GR (FLD (f_left)), EXTSIDI (FLD (f_imm6)));
    SET_H_GR (FLD (f_dest), opval);
    TRACE_RESULT (current_cpu, abuf, "gr", 'D', opval);
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
