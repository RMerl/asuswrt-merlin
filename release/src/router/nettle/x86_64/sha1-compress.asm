C x86_64/sha1-compress.asm

ifelse(<
   Copyright (C) 2004, 2008, 2013 Niels Möller

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

C Register usage. KVALUE and INPUT share a register.
define(<SA>,<%eax>)dnl
define(<SB>,<%r8d>)dnl
define(<SC>,<%ecx>)dnl
define(<SD>,<%edx>)dnl
define(<SE>,<%r9d>)dnl
define(<DATA>,<%rsp>)dnl
define(<T1>,<%r10d>)dnl
define(<T2>,<%r11d>)dnl
define(<KVALUE>, <%esi>)dnl			

C Arguments
define(<STATE>,<%rdi>)dnl
define(<INPUT>,<%rsi>)dnl

C Constants
define(<K1VALUE>, <<$>0x5A827999>)dnl		C  Rounds  0-19
define(<K2VALUE>, <<$>0x6ED9EBA1>)dnl		C  Rounds 20-39
define(<K3VALUE>, <<$>0x8F1BBCDC>)dnl		C  Rounds 40-59
define(<K4VALUE>, <<$>0xCA62C1D6>)dnl		C  Rounds 60-79
	
C Reads the input into register, byteswaps it, and stores it in the DATA array.
C SWAP(index, register)
define(<SWAP>, <
	movl	OFFSET($1)(INPUT), $2
	bswap	$2
	movl	$2, OFFSET($1) (DATA)
>)dnl

C The f functions,
C
C  f1(x,y,z) = z ^ (x & (y ^ z))
C  f2(x,y,z) = x ^ y ^ z
C  f3(x,y,z) = (x & y) | (z & (x | y))
C	     = (x & (y ^ z)) + (y & z)
C  f4 = f2

C This form for f3 was suggested by George Spelvin. The terms can be
C added into the result one at a time, saving one temporary.

C expand(i) is the expansion function
C
C   W[i] = (W[i - 16] ^ W[i - 14] ^ W[i - 8] ^ W[i - 3]) <<< 1
C
C where W[i] is stored in DATA[i mod 16].

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
	movl	OFFSET(eval($6 % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	roll	<$>1, T1
	movl	T1, OFFSET(eval($6 % 16)) (DATA)
	movl	$4, T2
	xorl	$3, T2
	andl	$2, T2
	xorl	$4, T2
	roll	<$>30, $2
	addl	T1, $5
	addl	KVALUE, $5
	movl	$1, T1
	roll	<$>5, T1
	addl	T1, $5
	addl	T2, $5
>)

dnl ROUND_F1_NOEXP(a, b, c, d, e, i)
define(<ROUND_F1_NOEXP>, <
	movl	$4, T2
	xorl	$3, T2
	movl	$1, T1
	andl	$2, T2
	addl	OFFSET($6) (DATA), $5
	xorl	$4, T2
	addl	T2, $5
	roll	<$>30, $2
	roll	<$>5, T1
	addl	T1, $5
	addl	KVALUE, $5
>)

dnl ROUND_F2(a, b, c, d, e, i)
define(<ROUND_F2>, <
	movl	OFFSET(eval($6 % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	roll	<$>1, T1
	movl	T1, OFFSET(eval($6 % 16)) (DATA)
	movl	$4, T2
	xorl	$3, T2
	xorl	$2, T2
	roll	<$>30, $2
	addl	T1, $5
	addl	KVALUE, $5
	movl	$1, T1
	roll	<$>5, T1
	addl	T1, $5
	addl	T2, $5
>)

dnl ROUND_F3(a, b, c, d, e, i)
define(<ROUND_F3>, <
	movl	OFFSET(eval($6 % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  2) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 +  8) % 16)) (DATA), T1
	xorl	OFFSET(eval(($6 + 13) % 16)) (DATA), T1
	roll	<$>1, T1
	movl	T1, OFFSET(eval($6 % 16)) (DATA)
	movl	$4, T2
	andl	$3, T2
	addl	T1, $5
 	addl	KVALUE, $5
	movl	$4, T1
	xorl	$3, T1
	andl	$2, T1
	addl	T2, $5
	roll	<$>30, $2
	movl	$1, T2
	roll	<$>5, T2
	addl	T1, $5
	addl	T2, $5
>)

	.file "sha1-compress.asm"

	C _nettle_sha1_compress(uint32_t *state, uint8_t *input)
	
	.text
	ALIGN(16)
PROLOGUE(_nettle_sha1_compress)
	C save all registers that need to be saved
	W64_ENTRY(2, 0)
	
	sub	$64, %rsp	C  %rsp = W

	C Load and byteswap data
	SWAP( 0, SA) SWAP( 1, SB) SWAP( 2, SC) SWAP( 3, SD)
	SWAP( 4, SA) SWAP( 5, SB) SWAP( 6, SC) SWAP( 7, SD)
	SWAP( 8, SA) SWAP( 9, SB) SWAP(10, SC) SWAP(11, SD)
	SWAP(12, SA) SWAP(13, SB) SWAP(14, SC) SWAP(15, SD)

	C Load the state vector
	movl	  (STATE), SA
	movl	 4(STATE), SB
	movl	 8(STATE), SC
	movl	12(STATE), SD
	movl	16(STATE), SE

	movl	K1VALUE, KVALUE
	ROUND_F1_NOEXP(SA, SB, SC, SD, SE,  0)
	ROUND_F1_NOEXP(SE, SA, SB, SC, SD,  1)
	ROUND_F1_NOEXP(SD, SE, SA, SB, SC,  2)
	ROUND_F1_NOEXP(SC, SD, SE, SA, SB,  3)
	ROUND_F1_NOEXP(SB, SC, SD, SE, SA,  4)

	ROUND_F1_NOEXP(SA, SB, SC, SD, SE,  5)
	ROUND_F1_NOEXP(SE, SA, SB, SC, SD,  6)
	ROUND_F1_NOEXP(SD, SE, SA, SB, SC,  7)
	ROUND_F1_NOEXP(SC, SD, SE, SA, SB,  8)
	ROUND_F1_NOEXP(SB, SC, SD, SE, SA,  9)

	ROUND_F1_NOEXP(SA, SB, SC, SD, SE, 10)
	ROUND_F1_NOEXP(SE, SA, SB, SC, SD, 11)
	ROUND_F1_NOEXP(SD, SE, SA, SB, SC, 12)
	ROUND_F1_NOEXP(SC, SD, SE, SA, SB, 13)
	ROUND_F1_NOEXP(SB, SC, SD, SE, SA, 14)

	ROUND_F1_NOEXP(SA, SB, SC, SD, SE, 15)
	ROUND_F1(SE, SA, SB, SC, SD, 16)
	ROUND_F1(SD, SE, SA, SB, SC, 17)
	ROUND_F1(SC, SD, SE, SA, SB, 18)
	ROUND_F1(SB, SC, SD, SE, SA, 19)

	movl	K2VALUE, KVALUE
	ROUND_F2(SA, SB, SC, SD, SE, 20)
	ROUND_F2(SE, SA, SB, SC, SD, 21)
	ROUND_F2(SD, SE, SA, SB, SC, 22)
	ROUND_F2(SC, SD, SE, SA, SB, 23)
	ROUND_F2(SB, SC, SD, SE, SA, 24)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 25)
	ROUND_F2(SE, SA, SB, SC, SD, 26)
	ROUND_F2(SD, SE, SA, SB, SC, 27)
	ROUND_F2(SC, SD, SE, SA, SB, 28)
	ROUND_F2(SB, SC, SD, SE, SA, 29)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 30)
	ROUND_F2(SE, SA, SB, SC, SD, 31)
	ROUND_F2(SD, SE, SA, SB, SC, 32)
	ROUND_F2(SC, SD, SE, SA, SB, 33)
	ROUND_F2(SB, SC, SD, SE, SA, 34)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 35)
	ROUND_F2(SE, SA, SB, SC, SD, 36)
	ROUND_F2(SD, SE, SA, SB, SC, 37)
	ROUND_F2(SC, SD, SE, SA, SB, 38)
	ROUND_F2(SB, SC, SD, SE, SA, 39)

	movl	K3VALUE, KVALUE
	ROUND_F3(SA, SB, SC, SD, SE, 40)
	ROUND_F3(SE, SA, SB, SC, SD, 41)
	ROUND_F3(SD, SE, SA, SB, SC, 42)
	ROUND_F3(SC, SD, SE, SA, SB, 43)
	ROUND_F3(SB, SC, SD, SE, SA, 44)
				     
	ROUND_F3(SA, SB, SC, SD, SE, 45)
	ROUND_F3(SE, SA, SB, SC, SD, 46)
	ROUND_F3(SD, SE, SA, SB, SC, 47)
	ROUND_F3(SC, SD, SE, SA, SB, 48)
	ROUND_F3(SB, SC, SD, SE, SA, 49)
				     
	ROUND_F3(SA, SB, SC, SD, SE, 50)
	ROUND_F3(SE, SA, SB, SC, SD, 51)
	ROUND_F3(SD, SE, SA, SB, SC, 52)
	ROUND_F3(SC, SD, SE, SA, SB, 53)
	ROUND_F3(SB, SC, SD, SE, SA, 54)
				     
	ROUND_F3(SA, SB, SC, SD, SE, 55)
	ROUND_F3(SE, SA, SB, SC, SD, 56)
	ROUND_F3(SD, SE, SA, SB, SC, 57)
	ROUND_F3(SC, SD, SE, SA, SB, 58)
	ROUND_F3(SB, SC, SD, SE, SA, 59)

	movl	K4VALUE, KVALUE
	ROUND_F2(SA, SB, SC, SD, SE, 60)
	ROUND_F2(SE, SA, SB, SC, SD, 61)
	ROUND_F2(SD, SE, SA, SB, SC, 62)
	ROUND_F2(SC, SD, SE, SA, SB, 63)
	ROUND_F2(SB, SC, SD, SE, SA, 64)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 65)
	ROUND_F2(SE, SA, SB, SC, SD, 66)
	ROUND_F2(SD, SE, SA, SB, SC, 67)
	ROUND_F2(SC, SD, SE, SA, SB, 68)
	ROUND_F2(SB, SC, SD, SE, SA, 69)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 70)
	ROUND_F2(SE, SA, SB, SC, SD, 71)
	ROUND_F2(SD, SE, SA, SB, SC, 72)
	ROUND_F2(SC, SD, SE, SA, SB, 73)
	ROUND_F2(SB, SC, SD, SE, SA, 74)
				     
	ROUND_F2(SA, SB, SC, SD, SE, 75)
	ROUND_F2(SE, SA, SB, SC, SD, 76)
	ROUND_F2(SD, SE, SA, SB, SC, 77)
	ROUND_F2(SC, SD, SE, SA, SB, 78)
	ROUND_F2(SB, SC, SD, SE, SA, 79)

	C Update the state vector
	addl	SA,   (STATE) 
	addl	SB,  4(STATE) 
	addl	SC,  8(STATE) 
	addl	SD, 12(STATE) 
	addl	SE, 16(STATE)

	add	$64, %rsp
	W64_EXIT(2, 0)
	ret
EPILOGUE(_nettle_sha1_compress)
