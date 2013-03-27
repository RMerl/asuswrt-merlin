/* collection of junk waiting time to sort out
   Copyright (C) 1998, 1999, 2000, 2001, 2003, 2007
   Free Software Foundation, Inc.
   Contributed by Red Hat

This file is part of the GNU Simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef FRV_SIM_H
#define FRV_SIM_H

#include "sim-options.h"

/* True if SPR is the number of accumulator or accumulator guard register.  */
#define SPR_IS_ACC(SPR) ((SPR) >= 1408 && (SPR) <= 1535)

/* Initialization of the frv cpu.  */
void frv_initialize (SIM_CPU *, SIM_DESC);
void frv_term (SIM_DESC);
void frv_power_on_reset (SIM_CPU *);
void frv_hardware_reset (SIM_CPU *);
void frv_software_reset (SIM_CPU *);

/* The reset register.  See FRV LSI section 10.3.1  */
#define RSTR_ADDRESS        0xfeff0500
#define RSTR_INITIAL_VALUE  0x00000400
#define RSTR_HARDWARE_RESET 0x00000200
#define RSTR_SOFTWARE_RESET 0x00000100

#define GET_RSTR_HR(rstr) (((rstr) >> 1) & 1)
#define GET_RSTR_SR(rstr) (((rstr)     ) & 1)

#define SET_RSTR_H(rstr) ((rstr) |= (1 << 9))
#define SET_RSTR_S(rstr) ((rstr) |= (1 << 8))

#define CLEAR_RSTR_P(rstr)  ((rstr) &= ~(1 << 10))
#define CLEAR_RSTR_H(rstr)  ((rstr) &= ~(1 <<  9))
#define CLEAR_RSTR_S(rstr)  ((rstr) &= ~(1 <<  8))
#define CLEAR_RSTR_HR(rstr) ((rstr) &= ~(1 <<  1))
#define CLEAR_RSTR_SR(rstr) ((rstr) &= ~1)

/* Cutomized hardware get/set functions.  */
extern USI  frvbf_h_spr_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_spr_set_handler (SIM_CPU *, UINT, USI);
extern USI  frvbf_h_gr_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_gr_set_handler (SIM_CPU *, UINT, USI);
extern UHI  frvbf_h_gr_hi_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_gr_hi_set_handler (SIM_CPU *, UINT, UHI);
extern UHI  frvbf_h_gr_lo_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_gr_lo_set_handler (SIM_CPU *, UINT, UHI);
extern DI   frvbf_h_gr_double_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_gr_double_set_handler (SIM_CPU *, UINT, DI);
extern SF   frvbf_h_fr_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_fr_set_handler (SIM_CPU *, UINT, SF);
extern DF   frvbf_h_fr_double_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_fr_double_set_handler (SIM_CPU *, UINT, DF);
extern USI  frvbf_h_fr_int_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_fr_int_set_handler (SIM_CPU *, UINT, USI);
extern DI   frvbf_h_cpr_double_get_handler (SIM_CPU *, UINT);
extern void frvbf_h_cpr_double_set_handler (SIM_CPU *, UINT, DI);
extern void frvbf_h_gr_quad_set_handler (SIM_CPU *, UINT, SI *);
extern void frvbf_h_fr_quad_set_handler (SIM_CPU *, UINT, SI *);
extern void frvbf_h_cpr_quad_set_handler (SIM_CPU *, UINT, SI *);
extern void frvbf_h_psr_s_set_handler (SIM_CPU *, BI);

extern USI  spr_psr_get_handler (SIM_CPU *);
extern void spr_psr_set_handler (SIM_CPU *, USI);
extern USI  spr_tbr_get_handler (SIM_CPU *);
extern void spr_tbr_set_handler (SIM_CPU *, USI);
extern USI  spr_bpsr_get_handler (SIM_CPU *);
extern void spr_bpsr_set_handler (SIM_CPU *, USI);
extern USI  spr_ccr_get_handler (SIM_CPU *);
extern void spr_ccr_set_handler (SIM_CPU *, USI);
extern void spr_cccr_set_handler (SIM_CPU *, USI);
extern USI  spr_cccr_get_handler (SIM_CPU *);
extern USI  spr_isr_get_handler (SIM_CPU *);
extern void spr_isr_set_handler (SIM_CPU *, USI);
extern USI  spr_sr_get_handler (SIM_CPU *, UINT);
extern void spr_sr_set_handler (SIM_CPU *, UINT, USI);

extern void frvbf_switch_supervisor_user_context (SIM_CPU *);

extern QI frvbf_set_icc_for_shift_left  (SIM_CPU *, SI, SI, QI);
extern QI frvbf_set_icc_for_shift_right (SIM_CPU *, SI, SI, QI);

/* Insn semantics.  */
extern void frvbf_signed_integer_divide (SIM_CPU *, SI, SI, int, int);
extern void frvbf_unsigned_integer_divide (SIM_CPU *, USI, USI, int, int);
extern SI   frvbf_shift_left_arith_saturate (SIM_CPU *, SI, SI);
extern SI   frvbf_iacc_cut (SIM_CPU *, DI, SI);

extern void frvbf_clear_accumulators (SIM_CPU *, SI, int);

extern SI   frvbf_scan_result (SIM_CPU *, SI);
extern SI   frvbf_cut (SIM_CPU *, SI, SI, SI);
extern SI   frvbf_media_cut (SIM_CPU *, DI, SI);
extern SI   frvbf_media_cut_ss (SIM_CPU *, DI, SI);
extern void frvbf_media_cop (SIM_CPU *, int);
extern UQI  frvbf_cr_logic (SIM_CPU *, SI, UQI, UQI);

extern void frvbf_set_write_next_vliw_addr_to_LR (SIM_CPU *, int);
extern int  frvbf_write_next_vliw_addr_to_LR;

extern void frvbf_set_ne_index (SIM_CPU *, int);
extern void frvbf_force_update (SIM_CPU *);

#define GETTWI GETTSI
#define SETTWI SETTSI
#define LEUINT LEUSI

/* Hardware/device support.
   ??? Will eventually want to move device stuff to config files.  */

/* Support for the MCCR register (Cache Control Register) is needed in order
   for overlays to work correctly with the scache: cached instructions need
   to be flushed when the instruction space is changed at runtime.  */

/* These were just copied from another port and are necessary to build, but
   but don't appear to be used.  */
#define MCCR_ADDR 0xffffffff
#define MCCR_CP 0x80
/* not supported */
#define MCCR_CM0 2
#define MCCR_CM1 1

/* sim_core_attach device argument.  */
extern device frv_devices;

/* FIXME: Temporary, until device support ready.  */
struct _device { int foo; };

/* maintain the address of the start of the previous VLIW insn sequence.  */
extern IADDR previous_vliw_pc;
extern CGEN_ATTR_VALUE_ENUM_TYPE frv_current_fm_slot;

/* Hardware status.  */
#define GET_HSR0() GET_H_SPR (H_SPR_HSR0)
#define SET_HSR0(hsr0) SET_H_SPR (H_SPR_HSR0, (hsr0))

#define GET_HSR0_ICE(hsr0) (((hsr0) >> 31) & 1)
#define SET_HSR0_ICE(hsr0) ((hsr0) |= (1 << 31))
#define CLEAR_HSR0_ICE(hsr0) ((hsr0) &= ~(1 << 31))

#define GET_HSR0_DCE(hsr0) (((hsr0) >> 30) & 1)
#define SET_HSR0_DCE(hsr0) ((hsr0) |= (1 << 30))
#define CLEAR_HSR0_DCE(hsr0) ((hsr0) &= ~(1 << 30))

#define GET_HSR0_CBM(hsr0) (((hsr0) >> 27) & 1)
#define GET_HSR0_RME(hsr0) (((hsr0) >> 22) & 1)
#define GET_HSR0_SA(hsr0)  (((hsr0) >> 12) & 1)
#define GET_HSR0_FRN(hsr0) (((hsr0) >> 11) & 1)
#define GET_HSR0_GRN(hsr0) (((hsr0) >> 10) & 1)
#define GET_HSR0_FRHE(hsr0) (((hsr0) >> 9) & 1)
#define GET_HSR0_FRLE(hsr0) (((hsr0) >> 8) & 1)
#define GET_HSR0_GRHE(hsr0) (((hsr0) >> 7) & 1)
#define GET_HSR0_GRLE(hsr0) (((hsr0) >> 6) & 1)

#define GET_IHSR8() GET_H_SPR (H_SPR_IHSR8)
#define GET_IHSR8_NBC(ihsr8) ((ihsr8) & 1)
#define GET_IHSR8_ICDM(ihsr8) (((ihsr8) >>  1) & 1)
#define GET_IHSR8_ICWE(ihsr8) (((ihsr8) >>  8) & 7)
#define GET_IHSR8_DCWE(ihsr8) (((ihsr8) >> 12) & 7)

void frvbf_insn_cache_preload (SIM_CPU *, SI, USI, int);
void frvbf_data_cache_preload (SIM_CPU *, SI, USI, int);
void frvbf_insn_cache_unlock (SIM_CPU *, SI);
void frvbf_data_cache_unlock (SIM_CPU *, SI);
void frvbf_insn_cache_invalidate (SIM_CPU *, SI, int);
void frvbf_data_cache_invalidate (SIM_CPU *, SI, int);
void frvbf_data_cache_flush (SIM_CPU *, SI, int);

/* FR-V Interrupt classes.
   These are declared in order of increasing priority.  */
enum frv_interrupt_class
{
  FRV_EXTERNAL_INTERRUPT,
  FRV_SOFTWARE_INTERRUPT,
  FRV_PROGRAM_INTERRUPT,
  FRV_BREAK_INTERRUPT,
  FRV_RESET_INTERRUPT,
  NUM_FRV_INTERRUPT_CLASSES
};

/* FR-V Interrupt kinds.
   These are declared in order of increasing priority.  */
enum frv_interrupt_kind
{
  /* External interrupts */
  FRV_INTERRUPT_LEVEL_1,
  FRV_INTERRUPT_LEVEL_2,
  FRV_INTERRUPT_LEVEL_3,
  FRV_INTERRUPT_LEVEL_4,
  FRV_INTERRUPT_LEVEL_5,
  FRV_INTERRUPT_LEVEL_6,
  FRV_INTERRUPT_LEVEL_7,
  FRV_INTERRUPT_LEVEL_8,
  FRV_INTERRUPT_LEVEL_9,
  FRV_INTERRUPT_LEVEL_10,
  FRV_INTERRUPT_LEVEL_11,
  FRV_INTERRUPT_LEVEL_12,
  FRV_INTERRUPT_LEVEL_13,
  FRV_INTERRUPT_LEVEL_14,
  FRV_INTERRUPT_LEVEL_15,
  /* Software interrupt */
  FRV_TRAP_INSTRUCTION,
  /* Program interrupts */
  FRV_COMMIT_EXCEPTION,
  FRV_DIVISION_EXCEPTION,
  FRV_DATA_STORE_ERROR,
  FRV_DATA_ACCESS_EXCEPTION,
  FRV_DATA_ACCESS_MMU_MISS,
  FRV_DATA_ACCESS_ERROR,
  FRV_MP_EXCEPTION,
  FRV_FP_EXCEPTION,
  FRV_MEM_ADDRESS_NOT_ALIGNED,
  FRV_REGISTER_EXCEPTION,
  FRV_MP_DISABLED,
  FRV_FP_DISABLED,
  FRV_PRIVILEGED_INSTRUCTION,
  FRV_ILLEGAL_INSTRUCTION,
  FRV_INSTRUCTION_ACCESS_EXCEPTION,
  FRV_INSTRUCTION_ACCESS_ERROR,
  FRV_INSTRUCTION_ACCESS_MMU_MISS,
  FRV_COMPOUND_EXCEPTION,
  /* Break interrupt */
  FRV_BREAK_EXCEPTION,
  /* Reset interrupt */
  FRV_RESET,
  NUM_FRV_INTERRUPT_KINDS
};

/* FRV interrupt exception codes */
enum frv_ec
{
  FRV_EC_DATA_STORE_ERROR             = 0x00,
  FRV_EC_INSTRUCTION_ACCESS_MMU_MISS  = 0x01,
  FRV_EC_INSTRUCTION_ACCESS_ERROR     = 0x02,
  FRV_EC_INSTRUCTION_ACCESS_EXCEPTION = 0x03,
  FRV_EC_PRIVILEGED_INSTRUCTION       = 0x04,
  FRV_EC_ILLEGAL_INSTRUCTION          = 0x05,
  FRV_EC_FP_DISABLED                  = 0x06,
  FRV_EC_MP_DISABLED                  = 0x07,
  FRV_EC_MEM_ADDRESS_NOT_ALIGNED      = 0x0b,
  FRV_EC_REGISTER_EXCEPTION           = 0x0c,
  FRV_EC_FP_EXCEPTION                 = 0x0d,
  FRV_EC_MP_EXCEPTION                 = 0x0e,
  FRV_EC_DATA_ACCESS_ERROR            = 0x10,
  FRV_EC_DATA_ACCESS_MMU_MISS         = 0x11,
  FRV_EC_DATA_ACCESS_EXCEPTION        = 0x12,
  FRV_EC_DIVISION_EXCEPTION           = 0x13,
  FRV_EC_COMMIT_EXCEPTION             = 0x14,
  FRV_EC_NOT_EXECUTED                 = 0x1f,
  FRV_EC_INTERRUPT_LEVEL_1            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_2            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_3            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_4            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_5            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_6            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_7            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_8            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_9            = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_10           = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_11           = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_12           = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_13           = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_14           = FRV_EC_NOT_EXECUTED,
  FRV_EC_INTERRUPT_LEVEL_15           = FRV_EC_NOT_EXECUTED,
  FRV_EC_TRAP_INSTRUCTION             = FRV_EC_NOT_EXECUTED,
  FRV_EC_COMPOUND_EXCEPTION           = FRV_EC_NOT_EXECUTED,
  FRV_EC_BREAK_EXCEPTION              = FRV_EC_NOT_EXECUTED,
  FRV_EC_RESET                        = FRV_EC_NOT_EXECUTED
};

/* FR-V Interrupt.
   This struct contains enough information to describe a particular interrupt
   occurance.  */
struct frv_interrupt
{
  enum frv_interrupt_kind  kind;
  enum frv_ec              ec;
  enum frv_interrupt_class iclass;
  unsigned char deferred;
  unsigned char precise;
  unsigned char handler_offset;
};

/* FR-V Interrupt table.
   Describes the interrupts supported by the FR-V.  */
extern struct frv_interrupt frv_interrupt_table[];

/* FR-V Interrupt State.
   Interrupts are queued during execution of parallel insns and the interupt(s)
   to be handled determined by analysing the queue after each VLIW insn.  */
#define FRV_INTERRUPT_QUEUE_SIZE (4 * 4) /* 4 interrupts x 4 insns for now.  */

/* register_exception codes */
enum frv_rec
{
  FRV_REC_UNIMPLEMENTED = 0,
  FRV_REC_UNALIGNED     = 1
};

/* instruction_access_exception codes */
enum frv_iaec
{
  FRV_IAEC_PROTECT_VIOLATION = 1
};

/* data_access_exception codes */
enum frv_daec
{
  FRV_DAEC_PROTECT_VIOLATION = 1
};

/* division_exception ISR codes */
enum frv_dtt
{
  FRV_DTT_NO_EXCEPTION     = 0,
  FRV_DTT_DIVISION_BY_ZERO = 1,
  FRV_DTT_OVERFLOW         = 2,
  FRV_DTT_BOTH             = 3
};

/* data written during an insn causing an interrupt */
struct frv_data_written
{
  USI words[4]; /* Actual data in words */
  int length;   /* length of data written */
};

/* fp_exception info */
/* Trap codes for FSR0 and FQ registers.  */
enum frv_fsr_traps
{
  FSR_INVALID_OPERATION = 0x20,
  FSR_OVERFLOW          = 0x10,
  FSR_UNDERFLOW         = 0x08,
  FSR_DIVISION_BY_ZERO  = 0x04,
  FSR_INEXACT           = 0x02,
  FSR_DENORMAL_INPUT    = 0x01,
  FSR_NO_EXCEPTION      = 0
};

/* Floating point trap types for FSR.  */
enum frv_fsr_ftt
{
  FTT_NONE               = 0,
  FTT_IEEE_754_EXCEPTION = 1,
  FTT_UNIMPLEMENTED_FPOP = 3,
  FTT_SEQUENCE_ERROR     = 4,
  FTT_INVALID_FR         = 6,
  FTT_DENORMAL_INPUT     = 7
};

struct frv_fp_exception_info
{
  enum frv_fsr_traps fsr_mask; /* interrupt code for FSR */
  enum frv_fsr_ftt   ftt;      /* floating point trap type */
};

struct frv_interrupt_queue_element
{
  enum frv_interrupt_kind kind;      /* kind of interrupt */
  IADDR                   vpc;       /* address of insn causing interrupt */
  int                     slot;      /* VLIW slot containing the insn.  */
  USI                     eaddress;  /* address of data access */
  union {
    enum frv_rec  rec;               /* register exception code */
    enum frv_iaec iaec;              /* insn access exception code */
    enum frv_daec daec;              /* data access exception code */
    enum frv_dtt  dtt;               /* division exception code */
    struct frv_fp_exception_info fp_info;
    struct frv_data_written data_written;
  } u;
};

struct frv_interrupt_timer
{
  int enabled;
  unsigned value;
  unsigned current;
  enum frv_interrupt_kind interrupt;
};

struct frv_interrupt_state
{
  /* The interrupt queue */
  struct frv_interrupt_queue_element queue[FRV_INTERRUPT_QUEUE_SIZE];
  int queue_index;

  /* interrupt queue element causing imprecise interrupt.  */
  struct frv_interrupt_queue_element *imprecise_interrupt;

  /* interrupt timer.  */
  struct frv_interrupt_timer timer;

  /* The last data written stored as an array of words.  */
  struct frv_data_written data_written;

  /* The vliw slot of the insn causing the interrupt.  */
  int slot;

  /* target register index for non excepting insns.  */
#define NE_NOFLAG (-1)
  int ne_index;

  /* Accumulated NE flags for non excepting floating point insns.  */
  SI f_ne_flags[2];
};

extern struct frv_interrupt_state frv_interrupt_state;

/* Macros to manipulate the PSR.  */
#define GET_PSR() GET_H_SPR (H_SPR_PSR)

#define SET_PSR_ET(psr, et) (           \
  (psr) = ((psr) & ~0x1) | ((et) & 0x1) \
)

#define GET_PSR_PS(psr) (((psr) >> 1) & 1)

#define SET_PSR_S(psr, s) (                          \
  (psr) = ((psr) & ~(0x1 << 2)) | (((s) & 0x1) << 2) \
)

/* Macros to handle the ISR register.  */
#define GET_ISR() GET_H_SPR (H_SPR_ISR)
#define SET_ISR(isr) SET_H_SPR (H_SPR_ISR, (isr))

#define GET_ISR_EDE(isr) (((isr) >> 5) & 1)

#define GET_ISR_DTT(isr) (((isr) >> 3) & 3)
#define SET_ISR_DTT(isr, dtt) (                        \
  (isr) = ((isr) & ~(0x3 << 3)) | (((dtt) & 0x3) << 3) \
)

#define SET_ISR_AEXC(isr) ((isr) |= (1 << 2))

#define GET_ISR_EMAM(isr) ((isr) & 1)

/* Macros to handle exception status registers.
   Get and set the hardware directly, since we may be getting/setting fields
   which are not accessible to the user.  */
#define GET_ESR(index) \
  (CPU (h_spr[H_SPR_ESR0 + (index)]))
#define SET_ESR(index, esr) \
  (CPU (h_spr[H_SPR_ESR0 + (index)]) = (esr))

#define SET_ESR_VALID(esr) ((esr) |= 1)
#define CLEAR_ESR_VALID(esr) ((esr) &= ~1)

#define SET_ESR_EC(esr, ec) (                           \
  (esr) = ((esr) & ~(0x1f << 1)) | (((ec) & 0x1f) << 1) \
)

#define SET_ESR_REC(esr, rec) (                        \
  (esr) = ((esr) & ~(0x3 << 6)) | (((rec) & 0x3) << 6) \
)

#define SET_ESR_IAEC(esr, iaec) (                       \
  (esr) = ((esr) & ~(0x1 << 8)) | (((iaec) & 0x1) << 8) \
)

#define SET_ESR_DAEC(esr, daec) (                       \
  (esr) = ((esr) & ~(0x1 << 9)) | (((daec) & 0x1) << 9) \
)

#define SET_ESR_EAV(esr) ((esr) |= (1 << 11))
#define CLEAR_ESR_EAV(esr) ((esr) &= ~(1 << 11))

#define GET_ESR_EDV(esr) (((esr) >> 12) & 1)
#define SET_ESR_EDV(esr) ((esr) |= (1 << 12))
#define CLEAR_ESR_EDV(esr) ((esr) &= ~(1 << 12))

#define GET_ESR_EDN(esr) ( \
  ((esr) >> 13) & 0xf      \
)
#define SET_ESR_EDN(esr, edn) (                       \
  (esr) = ((esr) & ~(0xf << 13)) | (((edn) & 0xf) << 13) \
)

#define SET_EPCR(index, address) \
  (CPU (h_spr[H_SPR_EPCR0 + (index)]) = (address))

#define SET_EAR(index, address) \
  (CPU (h_spr[H_SPR_EAR0 + (index)]) = (address))

#define SET_EDR(index, edr) \
  (CPU (h_spr[H_SPR_EDR0 + (index)]) = (edr))

#define GET_ESFR(index) \
  (CPU (h_spr[H_SPR_ESFR0 + (index)]))
#define SET_ESFR(index, esfr) \
  (CPU (h_spr[H_SPR_ESFR0 + (index)]) = (esfr))

#define GET_ESFR_FLAG(findex) ( \
  (findex) > 31 ? \
    ((CPU (h_spr[H_SPR_ESFR0]) >> ((findex)-32)) & 1) \
    : \
    ((CPU (h_spr[H_SPR_ESFR1]) >> (findex)) & 1) \
)
#define SET_ESFR_FLAG(findex) ( \
  (findex) > 31 ? \
    (CPU (h_spr[H_SPR_ESFR0]) = \
      (CPU (h_spr[H_SPR_ESFR0]) | (1 << ((findex)-32))) \
    ) : \
    (CPU (h_spr[H_SPR_ESFR1]) = \
      (CPU (h_spr[H_SPR_ESFR1]) | (1 << (findex))) \
    ) \
)

/* The FSR registers.
   Get and set the hardware directly, since we may be getting/setting fields
   which are not accessible to the user.  */
#define GET_FSR(index) \
  (CPU (h_spr[H_SPR_FSR0 + (index)]))
#define SET_FSR(index, fsr) \
  (CPU (h_spr[H_SPR_FSR0 + (index)]) = (fsr))

#define GET_FSR_TEM(fsr) ( \
  ((fsr) >> 24) & 0x3f     \
)

#define SET_FSR_QNE(fsr) ((fsr) |= (1 << 20))
#define GET_FSR_QNE(fsr) (((fsr) >> 20) & 1)

#define SET_FSR_FTT(fsr, ftt) (                          \
  (fsr) = ((fsr) & ~(0x7 << 17)) | (((ftt) & 0x7) << 17) \
)

#define GET_FSR_AEXC(fsr) ( \
  ((fsr) >> 10) & 0x3f      \
)
#define SET_FSR_AEXC(fsr, aexc) (                          \
  (fsr) = ((fsr) & ~(0x3f << 10)) | (((aexc) & 0x3f) << 10) \
)

/* SIMD instruction exception codes for FQ.  */
enum frv_sie
{
  SIE_NIL   = 0,
  SIE_FRi   = 1,
  SIE_FRi_1 = 2
};

/* MIV field of FQ.  */
enum frv_miv
{
  MIV_FLOAT = 0,
  MIV_MEDIA = 1
};

/* The FQ registers are 64 bits wide and are implemented as 32 bit pairs. The
   index here refers to the low order 32 bit element.
   Get and set the hardware directly, since we may be getting/setting fields
   which are not accessible to the user.  */
#define GET_FQ(index) \
  (CPU (h_spr[H_SPR_FQST0 + 2 * (index)]))
#define SET_FQ(index, fq) \
  (CPU (h_spr[H_SPR_FQST0 + 2 * (index)]) = (fq))

#define SET_FQ_MIV(fq, miv) (                          \
  (fq) = ((fq) & ~(0x1 << 31)) | (((miv) & 0x1) << 31) \
)

#define SET_FQ_SIE(fq, sie) (                          \
  (fq) = ((fq) & ~(0x3 << 15)) | (((sie) & 0x3) << 15) \
)

#define SET_FQ_FTT(fq, ftt) (                        \
  (fq) = ((fq) & ~(0x7 << 7)) | (((ftt) & 0x7) << 7) \
)

#define SET_FQ_CEXC(fq, cexc) (                         \
  (fq) = ((fq) & ~(0x3f << 1)) | (((cexc) & 0x3f) << 1) \
)

#define GET_FQ_VALID(fq) ((fq) & 1)
#define SET_FQ_VALID(fq) ((fq) |= 1)

#define SET_FQ_OPC(index, insn) \
  (CPU (h_spr[H_SPR_FQOP0 + 2 * (index)]) = (insn))

/* mp_exception support.  */
/* Media trap types for MSR.  */
enum frv_msr_mtt
{
  MTT_NONE                = 0,
  MTT_OVERFLOW            = 1,
  MTT_ACC_NOT_ALIGNED     = 2,
  MTT_ACC_NOT_IMPLEMENTED = 2, /* Yes -- same value as MTT_ACC_NOT_ALIGNED.  */
  MTT_CR_NOT_ALIGNED      = 3,
  MTT_UNIMPLEMENTED_MPOP  = 5,
  MTT_INVALID_FR          = 6
};

/* Media status registers.
   Get and set the hardware directly, since we may be getting/setting fields
   which are not accessible to the user.  */
#define GET_MSR(index) \
  (CPU (h_spr[H_SPR_MSR0 + (index)]))
#define SET_MSR(index, msr) \
  (CPU (h_spr[H_SPR_MSR0 + (index)]) = (msr))

#define GET_MSR_AOVF(msr) ((msr) & 1)
#define SET_MSR_AOVF(msr) ((msr) |=  1)

#define GET_MSR_OVF(msr) ( \
  ((msr) >> 1) & 0x1       \
)
#define SET_MSR_OVF(msr) ( \
  (msr) |= (1 << 1)        \
)
#define CLEAR_MSR_OVF(msr) ( \
  (msr) &= ~(1 << 1)         \
)

#define OR_MSR_SIE(msr, sie) (  \
  (msr) |= (((sie) & 0xf) << 2) \
)
#define CLEAR_MSR_SIE(msr) ( \
  (msr) &= ~(0xf << 2)       \
)

#define GET_MSR_MTT(msr) ( \
  ((msr) >> 12) & 0x7      \
)
#define SET_MSR_MTT(msr, mtt) (                          \
  (msr) = ((msr) & ~(0x7 << 12)) | (((mtt) & 0x7) << 12) \
)
#define GET_MSR_EMCI(msr) ( \
  ((msr) >> 24) & 0x1       \
)
#define GET_MSR_MPEM(msr) ( \
  ((msr) >> 27) & 0x1        \
)
#define GET_MSR_SRDAV(msr) ( \
  ((msr) >> 28) & 0x1        \
)
#define GET_MSR_RDAV(msr) ( \
  ((msr) >> 29) & 0x1       \
)
#define GET_MSR_RD(msr) ( \
  ((msr) >> 30) & 0x3     \
)

void frvbf_media_register_not_aligned (SIM_CPU *);
void frvbf_media_acc_not_aligned (SIM_CPU *);
void frvbf_media_cr_not_aligned (SIM_CPU *);
void frvbf_media_overflow (SIM_CPU *, int);

/* Functions for queuing and processing interrupts.  */
struct frv_interrupt_queue_element *
frv_queue_break_interrupt (SIM_CPU *);

struct frv_interrupt_queue_element *
frv_queue_software_interrupt (SIM_CPU *, SI);

struct frv_interrupt_queue_element *
frv_queue_program_interrupt (SIM_CPU *, enum frv_interrupt_kind);

struct frv_interrupt_queue_element *
frv_queue_external_interrupt (SIM_CPU *, enum frv_interrupt_kind);

struct frv_interrupt_queue_element *
frv_queue_illegal_instruction_interrupt (SIM_CPU *, const CGEN_INSN *);

struct frv_interrupt_queue_element *
frv_queue_privileged_instruction_interrupt (SIM_CPU *, const CGEN_INSN *);

struct frv_interrupt_queue_element *
frv_queue_float_disabled_interrupt (SIM_CPU *);

struct frv_interrupt_queue_element *
frv_queue_media_disabled_interrupt (SIM_CPU *);

struct frv_interrupt_queue_element *
frv_queue_non_implemented_instruction_interrupt (SIM_CPU *, const CGEN_INSN *);

struct frv_interrupt_queue_element *
frv_queue_register_exception_interrupt (SIM_CPU *, enum frv_rec);

struct frv_interrupt_queue_element *
frv_queue_mem_address_not_aligned_interrupt (SIM_CPU *, USI);

struct frv_interrupt_queue_element *
frv_queue_data_access_error_interrupt (SIM_CPU *, USI);

struct frv_interrupt_queue_element *
frv_queue_instruction_access_error_interrupt (SIM_CPU *);

struct frv_interrupt_queue_element *
frv_queue_instruction_access_exception_interrupt (SIM_CPU *);

struct frv_interrupt_queue_element *
frv_queue_fp_exception_interrupt (SIM_CPU *, struct frv_fp_exception_info *);

enum frv_dtt frvbf_division_exception (SIM_CPU *, enum frv_dtt, int, int);

struct frv_interrupt_queue_element *
frv_queue_interrupt (SIM_CPU *, enum frv_interrupt_kind);

void
frv_set_interrupt_queue_slot (SIM_CPU *, struct frv_interrupt_queue_element *);

void frv_set_mp_exception_registers (SIM_CPU *, enum frv_msr_mtt, int);
void frv_detect_insn_access_interrupts (SIM_CPU *, SCACHE *);

void frv_process_interrupts (SIM_CPU *);

void frv_break_interrupt (SIM_CPU *, struct frv_interrupt *, IADDR);
void frv_non_operating_interrupt (SIM_CPU *, enum frv_interrupt_kind, IADDR);
void frv_program_interrupt (
  SIM_CPU *, struct frv_interrupt_queue_element *, IADDR
);
void frv_software_interrupt (
  SIM_CPU *, struct frv_interrupt_queue_element *, IADDR
);
void frv_external_interrupt (
  SIM_CPU *, struct frv_interrupt_queue_element *, IADDR
);
void frv_program_or_software_interrupt (
  SIM_CPU *, struct frv_interrupt *, IADDR
);
void frv_clear_interrupt_classes (
  enum frv_interrupt_class, enum frv_interrupt_class
);

void
frv_save_data_written_for_interrupts (SIM_CPU *, CGEN_WRITE_QUEUE_ELEMENT *);

/* Special purpose traps.  */
#define TRAP_SYSCALL	0x80
#define TRAP_BREAKPOINT	0x81
#define TRAP_REGDUMP1	0x82
#define TRAP_REGDUMP2	0x83

/* Handle the trap insns  */
void frv_itrap (SIM_CPU *, PCADDR, USI, int);
void frv_mtrap (SIM_CPU *);
/* Handle the break insn.  */
void frv_break (SIM_CPU *);
/* Handle the rett insn.  */
USI frv_rett (SIM_CPU *current_cpu, PCADDR pc, BI debug_field);

/* Parallel write queue flags.  */
#define FRV_WRITE_QUEUE_FORCE_WRITE 1

#define CGEN_WRITE_QUEUE_ELEMENT_PIPE(element) CGEN_WRITE_QUEUE_ELEMENT_WORD1 (element)

/* Functions and macros for handling non-excepting instruction side effects.
   Get and set the hardware directly, since we may be getting/setting fields
   which are not accessible to the user.  */
#define GET_NECR()           (GET_H_SPR (H_SPR_NECR))
#define GET_NECR_ELOS(necr)  (((necr) >> 6) & 1)
#define GET_NECR_NEN(necr)   (((necr) >> 1) & 0x1f)
#define GET_NECR_VALID(necr) (((necr)     ) & 1)

#define NO_NESR    (-1)
/* NESR field values. See Tables 30-33 in section 4.4.2.1 of the FRV
   Architecture volume 1.  */
#define NESR_MEM_ADDRESS_NOT_ALIGNED 0x0b
#define NESR_REGISTER_NOT_ALIGNED    0x1
#define NESR_UQI_SIZE 0
#define NESR_QI_SIZE  1
#define NESR_UHI_SIZE 2
#define NESR_HI_SIZE  3
#define NESR_SI_SIZE  4
#define NESR_DI_SIZE  5
#define NESR_XI_SIZE  6

#define GET_NESR(index) GET_H_SPR (H_SPR_NESR0 + (index))
#define SET_NESR(index, value) (                          \
  sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set,    \
			 H_SPR_NESR0 + (index), (value)), \
  frvbf_force_update (current_cpu)                        \
)
#define GET_NESR_VALID(nesr) ((nesr) & 1)
#define SET_NESR_VALID(nesr) ((nesr) |= 1)

#define SET_NESR_EAV(nesr)   ((nesr) |= (1 << 31))

#define GET_NESR_FR(nesr)    (((nesr) >> 30) & 1)
#define SET_NESR_FR(nesr)    ((nesr) |= (1 << 30))
#define CLEAR_NESR_FR(nesr)  ((nesr) &= ~(1 << 30))

#define GET_NESR_DRN(nesr)   (((nesr) >> 24) & 0x3f)
#define SET_NESR_DRN(nesr, drn) (                            \
  (nesr) = ((nesr) & ~(0x3f << 24)) | (((drn) & 0x3f) << 24) \
)

#define SET_NESR_SIZE(nesr, data_size) (                         \
  (nesr) = ((nesr) & ~(0x7 << 21)) | (((data_size) & 0x7) << 21) \
)

#define SET_NESR_NEAN(nesr, index) (                           \
  (nesr) = ((nesr) & ~(0x1f << 10)) | (((index) & 0x1f) << 10) \
)

#define GET_NESR_DAEC(nesr) (((nesr) >> 9) & 1)
#define SET_NESR_DAEC(nesr, daec) (                   \
  (nesr) = ((nesr) & ~(1 << 9)) | (((daec) & 1) << 9) \
)

#define GET_NESR_REC(nesr) (((nesr) >> 6) & 3)
#define SET_NESR_REC(nesr, rec) (                    \
  (nesr) = ((nesr) & ~(3 << 6)) | (((rec) & 3) << 6) \
)

#define GET_NESR_EC(nesr) (((nesr) >> 1) & 0x1f)
#define SET_NESR_EC(nesr, ec) (                       \
  (nesr) = ((nesr) & ~(0x1f << 1)) | (((ec) & 0x1f) << 1) \
)

#define SET_NEEAR(index, address) ( \
  sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set,       \
			 H_SPR_NEEAR0 + (index), (address)), \
  frvbf_force_update (current_cpu)                           \
)

#define GET_NE_FLAGS(flags, NE_base) (   \
  (flags)[0] = GET_H_SPR ((NE_base)),    \
  (flags)[1] = GET_H_SPR ((NE_base) + 1) \
)
#define SET_NE_FLAGS(NE_base, flags) ( \
  sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, (NE_base),      \
                         (flags)[0]),                                  \
  frvbf_force_update (current_cpu),                                    \
  sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, (NE_base) + 1,  \
                         (flags)[1]),                                  \
  frvbf_force_update (current_cpu)                                     \
)

#define GET_NE_FLAG(flags, index) (    \
  (index) > 31 ?                       \
    ((flags[0] >> ((index) - 32)) & 1) \
  :                                    \
    ((flags[1] >> (index)) & 1)        \
)
#define SET_NE_FLAG(flags, index) (       \
  (index) > 31 ?                          \
    ((flags)[0] |= (1 << ((index) - 32))) \
  :                                       \
    ((flags)[1] |= (1 << (index)))        \
)
#define CLEAR_NE_FLAG(flags, index) (      \
  (index) > 31 ?                           \
    ((flags)[0] &= ~(1 << ((index) - 32))) \
  :                                        \
    ((flags)[1] &= ~(1 << (index)))        \
)

BI   frvbf_check_non_excepting_load (SIM_CPU *, SI, SI, SI, SI, QI, BI);
void frvbf_check_recovering_store (SIM_CPU *, PCADDR, SI, int, int);

void frvbf_clear_ne_flags (SIM_CPU *, SI, BI);
void frvbf_commit (SIM_CPU *, SI, BI);

void frvbf_fpu_error (CGEN_FPU *, int);

void frv_vliw_setup_insn (SIM_CPU *, const CGEN_INSN *);

extern int insns_in_slot[];

#define COUNT_INSNS_IN_SLOT(slot) \
{                                 \
  if (WITH_PROFILE_MODEL_P)       \
    ++insns_in_slot[slot];        \
}
    
#define INSNS_IN_SLOT(slot) (insns_in_slot[slot])
    
/* Multiple loads and stores.  */
void frvbf_load_multiple_GR (SIM_CPU *, PCADDR, SI, SI, int);
void frvbf_load_multiple_FRint (SIM_CPU *, PCADDR, SI, SI, int);
void frvbf_load_multiple_CPR (SIM_CPU *, PCADDR, SI, SI, int);
void frvbf_store_multiple_GR (SIM_CPU *, PCADDR, SI, SI, int);
void frvbf_store_multiple_FRint (SIM_CPU *, PCADDR, SI, SI, int);
void frvbf_store_multiple_CPR (SIM_CPU *, PCADDR, SI, SI, int);

/* Memory and cache support.  */
QI  frvbf_read_mem_QI  (SIM_CPU *, IADDR, SI);
UQI frvbf_read_mem_UQI (SIM_CPU *, IADDR, SI);
HI  frvbf_read_mem_HI  (SIM_CPU *, IADDR, SI);
UHI frvbf_read_mem_UHI (SIM_CPU *, IADDR, SI);
SI  frvbf_read_mem_SI  (SIM_CPU *, IADDR, SI);
SI  frvbf_read_mem_WI  (SIM_CPU *, IADDR, SI);
DI  frvbf_read_mem_DI  (SIM_CPU *, IADDR, SI);
DF  frvbf_read_mem_DF  (SIM_CPU *, IADDR, SI);

USI frvbf_read_imem_USI (SIM_CPU *, PCADDR);

void frvbf_write_mem_QI  (SIM_CPU *, IADDR, SI, QI);
void frvbf_write_mem_UQI (SIM_CPU *, IADDR, SI, UQI);
void frvbf_write_mem_HI  (SIM_CPU *, IADDR, SI, HI);
void frvbf_write_mem_UHI (SIM_CPU *, IADDR, SI, UHI);
void frvbf_write_mem_SI  (SIM_CPU *, IADDR, SI, SI);
void frvbf_write_mem_WI  (SIM_CPU *, IADDR, SI, SI);
void frvbf_write_mem_DI  (SIM_CPU *, IADDR, SI, DI);
void frvbf_write_mem_DF  (SIM_CPU *, IADDR, SI, DF);

void frvbf_mem_set_QI (SIM_CPU *, IADDR, SI, QI);
void frvbf_mem_set_HI (SIM_CPU *, IADDR, SI, HI);
void frvbf_mem_set_SI (SIM_CPU *, IADDR, SI, SI);
void frvbf_mem_set_DI (SIM_CPU *, IADDR, SI, DI);
void frvbf_mem_set_DF (SIM_CPU *, IADDR, SI, DF);
void frvbf_mem_set_XI (SIM_CPU *, IADDR, SI, SI *);

void frv_set_write_queue_slot (SIM_CPU *current_cpu);

/* FRV specific options.  */
extern const OPTION frv_options[];

#endif /* FRV_SIM_H */
