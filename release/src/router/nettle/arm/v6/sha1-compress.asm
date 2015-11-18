C arm/v6/sha1-compress.asm

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

	.file "sha1-compress.asm"
	.arch armv6

define(<STATE>, <r0>)
define(<INPUT>, <r1>)
define(<SA>, <r2>)
define(<SB>, <r3>)
define(<SC>, <r4>)
define(<SD>, <r5>)
define(<SE>, <r6>)
define(<T0>, <r7>)
define(<SHIFT>, <r8>)
define(<WPREV>, <r10>)
define(<W>, <r12>)
define(<K>, <lr>)

C FIXME: Could avoid a mov with even and odd variants.
define(<LOAD>, <
	ldr	T0, [INPUT], #+4
	sel	W, WPREV, T0
	ror	W, W, SHIFT
	mov	WPREV, T0
	rev	W, W
	str	W, [SP,#eval(4*$1)]
>)
define(<EXPN>, <
	ldr	W, [sp, #+eval(4*$1)]
	ldr	T0, [sp, #+eval(4*(($1 + 2) % 16))]
	eor	W, W, T0
	ldr	T0, [sp, #+eval(4*(($1 + 8) % 16))]
	eor	W, W, T0
	ldr	T0, [sp, #+eval(4*(($1 + 13) % 16))]
	eor	W, W, T0
	ror	W, W, #31
	str	W, [sp, #+eval(4*$1)]
>)

C F1(B,C,D) = D^(B&(C^D))
C ROUND1(A,B,C,D,E)
define(<ROUND1>, <
	eor	T0, $3, $4
	add	$5, $5, K
	and	T0, T0, $2
	add	$5, $5, $1, ror	#27
	eor	T0, T0, $4
	add	$5, $5, W
	ror	$2, $2, #2
	add	$5, $5, T0
>)
C F2(B,C,D) = B^C^D
define(<ROUND2>, <
	eor	T0, $2, $4
	add	$5, $5, K
	eor	T0, T0, $3
	add	$5, $5, $1, ror	#27
	add	$5, $5, W
	ror	$2, $2, #2
	add	$5, $5, T0
>)
C F3(B,C,D) = (B&C) | (D & (B|C)) = (B & (C ^ D)) + (C & D)
define(<ROUND3>, <
	eor	T0, $3, $4
	add	$5, $5, K
	and	T0, T0, $2
	add	$5, $5, $1, ror	#27
	add	$5, $5, T0
	add	$5, $5, W
	and	T0, $3, $4
	ror	$2, $2, #2
	add	$5, $5, T0
>)
	C void _nettle_sha1_compress(uint32_t *state, const uint8_t *input)
	
	.text
	.align 2
.LK1:
	.int	0x5A827999
.LK2:
	.int	0x6ED9EBA1
.LK3:
	.int	0x8F1BBCDC

PROLOGUE(_nettle_sha1_compress)
	push	{r4,r5,r6,r7,r8,r10,lr}
	sub	sp, sp, #64

	C Sets SHIFT to 8*low bits of input pointer. Sets up GE flags
	C as follows, corresponding to bytes to be used from WPREV	
	C   SHIFT	0	8	16	24
	C CPSR.GE	0000	1110	1100	1000
	ands	SHIFT, INPUT, #3
	and	INPUT, INPUT, $-4
	ldr	WPREV, [INPUT]
	addne	INPUT, INPUT, #4	C Unaligned input
	lsl	SHIFT, SHIFT, #3
	mov	T0, #0
	movne	T0, #-1
	lsl	W, T0, SHIFT
	uadd8	T0, T0, W		C Sets APSR.GE bits
	
	ldr	K, .LK1
	ldm	STATE, {SA,SB,SC,SD,SE}
	
	LOAD( 0) ROUND1(SA, SB, SC, SD, SE)
	LOAD( 1) ROUND1(SE, SA, SB, SC, SD)
	LOAD( 2) ROUND1(SD, SE, SA, SB, SC)
	LOAD( 3) ROUND1(SC, SD, SE, SA, SB)
	LOAD( 4) ROUND1(SB, SC, SD, SE, SA)

	LOAD( 5) ROUND1(SA, SB, SC, SD, SE)
	LOAD( 6) ROUND1(SE, SA, SB, SC, SD)
	LOAD( 7) ROUND1(SD, SE, SA, SB, SC)
	LOAD( 8) ROUND1(SC, SD, SE, SA, SB)
	LOAD( 9) ROUND1(SB, SC, SD, SE, SA)

	LOAD(10) ROUND1(SA, SB, SC, SD, SE)
	LOAD(11) ROUND1(SE, SA, SB, SC, SD)
	LOAD(12) ROUND1(SD, SE, SA, SB, SC)
	LOAD(13) ROUND1(SC, SD, SE, SA, SB)
	LOAD(14) ROUND1(SB, SC, SD, SE, SA)

	LOAD(15) ROUND1(SA, SB, SC, SD, SE)
	EXPN( 0) ROUND1(SE, SA, SB, SC, SD)
	EXPN( 1) ROUND1(SD, SE, SA, SB, SC)
	EXPN( 2) ROUND1(SC, SD, SE, SA, SB)
	EXPN( 3) ROUND1(SB, SC, SD, SE, SA)

	ldr	K, .LK2
	EXPN( 4) ROUND2(SA, SB, SC, SD, SE)
	EXPN( 5) ROUND2(SE, SA, SB, SC, SD)
	EXPN( 6) ROUND2(SD, SE, SA, SB, SC)
	EXPN( 7) ROUND2(SC, SD, SE, SA, SB)
	EXPN( 8) ROUND2(SB, SC, SD, SE, SA)

	EXPN( 9) ROUND2(SA, SB, SC, SD, SE)
	EXPN(10) ROUND2(SE, SA, SB, SC, SD)
	EXPN(11) ROUND2(SD, SE, SA, SB, SC)
	EXPN(12) ROUND2(SC, SD, SE, SA, SB)
	EXPN(13) ROUND2(SB, SC, SD, SE, SA)

	EXPN(14) ROUND2(SA, SB, SC, SD, SE)
	EXPN(15) ROUND2(SE, SA, SB, SC, SD)
	EXPN( 0) ROUND2(SD, SE, SA, SB, SC)
	EXPN( 1) ROUND2(SC, SD, SE, SA, SB)
	EXPN( 2) ROUND2(SB, SC, SD, SE, SA)

	EXPN( 3) ROUND2(SA, SB, SC, SD, SE)
	EXPN( 4) ROUND2(SE, SA, SB, SC, SD)
	EXPN( 5) ROUND2(SD, SE, SA, SB, SC)
	EXPN( 6) ROUND2(SC, SD, SE, SA, SB)
	EXPN( 7) ROUND2(SB, SC, SD, SE, SA)

	ldr	K, .LK3
	EXPN( 8) ROUND3(SA, SB, SC, SD, SE)
	EXPN( 9) ROUND3(SE, SA, SB, SC, SD)
	EXPN(10) ROUND3(SD, SE, SA, SB, SC)
	EXPN(11) ROUND3(SC, SD, SE, SA, SB)
	EXPN(12) ROUND3(SB, SC, SD, SE, SA)

	EXPN(13) ROUND3(SA, SB, SC, SD, SE)
	EXPN(14) ROUND3(SE, SA, SB, SC, SD)
	EXPN(15) ROUND3(SD, SE, SA, SB, SC)
	EXPN( 0) ROUND3(SC, SD, SE, SA, SB)
	EXPN( 1) ROUND3(SB, SC, SD, SE, SA)

	EXPN( 2) ROUND3(SA, SB, SC, SD, SE)
	EXPN( 3) ROUND3(SE, SA, SB, SC, SD)
	EXPN( 4) ROUND3(SD, SE, SA, SB, SC)
	EXPN( 5) ROUND3(SC, SD, SE, SA, SB)
	EXPN( 6) ROUND3(SB, SC, SD, SE, SA)

	EXPN( 7) ROUND3(SA, SB, SC, SD, SE)
	EXPN( 8) ROUND3(SE, SA, SB, SC, SD)
	EXPN( 9) ROUND3(SD, SE, SA, SB, SC)
	EXPN(10) ROUND3(SC, SD, SE, SA, SB)
	EXPN(11) ROUND3(SB, SC, SD, SE, SA)

	ldr	K, .LK4
	EXPN(12) ROUND2(SA, SB, SC, SD, SE)
	EXPN(13) ROUND2(SE, SA, SB, SC, SD)
	EXPN(14) ROUND2(SD, SE, SA, SB, SC)
	EXPN(15) ROUND2(SC, SD, SE, SA, SB)
	EXPN( 0) ROUND2(SB, SC, SD, SE, SA)

	EXPN( 1) ROUND2(SA, SB, SC, SD, SE)
	EXPN( 2) ROUND2(SE, SA, SB, SC, SD)
	EXPN( 3) ROUND2(SD, SE, SA, SB, SC)
	EXPN( 4) ROUND2(SC, SD, SE, SA, SB)
	EXPN( 5) ROUND2(SB, SC, SD, SE, SA)

	EXPN( 6) ROUND2(SA, SB, SC, SD, SE)
	EXPN( 7) ROUND2(SE, SA, SB, SC, SD)
	EXPN( 8) ROUND2(SD, SE, SA, SB, SC)
	EXPN( 9) ROUND2(SC, SD, SE, SA, SB)
	EXPN(10) ROUND2(SB, SC, SD, SE, SA)

	EXPN(11) ROUND2(SA, SB, SC, SD, SE)
	EXPN(12) ROUND2(SE, SA, SB, SC, SD)
	EXPN(13) ROUND2(SD, SE, SA, SB, SC)
	EXPN(14) ROUND2(SC, SD, SE, SA, SB)
	EXPN(15) ROUND2(SB, SC, SD, SE, SA)

	C Use registers we no longer need. 
	ldm	STATE, {INPUT,T0,SHIFT,W,K}
	add	SA, SA, INPUT
	add	SB, SB, T0
	add	SC, SC, SHIFT
	add	SD, SD, W
	add	SE, SE, K
	add	sp, sp, #64
	stm	STATE, {SA,SB,SC,SD,SE}
	pop	{r4,r5,r6,r7,r8,r10,pc}	
EPILOGUE(_nettle_sha1_compress)

.LK4:
	.int	0xCA62C1D6
