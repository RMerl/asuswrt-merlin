C x86_64/sha3-permute.asm

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

define(<CTX>, <%rdi>)		C 25 64-bit values, 200 bytes.
define(<COUNT>, <%r8>)		C Avoid clobbering %rsi, for W64.

define(<A00>,  <%rax>)
define(<A0102>, <%xmm0>)
define(<A0304>, <%xmm1>)

define(<A05>,  <%rcx>)
define(<A0607>, <%xmm2>)
define(<A0809>, <%xmm3>)
	
define(<A10>,  <%rdx>)
define(<A1112>, <%xmm4>)
define(<A1314>, <%xmm5>)

define(<A15>,  <%rbp>)
define(<A1617>, <%xmm6>)
define(<A1819>, <%xmm7>)
	
define(<A20>,  <%r9>)
define(<A2122>, <%xmm8>)
define(<A2324>, <%xmm9>)

define(<C0>, <%r10>)
define(<C12>, <%xmm10>)
define(<C34>, <%xmm11>)

define(<D0>, <%r11>)
define(<D12>, <%xmm12>)
define(<D34>, <%xmm13>)

C Wide temporaries
define(<W0>, <%xmm14>)
define(<W1>, <%xmm15>)
define(<W2>, <%xmm12>)		C Overlap D12
define(<W3>, <%xmm13>)		C Overlap D34

define(<T0>, <%r12>)
define(<T1>, <%r13>)
define(<T2>, <%r11>)		C Overlap D0
define(<T3>, <%r10>)		C Overlap C0

define(<RC>, <%r14>)

define(<OFFSET>, <ifelse($1,0,,eval(8*$1))>)
define(<STATE>, <OFFSET($1)(CTX)>)

define(<SWAP64>, <pshufd	<$>0x4e,>)

define(<DIRECT_MOVQ>, <no>)

C MOVQ(src, dst), for moves between a general register and an xmm
C register.

ifelse(DIRECT_MOVQ, yes, <
C movq calls that are equal to the corresponding movd,
C where the Apple assembler requires them to be written as movd.
define(<MOVQ>, <movd	$1, $2>)
>, <
C Moving via (cached) memory is generally faster.
define(<MOVQ>, <
	movq	$1, (CTX)
	movq	(CTX), $2
>)>)

C ROTL64(rot, register, temp)
C Caller needs to or together the result.
define(<ROTL64>, <
	movdqa	$2, $3
	psllq	<$>$1, $2
	psrlq	<$>eval(64-$1), $3
>)

	.file "sha3-permute.asm"
	
	C sha3_permute(struct sha3_state *ctx)
	.text
	ALIGN(16)
PROLOGUE(nettle_sha3_permute)
	W64_ENTRY(1, 16)
	push	%rbp
	push	%r12
	push	%r13
	push	%r14

	movl	$24, XREG(COUNT)
	lea	.rc-8(%rip), RC
	movq	STATE(0), A00
	movups	STATE(1), A0102
	movups	STATE(3), A0304
	movq	A00, C0

	movq	STATE(5), A05
	movdqa	A0102, C12
	movups	STATE(6), A0607
	movdqa	A0304, C34
	movups	STATE(8), A0809
	xorq	A05, C0
	
	movq	STATE(10), A10
	pxor	A0607, C12
	movups	STATE(11), A1112
	pxor	A0809, C34
	movups	STATE(13), A1314
	xorq	A10, C0

	movq	STATE(15), A15
	pxor	A1112, C12
	movups	STATE(16), A1617
	pxor	A1314, C34
	movups	STATE(18), A1819
	xorq	A15, C0

	movq	STATE(20), A20
	pxor	A1617, C12
	movups	STATE(21), A2122
	pxor	A1819, C34
	movups	STATE(23), A2324
	xorq	A20, C0
	pxor	A2122, C12
	pxor	A2324, C34
	
	ALIGN(16)
.Loop:
	C The theta step. Combine parity bits, then xor to state.
	C D0 = C4 ^ (C1 <<< 1)
	C D1 = C0 ^ (C2 <<< 1)
	C D2 = C1 ^ (C3 <<< 1)
	C D3 = C2 ^ (C4 <<< 1)
	C D4 = C3 ^ (C0 <<< 1)

	C Shift the words around, putting (C0, C1) in D12, (C2, C3) in
	C   D34, and (C4, C0) in C34.
	
	C Notes on "unpack" instructions:
	C   punpckhqdq 01, 23 gives 31
	C   punpcklqdq 01, 23 gives 20

	SWAP64	C34, C34		C Holds C4, C3
	movdqa	C12, D34
	MOVQ(C0, D12)
	punpcklqdq	C12, D12	C Holds C0, C1
	punpckhqdq	C34, D34	C Holds C2, C3
	punpcklqdq	D12, C34	C Holds	C4, C0
	MOVQ(C34, D0)
	MOVQ(C12, T0)
	rolq	$1, T0
	xorq	T0, D0

	C Can use C12 as temporary
	movdqa	D34, W0
	movdqa	D34, W1
	psllq	$1, W0
	psrlq	$63, W1
	pxor	W0, D12
	pxor	W1, D12		C Done D12
	
	movdqa	C34, C12
	psrlq	$63, C34
	psllq	$1, C12
	pxor	C34, D34
	pxor	C12, D34	C Done D34

	xorq	D0, A00
	xorq	D0, A05
	xorq	D0, A10
	xorq	D0, A15
	xorq	D0, A20
	pxor	D12, A0102
	pxor	D12, A0607
	pxor	D12, A1112
	pxor	D12, A1617
	pxor	D12, A2122
	pxor	D34, A0304
	pxor	D34, A0809
	pxor	D34, A1314
	pxor	D34, A1819
	pxor	D34, A2324

	C theta step done, no C, D or W temporaries alive.

	C rho and pi steps. When doing the permutations, also
	C transpose the matrix.
	
	C The combined permutation + transpose gives the following
	C cycles (rotation counts in parenthesis)
	C   0 <- 0(0)
	C   1 <- 3(28) <- 4(27) <- 2(62) <- 1(1)
	C   5 <- 6(44) <- 9(20) <- 8(55) <- 5(36)
	C   7 <- 7(6)
	C   10 <- 12(43) <- 13(25) <- 11(10) <- 10(3)
	C   14 <- 14(39)
	C   15 <- 18(21) <- 17(15) <- 19(8) <- 15(41)
	C   16 <- 16(45)
	C   20 <- 24(14) <- 21(2) <- 22(61) <- 20(18)
	C   23 <- 23(56)

	C Do the 1,2,3,4 row. First rotate, then permute.
	movdqa	A0102, W0
	movdqa	A0102, W1
	movdqa	A0102, W2
	psllq	$1, A0102
	psrlq	$63, W0
	psllq	$62, W1
	por	A0102, W0	C rotl 1  (A01)
	psrlq	$2, W2
	por	W1, W2		C rotl 62 (A02)

	movdqa	A0304, A0102
	movdqa	A0304, W1
	psllq	$28, A0102
	psrlq	$36, W1
	por	W1, A0102	C rotl 28 (A03)
	movdqa	A0304, W1
	psllq	$27, A0304
	psrlq	$37, W1
	por	W1, A0304	C rotl 27 (A04)
	
	punpcklqdq	W0, A0102
	punpckhqdq	W2, A0304

	C 5 <- 6(44) <- 9(20) <- 8(55) <- 5(36)
	C 7 <- 7(6)
        C      __   _______
	C  _ L'  ` L_    __`
	C |5|    |6|7|  |8|9|
	C   `-_________-^`-^
	
	rolq	$36, A05
	MOVQ(A05, W0)
	MOVQ(A0607, A05)
	rolq	$44, A05		C Done A05
	ROTL64(6, A0607, W1)
	por	A0607, W1
	movdqa	A0809, A0607
	ROTL64(20, A0607, W2)
	por	W2, A0607
	punpckhqdq	W1, A0607	C Done A0607
	ROTL64(55, A0809, W1)
	por	A0809, W1
	movdqa W0, A0809
	punpcklqdq	W1, A0809	C Done 0809

	C   10 <- 12(43) <- 13(25) <- 11(10) <- 10(3)
	C   14 <- 14(39)
        C      _____   ___
	C  __L'   __`_L_  `_____
	C |10|   |11|12|  |13|14|
	C   `-___-^`-______-^ 
	C

	rolq	$42, A10		C 42 + 25 = 3 (mod 64)
	SWAP64	A1112, W0
	MOVQ(A10, A1112)
	MOVQ(W0, A10)
	rolq	$43, A10		C Done A10

	punpcklqdq	A1314, A1112
	ROTL64(25, A1112, W1)
	por	W1, A1112		C Done A1112
	ROTL64(39, A1314, W2)
	por	A1314, W2
	ROTL64(10, W0, A1314)
	por	W0, A1314
	punpckhqdq	W2, A1314	C Done A1314
	
	
	C   15 <- 18(21) <- 17(15) <- 19(8) <- 15(41)
	C   16 <- 16(45)
	C      _____________
	C     /         _______
	C  _L'    ____L'    |  `_
	C |15|   |16|17|   |18|19|
	C   \        `_____-^   ^
	C    \_________________/

	SWAP64	A1819, W0
	rolq	$41, A15
	MOVQ(A15, W1)
	MOVQ(A1819, A15)
	rolq	$21, A15		C Done A15
	SWAP64	A1617, A1819
	ROTL64(45, A1617, W2)
	por	W2, A1617
	ROTL64(8, W0, W3)
	por	W3, W0
	punpcklqdq	W0, A1617	C Done A1617
	ROTL64(15, A1819, W2)
	por	W2, A1819
	punpcklqdq	W1, A1819	C Done A1819
	
	C   20 <- 24(14) <- 21(2) <- 22(61) <- 20(18)
	C   23 <- 23(56)
	C      _______________
	C     /               \
	C  _L'    _L'\_     ___`_
	C |20|   |21|22|   |23|24|
	C   \     `__ ^________-^
	C    \_______/

	rolq	$18, A20
	MOVQ(A20, W0)
	SWAP64	A2324, W1
	movd	W1, A20
	rolq	$14, A20		C Done A20
	ROTL64(56, A2324, W1)
	por	W1, A2324
	
	movdqa	A2122, W2
	ROTL64(2, W2, W1)
	por	W1, W2
	punpcklqdq	W2, A2324	C Done A2324

	ROTL64(61, A2122, W1)
	por	W1, A2122
	psrldq	$8, A2122
	punpcklqdq	W0, A2122	C Done A2122

	C chi step. With the transposed matrix, applied independently
	C to each column.
	movq	A05, T0
	notq	T0
	andq	A10, T0
	movq	A10, T1
	notq	T1
	andq	A15, T1
	movq	A15, T2
	notq	T2
	andq	A20, T2
	xorq	T2, A10
	movq	A20, T3
	notq	T3
	andq	A00, T3
	xorq	T3, A15
	movq	A00, T2
	notq	T2
	andq	A05, T2
	xorq	T2, A20
	xorq	T0, A00
	xorq	T1, A05

	movdqa	A0607, W0
	pandn	A1112, W0
	movdqa	A1112, W1
	pandn	A1617, W1
	movdqa	A1617, W2
	pandn	A2122, W2
	pxor	W2, A1112
	movdqa	A2122, W3
	pandn	A0102, W3
	pxor	W3, A1617
	movdqa	A0102, W2
	pandn	A0607, W2
	pxor	W2, A2122
	pxor	W0, A0102
	pxor	W1, A0607

	movdqa	A0809, W0
	pandn	A1314, W0
	movdqa	A1314, W1
	pandn	A1819, W1
	movdqa	A1819, W2
	pandn	A2324, W2
	pxor	W2, A1314
	movdqa	A2324, W3
	pandn	A0304, W3
	pxor	W3, A1819
	movdqa	A0304, W2
	pandn	A0809, W2
	pxor	W2, A2324
	pxor	W0, A0304
	pxor	W1, A0809

	xorq	(RC, COUNT, 8), A00

	C Transpose.
	C Swap (A05, A10) <->  A0102, and (A15, A20) <->  A0304,
	C and also copy to C12 and C34 while at it.
	
	MOVQ(A05, C12)
	MOVQ(A15, C34)
	MOVQ(A10, W0)
	MOVQ(A20, W1)
	movq	A00, C0
	punpcklqdq	W0, C12
	punpcklqdq	W1, C34
	MOVQ(A0102, A05)
	MOVQ(A0304, A15)
	psrldq	$8, A0102
	psrldq	$8, A0304
	xorq	A05, C0
	xorq	A15, C0
	MOVQ(A0102, A10)
	MOVQ(A0304, A20)

	movdqa	C12, A0102
	movdqa	C34, A0304

	C Transpose (A0607, A1112)
	movdqa	A0607, W0
	punpcklqdq	A1112, A0607
	xorq	A10, C0
	xorq	A20, C0
	punpckhqdq	W0, A1112
	SWAP64	A1112, A1112

	C Transpose (A1819, A2324)
	movdqa	A1819, W0
	punpcklqdq	A2324, A1819
	pxor	A0607, C12
	pxor	A1112, C12
	punpckhqdq	W0, A2324
	SWAP64	A2324, A2324

	C Transpose (A0809, A1314) and (A1617, A2122), and swap
	movdqa	A0809, W0
	movdqa	A1314, W1
	movdqa	A1617, A0809
	movdqa	A2122, A1314
	pxor	A1819, C34
	pxor	A2324, C34
	punpcklqdq	A2122, A0809
	punpckhqdq	A1617, A1314
	SWAP64	A1314, A1314
	movdqa	W0, A1617
	movdqa	W1, A2122
	pxor	A0809, C34
	pxor	A1314, C34
	punpcklqdq	W1, A1617
	punpckhqdq	W0, A2122
	SWAP64	A2122, A2122

	decl	XREG(COUNT)
	pxor	A1617, C12
	pxor	A2122, C12
	jnz	.Loop

	movq	A00, STATE(0)
	movups	A0102, STATE(1)
	movups	A0304, STATE(3)

	movq	A05, STATE(5)
	movups	A0607, STATE(6)
	movups	A0809, STATE(8)
		               
	movq	A10, STATE(10)
	movups	A1112, STATE(11)
	movups	A1314, STATE(13)
		               
	movq	A15, STATE(15)
	movups	A1617, STATE(16)
	movups	A1819, STATE(18)
		               
	movq	A20, STATE(20)
	movups	A2122, STATE(21)
	movups	A2324, STATE(23)

	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	W64_EXIT(1, 16)
	ret

EPILOGUE(nettle_sha3_permute)

ALIGN(16)
.rc:	C In reverse order
	.quad	0x8000000080008008
	.quad	0x0000000080000001
	.quad	0x8000000000008080
	.quad	0x8000000080008081
	.quad	0x800000008000000A
	.quad	0x000000000000800A
	.quad	0x8000000000000080
	.quad	0x8000000000008002
	.quad	0x8000000000008003
	.quad	0x8000000000008089
	.quad	0x800000000000008B
	.quad	0x000000008000808B
	.quad	0x000000008000000A
	.quad	0x0000000080008009
	.quad	0x0000000000000088
	.quad	0x000000000000008A
	.quad	0x8000000000008009
	.quad	0x8000000080008081
	.quad	0x0000000080000001
	.quad	0x000000000000808B
	.quad	0x8000000080008000
	.quad	0x800000000000808A
	.quad	0x0000000000008082
	.quad	0x0000000000000001
