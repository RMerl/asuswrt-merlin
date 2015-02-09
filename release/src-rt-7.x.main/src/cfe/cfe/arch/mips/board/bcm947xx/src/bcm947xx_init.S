/*  *********************************************************************
    *  BCM947XX Board Support Package
    *
    *  Board-specific initialization		File: BCM947XX_INIT.S
    *
    *  This module contains the assembly-language part of the init
    *  code for this board support package.  The routine
    *  "board_earlyinit" lives here.
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

#include "hndsoc.h"
#include "bcmdevs.h"
#include "sbchipc.h"
#include "sbmips.h"
#include "bsp_config.h"
#include "mipsmacros.h"

	.text


/*  *********************************************************************
    *  BOARD_EARLYINIT()
    *
    *  Initialize board registers.  This is the earliest
    *  time the BSP gets control.  This routine cannot assume that
    *  memory is operational, and therefore all code in this routine
    *  must run from registers only.  The $ra register must not
    *  be modified, as it contains the return address.
    *
    *  This routine will be called from uncached space, before
    *  the caches are initialized.  If you want to make
    *  subroutine calls from here, you must use the CALLKSEG1 macro.
    *
    *  Among other things, this is where the GPIO registers get
    *  programmed to make on-board LEDs function, or other startup
    *  that has to be done before anything will work.
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

/*
 * Define TRADDR to be an (uncached) addres to store the "leds" for tracing
 * This for example would use chipcommon's flash address register:
 * #define TRADDR		0xb8000044
 */
#if	defined(BCMDBG)
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
#define TRADDR		0xb8000040
#else
#define TRADDR		0xb8000044
#endif
#endif

LEAF(board_earlyinit)

	# XXX:the following code snipet sets clk frequency to 200M
	# correct pll clk freq to real speed in the 5350 case.
	# It is Ugly...but

	li	a2,PHYS_TO_K1(SI_ENUM_BASE)	# Point to chipcommon core
	li	a3,BCM5350_CHIP_ID		# 5350 ChipID
	lw	t1,CC_CHIPID(a2)		# ChipID register
	li	t2,CID_ID_MASK			# chip id is bit 0-15
	and	t2,t1,t2
	bne	t2,a3,2f			# if not 5350 then skip
	nop

	li	t2,CID_PKG_MASK			# if it is a vsim 5350, also skip
	and	t2,t1,t2
	li	a3,(HDLSIM5350_PKG_ID << CID_PKG_SHIFT)
	beq	t2,a3,4f			# if pkg opt 1 then skip
	nop

	li	a3,CLKC_5350_N
	lw	t3,CC_CLKC_N(a2)
	beq	a3,t3,4f			# move ahead if clk freq set correctly
	nop
	sw	a3,CC_CLKC_N(a2)		# set control N1 to select 6
	li	t3,1
	sw	t3,CC_WATCHDOG(a2)		# set WatchDog Reset
1:	b	1b
	nop

2:	li	a3,BCM5354_CHIP_ID		# 5354 ChipID
	bne	t2,a3,4f			# if not 5354 then skip
	nop
	li	t2,CID_REV_MASK			# Get chip rev
	and	t2,t1,t2
	li	t3,(3 << CID_REV_SHIFT)		# Is it an a3?
	blt	t2,t3,a0a1a2
	nop 

	/* Fix up for a3 (and up?) */
	li	a0,0x01330000			# Value for regcontrol 2
	li	t2,2
	sw	t2,PMU_REG_CONTROL_ADDR(a2)
	sw	a0,PMU_REG_CONTROL_DATA(a2)
	
a0a1a2:
	li	a0,0x00002000			# Value for regcontrol 0
	li	a1,0x06800000			# Value for regcontrol 1
	li	a3,0x02000000			# Value for regcontrol 3
	
fixregctl:
	li	t2,0x1				# Need define
	sw	t2,PMU_REG_CONTROL_ADDR(a2)
	sw	a1,PMU_REG_CONTROL_DATA(a2)
	
	/* 
	 * Trim the output voltage of the 1.2V BB switcher and 2.5V
	 * regulator to the correct value.
	 */
	li	t2,0x0
	sw	t2,PMU_REG_CONTROL_ADDR(a2)
	sw	a0,PMU_REG_CONTROL_DATA(a2)	# BB switcher to get 1.2V
	li	t2,0x3
	sw	t2,PMU_REG_CONTROL_ADDR(a2)
	sw	a3,PMU_REG_CONTROL_DATA(a2)	# of VDDP LDO to get 2.5V

	li	t2,0x1
	sw	t2,PMU_PLL_CONTROL_ADDR(a2)	# Check if PLL mode has been programmed
	li 	t3,0x66666602			# Expected PLL mode value
	lw	t2,PMU_PLL_CONTROL_DATA(a2)
	beq	t3,t2,3f
	nop

	li	t2,0x7ffff			# Should only turn off the PLL bit
	sw	t2,PMU_MIN_RES_MASK(a2)		# Disable base band PLL
	sw	t2,PMU_MAX_RES_MASK(a2)

	li	t2,0x1
	sw	t2,PMU_PLL_CONTROL_ADDR(a2)
	li	t2,0x66666602			# Set the PLL Mode properly
	sw	t2,PMU_PLL_CONTROL_DATA(a2)

	li	t1,0x1
	sw	t1,CC_WATCHDOG(a2)		# set WatchDog Reset
5:	b	5b
	nop

3:	li	t2,0xfffff			# Enable base band PLL 
	sw	t2,PMU_MIN_RES_MASK(a2)
	sw	t2,PMU_MAX_RES_MASK(a2)
	nop

	/* Init code for ff4 space without TLB, enabling RAC */
	li 	t0,0x1fa0000c			# Set up CBR to 0x1fax_xxxx
	mtc0 	t0,$22,6
	li 	t1,0x1fa00000
	lw 	t2,0x14(t1)
	or 	t3,t2,0xc0000000		# enable ffxx_xxxx space # without programming TLB
	sw 	t3,0x14(t1) 
	li 	t0,0xff40000c			# change CBR to ff4x_xxxx
	mtc0 	t0,$22,6

4:
#ifdef	TRADDR
	li	t0,TRADDR
	li	t1,0xaa55aa55
	sw	t1,0(t0)
#endif
	j	ra

END(board_earlyinit)

/*  *********************************************************************
    *  BOARD_DRAMINFO
    *
    *  Return the address of the DRAM information table
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   v0 - DRAM info table, return 0 to use default table
    ********************************************************************* */

LEAF(board_draminfo)

	move	v0,zero		# auto configure
	j	ra

END(board_draminfo)

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
    ********************************************************************* */

LEAF(board_setleds)

#ifdef	TRADDR
	li	t0,TRADDR
	sw	a0,0(t0)
#endif
	j	ra

END(board_setleds)
