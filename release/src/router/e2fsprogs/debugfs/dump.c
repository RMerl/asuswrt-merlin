/*
 * dump.c --- dump the contents of an inode out to a file
 *
 * Copyright (C) 1994 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for O_LARGEFILE */
#endif

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/*
 * The mode_xlate function translates a linux mode into a native-OS mode_t.
 */
static struct {
	__u16 lmask;
	mode_t mask;
} mode_table[] = {
	{ LINUX_S_IRUSR, S_IRUSR },
	{ LINUX_S_IWUSR, S_IWUSR },
	{ LINUX_S_IXUSR, S_IXUSR },
	{ LINUX_S_IRGRP, S_IRGRP },
	{ LINUX_S_IWGRP, S_IWGRP },
	{ LINUX_S_IXGRP, S_IXGRP },
	{ LINUX_S_IROTH, S_IROTH },
	{ LINUX_S_IWOTH, S_IWOTH },
	{ LINUX_S_IXOTH, S_IXOTH },
	{ 0, 0 }
};

static mode_t mode_xlate(__u16 lmode)
{
	mode_t	mode = 0;
	int	i;

	for (i=0; mode_table[i].lmask; i++) {
		if (lmode & mode_table[i].lmask)
			mode |= mode_table[i].mask;
	}
	return mode;
}

static void fix_perms(const char *cmd, const struct ext2_inode *inode,
		      int fd, const char *name)
{
	struct utimbuf ut;
	int i;

	if (fd != -1)
		i = fchmod(fd, mode_xlate(inode->i_mode));
	else
		i = chmod(name, mode_xlate(inode->i_mode));
	if (i == -1)
		com_err(cmd, errno, "while setting permissions of %s", name);

#ifndef HAVE_FCHOWN
	i = chown(name, inode->i_uid, inode->i_gid);
#else
	if (fd != -1)
		i = fchown(fd, inode->i_uid, inode->i_gid);
	else
		i = chown(name, inode->i_uid, inode->i_gid);
#endif
	if (i == -1)
		com_err(cmd, errno, "while changing ownership of %s", name);

	if (fd != -1)
		close(fd);

	ut.actime = inode->i_atime;
	ut.modtime = inode->i_mtime;
	if (utime(name, &ut) == -1)
		com_err(cmd, errno, "while setting times of %s", name);
}

static void dump_file(const char *cmdname, ext2_ino_t ino, int fd,
		      int preserve, char *outname)
{
	errcode_t retval;
	struct ext2_inode	inode;
	char		*buf = 0;
	ext2_file_t	e2_file;
	int		nbytes;
	unsigned int	got, blocksize = current_fs->blocksize;

	if (debugfs_read_inode(ino, &inode, cmdname))
		return;

	retval = ext2fs_file_open(current_fs, ino, 0, &e2_file);
	if (retval) {
		com_err(cmdname, retval, "while opening ext2 file");
		return;
	}
	retval = ext2fs_get_mem(blocksize, &buf);
	if (retval) {
		com_err(cmdname, retval, "while allocating memory");
		return;
	}
	while (1) {
		retval = ext2fs_file_read(e2_file, buf, blocksize, &got);
		if (retval)
			com_err(cmdname, retval, "while reading ext2 file");
		if (got == 0)
			break;
		nbytes = write(fd, buf, got);
		if ((unsigned) nbytes != got)
			com_err(cmdname, errno, "while writing file");
	}
	if (buf)
		ext2fs_free_mem(&buf);
	retval = ext2fs_file_close(e2_file);
	if (retval) {
		com_err(cmdname, retval, "while closing ext2 file");
		return;
	}

	if (preserve)
		fix_perms("dump_file", &inode, fd, outname);
	else if (fd != 1)
		close(fd);

	return;
}

void do_dump(int argc, char **argv)
{
	ext2_ino_t	inode;
	int		fd;
	int		c;
	int		preserve = 0;
	char		*in_fn, *out_fn;

	reset_getopt();
	while ((c = getopt (argc, argv, "p")) != EOF) {
		switch (c) {
		case 'p':
			preserve++;
			break;
		default:
		print_usage:
			com_err(argv[0], 0, "Usage: dump_inode [-p] "
				"<file> <output_file>");
			return;
		}
	}
	if (optind != argc-2)
		goto print_usage;

	if (check_fs_open(argv[0]))
		return;

	in_fn = argv[optind];
	out_fn = argv[optind+1];

	inode = string_to_inode(in_fn);
	if (!inode)
		return;

	fd = open(out_fn, O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE, 0666);
	if (fd < 0) {
		com_err(argv[0], errno, "while opening %s for dump_inode",
			out_fn);
		return;
	}

	dump_file(argv[0], inode, fd, preserve, out_fn);

	return;
}

static void rdump_symlink(ext2_ino_t ino, struct ext2_inode *inode,
			  const char *fullname)
{
	ext2_file_t e2_file;
	char *buf;
	errcode_t retval;

	buf = malloc(inode->i_size + 1);
	if (!buf) {
		com_err("rdump", errno, "while allocating for symlink");
		goto errout;
	}

	/* Apparently, this is the right way to detect and handle fast
	 * symlinks; see do_stat() in debugfs.c. */
	if (inode->i_blocks == 0)
		strcpy(buf, (char *) inode->i_block);
	else {
		unsigned bytes = inode->i_size;
		char *p = buf;
		retval = ext2fs_file_open(current_fs, ino, 0, &e2_file);
		if (retval) {
			com_err("rdump", retval, "while opening symlink");
			goto errout;
		}
		for (;;) {
			unsigned int got;
			retval = ext2fs_file_read(e2_file, p, bytes, &got);
			if (retval) {
				com_err("rdump", retval, "while reading symlink");
				goto errout;
			}
			bytes -= got;
			p += got;
			if (got == 0 || bytes == 0)
				break;
		}
		buf[inode->i_size] = 0;
		retval = ext2fs_file_close(e2_file);
		if (retval)
			com_err("rdump", retval, "while closing symlink");
	}

	if (symlink(buf, fullname) == -1) {
		com_err("rdump", errno, "while creating symlink %s -> %s", buf, fullname);
		goto errout;
	}

errout:
	free(buf);
}

static int rdump_dirent(struct ext2_dir_entry *, int, int, char *, void *);

static void rdump_inode(ext2_ino_t ino, struct ext2_inode *inode,
			const char *name, const char *dumproot)
{
	char *fullname;

	/* There are more efficient ways to do this, but this method
	 * requires only minimal debugging. */
	fullname = malloc(strlen(dumproot) + strlen(name) + 2);
	if (!fullname) {
		com_err("rdump", errno, "while allocating memory");
		return;
	}
	sprintf(fullname, "%s/%s", dumproot, name);

	if (LINUX_S_ISLNK(inode->i_mode))
		rdump_symlink(ino, inode, fullname);
	else if (LINUX_S_ISREG(inode->i_mode)) {
		int fd;
		fd = open(fullname, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, S_IRWXU);
		if (fd == -1) {
			com_err("rdump", errno, "while dumping %s", fullname);
			goto errout;
		}
		dump_file("rdump", ino, fd, 1, fullname);
	}
	else if (LINUX_S_ISDIR(inode->i_mode) && strcmp(name, ".") && strcmp(name, "..")) {
		errcode_t retval;

		/* Create the directory with 0700 permissions, because we
		 * expect to have to create entries it.  Then fix its perms
		 * once we've done the traversal. */
		if (mkdir(fullname, S_IRWXU) == -1) {
			com_err("rdump", errno, "while making directory %s", fullname);
			goto errout;
		}

		retval = ext2fs_dir_iterate(current_fs, ino, 0, 0,
					    rdump_dirent, (void *) fullname);
		if (retval)
			com_err("rdump", retval, "while dumping %s", fullname);

		fix_perms("rdump", inode, -1, fullname);
	}
	/* else do nothing (don't dump device files, sockets, fifos, etc.) */

errout:
	free(fullname);
}

static int rdump_dirent(struct ext2_dir_entry *dirent,
			int offset EXT2FS_ATTR((unused)),
			int blocksize EXT2FS_ATTR((unused)),
			char *buf EXT2FS_ATTR((unused)), void *private)
{
	char name[EXT2_NAME_LEN + 1];
	int thislen;
	const char *dumproot = private;
	struct ext2_inode inode;

	thislen = dirent->name_len & 0xFF;
	strncpy(name, dirent->name, thislen);
	name[thislen] = 0;

	if (debugfs_read_inode(dirent->inode, &inode, name))
		return 0;

	rdump_inode(dirent->inode, &inode, name, dumproot);

	return 0;
}

void do_rdump(int argc, char **argv)
{
	ext2_ino_t ino;
	struct ext2_inode inode;
	struct stat st;
	int i;
	char *p;

	if (common_args_process(argc, argv, 3, 3, "rdump",
				"<directory> <native directory>", 0))
		return;

	ino = string_to_inode(argv[1]);
	if (!ino)
		return;

	/* Ensure ARGV[2] is a directory. */
	i = stat(argv[2], &st);
	if (i == -1) {
		com_err("rdump", errno, "while statting %s", argv[2]);
		return;
	}
	if (!S_ISDIR(st.st_mode)) {
		com_err("rdump", 0, "%s is not a directory", argv[2]);
		return;
	}

	if (debugfs_read_inode(ino, &inode, argv[1]))
		return;

	p = strrchr(argv[1], '/');
	if (p)
		p++;
	else
		p = argv[1];

	rdump_inode(ino, &inode, p, argv[2]);
}

void do_cat(int argc, char **argv)
{
	ext2_ino_t	inode;

	if (common_inode_args_process(argc, argv, &inode, 0))
		return;

	fflush(stdout);
	fflush(stderr);
	dump_file(argv[0], inode, 1, 0, argv[2]);

	return;
}

