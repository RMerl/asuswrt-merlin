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
static struct ext2_inode set_inode;
static struct ext2_group_desc set_gd;
static dgrp_t set_bg;
static ext2_ino_t set_ino;
static int array_idx;

#define FLAG_ARRAY	0x0001

struct field_set_info {
	const char	*name;
	void	*ptr;
	unsigned int	size;
	errcode_t (*func)(struct field_set_info *info, char *arg);
	int flags;
	int max_idx;
};

static errcode_t parse_uint(struct field_set_info *info, char *arg);
static errcode_t parse_int(struct field_set_info *info, char *arg);
static errcode_t parse_string(struct field_set_info *info, char *arg);
static errcode_t parse_uuid(struct field_set_info *info, char *arg);
static errcode_t parse_hashalg(struct field_set_info *info, char *arg);
static errcode_t parse_time(struct field_set_info *info, char *arg);
static errcode_t parse_bmap(struct field_set_info *info, char *arg);
static errcode_t parse_gd_csum(struct field_set_info *info, char *arg);

static struct field_set_info super_fields[] = {
	{ "inodes_count", &set_sb.s_inodes_count, 4, parse_uint },
	{ "blocks_count", &set_sb.s_blocks_count, 4, parse_uint },
	{ "r_blocks_count", &set_sb.s_r_blocks_count, 4, parse_uint },
	{ "free_blocks_count", &set_sb.s_free_blocks_count, 4, parse_uint },
	{ "free_inodes_count", &set_sb.s_free_inodes_count, 4, parse_uint },
	{ "first_data_block", &set_sb.s_first_data_block, 4, parse_uint },
	{ "log_block_size", &set_sb.s_log_block_size, 4, parse_uint },
	{ "log_frag_size", &set_sb.s_log_frag_size, 4, parse_int },
	{ "blocks_per_group", &set_sb.s_blocks_per_group, 4, parse_uint },
	{ "frags_per_group", &set_sb.s_frags_per_group, 4, parse_uint },
	{ "inodes_per_group", &set_sb.s_inodes_per_group, 4, parse_uint },
	{ "mtime", &set_sb.s_mtime, 4, parse_time },
	{ "wtime", &set_sb.s_wtime, 4, parse_time },
	{ "mnt_count", &set_sb.s_mnt_count, 2, parse_uint },
	{ "max_mnt_count", &set_sb.s_max_mnt_count, 2, parse_int },
	/* s_magic */
	{ "state", &set_sb.s_state, 2, parse_uint },
	{ "errors", &set_sb.s_errors, 2, parse_uint },
	{ "minor_rev_level", &set_sb.s_minor_rev_level, 2, parse_uint },
	{ "lastcheck", &set_sb.s_lastcheck, 4, parse_time },
	{ "checkinterval", &set_sb.s_checkinterval, 4, parse_uint },
	{ "creator_os", &set_sb.s_creator_os, 4, parse_uint },
	{ "rev_level", &set_sb.s_rev_level, 4, parse_uint },
	{ "def_resuid", &set_sb.s_def_resuid, 2, parse_uint },
	{ "def_resgid", &set_sb.s_def_resgid, 2, parse_uint },
	{ "first_ino", &set_sb.s_first_ino, 4, parse_uint },
	{ "inode_size", &set_sb.s_inode_size, 2, parse_uint },
	{ "block_group_nr", &set_sb.s_block_group_nr, 2, parse_uint },
	{ "feature_compat", &set_sb.s_feature_compat, 4, parse_uint },
	{ "feature_incompat", &set_sb.s_feature_incompat, 4, parse_uint },
	{ "feature_ro_compat", &set_sb.s_feature_ro_compat, 4, parse_uint },
	{ "uuid", &set_sb.s_uuid, 16, parse_uuid },
	{ "volume_name",  &set_sb.s_volume_name, 16, parse_string },
	{ "last_mounted",  &set_sb.s_last_mounted, 64, parse_string },
	{ "lastcheck",  &set_sb.s_lastcheck, 4, parse_uint },
	{ "algorithm_usage_bitmap", &set_sb.s_algorithm_usage_bitmap,
		  4, parse_uint },
	{ "prealloc_blocks", &set_sb.s_prealloc_blocks, 1, parse_uint },
	{ "prealloc_dir_blocks", &set_sb.s_prealloc_dir_blocks, 1,
		  parse_uint },
	{ "reserved_gdt_blocks", &set_sb.s_reserved_gdt_blocks, 2,
		  parse_uint },
	{ "journal_uuid", &set_sb.s_journal_uuid, 16, parse_uuid },
	{ "journal_inum", &set_sb.s_journal_inum, 4, parse_uint },
	{ "journal_dev", &set_sb.s_journal_dev, 4, parse_uint },
	{ "last_orphan", &set_sb.s_last_orphan, 4, parse_uint },
	{ "hash_seed", &set_sb.s_hash_seed, 16, parse_uuid },
	{ "def_hash_version", &set_sb.s_def_hash_version, 1, parse_hashalg },
	{ "jnl_backup_type", &set_sb.s_jnl_backup_type, 1, parse_uint },
	{ "desc_size", &set_sb.s_desc_size, 2, parse_uint },
	{ "default_mount_opts", &set_sb.s_default_mount_opts, 4, parse_uint },
	{ "first_meta_bg", &set_sb.s_first_meta_bg, 4, parse_uint },
	{ "mkfs_time", &set_sb.s_mkfs_time, 4, parse_time },
	{ "jnl_blocks", &set_sb.s_jnl_blocks[0], 4, parse_uint, FLAG_ARRAY,
	  17 },
	{ "blocks_count_hi", &set_sb.s_blocks_count_hi, 4, parse_uint },
	{ "r_blocks_count_hi", &set_sb.s_r_blocks_count_hi, 4, parse_uint },
	{ "min_extra_isize", &set_sb.s_min_extra_isize, 2, parse_uint },
	{ "want_extra_isize", &set_sb.s_want_extra_isize, 2, parse_uint },
	{ "flags", &set_sb.s_flags, 4, parse_uint },
	{ "raid_stride", &set_sb.s_raid_stride, 2, parse_uint },
	{ "min_extra_isize", &set_sb.s_min_extra_isize, 4, parse_uint },
	{ "mmp_interval", &set_sb.s_mmp_interval, 2, parse_uint },
	{ "mmp_block", &set_sb.s_mmp_block, 8, parse_uint },
	{ "raid_stripe_width", &set_sb.s_raid_stripe_width, 4, parse_uint },
	{ "log_groups_per_flex", &set_sb.s_log_groups_per_flex, 1, parse_uint },
	{ "kbytes_written", &set_sb.s_kbytes_written, 8, parse_uint },
	{ "snapshot_inum", &set_sb.s_snapshot_inum, 4, parse_uint },
	{ "snapshot_id", &set_sb.s_snapshot_id, 4, parse_uint },
	{ "snapshot_r_blocks_count", &set_sb.s_snapshot_r_blocks_count, 8, 
	  parse_uint },
	{ "snapshot_list", &set_sb.s_snapshot_list, 4, parse_uint },
	{ "mount_opts",  &set_sb.s_mount_opts, 64, parse_string },
	{ 0, 0, 0, 0 }
};

static struct field_set_info inode_fields[] = {
	{ "inodes_count", &set_sb.s_inodes_count, 4, parse_uint },
	{ "mode", &set_inode.i_mode, 2, parse_uint },
	{ "uid", &set_inode.i_uid, 2, parse_uint },
	{ "size", &set_inode.i_size, 4, parse_uint },
	{ "atime", &set_inode.i_atime, 4, parse_time },
	{ "ctime", &set_inode.i_ctime, 4, parse_time },
	{ "mtime", &set_inode.i_mtime, 4, parse_time },
	{ "dtime", &set_inode.i_dtime, 4, parse_time },
	{ "gid", &set_inode.i_gid, 2, parse_uint },
	{ "links_count", &set_inode.i_links_count, 2, parse_uint },
	{ "blocks", &set_inode.i_blocks, 4, parse_uint },
	{ "flags", &set_inode.i_flags, 4, parse_uint },
	{ "version", &set_inode.osd1.linux1.l_i_version, 4, parse_uint },
	{ "translator", &set_inode.osd1.hurd1.h_i_translator, 4, parse_uint },
	{ "block", &set_inode.i_block[0], 4, parse_uint, FLAG_ARRAY,
	  EXT2_NDIR_BLOCKS },
	{ "block[IND]", &set_inode.i_block[EXT2_IND_BLOCK], 4, parse_uint },
	{ "block[DIND]", &set_inode.i_block[EXT2_DIND_BLOCK], 4, parse_uint },
	{ "block[TIND]", &set_inode.i_block[EXT2_TIND_BLOCK], 4, parse_uint },
	{ "generation", &set_inode.i_generation, 4, parse_uint },
	{ "file_acl", &set_inode.i_file_acl, 4, parse_uint },
	{ "file_acl_high", &set_inode.osd2.linux2.l_i_file_acl_high, 2,
	  parse_uint },
	{ "dir_acl", &set_inode.i_dir_acl, 4, parse_uint },
	{ "size_high", &set_inode.i_size_high, 4, parse_uint },
	{ "faddr", &set_inode.i_faddr, 4, parse_uint },
	{ "blocks_hi", &set_inode.osd2.linux2.l_i_blocks_hi, 2, parse_uint },
	{ "frag", &set_inode.osd2.hurd2.h_i_frag, 1, parse_uint },
	{ "fsize", &set_inode.osd2.hurd2.h_i_fsize, 1, parse_uint },
	{ "uid_high", &set_inode.osd2.linux2.l_i_uid_high, 2, parse_uint },
	{ "gid_high", &set_inode.osd2.linux2.l_i_gid_high, 2, parse_uint },
	{ "author", &set_inode.osd2.hurd2.h_i_author, 4, parse_uint },
	{ "bmap", NULL, 4, parse_bmap, FLAG_ARRAY },
	{ 0, 0, 0, 0 }
};

static struct field_set_info ext2_bg_fields[] = {
	{ "block_bitmap", &set_gd.bg_block_bitmap, 4, parse_uint },
	{ "inode_bitmap", &set_gd.bg_inode_bitmap, 4, parse_uint },
	{ "inode_table", &set_gd.bg_inode_table, 4, parse_uint },
	{ "free_blocks_count", &set_gd.bg_free_blocks_count, 2, parse_uint },
	{ "free_inodes_count", &set_gd.bg_free_inodes_count, 2, parse_uint },
	{ "used_dirs_count", &set_gd.bg_used_dirs_count, 2, parse_uint },
	{ "flags", &set_gd.bg_flags, 2, parse_uint },
	{ "reserved", &set_gd.bg_reserved, 2, parse_uint, FLAG_ARRAY, 2 },
	{ "itable_unused", &set_gd.bg_itable_unused, 2, parse_uint },
	{ "checksum", &set_gd.bg_checksum, 2, parse_gd_csum },
	{ 0, 0, 0, 0 }
};


static struct field_set_info *find_field(struct field_set_info *fields,
					 char *field)
{
	struct field_set_info *ss;
	const char	*prefix;
	char		*arg, *delim, *idx, *tmp;
	int		prefix_len;

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

	for (ss = fields ; ss->name ; ss++) {
		if (ss->flags & FLAG_ARRAY) {
			if (!idx || (strcmp(ss->name, arg) != 0))
				continue;
			if (ss->max_idx > 0 && array_idx >= ss->max_idx)
				continue;
		} else {
			if (strcmp(ss->name, field) != 0)
				continue;
		}
		free(arg);
		return ss;
	}
	free(arg);
	return NULL;
}

static errcode_t parse_uint(struct field_set_info *info, char *arg)
{
	unsigned long long num, limit;
	char *tmp;
	union {
		__u64	*ptr64;
		__u32	*ptr32;
		__u16	*ptr16;
		__u8	*ptr8;
	} u;

	u.ptr8 = (__u8 *) info->ptr;
	if (info->flags & FLAG_ARRAY)
		u.ptr8 += array_idx * info->size;

	errno = 0;
	num = STRTOULL(arg, &tmp, 0);
	if (*tmp || errno) {
		fprintf(stderr, "Couldn't parse '%s' for field %s.\n",
			arg, info->name);
		return EINVAL;
	}
	limit = ~0ULL >> ((8 - info->size) * 8);
	if (num > limit) {
		fprintf(stderr, "Value '%s' exceeds field %s maximum %llu.\n",
			arg, info->name, limit);
		return EINVAL;
	}
	switch (info->size) {
	case 8:
		*u.ptr64 = num;
		break;
	case 4:
		*u.ptr32 = num;
		break;
	case 2:
		*u.ptr16 = num;
		break;
	case 1:
		*u.ptr8 = num;
		break;
	}
	return 0;
}

static errcode_t parse_int(struct field_set_info *info, char *arg)
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

static errcode_t parse_string(struct field_set_info *info, char *arg)
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

static errcode_t parse_time(struct field_set_info *info, char *arg)
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

static errcode_t parse_uuid(struct field_set_info *info, char *arg)
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

static errcode_t parse_hashalg(struct field_set_info *info, char *arg)
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

static errcode_t parse_bmap(struct field_set_info *info, char *arg)
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

	retval = ext2fs_bmap(current_fs, set_ino, &set_inode, 0, BMAP_SET,
			     array_idx, &blk);
	if (retval) {
		com_err("set_inode", retval, "while setting block map");
	}
	return retval;
}

static errcode_t parse_gd_csum(struct field_set_info *info, char *arg)
{

	if (strcmp(arg, "calc") == 0) {
		ext2fs_group_desc_csum_set(current_fs, set_bg);
		set_gd = current_fs->group_desc[set_bg];
		printf("Checksum set to 0x%04x\n",
		       current_fs->group_desc[set_bg].bg_checksum);
		return 0;
	}

	return parse_uint(info, arg);
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
		strcpy(name, ss->name);
		if (ss->flags & FLAG_ARRAY) {
			if (ss->max_idx > 0)
				sprintf(idx, "[%d]", ss->max_idx);
			else
				strcpy(idx, "[]");
			strcat(name, idx);
		}
		fprintf(f, "\t%-20s\t%s\n", name, type);
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
	if (ss->func(ss, argv[2]) == 0) {
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

	if (debugfs_read_inode(set_ino, &set_inode, argv[1]))
		return;

	if (ss->func(ss, argv[3]) == 0) {
		if (debugfs_write_inode(set_ino, &set_inode, argv[1]))
			return;
	}
}

void do_set_block_group_descriptor(int argc, char *argv[])
{
	const char *usage = "<bg number> <field> <value>\n"
		"\t\"set_block_group_descriptor -l\" will list the names of "
		"the fields in a block group descriptor\n\twhich can be set.";
	struct field_set_info	*ss;
	char			*end;

	if ((argc == 2) && !strcmp(argv[1], "-l")) {
		print_possible_fields(ext2_bg_fields);
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


	if ((ss = find_field(ext2_bg_fields, argv[2])) == 0) {
		com_err(argv[0], 0, "invalid field specifier: %s", argv[2]);
		return;
	}

	set_gd = current_fs->group_desc[set_bg];

	if (ss->func(ss, argv[3]) == 0) {
		current_fs->group_desc[set_bg] = set_gd;
		ext2fs_mark_super_dirty(current_fs);
	}
}
