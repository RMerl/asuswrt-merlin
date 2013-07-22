/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Author: Adrian Hunter
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "tests.h"

#define MAX_ORPHANS 1000000

void orph(void)
{
	pid_t pid;
	unsigned i, j, k, n;
	int fd, done, full;
	int64_t repeat;
	ssize_t sz;
	char dir_name[256];
	int fds[MAX_ORPHANS];

	/* Create a directory to test in */
	pid = getpid();
	tests_cat_pid(dir_name, "orph_test_dir_", pid);
	if (chdir(dir_name) == -1)
		CHECK(mkdir(dir_name, 0777) != -1);
	CHECK(chdir(dir_name) != -1);

	repeat = tests_repeat_parameter;
	for (;;) {
		full = 0;
		done = 0;
		n = 0;
		while (n + 100 < MAX_ORPHANS && !done) {
			/* Make 100 more orphans */
			for (i = 0; i < 100; i++) {
				fd = tests_create_orphan(n + i);
				if (fd < 0) {
					done = 1;
					if (errno == ENOSPC)
						full = 1;
					else if (errno != EMFILE)
						CHECK(0);
					errno = 0;
					break;
				}
				fds[n + i] = fd;
			}
			if (!full) {
				/* Write to orphans just created */
				k = i;
				for (i = 0; i < k; i++) {
					if (tests_write_fragment_file(n + i,
								      fds[n+i],
								      0, 1000)
					    != 1000) {
						/*
						 * Out of space, so close
						 * remaining files
						 */
						for (j = i; j < k; j++)
							CHECK(close(fds[n + j])
							      != -1);
						done = 1;
						break;
					}
				}
			}
			if (!done)
				CHECK(tests_count_files_in_dir(".") == 0);
			n += i;
		}
		/* Check the data in the files */
		for (i = 0; i < n; i++)
			tests_check_fragment_file_fd(i, fds[i]);
		if (!full && n) {
			/* Ensure the file system is full */
			n -= 1;
			do {
				sz = write(fds[n], fds, 4096);
				if (sz == -1 && errno == ENOSPC) {
					errno = 0;
					break;
				}
				CHECK(sz >= 0);
			} while (sz == 4096);
			CHECK(close(fds[n]) != -1);
		}
		/* Check the data in the files */
		for (i = 0; i < n; i++)
			tests_check_fragment_file_fd(i, fds[i]);
		/* Sleep */
		if (tests_sleep_parameter > 0) {
			unsigned us = tests_sleep_parameter * 1000;
			unsigned rand_divisor = RAND_MAX / us;
			unsigned s = (us / 2) + (rand() / rand_divisor);
			usleep(s);
		}
		/* Close orphans */
		for (i = 0; i < n; i++)
			CHECK(close(fds[i]) != -1);
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
	}
	CHECK(tests_count_files_in_dir(".") == 0);
	CHECK(chdir("..") != -1);
	CHECK(rmdir(dir_name) != -1);
}

/* Title of this test */

const char *orph_get_title(void)
{
	return "Create many open unlinked files";
}

/* Description of this test */

const char *orph_get_description(void)
{
	return
		"Create a directory named orph_test_dir_pid, where " \
		"pid is the process id.  Within that directory, " \
		"create files and keep them open and unlink them. " \
		"Create as many files as possible until the file system is " \
		"full or the maximum allowed open files is reached. " \
		"If a sleep value is specified, the process sleeps. " \
		"The sleep value is given by the -p or --sleep option, " \
		"otherwise it defaults to 0. " \
		"Sleep is specified in milliseconds. " \
		"Then close the files. " \
		"If a repeat count is specified, then the task repeats " \
		"that number of times. " \
		"The repeat count is given by the -n or --repeat option, " \
		"otherwise it defaults to 1. " \
		"A repeat count of zero repeats forever. " \
		"Finally remove the directory.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test repetition */
	tests_repeat_parameter = 1;

	/* Set default test sleep */
	tests_sleep_parameter = 0;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, orph_get_title(),
			orph_get_description(), "nps");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	orph();
	return 0;
}
