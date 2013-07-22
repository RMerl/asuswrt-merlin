/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  PT1125 Definitions   	File: pt1125.h
    *  PT1125 is a slightly modified version of PT1120, set up to be
    *  a Production Tester, hence PT.
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the PT1125 production test board.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *           
    * modification history
    * --------------------
    * 01a,20Aug02,gtb  created for  pt1125 board.  
    *                           Copy of pt1120.h
    *                    
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

/*
 * I/O Address assignments for the PT1125 board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      16MB    CS0    Boot ROM
 * 0x1CC00000       2MB    CS1    Alternate boot ROM
 * 0x1B000000      64KB    CS2    External UART
 * 0x1B0A0000	   64KB    CS3    LED display
 *                         CS4    Unused
 *                         CS5    Unused
 *                         CS6    Unused
 *                         CS7    Unused
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    Output      Debug LED
 * GPIO1    Input       UART interrupt		    (interrupt)
 * GPIO2    Input       PHY interrupt 		    (interrupt)
 * GPIO3    N/A         fpga interface
 * GPIO4    N/A         fpga interface
 * GPIO5    Input       Temperature Sensor Alert    (interrupt)
 * GPIO6    N/A         fpga interface
 * GPIO7    N/A         fpga interface
 * GPIO8    N/A         fpga interface
 * GPIO9    N/A         fpga interface
 * GPIO10   N/A         fpga interface
 * GPIO11   N/A         fpga interface
 * GPIO12   N/A         fpga interface
 * GPIO13   N/A         fpga interface
 * GPIO14   N/A         fpga interface
 * GPIO15   N/A         fpga interface
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

#define GPIO_DEBUG_LED		0
#define GPIO_UART_INT		1
#define GPIO_PHY_INT		2
#define GPIO_RTC_OUT_INT	 3
#define GPIO_TEMP_SENSOR_INT	5

#define M_GPIO_DEBUG_LED	_SB_MAKEMASK1(GPIO_DEBUG_LED)
#define M_GPIO_RTC_OUT_INT	_SB_MAKEMASK1(GPIO_RTC_OUT_INT)

#define GPIO_OUTPUT_MASK (_SB_MAKEMASK1(GPIO_DEBUG_LED))

#define GPIO_INTERRUPT_MASK ((V_GPIO_INTR_TYPEX(GPIO_PHY_INT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_UART_INT,K_GPIO_INTR_LEVEL))| \
                             (V_GPIO_INTR_TYPEX(GPIO_TEMP_SENSOR_INT,K_GPIO_INTR_LEVEL))| \
                             (V_GPIO_INTR_TYPEX(GPIO_RTC_OUT_INT,K_GPIO_INTR_LEVEL)))


/*  *********************************************************************
    *  Generic Bus 
    ********************************************************************* */

/*
 * Boot ROM:  Using default parameters until CFE has been copied from
 * ROM to RAM and is executing in RAM
 */
#define BOOTROM_CS		0
#define BOOTROM_SIZE		NUM64K(16*MB)	/* size of boot ROM */
#define BOOTROM_PHYS 		0x1FC00000      /* address of boot ROM (CS0) */
#define BOOTROM_NCHIPS		8
#define BOOTROM_CHIPSIZE	2*MB


#define ALT_BOOTROM_CS		1
#define ALT_BOOTROM_PHYS	0x1CC00000	/* address of alternate boot ROM (CS1) */
#define ALT_BOOTROM_SIZE	NUM64K(2*MB)	/* size of alternate boot ROM */
#define ALT_BOOTROM_TIMING0	V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(24) | \
                                V_IO_RDY_SMPLE(1)
#define ALT_BOOTROM_TIMING1	V_IO_ALE_TO_WRITE(7) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define ALT_BOOTROM_CONFIG	V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX
#define ALT_BOOTROM_NCHIPS	1
#define ALT_BOOTROM_CHIPSIZE	2*MB

/*
 * External UART:  non-multiplexed, byte width, no parity, no ack
 */
#define UART_CS         2
#define UART_PHYS       0x1B000000      /* address of UART (CS2) */
#define UART_SIZE       NUM64K(8)       /* size allocated for UART access. minimum is 64KB */
#define UART_TIMING0    V_IO_ALE_WIDTH(4) | \
                        V_IO_ALE_TO_CS(2) | \
                        V_IO_CS_WIDTH(24) | \
                        V_IO_RDY_SMPLE(1)
#define UART_TIMING1    V_IO_ALE_TO_WRITE(7) | \
                        V_IO_WRITE_WIDTH(7) | \
                        V_IO_IDLE_CYCLE(6) | \
                        V_IO_CS_TO_OE(0) | \
                        V_IO_OE_TO_CS(0)
#define UART_CONFIG	V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX

/*
 * LEDs:  non-multiplexed, byte width, no parity, no ack
 */
#define LEDS_CS			3
#define LEDS_PHYS		0x1B0A0000
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
                              

/*  *********************************************************************
    *  SMBus Devices
    ********************************************************************* */

#define TEMPSENSOR_SMBUS_CHAN	0
#define TEMPSENSOR_SMBUS_DEV	0x2A
#define DRAM_SMBUS_CHAN		0
#define DRAM_SMBUS_DEV		0x54
#define BIGEEPROM_SMBUS_CHAN	0
#define BIGEEPROM_SMBUS_DEV	0x50

/*
#define X1240_SMBUS_CHAN	1
#define X1240_SMBUS_DEV		0x50
*/
#define M41T81_SMBUS_CHAN	1		
#define M41T81_SMBUS_DEV	0x68		
