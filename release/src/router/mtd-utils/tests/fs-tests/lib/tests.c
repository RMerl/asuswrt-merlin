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
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <mntent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <linux/fs.h>
#include <linux/jffs2.h>

#include "tests.h"

char *tests_file_system_mount_dir = TESTS_DEFAULT_FILE_SYSTEM_MOUNT_DIR;

char *tests_file_system_type = TESTS_DEFAULT_FILE_SYSTEM_TYPE;

int tests_ok_to_sync = 0; /* Whether to use fsync */

/* General purpose test parameter to specify some aspect of test size.
   May be used by different tests in different ways or not at all.
   Set by the -z or --size option. */
int64_t tests_size_parameter = 0;

/* General purpose test parameter to specify some aspect of test repetition.
   May be used by different tests in different ways or not at all.
   Set by the -n, --repeat options. */
int64_t tests_repeat_parameter = 0;

/* General purpose test parameter to specify some aspect of test sleeping.
   May be used by different tests in different ways or not at all.
   Set by the -p, --sleep options. */
int64_t tests_sleep_parameter = 0;

/* Program name from argv[0] */
char *program_name = "unknown";

/* General purpose test parameter to specify a file should be unlinked.
   May be used by different tests in different ways or not at all. */
int tests_unlink_flag = 0;

/* General purpose test parameter to specify a file should be closed.
   May be used by different tests in different ways or not at all. */
int tests_close_flag = 0;

/* General purpose test parameter to specify a file should be deleted.
   May be used by different tests in different ways or not at all. */
int tests_delete_flag = 0;

/* General purpose test parameter to specify a file have a hole.
   May be used by different tests in different ways or not at all. */
int tests_hole_flag = 0;

/* Whether it is ok to test on the root file system */
static int rootok = 0;

/* Maximum file name length of test file system (from statfs) */
long tests_max_fname_len = 255;

/* Function invoked by the CHECK macro */
void tests_test(int test,const char *msg,const char *file,unsigned line)
{
	int eno;
	time_t t;

	if (test)
		return;
	eno = errno;
	time(&t);
	fprintf(stderr,	"Test failed: %s on %s"
			"Test failed: %s in %s at line %u\n",
			program_name, ctime(&t), msg, file, line);
	if (eno) {
		fprintf(stderr,"errno = %d\n",eno);
		fprintf(stderr,"strerror = %s\n",strerror(eno));
	}
	exit(1);
}

static int is_zero(const char *p)
{
	for (;*p;++p)
		if (*p != '0')
			return 0;
	return 1;
}

static void fold(const char *text, int width)
{
	int pos, bpos = 0;
	const char *p;
	char line[1024];

	if (width > 1023) {
		printf("%s\n", text);
		return;
	}
	p = text;
	pos = 0;
	while (p[pos]) {
		while (!isspace(p[pos])) {
			line[pos] = p[pos];
			if (!p[pos])
				break;
			++pos;
			if (pos == width) {
				line[pos] = '\0';
				printf("%s\n", line);
				p += pos;
				pos = 0;
			}
		}
		while (pos < width) {
			line[pos] = p[pos];
			if (!p[pos]) {
				bpos = pos;
				break;
			}
			if (isspace(p[pos]))
				bpos = pos;
			++pos;
		}
		line[bpos] = '\0';
		printf("%s\n", line);
		p += bpos;
		pos = 0;
		while (p[pos] && isspace(p[pos]))
			++p;
	}
}

/* Handle common program options */
int tests_get_args(int argc,
		char *argv[],
		const char *title,
		const char *desc,
		const char *opts)
{
	int run_test = 0;
	int display_help = 0;
	int display_title = 0;
	int display_description = 0;
	int i;
	char *s;

	program_name = argv[0];

	s = getenv("TEST_FILE_SYSTEM_MOUNT_DIR");
	if (s)
		tests_file_system_mount_dir = strdup(s);
	s = getenv("TEST_FILE_SYSTEM_TYPE");
	if (s)
		tests_file_system_type = strdup(s);

	run_test = 1;
	rootok = 1;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0 ||
				strcmp(argv[i], "-h") == 0)
			display_help = 1;
		else if (strcmp(argv[i], "--title") == 0 ||
				strcmp(argv[i], "-t") == 0)
			display_title = 1;
		else if (strcmp(argv[i], "--description") == 0 ||
				strcmp(argv[i], "-d") == 0)
			display_description = 1;
		else if (strcmp(argv[i], "--sync") == 0 ||
				strcmp(argv[i], "-s") == 0)
			tests_ok_to_sync = 1;
		else if (strncmp(argv[i], "--size", 6) == 0 ||
				strncmp(argv[i], "-z", 2) == 0) {
			int64_t n;
			char *p;
			if (i+1 < argc && !isdigit(argv[i][strlen(argv[i])-1]))
				++i;
			p = argv[i];
			while (*p && !isdigit(*p))
				++p;
			n = atoll(p);
			if (n)
				tests_size_parameter = n;
			else {
				int all_zero = 1;
				for (; all_zero && *p; ++p)
					if (*p != '0')
						all_zero = 0;
				if (all_zero)
					tests_size_parameter = 0;
				else
					display_help = 1;
			}
		} else if (strncmp(argv[i], "--repeat", 8) == 0 ||
				strncmp(argv[i], "-n", 2) == 0) {
			int64_t n;
			char *p;
			if (i+1 < argc && !isdigit(argv[i][strlen(argv[i])-1]))
				++i;
			p = argv[i];
			while (*p && !isdigit(*p))
				++p;
			n = atoll(p);
			if (n || is_zero(p))
				tests_repeat_parameter = n;
			else
				display_help = 1;
		} else if (strncmp(argv[i], "--sleep", 7) == 0 ||
				strncmp(argv[i], "-p", 2) == 0) {
			int64_t n;
			char *p;
			if (i+1 < argc && !isdigit(argv[i][strlen(argv[i])-1]))
				++i;
			p = argv[i];
			while (*p && !isdigit(*p))
				++p;
			n = atoll(p);
			if (n || is_zero(p))
				tests_sleep_parameter = n;
			else
				display_help = 1;
		} else if (strcmp(argv[i], "--unlink") == 0 ||
				strcmp(argv[i], "-u") == 0)
			tests_unlink_flag = 1;
		else if (strcmp(argv[i], "--hole") == 0 ||
				strcmp(argv[i], "-o") == 0)
			tests_hole_flag = 1;
		else if (strcmp(argv[i], "--close") == 0 ||
				strcmp(argv[i], "-c") == 0)
			tests_close_flag = 1;
		else if (strcmp(argv[i], "--delete") == 0 ||
				strcmp(argv[i], "-e") == 0)
			tests_delete_flag = 1;
		else
			display_help = 1;
	}

	if (display_help) {
		run_test = 0;
		display_title = 0;
		display_description = 0;
		if (!opts)
			opts = "";
		printf("File System Test Program\n\n");
		printf("Test Title: %s\n\n", title);
		printf("Usage is: %s [ options ]\n",argv[0]);
		printf("    Options are:\n");
		printf("        -h, --help            ");
		printf("Display this help\n");
		printf("        -t, --title           ");
		printf("Display the test title\n");
		printf("        -d, --description     ");
		printf("Display the test description\n");
		if (strchr(opts, 's')) {
			printf("        -s, --sync            ");
			printf("Make use of fsync\n");
		}
		if (strchr(opts, 'z')) {
			printf("        -z, --size            ");
			printf("Set size parameter\n");
		}
		if (strchr(opts, 'n')) {
			printf("        -n, --repeat          ");
			printf("Set repeat parameter\n");
		}
		if (strchr(opts, 'p')) {
			printf("        -p, --sleep           ");
			printf("Set sleep parameter\n");
		}
		if (strchr(opts, 'u')) {
			printf("        -u, --unlink          ");
			printf("Unlink file\n");
		}
		if (strchr(opts, 'o')) {
			printf("        -o, --hole            ");
			printf("Create a hole in a file\n");
		}
		if (strchr(opts, 'c')) {
			printf("        -c, --close           ");
			printf("Close file\n");
		}
		if (strchr(opts, 'e')) {
			printf("        -e, --delete          ");
			printf("Delete file\n");
		}
		printf("\nBy default, testing is done in directory ");
		printf("/mnt/test_file_system. To change this\nuse ");
		printf("environmental variable ");
		printf("TEST_FILE_SYSTEM_MOUNT_DIR. By default, ");
		printf("the file\nsystem tested is jffs2. To change this ");
		printf("set TEST_FILE_SYSTEM_TYPE.\n\n");
		printf("Test Description:\n");
		fold(desc, 80);
	} else {
		if (display_title)
			printf("%s\n", title);
		if (display_description)
			printf("%s\n", desc);
		if (display_title || display_description)
			if (argc == 2 || (argc == 3 &&
					display_title &&
					display_description))
				run_test = 0;
	}
	return run_test;
}

/* Return the number of files (or directories) in the given directory */
unsigned tests_count_files_in_dir(const char *dir_name)
{
	DIR *dir;
	struct dirent *entry;
	unsigned count = 0;

	dir = opendir(dir_name);
	CHECK(dir != NULL);
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		if (entry) {
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0)
				++count;
		} else {
			CHECK(errno == 0);
			break;
		}
	}
	CHECK(closedir(dir) != -1);
	return count;
}

/* Change to the file system mount directory, check that it is empty,
   matches the file system type, and is not the root file system */
void tests_check_test_file_system(void)
{
	struct statfs fs_info;
	struct stat f_info;
	struct stat root_f_info;

	if (chdir(tests_file_system_mount_dir) == -1 ||
			statfs(tests_file_system_mount_dir, &fs_info) == -1) {
		fprintf(stderr, "Invalid test file system mount directory:"
			" %s\n", tests_file_system_mount_dir);
		fprintf(stderr,	"Use environment variable "
			"TEST_FILE_SYSTEM_MOUNT_DIR\n");
		CHECK(0);
	}
	tests_max_fname_len = fs_info.f_namelen;
	if (strcmp(tests_file_system_type, "jffs2") == 0 &&
			fs_info.f_type != JFFS2_SUPER_MAGIC) {
		fprintf(stderr,	"File system type is not jffs2\n");
		CHECK(0);
	}
	/* Check that the test file system is not the root file system */
	if (!rootok) {
		CHECK(stat(tests_file_system_mount_dir, &f_info) != -1);
		CHECK(stat("/", &root_f_info) != -1);
		CHECK(f_info.st_dev != root_f_info.st_dev);
	}
}

/* Get the free space for the file system of the current directory */
uint64_t tests_get_free_space(void)
{
	struct statvfs fs_info;

	CHECK(statvfs(tests_file_system_mount_dir, &fs_info) != -1);
	return (uint64_t) fs_info.f_bavail * (uint64_t) fs_info.f_frsize;
}

/* Get the total space for the file system of the current directory */
uint64_t tests_get_total_space(void)
{
	struct statvfs fs_info;

	CHECK(statvfs(tests_file_system_mount_dir, &fs_info) != -1);
	return (uint64_t) fs_info.f_blocks * (uint64_t) fs_info.f_frsize;
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
uint64_t tests_fill_file(int fd, uint64_t size)
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
		written = write(fd, write_buffer + start, sz);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
			break;
		}
		remains -= written;
		actual_size += written;
		if (written == sz)
			start = 0;
		else
			start += written;
	}
	tests_maybe_sync(fd);
	return actual_size;
}

/* Write size random bytes into file descriptor fd at offset,
   returning the number of bytes actually written */
uint64_t tests_write_filled_file(int fd, off_t offset, uint64_t size)
{
	ssize_t written;
	size_t sz;
	unsigned start = 0, length;
	uint64_t remains;
	uint64_t actual_size = 0;

	CHECK(lseek(fd, offset, SEEK_SET) == offset);

	init_write_buffer();
	remains = size;
	start = offset % WRITE_BUFFER_SIZE;
	while (remains > 0) {
		length = WRITE_BUFFER_SIZE - start;
		if (remains > length)
			sz = length;
		else
			sz = (size_t) remains;
		written = write(fd, write_buffer + start, sz);
		if (written <= 0) {
			CHECK(errno == ENOSPC); /* File system full */
			errno = 0;
			break;
		}
		remains -= written;
		actual_size += written;
		if (written == sz)
			start = 0;
		else
			start += written;
	}
	tests_maybe_sync(fd);
	return actual_size;
}

/* Check that a file written using tests_fill_file() and/or
   tests_write_filled_file() and/or tests_create_file()
   contains the expected random data */
void tests_check_filled_file_fd(int fd)
{
	ssize_t sz;
	char buf[WRITE_BUFFER_SIZE];

	CHECK(lseek(fd, 0, SEEK_SET) == 0);
	do {
		sz = read(fd, buf, WRITE_BUFFER_SIZE);
		CHECK(sz >= 0);
		CHECK(memcmp(buf, write_buffer, sz) == 0);
	} while (sz);
}

/* Check that a file written using tests_fill_file() and/or
   tests_write_filled_file() and/or tests_create_file()
   contains the expected random data */
void tests_check_filled_file(const char *file_name)
{
	int fd;

	fd = open(file_name, O_RDONLY);
	CHECK(fd != -1);
	tests_check_filled_file_fd(fd);
	CHECK(close(fd) != -1);
}

void tests_sync_directory(const char *file_name)
{
	char *path;
	char *dir;
	int fd;

	if (!tests_ok_to_sync)
		return;

	path = strdup(file_name);
	dir = dirname(path);
	fd = open(dir,O_RDONLY | tests_maybe_sync_flag());
	CHECK(fd != -1);
	CHECK(fsync(fd) != -1);
	CHECK(close(fd) != -1);
	free(path);
}

/* Delete a file */
void tests_delete_file(const char *file_name)
{
	CHECK(unlink(file_name) != -1);
	tests_sync_directory(file_name);
}

/* Create a file of size file_size */
uint64_t tests_create_file(const char *file_name, uint64_t file_size)
{
	int fd;
	int flags;
	mode_t mode;
	uint64_t actual_size; /* Less than size if the file system is full */

	flags = O_CREAT | O_TRUNC | O_WRONLY | tests_maybe_sync_flag();
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	fd = open(file_name, flags, mode);
	if (fd == -1 && errno == ENOSPC) {
		errno = 0;
		return 0; /* File system full */
	}
	CHECK(fd != -1);
	actual_size = tests_fill_file(fd, file_size);
	CHECK(close(fd) != -1);
	if (file_size != 0 && actual_size == 0)
		tests_delete_file(file_name);
	else
		tests_sync_directory(file_name);
	return actual_size;
}

/* Calculate: free_space * numerator / denominator */
uint64_t tests_get_big_file_size(unsigned numerator, unsigned denominator)
{
	if (denominator == 0)
		denominator = 1;
	if (numerator > denominator)
		numerator = denominator;
	return numerator * (tests_get_free_space() / denominator);
}

/* Create file "fragment_n" where n is the file_number, and unlink it */
int tests_create_orphan(unsigned file_number)
{
	int fd;
	int flags;
	mode_t mode;
	char file_name[256];

	sprintf(file_name, "fragment_%u", file_number);
	flags = O_CREAT | O_TRUNC | O_RDWR | tests_maybe_sync_flag();
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	fd = open(file_name, flags, mode);
	if (fd == -1 && (errno == ENOSPC || errno == EMFILE))
		return fd; /* File system full or too many open files */
	CHECK(fd != -1);
	tests_sync_directory(file_name);
	CHECK(unlink(file_name) != -1);
	return fd;
}

/* Write size bytes at offset to the file "fragment_n" where n is the
   file_number and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_write_fragment_file(unsigned file_number,
				int fd,
				off_t offset,
				unsigned size)
{
	int i, d;
	uint64_t u;
	ssize_t written;
	off_t pos;
	char buf[WRITE_BUFFER_SIZE];

	if (size > WRITE_BUFFER_SIZE)
		size = WRITE_BUFFER_SIZE;

	pos = lseek(fd, 0, SEEK_END);
	CHECK(pos != (off_t) -1);
	if (offset > pos)
		offset = pos;

	pos = lseek(fd, offset, SEEK_SET);
	CHECK(pos != (off_t) -1);
	CHECK(pos == offset);

	srand(file_number);
	while (offset--)
		rand();

	u = RAND_MAX;
	u += 1;
	u /= 256;
	d = (int) u;
	for (i = 0; i < size; ++i)
		buf[i] = rand() / d;

	written = write(fd, buf, size);
	if (written <= 0) {
		CHECK(errno == ENOSPC); /* File system full */
		errno = 0;
		written = 0;
	}
	tests_maybe_sync(fd);
	return (unsigned) written;
}

/* Write size bytes to the end of file descriptor fd using file_number
   to determine the random data written i.e. seed for random numbers */
unsigned tests_fill_fragment_file(unsigned file_number, int fd, unsigned size)
{
	off_t offset;

	offset = lseek(fd, 0, SEEK_END);
	CHECK(offset != (off_t) -1);

	return tests_write_fragment_file(file_number, fd, offset, size);
}

/* Write size bytes to the end of file "fragment_n" where n is the file_number
   and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_append_to_fragment_file(unsigned file_number,
					unsigned size,
					int create)
{
	int fd;
	int flags;
	mode_t mode;
	unsigned actual_growth;
	char file_name[256];

	sprintf(file_name, "fragment_%u", file_number);
	if (create)
		flags = O_CREAT | O_EXCL | O_WRONLY | tests_maybe_sync_flag();
	else
		flags = O_WRONLY | tests_maybe_sync_flag();
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	fd = open(file_name, flags, mode);
	if (fd == -1 && errno == ENOSPC) {
		errno = 0;
		return 0; /* File system full */
	}
	CHECK(fd != -1);
	actual_growth = tests_fill_fragment_file(file_number, fd, size);
	CHECK(close(fd) != -1);
	if (create && !actual_growth)
		tests_delete_fragment_file(file_number);
	return actual_growth;
}

/* Write size bytes at offset to the file "fragment_n" where n is the
   file_number and file_number also determines the random data written
   i.e. seed for random numbers */
unsigned tests_overwite_fragment_file(	unsigned file_number,
					off_t offset,
					unsigned size)
{
	int fd;
	unsigned actual_size;
	char file_name[256];

	sprintf(file_name, "fragment_%u", file_number);
	fd = open(file_name, O_RDWR | tests_maybe_sync_flag());
	if (fd == -1 && errno == ENOSPC) {
		errno = 0;
		return 0; /* File system full */
	}
	CHECK(fd != -1);
	actual_size = tests_write_fragment_file(file_number,
		fd, offset, size);
	CHECK(close(fd) != -1);
	return actual_size;
}

/* Delete file "fragment_n" where n is the file_number */
void tests_delete_fragment_file(unsigned file_number)
{
	char file_name[256];

	sprintf(file_name, "fragment_%u", file_number);
	tests_delete_file(file_name);
}

/* Check the random data in file "fragment_n" is what is expected */
void tests_check_fragment_file_fd(unsigned file_number, int fd)
{
	ssize_t sz, i;
	int d;
	uint64_t u;
	char buf[8192];

	CHECK(lseek(fd, 0, SEEK_SET) == 0);
	srand(file_number);
	u = RAND_MAX;
	u += 1;
	u /= 256;
	d = (int) u;
	for (;;) {
		sz = read(fd, buf, 8192);
		if (sz == 0)
			break;
		CHECK(sz >= 0);
		for (i = 0; i < sz; ++i)
			CHECK(buf[i] == (char) (rand() / d));
	}
}

/* Check the random data in file "fragment_n" is what is expected */
void tests_check_fragment_file(unsigned file_number)
{
	int fd;
	ssize_t sz, i;
	int d;
	uint64_t u;
	char file_name[256];
	char buf[8192];

	sprintf(file_name, "fragment_%u", file_number);
	fd = open(file_name, O_RDONLY);
	CHECK(fd != -1);
	srand(file_number);
	u = RAND_MAX;
	u += 1;
	u /= 256;
	d = (int) u;
	for (;;) {
		sz = read(fd, buf, 8192);
		if (sz == 0)
			break;
		CHECK(sz >= 0);
		for (i = 0; i < sz; ++i)
			CHECK(buf[i] == (char) (rand() / d));
	}
	CHECK(close(fd) != -1);
}

/* Central point to decide whether to use fsync */
void tests_maybe_sync(int fd)
{
	if (tests_ok_to_sync)
		CHECK(fsync(fd) != -1);
}

/* Return O_SYNC if ok to sync otherwise return 0 */
int tests_maybe_sync_flag(void)
{
	if (tests_ok_to_sync)
		return O_SYNC;
	return 0;
}

/* Return random number from 0 to n - 1 */
size_t tests_random_no(size_t n)
{
	uint64_t a, b;

	if (!n)
		return 0;
	if (n - 1 <= RAND_MAX) {
		a = rand();
		b = RAND_MAX;
		b += 1;
	} else {
		const uint64_t u = 1 + (uint64_t) RAND_MAX;
		a = rand();
		a *= u;
		a += rand();
		b = u * u;
		CHECK(n <= b);
	}
	if (RAND_MAX <= UINT32_MAX && n <= UINT32_MAX)
		return a * n / b;
	else /*if (RAND_MAX <= UINT64_MAX && n <= UINT64_MAX)*/ {
		uint64_t x, y;
		if (a < n) {
			x = a;
			y = n;
		} else {
			x = n;
			y = a;
		}
		return (x * (y / b)) + ((x * (y % b)) / b);
	}
}

/* Make a directory empty */
void tests_clear_dir(const char *dir_name)
{
	DIR *dir;
	struct dirent *entry;
	char buf[4096];

	dir = opendir(dir_name);
	CHECK(dir != NULL);
	CHECK(getcwd(buf, 4096) != NULL);
	CHECK(chdir(dir_name) != -1);
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		if (entry) {
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0) {
				if (entry->d_type == DT_DIR) {
					tests_clear_dir(entry->d_name);
					CHECK(rmdir(entry->d_name) != -1);
				} else
					CHECK(unlink(entry->d_name) != -1);
			}
		} else {
			CHECK(errno == 0);
			break;
		}
	}
	CHECK(chdir(buf) != -1);
	CHECK(closedir(dir) != -1);
}

/* Create an empty sub-directory or small file in the current directory */
int64_t tests_create_entry(char *return_name)
{
	int fd;
	char name[256];

	for (;;) {
		sprintf(name, "%u", (unsigned) tests_random_no(10000000));
		fd = open(name, O_RDONLY);
		if (fd == -1)
			break;
		close(fd);
	}
	if (return_name)
		strcpy(return_name, name);
	if (tests_random_no(2)) {
		return tests_create_file(name, tests_random_no(4096));
	} else {
		if (mkdir(name, 0777) == -1) {
			CHECK(errno == ENOSPC);
			errno = 0;
			return 0;
		}
		return TESTS_EMPTY_DIR_SIZE;
	}
}

/* Remove a random file of empty sub-directory from the current directory */
int64_t tests_remove_entry(void)
{
	DIR *dir;
	struct dirent *entry;
	unsigned count = 0, pos;
	int64_t result = 0;

	dir = opendir(".");
	CHECK(dir != NULL);
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		if (entry) {
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0)
				++count;
		} else {
			CHECK(errno == 0);
			break;
		}
	}
	pos = tests_random_no(count);
	count = 0;
	rewinddir(dir);
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		if (!entry) {
			CHECK(errno == 0);
			break;
		}
		if (strcmp(".",entry->d_name) != 0 &&
				strcmp("..",entry->d_name) != 0) {
			if (count == pos) {
				if (entry->d_type == DT_DIR) {
					tests_clear_dir(entry->d_name);
					CHECK(rmdir(entry->d_name) != -1);
					result = TESTS_EMPTY_DIR_SIZE;
				} else {
					struct stat st;
					CHECK(stat(entry->d_name, &st) != -1);
					result = st.st_size;
					CHECK(unlink(entry->d_name) != -1);
				}
			}
			++count;
		}
	}
	CHECK(closedir(dir) != -1);
	return result;
}

/* Read mount information from /proc/mounts or /etc/mtab */
int tests_get_mount_info(struct mntent *info)
{
	FILE *f;
	struct mntent *entry;
	int found = 0;

	f = fopen("/proc/mounts", "rb");
	if (!f)
		f = fopen("/etc/mtab", "rb");
	CHECK(f != NULL);
	while (!found) {
		entry = getmntent(f);
		if (entry) {
			if (strcmp(entry->mnt_dir,
				tests_file_system_mount_dir) == 0) {
				found = 1;
				*info = *entry;
			}
		} else
			break;
	}
	CHECK(fclose(f) == 0);
	return found;
}

/*
 * This funcion parses file-system options string, extracts standard mount
 * options from there, and saves them in the @flags variable. The non-standard
 * (fs-specific) mount options are left in @mnt_opts string, while the standard
 * ones will be removed from it.
 *
 * The reason for this perverted function is that we want to preserve mount
 * options when unmounting the file-system and mounting it again. But we cannot
 * pass standard* mount optins (like sync, ro, etc) as a string to the
 * 'mount()' function, because it fails. It accepts standard mount options only
 * as flags. And only the FS-specific mount options are accepted in form of a
 * string.
 */
static int process_mount_options(char **mnt_opts, unsigned long *flags)
{
	char *tmp, *opts, *p;
	const char *opt;

	/*
	 * We are going to use 'strtok()' which modifies the original string,
	 * so duplicate it.
	 */
	tmp = strdup(*mnt_opts);
	if (!tmp)
		goto out_mem;

	p = opts = calloc(1, strlen(*mnt_opts) + 1);
	if (!opts) {
		free(tmp);
		goto out_mem;
	}

	*flags = 0;
	opt = strtok(tmp, ",");
	while (opt) {
		if (!strcmp(opt, "rw"))
			;
		else if (!strcmp(opt, "ro"))
			*flags |= MS_RDONLY;
		else if (!strcmp(opt, "dirsync"))
			*flags |= MS_DIRSYNC;
		else if (!strcmp(opt, "noatime"))
			*flags |= MS_NOATIME;
		else if (!strcmp(opt, "nodiratime"))
			*flags |= MS_NODIRATIME;
		else if (!strcmp(opt, "noexec"))
			*flags |= MS_NOEXEC;
		else if (!strcmp(opt, "nosuid"))
			*flags |= MS_NOSUID;
		else if (!strcmp(opt, "relatime"))
			*flags |= MS_RELATIME;
		else if (!strcmp(opt, "sync"))
			*flags |= MS_SYNCHRONOUS;
		else {
			int len = strlen(opt);

			if (p != opts)
				*p++ = ',';
			memcpy(p, opt, len);
			p += len;
			*p = '\0';
		}

		opt = strtok(NULL, ",");
	}

	free(tmp);
	*mnt_opts = opts;
	return 0;

out_mem:
	fprintf(stderr, "cannot allocate memory\n");
	return 1;
}

/*
 * Re-mount test file system. Randomly choose how to do this: re-mount R/O then
 * re-mount R/W, or unmount, then mount R/W, or unmount then mount R/O then
 * re-mount R/W, etc. This should improve test coverage.
 */
void tests_remount(void)
{
	int err;
	struct mntent mount_info;
	char *source, *target, *filesystemtype, *data;
	char cwd[4096];
	unsigned long mountflags, flags;
	unsigned int rorw1, um, um_ro, um_rorw, rorw2;

	CHECK(tests_get_mount_info(&mount_info));

	if (strcmp(mount_info.mnt_dir,"/") == 0)
		return;

	/* Save current working directory */
	CHECK(getcwd(cwd, 4096) != NULL);
	/* Temporarily change working directory to '/' */
	CHECK(chdir("/") != -1);

	/* Choose what to do */
	rorw1 = tests_random_no(2);
	um = tests_random_no(2);
	um_ro = tests_random_no(2);
	um_rorw = tests_random_no(2);
	rorw2 = tests_random_no(2);

	if (rorw1 + um + rorw2 == 0)
		um = 1;

	source = mount_info.mnt_fsname;
	target = tests_file_system_mount_dir;
	filesystemtype = tests_file_system_type;
	data = mount_info.mnt_opts;
	process_mount_options(&data, &mountflags);

	if (rorw1) {
		/* Re-mount R/O and then re-mount R/W */
		flags = mountflags | MS_RDONLY | MS_REMOUNT;
		err = mount(source, target, filesystemtype, flags, data);
		CHECK(err != -1);

		flags = mountflags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		err = mount(source, target, filesystemtype, flags, data);
		CHECK(err != -1);
	}

	if (um) {
		/* Unmount and mount */
		if (um_ro) {
			/* But re-mount R/O before unmounting */
			flags = mountflags | MS_RDONLY | MS_REMOUNT;
			err = mount(source, target, filesystemtype,
				    flags, data);
			CHECK(err != -1);
		}

		CHECK(umount(target) != -1);

		if (!um_rorw) {
			/* Mount R/W straight away */
			err = mount(source, target, filesystemtype,
				    mountflags, data);
			CHECK(err != -1);
		} else {
			/* Mount R/O and then re-mount R/W */
			err = mount(source, target, filesystemtype,
				    mountflags | MS_RDONLY, data);
			CHECK(err != -1);

			flags = mountflags | MS_REMOUNT;
			flags &= ~((unsigned long)MS_RDONLY);
			err = mount(source, target, filesystemtype,
				    flags, data);
			CHECK(err != -1);
		}
	}

	if (rorw2) {
		/* Re-mount R/O and then re-mount R/W */
		flags = mountflags | MS_RDONLY | MS_REMOUNT;
		err = mount(source, target, filesystemtype, flags, data);
		CHECK(err != -1);

		flags = mountflags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		err = mount(source, target, filesystemtype, flags, data);
		CHECK(err != -1);
	}

	/* Restore the previous working directory */
	CHECK(chdir(cwd) != -1);
}

/* Un-mount or re-mount test file system */
static void tests_mnt(int mnt)
{
	static struct mntent mount_info;
	char *source;
	char *target;
	char *filesystemtype;
	unsigned long mountflags;
	char *data;
	static char cwd[4096];

	if (mnt == 0) {
		CHECK(tests_get_mount_info(&mount_info));
		if (strcmp(mount_info.mnt_dir,"/") == 0)
			return;
		CHECK(getcwd(cwd, 4096) != NULL);
		CHECK(chdir("/") != -1);
		CHECK(umount(tests_file_system_mount_dir) != -1);
	} else {
		source = mount_info.mnt_fsname;
		target = tests_file_system_mount_dir;
		filesystemtype = tests_file_system_type;
		data = mount_info.mnt_opts;
		process_mount_options(&data, &mountflags);

		CHECK(mount(source, target, filesystemtype, mountflags, data)
			!= -1);
		CHECK(chdir(cwd) != -1);
	}
}

/* Unmount test file system */
void tests_unmount(void)
{
	tests_mnt(0);
}

/* Mount test file system */
void tests_mount(void)
{
	tests_mnt(1);
}

/* Check whether the test file system is also the root file system */
int tests_fs_is_rootfs(void)
{
	struct stat f_info;
	struct stat root_f_info;

	CHECK(stat(tests_file_system_mount_dir, &f_info) != -1);
	CHECK(stat("/", &root_f_info) != -1);
	if (f_info.st_dev == root_f_info.st_dev)
		return 1;
	else
		return 0;
}

/* Try to make a directory empty */
void tests_try_to_clear_dir(const char *dir_name)
{
	DIR *dir;
	struct dirent *entry;
	char buf[4096];

	dir = opendir(dir_name);
	if (dir == NULL)
		return;
	if (getcwd(buf, 4096) == NULL || chdir(dir_name) == -1) {
		closedir(dir);
		return;
	}
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		if (entry) {
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0) {
				if (entry->d_type == DT_DIR) {
					tests_try_to_clear_dir(entry->d_name);
					rmdir(entry->d_name);
				} else
					unlink(entry->d_name);
			}
		} else {
			CHECK(errno == 0);
			break;
		}
	}
	if (chdir(buf) < 0)
		perror("chdir");
	closedir(dir);
}

/* Check whether the test file system is also the current file system */
int tests_fs_is_currfs(void)
{
	struct stat f_info;
	struct stat curr_f_info;

	CHECK(stat(tests_file_system_mount_dir, &f_info) != -1);
	CHECK(stat(".", &curr_f_info) != -1);
	if (f_info.st_dev == curr_f_info.st_dev)
		return 1;
	else
		return 0;
}

#define PID_BUF_SIZE 64

/* Concatenate a pid to a string in a signal safe way */
void tests_cat_pid(char *buf, const char *name, pid_t pid)
{
	char *p;
	unsigned x;
	const char digits[] = "0123456789";
	char pid_buf[PID_BUF_SIZE];

	x = (unsigned) pid;
	p = pid_buf + PID_BUF_SIZE;
	*--p = '\0';
	if (x)
		while (x) {
			*--p = digits[x % 10];
			x /= 10;
		}
	else
		*--p = '0';
	buf[0] = '\0';
	strcat(buf, name);
	strcat(buf, p);
}
