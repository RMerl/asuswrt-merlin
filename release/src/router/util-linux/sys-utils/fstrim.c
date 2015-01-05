/*
 * fstrim.c -- discard the part (or whole) of mounted filesystem.
 *
 * Copyright (C) 2010 Red Hat, Inc. All rights reserved.
 * Written by Lukas Czerner <lczerner@redhat.com>
 *            Karel Zak <kzak@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This program uses FITRIM ioctl to discard parts or the whole filesystem
 * online (mounted). You can specify range (start and length) to be
 * discarded, or simply discard whole filesystem.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

#include "nls.h"
#include "strutils.h"
#include "c.h"

#ifndef FITRIM
struct fstrim_range {
	uint64_t start;
	uint64_t len;
	uint64_t minlen;
};
#define FITRIM		_IOWR('X', 121, struct fstrim_range)
#endif

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] <mount point>\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -h, --help          this help\n"
		" -o, --offset <num>  offset in bytes to discard from\n"
		" -l, --length <num>  length of bytes to discard from the offset\n"
		" -m, --minimum <num> minimum extent length to discard\n"
		" -v, --verbose       print number of discarded bytes\n"), out);

	fputs(_("\nFor more information see fstrim(8).\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *path;
	int c, fd, verbose = 0;
	struct fstrim_range range;
	struct stat sb;

	static const struct option longopts[] = {
	    { "help",      0, 0, 'h' },
	    { "offset",    1, 0, 'o' },
	    { "length",    1, 0, 'l' },
	    { "minimum",   1, 0, 'm' },
	    { "verbose",   0, 0, 'v' },
	    { NULL,        0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	memset(&range, 0, sizeof(range));
	range.len = ULLONG_MAX;

	while ((c = getopt_long(argc, argv, "ho:l:m:v", longopts, NULL)) != -1) {
		switch(c) {
		case 'h':
			usage(stdout);
			break;
		case 'l':
			if (strtosize(optarg, (uint64_t *) &range.len))
				errx(EXIT_FAILURE,
				     _("failed to parse length: %s"), optarg);
			break;
		case 'o':
			if (strtosize(optarg, (uint64_t *) &range.start))
				errx(EXIT_FAILURE,
				     _("failed to parse offset: %s"), optarg);
			break;
		case 'm':
			if (strtosize(optarg, (uint64_t *) &range.minlen))
				errx(EXIT_FAILURE,
				     _("failed to parse minimum extent length: %s"),
				     optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(stderr);
			break;
		}
	}

	if (optind == argc)
		errx(EXIT_FAILURE, _("no mountpoint specified."));

	path = argv[optind++];

	if (optind != argc) {
		warnx(_("unexpected number of arguments"));
		usage(stderr);
	}

	if (stat(path, &sb) == -1)
		err(EXIT_FAILURE, _("%s: stat failed"), path);
	if (!S_ISDIR(sb.st_mode))
		errx(EXIT_FAILURE, _("%s: not a directory"), path);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, _("%s: open failed"), path);

	if (ioctl(fd, FITRIM, &range))
		err(EXIT_FAILURE, _("%s: FITRIM ioctl failed"), path);

	if (verbose)
		/* TRANSLATORS: The standard value here is a very large number. */
		printf(_("%s: %" PRIu64 " bytes were trimmed\n"),
						path, (uint64_t) range.len);
	close(fd);
	return EXIT_SUCCESS;
}
