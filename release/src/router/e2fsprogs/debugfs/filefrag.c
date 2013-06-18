/*
 * filefrag.c --- display the fragmentation information for a file
 *
 * Copyright (C) 2011 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

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

#define VERBOSE_OPT	0x0001
#define DIR_OPT		0x0002
#define RECURSIVE_OPT	0x0004

struct dir_list {
	char		*name;
	ext2_ino_t	ino;
	struct dir_list	*next;
};

struct filefrag_struct {
	FILE		*f;
	const char	*name;
	const char	*dir_name;
	int		options;
	int		logical_width;
	int		physical_width;
	int		ext;
	int		cont_ext;
	e2_blkcnt_t	num;
	e2_blkcnt_t	logical_start;
	blk64_t		physical_start;
	blk64_t		expected;
	struct dir_list *dir_list, *dir_last;
};

static int int_log10(unsigned long long arg)
{
	int     l = 0;

	arg = arg / 10;
	while (arg) {
		l++;
		arg = arg / 10;
	}
	return l;
}

static void print_header(struct filefrag_struct *fs)
{
	if (fs->options & VERBOSE_OPT) {
		fprintf(fs->f, "%4s %*s %*s %*s %*s\n", "ext",
			fs->logical_width, "logical", fs->physical_width,
			"physical", fs->physical_width, "expected",
			fs->logical_width, "length");
	}
}

static void report_filefrag(struct filefrag_struct *fs)
{
	if (fs->num == 0)
		return;
	if (fs->options & VERBOSE_OPT) {
		if (fs->expected)
			fprintf(fs->f, "%4d %*lu %*llu %*llu %*lu\n", fs->ext,
				fs->logical_width,
				(unsigned long) fs->logical_start,
				fs->physical_width, fs->physical_start,
				fs->physical_width, fs->expected,
				fs->logical_width, (unsigned long) fs->num);
		else
			fprintf(fs->f, "%4d %*lu %*llu %*s %*lu\n", fs->ext,
				fs->logical_width,
				(unsigned long) fs->logical_start,
				fs->physical_width, fs->physical_start,
				fs->physical_width, "",
				fs->logical_width, (unsigned long) fs->num);
	}
	fs->ext++;
}

static int filefrag_blocks_proc(ext2_filsys ext4_fs EXT2FS_ATTR((unused)),
				blk64_t *blocknr, e2_blkcnt_t blockcnt,
				blk64_t ref_block EXT2FS_ATTR((unused)),
				int ref_offset EXT2FS_ATTR((unused)),
				void *private)
{
	struct filefrag_struct *fs = private;

	if (blockcnt < 0 || *blocknr == 0)
		return 0;

	if ((fs->num == 0) || (blockcnt != fs->logical_start + fs->num) ||
	    (*blocknr != fs->physical_start + fs->num)) {
		report_filefrag(fs);
		if (blockcnt == fs->logical_start + fs->num)
			fs->expected = fs->physical_start + fs->num;
		else
			fs->expected = 0;
		fs->logical_start = blockcnt;
		fs->physical_start = *blocknr;
		fs->num = 1;
		fs->cont_ext++;
	} else
		fs->num++;
	return 0;
}

static void filefrag(ext2_ino_t ino, struct ext2_inode *inode,
		     struct filefrag_struct *fs)
{
	errcode_t	retval;
	int		blocksize = current_fs->blocksize;

	fs->logical_width = int_log10((EXT2_I_SIZE(inode) + blocksize - 1) /
				      blocksize) + 1;
	if (fs->logical_width < 7)
		fs->logical_width = 7;
	fs->ext = 0;
	fs->cont_ext = 0;
	fs->logical_start = 0;
	fs->physical_start = 0;
	fs->num = 0;

	if (fs->options & VERBOSE_OPT) {
		blk64_t num_blocks = ext2fs_inode_i_blocks(current_fs, inode);

		if (!(current_fs->super->s_feature_ro_compat &
		     EXT4_FEATURE_RO_COMPAT_HUGE_FILE) ||
		    !(inode->i_flags & EXT4_HUGE_FILE_FL))
			num_blocks /= current_fs->blocksize / 512;

		fprintf(fs->f, "\n%s has %llu block(s), i_size is %llu\n",
			fs->name, num_blocks, EXT2_I_SIZE(inode));
	}
	print_header(fs);
	retval = ext2fs_block_iterate3(current_fs, ino,
				       BLOCK_FLAG_READ_ONLY, NULL,
				       filefrag_blocks_proc, fs);
	if (retval)
		com_err("ext2fs_block_iterate3", retval, 0);

	report_filefrag(fs);
	fprintf(fs->f, "%s: %d contiguous extents%s\n", fs->name, fs->ext,
		LINUX_S_ISDIR(inode->i_mode) ? " (dir)" : "");
}

static int filefrag_dir_proc(ext2_ino_t dir EXT2FS_ATTR((unused)),
			     int	entry,
			     struct ext2_dir_entry *dirent,
			     int	offset EXT2FS_ATTR((unused)),
			     int	blocksize EXT2FS_ATTR((unused)),
			     char	*buf EXT2FS_ATTR((unused)),
			     void	*private)
{
	struct filefrag_struct *fs = private;
	struct ext2_inode	inode;
	ext2_ino_t		ino;
	char			name[EXT2_NAME_LEN + 1];
	char			*cp;
	int			thislen;

	if (entry == DIRENT_DELETED_FILE)
		return 0;

	thislen = dirent->name_len & 0xFF;
	strncpy(name, dirent->name, thislen);
	name[thislen] = '\0';
	ino = dirent->inode;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return 0;

	cp = malloc(strlen(fs->dir_name) + strlen(name) + 2);
	if (!cp) {
		fprintf(stderr, "Couldn't allocate memory for %s/%s\n",
			fs->dir_name, name);
		return 0;
	}

	sprintf(cp, "%s/%s", fs->dir_name, name);
	fs->name = cp;

	if (debugfs_read_inode(ino, &inode, fs->name))
		goto errout;

	filefrag(ino, &inode, fs);

	if ((fs->options & RECURSIVE_OPT) && LINUX_S_ISDIR(inode.i_mode)) {
		struct dir_list *p;

		p = malloc(sizeof(struct dir_list));
		if (!p) {
			fprintf(stderr, "Couldn't allocate dir_list for %s\n",
				fs->name);
			goto errout;
		}
		memset(p, 0, sizeof(struct dir_list));
		p->name = cp;
		p->ino = ino;
		if (fs->dir_last)
			fs->dir_last->next = p;
		else
			fs->dir_list = p;
		fs->dir_last = p;
		return 0;
	}
errout:
	free(cp);
	fs->name = 0;
	return 0;
}


static void dir_iterate(ext2_ino_t ino, struct filefrag_struct *fs)
{
	errcode_t	retval;
	struct dir_list	*p = NULL;

	fs->dir_name = fs->name;

	while (1) {
		retval = ext2fs_dir_iterate2(current_fs, ino, 0,
					     0, filefrag_dir_proc, fs);
		if (retval)
			com_err("ext2fs_dir_iterate2", retval, 0);
		if (p) {
			free(p->name);
			fs->dir_list = p->next;
			if (!fs->dir_list)
				fs->dir_last = 0;
			free(p);
		}
		p = fs->dir_list;
		if (!p)
			break;
		ino = p->ino;
		fs->dir_name = p->name;
	}
}

void do_filefrag(int argc, char *argv[])
{
	struct filefrag_struct fs;
	struct ext2_inode inode;
	ext2_ino_t	ino;
	int		c;

	memset(&fs, 0, sizeof(fs));
	if (check_fs_open(argv[0]))
		return;

	reset_getopt();
	while ((c = getopt(argc, argv, "dvr")) != EOF) {
		switch (c) {
		case 'd':
			fs.options |= DIR_OPT;
			break;
		case 'v':
			fs.options |= VERBOSE_OPT;
			break;
		case 'r':
			fs.options |= RECURSIVE_OPT;
			break;
		default:
			goto print_usage;
		}
	}

	if (argc > optind+1) {
	print_usage:
		com_err(0, 0, "Usage: filefrag [-dv] file");
		return;
	}

	if (argc == optind) {
		ino = cwd;
		fs.name = ".";
	} else {
		ino = string_to_inode(argv[optind]);
		fs.name = argv[optind];
	}
	if (!ino)
		return;

	if (debugfs_read_inode(ino, &inode, argv[0]))
		return;

	fs.f = open_pager();
	fs.physical_width = int_log10(ext2fs_blocks_count(current_fs->super));
	fs.physical_width++;
	if (fs.physical_width < 8)
		fs.physical_width = 8;

	if (!LINUX_S_ISDIR(inode.i_mode) || (fs.options & DIR_OPT))
		filefrag(ino, &inode, &fs);
	else
		dir_iterate(ino, &fs);

	fprintf(fs.f, "\n");
	close_pager(fs.f);

	return;
}
