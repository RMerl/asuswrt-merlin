/*
 * Provide a variety of APIs that query an rpcbind daemon to
 * discover RPC service ports and allowed protocol version
 * numbers.
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

#ifdef HAVE_LIBTIRPC
#include <netconfig.h>
#include <rpc/rpcb_prot.h>
#endif

#include "nfsrpc.h"

/*
 * Try a local socket first to access the local rpcbind daemon
 *
 * Rpcbind's local socket service does not seem to be working.
 * Disable this logic for now.
 */
#ifdef HAVE_LIBTIRPC
#undef NFS_GP_LOCAL
#else	/* !HAVE_LIBTIRPC */
#undef NFS_GP_LOCAL
#endif	/* !HAVE_LIBTIRPC */

#ifdef HAVE_LIBTIRPC
static const rpcvers_t default_rpcb_version = RPCBVERS_4;
#else	/* !HAVE_LIBTIRPC */
static const rpcvers_t default_rpcb_version = PMAPVERS;
#endif	/* !HAVE_LIBTIRPC */

#ifdef HAVE_DECL_AI_ADDRCONFIG
/*
 * getaddrinfo(3) generates a usable loopback address based on how the
 * local network interfaces are configured.  RFC 3484 requires that the
 * results are sorted so that the first result has the best likelihood
 * of working, so we try just that first result.
 *
 * Returns TRUE on success.
 */
static int nfs_gp_loopback_address(struct sockaddr *sap, socklen_t *salen)
{
	struct addrinfo *gai_results;
	struct addrinfo gai_hint = {
		.ai_flags	= AI_ADDRCONFIG,
	};
	socklen_t len = *salen;
	int ret = 0;

	if (getaddrinfo(NULL, "sunrpc", &gai_hint, &gai_results))
		return 0;

	switch (gai_results->ai_addr->sa_family) {
	case AF_INET:
	case AF_INET6:
		if (len >= gai_results->ai_addrlen) {
			memcpy(sap, gai_results->ai_addr,
					gai_results->ai_addrlen);
			*salen = gai_results->ai_addrlen;
			ret = 1;
		}
	}

	freeaddrinfo(gai_results);
	return ret;
}
#else
/*
 * Old versions of getaddrinfo(3) don't support AI_ADDRCONFIG, so we
 * have a fallback for building on legacy systems.
 */
static int nfs_gp_loopback_address(struct sockaddr *sap, socklen_t *salen)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sap;

	memset(sin, 0, sizeof(*sin));

	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	*salen = sizeof(*sin);

	return 1;
}
#endif

/*
 * Plant port number in @sap.  @port is already in network byte order.
 */
static void nfs_gp_set_port(struct sockaddr *sap, const in_port_t port)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sap;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sap;

	switch (sap->sa_family) {
	case AF_INET:
		sin->sin_port = port;
		break;
	case AF_INET6:
		sin6->sin6_port = port;
		break;
	}
}

/*
 * Look up a network service in /etc/services and return the
 * network-order port number of that service.
 */
static in_port_t nfs_gp_getservbyname(const char *service,
				      const unsigned short protocol)
{
	const struct addrinfo gai_hint = {
		.ai_family	= AF_INET,
		.ai_protocol	= protocol,
		.ai_flags	= AI_PASSIVE,
	};
	struct addrinfo *gai_results;
	const struct sockaddr_in *sin;
	in_port_t port;

	if (getaddrinfo(NULL, service, &gai_hint, &gai_results) != 0)
		return 0;

	sin = (const struct sockaddr_in *)gai_results->ai_addr;
	port = sin->sin_port;
	
	freeaddrinfo(gai_results);
	return port;
}

/*
 * Discover the port number that should be used to contact an
 * rpcbind service.  This will detect if the port has a local
 * value that may have been set in /etc/services.
 *
 * Returns network byte-order port number of rpcbind service
 * on this system.
 */
static in_port_t nfs_gp_get_rpcb_port(const unsigned short protocol)
{
	static const char *rpcb_netnametbl[] = {
		"rpcbind",
		"portmapper",
		"sunrpc",
		NULL,
	};
	unsigned int i;

	for (i = 0; rpcb_netnametbl[i] != NULL; i++) {
		in_port_t port;

		port = nfs_gp_getservbyname(rpcb_netnametbl[i], protocol);
		if (port != 0)
			return port;
	}

	return (in_port_t)htons((uint16_t)PMAPPORT);
}

/*
 * Set up an RPC client for communicating with an rpcbind daemon at
 * @sap over @transport with protocol version @version.
 *
 * Returns a pointer to a prepared RPC client if successful, and
 * @timeout is initialized; caller must destroy a non-NULL returned RPC
 * client.  Otherwise returns NULL, and rpc_createerr.cf_stat is set to
 * reflect the error.
 */
static CLIENT *nfs_gp_get_rpcbclient(const struct sockaddr *sap,
				     const socklen_t salen,
				     const unsigned short transport,
				     const rpcvers_t version,
				     struct timeval *timeout)
{
	static const char *rpcb_pgmtbl[] = {
		"rpcbind",
		"portmap",
		"portmapper",
		"sunrpc",
		NULL,
	};
	struct sockaddr_storage address;
	struct sockaddr *saddr = (struct sockaddr *)&address;
	rpcprog_t rpcb_prog = nfs_getrpcbyname(RPCBPROG, rpcb_pgmtbl);

	memcpy(saddr, sap, (size_t)salen);
	nfs_gp_set_port(saddr, nfs_gp_get_rpcb_port(transport));

	return nfs_get_rpcclient(saddr, salen, transport, rpcb_prog,
					version, timeout);
}

/*
 * One of the arguments passed when querying remote rpcbind services
 * via rpcbind v3 or v4 is a netid string.  This replaces the pm_prot
 * field used in legacy PMAP_GETPORT calls.
 *
 * RFC 1833 says netids are not standard but rather defined on the local
 * host.  There are, however, standard definitions for nc_protofmly and
 * nc_proto that can be used to derive a netid string on the local host,
 * based on the contents of /etc/netconfig.
 *
 * Walk through the local netconfig database and grab the netid of the
 * first entry that matches @family and @protocol and whose netid string
 * fits in the provided buffer.
 *
 * Returns a '\0'-terminated string if successful; otherwise NULL.
 * rpc_createerr.cf_stat is set to reflect the error.
 */
#ifdef HAVE_LIBTIRPC

static char *nfs_gp_get_netid(const sa_family_t family,
			      const unsigned short protocol)
{
	char *nc_protofmly, *nc_proto, *nc_netid;
	struct netconfig *nconf;
	struct protoent *proto;
	void *handle;

	switch (family) {
	case AF_LOCAL:
	case AF_INET:
		nc_protofmly = NC_INET;
		break;
	case AF_INET6:
		nc_protofmly = NC_INET6;
		break;
	default:
		goto out;
	}

	proto = getprotobynumber(protocol);
	if (proto == NULL)
		goto out;
	nc_proto = proto->p_name;

	handle = setnetconfig();
	while ((nconf = getnetconfig(handle)) != NULL) {

		if (nconf->nc_protofmly != NULL &&
		    strcmp(nconf->nc_protofmly, nc_protofmly) != 0)
			continue;
		if (nconf->nc_proto != NULL &&
		    strcmp(nconf->nc_proto, nc_proto) != 0)
			continue;

		nc_netid = strdup(nconf->nc_netid);
		endnetconfig(handle);
		return nc_netid;
	}
	endnetconfig(handle);

out:
	rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
	return NULL;
}

#endif	/* HAVE_LIBTIRPC */

/*
 * Extract a port number from a universal address, and terminate the
 * string in @addrstr just after the address part.
 *
 * Returns -1 if unsuccesful; otherwise a decoded port number (possibly 0)
 * is returned.
 */
static int nfs_gp_universal_porthelper(char *addrstr)
{
	char *p, *endptr;
	unsigned long portlo, porthi;
	int port = -1;

	p = strrchr(addrstr, '.');
	if (p == NULL)
		goto out;
	portlo = strtoul(p + 1, &endptr, 10);
	if (*endptr != '\0' || portlo > 255)
		goto out;
	*p = '\0';

	p = strrchr(addrstr, '.');
	if (p == NULL)
		goto out;
	porthi = strtoul(p + 1, &endptr, 10);
	if (*endptr != '\0' || porthi > 255)
		goto out;
	*p = '\0';
	port = (porthi << 8) | portlo;

out:
	return port;
}

/**
 * nfs_universal2port - extract port number from a "universal address"
 * @uaddr: '\0'-terminated C string containing a universal address
 *
 * Universal addresses (defined in RFC 1833) are used when calling an
 * rpcbind daemon via protocol versions 3 or 4..
 *
 * Returns -1 if unsuccesful; otherwise a decoded port number (possibly 0)
 * is returned.
 */
int nfs_universal2port(const char *uaddr)
{
	char *addrstr;
	int port = -1;

	addrstr = strdup(uaddr);
	if (addrstr != NULL) {
		port = nfs_gp_universal_porthelper(addrstr);
		free(addrstr);
	}
	return port;
}

/**
 * nfs_sockaddr2universal - convert a sockaddr to a "universal address"
 * @sap: pointer to a socket address
 * @salen: length of socket address
 *
 * Universal addresses (defined in RFC 1833) are used when calling an
 * rpcbind daemon via protocol versions 3 or 4..
 *
 * Returns a '\0'-terminated string if successful; caller must free
 * the returned string.  Otherwise NULL is returned and
 * rpc_createerr.cf_stat is set to reflect the error.
 *
 */
#ifdef HAVE_GETNAMEINFO

char *nfs_sockaddr2universal(const struct sockaddr *sap,
			     const socklen_t salen)
{
	struct sockaddr_un *sun = (struct sockaddr_un *)sap;
	char buf[NI_MAXHOST];
	uint16_t port;

	switch (sap->sa_family) {
	case AF_LOCAL:
		return strndup(sun->sun_path, sizeof(sun->sun_path));
	case AF_INET:
		if (getnameinfo(sap, salen, buf, (socklen_t)sizeof(buf),
					NULL, 0, NI_NUMERICHOST) != 0)
			goto out_err;
		port = ntohs(((struct sockaddr_in *)sap)->sin_port);
		break;
	case AF_INET6:
		if (getnameinfo(sap, salen, buf, (socklen_t)sizeof(buf),
					NULL, 0, NI_NUMERICHOST) != 0)
			goto out_err;
		port = ntohs(((struct sockaddr_in6 *)sap)->sin6_port);
		break;
	default:
		goto out_err;
	}

	(void)snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ".%u.%u",
			(unsigned)(port >> 8), (unsigned)(port & 0xff));

	return strdup(buf);

out_err:
	rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
	return NULL;
}

#else	/* HAVE_GETNAMEINFO */

char *nfs_sockaddr2universal(const struct sockaddr *sap,
			     const socklen_t salen)
{
	struct sockaddr_un *sun = (struct sockaddr_un *)sap;
	char buf[NI_MAXHOST];
	uint16_t port;
	char *addr;

	switch (sap->sa_family) {
	case AF_LOCAL:
		return strndup(sun->sun_path, sizeof(sun->sun_path));
	case AF_INET:
		addr = inet_ntoa(((struct sockaddr_in *)sap)->sin_addr);
		if (addr != NULL && strlen(addr) > sizeof(buf))
			goto out_err;
		strcpy(buf, addr);
		port = ntohs(((struct sockaddr_in *)sap)->sin_port);
		break;
	default:
		goto out_err;
	}

	(void)snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ".%u.%u",
			(unsigned)(port >> 8), (unsigned)(port & 0xff));

	return strdup(buf);

out_err:
	rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
	return NULL;
}

#endif	/* HAVE_GETNAMEINFO */

/*
 * Send a NULL request to the indicated RPC service.
 *
 * Returns 1 if the service responded; otherwise 0;
 */
static int nfs_gp_ping(CLIENT *client, struct timeval timeout)
{
	enum clnt_stat status;

	status = CLNT_CALL(client, NULLPROC,
			   (xdrproc_t)xdr_void, NULL,
			   (xdrproc_t)xdr_void, NULL,
			   timeout);

	if (status != RPC_SUCCESS) {
		rpc_createerr.cf_stat = status;
		CLNT_GETERR(client, &rpc_createerr.cf_error);
	}
	return (int)(status == RPC_SUCCESS);
}

#ifdef HAVE_LIBTIRPC

/*
 * Initialize the rpcb argument for a GETADDR request.
 *
 * The rpcbind daemon ignores the parms.r_owner field in GETADDR
 * requests, but we plant an eye-catcher to help distinguish these
 * requests in network traces.
 *
 * Returns 1 if successful, and caller must free strings pointed
 * to by r_netid and r_addr; otherwise 0.
 */
static int nfs_gp_init_rpcb_parms(const struct sockaddr *sap,
				  const socklen_t salen,
				  const rpcprog_t program,
				  const rpcvers_t version,
				  const unsigned short protocol,
				  struct rpcb *parms)
{
	char *netid, *addr;

	netid = nfs_gp_get_netid(sap->sa_family, protocol);
	if (netid == NULL)
		return 0;

	addr = nfs_sockaddr2universal(sap, salen);
	if (addr == NULL) {
		free(netid);
		return 0;
	}

	memset(parms, 0, sizeof(*parms));
	parms->r_prog	= program;
	parms->r_vers	= version;
	parms->r_netid	= netid;
	parms->r_addr	= addr;
	parms->r_owner	= "nfs-utils";	/* eye-catcher */

	return 1;
}

static void nfs_gp_free_rpcb_parms(struct rpcb *parms)
{
	free(parms->r_netid);
	free(parms->r_addr);
}

/*
 * Try rpcbind GETADDR via version 4.  If that fails, try same
 * request via version 3.
 *
 * Returns non-zero port number on success; otherwise returns
 * zero.  rpccreateerr is set to reflect the nature of the error.
 */
static unsigned short nfs_gp_rpcb_getaddr(CLIENT *client,
					  struct rpcb *parms,
					  struct timeval timeout)
{
	rpcvers_t rpcb_version;
	struct rpc_err rpcerr;
	int port = 0;

	for (rpcb_version = RPCBVERS_4;
	     rpcb_version >= RPCBVERS_3;
	     rpcb_version--) {
		enum clnt_stat status;
		char *uaddr = NULL;

		CLNT_CONTROL(client, CLSET_VERS, (void *)&rpcb_version);
		status = CLNT_CALL(client, (rpcproc_t)RPCBPROC_GETADDR,
				   (xdrproc_t)xdr_rpcb, (void *)parms,
				   (xdrproc_t)xdr_wrapstring, (void *)&uaddr,
				   timeout);

		switch (status) {
		case RPC_SUCCESS:
			if ((uaddr == NULL) || (uaddr[0] == '\0')) {
				rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
				continue;
			}

			port = nfs_universal2port(uaddr);
			xdr_free((xdrproc_t)xdr_wrapstring, (char *)&uaddr);
			if (port == -1) {
				rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
				return 0;
			}
			return (unsigned short)port;
		case RPC_PROGVERSMISMATCH:
			clnt_geterr(client, &rpcerr);
			if (rpcerr.re_vers.low > RPCBVERS4)
				return 0;
			continue;
		case RPC_PROCUNAVAIL:
		case RPC_PROGUNAVAIL:
			continue;
		default:
			/* Most likely RPC_TIMEDOUT or RPC_CANTRECV */
			rpc_createerr.cf_stat = status;
			clnt_geterr(client, &rpc_createerr.cf_error);
			return 0;
		}

	}

	if (port == 0) {
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		clnt_geterr(client, &rpc_createerr.cf_error);
	}
	return port;
}

#endif	/* HAVE_LIBTIRPC */

/*
 * Try GETPORT request via rpcbind version 2.
 *
 * Returns non-zero port number on success; otherwise returns
 * zero.  rpccreateerr is set to reflect the nature of the error.
 */
static unsigned long nfs_gp_pmap_getport(CLIENT *client,
					 struct pmap *parms,
					 struct timeval timeout)
{
	enum clnt_stat status;
	unsigned long port;

	status = CLNT_CALL(client, (rpcproc_t)PMAPPROC_GETPORT,
			   (xdrproc_t)xdr_pmap, (void *)parms,
			   (xdrproc_t)xdr_u_long, (void *)&port,
			   timeout);

	if (status != RPC_SUCCESS) {
		rpc_createerr.cf_stat = status;
		CLNT_GETERR(client, &rpc_createerr.cf_error);
		port = 0;
	} else if (port == 0)
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;

	return port;
}

#ifdef HAVE_LIBTIRPC

static unsigned short nfs_gp_getport_rpcb(CLIENT *client,
					  const struct sockaddr *sap,
					  const socklen_t salen,
					  const rpcprog_t program,
					  const rpcvers_t version,
					  const unsigned short protocol,
					  struct timeval timeout)
{
	unsigned short port = 0;
	struct rpcb parms;

	if (nfs_gp_init_rpcb_parms(sap, salen, program,
					version, protocol, &parms) != 0) {
		port = nfs_gp_rpcb_getaddr(client, &parms, timeout);
		nfs_gp_free_rpcb_parms(&parms);
	}

	return port;
}

#endif	/* HAVE_LIBTIRPC */

static unsigned long nfs_gp_getport_pmap(CLIENT *client,
					 const rpcprog_t program,
					 const rpcvers_t version,
					 const unsigned short protocol,
					 struct timeval timeout)
{
	struct pmap parms = {
		.pm_prog	= program,
		.pm_vers	= version,
		.pm_prot	= protocol,
	};
	rpcvers_t pmap_version = PMAPVERS;

	CLNT_CONTROL(client, CLSET_VERS, (void *)&pmap_version);
	return nfs_gp_pmap_getport(client, &parms, timeout);
}

/*
 * Try an AF_INET6 request via rpcbind v4/v3; try an AF_INET
 * request via rpcbind v2.
 *
 * Returns non-zero port number on success; otherwise returns
 * zero.  rpccreateerr is set to reflect the nature of the error.
 */
static unsigned short nfs_gp_getport(CLIENT *client,
				     const struct sockaddr *sap,
				     const socklen_t salen,
				     const rpcprog_t program,
				     const rpcvers_t version,
				     const unsigned short protocol,
				     struct timeval timeout)
{
	switch (sap->sa_family) {
#ifdef HAVE_LIBTIRPC
	case AF_INET6:
		return nfs_gp_getport_rpcb(client, sap, salen, program,
						version, protocol, timeout);
#endif	/* HAVE_LIBTIRPC */
	case AF_INET:
		return nfs_gp_getport_pmap(client, program, version,
							protocol, timeout);
	}

	rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
	return 0;
}

/**
 * nfs_rcp_ping - Determine if RPC service is responding to requests
 * @sap: pointer to address of server to query (port is already filled in)
 * @salen: length of server address
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: requested IPPROTO_ value of transport protocol
 * @timeout: pointer to request timeout (NULL means use default timeout)
 *
 * Returns 1 if the remote service responded without an error; otherwise
 * zero.
 */
int nfs_rpc_ping(const struct sockaddr *sap, const socklen_t salen,
		 const rpcprog_t program, const rpcvers_t version,
		 const unsigned short protocol, const struct timeval *timeout)
{
	CLIENT *client;
	struct timeval tout = { -1, 0 };
	int result = 0;

	if (timeout != NULL)
		tout = *timeout;

	client = nfs_get_rpcclient(sap, salen, protocol, program, version, &tout);
	if (client != NULL) {
		result = nfs_gp_ping(client, tout);
		CLNT_DESTROY(client);
	}

	return result;
}

/**
 * nfs_getport - query server's rpcbind to get port number for an RPC service
 * @sap: pointer to address of server to query
 * @salen: length of server's address
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: IPPROTO_ value of requested transport protocol
 *
 * Uses any acceptable rpcbind version to discover the port number for the
 * RPC service described by the given [program, version, transport] tuple.
 * Uses a quick timeout and an ephemeral source port.  Supports AF_INET and
 * AF_INET6 server addresses.
 *
 * Returns a positive integer representing the port number of the RPC
 * service advertised by the server (in host byte order), or zero if the
 * service is not advertised or there was some problem querying the server's
 * rpcbind daemon.  rpccreateerr is set to reflect the underlying cause of
 * the error.
 *
 * There are a variety of ways to choose which transport and rpcbind versions
 * to use.  We chose to conserve local resources and try to avoid incurring
 * timeouts.
 *
 * Transport
 * To provide rudimentary support for traversing firewalls, query the remote
 * using the same transport as the requested service.  This provides some
 * guarantee that the requested transport is available between this client
 * and the server, and if the caller specifically requests TCP, for example,
 * this may be becuase a firewall is in place that blocks UDP traffic.  We
 * could try both, but that could involve a lengthy timeout in several cases,
 * and would often consume an extra ephemeral port.
 *
 * Rpcbind version
 * To avoid using up too many ephemeral ports, AF_INET queries use tried-and-
 * true rpcbindv2, and don't try the newer versions; and AF_INET6 queries use
 * rpcbindv4, then rpcbindv3 on the same socket.  The newer rpcbind protocol
 * versions can adequately detect if a remote RPC service does not support
 * AF_INET6 at all.  The rpcbind socket is re-used in an attempt to keep the
 * overall number of consumed ephemeral ports low.
 */
unsigned short nfs_getport(const struct sockaddr *sap,
			   const socklen_t salen,
			   const rpcprog_t program,
			   const rpcvers_t version,
			   const unsigned short protocol)
{
	struct timeval timeout = { -1, 0 };
	unsigned short port = 0;
	CLIENT *client;

	client = nfs_gp_get_rpcbclient(sap, salen, protocol,
						default_rpcb_version, &timeout);
	if (client != NULL) {
		port = nfs_gp_getport(client, sap, salen, program,
					version, protocol, timeout);
		CLNT_DESTROY(client);
	}

	return port;
}

/**
 * nfs_getport_ping - query server's rpcbind and do RPC ping to verify result
 * @sap: IN: pointer to address of server to query;
 *	 OUT: pointer to updated address
 * @salen: length of server's address
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: IPPROTO_ value of requested transport protocol
 *
 * Uses any acceptable rpcbind version to discover the port number for the
 * RPC service described by the given [program, version, transport] tuple.
 * Uses a quick timeout and an ephemeral source port.  Supports AF_INET and
 * AF_INET6 server addresses.
 *
 * Returns a 1 and sets the port number in the passed-in server address
 * if both the query and the ping were successful; otherwise zero.
 * rpccreateerr is set to reflect the underlying cause of the error.
 */
int nfs_getport_ping(struct sockaddr *sap, const socklen_t salen,
		     const rpcprog_t program, const rpcvers_t version,
		     const unsigned short protocol)
{
	struct timeval timeout = { -1, 0 };
	unsigned short port = 0;
	CLIENT *client;
	int result = 0;
	
	client = nfs_gp_get_rpcbclient(sap, salen, protocol,
						default_rpcb_version, &timeout);
	if (client != NULL) {
		port = nfs_gp_getport(client, sap, salen, program,
					version, protocol, timeout);
		CLNT_DESTROY(client);
		client = NULL;
	}

	if (port != 0) {
		struct sockaddr_storage address;
		struct sockaddr *saddr = (struct sockaddr *)&address;

		memcpy(saddr, sap, (size_t)salen);
		nfs_gp_set_port(saddr, htons(port));

		client = nfs_get_rpcclient(saddr, salen, protocol,
						program, version, &timeout);
		if (client != NULL) {
			result = nfs_gp_ping(client, timeout);
			CLNT_DESTROY(client);
		}
	}

	if (result)
		nfs_gp_set_port(sap, htons(port));

	return result;
}

/**
 * nfs_getlocalport - query local rpcbind to get port number for an RPC service
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: IPPROTO_ value of requested transport protocol
 *
 * Uses any acceptable rpcbind version to discover the port number for the
 * RPC service described by the given [program, version, transport] tuple.
 * Uses a quick timeout and an ephemeral source port.  Supports AF_INET and
 * AF_INET6 local addresses.
 *
 * Returns a positive integer representing the port number of the RPC
 * service advertised by the server (in host byte order), or zero if the
 * service is not advertised or there was some problem querying the server's
 * rpcbind daemon.  rpccreateerr is set to reflect the underlying cause of
 * the error.
 *
 * Try an AF_LOCAL connection first.  The rpcbind daemon implementation should
 * listen on AF_LOCAL.
 *
 * If that doesn't work (for example, if portmapper is running, or rpcbind
 * isn't listening on /var/run/rpcbind.sock), send a query via UDP to localhost
 * (UDP doesn't leave a socket in TIME_WAIT, and the timeout is a relatively
 * short 3 seconds).
 *
 * getaddrinfo(3) generates a usable loopback address.  RFC 3484 requires that
 * the results are sorted so that the first result has the best likelihood of
 * working, so we try just that first result.  If IPv6 is all that is
 * available, we are sure to generate an AF_INET6 loopback address and use
 * rpcbindv4/v3 GETADDR.  AF_INET6 requests go via rpcbind v4/3 in order to
 * detect if the requested RPC service supports AF_INET6 or not.
 */
unsigned short nfs_getlocalport(const rpcprot_t program,
				const rpcvers_t version,
				const unsigned short protocol)
{
	struct sockaddr_storage address;
	struct sockaddr *lb_addr = (struct sockaddr *)&address;
	socklen_t lb_len = sizeof(*lb_addr);
	unsigned short port = 0;

#ifdef NFS_GP_LOCAL
	const struct sockaddr_un sun = {
		.sun_family	= AF_LOCAL,
		.sun_path	= _PATH_RPCBINDSOCK,
	};
	const struct sockaddr *sap = (struct sockaddr *)&sun;
	const socklen_t salen = SUN_LEN(&sun);
	CLIENT *client;
	struct timeval timeout = { -1, 0 };

	client = nfs_gp_get_rpcbclient(sap, salen, 0, RPCBVERS_4, &timeout);
	if (client != NULL) {
		struct rpcb parms;

		if (nfs_gp_init_rpcb_parms(sap, salen, program, version,
						protocol, &parms) != 0) {
			port = nfs_gp_rpcb_getaddr(client, &parms, timeout);
			nfs_gp_free_rpcb_parms(&parms);
		}
		CLNT_DESTROY(client);
	}
#endif	/* NFS_GP_LOCAL */

	if (port == 0) {
		if (nfs_gp_loopback_address(lb_addr, &lb_len)) {
			port = nfs_getport(lb_addr, lb_len,
						program, version, protocol);
		} else
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
	}

	return port;
}

/**
 * nfs_rpcb_getaddr - query rpcbind via rpcbind versions 4 and 3
 * @sap: pointer to address of server to query
 * @salen: length of server address
 * @transport: transport protocol to use for the query
 * @addr: pointer to r_addr address
 * @addrlen: length of address
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: requested IPPROTO_ value of transport protocol
 * @timeout: pointer to request timeout (NULL means use default timeout)
 *
 * Returns a positive integer representing the port number of the RPC
 * service advertised by the server (in host byte order), or zero if the
 * service is not advertised or there was some problem querying the
 * server's rpcbind daemon.  rpccreateerr is set to reflect the
 * underlying cause of the error.
 *
 * This function provides similar functionality to nfs_pmap_getport(),
 * but performs the rpcbind lookup via rpcbind version 4.  If the server
 * doesn't support rpcbind version 4, it will retry with version 3.
 * The GETADDR procedure is exactly the same in these two versions of
 * the rpcbind protocol, so the socket, RPC client, and arguments are
 * re-used when retrying, saving ephemeral port space.
 *
 * These RPC procedures take a universal address as an argument, so the
 * query will fail if the remote rpcbind daemon doesn't find an entry
 * with a matching address.  A matching address includes an ANYADDR
 * address of the same address family.  In this way an RPC server can
 * advertise via rpcbind that it does not support AF_INET6.
 */
#ifdef HAVE_LIBTIRPC

unsigned short nfs_rpcb_getaddr(const struct sockaddr *sap,
				const socklen_t salen,
				const unsigned short transport,
				const struct sockaddr *addr,
				const socklen_t addrlen,
				const rpcprog_t program,
				const rpcvers_t version,
				const unsigned short protocol,
				const struct timeval *timeout)
{
	CLIENT *client;
	struct rpcb parms;
	struct timeval tout = { -1, 0 };
	unsigned short port = 0;

	if (timeout != NULL)
		tout = *timeout;

	client = nfs_gp_get_rpcbclient(sap, salen, transport, RPCBVERS_4, &tout);
	if (client != NULL) {
		if (nfs_gp_init_rpcb_parms(addr, addrlen, program, version,
						protocol, &parms) != 0) {
			port = nfs_gp_rpcb_getaddr(client, &parms, tout);
			nfs_gp_free_rpcb_parms(&parms);
		}
		CLNT_DESTROY(client);
	}

	return port;
}

#else	/* !HAVE_LIBTIRPC */

unsigned short nfs_rpcb_getaddr(const struct sockaddr *sap,
				const socklen_t salen,
				const unsigned short transport,
				const struct sockaddr *addr,
				const socklen_t addrlen,
				const rpcprog_t program,
				const rpcvers_t version,
				const unsigned short protocol,
				const struct timeval *timeout)
{
	rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
	return 0;
}

#endif	/* !HAVE_LIBTIRPC */

/**
 * nfs_pmap_getport - query rpcbind via the portmap protocol (rpcbindv2)
 * @sin: pointer to AF_INET address of server to query
 * @transport: transport protocol to use for the query
 * @program: requested RPC program number
 * @version: requested RPC version number
 * @protocol: requested IPPROTO_ value of transport protocol
 * @timeout: pointer to request timeout (NULL means use default timeout)
 *
 * Returns a positive integer representing the port number of the RPC service
 * advertised by the server (in host byte order), or zero if the service is
 * not advertised or there was some problem querying the server's rpcbind
 * daemon.  rpccreateerr is set to reflect the underlying cause of the error.
 *
 * nfs_pmap_getport() is very similar to pmap_getport(), except that:
 *
 *  1.	This version always tries to use an ephemeral port, since reserved
 *	ports are not needed for GETPORT queries.  This conserves the very
 *	limited reserved port space, helping reduce failed socket binds
 *	during mount storms.
 *
 *  2.	This version times out quickly by default.  It time-limits the
 *	connect process as well as the actual RPC call, and even allows the
 *	caller to specify the timeout.
 *
 *  3.	This version shares code with the rpcbindv3 and rpcbindv4 query
 *	functions.  It can use a TI-RPC generated CLIENT.
 */
unsigned long nfs_pmap_getport(const struct sockaddr_in *sin,
			       const unsigned short transport,
			       const unsigned long program,
			       const unsigned long version,
			       const unsigned long protocol,
			       const struct timeval *timeout)
{
	CLIENT *client;
	struct pmap parms = {
		.pm_prog	= program,
		.pm_vers	= version,
		.pm_prot	= protocol,
	};
	struct timeval tout = { -1, 0 };
	unsigned long port = 0;

	if (timeout != NULL)
		tout = *timeout;

	client = nfs_gp_get_rpcbclient((struct sockaddr *)sin,
					(socklen_t)sizeof(*sin),
					transport, PMAPVERS, &tout);
	if (client != NULL) {
		port = nfs_gp_pmap_getport(client, &parms, tout);
		CLNT_DESTROY(client);
	}

	return port;
}
