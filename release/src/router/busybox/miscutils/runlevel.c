/* vi: set sw=4 ts=4: */
/*
 * runlevel	Prints out the previous and the current runlevel.
 *
 * Version:	@(#)runlevel  1.20  16-Apr-1997  MvS
 *
 *		This file is part of the sysvinit suite,
 *		Copyright 1991-1997 Miquel van Smoorenburg.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * initially busyboxified by Bernhard Reutner-Fischer
 */
#include "libbb.h"
#include <utmp.h>

int runlevel_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int runlevel_main(int argc UNUSED_PARAM, char **argv)
{
	struct utmp *ut;
	char prev;

	if (argv[1]) utmpname(argv[1]);

	setutent();
	while ((ut = getutent()) != NULL) {
		if (ut->ut_type == RUN_LVL) {
			prev = ut->ut_pid / 256;
			if (prev == 0) prev = 'N';
			printf("%c %c\n", prev, ut->ut_pid % 256);
			if (ENABLE_FEATURE_CLEAN_UP)
				endutent();
			return 0;
		}
	}

	puts("unknown");

	if (ENABLE_FEATURE_CLEAN_UP)
		endutent();
	return 1;
}
