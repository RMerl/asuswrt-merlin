/*
 * Copyright (c) 1983, 1989, 1993
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

 /* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "nls.h"
#include "c.h"

static int donice(int,int,int);

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %1$s [-n] <priority> [-p] <pid> [<pid>  ...]\n"
		" %1$s [-n] <priority>  -g <pgrp> [<pgrp> ...]\n"
		" %1$s [-n] <priority>  -u <user> [<user> ...]\n"),
		program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -g, --pgrp <id>        interpret as process group ID\n"
		" -h, --help             print help\n"
		" -n, --priority <num>   set the nice increment value\n"
		" -p, --pid <id>         force to be interpreted as process ID\n"
		" -u, --user <name|id>   interpret as username or user ID\n"
		" -v, --version          print version\n"), out);

	fputs(_("\nFor more information see renice(1).\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */
int
main(int argc, char **argv)
{
	int which = PRIO_PROCESS;
	int who = 0, prio, errs = 0;
	char *endptr = NULL;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	argc--;
	argv++;

	if (argc == 1) {
		if (strcmp(*argv, "-h") == 0 ||
		    strcmp(*argv, "--help") == 0)
			usage(stdout);

		if (strcmp(*argv, "-v") == 0 ||
		    strcmp(*argv, "--version") == 0) {
			printf(_("renice from %s\n"), PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		}
	}

	if (argc < 2)
		usage(stderr);

	if (strcmp(*argv, "-n") == 0 || strcmp(*argv, "--priority") == 0) {
		argc--;
		argv++;
	}

	prio = strtol(*argv, &endptr, 10);
	if (*endptr)
		usage(stderr);

	argc--;
	argv++;

	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0 || strcmp(*argv, "--pgrp") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0 || strcmp(*argv, "--user") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-p") == 0 || strcmp(*argv, "--pid") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (which == PRIO_USER) {
			register struct passwd *pwd = getpwnam(*argv);

			if (pwd == NULL) {
				warnx(_("unknown user %s"), *argv);
				continue;
			}
			who = pwd->pw_uid;
		} else {
			who = strtol(*argv, &endptr, 10);
			if (who < 0 || *endptr) {
				warnx(_("bad value %s"), *argv);
				continue;
			}
		}
		errs += donice(which, who, prio);
	}
	return errs != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int
donice(int which, int who, int prio) {
	int oldprio, newprio;
	const char *idtype = _("process ID");

	if (which == PRIO_USER)
		idtype = _("user ID");
	else if (which == PRIO_PGRP)
		idtype = _("process group ID");

	errno = 0;
	oldprio = getpriority(which, who);
	if (oldprio == -1 && errno) {
		warn(_("failed to get priority for %d (%s)"), who, idtype);
		return 1;
	}
	if (setpriority(which, who, prio) < 0) {
		warn(_("failed to set priority for %d (%s)"), who, idtype);
		return 1;
	}
	errno = 0;
	newprio = getpriority(which, who);
	if (newprio == -1 && errno) {
		warn(_("failed to get priority for %d (%s)"), who, idtype);
		return 1;
	}

	printf(_("%d (%s) old priority %d, new priority %d\n"),
	       who, idtype, oldprio, newprio);
	return 0;
}
