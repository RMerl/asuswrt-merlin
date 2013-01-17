/* vi: set sw=4 ts=4: */
/*
 * tune2fs.c - Change the file system parameters on an ext2 file system
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997, 1998, 1999, 2000 by Theodore Ts'o.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/*
 * History:
 * 93/06/01	- Creation
 * 93/10/31	- Added the -c option to change the maximal mount counts
 * 93/12/14	- Added -l flag to list contents of superblock
 *                M.J.E. Mol (marcel@duteca.et.tudelft.nl)
 *                F.W. ten Wolde (franky@duteca.et.tudelft.nl)
 * 93/12/29	- Added the -e option to change errors behavior
 * 94/02/27	- Ported to use the ext2fs library
 * 94/03/06	- Added the checks interval from Uwe Ohse (uwe@tirka.gun.de)
 */

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include "e2fsbb.h"
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"
#include "ext2fs/kernel-jbd.h"
#include "util.h"
#include "blkid/blkid.h"

#include "libbb.h"

static char * device_name = NULL;
static char * new_label, *new_last_mounted, *new_UUID;
static char * io_options;
static int c_flag, C_flag, e_flag, f_flag, g_flag, i_flag, l_flag, L_flag;
static int m_flag, M_flag, r_flag, s_flag = -1, u_flag, U_flag, T_flag;
static time_t last_check_time;
static int print_label;
static int max_mount_count, mount_count, mount_flags;
static unsigned long interval, reserved_blocks;
static unsigned reserved_ratio;
static unsigned long resgid, resuid;
static unsigned short errors;
static int open_flag;
static char *features_cmd;
static char *mntopts_cmd;

static int journal_size, journal_flags;
static char *journal_device = NULL;

static const char *please_fsck = "Please run e2fsck on the filesystem\n";

static __u32 ok_features[3] = {
	EXT3_FEATURE_COMPAT_HAS_JOURNAL | EXT2_FEATURE_COMPAT_DIR_INDEX,
	EXT2_FEATURE_INCOMPAT_FILETYPE,
	EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER
};

/*
 * Remove an external journal from the filesystem
 */
static void remove_journal_device(ext2_filsys fs)
{
	char		*journal_path;
	ext2_filsys	jfs;
	char		buf[1024];
	journal_superblock_t	*jsb;
	int		i, nr_users;
	errcode_t	retval;
	int		commit_remove_journal = 0;
	io_manager	io_ptr;

	if (f_flag)
		commit_remove_journal = 1; /* force removal even if error */

	uuid_unparse(fs->super->s_journal_uuid, buf);
	journal_path = blkid_get_devname(NULL, "UUID", buf);

	if (!journal_path) {
		journal_path =
			ext2fs_find_block_device(fs->super->s_journal_dev);
		if (!journal_path)
			return;
	}

	io_ptr = unix_io_manager;
	retval = ext2fs_open(journal_path, EXT2_FLAG_RW|
			     EXT2_FLAG_JOURNAL_DEV_OK, 0,
			     fs->blocksize, io_ptr, &jfs);
	if (retval) {
		bb_error_msg("Failed to open external journal");
		goto no_valid_journal;
	}
	if (!(jfs->super->s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)) {
		bb_error_msg("%s is not a journal device", journal_path);
		goto no_valid_journal;
	}

	/* Get the journal superblock */
	if ((retval = io_channel_read_blk(jfs->io, 1, -1024, buf))) {
		bb_error_msg("Failed to read journal superblock");
		goto no_valid_journal;
	}

	jsb = (journal_superblock_t *) buf;
	if ((jsb->s_header.h_magic != (unsigned) ntohl(JFS_MAGIC_NUMBER)) ||
	    (jsb->s_header.h_blocktype != (unsigned) ntohl(JFS_SUPERBLOCK_V2))) {
		bb_error_msg("Journal superblock not found!");
		goto no_valid_journal;
	}

	/* Find the filesystem UUID */
	nr_users = ntohl(jsb->s_nr_users);
	for (i=0; i < nr_users; i++) {
		if (memcmp(fs->super->s_uuid,
			   &jsb->s_users[i*16], 16) == 0)
			break;
	}
	if (i >= nr_users) {
		bb_error_msg("Filesystem's UUID not found on journal device");
		commit_remove_journal = 1;
		goto no_valid_journal;
	}
	nr_users--;
	for (i=0; i < nr_users; i++)
		memcpy(&jsb->s_users[i*16], &jsb->s_users[(i+1)*16], 16);
	jsb->s_nr_users = htonl(nr_users);

	/* Write back the journal superblock */
	if ((retval = io_channel_write_blk(jfs->io, 1, -1024, buf))) {
		bb_error_msg("Failed to write journal superblock");
		goto no_valid_journal;
	}

	commit_remove_journal = 1;

no_valid_journal:
	if (commit_remove_journal == 0)
		bb_error_msg_and_die("Journal NOT removed");
	fs->super->s_journal_dev = 0;
	uuid_clear(fs->super->s_journal_uuid);
	ext2fs_mark_super_dirty(fs);
	puts("Journal removed");
	free(journal_path);
}

/* Helper function for remove_journal_inode */
static int release_blocks_proc(ext2_filsys fs, blk_t *blocknr,
			       int blockcnt EXT2FS_ATTR((unused)),
			       void *private EXT2FS_ATTR((unused)))
{
	blk_t	block;
	int	group;

	block = *blocknr;
	ext2fs_unmark_block_bitmap(fs->block_map,block);
	group = ext2fs_group_of_blk(fs, block);
	fs->group_desc[group].bg_free_blocks_count++;
	fs->super->s_free_blocks_count++;
	return 0;
}

/*
 * Remove the journal inode from the filesystem
 */
static void remove_journal_inode(ext2_filsys fs)
{
	struct ext2_inode	inode;
	errcode_t		retval;
	ino_t			ino = fs->super->s_journal_inum;
	char *msg = "to read";
	char *s = "journal inode";

	retval = ext2fs_read_inode(fs, ino,  &inode);
	if (retval)
		goto REMOVE_JOURNAL_INODE_ERROR;
	if (ino == EXT2_JOURNAL_INO) {
		retval = ext2fs_read_bitmaps(fs);
		if (retval) {
			msg = "to read bitmaps";
			s = "";
			goto REMOVE_JOURNAL_INODE_ERROR;
		}
		retval = ext2fs_block_iterate(fs, ino, 0, NULL,
					      release_blocks_proc, NULL);
		if (retval) {
			msg = "clearing";
			goto REMOVE_JOURNAL_INODE_ERROR;
		}
		memset(&inode, 0, sizeof(inode));
		ext2fs_mark_bb_dirty(fs);
		fs->flags &= ~EXT2_FLAG_SUPER_ONLY;
	} else
		inode.i_flags &= ~EXT2_IMMUTABLE_FL;
	retval = ext2fs_write_inode(fs, ino, &inode);
	if (retval) {
		msg = "writing";
REMOVE_JOURNAL_INODE_ERROR:
		bb_error_msg_and_die("Failed %s %s", msg, s);
	}
	fs->super->s_journal_inum = 0;
	ext2fs_mark_super_dirty(fs);
}

/*
 * Update the default mount options
 */
static void update_mntopts(ext2_filsys fs, char *mntopts)
{
	struct ext2_super_block *sb= fs->super;

	if (e2p_edit_mntopts(mntopts, &sb->s_default_mount_opts, ~0))
		bb_error_msg_and_die("Invalid mount option set: %s", mntopts);
	ext2fs_mark_super_dirty(fs);
}

/*
 * Update the feature set as provided by the user.
 */
static void update_feature_set(ext2_filsys fs, char *features)
{
	int sparse, old_sparse, filetype, old_filetype;
	int journal, old_journal, dxdir, old_dxdir;
	struct ext2_super_block *sb= fs->super;
	__u32	old_compat, old_incompat, old_ro_compat;

	old_compat = sb->s_feature_compat;
	old_ro_compat = sb->s_feature_ro_compat;
	old_incompat = sb->s_feature_incompat;

	old_sparse = sb->s_feature_ro_compat &
		EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
	old_filetype = sb->s_feature_incompat &
		EXT2_FEATURE_INCOMPAT_FILETYPE;
	old_journal = sb->s_feature_compat &
		EXT3_FEATURE_COMPAT_HAS_JOURNAL;
	old_dxdir = sb->s_feature_compat &
		EXT2_FEATURE_COMPAT_DIR_INDEX;
	if (e2p_edit_feature(features, &sb->s_feature_compat, ok_features))
		bb_error_msg_and_die("Invalid filesystem option set: %s", features);
	sparse = sb->s_feature_ro_compat &
		EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
	filetype = sb->s_feature_incompat &
		EXT2_FEATURE_INCOMPAT_FILETYPE;
	journal = sb->s_feature_compat &
		EXT3_FEATURE_COMPAT_HAS_JOURNAL;
	dxdir = sb->s_feature_compat &
		EXT2_FEATURE_COMPAT_DIR_INDEX;
	if (old_journal && !journal) {
		if ((mount_flags & EXT2_MF_MOUNTED) &&
		    !(mount_flags & EXT2_MF_READONLY)) {
			bb_error_msg_and_die(
				"The has_journal flag may only be "
				"cleared when the filesystem is\n"
				"unmounted or mounted "
				"read-only");
		}
		if (sb->s_feature_incompat &
		    EXT3_FEATURE_INCOMPAT_RECOVER) {
			bb_error_msg_and_die(
				"The needs_recovery flag is set.  "
				"%s before clearing the has_journal flag.",
				please_fsck);
		}
		if (sb->s_journal_inum) {
			remove_journal_inode(fs);
		}
		if (sb->s_journal_dev) {
			remove_journal_device(fs);
		}
	}
	if (journal && !old_journal) {
		/*
		 * If adding a journal flag, let the create journal
		 * code below handle creating setting the flag and
		 * creating the journal.  We supply a default size if
		 * necessary.
		 */
		if (!journal_size)
			journal_size = -1;
		sb->s_feature_compat &= ~EXT3_FEATURE_COMPAT_HAS_JOURNAL;
	}
	if (dxdir && !old_dxdir) {
		if (!sb->s_def_hash_version)
			sb->s_def_hash_version = EXT2_HASH_TEA;
		if (uuid_is_null((unsigned char *) sb->s_hash_seed))
			uuid_generate((unsigned char *) sb->s_hash_seed);
	}

	if (sb->s_rev_level == EXT2_GOOD_OLD_REV &&
	    (sb->s_feature_compat || sb->s_feature_ro_compat ||
	     sb->s_feature_incompat))
		ext2fs_update_dynamic_rev(fs);
	if ((sparse != old_sparse) ||
	    (filetype != old_filetype)) {
		sb->s_state &= ~EXT2_VALID_FS;
		printf("\n%s\n", please_fsck);
	}
	if ((old_compat != sb->s_feature_compat) ||
	    (old_ro_compat != sb->s_feature_ro_compat) ||
	    (old_incompat != sb->s_feature_incompat))
		ext2fs_mark_super_dirty(fs);
}

/*
 * Add a journal to the filesystem.
 */
static void add_journal(ext2_filsys fs)
{
	if (fs->super->s_feature_compat &
	    EXT3_FEATURE_COMPAT_HAS_JOURNAL) {
		bb_error_msg_and_die("The filesystem already has a journal");
	}
	if (journal_device) {
		make_journal_device(journal_device, fs, 0, 0);
	} else if (journal_size) {
		make_journal_blocks(fs, journal_size, journal_flags, 0);
		/*
		 * If the filesystem wasn't mounted, we need to force
		 * the block group descriptors out.
		 */
		if ((mount_flags & EXT2_MF_MOUNTED) == 0)
			fs->flags &= ~EXT2_FLAG_SUPER_ONLY;
	}
	print_check_message(fs);
}

/*
 * Busybox stuff
 */
static char * x_blkid_get_devname(const char *token)
{
	char * dev_name;

	if (!(dev_name = blkid_get_devname(NULL, token, NULL)))
		bb_error_msg_and_die("Unable to resolve '%s'", token);
	return dev_name;
}

#ifdef CONFIG_E2LABEL
static void parse_e2label_options(int argc, char ** argv)
{
	if ((argc < 2) || (argc > 3))
		bb_show_usage();
	io_options = strchr(argv[1], '?');
	if (io_options)
		*io_options++ = 0;
	device_name = x_blkid_get_devname(argv[1]);
	if (argc == 3) {
		open_flag = EXT2_FLAG_RW | EXT2_FLAG_JOURNAL_DEV_OK;
		L_flag = 1;
		new_label = argv[2];
	} else
		print_label++;
}
#else
#define parse_e2label_options(x,y)
#endif

static time_t parse_time(char *str)
{
	struct	tm	ts;

	if (strcmp(str, "now") == 0) {
		return time(0);
	}
	memset(&ts, 0, sizeof(ts));
#ifdef HAVE_STRPTIME
	strptime(str, "%Y%m%d%H%M%S", &ts);
#else
	sscanf(str, "%4d%2d%2d%2d%2d%2d", &ts.tm_year, &ts.tm_mon,
	       &ts.tm_mday, &ts.tm_hour, &ts.tm_min, &ts.tm_sec);
	ts.tm_year -= 1900;
	ts.tm_mon -= 1;
	if (ts.tm_year < 0 || ts.tm_mon < 0 || ts.tm_mon > 11 ||
	    ts.tm_mday < 0 || ts.tm_mday > 31 || ts.tm_hour > 23 ||
	    ts.tm_min > 59 || ts.tm_sec > 61)
		ts.tm_mday = 0;
#endif
	if (ts.tm_mday == 0) {
		bb_error_msg_and_die("can't parse date/time specifier: %s", str);
	}
	return mktime(&ts);
}

static void parse_tune2fs_options(int argc, char **argv)
{
	int c;
	char * tmp;

	printf("tune2fs %s (%s)\n", E2FSPROGS_VERSION, E2FSPROGS_DATE);
	while ((c = getopt(argc, argv, "c:e:fg:i:jlm:o:r:s:u:C:J:L:M:O:T:U:")) != EOF)
		switch (c)
		{
			case 'c':
				max_mount_count = xatou_range(optarg, 0, 16000);
				if (max_mount_count == 0)
					max_mount_count = -1;
				c_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'C':
				mount_count = xatou_range(optarg, 0, 16000);
				C_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'e':
				if (strcmp (optarg, "continue") == 0)
					errors = EXT2_ERRORS_CONTINUE;
				else if (strcmp (optarg, "remount-ro") == 0)
					errors = EXT2_ERRORS_RO;
				else if (strcmp (optarg, "panic") == 0)
					errors = EXT2_ERRORS_PANIC;
				else {
					bb_error_msg_and_die("bad error behavior - %s", optarg);
				}
				e_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'f': /* Force */
				f_flag = 1;
				break;
			case 'g':
				resgid = bb_strtoul(optarg, NULL, 10);
				if (errno)
					resgid = xgroup2gid(optarg);
				g_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'i':
				interval = strtoul(optarg, &tmp, 0);
				switch (*tmp) {
				case 's':
					tmp++;
					break;
				case '\0':
				case 'd':
				case 'D': /* days */
					interval *= 86400;
					if (*tmp != '\0')
						tmp++;
					break;
				case 'm':
				case 'M': /* months! */
					interval *= 86400 * 30;
					tmp++;
					break;
				case 'w':
				case 'W': /* weeks */
					interval *= 86400 * 7;
					tmp++;
					break;
				}
				if (*tmp || interval > (365 * 86400)) {
					bb_error_msg_and_die("bad interval - %s", optarg);
				}
				i_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'j':
				if (!journal_size)
					journal_size = -1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'J':
				parse_journal_opts(&journal_device, &journal_flags, &journal_size, optarg);
				open_flag = EXT2_FLAG_RW;
				break;
			case 'l':
				l_flag = 1;
				break;
			case 'L':
				new_label = optarg;
				L_flag = 1;
				open_flag = EXT2_FLAG_RW |
					EXT2_FLAG_JOURNAL_DEV_OK;
				break;
			case 'm':
				reserved_ratio = xatou_range(optarg, 0, 50);
				m_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'M':
				new_last_mounted = optarg;
				M_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'o':
				if (mntopts_cmd) {
					bb_error_msg_and_die("-o may only be specified once");
				}
				mntopts_cmd = optarg;
				open_flag = EXT2_FLAG_RW;
				break;

			case 'O':
				if (features_cmd) {
					bb_error_msg_and_die("-O may only be specified once");
				}
				features_cmd = optarg;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'r':
				reserved_blocks = xatoul(optarg);
				r_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 's':
				s_flag = atoi(optarg);
				open_flag = EXT2_FLAG_RW;
				break;
			case 'T':
				T_flag = 1;
				last_check_time = parse_time(optarg);
				open_flag = EXT2_FLAG_RW;
				break;
			case 'u':
				resuid = bb_strtoul(optarg, NULL, 10);
				if (errno)
					resuid = xuname2uid(optarg);
				u_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'U':
				new_UUID = optarg;
				U_flag = 1;
				open_flag = EXT2_FLAG_RW |
					EXT2_FLAG_JOURNAL_DEV_OK;
				break;
			default:
				bb_show_usage();
		}
	if (optind < argc - 1 || optind == argc)
		bb_show_usage();
	if (!open_flag && !l_flag)
		bb_show_usage();
	io_options = strchr(argv[optind], '?');
	if (io_options)
		*io_options++ = 0;
	device_name = x_blkid_get_devname(argv[optind]);
}

static void tune2fs_clean_up(void)
{
	if (ENABLE_FEATURE_CLEAN_UP && device_name) free(device_name);
	if (ENABLE_FEATURE_CLEAN_UP && journal_device) free(journal_device);
}

int tune2fs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tune2fs_main(int argc, char **argv)
{
	errcode_t retval;
	ext2_filsys fs;
	struct ext2_super_block *sb;
	io_manager io_ptr;

	if (ENABLE_FEATURE_CLEAN_UP)
		atexit(tune2fs_clean_up);

	if (ENABLE_E2LABEL && (applet_name[0] == 'e')) /* e2label */
		parse_e2label_options(argc, argv);
	else
		parse_tune2fs_options(argc, argv);  /* tune2fs */

	io_ptr = unix_io_manager;
	retval = ext2fs_open2(device_name, io_options, open_flag,
			      0, 0, io_ptr, &fs);
	if (retval)
		bb_error_msg_and_die("No valid superblock on %s", device_name);
	sb = fs->super;
	if (print_label) {
		/* For e2label emulation */
		printf("%.*s\n", (int) sizeof(sb->s_volume_name),
		       sb->s_volume_name);
		return 0;
	}
	retval = ext2fs_check_if_mounted(device_name, &mount_flags);
	if (retval)
		bb_error_msg_and_die("can't determine if %s is mounted", device_name);
	/* Normally we only need to write out the superblock */
	fs->flags |= EXT2_FLAG_SUPER_ONLY;

	if (c_flag) {
		sb->s_max_mnt_count = max_mount_count;
		ext2fs_mark_super_dirty(fs);
		printf("Setting maximal mount count to %d\n", max_mount_count);
	}
	if (C_flag) {
		sb->s_mnt_count = mount_count;
		ext2fs_mark_super_dirty(fs);
		printf("Setting current mount count to %d\n", mount_count);
	}
	if (e_flag) {
		sb->s_errors = errors;
		ext2fs_mark_super_dirty(fs);
		printf("Setting error behavior to %d\n", errors);
	}
	if (g_flag) {
		sb->s_def_resgid = resgid;
		ext2fs_mark_super_dirty(fs);
		printf("Setting reserved blocks gid to %lu\n", resgid);
	}
	if (i_flag) {
		sb->s_checkinterval = interval;
		ext2fs_mark_super_dirty(fs);
		printf("Setting interval between check %lu seconds\n", interval);
	}
	if (m_flag) {
		sb->s_r_blocks_count = (sb->s_blocks_count / 100)
				* reserved_ratio;
		ext2fs_mark_super_dirty(fs);
		printf("Setting reserved blocks percentage to %u (%u blocks)\n",
			reserved_ratio, sb->s_r_blocks_count);
	}
	if (r_flag) {
		if (reserved_blocks >= sb->s_blocks_count/2)
			bb_error_msg_and_die("reserved blocks count is too big (%lu)", reserved_blocks);
		sb->s_r_blocks_count = reserved_blocks;
		ext2fs_mark_super_dirty(fs);
		printf("Setting reserved blocks count to %lu\n", reserved_blocks);
	}
	if (s_flag == 1) {
		if (sb->s_feature_ro_compat &
		    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
			bb_error_msg("\nThe filesystem already has sparse superblocks");
		else {
			sb->s_feature_ro_compat |=
				EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			sb->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(fs);
			printf("\nSparse superblock flag set.  %s", please_fsck);
		}
	}
	if (s_flag == 0) {
		if (!(sb->s_feature_ro_compat &
		      EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
			bb_error_msg("\nThe filesystem already has sparse superblocks disabled");
		else {
			sb->s_feature_ro_compat &=
				~EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			sb->s_state &= ~EXT2_VALID_FS;
			fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;
			ext2fs_mark_super_dirty(fs);
			printf("\nSparse superblock flag cleared.  %s", please_fsck);
		}
	}
	if (T_flag) {
		sb->s_lastcheck = last_check_time;
		ext2fs_mark_super_dirty(fs);
		printf("Setting time filesystem last checked to %s\n",
		       ctime(&last_check_time));
	}
	if (u_flag) {
		sb->s_def_resuid = resuid;
		ext2fs_mark_super_dirty(fs);
		printf("Setting reserved blocks uid to %lu\n", resuid);
	}
	if (L_flag) {
		if (strlen(new_label) > sizeof(sb->s_volume_name))
			bb_error_msg("Warning: label too long, truncating");
		memset(sb->s_volume_name, 0, sizeof(sb->s_volume_name));
		safe_strncpy(sb->s_volume_name, new_label,
			sizeof(sb->s_volume_name));
		ext2fs_mark_super_dirty(fs);
	}
	if (M_flag) {
		memset(sb->s_last_mounted, 0, sizeof(sb->s_last_mounted));
		safe_strncpy(sb->s_last_mounted, new_last_mounted,
			sizeof(sb->s_last_mounted));
		ext2fs_mark_super_dirty(fs);
	}
	if (mntopts_cmd)
		update_mntopts(fs, mntopts_cmd);
	if (features_cmd)
		update_feature_set(fs, features_cmd);
	if (journal_size || journal_device)
		add_journal(fs);

	if (U_flag) {
		if ((strcasecmp(new_UUID, "null") == 0) ||
		    (strcasecmp(new_UUID, "clear") == 0)) {
			uuid_clear(sb->s_uuid);
		} else if (strcasecmp(new_UUID, "time") == 0) {
			uuid_generate_time(sb->s_uuid);
		} else if (strcasecmp(new_UUID, "random") == 0) {
			uuid_generate(sb->s_uuid);
		} else if (uuid_parse(new_UUID, sb->s_uuid)) {
			bb_error_msg_and_die("Invalid UUID format");
		}
		ext2fs_mark_super_dirty(fs);
	}

	if (l_flag)
		list_super (sb);
	return (ext2fs_close (fs) ? 1 : 0);
}
