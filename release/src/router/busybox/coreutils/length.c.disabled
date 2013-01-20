/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox (obsolete?) extension. */

//usage:#define length_trivial_usage
//usage:       "STRING"
//usage:#define length_full_usage "\n\n"
//usage:       "Print STRING's length"
//usage:
//usage:#define length_example_usage
//usage:       "$ length Hello\n"
//usage:       "5\n"

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
