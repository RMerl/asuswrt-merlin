C x86_64/aesni/aes-decrypt-internal.asm


ifelse(<
   Copyright (C) 2015 Niels Möller

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

C Input argument
define(<ROUNDS>, <%rdi>)
define(<KEYS>,	<%rsi>)
C define(<TABLE>,	<%rdx>) C Unused here
define(<LENGTH>,<%rcx>)
define(<DST>,	<%r8>)
define(<SRC>,	<%r9>)

C Round counter
define(<CNT>, <%rdx>)
C Subkey pointer
define(<KEY>, <%rax>)

dnl aesdec %xmm1, %xmm0
define(<AESDEC>, <.byte 0x66, 0x0f, 0x38, 0xde, 0xc1>)
dnl aesdeclast %xmm1, %xmm0
define(<AESDECLAST>, <.byte 0x66, 0x0f, 0x38, 0xdf, 0xc1>)

	.file "aes-decrypt-internal.asm"

	C _aes_decrypt(unsigned rounds, const uint32_t *keys,
	C	       const struct aes_table *T,
	C	       size_t length, uint8_t *dst,
	C	       uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(_nettle_aes_decrypt)
	W64_ENTRY(6, 2)
	shr	$4, LENGTH
	test	LENGTH, LENGTH
	jz	.Lend

	decl	XREG(ROUNDS)

.Lblock_loop:
	mov	ROUNDS, CNT
	mov	KEYS, KEY
	movups	(SRC), %xmm0
	C FIXME: Better alignment of subkeys, so we can use movaps.
	movups	(KEY), %xmm1
	pxor	%xmm1, %xmm0

	C FIXME: Could use some unrolling. Also all subkeys fit in
	C registers, so they could be loaded once (on W64 we would
	C need to save and restore some xmm registers, though).

.Lround_loop:
	add	$16, KEY

	movups	(KEY), %xmm1
	AESDEC	C %xmm1, %xmm0
	decl	XREG(CNT)
	jnz	.Lround_loop

	movups	16(KEY), %xmm1
	AESDECLAST	C %xmm1, %xmm0

	movups	%xmm0, (DST)
	add	$16, SRC
	add	$16, DST
	dec	LENGTH
	jnz	.Lblock_loop

.Lend:
	W64_EXIT(6, 2)
	ret
EPILOGUE(_nettle_aes_decrypt)
