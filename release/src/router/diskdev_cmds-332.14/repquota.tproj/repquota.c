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
static char sccsid[] = "@(#)repquota.c	8.2 (Berkeley) 11/22/94";
#endif /* not lint */

/*
 * Quota report
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/quota.h>

#ifdef __APPLE__
#include <sys/mount.h>
#endif /* __APPLE__ */
#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#endif /* __APPLE__ */

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
#ifdef __APPLE__
u_int32_t quotamagic[MAXQUOTAS] = INITQMAGICS;
#endif /* __APPLE__ */

#ifndef __APPLE__
struct fileusage {
	struct	fileusage *fu_next;
	struct	dqblk fu_dqblk;
	u_long	fu_id;
	char	fu_name[1];
	/* actually bigger */
};
struct fileusage * addid(u_long, int);
struct fileusage * lookup(u_long, int);

#define FUHASH 1024	/* must be power of two */
struct fileusage *fuhead[MAXQUOTAS][FUHASH];
u_long highid[MAXQUOTAS];	/* highest addid()'ed identifier per type */
#endif /* NOT __APPLE__ */

int	vflag;			/* verbose */
int	aflag;			/* all file systems */

int	hasquota(struct statfs *, int, char **);
int	repquota(struct statfs *, int, char *);
int	oneof(char *, char **, int);
void	usage(void);

int
main(argc, argv)
	int argc;
	char **argv;
{
#ifndef __APPLE__
	register struct fstab *fs;
	register struct passwd *pw;
	register struct group *gr;
#endif /* __APPLE__ */
	int gflag = 0, uflag = 0, errs = 0;
	long i, argnum, done = 0;
#ifdef __APPLE__
        int nfst;
        struct statfs *fst;
#endif /* __APPLE__ */
	extern char *optarg;
	extern int optind;
	char ch, *qfnp;

	while ((ch = getopt(argc, argv, "aguv")) != EOF) {
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
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0 && !aflag)
		usage();
	if (!gflag && !uflag) {
		if (aflag)
			gflag++;
		uflag++;
	}

#ifdef __APPLE__
        nfst = getmntinfo(&fst, MNT_WAIT);
        if (nfst==0) {
          fprintf(stderr, "repquota: no filesystems mounted");
          return(1);
        }

        for (i=0; i<nfst; i++) {
                if(strcmp(fst[i].f_fstypename, "hfs")) {
		    if (strcmp(fst[i].f_fstypename, "ufs"))
		        continue;
		}
		if (aflag) {
			if (gflag && hasquota(&fst[i], GRPQUOTA, &qfnp))
				errs += repquota(&fst[i], GRPQUOTA, qfnp);
			if (uflag && hasquota(&fst[i], USRQUOTA, &qfnp))
				errs += repquota(&fst[i], USRQUOTA, qfnp);
			continue;
		}
		if ((argnum = oneof(fst[i].f_mntonname, argv, argc)) >= 0 ||
		    (argnum = oneof(fst[i].f_mntfromname, argv, argc)) >= 0) {
			done |= 1 << argnum;
			if (gflag && hasquota(&fst[i], GRPQUOTA, &qfnp))
				errs += repquota(&fst[i], GRPQUOTA, qfnp);
			if (uflag && hasquota(&fst[i], USRQUOTA, &qfnp))
				errs += repquota(&fst[i], USRQUOTA, qfnp);
		}
	}
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

	setfsent();
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_vfstype, "ufs"))
			continue;
		if (aflag) {
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += repquota(fs, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += repquota(fs, USRQUOTA, qfnp);
			continue;
		}
		if ((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) {
			done |= 1 << argnum;
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += repquota(fs, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += repquota(fs, USRQUOTA, qfnp);
		}
	}
	endfsent();
#endif /* __APPLE */
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not found in fstab\n", argv[i]);
	exit(errs);
}

void
usage()
{
	fprintf(stderr, "Usage:\n\t%s\n\t%s\n",
		"repquota [-v] [-g] [-u] -a",
		"repquota [-v] [-g] [-u] filesys ...");
	exit(1);
}

#ifdef __APPLE__
int
repquota(fst, type, qfpathname)
        struct statfs *fst;
	int type;
	char *qfpathname;
{
	FILE *qf;
	u_long id;
	struct dqblk dqbuf;
	char *timeprt();
	char *name;
	struct dqfilehdr dqhdr;
	static int warned = 0;
	static int multiple = 0;
	extern int errno;
	int i;
	struct passwd *pw;
	struct group *gr;
	int maxentries;
	u_int64_t bsoftlimit;
	u_int32_t isoftlimit;
	u_int64_t curbytes;
	u_int32_t curinodes;


	if (quotactl(fst->f_mntonname, QCMD(Q_SYNC, type), 0, 0) < 0 &&
	    errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		fprintf(stdout,
		    "*** Warning: Quotas are not compiled into this kernel\n");
	}
	if (multiple++)
		printf("\n");
	if (vflag)
		fprintf(stdout, "*** Report for %s quotas on %s (%s)\n",
		    qfextension[type], fst->f_mntonname, fst->f_mntfromname);
	if ((qf = fopen(qfpathname, "r")) == NULL) {
		perror(qfpathname);
		return (1);
	}

	/* Read in the quota file header. */
	if (fread((char *)&dqhdr, sizeof(struct dqfilehdr), 1, qf) > 0) {
		/* Check for non big endian file. */
		if (OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type] &&
		    OSSwapInt32(dqhdr.dqh_magic) == quotamagic[type]) {
			(void) fprintf(stderr,
			    "*** Error: %s: not in big endian byte order\n", qfpathname);
			(void) fclose(qf);
			return (1);
		}
		/* Sanity check the quota file header. */
		if ((OSSwapBigToHostInt32(dqhdr.dqh_magic) != quotamagic[type]) ||
		    (OSSwapBigToHostInt32(dqhdr.dqh_version) > QF_VERSION) ||
		    (!powerof2(OSSwapBigToHostInt32(dqhdr.dqh_maxentries)))) {
			(void) fprintf(stderr,
			    "repquota: %s: not a valid quota file\n", qfpathname);
			(void) fclose(qf);
			return (1);
		}
	}
	
	printf("                        1K Block limits               File limits\n");
	printf("User                used        soft        hard  grace    used  soft  hard  grace\n");

	maxentries = OSSwapBigToHostInt32(dqhdr.dqh_maxentries);

	/* Read the entries in the quota file. */
	for (i = 0; i < maxentries; i++) {
		if (fread(&dqbuf, sizeof(struct dqblk), 1, qf) == 0 && feof(qf))
			break;
		id = OSSwapBigToHostInt32(dqbuf.dqb_id);
		if (id == 0)
			continue;
		if (dqbuf.dqb_curinodes == 0 && dqbuf.dqb_curbytes == 0LL)
			continue;
		name = NULL;
		switch (type) {
		case USRQUOTA:
			if ((pw = getpwuid(id)) != 0)
				name = pw->pw_name;
			break;
		case GRPQUOTA:
			if ((gr = getgrgid(id)) != 0)
				name = gr->gr_name;
			break;
		}
		if (name)
			printf("%-10s", name);
		else
			printf("%-10u", (unsigned int)id);

		bsoftlimit = OSSwapBigToHostInt64( dqbuf.dqb_bsoftlimit );
		isoftlimit = OSSwapBigToHostInt32( dqbuf.dqb_isoftlimit );
		curbytes   = OSSwapBigToHostInt64( dqbuf.dqb_curbytes );
		curinodes  = OSSwapBigToHostInt32( dqbuf.dqb_curinodes );

		printf("%c%c%12qd%12qd%12qd%7s",
			bsoftlimit && 
			    curbytes >= 
			    bsoftlimit ? '+' : '-',
			isoftlimit &&
			    curinodes >=
			    isoftlimit ? '+' : '-',
			curbytes / 1024,
			bsoftlimit / 1024,
			OSSwapBigToHostInt64( dqbuf.dqb_bhardlimit ) / 1024,
			bsoftlimit && 
			    curbytes >= 
			    bsoftlimit ?
			    timeprt(OSSwapBigToHostInt32(dqbuf.dqb_btime)) : "");
		printf("  %6d%6d%6d%7s\n",
			curinodes,
			isoftlimit,
			OSSwapBigToHostInt32( dqbuf.dqb_ihardlimit ),
			isoftlimit &&
			    curinodes >=
			    isoftlimit ?
			    timeprt(OSSwapBigToHostInt32(dqbuf.dqb_itime)) : "");
	}
	fclose(qf);

	return (0);
}
#else
repquota(fs, type, qfpathname)
	register struct fstab *fs;
	int type;
	char *qfpathname;
{
	register struct fileusage *fup;
	FILE *qf;
	u_long id;
	struct dqblk dqbuf;
	char *timeprt();
	static struct dqblk zerodqblk;
	static int warned = 0;
	static int multiple = 0;
	extern int errno;

	if (quotactl(fs->fs_file, QCMD(Q_SYNC, type), 0, 0) < 0 &&
	    errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		fprintf(stdout,
		    "*** Warning: Quotas are not compiled into this kernel\n");
	}
	if (multiple++)
		printf("\n");
	if (vflag)
		fprintf(stdout, "*** Report for %s quotas on %s (%s)\n",
		    qfextension[type], fs->fs_file, fs->fs_spec);
	if ((qf = fopen(qfpathname, "r")) == NULL) {
		perror(qfpathname);
		return (1);
	}
	for (id = 0; ; id++) {
		fread(&dqbuf, sizeof(struct dqblk), 1, qf);
		if (feof(qf))
			break;
		if (dqbuf.dqb_curinodes == 0 && dqbuf.dqb_curblocks == 0)
			continue;
		if ((fup = lookup(id, type)) == 0)
			fup = addid(id, type, (char *)0);
		fup->fu_dqblk = dqbuf;
	}
	fclose(qf);
	printf("                        Block limits               File limits\n");
	printf("User            used    soft    hard  grace    used  soft  hard  grace\n");

	for (id = 0; id <= highid[type]; id++) {
		fup = lookup(id, type);
		if (fup == 0)
			continue;
		if (fup->fu_dqblk.dqb_curinodes == 0 &&
		    fup->fu_dqblk.dqb_curblocks == 0)
			continue;
		printf("%-10s", fup->fu_name);
		printf("%c%c%8d%8d%8d%7s",
			fup->fu_dqblk.dqb_bsoftlimit && 
			    fup->fu_dqblk.dqb_curblocks >= 
			    fup->fu_dqblk.dqb_bsoftlimit ? '+' : '-',
			fup->fu_dqblk.dqb_isoftlimit &&
			    fup->fu_dqblk.dqb_curinodes >=
			    fup->fu_dqblk.dqb_isoftlimit ? '+' : '-',
			dbtob(fup->fu_dqblk.dqb_curblocks) / 1024,
			dbtob(fup->fu_dqblk.dqb_bsoftlimit) / 1024,
			dbtob(fup->fu_dqblk.dqb_bhardlimit) / 1024,
			fup->fu_dqblk.dqb_bsoftlimit && 
			    fup->fu_dqblk.dqb_curblocks >= 
			    fup->fu_dqblk.dqb_bsoftlimit ?
			    timeprt(fup->fu_dqblk.dqb_btime) : "");
		printf("  %6d%6d%6d%7s\n",
			fup->fu_dqblk.dqb_curinodes,
			fup->fu_dqblk.dqb_isoftlimit,
			fup->fu_dqblk.dqb_ihardlimit,
			fup->fu_dqblk.dqb_isoftlimit &&
			    fup->fu_dqblk.dqb_curinodes >=
			    fup->fu_dqblk.dqb_isoftlimit ?
			    timeprt(fup->fu_dqblk.dqb_itime) : "");
		fup->fu_dqblk = zerodqblk;
	}
	return (0);
}
#endif /* __APPLE */

/*
 * Check to see if target appears in list of size cnt.
 */
int
oneof(target, list, cnt)
	register char *target, **list;
	int cnt;
{
	register int i;

	for (i = 0; i < cnt; i++)
		if (strcmp(target, list[i]) == 0)
			return (i);
	return (-1);
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
#endif /* __APPLE__ */


#ifndef __APPLE__

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
	return ((struct fileusage *)0);
}

/*
 * Add a new file usage id if it does not already exist.
 */
struct fileusage *
addid(id, type, name)
	u_long id;
	int type;
	char *name;
{
	struct fileusage *fup, **fhp;
	int len;
	extern char *calloc();

	if (fup = lookup(id, type))
		return (fup);
	if (name)
		len = strlen(name);
	else
		len = 10;
	if ((fup = (struct fileusage *)calloc(1, sizeof(*fup) + len)) == NULL) {
		fprintf(stderr, "out of memory for fileusage structures\n");
		exit(1);
	}
	fhp = &fuhead[type][id & (FUHASH - 1)];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_id = id;
	if (id > highid[type])
		highid[type] = id;
	if (name) {
		bcopy(name, fup->fu_name, len + 1);
	} else {
		sprintf(fup->fu_name, "%u", id);
	}
	return (fup);
}
#endif /* !__APPLE__ */

/*
 * Calculate the grace period and return a printable string for it.
 */
char *
timeprt(seconds)
	time_t seconds;
{
	time_t hours, minutes;
	static char buf[20];
	static time_t now;

	if (now == 0)
		time(&now);
	if (now > seconds)
		return ("none");
	seconds -= now;
	minutes = (seconds + 30) / 60;
	hours = (minutes + 30) / 60;
	if (hours >= 36) {
		sprintf(buf, "%ddays", (hours + 12) / 24);
		return (buf);
	}
	if (minutes >= 60) {
		sprintf(buf, "%2d:%d", minutes / 60, minutes % 60);
		return (buf);
	}
	sprintf(buf, "%2d", minutes);
	return (buf);
}
