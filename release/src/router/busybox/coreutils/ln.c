/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* BB_AUDIT GNU options missing: -d, -F, -i, and -v. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/ln.html */

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


#define LN_SYMLINK          1
#define LN_FORCE            2
#define LN_NODEREFERENCE    4
#define LN_BACKUP           8
#define LN_SUFFIX           16

int ln_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ln_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int opts;
	char *last;
	char *src_name;
	char *src;
	char *suffix = (char*)"~";
	struct stat statbuf;
	int (*link_func)(const char *, const char *);

	opt_complementary = "-1"; /* min one arg */
	opts = getopt32(argv, "sfnbS:", &suffix);

	last = argv[argc - 1];
	argv += optind;

	if (argc == optind + 1) {
		*--argv = last;
		last = bb_get_last_path_component_strip(xstrdup(last));
	}

	do {
		src_name = NULL;
		src = last;

		if (is_directory(src,
		                (opts & LN_NODEREFERENCE) ^ LN_NODEREFERENCE,
		                NULL)
		) {
			src_name = xstrdup(*argv);
			src = concat_path_file(src, bb_get_last_path_component_strip(src_name));
			free(src_name);
			src_name = src;
		}
		if (!(opts & LN_SYMLINK) && stat(*argv, &statbuf)) {
			// coreutils: "ln dangling_symlink new_hardlink" works
			if (lstat(*argv, &statbuf) || !S_ISLNK(statbuf.st_mode)) {
				bb_simple_perror_msg(*argv);
				status = EXIT_FAILURE;
				free(src_name);
				continue;
			}
		}

		if (opts & LN_BACKUP) {
			char *backup;
			backup = xasprintf("%s%s", src, suffix);
			if (rename(src, backup) < 0 && errno != ENOENT) {
				bb_simple_perror_msg(src);
				status = EXIT_FAILURE;
				free(backup);
				continue;
			}
			free(backup);
			/*
			 * When the source and dest are both hard links to the same
			 * inode, a rename may succeed even though nothing happened.
			 * Therefore, always unlink().
			 */
			unlink(src);
		} else if (opts & LN_FORCE) {
			unlink(src);
		}

		link_func = link;
		if (opts & LN_SYMLINK) {
			link_func = symlink;
		}

		if (link_func(*argv, src) != 0) {
			bb_simple_perror_msg(src);
			status = EXIT_FAILURE;
		}

		free(src_name);

	} while ((++argv)[1]);

	return status;
}
