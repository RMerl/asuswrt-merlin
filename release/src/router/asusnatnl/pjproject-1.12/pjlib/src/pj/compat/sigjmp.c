/* $Id: sigjmp.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/config.h>
#include <pj/compat/setjmp.h>

int __sigjmp_save(sigjmp_buf env, int savemask)
{
    return 0;
}

extern int __sigsetjmp(pj_jmp_buf env, int savemask);
extern void __longjmp(pj_jmp_buf env, int val) __attribute__((noreturn));

PJ_DEF(int) pj_setjmp(pj_jmp_buf env)
{
    return __sigsetjmp(env, 0);
}

PJ_DEF(void) pj_longjmp(pj_jmp_buf env, int val)
{
    __longjmp(env, val);
}

