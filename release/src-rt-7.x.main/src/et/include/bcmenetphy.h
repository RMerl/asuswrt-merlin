/*
 * Misc Broadcom BCM47XX MDC/MDIO enet phy definitions.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: bcmenetphy.h 376342 2012-12-24 21:02:49Z $
 */

#ifndef	_bcmenetphy_h_
#define	_bcmenetphy_h_

/* phy address */
#define	MAXEPHY		32			/* mdio phy addresses are 5bit quantities */
#define	EPHY_MASK	0x1f			/* phy mask */
#define	EPHY_NONE	31			/* nvram: no phy present at all */
#define	EPHY_NOREG	30			/* nvram: no local phy regs */

#define	MAXPHYREG	32			/* max 32 registers per phy */

/* just a few phy registers */
#define	CTL_RESET	(1 << 15)		/* reset */
#define	CTL_LOOP	(1 << 14)		/* loopback */
#define	CTL_SPEED	(1 << 13)		/* speed selection lsb 0=10, 1=100 */
#define	CTL_ANENAB	(1 << 12)		/* autonegotiation enable */
#define	CTL_RESTART	(1 << 9)		/* restart autonegotiation */
#define	CTL_DUPLEX	(1 << 8)		/* duplex mode 0=half, 1=full */
#define	CTL_SPEED_MSB	(1 << 6)		/* speed selection msb */

#define	CTL_SPEED_10	((0 << 6) | (0 << 13))	/* speed selection CTL.6=0, CTL.13=0 */
#define	CTL_SPEED_100	((0 << 6) | (1 << 13))	/* speed selection CTL.6=0, CTL.13=1 */
#define	CTL_SPEED_1000	((1 << 6) | (0 << 13))	/* speed selection CTL.6=1, CTL.13=0 */

#define	ADV_10FULL	(1 << 6)		/* autonegotiate advertise 10full */
#define	ADV_10HALF	(1 << 5)		/* autonegotiate advertise 10half */
#define	ADV_100FULL	(1 << 8)		/* autonegotiate advertise 100full */
#define	ADV_100HALF	(1 << 7)		/* autonegotiate advertise 100half */

/* link partner ability register */
#define LPA_SLCT	0x001f			/* same as advertise selector */
#define LPA_10HALF	0x0020			/* can do 10mbps half-duplex */
#define LPA_10FULL	0x0040			/* can do 10mbps full-duplex */
#define LPA_100HALF	0x0080			/* can do 100mbps half-duplex */
#define LPA_100FULL	0x0100			/* can do 100mbps full-duplex */
#define LPA_100BASE4	0x0200			/* can do 100mbps 4k packets */
#define LPA_RESV	0x1c00			/* unused */
#define LPA_RFAULT	0x2000			/* link partner faulted */
#define LPA_LPACK	0x4000			/* link partner acked us */
#define LPA_NPAGE	0x8000			/* next page bit */

#define LPA_DUPLEX	(LPA_10FULL | LPA_100FULL)
#define LPA_100		(LPA_100FULL | LPA_100HALF | LPA_100BASE4)

/* 1000BASE-T control register */
#define	ADV_1000HALF	0x0100			/* advertise 1000BASE-T half duplex */
#define	ADV_1000FULL	0x0200			/* advertise 1000BASE-T full duplex */

/* 1000BASE-T status register */
#define	LPA_1000HALF	0x0400			/* link partner 1000BASE-T half duplex */
#define	LPA_1000FULL	0x0800			/* link partner 1000BASE-T full duplex */

/* 1000BASE-T extended status register */
#define	EST_1000THALF	0x1000			/* 1000BASE-T half duplex capable */
#define	EST_1000TFULL	0x2000			/* 1000BASE-T full duplex capable */
#define	EST_1000XHALF	0x4000			/* 1000BASE-X half duplex capable */
#define	EST_1000XFULL	0x8000			/* 1000BASE-X full duplex capable */

#define	STAT_REMFAULT	(1 << 4)		/* remote fault */
#define	STAT_LINK	(1 << 2)		/* link status */
#define	STAT_JAB	(1 << 1)		/* jabber detected */
#define	AUX_FORCED	(1 << 2)		/* forced 10/100 */
#define	AUX_SPEED	(1 << 1)		/* speed 0=10mbps 1=100mbps */
#define	AUX_DUPLEX	(1 << 0)		/* duplex 0=half 1=full */

#endif	/* _bcmenetphy_h_ */
