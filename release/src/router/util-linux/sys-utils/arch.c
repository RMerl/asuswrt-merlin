/* arch -- print machine architecture information
 * Created: Mon Dec 20 12:27:15 1993 by faith@cs.unc.edu
 * Revised: Mon Dec 20 12:29:23 1993 by faith@cs.unc.edu
 * Copyright 1993 Rickard E. Faith (faith@cs.unc.edu)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/utsname.h>

#include "c.h"
#include "nls.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	/* Synopsis */
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fprintf(out, USAGE_MAN_TAIL("arch(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct utsname utsbuf;
	int ch;
	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (ch) {
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (uname(&utsbuf))
		err(EXIT_FAILURE, _("uname failed"));

	printf("%s\n", utsbuf.machine);

	return EXIT_SUCCESS;
}
