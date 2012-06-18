/*
 * ipxcp.h - IPX Control Protocol definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: ipxcp.h,v 1.1.1.4 2003/10/14 08:09:53 sparq Exp $
 */

/*
 * Options.
 */
#define IPX_NETWORK_NUMBER        1   /* IPX Network Number */
#define IPX_NODE_NUMBER           2
#define IPX_COMPRESSION_PROTOCOL  3
#define IPX_ROUTER_PROTOCOL       4
#define IPX_ROUTER_NAME           5
#define IPX_COMPLETE              6

/* Values for the router protocol */
#define IPX_NONE		  0
#define RIP_SAP			  2
#define NLSP			  4

typedef struct ipxcp_options {
    bool neg_node;		/* Negotiate IPX node number? */
    bool req_node;		/* Ask peer to send IPX node number? */

    bool neg_nn;		/* Negotiate IPX network number? */
    bool req_nn;		/* Ask peer to send IPX network number */

    bool neg_name;		/* Negotiate IPX router name */
    bool neg_complete;		/* Negotiate completion */
    bool neg_router;		/* Negotiate IPX router number */

    bool accept_local;		/* accept peer's value for ournode */
    bool accept_remote;		/* accept peer's value for hisnode */
    bool accept_network;	/* accept network number */

    bool tried_nlsp;		/* I have suggested NLSP already */
    bool tried_rip;		/* I have suggested RIP/SAP already */

    u_int32_t his_network;	/* base network number */
    u_int32_t our_network;	/* our value for network number */
    u_int32_t network;		/* the final network number */

    u_char his_node[6];		/* peer's node number */
    u_char our_node[6];		/* our node number */
    u_char name [48];		/* name of the router */
    int    router;		/* routing protocol */
} ipxcp_options;

extern fsm ipxcp_fsm[];
extern ipxcp_options ipxcp_wantoptions[];
extern ipxcp_options ipxcp_gotoptions[];
extern ipxcp_options ipxcp_allowoptions[];
extern ipxcp_options ipxcp_hisoptions[];

extern struct protent ipxcp_protent;
