/*
 * punch.c --- deallocate blocks allocated to an inode
 *
 * Copyright (C) 2010 Theodore Ts'o.
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
#include <errno.h>

#include "ext2_fs.h"
#include "ext2fs.h"

#undef PUNCH_DEBUG

/*
 * This function returns 1 if the specified block is all zeros
 */
static int check_zero_block(char *buf, int blocksize)
{
	char	*cp = buf;
	int	left = blocksize;

	while (left > 0) {
		if (*cp++)
			return 0;
		left--;
	}
	return 1;
}

/*
 * This clever recursive function handles i_blocks[] as well as
 * indirect, double indirect, and triple indirect blocks.  It iterates
 * over the entries in the i_blocks array or indirect blocks, and for
 * each one, will recursively handle any indirect blocks and then
 * frees and deallocates the blocks.
 */
static errcode_t ind_punch(ext2_filsys fs, struct ext2_inode *inode,
			   char *block_buf, blk_t *p, int level,
			   blk_t start, blk_t count, int max)
{
	errcode_t	retval;
	blk_t		b, offset;
	int		i, incr;
	int		freed = 0;

#ifdef PUNCH_DEBUG
	printf("Entering ind_punch, level %d, start %u, count %u, "
	       "max %d\n", level, start, count, max);
#endif
	incr = 1 << ((EXT2_BLOCK_SIZE_BITS(fs->super)-2)*level);
	for (i=0, offset=0; i < max; i++, p++, offset += incr) {
		if (offset > count)
			break;
		if (*p == 0 || (offset+incr) <= start)
			continue;
		b = *p;
		if (level > 0) {
			blk_t start2;
#ifdef PUNCH_DEBUG
			printf("Reading indirect block %u\n", b);
#endif
			retval = ext2fs_read_ind_block(fs, b, block_buf);
			if (retval)
				return retval;
			start2 = (start > offset) ? start - offset : 0;
			retval = ind_punch(fs, inode, block_buf + fs->blocksize,
					   (blk_t *) block_buf, level - 1,
					   start2, count - offset,
					   fs->blocksize >> 2);
			if (retval)
				return retval;
			retval = ext2fs_write_ind_block(fs, b, block_buf);
			if (retval)
				return retval;
			if (!check_zero_block(block_buf, fs->blocksize))
				continue;
		}
#ifdef PUNCH_DEBUG
		printf("Freeing block %u (offset %d)\n", b, offset);
#endif
		ext2fs_block_alloc_stats(fs, b, -1);
		*p = 0;
		freed++;
	}
#ifdef PUNCH_DEBUG
	printf("Freed %d blocks\n", freed);
#endif
	return ext2fs_iblk_sub_blocks(fs, inode, freed);
}

static errcode_t ext2fs_punch_ind(ext2_filsys fs, struct ext2_inode *inode,
				  char *block_buf, blk_t start, blk_t count)
{
	errcode_t		retval;
	char			*buf = 0;
	int			level;
	int			num = EXT2_NDIR_BLOCKS;
	blk_t			*bp = inode->i_block;
	blk_t			addr_per_block;
	blk_t			max = EXT2_NDIR_BLOCKS;

	if (!block_buf) {
		retval = ext2fs_get_array(3, fs->blocksize, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}

	addr_per_block = (blk_t) fs->blocksize >> 2;

	for (level=0; level < 4; level++, max *= addr_per_block) {
#ifdef PUNCH_DEBUG
		printf("Main loop level %d, start %u count %u "
		       "max %d num %d\n", level, start, count, max, num);
#endif
		if (start < max) {
			retval = ind_punch(fs, inode, block_buf, bp, level,
					   start, count, num);
			if (retval)
				goto errout;
			if (count > max)
				count -= max - start;
			else
				break;
			start = 0;
		} else
			start -= max;
		bp += num;
		if (level == 0) {
			num = 1;
			max = 1;
		}
	}
	retval = 0;
errout:
	if (buf)
		ext2fs_free_mem(&buf);
	return retval;
}

#ifdef PUNCH_DEBUG

#define dbg_printf(f, a...)  printf(f, ## a)

static void dbg_print_extent(char *desc, struct ext2fs_extent *extent)
{
	if (desc)
		printf("%s: ", desc);
	printf("extent: lblk %llu--%llu, len %u, pblk %llu, flags: ",
	       extent->e_lblk, extent->e_lblk + extent->e_len - 1,
	       extent->e_len, extent->e_pblk);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_LEAF)
		fputs("LEAF ", stdout);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_UNINIT)
		fputs("UNINIT ", stdout);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
		fputs("2ND_VISIT ", stdout);
	if (!extent->e_flags)
		fputs("(none)", stdout);
	fputc('\n', stdout);

}
#else
#define dbg_print_extent(desc, ex)	do { } while (0)
#define dbg_printf(f, a...)		do { } while (0)
#endif

static errcode_t ext2fs_punch_extent(ext2_filsys fs, ext2_ino_t ino,
				     struct ext2_inode *inode,
				     blk64_t start, blk64_t end)
{
	ext2_extent_handle_t	handle = 0;
	struct ext2fs_extent	extent;
	errcode_t		retval;
	blk64_t			free_start, next;
	__u32			free_count, newlen;
	int			freed = 0;

	retval = ext2fs_extent_open2(fs, ino, inode, &handle);
	if (retval)
		return retval;
	ext2fs_extent_goto(handle, start);
	retval = ext2fs_extent_get(handle, EXT2_EXTENT_CURRENT, &extent);
	if (retval)
		goto errout;
	while (1) {
		dbg_print_extent("main loop", &extent);
		next = extent.e_lblk + extent.e_len;
		dbg_printf("start %llu, end %llu, next %llu\n",
			   (unsigned long long) start,
			   (unsigned long long) end,
			   (unsigned long long) next);
		if (start <= extent.e_lblk) {
			if (end < extent.e_lblk)
				goto next_extent;
			dbg_printf("Case #%d\n", 1);
			/* Start of deleted region before extent; 
			   adjust beginning of extent */
			free_start = extent.e_pblk;
			if (next > end)
				free_count = end - extent.e_lblk + 1;
			else
				free_count = extent.e_len;
			extent.e_len -= free_count;
			extent.e_lblk += free_count;
			extent.e_pblk += free_count;
		} else if (end >= next-1) {
			if (start >= next)
				break;
			/* End of deleted region after extent;
			   adjust end of extent */
			dbg_printf("Case #%d\n", 2);
			newlen = start - extent.e_lblk;
			free_start = extent.e_pblk + newlen;
			free_count = extent.e_len - newlen;
			extent.e_len = newlen;
		} else {
			struct ext2fs_extent	newex;

			dbg_printf("Case #%d\n", 3);
			/* The hard case; we need to split the extent */
			newex.e_pblk = extent.e_pblk +
				(end + 1 - extent.e_lblk);
			newex.e_lblk = end + 1;
			newex.e_len = next - end - 1;
			newex.e_flags = extent.e_flags;

			extent.e_len = start - extent.e_lblk;
			free_start = extent.e_pblk + extent.e_len;
			free_count = end - start + 1;

			dbg_print_extent("inserting", &newex);
			retval = ext2fs_extent_insert(handle,
					EXT2_EXTENT_INSERT_AFTER, &newex);
			if (retval)
				goto errout;
			/* Now pointing at inserted extent; so go back */
			retval = ext2fs_extent_get(handle,
						   EXT2_EXTENT_PREV_LEAF,
						   &newex);
			if (retval)
				goto errout;
		} 
		if (extent.e_len) {
			dbg_print_extent("replacing", &extent);
			retval = ext2fs_extent_replace(handle, 0, &extent);
		} else {
			dbg_printf("deleting current extent%s\n", "");
			retval = ext2fs_extent_delete(handle, 0);
		}
		if (retval)
			goto errout;
		dbg_printf("Free start %llu, free count = %u\n",
		       free_start, free_count);
		while (free_count-- > 0) {
			ext2fs_block_alloc_stats(fs, free_start++, -1);
			freed++;
		}
	next_extent:
		retval = ext2fs_extent_get(handle, EXT2_EXTENT_NEXT_LEAF,
					   &extent);
		if (retval == EXT2_ET_EXTENT_NO_NEXT)
			break;
		if (retval)
			goto errout;
	}
	dbg_printf("Freed %d blocks\n", freed);
	retval = ext2fs_iblk_sub_blocks(fs, inode, freed);
errout:
	ext2fs_extent_free(handle);
	return retval;
}
	
/*
 * Deallocate all logical blocks starting at start to end, inclusive.
 * If end is ~0, then this is effectively truncate.
 */
extern errcode_t ext2fs_punch(ext2_filsys fs, ext2_ino_t ino,
			      struct ext2_inode *inode,
			      char *block_buf, blk64_t start,
			      blk64_t end)
{
	errcode_t		retval;
	struct ext2_inode	inode_buf;

	if (start > end)
		return EINVAL;

	if (start == end)
		return 0;

	/* Read inode structure if necessary */
	if (!inode) {
		retval = ext2fs_read_inode(fs, ino, &inode_buf);
		if (retval)
			return retval;
		inode = &inode_buf;
	}
	if (inode->i_flags & EXT4_EXTENTS_FL)
		retval = ext2fs_punch_extent(fs, ino, inode, start, end);
	else {
		blk_t	count;

		if (start > ~0U)
			return 0;
		count = ((end - start) < ~0U) ? (end - start) : ~0U;
		retval = ext2fs_punch_ind(fs, inode, block_buf, 
					  (blk_t) start, count);
	}
	if (retval)
		return retval;

	return ext2fs_write_inode(fs, ino, inode);
}
