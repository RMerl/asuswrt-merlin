/* vi: set sw=4 ts=4: */
/*
 * printenv implementation for busybox
 *
 * Copyright (C) 2005 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Mike Frysinger <vapier@gentoo.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int printenv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int printenv_main(int argc UNUSED_PARAM, char **argv)
{
	int exit_code = EXIT_SUCCESS;

	/* no variables specified, show whole env */
	if (!argv[1]) {
		int e = 0;
		while (environ[e])
			puts(environ[e++]);
	} else {
		/* search for specified variables and print them out if found */
		char *arg, *env;

		while ((arg = *++argv) != NULL) {
			env = getenv(arg);
			if (env)
				puts(env);
			else
				exit_code = EXIT_FAILURE;
		}
	}

	fflush_stdout_and_exit(exit_code);
}
