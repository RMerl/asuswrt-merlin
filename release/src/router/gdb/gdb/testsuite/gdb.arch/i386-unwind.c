/* Unwinder test program.

   Copyright 2003, 2004, 2007 Free Software Foundation, Inc.

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

void
trap (void)
{
  asm ("int $0x03");
}

/* Make sure that main directly follows a function without an
   epilogue.  */

asm(".text\n"
    "    .align 8\n"
    "    .globl gdb1435\n"
    "gdb1435:\n"
    "    pushl %ebp\n"
    "    mov   %esp, %ebp\n"
    "    call  " SYMBOL (trap) "\n"
    "    .globl " SYMBOL (main) "\n"
    SYMBOL (main) ":\n"
    "    pushl %ebp\n"
    "    mov   %esp, %ebp\n"
    "    call  gdb1435\n");
