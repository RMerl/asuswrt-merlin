/* vi: set sw=4 ts=4: */
/*
 * Mini chroot implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

#include "libbb.h"

int chroot_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chroot_main(int argc UNUSED_PARAM, char **argv)
{
	++argv;
	if (!*argv)
		bb_show_usage();
	xchroot(*argv);
	xchdir("/");

	++argv;
	if (!*argv) { /* no 2nd param (PROG), use shell */
		argv -= 2;
		argv[0] = getenv("SHELL");
		if (!argv[0]) {
			argv[0] = (char *) DEFAULT_SHELL;
		}
		argv[1] = (char *) "-i";
	}

	BB_EXECVP_or_die(argv);
}
