/*
 * fsfreeze.c -- Filesystem freeze/unfreeze IO for Linux
 *
 * Copyright (C) 2010 Hajime Taira <htaira@redhat.com>
 *                    Masatake Yamato <yamato@redhat.com>
 *
 * This program is free software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation: either version 1 or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#include "blkdev.h"
#include "nls.h"
#include "c.h"

static int freeze_f(int fd)
{
	return ioctl(fd, FIFREEZE, 0);
}

static int unfreeze_f(int fd)
{
	return ioctl(fd, FITHAW, 0);
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] <mount point>\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -h, --help        this help\n"
		" -f, --freeze      freeze the filesystem\n"
		" -u, --unfreeze    unfreeze the filesystem\n"), out);

	fputs(_("\nFor more information see fsfreeze(8).\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int fd = -1, c;
	int freeze = -1, rc = EXIT_FAILURE;
	char *path;
	struct stat sb;

	static const struct option longopts[] = {
	    { "help",      0, 0, 'h' },
	    { "freeze",    0, 0, 'f' },
	    { "unfreeze",  0, 0, 'u' },
	    { NULL,        0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "hfu", longopts, NULL)) != -1) {
		switch(c) {
		case 'h':
			usage(stdout);
			break;
		case 'f':
			freeze = TRUE;
			break;
		case 'u':
			freeze = FALSE;
			break;
		default:
			usage(stderr);
			break;
		}
	}

	if (freeze == -1)
		errx(EXIT_FAILURE, _("no action specified"));
	if (optind == argc)
		errx(EXIT_FAILURE, _("no filename specified"));
	path = argv[optind++];

	if (optind != argc) {
		warnx(_("unexpected number of arguments"));
		usage(stderr);
	}

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, _("%s: open failed"), path);

	if (fstat(fd, &sb) == -1) {
		warn(_("%s: fstat failed"), path);
		goto done;
	}

	if (!S_ISDIR(sb.st_mode)) {
		warnx(_("%s: is not a directory"), path);
		goto done;
	}

	if (freeze) {
		if (freeze_f(fd)) {
			warn(_("%s: freeze failed"), path);
			goto done;
		}
	} else {
		if (unfreeze_f(fd)) {
			warn(_("%s: unfreeze failed"), path);
			goto done;
		}
	}

	rc = EXIT_SUCCESS;
done:
	if (fd >= 0)
		close(fd);
	return rc;
}

