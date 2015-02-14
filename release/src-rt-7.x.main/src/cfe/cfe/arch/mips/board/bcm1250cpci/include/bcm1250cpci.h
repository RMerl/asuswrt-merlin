/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  BCM1250CPCI  Definitions   		   File: bcm1250cpci.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the BCM1250CPCI evaluation board.
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
 * I/O Address assignments for the BCM1250CPCI board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      4MB     CS0    Boot ROM
 * 0x1F800000      4MB     CS1    Alternate boot ROM
 *                         CS2    Unused
 * 0x100A0000      64KB    CS3    LED Display
 * 0x100B0000      64KB    CS4    IDE disk
 * 0x100C0000      64KB    CS5    USB controller
 * 0x11000000      64MB    CS6    PCMCIA
 * 0x100D0000      64KB    CS7    CPCI CPLD
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    Input       USB Interrupt (interrupt)
 * GPIO1    Input       USB EXT_INT_L (interrupt)
 * GPIO2    Input	PHY Interrupt (interrupt)
 * GPIO3    Input	CPCI Interrupt (interrupt)       
 * GPIO4    Input       IDE Interrupt (interrupt)
 * GPIO5    Input       Temperature Sensor Alert (interrupt)
 * GPIO6    N/A         PCMCIA interface
 * GPIO7    N/A         PCMCIA interface
 * GPIO8    N/A         PCMCIA interface
 * GPIO9    N/A         PCMCIA interface
 * GPIO10   N/A         PCMCIA interface
 * GPIO11   N/A         PCMCIA interface
 * GPIO12   N/A         PCMCIA interface
 * GPIO13   N/A         PCMCIA interface
 * GPIO14   N/A         PCMCIA interface
 * GPIO15   N/A         PCMCIA interface
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

#define GPIO_USB_INTERRUPT	0
#define GPIO_USB_EXT_INT	1
#define GPIO_PHY_INTERRUPT	2
#define GPIO_CPCI_INTERRUPT	3
#define GPIO_IDE_INTERRUPT	4
#define GPIO_TEMP_SENSOR_INT	5

#define M_GPIO_USB_INTERRUPT	_SB_MAKEMASK1(GPIO_USB_INTERRUPT)
#define M_GPIO_USB_EXT_INT	_SB_MAKEMASK1(GPIO_USB_EXT_INT)
#define M_GPIO_PHY_INTERRUPT	_SB_MAKEMASK1(GPIO_PHY_INTERRUPT)
#define M_GPIO_CPCI_INTERRUPT	_SB_MAKEMASK1(GPIO_CPCI_INTERRUPT)
#define M_GPIO_IDE_INTERRUPT	_SB_MAKEMASK1(GPIO_IDE_INTERRUPT)
#define M_GPIO_TEMP_SENSOR_INT	_SB_MAKEMASK1(GPIO_TEMP_SENSOR_INT)

/* Leave bidirectional pins in "input" state at boot. */

#define GPIO_OUTPUT_MASK (0)	/* all non-PCMCIA pins are interrupts */


#define GPIO_INTERRUPT_MASK ((V_GPIO_INTR_TYPEX(GPIO_USB_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_PHY_INTERRUPT,K_GPIO_INTR_LEVEL)) | \
                             (V_GPIO_INTR_TYPEX(GPIO_IDE_INTERRUPT,K_GPIO_INTR_LEVEL)))



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
#define ALT_BOOTROM_PHYS	0x1F800000	/* address of alternate boot ROM (CS1) */
#define ALT_BOOTROM_SIZE	NUM64K(4*MB)	/* size of alternate boot ROM */
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
                                

#define USBCTL_CS		5
#define USBCTL_PHYS		0x100C0000
#define USBCTL_SIZE		NUM64K(4)
#define USBCTL_TIMING0		V_IO_ALE_WIDTH(1) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(9) | \
                                V_IO_RDY_SMPLE(1)
#define USBCTL_TIMING1		V_IO_ALE_TO_WRITE(1) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(15) | \
                                V_IO_CS_TO_OE(1) | \
                                V_IO_OE_TO_CS(0)
#define USBCTL_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX
                              
/*
 * PCMCIA: this information was derived from chapter 12, table 12-5
 */
#define PCMCIA_CS		6
#define PCMCIA_PHYS		0x11000000
#define PCMCIA_SIZE		NUM64K(64*MB)
#define PCMCIA_TIMING0		V_IO_ALE_WIDTH(3) | \
                                V_IO_ALE_TO_CS(1) | \
                                V_IO_CS_WIDTH(17) | \
                                V_IO_RDY_SMPLE(1)
#define PCMCIA_TIMING1		V_IO_ALE_TO_WRITE(8) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(2) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define PCMCIA_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_2)

/* These values work a PCMCIA HD I have ...., but I am going to try the original
// #define PCMCIA_CS		6
// #define PCMCIA_PHYS		0x11000000
// #define PCMCIA_SIZE		NUM64K(64*MB)
// #define PCMCIA_TIMING0		V_IO_ALE_WIDTH(3) | \
//                                 V_IO_ALE_TO_CS(2) | \
//                                 V_IO_CS_WIDTH(25) | \
//                                 V_IO_RDY_SMPLE(1)
// #define PCMCIA_TIMING1		V_IO_ALE_TO_WRITE(8) | \
//                                 V_IO_WRITE_WIDTH(12) | \
//                                 V_IO_IDLE_CYCLE(2) | \
//                                 V_IO_CS_TO_OE(0) | \
//                                 V_IO_OE_TO_CS(0)
// #define PCMCIA_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_2)
*/

#define CPCICPLD_CS		7
#define CPCICPLD_PHYS		0x100D0000
#define CPCICPLD_SIZE		NUM64K(4)
#define CPCICPLD_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(13) | \
                                V_IO_RDY_SMPLE(1)
#define CPCICPLD_TIMING1		V_IO_ALE_TO_WRITE(2) | \
                                V_IO_WRITE_WIDTH(8) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define CPCICPLD_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX

/*  *********************************************************************
    *  SMBus devices
    ********************************************************************* */

#define TEMPSENSOR_SMBUS_CHAN	0
#define TEMPSENSOR_SMBUS_DEV	0x2A

#define M24LC128_0_SMBUS_CHAN	0
#define M24LC128_0_SMBUS_DEV	0x50

#define M24LC128_1_SMBUS_CHAN	1
#define M24LC128_1_SMBUS_DEV	0x50

#define X1240_SMBUS_CHAN		1
#define X1240_SMBUS_DEV			0x50

#define M41T81_SMBUS_CHAN		1			
#define M41T81_SMBUS_DEV		0x68		
