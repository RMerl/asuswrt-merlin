/* vi: set sw=4 ts=4: */
/*
 * mkdir.c --- make a directory in the filesystem
 *
 * Copyright (C) 1994, 1995 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

#ifndef EXT2_FT_DIR
#define EXT2_FT_DIR		2
#endif

errcode_t ext2fs_mkdir(ext2_filsys fs, ext2_ino_t parent, ext2_ino_t inum,
		       const char *name)
{
	errcode_t		retval;
	struct ext2_inode	parent_inode, inode;
	ext2_ino_t		ino = inum;
	ext2_ino_t		scratch_ino;
	blk_t			blk;
	char			*block = NULL;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/*
	 * Allocate an inode, if necessary
	 */
	if (!ino) {
		retval = ext2fs_new_inode(fs, parent, LINUX_S_IFDIR | 0755,
					  0, &ino);
		if (retval)
			goto cleanup;
	}

	/*
	 * Allocate a data block for the directory
	 */
	retval = ext2fs_new_block(fs, 0, 0, &blk);
	if (retval)
		goto cleanup;

	/*
	 * Create a scratch template for the directory
	 */
	retval = ext2fs_new_dir_block(fs, ino, parent, &block);
	if (retval)
		goto cleanup;

	/*
	 * Get the parent's inode, if necessary
	 */
	if (parent != ino) {
		retval = ext2fs_read_inode(fs, parent, &parent_inode);
		if (retval)
			goto cleanup;
	} else
		memset(&parent_inode, 0, sizeof(parent_inode));

	/*
	 * Create the inode structure....
	 */
	memset(&inode, 0, sizeof(struct ext2_inode));
	inode.i_mode = LINUX_S_IFDIR | (0777 & ~fs->umask);
	inode.i_uid = inode.i_gid = 0;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;
	inode.i_links_count = 2;
	inode.i_ctime = inode.i_atime = inode.i_mtime = time(NULL);
	inode.i_size = fs->blocksize;

	/*
	 * Write out the inode and inode data block
	 */
	retval = ext2fs_write_dir_block(fs, blk, block);
	if (retval)
		goto cleanup;
	retval = ext2fs_write_new_inode(fs, ino, &inode);
	if (retval)
		goto cleanup;

	/*
	 * Link the directory into the filesystem hierarchy
	 */
	if (name) {
		retval = ext2fs_lookup(fs, parent, name, strlen(name), 0,
				       &scratch_ino);
		if (!retval) {
			retval = EXT2_ET_DIR_EXISTS;
			name = 0;
			goto cleanup;
		}
		if (retval != EXT2_ET_FILE_NOT_FOUND)
			goto cleanup;
		retval = ext2fs_link(fs, parent, name, ino, EXT2_FT_DIR);
		if (retval)
			goto cleanup;
	}

	/*
	 * Update parent inode's counts
	 */
	if (parent != ino) {
		parent_inode.i_links_count++;
		retval = ext2fs_write_inode(fs, parent, &parent_inode);
		if (retval)
			goto cleanup;
	}

	/*
	 * Update accounting....
	 */
	ext2fs_block_alloc_stats(fs, blk, +1);
	ext2fs_inode_alloc_stats2(fs, ino, +1, 1);

cleanup:
	ext2fs_free_mem(&block);
	return retval;
}
