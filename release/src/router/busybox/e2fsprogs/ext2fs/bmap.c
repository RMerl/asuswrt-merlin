/* vi: set sw=4 ts=4: */
/*
 * bmap.c --- logical to physical block mapping
 *
 * Copyright (C) 1997 Theodore Ts'o.
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

#include "ext2_fs.h"
#include "ext2fs.h"

extern errcode_t ext2fs_bmap(ext2_filsys fs, ext2_ino_t ino,
			     struct ext2_inode *inode,
			     char *block_buf, int bmap_flags,
			     blk_t block, blk_t *phys_blk);

#define inode_bmap(inode, nr) ((inode)->i_block[(nr)])

static errcode_t block_ind_bmap(ext2_filsys fs, int flags,
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
#if BB_BIG_ENDIAN
		if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
		    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE))
			b = ext2fs_swab32(b);
#endif
		((blk_t *) block_buf)[nr] = b;
		return io_channel_write_blk(fs->io, ind, 1, block_buf);
	}

	b = ((blk_t *) block_buf)[nr];

#if BB_BIG_ENDIAN
	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
		b = ext2fs_swab32(b);
#endif

	if (!b && (flags & BMAP_ALLOC)) {
		b = nr ? ((blk_t *) block_buf)[nr-1] : 0;
		retval = ext2fs_alloc_block(fs, b,
					    block_buf + fs->blocksize, &b);
		if (retval)
			return retval;

#if BB_BIG_ENDIAN
		if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
		    (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE))
			((blk_t *) block_buf)[nr] = ext2fs_swab32(b);
		else
#endif
			((blk_t *) block_buf)[nr] = b;

		retval = io_channel_write_blk(fs->io, ind, 1, block_buf);
		if (retval)
			return retval;

		(*blocks_alloc)++;
	}

	*ret_blk = b;
	return 0;
}

static errcode_t block_dind_bmap(ext2_filsys fs, int flags,
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

static errcode_t block_tind_bmap(ext2_filsys fs, int flags,
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

errcode_t ext2fs_bmap(ext2_filsys fs, ext2_ino_t ino, struct ext2_inode *inode,
		      char *block_buf, int bmap_flags, blk_t block,
		      blk_t *phys_blk)
{
	struct ext2_inode inode_buf;
	blk_t addr_per_block;
	blk_t	b;
	char	*buf = NULL;
	errcode_t	retval = 0;
	int		blocks_alloc = 0, inode_dirty = 0;

	if (!(bmap_flags & BMAP_SET))
		*phys_blk = 0;

	/* Read inode structure if necessary */
	if (!inode) {
		retval = ext2fs_read_inode(fs, ino, &inode_buf);
		if (retval)
			return retval;
		inode = &inode_buf;
	}
	addr_per_block = (blk_t) fs->blocksize >> 2;

	if (!block_buf) {
		retval = ext2fs_get_mem(fs->blocksize * 2, &buf);
		if (retval)
			return retval;
		block_buf = buf;
	}

	if (block < EXT2_NDIR_BLOCKS) {
		if (bmap_flags & BMAP_SET) {
			b = *phys_blk;
#if BB_BIG_ENDIAN
			if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
			    (fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
				b = ext2fs_swab32(b);
#endif
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
					&blocks_alloc, block, phys_blk);
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
					 &blocks_alloc, block, phys_blk);
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
				 &blocks_alloc, block, phys_blk);
done:
	ext2fs_free_mem(&buf);
	if ((retval == 0) && (blocks_alloc || inode_dirty)) {
		inode->i_blocks += (blocks_alloc * fs->blocksize) / 512;
		retval = ext2fs_write_inode(fs, ino, inode);
	}
	return retval;
}
