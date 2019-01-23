C arm/neon/chacha-core-internal.asm

ifelse(<
   Copyright (C) 2013, 2015 Niels Möller

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

	.file "chacha-core-internal.asm"
	.fpu	neon

define(<DST>, <r0>)
define(<SRC>, <r1>)
define(<ROUNDS>, <r2>)

define(<X0>, <q0>)
define(<X1>, <q1>)
define(<X2>, <q2>)
define(<X3>, <q3>)
define(<T0>, <q8>)
define(<S0>, <q12>)
define(<S1>, <q13>)
define(<S2>, <q14>)
define(<S3>, <q15>)

define(<QROUND>, <
	C x0 += x1, x3 ^= x0, x3 lrot 16
	C x2 += x3, x1 ^= x2, x1 lrot 12
	C x0 += x1, x3 ^= x0, x3 lrot 8
	C x2 += x3, x1 ^= x2, x1 lrot 7

	vadd.i32	$1, $1, $2
	veor		$4, $4, $1
	vshl.i32	T0, $4, #16
	vshr.u32	$4, $4, #16
	veor		$4, $4, T0

	vadd.i32	$3, $3, $4
	veor		$2, $2, $3
	vshl.i32	T0, $2, #12
	vshr.u32	$2, $2, #20
	veor		$2, $2, T0

	vadd.i32	$1, $1, $2
	veor		$4, $4, $1
	vshl.i32	T0, $4, #8
	vshr.u32	$4, $4, #24
	veor		$4, $4, T0

	vadd.i32	$3, $3, $4
	veor		$2, $2, $3
	vshl.i32	T0, $2, #7
	vshr.u32	$2, $2, #25
	veor		$2, $2, T0
>)

	.text
	.align 4
	C _chacha_core(uint32_t *dst, const uint32_t *src, unsigned rounds)

PROLOGUE(_nettle_chacha_core)
	vldm	SRC, {X0,X1,X2,X3}

	vmov	S0, X0
	vmov	S1, X1
	vmov	S2, X2
	vmov	S3, X3

	C Input rows:
	C	 0  1  2  3	X0
	C	 4  5  6  7	X1
	C	 8  9 10 11	X2
	C	12 13 14 15	X3

.Loop:
	QROUND(X0, X1, X2, X3)

	C Rotate rows, to get
	C	 0  1  2  3
	C	 5  6  7  4  >>> 3
	C	10 11  8  9  >>> 2
	C	15 12 13 14  >>> 1
	vext.32	X1, X1, X1, #1
	vext.32	X2, X2, X2, #2
	vext.32	X3, X3, X3, #3

	QROUND(X0, X1, X2, X3)

	subs	ROUNDS, ROUNDS, #2
	C Inverse rotation
	vext.32	X1, X1, X1, #3
	vext.32	X2, X2, X2, #2
	vext.32	X3, X3, X3, #1

	bhi	.Loop

	vadd.u32	X0, X0, S0
	vadd.u32	X1, X1, S1
	vadd.u32	X2, X2, S2
	vadd.u32	X3, X3, S3

	vstm	DST, {X0,X1,X2,X3}
	bx	lr
EPILOGUE(_nettle_chacha_core)

divert(-1)
define chachastate
p/x $q0.u32
p/x $q1.u32
p/x $q2.u32
p/x $q3.u32
end
