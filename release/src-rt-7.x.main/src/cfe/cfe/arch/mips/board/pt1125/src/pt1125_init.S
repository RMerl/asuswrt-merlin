/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Board-specific initialization		File: PT1125_INIT.S
    *
    *  This module contains the assembly-language part of the init
    *  code for this board support package.  The routine
    *  "board_earlyinit" lives here.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *
    * modification history
    * --------------------
    * 01a,01aug02,gtb  Ported from  pt1120_init.S.
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001
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
    *     and retain this copyright notice and list of conditions as 
    *     they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation. Neither the "Broadcom 
    *     Corporation" name nor any trademark or logo of Broadcom 
    *     Corporation may be used to endorse or promote products 
    *     derived from this software without the prior written 
    *     permission of Broadcom Corporation.
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
#include "pt1125.h"
#include "sb1250_draminit.h"
#include "../dev/ns16550.h"

#if defined(_PT1125_DIAG_CFG_)
#undef SERIAL_PORT_LEDS
#define SERIAL_PORT_LEDS
#endif
		.text

/*  *********************************************************************
    *  Macros
    ********************************************************************* */


#if CFG_RUNFROMKSEG0
#define CALLKSEG1(x)  la k0,x ; or k0,K1BASE ; jal k0
#else
#define CALLKSEG1(x)  jal x
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
       # 0 is output
       # 1, 2, and 5 are interrupts
       # remainder are input (unused)
       #

		li	t0,PHYS_TO_K1(A_GPIO_DIRECTION)
		li	t1,GPIO_OUTPUT_MASK
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_GPIO_INT_TYPE)
		li	t1,GPIO_INTERRUPT_MASK
		sd	t1,0(t0)

       #
       # Turn on the diagnostic LED 
       #
		li	t0,PHYS_TO_K1(A_GPIO_PIN_SET)
		li	t1,M_GPIO_DEBUG_LED
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
       # Configure I/O bus for an external UART
       #     

                li      t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(UART_CS))

                li      t1, UART_PHYS >> S_IO_ADDRBASE
                sd      t1,R_IO_EXT_START_ADDR(t0)

                li      t1, (UART_SIZE-1)
                sd      t1,R_IO_EXT_MULT_SIZE(t0)

                li      t1, (UART_TIMING0)
                sd      t1,R_IO_EXT_TIME_CFG0(t0)

                li      t1, (UART_TIMING1)
                sd      t1,R_IO_EXT_TIME_CFG1(t0)

                li      t1, (UART_CONFIG)
                sd      t1,R_IO_EXT_CFG(t0)

#ifdef SERIAL_PORT_LEDS

	/*
	 * Initialize the UART well enough to output characters.
	 */
		li	t0, PHYS_TO_K1(UART_PHYS)

		li	t1, CFCR_DLAB
		sb	t1, R_UART_CFCR(t0)

		li	t1, BRTC(CFG_SERIAL_BAUD_RATE)
		sb	t1, R_UART_DATA(t0)
		dsra	t1, t1, 8
		sb	t1, R_UART_IER(t0)

		li	t1, CFCR_8BITS
		sb	t1, R_UART_CFCR(t0)

		li	t1, (MCR_DTR | MCR_RTS | MCR_IENABLE)
		sb	t1, R_UART_MCR(t0)

		li	t1, 0
		sb	t1, R_UART_IER(t0)

		li	t1, FIFO_ENABLE
		sb	t1, R_UART_FIFO(t0)

		/* A short delay.  */
		li	t1, 200
1:		addiu	t1, t1, -1
		bne	t1, zero, 1b

		li	t1, (FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST \
			     | FIFO_TRIGGER_1)
		sb	t1, R_UART_FIFO(t0)

		/* A short delay.  */
		li	t1, 200
1:		addiu	t1, t1, -1
		bne	t1, zero, 1b

		/* if (value & MASK) != MASK, write 0 to fifo reg.  */
		lb	t1, R_UART_IIR(t0)
		andi	t1, t1, IIR_FIFO_MASK
		xori	t1, t1, IIR_FIFO_MASK
		beq	t1, zero, 1f

		li	t1, 0
		sb	t1, R_UART_FIFO(t0)
1:
#endif

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

#define  _HARDWIRED_MEMORY_TABLE 1

#define CFG_DRAM_tROUNDTRIP DRT10(2,0)
/* CFG_DRAM_MIN_tMEMCLK must be set to 7.  Some DIMMS cause diag failures
    when the memclk is at 125 MHz but pass at 100 MHz or 133 MHz.  Setting to 7 causes
    memclk to be 133 MHz*/
#define CFG_DRAM_MIN_tMEMCLK  DRT10(7,0)
#define DEVADDR (CFG_DRAM_SMBUS_BASE)
#define DEFCHAN (CFG_DRAM_SMBUS_CHANNEL)

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
    DRAM_GLOBALS(0)	   		/* no  port interleaving */

    /*
     * Memory channel 0: Configure via SMBUS, Automatic Timing
     * Assumes SMBus device numbers are arranged such
     * that the first two addresses are CS0,1 and CS2,3 on MC0
     * and the second two addresses are CS0,1 and CS2,3 on MC1
     */
       DRAM_CHAN_CFG2(MC_CHAN1, CFG_DRAM_MIN_tMEMCLK, CFG_DRAM_tROUNDTRIP,  DRAM_TYPE_SPD, CASCHECK, CFG_DRAM_BLOCK_SIZE, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0)

      

       DRAM_CS_SPD(MC_CS0, 0, DEFCHAN, DEVADDR+2)	
       DRAM_CS_SPD(MC_CS2, 0, DEFCHAN, DEVADDR+3)

   
       DRAM_EOT

END(board_draminfo)



#ifdef SERIAL_PORT_LEDS
/*  *********************************************************************
    *  BOARD_UART_TXCHAR(x)
    *  
    *  Transmit one character out the UART on the GENERIC bus.
    *  
    *  Input parameters: 
    *  	   a0 - 8 bit character value.
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */

LEAF(board_uart_txchar)

	# Wait until there is space in the transmit buffer.

		li	t0, PHYS_TO_K1(UART_PHYS)

1:		lb	t1, R_UART_LSR(t0)
		andi	t1, t1, LSR_TXRDY
		beq	t1, zero, 1b

	# OK, now send a character.

		sb	a0, R_UART_DATA(t0)

	# And return.

		j	ra

END(board_uart_txchar)
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

#ifdef SERIAL_PORT_LEDS
#define	OUTPUT_CHAR(offset)					\
	li	t0, PHYS_TO_K1(LEDS_PHYS) ;			\
	sb	a0, offset(t0) ;				\
	bal	board_uart_txchar
#else
#define	OUTPUT_CHAR(offset)					\
	li	t0, PHYS_TO_K1(LEDS_PHYS) ;			\
	sb	a0, offset(t0)
#endif

LEAF(board_setleds)

		move	t3, ra
		move	t2, a0

#ifdef SERIAL_PORT_LEDS
		li	a0, '['
		bal	board_uart_txchar
#endif

		move	a0, t2
		rol	a0, 8
		OUTPUT_CHAR(LED_CHAR0)

		rol	a0, 8
		OUTPUT_CHAR(LED_CHAR1)

		rol	a0, 8
		OUTPUT_CHAR(LED_CHAR2)

		rol	a0, 8
		OUTPUT_CHAR(LED_CHAR3)

#ifdef SERIAL_PORT_LEDS
		li	a0, ']'
		bal	board_uart_txchar
		li	a0, '\r'
		bal	board_uart_txchar
		li	a0, '\n'
		bal	board_uart_txchar
#endif

		move	ra, t3
		j	ra

END(board_setleds)

/*  *********************************************************************
    *  cs0_remap()
    *  
    *  Change the size of the bootrom area.
    *  This routine is called only after CFE has been relocated to DRAM
    *  and is executing from DRAM.  After that point, the boot rom
    *  is serving as a flash file storage area.  Note:  this could be
    *  done in board_earlyinit since we're not changing the base address.
    *  
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */

LEAF(cs0_remap)
 		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(BOOTROM_CS))

         	li	t1,BOOTROM_SIZE-1    
 		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		j 	ra
END(cs0_remap)

/*  *********************************************************************
    *  cs1_remap()
    *  
    *  Change the size of the flash area.
    *  Usually CS1 is the promice, but if it's not, the flash is here.
    *  
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */

LEAF(cs1_remap)
 		li	t0,PHYS_TO_K1(A_IO_EXT_CS_BASE(ALT_BOOTROM_CS))

         	li	t1,BOOTROM_SIZE-1    
 		sd	t1,R_IO_EXT_MULT_SIZE(t0)

		j 	ra
END(cs1_remap)
