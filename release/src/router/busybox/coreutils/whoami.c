/* vi: set sw=4 ts=4: */
/*
 * Mini whoami implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

//usage:#define whoami_trivial_usage
//usage:       ""
//usage:#define whoami_full_usage "\n\n"
//usage:       "Print the user name associated with the current effective user id"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int whoami_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int whoami_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	if (argv[1])
		bb_show_usage();

	/* Will complain and die if username not found */
	puts(xuid2uname(geteuid()));

	return fflush_all();
}
