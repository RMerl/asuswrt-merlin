/*
 * Copyright (C) 1996, 1999 Olaf Kirch
 * Modified by Jeffrey A. Uphoff, 1997-1999.
 * Modified by H.J. Lu, 1998.
 * Modified by Lon Hohberger, Oct. 2000
 *   - Bugfix handling client responses.
 *   - Paranoia on NOTIFY_CALLBACK case
 *
 * NSM for Linux.
 */

/*
 * After reboot, notify all hosts on our notify list. In order not to
 * hang statd with delivery to dead hosts, we perform all RPC calls in
 * parallel.
 *
 * It would have been nice to use the portmapper's rmtcall feature,
 * but that's not possible for security reasons (the portmapper would
 * have to forward the call with root privs for most statd's, which
 * it won't if it's worth its money).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_rmt.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif /* HAVE_IFADDRS_H */
#include "sm_inter.h"
#include "statd.h"
#include "notlist.h"
#include "log.h"
#include "ha-callout.h"

#if SIZEOF_SOCKLEN_T - 0 == 0
#define socklen_t int
#endif

#define MAXMSGSIZE	(2048 / sizeof(unsigned int))

static unsigned long	xid = 0;	/* RPC XID counter */
static int		sockfd = -1;	/* notify socket */

/*
 * Initialize socket used to notify lockd of peer reboots.
 *
 * Returns the file descriptor of the new socket if successful;
 * otherwise returns -1 and logs an error.
 *
 * Lockd rejects such requests if the source port is not privileged.
 * statd_get_socket() must be invoked while statd still holds root
 * privileges in order for the socket to acquire a privileged source
 * port.
 */
int
statd_get_socket(void)
{
	struct sockaddr_in	sin;
	struct servent *se;
	int loopcnt = 100;

	if (sockfd >= 0)
		return sockfd;

	while (loopcnt-- > 0) {

		if (sockfd >= 0) close(sockfd);

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			note(N_CRIT, "%s: Can't create socket: %m", __func__);
			return -1;
		}


		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;

		if (bindresvport(sockfd, &sin) < 0) {
			dprintf(N_WARNING, "%s: can't bind to reserved port",
					__func__);
			break;
		}
		se = getservbyport(sin.sin_port, "udp");
		if (se == NULL)
			break;
		/* rather not use that port, try again */
	}
	FD_SET(sockfd, &SVC_FDSET);
	return sockfd;
}

static unsigned long
xmit_call(struct sockaddr_in *sin,
	  u_int32_t prog, u_int32_t vers, u_int32_t proc,
	  xdrproc_t func, void *obj)
/* 		__u32 prog, __u32 vers, __u32 proc, xdrproc_t func, void *obj) */
{
	unsigned int		msgbuf[MAXMSGSIZE], msglen;
	struct rpc_msg		mesg;
	struct pmap		pmap;
	XDR			xdr, *xdrs = &xdr;
	int			err;

	if (!xid)
		xid = getpid() + time(NULL);

	mesg.rm_xid = ++xid;
	mesg.rm_direction = CALL;
	mesg.rm_call.cb_rpcvers = 2;
	if (sin->sin_port == 0) {
		sin->sin_port = htons(PMAPPORT);
		mesg.rm_call.cb_prog = PMAPPROG;
		mesg.rm_call.cb_vers = PMAPVERS;
		mesg.rm_call.cb_proc = PMAPPROC_GETPORT;
		pmap.pm_prog = prog;
		pmap.pm_vers = vers;
		pmap.pm_prot = IPPROTO_UDP;
		pmap.pm_port = 0;
		func = (xdrproc_t) xdr_pmap;
		obj  = &pmap;
	} else {
		mesg.rm_call.cb_prog = prog;
		mesg.rm_call.cb_vers = vers;
		mesg.rm_call.cb_proc = proc;
	}
	mesg.rm_call.cb_cred.oa_flavor = AUTH_NULL;
	mesg.rm_call.cb_cred.oa_base = (caddr_t) NULL;
	mesg.rm_call.cb_cred.oa_length = 0;
	mesg.rm_call.cb_verf.oa_flavor = AUTH_NULL;
	mesg.rm_call.cb_verf.oa_base = (caddr_t) NULL;
	mesg.rm_call.cb_verf.oa_length = 0;

	/* Create XDR memory object for encoding */
	xdrmem_create(xdrs, (caddr_t) msgbuf, sizeof(msgbuf), XDR_ENCODE);

	/* Encode the RPC header part and payload */
	if (!xdr_callmsg(xdrs, &mesg) || !func(xdrs, obj)) {
		dprintf(N_WARNING, "%s: can't encode RPC message!", __func__);
		xdr_destroy(xdrs);
		return 0;
	}

	/* Get overall length of datagram */
	msglen = xdr_getpos(xdrs);

	if ((err = sendto(sockfd, msgbuf, msglen, 0,
			(struct sockaddr *) sin, sizeof(*sin))) < 0) {
		dprintf(N_WARNING, "%s: sendto failed: %m", __func__);
	} else if (err != msglen) {
		dprintf(N_WARNING, "%s: short write: %m", __func__);
	}

	xdr_destroy(xdrs);

	return err == msglen? xid : 0;
}

static notify_list *
recv_rply(struct sockaddr_in *sin, u_long *portp)
{
	unsigned int		msgbuf[MAXMSGSIZE], msglen;
	struct rpc_msg		mesg;
	notify_list		*lp = NULL;
	XDR			xdr, *xdrs = &xdr;
	socklen_t		alen = sizeof(*sin);

	/* Receive message */
	if ((msglen = recvfrom(sockfd, msgbuf, sizeof(msgbuf), 0,
			(struct sockaddr *) sin, &alen)) < 0) {
		dprintf(N_WARNING, "%s: recvfrom failed: %m", __func__);
		return NULL;
	}

	/* Create XDR object for decoding buffer */
	xdrmem_create(xdrs, (caddr_t) msgbuf, msglen, XDR_DECODE);

	memset(&mesg, 0, sizeof(mesg));
	mesg.rm_reply.rp_acpt.ar_results.where = NULL;
	mesg.rm_reply.rp_acpt.ar_results.proc = (xdrproc_t) xdr_void;

	if (!xdr_replymsg(xdrs, &mesg)) {
		note(N_WARNING, "%s: can't decode RPC message!", __func__);
		goto done;
	}

	if (mesg.rm_reply.rp_stat != 0) {
		note(N_WARNING, "%s: [%s] RPC status %d", 
				__func__,
				inet_ntoa(sin->sin_addr),
				mesg.rm_reply.rp_stat);
		goto done;
	}
	if (mesg.rm_reply.rp_acpt.ar_stat != 0) {
		note(N_WARNING, "%s: [%s] RPC status %d",
				__func__,
				inet_ntoa(sin->sin_addr),
				mesg.rm_reply.rp_acpt.ar_stat);
		goto done;
	}

	for (lp = notify; lp != NULL; lp = lp->next) {
		/* LH - this was a bug... it should have been checking
		 * the xid from the response message from the client,
		 * not the static, internal xid */
		if (lp->xid != mesg.rm_xid)
			continue;
		if (lp->addr.s_addr != sin->sin_addr.s_addr) {
			char addr [18];
			strncpy (addr, inet_ntoa(lp->addr),
				 sizeof (addr) - 1);
			addr [sizeof (addr) - 1] = '\0';
			dprintf(N_WARNING, "%s: address mismatch: "
				"expected %s, got %s", __func__,
				addr, inet_ntoa(sin->sin_addr));
		}
		if (lp->port == 0) {
			if (!xdr_u_long(xdrs, portp)) {
				note(N_WARNING,
					"%s: [%s] can't decode reply body!",
					__func__,
					inet_ntoa(sin->sin_addr));
				lp = NULL;
				goto done;
			}
		}
		break;
	}

done:
	xdr_destroy(xdrs);
	return lp;
}

/*
 * Notify operation for a single list entry
 */
static int
process_entry(notify_list *lp)
{
	struct sockaddr_in	sin;
	struct status		new_status;
	xdrproc_t		func;
	void			*objp;
	u_int32_t		proc, vers, prog;
/* 	__u32			proc, vers, prog; */

	if (NL_TIMES(lp) == 0) {
		note(N_DEBUG, "%s: Cannot notify %s, giving up.",
				__func__, inet_ntoa(NL_ADDR(lp)));
		return 0;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port   = lp->port;
	/* LH - moved address into switch */

	prog = NL_MY_PROG(lp);
	vers = NL_MY_VERS(lp);
	proc = NL_MY_PROC(lp);

	/* __FORCE__ loopback for callbacks to lockd ... */
	/* Just in case we somehow ignored it thus far */
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	func = (xdrproc_t) xdr_status;
	objp = &new_status;
	new_status.mon_name = NL_MON_NAME(lp);
	new_status.state    = NL_STATE(lp);
	memcpy(new_status.priv, NL_PRIV(lp), SM_PRIV_SIZE);

	lp->xid = xmit_call(&sin, prog, vers, proc, func, objp);
	if (!lp->xid) {
		note(N_WARNING, "%s: failed to notify port %d",
				__func__, ntohs(lp->port));
	}
	NL_TIMES(lp) -= 1;

	return 1;
}

/*
 * Process a datagram received on the notify socket
 */
int
process_reply(FD_SET_TYPE *rfds)
{
	struct sockaddr_in	sin;
	notify_list		*lp;
	u_long			port;

	if (sockfd == -1 || !FD_ISSET(sockfd, rfds))
		return 0;

	if (!(lp = recv_rply(&sin, &port)))
		return 1;

	if (lp->port == 0) {
		if (port != 0) {
			lp->port = htons((unsigned short) port);
			process_entry(lp);
			NL_WHEN(lp) = time(NULL) + NOTIFY_TIMEOUT;
			nlist_remove(&notify, lp);
			nlist_insert_timer(&notify, lp);
			return 1;
		}
		note(N_WARNING, "%s: [%s] service %d not registered",
			__func__, inet_ntoa(lp->addr), NL_MY_PROG(lp));
	} else {
		dprintf(N_DEBUG, "%s: Callback to %s (for %d) succeeded.",
			__func__, NL_MY_NAME(lp), NL_MON_NAME(lp));
	}
	nlist_free(&notify, lp);
	return 1;
}

/*
 * Process a notify list, either for notifying remote hosts after reboot
 * or for calling back (local) statd clients when the remote has notified
 * us of a crash. 
 */
int
process_notify_list(void)
{
	notify_list	*entry;
	time_t		now;

	while ((entry = notify) != NULL && NL_WHEN(entry) < time(&now)) {
		if (process_entry(entry)) {
			NL_WHEN(entry) = time(NULL) + NOTIFY_TIMEOUT;
			nlist_remove(&notify, entry);
			nlist_insert_timer(&notify, entry);
		} else {
			note(N_ERROR,
				"%s: Can't callback %s (%d,%d), giving up.",
					__func__,
					NL_MY_NAME(entry),
					NL_MY_PROG(entry),
					NL_MY_VERS(entry));
			nlist_free(&notify, entry);
		}
	}

	return 1;
}
