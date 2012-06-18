/* vi: set sw=4 ts=4: */
/*
 * readahead implementation for busybox
 *
 * Preloads the given files in RAM, to reduce access time.
 * Does this by calling the readahead(2) system call.
 *
 * Copyright (C) 2006  Michael Opdenacker <michael@free-electrons.com>
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "libbb.h"

int readahead_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int readahead_main(int argc UNUSED_PARAM, char **argv)
{
	int retval = EXIT_SUCCESS;

	if (!argv[1]) {
		bb_show_usage();
	}

	while (*++argv) {
		int fd = open_or_warn(*argv, O_RDONLY);
		if (fd >= 0) {
			off_t len;
			int r;

			/* fdlength was reported to be unreliable - use seek */
			len = xlseek(fd, 0, SEEK_END);
			xlseek(fd, 0, SEEK_SET);
			r = readahead(fd, 0, len);
			close(fd);
			if (r >= 0)
				continue;
		}
		retval = EXIT_FAILURE;
	}

	return retval;
}
