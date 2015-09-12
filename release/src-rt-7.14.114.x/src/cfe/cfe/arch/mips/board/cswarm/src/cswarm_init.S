/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: CSWARM_INIT.S
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
#include "bsp_config.h"
#include "cswarm.h"
#include "sb1250_draminit.h"

		.text

#if (defined(_CSWARM_DIAG3E_CFG_) || defined(_CSWARM_DIAG_CFG_))
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
	# Reprogram the SCD to make sure UART0 is enabled.
	# Some CSWARM boards have the SER0 enable bit when
	# they're not supposed to, which switches the UART
	# into synchronous mode.  Kill off the SCD bit.
	# XXX this should be investigated in hardware, as 
	# XXX it is a strap option on the CPU.
	#

		li      t0,PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t1,0(t0)
		dli	t2,~M_SYS_SER0_ENABLE
		and	t1,t1,t2
		sd	t1,0(t0)

       #
       # Set the MAC_HASH_BASE register of MAC2 to zero.
       # For pass2 screening we use this register to store
       # cache diagnostics.  In post-A3 screening
       # we don't run the cache test anymore, so 
       # put a sane value in just in case we don't
       # run the test.
       #

		li	t0,PHYS_TO_K1(A_MAC_REGISTER(2,R_MAC_HASH_BASE))
		sd	zero,0(t0)

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
       # Turn on the diagnostic LED and turn off the sturgeon NMI
       #
		li	t0,PHYS_TO_K1(A_GPIO_PIN_SET)
		li	t1,M_GPIO_DEBUG_LED
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_PIN_CLR)
		li	t1,M_GPIO_STURGEON_NMI
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
       # Configure the IDE interface
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(IDE_CS))

		li	t1,IDE_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,IDE_SIZE-1
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,IDE_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,IDE_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,IDE_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


       #
       # Configure the PCMCIA
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(PCMCIA_CS))

		li	t1,PCMCIA_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,PCMCIA_SIZE-1
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,PCMCIA_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,PCMCIA_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,PCMCIA_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)

#ifdef _SERIAL_PORT_LEDS_

       /*
        * Initialize the promice port.  read status until
	* it does not read 0xCC, then do a dummy read from the
	* data register.
	*/

		li	t1,PHYS_TO_K1(BOARD_PROMICE_BASE)

		li	t2,10000
1:		sub	t2,1
		beq	t2,zero,2f
		lbu	t0,BOARD_PROMICE_STATUS(t1)
		beq	t0,0xCC,1b

2:		lbu	t0,BOARD_PROMICE_DATA(t1)

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

#if CFG_EMBEDDED_PIC
#define LOADREL(reg,label)			\
	.set noreorder ;			\
	bal  1f	       ;			\
	nop	       ;			\
1:	nop	       ;			\
	.set reorder   ;			\
	la   reg,label-1b ;			\
	add  reg,ra
#else
#define	LOADREL(reg,label)			\
	la   reg,label
#endif

LEAF(board_draminfo)

		move	t0,ra

#ifdef _HARDWIRED_MEMORY_TABLE
		LOADREL(v0,myinfo)
#else
		move	v0,zero		# auto configure
#endif

		move	ra,t0
		j	ra



myinfo:
	DRAM_GLOBALS(CFG_DRAM_INTERLEAVE)
	

	/* 128MB on MC0 (SDRAM) */ 
        DRAM_CHAN_CFG(MC_CHAN0, DRT10(8,0), JEDEC,CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)
	DRAM_CS_GEOM(MC_CS0, 12, 8, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_64khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)
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
