/*
 * Broadcom 802.11abg Networking Device Driver Configuration file
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 *
 * $Id: wltunable_lx_router_mini.h 348402 2012-08-01 19:55:32Z $
 *
 * wl driver tunables
 */

#define D11CONF		0x12200000      /* D11 Core Rev 21, 25, 28 */
#define D11CONF2	0		/* D11 Core Rev > 31 */
#define GCONF		0		/* No G-Phy */
#define ACONF		0		/* No A-Phy */
#define HTCONF		0		/* No HT-Phy */
#define ACCONF		0		/* No AC-Phy */
#define NCONF		0x20300		/* N-Phy Rev 8, 9, 17 */
#define LPCONF		0		/* No LP-Phy */
#define SSLPNCONF	0x8		/* SSLPN-Phy Rev 3 */
#define LCNCONF		0		/* No LCN-Phy */
#define LCN40CONF	0		/* No LCN40-Phy */

#define MAXSCB		64
#define NRXBUFPOST	56	/* # rx buffers posted */
#define RXBND		24	/* max # rx frames to process */

#define WME_PER_AC_TX_PARAMS 1
#define WME_PER_AC_TUNING 1
