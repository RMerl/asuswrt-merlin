C x86_64/serpent-decrypt.asm

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

define(<SBOX0I>, <
	mov	$1, $5
	xor	$3, $5
	mov	$1, $7
	or	$2, $7
	mov	$3, $6
	xor	$4, $6
	xor	$6, $7
	and	$3, $6
	or	$2, $3
	xor	$4, $2
	or	$1, $6
	and	$3, $2
	xor	$2, $6
	or	$7, $1
	xor	$6, $1
	mov	$7, $2
	and	$1, $2
	not	$7
	or	$7, $4
	xor	$3, $4
	mov	$1, $8
	xor	$4, $8
	or	$4, $2
	xor	$2, $5
>)

define(<SBOX1I>, <
	mov	$2, $6
	or	$4, $6
	xor	$3, $6
	mov	$1, $8
	xor	$2, $8
	mov	$1, $5
	or	$6, $5
	and	$8, $5
	xor	$5, $2
	xor	$6, $8
	and	$4, $2
	mov	$1, $7
	and	$3, $7
	or	$7, $6
	or	$4, $7
	xor	$5, $7
	not	$7
	xor	$2, $6
	xor	$6, $5
	xor	$3, $5
	or	$7, $1
	xor	$1, $5
>)

define(<SBOX2I>, <
	mov	$1, $5
	xor	$4, $5
	mov	$3, $7
	xor	$4, $7
	mov	$2, $6
	or	$7, $6
	xor	$6, $5
	mov	$4, $6
	or	$5, $6
	and	$2, $6
	not	$4
	mov	$1, $8
	or	$3, $8
	and	$8, $7
	xor	$7, $6
	and	$2, $8
	and	$3, $1
	or	$4, $1
	xor	$1, $8
	and	$8, $3
	xor	$1, $3
	mov	$5, $7
	xor	$6, $7
	xor	$3, $7
>)

define(<SBOX3I>, <
	mov	$3, $8
	or	$4, $8
	mov	$2, $5
	and	$8, $5
	mov	$1, $7
	or	$4, $7
	mov	$3, $6
	xor	$7, $6
	xor	$6, $5
	xor	$1, $4
	xor	$4, $8
	xor	$2, $7
	and	$6, $7
	xor	$4, $7
	xor	$1, $6
	or	$5, $4
	and	$4, $6
	xor	$2, $6
	and	$7, $1
	or	$2, $1
	xor	$1, $8
>)

define(<SBOX4I>, <
	mov	$3, $6
	xor	$4, $6
	mov	$3, $7
	or	$4, $7
	xor	$2, $7
	or	$4, $2
	mov	$1, $5
	xor	$7, $5
	xor	$7, $4
	and	$1, $7
	xor	$7, $6
	xor	$1, $7
	or	$3, $7
	and	$2, $1
	mov	$1, $8
	xor	$4, $8
	not	$1
	or	$6, $1
	xor	$1, $5
	xor	$2, $1
	xor	$1, $7
>)

define(<SBOX5I>, <
	mov	$1, $6
	and	$4, $6
	mov	$3, $8
	xor	$6, $8
	mov	$2, $5
	and	$8, $5
	mov	$1, $7
	xor	$4, $7
	xor	$2, $4
	xor	$7, $5
	and	$1, $3
	and	$5, $1
	or	$2, $3
	xor	$5, $6
	xor	$3, $6
	mov	$5, $7
	or	$6, $7
	xor	$8, $7
	xor	$4, $7
	not	$2
	or	$1, $2
	xor	$2, $8
>)

define(<SBOX6I>, <
	mov	$1, $7
	xor	$3, $7
	not	$3
	mov	$2, $5
	xor	$4, $5
	mov	$1, $6
	or	$3, $6
	xor	$5, $6
	mov	$2, $8
	and	$7, $8
	or	$4, $8
	or	$3, $4
	or	$2, $3
	and	$1, $3
	mov	$3, $5
	xor	$8, $5
	not	$5
	and	$7, $8
	xor	$3, $8
	xor	$6, $1
	xor	$1, $8
	and	$5, $2
	xor	$2, $7
	xor	$4, $7
>)

define(<SBOX7I>, <
	mov	$1, $8
	and	$2, $8
	mov	$2, $7
	xor	$4, $7
	or	$8, $7
	mov	$1, $6
	or	$4, $6
	and	$3, $6
	xor	$6, $7
	or	$3, $8
	mov	$1, $5
	or	$2, $5
	and	$4, $5
	xor	$5, $8
	xor	$2, $5
	mov	$4, $6
	xor	$8, $6
	not	$6
	or	$5, $6
	xor	$3, $5
	xor	$1, $6
	or	$6, $4
	xor	$4, $5
>)

define(<LTI>, <
	rol	<$>10, $3
	rol	<$>27, $1
	mov	$2, TMP32
	shl	<$>7, TMP32
	xor	$4, $3
	xor	TMP32, $3
	xor	$2, $1
	xor	$4, $1
	rol	<$>25, $4
	rol	<$>31, $2
	mov	$1, TMP32
	shl	<$>3, TMP32
	xor	$3, $4
	xor	TMP32, $4
	xor	$1, $2
	xor	$3, $2
	rol	<$>29, $3
	rol	<$>19, $1
>)

define(<PNOT>, <
	pxor	MINUS1, $1
>)

define(<WSBOX0I>, <
	movdqa	$1, $5
	pxor	$3, $5
	movdqa	$1, $7
	por	$2, $7
	movdqa	$3, $6
	pxor	$4, $6
	pxor	$6, $7
	pand	$3, $6
	por	$2, $3
	pxor	$4, $2
	por	$1, $6
	pand	$3, $2
	pxor	$2, $6
	por	$7, $1
	pxor	$6, $1
	movdqa	$7, $2
	pand	$1, $2
	PNOT($7)
	por	$7, $4
	pxor	$3, $4
	movdqa	$1, $8
	pxor	$4, $8
	por	$4, $2
	pxor	$2, $5
>)

define(<WSBOX1I>, <
	movdqa	$2, $6
	por	$4, $6
	pxor	$3, $6
	movdqa	$1, $8
	pxor	$2, $8
	movdqa	$1, $5
	por	$6, $5
	pand	$8, $5
	pxor	$5, $2
	pxor	$6, $8
	pand	$4, $2
	movdqa	$1, $7
	pand	$3, $7
	por	$7, $6
	por	$4, $7
	pxor	$5, $7
	PNOT($7)
	pxor	$2, $6
	pxor	$6, $5
	pxor	$3, $5
	por	$7, $1
	pxor	$1, $5
>)

define(<WSBOX2I>, <
	movdqa	$1, $5
	pxor	$4, $5
	movdqa	$3, $7
	pxor	$4, $7
	movdqa	$2, $6
	por	$7, $6
	pxor	$6, $5
	movdqa	$4, $6
	por	$5, $6
	pand	$2, $6
	PNOT($4)
	movdqa	$1, $8
	por	$3, $8
	pand	$8, $7
	pxor	$7, $6
	pand	$2, $8
	pand	$3, $1
	por	$4, $1
	pxor	$1, $8
	pand	$8, $3
	pxor	$1, $3
	movdqa	$5, $7
	pxor	$6, $7
	pxor	$3, $7
>)

define(<WSBOX3I>, <
	movdqa	$3, $8
	por	$4, $8
	movdqa	$2, $5
	pand	$8, $5
	movdqa	$1, $7
	por	$4, $7
	movdqa	$3, $6
	pxor	$7, $6
	pxor	$6, $5
	pxor	$1, $4
	pxor	$4, $8
	pxor	$2, $7
	pand	$6, $7
	pxor	$4, $7
	pxor	$1, $6
	por	$5, $4
	pand	$4, $6
	pxor	$2, $6
	pand	$7, $1
	por	$2, $1
	pxor	$1, $8
>)

define(<WSBOX4I>, <
	movdqa	$3, $6
	pxor	$4, $6
	movdqa	$3, $7
	por	$4, $7
	pxor	$2, $7
	por	$4, $2
	movdqa	$1, $5
	pxor	$7, $5
	pxor	$7, $4
	pand	$1, $7
	pxor	$7, $6
	pxor	$1, $7
	por	$3, $7
	pand	$2, $1
	movdqa	$1, $8
	pxor	$4, $8
	PNOT($1)
	por	$6, $1
	pxor	$1, $5
	pxor	$2, $1
	pxor	$1, $7
>)

define(<WSBOX5I>, <
	movdqa	$1, $6
	pand	$4, $6
	movdqa	$3, $8
	pxor	$6, $8
	movdqa	$2, $5
	pand	$8, $5
	movdqa	$1, $7
	pxor	$4, $7
	pxor	$2, $4
	pxor	$7, $5
	pand	$1, $3
	pand	$5, $1
	por	$2, $3
	pxor	$5, $6
	pxor	$3, $6
	movdqa	$5, $7
	por	$6, $7
	pxor	$8, $7
	pxor	$4, $7
	PNOT($2)
	por	$1, $2
	pxor	$2, $8
>)

define(<WSBOX6I>, <
	movdqa	$1, $7
	pxor	$3, $7
	PNOT($3)
	movdqa	$2, $5
	pxor	$4, $5
	movdqa	$1, $6
	por	$3, $6
	pxor	$5, $6
	movdqa	$2, $8
	pand	$7, $8
	por	$4, $8
	por	$3, $4
	por	$2, $3
	pand	$1, $3
	movdqa	$3, $5
	pxor	$8, $5
	PNOT($5)
	pand	$7, $8
	pxor	$3, $8
	pxor	$6, $1
	pxor	$1, $8
	pand	$5, $2
	pxor	$2, $7
	pxor	$4, $7
>)

define(<WSBOX7I>, <
	movdqa	$1, $8
	pand	$2, $8
	movdqa	$2, $7
	pxor	$4, $7
	por	$8, $7
	movdqa	$1, $6
	por	$4, $6
	pand	$3, $6
	pxor	$6, $7
	por	$3, $8
	movdqa	$1, $5
	por	$2, $5
	pand	$4, $5
	pxor	$5, $8
	pxor	$2, $5
	movdqa	$4, $6
	pxor	$8, $6
	PNOT($6)
	por	$5, $6
	pxor	$3, $5
	pxor	$1, $6
	por	$6, $4
	pxor	$4, $5
>)

define(<WLTI>, <
	WROL(10, $3)
	WROL(27, $1)
	movdqa	$2, T0
	pslld	<$>7, T0
	pxor	$4, $3
	pxor	T0, $3
	pxor	$2, $1
	pxor	$4, $1
	WROL(25, $4)
	WROL(31, $2)
	movdqa	$1, T0
	pslld	<$>3, T0
	pxor	$3, $4
	pxor	T0, $4
	pxor	$1, $2
	pxor	$3, $2
	WROL(29, $3)
	WROL(19, $1)
>)

	.file "serpent-decrypt.asm"
	
	C serpent_decrypt(struct serpent_context *ctx, 
	C	          size_t length, uint8_t *dst,
	C	          const uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(nettle_serpent_decrypt)
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

	cmp	$-64, N
	ja	.Lblock_loop

	pcmpeqd	MINUS1, MINUS1

.Lwblock_loop:
	movups	(SRC, N), X0
	movups	16(SRC, N), X1
	movups	32(SRC, N), X2
	movups	48(SRC, N), X3

	WTRANSPOSE(X0,X1,X2,X3)

	mov	$384, CNT

	C FIXME: CNT known, no index register needed
	WKEYXOR(128, X0,X1,X2,X3)

	jmp	.Lwround_start

	ALIGN(16)

.Lwround_loop:
	WLTI(X0,X1,X2,X3)
.Lwround_start:
	WSBOX7I(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WKEYXOR(112, Y0,Y1,Y2,Y3)

	WLTI(Y0,Y1,Y2,Y3)
	WSBOX6I(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WKEYXOR(96, X0,X1,X2,X3)
	
	WLTI(X0,X1,X2,X3)
	WSBOX5I(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WKEYXOR(80, Y0,Y1,Y2,Y3)

	WLTI(Y0,Y1,Y2,Y3)
	WSBOX4I(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WKEYXOR(64, X0,X1,X2,X3)
	
	WLTI(X0,X1,X2,X3)
	WSBOX3I(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WKEYXOR(48, Y0,Y1,Y2,Y3)

	WLTI(Y0,Y1,Y2,Y3)
	WSBOX2I(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WKEYXOR(32, X0,X1,X2,X3)
	
	WLTI(X0,X1,X2,X3)
	WSBOX1I(X0,X1,X2,X3, Y0,Y1,Y2,Y3)
	WKEYXOR(16, Y0,Y1,Y2,Y3)

	WLTI(Y0,Y1,Y2,Y3)
	WSBOX0I(Y0,Y1,Y2,Y3, X0,X1,X2,X3)
	WKEYXOR(, X0,X1,X2,X3)

	sub	$128, CNT
	jnc	.Lwround_loop

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

.Lblock_loop:
	movl	(SRC, N), x0
	movl	4(SRC, N), x1
	movl	8(SRC, N), x2
	movl	12(SRC, N), x3

	xor	512(CTX), x0
	xor	516(CTX), x1
	xor	520(CTX), x2
	xor	524(CTX), x3

	mov	$384, CNT
	jmp	.Lround_start

	ALIGN(16)
.Lround_loop:
	LTI(x0,x1,x2,x3)
.Lround_start:
	SBOX7I(x0,x1,x2,x3, y0,y1,y2,y3)
	xor	112(CTX, CNT), y0
	xor	116(CTX, CNT), y1
	xor	120(CTX, CNT), y2
	xor	124(CTX, CNT), y3

	LTI(y0,y1,y2,y3)
	SBOX6I(y0,y1,y2,y3, x0,x1,x2,x3)
	xor	 96(CTX, CNT), x0
	xor	100(CTX, CNT), x1
	xor	104(CTX, CNT), x2
	xor	108(CTX, CNT), x3

	LTI(x0,x1,x2,x3)
	SBOX5I(x0,x1,x2,x3, y0,y1,y2,y3)
	xor	80(CTX, CNT), y0
	xor	84(CTX, CNT), y1
	xor	88(CTX, CNT), y2
	xor	92(CTX, CNT), y3

	LTI(y0,y1,y2,y3)
	SBOX4I(y0,y1,y2,y3, x0,x1,x2,x3)
	xor	64(CTX, CNT), x0
	xor	68(CTX, CNT), x1
	xor	72(CTX, CNT), x2
	xor	76(CTX, CNT), x3

	LTI(x0,x1,x2,x3)
	SBOX3I(x0,x1,x2,x3, y0,y1,y2,y3)
	xor	48(CTX, CNT), y0
	xor	52(CTX, CNT), y1
	xor	56(CTX, CNT), y2
	xor	60(CTX, CNT), y3

	LTI(y0,y1,y2,y3)
	SBOX2I(y0,y1,y2,y3, x0,x1,x2,x3)
	xor	32(CTX, CNT), x0
	xor	36(CTX, CNT), x1
	xor	40(CTX, CNT), x2
	xor	44(CTX, CNT), x3

	LTI(x0,x1,x2,x3)
	SBOX1I(x0,x1,x2,x3, y0,y1,y2,y3)
	xor	16(CTX, CNT), y0
	xor	20(CTX, CNT), y1
	xor	24(CTX, CNT), y2
	xor	28(CTX, CNT), y3

	LTI(y0,y1,y2,y3)
	SBOX0I(y0,y1,y2,y3, x0,x1,x2,x3)
	xor	  (CTX, CNT), x0
	xor	 4(CTX, CNT), x1
	xor	 8(CTX, CNT), x2
	xor	12(CTX, CNT), x3
	sub	$128, CNT
	jnc	.Lround_loop

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
