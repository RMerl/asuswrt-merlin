C x86_64/memxor.asm

ifelse(<
   Copyright (C) 2010, 2014, Niels Möller

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

C Register usage:
define(<DST>, <%rax>) C Originally in %rdi
define(<SRC>, <%rsi>)
define(<N>, <%rdx>)
define(<TMP>, <%r8>)
define(<TMP2>, <%r9>)
define(<CNT>, <%rdi>)
define(<S0>, <%r11>)
define(<S1>, <%rdi>) C Overlaps with CNT 

define(<USE_SSE2>, <no>)

	.file "memxor.asm"

	.text

	C memxor(void *dst, const void *src, size_t n)
	C 	          %rdi               %rsi      %rdx
	ALIGN(16)

PROLOGUE(nettle_memxor)
	W64_ENTRY(3, 0)

	test	N, N
	C Get number of unaligned bytes at the end
	C %rdi is used as CNT, %rax as DST and as return value
	mov	%rdi, %rax
	jz	.Ldone
	add 	N, CNT
	and	$7, CNT
	
	jz	.Laligned

	cmp	$8, N
	jc	.Lfinal_next

	C FIXME: Instead of this loop, could try cmov with memory
	C destination, as a sequence of one 8-bit, one 16-bit and one
	C 32-bit operations. (Except that cmov can't do 8-bit ops, so
	C that step has to use a conditional).
.Lalign_loop:
	
	sub	$1, N
	movb	(SRC, N), LREG(TMP)
	xorb	LREG(TMP), (DST, N)
	sub	$1, CNT
	jnz	.Lalign_loop

.Laligned:
ifdef(<USE_SSE2>, <
	cmp	$16, N
	jnc	.Lsse2_case
>)

	C Next destination word is -8(DST, N)
	C Setup for unrolling
	test	$8, N
	jz	.Lword_next

	sub	$8, N
	jz	.Lone_word

	mov	(SRC, N), TMP
	xor	TMP, (DST, N)
	
	jmp	.Lword_next

	ALIGN(16)

.Lword_loop:
	mov	8(SRC, N), TMP
	mov	(SRC, N), TMP2
	xor	TMP, 8(DST, N)
	xor	TMP2, (DST, N)

.Lword_next:
	sub	$16, N
	ja	.Lword_loop	C Not zero and no carry
	jnz	.Lfinal

	C Final operation is word aligned
	mov	8(SRC, N), TMP
	xor	TMP, 8(DST, N)
	
.Lone_word:
	mov	(SRC, N), TMP
	xor	TMP, (DST, N)

	W64_EXIT(3, 0)
	ret

.Lfinal:
	add	$15, N

.Lfinal_loop:
	movb	(SRC, N), LREG(TMP)
	xorb	LREG(TMP), (DST, N)
.Lfinal_next:
	sub	$1, N
	jnc	.Lfinal_loop

.Ldone:
	W64_EXIT(3, 0)
	ret

ifdef(<USE_SSE2>, <

.Lsse2_case:
	lea	(DST, N), TMP
	test	$8, TMP
	jz	.Lsse2_next
	sub	$8, N
	mov	(SRC, N), TMP
	xor	TMP, (DST, N)
	jmp	.Lsse2_next

	ALIGN(16)
.Lsse2_loop:
	movdqu	(SRC, N), %xmm0
	movdqa	(DST, N), %xmm1
	pxor	%xmm0, %xmm1
	movdqa	%xmm1, (DST, N)
.Lsse2_next:
	sub	$16, N
	ja	.Lsse2_loop
	
	C FIXME: See if we can do a full word first, before the
	C byte-wise final loop.
	jnz	.Lfinal		

	C Final operation is aligned
	movdqu	(SRC), %xmm0
	movdqa	(DST), %xmm1
	pxor	%xmm0, %xmm1
	movdqa	%xmm1, (DST)

	W64_EXIT(3, 0)
	ret
>)	

EPILOGUE(nettle_memxor)
