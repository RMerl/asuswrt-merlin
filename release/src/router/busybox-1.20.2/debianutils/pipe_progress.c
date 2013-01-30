/* vi: set sw=4 ts=4: */
/*
 * Monitor a pipe with a simple progress display.
 *
 * Copyright (C) 2003 by Rob Landley <rob@landley.net>, Joey Hess
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define pipe_progress_trivial_usage NOUSAGE_STR
//usage:#define pipe_progress_full_usage ""

#include "libbb.h"

#define PIPE_PROGRESS_SIZE 4096

/* Read a block of data from stdin, write it to stdout.
 * Activity is indicated by a '.' to stderr
 */
int pipe_progress_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pipe_progress_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char buf[PIPE_PROGRESS_SIZE];
	time_t t = time(NULL);
	int len;

	while ((len = safe_read(STDIN_FILENO, buf, PIPE_PROGRESS_SIZE)) > 0) {
		time_t new_time = time(NULL);
		if (new_time != t) {
			t = new_time;
			bb_putchar_stderr('.');
		}
		full_write(STDOUT_FILENO, buf, len);
	}

	bb_putchar_stderr('\n');

	return 0;
}
