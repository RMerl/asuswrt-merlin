/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Generic Bus Constants                     File: sb1250_genbus.h
    *  
    *  This module contains constants and macros useful for 
    *  manipulating the SB1250's Generic Bus interface
    *  
    *  SB1250 specification level:  User's manual 1/02/02
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


#ifndef _SB1250_GENBUS_H
#define _SB1250_GENBUS_H

#include "sb1250_defs.h"

/*
 * Generic Bus Region Configuration Registers (Table 11-4)
 */

#define S_IO_RDY_ACTIVE         0
#define M_IO_RDY_ACTIVE		_SB_MAKEMASK1(S_IO_RDY_ACTIVE)

#define S_IO_ENA_RDY            1
#define M_IO_ENA_RDY		_SB_MAKEMASK1(S_IO_ENA_RDY)

#define S_IO_WIDTH_SEL		2
#define M_IO_WIDTH_SEL		_SB_MAKEMASK(2,S_IO_WIDTH_SEL)
#define K_IO_WIDTH_SEL_1	0
#define K_IO_WIDTH_SEL_2	1
#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define K_IO_WIDTH_SEL_1L       2
#endif /* 1250 PASS2 || 112x PASS1 */
#define K_IO_WIDTH_SEL_4	3
#define V_IO_WIDTH_SEL(x)	_SB_MAKEVALUE(x,S_IO_WIDTH_SEL)
#define G_IO_WIDTH_SEL(x)	_SB_GETVALUE(x,S_IO_WIDTH_SEL,M_IO_WIDTH_SEL)

#define S_IO_PARITY_ENA		4
#define M_IO_PARITY_ENA		_SB_MAKEMASK1(S_IO_PARITY_ENA)
#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define S_IO_BURST_EN		5
#define M_IO_BURST_EN		_SB_MAKEMASK1(S_IO_BURST_EN)
#endif /* 1250 PASS2 || 112x PASS1 */
#define S_IO_PARITY_ODD		6
#define M_IO_PARITY_ODD		_SB_MAKEMASK1(S_IO_PARITY_ODD)
#define S_IO_NONMUX		7
#define M_IO_NONMUX		_SB_MAKEMASK1(S_IO_NONMUX)

#define S_IO_TIMEOUT		8
#define M_IO_TIMEOUT		_SB_MAKEMASK(8,S_IO_TIMEOUT)
#define V_IO_TIMEOUT(x)		_SB_MAKEVALUE(x,S_IO_TIMEOUT)
#define G_IO_TIMEOUT(x)		_SB_GETVALUE(x,S_IO_TIMEOUT,M_IO_TIMEOUT)

/*
 * Generic Bus Region Size register (Table 11-5)
 */

#define S_IO_MULT_SIZE		0
#define M_IO_MULT_SIZE		_SB_MAKEMASK(12,S_IO_MULT_SIZE)
#define V_IO_MULT_SIZE(x)	_SB_MAKEVALUE(x,S_IO_MULT_SIZE)
#define G_IO_MULT_SIZE(x)	_SB_GETVALUE(x,S_IO_MULT_SIZE,M_IO_MULT_SIZE)

#define S_IO_REGSIZE		16	 /* # bits to shift size for this reg */

/*
 * Generic Bus Region Address (Table 11-6)
 */

#define S_IO_START_ADDR		0
#define M_IO_START_ADDR		_SB_MAKEMASK(14,S_IO_START_ADDR)
#define V_IO_START_ADDR(x)	_SB_MAKEVALUE(x,S_IO_START_ADDR)
#define G_IO_START_ADDR(x)	_SB_GETVALUE(x,S_IO_START_ADDR,M_IO_START_ADDR)

#define S_IO_ADDRBASE		16	 /* # bits to shift addr for this reg */

/*
 * Generic Bus Region 0 Timing Registers (Table 11-7)
 */

#define S_IO_ALE_WIDTH		0
#define M_IO_ALE_WIDTH		_SB_MAKEMASK(3,S_IO_ALE_WIDTH)
#define V_IO_ALE_WIDTH(x)	_SB_MAKEVALUE(x,S_IO_ALE_WIDTH)
#define G_IO_ALE_WIDTH(x)	_SB_GETVALUE(x,S_IO_ALE_WIDTH,M_IO_ALE_WIDTH)

#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define M_IO_EARLY_CS	        _SB_MAKEMASK1(3)
#endif /* 1250 PASS2 || 112x PASS1 */

#define S_IO_ALE_TO_CS		4
#define M_IO_ALE_TO_CS		_SB_MAKEMASK(2,S_IO_ALE_TO_CS)
#define V_IO_ALE_TO_CS(x)	_SB_MAKEVALUE(x,S_IO_ALE_TO_CS)
#define G_IO_ALE_TO_CS(x)	_SB_GETVALUE(x,S_IO_ALE_TO_CS,M_IO_ALE_TO_CS)

#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define S_IO_BURST_WIDTH           _SB_MAKE64(6)
#define M_IO_BURST_WIDTH           _SB_MAKEMASK(2,S_IO_BURST_WIDTH)
#define V_IO_BURST_WIDTH(x)        _SB_MAKEVALUE(x,S_IO_BURST_WIDTH)
#define G_IO_BURST_WIDTH(x)        _SB_GETVALUE(x,S_IO_BURST_WIDTH,M_IO_BURST_WIDTH)
#endif /* 1250 PASS2 || 112x PASS1 */

#define S_IO_CS_WIDTH		8
#define M_IO_CS_WIDTH		_SB_MAKEMASK(5,S_IO_CS_WIDTH)
#define V_IO_CS_WIDTH(x)	_SB_MAKEVALUE(x,S_IO_CS_WIDTH)
#define G_IO_CS_WIDTH(x)	_SB_GETVALUE(x,S_IO_CS_WIDTH,M_IO_CS_WIDTH)

#define S_IO_RDY_SMPLE		13
#define M_IO_RDY_SMPLE		_SB_MAKEMASK(3,S_IO_RDY_SMPLE)
#define V_IO_RDY_SMPLE(x)	_SB_MAKEVALUE(x,S_IO_RDY_SMPLE)
#define G_IO_RDY_SMPLE(x)	_SB_GETVALUE(x,S_IO_RDY_SMPLE,M_IO_RDY_SMPLE)


/*
 * Generic Bus Timing 1 Registers (Table 11-8)
 */

#define S_IO_ALE_TO_WRITE	0
#define M_IO_ALE_TO_WRITE	_SB_MAKEMASK(3,S_IO_ALE_TO_WRITE)
#define V_IO_ALE_TO_WRITE(x)	_SB_MAKEVALUE(x,S_IO_ALE_TO_WRITE)
#define G_IO_ALE_TO_WRITE(x)	_SB_GETVALUE(x,S_IO_ALE_TO_WRITE,M_IO_ALE_TO_WRITE)

#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define M_IO_RDY_SYNC	        _SB_MAKEMASK1(3)
#endif /* 1250 PASS2 || 112x PASS1 */

#define S_IO_WRITE_WIDTH	4
#define M_IO_WRITE_WIDTH	_SB_MAKEMASK(4,S_IO_WRITE_WIDTH)
#define V_IO_WRITE_WIDTH(x)	_SB_MAKEVALUE(x,S_IO_WRITE_WIDTH)
#define G_IO_WRITE_WIDTH(x)	_SB_GETVALUE(x,S_IO_WRITE_WIDTH,M_IO_WRITE_WIDTH)

#define S_IO_IDLE_CYCLE		8
#define M_IO_IDLE_CYCLE		_SB_MAKEMASK(4,S_IO_IDLE_CYCLE)
#define V_IO_IDLE_CYCLE(x)	_SB_MAKEVALUE(x,S_IO_IDLE_CYCLE)
#define G_IO_IDLE_CYCLE(x)	_SB_GETVALUE(x,S_IO_IDLE_CYCLE,M_IO_IDLE_CYCLE)

#define S_IO_OE_TO_CS		12
#define M_IO_OE_TO_CS		_SB_MAKEMASK(2,S_IO_OE_TO_CS)
#define V_IO_OE_TO_CS(x)	_SB_MAKEVALUE(x,S_IO_OE_TO_CS)
#define G_IO_OE_TO_CS(x)	_SB_GETVALUE(x,S_IO_OE_TO_CS,M_IO_OE_TO_CS)

#define S_IO_CS_TO_OE		14
#define M_IO_CS_TO_OE		_SB_MAKEMASK(2,S_IO_CS_TO_OE)
#define V_IO_CS_TO_OE(x)	_SB_MAKEVALUE(x,S_IO_CS_TO_OE)
#define G_IO_CS_TO_OE(x)	_SB_GETVALUE(x,S_IO_CS_TO_OE,M_IO_CS_TO_OE)

/*
 * Generic Bus Interrupt Status Register (Table 11-9)
 */

#define M_IO_CS_ERR_INT		_SB_MAKEMASK(0,8)
#define M_IO_CS0_ERR_INT	_SB_MAKEMASK1(0)
#define M_IO_CS1_ERR_INT	_SB_MAKEMASK1(1)
#define M_IO_CS2_ERR_INT	_SB_MAKEMASK1(2)
#define M_IO_CS3_ERR_INT	_SB_MAKEMASK1(3)
#define M_IO_CS4_ERR_INT	_SB_MAKEMASK1(4)
#define M_IO_CS5_ERR_INT	_SB_MAKEMASK1(5)
#define M_IO_CS6_ERR_INT	_SB_MAKEMASK1(6)
#define M_IO_CS7_ERR_INT	_SB_MAKEMASK1(7)

#define M_IO_RD_PAR_INT		_SB_MAKEMASK1(9)
#define M_IO_TIMEOUT_INT	_SB_MAKEMASK1(10)
#define M_IO_ILL_ADDR_INT	_SB_MAKEMASK1(11)
#define M_IO_MULT_CS_INT	_SB_MAKEMASK1(12)
#if SIBYTE_HDR_FEATURE(1250, PASS2) || SIBYTE_HDR_FEATURE(112x, PASS1)
#define M_IO_COH_ERR	        _SB_MAKEMASK1(14)
#endif /* 1250 PASS2 || 112x PASS1 */

/*
 * PCMCIA configuration register (Table 12-6)
 */

#define M_PCMCIA_CFG_ATTRMEM	_SB_MAKEMASK1(0)
#define M_PCMCIA_CFG_3VEN	_SB_MAKEMASK1(1)
#define M_PCMCIA_CFG_5VEN	_SB_MAKEMASK1(2)
#define M_PCMCIA_CFG_VPPEN	_SB_MAKEMASK1(3)
#define M_PCMCIA_CFG_RESET	_SB_MAKEMASK1(4)
#define M_PCMCIA_CFG_APWRONEN	_SB_MAKEMASK1(5)
#define M_PCMCIA_CFG_CDMASK	_SB_MAKEMASK1(6)
#define M_PCMCIA_CFG_WPMASK	_SB_MAKEMASK1(7)
#define M_PCMCIA_CFG_RDYMASK	_SB_MAKEMASK1(8)
#define M_PCMCIA_CFG_PWRCTL	_SB_MAKEMASK1(9)

/*
 * PCMCIA status register (Table 12-7)
 */

#define M_PCMCIA_STATUS_CD1	_SB_MAKEMASK1(0)
#define M_PCMCIA_STATUS_CD2	_SB_MAKEMASK1(1)
#define M_PCMCIA_STATUS_VS1	_SB_MAKEMASK1(2)
#define M_PCMCIA_STATUS_VS2	_SB_MAKEMASK1(3)
#define M_PCMCIA_STATUS_WP	_SB_MAKEMASK1(4)
#define M_PCMCIA_STATUS_RDY	_SB_MAKEMASK1(5)
#define M_PCMCIA_STATUS_3VEN	_SB_MAKEMASK1(6)
#define M_PCMCIA_STATUS_5VEN	_SB_MAKEMASK1(7)
#define M_PCMCIA_STATUS_CDCHG	_SB_MAKEMASK1(8)
#define M_PCMCIA_STATUS_WPCHG	_SB_MAKEMASK1(9)
#define M_PCMCIA_STATUS_RDYCHG	_SB_MAKEMASK1(10)

/*
 * GPIO Interrupt Type Register (table 13-3)
 */

#define K_GPIO_INTR_DISABLE	0
#define K_GPIO_INTR_EDGE	1
#define K_GPIO_INTR_LEVEL	2
#define K_GPIO_INTR_SPLIT	3

#define S_GPIO_INTR_TYPEX(n)	(((n)/2)*2)
#define M_GPIO_INTR_TYPEX(n)	_SB_MAKEMASK(2,S_GPIO_INTR_TYPEX(n))
#define V_GPIO_INTR_TYPEX(n,x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPEX(n))
#define G_GPIO_INTR_TYPEX(n,x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPEX(n),M_GPIO_INTR_TYPEX(n))

#define S_GPIO_INTR_TYPE0	0
#define M_GPIO_INTR_TYPE0	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE0)
#define V_GPIO_INTR_TYPE0(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE0)
#define G_GPIO_INTR_TYPE0(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE0,M_GPIO_INTR_TYPE0)

#define S_GPIO_INTR_TYPE2	2
#define M_GPIO_INTR_TYPE2	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE2)
#define V_GPIO_INTR_TYPE2(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE2)
#define G_GPIO_INTR_TYPE2(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE2,M_GPIO_INTR_TYPE2)

#define S_GPIO_INTR_TYPE4	4
#define M_GPIO_INTR_TYPE4	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE4)
#define V_GPIO_INTR_TYPE4(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE4)
#define G_GPIO_INTR_TYPE4(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE4,M_GPIO_INTR_TYPE4)

#define S_GPIO_INTR_TYPE6	6
#define M_GPIO_INTR_TYPE6	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE6)
#define V_GPIO_INTR_TYPE6(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE6)
#define G_GPIO_INTR_TYPE6(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE6,M_GPIO_INTR_TYPE6)

#define S_GPIO_INTR_TYPE8	8
#define M_GPIO_INTR_TYPE8	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE8)
#define V_GPIO_INTR_TYPE8(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE8)
#define G_GPIO_INTR_TYPE8(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE8,M_GPIO_INTR_TYPE8)

#define S_GPIO_INTR_TYPE10	10
#define M_GPIO_INTR_TYPE10	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE10)
#define V_GPIO_INTR_TYPE10(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE10)
#define G_GPIO_INTR_TYPE10(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE10,M_GPIO_INTR_TYPE10)

#define S_GPIO_INTR_TYPE12	12
#define M_GPIO_INTR_TYPE12	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE12)
#define V_GPIO_INTR_TYPE12(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE12)
#define G_GPIO_INTR_TYPE12(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE12,M_GPIO_INTR_TYPE12)

#define S_GPIO_INTR_TYPE14	14
#define M_GPIO_INTR_TYPE14	_SB_MAKEMASK(2,S_GPIO_INTR_TYPE14)
#define V_GPIO_INTR_TYPE14(x)	_SB_MAKEVALUE(x,S_GPIO_INTR_TYPE14)
#define G_GPIO_INTR_TYPE14(x)	_SB_GETVALUE(x,S_GPIO_INTR_TYPE14,M_GPIO_INTR_TYPE14)


#endif
