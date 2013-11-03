/*
 * ipcp.c - PPP IP Control Protocol.
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

#define RCSID	"$Id: ipcp.c,v 1.73 2008/05/26 08:33:22 paulus Exp $"

/*
 * TODO:
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pppd.h"
#include "fsm.h"
#include "ipcp.h"
#include "pathnames.h"

static const char rcsid[] = RCSID;

/* global vars */
ipcp_options ipcp_wantoptions[NUM_PPP];	/* Options that we want to request */
ipcp_options ipcp_gotoptions[NUM_PPP];	/* Options that peer ack'd */
ipcp_options ipcp_allowoptions[NUM_PPP]; /* Options we allow peer to request */
ipcp_options ipcp_hisoptions[NUM_PPP];	/* Options that we ack'd */

u_int32_t netmask = 0;		/* IP netmask to set on interface */

bool	disable_defaultip = 0;	/* Don't use hostname for default IP adrs */
bool	noremoteip = 0;		/* Let him have no IP address */

/* Hook for a plugin to know when IP protocol has come up */
void (*ip_up_hook) __P((void)) = NULL;

/* Hook for a plugin to know when IP protocol has come down */
void (*ip_down_hook) __P((void)) = NULL;

/* Hook for a plugin to choose the remote IP address */
void (*ip_choose_hook) __P((u_int32_t *)) = NULL;

/* Notifiers for when IPCP goes up and down */
struct notifier *ip_up_notifier = NULL;
struct notifier *ip_down_notifier = NULL;

/* local vars */
static int default_route_set[NUM_PPP];	/* Have set up a default route */
static int proxy_arp_set[NUM_PPP];	/* Have created proxy arp entry */
static bool usepeerdns;			/* Ask peer for DNS addrs */
static bool usepeerwins;		/* Ask peer for WINS addrs */
static int ipcp_is_up;			/* have called np_up() */
static int ipcp_is_open;		/* haven't called np_finished() */
static bool ask_for_local;		/* request our address from peer */
static char vj_value[8];		/* string form of vj option value */
static char netmask_str[20];		/* string form of netmask value */
static char *path_ipup = _PATH_IPUP;	/* pathname of ip-up script */
static char *path_ipdown = _PATH_IPDOWN;/* pathname of ip-down script */
static char *path_ippreup = _PATH_IPPREUP;/* pathname of ip-pre-up script */

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void ipcp_resetci __P((fsm *));	/* Reset our CI */
static int  ipcp_cilen __P((fsm *));	        /* Return length of our CI */
static void ipcp_addci __P((fsm *, u_char *, int *)); /* Add our CI */
static int  ipcp_ackci __P((fsm *, u_char *, int));	/* Peer ack'd our CI */
static int  ipcp_nakci __P((fsm *, u_char *, int, int));/* Peer nak'd our CI */
static int  ipcp_rejci __P((fsm *, u_char *, int));	/* Peer rej'd our CI */
static int  ipcp_reqci __P((fsm *, u_char *, int *, int)); /* Rcv CI */
static void ipcp_up __P((fsm *));		/* We're UP */
static void ipcp_down __P((fsm *));		/* We're DOWN */
static void ipcp_finished __P((fsm *));	/* Don't need lower layer */

fsm ipcp_fsm[NUM_PPP];		/* IPCP fsm structure */

static fsm_callbacks ipcp_callbacks = { /* IPCP callback routines */
    ipcp_resetci,		/* Reset our Configuration Information */
    ipcp_cilen,			/* Length of our Configuration Information */
    ipcp_addci,			/* Add our Configuration Information */
    ipcp_ackci,			/* ACK our Configuration Information */
    ipcp_nakci,			/* NAK our Configuration Information */
    ipcp_rejci,			/* Reject our Configuration Information */
    ipcp_reqci,			/* Request peer's Configuration Information */
    ipcp_up,			/* Called when fsm reaches OPENED state */
    ipcp_down,			/* Called when fsm leaves OPENED state */
    NULL,			/* Called when we want the lower layer up */
    ipcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "IPCP"			/* String name of protocol */
};

/*
 * Command-line options.
 */
static int setvjslots __P((char **));
static int setdnsaddr __P((char **));
static int setwinsaddr __P((char **));
static int setnetmask __P((char **));
int setipaddr __P((char *, char **, int));
static void printipaddr __P((option_t *, void (*)(void *, char *,...),void *));

static option_t ipcp_option_list[] = {
    { "noip", o_bool, &ipcp_protent.enabled_flag,
      "Disable IP and IPCP" },
    { "-ip", o_bool, &ipcp_protent.enabled_flag,
      "Disable IP and IPCP", OPT_ALIAS },

    { "novj", o_bool, &ipcp_wantoptions[0].neg_vj,
      "Disable VJ compression", OPT_A2CLR, &ipcp_allowoptions[0].neg_vj },
    { "-vj", o_bool, &ipcp_wantoptions[0].neg_vj,
      "Disable VJ compression", OPT_ALIAS | OPT_A2CLR,
      &ipcp_allowoptions[0].neg_vj },

    { "novjccomp", o_bool, &ipcp_wantoptions[0].cflag,
      "Disable VJ connection-ID compression", OPT_A2CLR,
      &ipcp_allowoptions[0].cflag },
    { "-vjccomp", o_bool, &ipcp_wantoptions[0].cflag,
      "Disable VJ connection-ID compression", OPT_ALIAS | OPT_A2CLR,
      &ipcp_allowoptions[0].cflag },

    { "vj-max-slots", o_special, (void *)setvjslots,
      "Set maximum VJ header slots",
      OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, vj_value },

    { "ipcp-accept-local", o_bool, &ipcp_wantoptions[0].accept_local,
      "Accept peer's address for us", 1 },
    { "ipcp-accept-remote", o_bool, &ipcp_wantoptions[0].accept_remote,
      "Accept peer's address for it", 1 },

    { "ipparam", o_string, &ipparam,
      "Set ip script parameter", OPT_PRIO },

    { "noipdefault", o_bool, &disable_defaultip,
      "Don't use name for default IP adrs", 1 },

    { "ms-dns", 1, (void *)setdnsaddr,
      "DNS address for the peer's use" },
    { "ms-wins", 1, (void *)setwinsaddr,
      "Nameserver for SMB over TCP/IP for peer" },

    { "ipcp-restart", o_int, &ipcp_fsm[0].timeouttime,
      "Set timeout for IPCP", OPT_PRIO },
    { "ipcp-max-terminate", o_int, &ipcp_fsm[0].maxtermtransmits,
      "Set max #xmits for term-reqs", OPT_PRIO },
    { "ipcp-max-configure", o_int, &ipcp_fsm[0].maxconfreqtransmits,
      "Set max #xmits for conf-reqs", OPT_PRIO },
    { "ipcp-max-failure", o_int, &ipcp_fsm[0].maxnakloops,
      "Set max #conf-naks for IPCP", OPT_PRIO },

    { "defaultroute", o_bool, &ipcp_wantoptions[0].default_route,
      "Add default route", OPT_ENABLE|1, &ipcp_allowoptions[0].default_route },
    { "nodefaultroute", o_bool, &ipcp_allowoptions[0].default_route,
      "disable defaultroute option", OPT_A2CLR,
      &ipcp_wantoptions[0].default_route },
    { "-defaultroute", o_bool, &ipcp_allowoptions[0].default_route,
      "disable defaultroute option", OPT_ALIAS | OPT_A2CLR,
      &ipcp_wantoptions[0].default_route },

    { "proxyarp", o_bool, &ipcp_wantoptions[0].proxy_arp,
      "Add proxy ARP entry", OPT_ENABLE|1, &ipcp_allowoptions[0].proxy_arp },
    { "noproxyarp", o_bool, &ipcp_allowoptions[0].proxy_arp,
      "disable proxyarp option", OPT_A2CLR,
      &ipcp_wantoptions[0].proxy_arp },
    { "-proxyarp", o_bool, &ipcp_allowoptions[0].proxy_arp,
      "disable proxyarp option", OPT_ALIAS | OPT_A2CLR,
      &ipcp_wantoptions[0].proxy_arp },

    { "usepeerdns", o_bool, &usepeerdns,
      "Ask peer for DNS address(es)", 1 },

    { "usepeerwins", o_bool, &usepeerwins,
      "Ask peer for WINS address(es)", 1 },

    { "netmask", o_special, (void *)setnetmask,
      "set netmask", OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, netmask_str },

    { "ipcp-no-addresses", o_bool, &ipcp_wantoptions[0].old_addrs,
      "Disable old-style IP-Addresses usage", OPT_A2CLR,
      &ipcp_allowoptions[0].old_addrs },
    { "ipcp-no-address", o_bool, &ipcp_wantoptions[0].neg_addr,
      "Disable IP-Address usage", OPT_A2CLR,
      &ipcp_allowoptions[0].neg_addr },
#ifdef __linux__
    { "noremoteip", o_bool, &noremoteip,
      "Allow peer to have no IP address", 1 },
#endif
    { "nosendip", o_bool, &ipcp_wantoptions[0].neg_addr,
      "Don't send our IP address to peer", OPT_A2CLR,
      &ipcp_wantoptions[0].old_addrs},

    { "IP addresses", o_wild, (void *) &setipaddr,
      "set local and remote IP addresses",
      OPT_NOARG | OPT_A2PRINTER, (void *) &printipaddr },

    { "ip-up-script", o_string, &path_ipup,
      "Set pathname of ip-up script", OPT_PRIV },
    { "ip-down-script", o_string, &path_ipdown,
      "Set pathname of ip-down script", OPT_PRIV },
    { "ip-pre-up-script", o_string, &path_ippreup,
      "Set pathname of ip-pre-up script", OPT_PRIV },

    { NULL }
};

/*
 * Protocol entry points from main code.
 */
static void ipcp_init __P((int));
static void ipcp_open __P((int));
static void ipcp_close __P((int, char *));
static void ipcp_lowerup __P((int));
static void ipcp_lowerdown __P((int));
static void ipcp_input __P((int, u_char *, int));
static void ipcp_protrej __P((int));
static int  ipcp_printpkt __P((u_char *, int,
			       void (*) __P((void *, char *, ...)), void *));
static void ip_check_options __P((void));
static int  ip_demand_conf __P((int));
static int  ip_active_pkt __P((u_char *, int));
static void create_resolv __P((u_int32_t, u_int32_t));

struct protent ipcp_protent = {
    PPP_IPCP,
    ipcp_init,
    ipcp_input,
    ipcp_protrej,
    ipcp_lowerup,
    ipcp_lowerdown,
    ipcp_open,
    ipcp_close,
    ipcp_printpkt,
    NULL,
    1,
    "IPCP",
    "IP",
    ipcp_option_list,
    ip_check_options,
    ip_demand_conf,
    ip_active_pkt
};

static void ipcp_clear_addrs __P((int, u_int32_t, u_int32_t));
static void ipcp_script __P((char *, int));	/* Run an up/down script */
static void ipcp_script_done __P((void *));

/*
 * Lengths of configuration options.
 */
#define CILEN_VOID	2
#define CILEN_COMPRESS	4	/* min length for compression protocol opt. */
#define CILEN_VJ	6	/* length for RFC1332 Van-Jacobson opt. */
#define CILEN_ADDR	6	/* new-style single address option */
#define CILEN_ADDRS	10	/* old-style dual address option */


#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")

/*
 * This state variable is used to ensure that we don't
 * run an ipcp-up/down script while one is already running.
 */
static enum script_state {
    s_down,
    s_up,
} ipcp_script_state;
static pid_t ipcp_script_pid;

/*
 * Make a string representation of a network IP address.
 */
char *
ip_ntoa(ipaddr)
u_int32_t ipaddr;
{
    static char b[64];

    slprintf(b, sizeof(b), "%I", ipaddr);
    return b;
}

/*
 * Option parsing.
 */

/*
 * setvjslots - set maximum number of connection slots for VJ compression
 */
static int
setvjslots(argv)
    char **argv;
{
    int value;

    if (!int_option(*argv, &value))
	return 0;
    if (value < 2 || value > 16) {
	option_error("vj-max-slots value must be between 2 and 16");
	return 0;
    }
    ipcp_wantoptions [0].maxslotindex =
        ipcp_allowoptions[0].maxslotindex = value - 1;
    slprintf(vj_value, sizeof(vj_value), "%d", value);
    return 1;
}

/*
 * setdnsaddr - set the dns address(es)
 */
static int
setdnsaddr(argv)
    char **argv;
{
    u_int32_t dns;
    struct hostent *hp;

    dns = inet_addr(*argv);
    if (dns == (u_int32_t) -1) {
	if ((hp = gethostbyname(*argv)) == NULL) {
	    option_error("invalid address parameter '%s' for ms-dns option",
			 *argv);
	    return 0;
	}
	dns = *(u_int32_t *)hp->h_addr;
    }

    /* We take the last 2 values given, the 2nd-last as the primary
       and the last as the secondary.  If only one is given it
       becomes both primary and secondary. */
    if (ipcp_allowoptions[0].dnsaddr[1] == 0)
	ipcp_allowoptions[0].dnsaddr[0] = dns;
    else
	ipcp_allowoptions[0].dnsaddr[0] = ipcp_allowoptions[0].dnsaddr[1];

    /* always set the secondary address value. */
    ipcp_allowoptions[0].dnsaddr[1] = dns;

    return (1);
}

/*
 * setwinsaddr - set the wins address(es)
 * This is primrarly used with the Samba package under UNIX or for pointing
 * the caller to the existing WINS server on a Windows NT platform.
 */
static int
setwinsaddr(argv)
    char **argv;
{
    u_int32_t wins;
    struct hostent *hp;

    wins = inet_addr(*argv);
    if (wins == (u_int32_t) -1) {
	if ((hp = gethostbyname(*argv)) == NULL) {
	    option_error("invalid address parameter '%s' for ms-wins option",
			 *argv);
	    return 0;
	}
	wins = *(u_int32_t *)hp->h_addr;
    }

    /* We take the last 2 values given, the 2nd-last as the primary
       and the last as the secondary.  If only one is given it
       becomes both primary and secondary. */
    if (ipcp_allowoptions[0].winsaddr[1] == 0)
	ipcp_allowoptions[0].winsaddr[0] = wins;
    else
	ipcp_allowoptions[0].winsaddr[0] = ipcp_allowoptions[0].winsaddr[1];

    /* always set the secondary address value. */
    ipcp_allowoptions[0].winsaddr[1] = wins;

    return (1);
}

/*
 * setipaddr - Set the IP address
 * If doit is 0, the call is to check whether this option is
 * potentially an IP address specification.
 * Not static so that plugins can call it to set the addresses
 */
int
setipaddr(arg, argv, doit)
    char *arg;
    char **argv;
    int doit;
{
    struct hostent *hp;
    char *colon;
    u_int32_t local, remote;
    ipcp_options *wo = &ipcp_wantoptions[0];
    static int prio_local = 0, prio_remote = 0;

    /*
     * IP address pair separated by ":".
     */
    if ((colon = strchr(arg, ':')) == NULL)
	return 0;
    if (!doit)
	return 1;
  
    /*
     * If colon first character, then no local addr.
     */
    if (colon != arg && option_priority >= prio_local) {
	*colon = '\0';
	if ((local = inet_addr(arg)) == (u_int32_t) -1) {
	    if ((hp = gethostbyname(arg)) == NULL) {
		option_error("unknown host: %s", arg);
		return 0;
	    }
	    local = *(u_int32_t *)hp->h_addr;
	}
	if (bad_ip_adrs(local)) {
	    option_error("bad local IP address %s", ip_ntoa(local));
	    return 0;
	}
	if (local != 0)
	    wo->ouraddr = local;
	*colon = ':';
	prio_local = option_priority;
    }
  
    /*
     * If colon last character, then no remote addr.
     */
    if (*++colon != '\0' && option_priority >= prio_remote) {
	if ((remote = inet_addr(colon)) == (u_int32_t) -1) {
	    if ((hp = gethostbyname(colon)) == NULL) {
		option_error("unknown host: %s", colon);
		return 0;
	    }
	    remote = *(u_int32_t *)hp->h_addr;
	    if (remote_name[0] == 0)
		strlcpy(remote_name, colon, sizeof(remote_name));
	}
	if (bad_ip_adrs(remote)) {
	    option_error("bad remote IP address %s", ip_ntoa(remote));
	    return 0;
	}
	if (remote != 0)
	    wo->hisaddr = remote;
	prio_remote = option_priority;
    }

    return 1;
}

static void
printipaddr(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	ipcp_options *wo = &ipcp_wantoptions[0];

	if (wo->ouraddr != 0)
		printer(arg, "%I", wo->ouraddr);
	printer(arg, ":");
	if (wo->hisaddr != 0)
		printer(arg, "%I", wo->hisaddr);
}

/*
 * setnetmask - set the netmask to be used on the interface.
 */
static int
setnetmask(argv)
    char **argv;
{
    u_int32_t mask;
    int n;
    char *p;

    /*
     * Unfortunately, if we use inet_addr, we can't tell whether
     * a result of all 1s is an error or a valid 255.255.255.255.
     */
    p = *argv;
    n = parse_dotted_ip(p, &mask);

    mask = htonl(mask);

    if (n == 0 || p[n] != 0 || (netmask & ~mask) != 0) {
	option_error("invalid netmask value '%s'", *argv);
	return 0;
    }

    netmask = mask;
    slprintf(netmask_str, sizeof(netmask_str), "%I", mask);

    return (1);
}

int
parse_dotted_ip(p, vp)
    char *p;
    u_int32_t *vp;
{
    int n;
    u_int32_t v, b;
    char *endp, *p0 = p;

    v = 0;
    for (n = 3;; --n) {
	b = strtoul(p, &endp, 0);
	if (endp == p)
	    return 0;
	if (b > 255) {
	    if (n < 3)
		return 0;
	    /* accept e.g. 0xffffff00 */
	    *vp = b;
	    return endp - p0;
	}
	v |= b << (n * 8);
	p = endp;
	if (n == 0)
	    break;
	if (*p != '.')
	    return 0;
	++p;
    }
    *vp = v;
    return p - p0;
}


/*
 * ipcp_init - Initialize IPCP.
 */
static void
ipcp_init(unit)
    int unit;
{
    fsm *f = &ipcp_fsm[unit];
    ipcp_options *wo = &ipcp_wantoptions[unit];
    ipcp_options *ao = &ipcp_allowoptions[unit];

    f->unit = unit;
    f->protocol = PPP_IPCP;
    f->callbacks = &ipcp_callbacks;
    fsm_init(&ipcp_fsm[unit]);

    /*
     * Some 3G modems use repeated IPCP NAKs as a way of stalling
     * until they can contact a server on the network, so we increase
     * the default number of NAKs we accept before we start treating
     * them as rejects.
     */
    f->maxnakloops = 100;

    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));

    wo->neg_addr = wo->old_addrs = 1;
    wo->neg_vj = 1;
    wo->vj_protocol = IPCP_VJ_COMP;
    wo->maxslotindex = MAX_STATES - 1; /* really max index */
    wo->cflag = 1;


    /* max slots and slot-id compression are currently hardwired in */
    /* ppp_if.c to 16 and 1, this needs to be changed (among other */
    /* things) gmc */

    ao->neg_addr = ao->old_addrs = 1;
    ao->neg_vj = 1;
    ao->maxslotindex = MAX_STATES - 1;
    ao->cflag = 1;

    /*
     * XXX These control whether the user may use the proxyarp
     * and defaultroute options.
     */
    ao->proxy_arp = 1;
    ao->default_route = 1;
}


/*
 * ipcp_open - IPCP is allowed to come up.
 */
static void
ipcp_open(unit)
    int unit;
{
    fsm_open(&ipcp_fsm[unit]);
    ipcp_is_open = 1;
}


/*
 * ipcp_close - Take IPCP down.
 */
static void
ipcp_close(unit, reason)
    int unit;
    char *reason;
{
    fsm_close(&ipcp_fsm[unit], reason);
}


/*
 * ipcp_lowerup - The lower layer is up.
 */
static void
ipcp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ipcp_fsm[unit]);
}


/*
 * ipcp_lowerdown - The lower layer is down.
 */
static void
ipcp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_input - Input IPCP packet.
 */
static void
ipcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&ipcp_fsm[unit], p, len);
}


/*
 * ipcp_protrej - A Protocol-Reject was received for IPCP.
 *
 * Pretend the lower layer went down, so we shut up.
 */
static void
ipcp_protrej(unit)
    int unit;
{
    fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_resetci - Reset our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
ipcp_resetci(f)
    fsm *f;
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    ipcp_options *ao = &ipcp_allowoptions[f->unit];

    wo->req_addr = (wo->neg_addr || wo->old_addrs) &&
	(ao->neg_addr || ao->old_addrs);
    if (wo->ouraddr == 0)
	wo->accept_local = 1;
    if (wo->hisaddr == 0)
	wo->accept_remote = 1;
    wo->req_dns1 = usepeerdns;	/* Request DNS addresses from the peer */
    wo->req_dns2 = usepeerdns;
    wo->req_wins1 = usepeerwins; /* Request WINS addresses from the peer */
    wo->req_wins2 = usepeerwins;
    *go = *wo;
    if (!ask_for_local)
	go->ouraddr = 0;
    if (ip_choose_hook) {
	ip_choose_hook(&wo->hisaddr);
	if (wo->hisaddr) {
	    wo->accept_remote = 0;
	}
    }
    BZERO(&ipcp_hisoptions[f->unit], sizeof(ipcp_options));
}


/*
 * ipcp_cilen - Return length of our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static int
ipcp_cilen(f)
    fsm *f;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *ho = &ipcp_hisoptions[f->unit];

#define LENCIADDRS(neg)		(neg ? CILEN_ADDRS : 0)
#define LENCIVJ(neg, old)	(neg ? (old? CILEN_COMPRESS : CILEN_VJ) : 0)
#define LENCIADDR(neg)		(neg ? CILEN_ADDR : 0)
#define LENCIDNS(neg)		LENCIADDR(neg)
#define LENCIWINS(neg)		LENCIADDR(neg)

    /*
     * First see if we want to change our options to the old
     * forms because we have received old forms from the peer.
     */
    if (go->neg_addr && go->old_addrs && !ho->neg_addr && ho->old_addrs)
	go->neg_addr = 0;
    if (wo->neg_vj && !go->neg_vj && !go->old_vj) {
	/* try an older style of VJ negotiation */
	/* use the old style only if the peer did */
	if (ho->neg_vj && ho->old_vj) {
	    go->neg_vj = 1;
	    go->old_vj = 1;
	    go->vj_protocol = ho->vj_protocol;
	}
    }

    return (LENCIADDRS(!go->neg_addr && go->old_addrs) +
	    LENCIVJ(go->neg_vj, go->old_vj) +
	    LENCIADDR(go->neg_addr) +
	    LENCIDNS(go->req_dns1) +
	    LENCIDNS(go->req_dns2) +
	    LENCIWINS(go->req_wins1) +
	    LENCIWINS(go->req_wins2)) ;
}


/*
 * ipcp_addci - Add our desired CIs to a packet.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
ipcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    int len = *lenp;

#define ADDCIADDRS(opt, neg, val1, val2) \
    if (neg) { \
	if (len >= CILEN_ADDRS) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ADDRS, ucp); \
	    l = ntohl(val1); \
	    PUTLONG(l, ucp); \
	    l = ntohl(val2); \
	    PUTLONG(l, ucp); \
	    len -= CILEN_ADDRS; \
	} else \
	    go->old_addrs = 0; \
    }

#define ADDCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if (len >= vjlen) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(vjlen, ucp); \
	    PUTSHORT(val, ucp); \
	    if (!old) { \
		PUTCHAR(maxslotindex, ucp); \
		PUTCHAR(cflag, ucp); \
	    } \
	    len -= vjlen; \
	} else \
	    neg = 0; \
    }

#define ADDCIADDR(opt, neg, val) \
    if (neg) { \
	if (len >= CILEN_ADDR) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ADDR, ucp); \
	    l = ntohl(val); \
	    PUTLONG(l, ucp); \
	    len -= CILEN_ADDR; \
	} else \
	    neg = 0; \
    }

#define ADDCIDNS(opt, neg, addr) \
    if (neg) { \
	if (len >= CILEN_ADDR) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ADDR, ucp); \
	    l = ntohl(addr); \
	    PUTLONG(l, ucp); \
	    len -= CILEN_ADDR; \
	} else \
	    neg = 0; \
    }

#define ADDCIWINS(opt, neg, addr) \
    if (neg) { \
	if (len >= CILEN_ADDR) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ADDR, ucp); \
	    l = ntohl(addr); \
	    PUTLONG(l, ucp); \
	    len -= CILEN_ADDR; \
	} else \
	    neg = 0; \
    }

    ADDCIADDRS(CI_ADDRS, !go->neg_addr && go->old_addrs, go->ouraddr,
	       go->hisaddr);

    ADDCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    ADDCIADDR(CI_ADDR, go->neg_addr, go->ouraddr);

    ADDCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

    ADDCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

    ADDCIWINS(CI_MS_WINS1, go->req_wins1, go->winsaddr[0]);

    ADDCIWINS(CI_MS_WINS2, go->req_wins2, go->winsaddr[1]);
    
    *lenp -= len;
}


/*
 * ipcp_ackci - Ack our CIs.
 * Called by fsm_rconfack, Receive Configure ACK.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
ipcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_short cilen, citype, cishort;
    u_int32_t cilong;
    u_char cimaxslotindex, cicflag;

    /*
     * CIs must be in exactly the same order that we sent...
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */

#define ACKCIADDRS(opt, neg, val1, val2) \
    if (neg) { \
	u_int32_t l; \
	if ((len -= CILEN_ADDRS) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ADDRS || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val1 != cilong) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val2 != cilong) \
	    goto bad; \
    }

#define ACKCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if ((len -= vjlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != vjlen || \
	    citype != opt)  \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	    GETCHAR(cimaxslotindex, p); \
	    if (cimaxslotindex != maxslotindex) \
		goto bad; \
	    GETCHAR(cicflag, p); \
	    if (cicflag != cflag) \
		goto bad; \
	} \
    }

#define ACKCIADDR(opt, neg, val) \
    if (neg) { \
	u_int32_t l; \
	if ((len -= CILEN_ADDR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ADDR || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val != cilong) \
	    goto bad; \
    }

#define ACKCIDNS(opt, neg, addr) \
    if (neg) { \
	u_int32_t l; \
	if ((len -= CILEN_ADDR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ADDR || citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (addr != cilong) \
	    goto bad; \
    }

#define ACKCIWINS(opt, neg, addr) \
    if (neg) { \
	u_int32_t l; \
	if ((len -= CILEN_ADDR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ADDR || citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (addr != cilong) \
	    goto bad; \
    }

    ACKCIADDRS(CI_ADDRS, !go->neg_addr && go->old_addrs, go->ouraddr,
	       go->hisaddr);

    ACKCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    ACKCIADDR(CI_ADDR, go->neg_addr, go->ouraddr);

    ACKCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

    ACKCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

    ACKCIWINS(CI_MS_WINS1, go->req_wins1, go->winsaddr[0]);

    ACKCIWINS(CI_MS_WINS2, go->req_wins2, go->winsaddr[1]);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    IPCPDEBUG(("ipcp_ackci: received bad Ack!"));
    return (0);
}

/*
 * ipcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if IPCP is in the OPENED state.
 * Calback from fsm_rconfnakrej - Receive Configure-Nak or Configure-Reject.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static int
ipcp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, cicflag;
    u_char citype, cilen, *next;
    u_short cishort;
    u_int32_t ciaddr1, ciaddr2, l, cidnsaddr, ciwinsaddr;
    ipcp_options no;		/* options we've seen Naks for */
    ipcp_options try;		/* options to request next time */

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIADDRS(opt, neg, code) \
    if ((neg) && \
	(cilen = p[1]) == CILEN_ADDRS && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	GETLONG(l, p); \
	ciaddr2 = htonl(l); \
	no.old_addrs = 1; \
	code \
    }

#define NAKCIVJ(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_COMPRESS || cilen == CILEN_VJ) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
        code \
    }

#define NAKCIADDR(opt, neg, code) \
    if (go->neg && \
	(cilen = p[1]) == CILEN_ADDR && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	no.neg = 1; \
	code \
    }

#define NAKCIDNS(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cidnsaddr = htonl(l); \
	no.neg = 1; \
	code \
    }

#define NAKCIWINS(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciwinsaddr = htonl(l); \
	no.neg = 1; \
	code \
    }

    /*
     * Accept the peer's idea of {our,his} address, if different
     * from our idea, only if the accept_{local,remote} flag is set.
     */
    NAKCIADDRS(CI_ADDRS, !go->neg_addr && go->old_addrs,
	       if (treat_as_reject) {
		   try.old_addrs = 0;
	       } else {
		   if (go->accept_local && ciaddr1) {
		       /* take his idea of our address */
		       try.ouraddr = ciaddr1;
		   }
		   if (go->accept_remote && ciaddr2) {
		       /* take his idea of his address */
		       try.hisaddr = ciaddr2;
		   }
	       }
	);

    /*
     * Accept the peer's value of maxslotindex provided that it
     * is less than what we asked for.  Turn off slot-ID compression
     * if the peer wants.  Send old-style compress-type option if
     * the peer wants.
     */
    NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
	    if (treat_as_reject) {
		try.neg_vj = 0;
	    } else if (cilen == CILEN_VJ) {
		GETCHAR(cimaxslotindex, p);
		GETCHAR(cicflag, p);
		if (cishort == IPCP_VJ_COMP) {
		    try.old_vj = 0;
		    if (cimaxslotindex < go->maxslotindex)
			try.maxslotindex = cimaxslotindex;
		    if (!cicflag)
			try.cflag = 0;
		} else {
		    try.neg_vj = 0;
		}
	    } else {
		if (cishort == IPCP_VJ_COMP || cishort == IPCP_VJ_COMP_OLD) {
		    try.old_vj = 1;
		    try.vj_protocol = cishort;
		} else {
		    try.neg_vj = 0;
		}
	    }
	    );

    NAKCIADDR(CI_ADDR, neg_addr,
	      if (treat_as_reject) {
		  try.neg_addr = 0;
		  try.old_addrs = 0;
	      } else if (go->accept_local && ciaddr1) {
		  /* take his idea of our address */
		  try.ouraddr = ciaddr1;
	      }
	      );

    NAKCIDNS(CI_MS_DNS1, req_dns1,
	     if (treat_as_reject) {
		 try.req_dns1 = 0;
	     } else {
		 try.dnsaddr[0] = cidnsaddr;
	     }
	     );

    NAKCIDNS(CI_MS_DNS2, req_dns2,
	     if (treat_as_reject) {
		 try.req_dns2 = 0;
	     } else {
		 try.dnsaddr[1] = cidnsaddr;
	     }
	     );

    NAKCIWINS(CI_MS_WINS1, req_wins1,
	     if (treat_as_reject) {
		 try.req_wins1 = 0;
	     } else {
		 try.winsaddr[0] = ciwinsaddr;
	     }
	     );

    NAKCIWINS(CI_MS_WINS2, req_wins2,
	     if (treat_as_reject) {
		 try.req_wins2 = 0;
	     } else {
		 try.winsaddr[1] = ciwinsaddr;
	     }
	     );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If they want to negotiate about IP addresses, we comply.
     * If they want us to ask for compression, we refuse.
     * If they want us to ask for ms-dns, we do that, since some
     * peers get huffy if we don't.
     */
    while (len >= CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if ( cilen < CILEN_VOID || (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_COMPRESSTYPE:
	    if (go->neg_vj || no.neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS))
		goto bad;
	    no.neg_vj = 1;
	    break;
	case CI_ADDRS:
	    if ((!go->neg_addr && go->old_addrs) || no.old_addrs
		|| cilen != CILEN_ADDRS)
		goto bad;
	    try.neg_addr = 0;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    GETLONG(l, p);
	    ciaddr2 = htonl(l);
	    if (ciaddr2 && go->accept_remote)
		try.hisaddr = ciaddr2;
	    no.old_addrs = 1;
	    break;
	case CI_ADDR:
	    if (go->neg_addr || no.neg_addr || cilen != CILEN_ADDR)
		goto bad;
	    try.old_addrs = 0;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    if (try.ouraddr != 0)
		try.neg_addr = 1;
	    no.neg_addr = 1;
	    break;
	case CI_MS_DNS1:
	    if (go->req_dns1 || no.req_dns1 || cilen != CILEN_ADDR)
		goto bad;
	    GETLONG(l, p);
	    try.dnsaddr[0] = htonl(l);
	    try.req_dns1 = 1;
	    no.req_dns1 = 1;
	    break;
	case CI_MS_DNS2:
	    if (go->req_dns2 || no.req_dns2 || cilen != CILEN_ADDR)
		goto bad;
	    GETLONG(l, p);
	    try.dnsaddr[1] = htonl(l);
	    try.req_dns2 = 1;
	    no.req_dns2 = 1;
	    break;
	case CI_MS_WINS1:
	    if (go->req_wins1 || no.req_wins1 || cilen != CILEN_ADDR)
		goto bad;
	    GETLONG(l, p);
	    try.winsaddr[0] = htonl(l);
	    try.req_wins1 = 1;
	    no.req_wins1 = 1;
	    break;
	case CI_MS_WINS2:
	    if (go->req_wins2 || no.req_wins2 || cilen != CILEN_ADDR)
		goto bad;
	    GETLONG(l, p);
	    try.winsaddr[1] = htonl(l);
	    try.req_wins2 = 1;
	    no.req_wins2 = 1;
	    break;
	}
	p = next;
    }

    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any remaining options, we ignore them.
     */
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    IPCPDEBUG(("ipcp_nakci: received bad Nak!"));
    return 0;
}


/*
 * ipcp_rejci - Reject some of our CIs.
 * Callback from fsm_rconfnakrej.
 */
static int
ipcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, ciflag, cilen;
    u_short cishort;
    u_int32_t cilong;
    ipcp_options try;		/* options to request next time */

    try = *go;
    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIADDRS(opt, neg, val1, val2) \
    if ((neg) && \
	(cilen = p[1]) == CILEN_ADDRS && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val1) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val2) \
	    goto bad; \
	try.old_addrs = 0; \
    }

#define REJCIVJ(opt, neg, val, old, maxslot, cflag) \
    if (go->neg && \
	p[1] == (old? CILEN_COMPRESS : CILEN_VJ) && \
	len >= p[1] && \
	p[0] == opt) { \
	len -= p[1]; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */  \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	   GETCHAR(cimaxslotindex, p); \
	   if (cimaxslotindex != maxslot) \
	     goto bad; \
	   GETCHAR(ciflag, p); \
	   if (ciflag != cflag) \
	     goto bad; \
        } \
	try.neg = 0; \
     }

#define REJCIADDR(opt, neg, val) \
    if (go->neg && \
	(cilen = p[1]) == CILEN_ADDR && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIDNS(opt, neg, dnsaddr) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != dnsaddr) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIWINS(opt, neg, addr) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != addr) \
	    goto bad; \
	try.neg = 0; \
    }

    REJCIADDRS(CI_ADDRS, !go->neg_addr && go->old_addrs,
	       go->ouraddr, go->hisaddr);

    REJCIVJ(CI_COMPRESSTYPE, neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    REJCIADDR(CI_ADDR, neg_addr, go->ouraddr);

    REJCIDNS(CI_MS_DNS1, req_dns1, go->dnsaddr[0]);

    REJCIDNS(CI_MS_DNS2, req_dns2, go->dnsaddr[1]);

    REJCIWINS(CI_MS_WINS1, req_wins1, go->winsaddr[0]);

    REJCIWINS(CI_MS_WINS2, req_wins2, go->winsaddr[1]);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    /*
     * Now we can update state.
     */
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    IPCPDEBUG(("ipcp_rejci: received bad Reject!"));
    return 0;
}


/*
 * ipcp_reqci - Check the peer's requested CIs and send appropriate response.
 * Callback from fsm_rconfreq, Receive Configure Request
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
ipcp_reqci(f, inp, len, reject_if_disagree)
    fsm *f;
    u_char *inp;		/* Requested CIs */
    int *len;			/* Length of requested CIs */
    int reject_if_disagree;
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    ipcp_options *ao = &ipcp_allowoptions[f->unit];
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_int32_t tl, ciaddr1, ciaddr2;/* Parsed address values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */
    u_char maxslotindex, cflag;
    int d;

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
	    IPCPDEBUG(("ipcp_reqci: bad CI length!"));
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
	case CI_ADDRS:
	    if (!ao->old_addrs || ho->neg_addr ||
		cilen != CILEN_ADDRS) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);		/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof(u_int32_t), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
		/*
		 * If neither we nor he knows his address, reject the option.
		 */
		orc = CONFREJ;
		wo->req_addr = 0;	/* don't NAK with 0.0.0.0 later */
		break;
	    }

	    /*
	     * If he doesn't know our address, or if we both have our address
	     * but disagree about it, then NAK it with our idea.
	     */
	    GETLONG(tl, p);		/* Parse desination address (ours) */
	    ciaddr2 = htonl(tl);
	    if (ciaddr2 != wo->ouraddr) {
		if (ciaddr2 == 0 || !wo->accept_local) {
		    orc = CONFNAK;
		    if (!reject_if_disagree) {
			DECPTR(sizeof(u_int32_t), p);
			tl = ntohl(wo->ouraddr);
			PUTLONG(tl, p);
		    }
		} else {
		    wo->ouraddr = ciaddr2;	/* accept peer's idea */
		}
	    }

	    ho->old_addrs = 1;
	    ho->hisaddr = ciaddr1;
	    ho->ouraddr = ciaddr2;
	    break;

	case CI_ADDR:
	    if (!ao->neg_addr || ho->old_addrs ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);	/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof(u_int32_t), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
		/*
		 * Don't ACK an address of 0.0.0.0 - reject it instead.
		 */
		orc = CONFREJ;
		wo->req_addr = 0;	/* don't NAK with 0.0.0.0 later */
		break;
	    }
	
	    ho->neg_addr = 1;
	    ho->hisaddr = ciaddr1;
	    break;

	case CI_MS_DNS1:
	case CI_MS_DNS2:
	    /* Microsoft primary or secondary DNS request */
	    d = citype == CI_MS_DNS2;

	    /* If we do not have a DNS address then we cannot send it */
	    if (ao->dnsaddr[d] == 0 ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (htonl(tl) != ao->dnsaddr[d]) {
                DECPTR(sizeof(u_int32_t), p);
		tl = ntohl(ao->dnsaddr[d]);
		PUTLONG(tl, p);
		orc = CONFNAK;
            }
            break;

	case CI_MS_WINS1:
	case CI_MS_WINS2:
	    /* Microsoft primary or secondary WINS request */
	    d = citype == CI_MS_WINS2;

	    /* If we do not have a WINS address then we cannot send it */
	    if (ao->winsaddr[d] == 0 ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (htonl(tl) != ao->winsaddr[d]) {
                DECPTR(sizeof(u_int32_t), p);
		tl = ntohl(ao->winsaddr[d]);
		PUTLONG(tl, p);
		orc = CONFNAK;
            }
            break;
	
	case CI_COMPRESSTYPE:
	    if (!ao->neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS)) {
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);

	    if (!(cishort == IPCP_VJ_COMP ||
		  (cishort == IPCP_VJ_COMP_OLD && cilen == CILEN_COMPRESS))) {
		orc = CONFREJ;
		break;
	    }

	    ho->neg_vj = 1;
	    ho->vj_protocol = cishort;
	    if (cilen == CILEN_VJ) {
		GETCHAR(maxslotindex, p);
		if (maxslotindex > ao->maxslotindex) { 
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(ao->maxslotindex, p);
		    }
		}
		GETCHAR(cflag, p);
		if (cflag && !ao->cflag) {
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(wo->cflag, p);
		    }
		}
		ho->maxslotindex = maxslotindex;
		ho->cflag = cflag;
	    } else {
		ho->old_vj = 1;
		ho->maxslotindex = MAX_STATES - 1;
		ho->cflag = 1;
	    }
	    break;

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
	    else {
		if (rc == CONFREJ)	/* Rejecting prior CI? */
		    continue;		/* Don't send this one */
		if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		    rc = CONFNAK;	/* Not anymore... */
		    ucp = inp;		/* Backup */
		}
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
     * send a NAK with a CI_ADDR option appended.  We assume the
     * input buffer is long enough that we can append the extra
     * option safely.
     */
    if (rc != CONFREJ && !ho->neg_addr && !ho->old_addrs &&
	wo->req_addr && !reject_if_disagree && !noremoteip) {
	if (rc == CONFACK) {
	    rc = CONFNAK;
	    ucp = inp;			/* reset pointer */
	    wo->req_addr = 0;		/* don't ask again */
	}
	PUTCHAR(CI_ADDR, ucp);
	PUTCHAR(CILEN_ADDR, ucp);
	tl = ntohl(wo->hisaddr);
	PUTLONG(tl, ucp);
    }

    *len = ucp - inp;			/* Compute output length */
    IPCPDEBUG(("ipcp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}


/*
 * ip_check_options - check that any IP-related options are OK,
 * and assign appropriate defaults.
 */
static void
ip_check_options()
{
    struct hostent *hp;
    u_int32_t local;
    ipcp_options *wo = &ipcp_wantoptions[0];

    /*
     * Default our local IP address based on our hostname.
     * If local IP address already given, don't bother.
     */
    if (wo->ouraddr == 0 && !disable_defaultip) {
	/*
	 * Look up our hostname (possibly with domain name appended)
	 * and take the first IP address as our local IP address.
	 * If there isn't an IP address for our hostname, too bad.
	 */
	wo->accept_local = 1;	/* don't insist on this default value */
	if ((hp = gethostbyname(hostname)) != NULL) {
	    local = *(u_int32_t *)hp->h_addr;
	    if (local != 0 && !bad_ip_adrs(local))
		wo->ouraddr = local;
	}
    }
    ask_for_local = wo->ouraddr != 0 || !disable_defaultip;
}


/*
 * ip_demand_conf - configure the interface as though
 * IPCP were up, for use with dial-on-demand.
 */
static int
ip_demand_conf(u)
    int u;
{
    ipcp_options *wo = &ipcp_wantoptions[u];

    if (wo->hisaddr == 0 && !noremoteip) {
	/* make up an arbitrary address for the peer */
	wo->hisaddr = htonl(0x0a707070 + ifunit);
	wo->accept_remote = 1;
    }
    if (wo->ouraddr == 0) {
	/* make up an arbitrary address for us */
	wo->ouraddr = htonl(0x0a404040 + ifunit);
	wo->accept_local = 1;
	ask_for_local = 0;	/* don't tell the peer this address */
    }
    if (!sifaddr(u, wo->ouraddr, wo->hisaddr, GetMask(wo->ouraddr)))
	return 0;
    ipcp_script(path_ippreup, 1);
    if (!sifup(u))
	return 0;
    if (!sifnpmode(u, PPP_IP, NPMODE_QUEUE))
	return 0;
    if (wo->default_route)
	if (sifdefaultroute(u, wo->ouraddr, wo->hisaddr))
	    default_route_set[u] = 1;
    if (wo->proxy_arp)
	if (sifproxyarp(u, wo->hisaddr))
	    proxy_arp_set[u] = 1;

    notice("local  IP address %I", wo->ouraddr);
    if (wo->hisaddr)
	notice("remote IP address %I", wo->hisaddr);

    return 1;
}


/*
 * ipcp_up - IPCP has come UP.
 *
 * Configure the IP network interface appropriately and bring it up.
 */
static void
ipcp_up(f)
    fsm *f;
{
    u_int32_t mask;
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    ipcp_options *wo = &ipcp_wantoptions[f->unit];

    IPCPDEBUG(("ipcp: up"));

    /*
     * We must have a non-zero IP address for both ends of the link.
     */
    if (!ho->neg_addr && !ho->old_addrs)
	ho->hisaddr = wo->hisaddr;
    else if (ho->neg_addr && bad_ip_adrs(ho->hisaddr))
	ho->hisaddr = wo->hisaddr;

    if (!(go->neg_addr || go->old_addrs) && (wo->neg_addr || wo->old_addrs)
	&& wo->ouraddr != 0) {
	error("Peer refused to agree to our IP address");
	ipcp_close(f->unit, "Refused our IP address");
	return;
    }
    if (go->ouraddr == 0) {
	error("Could not determine local IP address");
	ipcp_close(f->unit, "Could not determine local IP address");
	return;
    }
    if (ho->hisaddr == 0 && !noremoteip) {
	ho->hisaddr = htonl(0x0a404040 + ifunit);
	warn("Could not determine remote IP address: defaulting to %I",
	     ho->hisaddr);
    }
    script_setenv("IPLOCAL", ip_ntoa(go->ouraddr), 0);
    if (ho->hisaddr != 0)
	script_setenv("IPREMOTE", ip_ntoa(ho->hisaddr), 1);

    if (!go->req_dns1)
	    go->dnsaddr[0] = 0;
    if (!go->req_dns2)
	    go->dnsaddr[1] = 0;
    if (go->dnsaddr[0])
	script_setenv("DNS1", ip_ntoa(go->dnsaddr[0]), 0);
    if (go->dnsaddr[1])
	script_setenv("DNS2", ip_ntoa(go->dnsaddr[1]), 0);
    if (usepeerdns && (go->dnsaddr[0] || go->dnsaddr[1])) {
	script_setenv("USEPEERDNS", "1", 0);
	create_resolv(go->dnsaddr[0], go->dnsaddr[1]);
    }
    if (go->winsaddr[0])
	script_setenv("WINS1", ip_ntoa(go->winsaddr[0]), 0);
    if (go->winsaddr[1])
	script_setenv("WINS2", ip_ntoa(go->winsaddr[1]), 0);
    if (usepeerwins && (go->winsaddr[0] || go->winsaddr[1]))
	script_setenv("USEPEERWINS", "1", 0);

    /*
     * Check that the peer is allowed to use the IP address it wants.
     */
    if (ho->hisaddr != 0 && !auth_ip_addr(f->unit, ho->hisaddr)) {
	error("Peer is not authorized to use remote address %I", ho->hisaddr);
	ipcp_close(f->unit, "Unauthorized remote IP address");
	return;
    }

    /* set tcp compression */
    sifvjcomp(f->unit, ho->neg_vj, ho->cflag, ho->maxslotindex);

    /*
     * If we are doing dial-on-demand, the interface is already
     * configured, so we put out any saved-up packets, then set the
     * interface to pass IP packets.
     */
    if (demand) {
	if (go->ouraddr != wo->ouraddr || ho->hisaddr != wo->hisaddr) {
	    ipcp_clear_addrs(f->unit, wo->ouraddr, wo->hisaddr);
	    if (go->ouraddr != wo->ouraddr) {
		warn("Local IP address changed to %I", go->ouraddr);
		script_setenv("OLDIPLOCAL", ip_ntoa(wo->ouraddr), 0);
		wo->ouraddr = go->ouraddr;
	    } else
		script_unsetenv("OLDIPLOCAL");
	    if (ho->hisaddr != wo->hisaddr && wo->hisaddr != 0) {
		warn("Remote IP address changed to %I", ho->hisaddr);
		script_setenv("OLDIPREMOTE", ip_ntoa(wo->hisaddr), 0);
		wo->hisaddr = ho->hisaddr;
	    } else
		script_unsetenv("OLDIPREMOTE");

	    /* Set the interface to the new addresses */
	    mask = GetMask(go->ouraddr);
	    if (!sifaddr(f->unit, go->ouraddr, ho->hisaddr, mask)) {
		if (debug)
		    warn("Interface configuration failed");
		ipcp_close(f->unit, "Interface configuration failed");
		return;
	    }

	    /* assign a default route through the interface if required */
	    if (ipcp_wantoptions[f->unit].default_route) 
		if (sifdefaultroute(f->unit, go->ouraddr, ho->hisaddr))
		    default_route_set[f->unit] = 1;

	    /* Make a proxy ARP entry if requested. */
	    if (ho->hisaddr != 0 && ipcp_wantoptions[f->unit].proxy_arp)
		if (sifproxyarp(f->unit, ho->hisaddr))
		    proxy_arp_set[f->unit] = 1;

	}
	demand_rexmit(PPP_IP,go->ouraddr);
	sifnpmode(f->unit, PPP_IP, NPMODE_PASS);

    } else {
	/*
	 * Set IP addresses and (if specified) netmask.
	 */
	mask = GetMask(go->ouraddr);

#if !(defined(SVR4) && (defined(SNI) || defined(__USLC__)))
	if (!sifaddr(f->unit, go->ouraddr, ho->hisaddr, mask)) {
	    if (debug)
		warn("Interface configuration failed");
	    ipcp_close(f->unit, "Interface configuration failed");
	    return;
	}
#endif

	/* run the pre-up script, if any, and wait for it to finish */
	ipcp_script(path_ippreup, 1);

	/* bring the interface up for IP */
	if (!sifup(f->unit)) {
	    if (debug)
		warn("Interface failed to come up");
	    ipcp_close(f->unit, "Interface configuration failed");
	    return;
	}

#if (defined(SVR4) && (defined(SNI) || defined(__USLC__)))
	if (!sifaddr(f->unit, go->ouraddr, ho->hisaddr, mask)) {
	    if (debug)
		warn("Interface configuration failed");
	    ipcp_close(f->unit, "Interface configuration failed");
	    return;
	}
#endif
	sifnpmode(f->unit, PPP_IP, NPMODE_PASS);

	/* assign a default route through the interface if required */
	if (ipcp_wantoptions[f->unit].default_route) 
	    if (sifdefaultroute(f->unit, go->ouraddr, ho->hisaddr))
		default_route_set[f->unit] = 1;

	/* Make a proxy ARP entry if requested. */
	if (ho->hisaddr != 0 && ipcp_wantoptions[f->unit].proxy_arp)
	    if (sifproxyarp(f->unit, ho->hisaddr))
		proxy_arp_set[f->unit] = 1;

	ipcp_wantoptions[0].ouraddr = go->ouraddr;

	notice("local  IP address %I", go->ouraddr);
	if (ho->hisaddr != 0)
	    notice("remote IP address %I", ho->hisaddr);
	if (go->dnsaddr[0])
	    notice("primary   DNS address %I", go->dnsaddr[0]);
	if (go->dnsaddr[1])
	    notice("secondary DNS address %I", go->dnsaddr[1]);
	if (go->winsaddr[0])
	    notice("primary   WINS address %I", go->winsaddr[0]);
	if (go->winsaddr[1])
	    notice("secondary WINS address %I", go->winsaddr[1]);
    }

    reset_link_stats(f->unit);

    np_up(f->unit, PPP_IP);
    ipcp_is_up = 1;

    notify(ip_up_notifier, 0);
    if (ip_up_hook)
	ip_up_hook();

    /*
     * Execute the ip-up script, like this:
     *	/etc/ppp/ip-up interface tty speed local-IP remote-IP
     */
    if (ipcp_script_state == s_down && ipcp_script_pid == 0) {
	ipcp_script_state = s_up;
	ipcp_script(path_ipup, 0);
    }
}


/*
 * ipcp_down - IPCP has gone DOWN.
 *
 * Take the IP network interface down, clear its addresses
 * and delete routes through it.
 */
static void
ipcp_down(f)
    fsm *f;
{
    IPCPDEBUG(("ipcp: down"));
    /* XXX a bit IPv4-centric here, we only need to get the stats
     * before the interface is marked down. */
    /* XXX more correct: we must get the stats before running the notifiers,
     * at least for the radius plugin */
    update_link_stats(f->unit);
    notify(ip_down_notifier, 0);
    if (ip_down_hook)
	ip_down_hook();
    if (ipcp_is_up) {
	ipcp_is_up = 0;
	np_down(f->unit, PPP_IP);
    }
    sifvjcomp(f->unit, 0, 0, 0);

    print_link_stats(); /* _after_ running the notifiers and ip_down_hook(),
			 * because print_link_stats() sets link_stats_valid
			 * to 0 (zero) */

    /*
     * If we are doing dial-on-demand, set the interface
     * to queue up outgoing packets (for now).
     */
    if (demand) {
	sifnpmode(f->unit, PPP_IP, NPMODE_QUEUE);
    } else {
	sifnpmode(f->unit, PPP_IP, NPMODE_DROP);
	sifdown(f->unit);
	ipcp_clear_addrs(f->unit, ipcp_gotoptions[f->unit].ouraddr,
			 ipcp_hisoptions[f->unit].hisaddr);
    }

    /* Execute the ip-down script */
    if (ipcp_script_state == s_up && ipcp_script_pid == 0) {
	ipcp_script_state = s_down;
	ipcp_script(path_ipdown, 0);
    }
}


/*
 * ipcp_clear_addrs() - clear the interface addresses, routes,
 * proxy arp entries, etc.
 */
static void
ipcp_clear_addrs(unit, ouraddr, hisaddr)
    int unit;
    u_int32_t ouraddr;  /* local address */
    u_int32_t hisaddr;  /* remote address */
{
    if (proxy_arp_set[unit]) {
	cifproxyarp(unit, hisaddr);
	proxy_arp_set[unit] = 0;
    }
    if (default_route_set[unit]) {
	cifdefaultroute(unit, ouraddr, hisaddr);
	default_route_set[unit] = 0;
    }
    cifaddr(unit, ouraddr, hisaddr);
}


/*
 * ipcp_finished - possibly shut down the lower layers.
 */
static void
ipcp_finished(f)
    fsm *f;
{
	if (ipcp_is_open) {
		ipcp_is_open = 0;
		np_finished(f->unit, PPP_IP);
	}
}


/*
 * ipcp_script_done - called when the ip-up or ip-down script
 * has finished.
 */
static void
ipcp_script_done(arg)
    void *arg;
{
    ipcp_script_pid = 0;
    switch (ipcp_script_state) {
    case s_up:
	if (ipcp_fsm[0].state != OPENED) {
	    ipcp_script_state = s_down;
	    ipcp_script(path_ipdown, 0);
	}
	break;
    case s_down:
	if (ipcp_fsm[0].state == OPENED) {
	    ipcp_script_state = s_up;
	    ipcp_script(path_ipup, 0);
	}
	break;
    }
}


/*
 * ipcp_script - Execute a script with arguments
 * interface-name tty-name speed local-IP remote-IP.
 */
static void
ipcp_script(script, wait)
    char *script;
    int wait;
{
    char strspeed[32], strlocal[32], strremote[32];
    char *argv[8];

    slprintf(strspeed, sizeof(strspeed), "%d", baud_rate);
    slprintf(strlocal, sizeof(strlocal), "%I", ipcp_gotoptions[0].ouraddr);
    slprintf(strremote, sizeof(strremote), "%I", ipcp_hisoptions[0].hisaddr);

    argv[0] = script;
    argv[1] = ifname;
    argv[2] = devnam;
    argv[3] = strspeed;
    argv[4] = strlocal;
    argv[5] = strremote;
    argv[6] = ipparam;
    argv[7] = NULL;
    if (wait)
	run_program(script, argv, 0, NULL, NULL, 1);
    else
	ipcp_script_pid = run_program(script, argv, 0, ipcp_script_done,
				      NULL, 0);
}

/*
 * create_resolv - create the replacement resolv.conf file
 */
static void
create_resolv(peerdns1, peerdns2)
    u_int32_t peerdns1, peerdns2;
{
    FILE *f;

    f = fopen(_PATH_RESOLV, "w");
    if (f == NULL) {
	error("Failed to create %s: %m", _PATH_RESOLV);
	return;
    }

    if (peerdns1)
	fprintf(f, "nameserver %s\n", ip_ntoa(peerdns1));

    if (peerdns2)
	fprintf(f, "nameserver %s\n", ip_ntoa(peerdns2));

    if (ferror(f))
	error("Write failed to %s: %m", _PATH_RESOLV);

    fclose(f);
}

/*
 * ipcp_printpkt - print the contents of an IPCP packet.
 */
static char *ipcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
ipcp_printpkt(p, plen, printer, arg)
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

    if (code >= 1 && code <= sizeof(ipcp_codenames) / sizeof(char *))
	printer(arg, " %s", ipcp_codenames[code-1]);
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
	    if (olen < 2 || olen > len) {
		break;
	    }
	    printer(arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
	    case CI_ADDRS:
		if (olen == CILEN_ADDRS) {
		    p += 2;
		    GETLONG(cilong, p);
		    printer(arg, "addrs %I", htonl(cilong));
		    GETLONG(cilong, p);
		    printer(arg, " %I", htonl(cilong));
		}
		break;
	    case CI_COMPRESSTYPE:
		if (olen >= CILEN_COMPRESS) {
		    p += 2;
		    GETSHORT(cishort, p);
		    printer(arg, "compress ");
		    switch (cishort) {
		    case IPCP_VJ_COMP:
			printer(arg, "VJ");
			break;
		    case IPCP_VJ_COMP_OLD:
			printer(arg, "old-VJ");
			break;
		    default:
			printer(arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_ADDR:
		if (olen == CILEN_ADDR) {
		    p += 2;
		    GETLONG(cilong, p);
		    printer(arg, "addr %I", htonl(cilong));
		}
		break;
	    case CI_MS_DNS1:
	    case CI_MS_DNS2:
	        p += 2;
		GETLONG(cilong, p);
		printer(arg, "ms-dns%d %I", (code == CI_MS_DNS1? 1: 2),
			htonl(cilong));
		break;
	    case CI_MS_WINS1:
	    case CI_MS_WINS2:
	        p += 2;
		GETLONG(cilong, p);
		printer(arg, "ms-wins%d %I", (code == CI_MS_WINS1? 1: 2),
			htonl(cilong));
		break;
	    }
	    while (p < optend) {
		GETCHAR(code, p);
		printer(arg, " %.2x", code);
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
	printer(arg, " %.2x", code);
    }

    return p - pstart;
}

/*
 * ip_active_pkt - see if this IP packet is worth bringing the link up for.
 * We don't bring the link up for IP fragments or for TCP FIN packets
 * with no data.
 */
#define IP_HDRLEN	20	/* bytes */
#define IP_OFFMASK	0x1fff
#ifndef IPPROTO_TCP
#define IPPROTO_TCP	6
#endif
#define TCP_HDRLEN	20
#define TH_FIN		0x01

/*
 * We use these macros because the IP header may be at an odd address,
 * and some compilers might use word loads to get th_off or ip_hl.
 */

#define net_short(x)	(((x)[0] << 8) + (x)[1])
#define get_iphl(x)	(((unsigned char *)(x))[0] & 0xF)
#define get_ipoff(x)	net_short((unsigned char *)(x) + 6)
#define get_ipproto(x)	(((unsigned char *)(x))[9])
#define get_tcpoff(x)	(((unsigned char *)(x))[12] >> 4)
#define get_tcpflags(x)	(((unsigned char *)(x))[13])

static int
ip_active_pkt(pkt, len)
    u_char *pkt;
    int len;
{
    u_char *tcp;
    int hlen;

    len -= PPP_HDRLEN;
    pkt += PPP_HDRLEN;
    if (len < IP_HDRLEN)
	return 0;
    if ((get_ipoff(pkt) & IP_OFFMASK) != 0)
	return 0;
    if (get_ipproto(pkt) != IPPROTO_TCP)
	return 1;
    hlen = get_iphl(pkt) * 4;
    if (len < hlen + TCP_HDRLEN)
	return 0;
    tcp = pkt + hlen;
    if ((get_tcpflags(tcp) & TH_FIN) != 0 && len == hlen + get_tcpoff(tcp) * 4)
	return 0;
    return 1;
}
