C arm/neon/sha3-permute.asm

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

	.file "sha3-permute.asm"
	.fpu	neon

define(<CTX>, <r0>)
define(<COUNT>, <r1>)
define(<RC>, <r2>)
C First column
define(<A0>, <d0>)
define(<A5>, <d2>)
define(<A10>, <d3>)
define(<A15>, <d4>)
define(<A20>, <d5>)

define(<A1>, <d6>)
define(<A2>, <d7>)
define(<A3>, <d8>)
define(<A4>, <d9>)

define(<A6>, <d16>)
define(<A7>, <d17>)
define(<A8>, <d18>)
define(<A9>, <d19>)

define(<A11>, <d20>)
define(<A12>, <d21>)
define(<A13>, <d22>)
define(<A14>, <d23>)

define(<A16>, <d24>)
define(<A17>, <d25>)
define(<A18>, <d26>)
define(<A19>, <d27>)

define(<A21>, <d28>)
define(<A22>, <d29>)
define(<A23>, <d30>)
define(<A24>, <d31>)

define(<T0>, <d10>)
define(<T1>, <d11>)

define(<C0>, <d1>)
define(<C1>, <d12>)
define(<C2>, <d13>)
define(<C3>, <d14>)
define(<C4>, <d15>)


C ROL(DST, SRC, COUNT)
C Must have SRC != DST
define(<ROL>, <
	vshr.u64	$1, $2, #eval(64-$3)
	vsli.i64	$1, $2, #$3
	>)
C sha3_permute(struct sha3_ctx *ctx)

	.text
	.align	3
.Lrc:
	.quad	0x0000000000000001
	.quad	0x0000000000008082
	.quad	0x800000000000808A
	.quad	0x8000000080008000
	.quad	0x000000000000808B
	.quad	0x0000000080000001
	.quad	0x8000000080008081
	.quad	0x8000000000008009
	.quad	0x000000000000008A
	.quad	0x0000000000000088
	.quad	0x0000000080008009
	.quad	0x000000008000000A
	.quad	0x000000008000808B
	.quad	0x800000000000008B
	.quad	0x8000000000008089
	.quad	0x8000000000008003
	.quad	0x8000000000008002
	.quad	0x8000000000000080
	.quad	0x000000000000800A
	.quad	0x800000008000000A
	.quad	0x8000000080008081
	.quad	0x8000000000008080
	.quad	0x0000000080000001
	.quad	0x8000000080008008
	
PROLOGUE(nettle_sha3_permute)
	vpush	{d8-d15}

	vld1.64	{A0}, [CTX]!
	vldm	CTX!, {A1,A2,A3,A4}
	vld1.64	{A5}, [CTX]!
	vldm	CTX!, {A6,A7,A8,A9}
	vld1.64	{A10}, [CTX]!
	vldm	CTX!, {A11,A12,A13,A14}
	vld1.64	{A15}, [CTX]!
	vldm	CTX!, {A16,A17,A18,A19}
	vld1.64	{A20}, [CTX]!
	vldm	CTX, {A21,A22,A23,A24}
	sub	CTX, CTX, #168

	mov	COUNT, #24
	adr	RC, .Lrc

	.align 3
.Loop:
	veor	QREG(T0), QREG(A5), QREG(A15)
	veor	C0, A0, T0
	veor	C0, C0, T1
	veor	QREG(C1), QREG(A1), QREG(A6)
	veor	QREG(C1), QREG(C1), QREG(A11)
	veor	QREG(C1), QREG(C1), QREG(A16)
	veor	QREG(C1), QREG(C1), QREG(A21)

	veor	QREG(C3), QREG(A3), QREG(A8)
	veor	QREG(C3), QREG(C3), QREG(A13)
	veor	QREG(C3), QREG(C3), QREG(A18)
	veor	QREG(C3), QREG(C3), QREG(A23)

	C 	D0 = C4 ^ (C1 <<< 1)
	C 	NOTE: Using ROL macro (and vsli) is slightly slower.
 	vshl.i64	T0, C1, #1
 	vshr.u64	T1, C1, #63
	veor	T0, T0, C4
	veor	T0, T0, T1
	vmov	T1, T0
	veor	A0, A0, T0
	veor	QREG(A5), QREG(A5), QREG(T0)
	veor	QREG(A15), QREG(A15), QREG(T0)
	
	C 	D1 = C0 ^ (C2 <<< 1)
	C 	D2 = C1 ^ (C3 <<< 1)
	ROL(T0, C2, 1)
	ROL(T1, C3, 1)
	veor	T0, T0, C0
	veor	T1, T1, C1
	veor	QREG(A1), QREG(A1), QREG(T0)
	veor	QREG(A6), QREG(A6), QREG(T0)
	veor	QREG(A11), QREG(A11), QREG(T0)
	veor	QREG(A16), QREG(A16), QREG(T0)
	veor	QREG(A21), QREG(A21), QREG(T0)

	C 	D3 = C2 ^ (C4 <<< 1)
	C 	D4 = C3 ^ (C0 <<< 1)
	ROL(T0, C4, 1)
	ROL(T1, C0, 1)
	veor	T0, T0, C2
	veor	T1, T1, C3
	veor	QREG(A3), QREG(A3), QREG(T0)
	veor	QREG(A8), QREG(A8), QREG(T0)
	veor	QREG(A13), QREG(A13), QREG(T0)
	veor	QREG(A18), QREG(A18), QREG(T0)
	veor	QREG(A23), QREG(A23), QREG(T0)

	ROL( T0,  A1,  1)
	ROL( A1,  A6, 44)
	ROL( A6,  A9, 20)
	ROL( A9, A22, 61)
	ROL(A22, A14, 39)
	ROL(A14, A20, 18)
	ROL(A20,  A2, 62)
	ROL( A2, A12, 43)
	ROL(A12, A13, 25)
	ROL(A13, A19,  8)
	ROL(A19, A23, 56)
	ROL(A23, A15, 41)
	ROL(A15,  A4, 27)
	ROL( A4, A24, 14)
	ROL(A24, A21,  2)
	ROL(A21,  A8, 55)
	ROL( A8, A16, 45)
	ROL(A16,  A5, 36)
	ROL( A5,  A3, 28)
	ROL( A3, A18, 21)
	ROL(A18, A17, 15)
	ROL(A17, A11, 10)
	ROL(A11,  A7,  6)
	ROL( A7, A10,  3)
	C New A10 value left in T0

	vbic	C0, A2, A1
	vbic	C1, A3, A2
	vbic	C2, A4, A3
	vbic	C3, A0, A4
	vbic	C4, A1, A0

	veor	A0, A0, C0
	vld1.64	{C0}, [RC :64]!
	veor	QREG(A1), QREG(A1), QREG(C1)
	veor	QREG(A3), QREG(A3), QREG(C3)
	veor	A0, A0, C0

	vbic	C0, A7, A6
	vbic	C1, A8, A7
	vbic	C2, A9, A8
	vbic	C3, A5, A9
	vbic	C4, A6, A5

	veor	A5, A5, C0
	veor	QREG(A6), QREG(A6), QREG(C1)
	veor	QREG(A8), QREG(A8), QREG(C3)

	vbic	C0, A12, A11
	vbic	C1, A13, A12
	vbic	C2, A14, A13
	vbic	C3, T0, A14
	vbic	C4, A11, T0

	veor	A10, T0, C0
	veor	QREG(A11), QREG(A11), QREG(C1)
	veor	QREG(A13), QREG(A13), QREG(C3)

	vbic	C0, A17, A16
	vbic	C1, A18, A17
	vbic	C2, A19, A18
	vbic	C3, A15, A19
	vbic	C4, A16, A15

	veor	A15, A15, C0
	veor	QREG(A16), QREG(A16), QREG(C1)
	veor	QREG(A18), QREG(A18), QREG(C3)

	vbic	C0, A22, A21
	vbic	C1, A23, A22
	vbic	C2, A24, A23
	vbic	C3, A20, A24
	vbic	C4, A21, A20

	subs	COUNT, COUNT, #1
	veor	A20, A20, C0
	veor	QREG(A21), QREG(A21), QREG(C1)
	veor	QREG(A23), QREG(A23), QREG(C3)

	bne	.Loop

	vst1.64	{A0}, [CTX]!
	vstm	CTX!, {A1,A2,A3,A4}
	vst1.64	{A5}, [CTX]!
	vstm	CTX!, {A6,A7,A8,A9}
	vst1.64	{A10}, [CTX]!
	vstm	CTX!, {A11,A12,A13,A14}
	vst1.64	{A15}, [CTX]!
	vstm	CTX!, {A16,A17,A18,A19}
	vst1.64	{A20}, [CTX]!
	vstm	CTX, {A21,A22,A23,A24}
	
	vpop	{d8-d15}
	bx	lr
EPILOGUE(nettle_sha3_permute)
