/*
 * support/nfs/client.c
 *
 * Add or delete an NFS client in knfsd.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "nfslib.h"

int
nfsaddclient(struct nfsctl_client *clp)
{
	struct nfsctl_arg	arg;

	arg.ca_version = NFSCTL_VERSION;
	memcpy(&arg.ca_client, clp, sizeof(arg.ca_client));
	return nfsctl(NFSCTL_ADDCLIENT, &arg, NULL);
}

int
nfsdelclient(struct nfsctl_client *clp)
{
	struct nfsctl_arg	arg;

	arg.ca_version = NFSCTL_VERSION;
	memcpy(&arg.ca_client, clp, sizeof(arg.ca_client));
	return nfsctl(NFSCTL_DELCLIENT, &arg, NULL);
}
