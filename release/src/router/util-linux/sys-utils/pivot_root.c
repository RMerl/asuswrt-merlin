/*
 * pivot_root.c - Change the root file system
 *
 * Copyright (C) 2000 Werner Almesberger
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"

#define pivot_root(new_root,put_old) syscall(SYS_pivot_root,new_root,put_old)

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	fprintf(out, _(" %s [options] new_root put_old\n"),
		program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fprintf(out, USAGE_MAN_TAIL("pivot_root(8)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
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

	if (argc != 3)
		usage(stderr);

	if (pivot_root(argv[1], argv[2]) < 0)
		err(EXIT_FAILURE, _("failed to change root from `%s' to `%s'"),
		    argv[1], argv[2]);

	return EXIT_SUCCESS;
}
