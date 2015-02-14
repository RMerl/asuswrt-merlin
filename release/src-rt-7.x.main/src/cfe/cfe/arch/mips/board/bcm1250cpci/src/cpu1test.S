/*  *********************************************************************
    *  BCM12500 Board Support Package
    *  
    *  CPU1 test routines			File: CPU1TEST.S
    *
    *  This file gives CPU1 something to do.
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
#include "bsp_config.h"
#include "bcm1250cpci.h"

		.text



#define SETLEDS(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       bal    cpu1_setleds


		.text


/*  *********************************************************************
    *  BOARD_SETLEDS(x)
    *  
    *  Set LEDs for boot-time progress indication.  Not used if
    *  the board does not have progress LEDs.  This routine
    *  must not call any other routines, since it may be invoked
    *  either from KSEG0 or KSEG1 and it may be invoked 
    *  whether or not the icache is operational.
    *  
    *  Input parameters: 
    *  	   a0 - LED value (8 bits per character, 4 characters)
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3
    ********************************************************************* */


#define LED_CHAR0	(32+8*3)
#define LED_CHAR1	(32+8*2)
#define LED_CHAR2	(32+8*1)
#define LED_CHAR3	(32+8*0)

LEAF(cpu1_setleds)

		li	t0,PHYS_TO_K1(LEDS_PHYS)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED_CHAR0(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED_CHAR1(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED_CHAR2(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED_CHAR3(t0)

		j	ra

END(cpu1_setleds)


/*  *********************************************************************
    *  CPU1PROC()
    *  
    *  This routine is started on the secondary processor.  It just
    *  makes some noise on the LEDs.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

#ifdef _FUNCSIM_
#define WAITCOUNT 100000
#else
#define WAITCOUNT 100000000
#endif

LEAF(cpu1proc)

		li	s0,0x80001000
		li	s1,0
		sd	s1,0(s0)

loop:

		SETLEDS('W','o','r','k')

		ld      t0,0(s0)
		bne	t0,s1,fail
		add	t0,1
		add	s1,1
		sd      t0,0(s0)

		mtc0	zero,C0_COUNT
1:		li	t0,WAITCOUNT
		mfc0	t1,C0_COUNT
		blt	t1,t0,1b

		SETLEDS('i','n','g','!')

		ld      t0,0(s0)
		bne	t0,s1,fail
		add	t0,1
		add	s1,1
		sd      t0,0(s0)


		mtc0	zero,C0_COUNT
1:		li	t0,WAITCOUNT
		mfc0	t1,C0_COUNT
		blt	t1,t0,1b

		b	loop

fail:
		SETLEDS('F','a','i','l')
1:		b	1b


END(cpu1proc)
