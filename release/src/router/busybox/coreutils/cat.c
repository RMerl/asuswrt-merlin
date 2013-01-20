/* vi: set sw=4 ts=4: */
/*
 * cat implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

//kbuild:lib-$(CONFIG_CAT)     += cat.o
//kbuild:lib-$(CONFIG_MORE)    += cat.o # more uses it if stdout isn't a tty
//kbuild:lib-$(CONFIG_LESS)    += cat.o # less too
//kbuild:lib-$(CONFIG_CRONTAB) += cat.o # crontab -l

//config:config CAT
//config:	bool "cat"
//config:	default y
//config:	help
//config:	  cat is used to concatenate files and print them to the standard
//config:	  output. Enable this option if you wish to enable the 'cat' utility.

//usage:#define cat_trivial_usage
//usage:       "[FILE]..."
//usage:#define cat_full_usage "\n\n"
//usage:       "Concatenate FILEs and print them to stdout"
//usage:
//usage:#define cat_example_usage
//usage:       "$ cat /proc/uptime\n"
//usage:       "110716.72 17.67"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


int bb_cat(char **argv)
{
	int fd;
	int retval = EXIT_SUCCESS;

	if (!*argv)
		argv = (char**) &bb_argv_dash;

	do {
		fd = open_or_warn_stdin(*argv);
		if (fd >= 0) {
			/* This is not a xfunc - never exits */
			off_t r = bb_copyfd_eof(fd, STDOUT_FILENO);
			if (fd != STDIN_FILENO)
				close(fd);
			if (r >= 0)
				continue;
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}

int cat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cat_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "u");
	argv += optind;
	return bb_cat(argv);
}
