/*
 * utils/mountd/mountd.h
 *
 * Declarations for mountd.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#ifndef MOUNTD_H
#define MOUNTD_H

#include <rpc/rpc.h>
#include <rpc/svc.h>
#include "nfslib.h"
#include "exportfs.h"
#include "mount.h"

union mountd_arguments {
	dirpath			dirpath;
};

union mountd_results {
	fhstatus		fstatus;
	mountlist		mountlist;
	exports			exports;
};

/*
 * Global Function prototypes.
 */
bool_t		mount_null_1_svc(struct svc_req *, void *, void *);
bool_t		mount_mnt_1_svc(struct svc_req *, dirpath *, fhstatus *);
bool_t		mount_dump_1_svc(struct svc_req *, void *, mountlist *);
bool_t		mount_umnt_1_svc(struct svc_req *, dirpath *, void *);
bool_t		mount_umntall_1_svc(struct svc_req *, void *, void *);
bool_t		mount_export_1_svc(struct svc_req *, void *, exports *);
bool_t		mount_exportall_1_svc(struct svc_req *, void *, exports *);
bool_t		mount_pathconf_2_svc(struct svc_req *, dirpath *, ppathcnf *);
bool_t		mount_mnt_3_svc(struct svc_req *, dirpath *, mountres3 *);

void		mount_dispatch(struct svc_req *, SVCXPRT *);
void		auth_init(char *export_file);
unsigned int	auth_reload(void);
nfs_export *	auth_authenticate(char *what, struct sockaddr_in *sin,
					char *path);
void		auth_export(nfs_export *exp);

void		mountlist_add(char *host, const char *path);
void		mountlist_del(char *host, const char *path);
void		mountlist_del_all(struct sockaddr_in *sin);
mountlist	mountlist_list(void);


#endif /* MOUNTD_H */
