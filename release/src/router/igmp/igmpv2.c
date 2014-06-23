/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igmpv2.c 241182 2011-02-17 21:50:03Z $
 */

/* FILE-CSTYLED */
#include "igmprt.h"

unsigned long   upstream;
int             forward_upstream;

extern int      inet_ntoa();
extern int      inet_addr();
/*
 * int check_src_set(src,set)
 * check if a source is in a set
 *
 */
int
check_src_set(struct in_addr src, igmp_src_t * set)
{
    igmp_src_t     *sr;
    for (sr = set; sr; sr = (igmp_src_t *) sr->igmps_next) {
	if (sr->igmps_source.s_addr == src.s_addr)
	    return TRUE;
    }
    return FALSE;
}
/*
 * int check_src (
 *   struct in_addr src,
 *     struct in_addr *sources,   
 *   int numsrc)   
 */
int
check_src(struct in_addr src, struct in_addr *sources, int numsrc)
{
    int             i;
    for (i = 0; i < numsrc; i++) {
	if (src.s_addr == sources[i].s_addr)
	    return TRUE;
    }
    return FALSE;
}

/*
 * void igmp_group_handle_allow
 * handle an allow report for a group
 *
 */
void
igmp_group_handle_allow(igmp_router_t * router,
			igmp_interface_t * ifp,
			igmp_group_t * gp,
			int numsrc, struct in_addr *sources)
{

}

/*
 * void igmp_group_handle_block
 * handle a block report for a group
 *
 */
void
igmp_group_handle_block(igmp_router_t * router,
			igmp_interface_t * ifp,
			igmp_group_t * gp,
			int numsrc, struct in_addr *sources)
{
}

/*
 * void igmp_group_toex()
 * Handle a to_ex{} report for a group
 */

void
igmp_group_handle_toex(igmp_router_t * router,
		       igmp_interface_t * ifp,
		       igmp_group_t * gp,
		       int numsrc, struct in_addr *sources)
{

}


/*
 * void igmp_group_toin()
 * handle to_in{} report for a group
 */
void
igmp_group_handle_toin(igmp_router_t * router,
		       igmp_interface_t * ifp,
		       igmp_group_t * gp,
		       int numsrc, struct in_addr *sources)
{
}

/*
 * void igmp_group_handle_isex()
 *
 * Handle a is_ex{A} report for a group 
 * the report have only one source
 */
void
igmp_group_handle_isex(igmp_router_t * router,
		       igmp_interface_t * ifp,
		       igmp_group_t * gp,
		       int numsrc, struct in_addr *sources)
{
    igmp_src_t     *src,
                   *old_src_set;
    int             i;

    /*
     * Reset timer 
     */
    gp->igmpg_timer = IGMP_GMI;	/* 260 */
    /*
     * Do the v3 logic 
     */
    old_src_set = gp->igmpg_sources;
    if (gp->igmpg_fmode == IGMP_FMODE_EXCLUDE) {
	/*
	 * $6.4.1: State = Excl(X,Y), Report = IS_EX(A) 
	 */
	for (i = 0; i < numsrc; i++) {
	    src = igmp_group_src_add(gp, sources[i]);
	    /*
	     * (A-X-Y) = GMI 
	     */
	    if ((check_src_set(src->igmps_source, old_src_set) == FALSE)
		&& (check_src(src->igmps_source, sources, numsrc) ==
		    TRUE)) {
		src->igmps_timer = IGMP_GMI;
		src->igmps_fstate = FALSE;
	    } else
		/*
		 * delete ( X-A) delete (Y-A) 
		 */
	    if ((check_src(src->igmps_source, sources, numsrc) ==
		     FALSE)
		    && (check_src_set(src->igmps_source, old_src_set)
			    == TRUE)) {
		// eddie igmp_src_cleanup(gp,sr);
		// eddie k_proxy_del_mfc (router->igmprt_socket,
		// sr->igmps_source.s_addr, gp->igmpg_addr.s_addr);
	    }
	}
    } else {
	/*
	 * $6.4.1: State = Incl(X,Y), Report = IS_EX(A) 
	 */
	for (i = 0; i < numsrc; i++) {
	    src = igmp_group_src_add(gp, sources[i]);
	    if ((check_src_set(src->igmps_source, old_src_set) == FALSE)
		&& (check_src(src->igmps_source, sources, numsrc) ==
		    TRUE)) {
		/*
		 * (B-A) = 0
		 */
		src->igmps_timer = 0;
	    } else {
		if ((check_src(src->igmps_source, sources, numsrc) ==
		     FALSE)
		    && (check_src_set(src->igmps_source, old_src_set) ==
			TRUE)) {
		    /*
		     * delete (A-B)
		     */
		    igmp_src_cleanup(gp, src);
		    k_proxy_del_mfc(router->igmprt_socket,
				    src->igmps_source.s_addr,
				    gp->igmpg_addr.s_addr);
		}
	    }
	}
    }
    gp->igmpg_fmode = IGMP_FMODE_EXCLUDE;

    /*
     * Member Join from Downstream 
     */
    if (ifp->igmpi_type != UPSTREAM) {
	// membership_dp is not used
	// member =
	// (membership_db*)update_multi(router,gp->igmpg_addr,gp->igmpg_fmode,numsrc,sources); 
	// 
#ifndef HND_FIX
	if (VALID_ADDR(gp->igmpg_addr)) {
	    k_proxy_chg_mfc(router->igmprt_socket, 0,
			    gp->igmpg_addr.s_addr, ifp->igmpi_index, 1);
	}
#endif /* HND_FIX */
    }
}

/*
 * void igmp_group_handle_isin()
 *
 * Handle a is_in{A} report for a group 
 * the report have only one source
 */
void
igmp_group_handle_isin(igmp_router_t * router,
		       igmp_interface_t * ifp,
		       igmp_group_t * gp,
		       int numsrc, struct in_addr *sources)
{
}

/***************************************************************************/
/*
 */
/*
 * Timer management routines 
 */
/************************************************************************* */

/*
 * void igmprt_timer_querier(igmp_interface_t *ifp)
 * handle the other querier timer
 *
 */
void
igmprt_timer_querier(igmp_interface_t * ifp)
{
    if (ifp->igmpi_oqp > 0)
	--ifp->igmpi_oqp;
    if (ifp->igmpi_oqp == 0)
	ifp->igmpi_isquerier = TRUE;
}

extern void igmp_info_print(igmp_router_t *);
extern void mcast_filter_del(unsigned char * ifname, struct in_addr mgrp_addr);

/*
 * void igmprt_timer_group(igmp_router_t* router, igmp_interface_t *ifp)
 *
 * handle the  groups timers for this router
 *
 */
void
igmprt_timer_group(igmp_router_t * router, igmp_interface_t * ifp)
{
    igmp_group_t   *gp;
    struct ip_mreq  mreq;
    struct in_addr  group_addr;
    igmp_interface_t *ifp1;

    for (gp = ifp->igmpi_groups; gp; gp = gp->igmpg_next) {
	if (gp->igmpg_addr.s_addr == inet_addr(IGMP_ALL_ROUTERS) ||
	    gp->igmpg_addr.s_addr == inet_addr(IGMP_ALL_ROUTERS_V3)) {
	    continue;
	}
	/*
	 * decrement group timer
	 */
	if (gp->igmpg_timer > 0) {
	    --gp->igmpg_timer;
	}
	/*
	 * handle group timer
	 */
	if (gp->igmpg_timer == 0) {
	    group_addr.s_addr = gp->igmpg_addr.s_addr;
	    igmp_group_cleanup(ifp, gp, router);
	    igmp_info_print(router);

	    /*
	     * Tell all upstream interfaces that we are gone 
	     */
	    for (ifp1 = router->igmprt_interfaces; ifp1;
		 ifp1 = ifp1->igmpi_next) {
		if (ifp1->igmpi_type == UPSTREAM) {
		    mreq.imr_multiaddr.s_addr = group_addr.s_addr;
		    mreq.imr_interface.s_addr = ifp1->igmpi_addr.s_addr;
		    if (VALID_ADDR(mreq.imr_multiaddr)) {
			IGMP_DBG("Deleting membership on interface %x for grp %x\n",
		       		 ifp1->igmpi_addr.s_addr, group_addr.s_addr);
			if (setsockopt
			    (router->igmprt_up_socket, IPPROTO_IP,
			     IP_DROP_MEMBERSHIP, (void *) &mreq,
			     sizeof(mreq)) < 0) {
			    IGMP_DBG("perror:setsockopt - IP_DROP_MEMBERSHIP\n");
			    // exit(1);
			}
		    }
#ifdef HND_FIX
                    mcast_filter_del((unsigned char *)ifp1->igmpi_name, group_addr);
#endif /* HND_FIX */
		}		/* if UPSTREAM */
	    }			/* for */
	}			/* if timer==0 */
    }				/* for groups */
}

/*
 * void igmprt_timer_source(igmp_router_t* router, igmp_interface_t *ifp)
 *
 * handle source timers
 */
void
igmprt_timer_source(igmp_router_t * router, igmp_interface_t * ifp)
{
}

/*****************************************************************************
 *
 *               Querry Routines
 *
 *****************************************************************************/
/*
 * sch_query_t * igmp_sch_create ( struct in_addr gp)
 * create a new scheduled entry
 */
sch_query_t    *
igmp_sch_create(struct in_addr gp)
{
    sch_query_t    *sh;
    if ((sh = (sch_query_t *) malloc(sizeof(*sh)))) {
	sh->gp_addr.s_addr = gp.s_addr;
	sh->sch_next = NULL;
    }
    return sh;
}
/*
 * void sch_group_specq_add(router,ifp,gp,sources,numsrc)
 * add a scheduled query entry
 */
void
sch_group_specq_add(igmp_interface_t * ifp,
		    struct in_addr gp, struct in_addr *sources, int numsrc)
{
    sch_query_t    *sch;
    int             i;
    if (numsrc != 0) {
	/*
	 * create the schedule entry
	 */
	sch = igmp_sch_create(gp);
	/*
	 * set the retransmissions number
	 */
	sch->igmp_retnum = IGMP_DEF_RV - 1;
	sch->numsrc = numsrc;
	for (i = 0; i < numsrc; i++)
	    sch->sources[i].s_addr = sources[i].s_addr;
	sch->sch_next = ifp->sch_group_query;
	ifp->sch_group_query = sch;
    } else
	return;
}


/*
 * void igmprt_membership_query()
 * Send a membership query on the specified interface, to the specified group.
 */
void
igmprt_membership_query(igmp_router_t * igmprt, igmp_interface_t * ifp,
			struct in_addr *group, struct in_addr *sources,
			int numsrc, int SRSP)
{
    char            buf[12];
    igmpv2q_t      *query;
    int             size;
    struct sockaddr_in sin;
    int             igmplen = 0;

    assert(igmprt != NULL);
    assert(ifp != NULL);

    query = (igmpv2q_t *) buf;

    /*
     * Set the common fields 
     */
    query->igmpq_type = IGMP_MEMBERSHIP_QUERY;
    query->igmpq_group.s_addr = group->s_addr;
    query->igmpq_cksum = 0;

    /*
     * Set the version specific fields 
     */
    switch (ifp->igmpi_version) {
    case IGMP_VERSION_1:
	igmplen = 8;
	query->igmpq_code = 0;
	break;
    case IGMP_VERSION_2:
	// printf("** sending version 2 query numsrc=%d***\n",numsrc);
	igmplen = 8;
	query->igmpq_code = ifp->igmpi_qri;
	break;
    case IGMP_VERSION_3:
	printf("VERSION 3 QUERY(do not processing)getchar()\n");
	getchar();
	break;
    default:
	return;
    }

    /*
     * Checksum 
     */
    query->igmpq_cksum = in_cksum((unsigned short *) query, igmplen);
    /*
     * Send out the query 
     */
    // eddie
    // size=sizeof(*query)+(numsrc-1)*sizeof(struct in_addr);
    size = sizeof(*query);
    sin.sin_family = AF_INET;
    if (group->s_addr == 0)
	sin.sin_addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
    else
	sin.sin_addr.s_addr = group->s_addr;
    sendto(ifp->igmpi_socket, (void *) query, size, 0,
	   (struct sockaddr *) &sin, sizeof(sin));
}

/*
 * void send_group_specific_query()
 * send a query to a specific group
 *
 */
void
send_group_specific_query(igmp_router_t * router,
			  igmp_interface_t * ifp, igmp_group_t * gp)
{
    int             SRSP = FALSE;
    if (gp->igmpg_timer > IGMP_TIMER_SCALE)
	SRSP = TRUE;
    else
	gp->igmpg_timer = IGMP_TIMER_SCALE;
    /*
     * send a group specific query imediately
     */
    igmprt_membership_query(router, ifp, &gp->igmpg_addr, NULL, 0, SRSP);
    /*
     * schedule retransmission
     */
    sch_group_specq_add(ifp, gp->igmpg_addr, NULL, 0);
}

/*
 * void send_group_src_specific_q()
 * send a group and source specific query
 */
void
send_group_src_specific_q(igmp_router_t * router,
			  igmp_interface_t * ifp,
			  igmp_group_t * gp,
			  struct in_addr *sources, int numsrc)
{
    int             i;
    igmp_src_t     *src;

    if (gp != NULL) {
	for (i = 0; i < numsrc; i++) {
	    src = igmp_group_src_lookup(gp, sources[i]);
	    if (src != NULL)
		src->igmps_timer = IGMP_TIMER_SCALE;
	    else
		return;
	}
	/*
	 * schedule retransmission
	 */
	sch_group_specq_add(ifp, gp->igmpg_addr, sources, numsrc);
    }
}

/*
 * void sch_query_cleanup(ifp,sch)
 * cleanup a scheduled record query from an inteeface
 *
 */
void
sch_query_cleanup(igmp_interface_t * ifp, sch_query_t * sch)
{
    sch_query_t    *sh;
    if (sch != ifp->sch_group_query) {
	for (sh = ifp->sch_group_query; sh->sch_next != sch;
	     sh = sh->sch_next);
	sh->sch_next = sch->sch_next;
	free(sch);
    } else {			/* delete the head */
	sh = ifp->sch_group_query;
	ifp->sch_group_query = sh->sch_next;
	free(sh);
    }
}

/*
 * void construct_set()
 * construct two sets of sources with source timer lower than LMQI
 * et another with source timer greater than LMQI
 */
void
construct_set(igmp_interface_t * ifp,
	      sch_query_t * sch,
	      struct in_addr *src_inf_lmqi,
	      struct in_addr *src_sup_lmqi, int *numsrc1, int *numsrc2)
{
    igmp_src_t     *src;
    igmp_group_t   *gp;
    int             i,
                    numsr1,
                    numsr2;
    /*
     * src_sup_lmqi=NULL; src_inf_lmqi=NULL;
     */

    numsr1 = numsr2 = 0;

    for (i = 0; i < sch->numsrc; i++) {
	/*
	 * lookup the group of the source
	 */
	if ((gp = igmp_interface_group_lookup(ifp, sch->gp_addr)) == NULL) {
	    *numsrc1 = numsr1;
	    *numsrc1 = numsr2;
	    return;
	}
	/*
	 * lookup the record source in the group
	 */
	if ((src = igmp_group_src_lookup(gp, sch->sources[i])) == NULL) {
	    *numsrc1 = numsr1;
	    *numsrc1 = numsr2;
	    return;
	}
	if (src->igmps_timer > IGMP_TIMER_SCALE) {
	    src_sup_lmqi[numsr1].s_addr = src->igmps_source.s_addr;
	    numsr1++;
	} else {
	    src_inf_lmqi[numsr2].s_addr = src->igmps_source.s_addr;
	    numsr2++;
	}
    }

    *numsrc1 = numsr1;
    *numsrc2 = numsr2;
}
/*
 * send_sh_query(router,ifp)
 * send scheduled query on an interface
 *
 */
void
send_sh_query(igmp_router_t * router, igmp_interface_t * ifp)
{
    sch_query_t    *sch;
    struct in_addr  src_inf_lmqi;
    struct in_addr  src_sup_lmqi;
    int             numsrc1;
    int             numsrc2;
    igmp_group_t   *gp;

    if (ifp->sch_group_query != NULL) {
	for (sch = ifp->sch_group_query; sch; sch = sch->sch_next) {
	    /*
	     * trait query per query
	     */
	    if (sch->numsrc == 0) {
		/*
		 * group specifq query
		 */
		if (sch->igmp_retnum > 0) {	/* another query yet */
		    if ((gp =
			 igmp_interface_group_lookup(ifp,
						     sch->gp_addr)) ==
			NULL) {
			return;
		    }
		    if (gp->igmpg_timer > IGMP_TIMER_SCALE)	/* group
								 * timer > 
								 * LMQI */
			igmprt_membership_query(router, ifp, &sch->gp_addr,
						NULL, 0, 1);
		    else
			igmprt_membership_query(router, ifp, &sch->gp_addr,
						NULL, 0, 0);
		    --sch->igmp_retnum;
		} else {	/* number retransmission = 0 */
		    /*
		     * delete the query record
		     */
		    sch_query_cleanup(ifp, sch);
		}
	    } else {
		/*
		 * group and source specifiq query
		 */
		if (sch->igmp_retnum > 0) {
		    construct_set(ifp, sch, &src_inf_lmqi, &src_sup_lmqi,
				  &numsrc1, &numsrc2);
		    /*
		     * send query of source with timer > LMQI
		     */
		    if (numsrc2 != 0) {
			igmprt_membership_query(router, ifp, &sch->gp_addr,
						&src_inf_lmqi, numsrc2, 0);
		    }
		    if (numsrc1 != 0) {
			igmprt_membership_query(router, ifp, &sch->gp_addr,
						&src_sup_lmqi, numsrc1, 1);
		    }

		    --sch->igmp_retnum;
		} else		/* retransmission =0 */
		    sch_query_cleanup(ifp, sch);

	    }
	}
    }

}
/*
 * void receive_membership_query()
 * handle a reception of membership query
 *
 */
void
receive_membership_query(igmp_interface_t * ifp,
			 struct in_addr gp,
			 struct in_addr *sources,
			 u_long src_query, int numsrc, int srsp)
{
    igmp_group_t   *gr;
    igmp_src_t     *src;
    int             i;
    if (src_query < ifp->igmpi_addr.s_addr) {	/* another querier is
						 * present with lower IP
						 * adress */
	ifp->igmpi_oqp = IGMP_OQPI;	/* (255)set the
					 * Other-Querier-Present timer to
					 * OQPI */
	ifp->igmpi_isquerier = FALSE;
    }
    if (srsp == FALSE) {	/* Supress Router-Side Processing flag not 
				 * set */
	gr = igmp_interface_group_lookup(ifp, gp);
	if (gr != NULL) {
	    if (numsrc == 0) {
		/*
		 * group specific query
		 */
		/*
		 * group timer is lowered to LMQI
		 */
		gr->igmpg_timer = IGMP_TIMER_SCALE;
	    } else {
		/*
		 * group and source specific query
		 */
		for (i = 0; i < numsrc; i++) {
		    src = igmp_group_src_lookup(gr, sources[i]);
		    if (src != NULL)
			src->igmps_timer = IGMP_TIMER_SCALE;
		}
	    }
	}
    }
}
/* FILE-CSTYLED */
