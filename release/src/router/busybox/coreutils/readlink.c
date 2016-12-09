/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 * Copyright (C) 2000,2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define readlink_trivial_usage
//usage:	IF_FEATURE_READLINK_FOLLOW("[-fnv] ") "FILE"
//usage:#define readlink_full_usage "\n\n"
//usage:       "Display the value of a symlink"
//usage:	IF_FEATURE_READLINK_FOLLOW( "\n"
//usage:     "\n	-f	Canonicalize by following all symlinks"
//usage:     "\n	-n	Don't add newline"
//usage:     "\n	-v	Verbose"
//usage:	)

#include "libbb.h"

/*
 * # readlink --version
 * readlink (GNU coreutils) 6.10
 * # readlink --help
 *   -f, --canonicalize
 *      canonicalize by following every symlink in
 *      every component of the given name recursively;
 *      all but the last component must exist
 *   -e, --canonicalize-existing
 *      canonicalize by following every symlink in
 *      every component of the given name recursively,
 *      all components must exist
 *   -m, --canonicalize-missing
 *      canonicalize by following every symlink in
 *      every component of the given name recursively,
 *      without requirements on components existence
 *   -n, --no-newline              do not output the trailing newline
 *   -q, --quiet, -s, --silent     suppress most error messages
 *   -v, --verbose                 report error messages
 *
 * bbox supports: -f (partially) -n -v (fully), -q -s (accepts but ignores)
 * Note: we export the -f flag, but our -f behaves like coreutils' -e.
 * Unfortunately, there isn't a C lib function we can leverage to get this
 * behavior which means we'd have to implement the full stack ourselves :(.
 */

int readlink_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int readlink_main(int argc UNUSED_PARAM, char **argv)
{
	char *buf;
	char *fname;

	IF_FEATURE_READLINK_FOLLOW(
		unsigned opt;
		/* We need exactly one non-option argument.  */
		opt_complementary = "=1";
		opt = getopt32(argv, "fnvsq");
		fname = argv[optind];
	)
	IF_NOT_FEATURE_READLINK_FOLLOW(
		const unsigned opt = 0;
		if (argc != 2) bb_show_usage();
		fname = argv[1];
	)

	/* compat: coreutils readlink reports errors silently via exit code */
	if (!(opt & 4)) /* not -v */
		logmode = LOGMODE_NONE;

	if (opt & 1) { /* -f */
		buf = xmalloc_realpath(fname);
	} else {
		buf = xmalloc_readlink_or_warn(fname);
	}

	if (!buf)
		return EXIT_FAILURE;
	printf((opt & 2) ? "%s" : "%s\n", buf);

	if (ENABLE_FEATURE_CLEAN_UP)
		free(buf);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
