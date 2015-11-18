C x86_64/salsa20-crypt.asm

ifelse(<
   Copyright (C) 2012 Niels Möller

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

define(<CTX>, <%rdi>)
define(<LENGTH>, <%rsi>)
define(<DST>, <%rdx>)
define(<SRC>, <%rcx>)
define(<T64>, <%r8>)
define(<POS>, <%r9>)
define(<X0>, <%xmm0>)
define(<X1>, <%xmm1>)
define(<X2>, <%xmm2>)
define(<X3>, <%xmm3>)
define(<T0>, <%xmm4>)
define(<T1>, <%xmm5>)
define(<M0101>, <%xmm6>)
define(<M0110>, <%xmm7>)
define(<M0011>, <%xmm8>)
define(<COUNT>, <%rax>)

include_src(<x86_64/salsa20.m4>)

C Possible improvements:
C 
C Do two blocks (or more) at a time in parallel, to avoid limitations
C due to data dependencies.
C 
C Avoid redoing the permutation of the input for each block (all but
C the two counter words are constant). Could also keep the input in
C registers.

	.file "salsa20-crypt.asm"
	
	C salsa20_crypt(struct salsa20_ctx *ctx, size_t length,
	C		uint8_t *dst, const uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(nettle_salsa20_crypt)
	W64_ENTRY(4, 9)	

	test	LENGTH, LENGTH
	jz	.Lend

	C Load mask registers
	mov	$-1, XREG(COUNT)
	movd	XREG(COUNT), M0101
	pshufd	$0x09, M0101, M0011	C 01 01 00 00
	pshufd	$0x41, M0101, M0110	C 01 00 00 01
	pshufd	$0x22, M0101, M0101	C 01 00 01 00
	
.Lblock_loop:
	movups	(CTX), X0
	movups	16(CTX), X1
	movups	32(CTX), X2
	movups	48(CTX), X3

	C On input, each xmm register is one row. We start with
	C
	C	 0  1  2  3     C K K K
	C	 4  5  6  7	K C I I
	C	 8  9 10 11	B B C K
	C	12 13 14 15	K K K C
	C
	C Diagrams are in little-endian order, with least significant word to
	C the left. We rotate the columns, to get instead
	C
	C	 0  5 10 15	C C C C
	C	 4  9 14  3	K B K K
	C	 8 13  2  7	B K K I
	C	12  1  6 11	K K I K
	C 
	C The original rows are now diagonals.
	SWAP(X0, X1, M0101)
	SWAP(X2, X3, M0101)
	SWAP(X1, X3, M0110)
	SWAP(X0, X2, M0011)	

	movl	$10, XREG(COUNT)
	ALIGN(16)
.Loop:
	QROUND(X0, X1, X2, X3)
	C For the row operations, we first rotate the rows, to get
	C	
	C	0 5 10 15
	C	3 4  9 14
	C	2 7  8 13
	C	1 6 11 12
	C 
	C Now the original rows are turned into into columns. (This
	C SIMD hack described in djb's papers).

	pshufd	$0x93, X1, X1	C	11 00 01 10 (least sign. left)
	pshufd	$0x4e, X2, X2	C	10 11 00 01
	pshufd	$0x39, X3, X3	C	01 10 11 00

	QROUND(X0, X3, X2, X1)

	C Inverse rotation of the rows
	pshufd	$0x39, X1, X1	C	01 10 11 00
	pshufd	$0x4e, X2, X2	C	10 11 00 01
	pshufd	$0x93, X3, X3	C	11 00 01 10

	decl	XREG(COUNT)
	jnz	.Loop

	SWAP(X0, X2, M0011)	
	SWAP(X1, X3, M0110)
	SWAP(X0, X1, M0101)
	SWAP(X2, X3, M0101)

	movups	(CTX), T0
	movups	16(CTX), T1
	paddd	T0, X0
	paddd	T1, X1
	movups	32(CTX), T0
	movups	48(CTX), T1
	paddd	T0, X2
	paddd	T1, X3

	C Increment block counter
	incq	32(CTX)

	cmp	$64, LENGTH
	jc	.Lfinal_xor

	movups	48(SRC), T1
	pxor	T1, X3
	movups	X3, 48(DST)
.Lxor3:
	movups	32(SRC), T0
	pxor	T0, X2
	movups	X2, 32(DST)
.Lxor2:
	movups	16(SRC), T1
	pxor	T1, X1
	movups	X1, 16(DST)
.Lxor1:
	movups	(SRC), T0	
	pxor	T0, X0
	movups	X0, (DST)

	lea	64(SRC), SRC
	lea	64(DST), DST
	sub	$64, LENGTH
	ja	.Lblock_loop
.Lend:
	W64_EXIT(4, 9)
	ret

.Lfinal_xor:
	cmp	$32, LENGTH
	jz	.Lxor2
	jc	.Llt32
	cmp	$48, LENGTH
	jz	.Lxor3
	jc	.Llt48
	movaps	X3, T0
	call	.Lpartial
	jmp	.Lxor3
.Llt48:
	movaps	X2, T0
	call	.Lpartial
	jmp	.Lxor2
.Llt32:
	cmp	$16, LENGTH
	jz	.Lxor1
	jc	.Llt16
	movaps	X1, T0
	call	.Lpartial
	jmp	.Lxor1
.Llt16:
	movaps	X0, T0
	call	.Lpartial
	jmp	.Lend

.Lpartial:
	mov	LENGTH, POS
	and	$-16, POS
	test	$8, LENGTH
	jz	.Llt8
	C This "movd" instruction should assemble to
	C 66 49 0f 7e e0          movq   %xmm4,%r8
	C Apparently, assemblers treat movd and movq (with the
	C arguments we use) in the same way, except for osx, which
	C barfs at movq.
	movd	T0, T64
	xor	(SRC, POS), T64
	mov	T64, (DST, POS)
	lea	8(POS), POS
	pshufd	$0xee, T0, T0		C 10 11 10 11
.Llt8:
	C And this is also really a movq.
	movd	T0, T64
	test	$4, LENGTH
	jz	.Llt4
	mov	XREG(T64), XREG(COUNT)
	xor	(SRC, POS), XREG(COUNT)
	mov	XREG(COUNT), (DST, POS)
	lea	4(POS), POS
	shr	$32, T64
.Llt4:
	test	$2, LENGTH
	jz	.Llt2
	mov	WREG(T64), WREG(COUNT)
	xor	(SRC, POS), WREG(COUNT)
	mov	WREG(COUNT), (DST, POS)
	lea	2(POS), POS
	shr	$16, XREG(T64)
.Llt2:
	test	$1, LENGTH
	jz	.Lret
	xor	(SRC, POS), LREG(T64)
	mov	LREG(T64), (DST, POS)

.Lret:
	ret

EPILOGUE(nettle_salsa20_crypt)
