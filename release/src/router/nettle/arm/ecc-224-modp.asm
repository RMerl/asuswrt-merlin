C arm/ecc-224-modp.asm

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

	.file "ecc-224-modp.asm"
	.arm

define(<RP>, <r1>)
define(<H>, <r0>) C Overlaps unused modulo argument

define(<T0>, <r2>)
define(<T1>, <r3>)
define(<T2>, <r4>)
define(<T3>, <r5>)
define(<T4>, <r6>)
define(<T5>, <r7>)
define(<T6>, <r8>)
define(<N3>, <r10>)
define(<L0>, <r11>)
define(<L1>, <r12>)
define(<L2>, <lr>)

	C ecc_224_modp (const struct ecc_modulo *m, mp_limb_t *rp)
	.text
	.align 2

PROLOGUE(nettle_ecc_224_modp)
	push	{r4,r5,r6,r7,r8,r10,r11,lr}

	add	L2, RP, #28
	ldm	L2, {T0,T1,T2,T3,T4,T5,T6}
	mov	H, #0

	adds	T0, T0, T4
	adcs	T1, T1, T5
	adcs	T2, T2, T6
	adc	H, H, #0

	C This switch from adcs to sbcs takes carry into account with
	C correct sign, but it always subtracts 1 too much. We arrange
	C to also add B^7 + 1 below, so the effect is adding p. This
	C addition of p also ensures that the result never is
	C negative.

	sbcs	N3, T3, T0
	sbcs	T4, T4, T1
	sbcs	T5, T5, T2
	sbcs	T6, T6, H
	mov	H, #1		C This is the B^7
	sbc	H, #0
	subs	T6, T6, T3
	sbc	H, #0

	C Now subtract from low half
	ldm	RP!, {L0,L1,L2}

	C Clear carry, with the sbcs, this is the 1.
	adds	RP, #0

	sbcs	T0, L0, T0
	sbcs	T1, L1, T1
	sbcs	T2, L2, T2
	ldm	RP!, {T3,L0,L1,L2}
	sbcs	T3, T3, N3
	sbcs	T4, L0, T4
	sbcs	T5, L1, T5
	sbcs	T6, L2, T6
	rsc	H, H, #0

	C Now -2 <= H <= 0 is the borrow, so subtract (B^3 - 1) |H|
	C Use (B^3 - 1) H = <H, H, H> if -1 <=H <= 0, and
	C     (B^3 - 1) H = <1,B-1, B-1, B-2> if H = -2
	subs	T0, T0, H
	asr	L1, H, #1
	sbcs	T1, T1, L1
	eor	H, H, L1
	sbcs	T2, T2, L1
	sbcs	T3, T3, H
	sbcs	T4, T4, #0
	sbcs	T5, T5, #0
	sbcs	T6, T6, #0
	sbcs	H, H, H

	C Final borrow, subtract (B^3 - 1) |H|
	subs	T0, T0, H
	sbcs	T1, T1, H
	sbcs	T2, T2, H
	sbcs	T3, T3, #0
	sbcs	T4, T4, #0
	sbcs	T5, T5, #0
	sbcs	T6, T6, #0

	stmdb	RP, {T0,T1,T2,T3,T4,T5,T6}

	pop	{r4,r5,r6,r7,r8,r10,r11,pc}
EPILOGUE(nettle_ecc_224_modp)
