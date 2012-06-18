/* Copyright (C) 1991, 92, 94, 95, 96, 97, 98 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _SYS_FPREGDEF_H
#define _SYS_FPREGDEF_H

/*
 * These definitions only cover the R3000-ish 16/32 register model.
 * But we're trying to be R3000 friendly anyway ...
 */
#define fv0	$f0      /* return value */
#define fv0f	$f1
#define fv1	$f2
#define fv1f	$f3
#define fa0	$f12     /* argument registers */
#define fa0f	$f13
#define fa1	$f14
#define fa1f	$f15
#define ft0	$f4      /* caller saved */
#define ft0f	$f5
#define ft1	$f6
#define ft1f	$f7
#define ft2	$f8
#define ft2f	$f9
#define ft3	$f10
#define ft3f	$f11
#define ft4	$f16
#define ft4f	$f17
#define ft5	$f18
#define ft5f	$f19
#define fs0	$f20     /* callee saved */
#define fs0f	$f21
#define fs1	$f22
#define fs1f	$f23
#define fs2	$f24
#define fs2f	$f25
#define fs3	$f26
#define fs3f	$f27
#define fs4	$f28
#define fs4f	$f29
#define fs5	$f30
#define fs5f	$f31

#define fcr31	$31      /* FPU status register */

#endif /* sys/fpregdef.h */
