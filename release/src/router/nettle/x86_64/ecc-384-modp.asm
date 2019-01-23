C x86_64/ecc-384-modp.asm

ifelse(<
   Copyright (C) 2013, 2015 Niels Möller

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

define(<RP>, <%rsi>)
define(<D5>, <%rax>)
define(<T0>, <%rbx>)
define(<T1>, <%rcx>)
define(<T2>, <%rdx>)
define(<T3>, <%rbp>)
define(<T4>, <%rdi>)
define(<T5>, <%r8>)
define(<H0>, <%r9>)
define(<H1>, <%r10>)
define(<H2>, <%r11>)
define(<H3>, <%r12>)
define(<H4>, <%r13>)
define(<H5>, <%r14>)
define(<C2>, <%r15>)
define(<C0>, H5)	C Overlap
define(<TMP>, RP)	C Overlap


PROLOGUE(nettle_ecc_384_modp)
	W64_ENTRY(2, 0)

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	C First get top 2 limbs, which need folding twice.
	C B^10 = B^6 + B^4 + 2^32 (B-1)B^4.
	C We handle the terms as follow:
	C
	C B^6: Folded immediatly.
	C
	C B^4: Delayed, added in in the next folding.
	C
	C 2^32(B-1) B^4: Low half limb delayed until the next
	C folding. Top 1.5 limbs subtracted and shifter now, resulting
	C in 2.5 limbs. The low limb saved in D5, high 1.5 limbs added
	C in.

	mov	80(RP), H4
	mov	88(RP), H5
	C Shift right 32 bits, into H1, H0
	mov	H4, H0
	mov	H5, H1
	mov	H5, D5
	shr	$32, H1
	shl	$32, D5
	shr	$32, H0
	or	D5, H0

	C	H1 H0
	C       -  H1 H0
	C       --------
	C       H1 H0 D5
	mov	H0, D5
	neg	D5
	sbb	H1, H0
	sbb	$0, H1

	xor	C2, C2
	add	H4, H0
	adc	H5, H1
	adc	$0, C2

	C Add in to high part
	add	48(RP), H0
	adc	56(RP), H1
	adc	$0, C2		C Do C2 later

	C +1 term
	mov	(RP), T0
	add	H0, T0
	mov	8(RP), T1
	adc	H1, T1
	mov	16(RP), T2
	mov	64(RP), H2
	adc	H2, T2
	mov	24(RP), T3
	mov	72(RP), H3
	adc	H3, T3
	mov	32(RP), T4
	adc	H4, T4
	mov	40(RP), T5
	adc	H5, T5
	sbb	C0, C0
	neg	C0		C FIXME: Switch sign of C0?

	push	RP

	C +B^2 term
	add	H0, T2
	adc	H1, T3
	adc	H2, T4
	adc	H3, T5
	adc	$0, C0

	C Shift left, including low half of H4
	mov	H3, TMP
	shl	$32, H4
	shr	$32, TMP
	or	TMP, H4

	mov	H2, TMP
	shl	$32, H3
	shr	$32, TMP
	or	TMP, H3

	mov	H1, TMP
	shl	$32, H2
	shr	$32, TMP
	or	TMP, H2

	mov	H0, TMP
	shl	$32, H1
	shr	$32, TMP
	or	TMP, H1

	shl	$32, H0

	C   H4 H3 H2 H1 H0  0
	C  -   H4 H3 H2 H1 H0
	C  ---------------
	C   H4 H3 H2 H1 H0 TMP

	mov	H0, TMP
	neg	TMP
	sbb	H1, H0
	sbb	H2, H1
	sbb	H3, H2
	sbb	H4, H3
	sbb	$0, H4

	add	TMP, T0
	adc	H0, T1
	adc	H1, T2
	adc	H2, T3
	adc	H3, T4
	adc	H4, T5
	adc	$0, C0

	C Remains to add in C2 and C0
	C Set H1, H0 = (2^96 - 2^32 + 1) C0
	mov	C0, H0
	mov	C0, H1
	shl	$32, H1
	sub	H1, H0
	sbb	$0, H1

	C Set H3, H2 = (2^96 - 2^32 + 1) C2
	mov	C2, H2
	mov	C2, H3
	shl	$32, H3
	sub	H3, H2
	sbb	$0, H3
	add	C0, H2		C No carry. Could use lea trick

	xor	C0, C0
	add	H0, T0
	adc	H1, T1
	adc	H2, T2
	adc	H3, T3
	adc	C2, T4
	adc	D5, T5		C Value delayed from initial folding
	adc	$0, C0		C Use sbb and switch sign?

	C Final unlikely carry
	mov	C0, H0
	mov	C0, H1
	shl	$32, H1
	sub	H1, H0
	sbb	$0, H1

	pop	RP

	add	H0, T0
	mov	T0, (RP)
	adc	H1, T1
	mov	T1, 8(RP)
	adc	C0, T2
	mov	T2, 16(RP)
	adc	$0, T3
	mov	T3, 24(RP)
	adc	$0, T4
	mov	T4, 32(RP)
	adc	$0, T5
	mov	T5, 40(RP)

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx

	W64_EXIT(2, 0)
	ret
EPILOGUE(nettle_ecc_384_modp)
