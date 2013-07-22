/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  MII register definitions			File: mii.h
    *  
    *  Register and bit definitions for the standard MII management
    *  interface.
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

#ifndef _MII_H_
#define _MII_H_

/* Access/command codes */

#define	MII_COMMAND_START	0x01
#define	MII_COMMAND_READ	0x02
#define	MII_COMMAND_WRITE	0x01
#define	MII_COMMAND_ACK		0x02


/* Registers */

#define	MII_BMCR	0x00 	/* Basic Mode Control (rw) */
#define	MII_BMSR	0x01	/* Basic Mode Status (ro) */
#define MII_PHYIDR1	0x02
#define MII_PHYIDR2	0x03
#define MII_ANAR	0x04	/* Autonegotiation Advertisement */
#define	MII_ANLPAR	0x05	/* Autonegotiation Link Partner Ability (rw) */
#define MII_ANER	0x06	/* Autonegotiation Expansion */
#define MII_K1CTL	0x09	/* 1000baseT control */
#define MII_K1STSR	0x0A	/* 1K Status Register (ro) */
#define MII_AUXCTL	0x18	/* aux control register */
#define MII_SHADOW	0x1C	/* shadow control register */

/* Basic Mode Control register (RW) */

#define BMCR_RESET		0x8000
#define BMCR_LOOPBACK		0x4000
#define BMCR_SPEED0		0x2000
#define BMCR_ANENABLE		0x1000
#define BMCR_POWERDOWN		0x0800
#define BMCR_ISOLATE		0x0400
#define BMCR_RESTARTAN		0x0200
#define BMCR_DUPLEX		0x0100
#define BMCR_COLTEST		0x0080
#define BMCR_SPEED1		0x0040
#define BMCR_SPEED1000		(BMCR_SPEED1)
#define BMCR_SPEED100		(BMCR_SPEED0)
#define BMCR_SPEED10		0


/* Basic Mode Status register (RO) */

#define BMSR_100BT4		0x8000
#define BMSR_100BT_FDX		0x4000
#define BMSR_100BT_HDX  	0x2000
#define BMSR_10BT_FDX   	0x1000
#define BMSR_10BT_HDX   	0x0800
#define BMSR_100BT2_FDX 	0x0400
#define BMSR_100BT2_HDX 	0x0200
#define BMSR_1000BT_XSR		0x0100
#define BMSR_PRESUP		0x0040
#define BMSR_ANCOMPLETE		0x0020
#define BMSR_REMFAULT		0x0010
#define BMSR_AUTONEG		0x0008
#define BMSR_LINKSTAT		0x0004
#define BMSR_JABDETECT		0x0002
#define BMSR_EXTCAPAB		0x0001


/* PHY Identifer registers (RO) */

#define PHYIDR1 		0x2000
#define PHYIDR2			0x5C60


/* Autonegotiation Advertisement register (RW) */

#define ANAR_NP			0x8000
#define ANAR_RF			0x2000
#define ANAR_ASYPAUSE		0x0800
#define ANAR_PAUSE		0x0400
#define ANAR_T4			0x0200
#define ANAR_TXFD		0x0100
#define ANAR_TXHD		0x0080
#define ANAR_10FD		0x0040
#define ANAR_10HD		0x0020
#define ANAR_PSB		0x001F

#define PSB_802_3		0x0001	/* 802.3 */

/* Autonegotiation Link Partner Abilities register (RW) */

#define ANLPAR_NP		0x8000
#define ANLPAR_ACK		0x4000
#define ANLPAR_RF		0x2000
#define ANLPAR_ASYPAUSE		0x0800
#define ANLPAR_PAUSE		0x0400
#define ANLPAR_T4		0x0200
#define ANLPAR_TXFD		0x0100
#define ANLPAR_TXHD		0x0080
#define ANLPAR_10FD		0x0040
#define ANLPAR_10HD		0x0020
#define ANLPAR_PSB		0x001F


/* Autonegotiation Expansion register (RO) */

#define ANER_PDF		0x0010
#define ANER_LPNPABLE		0x0008
#define ANER_NPABLE		0x0004
#define ANER_PAGERX		0x0002
#define ANER_LPANABLE		0x0001


#define ANNPTR_NP		0x8000
#define ANNPTR_MP		0x2000
#define ANNPTR_ACK2		0x1000
#define ANNPTR_TOGTX		0x0800
#define ANNPTR_CODE		0x0008

#define ANNPRR_NP		0x8000
#define ANNPRR_MP		0x2000
#define ANNPRR_ACK3		0x1000
#define ANNPRR_TOGTX		0x0800
#define ANNPRR_CODE		0x0008


#define K1TCR_TESTMODE		0x0000
#define K1TCR_MSMCE		0x1000
#define K1TCR_MSCV		0x0800
#define K1TCR_RPTR		0x0400
#define K1TCR_1000BT_FDX 	0x200
#define K1TCR_1000BT_HDX 	0x100

#define K1STSR_MSMCFLT		0x8000
#define K1STSR_MSCFGRES		0x4000
#define K1STSR_LRSTAT		0x2000
#define K1STSR_RRSTAT		0x1000
#define K1STSR_LP1KFD		0x0800
#define K1STSR_LP1KHD   	0x0400
#define K1STSR_LPASMDIR		0x0200

#define K1SCR_1KX_FDX		0x8000
#define K1SCR_1KX_HDX		0x4000
#define K1SCR_1KT_FDX		0x2000
#define K1SCR_1KT_HDX		0x1000

/* Shadow Register 0x1C */
#define	SHDW_SPR_CTRL	0x100C	/* shadow (1c) reg 00100 mask */
#define	SHDW_WR_EN	0x8000	/* shadow (1c) write enable bit mask */
#define	SHDW_NRG_DET	0x0002	/* shadow (1c) reg 00100 energy detect bit mask */

#endif /* _MII_H_ */
