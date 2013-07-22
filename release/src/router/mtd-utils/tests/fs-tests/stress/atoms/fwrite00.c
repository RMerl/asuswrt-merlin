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

#define WRITE_BUFFER_SIZE 32768

#define HOLE_BLOCK_SIZE 10000000

void filestress00(void)
{
	int fd, i, deleted;
	pid_t pid;
	ssize_t written;
	int64_t remains;
	int64_t repeat;
	size_t block;
	char file_name[256];
	char buf[WRITE_BUFFER_SIZE];

	fd = -1;
	deleted = 1;
	pid = getpid();
	tests_cat_pid(file_name, "filestress00_test_file_", pid);
	srand(pid);
	repeat = tests_repeat_parameter;
	for (;;) {
		/* Open the file */
		if (fd == -1) {
			fd = open(file_name, O_CREAT | O_WRONLY,
			  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (fd == -1 && errno == ENOSPC) {
				/* Break if repeat count exceeded */
				if (tests_repeat_parameter > 0 && --repeat <= 0)
					break;
				/* Sleep 2 secs and try again */
				sleep(2);
				continue;
			}
			CHECK(fd != -1);
			deleted = 0;
			if (tests_unlink_flag) {
				CHECK(unlink(file_name) != -1);
				deleted = 1;
			}
		}
		/* Get a different set of random data */
		for (i = 0; i < WRITE_BUFFER_SIZE;++i)
			buf[i] = rand();
		if (tests_hole_flag) {
			/* Make a hole */
			CHECK(lseek(fd, tests_size_parameter, SEEK_SET) != -1);
			written = write(fd, "!", 1);
			if (written <= 0) {
				/* File system full */
				CHECK(errno == ENOSPC);
				errno = 0;
			}
			CHECK(lseek(fd, 0, SEEK_SET) != -1);
			/* Write at set points into the hole */
			remains = tests_size_parameter;
			while (remains > HOLE_BLOCK_SIZE) {
				CHECK(lseek(fd, HOLE_BLOCK_SIZE,
						SEEK_CUR) != -1);
				written = write(fd, "!", 1);
				remains -= HOLE_BLOCK_SIZE;
				if (written <= 0) {
					/* File system full */
					CHECK(errno == ENOSPC);
					errno = 0;
					break;
				}
			}
		} else {
			/* Write data into the file */
			CHECK(lseek(fd, 0, SEEK_SET) != -1);
			remains = tests_size_parameter;
			while (remains > 0) {
				if (remains > WRITE_BUFFER_SIZE)
					block = WRITE_BUFFER_SIZE;
				else
					block = remains;
				written = write(fd, buf, block);
				if (written <= 0) {
					/* File system full */
					CHECK(errno == ENOSPC);
					errno = 0;
					break;
				}
				remains -= written;
			}
		}
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
		/* Close if tests_close_flag */
		if (tests_close_flag) {
			CHECK(close(fd) != -1);
			fd = -1;
		}
		/* Sleep */
		if (tests_sleep_parameter > 0) {
			unsigned us = tests_sleep_parameter * 1000;
			unsigned rand_divisor = RAND_MAX / us;
			unsigned s = (us / 2) + (rand() / rand_divisor);
			usleep(s);
		}
		/* Delete if tests_delete flag */
		if (!deleted && tests_delete_flag) {
			CHECK(unlink(file_name) != -1);
			deleted = 1;
		}
	}
	CHECK(close(fd) != -1);
	/* Sleep */
	if (tests_sleep_parameter > 0) {
		unsigned us = tests_sleep_parameter * 1000;
		unsigned rand_divisor = RAND_MAX / us;
		unsigned s = (us / 2) + (rand() / rand_divisor);
		usleep(s);
	}
	/* Tidy up */
	if (!deleted)
		CHECK(unlink(file_name) != -1);
}

/* Title of this test */

const char *filestress00_get_title(void)
{
	return "File stress test 00";
}

/* Description of this test */

const char *filestress00_get_description(void)
{
	return
		"Create a file named filestress00_test_file_pid, where " \
		"pid is the process id.  If the unlink option " \
		"(-u or --unlink) is specified, " \
		"unlink the file while holding the open file descriptor. " \
		"If the hole option (-o or --hole) is specified, " \
		"write a single character at the end of the file, creating a " \
		"hole. " \
		"Write a single character in the hole every 10 million " \
		"bytes. " \
		"If the hole option is not specified, then the file is " \
		"filled with random data. " \
		"If the close option (-c or --close) is specified the file " \
		"is closed. " \
		"If a sleep value is specified, the process sleeps. " \
		"If the delete option (-e or --delete) is specified, then " \
		"the file is deleted. " \
		"If a repeat count is specified, then the task repeats " \
		"that number of times. " \
		"The repeat count is given by the -n or --repeat option, " \
		"otherwise it defaults to 1. " \
		"A repeat count of zero repeats forever. " \
		"The file size is given by the -z or --size option, " \
		"otherwise it defaults to 1000000. " \
		"The sleep value is given by the -p or --sleep option, " \
		"otherwise it defaults to 0. " \
		"Sleep is specified in milliseconds.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 1000000;

	/* Set default test repetition */
	tests_repeat_parameter = 1;

	/* Set default test sleep */
	tests_sleep_parameter = 0;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, filestress00_get_title(),
			filestress00_get_description(), "znpuoce");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	filestress00();
	return 0;
}
