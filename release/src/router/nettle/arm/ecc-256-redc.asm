C arm/ecc-256-redc.asm

ifelse(<
   Copyright (C) 2013 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
>) 

	.file "ecc-256-redc.asm"
	.arm

define(<RP>, <r1>)

define(<T0>, <r0>) C Overlaps unused modulo argument
define(<T1>, <r2>)
define(<T2>, <r3>)
define(<T3>, <r4>)
define(<T4>, <r5>)
define(<T5>, <r6>)
define(<T6>, <r7>)
define(<T7>, <r8>)
define(<F0>, <r10>)
define(<F1>, <r11>)
define(<F2>, <r12>)
define(<F3>, <lr>)

	C ecc_256_redc (const struct ecc_modulo *m, mp_limb_t *rp)
	.text
	.align 2

PROLOGUE(nettle_ecc_256_redc)
	push	{r4,r5,r6,r7,r8,r10,r11,lr}

	ldm	RP!, {T0,T1,T2,T3,T4,T5,T6,T7}

	C Set <F3,F2,F1> to the high 4 limbs of (B^2-B+1)<T2,T1,T0>
	C         T2 T1
	C   T2 T1 T0
	C   -  T2 T1 T0
	C -------------
	C   F3 F2 F1 F0


	adds	F1, T0, T2
	adcs	F2, T1, #0
	adc	F3, T2, #0

	subs	F0, T1, T0
	sbcs	F1, F1, T1	C Could also be rsc ?
	sbcs	F2, F2, T2
	sbc	F3, F3, #0

	C Add:
	C   T10 T9 T8 T7 T6 T5 T4 T3
	C +  F3 F2 F1 F0 T0 T2 T1 T0
	C --------------------------
	C    T7 T6 T5 T4 T3 T2 T1 T0

	adds	T3, T3, T0
	adcs	T1, T4, T1
	adcs	T2, T5, T2
	adcs	T6, T6, T0
	mov	T0, T3		C FIXME: Be more clever?
	mov	T3, T6
	adcs	T4, T7, F0

	ldm	RP!, {T5,T6,T7}
	adcs	T5, T5, F1
	adcs	T6, T6, F2
	adcs	T7, T7, F3

	C New F3, F2, F1, F0, also adding in carry
	adcs	F1, T0, T2
	adcs	F2, T1, #0
	adc	F3, T2, #0

	subs	F0, T1, T0
	sbcs	F1, F1, T1	C Could also be rsc ?
	sbcs	F2, F2, T2
	sbc	F3, F3, #0

	C Start adding
	adds	T3, T3, T0
	adcs	T1, T4, T1
	adcs	T2, T5, T2
	adcs	T6, T6, T0
	mov	T0, T3		C FIXME: Be more clever?
	mov	T3, T6
	adcs	T4, T7, F0

	ldm	RP!, {T5,T6,T7}
	adcs	T5, T5, F1
	adcs	T6, T6, F2
	adcs	T7, T7, F3

	C Final iteration, eliminate only T0, T1
	C Set <F2, F1, F0> to the high 3 limbs of (B^2-B+1)<T1,T0>

	C      T1 T0 T1
	C      -  T1 T0
	C -------------
	C      F2 F1 F0

	C First add in carry
	adcs	F1, T0, #0
	adcs	F2, T1, #0
	subs	F0, T1, T0
	sbcs	F1, F1, T1
	sbc	F2, F2, #0

	C Add:
	C    T9 T8 T7 T6 T5 T4 T3 T2
	C +  F2 F1 F0 T0  0 T1 T0  0
	C --------------------------
	C    F2 F1 T7 T6 T5 T4 T3 T2

	adds	T3, T3, T0
	adcs	T4, T4, T1
	adcs	T5, T5, #0
	adcs	T6, T6, T0
	adcs	T7, T7, F0
	ldm	RP!, {T0, T1}
	mov	F3, #0
	adcs	F1, F1, T0
	adcs	F2, F2, T1

	C Sum is < B^8 + p, so it's enough to fold carry once,
	C If carry, add in
	C   B^7 - B^6 - B^3 + 1  = <0, B-2, B-1, B-1, B-1, 0, 0, 1>

	C Mask from carry flag, leaving carry intact
	adc	F3, F3, #0
	rsb	F3, F3, #0

	adcs	T0, T2, #0
	adcs	T1, T3, #0
	adcs	T2, T4, #0
	adcs	T3, T5, F3
	adcs	T4, T6, F3
	adcs	T5, T7, F3
	and	F3, F3, #-2
	adcs	T6, F1, F3
	adcs	T7, F2, #0

	sub	RP, RP, #64
	stm	RP, {T0,T1,T2,T3,T4,T5,T6,T7}

	pop	{r4,r5,r6,r7,r8,r10,r11,pc}
EPILOGUE(nettle_ecc_256_redc)
