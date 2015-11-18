C x86_64/ecc-25519-modp.asm

ifelse(<
   Copyright (C) 2014 Niels Möller

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

	.file "ecc-25519-modp.asm"

define(<RP>, <%rsi>)
define(<U0>, <%rdi>)	C Overlaps unused modulo input
define(<U1>, <%rcx>)
define(<U2>, <%r8>)
define(<U3>, <%r9>)
define(<T0>, <%r10>)
define(<T1>, <%r11>)
define(<M>, <%rbx>)

PROLOGUE(nettle_ecc_25519_modp)
	W64_ENTRY(2, 0)
	push	%rbx

	C First fold the limbs affecting bit 255
	mov	56(RP), %rax
	mov	$38, M
	mul	M
	mov	24(RP), U3
	xor	T0, T0
	add	%rax, U3
	adc	%rdx, T0

	mov	40(RP), %rax	C Do this early as possible
	mul	M
	
	add	U3, U3
	adc	T0, T0
	shr	U3		C Undo shift, clear high bit

	C Fold the high limb again, together with RP[5]
	imul	$19, T0

	mov	(RP), U0
	mov	8(RP), U1
	mov	16(RP), U2
	add	T0, U0
	adc	%rax, U1
	mov	32(RP), %rax
	adc	%rdx, U2
	adc	$0, U3

	C Fold final two limbs, RP[4] and RP[6]
	mul	M
	mov	%rax, T0
	mov	48(RP), %rax
	mov	%rdx, T1
	mul	M
	add	T0, U0
	mov	U0, (RP)
	adc	T1, U1
	mov	U1, 8(RP)
	adc	%rax, U2
	mov	U2, 16(RP)
	adc	%rdx, U3
	mov	U3, 24(RP)

	pop	%rbx
	W64_EXIT(2, 0)
	ret
EPILOGUE(nettle_ecc_25519_modp)
