/*  *********************************************************************
    *  SB1125 Board Support Package
    *  
    *  BCM11xx Checkout Board definitions	File: bcm91125c.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the BCM91125c checkout board.
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
 * I/O Address assignments for the bcm91125c board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      2MB     CS0    Boot ROM
 * 0x1F800000      2MB     CS1    Alternate boot ROM (PromICE)
 * 0x1E000000      16MB    CS2    Big Flash
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
 * GPIO0    Input	Not used
 * GPIO1    Input	RTC Out			    (interrupt)
 * GPIO2    Input       Not used
 * GPIO3    Input       Not used
 * GPIO4    Input	Not used
 * GPIO5    Input       Temperature Sensor Alert    (interrupt)
 * GPIO6    Input	HT Interrupt		    (interrupt)
 * GPIO7    Input	PHY Interrupt               (interrupt)
 * GPIO8    Input	Not used
 * GPIO9    Input	Not used
 * GPIO10   Input	Not used
 * GPIO11   Input	Not used
 * GPIO12   Input	Not used
 * GPIO13   Input	Not used
 * GPIO14   Output	Serial port 1 Loopback Enable
 * GPIO15   Output	Serial port 0 Loopback Enable
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

#define GPIO_PHY_INTERRUPT	7
#define GPIO_HT_INTERRUPT	6
#define GPIO_TEMP_SENSOR_INT	5
#define GPIO_RTC_OUT_INT	1

#define GPIO_SERIAL1_LOOPBACK	14
#define GPIO_SERIAL0_LOOPBACK	15


#define M_GPIO_SERIAL1_LOOPBACK	_SB_MAKEMASK1(GPIO_SERIAL1_LOOPBACK)
#define M_GPIO_SERIAL0_LOOPBACK	_SB_MAKEMASK1(GPIO_SERIAL0_LOOPBACK)
#define M_GPIO_PHY_INTERRUPT	_SB_MAKEMASK1(GPIO_PHY_INTERRUPT)
#define M_GPIO_TEMP_SENSOR_INT	_SB_MAKEMASK1(GPIO_TEMP_SENSOR_INT)
#define M_GPIO_HT_INTERRUPT	_SB_MAKEMASK1(GPIO_HT_INTERRUPT)
#define M_GPIO_RTC_OUT_INT	_SB_MAKEMASK1(GPIO_RTC_OUT_INT)

#define GPIO_OUTPUT_MASK (_SB_MAKEMASK1(GPIO_SERIAL0_LOOPBACK) | \
                          _SB_MAKEMASK1(GPIO_SERIAL1_LOOPBACK) )

#define GPIO_INTERRUPT_MASK ((V_GPIO_INTR_TYPEX(GPIO_PHY_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_TEMP_SENSOR_INT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_HT_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_RTC_OUT_INT,K_GPIO_INTR_LEVEL)))


/*  *********************************************************************
    *  Generic Bus 
    ********************************************************************* */

#define BOOTROM_CS		0
#define BOOTROM_PHYS		0x1FC00000	/* address of boot ROM (CS0) */
#define BOOTROM_SIZE		NUM64K(2*MB)	/* size of boot ROM */
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
#define ALT_BOOTROM_PHYS	0x1F800000	/* address of alternate boot ROM (CS1) */
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


#define BIG_FLASH_CS		2
#define BIG_FLASH_PHYS	0x1E000000	/* address of big flash ROM (CS2) */
#define BIG_FLASH_SIZE	NUM64K(16*MB)	/* size of big flash ROM */
#define BIG_FLASH_TIMING0	V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(24) | \
                                V_IO_RDY_SMPLE(1)
#define BIG_FLASH_TIMING1	V_IO_ALE_TO_WRITE(7) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define BIG_FLASH_CONFIG	V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX

/*
 * LEDs:  non-multiplexed, byte width, no parity, no ack
 */
#define LEDS_CS			3
#define LEDS_PHYS		0x100A0000
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
#define BIGEEPROM0_SMBUS_CHAN	0
#define BIGEEPROM0_SMBUS_DEV	0x50
#define BIGEEPROM1_SMBUS_CHAN	1
#define BIGEEPROM1_SMBUS_DEV	0x50

#define M41T81_SMBUS_CHAN	1     
#define M41T81_SMBUS_DEV	0x68	      
