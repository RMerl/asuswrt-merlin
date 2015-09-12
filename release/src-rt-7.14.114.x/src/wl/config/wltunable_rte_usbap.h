/*
 * Broadcom 802.11abg and 802.11ac Networking Device Driver Configuration file for USBAP
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wltunable_rte_usbap.h 302471 2011-12-12 20:06:35Z $
 *
 * wl_high driver tunables for 43236b1 and 43526b BMAC rte dev
 */

#define D11CONF		0x01000000	/* D11 Core Rev 24 (43236b1) */
#define D11CONF2	0x00000C00	/* D11 Core Rev > 31, 42(43526b), 43(43556b1) */

/* common tunable settings */
#define CTFPOOLSZ       384	/* max buffers in ctfpool */
#define WLC_DATAHIWAT		10
#define RXBND			16

/* 43236 specific tunable settings */
#define NTXD_USB_43236		64
#define NRPCTXBUFPOST_USB_43236	64
#define NRXD_USB_43236		32
#define NRXBUFPOST_USB_43236	16

/* 43526 specific tunable settings */
#define NTXD_USB_43526		128
#define NRPCTXBUFPOST_USB_43526	128
#define NRXD_USB_43526		128
#define NRXBUFPOST_USB_43526	64

/* 43556 specific tunable settings */
#define NTXD_USB_43556		128
#define NRPCTXBUFPOST_USB_43556	128
#define NRXD_USB_43556		128
#define NRXBUFPOST_USB_43556	64
