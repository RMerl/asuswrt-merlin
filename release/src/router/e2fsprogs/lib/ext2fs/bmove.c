/*
 * bmove.c --- Move blocks around to make way for a particular
 * 	filesystem structure.
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "ext2_fs.h"
#include "ext2fsP.h"

struct process_block_struct {
	ext2_ino_t		ino;
	struct ext2_inode *	inode;
	ext2fs_block_bitmap	reserve;
	ext2fs_block_bitmap	alloc_map;
	errcode_t		error;
	char			*buf;
	int			add_dir;
	int			flags;
};

static int process_block(ext2_filsys fs, blk_t	*block_nr,
			 e2_blkcnt_t blockcnt, blk_t ref_block,
			 int ref_offset, void *priv_data)
{
	struct process_block_struct *pb;
	errcode_t	retval;
	int		ret;
	blk_t		block, orig;

	pb = (struct process_block_struct *) priv_data;
	block = orig = *block_nr;
	ret = 0;

	/*
	 * Let's see if this is one which we need to relocate
	 */
	if (ext2fs_test_block_bitmap(pb->reserve, block)) {
		do {
			if (++block >= fs->super->s_blocks_count)
				block = fs->super->s_first_data_block;
			if (block == orig) {
				pb->error = EXT2_ET_BLOCK_ALLOC_FAIL;
				return BLOCK_ABORT;
			}
		} while (ext2fs_test_block_bitmap(pb->reserve, block) ||
			 ext2fs_test_block_bitmap(pb->alloc_map, block));

		retval = io_channel_read_blk(fs->io, orig, 1, pb->buf);
		if (retval) {
			pb->error = retval;
			return BLOCK_ABORT;
		}
		retval = io_channel_write_blk(fs->io, block, 1, pb->buf);
		if (retval) {
			pb->error = retval;
			return BLOCK_ABORT;
		}
		*block_nr = block;
		ext2fs_mark_block_bitmap(pb->alloc_map, block);
		ret = BLOCK_CHANGED;
		if (pb->flags & EXT2_BMOVE_DEBUG)
			printf("ino=%ld, blockcnt=%lld, %u->%u\n", pb->ino,
			       blockcnt, orig, block);
	}
	if (pb->add_dir) {
		retval = ext2fs_add_dir_block(fs->dblist, pb->ino,
					      block, (int) blockcnt);
		if (retval) {
			pb->error = retval;
			ret |= BLOCK_ABORT;
		}
	}
	return ret;
}

errcode_t ext2fs_move_blocks(ext2_filsys fs,
			     ext2fs_block_bitmap reserve,
			     ext2fs_block_bitmap alloc_map,
			     int flags)
{
	ext2_ino_t	ino;
	struct ext2_inode inode;
	errcode_t	retval;
	struct process_block_struct pb;
	ext2_inode_scan	scan;
	char		*block_buf;

	retval = ext2fs_open_inode_scan(fs, 0, &scan);
	if (retval)
		return retval;

	pb.reserve = reserve;
	pb.error = 0;
	pb.alloc_map = alloc_map ? alloc_map : fs->block_map;
	pb.flags = flags;

	retval = ext2fs_get_array(4, fs->blocksize, &block_buf);
	if (retval)
		return retval;
	pb.buf = block_buf + fs->blocksize * 3;

	/*
	 * If GET_DBLIST is set in the flags field, then we should
	 * gather directory block information while we're doing the
	 * block move.
	 */
	if (flags & EXT2_BMOVE_GET_DBLIST) {
		if (fs->dblist) {
			ext2fs_free_dblist(fs->dblist);
			fs->dblist = NULL;
		}
		retval = ext2fs_init_dblist(fs, 0);
		if (retval)
			return retval;
	}

	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval)
		return retval;

	while (ino) {
		if ((inode.i_links_count == 0) ||
		    !ext2fs_inode_has_valid_blocks(&inode))
			goto next;

		pb.ino = ino;
		pb.inode = &inode;

		pb.add_dir = (LINUX_S_ISDIR(inode.i_mode) &&
			      flags & EXT2_BMOVE_GET_DBLIST);

		retval = ext2fs_block_iterate2(fs, ino, 0, block_buf,
					      process_block, &pb);
		if (retval)
			return retval;
		if (pb.error)
			return pb.error;

	next:
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE)
			goto next;
	}
	return 0;
}

