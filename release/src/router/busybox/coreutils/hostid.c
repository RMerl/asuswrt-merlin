/* vi: set sw=4 ts=4: */
/*
 * Mini hostid implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int hostid_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hostid_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	if (argv[1]) {
		bb_show_usage();
	}

	printf("%lx\n", gethostid());

	return fflush_all();
}
