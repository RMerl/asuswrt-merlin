/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jef Poskanzer and Craig Leres of the Lawrence Berkeley Laboratory.
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
 * Modified for Linux, Mon Mar  8 18:16:24 1993, faith@cs.unc.edu
 * Wed Jun 22 21:41:56 1994, faith@cs.unc.edu:
 *      Added fix from Mike Grupenhoff (kashmir@umiacs.umd.edu)
 * Mon Jul  1 17:01:39 MET DST 1996, janl@math.uio.no:
 *      - Added fix from David.Chapell@mail.trincoll.edu enabeling daemons
 *	  to use write.
 *      - ANSIed it since I was working on it anyway.
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <utmp.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <paths.h>
#include <asm/param.h>
#include <getopt.h>
#include "c.h"
#include "carefulputc.h"
#include "nls.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out);
void search_utmp(char *, char *, char *, uid_t);
void do_write(char *, char *, uid_t);
void wr_fputs(char *);
static void __attribute__ ((__noreturn__)) done(int);
int term_chk(char *, int *, time_t *, int);
int utmp_chk(char *, char *);

static gid_t myegid;

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] <user> [<ttyname>]\n"),
	      program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -V, --version    output version information and exit\n"
		" -h, --help       display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	time_t atime;
	uid_t myuid;
	int msgsok, myttyfd, c;
	char tty[MAXPATHLEN], *mytty;

	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (c) {
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

	myegid = getegid();

	/* check that sender has write enabled */
	if (isatty(fileno(stdin)))
		myttyfd = fileno(stdin);
	else if (isatty(fileno(stdout)))
		myttyfd = fileno(stdout);
	else if (isatty(fileno(stderr)))
		myttyfd = fileno(stderr);
	else
		myttyfd = -1;

	if (myttyfd != -1) {
		if (!(mytty = ttyname(myttyfd)))
			errx(EXIT_FAILURE,
			     _("can't find your tty's name"));

		/*
		 * We may have /dev/ttyN but also /dev/pts/xx. Below,
		 * term_chk() will put "/dev/" in front, so remove that
		 * part.
		 */
		if (!strncmp(mytty, "/dev/", 5))
			mytty += 5;
		if (term_chk(mytty, &msgsok, &atime, 1))
			exit(EXIT_FAILURE);
		if (!msgsok)
			errx(EXIT_FAILURE,
			     _("you have write permission turned off"));

	} else
		mytty = "<no tty>";

	myuid = getuid();

	/* check args */
	switch (argc) {
	case 2:
		search_utmp(argv[1], tty, mytty, myuid);
		do_write(tty, mytty, myuid);
		break;
	case 3:
		if (!strncmp(argv[2], "/dev/", 5))
			argv[2] += 5;
		if (utmp_chk(argv[1], argv[2]))
			errx(EXIT_FAILURE,
			     _("%s is not logged in on %s"),
			     argv[1], argv[2]);
		if (term_chk(argv[2], &msgsok, &atime, 1))
			exit(EXIT_FAILURE);
		if (myuid && !msgsok)
			errx(EXIT_FAILURE,
			     _("%s has messages disabled on %s"),
			     argv[1], argv[2]);
		do_write(argv[2], mytty, myuid);
		break;
	default:
		usage(stderr);
	}

	done(0);
	/* NOTREACHED */
	return EXIT_FAILURE;
}


/*
 * utmp_chk - checks that the given user is actually logged in on
 *     the given tty
 */
int utmp_chk(char *user, char *tty)
{
	struct utmp u;
	struct utmp *uptr;
	int res = 1;

	utmpname(_PATH_UTMP);
	setutent();

	while ((uptr = getutent())) {
		memcpy(&u, uptr, sizeof(u));
		if (strncmp(user, u.ut_name, sizeof(u.ut_name)) == 0 &&
		    strncmp(tty, u.ut_line, sizeof(u.ut_line)) == 0) {
			res = 0;
			break;
		}
	}

	endutent();
	return res;
}

/*
 * search_utmp - search utmp for the "best" terminal to write to
 *
 * Ignores terminals with messages disabled, and of the rest, returns
 * the one with the most recent access time.  Returns as value the number
 * of the user's terminals with messages enabled, or -1 if the user is
 * not logged in at all.
 *
 * Special case for writing to yourself - ignore the terminal you're
 * writing from, unless that's the only terminal with messages enabled.
 */
void search_utmp(char *user, char *tty, char *mytty, uid_t myuid)
{
	struct utmp u;
	struct utmp *uptr;
	time_t bestatime, atime;
	int nloggedttys, nttys, msgsok, user_is_me;
	char atty[sizeof(u.ut_line) + 1];

	utmpname(_PATH_UTMP);
	setutent();

	nloggedttys = nttys = 0;
	bestatime = 0;
	user_is_me = 0;
	while ((uptr = getutent())) {
		memcpy(&u, uptr, sizeof(u));
		if (strncmp(user, u.ut_name, sizeof(u.ut_name)) == 0) {
			++nloggedttys;
			strncpy(atty, u.ut_line, sizeof(u.ut_line));
			atty[sizeof(u.ut_line)] = '\0';
			if (term_chk(atty, &msgsok, &atime, 0))
				/* bad term? skip */
				continue;
			if (myuid && !msgsok)
				/* skip ttys with msgs off */
				continue;
			if (strcmp(atty, mytty) == 0) {
				user_is_me = 1;
				/* don't write to yourself */
				continue;
			}
			if (u.ut_type != USER_PROCESS)
				/* it's not a valid entry */
				continue;
			++nttys;
			if (atime > bestatime) {
				bestatime = atime;
				strcpy(tty, atty);
			}
		}
	}

	endutent();
	if (nloggedttys == 0)
		errx(EXIT_FAILURE, _("%s is not logged in"), user);
	if (nttys == 0) {
		if (user_is_me) {
			/* ok, so write to yourself! */
			strcpy(tty, mytty);
			return;
		}
		errx(EXIT_FAILURE, _("%s has messages disabled"), user);
	} else if (nttys > 1) {
		warnx(_("%s is logged in more than once; writing to %s"),
		      user, tty);
	}
}

/*
 * term_chk - check that a terminal exists, and get the message bit
 *     and the access time
 */
int term_chk(char *tty, int *msgsokP, time_t * atimeP, int showerror)
{
	struct stat s;
	char path[MAXPATHLEN];

	if (strlen(tty) + 6 > sizeof(path))
		return 1;
	sprintf(path, "/dev/%s", tty);
	if (stat(path, &s) < 0) {
		if (showerror)
			warn("%s", path);
		return 1;
	}

	/* group write bit and group ownership */
	*msgsokP = (s.st_mode & (S_IWRITE >> 3)) && myegid == s.st_gid;
	*atimeP = s.st_atime;
	return 0;
}

/*
 * do_write - actually make the connection
 */
void do_write(char *tty, char *mytty, uid_t myuid)
{
	char *login, *pwuid, *nows;
	struct passwd *pwd;
	time_t now;
	char path[MAXPATHLEN], host[MAXHOSTNAMELEN], line[512];

	/* Determine our login name(s) before the we reopen() stdout */
	if ((pwd = getpwuid(myuid)) != NULL)
		pwuid = pwd->pw_name;
	else
		pwuid = "???";
	if ((login = getlogin()) == NULL)
		login = pwuid;

	if (strlen(tty) + 6 > sizeof(path))
		errx(EXIT_FAILURE, _("tty path %s too long"), tty);
	snprintf(path, sizeof(path), "/dev/%s", tty);
	if ((freopen(path, "w", stdout)) == NULL)
		err(EXIT_FAILURE, "%s", path);

	signal(SIGINT, done);
	signal(SIGHUP, done);

	/* print greeting */
	if (gethostname(host, sizeof(host)) < 0)
		strcpy(host, "???");
	now = time((time_t *) NULL);
	nows = ctime(&now);
	nows[16] = '\0';
	printf("\r\n\007\007\007");
	if (strcmp(login, pwuid))
		printf(_("Message from %s@%s (as %s) on %s at %s ..."),
		       login, host, pwuid, mytty, nows + 11);
	else
		printf(_("Message from %s@%s on %s at %s ..."),
		       login, host, mytty, nows + 11);
	printf("\r\n");

	while (fgets(line, sizeof(line), stdin) != NULL)
		wr_fputs(line);
}

/*
 * done - cleanup and exit
 */
static void __attribute__ ((__noreturn__))
    done(int dummy __attribute__ ((__unused__)))
{
	printf("EOF\r\n");
	_exit(EXIT_SUCCESS);
}

/*
 * wr_fputs - like fputs(), but makes control characters visible and
 *     turns \n into \r\n.
 */
void wr_fputs(char *s)
{
	char c;

#define	PUTC(c)	if (carefulputc(c, stdout) == EOF) \
    err(EXIT_FAILURE, _("carefulputc failed"));
	while (*s) {
		c = *s++;
		if (c == '\n')
			PUTC('\r');
		PUTC(c);
	}
	return;
#undef PUTC
}
