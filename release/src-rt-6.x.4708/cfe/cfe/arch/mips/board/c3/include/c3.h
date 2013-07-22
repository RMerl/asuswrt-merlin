/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  C3 Checkout Board Definitions   		File: c3.h
    *
    *  This file contains I/O, chip select, and GPIO assignments
    *  for the BCM12500 "C3" checkout board.
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


#include "sb1250_jtag.h"

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
 *                         CS6    PCMCIA
 *                         CS7    Unused
 *
 * GPIO assignments
 *
 * GPIO#    Direction   Description
 * -------  ---------   ------------------------------------------
 * GPIO0    Output      Debug LED
 * GPIO1    Output      To Xilinx RTS_TSTROBE
 * GPIO2    Bidir	To Xilinx IO_ADP0
 * GPIO3    Bidir       To Xilinx IO_ADP1
 * GPIO4    Bidir       To Xilinx IO_ADP2
 * GPIO5    Bidir       To Xilinx IO_ADP3
 * GPIO6    Bidir       To Xilinx GPIOa pin
 * GPIO7    Bidir       To Xilinx GPIOb pin
 * GPIO8    Input       Nonmaskable Interrupt       (interrupt)
 * GPIO9    Input       Temperature Sensor Alert    (interrupt)
 * GPIO10   Output      Xilinx Programming Interface X_CCLK
 * GPIO11   Input       Xilinx Programming Interface X_DOUT
 * GPIO12   Output      Xilinx Programming Interface X_DIN
 * GPIO13   Output      Xilinx Programming Interface X_PGM_L
 * GPIO14   Bidir       Xilinx Programming Interface X_DONE
 * GPIO15   Bidir       Xilinx Programming Interface X_INIT_L
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
#define GPIO_XILINX_RTS_TSTROBE	1
#define GPIO_XILINX_IO_ADP0	2
#define GPIO_XILINX_IO_ADP1	3
#define GPIO_XILINX_IO_ADP2	4
#define GPIO_XILINX_IO_ADP3	5
#define GPIO_XILINX_GPIO_A	6
#define GPIO_XILINX_GPIO_B	7
#define GPIO_NONMASKABLE_INT	8
#define GPIO_TEMP_SENSOR_INT	9
#define GPIO_XPROG_X_CCLK	10
#define GPIO_XPROG_X_DOUT	11
#define GPIO_XPROG_X_DIN	12
#define GPIO_XPROG_X_PGM_L	13
#define GPIO_XPROG_X_DONE	14
#define GPIO_XPROG_X_INIT_L	14


#define M_GPIO_DEBUG_LED	_SB_MAKEMASK1(GPIO_DEBUG_LED)
#define M_GPIO_XILINX_RTS_TSTROBE _SB_MAKEMASK1(GPIO_XILINX_RTS_TSTROBE)
#define M_GPIO_XILINX_IO_ADP0	_SB_MAKEMASK1(GPIO_XILINX_IO_ADP0)
#define M_GPIO_XILINX_IO_ADP1	_SB_MAKEMASK1(GPIO_XILINX_IO_ADP1)
#define M_GPIO_XILINX_IO_ADP2	_SB_MAKEMASK1(GPIO_XILINX_IO_ADP2)
#define M_GPIO_XILINX_IO_ADP3	_SB_MAKEMASK1(GPIO_XILINX_IO_ADP3)
#define M_GPIO_XILINX_GPIO_A	_SB_MAKEMASK1(GPIO_XILINX_GPIO_A)
#define M_GPIO_XILINX_GPIO_B	_SB_MAKEMASK1(GPIO_XILINX_GPIO_B)
#define M_GPIO_NONMASKABLE_INT	_SB_MAKEMASK1(GPIO_NONMASKABLE_INT)
#define M_GPIO_TEMP_SENSOR_INT	_SB_MAKEMASK1(GPIO_TEMP_SENSOR_INT)
#define M_GPIO_XPROG_X_CCLK	_SB_MAKEMASK1(GPIO_XPROG_X_CCLK)
#define M_GPIO_XPROG_X_DOUT	_SB_MAKEMASK1(GPIO_XPROG_X_DOUT)
#define M_GPIO_XPROG_X_DIN	_SB_MAKEMASK1(GPIO_XPROG_X_DIN)
#define M_GPIO_XPROG_X_PGM_L	_SB_MAKEMASK1(GPIO_XPROG_X_PGM_L)
#define M_GPIO_XPROG_X_DONE	_SB_MAKEMASK1(GPIO_XPROG_X_DONE)
#define M_GPIO_XPROG_X_INIT_L	_SB_MAKEMASK1(GPIO_XPROG_X_INIT_L)

/* Leave bidirectional pins in "input" state at boot. */

#define GPIO_OUTPUT_MASK (M_GPIO_DEBUG_LED | \
                          M_GPIO_XILINX_RTS_TSTROBE | \
                          M_GPIO_XPROG_X_CCLK | \
                          M_GPIO_XPROG_X_DIN | \
                          M_GPIO_XPROG_X_PGM_L)


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


/*
 * Xilinx:  non-multiplexed, byte width, no parity, no ack
 */
#define XILINX_CS		2
#define XILINX_PHYS		0x1F400000	/* address of Xilinx (CS2) */
#define XILINX_SIZE		NUM64K(4*MB)	/* size of Xilinx */
#define XILINX_TIMING0		V_IO_ALE_WIDTH(4) | \
                                V_IO_ALE_TO_CS(2) | \
                                V_IO_CS_WIDTH(24) | \
                                V_IO_RDY_SMPLE(1)
#define XILINX_TIMING1		V_IO_ALE_TO_WRITE(7) | \
                                V_IO_WRITE_WIDTH(7) | \
                                V_IO_IDLE_CYCLE(6) | \
                                V_IO_CS_TO_OE(0) | \
                                V_IO_OE_TO_CS(0)
#define XILINX_CONFIG		V_IO_WIDTH_SEL(K_IO_WIDTH_SEL_1) | M_IO_NONMUX



/* Memory base for JTAG console registers (near end of the JTAG
   region) */
#define C3_JTAG_CONS_BASE      (K_SCD_JTAG_MEMBASE+K_SCD_JTAG_MEMSIZE-0x80)

#define TEMPSENSOR_SMBUS_CHAN	0
#define TEMPSENSOR_SMBUS_DEV	0x2A
#define BIGEEPROM_SMBUS_CHAN	0
#define BIGEEPROM_SMBUS_DEV	0x50
#define X1240_SMBUS_CHAN	1
#define X1240_SMBUS_DEV		0x50
