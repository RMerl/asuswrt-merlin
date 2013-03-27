/* Unwinder test program.

   Copyright (C) 2003, 2004, 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef SYMBOL_PREFIX
#define SYMBOL(str)	SYMBOL_PREFIX #str
#else
#define SYMBOL(str)	#str
#endif

void gdb1253 (void);
void gdb1718 (void);
void gdb1338 (void);
void jump_at_beginning (void);

int
main (void)
{
  standard ();
  stack_align_ecx ();
  stack_align_edx ();
  stack_align_eax ();
  gdb1253 ();
  gdb1718 ();
  gdb1338 ();
  jump_at_beginning ();
  return 0;
}

/* A normal prologue.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (standard) ":\n"
    "    pushl %ebp\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from symtab/1253.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1253) ":\n"
    "    pushl %ebp\n"
    "    xorl  %ecx, %ecx\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from backtrace/1718.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1718) ":\n"
    "    pushl %ebp\n"
    "    movl  $0x11111111, %eax\n"
    "    movl  %esp, %ebp\n"
    "    pushl %esi\n"
    "    movl  $0x22222222, %esi\n"
    "    pushl %ebx\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from backtrace/1338.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1338) ":\n"
    "    pushl %edi\n"
    "    pushl %esi\n"
    "    pushl %ebx\n"
    "    int   $0x03\n"
    "    popl  %ebx\n"
    "    popl  %esi\n"
    "    popl  %edi\n"
    "    ret\n");

/* The purpose of this function is to verify that, during prologue
   skip, GDB does not follow a jump at the beginnning of the "real"
   code.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (jump_at_beginning) ":\n"
    "    pushl %ebp\n"
    "    movl  %esp,%ebp\n"
    "    jmp   .gdbjump\n"
    "    nop\n"
    ".gdbjump:\n"
    "    movl  %ebp,%esp\n"
    "    popl  %ebp\n"
    "    ret\n");

asm(".text\n"
    "    .align 8\n"
    SYMBOL (stack_align_ecx) ":\n"
    "    leal  4(%esp), %ecx\n"
    "    andl  $-16, %esp\n"
    "    pushl -4(%ecx)\n"
    "    pushl %ebp\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    pushl %ecx\n"
    "    int   $0x03\n"
    "    popl  %ecx\n"
    "    popl  %edi\n"
    "    popl  %ebp\n"
    "    leal  -4(%ecx), %esp\n"
    "    ret\n");

asm(".text\n"
    "    .align 8\n"
    SYMBOL (stack_align_edx) ":\n"
    "    leal  4(%esp), %edx\n"
    "    andl  $-16, %esp\n"
    "    pushl -4(%edx)\n"
    "    pushl %ebp\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    pushl %ecx\n"
    "    int   $0x03\n"
    "    popl  %ecx\n"
    "    popl  %edi\n"
    "    popl  %ebp\n"
    "    leal  -4(%edx), %esp\n"
    "    ret\n");

asm(".text\n"
    "    .align 8\n"
    SYMBOL (stack_align_eax) ":\n"
    "    leal  4(%esp), %eax\n"
    "    andl  $-16, %esp\n"
    "    pushl -4(%eax)\n"
    "    pushl %ebp\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    pushl %ecx\n"
    "    int   $0x03\n"
    "    popl  %ecx\n"
    "    popl  %edi\n"
    "    popl  %ebp\n"
    "    leal  -4(%eax), %esp\n"
    "    ret\n");

