/*
 * ipxcp.h - IPX Control Protocol definitions.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ipxcp.h,v 1.5 2002/12/04 23:03:32 paulus Exp $
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
