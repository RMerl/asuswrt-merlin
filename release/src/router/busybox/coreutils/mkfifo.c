/* vi: set sw=4 ts=4: */
/*
 * mkfifo implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/mkfifo.html */

#include "libbb.h"
#include "libcoreutils/coreutils.h"

int mkfifo_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkfifo_main(int argc UNUSED_PARAM, char **argv)
{
	mode_t mode;
	int retval = EXIT_SUCCESS;

	mode = getopt_mk_fifo_nod(argv);

	argv += optind;
	if (!*argv) {
		bb_show_usage();
	}

	do {
		if (mkfifo(*argv, mode) < 0) {
			bb_simple_perror_msg(*argv);	/* Avoid multibyte problems. */
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}
