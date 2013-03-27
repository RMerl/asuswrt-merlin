/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004, 2007 Free Software Foundation, Inc.

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

void arg_passing_test2 (void);

int
main (void)
{
  arg_passing_test2 ();
  return 0;
}


/* Asm for procedure arg_passing_test2.

   The challenge here is getting past the 'mr 0,3' and 'stb'
   instructions.  */

asm ("	.section	\".text\"\n"
     "	.align 2\n"
     "	.globl arg_passing_test2\n"
     "	.type	arg_passing_test2, @function\n"
     "arg_passing_test2:\n"
     "	stwu 1,-64(1)\n"
     "	stw 31,60(1)\n"
     "	mr 31,1\n"
     "	mr 0,3\n"
     "	evstdd 4,16(31)\n"
     "	stw 5,24(31)\n"
     "	stw 7,32(31)\n"
     "	stw 8,36(31)\n"
     "	stw 9,40(31)\n"
     "	stb 0,8(31)\n"
     "	lwz 11,0(1)\n"
     "	lwz 31,-4(11)\n"
     "	mr 1,11\n"
     "	blr\n"
     "	.size	arg_passing_test2, .-arg_passing_test2\n");
