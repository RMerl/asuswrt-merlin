/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
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

/* *:aeb */

/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nls.h"
#include "c.h"

static char *bindirs[] = {
	"/bin",
	"/usr/bin",
	"/sbin",
	"/usr/sbin",
	"/etc",
	"/usr/etc",
	"/lib",
	"/usr/lib",
	"/lib64",
	"/usr/lib64",
	"/usr/games",
	"/usr/games/bin",
	"/usr/games/lib",
	"/usr/emacs/etc",
	"/usr/lib/emacs/*/etc",
	"/usr/TeX/bin",
	"/usr/tex/bin",
	"/usr/interviews/bin/LINUX",

	"/usr/X11R6/bin",
	"/usr/X386/bin",
	"/usr/bin/X11",
	"/usr/X11/bin",
	"/usr/X11R5/bin",

	"/usr/local/bin",
	"/usr/local/sbin",
	"/usr/local/etc",
	"/usr/local/lib",
	"/usr/local/games",
	"/usr/local/games/bin",
	"/usr/local/emacs/etc",
	"/usr/local/TeX/bin",
	"/usr/local/tex/bin",
	"/usr/local/bin/X11",

	"/usr/contrib",
	"/usr/hosts",
	"/usr/include",

	"/usr/g++-include",

	"/usr/ucb",
	"/usr/old",
	"/usr/new",
	"/usr/local",
	"/usr/libexec",
	"/usr/share",

	"/opt/*/bin",

	0
};

static char *mandirs[] = {
	"/usr/man/*",
	"/usr/share/man/*",
	"/usr/X386/man/*",
	"/usr/X11/man/*",
	"/usr/TeX/man/*",
	"/usr/interviews/man/mann",
	0
};

static char *srcdirs[] = {
	"/usr/src/*",
	"/usr/src/lib/libc/*",
	"/usr/src/lib/libc/net/*",
	"/usr/src/ucb/pascal",
	"/usr/src/ucb/pascal/utilities",
	"/usr/src/undoc",
	0
};

static char sflag = 1, bflag = 1, mflag = 1, uflag;
static char **Sflag, **Bflag, **Mflag;
static int Scnt, Bcnt, Mcnt, count, print;

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] file\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -f <file>  define search scope\n"
		" -b         search only binaries\n"
		" -B <dirs>  define binaries lookup path\n"
		" -m         search only manual paths\n"
		" -M <dirs>  define man lookup path\n"
		" -s         search only sources path\n"
		" -S <dirs>  define sources lookup path\n"
		" -u         search from unusual enties\n"
		" -V         output version information and exit\n"
		" -h         display this help and exit\n\n"), out);

	fputs(_("See how to use file and dirs arguments from whereis(1) manual.\n"), out);
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
itsit(char *cp, char *dp)
{
	int i = strlen(dp);

	if (dp[0] == 's' && dp[1] == '.' && itsit(cp, dp + 2))
		return 1;
	if (!strcmp(dp + i - 2, ".Z"))
		i -= 2;
	else if (!strcmp(dp + i - 3, ".gz"))
		i -= 3;
	else if (!strcmp(dp + i - 4, ".bz2"))
		i -= 4;
	while (*cp && *dp && *cp == *dp)
		cp++, dp++, i--;
	if (*cp == 0 && *dp == 0)
		return 1;
	while (isdigit(*dp))
		dp++;
	if (*cp == 0 && *dp++ == '.') {
		--i;
		while (i > 0 && *dp)
			if (--i, *dp++ == '.')
				return (*dp++ == 'C' && *dp++ == 0);
		return 1;
	}
	return 0;
}

static void
findin(char *dir, char *cp)
{
	DIR *dirp;
	struct dirent *dp;
	char *d, *dd;
	size_t l;
	char dirbuf[1024];
	struct stat statbuf;

	dd = strchr(dir, '*');
	if (!dd) {
		dirp = opendir(dir);
		if (dirp == NULL)
			return;
		while ((dp = readdir(dirp)) != NULL) {
			if (itsit(cp, dp->d_name)) {
				count++;
				if (print)
					printf(" %s/%s", dir, dp->d_name);
			}
		}
		closedir(dirp);
		return;
	}

	l = strlen(dir);
	if (l < sizeof(dirbuf)) {
		/* refuse excessively long names */
		strcpy(dirbuf, dir);
		d = strchr(dirbuf, '*');
		*d = 0;
		dirp = opendir(dirbuf);
		if (dirp == NULL)
			return;
		while ((dp = readdir(dirp)) != NULL) {
			if (!strcmp(dp->d_name, ".") ||
			    !strcmp(dp->d_name, ".."))
				continue;
			if (strlen(dp->d_name) + l > sizeof(dirbuf))
				continue;
			sprintf(d, "%s", dp->d_name);
			if (stat(dirbuf, &statbuf))
				continue;
			if (!S_ISDIR(statbuf.st_mode))
				continue;
			strcat(d, dd + 1);
			findin(dirbuf, cp);
		}
		closedir(dirp);
	}
	return;

}

static void
findv(char **dirv, int dirc, char *cp)
{
	while (dirc > 0)
		findin(*dirv++, cp), dirc--;
}

static void
looksrc(char *cp)
{
	if (Sflag == 0)
		findv(srcdirs, ARRAY_SIZE(srcdirs)-1, cp);
	else
		findv(Sflag, Scnt, cp);
}

static void
lookbin(char *cp)
{
	if (Bflag == 0)
		findv(bindirs, ARRAY_SIZE(bindirs)-1, cp);
	else
		findv(Bflag, Bcnt, cp);
}

static void
lookman(char *cp)
{
	if (Mflag == 0)
		findv(mandirs, ARRAY_SIZE(mandirs)-1, cp);
	else
		findv(Mflag, Mcnt, cp);
}

static void
getlist(int *argcp, char ***argvp, char ***flagp, int *cntp)
{
	(*argvp)++;
	*flagp = *argvp;
	*cntp = 0;
	for ((*argcp)--; *argcp > 0 && (*argvp)[0][0] != '-'; (*argcp)--)
		(*cntp)++, (*argvp)++;
	(*argcp)++;
	(*argvp)--;
}

static void
zerof()
{
	if (sflag && bflag && mflag)
		sflag = bflag = mflag = 0;
}

static int
print_again(char *cp)
{
	if (print)
		printf("%s:", cp);
	if (sflag) {
		looksrc(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			return 1;
		}
	}
	count = 0;
	if (bflag) {
		lookbin(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			return 1;
		}
	}
	count = 0;
	if (mflag) {
		lookman(cp);
		if (uflag && print == 0 && count != 1) {
			print = 1;
			return 1;
		}
	}
	return 0;
}

static void
lookup(char *cp)
{
	register char *dp;

	for (dp = cp; *dp; dp++)
		continue;
	for (; dp > cp; dp--) {
		if (*dp == '.') {
			*dp = 0;
			break;
		}
	}
	for (dp = cp; *dp; dp++)
		if (*dp == '/')
			cp = dp + 1;
	if (uflag) {
		print = 0;
		count = 0;
	} else
		print = 1;

	while (print_again(cp))
		/* all in print_again() */ ;

	if (print)
		printf("\n");
}

/*
 * whereis name
 * look for source, documentation and binaries
 */
int
main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	argc--, argv++;
	if (argc == 0)
		usage(stderr);

	do
		if (argv[0][0] == '-') {
			register char *cp = argv[0] + 1;
			while (*cp) switch (*cp++) {

			case 'f':
				break;

			case 'S':
				getlist(&argc, &argv, &Sflag, &Scnt);
				break;

			case 'B':
				getlist(&argc, &argv, &Bflag, &Bcnt);
				break;

			case 'M':
				getlist(&argc, &argv, &Mflag, &Mcnt);
				break;

			case 's':
				zerof();
				sflag++;
				continue;

			case 'u':
				uflag++;
				continue;

			case 'b':
				zerof();
				bflag++;
				continue;

			case 'm':
				zerof();
				mflag++;
				continue;
			case 'V':
				printf(_("%s from %s\n"),
					program_invocation_short_name,
					PACKAGE_STRING);
				return EXIT_SUCCESS;
			case 'h':
				usage(stdout);
			default:
				usage(stderr);
			}
			argv++;
		} else
			lookup(*argv++);
	while (--argc > 0);
	return EXIT_SUCCESS;
}
