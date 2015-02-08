/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Simple setjmp/longjmp			File: lib_setjmp.S
    *  
    *  A very simple SETJMP and LONGJMP
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */




#include "sbmips.h"
#include "lib_setjmp.h"

#ifdef _MIPSREGS32_
#define SREG	sw
#define LREG	lw
#else
#define SREG	sd
#define LREG	ld
#endif

LEAF(lib_setjmp)

	SREG	s0,JMPB_S0(a0)
	SREG	s1,JMPB_S1(a0)
	SREG	s2,JMPB_S2(a0)
	SREG	s3,JMPB_S3(a0)
	SREG	s4,JMPB_S4(a0)
	SREG	s5,JMPB_S5(a0)
	SREG	s6,JMPB_S6(a0)
	SREG	s7,JMPB_S7(a0)
	SREG	fp,JMPB_FP(a0)
	SREG	sp,JMPB_SP(a0)
	SREG	ra,JMPB_RA(a0)
	move	v0,zero
	j	ra

END(lib_setjmp)

LEAF(lib_longjmp)
	LREG	s0,JMPB_S0(a0)
	LREG	s1,JMPB_S1(a0)
	LREG	s2,JMPB_S2(a0)
	LREG	s3,JMPB_S3(a0)
	LREG	s4,JMPB_S4(a0)
	LREG	s5,JMPB_S5(a0)
	LREG	s6,JMPB_S6(a0)
	LREG	s7,JMPB_S7(a0)
	LREG	fp,JMPB_FP(a0)
	LREG	sp,JMPB_SP(a0)
	LREG	ra,JMPB_RA(a0)
	move	v0,a1
	jr	ra
END(lib_longjmp)
