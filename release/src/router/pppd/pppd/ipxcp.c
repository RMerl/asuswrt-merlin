/*
 * ipxcp.c - PPP IPX Control Protocol.
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
 */

#ifdef IPX_CHANGE

#define RCSID	"$Id: ipxcp.c,v 1.24 2005/08/25 23:59:34 paulus Exp $"

/*
 * TODO:
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "pppd.h"
#include "fsm.h"
#include "ipxcp.h"
#include "pathnames.h"
#include "magic.h"

static const char rcsid[] = RCSID;

/* global vars */
ipxcp_options ipxcp_wantoptions[NUM_PPP];	/* Options that we want to request */
ipxcp_options ipxcp_gotoptions[NUM_PPP];	/* Options that peer ack'd */
ipxcp_options ipxcp_allowoptions[NUM_PPP];	/* Options we allow peer to request */
ipxcp_options ipxcp_hisoptions[NUM_PPP];	/* Options that we ack'd */

#define wo (&ipxcp_wantoptions[0])
#define ao (&ipxcp_allowoptions[0])
#define go (&ipxcp_gotoptions[0])
#define ho (&ipxcp_hisoptions[0])

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void ipxcp_resetci __P((fsm *));	/* Reset our CI */
static int  ipxcp_cilen __P((fsm *));		/* Return length of our CI */
static void ipxcp_addci __P((fsm *, u_char *, int *)); /* Add our CI */
static int  ipxcp_ackci __P((fsm *, u_char *, int));	/* Peer ack'd our CI */
static int  ipxcp_nakci __P((fsm *, u_char *, int, int));/* Peer nak'd our CI */
static int  ipxcp_rejci __P((fsm *, u_char *, int));	/* Peer rej'd our CI */
static int  ipxcp_reqci __P((fsm *, u_char *, int *, int)); /* Rcv CI */
static void ipxcp_up __P((fsm *));		/* We're UP */
static void ipxcp_down __P((fsm *));		/* We're DOWN */
static void ipxcp_finished __P((fsm *));	/* Don't need lower layer */
static void ipxcp_script __P((fsm *, char *)); /* Run an up/down script */

fsm ipxcp_fsm[NUM_PPP];		/* IPXCP fsm structure */

static fsm_callbacks ipxcp_callbacks = { /* IPXCP callback routines */
    ipxcp_resetci,		/* Reset our Configuration Information */
    ipxcp_cilen,		/* Length of our Configuration Information */
    ipxcp_addci,		/* Add our Configuration Information */
    ipxcp_ackci,		/* ACK our Configuration Information */
    ipxcp_nakci,		/* NAK our Configuration Information */
    ipxcp_rejci,		/* Reject our Configuration Information */
    ipxcp_reqci,		/* Request peer's Configuration Information */
    ipxcp_up,			/* Called when fsm reaches OPENED state */
    ipxcp_down,			/* Called when fsm leaves OPENED state */
    NULL,			/* Called when we want the lower layer up */
    ipxcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "IPXCP"			/* String name of protocol */
};

/*
 * Command-line options.
 */
static int setipxnode __P((char **));
static void printipxnode __P((option_t *,
			      void (*)(void *, char *, ...), void *));
static int setipxname __P((char **));

static option_t ipxcp_option_list[] = {
    { "ipx", o_bool, &ipxcp_protent.enabled_flag,
      "Enable IPXCP (and IPX)", OPT_PRIO | 1 },
    { "+ipx", o_bool, &ipxcp_protent.enabled_flag,
      "Enable IPXCP (and IPX)", OPT_PRIOSUB | OPT_ALIAS | 1 },
    { "noipx", o_bool, &ipxcp_protent.enabled_flag,
      "Disable IPXCP (and IPX)", OPT_PRIOSUB },
    { "-ipx", o_bool, &ipxcp_protent.enabled_flag,
      "Disable IPXCP (and IPX)", OPT_PRIOSUB | OPT_ALIAS },

    { "ipx-network", o_uint32, &ipxcp_wantoptions[0].our_network,
      "Set our IPX network number", OPT_PRIO, &ipxcp_wantoptions[0].neg_nn },

    { "ipxcp-accept-network", o_bool, &ipxcp_wantoptions[0].accept_network,
      "Accept peer IPX network number", 1,
      &ipxcp_allowoptions[0].accept_network },

    { "ipx-node", o_special, (void *)setipxnode,
      "Set IPX node number", OPT_A2PRINTER, (void *)printipxnode },

    { "ipxcp-accept-local", o_bool, &ipxcp_wantoptions[0].accept_local,
      "Accept our IPX address", 1,
      &ipxcp_allowoptions[0].accept_local },

    { "ipxcp-accept-remote", o_bool, &ipxcp_wantoptions[0].accept_remote,
      "Accept peer's IPX address", 1,
      &ipxcp_allowoptions[0].accept_remote },

    { "ipx-routing", o_int, &ipxcp_wantoptions[0].router,
      "Set IPX routing proto number", OPT_PRIO,
      &ipxcp_wantoptions[0].neg_router },

    { "ipx-router-name", o_special, setipxname,
      "Set IPX router name", OPT_PRIO | OPT_A2STRVAL | OPT_STATIC,
       &ipxcp_wantoptions[0].name },

    { "ipxcp-restart", o_int, &ipxcp_fsm[0].timeouttime,
      "Set timeout for IPXCP", OPT_PRIO },
    { "ipxcp-max-terminate", o_int, &ipxcp_fsm[0].maxtermtransmits,
      "Set max #xmits for IPXCP term-reqs", OPT_PRIO },
    { "ipxcp-max-configure", o_int, &ipxcp_fsm[0].maxconfreqtransmits,
      "Set max #xmits for IPXCP conf-reqs", OPT_PRIO },
    { "ipxcp-max-failure", o_int, &ipxcp_fsm[0].maxnakloops,
      "Set max #conf-naks for IPXCP", OPT_PRIO },

    { NULL }
};

/*
 * Protocol entry points.
 */

static void ipxcp_init __P((int));
static void ipxcp_open __P((int));
static void ipxcp_close __P((int, char *));
static void ipxcp_lowerup __P((int));
static void ipxcp_lowerdown __P((int));
static void ipxcp_input __P((int, u_char *, int));
static void ipxcp_protrej __P((int));
static int  ipxcp_printpkt __P((u_char *, int,
				void (*) __P((void *, char *, ...)), void *));

struct protent ipxcp_protent = {
    PPP_IPXCP,
    ipxcp_init,
    ipxcp_input,
    ipxcp_protrej,
    ipxcp_lowerup,
    ipxcp_lowerdown,
    ipxcp_open,
    ipxcp_close,
    ipxcp_printpkt,
    NULL,
    0,
    "IPXCP",
    "IPX",
    ipxcp_option_list,
    NULL,
    NULL,
    NULL
};

/*
 * Lengths of configuration options.
 */

#define CILEN_VOID	2
#define CILEN_COMPLETE	2	/* length of complete option */
#define CILEN_NETN	6	/* network number length option */
#define CILEN_NODEN	8	/* node number length option */
#define CILEN_PROTOCOL	4	/* Minimum length of routing protocol */
#define CILEN_NAME	3	/* Minimum length of router name */
#define CILEN_COMPRESS	4	/* Minimum length of compression protocol */

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")

static int ipxcp_is_up;

static char *ipx_ntoa __P((u_int32_t));

/* Used in printing the node number */
#define NODE(base) base[0], base[1], base[2], base[3], base[4], base[5]

/* Used to generate the proper bit mask */
#define BIT(num)   (1 << (num))

/*
 * Convert from internal to external notation
 */

static short int
to_external(internal)
short int internal;
{
    short int  external;

    if (internal & BIT(IPX_NONE) )
        external = IPX_NONE;
    else
        external = RIP_SAP;

    return external;
}

/*
 * Make a string representation of a network IP address.
 */

static char *
ipx_ntoa(ipxaddr)
u_int32_t ipxaddr;
{
    static char b[64];
    slprintf(b, sizeof(b), "%x", ipxaddr);
    return b;
}


static u_char *
setipxnodevalue(src,dst)
u_char *src, *dst;
{
    int indx;
    int item;

    for (;;) {
        if (!isxdigit (*src))
	    break;
	
	for (indx = 0; indx < 5; ++indx) {
	    dst[indx] <<= 4;
	    dst[indx] |= (dst[indx + 1] >> 4) & 0x0F;
	}

	item = toupper (*src) - '0';
	if (item > 9)
	    item -= 7;

	dst[5] = (dst[5] << 4) | item;
	++src;
    }
    return src;
}

static int ipx_prio_our, ipx_prio_his;

static int
setipxnode(argv)
    char **argv;
{
    u_char *end;
    int have_his = 0;
    u_char our_node[6];
    u_char his_node[6];

    memset (our_node, 0, 6);
    memset (his_node, 0, 6);

    end = setipxnodevalue (*argv, our_node);
    if (*end == ':') {
	have_his = 1;
	end = setipxnodevalue (++end, his_node);
    }

    if (*end == '\0') {
        ipxcp_wantoptions[0].neg_node = 1;
	if (option_priority >= ipx_prio_our) {
	    memcpy(&ipxcp_wantoptions[0].our_node[0], our_node, 6);
	    ipx_prio_our = option_priority;
	}
	if (have_his && option_priority >= ipx_prio_his) {
	    memcpy(&ipxcp_wantoptions[0].his_node[0], his_node, 6);
	    ipx_prio_his = option_priority;
	}
        return 1;
    }

    option_error("invalid parameter '%s' for ipx-node option", *argv);
    return 0;
}

static void
printipxnode(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	unsigned char *p;

	p = ipxcp_wantoptions[0].our_node;
	if (ipx_prio_our)
		printer(arg, "%.2x%.2x%.2x%.2x%.2x%.2x",
			p[0], p[1], p[2], p[3], p[4], p[5]);
	printer(arg, ":");
	p = ipxcp_wantoptions[0].his_node;
	if (ipx_prio_his)
		printer(arg, "%.2x%.2x%.2x%.2x%.2x%.2x",
			p[0], p[1], p[2], p[3], p[4], p[5]);
}

static int
setipxname (argv)
    char **argv;
{
    u_char *dest = ipxcp_wantoptions[0].name;
    char *src  = *argv;
    int  count;
    char ch;

    ipxcp_wantoptions[0].neg_name  = 1;
    ipxcp_allowoptions[0].neg_name = 1;
    memset (dest, '\0', sizeof (ipxcp_wantoptions[0].name));

    count = 0;
    while (*src) {
        ch = *src++;
	if (! isalnum (ch) && ch != '_') {
	    option_error("IPX router name must be alphanumeric or _");
	    return 0;
	}

	if (count >= sizeof (ipxcp_wantoptions[0].name) - 1) {
	    option_error("IPX router name is limited to %d characters",
			 sizeof (ipxcp_wantoptions[0].name) - 1);
	    return 0;
	}

	dest[count++] = toupper (ch);
    }
    dest[count] = 0;

    return 1;
}

/*
 * ipxcp_init - Initialize IPXCP.
 */
static void
ipxcp_init(unit)
    int unit;
{
    fsm *f = &ipxcp_fsm[unit];

    f->unit	 = unit;
    f->protocol	 = PPP_IPXCP;
    f->callbacks = &ipxcp_callbacks;
    fsm_init(&ipxcp_fsm[unit]);

    memset (wo->name,	  0, sizeof (wo->name));
    memset (wo->our_node, 0, sizeof (wo->our_node));
    memset (wo->his_node, 0, sizeof (wo->his_node));

    wo->neg_nn	       = 1;
    wo->neg_complete   = 1;
    wo->network	       = 0;

    ao->neg_node       = 1;
    ao->neg_nn	       = 1;
    ao->neg_name       = 1;
    ao->neg_complete   = 1;
    ao->neg_router     = 1;

    ao->accept_local   = 0;
    ao->accept_remote  = 0;
    ao->accept_network = 0;

    wo->tried_rip      = 0;
    wo->tried_nlsp     = 0;
}

/*
 * Copy the node number
 */

static void
copy_node (src, dst)
u_char *src, *dst;
{
    memcpy (dst, src, sizeof (ipxcp_wantoptions[0].our_node));
}

/*
 * Compare node numbers
 */

static int
compare_node (src, dst)
u_char *src, *dst;
{
    return memcmp (dst, src, sizeof (ipxcp_wantoptions[0].our_node)) == 0;
}

/*
 * Is the node number zero?
 */

static int
zero_node (node)
u_char *node;
{
    int indx;
    for (indx = 0; indx < sizeof (ipxcp_wantoptions[0].our_node); ++indx)
	if (node [indx] != 0)
	    return 0;
    return 1;
}

/*
 * Increment the node number
 */

static void
inc_node (node)
u_char *node;
{
    u_char   *outp;
    u_int32_t magic_num;

    outp      = node;
    magic_num = magic();
    *outp++   = '\0';
    *outp++   = '\0';
    PUTLONG (magic_num, outp);
}

/*
 * ipxcp_open - IPXCP is allowed to come up.
 */
static void
ipxcp_open(unit)
    int unit;
{
    fsm_open(&ipxcp_fsm[unit]);
}

/*
 * ipxcp_close - Take IPXCP down.
 */
static void
ipxcp_close(unit, reason)
    int unit;
    char *reason;
{
    fsm_close(&ipxcp_fsm[unit], reason);
}


/*
 * ipxcp_lowerup - The lower layer is up.
 */
static void
ipxcp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ipxcp_fsm[unit]);
}


/*
 * ipxcp_lowerdown - The lower layer is down.
 */
static void
ipxcp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ipxcp_fsm[unit]);
}


/*
 * ipxcp_input - Input IPXCP packet.
 */
static void
ipxcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&ipxcp_fsm[unit], p, len);
}


/*
 * ipxcp_protrej - A Protocol-Reject was received for IPXCP.
 *
 * Pretend the lower layer went down, so we shut up.
 */
static void
ipxcp_protrej(unit)
    int unit;
{
    fsm_lowerdown(&ipxcp_fsm[unit]);
}


/*
 * ipxcp_resetci - Reset our CI.
 */
static void
ipxcp_resetci(f)
    fsm *f;
{
    wo->req_node = wo->neg_node && ao->neg_node;
    wo->req_nn	 = wo->neg_nn	&& ao->neg_nn;

    if (wo->our_network == 0) {
	wo->neg_node	   = 1;
	ao->accept_network = 1;
    }
/*
 * If our node number is zero then change it.
 */
    if (zero_node (wo->our_node)) {
	inc_node (wo->our_node);
	ao->accept_local = 1;
	wo->neg_node	 = 1;
    }
/*
 * If his node number is zero then change it.
 */
    if (zero_node (wo->his_node)) {
	inc_node (wo->his_node);
	ao->accept_remote = 1;
    }
/*
 * If no routing agent was specified then we do RIP/SAP according to the
 * RFC documents. If you have specified something then OK. Otherwise, we
 * do RIP/SAP.
 */
    if (ao->router == 0) {
	ao->router |= BIT(RIP_SAP);
	wo->router |= BIT(RIP_SAP);
    }

    /* Always specify a routing protocol unless it was REJected. */
    wo->neg_router = 1;
/*
 * Start with these default values
 */
    *go = *wo;
}

/*
 * ipxcp_cilen - Return length of our CI.
 */

static int
ipxcp_cilen(f)
    fsm *f;
{
    int len;

    len	 = go->neg_nn	    ? CILEN_NETN     : 0;
    len += go->neg_node	    ? CILEN_NODEN    : 0;
    len += go->neg_name	    ? CILEN_NAME + strlen ((char *)go->name) - 1 : 0;

    /* RFC says that defaults should not be included. */
    if (go->neg_router && to_external(go->router) != RIP_SAP)
        len += CILEN_PROTOCOL;

    return (len);
}


/*
 * ipxcp_addci - Add our desired CIs to a packet.
 */
static void
ipxcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
/*
 * Add the options to the record.
 */
    if (go->neg_nn) {
	PUTCHAR (IPX_NETWORK_NUMBER, ucp);
	PUTCHAR (CILEN_NETN, ucp);
	PUTLONG (go->our_network, ucp);
    }

    if (go->neg_node) {
	int indx;
	PUTCHAR (IPX_NODE_NUMBER, ucp);
	PUTCHAR (CILEN_NODEN, ucp);
	for (indx = 0; indx < sizeof (go->our_node); ++indx)
	    PUTCHAR (go->our_node[indx], ucp);
    }

    if (go->neg_name) {
	    int cilen = strlen ((char *)go->name);
	int indx;
	PUTCHAR (IPX_ROUTER_NAME, ucp);
	PUTCHAR (CILEN_NAME + cilen - 1, ucp);
	for (indx = 0; indx < cilen; ++indx)
	    PUTCHAR (go->name [indx], ucp);
    }

    if (go->neg_router) {
        short external = to_external (go->router);
	if (external != RIP_SAP) {
	    PUTCHAR  (IPX_ROUTER_PROTOCOL, ucp);
	    PUTCHAR  (CILEN_PROTOCOL,      ucp);
	    PUTSHORT (external,            ucp);
	}
    }
}

/*
 * ipxcp_ackci - Ack our CIs.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
ipxcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    u_short cilen, citype, cishort;
    u_char cichar;
    u_int32_t cilong;

#define ACKCIVOID(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_VOID) < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_VOID || \
	    citype != opt) \
	    break; \
    }

#define ACKCICOMPLETE(opt,neg)	ACKCIVOID(opt, neg)

#define ACKCICHARS(opt, neg, val, cnt) \
    if (neg) { \
	int indx, count = cnt; \
	len -= (count + 2); \
	if (len < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != (count + 2) || \
	    citype != opt) \
	    break; \
	for (indx = 0; indx < count; ++indx) {\
	    GETCHAR(cichar, p); \
	    if (cichar != ((u_char *) &val)[indx]) \
	       break; \
	}\
	if (indx != count) \
	    break; \
    }

#define ACKCINODE(opt,neg,val) ACKCICHARS(opt,neg,val,sizeof(val))
#define ACKCINAME(opt,neg,val) ACKCICHARS(opt,neg,val,strlen((char *)val))

#define ACKCINETWORK(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_NETN) < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_NETN || \
	    citype != opt) \
	    break; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    break; \
    }

#define ACKCIPROTO(opt, neg, val) \
    if (neg) { \
	if (len < 2) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_PROTOCOL || citype != opt) \
	    break; \
	len -= cilen; \
	if (len < 0) \
	    break; \
	GETSHORT(cishort, p); \
	if (cishort != to_external (val) || cishort == RIP_SAP) \
	    break; \
      }
/*
 * Process the ACK frame in the order in which the frame was assembled
 */
    do {
	ACKCINETWORK  (IPX_NETWORK_NUMBER,  go->neg_nn,	    go->our_network);
	ACKCINODE     (IPX_NODE_NUMBER,	    go->neg_node,   go->our_node);
	ACKCINAME     (IPX_ROUTER_NAME,	    go->neg_name,   go->name);
	if (len > 0)
		ACKCIPROTO    (IPX_ROUTER_PROTOCOL, go->neg_router, go->router);
/*
 * This is the end of the record.
 */
	if (len == 0)
	    return (1);
    } while (0);
/*
 * The frame is invalid
 */
    IPXCPDEBUG(("ipxcp_ackci: received bad Ack!"));
    return (0);
}

/*
 * ipxcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if IPXCP is in the OPENED state.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */

static int
ipxcp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    u_char citype, cilen, *next;
    u_short s;
    u_int32_t l;
    ipxcp_options no;		/* options we've seen Naks for */
    ipxcp_options try;		/* options to request next time */

    BZERO(&no, sizeof(no));
    try = *go;

    while (len >= CILEN_VOID) {
	GETCHAR (citype, p);
	GETCHAR (cilen,	 p);
	len -= cilen;
	if (cilen < CILEN_VOID || len < 0)
	    goto bad;
	next = &p [cilen - CILEN_VOID];

	switch (citype) {
	case IPX_NETWORK_NUMBER:
	    if (!go->neg_nn || no.neg_nn || (cilen != CILEN_NETN))
		goto bad;
	    no.neg_nn = 1;

	    GETLONG(l, p);
	    if (treat_as_reject)
		try.neg_nn = 0;
	    else if (l && ao->accept_network)
		try.our_network = l;
	    break;

	case IPX_NODE_NUMBER:
	    if (!go->neg_node || no.neg_node || (cilen != CILEN_NODEN))
		goto bad;
	    no.neg_node = 1;

	    if (treat_as_reject)
		try.neg_node = 0;
	    else if (!zero_node (p) && ao->accept_local &&
		     ! compare_node (p, ho->his_node))
		copy_node (p, try.our_node);
	    break;

	    /* This has never been sent. Ignore the NAK frame */
	case IPX_COMPRESSION_PROTOCOL:
	    goto bad;

	case IPX_ROUTER_PROTOCOL:
	    if (!go->neg_router || (cilen < CILEN_PROTOCOL))
		goto bad;

	    GETSHORT (s, p);
	    if (s > 15)         /* This is just bad, but ignore for now. */
	        break;

	    s = BIT(s);
	    if (no.router & s)  /* duplicate NAKs are always bad */
		goto bad;

	    if (no.router == 0) /* Reset on first NAK only */
		try.router = 0;

	    no.router      |= s;
	    try.router     |= s;
	    try.neg_router  = 1;
	    break;

	    /* These, according to the RFC, must never be NAKed. */
	case IPX_ROUTER_NAME:
	case IPX_COMPLETE:
	    goto bad;

	    /* These are for options which we have not seen. */
	default:
	    break;
	}
	p = next;
    }

    /*
     * Do not permit the peer to force a router protocol which we do not
     * support. However, default to the condition that will accept "NONE".
     */
    try.router &= (ao->router | BIT(IPX_NONE));
    if (try.router == 0 && ao->router != 0)
	try.router = BIT(IPX_NONE);

    if (try.router != 0)
        try.neg_router = 1;
    
    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any options left, we ignore them.
     */
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    IPXCPDEBUG(("ipxcp_nakci: received bad Nak!"));
    return 0;
}

/*
 * ipxcp_rejci - Reject some of our CIs.
 */
static int
ipxcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    u_short cilen, citype, cishort;
    u_char cichar;
    u_int32_t cilong;
    ipxcp_options try;		/* options to request next time */

#define REJCINETWORK(opt, neg, val) \
    if (neg && p[0] == opt) { \
	if ((len -= CILEN_NETN) < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_NETN || \
	    citype != opt) \
	    break; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    break; \
	neg = 0; \
    }

#define REJCICHARS(opt, neg, val, cnt) \
    if (neg && p[0] == opt) { \
	int indx, count = cnt; \
	len -= (count + 2); \
	if (len < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != (count + 2) || \
	    citype != opt) \
	    break; \
	for (indx = 0; indx < count; ++indx) {\
	    GETCHAR(cichar, p); \
	    if (cichar != ((u_char *) &val)[indx]) \
	       break; \
	}\
	if (indx != count) \
	    break; \
	neg = 0; \
    }

#define REJCINODE(opt,neg,val) REJCICHARS(opt,neg,val,sizeof(val))
#define REJCINAME(opt,neg,val) REJCICHARS(opt,neg,val,strlen((char *)val))

#define REJCIVOID(opt, neg) \
    if (neg && p[0] == opt) { \
	if ((len -= CILEN_VOID) < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_VOID || citype != opt) \
	    break; \
	neg = 0; \
    }

/* a reject for RIP/SAP is invalid since we don't send it and you can't
   reject something which is not sent. (You can NAK, but you can't REJ.) */
#define REJCIPROTO(opt, neg, val, bit) \
    if (neg && p[0] == opt) { \
	if ((len -= CILEN_PROTOCOL) < 0) \
	    break; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_PROTOCOL) \
	    break; \
	GETSHORT(cishort, p); \
	if (cishort != to_external (val) || cishort == RIP_SAP) \
	    break; \
	neg = 0; \
    }
/*
 * Any Rejected CIs must be in exactly the same order that we sent.
 * Check packet length and CI length at each step.
 * If we find any deviations, then this packet is bad.
 */
    try = *go;

    do {
	REJCINETWORK (IPX_NETWORK_NUMBER,  try.neg_nn,	   try.our_network);
	REJCINODE    (IPX_NODE_NUMBER,	   try.neg_node,   try.our_node);
	REJCINAME    (IPX_ROUTER_NAME,	   try.neg_name,   try.name);
	REJCIPROTO   (IPX_ROUTER_PROTOCOL, try.neg_router, try.router, 0);
/*
 * This is the end of the record.
 */
	if (len == 0) {
	    if (f->state != OPENED)
		*go = try;
	    return (1);
	}
    } while (0);
/*
 * The frame is invalid at this point.
 */
    IPXCPDEBUG(("ipxcp_rejci: received bad Reject!"));
    return 0;
}

/*
 * ipxcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
ipxcp_reqci(f, inp, len, reject_if_disagree)
    fsm *f;
    u_char *inp;		/* Requested CIs */
    int *len;			/* Length of requested CIs */
    int reject_if_disagree;
{
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_int32_t cinetwork;	/* Parsed address values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */

    /*
     * Reset all his options.
     */
    BZERO(ho, sizeof(*ho));
    
    /*
     * Process all his options.
     */
    next = inp;
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p = next;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    IPXCPDEBUG(("ipxcp_reqci: bad CI length!"));
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	next += cilen;			/* Step to next CI */

	switch (citype) {		/* Check CI type */
/*
 * The network number must match. Choose the larger of the two.
 */
	case IPX_NETWORK_NUMBER:
	    /* if we wont negotiate the network number or the length is wrong
	       then reject the option */
	    if ( !ao->neg_nn || cilen != CILEN_NETN ) {
		orc = CONFREJ;
		break;		
	    }
	    GETLONG(cinetwork, p);

	    /* If the network numbers match then acknowledge them. */
	    if (cinetwork != 0) {
		ho->his_network = cinetwork;
		ho->neg_nn	= 1;
		if (wo->our_network == cinetwork)
		    break;
/*
 * If the network number is not given or we don't accept their change or
 * the network number is too small then NAK it.
 */
		if (! ao->accept_network || cinetwork < wo->our_network) {
		    DECPTR (sizeof (u_int32_t), p);
		    PUTLONG (wo->our_network, p);
		    orc = CONFNAK;
		}
		break;
	    }
/*
 * The peer sent '0' for the network. Give it ours if we have one.
 */
	    if (go->our_network != 0) {
		DECPTR (sizeof (u_int32_t), p);
		PUTLONG (wo->our_network, p);
		orc = CONFNAK;
/*
 * We don't have one. Reject the value.
 */
	    } else
		orc = CONFREJ;

	    break;
/*
 * The node number is required
 */
	case IPX_NODE_NUMBER:
	    /* if we wont negotiate the node number or the length is wrong
	       then reject the option */
	    if ( cilen != CILEN_NODEN ) {
		orc = CONFREJ;
		break;
	    }

	    copy_node (p, ho->his_node);
	    ho->neg_node = 1;
/*
 * If the remote does not have a number and we do then NAK it with the value
 * which we have for it. (We never have a default value of zero.)
 */
	    if (zero_node (ho->his_node)) {
		orc = CONFNAK;
		copy_node (wo->his_node, p);
		INCPTR (sizeof (wo->his_node), p);
		break;
	    }
/*
 * If you have given me the expected network node number then I'll accept
 * it now.
 */
	    if (compare_node (wo->his_node, ho->his_node)) {
		orc = CONFACK;
		ho->neg_node = 1;
		INCPTR (sizeof (wo->his_node), p);
		break;
	    }
/*
 * If his node number is the same as ours then ask him to try the next
 * value.
 */
	    if (compare_node (ho->his_node, go->our_node)) {
		inc_node (ho->his_node);
		orc = CONFNAK;
		copy_node (ho->his_node, p);
		INCPTR (sizeof (wo->his_node), p);
		break;
	    }
/*
 * If we don't accept a new value then NAK it.
 */
	    if (! ao->accept_remote) {
		copy_node (wo->his_node, p);
		INCPTR (sizeof (wo->his_node), p);
		orc = CONFNAK;
		break;
	    }
	    orc = CONFACK;
	    ho->neg_node = 1;
	    INCPTR (sizeof (wo->his_node), p);
	    break;
/*
 * Compression is not desired at this time. It is always rejected.
 */
	case IPX_COMPRESSION_PROTOCOL:
	    orc = CONFREJ;
	    break;
/*
 * The routing protocol is a bitmask of various types. Any combination
 * of the values RIP_SAP and NLSP are permissible. 'IPX_NONE' for no
 * routing protocol must be specified only once.
 */
	case IPX_ROUTER_PROTOCOL:
	    if ( !ao->neg_router || cilen < CILEN_PROTOCOL ) {
		orc = CONFREJ;
		break;		
	    }

	    GETSHORT (cishort, p);

	    if (wo->neg_router == 0) {
	        wo->neg_router = 1;
		wo->router     = BIT(IPX_NONE);
	    }

	    if ((cishort == IPX_NONE && ho->router != 0) ||
		(ho->router & BIT(IPX_NONE))) {
		orc = CONFREJ;
		break;
	    }

	    cishort = BIT(cishort);
	    if (ho->router & cishort) {
		orc = CONFREJ;
		break;
	    }

	    ho->router	  |= cishort;
	    ho->neg_router = 1;

	    /* Finally do not allow a router protocol which we do not
	       support. */

	    if ((cishort & (ao->router | BIT(IPX_NONE))) == 0) {
	        int protocol;

		if (cishort == BIT(NLSP) &&
		    (ao->router & BIT(RIP_SAP)) &&
		    !wo->tried_rip) {
		    protocol      = RIP_SAP;
		    wo->tried_rip = 1;
		} else
		    protocol = IPX_NONE;

		DECPTR (sizeof (u_int16_t), p);
		PUTSHORT (protocol, p);
		orc = CONFNAK;
	    }
	    break;
/*
 * The router name is advisorary. Just accept it if it is not too large.
 */
	case IPX_ROUTER_NAME:
	    if (cilen >= CILEN_NAME) {
		int name_size = cilen - CILEN_NAME;
		if (name_size > sizeof (ho->name))
		    name_size = sizeof (ho->name) - 1;
		memset (ho->name, 0, sizeof (ho->name));
		memcpy (ho->name, p, name_size);
		ho->name [name_size] = '\0';
		ho->neg_name = 1;
		orc = CONFACK;
		break;
	    }
	    orc = CONFREJ;
	    break;
/*
 * This is advisorary.
 */
	case IPX_COMPLETE:
	    if (cilen != CILEN_COMPLETE)
		orc = CONFREJ;
	    else {
		ho->neg_complete = 1;
		orc = CONFACK;
	    }
	    break;
/*
 * All other entries are not known at this time.
 */
	default:
	    orc = CONFREJ;
	    break;
	}
endswitch:
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (reject_if_disagree)	/* Getting fed up with sending NAKs? */
		orc = CONFREJ;		/* Get tough if so */
	    if (rc == CONFREJ)		/* Rejecting prior CI? */
		continue;		/* Don't send this one */
	    if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		rc  = CONFNAK;		/* Not anymore... */
		ucp = inp;		/* Backup */
	    }
	}

	if (orc == CONFREJ &&		/* Reject this CI */
	    rc != CONFREJ) {		/*  but no prior ones? */
	    rc = CONFREJ;
	    ucp = inp;			/* Backup */
	}

	/* Need to move CI? */
	if (ucp != cip)
	    BCOPY(cip, ucp, cilen);	/* Move it */

	/* Update output pointer */
	INCPTR(cilen, ucp);
    }

    /*
     * If we aren't rejecting this packet, and we want to negotiate
     * their address, and they didn't send their address, then we
     * send a NAK with a IPX_NODE_NUMBER option appended. We assume the
     * input buffer is long enough that we can append the extra
     * option safely.
     */

    if (rc != CONFREJ && !ho->neg_node &&
	wo->req_nn && !reject_if_disagree) {
	if (rc == CONFACK) {
	    rc = CONFNAK;
	    wo->req_nn = 0;		/* don't ask again */
	    ucp = inp;			/* reset pointer */
	}

	if (zero_node (wo->his_node))
	    inc_node (wo->his_node);

	PUTCHAR (IPX_NODE_NUMBER, ucp);
	PUTCHAR (CILEN_NODEN, ucp);
	copy_node (wo->his_node, ucp);
	INCPTR (sizeof (wo->his_node), ucp);
    }

    *len = ucp - inp;			/* Compute output length */
    IPXCPDEBUG(("ipxcp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}

/*
 * ipxcp_up - IPXCP has come UP.
 *
 * Configure the IP network interface appropriately and bring it up.
 */

static void
ipxcp_up(f)
    fsm *f;
{
    int unit = f->unit;

    IPXCPDEBUG(("ipxcp: up"));

    /* The default router protocol is RIP/SAP. */
    if (ho->router == 0)
        ho->router = BIT(RIP_SAP);

    if (go->router == 0)
        go->router = BIT(RIP_SAP);

    /* Fetch the network number */
    if (!ho->neg_nn)
	ho->his_network = wo->his_network;

    if (!ho->neg_node)
	copy_node (wo->his_node, ho->his_node);

    if (!wo->neg_node && !go->neg_node)
	copy_node (wo->our_node, go->our_node);

    if (zero_node (go->our_node)) {
        static char errmsg[] = "Could not determine local IPX node address";
	if (debug)
	    error(errmsg);
	ipxcp_close(f->unit, errmsg);
	return;
    }

    go->network = go->our_network;
    if (ho->his_network != 0 && ho->his_network > go->network)
	go->network = ho->his_network;

    if (go->network == 0) {
        static char errmsg[] = "Can not determine network number";
	if (debug)
	    error(errmsg);
	ipxcp_close (unit, errmsg);
	return;
    }

    /* bring the interface up */
    if (!sifup(unit)) {
	if (debug)
	    warn("sifup failed (IPX)");
	ipxcp_close(unit, "Interface configuration failed");
	return;
    }
    ipxcp_is_up = 1;

    /* set the network number for IPX */
    if (!sipxfaddr(unit, go->network, go->our_node)) {
	if (debug)
	    warn("sipxfaddr failed");
	ipxcp_close(unit, "Interface configuration failed");
	return;
    }

    np_up(f->unit, PPP_IPX);

    /*
     * Execute the ipx-up script, like this:
     *	/etc/ppp/ipx-up interface tty speed local-IPX remote-IPX
     */

    ipxcp_script (f, _PATH_IPXUP);
}

/*
 * ipxcp_down - IPXCP has gone DOWN.
 *
 * Take the IP network interface down, clear its addresses
 * and delete routes through it.
 */

static void
ipxcp_down(f)
    fsm *f;
{
    IPXCPDEBUG(("ipxcp: down"));

    if (!ipxcp_is_up)
	return;
    ipxcp_is_up = 0;
    np_down(f->unit, PPP_IPX);
    cipxfaddr(f->unit);
    sifnpmode(f->unit, PPP_IPX, NPMODE_DROP);
    sifdown(f->unit);
    ipxcp_script (f, _PATH_IPXDOWN);
}


/*
 * ipxcp_finished - possibly shut down the lower layers.
 */
static void
ipxcp_finished(f)
    fsm *f;
{
    np_finished(f->unit, PPP_IPX);
}


/*
 * ipxcp_script - Execute a script with arguments
 * interface-name tty-name speed local-IPX remote-IPX networks.
 */
static void
ipxcp_script(f, script)
    fsm *f;
    char *script;
{
    char strspeed[32],	 strlocal[32],	   strremote[32];
    char strnetwork[32], strpid[32];
    char *argv[14],	 strproto_lcl[32], strproto_rmt[32];

    slprintf(strpid, sizeof(strpid), "%d", getpid());
    slprintf(strspeed, sizeof(strspeed),"%d", baud_rate);

    strproto_lcl[0] = '\0';
    if (go->neg_router && ((go->router & BIT(IPX_NONE)) == 0)) {
	if (go->router & BIT(RIP_SAP))
	    strlcpy (strproto_lcl, "RIP ", sizeof(strproto_lcl));
	if (go->router & BIT(NLSP))
	    strlcat (strproto_lcl, "NLSP ", sizeof(strproto_lcl));
    }

    if (strproto_lcl[0] == '\0')
	strlcpy (strproto_lcl, "NONE ", sizeof(strproto_lcl));

    strproto_lcl[strlen (strproto_lcl)-1] = '\0';

    strproto_rmt[0] = '\0';
    if (ho->neg_router && ((ho->router & BIT(IPX_NONE)) == 0)) {
	if (ho->router & BIT(RIP_SAP))
	    strlcpy (strproto_rmt, "RIP ", sizeof(strproto_rmt));
	if (ho->router & BIT(NLSP))
	    strlcat (strproto_rmt, "NLSP ", sizeof(strproto_rmt));
    }

    if (strproto_rmt[0] == '\0')
	strlcpy (strproto_rmt, "NONE ", sizeof(strproto_rmt));

    strproto_rmt[strlen (strproto_rmt)-1] = '\0';

    strlcpy (strnetwork, ipx_ntoa (go->network), sizeof(strnetwork));

    slprintf (strlocal, sizeof(strlocal), "%0.6B", go->our_node);

    slprintf (strremote, sizeof(strremote), "%0.6B", ho->his_node);

    argv[0]  = script;
    argv[1]  = ifname;
    argv[2]  = devnam;
    argv[3]  = strspeed;
    argv[4]  = strnetwork;
    argv[5]  = strlocal;
    argv[6]  = strremote;
    argv[7]  = strproto_lcl;
    argv[8]  = strproto_rmt;
    argv[9]  = (char *)go->name;
    argv[10] = (char *)ho->name;
    argv[11] = ipparam;
    argv[12] = strpid;
    argv[13] = NULL;
    run_program(script, argv, 0, NULL, NULL, 0);
}

/*
 * ipxcp_printpkt - print the contents of an IPXCP packet.
 */
static char *ipxcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
ipxcp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, olen;
    u_char *pstart, *optend;
    u_short cishort;
    u_int32_t cilong;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(ipxcp_codenames) / sizeof(char *))
	printer(arg, " %s", ipxcp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;
    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	/* print option list */
	while (len >= 2) {
	    GETCHAR(code, p);
	    GETCHAR(olen, p);
	    p -= 2;
	    if (olen < CILEN_VOID || olen > len) {
		break;
	    }
	    printer(arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
	    case IPX_NETWORK_NUMBER:
		if (olen == CILEN_NETN) {
		    p += 2;
		    GETLONG(cilong, p);
		    printer (arg, "network %s", ipx_ntoa (cilong));
		}
		break;
	    case IPX_NODE_NUMBER:
		if (olen == CILEN_NODEN) {
		    p += 2;
		    printer (arg, "node ");
		    while (p < optend) {
			GETCHAR(code, p);
			printer(arg, "%.2x", (int) (unsigned int) (unsigned char) code);
		    }
		}
		break;
	    case IPX_COMPRESSION_PROTOCOL:
		if (olen == CILEN_COMPRESS) {
		    p += 2;
		    GETSHORT (cishort, p);
		    printer (arg, "compression %d", (int) cishort);
		}
		break;
	    case IPX_ROUTER_PROTOCOL:
		if (olen == CILEN_PROTOCOL) {
		    p += 2;
		    GETSHORT (cishort, p);
		    printer (arg, "router proto %d", (int) cishort);
		}
		break;
	    case IPX_ROUTER_NAME:
		if (olen >= CILEN_NAME) {
		    p += 2;
		    printer (arg, "router name \"");
		    while (p < optend) {
			GETCHAR(code, p);
			if (code >= 0x20 && code <= 0x7E)
			    printer (arg, "%c", (int) (unsigned int) (unsigned char) code);
			else
			    printer (arg, " \\%.2x", (int) (unsigned int) (unsigned char) code);
		    }
		    printer (arg, "\"");
		}
		break;
	    case IPX_COMPLETE:
		if (olen == CILEN_COMPLETE) {
		    p += 2;
		    printer (arg, "complete");
		}
		break;
	    default:
		break;
	    }

	    while (p < optend) {
		GETCHAR(code, p);
		printer(arg, " %.2x", (int) (unsigned int) (unsigned char) code);
	    }
	    printer(arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
	    printer(arg, " ");
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
	printer(arg, " %.2x", (int) (unsigned int) (unsigned char) code);
    }

    return p - pstart;
}
#endif /* ifdef IPX_CHANGE */
