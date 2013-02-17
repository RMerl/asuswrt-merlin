/*
 * nfsrpc.h -- RPC client APIs provided by support/nfs
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

#ifndef __NFS_UTILS_NFSRPC_H
#define __NFS_UTILS_NFSRPC_H

#include <rpc/types.h>
#include <rpc/clnt.h>

/*
 * Conventional RPC program numbers
 */
#ifndef RPCBPROG
#define RPCBPROG	((rpcprog_t)100000)
#endif
#ifndef PMAPPROG
#define PMAPPROG	((rpcprog_t)100000)
#endif

#ifndef NFSPROG
#define NFSPROG		((rpcprog_t)100003)
#endif
#ifndef MOUNTPROG
#define MOUNTPROG	((rpcprog_t)100005)
#endif
#ifndef NLMPROG
#define NLMPROG		((rpcprog_t)100021)
#endif
#ifndef NSMPROG
#define NSMPROG		((rpcprog_t)100024)
#endif

/*
 * Look up an RPC program name in /etc/rpc
 */
extern rpcprog_t	nfs_getrpcbyname(const rpcprog_t, const char *table[]);

/*
 * Acquire an RPC CLIENT * with an ephemeral source port
 */
extern CLIENT		*nfs_get_rpcclient(const struct sockaddr *,
				const socklen_t, const unsigned short,
				const rpcprog_t, const rpcvers_t,
				struct timeval *);

/*
 * Acquire an RPC CLIENT * with a privileged source port
 */
extern CLIENT		*nfs_get_priv_rpcclient( const struct sockaddr *,
				const socklen_t, const unsigned short,
				const rpcprog_t, const rpcvers_t,
				struct timeval *);

/*
 * Convert a socket address to a universal address
 */
extern char		*nfs_sockaddr2universal(const struct sockaddr *,
				const socklen_t);

/*
 * Extract port number from a universal address
 */
extern int		nfs_universal2port(const char *);

/*
 * Generic function that maps an RPC service tuple to an IP port
 * number of the service on a remote post, and sends a NULL
 * request to determine if the service is responding to requests
 */
extern int		nfs_getport_ping(struct sockaddr *sap,
				const socklen_t salen,
				const rpcprog_t program,
				const rpcvers_t version,
				const unsigned short protocol);

/*
 * Generic function that maps an RPC service tuple to an IP port
 * number of the service on a remote host
 */
extern unsigned short	nfs_getport(const struct sockaddr *,
				const socklen_t, const rpcprog_t,
				const rpcvers_t, const unsigned short);

/*
 * Generic function that maps an RPC service tuple to an IP port
 * number of the service on the local host
 */
extern unsigned short	nfs_getlocalport(const rpcprot_t,
				const rpcvers_t, const unsigned short);

/*
 * Function to invoke an rpcbind v3/v4 GETADDR request
 */
extern unsigned short	nfs_rpcb_getaddr(const struct sockaddr *,
				const socklen_t,
				const unsigned short,
				const struct sockaddr *,
				const socklen_t,
				const rpcprog_t,
				const rpcvers_t,
				const unsigned short,
				const struct timeval *);

/*
 * Function to invoke a portmap GETPORT request
 */
extern unsigned long	nfs_pmap_getport(const struct sockaddr_in *,
				const unsigned short,
				const unsigned long,
				const unsigned long,
				const unsigned long,
				const struct timeval *);

/*
 * Contact a remote RPC service to discover whether it is responding
 * to requests.
 */
extern int		nfs_rpc_ping(const struct sockaddr *sap,
				const socklen_t salen,
				const rpcprog_t program,
				const rpcvers_t version,
				const unsigned short protocol,
				const struct timeval *timeout);

#endif	/* __NFS_UTILS_NFSRPC_H */
