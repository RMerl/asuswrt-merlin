/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
static char sccsid[] = "@(#)quotaon.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Turn quota on/off for a filesystem.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/mount.h>
#ifdef __APPLE__
#include <sys/stat.h>
#endif /* __APPLE__ */
#include <ufs/ufs/quota.h>
#include <stdio.h>
#include <fstab.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Internal functions */
void usage(char *whoami);

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;

int	aflag;		/* all file systems */
int	gflag;		/* operate on group quotas */
int	uflag;		/* operate on user quotas */
int	vflag;		/* verbose */

int main(argc, argv)
	int argc;
	char **argv;
{
	register struct fstab *fs;
	char ch, *qfnp, *whoami, *rindex();
	long argnum, done = 0;
	int i, offmode = 0, errs = 0;
#ifdef __APPLE__
	int nfst;
	struct statfs *fst;
#endif /* __APPLE__ */
	extern char *optarg;
	extern int optind;

	whoami = rindex(*argv, '/') + 1;
	if (whoami == (char *)1)
		whoami = *argv;
	if (strcmp(whoami, "quotaoff") == 0)
		offmode++;
	else if (strcmp(whoami, "quotaon") != 0) {
		fprintf(stderr, "Name must be quotaon or quotaoff not %s\n",
			whoami);
		exit(1);
	}
	while ((ch = getopt(argc, argv, "avug")) != EOF) {
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
			usage(whoami);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc <= 0 && !aflag)
		usage(whoami);
	if (!gflag && !uflag) {
		gflag++;
		uflag++;
	}

#ifdef __APPLE__
	nfst = getmntinfo(&fst, MNT_WAIT);
	if (nfst==0) {
	  fprintf(stderr, "no filesystems mounted");
	  return(1);
	}

	for (i=0; i<nfst; i++) {
	  if(strcmp(fst[i].f_fstypename, "hfs")) {
	    if (strcmp(fst[i].f_fstypename, "ufs"))
	      continue;
	  }
	  if(fst[i].f_flags & MNT_RDONLY) {
	    errs++;
	    continue;
	  }

	  if (aflag) {
	    if (gflag && hasquota(&fst[i], GRPQUOTA, &qfnp))
	      errs += quotaonoff(&fst[i], offmode, GRPQUOTA, qfnp);
	    if (uflag && hasquota(&fst[i], USRQUOTA, &qfnp))
	      errs += quotaonoff(&fst[i], offmode, USRQUOTA, qfnp);
	    continue;
	  }
	  if ((argnum = oneof(fst[i].f_mntonname, argv, argc)) >= 0 ||
	      (argnum = oneof(fst[i].f_mntfromname, argv, argc)) >= 0) {
	    done |= 1 << argnum;
	    if (gflag && hasquota(&fst[i], GRPQUOTA, &qfnp))
	      errs += quotaonoff(&fst[i], offmode, GRPQUOTA, qfnp);
	    if (uflag && hasquota(&fst[i], USRQUOTA, &qfnp))
	      errs += quotaonoff(&fst[i], offmode, USRQUOTA, qfnp);
	  }
	}
#else
	setfsent();
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_vfstype, "ufs") ||
		    strcmp(fs->fs_type, FSTAB_RW))
			continue;
		if (aflag) {
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, USRQUOTA, qfnp);
			continue;
		}
		if ((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) {
			done |= 1 << argnum;
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, USRQUOTA, qfnp);
		}
	}
	endfsent();
#endif /* __APPLE__ */
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not found in fstab\n",
				argv[i]);
	exit(errs);
}

void usage(whoami)
	char *whoami;
{

	fprintf(stderr, "Usage:\n\t%s [-g] [-u] [-v] -a\n", whoami);
	fprintf(stderr, "\t%s [-g] [-u] [-v] filesys ...\n", whoami);
	exit(1);
}

#ifdef __APPLE__
int quotaonoff(fst, offmode, type, qfpathname)
	register struct statfs *fst;
	int offmode, type;
	char *qfpathname;
{
        if (strcmp(fst->f_mntonname, "/") && (fst->f_flags & MNT_RDONLY))
		return (1);
	if (offmode) {
		if (quotactl(fst->f_mntonname, QCMD(Q_QUOTAOFF, type), 0, 0) < 0) {
			fprintf(stderr, "quotaoff: ");
			perror(fst->f_mntonname);
			return (1);
		}
		if (vflag)
		  printf("%s: %s quotas turned off\n", fst->f_mntonname,
			       qfextension[type]);
		return (0);
	}
	if (quotactl(fst->f_mntonname, QCMD(Q_QUOTAON, type), 0, qfpathname) < 0) {
		fprintf(stderr, "quotaon: using %s on ", qfpathname);
		perror(fst->f_mntonname);
		return (1);
	}
	if (vflag)
		printf("%s: %s quotas turned on\n", fst->f_mntonname,
		    qfextension[type]);
	return (0);
}
#else
int quotaonoff(fs, offmode, type, qfpathname)
	register struct fstab *fs;
	int offmode, type;
	char *qfpathname;
{

	if (strcmp(fs->fs_file, "/") && readonly(fs))
		return (1);
	if (offmode) {
		if (quotactl(fs->fs_file, QCMD(Q_QUOTAOFF, type), 0, 0) < 0) {
			fprintf(stderr, "quotaoff: ");
			perror(fs->fs_file);
			return (1);
		}
		if (vflag)
			printf("%s: quotas turned off\n", fs->fs_file);
		return (0);
	}
	if (quotactl(fs->fs_file, QCMD(Q_QUOTAON, type), 0, qfpathname) < 0) {
		fprintf(stderr, "quotaon: using %s on", qfpathname);
		perror(fs->fs_file);
		return (1);
	}
	if (vflag)
		printf("%s: %s quotas turned on\n", fs->fs_file,
		    qfextension[type]);
	return (0);
}
#endif /* __APPLE__ */

/*
 * Check to see if target appears in list of size cnt.
 */
int oneof(target, list, cnt)
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
 * Check to see if a particular quota is to be enabled.
 */
#ifdef __APPLE__
int hasquota(fst, type, qfnamep)
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
int hasquota(fs, type, qfnamep)
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


/*
 * Verify file system is mounted and not readonly.
 */
int readonly(fs)
	register struct fstab *fs;
{
	struct statfs fsbuf;

	if (statfs(fs->fs_file, &fsbuf) < 0 ||
	    strcmp(fsbuf.f_mntonname, fs->fs_file) ||
	    strcmp(fsbuf.f_mntfromname, fs->fs_spec)) {
		printf("%s: not mounted\n", fs->fs_file);
		return (1);
	}
	if (fsbuf.f_flags & MNT_RDONLY) {
		printf("%s: mounted read-only\n", fs->fs_file);
		return (1);
	}
	return (0);
}
