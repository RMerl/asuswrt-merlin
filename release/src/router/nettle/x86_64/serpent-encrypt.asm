C x86_64/serpent-encrypt.asm

ifelse(<
   Copyright (C) 2011 Niels Möller

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

include_src(<x86_64/serpent.m4>)

C Register usage:

C Single block serpent state, two copies
define(<x0>, <%eax>)
define(<x1>, <%ebx>)
define(<x2>, <%ebp>)
define(<x3>, <%r8d>)

define(<y0>, <%r9d>)
define(<y1>, <%r10d>)
define(<y2>, <%r11d>)
define(<y3>, <%r12d>)

C Quadruple block serpent state, two copies
define(<X0>, <%xmm0>)
define(<X1>, <%xmm1>)
define(<X2>, <%xmm2>)
define(<X3>, <%xmm3>)

define(<Y0>, <%xmm4>)
define(<Y1>, <%xmm5>)
define(<Y2>, <%xmm6>)
define(<Y3>, <%xmm7>)

define(<MINUS1>, <%xmm8>)
define(<T0>, <%xmm9>)
define(<T1>, <%xmm10>)
define(<T2>, <%xmm11>)
define(<T3>, <%xmm12>)

C Arguments
define(<CTX>, <%rdi>)
define(<N>, <%rsi>)
define(<DST>, <%rdx>)
define(<SRC>, <%rcx>)

define(<CNT>, <%r13>)
define(<TMP32>, <%r14d>)

C SBOX macros. Inputs $1 - $4 (destroyed), outputs $5 - $8

define(<SBOX0>, <
	mov	$2, $8	C y3  = x1 ^ x2
	xor 	$3, $8
	mov	$1, $5	C y0  = x0 | x3
	or	$4, $5
	mov	$1, $6	C y1  = x0 ^ x1
	xor	$2, $6
	xor	$5, $8	C y3 ^= y0
	mov	$3, $7	C y2  = x2 | y3
	or	$8, $7
	xor	$4, $1	C x0 ^= x3
	and	$4, $7	C y2 &= x3
	xor	$3, $4	C x3 ^= x2
	or	$2, $3	C x2 |= x1
	mov	$6, $5	C y0  = y1 & x2
	and	$3, $5
	xor	$5, $7	C y2 ^= y0
	and	$7, $5	C y0 &= y2
	xor	$3, $5	C y0 ^= x2
	and	$1, $2	C x1 &= x0
	xor	$1, $5	C y0 ^= x0
	not	$5	C y0  = ~y0
	mov	$5, $6	C y1  = y0 ^ x1
	xor	$2, $6
	xor	$4, $6	C y1 ^= x3
>)

define(<SBOX1>, <
	mov	$1, $6	C y1  = x0 | x3
	or	$4, $6 
	mov	$3, $7	C y2  = x2 ^ x3
	xor	$4, $7
	mov	$2, $5	C y0  = ~x1
	not	$5
	mov	$1, $8	C y3  = x0 ^ x2
	xor	$3, $8
	or	$1, $5	C y0 |= x0
	and	$4, $8	C y3 &= x3
	mov	$6, $1	C x0  = y1 & y2
	and	$7, $1
	or	$2, $8	C y3 |= x1
	xor	$5, $7	C y2 ^= y0
	xor	$1, $8	C y3 ^= x0
	mov	$6, $1	C x0  = y1 ^ y3
	xor	$8, $1
	xor	$7, $1	C x0 ^= y2
	mov	$2, $6	C y1  = x1 & x3
	and	$4, $6
	xor	$1, $6	C y1 ^= x0
	mov	$6, $4	C x3  = y1 | y3
	or	$8, $4
	not	$8	C y3  = ~y3
	and 	$4, $5	C y0 &= x3
	xor	$3, $5	C y0 ^= x2
>)

define(<SBOX2>, <
	mov	$1, $7	C y2 = x1 | x2
	or	$3, $7
	mov	$1, $6
	xor	$2, $6
	mov	$4, $8
	xor	$7, $8
	mov	$6, $5
	xor	$8, $5
	or	$1, $4
	xor	$5, $3
	mov	$2, $1
	xor	$3, $1
	or	$2, $3
	and	$7, $1
	xor	$3, $8
	or	$8, $6
	xor	$1, $6
	mov	$8, $7
	xor	$6, $7
	xor	$2, $7
	not	$8
	xor	$4, $7
>)

define(<SBOX3>, <
	mov	$1, $6
	xor	$3, $6
	mov	$1, $5
	or	$4, $5
	mov	$1, $8
	and	$4, $8
	and	$5, $6
	or	$2, $8
	mov	$1, $7
	and	$2, $7
	or	$3, $7
	mov	$4, $3
	xor	$6, $3
	xor	$8, $6
	or	$3, $1
	xor	$2, $3
	and	$4, $8
	xor	$8, $5
	mov	$7, $8
	xor	$3, $8
	xor	$5, $7
	or	$8, $4
	and	$4, $2
	mov	$1, $5
	xor	$2, $5
>)

define(<SBOX4>, <
	mov	$1, $8
	or	$2, $8
	mov	$2, $7
	or	$3, $7
	xor	$1, $7
	and	$4, $8
	mov	$2, $5
	xor	$4, $5
	or	$7, $4
	and	$4, $1
	and	$3, $2
	xor	$8, $3
	xor	$7, $8
	or	$2, $7
	mov	$8, $6
	and	$5, $6
	xor	$6, $7
	xor	$5, $6
	or	$2, $6
	xor	$1, $6
	and	$4, $5
	xor	$3, $5
	not	$5
>)

define(<SBOX5>, <
	mov	$2, $5
	or	$4, $5
	xor	$3, $5
	mov	$2, $3
	xor	$4, $3
	mov	$1, $7
	xor	$3, $7
	and	$3, $1
	xor	$1, $5
	mov	$2, $8
	or	$7, $8
	or	$5, $2
	not	$5
	or	$5, $1
	xor	$3, $8
	xor	$1, $8
	mov	$4, $6
	or	$5, $6
	xor	$6, $4
	xor	$7, $6
	or	$4, $7
	xor	$2, $7
>)

define(<SBOX6>, <
	mov	$1, $5
	xor	$4, $5
	mov	$1, $6
	and	$4, $6
	mov	$1, $7
	or	$3, $7
	or	$2, $4
	xor	$3, $4
	xor	$2, $1
	mov	$2, $8
	or	$3, $8
	xor	$2, $3
	and	$5, $8
	xor	$3, $6
	not	$6
	and	$6, $5
	and	$6, $2
	xor	$8, $2
	xor	$4, $8
	xor	$2, $7
	not	$7
	xor	$7, $5
	xor	$1, $5
>)

define(<SBOX7>, <
	mov	$1, $5
	and	$3, $5
	mov	$2, $8
	or	$5, $8	C t04
	xor	$3, $8
	mov	$4, $6
	not	$6	C t02	
	and	$1, $6
	xor	$6, $8
	mov	$3, $6
	or	$8, $6
	xor	$1, $6
	mov	$1, $7
	and	$2, $7
	xor	$7, $3
	or	$4, $7
	xor	$7, $6
	mov	$2, $7
	or	$5, $7	C t04
	and	$8, $7
	xor	$6, $2
	or	$2, $7
	xor	$1, $7
	xor	$6, $5
	not	$4	C t02
	or	$4, $5
	xor	$3, $5
>)

define(<LT>, <
	rol	<$>13, $1
	rol	<$>3, $3
	xor	$1, $2
	xor	$3, $2
	mov	$1, TMP32
	shl	<$>3, TMP32
	xor	$3, $4
	xor	TMP32, $4
	rol	$2
	rol	<$>7, $4
	xor	$2, $1
	xor	$4, $1
	mov	$2, TMP32
	shl	<$>7, TMP32
	xor	$4, $3
	xor	TMP32, $3
	rol	<$>5, $1
	rol	<$>22, $3
>)

C Parallel operation on four blocks at a time.

C pnot instruction is missing. For lack of a spare register, XOR with
C constant in memory.
	
define(<PNOT>, <
	pxor	MINUS1, $1
>)

define(<WSBOX0>, <
	movdqa	$2, $8	C y3  = x1 ^ x2
	pxor 	$3, $8
	movdqa	$1, $5	C y0  = x0 | x3
	por	$4, $5
	movdqa	$1, $6	C y1  = x0 ^ x1
	pxor	$2, $6
	pxor	$5, $8	C y3 ^= y0
	movdqa	$3, $7	C y2  = x2 | y3
	por	$8, $7
	pxor	$4, $1	C x0 ^= x3
	pand	$4, $7	C y2 &= x3
	pxor	$3, $4	C x3 ^= x2
	por	$2, $3	C x2 |= x1
	movdqa	$6, $5	C y0  = y1 & x2
	pand	$3, $5
	pxor	$5, $7	C y2 ^= y0
	pand	$7, $5	C y0 &= y2
	pxor	$3, $5	C y0 ^= x2
	pand	$1, $2	C x1 &= x0
	pxor	$1, $5	C y0 ^= x0
	PNOT($5)	C y0  = ~y0
	movdqa	$5, $6	C y1  = y0 ^ x1
	pxor	$2, $6
	pxor	$4, $6	C y1 ^= x3
>)

define(<WSBOX1>, <
	movdqa	$1, $6	C y1  = x0 | x3
	por	$4, $6 
	movdqa	$3, $7	C y2  = x2 ^ x3
	pxor	$4, $7
	movdqa	$2, $5	C y0  = ~x1
	PNOT($5)
	movdqa	$1, $8	C y3  = x0 ^ x2
	pxor	$3, $8
	por	$1, $5	C y0 |= x0
	pand	$4, $8	C y3 &= x3
	movdqa	$6, $1	C x0  = y1 & y2
	pand	$7, $1
	por	$2, $8	C y3 |= x1
	pxor	$5, $7	C y2 ^= y0
	pxor	$1, $8	C y3 ^= x0
	movdqa	$6, $1	C x0  = y1 ^ y3
	pxor	$8, $1
	pxor	$7, $1	C x0 ^= y2
	movdqa	$2, $6	C y1  = x1 & x3
	pand	$4, $6
	pxor	$1, $6	C y1 ^= x0
	movdqa	$6, $4	C x3  = y1 | y3
	por	$8, $4
	PNOT($8)	C y3  = ~y3
	pand 	$4, $5	C y0 &= x3
	pxor	$3, $5	C y0 ^= x2
>)

define(<WSBOX2>, <
	movdqa	$1, $7	C y2 = x1 | x2
	por	$3, $7
	movdqa	$1, $6
	pxor	$2, $6
	movdqa	$4, $8
	pxor	$7, $8
	movdqa	$6, $5
	pxor	$8, $5
	por	$1, $4
	pxor	$5, $3
	movdqa	$2, $1
	pxor	$3, $1
	por	$2, $3
	pand	$7, $1
	pxor	$3, $8
	por	$8, $6
	pxor	$1, $6
	movdqa	$8, $7
	pxor	$6, $7
	pxor	$2, $7
	PNOT($8)
	pxor	$4, $7
>)

define(<WSBOX3>, <
	movdqa	$1, $6
	pxor	$3, $6
	movdqa	$1, $5
	por	$4, $5
	movdqa	$1, $8
	pand	$4, $8
	pand	$5, $6
	por	$2, $8
	movdqa	$1, $7
	pand	$2, $7
	por	$3, $7
	movdqa	$4, $3
	pxor	$6, $3
	pxor	$8, $6
	por	$3, $1
	pxor	$2, $3
	pand	$4, $8
	pxor	$8, $5
	movdqa	$7, $8
	pxor	$3, $8
	pxor	$5, $7
	por	$8, $4
	pand	$4, $2
	movdqa	$1, $5
	pxor	$2, $5
>)

define(<WSBOX4>, <
	movdqa	$1, $8
	por	$2, $8
	movdqa	$2, $7
	por	$3, $7
	pxor	$1, $7
	pand	$4, $8
	movdqa	$2, $5
	pxor	$4, $5
	por	$7, $4
	pand	$4, $1
	pand	$3, $2
	pxor	$8, $3
	pxor	$7, $8
	por	$2, $7
	movdqa	$8, $6
	pand	$5, $6
	pxor	$6, $7
	pxor	$5, $6
	por	$2, $6
	pxor	$1, $6
	pand	$4, $5
	pxor	$3, $5
	PNOT($5)
>)

define(<WSBOX5>, <
	movdqa	$2, $5
	por	$4, $5
	pxor	$3, $5
	movdqa	$2, $3
	pxor	$4, $3
	movdqa	$1, $7
	pxor	$3, $7
	pand	$3, $1
	pxor	$1, $5
	movdqa	$2, $8
	por	$7, $8
	por	$5, $2
	PNOT($5)
	por	$5, $1
	pxor	$3, $8
	pxor	$1, $8
	movdqa	$4, $6
	por	$5, $6
	pxor	$6, $4
	pxor	$7, $6
	por	$4, $7
	pxor	$2, $7
>)

define(<WSBOX6>, <
	movdqa	$1, $5
	pxor	$4, $5
	movdqa	$1, $6
	pand	$4, $6
	movdqa	$1, $7
	por	$3, $7
	por	$2, $4
	pxor	$3, $4
	pxor	$2, $1
	movdqa	$2, $8
	por	$3, $8
	pxor	$2, $3
	pand	$5, $8
	pxor	$3, $6
	PNOT($6)
	pand	$6, $5
	pand	$6, $2
	pxor	$8, $2
	pxor	$4, $8
	pxor	$2, $7
	PNOT($7)
	pxor	$7, $5
	pxor	$1, $5
>)

define(<WSBOX7>, <
	movdqa	$1, $5
	pand	$3, $5
	movdqa	$2, $8
	por	$5, $8	C t04
	pxor	$3, $8
	movdqa	$4, $6
	pandn	$1, $6	C t02 implicit
	pxor	$6, $8
	movdqa	$3, $6
	por	$8, $6
	pxor	$1, $6
	movdqa	$1, $7
	pand	$2, $7
	pxor	$7, $3
	por	$4, $7
	pxor	$7, $6
	movdqa	$2, $7
	por	$5, $7	C t04
	pand	$8, $7
	pxor	$6, $2
	por	$2, $7
	pxor	$1, $7
	pxor	$6, $5
	PNOT($4)	C t02
	por	$4, $5
	pxor	$3, $5
>)

C WLT(x0, x1, x2, x3)
define(<WLT>, <
	WROL(13, $1)
	WROL(3, $3)
	pxor	$1, $2
	pxor	$3, $2
	movdqa	$1, T0
	pslld	<$>3, T0
	pxor	$3, $4
	pxor	T0, $4
	WROL(1, $2)
	WROL(7, $4)
	pxor	$2, $1
	pxor	$4, $1
	movdqa	$2, T0
	pslld	<$>7, T0
	pxor	$4, $3
	pxor	T0, $3
	WROL(5, $1)
	WROL(22, $3)
>)

	.file "serpent-encrypt.asm"
	
	C serpent_encrypt(struct serpent_context *ctx, 
	C	          size_t length, uint8_t *dst,
	C	          const uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(nettle_serpent_encrypt)
        C save all registers that need to be saved
	W64_ENTRY(4, 13)
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14

	lea	(SRC, N), SRC
	lea	(DST, N), DST
	neg	N
	jz	.Lend

	C Point at the final subkey.
	lea	512(CTX), CTX

	cmp	$-64, N
	ja	.Lblock_loop

	pcmpeqd	MINUS1, MINUS1

.Lwblock_loop:
	movups	(SRC, N), X0
	movups	16(SRC, N), X1
	movups	32(SRC, N), X2
	movups	48(SRC, N), X3

	WTRANSPOSE(X0, X1, X2, X3)

	mov	$-512, CNT
	jmp	.Lwround_start

	ALIGN(16)
.Lwround_loop:
	WLT(X0,X1,X2,X3)
.Lwround_start:
	WKEYXOR(, X0,X1,X2,X3)
	WSBOX0(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WLT(Y0,Y1,Y2,Y3)

	WKEYXOR(16, Y0,Y1,Y2,Y3)
	WSBOX1(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WLT(X0,X1,X2,X3)

	WKEYXOR(32, X0,X1,X2,X3)
	WSBOX2(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WLT(Y0,Y1,Y2,Y3)

	WKEYXOR(48, Y0,Y1,Y2,Y3)
	WSBOX3(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WLT(X0,X1,X2,X3)

	WKEYXOR(64, X0,X1,X2,X3)
	WSBOX4(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WLT(Y0,Y1,Y2,Y3)

	WKEYXOR(80, Y0,Y1,Y2,Y3)
	WSBOX5(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WLT(X0,X1,X2,X3)

	WKEYXOR(96, X0,X1,X2,X3)
	WSBOX6(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WLT(Y0,Y1,Y2,Y3)

	WKEYXOR(112, Y0,Y1,Y2,Y3)
	WSBOX7(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	add	$128, CNT
	jnz	.Lwround_loop

	C FIXME: CNT known to be zero, no index register needed
	WKEYXOR(, X0,X1,X2,X3)

	WTRANSPOSE(X0,X1,X2,X3)

	movups	X0, (DST, N)
	movups	X1, 16(DST, N)
	movups	X2, 32(DST, N)
	movups	X3, 48(DST, N)

	C FIXME: Adjust N, so we can use just jnc without an extra cmp.
	add	$64, N
	jz	.Lend

	cmp	$-64, N
	jbe	.Lwblock_loop

C The single-block loop here is slightly slower than the double-block
C loop in serpent-encrypt.c.

C FIXME: Should use non-sse2 code only if we have a single block left.
C With two or three blocks, it should be better to do them in
C parallell.
	
.Lblock_loop:
	movl	(SRC, N), x0
	movl	4(SRC, N), x1
	movl	8(SRC, N), x2
	movl	12(SRC, N), x3

	mov	$-512, CNT
	jmp	.Lround_start
	
	ALIGN(16)
.Lround_loop:
	LT(x0,x1,x2,x3)
.Lround_start:
	xor	  (CTX, CNT), x0
	xor	 4(CTX, CNT), x1
	xor	 8(CTX, CNT), x2
	xor	12(CTX, CNT), x3
	SBOX0(x0,x1,x2,x3, y0,y1,y2,y3)
	LT(y0,y1,y2,y3)
	
	xor	16(CTX, CNT), y0
	xor	20(CTX, CNT), y1
	xor	24(CTX, CNT), y2
	xor	28(CTX, CNT), y3
	SBOX1(y0,y1,y2,y3, x0,x1,x2,x3)
	LT(x0,x1,x2,x3)

	xor	32(CTX, CNT), x0
	xor	36(CTX, CNT), x1
	xor	40(CTX, CNT), x2
	xor	44(CTX, CNT), x3
	SBOX2(x0,x1,x2,x3, y0,y1,y2,y3)
	LT(y0,y1,y2,y3)

	xor	48(CTX, CNT), y0
	xor	52(CTX, CNT), y1
	xor	56(CTX, CNT), y2
	xor	60(CTX, CNT), y3
	SBOX3(y0,y1,y2,y3, x0,x1,x2,x3)
	LT(x0,x1,x2,x3)

	xor	64(CTX, CNT), x0
	xor	68(CTX, CNT), x1
	xor	72(CTX, CNT), x2
	xor	76(CTX, CNT), x3
	SBOX4(x0,x1,x2,x3, y0,y1,y2,y3)
	LT(y0,y1,y2,y3)

	xor	80(CTX, CNT), y0
	xor	84(CTX, CNT), y1
	xor	88(CTX, CNT), y2
	xor	92(CTX, CNT), y3
	SBOX5(y0,y1,y2,y3, x0,x1,x2,x3)
	LT(x0,x1,x2,x3)

	xor	96(CTX, CNT), x0
	xor	100(CTX, CNT), x1
	xor	104(CTX, CNT), x2
	xor	108(CTX, CNT), x3
	SBOX6(x0,x1,x2,x3, y0,y1,y2,y3)
	LT(y0,y1,y2,y3)

	xor	112(CTX, CNT), y0
	xor	116(CTX, CNT), y1
	xor	120(CTX, CNT), y2
	xor	124(CTX, CNT), y3
	SBOX7(y0,y1,y2,y3, x0,x1,x2,x3)
	add	$128, CNT
	jnz	.Lround_loop

	C Apply final subkey.
	xor	  (CTX, CNT), x0
	xor	 4(CTX, CNT), x1
	xor	 8(CTX, CNT), x2
	xor	12(CTX, CNT), x3

	movl	x0, (DST, N)
	movl	x1, 4(DST, N)
	movl	x2, 8(DST, N)
	movl	x3, 12(DST, N)
	add	$16, N
	jnc	.Lblock_loop

.Lend:
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	W64_EXIT(4, 13)
	ret
