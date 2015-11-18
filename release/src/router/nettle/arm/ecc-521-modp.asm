C arm/ecc-521-modp.asm

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

	.file "ecc-521-modp.asm"
	.arm

define(<HP>, <r0>)
define(<RP>, <r1>)
define(<T0>, <r2>)
define(<T1>, <r3>)
define(<T2>, <r4>)
define(<F0>, <r5>)
define(<F1>, <r6>)
define(<F2>, <r7>)
define(<F3>, <r8>)
define(<H>, <r12>)
define(<N>, <lr>)

	C ecc_521_modp (const struct ecc_modulo *m, mp_limb_t *rp)
	.text
.Lc511:
	.int 511

	.align 2

PROLOGUE(nettle_ecc_521_modp)
	push	{r4,r5,r6,r7,r8,lr}

	C Use that B^17 = 2^23 (mod p)
	ldr	F3, [RP, #+68]		C 17
	add	HP, RP, #72		C 18
	ldr	T0, [RP]		C 0
	adds	T0, T0, F3, lsl	#23
	str	T0, [RP], #+4
	mov	N, #5

	C 5 iterations, reading limbs 18-20, 21-23, 24-26, 27-29, 30-32
	C and adding to limbs          1-3,    4-6,   7-9, 19-12, 13-15
.Loop:
	ldm	RP, {T0,T1,T2}		C  1+3*k --  3+3*k
	lsr	F0, F3, #9
	ldm	HP!, {F1,F2,F3}		C 18+3*k -- 20+3*k
	orr	F0, F0, F1, lsl #23
	lsr	F1, F1, #9
	orr	F1, F1, F2, lsl #23
	lsr	F2, F2, #9
	orr	F2, F2, F3, lsl #23
	adcs	T0, T0, F0
	adcs	T1, T1, F1
	adcs	T2, T2, F2
	sub	N, N, #1
	stm	RP!,{T0,T1,T2}
	teq	N, #0
	bne	.Loop

	ldr	F0, [RP], #-64		C 16
	ldr	F1, [HP]		C 33
	ldr	T0, .Lc511

	C Handling of high limbs
	C F0 = rp[16] + carry in + F3 >> 9
	adcs	F0, F0, F3, lsr #9
	C Copy low 9 bits to H, then shift right including carry
	and	H, F0, T0
	rrx	F0, F0
	lsr	F0, F0, #8
	C Add in F1 = rp[33], with weight 2^1056 = 2^14
	adds	F0, F0, F1, lsl #14
	lsr	F1, F1, #18
	adc	F1, F1, #0

	ldm	RP, {T0, T1}		C 0-1
	adds	T0, T0, F0
	adcs	T1, T1, F1
	stm	RP!, {T0, T1}

	ldm	RP, {T0,T1,T2,F0,F1,F2,F3}	C 2-8
	adcs	T0, T0, #0
	adcs	T1, T1, #0
	adcs	T2, T2, #0
	adcs	F0, F0, #0
	adcs	F1, F1, #0
	adcs	F2, F2, #0
	adcs	F3, F3, #0
	stm	RP!, {T0,T1,T2,F0,F1,F2,F3}	C 2-8
	ldm	RP, {T0,T1,T2,F0,F1,F2,F3}	C 9-15
	adcs	T0, T0, #0
	adcs	T1, T1, #0
	adcs	T2, T2, #0
	adcs	F0, F0, #0
	adcs	F1, F1, #0
	adcs	F2, F2, #0
	adcs	F3, F3, #0
	adcs	H, H, #0
	stm	RP, {T0,T1,T2,F0,F1,F2,F3,H}	C 9-16

	pop	{r4,r5,r6,r7,r8,pc}
EPILOGUE(nettle_ecc_521_modp)
