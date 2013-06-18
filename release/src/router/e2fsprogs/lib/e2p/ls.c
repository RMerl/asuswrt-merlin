/*
 * ls.c			- List the contents of an ext2fs superblock
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright (C) 1995, 1996, 1997  Theodore Ts'o <tytso@mit.edu>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>

#include "e2p.h"

static void print_user (unsigned short uid, FILE *f)
{
	struct passwd *pw;

	fprintf(f, "%u ", uid);
	pw = getpwuid (uid);
	if (pw == NULL)
		fprintf(f, "(user unknown)\n");
	else
		fprintf(f, "(user %s)\n", pw->pw_name);
}

static void print_group (unsigned short gid, FILE *f)
{
	struct group *gr;

	fprintf(f, "%u ", gid);
	gr = getgrgid (gid);
	if (gr == NULL)
		fprintf(f, "(group unknown)\n");
	else
		fprintf(f, "(group %s)\n", gr->gr_name);
}

#define MONTH_INT (86400 * 30)
#define WEEK_INT (86400 * 7)
#define DAY_INT	(86400)
#define HOUR_INT (60 * 60)
#define MINUTE_INT (60)

static const char *interval_string(unsigned int secs)
{
	static char buf[256], tmp[80];
	int		hr, min, num;

	buf[0] = 0;

	if (secs == 0)
		return "<none>";

	if (secs >= MONTH_INT) {
		num = secs / MONTH_INT;
		secs -= num*MONTH_INT;
		sprintf(buf, "%d month%s", num, (num>1) ? "s" : "");
	}
	if (secs >= WEEK_INT) {
		num = secs / WEEK_INT;
		secs -= num*WEEK_INT;
		sprintf(tmp, "%s%d week%s", buf[0] ? ", " : "",
			num, (num>1) ? "s" : "");
		strcat(buf, tmp);
	}
	if (secs >= DAY_INT) {
		num = secs / DAY_INT;
		secs -= num*DAY_INT;
		sprintf(tmp, "%s%d day%s", buf[0] ? ", " : "",
			num, (num>1) ? "s" : "");
		strcat(buf, tmp);
	}
	if (secs > 0) {
		hr = secs / HOUR_INT;
		secs -= hr*HOUR_INT;
		min = secs / MINUTE_INT;
		secs -= min*MINUTE_INT;
		sprintf(tmp, "%s%d:%02d:%02d", buf[0] ? ", " : "",
			hr, min, secs);
		strcat(buf, tmp);
	}
	return buf;
}

static void print_features(struct ext2_super_block * s, FILE *f)
{
#ifdef EXT2_DYNAMIC_REV
	int	i, j, printed=0;
	__u32	*mask = &s->s_feature_compat, m;

	fprintf(f, "Filesystem features:     ");
	for (i=0; i <3; i++,mask++) {
		for (j=0,m=1; j < 32; j++, m<<=1) {
			if (*mask & m) {
				fprintf(f, " %s", e2p_feature2string(i, m));
				printed++;
			}
		}
	}
	if (printed == 0)
		fprintf(f, " (none)");
	fprintf(f, "\n");
#endif
}

static void print_mntopts(struct ext2_super_block * s, FILE *f)
{
#ifdef EXT2_DYNAMIC_REV
	int	i, printed=0;
	__u32	mask = s->s_default_mount_opts, m;

	fprintf(f, "Default mount options:   ");
	if (mask & EXT3_DEFM_JMODE) {
		fprintf(f, " %s", e2p_mntopt2string(mask & EXT3_DEFM_JMODE));
		printed++;
	}
	for (i=0,m=1; i < 32; i++, m<<=1) {
		if (m & EXT3_DEFM_JMODE)
			continue;
		if (mask & m) {
			fprintf(f, " %s", e2p_mntopt2string(m));
			printed++;
		}
	}
	if (printed == 0)
		fprintf(f, " (none)");
	fprintf(f, "\n");
#endif
}

static void print_super_flags(struct ext2_super_block * s, FILE *f)
{
	int	flags_found = 0;

	if (s->s_flags == 0)
		return;

	fputs("Filesystem flags:         ", f);
	if (s->s_flags & EXT2_FLAGS_SIGNED_HASH) {
		fputs("signed_directory_hash ", f);
		flags_found++;
	}
	if (s->s_flags & EXT2_FLAGS_UNSIGNED_HASH) {
		fputs("unsigned_directory_hash ", f);
		flags_found++;
	}
	if (s->s_flags & EXT2_FLAGS_TEST_FILESYS) {
		fputs("test_filesystem ", f);
		flags_found++;
	}
	if (flags_found)
		fputs("\n", f);
	else
		fputs("(none)\n", f);
}

static __u64 e2p_blocks_count(struct ext2_super_block *super)
{
	return super->s_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_blocks_count_hi << 32 : 0);
}

static __u64 e2p_r_blocks_count(struct ext2_super_block *super)
{
	return super->s_r_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_r_blocks_count_hi << 32 : 0);
}

static __u64 e2p_free_blocks_count(struct ext2_super_block *super)
{
	return super->s_free_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_free_blocks_hi << 32 : 0);
}

#ifndef EXT2_INODE_SIZE
#define EXT2_INODE_SIZE(s) sizeof(struct ext2_inode)
#endif

#ifndef EXT2_GOOD_OLD_REV
#define EXT2_GOOD_OLD_REV 0
#endif

void list_super2(struct ext2_super_block * sb, FILE *f)
{
	int inode_blocks_per_group;
	char buf[80], *str;
	time_t	tm;

	inode_blocks_per_group = (((sb->s_inodes_per_group *
				    EXT2_INODE_SIZE(sb)) +
				   EXT2_BLOCK_SIZE(sb) - 1) /
				  EXT2_BLOCK_SIZE(sb));
	if (sb->s_volume_name[0]) {
		memset(buf, 0, sizeof(buf));
		strncpy(buf, sb->s_volume_name, sizeof(sb->s_volume_name));
	} else
		strcpy(buf, "<none>");
	fprintf(f, "Filesystem volume name:   %s\n", buf);
	if (sb->s_last_mounted[0]) {
		memset(buf, 0, sizeof(buf));
		strncpy(buf, sb->s_last_mounted, sizeof(sb->s_last_mounted));
	} else
		strcpy(buf, "<not available>");
	fprintf(f, "Last mounted on:          %s\n", buf);
	fprintf(f, "Filesystem UUID:          %s\n", e2p_uuid2str(sb->s_uuid));
	fprintf(f, "Filesystem magic number:  0x%04X\n", sb->s_magic);
	fprintf(f, "Filesystem revision #:    %d", sb->s_rev_level);
	if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
		fprintf(f, " (original)\n");
#ifdef EXT2_DYNAMIC_REV
	} else if (sb->s_rev_level == EXT2_DYNAMIC_REV) {
		fprintf(f, " (dynamic)\n");
#endif
	} else
		fprintf(f, " (unknown)\n");
	print_features(sb, f);
	print_super_flags(sb, f);
	print_mntopts(sb, f);
	if (sb->s_mount_opts[0])
		fprintf(f, "Mount options:            %s\n", sb->s_mount_opts);
	fprintf(f, "Filesystem state:        ");
	print_fs_state (f, sb->s_state);
	fprintf(f, "\n");
	fprintf(f, "Errors behavior:          ");
	print_fs_errors(f, sb->s_errors);
	fprintf(f, "\n");
	str = e2p_os2string(sb->s_creator_os);
	fprintf(f, "Filesystem OS type:       %s\n", str);
	free(str);
	fprintf(f, "Inode count:              %u\n", sb->s_inodes_count);
	fprintf(f, "Block count:              %llu\n", e2p_blocks_count(sb));
	fprintf(f, "Reserved block count:     %llu\n", e2p_r_blocks_count(sb));
	if (sb->s_overhead_blocks)
		fprintf(f, "Overhead blocks:          %u\n",
			sb->s_overhead_blocks);
	fprintf(f, "Free blocks:              %llu\n", e2p_free_blocks_count(sb));
	fprintf(f, "Free inodes:              %u\n", sb->s_free_inodes_count);
	fprintf(f, "First block:              %u\n", sb->s_first_data_block);
	fprintf(f, "Block size:               %u\n", EXT2_BLOCK_SIZE(sb));
	if (sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_BIGALLOC)
		fprintf(f, "Cluster size:             %u\n",
			EXT2_CLUSTER_SIZE(sb));
	else
		fprintf(f, "Fragment size:            %u\n",
			EXT2_CLUSTER_SIZE(sb));
	if (sb->s_reserved_gdt_blocks)
		fprintf(f, "Reserved GDT blocks:      %u\n",
			sb->s_reserved_gdt_blocks);
	fprintf(f, "Blocks per group:         %u\n", sb->s_blocks_per_group);
	if (sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_BIGALLOC)
		fprintf(f, "Clusters per group:       %u\n",
			sb->s_clusters_per_group);
	else
		fprintf(f, "Fragments per group:      %u\n",
			sb->s_clusters_per_group);
	fprintf(f, "Inodes per group:         %u\n", sb->s_inodes_per_group);
	fprintf(f, "Inode blocks per group:   %u\n", inode_blocks_per_group);
	if (sb->s_raid_stride)
		fprintf(f, "RAID stride:              %u\n",
			sb->s_raid_stride);
	if (sb->s_raid_stripe_width)
		fprintf(f, "RAID stripe width:        %u\n",
			sb->s_raid_stripe_width);
	if (sb->s_first_meta_bg)
		fprintf(f, "First meta block group:   %u\n",
			sb->s_first_meta_bg);
	if (sb->s_log_groups_per_flex)
		fprintf(f, "Flex block group size:    %u\n",
			1 << sb->s_log_groups_per_flex);
	if (sb->s_mkfs_time) {
		tm = sb->s_mkfs_time;
		fprintf(f, "Filesystem created:       %s", ctime(&tm));
	}
	tm = sb->s_mtime;
	fprintf(f, "Last mount time:          %s",
		sb->s_mtime ? ctime(&tm) : "n/a\n");
	tm = sb->s_wtime;
	fprintf(f, "Last write time:          %s", ctime(&tm));
	fprintf(f, "Mount count:              %u\n", sb->s_mnt_count);
	fprintf(f, "Maximum mount count:      %d\n", sb->s_max_mnt_count);
	tm = sb->s_lastcheck;
	fprintf(f, "Last checked:             %s", ctime(&tm));
	fprintf(f, "Check interval:           %u (%s)\n", sb->s_checkinterval,
	       interval_string(sb->s_checkinterval));
	if (sb->s_checkinterval)
	{
		time_t next;

		next = sb->s_lastcheck + sb->s_checkinterval;
		fprintf(f, "Next check after:         %s", ctime(&next));
	}
#define POW2(x) ((__u64) 1 << (x))
	if (sb->s_kbytes_written) {
		fprintf(f, "Lifetime writes:          ");
		if (sb->s_kbytes_written < POW2(13))
			fprintf(f, "%llu kB\n", sb->s_kbytes_written);
		else if (sb->s_kbytes_written < POW2(23))
			fprintf(f, "%llu MB\n",
				(sb->s_kbytes_written + POW2(9)) >> 10);
		else if (sb->s_kbytes_written < POW2(33))
			fprintf(f, "%llu GB\n",
				(sb->s_kbytes_written + POW2(19)) >> 20);
		else if (sb->s_kbytes_written < POW2(43))
			fprintf(f, "%llu TB\n",
				(sb->s_kbytes_written + POW2(29)) >> 30);
		else
			fprintf(f, "%llu PB\n",
				(sb->s_kbytes_written + POW2(39)) >> 40);
	}
	fprintf(f, "Reserved blocks uid:      ");
	print_user(sb->s_def_resuid, f);
	fprintf(f, "Reserved blocks gid:      ");
	print_group(sb->s_def_resgid, f);
	if (sb->s_rev_level >= EXT2_DYNAMIC_REV) {
		fprintf(f, "First inode:              %d\n", sb->s_first_ino);
		fprintf(f, "Inode size:	          %d\n", sb->s_inode_size);
		if (sb->s_min_extra_isize)
			fprintf(f, "Required extra isize:     %d\n",
				sb->s_min_extra_isize);
		if (sb->s_want_extra_isize)
			fprintf(f, "Desired extra isize:      %d\n",
				sb->s_want_extra_isize);
	}
	if (!e2p_is_null_uuid(sb->s_journal_uuid))
		fprintf(f, "Journal UUID:             %s\n",
			e2p_uuid2str(sb->s_journal_uuid));
	if (sb->s_journal_inum)
		fprintf(f, "Journal inode:            %u\n",
			sb->s_journal_inum);
	if (sb->s_journal_dev)
		fprintf(f, "Journal device:	          0x%04x\n",
			sb->s_journal_dev);
	if (sb->s_last_orphan)
		fprintf(f, "First orphan inode:       %u\n",
			sb->s_last_orphan);
	if ((sb->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) ||
	    sb->s_def_hash_version)
		fprintf(f, "Default directory hash:   %s\n",
			e2p_hash2string(sb->s_def_hash_version));
	if (!e2p_is_null_uuid(sb->s_hash_seed))
		fprintf(f, "Directory Hash Seed:      %s\n",
			e2p_uuid2str(sb->s_hash_seed));
	if (sb->s_jnl_backup_type) {
		fprintf(f, "Journal backup:           ");
		switch (sb->s_jnl_backup_type) {
		case 1:
			fprintf(f, "inode blocks\n");
			break;
		default:
			fprintf(f, "type %u\n", sb->s_jnl_backup_type);
		}
	}
	if (sb->s_snapshot_inum) {
		fprintf(f, "Snapshot inode:           %u\n",
			sb->s_snapshot_inum);
		fprintf(f, "Snapshot ID:              %u\n",
			sb->s_snapshot_id);
		fprintf(f, "Snapshot reserved blocks: %llu\n",
			sb->s_snapshot_r_blocks_count);
	}
	if (sb->s_snapshot_list)
		fprintf(f, "Snapshot list head:       %u\n",
			sb->s_snapshot_list);
	if (sb->s_error_count)
		fprintf(f, "FS Error count:           %u\n",
			sb->s_error_count);
	if (sb->s_first_error_time) {
		tm = sb->s_first_error_time;
		fprintf(f, "First error time:         %s", ctime(&tm));
		memset(buf, 0, sizeof(buf));
		strncpy(buf, (char *)sb->s_first_error_func,
			sizeof(sb->s_first_error_func));
		fprintf(f, "First error function:     %s\n", buf);
		fprintf(f, "First error line #:       %u\n",
			sb->s_first_error_line);
		fprintf(f, "First error inode #:      %u\n",
			sb->s_first_error_ino);
		fprintf(f, "First error block #:      %llu\n",
			sb->s_first_error_block);
	}
	if (sb->s_last_error_time) {
		tm = sb->s_last_error_time;
		fprintf(f, "Last error time:          %s", ctime(&tm));
		memset(buf, 0, sizeof(buf));
		strncpy(buf, (char *)sb->s_last_error_func,
			sizeof(sb->s_last_error_func));
		fprintf(f, "Last error function:      %s\n", buf);
		fprintf(f, "Last error line #:        %u\n",
			sb->s_last_error_line);
		fprintf(f, "Last error inode #:       %u\n",
			sb->s_last_error_ino);
		fprintf(f, "Last error block #:       %llu\n",
			sb->s_last_error_block);
	}
	if (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_MMP) {
		fprintf(f, "MMP block number:         %llu\n",
			(long long)sb->s_mmp_block);
		fprintf(f, "MMP update interval:      %u\n",
			sb->s_mmp_update_interval);
	}
	if (sb->s_usr_quota_inum)
		fprintf(f, "User quota inode:         %u\n",
			sb->s_usr_quota_inum);
	if (sb->s_grp_quota_inum)
		fprintf(f, "Group quota inode:        %u\n",
			sb->s_grp_quota_inum);

	if (sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_METADATA_CSUM)
		fprintf(f, "Checksum:                 0x%08x\n",
			sb->s_checksum);
}

void list_super (struct ext2_super_block * s)
{
	list_super2(s, stdout);
}

