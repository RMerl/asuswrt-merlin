/* pppol2tp.c - pppd plugin to implement PPPoL2TP protocol
 *   for Linux using kernel pppol2tp support.
 *
 * Requires kernel pppol2tp driver which is integrated into the kernel
 * from 2.6.23 onwards. For earlier kernels, a version can be obtained
 * from the OpenL2TP project at
 * http://www.sourceforge.net/projects/openl2tp/
 *
 * Original by Martijn van Oosterhout <kleptog@svana.org>
 * Modified by jchapman@katalix.com
 *
 * Heavily based upon pppoatm.c: original notice follows
 *
 * Copyright 2000 Mitchell Blank Jr.
 * Based in part on work from Jens Axboe and Paul Mackerras.
 * Updated to ppp-2.4.1 by Bernhard Kaindl
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "pppd.h"
#include "pathnames.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "ipcp.h"
#include <sys/stat.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <linux/version.h>
#include <linux/sockios.h>
#ifndef aligned_u64
/* should be defined in sys/types.h */
#define aligned_u64 unsigned long long __attribute__((aligned(8)))
#endif
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/if_pppox.h>
#include <linux/if_pppol2tp.h>

/* should be added to system's socket.h... */
#ifndef SOL_PPPOL2TP
#define SOL_PPPOL2TP	273
#endif

const char pppd_version[] = VERSION;

static int setdevname_pppol2tp(char **argv);

static int pppol2tp_fd = -1;
static char *pppol2tp_fd_str;
static bool pppol2tp_lns_mode = 0;
static bool pppol2tp_recv_seq = 0;
static bool pppol2tp_send_seq = 0;
static int pppol2tp_debug_mask = 0;
static int pppol2tp_reorder_timeout = 0;
static char pppol2tp_ifname[32] = { 0, };
int pppol2tp_tunnel_id = 0;
int pppol2tp_session_id = 0;

static int device_got_set = 0;
struct channel pppol2tp_channel;

static void (*old_snoop_recv_hook)(unsigned char *p, int len) = NULL;
static void (*old_snoop_send_hook)(unsigned char *p, int len) = NULL;

/* Hook provided to allow other plugins to handle ACCM changes */
void (*pppol2tp_send_accm_hook)(int tunnel_id, int session_id,
				uint32_t send_accm, uint32_t recv_accm) = NULL;

/* Hook provided to allow other plugins to handle IP up/down */
void (*pppol2tp_ip_updown_hook)(int tunnel_id, int session_id, int up) = NULL;

static option_t pppol2tp_options[] = {
	{ "pppol2tp", o_special, &setdevname_pppol2tp,
	  "FD for PPPoL2TP socket", OPT_DEVNAM | OPT_A2STRVAL,
          &pppol2tp_fd_str },
	{ "pppol2tp_lns_mode", o_bool, &pppol2tp_lns_mode,
	  "PPPoL2TP LNS behavior. Default off.",
	  OPT_PRIO | OPRIO_CFGFILE },
	{ "pppol2tp_send_seq", o_bool, &pppol2tp_send_seq,
	  "PPPoL2TP enable sequence numbers in transmitted data packets. "
	  "Default off.",
	  OPT_PRIO | OPRIO_CFGFILE },
	{ "pppol2tp_recv_seq", o_bool, &pppol2tp_recv_seq,
	  "PPPoL2TP enforce sequence numbers in received data packets. "
	  "Default off.",
	  OPT_PRIO | OPRIO_CFGFILE },
	{ "pppol2tp_reorderto", o_int, &pppol2tp_reorder_timeout,
	  "PPPoL2TP data packet reorder timeout. Default 0 (no reordering).",
	  OPT_PRIO },
	{ "pppol2tp_debug_mask", o_int, &pppol2tp_debug_mask,
	  "PPPoL2TP debug mask. Default: no debug.",
	  OPT_PRIO },
	{ "pppol2tp_ifname", o_string, &pppol2tp_ifname,
	  "Set interface name of PPP interface",
	  OPT_PRIO | OPT_PRIV | OPT_STATIC, NULL, 16 },
	{ "pppol2tp_tunnel_id", o_int, &pppol2tp_tunnel_id,
	  "PPPoL2TP tunnel_id.",
	  OPT_PRIO },
	{ "pppol2tp_session_id", o_int, &pppol2tp_session_id,
	  "PPPoL2TP session_id.",
	  OPT_PRIO },
	{ NULL }
};

static int setdevname_pppol2tp(char **argv)
{
	struct sockaddr_pppol2tp sax;
	int len = sizeof(sax);

	if (device_got_set)
		return 0;

	if (!int_option(*argv, &pppol2tp_fd))
		return 0;

	if(getsockname(pppol2tp_fd, (struct sockaddr *)&sax, &len) < 0) {
		fatal("Given FD for PPPoL2TP socket invalid (%s)",
		      strerror(errno));
	}
	if(sax.sa_family != AF_PPPOX || sax.sa_protocol != PX_PROTO_OL2TP) {
		fatal("Socket is not a PPPoL2TP socket");
	}

	/* Do a test getsockopt() to ensure that the kernel has the necessary
	 * feature available.
	 * driver returns -ENOTCONN until session established!
	if (getsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_DEBUG,
		       &tmp, &tmp_len) < 0) {
		fatal("PPPoL2TP kernel driver not installed");
	} */

	/* Setup option defaults. Compression options are disabled! */

	modem = 0;

	lcp_allowoptions[0].neg_accompression = 1;
	lcp_wantoptions[0].neg_accompression = 0;

	lcp_allowoptions[0].neg_pcompression = 1;
	lcp_wantoptions[0].neg_pcompression = 0;

	ccp_allowoptions[0].deflate = 0;
	ccp_wantoptions[0].deflate = 0;

	ipcp_allowoptions[0].neg_vj = 0;
	ipcp_wantoptions[0].neg_vj = 0;

	ccp_allowoptions[0].bsd_compress = 0;
	ccp_wantoptions[0].bsd_compress = 0;

	the_channel = &pppol2tp_channel;
	device_got_set = 1;

	return 1;
}

static int connect_pppol2tp(void)
{
	struct sockaddr_pppol2tp sax;
	int len = sizeof(sax);

	if(pppol2tp_fd == -1) {
		fatal("No PPPoL2TP FD specified");
	}

	getsockname(pppol2tp_fd, (struct sockaddr *)&sax, &len);
	sprintf(ppp_devnam, "l2tp (%s)", inet_ntoa(sax.pppol2tp.addr.sin_addr));

	return pppol2tp_fd;
}

static void disconnect_pppol2tp(void)
{
	if (pppol2tp_fd >= 0) {
		close(pppol2tp_fd);
		pppol2tp_fd = -1;
	}
}

static void send_config_pppol2tp(int mtu,
			      u_int32_t asyncmap,
			      int pcomp,
			      int accomp)
{
	struct ifreq ifr;
	int on = 1;
	int fd;
	char reorderto[16];
	char tid[8];
	char sid[8];

	if (pppol2tp_ifname[0]) {
		struct ifreq ifr;
		int fd;

		fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd >= 0) {
			memset (&ifr, '\0', sizeof (ifr));
			strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
			strlcpy(ifr.ifr_newname, pppol2tp_ifname,
				sizeof(ifr.ifr_name));
			ioctl(fd, SIOCSIFNAME, (caddr_t) &ifr);
			strlcpy(ifname, pppol2tp_ifname, 32);
			if (pppol2tp_debug_mask & PPPOL2TP_MSG_CONTROL) {
				dbglog("ppp%d: interface name %s",
				       ifunit, ifname);
			}
		}
		close(fd);
	}

	if ((lcp_allowoptions[0].mru > 0) && (mtu > lcp_allowoptions[0].mru)) {
		warn("Overriding mtu %d to %d", mtu, lcp_allowoptions[0].mru);
		mtu = lcp_allowoptions[0].mru;
	}
	netif_set_mtu(ifunit, mtu);

	reorderto[0] = '\0';
	if (pppol2tp_reorder_timeout > 0)
		sprintf(&reorderto[0], "%d ", pppol2tp_reorder_timeout);
	tid[0] = '\0';
	if (pppol2tp_tunnel_id > 0)
		sprintf(&tid[0], "%hu ", pppol2tp_tunnel_id);
	sid[0] = '\0';
	if (pppol2tp_session_id > 0)
		sprintf(&sid[0], "%hu ", pppol2tp_session_id);

	dbglog("PPPoL2TP options: %s%s%s%s%s%s%s%s%sdebugmask %d",
	       pppol2tp_recv_seq ? "recvseq " : "",
	       pppol2tp_send_seq ? "sendseq " : "",
	       pppol2tp_lns_mode ? "lnsmode " : "",
	       pppol2tp_reorder_timeout ? "reorderto " : "", reorderto,
	       pppol2tp_tunnel_id ? "tid " : "", tid,
	       pppol2tp_session_id ? "sid " : "", sid,
	       pppol2tp_debug_mask);

	if (pppol2tp_recv_seq)
		if (setsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_RECVSEQ,
			       &on, sizeof(on)) < 0)
			fatal("setsockopt(PPPOL2TP_RECVSEQ): %m");
	if (pppol2tp_send_seq)
		if (setsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_SENDSEQ,
			       &on, sizeof(on)) < 0)
			fatal("setsockopt(PPPOL2TP_SENDSEQ): %m");
	if (pppol2tp_lns_mode)
		if (setsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_LNSMODE,
			       &on, sizeof(on)) < 0)
			fatal("setsockopt(PPPOL2TP_LNSMODE): %m");
	if (pppol2tp_reorder_timeout)
		if (setsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_REORDERTO,
			       &pppol2tp_reorder_timeout,
			       sizeof(pppol2tp_reorder_timeout)) < 0)
			fatal("setsockopt(PPPOL2TP_REORDERTO): %m");
	if (pppol2tp_debug_mask)
		if (setsockopt(pppol2tp_fd, SOL_PPPOL2TP, PPPOL2TP_SO_DEBUG,
			       &pppol2tp_debug_mask, sizeof(pppol2tp_debug_mask)) < 0)
			fatal("setsockopt(PPPOL2TP_DEBUG): %m");
}

static void recv_config_pppol2tp(int mru,
			      u_int32_t asyncmap,
			      int pcomp,
			      int accomp)
{
	if ((lcp_allowoptions[0].mru > 0) && (mru > lcp_allowoptions[0].mru)) {
		warn("Overriding mru %d to mtu value %d", mru,
		     lcp_allowoptions[0].mru);
		mru = lcp_allowoptions[0].mru;
	}
	if ((ifunit >= 0) && ioctl(pppol2tp_fd, PPPIOCSMRU, (caddr_t) &mru) < 0)
		error("Couldn't set PPP MRU: %m");
}

/*****************************************************************************
 * Snoop LCP message exchanges to capture negotiated ACCM values.
 * When asyncmap values have been seen from both sides, give the values to
 * L2TP.
 * This code is derived from Roaring Penguin L2TP.
 *****************************************************************************/

static void pppol2tp_lcp_snoop(unsigned char *buf, int len, int incoming)
{
	static bool got_send_accm = 0;
	static bool got_recv_accm = 0;
	static uint32_t recv_accm = 0xffffffff;
	static uint32_t send_accm = 0xffffffff;
	static bool snooping = 1;

	uint16_t protocol;
	uint16_t lcp_pkt_len;
	int opt, opt_len;
	int reject;
	unsigned char const *opt_data;
	uint32_t accm;

	/* Skip HDLC header */
	buf += 2;
	len -= 2;

	/* Unreasonably short frame?? */
	if (len <= 0) return;

	/* Get protocol */
	if (buf[0] & 0x01) {
		/* Compressed protcol field */
		protocol = buf[0];
	} else {
		protocol = ((unsigned int) buf[0]) * 256 + buf[1];
	}

	/* If it's a network protocol, stop snooping */
	if (protocol <= 0x3fff) {
		if (pppol2tp_debug_mask & PPPOL2TP_MSG_DEBUG) {
			dbglog("Turning off snooping: "
			       "Network protocol %04x found.",
			       protocol);
		}
		snooping = 0;
		return;
	}

	/* If it's not LCP, do not snoop */
	if (protocol != 0xc021) {
		return;
	}

	/* Skip protocol; go to packet data */
	buf += 2;
	len -= 2;

	/* Unreasonably short frame?? */
	if (len <= 0) return;

	/* Look for Configure-Ack or Configure-Reject code */
	if (buf[0] != CONFACK && buf[0] != CONFREJ) return;

	reject = (buf[0] == CONFREJ);

	lcp_pkt_len = ((unsigned int) buf[2]) * 256 + buf[3];

	/* Something fishy with length field? */
	if (lcp_pkt_len > len) return;

	/* Skip to options */
	len = lcp_pkt_len - 4;
	buf += 4;

	while (len > 0) {
		/* Pull off an option */
		opt = buf[0];
		opt_len = buf[1];
		opt_data = &buf[2];
		if (opt_len > len || opt_len < 2) break;
		len -= opt_len;
		buf += opt_len;
		if (pppol2tp_debug_mask & PPPOL2TP_MSG_DEBUG) {
			dbglog("Found option type %02x; len %d", opt, opt_len);
		}

		/* We are specifically interested in ACCM */
		if (opt == CI_ASYNCMAP && opt_len == 0x06) {
			if (reject) {
				/* ACCM negotiation REJECTED; use default */
				accm = 0xffffffff;
				if (pppol2tp_debug_mask & PPPOL2TP_MSG_DATA) {
					dbglog("Rejected ACCM negotiation; "
					       "defaulting (%s)",
					       incoming ? "incoming" : "outgoing");
				}
				recv_accm = accm;
				send_accm = accm;
				got_recv_accm = 1;
				got_send_accm = 1;
			} else {
				memcpy(&accm, opt_data, sizeof(accm));
				if (pppol2tp_debug_mask & PPPOL2TP_MSG_DATA) {
					dbglog("Found ACCM of %08x (%s)", accm,
					       incoming ? "incoming" : "outgoing");
				}
				if (incoming) {
					recv_accm = accm;
					got_recv_accm = 1;
				} else {
					send_accm = accm;
					got_send_accm = 1;
				}
			}

			if (got_recv_accm && got_send_accm) {
				if (pppol2tp_debug_mask & PPPOL2TP_MSG_CONTROL) {
					dbglog("Telling L2TP: Send ACCM = %08x; "
					       "Receive ACCM = %08x", send_accm, recv_accm);
				}
				if (pppol2tp_send_accm_hook != NULL) {
					(*pppol2tp_send_accm_hook)(pppol2tp_tunnel_id,
								   pppol2tp_session_id,
								   send_accm, recv_accm);
				}
				got_recv_accm = 0;
				got_send_accm = 0;
			}
		}
	}
}

static void pppol2tp_lcp_snoop_recv(unsigned char *p, int len)
{
	if (old_snoop_recv_hook != NULL)
		(*old_snoop_recv_hook)(p, len);
	pppol2tp_lcp_snoop(p, len, 1);
}

static void pppol2tp_lcp_snoop_send(unsigned char *p, int len)
{
	if (old_snoop_send_hook != NULL)
		(*old_snoop_send_hook)(p, len);
	pppol2tp_lcp_snoop(p, len, 0);
}

/*****************************************************************************
 * Interface up/down events
 *****************************************************************************/

static void pppol2tp_ip_up(void *opaque, int arg)
{
	/* may get called twice (for IPv4 and IPv6) but the hook handles that well */
	if (pppol2tp_ip_updown_hook != NULL) {
		(*pppol2tp_ip_updown_hook)(pppol2tp_tunnel_id,
					   pppol2tp_session_id, 1);
	}
}

static void pppol2tp_ip_down(void *opaque, int arg)
{
	/* may get called twice (for IPv4 and IPv6) but the hook handles that well */
	if (pppol2tp_ip_updown_hook != NULL) {
		(*pppol2tp_ip_updown_hook)(pppol2tp_tunnel_id,
					   pppol2tp_session_id, 0);
	}
}

/*****************************************************************************
 * Application init
 *****************************************************************************/

static void pppol2tp_check_options(void)
{
	/* Enable LCP snooping for ACCM options only for LNS */
	if (pppol2tp_lns_mode) {
		if ((pppol2tp_tunnel_id == 0) || (pppol2tp_session_id == 0)) {
			fatal("tunnel_id/session_id values not specified");
		}
		if (pppol2tp_debug_mask & PPPOL2TP_MSG_CONTROL) {
			dbglog("Enabling LCP snooping");
		}
		old_snoop_recv_hook = snoop_recv_hook;
		old_snoop_send_hook = snoop_send_hook;

		snoop_recv_hook = pppol2tp_lcp_snoop_recv;
		snoop_send_hook = pppol2tp_lcp_snoop_send;
	}
}

/* Called just before pppd exits.
 */
static void pppol2tp_cleanup(void)
{
	if (pppol2tp_debug_mask & PPPOL2TP_MSG_DEBUG) {
		dbglog("pppol2tp: exiting.");
	}
	disconnect_pppol2tp();
}

void plugin_init(void)
{
#if defined(__linux__)
	extern int new_style_driver;	/* From sys-linux.c */
	if (!ppp_available() && !new_style_driver)
		fatal("Kernel doesn't support ppp_generic - "
		    "needed for PPPoL2TP");
#else
	fatal("No PPPoL2TP support on this OS");
#endif
	add_options(pppol2tp_options);

	/* Hook up ip up/down notifiers to send indicator to openl2tpd
	 * that the link is up
	 */
	add_notifier(&ip_up_notifier, pppol2tp_ip_up, NULL);
	add_notifier(&ip_down_notifier, pppol2tp_ip_down, NULL);
#ifdef INET6
	add_notifier(&ipv6_up_notifier, pppol2tp_ip_up, NULL);
	add_notifier(&ipv6_down_notifier, pppol2tp_ip_down, NULL);
#endif
}

struct channel pppol2tp_channel = {
    options: pppol2tp_options,
    process_extra_options: NULL,
    check_options: &pppol2tp_check_options,
    connect: &connect_pppol2tp,
    disconnect: &disconnect_pppol2tp,
    establish_ppp: &generic_establish_ppp,
    disestablish_ppp: &generic_disestablish_ppp,
    send_config: &send_config_pppol2tp,
    recv_config: &recv_config_pppol2tp,
    close: NULL,
    cleanup: NULL
};
