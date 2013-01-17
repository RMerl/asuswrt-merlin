/* vi: set sw=4 ts=4: */
/*
 * ls.c			- List the contents of an ext2fs superblock
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright (C) 1995, 1996, 1997  Theodore Ts'o <tytso@mit.edu>
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>

#include "e2p.h"

static void print_user(unsigned short uid, FILE *f)
{
	struct passwd *pw = getpwuid(uid);
	fprintf(f, "%u (user %s)\n", uid,
			(pw == NULL ? "unknown" : pw->pw_name));
}

static void print_group(unsigned short gid, FILE *f)
{
	struct group *gr = getgrgid(gid);
	fprintf(f, "%u (group %s)\n", gid,
			(gr == NULL ? "unknown" : gr->gr_name));
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
	fprintf(f,
		"Last mounted on:          %s\n"
		"Filesystem UUID:          %s\n"
		"Filesystem magic number:  0x%04X\n"
		"Filesystem revision #:    %d",
		buf, e2p_uuid2str(sb->s_uuid), sb->s_magic, sb->s_rev_level);
	if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
		fprintf(f, " (original)\n");
#ifdef EXT2_DYNAMIC_REV
	} else if (sb->s_rev_level == EXT2_DYNAMIC_REV) {
		fprintf(f, " (dynamic)\n");
#endif
	} else
		fprintf(f, " (unknown)\n");
	print_features(sb, f);
	print_mntopts(sb, f);
	fprintf(f, "Filesystem state:        ");
	print_fs_state (f, sb->s_state);
	fprintf(f, "\nErrors behavior:          ");
	print_fs_errors(f, sb->s_errors);
	str = e2p_os2string(sb->s_creator_os);
	fprintf(f,
		"\n"
		"Filesystem OS type:       %s\n"
		"Inode count:              %u\n"
		"Block count:              %u\n"
		"Reserved block count:     %u\n"
		"Free blocks:              %u\n"
		"Free inodes:              %u\n"
		"First block:              %u\n"
		"Block size:               %u\n"
		"Fragment size:            %u\n",
		str, sb->s_inodes_count, sb->s_blocks_count, sb->s_r_blocks_count,
		sb->s_free_blocks_count, sb->s_free_inodes_count,
		sb->s_first_data_block, EXT2_BLOCK_SIZE(sb), EXT2_FRAG_SIZE(sb));
	free(str);
	if (sb->s_reserved_gdt_blocks)
		fprintf(f, "Reserved GDT blocks:      %u\n",
			sb->s_reserved_gdt_blocks);
	fprintf(f,
		"Blocks per group:         %u\n"
		"Fragments per group:      %u\n"
		"Inodes per group:         %u\n"
		"Inode blocks per group:   %u\n",
		sb->s_blocks_per_group, sb->s_frags_per_group,
		sb->s_inodes_per_group, inode_blocks_per_group);
	if (sb->s_first_meta_bg)
		fprintf(f, "First meta block group:   %u\n",
			sb->s_first_meta_bg);
	if (sb->s_mkfs_time) {
		tm = sb->s_mkfs_time;
		fprintf(f, "Filesystem created:       %s", ctime(&tm));
	}
	tm = sb->s_mtime;
	fprintf(f, "Last mount time:          %s",
		sb->s_mtime ? ctime(&tm) : "n/a\n");
	tm = sb->s_wtime;
	fprintf(f,
		"Last write time:          %s"
		"Mount count:              %u\n"
		"Maximum mount count:      %d\n",
		ctime(&tm), sb->s_mnt_count, sb->s_max_mnt_count);
	tm = sb->s_lastcheck;
	fprintf(f,
		"Last checked:             %s"
		"Check interval:           %u (%s)\n",
		ctime(&tm),
		sb->s_checkinterval, interval_string(sb->s_checkinterval));
	if (sb->s_checkinterval)
	{
		time_t next;

		next = sb->s_lastcheck + sb->s_checkinterval;
		fprintf(f, "Next check after:         %s", ctime(&next));
	}
	fprintf(f, "Reserved blocks uid:      ");
	print_user(sb->s_def_resuid, f);
	fprintf(f, "Reserved blocks gid:      ");
	print_group(sb->s_def_resgid, f);
	if (sb->s_rev_level >= EXT2_DYNAMIC_REV) {
		fprintf(f,
			"First inode:              %d\n"
			"Inode size:		  %d\n",
			sb->s_first_ino, sb->s_inode_size);
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
		if (sb->s_jnl_backup_type == 1)
			fprintf(f, "inode blocks\n");
		else
			fprintf(f, "type %u\n", sb->s_jnl_backup_type);
	}
}
