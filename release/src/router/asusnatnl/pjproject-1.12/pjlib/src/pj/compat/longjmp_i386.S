/* longjmp for i386.
   Copyright (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define _ASM
#define _SETJMP_H
#define PJ_LINUX_KERNEL	    1
#include <pj/compat/setjmp.h>

.global __longjmp
.type   __longjmp,%function
.align 4
__longjmp:
	movl 4(%esp), %ecx	/* User's jmp_buf in %ecx.  */
	movl 8(%esp), %eax	/* Second argument is return value.  */
	/* Save the return address now.  */
	movl (JB_PC*4)(%ecx), %edx
	/* Restore registers.  */
	movl (JB_BX*4)(%ecx), %ebx
	movl (JB_SI*4)(%ecx), %esi
	movl (JB_DI*4)(%ecx), %edi
	movl (JB_BP*4)(%ecx), %ebp
	movl (JB_SP*4)(%ecx), %esp
	/* Jump to saved PC.  */
	jmp *%edx
.size __longjmp,.-__longjmp

