/*
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: ip_autofw.h,v 1.1 2008/10/02 03:42:40 Exp $
 */

#ifndef _IP_AUTOFW_H
#define _IP_AUTOFW_H

#define AUTOFW_MASTER_TIMEOUT 600	/* 600 secs */

struct ip_autofw_info {
	u_int16_t proto;	/* Related protocol */
	u_int16_t dport[2];	/* Related destination port range */
	u_int16_t to[2];	/* Port range to map related destination port range to */
};

struct ip_autofw_expect {
	u_int16_t dport[2];	/* Related destination port range */
	u_int16_t to[2];	/* Port range to map related destination port range to */
};

#endif /* _IP_AUTOFW_H */
