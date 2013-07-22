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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tests.h"

void test_1(void)
{
	int fd;
	pid_t pid;
	uint64_t i;
	uint64_t block;
	uint64_t actual_size;
	char name[256];
	char old[16];
	char buf[16];
	off_t old_len;
	char dir_name[256];

	/* Create a directory to test in */
	pid = getpid();
	tests_cat_pid(dir_name, "test_1_test_dir_", pid);
	if (chdir(dir_name) == -1)
		CHECK(mkdir(dir_name, 0777) != -1);
	CHECK(chdir(dir_name) != -1);
	/* Create a file that fills half the free space on the file system */
	tests_create_file("big_file", tests_get_big_file_size(1,2));
	CHECK(tests_count_files_in_dir(".") == 1);
	fd = open("big_file", O_RDWR | tests_maybe_sync_flag());
	CHECK(fd != -1);
	CHECK(read(fd, old, 5) == 5);
	CHECK(lseek(fd, 0, SEEK_SET) != (off_t) -1);
	CHECK(write(fd,"start", 5) == 5);
	CHECK(lseek(fd,0,SEEK_END) != (off_t) -1);
	CHECK(write(fd, "end", 3) == 3);
	tests_maybe_sync(fd);
	/* Delete the file while it is still open */
	tests_delete_file("big_file");
	CHECK(tests_count_files_in_dir(".") == 0);
	/* Create files to file up the file system */
	for (block = 1000000, i = 1; ; block /= 10) {
		while (i != 0) {
			sprintf(name, "fill_up_%llu", (unsigned long long)i);
			actual_size = tests_create_file(name, block);
			if (actual_size != 0)
				++i;
			if (actual_size != block)
				break;
		}
		if (block == 1)
			break;
	}
	/* Check the big file */
	CHECK(lseek(fd, 0, SEEK_SET) != (off_t) -1);
	CHECK(read(fd, buf, 5) == 5);
	CHECK(strncmp(buf, "start", 5) == 0);
	CHECK(lseek(fd, -3, SEEK_END) != (off_t) -1);
	CHECK(read(fd, buf, 3) == 3);
	CHECK(strncmp(buf, "end", 3) == 0);
	/* Check the other files and delete them */
	i -= 1;
	CHECK(tests_count_files_in_dir(".") == i);
	for (; i > 0; --i) {
		sprintf(name, "fill_up_%llu", (unsigned long long)i);
		tests_check_filled_file(name);
		tests_delete_file(name);
	}
	CHECK(tests_count_files_in_dir(".") == 0);
	/* Check the big file again */
	CHECK(lseek(fd, 0, SEEK_SET) != (off_t) -1);
	CHECK(read(fd, buf, 5) == 5);
	CHECK(strncmp(buf, "start", 5) == 0);
	CHECK(lseek(fd, -3, SEEK_END) != (off_t) -1);
	CHECK(read(fd, buf, 3) == 3);
	CHECK(strncmp(buf, "end", 3) == 0);
	CHECK(lseek(fd, 0, SEEK_SET) != (off_t) -1);
	CHECK(write(fd,old, 5) == 5);
	old_len = lseek(fd, -3, SEEK_END);
	CHECK(old_len != (off_t) -1);
	CHECK(ftruncate(fd,old_len) != -1);
	tests_check_filled_file_fd(fd);
	/* Close the big file*/
	CHECK(close(fd) != -1);
	CHECK(tests_count_files_in_dir(".") == 0);
	CHECK(chdir("..") != -1);
	CHECK(rmdir(dir_name) != -1);
}

/* Title of this test */

const char *test_1_get_title(void)
{
	return "Fill file system while holding deleted big file descriptor";
}

/* Description of this test */

const char *test_1_get_description(void)
{
	return
		"Create a directory named test_1_test_dir_pid, where " \
		"pid is the process id.  Within that directory, " \
		"create a big file (approx. half the file system in size), " \
		"open it, and unlink it. " \
		"Create many smaller files until the file system is full. " \
		"Check the big file is ok. " \
		"Delete all the smaller files. " \
		"Check the big file again. " \
		"Finally delete the big file and directory.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, test_1_get_title(),
			test_1_get_description(), "s");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	test_1();
	return 0;
}
