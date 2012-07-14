/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: CARMEL_INIT.S
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
#include "carmel.h"
#include "mipsmacros.h"
#include "sb1250_draminit.h"
#include "sb1250_smbus.h"

		.text


/*  *********************************************************************
    *  Macros
    ********************************************************************* */
/*
 * Define this to send the LED messages to the serial port instead
 * of to the LEDs.
 */

/*#define _SERIAL_PORT_LEDS_*/

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
       # zero the system scratch register.
       #

		li	t0,PHYS_TO_K1(A_SCD_SCRATCH)
		sd	zero,0(t0)


       #
       # Configure the GPIOs
       #

		li	t0,PHYS_TO_K1(A_GPIO_PIN_CLR)
		li	t1,0xFFFF			/* clear all pins */
		sd	t1,0(t0)

       #
       # Bring Monterey out of reset, set FPGA pins to some known state.
       #

		li	t0,PHYS_TO_K1(A_GPIO_PIN_SET)
		li	t1,(M_GPIO_MONTEREY_RESET | M_GPIO_FPGA_INIT | M_GPIO_FPGA_PGM | \
                            M_GPIO_FPGA_DIN | M_GPIO_FPGA_CCLK)
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_DIRECTION)
		li	t1,GPIO_OUTPUT_MASK
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_INPUT_INVERT)
		li	t1,GPIO_INVERT_MASK
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_INT_TYPE)
		li	t1,GPIO_INTERRUPT_MASK
		sd	t1,0(t0)

       #
       # Configure the 7-Seg Display
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
       # Configure the 4-character Display (on Monterey)
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(MLEDS_CS))
		li	t1,MLEDS_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,MLEDS_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,MLEDS_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,MLEDS_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,MLEDS_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


       #
       # Configure the quad UART
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(UART_CS))
		li	t1,UART_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,UART_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,UART_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,UART_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,UART_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


       #
       # Configure the IDE / CompactFlash
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(IDE_CS))
		li	t1,IDE_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,IDE_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,IDE_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,IDE_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,IDE_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)



       #
       # Configure the Aravali/FPGA
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(ARAVALI_CS))
		li	t1,ARAVALI_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,ARAVALI_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,ARAVALI_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,ARAVALI_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,ARAVALI_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)



       #
       # Configure the Aravali/FPGA (JTAG region)
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(ARAVALI2_CS))
		li	t1,ARAVALI2_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,ARAVALI2_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,ARAVALI2_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,ARAVALI2_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,ARAVALI2_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


       #
       # Configure the ADC
       #     

		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(ADC_CS))
		li	t1,ADC_PHYS >> S_IO_ADDRBASE
		sd	t1,R_IO_EXT_START_ADDR(t0)

		li	t1,ADC_SIZE-1	/* Needs to be 1 smaller, se UM for details */
		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		li	t1,ADC_TIMING0
		sd	t1,R_IO_EXT_TIME_CFG0(t0)

		li	t1,ADC_TIMING1
		sd	t1,R_IO_EXT_TIME_CFG1(t0)

		li	t1,ADC_CONFIG
		sd	t1,R_IO_EXT_CFG(t0)


	/*
	 * If the CPU is a 1250 or hybrid, certain initialization has
	 * to be done so that the chip can be used like an 112x.
	 */

	/* First, figure out what type of SOC we're on. */
	ld	t0, PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
	and	t1, t0, M_SYS_PART
	dsrl	t1, t1, S_SYS_PART	# part number now in t1
	and	t2, t0, M_SYS_REVISION
	dsrl	t2, t2, S_SYS_REVISION	# revision now in t2

	/*
	 * Calculating SOC type:
	 *	soc_type = part & 0xf;
	 *	if (system_revision <= PASS2
	 *	    && (soc_type == 2 || soc_type == 5))
	 *	  soc_type = 0;			# really a 1250.
	 */
	andi	t3, t1, 0xf			# soc_type (t3) = part & 0xf;
	beq	t3, 2, soc_type_bogus
	beq	t3, 5, soc_type_bogus
	b	soc_type_ok

soc_type_bogus:
        move    t3, zero

soc_type_ok:
	bne	t3, 0, is_bcm112x /* if not SOC type 0 (1250), assume OK. */

	/*
	 * We have a 1250 or hybrid.  Initialize registers as appropriate.
	 */


	/*
	 * If we're not already running as a uniprocessor, get us there.
	 */
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
		li	t1,V_DUART_BAUD_RATE(115200)
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
    *  Return the address of the DRAM information table.
    *  Check if SPD contains valid parameters.  If so, use it.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   v0 - DRAM info table, return 0 to use default table
    ********************************************************************* */

#define smb_cmd      t9
#define smb_start    t8
#define smb_status   t7
#define smb_data     t6
#define ra_saved     t5
#define spd_byte     t4
#define SMBUS_0_READBYTE(slvaddr,devaddr,result)			\
1:	ld	t1,(smb_status)	;					\
	and	t1,M_SMB_BUSY ;						\
	bnez	t1,1b ;							\
	li	t1,devaddr ;						\
	sd	t1,(smb_cmd) ;						\
	li	t1,(V_SMB_TT(K_SMB_TT_WR1BYTE) | slvaddr) ;		\
	sd	t1,(smb_start) ;					\
1:	ld	t1,(smb_status) ;					\
	and	t1,M_SMB_BUSY ;						\
	bnez	t1,1b ;							\
	li	t1,(V_SMB_TT(K_SMB_TT_RD1BYTE) | slvaddr) ;		\
	sd	t1,(smb_start) ;					\
1:	ld	t1,(smb_status) ;					\
	and	t1,M_SMB_BUSY ;						\
	bnez	t1,1b ;							\
	ld	t1,(smb_data) ;						\
	and	t1,0xFF ;						\
	move	result,t1

LEAF(board_draminfo)

       /*
	* Check a few bytes to make sure SPD EEPROM contains valid info.  If it does, use it.
	* if not, use manual tables.  The ATMEL EEPROM is @ chan 0, addr 0x54.
        */

	        li	smb_cmd,PHYS_TO_K1(A_SMB_CMD_0)
		li	smb_start,PHYS_TO_K1(A_SMB_START_0)
		li	smb_status,PHYS_TO_K1(A_SMB_STATUS_0)
		li	smb_data,PHYS_TO_K1(A_SMB_DATA_0)
		move	ra_saved,ra	/* Save ra since LOADREL below will thrash it*/

	# Initialize smbus chan 0
	        li	t0,PHYS_TO_K1(A_SMB_FREQ_0)
		li	t1,K_SMB_FREQ_100KHZ
		sd	t1,(t0)

	        li	t0,PHYS_TO_K1(A_SMB_CONTROL_0)
		sd	zero,(t0)

	# Check memory type @ byte 2.  0x07(SDRAM DDR)
		SMBUS_0_READBYTE(SPDEEPROM_SMBUS_DEV,JEDEC_SPD_MEMTYPE,spd_byte)
		li	t2,0x07 /*JEDEC SDRAM memory type*/
		bne	spd_byte,t2,man
	
	# Check rows @ byte 3.  0x0D(13)
		SMBUS_0_READBYTE(SPDEEPROM_SMBUS_DEV,JEDEC_SPD_ROWS,spd_byte)
		li	t2,0x0D  /*There should be 13 rows*/
		bne	spd_byte,t2,man

	# Check columns @ byte 4.  0x09(9)
		SMBUS_0_READBYTE(SPDEEPROM_SMBUS_DEV,JEDEC_SPD_COLS,spd_byte)
		li	t2,0x09  /*There should be 9 columns*/
		bne	spd_byte,t2,man

	# Check refresh rate/type @ byte 12. 0x82 -> 128 KHZ		   		
		SMBUS_0_READBYTE(SPDEEPROM_SMBUS_DEV,JEDEC_SPD_RFSH,spd_byte)
		li	t2,0x82  /*Refresh rate should be set for 128 KHZ*/
		bne	spd_byte,t2,man
    			
	# All checks passed.  Use SPD to configure 
		LOADREL(v0,spd)
		b	done		

man:		LOADREL(v0,man_table)

done:		move	ra,ra_saved
		j	ra

spd:

	/*
	 * Globals: Only one memory controller, no port interleaving
	 */
	DRAM_GLOBALS(0)
        DRAM_CHAN_CFG(MC_CHAN1, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)
	DRAM_CS_SPD(MC_CS0, 0, 0, 0x54)
	DRAM_EOT

#if !defined(_CARMEL_STAKTEK_)

	/* Non-stacked DRAM */

man_table:

	/*
	 * Globals: Only one memory controller, no port interleaving
	 */
	DRAM_GLOBALS(0)

	/* 
	 * 128MB on MC 1 (JEDEC SDRAM) 
	 * Samsung K4H561638B - 16Mx16 chips
	 *
	 * Minimum tMEMCLK: 8.0ns (125Mhz max freq)
	 *
	 * CS0 Geometry: 13 rows, 9 columns, 2 bankbits
	 *
	 * 128khz refresh, CAS Latency 2.5
	 * Timing (ns):   tCK=7.50 tRAS=45 tRP=20.00 tRRD=15.0 tRCD=20.0 tRFC=auto tRC=auto
	 *
	 * Clock Config: Default
	 */

        DRAM_CHAN_CFG(MC_CHAN1, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)

	DRAM_CS_GEOM(MC_CS0, 13, 9, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_128khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)

	DRAM_EOT

#else

	/* Stacked DRAM */

man_table:

	/*
	 * Globals: Only one memory controller, no port interleaving
	 */
	DRAM_GLOBALS(0)

	/* 
	 * 256MB on MC 1 (JEDEC SDRAM) 
	 * Samsung K4H561638B - 16Mx16 chips
	 *
	 * Minimum tMEMCLK: 8.0ns (125Mhz max freq)
	 *
	 * CS0 Geometry: 13 rows, 9 columns, 2 bankbits
	 *
	 * 128khz refresh, CAS Latency 2.5
	 * Timing (ns):   tCK=7.50 tRAS=45 tRP=20.00 tRRD=15.0 tRCD=20.0 tRFC=auto tRC=auto
	 *
	 * Clock Config: Default
	 */

        DRAM_CHAN_CFG(MC_CHAN1, DRT10(8,0), JEDEC, CASCHECK, BLKSIZE32, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)

	DRAM_CS_GEOM(MC_CS0, 13, 9, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_128khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)
	DRAM_CS_GEOM(MC_CS1, 13, 9, 2)
	DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_128khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0)

	DRAM_EOT

#endif

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

1:		li     	t0,PHYS_TO_K1(A_DUART_STATUS_A)
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

#define LED8_CHAR0	(0x38+0)
#define LED8_CHAR1	(0x38+1)
#define LED8_CHAR2	(0x38+2)
#define LED8_CHAR3	(0x38+3)

LEAF(board_setleds)


	/*
	 * Write some data and then
	 * try to read back what we wrote.   Only the
	 * 8-char LED has readback capability.
	 */

		li	t0,PHYS_TO_K1(MLEDS_PHYS)

		li	t1,0x20
		sb	t1,LED8_CHAR0(t0)
		li	t1,0x5F
		sb	t1,LED8_CHAR1(t0)

		lb	t1,LED8_CHAR0(t0)
		bne	t1,0x20,1f

		lb	t1,LED8_CHAR1(t0)
		bne	t1,0x5F,1f

	/*
	 * Sending to LEDs (8-character style)
	 */


		li	t0,PHYS_TO_K1(MLEDS_PHYS)

		move	t3,a0

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED8_CHAR0(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED8_CHAR1(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED8_CHAR2(t0)

		rol	a0,a0,8
		and	t1,a0,0xFF
		sb	t1,LED8_CHAR3(t0)

		b	2f		/* go to seial leds */

	/*
	 * Sending to LEDs (4-character style)
	 */

1:		li	t0,PHYS_TO_K1(MLEDS_PHYS)

		move	t3,a0

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

2:		move	a0,t3

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
		li	a0,13
		bal	board_uarta_txchar
		li	a0,10
		bal	board_uarta_txchar

		move	ra,t3
#endif

		j	ra

END(board_setleds)

/*  *********************************************************************
    *  BOARD_SETDISPAY(x)
    *  
    *  Set 7-Seg display.
    *
    *  Input parameters: 
    *  	   a0 - display value (8 bits)
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t1
    ********************************************************************* */


LEAF(board_setdisplay)

	and	t1,a0,0xFF
	sb	t1,PHYS_TO_K1(LEDS_PHYS)

	j	ra

END(board_setdisplay)
