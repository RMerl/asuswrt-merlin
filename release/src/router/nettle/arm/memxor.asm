C arm/memxor.asm

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

C Possible speedups:
C
C The ldm instruction can do load two registers per cycle,
C if the address is two-word aligned. Or three registers in two
C cycles, regardless of alignment.

C Register usage:

define(<DST>, <r0>)
define(<SRC>, <r1>)
define(<N>, <r2>)
define(<CNT>, <r6>)
define(<TNC>, <r12>)

	.syntax unified

	.file "memxor.asm"

	.text
	.arm

	C memxor(void *dst, const void *src, size_t n)
	.align 4
PROLOGUE(nettle_memxor)
	cmp	N, #0
	beq	.Lmemxor_done

	cmp	N, #7
	bcs	.Lmemxor_large

	C Simple byte loop
.Lmemxor_bytes:
	ldrb	r3, [SRC], #+1
	ldrb	r12, [DST]
	eor	r3, r12
	strb	r3, [DST], #+1
	subs	N, #1
	bne	.Lmemxor_bytes

.Lmemxor_done:
	bx	lr

.Lmemxor_align_loop:
	ldrb	r3, [SRC], #+1
	ldrb	r12, [DST]
	eor	r3, r12
	strb	r3, [DST], #+1
	sub	N, #1

.Lmemxor_large:
	tst	DST, #3
	bne	.Lmemxor_align_loop

	C We have at least 4 bytes left to do here.
	sub	N, #4

	ands	r3, SRC, #3
	beq	.Lmemxor_same

	C Different alignment case.
	C     v original SRC
	C +-------+------+
	C |SRC    |SRC+4 |
	C +---+---+------+
	C     |DST    |
	C     +-------+
	C
	C With little-endian, we need to do
	C DST[i] ^= (SRC[i] >> CNT) ^ (SRC[i+1] << TNC)

	push	{r4,r5,r6}
	
	lsl	CNT, r3, #3
	bic	SRC, #3
	rsb	TNC, CNT, #32

	ldr	r4, [SRC], #+4

	tst	N, #4
	itet	eq
	moveq	r5, r4
	subne	N, #4
	beq	.Lmemxor_odd

.Lmemxor_word_loop:
	ldr	r5, [SRC], #+4
	ldr	r3, [DST]
	eor	r3, r3, r4, lsr CNT
	eor	r3, r3, r5, lsl TNC
	str	r3, [DST], #+4
.Lmemxor_odd:
	ldr	r4, [SRC], #+4
	ldr	r3, [DST]
	eor	r3, r3, r5, lsr CNT
	eor	r3, r3, r4, lsl TNC
	str	r3, [DST], #+4
	subs	N, #8
	bcs	.Lmemxor_word_loop
	adds	N, #8
	beq	.Lmemxor_odd_done

	C We have TNC/8 left-over bytes in r4, high end
	lsr	r4, CNT
	ldr	r3, [DST]
	eor	r3, r4

	pop	{r4,r5,r6}

	C Store bytes, one by one.
.Lmemxor_leftover:
	strb	r3, [DST], #+1
	subs	N, #1
	beq	.Lmemxor_done
	subs	TNC, #8
	lsr	r3, #8
	bne	.Lmemxor_leftover
	b	.Lmemxor_bytes
.Lmemxor_odd_done:
	pop	{r4,r5,r6}
	bx	lr

.Lmemxor_same:
	push	{r4,r5,r6,r7,r8,r10,r11,r14}	C lr is the link register

	subs	N, #8
	bcc	.Lmemxor_same_end

	ldmia	SRC!, {r3, r4, r5}
	C Keep address for loads in r14
	mov	r14, DST
	ldmia	r14!, {r6, r7, r8}
	subs	N, #12
	eor	r10, r3, r6
	eor	r11, r4, r7
	eor	r12, r5, r8
	bcc	.Lmemxor_same_final_store
	subs	N, #12
	ldmia	r14!, {r6, r7, r8}
	bcc	.Lmemxor_same_wind_down

	C 6 cycles per iteration, 0.50 cycles/byte. For this speed,
	C loop starts at offset 0x11c in the object file.

.Lmemxor_same_loop:
	C r10-r12 contains values to be stored at DST
	C r6-r8 contains values read from r14, in advance
	ldmia	SRC!, {r3, r4, r5}
	subs	N, #12
	stmia	DST!, {r10, r11, r12}
	eor	r10, r3, r6
	eor	r11, r4, r7
	eor	r12, r5, r8
	ldmia	r14!, {r6, r7, r8}
	bcs	.Lmemxor_same_loop

.Lmemxor_same_wind_down:
	C Wind down code
	ldmia	SRC!, {r3, r4, r5}
	stmia	DST!, {r10, r11, r12}
	eor	r10, r3, r6
	eor	r11, r4, r7
	eor	r12, r5, r8
.Lmemxor_same_final_store:
	stmia	DST!, {r10, r11, r12}
	
.Lmemxor_same_end:
	C We have 0-11 bytes left to do, and N holds number of bytes -12.
	adds	N, #4
	bcc	.Lmemxor_same_lt_8
	C Do 8 bytes more, leftover is in N
	ldmia	SRC!, {r3, r4}
	ldmia	DST, {r6, r7}
	eor	r3, r6
	eor	r4, r7
	stmia	DST!, {r3, r4}
	pop	{r4,r5,r6,r7,r8,r10,r11,r14}
	beq	.Lmemxor_done
	b	.Lmemxor_bytes

.Lmemxor_same_lt_8:
	pop	{r4,r5,r6,r7,r8,r10,r11,r14}
	adds	N, #4
	bcc	.Lmemxor_same_lt_4

	ldr	r3, [SRC], #+4
	ldr	r12, [DST]
	eor	r3, r12
	str	r3, [DST], #+4
	beq	.Lmemxor_done
	b	.Lmemxor_bytes

.Lmemxor_same_lt_4:
	adds	N, #4
	beq	.Lmemxor_done
	b	.Lmemxor_bytes
	
EPILOGUE(nettle_memxor)
