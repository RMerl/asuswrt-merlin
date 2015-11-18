C x86_64/aes-decrypt-internal.asm

ifelse(<
   Copyright (C) 2001, 2002, 2005, Rafael R. Sevilla, Niels Möller
   Copyright (C) 2008, 2013 Niels Möller

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

include_src(<x86_64/aes.m4>)

C Register usage:

C AES state, use two of them
define(<SA>,<%eax>)
define(<SB>,<%ebx>)
define(<SC>,<%ecx>)
define(<SD>,<%edx>)

define(<TA>,<%r10d>)
define(<TB>,<%r11d>)
define(<TC>,<%r12d>)

C Input argument
define(<ROUNDS>, <%rdi>)
define(<KEYS>,	<%rsi>)
define(<PARAM_TABLE>,	<%rdx>)
define(<PARAM_LENGTH>,<%rcx>)
define(<DST>,	<%r8>)
define(<SRC>,	<%r9>)

define(<TABLE>, <%r13>) 
define(<LENGTH>,<%r14>)
define(<KEY>,	<%r15>)

C Must correspond to an old-style register, for movzb from %ah--%dh to
C work.
define(<TMP>,<%rbp>)

	.file "aes-decrypt-internal.asm"
	
	C _aes_decrypt(unsigned rounds, const uint32_t *keys,
	C	       const struct aes_table *T,
	C	       size_t length, uint8_t *dst,
	C	       uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(_nettle_aes_decrypt)
	W64_ENTRY(6, 0)
	test	PARAM_LENGTH, PARAM_LENGTH
	jz	.Lend

        C save all registers that need to be saved
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15	

	subl	$1, XREG(ROUNDS)
	push	ROUNDS		C Rounds at (%rsp) 
	
	mov	PARAM_TABLE, TABLE
	mov	PARAM_LENGTH, LENGTH
	shr	$4, LENGTH
.Lblock_loop:
	mov	KEYS, KEY
	
	AES_LOAD(SA, SB, SC, SD, SRC, KEY)
	add	$16, SRC	C Increment src pointer

	movl	(%rsp), XREG(ROUNDS)

	add	$16, KEY	C  point to next key
	ALIGN(16)
.Lround_loop:
	AES_ROUND(TABLE, SA,SD,SC,SB, TA, TMP)
	AES_ROUND(TABLE, SB,SA,SD,SC, TB, TMP)
	AES_ROUND(TABLE, SC,SB,SA,SD, TC, TMP)
	AES_ROUND(TABLE, SD,SC,SB,SA, SD, TMP)

	movl	TA, SA
	movl	TB, SB
	movl	TC, SC

	xorl	(KEY),SA	C  add current session key to plaintext
	xorl	4(KEY),SB
	xorl	8(KEY),SC
	xorl	12(KEY),SD

	add	$16, KEY	C  point to next key
	decl	XREG(ROUNDS)
	jnz	.Lround_loop

	C last round
	AES_FINAL_ROUND(SA,SD,SC,SB, TABLE, TA, TMP)
	AES_FINAL_ROUND(SB,SA,SD,SC, TABLE, TB, TMP)
	AES_FINAL_ROUND(SC,SB,SA,SD, TABLE, TC, TMP)
	AES_FINAL_ROUND(SD,SC,SB,SA, TABLE, SD, TMP)

	C Inverse S-box substitution
	mov	$3, XREG(ROUNDS)
.Lsubst:
	AES_SUBST_BYTE(TA,TB,TC,SD, TABLE, TMP)

	decl	XREG(ROUNDS)
	jnz	.Lsubst

	C Add last subkey, and store decrypted data
	AES_STORE(TA,TB,TC,SD, KEY, DST)
	
	add	$16, DST
	dec	LENGTH

	jnz	.Lblock_loop

	lea	8(%rsp), %rsp	C Drop ROUNDS
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
.Lend:
	W64_EXIT(6, 0)
	ret
EPILOGUE(_nettle_aes_decrypt)
