/*
 * mount_dispatch	This file contains the function dispatch table.
 *
 * Copyright (C) 1995 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_TCP_WRAPPER
#include "tcpwrapper.h"
#endif

#include "mountd.h"
#include "rpcmisc.h"

/*
 * Procedures for MNTv1
 */
static struct rpc_dentry mnt_1_dtable[] = {
	dtable_ent(mount_null,1,void,void),		/* NULL */
	dtable_ent(mount_mnt,1,dirpath,fhstatus),	/* MNT */
	dtable_ent(mount_dump,1,void,mountlist),	/* DUMP */
	dtable_ent(mount_umnt,1,dirpath,void),		/* UMNT */
	dtable_ent(mount_umntall,1,void,void),		/* UMNTALL */
	dtable_ent(mount_export,1,void,exports),	/* EXPORT */
	dtable_ent(mount_exportall,1,void,exports),	/* EXPORTALL */
};

/*
 * Procedures for MNTv2
 */
static struct rpc_dentry mnt_2_dtable[] = {
	dtable_ent(mount_null,1,void,void),		/* NULL */
	dtable_ent(mount_mnt,1,dirpath,fhstatus),	/* MNT */
	dtable_ent(mount_dump,1,void,mountlist),	/* DUMP */
	dtable_ent(mount_umnt,1,dirpath,void),		/* UMNT */
	dtable_ent(mount_umntall,1,void,void),		/* UMNTALL */
	dtable_ent(mount_export,1,void,exports),	/* EXPORT */
	dtable_ent(mount_exportall,1,void,exports),	/* EXPORTALL */
	dtable_ent(mount_pathconf,2,dirpath,ppathcnf),	/* PATHCONF */
};

/*
 * Procedures for MNTv3
 */
static struct rpc_dentry mnt_3_dtable[] = {
	dtable_ent(mount_null,1,void,void),		/* NULL */
	dtable_ent(mount_mnt,3,dirpath,mountres3),	/* MNT */
	dtable_ent(mount_dump,1,void,mountlist),	/* DUMP */
	dtable_ent(mount_umnt,1,dirpath,void),		/* UMNT */
	dtable_ent(mount_umntall,1,void,void),		/* UMNTALL */
	dtable_ent(mount_export,1,void,exports),	/* EXPORT */
};

#define number_of(x)	(sizeof(x)/sizeof(x[0]))

static struct rpc_dtable	dtable[] = {
	{ mnt_1_dtable,		number_of(mnt_1_dtable) },
	{ mnt_2_dtable,		number_of(mnt_2_dtable) },
	{ mnt_3_dtable,		number_of(mnt_3_dtable) },
};

/*
 * The main dispatch routine.
 */
void
mount_dispatch(struct svc_req *rqstp, SVCXPRT *transp)
{
	union mountd_arguments 	argument;
	union mountd_results	result;
#ifdef HAVE_TCP_WRAPPER
	struct sockaddr_in *sin = nfs_getrpccaller_in(transp);

	/* remote host authorization check */
	if (sin->sin_family == AF_INET &&
	    !check_default("mountd", sin, rqstp->rq_proc, MOUNTPROG)) {
		svcerr_auth (transp, AUTH_FAILED);
		return;
	}
#endif

	rpc_dispatch(rqstp, transp, dtable, number_of(dtable),
			&argument, &result);
}
