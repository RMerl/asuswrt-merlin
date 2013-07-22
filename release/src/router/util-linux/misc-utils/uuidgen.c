/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1999, Andreas Dilger and Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind;
#endif

#include "uuid.h"
#include "nls.h"
#include "c.h"

#define DO_TYPE_TIME	1
#define DO_TYPE_RANDOM	2

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -r, --random     generate random-based uuid\n"
		" -t, --time       generate time-based uuid\n"
		" -V, --version    output version information and exit\n"
		" -h, --help       display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main (int argc, char *argv[])
{
	int    c;
	int    do_type = 0;
	char   str[37];
	uuid_t uu;

	static const struct option longopts[] = {
		{"random", required_argument, NULL, 'r'},
		{"time", no_argument, NULL, 't'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "rtVh", longopts, NULL)) != -1)
		switch (c) {
		case 't':
			do_type = DO_TYPE_TIME;
			break;
		case 'r':
			do_type = DO_TYPE_RANDOM;
			break;
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

	switch (do_type) {
	case DO_TYPE_TIME:
		uuid_generate_time(uu);
		break;
	case DO_TYPE_RANDOM:
		uuid_generate_random(uu);
		break;
	default:
		uuid_generate(uu);
		break;
	}

	uuid_unparse(uu, str);

	printf("%s\n", str);

	return EXIT_SUCCESS;
}
