/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: LAUSANNE_INIT.S
    *
    *  This module contains the assembly-language part of the init
    *  code for this board support package.  The routine
    *  "board_earlyinit" lives here.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  Lausanne edits:	Jeffrey Cheng (chengj@broadcom.com)
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
#include "lausanne.h"
#include "mipsmacros.h"
#include "sb1250_draminit.h"

		.text


/*  *********************************************************************
    *  Macros
    ********************************************************************* */


/*
 * Define this to send the LED messages to the serial port instead
 * of to the LEDs.
 */

/* #define _SERIAL_PORT_LEDS_ */

#ifdef _SERIAL_PORT_LEDS_
#include "sb1250_uart.h"		/* need this for serial defs */
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
       # Configure the GPIOs
       #

		li	t0,PHYS_TO_K1(A_GPIO_DIRECTION)
		li	t1,GPIO_OUTPUT_MASK
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_INT_TYPE)
		li	t1,GPIO_INTERRUPT_MASK
		sd	t1,0(t0)

/* removed sturgeon and test led clear and set for lausanne */
	
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
       # Configure the CPLD interface
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(CPLD_CS))

		li	t1,CPLD_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,CPLD_SIZE-1
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,CPLD_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,CPLD_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,CPLD_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


 
#ifdef _SERIAL_PORT_LEDS_

       /*
        * If sending the LED messages to the serial port, we will need
	* to initialize the port now.
	*/

	# Program the mode register for 8 bits/char, no parity

		li	t0,PHYS_TO_K1(A_DUART_MODE_REG_1_A)	
		li	t1,V_DUART_BITS_PER_CHAR_8 | V_DUART_PARITY_MODE_NONE
		sd	t1,(t0)

	# Program the mode register for 1 stop bit, ignore CTS

		li	t0,PHYS_TO_K1(A_DUART_MODE_REG_2_A)	
		li	t1,M_DUART_STOP_BIT_LEN_1
		sd	t1,(t0)

	# Program the baud rate to 115200

		li	t0,PHYS_TO_K1(A_DUART_CLK_SEL_A)
		li	t1,V_DUART_BAUD_RATE(CFG_SERIAL_BAUD_RATE)
		sd	t1,(t0)

	# Dont use any interrupts

		li	t0,PHYS_TO_K1(A_DUART_IMR)
		ld	t1,(t0)
		and	t1,~M_DUART_IMR_ALL_A
		sd	t1,(t0)

	# Enable sending and receiving

		li	t0,PHYS_TO_K1(A_DUART_CMD_A)
		li	t1,M_DUART_RX_EN | M_DUART_TX_EN
		sd	t1,(t0)

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

		move	t0,ra

		LOADREL(v0,myinfo)

		move	ra,t0
		j	ra

myinfo:


    /*
     * Global data: Interleave mode from bsp_config.h
     */

    DRAM_GLOBALS(CFG_DRAM_INTERLEAVE)	   		/* do port interleaving if possible */

    /*
     * Memory channel 0: Configure via SMBUS, Automatic Timing, Big Memory mode, Force Register
     *
     * There's an external register on the board, so dimms appear to be "registered" even though
     * the SPD says they're not.
     *
     * SPD SMBus Channel 0 Device 0x50	-> MC0 slot 0  \___ goes to real MC0 CS0 via external decode	
     * SPD SMBus Channel 0 Device 0x51	-> MC0 slot 1  /
     * SPD SMBus Channel 0 Device 0x52	-> MC0 slot 2  \___ goes to real MC0 CS1 via external decode
     * SPD SMBus Channel 0 Device 0x53	-> MC0 slot 3  /
     *
     * DRAM must always be added in pairs!
     */

    DRAM_CHAN_CFG(MC_CHAN0, CFG_DRAM_MIN_tMEMCLK, JEDEC, CASCHECK, CFG_DRAM_BLOCK_SIZE, NOCSINTLV, CFG_DRAM_ECC, (MCFLG_BIGMEM | MCFLG_FORCEREG))
    DRAM_CHAN_CLKCFG(0x0F,0x08,0x08,0x00,0x0F,0x00)

    DRAM_CS_SPD(MC_CS0, 0, 0, 0x50)
    DRAM_CS_SPD(MC_CS1, 0, 0, 0x52)

    /*
     * Memory channel 1: Configure via SMBUS, Automatic Timing, Big Memory mode, Force Register
     *
     * There's an external register on the board, so dimms appear to be "registered" even though
     * the SPD says they're not.
     *
     * SPD SMBus Channel 0 Device 0x54	-> MC1 slot 0  \___ goes to real MC1 CS0 via external decode	
     * SPD SMBus Channel 0 Device 0x55	-> MC1 slot 1  /
     * SPD SMBus Channel 0 Device 0x56	-> MC1 slot 2  \___ goes to real MC1 CS1 via external decode
     * SPD SMBus Channel 0 Device 0x57	-> MC1 slot 3  /
     *
     * DRAM must always be added in pairs!
     */

    DRAM_CHAN_CFG(MC_CHAN1, CFG_DRAM_MIN_tMEMCLK, JEDEC, CASCHECK, CFG_DRAM_BLOCK_SIZE, NOCSINTLV, CFG_DRAM_ECC, (MCFLG_BIGMEM | MCFLG_FORCEREG))
    DRAM_CHAN_CLKCFG(0x0F,0x08,0x08,0x00,0x0F,0x00)

    DRAM_CS_SPD(MC_CS0, 0, 0, 0x54)
    DRAM_CS_SPD(MC_CS1, 0, 0, 0x56)

   /*
    * End of Table
    */

    DRAM_EOT



END(board_draminfo)


/*  *********************************************************************
    *  BOARD_UARTA_TXCHAR
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
LEAF(board_uarta_txchar)

	# Wait until there is space in the transmit buffer

1:		li	t0,PHYS_TO_K1(A_DUART_STATUS_A)
		ld	t1,(t0)			# Get status bits
		and	t1,M_DUART_TX_RDY	# test for ready
		beq	t1,0,1b			# keep going till ready

	# Okay, now send the character.

		li	t0,PHYS_TO_K1(A_DUART_TX_HOLD_A)
		sd	a0,(t0)

	# done!

		j	ra

END(board_uarta_txchar)
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

       /*
        * Sending to serial port
	*/
		move	t3,ra
		move	t2,a0

		li	a0,'['
		bal	board_uarta_txchar

		move	a0,t2
		rol	a0,8
		bal	board_uarta_txchar
		rol	a0,8
		bal	board_uarta_txchar
		rol	a0,8
		bal	board_uarta_txchar
		rol	a0,8
		bal	board_uarta_txchar

		li	a0,']'
		bal	board_uarta_txchar

		move	ra,t3
		j	ra

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

		j	ra
#endif

END(board_setleds)
