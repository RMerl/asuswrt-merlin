/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)edquota.c	8.3 (Berkeley) 4/27/95";
#endif /* not lint */

/*
 * Disk quota editor.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef __APPLE__
#include <sys/mount.h>
#endif /* __APPLE__ */
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/quota.h>
#include <errno.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#endif /* __APPLE__ */

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
char *quotagroup = QUOTAGROUP;
char tmpfil[] = _PATH_TMP;

#ifdef __APPLE__
u_int32_t quotamagic[MAXQUOTAS] = INITQMAGICS;
#endif /* __APPLE__ */

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN + 1];
	char	qfname[1];	/* actually longer */
} *getprivs();
#define	FOUND	0x01

int	alldigits __P((char *));
int	cvtatos __P((time_t, char *, time_t *));
int	editit __P((char *));
void	freeprivs __P((struct quotause *));
int	getentry __P((char *, int));
int	hasquota __P((struct statfs *, int, char **));
void	putprivs __P((long, int, struct quotause *));
int	readprivs __P((struct quotause *, int));
int	readtimes __P((struct quotause *, int));
void	usage __P((void));
int	writeprivs __P((struct quotause *, int, char *, int));
int	writetimes __P((struct quotause *, int, int));

#ifdef __APPLE__
int	qfinit(int, struct statfs *, int);
int	qflookup(int, u_long, int, struct dqblk *);
int	qfupdate(int, u_long, int, struct dqblk *);
#endif /* __APPLE__ */


int
main(argc, argv)
	register char **argv;
	int argc;
{
	register struct quotause *qup, *protoprivs, *curprivs;
	extern char *optarg;
	extern int optind;
	register long id, protoid;
	register int quotatype, tmpfd;
	char *protoname = NULL, ch;
	int tflag = 0, pflag = 0;

	if (argc < 2)
		usage();
	if (getuid()) {
		fprintf(stderr, "edquota: permission denied\n");
		exit(1);
	}
	quotatype = USRQUOTA;

	while ((ch = getopt(argc, argv, "ugtp:")) != EOF) {
		switch(ch) {
		case 'p':
			protoname = optarg;
			pflag++;
			break;
		case 'g':
			quotatype = GRPQUOTA;
			break;
		case 'u':
			quotatype = USRQUOTA;
			break;
		case 't':
			tflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (pflag) {
		if ((protoid = getentry(protoname, quotatype)) == -1)
			exit(1);
		protoprivs = getprivs(protoid, quotatype);
#ifdef __APPLE__
		if (protoprivs == (struct quotause *) NULL)
			exit(0);
#endif /* __APPLE__ */
		for (qup = protoprivs; qup; qup = qup->next) {
			qup->dqblk.dqb_btime = 0;
			qup->dqblk.dqb_itime = 0;
		}
		while (argc-- > 0) {
			if ((id = getentry(*argv++, quotatype)) < 0)
				continue;
			putprivs(id, quotatype, protoprivs);
		}
		exit(0);
	}
	tmpfd = mkstemp(tmpfil);
	fchown(tmpfd, getuid(), getgid());
	if (tflag) {
		protoprivs = getprivs(0, quotatype);
#ifdef __APPLE__
		if (protoprivs == (struct quotause *) NULL)
		  exit(0);
#endif /* __APPLE__ */
		if (writetimes(protoprivs, tmpfd, quotatype) == 0)
			exit(1);
		if (editit(tmpfil)) {
			/*
			 * Re-open tmpfil to be editor independent.
			 */
			close(tmpfd);
			tmpfd = open(tmpfil, O_RDWR, 0);
			if (tmpfd < 0) {
				freeprivs(protoprivs);
				unlink(tmpfil);
				exit(1);
			}
			if (readtimes(protoprivs, tmpfd))
				putprivs(0, quotatype, protoprivs);
		}
		freeprivs(protoprivs);
		exit(0);
	}
	for ( ; argc > 0; argc--, argv++) {
		if ((id = getentry(*argv, quotatype)) == -1)
			continue;
		curprivs = getprivs(id, quotatype);
#ifdef __APPLE__
		if (curprivs == (struct quotause *) NULL)
		  exit(0);
#endif /* __APPLE__ */


		if (writeprivs(curprivs, tmpfd, *argv, quotatype) == 0) {
			freeprivs(curprivs);
			continue;
		}
		if (editit(tmpfil)) {
			/*
			 * Re-open tmpfil to be editor independent.
			 */
			close(tmpfd);
			tmpfd = open(tmpfil, O_RDWR, 0);
			if (tmpfd < 0) {
				freeprivs(curprivs);
				unlink(tmpfil);
				exit(1);
			}
			if (readprivs(curprivs, tmpfd))
				putprivs(id, quotatype, curprivs);
		}
		freeprivs(curprivs);
	}
	close(tmpfd);
	unlink(tmpfil);
	exit(0);
}

void
usage()
{
	fprintf(stderr, "%s%s%s%s",
		"Usage: edquota [-u] [-p username] username ...\n",
		"\tedquota -g [-p groupname] groupname ...\n",
		"\tedquota [-u] -t\n", "\tedquota -g -t\n");
#ifdef __APPLE__
	fprintf(stderr, "\nQuota file editing triggers only on filesystems with a\n");
	fprintf(stderr, "%s.%s or %s.%s file located at its root.\n", 
		QUOTAOPSNAME, qfextension[USRQUOTA],
		QUOTAOPSNAME, qfextension[GRPQUOTA]);
#endif /* __APPLE__ */
	exit(1);
}

/*
 * This routine converts a name for a particular quota type to
 * an identifier. This routine must agree with the kernel routine
 * getinoquota as to the interpretation of quota types.
 */
int
getentry(name, quotatype)
	char *name;
	int quotatype;
{
	struct passwd *pw;
	struct group *gr;

	if (alldigits(name))
		return (atoi(name));
	switch(quotatype) {
	case USRQUOTA:
		if (pw = getpwnam(name))
			return (pw->pw_uid);
		fprintf(stderr, "%s: no such user\n", name);
		break;
	case GRPQUOTA:
		if (gr = getgrnam(name))
			return (gr->gr_gid);
		fprintf(stderr, "%s: no such group\n", name);
		break;
	default:
		fprintf(stderr, "%d: unknown quota type\n", quotatype);
		break;
	}
	sleep(1);
	return (-1);
}

/*
 * Collect the requested quota information.
 */
#ifdef __APPLE__
struct quotause *
getprivs(id, quotatype)
	register long id;
	int quotatype;
{
	struct statfs *fst;
	register struct quotause *qup, *quptail;
	struct quotause *quphead;
	int qcmd, qupsize, fd;
	char *qfpathname;
	static int warned = 0;
	int nfst, i;
	extern int errno;


	quptail = quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);

	nfst = getmntinfo(&fst, MNT_WAIT);
        if (nfst==0) {
	  fprintf(stderr, "edquota: no mounted filesystems\n");
	  exit(1);
        }

	for (i=0; i<nfst; i++) {
	        if (strcmp(fst[i].f_fstypename, "hfs")) {
		    if (strcmp(fst[i].f_fstypename, "ufs"))
		        continue;
		}
		if (!hasquota(&fst[i], quotatype, &qfpathname))
			continue;
		qupsize = sizeof(*qup) + strlen(qfpathname);
		if ((qup = (struct quotause *)malloc(qupsize)) == NULL) {
			fprintf(stderr, "edquota: out of memory\n");
			exit(2);
		}
		if (quotactl(fst[i].f_mntonname, qcmd, id, (char *)&qup->dqblk) != 0) {
	    		if (errno == EOPNOTSUPP && !warned) {
				warned++;
				fprintf(stderr, "Warning: %s\n",
				    "Quotas are not compiled into this kernel");
				sleep(3);
			}
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
				if (fd < 0 && errno != ENOENT) {
					perror(qfpathname);
					free(qup);
					continue;
				}
				fprintf(stderr, "Creating quota file %s\n",
				    qfpathname);
				sleep(3);
				(void) fchown(fd, getuid(),
				    getentry(quotagroup, GRPQUOTA));
				(void) fchmod(fd, 0640);
				if (qfinit(fd, &fst[i], quotatype)) {	
					perror(qfpathname);
					close(fd);
					free(qup);
					continue;
				}
			}
			if (qflookup(fd, id, quotatype, &qup->dqblk) != 0) {
				fprintf(stderr, "edquota: lookup error in ");
				perror(qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->qfname, qfpathname);
		strcpy(qup->fsname, fst[i].f_mntonname);

		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		qup->next = 0;
	}
	return (quphead);
}
#else
struct quotause *
getprivs(id, quotatype)
	register long id;
	int quotatype;
{
	register struct fstab *fs;
	register struct quotause *qup, *quptail;
	struct quotause *quphead;
	int qcmd, qupsize, fd;
	char *qfpathname;
	static int warned = 0;
	extern int errno;

	setfsent();
	quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);
	while (fs = getfsent()) {
		if (strcmp(fs->fs_vfstype, "ufs"))
			continue;
		if (!hasquota(fs, quotatype, &qfpathname))
			continue;
		qupsize = sizeof(*qup) + strlen(qfpathname);
		if ((qup = (struct quotause *)malloc(qupsize)) == NULL) {
			fprintf(stderr, "edquota: out of memory\n");
			exit(2);
		}
		if (quotactl(fs->fs_file, qcmd, id, &qup->dqblk) != 0) {
	    		if (errno == EOPNOTSUPP && !warned) {
				warned++;
				fprintf(stderr, "Warning: %s\n",
				    "Quotas are not compiled into this kernel");
				sleep(3);
			}
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
				if (fd < 0 && errno != ENOENT) {
					perror(qfpathname);
					free(qup);
					continue;
				}
				fprintf(stderr, "Creating quota file %s\n",
				    qfpathname);
				sleep(3);
				(void) fchown(fd, getuid(),
				    getentry(quotagroup, GRPQUOTA));
				(void) fchmod(fd, 0640);
			}
			lseek(fd, (off_t)(id * sizeof(struct dqblk)), L_SET);
			switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
			case 0:			/* EOF */
				/*
				 * Convert implicit 0 quota (EOF)
				 * into an explicit one (zero'ed dqblk)
				 */
				bzero((caddr_t)&qup->dqblk,
				    sizeof(struct dqblk));
				break;

			case sizeof(struct dqblk):	/* OK */
				break;

			default:		/* ERROR */
				fprintf(stderr, "edquota: read error in ");
				perror(qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->qfname, qfpathname);
		strcpy(qup->fsname, fs->fs_file);
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		qup->next = 0;
	}
	endfsent();
	return (quphead);
}
#endif /* __APPLE */

#ifdef __APPLE__
#define ONEGIGABYTE        (1024*1024*1024)
/*
 * Initialize a new quota file.
 */
int
qfinit(fd, fst, type)
	int fd;
	struct statfs *fst;
	int type;
{
	struct dqfilehdr dqhdr = {0};
	u_int64_t fs_size;
	int max = 0;

	/*
	 * Calculate the size of the hash table from the size of
	 * the file system.  Note that the open addressing hashing
	 * used by the quota file assumes that this table will not
	 * be more than 90% full.
	 */
	fs_size = (u_int64_t)fst->f_blocks * (u_int64_t)fst->f_bsize;

	if (type == USRQUOTA) {
		max = QF_USERS_PER_GB * (fs_size / ONEGIGABYTE);

		if (max < QF_MIN_USERS)
			max = QF_MIN_USERS;
		else if (max > QF_MAX_USERS)
			max = QF_MAX_USERS;
	} else if (type == GRPQUOTA) {
		max = QF_GROUPS_PER_GB * (fs_size / ONEGIGABYTE);

		if (max < QF_MIN_GROUPS)
			max = QF_MIN_GROUPS;
		else if (max > QF_MAX_GROUPS)
			max = QF_MAX_GROUPS;
	}
	/* Round up to a power of 2 */
	if (max && !powerof2(max)) {
		int x = max;
		max = 4;
		while (x>>1 != 1) {
			x = x >> 1;
			max = max << 1;
		}
	}

	(void) ftruncate(fd, (off_t)((max + 1) * sizeof(struct dqblk)));
	dqhdr.dqh_magic      = OSSwapHostToBigInt32(quotamagic[type]);
	dqhdr.dqh_version    = OSSwapHostToBigConstInt32(QF_VERSION);
	dqhdr.dqh_maxentries = OSSwapHostToBigInt32(max);
	dqhdr.dqh_btime      = OSSwapHostToBigConstInt32(MAX_DQ_TIME);
	dqhdr.dqh_itime      = OSSwapHostToBigConstInt32(MAX_IQ_TIME);
	memmove(dqhdr.dqh_string, QF_STRING_TAG, strlen(QF_STRING_TAG));
	(void) lseek(fd, 0, L_SET);
	(void) write(fd, &dqhdr, sizeof(dqhdr));

	return (0);
}

/*
 * Lookup an entry in a quota file.
 */
int
qflookup(fd, id, type, dqbp)
	int fd;
	u_long id;
	int type;
	struct dqblk *dqbp;
{
	struct dqfilehdr dqhdr;
	int i, skip, last, m;
	u_long mask;

	bzero(dqbp, sizeof(struct dqblk));

	if (id == 0)
		return (0);

	lseek(fd, 0, L_SET);
	if (read(fd, &dqhdr, sizeof(struct dqfilehdr)) != sizeof(struct dqfilehdr))
		return (-1);

	/* Sanity check the quota file header. */
	if ((OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type]) ||
	    (OSSwapBigToHostInt32(dqhdr.dqh_version) > 1) ||
	    (!powerof2(OSSwapBigToHostInt32(dqhdr.dqh_maxentries)))) {
		fprintf(stderr, "quota: invalid quota file header\n");
		return (-1);
	}

	m = OSSwapBigToHostInt32(dqhdr.dqh_maxentries);
	mask = m - 1;
	i = dqhash1(id, dqhashshift(m), mask);
	skip = dqhash2(id, mask);

	for (last = (i + (m-1) * skip) & mask;
	     i != last;
	     i = (i + skip) & mask) {
		lseek(fd, dqoffset(i), L_SET);
		if (read(fd, dqbp, sizeof(struct dqblk)) < sizeof(struct dqblk))
			return (-1);
		/*
		 * Stop when an empty entry is found
		 * or we encounter a matching id.
		 */
		if (dqbp->dqb_id == 0 || OSSwapBigToHostInt32(dqbp->dqb_id) == id)
			break;
	}
	/* Put data in host native byte order. */
	dqbp->dqb_bhardlimit = OSSwapBigToHostInt64(dqbp->dqb_bhardlimit);
	dqbp->dqb_bsoftlimit = OSSwapBigToHostInt64(dqbp->dqb_bsoftlimit);
	dqbp->dqb_curbytes   = OSSwapBigToHostInt64(dqbp->dqb_curbytes);
	dqbp->dqb_ihardlimit = OSSwapBigToHostInt32(dqbp->dqb_ihardlimit);
	dqbp->dqb_isoftlimit = OSSwapBigToHostInt32(dqbp->dqb_isoftlimit);
	dqbp->dqb_curinodes  = OSSwapBigToHostInt32(dqbp->dqb_curinodes);
	dqbp->dqb_btime      = OSSwapBigToHostInt32(dqbp->dqb_btime);
	dqbp->dqb_itime      = OSSwapBigToHostInt32(dqbp->dqb_itime);
	dqbp->dqb_id         = OSSwapBigToHostInt32(dqbp->dqb_id);

	return (0);
}
#endif /* __APPLE */


/*
 * Store the requested quota information.
 */
void
putprivs(id, quotatype, quplist)
	long id;
	int quotatype;
	struct quotause *quplist;
{
	register struct quotause *qup;
	int qcmd, fd;

	qcmd = QCMD(Q_SETQUOTA, quotatype);
	for (qup = quplist; qup; qup = qup->next) {
		if (quotactl(qup->fsname, qcmd, id, (char *)&qup->dqblk) == 0)
			continue;
#ifdef __APPLE__
		if ((fd = open(qup->qfname, O_RDWR)) < 0) {
			perror(qup->qfname);
		} else {
			if (qfupdate(fd, id, quotatype, &qup->dqblk) != 0) {
				fprintf(stderr, "edquota: ");
				perror(qup->qfname);
			}
#else
		if ((fd = open(qup->qfname, O_WRONLY)) < 0) {
			perror(qup->qfname);
		} else {
			lseek(fd,
			    (off_t)(id * (long)sizeof (struct dqblk)), L_SET);
			if (write(fd, &qup->dqblk, sizeof (struct dqblk)) !=
			    sizeof (struct dqblk)) {
				fprintf(stderr, "edquota: ");
				perror(qup->qfname);
			}
#endif /* __APPLE */
			close(fd);
		}
	}
}

#ifdef __APPLE__
/*
 * Update an entry in a quota file.
 */
int
qfupdate(fd, id, type, dqbp)
	int fd;
	u_long id;
	int type;
	struct dqblk *dqbp;
{
	struct dqblk dqbuf;
	struct dqfilehdr dqhdr;
	int i, skip, last, m;
	u_long mask;

	if (id == 0)
		return (0);

	lseek(fd, 0, L_SET);
	if (read(fd, &dqhdr, sizeof(struct dqfilehdr)) != sizeof(struct dqfilehdr))
		return (-1);

	/* Sanity check the quota file header. */
	if ((OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type]) ||
	    (OSSwapBigToHostInt32(dqhdr.dqh_version) > QF_VERSION) ||
	    (!powerof2(OSSwapBigToHostInt32(dqhdr.dqh_maxentries)))) {
		fprintf(stderr, "quota: invalid quota file header\n");
		return (EINVAL);
	}

	m = OSSwapBigToHostInt32(dqhdr.dqh_maxentries);
	mask = m - 1;
	i = dqhash1(id, dqhashshift(m), mask);
	skip = dqhash2(id, mask);

	for (last = (i + (m-1) * skip) & mask;
	     i != last;
	     i = (i + skip) & mask) {
		lseek(fd, dqoffset(i), L_SET);
		if (read(fd, &dqbuf, sizeof(struct dqblk)) < sizeof(struct dqblk))
			return (-1);
		/*
		 * Stop when an empty entry is found
		 * or we encounter a matching id.
		 */
		if (dqbuf.dqb_id == 0 || OSSwapBigToHostInt32(dqbuf.dqb_id) == id) {

			/* Convert buffer to big endian before writing. */
			dqbp->dqb_bhardlimit = OSSwapHostToBigInt64(dqbp->dqb_bhardlimit);
			dqbp->dqb_bsoftlimit = OSSwapHostToBigInt64(dqbp->dqb_bsoftlimit);
			dqbp->dqb_curbytes   = OSSwapHostToBigInt64(dqbp->dqb_curbytes);
			dqbp->dqb_ihardlimit = OSSwapHostToBigInt32(dqbp->dqb_ihardlimit);
			dqbp->dqb_isoftlimit = OSSwapHostToBigInt32(dqbp->dqb_isoftlimit);
			dqbp->dqb_curinodes  = OSSwapHostToBigInt32(dqbp->dqb_curinodes);
			dqbp->dqb_btime      = OSSwapHostToBigInt32(dqbp->dqb_btime);
			dqbp->dqb_itime      = OSSwapHostToBigInt32(dqbp->dqb_itime);
			dqbp->dqb_id         = OSSwapHostToBigInt32(id);

			lseek(fd, dqoffset(i), L_SET);
			if (write(fd, dqbp, sizeof (struct dqblk)) !=
			    sizeof (struct dqblk)) {
			    	return (-1);
			}
			return (0);
		}
	}
	errno = ENOSPC;
	return (-1);
}
#endif /* __APPLE__ */

/*
 * Take a list of priviledges and get it edited.
 */
int
editit(tmpfile)
	char *tmpfile;
{
	long omask;
	int pid, stat;
	extern char *getenv();

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
 top:
	if ((pid = fork()) < 0) {
		extern int errno;

		if (errno == EPROCLIM) {
			fprintf(stderr, "You have too many processes\n");
			return(0);
		}
		if (errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		perror("fork");
		return (0);
	}
	if (pid == 0) {
		register char *ed;

		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = _PATH_VI;
		execlp(ed, ed, tmpfile, NULL);
		perror(ed);
		exit(1);
	}
	waitpid(pid, &stat, 0);
	sigsetmask(omask);
	if (!WIFEXITED(stat) || WEXITSTATUS(stat) != 0)
		return (0);
	return (1);
}

/*
 * Convert a quotause list to an ASCII file.
 */
int
writeprivs(quplist, outfd, name, quotatype)
	struct quotause *quplist;
	int outfd;
	char *name;
	int quotatype;
{
	register struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, L_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL) {
		fprintf(stderr, "edquota: ");
		perror(tmpfil);
		exit(1);
	}
	fprintf(fd, "Quotas for %s %s:\n", qfextension[quotatype], name);
	for (qup = quplist; qup; qup = qup->next) {
#ifdef __APPLE__
	        fprintf(fd, "%s: %s %qd, limits (soft = %qd, hard = %qd)\n",
		    qup->fsname, "1K blocks in use:",
		    qup->dqblk.dqb_curbytes / 1024,
		    qup->dqblk.dqb_bsoftlimit / 1024,
		    qup->dqblk.dqb_bhardlimit / 1024);
#else
		fprintf(fd, "%s: %s %d, limits (soft = %d, hard = %d)\n",
		    qup->fsname, "blocks in use:",
		    dbtob(qup->dqblk.dqb_curblocks) / 1024,
		    dbtob(qup->dqblk.dqb_bsoftlimit) / 1024,
		    dbtob(qup->dqblk.dqb_bhardlimit) / 1024);
#endif /* __APPLE__ */

		fprintf(fd, "%s %d, limits (soft = %d, hard = %d)\n",
		    "\tinodes in use:", qup->dqblk.dqb_curinodes,
		    qup->dqblk.dqb_isoftlimit, qup->dqblk.dqb_ihardlimit);
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes to an ASCII file into a quotause list.
 */
int
readprivs(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	register struct quotause *qup;
	FILE *fd;
	int cnt;
	register char *cp;
	struct dqblk dqblk;
	char *fsp, line1[BUFSIZ], line2[BUFSIZ];

	lseek(infd, 0, L_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		fprintf(stderr, "Can't re-read temp file!!\n");
		return (0);
	}
	/*
	 * Discard title line, then read pairs of lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL &&
	       fgets(line2, sizeof (line2), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			fprintf(stderr, "%s: bad format\n", line1);
			return (0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			fprintf(stderr, "%s: %s: bad format\n", fsp,
			    &fsp[strlen(fsp) + 1]);
			return (0);
		}
#ifdef __APPLE__
		/* We expect input to be in 1K blocks */
		cnt = sscanf(cp,
			       " 1K blocks in use: %qd, limits (soft = %qd, hard = %qd)",
			       &dqblk.dqb_curbytes, &dqblk.dqb_bsoftlimit,
			       &dqblk.dqb_bhardlimit);
		if (cnt != 3) {
			fprintf(stderr, "%s:%s: bad format\n", fsp, cp);
			return (0);
		}

		/* convert default 1K blocks to byte count */
		dqblk.dqb_curbytes = dqblk.dqb_curbytes * 1024;
		dqblk.dqb_bsoftlimit = dqblk.dqb_bsoftlimit * 1024;
		dqblk.dqb_bhardlimit = dqblk.dqb_bhardlimit * 1024;
#else
		cnt = sscanf(cp,
		    " blocks in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curblocks, &dqblk.dqb_bsoftlimit,
		    &dqblk.dqb_bhardlimit);
		if (cnt != 3) {
			fprintf(stderr, "%s:%s: bad format\n", fsp, cp);
			return (0);
		}
		dqblk.dqb_curblocks = btodb(dqblk.dqb_curblocks * 1024);
		dqblk.dqb_bsoftlimit = btodb(dqblk.dqb_bsoftlimit * 1024);
		dqblk.dqb_bhardlimit = btodb(dqblk.dqb_bhardlimit * 1024);
#endif /* __APPLE__ */

		if ((cp = strtok(line2, "\n")) == NULL) {
			fprintf(stderr, "%s: %s: bad format\n", fsp, line2);
			return (0);
		}
		cnt = sscanf(cp,
		    "\tinodes in use: %d, limits (soft = %d, hard = %d)",
		    &dqblk.dqb_curinodes, &dqblk.dqb_isoftlimit,
		    &dqblk.dqb_ihardlimit);
		if (cnt != 3) {
			fprintf(stderr, "%s: %s: bad format\n", fsp, line2);
			return (0);
		}
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			/*
			 * Cause time limit to be reset when the quota
			 * is next used if previously had no soft limit
			 * or were under it, but now have a soft limit
			 * and are over it.
			 */
#ifdef __APPLE__
			if (dqblk.dqb_bsoftlimit &&
			    qup->dqblk.dqb_curbytes >= dqblk.dqb_bsoftlimit &&
			    (qup->dqblk.dqb_bsoftlimit == 0 ||
			     qup->dqblk.dqb_curbytes <
			     qup->dqblk.dqb_bsoftlimit))
				qup->dqblk.dqb_btime = 0;
#else
			if (dqblk.dqb_bsoftlimit &&
			    qup->dqblk.dqb_curblocks >= dqblk.dqb_bsoftlimit &&
			    (qup->dqblk.dqb_bsoftlimit == 0 ||
			     qup->dqblk.dqb_curblocks <
			     qup->dqblk.dqb_bsoftlimit))
				qup->dqblk.dqb_btime = 0;
#endif /* __APPLE__ */
			if (dqblk.dqb_isoftlimit &&
			    qup->dqblk.dqb_curinodes >= dqblk.dqb_isoftlimit &&
			    (qup->dqblk.dqb_isoftlimit == 0 ||
			     qup->dqblk.dqb_curinodes <
			     qup->dqblk.dqb_isoftlimit))
				qup->dqblk.dqb_itime = 0;
			qup->dqblk.dqb_bsoftlimit = dqblk.dqb_bsoftlimit;
			qup->dqblk.dqb_bhardlimit = dqblk.dqb_bhardlimit;
			qup->dqblk.dqb_isoftlimit = dqblk.dqb_isoftlimit;
			qup->dqblk.dqb_ihardlimit = dqblk.dqb_ihardlimit;
			qup->flags |= FOUND;
#ifdef __APPLE__
			if (dqblk.dqb_curbytes == qup->dqblk.dqb_curbytes &&
			    dqblk.dqb_curinodes == qup->dqblk.dqb_curinodes)
				break;
#else
			if (dqblk.dqb_curblocks == qup->dqblk.dqb_curblocks &&
			    dqblk.dqb_curinodes == qup->dqblk.dqb_curinodes)
				break;
#endif /* __APPLE__ */
			fprintf(stderr,
			    "%s: cannot change current allocation\n", fsp);
			break;
		}
	}
	fclose(fd);
	/*
	 * Disable quotas for any filesystems that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_bsoftlimit = 0;
		qup->dqblk.dqb_bhardlimit = 0;
		qup->dqblk.dqb_isoftlimit = 0;
		qup->dqblk.dqb_ihardlimit = 0;
	}
	return (1);
}

/*
 * Convert a quotause list to an ASCII file of grace times.
 */
int
writetimes(quplist, outfd, quotatype)
	struct quotause *quplist;
	int outfd;
	int quotatype;
{
	register struct quotause *qup;
	char *cvtstoa();
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, L_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL) {
		fprintf(stderr, "edquota: ");
		perror(tmpfil);
		exit(1);
	}
	fprintf(fd, "Time units may be: days, hours, minutes, or seconds\n");
	fprintf(fd, "Grace period before enforcing soft limits for %ss:\n",
	    qfextension[quotatype]);
	for (qup = quplist; qup; qup = qup->next) {
		fprintf(fd, "%s: block grace period: %s, ",
		    qup->fsname, cvtstoa(qup->dqblk.dqb_btime));
		fprintf(fd, "file grace period: %s\n",
		    cvtstoa(qup->dqblk.dqb_itime));
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes of grace times in an ASCII file into a quotause list.
 */
int
readtimes(quplist, infd)
	struct quotause *quplist;
	int infd;
{
	register struct quotause *qup;
	FILE *fd;
	int cnt;
	register char *cp;
	time_t itime, btime, iseconds, bseconds;
	char *fsp, bunits[10], iunits[10], line1[BUFSIZ];

	lseek(infd, 0, L_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		fprintf(stderr, "Can't re-read temp file!!\n");
		return (0);
	}
	/*
	 * Discard two title lines, then read lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			fprintf(stderr, "%s: bad format\n", line1);
			return (0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			fprintf(stderr, "%s: %s: bad format\n", fsp,
			    &fsp[strlen(fsp) + 1]);
			return (0);
		}
		cnt = sscanf(cp,
		    " block grace period: %d %s file grace period: %d %s",
		    &btime, bunits, &itime, iunits);
		if (cnt != 4) {
			fprintf(stderr, "%s:%s: bad format\n", fsp, cp);
			return (0);
		}
		if (cvtatos(btime, bunits, &bseconds) == 0)
			return (0);
		if (cvtatos(itime, iunits, &iseconds) == 0)
			return (0);
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			qup->dqblk.dqb_btime = bseconds;
			qup->dqblk.dqb_itime = iseconds;
			qup->flags |= FOUND;
			break;
		}
	}
	fclose(fd);
	/*
	 * reset default grace periods for any filesystems
	 * that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_btime = 0;
		qup->dqblk.dqb_itime = 0;
	}
	return (1);
}

/*
 * Convert seconds to ASCII times.
 */
char *
cvtstoa(time)
	time_t time;
{
	static char buf[20];

	if (time % (24 * 60 * 60) == 0) {
		time /= 24 * 60 * 60;
		sprintf(buf, "%d day%s", (int)time, time == 1 ? "" : "s");
	} else if (time % (60 * 60) == 0) {
		time /= 60 * 60;
		sprintf(buf, "%d hour%s", (int)time, time == 1 ? "" : "s");
	} else if (time % 60 == 0) {
		time /= 60;
		sprintf(buf, "%d minute%s", (int)time, time == 1 ? "" : "s");
	} else
		sprintf(buf, "%d second%s", (int)time, time == 1 ? "" : "s");
	return (buf);
}

/*
 * Convert ASCII input times to seconds.
 */
int
cvtatos(time, units, seconds)
	time_t time;
	char *units;
	time_t *seconds;
{

	if (bcmp(units, "second", 6) == 0)
		*seconds = time;
	else if (bcmp(units, "minute", 6) == 0)
		*seconds = time * 60;
	else if (bcmp(units, "hour", 4) == 0)
		*seconds = time * 60 * 60;
	else if (bcmp(units, "day", 3) == 0)
		*seconds = time * 24 * 60 * 60;
	else {
		printf("%s: bad units, specify %s\n", units,
		    "days, hours, minutes, or seconds");
		return (0);
	}
	return (1);
}

/*
 * Free a list of quotause structures.
 */
void
freeprivs(quplist)
	struct quotause *quplist;
{
	register struct quotause *qup, *nextqup;

	for (qup = quplist; qup; qup = nextqup) {
		nextqup = qup->next;
		free(qup);
	}
}

/*
 * Check whether a string is completely composed of digits.
 */
int
alldigits(s)
	register char *s;
{
	register int c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
#ifdef __APPLE__
int
hasquota(fst, type, qfnamep)
        register struct statfs *fst;
	int type;
	char **qfnamep;
{
        struct stat sb;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}

        /*
	  We only support the default path to the
	  on disk quota files.
	*/

        (void)sprintf(buf, "%s/%s.%s", fst->f_mntonname,
		      QUOTAOPSNAME, qfextension[type] );
        if (stat(buf, &sb) != 0) {
          /* There appears to be no mount option file */
          return(0);
        }

	(void) sprintf(buf, "%s/%s.%s", fst->f_mntonname, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}
#else
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp, *index(), *strtok();
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if (cp = index(opt, '='))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	(void) sprintf(buf, "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}
#endif /* __APPLE */
