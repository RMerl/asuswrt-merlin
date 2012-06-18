/* vi: set sw=4 ts=4: */
/*
 * Mini touch implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 _NOT_ compliant -- options -a, -m, -r, -t not supported. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/touch.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Previous version called open() and then utime().  While this will be
 * be necessary to implement -r and -t, it currently only makes things bigger.
 * Also, exiting on a failure was a bug.  All args should be processed.
 */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

/* coreutils implements:
 * -a   change only the access time
 * -c, --no-create
 *      do not create any files
 * -d, --date=STRING
 *      parse STRING and use it instead of current time
 * -f   (ignored, BSD compat)
 * -m   change only the modification time
 * -r, --reference=FILE
 *      use this file's times instead of current time
 * -t STAMP
 *      use [[CC]YY]MMDDhhmm[.ss] instead of current time
 * --time=WORD
 *      change the specified time: WORD is access, atime, or use
 */

int touch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int touch_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	int status = EXIT_SUCCESS;
	int opts;
#if ENABLE_DESKTOP
# if ENABLE_LONG_OPTS
	static const char touch_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"no-create\0"         No_argument       "c"
		"reference\0"         Required_argument "r"
		"date\0"              Required_argument "d"
	;
# endif
	char *reference_file = NULL;
	char *date_str = NULL;
	struct timeval timebuf[2];
	timebuf[1].tv_usec = timebuf[0].tv_usec = 0;
#else
# define reference_file NULL
# define date_str       NULL
# define timebuf        ((struct timeval*)NULL)
#endif

#if ENABLE_DESKTOP && ENABLE_LONG_OPTS
	applet_long_options = touch_longopts;
#endif
	/* -d and -t both set time. In coreutils,
	 * accepted data format differs a bit between -d and -t.
	 * We accept the same formats for both */
	opts = getopt32(argv, "c" IF_DESKTOP("r:d:t:")
				/*ignored:*/ "fma"
				IF_DESKTOP(, &reference_file)
				IF_DESKTOP(, &date_str)
				IF_DESKTOP(, &date_str)
	);

	opts &= 1; /* only -c bit is left */
	argv += optind;
	if (!*argv) {
		bb_show_usage();
	}

	if (reference_file) {
		struct stat stbuf;
		xstat(reference_file, &stbuf);
		timebuf[1].tv_sec = timebuf[0].tv_sec = stbuf.st_mtime;
	}

	if (date_str) {
		struct tm tm_time;
		time_t t;

		//time(&t);
		//localtime_r(&t, &tm_time);
		memset(&tm_time, 0, sizeof(tm_time));
		parse_datestr(date_str, &tm_time);

		/* Correct any day of week and day of year etc. fields */
		tm_time.tm_isdst = -1;	/* Be sure to recheck dst */
		t = validate_tm_time(date_str, &tm_time);

		timebuf[1].tv_sec = timebuf[0].tv_sec = t;
	}

	do {
		if (utimes(*argv, (reference_file || date_str) ? timebuf : NULL) != 0) {
			if (errno == ENOENT) { /* no such file */
				if (opts) { /* creation is disabled, so ignore */
					continue;
				}
				/* Try to create the file */
				fd = open(*argv, O_RDWR | O_CREAT, 0666);
				if (fd >= 0) {
					xclose(fd);
					if (reference_file || date_str)
						utimes(*argv, timebuf);
					continue;
				}
			}
			status = EXIT_FAILURE;
			bb_simple_perror_msg(*argv);
		}
	} while (*++argv);

	return status;
}
