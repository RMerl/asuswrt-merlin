/* vi: set sw=4 ts=4: */
/*
 * mesg implementation for busybox
 *
 * Copyright (c) 2002 Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#ifdef USE_TTY_GROUP
#define S_IWGRP_OR_S_IWOTH	S_IWGRP
#else
#define S_IWGRP_OR_S_IWOTH	(S_IWGRP | S_IWOTH)
#endif

int mesg_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mesg_main(int argc UNUSED_PARAM, char **argv)
{
	struct stat sb;
	const char *tty;
	char c = 0;

	argv++;

	if (!argv[0]
	 || (!argv[1] && ((c = argv[0][0]) == 'y' || c == 'n'))
	) {
		tty = xmalloc_ttyname(STDERR_FILENO);
		if (tty == NULL) {
			tty = "ttyname";
		} else if (stat(tty, &sb) == 0) {
			mode_t m;
			if (c == 0) {
				puts((sb.st_mode & (S_IWGRP|S_IWOTH)) ? "is y" : "is n");
				return EXIT_SUCCESS;
			}
			m = (c == 'y') ? sb.st_mode | S_IWGRP_OR_S_IWOTH
			               : sb.st_mode & ~(S_IWGRP|S_IWOTH);
			if (chmod(tty, m) == 0) {
				return EXIT_SUCCESS;
			}
		}
		bb_simple_perror_msg_and_die(tty);
	}
	bb_show_usage();
}
