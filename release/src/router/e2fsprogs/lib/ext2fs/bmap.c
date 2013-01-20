/*
 * bmap.c --- logical to physical block mapping
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
#include <errno.h>

#include "ext2_fs.h"
#include "ext2fs.h"

#if defined(__GNUC__) && !defined(NO_INLINE_FUNCS)
#define _BMAP_INLINE_	__inline__
#else
#define _BMAP_INLINE_
#endif

extern errcode_t ext2fs_bmap(ext2_filsys fs, ext2_ino_t ino,
			     struct ext2_inode *inode,
			     char *block_buf, int bmap_flags,
			     blk_t block, blk_t *phys_blk);

#define inode_bmap(inode, nr) ((inode)->i_block[(nr)])

static _BMAP_INLINE_ errcode_t block_ind_bmap(ext2_filsys fs, int flags,
					      blk_t ind, char *block_buf,
					      int *blocks_alloc,
					      blk_t nr, blk_t *ret_blk)
{
	errcode_t	retval;
	blk_t		b;

	if (!ind) {
		if (flags & BMAP_SET)
			return EXT2_ET_SET_BMAP_NO_IND;
		*ret_blk = 0;
		return 0;
	}
	retval = io_channel_read_blk(fs->io, ind, 1, block_buf);
	if (retval)
		return retval;

	if (flags & BMAP_SET) {
		b = *ret_blk;
#ifdef WORDS_BIGENDIAN
		b = ext2fs_swab32(b);
#endif
		((blk_t *) block_buf)[nr] = b;
		return io_channel_write_blk(fs->io, ind, 1, block_buf);
	}

	b = ((blk_t *) block_buf)[nr];

#ifdef WORDS_BIGENDIAN
	b = ext2fs_swab32(b);
#endif

	if (!b && (flags & BMAP_ALLOC)) {
		b = nr ? ((blk_t *) block_buf)[nr-1] : 0;
		retval = ext2fs_alloc_block(fs, b,
					    block_buf + fs->blocksize, &b);
		if (retval)
			return retval;

#ifdef WORDS_BIGENDIAN
		((blk_t *) block_buf)[nr] = ext2fs_swab32(b);
#else
		((blk_t *) block_buf)[nr] = b;
#endif

		retval = io_channel_write_blk(fs->io, ind, 1, block_buf);
		if (retval)
			return retval;

		(*blocks_alloc)++;
	}

	*ret_blk = b;
	return 0;
}

static _BMAP_INLINE_ errcode_t block_dind_bmap(ext2_filsys fs, int flags,
					       blk_t dind, char *block_buf,
					       int *blocks_alloc,
					       blk_t nr, blk_t *ret_blk)
{
	blk_t		b;
	errcode_t	retval;
	blk_t		addr_per_block;

	addr_per_block = (blk_t) fs->blocksize >> 2;

	retval = block_ind_bmap(fs, flags & ~BMAP_SET, dind, block_buf,
				blocks_alloc, nr / addr_per_block, &b);
	if (retval)
		return retval;
	retval = block_ind_bmap(fs, flags, b, block_buf, blocks_alloc,
				nr % addr_per_block, ret_blk);
	return retval;
}

static _BMAP_INLINE_ errcode_t block_tind_bmap(ext2_filsys fs, int flags,
					       blk_t tind, char *block_buf,
					       int *blocks_alloc,
					       blk_t nr, blk_t *ret_blk)
{
	blk_t		b;
	errcode_t	retval;
	blk_t		addr_per_block;

	addr_per_block = (blk_t) fs->blocksize >> 2;

	retval = block_dind_bmap(fs, flags & ~BMAP_SET, tind, block_buf,
				 blocks_alloc, nr / addr_per_block, &b);
	if (retval)
		return retval;
	retval = block_ind_bmap(fs, flags, b, block_buf, blocks_alloc,
				nr % addr_per_block, ret_blk);
	return retval;
}

errcode_t ext2fs_bmap2(ext2_filsys fs, ext2_ino_t ino, struct ext2_inode *inode,
		       char *block_buf, int bmap_flags, blk64_t block,
		       int *ret_flags, blk64_t *phys_blk)
{
	struct ext2_inode inode_buf;
	ext2_extent_handle_t handle = 0;
	blk_t addr_per_block;
	blk_t	b, blk32;
	char	*buf = 0;
	errcode_t	retval = 0;
	int		blocks_alloc = 0, inode_dirty = 0;

	if (!(bmap_flags & BMAP_SET))
		*phys_blk = 0;

	if (ret_flags)
		*ret_flags = 0;

	/* Read inode structure if necessary */
	if (!inode) {
		retval = ext2fs_read_inode(fs, ino, &inode_buf);
		if (retval)
			return retval;
		inode = &inode_buf;
	}
	addr_per_block = (blk_t) fs->blocksize >> 2;

	if (inode->i_flags & EXT4_EXTENTS_FL) {
		struct ext2fs_extent	extent;
		unsigned int		offset;

		retval = ext2fs_extent_open2(fs, ino, inode, &handle);
		if (retval)
			goto done;
		if (bmap_flags & BMAP_SET) {
			retval = ext2fs_extent_set_bmap(handle, block,
							*phys_blk, 0);
			goto done;
		}
		retval = ext2fs_extent_goto(handle, block);
		if (retval) {
			/* If the extent is not found, return phys_blk = 0 */
			if (retval == EXT2_ET_EXTENT_NOT_FOUND)
				goto got_block;
			goto done;
		}
		retval = ext2fs_extent_get(handle, EXT2_EXTENT_CURRENT, &extent);
		if (retval)
			goto done;
		offset = block - extent.e_lblk;
		if (block >= extent.e_lblk && (offset <= extent.e_len)) {
			*phys_blk = extent.e_pblk + offset;
			if (ret_flags && extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT)
				*ret_flags |= BMAP_RET_UNINIT;
		}
	got_block:
		if ((*phys_blk == 0) && (bmap_flags & BMAP_ALLOC)) {
			retval = ext2fs_alloc_block(fs, b, block_buf, &b);
			if (retval)
				goto done;
			retval = ext2fs_extent_set_bmap(handle, block,
							(blk64_t) b, 0);
			if (retval)
				goto done;
			/* Update inode after setting extent */
			retval = ext2fs_read_inode(fs, ino, inode);
			if (retval)
				return retval;
			blocks_alloc++;
			*phys_blk = b;
		}
		retval = 0;
		goto done;
	}

	if (!block_buf) {
		retval = ext2fs_get_array(2, fs->blocksize, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}

	if (block < EXT2_NDIR_BLOCKS) {
		if (bmap_flags & BMAP_SET) {
			b = *phys_blk;
			inode_bmap(inode, block) = b;
			inode_dirty++;
			goto done;
		}

		*phys_blk = inode_bmap(inode, block);
		b = block ? inode_bmap(inode, block-1) : 0;

		if ((*phys_blk == 0) && (bmap_flags & BMAP_ALLOC)) {
			retval = ext2fs_alloc_block(fs, b, block_buf, &b);
			if (retval)
				goto done;
			inode_bmap(inode, block) = b;
			blocks_alloc++;
			*phys_blk = b;
		}
		goto done;
	}

	/* Indirect block */
	block -= EXT2_NDIR_BLOCKS;
	blk32 = *phys_blk;
	if (block < addr_per_block) {
		b = inode_bmap(inode, EXT2_IND_BLOCK);
		if (!b) {
			if (!(bmap_flags & BMAP_ALLOC)) {
				if (bmap_flags & BMAP_SET)
					retval = EXT2_ET_SET_BMAP_NO_IND;
				goto done;
			}

			b = inode_bmap(inode, EXT2_IND_BLOCK-1);
 			retval = ext2fs_alloc_block(fs, b, block_buf, &b);
			if (retval)
				goto done;
			inode_bmap(inode, EXT2_IND_BLOCK) = b;
			blocks_alloc++;
		}
		retval = block_ind_bmap(fs, bmap_flags, b, block_buf,
					&blocks_alloc, block, &blk32);
		if (retval == 0)
			*phys_blk = blk32;
		goto done;
	}

	/* Doubly indirect block  */
	block -= addr_per_block;
	if (block < addr_per_block * addr_per_block) {
		b = inode_bmap(inode, EXT2_DIND_BLOCK);
		if (!b) {
			if (!(bmap_flags & BMAP_ALLOC)) {
				if (bmap_flags & BMAP_SET)
					retval = EXT2_ET_SET_BMAP_NO_IND;
				goto done;
			}

			b = inode_bmap(inode, EXT2_IND_BLOCK);
 			retval = ext2fs_alloc_block(fs, b, block_buf, &b);
			if (retval)
				goto done;
			inode_bmap(inode, EXT2_DIND_BLOCK) = b;
			blocks_alloc++;
		}
		retval = block_dind_bmap(fs, bmap_flags, b, block_buf,
					 &blocks_alloc, block, &blk32);
		if (retval == 0)
			*phys_blk = blk32;
		goto done;
	}

	/* Triply indirect block */
	block -= addr_per_block * addr_per_block;
	b = inode_bmap(inode, EXT2_TIND_BLOCK);
	if (!b) {
		if (!(bmap_flags & BMAP_ALLOC)) {
			if (bmap_flags & BMAP_SET)
				retval = EXT2_ET_SET_BMAP_NO_IND;
			goto done;
		}

		b = inode_bmap(inode, EXT2_DIND_BLOCK);
		retval = ext2fs_alloc_block(fs, b, block_buf, &b);
		if (retval)
			goto done;
		inode_bmap(inode, EXT2_TIND_BLOCK) = b;
		blocks_alloc++;
	}
	retval = block_tind_bmap(fs, bmap_flags, b, block_buf,
				 &blocks_alloc, block, &blk32);
	if (retval == 0)
		*phys_blk = blk32;
done:
	if (buf)
		ext2fs_free_mem(&buf);
	if (handle)
		ext2fs_extent_free(handle);
	if ((retval == 0) && (blocks_alloc || inode_dirty)) {
		ext2fs_iblk_add_blocks(fs, inode, blocks_alloc);
		retval = ext2fs_write_inode(fs, ino, inode);
	}
	return retval;
}

errcode_t ext2fs_bmap(ext2_filsys fs, ext2_ino_t ino, struct ext2_inode *inode,
		      char *block_buf, int bmap_flags, blk_t block,
		      blk_t *phys_blk)
{
	errcode_t ret;
	blk64_t	ret_blk = *phys_blk;

	ret = ext2fs_bmap2(fs, ino, inode, block_buf, bmap_flags, block,
			    0, &ret_blk);
	if (ret)
		return ret;
	if (ret_blk >= ((long long) 1 << 32))
		return EOVERFLOW;
	*phys_blk = ret_blk;
	return 0;
}
