/* Test program for SSE registers.

   Copyright 2004, 2007 Free Software Foundation, Inc.

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

#include <stdio.h>
#include "i386-cpuid.h"

typedef struct {
  float f[4];
} v4sf_t;


v4sf_t data[8] =
  {
    { {  0.0, 0.25, 0.50, 0.75 } },
    { {  1.0, 1.25, 1.50, 1.75 } },
    { {  2.0, 2.25, 2.50, 2.75 } },
    { {  3.0, 3.25, 3.50, 3.75 } },
    { {  4.0, 4.25, 4.50, 4.75 } },
    { {  5.0, 5.25, 5.50, 5.75 } },
    { {  6.0, 6.25, 6.50, 6.75 } },
    { {  7.0, 7.25, 7.50, 7.75 } },
  };


int
have_sse (void)
{
  int edx = i386_cpuid ();

  if (edx & bit_SSE)
    return 1;
  else
    return 0;
}

int
main (int argc, char **argv)
{
  if (have_sse ())
    {
      asm ("movaps 0(%0), %%xmm0\n\t"
           "movaps 16(%0), %%xmm1\n\t"
           "movaps 32(%0), %%xmm2\n\t"
           "movaps 48(%0), %%xmm3\n\t"
           "movaps 64(%0), %%xmm4\n\t"
           "movaps 80(%0), %%xmm5\n\t"
           "movaps 96(%0), %%xmm6\n\t"
           "movaps 112(%0), %%xmm7\n\t"
           : /* no output operands */
           : "r" (data) 
           : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");

      puts ("Hi!"); /* first breakpoint here */

      asm (
           "movaps %%xmm0, 0(%0)\n\t"
           "movaps %%xmm1, 16(%0)\n\t"
           "movaps %%xmm2, 32(%0)\n\t"
           "movaps %%xmm3, 48(%0)\n\t"
           "movaps %%xmm4, 64(%0)\n\t"
           "movaps %%xmm5, 80(%0)\n\t"
           "movaps %%xmm6, 96(%0)\n\t"
           "movaps %%xmm7, 112(%0)\n\t"
           : /* no output operands */
           : "r" (data) 
           : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");

      puts ("Bye!"); /* second breakpoint here */
    }

  return 0;
}
