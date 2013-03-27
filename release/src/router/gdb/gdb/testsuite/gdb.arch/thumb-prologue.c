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

void tpcs_frame (void);

int
main (void)
{
  tpcs_frame ();
  return 0;
}

/* Normally Thumb functions use r7 as the frame pointer.  However,
   with the GCC option -mtpcs-frame, they may use fp instead.  Make
   sure that the prologue analyzer can handle this.  */

asm(".text\n"
    "	.align 2\n"
    "	.thumb_func\n"
    "	.code 16\n"
    "tpcs_frame_1:\n"
    "	sub	sp, #16\n"
    "	push	{r7}\n"
    "	add	r7, sp, #20\n"
    "	str	r7, [sp, #8]\n"
    "	mov	r7, pc\n"
    "	str	r7, [sp, #16]\n"
    "	mov	r7, fp\n"
    "	str	r7, [sp, #4]\n"
    "	mov	r7, lr\n"
    "	str	r7, [sp, #12]\n"
    "	add	r7, sp, #16\n"
    "	mov	fp, r7\n"
    "	mov	r7, sl\n"
    "	push	{r7}\n"

    /* We'll set a breakpoint at this call.  We can't hardcode a trap
       instruction; the right instruction to use varies too much.  And
       we can't use a global label, because GDB will think that's the
       start of a new function.  So, this slightly convoluted
       technique.  */
    ".Ltpcs:\n"
    "	nop\n"

    "	pop	{r2}\n"
    "	mov	sl, r2\n"
    "	pop	{r7}\n"
    "	pop	{r1, r2}\n"
    "	mov	fp, r1\n"
    "	mov	sp, r2\n"
    "	bx	lr\n"

    "	.align 2\n"
    "	.type tpcs_offset, %object\n"
    "tpcs_offset:\n"
    "	.word .Ltpcs - tpcs_frame_1\n"

    "	.align 2\n"
    "	.thumb_func\n"
    "	.code 16\n"
    "tpcs_frame:\n"
    "	sub	sp, #16\n"
    "	push	{r7}\n"
    "	add	r7, sp, #20\n"
    "	str	r7, [sp, #8]\n"
    "	mov	r7, pc\n"
    "	str	r7, [sp, #16]\n"
    "	mov	r7, fp\n"
    "	str	r7, [sp, #4]\n"
    "	mov	r7, lr\n"
    "	str	r7, [sp, #12]\n"
    "	add	r7, sp, #16\n"
    "	mov	fp, r7\n"
    "	mov	r7, sl\n"
    "	push	{r7}\n"

    /* Clobber saved regs around the call.  */
    "	mov	r7, #0\n"
    "	mov	lr, r7\n"
    "	bl	tpcs_frame_1\n"

    "	pop	{r2}\n"
    "	mov	sl, r2\n"
    "	pop	{r7}\n"
    "	pop	{r1, r2, r3}\n"
    "	mov	fp, r1\n"
    "	mov	sp, r2\n"
    "	mov	lr, r3\n"
    "	bx	lr\n"
);
