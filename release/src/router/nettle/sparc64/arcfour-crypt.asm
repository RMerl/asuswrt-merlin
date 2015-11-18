C sparc64/arcfour-crypt.asm

ifelse(<
   Copyright (C) 2002, 2005 Niels Möller

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

C	Define to YES, to enable the complex code to special case SRC
C	and DST with compatible alignment.
	
define(<WITH_ALIGN>, <YES>)

C	Registers

define(<CTX>,	<%i0>)
define(<LENGTH>,<%i1>)
define(<DST>,	<%i2>)
define(<SRC>,	<%i3>)

define(<I1>,	<%i4>)
define(<I2>,	<%i5>)
define(<J>,	<%g1>)
define(<SI>,	<%g2>)
define(<SJ>,	<%g3>)
define(<TMP>,	<%o0>)
define(<TMP2>,	<%o1>)
define(<N>,	<%o2>)
define(<DATA>,	<%o3>)

C	Computes the next byte of the key stream. As input, i must
C	already point to the index for the current access, the index
C	for the next access is stored in ni. The resulting key byte is
C	stored in res.
C	ARCFOUR_BYTE(i, ni, res)
define(<ARCFOUR_BYTE>, <
	ldub	[CTX + $1], SI
	add	$1, 1, $2
	add	J, SI, J
	and	J, 0xff, J
	ldub	[CTX + J], SJ
	and	$2, 0xff, $2
	stb	SI, [CTX + J]
	add	SI, SJ, SI
	and	SI, 0xff, SI
	stb	SJ, [CTX + $1]
	ldub	[CTX + SI], $3
>)dnl
			
define(<FRAME_SIZE>, 192)

	.file "arcfour-crypt.asm"

	C arcfour_crypt(struct arcfour_ctx *ctx,
	C               size_t length, uint8_t *dst,
	C               const uint8_t *src)

	.section	".text"
	.align 16
	.proc	020
	
PROLOGUE(nettle_arcfour_crypt)

	save	%sp, -FRAME_SIZE, %sp
	cmp	LENGTH, 0
	be	.Lend
	nop
	
	C	Load both I and J
	lduh	[CTX + ARCFOUR_I], I1
	and	I1, 0xff, J
	srl	I1, 8, I1

	C	We want an even address for DST
	andcc	DST, 1, %g0
	add	I1, 1 ,I1
	beq	.Laligned2
	and	I1, 0xff, I1

	mov	I1, I2
	ldub	[SRC], DATA
	ARCFOUR_BYTE(I2, I1, TMP)
	subcc	LENGTH, 1, LENGTH
	add	SRC, 1, SRC
	xor	DATA, TMP, DATA
	stb	DATA, [DST]
	beq	.Ldone
	add	DST, 1, DST

.Laligned2:

	cmp	LENGTH, 2
	blu	.Lfinal1
	C	Harmless delay slot instruction	
	andcc	DST, 2, %g0
	beq	.Laligned4
	nop

	ldub	[SRC], DATA
	ARCFOUR_BYTE(I1, I2, TMP)
	ldub	[SRC + 1], TMP2
	add	SRC, 2, SRC
	xor	DATA, TMP, DATA
	sll	DATA, 8, DATA	

	ARCFOUR_BYTE(I2, I1, TMP)
	xor	TMP2, TMP, TMP
	subcc	LENGTH, 2, LENGTH
	or	DATA, TMP, DATA

	sth	DATA, [DST]
	beq	.Ldone
	add	DST, 2, DST
	
.Laligned4:
	cmp	LENGTH, 4
	blu	.Lfinal2
	C	Harmless delay slot instruction
	srl	LENGTH, 2, N
	
.Loop:
	C	Main loop, with aligned writes
	
	C	FIXME: Could check if SRC is aligned, and
	C	use 32-bit reads in that case.

	ldub	[SRC], DATA
	ARCFOUR_BYTE(I1, I2, TMP)
	ldub	[SRC + 1], TMP2
	xor	TMP, DATA, DATA
	sll	DATA, 8, DATA

	ARCFOUR_BYTE(I2, I1, TMP)
	xor	TMP2, TMP, TMP
	ldub	[SRC + 2], TMP2
	or	TMP, DATA, DATA
	sll	DATA, 8, DATA

	ARCFOUR_BYTE(I1, I2, TMP)
	xor	TMP2, TMP, TMP
	ldub	[SRC + 3], TMP2
	or	TMP, DATA, DATA
	sll	DATA, 8, DATA

	ARCFOUR_BYTE(I2, I1, TMP)
	xor	TMP2, TMP, TMP
	or	TMP, DATA, DATA
	subcc	N, 1, N
	add	SRC, 4, SRC
	st	DATA, [DST]
	bne	.Loop
	add	DST, 4, DST
	
	andcc	LENGTH, 3, LENGTH
	beq	.Ldone
	nop

.Lfinal2:
	C	DST address must be 2-aligned
	cmp	LENGTH, 2
	blu	.Lfinal1
	nop

	ldub	[SRC], DATA
	ARCFOUR_BYTE(I1, I2, TMP)
	ldub	[SRC + 1], TMP2
	add	SRC, 2, SRC
	xor	DATA, TMP, DATA
	sll	DATA, 8, DATA	

	ARCFOUR_BYTE(I2, I1, TMP)
	xor	TMP2, TMP, TMP
	or	DATA, TMP, DATA

	sth	DATA, [DST]
	beq	.Ldone
	add	DST, 2, DST

.Lfinal1:
	mov	I1, I2
	ldub	[SRC], DATA
	ARCFOUR_BYTE(I2, I1, TMP)
	xor	DATA, TMP, DATA
	stb	DATA, [DST]

.Ldone:
	C	Save back I and J
	sll	I2, 8, I2
	or	I2, J, I2
	stuh	I2, [CTX + ARCFOUR_I]

.Lend:
	ret
	restore

EPILOGUE(nettle_arcfour_crypt)

C	Stats for AES 128 on sellafield.lysator.liu.se (UE450, 296 MHz)

C 1:	nettle-1.13 C-code
C 2:	New assembler code (basically the same as for sparc32)

C	MB/s	cycles/byte
C 1:	3.6	77.7
C 2:	21.8	13.0
