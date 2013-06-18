/*
 * set_fields.c --- set a superblock value
 *
 * Copyright (C) 2000, 2001, 2002, 2003, 2004 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _XOPEN_SOURCE 600 /* for inclusion of strptime() and strtoull */

#ifdef HAVE_STRTOULL
#define STRTOULL strtoull
#else
#define STRTOULL strtoul
#endif

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#include <fcntl.h>
#include <utime.h>

#include "debugfs.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"

static struct ext2_super_block set_sb;
static struct ext2_inode_large set_inode;
static struct ext2_group_desc set_gd;
static struct ext4_group_desc set_gd4;
static struct mmp_struct set_mmp;
static dgrp_t set_bg;
static ext2_ino_t set_ino;
static int array_idx;

#define FLAG_ARRAY	0x0001

struct field_set_info {
	const char	*name;
	void	*ptr;
	void	*ptr2;
	unsigned int	size;
	errcode_t (*func)(struct field_set_info *info, char *field, char *arg);
	int flags;
	int max_idx;
};

static errcode_t parse_uint(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_int(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_string(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_uuid(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_hashalg(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_time(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_bmap(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_gd_csum(struct field_set_info *info, char *field, char *arg);
static errcode_t parse_mmp_clear(struct field_set_info *info, char *field,
				 char *arg);

static struct field_set_info super_fields[] = {
	{ "inodes_count", &set_sb.s_inodes_count, NULL, 4, parse_uint },
	{ "blocks_count", &set_sb.s_blocks_count, &set_sb.s_blocks_count_hi,
		4, parse_uint },
	{ "r_blocks_count", &set_sb.s_r_blocks_count,
		&set_sb.s_r_blocks_count_hi, 4, parse_uint },
	{ "free_blocks_count", &set_sb.s_free_blocks_count,
		&set_sb.s_free_blocks_hi, 4, parse_uint },
	{ "free_inodes_count", &set_sb.s_free_inodes_count, NULL, 4, parse_uint },
	{ "first_data_block", &set_sb.s_first_data_block, NULL, 4, parse_uint },
	{ "log_block_size", &set_sb.s_log_block_size, NULL, 4, parse_uint },
	{ "log_cluster_size", &set_sb.s_log_cluster_size, NULL, 4, parse_int },
	{ "blocks_per_group", &set_sb.s_blocks_per_group, NULL, 4, parse_uint },
	{ "clusters_per_group", &set_sb.s_clusters_per_group, NULL, 4, parse_uint },
	{ "inodes_per_group", &set_sb.s_inodes_per_group, NULL, 4, parse_uint },
	{ "mtime", &set_sb.s_mtime, NULL, 4, parse_time },
	{ "wtime", &set_sb.s_wtime, NULL, 4, parse_time },
	{ "mnt_count", &set_sb.s_mnt_count, NULL, 2, parse_uint },
	{ "max_mnt_count", &set_sb.s_max_mnt_count, NULL, 2, parse_int },
	/* s_magic */
	{ "state", &set_sb.s_state, NULL, 2, parse_uint },
	{ "errors", &set_sb.s_errors, NULL, 2, parse_uint },
	{ "minor_rev_level", &set_sb.s_minor_rev_level, NULL, 2, parse_uint },
	{ "lastcheck", &set_sb.s_lastcheck, NULL, 4, parse_time },
	{ "checkinterval", &set_sb.s_checkinterval, NULL, 4, parse_uint },
	{ "creator_os", &set_sb.s_creator_os, NULL, 4, parse_uint },
	{ "rev_level", &set_sb.s_rev_level, NULL, 4, parse_uint },
	{ "def_resuid", &set_sb.s_def_resuid, NULL, 2, parse_uint },
	{ "def_resgid", &set_sb.s_def_resgid, NULL, 2, parse_uint },
	{ "first_ino", &set_sb.s_first_ino, NULL, 4, parse_uint },
	{ "inode_size", &set_sb.s_inode_size, NULL, 2, parse_uint },
	{ "block_group_nr", &set_sb.s_block_group_nr, NULL, 2, parse_uint },
	{ "feature_compat", &set_sb.s_feature_compat, NULL, 4, parse_uint },
	{ "feature_incompat", &set_sb.s_feature_incompat, NULL, 4, parse_uint },
	{ "feature_ro_compat", &set_sb.s_feature_ro_compat, NULL, 4, parse_uint },
	{ "uuid", &set_sb.s_uuid, NULL, 16, parse_uuid },
	{ "volume_name",  &set_sb.s_volume_name, NULL, 16, parse_string },
	{ "last_mounted",  &set_sb.s_last_mounted, NULL, 64, parse_string },
	{ "lastcheck",  &set_sb.s_lastcheck, NULL, 4, parse_uint },
	{ "algorithm_usage_bitmap", &set_sb.s_algorithm_usage_bitmap, NULL,
		  4, parse_uint },
	{ "prealloc_blocks", &set_sb.s_prealloc_blocks, NULL, 1, parse_uint },
	{ "prealloc_dir_blocks", &set_sb.s_prealloc_dir_blocks, NULL, 1,
		  parse_uint },
	{ "reserved_gdt_blocks", &set_sb.s_reserved_gdt_blocks, NULL, 2,
		  parse_uint },
	{ "journal_uuid", &set_sb.s_journal_uuid, NULL, 16, parse_uuid },
	{ "journal_inum", &set_sb.s_journal_inum, NULL, 4, parse_uint },
	{ "journal_dev", &set_sb.s_journal_dev, NULL, 4, parse_uint },
	{ "last_orphan", &set_sb.s_last_orphan, NULL, 4, parse_uint },
	{ "hash_seed", &set_sb.s_hash_seed, NULL, 16, parse_uuid },
	{ "def_hash_version", &set_sb.s_def_hash_version, NULL, 1, parse_hashalg },
	{ "jnl_backup_type", &set_sb.s_jnl_backup_type, NULL, 1, parse_uint },
	{ "desc_size", &set_sb.s_desc_size, NULL, 2, parse_uint },
	{ "default_mount_opts", &set_sb.s_default_mount_opts, NULL, 4, parse_uint },
	{ "first_meta_bg", &set_sb.s_first_meta_bg, NULL, 4, parse_uint },
	{ "mkfs_time", &set_sb.s_mkfs_time, NULL, 4, parse_time },
	{ "jnl_blocks", &set_sb.s_jnl_blocks[0], NULL, 4, parse_uint, FLAG_ARRAY,
	  17 },
	{ "min_extra_isize", &set_sb.s_min_extra_isize, NULL, 2, parse_uint },
	{ "want_extra_isize", &set_sb.s_want_extra_isize, NULL, 2, parse_uint },
	{ "flags", &set_sb.s_flags, NULL, 4, parse_uint },
	{ "raid_stride", &set_sb.s_raid_stride, NULL, 2, parse_uint },
	{ "min_extra_isize", &set_sb.s_min_extra_isize, NULL, 4, parse_uint },
	{ "mmp_interval", &set_sb.s_mmp_update_interval, NULL, 2, parse_uint },
	{ "mmp_block", &set_sb.s_mmp_block, NULL, 8, parse_uint },
	{ "raid_stripe_width", &set_sb.s_raid_stripe_width, NULL, 4, parse_uint },
	{ "log_groups_per_flex", &set_sb.s_log_groups_per_flex, NULL, 1, parse_uint },
	{ "kbytes_written", &set_sb.s_kbytes_written, NULL, 8, parse_uint },
	{ "snapshot_inum", &set_sb.s_snapshot_inum, NULL, 4, parse_uint },
	{ "snapshot_id", &set_sb.s_snapshot_id, NULL, 4, parse_uint },
	{ "snapshot_r_blocks_count", &set_sb.s_snapshot_r_blocks_count,
	  NULL, 8, parse_uint },
	{ "snapshot_list", &set_sb.s_snapshot_list, NULL, 4, parse_uint },
	{ "mount_opts",  &set_sb.s_mount_opts, NULL, 64, parse_string },
	{ "usr_quota_inum", &set_sb.s_usr_quota_inum, NULL, 4, parse_uint },
	{ "grp_quota_inum", &set_sb.s_grp_quota_inum, NULL, 4, parse_uint },
	{ "overhead_blocks", &set_sb.s_overhead_blocks, NULL, 4, parse_uint },
	{ "checksum", &set_sb.s_checksum, NULL, 4, parse_uint },
	{ 0, 0, 0, 0 }
};

static struct field_set_info inode_fields[] = {
	{ "inodes_count", &set_sb.s_inodes_count, NULL, 4, parse_uint },
	{ "mode", &set_inode.i_mode, NULL, 2, parse_uint },
	{ "uid", &set_inode.i_uid, &set_inode.osd2.linux2.l_i_uid_high,
		2, parse_uint },
	{ "size", &set_inode.i_size, &set_inode.i_size_high, 4, parse_uint },
	{ "atime", &set_inode.i_atime, NULL, 4, parse_time },
	{ "ctime", &set_inode.i_ctime, NULL, 4, parse_time },
	{ "mtime", &set_inode.i_mtime, NULL, 4, parse_time },
	{ "dtime", &set_inode.i_dtime, NULL, 4, parse_time },
	{ "gid", &set_inode.i_gid, &set_inode.osd2.linux2.l_i_gid_high,
		2, parse_uint },
	{ "links_count", &set_inode.i_links_count, NULL, 2, parse_uint },
	/* Special case: i_blocks is 4 bytes, i_blocks_high is 2 bytes */
	{ "blocks", &set_inode.i_blocks, &set_inode.osd2.linux2.l_i_blocks_hi,
		6, parse_uint },
	{ "flags", &set_inode.i_flags, NULL, 4, parse_uint },
	{ "version", &set_inode.osd1.linux1.l_i_version,
		&set_inode.i_version_hi, 4, parse_uint },
	{ "translator", &set_inode.osd1.hurd1.h_i_translator, NULL, 4, parse_uint },
	{ "block", &set_inode.i_block[0], NULL, 4, parse_uint, FLAG_ARRAY,
	  EXT2_NDIR_BLOCKS },
	{ "block[IND]", &set_inode.i_block[EXT2_IND_BLOCK], NULL, 4, parse_uint },
	{ "block[DIND]", &set_inode.i_block[EXT2_DIND_BLOCK], NULL, 4, parse_uint },
	{ "block[TIND]", &set_inode.i_block[EXT2_TIND_BLOCK], NULL, 4, parse_uint },
	{ "generation", &set_inode.i_generation, NULL, 4, parse_uint },
	/* Special case: i_file_acl_high is 2 bytes */
	{ "file_acl", &set_inode.i_file_acl, 
		&set_inode.osd2.linux2.l_i_file_acl_high, 6, parse_uint },
	{ "dir_acl", &set_inode.i_dir_acl, NULL, 4, parse_uint },
	{ "faddr", &set_inode.i_faddr, NULL, 4, parse_uint },
	{ "frag", &set_inode.osd2.hurd2.h_i_frag, NULL, 1, parse_uint },
	{ "fsize", &set_inode.osd2.hurd2.h_i_fsize, NULL, 1, parse_uint },
	{ "checksum", &set_inode.osd2.linux2.l_i_checksum_lo, 
		&set_inode.i_checksum_hi, 2, parse_uint },
	{ "author", &set_inode.osd2.hurd2.h_i_author, NULL,
		4, parse_uint },
	{ "extra_isize", &set_inode.i_extra_isize, NULL,
		2, parse_uint },
	{ "ctime_extra", &set_inode.i_ctime_extra, NULL,
		4, parse_uint },
	{ "mtime_extra", &set_inode.i_mtime_extra, NULL,
		4, parse_uint },
	{ "atime_extra", &set_inode.i_atime_extra, NULL,
		4, parse_uint },
	{ "crtime", &set_inode.i_crtime, NULL, 4, parse_uint },
	{ "crtime_extra", &set_inode.i_crtime_extra, NULL,
		4, parse_uint },
	{ "bmap", NULL, NULL, 4, parse_bmap, FLAG_ARRAY },
	{ 0, 0, 0, 0 }
};

static struct field_set_info ext2_bg_fields[] = {
	{ "block_bitmap", &set_gd.bg_block_bitmap, NULL, 4, parse_uint },
	{ "inode_bitmap", &set_gd.bg_inode_bitmap, NULL, 4, parse_uint },
	{ "inode_table", &set_gd.bg_inode_table, NULL, 4, parse_uint },
	{ "free_blocks_count", &set_gd.bg_free_blocks_count, NULL, 2, parse_uint },
	{ "free_inodes_count", &set_gd.bg_free_inodes_count, NULL, 2, parse_uint },
	{ "used_dirs_count", &set_gd.bg_used_dirs_count, NULL, 2, parse_uint },
	{ "flags", &set_gd.bg_flags, NULL, 2, parse_uint },
	{ "itable_unused", &set_gd.bg_itable_unused, NULL, 2, parse_uint },
	{ "checksum", &set_gd.bg_checksum, NULL, 2, parse_gd_csum },
	{ 0, 0, 0, 0 }
};

static struct field_set_info ext4_bg_fields[] = {
	{ "block_bitmap", &set_gd4.bg_block_bitmap,
		&set_gd4.bg_block_bitmap_hi, 4, parse_uint },
	{ "inode_bitmap", &set_gd4.bg_inode_bitmap,
		&set_gd4.bg_inode_bitmap_hi, 4, parse_uint },
	{ "inode_table", &set_gd4.bg_inode_table,
		&set_gd4.bg_inode_table_hi, 4, parse_uint },
	{ "free_blocks_count", &set_gd4.bg_free_blocks_count,
		&set_gd4.bg_free_blocks_count_hi, 2, parse_uint },
	{ "free_inodes_count", &set_gd4.bg_free_inodes_count,
		&set_gd4.bg_free_inodes_count_hi, 2, parse_uint },
	{ "used_dirs_count", &set_gd4.bg_used_dirs_count,
		&set_gd4.bg_used_dirs_count_hi, 2, parse_uint },
	{ "flags", &set_gd4.bg_flags, NULL, 2, parse_uint },
	{ "exclude_bitmap", &set_gd4.bg_exclude_bitmap_lo,
		&set_gd4.bg_exclude_bitmap_hi, 4, parse_uint },
	{ "block_bitmap_csum", &set_gd4.bg_block_bitmap_csum_lo,
		&set_gd4.bg_block_bitmap_csum_hi, 2, parse_uint },
	{ "inode_bitmap_csum", &set_gd4.bg_inode_bitmap_csum_lo,
		&set_gd4.bg_inode_bitmap_csum_hi, 2, parse_uint },
	{ "itable_unused", &set_gd4.bg_itable_unused,
		&set_gd4.bg_itable_unused_hi, 2, parse_uint },
	{ "checksum", &set_gd4.bg_checksum, NULL, 2, parse_gd_csum },
	{ 0, 0, 0, 0 }
};

static struct field_set_info mmp_fields[] = {
	{ "clear", &set_mmp.mmp_magic, NULL, sizeof(set_mmp), parse_mmp_clear },
	{ "magic", &set_mmp.mmp_magic, NULL, 4, parse_uint },
	{ "seq", &set_mmp.mmp_seq, NULL, 4, parse_uint },
	{ "time", &set_mmp.mmp_time, NULL, 8, parse_uint },
	{ "nodename", &set_mmp.mmp_nodename, NULL, sizeof(set_mmp.mmp_nodename),
		parse_string },
	{ "bdevname", &set_mmp.mmp_bdevname, NULL, sizeof(set_mmp.mmp_bdevname),
		parse_string },
	{ "check_interval", &set_mmp.mmp_check_interval, NULL, 2, parse_uint },
};

static int check_suffix(const char *field)
{
	int len = strlen(field);

	if (len <= 3)
		return 0;
	field += len-3;
	if (!strcmp(field, "_lo"))
		return 1;
	if (!strcmp(field, "_hi"))
		return 2;
	return 0;
}

static struct field_set_info *find_field(struct field_set_info *fields,
					 char *field)
{
	struct field_set_info *ss;
	const char	*prefix;
	char		*arg, *delim, *idx, *tmp;
	int		suffix, prefix_len;

	if (fields == super_fields)
		prefix = "s_";
	else if (fields == inode_fields)
		prefix = "i_";
	else
		prefix = "bg_";
	prefix_len = strlen(prefix);
	if (strncmp(field, prefix, prefix_len) == 0)
		field += prefix_len;

	arg = malloc(strlen(field)+1);
	if (!arg)
		return NULL;
	strcpy(arg, field);

	idx = strchr(arg, '[');
	if (idx) {
		*idx++ = 0;
		delim = idx + strlen(idx) - 1;
		if (!*idx || *delim != ']')
			idx = 0;
		else
			*delim = 0;
	}
	/*
	 * Can we parse the number?
	 */
	if (idx) {
		array_idx = strtol(idx, &tmp, 0);
		if (*tmp)
			idx = 0;
	}

	/*
	 * If there is a valid _hi or a _lo suffix, strip it off
	 */
	suffix = check_suffix(arg);
	if (suffix > 0)
		arg[strlen(arg)-3] = 0;

	for (ss = fields ; ss->name ; ss++) {
		if (suffix && ss->ptr2 == 0)
			continue;
		if (ss->flags & FLAG_ARRAY) {
			if (!idx || (strcmp(ss->name, arg) != 0))
				continue;
			if (ss->max_idx > 0 && array_idx >= ss->max_idx)
				continue;
		} else {
			if (strcmp(ss->name, arg) != 0)
				continue;
		}
		free(arg);
		return ss;
	}
	free(arg);
	return NULL;
}

/*
 * Note: info->size == 6 is special; this means a base size 4 bytes,
 * and secondiory (high) size of 2 bytes.  This is needed for the
 * special case of i_blocks_high and i_file_acl_high.
 */
static errcode_t parse_uint(struct field_set_info *info, char *field,
			    char *arg)
{
	unsigned long long n, num, mask, limit;
	int suffix = check_suffix(field);
	char *tmp;
	void *field1 = info->ptr, *field2 = info->ptr2;
	int size = (info->size == 6) ? 4 : info->size;
	union {
		__u64	*ptr64;
		__u32	*ptr32;
		__u16	*ptr16;
		__u8	*ptr8;
	} u;

	if (suffix == 1)
		field2 = 0;
	if (suffix == 2) {
		field1 = field2;
		field2 = 0;
	}

	u.ptr8 = (__u8 *) field1;
	if (info->flags & FLAG_ARRAY)
		u.ptr8 += array_idx * info->size;

	errno = 0;
	num = STRTOULL(arg, &tmp, 0);
	if (*tmp || errno) {
		fprintf(stderr, "Couldn't parse '%s' for field %s.\n",
			arg, info->name);
		return EINVAL;
	}
	mask = ~0ULL >> ((8 - size) * 8);
	limit = ~0ULL >> ((8 - info->size) * 8);
	if (field2 && info->size != 6)
		limit = ~0ULL >> ((8 - info->size*2) * 8);

	if (num > limit) {
		fprintf(stderr, "Value '%s' exceeds field %s maximum %llu.\n",
			arg, info->name, limit);
		return EINVAL;
	}
	n = num & mask;
	switch (size) {
	case 8:
		*u.ptr64 = n;
		break;
	case 4:
		*u.ptr32 = n;
		break;
	case 2:
		*u.ptr16 = n;
		break;
	case 1:
		*u.ptr8 = n;
		break;
	}
	if (!field2)
		return 0;
	n = num >> (size*8);
	u.ptr8 = (__u8 *) field2;
	if (info->size == 6)
		size = 2;
	switch (size) {
	case 8:
		*u.ptr64 = n;
		break;
	case 4:
		*u.ptr32 = n;
		break;
	case 2:
		*u.ptr16 = n;
		break;
	case 1:
		*u.ptr8 = n;
		break;
	}
	return 0;
}

static errcode_t parse_int(struct field_set_info *info,
			   char *field EXT2FS_ATTR((unused)), char *arg)
{
	long	num;
	char *tmp;
	__s32	*ptr32;
	__s16	*ptr16;
	__s8	*ptr8;

	num = strtol(arg, &tmp, 0);
	if (*tmp) {
		fprintf(stderr, "Couldn't parse '%s' for field %s.\n",
			arg, info->name);
		return EINVAL;
	}
	switch (info->size) {
	case 4:
		ptr32 = (__s32 *) info->ptr;
		*ptr32 = num;
		break;
	case 2:
		ptr16 = (__s16 *) info->ptr;
		*ptr16 = num;
		break;
	case 1:
		ptr8 = (__s8 *) info->ptr;
		*ptr8 = num;
		break;
	}
	return 0;
}

static errcode_t parse_string(struct field_set_info *info,
			      char *field EXT2FS_ATTR((unused)), char *arg)
{
	char	*cp = (char *) info->ptr;

	if (strlen(arg) >= info->size) {
		fprintf(stderr, "Error maximum size for %s is %d.\n",
			info->name, info->size);
		return EINVAL;
	}
	strcpy(cp, arg);
	return 0;
}

static errcode_t parse_time(struct field_set_info *info,
			    char *field EXT2FS_ATTR((unused)), char *arg)
{
	time_t		t;
	__u32		*ptr32;

	ptr32 = (__u32 *) info->ptr;

	t = string_to_time(arg);

	if (t == ((time_t) -1)) {
		fprintf(stderr, "Couldn't parse '%s' for field %s.\n",
			arg, info->name);
		return EINVAL;
	}
	*ptr32 = t;
	return 0;
}

static errcode_t parse_uuid(struct field_set_info *info,
			    char *field EXT2FS_ATTR((unused)), char *arg)
{
	unsigned char *	p = (unsigned char *) info->ptr;

	if ((strcasecmp(arg, "null") == 0) ||
	    (strcasecmp(arg, "clear") == 0)) {
		uuid_clear(p);
	} else if (strcasecmp(arg, "time") == 0) {
		uuid_generate_time(p);
	} else if (strcasecmp(arg, "random") == 0) {
		uuid_generate(p);
	} else if (uuid_parse(arg, p)) {
		fprintf(stderr, "Invalid UUID format: %s\n", arg);
		return EINVAL;
	}
	return 0;
}

static errcode_t parse_hashalg(struct field_set_info *info,
			       char *field EXT2FS_ATTR((unused)), char *arg)
{
	int	hashv;
	unsigned char	*p = (unsigned char *) info->ptr;

	hashv = e2p_string2hash(arg);
	if (hashv < 0) {
		fprintf(stderr, "Invalid hash algorithm: %s\n", arg);
		return EINVAL;
	}
	*p = hashv;
	return 0;
}

static errcode_t parse_bmap(struct field_set_info *info,
			    char *field EXT2FS_ATTR((unused)), char *arg)
{
	unsigned long	num;
	blk_t		blk;
	errcode_t	retval;
	char		*tmp;

	num = strtoul(arg, &tmp, 0);
	if (*tmp) {
		fprintf(stderr, "Couldn't parse '%s' for field %s.\n",
			arg, info->name);
		return EINVAL;
	}
	blk = num;

	retval = ext2fs_bmap(current_fs, set_ino,
			     (struct ext2_inode *) &set_inode,
			     0, BMAP_SET, array_idx, &blk);
	if (retval) {
		com_err("set_inode", retval, "while setting block map");
	}
	return retval;
}

static errcode_t parse_gd_csum(struct field_set_info *info, char *field,
			       char *arg)
{

	if (strcmp(arg, "calc") == 0) {
		ext2fs_group_desc_csum_set(current_fs, set_bg);
		memcpy(&set_gd, ext2fs_group_desc(current_fs,
					current_fs->group_desc,
					set_bg),
			sizeof(set_gd));
		printf("Checksum set to 0x%04x\n",
		       ext2fs_bg_checksum(current_fs, set_bg));
		return 0;
	}

	return parse_uint(info, field, arg);
}

static void print_possible_fields(struct field_set_info *fields)
{
	struct field_set_info *ss;
	const char	*type, *cmd;
	FILE *f;
	char name[40], idx[40];

	if (fields == super_fields) {
		type = "Superblock";
		cmd = "set_super_value";
	} else if (fields == inode_fields) {
		type = "Inode";
		cmd = "set_inode";
	} else if (fields == mmp_fields) {
		type = "MMP";
		cmd = "set_mmp_value";
	} else {
		type = "Block group descriptor";
		cmd = "set_block_group";
	}
	f = open_pager();

	fprintf(f, "%s fields supported by the %s command:\n", type, cmd);

	for (ss = fields ; ss->name ; ss++) {
		type = "unknown";
		if (ss->func == parse_string)
			type = "string";
		else if (ss->func == parse_int)
			type = "integer";
		else if (ss->func == parse_uint)
			type = "unsigned integer";
		else if (ss->func == parse_uuid)
			type = "UUID";
		else if (ss->func == parse_hashalg)
			type = "hash algorithm";
		else if (ss->func == parse_time)
			type = "date/time";
		else if (ss->func == parse_bmap)
			type = "set physical->logical block map";
		else if (ss->func == parse_gd_csum)
			type = "unsigned integer OR \"calc\"";
		strcpy(name, ss->name);
		if (ss->flags & FLAG_ARRAY) {
			if (ss->max_idx > 0)
				sprintf(idx, "[%d]", ss->max_idx);
			else
				strcpy(idx, "[]");
			strcat(name, idx);
		}
		if (ss->ptr2)
			strcat(name, "[_hi|_lo]");
		fprintf(f, "\t%-25s\t%s\n", name, type);
	}
	close_pager(f);
}


void do_set_super(int argc, char *argv[])
{
	const char *usage = "<field> <value>\n"
		"\t\"set_super_value -l\" will list the names of "
		"superblock fields\n\twhich can be set.";
	static struct field_set_info *ss;

	if ((argc == 2) && !strcmp(argv[1], "-l")) {
		print_possible_fields(super_fields);
		return;
	}

	if (common_args_process(argc, argv, 3, 3, "set_super_value",
				usage, CHECK_FS_RW))
		return;

	if ((ss = find_field(super_fields, argv[1])) == 0) {
		com_err(argv[0], 0, "invalid field specifier: %s", argv[1]);
		return;
	}
	set_sb = *current_fs->super;
	if (ss->func(ss, argv[1], argv[2]) == 0) {
		*current_fs->super = set_sb;
		ext2fs_mark_super_dirty(current_fs);
	}
}

void do_set_inode(int argc, char *argv[])
{
	const char *usage = "<inode> <field> <value>\n"
		"\t\"set_inode_field -l\" will list the names of "
		"the fields in an ext2 inode\n\twhich can be set.";
	static struct field_set_info *ss;

	if ((argc == 2) && !strcmp(argv[1], "-l")) {
		print_possible_fields(inode_fields);
		return;
	}

	if (common_args_process(argc, argv, 4, 4, "set_inode",
				usage, CHECK_FS_RW))
		return;

	if ((ss = find_field(inode_fields, argv[2])) == 0) {
		com_err(argv[0], 0, "invalid field specifier: %s", argv[2]);
		return;
	}

	set_ino = string_to_inode(argv[1]);
	if (!set_ino)
		return;

	if (debugfs_read_inode_full(set_ino,
			(struct ext2_inode *) &set_inode, argv[1],
				    sizeof(set_inode)))
		return;

	if (ss->func(ss, argv[2], argv[3]) == 0) {
		if (debugfs_write_inode_full(set_ino, 
			     (struct ext2_inode *) &set_inode,
			     argv[1], sizeof(set_inode)))
			return;
	}
}

void do_set_block_group_descriptor(int argc, char *argv[])
{
	const char *usage = "<bg number> <field> <value>\n"
		"\t\"set_block_group_descriptor -l\" will list the names of "
		"the fields in a block group descriptor\n\twhich can be set.";
	struct field_set_info	*table;
	struct field_set_info	*ss;
	char			*end;
	void			*edit, *target;
	int			size;

	/*
	 *Determine whether we are editing an ext2 or ext4 block
	 * group descriptor
	 */
	if (current_fs && current_fs->super->s_feature_incompat &
	    EXT4_FEATURE_INCOMPAT_64BIT) {
		table = ext4_bg_fields;
		edit = &set_gd4;
		size = sizeof(set_gd4);
	} else {
		table = ext2_bg_fields;
		edit = &set_gd;
		size = sizeof(set_gd);
	}

	if ((argc == 2) && !strcmp(argv[1], "-l")) {
		print_possible_fields(table);
		return;
	}

	if (common_args_process(argc, argv, 4, 4, "set_block_group_descriptor",
				usage, CHECK_FS_RW))
		return;

	set_bg = strtoul(argv[1], &end, 0);
	if (*end) {
		com_err(argv[0], 0, "invalid block group number: %s", argv[1]);
		return;
	}

	if (set_bg >= current_fs->group_desc_count) {
		com_err(argv[0], 0, "block group number too big: %d", set_bg);
		return;
	}

	if ((ss = find_field(table, argv[2])) == 0) {
		com_err(argv[0], 0, "invalid field specifier: %s", argv[2]);
		return;
	}

	target = ext2fs_group_desc(current_fs, current_fs->group_desc, set_bg);
	memcpy(edit, target, size);
	if (ss->func(ss, argv[2], argv[3]) == 0) {
		memcpy(target, edit, size);
		ext2fs_mark_super_dirty(current_fs);
	}
}

static errcode_t parse_mmp_clear(struct field_set_info *info,
				 char *field EXT2FS_ATTR((unused)),
				 char *arg EXT2FS_ATTR((unused)))
{
	errcode_t retval;

	retval = ext2fs_mmp_clear(current_fs);
	if (retval != 0)
		com_err("set_mmp_value", retval, "while clearing MMP block\n");
	else
		memcpy(info->ptr, current_fs->mmp_buf, info->size);

	return 1; /* we don't need the MMP block written again */
}

void do_set_mmp_value(int argc, char *argv[])
{
	const char *usage = "<field> <value>\n"
		"\t\"set_mmp_value -l\" will list the names of "
		"MMP fields\n\twhich can be set.";
	static struct field_set_info *smmp;
	struct mmp_struct *mmp_s;
	errcode_t retval;

	if (argc == 2 && strcmp(argv[1], "-l") == 0) {
		print_possible_fields(mmp_fields);
		return;
	}

	if (check_fs_open(argv[0]))
		return;

	if (current_fs->super->s_mmp_block == 0) {
		com_err(argv[0], 0, "no MMP block allocated\n");
		return;
	}

	if (common_args_process(argc, argv, 2, 3, "set_mmp_value",
				usage, CHECK_FS_RW))
		return;

	mmp_s = current_fs->mmp_buf;
	if (mmp_s == NULL) {
		retval = ext2fs_get_mem(current_fs->blocksize, &mmp_s);
		if (retval) {
			com_err(argv[0], retval, "allocating MMP buffer\n");
			return;
		}
		retval = ext2fs_mmp_read(current_fs,
					 current_fs->super->s_mmp_block, mmp_s);
		if (retval) {
			com_err(argv[0], retval, "reading MMP block %llu.\n",
				(long long)current_fs->super->s_mmp_block);
			ext2fs_free_mem(mmp_s);
			return;
		}
		current_fs->mmp_buf = mmp_s;
	}

	smmp = find_field(mmp_fields, argv[1]);
	if (smmp == 0) {
		com_err(argv[0], 0, "invalid field specifier: %s", argv[1]);
		return;
	}

	set_mmp = *mmp_s;
	if (smmp->func(smmp, argv[1], argv[2]) == 0) {
		ext2fs_mmp_write(current_fs, current_fs->super->s_mmp_block,
				 &set_mmp);
		*mmp_s = set_mmp;
	}
}

