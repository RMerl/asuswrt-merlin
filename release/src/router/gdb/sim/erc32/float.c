/*
 * This file is part of SIS.
 *
 * SIS, SPARC instruction simulator. Copyright (C) 1995 Jiri Gaisler, European
 * Space Agency
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This file implements the interface between the host and the simulated
 * FPU. IEEE trap handling is done as follows: 
 * 1. In the host, all IEEE traps are masked
 * 2. After each simulated FPU instruction, check if any exception occured 
 *    by reading the exception bits from the host FPU status register 
 *    (get_accex()).
 * 3. Propagate any exceptions to the simulated FSR.
 * 4. Clear host exception bits
 *
 *
 * This can also be done using ieee_flags() library routine on sun.
 */

#include "sis.h"

/* Forward declarations */

extern uint32	_get_sw PARAMS ((void));
extern uint32	_get_cw PARAMS ((void));
static void	__setfpucw PARAMS ((unsigned short fpu_control));

/* This host dependent routine should return the accrued exceptions */
int
get_accex()
{
#ifdef sparc
    return ((_get_fsr_raw() >> 5) & 0x1F);
#elif i386
    uint32 accx;

    accx = _get_sw() & 0x3f;
    accx = ((accx & 1) << 4) | ((accx & 2) >> 1) | ((accx & 4) >> 1) |
	   (accx & 8) | ((accx & 16) >> 2) | ((accx & 32) >> 5);
    return(accx);
#else
    return(0);
#warning no fpu trap support for this target
#endif

}

/* How to clear the accrued exceptions */
void
clear_accex()
{
#ifdef sparc
    set_fsr((_get_fsr_raw() & ~0x3e0));
#elif i386
    asm("\n"
".text\n"
"	fnclex\n"
"\n"
"    ");
#else
#warning no fpu trap support for this target
#endif
}

/* How to map SPARC FSR onto the host */
void
set_fsr(fsr)
uint32 fsr;
{
#ifdef sparc
	_set_fsr_raw(fsr & ~0x0f800000);
#elif i386
     void __setfpucw(unsigned short fpu_control);
     uint32 rawfsr;

     fsr >>= 30;
     switch (fsr) {
	case 0: 
	case 2: break;
	case 1: fsr = 3;
	case 3: fsr = 1;
     }
     rawfsr = _get_cw();
     rawfsr |= (fsr << 10) | 0x3ff;
     __setfpucw(rawfsr);
#else
#warning no fpu trap support for this target
#endif
}


/* Host dependent support functions */

#ifdef sparc

    asm("\n"
"\n"
".text\n"
"        .align 4\n"
"        .global __set_fsr_raw,_set_fsr_raw\n"
"__set_fsr_raw:\n"
"_set_fsr_raw:\n"
"        save %sp,-104,%sp\n"
"        st %i0,[%fp+68]\n"
"        ld [%fp+68], %fsr\n"
"        mov 0,%i0\n"
"        ret\n"
"        restore\n"
"\n"
"        .align 4\n"
"        .global __get_fsr_raw\n"
"        .global _get_fsr_raw\n"
"__get_fsr_raw:\n"
"_get_fsr_raw:\n"
"        save %sp,-104,%sp\n"
"        st %fsr,[%fp+68]\n"
"        ld [%fp+68], %i0\n"
"        ret\n"
"        restore\n"
"\n"
"    ");

#elif i386

    asm("\n"
"\n"
".text\n"
"        .align 8\n"
".globl _get_sw,__get_sw\n"
"__get_sw:\n"
"_get_sw:\n"
"        pushl %ebp\n"
"        movl %esp,%ebp\n"
"        movl $0,%eax\n"
"        fnstsw %ax\n"
"        movl %ebp,%esp\n"
"        popl %ebp\n"
"        ret\n"
"\n"
"        .align 8\n"
".globl _get_cw,__get_cw\n"
"__get_cw:\n"
"_get_cw:\n"
"        pushl %ebp\n"
"        movl %esp,%ebp\n"
"        subw $2,%esp\n"
"        fnstcw -2(%ebp)\n"
"        movw -2(%ebp),%eax\n"
"        movl %ebp,%esp\n"
"        popl %ebp\n"
"        ret\n"
"\n"
"\n"
"    ");


#else
#warning no fpu trap support for this target
#endif

#if i386
/* #if defined _WIN32 || defined __GO32__ */
/* This is so floating exception handling works on NT
   These definitions are from the linux fpu_control.h, which
   doesn't exist on NT.

   default to:
     - extended precision
     - rounding to nearest
     - exceptions on overflow, zero divide and NaN
*/
#define _FPU_DEFAULT  0x1372 
#define _FPU_RESERVED 0xF0C0  /* Reserved bits in cw */

static void
__setfpucw(unsigned short fpu_control)
{
  volatile unsigned short cw;

  /* If user supplied _fpu_control, use it ! */
  if (!fpu_control)
  { 
    /* use defaults */
    fpu_control = _FPU_DEFAULT;
  }
  /* Get Control Word */
  __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
  
  /* mask in */
  cw &= _FPU_RESERVED;
  cw = cw | (fpu_control & ~_FPU_RESERVED);

  /* set cw */
  __asm__ volatile ("fldcw %0" :: "m" (cw));
}
/* #endif */
#endif
