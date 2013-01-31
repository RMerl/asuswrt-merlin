/* vi: set sw=4 ts=4: */
/*
 * closefs.c --- close an ext2 filesystem
 *
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>
#include <string.h>

#include "ext2_fs.h"
#include "ext2fsP.h"

static int test_root(int a, int b)
{
	if (a == 0)
		return 1;
	while (1) {
		if (a == 1)
			return 1;
		if (a % b)
			return 0;
		a = a / b;
	}
}

int ext2fs_bg_has_super(ext2_filsys fs, int group_block)
{
	if (!(fs->super->s_feature_ro_compat &
	      EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
		return 1;

	if (test_root(group_block, 3) || (test_root(group_block, 5)) ||
	    test_root(group_block, 7))
		return 1;

	return 0;
}

int ext2fs_super_and_bgd_loc(ext2_filsys fs,
			     dgrp_t group,
			     blk_t *ret_super_blk,
			     blk_t *ret_old_desc_blk,
			     blk_t *ret_new_desc_blk,
			     int *ret_meta_bg)
{
	blk_t	group_block, super_blk = 0, old_desc_blk = 0, new_desc_blk = 0;
	unsigned int meta_bg, meta_bg_size;
	int	numblocks, has_super;
	int	old_desc_blocks;

	group_block = fs->super->s_first_data_block +
		(group * fs->super->s_blocks_per_group);

	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks =
			fs->desc_blocks + fs->super->s_reserved_gdt_blocks;

	if (group == fs->group_desc_count-1) {
		numblocks = (fs->super->s_blocks_count -
			     fs->super->s_first_data_block) %
			fs->super->s_blocks_per_group;
		if (!numblocks)
			numblocks = fs->super->s_blocks_per_group;
	} else
		numblocks = fs->super->s_blocks_per_group;

	has_super = ext2fs_bg_has_super(fs, group);

	if (has_super) {
		super_blk = group_block;
		numblocks--;
	}
	meta_bg_size = (fs->blocksize / sizeof (struct ext2_group_desc));
	meta_bg = group / meta_bg_size;

	if (!(fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG) ||
	    (meta_bg < fs->super->s_first_meta_bg)) {
		if (has_super) {
			old_desc_blk = group_block + 1;
			numblocks -= old_desc_blocks;
		}
	} else {
		if (((group % meta_bg_size) == 0) ||
		    ((group % meta_bg_size) == 1) ||
		    ((group % meta_bg_size) == (meta_bg_size-1))) {
			if (has_super)
				has_super = 1;
			new_desc_blk = group_block + has_super;
			numblocks--;
		}
	}

	numblocks -= 2 + fs->inode_blocks_per_group;

	if (ret_super_blk)
		*ret_super_blk = super_blk;
	if (ret_old_desc_blk)
		*ret_old_desc_blk = old_desc_blk;
	if (ret_new_desc_blk)
		*ret_new_desc_blk = new_desc_blk;
	if (ret_meta_bg)
		*ret_meta_bg = meta_bg;
	return numblocks;
}


/*
 * This function forces out the primary superblock.  We need to only
 * write out those fields which we have changed, since if the
 * filesystem is mounted, it may have changed some of the other
 * fields.
 *
 * It takes as input a superblock which has already been byte swapped
 * (if necessary).
 *
 */
static errcode_t write_primary_superblock(ext2_filsys fs,
					  struct ext2_super_block *super)
{
	__u16		*old_super, *new_super;
	int		check_idx, write_idx, size;
	errcode_t	retval;

	if (!fs->io->manager->write_byte || !fs->orig_super) {
		io_channel_set_blksize(fs->io, SUPERBLOCK_OFFSET);
		retval = io_channel_write_blk(fs->io, 1, -SUPERBLOCK_SIZE,
					      super);
		io_channel_set_blksize(fs->io, fs->blocksize);
		return retval;
	}

	old_super = (__u16 *) fs->orig_super;
	new_super = (__u16 *) super;

	for (check_idx = 0; check_idx < SUPERBLOCK_SIZE/2; check_idx++) {
		if (old_super[check_idx] == new_super[check_idx])
			continue;
		write_idx = check_idx;
		for (check_idx++; check_idx < SUPERBLOCK_SIZE/2; check_idx++)
			if (old_super[check_idx] == new_super[check_idx])
				break;
		size = 2 * (check_idx - write_idx);
		retval = io_channel_write_byte(fs->io,
			       SUPERBLOCK_OFFSET + (2 * write_idx), size,
					       new_super + write_idx);
		if (retval)
			return retval;
	}
	memcpy(fs->orig_super, super, SUPERBLOCK_SIZE);
	return 0;
}


/*
 * Updates the revision to EXT2_DYNAMIC_REV
 */
void ext2fs_update_dynamic_rev(ext2_filsys fs)
{
	struct ext2_super_block *sb = fs->super;

	if (sb->s_rev_level > EXT2_GOOD_OLD_REV)
		return;

	sb->s_rev_level = EXT2_DYNAMIC_REV;
	sb->s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	sb->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
	/* s_uuid is handled by e2fsck already */
	/* other fields should be left alone */
}

static errcode_t write_backup_super(ext2_filsys fs, dgrp_t group,
				    blk_t group_block,
				    struct ext2_super_block *super_shadow)
{
	dgrp_t	sgrp = group;

	if (sgrp > ((1 << 16) - 1))
		sgrp = (1 << 16) - 1;
#if BB_BIG_ENDIAN
	if (fs->flags & EXT2_FLAG_SWAP_BYTES)
		super_shadow->s_block_group_nr = ext2fs_swab16(sgrp);
	else
#endif
		fs->super->s_block_group_nr = sgrp;

	return io_channel_write_blk(fs->io, group_block, -SUPERBLOCK_SIZE,
				    super_shadow);
}


errcode_t ext2fs_flush(ext2_filsys fs)
{
	dgrp_t		i;
	blk_t		group_block;
	errcode_t	retval;
	unsigned long	fs_state;
	struct ext2_super_block *super_shadow = NULL;
	struct ext2_group_desc *group_shadow = NULL;
	char	*group_ptr;
	int	old_desc_blocks;
#if BB_BIG_ENDIAN
	dgrp_t		j;
	struct ext2_group_desc *s, *t;
#endif

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	fs_state = fs->super->s_state;

	fs->super->s_wtime = time(NULL);
	fs->super->s_block_group_nr = 0;
#if BB_BIG_ENDIAN
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		retval = EXT2_ET_NO_MEMORY;
		retval = ext2fs_get_mem(SUPERBLOCK_SIZE, &super_shadow);
		if (retval)
			goto errout;
		retval = ext2fs_get_mem((size_t)(fs->blocksize *
						 fs->desc_blocks),
					&group_shadow);
		if (retval)
			goto errout;
		memset(group_shadow, 0, (size_t) fs->blocksize *
		       fs->desc_blocks);

		/* swap the group descriptors */
		for (j=0, s=fs->group_desc, t=group_shadow;
		     j < fs->group_desc_count; j++, t++, s++) {
			*t = *s;
			ext2fs_swap_group_desc(t);
		}
	} else {
		super_shadow = fs->super;
		group_shadow = fs->group_desc;
	}
#else
	super_shadow = fs->super;
	group_shadow = fs->group_desc;
#endif

	/*
	 * If this is an external journal device, don't write out the
	 * block group descriptors or any of the backup superblocks
	 */
	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
		goto write_primary_superblock_only;

	/*
	 * Set the state of the FS to be non-valid.  (The state has
	 * already been backed up earlier, and will be restored after
	 * we write out the backup superblocks.)
	 */
	fs->super->s_state &= ~EXT2_VALID_FS;
#if BB_BIG_ENDIAN
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		*super_shadow = *fs->super;
		ext2fs_swap_super(super_shadow);
	}
#endif

	/*
	 * Write out the master group descriptors, and the backup
	 * superblocks and group descriptors.
	 */
	group_block = fs->super->s_first_data_block;
	group_ptr = (char *) group_shadow;
	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks;

	for (i = 0; i < fs->group_desc_count; i++) {
		blk_t	super_blk, old_desc_blk, new_desc_blk;
		int	meta_bg;

		ext2fs_super_and_bgd_loc(fs, i, &super_blk, &old_desc_blk,
					 &new_desc_blk, &meta_bg);

		if (!(fs->flags & EXT2_FLAG_MASTER_SB_ONLY) &&i && super_blk) {
			retval = write_backup_super(fs, i, super_blk,
						    super_shadow);
			if (retval)
				goto errout;
		}
		if (fs->flags & EXT2_FLAG_SUPER_ONLY)
			continue;
		if ((old_desc_blk) &&
		    (!(fs->flags & EXT2_FLAG_MASTER_SB_ONLY) || (i == 0))) {
			retval = io_channel_write_blk(fs->io,
			      old_desc_blk, old_desc_blocks, group_ptr);
			if (retval)
				goto errout;
		}
		if (new_desc_blk) {
			retval = io_channel_write_blk(fs->io, new_desc_blk,
				1, group_ptr + (meta_bg*fs->blocksize));
			if (retval)
				goto errout;
		}
	}
	fs->super->s_block_group_nr = 0;
	fs->super->s_state = fs_state;
#if BB_BIG_ENDIAN
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		*super_shadow = *fs->super;
		ext2fs_swap_super(super_shadow);
	}
#endif

	/*
	 * If the write_bitmaps() function is present, call it to
	 * flush the bitmaps.  This is done this way so that a simple
	 * program that doesn't mess with the bitmaps doesn't need to
	 * drag in the bitmaps.c code.
	 */
	if (fs->write_bitmaps) {
		retval = fs->write_bitmaps(fs);
		if (retval)
			goto errout;
	}

write_primary_superblock_only:
	/*
	 * Write out master superblock.  This has to be done
	 * separately, since it is located at a fixed location
	 * (SUPERBLOCK_OFFSET).  We flush all other pending changes
	 * out to disk first, just to avoid a race condition with an
	 * insy-tinsy window....
	 */
	retval = io_channel_flush(fs->io);
	retval = write_primary_superblock(fs, super_shadow);
	if (retval)
		goto errout;

	fs->flags &= ~EXT2_FLAG_DIRTY;

	retval = io_channel_flush(fs->io);
errout:
	fs->super->s_state = fs_state;
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		if (super_shadow)
			ext2fs_free_mem(&super_shadow);
		if (group_shadow)
			ext2fs_free_mem(&group_shadow);
	}
	return retval;
}

errcode_t ext2fs_close(ext2_filsys fs)
{
	errcode_t	retval;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (fs->flags & EXT2_FLAG_DIRTY) {
		retval = ext2fs_flush(fs);
		if (retval)
			return retval;
	}
	if (fs->write_bitmaps) {
		retval = fs->write_bitmaps(fs);
		if (retval)
			return retval;
	}
	ext2fs_free(fs);
	return 0;
}
