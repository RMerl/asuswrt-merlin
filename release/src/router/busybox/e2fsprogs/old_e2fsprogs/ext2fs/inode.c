/* vi: set sw=4 ts=4: */
/*
 * inode.c --- utility routines to read and write inodes
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "ext2_fs.h"
#include "ext2fsP.h"
#include "e2image.h"

struct ext2_struct_inode_scan {
	errcode_t		magic;
	ext2_filsys		fs;
	ext2_ino_t		current_inode;
	blk_t			current_block;
	dgrp_t			current_group;
	ext2_ino_t		inodes_left;
	blk_t			blocks_left;
	dgrp_t			groups_left;
	blk_t			inode_buffer_blocks;
	char *			inode_buffer;
	int			inode_size;
	char *			ptr;
	int			bytes_left;
	char			*temp_buffer;
	errcode_t		(*done_group)(ext2_filsys fs,
					      dgrp_t group,
					      void * priv_data);
	void *			done_group_data;
	int			bad_block_ptr;
	int			scan_flags;
	int			reserved[6];
};

/*
 * This routine flushes the icache, if it exists.
 */
errcode_t ext2fs_flush_icache(ext2_filsys fs)
{
	int	i;

	if (!fs->icache)
		return 0;

	for (i=0; i < fs->icache->cache_size; i++)
		fs->icache->cache[i].ino = 0;

	fs->icache->buffer_blk = 0;
	return 0;
}

static errcode_t create_icache(ext2_filsys fs)
{
	errcode_t	retval;

	if (fs->icache)
		return 0;
	retval = ext2fs_get_mem(sizeof(struct ext2_inode_cache), &fs->icache);
	if (retval)
		return retval;

	memset(fs->icache, 0, sizeof(struct ext2_inode_cache));
	retval = ext2fs_get_mem(fs->blocksize, &fs->icache->buffer);
	if (retval) {
		ext2fs_free_mem(&fs->icache);
		return retval;
	}
	fs->icache->buffer_blk = 0;
	fs->icache->cache_last = -1;
	fs->icache->cache_size = 4;
	fs->icache->refcount = 1;
	retval = ext2fs_get_mem(sizeof(struct ext2_inode_cache_ent)
				* fs->icache->cache_size,
				&fs->icache->cache);
	if (retval) {
		ext2fs_free_mem(&fs->icache->buffer);
		ext2fs_free_mem(&fs->icache);
		return retval;
	}
	ext2fs_flush_icache(fs);
	return 0;
}

errcode_t ext2fs_open_inode_scan(ext2_filsys fs, int buffer_blocks,
				 ext2_inode_scan *ret_scan)
{
	ext2_inode_scan	scan;
	errcode_t	retval;
	errcode_t (*save_get_blocks)(ext2_filsys f, ext2_ino_t ino, blk_t *blocks);

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/*
	 * If fs->badblocks isn't set, then set it --- since the inode
	 * scanning functions require it.
	 */
	if (fs->badblocks == 0) {
		/*
		 * Temporarly save fs->get_blocks and set it to zero,
		 * for compatibility with old e2fsck's.
		 */
		save_get_blocks = fs->get_blocks;
		fs->get_blocks = 0;
		retval = ext2fs_read_bb_inode(fs, &fs->badblocks);
		if (retval) {
			ext2fs_badblocks_list_free(fs->badblocks);
			fs->badblocks = 0;
		}
		fs->get_blocks = save_get_blocks;
	}

	retval = ext2fs_get_mem(sizeof(struct ext2_struct_inode_scan), &scan);
	if (retval)
		return retval;
	memset(scan, 0, sizeof(struct ext2_struct_inode_scan));

	scan->magic = EXT2_ET_MAGIC_INODE_SCAN;
	scan->fs = fs;
	scan->inode_size = EXT2_INODE_SIZE(fs->super);
	scan->bytes_left = 0;
	scan->current_group = 0;
	scan->groups_left = fs->group_desc_count - 1;
	scan->inode_buffer_blocks = buffer_blocks ? buffer_blocks : 8;
	scan->current_block = scan->fs->
		group_desc[scan->current_group].bg_inode_table;
	scan->inodes_left = EXT2_INODES_PER_GROUP(scan->fs->super);
	scan->blocks_left = scan->fs->inode_blocks_per_group;
	retval = ext2fs_get_mem((size_t) (scan->inode_buffer_blocks *
					  fs->blocksize),
				&scan->inode_buffer);
	scan->done_group = 0;
	scan->done_group_data = 0;
	scan->bad_block_ptr = 0;
	if (retval) {
		ext2fs_free_mem(&scan);
		return retval;
	}
	retval = ext2fs_get_mem(scan->inode_size, &scan->temp_buffer);
	if (retval) {
		ext2fs_free_mem(&scan->inode_buffer);
		ext2fs_free_mem(&scan);
		return retval;
	}
	if (scan->fs->badblocks && scan->fs->badblocks->num)
		scan->scan_flags |= EXT2_SF_CHK_BADBLOCKS;
	*ret_scan = scan;
	return 0;
}

void ext2fs_close_inode_scan(ext2_inode_scan scan)
{
	if (!scan || (scan->magic != EXT2_ET_MAGIC_INODE_SCAN))
		return;

	ext2fs_free_mem(&scan->inode_buffer);
	scan->inode_buffer = NULL;
	ext2fs_free_mem(&scan->temp_buffer);
	scan->temp_buffer = NULL;
	ext2fs_free_mem(&scan);
}

void ext2fs_set_inode_callback(ext2_inode_scan scan,
			       errcode_t (*done_group)(ext2_filsys fs,
						       dgrp_t group,
						       void * priv_data),
			       void *done_group_data)
{
	if (!scan || (scan->magic != EXT2_ET_MAGIC_INODE_SCAN))
		return;

	scan->done_group = done_group;
	scan->done_group_data = done_group_data;
}

int ext2fs_inode_scan_flags(ext2_inode_scan scan, int set_flags,
			    int clear_flags)
{
	int	old_flags;

	if (!scan || (scan->magic != EXT2_ET_MAGIC_INODE_SCAN))
		return 0;

	old_flags = scan->scan_flags;
	scan->scan_flags &= ~clear_flags;
	scan->scan_flags |= set_flags;
	return old_flags;
}

/*
 * This function is called by ext2fs_get_next_inode when it needs to
 * get ready to read in a new blockgroup.
 */
static errcode_t get_next_blockgroup(ext2_inode_scan scan)
{
	scan->current_group++;
	scan->groups_left--;

	scan->current_block = scan->fs->
		group_desc[scan->current_group].bg_inode_table;

	scan->current_inode = scan->current_group *
		EXT2_INODES_PER_GROUP(scan->fs->super);

	scan->bytes_left = 0;
	scan->inodes_left = EXT2_INODES_PER_GROUP(scan->fs->super);
	scan->blocks_left = scan->fs->inode_blocks_per_group;
	return 0;
}

errcode_t ext2fs_inode_scan_goto_blockgroup(ext2_inode_scan scan,
					    int	group)
{
	scan->current_group = group - 1;
	scan->groups_left = scan->fs->group_desc_count - group;
	return get_next_blockgroup(scan);
}

/*
 * This function is called by get_next_blocks() to check for bad
 * blocks in the inode table.
 *
 * This function assumes that badblocks_list->list is sorted in
 * increasing order.
 */
static errcode_t check_for_inode_bad_blocks(ext2_inode_scan scan,
					    blk_t *num_blocks)
{
	blk_t	blk = scan->current_block;
	badblocks_list	bb = scan->fs->badblocks;

	/*
	 * If the inode table is missing, then obviously there are no
	 * bad blocks.  :-)
	 */
	if (blk == 0)
		return 0;

	/*
	 * If the current block is greater than the bad block listed
	 * in the bad block list, then advance the pointer until this
	 * is no longer the case.  If we run out of bad blocks, then
	 * we don't need to do any more checking!
	 */
	while (blk > bb->list[scan->bad_block_ptr]) {
		if (++scan->bad_block_ptr >= bb->num) {
			scan->scan_flags &= ~EXT2_SF_CHK_BADBLOCKS;
			return 0;
		}
	}

	/*
	 * If the current block is equal to the bad block listed in
	 * the bad block list, then handle that one block specially.
	 * (We could try to handle runs of bad blocks, but that
	 * only increases CPU efficiency by a small amount, at the
	 * expense of a huge expense of code complexity, and for an
	 * uncommon case at that.)
	 */
	if (blk == bb->list[scan->bad_block_ptr]) {
		scan->scan_flags |= EXT2_SF_BAD_INODE_BLK;
		*num_blocks = 1;
		if (++scan->bad_block_ptr >= bb->num)
			scan->scan_flags &= ~EXT2_SF_CHK_BADBLOCKS;
		return 0;
	}

	/*
	 * If there is a bad block in the range that we're about to
	 * read in, adjust the number of blocks to read so that we we
	 * don't read in the bad block.  (Then the next block to read
	 * will be the bad block, which is handled in the above case.)
	 */
	if ((blk + *num_blocks) > bb->list[scan->bad_block_ptr])
		*num_blocks = (int) (bb->list[scan->bad_block_ptr] - blk);

	return 0;
}

/*
 * This function is called by ext2fs_get_next_inode when it needs to
 * read in more blocks from the current blockgroup's inode table.
 */
static errcode_t get_next_blocks(ext2_inode_scan scan)
{
	blk_t		num_blocks;
	errcode_t	retval;

	/*
	 * Figure out how many blocks to read; we read at most
	 * inode_buffer_blocks, and perhaps less if there aren't that
	 * many blocks left to read.
	 */
	num_blocks = scan->inode_buffer_blocks;
	if (num_blocks > scan->blocks_left)
		num_blocks = scan->blocks_left;

	/*
	 * If the past block "read" was a bad block, then mark the
	 * left-over extra bytes as also being bad.
	 */
	if (scan->scan_flags & EXT2_SF_BAD_INODE_BLK) {
		if (scan->bytes_left)
			scan->scan_flags |= EXT2_SF_BAD_EXTRA_BYTES;
		scan->scan_flags &= ~EXT2_SF_BAD_INODE_BLK;
	}

	/*
	 * Do inode bad block processing, if necessary.
	 */
	if (scan->scan_flags & EXT2_SF_CHK_BADBLOCKS) {
		retval = check_for_inode_bad_blocks(scan, &num_blocks);
		if (retval)
			return retval;
	}

	if ((scan->scan_flags & EXT2_SF_BAD_INODE_BLK) ||
	    (scan->current_block == 0)) {
		memset(scan->inode_buffer, 0,
		       (size_t) num_blocks * scan->fs->blocksize);
	} else {
		retval = io_channel_read_blk(scan->fs->io,
					     scan->current_block,
					     (int) num_blocks,
					     scan->inode_buffer);
		if (retval)
			return EXT2_ET_NEXT_INODE_READ;
	}
	scan->ptr = scan->inode_buffer;
	scan->bytes_left = num_blocks * scan->fs->blocksize;

	scan->blocks_left -= num_blocks;
	if (scan->current_block)
		scan->current_block += num_blocks;
	return 0;
}

errcode_t ext2fs_get_next_inode_full(ext2_inode_scan scan, ext2_ino_t *ino,
				     struct ext2_inode *inode, int bufsize)
{
	errcode_t	retval;
	int		extra_bytes = 0;

	EXT2_CHECK_MAGIC(scan, EXT2_ET_MAGIC_INODE_SCAN);

	/*
	 * Do we need to start reading a new block group?
	 */
	if (scan->inodes_left <= 0) {
	force_new_group:
		if (scan->done_group) {
			retval = (scan->done_group)
				(scan->fs, scan->current_group,
				 scan->done_group_data);
			if (retval)
				return retval;
		}
		if (scan->groups_left <= 0) {
			*ino = 0;
			return 0;
		}
		retval = get_next_blockgroup(scan);
		if (retval)
			return retval;
	}
	/*
	 * This is done outside the above if statement so that the
	 * check can be done for block group #0.
	 */
	if (scan->current_block == 0) {
		if (scan->scan_flags & EXT2_SF_SKIP_MISSING_ITABLE) {
			goto force_new_group;
		} else
			return EXT2_ET_MISSING_INODE_TABLE;
	}


	/*
	 * Have we run out of space in the inode buffer?  If so, we
	 * need to read in more blocks.
	 */
	if (scan->bytes_left < scan->inode_size) {
		memcpy(scan->temp_buffer, scan->ptr, scan->bytes_left);
		extra_bytes = scan->bytes_left;

		retval = get_next_blocks(scan);
		if (retval)
			return retval;
#if 0
		/*
		 * XXX test  Need check for used inode somehow.
		 * (Note: this is hard.)
		 */
		if (is_empty_scan(scan))
			goto force_new_group;
#endif
	}

	retval = 0;
	if (extra_bytes) {
		memcpy(scan->temp_buffer+extra_bytes, scan->ptr,
		       scan->inode_size - extra_bytes);
		scan->ptr += scan->inode_size - extra_bytes;
		scan->bytes_left -= scan->inode_size - extra_bytes;

#if BB_BIG_ENDIAN
		if ((scan->fs->flags & EXT2_FLAG_SWAP_BYTES) ||
		    (scan->fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
			ext2fs_swap_inode_full(scan->fs,
				(struct ext2_inode_large *) inode,
				(struct ext2_inode_large *) scan->temp_buffer,
				0, bufsize);
		else
#endif
			*inode = *((struct ext2_inode *) scan->temp_buffer);
		if (scan->scan_flags & EXT2_SF_BAD_EXTRA_BYTES)
			retval = EXT2_ET_BAD_BLOCK_IN_INODE_TABLE;
		scan->scan_flags &= ~EXT2_SF_BAD_EXTRA_BYTES;
	} else {
#if BB_BIG_ENDIAN
		if ((scan->fs->flags & EXT2_FLAG_SWAP_BYTES) ||
		    (scan->fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
			ext2fs_swap_inode_full(scan->fs,
				(struct ext2_inode_large *) inode,
				(struct ext2_inode_large *) scan->ptr,
				0, bufsize);
		else
#endif
			memcpy(inode, scan->ptr, bufsize);
		scan->ptr += scan->inode_size;
		scan->bytes_left -= scan->inode_size;
		if (scan->scan_flags & EXT2_SF_BAD_INODE_BLK)
			retval = EXT2_ET_BAD_BLOCK_IN_INODE_TABLE;
	}

	scan->inodes_left--;
	scan->current_inode++;
	*ino = scan->current_inode;
	return retval;
}

errcode_t ext2fs_get_next_inode(ext2_inode_scan scan, ext2_ino_t *ino,
				struct ext2_inode *inode)
{
	return ext2fs_get_next_inode_full(scan, ino, inode,
						sizeof(struct ext2_inode));
}

/*
 * Functions to read and write a single inode.
 */
errcode_t ext2fs_read_inode_full(ext2_filsys fs, ext2_ino_t ino,
				 struct ext2_inode * inode, int bufsize)
{
	unsigned long	group, block, block_nr, offset;
	char		*ptr;
	errcode_t	retval;
	int		clen, i, inodes_per_block, length;
	io_channel	io;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/* Check to see if user has an override function */
	if (fs->read_inode) {
		retval = (fs->read_inode)(fs, ino, inode);
		if (retval != EXT2_ET_CALLBACK_NOTHANDLED)
			return retval;
	}
	/* Create inode cache if not present */
	if (!fs->icache) {
		retval = create_icache(fs);
		if (retval)
			return retval;
	}
	/* Check to see if it's in the inode cache */
	if (bufsize == sizeof(struct ext2_inode)) {
		/* only old good inode can be retrieve from the cache */
		for (i=0; i < fs->icache->cache_size; i++) {
			if (fs->icache->cache[i].ino == ino) {
				*inode = fs->icache->cache[i].inode;
				return 0;
			}
		}
	}
	if ((ino == 0) || (ino > fs->super->s_inodes_count))
		return EXT2_ET_BAD_INODE_NUM;
	if (fs->flags & EXT2_FLAG_IMAGE_FILE) {
		inodes_per_block = fs->blocksize / EXT2_INODE_SIZE(fs->super);
		block_nr = fs->image_header->offset_inode / fs->blocksize;
		block_nr += (ino - 1) / inodes_per_block;
		offset = ((ino - 1) % inodes_per_block) *
			EXT2_INODE_SIZE(fs->super);
		io = fs->image_io;
	} else {
		group = (ino - 1) / EXT2_INODES_PER_GROUP(fs->super);
		offset = ((ino - 1) % EXT2_INODES_PER_GROUP(fs->super)) *
			EXT2_INODE_SIZE(fs->super);
		block = offset >> EXT2_BLOCK_SIZE_BITS(fs->super);
		if (!fs->group_desc[(unsigned)group].bg_inode_table)
			return EXT2_ET_MISSING_INODE_TABLE;
		block_nr = fs->group_desc[(unsigned)group].bg_inode_table +
			block;
		io = fs->io;
	}
	offset &= (EXT2_BLOCK_SIZE(fs->super) - 1);

	length = EXT2_INODE_SIZE(fs->super);
	if (bufsize < length)
		length = bufsize;

	ptr = (char *) inode;
	while (length) {
		clen = length;
		if ((offset + length) > fs->blocksize)
			clen = fs->blocksize - offset;

		if (block_nr != fs->icache->buffer_blk) {
			retval = io_channel_read_blk(io, block_nr, 1,
						     fs->icache->buffer);
			if (retval)
				return retval;
			fs->icache->buffer_blk = block_nr;
		}

		memcpy(ptr, ((char *) fs->icache->buffer) + (unsigned) offset,
		       clen);

		offset = 0;
		length -= clen;
		ptr += clen;
		block_nr++;
	}

#if BB_BIG_ENDIAN
	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
		ext2fs_swap_inode_full(fs, (struct ext2_inode_large *) inode,
				       (struct ext2_inode_large *) inode,
				       0, length);
#endif

	/* Update the inode cache */
	fs->icache->cache_last = (fs->icache->cache_last + 1) %
		fs->icache->cache_size;
	fs->icache->cache[fs->icache->cache_last].ino = ino;
	fs->icache->cache[fs->icache->cache_last].inode = *inode;

	return 0;
}

errcode_t ext2fs_read_inode(ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode * inode)
{
	return ext2fs_read_inode_full(fs, ino, inode,
					sizeof(struct ext2_inode));
}

errcode_t ext2fs_write_inode_full(ext2_filsys fs, ext2_ino_t ino,
				  struct ext2_inode * inode, int bufsize)
{
	unsigned long group, block, block_nr, offset;
	errcode_t retval = 0;
	struct ext2_inode_large temp_inode, *w_inode;
	char *ptr;
	int clen, i, length;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	/* Check to see if user provided an override function */
	if (fs->write_inode) {
		retval = (fs->write_inode)(fs, ino, inode);
		if (retval != EXT2_ET_CALLBACK_NOTHANDLED)
			return retval;
	}

	/* Check to see if the inode cache needs to be updated */
	if (fs->icache) {
		for (i=0; i < fs->icache->cache_size; i++) {
			if (fs->icache->cache[i].ino == ino) {
				fs->icache->cache[i].inode = *inode;
				break;
			}
		}
	} else {
		retval = create_icache(fs);
		if (retval)
			return retval;
	}

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	if ((ino == 0) || (ino > fs->super->s_inodes_count))
		return EXT2_ET_BAD_INODE_NUM;

	length = bufsize;
	if (length < EXT2_INODE_SIZE(fs->super))
		length = EXT2_INODE_SIZE(fs->super);

	if (length > (int) sizeof(struct ext2_inode_large)) {
		w_inode = xmalloc(length);
	} else
		w_inode = &temp_inode;
	memset(w_inode, 0, length);

#if BB_BIG_ENDIAN
	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE))
		ext2fs_swap_inode_full(fs, w_inode,
				       (struct ext2_inode_large *) inode,
				       1, bufsize);
	else
#endif
		memcpy(w_inode, inode, bufsize);

	group = (ino - 1) / EXT2_INODES_PER_GROUP(fs->super);
	offset = ((ino - 1) % EXT2_INODES_PER_GROUP(fs->super)) *
		EXT2_INODE_SIZE(fs->super);
	block = offset >> EXT2_BLOCK_SIZE_BITS(fs->super);
	if (!fs->group_desc[(unsigned) group].bg_inode_table)
		return EXT2_ET_MISSING_INODE_TABLE;
	block_nr = fs->group_desc[(unsigned) group].bg_inode_table + block;

	offset &= (EXT2_BLOCK_SIZE(fs->super) - 1);

	length = EXT2_INODE_SIZE(fs->super);
	if (length > bufsize)
		length = bufsize;

	ptr = (char *) w_inode;

	while (length) {
		clen = length;
		if ((offset + length) > fs->blocksize)
			clen = fs->blocksize - offset;

		if (fs->icache->buffer_blk != block_nr) {
			retval = io_channel_read_blk(fs->io, block_nr, 1,
						     fs->icache->buffer);
			if (retval)
				goto errout;
			fs->icache->buffer_blk = block_nr;
		}


		memcpy((char *) fs->icache->buffer + (unsigned) offset,
		       ptr, clen);

		retval = io_channel_write_blk(fs->io, block_nr, 1,
					      fs->icache->buffer);
		if (retval)
			goto errout;

		offset = 0;
		ptr += clen;
		length -= clen;
		block_nr++;
	}

	fs->flags |= EXT2_FLAG_CHANGED;
errout:
	if (w_inode && w_inode != &temp_inode)
		free(w_inode);
	return retval;
}

errcode_t ext2fs_write_inode(ext2_filsys fs, ext2_ino_t ino,
			     struct ext2_inode *inode)
{
	return ext2fs_write_inode_full(fs, ino, inode,
				       sizeof(struct ext2_inode));
}

/*
 * This function should be called when writing a new inode.  It makes
 * sure that extra part of large inodes is initialized properly.
 */
errcode_t ext2fs_write_new_inode(ext2_filsys fs, ext2_ino_t ino,
				 struct ext2_inode *inode)
{
	struct ext2_inode	*buf;
	int			size = EXT2_INODE_SIZE(fs->super);
	struct ext2_inode_large	*large_inode;

	if (size == sizeof(struct ext2_inode))
		return ext2fs_write_inode_full(fs, ino, inode,
					       sizeof(struct ext2_inode));

	buf = xmalloc(size);

	memset(buf, 0, size);
	*buf = *inode;

	large_inode = (struct ext2_inode_large *) buf;
	large_inode->i_extra_isize = sizeof(struct ext2_inode_large) -
		EXT2_GOOD_OLD_INODE_SIZE;

	return ext2fs_write_inode_full(fs, ino, buf, size);
}


errcode_t ext2fs_get_blocks(ext2_filsys fs, ext2_ino_t ino, blk_t *blocks)
{
	struct ext2_inode	inode;
	int			i;
	errcode_t		retval;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (ino > fs->super->s_inodes_count)
		return EXT2_ET_BAD_INODE_NUM;

	if (fs->get_blocks) {
		if (!(*fs->get_blocks)(fs, ino, blocks))
			return 0;
	}
	retval = ext2fs_read_inode(fs, ino, &inode);
	if (retval)
		return retval;
	for (i=0; i < EXT2_N_BLOCKS; i++)
		blocks[i] = inode.i_block[i];
	return 0;
}

errcode_t ext2fs_check_directory(ext2_filsys fs, ext2_ino_t ino)
{
	struct	ext2_inode	inode;
	errcode_t		retval;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (ino > fs->super->s_inodes_count)
		return EXT2_ET_BAD_INODE_NUM;

	if (fs->check_directory) {
		retval = (fs->check_directory)(fs, ino);
		if (retval != EXT2_ET_CALLBACK_NOTHANDLED)
			return retval;
	}
	retval = ext2fs_read_inode(fs, ino, &inode);
	if (retval)
		return retval;
	if (!LINUX_S_ISDIR(inode.i_mode))
		return EXT2_ET_NO_DIRECTORY;
	return 0;
}

