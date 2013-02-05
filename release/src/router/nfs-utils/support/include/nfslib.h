/*
 * support/include/nfslib.h
 *
 * General support functions for NFS user-space programs.
 *
 * Copyright (C) 1995 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef NFSLIB_H
#define NFSLIB_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <paths.h>
#include <rpcsvc/nfs_prot.h>
#include <nfs/nfs.h>
#include "xlog.h"

#ifndef _PATH_EXPORTS
#define _PATH_EXPORTS		"/etc/exports"
#endif
#ifndef _PATH_IDMAPDCONF
#define _PATH_IDMAPDCONF	"/etc/idmapd.conf"
#endif
#ifndef _PATH_XTAB
#define _PATH_XTAB		NFS_STATEDIR "/xtab"
#endif
#ifndef _PATH_XTABTMP
#define _PATH_XTABTMP		NFS_STATEDIR "/xtab.tmp"
#endif
#ifndef _PATH_XTABLCK
#define _PATH_XTABLCK		NFS_STATEDIR "/.xtab.lock"
#endif
#ifndef _PATH_ETAB
#define _PATH_ETAB		NFS_STATEDIR "/etab"
#endif
#ifndef _PATH_ETABTMP
#define _PATH_ETABTMP		NFS_STATEDIR "/etab.tmp"
#endif
#ifndef _PATH_ETABLCK
#define _PATH_ETABLCK		NFS_STATEDIR "/.etab.lock"
#endif
#ifndef _PATH_RMTAB
#define _PATH_RMTAB		NFS_STATEDIR "/rmtab"
#endif
#ifndef _PATH_RMTABTMP
#define _PATH_RMTABTMP		_PATH_RMTAB ".tmp"
#endif
#ifndef _PATH_RMTABLCK
#define _PATH_RMTABLCK		NFS_STATEDIR "/.rmtab.lock"
#endif
#ifndef _PATH_PROC_EXPORTS
#define	_PATH_PROC_EXPORTS	"/proc/fs/nfs/exports"
#define	_PATH_PROC_EXPORTS_ALT	"/proc/fs/nfsd/exports"
#endif

/* Maximum number of security flavors on an export: */
#define SECFLAVOR_COUNT 8

struct sec_entry {
	struct flav_info *flav;
	int flags;
};

/*
 * Data related to a single exports entry as returned by getexportent.
 * FIXME: export options should probably be parsed at a later time to
 * allow overrides when using exportfs.
 */
struct exportent {
	char *		e_hostname;
	char		e_path[NFS_MAXPATHLEN+1];
	int		e_flags;
	int		e_anonuid;
	int		e_anongid;
	int *		e_squids;
	int		e_nsquids;
	int *		e_sqgids;
	int		e_nsqgids;
	int		e_fsid;
	char *		e_mountpoint;
	int             e_fslocmethod;
	char *          e_fslocdata;
	char *		e_uuid;
	struct sec_entry e_secinfo[SECFLAVOR_COUNT+1];
};

struct rmtabent {
	char		r_client[NFSCLNT_IDMAX+1];
	char		r_path[NFS_MAXPATHLEN+1];
	int		r_count;
};

/*
 * configuration file parsing
 */
void			setexportent(char *fname, char *type);
struct exportent *	getexportent(int,int);
void 			secinfo_show(FILE *fp, struct exportent *ep);
void			putexportent(struct exportent *xep);
void			endexportent(void);
struct exportent *	mkexportent(char *hname, char *path, char *opts);
void			dupexportent(struct exportent *dst,
					struct exportent *src);
int			updateexportent(struct exportent *eep, char *options);

int			setrmtabent(char *type);
struct rmtabent *	getrmtabent(int log, long *pos);
void			putrmtabent(struct rmtabent *xep, long *pos);
void			endrmtabent(void);
void			rewindrmtabent(void);
FILE *			fsetrmtabent(char *fname, char *type);
struct rmtabent *	fgetrmtabent(FILE *fp, int log, long *pos);
void			fputrmtabent(FILE *fp, struct rmtabent *xep, long *pos);
void			fendrmtabent(FILE *fp);
void			frewindrmtabent(FILE *fp);

/*
 * wildmat borrowed from INN
 */
int			wildmat(char *text, char *pattern);

/*
 * nfsd library functions.
 */
int			nfsctl(int, struct nfsctl_arg *, union nfsctl_res *);
int			nfssvc(int port, int nrservs, unsigned int versbits, unsigned int portbits, char *haddr);
int			nfsaddclient(struct nfsctl_client *clp);
int			nfsdelclient(struct nfsctl_client *clp);
int			nfsexport(struct nfsctl_export *exp);
int			nfsunexport(struct nfsctl_export *exp);
struct nfs_fh_len *	getfh_old(struct sockaddr *addr, dev_t dev, ino_t ino);
struct nfs_fh_len *	getfh(struct sockaddr *addr, const char *);
struct nfs_fh_len *	getfh_size(struct sockaddr *addr, const char *, int size);

void qword_print(FILE *f, char *str);
void qword_printhex(FILE *f, char *str, int slen);
void qword_printint(FILE *f, int num);
int qword_eol(FILE *f);
int readline(int fd, char **buf, int *lenp);
int qword_get(char **bpp, char *dest, int bufsize);
int qword_get_int(char **bpp, int *anint);
void cache_flush(int force);
int check_new_cache(void);
void qword_add(char **bpp, int *lp, char *str);
void qword_addhex(char **bpp, int *lp, char *buf, int blen);
void qword_addint(char **bpp, int *lp, int n);
void qword_adduint(char **bpp, int *lp, unsigned int n);
void qword_addeol(char **bpp, int *lp);

void closeall(int min);

int			svctcp_socket (u_long __number, int __reuse);
int			svcudp_socket (u_long __number, int __reuse);

#endif /* NFSLIB_H */
