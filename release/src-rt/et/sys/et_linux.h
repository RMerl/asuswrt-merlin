/*
 * Linux device driver tunables for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: et_linux.h,v 1.16.12.3 2011-01-18 22:07:25 Exp $
 */

#ifndef _et_linux_h_
#define _et_linux_h_

/* tunables */
#define	NTXD		128		/* # tx dma ring descriptors (must be ^2) */
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

#define CTFPOOLSZ	512

#define	PREFSZ			96
#define ETPREFHDRS(h, sz)	OSL_PREF_RANGE_ST((h), (sz))

#endif	/* _et_linux_h_ */
