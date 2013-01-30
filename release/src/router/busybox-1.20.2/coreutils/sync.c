/* vi: set sw=4 ts=4: */
/*
 * Mini sync implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

//usage:#define sync_trivial_usage
//usage:       ""
//usage:#define sync_full_usage "\n\n"
//usage:       "Write all buffered blocks to disk"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int sync_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sync_main(int argc UNUSED_PARAM, char **argv IF_NOT_DESKTOP(UNUSED_PARAM))
{
	/* coreutils-6.9 compat */
	bb_warn_ignoring_args(argv[1]);

	sync();

	return EXIT_SUCCESS;
}
