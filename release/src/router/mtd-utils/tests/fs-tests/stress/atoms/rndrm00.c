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

void rndrm00(void)
{
	int64_t repeat;
	int64_t size, this_size;
	pid_t pid;
	char dir_name[256];

	/* Create a directory to test in */
	pid = getpid();
	tests_cat_pid(dir_name, "rndrm00_test_dir_", pid);
	if (chdir(dir_name) == -1)
		CHECK(mkdir(dir_name, 0777) != -1);
	CHECK(chdir(dir_name) != -1);
	/* Repeat loop */
	repeat = tests_repeat_parameter;
	size = 0;
	for (;;) {
		/* Create and remove sub-dirs and small files, */
		/* but tending to grow */
		do {
			if (tests_random_no(3)) {
				this_size = tests_create_entry(NULL);
				if (!this_size)
					break;
				size += this_size;
			} else {
				this_size = tests_remove_entry();
				size -= this_size;
				if (size < 0)
					size = 0;
				if (!this_size)
					this_size = 1;
			}
		} while (this_size &&
			(tests_size_parameter == 0 ||
			size < tests_size_parameter));
		/* Create and remove sub-dirs and small files, but */
		/* but tending to shrink */
		do {
			if (!tests_random_no(3)) {
				this_size = tests_create_entry(NULL);
				size += this_size;
			} else {
				this_size = tests_remove_entry();
				size -= this_size;
				if (size < 0)
					size = 0;
			}
		} while ((tests_size_parameter != 0 &&
			size > tests_size_parameter / 10) ||
			(tests_size_parameter == 0 && size > 100000));
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
		/* Sleep */
		if (tests_sleep_parameter > 0) {
			unsigned us = tests_sleep_parameter * 1000;
			unsigned rand_divisor = RAND_MAX / us;
			unsigned s = (us / 2) + (rand() / rand_divisor);
			usleep(s);
		}
	}
	/* Tidy up by removing everything */
	tests_clear_dir(".");
	CHECK(chdir("..") != -1);
	CHECK(rmdir(dir_name) != -1);
}

/* Title of this test */

const char *rndrm00_get_title(void)
{
	return "Randomly create and remove directories and files";
}

/* Description of this test */

const char *rndrm00_get_description(void)
{
	return
		"Create a directory named rndrm00_test_dir_pid, where " \
		"pid is the process id.  Within that directory, " \
		"randomly create and remove " \
		"a number of sub-directories and small files, " \
		"but do more creates than removes. " \
		"When the total size of all sub-directories and files " \
		"is greater than the size specified by the size parameter, " \
		"start to do more removes than creates. " \
		"The size parameter is given by the -z or --size option, " \
		"otherwise it defaults to 1000000. " \
		"A size of zero fills the file system until there is no "
		"space left. " \
		"The task repeats, sleeping in between each iteration. " \
		"The repeat count is set by the -n or --repeat option, " \
		"otherwise it defaults to 1. " \
		"A repeat count of zero repeats forever. " \
		"The sleep value is given by the -p or --sleep option, " \
		"otherwise it defaults to 0. "
		"Sleep is specified in milliseconds.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test size */
	tests_size_parameter = 1000000;

	/* Set default test repetition */
	tests_repeat_parameter = 1;

	/* Set default test sleep */
	tests_sleep_parameter = 0;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, rndrm00_get_title(),
			rndrm00_get_description(), "znp");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	rndrm00();
	return 0;
}
