/*
 * online.c --- Do on-line resizing of the ext3 filesystem
 *
 * Copyright (C) 2006 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include "resize2fs.h"
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>

extern char *program_name;

#define MAX_32_NUM ((((unsigned long long) 1) << 32) - 1)

#ifdef __linux__
static int parse_version_number(const char *s)
{
	int	major, minor, rev;
	char	*endptr;
	const char *cp = s;

	if (!s)
		return 0;
	major = strtol(cp, &endptr, 10);
	if (cp == endptr || *endptr != '.')
		return 0;
	cp = endptr + 1;
	minor = strtol(cp, &endptr, 10);
	if (cp == endptr || *endptr != '.')
		return 0;
	cp = endptr + 1;
	rev = strtol(cp, &endptr, 10);
	if (cp == endptr)
		return 0;
	return ((((major * 256) + minor) * 256) + rev);
}

#define VERSION_CODE(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#endif

errcode_t online_resize_fs(ext2_filsys fs, const char *mtpt,
			   blk64_t *new_size, int flags EXT2FS_ATTR((unused)))
{
#ifdef __linux__
	struct ext2_new_group_input input;
	struct ext4_new_group_input input64;
	struct ext2_super_block *sb = fs->super;
	unsigned long		new_desc_blocks;
	ext2_filsys 		new_fs;
	errcode_t 		retval;
	double			percent;
	dgrp_t			i;
	blk_t			size;
	int			fd, overhead;
	int			use_old_ioctl = 1;
	int			no_meta_bg_resize = 0;
	int			no_resize_ioctl = 0;

	if (getenv("RESIZE2FS_KERNEL_VERSION")) {
		char *version_to_emulate = getenv("RESIZE2FS_KERNEL_VERSION");
		int kvers = parse_version_number(version_to_emulate);

		if (kvers < VERSION_CODE(3, 7, 0))
			no_meta_bg_resize = 1;
		if (kvers < VERSION_CODE(3, 3, 0))
			no_resize_ioctl = 1;
	}

	printf(_("Filesystem at %s is mounted on %s; "
		 "on-line resizing required\n"), fs->device_name, mtpt);

	if (*new_size < ext2fs_blocks_count(sb)) {
		com_err(program_name, 0, _("On-line shrinking not supported"));
		exit(1);
	}

	/*
	 * If the number of descriptor blocks is going to increase,
	 * the on-line resizing inode must be present.
	 */
	new_desc_blocks = ext2fs_div_ceil(
		ext2fs_div64_ceil(*new_size -
				  fs->super->s_first_data_block,
				  EXT2_BLOCKS_PER_GROUP(fs->super)),
		EXT2_DESC_PER_BLOCK(fs->super));
	printf("old_desc_blocks = %lu, new_desc_blocks = %lu\n",
	       fs->desc_blocks, new_desc_blocks);

	/*
	 * Do error checking to make sure the resize will be successful.
	 */
	if ((access("/sys/fs/ext4/features/meta_bg_resize", R_OK) != 0) ||
	    no_meta_bg_resize) {
		if (!EXT2_HAS_COMPAT_FEATURE(fs->super,
					EXT2_FEATURE_COMPAT_RESIZE_INODE) &&
		    (new_desc_blocks != fs->desc_blocks)) {
			com_err(program_name, 0,
				_("Filesystem does not support online resizing"));
			exit(1);
		}

		if (EXT2_HAS_COMPAT_FEATURE(fs->super,
					EXT2_FEATURE_COMPAT_RESIZE_INODE) &&
		    new_desc_blocks > (fs->desc_blocks +
				       fs->super->s_reserved_gdt_blocks)) {
			com_err(program_name, 0,
				_("Not enough reserved gdt blocks for resizing"));
			exit(1);
		}

		if ((ext2fs_blocks_count(sb) > MAX_32_NUM) ||
		    (*new_size > MAX_32_NUM)) {
			com_err(program_name, 0,
				_("Kernel does not support resizing a file system this large"));
			exit(1);
		}
	}

	fd = open(mtpt, O_RDONLY);
	if (fd < 0) {
		com_err(program_name, errno,
			_("while trying to open mountpoint %s"), mtpt);
		exit(1);
	}

	if (no_resize_ioctl) {
		printf(_("Old resize interface requested.\n"));
	} else if (ioctl(fd, EXT4_IOC_RESIZE_FS, new_size)) {
		/*
		 * If kernel does not support EXT4_IOC_RESIZE_FS, use the
		 * old online resize. Note that the old approach does not
		 * handle >32 bit file systems
		 *
		 * Sigh, if we are running a 32-bit binary on a 64-bit
		 * kernel (which happens all the time on the MIPS
		 * architecture in Debian, but can happen on other CPU
		 * architectures as well) we will get EINVAL returned
		 * when an ioctl doesn't exist, at least up to Linux
		 * 3.1.  See compat_sys_ioctl() in fs/compat_ioctl.c
		 * in the kernel sources.  This is probably a kernel
		 * bug, but work around it here.
		 */
		if ((errno != ENOTTY) && (errno != EINVAL)) {
			if (errno == EPERM)
				com_err(program_name, 0,
				_("Permission denied to resize filesystem"));
			else
				com_err(program_name, errno,
				_("While checking for on-line resizing "
				  "support"));
			exit(1);
		}
	} else {
		close(fd);
		return 0;
	}

	size = ext2fs_blocks_count(sb);

	if (ioctl(fd, EXT2_IOC_GROUP_EXTEND, &size)) {
		if (errno == EPERM)
			com_err(program_name, 0,
				_("Permission denied to resize filesystem"));
		else if (errno == ENOTTY)
			com_err(program_name, 0,
			_("Kernel does not support online resizing"));
		else
			com_err(program_name, errno,
			_("While checking for on-line resizing support"));
		exit(1);
	}

	percent = (ext2fs_r_blocks_count(sb) * 100.0) /
		ext2fs_blocks_count(sb);

	retval = ext2fs_read_bitmaps(fs);
	if (retval)
		return retval;

	retval = ext2fs_dup_handle(fs, &new_fs);
	if (retval)
		return retval;

	/* The current method of adding one block group at a time to a
	 * mounted filesystem means it is impossible to accomodate the
	 * flex_bg allocation method of placing the metadata together
	 * in a single block group.  For now we "fix" this issue by
	 * using the traditional layout for new block groups, where
	 * each block group is self-contained and contains its own
	 * bitmap blocks and inode tables.  This means we don't get
	 * the layout advantages of flex_bg in the new block groups,
	 * but at least it allows on-line resizing to function.
	 */
	new_fs->super->s_feature_incompat &= ~EXT4_FEATURE_INCOMPAT_FLEX_BG;
	retval = adjust_fs_info(new_fs, fs, 0, *new_size);
	if (retval)
		return retval;

	printf(_("Performing an on-line resize of %s to %llu (%dk) blocks.\n"),
	       fs->device_name, *new_size, fs->blocksize / 1024);

	size = fs->group_desc_count * sb->s_blocks_per_group +
		sb->s_first_data_block;
	if (size > *new_size)
		size = *new_size;

	if (ioctl(fd, EXT2_IOC_GROUP_EXTEND, &size)) {
		com_err(program_name, errno,
			_("While trying to extend the last group"));
		exit(1);
	}

	for (i = fs->group_desc_count;
	     i < new_fs->group_desc_count; i++) {

		overhead = (int) (2 + new_fs->inode_blocks_per_group);

		if (ext2fs_bg_has_super(new_fs, new_fs->group_desc_count - 1))
			overhead += 1 + new_fs->desc_blocks +
				new_fs->super->s_reserved_gdt_blocks;

		input.group = i;
		input.block_bitmap = ext2fs_block_bitmap_loc(new_fs, i);
		input.inode_bitmap = ext2fs_inode_bitmap_loc(new_fs, i);
		input.inode_table = ext2fs_inode_table_loc(new_fs, i);
		input.blocks_count = ext2fs_group_blocks_count(new_fs, i);
		input.reserved_blocks = (blk_t) (percent * input.blocks_count
						 / 100.0);

#if 0
		printf("new block bitmap is at 0x%04x\n", input.block_bitmap);
		printf("new inode bitmap is at 0x%04x\n", input.inode_bitmap);
		printf("new inode table is at 0x%04x-0x%04x\n",
		       input.inode_table,
		       input.inode_table + new_fs->inode_blocks_per_group-1);
		printf("new group has %u blocks\n", input.blocks_count);
		printf("new group will reserve %d blocks\n",
		       input.reserved_blocks);
		printf("new group has %d free blocks\n",
		       ext2fs_bg_free_blocks_count(new_fs, i),
		printf("new group has %d free inodes (%d blocks)\n",
		       ext2fs_bg_free_inodes_count(new_fs, i),
		       new_fs->inode_blocks_per_group);
		printf("Adding group #%d\n", input.group);
#endif

		if (use_old_ioctl &&
		    ioctl(fd, EXT2_IOC_GROUP_ADD, &input) == 0)
			continue;
		else
			use_old_ioctl = 0;

		input64.group = input.group;
		input64.block_bitmap = input.block_bitmap;
		input64.inode_bitmap = input.inode_bitmap;
		input64.inode_table = input.inode_table;
		input64.blocks_count = input.blocks_count;
		input64.reserved_blocks = input.reserved_blocks;
		input64.unused = input.unused;

		if (ioctl(fd, EXT4_IOC_GROUP_ADD, &input64) < 0) {
			com_err(program_name, errno,
				_("While trying to add group #%d"),
				input.group);
			exit(1);
		}
	}

	ext2fs_free(new_fs);
	close(fd);

	return 0;
#else
	printf(_("Filesystem at %s is mounted on %s, and on-line resizing is "
		 "not supported on this system.\n"), fs->device_name, mtpt);
	exit(1);
#endif
}
