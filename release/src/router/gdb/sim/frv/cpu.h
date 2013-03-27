/* CPU family header for frvbf.

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

#ifndef CPU_FRVBF_H
#define CPU_FRVBF_H

/* Maximum number of instructions that are fetched at a time.
   This is for LIW type instructions sets (e.g. m32r).  */
#define MAX_LIW_INSNS 1

/* Maximum number of instructions that can be executed in parallel.  */
#define MAX_PARALLEL_INSNS 8

/* CPU state information.  */
typedef struct {
  /* Hardware elements.  */
  struct {
  /* relocation annotation */
  BI h_reloc_ann;
#define GET_H_RELOC_ANN() CPU (h_reloc_ann)
#define SET_H_RELOC_ANN(x) (CPU (h_reloc_ann) = (x))
  /* program counter */
  USI h_pc;
#define GET_H_PC() CPU (h_pc)
#define SET_H_PC(x) (CPU (h_pc) = (x))
  /* PSR.IMPLE */
  UQI h_psr_imple;
#define GET_H_PSR_IMPLE() CPU (h_psr_imple)
#define SET_H_PSR_IMPLE(x) (CPU (h_psr_imple) = (x))
  /* PSR.VER */
  UQI h_psr_ver;
#define GET_H_PSR_VER() CPU (h_psr_ver)
#define SET_H_PSR_VER(x) (CPU (h_psr_ver) = (x))
  /* PSR.ICE bit */
  BI h_psr_ice;
#define GET_H_PSR_ICE() CPU (h_psr_ice)
#define SET_H_PSR_ICE(x) (CPU (h_psr_ice) = (x))
  /* PSR.NEM bit */
  BI h_psr_nem;
#define GET_H_PSR_NEM() CPU (h_psr_nem)
#define SET_H_PSR_NEM(x) (CPU (h_psr_nem) = (x))
  /* PSR.CM  bit */
  BI h_psr_cm;
#define GET_H_PSR_CM() CPU (h_psr_cm)
#define SET_H_PSR_CM(x) (CPU (h_psr_cm) = (x))
  /* PSR.BE  bit */
  BI h_psr_be;
#define GET_H_PSR_BE() CPU (h_psr_be)
#define SET_H_PSR_BE(x) (CPU (h_psr_be) = (x))
  /* PSR.ESR bit */
  BI h_psr_esr;
#define GET_H_PSR_ESR() CPU (h_psr_esr)
#define SET_H_PSR_ESR(x) (CPU (h_psr_esr) = (x))
  /* PSR.EF  bit */
  BI h_psr_ef;
#define GET_H_PSR_EF() CPU (h_psr_ef)
#define SET_H_PSR_EF(x) (CPU (h_psr_ef) = (x))
  /* PSR.EM  bit */
  BI h_psr_em;
#define GET_H_PSR_EM() CPU (h_psr_em)
#define SET_H_PSR_EM(x) (CPU (h_psr_em) = (x))
  /* PSR.PIL     */
  UQI h_psr_pil;
#define GET_H_PSR_PIL() CPU (h_psr_pil)
#define SET_H_PSR_PIL(x) (CPU (h_psr_pil) = (x))
  /* PSR.PS  bit */
  BI h_psr_ps;
#define GET_H_PSR_PS() CPU (h_psr_ps)
#define SET_H_PSR_PS(x) (CPU (h_psr_ps) = (x))
  /* PSR.ET  bit */
  BI h_psr_et;
#define GET_H_PSR_ET() CPU (h_psr_et)
#define SET_H_PSR_ET(x) (CPU (h_psr_et) = (x))
  /* PSR.S bit */
  BI h_psr_s;
#define GET_H_PSR_S() CPU (h_psr_s)
#define SET_H_PSR_S(x) \
do { \
frvbf_h_psr_s_set_handler (current_cpu, (x));\
;} while (0)
  /* TBR.TBA */
  USI h_tbr_tba;
#define GET_H_TBR_TBA() CPU (h_tbr_tba)
#define SET_H_TBR_TBA(x) (CPU (h_tbr_tba) = (x))
  /* TBR.TT */
  UQI h_tbr_tt;
#define GET_H_TBR_TT() CPU (h_tbr_tt)
#define SET_H_TBR_TT(x) (CPU (h_tbr_tt) = (x))
  /* PSR.S   bit */
  BI h_bpsr_bs;
#define GET_H_BPSR_BS() CPU (h_bpsr_bs)
#define SET_H_BPSR_BS(x) (CPU (h_bpsr_bs) = (x))
  /* PSR.ET  bit */
  BI h_bpsr_bet;
#define GET_H_BPSR_BET() CPU (h_bpsr_bet)
#define SET_H_BPSR_BET(x) (CPU (h_bpsr_bet) = (x))
  /* general registers */
  USI h_gr[64];
#define GET_H_GR(index) frvbf_h_gr_get_handler (current_cpu, index)
#define SET_H_GR(index, x) \
do { \
frvbf_h_gr_set_handler (current_cpu, (index), (x));\
;} while (0)
  /* floating point registers */
  SF h_fr[64];
#define GET_H_FR(index) frvbf_h_fr_get_handler (current_cpu, index)
#define SET_H_FR(index, x) \
do { \
frvbf_h_fr_set_handler (current_cpu, (index), (x));\
;} while (0)
  /* coprocessor registers */
  SI h_cpr[64];
#define GET_H_CPR(a1) CPU (h_cpr)[a1]
#define SET_H_CPR(a1, x) (CPU (h_cpr)[a1] = (x))
  /* special purpose registers */
  USI h_spr[4096];
#define GET_H_SPR(index) frvbf_h_spr_get_handler (current_cpu, index)
#define SET_H_SPR(index, x) \
do { \
frvbf_h_spr_set_handler (current_cpu, (index), (x));\
;} while (0)
  /* Integer condition code registers */
  UQI h_iccr[4];
#define GET_H_ICCR(a1) CPU (h_iccr)[a1]
#define SET_H_ICCR(a1, x) (CPU (h_iccr)[a1] = (x))
  /* Floating point condition code registers */
  UQI h_fccr[4];
#define GET_H_FCCR(a1) CPU (h_fccr)[a1]
#define SET_H_FCCR(a1, x) (CPU (h_fccr)[a1] = (x))
  /* Condition code registers */
  UQI h_cccr[8];
#define GET_H_CCCR(a1) CPU (h_cccr)[a1]
#define SET_H_CCCR(a1, x) (CPU (h_cccr)[a1] = (x))
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} FRVBF_CPU_DATA;

/* Virtual regs.  */

#define GET_H_GR_DOUBLE(index) frvbf_h_gr_double_get_handler (current_cpu, index)
#define SET_H_GR_DOUBLE(index, x) \
do { \
frvbf_h_gr_double_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_GR_HI(index) frvbf_h_gr_hi_get_handler (current_cpu, index)
#define SET_H_GR_HI(index, x) \
do { \
frvbf_h_gr_hi_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_GR_LO(index) frvbf_h_gr_lo_get_handler (current_cpu, index)
#define SET_H_GR_LO(index, x) \
do { \
frvbf_h_gr_lo_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_FR_DOUBLE(index) frvbf_h_fr_double_get_handler (current_cpu, index)
#define SET_H_FR_DOUBLE(index, x) \
do { \
frvbf_h_fr_double_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_FR_INT(index) frvbf_h_fr_int_get_handler (current_cpu, index)
#define SET_H_FR_INT(index, x) \
do { \
frvbf_h_fr_int_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_FR_HI(index) SRLSI (GET_H_FR_INT (index), 16)
#define SET_H_FR_HI(index, x) \
do { \
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 65535), SLLHI ((x), 16)));\
;} while (0)
#define GET_H_FR_LO(index) ANDSI (GET_H_FR_INT (index), 65535)
#define SET_H_FR_LO(index, x) \
do { \
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 0xffff0000), ANDHI ((x), 65535)));\
;} while (0)
#define GET_H_FR_0(index) ANDSI (GET_H_FR_INT (index), 255)
#define SET_H_FR_0(index, x) \
do { \
{\
if (GTSI ((x), 255)) {\
  (x) = 255;\
}\
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 0xffffff00), (x)));\
}\
;} while (0)
#define GET_H_FR_1(index) ANDSI (SRLSI (GET_H_FR_INT (index), 8), 255)
#define SET_H_FR_1(index, x) \
do { \
{\
if (GTSI ((x), 255)) {\
  (x) = 255;\
}\
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 0xffff00ff), SLLHI ((x), 8)));\
}\
;} while (0)
#define GET_H_FR_2(index) ANDSI (SRLSI (GET_H_FR_INT (index), 16), 255)
#define SET_H_FR_2(index, x) \
do { \
{\
if (GTSI ((x), 255)) {\
  (x) = 255;\
}\
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 0xff00ffff), SLLHI ((x), 16)));\
}\
;} while (0)
#define GET_H_FR_3(index) ANDSI (SRLSI (GET_H_FR_INT (index), 24), 255)
#define SET_H_FR_3(index, x) \
do { \
{\
if (GTSI ((x), 255)) {\
  (x) = 255;\
}\
SET_H_FR_INT ((index), ORSI (ANDSI (GET_H_FR_INT ((index)), 16777215), SLLHI ((x), 24)));\
}\
;} while (0)
#define GET_H_CPR_DOUBLE(index) frvbf_h_cpr_double_get_handler (current_cpu, index)
#define SET_H_CPR_DOUBLE(index, x) \
do { \
frvbf_h_cpr_double_set_handler (current_cpu, (index), (x));\
;} while (0)
#define GET_H_ACCG(index) ANDSI (GET_H_SPR (((index) + (1472))), 255)
#define SET_H_ACCG(index, x) \
do { \
CPU (h_spr[(((index)) + (1472))]) = ANDSI ((x), 255);\
;} while (0)
#define GET_H_ACC40S(index) ORDI (SLLDI (EXTQIDI (TRUNCSIQI (GET_H_SPR (((index) + (1472))))), 32), ZEXTSIDI (GET_H_SPR (((index) + (1408)))))
#define SET_H_ACC40S(index, x) \
do { \
{\
frv_check_spr_write_access (current_cpu, (((index)) + (1408)));\
CPU (h_spr[(((index)) + (1472))]) = ANDDI (SRLDI ((x), 32), 255);\
CPU (h_spr[(((index)) + (1408))]) = TRUNCDISI ((x));\
}\
;} while (0)
#define GET_H_ACC40U(index) ORDI (SLLDI (ZEXTSIDI (GET_H_SPR (((index) + (1472)))), 32), ZEXTSIDI (GET_H_SPR (((index) + (1408)))))
#define SET_H_ACC40U(index, x) \
do { \
{\
frv_check_spr_write_access (current_cpu, (((index)) + (1408)));\
CPU (h_spr[(((index)) + (1472))]) = ANDDI (SRLDI ((x), 32), 255);\
CPU (h_spr[(((index)) + (1408))]) = TRUNCDISI ((x));\
}\
;} while (0)
#define GET_H_IACC0(index) ORDI (SLLDI (EXTSIDI (GET_H_SPR (((UINT) 280))), 32), ZEXTSIDI (GET_H_SPR (((UINT) 281))))
#define SET_H_IACC0(index, x) \
do { \
{\
SET_H_SPR (((UINT) 280), TRUNCDISI (SRLDI ((x), 32)));\
SET_H_SPR (((UINT) 281), TRUNCDISI ((x)));\
}\
;} while (0)

/* Cover fns for register access.  */
BI frvbf_h_reloc_ann_get (SIM_CPU *);
void frvbf_h_reloc_ann_set (SIM_CPU *, BI);
USI frvbf_h_pc_get (SIM_CPU *);
void frvbf_h_pc_set (SIM_CPU *, USI);
UQI frvbf_h_psr_imple_get (SIM_CPU *);
void frvbf_h_psr_imple_set (SIM_CPU *, UQI);
UQI frvbf_h_psr_ver_get (SIM_CPU *);
void frvbf_h_psr_ver_set (SIM_CPU *, UQI);
BI frvbf_h_psr_ice_get (SIM_CPU *);
void frvbf_h_psr_ice_set (SIM_CPU *, BI);
BI frvbf_h_psr_nem_get (SIM_CPU *);
void frvbf_h_psr_nem_set (SIM_CPU *, BI);
BI frvbf_h_psr_cm_get (SIM_CPU *);
void frvbf_h_psr_cm_set (SIM_CPU *, BI);
BI frvbf_h_psr_be_get (SIM_CPU *);
void frvbf_h_psr_be_set (SIM_CPU *, BI);
BI frvbf_h_psr_esr_get (SIM_CPU *);
void frvbf_h_psr_esr_set (SIM_CPU *, BI);
BI frvbf_h_psr_ef_get (SIM_CPU *);
void frvbf_h_psr_ef_set (SIM_CPU *, BI);
BI frvbf_h_psr_em_get (SIM_CPU *);
void frvbf_h_psr_em_set (SIM_CPU *, BI);
UQI frvbf_h_psr_pil_get (SIM_CPU *);
void frvbf_h_psr_pil_set (SIM_CPU *, UQI);
BI frvbf_h_psr_ps_get (SIM_CPU *);
void frvbf_h_psr_ps_set (SIM_CPU *, BI);
BI frvbf_h_psr_et_get (SIM_CPU *);
void frvbf_h_psr_et_set (SIM_CPU *, BI);
BI frvbf_h_psr_s_get (SIM_CPU *);
void frvbf_h_psr_s_set (SIM_CPU *, BI);
USI frvbf_h_tbr_tba_get (SIM_CPU *);
void frvbf_h_tbr_tba_set (SIM_CPU *, USI);
UQI frvbf_h_tbr_tt_get (SIM_CPU *);
void frvbf_h_tbr_tt_set (SIM_CPU *, UQI);
BI frvbf_h_bpsr_bs_get (SIM_CPU *);
void frvbf_h_bpsr_bs_set (SIM_CPU *, BI);
BI frvbf_h_bpsr_bet_get (SIM_CPU *);
void frvbf_h_bpsr_bet_set (SIM_CPU *, BI);
USI frvbf_h_gr_get (SIM_CPU *, UINT);
void frvbf_h_gr_set (SIM_CPU *, UINT, USI);
DI frvbf_h_gr_double_get (SIM_CPU *, UINT);
void frvbf_h_gr_double_set (SIM_CPU *, UINT, DI);
UHI frvbf_h_gr_hi_get (SIM_CPU *, UINT);
void frvbf_h_gr_hi_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_gr_lo_get (SIM_CPU *, UINT);
void frvbf_h_gr_lo_set (SIM_CPU *, UINT, UHI);
SF frvbf_h_fr_get (SIM_CPU *, UINT);
void frvbf_h_fr_set (SIM_CPU *, UINT, SF);
DF frvbf_h_fr_double_get (SIM_CPU *, UINT);
void frvbf_h_fr_double_set (SIM_CPU *, UINT, DF);
USI frvbf_h_fr_int_get (SIM_CPU *, UINT);
void frvbf_h_fr_int_set (SIM_CPU *, UINT, USI);
UHI frvbf_h_fr_hi_get (SIM_CPU *, UINT);
void frvbf_h_fr_hi_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_fr_lo_get (SIM_CPU *, UINT);
void frvbf_h_fr_lo_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_fr_0_get (SIM_CPU *, UINT);
void frvbf_h_fr_0_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_fr_1_get (SIM_CPU *, UINT);
void frvbf_h_fr_1_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_fr_2_get (SIM_CPU *, UINT);
void frvbf_h_fr_2_set (SIM_CPU *, UINT, UHI);
UHI frvbf_h_fr_3_get (SIM_CPU *, UINT);
void frvbf_h_fr_3_set (SIM_CPU *, UINT, UHI);
SI frvbf_h_cpr_get (SIM_CPU *, UINT);
void frvbf_h_cpr_set (SIM_CPU *, UINT, SI);
DI frvbf_h_cpr_double_get (SIM_CPU *, UINT);
void frvbf_h_cpr_double_set (SIM_CPU *, UINT, DI);
USI frvbf_h_spr_get (SIM_CPU *, UINT);
void frvbf_h_spr_set (SIM_CPU *, UINT, USI);
USI frvbf_h_accg_get (SIM_CPU *, UINT);
void frvbf_h_accg_set (SIM_CPU *, UINT, USI);
DI frvbf_h_acc40S_get (SIM_CPU *, UINT);
void frvbf_h_acc40S_set (SIM_CPU *, UINT, DI);
UDI frvbf_h_acc40U_get (SIM_CPU *, UINT);
void frvbf_h_acc40U_set (SIM_CPU *, UINT, UDI);
DI frvbf_h_iacc0_get (SIM_CPU *, UINT);
void frvbf_h_iacc0_set (SIM_CPU *, UINT, DI);
UQI frvbf_h_iccr_get (SIM_CPU *, UINT);
void frvbf_h_iccr_set (SIM_CPU *, UINT, UQI);
UQI frvbf_h_fccr_get (SIM_CPU *, UINT);
void frvbf_h_fccr_set (SIM_CPU *, UINT, UQI);
UQI frvbf_h_cccr_get (SIM_CPU *, UINT);
void frvbf_h_cccr_set (SIM_CPU *, UINT, UQI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN frvbf_fetch_register;
extern CPUREG_STORE_FN frvbf_store_register;

typedef struct {
  int empty;
} MODEL_FRV_DATA;

typedef struct {
  DI prev_fr_load;
  DI prev_fr_complex_1;
  DI prev_fr_complex_2;
  DI prev_ccr_complex;
  DI prev_acc_mmac;
  DI cur_fr_load;
  DI cur_fr_complex_1;
  DI cur_fr_complex_2;
  SI cur_ccr_complex;
  DI cur_acc_mmac;
} MODEL_FR550_DATA;

typedef struct {
  DI prev_fpop;
  DI prev_media;
  DI prev_cc_complex;
  DI cur_fpop;
  DI cur_media;
  DI cur_cc_complex;
} MODEL_FR500_DATA;

typedef struct {
  int empty;
} MODEL_TOMCAT_DATA;

typedef struct {
  DI prev_fp_load;
  DI prev_fr_p4;
  DI prev_fr_p6;
  DI prev_acc_p2;
  DI prev_acc_p4;
  DI cur_fp_load;
  DI cur_fr_p4;
  DI cur_fr_p6;
  DI cur_acc_p2;
  DI cur_acc_p4;
} MODEL_FR400_DATA;

typedef struct {
  DI prev_fp_load;
  DI prev_fr_p4;
  DI prev_fr_p6;
  DI prev_acc_p2;
  DI prev_acc_p4;
  DI cur_fp_load;
  DI cur_fr_p4;
  DI cur_fr_p6;
  DI cur_acc_p2;
  DI cur_acc_p4;
} MODEL_FR450_DATA;

typedef struct {
  int empty;
} MODEL_SIMPLE_DATA;

/* Instruction argument buffer.  */

union sem_fields {
  struct { /* no operands */
    int empty;
  } fmt_empty;
  struct { /*  */
    unsigned short out_h_spr_USI_2;
  } sfmt_break;
  struct { /*  */
    UINT f_debug;
  } sfmt_rett;
  struct { /*  */
    IADDR i_label24;
  } sfmt_call;
  struct { /*  */
    INT f_u12;
    UINT f_FRk;
    unsigned char out_FRkhi;
  } sfmt_mhsethis;
  struct { /*  */
    INT f_u12;
    UINT f_FRk;
    unsigned char out_FRklo;
  } sfmt_mhsetlos;
  struct { /*  */
    INT f_s16;
    UINT f_GRk;
    unsigned char out_GRk;
  } sfmt_setlos;
  struct { /*  */
    UINT f_GRk;
    UINT f_u16;
    unsigned char out_GRkhi;
  } sfmt_sethi;
  struct { /*  */
    UINT f_GRk;
    UINT f_u16;
    unsigned char out_GRklo;
  } sfmt_setlo;
  struct { /*  */
    UINT f_ACCGi;
    UINT f_FRk;
    unsigned char in_ACCGi;
    unsigned char out_FRintk;
  } sfmt_mrdaccg;
  struct { /*  */
    INT f_s5;
    UINT f_FRk;
    unsigned char in_FRkhi;
    unsigned char out_FRkhi;
  } sfmt_mhsethih;
  struct { /*  */
    INT f_s5;
    UINT f_FRk;
    unsigned char in_FRklo;
    unsigned char out_FRklo;
  } sfmt_mhsetloh;
  struct { /*  */
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRdoublej;
    unsigned char out_FRintk;
  } sfmt_fdtoi;
  struct { /*  */
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRintj;
    unsigned char out_FRdoublek;
  } sfmt_fitod;
  struct { /*  */
    INT f_d12;
    UINT f_GRi;
    UINT f_LI;
    unsigned char in_GRi;
  } sfmt_jmpil;
  struct { /*  */
    IADDR i_label16;
    UINT f_FCCi_2;
    UINT f_hint;
    unsigned char in_FCCi_2;
  } sfmt_fbne;
  struct { /*  */
    IADDR i_label16;
    UINT f_ICCi_2;
    UINT f_hint;
    unsigned char in_ICCi_2;
  } sfmt_beq;
  struct { /*  */
    UINT f_GRj;
    UINT f_spr;
    unsigned short in_spr;
    unsigned char out_GRj;
  } sfmt_movsg;
  struct { /*  */
    UINT f_GRj;
    UINT f_spr;
    unsigned short out_spr;
    unsigned char in_GRj;
  } sfmt_movgs;
  struct { /*  */
    UINT f_ACCGk;
    UINT f_FRi;
    unsigned char in_ACCGk;
    unsigned char in_FRinti;
    unsigned char out_ACCGk;
  } sfmt_mwtaccg;
  struct { /*  */
    INT f_s6;
    UINT f_ACC40Si;
    UINT f_FRk;
    unsigned char in_ACC40Si;
    unsigned char out_FRintk;
  } sfmt_mcuti;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    UINT f_lock;
    unsigned char in_GRi;
    unsigned char in_GRj;
  } sfmt_icpl;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    UINT f_ae;
    unsigned char in_GRi;
    unsigned char in_GRj;
  } sfmt_icei;
  struct { /*  */
    INT f_d12;
    UINT f_FRk;
    UINT f_GRi;
    unsigned char in_FRdoublek;
    unsigned char in_GRi;
  } sfmt_stdfi;
  struct { /*  */
    INT f_d12;
    UINT f_GRi;
    UINT f_GRk;
    unsigned char in_GRdoublek;
    unsigned char in_GRi;
  } sfmt_stdi;
  struct { /*  */
    INT f_d12;
    UINT f_FRk;
    UINT f_GRi;
    unsigned char in_FRintk;
    unsigned char in_GRi;
  } sfmt_stbfi;
  struct { /*  */
    INT f_d12;
    UINT f_FRk;
    UINT f_GRi;
    unsigned char in_GRi;
    unsigned char out_FRdoublek;
  } sfmt_lddfi;
  struct { /*  */
    INT f_d12;
    UINT f_FRk;
    UINT f_GRi;
    unsigned char in_GRi;
    unsigned char out_FRintk;
  } sfmt_ldbfi;
  struct { /*  */
    INT f_d12;
    UINT f_GRi;
    UINT f_GRk;
    unsigned char in_GRi;
    unsigned char out_GRdoublek;
  } sfmt_smuli;
  struct { /*  */
    UINT f_GRj;
    UINT f_GRk;
    unsigned char in_GRj;
    unsigned char in_h_iacc0_DI_0;
    unsigned char out_GRk;
  } sfmt_scutss;
  struct { /*  */
    UINT f_ACC40Si;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_ACC40Si;
    unsigned char in_FRintj;
    unsigned char out_FRintk;
  } sfmt_mcut;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    UINT f_u6;
    unsigned char in_FRinti;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRinti_1;
    unsigned char out_FRintk;
  } sfmt_mwcuti;
  struct { /*  */
    INT f_u12;
    UINT f_FRk;
    unsigned char in_FRintk;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_mhdsets;
  struct { /*  */
    UINT f_FCCi_2;
    UINT f_FRi;
    UINT f_FRj;
    unsigned char in_FRdoublei;
    unsigned char in_FRdoublej;
    unsigned char out_FCCi_2;
  } sfmt_fcmpd;
  struct { /*  */
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRj;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_1;
    unsigned char out_FRintk;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_1;
  } sfmt_fdstoi;
  struct { /*  */
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRintj;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintj_1;
    unsigned char out_FRk;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_1;
  } sfmt_fditos;
  struct { /*  */
    UINT f_CRi;
    UINT f_CRj;
    UINT f_CRk;
    unsigned char in_CRi;
    unsigned char in_CRj;
    unsigned char out_CRk;
  } sfmt_andcr;
  struct { /*  */
    INT f_d12;
    UINT f_GRi;
    UINT f_GRk;
    unsigned char in_GRi;
    unsigned char in_GRk;
    unsigned char out_GRk;
  } sfmt_swapi;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_h_iacc0_DI_0;
    unsigned char out_h_iacc0_DI_0;
  } sfmt_smass;
  struct { /*  */
    INT f_s6;
    UINT f_FRi;
    UINT f_FRk;
    unsigned char in_FRintieven;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_mdrotli;
  struct { /*  */
    INT f_s6;
    UINT f_ACC40Si;
    UINT f_FRk;
    unsigned char in_ACC40Si;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Si_1;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_mdcutssi;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRinti_1;
    unsigned char out_FRintk;
  } sfmt_mwcut;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRdoublei;
    unsigned char in_FRdoublej;
    unsigned char in_FRdoublek;
    unsigned char out_FRdoublek;
  } sfmt_fmaddd;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRj;
    unsigned char out_FRintk;
  } sfmt_cfstoi;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintj;
    unsigned char out_FRk;
  } sfmt_cfitos;
  struct { /*  */
    UINT f_CCi;
    UINT f_CRj_float;
    UINT f_FCCi_3;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FCCi_3;
    unsigned char out_CRj_float;
  } sfmt_cfckne;
  struct { /*  */
    SI f_CRj_int;
    UINT f_CCi;
    UINT f_ICCi_3;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_ICCi_3;
    unsigned char out_CRj_int;
  } sfmt_cckeq;
  struct { /*  */
    UINT f_FCCi_2;
    UINT f_ccond;
    UINT f_hint;
    unsigned short in_h_spr_USI_272;
    unsigned short in_h_spr_USI_273;
    unsigned short out_h_spr_USI_273;
    unsigned char in_FCCi_2;
  } sfmt_fcbeqlr;
  struct { /*  */
    UINT f_ICCi_2;
    UINT f_ccond;
    UINT f_hint;
    unsigned short in_h_spr_USI_272;
    unsigned short in_h_spr_USI_273;
    unsigned short out_h_spr_USI_273;
    unsigned char in_ICCi_2;
  } sfmt_bceqlr;
  struct { /*  */
    UINT f_CPRk;
    UINT f_GRi;
    UINT f_GRj;
    unsigned char in_CPRdoublek;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
  } sfmt_stdcu;
  struct { /*  */
    UINT f_CPRk;
    UINT f_GRi;
    UINT f_GRj;
    unsigned char in_CPRk;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
  } sfmt_stcu;
  struct { /*  */
    UINT f_CPRk;
    UINT f_GRi;
    UINT f_GRj;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_CPRdoublek;
    unsigned char out_GRi;
  } sfmt_lddcu;
  struct { /*  */
    UINT f_CPRk;
    UINT f_GRi;
    UINT f_GRj;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_CPRk;
    unsigned char out_GRi;
  } sfmt_ldcu;
  struct { /*  */
    INT f_s5;
    UINT f_FRk;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_mhdseth;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_LI;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
  } sfmt_cjmpl;
  struct { /*  */
    INT f_s10;
    UINT f_GRi;
    UINT f_GRk;
    UINT f_ICCi_1;
    unsigned char in_GRi;
    unsigned char in_ICCi_1;
    unsigned char out_GRdoublek;
    unsigned char out_ICCi_1;
  } sfmt_smulicc;
  struct { /*  */
    INT f_s10;
    UINT f_GRi;
    UINT f_GRk;
    UINT f_ICCi_1;
    unsigned char in_GRi;
    unsigned char in_ICCi_1;
    unsigned char out_GRk;
    unsigned char out_ICCi_1;
  } sfmt_addicc;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char out_FRintk;
  } sfmt_cmand;
  struct { /*  */
    UINT f_CCi;
    UINT f_FCCi_2;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char out_FCCi_2;
  } sfmt_cfcmps;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintk;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_1;
    unsigned char out_GRj;
    unsigned char out_h_gr_USI_add__DFLT_index_of__DFLT_GRj_1;
  } sfmt_cmovfgd;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRj;
    unsigned char in_h_gr_USI_add__DFLT_index_of__DFLT_GRj_1;
    unsigned char out_FRintk;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_1;
  } sfmt_cmovgfd;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_ICCi_1;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_ICCi_1;
    unsigned char out_GRdoublek;
    unsigned char out_ICCi_1;
  } sfmt_smulcc;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_ICCi_1;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_ICCi_1;
    unsigned char out_GRk;
    unsigned char out_ICCi_1;
  } sfmt_addcc;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRk;
    UINT f_cond;
    UINT f_u6;
    unsigned char in_CCi;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_cmexpdhw;
  struct { /*  */
    UINT f_ACC40Si;
    UINT f_ACC40Sk;
    unsigned char in_ACC40Si;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Si_1;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Si_2;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Si_3;
    unsigned char out_ACC40Sk;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_1;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_2;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_3;
  } sfmt_mdasaccs;
  struct { /*  */
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRintj;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_FRintj;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_mabshs;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    UINT f_u6;
    unsigned char in_FRinti;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_1;
    unsigned char out_FRinti;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_mcplhi;
  struct { /*  */
    UINT f_FCCi_2;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_1;
    unsigned char out_FCCi_2;
    unsigned char out_h_fccr_UQI_add__DFLT_index_of__DFLT_FCCi_2_1;
  } sfmt_nfdcmps;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char in_FRk;
    unsigned char out_FRk;
  } sfmt_cfmadds;
  struct { /*  */
    INT f_d12;
    UINT f_FCCi_2;
    UINT f_GRi;
    unsigned short out_h_spr_USI_1;
    unsigned short out_h_spr_USI_768;
    unsigned short out_h_spr_USI_769;
    unsigned short out_h_spr_USI_770;
    unsigned short out_h_spr_USI_771;
    unsigned char in_FCCi_2;
    unsigned char in_GRi;
  } sfmt_ftine;
  struct { /*  */
    INT f_d12;
    UINT f_GRi;
    UINT f_ICCi_2;
    unsigned short out_h_spr_USI_1;
    unsigned short out_h_spr_USI_768;
    unsigned short out_h_spr_USI_769;
    unsigned short out_h_spr_USI_770;
    unsigned short out_h_spr_USI_771;
    unsigned char in_GRi;
    unsigned char in_ICCi_2;
  } sfmt_tieq;
  struct { /*  */
    UINT f_FRk;
    UINT f_GRj;
    unsigned char in_FRintk;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_1;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char in_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_3;
    unsigned char out_GRj;
    unsigned char out_h_gr_USI_add__DFLT_index_of__DFLT_GRj_1;
    unsigned char out_h_gr_USI_add__DFLT_index_of__DFLT_GRj_2;
    unsigned char out_h_gr_USI_add__DFLT_index_of__DFLT_GRj_3;
  } sfmt_movfgq;
  struct { /*  */
    UINT f_FRk;
    UINT f_GRj;
    unsigned char in_GRj;
    unsigned char in_h_gr_USI_add__DFLT_index_of__DFLT_GRj_1;
    unsigned char in_h_gr_USI_add__DFLT_index_of__DFLT_GRj_2;
    unsigned char in_h_gr_USI_add__DFLT_index_of__DFLT_GRj_3;
    unsigned char out_FRintk;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_1;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char out_h_fr_int_USI_add__DFLT_index_of__DFLT_FRintk_3;
  } sfmt_movgfq;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_GRk;
    unsigned char out_GRk;
  } sfmt_cswap;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRdoublek;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
  } sfmt_cstdfu;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRdoublek;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
  } sfmt_cstdu;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintk;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
  } sfmt_cstbfu;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_GRk;
    unsigned char out_GRi;
  } sfmt_cstbu;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_FRdoublek;
    unsigned char out_GRi;
  } sfmt_clddfu;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRdoublek;
    unsigned char out_GRi;
  } sfmt_clddu;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRk;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_FRintk;
    unsigned char out_GRi;
  } sfmt_cldbfu;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char out_GRi;
    unsigned char out_GRk;
  } sfmt_cldsbu;
  struct { /*  */
    UINT f_FCCk;
    UINT f_FRi;
    UINT f_FRj;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_FCCk;
    unsigned char out_h_fccr_UQI_add__DFLT_index_of__DFLT_FCCk_1;
  } sfmt_mcmpsh;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    UINT f_u6;
    unsigned char in_FRinti;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char out_FRinti;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_msllhi;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char in_FRk;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRk_1;
    unsigned char out_FRk;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_1;
  } sfmt_fdmadds;
  struct { /*  */
    UINT f_FCCi_2;
    UINT f_GRi;
    UINT f_GRj;
    unsigned short out_h_spr_USI_1;
    unsigned short out_h_spr_USI_768;
    unsigned short out_h_spr_USI_769;
    unsigned short out_h_spr_USI_770;
    unsigned short out_h_spr_USI_771;
    unsigned char in_FCCi_2;
    unsigned char in_GRi;
    unsigned char in_GRj;
  } sfmt_ftne;
  struct { /*  */
    UINT f_GRi;
    UINT f_GRj;
    UINT f_ICCi_2;
    unsigned short out_h_spr_USI_1;
    unsigned short out_h_spr_USI_768;
    unsigned short out_h_spr_USI_769;
    unsigned short out_h_spr_USI_770;
    unsigned short out_h_spr_USI_771;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_ICCi_2;
  } sfmt_teq;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_h_iccr_UQI_and__DFLT_index_of__DFLT_CCi_3;
    unsigned char out_GRdoublek;
    unsigned char out_h_iccr_UQI_and__DFLT_index_of__DFLT_CCi_3;
  } sfmt_csmulcc;
  struct { /*  */
    UINT f_CCi;
    UINT f_GRi;
    UINT f_GRj;
    UINT f_GRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_GRi;
    unsigned char in_GRj;
    unsigned char in_h_iccr_UQI_and__DFLT_index_of__DFLT_CCi_3;
    unsigned char out_GRk;
    unsigned char out_h_iccr_UQI_and__DFLT_index_of__DFLT_CCi_3;
  } sfmt_caddcc;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    unsigned char in_FRinti;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char out_FRinti;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_add__DFLT_0_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_add__DFLT_0_1;
  } sfmt_munpackh;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_1;
    unsigned char out_FRk;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_1;
  } sfmt_cfmas;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRk;
    UINT f_cond;
    UINT f_u6;
    unsigned char in_CCi;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_cmexpdhd;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_cmaddhss;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    UINT f_u6;
    unsigned char in_FRintieven;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char out_FRintieven;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_mqsllhi;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRi;
    unsigned char in_FRj;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_2;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRi_3;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_1;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_2;
    unsigned char in_h_fr_SF_add__DFLT_index_of__DFLT_FRj_3;
    unsigned char out_FRk;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_1;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_2;
    unsigned char out_h_fr_SF_add__DFLT_index_of__DFLT_FRk_3;
  } sfmt_fdmas;
  struct { /*  */
    UINT f_ACC40Uk;
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_cond;
    unsigned char in_ACC40Uk;
    unsigned char in_CCi;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char in_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_1;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_ACC40Uk;
    unsigned char out_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_1;
  } sfmt_cmmachu;
  struct { /*  */
    UINT f_ACC40Sk;
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_cond;
    unsigned char in_ACC40Sk;
    unsigned char in_CCi;
    unsigned char in_FRinti;
    unsigned char in_FRintj;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_1;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRinti_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_ACC40Sk;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_1;
  } sfmt_cmmachs;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintjeven;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char out_FRintjeven;
    unsigned char out_FRintk;
    unsigned char out_h_fr_0_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_1_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_2_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_3_UHI_add__DFLT_index_of__DFLT_FRintk_0;
  } sfmt_cmhtob;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintj;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_0_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_1_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_2_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_3_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_FRintj;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_cmbtoh;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    unsigned char in_FRintieven;
    unsigned char in_FRintjeven;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char out_FRintieven;
    unsigned char out_FRintjeven;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_mdpackh;
  struct { /*  */
    UINT f_FRi;
    UINT f_FRk;
    unsigned char in_FRintieven;
    unsigned char in_FRintk;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char out_FRintieven;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_add__DFLT_0_1;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_add__DFLT_2_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_add__DFLT_0_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_add__DFLT_2_1;
  } sfmt_mdunpackh;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintj;
    unsigned char in_FRintk;
    unsigned char in_h_fr_0_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_1_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_2_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char in_h_fr_3_UHI_add__DFLT_index_of__DFLT_FRintj_0;
    unsigned char out_FRintj;
    unsigned char out_FRintk;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_1;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintk_3;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_2;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintk_3;
  } sfmt_cmbtohe;
  struct { /*  */
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_FRk;
    UINT f_cond;
    unsigned char in_CCi;
    unsigned char in_FRintieven;
    unsigned char in_FRintjeven;
    unsigned char in_FRintkeven;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char out_FRintkeven;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_0;
    unsigned char out_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintkeven_1;
  } sfmt_cmqaddhss;
  struct { /*  */
    UINT f_ACC40Uk;
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_cond;
    unsigned char in_ACC40Uk;
    unsigned char in_CCi;
    unsigned char in_FRintieven;
    unsigned char in_FRintjeven;
    unsigned char in_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_1;
    unsigned char in_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_2;
    unsigned char in_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_3;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char out_ACC40Uk;
    unsigned char out_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_1;
    unsigned char out_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_2;
    unsigned char out_h_acc40U_UDI_add__DFLT_index_of__DFLT_ACC40Uk_3;
  } sfmt_cmqmachu;
  struct { /*  */
    UINT f_ACC40Sk;
    UINT f_CCi;
    UINT f_FRi;
    UINT f_FRj;
    UINT f_cond;
    unsigned char in_ACC40Sk;
    unsigned char in_CCi;
    unsigned char in_FRintieven;
    unsigned char in_FRintjeven;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_1;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_2;
    unsigned char in_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_3;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_hi_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintieven_1;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_0;
    unsigned char in_h_fr_lo_UHI_add__DFLT_index_of__DFLT_FRintjeven_1;
    unsigned char out_ACC40Sk;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_1;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_2;
    unsigned char out_h_acc40S_DI_add__DFLT_index_of__DFLT_ACC40Sk_3;
  } sfmt_cmqmachs;
#if WITH_SCACHE_PBB
  /* Writeback handler.  */
  struct {
    /* Pointer to argbuf entry for insn whose results need writing back.  */
    const struct argbuf *abuf;
  } write;
  /* x-before handler */
  struct {
    /*const SCACHE *insns[MAX_PARALLEL_INSNS];*/
    int first_p;
  } before;
  /* x-after handler */
  struct {
    int empty;
  } after;
  /* This entry is used to terminate each pbb.  */
  struct {
    /* Number of insns in pbb.  */
    int insn_count;
    /* Next pbb to execute.  */
    SCACHE *next;
    SCACHE *branch_target;
  } chain;
#endif
};

/* The ARGBUF struct.  */
struct argbuf {
  /* These are the baseclass definitions.  */
  IADDR addr;
  const IDESC *idesc;
  char trace_p;
  char profile_p;
  /* ??? Temporary hack for skip insns.  */
  char skip_count;
  char unused;
  /* cpu specific data follows */
  union sem semantic;
  int written;
  union sem_fields fields;
};

/* A cached insn.

   ??? SCACHE used to contain more than just argbuf.  We could delete the
   type entirely and always just use ARGBUF, but for future concerns and as
   a level of abstraction it is left in.  */

struct scache {
  struct argbuf argbuf;
  int first_insn_p;
  int last_insn_p;
};

/* Macros to simplify extraction, reading and semantic code.
   These define and assign the local vars that contain the insn's fields.  */

#define EXTRACT_IFMT_EMPTY_VARS \
  unsigned int length;
#define EXTRACT_IFMT_EMPTY_CODE \
  length = 0; \

#define EXTRACT_IFMT_ADD_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1_null; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_ADD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1_null = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_NOT_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ICCi_1_null; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_NOT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1_null = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SMUL_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1_null; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_SMUL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1_null = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SMU_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_SMU_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SLASS_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_SLASS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SCUTSS_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_SCUTSS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CADD_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CADD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CNOT_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CNOT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CSMUL_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CSMUL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ADDCC_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_ADDCC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1 = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SMULCC_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_SMULCC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1 = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ADDI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_ADDI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_SMULI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_SMULI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_ADDICC_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1; \
  INT f_s10; \
  unsigned int length;
#define EXTRACT_IFMT_ADDICC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1 = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_s10 = EXTRACT_LSB0_INT (insn, 32, 9, 10); \

#define EXTRACT_IFMT_SMULICC_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1; \
  INT f_s10; \
  unsigned int length;
#define EXTRACT_IFMT_SMULICC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1 = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_s10 = EXTRACT_LSB0_INT (insn, 32, 9, 10); \

#define EXTRACT_IFMT_CMPB_VARS \
  UINT f_pack; \
  UINT f_GRk_null; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ICCi_1; \
  UINT f_ope2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMPB_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ICCi_1 = EXTRACT_LSB0_UINT (insn, 32, 11, 2); \
  f_ope2 = EXTRACT_LSB0_UINT (insn, 32, 9, 4); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_SETLO_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_misc_null_4; \
  UINT f_u16; \
  unsigned int length;
#define EXTRACT_IFMT_SETLO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_4 = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_u16 = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_SETHI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_misc_null_4; \
  UINT f_u16; \
  unsigned int length;
#define EXTRACT_IFMT_SETHI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_4 = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_u16 = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_SETLOS_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_misc_null_4; \
  INT f_s16; \
  unsigned int length;
#define EXTRACT_IFMT_SETLOS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_4 = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_s16 = EXTRACT_LSB0_INT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_LDBF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_LDBF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LDC_VARS \
  UINT f_pack; \
  UINT f_CPRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_LDC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_CPRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LDD_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_LDD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LDDF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_LDDF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LDDC_VARS \
  UINT f_pack; \
  UINT f_CPRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_LDDC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_CPRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LDSBI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_LDSBI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_LDBFI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_LDBFI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_LDDI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_LDDI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_LDDFI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_LDDFI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_CLDBF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CLDBF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CLDDF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CLDDF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MOVGF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_MOVGF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMOVGF_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMOVGF_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MOVGS_VARS \
  UINT f_pack; \
  UINT f_op; \
  UINT f_spr_h; \
  UINT f_spr_l; \
  UINT f_spr; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_MOVGS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_spr_h = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_spr_l = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
{\
  f_spr = ((((f_spr_h) << (6))) | (f_spr_l));\
}\
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_BRA_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  SI f_label16; \
  unsigned int length;
#define EXTRACT_IFMT_BRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16 = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BNO_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_label16_null; \
  unsigned int length;
#define EXTRACT_IFMT_BNO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16_null = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_BEQ_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2; \
  UINT f_op; \
  UINT f_hint; \
  SI f_label16; \
  unsigned int length;
#define EXTRACT_IFMT_BEQ_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16 = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_FBRA_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  SI f_label16; \
  unsigned int length;
#define EXTRACT_IFMT_FBRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16 = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_FBNO_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_label16_null; \
  unsigned int length;
#define EXTRACT_IFMT_FBNO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16_null = EXTRACT_LSB0_UINT (insn, 32, 15, 16); \

#define EXTRACT_IFMT_FBNE_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_hint; \
  SI f_label16; \
  unsigned int length;
#define EXTRACT_IFMT_FBNE_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_label16 = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BCTRLR_VARS \
  UINT f_pack; \
  UINT f_cond_null; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BCTRLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_cond_null = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BRALR_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BRALR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BNOLR_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BNOLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BEQLR_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BEQLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FBRALR_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FBRALR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FBNOLR_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FBNOLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FBEQLR_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FBEQLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond_null = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BCRALR_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BCRALR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BCEQLR_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_BCEQLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FCBRALR_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FCBRALR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FCBEQLR_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_hint; \
  UINT f_ope3; \
  UINT f_ccond; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FCBEQLR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_hint = EXTRACT_LSB0_UINT (insn, 32, 17, 2); \
  f_ope3 = EXTRACT_LSB0_UINT (insn, 32, 15, 3); \
  f_ccond = EXTRACT_LSB0_UINT (insn, 32, 12, 1); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_JMPL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_off; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_JMPL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_off = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_2 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CALLL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_on; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_2; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CALLL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_on = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_2 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_JMPIL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_off; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_JMPIL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_off = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_CALLIL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_on; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_CALLIL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_on = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_CALL_VARS \
  UINT f_pack; \
  UINT f_op; \
  INT f_labelH6; \
  UINT f_labelL18; \
  INT f_label24; \
  unsigned int length;
#define EXTRACT_IFMT_CALL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_labelH6 = EXTRACT_LSB0_INT (insn, 32, 30, 6); \
  f_labelL18 = EXTRACT_LSB0_UINT (insn, 32, 17, 18); \
{\
  f_label24 = ((((((((f_labelH6) << (18))) | (f_labelL18))) << (2))) + (pc));\
}\

#define EXTRACT_IFMT_RETT_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_debug; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_RETT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_debug = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_REI_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_eir; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_REI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_eir = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_TRA_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_TRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_TNO_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_GRi_null; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_TNO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_TEQ_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_TEQ_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FTRA_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_FTRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FTNO_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_GRi_null; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_FTNO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FTNE_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_FTNE_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_TIRA_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_TIRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_TINO_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_GRi_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_TINO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_TIEQ_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  UINT f_ICCi_2; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_TIEQ_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FTIRA_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_ICCi_2_null; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_FTIRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_ICCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FTINO_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2_null; \
  UINT f_op; \
  UINT f_GRi_null; \
  UINT f_s12_null; \
  unsigned int length;
#define EXTRACT_IFMT_FTINO_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2_null = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_s12_null = EXTRACT_LSB0_UINT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_FTINE_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_GRi; \
  INT f_d12; \
  unsigned int length;
#define EXTRACT_IFMT_FTINE_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_d12 = EXTRACT_LSB0_INT (insn, 32, 11, 12); \

#define EXTRACT_IFMT_BREAK_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_misc_null_3; \
  UINT f_ope4; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_BREAK_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_misc_null_3 = EXTRACT_LSB0_UINT (insn, 32, 11, 4); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ANDCR_VARS \
  UINT f_pack; \
  UINT f_misc_null_6; \
  UINT f_CRk; \
  UINT f_op; \
  UINT f_misc_null_7; \
  UINT f_CRi; \
  UINT f_ope1; \
  UINT f_misc_null_8; \
  UINT f_CRj; \
  unsigned int length;
#define EXTRACT_IFMT_ANDCR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_6 = EXTRACT_LSB0_UINT (insn, 32, 30, 3); \
  f_CRk = EXTRACT_LSB0_UINT (insn, 32, 27, 3); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_7 = EXTRACT_LSB0_UINT (insn, 32, 17, 3); \
  f_CRi = EXTRACT_LSB0_UINT (insn, 32, 14, 3); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_misc_null_8 = EXTRACT_LSB0_UINT (insn, 32, 5, 3); \
  f_CRj = EXTRACT_LSB0_UINT (insn, 32, 2, 3); \

#define EXTRACT_IFMT_NOTCR_VARS \
  UINT f_pack; \
  UINT f_misc_null_6; \
  UINT f_CRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_misc_null_8; \
  UINT f_CRj; \
  unsigned int length;
#define EXTRACT_IFMT_NOTCR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_6 = EXTRACT_LSB0_UINT (insn, 32, 30, 3); \
  f_CRk = EXTRACT_LSB0_UINT (insn, 32, 27, 3); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_misc_null_8 = EXTRACT_LSB0_UINT (insn, 32, 5, 3); \
  f_CRj = EXTRACT_LSB0_UINT (insn, 32, 2, 3); \

#define EXTRACT_IFMT_CKRA_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  SI f_CRj_int; \
  UINT f_op; \
  UINT f_misc_null_5; \
  UINT f_ICCi_3_null; \
  unsigned int length;
#define EXTRACT_IFMT_CKRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_int = ((EXTRACT_LSB0_UINT (insn, 32, 26, 2)) + (4)); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_5 = EXTRACT_LSB0_UINT (insn, 32, 17, 16); \
  f_ICCi_3_null = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CKEQ_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  SI f_CRj_int; \
  UINT f_op; \
  UINT f_misc_null_5; \
  UINT f_ICCi_3; \
  unsigned int length;
#define EXTRACT_IFMT_CKEQ_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_int = ((EXTRACT_LSB0_UINT (insn, 32, 26, 2)) + (4)); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_5 = EXTRACT_LSB0_UINT (insn, 32, 17, 16); \
  f_ICCi_3 = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_FCKRA_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_CRj_float; \
  UINT f_op; \
  UINT f_misc_null_5; \
  UINT f_FCCi_3; \
  unsigned int length;
#define EXTRACT_IFMT_FCKRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_float = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_misc_null_5 = EXTRACT_LSB0_UINT (insn, 32, 17, 16); \
  f_FCCi_3 = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CCKRA_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  SI f_CRj_int; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_misc_null_9; \
  UINT f_ICCi_3_null; \
  unsigned int length;
#define EXTRACT_IFMT_CCKRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_int = ((EXTRACT_LSB0_UINT (insn, 32, 26, 2)) + (4)); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_misc_null_9 = EXTRACT_LSB0_UINT (insn, 32, 5, 4); \
  f_ICCi_3_null = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CCKEQ_VARS \
  UINT f_pack; \
  UINT f_int_cc; \
  SI f_CRj_int; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_misc_null_9; \
  UINT f_ICCi_3; \
  unsigned int length;
#define EXTRACT_IFMT_CCKEQ_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_int_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_int = ((EXTRACT_LSB0_UINT (insn, 32, 26, 2)) + (4)); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_misc_null_9 = EXTRACT_LSB0_UINT (insn, 32, 5, 4); \
  f_ICCi_3 = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CFCKRA_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_CRj_float; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_misc_null_9; \
  UINT f_FCCi_3_null; \
  unsigned int length;
#define EXTRACT_IFMT_CFCKRA_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_float = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_misc_null_9 = EXTRACT_LSB0_UINT (insn, 32, 5, 4); \
  f_FCCi_3_null = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CFCKNE_VARS \
  UINT f_pack; \
  UINT f_flt_cc; \
  UINT f_CRj_float; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_misc_null_9; \
  UINT f_FCCi_3; \
  unsigned int length;
#define EXTRACT_IFMT_CFCKNE_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_flt_cc = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_CRj_float = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_misc_null_9 = EXTRACT_LSB0_UINT (insn, 32, 5, 4); \
  f_FCCi_3 = EXTRACT_LSB0_UINT (insn, 32, 1, 2); \

#define EXTRACT_IFMT_CJMPL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_off; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CJMPL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_off = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CCALLL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_LI_on; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_CCALLL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_LI_on = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ICEI_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_ae; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_ICEI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_ae = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ICPL_VARS \
  UINT f_pack; \
  UINT f_misc_null_1; \
  UINT f_lock; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_ICPL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_misc_null_1 = EXTRACT_LSB0_UINT (insn, 32, 30, 5); \
  f_lock = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_ICUL_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_ICUL_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_BAR_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_BAR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_LRAI_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_LRAE; \
  UINT f_LRAD; \
  UINT f_LRAS; \
  UINT f_LRA_null; \
  unsigned int length;
#define EXTRACT_IFMT_LRAI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_LRAE = EXTRACT_LSB0_UINT (insn, 32, 5, 1); \
  f_LRAD = EXTRACT_LSB0_UINT (insn, 32, 4, 1); \
  f_LRAS = EXTRACT_LSB0_UINT (insn, 32, 3, 1); \
  f_LRA_null = EXTRACT_LSB0_UINT (insn, 32, 2, 3); \

#define EXTRACT_IFMT_TLBPR_VARS \
  UINT f_pack; \
  UINT f_TLBPR_null; \
  UINT f_TLBPRopx; \
  UINT f_TLBPRL; \
  UINT f_op; \
  UINT f_GRi; \
  UINT f_ope1; \
  UINT f_GRj; \
  unsigned int length;
#define EXTRACT_IFMT_TLBPR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_TLBPR_null = EXTRACT_LSB0_UINT (insn, 32, 30, 2); \
  f_TLBPRopx = EXTRACT_LSB0_UINT (insn, 32, 28, 3); \
  f_TLBPRL = EXTRACT_LSB0_UINT (insn, 32, 25, 1); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_GRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_COP1_VARS \
  UINT f_pack; \
  UINT f_CPRk; \
  UINT f_op; \
  UINT f_CPRi; \
  INT f_s6_1; \
  UINT f_CPRj; \
  unsigned int length;
#define EXTRACT_IFMT_COP1_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_CPRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_CPRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_s6_1 = EXTRACT_LSB0_INT (insn, 32, 11, 6); \
  f_CPRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CLRGR_VARS \
  UINT f_pack; \
  UINT f_GRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_CLRGR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_GRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CLRFR_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_GRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_CLRFR_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_GRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FITOS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FITOS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FSTOI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FSTOI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FITOD_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FITOD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FDTOI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FDTOI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CFITOS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CFITOS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CFSTOI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CFSTOI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FMOVS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FMOVS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FMOVD_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FMOVD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CFMOVS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CFMOVS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FADDS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FADDS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FADDD_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FADDD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CFADDS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CFADDS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FCMPS_VARS \
  UINT f_pack; \
  UINT f_cond_null; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FCMPS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_cond_null = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FCMPD_VARS \
  UINT f_pack; \
  UINT f_cond_null; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_FCMPD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_cond_null = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CFCMPS_VARS \
  UINT f_pack; \
  UINT f_cond_null; \
  UINT f_FCCi_2; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CFCMPS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_cond_null = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCi_2 = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MHSETLOS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ope1; \
  INT f_u12_h; \
  UINT f_u12_l; \
  INT f_u12; \
  unsigned int length;
#define EXTRACT_IFMT_MHSETLOS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u12_h = EXTRACT_LSB0_INT (insn, 32, 17, 6); \
  f_u12_l = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \
{\
  f_u12 = ((((f_u12_h) << (6))) | (f_u12_l));\
}\

#define EXTRACT_IFMT_MHSETHIS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ope1; \
  INT f_u12_h; \
  UINT f_u12_l; \
  INT f_u12; \
  unsigned int length;
#define EXTRACT_IFMT_MHSETHIS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u12_h = EXTRACT_LSB0_INT (insn, 32, 17, 6); \
  f_u12_l = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \
{\
  f_u12 = ((((f_u12_h) << (6))) | (f_u12_l));\
}\

#define EXTRACT_IFMT_MHDSETS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ope1; \
  INT f_u12_h; \
  UINT f_u12_l; \
  INT f_u12; \
  unsigned int length;
#define EXTRACT_IFMT_MHDSETS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u12_h = EXTRACT_LSB0_INT (insn, 32, 17, 6); \
  f_u12_l = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \
{\
  f_u12 = ((((f_u12_h) << (6))) | (f_u12_l));\
}\

#define EXTRACT_IFMT_MHSETLOH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_misc_null_11; \
  INT f_s5; \
  unsigned int length;
#define EXTRACT_IFMT_MHSETLOH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_misc_null_11 = EXTRACT_LSB0_UINT (insn, 32, 5, 1); \
  f_s5 = EXTRACT_LSB0_INT (insn, 32, 4, 5); \

#define EXTRACT_IFMT_MHSETHIH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_misc_null_11; \
  INT f_s5; \
  unsigned int length;
#define EXTRACT_IFMT_MHSETHIH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_misc_null_11 = EXTRACT_LSB0_UINT (insn, 32, 5, 1); \
  f_s5 = EXTRACT_LSB0_INT (insn, 32, 4, 5); \

#define EXTRACT_IFMT_MHDSETH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_misc_null_11; \
  INT f_s5; \
  unsigned int length;
#define EXTRACT_IFMT_MHDSETH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_misc_null_11 = EXTRACT_LSB0_UINT (insn, 32, 5, 1); \
  f_s5 = EXTRACT_LSB0_INT (insn, 32, 4, 5); \

#define EXTRACT_IFMT_MAND_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MAND_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMAND_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMAND_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MNOT_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MNOT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMNOT_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_rs_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMNOT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_rs_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MROTLI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_MROTLI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MCUT_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ACC40Si; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MCUT_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACC40Si = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MCUTI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ACC40Si; \
  UINT f_ope1; \
  INT f_s6; \
  unsigned int length;
#define EXTRACT_IFMT_MCUTI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACC40Si = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_s6 = EXTRACT_LSB0_INT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MDCUTSSI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ACC40Si; \
  UINT f_ope1; \
  INT f_s6; \
  unsigned int length;
#define EXTRACT_IFMT_MDCUTSSI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACC40Si = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_s6 = EXTRACT_LSB0_INT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MDROTLI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  INT f_s6; \
  unsigned int length;
#define EXTRACT_IFMT_MDROTLI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_s6 = EXTRACT_LSB0_INT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MQSATHS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MQSATHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MCMPSH_VARS \
  UINT f_pack; \
  UINT f_cond_null; \
  UINT f_FCCk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MCMPSH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_cond_null = EXTRACT_LSB0_UINT (insn, 32, 30, 4); \
  f_FCCk = EXTRACT_LSB0_UINT (insn, 32, 26, 2); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MABSHS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MABSHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMQADDHSS_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMQADDHSS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MQSLLHI_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_MQSLLHI_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MADDACCS_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_ACC40Si; \
  UINT f_ope1; \
  UINT f_ACCj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MADDACCS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACC40Si = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_ACCj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MMULHS_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MMULHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMMULHS_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMMULHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MQMULHS_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MQMULHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMQMULHS_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMQMULHS_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MMACHU_VARS \
  UINT f_pack; \
  UINT f_ACC40Uk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MMACHU_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Uk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMMACHU_VARS \
  UINT f_pack; \
  UINT f_ACC40Uk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMMACHU_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Uk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MQMACHU_VARS \
  UINT f_pack; \
  UINT f_ACC40Uk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MQMACHU_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Uk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMQMACHU_VARS \
  UINT f_pack; \
  UINT f_ACC40Uk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMQMACHU_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Uk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMEXPDHW_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_CMEXPDHW_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MEXPDHD_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_MEXPDHD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMEXPDHD_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_u6; \
  unsigned int length;
#define EXTRACT_IFMT_CMEXPDHD_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_u6 = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MUNPACKH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MUNPACKH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MDUNPACKH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MDUNPACKH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MBTOH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MBTOH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMBTOH_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMBTOH_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MHTOB_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_MHTOB_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMHTOB_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMHTOB_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_CMBTOHE_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_CCi; \
  UINT f_cond; \
  UINT f_ope4; \
  UINT f_FRj; \
  unsigned int length;
#define EXTRACT_IFMT_CMBTOHE_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_CCi = EXTRACT_LSB0_UINT (insn, 32, 11, 3); \
  f_cond = EXTRACT_LSB0_UINT (insn, 32, 8, 1); \
  f_ope4 = EXTRACT_LSB0_UINT (insn, 32, 7, 2); \
  f_FRj = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MNOP_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_A; \
  UINT f_misc_null_10; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MNOP_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_A = EXTRACT_LSB0_UINT (insn, 32, 17, 1); \
  f_misc_null_10 = EXTRACT_LSB0_UINT (insn, 32, 16, 5); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MCLRACC_0_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_A; \
  UINT f_misc_null_10; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MCLRACC_0_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_A = EXTRACT_LSB0_UINT (insn, 32, 17, 1); \
  f_misc_null_10 = EXTRACT_LSB0_UINT (insn, 32, 16, 5); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MRDACC_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ACC40Si; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MRDACC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACC40Si = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MRDACCG_VARS \
  UINT f_pack; \
  UINT f_FRk; \
  UINT f_op; \
  UINT f_ACCGi; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MRDACCG_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_FRk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_ACCGi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MWTACC_VARS \
  UINT f_pack; \
  UINT f_ACC40Sk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MWTACC_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACC40Sk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_MWTACCG_VARS \
  UINT f_pack; \
  UINT f_ACCGk; \
  UINT f_op; \
  UINT f_FRi; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_MWTACCG_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_ACCGk = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

#define EXTRACT_IFMT_FNOP_VARS \
  UINT f_pack; \
  UINT f_rd_null; \
  UINT f_op; \
  UINT f_FRi_null; \
  UINT f_ope1; \
  UINT f_FRj_null; \
  unsigned int length;
#define EXTRACT_IFMT_FNOP_CODE \
  length = 4; \
  f_pack = EXTRACT_LSB0_UINT (insn, 32, 31, 1); \
  f_rd_null = EXTRACT_LSB0_UINT (insn, 32, 30, 6); \
  f_op = EXTRACT_LSB0_UINT (insn, 32, 24, 7); \
  f_FRi_null = EXTRACT_LSB0_UINT (insn, 32, 17, 6); \
  f_ope1 = EXTRACT_LSB0_UINT (insn, 32, 11, 6); \
  f_FRj_null = EXTRACT_LSB0_UINT (insn, 32, 5, 6); \

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_FRVBF_H */
