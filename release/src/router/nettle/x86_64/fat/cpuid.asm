C x86_64/fat/cpuid.asm

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
C cpuid input: %edi
C output pointer: %rsi 	

	.file "cpuid.asm"

	C void _nettle_cpuid(uint32_t in, uint32_t *out)

	.text
	ALIGN(16)
PROLOGUE(_nettle_cpuid)
	W64_ENTRY(2)
	push	%rbx
	
	movl	%edi, %eax
	cpuid
	mov	%eax, (%rsi)
	mov	%ebx, 4(%rsi)
	mov	%ecx, 8(%rsi)
	mov	%edx, 12(%rsi)

	pop	%rbx
	W64_EXIT(2)
	ret
EPILOGUE(_nettle_cpuid)

