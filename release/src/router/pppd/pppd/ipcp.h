/*
 * ipcp.h - IP Control Protocol definitions.
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
 * $Id: ipcp.h,v 1.14 2002/12/04 23:03:32 paulus Exp $
 */

/*
 * Options.
 */
#define CI_ADDRS	1	/* IP Addresses */
#define CI_COMPRESSTYPE	2	/* Compression Type */
#define	CI_ADDR		3

#define CI_MS_DNS1	129	/* Primary DNS value */
#define CI_MS_WINS1	130	/* Primary WINS value */
#define CI_MS_DNS2	131	/* Secondary DNS value */
#define CI_MS_WINS2	132	/* Secondary WINS value */

#define MAX_STATES 16		/* from slcompress.h */

#define IPCP_VJMODE_OLD 1	/* "old" mode (option # = 0x0037) */
#define IPCP_VJMODE_RFC1172 2	/* "old-rfc"mode (option # = 0x002d) */
#define IPCP_VJMODE_RFC1332 3	/* "new-rfc"mode (option # = 0x002d, */
                                /*  maxslot and slot number compression) */

#define IPCP_VJ_COMP 0x002d	/* current value for VJ compression option*/
#define IPCP_VJ_COMP_OLD 0x0037	/* "old" (i.e, broken) value for VJ */
				/* compression option*/ 

typedef struct ipcp_options {
    bool neg_addr;		/* Negotiate IP Address? */
    bool old_addrs;		/* Use old (IP-Addresses) option? */
    bool req_addr;		/* Ask peer to send IP address? */
    bool default_route;		/* Assign default route through interface? */
    bool proxy_arp;		/* Make proxy ARP entry for peer? */
    bool neg_vj;		/* Van Jacobson Compression? */
    bool old_vj;		/* use old (short) form of VJ option? */
    bool accept_local;		/* accept peer's value for ouraddr */
    bool accept_remote;		/* accept peer's value for hisaddr */
    bool req_dns1;		/* Ask peer to send primary DNS address? */
    bool req_dns2;		/* Ask peer to send secondary DNS address? */
    int  vj_protocol;		/* protocol value to use in VJ option */
    int  maxslotindex;		/* values for RFC1332 VJ compression neg. */
    bool cflag;
    u_int32_t ouraddr, hisaddr;	/* Addresses in NETWORK BYTE ORDER */
    u_int32_t dnsaddr[2];	/* Primary and secondary MS DNS entries */
    u_int32_t winsaddr[2];	/* Primary and secondary MS WINS entries */
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

char *ip_ntoa __P((u_int32_t));

extern struct protent ipcp_protent;
