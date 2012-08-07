/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Mini startup module for CFE apps		File: minicrt0.S
    *
    *  About the most minimal startup routine you can get.
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

/*
 * This module should be linked first!
 */

#define STACK_SIZE 8192

		.bss
		.comm	stack_bottom,STACK_SIZE
		.comm	__junk,4

		.text

		.extern appletmain
		.globl __start

__start:

	/*
	 * Set up our GP (in case we're using it)
	 * Set up the stack pointer (don't use CFE's stack)
	 */

		la	gp,_gp
		la	sp,stack_bottom+STACK_SIZE-32

	/*
	 * Zero BSS.  No need to do this very efficiently, do it
	 * 32 bits at a time to ensure we can compile this
	 * with -mips1.
	 */

		la	t0,_fbss
		la	t1,_end

1:		sw	zero,0(t0)
		add	t0,4
		blt	t0,t1,1b

	/*
	 * Jump to main program.  Note that we didn't trash A0..A3,
	 * our application will need them.  So, if you add stuff here,
	 * be sure to preserve the A0..A3 registers for the C code.
	 */

		j	appletmain
