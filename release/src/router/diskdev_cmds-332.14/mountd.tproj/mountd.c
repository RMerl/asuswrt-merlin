/*
 * Copyright (c) 1999-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pathnames.h	8.1 (Berkeley) 6/5/93
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/wait.h>
#include <sys/queue.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

#include <stdarg.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

#define EXPORT_FROM_NETINFO 0	/* export list from NetInfo */
#define EXPORT_FROM_FILE 1	/* export list from file only */
#define EXPORT_FROM_FILEFIRST 2	/* export list from file first, then NetInfo */
static int source = EXPORT_FROM_FILEFIRST;

#include <netinfo/ni.h>

/*
 * Structure for maintaining list of export IDs for exported volumes
 */
struct expidlist {
	LIST_ENTRY(expidlist)	xid_list;
	char			xid_path[MAXPATHLEN];	/* exported sub-directory */
	u_int32_t		xid_id;			/* export ID */
};

/*
 * Structure for maintaining list of UUIDs for exported volumes
 */
struct uuidlist {
	TAILQ_ENTRY(uuidlist)		ul_list;
	char				ul_mntfromname[MAXPATHLEN];
	char				ul_mntonname[MAXPATHLEN];
	u_char				ul_uuid[16];	/* UUID used */
	u_char				ul_dauuid[16];	/* DiskArb UUID */
	char				ul_davalid;	/* DiskArb UUID valid */
	char				ul_exported;	/* currently exported? */
	u_int32_t			ul_fsid;	/* exported FS ID */
	LIST_HEAD(expidhead,expidlist)	ul_exportids;	/* export ID list */
};
TAILQ_HEAD(,uuidlist) ulhead;
#define UL_CHECK_MNTFROM	0x1
#define UL_CHECK_MNTON		0x2
#define UL_CHECK_ALL		0x3

/*
 * Default FSID is just a "hash" of UUID
 */
#define UUID2FSID(U) \
	(*((u_int32_t*)(U))     ^ *(((u_int32_t*)(U))+1) ^ \
	 *(((u_int32_t*)(U))+2) ^ *(((u_int32_t*)(U))+3))

/*
 * Structure for keeping the (outstanding) mount list
 */
struct mountlist {
	struct mountlist	*ml_next;
	char			*ml_host;	/* NFS client name or address */
	char			*ml_dir;	/* mounted directory */
};

/*
 * Structure used to hold a list of directories
 */
struct dirlist {
	struct dirlist		*dl_next;
	char			*dl_dir;
};

/*
 * Structures for keeping the export lists
 */

TAILQ_HEAD(expfstqh, expfs);
TAILQ_HEAD(expdirtqh, expdir);

/*
 * Structure to hold the exports for each exported file system.
 */
struct expfs {
	TAILQ_ENTRY(expfs)	xf_next;
	struct expdirtqh	xf_dirl;	/* list of exported directories */
	int			xf_flag;	/* internal flags for this struct */
	u_char			xf_uuid[16];	/* file system's UUID */
	char			*xf_fsdir;	/* mount point of this file system */
};
/* xf_flag bits */
#define	XF_LINKED	0x1

/*
 * Structure to hold info about an exported directory
 */
struct expdir {
	TAILQ_ENTRY(expdir)	xd_next;
	struct host		*xd_hosts;	/* List of hosts this dir exported to */
	struct expdirtqh	xd_mountdirs;	/* list of mountable sub-directories */
	int			xd_flags;	/* default export flags */
	char			*xd_dir;	/* pathname of exported directory */
};

/*
 * Structures for holding sets of exported-to hosts/nets/addresses
 */

/* holds a network/mask and name */
struct netmsk {
	u_long	nt_net;		/* network address */
	u_long	nt_mask;	/* network mask */
	char 	*nt_name;	/* network name */
};

/* holds either a host or network */
union grouptypes {
	struct hostent *gt_hostent;
	struct netmsk	gt_net;
#ifdef ISO
	struct sockaddr_iso *gt_isoaddr;
#endif
};

/* host/network list entry */
struct grouplist {
	int gr_type;			/* type of group */
	union grouptypes gr_u;		/* the host/network */
	struct grouplist *gr_next;
};
/* Group types */
#define	GT_NULL		0x0		/* not fully-initialized yet */
#define	GT_HOST		0x1		/* this is a single host address */
#define	GT_NET		0x2		/* this is a network */
#define	GT_ISO		0x4

/*
 * host/network flags list entry
 */
struct host {
	int		 ht_flag;	/* export options for these hosts */
	struct grouplist *ht_grp;	/* host/network flags applies to */
	struct host	 *ht_next;
};

#define DEFAULTHOSTNAME "localhost"

struct fhreturn {
	int		fhr_flag;
	int		fhr_vers;
	fhandle_t	fhr_fh;
};

/* Global defs */
int	add_dir(struct dirlist **, char *);
char *	add_expdir(struct expdir **, char *, int);
void	add_mlist(char *, char *);
int	check_dirpath(char *);
int	check_options(int);
void	del_mlist(char *, char *);
int	expdir_search(struct expfs *, char *, u_long, int *);
int	do_export(struct expfs *, struct grouplist *, int,
		struct xucred *, char *, struct statfs *, struct uuidlist *);
int	do_opt(char **, char **, struct grouplist *, int *,
		int *, int *, struct xucred *);
struct expfs *ex_search(u_char *);
struct host *find_host(struct host *, u_long);
void	free_dirlist(struct dirlist *dl);
void	free_expdir(struct expdir *);
void	free_expfs(struct expfs *);
void	free_grp(struct grouplist *);
void	free_host(struct host *);
struct expdir *get_expdir(void);
struct expfs *get_expfs(void);
void	get_hostnames(char **hostnamearray);
char *	get_ifinfo(int, int, int *);
void	get_exportlist(void);
int	get_host_addresses(char *, struct grouplist *);
struct host *get_host(void);
int	get_line(int);
int	ni_get_line(void);
int	file_get_line(void);
void	get_mountlist(void);
int	get_net(char *, struct netmsk *, int);
void	getexp_err(struct expfs *, struct grouplist *);
struct grouplist *get_grp(void);
int	hang_options(struct expdir *, int, struct grouplist *);
int	hang_options_mountdir(struct expdir *, char *, int, struct grouplist *);
void	mntsrv(struct svc_req *, SVCXPRT *);
void	my_svc_run(void);
void	nextfield(char **, char **);
void	out_of_mem(char *);
void	parsecred(char *, struct xucred *);
int	put_exlist(struct expdir *, XDR *);
int	register_export(const char *, char * const *, int);
void	sigmux(int);
int	xdr_dir(XDR *, char *);
int	xdr_explist(XDR *, caddr_t);
int	xdr_fhs(XDR *, caddr_t);
int	xdr_mlist(XDR *, caddr_t);

int	get_uuid_from_diskarb(const char *, u_char *);
struct uuidlist * get_uuid_from_list(const struct statfs *, u_char *, const int);
struct uuidlist * add_uuid_to_list(const struct statfs *, u_char *, u_char *);
struct uuidlist * get_uuid(const struct statfs *, u_char *);
struct uuidlist * find_uuid(u_char *);
struct uuidlist * find_uuid_by_fsid(u_int32_t);
void	uuidlist_clearexport(void);
char *	uuidstring(u_char *, char *);
void	uuidlist_save(void);
void	uuidlist_restore(void);

struct expidlist * find_export_id(struct uuidlist *, u_int32_t);
struct expidlist * get_export_id(struct uuidlist *, u_char *);

/* C library */
int	getnetgrent(char **host, char **user, char **domain);
void	endnetgrent(void);
void	setnetgrent(const char *netgroup);

#ifdef ISO
struct iso_addr *iso_addr(void);
#endif

void ni_exports_open(void);
void ni_exports_close(void);

struct expfstqh xfshead;
struct mountlist *mlhead;
struct grouplist *grphead;
char exportsfilepath[MAXPATHLEN];
struct xucred def_anon = {
	XUCRED_VERSION,
	(uid_t) -2,
	1,
	{ (gid_t) -2 },
};
int resvport_only = 1;
int dir_only = 1;

/* export options */
#define	OP_MAPROOT	0x00000001	/* map root credentials */
#define	OP_MAPALL	0x00000002	/* map all credentials */
#define	OP_KERB		0x00000004	/* use Kerberos */
#define	OP_MASK		0x00000008	/* network mask specified */
#define	OP_NET		0x00000010	/* network address specified */
#define	OP_ISO		0x00000020	/* ISO address specified */
#define	OP_ALLDIRS	0x00000040	/* allow mounting subdirs */
#define	OP_READONLY	0x00000080	/* export read-only */
#define	OP_32BITCLIENTS	0x00000100	/* use 32-bit directory cookies */
#define	OP_DEFEXP	0x40000000	/* default export for everyone (else) */
#define	OP_EXOPTMASK	0x000000C7	/* export options mask */

#define MAXHOSTNAMES 10
char *our_hostnames[MAXHOSTNAMES];	/* for system we are running on */

static char urlprefix[] = "nfs://";
static char slpprefix[] = "service:x-file-service:nfs://";
static int maxprefix = (sizeof(slpprefix) > sizeof(urlprefix) ?
			sizeof(slpprefix) : sizeof(urlprefix));

static char URLRegistrar[] = _PATH_SLP_REG;

#define ADD_URL	1
#define DELETE_URL	0

int debug = 0;
void SYSLOG(int, const char *, ...);
#define log SYSLOG

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * and "-n" to allow nonroot mount.
 */
int
main(int argc, char *argv[])
{
	SVCXPRT *udptransp, *tcptransp;
	int c, i;

	while ((c = getopt(argc, argv, "Ddfnr")) != EOF) {
		switch (c) {
			case 'D':
				debug = 1;
				break;
			case 'd':
				debug = 2;
				fprintf(stderr, "Debug Enabled.  Logging to stderr.\n");
				break;
			case 'f':
				source = EXPORT_FROM_FILE;
				break;
			case 'n':
				resvport_only = 0;
				break;
			case 'r':
				dir_only = 0;
				break;
			default:
				fprintf(stderr, "Usage: mountd [-d] [-r] [-n] [-f] [export_file]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;
	TAILQ_INIT(&xfshead);
	mlhead = NULL;
	grphead = NULL;
	TAILQ_INIT(&ulhead);

	for (i = 0; i < MAXHOSTNAMES; ++i)
		our_hostnames[i] = NULL;
	if (argc == 1) {
		source = EXPORT_FROM_FILE;
		strncpy(exportsfilepath, *argv, MAXPATHLEN-1);
		exportsfilepath[MAXPATHLEN-1] = '\0';
	} else
		strcpy(exportsfilepath, _PATH_EXPORTS);
	openlog("mountd", LOG_PID, LOG_DAEMON);
	if (debug < 2)
		log(LOG_DEBUG, "Debug Logging Enabled.");
	uuidlist_restore();
	log(LOG_DEBUG, "Getting export list.");
	get_exportlist();
	log(LOG_DEBUG, "Getting mount list.");
	get_mountlist();
	log(LOG_DEBUG, "Here we go.");
	if (debug < 2) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGHUP, sigmux);
	signal(SIGTERM, sigmux);
	{ FILE *pidfile = fopen(_PATH_MOUNTDPID, "w");
	  if (pidfile != NULL) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	  }
	}
	if ((udptransp = svcudp_create(RPC_ANYSOCK)) == NULL ||
	    (tcptransp = svctcp_create(RPC_ANYSOCK, 0, 0)) == NULL) {
		log(LOG_ERR, "Can't create socket");
		exit(1);
	}
	pmap_unset(RPCPROG_MNT, 1);
	pmap_unset(RPCPROG_MNT, 3);
	if (!svc_register(udptransp, RPCPROG_MNT, 1, mntsrv, IPPROTO_UDP) ||
	    !svc_register(udptransp, RPCPROG_MNT, 3, mntsrv, IPPROTO_UDP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, 1, mntsrv, IPPROTO_TCP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, 3, mntsrv, IPPROTO_TCP)) {
		log(LOG_ERR, "Can't register mount");
		exit(1);
	}
	my_svc_run();
	log(LOG_ERR, "Mountd died");
	exit(1);
}

volatile static int gothup;
volatile static int gotterm;

void
sigmux(int sig)
{

	switch (sig) {
	case SIGHUP:
		gothup = 1;
		break;
	case SIGTERM:
		gotterm = 1;
		break;
	}
}

void
my_svc_run(void)
{
	fd_set	readfdset;
	static int tsize = 0;
	int x;

	if (tsize == 0)
		tsize = getdtablesize();

	for ( ; ; ) {
		bcopy(&svc_fdset, &readfdset, sizeof(svc_fdset));
		x = select(tsize, &readfdset, NULL, NULL, NULL);
		if (x > 0) {
			svc_getreqset(&readfdset);
		} else if (x == -1) {
			switch (errno) {
			case EINTR:
				if (gotterm) {
					gotterm = 0;
					exit(0);
				}
				if (gothup) {
					gothup = 0;
					get_exportlist();
				}
			}
		}
	}
}

/*
 * The mount rpc service
 */
void
mntsrv( struct svc_req *rqstp,
	SVCXPRT *transp)
{
	struct expfs *xf;
	struct fhreturn fhr;
	struct stat stb;
	struct statfs fsb;
	struct hostent *hp;
	u_long saddr;
	u_short sport;
	char rpcpath[RPCMNT_PATHLEN + 1], dirpath[MAXPATHLEN], *mhost;
	int bad = ENOENT, options;
	sigset_t sighup_mask;
	u_char uuid[16];

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	saddr = transp->xp_raddr.sin_addr.s_addr;
	sport = ntohs(transp->xp_raddr.sin_port);
	hp = NULL;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send NULL reply");
		return;
	case RPCMNT_MOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			svcerr_decode(transp);
			return;
		}

		/*
		 * Get the real pathname and make sure it is a directory
		 * or a regular file if the -r option was specified
		 * and it exists.
		 */
		if (realpath(rpcpath, dirpath) == 0 ||
		    stat(dirpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) &&
		     (dir_only || !S_ISREG(stb.st_mode))) ||
		    statfs(dirpath, &fsb) < 0) {
			chdir("/");	/* Just in case realpath doesn't */
			log(LOG_DEBUG, "stat failed on %s", dirpath);
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
			return;
		}

		/* get UUID for volume */
		if (!get_uuid_from_list(&fsb, uuid, UL_CHECK_ALL)) {
			log(LOG_DEBUG, "no exported volume uuid for %s", dirpath);
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
			return;
		}

		/* Check in the exports list */
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		xf = ex_search(uuid);
		if (xf && expdir_search(xf, dirpath, saddr, &options)) {
			fhr.fhr_flag = options;
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(fhandle_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				log(LOG_DEBUG, "Can't get fh for %s: %s (%d)", dirpath,
					strerror(errno), errno);
				bad = EACCES; /* path must not be exported */
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
					log(LOG_ERR, "Can't send reply for failed mount");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (!svc_sendreply(transp, (xdrproc_t)xdr_fhs, (caddr_t)&fhr))
				log(LOG_ERR, "Can't send mount reply");
			hp = gethostbyaddr((caddr_t)&transp->xp_raddr.sin_addr,
				sizeof(transp->xp_raddr.sin_addr), AF_INET);
			mhost = hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr);
			add_mlist(mhost, dirpath);
			log(LOG_DEBUG, "Mount successful: %s %s", mhost, dirpath);
		} else {
			bad = EACCES;
			if (debug) {
				mhost = inet_ntoa(transp->xp_raddr.sin_addr);
				log(LOG_DEBUG, "Mount failed: %s %s", mhost, dirpath);
			}
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
		}
		sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case RPCMNT_DUMP:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_mlist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send dump reply");
		return;
	case RPCMNT_UMOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, dirpath)) {
			svcerr_decode(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send umount reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, dirpath);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), dirpath);
		return;
	case RPCMNT_UMNTALL:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send umntall reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, (char *)NULL);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), (char *)NULL);
		return;
	case RPCMNT_EXPORT:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_explist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send export reply");
		return;
	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dirpath string
 */
int
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
int
xdr_fhs(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	register struct fhreturn *fhrp = (struct fhreturn *)cp;
	long ok = 0, len, auth;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)fhrp->fhr_fh.fh_data, NFSX_V2FH));
	case 3:
		len = fhrp->fhr_fh.fh_len;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)fhrp->fhr_fh.fh_data, fhrp->fhr_fh.fh_len))
			return (0);
		if (fhrp->fhr_flag & OP_KERB)
			auth = RPCAUTH_KERB4;
		else
			auth = RPCAUTH_UNIX;
		len = 1;
		if (!xdr_long(xdrsp, &len))
			return (0);
		return (xdr_long(xdrsp, &auth));
	};
	return (0);
}

int
xdr_mlist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct mountlist *mlp;
	int trueval = 1;
	int falseval = 0;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &trueval))
			return (0);
		if (!xdr_string(xdrsp, &mlp->ml_host, RPCMNT_NAMELEN))
			return (0);
		if (!xdr_string(xdrsp, &mlp->ml_dir, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
int
xdr_explist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct expfs *xf;
	struct expdir *xd;
	int falseval = 0;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
	TAILQ_FOREACH_REVERSE(xf, &xfshead, xf_next, expfstqh) {
		TAILQ_FOREACH_REVERSE(xd, &xf->xf_dirl, xd_next, expdirtqh) {
			if (put_exlist(xd, xdrsp))
				goto errout;
		}
	}
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
errout:
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to output the mountable exported
 * directory paths.
 */
int
put_exlist(struct expdir *xd, XDR *xdrsp)
{
	struct expdir *mxd;
	struct grouplist *grp;
	struct host *hp;
	int trueval = 1;
	int falseval = 0;
	char *strp;

	if (!xd)
		return (0);

	if (!xdr_bool(xdrsp, &trueval))
		return (1);
	strp = xd->xd_dir;
	if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
		return (1);
	if (!(xd->xd_flags & OP_DEFEXP)) {
		hp = xd->xd_hosts;
		while (hp) {
			grp = hp->ht_grp;
			if (grp->gr_type == GT_HOST) {
				if (!xdr_bool(xdrsp, &trueval))
					return (1);
				strp = grp->gr_u.gt_hostent->h_name;
				if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
					return (1);
			} else if (grp->gr_type == GT_NET) {
				if (!xdr_bool(xdrsp, &trueval))
					return (1);
				strp = grp->gr_u.gt_net.nt_name;
				if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
					return (1);
			}
			hp = hp->ht_next;
		}
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (1);

	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		if (put_exlist(mxd, xdrsp))
			return (1);
	}

	return (0);
}


/*
 * Snitched from unpv12e (R. W. Stevens)
 * Fill in an array of sockaddr structs,
 *  based on a bit array indicating presence.
 */
void
get_rtinfo(int addrs,
	   struct sockaddr *sa,
	   struct sockaddr **rti_info)
{
	register int i;

/*
 * Round up 'a' to next multiple of 'size'
 */
#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

/*
 * Step to next socket address structure;
 * if sa_len is 0, assume it is sizeof(u_long).
 */
#define NEXT_SA(ap)	ap = (struct sockaddr *) \
		((caddr_t) ap + (ap->sa_len ? \
				 ROUNDUP(ap->sa_len, sizeof (u_long)) : \
				 sizeof(u_long)))

	for (i = 0; i < RTAX_MAX; i++)
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		} else
			rti_info[i] = NULL;
}


/*
 * Use sysctl() to get the info on interface-related
 *  information.  We can look for a specific device, or
 *  a specific protocol family.
 */
char *
get_ifinfo(int family, int index, int *buflen)
{
	int mib[6];
	char *buf;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;	/* PF_ maybe */
	mib[2] = 0;		/* XXX */
	mib[3] = family;	/* Target family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = index;		/* either a specific link or 0 */

	/* Determine how much space is needed */
	if (sysctl(mib, 6, NULL, (size_t *)buflen, NULL, 0) == -1) {
		log(LOG_ERR,
		    "Error requesting network interface info size: errno = %d",
		    errno);
		return (NULL);
	}

	/* ... allocate space, */
	buf = (char *)malloc(*buflen);
	if (buf == NULL) {
		log(LOG_ERR,
		    "Error allocating %d bytes for network interface into",
		    buflen);
		return (NULL);
	}

	/* ... and then get a buffer full */
	if (sysctl(mib, 6, buf, (size_t *)buflen, NULL, 0) == -1) {
		free(buf);
		log(LOG_ERR,
		    "Error requesting network interface info: errno = %d",
		    errno);
		return (NULL);
	}

	return (buf);
}



/*
 * Add a host name to the global table if it's not already in the table:
 */

void
record_hostname(const char *hostname, char **hostnamearray)
{
	int i, hostnamelen;

	hostnamelen = strlen(hostname);
	if (hostnamelen > MAXHOSTNAMELEN)
		return;
	for (i = 0; i < MAXHOSTNAMES && hostnamearray[i]; i++)
		if (!strcmp(hostnamearray[i], hostname))
			return;
	if (i >= MAXHOSTNAMES)
		return;
	hostnamearray[i] = malloc(hostnamelen + 1);
	if (hostnamearray[i] == NULL) {
		log(LOG_ERR, "record_hostname: couldn't allocate memory for hostname '%s'.",
			hostname);
		return;
	}
	strcpy(hostnamearray[i], hostname);
	log(LOG_DEBUG, "record_hostname: hostnamearray[%d] = '%s'.",
		i, hostnamearray[i]);
}



/*
 * Get the list of host names:
 */
void
get_hostnames(char **hostnamearray)
{
	char *ifinfo;
	int ifinfo_len;
	int i;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	int if_flags = 0;
	struct hostent *hostinfo;
	struct sockaddr_in *sain;
	
	ifinfo = get_ifinfo(AF_INET, 0, &ifinfo_len);
	if (ifinfo == NULL) {
		log(LOG_ERR, "get_hostnames: no ifinfo");
		return;
	}

	/* Release any currently allocated host name strings: */
	for (i = 0; (i < MAXHOSTNAMES) && (hostnamearray[i] != NULL); ++i) {
		free(hostnamearray[i]);
		hostnamearray[i] = NULL;
	}
	
	/*
	 * Process info from the retrieved interface information
	 * We're looking for 'if' info and associated socketaddrs
	 * that describe assigned protocol addresses
	 * From Stevens' Unix Network Programming, V1, 2nd Ed.
	 * Ch. 17 (esp. 17.4).
	 * We borrowed a bit of code, as indicated.
	 */
	/* 'ifinfo' contains a batch of 'ifm' and 'ifam' structs */
	for (ifm = (struct if_msghdr *)ifinfo; (char *)ifm < ifinfo+ifinfo_len;
	     ifm = (struct if_msghdr *)((char *)ifm + ifm->ifm_msglen)) {
		/*
		 * We're interested in:
		 *  RTM_IFINFO (ifm structs describing an interface)
		 *  RTM_NEWADDR (ifam structs describing 'protocol' addresses
		 *   associated with the preceding ifm)
		 * Note that each IFINFO struct is followed by all of its
		 *  NEWADDR structs.
		 */
		if (ifm->ifm_type == RTM_IFINFO) {
			if_flags = ifm->ifm_flags; /* To check IFF_LOOPBACK */
			continue;
		}
		if (ifm->ifm_type != RTM_NEWADDR || (if_flags & IFF_LOOPBACK))
			continue;
		ifam = (struct ifa_msghdr *)ifm;
		sa = (struct sockaddr *)(ifam+1);
		get_rtinfo(ifam->ifam_addrs, sa, rti_info);
		sa = rti_info[RTAX_IFA];
		if (!sa)
			continue;
		if (sa->sa_family != AF_INET) {
			log(LOG_DEBUG, "unknown socket address family %d.",
				sa->sa_family);
			continue;
		}
		sain = (struct sockaddr_in *)sa;
		hostinfo = gethostbyaddr((caddr_t)&sain->sin_addr,
					 sizeof(sain->sin_addr), AF_INET);
		if (hostinfo) {
			record_hostname(hostinfo->h_name, hostnamearray);
		} else {
			record_hostname(inet_ntoa(sain->sin_addr), hostnamearray);
		};
	}
	free(ifinfo);	/* allocated by get_ifinfo */
}

/*
 * Strip escaped spaces and remove quotes around quoted strings.
 */
char *
clean_white_space(char *line)
{
	int len, esc;
	char c, *p, *s;

	if (line == NULL)
		return NULL;
	len = strlen(line);
	s = malloc(len + 1);
	if (s == NULL)
		return NULL;

	len = 0;
	esc = 0;
	c = '\0';

	p = line;

	if (*p == '\'' || *p == '"') {
		c = *p;
		p++;
	}

	for (;*p != '\0'; p++) {
		if (esc == 1) {
			s[len++] = *p;
			esc = 0;
		} else if (*p == c)
			break;
		else if (*p == '\\')
			esc = 1;
		else if (c == '\0' && (*p == ' ' || *p == '\t'))
			break;
		else s[len++] = *p;
	}

	s[len] = '\0';

	return (s);
}


int
safe_exec(char *pgm, char *arg1, char *arg2)
{
	int pid;
	union wait status;

	pid = fork();
	if (pid == 0) {
		(void) execl(pgm, pgm, arg1, arg2, NULL);
		log(LOG_ERR, "Exec of %s failed: %s (%d)", pgm,
			strerror(errno), errno);
		exit(2);
	}
	if (pid == -1) {
		log(LOG_ERR, "Fork for %s failed: %s (%d)", pgm,
			strerror(errno), errno);
		exit(2);
	}
	if (wait4(pid, (int *)&status, 0, NULL) != pid) {
		log(LOG_ERR, "BUG executing %s", pgm);
		exit(2);
	}
	if (!WIFEXITED(status)) {
		log(LOG_ERR, "%s aborted by signal %d", pgm, WTERMSIG(status));
		exit(2);
	}
	return (WEXITSTATUS(status));
}


/*
 * slp_reg returns this if you try to delete an unregistered URL
 * Better is to use "slp_reg -l" to enumerate actual registration, but that
 * flag is not yet implemented
 */
#define URLNOTFOUND	253

int
register_export(const char *path, char * const * hostnamearray, int addurl)
{
	int i;
	int urlstringlength = maxprefix + MAXHOSTNAMELEN + strlen(path);
	char *urlstring;
	int rv, result = 0;

	/* note any errors we return are to be nonfatal and already logged */

	urlstring = malloc(urlstringlength);
	if (urlstring == NULL) {
		log(LOG_ERR, "can't allocate url string to register export: %s",
			path);
		return (ENOMEM);
	}
	
	for (i = 0; (i < MAXHOSTNAMES) && (hostnamearray[i] != NULL); ++i) {
		if (urlstringlength < maxprefix + strlen(hostnamearray[i]) +
				      strlen(path)) {
			log(LOG_ERR, "huge hostname (ignored): %s",
			    hostnamearray[i]);
			result = ENAMETOOLONG;
			continue;
		}

		/* Register traditional URL: */
		strcpy(urlstring, urlprefix);
		strcat(urlstring, hostnamearray[i]);
		strcat(urlstring, path);
		log(LOG_DEBUG, "%sregistering URL %s",
			(addurl ? "" : "un"), urlstring);
		rv = safe_exec(URLRegistrar, (addurl ? "-r" : "-d"), urlstring);
		if (rv && (addurl || rv != URLNOTFOUND)) {
			log(LOG_ERR, "%s exit status %d", URLRegistrar, rv);
			result = rv; /* arbitrarily retaining last failure */
		}

		/* Register new-style URL: */
		strcpy(urlstring, slpprefix);
		strcat(urlstring, hostnamearray[i]);
		strcat(urlstring, path);
		log(LOG_DEBUG, "%sregistering URL %s",
			(addurl ? "" : "un"), urlstring);
		rv = safe_exec(URLRegistrar, (addurl ? "-r" : "-d"), urlstring);
		if (rv && (addurl || rv != URLNOTFOUND)) {
			log(LOG_ERR, "%s exit status %d", URLRegistrar, rv);
			result = rv; /* arbitrarily retaining last failure */
		}
	}
	free(urlstring);
	return (result);
}


/*
 * Query DiskArb for a volume's UUID
 */
int
get_uuid_from_diskarb(const char *path, u_char *uuid)
{
	DASessionRef session;
	DADiskRef disk;
	CFDictionaryRef dd;
	CFTypeRef val;
	CFUUIDBytes uuidbytes;
	int rv = 1;

	session = NULL;
	disk = NULL;
	dd = NULL;

	session = DASessionCreate(NULL);
	if (!session) {
		log(LOG_ERR, "can't create DiskArb session");
		rv = 0;
		goto out;
	}
	disk = DADiskCreateFromBSDName(NULL, session, path);
	if (!disk) {
		log(LOG_DEBUG, "DADiskCreateFromBSDName(%s) failed", path);
		rv = 0;
		goto out;
	}
	dd = DADiskCopyDescription(disk);
	if (!dd) {
		log(LOG_DEBUG, "DADiskCopyDescription(%s) failed", path);
		rv = 0;
		goto out;
	}

	if (!CFDictionaryGetValueIfPresent(dd, (kDADiskDescriptionVolumeUUIDKey), &val)) {
		log(LOG_DEBUG, "unable to get UUID for volume %s", path);
		rv = 0;
		goto out;
	}
	uuidbytes = CFUUIDGetUUIDBytes(val);
	bcopy(&uuidbytes, uuid, sizeof(uuidbytes));

out:
	if (session) CFRelease(session);
	if (disk) CFRelease(disk);
	if (dd) CFRelease(dd);
	return (rv);
}

/*
 * find the UUID for this volume in the UUID list
 */
struct uuidlist *
get_uuid_from_list(const struct statfs *fsb, u_char *uuid, const int flags)
{
	struct uuidlist *ulp;

	if (!(flags & UL_CHECK_ALL))
		return (NULL);

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if ((flags & UL_CHECK_MNTFROM) &&
		    strcmp(fsb->f_mntfromname, ulp->ul_mntfromname))
			continue;
		if ((flags & UL_CHECK_MNTON) &&
		    strcmp(fsb->f_mntonname, ulp->ul_mntonname))
			continue;
		if (uuid)
			bcopy(&ulp->ul_uuid, uuid, sizeof(ulp->ul_uuid));
		break;
	}
	return (ulp);
}

/*
 * find UUID list entry with the given UUID
 */
struct uuidlist *
find_uuid(u_char *uuid)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if (!bcmp(ulp->ul_uuid, uuid, sizeof(ulp->ul_uuid)))
			break;
	}
	return (ulp);
}

/*
 * find UUID list entry with the given FSID
 */
struct uuidlist *
find_uuid_by_fsid(u_int32_t fsid)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if (ulp->ul_fsid == fsid)
			break;
	}
	return (ulp);
}

/*
 * add a UUID to the UUID list
 */
struct uuidlist *
add_uuid_to_list(const struct statfs *fsb, u_char *dauuid, u_char *uuid)
{
	struct uuidlist *ulpnew;
	u_int32_t xfsid;

	ulpnew = malloc(sizeof(struct uuidlist));
	if (!ulpnew) {
		log(LOG_ERR, "add_uuid_to_list: out of memory");
		return (NULL);
	}
	bzero(ulpnew, sizeof(*ulpnew));
	LIST_INIT(&ulpnew->ul_exportids);
	if (dauuid) {
		bcopy(dauuid, ulpnew->ul_dauuid, sizeof(ulpnew->ul_dauuid));
		ulpnew->ul_davalid = 1;
	}
	bcopy(uuid, ulpnew->ul_uuid, sizeof(ulpnew->ul_uuid));
	strcpy(ulpnew->ul_mntfromname, fsb->f_mntfromname);
	strcpy(ulpnew->ul_mntonname, fsb->f_mntonname);

	/* make sure exported FS ID is unique */
	xfsid = UUID2FSID(uuid);
	ulpnew->ul_fsid = xfsid;
	while (find_uuid_by_fsid(ulpnew->ul_fsid))
		if (++ulpnew->ul_fsid == xfsid) {
			/* exhausted exported FS ID values! */
			log(LOG_ERR, "exported FS ID values exhausted, can't add %s",
				ulpnew->ul_mntonname);
			free(ulpnew);
			return (NULL);
		}

	TAILQ_INSERT_TAIL(&ulhead, ulpnew, ul_list);
	return (ulpnew);
}

/*
 * get the UUID to use for this volume's file handles
 * and add it to the UUID list if it isn't there yet.
 */
struct uuidlist *
get_uuid(const struct statfs *fsb, u_char *uuid)
{
	CFUUIDRef cfuuid;
	CFUUIDBytes uuidbytes;
	struct uuidlist *ulp;
	u_char dauuid[16];
	int davalid, uuidchanged, reportuuid = 0;
	char buf[64], buf2[64];

	/* get DiskArb's idea of the UUID (if any) */
	davalid = get_uuid_from_diskarb(fsb->f_mntfromname, dauuid);

	if (davalid) {
		log(LOG_DEBUG, "get_uuid: %s %s DiskArb says: %s",
			fsb->f_mntfromname, fsb->f_mntonname,
			uuidstring(dauuid, buf));
	}

	/* try to get UUID out of UUID list */
	if ((ulp = get_uuid_from_list(fsb, uuid, UL_CHECK_MNTON))) {
		log(LOG_DEBUG, "get_uuid: %s %s found: %s",
			fsb->f_mntfromname, fsb->f_mntonname,
			uuidstring(uuid, buf));
		/*
		 * Check against any DiskArb UUID.
		 * If diskarb UUID is different then drop the uuid entry.
		 */
		if (davalid) {
			if (!ulp->ul_davalid)
				uuidchanged = 1;
			else if (bcmp(ulp->ul_dauuid, dauuid, sizeof(dauuid)))
				uuidchanged = 1;
			else
				uuidchanged = 0;
		} else {
			if (ulp->ul_davalid) {
				/*
				 * We had a UUID before, but now we don't?
				 * Assume this is just a transient error,
				 * issue a warning, and stick with the old UUID.
				 */
				uuidstring(ulp->ul_dauuid, buf);
				log(LOG_WARNING, "lost UUID for %s, was %s, keeping old UUID",
					fsb->f_mntonname, buf);
				uuidchanged = 0;
			} else
				uuidchanged = 0;
		}
		if (uuidchanged) {
			uuidstring(ulp->ul_dauuid, buf);
			if (davalid)
				uuidstring(dauuid, buf2);
			else
				strcpy(buf2, "------------------------------------");
			if (ulp->ul_exported) {
				/*
				 * Woah!  We already have this file system exported with
				 * a different UUID (UUID changed while processing the
				 * exports list).  Ignore the UUID change for now so that
				 * all the exports for this file system will be registered
				 * using the same UUID/FSID.
				 *
				 * XXX Should we do something like set gothup=1 so that
				 * we will reregister all the exports (with the new UUID)?
				 * If so, what's to prevent an infinite loop if we always
				 * seem to be hitting this problem?
				 */
				log(LOG_WARNING, "ignoring UUID change for already exported file system %s, was %s now %s",
					fsb->f_mntonname, buf, buf2);
				uuidchanged = 0;
			}
		}
		if (uuidchanged) {
			log(LOG_WARNING, "UUID changed for %s, was %s now %s",
				fsb->f_mntonname, buf, buf2);
			bcopy(dauuid, uuid, sizeof(dauuid));
			/* remove old UUID from list */
			TAILQ_REMOVE(&ulhead, ulp, ul_list);
			free(ulp);
			ulp = NULL;
		} else {
			ulp->ul_exported = 1;
		}
	} else if (davalid) {
		/*
		 * The UUID wasn't in the list, but DiskArb has a UUID for it.
		 * (If the DiskArb UUID conflicts with something already in the
		 * list, we'll need to create a new UUID for it below.)
		 */
		bcopy(dauuid, uuid, sizeof(dauuid));
	} else {
		/*
		 * We need to create a UUID to use for this volume.
		 * This is because it wasn't already in the list, and
		 * either DiskArb didn't have a UUID for the volume or
		 * the UUID DiskArb has for the volume conflicts with
		 * a UUID for a volume already in the list.
		 */
		reportuuid = 1;
		cfuuid = CFUUIDCreate(NULL);
		uuidbytes = CFUUIDGetUUIDBytes(cfuuid);
		bcopy(&uuidbytes, uuid, sizeof(uuidbytes));
		CFRelease(cfuuid);
	}

	if (!ulp) {
		/*
		 * Add the UUID to the list, but make sure it is unique first.
		 */
		while ((ulp = find_uuid(uuid))) {
			reportuuid = 1;
			uuidstring(uuid, buf);
			log(LOG_WARNING, "%s UUID conflict with %s, %s",
				fsb->f_mntonname, ulp->ul_mntonname, buf);
			cfuuid = CFUUIDCreate(NULL);
			uuidbytes = CFUUIDGetUUIDBytes(cfuuid);
			bcopy(&uuidbytes, uuid, sizeof(uuidbytes));
			CFRelease(cfuuid);
			/* double check that the UUID is unique */
		}
		ulp = add_uuid_to_list(fsb, (davalid ? dauuid : NULL), uuid);
		if (!ulp) {
			log(LOG_ERR, "error adding %s", fsb->f_mntonname);
		} else {
			ulp->ul_exported = 1;
		}
	} else if (!ulp->ul_mntfromname[0]) {
		/*
		 * If the volume didn't exist when mountd read the
		 * mountdexptab, it's possible this ulp doesn't
		 * have a copy of it's mntfromname.  So, we make
		 * sure to grab a copy here before the volume gets
		 * exported.
		 */
		strcpy(ulp->ul_mntfromname, fsb->f_mntfromname);
	}

	if (reportuuid)
		log(LOG_WARNING, "%s using UUID %s", fsb->f_mntonname, uuidstring(uuid, buf));
	else
		log(LOG_DEBUG, "%s using UUID %s", fsb->f_mntonname, uuidstring(uuid, buf));

	return (ulp);
}

/*
 * clear export flags on all UUID entries
 */
void
uuidlist_clearexport(void)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		ulp->ul_exported = 0;
	}
}

/* convert UUID bytes to UUID string */
#define HEXTOC(c) \
	((c) >= 'a' ? ((c) - ('a' - 10)) : \
	((c) >= 'A' ? ((c) - ('A' - 10)) : ((c) - '0')))
#define HEXSTRTOI(p) \
	((HEXTOC(p[0]) << 4) + HEXTOC(p[1]))
char *
uuidstring(u_char *uuid, char *string)
{
	sprintf(string, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		uuid[0] & 0xff, uuid[1] & 0xff, uuid[2] & 0xff, uuid[3] & 0xff,
		uuid[4] & 0xff, uuid[5] & 0xff,
		uuid[6] & 0xff, uuid[7] & 0xff,
		uuid[8] & 0xff, uuid[9] & 0xff,
		uuid[10] & 0xff, uuid[11] & 0xff, uuid[12] & 0xff,
		uuid[13] & 0xff, uuid[14] & 0xff, uuid[15] & 0xff);
	return (string);
}

/*
 * save the exported volume UUID list to the mountdexptab file
 *
 * We have the option of saving all UUIDs in the list, or just
 * saving the ones that are currently exported.  However, if we
 * have a volume exported, then removed from the export list, and
 * then added back to the export list, it may be expected that the
 * file handles/UUIDs will be the same.  But if we don't save what
 * the UUIDs were before, we risk the chance of using a different
 * UUID for the second export.  This can happen if the volume's
 * DiskArb UUID is not used for export (because DiskArb doesn't have
 * a UUID for it, or because there was a UUID conflict and we needed
 * to use a different UUID).
 */
void
uuidlist_save(void)
{
	FILE *ulfile;
	struct uuidlist *ulp;
	struct expidlist *xid;
	char buf[64], buf2[64];

	if ((ulfile = fopen(_PATH_MOUNTEXPLIST, "w")) == NULL) {
		log(LOG_ERR, "Can't write %s: %s (%d)", _PATH_MOUNTEXPLIST,
			strerror(errno), errno);
		return;
	}
	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
#ifdef SAVE_ONLY_EXPORTED_UUIDS
		if (!ulp->ul_exported)
			continue;
#endif
		if (ulp->ul_davalid)
			uuidstring(ulp->ul_dauuid, buf);
		else
			strcpy(buf, "------------------------------------");
		uuidstring(ulp->ul_uuid, buf2);
		fprintf(ulfile, "%s %s 0x%08X %s\n", buf, buf2, ulp->ul_fsid, ulp->ul_mntonname);
		LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
			fprintf(ulfile, "XID 0x%08X %s\n", xid->xid_id,
				((xid->xid_path[0] == '\0') ? "." : xid->xid_path));
		}
	}
	fclose(ulfile);
}

/*
 * read in the exported volume UUID list from the mountdexptab file
 */
void
uuidlist_restore(void)
{
	struct uuidlist *ulp;
	struct expidlist *xid;
	char *cp, str[2*MAXPATHLEN];
	FILE *ulfile;
	int i, slen, linenum, davalid, uuidchanged;
	struct statfs fsb;
	u_char dauuid[16];
	char buf[64], buf2[64];

	if ((ulfile = fopen(_PATH_MOUNTEXPLIST, "r")) == NULL) {
		if (errno != ENOENT)
			log(LOG_NOTICE, "Can't open %s: %s (%d)", _PATH_MOUNTEXPLIST,
				strerror(errno), errno);
		else
			log(LOG_DEBUG, "Can't open %s, %s (%d)", _PATH_MOUNTEXPLIST,
				strerror(errno), errno);
		return;
	}
	ulp = NULL;
	linenum = 0;
	while (fgets(str, 2*MAXPATHLEN, ulfile) != NULL) {
		linenum++;
		slen = strlen(str);
		if (str[slen-1] == '\n')
			str[slen-1] = '\0';
		if (!strncmp(str, "XID ", 4)) {
			/* we have an export ID line for the current UUID */
			if (!ulp) {
				log(LOG_ERR, "ignoring orphaned export ID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				continue;
			}
			/* parse XID and add to current UUID */
			xid = malloc(sizeof(*xid));
			if (xid == NULL)
				out_of_mem("uuidlist_restore");
			cp = str + 4;
			slen -= 4;
			if (sscanf(cp, "%i", &xid->xid_id) != 1) {
				log(LOG_ERR, "invalid export ID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(xid);
				continue;
			}
			while (*cp && (*cp != ' ')) {
				cp++;
				slen--;
			}
			cp++;
			slen--;
			if (slen >= sizeof(xid->xid_path)) {
				log(LOG_ERR, "export ID path too long at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(xid);
				continue;
			}
			if ((cp[0] == '.') && (cp[1] == '\0'))
				xid->xid_path[0] = '\0';
			else
				strcpy(xid->xid_path, cp);
			LIST_INSERT_HEAD(&ulp->ul_exportids, xid, xid_list);
			continue;
		}
		ulp = malloc(sizeof(*ulp));
		if (ulp == NULL)
			out_of_mem("uuidlist_restore");
		bzero(ulp, sizeof(*ulp));
		LIST_INIT(&ulp->ul_exportids);
		cp = str;
		if (*cp == '-') {
			/* DiskArb UUID not present */
			ulp->ul_davalid = 0;
			bzero(ulp->ul_dauuid, sizeof(ulp->ul_dauuid));
			while (*cp && (*cp != ' '))
				cp++;
		} else {
			ulp->ul_davalid = 1;
			for (i=0; i < sizeof(ulp->ul_dauuid); i++, cp+=2) {
				if (*cp == '-')
					cp++;
				if (!isxdigit(*cp) || !isxdigit(*(cp+1))) {
					log(LOG_ERR, "invalid UUID at line %d of %s",
						linenum, _PATH_MOUNTEXPLIST);
					free(ulp);
					ulp = NULL;
					break;
				}
				ulp->ul_dauuid[i] = HEXSTRTOI(cp);
			}
		}
		if (ulp == NULL)
			continue;
		cp++;
		for (i=0; i < sizeof(ulp->ul_uuid); i++, cp+=2) {
			if (*cp == '-')
				cp++;
			if (!isxdigit(*cp) || !isxdigit(*(cp+1))) {
				log(LOG_ERR, "invalid UUID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(ulp);
				ulp = NULL;
				break;
			}
			ulp->ul_uuid[i] = HEXSTRTOI(cp);
		}
		if (ulp == NULL)
			continue;
		if (*cp != ' ') {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		cp++;
		if (sscanf(cp, "%i", &ulp->ul_fsid) != 1) {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		while (*cp && (*cp != ' '))
			cp++;
		if (*cp != ' ') {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		cp++;
		strncpy(ulp->ul_mntonname, cp, MAXPATHLEN);
		ulp->ul_mntonname[MAXPATHLEN-1] = '\0';

		/* verify the path exists and that it is a mount point */
		if (!check_dirpath(ulp->ul_mntonname) ||
		    (statfs(ulp->ul_mntonname, &fsb) < 0) ||
		    strcmp(ulp->ul_mntonname, fsb.f_mntonname)) {
			/* don't drop the UUID record if the volume isn't currently mounted! */
			/* If it's mounted/exported later, we want to use the same record. */
			log(LOG_DEBUG, "export entry for non-existent file system %s at line %d of %s",
				ulp->ul_mntonname, linenum, _PATH_MOUNTEXPLIST);
			ulp->ul_mntfromname[0] = '\0';
			TAILQ_INSERT_TAIL(&ulhead, ulp, ul_list);
			continue;
		}

		/* grab the path's mntfromname */
		strncpy(ulp->ul_mntfromname, fsb.f_mntfromname, MAXPATHLEN);
		ulp->ul_mntfromname[MAXPATHLEN-1] = '\0';

		/*
		 * Grab DiskArb's UUID for this volume (if any) and
		 * see if it has changed.
		 */
		davalid = get_uuid_from_diskarb(ulp->ul_mntfromname, dauuid);
		if (davalid) {
			if (!ulp->ul_davalid)
				uuidchanged = 1;
			else if (bcmp(ulp->ul_dauuid, dauuid, sizeof(dauuid)))
				uuidchanged = 1;
			else
				uuidchanged = 0;
		} else {
			if (ulp->ul_davalid) {
				/*
				 * We had a UUID before, but now we don't?
				 * Assume this is just a transient error,
				 * issue a warning, and stick with the old UUID.
				 */
				uuidstring(ulp->ul_dauuid, buf);
				log(LOG_WARNING, "lost UUID for %s, was %s, keeping old UUID",
					fsb.f_mntonname, buf);
				uuidchanged = 0;
			} else
				uuidchanged = 0;
		}
		if (uuidchanged) {
			/* The UUID changed, so we'll drop any entry */
			uuidstring(ulp->ul_dauuid, buf);
			if (davalid)
				uuidstring(dauuid, buf2);
			else
				strcpy(buf2, "------------------------------------");
			log(LOG_WARNING, "UUID changed for %s, was %s now %s",
				ulp->ul_mntonname, buf, buf2);
			free(ulp);
			continue;
		}

		TAILQ_INSERT_TAIL(&ulhead, ulp, ul_list);
	}
	fclose(ulfile);
}

struct expidlist *
find_export_id(struct uuidlist *ulp, u_int32_t id)
{
	struct expidlist *xid;

	LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
		if (xid->xid_id == id)
			break;
	}

	return (xid);
}

struct expidlist *
get_export_id(struct uuidlist *ulp, u_char *path)
{
	struct expidlist *xid;
	u_int32_t maxid = 0;

	LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
		if (!strcmp(xid->xid_path, (char *)path))
			break;
		if (maxid < xid->xid_id)
			maxid = xid->xid_id;
	}
	if (xid)
		return (xid);
	/* add it */
	xid = malloc(sizeof(*xid));
	if (!xid) {
		log(LOG_ERR, "get_export_id: out of memory");
		return (NULL);
	}
	bzero(xid, sizeof(*xid));
	strcpy(xid->xid_path, (char *)path);
	xid->xid_id = maxid + 1;
	while (find_export_id(ulp, xid->xid_id)) {
		xid->xid_id++;
		if (xid->xid_id == maxid) {
			/* exhausted export id values! */
			log(LOG_ERR, "export ID values exhausted for %s",
				ulp->ul_mntonname);
			free(xid);
			return (NULL);
		}
	}
	LIST_INSERT_HEAD(&ulp->ul_exportids, xid, xid_list);
	return (xid);
}

#define LINESIZ	10240
char line[LINESIZ];
FILE *exp_file;

/*
 * Get the export list
 */
void
get_exportlist(void)
{
	struct expfs *xf, *xf2, *xf3;
	struct grouplist *grp, *tgrp, *pgrp;
	struct expdir *xd, *xd2, *xd3;
	struct dirlist *dirhead, *dirl, *dirl2;
	struct dirlist *reghead, *reg;
	struct statfs fsb, *fsp, *firstfsp;
	struct hostent *hpe;
	struct xucred anon;
	char *cp, *endcp, *word, *hst, *usr, *dom, savedc, *savedcp, *s;
	int len, dlen, hostcount, badhostcount, exflags, got_nondir, num, i, netgrp, cmp;
	struct nfs_export_args nxa;
	u_char uuid[16];
	struct uuidlist *ulp;
	char buf[64];
	int error;
	int opt_flags;

	/*
	 * First, get rid of the old list
	 */
	log(LOG_DEBUG, "get_exportlist: freeing old exports...");
	while ((xf = TAILQ_FIRST(&xfshead))) {
		TAILQ_REMOVE(&xfshead, xf, xf_next);
		free_expfs(xf);
	}
	TAILQ_INIT(&xfshead);

	log(LOG_DEBUG, "get_exportlist: freeing old groups...");
	grp = grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	grphead = NULL;

	get_hostnames(our_hostnames);

	num = getmntinfo(&firstfsp, MNT_NOWAIT);
	/*
	 * Unregistration is slow due to running slp_reg.  We
	 * do it *before* removing exports from kernel.  The point is to
	 * minimize the amount of time a legit export vanished from kernel
	 * so active clients don't receive fatal errors.
	 */
	for (i = 0, fsp = firstfsp; i < num; i++, fsp++)
		register_export(fsp->f_mntonname, our_hostnames, DELETE_URL);
	/*
	 * Delete exports that are in the kernel for all local file systems.
	 */
	nxa.nxa_flags = NXA_DELETE_ALL;
	nxa.nxa_expid = 0;
	nxa.nxa_exppath = NULL;
	nxa.nxa_netcount = 0;
	nxa.nxa_nets = NULL;
	nxa.nxa_fsid = 0;
	nxa.nxa_fspath = NULL;
	error = nfssvc(NFSSVC_EXPORT, &nxa);
	if (error && (errno != ENOENT))
		log(LOG_ERR, "Can't delete all exports, %d", errno);
	uuidlist_clearexport();

	if ((exp_file = fopen(exportsfilepath, "r")) != NULL) {
		source = EXPORT_FROM_FILE;
	} else {
		if (source == EXPORT_FROM_FILE) {
			log(LOG_ERR, "Can't open %s", exportsfilepath);
			exit(2);
		}
		ni_exports_open();
		source = EXPORT_FROM_NETINFO;
	}

	/*
	 * Read in the exports and build the list, calling nfssvc(NFSSVC_EXPORT)
	 * as we go along to push the export rules into the kernel.
	 */

	dirhead = NULL;
	reghead = NULL;
	while (get_line(source)) {
		/*
		 * Create new exports list entry
		 */
		log(LOG_DEBUG, "---> Got line: %s", line);

		/*
		 * Set defaults.
		 */
		hostcount = badhostcount = 0;
		anon = def_anon;
		exflags = 0;
		opt_flags = 0;
		got_nondir = 0;
		xf = NULL;
		ulp = NULL;

		/* grab a group */
		pgrp = NULL;
		tgrp = grp = get_grp();
		if (!tgrp) {
			log(LOG_ERR, "can't allocate initial group for export");
			getexp_err(xf, tgrp);
			goto nextline;
		}

		/*
		 * process all the fields in this line
		 * (we loop until nextfield finds nothing)
		 */
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;

		while (endcp > cp) {
			log(LOG_DEBUG, "got field: %.*s", endcp-cp, cp);
			if (*cp == '-') {
			    /*
			     * looks like we have some options
			     */
			    if (xf == NULL) {
				log(LOG_ERR, "got options with no exported directory: %s", cp);
				getexp_err(xf, tgrp);
				goto nextline;
			    }
			    log(LOG_DEBUG, "processing option: %.*s", endcp-cp, cp);
			    got_nondir = 1;
			    savedcp = cp;
			    if (do_opt(&cp, &endcp, grp, &hostcount, &opt_flags,
				&exflags, &anon)) {
				log(LOG_ERR, "error processing options: %s", savedcp);
				getexp_err(xf, tgrp);
				goto nextline;
			    }
			} else if ((*cp == '/') ||
			    ((*cp == '\'' || *cp == '"') && (*(cp+1) == '/'))) {
			    /*
			     * looks like we have a pathname
			     */
			    log(LOG_DEBUG, "processing pathname: %.*s", endcp-cp, cp);
			    word = clean_white_space(cp);
			    log(LOG_DEBUG, "   cleaned pathname: %s", word);
			    if (word == NULL) {
				    log(LOG_ERR, "error processing pathname (out of memory)");
				    getexp_err(xf, tgrp);
				    goto nextline;
			    }
			    if (strlen(word) > RPCMNT_NAMELEN) {
				    log(LOG_ERR, "pathname too long (%d > %d): %s",
				    	strlen(word), RPCMNT_NAMELEN, word);
				    getexp_err(xf, tgrp);
				    free(word);
				    goto nextline;
			    }
			    if (!check_dirpath(word)) {
				log(LOG_ERR, "path contains symlinks: %s", word);
				getexp_err(xf, tgrp);
				free(word);
				goto nextline;
			    }
			    if (statfs(word, &fsb) < 0) {
				log(LOG_ERR, "statfs failed (%s) for path: %s",
					strerror(errno), word);
				getexp_err(xf, tgrp);
				free(word);
				goto nextline;
			    }
			    if (got_nondir) {
				    log(LOG_ERR, "Directories must be first: %s", word);
				    getexp_err(xf, tgrp);
				    free(word);
				    goto nextline;
			    }
			    /* get UUID for volume */
			    ulp = get_uuid(&fsb, uuid);
			    if (!ulp) {
				    log(LOG_ERR, "couldn't get UUID for volume: %s", fsb.f_mntonname);
				    getexp_err(xf, tgrp);
				    free(word);
				    goto nextline;
			    }
			    if (xf) {
				if (bcmp(xf->xf_uuid, uuid, sizeof(uuid))) {
				    log(LOG_ERR, "UUID mismatch for: %s\ndirectories must be on same volume", word);
				    getexp_err(xf, tgrp);
				    free(word);
				    goto nextline;
				}
			    } else {
				/*
				 * See if this directory is already
				 * in the export list.
				 */
				xf = ex_search(uuid);
				if (xf == NULL) {
				    xf = get_expfs();
				    if (xf)
					    xf->xf_fsdir = malloc(strlen(fsb.f_mntonname) + 1);
				    if (!xf || !xf->xf_fsdir) {
					    log(LOG_ERR, "Can't allocate memory to export volume: %s",
					    	fsb.f_mntonname);
					    getexp_err(xf, tgrp);
					    free(word);
					    goto nextline;
				    }
				    bcopy(uuid, xf->xf_uuid, sizeof(uuid));
				    strcpy(xf->xf_fsdir, fsb.f_mntonname);
				    log(LOG_DEBUG, "New expfs uuid=%s", uuidstring(uuid, buf));
				} else {
				    log(LOG_DEBUG, "Found expfs uuid=%s", uuidstring(uuid, buf));
				}
			    }
			    /*
			     * Add path to this line's directory list
			     */
			    error = add_dir(&dirhead, word);
			    if (error == EEXIST) {
				log(LOG_WARNING, "duplicate directory: %s", word);
				free(word);
			    } else if (error == ENOMEM) {
			        log(LOG_ERR, "Can't allocate memory to add directory: %s", word);
			        getexp_err(xf, tgrp);
			        free(word);
			        goto nextline;
			    }
			} else {
			    savedc = *endcp;
			    *endcp = '\0';
			    log(LOG_DEBUG, "got host/netgroup: %s", cp);
			    got_nondir = 1;
			    if (xf == NULL) {
				log(LOG_ERR, "got host/group with no directory?: %s", cp);
				getexp_err(xf, tgrp);
				goto nextline;
			    }
			    /*
			     * Get the host or netgroup.
			     */
			    setnetgrent(cp);
			    netgrp = getnetgrent(&hst, &usr, &dom);
			    do {
				if (hostcount && (grp->gr_type != GT_NULL)) {
					grp->gr_next = get_grp();
					if (!grp->gr_next) {
						log(LOG_ERR, "can't allocate next group for export");
						getexp_err(xf, tgrp);
						goto nextline;
					}
					pgrp = grp;
					grp = grp->gr_next;
				}
				if (netgrp) {
				    if (get_host_addresses(hst, grp)) {
					log(LOG_ERR, "couldn't get address for netgroup:host - %s:%s", cp, hst);
					badhostcount++;
				    } else {
					log(LOG_DEBUG, "got netgroup:host: %s:%s", cp, hst);
					hostcount++;
				    }
				} else if (get_host_addresses(cp, grp)) {
					log(LOG_ERR, "couldn't get address for host: %s", cp);
					badhostcount++;
				} else {
					log(LOG_DEBUG, "got host: %s", cp);
					hostcount++;
				}
			    } while (netgrp && getnetgrent(&hst, &usr, &dom));
			    endnetgrent();
			    *endcp = savedc;
			}
			cp = endcp;
			nextfield(&cp, &endcp);
		}

		/*
		 * Done processing exports line fields.
		 */
		if (!(opt_flags & (OP_MAPROOT|OP_MAPALL|OP_KERB))) {
			/* If no mapping option specified, map root by default */
			exflags |= NX_MAPROOT;
			opt_flags |= OP_MAPROOT;
		}
		if (check_options(opt_flags)) {
			log(LOG_ERR, "bad export options");
			getexp_err(xf, tgrp);
			goto nextline;
		}

		if (!hostcount) {
			if (badhostcount) {
				log(LOG_ERR, "no valid hosts found for export");
				getexp_err(xf, tgrp);
				goto nextline;
			}
			/* add a default group and make the grp list NULL */
			grp->gr_type = GT_HOST;
			log(LOG_DEBUG, "Adding a default entry");
			hpe = malloc(sizeof(struct hostent));
			if (hpe)
				hpe->h_name = malloc(sizeof(DEFAULTHOSTNAME));
			if (!hpe || !hpe->h_name) {
				if (hpe)
					free(hpe);
				log(LOG_ERR, "Can't allocate memory for default export entry %s",
					xf->xf_fsdir);
				getexp_err(xf, tgrp);
				goto nextline;
			}
			strcpy(hpe->h_name, DEFAULTHOSTNAME);
			hpe->h_addrtype = AF_INET;
			hpe->h_length = sizeof (u_long);
			hpe->h_addr_list = (char **)NULL;
			grp->gr_u.gt_hostent = hpe;
		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			/*
			 * Don't allow a network export coincide with a list of
			 * host(s) on the same line.
			 */
			log(LOG_ERR, "can't specify both network and hosts on same line");
			getexp_err(xf, tgrp);
			goto nextline;
		} else if (badhostcount && pgrp && (grp->gr_type == GT_NULL)) {
			/*
			 * We ended with a bad host, so the final grp wasn't used.
			 * Remove that unused grp from the end of the list.
			 */
			pgrp->gr_next = NULL;
			free_grp(grp);
			grp = pgrp;
		}

		/*
		 * Walk the dirlist.
		 * Verify the next dir is (or can be) an exported directory.
		 * Check subsequent dirs to see if they are mount subdirs of that dir.
		 */
		dirl = dirhead;
		while (dirl) {
			/* check for existing/conflicting export */
			log(LOG_DEBUG, "dir: %s", dirl->dl_dir);
			s = dirl->dl_dir;
			len = strlen(s);
			xd3 = NULL;
			cmp = 1;
			TAILQ_FOREACH(xd2, &xf->xf_dirl, xd_next) {
				dlen = strlen(xd2->xd_dir);
				cmp = strncmp(dirl->dl_dir, xd2->xd_dir, dlen);
				log(LOG_DEBUG, "     %s compare %s %d", dirl->dl_dir, xd2->xd_dir, cmp);
				if (!cmp) {
					if ((len == dlen) || (dirl->dl_dir[dlen] == '/')) {
						/* found a match */
						break;
					}
					/* dirl was actually longer than xd2 */
					cmp = 1;
				}
				if (cmp > 0)
					break;
				xd3 = xd2;
			}
			if (cmp != 0) {
				if (xd3 && !strncmp(xd3->xd_dir, s, len) &&
				    (xd3->xd_dir[len] == '/')) {
					log(LOG_ERR, "%s conflicts with existing export %s", s, xd3->xd_dir);
					getexp_err(xf, tgrp);
					goto nextline;
				}
				if (xd2 && !strncmp(xd2->xd_dir, s, strlen(xd2->xd_dir)) &&
				    (s[strlen(xd2->xd_dir)] == '/')) {
					log(LOG_ERR, "%s conflicts with existing export %s", s, xd2->xd_dir);
					getexp_err(xf, tgrp);
					goto nextline;
				}
				xd = get_expdir();
				if (xd)
					xd->xd_dir = strdup(dirl->dl_dir);
				if (!xd || !xd->xd_dir) {
					if (xd)
						free_expdir(xd);
					log(LOG_ERR, "can't allocate memory for export %s", dirl->dl_dir);
					getexp_err(xf, tgrp);
					goto nextline;
				}
				log(LOG_DEBUG, "     %s new xd", xd->xd_dir);
			} else {
				xd = xd2;
				log(LOG_DEBUG, "     %s xd is %s", dirl->dl_dir, xd->xd_dir);
				if (!hostcount && (xd->xd_flags & OP_DEFEXP)) {
					log(LOG_ERR, "%s already has a default export", s);
					getexp_err(xf, tgrp);
					goto nextline;
				}
			}

			/*
			 * Send list of hosts to do_export for pushing the exports into
			 * the kernel.
			 */
			if (do_export(xf, tgrp, exflags, &anon, dirl->dl_dir, &fsb, ulp)) {
				log(LOG_ERR, "kernel export registration failed");
				getexp_err(xf, tgrp);
				goto nextline;
			}
			/* Success. Update the data structures.  */
			log(LOG_DEBUG, "kernel export registered for %s", dirl->dl_dir);

			/* add options to this exported directory */
			if (hang_options(xd, opt_flags, (hostcount ? tgrp : NULL))) {
				log(LOG_ERR, "export option conflict for %s", xd->xd_dir);
				// XXX what to do about already successful exports?
				getexp_err(xf, tgrp);
				goto nextline;
			}

			/* add mount subdirectories of this directory */
			dirl2 = dirl->dl_next;
			while (dirl2) {
				if (strncmp(dirl->dl_dir, dirl2->dl_dir, len) ||
				    (dirl2->dl_dir[len] != '/'))
					break;
				error = hang_options_mountdir(xd, dirl2->dl_dir, opt_flags,
						(hostcount ? tgrp : NULL));
				if (error == EEXIST) {
					log(LOG_WARNING, "mount subdirectory export option conflict for %s",
						dirl2->dl_dir);
				} else if (error == ENOMEM) {
					log(LOG_WARNING, "unable to add mount subdirectory for %s, %s",
						xd->xd_dir, dirl2->dl_dir);
				}
				dirl2 = dirl2->dl_next;
			}
			dirl = dirl2;

			if (cmp) {
				/* add new expdir to xf */
				if (xd3) {
					TAILQ_INSERT_AFTER(&xf->xf_dirl, xd3, xd, xd_next);
				} else {
					TAILQ_INSERT_HEAD(&xf->xf_dirl, xd, xd_next);
				}
				/*
				 * Add exported directory path to reghead,
				 * the list of paths we will register.
				 */
				s = strdup(xd->xd_dir);
				if (add_dir(&reghead, s))
					free(s);
			}

		}

		if (hostcount) {
			grp->gr_next = grphead;
			grphead = tgrp;
		} else {
			free_grp(grp);
		}
		if ((xf->xf_flag & XF_LINKED) == 0) {
			/* Insert in the list in alphabetical order. */
			xf3 = NULL;
			TAILQ_FOREACH(xf2, &xfshead, xf_next) {
				if (strcmp(xf->xf_fsdir, xf2->xf_fsdir) < 0)
					break;
				xf3 = xf2;
			}
			if (xf3) {
				TAILQ_INSERT_AFTER(&xfshead, xf3, xf, xf_next);
			} else {
				TAILQ_INSERT_HEAD(&xfshead, xf, xf_next);
			}
			xf->xf_flag |= XF_LINKED;
		}
nextline:
		if (dirhead) {
			free_dirlist(dirhead);
			dirhead = NULL;
		}

	}

	/*
	 * Registration is slow due to running slp_reg.  We
	 * do it *after* adding exports from kernel.  The point is to
	 * minimize the amount of time a legit export vanished from kernel
	 * so active clients won't receive fatal errors.
	 */
 	reg = reghead;
 	while (reg) {
		register_export(reg->dl_dir, our_hostnames, ADD_URL);
		reg = reg->dl_next;
	}
	free_dirlist(reghead);
	reghead = NULL;

	if (source == EXPORT_FROM_NETINFO) ni_exports_close();
	else fclose(exp_file);

	uuidlist_save();
}

/*
 * Allocate an export list element
 */
struct expfs *
get_expfs(void)
{
	struct expfs *xf;

	xf = malloc(sizeof(*xf));
	if (xf == NULL)
		return (NULL);
	memset(xf, 0, sizeof(*xf));
	TAILQ_INIT(&xf->xf_dirl);
	return (xf);
}

struct expdir *
get_expdir(void)
{
	struct expdir *xd;

	xd = malloc(sizeof(*xd));
	if (xd == NULL)
		return (NULL);
	memset(xd, 0, sizeof(*xd));
	TAILQ_INIT(&xd->xd_mountdirs);
	return (xd);
}

/*
 * Allocate a group list element
 */
struct grouplist *
get_grp(void)
{
	struct grouplist *grp;

	grp = malloc(sizeof(*grp));
	if (grp == NULL)
		return (NULL);
	memset(grp, 0, sizeof(*grp));
	return (grp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
void
getexp_err(struct expfs *xf, struct grouplist *grp)
{
	struct grouplist *tgrp;

	/* XXX might be nice if we could report the file/line# here */
	log(LOG_ERR, "Error processing exports list line: %s", line);
	if (xf && (xf->xf_flag & XF_LINKED) == 0)
		free_expfs(xf);
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
}

/*
 * Search the export list for a matching fs.
 */
struct expfs *
ex_search(u_char *uuid)
{
	struct expfs *xf;

	TAILQ_FOREACH(xf, &xfshead, xf_next) {
		if (!bcmp(xf->xf_uuid, uuid, 16))
			return (xf);
	}
	return (xf);
}

/*
 * add a directory to a dirlist (sorted)
 *
 * Note that the list is sorted to place subdirectories
 * after the entry for their matching parent directory.
 * This isn't strictly sorted because other directories may
 * have similar names with characters that sort lower than '/'.
 * For example: /export /export.test /export/subdir
 */
int
add_dir(struct dirlist **dlpp, char *cp)
{
	struct dirlist *newdl, *dl, *dl2, *dl3, *dlstop;
	int cplen, dlen, cmp;

	dlstop = NULL;
	dl2 = NULL;
	dl = *dlpp;
	cplen = strlen(cp);

	while (dl && (dl != dlstop)) {
		dlen = strlen(dl->dl_dir);
		cmp = strncmp(cp, dl->dl_dir, dlen);
		log(LOG_DEBUG, "add_dir: %s compare %s %d", cp, dl->dl_dir, cmp);
		if (cmp < 0)
			break;
		if (cmp == 0) {
			if (cplen == dlen) // duplicate
				return (EEXIST);
			if (cp[dlen] == '/') {
				/*
				 * Find the next entry that isn't a
				 * subdirectory of this directory so
				 * we know when to stop looking for
				 * the insertion point.
				 */
				log(LOG_DEBUG, "add_dir: %s compare %s %d subdir match",
					cp, dl->dl_dir, cmp);
				dlstop = dl->dl_next;
				while (dlstop && !strncmp(dl->dl_dir, dlstop->dl_dir, dlen) &&
					(!dlstop->dl_dir[dlen] || (dlstop->dl_dir[dlen] == '/')))
					dlstop = dlstop->dl_next;
			} else {
				/*
				 * The new dir should go after this directory and
				 * its subdirectories.  So, skip subdirs of this dir.
				 */
				log(LOG_DEBUG, "add_dir: %s compare %s %d partial match", cp, dl->dl_dir, cmp);
				dl3 = dl;
				dl2 = dl;
				dl = dl->dl_next;
				while (dl && !strncmp(dl3->dl_dir, dl->dl_dir, dlen) &&
					(!dl->dl_dir[dlen] || (dl->dl_dir[dlen] == '/'))) {
					dl2 = dl;
					dl = dl->dl_next;
					}
				continue;
			}
		}
		dl2 = dl;
		dl = dl->dl_next;
	}
	if (dl && (dl == dlstop))
		log(LOG_DEBUG, "add_dir: %s stopped before %s", cp, dlstop->dl_dir);
	newdl = malloc(sizeof(*dl));
	if (newdl == NULL) {
		log(LOG_ERR, "can't allocate memory to add dir %s", cp);
		return (ENOMEM);
	}
	newdl->dl_dir = cp;
	if (dl2 == NULL) {
		newdl->dl_next = *dlpp;
		*dlpp = newdl;
	} else {
		newdl->dl_next = dl;
		dl2->dl_next = newdl;
	}
	if (debug > 1) {
		dl = *dlpp;
		while (dl) {
			log(LOG_DEBUG, "DIRLIST: %s", dl->dl_dir);
			dl = dl->dl_next;
		}
	}
	return (0);
}

/*
 * free up all the elements in a dirlist
 */
void
free_dirlist(struct dirlist *dl)
{
	struct dirlist *dl2;

	while (dl) {
		dl2 = dl->dl_next;
		if (dl->dl_dir)
			free(dl->dl_dir);
		free(dl);
		dl = dl2;
	}
}

/*
 * hang export options for a list of groups off of an exported directory
 */
int
hang_options(struct expdir *xd, int opt_flags, struct grouplist *grp)
{
	struct host *ht;

	if (!grp) {
		/* default export */
		if (xd->xd_flags & OP_DEFEXP) {
			/* exported directory already has default export! */
			if ((xd->xd_flags & OP_EXOPTMASK) == opt_flags) {
				log(LOG_WARNING, "duplicate default export for %s", xd->xd_dir);
				return (0);
			} else {
				log(LOG_ERR, "multiple/conflicting default exports for %s", xd->xd_dir);
				return (EEXIST);
			}
		}
		xd->xd_flags = opt_flags | OP_DEFEXP;
		log(LOG_DEBUG, "hang_options: %s default 0x%x", xd->xd_dir, xd->xd_flags);
		return (0);
	}

	while (grp) {
		// XXX should we first check for an existing entry for this group?
		ht = get_host();
		if (!ht) {
			log(LOG_ERR, "Can't allocate memory for host: %s",
				((grp->gr_type == GT_NET) ? grp->gr_u.gt_net.nt_name :
				((grp->gr_type == GT_HOST) ? grp->gr_u.gt_hostent->h_name : "???")));
			return(ENOMEM);
		}
		ht->ht_flag = opt_flags;
		ht->ht_grp = grp;
		ht->ht_next = xd->xd_hosts;
		xd->xd_hosts = ht;
		log(LOG_DEBUG, "hang_options: %s %s 0x%x", xd->xd_dir,
			((grp->gr_type == GT_NET) ? grp->gr_u.gt_net.nt_name :
			((grp->gr_type == GT_HOST) ? grp->gr_u.gt_hostent->h_name : "???")),
			opt_flags);
		grp = grp->gr_next;
	}

	return (0);
}

/*
 * hang export options for mountable subdirectories of an exported directory
 */
int
hang_options_mountdir(struct expdir *xd, char *dir, int opt_flags, struct grouplist *grp)
{
	struct host *ht;
	struct expdir *mxd, *mxd2, *mxd3;
	int cmp;

	/* check for existing mountdir */
	mxd = mxd3 = NULL;
	TAILQ_FOREACH(mxd2, &xd->xd_mountdirs, xd_next) {
		cmp = strcmp(dir, mxd2->xd_dir);
		if (!cmp) {
			/* found it */
			mxd = mxd2;
			break;
		} else if (cmp < 0) {
			/* found where it needs to be inserted */
			break;
		}
		mxd3 = mxd2;
	}
	if (!mxd) {
		mxd = get_expdir();
		if (mxd)
			mxd->xd_dir = strdup(dir);
		if (!mxd || !mxd->xd_dir) {
			if (mxd)
				free_expdir(mxd);
			log(LOG_ERR, "can't allocate memory for mountable sub-directory; %s", dir);
			return (ENOMEM);
		}
		if (mxd3) {
			TAILQ_INSERT_AFTER(&xd->xd_mountdirs, mxd3, mxd, xd_next);
		} else {
			TAILQ_INSERT_HEAD(&xd->xd_mountdirs, mxd, xd_next);
		}
	}

	if (!grp) {
		/* default export */
		if (mxd->xd_flags & OP_DEFEXP) {
			/* exported directory already has default export! */
			if ((mxd->xd_flags & OP_EXOPTMASK) == opt_flags) {
				log(LOG_WARNING, "duplicate default export for %s", mxd->xd_dir);
				return (0);
			} else {
				log(LOG_ERR, "multiple/conflicting default exports for %s", mxd->xd_dir);
				return (EEXIST);
			}
		}
		mxd->xd_flags = opt_flags | OP_DEFEXP;
		log(LOG_DEBUG, "hang_options_mountdir: %s default 0x%x", mxd->xd_dir, mxd->xd_flags);
		return (0);
	}

	while (grp) {
		// XXX should we first check for an existing entry for this group?
		ht = get_host();
		if (!ht) {
			log(LOG_ERR, "Can't allocate memory for host: %s",
				((grp->gr_type == GT_NET) ? grp->gr_u.gt_net.nt_name :
				((grp->gr_type == GT_HOST) ? grp->gr_u.gt_hostent->h_name : "???")));
			return(ENOMEM);
		}
		ht->ht_flag = opt_flags;
		ht->ht_grp = grp;
		ht->ht_next = mxd->xd_hosts;
		mxd->xd_hosts = ht;
		log(LOG_DEBUG, "hang_options_mountdir: %s %s 0x%x", mxd->xd_dir,
			((grp->gr_type == GT_NET) ? grp->gr_u.gt_net.nt_name :
			((grp->gr_type == GT_HOST) ? grp->gr_u.gt_hostent->h_name : "???")),
			opt_flags);
		grp = grp->gr_next;
	}

	return (0);
}

/*
 * Search for an exported directory on an exported file system that
 * a given host can mount and return the export options.
 *
 * Search order:
 * an exact match on exported directory path
 * an exact match on exported directory mountdir path
 * a subdir match on exported directory mountdir path with ALLDIRS
 * a subdir match on exported directory path with ALLDIRS
 */
int
expdir_search(struct expfs *xf, char *dirpath, u_long saddr, int *options)
{
	struct expdir *xd, *mxd;
	struct host *hp;
	int dirpathlen, dlen = 0;
	int chkalldirs = 0;

	dirpathlen = strlen(dirpath);

	TAILQ_FOREACH(xd, &xf->xf_dirl, xd_next) {
		dlen = strlen(xd->xd_dir);
		if (strncmp(dirpath, xd->xd_dir, dlen))
			continue;
		if ((dirpathlen == dlen) || (dirpath[dlen] == '/'))
			break;
	}
	if (!xd) {
		log(LOG_DEBUG, "expdir_search: no matching export: %s", dirpath);
		return (0);
	}

	log(LOG_DEBUG, "expdir_search: %s -> %s", dirpath, xd->xd_dir);

	if (dirpathlen == dlen) {
		/* exact match on exported directory path */
check_xd_hosts:
		/* find options for this host */
		hp = find_host(xd->xd_hosts, saddr);
		if (hp && (!chkalldirs || (hp->ht_flag & OP_ALLDIRS))) {
			log(LOG_DEBUG, "expdir_search: %s host %s", dirpath, (chkalldirs ? "alldirs" : "match"));
			*options = hp->ht_flag;
		} else if ((xd->xd_flags & OP_DEFEXP) && (!chkalldirs || (xd->xd_flags & OP_ALLDIRS))) {
			log(LOG_DEBUG, "expdir_search: %s defexp %s", dirpath, (chkalldirs ? "alldirs" : "match"));
			*options = xd->xd_flags;
		} else {
			// not exported to this host
			*options = 0;
			log(LOG_DEBUG, "expdir_search: %s NO match", dirpath);
			return (0);
		}
		return (1);
	}

	// search for a matching mountdir
	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		dlen = strlen(mxd->xd_dir);
		if (strncmp(dirpath, mxd->xd_dir, dlen))
			continue;
		log(LOG_DEBUG, "expdir_search: %s subdir path partial match %s", dirpath, mxd->xd_dir);
		if ((dirpathlen != dlen) && (dirpath[dlen] != '/'))
			continue;
		chkalldirs = (dirpathlen != dlen);
		/* found a match on a mountdir */
		hp = find_host(mxd->xd_hosts, saddr);
		if (hp && (!chkalldirs || (hp->ht_flag & OP_ALLDIRS))) {
			log(LOG_DEBUG, "expdir_search: %s -> %s subdir host %s", dirpath, mxd->xd_dir, (chkalldirs ? "alldirs" : "match"));
			*options = hp->ht_flag;
			return (1);
		} else if ((mxd->xd_flags & OP_DEFEXP) && (!chkalldirs || (mxd->xd_flags & OP_ALLDIRS))) {
			log(LOG_DEBUG, "expdir_search: %s -> %s subdir defexp %s", dirpath, mxd->xd_dir, (chkalldirs ? "alldirs" : "match"));
			*options = mxd->xd_flags;
			return (1);
		}
		/* not exported to this host */
	}

	log(LOG_DEBUG, "expdir_search: %s NO match, check alldirs", dirpath);
	chkalldirs = 1;
	goto check_xd_hosts;
}

/*
 * search a host list for a match for the given address
 */
struct host *
find_host(struct host *hp, u_long saddr)
{
	struct grouplist *grp;
	u_long **addrp;

	while (hp) {
		grp = hp->ht_grp;
		switch (grp->gr_type) {
		case GT_HOST:
			addrp = (u_long **) grp->gr_u.gt_hostent->h_addr_list;
			while (*addrp) {
				if (**addrp == saddr)
					return (hp);
				addrp++;
			}
			break;
		case GT_NET:
			if ((saddr & grp->gr_u.gt_net.nt_mask) ==
			    grp->gr_u.gt_net.nt_net)
				return (hp);
			break;
		}
		hp = hp->ht_next;
	}

	return (NULL);
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
int
do_opt( char **cpp,
	char **endcpp,
	struct grouplist *grp,
	int *hostcountp,
	int *opt_flagsp,
	int *exflagsp,
	struct xucred *cr)
{
	char *cpoptarg, *cpoptend;
	char *cp, *endcp, *cpopt, savedc, savedc2 = '\0';
	int allflag, usedarg;

	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		if (NULL != (cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if (NULL != (cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
		} else {
			if (NULL != (cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
			else {
				*cp = savedc;
				nextfield(&cp, &endcp);
				**endcpp = '\0';
				if (endcp > cp && *cp != '-') {
					cpoptarg = cp;
					savedc2 = *endcp;
					*endcp = '\0';
					usedarg = 0;
				}
			}
		}
		if (!strcmp(cpopt, "ro") || !strcmp(cpopt, "o")) {
			*exflagsp |= NX_READONLY;
			*opt_flagsp |= OP_READONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
			!(allflag = strcmp(cpopt, "mapall")) ||
			!strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			parsecred(cpoptarg, cr);
			if (allflag == 0) {
				*exflagsp |= NX_MAPALL;
				*opt_flagsp |= OP_MAPALL;
			} else {
				*exflagsp |= NX_MAPROOT;
				*opt_flagsp |= OP_MAPROOT;
			}
		} else if (!strcmp(cpopt, "kerb") || !strcmp(cpopt, "k")) {
			*exflagsp |= NX_KERB;
			*opt_flagsp |= OP_KERB;
		} else if (cpoptarg && (!strcmp(cpopt, "mask") ||
			!strcmp(cpopt, "m"))) {
			if (get_net(cpoptarg, &grp->gr_u.gt_net, 1)) {
				log(LOG_ERR, "Bad mask: %s", cpoptarg);
				return (1);
			}
			usedarg++;
			*opt_flagsp |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
			!strcmp(cpopt, "n"))) {
			if (grp->gr_type != GT_NULL) {
				log(LOG_ERR, "Network/host conflict");
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_u.gt_net, 0)) {
				log(LOG_ERR, "Bad net: %s", cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*hostcountp = *hostcountp + 1;
			usedarg++;
			*opt_flagsp |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			*opt_flagsp |= OP_ALLDIRS;
#ifdef ISO
		} else if (cpoptarg && !strcmp(cpopt, "iso")) {
			if (get_isoaddr(cpoptarg, grp)) {
				log(LOG_ERR, "Bad iso addr: %s", cpoptarg);
				return (1);
			}
			*hostcountp = *hostcountp + 1;
			usedarg++;
			*opt_flagsp |= OP_ISO;
#endif /* ISO */
		} else if (!strcmp(cpopt, "32bitclients")) {
			*exflagsp |= NX_32BITCLIENTS;
			*opt_flagsp |= OP_32BITCLIENTS;
		} else {
			log(LOG_ERR, "Bad opt %s", cpopt);
			return (1);
		}
		if (usedarg >= 0) {
			*endcp = savedc2;
			**endcpp = savedc;
			if (usedarg > 0) {
				*cpp = cp;
				*endcpp = endcp;
			}
			return (0);
		}
		cpopt = cpoptend;
	}
	**endcpp = savedc;
	return (0);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
int
get_host_addresses(char *cp, struct grouplist *grp)
{
	struct hostent *hp, *nhp;
	char **addrp, **naddrp;
	struct hostent t_host;
	int i;
	u_long saddr;
	char *aptr[2];

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((hp = gethostbyname(cp)) == NULL) {
		if (!isdigit(*cp)) {
			log(LOG_ERR, "Gethostbyname failed for %s", cp);
			return (1);
		}
		saddr = inet_addr(cp);
		if (saddr == -1) {
			log(LOG_ERR, "Inet_addr failed for %s", cp);
			return (1);
		}
		if ((hp = gethostbyaddr((caddr_t)&saddr, sizeof (saddr),
					AF_INET)) == NULL) {
			hp = &t_host;
			hp->h_name = cp;
			hp->h_addrtype = AF_INET;
			hp->h_length = sizeof (u_long);
			hp->h_addr_list = aptr;
			aptr[0] = (char *)&saddr;
			aptr[1] = (char *)NULL;
		}
	}
	nhp = malloc(sizeof(struct hostent));
	if (nhp == NULL)
		goto nomem;
	memmove(nhp, hp, sizeof(struct hostent));
	i = strlen(hp->h_name)+1;
	nhp->h_name = malloc(i);
	if (nhp->h_name == NULL)
		goto nomem;
	memmove(nhp->h_name, hp->h_name, i);
	addrp = hp->h_addr_list;
	i = 1;
	while (*addrp++)
		i++;
	naddrp = nhp->h_addr_list = malloc(i*sizeof(char *));
	if (naddrp == NULL)
		goto nomem;
	bzero(naddrp, i*sizeof(char *));
	addrp = hp->h_addr_list;
	while (*addrp) {
		*naddrp = malloc(hp->h_length);
		if (*naddrp == NULL)
			goto nomem;
		memmove(*naddrp, *addrp, hp->h_length);
		addrp++;
		naddrp++;
	}
	*naddrp = NULL;
	grp->gr_type = GT_HOST;
	grp->gr_u.gt_hostent = nhp;
	return (0);
nomem:
	if (nhp) {
		if (nhp->h_name) {
			naddrp = nhp->h_addr_list;
			while (naddrp && *naddrp)
				free(*naddrp++);
			if (nhp->h_addr_list)
				free(nhp->h_addr_list);
			free(nhp->h_name);
		}
		free(nhp);
	}
	log(LOG_ERR, "can't allocate memory for host address(es) for %s", cp);
	return (1);
}

/*
 * Free up an exported directory structure
 */
void
free_expdir(struct expdir *xd)
{
	struct expdir *xd2;

	free_host(xd->xd_hosts);
	while ((xd2 = TAILQ_FIRST(&xd->xd_mountdirs))) {
		TAILQ_REMOVE(&xd->xd_mountdirs, xd2, xd_next);
		free_expdir(xd2);
	}
	if (xd->xd_dir)
		free(xd->xd_dir);
	free(xd);
}

/*
 * Free up an exportfs structure
 */
void
free_expfs(struct expfs *xf)
{
	struct expdir *xd;

	while ((xd = TAILQ_FIRST(&xf->xf_dirl))) {
		TAILQ_REMOVE(&xf->xf_dirl, xd, xd_next);
		free_expdir(xd);
	}
	if (xf->xf_fsdir)
		free(xf->xf_fsdir);
	free(xf);
}

/*
 * Free up a list of hosts.
 */
void
free_host(struct host *hp)
{
	struct host *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free((caddr_t)hp2);
	}
}

/*
 * Allocate a host structure
 */
struct host *
get_host(void)
{
	struct host *hp;

	hp = malloc(sizeof(struct host));
	if (hp == NULL)
		return (NULL);
	hp->ht_next = NULL;
	hp->ht_flag = 0;
	return (hp);
}

#ifdef ISO
/*
 * Translate an iso address.
 */
int
get_isoaddr(char *cp, struct grouplist *grp)
{
	struct iso_addr *isop;
	struct sockaddr_iso *isoaddr;

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((isop = iso_addr(cp)) == NULL) {
		log(LOG_ERR, "iso_addr failed, ignored");
		return (1);
	}
	isoaddr = malloc(sizeof (struct sockaddr_iso));
	if (isoaddr == NULL) {
		log(LOG_ERR, "can't allocate memory for ISO address: %s", cp);
		return (1);
	}
	memset(isoaddr, 0, sizeof(struct sockaddr_iso));
	memmove(&isoaddr->siso_addr, isop, sizeof(struct iso_addr));
	isoaddr->siso_len = sizeof(struct sockaddr_iso);
	isoaddr->siso_family = AF_ISO;
	grp->gr_type = GT_ISO;
	grp->gr_u.gt_isoaddr = isoaddr;
	return (0);
}
#endif	/* ISO */

/*
 * Out of memory, fatal
 */
void
out_of_mem(char *msg)
{
	log(LOG_ERR, "%s: Out of memory", msg);
	exit(2);
}

/*
 * Do the NFSSVC_EXPORT syscall to push the export info into the kernel.
 */
int
do_export(
	struct expfs *xf,
	struct grouplist *grplist,
	int exflags,
	struct xucred *anoncrp,
	char *dir,
	struct statfs *fsb,
	struct uuidlist *ulp)
{
	struct nfs_export_args nxa;
	struct nfs_export_net_args *netargs, *na;
	struct expidlist *xid;
	struct grouplist *grp;
	u_long **addrp;
	struct sockaddr_in *sin, *imask;
	u_long net;
	char *subdir;

	nxa.nxa_flags = NXA_ADD;
	nxa.nxa_fsid = ulp->ul_fsid;
	nxa.nxa_fspath = fsb->f_mntonname;

	/* get export path and ID */
	if (strncmp(dir, fsb->f_mntonname, strlen(fsb->f_mntonname)))
		log(LOG_WARNING, "do_export(): exported dir/fs mismatch: %s %s",
			dir, fsb->f_mntonname);
	/* point subdir beyond mount path string */
	subdir = dir + strlen(fsb->f_mntonname);
	/* skip "/" between mount and subdir */
	while (*subdir && (*subdir == '/'))
		subdir++;
	xid = get_export_id(ulp, (u_char *)subdir);
	if (!xid) {
		log(LOG_ERR, "do_export(): unable to get export ID for %s", dir);
		return (1);
	}
	nxa.nxa_exppath = xid->xid_path;
	nxa.nxa_expid = xid->xid_id;

	/* first, count the number of hosts/nets we're pushing in for this export */
	nxa.nxa_netcount = 0;
	grp = grplist;
	while (grp) {
		if (grp->gr_type == GT_HOST) {
			addrp = (u_long **)grp->gr_u.gt_hostent->h_addr_list;
			if (!addrp) /* default export */
				nxa.nxa_netcount++;
			/* count # addresses given for this host */
			while (addrp && *addrp) {
				nxa.nxa_netcount++;
				addrp++;
			}
		} else {
			nxa.nxa_netcount++;
		}
		grp = grp->gr_next;
	}
	netargs = malloc(nxa.nxa_netcount * sizeof(struct nfs_export_net_args));
	if (!netargs) {
		/* XXX we could possibly fall back to pushing them in, one-by-one */
		log(LOG_ERR, "do_export(): malloc failed for %d net args", nxa.nxa_netcount);
		return (1);
	}
	nxa.nxa_nets = netargs;

#define INIT_NETARG(N) \
	do { \
		(N)->nxna_flags = exflags; \
		(N)->nxna_cred = *anoncrp; \
		sin = (struct sockaddr_in*)&(N)->nxna_addr; \
		imask = (struct sockaddr_in*)&(N)->nxna_mask; \
		memset(sin, 0, sizeof(*sin)); \
		memset(imask, 0, sizeof(*imask)); \
		sin->sin_family = AF_INET; \
		sin->sin_len = sizeof(*sin); \
		imask->sin_family = AF_INET; \
		imask->sin_len = sizeof(*imask); \
	} while (0)

	na = netargs;
	grp = grplist;
	while (grp) {
		switch (grp->gr_type) {
		case GT_HOST:
			addrp = (u_long **)grp->gr_u.gt_hostent->h_addr_list;
			if (!addrp) {
				/* default export, no address */
				INIT_NETARG(na);
				sin->sin_len = 0;
				imask->sin_len = 0;
				na++;
				break;
			}
			/* handle each host address in h_addr_list */
			while (*addrp) {
				INIT_NETARG(na);
				sin->sin_addr.s_addr = **addrp;
				imask->sin_len = 0;
				addrp++;
				na++;
			}
			break;
		case GT_NET:
			INIT_NETARG(na);
			if (grp->gr_u.gt_net.nt_mask)
			    imask->sin_addr.s_addr = grp->gr_u.gt_net.nt_mask;
			else {
			    net = ntohl(grp->gr_u.gt_net.nt_net);
			    if (IN_CLASSA(net))
				imask->sin_addr.s_addr = inet_addr("255.0.0.0");
			    else if (IN_CLASSB(net))
				imask->sin_addr.s_addr =
				    inet_addr("255.255.0.0");
			    else
				imask->sin_addr.s_addr =
				    inet_addr("255.255.255.0");
			    grp->gr_u.gt_net.nt_mask = imask->sin_addr.s_addr;
			}
			sin->sin_addr.s_addr = grp->gr_u.gt_net.nt_net;
			na++;
			break;
#ifdef ISO
		case GT_ISO:
			INIT_NETARG(na);
			*((struct sockaddr_iso *)sin) = *grp->gr_u.gt_isoaddr;
			imask->sin_len = 0;
			na++;
			break;
#endif	/* ISO */
		default:
			log(LOG_ERR, "Bad grouptype");
			free(netargs);
			return (1);
		}

		grp = grp->gr_next;
	}

	if (nfssvc(NFSSVC_EXPORT, &nxa)) {
		if (errno == EPERM) {
			log(LOG_ERR, "Can't change attributes for %s.  See 'exports' man page.", dir);
			free(netargs);
			return (1);
		}
		log(LOG_ERR, "Can't export %s: %s (%d)", dir, strerror(errno), errno);
		free(netargs);
		return (1);
	}

	free(netargs);
	return (0);
}

/*
 * Translate a net address.
 */
int
get_net(char *cp, struct netmsk *net, int maskflg)
{
	struct netent *np;
	long netaddr;
	struct in_addr inetaddr, inetaddr2;
	char *name;

	if (NULL != (np = getnetbyname(cp)))
		inetaddr = inet_makeaddr(np->n_net, 0);
	else if (isdigit(*cp)) {
		if ((netaddr = inet_network(cp)) == -1)
			return (1);
		inetaddr = inet_makeaddr(netaddr, 0);
		/*
		 * Due to arbritrary subnet masks, you don't know how many
		 * bits to shift the address to make it into a network,
		 * however you do know how to make a network address into
		 * a host with host == 0 and then compare them.
		 * (What a pest)
		 */
		if (!maskflg) {
			setnetent(0);
			while (NULL != (np = getnetent())) {
				inetaddr2 = inet_makeaddr(np->n_net, 0);
				if (inetaddr2.s_addr == inetaddr.s_addr)
					break;
			}
			endnetent();
		}
	} else
		return (1);
	if (maskflg)
		net->nt_mask = inetaddr.s_addr;
	else {
		if (np)
			name = np->n_name;
		else
			name = inet_ntoa(inetaddr);
		net->nt_name = malloc(strlen(name) + 1);
		if (net->nt_name == NULL) {
			log(LOG_ERR, "can't allocate memory for net: %s", cp);
			return (1);
		}
		strcpy(net->nt_name, name);
		net->nt_net = inetaddr.s_addr;
	}
	return (0);
}

/*
 * Find the next field in a line.
 * Fields are separated by white space.
 * Space and tab characters may be escaped.
 * Quoted strings are not broken at white space.
 */
void
nextfield(char **line_start, char **line_end)
{
	char *a, q;

	if (line_start == NULL)
		return;
	a = *line_start;

	/* Skip white space */
	while (*a == ' ' || *a == '\t')
		a++;
	*line_start = a;

	/* Stop at end of line */
	if (*a == '\n' || *a == '\0') {
		*line_end = a;
		return;
	}

	/* Check for single or double quote */
	if (*a == '\'' || *a == '"') {
		q = *a;
		a++;

		while (*a != q && *a != '\0' && *a != '\n')
			a++;
		if (*a == q)
			a++;
		*line_end = a;
		return;
	}

	/* Skip to next non-escaped white space or end of line */
	for (;; a++) {
		if (*a == '\0' || *a == '\n')
			break;
		else if (*a == '\\') {
			a++;
			if (*a == '\n' || *a == '\0')
				break;
		} else if (*a == ' ' || *a == '\t')
			break;
	}

	*line_end = a;
}

/* char *line is a global */
int
get_line(int source)
{
	if (source == EXPORT_FROM_NETINFO)
		return ni_get_line();
	return file_get_line();
}

/*
 * Get export entries from NetInfo and give it back to mountd a line
 * at a time.  Since mountd does all kinds of hacking on exports
 * at the same time that it parses the entries, it's a lot easier
 * replacing this get_line() than get_exportlist().
 */
static ni_idlist ni_exports_list;
static int ni_exports_index;
static void *ni;

void
ni_exports_open(void)
{
	int status;
	ni_id dir;

	status = ni_open(NULL, ".", &ni);
	if (status != NI_OK)
		log(LOG_ERR, "NetInfo open failed: %s", ni_error(status));

	NI_INIT(&ni_exports_list);
	ni_exports_index = 0;

	status = ni_pathsearch(ni, &dir, "/exports");
	if (status == NI_NODIR)
		return;
	if (status != NI_OK) {
		log(LOG_ERR, "NetInfo error searching for /exports: %s",
		    ni_error(status));
		return;
	}

	status = ni_children(ni, &dir, &ni_exports_list);
	if (status != NI_OK) {
		log(LOG_ERR, "NetInfo error reading /exports: %s",
		    ni_error(status));
		return;
	}
}

void
ni_exports_close(void)
{
	ni_free(ni);
	ni_idlist_free(&ni_exports_list);
	ni_exports_index = 0;
}

int
ni_get_line(void)
{
	ni_id dir;
	ni_proplist pl;
	ni_namelist *nl;
	ni_index where;
	int status, i;

	while (1) {
		line[0] = '\0';
		if (ni_exports_index >= ni_exports_list.ni_idlist_len)
			return (0);

		dir.nii_object = ni_exports_list.ni_idlist_val[ni_exports_index++];
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK) {
			log(LOG_ERR,
			    "NetInfo error reading /exports/dir:%lu: %s",
			    dir.nii_object, ni_error(status));
			continue;
		}
		where = ni_proplist_match(pl, "name", NULL);
		if (where == NI_INDEX_NULL) {
			log(LOG_ERR,
			    "NetInfo directory /exports/dir:%lu has no name",
			    dir.nii_object);
			continue;
		}
		if (pl.ni_proplist_val[where].nip_val.ni_namelist_len == 0) {
			log(LOG_ERR,
			    "NetInfo directory /exports/dir:%lu has no name",
			    dir.nii_object);
			continue;
		}
		nl = &pl.ni_proplist_val[where].nip_val;
		for (i = 0; i < nl->ni_namelist_len; i++) {
			strcat(line, nl->ni_namelist_val[i]);
			if (i + 1 < nl->ni_namelist_len)
				strcat(line, " ");
		}
		where = ni_proplist_match(pl, "opts", NULL);
		if (where != NI_INDEX_NULL &&
		    pl.ni_proplist_val[where].nip_val.ni_namelist_len > 0) {
			nl = &pl.ni_proplist_val[where].nip_val;
			for (i = 0; i < nl->ni_namelist_len; i++) {
				strcat(line, " -");
				strcat(line, nl->ni_namelist_val[i]);
			}
		}
		where = ni_proplist_match(pl, "clients", NULL);
		if (where != NI_INDEX_NULL &&
		    pl.ni_proplist_val[where].nip_val.ni_namelist_len > 0) {
			nl = &pl.ni_proplist_val[where].nip_val;
			for (i = 0; i < nl->ni_namelist_len; i++) {
				strcat(line, " ");
				strcat(line, nl->ni_namelist_val[i]);
			}
		}
		break;
	}
	return (1);
}

/*
 * Get an exports file line. Skip over blank lines and handle line
 * continuations.
 */
int
file_get_line(void)
{
	char *p, *cp;
	int len;
	int totlen, cont_line;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	p = line;
	totlen = 0;
	do {
		if (fgets(p, LINESIZ - totlen, exp_file) == NULL)
			return (0);
		len = strlen(p);
		cp = p + len - 1;
		cont_line = 0;
		while (cp >= p &&
		       (*cp == ' ' || *cp == '\t' || *cp == '\n' ||
			*cp == '\\')) {
			if (*cp == '\\')
				cont_line = 1;
			cp--;
			len--;
		}
		*++cp = '\0';
		if (len > 0) {
			totlen += len;
			if (totlen >= LINESIZ) {
				log(LOG_ERR, "Exports line too long");
				exit(2);
			}
			p = cp;
		}
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
void
parsecred(char *namelist, struct xucred *cr)
{
	char *name;
	int cnt;
	char *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups, groups[NGROUPS + 1];

	/*
	 * Set up the unpriviledged user.
	 */
	cr->cr_version = XUCRED_VERSION;
	cr->cr_uid = -2;
	cr->cr_groups[0] = -2;
	cr->cr_ngroups = 1;
	/*
	 * Get the user's password table entry.
	 */
	names = strsep(&namelist, " \t\n");
	name = strsep(&names, ":");
	if (isdigit(*name) || *name == '-')
		pw = getpwuid(atoi(name));
	else
		pw = getpwnam(name);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			log(LOG_ERR, "Unknown user: %s", name);
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups))
			log(LOG_ERR, "Too many groups");
		/*
		 * Convert from int's to gid_t's and compress out duplicate
		 */
		cr->cr_ngroups = ngroups - 1;
		cr->cr_groups[0] = groups[0];
		for (cnt = 2; cnt < ngroups; cnt++)
			cr->cr_groups[cnt - 1] = groups[cnt];
		return;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL)
		cr->cr_uid = pw->pw_uid;
	else if (isdigit(*name) || *name == '-')
		cr->cr_uid = atoi(name);
	else {
		log(LOG_ERR, "Unknown user: %s", name);
		return;
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS) {
		name = strsep(&names, ":");
		if (isdigit(*name) || *name == '-') {
			cr->cr_groups[cr->cr_ngroups++] = atoi(name);
		} else {
			if ((gr = getgrnam(name)) == NULL) {
				log(LOG_ERR, "Unknown group: %s", name);
				continue;
			}
			cr->cr_groups[cr->cr_ngroups++] = gr->gr_gid;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS)
		log(LOG_ERR, "Too many groups");
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
void
get_mountlist(void)
{
	struct mountlist *mlp, *lastmlp;
	char *host, *dir, *cp;
	char str[STRSIZ];
	FILE *mlfile;
	int hlen, dlen;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		if (errno != ENOENT)
			log(LOG_ERR, "Can't open %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
		else
			log(LOG_DEBUG, "Can't open %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
		return;
	}
	lastmlp = NULL;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dir = strsep(&cp, " \t\n");
		if ((host == NULL) || (dir == NULL))
			continue;
		hlen = strlen(host);
		if (hlen > RPCMNT_NAMELEN)
			hlen = RPCMNT_NAMELEN;
		dlen = strlen(dir);
		if (dlen > RPCMNT_PATHLEN)
			dlen = RPCMNT_PATHLEN;
		mlp = malloc(sizeof(*mlp));
		if (mlp) {
			mlp->ml_host = malloc(hlen+1);
			mlp->ml_dir = malloc(dlen+1);
		}
		if (!mlp || !mlp->ml_host || !mlp->ml_dir) {
			log(LOG_ERR, "can't allocate memory while reading in mount list: %s %s",
				host, dir);
			if (mlp) {
				if (mlp->ml_host)
					free(mlp->ml_host);
				if (mlp->ml_dir)
					free(mlp->ml_dir);
				free(mlp);
			}
			break;
		}
		strncpy(mlp->ml_host, host, hlen);
		mlp->ml_host[hlen] = '\0';
		strncpy(mlp->ml_dir, dir, dlen);
		mlp->ml_dir[dlen] = '\0';
		mlp->ml_next = NULL;
		if (lastmlp)
			lastmlp->ml_next = mlp;
		else
			mlhead = mlp;
		lastmlp = mlp;
	}
	fclose(mlfile);
}

void
del_mlist(char *host, char *dir)
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	FILE *mlfile;
	int fnd = 0;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, host) &&
		    (!dir || !strcmp(mlp->ml_dir, dir))) {
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free(mlp2->ml_host);
			free(mlp2->ml_dir);
			free(mlp2);
		} else {
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			log(LOG_ERR, "Can't write %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
			return;
		}
		mlp = mlhead;
		while (mlp) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dir);
			mlp = mlp->ml_next;
		}
		fclose(mlfile);
	}
}

void
add_mlist(char *host, char *dir)
{
	struct mountlist *mlp, **mlpp;
	FILE *mlfile;
	int hlen, dlen;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, host) && !strcmp(mlp->ml_dir, dir))
			return;
		mlpp = &mlp->ml_next;
		mlp = mlp->ml_next;
	}

	hlen = strlen(host);
	if (hlen > RPCMNT_NAMELEN)
		hlen = RPCMNT_NAMELEN;
	dlen = strlen(dir);
	if (dlen > RPCMNT_PATHLEN)
		dlen = RPCMNT_PATHLEN;

	mlp = malloc(sizeof(*mlp));
	if (mlp) {
		mlp->ml_host = malloc(hlen+1);
		mlp->ml_dir = malloc(dlen+1);
	}
	if (!mlp || !mlp->ml_host || !mlp->ml_dir) {
		if (mlp) {
			if (mlp->ml_host)
				free(mlp->ml_host);
			if (mlp->ml_dir)
				free(mlp->ml_dir);
			free(mlp);
		}
		log(LOG_ERR, "can't allocate memory to add to mount list: %s %s", host, dir);
		return;
	}
	strncpy(mlp->ml_host, host, hlen);
	mlp->ml_host[hlen] = '\0';
	strncpy(mlp->ml_dir, dir, dlen);
	mlp->ml_dir[dlen] = '\0';
	mlp->ml_next = NULL;
	*mlpp = mlp;
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		log(LOG_ERR, "Can't append %s: %s (%d)",
		    _PATH_RMOUNTLIST, strerror(errno), errno);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dir);
	fclose(mlfile);
}

#if 0
/*
 * This function is called via. SIGTERM when the system is going down.
 * It sends a broadcast RPCMNT_UMNTALL.
 * XXX This is really an NFS client call and should be moved out of mountd
 * XXX and moved into a client utility that gets called at startup before
 * XXX any NFS file systems are mounted.
 */
void
send_umntall(void)
{
	/* NULL callback tells it not to wait. */
	clnt_broadcast(RPCPROG_MNT, RPCMNT_VER1, RPCMNT_UMNTALL,
			      xdr_void, (caddr_t)0, xdr_void, (caddr_t)0,
			      NULL);
	exit(0);
}
#endif

/*
 * Free up a group list.
 */
void
free_grp(struct grouplist *grp)
{
	char **addrp;

	if (grp->gr_type == GT_HOST) {
		if (grp->gr_u.gt_hostent->h_name) {
			addrp = grp->gr_u.gt_hostent->h_addr_list;
			while (addrp && *addrp)
				free(*addrp++);
			if (grp->gr_u.gt_hostent->h_addr_list)
				free((caddr_t)grp->gr_u.gt_hostent->h_addr_list);
			free(grp->gr_u.gt_hostent->h_name);
		}
		free((caddr_t)grp->gr_u.gt_hostent);
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_u.gt_net.nt_name)
			free(grp->gr_u.gt_net.nt_name);
	}
#ifdef ISO
	else if (grp->gr_type == GT_ISO)
		free((caddr_t)grp->gr_u.gt_isoaddr);
#endif
	free((caddr_t)grp);
}

void
SYSLOG(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (debug > 1) {
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
		fflush(stderr);
	} else if (debug || (pri != LOG_DEBUG)) {
		vsyslog(pri, fmt, ap);
	}
	va_end(ap);
}

/*
 * Check options for consistency.
 */
int
check_options(int opt_flags)
{
	if ((opt_flags & (OP_MAPROOT | OP_MAPALL)) == (OP_MAPROOT|OP_MAPALL) ||
	    (opt_flags & (OP_MAPROOT | OP_KERB)) == (OP_MAPROOT | OP_KERB) ||
	    (opt_flags & (OP_MAPALL | OP_KERB)) == (OP_MAPALL | OP_KERB)) {
		log(LOG_ERR, "-mapall, -maproot and -kerb mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		log(LOG_ERR, "-mask requires -net");
		return (1);
	}
	if ((opt_flags & (OP_NET | OP_ISO)) == (OP_NET | OP_ISO)) {
		log(LOG_ERR, "-net and -iso mutually exclusive");
		return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 * if no symbolic links are found.
 */
int
check_dirpath(char *dir)
{
	char *cp;
	int ret = 1;
	struct stat sb;

	for (cp = dir + 1; *cp && ret; cp++)
		if (*cp == '/') {
			*cp = '\0';
			if ((lstat(dir, &sb) < 0) || !S_ISDIR(sb.st_mode))
				ret = 0;
			*cp = '/';
		}
	if (ret && ((lstat(dir, &sb) < 0) || !S_ISDIR(sb.st_mode)))
		ret = 0;
	return (ret);
}

