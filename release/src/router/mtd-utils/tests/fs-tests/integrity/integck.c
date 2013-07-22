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
#include <dirent.h>
#include <getopt.h>
#include <assert.h>
#include <mntent.h>
#include <execinfo.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <linux/fs.h>

#define PROGRAM_VERSION "1.1"
#define PROGRAM_NAME "integck"
#include "common.h"
#include "libubi.h"

/*
 * WARNING! This is a dirty hack! The symbols for static functions are not
 * printed in the stack backtrace. So we remove ths 'static' keyword using the
 * pre-processor. This is really error-prone because this won't work if, e.g.,
 * local static variables were used.
 */
#ifdef INTEGCK_DEBUG
#define static
#endif

#define MAX_RANDOM_SEED 10000000

/* The pattern for the top directory where we run the test */
#define TEST_DIR_PATTERN "integck_test_dir_%u"

/* Maximum buffer size for a single read/write operation */
#define IO_BUFFER_SIZE 32768

/*
 * Check if a condition is true and die if not.
 */
#define stringify1(x) #x
#define stringify(x) stringify1(x)
#define CHECK(cond) do {                                                     \
	if (!(cond))                                                         \
		check_failed(stringify(cond), __func__, __FILE__, __LINE__); \
} while(0)

/*
 * In case of emulated power cut failures the FS has to return EROFS. But
 * unfortunately, the Linux kernel sometimes returns EIO to user-space anyway
 * (when write-back fails the return code is awayse EIO).
 */
#define pcv(fmt, ...) do {                                                   \
	int __err = 1;                                                       \
	if (args.power_cut_mode && (errno == EROFS || errno == EIO))         \
		__err = 0;                                                   \
	if (!args.power_cut_mode || args.verbose || __err)                   \
		normsg(fmt " (line %d, error %d (%s))",                      \
		       ##__VA_ARGS__, __LINE__, errno, strerror(errno));     \
	CHECK(!__err);                                                       \
} while(0)

#define v(fmt, ...) do {                                                     \
	if (args.verbose)                                                    \
		normsg(fmt " (line %d)", ##__VA_ARGS__, __LINE__);           \
} while(0)

/* The variables below are set by command line arguments */
static struct {
	long repeat_cnt;
	int power_cut_mode;
	int verify_ops;
	int reattach;
	int mtdn;
	int verbose;
	const char *mount_point;
} args = {
	.repeat_cnt = 1,
};

/*
 * The below data structure describes the tested file-system.
 *
 * max_name_len: maximum file name length
 * page_size: memory page size to use with 'mmap()'
 * log10_initial_free: logarighm base 10 of the initial amount of free space in
 *                     the tested file-system
 * nospc_size_ok: file size is updated even if the write operation failed with
 *                ENOSPC error
 * can_mmap: file-system supports share writable 'mmap()' operation
 * can_remount: is it possible to re-mount the tested file-system?
 * fstype: file-system type (e.g., "ubifs")
 * fsdev: the underlying device mounted by the tested file-system
 * mount_opts: non-standard mount options of the tested file-system (non-standard
 *             options are stored in string form as a comma-separated list)
 * mount_flags: standard mount options of the tested file-system (standard
 *              options as stored as a set of flags)
 * mount_point: tested file-system mount point path
 * test_dir: the directory on the tested file-system where we test
 */
static struct {
	int max_name_len;
	int page_size;
	unsigned int log10_initial_free;
	unsigned int nospc_size_ok:1;
	unsigned int can_mmap:1;
	unsigned int can_remount:1;
	char *fstype;
	char *fsdev;
	char *mount_opts;
	unsigned long mount_flags;
	char *mount_point;
	char *test_dir;
} fsinfo = {
	.nospc_size_ok = 1,
	.can_mmap = 1,
};

/* Structures to store data written to the test file system,
   so that we can check whether the file system is correct. */

struct write_info /* Record of random data written into a file */
{
	struct write_info *next;
	off_t offset; /* Where in the file the data was written */
	union {
		off_t random_offset; /* Call rand_r() this number of times first */
		off_t new_length; /* For truncation records new file length */
	};
	size_t size; /* Number of bytes written */
	unsigned int random_seed; /* Seed for rand_r() to create random data. If
				     greater than MAX_RANDOM_SEED then this is
				     a truncation record (raw_writes only) */
};

struct dir_entry_info;

struct file_info /* Each file has one of these */
{
	struct write_info *writes; /* Record accumulated writes to the file */
	struct write_info *raw_writes;
				/* Record in order all writes to the file */
	struct fd_info *fds; /* All open file descriptors for this file */
	struct dir_entry_info *links;
	off_t length;
	int link_count;
	unsigned int check_run_no; /* Run number used when checking */
	unsigned int no_space_error:1; /* File has incurred a ENOSPC error */
	unsigned int clean:1; /* Non-zero if the file is synchronized */
};

struct symlink_info /* Each symlink has one of these */
{
	char *target_pathname;
	struct dir_entry_info *entry; /* dir entry of this symlink */
};

struct dir_info /* Each directory has one of these */
{
	struct dir_info *parent; /* Parent directory or null
					for our top directory */
	unsigned int number_of_entries;
	struct dir_entry_info *first;
	struct dir_entry_info *entry; /* Dir entry of this dir */
	unsigned int clean:1; /* Non-zero if the directory is synchronized */
};

struct dir_entry_info /* Each entry in a directory has one of these */
{
	struct dir_entry_info *next; /* List of entries in directory */
	struct dir_entry_info *prev; /* List of entries in directory */
	struct dir_entry_info *next_link; /* List of hard links for same file */
	struct dir_entry_info *prev_link; /* List of hard links for same file */
	char *name;
	struct dir_info *parent; /* Parent directory */
	union {
		struct file_info *file;
		struct dir_info *dir;
		struct symlink_info *symlink;
		void *target;
	};
	char type; /* f => file, d => dir, s => symlink */
	char checked; /* Temporary flag used when checking */
};

struct fd_info /* We keep a number of files open */
{
	struct fd_info *next;
	struct file_info *file;
	int fd;
};

struct open_file_info /* We keep a list of open files */
{
	struct open_file_info *next;
	struct fd_info *fdi;
};

static struct dir_info *top_dir = NULL; /* Our top directory */

static struct open_file_info *open_files = NULL; /* We keep a list of
							open files */
static size_t open_files_count = 0;

static int grow   = 1; /* Should we try to grow files and directories */
static int shrink = 0; /* Should we try to shrink files and directories */
static int full   = 0; /* Flag that the file system is full */
static uint64_t operation_count = 0; /* Number of operations used to fill
                                        up the file system */
static unsigned int check_run_no;
static unsigned int random_seed;

/*
 * A buffer which is used by 'make_name()' to return the generated random name.
 */
static char *random_name_buf;

/*
 * This is a helper for the 'CHECK()' macro - prints a scary error message and
 * terminates the program.
 */
static void check_failed(const char *cond, const char *func, const char *file,
			 int line)
{
	int error = errno, count;
	void *addresses[128];

	fflush(stdout);
	fflush(stderr);
	errmsg("condition '%s' failed in %s() at %s:%d",
	       cond, func, file, line);
	normsg("error %d (%s)", error, strerror(error));
	/*
	 * Note, to make this work well you need:
	 * 1. Make all functions non-static - add "#define static'
	 * 2. Compile with -rdynamic and -g gcc options
	 * 3. Preferrably compile with -O0 to avoid inlining
	 */
	count = backtrace(addresses, 128);
	backtrace_symbols_fd(addresses, count, fileno(stdout));
	exit(EXIT_FAILURE);
}

/*
 * Is this 'struct write_info' actually holds information about a truncation?
 */
static int is_truncation(struct write_info *w)
{
	return w->random_seed > MAX_RANDOM_SEED;
}

/*
 * Return a random number between 0 and max - 1.
 */
static unsigned int random_no(unsigned int max)
{
	assert(max < RAND_MAX);
	if (max == 0)
		return 0;
	return rand_r(&random_seed) % max;
}

/*
 * Allocate a buffer of 'size' bytes and fill it with zeroes.
 */
static void *zalloc(size_t size)
{
	void *buf = malloc(size);

	CHECK(buf != NULL);
	memset(buf, 0, size);
	return buf;
}

/*
 * Duplicate a string.
 */
static char *dup_string(const char *s)
{
	char *str;

	assert(s != NULL);
	str = strdup(s);
	CHECK(str != NULL);
	return str;
}

static char *cat_strings(const char *a, const char *b)
{
	char *str;
	size_t sz;

	if (a && !b)
		return dup_string(a);
	if (b && !a)
		return dup_string(b);
	if (!a && !b)
		return NULL;
	sz = strlen(a) + strlen(b) + 1;
	str = malloc(sz);
	CHECK(str != NULL);
	strcpy(str, a);
	strcat(str, b);
	return str;
}

static char *cat_paths(const char *a, const char *b)
{
	char *str;
	size_t sz, na, nb;
	int as = 0, bs = 0;

	assert(a != NULL);
	assert(b != NULL);

	na = strlen(a);
	nb = strlen(b);
	if (na && a[na - 1] == '/')
		as = 1;
	if (nb && b[0] == '/')
		bs = 1;
	if ((as && !bs) || (!as && bs))
		return cat_strings(a, b);
	if (as && bs)
		return cat_strings(a, b + 1);

	sz = na + nb + 2;
	str = malloc(sz);
	CHECK(str != NULL);
	strcpy(str, a);
	strcat(str, "/");
	strcat(str, b);
	return str;
}

/*
 * Get the free space for the tested file system.
 */
static void get_fs_space(uint64_t *total, uint64_t *free)
{
	struct statvfs st;

	CHECK(statvfs(fsinfo.mount_point, &st) == 0);
	if (total)
		*total = (uint64_t)st.f_blocks * (uint64_t)st.f_frsize;
	if (free)
		*free =  (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
}

static char *dir_path(struct dir_info *parent, const char *name)
{
	char *parent_path, *path;

	if (!parent)
		return cat_paths(fsinfo.mount_point, name);
	parent_path = dir_path(parent->parent, parent->entry->name);
	path = cat_paths(parent_path, name);
	free(parent_path);
	return path;
}

static void open_file_add(struct fd_info *fdi)
{
	struct open_file_info *ofi;

	ofi = zalloc(sizeof(struct open_file_info));
	ofi->next = open_files;
	ofi->fdi = fdi;
	open_files = ofi;
	open_files_count += 1;
}

static void open_file_remove(struct fd_info *fdi)
{
	struct open_file_info *ofi;
	struct open_file_info **prev;

	prev = &open_files;
	for (ofi = open_files; ofi; ofi = ofi->next) {
		if (ofi->fdi == fdi) {
			*prev = ofi->next;
			free(ofi);
			open_files_count -= 1;
			return;
		}
		prev = &ofi->next;
	}
	CHECK(0); /* We are trying to remove something that is not there */
}

static struct fd_info *add_fd(struct file_info *file, int fd)
{
	struct fd_info *fdi;

	fdi = zalloc(sizeof(struct fd_info));
	fdi->next = file->fds;
	fdi->file = file;
	fdi->fd = fd;
	file->fds = fdi;
	open_file_add(fdi);
	return fdi;
}

/*
 * Free all the information about writes to a file.
 */
static void free_writes_info(struct file_info *file)
{
	struct write_info *w, *next;

	w = file->writes;
	while (w) {
		next = w->next;
		free(w);
		w = next;
	}

	w = file->raw_writes;
	while (w) {
		next = w->next;
		free(w);
		w = next;
	}
}

static void *add_dir_entry(struct dir_info *parent, char type, const char *name,
			   void *target)
{
	struct dir_entry_info *entry;

	entry = zalloc(sizeof(struct dir_entry_info));
	entry->type = type;
	entry->name = dup_string(name);
	entry->parent = parent;

	entry->next = parent->first;
	if (parent->first)
		parent->first->prev = entry;
	parent->first = entry;
	parent->number_of_entries += 1;
	parent->clean = 0;

	if (entry->type == 'f') {
		struct file_info *file = target;

		if (!file)
			file = zalloc(sizeof(struct file_info));
		entry->file = file;
		entry->next_link = file->links;
		if (file->links)
			file->links->prev_link = entry;
		file->links = entry;
		file->link_count += 1;
		return file;
	} else if (entry->type == 'd') {
		struct dir_info *dir = target;

		if (!dir)
			dir = zalloc(sizeof(struct dir_info));
		entry->dir = dir;
		dir->entry = entry;
		dir->parent = parent;
		return dir;
	} else if (entry->type == 's') {
		struct symlink_info *symlink = target;

		if (!symlink)
			symlink = zalloc(sizeof(struct symlink_info));
		entry->symlink = symlink;
		symlink->entry = entry;
		return symlink;
	} else
		assert(0);
}

static void remove_dir_entry(struct dir_entry_info *entry, int free_target)
{
	entry->parent->clean = 0;
	entry->parent->number_of_entries -= 1;
	if (entry->parent->first == entry)
		entry->parent->first = entry->next;
	if (entry->prev)
		entry->prev->next = entry->next;
	if (entry->next)
		entry->next->prev = entry->prev;

	if (entry->type == 'f') {
		struct file_info *file = entry->file;

		if (entry->prev_link)
			entry->prev_link->next_link = entry->next_link;
		if (entry->next_link)
			entry->next_link->prev_link = entry->prev_link;
		if (file->links == entry)
			file->links = entry->next_link;
		file->link_count -= 1;
		if (file->link_count == 0)
			assert(file->links == NULL);

		/* Free struct file_info if file is not open and not linked */
		if (free_target && !file->fds && !file->links) {
			free_writes_info(file);
			free(file);
		}
	}

	if (free_target) {
		if (entry->type == 'd') {
			free(entry->dir);
		} else if (entry->type == 's') {
			free(entry->symlink->target_pathname);
			free(entry->symlink);
		}
	}

	free(entry->name);
	free(entry);
}

/*
 * Create a new directory "name" in the parent directory described by "parent"
 * and add it to the in-memory list of directories. Returns zero in case of
 * success and -1 in case of failure.
 */
static int dir_new(struct dir_info *parent, const char *name)
{
	char *path;

	assert(parent);

	path = dir_path(parent, name);
	v("creating dir %s", path);
	if (mkdir(path, 0777) != 0) {
		if (errno == ENOSPC) {
			full = 1;
			free(path);
			return 0;
		}
		pcv("cannot create directory %s", path);
		free(path);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(path, &st) == 0);
		CHECK(S_ISDIR(st.st_mode));
	}
	free(path);

	add_dir_entry(parent, 'd', name, NULL);
	return 0;
}

static int file_delete(struct file_info *file);
static int file_unlink(struct dir_entry_info *entry);
static int symlink_remove(struct symlink_info *symlink);

static int dir_remove(struct dir_info *dir)
{
	char *path;

	/* Remove directory contents */
	while (dir->first) {
		struct dir_entry_info *entry;
		int ret = 0;

		entry = dir->first;
		if (entry->type == 'd')
			ret = dir_remove(entry->dir);
		else if (entry->type == 'f')
			ret = file_unlink(entry);
		else if (entry->type == 's')
			ret = symlink_remove(entry->symlink);
		else
			CHECK(0); /* Invalid struct dir_entry_info */
		if (ret)
			return -1;
	}

	/* Remove directory form the file-system */
	path = dir_path(dir->parent, dir->entry->name);
	if (rmdir(path) != 0) {
		pcv("cannot remove directory entry %s", path);
		free(path);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(path, &st) == -1);
		CHECK(errno == ENOENT);
	}

	/* Remove entry from parent directory */
	remove_dir_entry(dir->entry, 1);

	free(path);
	return 0;
}

static int file_new(struct dir_info *parent, const char *name)
{
	char *path;
	mode_t mode;
	int fd;

	assert(parent != NULL);

	path = dir_path(parent, name);
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	v("creating file %s", path);
	fd = open(path, O_CREAT | O_EXCL | O_RDWR, mode);
	if (fd == -1) {
		if (errno == ENOSPC) {
			full = 1;
			free(path);
			return 0;
		}
		pcv("cannot create file %s", path);
		free(path);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(path, &st) == 0);
		CHECK(S_ISREG(st.st_mode));
	}

	add_dir_entry(parent, 'f', name, NULL);

	close(fd);
	free(path);
	return 0;
}

static int link_new(struct dir_info *parent, const char *name,
		    struct file_info *file)
{
	struct dir_entry_info *entry;
	char *path, *target;
	int ret;
	struct stat st1, st2;

	entry = file->links;
	if (!entry)
		return 0;

	path = dir_path(parent, name);
	target = dir_path(entry->parent, entry->name);

	if (args.verify_ops)
		CHECK(lstat(target, &st1) == 0);

	v("creating hardlink %s ---> %s", path, target);
	ret = link(target, path);
	if (ret != 0) {
		if (errno == ENOSPC) {
			ret = 0;
			full = 1;
		} else
			pcv("cannot create hardlink %s in directory %s to file %s",
			    path, parent->entry->name, target);
		free(target);
		free(path);
		return ret;
	}

	if (args.verify_ops) {
		CHECK(lstat(path, &st2) == 0);
		CHECK(S_ISREG(st2.st_mode));
		CHECK(st1.st_ino == st2.st_ino);
		CHECK(st2.st_nlink > 1);
		CHECK(st2.st_nlink == st1.st_nlink + 1);
	}

	add_dir_entry(parent, 'f', name, file);

	free(target);
	free(path);
	return 0;
}

static void file_close(struct fd_info *fdi);

static void file_close_all(struct file_info *file)
{
	struct fd_info *fdi = file->fds;

	while (fdi) {
		struct fd_info *next = fdi->next;

		file_close(fdi);
		fdi = next;
	}
}

/*
 * Unlink a directory entry for a file.
 */
static int file_unlink(struct dir_entry_info *entry)
{
	char *path;
	int ret;

	path = dir_path(entry->parent, entry->name);
	/* Unlink the file */
	ret = unlink(path);
	if (ret) {
		pcv("cannot unlink file %s", path);
		free(path);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(path, &st) == -1);
		CHECK(errno == ENOENT);
	}

	/* Remove file entry from parent directory */
	remove_dir_entry(entry, 1);

	free(path);
	return 0;
}

static struct dir_entry_info *pick_entry(struct file_info *file)
{
	struct dir_entry_info *entry;
	unsigned int r;

	if (!file->link_count)
		return NULL;
	r = random_no(file->link_count);
	entry = file->links;
	while (entry && r--)
		entry = entry->next_link;
	return entry;
}

static int file_unlink_file(struct file_info *file)
{
	struct dir_entry_info *entry;

	entry = pick_entry(file);
	if (!entry)
		return 0;
	return file_unlink(entry);
}

/*
 * Close all open descriptors for a file described by 'file' and delete it by
 * unlinking all its hardlinks.
 */
static int file_delete(struct file_info *file)
{
	struct dir_entry_info *entry = file->links;

	file_close_all(file);
	while (entry) {
		struct dir_entry_info *next = entry->next_link;

		if (file_unlink(entry))
			return -1;
		entry = next;
	}

	return 0;
}

static void file_info_display(struct file_info *file)
{
	struct dir_entry_info *entry;
	struct write_info *w;
	unsigned int wcnt;

	normsg("File Info:");
	normsg("    Link count: %d", file->link_count);
	normsg("    Links:");
	entry = file->links;
	while (entry) {
		normsg("      Name: %s", entry->name);
		normsg("      Directory: %s", entry->parent->entry->name);
		entry = entry->next_link;
	}
	normsg("    Length: %llu", (unsigned long long)file->length);
	normsg("    File was open: %s",
	       (file->fds == NULL) ? "false" : "true");
	normsg("    File was deleted: %s",
	       (file->link_count == 0) ? "true" : "false");
	normsg("    File was out of space: %s",
	       (file->no_space_error == 0) ? "false" : "true");
	normsg("    File Data:");
	wcnt = 0;
	w = file->writes;
	while (w) {
		normsg("        Offset: %llu  Size: %zu  Seed: %llu  R.Off: %llu",
		       (unsigned long long)w->offset, w->size,
		       (unsigned long long)w->random_seed,
		       (unsigned long long)w->random_offset);
		wcnt += 1;
		w = w->next;
	}
	normsg("    %u writes", wcnt);
	normsg("    ============================================");
	normsg("    Write Info:");
	wcnt = 0;
	w = file->raw_writes;
	while (w) {
		if (is_truncation(w))
			normsg("        Trunc from %llu to %llu",
			       (unsigned long long)w->offset,
			       (unsigned long long)w->new_length);
		else
			normsg("        Offset: %llu  Size: %zu  Seed: %llu  R.Off: %llu",
			       (unsigned long long)w->offset, w->size,
			       (unsigned long long)w->random_seed,
			       (unsigned long long)w->random_offset);
		wcnt += 1;
		w = w->next;
	}
	normsg("    %u writes or truncations", wcnt);
	normsg("    ============================================");
}

static int file_open(struct file_info *file)
{
	int fd, flags = O_RDWR;
	char *path;

	assert(file->links);

	path = dir_path(file->links->parent, file->links->name);
	if (random_no(100) == 1)
		flags |= O_SYNC;
	fd = open(path, flags);
	if (fd == -1) {
		pcv("cannot open file %s", path);
		free(path);
		return -1;
	}
	free(path);
	add_fd(file, fd);
	return 0;
}

static const char *get_file_name(struct file_info *file)
{
	if (file->links)
		return file->links->name;
	return "(unlinked file, no names)";
}

/*
 * Write random 'size' bytes of random data to offset 'offset'. Seed the random
 * gererator with 'seed'. Return amount of written data on success and -1 on
 * failure.
 */
static ssize_t file_write_data(struct file_info *file, int fd, off_t offset,
			       size_t size, unsigned int seed)
{
	size_t remains, actual, block;
	ssize_t written;
	char buf[IO_BUFFER_SIZE];

	CHECK(lseek(fd, offset, SEEK_SET) != (off_t)-1);
	remains = size;
	actual = 0;
	written = IO_BUFFER_SIZE;
	v("write %zd bytes, offset %llu, file %s",
	  size, (unsigned long long)offset, get_file_name(file));
	while (remains) {
		/* Fill up buffer with random data */
		if (written < IO_BUFFER_SIZE) {
			memmove(buf, buf + written, IO_BUFFER_SIZE - written);
			written = IO_BUFFER_SIZE - written;
		} else
			written = 0;
		for (; written < IO_BUFFER_SIZE; ++written)
			buf[written] = rand_r(&seed);
		/* Write a block of data */
		if (remains > IO_BUFFER_SIZE)
			block = IO_BUFFER_SIZE;
		else
			block = remains;
		written = write(fd, buf, block);
		if (written < 0) {
			if (errno == ENOSPC) {
				full = 1;
				file->no_space_error = 1;
				break;
			}
			pcv("failed to write %zu bytes to offset %llu of file %s",
			    block, (unsigned long long)(offset + actual),
			    get_file_name(file));
			return -1;
		}
		remains -= written;
		actual += written;
	}
	return actual;
}

static void file_check_data(struct file_info *file, int fd,
			    struct write_info *w);

/*
 * Save the information about a file write operation and verify the write if
 * necessary.
 */
static void file_write_info(struct file_info *file, int fd, off_t offset,
			    size_t size, unsigned int seed)
{
	struct write_info *new_write, *w, **prev, *tmp;
	int inserted;
	off_t end, chg;

	/* Create struct write_info */
	new_write = zalloc(sizeof(struct write_info));
	new_write->offset = offset;
	new_write->size = size;
	new_write->random_seed = seed;

	w = zalloc(sizeof(struct write_info));
	w->next = file->raw_writes;
	w->offset = offset;
	w->size = size;
	w->random_seed = seed;
	file->raw_writes = w;

	if (args.verify_ops && !args.power_cut_mode)
		file_check_data(file, fd, new_write);

	/* Insert it into file->writes */
	inserted = 0;
	end = offset + size;
	w = file->writes;
	prev = &file->writes;
	while (w) {
		if (w->offset >= end) {
			/* w comes after new_write, so insert before it */
			new_write->next = w;
			*prev = new_write;
			inserted = 1;
			break;
		}
		/* w does not come after new_write */
		if (w->offset + w->size > offset) {
			/* w overlaps new_write */
			if (w->offset < offset) {
				/* w begins before new_write begins */
				if (w->offset + w->size <= end)
					/* w ends before new_write ends */
					w->size = offset - w->offset;
				else {
					/* w ends after new_write ends */
					/* Split w */
					tmp = malloc(sizeof(struct write_info));
					CHECK(tmp != NULL);
					*tmp = *w;
					chg = end - tmp->offset;
					tmp->offset += chg;
					tmp->random_offset += chg;
					tmp->size -= chg;
					w->size = offset - w->offset;
					/* Insert new struct write_info */
					w->next = new_write;
					new_write->next = tmp;
					inserted = 1;
					break;
				}
			} else {
				/* w begins after new_write begins */
				if (w->offset + w->size <= end) {
					/* w is completely overlapped,
					   so remove it */
					*prev = w->next;
					tmp = w;
					w = w->next;
					free(tmp);
					continue;
				}
				/* w ends after new_write ends */
				chg = end - w->offset;
				w->offset += chg;
				w->random_offset += chg;
				w->size -= chg;
				continue;
			}
		}
		prev = &w->next;
		w = w->next;
	}
	if (!inserted)
		*prev = new_write;
	/* Update file length */
	if (end > file->length)
		file->length = end;
}

/* Randomly select offset and and size to write in a file */
static void get_offset_and_size(struct file_info *file,
				off_t *offset,
				size_t *size)
{
	unsigned int r, n;

	r = random_no(100);
	if (r == 0 && grow)
		/* 1 time in 100, when growing, write off the end of the file */
		*offset = file->length + random_no(10000000);
	else if (r < 4)
		/* 3 (or 4) times in 100, write at the beginning of file */
		*offset = 0;
	else if (r < 52 || !grow)
		/* 48 times in 100, write into the file */
		*offset = random_no(file->length);
	else
		/* 48 times in 100, write at the end of the  file */
		*offset = file->length;
	/* Distribute the size logarithmically */
	if (random_no(1000) == 0)
		r = random_no(fsinfo.log10_initial_free + 2);
	else
		r = random_no(fsinfo.log10_initial_free);
	n = 1;
	while (r--)
		n *= 10;
	*size = random_no(n);
	if (!grow && *offset + *size > file->length)
		*size = file->length - *offset;
	if (*size == 0)
		*size = 1;
}

static void file_check_hole(struct file_info *file, int fd, off_t offset,
			    size_t size);

static void file_truncate_info(struct file_info *file, int fd,
			       size_t new_length)
{
	struct write_info *w, **prev, *tmp;

	/* Remove / truncate file->writes */
	w = file->writes;
	prev = &file->writes;
	while (w) {
		if (w->offset >= new_length) {
			/* w comes after eof, so remove it */
			*prev = w->next;
			tmp = w;
			w = w->next;
			free(tmp);
			continue;
		}
		if (w->offset + w->size > new_length)
			w->size = new_length - w->offset;
		prev = &w->next;
		w = w->next;
	}
	/* Add an entry in raw_writes for the truncation */
	w = zalloc(sizeof(struct write_info));
	w->next = file->raw_writes;
	w->offset = file->length;
	w->new_length = new_length;
	w->random_seed = MAX_RANDOM_SEED + 1;
	file->raw_writes = w;

	if (args.verify_ops && !args.power_cut_mode &&
	    new_length > file->length)
		file_check_hole(file, fd, file->length,
				new_length - file->length);

	/* Update file length */
	file->length = new_length;
}

/*
 * Truncate a file to length 'new_length'. If there is no enough space to
 * peform the operation, this function returns 1. Returns 0 on success and -1
 * on failure.
 */
static int file_ftruncate(struct file_info *file, int fd, off_t new_length)
{
	if (ftruncate(fd, new_length) != 0) {
		if (errno == ENOSPC) {
			file->no_space_error = 1;
			/* Delete errored files */
			if (!fsinfo.nospc_size_ok)
				if (file_delete(file))
					return -1;
			return 1;
		} else
			pcv("cannot truncate file %s to %llu",
			    get_file_name(file),
			    (unsigned long long)new_length);
		return -1;
	}

	if (args.verify_ops)
		CHECK(lseek(fd, 0, SEEK_END) == new_length);

	return 0;
}

/*
 * 'mmap()' a file and randomly select where to write data.
 */
static int file_mmap_write(struct file_info *file)
{
	size_t write_cnt = 0, r, i, len, size;
	struct write_info *w = file->writes;
	void *addr;
	char *waddr, *path;
	off_t offs, offset;
	unsigned int seed, seed_tmp;
	uint64_t free_space;
	int fd;

	assert(!args.power_cut_mode);

	if (!file->links)
		return 0;
	get_fs_space(NULL, &free_space);
	if (!free_space)
		return 0;

	/* Randomly pick a written area of the file */
	if (!w)
		return 0;
	while (w) {
		write_cnt += 1;
		w = w->next;
	}
	r = random_no(write_cnt);
	w = file->writes;
	for (i = 0; w && w->next && i < r; i++)
		w = w->next;

	offs = (w->offset / fsinfo.page_size) * fsinfo.page_size;
	len = w->size + (w->offset - offs);
	if (len > 1 << 24)
		len = 1 << 24;

	/* Open it */
	assert(file->links);
	path = dir_path(file->links->parent, file->links->name);
	fd = open(path, O_RDWR);
	if (fd == -1) {
		pcv("cannot open file %s to do mmap", path);
		goto out_error;
	}

	/* mmap it */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offs);
	if (addr == MAP_FAILED) {
		pcv("cannot mmap file %s", path);
		goto out_error;
	}

	/* Randomly select a part of the mmapped area to write */
	size = random_no(w->size);
	if (size > free_space)
		size = free_space;
	if (size == 0)
		size = 1;
	offset = w->offset + random_no(w->size - size);

	/* Write it */
	seed_tmp = seed = random_no(MAX_RANDOM_SEED);
	waddr = addr + (offset - offs);
	for (i = 0; i < size; i++)
		waddr[i] = rand_r(&seed_tmp);

	/* Unmap it */
	if (munmap(addr, len)) {
		pcv("cannot unmap file %s", path);
		goto out_error;
	}

	/* Record what was written */
	file_write_info(file, fd, offset, size, seed);
	free(path);
	CHECK(close(fd) == 0);
	return 0;

out_error:
	free(path);
	CHECK(close(fd) == 0);
	return -1;
}

/*
 * Write random amount of data to a random offset in an open file or randomly
 * choose to truncate it.
 */
static int file_write(struct file_info *file, int fd)
{
	int ret;

	file->clean = 0;

	if (!args.power_cut_mode && fsinfo.can_mmap && !full &&
	    file->link_count && random_no(100) == 1) {
		/*
		 * Do not do 'mmap()' operations if:
		 * 1. we are in power cut testing mode, because an emulated
		 *    power cut failure may cause SIGBUS when we are writing to
		 *    the 'mmap()'ed area, and SIGBUS is not easy to ignore.
		 * 2. When the file-system is full, because again, writing to the
		 *    'mmap()'ed area may cause SIGBUS when the space allocation
		 *    fails. This is not enough to guarantee we never get
		 *    SIGBUS, though. For example, if we write a lot to a hole,
		 *    this might require a lot of additional space, and we may
		 *    fail here. But I do not know why we never observed this,
		 *    probably this is just very unlikely.
		 */
		ret = file_mmap_write(file);
	} else {
		int truncate = 0;
		off_t offset;
		size_t size;
		ssize_t actual;
		unsigned int seed;

		get_offset_and_size(file, &offset, &size);
		seed = random_no(MAX_RANDOM_SEED);
		actual = file_write_data(file, fd, offset, size, seed);
		if (actual < 0)
			return -1;

		if (actual != 0)
			file_write_info(file, fd, offset, actual, seed);

		if (offset + actual <= file->length && shrink) {
			/*
			 * 1 time in 100, when shrinking truncate after the
			 * write.
			 */
			if (random_no(100) == 0)
				truncate = 1;
		}

		if (truncate) {
			size_t new_length = offset + actual;

			ret = file_ftruncate(file, fd, new_length);
			if (ret == -1)
				return -1;
			if (!ret)
				file_truncate_info(file, fd, new_length);
		}

		/* Delete errored files */
		if (!fsinfo.nospc_size_ok && file->no_space_error)
			return file_delete(file);
	}

	/* Sync sometimes */
	if (random_no(100) >= 99) {
		if (random_no(100) >= 50) {
			v("fsyncing file %s", get_file_name(file));
			ret = fsync(fd);
			if (ret)
				pcv("fsync failed for %s", get_file_name(file));
		} else {
			v("fdatasyncing file %s", get_file_name(file));
			ret = fdatasync(fd);
			if (ret)
				pcv("fdatasync failed for %s",
				    get_file_name(file));
		}
		if (ret)
			return -1;
		file->clean = 1;
	}

	return 0;
}

/*
 * Write random amount of data to a random offset in a file or randomly
 * choose to truncate it.
 */
static int file_write_file(struct file_info *file)
{
	int fd, ret;
	char *path;

	assert(file->links);
	path = dir_path(file->links->parent, file->links->name);
	fd = open(path, O_RDWR);
	if (fd == -1) {
		pcv("cannot open file %s for writing", path);
		free(path);
		return -1;
	}
	ret = file_write(file, fd);
	CHECK(close(fd) == 0);
	free(path);
	return ret;
}

/*
 * Truncate an open file randomly.
 */
static int file_truncate(struct file_info *file, int fd)
{
	int ret;
	size_t new_length = random_no(file->length);

	file->clean = 0;
	ret = file_ftruncate(file, fd, new_length);
	if (ret == -1)
		return -1;
	if (!ret)
		file_truncate_info(file, fd, new_length);
	return 0;
}

static int file_truncate_file(struct file_info *file)
{
	int fd;
	char *path;
	int ret;

	assert(file->links);
	path = dir_path(file->links->parent, file->links->name);
	fd = open(path, O_WRONLY);
	if (fd == -1) {
		pcv("cannot open file %s to truncate", path);
		free(path);
		return -1;
	}
	free(path);
	ret = file_truncate(file, fd);
	CHECK(close(fd) == 0);
	return ret;
}

static void file_close(struct fd_info *fdi)
{
	struct file_info *file;
	struct fd_info *fdp;
	struct fd_info **prev;

	/* Close file */
	CHECK(close(fdi->fd) == 0);
	/* Remove struct fd_info */
	open_file_remove(fdi);
	file = fdi->file;
	prev = &file->fds;
	for (fdp = file->fds; fdp; fdp = fdp->next) {
		if (fdp == fdi) {
			*prev = fdi->next;
			free(fdi);
			if (!file->link_count && !file->fds) {
				free_writes_info(file);
				free(file);
			}
			return;
		}
		prev = &fdp->next;
	}
	CHECK(0); /* Didn't find struct fd_info */
}

static void file_rewrite_data(int fd, struct write_info *w, char *buf)
{
	size_t remains, block;
	ssize_t written;
	off_t r;
	unsigned int seed = w->random_seed;

	for (r = 0; r < w->random_offset; ++r)
		rand_r(&seed);
	CHECK(lseek(fd, w->offset, SEEK_SET) != (off_t)-1);
	remains = w->size;
	written = IO_BUFFER_SIZE;
	while (remains) {
		/* Fill up buffer with random data */
		if (written < IO_BUFFER_SIZE)
			memmove(buf, buf + written, IO_BUFFER_SIZE - written);
		else
			written = 0;
		for (; written < IO_BUFFER_SIZE; ++written)
			buf[written] = rand_r(&seed);
		/* Write a block of data */
		if (remains > IO_BUFFER_SIZE)
			block = IO_BUFFER_SIZE;
		else
			block = remains;
		written = write(fd, buf, block);
		CHECK(written == block);
		remains -= written;
	}
}

static void save_file(int fd, struct file_info *file)
{
	int w_fd;
	struct write_info *w;
	char buf[IO_BUFFER_SIZE];
	char name[256];

	/* Open file to save contents to */
	strcpy(name, "/tmp/");
	strcat(name, get_file_name(file));
	strcat(name, ".integ.sav.read");
	normsg("Saving %sn", name);
	w_fd = open(name, O_CREAT | O_WRONLY, 0777);
	CHECK(w_fd != -1);

	/* Start at the beginning */
	CHECK(lseek(fd, 0, SEEK_SET) != (off_t)-1);

	for (;;) {
		ssize_t r = read(fd, buf, IO_BUFFER_SIZE);
		CHECK(r != -1);
		if (!r)
			break;
		CHECK(write(w_fd, buf, r) == r);
	}
	CHECK(close(w_fd) == 0);

	/* Open file to save contents to */
	strcpy(name, "/tmp/");
	strcat(name, get_file_name(file));
	strcat(name, ".integ.sav.written");
	normsg("Saving %s", name);
	w_fd = open(name, O_CREAT | O_WRONLY, 0777);
	CHECK(w_fd != -1);

	for (w = file->writes; w; w = w->next)
		file_rewrite_data(w_fd, w, buf);

	CHECK(close(w_fd) == 0);
}

static void file_check_hole(struct file_info *file, int fd, off_t offset,
			    size_t size)
{
	size_t remains, block, i;
	char buf[IO_BUFFER_SIZE];

	CHECK(lseek(fd, offset, SEEK_SET) != (off_t)-1);
	remains = size;
	while (remains) {
		if (remains > IO_BUFFER_SIZE)
			block = IO_BUFFER_SIZE;
		else
			block = remains;
		CHECK(read(fd, buf, block) == block);
		for (i = 0; i < block; ++i) {
			if (buf[i] != 0) {
				errmsg("file_check_hole failed at %zu checking "
				       "hole at %llu size %zu", size - remains + i,
				       (unsigned long long)offset, size);
				file_info_display(file);
				save_file(fd, file);
			}
			CHECK(buf[i] == 0);
		}
		remains -= block;
	}
}

static void file_check_data(struct file_info *file, int fd,
			    struct write_info *w)
{
	size_t remains, block, i;
	off_t r;
	char buf[IO_BUFFER_SIZE];
	unsigned int seed = w->random_seed;

	if (args.power_cut_mode && !file->clean)
		return;

	for (r = 0; r < w->random_offset; ++r)
		rand_r(&seed);
	CHECK(lseek(fd, w->offset, SEEK_SET) != (off_t)-1);
	remains = w->size;
	while (remains) {
		if (remains > IO_BUFFER_SIZE)
			block = IO_BUFFER_SIZE;
		else
			block = remains;
		CHECK(read(fd, buf, block) == block);
		for (i = 0; i < block; ++i) {
			char c = (char)rand_r(&seed);
			if (buf[i] != c) {
				errmsg("file_check_data failed at %zu checking "
				       "data at %llu size %zu", w->size - remains + i,
					(unsigned long long)w->offset, w->size);
				file_info_display(file);
				save_file(fd, file);
			}
			CHECK(buf[i] == c);
		}
		remains -= block;
	}
}

static void file_check(struct file_info *file, int fd)
{
	int open_and_close = 0, link_count = 0;
	char *path = NULL;
	off_t pos;
	struct write_info *w;
	struct dir_entry_info *entry;
	struct stat st;

	/*
	 * In case of power cut emulation testing check only clean files, i.e.
	 * the files which have not been modified since last 'fsync()'.
	 */
	if (args.power_cut_mode && !file->clean)
		return;

	/* Do not check files that have errored */
	if (!fsinfo.nospc_size_ok && file->no_space_error)
		return;
	/* Do not check the same file twice */
	if (file->check_run_no == check_run_no)
		return;
	file->check_run_no = check_run_no;
	if (fd == -1) {
		/* Open file */
		open_and_close = 1;
		assert(file->links);
		path = dir_path(file->links->parent, get_file_name(file));
		v("checking file %s", path);
		fd = open(path, O_RDONLY);
		if (fd == -1) {
			sys_errmsg("cannot open file %s", path);
			CHECK(0);
		}
	} else
		v("checking file %s", get_file_name(file));

	/* Check length */
	pos = lseek(fd, 0, SEEK_END);
	if (pos != file->length) {
		errmsg("file_check failed checking length expected %llu actual %llu\n",
		       (unsigned long long)file->length, (unsigned long long)pos);
		file_info_display(file);
		save_file(fd, file);
	}
	CHECK(pos == file->length);
	/* Check each write */
	pos = 0;
	for (w = file->writes; w; w = w->next) {
		if (w->offset > pos)
			file_check_hole(file, fd, pos, w->offset - pos);
		file_check_data(file, fd, w);
		pos = w->offset + w->size;
	}
	if (file->length > pos)
		file_check_hole(file, fd, pos, file->length - pos);
	CHECK(fstat(fd, &st) == 0);
	CHECK(file->link_count == st.st_nlink);
	if (open_and_close) {
		CHECK(close(fd) == 0);
		free(path);
	}
	entry = file->links;
	while (entry) {
		link_count += 1;
		entry = entry->next_link;
	}
	CHECK(link_count == file->link_count);
}

static char *symlink_path(const char *path, const char *target_pathname)
{
	char *p;
	size_t len, totlen, tarlen;

	if (target_pathname[0] == '/')
		return dup_string(target_pathname);
	p = strrchr(path, '/');
	len = p - path;
	len += 1;
	tarlen = strlen(target_pathname);
	totlen = len + tarlen + 1;
	p = malloc(totlen);
	CHECK(p != NULL);
	strncpy(p, path, len);
	p[len] = '\0';
	strcat(p, target_pathname);
	return p;
}

void symlink_check(const struct symlink_info *symlink)
{
	char *path, buf[8192], *target;
	struct stat st1, st2;
	ssize_t len;
	int ret1, ret2;

	if (args.power_cut_mode)
		return;

	path = dir_path(symlink->entry->parent, symlink->entry->name);

	v("checking symlink %s", path);
	CHECK(lstat(path, &st1) == 0);
	CHECK(S_ISLNK(st1.st_mode));
	CHECK(st1.st_nlink == 1);

	len = readlink(path, buf, 8192);

	CHECK(len > 0 && len < 8192);
	buf[len] = '\0';
	CHECK(strlen(symlink->target_pathname) == len);
	CHECK(strncmp(symlink->target_pathname, buf, len) == 0);

	/* Check symlink points where it should */
	ret1 = stat(path, &st1);
	target = symlink_path(path, symlink->target_pathname);
	ret2 = stat(target, &st2);
	CHECK(ret1 == ret2);
	if (ret1 == 0) {
		CHECK(st1.st_dev == st2.st_dev);
		CHECK(st1.st_ino == st2.st_ino);
	}
	free(target);
	free(path);
}

static int search_comp(const void *pa, const void *pb)
{
	const struct dirent *a = (const struct dirent *) pa;
	const struct dir_entry_info *b = * (const struct dir_entry_info **) pb;
	return strcmp(a->d_name, b->name);
}

static void dir_entry_check(struct dir_entry_info **entry_array,
			size_t number_of_entries,
			struct dirent *ent)
{
	struct dir_entry_info **found;
	struct dir_entry_info *entry;
	size_t sz;

	sz = sizeof(struct dir_entry_info *);
	found = bsearch(ent, entry_array, number_of_entries, sz, search_comp);
	CHECK(found != NULL);
	entry = *found;
	CHECK(!entry->checked);
	entry->checked = 1;
}

static int sort_comp(const void *pa, const void *pb)
{
	const struct dir_entry_info *a = * (const struct dir_entry_info **) pa;
	const struct dir_entry_info *b = * (const struct dir_entry_info **) pb;
	return strcmp(a->name, b->name);
}

static void dir_check(struct dir_info *dir)
{
	struct dir_entry_info *entry, **entry_array, **p;
	size_t sz, n;
	DIR *d;
	struct dirent *ent;
	unsigned int checked = 0;
	char *path;
	int link_count = 2; /* Parent and dot */
	struct stat st;

	path = dir_path(dir->parent, dir->entry->name);

	if (!args.power_cut_mode || dir->clean) {
		v("checking dir %s", path);

		/* Create an array of entries */
		sz = sizeof(struct dir_entry_info *);
		n = dir->number_of_entries;
		entry_array = malloc(sz * n);
		CHECK(entry_array != NULL);

		entry = dir->first;
		p = entry_array;
		while (entry) {
			*p++ = entry;
			entry->checked = 0;
			entry = entry->next;
		}

		/* Sort it by name */
		qsort(entry_array, n, sz, sort_comp);

		/* Go through directory on file system checking entries match */
		d = opendir(path);
		if (!d) {
			sys_errmsg("cannot open directory %s", path);
			CHECK(0);
		}

		for (;;) {
			errno = 0;
			ent = readdir(d);
			if (ent) {
				if (strcmp(".",ent->d_name) != 0 &&
						strcmp("..",ent->d_name) != 0) {
					dir_entry_check(entry_array, n, ent);
					checked += 1;
				}
			} else {
				CHECK(errno == 0);
				break;
			}
		}
		free(entry_array);
		CHECK(closedir(d) == 0);

		CHECK(checked == dir->number_of_entries);
	}

	/* Now check each entry */
	entry = dir->first;
	while (entry) {
		if (entry->type == 'd') {
			dir_check(entry->dir);
			link_count += 1; /* <subdir>/.. */
		} else if (entry->type == 'f')
			file_check(entry->file, -1);
		else if (entry->type == 's')
			symlink_check(entry->symlink);
		else
			CHECK(0);
		entry = entry->next;
	}

	if (!args.power_cut_mode || dir->clean) {
		CHECK(stat(path, &st) == 0);
		if (link_count != st.st_nlink) {
			errmsg("calculated link count %d, FS reports %d for dir %s",
			       link_count, (int)st.st_nlink, path);
			CHECK(0);
		}
	}

	free(path);
}

static void check_deleted_files(void)
{
	struct open_file_info *ofi;

	for (ofi = open_files; ofi; ofi = ofi->next)
		if (!ofi->fdi->file->link_count)
			file_check(ofi->fdi->file, ofi->fdi->fd);
}

static void close_open_files(void)
{
	struct open_file_info *ofi;

	for (ofi = open_files; ofi; ofi = open_files)
		file_close(ofi->fdi);
}

static char *make_name(struct dir_info *dir)
{
	struct dir_entry_info *entry;
	int found;

	do {
		found = 0;
		if (random_no(5) == 1) {
			int i, n = random_no(fsinfo.max_name_len) + 1;

			for (i = 0; i < n; i++)
				random_name_buf[i] = 'a' + random_no(26);
			random_name_buf[i] = '\0';
		} else
			sprintf(random_name_buf, "%u", random_no(1000000));
		for (entry = dir->first; entry; entry = entry->next) {
			if (strcmp(entry->name, random_name_buf) == 0) {
				found = 1;
				break;
			}
		}
	} while (found);

	return random_name_buf;
}

static struct file_info *pick_file(void)
{
	struct dir_info *dir = top_dir;

	for (;;) {
		struct dir_entry_info *entry;
		unsigned int r;

		r = random_no(dir->number_of_entries);
		entry = dir->first;
		while (entry && r) {
			entry = entry->next;
			--r;
		}
		for (;;) {
			if (!entry)
				return NULL;
			if (entry->type == 'f')
				return entry->file;
			if (entry->type == 'd')
				if (entry->dir->number_of_entries != 0)
					break;
			entry = entry->next;
		}
		dir = entry->dir;
	}
}

static struct dir_info *pick_dir(void)
{
	struct dir_info *dir = top_dir;

	if (random_no(40) >= 30)
		return dir;
	for (;;) {
		struct dir_entry_info *entry;
		size_t r;

		r = random_no(dir->number_of_entries);
		entry = dir->first;
		while (entry && r) {
			entry = entry->next;
			--r;
		}
		for (;;) {
			if (!entry)
				break;
			if (entry->type == 'd')
				break;
			entry = entry->next;
		}
		if (!entry) {
			entry = dir->first;
			for (;;) {
				if (!entry)
					break;
				if (entry->type == 'd')
					break;
				entry = entry->next;
			}
		}
		if (!entry)
			return dir;
		dir = entry->dir;
		if (random_no(40) >= 30)
			return dir;
	}
}

static char *pick_rename_name(struct dir_info **parent,
			      struct dir_entry_info **rename_entry, int isdir)
{
	struct dir_info *dir = pick_dir();
	struct dir_entry_info *entry;
	unsigned int r;

	*parent = dir;
	*rename_entry = NULL;

	if (grow || random_no(20) < 10)
		return dup_string(make_name(dir));

	r = random_no(dir->number_of_entries);
	entry = dir->first;
	while (entry && r) {
		entry = entry->next;
		--r;
	}
	if (!entry)
		entry = dir->first;
	if (!entry ||
	    (entry->type == 'd' && entry->dir->number_of_entries != 0))
		return dup_string(make_name(dir));

	if ((isdir && entry->type != 'd') ||
	    (!isdir && entry->type == 'd'))
		return dup_string(make_name(dir));

	*rename_entry = entry;
	return dup_string(entry->name);
}

static int rename_entry(struct dir_entry_info *entry)
{
	struct dir_entry_info *rename_entry = NULL;
	struct dir_info *parent;
	char *path, *to, *name;
	int ret, isdir, retry;
	struct stat st1, st2;

	if (!entry->parent)
		return 0;

	for (retry = 0; retry < 3; retry++) {
		path = dir_path(entry->parent, entry->name);
		isdir = entry->type == 'd' ? 1 : 0;
		name = pick_rename_name(&parent, &rename_entry, isdir);
		to = dir_path(parent, name);
		/*
		 * Check we are not trying to move a directory to a subdirectory
		 * of itself.
		 */
		if (isdir) {
			struct dir_info *p;

			for (p = parent; p; p = p->parent)
				if (p == entry->dir)
					break;
			if (p == entry->dir) {
				free(path);
				free(name);
				free(to);
				path = NULL;
				continue;
			}
		}
		break;
	}

	if (!path)
		return 0;

	if (args.verify_ops)
		CHECK(lstat(path, &st1) == 0);

	if (rename_entry)
		v("moving %s (type %c) inoto %s (type %c)",
		  path, entry->type, to, rename_entry->type);
	else
		v("renaming %s (type %c) to %s", path, entry->type, to);

	ret = rename(path, to);
	if (ret != 0) {
		ret = 0;
		if (errno == ENOSPC)
			full = 1;
		else if (errno !=  EBUSY) {
			pcv("failed to rename %s to %s", path, to);
			ret = -1;
		}
		free(path);
		free(name);
		free(to);
		return ret;
	}

	if (args.verify_ops) {
		CHECK(lstat(to, &st2) == 0);
		CHECK(st1.st_ino == st2.st_ino);
	}

	free(path);
	free(to);

	if (rename_entry && rename_entry->type == entry->type &&
	    rename_entry->target == entry->target) {
		free(name);
		return 0;
	}

	add_dir_entry(parent, entry->type, name, entry->target);
	if (rename_entry)
		remove_dir_entry(rename_entry, 1);
	remove_dir_entry(entry, 0);
	free(name);
	return 0;
}

static size_t str_count(const char *s, char c)
{
	size_t count = 0;
	char cc;

	while ((cc = *s++) != '\0')
		if (cc == c)
			count += 1;
	return count;
}

static char *relative_path(const char *path1, const char *path2)
{
	const char *p1, *p2;
	char *rel;
	size_t up, len, len2, i;

	p1 = path1;
	p2 = path2;
	while (*p1 == *p2 && *p1) {
		p1 += 1;
		p2 += 1;
	}
	len2 = strlen(p2);
	up = str_count(p1, '/');
	if (up == 0 && len2 != 0)
		return dup_string(p2);
	if (up == 0 && len2 == 0) {
		p2 = strrchr(path2, '/');
		return dup_string(p2);
	}
	if (up == 1 && len2 == 0)
		return dup_string(".");
	if (len2 == 0)
		up -= 1;
	len = up * 3 + len2 + 1;
	rel = malloc(len);
	CHECK(rel != NULL);
	rel[0] = '\0';
	if (up) {
		strcat(rel, "..");
		for (i = 1; i < up; i++)
			strcat(rel, "/..");
		if (len2)
			strcat(rel, "/");
	}
	if (len2)
		strcat(rel, p2);
	return rel;
}

static char *pick_symlink_target(const char *symlink_path)
{
	struct dir_info *dir;
	struct dir_entry_info *entry;
	char *path, *rel_path;
	unsigned int r;

	dir = pick_dir();

	if (random_no(100) < 10)
		return dir_path(dir, make_name(dir));

	r = random_no(dir->number_of_entries);
	entry = dir->first;
	while (entry && r) {
		entry = entry->next;
		--r;
	}
	if (!entry)
		entry = dir->first;
	if (!entry)
		return dir_path(dir, make_name(dir));
	path = dir_path(dir, entry->name);
	if (random_no(20) < 10)
		return path;
	rel_path = relative_path(symlink_path, path);
	free(path);
	return rel_path;
}

static void verify_symlink(const char *target, const char *path)
{
	int bytes;
	char buf[PATH_MAX + 1];

	bytes = readlink(path, buf, PATH_MAX);
	CHECK(bytes >= 0);
	CHECK(bytes < PATH_MAX);
	buf[bytes] = '\0';
	CHECK(!strcmp(buf, target));
}

static int symlink_new(struct dir_info *dir, const char *nm)
{
	struct symlink_info *s;
	char *path, *target, *name = dup_string(nm);

	/*
	 * Note, we need to duplicate the input 'name' string because of the
	 * shared random_name_buf.
	 */
	path = dir_path(dir, name);
	target = pick_symlink_target(path);
	v("creating symlink %s ---> %s", path, target);
	if (symlink(target, path) != 0) {
		int ret = 0;

		if (errno == ENOSPC)
			full = 1;
		else if (errno != ENAMETOOLONG) {
			pcv("cannot create symlink %s in directory %s to file %s",
			    path, dir->entry->name, target);
			ret = -1;
		}
		free(target);
		free(name);
		free(path);
		return ret;
	}

	if (args.verify_ops)
		verify_symlink(target, path);

	s = add_dir_entry(dir, 's', name, NULL);
	s->target_pathname = target;

	free(path);
	free(name);
	return 0;
}

static int symlink_remove(struct symlink_info *symlink)
{
	char *path;

	path = dir_path(symlink->entry->parent, symlink->entry->name);
	if (unlink(path) != 0) {
		pcv("cannot unlink symlink %s", path);
		free(path);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(path, &st) == -1);
		CHECK(errno == ENOENT);
	}

	remove_dir_entry(symlink->entry, 1);

	free(path);
	return 0;
}

static int operate_on_dir(struct dir_info *dir);

/* Randomly select something to do with a file */
static int operate_on_file(struct file_info *file)
{
	/* Try to keep at least 10 files open */
	if (open_files_count < 10)
		return file_open(file);
	/* Try to keep about 20 files open */
	if (open_files_count < 20 && random_no(2) == 0)
		return file_open(file);
	/* Try to keep up to 40 files open */
	if (open_files_count < 40 && random_no(20) == 0)
		return file_open(file);

	/* Occasionly truncate */
	if (shrink && random_no(100) == 0)
		return file_truncate_file(file);
	/* Mostly just write */
	if (file_write_file(file) != 0)
		return -1;
	/* Once in a while check it too */
	if (random_no(100) == 1) {
		int fd = -2;

		if (file->links)
			fd = -1;
		else if (file->fds)
			fd = file->fds->fd;
		if (fd != -2) {
			check_run_no += 1;
			file_check(file, fd);
		}
	}
	return 0;
}

/*
 * The operate on entry function is recursive because it calls
 * 'operate_on_dir()' which calls 'operate_on_entry()' again. This variable is
 * used to limit the recursion depth.
 */
static int recursion_depth;

/* Randomly select something to do with a directory entry */
static int operate_on_entry(struct dir_entry_info *entry)
{
	int ret = 0;

	recursion_depth += 1;

	/* 1 time in 1000 rename */
	if (random_no(1000) == 0)
		ret = rename_entry(entry);
	else if (entry->type == 's') {
		symlink_check(entry->symlink);
		/* If shrinking, 1 time in 50, remove a symlink */
		if (shrink && random_no(50) == 0)
			ret = symlink_remove(entry->symlink);
	} else if (entry->type == 'd') {
		/* If shrinking, 1 time in 50, remove a directory */
		if (shrink && random_no(50) == 0)
			ret = dir_remove(entry->dir);
		else if (recursion_depth < 20)
			ret = operate_on_dir(entry->dir);
	} else if (entry->type == 'f') {
		/* If shrinking, 1 time in 10, remove a file */
		if (shrink && random_no(10) == 0)
			ret = file_delete(entry->file);
		/* If not growing, 1 time in 10, unlink a file with links > 1 */
		else if (!grow && entry->file->link_count > 1 &&
			 random_no(10) == 0)
			ret = file_unlink_file(entry->file);
		else
			ret = operate_on_file(entry->file);
	}

	recursion_depth -= 1;
	return ret;
}

/* Synchronize a directory */
static int sync_directory(const char *path)
{
	int fd, ret;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		pcv("cannot open directory %s", path);
		return -1;
	}

	if (random_no(100) >= 50) {
		v("fsyncing dir %s", path);
		ret = fsync(fd);
		if (ret)
			pcv("directory fsync failed for %s", path);
	} else {
		v("fdatasyncing dir %s", path);
		ret = fdatasync(fd);
		if (ret)
			pcv("directory fdatasync failed for %s", path);
	}
	close(fd);
	return ret;
}

/*
 * Randomly select something to do with a directory.
 */
static int operate_on_dir(struct dir_info *dir)
{
	struct dir_entry_info *entry;
	struct file_info *file;
	unsigned int r;
	int ret = 0;

	r = random_no(14);
	if (r == 0 && grow)
		/* When growing, 1 time in 14 create a file */
		ret = file_new(dir, make_name(dir));
	else if (r == 1 && grow)
		/* When growing, 1 time in 14 create a directory */
		ret = dir_new(dir, make_name(dir));
	else if (r == 2 && grow && (file = pick_file()) != NULL)
		/* When growing, 1 time in 14 create a hard link */
		ret = link_new(dir, make_name(dir), file);
	else if (r == 3 && grow && random_no(5) == 0)
		/* When growing, 1 time in 70 create a symbolic link */
		ret = symlink_new(dir, make_name(dir));
	else {
		/* Otherwise randomly select an entry to operate on */
		r = random_no(dir->number_of_entries);
		entry = dir->first;
		while (entry && r) {
			entry = entry->next;
			--r;
		}
		if (entry)
			ret = operate_on_entry(entry);
	}

	if (ret)
		return ret;

	/* Synchronize the directory sometimes */
	if (random_no(100) >= 99) {
		char *path;

		path = dir_path(dir->parent, dir->entry->name);
		ret = sync_directory(path);
		free(path);
		if (!ret)
			dir->clean = 1;
	}

	return ret;
}

/*
 * Randomly select something to do with an open file.
 */
static int operate_on_open_file(struct fd_info *fdi)
{
	int ret = 0;
	unsigned int r = random_no(1000);

	if (shrink && r < 5)
		ret = file_truncate(fdi->file, fdi->fd);
	else if (r < 21)
		file_close(fdi);
	else if (shrink && r < 121 && fdi->file->link_count)
		ret = file_delete(fdi->file);
	else
		ret = file_write(fdi->file, fdi->fd);

	return ret;
}

/*
 * Randomly select an open file and do a random operation on it.
 */
static int operate_on_an_open_file(void)
{
	unsigned int r;
	struct open_file_info *ofi;

	/* When shrinking, close all open files 1 time in 128 */
	if (shrink) {
		static int x = 0;

		x += 1;
		x &= 127;
		if (x == 0) {
			close_open_files();
			return 0;
		}
	}

	/* Close any open files that have errored */
	if (!fsinfo.nospc_size_ok) {
		ofi = open_files;
		while (ofi) {
			if (ofi->fdi->file->no_space_error) {
				struct fd_info *fdi;

				fdi = ofi->fdi;
				ofi = ofi->next;
				file_close(fdi);
			} else
				ofi = ofi->next;
		}
	}

	r = random_no(open_files_count);
	for (ofi = open_files; ofi; ofi = ofi->next, r--)
		if (!r) {
			return operate_on_open_file(ofi->fdi);
		}

	return 0;
}

/*
 * Do a random file-system operation.
 */
static int do_an_operation(void)
{
	/* Half the time operate on already open files */
	if (random_no(100) < 50)
		return operate_on_dir(top_dir);
	else
		return operate_on_an_open_file();
}

/*
 * Fill the tested file-system with random stuff.
 */
static int create_test_data(void)
{
	uint64_t i, n;

	grow = 1;
	shrink = 0;
	full = 0;
	operation_count = 0;
	while (!full) {
		if (do_an_operation())
			return -1;
		operation_count += 1;
	}

	/* Drop to less than 90% full */
	grow = 0;
	shrink = 1;
	n = operation_count / 40;
	while (n--) {
		uint64_t free, total;

		for (i = 0; i < 10; i++)
			if (do_an_operation())
				return -1;

		get_fs_space(&total, &free);
		if ((free * 100) / total >= 10)
			break;
	}

	grow = 0;
	shrink = 0;
	full = 0;
	n = operation_count * 2;
	for (i = 0; i < n; i++)
		if (do_an_operation())
			return -1;

	return 0;
}

/*
 * Do more random operation on the tested file-system.
 */
static int update_test_data(void)
{
	uint64_t i, n;

	grow = 1;
	shrink = 0;
	full = 0;
	while (!full)
		if (do_an_operation())
			return -1;

	/* Drop to less than 50% full */
	grow = 0;
	shrink = 1;
	n = operation_count / 10;
	while (n--) {
		uint64_t free, total;

		for (i = 0; i < 10; i++)
			if (do_an_operation())
				return -1;

		get_fs_space(&total, &free);
		if ((free * 100) / total >= 50)
			break;
	}

	grow = 0;
	shrink = 0;
	full = 0;
	n = operation_count * 2;
	for (i = 0; i < n; i++)
		if (do_an_operation())
			return -1;

	return 0;
}

/*
 * Recursively remove a directory, just like "rm -rf" shell command.
 */
static int rm_minus_rf_dir(const char *dir_name)
{
	int ret;
	DIR *dir;
	struct dirent *dent;
	char buf[PATH_MAX];

	v("removing all files");
	dir = opendir(dir_name);
	CHECK(dir != NULL);
	CHECK(getcwd(buf, PATH_MAX) != NULL);
	CHECK(chdir(dir_name) == 0);

	for (;;) {
		errno = 0;
		dent = readdir(dir);
		if (!dent) {
			CHECK(errno == 0);
			break;
		}

		if (strcmp(dent->d_name, ".") &&
		    strcmp(dent->d_name, "..")) {
			if (dent->d_type == DT_DIR) {
				ret = rm_minus_rf_dir(dent->d_name);
				if (ret) {
					CHECK(closedir(dir) == 0);
					return -1;
				}
			} else {
				ret = unlink(dent->d_name);
				if (ret) {
					pcv("cannot unlink %s", dent->d_name);
					CHECK(closedir(dir) == 0);
					return -1;
				}
			}
		}
	}

	CHECK(chdir(buf) == 0);
	CHECK(closedir(dir) == 0);

	if (args.verify_ops) {
		dir = opendir(dir_name);
		CHECK(dir != NULL);
		do {
			errno = 0;
			dent = readdir(dir);
			if (dent)
				CHECK(!strcmp(dent->d_name, ".") ||
				      !strcmp(dent->d_name, ".."));
		} while (dent);
		CHECK(errno == 0);
		CHECK(closedir(dir) == 0);
	}

	ret = rmdir(dir_name);
	if (ret) {
		pcv("cannot remove directory %s", dir_name);
		return -1;
	}

	if (args.verify_ops) {
		struct stat st;

		CHECK(lstat(dir_name, &st) == -1);
		CHECK(errno == ENOENT);
	}

	return 0;
}

/**
 * Re-mount the test file-system. This function randomly select how to
 * re-mount.
 */
static int remount_tested_fs(void)
{
	int ret;
	unsigned long flags;
	unsigned int rorw1, um, um_ro, um_rorw, rorw2;

	CHECK(chdir("/") == 0);
	v("remounting file-system");

	/* Choose what to do */
	rorw1 = random_no(2);
	um = random_no(2);
	um_ro = random_no(2);
	um_rorw = random_no(2);
	rorw2 = random_no(2);

	if (rorw1 + um + rorw2 == 0)
		um = 1;

	if (rorw1) {
		flags = fsinfo.mount_flags | MS_RDONLY | MS_REMOUNT;
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("cannot remount %s R/O (1)",
			    fsinfo.mount_point);
			return -1;
		}

		flags = fsinfo.mount_flags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("remounted %s R/O (1), but cannot re-mount it R/W",
			    fsinfo.mount_point);
			return -1;
		}
	}

	if (um) {
		if (um_ro) {
			flags = fsinfo.mount_flags | MS_RDONLY | MS_REMOUNT;
			ret = mount(fsinfo.fsdev, fsinfo.mount_point,
				    fsinfo.fstype, flags, fsinfo.mount_opts);
			if (ret) {
				pcv("cannot remount %s R/O (2)",
				    fsinfo.mount_point);
				return -1;
			}
		}

		ret = umount(fsinfo.mount_point);
		if (ret) {
			pcv("cannot unmount %s", fsinfo.mount_point);
			return -1;
		}

		if (!um_rorw) {
			ret = mount(fsinfo.fsdev, fsinfo.mount_point,
				    fsinfo.fstype, fsinfo.mount_flags,
				    fsinfo.mount_opts);
			if (ret) {
				pcv("unmounted %s, but cannot mount it back R/W",
				    fsinfo.mount_point);
				return -1;
			}
		} else {
			ret = mount(fsinfo.fsdev, fsinfo.mount_point,
				    fsinfo.fstype, fsinfo.mount_flags | MS_RDONLY,
				    fsinfo.mount_opts);
			if (ret) {
				pcv("unmounted %s, but cannot mount it back R/O",
				    fsinfo.mount_point);
				return -1;
			}

			flags = fsinfo.mount_flags | MS_REMOUNT;
			flags &= ~((unsigned long)MS_RDONLY);
			ret = mount(fsinfo.fsdev, fsinfo.mount_point,
				    fsinfo.fstype, flags, fsinfo.mount_opts);
			if (ret) {
				pcv("unmounted %s, mounted R/O, but cannot re-mount it R/W",
				     fsinfo.mount_point);
				return -1;
			}
		}
	}

	if (rorw2) {
		flags = fsinfo.mount_flags | MS_RDONLY | MS_REMOUNT;
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("cannot re-mount %s R/O (3)", fsinfo.mount_point);
			return -1;
		}

		flags = fsinfo.mount_flags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("remounted %s R/O (3), but cannot re-mount it back R/W",
			     fsinfo.mount_point);
			return -1;
		}
	}

	CHECK(chdir(fsinfo.mount_point) == 0);
	return 0;
}

static void check_tested_fs(void)
{
	v("checking the file-sytem");
	check_run_no += 1;
	dir_check(top_dir);
	check_deleted_files();
}

/*
 * This is a helper function which just reads whole file. We do this in case of
 * emulated power cuts testing to make sure that unclean files can be at least
 * read.
 */
static void read_whole_file(const char *name)
{
	size_t rd;
	char buf[IO_BUFFER_SIZE];
	int fd;

	fd = open(name, O_RDONLY);
	CHECK(fd != -1);

	do {
		rd = read(fd, buf, IO_BUFFER_SIZE);
		CHECK(rd != -1);
	} while (rd);

	close(fd);
}

/*
 * Recursively walk whole tested file-system and make sure we can read
 * everything. This is done in case of power cuts emulation testing to ensure
 * that everything in the file-system is readable.
 */
static void read_all(const char *dir_name)
{
	DIR *dir;
	struct dirent *dent;
	char buf[PATH_MAX];

	assert(args.power_cut_mode);
	v("reading all files");

	dir = opendir(dir_name);
	if (!dir) {
		errmsg("cannot open %s", dir_name);
		CHECK(0);
	}
	CHECK(getcwd(buf, PATH_MAX) != NULL);
	CHECK(chdir(dir_name) == 0);

	for (;;) {
		errno = 0;
		dent = readdir(dir);
		if (!dent) {
			CHECK(errno == 0);
			break;
		}

		if (!strcmp(dent->d_name, ".") ||
		    !strcmp(dent->d_name, ".."))
			continue;

		if (dent->d_type == DT_DIR)
			read_all(dent->d_name);
		else if (dent->d_type == DT_REG)
			read_whole_file(dent->d_name);
		else if (dent->d_type == DT_LNK) {
			char b[IO_BUFFER_SIZE];

			CHECK(readlink(dent->d_name, b, IO_BUFFER_SIZE) != -1);
		}
	}

	CHECK(chdir(buf) == 0);
	CHECK(closedir(dir) == 0);
}

/*
 * Perform the test. Returns zero on success and -1 on failure.
 */
static int integck(void)
{
	int ret;
	long rpt;

	CHECK(chdir(fsinfo.mount_point) == 0);
	assert(!top_dir);

	/* Create our top directory */
	if (chdir(fsinfo.test_dir) == 0) {
		CHECK(chdir("..") == 0);
		ret = rm_minus_rf_dir(fsinfo.test_dir);
		if (ret)
			return -1;
	}

	v("creating top dir %s", fsinfo.test_dir);
	ret = mkdir(fsinfo.test_dir, 0777);
	if (ret) {
		pcv("cannot create top test directory %s", fsinfo.test_dir);
		return -1;
	}

	ret = sync_directory(fsinfo.test_dir);
	if (ret)
		return -1;

	top_dir = zalloc(sizeof(struct dir_info));
	top_dir->entry = zalloc(sizeof(struct dir_entry_info));
	top_dir->entry->name = dup_string(fsinfo.test_dir);

	ret = create_test_data();
	if (ret)
		return -1;

	if (fsinfo.can_remount) {
		close_open_files();
		ret = remount_tested_fs();
		if (ret)
			return -1;
	} else
		assert(!args.power_cut_mode);

	/* Check everything */
	check_tested_fs();

	for (rpt = 0; args.repeat_cnt == 0 || rpt < args.repeat_cnt; ++rpt) {
		ret = update_test_data();
		if (ret)
			return -1;

		if (fsinfo.can_remount) {
			close_open_files();
			ret = remount_tested_fs();
			if (ret)
				return -1;
		}

		/* Check everything */
		check_tested_fs();
	}

	/* Tidy up by removing everything */
	close_open_files();
	ret = rm_minus_rf_dir(fsinfo.test_dir);
	if (ret)
		return -1;

	return 0;
}

/*
 * This is a helper function for 'get_tested_fs_info()'. It parses file-system
 * mount options string, extracts standard mount options from there, and saves
 * them in the 'fsinfo.mount_flags' variable, and non-standard mount options
 * are saved in the 'fsinfo.mount_opts' variable. The reason for this is that
 * we want to preserve mount options when unmounting the file-system and
 * mounting it again. This is because we cannot pass standard mount optins
 * (like sync, ro, etc) as a string to the 'mount()' function, because it
 * fails. It accepts standard mount options only as flags. And only the
 * FS-specific mount options are accepted in form of a string.
 */
static void parse_mount_options(const char *mount_opts)
{
	char *tmp, *opts, *p;
	const char *opt;

	/*
	 * We are going to use 'strtok()' which modifies the original string,
	 * so duplicate it.
	 */
	tmp = dup_string(mount_opts);
	p = opts = zalloc(strlen(mount_opts) + 1);

	opt = strtok(tmp, ",");
	while (opt) {
		if (!strcmp(opt, "rw"))
			;
		else if (!strcmp(opt, "ro"))
			fsinfo.mount_flags |= MS_RDONLY;
		else if (!strcmp(opt, "dirsync"))
			fsinfo.mount_flags |= MS_DIRSYNC;
		else if (!strcmp(opt, "noatime"))
			fsinfo.mount_flags |= MS_NOATIME;
		else if (!strcmp(opt, "nodiratime"))
			fsinfo.mount_flags |= MS_NODIRATIME;
		else if (!strcmp(opt, "noexec"))
			fsinfo.mount_flags |= MS_NOEXEC;
		else if (!strcmp(opt, "nosuid"))
			fsinfo.mount_flags |= MS_NOSUID;
		else if (!strcmp(opt, "relatime"))
			fsinfo.mount_flags |= MS_RELATIME;
		else if (!strcmp(opt, "sync"))
			fsinfo.mount_flags |= MS_SYNCHRONOUS;
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
	fsinfo.mount_opts = opts;
}

/*
 * This is a helper function which searches for the tested file-system mount
 * description.
 */
static struct mntent *get_tested_fs_mntent(void)
{
	const char *mp;
	struct mntent *mntent;
        FILE *f;

	mp = "/proc/mounts";
        f = fopen(mp, "rb");
        if (!f) {
		mp = "/etc/mtab";
                f = fopen(mp, "rb");
	}
	CHECK(f != NULL);

        while ((mntent = getmntent(f)) != NULL)
		if (!strcmp(mntent->mnt_dir, fsinfo.mount_point))
			break;
        CHECK(fclose(f) == 0);
	return mntent;
}

/*
 * Fill 'fsinfo' with information about the tested file-system.
 */
static void get_tested_fs_info(void)
{
	struct statfs fs_info;
	struct mntent *mntent;
	uint64_t z;
	char *p;
	unsigned int pid;

	/* Remove trailing '/' symbols from the mount point */
	p = dup_string(args.mount_point);
	fsinfo.mount_point = p;
	p += strlen(p);
	while (*--p == '/');
	*(p + 1) = '\0';

	CHECK(statfs(fsinfo.mount_point, &fs_info) == 0);
	fsinfo.max_name_len = fs_info.f_namelen;

	mntent = get_tested_fs_mntent();
	if (!mntent) {
		errmsg("cannot find file-system info");
		CHECK(0);
	}

	fsinfo.fstype = dup_string(mntent->mnt_type);
	fsinfo.fsdev = dup_string(mntent->mnt_fsname);
	parse_mount_options(mntent->mnt_opts);

	/* Get memory page size for 'mmap()' */
	fsinfo.page_size = sysconf(_SC_PAGE_SIZE);
	CHECK(fsinfo.page_size > 0);

	/*
	 * JFFS2 does not support shared writable mmap and it may report
	 * incorrect file size after "no space" errors.
	 */
	if (strcmp(fsinfo.fstype, "jffs2") == 0) {
		fsinfo.nospc_size_ok = 0;
		fsinfo.can_mmap = 0;
	}

	get_fs_space(NULL, &z);
	for (; z >= 10; z /= 10)
		fsinfo.log10_initial_free += 1;

	/* Pick the test directory name */
	p = malloc(sizeof(TEST_DIR_PATTERN) + 20);
	CHECK(p != NULL);
	pid = getpid();
	CHECK(sprintf(p, "integck_test_dir_%u", pid) > 0);
	fsinfo.test_dir = p;

	normsg("pid %u, testing \"%s\" at \"%s\"",
	       pid, fsinfo.fstype, fsinfo.mount_point);
}

static const char doc[] = PROGRAM_NAME " version " PROGRAM_VERSION
" - a stress test which checks the file-system integrity.\n"
"\n"
"The test creates a directory named \"integck_test_dir_<pid>\", where where\n"
"<pid> is the process id. Then it randomly creates and deletes files,\n"
"directories, symlinks, and hardlinks, randomly writes and truncate files,\n"
"sometimes makes holes in files, sometimes fsync()'s them. Then it un-mounts and\n"
"re-mounts the tested file-system and checks the contents - everything (files,\n"
"directories, etc) should be there and the contents of the files should be\n"
"correct. This is repeated a number of times (set with -n, default 1).\n"
"\n"
"By default the test does not verify file-system modifications and assumes they\n"
"are done correctly if the file-system returns success. However, the -e flag\n"
"enables additional verifications and the test verifies all the file-system\n"
"operations it performs.\n"
"\n"
"This test is also able to perform power cut testing. The underlying file-system\n"
"or the device driver should be able to emulate power-cuts, by switching to R/O\n"
"mode at random moments. And the file-system should return EROFS (read-only\n"
"file-system error) for all operations which modify it. In this case this test\n"
"program re-mounts the file-system and checks that all files and directories\n"
"which have been successfully synchronized before the power cut. And the test\n"
"continues forever.\n";

static const char optionsstr[] =
"-n, --repeat=<count> repeat count, default is 1; zero value - repeat forever\n"
"-p, --power-cut      power cut testing mode (-n parameter is ignored and the\n"
"                     test continues forever)\n"
"-e, --verify-ops     verify all operations, e.g., every time a file is written\n"
"                     to, read the data back and verify it, every time a\n"
"                     directory is created, check that it exists, etc\n"
"-v, --verbose        be verbose\n"
"-m, --reattach=<mtd> re-attach mtd device number <mtd> (only if doing UBIFS power\n"
"                     cut emulation testing)\n"
"-h, -?, --help       print help message\n"
"-V, --version        print program version\n";

static const struct option long_options[] = {
	{ .name = "repeat",     .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "power-cut",  .has_arg = 0, .flag = NULL, .val = 'p' },
	{ .name = "verify-ops", .has_arg = 0, .flag = NULL, .val = 'e' },
	{ .name = "reattach",   .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "verbose",    .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "help",       .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",    .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

/*
 * Parse and validate input command-line options. Returns zero on success and
 * -1 on error.
 */
static int parse_opts(int argc, char * const argv[])
{
	struct stat st;

	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "n:pm:evVh?", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'n':
			args.repeat_cnt = simple_strtoul(optarg, &error);
			if (error || args.repeat_cnt < 0)
				return errmsg("bad repeat count: \"%s\"", optarg);
			break;
		case 'p':
			args.power_cut_mode = 1;
			break;
		case 'm':
			args.mtdn = simple_strtoul(optarg, &error);
			if (error || args.mtdn < 0)
				return errmsg("bad mtd device number: \"%s\"", optarg);
			args.reattach = 1;
			break;
		case 'e':
			args.verify_ops = 1;
			break;
		case 'v':
			args.verbose = 1;
			break;
		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case 'h':
		case '?':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);
		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		return errmsg("test file-system was not specified (use -h for help)");
	else if (optind != argc - 1)
		return errmsg("more then one test file-system specified (use -h for help)");

	if (args.reattach && !args.power_cut_mode)
		return errmsg("-m makes sense only together with -e");

	if (args.power_cut_mode)
		/* Repeat forever if we are in power cut testing mode */
		args.repeat_cnt = 0;

	args.mount_point = argv[optind];

	if (chdir(args.mount_point) != 0 || lstat(args.mount_point, &st) != 0)
		return errmsg("invalid test file system mount directory: %s",
			      args.mount_point);

	return 0;
}

/*
 * Free all the in-memory information about the tested file-system contents
 * starting from sub-directory 'dir'.
 */
static void free_fs_info(struct dir_info *dir)
{
	struct dir_entry_info *entry;

	/* Now check each entry */
	while (dir->first) {
		entry = dir->first;
		if (entry->type == 'd') {
			struct dir_info *d = entry->dir;

			remove_dir_entry(entry, 0);
			free_fs_info(d);
			free(d);
		} else if (entry->type == 'f') {
			remove_dir_entry(entry, 1);
		} else if (entry->type == 's') {
			remove_dir_entry(entry, 1);
		} else
			assert(0);
	}
}

/*
 * Detach the MTD device from UBI and attach it back. This function is used
 * whed performing emulated power cut testing andthe power cuts are amulated by
 * UBI, not by UBIFS. In this case, to recover from the emulated power cut we
 * have to unmount UBIFS and re-attach the MTD device.
 */
static int reattach(void)
{
	int err = 0;
	libubi_t libubi;
	struct ubi_attach_request req;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			return errmsg("UBI is not present in the system");
		return sys_errmsg("cannot open libubi");
	}

	err = ubi_detach_mtd(libubi, "/dev/ubi_ctrl", args.mtdn);
	if (err) {
		sys_errmsg("cannot detach mtd%d", args.mtdn);
		goto out;
	}

	req.dev_num = UBI_DEV_NUM_AUTO;
	req.mtd_num = args.mtdn;
	req.vid_hdr_offset = 0;
	req.mtd_dev_node = NULL;

	err = ubi_attach(libubi, "/dev/ubi_ctrl", &req);
	if (err)
		sys_errmsg("cannot attach mtd%d", args.mtdn);

out:
	libubi_close(libubi);
	return err;
}

/*
 * Recover the tested file-system from an emulated power cut failure by
 * unmounting it and mounting it again.
 */
static int recover_tested_fs(void)
{
	int ret;
	unsigned long flags;
	unsigned int  um_rorw, rorw2;
	struct mntent *mntent;

	CHECK(chdir("/") == 0);

	/* Choose what to do */
	um_rorw = random_no(2);
	rorw2 = random_no(2);

	/*
	 * At this point we do not know for sure whether the tested FS is
	 * mounted, because the emulated power cut error could have happened
	 * while mounting in 'remount_tested_fs()'.
	 */
	mntent = get_tested_fs_mntent();
	if (mntent)
		CHECK(umount(fsinfo.mount_point) != -1);

	if (args.reattach)
		CHECK(reattach() == 0);

	if (!um_rorw) {
		ret = mount(fsinfo.fsdev, fsinfo.mount_point,
			    fsinfo.fstype, fsinfo.mount_flags,
			    fsinfo.mount_opts);
		if (ret) {
			pcv("unmounted %s, but cannot mount it back R/W",
			    fsinfo.mount_point);
			return -1;
		}
	} else {
		ret = mount(fsinfo.fsdev, fsinfo.mount_point,
			    fsinfo.fstype, fsinfo.mount_flags | MS_RDONLY,
			    fsinfo.mount_opts);
		if (ret) {
			pcv("unmounted %s, but cannot mount it back R/O",
			    fsinfo.mount_point);
			return -1;
		}

		flags = fsinfo.mount_flags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		ret = mount(fsinfo.fsdev, fsinfo.mount_point,
			    fsinfo.fstype, flags, fsinfo.mount_opts);
		if (ret) {
			pcv("unmounted %s, mounted R/O, but cannot re-mount it R/W",
			     fsinfo.mount_point);
			return -1;
		}
	}

	if (rorw2) {
		flags = fsinfo.mount_flags | MS_RDONLY | MS_REMOUNT;
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("cannot re-mount %s R/O", fsinfo.mount_point);
			return -1;
		}

		flags = fsinfo.mount_flags | MS_REMOUNT;
		flags &= ~((unsigned long)MS_RDONLY);
		ret = mount(fsinfo.fsdev, fsinfo.mount_point, fsinfo.fstype,
			    flags, fsinfo.mount_opts);
		if (ret) {
			pcv("remounted %s R/O, but cannot re-mount it back R/W",
			     fsinfo.mount_point);
			return -1;
		}
	}

	return 0;
}

static void free_test_data(void)
{
	if (top_dir) {
		free_fs_info(top_dir);
		free(top_dir->entry->name);
		free(top_dir->entry);
		free(top_dir);
		top_dir = NULL;
	}
}

int main(int argc, char *argv[])
{
	int ret;
	long rpt;

	ret = parse_opts(argc, argv);
	if (ret)
		return EXIT_FAILURE;

	get_tested_fs_info();

	/* Seed the random generator with out PID */
	random_seed = getpid();

	random_name_buf = malloc(fsinfo.max_name_len + 1);
	CHECK(random_name_buf != NULL);

	/* Refuse the file-system if it is mounted R/O */
	if (fsinfo.mount_flags & MS_RDONLY) {
		ret = -1;
		errmsg("the file-system is mounted read-only");
		goto out_free;
	}

	/* Check whether we can re-mount the tested FS  */
	do {
		ret = recover_tested_fs();
	} while (ret && args.power_cut_mode && errno == EROFS);

	if (!ret) {
		fsinfo.can_remount = 1;
	} else {
		warnmsg("file-system %s cannot be unmounted (%s)",
			fsinfo.mount_point, strerror(errno));
		if (args.power_cut_mode) {
			/*
			 * When testing emulated power cuts we have to be able
			 * to re-mount the file-system to clean the EROFS
			 * state.
			 */
			errmsg("power cut mode requers unmountable FS");
			goto out_free;
		}
	}

	/* Do the actual test */
	for (rpt = 0; ; rpt++) {
		ret = integck();

		/*
		 * Iterate forever only in case of power-cut emulation testing.
		 */
		if (!args.power_cut_mode) {
			CHECK(!ret);
			break;
		}

		CHECK(ret);
		CHECK(errno == EROFS || errno == EIO);

		close_open_files();

		do {
			ret = recover_tested_fs();
			if (ret) {
				CHECK(errno == EROFS);
				rpt += 1;
			}
			/*
			 * Mount may also fail due to an emulated power cut
			 * while mounting - keep re-starting.
			 */
		} while (ret);

		CHECK(chdir(fsinfo.mount_point) == 0);

		/* Make sure everything is readable after an emulated power cut */
		if (top_dir) {
			/* Check the clean data */
			check_tested_fs();
			read_all(fsinfo.test_dir);
		}

		free_test_data();

		/*
		 * The file-system became read-only and we are in power cut
		 * testing mode. Re-mount the file-system and re-start the
		 * test.
		 */
		if (args.verbose)
			normsg("re-mount the FS and re-start - count %ld", rpt);

	}

	close_open_files();
	free_test_data();

out_free:
	free(random_name_buf);
	free(fsinfo.mount_opts);
	free(fsinfo.mount_point);
	free(fsinfo.fstype);
	free(fsinfo.fsdev);
	free(fsinfo.test_dir);
	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
