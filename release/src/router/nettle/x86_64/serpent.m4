C x86_64/serpent.m4

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
	
C WROL(count, w)
define(<WROL>, <
	movdqa	$2, T0
	pslld	<$>$1, $2
	psrld	<$>eval(32 - $1), T0
	por	T0, $2
>)

C Note: Diagrams use little-endian representation, with least
C significant word to the left.
	
C Transpose values from:
C     +----+----+----+----+
C x0: | a0 | a1 | a2 | a3 |
C x1: | b0 | b1 | b2 | b3 |
C x2: | c0 | c1 | c2 | c3 |
C x3: | d0 | d1 | d2 | d3 |
C     +----+----+----+----+
C To:
C     +----+----+----+----+
C x0: | a0 | b0 | c0 | d0 |
C x1: | a1 | b1 | c1 | d1 |
C x2: | a2 | b2 | c2 | d2 |
C x3: | a3 | b3 | c3 | d3 |
C     +----+----+----+----+

define(<WTRANSPOSE>, <
	movdqa		$1, T0
	punpcklqdq	$3, T0			C |a0 a1 c0 c1|
	punpckhqdq	$3, $1			C |a2 a3 c2 c3|
	pshufd		<$>0xd8, T0, T0		C |a0 c0 a1 c1|
	pshufd		<$>0xd8, $1, T1		C |a2 c2 a3 c3|
	
	movdqa		$2, T2
	punpcklqdq	$4, T2			C |b0 b1 d0 11|
	punpckhqdq	$4, $2			C |b2 b3 d2 d3|
	pshufd		<$>0xd8, T2, T2		C |b0 d0 b1 d1|
	pshufd		<$>0xd8, $2, T3		C |b2 d2 b3 d3|

	movdqa		T0, $1
	punpckldq	T2, $1			C |a0 b0 c0 d0|
	movdqa		T0, $2
	punpckhdq	T2, $2			C |a1 b1 c1 d1|

	movdqa		T1, $3
	punpckldq	T3, $3			C |a2 b2 c2 d2|
	movdqa		T1, $4
	punpckhdq	T3, $4			C |a3 b3 c3 d3|
>)

C FIXME: Arrange 16-byte alignment, so we can use movaps?
define(<WKEYXOR>, <
	movups	$1(CTX, CNT), T0
	pshufd	<$>0x55, T0, T1
	pshufd	<$>0xaa, T0, T2
	pxor	T1, $3
	pxor	T2, $4
	pshufd	<$>0xff, T0, T1
	pshufd	<$>0x00, T0, T0
	pxor	T1, $5
	pxor	T0, $2
>)
