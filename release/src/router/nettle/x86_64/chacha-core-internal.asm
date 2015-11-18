C x86_64/chacha-core-internal.asm

ifelse(<
   Copyright (C) 2012, 2014 Niels Möller

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

define(<DST>, <%rdi>)
define(<SRC>, <%rsi>)
define(<COUNT>, <%rdx>)
define(<X0>, <%xmm0>)
define(<X1>, <%xmm1>)
define(<X2>, <%xmm2>)
define(<X3>, <%xmm3>)
define(<T0>, <%xmm4>)
define(<T1>, <%xmm5>)

define(<USE_PSHUFW>, <yes>)

C ROTL_BY_16(REG, TMP)
ifelse(USE_PSHUFW, <yes>, <
define(<ROTL_BY_16>, <
	pshufhw	<$>0xb1, $1, $1
	pshuflw	<$>0xb1, $1, $1
>)>, <
define(<ROTL_BY_16>, <
	pslld	<$>16, $1
	psrld	<$>16, $2
	por	$2, $1
>)
>)
C QROUND
define(<QROUND>, <
	paddd	X1, X0
	pxor	X0, X3
	movaps	X3, T0
	ROTL_BY_16(X3, T0)

	paddd	X3, X2
	pxor	X2, X1
	movaps	X1, T0
	pslld	<$>12, X1
	psrld	<$>20, T0
	por	T0, X1

	paddd	X1, X0
	pxor	X0, X3
	movaps	X3, T0
	pslld	<$>8, X3
	psrld	<$>24, T0
	por	T0, X3
		
	paddd	X3, X2
	pxor	X2, X1
	movaps	X1, T0
	pslld	<$>7, X1
	psrld	<$>25, T0
	por	T0, X1
>)
	
	C _chacha_core(uint32_t *dst, const uint32_t *src, unsigned rounds)
	.text
	ALIGN(16)
PROLOGUE(_nettle_chacha_core)
	W64_ENTRY(3, 6)

	movups	(SRC), X0
	movups	16(SRC), X1
	movups	32(SRC), X2
	movups	48(SRC), X3

	shrl	$1, XREG(COUNT)

	ALIGN(16)
.Loop:
	QROUND(X0, X1, X2, X3)
	pshufd	$0x39, X1, X1
	pshufd	$0x4e, X2, X2
	pshufd	$0x93, X3, X3

	QROUND(X0, X1, X2, X3)
	pshufd	$0x93, X1, X1
	pshufd	$0x4e, X2, X2
	pshufd	$0x39, X3, X3

	decl	XREG(COUNT)
	jnz	.Loop

	movups	(SRC), T0
	movups	16(SRC), T1
	paddd	T0, X0
	paddd	T1, X1
	movups	X0,(DST)
	movups	X1,16(DST)
	movups	32(SRC), T0
	movups	48(SRC), T1
	paddd	T0, X2
	paddd	T1, X3
	movups	X2,32(DST)
	movups	X3,48(DST)
	W64_EXIT(3, 6)
	ret
EPILOGUE(_nettle_chacha_core)
