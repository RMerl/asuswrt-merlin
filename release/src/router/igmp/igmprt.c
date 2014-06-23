/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igmprt.c 241182 2011-02-17 21:50:03Z $
 */

#include "igmprt.h"
#include <sys/time.h>

#define CLI_MAX_BUF_SZ 128
#define MIN_DEV_LEN 4

igmp_router_t   router;
igmp_interface_t *downstream;
char            upstream_interface[10][IFNAMSIZ];
// Keven Kuo
int             interface_lo = FALSE;
extern int      inet_ntoa();
extern int      inet_addr();
extern int      bcmSystemEx(char *cmd, int printflag);
extern void     mcast_filter_add(unsigned char *, struct in_addr);
extern void     mcast_filter_del(unsigned char *, struct in_addr);
extern int      k_proxy_del_vif(int, unsigned long, vifi_t);

extern void     wait_for_interfaces();

// extern int igmp_timeout();
char            grpinfo[64] = "";

/*
 * Structure of an internet header, naked of options.
 */
struct ip {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int    ip_hl:4;	/* header length */
    unsigned int    ip_v:4;	/* version */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned int    ip_v:4;	/* version */
    unsigned int    ip_hl:4;	/* header length */
#endif
    u_int8_t        ip_tos;	/* type of service */
    u_short         ip_len;	/* total length */
    u_short         ip_id;	/* identification */
    u_short         ip_off;	/* fragment offset field */
#define   IP_RF 0x8000		/* reserved fragment flag */
#define   IP_DF 0x4000		/* dont fragment flag */
#define   IP_MF 0x2000		/* more fragments flag */
#define   IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
    u_int8_t        ip_ttl;	/* time to live */
    u_int8_t        ip_p;	/* protocol */
    u_short         ip_sum;	/* checksum */
    struct in_addr  ip_src,
                    ip_dst;	/* source and dest address */
};
/*
 * version and isquerier variable from the config file
 */

int             version,
                querier;

#ifdef HND_FIX
int
bcmSystemEx(char *cmd, int printflag)
{
    printf("Filters TBD\n");
    return (0);
}
#endif /* HND_FIX */


void
igmp_info_print(igmp_router_t * router)
{
#ifdef DEBUG_PROXY
    igmp_interface_t *ifp;
    igmp_group_t   *gp;
    igmp_src_t     *src;
    igmp_rep_t     *rep;

    printf("\nIGMP Table\n");
    printf("-----------------\n");
    printf("\n%-14s %-9s %-14s %-5s %-14s %-14s\n", "interface", "version",
	   "groups", "mode", "source", "Membres");
    for (ifp = router->igmprt_interfaces; ifp;
	 ifp = (igmp_interface_t *) ifp->igmpi_next) {
	printf("%-14s 0x%x\n", (char *) inet_ntoa(ifp->igmpi_addr),
	       ifp->igmpi_version);
	if (ifp->igmpi_groups != NULL) {
	    for (gp = ifp->igmpi_groups; gp;
		 gp = (igmp_group_t *) gp->igmpg_next) {
		printf("%32s %11s\n", (char *) inet_ntoa(gp->igmpg_addr),
		       (gp->igmpg_fmode ==
			IGMP_FMODE_INCLUDE) ? "INCLUDE" : "EXCLUDE");
		if (gp->igmpg_sources != NULL)
		    for (src = gp->igmpg_sources; src;
			 src = (igmp_src_t *) src->igmps_next)
			printf("%50s\n",
			       (char *) inet_ntoa(src->igmps_source));
		if (gp->igmpg_members != NULL)
		    for (rep = gp->igmpg_members; rep;
			 rep = (igmp_rep_t *) rep->igmpr_next)
			/*
			 * if (gp->igmpg_sources != NULL)
			 * printf("%17s\n",inet_ntoa(rep->igmpr_addr));
			 * else
			 */
			printf("%70s\n",
			       (char *) inet_ntoa(rep->igmpr_addr));
		else
		    printf("\n");
	    }
	} else
	    printf("\n");

    }
#endif
}

/****************************************************************************
 *
 * Routines pour les enregistrements des membres: Ceci n'a pas ete specifie
 * dans le draft, mais pour se rendre compte vite q'un membre viend de quitter 
 * un groupe, ou pour garder trace du membres on peut utiliser ces routines
 *
 ***************************************************************************/

/*
 * igmp_rep_t* igmp_rep_create()
 *
 * Cree un enregistrement d'un membre d'un groupe
 */
igmp_rep_t     *
igmp_rep_create(struct in_addr srcaddr)
{
    igmp_rep_t     *rep;

    if ((rep = (igmp_rep_t *) malloc(sizeof(*rep)))) {
	rep->igmpr_addr.s_addr = srcaddr.s_addr;
	rep->igmpr_next = NULL;
    }
    return rep;
}

int
igmp_rep_count(igmp_group_t * gp)
{
    igmp_rep_t     *re;
    int             count = 0;

    assert(gp != NULL);
    for (re = gp->igmpg_members; re != NULL;
	 re = (igmp_rep_t *) re->igmpr_next)
	count++;

    return count;

}
/*
 * void igmp_rep_cleanup()
 *
 */
void
igmp_rep_cleanup(igmp_group_t * gp, igmp_rep_t * rep)
{
    igmp_rep_t     *re;

    assert(rep != NULL);
    assert(gp != NULL);
    if (rep != gp->igmpg_members) {
	for (re = gp->igmpg_members; (igmp_rep_t *) re->igmpr_next != rep;
	     re = (igmp_rep_t *) re->igmpr_next);
	re->igmpr_next = rep->igmpr_next;
	// printf("*** free rep (0x%x)\n",rep->igmpr_addr.s_addr);
	free(rep);
    } else {
	/*
	 * delete the head
	 */
	re = gp->igmpg_members;
	gp->igmpg_members = (igmp_rep_t *) re->igmpr_next;
	// printf("*** free rep head (0x%x)\n",re->igmpr_addr.s_addr);
	free(re);
	// printf("*** after free rep head \n");
    }
}

/*
 * void igmp_rep_print()
 *
 */
void
igmp_rep_print(igmp_rep_t * rep)
{
    printf("\t\tmembre:%-16s\n", (char *) inet_ntoa(rep->igmpr_addr));
}
/*
 * igmp_rep_t* igmp_group_rep_lookup()
 *
 * Lookup a membre in the sourcetable of a group
 */
igmp_rep_t     *
igmp_group_rep_lookup(igmp_group_t * gp, struct in_addr srcaddr)
{
    igmp_rep_t     *rep;

    assert(gp != NULL);
    for (rep = (igmp_rep_t *) gp->igmpg_members; rep;
	 rep = (igmp_rep_t *) rep->igmpr_next)
	if (rep->igmpr_addr.s_addr == srcaddr.s_addr)
	    return rep;
    return NULL;
}

/*
 * igmp_rep_t* igmp_group_rep_add()
 *
 * Add a member to the set of sources of a group
 */
igmp_rep_t     *
igmp_group_rep_add(igmp_group_t * gp, struct in_addr srcaddr)
{
    igmp_rep_t     *rep;

    assert(gp != NULL);
    /*
     * Return the source if it's already present 
     */
    if ((rep = igmp_group_rep_lookup(gp, srcaddr))) {
	// printf("igmp_group_rep_add: src already present \n");
	return rep;
    }
    /*
     * Create the source and add to the set 
     */
    if ((rep = igmp_rep_create(srcaddr))) {
	rep->igmpr_next = (struct igmp_rep_t *)gp->igmpg_members;
	gp->igmpg_members = rep;
	// printf("igmp_group_rep_add: create the src and add to the set
	// \n");
    }
    return rep;
}
/******************************************************************************
 *
 * igmp source routines 
 * 
 *****************************************************************************/
/*
 * igm_src_t* igmp_src_create()
 *
 */
igmp_src_t     *
igmp_src_create(struct in_addr srcaddr)
{
    igmp_src_t     *src;

    if ((src = (igmp_src_t *) malloc(sizeof(*src)))) {
	src->igmps_source.s_addr = srcaddr.s_addr;
	src->igmps_timer = 0;
	src->igmps_fstate = TRUE;
	src->igmps_next = NULL;
    }
    return src;
}

/*
 * void igmp_src_cleanup()
 *
 */
void
igmp_src_cleanup(igmp_group_t * gp, igmp_src_t * src)
{
    igmp_src_t     *sr;

    assert(src != NULL);
    assert(gp != NULL);
    if (src != gp->igmpg_sources) {
	for (sr = gp->igmpg_sources; (igmp_src_t *) sr->igmps_next != src;
	     sr = (igmp_src_t *) sr->igmps_next);
	sr->igmps_next = src->igmps_next;
	free(src);
    } else {
	/*
	 * delete the head
	 */
	sr = gp->igmpg_sources;
	gp->igmpg_sources = (igmp_src_t *) sr->igmps_next;
	free(sr);
    }
}

/*
 * void igmp_src_print()
 *
 * Imprimer un enregistrement d'une source
 */
void
igmp_src_print(igmp_src_t * src)
{
    printf("\t\tsource:%-16s  %d\n",
	   (char *) inet_ntoa(src->igmps_source), src->igmps_timer);
}
/*
 * igmp_src_t* igmp_group_src_lookup()
 *
 * Lookup a source in the sourcetable of a group
 */
igmp_src_t     *
igmp_group_src_lookup(igmp_group_t * gp, struct in_addr srcaddr)
{
    igmp_src_t     *src;

    if (gp != NULL) {
	for (src = gp->igmpg_sources; src != NULL;
	     src = (igmp_src_t *) src->igmps_next) {
	    if (src->igmps_source.s_addr == srcaddr.s_addr)
		return src;
	}
    }
    return NULL;
}

/*
 * igmp_src_t* igmp_group_src_add()
 *
 * Add a source to the set of sources of a group
 */
igmp_src_t     *
igmp_group_src_add(igmp_group_t * gp, struct in_addr srcaddr)
{
    igmp_src_t     *src;

    assert(gp != NULL);
    /*
     * Return the source if it's already present 
     */
    if ((src = igmp_group_src_lookup(gp, srcaddr)))
	return src;
    /*
     * Create the source and add to the set 
     */
    if ((src = igmp_src_create(srcaddr))) {
	src->igmps_next = (struct igmp_src_t *)gp->igmpg_sources;
	gp->igmpg_sources = src;
	// printf ( "DEBUG: igmp_group_src_add first member %s\n", (char
	// *)inet_ntoa(srcaddr) );
    }
    return src;
}



/******************************************************************************
 *
 * igmp group routines 
 *
 *****************************************************************************/

/*
 * igmp_group_t* igmp_group_create()
 *
 * Create a group record
 */
igmp_group_t   *
igmp_group_create(struct in_addr groupaddr)
{
    igmp_group_t   *gp;

    if ((gp = (igmp_group_t *) malloc(sizeof(*gp)))) {
	gp->igmpg_addr.s_addr = groupaddr.s_addr;
	// eddie
	gp->igmpg_fmode = IGMP_FMODE_EXCLUDE;
	gp->igmpg_version = IGMP_VERSION_2;	/* default version is V2 */
	// eddie
	gp->igmpg_timer = 0;
	gp->igmpg_sources = NULL;
	gp->igmpg_members = NULL;
	gp->igmpg_next = NULL;
	return gp;
    } else {
	return NULL;
    }
}

/*
 * void igmp_group_cleanup()
 *
 * Cleanup a group record
 */
void
igmp_group_cleanup(igmp_interface_t * ifp,
		   igmp_group_t * gp, igmp_router_t * router)
{
#ifndef HND_FIX
    char cmd[CLI_MAX_BUF_SZ];
#endif /* HND_FIX */
    igmp_group_t   *g;
    assert(gp != NULL);
    assert(ifp != NULL);

    if (!interface_lo) {
#ifndef HND_FIX
	sprintf(cmd,
		"iptables -t filter -I FORWARD 1 -i %s -d %s -j DROP 2>/dev/null",
		router->igmprt_interfaces->igmpi_name,
		(char *) inet_ntoa(gp->igmpg_addr));
	bcmSystemEx(cmd, 1);
	sprintf(cmd,
		"iptables -t filter -D FORWARD -i %s -d %s -j ACCEPT 2>/dev/null",
		router->igmprt_interfaces->igmpi_name,
		(char *) inet_ntoa(gp->igmpg_addr));
	bcmSystemEx(cmd, 1);
#endif /* HND_FIX */
    }
    igmp_info_print((igmp_router_t *)router);
    if (ifp->igmpi_groups != gp) {
	g = ifp->igmpi_groups;
	while ((igmp_group_t *) g->igmpg_next != gp) {
	    g = (igmp_group_t *) g->igmpg_next;
	    if (g == NULL) {
		// printf("BUG in igmp_group_cleanup sleep 100000\n"); 
		getchar();
	    }
	}
	g->igmpg_next = gp->igmpg_next;
	free(gp);
    } else {			/* delete the head */
	g = ifp->igmpi_groups;
	ifp->igmpi_groups = (igmp_group_t *) g->igmpg_next;
	free(g);
    }
}

/*
 * void igmp_group_print()
 *
 * Print a group record
 */
void
igmp_group_print(igmp_group_t * gp)
{
    igmp_src_t     *src;
    igmp_rep_t     *rep;
    printf("  %-16s %s %d\n",
	   (char *) inet_ntoa(gp->igmpg_addr),
	   (gp->igmpg_fmode == IGMP_FMODE_EXCLUDE) ? "exclude" : "include",
	   gp->igmpg_timer);
    if (gp->igmpg_sources != NULL)
	for (src = gp->igmpg_sources; src;
	     src = (igmp_src_t *) src->igmps_next)
	    printf("source : %s timer : %d\n",
		   (char *) inet_ntoa(src->igmps_source.s_addr),
		   src->igmps_timer);
    if (gp->igmpg_members != NULL)
	for (rep = gp->igmpg_members; rep;
	     rep = (igmp_rep_t *) rep->igmpr_next)
	    printf("member : %s \n",
		   (char *) inet_ntoa(rep->igmpr_addr.s_addr));
}

/******************************************************************************
 *
 * igmp interface routines 
 *
 *****************************************************************************/

/*
 * igmp_interface_t* igmp_interface_create()
 *
 * Create and initialize interface record
 */
igmp_interface_t *
igmp_interface_create(igmp_router_t *igmprt, struct in_addr ifaddr, char *ifname, vifi_t index)
{
    igmp_interface_t *ifp;
    struct ip_mreq  mreq;
    u_char i;
    short flags;
#ifdef HND_FIX
    int igmpi_type;

    if (strncmp(ifname, "br", 2) == 0) {
    	if (strncmp(ifname, "br0", 3) != 0) {
		return NULL;
	}
	igmpi_type = DOWNSTREAM;
    }
    else
	igmpi_type = UPSTREAM;
#endif

    if (((ifp = (igmp_interface_t *) malloc(sizeof(*ifp)))) == NULL)
	return NULL;

    /*
     * Allocate a buffer to receive igmp messages 
     */
    ifp->igmpi_bufsize = get_interface_mtu(ifname);
    // printf("MTU=%d \n",ifp->igmpi_bufsize);
    if (ifp->igmpi_bufsize == -1)
	ifp->igmpi_bufsize = MAX_MSGBUFSIZE;
    if ((ifp->igmpi_buf = (char *) malloc(ifp->igmpi_bufsize)) == NULL) {
	free(ifp);
	return NULL;
    }

    /*
     * Initialize all fields 
     */
    ifp->igmpi_addr.s_addr = ifaddr.s_addr;
    strncpy(ifp->igmpi_name, ifname, IFNAMSIZ);

#ifdef HND_FIX
    ifp->igmpi_type = igmpi_type;
#else
    if (strncmp(ifname, "br", 2) == 0) 
	ifp->igmpi_type = DOWNSTREAM;
    else
	ifp->igmpi_type = UPSTREAM;
#endif

    /*
     * Create a raw igmp socket 
     */
    if (ifp->igmpi_type == DOWNSTREAM)
	ifp->igmpi_socket = igmprt->igmprt_down_socket;
    else
	ifp->igmpi_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (ifp->igmpi_socket == -1) {
	printf("can't create socket \n");
	free(ifp->igmpi_buf);
	free(ifp);
	return NULL;
    }
    ifp->igmpi_groups = NULL;
    ifp->sch_group_query = NULL;
    ifp->igmpi_isquerier = TRUE;
    ifp->igmpi_version = version;	/* IGMP_VERSION_3; */
    ifp->igmpi_qi = IGMP_DEF_QI;
    ifp->igmpi_qri = IGMP_DEF_QRI;
    ifp->igmpi_rv = IGMP_DEF_RV;
    ifp->igmpi_gmi = ifp->igmpi_rv * ifp->igmpi_qi + ifp->igmpi_qri;
    ifp->igmpi_ti_qi = 0;
    ifp->igmpi_next = NULL;



    /*
     * Set reuseaddr, ttl, loopback and set outgoing interface 
     */
    i = 1;
    setsockopt(ifp->igmpi_socket, SOL_SOCKET, SO_REUSEADDR, (void *) &i,
	       sizeof(i));
    i = 1;
    setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_MULTICAST_TTL,
	       (void *) &i, sizeof(i));
    // eddie disable LOOP
    i = 0;
    setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
	       (void *) &i, sizeof(i));
    setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_MULTICAST_IF,
	       (void *) &ifaddr, sizeof(ifaddr));

    // eddie In linux use IP_PKTINFO
    // IP_RECVIF returns the interface of received datagram
    // setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_RECVIF, &i,
    // sizeof(i));
    i = 1;
    setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_PKTINFO, &i, sizeof(i));

    /*
     * Add membership to ALL_ROUTERS and ALL_ROUTERS_V3 on this interface 
     */
    if (!interface_lo) {
	mreq.imr_multiaddr.s_addr = inet_addr(IGMP_ALL_ROUTERS);
	mreq.imr_interface.s_addr = ifaddr.s_addr;	/* htonl(0); */
	if (ifp->igmpi_type == DOWNSTREAM) {
	    setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		       (void *) &mreq, sizeof(mreq));
	}
    }
    // Keven
    mreq.imr_multiaddr.s_addr = inet_addr(IGMP_ALL_ROUTERS_V3);
    mreq.imr_interface.s_addr = ifaddr.s_addr;	/* htonl(0); */
    if (ifp->igmpi_type == DOWNSTREAM) {
	setsockopt(ifp->igmpi_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   (void *) &mreq, sizeof(mreq));
    }

    igmp_info_print(&router);

    /*
     * Tell the kernel this interface belongs to a multicast router 
     */
    if (ifp->igmpi_type == DOWNSTREAM)
	mrouter_onoff(ifp->igmpi_socket, 1);
    ifp->igmpi_index = index;

    /*
     * Set the interface flags to receive all multicast packets 
     */
    ifp->igmpi_save_flags = get_interface_flags(ifname);
    if (ifp->igmpi_save_flags != -1) {
	set_interface_flags(ifname, ifp->igmpi_save_flags | IFF_ALLMULTI);
	/*
	 * If IFF_ALLMULTI didn't work, try IFF_PROMISC 
	 */
	flags = get_interface_flags(ifname);
	if ((flags & IFF_ALLMULTI) != IFF_ALLMULTI)
	    set_interface_flags(ifname,
				ifp->igmpi_save_flags | IFF_PROMISC);
    }

    return ifp;
}

/*
 * void igmp_interface_cleanup()
 *
 * Cleanup an interface 
 */
void
igmp_interface_cleanup(igmp_interface_t * ifp)
{
    igmp_group_t   *gp,
                   *gp2;

    assert(ifp != NULL);

    /*
     * Cleanup all groups 
     */
    gp = ifp->igmpi_groups;
    ifp->igmpi_groups = NULL;
    while (gp != NULL) {
	gp2 = gp;
	gp = gp->igmpg_next;
	free(gp2);
    }
    /*
     * Tell the kernel the multicast router is no more 
     */
    if (ifp->igmpi_type == DOWNSTREAM)
	k_stop_proxy(ifp->igmpi_socket);
    close(ifp->igmpi_socket);

    free(ifp->igmpi_buf);

    /*
     * If we managed to get/set the interface flags, revert 
     */
    if (ifp->igmpi_save_flags != -1)
	set_interface_flags(ifp->igmpi_name, ifp->igmpi_save_flags);
    free(ifp);
}

/*
 * igmp_group_t* igmp_interface_group_add()
 *
 * Add a group to the set of groups of an interface
 */
igmp_group_t   *
igmp_interface_group_add(igmp_router_t * router,
			 igmp_interface_t * ifp, struct in_addr groupaddr)
{
#ifndef HND_FIX
    char            cmd[CLI_MAX_BUF_SZ];
#endif /* HND_FIX */
    igmp_group_t   *gp;
    struct ip_mreq  mreq;
    igmp_interface_t *upstream_interface;
    struct in_addr  up;

    // printf("Add a group to the set of groups of an interface \n");
    assert(ifp != NULL);
    /*
     * Return the group if it's already present 
     */
    if ((gp = igmp_interface_group_lookup(ifp, groupaddr)))
	return gp;
    /*
     * Create the group and add to the set 
     */
    if ((gp = igmp_group_create(groupaddr))) {
	// printf("created a group \n");
	gp->igmpg_next = ifp->igmpi_groups;
	ifp->igmpi_groups = gp;

	// printf("adding to all upstream interfaces \n");

	for (ifp = router->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	    if (ifp->igmpi_type == UPSTREAM) {
		IGMP_DBG("Adding membership on interface %x for grp %x\n",
		         ifp->igmpi_addr.s_addr, groupaddr.s_addr);
		mreq.imr_multiaddr.s_addr = groupaddr.s_addr;
		mreq.imr_interface.s_addr = ifp->igmpi_addr.s_addr;
		if (VALID_ADDR(mreq.imr_multiaddr)) {	/* only if the
							 * join is from
							 * downstream */
		    up.s_addr = ifp->igmpi_addr.s_addr;
		    upstream_interface =
			igmprt_interface_lookup(router, up);
		    if (igmp_interface_group_lookup
			(upstream_interface, mreq.imr_multiaddr)
			== NULL) {
			if (setsockopt
			    (router->igmprt_up_socket, IPPROTO_IP,
			     IP_ADD_MEMBERSHIP, (void *) &mreq,
			     sizeof(mreq)) < 0) {
			    syslog(LOG_NOTICE,
				   "igmp:IP_ADD_MEMBERSHIP Failed\n");
			    continue;
			}
			IGMP_DBG("IP_ADD_MEMBERSHIP done for %x\n",
			         mreq.imr_multiaddr.s_addr);
			// printf("* DONE IP_ADD_MEMBERSHIP gaddr %s iname 
			// %s upstiname %s rtiname %s rtiaddr %s* \n",
			// (char *)inet_ntoa(groupaddr), ifp->igmpi_name,
			// upstream_interface->igmpi_name,
			// router->igmprt_interfaces->igmpi_name,
			// (char
			// *)inet_ntoa(router->igmprt_interfaces->igmpi_addr));
			if (!interface_lo) {
#ifdef HND_FIX
			    mcast_filter_add((unsigned char *)ifp->igmpi_name, groupaddr);
#else /* HND_FIX */
			    sprintf(cmd,
				    "iptables -D FORWARD -i %s -d %s -j DROP 2>/dev/null",
				    ifp->igmpi_name,
				    (char *) inet_ntoa(groupaddr));
			    bcmSystemEx(cmd, 1);
			    sprintf(cmd,
				    "iptables -t filter -I FORWARD 1 -i %s -d %s -j ACCEPT 2>/dev/null",
				    ifp->igmpi_name,
				    (char *) inet_ntoa(groupaddr));
			    bcmSystemEx(cmd, 1);
#endif /* HND_FIX */
			}
		    }
		}
	    }
	}			/* for */
    }
    return gp;
}

/*
 * igmp_group_t* igmp_interface_group_lookup()
 *
 * Lookup a group in the grouptable of an interface
 */
igmp_group_t   *
igmp_interface_group_lookup(igmp_interface_t * ifp,
			    struct in_addr groupaddr)
{
    igmp_group_t   *gp;

    assert(ifp != NULL);
    for (gp = ifp->igmpi_groups; gp; gp = gp->igmpg_next)
	if (gp->igmpg_addr.s_addr == groupaddr.s_addr)
	    return gp;
    // printf("igmp_interface_group_lookup return NULL \n");
    return NULL;
}
void
igmp_interface_membership_leave_v2(igmp_router_t * router,
				   igmp_interface_t * ifp,
				   struct in_addr src,
				   igmpr_t * report, int len)
{
    igmp_group_t   *gp;
    igmp_rep_t     *rep;
    igmp_interface_t *ifi;
    struct ip_mreq  mreq;
    int             count = 0;
    int             needdrop = 0;
    char            cmd[128];
    char            br[32];
    char            dev[32];
    char            dest[32];
    char            host[32];

    if (!IN_MULTICAST(ntohl(report->igmpr_group.s_addr))) {
	printf("Ignore non-multicast ...\n");
	return;
    }

    for (ifi = router->igmprt_interfaces; ifi; ifi = ifi->igmpi_next) {
	if (ifi->igmpi_type == DOWNSTREAM) {
	    for (gp = ifi->igmpi_groups; gp; gp = gp->igmpg_next) {
		if (gp->igmpg_addr.s_addr ==
		    ntohl(report->igmpr_group.s_addr)) {
		    rep = igmp_group_rep_lookup(gp, src);
		    if (rep)
			igmp_rep_cleanup(gp, rep);
		    count = igmp_rep_count(gp);
		    if (count == 0) {
			igmp_group_cleanup(ifi, gp, router);
			// printf("DEBUG: LEAVE GROUP member rep count=%d
			// rtiname %s \n",count,
			// router->igmprt_interfaces->igmpi_name);
			needdrop = 1;
		    }
		}
	    }			/* for groups */
	}			/* DOWNSTREAM */
    }				/* for interfaces */

    IGMP_DBG("Deleting membership for %x\n", report->igmpr_group.s_addr);

    if (needdrop) {
	for (ifi = router->igmprt_interfaces; ifi; ifi = ifi->igmpi_next) {
	    if (ifi->igmpi_type == UPSTREAM) {
		mreq.imr_multiaddr.s_addr =
		    ntohl(report->igmpr_group.s_addr);
		mreq.imr_interface.s_addr = ifi->igmpi_addr.s_addr;
		if (VALID_ADDR(mreq.imr_multiaddr)) {
		    if (setsockopt
			(router->igmprt_up_socket, IPPROTO_IP,
			 IP_DROP_MEMBERSHIP, (void *) &mreq,
			 sizeof(mreq)) < 0) {
			syslog(LOG_NOTICE,
			       "igmp:IP_DROP_MEMBERSHIP Failed\n");
		    }
		    // printf("IP_DROP_MERSHIP
		    // for(0x%x)\n",ifi->igmpi_addr.s_addr);
		    IGMP_DBG("Deleting membership done for %x on if %x\n",
			     report->igmpr_group.s_addr, ifi->igmpi_addr.s_addr);
		}
	    }
	}
	if (!interface_lo) {
	    if (strlen(grpinfo) > MIN_DEV_LEN) {
		sscanf(grpinfo, "%s %s %s/%s", br, dev, dest, host);
		sprintf(cmd,
			"brctl clearportsnooping %s %s %s/ffffffffffff 1> /dev/null",
			br, dev, dest);
#ifndef HND_FIX
		system(cmd);
#endif /* HND_FIX */
	    }
	}
    }				/* needdrop */
}

/*
 * void igmp_interface_membership_report_v12()
 *
 * Process a v1/v2 membership report
 */
void
igmp_interface_membership_report_v12(igmp_router_t * router,
				     igmp_interface_t * ifp,
				     struct in_addr src,
				     igmpr_t * report, int len)
{
    igmp_group_t   *gp;
    igmp_rep_t     *rep;

    /*
     * Ignore a report for a non-multicast address 
     */
    if (!IN_MULTICAST(ntohl(report->igmpr_group.s_addr))) {
	printf("Ignore non-multicast ...\n");
	return;
    }
    /*
     * Find the group, and if not present, add it 
     */
    // printf("*** igmp_interface_group_add 0x%x\n",report->igmpr_group);
    if ((gp =
	 igmp_interface_group_add(router, ifp,
				  report->igmpr_group)) == NULL)
	return;
    /*
     * find the member and add it if not present
     */
    rep = igmp_group_rep_add(gp, src);
    // printf("*** igmp_group_rep_add call igmp_group_handle_isex\n");

    /*
     * Consider this to be a v3 is_ex{} report 
     */
    igmp_group_handle_isex(router, ifp, gp, 0, NULL);
}

/*
 * void igmp_interface_print()
 *
 * Print status of an interface 
 */
void
igmp_interface_print(igmp_interface_t * ifp)
{
    igmp_group_t   *gp;

    syslog(LOG_NOTICE, " interface %s, %s ver=0x%x name=%s index=%d\n",
	   (char *) inet_ntoa(ifp->igmpi_addr),
	   (ifp->igmpi_type == UPSTREAM) ? "UPSTREAM" : "DOWNSTREAM",
	   ifp->igmpi_version, ifp->igmpi_name, ifp->igmpi_index);
    for (gp = ifp->igmpi_groups; gp; gp = gp->igmpg_next)
	igmp_group_print(gp);
}

/******************************************************************************
 *
 * igmp router routines
 *
 *****************************************************************************/

/*
 * int igmprt_init()
 *
 * Initialize igmp router
 */
int
igmprt_init(igmp_router_t * igmprt)
{
    igmprt->igmprt_interfaces = NULL;
    // igmprt->igmprt_thr_timer = igmprt->igmprt_thr_input = NULL;
    igmprt->igmprt_flag_timer = 0;
    igmprt->igmprt_flag_input = 0;
    /*
     * create socket to tell BB-RAS about member reports from downstream 
     */
    igmprt->igmprt_up_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (igmprt->igmprt_up_socket < 0) {
	perror("can't open upstream socket");
	exit(1);
    }
    /*
     * create raw socket to update mfc and vif table
     */
    // igmprt->igmprt_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
    igmprt->igmprt_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (igmprt->igmprt_socket < 0) {
	perror("can't open igmp socket");
	exit(1);
    }

    /* Create the raw socket for downstream interfaces to receive IGMP packets on*/
    igmprt->igmprt_down_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
    if (igmprt->igmprt_down_socket < 0) {
	perror("can't open igmp socket");
	exit(1);
    }
    forward_upstream = 0;

    // signal(SIGALRM, igmp_timeout);
    // alarm(1);
    if (!interface_lo) {
#ifndef HND_FIX
	system
	    ("brctl clearportsnooping br0 eth0 000000000000/ffffffffffff 1> /dev/null");
#endif /* HND_FIX */
    }
    return 1;
}

/*
 * void igmprt_destroy()
 *
 * Cleanup the router 
 */
void
igmprt_destroy(igmp_router_t * igmprt)
{
    igmp_interface_t *ifp,
                   *ifp2;
    igmp_group_t   *gp;

    for (ifp = igmprt->igmprt_interfaces; ifp;) {
#ifdef HND_FIX
	for (gp = ifp->igmpi_groups; gp; gp = gp->igmpg_next)
	    k_proxy_del_mfc(igmprt->igmprt_socket, gp->igmpg_source.s_addr,
			    gp->igmpg_addr.s_addr);
#endif /* HND_FIX */
	k_proxy_del_vif(igmprt->igmprt_socket, ifp->igmpi_addr.s_addr,
			ifp->igmpi_index);
	ifp2 = ifp;
	ifp = ifp->igmpi_next;
	igmp_interface_cleanup(ifp2);
    }
    igmprt_stop(igmprt);

}

/*
 * interface_t* igmprt_interface_lookup()
 *
 * Lookup a group, identified by the interface address
 */
igmp_interface_t *
igmprt_interface_lookup(igmp_router_t * igmprt, struct in_addr ifaddr)
{
    igmp_interface_t *ifp;

    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	if (ifp->igmpi_addr.s_addr == ifaddr.s_addr)
	    return ifp;
    }
    return NULL;
}

/*
 * interface_t* igmprt_interface_lookup_index()
 *
 * Lookup a group, identified by the interface index
 */
igmp_interface_t *
igmprt_interface_lookup_index(igmp_router_t * igmprt, int ifp_index)
{
    igmp_interface_t *ifp;

    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	if (ifp->igmpi_index == ifp_index)
	    return ifp;
    }
    return NULL;
}

/*
 * igmp_entry_t* igmprt_group_lookup() 
 *
 * Lookup a group, identified by the interface and group address
 */
igmp_group_t   *
igmprt_group_lookup(igmp_router_t * igmprt,
		    struct in_addr ifaddr, struct in_addr groupaddr)
{
    igmp_interface_t *ifp;

    if ((ifp = igmprt_interface_lookup(igmprt, ifaddr)))
	return igmp_interface_group_lookup(ifp, groupaddr);
    return NULL;
}

/*
 * struct igmp_interface_t* igmprt_interface_add()
 *
 * Add an interface to the interfacetable
 */
igmp_interface_t *
igmprt_interface_add(igmp_router_t * igmprt,
		     struct in_addr ifaddr, char *ifname, vifi_t index)
{
    igmp_interface_t *ifp;

    /*
     * Return the interface if it's already in the set 
     */
    if ((ifp = igmprt_interface_lookup(igmprt, ifaddr)))
	return ifp;
    /*
     * Create the interface and add to the set 
     */
    if ((ifp = igmp_interface_create(igmprt, ifaddr, ifname, index))) {
	ifp->igmpi_next = igmprt->igmprt_interfaces;
	igmprt->igmprt_interfaces = ifp;
    }
    return ifp;
}

/*
 * igmp_group_t* igmprt_group_add()
 *
 * Add a group to the grouptable of some interface
 */
igmp_group_t   *
igmprt_group_add(igmp_router_t * igmprt,
		 struct in_addr ifaddr, struct in_addr groupaddr)
{
    igmp_interface_t *ifp;

    if ((ifp = igmprt_interface_lookup(igmprt, ifaddr)))
	return NULL;
    return igmp_interface_group_add(igmprt, ifp, groupaddr);
}

/*
 * void igmprt_timergroup(igmp_router_t* igmprt)
 *
 * Decrement timers and handle whatever has to be done when one expires
 */
void
igmprt_timer(igmp_router_t * igmprt)
{
    igmp_interface_t *ifp;
    struct in_addr  zero;

    zero.s_addr = 0;

    /*
     * Handle every interface 
     */
    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	/*
	 * If we're the querier for this network, handle all querier
	 * duties 
	 */
	if (ifp->igmpi_type == DOWNSTREAM) {
	    if (ifp->igmpi_isquerier == TRUE) {
		/*
		 * Deal with the general query 
		 */
		if (--ifp->igmpi_ti_qi <= 0) {
		    ifp->igmpi_ti_qi = ifp->igmpi_qi;
		    igmprt_membership_query(igmprt, ifp, &zero, NULL, 0,
					    0);
		}
	    } else {
		/*
		 * If not the querier, deal with other-querier-present
		 * timer
		 */
		igmprt_timer_querier(ifp);
	    }
	    /*
	     * handle group timer
	     */
	    igmprt_timer_group(igmprt, ifp);
	}			/* DOWNSTREAM */
    }				/* for */
}

void           *
igmprt_timer_thread(void *arg)
{
    igmp_router_t  *igmprt = (igmp_router_t *) arg;

    if (igmprt->igmprt_flag_timer) {
	igmprt_timer(igmprt);
    }
    /*
     * Should be changed to take care of drifting 
     */
    usleep(1000000);
    return NULL;
}

#define SIOCGEXTIF   0x8908
#define QUERY_TIMEOUT   130

typedef struct igmp_member_item igmp_member_item;

struct igmp_member_item {
    char            br[8];
    char            dev[32];
    char            member[32];
    char            src[32];
    int             tstamp;
    igmp_member_item *next;
};

typedef struct {
    igmp_member_item *head;
} igmp_member_group;

igmp_member_group imGroup;

void
init_group()
{
    imGroup.head = NULL;
}

/*
 * igmp_timeout() { igmp_member_item *pItem = imGroup.head;
 * igmp_member_item *tmpItem = NULL; char cmd[128]=""; struct timeval
 * current_time;
 * 
 * gettimeofday(&current_time, NULL); while (pItem) { if
 * ((current_time.tv_sec - pItem->tstamp) > QUERY_TIMEOUT) {
 * //sprintf(cmd, "brctl clearportsnooping %s %s %s/%s", pItem->br,
 * pItem->dev, pItem->member, pItem->src); //system(cmd); if (!tmpItem)
 * imGroup.head = pItem->next; else tmpItem->next = pItem->next;
 * free(pItem); } tmpItem = pItem; pItem = pItem->next; } alarm(1); } 
 */

void
printGroup()
{
    igmp_member_item *pItem = imGroup.head;

    while (pItem) {
	printf("%s %s %s %s %i\n", pItem->br, pItem->dev, pItem->member,
	       pItem->src, pItem->tstamp);
	pItem = pItem->next;
    }
    printf("\n");
    return;
}

int
igmp_interface_add_membership(char *br, char *dev, char *member, char *src,
			      struct timeval *current_time)
{
    igmp_member_item *pItem = imGroup.head;
    igmp_member_item *sItem = NULL;

    while (pItem) {
	if ((!memcmp(pItem->br, br, strlen(br))) &&
	    (!memcmp(pItem->dev, dev, strlen(dev))) &&
	    (!memcmp(pItem->member, member, strlen(member))) &&
	    (!memcmp(pItem->src, src, strlen(src)))) {
	    pItem->tstamp = current_time->tv_sec;
	    return 0;
	}
	pItem = pItem->next;
    }

    if (!(sItem = (igmp_member_item *) malloc(sizeof(igmp_member_item))))
	return 0;
    strcpy(sItem->br, br);
    strcpy(sItem->dev, dev);
    strcpy(sItem->member, member);
    strcpy(sItem->src, src);
    sItem->tstamp = current_time->tv_sec;

    sItem->next = imGroup.head;
    imGroup.head = sItem;

    return 1;
}

int
igmp_interface_leave_membership(char *br, char *dev, char *member,
				char *src)
{
    igmp_member_item *pItem = imGroup.head;
    igmp_member_item *tmpItem = NULL;
    int             noremove = 0;

    while (pItem) {
	if ((!memcmp(pItem->br, br, strlen(br))) &&
	    (!memcmp(pItem->dev, dev, strlen(dev))) &&
	    (!memcmp(pItem->member, member, strlen(member)))) {
	    if (!memcmp(pItem->src, src, strlen(src))) {
		if (!tmpItem)
		    imGroup.head = pItem->next;
		else
		    tmpItem->next = pItem->next;
		free(pItem);
	    } else {
		noremove = 1;
	    }
	}
	tmpItem = pItem;
	pItem = pItem->next;
    }

    if (!noremove)
	return 1;

    return 0;
}


// // Do the following to make snooping cosistent with proxy
// if proxy try to remove the 236.1.2.3 for br0 from the system 
// go through our list
// remove all the br0, x, 236.1.2.3, x
// system("brctl remove br0 x, 236.1.2.3");
// 



/*
 * void igmprt_input()
 *
 * Process an incoming igmp message
 */
void
igmprt_input(igmp_router_t * igmprt, igmp_interface_t * ifp)
{

    struct msghdr   msg;
    struct iovec    iov;
    struct cmsghdr *cmsg;
    char           *ctrl = (char *) malloc(MAXCTRLSIZE);
    int             if_index = 0;
    struct in_pktinfo *info;
    struct ip      *iph;
    unsigned char  *ptype;
    int             n,
                    igmplen;
    igmpv2q_t      *query;
    struct in_addr  src,
                    dst;
    int             srsp = 1;
    int             badmsg = 0;
    igmp_interface_t *ifpi;
    struct sockaddr sa;

    char            cmd[128] = "";
    char            br[32];
    char            dev[32];
    char            dest[32];
    char            host[32];
    struct timeval  current_time;
#ifdef HND_FIX
    igmp_group_t   *gp;
#endif /* HND_FIX */

    /*
     * Read the igmp message 
     */
    iov.iov_base = (char *) ifp->igmpi_buf;
    iov.iov_len = ifp->igmpi_bufsize;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = MAXCTRLSIZE;
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(struct sockaddr);

    n = recvmsg(ifp->igmpi_socket, &msg, MSG_WAITALL);
    if (n <= sizeof(*iph)) {
	printf("^^^^^^^ igmprt_input:BAD packet received n=%d \n", n);
	free(ctrl);
	return;
    }
    // kernel: ip_sockglue.c
    // ip_cmsg_send(struct msghdr *msg,struct ipcm_cookie *ipc);
    // 

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	 cmsg = CMSG_NXTHDR(&msg, cmsg)) {
	if (cmsg->cmsg_type == IP_PKTINFO) {
	    info = (struct in_pktinfo *) CMSG_DATA(cmsg);
	    if_index = info->ipi_ifindex;
	} else {
	    printf("^^^^ BAD CMSG_HDR ^^^^^^^^\n");
	    badmsg = 1;
	}
    }
    free(ctrl);
    if (badmsg) {
	downstream->igmpi_ti_qi = 10;
	downstream->igmpi_isquerier = TRUE;
	return;
    }
    /*
     * Set pointer to start of report 
     */
    iph = (struct ip *) ifp->igmpi_buf;
    if ((igmplen = n - (iph->ip_hl << 2)) < IGMP_MINLEN)
	return;
    src = iph->ip_src;
    ptype = (unsigned char *)(ifp->igmpi_buf + (iph->ip_hl << 2));
    /*
     * lookup the network interface from which the packet arrived
     */
    ifpi = igmprt_interface_lookup_index(igmprt, if_index);
    if (ifpi == NULL)
	return;
    /*
     * Handle the message 
     */
    switch (*ptype) {
    case IGMP_MEMBERSHIP_QUERY:
	if (interface_lo)
	    return;
	// printf("source %s len=%d\n",(char *)inet_ntoa(src),igmplen);
	query = (igmpv2q_t *) (ifp->igmpi_buf + (iph->ip_hl << 2));
	if (query->igmpq_code == 0) {
	    /*
	     * version 1 query
	     */
	    receive_membership_query(ifpi, query->igmpq_group, NULL,
				     src.s_addr, 0, srsp);
	} else if (igmplen == 8) {
	    /*
	     * version 2 query
	     */
	    receive_membership_query(ifpi, query->igmpq_group, NULL,
				     src.s_addr, 0, TRUE);
	} else if (igmplen >= 12) {
	    /*
	     * version 3 query
	     */
	    // srsp=IGMP_SRSP(query);
	    // receive_membership_query(ifpi,query->igmpq_group,query->igmpq_sources,src.s_addr,query->igmpq_numsrc,srsp);
	}
	break;
    case IGMP_V1_MEMBERSHIP_REPORT:
    case IGMP_V2_MEMBERSHIP_REPORT:
	if (interface_lo)
	    return;
	if (ifpi->igmpi_type == UPSTREAM)
	    return;
	igmp_interface_membership_report_v12(igmprt, ifpi, src,
					     (igmpr_t *) ptype, igmplen);
	igmp_info_print(&router);	/* but this more beautiful */
	ioctl(ifp->igmpi_socket, SIOCGEXTIF, grpinfo);
	sscanf(grpinfo, "%s %s %s/%s", br, dev, dest, host);
	gettimeofday(&current_time, NULL);
	igmp_interface_add_membership(br, dev, dest, host, &current_time);
	if (strlen(grpinfo) > MIN_DEV_LEN) {
	    sprintf(cmd, "brctl setportsnooping %s 1> /dev/null", grpinfo);
#ifndef HND_FIX
	    system(cmd);
#endif /* HND_FIX */
	}
	break;
    case IGMP_V3_MEMBERSHIP_REPORT:
	// Keven Kuo
	if (ifpi->igmpi_type == DOWNSTREAM) {
	    if (ifpi->igmpi_isquerier == TRUE) {
		ifp->igmpi_ti_qi = ifp->igmpi_qi;
		ifpi->igmpi_version = IGMP_VERSION_2;
		dst.s_addr = 0;
		igmprt_membership_query(igmprt, ifpi, &dst, NULL, 0, 0);
	    }
	}
	break;
    case IGMP_V2_MEMBERSHIP_LEAVE:
	if (interface_lo)
	    return;
	IGMP_DBG("Membership leave received on if %d from %x\n", if_index,
	         src.s_addr);
	if (ifpi->igmpi_type != UPSTREAM) {
	    igmp_interface_membership_leave_v2(igmprt, ifpi, src,
					       (igmpr_t *) ptype, igmplen);
	    ioctl(ifp->igmpi_socket, SIOCGEXTIF, grpinfo);
	    sscanf(grpinfo, "%s %s %s/%s", br, dev, dest, host);
	    igmp_interface_leave_membership(br, dev, dest, host);
	    if (strlen(grpinfo) > MIN_DEV_LEN) {
		sprintf(cmd, "brctl clearportsnooping %s 1> /dev/null",
			grpinfo);
#ifndef HND_FIX
		system(cmd);
#endif /* HND_FIX */
	    }
	}
	break;
#ifdef HND_FIX
#define IGMPMSG_NOCACHE  1
    case IGMPMSG_NOCACHE:
	if (iph->ip_p == 0) {
	    if ((iph->ip_src.s_addr == 0) || (iph->ip_dst.s_addr == 0))
		return;
	} else
	    return;

	//printf("New message from kernel on if %d grp %x src %x ind %d\n",
	//       if_index, iph->ip_dst.s_addr, iph->ip_src.s_addr,
	//       ifp->igmpi_index);

	/*
	 * New (source, group) message from kernel, update/install route 
	 */
	for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	    if (ifp->igmpi_addr.s_addr == iph->ip_src.s_addr) {
		/*
		 * Ignore the MFC activation (source, group) message from
		 * ourself. 
		 */
		return;
	    }
	}

	/*
	 * Join for a new group on downstream interface 
	 */
	for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	    gp = igmp_interface_group_lookup(ifp, iph->ip_dst);
	    if (gp != NULL) {
		if (ifp->igmpi_type == DOWNSTREAM) {
		    gp->igmpg_source.s_addr = iph->ip_src.s_addr;
		    k_proxy_chg_mfc(igmprt->igmprt_socket,
				    iph->ip_src.s_addr, iph->ip_dst.s_addr,
				    ifp->igmpi_index, 1);
		}
	    }
	}
	// k_proxy_del_mfc(igmprt->igmprt_socket, iph->ip_src.s_addr,
	// iph->ip_dst.s_addr);
	break;
#endif /* HND_FIX */
    default:
	break;
    }
}

/*
 * void* igmprt_input_thread(void* arg)
 *
 * Wait on all interfaces for packets to arrive
 * igmp_router_t router;
 */
void           *
igmprt_input_thread(void *arg)
{
    igmp_router_t  *igmprt = (igmp_router_t *) arg;
    igmp_interface_t *ifp;
    fd_set          rset;
    int             n,
                    maxfd;
    struct timeval  to;

    /*
     * Add the sockets from all interfaces to the set 
     */
    FD_ZERO(&rset);
    maxfd = 0;
    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	if (ifp->igmpi_type == DOWNSTREAM) {
	    // printf("select: DOWNSTREAM\n");
	    FD_SET(ifp->igmpi_socket, &rset);
	    if (maxfd < ifp->igmpi_socket)
		maxfd = ifp->igmpi_socket;
	}
    }
    if (maxfd == 0) {
	syslog(LOG_NOTICE, "igmp:no interface available\n");
	return NULL;
    }
    /*
     * Wait for data on all sockets 
     */

    // printf("igmprt_input_thread: select maxfd=%d\n",maxfd);
    to.tv_sec = 0;
    to.tv_usec = 0;
    n = select(maxfd + 1, &rset, NULL, NULL, &to);
    // n--;
    // printf("igmprt_input_thread: select return n=%d\n",n);
    if (n > 0) {
	for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next) {
	    if (ifp->igmpi_type == DOWNSTREAM) {
		if (FD_ISSET(ifp->igmpi_socket, &rset))
		    igmprt_input(igmprt, ifp);
		if (--n == 0)
		    break;
	    }
	}
    }
    return NULL;
}

/*
 * void igmprt_start(igmp_router_t* igmprt)
 *
 * Start the threads of this router 
 */
void
igmprt_start(igmp_router_t * igmprt)
{
    /*
     * Return if already running 
     */
    if (igmprt->igmprt_running)
	return;

    if (!interface_lo) {
	/*
	 * Create and start the timer handling (thread) 
	 */
	igmprt->igmprt_flag_timer = 1;
    }

    /*
     * Create and start input handling (thread) 
     */
    igmprt->igmprt_flag_input = 1;

    igmprt->igmprt_running = 1;

}

/*
 * void igmprt_stop(igmp_router_t* igmprt)
 *
 * Stop the threads of this router 
 */
void
igmprt_stop(igmp_router_t * igmprt)
{

    /*
     * Return if not running 
     */
    if (!igmprt->igmprt_running)
	return;

    // k_stop_proxy(igmprt->igmprt_socket);

    /*
     * Signal threads to stop 
     */
    igmprt->igmprt_flag_timer = 0;
    igmprt->igmprt_flag_input = 0;

    /*
     * Wait for the threads to finish 
     */
    igmprt->igmprt_running = 0;
    /*
     * Make sure select of input-thread wakes up 
     */
    /*
     * if ((ifp = igmprt->igmprt_interfaces)) write(ifp->igmpi_socket, &i, 
     * sizeof(i));
     */
}

/*
 * void igmprt_print()
 *
 * Print the status of the igmpv3 proxy/router
 */
void
igmprt_print(igmp_router_t * igmprt)
{
    igmp_interface_t *ifp;

    assert(igmprt != NULL);
    for (ifp = igmprt->igmprt_interfaces; ifp; ifp = ifp->igmpi_next)
	igmp_interface_print(ifp);
}

/******************************************************************************
 *
 * The application
 *
 *****************************************************************************/

#define BUF_CMD   100

int             go_on = 1;
char           *pidfile = "/var/run/igmp_pid";

void
write_pid()
{
    FILE           *fp = fopen(pidfile, "w+");
    if (fp) {
	fprintf(fp, "%d\n", getpid());
	fclose(fp);
    } else
	printf("Cannot create pid file\n");
}

void
done(int sig)
{
    igmprt_destroy(&router);
    unlink(pidfile);
    exit(0);
}
#ifdef USE_DMALLOC
#define LOG_POLL_INTERVAL  20	// in sec.
void
dmallocLogPoll(int junk)
{
    dmalloc_log_unfreed();
    // printf("\n*** dmallocLogPoll after dmalloc_log_unfreed call\n");
    alarm(LOG_POLL_INTERVAL);
}
#endif


#ifdef BUILD_STATIC
int
igmp_main(int argc, char **argv)
#else
int
main(int argc, char **argv)
#endif
{
    struct sockaddr_in *psin;
    interface_list_t *ifl,
                   *ifp;
    vifi_t          vifi;
    // struct sigaction sa;
    igmp_interface_t *ifc;
    int             i;

#ifdef HND_FIX
    if (daemon(1, 1) == -1) {
	fprintf(stderr, "Could not deamonize igmp proxy\n");
	return (-1);
    }
#endif /* HND_FIX */

    write_pid();
    // printf("pid=%d \n",getpid());
    argc--;			/* skip igmp */
    argv++;
    for (i = 0; i < argc; i++) {
	strcpy(upstream_interface[i], *argv++);
	// Keven Kuo
	if (strcmp(upstream_interface[i], "lo") == 0)
	    interface_lo = TRUE;
    }
    openlog("igmp", LOG_PID | LOG_NDELAY, LOG_USER);
    syslog(LOG_NOTICE, "igmp started!\n");
    // a hack to wait for all ppp up

    /*
     * Initialize 
     */
    signal(SIGUSR1, done);
    signal(SIGKILL, done);
    signal(SIGABRT, done);
    signal(SIGTERM, done);
    signal(SIGHUP, done);
    // sa.sa_handler = igmprt_timer_thread;
    // sa.sa_flags = SA_RESTART;
    // sigaction(SIGALRM, &sa,NULL);
    querier = 1;
    version = 22;
    /*
     * Create and initialize the router 
     */
    igmprt_init(&router);
    // k_init_proxy(((igmp_router_t *) &router)->igmprt_socket);
    numvifs = 0;

    /*
     * wait for interfaces to be ready 
     */
    wait_for_interfaces();

    /*
     * Add all the multicast enabled ipv4 interfaces 
     */
    ifl = get_interface_list(AF_INET, IFF_MULTICAST, IFF_LOOPBACK);
    for (vifi = 0, ifp = ifl; ifp; ifp = ifp->ifl_next, vifi++) {
	psin = (struct sockaddr_in *) &ifp->ifl_addr;
	igmprt_interface_add(&router, psin->sin_addr, ifp->ifl_name,
			     ifp->ifl_index);
	k_proxy_add_vif(((igmp_router_t *) & router)->igmprt_socket,
			psin->sin_addr.s_addr, ifp->ifl_index);
	numvifs++;
    }
    free_interface_list(ifl);
    /*
     * Print the status of the router 
     */
    igmprt_print(&router);
    /*
     * Start the router 
     */
    igmprt_start(&router);
    // system("iptables -I INPUT 1 -p 2 -j ACCEPT");
    for (ifc = (&router)->igmprt_interfaces; ifc; ifc = ifc->igmpi_next) {
	if (ifc->igmpi_type == DOWNSTREAM) {
	    downstream = ifc;
	}
    }

#ifdef USE_DMALLOC
    /*
     * # basic debugging for non-freed memory 0x403 -- log-stats,
     * log-non-free, check-fence
     * 
     * # same as above plus heap check 0xc03 -- log-stats, log-non-free,
     * check-fence, check-heap
     * 
     * # same as above plus error abort 0x400c0b -- log-stats,
     * log-non-free, log-trans, check-fence, check-heap, error-abort
     * 
     * # debug with extensive checking (very extensive) 0x700c2b --
     * log-stats, log-non-free, log-trans, log-admin, check-fence,
     * check-heap, realloc-copy, free-blank, error-abort
     * 
     * Just replace the hex number below to change the dmalloc option,
     * eg. change "debug=0x403,log=gotToSyslog" to
     * "debug=0x400c0b,log=gotToSyslog for more checking. Change the
     * define on LOG_POLL_INTERVAL (in secondes) for how offen to send
     * the unfreed memory info to syslog. 
     */

    dmalloc_debug_setup("debug=0x403,log=goToSyslog");
    signal(SIGALRM, dmallocLogPoll);
    alarm(LOG_POLL_INTERVAL);
    printf("\n*** DMALLOC Initialized***\n");
#endif

    while (go_on) {
	igmprt_timer_thread(&router);
	igmprt_input_thread(&router);
    }

    /*
     * Done 
     */
    done(0);
    return 0;
}
/* FILE-CSTYLED */
