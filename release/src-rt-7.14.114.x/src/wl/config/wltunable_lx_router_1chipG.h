/*
 * Broadcom 802.11abg Networking Device Driver Configuration file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wltunable_lx_router_1chipG.h 413605 2013-07-19 19:06:36Z $
 *
 * wl driver tunables single-chip (4712/535x) G-only driver
 */

#define D11CONF		0x0080		/* D11 Core Rev 7 (4712/535x) */
#define D11CONF2	0		/* D11 Core Rev > 31 */
#define GCONF		0x0004		/* G-Phy Rev 2 (4712/535x) */

#define WME_PER_AC_TX_PARAMS 1
#define WME_PER_AC_TUNING 1

#define WLRXEXTHDROOM -1	/* to reserve extra headroom in DMA Rx buffer */
