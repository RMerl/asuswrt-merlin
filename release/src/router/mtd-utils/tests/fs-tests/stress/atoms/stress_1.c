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

void stress_1(void)
{
	int fd, i;
	pid_t pid;
	ssize_t written;
	int64_t remains;
	size_t block;
	char file_name[256];
	char buf[WRITE_BUFFER_SIZE];

	pid = getpid();
	tests_cat_pid(file_name, "stress_1_test_file_", pid);
	fd = open(file_name, O_CREAT | O_WRONLY,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	CHECK(fd != -1);
	srand(pid);
	for (i = 0; i < WRITE_BUFFER_SIZE;++i)
		buf[i] = rand();
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
	CHECK(close(fd) != -1);
	if (tests_delete_flag)
		CHECK(unlink(file_name) != -1);
}

/* Title of this test */

const char *stress_1_get_title(void)
{
	return "Create / overwrite a large file";
}

/* Description of this test */

const char *stress_1_get_description(void)
{
	return
		"Create a file named stress_1_test_file_pid, " \
		"where pid is the process id. " \
		"The size is given by the -z or --size option, " \
		"otherwise it defaults to 1000000. " \
		"The file will be deleted if the delete option " \
		"is specified. ";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 1000000;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, stress_1_get_title(),
			stress_1_get_description(), "ze");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	stress_1();
	return 0;
}
