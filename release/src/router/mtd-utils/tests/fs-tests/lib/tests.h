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

#ifndef included_tests_tests_h__
#define included_tests_tests_h__

#include <stdint.h>

/* Main macro for testing */
#define CHECK(x) tests_test((x),__func__,__FILE__,__LINE__)

/* The default directory in which tests are conducted */
#define TESTS_DEFAULT_FILE_SYSTEM_MOUNT_DIR "/mnt/test_file_system"

/* The default file system type to test */
#define TESTS_DEFAULT_FILE_SYSTEM_TYPE "jffs2"

/* Estimated size of an empty directory */
#define TESTS_EMPTY_DIR_SIZE 128

/* Function invoked by the CHECK macro */
void tests_test(int test,const char *msg,const char *file,unsigned line);

/* Handle common program options */
int tests_get_args(int argc,
		char *argv[],
		const char *title,
		const char *desc,
		const char *opts);

/* Return the number of files (or directories) in the given directory */
unsigned tests_count_files_in_dir(const char *dir_name);

/* Change to the file system mount directory, check that it is empty,
   matches the file system type, and is not the root file system */
void tests_check_test_file_system(void);

/* Get the free space for the file system of the current directory */
uint64_t tests_get_free_space(void);

/* Get the total space for the file system of the current directory */
uint64_t tests_get_total_space(void);

/* Write size random bytes into file descriptor fd at the current position,
   returning the number of bytes actually written */
uint64_t tests_fill_file(int fd, uint64_t size);

/* Write size random bytes into file descriptor fd at offset,
   returning the number of bytes actually written */
uint64_t tests_write_filled_file(int fd, off_t offset, uint64_t size);

/* Check that a file written using tests_fill_file() and/or
   tests_write_filled_file() and/or tests_create_file()
   contains the expected random data */
void tests_check_filled_file_fd(int fd);

/* Check that a file written using tests_fill_file() and/or
   tests_write_filled_file() and/or tests_create_file()
   contains the expected random data */
void tests_check_filled_file(const char *file_name);

/* Delete a file */
void tests_delete_file(const char *file_name);

/* Create a file of size file_size */
uint64_t tests_create_file(const char *file_name, uint64_t file_size);

/* Calculate: free_space * numerator / denominator */
uint64_t tests_get_big_file_size(unsigned numerator, unsigned denominator);

/* Create file "fragment_n" where n is the file_number, and unlink it */
int tests_create_orphan(unsigned file_number);

/* Write size bytes at offset to the file "fragment_n" where n is the
   file_number and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_write_fragment_file(unsigned file_number,
				int fd,
				off_t offset,
				unsigned size);

/* Write size bytes to the end of file descriptor fd using file_number
   to determine the random data written i.e. seed for random numbers */
unsigned tests_fill_fragment_file(unsigned file_number,
				int fd,
				unsigned size);

/* Write size bytes to the end of file "fragment_n" where n is the file_number
   and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_append_to_fragment_file(unsigned file_number,
					unsigned size,
					int create);

/* Write size bytes at offset to the file "fragment_n" where n is the
   file_number and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_overwite_fragment_file(	unsigned file_number,
					off_t offset,
					unsigned size);

/* Delete file "fragment_n" where n is the file_number */
void tests_delete_fragment_file(unsigned file_number);

/* Check the random data in file "fragment_n" is what is expected */
void tests_check_fragment_file_fd(unsigned file_number, int fd);

/* Check the random data in file "fragment_n" is what is expected */
void tests_check_fragment_file(unsigned file_number);

/* Central point to decide whether to use fsync */
void tests_maybe_sync(int fd);

/* Return O_SYNC if ok to sync otherwise return 0 */
int tests_maybe_sync_flag(void);

/* Return random number from 0 to n - 1 */
size_t tests_random_no(size_t n);

/* Make a directory empty */
void tests_clear_dir(const char *dir_name);

/* Create an empty sub-directory or small file in the current directory */
int64_t tests_create_entry(char *return_name);

/* Remove a random file of empty sub-directory from the current directory */
int64_t tests_remove_entry(void);

/* Un-mount and re-mount test file system */
void tests_remount(void);

/* Un-mount test file system */
void tests_unmount(void);

/* Mount test file system */
void tests_mount(void);

/* Check whether the test file system is also the root file system */
int tests_fs_is_rootfs(void);

/* Try to make a directory empty */
void tests_try_to_clear_dir(const char *dir_name);

/* Check whether the test file system is also the current file system */
int tests_fs_is_currfs(void);

/* Concatenate a pid to a string in a signal safe way */
void tests_cat_pid(char *buf, const char *name, pid_t pid);

extern char *tests_file_system_mount_dir;

extern char *tests_file_system_type;

/* General purpose test parameter to specify some aspect of test size.
   May be used by different tests in different ways.
   Set by the -z, --size options. */
extern int64_t tests_size_parameter;

/* General purpose test parameter to specify some aspect of test repetition.
   May be used by different tests in different ways.
   Set by the -n, --repeat options. */
extern int64_t tests_repeat_parameter;

/* General purpose test parameter to specify some aspect of test sleeping.
   May be used by different tests in different ways.
   Set by the -p, --sleep options. */
extern int64_t tests_sleep_parameter;

/* General purpose test parameter to specify a file should be unlinked.
   May be used by different tests in different ways or not at all. */
extern int tests_unlink_flag;

/* General purpose test parameter to specify a file should be closed.
   May be used by different tests in different ways or not at all. */
extern int tests_close_flag;

/* General purpose test parameter to specify a file should be deleted.
   May be used by different tests in different ways or not at all. */
extern int tests_delete_flag;

/* General purpose test parameter to specify a file have a hole.
   May be used by different tests in different ways or not at all. */
extern int tests_hole_flag;

/* Program name from argv[0] */
extern char *program_name;

/* Maximum file name length of test file system (from statfs) */
extern long tests_max_fname_len;

#endif
