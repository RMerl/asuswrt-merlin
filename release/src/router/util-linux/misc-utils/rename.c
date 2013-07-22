/*
 * rename.c - aeb 2000-01-01
 *
--------------------------------------------------------------
#!/bin/sh
if [ $# -le 2 ]; then echo call: rename from to files; exit; fi
FROM="$1"
TO="$2"
shift
shift
for i in $@; do N=`echo "$i" | sed "s/$FROM/$TO/g"`; mv "$i" "$N"; done
--------------------------------------------------------------
 * This shell script will do renames of files, but may fail
 * in cases involving special characters. Here a C version.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"
#include "c.h"

static int do_rename(char *from, char *to, char *s, int verbose)
{
	char *newname, *where, *p, *q;
	int flen, tlen, slen;

	where = strstr(s, from);
	if (where == NULL)
		return 0;

	flen = strlen(from);
	tlen = strlen(to);
	slen = strlen(s);
	newname = xmalloc(tlen + slen + 1);

	p = s;
	q = newname;
	while (p < where)
		*q++ = *p++;
	p = to;
	while (*p)
		*q++ = *p++;
	p = where + flen;
	while (*p)
		*q++ = *p++;
	*q = 0;

	if (rename(s, newname) != 0)
		err(EXIT_FAILURE, _("renaming %s to %s failed"),
		    s, newname);
	if (verbose)
		printf("`%s' -> `%s'\n", s, newname);

	free(newname);
	return 1;
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] expression replacement file...\n"),
		program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -v, --verbose    explain what is being done\n"
		" -V, --version    output version information and exit\n"
		" -h, --help       display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *from, *to;
	int i, c, verbose = 0;

	static const struct option longopts[] = {
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "vVh", longopts, NULL)) != -1)
		switch (c) {
		case 'v':
			verbose = 1;
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

	argc -= optind;
	argv += optind;

	if (argc < 3) {
		warnx("not enough arguments");
		usage(stderr);
	}

	from = argv[0];
	to = argv[1];

	for (i = 2; i < argc; i++)
		do_rename(from, to, argv[i], verbose);

	return EXIT_SUCCESS;
}
