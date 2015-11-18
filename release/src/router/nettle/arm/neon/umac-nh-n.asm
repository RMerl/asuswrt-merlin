C arm/neon/umac-nh-n.asm

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

	.file "umac-nh.asm"
	.fpu	neon

define(<OUT>, <r0>)
define(<ITERS>, <r1>)
define(<KEY>, <r2>)
define(<LENGTH>, <r3>)
define(<MSG>, <r12>)
define(<SHIFT>, <r14>)

define(<QA>, <q0>)
define(<QB>, <q1>)
define(<QY0>, <q3>)	C Accumulates for the first two operations.
define(<DM>, <d4>)
define(<QY1>, <q4>)	C Used for 3 and 4 iterations.
define(<QC>, <q5>)
define(<QD>, <q6>)
define(<QLEFT>, <q8>)
define(<QRIGHT>, <q9>)
define(<QT0>, <q10>)
define(<QT1>, <q11>)
define(<QT2>, <q12>)
define(<QK0>, <q13>)
define(<QK1>, <q14>)
define(<QK2>, <q15>)

C FIXME: Try permuting subkeys using vld4, vzip or similar.

	.text
	.align	3
	
PROLOGUE(_nettle_umac_nh_n)
	ldr	MSG, [sp]
	str	lr, [sp, #-4]!
	
	C Setup for 64-bit aligned reads
	ands	SHIFT, MSG, #7
	and	MSG, MSG, #-8
	vld1.8	{DM}, [MSG :64]
	addne	MSG, MSG, #8
	addeq	SHIFT, SHIFT, #8

	C FIXME: Combine as rsb ?
	lsl	SHIFT, SHIFT, #3
	neg	SHIFT, SHIFT

	C Right shift in QRIGHT (both halves)
	vmov.i32 D0REG(QRIGHT)[0], SHIFT
	vmov.32	 D1REG(QRIGHT), D0REG(QRIGHT)
	add	SHIFT, SHIFT, #64
	
	vmov.i32 D0REG(QLEFT)[0], SHIFT
	vmov.32	 D1REG(QLEFT), D0REG(QLEFT)
	cmp	r1, #3
	vmov.i64 QY0, #0

	vshl.u64 DM, DM, D0REG(QRIGHT)
	bcc	.Lnh2
	beq	.Lnh3
	
.Lnh4:	
	C Permute key words, so we in each iteration have them in order
	C
	C P0: [0, 4,1, 5] P1: [ 2, 6, 3, 7] P2: [ 4, 8, 5, 9] P3: [ 6,10, 7,11]
	C P4: [8,12,9,13] P5: [10,14,11,15] P6: [12,16,13,17] P7: [14,18,15,19]
	C
	C Also arrange the message words, so we get them as
	C M0: [0,0,1,1] M1: [ 2, 2, 3, 3] M2: [ 4, 4, 5, 5] M3: [ 6, 6, 7, 7]
	C M4: [8,8,9,9] M5: [10,10,11,11] M6: [12,12,13,13] M7: [14,14,15,15]
	C
	C Then, accumulate Y0 (first two "iters") using
	C
	C Y0 += (M0+P0) * (M2+P2) + (M1+P1) * (M3+P3) 
	C Y1 += (M0+P4) * (M2+P6) + (M1+P5) * (M3+P7)
	C
	C Next iteration is then
	C
	C Y0 += (M4+P4) * (M6+P6) + (M5+P5) * (M7 + P7) 
	C Y1 += (M4+P6) * (M6+P8) + (M5+P7) * (M7 + P11)
	C
	C So we can reuse P4, P5, P6, P7 from the previous iteration.

	C How to for in registers? We need 4 Q regs for P0-P3, and one
	C more for the last read key. We need at least two regiters
	C for the message (QA and QB, more if we want to expand only
	C once). For the Y0 update, we can let the factors overwrite
	C P0-P3, and for the Y1 update, we can overwrite M0-M3.
	
	vpush	{q4,q5,q6}
	vld1.32 {QK0,QK1}, [KEY]!
	vld1.32 {QK2}, [KEY]!
	vmov	QT0, QK1
	vmov	QT1, QK2
	
	C Permute keys. QK2 us untouched, permuted subkeys put in QK0,QK1,QT0,QT1
	vtrn.32	QK0, QK1		C Gives us [0, 4, 2, 6] and [1, 5, 3, 7]
	vswp D1REG(QK0), D0REG(QK1)	C Gives us [0, 4, 1, 5] and [2, 6, 3, 7]
	vtrn.32	QT0, QT1		C Gives us [4,8,6,10] and [5 ,9,7,11]
	vswp D1REG(QT0), D0REG(QT1)	C Gives us [4,8,5, 9] and [6,10,7,11]

	vmov.i64 QY1, #0
.Loop4:
	C Set m[i] <-- m[i-1] >> RSHIFT + m[i] << LSHIFT
	vld1.8 {QA, QB}, [MSG :64]!
	vshl.u64 QC, QA, QRIGHT
	vshl.u64 QD, QB, QRIGHT
	vshl.u64 QA, QA, QLEFT
	vshl.u64 QB, QB, QLEFT
	veor	D0REG(QA), D0REG(QA), DM
	veor	D1REG(QA), D1REG(QA), D0REG(QC)
	veor	D0REG(QB), D0REG(QB), D1REG(QC)
	veor	D1REG(QB), D1REG(QB), D0REG(QD)
	vmov	DM, D1REG(QD)

	C Explode message (too bad there's no vadd with scalar)
	vdup.32	D1REG(QD), D1REG(QB)[1]
	vdup.32	D0REG(QD), D1REG(QB)[0]
	vdup.32	D1REG(QC), D0REG(QB)[1]
	vdup.32	D0REG(QC), D0REG(QB)[0]
	vdup.32	D1REG(QB), D1REG(QA)[1]
	vdup.32	D0REG(QB), D1REG(QA)[0]
	vdup.32	D1REG(QA), D0REG(QA)[1]
	vdup.32	D0REG(QA), D0REG(QA)[0]

	vadd.i32 QK0, QK0, QA
	vadd.i32 QK1, QK1, QB
	vadd.i32 QT0, QT0, QC
	vadd.i32 QT1, QT1, QD

	vmlal.u32 QY0, D0REG(QK0), D0REG(QT0)
	vmlal.u32 QY0, D1REG(QK0), D1REG(QT0)
	vmlal.u32 QY0, D0REG(QK1), D0REG(QT1)
	vmlal.u32 QY0, D1REG(QK1), D1REG(QT1)
	
	C Next 4 subkeys
	vld1.32	{QT0,QT1}, [KEY]!
	vmov	QK0, QK2
	vmov	QK1, QT0
	vmov	QK2, QT1		C Save
	vtrn.32	QK0, QK1		C Gives us [8,12,10,14] and [9,13,11,15]
	vswp D1REG(QK0), D0REG(QK1)	C Gives us [8,12,9,13] and [10,14,11,15]
	vtrn.32	QT0, QT1		C Gives us [12,16,14,18] and [13,17,15,19]
	vswp D1REG(QT0), D0REG(QT1)	C Gives us [12,16,13,17] and [14,18,15,19]

	vadd.i32 QA, QA, QK0
	vadd.i32 QB, QB, QK1
	vadd.i32 QC, QC, QT0
	vadd.i32 QD, QD, QT1

	subs	LENGTH, LENGTH, #32

	vmlal.u32 QY1, D0REG(QA), D0REG(QC)
	vmlal.u32 QY1, D1REG(QA), D1REG(QC)
	vmlal.u32 QY1, D0REG(QB), D0REG(QD)
	vmlal.u32 QY1, D1REG(QB), D1REG(QD)

	bhi	.Loop4

	vst1.64	{QY0, QY1}, [OUT]
	
	vpop	{q4,q5,q6}
	
	ldr	pc, [sp], #+4

.Lnh3:
	vpush	{q4}
	vld1.32 {QK0,QK1}, [KEY]!
	vmov.i64 QY1, #0
.Loop3:
	C Set m[i] <-- m[i-1] >> RSHIFT + m[i] << LSHIFT
	vld1.8 {QA, QB}, [MSG :64]!
	vshl.u64 QT0, QA, QRIGHT
	vshl.u64 QT1, QB, QRIGHT
	vshl.u64 QA, QA, QLEFT
	vshl.u64 QB, QB, QLEFT
	veor	D0REG(QA), D0REG(QA), DM
	veor	D1REG(QA), D1REG(QA), D0REG(QT0)
	veor	D0REG(QB), D0REG(QB), D1REG(QT0)
	veor	D1REG(QB), D1REG(QB), D0REG(QT1)
	vmov	DM, D1REG(QT1)
	
	vld1.32	{QK2}, [KEY]!
	C Construct factors, with low half corresponding to first iteration,
	C and high half corresponding to the second iteration.
	vmov	QT0, QK1
	vtrn.32	QK0, QT0		C Gives us [0, 4, 2, 6] and [1, 5, 3, 7]
	vswp D1REG(QK0), D0REG(QT0)	C Gives us [0, 4, 1, 5] and [2, 6, 3, 7]
	vdup.32	D0REG(QT1), D0REG(QA)[0]
	vdup.32	D1REG(QT1), D0REG(QA)[1]
	vadd.i32	QT1, QT1, QK0

	vmov	QK0, QK2		C Save for next iteration
	vtrn.32	QK1, QK2		C Gives us [4, 8, 2, 1] and [1, 5, 3, 7]
	vswp	D1REG(QK1), D0REG(QK2)	C Gives us [4, 8, 1, 5] and [2, 1, 3, 7]
	
	vdup.32	D0REG(QT2), D0REG(QB)[0]
	vdup.32	D1REG(QT2), D0REG(QB)[1]
	vadd.i32 QK1, QK1, QT2
	vmlal.u32 QY0, D0REG(QT1), D0REG(QK1)
	vmlal.u32 QY0, D1REG(QT1), D1REG(QK1)

	vdup.32	D0REG(QT1), D1REG(QA)[0]
	vdup.32	D1REG(QT1), D1REG(QA)[1]
	vadd.i32	QT0, QT0, QT1
	vdup.32	D0REG(QT1), D1REG(QB)[0]
	vdup.32	D1REG(QT1), D1REG(QB)[1]
	vadd.i32	QK2, QK2, QT1

	vmlal.u32 QY0, D0REG(QT0), D0REG(QK2)
	vmlal.u32 QY0, D1REG(QT0), D1REG(QK2)

	vld1.32	{QK1}, [KEY]!
	vadd.i32 QA, QA, QK0
	vadd.i32 QB, QB, QK1
	subs	LENGTH, LENGTH, #32
	vmlal.u32 QY1, D0REG(QA), D0REG(QB)
	vmlal.u32 QY1, D1REG(QA), D1REG(QB)
	bhi	.Loop3

	vadd.i64 D0REG(QY1), D0REG(QY1), D1REG(QY1)
	vst1.64	{D0REG(QY0), D1REG(QY0), D0REG(QY1)}, [OUT]
	
	vpop	{q4}
	
	ldr	pc, [sp], #+4
	
.Lnh2:
	vld1.32 {QK0}, [KEY]!
.Loop2:
	C Set m[i] <-- m[i-1] >> RSHIFT + m[i] << LSHIFT
	vld1.8 {QA, QB}, [MSG :64]!
	vshl.u64 QT0, QA, QRIGHT
	vshl.u64 QT1, QB, QRIGHT
	vshl.u64 QA, QA, QLEFT
	vshl.u64 QB, QB, QLEFT
	veor	D0REG(QA), D0REG(QA), DM
	veor	D1REG(QA), D1REG(QA), D0REG(QT0)
	veor	D0REG(QB), D0REG(QB), D1REG(QT0)
	veor	D1REG(QB), D1REG(QB), D0REG(QT1)
	vmov	DM, D1REG(QT1)
	
	vld1.32	{QK1,QK2}, [KEY]!
	C Construct factors, with low half corresponding to first iteration,
	C and high half corresponding to the second iteration.
	vmov	QT0, QK1
	vtrn.32	QK0, QT0		C Gives us [0, 4, 2, 6] and [1, 5, 3, 7]
	vswp D1REG(QK0), D0REG(QT0)	C Gives us [0, 4, 1, 5] and [2, 6, 3, 7]
	vdup.32	D0REG(QT1), D0REG(QA)[0]
	vdup.32	D1REG(QT1), D0REG(QA)[1]
	vadd.i32	QT1, QT1, QK0

	vmov	QK0, QK2		C Save for next iteration
	vtrn.32	QK1, QK2		C Gives us [4, 8, 6, 10] and [5,  9, 7, 11]
	vswp	D1REG(QK1), D0REG(QK2)	C Gives us [4, 8, 5,  9] and [6, 10, 7, 11]
	
	vdup.32	D0REG(QT2), D0REG(QB)[0]
	vdup.32	D1REG(QT2), D0REG(QB)[1]
	vadd.i32 QK1, QK1, QT2
	vmlal.u32 QY0, D0REG(QT1), D0REG(QK1)
	vmlal.u32 QY0, D1REG(QT1), D1REG(QK1)

	vdup.32	D0REG(QT1), D1REG(QA)[0]
	vdup.32	D1REG(QT1), D1REG(QA)[1]
	vadd.i32	QT0, QT0, QT1
	vdup.32	D0REG(QT1), D1REG(QB)[0]
	vdup.32	D1REG(QT1), D1REG(QB)[1]
	vadd.i32	QK2, QK2, QT1

	subs	LENGTH, LENGTH, #32
	
	vmlal.u32 QY0, D0REG(QT0), D0REG(QK2)
	vmlal.u32 QY0, D1REG(QT0), D1REG(QK2)
	
	bhi	.Loop2
	vst1.64	{QY0}, [OUT]

.Lend:
	ldr	pc, [sp], #+4
EPILOGUE(_nettle_umac_nh_n)
