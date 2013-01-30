/* vi: set sw=4 ts=4: */
/*
 * tty implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv4 compliant */
/* http://www.opengroup.org/onlinepubs/9699919799/utilities/tty.html */

#include "libbb.h"

int tty_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tty_main(int argc UNUSED_PARAM, char **argv)
{
	const char *s;
	IF_INCLUDE_SUSv2(int silent;)	/* Note: No longer relevant in SUSv3. */
	int retval;

	xfunc_error_retval = 2;	/* SUSv3 requires > 1 for error. */

	IF_INCLUDE_SUSv2(silent = getopt32(argv, "s");)
	IF_INCLUDE_SUSv2(argv += optind;)
	IF_NOT_INCLUDE_SUSv2(argv += 1;)

	/* gnu tty outputs a warning that it is ignoring all args. */
	bb_warn_ignoring_args(argv[0]);

	retval = EXIT_SUCCESS;

	s = xmalloc_ttyname(STDIN_FILENO);
	if (s == NULL) {
	/* According to SUSv3, ttyname can fail with EBADF or ENOTTY.
	 * We know the file descriptor is good, so failure means not a tty. */
		s = "not a tty";
		retval = EXIT_FAILURE;
	}
	IF_INCLUDE_SUSv2(if (!silent) puts(s);)
	IF_NOT_INCLUDE_SUSv2(puts(s);)

	fflush_stdout_and_exit(retval);
}
