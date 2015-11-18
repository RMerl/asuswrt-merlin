C x86/aes-decrypt-internal.asm

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

include_src(<x86/aes.m4>)

C Register usage:

C AES state
define(<SA>,<%eax>)
define(<SB>,<%ebx>)
define(<SC>,<%ecx>)
define(<SD>,<%edx>)

C Primary use of these registers. They're also used temporarily for other things.
define(<T>,<%ebp>)
define(<TMP>,<%edi>)
define(<KEY>,<%esi>)

define(<PARAM_ROUNDS>,	<40(%esp)>)
define(<PARAM_KEYS>,	<44(%esp)>)
define(<PARAM_TABLE>,	<48(%esp)>)
define(<PARAM_LENGTH>,	<52(%esp)>)
define(<PARAM_DST>,	<56(%esp)>)
define(<PARAM_SRC>,	<60(%esp)>)

define(<FRAME_KEY>,	<16(%esp)>)
define(<FRAME_COUNT>,	<12(%esp)>)
define(<TA>,		<8(%esp)>)
define(<TB>,		<4(%esp)>)
define(<TC>,		<(%esp)>)

C The aes state is kept in %eax, %ebx, %ecx and %edx
C
C %esi is used as temporary, to point to the input, and to the
C subkeys, etc.
C
C %ebp is used as the round counter, and as a temporary in the final round.
C
C %edi is a temporary, often used as an accumulator.

	.file "aes-decrypt-internal.asm"
	
	C _aes_decrypt(unsigned rounds, const uint32_t *keys,
	C	       const struct aes_table *T,
	C	       size_t length, uint8_t *dst,
	C	       uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(_nettle_aes_decrypt)
	C save all registers that need to be saved
	pushl	%ebx		C  20(%esp)
	pushl	%ebp		C  16(%esp)
	pushl	%esi		C  12(%esp)
	pushl	%edi		C  8(%esp)

	subl	$20, %esp	C  loop counter and save area for the key pointer

	movl	PARAM_LENGTH, %ebp
	testl	%ebp,%ebp
	jz	.Lend

	shrl	$4, PARAM_LENGTH
	subl	$1, PARAM_ROUNDS
.Lblock_loop:
	movl	PARAM_KEYS, KEY	C  address of subkeys
	
	movl	PARAM_SRC, TMP	C  address of plaintext
	AES_LOAD(SA, SB, SC, SD, TMP, KEY)
	addl	$16, PARAM_SRC	C Increment src pointer
	movl	PARAM_TABLE, T

	movl	PARAM_ROUNDS, TMP
	C Loop counter on stack
	movl	TMP, FRAME_COUNT

	addl	$16,KEY		C  point to next key
	movl	KEY,FRAME_KEY
	ALIGN(16)
.Lround_loop:
	AES_ROUND(T, SA,SD,SC,SB, TMP, KEY)
	movl	TMP, TA

	AES_ROUND(T, SB,SA,SD,SC, TMP, KEY)
	movl	TMP, TB

	AES_ROUND(T, SC,SB,SA,SD, TMP, KEY)
	movl	TMP, TC

	AES_ROUND(T, SD,SC,SB,SA, SD, KEY)
	
	movl	TA, SA
	movl	TB, SB
	movl	TC, SC
	
	movl	FRAME_KEY, KEY

	xorl	(KEY),SA	C  add current session key to plaintext
	xorl	4(KEY),SB
	xorl	8(KEY),SC
	xorl	12(KEY),SD
	addl	$16,FRAME_KEY	C  point to next key
	decl	FRAME_COUNT
	jnz	.Lround_loop

	C last round

	AES_FINAL_ROUND(SA,SD,SC,SB,T, TMP, KEY)
	movl	TMP, TA

	AES_FINAL_ROUND(SB,SA,SD,SC,T, TMP, KEY)
	movl	TMP, TB

	AES_FINAL_ROUND(SC,SB,SA,SD,T, TMP, KEY)
	movl	TMP, TC

	AES_FINAL_ROUND(SD,SC,SB,SA,T, SD, KEY)

	movl	TA, SA
	movl	TB, SB
	movl	TC, SC

	C Inverse S-box substitution
	mov	$3,TMP
.Lsubst:
	AES_SUBST_BYTE(SA,SB,SC,SD, T, KEY)

	decl	TMP
	jnz	.Lsubst

	C Add last subkey, and store decrypted data
	movl	PARAM_DST,TMP
	movl	FRAME_KEY, KEY
	AES_STORE(SA,SB,SC,SD, KEY, TMP)
	
	addl	$16, PARAM_DST		C Increment destination pointer
	decl	PARAM_LENGTH

	jnz	.Lblock_loop

.Lend:
	addl	$20, %esp
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
EPILOGUE(_nettle_aes_decrypt)
