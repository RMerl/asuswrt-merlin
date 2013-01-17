/* vi: set sw=4 ts=4: */
/*
 * yes implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reductions and removed redundant applet name prefix from error messages.
 */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

//usage:#define yes_trivial_usage
//usage:       "[STRING]"
//usage:#define yes_full_usage "\n\n"
//usage:       "Repeatedly output a line with STRING, or 'y'"

int yes_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int yes_main(int argc UNUSED_PARAM, char **argv)
{
	char **pp;

	argv[0] = (char*)"y";
	if (argv[1])
		++argv;

	do {
		pp = argv;
		while (1) {
			fputs(*pp, stdout);
			if (!*++pp)
				break;
			putchar(' ');
		}
	} while (putchar('\n') != EOF);

	bb_perror_nomsg_and_die();
}
