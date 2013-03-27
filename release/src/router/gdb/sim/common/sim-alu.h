/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

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


#ifndef _SIM_ALU_H_
#define _SIM_ALU_H_

#include "symcat.h"


/* INTEGER ALU MODULE:

   This module provides an implementation of 2's complement arithmetic
   including the recording of carry and overflow status bits.


   EXAMPLE:

   Code using this module includes it into sim-main.h and then, as a
   convention, defines macro's ALU*_END that records the result of any
   arithmetic performed.  Ex:

   	#include "sim-alu.h"
	#define ALU32_END(RES) \
	(RES) = ALU32_OVERFLOW_RESULT; \
	carry = ALU32_HAD_CARRY_BORROW; \
	overflow = ALU32_HAD_OVERFLOW

   The macro's are then used vis:

        {
	  ALU32_BEGIN (GPR[i]);
	  ALU32_ADDC (GPR[j]);
	  ALU32_END (GPR[k]);
	}


   NOTES:

   Macros exist for efficiently computing 8, 16, 32 and 64 bit
   arithmetic - ALU8_*, ALU16_*, ....  In addition, according to
   TARGET_WORD_BITSIZE a set of short-hand macros are defined - ALU_*

   Initialization:

	ALU*_BEGIN(ACC): Declare initialize the ALU accumulator with ACC.

   Results:

        The calculation of the final result may be computed a number
        of different ways.  Three different overflow macro's are
        defined, the most efficient one to use depends on which other
        outputs from the alu are being used.

	ALU*_RESULT: Generic ALU result output.

   	ALU*_HAD_OVERFLOW: Returns a nonzero value if signed overflow
   	occurred.

	ALU*_OVERFLOW_RESULT: If the macro ALU*_HAD_OVERFLOW is being
	used this is the most efficient result available.  Ex:

		#define ALU16_END(RES) \
		if (ALU16_HAD_OVERFLOW) \
		  sim_engine_halt (...); \
		(RES) = ALU16_OVERFLOW_RESULT
   
   	ALU*_HAD_CARRY_BORROW: Returns a nonzero value if unsigned
   	overflow or underflow (also referred to as carry and borrow)
   	occurred.

	ALU*_CARRY_BORROW_RESULT: If the macro ALU*_HAD_CARRY_BORROW is being
	used this is the most efficient result available.  Ex:

		#define ALU64_END(RES) \
		State.carry = ALU64_HAD_CARRY_BORROW; \
		(RES) = ALU64_CARRY_BORROW_RESULT
   
   
   Addition:

	ALU*_ADD(VAL): Add VAL to the ALU accumulator.  Record any
	overflow as well as the final result.

	ALU*_ADDC(VAL): Add VAL to the ALU accumulator.  Record any
	carry-out or overflow as well as the final result.

	ALU*_ADDC_C(VAL,CI): Add VAL and CI (carry-in).  Record any
	carry-out or overflow as well as the final result.

   Subtraction:

	ALU*_SUB(VAL): Subtract VAL from the ALU accumulator.  Record
	any underflow as well as the final result.

	ALU*_SUBC(VAL): Subtract VAL from the ALU accumulator using
	negated addition.  Record any underflow or carry-out as well
	as the final result.

	ALU*_SUBB(VAL): Subtract VAL from the ALU accumulator using
	direct subtraction (ACC+~VAL+1).  Record any underflow or
	borrow-out as well as the final result.

	ALU*_SUBC_X(VAL,CI): Subtract VAL and CI (carry-in) from the
	ALU accumulator using extended negated addition (ACC+~VAL+CI).
	Record any underflow or carry-out as well as the final result.

	ALU*_SUBB_B(VAL,BI): Subtract VAL and BI (borrow-in) from the
	ALU accumulator using direct subtraction.  Record any
	underflow or borrow-out as well as the final result.


 */



/* Twos complement arithmetic - addition/subtraction - carry/borrow
   (or you thought you knew the answer to 0-0)

   

   Notation and Properties:


   Xn denotes the value X stored in N bits.

   MSBn (X): The most significant (sign) bit of X treated as an N bit
   value.

   SEXTn (X): The infinite sign extension of X treated as an N bit
   value.

   MAXn, MINn: The upper and lower bound of a signed, two's
   complement N bit value.

   UMAXn: The upper bound of an unsigned N bit value (the lower
   bound is always zero).

   Un: UMAXn + 1.  Unsigned arithmetic is computed `modulo (Un)'.  

   X[p]: Is bit P of X.  X[0] denotes the least significant bit.

   ~X[p]: Is the inversion of bit X[p]. Also equal to 1-X[p],
   (1+X[p])mod(2).



   Addition - Overflow - Introduction:


   Overflow/Overflow indicates an error in computation of signed
   arithmetic.  i.e. given X,Y in [MINn..MAXn]; overflow
   indicates that the result X+Y > MAXn or X+Y < MIN_INTx.

   Hardware traditionally implements overflow by computing the XOR of
   carry-in/carry-out of the most significant bit of the ALU. Here
   other methods need to be found.



   Addition - Overflow - method 1:


   Overflow occurs when the sign (most significant bit) of the two N
   bit operands is identical but different to the sign of the result:

                Rn = (Xn + Yn)
		V = MSBn (~(Xn ^ Yn) & (Rn ^ Xn))



   Addition - Overflow - method 2:


   The two N bit operands are sign extended to M>N bits and then
   added.  Overflow occurs when SIGN_BIT<n> and SIGN_BIT<m> do not
   match.
  
   		Rm = (SEXTn (Xn) + SEXTn (Yn))
		V = MSBn ((Rm >> (M - N)) ^ Rm)



   Addition - Overflow - method 3:


   The two N bit operands are sign extended to M>N bits and then
   added.  Overflow occurs when the result is outside of the sign
   extended range [MINn .. MAXn].



   Addition - Overflow - method 4:


   Given the Result and Carry-out bits, the oVerflow from the addition
   of X, Y and carry-In can be computed using the equation:

                Rn = (Xn + Yn)
		V = (MSBn ((Xn ^ Yn) ^ Rn)) ^ C)

   As shown in the table below:

         I  X  Y  R  C | V | X^Y  ^R  ^C
        ---------------+---+-------------
         0  0  0  0  0 | 0 |  0    0   0
         0  0  1  1  0 | 0 |  1    0   0
         0  1  0  1  0 | 0 |  1    0   0
         0  1  1  0  1 | 1 |  0    0   1
         1  0  0  1  0 | 1 |  0    1   1
         1  0  1  0  1 | 0 |  1    1   0
         1  1  0  0  1 | 0 |  1    1   0
         1  1  1  1  1 | 0 |  0    1   0



   Addition - Carry - Introduction:


   Carry (poorly named) indicates that an overflow occurred for
   unsigned N bit addition.  i.e. given X, Y in [0..UMAXn] then
   carry indicates X+Y > UMAXn or X+Y >= Un.

   The following table lists the output for all given inputs into a
   full-adder.
  
         I  X  Y  R | C
        ------------+---
         0  0  0  0 | 0
         0  0  1  1 | 0
         0  1  0  1 | 0
         0  1  1  0 | 1
         1  0  0  1 | 0
         1  0  1  0 | 1
         1  1  0  0 | 1
         1  1  1  1 | 1

   (carry-In, X, Y, Result, Carry-out):



   Addition - Carry - method 1:


   Looking at the terms X, Y and R we want an equation for C.

       XY\R  0  1
          +-------
       00 |  0  0 
       01 |  1  0
       11 |  1  1
       10 |  1  0

   This giving us the sum-of-prod equation:

		MSBn ((Xn & Yn) | (Xn & ~Rn) | (Yn & ~Rn))

   Verifying:

         I  X  Y  R | C | X&Y  X&~R Y&~R 
        ------------+---+---------------
         0  0  0  0 | 0 |  0    0    0
         0  0  1  1 | 0 |  0    0    0
         0  1  0  1 | 0 |  0    0    0
         0  1  1  0 | 1 |  1    1    1
         1  0  0  1 | 0 |  0    0    0
         1  0  1  0 | 1 |  0    0    1
         1  1  0  0 | 1 |  0    1    0
         1  1  1  1 | 1 |  1    0    0



   Addition - Carry - method 2:


   Given two signed N bit numbers, a carry can be detected by treating
   the numbers as N bit unsigned and adding them using M>N unsigned
   arithmetic.  Carry is indicated by bit (1 << N) being set (result
   >= 2**N).



   Addition - Carry - method 3:


   Given the oVerflow bit.  The carry can be computed from:

		(~R&V) | (R&V)



   Addition - Carry - method 4:

   Given two signed numbers.  Treating them as unsigned we have:

		0 <= X < Un, 0 <= Y < Un
	==>	X + Y < 2 Un

   Consider Y when carry occurs:

		X + Y >= Un, Y < Un
	==>	(Un - X) <= Y < Un               # rearrange
	==>	Un <= X + Y < Un + X < 2 Un      # add Xn
	==>	0 <= (X + Y) mod Un < X mod Un

   or when carry as occurred:

               (X + Y) mod Un < X mod Un

   Consider Y when carry does not occur:

		X + Y < Un
	have	X < Un, Y >= 0
	==>	X <= X + Y < Un
	==>     X mod Un <= (X + Y) mod Un

   or when carry has not occurred:

	        ! ( (X + Y) mod Un < X mod Un)

   hence we get carry by computing in N bit unsigned arithmetic.

                carry <- (Xn + Yn) < Xn



   Subtraction - Introduction


   There are two different ways of computing the signed two's
   complement difference of two numbers.  The first is based on
   negative addition, the second on direct subtraction.



   Subtraction - Carry - Introduction - Negated Addition


   The equation X - Y can be computed using:

   		X + (-Y)
	==>	X + ~Y + 1		# -Y = ~Y + 1

   In addition to the result, the equation produces Carry-out.  For
   succeeding extended precision calculations, the more general
   equation can be used:

		C[p]:R[p]  =  X[p] + ~Y[p] + C[p-1]
 	where	C[0]:R[0]  =  X[0] + ~Y[0] + 1



   Subtraction - Borrow - Introduction - Direct Subtraction


   The alternative to negative addition is direct subtraction where
   `X-Y is computed directly.  In addition to the result of the
   calculation, a Borrow bit is produced.  In general terms:

		B[p]:R[p]  =  X[p] - Y[p] - B[p-1]
	where	B[0]:R[0]  =  X[0] - Y[0]

   The Borrow bit is the complement of the Carry bit produced by
   Negated Addition above.  A dodgy proof follows:

   	Case 0:
		C[0]:R[0] = X[0] + ~Y[0] + 1
	==>	C[0]:R[0] = X[0] + 1 - Y[0] + 1	# ~Y[0] = (1 - Y[0])?
	==>	C[0]:R[0] = 2 + X[0] - Y[0]
	==>	C[0]:R[0] = 2 + B[0]:R[0]
	==>	C[0]:R[0] = (1 + B[0]):R[0]
	==>	C[0] = ~B[0]			# (1 + B[0]) mod 2 = ~B[0]?

	Case P:
		C[p]:R[p] = X[p] + ~Y[p] + C[p-1]
	==>	C[p]:R[p] = X[p] + 1 - Y[0] + 1 - B[p-1]
	==>	C[p]:R[p] = 2 + X[p] - Y[0] - B[p-1]
	==>	C[p]:R[p] = 2 + B[p]:R[p]
	==>	C[p]:R[p] = (1 + B[p]):R[p]
	==>     C[p] = ~B[p]

   The table below lists all possible inputs/outputs for a
   full-subtractor:

   	X  Y  I  |  R  B
	0  0  0  |  0  0
	0  0  1  |  1  1
	0  1  0  |  1  1
	0  1  1  |  0  1
	1  0  0  |  1  0
	1  0  1  |  0  0
	1  1  0  |  0  0
	1  1  1  |  1  1



   Subtraction - Method 1


   Treating Xn and Yn as unsigned values then a borrow (unsigned
   underflow) occurs when:

		B = Xn < Yn
	==>	C = Xn >= Yn

 */



/* 8 bit target expressions:

   Since the host's natural bitsize > 8 bits, carry method 2 and
   overflow method 2 are used. */

#define ALU8_BEGIN(VAL) \
unsigned alu8_cr = (unsigned8) (VAL); \
signed alu8_vr = (signed8) (alu8_cr)

#define ALU8_SET(VAL) \
alu8_cr = (unsigned8) (VAL); \
alu8_vr = (signed8) (alu8_cr)

#define ALU8_SET_CARRY_BORROW(CARRY)					\
do {									\
  if (CARRY)								\
    alu8_cr |= ((signed)-1) << 8;					\
  else									\
    alu8_cr &= 0xff;							\
} while (0)

#define ALU8_HAD_CARRY_BORROW (alu8_cr & LSBIT32(8))
#define ALU8_HAD_OVERFLOW (((alu8_vr >> 8) ^ alu8_vr) & LSBIT32 (8-1))

#define ALU8_RESULT ((unsigned8) alu8_cr)
#define ALU8_CARRY_BORROW_RESULT ((unsigned8) alu8_cr)
#define ALU8_OVERFLOW_RESULT ((unsigned8) alu8_vr)

/* #define ALU8_END ????? - target dependant */



/* 16 bit target expressions:

   Since the host's natural bitsize > 16 bits, carry method 2 and
   overflow method 2 are used. */

#define ALU16_BEGIN(VAL) \
signed alu16_cr = (unsigned16) (VAL); \
unsigned alu16_vr = (signed16) (alu16_cr)

#define ALU16_SET(VAL) \
alu16_cr = (unsigned16) (VAL); \
alu16_vr = (signed16) (alu16_cr)

#define ALU16_SET_CARRY_BORROW(CARRY)					\
do {									\
  if (CARRY)								\
    alu16_cr |= ((signed)-1) << 16;					\
  else									\
    alu16_cr &= 0xffff;							\
} while (0)

#define ALU16_HAD_CARRY_BORROW (alu16_cr & LSBIT32(16))
#define ALU16_HAD_OVERFLOW (((alu16_vr >> 16) ^ alu16_vr) & LSBIT32 (16-1))

#define ALU16_RESULT ((unsigned16) alu16_cr)
#define ALU16_CARRY_BORROW_RESULT ((unsigned16) alu16_cr)
#define ALU16_OVERFLOW_RESULT ((unsigned16) alu16_vr)

/* #define ALU16_END ????? - target dependant */



/* 32 bit target expressions:

   Since most hosts do not support 64 (> 32) bit arithmetic, carry
   method 4 and overflow method 4 are used. */

#define ALU32_BEGIN(VAL) \
unsigned32 alu32_r = (VAL); \
int alu32_c = 0; \
int alu32_v = 0

#define ALU32_SET(VAL) \
alu32_r = (VAL); \
alu32_c = 0; \
alu32_v = 0

#define ALU32_SET_CARRY_BORROW(CARRY) alu32_c = (CARRY)

#define ALU32_HAD_CARRY_BORROW (alu32_c)
#define ALU32_HAD_OVERFLOW (alu32_v)

#define ALU32_RESULT (alu32_r)
#define ALU32_CARRY_BORROW_RESULT (alu32_r)
#define ALU32_OVERFLOW_RESULT (alu32_r)



/* 64 bit target expressions:

   Even though the host typically doesn't support native 64 bit
   arithmetic, it is still used. */

#define ALU64_BEGIN(VAL) \
unsigned64 alu64_r = (VAL); \
int alu64_c = 0; \
int alu64_v = 0

#define ALU64_SET(VAL) \
alu64_r = (VAL); \
alu64_c = 0; \
alu64_v = 0

#define ALU64_SET_CARRY_BORROW(CARRY) alu64_c = (CARRY)

#define ALU64_HAD_CARRY_BORROW (alu64_c)
#define ALU64_HAD_OVERFLOW (alu64_v)

#define ALU64_RESULT (alu64_r)
#define ALU64_CARRY_BORROW_RESULT (alu64_r)
#define ALU64_OVERFLOW_RESULT (alu64_r)



/* Generic versions of above macros */

#define ALU_BEGIN	    XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_BEGIN)
#define ALU_SET		    XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SET)
#define ALU_SET_CARRY	    XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SET_CARRY)

#define ALU_HAD_OVERFLOW    XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_HAD_OVERFLOW)
#define ALU_HAD_CARRY       XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_HAD_CARRY)

#define ALU_RESULT          XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_RESULT)
#define ALU_OVERFLOW_RESULT XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_OVERFLOW_RESULT)
#define ALU_CARRY_RESULT    XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_CARRY_RESULT)



/* Basic operation - add (overflowing) */

#define ALU8_ADD(VAL)							\
do {									\
  unsigned8 alu8add_val = (VAL);					\
  ALU8_ADDC (alu8add_val);						\
} while (0)

#define ALU16_ADD(VAL)							\
do {									\
  unsigned16 alu16add_val = (VAL);					\
  ALU16_ADDC (alu8add_val);						\
} while (0)

#define ALU32_ADD(VAL)							\
do {									\
  unsigned32 alu32add_val = (VAL);					\
  ALU32_ADDC (alu32add_val);						\
} while (0)

#define ALU64_ADD(VAL)							\
do {									\
  unsigned64 alu64add_val = (unsigned64) (VAL);				\
  ALU64_ADDC (alu64add_val);						\
} while (0)

#define ALU_ADD XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_ADD)



/* Basic operation - add carrying (and overflowing) */

#define ALU8_ADDC(VAL)							\
do {									\
  unsigned8 alu8addc_val = (VAL);					\
  alu8_cr += (unsigned8)(alu8addc_val);					\
  alu8_vr += (signed8)(alu8addc_val);					\
} while (0)

#define ALU16_ADDC(VAL)							\
do {									\
  unsigned16 alu16addc_val = (VAL);					\
  alu16_cr += (unsigned16)(alu16addc_val);				\
  alu16_vr += (signed16)(alu16addc_val);				\
} while (0)

#define ALU32_ADDC(VAL)							\
do {									\
  unsigned32 alu32addc_val = (VAL);					\
  unsigned32 alu32addc_sign = alu32addc_val ^ alu32_r;			\
  alu32_r += (alu32addc_val);						\
  alu32_c = (alu32_r < alu32addc_val);					\
  alu32_v = ((alu32addc_sign ^ - (unsigned32)alu32_c) ^ alu32_r) >> 31;	\
} while (0)

#define ALU64_ADDC(VAL)							\
do {									\
  unsigned64 alu64addc_val = (unsigned64) (VAL);			\
  unsigned64 alu64addc_sign = alu64addc_val ^ alu64_r;			\
  alu64_r += (alu64addc_val);						\
  alu64_c = (alu64_r < alu64addc_val);					\
  alu64_v = ((alu64addc_sign ^ - (unsigned64)alu64_c) ^ alu64_r) >> 63;	\
} while (0)

#define ALU_ADDC XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_ADDC)



/* Compound operation - add carrying (and overflowing) with carry-in */

#define ALU8_ADDC_C(VAL,C)						\
do {									\
  unsigned8 alu8addcc_val = (VAL);					\
  unsigned8 alu8addcc_c = (C);						\
  alu8_cr += (unsigned)(unsigned8)alu8addcc_val + alu8addcc_c;		\
  alu8_vr += (signed)(signed8)(alu8addcc_val) + alu8addcc_c;		\
} while (0)

#define ALU16_ADDC_C(VAL,C)						\
do {									\
  unsigned16 alu16addcc_val = (VAL);					\
  unsigned16 alu16addcc_c = (C);					\
  alu16_cr += (unsigned)(unsigned16)alu16addcc_val + alu16addcc_c;	\
  alu16_vr += (signed)(signed16)(alu16addcc_val) + alu16addcc_c;	\
} while (0)

#define ALU32_ADDC_C(VAL,C)						\
do {									\
  unsigned32 alu32addcc_val = (VAL);					\
  unsigned32 alu32addcc_c = (C);					\
  unsigned32 alu32addcc_sign = (alu32addcc_val ^ alu32_r);		\
  alu32_r += (alu32addcc_val + alu32addcc_c);				\
  alu32_c = ((alu32_r < alu32addcc_val)					\
             || (alu32addcc_c && alu32_r == alu32addcc_val));		\
  alu32_v = ((alu32addcc_sign ^ - (unsigned32)alu32_c) ^ alu32_r) >> 31;\
} while (0)

#define ALU64_ADDC_C(VAL,C)						\
do {									\
  unsigned64 alu64addcc_val = (VAL);					\
  unsigned64 alu64addcc_c = (C);					\
  unsigned64 alu64addcc_sign = (alu64addcc_val ^ alu64_r);		\
  alu64_r += (alu64addcc_val + alu64addcc_c);				\
  alu64_c = ((alu64_r < alu64addcc_val)					\
             || (alu64addcc_c && alu64_r == alu64addcc_val));		\
  alu64_v = ((alu64addcc_sign ^ - (unsigned64)alu64_c) ^ alu64_r) >> 63;\
} while (0)

#define ALU_ADDC_C XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_ADDC_C)



/* Basic operation - subtract (overflowing) */

#define ALU8_SUB(VAL)							\
do {									\
  unsigned8 alu8sub_val = (VAL);					\
  ALU8_ADDC_C (~alu8sub_val, 1);					\
} while (0)

#define ALU16_SUB(VAL)							\
do {									\
  unsigned16 alu16sub_val = (VAL);					\
  ALU16_ADDC_C (~alu16sub_val, 1);					\
} while (0)

#define ALU32_SUB(VAL)							\
do {									\
  unsigned32 alu32sub_val = (VAL);					\
  ALU32_ADDC_C (~alu32sub_val, 1);					\
} while (0)

#define ALU64_SUB(VAL)							\
do {									\
  unsigned64 alu64sub_val = (VAL);					\
  ALU64_ADDC_C (~alu64sub_val, 1);					\
} while (0)

#define ALU_SUB XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SUB)



/* Basic operation - subtract carrying (and overflowing) */

#define ALU8_SUBC(VAL)							\
do {									\
  unsigned8 alu8subc_val = (VAL);					\
  ALU8_ADDC_C (~alu8subc_val, 1);					\
} while (0)

#define ALU16_SUBC(VAL)							\
do {									\
  unsigned16 alu16subc_val = (VAL);					\
  ALU16_ADDC_C (~alu16subc_val, 1);					\
} while (0)

#define ALU32_SUBC(VAL)							\
do {									\
  unsigned32 alu32subc_val = (VAL);					\
  ALU32_ADDC_C (~alu32subc_val, 1);					\
} while (0)

#define ALU64_SUBC(VAL)							\
do {									\
  unsigned64 alu64subc_val = (VAL);					\
  ALU64_ADDC_C (~alu64subc_val, 1);					\
} while (0)

#define ALU_SUBC XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SUBC)



/* Compound operation - subtract carrying (and overflowing), extended */

#define ALU8_SUBC_X(VAL,C)						\
do {									\
  unsigned8 alu8subcx_val = (VAL);					\
  unsigned8 alu8subcx_c = (C);						\
  ALU8_ADDC_C (~alu8subcx_val, alu8subcx_c);				\
} while (0)

#define ALU16_SUBC_X(VAL,C)						\
do {									\
  unsigned16 alu16subcx_val = (VAL);					\
  unsigned16 alu16subcx_c = (C);					\
  ALU16_ADDC_C (~alu16subcx_val, alu16subcx_c);				\
} while (0)

#define ALU32_SUBC_X(VAL,C)						\
do {									\
  unsigned32 alu32subcx_val = (VAL);					\
  unsigned32 alu32subcx_c = (C);					\
  ALU32_ADDC_C (~alu32subcx_val, alu32subcx_c);				\
} while (0)

#define ALU64_SUBC_X(VAL,C)						\
do {									\
  unsigned64 alu64subcx_val = (VAL);					\
  unsigned64 alu64subcx_c = (C);					\
  ALU64_ADDC_C (~alu64subcx_val, alu64subcx_c);				\
} while (0)

#define ALU_SUBC_X XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SUBC_X)



/* Basic operation - subtract borrowing (and overflowing) */

#define ALU8_SUBB(VAL)							\
do {									\
  unsigned8 alu8subb_val = (VAL);					\
  alu8_cr -= (unsigned)(unsigned8)alu8subb_val;				\
  alu8_vr -= (signed)(signed8)alu8subb_val;				\
} while (0)

#define ALU16_SUBB(VAL)							\
do {									\
  unsigned16 alu16subb_val = (VAL);					\
  alu16_cr -= (unsigned)(unsigned16)alu16subb_val;			\
  alu16_vr -= (signed)(signed16)alu16subb_val;				\
} while (0)

#define ALU32_SUBB(VAL)							\
do {									\
  unsigned32 alu32subb_val = (VAL);					\
  unsigned32 alu32subb_sign = alu32subb_val ^ alu32_r;			\
  alu32_c = (alu32_r < alu32subb_val);					\
  alu32_r -= (alu32subb_val);						\
  alu32_v = ((alu32subb_sign ^ - (unsigned32)alu32_c) ^ alu32_r) >> 31;	\
} while (0)

#define ALU64_SUBB(VAL)							\
do {									\
  unsigned64 alu64subb_val = (VAL);					\
  unsigned64 alu64subb_sign = alu64subb_val ^ alu64_r;			\
  alu64_c = (alu64_r < alu64subb_val);					\
  alu64_r -= (alu64subb_val);						\
  alu64_v = ((alu64subb_sign ^ - (unsigned64)alu64_c) ^ alu64_r) >> 31;	\
} while (0)

#define ALU_SUBB XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SUBB)



/* Compound operation - subtract borrowing (and overflowing) with borrow-in */

#define ALU8_SUBB_B(VAL,B)						\
do {									\
  unsigned8 alu8subbb_val = (VAL);					\
  unsigned8 alu8subbb_b = (B);						\
  alu8_cr -= (unsigned)(unsigned8)alu8subbb_val;			\
  alu8_cr -= (unsigned)(unsigned8)alu8subbb_b;				\
  alu8_vr -= (signed)(signed8)alu8subbb_val + alu8subbb_b;		\
} while (0)

#define ALU16_SUBB_B(VAL,B)						\
do {									\
  unsigned16 alu16subbb_val = (VAL);					\
  unsigned16 alu16subbb_b = (B);					\
  alu16_cr -= (unsigned)(unsigned16)alu16subbb_val;			\
  alu16_cr -= (unsigned)(unsigned16)alu16subbb_b;			\
  alu16_vr -= (signed)(signed16)alu16subbb_val + alu16subbb_b;		\
} while (0)

#define ALU32_SUBB_B(VAL,B)						\
do {									\
  unsigned32 alu32subbb_val = (VAL);					\
  unsigned32 alu32subbb_b = (B);					\
  ALU32_ADDC_C (~alu32subbb_val, !alu32subbb_b);			\
  alu32_c = !alu32_c;							\
} while (0)

#define ALU64_SUBB_B(VAL,B)						\
do {									\
  unsigned64 alu64subbb_val = (VAL);					\
  unsigned64 alu64subbb_b = (B);					\
  ALU64_ADDC_C (~alu64subbb_val, !alu64subbb_b);			\
  alu64_c = !alu64_c;							\
} while (0)

#define ALU_SUBB_B XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_SUBB_B)



/* Basic operation - negate (overflowing) */

#define ALU8_NEG()							\
do {									\
  signed alu8neg_val = (ALU8_RESULT);					\
  ALU8_SET (1);								\
  ALU8_ADDC (~alu8neg_val);						\
} while (0)

#define ALU16_NEG()							\
do {									\
  signed alu16neg_val = (ALU16_RESULT);				\
  ALU16_SET (1);							\
  ALU16_ADDC (~alu16neg_val);						\
} while (0)

#define ALU32_NEG()							\
do {									\
  unsigned32 alu32neg_val = (ALU32_RESULT);				\
  ALU32_SET (1);							\
  ALU32_ADDC (~alu32neg_val);						\
} while(0)

#define ALU64_NEG()							\
do {									\
  unsigned64 alu64neg_val = (ALU64_RESULT);				\
  ALU64_SET (1);							\
  ALU64_ADDC (~alu64neg_val);						\
} while (0)

#define ALU_NEG XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_NEG)




/* Basic operation - negate carrying (and overflowing) */

#define ALU8_NEGC()							\
do {									\
  signed alu8negc_val = (ALU8_RESULT);					\
  ALU8_SET (1);								\
  ALU8_ADDC (~alu8negc_val);						\
} while (0)

#define ALU16_NEGC()							\
do {									\
  signed alu16negc_val = (ALU16_RESULT);				\
  ALU16_SET (1);							\
  ALU16_ADDC (~alu16negc_val);						\
} while (0)

#define ALU32_NEGC()							\
do {									\
  unsigned32 alu32negc_val = (ALU32_RESULT);				\
  ALU32_SET (1);							\
  ALU32_ADDC (~alu32negc_val);						\
} while(0)

#define ALU64_NEGC()							\
do {									\
  unsigned64 alu64negc_val = (ALU64_RESULT);				\
  ALU64_SET (1);							\
  ALU64_ADDC (~alu64negc_val);						\
} while (0)

#define ALU_NEGC XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_NEGC)




/* Basic operation - negate borrowing (and overflowing) */

#define ALU8_NEGB()							\
do {									\
  signed alu8negb_val = (ALU8_RESULT);					\
  ALU8_SET (0);								\
  ALU8_SUBB (alu8negb_val);						\
} while (0)

#define ALU16_NEGB()							\
do {									\
  signed alu16negb_val = (ALU16_RESULT);				\
  ALU16_SET (0);							\
  ALU16_SUBB (alu16negb_val);						\
} while (0)

#define ALU32_NEGB()							\
do {									\
  unsigned32 alu32negb_val = (ALU32_RESULT);				\
  ALU32_SET (0);							\
  ALU32_SUBB (alu32negb_val);						\
} while(0)

#define ALU64_NEGB()							\
do {									\
  unsigned64 alu64negb_val = (ALU64_RESULT);				\
  ALU64_SET (0);							\
  ALU64_SUBB (alu64negb_val);						\
} while (0)

#define ALU_NEGB XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_NEGB)




/* Other */

#define ALU8_OR(VAL)							\
do {									\
  error("ALU16_OR");							\
} while (0)

#define ALU16_OR(VAL)							\
do {									\
  error("ALU16_OR");							\
} while (0)

#define ALU32_OR(VAL)							\
do {									\
  alu32_r |= (VAL);							\
  alu32_c = 0;								\
  alu32_v = 0;								\
} while (0)

#define ALU64_OR(VAL)							\
do {									\
  alu64_r |= (VAL);							\
  alu64_c = 0;								\
  alu64_v = 0;								\
} while (0)

#define ALU_OR(VAL) XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_OR)(VAL)



#define ALU16_XOR(VAL)							\
do {									\
  error("ALU16_XOR");							\
} while (0)

#define ALU32_XOR(VAL)							\
do {									\
  alu32_r ^= (VAL);							\
  alu32_c = 0;								\
  alu32_v = 0;								\
} while (0)

#define ALU64_XOR(VAL)							\
do {									\
  alu64_r ^= (VAL);							\
  alu64_c = 0;								\
  alu64_v = 0;								\
} while (0)

#define ALU_XOR(VAL) XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_XOR)(VAL)




#define ALU16_AND(VAL)							\
do {									\
  error("ALU_AND16");							\
} while (0)

#define ALU32_AND(VAL)							\
do {									\
  alu32_r &= (VAL);							\
  alu32_r = 0;								\
  alu32_v = 0;								\
} while (0)

#define ALU64_AND(VAL)							\
do {									\
  alu64_r &= (VAL);							\
  alu64_r = 0;								\
  alu64_v = 0;								\
} while (0)

#define ALU_AND(VAL) XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_AND)(VAL)




#define ALU16_NOT(VAL)							\
do {									\
  error("ALU_NOT16");							\
} while (0)

#define ALU32_NOT							\
do {									\
  alu32_r = ~alu32_r;							\
  alu32_c = 0;								\
  alu32_v = 0;								\
} while (0)

#define ALU64_NOT							\
do {									\
  alu64_r = ~alu64_r;							\
  alu64_c = 0;								\
  alu64_v = 0;								\
} while (0)

#define ALU_NOT XCONCAT3(ALU,WITH_TARGET_WORD_BITSIZE,_NOT)

#endif
