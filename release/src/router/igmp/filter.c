/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: filter.c 241182 2011-02-17 21:50:03Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bcmnvram.h>
#include <netconf.h>

#define IGMP_DBG(fmt, args...)

/* FILE-CSTYLED */

void
mcast_filter_init(netconf_filter_t * filter, uint8 * ifname,
		  struct in_addr mgrp)
{
    memset(filter, 0, sizeof(netconf_filter_t));
    //filter->match.state = NETCONF_NEW;
    strncpy(filter->match.in.name, (char *)ifname, IFNAMSIZ);
    filter->match.src.netmask.s_addr = htonl(INADDR_ANY);
    filter->match.src.ipaddr.s_addr = htonl(INADDR_ANY);
    filter->match.dst.netmask.s_addr = htonl(INADDR_BROADCAST);
    filter->match.dst.ipaddr.s_addr = mgrp.s_addr;
    filter->target = NETCONF_ACCEPT;
    filter->dir = NETCONF_FORWARD;

    return;
}

void
mcast_filter_add(uint8 * ifname, struct in_addr mgrp_addr)
{
    netconf_filter_t filter;
    int             ret;

    IGMP_DBG("Adding filter on %s for multicast group %s\n",
	     ifname, inet_ntoa(mgrp_addr));

    mcast_filter_init(&filter, ifname, mgrp_addr);

    ret = netconf_add_filter(&filter);
    if (ret) {
	IGMP_DBG("Netconf couldn't add filter %d\n", ret);
	return;
    }

    return;
}

void
mcast_filter_del(uint8 * ifname, struct in_addr mgrp_addr)
{
    netconf_filter_t filter;
    int             ret;

    IGMP_DBG("Deleting filter on %s for multicast group %s\n",
	     ifname, inet_ntoa(mgrp_addr));

    mcast_filter_init(&filter, ifname, mgrp_addr);

    ret = netconf_del_filter(&filter);
    if (ret) {
	IGMP_DBG("Netconf couldn't delete filter %d\n", ret);
	return;
    }

    return;
}
