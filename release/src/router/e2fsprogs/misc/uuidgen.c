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
#include "uuid/uuid.h"
#include "nls-enable.h"

#define DO_TYPE_TIME	1
#define DO_TYPE_RANDOM	2

static void usage(const char *progname)
{
	fprintf(stderr, _("Usage: %s [-r] [-t]\n"), progname);
	exit(1);
}

int
main (int argc, char *argv[])
{
	int    c;
	int    do_type = 0;
	char   str[37];
	uuid_t uu;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif

	while ((c = getopt (argc, argv, "tr")) != EOF)
		switch (c) {
		case 't':
			do_type = DO_TYPE_TIME;
			break;
		case 'r':
			do_type = DO_TYPE_RANDOM;
			break;
		default:
			usage(argv[0]);
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

	return 0;
}
