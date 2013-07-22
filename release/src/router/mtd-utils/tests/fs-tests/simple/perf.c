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
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#include "tests.h"

#define BLOCK_SIZE 32 * 1024

struct timeval tv_start;
struct timeval tv_stop;

static inline void start_timer(void)
{
	CHECK(gettimeofday(&tv_start, NULL) != -1);
}

static inline long long stop_timer(void)
{
	long long usecs;

	CHECK(gettimeofday(&tv_stop, NULL) != -1);
	usecs = (tv_stop.tv_sec - tv_start.tv_sec);
	usecs *= 1000000;
	usecs += tv_stop.tv_usec;
	usecs -= tv_start.tv_usec;
	return usecs;
}

static unsigned speed(size_t bytes, long long usecs)
{
	unsigned long long k;

	k = bytes * 1000000ULL;
	k /= usecs;
	k /= 1024;
	CHECK(k <= UINT_MAX);
	return (unsigned) k;
}

void perf(void)
{
	pid_t pid;
	int fd, i;
	ssize_t written, readsz;
	size_t remains, block, actual_size;
	char file_name[256];
	unsigned char *buf;
	long long write_time, unmount_time, mount_time, read_time;

	/* Sync all file systems */
	sync();
	/* Make random data to write */
	buf = malloc(BLOCK_SIZE);
	CHECK(buf != NULL);
	pid = getpid();
	srand(pid);
	for (i = 0; i < BLOCK_SIZE; i++)
		buf[i] = rand();
	/* Open file */
	tests_cat_pid(file_name, "perf_test_file_", pid);
	start_timer();
	fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	CHECK(fd != -1);
	CHECK(tests_size_parameter > 0);
	CHECK(tests_size_parameter <= SIZE_MAX);
	/* Write to file */
	actual_size = 0;
	remains = tests_size_parameter;
	while (remains > 0) {
		if (remains > BLOCK_SIZE)
			block = BLOCK_SIZE;
		else
			block = remains;
		written = write(fd, buf, block);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
			break;
		}
		remains -= written;
		actual_size += written;
	}
	CHECK(fsync(fd) != -1);
	CHECK(close(fd) != -1);
	write_time = stop_timer();
	/* Unmount */
	start_timer();
	tests_unmount();
	unmount_time = stop_timer();
	/* Mount */
	start_timer();
	tests_mount();
	mount_time = stop_timer();
	/* Open file, read it, and close it */
	start_timer();
	fd = open(file_name, O_RDONLY);
	CHECK(fd != -1);
	remains = actual_size;
	while (remains > 0) {
		if (remains > BLOCK_SIZE)
			block = BLOCK_SIZE;
		else
			block = remains;
		readsz = read(fd, buf, block);
		CHECK(readsz == block);
		remains -= readsz;
	}
	CHECK(close(fd) != -1);
	read_time = stop_timer();
	CHECK(unlink(file_name) != -1);
	/* Display timings */
	printf("File system read and write speed\n");
	printf("================================\n");
	printf("Specfied file size: %lld\n",
	       (unsigned long long)tests_size_parameter);
	printf("Actual file size: %zu\n", actual_size);
	printf("Write time (us): %lld\n", write_time);
	printf("Unmount time (us): %lld\n", unmount_time);
	printf("Mount time (us): %lld\n", mount_time);
	printf("Read time (us): %lld\n", read_time);
	printf("Write speed (KiB/s): %u\n", speed(actual_size, write_time));
	printf("Read speed (KiB/s): %u\n", speed(actual_size, read_time));
	printf("Test completed\n");
}

/* Title of this test */

const char *perf_get_title(void)
{
	return "Measure file system read and write speed";
}

/* Description of this test */

const char *perf_get_description(void)
{
	return
		"Syncs the file system (a newly created empty file system is " \
		"preferable). Creates a file named perf_test_file_pid, where " \
		"pid is the process id. The file is filled with random data. " \
		"The size of the file is given by the -z or --size option, " \
		"otherwise it defaults to 10MiB. Unmounts the file system. " \
		"Mounts the file system. Reads the entire file in 32KiB size " \
		"blocks. Displays the time taken for each activity. Deletes " \
		"the file. Note that the file is synced after writing and " \
		"that time is included in the write time and speed.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test file size */
	tests_size_parameter = 10 * 1024 * 1024;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, perf_get_title(),
			perf_get_description(), "z");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	perf();
	return 0;
}
