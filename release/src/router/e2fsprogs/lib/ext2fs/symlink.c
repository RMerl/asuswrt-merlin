/*
 * symlink.c --- make a symlink in the filesystem, based on mkdir.c
 *
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
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

errcode_t ext2fs_symlink(ext2_filsys fs, ext2_ino_t parent, ext2_ino_t ino,
			 const char *name, char *target)
{
	ext2_extent_handle_t	handle;
	errcode_t		retval;
	struct ext2_inode	inode;
	ext2_ino_t		scratch_ino;
	blk64_t			blk;
	int			fastlink;
	int			target_len;
	char			*block_buf = 0;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/* The Linux kernel doesn't allow for links longer than a block */
	target_len = strlen(target);
	if (target_len > fs->blocksize) {
		retval = EXT2_ET_INVALID_ARGUMENT;
		goto cleanup;
	}

	/*
	 * Allocate a data block for slow links
	 */
	fastlink = (target_len < sizeof(inode.i_block));
	if (!fastlink) {
		retval = ext2fs_new_block2(fs, 0, 0, &blk);
		if (retval)
			goto cleanup;
		retval = ext2fs_get_mem(fs->blocksize, &block_buf);
		if (retval)
			goto cleanup;
	}

	/*
	 * Allocate an inode, if necessary
	 */
	if (!ino) {
		retval = ext2fs_new_inode(fs, parent, LINUX_S_IFLNK | 0755,
					  0, &ino);
		if (retval)
			goto cleanup;
	}

	/*
	 * Create the inode structure....
	 */
	memset(&inode, 0, sizeof(struct ext2_inode));
	inode.i_mode = LINUX_S_IFLNK | 0777;
	inode.i_uid = inode.i_gid = 0;
	ext2fs_iblk_set(fs, &inode, fastlink ? 0 : 1);
	inode.i_links_count = 1;
	inode.i_size = target_len;
	/* The time fields are set by ext2fs_write_new_inode() */

	if (fastlink) {
		/* Fast symlinks, target stored in inode */
		strcpy((char *)&inode.i_block, target);
	} else {
		/* Slow symlinks, target stored in the first block */
		memset(block_buf, 0, fs->blocksize);
		strcpy(block_buf, target);
		if (fs->super->s_feature_incompat &
		   EXT3_FEATURE_INCOMPAT_EXTENTS) {
			/*
			 * The extent bmap is setup after the inode and block
			 * have been written out below.
			 */
			inode.i_flags |= EXT4_EXTENTS_FL;
		} else {
			inode.i_block[0] = blk;
		}
	}

	/*
	 * Write out the inode and inode data block.  The inode generation
	 * number is assigned by write_new_inode, which means that the
	 * operations using ino must come after it.
	 */
	retval = ext2fs_write_new_inode(fs, ino, &inode);
	if (retval)
		goto cleanup;

	if (!fastlink) {
		retval = io_channel_write_blk(fs->io, blk, 1, block_buf);
		if (retval)
			goto cleanup;

		if (fs->super->s_feature_incompat &
		    EXT3_FEATURE_INCOMPAT_EXTENTS) {
			retval = ext2fs_extent_open2(fs, ino, &inode, &handle);
			if (retval)
				goto cleanup;
			retval = ext2fs_extent_set_bmap(handle, 0, blk, 0);
			ext2fs_extent_free(handle);
			if (retval)
				goto cleanup;
		}
	}

	/*
	 * Link the symlink into the filesystem hierarchy
	 */
	if (name) {
		retval = ext2fs_lookup(fs, parent, name, strlen(name), 0,
				       &scratch_ino);
		if (!retval) {
			retval = EXT2_ET_FILE_EXISTS;
			goto cleanup;
		}
		if (retval != EXT2_ET_FILE_NOT_FOUND)
			goto cleanup;
		retval = ext2fs_link(fs, parent, name, ino, EXT2_FT_SYMLINK);
		if (retval)
			goto cleanup;
	}

	/*
	 * Update accounting....
	 */
	if (!fastlink)
		ext2fs_block_alloc_stats2(fs, blk, +1);
	ext2fs_inode_alloc_stats2(fs, ino, +1, 0);

cleanup:
	if (block_buf)
		ext2fs_free_mem(&block_buf);
	return retval;
}
