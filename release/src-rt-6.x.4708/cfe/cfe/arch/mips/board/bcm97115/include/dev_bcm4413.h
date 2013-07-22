/*
    EXTERNAL SOURCE RELEASE on 08/16/2001 2.34.0.2 - Subject to change without notice.

*/
/*
    Copyright 2001, Broadcom Corporation
    All Rights Reserved.

    This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
    the contents of this file may not be disclosed to third parties, copied or
    duplicated in any form, in whole or in part, without the prior written
    permission of Broadcom Corporation.


*/
/*
 * Hardware-specific definitions for
 * "Lassen" BCM44XX PCI 10/100 Mbit/s Ethernet chip.
 *
 * This is an "Epigram" PCI/DMA engine bolted onto the ISB
 * atop EMAC and EPHY cores from the BCM3352.
 *
 * Copyright(c) 2000 Broadcom Corporation
 *
 * $Id: dev_bcm4413.h 241205 2011-02-17 21:57:41Z $
 */

#ifndef _BCMENET_H
#define	_BCMENET_H

#include "lib_types.h"

#define	BCMENET_NFILTERS	64		/* # ethernet address filter entries */
#define	BCMENET_MCHASHBASE	0x200		/* multicast hash filter base address */
#define	BCMENET_MCHASHSIZE	256		/* multicast hash filter size in bytes */
#define	BCMENET_MAX_DMA		4096		/* chip has 12 bits of DMA addressing */

/* power management wakeup pattern constants */
#define	BCMENET_NPMP		4		/* chip supports 4 wakeup patterns */
#define	BCMENET_PMPBASE		0x400		/* wakeup pattern base address */
#define	BCMENET_PMPSIZE		0x80		/* 128bytes each pattern */
#define	BCMENET_PMMBASE		0x600		/* wakeup mask base address */
#define	BCMENET_PMMSIZE		0x10		/* 128bits each mask */

/* cpp contortions to concatenate w/arg prescan */
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)

/*
 * EMAC MIB Registers
 */
typedef volatile struct {
	uint32_t tx_good_octets;
	uint32_t tx_good_pkts;
	uint32_t tx_octets;
	uint32_t tx_pkts;
	uint32_t tx_broadcast_pkts;
	uint32_t tx_multicast_pkts;
	uint32_t tx_len_64;
	uint32_t tx_len_65_to_127;
	uint32_t tx_len_128_to_255;
	uint32_t tx_len_256_to_511;
	uint32_t tx_len_512_to_1023;
	uint32_t tx_len_1024_to_max;
	uint32_t tx_jabber_pkts;
	uint32_t tx_oversize_pkts;
	uint32_t tx_fragment_pkts;
	uint32_t tx_underruns;
	uint32_t tx_total_cols;
	uint32_t tx_single_cols;
	uint32_t tx_multiple_cols;
	uint32_t tx_excessive_cols;
	uint32_t tx_late_cols;
	uint32_t tx_defered;
	uint32_t tx_carrier_lost;
	uint32_t tx_pause_pkts;
	uint32_t PAD[8];

	uint32_t rx_good_octets;
	uint32_t rx_good_pkts;
	uint32_t rx_octets;
	uint32_t rx_pkts;
	uint32_t rx_broadcast_pkts;
	uint32_t rx_multicast_pkts;
	uint32_t rx_len_64;
	uint32_t rx_len_65_to_127;
	uint32_t rx_len_128_to_255;
	uint32_t rx_len_256_to_511;
	uint32_t rx_len_512_to_1023;
	uint32_t rx_len_1024_to_max;
	uint32_t rx_jabber_pkts;
	uint32_t rx_oversize_pkts;
	uint32_t rx_fragment_pkts;
	uint32_t rx_missed_pkts;
	uint32_t rx_crc_align_errs;
	uint32_t rx_undersize;
	uint32_t rx_crc_errs;
	uint32_t rx_align_errs;
	uint32_t rx_symbol_errs;
	uint32_t rx_pause_pkts;
	uint32_t rx_nonpause_pkts;
} bcmenetmib_t;

/*
 * Host Interface Registers
 */
typedef volatile struct _bcmenetregs {
	/* Device and Power Control */
	uint32_t	devcontrol;
	uint32_t	devstatus;
	//uint32_t	PAD[2];
	uint32_t  compatcontrol;
	uint32_t  biststatus;
	uint32_t	wakeuplength;
	uint32_t	PAD[3];

	/* Interrupt Control */
	uint32_t	intstatus;
	uint32_t	intmask;
	uint32_t	gptimer;
	uint32_t	PAD[23];

	/* Ethernet MAC Address Filtering Control */
	uint32_t	enetaddrlower;
	uint32_t	enetaddrupper;
	uint32_t	enetftaddr;
	uint32_t	enetftdata;
	uint32_t	PAD[4];

	/* Ethernet MAC Control */
	uint32_t	emaccontrol;
	uint32_t	emacflowcontrol;

	/* GPIO Interface */
	uint32_t	gpiooutput;
	uint32_t	gpioouten;
	uint32_t	gpioinput;
	uint32_t	PAD;

	/* Direct FIFO Interface */
	uint16_t	xmtfifocontrol;
	uint16_t	xmtfifodata;
	uint16_t	rcvfifocontrol;
	uint16_t	rcvfifodata;
	uint32_t	PAD[2];

	/* CardBus Function Registers */
	uint32_t	funcevnt;
	uint32_t	funcevntmask;
	uint32_t	funcstate;
	uint32_t	funcforce;

	uint32_t	PAD[8];

	/* DMA Lazy Interrupt Control */
	uint32_t	intrecvlazy;
	uint32_t	PAD[31];

	/* Shadow PCI config */
	uint16_t	pcicfg[64];

	/* Transmit DMA engine */
	uint32_t	xmtcontrol;
	uint32_t	xmtaddr;
	uint32_t	xmtptr;
	uint32_t	xmtstatus;

	/* Receive Dma engine */
	uint32_t	rcvcontrol;
	uint32_t	rcvaddr;
	uint32_t	rcvptr;
	uint32_t	rcvstatus;
	uint32_t	PAD[120];

	/* EMAC Registers */
	uint32_t rxcontrol;
	uint32_t rxmaxlength;
	uint32_t txmaxlength;
	uint32_t PAD;
	uint32_t mdiocontrol;
	uint32_t mdiodata;
	uint32_t emacintmask;
	uint32_t emacintstatus;
	uint32_t camdatalo;
	uint32_t camdatahi;
	uint32_t camcontrol;
	uint32_t enetcontrol;
	uint32_t txcontrol;
	uint32_t txwatermark;
	uint32_t mibcontrol;
	uint32_t PAD[49];

	/* EMAC MIB statistic counters */
	bcmenetmib_t	mib;
} bcmenetregs_t;

/* device control */
#define	DC_RS		((uint32_t)1 << 0)	/* emac reset */
#define	DC_XE		((uint32_t)1 << 5)	/* transmit enable */
#define	DC_RE		((uint32_t)1 << 6)	/* receive enable */
#define	DC_PM		((uint32_t)1 << 7)	/* pattern filtering enable */
#define	DC_MD		((uint32_t)1 << 8)	/* emac clock disable */
#define	DC_BS		((uint32_t)1 << 11)	/* msi byte swap */
#define	DC_PD		((uint32_t)1 << 14)	/* ephy clock disable */
#define	DC_PR		((uint32_t)1 << 15)	/* ephy reset */

/* device status */
#define	DS_RI_MASK	0x1f			/* revision id */
#define	DS_MM_MASK	0xe0			/* MSI mode */
#define	DS_MM_SHIFT	5
#define	DS_MM		((uint32_t)1 << 8)	/* msi mode (otherwise pci mode) */
#define	DS_SP		((uint32_t)1 << 9)	/* serial prom present */
#define	DS_SL		((uint32_t)1 << 10)	/* low-order 512 bits of sprom locked */
#define	DS_SS_MASK	0x1800			/* sprom size */
#define	DS_SS_SHIFT	11
#define	DS_SS_1K	0x0800			/*  1 kbit */
#define	DS_SS_4K	0x1000			/*  4 kbit */
#define	DS_SS_16K	0x1800			/* 16 kbit */
#define	DS_RR		((uint32_t)1 << 15)	/* register read ready (4412) */
#define	DS_BO_MASK	0xffff0000		/* bond option id */
#define	DS_BO_SHIFT	16

#define DS_MM_PPC	0x0000			/* 4412 msi subtype==PPC */
#define DS_MM_ARM7T	((uint32_t)1 << DS_MM_SHIFT)/* 4412 msi subtype == ARM7T */
#define DS_MM_M68K	((uint32_t)2 << DS_MM_SHIFT)/* 4412 msi subtype == M68K */
#define DS_MM_GENASYNC	((uint32_t)3 << DS_MM_SHIFT)/* 4412 msi subtype == Generic Async */
#define DS_MM_GENSYNC	((uint32_t)4 << DS_MM_SHIFT)/* 4412 msi subtype == Generic Sync */
#define DS_MM_SH3	((uint32_t)5 << DS_MM_SHIFT)/* 4412 msi subtype == Generic Sync */

/* wakeup length */
#define	WL_P0_MASK	0x7f			/* pattern 0 */
#define	WL_D0		((uint32_t)1 << 7)
#define	WL_P1_MASK	0x7f00			/* pattern 1 */
#define	WL_P1_SHIFT	8
#define	WL_D1		((uint32_t)1 << 15)
#define	WL_P2_MASK	0x7f0000		/* pattern 2 */
#define	WL_P2_SHIFT	16
#define	WL_D2		((uint32_t)1 << 23)
#define	WL_P3_MASK	0x7f000000		/* pattern 3 */
#define	WL_P3_SHIFT	24
#define	WL_D3		((uint32_t)1 << 31)

/* intstatus and intmask */
#define	I_TO		((uint32_t)1 << 7)	/* general purpose timeout */
#define	I_PC		((uint32_t)1 << 10)	/* pci descriptor error */
#define	I_PD		((uint32_t)1 << 11)	/* pci data error */
#define	I_DE		((uint32_t)1 << 12)	/* descriptor protocol error */
#define	I_RU		((uint32_t)1 << 13)	/* receive descriptor underflow */
#define	I_RO		((uint32_t)1 << 14)	/* receive fifo overflow */
#define	I_XU		((uint32_t)1 << 15)	/* transmit fifo underflow */
#define	I_RI		((uint32_t)1 << 16)	/* receive interrupt */
#define	I_XI		((uint32_t)1 << 24)	/* transmit interrupt */
#define	I_EM		((uint32_t)1 << 26)	/* emac interrupt */

/* emaccontrol */
#define	EMC_CG		((uint32_t)1 << 0)	/* crc32 generation enable */
#define	EMC_CK		((uint32_t)1 << 1)	/* clock source: 0=ephy 1=iline */
#define	EMC_PD		((uint32_t)1 << 2)	/* ephy power down */
#define	EMC_PA		((uint32_t)1 << 3)	/* ephy activity status */
#define	EMC_LED_MASK	0xe0			/* ephy led config */
#define	EMC_GPIO_MASK	0xc000			/* gpio pin config */
#define	EMC_GPIO_SHIFT	14

/* emacflowcontrol */
#define	EMF_RFH_MASK	0xff			/* rx fifo hi water mark */
#define	EMF_PG		((uint32_t)1 << 15)	/* enable pause frame generation */

/* gpio output/enable/input */
#define	GPIO_MASK	0x7ff			/* output/enable/input */

/* transmit fifo control */
#define	XFC_BV_MASK	0x3			/* bytes valid */
#define	XFC_LO		(1 << 0)		/* low byte valid */
#define	XFC_HI		(1 << 1)		/* high byte valid */
#define	XFC_BOTH	(XFC_HI | XFC_LO)	/* both bytes valid */
#define	XFC_EF		(1 << 2)		/* end of frame */
#define	XFC_FR		(1 << 3)		/* frame ready */

/* receive fifo control */
#define	RFC_FR		(1 << 0)		/* frame ready */
#define	RFC_DR		(1 << 1)		/* data ready */

/* funcevnt, funcevntmask, funcstate, and funcforce */
#define	F_RD		((uint32_t)1 << 1)	/* card function ready */
#define	F_GW		((uint32_t)1 << 4)	/* general wakeup */
#define	F_WU		((uint32_t)1 << 14)	/* wakeup mask */
#define	F_IR		((uint32_t)1 << 15)	/* interrupt request pending */

/* interrupt receive lazy */
#define	IRL_TO_MASK	0x00ffffff		/* timeout */
#define	IRL_FC_MASK	0xff000000		/* frame count */
#define	IRL_FC_SHIFT	24			/* frame count */

/* transmit channel control */
#define	XC_XE		((uint32_t)1 << 0)	/* transmit enable */
#define	XC_SE		((uint32_t)1 << 1)	/* transmit suspend request */
#define	XC_LE		((uint32_t)1 << 2)	/* dma loopback enable */

/* transmit descriptor table pointer */
#define	XP_LD_MASK	0xfff			/* last valid descriptor */

/* transmit channel status */
#define	XS_CD_MASK	0x0fff			/* current descriptor pointer */
#define	XS_XS_MASK	0xf000			/* transmit state */
#define	XS_XS_SHIFT	12
#define	XS_XS_DISABLED	0x0000			/* disabled */
#define	XS_XS_ACTIVE	0x1000			/* active */
#define	XS_XS_IDLE	0x2000			/* idle wait */
#define	XS_XS_STOPPED	0x3000			/* stopped */
#define	XS_XS_SUSP	0x4000			/* suspended */
#define	XS_XE_MASK	0xf0000			/* transmit errors */
#define	XS_XE_SHIFT	16
#define	XS_XE_NOERR	0x00000			/* no error */
#define	XS_XE_DPE	0x10000			/* descriptor protocol error */
#define	XS_XE_DFU	0x20000			/* data fifo underrun */
#define	XS_XE_PCIBR	0x30000			/* pci bus error on buffer read */
#define	XS_XE_PCIDA	0x40000			/* pci bus error on descriptor access */

/* receive channel control */
#define	RC_RE		((uint32_t)1 << 0)	/* receive enable */
#define	RC_RO_MASK	0x7e			/* receive frame offset */
#define	RC_RO_SHIFT	1
#define	RC_FM		((uint32_t)1 << 8)	/* direct fifo receive mode */

/* receive descriptor table pointer */
#define	RP_LD_MASK	0xfff			/* last valid descriptor */

/* receive channel status */
#define	RS_CD_MASK	0x0fff			/* current descriptor pointer */
#define	RS_RS_MASK	0xf000			/* receive state */
#define	RS_RS_SHIFT	12
#define	RS_RS_DISABLED	0x0000			/* disabled */
#define	RS_RS_ACTIVE	0x1000			/* active */
#define	RS_RS_IDLE	0x2000			/* idle wait */
#define	RS_RS_STOPPED	0x3000			/* reserved */
#define	RS_RE_MASK	0xf0000			/* receive errors */
#define	RS_RE_SHIFT	16
#define	RS_RE_NOERR	0x00000			/* no error */
#define	RS_RE_DPE	0x10000			/* descriptor protocol error */
#define	RS_RE_DFO	0x20000			/* data fifo overflow */
#define	RS_RE_PCIBW	0x30000			/* pci bus error on buffer write */
#define	RS_RE_PCIDA	0x40000			/* pci bus error on descriptor access */

/* emac receive control */
#define	ERC_DB		((uint32_t)1 << 0)	/* disable broadcast */
#define	ERC_AM		((uint32_t)1 << 1)	/* accept all multicast */
#define	ERC_RDT		((uint32_t)1 << 2)	/* receive disable while transmitting */
#define	ERC_PE		((uint32_t)1 << 3)	/* promiscuous enable */
#define	ERC_LE		((uint32_t)1 << 4)	/* loopback enable */
#define	ERC_FE		((uint32_t)1 << 5)	/* enable flow control */
#define	ERC_UF		((uint32_t)1 << 6)	/* accept unicast flow control frame */

/* emac mdio control */
#define	MC_MF_MASK	0x7f			/* mdc frequency */
#define	MC_PE		((uint32_t)1 << 7)	/* mii preamble enable */

/* emac mdio data */
#define	MD_DATA_MASK	0xffff			/* r/w data */
#define	MD_TA_MASK	0x30000			/* turnaround value */
#define	MD_TA_SHIFT	16
#define	MD_TA_VALID	(2 << MD_TA_SHIFT)	/* valid ta */
#define	MD_RA_MASK	0x7c0000		/* register address */
#define	MD_RA_SHIFT	18
#define	MD_PMD_MASK	0xf800000		/* physical media device */
#define	MD_PMD_SHIFT	23
#define	MD_OP_MASK	0x30000000		/* opcode */
#define	MD_OP_SHIFT	28
#define	MD_OP_WRITE	(1 << MD_OP_SHIFT)	/* write op */
#define	MD_OP_READ	(2 << MD_OP_SHIFT)	/* read op */
#define	MD_SB_MASK	0xc0000000		/* start bits */
#define	MD_SB_SHIFT	30
#define	MD_SB_START	(0x1 << MD_SB_SHIFT)	/* start of frame */

/* emac intstatus and intmask */
#define	EI_MII		((uint32_t)1 << 0)	/* mii mdio interrupt */
#define	EI_MIB		((uint32_t)1 << 1)	/* mib interrupt */
#define	EI_FLOW		((uint32_t)1 << 2)	/* flow control interrupt */

/* emac cam data high */
#define	CD_V		((uint32_t)1 << 16)	/* valid bit */

/* emac cam control */
#define	CC_CE		((uint32_t)1 << 0)	/* cam enable */
#define	CC_MS		((uint32_t)1 << 1)	/* mask select */
#define	CC_RD		((uint32_t)1 << 2)	/* read */
#define	CC_WR		((uint32_t)1 << 3)	/* write */
#define	CC_INDEX_MASK	0x3f0000		/* index */
#define	CC_INDEX_SHIFT	16
#define	CC_CB		((uint32_t)1 << 31)	/* cam busy */

/* emac ethernet control */
#define	EC_EE		((uint32_t)1 << 0)	/* emac enable */
#define	EC_ED		((uint32_t)1 << 1)	/* emac disable */
#define	EC_ES		((uint32_t)1 << 2)	/* emac soft reset */
#define	EC_EP		((uint32_t)1 << 3)	/* external phy select */

/* emac transmit control */
#define	EXC_FD		((uint32_t)1 << 0)	/* full duplex */
#define	EXC_FM		((uint32_t)1 << 1)	/* flowmode */
#define	EXC_SB		((uint32_t)1 << 2)	/* single backoff enable */
#define	EXC_SS		((uint32_t)1 << 3)	/* small slottime */

/* emac mib control */
#define	EMC_RZ		((uint32_t)1 << 0)	/* autoclear on read */

/*
 * Transmit/Receive Ring Descriptor
 */
typedef volatile struct {
	uint32_t	ctrl;		/* misc control bits & bufcount */
	uint32_t	addr;		/* data buffer address */
} bcmenetdd_t;

#define	CTRL_BC_MASK	0x1fff			/* buffer byte count */
#define	CTRL_EOT	((uint32_t)1 << 28)	/* end of descriptor table */
#define	CTRL_IOC	((uint32_t)1 << 29)	/* interrupt on completion */
#define	CTRL_EOF	((uint32_t)1 << 30)	/* end of frame */
#define	CTRL_SOF	((uint32_t)1 << 31)	/* start of frame */

/*
 * Enet receive frame buffer header consists of
 * 16bits of frame length, followed by
 * 16bits of EMAC rx descriptor info, followed by 24bytes of
 * undefined data (where the iline rxhdr would be),
 * followed by the start of ethernet frame (ether_header).
 */
typedef volatile struct {
	uint16_t	len;
	uint16_t	flags;
	uint8_t	pad[24];
} bcmenetrxhdr_t;

#define	RXHDR_LEN	28

#define	RXF_L		((uint16_t)1 << 11)	/* last buffer in a frame */
#define	RXF_MISS	((uint16_t)1 << 7)	/* received due to promisc mode */
#define	RXF_BRDCAST	((uint16_t)1 << 6)	/* dest is broadcast address */
#define	RXF_MULT	((uint16_t)1 << 5)	/* dest is multicast address */
#define	RXF_LG		((uint16_t)1 << 4)	/* frame length > rxmaxlength */
#define	RXF_NO		((uint16_t)1 << 3)	/* odd number of nibbles */
#define	RXF_RXER	((uint16_t)1 << 2)	/* receive symbol error */
#define	RXF_CRC		((uint16_t)1 << 1)	/* crc error */
#define	RXF_OV		((uint16_t)1 << 0)	/* fifo overflow */


#define barrier() __asm__ __volatile__("": : :"memory")

#define	R_REG(r)	((sizeof *(r) == sizeof (uint32_t))? \
	((uint32_t)( *(&((uint16_t*)(r))[0]) | (*(&((uint16_t*)(r))[1]) << 16))) \
	: *(r))
#define	W_REG(r, v)	do { ((sizeof *(r) == sizeof (uint32_t))? \
	(*(&((uint16_t*)r)[0]) = ((v) & 0xffff), *(&((uint16_t*)r)[1]) = (((v) >> 16) & 0xffff)) \
	: (*(r) = (v))); barrier(); } while( 0 )


#define	AND_REG(r, v)	W_REG((r), (R_REG((r)) & (v)))
#define	OR_REG(r, v)	W_REG((r), (R_REG((r)) | (v)))


#endif	/* _BCMENET_H */
