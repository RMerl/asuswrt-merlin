/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  SENTOSA  Definitions   		   	File: sentosa.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the BCM912500E checkout board.
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
 * I/O Address assignments for the C3 board
 *
 * Summary of address map:
 *
 * Address         Size   CSel    Description
 * --------------- ----   ------  --------------------------------
 * 0x1FC00000      4MB     CS0    Boot ROM
 * 0x1F800000      4MB     CS1    Alternate boot ROM
 *                         CS2    Unused
 *                         CS3    Unused
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
 * GPIO1    N/A		Not used, routed to daughter card
 * GPIO2    N/A		Not used, routed to daughter card
 * GPIO3    N/A		Not used, routed to daughter card
 * GPIO4    N/A		Not used, routed to daughter card
 * GPIO5    N/A		Not used, routed to daughter card
 * GPIO6    N/A		Not used, routed to daughter card
 * GPIO7    N/A		Not used, routed to daughter card
 * GPIO8    N/A		Not used, routed to daughter card
 * GPIO9    N/A		Not used, routed to daughter card
 * GPIO10   N/A		Not used, routed to daughter card
 * GPIO11   N/A		Not used, routed to daughter card
 * GPIO12   N/A		Not used, routed to daughter card
 * GPIO13   N/A		Not used, routed to daughter card
 * GPIO14   N/A		Not used, routed to daughter card
 * GPIO15   N/A		Not used, routed to daughter card
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

#define M_GPIO_DEBUG_LED	_SB_MAKEMASK1(GPIO_DEBUG_LED)

/* Leave bidirectional pins in "input" state at boot. */

#define GPIO_OUTPUT_MASK (M_GPIO_DEBUG_LED)
#define GPIO_INTERRUPT_MASK (0)

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

/*  *********************************************************************
    *  SMBus 
    ********************************************************************* */

#define TEMPSENSOR_SMBUS_CHAN	0		/* All Sentosa */
#define TEMPSENSOR_SMBUS_DEV	0x2A

#define BIGEEPROM1_SMBUS_CHAN	1		/* only on Rev 2.0 Sentosa */
#define BIGEEPROM1_SMBUS_DEV	0x50

#define X1240_SMBUS_CHAN	1		/* Only on Rev 1.0 Sentosa */
#define X1240_SMBUS_DEV		0x57		/* and some early 2.0 */

#define M41T81_SMBUS_CHAN	1		/* Only on rev 2.0 Sentosa */
#define M41T81_SMBUS_DEV	0x68		/* that does not have an X1240 */


/*  *********************************************************************
    *  Board revision numbers
    ********************************************************************* */

/* Maps from SYSTEM_CFG register to actual board rev #'s */

#define SENTOSA_REV_1	0
#define SENTOSA_REV_2	1
#define SENTOSA_REV_3	2
