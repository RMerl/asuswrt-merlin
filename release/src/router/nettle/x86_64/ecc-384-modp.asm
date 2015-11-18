C x86_64/ecc-384-modp.asm

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

define(<RP>, <%rsi>)
define(<D4>, <%rax>)
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
define(<D0>, RP)	C Overlap
define(<TMP>, H4)	C Overlap

PROLOGUE(nettle_ecc_384_modp)
	W64_ENTRY(2, 0)

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	C First get top 2 limbs, which need folding twice
	C
	C   H5 H4
	C     -H5
	C  ------
	C   H0 D4
	C
	C Then shift right, (H1,H0,D4)  <--  (H0,D4) << 32
	C and add
	C
	C     H5 H4
	C     H1 H0
	C ----------
	C  C2 H1 H0

	mov	80(RP), D4
	mov	88(RP), H0
	mov	D4, H4
	mov	H0, H5
	sub	H0, D4
	sbb	$0, H0

	mov	D4, T2
	mov	H0, H1
	shl	$32, H0
	shr	$32, T2
	shr	$32, H1
	or	T2, H0

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

	C   H3 H2 H1 H0  0
	C - H4 H3 H2 H1 H0
	C  ---------------
	C   H3 H2 H1 H0 D0

	mov	XREG(D4), XREG(D4)
	mov	H0, D0
	neg	D0
	sbb	H1, H0
	sbb	H2, H1
	sbb	H3, H2
	sbb	H4, H3
	sbb	$0, D4

	C Shift right. High bits are sign, to be added to C0.
	mov	D4, TMP
	sar	$32, TMP
	shl	$32, D4
	add	TMP, C0

	mov	H3, TMP
	shr	$32, TMP
	shl	$32, H3
	or	TMP, D4

	mov	H2, TMP
	shr	$32, TMP
	shl	$32, H2
	or	TMP, H3

	mov	H1, TMP
	shr	$32, TMP
	shl	$32, H1
	or	TMP, H2

	mov	H0, TMP
	shr	$32, TMP
	shl	$32, H0
	or	TMP, H1

	mov	D0, TMP
	shr	$32, TMP
	shl	$32, D0
	or	TMP, H0

	add	D0, T0
	adc	H0, T1
	adc	H1, T2
	adc	H2, T3
	adc	H3, T4
	adc	D4, T5
	adc	$0, C0

	C Remains to add in C2 and C0
	C                         C0  C0<<32  (-2^32+1)C0
	C    C2  C2<<32  (-2^32+1)C2
	C where C2 is always positive, while C0 may be -1.
	mov	C0, H0
	mov	C0, H1
	mov	C0, H2
	sar	$63, C0		C Get sign
	shl	$32, H1
	sub	H1, H0		C Gives borrow iff C0 > 0
	sbb	$0, H1
	add	C0, H2

	add	H0, T0
	adc	H1, T1
	adc	$0, H2
	adc	$0, C0

	C Set (H1 H0)  <-- C2 << 96 - C2 << 32 + 1
	mov	C2, H0
	mov	C2, H1
	shl	$32, H1
	sub	H1, H0
	sbb	$0, H1

	add	H2, H0
	adc	C0, H1
	adc	C2, C0
	mov	C0, H2
	sar	$63, C0
	add	H0, T2
	adc	H1, T3
	adc	H2, T4
	adc	C0, T5
	sbb	C0, C0

	C Final unlikely carry
	mov	C0, H0
	mov	C0, H1
	mov	C0, H2
	sar	$63, C0
	shl	$32, H1
	sub	H1, H0
	sbb	$0, H1
	add	C0, H2

	pop	RP

	sub	H0, T0
	mov	T0, (RP)
	sbb	H1, T1
	mov	T1, 8(RP)
	sbb	H2, T2
	mov	T2, 16(RP)
	sbb	C0, T3
	mov	T3, 24(RP)
	sbb	C0, T4
	mov	T4, 32(RP)
	sbb	C0, T5
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
