C arm/v6/sha256-compress.asm

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

	.file "sha256-compress.asm"
	.arch armv6

define(<STATE>, <r0>)
define(<INPUT>, <r1>)
define(<K>, <r2>)
define(<SA>, <r3>)
define(<SB>, <r4>)
define(<SC>, <r5>)
define(<SD>, <r6>)
define(<SE>, <r7>)
define(<SF>, <r8>)
define(<SG>, <r10>)
define(<SH>, <r11>)
define(<T0>, <r12>)
define(<T1>, <r1>)	C Overlap INPUT
define(<COUNT>, <r0>)	C Overlap STATE
define(<W>, <r14>)

C Used for data load
define(<I0>, <r3>)
define(<I1>, <r4>)
define(<I2>, <r5>)
define(<I3>, <r6>)
define(<I4>, <r7>)
define(<DST>, <r8>)
define(<SHIFT>, <r10>)
define(<ILEFT>, <r11>)

define(<EXPN>, <
	ldr	W, [sp, #+eval(4*$1)]
	ldr	T0, [sp, #+eval(4*(($1 + 14) % 16))]
	ror	T1, T0, #17
	eor	T1, T1, T0, ror #19
	eor	T1, T1, T0, lsr #10
	add	W, W, T1
	ldr	T0, [sp, #+eval(4*(($1 + 9) % 16))]
	add	W, W, T0
	ldr	T0, [sp, #+eval(4*(($1 + 1) % 16))]
	ror	T1, T0, #7
	eor	T1, T1, T0, ror #18
	eor	T1, T1, T0, lsr #3
	add	W, W, T1
	str	W, [sp, #+eval(4*$1)]
>)

C ROUND(A,B,C,D,E,F,G,H)
C
C H += S1(E) + Choice(E,F,G) + K + W
C D += H
C H += S0(A) + Majority(A,B,C)
C
C Where
C
C S1(E) = E<<<26 ^ E<<<21 ^ E<<<7
C S0(A) = A<<<30 ^ A<<<19 ^ A<<<10
C Choice (E, F, G) = G^(E&(F^G))
C Majority (A,B,C) = (A&B) + (C&(A^B))
	
define(<ROUND>, <
	ror	T0, $5, #6
	eor	T0, T0, $5, ror #11
	eor	T0, T0, $5, ror #25
	add	$8, $8, T0
	eor	T0, $6, $7
	and	T0, T0, $5
	eor	T0, T0, $7
	add	$8,$8, T0
	ldr	T0, [K], #+4
	add	$8, $8, W
	add	$8, $8, T0
	add	$4, $4, $8
	ror	T0, $1, #2
	eor	T0, T0, $1, ror #13
	eor	T0, T0, $1, ror #22
	add	$8, $8, T0
	and	T0, $1, $2
	add	$8, $8, T0
	eor	T0, $1, $2
	and	T0, T0, $3
	add	$8, $8, T0
>)

define(<NOEXPN>, <
	ldr	W, [sp, + $1]
	add	$1, $1, #4
>)
	C void
	C _nettle_sha256_compress(uint32_t *state, const uint8_t *input, const uint32_t *k)

	.text
	.align 2

PROLOGUE(_nettle_sha256_compress)
	push	{r4,r5,r6,r7,r8,r10,r11,r14}
	sub	sp, sp, #68
	str	STATE, [sp, #+64]

	C Load data up front, since we don't have enough registers
	C to load and shift on-the-fly
	ands	SHIFT, INPUT, #3
	and	INPUT, INPUT, $-4
	ldr	I0, [INPUT]
	addne	INPUT, INPUT, #4
	lsl	SHIFT, SHIFT, #3
	mov	T0, #0
	movne	T0, #-1
	lsl	I1, T0, SHIFT
	uadd8	T0, T0, I1		C Sets APSR.GE bits

	mov	DST, sp
	mov	ILEFT, #4
.Lcopy:
	ldm	INPUT!, {I1,I2,I3,I4}
	sel	I0, I0, I1
	ror	I0, I0, SHIFT
	rev	I0, I0
	sel	I1, I1, I2
	ror	I1, I1, SHIFT
	rev	I1, I1
	sel	I2, I2, I3
	ror	I2, I2, SHIFT
	rev	I2, I2
	sel	I3, I3, I4
	ror	I3, I3, SHIFT
	rev	I3, I3
	subs	ILEFT, ILEFT, #1
	stm	DST!, {I0,I1,I2,I3}
	mov	I0, I4	
	bne	.Lcopy
	
	ldm	STATE, {SA,SB,SC,SD,SE,SF,SG,SH}

	mov	COUNT,#0

.Loop1:
	NOEXPN(COUNT) ROUND(SA,SB,SC,SD,SE,SF,SG,SH)
	NOEXPN(COUNT) ROUND(SH,SA,SB,SC,SD,SE,SF,SG)
	NOEXPN(COUNT) ROUND(SG,SH,SA,SB,SC,SD,SE,SF)
	NOEXPN(COUNT) ROUND(SF,SG,SH,SA,SB,SC,SD,SE)
	NOEXPN(COUNT) ROUND(SE,SF,SG,SH,SA,SB,SC,SD)
	NOEXPN(COUNT) ROUND(SD,SE,SF,SG,SH,SA,SB,SC)
	NOEXPN(COUNT) ROUND(SC,SD,SE,SF,SG,SH,SA,SB)
	NOEXPN(COUNT) ROUND(SB,SC,SD,SE,SF,SG,SH,SA)
	cmp	COUNT,#64
	bne	.Loop1

	mov	COUNT, #3
.Loop2:
	
	EXPN( 0) ROUND(SA,SB,SC,SD,SE,SF,SG,SH)
	EXPN( 1) ROUND(SH,SA,SB,SC,SD,SE,SF,SG)
	EXPN( 2) ROUND(SG,SH,SA,SB,SC,SD,SE,SF)
	EXPN( 3) ROUND(SF,SG,SH,SA,SB,SC,SD,SE)
	EXPN( 4) ROUND(SE,SF,SG,SH,SA,SB,SC,SD)
	EXPN( 5) ROUND(SD,SE,SF,SG,SH,SA,SB,SC)
	EXPN( 6) ROUND(SC,SD,SE,SF,SG,SH,SA,SB)
	EXPN( 7) ROUND(SB,SC,SD,SE,SF,SG,SH,SA)
	EXPN( 8) ROUND(SA,SB,SC,SD,SE,SF,SG,SH)
	EXPN( 9) ROUND(SH,SA,SB,SC,SD,SE,SF,SG)
	EXPN(10) ROUND(SG,SH,SA,SB,SC,SD,SE,SF)
	EXPN(11) ROUND(SF,SG,SH,SA,SB,SC,SD,SE)
	EXPN(12) ROUND(SE,SF,SG,SH,SA,SB,SC,SD)
	EXPN(13) ROUND(SD,SE,SF,SG,SH,SA,SB,SC)
	EXPN(14) ROUND(SC,SD,SE,SF,SG,SH,SA,SB)
	subs	COUNT, COUNT, #1
	EXPN(15) ROUND(SB,SC,SD,SE,SF,SG,SH,SA)
	bne	.Loop2

	ldr	STATE, [sp, #+64]
	C No longer needed registers
	ldm	STATE, {r1,r2,r12,r14}
	add	SA, SA, r1
	add	SB, SB, r2
	add	SC, SC, r12
	add	SD, SD, r14
	stm	STATE!, {SA,SB,SC,SD}
	ldm	STATE, {r1,r2,r12,r14}
	add	SE, SE, r1
	add	SF, SF, r2
	add	SG, SG, r12
	add	SH, SH, r14
	stm	STATE!, {SE,SF,SG,SH}
	add	sp, sp, #68
	pop	{r4,r5,r6,r7,r8,r10,r11,pc}
EPILOGUE(_nettle_sha256_compress)
