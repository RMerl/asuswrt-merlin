/* Copyright 2005, 2007 Free Software Foundation, Inc.

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

void gt (double a);

int
main (void)
{
  gt (360.0);
  gt (-360.0);

  ge (360.0);
  ge (-360.0);

  lt (-360.0);
  lt (360.0);

  le (-360.0);
  le (360.0);

  eq (0.0);
  eq (360.0);

  ne (360.0);
  ne (0.0);

  return 0;
}

asm ("       .text\n"
     "       .ent gt\n"
     "gt:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fbgt $f16,$gt_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$gt_1:\n"
     "       ret $31,($26),1\n"
     "       .end gt\n");

asm ("       .text\n"
     "       .ent ge\n"
     "ge:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fbge $f16,$ge_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$ge_1:\n"
     "       ret $31,($26),1\n"
     "       .end ge\n");

asm ("       .text\n"
     "       .ent lt\n"
     "lt:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fblt $f16,$lt_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$lt_1:\n"
     "       ret $31,($26),1\n"
     "       .end lt\n");

asm ("       .text\n"
     "       .ent le\n"
     "le:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fble $f16,$le_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$le_1:\n"
     "       ret $31,($26),1\n"
     "       .end le\n");

asm ("       .text\n"
     "       .ent eq\n"
     "eq:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fbeq $f16,$eq_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$eq_1:\n"
     "       ret $31,($26),1\n"
     "       .end eq\n");

asm ("       .text\n"
     "       .ent ne\n"
     "ne:\n"
     "       .frame $30,0,$26,0\n"
     "       .prologue 0\n"
     "       cpys $f31,$f31,$f0\n"
     "       fbne $f16,$ne_1\n"      /* stop at this instruction.  */
     "       cpysn $f16,$f16,$f0\n"
     "$ne_1:\n"
     "       ret $31,($26),1\n"
     "       .end ne\n");


