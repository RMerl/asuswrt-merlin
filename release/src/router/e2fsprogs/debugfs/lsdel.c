/*
 * lsdel.c --- routines to try to help a user recover a deleted file.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
 * Theodore Ts'o.  This file may be redistributed under the terms of
 * the GNU Public License.
 */

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

#include "debugfs.h"

struct deleted_info {
	ext2_ino_t	ino;
	unsigned short	mode;
	__u32		uid;
	__u64		size;
	time_t		dtime;
	e2_blkcnt_t	num_blocks;
	e2_blkcnt_t	free_blocks;
};

struct lsdel_struct {
	ext2_ino_t		inode;
	e2_blkcnt_t		num_blocks;
	e2_blkcnt_t		free_blocks;
	e2_blkcnt_t		bad_blocks;
};

static int deleted_info_compare(const void *a, const void *b)
{
	const struct deleted_info *arg1, *arg2;

	arg1 = (const struct deleted_info *) a;
	arg2 = (const struct deleted_info *) b;

	return arg1->dtime - arg2->dtime;
}

static int lsdel_proc(ext2_filsys fs,
		      blk_t	*block_nr,
		      e2_blkcnt_t blockcnt EXT2FS_ATTR((unused)),
		      blk_t ref_block EXT2FS_ATTR((unused)),
		      int ref_offset EXT2FS_ATTR((unused)),
		      void *private)
{
	struct lsdel_struct *lsd = (struct lsdel_struct *) private;

	lsd->num_blocks++;

	if (*block_nr < fs->super->s_first_data_block ||
	    *block_nr >= fs->super->s_blocks_count) {
		lsd->bad_blocks++;
		return BLOCK_ABORT;
	}

	if (!ext2fs_test_block_bitmap(fs->block_map,*block_nr))
		lsd->free_blocks++;

	return 0;
}

void do_lsdel(int argc, char **argv)
{
	struct lsdel_struct 	lsd;
	struct deleted_info	*delarray;
	int			num_delarray, max_delarray;
	ext2_inode_scan		scan = 0;
	ext2_ino_t		ino;
	struct ext2_inode	inode;
	errcode_t		retval;
	char			*block_buf;
	int			i;
 	long			secs = 0;
 	char			*tmp;
	time_t			now;
	FILE			*out;

	if (common_args_process(argc, argv, 1, 2, "ls_deleted_inodes",
				"[secs]", 0))
		return;

	if (argc > 1) {
		secs = strtol(argv[1],&tmp,0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad time - %s",argv[1]);
			return;
		}
	}

	now = current_fs->now ? current_fs->now : time(0);
	max_delarray = 100;
	num_delarray = 0;
	delarray = malloc(max_delarray * sizeof(struct deleted_info));
	if (!delarray) {
		com_err("ls_deleted_inodes", ENOMEM,
			"while allocating deleted information storage");
		exit(1);
	}

	block_buf = malloc(current_fs->blocksize * 3);
	if (!block_buf) {
		com_err("ls_deleted_inodes", ENOMEM, "while allocating block buffer");
		goto error_out;
	}

	retval = ext2fs_open_inode_scan(current_fs, 0, &scan);
	if (retval) {
		com_err("ls_deleted_inodes", retval,
			"while opening inode scan");
		goto error_out;
	}

	do {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
	} while (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE);
	if (retval) {
		com_err("ls_deleted_inodes", retval,
			"while starting inode scan");
		goto error_out;
	}

	while (ino) {
		if ((inode.i_dtime == 0) ||
		    (secs && ((unsigned) abs(now - secs) > inode.i_dtime)))
			goto next;

		lsd.inode = ino;
		lsd.num_blocks = 0;
		lsd.free_blocks = 0;
		lsd.bad_blocks = 0;

		retval = ext2fs_block_iterate2(current_fs, ino,
					       BLOCK_FLAG_READ_ONLY, block_buf,
					       lsdel_proc, &lsd);
		if (retval) {
			com_err("ls_deleted_inodes", retval,
				"while calling ext2fs_block_iterate2");
			goto next;
		}
		if (lsd.free_blocks && !lsd.bad_blocks) {
			if (num_delarray >= max_delarray) {
				max_delarray += 50;
				delarray = realloc(delarray,
			   max_delarray * sizeof(struct deleted_info));
				if (!delarray) {
					com_err("ls_deleted_inodes",
						ENOMEM,
						"while reallocating array");
					exit(1);
				}
			}

			delarray[num_delarray].ino = ino;
			delarray[num_delarray].mode = inode.i_mode;
			delarray[num_delarray].uid = inode_uid(inode);
			delarray[num_delarray].size = inode.i_size;
			if (!LINUX_S_ISDIR(inode.i_mode))
				delarray[num_delarray].size |=
					((__u64) inode.i_size_high << 32);
			delarray[num_delarray].dtime = inode.i_dtime;
			delarray[num_delarray].num_blocks = lsd.num_blocks;
			delarray[num_delarray].free_blocks = lsd.free_blocks;
			num_delarray++;
		}

	next:
		do {
			retval = ext2fs_get_next_inode(scan, &ino, &inode);
		} while (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE);
		if (retval) {
			com_err("ls_deleted_inodes", retval,
				"while doing inode scan");
			goto error_out;
		}
	}

	out = open_pager();

	fprintf(out, " Inode  Owner  Mode    Size      Blocks   Time deleted\n");

	qsort(delarray, num_delarray, sizeof(struct deleted_info),
	      deleted_info_compare);

	for (i = 0; i < num_delarray; i++) {
		fprintf(out, "%6u %6d %6o %6llu %6lld/%6lld %s",
			delarray[i].ino,
			delarray[i].uid, delarray[i].mode, delarray[i].size,
			delarray[i].free_blocks, delarray[i].num_blocks,
			time_to_string(delarray[i].dtime));
	}
	fprintf(out, "%d deleted inodes found.\n", num_delarray);
	close_pager(out);

error_out:
	free(block_buf);
	free(delarray);
	if (scan)
		ext2fs_close_inode_scan(scan);
	return;
}



