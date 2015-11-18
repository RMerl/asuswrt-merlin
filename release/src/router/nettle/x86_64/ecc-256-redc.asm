C x86_64/ecc-256-redc.asm

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

define(<RP>, <%rsi>)
define(<U0>, <%rdi>) C Overlaps unused modulo input
define(<U1>, <%rcx>)
define(<U2>, <%rax>)
define(<U3>, <%rdx>)
define(<U4>, <%r8>)
define(<U5>, <%r9>)
define(<U6>, <%r10>)
define(<F0>, <%r11>)
define(<F1>, <%r12>)
define(<F2>, <%rbx>)
define(<F3>, <%rbp>)

C FOLD(x), sets (F3,F2,F1,F0)  <-- (x << 224) - (x << 128) - (x<<32)
define(<FOLD>, <
	mov	$1, F2
	mov	$1, F3
	shl	<$>32, F2
	shr	<$>32, F3
	xor	F0,F0
	xor	F1,F1
	sub	F2, F0
	sbb	F3, F1
	sbb	$1, F2
	sbb	<$>0, F3
>)
PROLOGUE(nettle_ecc_256_redc)
	W64_ENTRY(2, 0)
	C save all registers that need to be saved
	push	%rbx
	push	%rbp
	push	%r12

	mov	(RP), U0
	FOLD(U0)
	mov	8(RP), U1
	mov	16(RP), U2
	mov	24(RP), U3
	sub	F0, U1
	sbb	F1, U2
	sbb	F2, U3
	sbb	F3, U0		C Add in later

	FOLD(U1)
	mov	32(RP), U4
	sub	F0, U2
	sbb	F1, U3
	sbb	F2, U4
	sbb	F3, U1

	FOLD(U2)
	mov	40(RP), U5
	sub	F0, U3
	sbb	F1, U4
	sbb	F2, U5
	sbb	F3, U2

	FOLD(U3)
	mov	48(RP), U6
	sub	F0, U4
	sbb	F1, U5
	sbb	F2, U6
	sbb	F3, U3

	add	U4, U0
	adc	U5, U1
	adc	U6, U2
	adc	56(RP), U3

	C If carry, we need to add in
	C 2^256 - p = <0xfffffffe, 0xff..ff, 0xffffffff00000000, 1>
	sbb	F2, F2
	mov	F2, F0
	mov	F2, F1
	mov	XREG(F2), XREG(F3)
	neg	F0
	shl	$32, F1
	and	$-2, XREG(F3)

	add	F0, U0
	mov	U0, (RP)
	adc	F1, U1
	mov	U1, 8(RP)
	adc	F2, U2
	mov	U2, 16(RP)
	adc	F3, U3

	mov	U3, 24(RP)

	pop	%r12
	pop	%rbp
	pop	%rbx
	W64_EXIT(2, 0)
	ret
EPILOGUE(nettle_ecc_256_redc)
