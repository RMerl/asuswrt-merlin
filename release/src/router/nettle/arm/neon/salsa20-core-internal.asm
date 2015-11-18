C arm/neon/salsa20-core-internal.asm

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

	.file "salsa20-core-internal.asm"
	.fpu	neon

define(<DST>, <r0>)
define(<SRC>, <r1>)
define(<ROUNDS>, <r2>)

define(<X0>, <q0>)
define(<X1>, <q1>)
define(<X2>, <q2>)
define(<X3>, <q3>)
define(<T0>, <q8>)
define(<T1>, <q9>)
define(<M0101>, <q10>)
define(<M0110>, <q11>)
define(<M0011>, <q12>)
define(<S1>, <q13>)
define(<S2>, <q14>)
define(<S3>, <q15>)
	
define(<QROUND>, <
	vadd.i32	T0, $1, $4
	vshl.i32	T1, T0, #7
	vshr.u32	T0, T0, #25
	veor		$2, $2, T0
	veor		$2, $2, T1

	vadd.i32	T0, $1, $2
	vshl.i32	T1, T0, #9
	vshr.u32	T0, T0, #23
	veor		$3, $3, T0
	veor		$3, $3, T1

	vadd.i32	T0, $2, $3
	vshl.i32	T1, T0, #13
	vshr.u32	T0, T0, #19
	veor		$4, $4, T0
	veor		$4, $4, T1

	vadd.i32	T0, $3, $4
	vshl.i32	T1, T0, #18
	vshr.u32	T0, T0, #14
	veor		$1, $1, T0
	veor		$1, $1, T1
>)
	
	.text
	.align 4
.Lmasks:
	.int 0,-1, 0,-1
	.int 0,-1,-1, 0
	.int 0, 0,-1,-1

	C _salsa20_core(uint32_t *dst, const uint32_t *src, unsigned rounds)

PROLOGUE(_nettle_salsa20_core)
	vldm	SRC, {X0,X1,X2,X3}

	C Input rows:
	C	 0  1  2  3	X0
	C	 4  5  6  7	X1
	C	 8  9 10 11	X2
	C	12 13 14 15	X3
	C Permuted to:
	C	 0  5 10 15
	C	 4  9 14  3
	C	 8 13  2  7
	C	12  1  6 11

	C FIXME: Construct in some other way?
	adr	r12, .Lmasks
	vldm	r12, {M0101, M0110, M0011}

	vmov	S1, X1
	vmov	S2, X2
	vmov	S3, X3

	C Swaps in columns 1, 3:
	C	 0  5  2  7	X0 ^
	C	 4  1  6  3	T0 v
	C	 8 13 10 15	T1  ^
	C	12  9 14 11	X3  v
	vmov	T0, X1
	vmov	T1, X2
	vbit	T0, X0, M0101
	vbit	X0, X1, M0101
	vbit	T1, X3, M0101
	vbit	X3, X2, M0101

	C Swaps in column 1, 2:
	C	 0  5  2  7	X0
	C	 4  9 14  3	X1 ^
	C	 8 13 10 15	T1 |
	C	12  1  6 11	X3 v
	vmov	X1, T0
	vbit	X1, X3, M0110
	vbit	X3, T0, M0110

	C Swaps in columm 2,3:
	C	 0  5 10 15	X0 ^
	C	 4  9 14  3	X1 |
	C	 8 13  2  7	X2 v
	C	12  1  6 11	X3
	vmov	X2, T1
	vbit	X2, X0, M0011
	vbit	X0, T1, M0011

.Loop:
	QROUND(X0, X1, X2, X3)

	C Rotate rows, to get
	C	 0  5 10 15
	C	 3  4  9 14  >>> 1
	C	 2  7  8 13  >>> 2
	C	 1  6 11 12  >>> 3
	vext.32	X1, X1, X1, #3
	vext.32	X2, X2, X2, #2
	vext.32	X3, X3, X3, #1

	QROUND(X0, X3, X2, X1)

	subs	ROUNDS, ROUNDS, #2
	C Inverse rotation
	vext.32	X1, X1, X1, #1
	vext.32	X2, X2, X2, #2
	vext.32	X3, X3, X3, #3

	bhi	.Loop

	C Inverse swaps
	vmov	T1, X2
	vbit	T1, X0, M0011
	vbit	X0, X2, M0011

	vmov	T0, X1
	vbit	T0, X3, M0110
	vbit	X3, X1, M0110

	vmov	X1, T0
	vmov	X2, T1
	vbit	X1, X0, M0101
	vbit	X0, T0, M0101
	vbit	X2, X3, M0101
	vbit	X3, T1, M0101

	vld1.64	{T0}, [SRC]
	vadd.u32	X0, X0, T0
	vadd.u32	X1, X1, S1
	vadd.u32	X2, X2, S2
	vadd.u32	X3, X3, S3

	vstm	DST, {X0,X1,X2,X3}
	bx	lr
EPILOGUE(_nettle_salsa20_core)

divert(-1)
define salsastate
p/x $q0.u32
p/x $q1.u32
p/x $q2.u32
p/x $q3.u32
end
