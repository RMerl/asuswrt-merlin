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

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "ext2_fs.h"

#define sb_struct ext2_super_block
#define sb_struct_name "ext2_super_block"

struct sb_struct sb;

#ifndef offsetof
#define offsetof(type, member)  __builtin_offsetof (type, member)
#endif

#define check_field(x, s) cur_offset = do_field(#x, s, sizeof(sb.x),	       \
						offsetof(struct sb_struct, x), \
						cur_offset)

static int do_field(const char *field, unsigned size, unsigned cur_size,
		    unsigned offset, unsigned cur_offset)
{
	if (size != cur_size) {
		printf("error: %s size %u should be %u\n",
		       field, cur_size, size);
		exit(1);
	}
	if (offset != cur_offset) {
		printf("error: %s offset %u should be %u\n",
		       field, cur_offset, offset);
		exit(1);
	}
	printf("%8d %-30s %3u\n", offset, field, size);
	return offset + size;
}

int main(int argc, char **argv)
{
#if (__GNUC__ >= 4)
	int cur_offset = 0;

	printf("%8s %-30s %3s\n", "offset", "field", "size");
	check_field(s_inodes_count, 4);
	check_field(s_blocks_count, 4);
	check_field(s_r_blocks_count, 4);
	check_field(s_free_blocks_count, 4);
	check_field(s_free_inodes_count, 4);
	check_field(s_first_data_block, 4);
	check_field(s_log_block_size, 4);
	check_field(s_log_cluster_size, 4);
	check_field(s_blocks_per_group, 4);
	check_field(s_clusters_per_group, 4);
	check_field(s_inodes_per_group, 4);
	check_field(s_mtime, 4);
	check_field(s_wtime, 4);
	check_field(s_mnt_count, 2);
	check_field(s_max_mnt_count, 2);
	check_field(s_magic, 2);
	check_field(s_state, 2);
	check_field(s_errors, 2);
	check_field(s_minor_rev_level, 2);
	check_field(s_lastcheck, 4);
	check_field(s_checkinterval, 4);
	check_field(s_creator_os, 4);
	check_field(s_rev_level, 4);
	check_field(s_def_resuid, 2);
	check_field(s_def_resgid, 2);
	check_field(s_first_ino, 4);
	check_field(s_inode_size, 2);
	check_field(s_block_group_nr, 2);
	check_field(s_feature_compat, 4);
	check_field(s_feature_incompat, 4);
	check_field(s_feature_ro_compat, 4);
	check_field(s_uuid, 16);
	check_field(s_volume_name, 16);
	check_field(s_last_mounted, 64);
	check_field(s_algorithm_usage_bitmap, 4);
	check_field(s_prealloc_blocks, 1);
	check_field(s_prealloc_dir_blocks, 1);
	check_field(s_reserved_gdt_blocks, 2);
	check_field(s_journal_uuid, 16);
	check_field(s_journal_inum, 4);
	check_field(s_journal_dev, 4);
	check_field(s_last_orphan, 4);
	check_field(s_hash_seed, 4 * 4);
	check_field(s_def_hash_version, 1);
	check_field(s_jnl_backup_type, 1);
	check_field(s_desc_size, 2);
	check_field(s_default_mount_opts, 4);
	check_field(s_first_meta_bg, 4);
	check_field(s_mkfs_time, 4);
	check_field(s_jnl_blocks, 17 * 4);
	check_field(s_blocks_count_hi, 4);
	check_field(s_r_blocks_count_hi, 4);
	check_field(s_free_blocks_hi, 4);
	check_field(s_min_extra_isize, 2);
	check_field(s_want_extra_isize, 2);
	check_field(s_flags, 4);
	check_field(s_raid_stride, 2);
	check_field(s_mmp_update_interval, 2);
	check_field(s_mmp_block, 8);
	check_field(s_raid_stripe_width, 4);
	check_field(s_log_groups_per_flex, 1);
	check_field(s_reserved_char_pad, 1);
	check_field(s_reserved_pad, 2);
	check_field(s_kbytes_written, 8);
	check_field(s_snapshot_inum, 4);
	check_field(s_snapshot_id, 4);
	check_field(s_snapshot_r_blocks_count, 8);
	check_field(s_snapshot_list, 4);
	check_field(s_error_count, 4);
	check_field(s_first_error_time, 4);
	check_field(s_first_error_ino, 4);
	check_field(s_first_error_block, 8);
	check_field(s_first_error_func, 32);
	check_field(s_first_error_line, 4);
	check_field(s_last_error_time, 4);
	check_field(s_last_error_ino, 4);
	check_field(s_last_error_line, 4);
	check_field(s_last_error_block, 8);
	check_field(s_last_error_func, 32);
	check_field(s_mount_opts, 64);
	check_field(s_usr_quota_inum, 4);
	check_field(s_grp_quota_inum, 4);
	check_field(s_overhead_blocks, 4);
	check_field(s_reserved, 108 * 4);
	check_field(s_checksum, 4);
	do_field("Superblock end", 0, 0, cur_offset, 1024);
#endif
	return 0;
}
