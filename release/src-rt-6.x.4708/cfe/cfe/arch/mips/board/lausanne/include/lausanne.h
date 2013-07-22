/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Lausanne Definitions   		   File: lausanne.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the Lausanne checkout board.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  Edits for Lausanne:  Jeffrey Cheng (chengj@broadcom.com)
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
 * I/O Address assignments for the LAUSANNE board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      2MB     CS0    Boot ROM
 * 0x1F800000      2MB     CS1    Alternate boot ROM
 * 0x100B0000      64KB    CS2    CPLD
 * 0x100A0000	   64KB    CS3    LED display
 *                         CS4    Unused
 *                         CS5    Unused
 *                         CS6    Unused
 *                         CS7    Unused
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    N/A         Test point
 * GPIO1    N/A         Test point
 * GPIO2    Input       PHY Interrupt               (interrupt)
 * GPIO3    Input       Nonmaskable Interrupt       (interrupt)
 * GPIO4    N/A         Test point
 * GPIO5    Input       Temperature Sensor Alert    (interrupt)
 * GPIO6    N/A         CPLD
 * GPIO7    N/A         CPLD
 * GPIO8    N/A         CPLD
 * GPIO9    Input       Boot Select
 * GPIO10   N/A         Test point
 * GPIO11   N/A         Test point
 * GPIO12   N/A         Test point
 * GPIO13   N/A         Test point
 * GPIO14   N/A         Test point
 * GPIO15   Output      Flash Byte Enable
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
#define GPIO_TEMP_SENSOR_INT	5
#define GPIO_BOOT_SELECT	9
#define GPIO_FLASH_BYTE_EN	15

#define GPIO_OUTPUT_MASK (_SB_MAKE64(GPIO_FLASH_BYTE_EN))
#define GPIO_INTERRUPT_MASK (V_GPIO_INTR_TYPEX(GPIO_PHY_INTERRUPT,K_GPIO_INTR_LEVEL))


/*  *********************************************************************
    *  Generic Bus 
    ********************************************************************* */

#define BOOTROM_CS		0
#define BOOTROM_PHYS		0x1FC00000	/* address of boot ROM (CS0) */
#define BOOTROM_SIZE		NUM64K(4*MB)	/* size of boot ROM */
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

#define ALT_BOOTROM_CS		1
#define ALT_BOOTROM_PHYS	0x1EC00000	/* address of alternate boot ROM (CS1) */
#define ALT_BOOTROM_SIZE	NUM64K(16*MB)	/* size of alternate boot ROM */
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

/*
 * CPLD: multiplexed, byte width [io_ad7:io_ad0], no parity, no ack
 * See documentation: /home/chengj/systems/cpld/lausanne/Chksum.doc
 */
#define CPLD_CS			2
#define CPLD_PHYS		0x1D0B0000
#define CPLD_SIZE		NUM64K(256)
#define CPLD_TIMING0		V_IO_ALE_WIDTH(3) | \
                                V_IO_ALE_TO_CS(1) | \
                                V_IO_CS_WIDTH(8) | \
                                V_IO_RDY_SMPLE(2)
#define CPLD_TIMING1		V_IO_ALE_TO_WRITE(4) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(1) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define CPLD_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1L)

                                
/*
 * LEDs:  non-multiplexed, byte width, no parity, no ack
 */
#define LEDS_CS			3
#define LEDS_PHYS		0x1D0A0000
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

#define TEMPSENSOR_SMBUS_CHAN	1
#define TEMPSENSOR_SMBUS_DEV	0x2A
#define DRAM_SMBUS_CHAN		0
#define DRAM_SMBUS_DEV		0x50
/* The SMBus eeprom is on channel 1 for lausanne */
#define BIGEEPROM_SMBUS_CHAN	1
#define BIGEEPROM_SMBUS_DEV	0x50

/*
 * REV2 definitions
 */

#include "sb1250_pass2.h"
