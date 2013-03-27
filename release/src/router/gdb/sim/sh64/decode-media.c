/* Simulator instruction decoder for sh64_media.

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
static IDESC sh64_media_insn_data[SH64_MEDIA_INSN__MAX];

/* Commas between elements are contained in the macros.
   Some of these are conditionally compiled out.  */

static const struct insn_sem sh64_media_insn_sem[] =
{
  { VIRTUAL_INSN_X_INVALID, SH64_MEDIA_INSN_X_INVALID, SH64_MEDIA_SFMT_EMPTY },
  { VIRTUAL_INSN_X_AFTER, SH64_MEDIA_INSN_X_AFTER, SH64_MEDIA_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEFORE, SH64_MEDIA_INSN_X_BEFORE, SH64_MEDIA_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CTI_CHAIN, SH64_MEDIA_INSN_X_CTI_CHAIN, SH64_MEDIA_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CHAIN, SH64_MEDIA_INSN_X_CHAIN, SH64_MEDIA_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEGIN, SH64_MEDIA_INSN_X_BEGIN, SH64_MEDIA_SFMT_EMPTY },
  { SH_INSN_ADD, SH64_MEDIA_INSN_ADD, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ADDL, SH64_MEDIA_INSN_ADDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ADDI, SH64_MEDIA_INSN_ADDI, SH64_MEDIA_SFMT_ADDI },
  { SH_INSN_ADDIL, SH64_MEDIA_INSN_ADDIL, SH64_MEDIA_SFMT_ADDI },
  { SH_INSN_ADDZL, SH64_MEDIA_INSN_ADDZL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ALLOCO, SH64_MEDIA_INSN_ALLOCO, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_AND, SH64_MEDIA_INSN_AND, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ANDC, SH64_MEDIA_INSN_ANDC, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ANDI, SH64_MEDIA_INSN_ANDI, SH64_MEDIA_SFMT_ADDI },
  { SH_INSN_BEQ, SH64_MEDIA_INSN_BEQ, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BEQI, SH64_MEDIA_INSN_BEQI, SH64_MEDIA_SFMT_BEQI },
  { SH_INSN_BGE, SH64_MEDIA_INSN_BGE, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BGEU, SH64_MEDIA_INSN_BGEU, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BGT, SH64_MEDIA_INSN_BGT, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BGTU, SH64_MEDIA_INSN_BGTU, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BLINK, SH64_MEDIA_INSN_BLINK, SH64_MEDIA_SFMT_BLINK },
  { SH_INSN_BNE, SH64_MEDIA_INSN_BNE, SH64_MEDIA_SFMT_BEQ },
  { SH_INSN_BNEI, SH64_MEDIA_INSN_BNEI, SH64_MEDIA_SFMT_BEQI },
  { SH_INSN_BRK, SH64_MEDIA_INSN_BRK, SH64_MEDIA_SFMT_BRK },
  { SH_INSN_BYTEREV, SH64_MEDIA_INSN_BYTEREV, SH64_MEDIA_SFMT_BYTEREV },
  { SH_INSN_CMPEQ, SH64_MEDIA_INSN_CMPEQ, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_CMPGT, SH64_MEDIA_INSN_CMPGT, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_CMPGTU, SH64_MEDIA_INSN_CMPGTU, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_CMVEQ, SH64_MEDIA_INSN_CMVEQ, SH64_MEDIA_SFMT_CMVEQ },
  { SH_INSN_CMVNE, SH64_MEDIA_INSN_CMVNE, SH64_MEDIA_SFMT_CMVEQ },
  { SH_INSN_FABSD, SH64_MEDIA_INSN_FABSD, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FABSS, SH64_MEDIA_INSN_FABSS, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FADDD, SH64_MEDIA_INSN_FADDD, SH64_MEDIA_SFMT_FADDD },
  { SH_INSN_FADDS, SH64_MEDIA_INSN_FADDS, SH64_MEDIA_SFMT_FADDS },
  { SH_INSN_FCMPEQD, SH64_MEDIA_INSN_FCMPEQD, SH64_MEDIA_SFMT_FCMPEQD },
  { SH_INSN_FCMPEQS, SH64_MEDIA_INSN_FCMPEQS, SH64_MEDIA_SFMT_FCMPEQS },
  { SH_INSN_FCMPGED, SH64_MEDIA_INSN_FCMPGED, SH64_MEDIA_SFMT_FCMPEQD },
  { SH_INSN_FCMPGES, SH64_MEDIA_INSN_FCMPGES, SH64_MEDIA_SFMT_FCMPEQS },
  { SH_INSN_FCMPGTD, SH64_MEDIA_INSN_FCMPGTD, SH64_MEDIA_SFMT_FCMPEQD },
  { SH_INSN_FCMPGTS, SH64_MEDIA_INSN_FCMPGTS, SH64_MEDIA_SFMT_FCMPEQS },
  { SH_INSN_FCMPUND, SH64_MEDIA_INSN_FCMPUND, SH64_MEDIA_SFMT_FCMPEQD },
  { SH_INSN_FCMPUNS, SH64_MEDIA_INSN_FCMPUNS, SH64_MEDIA_SFMT_FCMPEQS },
  { SH_INSN_FCNVDS, SH64_MEDIA_INSN_FCNVDS, SH64_MEDIA_SFMT_FCNVDS },
  { SH_INSN_FCNVSD, SH64_MEDIA_INSN_FCNVSD, SH64_MEDIA_SFMT_FCNVSD },
  { SH_INSN_FDIVD, SH64_MEDIA_INSN_FDIVD, SH64_MEDIA_SFMT_FADDD },
  { SH_INSN_FDIVS, SH64_MEDIA_INSN_FDIVS, SH64_MEDIA_SFMT_FADDS },
  { SH_INSN_FGETSCR, SH64_MEDIA_INSN_FGETSCR, SH64_MEDIA_SFMT_FGETSCR },
  { SH_INSN_FIPRS, SH64_MEDIA_INSN_FIPRS, SH64_MEDIA_SFMT_FIPRS },
  { SH_INSN_FLDD, SH64_MEDIA_INSN_FLDD, SH64_MEDIA_SFMT_FLDD },
  { SH_INSN_FLDP, SH64_MEDIA_INSN_FLDP, SH64_MEDIA_SFMT_FLDP },
  { SH_INSN_FLDS, SH64_MEDIA_INSN_FLDS, SH64_MEDIA_SFMT_FLDS },
  { SH_INSN_FLDXD, SH64_MEDIA_INSN_FLDXD, SH64_MEDIA_SFMT_FLDXD },
  { SH_INSN_FLDXP, SH64_MEDIA_INSN_FLDXP, SH64_MEDIA_SFMT_FLDXP },
  { SH_INSN_FLDXS, SH64_MEDIA_INSN_FLDXS, SH64_MEDIA_SFMT_FLDXS },
  { SH_INSN_FLOATLD, SH64_MEDIA_INSN_FLOATLD, SH64_MEDIA_SFMT_FCNVSD },
  { SH_INSN_FLOATLS, SH64_MEDIA_INSN_FLOATLS, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FLOATQD, SH64_MEDIA_INSN_FLOATQD, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FLOATQS, SH64_MEDIA_INSN_FLOATQS, SH64_MEDIA_SFMT_FCNVDS },
  { SH_INSN_FMACS, SH64_MEDIA_INSN_FMACS, SH64_MEDIA_SFMT_FMACS },
  { SH_INSN_FMOVD, SH64_MEDIA_INSN_FMOVD, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FMOVDQ, SH64_MEDIA_INSN_FMOVDQ, SH64_MEDIA_SFMT_FMOVDQ },
  { SH_INSN_FMOVLS, SH64_MEDIA_INSN_FMOVLS, SH64_MEDIA_SFMT_FMOVLS },
  { SH_INSN_FMOVQD, SH64_MEDIA_INSN_FMOVQD, SH64_MEDIA_SFMT_FMOVQD },
  { SH_INSN_FMOVS, SH64_MEDIA_INSN_FMOVS, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FMOVSL, SH64_MEDIA_INSN_FMOVSL, SH64_MEDIA_SFMT_FMOVSL },
  { SH_INSN_FMULD, SH64_MEDIA_INSN_FMULD, SH64_MEDIA_SFMT_FADDD },
  { SH_INSN_FMULS, SH64_MEDIA_INSN_FMULS, SH64_MEDIA_SFMT_FADDS },
  { SH_INSN_FNEGD, SH64_MEDIA_INSN_FNEGD, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FNEGS, SH64_MEDIA_INSN_FNEGS, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FPUTSCR, SH64_MEDIA_INSN_FPUTSCR, SH64_MEDIA_SFMT_FPUTSCR },
  { SH_INSN_FSQRTD, SH64_MEDIA_INSN_FSQRTD, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FSQRTS, SH64_MEDIA_INSN_FSQRTS, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FSTD, SH64_MEDIA_INSN_FSTD, SH64_MEDIA_SFMT_FSTD },
  { SH_INSN_FSTP, SH64_MEDIA_INSN_FSTP, SH64_MEDIA_SFMT_FLDP },
  { SH_INSN_FSTS, SH64_MEDIA_INSN_FSTS, SH64_MEDIA_SFMT_FSTS },
  { SH_INSN_FSTXD, SH64_MEDIA_INSN_FSTXD, SH64_MEDIA_SFMT_FSTXD },
  { SH_INSN_FSTXP, SH64_MEDIA_INSN_FSTXP, SH64_MEDIA_SFMT_FLDXP },
  { SH_INSN_FSTXS, SH64_MEDIA_INSN_FSTXS, SH64_MEDIA_SFMT_FSTXS },
  { SH_INSN_FSUBD, SH64_MEDIA_INSN_FSUBD, SH64_MEDIA_SFMT_FADDD },
  { SH_INSN_FSUBS, SH64_MEDIA_INSN_FSUBS, SH64_MEDIA_SFMT_FADDS },
  { SH_INSN_FTRCDL, SH64_MEDIA_INSN_FTRCDL, SH64_MEDIA_SFMT_FCNVDS },
  { SH_INSN_FTRCSL, SH64_MEDIA_INSN_FTRCSL, SH64_MEDIA_SFMT_FABSS },
  { SH_INSN_FTRCDQ, SH64_MEDIA_INSN_FTRCDQ, SH64_MEDIA_SFMT_FABSD },
  { SH_INSN_FTRCSQ, SH64_MEDIA_INSN_FTRCSQ, SH64_MEDIA_SFMT_FCNVSD },
  { SH_INSN_FTRVS, SH64_MEDIA_INSN_FTRVS, SH64_MEDIA_SFMT_FTRVS },
  { SH_INSN_GETCFG, SH64_MEDIA_INSN_GETCFG, SH64_MEDIA_SFMT_GETCFG },
  { SH_INSN_GETCON, SH64_MEDIA_INSN_GETCON, SH64_MEDIA_SFMT_GETCON },
  { SH_INSN_GETTR, SH64_MEDIA_INSN_GETTR, SH64_MEDIA_SFMT_GETTR },
  { SH_INSN_ICBI, SH64_MEDIA_INSN_ICBI, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_LDB, SH64_MEDIA_INSN_LDB, SH64_MEDIA_SFMT_LDB },
  { SH_INSN_LDL, SH64_MEDIA_INSN_LDL, SH64_MEDIA_SFMT_LDL },
  { SH_INSN_LDQ, SH64_MEDIA_INSN_LDQ, SH64_MEDIA_SFMT_LDQ },
  { SH_INSN_LDUB, SH64_MEDIA_INSN_LDUB, SH64_MEDIA_SFMT_LDB },
  { SH_INSN_LDUW, SH64_MEDIA_INSN_LDUW, SH64_MEDIA_SFMT_LDUW },
  { SH_INSN_LDW, SH64_MEDIA_INSN_LDW, SH64_MEDIA_SFMT_LDUW },
  { SH_INSN_LDHIL, SH64_MEDIA_INSN_LDHIL, SH64_MEDIA_SFMT_LDHIL },
  { SH_INSN_LDHIQ, SH64_MEDIA_INSN_LDHIQ, SH64_MEDIA_SFMT_LDHIQ },
  { SH_INSN_LDLOL, SH64_MEDIA_INSN_LDLOL, SH64_MEDIA_SFMT_LDLOL },
  { SH_INSN_LDLOQ, SH64_MEDIA_INSN_LDLOQ, SH64_MEDIA_SFMT_LDLOQ },
  { SH_INSN_LDXB, SH64_MEDIA_INSN_LDXB, SH64_MEDIA_SFMT_LDXB },
  { SH_INSN_LDXL, SH64_MEDIA_INSN_LDXL, SH64_MEDIA_SFMT_LDXL },
  { SH_INSN_LDXQ, SH64_MEDIA_INSN_LDXQ, SH64_MEDIA_SFMT_LDXQ },
  { SH_INSN_LDXUB, SH64_MEDIA_INSN_LDXUB, SH64_MEDIA_SFMT_LDXUB },
  { SH_INSN_LDXUW, SH64_MEDIA_INSN_LDXUW, SH64_MEDIA_SFMT_LDXUW },
  { SH_INSN_LDXW, SH64_MEDIA_INSN_LDXW, SH64_MEDIA_SFMT_LDXW },
  { SH_INSN_MABSL, SH64_MEDIA_INSN_MABSL, SH64_MEDIA_SFMT_BYTEREV },
  { SH_INSN_MABSW, SH64_MEDIA_INSN_MABSW, SH64_MEDIA_SFMT_BYTEREV },
  { SH_INSN_MADDL, SH64_MEDIA_INSN_MADDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MADDW, SH64_MEDIA_INSN_MADDW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MADDSL, SH64_MEDIA_INSN_MADDSL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MADDSUB, SH64_MEDIA_INSN_MADDSUB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MADDSW, SH64_MEDIA_INSN_MADDSW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPEQB, SH64_MEDIA_INSN_MCMPEQB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPEQL, SH64_MEDIA_INSN_MCMPEQL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPEQW, SH64_MEDIA_INSN_MCMPEQW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPGTL, SH64_MEDIA_INSN_MCMPGTL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPGTUB, SH64_MEDIA_INSN_MCMPGTUB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMPGTW, SH64_MEDIA_INSN_MCMPGTW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCMV, SH64_MEDIA_INSN_MCMV, SH64_MEDIA_SFMT_MCMV },
  { SH_INSN_MCNVSLW, SH64_MEDIA_INSN_MCNVSLW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCNVSWB, SH64_MEDIA_INSN_MCNVSWB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MCNVSWUB, SH64_MEDIA_INSN_MCNVSWUB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR1, SH64_MEDIA_INSN_MEXTR1, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR2, SH64_MEDIA_INSN_MEXTR2, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR3, SH64_MEDIA_INSN_MEXTR3, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR4, SH64_MEDIA_INSN_MEXTR4, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR5, SH64_MEDIA_INSN_MEXTR5, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR6, SH64_MEDIA_INSN_MEXTR6, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MEXTR7, SH64_MEDIA_INSN_MEXTR7, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMACFXWL, SH64_MEDIA_INSN_MMACFXWL, SH64_MEDIA_SFMT_MCMV },
  { SH_INSN_MMACNFX_WL, SH64_MEDIA_INSN_MMACNFX_WL, SH64_MEDIA_SFMT_MCMV },
  { SH_INSN_MMULL, SH64_MEDIA_INSN_MMULL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULW, SH64_MEDIA_INSN_MMULW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULFXL, SH64_MEDIA_INSN_MMULFXL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULFXW, SH64_MEDIA_INSN_MMULFXW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULFXRPW, SH64_MEDIA_INSN_MMULFXRPW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULHIWL, SH64_MEDIA_INSN_MMULHIWL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULLOWL, SH64_MEDIA_INSN_MMULLOWL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MMULSUMWQ, SH64_MEDIA_INSN_MMULSUMWQ, SH64_MEDIA_SFMT_MCMV },
  { SH_INSN_MOVI, SH64_MEDIA_INSN_MOVI, SH64_MEDIA_SFMT_MOVI },
  { SH_INSN_MPERMW, SH64_MEDIA_INSN_MPERMW, SH64_MEDIA_SFMT_MPERMW },
  { SH_INSN_MSADUBQ, SH64_MEDIA_INSN_MSADUBQ, SH64_MEDIA_SFMT_MCMV },
  { SH_INSN_MSHALDSL, SH64_MEDIA_INSN_MSHALDSL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHALDSW, SH64_MEDIA_INSN_MSHALDSW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHARDL, SH64_MEDIA_INSN_MSHARDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHARDW, SH64_MEDIA_INSN_MSHARDW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHARDSQ, SH64_MEDIA_INSN_MSHARDSQ, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFHIB, SH64_MEDIA_INSN_MSHFHIB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFHIL, SH64_MEDIA_INSN_MSHFHIL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFHIW, SH64_MEDIA_INSN_MSHFHIW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFLOB, SH64_MEDIA_INSN_MSHFLOB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFLOL, SH64_MEDIA_INSN_MSHFLOL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHFLOW, SH64_MEDIA_INSN_MSHFLOW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHLLDL, SH64_MEDIA_INSN_MSHLLDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHLLDW, SH64_MEDIA_INSN_MSHLLDW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHLRDL, SH64_MEDIA_INSN_MSHLRDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSHLRDW, SH64_MEDIA_INSN_MSHLRDW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSUBL, SH64_MEDIA_INSN_MSUBL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSUBW, SH64_MEDIA_INSN_MSUBW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSUBSL, SH64_MEDIA_INSN_MSUBSL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSUBSUB, SH64_MEDIA_INSN_MSUBSUB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MSUBSW, SH64_MEDIA_INSN_MSUBSW, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MULSL, SH64_MEDIA_INSN_MULSL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_MULUL, SH64_MEDIA_INSN_MULUL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_NOP, SH64_MEDIA_INSN_NOP, SH64_MEDIA_SFMT_NOP },
  { SH_INSN_NSB, SH64_MEDIA_INSN_NSB, SH64_MEDIA_SFMT_BYTEREV },
  { SH_INSN_OCBI, SH64_MEDIA_INSN_OCBI, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_OCBP, SH64_MEDIA_INSN_OCBP, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_OCBWB, SH64_MEDIA_INSN_OCBWB, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_OR, SH64_MEDIA_INSN_OR, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_ORI, SH64_MEDIA_INSN_ORI, SH64_MEDIA_SFMT_ORI },
  { SH_INSN_PREFI, SH64_MEDIA_INSN_PREFI, SH64_MEDIA_SFMT_ALLOCO },
  { SH_INSN_PTA, SH64_MEDIA_INSN_PTA, SH64_MEDIA_SFMT_PTA },
  { SH_INSN_PTABS, SH64_MEDIA_INSN_PTABS, SH64_MEDIA_SFMT_PTABS },
  { SH_INSN_PTB, SH64_MEDIA_INSN_PTB, SH64_MEDIA_SFMT_PTA },
  { SH_INSN_PTREL, SH64_MEDIA_INSN_PTREL, SH64_MEDIA_SFMT_PTREL },
  { SH_INSN_PUTCFG, SH64_MEDIA_INSN_PUTCFG, SH64_MEDIA_SFMT_PUTCFG },
  { SH_INSN_PUTCON, SH64_MEDIA_INSN_PUTCON, SH64_MEDIA_SFMT_PUTCON },
  { SH_INSN_RTE, SH64_MEDIA_INSN_RTE, SH64_MEDIA_SFMT_NOP },
  { SH_INSN_SHARD, SH64_MEDIA_INSN_SHARD, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHARDL, SH64_MEDIA_INSN_SHARDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHARI, SH64_MEDIA_INSN_SHARI, SH64_MEDIA_SFMT_SHARI },
  { SH_INSN_SHARIL, SH64_MEDIA_INSN_SHARIL, SH64_MEDIA_SFMT_SHARIL },
  { SH_INSN_SHLLD, SH64_MEDIA_INSN_SHLLD, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHLLDL, SH64_MEDIA_INSN_SHLLDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHLLI, SH64_MEDIA_INSN_SHLLI, SH64_MEDIA_SFMT_SHARI },
  { SH_INSN_SHLLIL, SH64_MEDIA_INSN_SHLLIL, SH64_MEDIA_SFMT_SHARIL },
  { SH_INSN_SHLRD, SH64_MEDIA_INSN_SHLRD, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHLRDL, SH64_MEDIA_INSN_SHLRDL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SHLRI, SH64_MEDIA_INSN_SHLRI, SH64_MEDIA_SFMT_SHARI },
  { SH_INSN_SHLRIL, SH64_MEDIA_INSN_SHLRIL, SH64_MEDIA_SFMT_SHARIL },
  { SH_INSN_SHORI, SH64_MEDIA_INSN_SHORI, SH64_MEDIA_SFMT_SHORI },
  { SH_INSN_SLEEP, SH64_MEDIA_INSN_SLEEP, SH64_MEDIA_SFMT_NOP },
  { SH_INSN_STB, SH64_MEDIA_INSN_STB, SH64_MEDIA_SFMT_STB },
  { SH_INSN_STL, SH64_MEDIA_INSN_STL, SH64_MEDIA_SFMT_STL },
  { SH_INSN_STQ, SH64_MEDIA_INSN_STQ, SH64_MEDIA_SFMT_STQ },
  { SH_INSN_STW, SH64_MEDIA_INSN_STW, SH64_MEDIA_SFMT_STW },
  { SH_INSN_STHIL, SH64_MEDIA_INSN_STHIL, SH64_MEDIA_SFMT_STHIL },
  { SH_INSN_STHIQ, SH64_MEDIA_INSN_STHIQ, SH64_MEDIA_SFMT_STHIQ },
  { SH_INSN_STLOL, SH64_MEDIA_INSN_STLOL, SH64_MEDIA_SFMT_STLOL },
  { SH_INSN_STLOQ, SH64_MEDIA_INSN_STLOQ, SH64_MEDIA_SFMT_STLOQ },
  { SH_INSN_STXB, SH64_MEDIA_INSN_STXB, SH64_MEDIA_SFMT_STXB },
  { SH_INSN_STXL, SH64_MEDIA_INSN_STXL, SH64_MEDIA_SFMT_STXL },
  { SH_INSN_STXQ, SH64_MEDIA_INSN_STXQ, SH64_MEDIA_SFMT_STXQ },
  { SH_INSN_STXW, SH64_MEDIA_INSN_STXW, SH64_MEDIA_SFMT_STXW },
  { SH_INSN_SUB, SH64_MEDIA_INSN_SUB, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SUBL, SH64_MEDIA_INSN_SUBL, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_SWAPQ, SH64_MEDIA_INSN_SWAPQ, SH64_MEDIA_SFMT_SWAPQ },
  { SH_INSN_SYNCI, SH64_MEDIA_INSN_SYNCI, SH64_MEDIA_SFMT_NOP },
  { SH_INSN_SYNCO, SH64_MEDIA_INSN_SYNCO, SH64_MEDIA_SFMT_NOP },
  { SH_INSN_TRAPA, SH64_MEDIA_INSN_TRAPA, SH64_MEDIA_SFMT_TRAPA },
  { SH_INSN_XOR, SH64_MEDIA_INSN_XOR, SH64_MEDIA_SFMT_ADD },
  { SH_INSN_XORI, SH64_MEDIA_INSN_XORI, SH64_MEDIA_SFMT_XORI },
};

static const struct insn_sem sh64_media_insn_sem_invalid = {
  VIRTUAL_INSN_X_INVALID, SH64_MEDIA_INSN_X_INVALID, SH64_MEDIA_SFMT_EMPTY
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
sh64_media_init_idesc_table (SIM_CPU *cpu)
{
  IDESC *id,*tabend;
  const struct insn_sem *t,*tend;
  int tabsize = SH64_MEDIA_INSN__MAX;
  IDESC *table = sh64_media_insn_data;

  memset (table, 0, tabsize * sizeof (IDESC));

  /* First set all entries to the `invalid insn'.  */
  t = & sh64_media_insn_sem_invalid;
  for (id = table, tabend = table + tabsize; id < tabend; ++id)
    init_idesc (cpu, id, t);

  /* Now fill in the values for the chosen cpu.  */
  for (t = sh64_media_insn_sem, tend = t + sizeof (sh64_media_insn_sem) / sizeof (*t);
       t != tend; ++t)
    {
      init_idesc (cpu, & table[t->index], t);
    }

  /* Link the IDESC table into the cpu.  */
  CPU_IDESC (cpu) = table;
}

/* Given an instruction, return a pointer to its IDESC entry.  */

const IDESC *
sh64_media_decode (SIM_CPU *current_cpu, IADDR pc,
              CGEN_INSN_INT base_insn, CGEN_INSN_INT entire_insn,
              ARGBUF *abuf)
{
  /* Result of decoder.  */
  SH64_MEDIA_INSN_TYPE itype;

  {
    CGEN_INSN_INT insn = base_insn;

    {
      unsigned int val = (((insn >> 22) & (63 << 4)) | ((insn >> 16) & (15 << 0)));
      switch (val)
      {
      case 1 :
        if ((entire_insn & 0xfc0f000f) == 0x10000)
          { itype = SH64_MEDIA_INSN_CMPEQ; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 3 :
        if ((entire_insn & 0xfc0f000f) == 0x30000)
          { itype = SH64_MEDIA_INSN_CMPGT; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 7 :
        if ((entire_insn & 0xfc0f000f) == 0x70000)
          { itype = SH64_MEDIA_INSN_CMPGTU; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 8 :
        if ((entire_insn & 0xfc0f000f) == 0x80000)
          { itype = SH64_MEDIA_INSN_ADDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 9 :
        if ((entire_insn & 0xfc0f000f) == 0x90000)
          { itype = SH64_MEDIA_INSN_ADD; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 10 :
        if ((entire_insn & 0xfc0f000f) == 0xa0000)
          { itype = SH64_MEDIA_INSN_SUBL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 11 :
        if ((entire_insn & 0xfc0f000f) == 0xb0000)
          { itype = SH64_MEDIA_INSN_SUB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 12 :
        if ((entire_insn & 0xfc0f000f) == 0xc0000)
          { itype = SH64_MEDIA_INSN_ADDZL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 13 :
        if ((entire_insn & 0xfc0ffc0f) == 0xdfc00)
          { itype = SH64_MEDIA_INSN_NSB; goto extract_sfmt_byterev; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 14 :
        if ((entire_insn & 0xfc0f000f) == 0xe0000)
          { itype = SH64_MEDIA_INSN_MULUL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 15 :
        if ((entire_insn & 0xfc0ffc0f) == 0xffc00)
          { itype = SH64_MEDIA_INSN_BYTEREV; goto extract_sfmt_byterev; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 16 :
        if ((entire_insn & 0xfc0f000f) == 0x4000000)
          { itype = SH64_MEDIA_INSN_SHLLDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 17 :
        if ((entire_insn & 0xfc0f000f) == 0x4010000)
          { itype = SH64_MEDIA_INSN_SHLLD; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 18 :
        if ((entire_insn & 0xfc0f000f) == 0x4020000)
          { itype = SH64_MEDIA_INSN_SHLRDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 19 :
        if ((entire_insn & 0xfc0f000f) == 0x4030000)
          { itype = SH64_MEDIA_INSN_SHLRD; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 22 :
        if ((entire_insn & 0xfc0f000f) == 0x4060000)
          { itype = SH64_MEDIA_INSN_SHARDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 23 :
        if ((entire_insn & 0xfc0f000f) == 0x4070000)
          { itype = SH64_MEDIA_INSN_SHARD; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 25 :
        if ((entire_insn & 0xfc0f000f) == 0x4090000)
          { itype = SH64_MEDIA_INSN_OR; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 27 :
        if ((entire_insn & 0xfc0f000f) == 0x40b0000)
          { itype = SH64_MEDIA_INSN_AND; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 29 :
        if ((entire_insn & 0xfc0f000f) == 0x40d0000)
          { itype = SH64_MEDIA_INSN_XOR; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 30 :
        if ((entire_insn & 0xfc0f000f) == 0x40e0000)
          { itype = SH64_MEDIA_INSN_MULSL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 31 :
        if ((entire_insn & 0xfc0f000f) == 0x40f0000)
          { itype = SH64_MEDIA_INSN_ANDC; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 33 :
        if ((entire_insn & 0xfc0f000f) == 0x8010000)
          { itype = SH64_MEDIA_INSN_MADDW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 34 :
        if ((entire_insn & 0xfc0f000f) == 0x8020000)
          { itype = SH64_MEDIA_INSN_MADDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 36 :
        if ((entire_insn & 0xfc0f000f) == 0x8040000)
          { itype = SH64_MEDIA_INSN_MADDSUB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 37 :
        if ((entire_insn & 0xfc0f000f) == 0x8050000)
          { itype = SH64_MEDIA_INSN_MADDSW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 38 :
        if ((entire_insn & 0xfc0f000f) == 0x8060000)
          { itype = SH64_MEDIA_INSN_MADDSL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 41 :
        if ((entire_insn & 0xfc0f000f) == 0x8090000)
          { itype = SH64_MEDIA_INSN_MSUBW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 42 :
        if ((entire_insn & 0xfc0f000f) == 0x80a0000)
          { itype = SH64_MEDIA_INSN_MSUBL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 44 :
        if ((entire_insn & 0xfc0f000f) == 0x80c0000)
          { itype = SH64_MEDIA_INSN_MSUBSUB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 45 :
        if ((entire_insn & 0xfc0f000f) == 0x80d0000)
          { itype = SH64_MEDIA_INSN_MSUBSW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 46 :
        if ((entire_insn & 0xfc0f000f) == 0x80e0000)
          { itype = SH64_MEDIA_INSN_MSUBSL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 49 :
        if ((entire_insn & 0xfc0f000f) == 0xc010000)
          { itype = SH64_MEDIA_INSN_MSHLLDW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 50 :
        if ((entire_insn & 0xfc0f000f) == 0xc020000)
          { itype = SH64_MEDIA_INSN_MSHLLDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 53 :
        if ((entire_insn & 0xfc0f000f) == 0xc050000)
          { itype = SH64_MEDIA_INSN_MSHALDSW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 54 :
        if ((entire_insn & 0xfc0f000f) == 0xc060000)
          { itype = SH64_MEDIA_INSN_MSHALDSL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 57 :
        if ((entire_insn & 0xfc0f000f) == 0xc090000)
          { itype = SH64_MEDIA_INSN_MSHARDW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 58 :
        if ((entire_insn & 0xfc0f000f) == 0xc0a0000)
          { itype = SH64_MEDIA_INSN_MSHARDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 59 :
        if ((entire_insn & 0xfc0f000f) == 0xc0b0000)
          { itype = SH64_MEDIA_INSN_MSHARDSQ; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 61 :
        if ((entire_insn & 0xfc0f000f) == 0xc0d0000)
          { itype = SH64_MEDIA_INSN_MSHLRDW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 62 :
        if ((entire_insn & 0xfc0f000f) == 0xc0e0000)
          { itype = SH64_MEDIA_INSN_MSHLRDL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 86 :
        if ((entire_insn & 0xfc0f000f) == 0x14060000)
          { itype = SH64_MEDIA_INSN_FIPRS; goto extract_sfmt_fiprs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 94 :
        if ((entire_insn & 0xfc0f000f) == 0x140e0000)
          { itype = SH64_MEDIA_INSN_FTRVS; goto extract_sfmt_ftrvs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 96 :
        if ((entire_insn & 0xfc0f000f) == 0x18000000)
          { itype = SH64_MEDIA_INSN_FABSS; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 97 :
        if ((entire_insn & 0xfc0f000f) == 0x18010000)
          { itype = SH64_MEDIA_INSN_FABSD; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 98 :
        if ((entire_insn & 0xfc0f000f) == 0x18020000)
          { itype = SH64_MEDIA_INSN_FNEGS; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 99 :
        if ((entire_insn & 0xfc0f000f) == 0x18030000)
          { itype = SH64_MEDIA_INSN_FNEGD; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 112 :
        if ((entire_insn & 0xfc0ffc0f) == 0x1c00fc00)
          { itype = SH64_MEDIA_INSN_FMOVLS; goto extract_sfmt_fmovls; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 113 :
        if ((entire_insn & 0xfc0ffc0f) == 0x1c01fc00)
          { itype = SH64_MEDIA_INSN_FMOVQD; goto extract_sfmt_fmovqd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 114 :
        if ((entire_insn & 0xfffffc0f) == 0x1ff2fc00)
          { itype = SH64_MEDIA_INSN_FGETSCR; goto extract_sfmt_fgetscr; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 120 :
        if ((entire_insn & 0xfc0f000f) == 0x1c080000)
          { itype = SH64_MEDIA_INSN_FLDXS; goto extract_sfmt_fldxs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 121 :
        if ((entire_insn & 0xfc0f000f) == 0x1c090000)
          { itype = SH64_MEDIA_INSN_FLDXD; goto extract_sfmt_fldxd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 125 :
        if ((entire_insn & 0xfc0f000f) == 0x1c0d0000)
          { itype = SH64_MEDIA_INSN_FLDXP; goto extract_sfmt_fldxp; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 129 :
        if ((entire_insn & 0xfc0f000f) == 0x20010000)
          { itype = SH64_MEDIA_INSN_CMVEQ; goto extract_sfmt_cmveq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 131 :
        if ((entire_insn & 0xfc0f000f) == 0x20030000)
          { itype = SH64_MEDIA_INSN_SWAPQ; goto extract_sfmt_swapq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 133 :
        if ((entire_insn & 0xfc0f000f) == 0x20050000)
          { itype = SH64_MEDIA_INSN_CMVNE; goto extract_sfmt_cmveq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 159 :
        if ((entire_insn & 0xfc0ffc0f) == 0x240ffc00)
          { itype = SH64_MEDIA_INSN_GETCON; goto extract_sfmt_getcon; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 160 :
        if ((entire_insn & 0xfc0f000f) == 0x28000000)
          { itype = SH64_MEDIA_INSN_MCMPEQB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 161 :
        if ((entire_insn & 0xfc0f000f) == 0x28010000)
          { itype = SH64_MEDIA_INSN_MCMPEQW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 162 :
        if ((entire_insn & 0xfc0f000f) == 0x28020000)
          { itype = SH64_MEDIA_INSN_MCMPEQL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 164 :
        if ((entire_insn & 0xfc0f000f) == 0x28040000)
          { itype = SH64_MEDIA_INSN_MCMPGTUB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 165 :
        if ((entire_insn & 0xfc0f000f) == 0x28050000)
          { itype = SH64_MEDIA_INSN_MCMPGTW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 166 :
        if ((entire_insn & 0xfc0f000f) == 0x28060000)
          { itype = SH64_MEDIA_INSN_MCMPGTL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 167 :
        if ((entire_insn & 0xfc0f000f) == 0x28070000)
          { itype = SH64_MEDIA_INSN_MEXTR1; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 169 :
        if ((entire_insn & 0xfc0ffc0f) == 0x2809fc00)
          { itype = SH64_MEDIA_INSN_MABSW; goto extract_sfmt_byterev; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 170 :
        if ((entire_insn & 0xfc0ffc0f) == 0x280afc00)
          { itype = SH64_MEDIA_INSN_MABSL; goto extract_sfmt_byterev; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 171 :
        if ((entire_insn & 0xfc0f000f) == 0x280b0000)
          { itype = SH64_MEDIA_INSN_MEXTR2; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 173 :
        if ((entire_insn & 0xfc0f000f) == 0x280d0000)
          { itype = SH64_MEDIA_INSN_MPERMW; goto extract_sfmt_mpermw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 175 :
        if ((entire_insn & 0xfc0f000f) == 0x280f0000)
          { itype = SH64_MEDIA_INSN_MEXTR3; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 176 :
        if ((entire_insn & 0xfc0f000f) == 0x2c000000)
          { itype = SH64_MEDIA_INSN_MSHFLOB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 177 :
        if ((entire_insn & 0xfc0f000f) == 0x2c010000)
          { itype = SH64_MEDIA_INSN_MSHFLOW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 178 :
        if ((entire_insn & 0xfc0f000f) == 0x2c020000)
          { itype = SH64_MEDIA_INSN_MSHFLOL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 179 :
        if ((entire_insn & 0xfc0f000f) == 0x2c030000)
          { itype = SH64_MEDIA_INSN_MEXTR4; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 180 :
        if ((entire_insn & 0xfc0f000f) == 0x2c040000)
          { itype = SH64_MEDIA_INSN_MSHFHIB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 181 :
        if ((entire_insn & 0xfc0f000f) == 0x2c050000)
          { itype = SH64_MEDIA_INSN_MSHFHIW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 182 :
        if ((entire_insn & 0xfc0f000f) == 0x2c060000)
          { itype = SH64_MEDIA_INSN_MSHFHIL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 183 :
        if ((entire_insn & 0xfc0f000f) == 0x2c070000)
          { itype = SH64_MEDIA_INSN_MEXTR5; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 187 :
        if ((entire_insn & 0xfc0f000f) == 0x2c0b0000)
          { itype = SH64_MEDIA_INSN_MEXTR6; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 191 :
        if ((entire_insn & 0xfc0f000f) == 0x2c0f0000)
          { itype = SH64_MEDIA_INSN_MEXTR7; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 192 :
        if ((entire_insn & 0xfc0f000f) == 0x30000000)
          { itype = SH64_MEDIA_INSN_FMOVSL; goto extract_sfmt_fmovsl; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 193 :
        if ((entire_insn & 0xfc0f000f) == 0x30010000)
          { itype = SH64_MEDIA_INSN_FMOVDQ; goto extract_sfmt_fmovdq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 194 :
        if ((entire_insn & 0xfc0f03ff) == 0x300203f0)
          { itype = SH64_MEDIA_INSN_FPUTSCR; goto extract_sfmt_fputscr; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 200 :
        if ((entire_insn & 0xfc0f000f) == 0x30080000)
          { itype = SH64_MEDIA_INSN_FCMPEQS; goto extract_sfmt_fcmpeqs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 201 :
        if ((entire_insn & 0xfc0f000f) == 0x30090000)
          { itype = SH64_MEDIA_INSN_FCMPEQD; goto extract_sfmt_fcmpeqd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 202 :
        if ((entire_insn & 0xfc0f000f) == 0x300a0000)
          { itype = SH64_MEDIA_INSN_FCMPUNS; goto extract_sfmt_fcmpeqs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 203 :
        if ((entire_insn & 0xfc0f000f) == 0x300b0000)
          { itype = SH64_MEDIA_INSN_FCMPUND; goto extract_sfmt_fcmpeqd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 204 :
        if ((entire_insn & 0xfc0f000f) == 0x300c0000)
          { itype = SH64_MEDIA_INSN_FCMPGTS; goto extract_sfmt_fcmpeqs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 205 :
        if ((entire_insn & 0xfc0f000f) == 0x300d0000)
          { itype = SH64_MEDIA_INSN_FCMPGTD; goto extract_sfmt_fcmpeqd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 206 :
        if ((entire_insn & 0xfc0f000f) == 0x300e0000)
          { itype = SH64_MEDIA_INSN_FCMPGES; goto extract_sfmt_fcmpeqs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 207 :
        if ((entire_insn & 0xfc0f000f) == 0x300f0000)
          { itype = SH64_MEDIA_INSN_FCMPGED; goto extract_sfmt_fcmpeqd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 208 :
        if ((entire_insn & 0xfc0f000f) == 0x34000000)
          { itype = SH64_MEDIA_INSN_FADDS; goto extract_sfmt_fadds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 209 :
        if ((entire_insn & 0xfc0f000f) == 0x34010000)
          { itype = SH64_MEDIA_INSN_FADDD; goto extract_sfmt_faddd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 210 :
        if ((entire_insn & 0xfc0f000f) == 0x34020000)
          { itype = SH64_MEDIA_INSN_FSUBS; goto extract_sfmt_fadds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 211 :
        if ((entire_insn & 0xfc0f000f) == 0x34030000)
          { itype = SH64_MEDIA_INSN_FSUBD; goto extract_sfmt_faddd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 212 :
        if ((entire_insn & 0xfc0f000f) == 0x34040000)
          { itype = SH64_MEDIA_INSN_FDIVS; goto extract_sfmt_fadds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 213 :
        if ((entire_insn & 0xfc0f000f) == 0x34050000)
          { itype = SH64_MEDIA_INSN_FDIVD; goto extract_sfmt_faddd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 214 :
        if ((entire_insn & 0xfc0f000f) == 0x34060000)
          { itype = SH64_MEDIA_INSN_FMULS; goto extract_sfmt_fadds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 215 :
        if ((entire_insn & 0xfc0f000f) == 0x34070000)
          { itype = SH64_MEDIA_INSN_FMULD; goto extract_sfmt_faddd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 222 :
        if ((entire_insn & 0xfc0f000f) == 0x340e0000)
          { itype = SH64_MEDIA_INSN_FMACS; goto extract_sfmt_fmacs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 224 :
        if ((entire_insn & 0xfc0f000f) == 0x38000000)
          { itype = SH64_MEDIA_INSN_FMOVS; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 225 :
        if ((entire_insn & 0xfc0f000f) == 0x38010000)
          { itype = SH64_MEDIA_INSN_FMOVD; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 228 :
        if ((entire_insn & 0xfc0f000f) == 0x38040000)
          { itype = SH64_MEDIA_INSN_FSQRTS; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 229 :
        if ((entire_insn & 0xfc0f000f) == 0x38050000)
          { itype = SH64_MEDIA_INSN_FSQRTD; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 230 :
        if ((entire_insn & 0xfc0f000f) == 0x38060000)
          { itype = SH64_MEDIA_INSN_FCNVSD; goto extract_sfmt_fcnvsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 231 :
        if ((entire_insn & 0xfc0f000f) == 0x38070000)
          { itype = SH64_MEDIA_INSN_FCNVDS; goto extract_sfmt_fcnvds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 232 :
        if ((entire_insn & 0xfc0f000f) == 0x38080000)
          { itype = SH64_MEDIA_INSN_FTRCSL; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 233 :
        if ((entire_insn & 0xfc0f000f) == 0x38090000)
          { itype = SH64_MEDIA_INSN_FTRCDQ; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 234 :
        if ((entire_insn & 0xfc0f000f) == 0x380a0000)
          { itype = SH64_MEDIA_INSN_FTRCSQ; goto extract_sfmt_fcnvsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 235 :
        if ((entire_insn & 0xfc0f000f) == 0x380b0000)
          { itype = SH64_MEDIA_INSN_FTRCDL; goto extract_sfmt_fcnvds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 236 :
        if ((entire_insn & 0xfc0f000f) == 0x380c0000)
          { itype = SH64_MEDIA_INSN_FLOATLS; goto extract_sfmt_fabss; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 237 :
        if ((entire_insn & 0xfc0f000f) == 0x380d0000)
          { itype = SH64_MEDIA_INSN_FLOATQD; goto extract_sfmt_fabsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 238 :
        if ((entire_insn & 0xfc0f000f) == 0x380e0000)
          { itype = SH64_MEDIA_INSN_FLOATLD; goto extract_sfmt_fcnvsd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 239 :
        if ((entire_insn & 0xfc0f000f) == 0x380f0000)
          { itype = SH64_MEDIA_INSN_FLOATQS; goto extract_sfmt_fcnvds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 248 :
        if ((entire_insn & 0xfc0f000f) == 0x3c080000)
          { itype = SH64_MEDIA_INSN_FSTXS; goto extract_sfmt_fstxs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 249 :
        if ((entire_insn & 0xfc0f000f) == 0x3c090000)
          { itype = SH64_MEDIA_INSN_FSTXD; goto extract_sfmt_fstxd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 253 :
        if ((entire_insn & 0xfc0f000f) == 0x3c0d0000)
          { itype = SH64_MEDIA_INSN_FSTXP; goto extract_sfmt_fldxp; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 256 :
        if ((entire_insn & 0xfc0f000f) == 0x40000000)
          { itype = SH64_MEDIA_INSN_LDXB; goto extract_sfmt_ldxb; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 257 :
        if ((entire_insn & 0xfc0f000f) == 0x40010000)
          { itype = SH64_MEDIA_INSN_LDXW; goto extract_sfmt_ldxw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 258 :
        if ((entire_insn & 0xfc0f000f) == 0x40020000)
          { itype = SH64_MEDIA_INSN_LDXL; goto extract_sfmt_ldxl; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 259 :
        if ((entire_insn & 0xfc0f000f) == 0x40030000)
          { itype = SH64_MEDIA_INSN_LDXQ; goto extract_sfmt_ldxq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 260 :
        if ((entire_insn & 0xfc0f000f) == 0x40040000)
          { itype = SH64_MEDIA_INSN_LDXUB; goto extract_sfmt_ldxub; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 261 :
        if ((entire_insn & 0xfc0f000f) == 0x40050000)
          { itype = SH64_MEDIA_INSN_LDXUW; goto extract_sfmt_ldxuw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 273 :
        if ((entire_insn & 0xff8ffc0f) == 0x4401fc00)
          { itype = SH64_MEDIA_INSN_BLINK; goto extract_sfmt_blink; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 277 :
        if ((entire_insn & 0xff8ffc0f) == 0x4405fc00)
          { itype = SH64_MEDIA_INSN_GETTR; goto extract_sfmt_gettr; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 288 :
        if ((entire_insn & 0xfc0f000f) == 0x48000000)
          { itype = SH64_MEDIA_INSN_MSADUBQ; goto extract_sfmt_mcmv; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 289 :
        if ((entire_insn & 0xfc0f000f) == 0x48010000)
          { itype = SH64_MEDIA_INSN_MMACFXWL; goto extract_sfmt_mcmv; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 291 :
        if ((entire_insn & 0xfc0f000f) == 0x48030000)
          { itype = SH64_MEDIA_INSN_MCMV; goto extract_sfmt_mcmv; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 293 :
        if ((entire_insn & 0xfc0f000f) == 0x48050000)
          { itype = SH64_MEDIA_INSN_MMACNFX_WL; goto extract_sfmt_mcmv; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 297 :
        if ((entire_insn & 0xfc0f000f) == 0x48090000)
          { itype = SH64_MEDIA_INSN_MMULSUMWQ; goto extract_sfmt_mcmv; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 305 :
        if ((entire_insn & 0xfc0f000f) == 0x4c010000)
          { itype = SH64_MEDIA_INSN_MMULW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 306 :
        if ((entire_insn & 0xfc0f000f) == 0x4c020000)
          { itype = SH64_MEDIA_INSN_MMULL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 309 :
        if ((entire_insn & 0xfc0f000f) == 0x4c050000)
          { itype = SH64_MEDIA_INSN_MMULFXW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 310 :
        if ((entire_insn & 0xfc0f000f) == 0x4c060000)
          { itype = SH64_MEDIA_INSN_MMULFXL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 312 :
        if ((entire_insn & 0xfc0f000f) == 0x4c080000)
          { itype = SH64_MEDIA_INSN_MCNVSWB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 313 :
        if ((entire_insn & 0xfc0f000f) == 0x4c090000)
          { itype = SH64_MEDIA_INSN_MMULFXRPW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 314 :
        if ((entire_insn & 0xfc0f000f) == 0x4c0a0000)
          { itype = SH64_MEDIA_INSN_MMULLOWL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 316 :
        if ((entire_insn & 0xfc0f000f) == 0x4c0c0000)
          { itype = SH64_MEDIA_INSN_MCNVSWUB; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 317 :
        if ((entire_insn & 0xfc0f000f) == 0x4c0d0000)
          { itype = SH64_MEDIA_INSN_MCNVSLW; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 318 :
        if ((entire_insn & 0xfc0f000f) == 0x4c0e0000)
          { itype = SH64_MEDIA_INSN_MMULHIWL; goto extract_sfmt_add; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 384 :
        if ((entire_insn & 0xfc0f000f) == 0x60000000)
          { itype = SH64_MEDIA_INSN_STXB; goto extract_sfmt_stxb; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 385 :
        if ((entire_insn & 0xfc0f000f) == 0x60010000)
          { itype = SH64_MEDIA_INSN_STXW; goto extract_sfmt_stxw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 386 :
        if ((entire_insn & 0xfc0f000f) == 0x60020000)
          { itype = SH64_MEDIA_INSN_STXL; goto extract_sfmt_stxl; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 387 :
        if ((entire_insn & 0xfc0f000f) == 0x60030000)
          { itype = SH64_MEDIA_INSN_STXQ; goto extract_sfmt_stxq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 401 :
        if ((entire_insn & 0xfc0f018f) == 0x64010000)
          { itype = SH64_MEDIA_INSN_BEQ; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 403 :
        if ((entire_insn & 0xfc0f018f) == 0x64030000)
          { itype = SH64_MEDIA_INSN_BGE; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 405 :
        if ((entire_insn & 0xfc0f018f) == 0x64050000)
          { itype = SH64_MEDIA_INSN_BNE; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 407 :
        if ((entire_insn & 0xfc0f018f) == 0x64070000)
          { itype = SH64_MEDIA_INSN_BGT; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 411 :
        if ((entire_insn & 0xfc0f018f) == 0x640b0000)
          { itype = SH64_MEDIA_INSN_BGEU; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 415 :
        if ((entire_insn & 0xfc0f018f) == 0x640f0000)
          { itype = SH64_MEDIA_INSN_BGTU; goto extract_sfmt_beq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 417 :
        if ((entire_insn & 0xffff018f) == 0x6bf10000)
          { itype = SH64_MEDIA_INSN_PTABS; goto extract_sfmt_ptabs; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 421 :
        if ((entire_insn & 0xffff018f) == 0x6bf50000)
          { itype = SH64_MEDIA_INSN_PTREL; goto extract_sfmt_ptrel; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 432 :
        if ((entire_insn & 0xffffffff) == 0x6ff0fff0)
          { itype = SH64_MEDIA_INSN_NOP; goto extract_sfmt_nop; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 433 :
        if ((entire_insn & 0xfc0fffff) == 0x6c01fff0)
          { itype = SH64_MEDIA_INSN_TRAPA; goto extract_sfmt_trapa; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 434 :
        if ((entire_insn & 0xffffffff) == 0x6ff2fff0)
          { itype = SH64_MEDIA_INSN_SYNCI; goto extract_sfmt_nop; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 435 :
        if ((entire_insn & 0xffffffff) == 0x6ff3fff0)
          { itype = SH64_MEDIA_INSN_RTE; goto extract_sfmt_nop; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 437 :
        if ((entire_insn & 0xffffffff) == 0x6ff5fff0)
          { itype = SH64_MEDIA_INSN_BRK; goto extract_sfmt_brk; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 438 :
        if ((entire_insn & 0xffffffff) == 0x6ff6fff0)
          { itype = SH64_MEDIA_INSN_SYNCO; goto extract_sfmt_nop; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 439 :
        if ((entire_insn & 0xffffffff) == 0x6ff7fff0)
          { itype = SH64_MEDIA_INSN_SLEEP; goto extract_sfmt_nop; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 447 :
        if ((entire_insn & 0xfc0ffc0f) == 0x6c0ffc00)
          { itype = SH64_MEDIA_INSN_PUTCON; goto extract_sfmt_putcon; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 512 : /* fall through */
      case 513 : /* fall through */
      case 514 : /* fall through */
      case 515 : /* fall through */
      case 516 : /* fall through */
      case 517 : /* fall through */
      case 518 : /* fall through */
      case 519 : /* fall through */
      case 520 : /* fall through */
      case 521 : /* fall through */
      case 522 : /* fall through */
      case 523 : /* fall through */
      case 524 : /* fall through */
      case 525 : /* fall through */
      case 526 : /* fall through */
      case 527 :
        if ((entire_insn & 0xfc00000f) == 0x80000000)
          { itype = SH64_MEDIA_INSN_LDB; goto extract_sfmt_ldb; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 528 : /* fall through */
      case 529 : /* fall through */
      case 530 : /* fall through */
      case 531 : /* fall through */
      case 532 : /* fall through */
      case 533 : /* fall through */
      case 534 : /* fall through */
      case 535 : /* fall through */
      case 536 : /* fall through */
      case 537 : /* fall through */
      case 538 : /* fall through */
      case 539 : /* fall through */
      case 540 : /* fall through */
      case 541 : /* fall through */
      case 542 : /* fall through */
      case 543 :
        if ((entire_insn & 0xfc00000f) == 0x84000000)
          { itype = SH64_MEDIA_INSN_LDW; goto extract_sfmt_lduw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 544 : /* fall through */
      case 545 : /* fall through */
      case 546 : /* fall through */
      case 547 : /* fall through */
      case 548 : /* fall through */
      case 549 : /* fall through */
      case 550 : /* fall through */
      case 551 : /* fall through */
      case 552 : /* fall through */
      case 553 : /* fall through */
      case 554 : /* fall through */
      case 555 : /* fall through */
      case 556 : /* fall through */
      case 557 : /* fall through */
      case 558 : /* fall through */
      case 559 :
        if ((entire_insn & 0xfc00000f) == 0x88000000)
          { itype = SH64_MEDIA_INSN_LDL; goto extract_sfmt_ldl; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 560 : /* fall through */
      case 561 : /* fall through */
      case 562 : /* fall through */
      case 563 : /* fall through */
      case 564 : /* fall through */
      case 565 : /* fall through */
      case 566 : /* fall through */
      case 567 : /* fall through */
      case 568 : /* fall through */
      case 569 : /* fall through */
      case 570 : /* fall through */
      case 571 : /* fall through */
      case 572 : /* fall through */
      case 573 : /* fall through */
      case 574 : /* fall through */
      case 575 :
        if ((entire_insn & 0xfc00000f) == 0x8c000000)
          { itype = SH64_MEDIA_INSN_LDQ; goto extract_sfmt_ldq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 576 : /* fall through */
      case 577 : /* fall through */
      case 578 : /* fall through */
      case 579 : /* fall through */
      case 580 : /* fall through */
      case 581 : /* fall through */
      case 582 : /* fall through */
      case 583 : /* fall through */
      case 584 : /* fall through */
      case 585 : /* fall through */
      case 586 : /* fall through */
      case 587 : /* fall through */
      case 588 : /* fall through */
      case 589 : /* fall through */
      case 590 : /* fall through */
      case 591 :
        if ((entire_insn & 0xfc00000f) == 0x90000000)
          { itype = SH64_MEDIA_INSN_LDUB; goto extract_sfmt_ldb; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 592 : /* fall through */
      case 593 : /* fall through */
      case 594 : /* fall through */
      case 595 : /* fall through */
      case 596 : /* fall through */
      case 597 : /* fall through */
      case 598 : /* fall through */
      case 599 : /* fall through */
      case 600 : /* fall through */
      case 601 : /* fall through */
      case 602 : /* fall through */
      case 603 : /* fall through */
      case 604 : /* fall through */
      case 605 : /* fall through */
      case 606 : /* fall through */
      case 607 :
        if ((entire_insn & 0xfc00000f) == 0x94000000)
          { itype = SH64_MEDIA_INSN_FLDS; goto extract_sfmt_flds; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 608 : /* fall through */
      case 609 : /* fall through */
      case 610 : /* fall through */
      case 611 : /* fall through */
      case 612 : /* fall through */
      case 613 : /* fall through */
      case 614 : /* fall through */
      case 615 : /* fall through */
      case 616 : /* fall through */
      case 617 : /* fall through */
      case 618 : /* fall through */
      case 619 : /* fall through */
      case 620 : /* fall through */
      case 621 : /* fall through */
      case 622 : /* fall through */
      case 623 :
        if ((entire_insn & 0xfc00000f) == 0x98000000)
          { itype = SH64_MEDIA_INSN_FLDP; goto extract_sfmt_fldp; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 624 : /* fall through */
      case 625 : /* fall through */
      case 626 : /* fall through */
      case 627 : /* fall through */
      case 628 : /* fall through */
      case 629 : /* fall through */
      case 630 : /* fall through */
      case 631 : /* fall through */
      case 632 : /* fall through */
      case 633 : /* fall through */
      case 634 : /* fall through */
      case 635 : /* fall through */
      case 636 : /* fall through */
      case 637 : /* fall through */
      case 638 : /* fall through */
      case 639 :
        if ((entire_insn & 0xfc00000f) == 0x9c000000)
          { itype = SH64_MEDIA_INSN_FLDD; goto extract_sfmt_fldd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 655 :
        if ((entire_insn & 0xfc00000f) == 0xa0000000)
          { itype = SH64_MEDIA_INSN_STB; goto extract_sfmt_stb; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 671 :
        if ((entire_insn & 0xfc00000f) == 0xa4000000)
          { itype = SH64_MEDIA_INSN_STW; goto extract_sfmt_stw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 687 :
        if ((entire_insn & 0xfc00000f) == 0xa8000000)
          { itype = SH64_MEDIA_INSN_STL; goto extract_sfmt_stl; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 703 :
        if ((entire_insn & 0xfc00000f) == 0xac000000)
          { itype = SH64_MEDIA_INSN_STQ; goto extract_sfmt_stq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 719 :
        if ((entire_insn & 0xfc00000f) == 0xb0000000)
          { itype = SH64_MEDIA_INSN_LDUW; goto extract_sfmt_lduw; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 735 :
        if ((entire_insn & 0xfc00000f) == 0xb4000000)
          { itype = SH64_MEDIA_INSN_FSTS; goto extract_sfmt_fsts; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 751 :
        if ((entire_insn & 0xfc00000f) == 0xb8000000)
          { itype = SH64_MEDIA_INSN_FSTP; goto extract_sfmt_fldp; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
        if ((entire_insn & 0xfc00000f) == 0xbc000000)
          { itype = SH64_MEDIA_INSN_FSTD; goto extract_sfmt_fstd; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 770 :
        if ((entire_insn & 0xfc0f000f) == 0xc0020000)
          { itype = SH64_MEDIA_INSN_LDLOL; goto extract_sfmt_ldlol; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 771 :
        if ((entire_insn & 0xfc0f000f) == 0xc0030000)
          { itype = SH64_MEDIA_INSN_LDLOQ; goto extract_sfmt_ldloq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 774 :
        if ((entire_insn & 0xfc0f000f) == 0xc0060000)
          { itype = SH64_MEDIA_INSN_LDHIL; goto extract_sfmt_ldhil; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 775 :
        if ((entire_insn & 0xfc0f000f) == 0xc0070000)
          { itype = SH64_MEDIA_INSN_LDHIQ; goto extract_sfmt_ldhiq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 783 :
        if ((entire_insn & 0xfc0f000f) == 0xc00f0000)
          { itype = SH64_MEDIA_INSN_GETCFG; goto extract_sfmt_getcfg; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 784 :
        if ((entire_insn & 0xfc0f000f) == 0xc4000000)
          { itype = SH64_MEDIA_INSN_SHLLIL; goto extract_sfmt_sharil; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 785 :
        if ((entire_insn & 0xfc0f000f) == 0xc4010000)
          { itype = SH64_MEDIA_INSN_SHLLI; goto extract_sfmt_shari; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 786 :
        if ((entire_insn & 0xfc0f000f) == 0xc4020000)
          { itype = SH64_MEDIA_INSN_SHLRIL; goto extract_sfmt_sharil; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 787 :
        if ((entire_insn & 0xfc0f000f) == 0xc4030000)
          { itype = SH64_MEDIA_INSN_SHLRI; goto extract_sfmt_shari; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 790 :
        if ((entire_insn & 0xfc0f000f) == 0xc4060000)
          { itype = SH64_MEDIA_INSN_SHARIL; goto extract_sfmt_sharil; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 791 :
        if ((entire_insn & 0xfc0f000f) == 0xc4070000)
          { itype = SH64_MEDIA_INSN_SHARI; goto extract_sfmt_shari; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 797 :
        if ((entire_insn & 0xfc0f000f) == 0xc40d0000)
          { itype = SH64_MEDIA_INSN_XORI; goto extract_sfmt_xori; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 800 : /* fall through */
      case 801 : /* fall through */
      case 802 : /* fall through */
      case 803 : /* fall through */
      case 804 : /* fall through */
      case 805 : /* fall through */
      case 806 : /* fall through */
      case 807 : /* fall through */
      case 808 : /* fall through */
      case 809 : /* fall through */
      case 810 : /* fall through */
      case 811 : /* fall through */
      case 812 : /* fall through */
      case 813 : /* fall through */
      case 814 : /* fall through */
      case 815 :
        if ((entire_insn & 0xfc00000f) == 0xc8000000)
          { itype = SH64_MEDIA_INSN_SHORI; goto extract_sfmt_shori; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 816 : /* fall through */
      case 817 : /* fall through */
      case 818 : /* fall through */
      case 819 : /* fall through */
      case 820 : /* fall through */
      case 821 : /* fall through */
      case 822 : /* fall through */
      case 823 : /* fall through */
      case 824 : /* fall through */
      case 825 : /* fall through */
      case 826 : /* fall through */
      case 827 : /* fall through */
      case 828 : /* fall through */
      case 829 : /* fall through */
      case 830 : /* fall through */
      case 831 :
        if ((entire_insn & 0xfc00000f) == 0xcc000000)
          { itype = SH64_MEDIA_INSN_MOVI; goto extract_sfmt_movi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 832 : /* fall through */
      case 833 : /* fall through */
      case 834 : /* fall through */
      case 835 : /* fall through */
      case 836 : /* fall through */
      case 837 : /* fall through */
      case 838 : /* fall through */
      case 839 : /* fall through */
      case 840 : /* fall through */
      case 841 : /* fall through */
      case 842 : /* fall through */
      case 843 : /* fall through */
      case 844 : /* fall through */
      case 845 : /* fall through */
      case 846 : /* fall through */
      case 847 :
        if ((entire_insn & 0xfc00000f) == 0xd0000000)
          { itype = SH64_MEDIA_INSN_ADDI; goto extract_sfmt_addi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 848 : /* fall through */
      case 849 : /* fall through */
      case 850 : /* fall through */
      case 851 : /* fall through */
      case 852 : /* fall through */
      case 853 : /* fall through */
      case 854 : /* fall through */
      case 855 : /* fall through */
      case 856 : /* fall through */
      case 857 : /* fall through */
      case 858 : /* fall through */
      case 859 : /* fall through */
      case 860 : /* fall through */
      case 861 : /* fall through */
      case 862 : /* fall through */
      case 863 :
        if ((entire_insn & 0xfc00000f) == 0xd4000000)
          { itype = SH64_MEDIA_INSN_ADDIL; goto extract_sfmt_addi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 864 : /* fall through */
      case 865 : /* fall through */
      case 866 : /* fall through */
      case 867 : /* fall through */
      case 868 : /* fall through */
      case 869 : /* fall through */
      case 870 : /* fall through */
      case 871 : /* fall through */
      case 872 : /* fall through */
      case 873 : /* fall through */
      case 874 : /* fall through */
      case 875 : /* fall through */
      case 876 : /* fall through */
      case 877 : /* fall through */
      case 878 : /* fall through */
      case 879 :
        if ((entire_insn & 0xfc00000f) == 0xd8000000)
          { itype = SH64_MEDIA_INSN_ANDI; goto extract_sfmt_addi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 880 : /* fall through */
      case 881 : /* fall through */
      case 882 : /* fall through */
      case 883 : /* fall through */
      case 884 : /* fall through */
      case 885 : /* fall through */
      case 886 : /* fall through */
      case 887 : /* fall through */
      case 888 : /* fall through */
      case 889 : /* fall through */
      case 890 : /* fall through */
      case 891 : /* fall through */
      case 892 : /* fall through */
      case 893 : /* fall through */
      case 894 : /* fall through */
      case 895 :
        if ((entire_insn & 0xfc00000f) == 0xdc000000)
          { itype = SH64_MEDIA_INSN_ORI; goto extract_sfmt_ori; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 897 :
        if ((entire_insn & 0xfc0ffc0f) == 0xe001fc00)
          { itype = SH64_MEDIA_INSN_PREFI; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 898 :
        if ((entire_insn & 0xfc0f000f) == 0xe0020000)
          { itype = SH64_MEDIA_INSN_STLOL; goto extract_sfmt_stlol; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 899 :
        if ((entire_insn & 0xfc0f000f) == 0xe0030000)
          { itype = SH64_MEDIA_INSN_STLOQ; goto extract_sfmt_stloq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 900 :
        if ((entire_insn & 0xfc0f03ff) == 0xe00403f0)
          { itype = SH64_MEDIA_INSN_ALLOCO; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 901 :
        if ((entire_insn & 0xfc0f03ff) == 0xe00503f0)
          { itype = SH64_MEDIA_INSN_ICBI; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 902 :
        if ((entire_insn & 0xfc0f000f) == 0xe0060000)
          { itype = SH64_MEDIA_INSN_STHIL; goto extract_sfmt_sthil; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 903 :
        if ((entire_insn & 0xfc0f000f) == 0xe0070000)
          { itype = SH64_MEDIA_INSN_STHIQ; goto extract_sfmt_sthiq; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 904 :
        if ((entire_insn & 0xfc0f03ff) == 0xe00803f0)
          { itype = SH64_MEDIA_INSN_OCBP; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 905 :
        if ((entire_insn & 0xfc0f03ff) == 0xe00903f0)
          { itype = SH64_MEDIA_INSN_OCBI; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 908 :
        if ((entire_insn & 0xfc0f03ff) == 0xe00c03f0)
          { itype = SH64_MEDIA_INSN_OCBWB; goto extract_sfmt_alloco; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 911 :
        if ((entire_insn & 0xfc0f000f) == 0xe00f0000)
          { itype = SH64_MEDIA_INSN_PUTCFG; goto extract_sfmt_putcfg; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 913 :
        if ((entire_insn & 0xfc0f018f) == 0xe4010000)
          { itype = SH64_MEDIA_INSN_BEQI; goto extract_sfmt_beqi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      case 917 :
        if ((entire_insn & 0xfc0f018f) == 0xe4050000)
          { itype = SH64_MEDIA_INSN_BNEI; goto extract_sfmt_beqi; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 943 :
        if ((entire_insn & 0xfc00018f) == 0xe8000000)
          { itype = SH64_MEDIA_INSN_PTA; goto extract_sfmt_pta; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
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
      case 959 :
        if ((entire_insn & 0xfc00018f) == 0xec000000)
          { itype = SH64_MEDIA_INSN_PTB; goto extract_sfmt_pta; }
        itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      default : itype = SH64_MEDIA_INSN_X_INVALID; goto extract_sfmt_empty;
      }
    }
  }

  /* The instruction has been decoded, now extract the fields.  */

 extract_sfmt_empty:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_empty", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_add:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_addi:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_left;
    INT f_disp10;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10 = EXTRACT_MSB0_INT (insn, 32, 12, 10);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10) = f_disp10;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi", "f_disp10 0x%x", 'x', f_disp10, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_alloco:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_alloco", "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_beq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beq.f
    UINT f_left;
    UINT f_right;
    UINT f_tra;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_tra) = f_tra;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_beq", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_tra 0x%x", 'x', f_tra, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (in_tra) = f_tra;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_beqi:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beqi.f
    UINT f_left;
    INT f_imm6;
    UINT f_tra;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_imm6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm6) = f_imm6;
  FLD (f_left) = f_left;
  FLD (f_tra) = f_tra;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_beqi", "f_imm6 0x%x", 'x', f_imm6, "f_left 0x%x", 'x', f_left, "f_tra 0x%x", 'x', f_tra, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_tra) = f_tra;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_blink:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_blink.f
    UINT f_trb;
    UINT f_dest;

    f_trb = EXTRACT_MSB0_UINT (insn, 32, 9, 3);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_trb) = f_trb;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_blink", "f_dest 0x%x", 'x', f_dest, "f_trb 0x%x", 'x', f_trb, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_trb) = f_trb;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_brk:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_brk", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_byterev:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_byterev", "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_cmveq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_cmveq", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fabsd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fabsd", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drgh) = f_left_right;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fabss:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fabss", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frgh) = f_left_right;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_faddd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_faddd", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drg) = f_left;
      FLD (in_drh) = f_right;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fadds:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fadds", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frg) = f_left;
      FLD (in_frh) = f_right;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcmpeqd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcmpeqd", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drg) = f_left;
      FLD (in_drh) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcmpeqs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcmpeqs", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frg) = f_left;
      FLD (in_frh) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcnvds:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcnvds", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drgh) = f_left_right;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fcnvsd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fcnvsd", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frgh) = f_left_right;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fgetscr:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_shori.f
    UINT f_dest;

    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fgetscr", "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fiprs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fiprs", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fvg) = f_left;
      FLD (in_fvh) = f_right;
      FLD (out_frf) = f_dest;
      FLD (out_fvg) = f_left;
      FLD (out_fvh) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fldd.f
    UINT f_left;
    SI f_disp10x8;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x8) = f_disp10x8;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldd", "f_disp10x8 0x%x", 'x', f_disp10x8, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldp:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fldd.f
    UINT f_left;
    SI f_disp10x8;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x8) = f_disp10x8;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldp", "f_disp10x8 0x%x", 'x', f_disp10x8, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpf) = f_dest;
      FLD (in_rm) = f_left;
      FLD (out_fpf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_flds:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_flds.f
    UINT f_left;
    SI f_disp10x4;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x4) = f_disp10x4;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_flds", "f_disp10x4 0x%x", 'x', f_disp10x4, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldxd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldxd", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldxp:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldxp", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fpf) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_fpf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fldxs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fldxs", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmacs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmacs", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frf) = f_dest;
      FLD (in_frg) = f_left;
      FLD (in_frh) = f_right;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmovdq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmovdq", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drgh) = f_left_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmovls:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmovls", "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_frf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmovqd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmovqd", "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_drf) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fmovsl:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fmovsl", "f_left_right 0x%x", 'x', f_left_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frgh) = f_left_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fputscr:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fabsd.f
    UINT f_left;
    UINT f_right;
    UINT f_left_right;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
  f_left_right = f_left;

  /* Record the fields for the semantic handler.  */
  FLD (f_left_right) = f_left_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fputscr", "f_left_right 0x%x", 'x', f_left_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frgh) = f_left_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fstd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fldd.f
    UINT f_left;
    SI f_disp10x8;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x8) = f_disp10x8;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fstd", "f_disp10x8 0x%x", 'x', f_disp10x8, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drf) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fsts:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_flds.f
    UINT f_left;
    SI f_disp10x4;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x4) = f_disp10x4;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fsts", "f_disp10x4 0x%x", 'x', f_disp10x4, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frf) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fstxd:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fstxd", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_drf) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_fstxs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_fstxs", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_frf) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ftrvs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ftrvs", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_fvf) = f_dest;
      FLD (in_fvh) = f_right;
      FLD (in_mtrxg) = f_left;
      FLD (out_fvf) = f_dest;
      FLD (out_fvh) = f_right;
      FLD (out_mtrxg) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_getcfg:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_getcfg", "f_disp6 0x%x", 'x', f_disp6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_getcon:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_getcon", "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_gettr:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_blink.f
    UINT f_trb;
    UINT f_dest;

    f_trb = EXTRACT_MSB0_UINT (insn, 32, 9, 3);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_trb) = f_trb;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_gettr", "f_trb 0x%x", 'x', f_trb, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_trb) = f_trb;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldb:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_left;
    INT f_disp10;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10 = EXTRACT_MSB0_INT (insn, 32, 12, 10);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10) = f_disp10;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldb", "f_disp10 0x%x", 'x', f_disp10, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldl:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_flds.f
    UINT f_left;
    SI f_disp10x4;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x4) = f_disp10x4;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldl", "f_disp10x4 0x%x", 'x', f_disp10x4, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fldd.f
    UINT f_left;
    SI f_disp10x8;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x8) = f_disp10x8;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldq", "f_disp10x8 0x%x", 'x', f_disp10x8, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lduw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_lduw.f
    UINT f_left;
    SI f_disp10x2;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x2 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (1));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x2) = f_disp10x2;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lduw", "f_disp10x2 0x%x", 'x', f_disp10x2, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldhil:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldhil", "f_disp6 0x%x", 'x', f_disp6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldhiq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldhiq", "f_disp6 0x%x", 'x', f_disp6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldlol:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldlol", "f_disp6 0x%x", 'x', f_disp6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldloq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldloq", "f_disp6 0x%x", 'x', f_disp6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxb:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxb", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxl:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxl", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxq", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxub:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxub", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxuw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxuw", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ldxw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldxw", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mcmv:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mcmv", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_movi:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_movi.f
    INT f_imm16;
    UINT f_dest;

    f_imm16 = EXTRACT_MSB0_INT (insn, 32, 6, 16);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm16) = f_imm16;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_movi", "f_imm16 0x%x", 'x', f_imm16, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_mpermw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mpermw", "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_nop:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_nop", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_ori:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ori.f
    UINT f_left;
    INT f_imm10;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_imm10 = EXTRACT_MSB0_INT (insn, 32, 12, 10);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm10) = f_imm10;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ori", "f_imm10 0x%x", 'x', f_imm10, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_pta:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_pta.f
    DI f_disp16;
    UINT f_tra;

    f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 6, 16)) << (2))) + (pc));
    f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp16) = f_disp16;
  FLD (f_tra) = f_tra;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_pta", "f_disp16 0x%x", 'x', f_disp16, "f_tra 0x%x", 'x', f_tra, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (out_tra) = f_tra;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ptabs:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beq.f
    UINT f_right;
    UINT f_tra;

    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3);

  /* Record the fields for the semantic handler.  */
  FLD (f_right) = f_right;
  FLD (f_tra) = f_tra;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ptabs", "f_right 0x%x", 'x', f_right, "f_tra 0x%x", 'x', f_tra, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_right;
      FLD (out_tra) = f_tra;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_ptrel:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_beq.f
    UINT f_right;
    UINT f_tra;

    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3);

  /* Record the fields for the semantic handler.  */
  FLD (f_right) = f_right;
  FLD (f_tra) = f_tra;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ptrel", "f_right 0x%x", 'x', f_right, "f_tra 0x%x", 'x', f_tra, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rn) = f_right;
      FLD (out_tra) = f_tra;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_putcfg:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_putcfg", "f_disp6 0x%x", 'x', f_disp6, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_putcon:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_putcon", "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_shari:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_shari.f
    UINT f_left;
    UINT f_uimm6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_uimm6 = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_uimm6) = f_uimm6;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_shari", "f_left 0x%x", 'x', f_left, "f_uimm6 0x%x", 'x', f_uimm6, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sharil:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_shari.f
    UINT f_left;
    UINT f_uimm6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_uimm6 = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  FLD (f_uimm6) = f_uimm6;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sharil", "f_left 0x%x", 'x', f_left, "f_uimm6 0x%x", 'x', f_uimm6, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_shori:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_shori.f
    UINT f_uimm16;
    UINT f_dest;

    f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 6, 16);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_uimm16) = f_uimm16;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_shori", "f_dest 0x%x", 'x', f_dest, "f_uimm16 0x%x", 'x', f_uimm16, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stb:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_left;
    INT f_disp10;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10 = EXTRACT_MSB0_INT (insn, 32, 12, 10);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10) = f_disp10;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stb", "f_disp10 0x%x", 'x', f_disp10, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stl:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_flds.f
    UINT f_left;
    SI f_disp10x4;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x4) = f_disp10x4;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stl", "f_disp10x4 0x%x", 'x', f_disp10x4, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_fldd.f
    UINT f_left;
    SI f_disp10x8;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x8) = f_disp10x8;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stq", "f_disp10x8 0x%x", 'x', f_disp10x8, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_lduw.f
    UINT f_left;
    SI f_disp10x2;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp10x2 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (1));
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp10x2) = f_disp10x2;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stw", "f_disp10x2 0x%x", 'x', f_disp10x2, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sthil:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sthil", "f_disp6 0x%x", 'x', f_disp6, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_sthiq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sthiq", "f_disp6 0x%x", 'x', f_disp6, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stlol:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stlol", "f_disp6 0x%x", 'x', f_disp6, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stloq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_getcfg.f
    UINT f_left;
    INT f_disp6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_disp6) = f_disp6;
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stloq", "f_disp6 0x%x", 'x', f_disp6, "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stxb:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stxb", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stxl:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stxl", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stxq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stxq", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_stxw:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_stxw", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_swapq:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_add.f
    UINT f_left;
    UINT f_right;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_dest) = f_dest;
  FLD (f_left) = f_left;
  FLD (f_right) = f_right;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_swapq", "f_dest 0x%x", 'x', f_dest, "f_left 0x%x", 'x', f_left, "f_right 0x%x", 'x', f_right, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rd) = f_dest;
      FLD (in_rm) = f_left;
      FLD (in_rn) = f_right;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_trapa:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_left) = f_left;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_trapa", "f_left 0x%x", 'x', f_left, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_xori:
  {
    const IDESC *idesc = &sh64_media_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_xori.f
    UINT f_left;
    INT f_imm6;
    UINT f_dest;

    f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6);
    f_imm6 = EXTRACT_MSB0_INT (insn, 32, 16, 6);
    f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm6) = f_imm6;
  FLD (f_left) = f_left;
  FLD (f_dest) = f_dest;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_xori", "f_imm6 0x%x", 'x', f_imm6, "f_left 0x%x", 'x', f_left, "f_dest 0x%x", 'x', f_dest, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
      FLD (in_rm) = f_left;
      FLD (out_rd) = f_dest;
    }
#endif
#undef FLD
    return idesc;
  }

}
