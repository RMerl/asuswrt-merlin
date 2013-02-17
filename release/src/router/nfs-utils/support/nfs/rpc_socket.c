/*
 * Generic RPC client socket-level APIs for nfs-utils
 *
 * Copyright (C) 2008 Oracle Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>

#include "nfsrpc.h"

#ifdef HAVE_LIBTIRPC
#include <netconfig.h>
#include <rpc/rpcb_prot.h>
#endif	/* HAVE_LIBTIRPC */

/*
 * If "-1" is specified in the tv_sec field, use these defaults instead.
 */
#define NFSRPC_TIMEOUT_UDP	(3)
#define NFSRPC_TIMEOUT_TCP	(10)

/*
 * Set up an RPC client for communicating via a AF_LOCAL socket.
 *
 * @timeout is initialized upon return
 *
 * Returns a pointer to a prepared RPC client if successful; caller
 * must destroy a non-NULL returned RPC client.  Otherwise NULL, and
 * rpc_createerr.cf_stat is set to reflect the error.
 */
static CLIENT *nfs_get_localclient(const struct sockaddr *sap,
				   const socklen_t salen,
				   const rpcprog_t program,
				   const rpcvers_t version,
				   struct timeval *timeout)
{
#ifdef HAVE_LIBTIRPC
	struct sockaddr_storage address;
	const struct netbuf nbuf = {
		.maxlen		= sizeof(struct sockaddr_un),
		.len		= (size_t)salen,
		.buf		= &address,
	};
#endif	/* HAVE_LIBTIRPC */
	CLIENT *client;
	int sock;

	sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sock == -1) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		return NULL;
	}

	if (timeout->tv_sec == -1)
		timeout->tv_sec = NFSRPC_TIMEOUT_TCP;

#ifdef HAVE_LIBTIRPC
	memcpy(nbuf.buf, sap, (size_t)salen);
	client = clnt_vc_create(sock, &nbuf, program, version, 0, 0);
#else	/* !HAVE_LIBTIRPC */
	client = clntunix_create((struct sockaddr_un *)sap,
					program, version, &sock, 0, 0);
#endif	/* !HAVE_LIBTIRPC */
	if (client != NULL)
		CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
	else
		(void)close(sock);

	return client;
}

/*
 * Bind a socket using an unused ephemeral source port.
 *
 * Returns zero on success, or returns -1 on error.  errno is
 * set to reflect the nature of the error.
 */
static int nfs_bind(const int sock, const sa_family_t family)
{
	struct sockaddr_in sin = {
		.sin_family		= AF_INET,
		.sin_addr.s_addr	= htonl(INADDR_ANY),
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family		= AF_INET6,
		.sin6_addr		= IN6ADDR_ANY_INIT,
	};

	switch (family) {
	case AF_INET:
		return bind(sock, (struct sockaddr *)&sin,
					(socklen_t)sizeof(sin));
	case AF_INET6:
		return bind(sock, (struct sockaddr *)&sin6,
					(socklen_t)sizeof(sin6));
	}

	errno = EAFNOSUPPORT;
	return -1;
}

#ifdef IPV6_SUPPORTED

/*
 * Bind a socket using an unused privileged source port.
 *
 * Returns zero on success, or returns -1 on error.  errno is
 * set to reflect the nature of the error.
 */
static int nfs_bindresvport(const int sock, const sa_family_t family)
{
	struct sockaddr_in sin = {
		.sin_family		= AF_INET,
		.sin_addr.s_addr	= htonl(INADDR_ANY),
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family		= AF_INET6,
		.sin6_addr		= IN6ADDR_ANY_INIT,
	};

	switch (family) {
	case AF_INET:
		return bindresvport_sa(sock, (struct sockaddr *)&sin);
	case AF_INET6:
		return bindresvport_sa(sock, (struct sockaddr *)&sin6);
	}

	errno = EAFNOSUPPORT;
	return -1;
}

#else	/* !IPV6_SUPPORTED */

/*
 * Bind a socket using an unused privileged source port.
 *
 * Returns zero on success, or returns -1 on error.  errno is
 * set to reflect the nature of the error.
 */
static int nfs_bindresvport(const int sock, const sa_family_t family)
{
	if (family != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	return bindresvport(sock, NULL);
}

#endif	/* !IPV6_SUPPORTED */

/*
 * Perform a non-blocking connect on the socket fd.
 *
 * @timeout is modified to contain the time remaining (i.e. time provided
 * minus time elasped).
 *
 * Returns zero on success, or returns -1 on error.  errno is
 * set to reflect the nature of the error.
 */
static int nfs_connect_nb(const int fd, const struct sockaddr *sap,
			  const socklen_t salen, struct timeval *timeout)
{
	int flags, ret;
	fd_set rset;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;

	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		return -1;

	/*
	 * From here on subsequent sys calls could change errno so
	 * we set ret = -errno to capture it in case we decide to
	 * use it later.
	 */
	ret = connect(fd, sap, salen);
	if (ret < 0 && errno != EINPROGRESS) {
		ret = -1;
		goto done;
	}

	if (ret == 0)
		goto done;

	/* now wait */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);

	ret = select(fd + 1, NULL, &rset, NULL, timeout);
	if (ret <= 0) {
		if (ret == 0)
			errno = ETIMEDOUT;
		ret = -1;
		goto done;
	}

	if (FD_ISSET(fd, &rset)) {
		int status;
		socklen_t len = (socklen_t)sizeof(ret);

		status = getsockopt(fd, SOL_SOCKET, SO_ERROR, &ret, &len);
		if (status < 0) {
			ret = -1;
			goto done;
		}

		/* Oops - something wrong with connect */
		if (ret != 0) {
			errno = ret;
			ret = -1;
		}
	}

done:
	(void)fcntl(fd, F_SETFL, flags);
	return ret;
}

/*
 * Set up an RPC client for communicating via a datagram socket.
 * A connected UDP socket is used to detect a missing remote
 * listener as quickly as possible.
 *
 * @timeout is initialized upon return
 *
 * Returns a pointer to a prepared RPC client if successful; caller
 * must destroy a non-NULL returned RPC client.  Otherwise NULL, and
 * rpc_createerr.cf_stat is set to reflect the error.
 */
static CLIENT *nfs_get_udpclient(const struct sockaddr *sap,
				 const socklen_t salen,
				 const rpcprog_t program,
				 const rpcvers_t version,
				 struct timeval *timeout,
				 const int resvport)
{
	CLIENT *client;
	int ret, sock;
#ifdef HAVE_LIBTIRPC
	struct sockaddr_storage address;
	const struct netbuf nbuf = {
		.maxlen		= salen,
		.len		= salen,
		.buf		= &address,
	};

#else	/* !HAVE_LIBTIRPC */

	if (sap->sa_family != AF_INET) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return NULL;
	}
#endif	/* !HAVE_LIBTIRPC */

	sock = socket((int)sap->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		return NULL;
	}

	if (resvport)
		ret = nfs_bindresvport(sock, sap->sa_family);
	else
		ret = nfs_bind(sock, sap->sa_family);
	if (ret < 0) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		(void)close(sock);
		return NULL;
	}

	if (timeout->tv_sec == -1)
		timeout->tv_sec = NFSRPC_TIMEOUT_UDP;

	ret = nfs_connect_nb(sock, sap, salen, timeout);
	if (ret != 0) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		(void)close(sock);
		return NULL;
	}

#ifdef HAVE_LIBTIRPC
	memcpy(nbuf.buf, sap, (size_t)salen);
	client = clnt_dg_create(sock, &nbuf, program, version, 0, 0);
#else	/* !HAVE_LIBTIRPC */
	client = clntudp_create((struct sockaddr_in *)sap, program,
					version, *timeout, &sock);
#endif	/* !HAVE_LIBTIRPC */
	if (client != NULL) {
		CLNT_CONTROL(client, CLSET_RETRY_TIMEOUT, (char *)timeout);
		CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
	} else
		(void)close(sock);

	return client;
}

/*
 * Set up and connect an RPC client for communicating via a stream socket.
 *
 * @timeout is initialized upon return
 *
 * Returns a pointer to a prepared and connected RPC client if
 * successful; caller must destroy a non-NULL returned RPC client.
 * Otherwise NULL, and rpc_createerr.cf_stat is set to reflect the
 * error.
 */
static CLIENT *nfs_get_tcpclient(const struct sockaddr *sap,
				 const socklen_t salen,
				 const rpcprog_t program,
				 const rpcvers_t version,
				 struct timeval *timeout,
				 const int resvport)
{
	CLIENT *client;
	int ret, sock;
#ifdef HAVE_LIBTIRPC
	struct sockaddr_storage address;
	const struct netbuf nbuf = {
		.maxlen		= salen,
		.len		= salen,
		.buf		= &address,
	};

#else	/* !HAVE_LIBTIRPC */

	if (sap->sa_family != AF_INET) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return NULL;
	}
#endif	/* !HAVE_LIBTIRPC */

	sock = socket((int)sap->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		return NULL;
	}

	if (resvport)
		ret = nfs_bindresvport(sock, sap->sa_family);
	else
		ret = nfs_bind(sock, sap->sa_family);
	if (ret < 0) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		(void)close(sock);
		return NULL;
	}

	if (timeout->tv_sec == -1)
		timeout->tv_sec = NFSRPC_TIMEOUT_TCP;

	ret = nfs_connect_nb(sock, sap, salen, timeout);
	if (ret != 0) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		(void)close(sock);
		return NULL;
	}

#ifdef HAVE_LIBTIRPC
	memcpy(nbuf.buf, sap, (size_t)salen);
	client = clnt_vc_create(sock, &nbuf, program, version, 0, 0);
#else	/* !HAVE_LIBTIRPC */
	client = clnttcp_create((struct sockaddr_in *)sap,
					program, version, &sock, 0, 0);
#endif	/* !HAVE_LIBTIRPC */
	if (client != NULL)
		CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
	else
		(void)close(sock);

	return client;
}

/**
 * nfs_get_rpcclient - acquire an RPC client
 * @sap: pointer to socket address of RPC server
 * @salen: length of socket address
 * @transport: IPPROTO_ value of transport protocol to use
 * @program: RPC program number
 * @version: RPC version number
 * @timeout: pointer to request timeout (must not be NULL)
 *
 * Set up an RPC client for communicating with an RPC program @program
 * and @version on the server @sap over @transport.  An unprivileged
 * source port is used.
 *
 * Returns a pointer to a prepared RPC client if successful, and
 * @timeout is initialized; caller must destroy a non-NULL returned RPC
 * client.  Otherwise returns NULL, and rpc_createerr.cf_stat is set to
 * reflect the error.
 */
CLIENT *nfs_get_rpcclient(const struct sockaddr *sap,
			  const socklen_t salen,
			  const unsigned short transport,
			  const rpcprog_t program,
			  const rpcvers_t version,
			  struct timeval *timeout)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sap;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sap;

	switch (sap->sa_family) {
	case AF_LOCAL:
		return nfs_get_localclient(sap, salen, program,
						version, timeout);
	case AF_INET:
		if (sin->sin_port == 0) {
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
			return NULL;
		}
		break;
	case AF_INET6:
		if (sin6->sin6_port == 0) {
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
			return NULL;
		}
		break;
	default:
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return NULL;
	}

	switch (transport) {
	case IPPROTO_TCP:
		return nfs_get_tcpclient(sap, salen, program, version,
						timeout, 0);
	case 0:
	case IPPROTO_UDP:
		return nfs_get_udpclient(sap, salen, program, version,
						timeout, 0);
	}

	rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
	return NULL;
}

/**
 * nfs_get_priv_rpcclient - acquire an RPC client
 * @sap: pointer to socket address of RPC server
 * @salen: length of socket address
 * @transport: IPPROTO_ value of transport protocol to use
 * @program: RPC program number
 * @version: RPC version number
 * @timeout: pointer to request timeout (must not be NULL)
 *
 * Set up an RPC client for communicating with an RPC program @program
 * and @version on the server @sap over @transport.  A privileged
 * source port is used.
 *
 * Returns a pointer to a prepared RPC client if successful, and
 * @timeout is initialized; caller must destroy a non-NULL returned RPC
 * client.  Otherwise returns NULL, and rpc_createerr.cf_stat is set to
 * reflect the error.
 */
CLIENT *nfs_get_priv_rpcclient(const struct sockaddr *sap,
			       const socklen_t salen,
			       const unsigned short transport,
			       const rpcprog_t program,
			       const rpcvers_t version,
			       struct timeval *timeout)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sap;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sap;

	switch (sap->sa_family) {
	case AF_LOCAL:
		return nfs_get_localclient(sap, salen, program,
						version, timeout);
	case AF_INET:
		if (sin->sin_port == 0) {
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
			return NULL;
		}
		break;
	case AF_INET6:
		if (sin6->sin6_port == 0) {
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
			return NULL;
		}
		break;
	default:
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return NULL;
	}

	switch (transport) {
	case IPPROTO_TCP:
		return nfs_get_tcpclient(sap, salen, program, version,
						timeout, 1);
	case 0:
	case IPPROTO_UDP:
		return nfs_get_udpclient(sap, salen, program, version,
						timeout, 1);
	}

	rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
	return NULL;
}

/**
 * nfs_getrpcbyname - convert an RPC program name to a rpcprog_t
 * @program: default program number to use if names not found in db
 * @table: pointer to table of 'char *' names to try to find
 *
 * Returns program number of first name to be successfully looked
 * up, or the default program number if all lookups fail.
 */
rpcprog_t nfs_getrpcbyname(const rpcprog_t program, const char *table[])
{
#ifdef HAVE_GETRPCBYNAME
	struct rpcent *entry;
	unsigned int i;

	if (table != NULL)
		for (i = 0; table[i] != NULL; i++) {
			entry = getrpcbyname(table[i]);
			if (entry)
				return (rpcprog_t)entry->r_number;
		}
#endif	/* HAVE_GETRPCBYNAME */

	return program;
}
