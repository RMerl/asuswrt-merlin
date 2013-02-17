/*
 * Miscellaneous functions for RPC service startup and shutdown.
 *
 * This code is partially snarfed from rpcgen -s tcp -s udp,
 * partly written by Mark Shand, Donald Becker, and Rick
 * Sladkey. It was tweaked slightly by Olaf Kirch to be
 * usable by both unfsd and mountd.
 *
 * This software may be used for any purpose provided
 * the above copyright notice is retained.  It is supplied
 * as is, with no warranty expressed or implied.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "nfslib.h"

#if SIZEOF_SOCKLEN_T - 0 == 0
#define socklen_t int
#endif

#define _RPCSVC_CLOSEDOWN	120
int	_rpcpmstart = 0;
int	_rpcfdtype = 0;
int	_rpcsvcdirty = 0;

static void
closedown(int sig)
{
	(void) signal(sig, closedown);

	if (_rpcsvcdirty == 0) {
		static int size;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM)
			exit(0);

		if (size == 0)
			size = getdtablesize();

		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1)
			exit(0);
	}

	(void) alarm(_RPCSVC_CLOSEDOWN);
}

/*
 * Create listener socket for a given port
 *
 * Return an open network socket on success; otherwise return -1
 * if some error occurs.
 */
static int
makesock(int port, int proto)
{
	struct sockaddr_in sin;
	int	sock, sock_type, val;

	sock_type = (proto == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM;
	sock = socket(AF_INET, sock_type, proto);
	if (sock < 0) {
		xlog(L_FATAL, "Could not make a socket: %s",
					strerror(errno));
		return -1;
	}
	memset((char *) &sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	val = 1;
	if (proto == IPPROTO_TCP)
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       &val, sizeof(val)) < 0)
			xlog(L_ERROR, "setsockopt failed: %s",
			     strerror(errno));

	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		xlog(L_FATAL, "Could not bind name to socket: %s",
					strerror(errno));
		return -1;
	}

	return sock;
}

void
rpc_init(char *name, int prog, int vers,
	 void (*dispatch)(struct svc_req *, register SVCXPRT *),
	 int defport)
{
	struct sockaddr_in saddr;
	SVCXPRT	*transp;
	int	sock;
	socklen_t asize;

	asize = sizeof(saddr);
	sock = 0;
	if (getsockname(0, (struct sockaddr *) &saddr, &asize) == 0
	    && saddr.sin_family == AF_INET) {
		socklen_t ssize = sizeof(int);
		int fdtype = 0;
		if (getsockopt(0, SOL_SOCKET, SO_TYPE,
				(char *)&fdtype, &ssize) == -1)
			xlog(L_FATAL, "getsockopt failed: %s", strerror(errno));
		/* inetd passes a UDP socket or a listening TCP socket.
		 * listen will fail on a connected TCP socket(passed by rsh).
		 */
		if (!(fdtype == SOCK_STREAM && listen(0,5) == -1)) {
			_rpcfdtype = fdtype;
			_rpcpmstart = 1;
		}
	}
	if (!_rpcpmstart) {
		pmap_unset(prog, vers);
		sock = RPC_ANYSOCK;
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		static SVCXPRT *last_transp = NULL;

		if (_rpcpmstart == 0) {
			if (last_transp
			    && (!defport || defport == last_transp->xp_port)) {
				transp = last_transp;
				goto udp_transport;
			}
			if (defport == 0)
				sock = RPC_ANYSOCK;
			else
				sock = makesock(defport, IPPROTO_UDP);
		}
		if (sock == RPC_ANYSOCK)
			sock = svcudp_socket (prog, 1);
		transp = svcudp_create(sock);
		if (transp == NULL) {
			xlog(L_FATAL, "cannot create udp service.");
		}
      udp_transport:
		if (!svc_register(transp, prog, vers, dispatch, IPPROTO_UDP)) {
			xlog(L_FATAL, "unable to register (%s, %d, udp).",
					name, vers);
		}
		last_transp = transp;
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		static SVCXPRT *last_transp = NULL;

		if (_rpcpmstart == 0) {
			if (last_transp
			    && (!defport || defport == last_transp->xp_port)) {
				transp = last_transp;
				goto tcp_transport;
			}
			if (defport == 0)
				sock = RPC_ANYSOCK;
			else
				sock = makesock(defport, IPPROTO_TCP);
		}
		if (sock == RPC_ANYSOCK)
			sock = svctcp_socket (prog, 1);
		transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			xlog(L_FATAL, "cannot create tcp service.");
		}
      tcp_transport:
		if (!svc_register(transp, prog, vers, dispatch, IPPROTO_TCP)) {
			xlog(L_FATAL, "unable to register (%s, %d, tcp).",
					name, vers);
		}
		last_transp = transp;
	}

	if (_rpcpmstart) {
		signal(SIGALRM, closedown);
		alarm(_RPCSVC_CLOSEDOWN);
	}
}
