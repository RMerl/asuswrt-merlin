/*
 * Linux device driver tunables for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver
 *
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
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
 * $Id: et_linux.h 320789 2012-03-13 04:01:27Z $
 */

#ifndef _et_linux_h_
#define _et_linux_h_

/* tunables */
#define	NTXD		512		/* # tx dma ring descriptors (must be ^2) */
#define	NRXD		512		/* # rx dma ring descriptors (must be ^2) */
#if defined(CONFIG_RAM_SIZE) && (CONFIG_RAM_SIZE <= 16)
#define NRXBUFPOST      256             /* try to keep this # rbufs posted to the chip */
#else
#define NRXBUFPOST      320             /* try to keep this # rbufs posted to the chip */
#endif
#define	BUFSZ		2048		/* packet data buffer size */
#define	RXBUFSZ		(BUFSZ - 256)	/* receive buffer size */

#ifndef RXBND
#define RXBND		32		/* max # rx frames to process in dpc */
#endif

#if defined(ILSIM) || defined(__arch_um__)
#undef	NTXD
#define	NTXD		16
#undef	NRXD
#define	NRXD		16
#undef	NRXBUFPOST
#define	NRXBUFPOST	2
#endif

#define	PKTCBND		32

#define CTFPOOLSZ	768

#define	PREFSZ			96
#ifndef PKTC
#define ETPREFHDRS(h, sz)	OSL_PREF_RANGE_ST((h), (sz))
#else
#define ETPREFHDRS(h, sz)
#endif

#endif	/* _et_linux_h_ */
