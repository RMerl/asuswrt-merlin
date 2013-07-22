/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <blkid.h>

#include "nls.h"
#include "c.h"

static void __attribute__((__noreturn__)) usage(int rc)
{
	const char *p = program_invocation_short_name;

	if (!p)
		p = "findfs";

	fprintf(stderr, _("Usage: %s LABEL=<label>|UUID=<uuid>\n"), p);
	exit(rc);
}

int main(int argc, char **argv)
{
	char	*dev, *tk, *vl;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (argc != 2)
		/* we return '2' for backward compatibility
		 * with version from e2fsprogs */
		usage(2);

	if (!strncmp(argv[1], "LABEL=", 6)) {
		tk = "LABEL";
		vl = argv[1] + 6;
	} else if (!strncmp(argv[1], "UUID=", 5)) {
		tk = "UUID";
		vl = argv[1] + 5;
	} else if (!strcmp(argv[1], "-h") == 0 ||
		   !strcmp(argv[1], "--help") == 0) {
		usage(EXIT_SUCCESS);
	} else
		usage(2);

	dev = blkid_evaluate_tag(tk, vl, NULL);
	if (!dev)
		errx(EXIT_FAILURE, _("unable to resolve '%s'"),	argv[1]);

	puts(dev);
	exit(EXIT_SUCCESS);
}

