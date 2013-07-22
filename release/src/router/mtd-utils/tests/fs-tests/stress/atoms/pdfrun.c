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

void adjust_size(void)
{
	char dummy[1024];
	unsigned long total_memory;
	FILE *f;

	total_memory = 0;
	f = fopen("/proc/meminfo", "r");
	if (fscanf(f, "%s %lu", dummy, &total_memory) != 2)
		perror("fscanf error");
	fclose(f);


	if (total_memory > 0 && tests_size_parameter > total_memory / 2)
		tests_size_parameter = total_memory / 2;
}

void run_pdf(void)
{
	int fd, i;
	pid_t pid;
	int64_t repeat;
	ssize_t written;
	int64_t remains;
	size_t block;
	char file_name[256];
	char buf[WRITE_BUFFER_SIZE];

	if (tests_fs_is_currfs())
		return;
	adjust_size();
	pid = getpid();
	tests_cat_pid(file_name, "run_pdf_test_file_", pid);
	fd = open(file_name, O_CREAT | O_WRONLY,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	CHECK(fd != -1);
	pid = getpid();
	srand(pid);
	repeat = tests_repeat_parameter;
	for (;;) {
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
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
		CHECK(lseek(fd, 0, SEEK_SET) == 0);
	}
	CHECK(close(fd) != -1);
	CHECK(unlink(file_name) != -1);
}

/* Title of this test */

const char *run_pdf_get_title(void)
{
	return "Create / overwrite a large file in the current directory";
}

/* Description of this test */

const char *run_pdf_get_description(void)
{
	return
		"Create a file named run_pdf_test_file_pid, " \
		"where pid is the process id.  The file is created " \
		"in the current directory, " \
		"if the current directory is NOT on the test " \
		"file system, otherwise no action is taken. " \
		"If a repeat count is specified, then the task repeats " \
		"that number of times. " \
		"The repeat count is given by the -n or --repeat option, " \
		"otherwise it defaults to 1. " \
		"A repeat count of zero repeats forever. " \
		"The size is given by the -z or --size option, " \
		"otherwise it defaults to 1000000. " \
		"The size is adjusted so that it is not more than " \
		"half the size of total memory.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 1000000;

	/* Set default test repetition */
	tests_repeat_parameter = 1;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, run_pdf_get_title(),
			run_pdf_get_description(), "zn");
	if (!run_test)
		return 1;
	/* Do the actual test */
	run_pdf();
	return 0;
}
