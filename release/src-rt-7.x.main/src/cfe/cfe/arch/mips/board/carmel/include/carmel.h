/*  *********************************************************************
    *  SB1125 Board Support Package
    *  
    *  Carmel Board definitions			File: carmel.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the CARMEL (BCM1120) board.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *		Binh Vo (binh@broadcom.com)
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


/*
 * I/O Address assignments for the carmel board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      16MB    CS0    Boot ROM
 * 0x100F0000	   64KB    CS1    A/D converters on FPGA
 * 0x100C0000	   64KB	   CS2    7-segment LED display    
 * 0x100A0000	   64KB    CS3    4-char LED display on Monterey
 * 0x100D0000      64KB    CS4    Quad UART
 * 0x11000000      128MB   CS5    FPGA on Monterey/LittleSur/BigSur
 * 0x100B0000      64KB    CS6    CompactFlash
 * 0x100E0000      64KB    CS7    FPGA (for JTAG region)
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    Input	Not used
 * GPIO1    Input	Not used
 * GPIO2    Input 	PHY Interrupt		    (interrupt)
 * GPIO3    Input	Nonmaskable Interrupt	    (interrupt)
 * GPIO4    Input	Not used
 * GPIO5    Input       Not used
 * GPIO6    Input	CF Inserted (high = inserted)
 * GPIO7    Output	Monterey Reset (active low)
 * GPIO8    Input	Quad UART Interrupt	    (interrupt)
 * GPIO9    Input	CF Interrupt		    (interrupt)
 * GPIO10   Output	FPGA CCLK
 * GPIO11   Input	FPGA DOUT
 * GPIO12   Output	FPGA DIN
 * GPIO13   Output	FPGA PGM
 * GPIO14   Input	FPGA DONE
 * GPIO15   Output	FPGA INIT
 */

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define MB (1024*1024)
#define K64 65536
#define NUM64K(x) (((x)+(K64-1))/K64)


/*  *********************************************************************
    *  GPIO pins
    ********************************************************************* */

#define GPIO_PHY_INTERRUPT	2
#define GPIO_NONMASKABLE_INT	3
#define GPIO_CF_INSERTED	6
#define GPIO_MONTEREY_RESET	7
#define GPIO_QUADUART_INT	8
#define GPIO_CF_INT		9

#define GPIO_FPGA_CCLK		10	/* output */
#define GPIO_FPGA_DOUT		11	/* input */
#define GPIO_FPGA_DIN		12	/* output */
#define GPIO_FPGA_PGM		13	/* output */
#define GPIO_FPGA_DONE		14	/* input */
#define GPIO_FPGA_INIT		15	/* output */

#define GPIO_FPGACONV_CCLK	10
#define GPIO_FPGACONV_DIN	12
#define GPIO_FPGACONV_PGM	5
#define GPIO_FPGACONV_DONE	14
#define GPIO_FPGACONV_INIT	15

#define M_GPIO_PHY_INTERRUPT	_SB_MAKEMASK1(GPIO_PHY_INTERRUPT)
#define M_GPIO_NONMASKABLE_INT	_SB_MAKEMASK1(GPIO_NONMASKABLE_INT)
#define M_GPIO_CF_INSERTED	_SB_MAKEMASK1(GPIO_CF_INSERTED)
#define M_GPIO_MONTEREY_RESET	_SB_MAKEMASK1(GPIO_MONTEREY_RESET)
#define M_GPIO_QUADUART_INT	_SB_MAKEMASK1(GPIO_QUADUART_INT)
#define M_GPIO_CF_INT		_SB_MAKEMASK1(GPIO_CF_INT)

#define M_GPIO_FPGA_CCLK	_SB_MAKEMASK1(GPIO_FPGA_CCLK)
#define M_GPIO_FPGA_DOUT	_SB_MAKEMASK1(GPIO_FPGA_DOUT)
#define M_GPIO_FPGA_DIN		_SB_MAKEMASK1(GPIO_FPGA_DIN)
#define M_GPIO_FPGA_PGM		_SB_MAKEMASK1(GPIO_FPGA_PGM)
#define M_GPIO_FPGA_DONE	_SB_MAKEMASK1(GPIO_FPGA_DONE)
#define M_GPIO_FPGA_INIT	_SB_MAKEMASK1(GPIO_FPGA_INIT)

#define M_GPIO_FPGACONV_CCLK	_SB_MAKEMASK1(GPIO_FPGACONV_CCLK)
#define M_GPIO_FPGACONV_DIN	_SB_MAKEMASK1(GPIO_FPGACONV_DIN)
#define M_GPIO_FPGACONV_PGM	_SB_MAKEMASK1(GPIO_FPGACONV_PGM)
#define M_GPIO_FPGACONV_DONE	_SB_MAKEMASK1(GPIO_FPGACONV_DONE)
#define M_GPIO_FPGACONV_INIT	_SB_MAKEMASK1(GPIO_FPGACONV_INIT)


#define GPIO_INTERRUPT_MASK ((V_GPIO_INTR_TYPEX(GPIO_PHY_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_QUADUART_INT,K_GPIO_INTR_LEVEL)))

#define GPIO_INVERT_MASK (M_GPIO_QUADUART_INT)

#define GPIO_OUTPUT_MASK (M_GPIO_MONTEREY_RESET)
/* | M_GPIO_FPGA_CCLK | M_GPIO_FPGA_DIN | \
                          M_GPIO_FPGA_PGM | M_GPIO_FPGA_INIT)
*/


/*  *********************************************************************
    *  Generic Bus 
    ********************************************************************* */

#define BOOTROM_CS		0
#define BOOTROM_PHYS		0x1FC00000	/* address of boot ROM (CS0) */
#define BOOTROM_SIZE		NUM64K(16*MB)	/* size of boot ROM */
#define BOOTROM_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(24) | \
                                V_IO_RDY_SMPLE(1)
#define BOOTROM_TIMING1		V_IO_ALE_TO_WRITE(7) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define BOOTROM_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX


/*
 * ADC: non-multiplexed, word(16) width, no parity, ack mode
 */
#define ADC_CS			1
#define ADC_PHYS		0x100F0000
#define ADC_SIZE		NUM64K(65536)
#define ADC_TIMING0		V_IO_ALE_WIDTH(3) | \
                                V_IO_ALE_TO_CS(1) | \
                                V_IO_CS_WIDTH(8) | \
                                V_IO_RDY_SMPLE(2)
#define ADC_TIMING1		V_IO_ALE_TO_WRITE(4) | \
                                V_IO_WRITE_WIDTH(0xA) | \
                                V_IO_IDLE_CYCLE(1) | \
                                V_IO_CS_TO_OE(3) | \
                                V_IO_OE_TO_CS(2)
#define ADC_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_2) | \
                                M_IO_RDY_ACTIVE | \
                                M_IO_ENA_RDY
                                


/*
 * LEDs:  non-multiplexed, byte width, no parity, no ack
 */
#define LEDS_CS			2
#define LEDS_PHYS		0x100C0000
#define LEDS_SIZE		NUM64K(4)
#define LEDS_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(13) | \
                                V_IO_RDY_SMPLE(1)
#define LEDS_TIMING1		V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define LEDS_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX
                              

/*
 * MLEDs:  non-multiplexed, byte width, no parity, no ack
 * This is the LED display on the Monterey board.
 */
#define MLEDS_CS		3
#define MLEDS_PHYS		0x100A0000	/* same address as SWARM */
#define MLEDS_SIZE		NUM64K(4)
#define MLEDS_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(13) | \
                                V_IO_RDY_SMPLE(1)
#define MLEDS_TIMING1		V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define MLEDS_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX
                              
/*
 * Quad UART:  non-multiplexed, byte width, no parity, no ack
 * This is the Exar quad uart on the Monterey board.
 */

#define UART_CS			4
#define UART_PHYS		0x100D0000	
#define UART_SIZE		NUM64K(32)	
#define UART_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(24) | \
                                V_IO_RDY_SMPLE(1)
#define UART_TIMING1		V_IO_ALE_TO_WRITE(7) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define UART_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX


/*
 * ARAVALI:  multiplexed, 8-bit width, no parity, ack
 * 128MB region
 */
#define ARAVALI_CS		5
#define ARAVALI_PHYS		0x11000000
#define ARAVALI_SIZE		NUM64K(128*1024*1024)
#define ARAVALI_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(6) | \
                                V_IO_RDY_SMPLE(1)
#define ARAVALI_TIMING1		V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define ARAVALI_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_ENA_RDY | V_IO_TIMEOUT(8)
                              

/*
 * IDE: non-multiplexed, word(16) width, no parity, ack mode
 * See BCM12500 Application Note: "BCM12500 Generic Bus Interface
 * to ATA/ATAPI PIO Mode 3 (IDE) Hard Disk"
 */
#define IDE_CS			6
#define IDE_PHYS		0x100B0000	/* same address as SWARM */
#define IDE_SIZE		NUM64K(256)
#define IDE_TIMING0		V_IO_ALE_WIDTH(3) | \
                                V_IO_ALE_TO_CS(1) | \
                                V_IO_CS_WIDTH(8) | \
                                V_IO_RDY_SMPLE(2)
#define IDE_TIMING1		V_IO_ALE_TO_WRITE(4) | \
                                V_IO_WRITE_WIDTH(0xA) | \
                                V_IO_IDLE_CYCLE(1) | \
                                V_IO_CS_TO_OE(3) | \
                                V_IO_OE_TO_CS(2)
#define IDE_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_2) | \
                                M_IO_RDY_ACTIVE | \
                                M_IO_ENA_RDY
                                


/*
 * ARAVALI2: (JTAG region)  multiplexed, 32-bit width, no parity, ack
 * 64KB region
 */
#define ARAVALI2_CS		7
#define ARAVALI2_PHYS		0x100E0000
#define ARAVALI2_SIZE		NUM64K(65536)
#define ARAVALI2_TIMING0	V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(6) | \
                                V_IO_RDY_SMPLE(1)
#define ARAVALI2_TIMING1	V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define ARAVALI2_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_4) | M_IO_ENA_RDY | V_IO_TIMEOUT(8)
                              



/*  *********************************************************************
    *  SMBus Devices
    ********************************************************************* */

/*
 * SMBus 0
 */

#define SPDEEPROM_SMBUS_CHAN	0		/* SPD for memory chips */
#define SPDEEPROM_SMBUS_DEV	0x54

#define ENVEEPROM_SMBUS_CHAN	0		/* CFE's environment */
#define ENVEEPROM_SMBUS_DEV	0x50

#define IDEEPROM_SMBUS_CHAN	0		/* On Monterey board */
#define IDEEPROM_SMBUS_CHAN_ALT	1		/* On LittleSur board */
#define IDEEPROM_SMBUS_DEV	0x51
