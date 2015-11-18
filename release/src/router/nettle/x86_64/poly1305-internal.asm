C x86_64/poly1305-internal.asm

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

	.file "poly1305-internal.asm"

C Registers mainly used by poly1305_block
define(<CTX>, <%rdi>)
define(<T0>, <%rcx>)
define(<T1>, <%rsi>)
define(<T2>, <%r8>)
define(<H0>, <%r9>)
define(<H1>, <%r10>)
define(<H2>, <%r11>)
	
	C poly1305_set_key(struct poly1305_ctx *ctx, const uint8_t key[16])
	.text
	C Registers:
	C  %rdi: ctx
	C  %rsi: key
	C  %r8: mask
	ALIGN(16)
PROLOGUE(nettle_poly1305_set_key)
	W64_ENTRY(2,0)
	mov	$0x0ffffffc0fffffff, %r8
	mov	(%rsi), %rax
	and	%r8, %rax
	and	$-4, %r8
	mov	%rax, (CTX)
	mov	8(%rsi), %rax
	and	%r8, %rax
	mov	%rax, P1305_R1 (CTX)
	shr	$2, %rax
	imul	$5, %rax
	mov	%rax, P1305_S1 (CTX)
	xor	XREG(%rax), XREG(%rax)
	mov	%rax, P1305_H0 (CTX)
	mov	%rax, P1305_H1 (CTX)
	mov	XREG(%rax), P1305_H2 (CTX)
	
	W64_EXIT(2,0)
	ret

EPILOGUE(nettle_poly1305_set_key)

C 64-bit multiplication mod 2^130 - 5
C
C (x_0 + B x_1 + B^2 x_2) * (r_0 + B r_1) =
C     1   B B^2 B^3 
C   x_0 r_0
C       x_0 r_1
C	x_1 r_0
C	    x_1 r_1
C	    x_2 r_0
C               x_2 r_1
C Then r_1 B^2 = r_1/4 (2^130) = 5/4 r_1.
C and  r_1 B^3 = 5/4 B r_1
C So we get
C
C  x_0 r_0 + x_1 (5/4 r_1) + B (x_0 r_1 + x_1 r_0 + x_2 5/4 r_1 + B x_2 r_0)
C     1   B B^2 B^3 
C   x_0 r_0
C   x_1 r'_1
C       x_0 r_1
C	x_1 r_0
C       x_2 r'_1
C           x_2 r_0

	C _poly1305_block (struct poly1305_ctx *ctx, const uint8_t m[16], unsigned hi)
	
PROLOGUE(_nettle_poly1305_block)
	W64_ENTRY(3, 0)
	mov	(%rsi), T0
	mov	8(%rsi), T1
	mov	XREG(%rdx),	XREG(T2)

	C Registers:
	C Inputs:  CTX, T0, T1, T2,
	C Outputs: H0, H1, H2, stored into the context.

	add	P1305_H0 (CTX), T0
	adc	P1305_H1 (CTX), T1
	adc	P1305_H2 (CTX), XREG(T2)
	mov	P1305_R0 (CTX), %rax
	mul	T0			C x0*r0
	mov	%rax, H0
	mov	%rdx, H1
	mov	P1305_S1 (CTX), %rax	C 5/4 r1
	mov	%rax, H2
	mul	T1			C x1*r1'
	imul	T2, H2			C x2*r1'
	imul	P1305_R0 (CTX), T2	C x2*r0
	add	%rax, H0
	adc	%rdx, H1
	mov	P1305_R0 (CTX), %rax
	mul	T1			C x1*r0
	add	%rax, H2
	adc	%rdx, T2
	mov	P1305_R1 (CTX), %rax
	mul	T0			C x0*r1
	add	%rax, H2
	adc	%rdx, T2
	mov	T2, %rax
	shr	$2, %rax
	imul	$5, %rax
	and	$3, XREG(T2)
	add	%rax, H0
	adc	H2, H1
	adc	$0, XREG(T2)
	mov	H0, P1305_H0 (CTX)
	mov	H1, P1305_H1 (CTX)
	mov	XREG(T2), P1305_H2 (CTX)
	W64_EXIT(3, 0)
	ret
EPILOGUE(_nettle_poly1305_block)

	C poly1305_digest (struct poly1305_ctx *ctx, uint8_t *s)
	C Registers:
	C   %rdi: ctx
	C   %rsi: s
	
PROLOGUE(nettle_poly1305_digest)
	W64_ENTRY(2, 0)

	mov	P1305_H0 (CTX), H0
	mov	P1305_H1 (CTX), H1
	mov	P1305_H2 (CTX), XREG(H2)
	mov	XREG(H2), XREG(%rax)
	shr	$2, XREG(%rax)
	and	$3, H2
	imul	$5, XREG(%rax)
	add	%rax, H0
	adc	$0, H1
	adc	$0, XREG(H2)

C Use %rax instead of %rsi
define(<T1>, <%rax>)
	C Add 5, use result if >= 2^130
	mov	$5, T0
	xor	T1, T1
	add	H0, T0
	adc	H1, T1
	adc	$0, XREG(H2)
	cmp	$4, XREG(H2)
	cmovnc	T0, H0
	cmovnc	T1, H1

	add	H0, (%rsi)
	adc	H1, 8(%rsi)

	xor	XREG(%rax), XREG(%rax)
	mov	%rax, P1305_H0 (CTX)
	mov	%rax, P1305_H1 (CTX)
	mov	XREG(%rax), P1305_H2 (CTX)
	W64_EXIT(2, 0)
	ret

