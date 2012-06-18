/*
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: nf_conntrack_autofw.h,v 1.1 2008/10/02 03:41:50 Exp $
 */

#ifndef _NF_CONNTRACK_AUTOFW_H
#define _NF_CONNTRACK_AUTOFW_H

#ifdef __KERNEL__

struct nf_ct_autofw_master {
	u_int16_t dport[2];     /* Related destination port range */
	u_int16_t to[2];        /* Port range to map related destination port range to */
};


#endif /* __KERNEL__ */

#endif /* _NF_CONNTRACK_AUTOFW_H */
