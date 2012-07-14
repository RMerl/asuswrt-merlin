/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: BCM91125C_INIT.S
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


#include "sbmips.h"
#include "sb1250_genbus.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "sb1250_mc.h"
#include "sb1250_mac.h"
#include "bsp_config.h"
#include "bcm91125c.h"
#include "mipsmacros.h"
#include "sb1250_draminit.h"

		.text

#if defined(_BCM91125C_DIAG_CFG_)
#define _SERIAL_PORT_LEDS_
#endif

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#ifdef _SERIAL_PORT_LEDS_
#define BOARD_PROMICE_BASE	(0x1FDFFC00)
#define BOARD_PROMICE_ZERO	(0)
#define BOARD_PROMICE_ONE	(1)
#define BOARD_PROMICE_DATA	(2)
#define BOARD_PROMICE_STATUS	(3)

#define TDA 0x01 	/* Target data available */
#define HDA 0x02 	/* Host data available */
#define OVR 0x04 	/* Host data overflow */
#endif


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

LEAF(board_earlyinit)

       #
       # Configure the GPIOs
       #

		li	t0,PHYS_TO_K1(A_GPIO_DIRECTION)
		li	t1,GPIO_OUTPUT_MASK
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_INT_TYPE)
		li	t1,GPIO_INTERRUPT_MASK
		sd	t1,0(t0)

       #
       # Set both loopback pins to "disable"
       #
		li	t0,PHYS_TO_K1(A_GPIO_PIN_CLR)
		li	t1,M_GPIO_SERIAL0_LOOPBACK | M_GPIO_SERIAL1_LOOPBACK
		sd	t1,0(t0)


       #
       # Configure the LEDs
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(LEDS_CS))
		li	t1,LEDS_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,LEDS_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,LEDS_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,LEDS_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,LEDS_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)



       #
       # Configure the alternate boot ROM
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(ALT_BOOTROM_CS))

		li	t1,ALT_BOOTROM_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,ALT_BOOTROM_SIZE-1
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,ALT_BOOTROM_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,ALT_BOOTROM_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,ALT_BOOTROM_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)




       #
       # Configure the Big Flash
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(BIG_FLASH_CS))

		li	t1,BIG_FLASH_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,BIG_FLASH_SIZE-1
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,BIG_FLASH_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,BIG_FLASH_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,BIG_FLASH_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)

	/*
	 * If the CPU is a 1250 or hybrid, certain initialization has
	 * to be done so that the chip can be used like an 112x.
	 */

	/* First, figure out what type of SOC we're on. */
	ld	t1, PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
	SYS_SOC_TYPE(t3, t1)
	bne	t3, K_SYS_SOC_TYPE_BCM1250, is_bcm112x

	/*
	 * We have a 1250 or hybrid.  Initialize registers as appropriate.
	 */

	/*
	 * If we're not already running as a uniprocessor, get us there.
	 */
	dsrl    t1, t1, S_SYS_PART      # part number now in t1
	and	t3, t1, 0xf00
	dsrl	t3, t3, 8		# t3 = numcpus

	ld	t4, PHYS_TO_K1(A_SCD_SYSTEM_CFG)
	or	t4, t4, M_SYS_SB_SOFTRES
	xor	t4, t4, M_SYS_SB_SOFTRES
	sd	t4, PHYS_TO_K1(A_SCD_SYSTEM_CFG)	/* clear soft reset */

	beq	t3, 1, 2f

	or	t4, t4, M_SYS_SB_SOFTRES | M_SYS_UNICPU0
	sd	t4, PHYS_TO_K1(A_SCD_SYSTEM_CFG)	/* go unicpu */
	sync
1:
	b	1b
2:
	
	/*
	 * Clean up MC 0.
	 */

	li	t0, PHYS_TO_K1(A_MC_BASE_0)
	dli	t1, V_MC_CONFIG_DEFAULT | M_MC_ECC_DISABLE | \
		    V_MC_CS_MODE_MSB_CS
	sd	t1, R_MC_CONFIG(t0)
	sd	zero, R_MC_CS_START(t0)
	sd	zero, R_MC_CS_END(t0)
	sd	zero, R_MC_CS_INTERLEAVE(t0)
	sd	zero, R_MC_CS_ATTR(t0)
	sd	zero, R_MC_TEST_DATA(t0)
	sd	zero, R_MC_TEST_ECC(t0)

	/*
	 * Zero out MAC 2's address register.  (This has
	 * undefined value after reset, but OSes may check
	 * it on some parts to see if they should init
	 * the interface.  This is a convenient place
	 * to zero it.)
	 */

	li	t0, PHYS_TO_K1(A_MAC_BASE_2)
	sd	zero, R_MAC_ETHERNET_ADDR(t0)

is_bcm112x:

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

		move	t0,ra

		LOADREL(v0,myinfo)

		move	ra,t0
		j	ra

myinfo:
	/*
	 * Globals: Only one memory controller, no port interleaving
	 */
	DRAM_GLOBALS(0)

        DRAM_CHAN_CFG(MC_CHAN1, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)

	DRAM_CS_SPD(MC_CS0, 0, 0, 0x54)
        DRAM_CS_SPD(MC_CS2, 0, 0, 0x55)

	DRAM_EOT

END(board_draminfo)


/*  *********************************************************************
    *  BOARD_PIAI2_TXCHAR
    *  
    *  Transmit a single character via UART A
    *  
    *  Input parameters: 
    *  	   a0 - character to transmit (low-order 8 bits)
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */

#ifdef _SERIAL_PORT_LEDS_

LEAF(board_piai2_txchar)

	# Wait until there is space in the transmit buffer

	        li      t0,PHYS_TO_K1(BOARD_PROMICE_BASE)

1:		lb	t1,BOARD_PROMICE_STATUS(t0)
		andi	t1,TDA
		bne	t1,zero,1b

	# Okay, now send the character.

		sb	a0,BOARD_PROMICE_ZERO(t0)

	# done!

		j	ra

END(board_piai2_txchar)
#endif

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

LEAF(board_setleds)

#ifdef _SERIAL_PORT_LEDS_
		move	t3,ra
		move	t2,a0

		li	a0,'['
		bal	board_piai2_txchar

		move	a0,t2
		rol	a0,8
		li	t0,PHYS_TO_K1(LEDS_PHYS)
		sb	a0,LED_CHAR0(t0)
		bal	board_piai2_txchar

		rol	a0,8
		li	t0,PHYS_TO_K1(LEDS_PHYS)
		sb	a0,LED_CHAR1(t0)
		bal	board_piai2_txchar

		rol	a0,8
		li	t0,PHYS_TO_K1(LEDS_PHYS)
		sb	a0,LED_CHAR2(t0)
		bal	board_piai2_txchar

		rol	a0,8
		li	t0,PHYS_TO_K1(LEDS_PHYS)
		sb	a0,LED_CHAR3(t0)
		bal	board_piai2_txchar

		li	a0,']'
		bal	board_piai2_txchar
		li	a0,13
		bal	board_piai2_txchar
		li	a0,10
		bal	board_piai2_txchar

		move	ra,t3
#else

	/*
	 * Sending to LEDs
	 */
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

#endif
		j	ra

END(board_setleds)
