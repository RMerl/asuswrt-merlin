/*
 * Copyright (c) 1988, 1990, 1993
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
 * Modified Sun Mar 12 10:34:34 1995, faith@cs.unc.edu, for Linux
 */

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <errno.h>
#include <paths.h>
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"
#include "strutils.h"
#include "ttymsg.h"
#include "pathnames.h"
#include "carefulputc.h"
#include "c.h"

#define	IGNOREUSER	"sleeper"
#define WRITE_TIME_OUT	300		/* in seconds */

#ifndef MAXHOSTNAMELEN
# ifdef HOST_NAME_MAX
#  define MAXHOSTNAMELEN HOST_NAME_MAX
# else
#  define MAXHOSTNAMELEN 64
# endif
#endif

/* Function prototypes */
char *makemsg(char *fname, size_t *mbufsize, int print_banner);
static void usage(FILE *out);

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
		_(" %s [options] [<file>]\n"),program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -n, --nobanner          do not print banner, works only for root\n"
		" -t, --timeout <timeout> write timeout in seconds\n"
		" -V, --version           output version information and exit\n"
		" -h, --help              display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv) {
	extern int optind;
	int ch;
	struct iovec iov;
	struct utmp *utmpptr;
	char *p;
	char line[sizeof(utmpptr->ut_line) + 1];
	int print_banner = TRUE;
	char *mbuf;
	size_t mbufsize;
	long timeout = WRITE_TIME_OUT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	static const struct option longopts[] = {
		{ "nobanner",	no_argument,		0, 'n' },
		{ "timeout",	required_argument,	0, 't' },
		{ "version",	no_argument,		0, 'V' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL,	0, 0, 0 }
	};

	while ((ch = getopt_long(argc, argv, "nt:Vh", longopts, NULL)) != -1) {
		switch (ch) {
		case 'n':
			if (geteuid() == 0)
				print_banner = FALSE;
			else
				warnx(_("--nobanner is available only for root"));
			break;
		case 't':
			timeout = strtoll_or_err(optarg, _("invalid timeout argument"));
			if (timeout < 1)
				errx(EXIT_FAILURE, _("invalid timeout argument: %s"), optarg);
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		usage(stderr);

	mbuf = makemsg(*argv, &mbufsize, print_banner);

	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;
	while((utmpptr = getutent())) {
		if (!utmpptr->ut_name[0] ||
		    !strncmp(utmpptr->ut_name, IGNOREUSER,
			     sizeof(utmpptr->ut_name)))
			continue;
#ifdef USER_PROCESS
		if (utmpptr->ut_type != USER_PROCESS)
			continue;
#endif

		/* Joey Hess reports that use-sessreg in /etc/X11/wdm/
		   produces ut_line entries like :0, and a write
		   to /dev/:0 fails. */
		if (utmpptr->ut_line[0] == ':')
			continue;

		xstrncpy(line, utmpptr->ut_line, sizeof(utmpptr->ut_line));
		if ((p = ttymsg(&iov, 1, line, timeout)) != NULL)
			warnx("%s", p);
	}
	endutent();
	free(mbuf);
	exit(EXIT_SUCCESS);
}

char *
makemsg(char *fname, size_t *mbufsize, int print_banner)
{
	register int ch, cnt;
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now;
	FILE *fp;
	int fd;
	char *p, *whom, *where, *hostname, *lbuf, *tmpname, *tmpenv, *mbuf;
	long line_max;

	hostname = xmalloc(sysconf(_SC_HOST_NAME_MAX) + 1);
	line_max = sysconf(_SC_LINE_MAX);
	lbuf = xmalloc(line_max);

	tmpname = xmalloc(PATH_MAX);
	tmpenv = getenv("TMPDIR");
	if ((tmpenv))
		snprintf(tmpname, PATH_MAX, "%s/%s.XXXXXX", tmpenv,
			program_invocation_short_name);
	else
		snprintf(tmpname, PATH_MAX, "%s/%s.XXXXXX", _PATH_TMP,
			program_invocation_short_name);
	if (!(fd = mkstemp(tmpname)) || !(fp = fdopen(fd, "r+")))
		err(EXIT_FAILURE, _("can't open temporary file %s"), tmpname);
	unlink(tmpname);
	free(tmpname);

	if (print_banner == TRUE) {
		if (!(whom = getlogin()) || !*whom)
			whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
		if (!whom) {
			whom = "someone";
			warn(_("cannot get passwd uid"));
		}
		where = ttyname(STDOUT_FILENO);
		if (!where) {
			where = "somewhere";
			warn(_("cannot get tty name"));
		}
		gethostname(hostname, sizeof(hostname));
		time(&now);
		lt = localtime(&now);

		/*
		 * all this stuff is to blank out a square for the message;
		 * we wrap message lines at column 79, not 80, because some
		 * terminals wrap after 79, some do not, and we can't tell.
		 * Which means that we may leave a non-blank character
		 * in column 80, but that can't be helped.
		 */
		/* snprintf is not always available, but the sprintf's here
		   will not overflow as long as %d takes at most 100 chars */
		fprintf(fp, "\r%79s\r\n", " ");
		sprintf(lbuf, _("Broadcast Message from %s@%s"),
			      whom, hostname);
		fprintf(fp, "%-79.79s\007\007\r\n", lbuf);
		sprintf(lbuf, "        (%s) at %d:%02d ...",
			      where, lt->tm_hour, lt->tm_min);
		fprintf(fp, "%-79.79s\r\n", lbuf);
	}
	fprintf(fp, "%79s\r\n", " ");

	free(hostname);

	if (fname) {
		/*
		 * When we are not root, but suid or sgid, refuse to read files
		 * (e.g. device files) that the user may not have access to.
		 * After all, our invoker can easily do "wall < file"
		 * instead of "wall file".
		 */
		uid_t uid = getuid();
		if (uid && (uid != geteuid() || getgid() != getegid()))
			errx(EXIT_FAILURE, _("will not read %s - use stdin."),
			     fname);

		if (!freopen(fname, "r", stdin))
			err(EXIT_FAILURE, _("cannot open file %s"), fname);
	}

	while (fgets(lbuf, line_max, stdin)) {
		for (cnt = 0, p = lbuf; (ch = *p) != '\0'; ++p, ++cnt) {
			if (cnt == 79 || ch == '\n') {
				for (; cnt < 79; ++cnt)
					putc(' ', fp);
				putc('\r', fp);
				putc('\n', fp);
				cnt = 0;
			}
			if (ch != '\n')
				carefulputc(ch, fp);
		}
	}
	fprintf(fp, "%79s\r\n", " ");

	free(lbuf);
	rewind(fp);

	if (fstat(fd, &sbuf))
		err(EXIT_FAILURE, _("fstat failed"));

	*mbufsize = (size_t) sbuf.st_size;
	mbuf = xmalloc(*mbufsize);

	if (fread(mbuf, 1, *mbufsize, fp) != *mbufsize)
		err(EXIT_FAILURE, _("fread failed"));

	close(fd);
	fclose(fp);
	return mbuf;
}
