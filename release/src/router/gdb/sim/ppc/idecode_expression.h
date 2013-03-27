/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003 Andrew Cagney

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

/* Additional, and optional expressions.  */
#ifdef WITH_ALTIVEC
#include "altivec_expression.h"
#endif
#ifdef WITH_E500
#include "e500_expression.h"
#endif

/* 32bit target expressions:

   Each calculation is performed three times using each of the
   signed64, unsigned64 and long integer types.  The macro ALU_END
   (in _ALU_RESULT_VAL) then selects which of the three alternative
   results will be used in the final assignment of the target
   register.  As this selection is determined at compile time by
   fields in the instruction (OE, EA, Rc) the compiler has sufficient
   information to firstly simplify the selection code into a single
   case and then back anotate the equations and hence eliminate any
   resulting dead code.  That dead code being the calculations that,
   as it turned out were not in the end needed.

   64bit arrithemetic is used firstly because it allows the use of
   gcc's efficient long long operators (typically efficiently output
   inline) and secondly because the resultant answer will contain in
   the low 32bits the answer while in the high 32bits is either carry
   or status information. */

/* 64bit target expressions:

   Unfortunatly 128bit arrithemetic isn't that common.  Consequently
   the 32/64 bit trick can not be used.  Instead all calculations are
   required to retain carry/overflow information in separate
   variables.  Even with this restriction it is still possible for the
   trick of letting the compiler discard the calculation of unneeded
   values */


/* Macro's to type cast 32bit constants to 64bits */
#define SIGNED64(val)   ((signed64)(signed32)(val))
#define UNSIGNED64(val) ((unsigned64)(unsigned32)(val))


/* Start a section of ALU code */

#define ALU_BEGIN(val) \
{ \
  natural_word alu_val; \
  unsigned64 alu_carry_val; \
  signed64 alu_overflow_val; \
  ALU_SET(val)


/* assign the result to the target register */

#define ALU_END(TARG,CA,OE,Rc) \
{ /* select the result to use */ \
  signed_word const alu_result = _ALU_RESULT_VAL(CA,OE,Rc); \
  /* determine the overflow bit if needed */ \
  if (OE) { \
    if ((((unsigned64)(alu_overflow_val & BIT64(0))) \
	 >> 32) \
        == (alu_overflow_val & BIT64(32))) \
      XER &= (~xer_overflow); \
    else \
      XER |= (xer_summary_overflow | xer_overflow); \
  } \
  /* Update the carry bit if needed */ \
  if (CA) { \
    XER = ((XER & ~xer_carry) \
           | SHUFFLED32((alu_carry_val >> 32), 31, xer_carry_bit)); \
    /* if (alu_carry_val & BIT64(31)) \
         XER |= (xer_carry); \
       else \
         XER &= (~xer_carry); */ \
  } \
  TRACE(trace_alu, (" Result = %ld (0x%lx), XER = %ld\n", \
                    (long)alu_result, (long)alu_result, (long)XER)); \
  /* Update the Result Conditions if needed */ \
  CR0_COMPARE(alu_result, 0, Rc); \
  /* assign targ same */ \
  TARG = alu_result; \
}}

/* select the result from the different options */

#define _ALU_RESULT_VAL(CA,OE,Rc) (WITH_TARGET_WORD_BITSIZE == 64 \
				   ? alu_val \
				   : (OE \
				      ? alu_overflow_val \
				      : (CA \
					 ? alu_carry_val \
					 : alu_val)))


/* More basic alu operations */
#if (WITH_TARGET_WORD_BITSIZE == 64)
#define ALU_SET(val) \
do { \
  alu_val = val; \
  alu_carry_val = ((unsigned64)alu_val) >> 32; \
  alu_overflow_val = ((signed64)alu_val) >> 32; \
} while (0)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_SET(val) \
do { \
  alu_val = val; \
  alu_carry_val = (unsigned32)(alu_val); \
  alu_overflow_val = (signed32)(alu_val); \
} while (0)
#endif

#if (WITH_TARGET_WORD_BITSIZE == 64)
#define ALU_ADD(val) \
do { \
  unsigned64 alu_lo = (UNSIGNED64(alu_val) \
		       + UNSIGNED64(val)); \
  signed alu_carry = ((alu_lo & BIT(31)) != 0); \
  alu_carry_val = (alu_carry_val \
		   + UNSIGNED64(EXTRACTED(val, 0, 31)) \
		   + alu_carry); \
  alu_overflow_val = (alu_overflow_val \
		      + SIGNED64(EXTRACTED(val, 0, 31)) \
		      + alu_carry); \
  alu_val = alu_val + val; \
} while (0)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_ADD(val) \
do { \
  alu_val += val; \
  alu_carry_val += (unsigned32)(val); \
  alu_overflow_val += (signed32)(val); \
} while (0)
#endif


#if (WITH_TARGET_WORD_BITSIZE == 64)
#define ALU_ADD_CA \
do { \
  signed carry = MASKED32(XER, xer_carry_bit, xer_carry_bit) != 0; \
  ALU_ADD(carry); \
} while (0)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_ADD_CA \
do { \
  signed carry = MASKED32(XER, xer_carry_bit, xer_carry_bit) != 0; \
  ALU_ADD(carry); \
} while (0)
#endif


#if 0
#if (WITH_TARGET_WORD_BITSIZE == 64)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_SUB(val) \
do { \
  alu_val -= val; \
  alu_carry_val -= (unsigned32)(val); \
  alu_overflow_val -= (signed32)(val); \
} while (0)
#endif
#endif

#if (WITH_TARGET_WORD_BITSIZE == 64)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_OR(val) \
do { \
  alu_val |= val; \
  alu_carry_val = (unsigned32)(alu_val); \
  alu_overflow_val = (signed32)(alu_val); \
} while (0)
#endif


#if (WITH_TARGET_WORD_BITSIZE == 64)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_XOR(val) \
do { \
  alu_val ^= val; \
  alu_carry_val = (unsigned32)(alu_val); \
  alu_overflow_val = (signed32)(alu_val); \
} while (0)
#endif


#if 0
#if (WITH_TARGET_WORD_BITSIZE == 64)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_NEGATE \
do { \
  alu_val = -alu_val; \
  alu_carry_val = -alu_carry_val; \
  alu_overflow_val = -alu_overflow_val; \
} while(0)
#endif
#endif


#if (WITH_TARGET_WORD_BITSIZE == 64)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_AND(val) \
do { \
  alu_val &= val; \
  alu_carry_val = (unsigned32)(alu_val); \
  alu_overflow_val = (signed32)(alu_val); \
} while (0)
#endif


#if (WITH_TARGET_WORD_BITSIZE == 64)
#define ALU_NOT \
do { \
  signed64 new_alu_val = ~alu_val; \
  ALU_SET(new_alu_val); \
} while (0)
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define ALU_NOT \
do { \
  signed new_alu_val = ~alu_val; \
  ALU_SET(new_alu_val); \
} while(0)
#endif


/* Macros for updating the condition register */

#define CR1_UPDATE(Rc) \
do { \
  if (Rc) { \
    CR_SET(1, EXTRACTED32(FPSCR, fpscr_fx_bit, fpscr_ox_bit)); \
  } \
} while (0)


#define _DO_CR_COMPARE(LHS, RHS) \
(((LHS) < (RHS)) \
 ? cr_i_negative \
 : (((LHS) > (RHS)) \
    ? cr_i_positive \
    : cr_i_zero))

#define CR_SET(REG, VAL) MBLIT32(CR, REG*4, REG*4+3, VAL)
#define CR_FIELD(REG) EXTRACTED32(CR, REG*4, REG*4+3)
#define CR_SET_XER_SO(REG, VAL) \
do { \
  creg new_bits = ((XER & xer_summary_overflow) \
                   ? (cr_i_summary_overflow | VAL) \
                   : VAL); \
  CR_SET(REG, new_bits); \
} while(0)

#define CR_COMPARE(REG, LHS, RHS) \
do { \
  creg new_bits = ((XER & xer_summary_overflow) \
                   ? (cr_i_summary_overflow | _DO_CR_COMPARE(LHS,RHS)) \
                   : _DO_CR_COMPARE(LHS,RHS)); \
  CR_SET(REG, new_bits); \
} while (0)

#define CR0_COMPARE(LHS, RHS, Rc) \
do { \
  if (Rc) { \
    CR_COMPARE(0, LHS, RHS); \
    TRACE(trace_alu, \
	  ("CR=0x%08lx, LHS=%ld, RHS=%ld\n", \
	   (unsigned long)CR, (long)LHS, (long)RHS)); \
  } \
} while (0)



/* Bring data in from the cold */

#define MEM(SIGN, EA, NR_BYTES) \
((SIGN##_##NR_BYTES) vm_data_map_read_##NR_BYTES(cpu_data_map(processor), EA, \
						 processor, cia)) \

#define STORE(EA, NR_BYTES, VAL) \
do { \
  vm_data_map_write_##NR_BYTES(cpu_data_map(processor), EA, VAL, \
			       processor, cia); \
} while (0)



/* some FPSCR update macros. */

#define FPSCR_BEGIN \
{ \
  fpscreg old_fpscr UNUSED = FPSCR

#define FPSCR_END(Rc) { \
  /* always update VX */ \
  if ((FPSCR & fpscr_vx_bits)) \
    FPSCR |= fpscr_vx; \
  else \
    FPSCR &= ~fpscr_vx; \
  /* always update FEX */ \
  if (((FPSCR & fpscr_vx) && (FPSCR & fpscr_ve)) \
      || ((FPSCR & fpscr_ox) && (FPSCR & fpscr_oe)) \
      || ((FPSCR & fpscr_ux) && (FPSCR & fpscr_ue)) \
      || ((FPSCR & fpscr_zx) && (FPSCR & fpscr_ze)) \
      || ((FPSCR & fpscr_xx) && (FPSCR & fpscr_xe))) \
    FPSCR |= fpscr_fex; \
  else \
    FPSCR &= ~fpscr_fex; \
  CR1_UPDATE(Rc); \
  /* interrupt enabled? */ \
  if ((MSR & (msr_floating_point_exception_mode_0 \
              | msr_floating_point_exception_mode_1)) \
      && (FPSCR & fpscr_fex)) \
    program_interrupt(processor, cia, \
                      floating_point_enabled_program_interrupt); \
}}

#define FPSCR_SET(REG, VAL) MBLIT32(FPSCR, REG*4, REG*4+3, VAL)
#define FPSCR_FIELD(REG) EXTRACTED32(FPSCR, REG*4, REG*4+3)

#define FPSCR_SET_FPCC(VAL) MBLIT32(FPSCR, fpscr_fpcc_bit, fpscr_fpcc_bit+3, VAL)

/* Handle various exceptions */

#define FPSCR_OR_VX(VAL) \
do { \
  /* NOTE: VAL != 0 */ \
  FPSCR |= (VAL); \
  FPSCR |= fpscr_fx; \
} while (0)

#define FPSCR_SET_OX(COND) \
do { \
  if (COND) { \
    FPSCR |= fpscr_ox; \
    FPSCR |= fpscr_fx; \
  } \
  else \
    FPSCR &= ~fpscr_ox; \
} while (0)

#define FPSCR_SET_UX(COND) \
do { \
  if (COND) { \
    FPSCR |= fpscr_ux; \
    FPSCR |= fpscr_fx; \
  } \
  else \
    FPSCR &= ~fpscr_ux; \
} while (0)

#define FPSCR_SET_ZX(COND) \
do { \
  if (COND) { \
    FPSCR |= fpscr_zx; \
    FPSCR |= fpscr_fx; \
  } \
  else \
    FPSCR &= ~fpscr_zx; \
} while (0)

#define FPSCR_SET_XX(COND) \
do { \
  if (COND) { \
    FPSCR |= fpscr_xx; \
    FPSCR |= fpscr_fx; \
  } \
} while (0)

/* Note: code using SET_FI must also explicitly call SET_XX */

#define FPSCR_SET_FR(COND) do { \
  if (COND) \
    FPSCR |= fpscr_fr; \
  else \
    FPSCR &= ~fpscr_fr; \
} while (0)

#define FPSCR_SET_FI(COND) \
do { \
  if (COND) { \
    FPSCR |= fpscr_fi; \
  } \
  else \
    FPSCR &= ~fpscr_fi; \
} while (0)

#define FPSCR_SET_FPRF(VAL) \
do { \
  FPSCR = (FPSCR & ~fpscr_fprf) | (VAL); \
} while (0)
