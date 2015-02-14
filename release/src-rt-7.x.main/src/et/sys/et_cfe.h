/*
 * CFE device driver tunables for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver

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
 *
 * $Id: et_cfe.h 267700 2011-06-19 15:41:07Z $
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
