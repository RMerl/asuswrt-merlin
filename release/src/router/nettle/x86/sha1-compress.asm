C x86/sha1-compress.asm

ifelse(<
   Copyright (C) 2004, 2009 Niels Möller

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

C Register usage
define(<SA>,<%eax>)
define(<SB>,<%ebx>)
define(<SC>,<%ecx>)
define(<SD>,<%edx>)
define(<SE>,<%ebp>)
define(<DATA>,<%esp>)
define(<T1>,<%edi>)
define(<T2>,<%esi>)
	
C Constants
define(<K1VALUE>, <0x5A827999>)		C  Rounds  0-19
define(<K2VALUE>, <0x6ED9EBA1>)		C  Rounds 20-39
define(<K3VALUE>, <0x8F1BBCDC>)		C  Rounds 40-59
define(<K4VALUE>, <0xCA62C1D6>)		C  Rounds 60-79
	
C Reads the input via T2 into register, byteswaps it, and stores it in the DATA array.
C SWAP(index, register)
define(<SWAP>, <
	movl	OFFSET($1)(T2), $2
	bswap	$2
	movl	$2, OFFSET($1) (DATA)
>)dnl

C The f functions,
C
C  f1(x,y,z) = z ^ (x & (y ^ z))
C  f2(x,y,z) = x ^ y ^ z
C  f3(x,y,z) = (x & (y ^ z)) + (y & z)
C  f4 = f2

C This form for f3 was suggested by George Spelvin. The terms can be
C added into the result one at a time, saving one temporary.

C The form of one sha1 round is
C
C   a' = e + a <<< 5 + f( b, c, d ) + k + w;
C   b' = a;
C   c' = b <<< 30;
C   d' = c;
C   e' = d;
C
C where <<< denotes rotation. We permute our variables, so that we
C instead get
C
C   e += a <<< 5 + f( b, c, d ) + k + w;
C   b <<<= 30

dnl ROUND_F1(a, b, c, d, e, i)
define(<ROUND_F1>, <
	mov	OFFSET(eval($6 % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	rol	<$>1, T1
	mov	T1, OFFSET(eval($6 % 16)) (DATA)
	mov	$4, T2
	xor	$3, T2
	and	$2, T2
	xor	$4, T2
	rol	<$>30, $2
	lea	K1VALUE (T1, $5), $5
	mov	$1, T1
	rol	<$>5, T1
	add	T1, $5
	add	T2, $5
>)

dnl ROUND_F1_NOEXP(a, b, c, d, e, i)
define(<ROUND_F1_NOEXP>, <
	mov	$4, T2
	xor	$3, T2
	mov	$1, T1
	and	$2, T2
	add	OFFSET($6) (DATA), $5
	xor	$4, T2
	add	T2, $5
	rol	<$>30, $2
	rol	<$>5, T1
	lea	K1VALUE (T1, $5), $5
>)

dnl ROUND_F2(a, b, c, d, e, i, k)
define(<ROUND_F2>, <
	mov	OFFSET(eval($6 % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	rol	<$>1, T1
	mov	T1, OFFSET(eval($6 % 16)) (DATA)
	mov	$4, T2
	xor	$3, T2
	xor	$2, T2
	rol	<$>30, $2
	lea	$7 (T1, $5), $5
	mov	$1, T1
	rol	<$>5, T1
	add	T1, $5
	add	T2, $5
>)

dnl ROUND_F3(a, b, c, d, e, i)
define(<ROUND_F3>, <
	mov	OFFSET(eval($6 % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xor	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	rol	<$>1, T1
	mov	T1, OFFSET(eval($6 % 16)) (DATA)
	mov	$4, T2
	and	$3, T2
 	lea	K3VALUE (T1, $5), $5
	mov	$4, T1
	xor	$3, T1
	and	$2, T1
	add	T2, $5
	rol	<$>30, $2
	mov	$1, T2
	rol	<$>5, T2
	add	T1, $5
	add	T2, $5
>)

	.file "sha1-compress.asm"

	C _nettle_sha1_compress(uint32_t *state, uint8_t *data)
	
	.text

PROLOGUE(_nettle_sha1_compress)
	C save all registers that need to be saved
	C 			   88(%esp)  data
	C 			   84(%esp)  state
	C 			   80(%esp)  Return address
	pushl	%ebx		C  76(%esp)
	pushl	%ebp		C  72(%esp)
	pushl	%esi		C  68(%esp)
	pushl	%edi		C  64(%esp)

	subl	$64, %esp	C  %esp = W

	C Loop-mixed to 520 cycles (for the complete function call) on
	C AMD K7.
ALIGN(32)
	mov	88(%esp), T2
	mov	OFFSET(2)(T2), %ecx
	mov	OFFSET(0)(T2), %eax
	bswap	%ecx
	bswap	%eax
	mov	%ecx, OFFSET(2) (DATA)
	mov	%eax, OFFSET(0) (DATA)
	mov	OFFSET(3)(T2), %edx
	mov	OFFSET(6)(T2), %ecx
	mov	OFFSET(4)(T2), %eax
	mov	OFFSET(1)(T2), %ebx
	bswap	%ebx
	bswap	%eax
	bswap	%ecx
	mov	%ecx, OFFSET(6) (DATA)
	mov	%eax, OFFSET(4) (DATA)
	bswap	%edx
	mov	%edx, OFFSET(3) (DATA)
	mov	%ebx, OFFSET(1) (DATA)
	mov	OFFSET(10)(T2), %ecx
	mov	OFFSET(8)(T2), %eax
	mov	OFFSET(7)(T2), %edx
	bswap	%eax
	bswap	%edx
	mov	%edx, OFFSET(7) (DATA)
	mov	OFFSET(5)(T2), %ebx
	mov	%eax, OFFSET(8) (DATA)
	mov	OFFSET(11)(T2), %edx
	bswap	%ecx
	bswap	%edx
	mov	OFFSET(12)(T2), %eax
	bswap	%ebx
	mov	%ecx, OFFSET(10) (DATA)
	mov	%ebx, OFFSET(5) (DATA)
	mov	%edx, OFFSET(11) (DATA)
	mov	OFFSET(15)(T2), %edx
	mov	84(%esp),T1
	mov	OFFSET(9)(T2), %ebx
	bswap	%edx
	bswap	%ebx
	bswap	%eax
	mov	OFFSET(14)(T2), %ecx
	mov	%edx, OFFSET(15) (DATA)
	bswap	%ecx
	mov	%ecx, OFFSET(14) (DATA)
	mov	%ebx, OFFSET(9) (DATA)
	mov	OFFSET(13)(T2), %ebx
	mov	12(T1), SD
	bswap	%ebx
	mov	%ebx, OFFSET(13) (DATA)
	mov	8(T1),  SC
	mov	16(T1), SE
	mov	4(T1),  SB
	mov	SD, T2
	add	OFFSET(0) (DATA), SE
	xor	SC, T2
	mov	%eax, OFFSET(12) (DATA)
	mov	(T1),   SA
	and	SB, T2
	xor	SD, T2
	rol	$30, SB
	add	T2, SE
	mov	SA, T1
	mov	SC, T2
	add	OFFSET(1) (DATA), SD
	rol	$5, T1
	xor	SB, T2
	and	SA, T2
	xor	SC, T2
	lea	K1VALUE (T1, SE), SE
	add	T2, SD
	mov	SB, T2
	rol	$30, SA
	xor	SA, T2
	and	SE, T2
	mov	SE, T1
	add	OFFSET(2) (DATA), SC
	rol	$30, SE
	xor	SB, T2
	rol	$5, T1
	lea	K1VALUE (T1, SD), SD
	mov	SD, T1
	rol	$5, T1
	add	T2, SC
	mov	SA, T2
	xor	SE, T2
	lea	K1VALUE (T1, SC), SC
	and	SD, T2
	xor	SA, T2
	add	OFFSET(3) (DATA), SB
	mov	SC, T1
	add	T2, SB
	mov	SE, T2
	rol	$30, SD
	xor	SD, T2
	and	SC, T2
	rol	$5, T1
	xor	SE, T2
	add	OFFSET(4) (DATA), SA
	lea	K1VALUE (T1, SB), SB
	add	T2, SA
	rol	$30, SC
	mov	SD, T2
	xor	SC, T2
	and	SB, T2
	mov	SB, T1
	rol	$5, T1
	add	OFFSET(5) (DATA), SE
	rol	$30, SB
	xor	SD, T2
	add	T2, SE
	mov	SC, T2
	xor	SB, T2
	lea	K1VALUE (T1, SA), SA
	mov	SA, T1
	add	OFFSET(6) (DATA), SD
	and	SA, T2
	rol	$5, T1
	xor	SC, T2
	lea	K1VALUE (T1, SE), SE
	rol	$30, SA
	add	T2, SD
	mov	SB, T2
	mov	SE, T1
	xor	SA, T2
	and	SE, T2
	rol	$5, T1
	lea	K1VALUE (T1, SD), SD
	xor	SB, T2
	add	OFFSET(7) (DATA), SC
	rol	$30, SE
	add	OFFSET(8) (DATA), SB
	mov	SD, T1
	add	T2, SC
	mov	SA, T2
	xor	SE, T2
	rol	$5, T1
	and	SD, T2
	lea	K1VALUE (T1, SC), SC
	xor	SA, T2
	add	T2, SB
	mov	SE, T2
	mov	SC, T1
	rol	$30, SD
	xor	SD, T2
	rol	$5, T1
	lea	K1VALUE (T1, SB), SB
	and	SC, T2
	xor	SE, T2
	add	OFFSET(10) (DATA), SE
	add	OFFSET(9) (DATA), SA
	mov	SB, T1
	add	T2, SA
	rol	$5, T1
	lea	K1VALUE (T1, SA), SA
	mov	SD, T2
	rol	$30, SC
	xor	SC, T2
	and	SB, T2
	xor	SD, T2
	rol	$30, SB
	add	T2, SE
	mov	SC, T2
	mov	SA, T1
	xor	SB, T2
	add	OFFSET(11) (DATA), SD
	and	SA, T2
	rol	$30, SA
	rol	$5, T1
	xor	SC, T2
	lea	K1VALUE (T1, SE), SE
	add	T2, SD
	mov	SB, T2
	xor	SA, T2
	mov	SE, T1
	rol	$5, T1
	and	SE, T2
	lea	K1VALUE (T1, SD), SD
	xor	SB, T2
	add	OFFSET(12) (DATA), SC
	add	T2, SC
	rol	$30, SE
	mov	SA, T2
	xor	SE, T2
	mov	SD, T1
	rol	$5, T1
	and	SD, T2
	add	OFFSET(13) (DATA), SB
	lea	K1VALUE (T1, SC), SC
	xor	SA, T2
	add	T2, SB
	mov	SE, T2
	rol	$30, SD
	xor	SD, T2
	and	SC, T2
	mov	SC, T1
	rol	$5, T1
	rol	$30, SC
	add	OFFSET(14) (DATA), SA
	xor	SE, T2
	add	T2, SA
	mov	SD, T2
	xor	SC, T2
	lea	K1VALUE (T1, SB), SB
	and	SB, T2
	mov	SB, T1
	rol	$5, T1
	lea	K1VALUE (T1, SA), SA
	mov	SA, T1
	xor	SD, T2
	add	OFFSET(15) (DATA), SE
	add	T2, SE
	rol	$5, T1
	lea	K1VALUE (T1, SE), SE
	mov	OFFSET(0) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	mov	SC, T2
	xor	OFFSET(8) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	rol	$30, SB
	xor	SB, T2
	and	SA, T2
	xor	SC, T2
	rol	$1, T1
	lea	K1VALUE (T1, T2), T2
	mov	T1, OFFSET(0) (DATA)
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(1) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	rol	$30, SA
	add	T2, SD
	mov	SB, T2
	xor	SA, T2
	and	SE, T2
	xor	OFFSET(9) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	xor	SB, T2
	rol	$1, T1
	mov	T1, OFFSET(1) (DATA)
	lea	K1VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(2) (DATA), T1
	xor	OFFSET(4) (DATA), T1
	rol	$30, SE
	add	T2, SC
	mov	SA, T2
	xor	SE, T2
	xor	OFFSET(10) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	and	SD, T2
	rol	$1, T1
	xor	SA, T2
	mov	T1, OFFSET(2) (DATA)
	lea	K1VALUE (T1, T2), T2
	mov	SC, T1
	rol	$30, SD
	rol	$5, T1
	add	T1, SB
	add	T2, SB
	mov	SE, T2
	mov	OFFSET(3) (DATA), T1
	xor	SD, T2
	xor	OFFSET(5) (DATA), T1
	and	SC, T2
	xor	SE, T2
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(3) (DATA)
	lea	K1VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(4) (DATA), T1
	xor	OFFSET(6) (DATA), T1
	rol	$30, SC
	xor	OFFSET(12) (DATA), T1
	add	T2, SA
	xor	OFFSET(1) (DATA), T1
	mov	SD, T2
	xor	SC, T2
	rol	$1, T1
	xor	SB, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(4) (DATA)
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(5) (DATA), T1
	add	T2, SE
	mov	SC, T2
	xor	OFFSET(7) (DATA), T1
	rol	$30, SB
	xor	OFFSET(13) (DATA), T1
	xor	SB, T2
	xor	OFFSET(2) (DATA), T1
	xor	SA, T2
	rol	$1, T1
	mov	T1, OFFSET(5) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(6) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	add	T2, SD
	rol	$30, SA
	xor	OFFSET(14) (DATA), T1
	mov	SB, T2
	xor	OFFSET(3) (DATA), T1
	xor	SA, T2
	rol	$1, T1
	xor	SE, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(6) (DATA)
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	add	T2, SC
	mov	SA, T2
	rol	$30, SE
	mov	OFFSET(7) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	xor	SE, T2
	xor	OFFSET(15) (DATA), T1
	xor	OFFSET(4) (DATA), T1
	xor	SD, T2
	rol	$1, T1
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(7) (DATA)
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(8) (DATA), T1
	xor	OFFSET(10) (DATA), T1
	add	T2, SB
	rol	$30, SD
	mov	SE, T2
	xor	OFFSET(0) (DATA), T1
	xor	OFFSET(5) (DATA), T1
	xor	SD, T2
	xor	SC, T2
	rol	$1, T1
	mov	T1, OFFSET(8) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(9) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	add	T2, SA
	xor	OFFSET(6) (DATA), T1
	mov	SD, T2
	rol	$1, T1
	rol	$30, SC
	xor	SC, T2
	mov	T1, OFFSET(9) (DATA)
	xor	SB, T2
	lea	K2VALUE (T1, T2), T2
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(10) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	add	T2, SE
	mov	SC, T2
	rol	$30, SB
	xor	OFFSET(7) (DATA), T1
	xor	SB, T2
	rol	$1, T1
	xor	SA, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(10) (DATA)
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(11) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	rol	$30, SA
	xor	OFFSET(3) (DATA), T1
	add	T2, SD
	xor	OFFSET(8) (DATA), T1
	mov	SB, T2
	xor	SA, T2
	rol	$1, T1
	mov	T1, OFFSET(11) (DATA)
	xor	SE, T2
	lea	K2VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(12) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	rol	$30, SE
	add	T2, SC
	xor	OFFSET(4) (DATA), T1
	mov	SA, T2
	xor	OFFSET(9) (DATA), T1
	xor	SE, T2
	rol	$1, T1
	xor	SD, T2
	mov	T1, OFFSET(12) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	rol	$30, SD
	mov	OFFSET(13) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	add	T2, SB
	mov	SE, T2
	xor	OFFSET(5) (DATA), T1
	xor	SD, T2
	xor	OFFSET(10) (DATA), T1
	xor	SC, T2
	rol	$1, T1
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(13) (DATA)
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	add	T2, SA
	mov	SD, T2
	mov	OFFSET(14) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	rol	$30, SC
	xor	OFFSET(6) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	SC, T2
	xor	SB, T2
	rol	$1, T1
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(14) (DATA)
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(15) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	add	T2, SE
	mov	SC, T2
	rol	$30, SB
	xor	SB, T2
	xor	OFFSET(7) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	xor	SA, T2
	rol	$1, T1
	mov	T1, OFFSET(15) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(0) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	add	T2, SD
	mov	SB, T2
	rol	$30, SA
	xor	SA, T2
	xor	OFFSET(13) (DATA), T1
	rol	$1, T1
	xor	SE, T2
	mov	T1, OFFSET(0) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(1) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	add	T2, SC
	mov	SA, T2
	rol	$30, SE
	xor	SE, T2
	xor	OFFSET(9) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	rol	$1, T1
	xor	SD, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(1) (DATA)
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(2) (DATA), T1
	rol	$30, SD
	xor	OFFSET(4) (DATA), T1
	add	T2, SB
	mov	SE, T2
	xor	OFFSET(10) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	xor	SD, T2
	xor	SC, T2
	rol	$1, T1
	mov	T1, OFFSET(2) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(3) (DATA), T1
	xor	OFFSET(5) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	add	T2, SA
	rol	$30, SC
	mov	SD, T2
	xor	SC, T2
	rol	$1, T1
	xor	SB, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(3) (DATA)
	mov	SA, T1
	rol	$5, T1
	rol	$30, SB
	add	T1, SE
	mov	OFFSET(4) (DATA), T1
	add	T2, SE
	xor	OFFSET(6) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	mov	SC, T2
	xor	SB, T2
	rol	$1, T1
	xor	SA, T2
	lea	K2VALUE (T1, T2), T2
	mov	T1, OFFSET(4) (DATA)
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	add	T2, SD
	mov	OFFSET(5) (DATA), T1
	mov	SB, T2
	rol	$30, SA
	xor	SA, T2
	xor	SE, T2
	xor	OFFSET(7) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(5) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(6) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	add	T2, SC
	xor	OFFSET(14) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(6) (DATA)
	mov	SA, T2
	rol	$30, SE
	xor	SE, T2
	xor	SD, T2
	lea	K2VALUE (T1, T2), T2
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	add	T2, SB
	mov	OFFSET(7) (DATA), T1
	mov	SE, T2
	rol	$30, SD
	xor	OFFSET(9) (DATA), T1
	xor	SD, T2
	xor	SC, T2
	xor	OFFSET(15) (DATA), T1
	xor	OFFSET(4) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(7) (DATA)
	lea	K2VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(8) (DATA), T1
	xor	OFFSET(10) (DATA), T1
	rol	$30, SC
	xor	OFFSET(0) (DATA), T1
	add	T2, SA
	mov	SD, T2
	xor	OFFSET(5) (DATA), T1
	rol	$1, T1
	and	SC, T2
	mov	T1, OFFSET(8) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SE
	mov	SA, T1
	mov	SD, T2
	xor	SC, T2
	and	SB, T2
	rol	$30, SB
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(9) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	add	T2, SE
	mov	SC, T2
	xor	OFFSET(6) (DATA), T1
	rol	$1, T1
	and	SB, T2
	mov	T1, OFFSET(9) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SD
	mov	SC, T2
	xor	SB, T2
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(10) (DATA), T1
	and	SA, T2
	add	T2, SD
	xor	OFFSET(12) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	rol	$30, SA
	mov	SB, T2
	and	SA, T2
	xor	OFFSET(7) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(10) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SC
	mov	SD, T1
	rol	$5, T1
	mov	SB, T2
	add	T1, SC
	mov	OFFSET(11) (DATA), T1
	xor	SA, T2
	xor	OFFSET(13) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	and	SE, T2
	xor	OFFSET(8) (DATA), T1
	add	T2, SC
	rol	$1, T1
	mov	SA, T2
	mov	T1, OFFSET(11) (DATA)
	rol	$30, SE
	and	SE, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SB
	mov	SA, T2
	mov	SC, T1
	xor	SE, T2
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(12) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	xor	OFFSET(4) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	and	SD, T2
	rol	$30, SD
	add	T2, SB
	rol	$1, T1
	mov	T1, OFFSET(12) (DATA)
	mov	SE, T2
	and	SD, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SA
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(13) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	mov	SE, T2
	xor	OFFSET(5) (DATA), T1
	xor	SD, T2
	and	SC, T2
	xor	OFFSET(10) (DATA), T1
	add	T2, SA
	rol	$1, T1
	rol	$30, SC
	mov	T1, OFFSET(13) (DATA)
	mov	SD, T2
	and	SC, T2
	lea	K3VALUE (T1, T2), T1
	mov	SD, T2
	add	T1, SE
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(14) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	xor	SC, T2
	and	SB, T2
	xor	OFFSET(6) (DATA), T1
	rol	$30, SB
	xor	OFFSET(11) (DATA), T1
	rol	$1, T1
	add	T2, SE
	mov	SC, T2
	mov	T1, OFFSET(14) (DATA)
	and	SB, T2
	lea	K3VALUE (T1, T2), T1
	mov	SC, T2
	add	T1, SD
	mov	SE, T1
	xor	SB, T2
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(15) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	and	SA, T2
	xor	OFFSET(7) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	add	T2, SD
	rol	$30, SA
	mov	SB, T2
	rol	$1, T1
	mov	T1, OFFSET(15) (DATA)
	and	SA, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SC
	mov	SD, T1
	mov	SB, T2
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(0) (DATA), T1
	xor	SA, T2
	xor	OFFSET(2) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	and	SE, T2
	add	T2, SC
	rol	$30, SE
	rol	$1, T1
	mov	T1, OFFSET(0) (DATA)
	mov	SA, T2
	and	SE, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SB
	mov	SC, T1
	mov	SA, T2
	xor	SE, T2
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(1) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	and	SD, T2
	xor	OFFSET(14) (DATA), T1
	add	T2, SB
	rol	$30, SD
	mov	SE, T2
	rol	$1, T1
	and	SD, T2
	mov	T1, OFFSET(1) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SA
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	SE, T2
	mov	OFFSET(2) (DATA), T1
	xor	SD, T2
	xor	OFFSET(4) (DATA), T1
	xor	OFFSET(10) (DATA), T1
	and	SC, T2
	add	T2, SA
	xor	OFFSET(15) (DATA), T1
	rol	$30, SC
	mov	SD, T2
	rol	$1, T1
	mov	T1, OFFSET(2) (DATA)
	and	SC, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SE
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(3) (DATA), T1
	xor	OFFSET(5) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	mov	SD, T2
	rol	$1, T1
	xor	SC, T2
	and	SB, T2
	mov	T1, OFFSET(3) (DATA)
	rol	$30, SB
	add	T2, SE
	mov	SC, T2
	and	SB, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SD
	mov	SE, T1
	mov	SC, T2
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(4) (DATA), T1
	xor	OFFSET(6) (DATA), T1
	xor	SB, T2
	and	SA, T2
	add	T2, SD
	mov	SB, T2
	xor	OFFSET(12) (DATA), T1
	rol	$30, SA
	and	SA, T2
	xor	OFFSET(1) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(4) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SC
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(5) (DATA), T1
	xor	OFFSET(7) (DATA), T1
	mov	SB, T2
	xor	OFFSET(13) (DATA), T1
	xor	SA, T2
	xor	OFFSET(2) (DATA), T1
	and	SE, T2
	rol	$30, SE
	add	T2, SC
	rol	$1, T1
	mov	SA, T2
	mov	T1, OFFSET(5) (DATA)
	and	SE, T2
	lea	K3VALUE (T1, T2), T1
	add	T1, SB
	mov	SA, T2
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	xor	SE, T2
	and	SD, T2
	mov	OFFSET(6) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	rol	$1, T1
	add	T2, SB
	rol	$30, SD
	mov	SE, T2
	and	SD, T2
	mov	T1, OFFSET(6) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SA
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(7) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	mov	SE, T2
	xor	SD, T2
	xor	OFFSET(15) (DATA), T1
	and	SC, T2
	rol	$30, SC
	add	T2, SA
	mov	SD, T2
	xor	OFFSET(4) (DATA), T1
	rol	$1, T1
	and	SC, T2
	mov	T1, OFFSET(7) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SE
	mov	SA, T1
	rol	$5, T1
	mov	SD, T2
	add	T1, SE
	mov	OFFSET(8) (DATA), T1
	xor	OFFSET(10) (DATA), T1
	xor	SC, T2
	xor	OFFSET(0) (DATA), T1
	and	SB, T2
	add	T2, SE
	xor	OFFSET(5) (DATA), T1
	rol	$30, SB
	mov	SC, T2
	and	SB, T2
	rol	$1, T1
	mov	T1, OFFSET(8) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SD
	mov	SE, T1
	rol	$5, T1
	mov	SC, T2
	xor	SB, T2
	add	T1, SD
	and	SA, T2
	mov	OFFSET(9) (DATA), T1
	rol	$30, SA
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	add	T2, SD
	mov	SB, T2
	and	SA, T2
	xor	OFFSET(6) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(9) (DATA)
	lea	K3VALUE (T1, T2), T1
	add	T1, SC
	mov	SD, T1
	rol	$5, T1
	mov	SB, T2
	xor	SA, T2
	and	SE, T2
	add	T1, SC
	mov	OFFSET(10) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	add	T2, SC
	mov	SA, T2
	rol	$30, SE
	xor	OFFSET(7) (DATA), T1
	rol	$1, T1
	and	SE, T2
	mov	T1, OFFSET(10) (DATA)
	lea	K3VALUE (T1, T2), T1
	mov	SA, T2
	xor	SE, T2
	add	T1, SB
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(11) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	and	SD, T2
	add	T2, SB
	mov	SE, T2
	rol	$1, T1
	mov	T1, OFFSET(11) (DATA)
	rol	$30, SD
	and	SD, T2
	lea	K3VALUE (T1, T2), T1
	mov	SE, T2
	add	T1, SA
	xor	SD, T2
	mov	SB, T1
	and	SC, T2
	rol	$30, SC
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(12) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	add	T2, SA
	mov	SD, T2
	xor	OFFSET(4) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(12) (DATA)
	xor	SC, T2
	xor	SB, T2
	lea	K4VALUE (T1, T2), T2
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(13) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	add	T2, SE
	rol	$30, SB
	mov	SC, T2
	xor	OFFSET(5) (DATA), T1
	xor	SB, T2
	xor	OFFSET(10) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(13) (DATA)
	xor	SA, T2
	lea	K4VALUE (T1, T2), T2
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(14) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	rol	$30, SA
	add	T2, SD
	mov	SB, T2
	xor	SA, T2
	xor	SE, T2
	xor	OFFSET(6) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(14) (DATA)
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	add	T2, SC
	mov	OFFSET(15) (DATA), T1
	mov	SA, T2
	rol	$30, SE
	xor	OFFSET(1) (DATA), T1
	xor	OFFSET(7) (DATA), T1
	xor	SE, T2
	xor	SD, T2
	xor	OFFSET(12) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(15) (DATA)
	lea	K4VALUE (T1, T2), T2
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(0) (DATA), T1
	add	T2, SB
	xor	OFFSET(2) (DATA), T1
	mov	SE, T2
	rol	$30, SD
	xor	OFFSET(8) (DATA), T1
	xor	SD, T2
	xor	OFFSET(13) (DATA), T1
	xor	SC, T2
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(0) (DATA)
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(1) (DATA), T1
	rol	$30, SC
	xor	OFFSET(3) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	add	T2, SA
	mov	SD, T2
	xor	SC, T2
	rol	$1, T1
	xor	SB, T2
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(1) (DATA)
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(2) (DATA), T1
	rol	$30, SB
	xor	OFFSET(4) (DATA), T1
	add	T2, SE
	mov	SC, T2
	xor	SB, T2
	xor	OFFSET(10) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	xor	SA, T2
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(2) (DATA)
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	mov	OFFSET(3) (DATA), T1
	xor	OFFSET(5) (DATA), T1
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	rol	$30, SA
	add	T2, SD
	mov	SB, T2
	rol	$1, T1
	mov	T1, OFFSET(3) (DATA)
	xor	SA, T2
	xor	SE, T2
	lea	K4VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(4) (DATA), T1
	add	T2, SC
	rol	$30, SE
	xor	OFFSET(6) (DATA), T1
	mov	SA, T2
	xor	OFFSET(12) (DATA), T1
	xor	SE, T2
	xor	OFFSET(1) (DATA), T1
	rol	$1, T1
	xor	SD, T2
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(4) (DATA)
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	rol	$30, SD
	mov	OFFSET(5) (DATA), T1
	add	T2, SB
	xor	OFFSET(7) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	mov	SE, T2
	xor	SD, T2
	xor	OFFSET(2) (DATA), T1
	xor	SC, T2
	rol	$1, T1
	mov	T1, OFFSET(5) (DATA)
	lea	K4VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(6) (DATA), T1
	xor	OFFSET(8) (DATA), T1
	xor	OFFSET(14) (DATA), T1
	add	T2, SA
	xor	OFFSET(3) (DATA), T1
	mov	SD, T2
	rol	$30, SC
	rol	$1, T1
	xor	SC, T2
	mov	T1, OFFSET(6) (DATA)
	xor	SB, T2
	lea	K4VALUE (T1, T2), T2
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	add	T2, SE
	mov	OFFSET(7) (DATA), T1
	xor	OFFSET(9) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	rol	$30, SB
	xor	OFFSET(4) (DATA), T1
	mov	SC, T2
	rol	$1, T1
	mov	T1, OFFSET(7) (DATA)
	xor	SB, T2
	xor	SA, T2
	lea	K4VALUE (T1, T2), T2
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	rol	$30, SA
	mov	OFFSET(8) (DATA), T1
	xor	OFFSET(10) (DATA), T1
	add	T2, SD
	xor	OFFSET(0) (DATA), T1
	xor	OFFSET(5) (DATA), T1
	rol	$1, T1
	mov	SB, T2
	mov	T1, OFFSET(8) (DATA)
	xor	SA, T2
	xor	SE, T2
	lea	K4VALUE (T1, T2), T2
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	add	T2, SC
	mov	SA, T2
	mov	OFFSET(9) (DATA), T1
	rol	$30, SE
	xor	OFFSET(11) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	xor	OFFSET(6) (DATA), T1
	xor	SE, T2
	xor	SD, T2
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(9) (DATA)
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	rol	$30, SD
	mov	OFFSET(10) (DATA), T1
	xor	OFFSET(12) (DATA), T1
	xor	OFFSET(2) (DATA), T1
	add	T2, SB
	mov	SE, T2
	xor	SD, T2
	xor	SC, T2
	xor	OFFSET(7) (DATA), T1
	rol	$1, T1
	mov	T1, OFFSET(10) (DATA)
	lea	K4VALUE (T1, T2), T2
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	mov	OFFSET(11) (DATA), T1
	xor	OFFSET(13) (DATA), T1
	xor	OFFSET(3) (DATA), T1
	add	T2, SA
	mov	SD, T2
	rol	$30, SC
	xor	SC, T2
	xor	OFFSET(8) (DATA), T1
	rol	$1, T1
	xor	SB, T2
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(11) (DATA)
	mov	SA, T1
	rol	$5, T1
	add	T1, SE
	mov	OFFSET(12) (DATA), T1
	add	T2, SE
	xor	OFFSET(14) (DATA), T1
	rol	$30, SB
	mov	SC, T2
	xor	OFFSET(4) (DATA), T1
	xor	SB, T2
	xor	SA, T2
	xor	OFFSET(9) (DATA), T1
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(12) (DATA)
	mov	SE, T1
	rol	$5, T1
	add	T1, SD
	add	T2, SD
	rol	$30, SA
	mov	OFFSET(13) (DATA), T1
	xor	OFFSET(15) (DATA), T1
	mov	SB, T2
	xor	OFFSET(5) (DATA), T1
	xor	SA, T2
	xor	OFFSET(10) (DATA), T1
	xor	SE, T2
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(13) (DATA)
	mov	SD, T1
	rol	$5, T1
	add	T1, SC
	mov	OFFSET(14) (DATA), T1
	xor	OFFSET(0) (DATA), T1
	xor	OFFSET(6) (DATA), T1
	add	T2, SC
	rol	$30, SE
	mov	SA, T2
	xor	SE, T2
	xor	OFFSET(11) (DATA), T1
	xor	SD, T2
	rol	$1, T1
	lea	K4VALUE (T1, T2), T2
	mov	T1, OFFSET(14) (DATA)
	mov	SC, T1
	rol	$5, T1
	add	T1, SB
	mov	OFFSET(15) (DATA), T1
	xor	OFFSET(1) (DATA), T1
	xor	OFFSET(7) (DATA), T1
	rol	$30, SD
	add	T2, SB
	xor	OFFSET(12) (DATA), T1
	mov	SE, T2
	xor	SD, T2
	rol	$1, T1
	xor	SC, T2
	lea	K4VALUE (T1, T2), T2
	rol	$30, SC
	mov	T1, OFFSET(15) (DATA)
	mov	SB, T1
	rol	$5, T1
	add	T1, SA
	add	T2, SA

C 	C Load and byteswap data
C 	movl	88(%esp), T2
C 
C 	SWAP( 0, %eax) SWAP( 1, %ebx) SWAP( 2, %ecx) SWAP( 3, %edx)
C 	SWAP( 4, %eax) SWAP( 5, %ebx) SWAP( 6, %ecx) SWAP( 7, %edx)
C 	SWAP( 8, %eax) SWAP( 9, %ebx) SWAP(10, %ecx) SWAP(11, %edx)
C 	SWAP(12, %eax) SWAP(13, %ebx) SWAP(14, %ecx) SWAP(15, %edx)
C 
C 	C load the state vector
C 	movl	84(%esp),T1
C 	movl	(T1),   SA
C 	movl	4(T1),  SB
C 	movl	8(T1),  SC
C 	movl	12(T1), SD
C 	movl	16(T1), SE
C 
C 	ROUND_F1_NOEXP(SA, SB, SC, SD, SE,  0)
C 	ROUND_F1_NOEXP(SE, SA, SB, SC, SD,  1)
C 	ROUND_F1_NOEXP(SD, SE, SA, SB, SC,  2)
C 	ROUND_F1_NOEXP(SC, SD, SE, SA, SB,  3)
C 	ROUND_F1_NOEXP(SB, SC, SD, SE, SA,  4)
C 
C 	ROUND_F1_NOEXP(SA, SB, SC, SD, SE,  5)
C 	ROUND_F1_NOEXP(SE, SA, SB, SC, SD,  6)
C 	ROUND_F1_NOEXP(SD, SE, SA, SB, SC,  7)
C 	ROUND_F1_NOEXP(SC, SD, SE, SA, SB,  8)
C 	ROUND_F1_NOEXP(SB, SC, SD, SE, SA,  9)
C 
C 	ROUND_F1_NOEXP(SA, SB, SC, SD, SE, 10)
C 	ROUND_F1_NOEXP(SE, SA, SB, SC, SD, 11)
C 	ROUND_F1_NOEXP(SD, SE, SA, SB, SC, 12)
C 	ROUND_F1_NOEXP(SC, SD, SE, SA, SB, 13)
C 	ROUND_F1_NOEXP(SB, SC, SD, SE, SA, 14)
C 
C 	ROUND_F1_NOEXP(SA, SB, SC, SD, SE, 15)
C 	ROUND_F1(SE, SA, SB, SC, SD, 16)
C 	ROUND_F1(SD, SE, SA, SB, SC, 17)
C 	ROUND_F1(SC, SD, SE, SA, SB, 18)
C 	ROUND_F1(SB, SC, SD, SE, SA, 19)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 20, K2VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 21, K2VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 22, K2VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 23, K2VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 24, K2VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 25, K2VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 26, K2VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 27, K2VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 28, K2VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 29, K2VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 30, K2VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 31, K2VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 32, K2VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 33, K2VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 34, K2VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 35, K2VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 36, K2VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 37, K2VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 38, K2VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 39, K2VALUE)
C 
C 	ROUND_F3(SA, SB, SC, SD, SE, 40)
C 	ROUND_F3(SE, SA, SB, SC, SD, 41)
C 	ROUND_F3(SD, SE, SA, SB, SC, 42)
C 	ROUND_F3(SC, SD, SE, SA, SB, 43)
C 	ROUND_F3(SB, SC, SD, SE, SA, 44)
C 
C 	ROUND_F3(SA, SB, SC, SD, SE, 45)
C 	ROUND_F3(SE, SA, SB, SC, SD, 46)
C 	ROUND_F3(SD, SE, SA, SB, SC, 47)
C 	ROUND_F3(SC, SD, SE, SA, SB, 48)
C 	ROUND_F3(SB, SC, SD, SE, SA, 49)
C 
C 	ROUND_F3(SA, SB, SC, SD, SE, 50)
C 	ROUND_F3(SE, SA, SB, SC, SD, 51)
C 	ROUND_F3(SD, SE, SA, SB, SC, 52)
C 	ROUND_F3(SC, SD, SE, SA, SB, 53)
C 	ROUND_F3(SB, SC, SD, SE, SA, 54)
C 
C 	ROUND_F3(SA, SB, SC, SD, SE, 55)
C 	ROUND_F3(SE, SA, SB, SC, SD, 56)
C 	ROUND_F3(SD, SE, SA, SB, SC, 57)
C 	ROUND_F3(SC, SD, SE, SA, SB, 58)
C 	ROUND_F3(SB, SC, SD, SE, SA, 59)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 60, K4VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 61, K4VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 62, K4VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 63, K4VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 64, K4VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 65, K4VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 66, K4VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 67, K4VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 68, K4VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 69, K4VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 70, K4VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 71, K4VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 72, K4VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 73, K4VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 74, K4VALUE)
C 
C 	ROUND_F2(SA, SB, SC, SD, SE, 75, K4VALUE)
C 	ROUND_F2(SE, SA, SB, SC, SD, 76, K4VALUE)
C 	ROUND_F2(SD, SE, SA, SB, SC, 77, K4VALUE)
C 	ROUND_F2(SC, SD, SE, SA, SB, 78, K4VALUE)
C 	ROUND_F2(SB, SC, SD, SE, SA, 79, K4VALUE)

	C Update the state vector
	movl	84(%esp),T1
	addl	SA, (T1) 
	addl	SB, 4(T1) 
	addl	SC, 8(T1) 
	addl	SD, 12(T1) 
	addl	SE, 16(T1)

	addl	$64, %esp
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
EPILOGUE(_nettle_sha1_compress)

C TODO:

C * Extend loopmixer so that it can exploit associativity, and for
C   example reorder
C
C	add 	%eax, %ebx
C	add	%ecx, %ebx

C * Use mmx instructions for the data expansion, doing two words at a
C   time.
