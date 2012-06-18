/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox (obsolete?) extension. */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int length_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int length_main(int argc, char **argv)
{
	if ((argc != 2) || (**(++argv) == '-')) {
		bb_show_usage();
	}

	printf("%u\n", (unsigned)strlen(*argv));

	return fflush_all();
}
