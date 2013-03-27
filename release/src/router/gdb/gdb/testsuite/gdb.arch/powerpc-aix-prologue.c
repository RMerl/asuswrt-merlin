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

void li_stw (void);

int
main (void)
{
  li_stw ();
  return 0;
}

/* Asm for procedure li_stw().

   The purpose of this function is to verify that GDB does not
   include the li insn as part of the function prologue (only part
   of the prologue if part of a pair of insns saving vector registers).
   Similarly, GDB should not include the stw insn following the li insn,
   because the source register is not used for parameter passing.  */


asm ("        .csect .text[PR]\n"
     "        .align 2\n"
     "        .lglobl .li_stw\n"
     "        .csect li_stw[DS]\n"
     "li_stw:\n"
     "        .long .li_stw, TOC[tc0], 0\n"
     "        .csect .text[PR]\n"
     ".li_stw:\n"
     "        stw 31,-4(1)\n"
     "        stwu 1,-48(1)\n"
     "        mr 31,1\n"
     "        stw 11,24(31)\n"
     "        li 0,8765\n"
     "        stw 0,28(31)\n"
     "        lwz 1,0(1)\n"
     "        lwz 31,-4(1)\n"
     "        blr\n");

