/* vi: set sw=4 ts=4: */
/*
 * initialize.c --- initialize a filesystem handle given superblock
 *	parameters.  Used by mke2fs when initializing a filesystem.
 *
 * Copyright (C) 1994, 1995, 1996 Theodore Ts'o.
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

#if defined(__linux__)    &&	defined(EXT2_OS_LINUX)
#define CREATOR_OS EXT2_OS_LINUX
#else
#if defined(__GNU__)     &&	defined(EXT2_OS_HURD)
#define CREATOR_OS EXT2_OS_HURD
#else
#if defined(__FreeBSD__) &&	defined(EXT2_OS_FREEBSD)
#define CREATOR_OS EXT2_OS_FREEBSD
#else
#if defined(LITES)	   &&	defined(EXT2_OS_LITES)
#define CREATOR_OS EXT2_OS_LITES
#else
#define CREATOR_OS EXT2_OS_LINUX /* by default */
#endif /* defined(LITES) && defined(EXT2_OS_LITES) */
#endif /* defined(__FreeBSD__) && defined(EXT2_OS_FREEBSD) */
#endif /* defined(__GNU__)     && defined(EXT2_OS_HURD) */
#endif /* defined(__linux__)   && defined(EXT2_OS_LINUX) */

/*
 * Note we override the kernel include file's idea of what the default
 * check interval (never) should be.  It's a good idea to check at
 * least *occasionally*, specially since servers will never rarely get
 * to reboot, since Linux is so robust these days.  :-)
 *
 * 180 days (six months) seems like a good value.
 */
#ifdef EXT2_DFL_CHECKINTERVAL
#undef EXT2_DFL_CHECKINTERVAL
#endif
#define EXT2_DFL_CHECKINTERVAL (86400L * 180L)

/*
 * Calculate the number of GDT blocks to reserve for online filesystem growth.
 * The absolute maximum number of GDT blocks we can reserve is determined by
 * the number of block pointers that can fit into a single block.
 */
static int calc_reserved_gdt_blocks(ext2_filsys fs)
{
	struct ext2_super_block *sb = fs->super;
	unsigned long bpg = sb->s_blocks_per_group;
	unsigned int gdpb = fs->blocksize / sizeof(struct ext2_group_desc);
	unsigned long max_blocks = 0xffffffff;
	unsigned long rsv_groups;
	int rsv_gdb;

	/* We set it at 1024x the current filesystem size, or
	 * the upper block count limit (2^32), whichever is lower.
	 */
	if (sb->s_blocks_count < max_blocks / 1024)
		max_blocks = sb->s_blocks_count * 1024;
	rsv_groups = (max_blocks - sb->s_first_data_block + bpg - 1) / bpg;
	rsv_gdb = (rsv_groups + gdpb - 1) / gdpb - fs->desc_blocks;
	if (rsv_gdb > EXT2_ADDR_PER_BLOCK(sb))
		rsv_gdb = EXT2_ADDR_PER_BLOCK(sb);
#ifdef RES_GDT_DEBUG
	printf("max_blocks %lu, rsv_groups = %lu, rsv_gdb = %lu\n",
	       max_blocks, rsv_groups, rsv_gdb);
#endif

	return rsv_gdb;
}

errcode_t ext2fs_initialize(const char *name, int flags,
			    struct ext2_super_block *param,
			    io_manager manager, ext2_filsys *ret_fs)
{
	ext2_filsys	fs;
	errcode_t	retval;
	struct ext2_super_block *super;
	int		frags_per_block;
	unsigned int	rem;
	unsigned int	overhead = 0;
	blk_t		group_block;
	unsigned int	ipg;
	dgrp_t		i;
	blk_t		numblocks;
	int		rsv_gdt;
	char		*buf;

	if (!param || !param->s_blocks_count)
		return EXT2_ET_INVALID_ARGUMENT;

	retval = ext2fs_get_mem(sizeof(struct struct_ext2_filsys), &fs);
	if (retval)
		return retval;

	memset(fs, 0, sizeof(struct struct_ext2_filsys));
	fs->magic = EXT2_ET_MAGIC_EXT2FS_FILSYS;
	fs->flags = flags | EXT2_FLAG_RW;
	fs->umask = 022;
#ifdef WORDS_BIGENDIAN
	fs->flags |= EXT2_FLAG_SWAP_BYTES;
#endif
	retval = manager->open(name, IO_FLAG_RW, &fs->io);
	if (retval)
		goto cleanup;
	fs->image_io = fs->io;
	fs->io->app_data = fs;
	retval = ext2fs_get_mem(strlen(name)+1, &fs->device_name);
	if (retval)
		goto cleanup;

	strcpy(fs->device_name, name);
	retval = ext2fs_get_mem(SUPERBLOCK_SIZE, &super);
	if (retval)
		goto cleanup;
	fs->super = super;

	memset(super, 0, SUPERBLOCK_SIZE);

#define set_field(field, default) (super->field = param->field ? \
				   param->field : (default))

	super->s_magic = EXT2_SUPER_MAGIC;
	super->s_state = EXT2_VALID_FS;

	set_field(s_log_block_size, 0);	/* default blocksize: 1024 bytes */
	set_field(s_log_frag_size, 0); /* default fragsize: 1024 bytes */
	set_field(s_first_data_block, super->s_log_block_size ? 0 : 1);
	set_field(s_max_mnt_count, EXT2_DFL_MAX_MNT_COUNT);
	set_field(s_errors, EXT2_ERRORS_DEFAULT);
	set_field(s_feature_compat, 0);
	set_field(s_feature_incompat, 0);
	set_field(s_feature_ro_compat, 0);
	set_field(s_first_meta_bg, 0);
	if (super->s_feature_incompat & ~EXT2_LIB_FEATURE_INCOMPAT_SUPP) {
		retval = EXT2_ET_UNSUPP_FEATURE;
		goto cleanup;
	}
	if (super->s_feature_ro_compat & ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP) {
		retval = EXT2_ET_RO_UNSUPP_FEATURE;
		goto cleanup;
	}

	set_field(s_rev_level, EXT2_GOOD_OLD_REV);
	if (super->s_rev_level >= EXT2_DYNAMIC_REV) {
		set_field(s_first_ino, EXT2_GOOD_OLD_FIRST_INO);
		set_field(s_inode_size, EXT2_GOOD_OLD_INODE_SIZE);
	}

	set_field(s_checkinterval, EXT2_DFL_CHECKINTERVAL);
	super->s_mkfs_time = super->s_lastcheck = time(NULL);

	super->s_creator_os = CREATOR_OS;

	fs->blocksize = EXT2_BLOCK_SIZE(super);
	fs->fragsize = EXT2_FRAG_SIZE(super);
	frags_per_block = fs->blocksize / fs->fragsize;

	/* default: (fs->blocksize*8) blocks/group, up to 2^16 (GDT limit) */
	set_field(s_blocks_per_group, fs->blocksize * 8);
	if (super->s_blocks_per_group > EXT2_MAX_BLOCKS_PER_GROUP(super))
		super->s_blocks_per_group = EXT2_MAX_BLOCKS_PER_GROUP(super);
	super->s_frags_per_group = super->s_blocks_per_group * frags_per_block;

	super->s_blocks_count = param->s_blocks_count;
	super->s_r_blocks_count = param->s_r_blocks_count;
	if (super->s_r_blocks_count >= param->s_blocks_count) {
		retval = EXT2_ET_INVALID_ARGUMENT;
		goto cleanup;
	}

	/*
	 * If we're creating an external journal device, we don't need
	 * to bother with the rest.
	 */
	if (super->s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		fs->group_desc_count = 0;
		ext2fs_mark_super_dirty(fs);
		*ret_fs = fs;
		return 0;
	}

retry:
	fs->group_desc_count = (super->s_blocks_count -
				super->s_first_data_block +
				EXT2_BLOCKS_PER_GROUP(super) - 1)
		/ EXT2_BLOCKS_PER_GROUP(super);
	if (fs->group_desc_count == 0) {
		retval = EXT2_ET_TOOSMALL;
		goto cleanup;
	}
	fs->desc_blocks = (fs->group_desc_count +
			   EXT2_DESC_PER_BLOCK(super) - 1)
		/ EXT2_DESC_PER_BLOCK(super);

	i = fs->blocksize >= 4096 ? 1 : 4096 / fs->blocksize;
	set_field(s_inodes_count, super->s_blocks_count / i);

	/*
	 * Make sure we have at least EXT2_FIRST_INO + 1 inodes, so
	 * that we have enough inodes for the filesystem(!)
	 */
	if (super->s_inodes_count < EXT2_FIRST_INODE(super)+1)
		super->s_inodes_count = EXT2_FIRST_INODE(super)+1;

	/*
	 * There should be at least as many inodes as the user
	 * requested.  Figure out how many inodes per group that
	 * should be.  But make sure that we don't allocate more than
	 * one bitmap's worth of inodes each group.
	 */
	ipg = (super->s_inodes_count + fs->group_desc_count - 1) /
		fs->group_desc_count;
	if (ipg > fs->blocksize * 8) {
		if (super->s_blocks_per_group >= 256) {
			/* Try again with slightly different parameters */
			super->s_blocks_per_group -= 8;
			super->s_blocks_count = param->s_blocks_count;
			super->s_frags_per_group = super->s_blocks_per_group *
				frags_per_block;
			goto retry;
		} else
			return EXT2_ET_TOO_MANY_INODES;
	}

	if (ipg > (unsigned) EXT2_MAX_INODES_PER_GROUP(super))
		ipg = EXT2_MAX_INODES_PER_GROUP(super);

	super->s_inodes_per_group = ipg;
	if (super->s_inodes_count > ipg * fs->group_desc_count)
		super->s_inodes_count = ipg * fs->group_desc_count;

	/*
	 * Make sure the number of inodes per group completely fills
	 * the inode table blocks in the descriptor.  If not, add some
	 * additional inodes/group.  Waste not, want not...
	 */
	fs->inode_blocks_per_group = (((super->s_inodes_per_group *
					EXT2_INODE_SIZE(super)) +
				       EXT2_BLOCK_SIZE(super) - 1) /
				      EXT2_BLOCK_SIZE(super));
	super->s_inodes_per_group = ((fs->inode_blocks_per_group *
				      EXT2_BLOCK_SIZE(super)) /
				     EXT2_INODE_SIZE(super));
	/*
	 * Finally, make sure the number of inodes per group is a
	 * multiple of 8.  This is needed to simplify the bitmap
	 * splicing code.
	 */
	super->s_inodes_per_group &= ~7;
	fs->inode_blocks_per_group = (((super->s_inodes_per_group *
					EXT2_INODE_SIZE(super)) +
				       EXT2_BLOCK_SIZE(super) - 1) /
				      EXT2_BLOCK_SIZE(super));

	/*
	 * adjust inode count to reflect the adjusted inodes_per_group
	 */
	super->s_inodes_count = super->s_inodes_per_group *
		fs->group_desc_count;
	super->s_free_inodes_count = super->s_inodes_count;

	/*
	 * check the number of reserved group descriptor table blocks
	 */
	if (super->s_feature_compat & EXT2_FEATURE_COMPAT_RESIZE_INODE)
		rsv_gdt = calc_reserved_gdt_blocks(fs);
	else
		rsv_gdt = 0;
	set_field(s_reserved_gdt_blocks, rsv_gdt);
	if (super->s_reserved_gdt_blocks > EXT2_ADDR_PER_BLOCK(super)) {
		retval = EXT2_ET_RES_GDT_BLOCKS;
		goto cleanup;
	}

	/*
	 * Overhead is the number of bookkeeping blocks per group.  It
	 * includes the superblock backup, the group descriptor
	 * backups, the inode bitmap, the block bitmap, and the inode
	 * table.
	 */

	overhead = (int) (2 + fs->inode_blocks_per_group);

	if (ext2fs_bg_has_super(fs, fs->group_desc_count - 1))
		overhead += 1 + fs->desc_blocks + super->s_reserved_gdt_blocks;

	/* This can only happen if the user requested too many inodes */
	if (overhead > super->s_blocks_per_group)
		return EXT2_ET_TOO_MANY_INODES;

	/*
	 * See if the last group is big enough to support the
	 * necessary data structures.  If not, we need to get rid of
	 * it.
	 */
	rem = ((super->s_blocks_count - super->s_first_data_block) %
	       super->s_blocks_per_group);
	if ((fs->group_desc_count == 1) && rem && (rem < overhead))
		return EXT2_ET_TOOSMALL;
	if (rem && (rem < overhead+50)) {
		super->s_blocks_count -= rem;
		goto retry;
	}

	/*
	 * At this point we know how big the filesystem will be.  So
	 * we can do any and all allocations that depend on the block
	 * count.
	 */

	retval = ext2fs_get_mem(strlen(fs->device_name) + 80, &buf);
	if (retval)
		goto cleanup;

	sprintf(buf, "block bitmap for %s", fs->device_name);
	retval = ext2fs_allocate_block_bitmap(fs, buf, &fs->block_map);
	if (retval)
		goto cleanup;

	sprintf(buf, "inode bitmap for %s", fs->device_name);
	retval = ext2fs_allocate_inode_bitmap(fs, buf, &fs->inode_map);
	if (retval)
		goto cleanup;

	ext2fs_free_mem(&buf);

	retval = ext2fs_get_mem((size_t) fs->desc_blocks * fs->blocksize,
				&fs->group_desc);
	if (retval)
		goto cleanup;

	memset(fs->group_desc, 0, (size_t) fs->desc_blocks * fs->blocksize);

	/*
	 * Reserve the superblock and group descriptors for each
	 * group, and fill in the correct group statistics for group.
	 * Note that although the block bitmap, inode bitmap, and
	 * inode table have not been allocated (and in fact won't be
	 * by this routine), they are accounted for nevertheless.
	 */
	group_block = super->s_first_data_block;
	super->s_free_blocks_count = 0;
	for (i = 0; i < fs->group_desc_count; i++) {
		numblocks = ext2fs_reserve_super_and_bgd(fs, i, fs->block_map);

		super->s_free_blocks_count += numblocks;
		fs->group_desc[i].bg_free_blocks_count = numblocks;
		fs->group_desc[i].bg_free_inodes_count =
			fs->super->s_inodes_per_group;
		fs->group_desc[i].bg_used_dirs_count = 0;

		group_block += super->s_blocks_per_group;
	}

	ext2fs_mark_super_dirty(fs);
	ext2fs_mark_bb_dirty(fs);
	ext2fs_mark_ib_dirty(fs);

	io_channel_set_blksize(fs->io, fs->blocksize);

	*ret_fs = fs;
	return 0;
cleanup:
	ext2fs_free(fs);
	return retval;
}
