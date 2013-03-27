! Copyright 2004 Free Software Foundation, Inc.
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 3 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with this program.  If not, see <http://www.gnu.org/licenses/>.
!
! Please email any bugs, comments, and/or additions to this file to:
! bug-gdb@gnu.org
!
! This file is part of the gdb testsuite.
!
! It was generated using "sh-elf-gcc -S gdb1291.c", using the following
! source file:
!
!	#include <stdio.h>
!	
!	main()
!	{
!	  printf("hello world\n");
!	  sub1();
!	  sub2();
!	}
!	sub1()
!	{
!	  int buf[64];
!	
!	}
!	
!	sub2()
!	{
!	  int buf[65];
!	
!	}
!
! We use a pregenerated assembly file as the test input to avoid possible
! problems with future versions of gcc generating different code.

	.file	"gdb1291.c"
	.text
	.section	.rodata
	.align 2
.LC0:
	.string	"hello world\n"
	.text
	.align 1
	.global	_main
	.type	_main, @function
_main:
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	mov	r15,r14
	mov.l	.L2,r1
	mov	r1,r4
	mov.l	.L3,r1
	jsr	@r1
	nop
	mov.l	.L4,r1
	jsr	@r1
	nop
	mov.l	.L5,r1
	jsr	@r1
	nop
	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts	
	nop
.L6:
	.align 2
.L2:
	.long	.LC0
.L3:
	.long	_printf
.L4:
	.long	_sub1
.L5:
	.long	_sub2
	.size	_main, .-_main
	.align 1
	.global	_sub1
	.type	_sub1, @function
_sub1:
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	add	#-128,r15
	add	#-128,r15
	mov	r15,r14
	mov.w	.L8,r7
	add	r7,r14
	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts	
	nop
	.align 1
.L8:
	.short	256
	.size	_sub1, .-_sub1
	.align 1
	.global	_sub2
	.type	_sub2, @function
_sub2:
	mov.l	r14,@-r15
	sts.l	pr,@-r15
	mov.w	.L11,r1
	sub	r1,r15
	mov	r15,r14
	mov.w	.L11,r7
	add	r7,r14
	mov	r14,r15
	lds.l	@r15+,pr
	mov.l	@r15+,r14
	rts	
	nop
	.align 1
.L11:
	.short	260
	.size	_sub2, .-_sub2
	.ident	"GCC: (GNU) 3.5.0 20040204 (experimental)"
