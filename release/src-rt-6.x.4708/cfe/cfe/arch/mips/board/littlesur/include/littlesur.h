/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  LITTLESUR  Definitions   		   	File: littlesur.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the LITTLESUR checkout board.
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


/*
 * I/O Address assignments for the littlesur board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      16MB    CS0    Boot ROM
 * 			   CS1    Unused
 *                         CS2    Unused
 * 0x100A0000      64KB    CS3    LED display
 * 0x100B0000      64KB    CS4    IDE Disk
 *                         CS5    Unused
 * 0x100C0000      64KB    CS6    Real Time Clock (RTC)
 *                         CS7    Unused
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    Inout       Carmel GPIO 0
 * GPIO1    Input	RTC OUT			(interrupt)
 * GPIO2    Input	PHY Interrupt	    	(interrupt)
 * GPIO3    Input	Nonmaskable Interrupt	(interrupt)
 * GPIO4    Input	IDE Disk Interrupt	(interrupt)
 * GPIO5    Input	Temperature Sensor Alert (interrupt)
 * GPIO6    Input	HT Loopback Reset
 * GPIO7    Input       HT Loopback Power OK 
 * GPIO8    Input	Carmel GPIO 9
 * GPIO9    N/A		Unused
 * GPIO10   N/A		Unused
 * GPIO11   N/A		Unused
 * GPIO12   Input	Carmel GPIO 12
 * GPIO13   Input	Carmel GPIO 13
 * GPIO14   Input	Slot1 GPIO 0
 * GPIO15   Input	Slot1 GPIO 1
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

#define GPIO_RTC_OUT_INT       	1
#define GPIO_PHY_INTERRUPT	2
#define GPIO_NONMASKABLE_INT	3
#define GPIO_IDE_INTERRUPT	4
#define GPIO_TEMP_SENSOR_INT	5

#define M_GPIO_RTC_OUT_INT	_SB_MAKEMASK1(GPIO_RTC_OUT_INT)
#define M_GPIO_PHY_INTERRUPT	_SB_MAKEMASK1(GPIO_PHY_INTERRUPT)
#define M_GPIO_NONMASKABLE_INT	_SB_MAKEMASK1(GPIO_NONMASKABLE_INT)
#define M_GPIO_IDE_INTERRUPT	_SB_MAKEMASK1(GPIO_IDE_INTERRUPT)
#define M_GPIO_TEMP_SENSOR_INT	_SB_MAKEMASK1(GPIO_TEMP_SENSOR_INT)

#define GPIO_INTERRUPT_MASK ((V_GPIO_INTR_TYPEX(GPIO_PHY_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_IDE_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_TEMP_SENSOR_INT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_RTC_OUT_INT,K_GPIO_INTR_LEVEL)))

#define GPIO_OUTPUT_MASK     	0

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

/*
 * LEDs:  non-multiplexed, byte width, no parity, no ack
 *
 */
#define LEDS_CS			3
#define LEDS_PHYS		0x100A0000	/* same address as SWARM */
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
 * RTC:  non-multiplexed, byte width, no parity, ack mode
 *
 */
#define RTC_CS			6
#define RTC_PHYS		0x100C0000
#define RTC_SIZE		NUM64K(4)
#define RTC_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(1) | \
                                V_IO_CS_WIDTH(23) | \
                                V_IO_RDY_SMPLE(1)
#define RTC_TIMING1		V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(3) | \
                                V_IO_OE_TO_CS(2)
#define RTC_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1L)

/*
 * IDE: non-multiplexed, word(16) width, no parity, ack mode
 * See BCM12500 Application Note: "BCM12500 Generic Bus Interface
 * to ATA/ATAPI PIO Mode 3 (IDE) Hard Disk"
 */
#define IDE_CS			4
#define IDE_PHYS		0x100B0000
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
                                

/*  *********************************************************************
    *  SMBus 
    ********************************************************************* */

#define TEMPSENSOR_SMBUS_CHAN	0
#define TEMPSENSOR_SMBUS_DEV	0x2A

#define BIGEEPROM_SMBUS_CHAN_1	1
#define BIGEEPROM_SMBUS_DEV_1	0x51

#define BIGEEPROM_SMBUS_CHAN_0	0
#define BIGEEPROM_SMBUS_DEV_0	0x50


/*  *********************************************************************
    *  Board revision numbers
    ********************************************************************* */

/* Maps from SYSTEM_CFG register to actual board rev #'s */

#define LITTLESUR_REV_1	0
