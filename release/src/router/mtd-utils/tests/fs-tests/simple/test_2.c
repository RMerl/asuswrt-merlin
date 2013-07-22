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

void test_2(void)
{
	pid_t pid;
	int create, full;
	unsigned i, number_of_files;
	unsigned growth;
	unsigned size;
	uint64_t big_file_size;
	int fd;
	off_t offset;
	char dir_name[256];

	/* Create a directory to test in */
	pid = getpid();
	tests_cat_pid(dir_name, "test_2_test_dir_", pid);
	if (chdir(dir_name) == -1)
		CHECK(mkdir(dir_name, 0777) != -1);
	CHECK(chdir(dir_name) != -1);
	/* Create up to 1000 files appending 400 bytes at a time to each file */
	/* until the file system is full.*/
	create = 1;
	full = 0;
	number_of_files = 1000;
	while (!full) {
		for (i = 0; i < number_of_files; ++i) {
			growth = tests_append_to_fragment_file(i, 400, create);
			if (!growth) {
				full = 1;
				if (create)
					number_of_files = i;
				break;
			}
		}
		create = 0;
	}
	/* Check the files */
	CHECK(tests_count_files_in_dir(".") == number_of_files);
	for (i = 0; i < number_of_files; ++i)
		tests_check_fragment_file(i);
	/* Delete half of them */
	for (i = 1; i < number_of_files; i += 2)
		tests_delete_fragment_file(i);
	/* Check them again */
	CHECK(tests_count_files_in_dir(".") == (number_of_files + 1) / 2);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	CHECK(tests_count_files_in_dir(".") == (number_of_files + 1) / 2);
	/* Create a big file that fills two thirds of the free space */
	big_file_size = tests_get_big_file_size(2,3);
	/* Check the big file */
	tests_create_file("big_file", big_file_size);
	CHECK(tests_count_files_in_dir(".") == 1 + (number_of_files + 1) / 2);
	tests_check_filled_file("big_file");
	/* Open the big file */
	fd = open("big_file",O_RDWR | tests_maybe_sync_flag());
	CHECK(fd != -1);
	/* Delete the big file while it is still open */
	tests_delete_file("big_file");
	/* Check the big file again */
	CHECK(tests_count_files_in_dir(".") == (number_of_files + 1) / 2);
	tests_check_filled_file_fd(fd);

	/* Write parts of the files and check them */

	offset = 100; /* Offset to write at, in the small files */
	size = 200; /* Number of bytes to write at the offset */

	for (i = 0; i < number_of_files; i += 2)
		tests_overwite_fragment_file(i, offset, size);
	/* Rewrite the big file entirely */
	tests_write_filled_file(fd, 0, big_file_size);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	tests_check_filled_file_fd(fd);

	offset = 300; /* Offset to write at, in the small files */
	size = 400; /* Number of bytes to write at the offset */

	for (i = 0; i < number_of_files; i += 2)
		tests_overwite_fragment_file(i, offset, size);
	/* Rewrite the big file entirely */
	tests_write_filled_file(fd, 0, big_file_size);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	tests_check_filled_file_fd(fd);

	offset = 110; /* Offset to write at, in the small files */
	size = 10; /* Number of bytes to write at the offset */

	for (i = 0; i < number_of_files; i += 2)
		tests_overwite_fragment_file(i, offset, size);
	/* Rewrite the big file entirely */
	tests_write_filled_file(fd, 0, big_file_size);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	tests_check_filled_file_fd(fd);

	offset = 10; /* Offset to write at, in the small files */
	size = 1000; /* Number of bytes to write at the offset */

	for (i = 0; i < number_of_files; i += 2)
		tests_overwite_fragment_file(i, offset, size);
	/* Rewrite the big file entirely */
	tests_write_filled_file(fd, 0, big_file_size);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	tests_check_filled_file_fd(fd);

	offset = 0; /* Offset to write at, in the small files */
	size = 100000; /* Number of bytes to write at the offset */

	for (i = 0; i < number_of_files; i += 2)
		tests_overwite_fragment_file(i, offset, size);
	/* Rewrite the big file entirely */
	tests_write_filled_file(fd, 0, big_file_size);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	tests_check_filled_file_fd(fd);

	/* Close the big file*/
	CHECK(close(fd) != -1);
	/* Check the small files */
	CHECK(tests_count_files_in_dir(".") == (number_of_files + 1) / 2);
	for (i = 0; i < number_of_files; i += 2)
		tests_check_fragment_file(i);
	/* Delete the small files */
	for (i = 0; i < number_of_files; i += 2)
		tests_delete_fragment_file(i);
	CHECK(tests_count_files_in_dir(".") == 0);
	CHECK(chdir("..") != -1);
	CHECK(rmdir(dir_name) != -1);
}

/* Title of this test */

const char *test_2_get_title(void)
{
	return "Repeated write many small files and one big deleted file";
}

/* Description of this test */

const char *test_2_get_description(void)
{
	return
		"Create a directory named test_2_test_dir_pid, where " \
		"pid is the process id.  Within that directory, " \
		"create about 1000 files.  Append 400 bytes to each until " \
		"the file system is full.  Then delete half of them.  Then " \
		"create a big file that uses about 2/3 of the remaining free " \
		"space.  Get a file descriptor for the big file, and delete " \
		"the big file.  Then repeatedly write to the small files " \
		"and the big file. " \
		"Finally delete the big file and directory.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, test_2_get_title(),
			test_2_get_description(), "s");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	test_2();
	return 0;
}
