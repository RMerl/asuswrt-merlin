/* vi: set sw=4 ts=4: */
/*
 * ext2fs.h --- ext2fs
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "ext2fs.h"
#include "bitops.h"
#include <string.h>

/*
 *  Allocate memory
 */
errcode_t ext2fs_get_mem(unsigned long size, void *ptr)
{
	void **pp = (void **)ptr;

	*pp = malloc(size);
	if (!*pp)
		return EXT2_ET_NO_MEMORY;
	return 0;
}

/*
 * Free memory
 */
errcode_t ext2fs_free_mem(void *ptr)
{
	void **pp = (void **)ptr;

	free(*pp);
	*pp = 0;
	return 0;
}

/*
 *  Resize memory
 */
errcode_t ext2fs_resize_mem(unsigned long EXT2FS_ATTR((unused)) old_size,
				     unsigned long size, void *ptr)
{
	void *p;

	/* Use "memcpy" for pointer assignments here to avoid problems
	 * with C99 strict type aliasing rules. */
	memcpy(&p, ptr, sizeof (p));
	p = xrealloc(p, size);
	memcpy(ptr, &p, sizeof (p));
	return 0;
}

/*
 * Mark a filesystem superblock as dirty
 */
void ext2fs_mark_super_dirty(ext2_filsys fs)
{
	fs->flags |= EXT2_FLAG_DIRTY | EXT2_FLAG_CHANGED;
}

/*
 * Mark a filesystem as changed
 */
void ext2fs_mark_changed(ext2_filsys fs)
{
	fs->flags |= EXT2_FLAG_CHANGED;
}

/*
 * Check to see if a filesystem has changed
 */
int ext2fs_test_changed(ext2_filsys fs)
{
	return (fs->flags & EXT2_FLAG_CHANGED);
}

/*
 * Mark a filesystem as valid
 */
void ext2fs_mark_valid(ext2_filsys fs)
{
	fs->flags |= EXT2_FLAG_VALID;
}

/*
 * Mark a filesystem as NOT valid
 */
void ext2fs_unmark_valid(ext2_filsys fs)
{
	fs->flags &= ~EXT2_FLAG_VALID;
}

/*
 * Check to see if a filesystem is valid
 */
int ext2fs_test_valid(ext2_filsys fs)
{
	return (fs->flags & EXT2_FLAG_VALID);
}

/*
 * Mark the inode bitmap as dirty
 */
void ext2fs_mark_ib_dirty(ext2_filsys fs)
{
	fs->flags |= EXT2_FLAG_IB_DIRTY | EXT2_FLAG_CHANGED;
}

/*
 * Mark the block bitmap as dirty
 */
void ext2fs_mark_bb_dirty(ext2_filsys fs)
{
	fs->flags |= EXT2_FLAG_BB_DIRTY | EXT2_FLAG_CHANGED;
}

/*
 * Check to see if a filesystem's inode bitmap is dirty
 */
int ext2fs_test_ib_dirty(ext2_filsys fs)
{
	return (fs->flags & EXT2_FLAG_IB_DIRTY);
}

/*
 * Check to see if a filesystem's block bitmap is dirty
 */
int ext2fs_test_bb_dirty(ext2_filsys fs)
{
	return (fs->flags & EXT2_FLAG_BB_DIRTY);
}

/*
 * Return the group # of a block
 */
int ext2fs_group_of_blk(ext2_filsys fs, blk_t blk)
{
	return (blk - fs->super->s_first_data_block) /
		fs->super->s_blocks_per_group;
}

/*
 * Return the group # of an inode number
 */
int ext2fs_group_of_ino(ext2_filsys fs, ext2_ino_t ino)
{
	return (ino - 1) / fs->super->s_inodes_per_group;
}

blk_t ext2fs_inode_data_blocks(ext2_filsys fs,
					struct ext2_inode *inode)
{
	return inode->i_blocks -
		(inode->i_file_acl ? fs->blocksize >> 9 : 0);
}









__u16 ext2fs_swab16(__u16 val)
{
	return (val >> 8) | (val << 8);
}

__u32 ext2fs_swab32(__u32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
		((val<<8)&0xFF0000) | (val<<24));
}

int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno);

int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_TEST_ERROR, bitno);
		return 0;
	}
	return ext2fs_test_bit(bitno - bitmap->start, bitmap->bitmap);
}

int ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	return ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap)
				       bitmap,
					  block);
}

int ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block)
{
	return ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap,
					    block);
}

int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap,
					  block);
}

int ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode)
{
	return ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap) bitmap,
					  inode);
}

int ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ext2_ino_t inode)
{
	return ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap,
				     inode);
}

int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap,
					  inode);
}

void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
	ext2fs_set_bit(block - bitmap->start, bitmap->bitmap);
}

void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					      blk_t block)
{
	ext2fs_clear_bit(block - bitmap->start, bitmap->bitmap);
}

int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
	return ext2fs_test_bit(block - bitmap->start, bitmap->bitmap);
}

void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ext2_ino_t inode)
{
	ext2fs_set_bit(inode - bitmap->start, bitmap->bitmap);
}

void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					      ext2_ino_t inode)
{
	ext2fs_clear_bit(inode - bitmap->start, bitmap->bitmap);
}

int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					   ext2_ino_t inode)
{
	return ext2fs_test_bit(inode - bitmap->start, bitmap->bitmap);
}

blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap)
{
	return bitmap->start;
}

ext2_ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap)
{
	return bitmap->start;
}

blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap)
{
	return bitmap->end;
}

ext2_ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap)
{
	return bitmap->end;
}

int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					    blk_t block, int num)
{
	int	i;

	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
						 blk_t block, int num)
{
	int	i;

	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num)
{
	int	i;

	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num)
{
	int	i;

	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num)
{
	int	i;

	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}

void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						    blk_t block, int num)
{
	int	i;
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}
