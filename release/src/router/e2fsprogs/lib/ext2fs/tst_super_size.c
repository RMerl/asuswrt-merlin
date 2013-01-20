/*
 * This testing program makes sure superblock size is 1024 bytes long
 *
 * Copyright (C) 2007 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "ext2_fs.h"

#define sb_struct ext2_super_block
#define sb_struct_name "ext2_super_block"

struct sb_struct sb;

int verbose = 0;

#define offsetof(type, member)  __builtin_offsetof (type, member)
#define check_field(x) cur_offset = do_field(#x, sizeof(sb.x),		\
					      offsetof(struct sb_struct, x), \
					      cur_offset)

static int do_field(const char *field, size_t size, int offset, int cur_offset)
{
	if (offset != cur_offset) {
		printf("Warning!  Unexpected offset at %s\n", field);
		exit(1);
	}
	printf("%8d %-30s %3u\n", offset, field, (unsigned) size);
	return offset + size;
}

void check_superblock_fields()
{
#if (__GNUC__ >= 4)
	int cur_offset = 0;

	printf("%8s %-30s %3s\n", "offset", "field", "size");
	check_field(s_inodes_count);
	check_field(s_blocks_count);
	check_field(s_r_blocks_count);
	check_field(s_free_blocks_count);
	check_field(s_free_inodes_count);
	check_field(s_first_data_block);
	check_field(s_log_block_size);
	check_field(s_log_frag_size);
	check_field(s_blocks_per_group);
	check_field(s_frags_per_group);
	check_field(s_inodes_per_group);
	check_field(s_mtime);
	check_field(s_wtime);
	check_field(s_mnt_count);
	check_field(s_max_mnt_count);
	check_field(s_magic);
	check_field(s_state);
	check_field(s_errors);
	check_field(s_minor_rev_level);
	check_field(s_lastcheck);
	check_field(s_checkinterval);
	check_field(s_creator_os);
	check_field(s_rev_level);
	check_field(s_def_resuid);
	check_field(s_def_resgid);
	check_field(s_first_ino);
	check_field(s_inode_size);
	check_field(s_block_group_nr);
	check_field(s_feature_compat);
	check_field(s_feature_incompat);
	check_field(s_feature_ro_compat);
	check_field(s_uuid);
	check_field(s_volume_name);
	check_field(s_last_mounted);
	check_field(s_algorithm_usage_bitmap);
	check_field(s_prealloc_blocks);
	check_field(s_prealloc_dir_blocks);
	check_field(s_reserved_gdt_blocks);
	check_field(s_journal_uuid);
	check_field(s_journal_inum);
	check_field(s_journal_dev);
	check_field(s_last_orphan);
	check_field(s_hash_seed);
	check_field(s_def_hash_version);
	check_field(s_jnl_backup_type);
	check_field(s_desc_size);
	check_field(s_default_mount_opts);
	check_field(s_first_meta_bg);
	check_field(s_mkfs_time);
	check_field(s_jnl_blocks);
	check_field(s_blocks_count_hi);
	check_field(s_r_blocks_count_hi);
	check_field(s_free_blocks_hi);
	check_field(s_min_extra_isize);
	check_field(s_want_extra_isize);
	check_field(s_flags);
	check_field(s_raid_stride);
	check_field(s_mmp_interval);
	check_field(s_mmp_block);
	check_field(s_raid_stripe_width);
	check_field(s_log_groups_per_flex);
	check_field(s_reserved_char_pad);
	check_field(s_reserved_pad);
	check_field(s_kbytes_written);
	check_field(s_snapshot_inum);
	check_field(s_snapshot_id);
	check_field(s_snapshot_r_blocks_count);
	check_field(s_snapshot_list);
	check_field(s_error_count);
	check_field(s_first_error_time);
	check_field(s_first_error_ino);
	check_field(s_first_error_block);
	check_field(s_first_error_func);
	check_field(s_first_error_line);
	check_field(s_last_error_time);
	check_field(s_last_error_ino);
	check_field(s_last_error_line);
	check_field(s_last_error_block);
	check_field(s_last_error_func);
	check_field(s_mount_opts);
	check_field(s_reserved);
	printf("Ending offset is %d\n\n", cur_offset);
#endif
}


int main(int argc, char **argv)
{
	int s = sizeof(struct sb_struct);

	check_superblock_fields();
	printf("Size of struct %s is %d\n", sb_struct_name, s);
	if (s != 1024) {
		exit(1);
	}
	exit(0);
}
