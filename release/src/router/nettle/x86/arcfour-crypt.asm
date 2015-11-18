C x86/arcfour-crypt.asm

ifelse(<
   Copyright (C) 2004, Niels Möller

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

	.file "arcfour-crypt.asm"

	C arcfour_crypt(struct arcfour_ctx *ctx,
	C               size_t length, uint8_t *dst,
	C               const uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(nettle_arcfour_crypt)
	C save all registers that need to be saved
	pushl	%ebx		C  12(%esp)
	pushl	%ebp		C  8(%esp)
	pushl	%esi		C  4(%esp)
	pushl	%edi		C  0(%esp)

C Input arguments:
	C ctx = 20(%esp)
	C length = 24(%esp)
	C dst = 28(%esp)
	C src = 32(%esp)
C Register usage:
	C %ebp = ctx
	C %esi = src
	C %edi = dst
	C %edx = loop counter
	C %eax = i
	C %ebx = j
	C %cl  = si
	C %ch  = sj

	movl	24(%esp), %edx		C  length
	movl	20(%esp), %ebp		C  ctx
	movl	28(%esp), %edi		C  dst
	movl	32(%esp), %esi		C  src

	lea	(%edx, %edi), %edi
	lea	(%edx, %esi), %esi
	negl	%edx
	jnc	.Lend
	
	movzbl  ARCFOUR_I (%ebp), %eax	C  i
	movzbl  ARCFOUR_J (%ebp), %ebx	C  j

	incb	%al
	sarl	$1, %edx
	jc	.Lloop_odd
	
	ALIGN(16)
.Lloop:
	movb	(%ebp, %eax), %cl	C  si.
	addb    %cl, %bl
	movb    (%ebp, %ebx), %ch	C  sj
	movb    %ch, (%ebp, %eax)	C  S[i] = sj
	incl	%eax
	movzbl	%al, %eax
	movb	%cl, (%ebp, %ebx)	C  S[j] = si
	addb    %ch, %cl
	movzbl  %cl, %ecx		C  Clear, so it can be used
					C  for indexing.
	movb    (%ebp, %ecx), %cl
	xorb    (%esi, %edx, 2), %cl
	movb    %cl, (%edi, %edx, 2)

	C FIXME: Could exchange cl and ch in the second half
	C and try to interleave instructions better.
.Lloop_odd:
	movb	(%ebp, %eax), %cl	C  si.
	addb    %cl, %bl
	movb    (%ebp, %ebx), %ch	C  sj
	movb    %ch, (%ebp, %eax)	C  S[i] = sj
	incl	%eax
	movzbl	%al, %eax
	movb	%cl, (%ebp, %ebx)	C  S[j] = si
	addb    %ch, %cl
	movzbl  %cl, %ecx		C  Clear, so it can be used
					C  for indexing.
	movb    (%ebp, %ecx), %cl
	xorb    1(%esi, %edx, 2), %cl
	incl    %edx
	movb    %cl, -1(%edi, %edx, 2)

	jnz	.Lloop

C .Lloop_done:
	decb	%al
	movb	%al, ARCFOUR_I (%ebp)		C  Store the new i and j.
	movb	%bl, ARCFOUR_J (%ebp)
.Lend:
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
EPILOGUE(nettle_arcfour_crypt)
