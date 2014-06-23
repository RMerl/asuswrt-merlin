/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igmprt.h 341899 2012-06-29 04:06:38Z $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/param.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <bits/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <netinet/if_ether.h>
#include <net/if.h>

#include <netinet/in_systm.h>
#include <linux/ip.h>

#ifdef LINUX_2_6_36
#include <linux/in.h>
#endif
#include <linux/mroute.h>

#include <syslog.h>
#include <assert.h>

#include "util.h"
#include "igmp.h"


// eddie typedef u_short vifi_t; 
extern vifi_t   numvifs;
extern unsigned long upstream;
extern int      forward_upstream;

#define UNKNOWN -1
#define EMPTY 0
#define IGMPVERSION 1
#define IS_QUERIER  2
#define UPSTREAM    4
#define DOWNSTREAM  5

#define DEFAULT_VERSION 22
#define DEFAULT_ISQUERIER 1
#define configfile "igmprt.conf"

#define	MAX_MSGBUFSIZE		9180
#define MAXVIFS	                32
#define	MAX_ADDRS	       	500
#define TRUE	         	1
#define FALSE	         	0

// eddie
#define	INADDR_ALLRTRS_IGMPV3_GROUP 0xE0000016U
#define INADDR_DVMPR 0xE0000004U
#define INADDR_OSPF 0xE0000005U
#define INADDR_PIM 0xE0000009U

// eddie
#define MAXCTRLSIZE						\
	(sizeof(struct cmsghdr) + sizeof(struct sockaddr_in) +	\
	sizeof(struct cmsghdr) + sizeof(int) + 32)

#define CMSG_IFINDEX(cmsg) 				\
	(((struct sockaddr_dl*)(cmsg + 1))->sdl_index)	\

#define VALID_ADDR(x)\
     ((ntohl((x).s_addr) != INADDR_ALLRTRS_GROUP) &&  (ntohl((x).s_addr) != INADDR_ALLHOSTS_GROUP) && (ntohl((x).s_addr) != INADDR_ALLRTRS_IGMPV3_GROUP) && (ntohl((x).s_addr) != INADDR_DVMPR) && (ntohl((x).s_addr) != INADDR_PIM))


#define IGMP_DBG(fmt, args...)


// typedef u_short vifi_t; 
/*
 * IGMP interface type 
 */
typedef struct _igmp_interface_t {
    struct in_addr  igmpi_addr;
    char            igmpi_name[IFNAMSIZ];
    vifi_t          igmpi_index;
    int             igmpi_type;	/* interface type:upstream/downstream */
    igmp_group_t   *igmpi_groups;
    sch_query_t    *sch_group_query;
    int             igmpi_version;
    int             igmpi_isquerier;
    int             igmpi_qi;	/* query interval */
    int             igmpi_qri;	/* query response interval */
    int             igmpi_gmi;	/* group membership interval */
    int             igmpi_oqp;	/* other querier present timer */
    int             igmpi_rv;	/* robustness variable */
    int             igmpi_ti_qi;	/* timer: query interval */
    int             igmpi_socket;	/* igmp socket */
    struct _igmp_interface_t *igmpi_next;
    int             igmpi_save_flags;
    char           *igmpi_buf;
    int             igmpi_bufsize;
    int             igmpi_multienabled;	/* From Sean Lee's GUI per PVC */
} igmp_interface_t;

/*
 * proxy membership database 
 */
typedef struct membership_db {
    struct {
	struct in_addr  group;
	int             fmode;
	int             numsources;
	struct in_addr  sources[500];
    } membership;
    struct membership_db *next;
} membership_db;


/*
 * IGMP router type 
 */
typedef struct _igmp_router_t {
    igmp_interface_t *igmprt_interfaces;
    membership_db  *igmprt_membership_db;
    int             igmprt_flag_timer;
    int             igmprt_flag_input;
    int             igmprt_running;
    int             igmprt_up_socket;
    int		    igmprt_down_socket;
    int             igmprt_socket;
} igmp_router_t;


/***
 *
 * routines
 *
 ***/
/*
 * sources routines 
 */
igmp_src_t     *igmp_group_src_add(igmp_group_t * gp,
				   struct in_addr srcaddr);

igmp_src_t     *igmp_group_src_lookup(igmp_group_t * gp,
				      struct in_addr srcaddr);

void            igmp_src_cleanup(igmp_group_t * gp, igmp_src_t * src);



/*
 * group routines 
 */

igmp_group_t   *igmp_group_create(struct in_addr groupaddr);

void
                igmp_group_cleanup(igmp_interface_t * ifp, igmp_group_t * gp, igmp_router_t * router);

void
                igmp_group_handle_isex(igmp_router_t * router, igmp_interface_t * ifp, igmp_group_t * gp,
				       int numsrc,
				       struct in_addr *sources);

void
                igmp_group_print(igmp_group_t * gp);

/*
 * interface routines 
 */

igmp_interface_t *igmp_interface_create(igmp_router_t *router, struct in_addr ifaddr,
					char *ifname, vifi_t index);

void
                igmp_interface_cleanup(igmp_interface_t * ifp);

igmp_group_t   *igmp_interface_group_add(igmp_router_t * router,
					 igmp_interface_t * ifp,
					 struct in_addr groupaddr);

igmp_group_t   *igmp_interface_group_lookup(igmp_interface_t * ifp,
					    struct in_addr groupaddr);

void
                igmp_interface_membership_report_v12(igmp_router_t * router, igmp_interface_t * ifp, struct in_addr src,
						     igmpr_t * report,
						     int len);
void
                igmp_interface_membership_leave_v2(igmp_router_t * router, igmp_interface_t * ifp, struct in_addr src,
						   igmpr_t * report,
						   int len);

void
                igmp_interface_print(igmp_interface_t * ifp);

/*
 * router routines 
 */

int
                igmprt_init(igmp_router_t * igmprt);

void
                igmprt_cleanup(igmp_router_t * igmprt);

igmp_interface_t *igmprt_interface_lookup(igmp_router_t * igmprt,
					  struct in_addr ifaddr);

igmp_group_t   *igmprt_group_lookup(igmp_router_t * igmprt,
				    struct in_addr ifaddr,
				    struct in_addr groupaddr);

igmp_interface_t *igmprt_interface_add(igmp_router_t * igmprt,
				       struct in_addr ifaddr, char *ifname,
				       vifi_t index);

igmp_group_t   *igmprt_group_add(igmp_router_t * igmprt,
				 struct in_addr ifaddr,
				 struct in_addr groupaddr);

void
                igmprt_timer();

void           *igmprt_timer_thread(void *arg);

void
                igmprt_input(igmp_router_t * igmprt, igmp_interface_t * ifp);

void           *igmprt_input_thread(void *arg);

void
                igmprt_start(igmp_router_t * igmprt);

void
                igmprt_stop(igmp_router_t * igmprt);

void
                igmprt_print(igmp_router_t * igmprt);

void
                igmprt_membership_query(igmp_router_t * igmprt, igmp_interface_t * ifp,
					struct in_addr *group,
					struct in_addr *sources,
					int numsrc, int SRSP);

void
                receive_membership_query(igmp_interface_t * ifp, struct in_addr gp, struct in_addr *sources, u_long src_query, int numsrc, int srsp);

void
                send_sh_query(igmp_router_t * router, igmp_interface_t * ifp);
void
                send_group_specific_query(igmp_router_t * router, igmp_interface_t * ifp, igmp_group_t * gp);
void
                send_group_src_specific_q(igmp_router_t * router, igmp_interface_t * ifp, igmp_group_t * gp, struct in_addr *sources, int numsrc);

/*
 * proxy routines 
 */

void
                k_init_proxy(int socket);
void
                k_stop_proxy(int socket);
int
                k_proxy_add_vif(int socket, unsigned long vifaddr, vifi_t vifi);
int
                k_proxy_del_mfc(int socket, u_long source, u_long group);
int
                k_proxy_chg_mfc(int socket, u_long source, u_long group, vifi_t outvif, int fstate);
membership_db  *create_membership(struct in_addr group, int fmode,
				  int numsources, struct in_addr *sources);
membership_db  *find_membership(membership_db * membership,
				struct in_addr group);
membership_db  *update_multi(igmp_router_t * igmprt, struct in_addr group,
			     int fmode, int nsources,
			     struct in_addr *sources);
int
                find_source(struct in_addr sr, int nsources, struct in_addr *sources);
void            igmprt_timer_querier(igmp_interface_t * ifp);
void            igmprt_timer_group(igmp_router_t * router,
				   igmp_interface_t * ifp);
/* FILE-CSTYLED */
