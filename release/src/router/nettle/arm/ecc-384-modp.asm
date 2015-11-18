C arm/ecc-384-modp.asm

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

	.file "ecc-384-modp.asm"
	.arm

define(<RP>, <r1>)
define(<T0>, <r0>)
define(<T1>, <r2>)
define(<T2>, <r3>)
define(<T3>, <r4>)
define(<F0>, <r5>)
define(<F1>, <r6>)
define(<F2>, <r7>)
define(<F3>, <r8>)
define(<F4>, <r10>)
define(<N>, <r12>)
define(<H>, <lr>)
	
	C ecc_384_modp (const struct ecc_modulo *m, mp_limb_t *rp)
	.text
	.align 2

PROLOGUE(nettle_ecc_384_modp)
	push	{r4,r5,r6,r7,r8,r10,lr}

	add	RP, RP, #80
	ldm	RP, {T0, T1, T2, T3}	C 20-23

	C First get top 4 limbs, which need folding twice, as
	C
	C     T3 T2 T1 T0
	C        T3 T2 T1
	C             -T3
	C ----------------
	C  F4 F3 F2 F1 F0
	C
	C Start with
	C
	C   T3 T1 T0
	C         T1
	C        -T3
	C -----------
	C   F2 F1 F0   Always fits
	
	adds	F0, T0, T1
	adcs	F1, T1, #0
	adcs	F2, T3, #0
	subs	F0, F0, T3
	sbcs	F1, F1, #0
	sbcs	F2, F2, #0

	C      T3 T2 T2  0
	C         F2 F1 F0
	C  ----------------
	C   F4 F3 F2 F1 F0

	mov	F4, #0
	adds	F1, F1, T2
	adcs	F2, F2, T2
	adcs	F3, T3, #0
	adcs	F4, F4, #0

	C Add in to high part
	sub	RP, RP, #32
	ldm	RP, {T0, T1, T2, T3}	C 12-15
	mov	H, #0
	adds	F0, T0, F0
	adcs	F1, T1, F1
	adcs	F2, T2, F2
	adcs	F3, T3, F3
	adcs	F4, F4, #0			C Do F4 later

	C Add to low part, keeping carry (positive or negative) in H
	sub	RP, RP, #48
	ldm	RP, {T0, T1, T2, T3}	C 0-3
	mov	H, #0
	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3
	adc	H, H, #0
	subs	T1, T1, F0
	sbcs	T2, T2, F1
	sbcs	T3, T3, F2
	sbc	H, H, #0
	adds	T3, T3, F0
	adc	H, H, #0
	
	stm	RP!, {T0,T1,T2,T3}	C 0-3
	mov	N, #2
.Loop:
	ldm	RP, {T0,T1,T2,T3}	C 4-7

	C First, propagate carry
	adds	T0, T0, H
	asr	H, #31		C Sign extend
	adcs	T1, T1, H
	adcs	T2, T2, H
	adcs	T3, T3, H
	adc	H, H, #0

	C +B^4 term
	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3
	adc	H, H, #0

	C +B^3 terms
	ldr	F0, [RP, #+48]		C 16
	adds	T0, T0, F1
	adcs	T1, T1, F2
	adcs	T2, T2, F3
	adcs	T3, T3, F0
	adc	H, H, #0

	C -B
	ldr	F1, [RP, #+52]		C 17-18
	ldr	F2, [RP, #+56]
	subs	T0, T0, F3
	sbcs	T1, T1, F0
	sbcs	T2, T2, F1
	sbcs	T3, T3, F2
	sbcs	H, H, #0

	C +1
	ldr	F3, [RP, #+60]		C 19
	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3
	adc	H, H, #0
	subs	N, N, #1
	stm	RP!, {T0,T1,T2,T3}
	bne	.Loop

	C Fold high limbs, we need to add in
	C
	C F4 F4 0 -F4 F4 H H 0 -H H
	C
	C We always have F4 >= 0, but we can have H < 0.
	C Sign extension gets tricky when F4 = 0 and H < 0.
	sub	RP, RP, #48

	ldm	RP, {T0,T1,T2,T3}	C 0-3

	C     H  H  0 -H  H
	C  ----------------
	C  S  H F3 F2 F1 F0
	C
	C Define S = H >> 31 (asr), we then have
	C
	C  F0 = H
	C  F1 = S - H
	C  F2 = - [H > 0]
	C  F3 = H - [H > 0]
	C   H = H + S
	C 
	C And we get underflow in S - H iff H > 0

	C				H = 0	H > 0	H = -1
	mov	F0, H		C	0	H	-1
	asr	H, #31
	subs	F1, H, F0	C	0,C=1	-H,C=0	0,C=1
	sbc	F2, F2, F2	C	0	-1	0
	sbc	F3, F0, #0	C	0	H-1	-1

	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3
	adc	H, H, F0	C	0+cy	H+cy	-2+cy

	stm	RP!, {T0,T1,T2,T3}	C 0-3
	ldm	RP, {T0,T1,T2,T3}	C 4-7
	
	C   F4  0 -F4
	C ---------
	C   F3 F2  F1
	
	rsbs	F1, F4, #0
	sbc	F2, F2, F2
	sbc	F3, F4, #0

	C Sign extend H
	adds	F0, F4, H
	asr	H, H, #31	
	adcs	F1, F1, H
	adcs	F2, F2, H
	adcs	F3, F3, H
	adcs	F4, F4, H
	adc	H, H, #0
	
	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3

	stm	RP!, {T0,T1,T2,T3}	C 4-7
	ldm	RP, {T0,T1,T2,T3}	C 8-11

	adcs	T0, T0, F4
	adcs	T1, T1, H
	adcs	T2, T2, H
	adcs	T3, T3, H
	adc	H, H, #0
	
	stm	RP, {T0,T1,T2,T3}	C 8-11

	C Final (unlikely) carry
	sub	RP, RP, #32
	ldm	RP, {T0,T1,T2,T3}	C 0-3
	C Fold H into F0-F4
	mov	F0, H
	asr	H, #31
	subs	F1, H, F0
	sbc	F2, F2, F2
	sbc	F3, F0, #0
	add	F4, F0, H

	adds	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	adcs	T3, T3, F3
	
	stm	RP!, {T0,T1,T2,T3}	C 0-3
	ldm	RP, {T0,T1,T2,T3}	C 4-7
	adcs	T0, T0, F4
	adcs	T1, T1, H
	adcs	T2, T2, H
	adcs	T3, T3, H
	stm	RP!, {T0,T1,T2,T3}	C 4-7
	ldm	RP, {T0,T1,T2,T3}	C 8-11
	adcs	T0, T0, H
	adcs	T1, T1, H
	adcs	T2, T2, H
	adcs	T3, T3, H
	stm	RP!, {T0,T1,T2,T3}	C 8-11
	pop	{r4,r5,r6,r7,r8,r10,pc}
EPILOGUE(nettle_ecc_384_modp)
