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
#include <limits.h>

#include "tests.h"

#define BLOCK_SIZE 32768
#define BUFFER_SIZE 32768

static void check_file(int fd, char *data, size_t length)
{
	ssize_t n, i;
	char buf[BUFFER_SIZE];

	CHECK(lseek(fd, 0, SEEK_SET) != -1);
	n = 0;
	for (;;) {
		i = read(fd, buf, BUFFER_SIZE);
		CHECK(i >= 0);
		if (i == 0)
			break;
		CHECK(memcmp(buf, data + n, i) == 0);
		n += i;
	}
	CHECK(n == length);
}

void rndwrite00(void)
{
	int fd;
	pid_t pid;
	ssize_t written;
	size_t remains;
	size_t block;
	size_t actual_size;
	size_t check_every;
	char *data, *p, *q;
	off_t offset;
	size_t size;
	int64_t repeat;
	char file_name[256];
	char buf[4096];

	/* Create file */
	pid = getpid();
	tests_cat_pid(file_name, "rndwrite00_test_file_", pid);
	fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	CHECK(fd != -1);
	/* Allocate memory to hold file data */
	CHECK(tests_size_parameter > 0);
	CHECK(tests_size_parameter <= SIZE_MAX);
	data = (char *) malloc(tests_size_parameter);
	CHECK(data != NULL);
	/* Fill with random data */
	srand(pid);
	for (p = data, q = data + tests_size_parameter; p != q; ++p)
		*p = rand();
	/* Write to file */
	p = data;
	remains = tests_size_parameter;
	while (remains > 0) {
		if (remains > BLOCK_SIZE)
			block = BLOCK_SIZE;
		else
			block = remains;
		written = write(fd, p, block);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
			break;
		}
		remains -= written;
		p += written;
	}
	actual_size = p - data;
	/* Repeating bit */
	repeat = tests_repeat_parameter;
	check_every = actual_size / 8192;
	for (;;) {
		offset = tests_random_no(actual_size);
		size = tests_random_no(4096);
		/* Don't change the file size */
		if (offset + size > actual_size)
			size = actual_size - offset;
		if (!size)
			continue;
		for (p = buf, q = p + size; p != q; ++p)
			*p = rand();
		CHECK(lseek(fd, offset, SEEK_SET) != -1);
		written = write(fd, buf, size);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
		} else
			memcpy(data + offset, buf, written);
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
		if (repeat % check_every == 0)
			check_file(fd, data, actual_size);
		/* Sleep */
		if (tests_sleep_parameter > 0) {
			unsigned us = tests_sleep_parameter * 1000;
			unsigned rand_divisor = RAND_MAX / us;
			unsigned s = (us / 2) + (rand() / rand_divisor);
			usleep(s);
		}
	}
	/* Check and close file */
	check_file(fd, data, actual_size);
	CHECK(close(fd) != -1);
	if (tests_delete_flag)
		CHECK(unlink(file_name) != -1);
}

/* Title of this test */

const char *rndwrite00_get_title(void)
{
	return "Randomly write a large test file";
}

/* Description of this test */

const char *rndwrite00_get_description(void)
{
	return
		"Create a file named rndwrite00_test_file_pid, where " \
		"pid is the process id. " \
		"The file is filled with random data. " \
		"The size of the file is given by the -z or --size option, " \
		"otherwise it defaults to 1000000. " \
		"Then a randomly sized block of random data is written at a " \
		"random location in the file. "\
		"The block size is always in the range 1 to 4095. " \
		"If a sleep value is specified, the process sleeps. " \
		"The number of writes is given by the repeat count. " \
		"The repeat count is set by the -n or --repeat option, " \
		"otherwise it defaults to 10000. " \
		"A repeat count of zero repeats forever. " \
		"The sleep value is given by the -p or --sleep option, " \
		"otherwise it defaults to 0. "
		"Sleep is specified in milliseconds. " \
		"Periodically the data in the file is checked with a copy " \
		"held in memory. " \
		"If the delete option is specified the file is finally " \
		"deleted.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 1000000;

	/* Set default test repetition */
	tests_repeat_parameter = 10000;

	/* Set default test sleep */
	tests_sleep_parameter = 0;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, rndwrite00_get_title(),
			rndwrite00_get_description(), "zne");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	rndwrite00();
	return 0;
}
