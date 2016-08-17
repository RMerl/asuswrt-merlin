C arm/v6/aes-encrypt-internal.asm

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

	.arch armv6

include_src(<arm/aes.m4>)

C	Benchmarked at at 706, 870, 963 cycles/block on cortex A9,
C	for 128, 192 and 256 bit key sizes.

C	Possible improvements: More efficient load and store with
C	aligned accesses. Better scheduling.

define(<PARAM_ROUNDS>, <r0>)
define(<PARAM_KEYS>, <r1>)
define(<TABLE>, <r2>)
define(<LENGTH>, <r3>)
C On stack: DST, SRC

define(<W0>, <r4>)
define(<W1>, <r5>)
define(<W2>, <r6>)
define(<W3>, <r7>)
define(<T0>, <r8>)
define(<COUNT>, <r10>)
define(<KEY>, <r11>)

define(<X0>, <r0>)	C Overlaps PARAM_ROUNDS and PARAM_KEYS
define(<X1>, <r1>)
define(<X2>, <r12>)
define(<X3>, <r14>)	C lr

define(<FRAME_ROUNDS>>,  <[sp]>)
define(<FRAME_KEYS>,  <[sp, #+4]>)
C 8 saved registers
define(<FRAME_DST>,  <[sp, #+40]>)
define(<FRAME_SRC>,  <[sp, #+44]>)

define(<SRC>, <r12>)	C Overlap registers used in inner loop.
define(<DST>, <COUNT>)

C 53 instr.
C It's tempting to use eor with rotation, but that's slower.
C AES_ENCRYPT_ROUND(x0,x1,x2,x3,w0,w1,w2,w3,key)
define(<AES_ENCRYPT_ROUND>, <
	uxtb	T0, $1 
	ldr	$5, [TABLE, T0, lsl #2]
	uxtb	T0, $2
	ldr	$6, [TABLE, T0, lsl #2]
	uxtb	T0, $3
	ldr	$7, [TABLE, T0, lsl #2]
	uxtb	T0, $4
	ldr	$8, [TABLE, T0, lsl #2]

	uxtb	T0, $2, ror #8
	add	TABLE, TABLE, #1024
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$5, $5, T0
	uxtb	T0, $3, ror #8
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$6, $6, T0
	uxtb	T0, $4, ror #8
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$7, $7, T0
	uxtb	T0, $1, ror #8
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$8, $8, T0

	uxtb	T0, $3, ror #16
	add	TABLE, TABLE, #1024
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$5, $5, T0
	uxtb	T0, $4, ror #16
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$6, $6, T0
	uxtb	T0, $1, ror #16
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$7, $7, T0
	uxtb	T0, $2, ror #16
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$8, $8, T0

	uxtb	T0, $4, ror #24
	add	TABLE, TABLE, #1024
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$5, $5, T0
	uxtb	T0, $1, ror #24
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$6, $6, T0
	uxtb	T0, $2, ror #24
	ldr	T0, [TABLE, T0, lsl #2]
	eor	$7, $7, T0
	uxtb	T0, $3, ror #24
	ldr	T0, [TABLE, T0, lsl #2]

	ldm	$9!, {$1,$2,$3,$4}
	eor	$8, $8, T0
	sub	TABLE, TABLE, #3072
	eor	$5, $5, $1
	eor	$6, $6, $2
	eor	$7, $7, $3
	eor	$8, $8, $4
>)

	.file "aes-encrypt-internal.asm"
	
	C _aes_encrypt(unsigned rounds, const uint32_t *keys,
	C	       const struct aes_table *T,
	C	       size_t length, uint8_t *dst,
	C	       uint8_t *src)
	.text
	ALIGN(4)
PROLOGUE(_nettle_aes_encrypt)
	teq	LENGTH, #0
	beq	.Lend

	ldr	SRC, [sp, #+4]

	push	{r0,r1, r4,r5,r6,r7,r8,r10,r11,lr}

	ALIGN(16)
.Lblock_loop:
	ldm	sp, {COUNT, KEY}

	add	TABLE, TABLE, #AES_TABLE0

	AES_LOAD(SRC,KEY,W0)
	AES_LOAD(SRC,KEY,W1)
	AES_LOAD(SRC,KEY,W2)
	AES_LOAD(SRC,KEY,W3)

	str	SRC, FRAME_SRC

	b	.Lentry
	ALIGN(16)
.Lround_loop:
	C	Transform X -> W
	AES_ENCRYPT_ROUND(X0, X1, X2, X3, W0, W1, W2, W3, KEY)
	
.Lentry:
	subs	COUNT, COUNT,#2
	C	Transform W -> X
	AES_ENCRYPT_ROUND(W0, W1, W2, W3, X0, X1, X2, X3, KEY)

	bne	.Lround_loop

	sub	TABLE, TABLE, #AES_TABLE0

	C	Final round
	ldr	DST, FRAME_DST

	AES_FINAL_ROUND_V6(X0, X1, X2, X3, KEY, W0)
	AES_FINAL_ROUND_V6(X1, X2, X3, X0, KEY, W1)
	AES_FINAL_ROUND_V6(X2, X3, X0, X1, KEY, W2)
	AES_FINAL_ROUND_V6(X3, X0, X1, X2, KEY, W3)

	ldr	SRC, FRAME_SRC
	
	AES_STORE(DST,W0)
	AES_STORE(DST,W1)
	AES_STORE(DST,W2)
	AES_STORE(DST,W3)

	str	DST, FRAME_DST
	subs	LENGTH, LENGTH, #16
	bhi	.Lblock_loop

	add	sp, sp, #8	C Drop saved r0, r1
	pop	{r4,r5,r6,r7,r8,r10,r11,pc}
	
.Lend:
	bx	lr
EPILOGUE(_nettle_aes_encrypt)
