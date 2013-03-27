/* CPU family header for sh64.

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

#ifndef CPU_SH64_H
#define CPU_SH64_H

/* Maximum number of instructions that are fetched at a time.
   This is for LIW type instructions sets (e.g. m32r).  */
#define MAX_LIW_INSNS 1

/* Maximum number of instructions that can be executed in parallel.  */
#define MAX_PARALLEL_INSNS 1

/* CPU state information.  */
typedef struct {
  /* Hardware elements.  */
  struct {
  /* Program counter */
  UDI h_pc;
#define GET_H_PC() CPU (h_pc)
#define SET_H_PC(x) \
do { \
{\
CPU (h_ism) = ANDDI ((x), 1);\
CPU (h_pc) = ANDDI ((x), INVDI (1));\
}\
;} while (0)
  /* General purpose integer registers */
  DI h_gr[64];
#define GET_H_GR(index) ((((index) == (63))) ? (0) : (CPU (h_gr[index])))
#define SET_H_GR(index, x) \
do { \
if ((((index)) != (63))) {\
CPU (h_gr[(index)]) = (x);\
} else {\
((void) 0); /*nop*/\
}\
;} while (0)
  /* Control registers */
  DI h_cr[64];
#define GET_H_CR(index) ((((index) == (0))) ? (ZEXTSIDI (CPU (h_sr))) : (CPU (h_cr[index])))
#define SET_H_CR(index, x) \
do { \
if ((((index)) == (0))) {\
CPU (h_sr) = (x);\
} else {\
CPU (h_cr[(index)]) = (x);\
}\
;} while (0)
  /* Status register */
  SI h_sr;
#define GET_H_SR() CPU (h_sr)
#define SET_H_SR(x) (CPU (h_sr) = (x))
  /* Floating point status and control register */
  SI h_fpscr;
#define GET_H_FPSCR() CPU (h_fpscr)
#define SET_H_FPSCR(x) (CPU (h_fpscr) = (x))
  /* Single precision floating point registers */
  SF h_fr[64];
#define GET_H_FR(a1) CPU (h_fr)[a1]
#define SET_H_FR(a1, x) (CPU (h_fr)[a1] = (x))
  /* Single/Double precision floating point registers */
  DF h_fsd[16];
#define GET_H_FSD(index) ((GET_H_PRBIT ()) ? (GET_H_DRC (index)) : (CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), CPU (h_fr[index]))))
#define SET_H_FSD(index, x) \
do { \
if (GET_H_PRBIT ()) {\
SET_H_DRC ((index), (x));\
} else {\
SET_H_FRC ((index), CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), (x)));\
}\
;} while (0)
  /* floating point registers for fmov */
  DF h_fmov[16];
#define GET_H_FMOV(index) ((NOTBI (GET_H_SZBIT ())) ? (CGEN_CPU_FPU (current_cpu)->ops->fextsfdf (CGEN_CPU_FPU (current_cpu), GET_H_FRC (index))) : (((((((index) & (1))) == (1))) ? (GET_H_XD (((index) & ((~ (1)))))) : (GET_H_DR (index)))))
#define SET_H_FMOV(index, x) \
do { \
if (NOTBI (GET_H_SZBIT ())) {\
SET_H_FRC ((index), CGEN_CPU_FPU (current_cpu)->ops->ftruncdfsf (CGEN_CPU_FPU (current_cpu), (x)));\
} else {\
if ((((((index)) & (1))) == (1))) {\
SET_H_XD ((((index)) & ((~ (1)))), (x));\
} else {\
SET_H_DR ((index), (x));\
}\
}\
;} while (0)
  /* Branch target registers */
  DI h_tr[8];
#define GET_H_TR(a1) CPU (h_tr)[a1]
#define SET_H_TR(a1, x) (CPU (h_tr)[a1] = (x))
  /* Current instruction set mode */
  BI h_ism;
#define GET_H_ISM() CPU (h_ism)
#define SET_H_ISM(x) \
do { \
cgen_rtx_error (current_cpu, "cannot set ism directly");\
;} while (0)
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} SH64_CPU_DATA;

/* Virtual regs.  */

#define GET_H_GRC(index) ANDDI (CPU (h_gr[index]), ZEXTSIDI (0xffffffff))
#define SET_H_GRC(index, x) \
do { \
CPU (h_gr[(index)]) = EXTSIDI ((x));\
;} while (0)
#define GET_H_FRBIT() ANDSI (SRLSI (CPU (h_fpscr), 21), 1)
#define SET_H_FRBIT(x) \
do { \
CPU (h_fpscr) = ORSI (ANDSI (CPU (h_fpscr), (~ (((1) << (21))))), SLLSI ((x), 21));\
;} while (0)
#define GET_H_SZBIT() ANDSI (SRLSI (CPU (h_fpscr), 20), 1)
#define SET_H_SZBIT(x) \
do { \
CPU (h_fpscr) = ORSI (ANDSI (CPU (h_fpscr), (~ (((1) << (20))))), SLLSI ((x), 20));\
;} while (0)
#define GET_H_PRBIT() ANDSI (SRLSI (CPU (h_fpscr), 19), 1)
#define SET_H_PRBIT(x) \
do { \
CPU (h_fpscr) = ORSI (ANDSI (CPU (h_fpscr), (~ (((1) << (19))))), SLLSI ((x), 19));\
;} while (0)
#define GET_H_SBIT() ANDSI (SRLSI (CPU (h_sr), 1), 1)
#define SET_H_SBIT(x) \
do { \
CPU (h_sr) = ORSI (ANDSI (CPU (h_sr), (~ (2))), SLLSI ((x), 1));\
;} while (0)
#define GET_H_MBIT() ANDSI (SRLSI (CPU (h_sr), 9), 1)
#define SET_H_MBIT(x) \
do { \
CPU (h_sr) = ORSI (ANDSI (CPU (h_sr), (~ (((1) << (9))))), SLLSI ((x), 9));\
;} while (0)
#define GET_H_QBIT() ANDSI (SRLSI (CPU (h_sr), 8), 1)
#define SET_H_QBIT(x) \
do { \
CPU (h_sr) = ORSI (ANDSI (CPU (h_sr), (~ (((1) << (8))))), SLLSI ((x), 8));\
;} while (0)
#define GET_H_FP(index) CPU (h_fr[index])
#define SET_H_FP(index, x) \
do { \
CPU (h_fr[(index)]) = (x);\
;} while (0)
#define GET_H_FV(index) CPU (h_fr[index])
#define SET_H_FV(index, x) \
do { \
CPU (h_fr[(index)]) = (x);\
;} while (0)
#define GET_H_FMTX(index) CPU (h_fr[index])
#define SET_H_FMTX(index, x) \
do { \
CPU (h_fr[(index)]) = (x);\
;} while (0)
#define GET_H_DR(index) SUBWORDDIDF (ORDI (SLLDI (ZEXTSIDI (SUBWORDSFSI (CPU (h_fr[index]))), 32), ZEXTSIDI (SUBWORDSFSI (CPU (h_fr[((index) + (1))])))))
#define SET_H_DR(index, x) \
do { \
{\
CPU (h_fr[(index)]) = SUBWORDSISF (SUBWORDDFSI ((x), 0));\
CPU (h_fr[(((index)) + (1))]) = SUBWORDSISF (SUBWORDDFSI ((x), 1));\
}\
;} while (0)
#define GET_H_ENDIAN() sh64_endian (current_cpu)
#define SET_H_ENDIAN(x) \
do { \
cgen_rtx_error (current_cpu, "cannot alter target byte order mid-program");\
;} while (0)
#define GET_H_FRC(index) CPU (h_fr[((((16) * (GET_H_FRBIT ()))) + (index))])
#define SET_H_FRC(index, x) \
do { \
CPU (h_fr[((((16) * (GET_H_FRBIT ()))) + ((index)))]) = (x);\
;} while (0)
#define GET_H_DRC(index) GET_H_DR (((((16) * (GET_H_FRBIT ()))) + (index)))
#define SET_H_DRC(index, x) \
do { \
SET_H_DR (((((16) * (GET_H_FRBIT ()))) + ((index))), (x));\
;} while (0)
#define GET_H_XF(index) CPU (h_fr[((((16) * (NOTBI (GET_H_FRBIT ())))) + (index))])
#define SET_H_XF(index, x) \
do { \
CPU (h_fr[((((16) * (NOTBI (GET_H_FRBIT ())))) + ((index)))]) = (x);\
;} while (0)
#define GET_H_XD(index) GET_H_DR (((((16) * (NOTBI (GET_H_FRBIT ())))) + (index)))
#define SET_H_XD(index, x) \
do { \
SET_H_DR (((((16) * (NOTBI (GET_H_FRBIT ())))) + ((index))), (x));\
;} while (0)
#define GET_H_FVC(index) CPU (h_fr[((((16) * (GET_H_FRBIT ()))) + (index))])
#define SET_H_FVC(index, x) \
do { \
CPU (h_fr[((((16) * (GET_H_FRBIT ()))) + ((index)))]) = (x);\
;} while (0)
#define GET_H_GBR() SUBWORDDISI (CPU (h_gr[((UINT) 16)]), 1)
#define SET_H_GBR(x) \
do { \
CPU (h_gr[((UINT) 16)]) = EXTSIDI ((x));\
;} while (0)
#define GET_H_VBR() SUBWORDDISI (CPU (h_gr[((UINT) 20)]), 1)
#define SET_H_VBR(x) \
do { \
CPU (h_gr[((UINT) 20)]) = EXTSIDI ((x));\
;} while (0)
#define GET_H_PR() SUBWORDDISI (CPU (h_gr[((UINT) 18)]), 1)
#define SET_H_PR(x) \
do { \
CPU (h_gr[((UINT) 18)]) = EXTSIDI ((x));\
;} while (0)
#define GET_H_MACL() SUBWORDDISI (CPU (h_gr[((UINT) 17)]), 1)
#define SET_H_MACL(x) \
do { \
CPU (h_gr[((UINT) 17)]) = ORDI (SLLDI (ZEXTSIDI (SUBWORDDISI (CPU (h_gr[((UINT) 17)]), 0)), 32), ZEXTSIDI ((x)));\
;} while (0)
#define GET_H_MACH() SUBWORDDISI (CPU (h_gr[((UINT) 17)]), 0)
#define SET_H_MACH(x) \
do { \
CPU (h_gr[((UINT) 17)]) = ORDI (SLLDI (ZEXTSIDI ((x)), 32), ZEXTSIDI (SUBWORDDISI (CPU (h_gr[((UINT) 17)]), 1)));\
;} while (0)
#define GET_H_TBIT() ANDBI (CPU (h_gr[((UINT) 19)]), 1)
#define SET_H_TBIT(x) \
do { \
CPU (h_gr[((UINT) 19)]) = ORDI (ANDDI (CPU (h_gr[((UINT) 19)]), INVDI (1)), ZEXTBIDI ((x)));\
;} while (0)

/* Cover fns for register access.  */
UDI sh64_h_pc_get (SIM_CPU *);
void sh64_h_pc_set (SIM_CPU *, UDI);
DI sh64_h_gr_get (SIM_CPU *, UINT);
void sh64_h_gr_set (SIM_CPU *, UINT, DI);
SI sh64_h_grc_get (SIM_CPU *, UINT);
void sh64_h_grc_set (SIM_CPU *, UINT, SI);
DI sh64_h_cr_get (SIM_CPU *, UINT);
void sh64_h_cr_set (SIM_CPU *, UINT, DI);
SI sh64_h_sr_get (SIM_CPU *);
void sh64_h_sr_set (SIM_CPU *, SI);
SI sh64_h_fpscr_get (SIM_CPU *);
void sh64_h_fpscr_set (SIM_CPU *, SI);
BI sh64_h_frbit_get (SIM_CPU *);
void sh64_h_frbit_set (SIM_CPU *, BI);
BI sh64_h_szbit_get (SIM_CPU *);
void sh64_h_szbit_set (SIM_CPU *, BI);
BI sh64_h_prbit_get (SIM_CPU *);
void sh64_h_prbit_set (SIM_CPU *, BI);
BI sh64_h_sbit_get (SIM_CPU *);
void sh64_h_sbit_set (SIM_CPU *, BI);
BI sh64_h_mbit_get (SIM_CPU *);
void sh64_h_mbit_set (SIM_CPU *, BI);
BI sh64_h_qbit_get (SIM_CPU *);
void sh64_h_qbit_set (SIM_CPU *, BI);
SF sh64_h_fr_get (SIM_CPU *, UINT);
void sh64_h_fr_set (SIM_CPU *, UINT, SF);
SF sh64_h_fp_get (SIM_CPU *, UINT);
void sh64_h_fp_set (SIM_CPU *, UINT, SF);
SF sh64_h_fv_get (SIM_CPU *, UINT);
void sh64_h_fv_set (SIM_CPU *, UINT, SF);
SF sh64_h_fmtx_get (SIM_CPU *, UINT);
void sh64_h_fmtx_set (SIM_CPU *, UINT, SF);
DF sh64_h_dr_get (SIM_CPU *, UINT);
void sh64_h_dr_set (SIM_CPU *, UINT, DF);
DF sh64_h_fsd_get (SIM_CPU *, UINT);
void sh64_h_fsd_set (SIM_CPU *, UINT, DF);
DF sh64_h_fmov_get (SIM_CPU *, UINT);
void sh64_h_fmov_set (SIM_CPU *, UINT, DF);
DI sh64_h_tr_get (SIM_CPU *, UINT);
void sh64_h_tr_set (SIM_CPU *, UINT, DI);
BI sh64_h_endian_get (SIM_CPU *);
void sh64_h_endian_set (SIM_CPU *, BI);
BI sh64_h_ism_get (SIM_CPU *);
void sh64_h_ism_set (SIM_CPU *, BI);
SF sh64_h_frc_get (SIM_CPU *, UINT);
void sh64_h_frc_set (SIM_CPU *, UINT, SF);
DF sh64_h_drc_get (SIM_CPU *, UINT);
void sh64_h_drc_set (SIM_CPU *, UINT, DF);
SF sh64_h_xf_get (SIM_CPU *, UINT);
void sh64_h_xf_set (SIM_CPU *, UINT, SF);
DF sh64_h_xd_get (SIM_CPU *, UINT);
void sh64_h_xd_set (SIM_CPU *, UINT, DF);
SF sh64_h_fvc_get (SIM_CPU *, UINT);
void sh64_h_fvc_set (SIM_CPU *, UINT, SF);
SI sh64_h_gbr_get (SIM_CPU *);
void sh64_h_gbr_set (SIM_CPU *, SI);
SI sh64_h_vbr_get (SIM_CPU *);
void sh64_h_vbr_set (SIM_CPU *, SI);
SI sh64_h_pr_get (SIM_CPU *);
void sh64_h_pr_set (SIM_CPU *, SI);
SI sh64_h_macl_get (SIM_CPU *);
void sh64_h_macl_set (SIM_CPU *, SI);
SI sh64_h_mach_get (SIM_CPU *);
void sh64_h_mach_set (SIM_CPU *, SI);
BI sh64_h_tbit_get (SIM_CPU *);
void sh64_h_tbit_set (SIM_CPU *, BI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN sh64_fetch_register;
extern CPUREG_STORE_FN sh64_store_register;

typedef struct {
  int empty;
} MODEL_SH4_DATA;

typedef struct {
  int empty;
} MODEL_SH5_DATA;

typedef struct {
  int empty;
} MODEL_SH5_MEDIA_DATA;

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_SH64_H */
