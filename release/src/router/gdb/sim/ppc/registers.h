/*  This file is part of the program psim.

    Copyright 1994, 1997, 2003 Andrew Cagney

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _REGISTERS_H_
#define _REGISTERS_H_


/*
 * The PowerPC registers
 *
 */

/* FIXME:

   For the moment use macro's to determine if the E500 or Altivec
   registers should be included.  IGEN should instead of a :register:
   field to facilitate the specification and generation of per ISA
   registers.  */

#ifdef WITH_E500
#include "e500_registers.h"
#endif
#if WITH_ALTIVEC
#include "altivec_registers.h"
#endif

/**
 ** General Purpose Registers
 **/

typedef signed_word gpreg;



/**
 ** Floating Point Registers
 **/

typedef unsigned64 fpreg;



/**
 ** The condition register
 **
 **/

typedef unsigned32 creg;

/* The following sub bits are defined for the condition register */
enum {
  cr_i_negative = BIT4(0),
  cr_i_positive = BIT4(1),
  cr_i_zero = BIT4(2),
  cr_i_summary_overflow = BIT4(3),
#if 0
  /* cr0 - integer status */
  cr0_i_summary_overflow_bit = 3,
  cr0_i_negative = BIT32(0),
  cr0_i_positive = BIT32(1),
  cr0_i_zero = BIT32(2),
  cr0_i_summary_overflow = BIT32(3),
  cr0_i_mask = MASK32(0,3),
#endif
  /* cr1 - floating-point status */
  cr1_i_floating_point_exception_summary_bit = 4,
  cr1_i_floating_point_enabled_exception_summary_bit = 5,
  cr1_i_floating_point_invalid_operation_exception_summary_bit = 6,
  cr1_i_floating_point_overflow_exception_bit = 7,
  cr1_i_floating_point_exception_summary = BIT32(4),
  cr1_i_floating_point_enabled_exception_summary = BIT32(5),
  cr1_i_floating_point_invalid_operation_exception_summary = BIT32(6),
  cr1_i_floating_point_overflow_exception = BIT32(7),
  cr1_i_mask = MASK32(4,7),
};


/* Condition register 1 contains the result of floating point arithmetic */
enum {
  cr_fp_exception = BIT4(0),
  cr_fp_enabled_exception = BIT4(1),
  cr_fp_invalid_exception = BIT4(2),
  cr_fp_overflow_exception = BIT4(3),
};



/**
 ** Floating-Point Status and Control Register
 **/

typedef unsigned32 fpscreg;
enum {
  fpscr_fx_bit = 0,
  fpscr_fx = BIT32(0),
  fpscr_fex_bit = 1,
  fpscr_fex = BIT32(1),
  fpscr_vx_bit = 2,
  fpscr_vx = BIT32(2),
  fpscr_ox_bit = 3,
  fpscr_ox = BIT32(3),
  fpscr_ux = BIT32(4),
  fpscr_zx = BIT32(5),
  fpscr_xx = BIT32(6),
  fpscr_vxsnan = BIT32(7), /* SNAN */
  fpscr_vxisi = BIT32(8), /* INF - INF */
  fpscr_vxidi = BIT32(9), /* INF / INF */
  fpscr_vxzdz = BIT32(10), /* 0 / 0 */
  fpscr_vximz = BIT32(11), /* INF * 0 */
  fpscr_vxvc = BIT32(12),
  fpscr_fr = BIT32(13),
  fpscr_fi = BIT32(14),
  fpscr_fprf = MASK32(15, 19),
  fpscr_c = BIT32(15),
  fpscr_fpcc_bit = 16, /* well sort of */
  fpscr_fpcc = MASK32(16, 19),
  fpscr_fl = BIT32(16),
  fpscr_fg = BIT32(17),
  fpscr_fe = BIT32(18),
  fpscr_fu = BIT32(19),
  fpscr_rf_quiet_nan = fpscr_c | fpscr_fu,
  fpscr_rf_neg_infinity = fpscr_fl | fpscr_fu,
  fpscr_rf_neg_normal_number = fpscr_fl,
  fpscr_rf_neg_denormalized_number = fpscr_c | fpscr_fl,
  fpscr_rf_neg_zero = fpscr_c | fpscr_fe,
  fpscr_rf_pos_zero = fpscr_fe,
  fpscr_rf_pos_denormalized_number = fpscr_c | fpscr_fg,
  fpscr_rf_pos_normal_number = fpscr_fg,
  fpscr_rf_pos_infinity = fpscr_fg | fpscr_fu,
  fpscr_reserved_20 = BIT32(20),
  fpscr_vxsoft = BIT32(21),
  fpscr_vxsqrt = BIT32(22),
  fpscr_vxcvi = BIT32(23),
  fpscr_ve = BIT32(24),
  fpscr_oe = BIT32(25),
  fpscr_ue = BIT32(26),
  fpscr_ze = BIT32(27),
  fpscr_xe = BIT32(28),
  fpscr_ni = BIT32(29),
  fpscr_rn = MASK32(30, 31),
  fpscr_rn_round_to_nearest = 0,
  fpscr_rn_round_towards_zero = MASK32(31,31),
  fpscr_rn_round_towards_pos_infinity = MASK32(30,30),
  fpscr_rn_round_towards_neg_infinity = MASK32(30,31),
  fpscr_vx_bits = (fpscr_vxsnan | fpscr_vxisi | fpscr_vxidi
		   | fpscr_vxzdz | fpscr_vximz | fpscr_vxvc
		   | fpscr_vxsoft | fpscr_vxsqrt | fpscr_vxcvi),
};



/**
 ** XER Register
 **/

typedef unsigned32 xereg;

enum {
  xer_summary_overflow = BIT32(0), xer_summary_overflow_bit = 0,
  xer_carry = BIT32(2), xer_carry_bit = 2,
  xer_overflow = BIT32(1),
  xer_reserved_3_24 = MASK32(3,24),
  xer_byte_count_mask = MASK32(25,31)
};


/**
 ** SPR's
 **/

#include "spreg.h"


/**
 ** Segment Registers
 **/

typedef unsigned32 sreg;
enum {
  nr_of_srs = 16
};


/**
 ** Machine state register 
 **/
typedef unsigned_word msreg; /* 32 or 64 bits */

enum {
#if (WITH_TARGET_WORD_BITSIZE == 64)
  msr_64bit_mode = BIT(0),
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
  msr_64bit_mode = 0,
#endif
  msr_power_management_enable = BIT(45),
  msr_tempoary_gpr_remapping = BIT(46), /* 603 specific */
  msr_interrupt_little_endian_mode = BIT(47),
  msr_external_interrupt_enable = BIT(48),
  msr_problem_state = BIT(49),
  msr_floating_point_available = BIT(50),
  msr_machine_check_enable = BIT(51),
  msr_floating_point_exception_mode_0 = BIT(52),
  msr_single_step_trace_enable = BIT(53),
  msr_branch_trace_enable = BIT(54),
  msr_floating_point_exception_mode_1 = BIT(55),
  msr_interrupt_prefix = BIT(57),
  msr_instruction_relocate = BIT(58),
  msr_data_relocate = BIT(59),
  msr_recoverable_interrupt = BIT(62),
  msr_little_endian_mode = BIT(63)
};

enum {
  srr1_hash_table_or_ibat_miss = BIT(33),
  srr1_direct_store_error_exception = BIT(35),
  srr1_protection_violation = BIT(36),
  srr1_segment_table_miss = BIT(42),
  srr1_floating_point_enabled = BIT(43),
  srr1_illegal_instruction = BIT(44),
  srr1_priviliged_instruction = BIT(45),
  srr1_trap = BIT(46),
  srr1_subsequent_instruction = BIT(47)
};

/**
 ** storage interrupt registers
 **/

typedef enum {
  dsisr_direct_store_error_exception = BIT32(0),
  dsisr_hash_table_or_dbat_miss = BIT32(1),
  dsisr_protection_violation = BIT32(4),
  dsisr_earwax_violation = BIT32(5),
  dsisr_store_operation = BIT32(6),
  dsisr_segment_table_miss = BIT32(10),
  dsisr_earwax_disabled = BIT32(11)
} dsisr_status;



/**
 ** And the registers proper
 **/
typedef struct _registers {

  gpreg gpr[32];
  fpreg fpr[32];
  creg cr;
  fpscreg fpscr;

  /* Machine state register */
  msreg msr;

  /* Spr's */
  spreg spr[nr_of_sprs];

  /* Segment Registers */
  sreg sr[nr_of_srs];

#if WITH_ALTIVEC
  struct altivec_regs altivec;
#endif
#if WITH_E500
  struct e500_regs e500;
#endif

} registers;

/* dump out all the registers */

INLINE_REGISTERS\
(void) registers_dump
(registers *regs);


/* return information on a register based on name */

typedef enum {
  reg_invalid,
  reg_gpr, reg_fpr, reg_spr, reg_msr,
  reg_cr, reg_fpscr, reg_pc, reg_sr,
  reg_insns, reg_stalls, reg_cycles,
#ifdef WITH_ALTIVEC
  reg_vr, reg_vscr,
#endif
#ifdef WITH_E500
  reg_acc, reg_gprh, reg_evr,
#endif
  nr_register_types
} register_types;

typedef struct {
  register_types type;
  int index;
  int size;
} register_descriptions;

INLINE_REGISTERS\
(register_descriptions) register_description
(const char reg[]);


/* Special purpose registers by their more common names */

#define SPREG(N)	cpu_registers(processor)->spr[N]
#define XER             SPREG(spr_xer)
#define LR              SPREG(spr_lr)
#define CTR             SPREG(spr_ctr)
#define SRR0		SPREG(spr_srr0)
#define SRR1		SPREG(spr_srr1)
#define DAR		SPREG(spr_dar)
#define DSISR		SPREG(spr_dsisr)

/* general purpose registers - indexed access */
#define GPR(N)          cpu_registers(processor)->gpr[N]

/* segment registers */
#define SEGREG(N)       cpu_registers(processor)->sr[N]

/* condition register */
#define CR              cpu_registers(processor)->cr

/* machine status register */
#define MSR        	cpu_registers(processor)->msr

/* floating-point status condition register */
#define FPSCR		cpu_registers(processor)->fpscr

#endif /* _REGISTERS_H_ */
