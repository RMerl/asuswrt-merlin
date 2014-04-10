/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
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
/*-
 * Copyright (c) 1980, 1989, 1993
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
 */


#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

typedef enum { MNTON, MNTFROM } mntwhat;

int	fake, fflag, vflag;
char	*nfshost;

uid_t real_uid, eff_uid;

int	 checkvfsname(const char *, char **);
char	*getmntname(const char *, mntwhat, char **);
int	 getmntfsid(const char *, fsid_t *);
int	 sysctl_fsid(int, fsid_t *, void *, size_t *, void *, size_t);
int	 unmount_by_fsid(const char *mntpt, int flag);
char	**makevfslist(char *);
int	 selected(int);
int	 namematch(struct hostent *);
int	 umountall(char **);
int	 umountfs(char *, char **);
void	 usage(void);
int	 xdr_dir(XDR *, char *);

int
main(int argc, char *argv[])
{
	int all, ch, errs, mnts;
	char **typelist = NULL;
	struct statfs *mntbuf;

	/* drop setuid root privs asap */
	eff_uid = geteuid();
	real_uid = getuid();
	seteuid(real_uid); 

	/* Start disks transferring immediately. */
	sync();

	all = 0;
	while ((ch = getopt(argc, argv, "AaFfh:t:v")) != EOF)
		switch (ch) {
		case 'A':
			all = 2;
			break;
		case 'a':
			all = 1;
			break;
		case 'F':
			fake = 1;
			break;
		case 'f':
			fflag = MNT_FORCE;
			break;
		case 'h':	/* -h implies -A. */
			all = 2;
			nfshost = optarg;
			break;
		case 't':
			if (typelist != NULL)
				errx(1, "only one -t option may be specified.");
			typelist = makevfslist(optarg);
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc == 0 && !all || argc != 0 && all)
		usage();

	/* -h implies "-t nfs" if no -t flag. */
	if ((nfshost != NULL) && (typelist == NULL))
		typelist = makevfslist("nfs");

	if (fflag & MNT_FORCE) {
		/*
		 * If we really mean business, we don't want to get hung up on
		 * any remote file systems.  So, we set the "noremotehang" flag.
		 */
		pid_t pid;
		pid = getpid();
		seteuid(eff_uid);
		errs = sysctlbyname("vfs.generic.noremotehang", NULL, NULL, &pid, sizeof(pid));
		seteuid(real_uid);
		if ((errs != 0) && vflag)
		        warn("sysctl vfs.generic.noremotehang");
	}

	errs = EXIT_SUCCESS;
	switch (all) {
	case 2:
		if ((mnts = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
			warn("getmntinfo");
			errs = 1;
			break;
		}
		for (errs = 0, mnts--; mnts > 0; mnts--) {
			if (checkvfsname(mntbuf[mnts].f_fstypename, typelist))
				continue;
			if (umountfs(mntbuf[mnts].f_mntonname, typelist) != 0)
				errs = 1;
		}
		break;
	case 1:
		if (setfsent() == 0)
			err(1, "%s", _PATH_FSTAB);
		errs = umountall(typelist);
		break;
	case 0:
		for (errs = 0; *argv != NULL; ++argv)
			if (umountfs(*argv, typelist) != 0)
				errs = 1;
		break;
	}
	exit(errs);
}

int
umountall(char **typelist)
{
	struct fstab *fs;
	int rval;
	char *cp;
	struct vfsconf vfc;

	while ((fs = getfsent()) != NULL) {
		/* Ignore the root. */
		if (strcmp(fs->fs_file, "/") == 0)
			continue;
		/*
		 * !!!
		 * Historic practice: ignore unknown FSTAB_* fields.
		 */
		if (strcmp(fs->fs_type, FSTAB_RW) &&
		    strcmp(fs->fs_type, FSTAB_RO) &&
		    strcmp(fs->fs_type, FSTAB_RQ))
			continue;
		/* If an unknown file system type, complain. */
		if (getvfsbyname(fs->fs_vfstype, &vfc) < 0) {
			warnx("%s: unknown mount type", fs->fs_vfstype);
			continue;
		}
		if (checkvfsname(fs->fs_vfstype, typelist))
			continue;

		/* 
		 * We want to unmount the file systems in the reverse order
		 * that they were mounted.  So, we save off the file name
		 * in some allocated memory, and then call recursively.
		 */
		if ((cp = malloc((size_t)strlen(fs->fs_file) + 1)) == NULL)
			err(1, NULL);
		(void)strcpy(cp, fs->fs_file);
		rval = umountall(typelist);
		return (umountfs(cp, typelist) || rval);
	}
	return (0);
}

int
umountfs(char *name, char **typelist)
{
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct sockaddr_in saddr;
	struct stat sb;
	struct timeval pertry, try;
	CLIENT *clp;
	int so, isftpfs;
	char *type, *delimp, *hostp, *mntpt, rname[MAXPATHLEN], *expname, *tname;

	if (fflag & MNT_FORCE) {
		/*
		 * For force unmounts, we first directly check the
		 * current mount list for a match.  If we find it,
		 * we skip the realpath()/stat() below to avoid
		 * depending on the "noremotehang" flag to save us
		 * if we get hung up on an unresponsive file system.
		 */
		tname = name;
		/* check if name is a non-device "mount from" name */
		if ((mntpt = getmntname(tname, MNTON, &type)) == NULL) {
			/* or if name is a mounted-on directory */
			mntpt = tname;
			tname = getmntname(mntpt, MNTFROM, &type);
		}
		if (mntpt && tname) {
			/* we found a match */
			name = tname;
			goto got_mount_point;
		}
	}

	/*
	 * Note: in the face of path resolution errors (realpath/stat),
	 * we just try using the name passed in as is.
	 */

	if (realpath(name, rname) == NULL) {
		if (vflag)
			warn("realpath(%s)", rname);
	} else {
		name = rname;
	}

	if (stat(name, &sb) < 0) {
		if (vflag)
			warn("stat(%s)", name);
		/* maybe name is a non-device "mount from" name? */
		if ((mntpt = getmntname(name, MNTON, &type)) == NULL) {
			/* or name is a directory we simply can't reach? */
			mntpt = name;
			if ((name = getmntname(mntpt, MNTFROM, &type)) == NULL) {
				warnx("%s: not currently mounted", mntpt);
				return (1);
			}
		}
	} else if (S_ISBLK(sb.st_mode)) {
		if ((mntpt = getmntname(name, MNTON, &type)) == NULL) {
			warnx("%s: not currently mounted", name);
			return (1);
		}
	} else if (S_ISDIR(sb.st_mode)) {
		mntpt = name;
		if ((name = getmntname(mntpt, MNTFROM, &type)) == NULL) {
			warnx("%s: not currently mounted", mntpt);
			return (1);
		}
	} else {
		warnx("%s: not a directory or special device", name);
		return (1);
	}

got_mount_point:

	if (checkvfsname(type, typelist))
		return (1);

	if (!strncmp("ftp://", name, 6))
		isftpfs = 1;
	else
		isftpfs = 0;

	hp = NULL;
	delimp = NULL;
	expname = NULL;
	if (!strcmp(type, "nfs") && !isftpfs) {
		if ((delimp = strchr(name, '@')) != NULL) {
			hostp = delimp + 1;
			*delimp = '\0';
			hp = gethostbyname(hostp);
			*delimp = '@';
			expname = name;
		} else if ((delimp = strchr(name, ':')) != NULL) {
			*delimp = '\0';
			hostp = name;
			hp = gethostbyname(hostp);
			expname = delimp + 1;
			*delimp = ':';
		}
	}

	if (!namematch(hp))
		return (1);

	if (vflag)
		(void)printf("%s unmount from %s\n", name, mntpt);
	if (fake)
		return (0);

	if (unmount(mntpt, fflag) < 0) {
		/*
		 * If we're root and it looks like the error is that the
		 * mounted on directory is just not reachable or if we really
		 * want this filesystem unmounted (MNT_FORCE), then try doing
		 * the unmount by fsid.  (Note: the sysctl only works for root)
		 */
		if ((real_uid == 0) &&
		    ((errno == ESTALE) || (errno == ENOENT) || (fflag & MNT_FORCE))) {
			if (vflag)
				warn("unmount(%s)", mntpt);
			if (unmount_by_fsid(mntpt, fflag) < 0) {
				warn("unmount(%s)", mntpt);
				return (1);
			}
		} else {
			warn("unmount(%s)", mntpt);
			return (1);
		}
	}

	if ((hp != NULL) && !(fflag & MNT_FORCE)) {
		*delimp = '\0';
		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = 0;
		memmove(&saddr.sin_addr, hp->h_addr, hp->h_length);
		pertry.tv_sec = 3;
		pertry.tv_usec = 0;
		so = RPC_ANYSOCK;
		/*
		 * temporarily revert to root, to avoid reserved port
		 * number restriction (port# less than 1024)
		 */
		seteuid(eff_uid);
		if ((clp = clntudp_create(&saddr,
		    RPCPROG_MNT, RPCMNT_VER1, pertry, &so)) == NULL) {
			clnt_pcreateerror("Cannot MNT PRC");
			seteuid(real_uid);
			/* unmount succeeded above, so don't actually return error */
			return (0);
		}
		clp->cl_auth = authunix_create_default();
		seteuid(real_uid);
		try.tv_sec = 20;
		try.tv_usec = 0;
		clnt_stat = clnt_call(clp,
		    RPCMNT_UMOUNT, xdr_dir, expname, xdr_void, (caddr_t)0, try);
		if (clnt_stat != RPC_SUCCESS) {
			clnt_perror(clp, "Bad MNT RPC");
			/* unmount succeeded above, so don't actually return error */
			return (0);
		}
		auth_destroy(clp->cl_auth);
		clnt_destroy(clp);
	}
	return (0);
}

static struct statfs *mntbuf;
static int mntsize;

char *
getmntname(const char *name, mntwhat what, char **type)
{
	int i;

	if (mntbuf == NULL &&
	    (mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return (NULL);
	}
	for (i = 0; i < mntsize; i++) {
		if ((what == MNTON) && !strcmp(mntbuf[i].f_mntfromname, name)) {
			if (type)
				*type = mntbuf[i].f_fstypename;
			return (mntbuf[i].f_mntonname);
		}
		if ((what == MNTFROM) && !strcmp(mntbuf[i].f_mntonname, name)) {
			if (type)
				*type = mntbuf[i].f_fstypename;
			return (mntbuf[i].f_mntfromname);
		}
	}
	return (NULL);
}

int
getmntfsid(const char *name, fsid_t *fsid)
{
	int i;

	if (mntbuf == NULL &&
	    (mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return (-1);
	}
	for (i = 0; i < mntsize; i++) {
		if (!strcmp(mntbuf[i].f_mntonname, name)) {
			*fsid = mntbuf[i].f_fsid;
			return (0);
		}
	}
	return (-1);
}

int
namematch(struct hostent *hp)
{
	char *cp, **np;

	if (nfshost == NULL)
		return (1);

	if (hp == NULL)
		return (0);

	if (strcasecmp(nfshost, hp->h_name) == 0)
		return (1);

	if ((cp = strchr(hp->h_name, '.')) != NULL) {
		*cp = '\0';
		if (strcasecmp(nfshost, hp->h_name) == 0)
			return (1);
	}
	for (np = hp->h_aliases; *np; np++) {
		if (strcasecmp(nfshost, *np) == 0)
			return (1);
		if ((cp = strchr(*np, '.')) != NULL) {
			*cp = '\0';
			if (strcasecmp(nfshost, *np) == 0)
				return (1);
		}
	}
	return (0);
}


int
sysctl_fsid(
	int op,
	fsid_t *fsid,
	void *oldp,
	size_t *oldlenp,
	void *newp,
	size_t newlen)
{
	int ctlname[CTL_MAXNAME+2];
	size_t ctllen;
	const char *sysstr = "vfs.generic.ctlbyfsid";
	struct vfsidctl vc;

	ctllen = CTL_MAXNAME+2;
	if (sysctlnametomib(sysstr, ctlname, &ctllen) == -1) {
		warn("sysctlnametomib(%s)", sysstr);
		return (-1);
	};
	ctlname[ctllen] = op;

	bzero(&vc, sizeof(vc));
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = *fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return (sysctl(ctlname, ctllen + 1, oldp, oldlenp, &vc, sizeof(vc)));
}


int
unmount_by_fsid(const char *mntpt, int flag)
{
	fsid_t fsid;
	if (getmntfsid(mntpt, &fsid) < 0)
		return (-1);
	if (vflag)
		printf("attempting to unmount %s by fsid\n", mntpt);
	return sysctl_fsid(VFS_CTL_UMOUNT, &fsid, NULL, 0, &flag, sizeof(flag));
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s\n       %s\n",
	    "umount [-fv] [-t fstypelist] special | node",
	    "umount -a[fv] [-h host] [-t fstypelist]");
	exit(1);
}
