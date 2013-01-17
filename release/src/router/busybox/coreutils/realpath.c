/* vi: set sw=4 ts=4: */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox extension. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Now does proper error checking on output and returns a failure exit code
 * if one or more paths cannot be resolved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define realpath_trivial_usage
//usage:       "FILE..."
//usage:#define realpath_full_usage "\n\n"
//usage:       "Return the absolute pathnames of given FILE"

#include "libbb.h"

int realpath_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int realpath_main(int argc UNUSED_PARAM, char **argv)
{
	int retval = EXIT_SUCCESS;

	if (!*++argv) {
		bb_show_usage();
	}

	do {
		char *resolved_path = xmalloc_realpath(*argv);
		if (resolved_path != NULL) {
			puts(resolved_path);
			free(resolved_path);
		} else {
			retval = EXIT_FAILURE;
			bb_simple_perror_msg(*argv);
		}
	} while (*++argv);

	fflush_stdout_and_exit(retval);
}
