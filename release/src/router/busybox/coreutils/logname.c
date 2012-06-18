/* vi: set sw=4 ts=4: */
/*
 * Mini logname implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/logname.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * SUSv3 specifies the string used is that returned from getlogin().
 * The previous implementation used getpwuid() for geteuid(), which
 * is _not_ the same.  Erik apparently made this change almost 3 years
 * ago to avoid failing when no utmp was available.  However, the
 * correct course of action wrt SUSv3 for a failing getlogin() is
 * a diagnostic message and an error return.
 */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int logname_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int logname_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char buf[64];

	if (argv[1]) {
		bb_show_usage();
	}

	/* Using _r function - avoid pulling in static buffer from libc */
	if (getlogin_r(buf, sizeof(buf)) == 0) {
		puts(buf);
		return fflush_all();
	}

	bb_perror_msg_and_die("getlogin");
}
