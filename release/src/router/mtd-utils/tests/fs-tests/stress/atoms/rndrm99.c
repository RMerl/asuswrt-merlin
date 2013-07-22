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
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

#include "tests.h"

uint32_t files_created = 0;
uint32_t files_removed = 0;
uint32_t dirs_created = 0;
uint32_t dirs_removed = 0;
int64_t *size_ptr = 0;

void display_stats(void)
{
	printf(	"\nrndrm99 stats:\n"
		"\tNumber of files created = %u\n"
		"\tNumber of files deleted = %u\n"
		"\tNumber of directories created = %u\n"
		"\tNumber of directories deleted = %u\n"
		"\tCurrent net size of creates and deletes = %lld\n",
		(unsigned) files_created,
		(unsigned) files_removed,
		(unsigned) dirs_created,
		(unsigned) dirs_removed,
		(long long) (size_ptr ? *size_ptr : 0));
	fflush(stdout);
}

struct timeval tv_before;
struct timeval tv_after;

void before(void)
{
	CHECK(gettimeofday(&tv_before, NULL) != -1);
}

void after(const char *msg)
{
	time_t diff;
	CHECK(gettimeofday(&tv_after, NULL) != -1);
	diff = tv_after.tv_sec - tv_before.tv_sec;
	if (diff >= 8) {
		printf("\nrndrm99: the following fn took more than 8 seconds: %s (took %u secs)\n",msg,(unsigned) diff);
		fflush(stdout);
		display_stats();
	}
}

#define WRITE_BUFFER_SIZE 32768

static char write_buffer[WRITE_BUFFER_SIZE];

static void init_write_buffer()
{
	static int init = 0;

	if (!init) {
		int i, d;
		uint64_t u;

		u = RAND_MAX;
		u += 1;
		u /= 256;
		d = (int) u;
		srand(1);
		for (i = 0; i < WRITE_BUFFER_SIZE; ++i)
			write_buffer[i] = rand() / d;
		init = 1;
	}
}

/* Write size random bytes into file descriptor fd at the current position,
   returning the number of bytes actually written */
uint64_t fill_file(int fd, uint64_t size)
{
	ssize_t written;
	size_t sz;
	unsigned start = 0, length;
	uint64_t remains;
	uint64_t actual_size = 0;

	init_write_buffer();
	remains = size;
	while (remains > 0) {
		length = WRITE_BUFFER_SIZE - start;
		if (remains > length)
			sz = length;
		else
			sz = (size_t) remains;
		before();
		written = write(fd, write_buffer + start, sz);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
			after("write");
			fprintf(stderr,"\nrndrm99: write failed with ENOSPC\n");fflush(stderr);
			display_stats();
			break;
		}
		after("write");
		remains -= written;
		actual_size += written;
		if ((size_t) written == sz)
			start = 0;
		else
			start += written;
	}
	return actual_size;
}

/* Create a file of size file_size */
uint64_t create_file(const char *file_name, uint64_t file_size)
{
	int fd;
	int flags;
	mode_t mode;
	uint64_t actual_size; /* Less than size if the file system is full */

	flags = O_CREAT | O_TRUNC | O_WRONLY;
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	before();
	fd = open(file_name, flags, mode);
	if (fd == -1 && errno == ENOSPC) {
		errno = 0;
		after("open");
		fprintf(stderr,"\nrndrm99: open failed with ENOSPC\n");fflush(stderr);
		display_stats();
		return 0; /* File system full */
	}
	CHECK(fd != -1);
	after("open");
	actual_size = fill_file(fd, file_size);
	before();
	CHECK(close(fd) != -1);
	after("close");
	if (file_size != 0 && actual_size == 0) {
		printf("\nrndrm99: unlinking zero size file\n");fflush(stdout);
		before();
		CHECK(unlink(file_name) != -1);
		after("unlink (create_file)");
	}
	return actual_size;
}

/* Create an empty sub-directory or small file in the current directory */
int64_t create_entry(char *return_name)
{
	int fd;
	char name[256];
	int64_t res;

	for (;;) {
		sprintf(name, "%u", (unsigned) tests_random_no(10000000));
		before();
		fd = open(name, O_RDONLY);
		after("open (create_entry)");
		if (fd == -1)
			break;
		before();
		close(fd);
		after("close (create_entry)");
	}
	if (return_name)
		strcpy(return_name, name);
	if (tests_random_no(2)) {
		res = create_file(name, tests_random_no(4096));
		if (res > 0)
			files_created += 1;
		return res;
	} else {
		before();
		if (mkdir(name, 0777) == -1) {
			CHECK(errno == ENOSPC);
			after("mkdir");
			errno = 0;
			fprintf(stderr,"\nrndrm99: mkdir failed with ENOSPC\n");fflush(stderr);
			display_stats();
			return 0;
		}
		after("mkdir");
		dirs_created += 1;
		return TESTS_EMPTY_DIR_SIZE;
	}
}

/* Remove a random file of empty sub-directory from the current directory */
int64_t remove_entry(void)
{
	DIR *dir;
	struct dirent *entry;
	unsigned count = 0, pos;
	int64_t result = 0;

	before();
	dir = opendir(".");
	CHECK(dir != NULL);
	after("opendir");
	for (;;) {
		errno = 0;
		before();
		entry = readdir(dir);
		if (entry) {
			after("readdir 1");
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0)
				++count;
		} else {
			CHECK(errno == 0);
			after("readdir 1");
			break;
		}
	}
	pos = tests_random_no(count);
	count = 0;
	before();
	rewinddir(dir);
	after("rewinddir");
	for (;;) {
		errno = 0;
		before();
		entry = readdir(dir);
		if (!entry) {
			CHECK(errno == 0);
			after("readdir 2");
			break;
		}
		after("readdir 2");
		if (strcmp(".",entry->d_name) != 0 &&
				strcmp("..",entry->d_name) != 0) {
			if (count == pos) {
				if (entry->d_type == DT_DIR) {
					before();
					tests_clear_dir(entry->d_name);
					after("tests_clear_dir");
					before();
					CHECK(rmdir(entry->d_name) != -1);
					after("rmdir");
					result = TESTS_EMPTY_DIR_SIZE;
					dirs_removed += 1;
				} else {
					struct stat st;
					before();
					CHECK(stat(entry->d_name, &st) != -1);
					after("stat");
					result = st.st_size;
					before();
					CHECK(unlink(entry->d_name) != -1);
					after("unlink");
					files_removed += 1;
				}
			}
			++count;
		}
	}
	before();
	CHECK(closedir(dir) != -1);
	after("closedir");
	return result;
}

void rndrm99(void)
{
	int64_t repeat, loop_cnt;
	int64_t size, this_size;
	pid_t pid;
	char dir_name[256];

	size_ptr = &size;
	/* Create a directory to test in */
	pid = getpid();
	tests_cat_pid(dir_name, "rndrm99_test_dir_", pid);
	if (chdir(dir_name) == -1)
		CHECK(mkdir(dir_name, 0777) != -1);
	CHECK(chdir(dir_name) != -1);
	/* Repeat loop */
	repeat = tests_repeat_parameter;
	size = 0;
	for (;;) {
		/* Create and remove sub-dirs and small files, */
		/* but tending to grow */
		printf("\nrndrm99: growing\n");fflush(stdout);
		loop_cnt = 0;
		do {
			if (loop_cnt++ % 2000 == 0)
				display_stats();
			if (tests_random_no(3)) {
				this_size = create_entry(NULL);
				if (!this_size)
					break;
				size += this_size;
			} else {
				this_size = remove_entry();
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
		printf("\nrndrm99: shrinking\n");fflush(stdout);
		loop_cnt = 0;
		do {
			if (loop_cnt++ % 2000 == 0)
				display_stats();
			if (!tests_random_no(3)) {
				this_size = create_entry(NULL);
				size += this_size;
			} else {
				this_size = remove_entry();
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
			printf("\nrndrm99: sleeping\n");fflush(stdout);
			usleep(s);
		}
	}
	printf("\nrndrm99: tidying\n");fflush(stdout);
	display_stats();
	/* Tidy up by removing everything */
	tests_clear_dir(".");
	CHECK(chdir("..") != -1);
	CHECK(rmdir(dir_name) != -1);
	size_ptr = 0;
}

/* Title of this test */

const char *rndrm99_get_title(void)
{
	return "Randomly create and remove directories and files";
}

/* Description of this test */

const char *rndrm99_get_description(void)
{
	return
		"Create a directory named rndrm99_test_dir_pid, where " \
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
	run_test = tests_get_args(argc, argv, rndrm99_get_title(),
			rndrm99_get_description(), "znp");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	rndrm99();
	return 0;
}
