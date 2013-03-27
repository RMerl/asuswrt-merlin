/* Unwinder test program.

   Copyright 2006, 2007 Free Software Foundation, Inc.

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

void gdb2029 (void);

int
main (void)
{
  gdb2029 ();
  optimized_1 ();
  return 0;
}

void
optimized_1_marker (void)
{
}

void
gdb2029_marker (void)
{
}

/* A typical PIC prologue from GCC.  */

asm(".text\n"
    "    .p2align 3\n"
    SYMBOL (gdb2029) ":\n"
    "	stwu	%r1, -32(%r1)\n"
    "	mflr	%r0\n"
    "	bcl-	20,31,.+4\n"
    "	stw	%r30, 24(%r1)\n"
    "	mflr	%r30\n"
    "	stw	%r0, 36(%r1)\n"
    "	bl	gdb2029_marker\n"
    "	lwz	%r0, 36(%r1)\n"
    "	lwz	%r30, 24(%r1)\n"
    "	mtlr	%r0\n"
    "	addi	%r1, %r1, 32\n"
    "	blr");

/* A heavily scheduled prologue.  */
asm(".text\n"
    "	.p2align 3\n"
    SYMBOL (optimized_1) ":\n"
    "	stwu	%r1,-32(%r1)\n"
    "	lis	%r9,-16342\n"
    "	lis	%r11,-16342\n"
    "	mflr	%r0\n"
    "	addi	%r11,%r11,3776\n"
    "	stmw	%r27,12(%r1)\n"
    "	addi	%r31,%r9,3152\n"
    "	cmplw	%cr7,%r31,%r11\n"
    "	stw	%r0,36(%r1)\n"
    "	mr	%r30,%r3\n"
    "	bl	optimized_1_marker\n"
    "	lwz	%r0,36(%r1)\n"
    "	lmw	%r27,12(%r1)\n"
    "	addi	%r1,%r1,32\n"
    "	blr");
