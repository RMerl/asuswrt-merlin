C arm/neon/sha512-compress.asm

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

	.file "sha512-compress.asm"
	.fpu	neon

define(<STATE>, <r0>)
define(<INPUT>, <r1>)
define(<K>, <r2>)
define(<COUNT>, <r3>)
define(<SHIFT>, <r12>)

define(<SA>, <d0>)
define(<SB>, <d1>)
define(<SC>, <d2>)
define(<SD>, <d3>)
define(<SE>, <d4>)
define(<SF>, <d5>)
define(<SG>, <d6>)
define(<SH>, <d7>)
define(<QSAB>, <q0>)
define(<QSCD>, <q1>)
define(<QSEF>, <q2>)
define(<QSGH>, <q3>)

C d8-d15 are callee-save	
define(<DT0>, <d8>)
define(<DT1>, <d9>)
define(<QT01>, <q4>)
define(<DT2>, <d10>)
define(<DT3>, <d11>)
define(<QT23>, <q5>)
define(<DT4>, <d12>)
define(<DT5>, <d13>)
define(<QT45>, <q6>)

C Used only when reading the input, can overlap with state
define(<DT6>, <d0>)
define(<DT7>, <d1>)
define(<QT67>, <q0>)

define(<DW0>, <d16>)
define(<DW1>, <d17>)
define(<DW2>, <d18>)
define(<DW3>, <d19>)
define(<DW4>, <d20>)
define(<DW5>, <d21>)
define(<DW6>, <d22>)
define(<DW7>, <d23>)
define(<DW8>, <d24>)
define(<DW9>, <d25>)
define(<DW10>, <d26>)
define(<DW11>, <d27>)
define(<DW12>, <d28>)
define(<DW13>, <d29>)
define(<DW14>, <d30>)
define(<DW15>, <d31>)
define(<QW0001>, <q8>)
define(<QW0203>, <q9>)
define(<QW0405>, <q10>)
define(<QW0607>, <q11>)
define(<QW0809>, <q12>)
define(<QW1011>, <q13>)
define(<QW1213>, <q14>)
define(<QW1415>, <q15>)

define(<EXPAND_ME>, <$1>)
define(<W>, <EXPAND_ME(<DW>eval(($1) % 16))>)

C If x = W(i+14), y = w(i+1), we xor in parallel
C
C	x << 45		y << 63
C	x >> 19		y >> 1
C	x << 3		y << 56
C	x >> 61		y >> 8
C  xor	x >> 6		y >> 7
C  -----------------------------
C	DT0		DT1
define(<EXPN>, <
	vshl.i64	DT0, W($1+14), #45
	vshl.i64	DT1, W($1 + 1), #63
	vshr.u64	DT2, W($1+14), #19
	vshr.u64	DT3, W($1 + 1), #1
	vshl.i64	DT4, W($1+14), #3
	vshl.i64	DT5, W($1 + 1), #56
	veor.i64	QT01, QT01, QT23
	vshr.u64	DT2, W($1+14), #61
	vshr.u64	DT3, W($1 + 1), #8
	veor.i64	QT01, QT01, QT45
	vshr.u64	DT4, W($1+14), #6
	vshr.u64	DT5, W($1 + 1), #7
	veor.i64	QT01, QT01, QT23
	vadd.i64	W($1), W($1), W($1 + 9)
	veor.i64	QT01, QT01, QT45
	vadd.i64	W($1), W($1), DT0
	vadd.i64	W($1), W($1), DT1
>)

C ROUND(A,B,C,D,E,F,G,H,i)
C
C H += S1(E) + Choice(E,F,G) + K + W
C D += H
C H += S0(A) + Majority(A,B,C)
C
C Where
C
C S1(E) = E<<<50 ^ E<<<46 ^ E<<<23
C S0(A) = A<<<36 ^ A<<<30 ^ A<<<25
C Choice (E, F, G) = G^(E&(F^G))
C Majority (A,B,C) = (A&B) + (C&(A^B))

C Do S1 and S0 in parallel
C
C	e << 50		a << 36
C	e >> 14		a >> 28
C	e << 46		a << 30
C	e >> 18		a >> 34
C	e << 23		a << 25
C  xor	e >> 41		a >> 39
C  ----------------------------
C	DT0		DT1
define(<ROUND>, <
	vshl.i64	DT0, $5, #50
	vshl.i64	DT1, $1, #36
	vshr.u64	DT2, $5, #14
	vshr.u64	DT3, $1, #28
	vshl.i64	DT4, $5, #46
	vshl.i64	DT5, $1, #30
	veor		QT01, QT01, QT23
	vshr.u64	DT2, $5, #18
	vshr.u64	DT3, $1, #34
	veor		QT01, QT01, QT45
	vshl.i64	DT4, $5, #23
	vshl.i64	DT5, $1, #25
	veor		QT01, QT01, QT23
	vshr.u64	DT2, $5, #41
	vshr.u64	DT3, $1, #39
	veor		QT01, QT01, QT45
	veor		DT4, $6, $7
	veor		DT5, $1, $2
	vand		DT4, DT4, $5
	vand		DT5, DT5, $3
	veor		DT4, DT4, $7
	veor		QT01, QT01, QT23
	vand		DT2, $1, $2
	vldr		DT3, [K,#eval(8*$9)]
	vadd.i64	$8, $8, W($9)
	vadd.i64	QT01, QT01, QT45
	vadd.i64	$8, $8, DT3
	vadd.i64	$8, $8, DT0
	vadd.i64	DT1, DT1, DT2
	vadd.i64	$4, $4, $8
	vadd.i64	$8, $8, DT1
>)

	C void
	C _nettle_sha512_compress(uint64_t *state, const uint8_t *input, const uint64_t *k)

	.text
	.align 2

PROLOGUE(_nettle_sha512_compress)
	vpush	{d8,d9,d10,d11,d12,d13}
	
	ands	SHIFT, INPUT, #7
	and	INPUT, INPUT, #-8
	vld1.8	{DT5}, [INPUT :64]
	addne	INPUT, INPUT, #8
	addeq	SHIFT, SHIFT, #8
	lsl	SHIFT, SHIFT, #3

	C Put right shift in DT0 and DT1, aka QT01
	neg	SHIFT, SHIFT
	vmov.i32	DT0, #0
	vmov.32		DT0[0], SHIFT
	vmov		DT1, DT0
	C Put left shift in DT2 and DT3, aka QT23
	add		SHIFT, SHIFT, #64
	vmov.i32	DT2, #0
	vmov.32		DT2[0], SHIFT
	vmov		DT3, DT2
	vshl.u64	DT5, DT5, DT0

	C Set w[i] <-- w[i-1] >> RSHIFT + w[i] << LSHIFT
	vld1.8		{W(0),W(1),W(2),W(3)}, [INPUT :64]!
	vshl.u64	QT67, QW0001, QT01	C Right shift
	vshl.u64	QW0001, QW0001, QT23	C Left shift
	veor		W(0), W(0), DT5
	veor		W(1), W(1), DT6
	vrev64.8	QW0001, QW0001
	vshl.u64	QT45, QW0203, QT01	C Right shift
	vshl.u64	QW0203, QW0203, QT23	C Left shift
	veor		W(2), W(2), DT7
	veor		W(3), W(3), DT4
	vrev64.8	QW0203, QW0203

	vld1.8		{W(4),W(5),W(6),W(7)}, [INPUT :64]!
	vshl.u64	QT67, QW0405, QT01	C Right shift
	vshl.u64	QW0405, QW0405, QT23	C Left shift
	veor		W(4), W(4), DT5
	veor		W(5), W(5), DT6
	vrev64.8	QW0405, QW0405
	vshl.u64	QT45, QW0607, QT01	C Right shift
	vshl.u64	QW0607, QW0607, QT23	C Left shift
	veor		W(6), W(6), DT7
	veor		W(7), W(7), DT4
	vrev64.8	QW0607, QW0607

	vld1.8		{W(8),W(9),W(10),W(11)}, [INPUT :64]!
	vshl.u64	QT67, QW0809, QT01	C Right shift
	vshl.u64	QW0809, QW0809, QT23	C Left shift
	veor		W(8), W(8), DT5
	veor		W(9), W(9), DT6
	vrev64.8	QW0809, QW0809
	vshl.u64	QT45, QW1011, QT01	C Right shift
	vshl.u64	QW1011, QW1011, QT23	C Left shift
	veor		W(10), W(10), DT7
	veor		W(11), W(11), DT4
	vrev64.8	QW1011, QW1011

	vld1.8		{W(12),W(13),W(14),W(15)}, [INPUT :64]!
	vshl.u64	QT67, QW1213, QT01	C Right shift
	vshl.u64	QW1213, QW1213, QT23	C Left shift
	veor		W(12), W(12), DT5
	veor		W(13), W(13), DT6
	vrev64.8	QW1213, QW1213
	vshl.u64	QT45, QW1415, QT01	C Right shift
	vshl.u64	QW1415, QW1415, QT23	C Left shift
	veor		W(14), W(14), DT7
	veor		W(15), W(15), DT4
	vrev64.8	QW1415, QW1415

	vldm	STATE, {SA,SB,SC,SD,SE,SF,SG,SH}

	ROUND(SA,SB,SC,SD,SE,SF,SG,SH, 0)
	ROUND(SH,SA,SB,SC,SD,SE,SF,SG, 1)
	ROUND(SG,SH,SA,SB,SC,SD,SE,SF, 2)
	ROUND(SF,SG,SH,SA,SB,SC,SD,SE, 3)
	ROUND(SE,SF,SG,SH,SA,SB,SC,SD, 4)
	ROUND(SD,SE,SF,SG,SH,SA,SB,SC, 5)
	ROUND(SC,SD,SE,SF,SG,SH,SA,SB, 6)
	ROUND(SB,SC,SD,SE,SF,SG,SH,SA, 7)

	ROUND(SA,SB,SC,SD,SE,SF,SG,SH, 8)
	ROUND(SH,SA,SB,SC,SD,SE,SF,SG, 9)
	ROUND(SG,SH,SA,SB,SC,SD,SE,SF, 10)
	ROUND(SF,SG,SH,SA,SB,SC,SD,SE, 11)
	ROUND(SE,SF,SG,SH,SA,SB,SC,SD, 12)
	ROUND(SD,SE,SF,SG,SH,SA,SB,SC, 13)
	ROUND(SC,SD,SE,SF,SG,SH,SA,SB, 14)
	ROUND(SB,SC,SD,SE,SF,SG,SH,SA, 15)

	add	K, K, #128

	mov	COUNT, #4
.Loop:

	EXPN( 0) ROUND(SA,SB,SC,SD,SE,SF,SG,SH,  0)
	EXPN( 1) ROUND(SH,SA,SB,SC,SD,SE,SF,SG,  1)
	EXPN( 2) ROUND(SG,SH,SA,SB,SC,SD,SE,SF,  2)
	EXPN( 3) ROUND(SF,SG,SH,SA,SB,SC,SD,SE,  3)
	EXPN( 4) ROUND(SE,SF,SG,SH,SA,SB,SC,SD,  4)
	EXPN( 5) ROUND(SD,SE,SF,SG,SH,SA,SB,SC,  5)
	EXPN( 6) ROUND(SC,SD,SE,SF,SG,SH,SA,SB,  6)
	EXPN( 7) ROUND(SB,SC,SD,SE,SF,SG,SH,SA,  7)
	EXPN( 8) ROUND(SA,SB,SC,SD,SE,SF,SG,SH,  8)
	EXPN( 9) ROUND(SH,SA,SB,SC,SD,SE,SF,SG,  9)
	EXPN(10) ROUND(SG,SH,SA,SB,SC,SD,SE,SF, 10)
	EXPN(11) ROUND(SF,SG,SH,SA,SB,SC,SD,SE, 11)
	EXPN(12) ROUND(SE,SF,SG,SH,SA,SB,SC,SD, 12)
	EXPN(13) ROUND(SD,SE,SF,SG,SH,SA,SB,SC, 13)
	EXPN(14) ROUND(SC,SD,SE,SF,SG,SH,SA,SB, 14)
	subs	COUNT, COUNT, #1
	EXPN(15) ROUND(SB,SC,SD,SE,SF,SG,SH,SA, 15)
	add	K, K, #128
	bne	.Loop

	vld1.64		{DW0, DW1, DW2, DW3}, [STATE]
	vadd.i64	QSAB, QSAB, QW0001
	vadd.i64	QSCD, QSCD, QW0203
	vst1.64		{SA,SB,SC,SD}, [STATE]!
	vld1.64		{DW0, DW1, DW2, DW3}, [STATE]
	vadd.i64	QSEF, QSEF, QW0001
	vadd.i64	QSGH, QSGH, QW0203
	vst1.64		{SE,SF,SG,SH}, [STATE]!

	vpop	{d8,d9,d10,d11,d12,d13}
	bx	lr
EPILOGUE(_nettle_sha512_compress)

divert(-1)
define shastate
p/x $d0.u64
p/x $d1.u64
p/x $d2.u64
p/x $d3.u64
p/x $d4.u64
p/x $d5.u64
p/x $d6.u64
p/x $d7.u64
end
