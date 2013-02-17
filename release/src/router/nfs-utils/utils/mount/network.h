/*
 * network.h -- Provide common network functions for NFS mount/umount
 *
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2007 Chuck Lever <chuck.lever@oracle.com>
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

#ifndef _NFS_UTILS_MOUNT_NETWORK_H
#define _NFS_UTILS_MOUNT_NETWORK_H

#include <rpc/pmap_prot.h>

#define MNT_SENDBUFSIZE (2048U)
#define MNT_RECVBUFSIZE (1024U)

typedef struct {
	char **hostname;
	struct sockaddr_in saddr;
	struct pmap pmap;
} clnt_addr_t;

/* RPC call timeout values */
static const struct timeval TIMEOUT = { 20, 0 };
static const struct timeval RETRY_TIMEOUT = { 3, 0 };

int probe_bothports(clnt_addr_t *, clnt_addr_t *);
int nfs_probe_bothports(const struct sockaddr *, const socklen_t,
			struct pmap *, const struct sockaddr *,
			const socklen_t, struct pmap *);
int nfs_gethostbyname(const char *, struct sockaddr_in *);
int nfs_name_to_address(const char *, const sa_family_t,
		struct sockaddr *, socklen_t *);
int nfs_string_to_sockaddr(const char *, const size_t,
			   struct sockaddr *, socklen_t *);
int nfs_present_sockaddr(const struct sockaddr *,
			 const socklen_t, char *, const size_t);
int nfs_callback_address(const struct sockaddr *, const socklen_t,
		struct sockaddr *, socklen_t *);
int clnt_ping(struct sockaddr_in *, const unsigned long,
		const unsigned long, const unsigned int,
		struct sockaddr_in *);

struct mount_options;

void nfs_options2pmap(struct mount_options *,
		      struct pmap *, struct pmap *);

int start_statd(void);

unsigned long nfsvers_to_mnt(const unsigned long);

int nfs_call_umount(clnt_addr_t *, dirpath *);
int nfs_advise_umount(const struct sockaddr *, const socklen_t,
		      const struct pmap *, const dirpath *);
CLIENT *mnt_openclnt(clnt_addr_t *, int *);
void mnt_closeclnt(CLIENT *, int);

#endif	/* _NFS_UTILS_MOUNT_NETWORK_H */
