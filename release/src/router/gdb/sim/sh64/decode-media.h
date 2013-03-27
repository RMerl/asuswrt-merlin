/* Decode header for sh64_media.

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

#ifndef SH64_MEDIA_DECODE_H
#define SH64_MEDIA_DECODE_H

extern const IDESC *sh64_media_decode (SIM_CPU *, IADDR,
                                  CGEN_INSN_INT, CGEN_INSN_INT,
                                  ARGBUF *);
extern void sh64_media_init_idesc_table (SIM_CPU *);
extern void sh64_media_sem_init_idesc_table (SIM_CPU *);
extern void sh64_media_semf_init_idesc_table (SIM_CPU *);

/* Enum declaration for instructions in cpu family sh64.  */
typedef enum sh64_media_insn_type {
  SH64_MEDIA_INSN_X_INVALID, SH64_MEDIA_INSN_X_AFTER, SH64_MEDIA_INSN_X_BEFORE, SH64_MEDIA_INSN_X_CTI_CHAIN
 , SH64_MEDIA_INSN_X_CHAIN, SH64_MEDIA_INSN_X_BEGIN, SH64_MEDIA_INSN_ADD, SH64_MEDIA_INSN_ADDL
 , SH64_MEDIA_INSN_ADDI, SH64_MEDIA_INSN_ADDIL, SH64_MEDIA_INSN_ADDZL, SH64_MEDIA_INSN_ALLOCO
 , SH64_MEDIA_INSN_AND, SH64_MEDIA_INSN_ANDC, SH64_MEDIA_INSN_ANDI, SH64_MEDIA_INSN_BEQ
 , SH64_MEDIA_INSN_BEQI, SH64_MEDIA_INSN_BGE, SH64_MEDIA_INSN_BGEU, SH64_MEDIA_INSN_BGT
 , SH64_MEDIA_INSN_BGTU, SH64_MEDIA_INSN_BLINK, SH64_MEDIA_INSN_BNE, SH64_MEDIA_INSN_BNEI
 , SH64_MEDIA_INSN_BRK, SH64_MEDIA_INSN_BYTEREV, SH64_MEDIA_INSN_CMPEQ, SH64_MEDIA_INSN_CMPGT
 , SH64_MEDIA_INSN_CMPGTU, SH64_MEDIA_INSN_CMVEQ, SH64_MEDIA_INSN_CMVNE, SH64_MEDIA_INSN_FABSD
 , SH64_MEDIA_INSN_FABSS, SH64_MEDIA_INSN_FADDD, SH64_MEDIA_INSN_FADDS, SH64_MEDIA_INSN_FCMPEQD
 , SH64_MEDIA_INSN_FCMPEQS, SH64_MEDIA_INSN_FCMPGED, SH64_MEDIA_INSN_FCMPGES, SH64_MEDIA_INSN_FCMPGTD
 , SH64_MEDIA_INSN_FCMPGTS, SH64_MEDIA_INSN_FCMPUND, SH64_MEDIA_INSN_FCMPUNS, SH64_MEDIA_INSN_FCNVDS
 , SH64_MEDIA_INSN_FCNVSD, SH64_MEDIA_INSN_FDIVD, SH64_MEDIA_INSN_FDIVS, SH64_MEDIA_INSN_FGETSCR
 , SH64_MEDIA_INSN_FIPRS, SH64_MEDIA_INSN_FLDD, SH64_MEDIA_INSN_FLDP, SH64_MEDIA_INSN_FLDS
 , SH64_MEDIA_INSN_FLDXD, SH64_MEDIA_INSN_FLDXP, SH64_MEDIA_INSN_FLDXS, SH64_MEDIA_INSN_FLOATLD
 , SH64_MEDIA_INSN_FLOATLS, SH64_MEDIA_INSN_FLOATQD, SH64_MEDIA_INSN_FLOATQS, SH64_MEDIA_INSN_FMACS
 , SH64_MEDIA_INSN_FMOVD, SH64_MEDIA_INSN_FMOVDQ, SH64_MEDIA_INSN_FMOVLS, SH64_MEDIA_INSN_FMOVQD
 , SH64_MEDIA_INSN_FMOVS, SH64_MEDIA_INSN_FMOVSL, SH64_MEDIA_INSN_FMULD, SH64_MEDIA_INSN_FMULS
 , SH64_MEDIA_INSN_FNEGD, SH64_MEDIA_INSN_FNEGS, SH64_MEDIA_INSN_FPUTSCR, SH64_MEDIA_INSN_FSQRTD
 , SH64_MEDIA_INSN_FSQRTS, SH64_MEDIA_INSN_FSTD, SH64_MEDIA_INSN_FSTP, SH64_MEDIA_INSN_FSTS
 , SH64_MEDIA_INSN_FSTXD, SH64_MEDIA_INSN_FSTXP, SH64_MEDIA_INSN_FSTXS, SH64_MEDIA_INSN_FSUBD
 , SH64_MEDIA_INSN_FSUBS, SH64_MEDIA_INSN_FTRCDL, SH64_MEDIA_INSN_FTRCSL, SH64_MEDIA_INSN_FTRCDQ
 , SH64_MEDIA_INSN_FTRCSQ, SH64_MEDIA_INSN_FTRVS, SH64_MEDIA_INSN_GETCFG, SH64_MEDIA_INSN_GETCON
 , SH64_MEDIA_INSN_GETTR, SH64_MEDIA_INSN_ICBI, SH64_MEDIA_INSN_LDB, SH64_MEDIA_INSN_LDL
 , SH64_MEDIA_INSN_LDQ, SH64_MEDIA_INSN_LDUB, SH64_MEDIA_INSN_LDUW, SH64_MEDIA_INSN_LDW
 , SH64_MEDIA_INSN_LDHIL, SH64_MEDIA_INSN_LDHIQ, SH64_MEDIA_INSN_LDLOL, SH64_MEDIA_INSN_LDLOQ
 , SH64_MEDIA_INSN_LDXB, SH64_MEDIA_INSN_LDXL, SH64_MEDIA_INSN_LDXQ, SH64_MEDIA_INSN_LDXUB
 , SH64_MEDIA_INSN_LDXUW, SH64_MEDIA_INSN_LDXW, SH64_MEDIA_INSN_MABSL, SH64_MEDIA_INSN_MABSW
 , SH64_MEDIA_INSN_MADDL, SH64_MEDIA_INSN_MADDW, SH64_MEDIA_INSN_MADDSL, SH64_MEDIA_INSN_MADDSUB
 , SH64_MEDIA_INSN_MADDSW, SH64_MEDIA_INSN_MCMPEQB, SH64_MEDIA_INSN_MCMPEQL, SH64_MEDIA_INSN_MCMPEQW
 , SH64_MEDIA_INSN_MCMPGTL, SH64_MEDIA_INSN_MCMPGTUB, SH64_MEDIA_INSN_MCMPGTW, SH64_MEDIA_INSN_MCMV
 , SH64_MEDIA_INSN_MCNVSLW, SH64_MEDIA_INSN_MCNVSWB, SH64_MEDIA_INSN_MCNVSWUB, SH64_MEDIA_INSN_MEXTR1
 , SH64_MEDIA_INSN_MEXTR2, SH64_MEDIA_INSN_MEXTR3, SH64_MEDIA_INSN_MEXTR4, SH64_MEDIA_INSN_MEXTR5
 , SH64_MEDIA_INSN_MEXTR6, SH64_MEDIA_INSN_MEXTR7, SH64_MEDIA_INSN_MMACFXWL, SH64_MEDIA_INSN_MMACNFX_WL
 , SH64_MEDIA_INSN_MMULL, SH64_MEDIA_INSN_MMULW, SH64_MEDIA_INSN_MMULFXL, SH64_MEDIA_INSN_MMULFXW
 , SH64_MEDIA_INSN_MMULFXRPW, SH64_MEDIA_INSN_MMULHIWL, SH64_MEDIA_INSN_MMULLOWL, SH64_MEDIA_INSN_MMULSUMWQ
 , SH64_MEDIA_INSN_MOVI, SH64_MEDIA_INSN_MPERMW, SH64_MEDIA_INSN_MSADUBQ, SH64_MEDIA_INSN_MSHALDSL
 , SH64_MEDIA_INSN_MSHALDSW, SH64_MEDIA_INSN_MSHARDL, SH64_MEDIA_INSN_MSHARDW, SH64_MEDIA_INSN_MSHARDSQ
 , SH64_MEDIA_INSN_MSHFHIB, SH64_MEDIA_INSN_MSHFHIL, SH64_MEDIA_INSN_MSHFHIW, SH64_MEDIA_INSN_MSHFLOB
 , SH64_MEDIA_INSN_MSHFLOL, SH64_MEDIA_INSN_MSHFLOW, SH64_MEDIA_INSN_MSHLLDL, SH64_MEDIA_INSN_MSHLLDW
 , SH64_MEDIA_INSN_MSHLRDL, SH64_MEDIA_INSN_MSHLRDW, SH64_MEDIA_INSN_MSUBL, SH64_MEDIA_INSN_MSUBW
 , SH64_MEDIA_INSN_MSUBSL, SH64_MEDIA_INSN_MSUBSUB, SH64_MEDIA_INSN_MSUBSW, SH64_MEDIA_INSN_MULSL
 , SH64_MEDIA_INSN_MULUL, SH64_MEDIA_INSN_NOP, SH64_MEDIA_INSN_NSB, SH64_MEDIA_INSN_OCBI
 , SH64_MEDIA_INSN_OCBP, SH64_MEDIA_INSN_OCBWB, SH64_MEDIA_INSN_OR, SH64_MEDIA_INSN_ORI
 , SH64_MEDIA_INSN_PREFI, SH64_MEDIA_INSN_PTA, SH64_MEDIA_INSN_PTABS, SH64_MEDIA_INSN_PTB
 , SH64_MEDIA_INSN_PTREL, SH64_MEDIA_INSN_PUTCFG, SH64_MEDIA_INSN_PUTCON, SH64_MEDIA_INSN_RTE
 , SH64_MEDIA_INSN_SHARD, SH64_MEDIA_INSN_SHARDL, SH64_MEDIA_INSN_SHARI, SH64_MEDIA_INSN_SHARIL
 , SH64_MEDIA_INSN_SHLLD, SH64_MEDIA_INSN_SHLLDL, SH64_MEDIA_INSN_SHLLI, SH64_MEDIA_INSN_SHLLIL
 , SH64_MEDIA_INSN_SHLRD, SH64_MEDIA_INSN_SHLRDL, SH64_MEDIA_INSN_SHLRI, SH64_MEDIA_INSN_SHLRIL
 , SH64_MEDIA_INSN_SHORI, SH64_MEDIA_INSN_SLEEP, SH64_MEDIA_INSN_STB, SH64_MEDIA_INSN_STL
 , SH64_MEDIA_INSN_STQ, SH64_MEDIA_INSN_STW, SH64_MEDIA_INSN_STHIL, SH64_MEDIA_INSN_STHIQ
 , SH64_MEDIA_INSN_STLOL, SH64_MEDIA_INSN_STLOQ, SH64_MEDIA_INSN_STXB, SH64_MEDIA_INSN_STXL
 , SH64_MEDIA_INSN_STXQ, SH64_MEDIA_INSN_STXW, SH64_MEDIA_INSN_SUB, SH64_MEDIA_INSN_SUBL
 , SH64_MEDIA_INSN_SWAPQ, SH64_MEDIA_INSN_SYNCI, SH64_MEDIA_INSN_SYNCO, SH64_MEDIA_INSN_TRAPA
 , SH64_MEDIA_INSN_XOR, SH64_MEDIA_INSN_XORI, SH64_MEDIA_INSN__MAX
} SH64_MEDIA_INSN_TYPE;

/* Enum declaration for semantic formats in cpu family sh64.  */
typedef enum sh64_media_sfmt_type {
  SH64_MEDIA_SFMT_EMPTY, SH64_MEDIA_SFMT_ADD, SH64_MEDIA_SFMT_ADDI, SH64_MEDIA_SFMT_ALLOCO
 , SH64_MEDIA_SFMT_BEQ, SH64_MEDIA_SFMT_BEQI, SH64_MEDIA_SFMT_BLINK, SH64_MEDIA_SFMT_BRK
 , SH64_MEDIA_SFMT_BYTEREV, SH64_MEDIA_SFMT_CMVEQ, SH64_MEDIA_SFMT_FABSD, SH64_MEDIA_SFMT_FABSS
 , SH64_MEDIA_SFMT_FADDD, SH64_MEDIA_SFMT_FADDS, SH64_MEDIA_SFMT_FCMPEQD, SH64_MEDIA_SFMT_FCMPEQS
 , SH64_MEDIA_SFMT_FCNVDS, SH64_MEDIA_SFMT_FCNVSD, SH64_MEDIA_SFMT_FGETSCR, SH64_MEDIA_SFMT_FIPRS
 , SH64_MEDIA_SFMT_FLDD, SH64_MEDIA_SFMT_FLDP, SH64_MEDIA_SFMT_FLDS, SH64_MEDIA_SFMT_FLDXD
 , SH64_MEDIA_SFMT_FLDXP, SH64_MEDIA_SFMT_FLDXS, SH64_MEDIA_SFMT_FMACS, SH64_MEDIA_SFMT_FMOVDQ
 , SH64_MEDIA_SFMT_FMOVLS, SH64_MEDIA_SFMT_FMOVQD, SH64_MEDIA_SFMT_FMOVSL, SH64_MEDIA_SFMT_FPUTSCR
 , SH64_MEDIA_SFMT_FSTD, SH64_MEDIA_SFMT_FSTS, SH64_MEDIA_SFMT_FSTXD, SH64_MEDIA_SFMT_FSTXS
 , SH64_MEDIA_SFMT_FTRVS, SH64_MEDIA_SFMT_GETCFG, SH64_MEDIA_SFMT_GETCON, SH64_MEDIA_SFMT_GETTR
 , SH64_MEDIA_SFMT_LDB, SH64_MEDIA_SFMT_LDL, SH64_MEDIA_SFMT_LDQ, SH64_MEDIA_SFMT_LDUW
 , SH64_MEDIA_SFMT_LDHIL, SH64_MEDIA_SFMT_LDHIQ, SH64_MEDIA_SFMT_LDLOL, SH64_MEDIA_SFMT_LDLOQ
 , SH64_MEDIA_SFMT_LDXB, SH64_MEDIA_SFMT_LDXL, SH64_MEDIA_SFMT_LDXQ, SH64_MEDIA_SFMT_LDXUB
 , SH64_MEDIA_SFMT_LDXUW, SH64_MEDIA_SFMT_LDXW, SH64_MEDIA_SFMT_MCMV, SH64_MEDIA_SFMT_MOVI
 , SH64_MEDIA_SFMT_MPERMW, SH64_MEDIA_SFMT_NOP, SH64_MEDIA_SFMT_ORI, SH64_MEDIA_SFMT_PTA
 , SH64_MEDIA_SFMT_PTABS, SH64_MEDIA_SFMT_PTREL, SH64_MEDIA_SFMT_PUTCFG, SH64_MEDIA_SFMT_PUTCON
 , SH64_MEDIA_SFMT_SHARI, SH64_MEDIA_SFMT_SHARIL, SH64_MEDIA_SFMT_SHORI, SH64_MEDIA_SFMT_STB
 , SH64_MEDIA_SFMT_STL, SH64_MEDIA_SFMT_STQ, SH64_MEDIA_SFMT_STW, SH64_MEDIA_SFMT_STHIL
 , SH64_MEDIA_SFMT_STHIQ, SH64_MEDIA_SFMT_STLOL, SH64_MEDIA_SFMT_STLOQ, SH64_MEDIA_SFMT_STXB
 , SH64_MEDIA_SFMT_STXL, SH64_MEDIA_SFMT_STXQ, SH64_MEDIA_SFMT_STXW, SH64_MEDIA_SFMT_SWAPQ
 , SH64_MEDIA_SFMT_TRAPA, SH64_MEDIA_SFMT_XORI
} SH64_MEDIA_SFMT_TYPE;

/* Function unit handlers (user written).  */

extern int sh64_model_sh5_u_ftrv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*fvn*/);
extern int sh64_model_sh5_u_fipr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*fvm*/, INT /*fvn*/);
extern int sh64_model_sh5_u_ocb (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_mulr_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_mulr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_u_load_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_set_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_fcnv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_fcmp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_fsqrt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_fdiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_fpu_load_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_use_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_ldsl_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_lds_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_flds_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_load_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_set_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_fpu_memory_access (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_u_set_fr_0 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_set_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_load_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_maybe_fpu (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_fpu (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_write_back (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_multiply_result (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_shift (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_tas (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_mulsw (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_mull (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_dmul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_macl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_macw (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_multiply (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_set_mac (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_load_mac (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_load_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_load_gbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_u_load_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_u_stc_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_ldcl_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_ldcl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_tbit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_ldc_gbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_ldc_sr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_set_sr_bit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_use_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_load_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_sts_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_lds_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_memory_access (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_logic_b (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_jsr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_jmp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_sx (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_putcfg (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_getcfg (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_pt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*targetreg*/);
extern int sh64_model_sh5_media_u_ftrvs (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_fsqrtd (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_fdivd (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_cond_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*targetreg*/);
extern int sh64_model_sh5_media_u_blink (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*targetreg*/);
extern int sh64_model_sh5_media_u_use_tr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_use_mtrx (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_use_fv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_use_fp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_load_mtrx (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_load_fv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_load_fp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_mtrx (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_fv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_fp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_ftrv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*fvn*/);
extern int sh64_model_sh5_media_u_fipr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*fvm*/, INT /*fvn*/);
extern int sh64_model_sh5_media_u_ocb (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_mulr_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_mulr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_load_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_dr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_fcnv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_fcmp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_fsqrt (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_fdiv (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_fpu_load_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_use_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_ldsl_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_lds_fpscr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_flds_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_load_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_set_fpul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_fpu_memory_access (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_set_fr_0 (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_set_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_load_fr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_maybe_fpu (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_fpu (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_trap (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_write_back (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_multiply_result (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_shift (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_tas (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_mulsw (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_mull (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_dmul (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_macl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_macw (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_multiply (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_set_mac (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_load_mac (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_load_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_load_gbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*usereg*/);
extern int sh64_model_sh5_media_u_load_gr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/, INT /*loadreg*/);
extern int sh64_model_sh5_media_u_stc_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_ldcl_vbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_ldcl (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_tbit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_ldc_gbr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_ldc_sr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_set_sr_bit (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_use_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_load_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_sts_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_lds_pr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_memory_access (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_logic_b (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_jsr (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_jmp (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_branch (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_sx (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int sh64_model_sh5_media_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);

/* Profiling before/after handlers (user written) */

extern void sh64_model_insn_before (SIM_CPU *, int /*first_p*/);
extern void sh64_model_insn_after (SIM_CPU *, int /*last_p*/, int /*cycles*/);

#endif /* SH64_MEDIA_DECODE_H */
