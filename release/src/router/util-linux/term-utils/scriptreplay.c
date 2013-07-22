/*
 * Copyright (C) 2008, Karel Zak <kzak@redhat.com>
 * Copyright (C) 2008, James Youngman <jay@gnu.org>
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
 *
 *
 * Based on scriptreplay.pl by Joey Hess <joey@kitenet.net>
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>

#include "nls.h"
#include "c.h"

#define SCRIPT_MIN_DELAY 0.0001		/* from original sripreplay.pl */

void __attribute__((__noreturn__))
usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [-t] timingfile [typescript] [divisor]\n"),
	      program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -t, --timing <file>     script timing output file\n"
		" -s, --typescript <file> script terminal session output file\n"
		" -d, --divisor <num>     speed up or slow down execution with time divisor\n"
		" -V, --version           output version information and exit\n"
		" -h, --help              display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static double
getnum(const char *s)
{
	double d;
	char *end;

	errno = 0;
	d = strtod(s, &end);

	if (end && *end != '\0')
		errx(EXIT_FAILURE, _("expected a number, but got '%s'"), s);

	if ((d == HUGE_VAL || d == -HUGE_VAL) && ERANGE == errno)
		err(EXIT_FAILURE, _("divisor '%s'"), s);

	if (!(d==d)) { /* did they specify "nan"? */
		errno = EINVAL;
		err(EXIT_FAILURE, _("divisor '%s'"), s);
	}
	return d;
}

static void
delay_for(double delay)
{
#ifdef HAVE_NANOSLEEP
	struct timespec ts, remainder;
	ts.tv_sec = (time_t) delay;
	ts.tv_nsec = (delay - ts.tv_sec) * 1.0e9;

	while (-1 == nanosleep(&ts, &remainder)) {
		if (EINTR == errno)
			ts = remainder;
		else
			break;
	}
#else
	struct timeval tv;
	tv.tv_sec = (long) delay;
	tv.tv_usec = (delay - tv.tv_sec) * 1.0e6;
	select(0, NULL, NULL, NULL, &tv);
#endif
}

static void
emit(FILE *fd, const char *filename, size_t ct)
{
	char buf[BUFSIZ];

	while(ct) {
		size_t len, cc;

		cc = ct > sizeof(buf) ? sizeof(buf) : ct;
		len = fread(buf, 1, cc, fd);

		if (!len)
		       break;

		ct -= len;
		cc = write(STDOUT_FILENO, buf, len);
		if (cc != len)
			err(EXIT_FAILURE, _("write to stdout failed"));
	}

	if (!ct)
		return;
	if (feof(fd))
		errx(EXIT_FAILURE, _("unexpected end of file on %s"), filename);

	err(EXIT_FAILURE, _("failed to read typescript file %s"), filename);
}


int
main(int argc, char *argv[])
{
	FILE *tfile, *sfile;
	const char *sname = NULL, *tname = NULL;
	double divi = 1;
	int c, diviopt = FALSE, idx;
	unsigned long line;
	size_t oldblk = 0;
	char ch;

	static const struct option longopts[] = {
		{ "timing",	required_argument,	0, 't' },
		{ "typescript",	required_argument,	0, 's' },
		{ "divisor",	required_argument,	0, 'd' },
		{ "version",	no_argument,		0, 'V' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL,		0, 0, 0 }
	};

	/* Because we use space as a separator, we can't afford to use any
	 * locale which tolerates a space in a number.  In any case, script.c
	 * sets the LC_NUMERIC locale to C, anyway.
	 */
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt_long(argc, argv, "t:s:d:Vh", longopts, NULL)) != -1)
		switch(ch) {
		case 't':
			tname = optarg;
			break;
		case 's':
			sname = optarg;
			break;
		case 'd':
			diviopt = TRUE;
			divi = getnum(optarg);
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
	idx = 0;

	if ((argc < 1 && !tname) || argc > 3) {
		warnx(_("wrong number of arguments"));
		usage(stderr);
	}
	if (!tname)
		tname = argv[idx++];
	if (!sname)
		sname = idx < argc ? argv[idx++] : "typescript";
	if (!diviopt)
		divi = idx < argc ? getnum(argv[idx]) : 1;

	tfile = fopen(tname, "r");
	if (!tfile)
		err(EXIT_FAILURE, _("cannot open timing file %s"), tname);
	sfile = fopen(sname, "r");
	if (!sfile)
		err(EXIT_FAILURE, _("cannot open typescript file %s"), sname);

	/* ignore the first typescript line */
	while((c = fgetc(sfile)) != EOF && c != '\n');

	for(line = 0; ; line++) {
		double delay;
		size_t blk;
		char nl;

		if (fscanf(tfile, "%lf %zd%c\n", &delay, &blk, &nl) != 3 ||
				                                 nl != '\n') {
			if (feof(tfile))
				break;
			if (ferror(tfile))
				err(EXIT_FAILURE,
					_("failed to read timing file %s"), tname);
			errx(EXIT_FAILURE,
				_("timings file %s: %lu: unexpected format"),
				tname, line);
		}
		delay /= divi;

		if (delay > SCRIPT_MIN_DELAY)
			delay_for(delay);

		if (oldblk)
			emit(sfile, sname, oldblk);
		oldblk = blk;
	}

	fclose(sfile);
	fclose(tfile);
	printf("\n");
	exit(EXIT_SUCCESS);
}
