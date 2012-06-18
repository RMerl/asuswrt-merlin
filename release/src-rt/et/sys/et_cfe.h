/*
 * CFE device driver tunables for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver

 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: et_cfe.h,v 1.5 2006-03-16 19:28:57 Exp $
 */

#ifndef _et_cfe_h_
#define _et_cfe_h_

#define NTXD	        16
#define NRXD	        16
#define NRXBUFPOST	8   

#define NBUFS		(NRXBUFPOST + 8)

/* common tunables */
#define	RXBUFSZ		LBDATASZ	/* rx packet buffer size */
#define	MAXMULTILIST	32		/* max # multicast addresses */

#endif /* _et_cfe_h_ */
