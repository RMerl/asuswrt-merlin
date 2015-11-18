C x86_64/sha512-compress.asm

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

	.file "sha512-compress.asm"
define(<STATE>, <%rdi>)
define(<INPUT>, <%rsi>)
define(<K>, <%rdx>)
define(<SA>, <%rax>)
define(<SB>, <%rbx>)
define(<SC>, <%rcx>)
define(<SD>, <%r8>)
define(<SE>, <%r9>)
define(<SF>, <%r10>)
define(<SG>, <%r11>)
define(<SH>, <%r12>)
define(<T0>, <%r13>)
define(<T1>, <%rdi>)	C Overlap STATE
define(<COUNT>, <%r14>)
define(<W>, <%r15>)

define(<EXPN>, <
	mov	OFFSET64($1)(%rsp), W
	mov	OFFSET64(eval(($1 + 14) % 16))(%rsp), T0
	mov	T0, T1
	shr	<$>6, T0
	rol	<$>3, T1
	xor	T1, T0
	rol	<$>42, T1
	xor	T1, T0
	add	T0, W
	mov	OFFSET64(eval(($1 + 1) % 16))(%rsp), T0
	mov	T0, T1
	shr	<$>7, T0
	rol	<$>56, T1
	xor	T1, T0
	rol	<$>7, T1
	xor	T1, T0
	add	T0, W
	add	OFFSET64(eval(($1 + 9) % 16))(%rsp), W
	mov	W, OFFSET64($1)(%rsp)
>)

C ROUND(A,B,C,D,E,F,G,H,K)
C
C H += S1(E) + Choice(E,F,G) + K + W
C D += H
C H += S0(A) + Majority(A,B,C)
C
C Where
C
C S1(E) = E<<<50 ^ E<<<46 ^ E<<<23
C S0(A) = A<<<36 ^ A<<<30 ^ A<<<25
C Choice (E, F, G) = G^(E&(F^G))
C Majority (A,B,C) = (A&B) + (C&(A^B))

define(<ROUND>, <
	mov	$5, T0
	mov	$5, T1
	rol	<$>23, T0
	rol	<$>46, T1
	xor	T0, T1
	rol	<$>27, T0
	xor	T0, T1
	add	W, $8
	add	T1, $8
	mov	$7, T0
	xor	$6, T0
	and	$5, T0
	xor	$7, T0
	add	OFFSET64($9)(K,COUNT,8), $8
	add	T0, $8
	add	$8, $4

	mov	$1, T0
	mov	$1, T1
	rol	<$>25, T0
	rol	<$>30, T1
	xor	T0, T1
	rol	<$>11, T0
	xor	T0, T1
	add	T1, $8
	mov	$1, T0
	mov	$1, T1
	and	$2, T0
	xor	$2, T1
	add	T0, $8
	and	$3, T1
	add	T1, $8
>)

define(<NOEXPN>, <
	mov	OFFSET64($1)(INPUT, COUNT, 8), W
	bswap	W
	mov	W, OFFSET64($1)(%rsp, COUNT, 8)
>)

	C void
	C _nettle_sha512_compress(uint64_t *state, const uint8_t *input, const uint64_t *k)

	.text
	ALIGN(16)

PROLOGUE(_nettle_sha512_compress)
	W64_ENTRY(3, 0)

	sub	$184, %rsp
	mov	%rbx, 128(%rsp)
	mov	STATE, 136(%rsp)	C Save state, to free a register
	mov	%rbp, 144(%rsp)
	mov	%r12, 152(%rsp)
	mov	%r13, 160(%rsp)
	mov	%r14, 168(%rsp)
	mov	%r15, 176(%rsp)

	mov	(STATE),   SA
	mov	8(STATE),  SB
	mov	16(STATE),  SC
	mov	24(STATE), SD
	mov	32(STATE), SE
	mov	40(STATE), SF
	mov	48(STATE), SG
	mov	56(STATE), SH
	xor	COUNT, COUNT
	ALIGN(16)

.Loop1:
	NOEXPN(0) ROUND(SA,SB,SC,SD,SE,SF,SG,SH,0)
	NOEXPN(1) ROUND(SH,SA,SB,SC,SD,SE,SF,SG,1)
	NOEXPN(2) ROUND(SG,SH,SA,SB,SC,SD,SE,SF,2)
	NOEXPN(3) ROUND(SF,SG,SH,SA,SB,SC,SD,SE,3)
	NOEXPN(4) ROUND(SE,SF,SG,SH,SA,SB,SC,SD,4)
	NOEXPN(5) ROUND(SD,SE,SF,SG,SH,SA,SB,SC,5)
	NOEXPN(6) ROUND(SC,SD,SE,SF,SG,SH,SA,SB,6)
	NOEXPN(7) ROUND(SB,SC,SD,SE,SF,SG,SH,SA,7)
	add	$8, COUNT
	cmp	$16, COUNT
	jne	.Loop1

.Loop2:
	EXPN( 0) ROUND(SA,SB,SC,SD,SE,SF,SG,SH,0)
	EXPN( 1) ROUND(SH,SA,SB,SC,SD,SE,SF,SG,1)
	EXPN( 2) ROUND(SG,SH,SA,SB,SC,SD,SE,SF,2)
	EXPN( 3) ROUND(SF,SG,SH,SA,SB,SC,SD,SE,3)
	EXPN( 4) ROUND(SE,SF,SG,SH,SA,SB,SC,SD,4)
	EXPN( 5) ROUND(SD,SE,SF,SG,SH,SA,SB,SC,5)
	EXPN( 6) ROUND(SC,SD,SE,SF,SG,SH,SA,SB,6)
	EXPN( 7) ROUND(SB,SC,SD,SE,SF,SG,SH,SA,7)
	EXPN( 8) ROUND(SA,SB,SC,SD,SE,SF,SG,SH,8)
	EXPN( 9) ROUND(SH,SA,SB,SC,SD,SE,SF,SG,9)
	EXPN(10) ROUND(SG,SH,SA,SB,SC,SD,SE,SF,10)
	EXPN(11) ROUND(SF,SG,SH,SA,SB,SC,SD,SE,11)
	EXPN(12) ROUND(SE,SF,SG,SH,SA,SB,SC,SD,12)
	EXPN(13) ROUND(SD,SE,SF,SG,SH,SA,SB,SC,13)
	EXPN(14) ROUND(SC,SD,SE,SF,SG,SH,SA,SB,14)
	EXPN(15) ROUND(SB,SC,SD,SE,SF,SG,SH,SA,15)
	add	$16, COUNT
	cmp	$80, COUNT
	jne	.Loop2

	mov	136(%rsp), STATE

	add	SA, (STATE)
	add	SB, 8(STATE)
	add	SC, 16(STATE)
	add	SD, 24(STATE)
	add	SE, 32(STATE)
	add	SF, 40(STATE)
	add	SG, 48(STATE)
	add	SH, 56(STATE)

	mov	128(%rsp), %rbx
	mov	144(%rsp), %rbp
	mov	152(%rsp), %r12
	mov	160(%rsp), %r13
	mov	168(%rsp),%r14
	mov	176(%rsp),%r15

	add	$184, %rsp
	W64_EXIT(3, 0)
	ret
EPILOGUE(_nettle_sha512_compress)
