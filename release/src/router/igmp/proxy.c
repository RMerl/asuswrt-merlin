/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: proxy.c 241182 2011-02-17 21:50:03Z $
 */

#include "igmprt.h"

vifi_t          numvifs;
char            buffer[500 * 16];
unsigned long   upstream;

extern int      inet_ntoa();
extern int      inet_addr();

/*
 * Open and init the multicast routing in the kernel.
 */

void
k_init_proxy(int socket)
{
    int             v = 1;

    if (setsockopt(socket, IPPROTO_IP, MRT_INIT, (char *) &v, sizeof(int))
	< 0)
	syslog(LOG_NOTICE, "setsockopt- MRT_INIT\n");
}



/*
 * Stops the multicast routing in the kernel.
 */

void
k_stop_proxy(int socket)
{
    if (setsockopt(socket, IPPROTO_IP, MRT_DONE, (char *) NULL, 0) < 0)
	syslog(LOG_NOTICE, "setsockopt- MRT_DONE\n");
}

int
k_proxy_del_vif(int socket, unsigned long vifaddr, vifi_t vifi)
{
    struct vifctl   vc;
    int             error;

    vc.vifc_vifi = vifi;
    vc.vifc_flags = 0;
    vc.vifc_threshold = 0;
    vc.vifc_rate_limit = 0;
    vc.vifc_lcl_addr.s_addr = vifaddr;
    vc.vifc_rmt_addr.s_addr = INADDR_ANY;
    if ((error =
	 setsockopt(socket, IPPROTO_IP, MRT_DEL_VIF, (char *) &vc,
		    sizeof(vc))) < 0) {
	syslog(LOG_NOTICE, "setsockopt- MRT_ADD_VIF\n");
	return FALSE;
    }
    return TRUE;
}



/*
 * Add a virtual interface to the kernel 
 * using the pimd API:MRT_ADD_VIF
 * 
 */

int
k_proxy_add_vif(int socket, unsigned long vifaddr, vifi_t vifi)
{
    struct vifctl   vc;
    int             error;

    vc.vifc_vifi = vifi;
    vc.vifc_flags = 0;
    vc.vifc_threshold = 0;
    vc.vifc_rate_limit = 0;
    vc.vifc_lcl_addr.s_addr = vifaddr;
    vc.vifc_rmt_addr.s_addr = INADDR_ANY;
    if ((error =
	 setsockopt(socket, IPPROTO_IP, MRT_ADD_VIF, (char *) &vc,
		    sizeof(vc))) < 0) {
	syslog(LOG_NOTICE, "setsockopt- MRT_ADD_VIF\n");
	return FALSE;
    }
    return TRUE;
}


/*
 * Del an MFC entry from the kernel
 * using pimd API:MRT_DEL_MFC
 */

int
k_proxy_del_mfc(int socket, u_long source, u_long group)
{
    struct mfcctl   mc;

    mc.mfcc_origin.s_addr = source;
    mc.mfcc_mcastgrp.s_addr = group;
    if (setsockopt
	(socket, IPPROTO_IP, MRT_DEL_MFC, (char *) &mc, sizeof(mc)) < 0) {
	syslog(LOG_NOTICE, "setsockopt- MRT_DEL_MFC\n");
	return FALSE;
    }
    IGMP_DBG("DEL_MFC: Deleting %lx %lx\n", group, source);
    return TRUE;
}



/*
 * Install and modify a MFC entry in the kernel (S,G,interface address)
 * using pimd API: MRT_AD_MFC
 */

extern igmp_router_t router;

int
k_proxy_chg_mfc(int socket, u_long source, u_long group, vifi_t outvif,
		int fstate)
{
    struct mfcctl   mc;
    igmp_interface_t *ifp;
    igmp_router_t  *igmprt = &router;


    /*
     ** mfcc_parent holds the inputs(Upstreams) interfaces
     ** in a multiple PVC case, the inputs can have up to 8
     ** interfaces. Sean Lee is going to provide info on which PVC's have IGMP enabled
     */

    memset(&mc, 0, sizeof(struct mfcctl));
    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next)
	if (ifp->igmpi_type == UPSTREAM /* && ifp->igmpi_multienabled */ ) {
	    mc.mfcc_origin.s_addr = source;
	    mc.mfcc_mcastgrp.s_addr = group;
	    mc.mfcc_parent = ifp->igmpi_index;
	    mc.mfcc_ttls[outvif] = fstate;
	    if (setsockopt
		(socket, IPPROTO_IP, MRT_ADD_MFC, (char *) &mc,
		 sizeof(mc)) < 0) {
		syslog(LOG_NOTICE, "setsockopt- MRT_ADD_MFC\n");
		return (FALSE);
	    }
	    IGMP_DBG("ADD_MFC: Add %lx %lx\n", group, source);
	}
    return (TRUE);

}

/*
 * create entry in the membership database
 */

membership_db  *
create_membership(struct in_addr group, int fmode, int numsources,
		  struct in_addr sources[MAX_ADDRS])
{
    membership_db  *member;
    int             i;
    if ((member = (membership_db *) malloc(sizeof(*member)))) {
	member->membership.group = group;
	member->membership.fmode = fmode;
	member->membership.numsources = numsources;
	for (i = 0; i < numsources; i++)
	    member->membership.sources[i].s_addr = sources[i].s_addr;
	member->next = NULL;
	return member;
    } else
	return NULL;
}


/*
 * lookup for a group entry in the membership database
 */

membership_db  *
find_membership(membership_db * membership, struct in_addr group)
{
    membership_db  *memb;

    for (memb = membership; memb; memb = memb->next)
	if (memb->membership.group.s_addr == group.s_addr)
	    return memb;
    return NULL;
}

/*
 * find a source in a in a source list
 */

int
find_source(struct in_addr sr, int nsources, struct in_addr *sources)
{
    int             i;

    for (i = 0; i < nsources; i++)
	if (sources[i].s_addr == sr.s_addr)
	    return TRUE;
    return FALSE;
}


/* FILE-CSTYLED */
