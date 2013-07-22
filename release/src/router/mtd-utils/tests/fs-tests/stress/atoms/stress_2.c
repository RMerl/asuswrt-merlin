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

void stress_2(void)
{
	int fd, i;
	pid_t pid;
	ssize_t written;
	int64_t remains;
	int64_t repeat;
	size_t block;
	char *file_name;
	char buf[WRITE_BUFFER_SIZE];

	file_name = "stress_2_test_file";
	fd = open(file_name, O_CREAT | O_WRONLY,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	CHECK(fd != -1);
	CHECK(unlink(file_name) != -1);
	pid = getpid();
	srand(pid);
	repeat = tests_repeat_parameter;
	for (;;) {
		for (i = 0; i < WRITE_BUFFER_SIZE;++i)
			buf[i] = rand();
		CHECK(lseek(fd, 0, SEEK_SET) != -1);
		remains = tests_size_parameter;
		while (remains > 0) {
			if (remains > WRITE_BUFFER_SIZE)
				block = WRITE_BUFFER_SIZE;
			else
				block = remains;
			written = write(fd, buf, block);
			if (written <= 0) {
				CHECK(errno == ENOSPC); /* File system full */
				errno = 0;
				break;
			}
			remains -= written;
		}
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
	}
	CHECK(close(fd) != -1);
}

/* Title of this test */

const char *stress_2_get_title(void)
{
	return "Create / overwrite a large deleted file";
}

/* Description of this test */

const char *stress_2_get_description(void)
{
	return
		"Create a file named stress_2_test_file. " \
		"Open it, delete it while holding the open file descriptor, " \
		"then fill it with random data. " \
		"Repeated re-write the file some number of times. " \
		"The repeat count is given by the -n or --repeat option, " \
		"otherwise it defaults to 10. " \
		"The file size is given by the -z or --size option, " \
		"otherwise it defaults to 1000000.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 1000000;

	/* Set default test repetition */
	tests_repeat_parameter = 10;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, stress_2_get_title(),
			stress_2_get_description(), "zn");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	stress_2();
	return 0;
}
