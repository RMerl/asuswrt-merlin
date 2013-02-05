/*
 * nfsmount.c -- Linux NFS mount
 * Copyright (C) 1993 Rick Sladkey <jrs@world.std.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Wed Feb  8 12:51:48 1995, biro@yggdrasil.com (Ross Biro): allow all port
 * numbers to be specified on the command line.
 *
 * Fri, 8 Mar 1996 18:01:39, Swen Thuemmler <swen@uni-paderborn.de>:
 * Omit the call to connect() for Linux version 1.3.11 or later.
 *
 * Wed Oct  1 23:55:28 1997: Dick Streefland <dick_streefland@tasking.com>
 * Implemented the "bg", "fg" and "retry" mount options for NFS.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * Modified by Olaf Kirch and Trond Myklebust for new NFS code,
 * plus NFSv3 stuff.
 *
 * 2006-06-06 Amit Gud <agud@redhat.com>
 * - Moved with modifcations to nfs-utils/utils/mount from util-linux/mount.
 */

/*
 * nfsmount.c,v 1.1.1.1 1993/11/18 08:40:51 jrs Exp
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <sys/mount.h>
#include <paths.h>
#include <syslog.h>

#include "xcommon.h"
#include "mount.h"
#include "nfs_mount.h"
#include "mount_constants.h"
#include "nls.h"
#include "error.h"
#include "network.h"
#include "version.h"

#ifdef HAVE_RPCSVC_NFS_PROT_H
#include <rpcsvc/nfs_prot.h>
#else
#include <linux/nfs.h>
#define nfsstat nfs_stat
#endif

#ifndef NFS_PORT
#define NFS_PORT 2049
#endif
#ifndef NFS_FHSIZE
#define NFS_FHSIZE 32
#endif

#ifndef HAVE_INET_ATON
#define inet_aton(a,b) (0)
#endif

typedef dirpath mnt2arg_t;
typedef dirpath mnt3arg_t;
typedef dirpath mntarg_t;

typedef struct fhstatus  mnt2res_t;
typedef struct mountres3 mnt3res_t;
typedef union {
	mnt2res_t nfsv2;
	mnt3res_t nfsv3;
} mntres_t;

extern int nfs_mount_data_version;
extern char *progname;
extern int verbose;
extern int sloppy;

static inline enum clnt_stat
nfs3_mount(CLIENT *clnt, mnt3arg_t *mnt3arg, mnt3res_t *mnt3res)
{
	return clnt_call(clnt, MOUNTPROC3_MNT,
			 (xdrproc_t) xdr_dirpath, (caddr_t) mnt3arg,
			 (xdrproc_t) xdr_mountres3, (caddr_t) mnt3res,
			 TIMEOUT);
}

static inline enum clnt_stat
nfs2_mount(CLIENT *clnt, mnt2arg_t *mnt2arg, mnt2res_t *mnt2res)
{
	return clnt_call(clnt, MOUNTPROC_MNT,
			 (xdrproc_t) xdr_dirpath, (caddr_t) mnt2arg,
			 (xdrproc_t) xdr_fhstatus, (caddr_t) mnt2res,
			 TIMEOUT);
}

static int
nfs_call_mount(clnt_addr_t *mnt_server, clnt_addr_t *nfs_server,
	       mntarg_t *mntarg, mntres_t *mntres)
{
	CLIENT *clnt;
	enum clnt_stat stat;
	int msock;

	if (!probe_bothports(mnt_server, nfs_server))
		goto out_bad;

	clnt = mnt_openclnt(mnt_server, &msock);
	if (!clnt)
		goto out_bad;
	/* make pointers in xdr_mountres3 NULL so
	 * that xdr_array allocates memory for us
	 */
	memset(mntres, 0, sizeof(*mntres));
	switch (mnt_server->pmap.pm_vers) {
	case 3:
		stat = nfs3_mount(clnt, mntarg, &mntres->nfsv3);
		break;
	case 2:
	case 1:
		stat = nfs2_mount(clnt, mntarg, &mntres->nfsv2);
		break;
	default:
		goto out_bad;
	}
	if (stat != RPC_SUCCESS) {
		clnt_geterr(clnt, &rpc_createerr.cf_error);
		rpc_createerr.cf_stat = stat;
	}
	mnt_closeclnt(clnt, msock);
	if (stat == RPC_SUCCESS)
		return 1;
 out_bad:
	return 0;
}

static int
parse_options(char *old_opts, struct nfs_mount_data *data,
	      int *bg, int *retry, clnt_addr_t *mnt_server,
	      clnt_addr_t *nfs_server, char *new_opts, const int opt_size)
{
	struct sockaddr_in *mnt_saddr = &mnt_server->saddr;
	struct pmap *mnt_pmap = &mnt_server->pmap;
	struct pmap *nfs_pmap = &nfs_server->pmap;
	int len;
	char *opt, *opteq, *p, *opt_b;
	char *mounthost = NULL;
	char cbuf[128];
	int open_quote = 0;

	data->flags = 0;
	*bg = 0;

	len = strlen(new_opts);
	for (p=old_opts, opt_b=NULL; p && *p; p++) {
		if (!opt_b)
			opt_b = p;		/* begin of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in a quoted block */
		if (*p == ',')
			*p = '\0';		/* terminate the option item */
		if (*p == '\0' || *(p+1) == '\0') {
			opt = opt_b;		/* opt is useful now */
			opt_b = NULL;
		}
		else
			continue;		/* still somewhere in the option item */

		if (strlen(opt) >= sizeof(cbuf))
			goto bad_parameter;
		if ((opteq = strchr(opt, '=')) && isdigit(opteq[1])) {
			int val = atoi(opteq + 1);	
			*opteq = '\0';
			if (!strcmp(opt, "rsize"))
				data->rsize = val;
			else if (!strcmp(opt, "wsize"))
				data->wsize = val;
			else if (!strcmp(opt, "timeo"))
				data->timeo = val;
			else if (!strcmp(opt, "retrans"))
				data->retrans = val;
			else if (!strcmp(opt, "acregmin"))
				data->acregmin = val;
			else if (!strcmp(opt, "acregmax"))
				data->acregmax = val;
			else if (!strcmp(opt, "acdirmin"))
				data->acdirmin = val;
			else if (!strcmp(opt, "acdirmax"))
				data->acdirmax = val;
			else if (!strcmp(opt, "actimeo")) {
				data->acregmin = val;
				data->acregmax = val;
				data->acdirmin = val;
				data->acdirmax = val;
			}
			else if (!strcmp(opt, "retry"))
				*retry = val;
			else if (!strcmp(opt, "port"))
				nfs_pmap->pm_port = val;
			else if (!strcmp(opt, "mountport"))
			        mnt_pmap->pm_port = val;
			else if (!strcmp(opt, "mountprog"))
				mnt_pmap->pm_prog = val;
			else if (!strcmp(opt, "mountvers"))
				mnt_pmap->pm_vers = val;
			else if (!strcmp(opt, "mounthost"))
				mounthost=xstrndup(opteq+1, strcspn(opteq+1," \t\n\r,"));
			else if (!strcmp(opt, "nfsprog"))
				nfs_pmap->pm_prog = val;
			else if (!strcmp(opt, "nfsvers") ||
				 !strcmp(opt, "vers")) {
				nfs_pmap->pm_vers = val;
				opt = "nfsvers";
#if NFS_MOUNT_VERSION >= 2
			} else if (!strcmp(opt, "namlen")) {
				if (nfs_mount_data_version >= 2)
					data->namlen = val;
				else if (sloppy)
					continue;
				else
					goto bad_parameter;
#endif
			} else if (!strcmp(opt, "addr")) {
				/* ignore */;
				continue;
 			} else if (sloppy)
				continue;
			else
				goto bad_parameter;
			sprintf(cbuf, "%s=%s,", opt, opteq+1);
		} else if (opteq) {
			*opteq = '\0';
			if (!strcmp(opt, "proto")) {
				if (!strcmp(opteq+1, "udp")) {
					nfs_pmap->pm_prot = IPPROTO_UDP;
					mnt_pmap->pm_prot = IPPROTO_UDP;
#if NFS_MOUNT_VERSION >= 2
					data->flags &= ~NFS_MOUNT_TCP;
				} else if (!strcmp(opteq+1, "tcp") &&
					   nfs_mount_data_version > 2) {
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					data->flags |= NFS_MOUNT_TCP;
#endif
				} else if (sloppy)
					continue;
				else
					goto bad_parameter;
#if NFS_MOUNT_VERSION >= 5
			} else if (!strcmp(opt, "sec")) {
				char *secflavor = opteq+1;
				/* see RFC 2623 */
				if (nfs_mount_data_version < 5) {
					printf(_("Warning: ignoring sec=%s option\n"),
							secflavor);
					continue;
				} else if (!strcmp(secflavor, "none"))
					data->pseudoflavor = AUTH_NONE;
				else if (!strcmp(secflavor, "sys"))
					data->pseudoflavor = AUTH_SYS;
				else if (!strcmp(secflavor, "krb5"))
					data->pseudoflavor = AUTH_GSS_KRB5;
				else if (!strcmp(secflavor, "krb5i"))
					data->pseudoflavor = AUTH_GSS_KRB5I;
				else if (!strcmp(secflavor, "krb5p"))
					data->pseudoflavor = AUTH_GSS_KRB5P;
				else if (!strcmp(secflavor, "lipkey"))
					data->pseudoflavor = AUTH_GSS_LKEY;
				else if (!strcmp(secflavor, "lipkey-i"))
					data->pseudoflavor = AUTH_GSS_LKEYI;
				else if (!strcmp(secflavor, "lipkey-p"))
					data->pseudoflavor = AUTH_GSS_LKEYP;
				else if (!strcmp(secflavor, "spkm3"))
					data->pseudoflavor = AUTH_GSS_SPKM;
				else if (!strcmp(secflavor, "spkm3i"))
					data->pseudoflavor = AUTH_GSS_SPKMI;
				else if (!strcmp(secflavor, "spkm3p"))
					data->pseudoflavor = AUTH_GSS_SPKMP;
				else if (sloppy)
					continue;
				else {
					printf(_("Warning: Unrecognized security flavor %s.\n"),
						secflavor);
					goto bad_parameter;
				}
				data->flags |= NFS_MOUNT_SECFLAVOUR;
#endif
			} else if (!strcmp(opt, "mounthost"))
			        mounthost=xstrndup(opteq+1,
						   strcspn(opteq+1," \t\n\r,"));
			 else if (!strcmp(opt, "context")) {
				char *context = opteq + 1;
				int ctxlen = strlen(context);

				if (ctxlen > NFS_MAX_CONTEXT_LEN) {
					nfs_error(_("context parameter exceeds"
							" limit of %d"),
							NFS_MAX_CONTEXT_LEN);
					goto bad_parameter;
				}
				/* The context string is in the format of
				 * "system_u:object_r:...".  We only want
				 * the context str between the quotes.
				 */
				if (*context == '"')
					strncpy(data->context, context+1,
							ctxlen-2);
				else
					strncpy(data->context, context,
							NFS_MAX_CONTEXT_LEN);
 			} else if (sloppy)
				continue;
			else
				goto bad_parameter;
			sprintf(cbuf, "%s=%s,", opt, opteq+1);
		} else {
			int val = 1;
			if (!strncmp(opt, "no", 2)) {
				val = 0;
				opt += 2;
			}
			if (!strcmp(opt, "bg"))
				*bg = val;
			else if (!strcmp(opt, "fg"))
				*bg = !val;
			else if (!strcmp(opt, "soft")) {
				data->flags &= ~NFS_MOUNT_SOFT;
				if (val)
					data->flags |= NFS_MOUNT_SOFT;
			} else if (!strcmp(opt, "hard")) {
				data->flags &= ~NFS_MOUNT_SOFT;
				if (!val)
					data->flags |= NFS_MOUNT_SOFT;
			} else if (!strcmp(opt, "intr")) {
				data->flags &= ~NFS_MOUNT_INTR;
				if (val)
					data->flags |= NFS_MOUNT_INTR;
			} else if (!strcmp(opt, "posix")) {
				data->flags &= ~NFS_MOUNT_POSIX;
				if (val)
					data->flags |= NFS_MOUNT_POSIX;
			} else if (!strcmp(opt, "cto")) {
				data->flags &= ~NFS_MOUNT_NOCTO;
				if (!val)
					data->flags |= NFS_MOUNT_NOCTO;
			} else if (!strcmp(opt, "ac")) {
				data->flags &= ~NFS_MOUNT_NOAC;
				if (!val)
					data->flags |= NFS_MOUNT_NOAC;
#if NFS_MOUNT_VERSION >= 2
			} else if (!strcmp(opt, "tcp")) {
				data->flags &= ~NFS_MOUNT_TCP;
				if (val) {
					if (nfs_mount_data_version < 2)
						goto bad_option;
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					data->flags |= NFS_MOUNT_TCP;
				} else {
					mnt_pmap->pm_prot = IPPROTO_UDP;
					nfs_pmap->pm_prot = IPPROTO_UDP;
				}
			} else if (!strcmp(opt, "udp")) {
				data->flags &= ~NFS_MOUNT_TCP;
				if (!val) {
					if (nfs_mount_data_version < 2)
						goto bad_option;
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					data->flags |= NFS_MOUNT_TCP;
				} else {
					nfs_pmap->pm_prot = IPPROTO_UDP;
					mnt_pmap->pm_prot = IPPROTO_UDP;
				}
#endif
#if NFS_MOUNT_VERSION >= 3
			} else if (!strcmp(opt, "lock")) {
				data->flags &= ~NFS_MOUNT_NONLM;
				if (!val) {
					if (nfs_mount_data_version < 3)
						goto bad_option;
					data->flags |= NFS_MOUNT_NONLM;
				}
#endif
#if NFS_MOUNT_VERSION >= 4
			} else if (!strcmp(opt, "broken_suid")) {
				data->flags &= ~NFS_MOUNT_BROKEN_SUID;
				if (val) {
					if (nfs_mount_data_version < 4)
						goto bad_option;
					data->flags |= NFS_MOUNT_BROKEN_SUID;
				}
			} else if (!strcmp(opt, "acl")) {
				data->flags &= ~NFS_MOUNT_NOACL;
				if (!val)
					data->flags |= NFS_MOUNT_NOACL;
			} else if (!strcmp(opt, "rdirplus")) {
				data->flags &= ~NFS_MOUNT_NORDIRPLUS;
				if (!val)
					data->flags |= NFS_MOUNT_NORDIRPLUS;
			} else if (!strcmp(opt, "sharecache")) {
				data->flags &= ~NFS_MOUNT_UNSHARED;
				if (!val)
					data->flags |= NFS_MOUNT_UNSHARED;
#endif
			} else {
			bad_option:
				if (sloppy)
					continue;
				nfs_error(_("%s: Unsupported nfs mount option:"
						" %s%s"), progname,
						val ? "" : "no", opt);
				goto out_bad;
			}
			sprintf(cbuf, val ? "%s," : "no%s,", opt);
		}
		len += strlen(cbuf);
		if (len >= opt_size) {
			nfs_error(_("%s: excessively long option argument"),
					progname);
			goto out_bad;
		}
		strcat(new_opts, cbuf);
	}
	/* See if the nfs host = mount host. */
	if (mounthost) {
		if (!nfs_gethostbyname(mounthost, mnt_saddr))
			goto out_bad;
		*mnt_server->hostname = mounthost;
	}
	return 1;
 bad_parameter:
	nfs_error(_("%s: Bad nfs mount parameter: %s\n"), progname, opt);
 out_bad:
	return 0;
}

static int nfsmnt_check_compat(const struct pmap *nfs_pmap,
				const struct pmap *mnt_pmap)
{
	unsigned int max_nfs_vers = (nfs_mount_data_version >= 4) ? 3 : 2;
	unsigned int max_mnt_vers = (nfs_mount_data_version >= 4) ? 3 : 2;

	if (nfs_pmap->pm_vers == 4) {
		nfs_error(_("%s: Please use '-t nfs4' "
				"instead of '-o vers=4'"), progname);
		goto out_bad;
	}

	if (nfs_pmap->pm_vers) {
		if (nfs_pmap->pm_vers > max_nfs_vers || nfs_pmap->pm_vers < 2) {
			nfs_error(_("%s: NFS version %ld is not supported"),
					progname, nfs_pmap->pm_vers);
			goto out_bad;
		}
	}

	if (mnt_pmap->pm_vers > max_mnt_vers) {
		nfs_error(_("%s: NFS mount version %ld is not supported"),
				progname, mnt_pmap->pm_vers);
		goto out_bad;
	}

	return 1;

out_bad:
	return 0;
}

int
nfsmount(const char *spec, const char *node, int flags,
	 char **extra_opts, int fake, int running_bg)
{
	char hostdir[1024];
	char *hostname, *dirname, *old_opts, *mounthost = NULL;
	char new_opts[1024], cbuf[1024];
	static struct nfs_mount_data data;
	int val;
	static int doonce = 0;

	clnt_addr_t mnt_server = { &mounthost, };
	clnt_addr_t nfs_server = { &hostname, };
	struct sockaddr_in *nfs_saddr = &nfs_server.saddr;
	struct pmap  *mnt_pmap = &mnt_server.pmap,
		     *nfs_pmap = &nfs_server.pmap;
	struct pmap  save_mnt, save_nfs;

	int fsock = -1;

	mntres_t mntres;

	struct stat statbuf;
	char *s;
	int bg, retry;
	int retval = EX_FAIL;
	time_t t;
	time_t prevt;
	time_t timeout;

	if (strlen(spec) >= sizeof(hostdir)) {
		nfs_error(_("%s: excessively long host:dir argument"),
				progname);
		goto fail;
	}
	strcpy(hostdir, spec);
	if ((s = strchr(hostdir, ':'))) {
		hostname = hostdir;
		dirname = s + 1;
		*s = '\0';
		/* Ignore all but first hostname in replicated mounts
		   until they can be fully supported. (mack@sgi.com) */
		if ((s = strchr(hostdir, ','))) {
			*s = '\0';
			nfs_error(_("%s: warning: "
				  "multiple hostnames not supported"),
					progname);
		}
	} else {
		nfs_error(_("%s: directory to mount not in host:dir format"),
				progname);
		goto fail;
	}

	if (!nfs_gethostbyname(hostname, nfs_saddr))
		goto fail;
	mounthost = hostname;
	memcpy (&mnt_server.saddr, nfs_saddr, sizeof (mnt_server.saddr));

	/* add IP address to mtab options for use when unmounting */

	s = inet_ntoa(nfs_saddr->sin_addr);
	old_opts = *extra_opts;
	if (!old_opts)
		old_opts = "";

	/* Set default options.
	 * rsize/wsize (and bsize, for ver >= 3) are left 0 in order to
	 * let the kernel decide.
	 * timeo is filled in after we know whether it'll be TCP or UDP. */
	memset(&data, 0, sizeof(data));
	data.acregmin	= 3;
	data.acregmax	= 60;
	data.acdirmin	= 30;
	data.acdirmax	= 60;
#if NFS_MOUNT_VERSION >= 2
	data.namlen	= NAME_MAX;
#endif

	bg = 0;
	retry = -1;

	memset(mnt_pmap, 0, sizeof(*mnt_pmap));
	mnt_pmap->pm_prog = MOUNTPROG;
	memset(nfs_pmap, 0, sizeof(*nfs_pmap));
	nfs_pmap->pm_prog = NFS_PROGRAM;

	/* parse options */
	new_opts[0] = 0;
	if (!parse_options(old_opts, &data, &bg, &retry, &mnt_server, &nfs_server,
			   new_opts, sizeof(new_opts)))
		goto fail;
	if (!nfsmnt_check_compat(nfs_pmap, mnt_pmap))
		goto fail;

	if (retry == -1) {
		if (bg)
			retry = 10000;	/* 10000 mins == ~1 week*/
		else
			retry = 2;	/* 2 min default on fg mounts */
	}

#ifdef NFS_MOUNT_DEBUG
	printf(_("rsize = %d, wsize = %d, timeo = %d, retrans = %d\n"),
	       data.rsize, data.wsize, data.timeo, data.retrans);
	printf(_("acreg (min, max) = (%d, %d), acdir (min, max) = (%d, %d)\n"),
	       data.acregmin, data.acregmax, data.acdirmin, data.acdirmax);
	printf(_("port = %lu, bg = %d, retry = %d, flags = %.8x\n"),
	       nfs_pmap->pm_port, bg, retry, data.flags);
	printf(_("mountprog = %lu, mountvers = %lu, nfsprog = %lu, nfsvers = %lu\n"),
	       mnt_pmap->pm_prog, mnt_pmap->pm_vers,
	       nfs_pmap->pm_prog, nfs_pmap->pm_vers);
	printf(_("soft = %d, intr = %d, posix = %d, nocto = %d, noac = %d"),
	       (data.flags & NFS_MOUNT_SOFT) != 0,
	       (data.flags & NFS_MOUNT_INTR) != 0,
	       (data.flags & NFS_MOUNT_POSIX) != 0,
	       (data.flags & NFS_MOUNT_NOCTO) != 0,
	       (data.flags & NFS_MOUNT_NOAC) != 0);
#if NFS_MOUNT_VERSION >= 2
	printf(_(", tcp = %d"),
	       (data.flags & NFS_MOUNT_TCP) != 0);
#endif
#if NFS_MOUNT_VERSION >= 4
	printf(_(", noacl = %d"), (data.flags & NFS_MOUNT_NOACL) != 0);
#endif
#if NFS_MOUNT_VERSION >= 5
	printf(_(", sec = %u"), data.pseudoflavor);
	printf(_(", readdirplus = %d"), (data.flags & NFS_MOUNT_NORDIRPLUS) != 0);
#endif
	printf("\n");
#endif

	data.version = nfs_mount_data_version;

	if (flags & MS_REMOUNT)
		goto out_ok;

	/* create mount deamon client */

	/*
	 * The following loop implements the mount retries. On the first
	 * call, "running_bg" is 0. When the mount times out, and the
	 * "bg" option is set, the exit status EX_BG will be returned.
	 * For a backgrounded mount, there will be a second call by the
	 * child process with "running_bg" set to 1.
	 *
	 * The case where the mount point is not present and the "bg"
	 * option is set, is treated as a timeout. This is done to
	 * support nested mounts.
	 *
	 * The "retry" count specified by the user is the number of
	 * minutes to retry before giving up.
	 *
	 * Only the first error message will be displayed.
	 */
	timeout = time(NULL) + 60 * retry;
	prevt = 0;
	t = 30;
	val = 1;

	memcpy(&save_nfs, nfs_pmap, sizeof(save_nfs));
	memcpy(&save_mnt, mnt_pmap, sizeof(save_mnt));
	for (;;) {
		if (bg && stat(node, &statbuf) == -1) {
			/* no mount point yet - sleep */
			if (running_bg) {
				sleep(val);	/* 1, 2, 4, 8, 16, 30, ... */
				val *= 2;
				if (val > 30)
					val = 30;
			}
		} else {
			int stat;
			/* be careful not to use too many CPU cycles */
			if (t - prevt < 30)
				sleep(30);

			stat = nfs_call_mount(&mnt_server, &nfs_server,
					      &dirname, &mntres);
			if (stat)
				break;
			memcpy(nfs_pmap, &save_nfs, sizeof(*nfs_pmap));
			memcpy(mnt_pmap, &save_mnt, sizeof(*mnt_pmap));
			prevt = t;
		}
		if (!bg) {
			switch(rpc_createerr.cf_stat){
			case RPC_TIMEDOUT:
				break;
			case RPC_SYSTEMERROR:
				if (errno == ETIMEDOUT)
					break;
			default:
				rpc_mount_errors(*nfs_server.hostname, 0, bg);
		        goto fail;
			}
			t = time(NULL);
			if (t >= timeout) {
				rpc_mount_errors(*nfs_server.hostname, 0, bg);
				goto fail;
			}
			rpc_mount_errors(*nfs_server.hostname, 1, bg);
			continue;
		}
		if (!running_bg) {
			if (retry > 0)
				retval = EX_BG;
			goto fail;
		}
		t = time(NULL);
		if (t >= timeout) {
			rpc_mount_errors(*nfs_server.hostname, 0, bg);
			goto fail;
		}
		if (doonce++ < 1)
			rpc_mount_errors(*nfs_server.hostname, 1, bg);
	}

	if (mnt_pmap->pm_vers <= 2) {
		if (mntres.nfsv2.fhs_status != 0) {
			nfs_error(_("%s: %s:%s failed, reason given by server: %s"),
					progname, hostname, dirname,
					nfs_strerror(mntres.nfsv2.fhs_status));
			goto fail;
		}
		memcpy(data.root.data,
		       (char *) mntres.nfsv2.fhstatus_u.fhs_fhandle,
		       NFS_FHSIZE);
#if NFS_MOUNT_VERSION >= 4
		data.root.size = NFS_FHSIZE;
		memcpy(data.old_root.data,
		       (char *) mntres.nfsv2.fhstatus_u.fhs_fhandle,
		       NFS_FHSIZE);
#endif
	} else {
#if NFS_MOUNT_VERSION >= 4
		mountres3_ok *mountres;
		fhandle3 *fhandle;
		int i,  n_flavors, *flavor, yum = 0;
		if (mntres.nfsv3.fhs_status != 0) {
			nfs_error(_("%s: %s:%s failed, reason given by server: %s"),
					progname, hostname, dirname,
					nfs_strerror(mntres.nfsv3.fhs_status));
			goto fail;
		}
#if NFS_MOUNT_VERSION >= 5
		mountres = &mntres.nfsv3.mountres3_u.mountinfo;
		n_flavors = mountres->auth_flavors.auth_flavors_len;
		if (n_flavors <= 0)
			goto noauth_flavors;

		flavor = mountres->auth_flavors.auth_flavors_val;
		for (i = 0; i < n_flavors; ++i) {
			/*
			 * Per RFC2623, section 2.7, we should prefer the
			 * flavour listed first.
			 * If no flavour requested, use the first simple
			 * flavour that is offered.
			 */
			if (! (data.flags & NFS_MOUNT_SECFLAVOUR) &&
			    (flavor[i] == AUTH_SYS ||
			     flavor[i] == AUTH_NONE)) {
				data.pseudoflavor = flavor[i];
				data.flags |= NFS_MOUNT_SECFLAVOUR;
			}
			if (flavor[i] == data.pseudoflavor)
				yum = 1;
#ifdef NFS_MOUNT_DEBUG
			printf(_("auth flavor %d: %d\n"), i, flavor[i]);
#endif
		}
		if (!yum) {
			nfs_error(_("%s: %s:%s failed, security flavor "
					"not supported"),
					progname, hostname, dirname);
			/* server has registered us in rmtab, send umount */
			nfs_call_umount(&mnt_server, &dirname);
			goto fail;
		}
noauth_flavors:
#endif
		fhandle = &mntres.nfsv3.mountres3_u.mountinfo.fhandle;
		memset(data.old_root.data, 0, NFS_FHSIZE);
		memset(&data.root, 0, sizeof(data.root));
		data.root.size = fhandle->fhandle3_len;
		memcpy(data.root.data,
		       (char *) fhandle->fhandle3_val,
		       fhandle->fhandle3_len);

		data.flags |= NFS_MOUNT_VER3;
#endif
	}

	if (nfs_mount_data_version == 1) {
		/* create nfs socket for kernel */
		if (nfs_pmap->pm_prot == IPPROTO_TCP)
			fsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		else
			fsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (fsock < 0) {
			perror(_("nfs socket"));
			goto fail;
		}
		if (bindresvport(fsock, 0) < 0) {
			perror(_("nfs bindresvport"));
			goto fail;
		}
	}

#ifdef NFS_MOUNT_DEBUG
	printf(_("using port %lu for nfs deamon\n"), nfs_pmap->pm_port);
#endif
	nfs_saddr->sin_port = htons(nfs_pmap->pm_port);
	/*
	 * connect() the socket for kernels 1.3.10 and below only,
	 * to avoid problems with multihomed hosts.
	 * --Swen
	 */
	if (linux_version_code() <= MAKE_VERSION(1, 3, 10) && fsock != -1
	    && connect(fsock, (struct sockaddr *) nfs_saddr,
		       sizeof (*nfs_saddr)) < 0) {
		perror(_("nfs connect"));
		goto fail;
	}

#if NFS_MOUNT_VERSION >= 2
	if (nfs_pmap->pm_prot == IPPROTO_TCP)
		data.flags |= NFS_MOUNT_TCP;
	else
		data.flags &= ~NFS_MOUNT_TCP;
#endif

	/* prepare data structure for kernel */

	data.fd = fsock;
	memcpy((char *) &data.addr, (char *) nfs_saddr, sizeof(data.addr));
	strncpy(data.hostname, hostname, sizeof(data.hostname));

 out_ok:
	/* Ensure we have enough padding for the following strcat()s */
	if (strlen(new_opts) + strlen(s) + 30 >= sizeof(new_opts)) {
		nfs_error(_("%s: excessively long option argument"),
				progname);
		goto fail;
	}

	snprintf(cbuf, sizeof(cbuf)-1, "addr=%s", s);
	strcat(new_opts, cbuf);

	*extra_opts = xstrdup(new_opts);

	if (!fake && !(data.flags & NFS_MOUNT_NONLM)) {
		if (!start_statd()) {
			nfs_error(_("%s: rpc.statd is not running but is "
				"required for remote locking.\n"
				"   Either use '-o nolock' to keep "
				"locks local, or start statd."),
					progname);
			goto fail;
		}
	}

	if (!fake) {
		if (mount(spec, node, "nfs",
				flags & ~(MS_USER|MS_USERS), &data)) {
			mount_error(spec, node, errno);
			goto fail;
		}
	}

	return EX_SUCCESS;

	/* abort */
 fail:
	if (fsock != -1)
		close(fsock);
	return retval;
}
