C x86_64/memxor3.asm

ifelse(<
   Copyright (C) 2010, 2014 Niels Möller

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
define(<AP>, <%rsi>)
define(<BP>, <%rdx>)
define(<N>, <%r10>)
define(<TMP>, <%r8>)
define(<TMP2>, <%r9>)
define(<CNT>, <%rdi>)
define(<S0>, <%r11>)
define(<S1>, <%rdi>) C Overlaps with CNT 

define(<USE_SSE2>, <no>)

	.file "memxor3.asm"

	.text

	C memxor3(void *dst, const void *a, const void *b, size_t n)
	C 	          %rdi              %rsi              %rdx      %rcx
	ALIGN(16)
	
PROLOGUE(nettle_memxor3)
	W64_ENTRY(4, 0)
	C %cl needed for shift count, so move away N
	mov	%rcx, N
.Lmemxor3_entry:
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
	movb	(AP, N), LREG(TMP)
	xorb	(BP, N), LREG(TMP)
	movb	LREG(TMP), (DST, N)
	sub	$1, CNT
	jnz	.Lalign_loop

.Laligned:
ifelse(USE_SSE2, yes, <
	cmp	$16, N
	jnc	.Lsse2_case
>)
	C Check for the case that AP and BP have the same alignment,
	C but different from DST.
	mov	AP, TMP
	sub	BP, TMP
	test	$7, TMP
	jnz	.Lno_shift_case
	mov	AP, %rcx
	sub	DST, %rcx
	and	$7, %rcx
	jz	.Lno_shift_case
	sub	%rcx, AP
	sub	%rcx, BP
	shl	$3, %rcx

	C Unrolling, with aligned values alternating in S0 and S1
	test	$8, N
	jnz	.Lshift_odd
	mov	(AP, N), S1
	xor	(BP, N), S1
	jmp	.Lshift_next

.Lshift_odd:
	mov	-8(AP, N), S1
	mov	(AP, N), S0
	xor	-8(BP, N), S1
	xor	(BP, N), S0
	mov	S1, TMP
	shr	%cl, TMP
	neg	%cl
	shl	%cl, S0
	neg	%cl
	
	or	S0, TMP
	mov	TMP, -8(DST, N)
	sub	$8, N
	jz	.Ldone
	jmp 	.Lshift_next

	ALIGN(16)

.Lshift_loop:
	mov	8(AP, N), S0
	xor	8(BP, N), S0
	mov	S0, TMP
	shr	%cl, TMP
	neg	%cl
	shl	%cl, S1
	neg	%cl
	or	S1, TMP
	mov	TMP, 8(DST, N)

	mov	(AP, N), S1
	xor	(BP, N), S1
	mov	S1, TMP
	shr	%cl, TMP
	neg	%cl
	shl	%cl, S0
	neg 	%cl
	or	S0, TMP
	mov	TMP, (DST, N)
.Lshift_next:
	sub	$16, N
	C FIXME: Handle the case N == 16 specially,
	C like in the non-shifted case? 
C 	ja	.Lshift_loop
C 	jz	.Ldone
	jnc	.Lshift_loop

	add	$15, N
	jnc	.Ldone

	shr	$3, %rcx
	add	%rcx, AP
	add	%rcx, BP
	jmp	.Lfinal_loop
	
.Lno_shift_case:
	C Next destination word is -8(DST, N)
	C Setup for unrolling
	test	$8, N
	jz	.Lword_next

	sub	$8, N
	jz	.Lone_word

	mov	(AP, N), TMP
	xor	(BP, N), TMP
	mov	TMP, (DST, N)
	
	jmp	.Lword_next

	ALIGN(16)

.Lword_loop:
	mov	8(AP, N), TMP
	mov	(AP, N), TMP2
	xor	8(BP, N), TMP
	xor	(BP, N), TMP2
	mov	TMP, 8(DST, N)
	mov	TMP2, (DST, N)

.Lword_next:
	sub	$16, N
	ja	.Lword_loop	C Not zero and no carry
	jnz	.Lfinal

	C Final operation is word aligned
	mov	8(AP, N), TMP
	xor	8(BP, N), TMP
	mov	TMP, 8(DST, N)
	
.Lone_word:
	mov	(AP, N), TMP
	xor	(BP, N), TMP
	mov	TMP, (DST, N)

	C ENTRY might have been 3 args, too, but it doesn't matter for the exit
	W64_EXIT(4, 0)
	ret

.Lfinal:
	add	$15, N

.Lfinal_loop:
	movb	(AP, N), LREG(TMP)
	xorb	(BP, N), LREG(TMP)
	movb	LREG(TMP), (DST, N)
.Lfinal_next:
	sub	$1, N
	jnc	.Lfinal_loop

.Ldone:
	C ENTRY might have been 3 args, too, but it doesn't matter for the exit
	W64_EXIT(4, 0)
	ret

ifelse(USE_SSE2, yes, <

.Lsse2_case:
	lea	(DST, N), TMP
	test	$8, TMP
	jz	.Lsse2_next
	sub	$8, N
	mov	(AP, N), TMP
	xor	(BP, N), TMP
	mov	TMP, (DST, N)
	jmp	.Lsse2_next

	ALIGN(16)
.Lsse2_loop:
	movdqu	(AP, N), %xmm0
	movdqu	(BP, N), %xmm1
	pxor	%xmm0, %xmm1
	movdqa	%xmm1, (DST, N)
.Lsse2_next:
	sub	$16, N
	ja	.Lsse2_loop
	
	C FIXME: See if we can do a full word first, before the
	C byte-wise final loop.
	jnz	.Lfinal		

	C Final operation is aligned
	movdqu	(AP), %xmm0
	movdqu	(BP), %xmm1
	pxor	%xmm0, %xmm1
	movdqa	%xmm1, (DST)
	C ENTRY might have been 3 args, too, but it doesn't matter for the exit
	W64_EXIT(4, 0)
	ret
>)	
	

EPILOGUE(nettle_memxor3)
