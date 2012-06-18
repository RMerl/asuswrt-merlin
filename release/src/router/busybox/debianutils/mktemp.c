/* vi: set sw=4 ts=4: */
/*
 * Mini mktemp implementation for busybox
 *
 *
 * Copyright (C) 2000 by Daniel Jacobowitz
 * Written by Daniel Jacobowitz <dan@debian.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/* Coreutils 6.12 man page says:
 *        mktemp [OPTION]... [TEMPLATE]
 * Create a temporary file or directory, safely, and print its name. If
 * TEMPLATE is not specified, use tmp.XXXXXXXXXX.
 * -d, --directory
 *        create a directory, not a file
 * -q, --quiet
 *        suppress diagnostics about file/dir-creation failure
 * -u, --dry-run
 *        do not create anything; merely print a name (unsafe)
 * --tmpdir[=DIR]
 *        interpret TEMPLATE relative to DIR. If DIR is not specified,
 *        use  $TMPDIR if set, else /tmp.  With this option, TEMPLATE must
 *        not be an absolute name. Unlike with -t, TEMPLATE may contain
 *        slashes, but even here, mktemp still creates only the final com-
 *        ponent.
 * -p DIR use DIR as a prefix; implies -t [deprecated]
 * -t     interpret TEMPLATE as a single file name component, relative  to
 *        a  directory:  $TMPDIR, if set; else the directory specified via
 *        -p; else /tmp [deprecated]
 */


#include "libbb.h"

int mktemp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mktemp_main(int argc UNUSED_PARAM, char **argv)
{
	const char *path;
	char *chp;
	unsigned opts;

	path = getenv("TMPDIR");
	if (!path || path[0] == '\0')
		path = "/tmp";

	/* -q and -t are ignored */
	opt_complementary = "?1"; /* 1 argument max */
	opts = getopt32(argv, "dqtp:", &path);

	chp = argv[optind] ? argv[optind] : xstrdup("tmp.XXXXXX");
	if (!strchr(chp, '/') || (opts & 8))
		chp = concat_path_file(path, chp);

	if (opts & 1) { /* -d */
		if (mkdtemp(chp) == NULL)
			return EXIT_FAILURE;
	} else {
		if (mkstemp(chp) < 0)
			return EXIT_FAILURE;
	}

	puts(chp);

	return EXIT_SUCCESS;
}
