/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 * Modified Fri Mar 10 20:27:19 1995, faith@cs.unc.edu, for Linux
 * Modified Mon Jul  1 18:14:10 1996, janl@ifi.uio.no, writing to stdout
 *	as suggested by Michael Meskes <meskes@Informatik.RWTH-Aachen.DE>
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2010-12-01 Marek Polacek <mmpolacek@gmail.com>
 * - cleanups
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include "nls.h"
#include "c.h"

/* exit codes */

#define IS_ALLOWED        0  /* Receiving messages is allowed.  */
#define IS_NOT_ALLOWED    1  /* Receiving messages is not allowed.  */
#define MESG_EXIT_FAILURE 2  /* An error occurred.  */

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] [y | n]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -v, --verbose      explain what is being done\n"
		" -V, --version      output version information and exit\n"
		" -h, --help         output help screen and exit\n\n"), out);

	exit(out == stderr ? MESG_EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct stat sb;
	char *tty;
	int ch, verbose = FALSE;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	static const struct option longopts[] = {
		{ "verbose",    no_argument,       0, 'v' },
		{ "version",    no_argument,       0, 'V' },
		{ "help",       no_argument,       0, 'h' },
		{ NULL,         0, 0, 0 }
	};

	while ((ch = getopt_long(argc, argv, "vVh", longopts, NULL)) != -1)
		switch (ch) {
		case 'v':
			verbose = TRUE;
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

	argc -= optind;
	argv += optind;

	if ((tty = ttyname(STDERR_FILENO)) == NULL)
		err(MESG_EXIT_FAILURE, _("ttyname failed"));

	if (stat(tty, &sb) < 0)
		err(MESG_EXIT_FAILURE, _("stat %s failed"), tty);

	if (!*argv) {
		if (sb.st_mode & (S_IWGRP | S_IWOTH)) {
			puts(_("is y"));
			return IS_ALLOWED;
		}
		puts(_("is n"));
		return IS_NOT_ALLOWED;
	}

	switch (*argv[0]) {
	case 'y':
#ifdef USE_TTY_GROUP
		if (chmod(tty, sb.st_mode | S_IWGRP) < 0)
#else
		if (chmod(tty, sb.st_mode | S_IWGRP | S_IWOTH) < 0)
#endif
			err(MESG_EXIT_FAILURE, _("change %s mode failed"), tty);
		if (verbose)
			puts(_("write access to your terminal is allowed"));
		return IS_ALLOWED;
	case 'n':
		if (chmod(tty, sb.st_mode & ~(S_IWGRP|S_IWOTH)) < 0)
			 err(MESG_EXIT_FAILURE, _("change %s mode failed"), tty);
		if (verbose)
			puts(_("write access to your terminal is denied"));
		return IS_NOT_ALLOWED;
	default:
		warnx(_("invalid argument: %c"), *argv[0]);
		usage(stderr);
	}
}
