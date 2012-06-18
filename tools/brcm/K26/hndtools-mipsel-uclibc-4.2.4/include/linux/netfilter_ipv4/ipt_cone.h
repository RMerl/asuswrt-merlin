/*
 * Inlcude file for match cone target.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: ipt_cone.h,v 1.1 2010/02/07 01:14:25 Exp $
 */

#ifndef	IPT_CONE_H
#define IPT_CONE_H

void ipt_cone_place_in_hashes(struct nf_conn *ct);
void ipt_cone_cleanup_conntrack(struct nf_conn_nat *nat);

#endif	/* IPT_CONE_H */
