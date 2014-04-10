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


/*
 * Fix up / report on disk quotas & usage
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/queue.h>
#ifdef __APPLE__
#include <sys/mount.h>
#endif /* __APPLE__ */

#include <sys/quota.h>

#include <fcntl.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#endif /* __APPLE__ */

#include "quotacheck.h"

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
char *quotagroup = QUOTAGROUP;

#ifdef __APPLE__
u_int32_t quotamagic[MAXQUOTAS] = INITQMAGICS;
#endif /* __APPLE__ */


#define FUHASH 1024	/* must be power of two */
struct fileusage *fuhead[MAXQUOTAS][FUHASH];

int	aflag;			/* all file systems */
int	gflag;			/* check group quotas */
int	uflag;			/* check user quotas */
int	vflag;			/* verbose */

#ifdef __APPLE__
int	maxentries;		/* maximum entries in disk quota file */
#else
u_long	highid[MAXQUOTAS];	/* highest addid()'ed identifier per type */
#endif /* __APPLE__ */


#ifdef __APPLE__
char *	blockcheck(char *);
int	chkquota(char *, char *, char *, struct quotaname *);
int	getquotagid(void);
int	hasquota(struct statfs *, int, char **);
struct fileusage *
	lookup(u_long, int);
void *	needchk(struct statfs *);
int	oneof(char *, char*[], int);
int	qfinsert(FILE *, struct dqblk *, int, int);
int	qfmaxentries(char *, int);
void	usage(void);
void	dqbuftohost(struct dqblk *dqbp);

#else
struct fileusage *
	 addid __P((u_long, int, char *));
char	*blockcheck __P((char *));
int	 chkquota __P((char *, char *, struct quotaname *));
int	 getquotagid __P((void));
int	 hasquota __P((struct fstab *, int, char **));
struct fileusage *
	 lookup __P((u_long, int));
void	*needchk __P((struct fstab *));
int	 oneof __P((char *, char*[], int));
int	 update __P((char *, char *, int));
void	 usage __P((void));
#endif /* __APPLE__ */


int
main(argc, argv)
	int argc;
	char *argv[];
{
#ifndef __APPLE__
	register struct fstab *fs;
	register struct passwd *pw;
	register struct group *gr;
#endif /* !__APPLE__ */
	struct quotaname *auxdata;
	int i, argnum, maxrun, errs;
	long done = 0;
	char ch, *name;

#ifdef __APPLE__
        int nfst;
        struct statfs *fst;
#endif /* __APPLE__ */	

	errs = maxrun = 0;
	while ((ch = getopt(argc, argv, "aguvl:")) != EOF) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'l':
			maxrun = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((argc == 0 && !aflag) || (argc > 0 && aflag))
		usage();
	if (!gflag && !uflag) {
		gflag++;
		uflag++;
	}

#ifdef __APPLE__
	nfst = getmntinfo(&fst, MNT_WAIT);
	if (nfst==0) {
		fprintf(stderr, "quotacheck: no mounted filesystems\n");
		exit(1);
	}

	for (i=0; i<nfst; i++) {
		if(strcmp(fst[i].f_fstypename, "hfs")) {
			if (strcmp(fst[i].f_fstypename, "ufs"))
				continue;
		}

		if (aflag) {
			if ((auxdata = needchk(&fst[i])) &&
			    (name = blockcheck(fst[i].f_mntfromname))) {
				errs += chkquota(fst[i].f_fstypename, name, fst[i].f_mntonname, auxdata);
			}
	    
			if (i+1 == nfst)
				exit (errs);
			else
				continue;
	  	}

		if (((argnum = oneof(fst[i].f_mntonname, argv, argc)) >= 0 ||
		    (argnum = oneof(fst[i].f_mntfromname, argv, argc)) >= 0) &&
		    (auxdata = needchk(&fst[i])) &&
		    (name = blockcheck(fst[i].f_mntfromname))) {
			done |= 1 << argnum;
			errs += chkquota(fst[i].f_fstypename, name, fst[i].f_mntonname, auxdata);
		}
	} /* end for loop */

	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not identified as a quota filesystem\n",
				argv[i]);

	exit(errs);
#else
	if (gflag) {
		setgrent();
		while ((gr = getgrent()) != 0)
			(void) addid((u_long)gr->gr_gid, GRPQUOTA, gr->gr_name);
		endgrent();
	}
	if (uflag) {
		setpwent();
		while ((pw = getpwent()) != 0)
			(void) addid((u_long)pw->pw_uid, USRQUOTA, pw->pw_name);
		endpwent();
	}
	if (aflag)
		exit(checkfstab(1, maxrun, needchk, chkquota));
	if (setfsent() == 0)
		err(1, "%s: can't open", FSTAB);
	while ((fs = getfsent()) != NULL) {
		if (((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) &&
		    (auxdata = needchk(fs)) &&
		    (name = blockcheck(fs->fs_spec))) {
			done |= 1 << argnum;
			errs += chkquota(name, fs->fs_file, auxdata);
		}
	}
	endfsent();
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not found in %s\n",
				argv[i], FSTAB);
	exit(errs);
#endif /* __APPLE__ */
}

void
usage()
{
	(void)fprintf(stderr, "usage:\t%s\n\t%s\n",
		"quotacheck -a [-guv]",
		"quotacheck [-guv] filesys ...");
	exit(1);
}

#ifdef __APPLE__
void *
needchk(fst)
	struct statfs *fst;
{
	register struct quotaname *qnp;
	char *qfnp;

	if(strcmp(fst->f_fstypename, "hfs")) {
		if (strcmp(fst->f_fstypename, "ufs"))
			return(NULL);
	}
	if(fst->f_flags & MNT_RDONLY)
		return (NULL);
	if ((qnp = malloc(sizeof(*qnp))) == NULL)
		err(1, "%s", strerror(errno));
	qnp->flags = 0;
	if (gflag && hasquota(fst, GRPQUOTA, &qfnp)) {
		strcpy(qnp->grpqfname, qfnp);
		qnp->flags |= HASGRP;
	}
	if (uflag && hasquota(fst, USRQUOTA, &qfnp)) {
		strcpy(qnp->usrqfname, qfnp);
		qnp->flags |= HASUSR;
	}
	if (qnp->flags)
		return (qnp);
	free(qnp);
	return (NULL);
}
#else
void *
needchk(fs)
	register struct fstab *fs;
{
	register struct quotaname *qnp;
	char *qfnp;

	if (strcmp(fs->fs_vfstype, "ufs") ||
	    strcmp(fs->fs_type, FSTAB_RW))
		return (NULL);
	if ((qnp = malloc(sizeof(*qnp))) == NULL)
		err(1, "%s", strerror(errno));
	qnp->flags = 0;
	if (gflag && hasquota(fs, GRPQUOTA, &qfnp)) {
		strcpy(qnp->grpqfname, qfnp);
		qnp->flags |= HASGRP;
	}
	if (uflag && hasquota(fs, USRQUOTA, &qfnp)) {
		strcpy(qnp->usrqfname, qfnp);
		qnp->flags |= HASUSR;
	}
	if (qnp->flags)
		return (qnp);
	free(qnp);
	return (NULL);
}
#endif /* __APPLE__ */

/*
 * Scan the specified filesystem to check quota(s) present on it.
 */
#ifdef __APPLE__
int
chkquota(fstype, fsname, mntpt, qnp)
	char *fstype, *fsname, *mntpt;
	register struct quotaname *qnp;
{
	int errs = 1;

	if (vflag) {
		fprintf(stdout, "*** Checking ");
		if (qnp->flags & HASUSR)
			fprintf(stdout, "%s%s", qfextension[USRQUOTA],
			    (qnp->flags & HASGRP) ? " and " : "");
		if (qnp->flags & HASGRP)
			fprintf(stdout, "%s", qfextension[GRPQUOTA]);
		fprintf(stdout, " quotas for %s (%s)\n", fsname, mntpt);
	}

	if(strcmp(fstype, "hfs") == 0)
		errs = chkquota_hfs(fsname, mntpt, qnp);
	else if(strcmp(fstype, "ufs") == 0)
		errs = chkquota_ufs(fsname, mntpt, qnp);

	return (errs);
}
#else
int
chkquota(fsname, mntpt, qnp)
	char *fsname, *mntpt;
	register struct quotaname *qnp;
{
        int errs = 0;

	errs = chkquota_ufs(fsname, mntpt, qnp);
	return(errs);
}
#endif /* __APPLE__ */

/*
 * Update a specified quota file.
 */
#ifdef __APPLE__
int
update(fsname, quotafile, type)
	char *fsname, *quotafile;
	register int type;
{
	register struct fileusage *fup;
	register FILE *qfi, *qfo;
	register u_long i;
	struct dqblk dqbuf;
	struct dqfilehdr dqhdr = {0};
	int m, shift;
	int idcnt = 0;
	static int warned = 0;
	static struct dqblk zerodqbuf;
	static struct fileusage zerofileusage;

	if ((qfo = fopen(quotafile, "r+")) == NULL) {
		if (errno == ENOENT)
			qfo = fopen(quotafile, "w+");
		if (qfo) {
			fprintf(stderr,
			    "quotacheck: creating quota file %s\n", quotafile);
#define	MODE	(S_IRUSR|S_IWUSR|S_IRGRP)
			(void) fchown(fileno(qfo), getuid(), getquotagid());
			(void) fchmod(fileno(qfo), MODE);
		} else {
			fprintf(stderr,
			    "quotacheck: %s: %s\n", quotafile, strerror(errno));
			return (1);
		}
	}
	if ((qfi = fopen(quotafile, "r")) == NULL) {
		fprintf(stderr,
		    "quotacheck: %s: %s\n", quotafile, strerror(errno));
		(void) fclose(qfo);
		return (1);
	}
	if (quotactl(fsname, QCMD(Q_SYNC, type), (u_long)0, (caddr_t)0) < 0 &&
		errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		fprintf(stdout, "*** Warning: %s\n",
		    "Quotas are not compiled into this kernel");
	}

	/* Read in the quota file header. */
	if (fread((char *)&dqhdr, sizeof(struct dqfilehdr), 1, qfi) > 0) {
		/* Check for reverse endian file. */
		if (OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type] &&
		    OSSwapInt32(dqhdr.dqh_magic) == quotamagic[type]) {
			fprintf(stderr,
			    "quotacheck: %s: not in big endian byte order\n",
			    quotafile);
			(void) fclose(qfo);
			(void) fclose(qfi);
			return (1);
		}
		/* Sanity check the quota file header. */
		if ((OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type]) ||
		    (OSSwapBigToHostInt32(dqhdr.dqh_version) > QF_VERSION) ||
		    (!powerof2(OSSwapBigToHostInt32(dqhdr.dqh_maxentries)))) {
			fprintf(stderr,
			    "quotacheck: %s: not a valid quota file\n",
			    quotafile);
			(void) fclose(qfo);
			(void) fclose(qfi);
			return (1);
		}
		m = OSSwapBigToHostInt32(dqhdr.dqh_maxentries);
	} else /* empty file */ {
		if (maxentries)
			m = maxentries;
		else
			m = qfmaxentries(fsname, type);

		ftruncate(fileno(qfo), (off_t)((m + 1) * sizeof(struct dqblk)));

		/* Initialize file header in big endian. */
		dqhdr.dqh_magic = OSSwapHostToBigInt32(quotamagic[type]);
		dqhdr.dqh_version = OSSwapHostToBigConstInt32(QF_VERSION);
		dqhdr.dqh_maxentries = OSSwapHostToBigInt32(m);
		dqhdr.dqh_btime = OSSwapHostToBigConstInt32(MAX_DQ_TIME);
		dqhdr.dqh_itime = OSSwapHostToBigConstInt32(MAX_IQ_TIME);
		memmove(dqhdr.dqh_string, QF_STRING_TAG, strlen(QF_STRING_TAG));
		goto orphans;  /* just insert all new records */
	}

	/* Examine all the entries in the quota file. */
	for (i = 0; i < m; i++) {
		if (fread((char *)&dqbuf, sizeof(struct dqblk), 1, qfi) == 0) {
			fprintf(stderr,
			    "quotacheck: problem reading at index %ld\n", i);
			continue;
		}
		if (dqbuf.dqb_id == 0)
			continue;
		
		++idcnt;
		if ((fup = lookup(OSSwapBigToHostInt32(dqbuf.dqb_id), type)) == 0)
			fup = &zerofileusage;
		else
			fup->fu_checked = 1;

		if (OSSwapBigToHostInt32(dqbuf.dqb_curinodes) == fup->fu_curinodes &&
		    OSSwapBigToHostInt64(dqbuf.dqb_curbytes) == fup->fu_curbytes) {
			fup->fu_curinodes = 0;
			fup->fu_curbytes = 0;
			continue;
		}
		if (vflag) {
			if (aflag)
				fprintf(stdout, "%s: ", fsname);
			fprintf(stdout, "%-12s fixed:", fup->fu_name);
			if (OSSwapBigToHostInt32(dqbuf.dqb_curinodes) != fup->fu_curinodes)
				fprintf(stdout, "\tinodes %u -> %u",
				    OSSwapBigToHostInt32(dqbuf.dqb_curinodes), fup->fu_curinodes);
			if (OSSwapBigToHostInt64(dqbuf.dqb_curbytes) != fup->fu_curbytes)
				fprintf(stdout, "\t1K blocks %qd -> %qd",
				    (OSSwapBigToHostInt64(dqbuf.dqb_curbytes)/1024), (fup->fu_curbytes/1024));
			fprintf(stdout, "\n");
		}
		/*
		 * Reset time limit if have a soft limit and were
		 * previously under it, but are now over it.
		 */
		if (dqbuf.dqb_bsoftlimit != 0 &&
		    OSSwapBigToHostInt64(dqbuf.dqb_curbytes) < OSSwapBigToHostInt64(dqbuf.dqb_bsoftlimit) &&
		    fup->fu_curbytes >= OSSwapBigToHostInt64(dqbuf.dqb_bsoftlimit))
			dqbuf.dqb_btime = 0;
		if (dqbuf.dqb_isoftlimit != 0 &&
		    OSSwapBigToHostInt32(dqbuf.dqb_curinodes) < OSSwapBigToHostInt32(dqbuf.dqb_isoftlimit) &&
		    fup->fu_curinodes >= OSSwapBigToHostInt32(dqbuf.dqb_isoftlimit))
			dqbuf.dqb_itime = 0;
		dqbuf.dqb_curinodes = OSSwapHostToBigInt32(fup->fu_curinodes);
		dqbuf.dqb_curbytes = OSSwapHostToBigInt64(fup->fu_curbytes);

		/* Write dqblk in big endian. */
		fseek(qfo, dqoffset(i), SEEK_SET);
		fwrite((char *)&dqbuf, sizeof(struct dqblk), 1, qfo);

		dqbuftohost(&dqbuf);
		(void) quotactl(fsname, QCMD(Q_SETUSE, type), dqbuf.dqb_id,
		    (caddr_t)&dqbuf);
		fup->fu_curinodes = 0;
		fup->fu_curbytes = 0;
	}
orphans:
	/* Look for any fileusage orphans */

	shift = dqhashshift(m);
	for (i = 0; i < FUHASH; ++i) {
		for (fup = fuhead[type][i]; fup != 0; fup = fup->fu_next) {
			if (fup->fu_checked || fup->fu_id == 0)
				continue;
			if (vflag) {
				if (aflag)
					fprintf(stdout, "%s: ", fsname);
				fprintf(stdout,
				    "%-12s added:\tinodes %u\t1K blocks %qd\n",
				    fup->fu_name, fup->fu_curinodes,
				    (fup->fu_curbytes/1024));
			}
			/* Initialize dqbuf in big endian. */
			dqbuf = zerodqbuf;
			dqbuf.dqb_id = OSSwapHostToBigInt32(fup->fu_id);
			dqbuf.dqb_curinodes = OSSwapHostToBigInt32(fup->fu_curinodes);
			dqbuf.dqb_curbytes = OSSwapHostToBigInt64(fup->fu_curbytes);
			/* insert this dqb */
			if (qfinsert(qfo, &dqbuf, m, shift)) {
				i = FUHASH;
				break;
			}

			dqbuftohost(&dqbuf);
			(void) quotactl(fsname, QCMD(Q_SETUSE, type),
				dqbuf.dqb_id, (caddr_t)&dqbuf);
			++idcnt;
		}
	}

	/* Write the quota file header */
	dqhdr.dqh_entrycnt = OSSwapHostToBigInt32(idcnt);
	fseek(qfo, (long)0, SEEK_SET);
	fwrite((char *)&dqhdr, sizeof(struct dqfilehdr), 1, qfo);

	fclose(qfi);
	fflush(qfo);
	fclose(qfo);
	return (0);
}

/* Convert a dqblk to host native byte order. */
void
dqbuftohost(struct dqblk *dqbp)
{
	dqbp->dqb_bhardlimit = OSSwapBigToHostInt64(dqbp->dqb_bhardlimit);
	dqbp->dqb_bsoftlimit = OSSwapBigToHostInt64(dqbp->dqb_bsoftlimit);
	dqbp->dqb_curbytes   = OSSwapBigToHostInt64(dqbp->dqb_curbytes);
	dqbp->dqb_ihardlimit = OSSwapBigToHostInt32(dqbp->dqb_ihardlimit);
	dqbp->dqb_isoftlimit = OSSwapBigToHostInt32(dqbp->dqb_isoftlimit);
	dqbp->dqb_curinodes  = OSSwapBigToHostInt32(dqbp->dqb_curinodes);
	dqbp->dqb_btime      = OSSwapBigToHostInt32(dqbp->dqb_btime);
	dqbp->dqb_itime      = OSSwapBigToHostInt32(dqbp->dqb_itime);
	dqbp->dqb_id         = OSSwapBigToHostInt32(dqbp->dqb_id);
}

#else
int
update(fsname, quotafile, type)
	char *fsname, *quotafile;
	register int type;
{
	register struct fileusage *fup;
	register FILE *qfi, *qfo;
	register u_long id, lastid;
	struct dqblk dqbuf;
	static int warned = 0;
	static struct dqblk zerodqbuf;
	static struct fileusage zerofileusage;

	if ((qfo = fopen(quotafile, "r+")) == NULL) {
		if (errno == ENOENT)
			qfo = fopen(quotafile, "w+");
		if (qfo) {
			(void) fprintf(stderr,
			    "quotacheck: creating quota file %s\n", quotafile);
#define	MODE	(S_IRUSR|S_IWUSR|S_IRGRP)
			(void) fchown(fileno(qfo), getuid(), getquotagid());
			(void) fchmod(fileno(qfo), MODE);
		} else {
			(void) fprintf(stderr,
			    "quotacheck: %s: %s\n", quotafile, strerror(errno));
			return (1);
		}
	}
	if ((qfi = fopen(quotafile, "r")) == NULL) {
		(void) fprintf(stderr,
		    "quotacheck: %s: %s\n", quotafile, strerror(errno));
		(void) fclose(qfo);
		return (1);
	}
	if (quotactl(fsname, QCMD(Q_SYNC, type), (u_long)0, (caddr_t)0) < 0 &&
	    errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		(void)printf("*** Warning: %s\n",
		    "Quotas are not compiled into this kernel");
	}
	for (lastid = highid[type], id = 0; id <= lastid; id++) {
		if (fread((char *)&dqbuf, sizeof(struct dqblk), 1, qfi) == 0)
			dqbuf = zerodqbuf;
		if ((fup = lookup(id, type)) == 0)
			fup = &zerofileusage;
		if (dqbuf.dqb_curinodes == fup->fu_curinodes &&
		    dqbuf.dqb_curblocks == fup->fu_curblocks) {
			fup->fu_curinodes = 0;
			fup->fu_curblocks = 0;
			fseek(qfo, (long)sizeof(struct dqblk), 1);
			continue;
		}
		if (vflag) {
			if (aflag)
				printf("%s: ", fsname);
			printf("%-8s fixed:", fup->fu_name);
			if (dqbuf.dqb_curinodes != fup->fu_curinodes)
				(void)printf("\tinodes %d -> %d",
					dqbuf.dqb_curinodes, fup->fu_curinodes);
			if (dqbuf.dqb_curblocks != fup->fu_curblocks)
				(void)printf("\tblocks %d -> %d",
					dqbuf.dqb_curblocks, fup->fu_curblocks);
			(void)printf("\n");
		}
		/*
		 * Reset time limit if have a soft limit and were
		 * previously under it, but are now over it.
		 */
		if (dqbuf.dqb_bsoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_bsoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_bsoftlimit)
			dqbuf.dqb_btime = 0;
		if (dqbuf.dqb_isoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_isoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_isoftlimit)
			dqbuf.dqb_itime = 0;
		dqbuf.dqb_curinodes = fup->fu_curinodes;
		dqbuf.dqb_curblocks = fup->fu_curblocks;
		fwrite((char *)&dqbuf, sizeof(struct dqblk), 1, qfo);
		(void) quotactl(fsname, QCMD(Q_SETUSE, type), id,
		    (caddr_t)&dqbuf);
		fup->fu_curinodes = 0;
		fup->fu_curblocks = 0;
	}
	fclose(qfi);
	fflush(qfo);
	ftruncate(fileno(qfo),
	    (off_t)((highid[type] + 1) * sizeof(struct dqblk)));
	fclose(qfo);
	return (0);
}
#endif /* __APPLE__ */

#ifdef __APPLE__
/*
 * Insert an entry into a quota file.
 * 
 * The dqblk pointed to by dqbp is in big endian.
 */
int
qfinsert(file, dqbp, maxentries, shift)
	FILE *file;
	struct dqblk *dqbp;
	int maxentries, shift;
{
	struct dqblk dqbuf;
	int i, skip, last;
	u_long mask;
	u_long id;
	u_long offset;

	id = OSSwapBigToHostInt32(dqbp->dqb_id);
	if (id == 0)
		return (0);
	mask = maxentries - 1;
	i = dqhash1(id, dqhashshift(maxentries), mask);
	skip = dqhash2(id, mask);

	for (last = (i + (maxentries-1) * skip) & mask;
	     i != last;
	     i = (i + skip) & mask) {
		offset = dqoffset(i);
		fseek(file, (long)offset, SEEK_SET);
		if (fread((char *)&dqbuf, sizeof(struct dqblk), 1, file) == 0) {
			fprintf(stderr, "quotacheck: read error at index %d\n", i);
			return (EIO);
		}
		/*
		 * Stop when an empty entry is found
		 * or we encounter a matching id.
		 */
		if (dqbuf.dqb_id == 0 || dqbuf.dqb_id == dqbp->dqb_id) {
			dqbuf = *dqbp;
			fseek(file, (long)offset, SEEK_SET);
			fwrite((char *)&dqbuf, sizeof(struct dqblk), 1, file);
			return (0);
		}
	}
	fprintf(stderr, "quotacheck: exceeded maximum entries (%d)\n", maxentries);
	return (ENOSPC);
}

#define ONEGIGABYTE        (1024*1024*1024)

/*
 * Calculate the size of the hash table from the size of
 * the file system.  The open addressing hashing used on
 * the quota file assumes that this table will never be
 * more than 90% full.
 */
int
qfmaxentries(mntpt, type)
	char *mntpt;
	int type;
{
	struct statfs fs_stat;
	u_int64_t fs_size;
	int max = 0;

	if (statfs(mntpt, &fs_stat)) {
		fprintf(stderr, "quotacheck: %s: %s\n",
		    mntpt, strerror(errno));
		return (0);
	}
	fs_size = (u_int64_t)fs_stat.f_blocks * (u_int64_t)fs_stat.f_bsize;

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
	return (max);
}
#endif /* __APPLE__ */

/*
 * Check to see if target appears in list of size cnt.
 */
int
oneof(target, list, cnt)
	register char *target, *list[];
	int cnt;
{
	register int i;

	for (i = 0; i < cnt; i++)
		if (strcmp(target, list[i]) == 0)
			return (i);
	return (-1);
}

/*
 * Determine the group identifier for quota files.
 */
int
getquotagid()
{
	struct group *gr;

	if (gr = getgrnam(quotagroup))
		return (gr->gr_gid);
	return (-1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
#ifdef __APPLE__
int
hasquota(fst, type, qfnamep)
	struct statfs *fst;
	int type;
	char **qfnamep;
{
	struct stat sb;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		(void)snprintf(usrname, sizeof(usrname),
		    "%s%s", qfextension[USRQUOTA], qfname);
		(void)snprintf(grpname, sizeof(grpname),
		    "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}

        /*
	  We only support the default path to the
	  on disk quota files.
	*/

        (void)snprintf(buf, sizeof(buf), "%s/%s.%s", fst->f_mntonname,
		       QUOTAOPSNAME, qfextension[type] );
        if (stat(buf, &sb) != 0) {
          /* There appears to be no mount option file */
          return(0);
        }

	(void)snprintf(buf, sizeof(buf),
		       "%s/%s.%s", fst->f_mntonname, qfname, qfextension[type]);
	*qfnamep = buf;

	return (1);
}
#else
int
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		(void)snprintf(usrname, sizeof(usrname),
		    "%s%s", qfextension[USRQUOTA], qfname);
		(void)snprintf(grpname, sizeof(grpname),
		    "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if (cp = strchr(opt, '='))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp)
		*qfnamep = cp;
	else {
		(void)snprintf(buf, sizeof(buf),
		    "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
		*qfnamep = buf;
	}
	return (1);
}
#endif /* __APPLE__ */

/*
 * Routines to manage the file usage table.
 *
 * Lookup an id of a specific type.
 */
struct fileusage *
lookup(id, type)
	u_long id;
	int type;
{
	register struct fileusage *fup;

	for (fup = fuhead[type][id & (FUHASH-1)]; fup != 0; fup = fup->fu_next)
		if (fup->fu_id == id)
			return (fup);
	return (NULL);
}

/*
 * Add a new file usage id if it does not already exist.
 */
#ifdef __APPLE__
struct fileusage *
addid(id, type)
	u_long id;
	int type;
{
	struct fileusage *fup, **fhp;
	struct passwd *pw;
	struct group *gr;
	char *name;
	int len;

	if (fup = lookup(id, type))
		return (fup);

	name = NULL;
	len = 10;
	switch (type) {
	case USRQUOTA:
		if ((pw = getpwuid(id)) != 0) {
			name = pw->pw_name;
			len = strlen(name);
		}
		break;
	case GRPQUOTA:
		if ((gr = getgrgid(id)) != 0) {
			name = gr->gr_name;
			len = strlen(name);
		}
		break;
	}

	if ((fup = calloc(1, sizeof(*fup) + len)) == NULL)
		err(1, "%s", strerror(errno));
	fhp = &fuhead[type][id & (FUHASH - 1)];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_id = id;
	if (name)
		memmove(fup->fu_name, name, len + 1);
	else 
		(void)sprintf(fup->fu_name, "%u", (unsigned int)id);

	return (fup);
}
#else
struct fileusage *
addid(id, type, name)
	u_long id;
	int type;
	char *name;
{
	struct fileusage *fup, **fhp;
	int len;

	if (fup = lookup(id, type))
		return (fup);
	if (name)
		len = strlen(name);
	else
		len = 10;
	if ((fup = calloc(1, sizeof(*fup) + len)) == NULL)
		err(1, "%s", strerror(errno));
	fhp = &fuhead[type][id & (FUHASH - 1)];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_id = id;
	if (id > highid[type])
		highid[type] = id;
	if (name)
		memmove(fup->fu_name, name, len + 1);
	else
		(void)sprintf(fup->fu_name, "%u", id);
	return (fup);
}
#endif /* __APPLE__ */

