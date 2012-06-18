/*
 * mii.h 1.4 2000/04/25 22:06:15
 *
 * Media Independent Interface support: register layout and ioctl's
 *
 * Copyright (C) 2000 David A. Hinds -- dhinds@pcmcia.sourceforge.org
 */

#ifndef _LINUX_MII_H
#define _LINUX_MII_H

/* network interface ioctl's for MII commands */
#ifndef SIOCGMIIPHY
#define SIOCGMIIPHY (SIOCDEVPRIVATE)	/* Read from current PHY */
#define SIOCGMIIREG (SIOCDEVPRIVATE+1) 	/* Read any PHY register */
#define SIOCSMIIREG (SIOCDEVPRIVATE+2) 	/* Write any PHY register */
#define SIOCGPARAMS (SIOCDEVPRIVATE+3) 	/* Read operational parameters */
#define SIOCSPARAMS (SIOCDEVPRIVATE+4) 	/* Set operational parameters */
#endif

#include <linux/types.h>

/* This data structure is used for all the MII ioctl's */
struct mii_data {
    __u16	phy_id;
    __u16	reg_num;
    __u16	val_in;
    __u16	val_out;
};

/* Basic Mode Control Register */
#define MII_BMCR		0x00
#define  MII_BMCR_RESET		0x8000
#define  MII_BMCR_LOOPBACK	0x4000
#define  MII_BMCR_100MBIT	0x2000
#define  MII_BMCR_AN_ENA	0x1000
#define  MII_BMCR_POWERDONW	0x0800
#define  MII_BMCR_ISOLATE	0x0400
#define  MII_BMCR_RESTART	0x0200
#define  MII_BMCR_DUPLEX	0x0100
#define  MII_BMCR_COLTEST	0x0080
#define  MII_BMCR_1000MBIT	0x0040

/* Basic Mode Status Register */
#define MII_BMSR		0x01
#define  MII_BMSR_CAP_MASK	0xf800
#define  MII_BMSR_100BASET4	0x8000	/* Can do 100mbps, 4k packets  */
#define  MII_BMSR_100BASETX_FD	0x4000	/* Can do 100mbps, full-duplex */
#define  MII_BMSR_100BASETX_HD	0x2000	/* Can do 100mbps, half-duplex */
#define  MII_BMSR_10BASET_FD	0x1000	/* Can do 10mbps, full-duplex  */
#define  MII_BMSR_10BASET_HD	0x0800	/* Can do 10mbps, half-duplex  */
#define  MII_BMSR_100FULL2	0x0400	/* Can do 100BASE-T2 FDX */
#define  MII_BMSR_100HALF2	0x0200	/* Can do 100BASE-T2 HDX */
#define  MII_BMSR_ESTATEN	0x0100  /* Extended Status in R15 */
#define  MII_BMSR_NO_PREAMBLE	0x0040
#define  MII_BMSR_AN_COMPLETE	0x0020	/* Auto-negotiation complete   */
#define  MII_BMSR_REMOTE_FAULT	0x0010	/* Remote fault detected       */
#define  MII_BMSR_AN_ABLE	0x0008	/* Able to do auto-negotiation */
#define  MII_BMSR_LINK_VALID	0x0004	/* Link status                 */
#define  MII_BMSR_JABBER	0x0002	/* Jabber detected             */
#define  MII_BMSR_ERCAP		0x0001	/* Ext-reg capability          */

#define MII_PHY_ID1		0x02
#define MII_PHY_ID2		0x03

/* Auto-Negotiation Advertisement Register */
#define MII_ANAR		0x04
/* Auto-Negotiation Link Partner Ability Register */
#define MII_ANLPAR		0x05
#define  MII_AN_NEXT_PAGE	0x8000
#define  MII_AN_ACK		0x4000
#define  MII_AN_REMOTE_FAULT	0x2000
#define  MII_AN_ABILITY_MASK	(MII_AN_FLOW_CONTROL|MII_AN_100BASET4|MII_AN_100BASETX_FD|MII_AN_100BASETX_HD|MII_AN_10BASET_FD|MII_AN_10BASET_HD)
#define  MII_AN_FLOW_CONTROL	0x0400
#define  MII_AN_100BASET4	0x0200
#define  MII_AN_100BASETX_FD	0x0100
#define  MII_AN_100BASETX_HD	0x0080
#define  MII_AN_10BASET_FD	0x0040
#define  MII_AN_10BASET_HD	0x0020
#define  MII_AN_PROT_MASK	0x001f
#define  MII_AN_PROT_802_3	0x0001

/* Auto-Negotiation Expansion Register */
#define MII_ANER		0x06
#define  MII_ANER_MULT_FAULT	0x0010
#define  MII_ANER_LP_NP_ABLE	0x0008
#define  MII_ANER_NP_ABLE	0x0004
#define  MII_ANER_PAGE_RX	0x0002
#define  MII_ANER_LP_AN_ABLE	0x0001

/* Gigabit Registers */
#define MII_CTRL1000		0x09
#define   MII_AN2_UNKNOWN1	0x0800
#define   MII_AN2_UNKNOWN2	0x0400
#define   MII_AN2_1000FULL	0x0200
#define   MII_AN2_1000HALF	0x0100

#define MII_STAT1000		0x0a
#define   MII_LPA2_UNKNOWN1	0x4000
#define   MII_LPA2_1000LOCALOK	0x2000
#define   MII_LPA2_1000REMRXOK	0x1000
#define   MII_LPA2_1000FULL	0x0800
#define   MII_LPA2_1000HALF	0x0400

#define MII_ESTAT1000		0x0f
#define   MII_EST_1000THALF	0x1000		/* 1000BASE-T half duplex capable */
#define   MII_EST_1000TFULL	0x2000		/* 1000BASE-T full duplex capable */
#define   MII_EST_1000XHALF	0x4000		/* 1000BASE-X half duplex capable */
#define   MII_EST_1000XFULL	0x8000		/* 1000BASE-X full duplex capable */

/* Last register we need for show_basic_mii() */
#define MII_BASIC_MAX		MII_STAT1000

#define MII_MAXPHYREG		32
#define EPHY_NONE		31	/* nvram: no phy present at all */
#define EPHY_NOREG		30	/* nvram: no local phy regs */

#endif /* _LINUX_MII_H */
