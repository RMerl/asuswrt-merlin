/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: BIGSUR_INIT.S
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
#include "bcm1400_genbus.h"
#include "bcm1400_regs.h"
#include "bcm1400_scd.h"
#include "bsp_config.h"
#include "mipsmacros.h"
#include "bigsur.h"
#include "bcm1400_draminit.h"

		.text



/*  *********************************************************************
    *  Macros
    ********************************************************************* */

/* #define _SERIAL_PORT_LEDS_ */

#ifdef _SERIAL_PORT_LEDS_
#include "bcm1400_uart.h"
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
       # Turn off the diagnostic LED.
       #
		li	t0,PHYS_TO_K1(A_GPIO_PIN_CLR)
		li	t1,M_GPIO_DEBUG_LED
		sd	t1,0(t0)


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
	# Make sure that the mailbox register is cleared
	#

		li	t0,PHYS_TO_K1(A_IMR_REGISTER(0,R_IMR_MAILBOX_CLR_CPU))
		nor	t1, zero, zero
		sd	t1,(t0)
	
#ifdef _SERIAL_PORT_LEDS_

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

	#
	# This board has soldered-down memory. (?)
	#

		move	t0,ra

		LOADREL(v0,dramtab)

		move	ra,t0
		j	ra


	/*
	 * Regular DRAM for regular Bigsurs
	 */

dramtab:

	/*
	 * DRAM globals: interleave OK
	 */

	DRAM_GLOBALS(CFG_DRAM_INTERLEAVE)	 
	

	/* 
	 * 128MB on MC 0 (JEDEC SDRAM) 
	 * Samsung K4H561638B - 16Mx16 chips
	 *
	 * Minimum tMEMCLK: 8.0ns (125Mhz max freq)
	 *
	 * CS0 Geometry: 13 rows, 9 columns, 2 bankbits
	 *
	 * 64khz refresh, CAS Latency 2.5
	 * Timing (ns):   tCK=7.50 tRAS=45 tRP=20.00 tRRD=15.0 tRCD=20.0 tRFC=auto tRC=auto
	 *
	 * Clock Config: Default
	 */

        DRAM_CHAN_CFG(MC_CHAN0, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)
	DRAM_CS_GEOM(MC_CS0, 13, 9, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_64khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)


	/* 
	 * 128MB on MC 1 (JEDEC SDRAM)
	 * Samsung K4H561638B - 16Mx16 chips
	 *
	 * Minimum tMEMCLK: 8.0ns (125Mhz max freq)
	 *
	 * CS0 Geometry: 13 rows, 9 columns, 2 bankbits
	 *
	 * 64khz refresh, CAS Latency 2.5
	 * Timing (ns):   tCK=7.50 tRAS=45 tRP=20.00 tRRD=15.0 tRCD=20.0 tRFC=auto tRC=auto
	 *
	 * Clock Config: Default
	 */

        DRAM_CHAN_CFG(MC_CHAN1, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)
	DRAM_CS_GEOM(MC_CS0, 13, 9, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_64khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)

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


LEAF(board_setleds)

#ifdef _SERIAL_PORT_LEDS_
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

#endif

		j	ra

END(board_setleds)
