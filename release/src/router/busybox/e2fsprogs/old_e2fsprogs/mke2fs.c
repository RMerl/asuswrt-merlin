/* vi: set sw=4 ts=4: */
/*
 * mke2fs.c - Make a ext2fs filesystem.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
 *	2003, 2004, 2005 by Theodore Ts'o.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* Usage: mke2fs [options] device
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-).
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <mntent.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "e2fsbb.h"
#include "ext2fs/ext2_fs.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"
#include "ext2fs/ext2fs.h"
#include "util.h"

#define STRIDE_LENGTH 8

#ifndef __sparc__
#define ZAP_BOOTBLOCK
#endif

static const char * device_name;

/* Command line options */
static int	cflag;
static int	quiet;
static int	super_only;
static int	force;
static int	noaction;
static int	journal_size;
static int	journal_flags;
static const char *bad_blocks_filename;
static __u32	fs_stride;

static struct ext2_super_block param;
static char *creator_os;
static char *volume_label;
static char *mount_dir;
static char *journal_device = NULL;
static int   sync_kludge; /* Set using the MKE2FS_SYNC env. option */

static int sys_page_size = 4096;
static int linux_version_code = 0;

static int int_log2(int arg)
{
	int l = 0;

	arg >>= 1;
	while (arg) {
		l++;
		arg >>= 1;
	}
	return l;
}

static int int_log10(unsigned int arg)
{
	int	l;

	for (l = 0; arg; l++)
		arg = arg / 10;
	return l;
}

/*
 * This function sets the default parameters for a filesystem
 *
 * The type is specified by the user.  The size is the maximum size
 * (in megabytes) for which a set of parameters applies, with a size
 * of zero meaning that it is the default parameter for the type.
 * Note that order is important in the table below.
 */
#define DEF_MAX_BLOCKSIZE -1
static const char default_str[] = "default";
struct mke2fs_defaults {
	const char	*type;
	int		size;
	int		blocksize;
	int		inode_ratio;
};

static const struct mke2fs_defaults settings[] = {
	{ default_str,	 0, 4096, 8192 },
	{ default_str, 512, 1024, 4096 },
	{ default_str,	 3, 1024, 8192 },
	{ "journal",	 0, 4096, 8192 },
	{ "news",	 0, 4096, 4096 },
	{ "largefile",	 0, 4096, 1024 * 1024 },
	{ "largefile4",  0, 4096, 4096 * 1024 },
	{ 0,		 0,    0, 0},
};

static void set_fs_defaults(const char *fs_type,
			    struct ext2_super_block *super,
			    int blocksize, int sector_size,
			    int *inode_ratio)
{
	int	megs;
	int	ratio = 0;
	const struct mke2fs_defaults *p;
	int	use_bsize = 1024;

	megs = super->s_blocks_count * (EXT2_BLOCK_SIZE(super) / 1024) / 1024;
	if (inode_ratio)
		ratio = *inode_ratio;
	if (!fs_type)
		fs_type = default_str;
	for (p = settings; p->type; p++) {
		if ((strcmp(p->type, fs_type) != 0) &&
		    (strcmp(p->type, default_str) != 0))
			continue;
		if ((p->size != 0) && (megs > p->size))
			continue;
		if (ratio == 0)
			*inode_ratio = p->inode_ratio < blocksize ?
				blocksize : p->inode_ratio;
		use_bsize = p->blocksize;
	}
	if (blocksize <= 0) {
		if (use_bsize == DEF_MAX_BLOCKSIZE) {
			use_bsize = sys_page_size;
			if ((linux_version_code < (2*65536 + 6*256)) &&
			    (use_bsize > 4096))
				use_bsize = 4096;
		}
		if (sector_size && use_bsize < sector_size)
			use_bsize = sector_size;
		if ((blocksize < 0) && (use_bsize < (-blocksize)))
			use_bsize = -blocksize;
		blocksize = use_bsize;
		super->s_blocks_count /= blocksize / 1024;
	}
	super->s_log_frag_size = super->s_log_block_size =
		int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);
}


/*
 * Helper function for read_bb_file and test_disk
 */
static void invalid_block(ext2_filsys fs EXT2FS_ATTR((unused)), blk_t blk)
{
	bb_error_msg("Bad block %u out of range; ignored", blk);
}

/*
 * Busybox stuff
 */
static void mke2fs_error_msg_and_die(int retval, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void mke2fs_error_msg_and_die(int retval, const char *fmt, ...)
{
	va_list ap;

	if (retval) {
		va_start(ap, fmt);
		fprintf(stderr, "\nCould not ");
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		va_end(ap);
		exit(EXIT_FAILURE);
	}
}

static void mke2fs_verbose(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void mke2fs_verbose(const char *fmt, ...)
{
	va_list ap;

	if (!quiet) {
		va_start(ap, fmt);
		vfprintf(stdout, fmt, ap);
		fflush(stdout);
		va_end(ap);
	}
}

static void mke2fs_verbose_done(void)
{
	mke2fs_verbose("done\n");
}

static void mke2fs_warning_msg(int retval, char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
static void mke2fs_warning_msg(int retval, char *fmt, ... )
{
	va_list ap;

	if (retval) {
		va_start(ap, fmt);
		fprintf(stderr, "\nWarning: ");
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
}

/*
 * Reads the bad blocks list from a file
 */
static void read_bb_file(ext2_filsys fs, badblocks_list *bb_list,
			 const char *bad_blocks_file)
{
	FILE		*f;
	errcode_t	retval;

	f = xfopen_for_read(bad_blocks_file);
	retval = ext2fs_read_bb_FILE(fs, f, bb_list, invalid_block);
	fclose (f);
	mke2fs_error_msg_and_die(retval, "read bad blocks from list");
}

/*
 * Runs the badblocks program to test the disk
 */
static void test_disk(ext2_filsys fs, badblocks_list *bb_list)
{
	FILE		*f;
	errcode_t	retval;
	char		buf[1024];

	sprintf(buf, "badblocks -b %d %s%s%s %d", fs->blocksize,
		quiet ? "" : "-s ", (cflag > 1) ? "-w " : "",
		fs->device_name, fs->super->s_blocks_count);
	mke2fs_verbose("Running command: %s\n", buf);
	f = popen(buf, "r");
	if (!f) {
		bb_perror_msg_and_die("can't run '%s'", buf);
	}
	retval = ext2fs_read_bb_FILE(fs, f, bb_list, invalid_block);
	pclose(f);
	mke2fs_error_msg_and_die(retval, "read bad blocks from program");
}

static void handle_bad_blocks(ext2_filsys fs, badblocks_list bb_list)
{
	dgrp_t			i;
	blk_t			j;
	unsigned		must_be_good;
	blk_t			blk;
	badblocks_iterate	bb_iter;
	errcode_t		retval;
	blk_t			group_block;
	int			group;
	int			group_bad;

	if (!bb_list)
		return;

	/*
	 * The primary superblock and group descriptors *must* be
	 * good; if not, abort.
	 */
	must_be_good = fs->super->s_first_data_block + 1 + fs->desc_blocks;
	for (i = fs->super->s_first_data_block; i <= must_be_good; i++) {
		if (ext2fs_badblocks_list_test(bb_list, i)) {
			bb_error_msg_and_die(
				"Block %d in primary superblock/group descriptor area bad\n"
				"Blocks %d through %d must be good in order to build a filesystem\n"
				"Aborting ...", i, fs->super->s_first_data_block, must_be_good);
		}
	}

	/*
	 * See if any of the bad blocks are showing up in the backup
	 * superblocks and/or group descriptors.  If so, issue a
	 * warning and adjust the block counts appropriately.
	 */
	group_block = fs->super->s_first_data_block +
		fs->super->s_blocks_per_group;

	for (i = 1; i < fs->group_desc_count; i++) {
		group_bad = 0;
		for (j=0; j < fs->desc_blocks+1; j++) {
			if (ext2fs_badblocks_list_test(bb_list,
						       group_block + j)) {
				mke2fs_warning_msg(!group_bad,
					"the backup superblock/group descriptors at block %d contain\n"
					"bad blocks\n", group_block);
				group_bad++;
				group = ext2fs_group_of_blk(fs, group_block+j);
				fs->group_desc[group].bg_free_blocks_count++;
				fs->super->s_free_blocks_count++;
			}
		}
		group_block += fs->super->s_blocks_per_group;
	}

	/*
	 * Mark all the bad blocks as used...
	 */
	retval = ext2fs_badblocks_list_iterate_begin(bb_list, &bb_iter);
	mke2fs_error_msg_and_die(retval, "mark bad blocks as used");

	while (ext2fs_badblocks_list_iterate(bb_iter, &blk))
		ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_badblocks_list_iterate_end(bb_iter);
}

/*
 * These functions implement a generalized progress meter.
 */
struct progress_struct {
	char		format[20];
	char		backup[80];
	__u32		max;
	int		skip_progress;
};

static void progress_init(struct progress_struct *progress,
			  const char *label,__u32 max)
{
	int	i;

	memset(progress, 0, sizeof(struct progress_struct));
	if (quiet)
		return;

	/*
	 * Figure out how many digits we need
	 */
	i = int_log10(max);
	sprintf(progress->format, "%%%dd/%%%dld", i, i);
	memset(progress->backup, '\b', sizeof(progress->backup)-1);
	progress->backup[sizeof(progress->backup)-1] = 0;
	if ((2*i)+1 < (int) sizeof(progress->backup))
		progress->backup[(2*i)+1] = 0;
	progress->max = max;

	progress->skip_progress = 0;
	if (getenv("MKE2FS_SKIP_PROGRESS"))
		progress->skip_progress++;

	fputs(label, stdout);
	fflush(stdout);
}

static void progress_update(struct progress_struct *progress, __u32 val)
{
	if ((progress->format[0] == 0) || progress->skip_progress)
		return;
	printf(progress->format, val, progress->max);
	fputs(progress->backup, stdout);
}

static void progress_close(struct progress_struct *progress)
{
	if (progress->format[0] == 0)
		return;
	printf("%-28s\n", "done");
}


/*
 * Helper function which zeros out _num_ blocks starting at _blk_.  In
 * case of an error, the details of the error is returned via _ret_blk_
 * and _ret_count_ if they are non-NULL pointers.  Returns 0 on
 * success, and an error code on an error.
 *
 * As a special case, if the first argument is NULL, then it will
 * attempt to free the static zeroizing buffer.  (This is to keep
 * programs that check for memory leaks happy.)
 */
static errcode_t zero_blocks(ext2_filsys fs, blk_t blk, int num,
			     struct progress_struct *progress,
			     blk_t *ret_blk, int *ret_count)
{
	int		j, count, next_update, next_update_incr;
	static char	*buf;
	errcode_t	retval;

	/* If fs is null, clean up the static buffer and return */
	if (!fs) {
		if (buf) {
			free(buf);
			buf = 0;
		}
		return 0;
	}
	/* Allocate the zeroizing buffer if necessary */
	if (!buf) {
		buf = xzalloc(fs->blocksize * STRIDE_LENGTH);
	}
	/* OK, do the write loop */
	next_update = 0;
	next_update_incr = num / 100;
	if (next_update_incr < 1)
		next_update_incr = 1;
	for (j=0; j < num; j += STRIDE_LENGTH, blk += STRIDE_LENGTH) {
		count = num - j;
		if (count > STRIDE_LENGTH)
			count = STRIDE_LENGTH;
		retval = io_channel_write_blk(fs->io, blk, count, buf);
		if (retval) {
			if (ret_count)
				*ret_count = count;
			if (ret_blk)
				*ret_blk = blk;
			return retval;
		}
		if (progress && j > next_update) {
			next_update += num / 100;
			progress_update(progress, blk);
		}
	}
	return 0;
}

static void write_inode_tables(ext2_filsys fs)
{
	errcode_t	retval;
	blk_t		blk;
	dgrp_t		i;
	int		num;
	struct progress_struct progress;

	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, "Writing inode tables: ",
			      fs->group_desc_count);

	for (i = 0; i < fs->group_desc_count; i++) {
		progress_update(&progress, i);

		blk = fs->group_desc[i].bg_inode_table;
		num = fs->inode_blocks_per_group;

		retval = zero_blocks(fs, blk, num, 0, &blk, &num);
		mke2fs_error_msg_and_die(retval,
			"write %d blocks in inode table starting at %d.",
			num, blk);
		if (sync_kludge) {
			if (sync_kludge == 1)
				sync();
			else if ((i % sync_kludge) == 0)
				sync();
		}
	}
	zero_blocks(0, 0, 0, 0, 0, 0);
	progress_close(&progress);
}

static void create_root_dir(ext2_filsys fs)
{
	errcode_t		retval;
	struct ext2_inode	inode;

	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, 0);
	mke2fs_error_msg_and_die(retval, "create root dir");
	if (geteuid()) {
		retval = ext2fs_read_inode(fs, EXT2_ROOT_INO, &inode);
		mke2fs_error_msg_and_die(retval, "read root inode");
		inode.i_uid = getuid();
		if (inode.i_uid)
			inode.i_gid = getgid();
		retval = ext2fs_write_new_inode(fs, EXT2_ROOT_INO, &inode);
		mke2fs_error_msg_and_die(retval, "set root inode ownership");
	}
}

static void create_lost_and_found(ext2_filsys fs)
{
	errcode_t		retval;
	ext2_ino_t		ino;
	const char		*name = "lost+found";
	int			i = 1;
	char			*msg = "create";
	int			lpf_size = 0;

	fs->umask = 077;
	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, 0, name);
	if (retval) {
		goto CREATE_LOST_AND_FOUND_ERROR;
	}

	retval = ext2fs_lookup(fs, EXT2_ROOT_INO, name, strlen(name), 0, &ino);
	if (retval) {
		msg = "lookup";
		goto CREATE_LOST_AND_FOUND_ERROR;
	}

	for (; i < EXT2_NDIR_BLOCKS; i++) {
		if ((lpf_size += fs->blocksize) >= 16*1024)
			break;
		retval = ext2fs_expand_dir(fs, ino);
		msg = "expand";
CREATE_LOST_AND_FOUND_ERROR:
		mke2fs_error_msg_and_die(retval, "%s %s", msg, name);
	}
}

static void create_bad_block_inode(ext2_filsys fs, badblocks_list bb_list)
{
	errcode_t	retval;

	ext2fs_mark_inode_bitmap(fs->inode_map, EXT2_BAD_INO);
	fs->group_desc[0].bg_free_inodes_count--;
	fs->super->s_free_inodes_count--;
	retval = ext2fs_update_bb_inode(fs, bb_list);
	mke2fs_error_msg_and_die(retval, "set bad block inode");
}

static void reserve_inodes(ext2_filsys fs)
{
	ext2_ino_t	i;
	int		group;

	for (i = EXT2_ROOT_INO + 1; i < EXT2_FIRST_INODE(fs->super); i++) {
		ext2fs_mark_inode_bitmap(fs->inode_map, i);
		group = ext2fs_group_of_ino(fs, i);
		fs->group_desc[group].bg_free_inodes_count--;
		fs->super->s_free_inodes_count--;
	}
	ext2fs_mark_ib_dirty(fs);
}

#define BSD_DISKMAGIC   (0x82564557UL)  /* The disk magic number */
#define BSD_MAGICDISK   (0x57455682UL)  /* The disk magic number reversed */
#define BSD_LABEL_OFFSET        64

static void zap_sector(ext2_filsys fs, int sect, int nsect)
{
	char *buf;
	char *fmt = "could not %s %d";
	int retval;
	unsigned int *magic;

	buf = xmalloc(512*nsect);

	if (sect == 0) {
		/* Check for a BSD disklabel, and don't erase it if so */
		retval = io_channel_read_blk(fs->io, 0, -512, buf);
		if (retval)
			mke2fs_warning_msg(retval, fmt, "read block", 0);
		else {
			magic = (unsigned int *) (buf + BSD_LABEL_OFFSET);
			if ((*magic == BSD_DISKMAGIC) ||
			    (*magic == BSD_MAGICDISK))
				return;
		}
	}

	memset(buf, 0, 512*nsect);
	io_channel_set_blksize(fs->io, 512);
	retval = io_channel_write_blk(fs->io, sect, -512*nsect, buf);
	io_channel_set_blksize(fs->io, fs->blocksize);
	free(buf);
	mke2fs_warning_msg(retval, fmt, "erase sector", sect);
}

static void create_journal_dev(ext2_filsys fs)
{
	struct progress_struct	progress;
	errcode_t		retval;
	char			*buf;
	char			*fmt = "%s journal superblock";
	blk_t			blk;
	int			count;

	retval = ext2fs_create_journal_superblock(fs,
				  fs->super->s_blocks_count, 0, &buf);
	mke2fs_error_msg_and_die(retval, fmt, "init");
	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, "Zeroing journal device: ",
			      fs->super->s_blocks_count);

	retval = zero_blocks(fs, 0, fs->super->s_blocks_count,
			     &progress, &blk, &count);
	mke2fs_error_msg_and_die(retval, "zero journal device (block %u, count %d)",
			blk, count);
	zero_blocks(0, 0, 0, 0, 0, 0);

	retval = io_channel_write_blk(fs->io,
				      fs->super->s_first_data_block+1,
				      1, buf);
	mke2fs_error_msg_and_die(retval, fmt, "write");
	progress_close(&progress);
}

static void show_stats(ext2_filsys fs)
{
	struct ext2_super_block *s = fs->super;
	char			*os;
	blk_t			group_block;
	dgrp_t			i;
	int			need, col_left;

	mke2fs_warning_msg((param.s_blocks_count != s->s_blocks_count),
		"%d blocks unused\n", param.s_blocks_count - s->s_blocks_count);
	os = e2p_os2string(fs->super->s_creator_os);
	printf(	"Filesystem label=%.*s\n"
			"OS type: %s\n"
			"Block size=%u (log=%u)\n"
			"Fragment size=%u (log=%u)\n"
			"%u inodes, %u blocks\n"
			"%u blocks (%2.2f%%) reserved for the super user\n"
			"First data block=%u\n",
			(int) sizeof(s->s_volume_name),
			s->s_volume_name,
			os,
			fs->blocksize, s->s_log_block_size,
			fs->fragsize, s->s_log_frag_size,
			s->s_inodes_count, s->s_blocks_count,
			s->s_r_blocks_count, 100.0 * s->s_r_blocks_count / s->s_blocks_count,
			s->s_first_data_block);
	free(os);
	if (s->s_reserved_gdt_blocks) {
		printf("Maximum filesystem blocks=%lu\n",
		       (s->s_reserved_gdt_blocks + fs->desc_blocks) *
		       (fs->blocksize / sizeof(struct ext2_group_desc)) *
		       s->s_blocks_per_group);
	}
	printf(	"%u block group%s\n"
			"%u blocks per group, %u fragments per group\n"
			"%u inodes per group\n",
			fs->group_desc_count, (fs->group_desc_count > 1) ? "s" : "",
			s->s_blocks_per_group, s->s_frags_per_group,
			s->s_inodes_per_group);
	if (fs->group_desc_count == 1) {
		bb_putchar('\n');
		return;
	}

	printf("Superblock backups stored on blocks: ");
	group_block = s->s_first_data_block;
	col_left = 0;
	for (i = 1; i < fs->group_desc_count; i++) {
		group_block += s->s_blocks_per_group;
		if (!ext2fs_bg_has_super(fs, i))
			continue;
		if (i != 1)
			printf(", ");
		need = int_log10(group_block) + 2;
		if (need > col_left) {
			printf("\n\t");
			col_left = 72;
		}
		col_left -= need;
		printf("%u", group_block);
	}
	puts("\n");
}

/*
 * Set the S_CREATOR_OS field.  Return true if OS is known,
 * otherwise, 0.
 */
static int set_os(struct ext2_super_block *sb, char *os)
{
	if (isdigit (*os)) {
		sb->s_creator_os = atoi(os);
		return 1;
	}

	if ((sb->s_creator_os = e2p_string2os(os)) >= 0) {
		return 1;
	} else if (!strcasecmp("GNU", os)) {
		sb->s_creator_os = EXT2_OS_HURD;
		return 1;
	}
	return 0;
}

static void parse_extended_opts(struct ext2_super_block *sb_param,
				const char *opts)
{
	char	*buf, *token, *next, *p, *arg;
	int	r_usage = 0;

	buf = xstrdup(opts);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		}
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "stride") == 0) {
			if (!arg) {
				r_usage++;
				continue;
			}
			fs_stride = strtoul(arg, &p, 0);
			if (*p || (fs_stride == 0)) {
				bb_error_msg("Invalid stride parameter: %s", arg);
				r_usage++;
				continue;
			}
		} else if (!strcmp(token, "resize")) {
			unsigned long resize, bpg, rsv_groups;
			unsigned long group_desc_count, desc_blocks;
			unsigned int gdpb, blocksize;
			int rsv_gdb;

			if (!arg) {
				r_usage++;
				continue;
			}

			resize = parse_num_blocks(arg,
						  sb_param->s_log_block_size);

			if (resize == 0) {
				bb_error_msg("Invalid resize parameter: %s", arg);
				r_usage++;
				continue;
			}
			if (resize <= sb_param->s_blocks_count) {
				bb_error_msg("The resize maximum must be greater "
						"than the filesystem size");
				r_usage++;
				continue;
			}

			blocksize = EXT2_BLOCK_SIZE(sb_param);
			bpg = sb_param->s_blocks_per_group;
			if (!bpg)
				bpg = blocksize * 8;
			gdpb = blocksize / sizeof(struct ext2_group_desc);
			group_desc_count = (sb_param->s_blocks_count +
					    bpg - 1) / bpg;
			desc_blocks = (group_desc_count +
				       gdpb - 1) / gdpb;
			rsv_groups = (resize + bpg - 1) / bpg;
			rsv_gdb = (rsv_groups + gdpb - 1) / gdpb -
				desc_blocks;
			if (rsv_gdb > EXT2_ADDR_PER_BLOCK(sb_param))
				rsv_gdb = EXT2_ADDR_PER_BLOCK(sb_param);

			if (rsv_gdb > 0) {
				sb_param->s_feature_compat |=
					EXT2_FEATURE_COMPAT_RESIZE_INO;

				sb_param->s_reserved_gdt_blocks = rsv_gdb;
			}
		} else
			r_usage++;
	}
	if (r_usage) {
		bb_error_msg_and_die(
			"\nBad options specified.\n\n"
			"Extended options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid extended options are:\n"
			"\tstride=<stride length in blocks>\n"
			"\tresize=<resize maximum size in blocks>\n");
	}
}

static __u32 ok_features[3] = {
	EXT3_FEATURE_COMPAT_HAS_JOURNAL |
		EXT2_FEATURE_COMPAT_RESIZE_INO |
		EXT2_FEATURE_COMPAT_DIR_INDEX,  /* Compat */
	EXT2_FEATURE_INCOMPAT_FILETYPE|         /* Incompat */
		EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|
		EXT2_FEATURE_INCOMPAT_META_BG,
	EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER     /* R/O compat */
};

static int PRS(int argc, char **argv)
{
	int		c;
	int		size;
	char *		tmp;
	int		blocksize = 0;
	int		inode_ratio = 0;
	int		inode_size = 0;
	int		reserved_ratio = 5;
	int		sector_size = 0;
	int		show_version_only = 0;
	ext2_ino_t	num_inodes = 0;
	errcode_t	retval;
	char *		extended_opts = NULL;
	const char *	fs_type = NULL;
	blk_t		dev_size;
	long		sysval;

	/* Update our PATH to include /sbin  */
	e2fs_set_sbin_path();

	tmp = getenv("MKE2FS_SYNC");
	if (tmp)
		sync_kludge = atoi(tmp);

	/* Determine the system page size if possible */
#if (!defined(_SC_PAGESIZE) && defined(_SC_PAGE_SIZE))
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif
#ifdef _SC_PAGESIZE
	sysval = sysconf(_SC_PAGESIZE);
	if (sysval > 0)
		sys_page_size = sysval;
#endif /* _SC_PAGESIZE */

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	memset(&param, 0, sizeof(struct ext2_super_block));
	param.s_rev_level = 1;  /* Create revision 1 filesystems now */
	param.s_feature_incompat |= EXT2_FEATURE_INCOMPAT_FILETYPE;
	param.s_feature_ro_compat |= EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;

#ifdef __linux__
	linux_version_code = get_linux_version_code();
	if (linux_version_code && linux_version_code < KERNEL_VERSION(2,2,0)) {
		param.s_rev_level = 0;
		param.s_feature_incompat = 0;
		param.s_feature_compat = 0;
		param.s_feature_ro_compat = 0;
	}
#endif

	/* If called as mkfs.ext3, create a journal inode */
	if (last_char_is(applet_name, '3'))
		journal_size = -1;

	while ((c = getopt (argc, argv,
		    "b:cE:f:g:i:jl:m:no:qr:R:s:tvI:J:ST:FL:M:N:O:V")) != EOF) {
		switch (c) {
		case 'b':
			blocksize = xatou_range(optarg, EXT2_MIN_BLOCK_SIZE, EXT2_MAX_BLOCK_SIZE);
			mke2fs_warning_msg((blocksize > 4096),
				"blocksize %d not usable on most systems",
				blocksize);
			param.s_log_block_size =
				int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);
			break;
		case 'c':	/* Check for bad blocks */
		case 't':	/* deprecated */
			cflag++;
			break;
		case 'f':
			size = xatou_range(optarg, EXT2_MIN_BLOCK_SIZE, EXT2_MAX_BLOCK_SIZE);
			param.s_log_frag_size =
				int_log2(size >> EXT2_MIN_BLOCK_LOG_SIZE);
			mke2fs_warning_msg(1, "fragments not supported. Ignoring -f option");
			break;
		case 'g':
			param.s_blocks_per_group = xatou32(optarg);
			if ((param.s_blocks_per_group % 8) != 0) {
				bb_error_msg_and_die("blocks per group must be multiple of 8");
			}
			break;
		case 'i':
			/* Huh? is "* 1024" correct? */
			inode_ratio = xatou_range(optarg, EXT2_MIN_BLOCK_SIZE, EXT2_MAX_BLOCK_SIZE * 1024);
			break;
		case 'J':
			parse_journal_opts(&journal_device, &journal_flags, &journal_size, optarg);
			break;
		case 'j':
			param.s_feature_compat |=
				EXT3_FEATURE_COMPAT_HAS_JOURNAL;
			if (!journal_size)
				journal_size = -1;
			break;
		case 'l':
			bad_blocks_filename = optarg;
			break;
		case 'm':
			reserved_ratio = xatou_range(optarg, 0, 50);
			break;
		case 'n':
			noaction++;
			break;
		case 'o':
			creator_os = optarg;
			break;
		case 'r':
			param.s_rev_level = xatoi_positive(optarg);
			if (param.s_rev_level == EXT2_GOOD_OLD_REV) {
				param.s_feature_incompat = 0;
				param.s_feature_compat = 0;
				param.s_feature_ro_compat = 0;
			}
			break;
		case 's':	/* deprecated */
			if (xatou(optarg))
				param.s_feature_ro_compat |=
					EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			else
				param.s_feature_ro_compat &=
					~EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			break;
#ifdef EXT2_DYNAMIC_REV
		case 'I':
			inode_size = xatoi_positive(optarg);
			break;
#endif
		case 'N':
			num_inodes = xatoi_positive(optarg);
			break;
		case 'v':
			quiet = 0;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'F':
			force = 1;
			break;
		case 'L':
			volume_label = optarg;
			break;
		case 'M':
			mount_dir = optarg;
			break;
		case 'O':
			if (!strcmp(optarg, "none")) {
				param.s_feature_compat = 0;
				param.s_feature_incompat = 0;
				param.s_feature_ro_compat = 0;
				break;
			}
			if (e2p_edit_feature(optarg,
					    &param.s_feature_compat,
					    ok_features)) {
				bb_error_msg_and_die("Invalid filesystem option set: %s", optarg);
			}
			break;
		case 'E':
		case 'R':
			extended_opts = optarg;
			break;
		case 'S':
			super_only = 1;
			break;
		case 'T':
			fs_type = optarg;
			break;
		case 'V':
			/* Print version number and exit */
			show_version_only = 1;
			quiet = 0;
			break;
		default:
			bb_show_usage();
		}
	}
	if ((optind == argc) /*&& !show_version_only*/)
		bb_show_usage();
	device_name = argv[optind++];

	mke2fs_verbose("mke2fs %s (%s)\n", E2FSPROGS_VERSION, E2FSPROGS_DATE);

	if (show_version_only) {
		return 0;
	}

	/*
	 * If there's no blocksize specified and there is a journal
	 * device, use it to figure out the blocksize
	 */
	if (blocksize <= 0 && journal_device) {
		ext2_filsys     jfs;
		io_manager      io_ptr;

#ifdef CONFIG_TESTIO_DEBUG
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
#else
		io_ptr = unix_io_manager;
#endif
		retval = ext2fs_open(journal_device,
				     EXT2_FLAG_JOURNAL_DEV_OK, 0,
				     0, io_ptr, &jfs);
		mke2fs_error_msg_and_die(retval, "open journal device %s", journal_device);
		if ((blocksize < 0) && (jfs->blocksize < (unsigned) (-blocksize))) {
			bb_error_msg_and_die(
				"Journal dev blocksize (%d) smaller than "
				"minimum blocksize %d\n", jfs->blocksize,
				-blocksize);
		}
		blocksize = jfs->blocksize;
		param.s_log_block_size =
			int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);
		ext2fs_close(jfs);
	}

	if (blocksize > sys_page_size) {
		mke2fs_warning_msg(1, "%d-byte blocks too big for system (max %d)",
			blocksize, sys_page_size);
		if (!force) {
			proceed_question();
		}
		bb_error_msg("Forced to continue");
	}
	mke2fs_warning_msg(((blocksize > 4096) &&
		(param.s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL)),
		"some 2.4 kernels do not support "
		"blocksizes greater than 4096 using ext3.\n"
		"Use -b 4096 if this is an issue for you\n");

	if (optind < argc) {
		param.s_blocks_count = parse_num_blocks(argv[optind++],
				param.s_log_block_size);
		mke2fs_error_msg_and_die(!param.s_blocks_count, "invalid blocks count - %s", argv[optind - 1]);
	}
	if (optind < argc)
		bb_show_usage();

	if (param.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		if (!fs_type)
			fs_type = "journal";
		reserved_ratio = 0;
		param.s_feature_incompat = EXT3_FEATURE_INCOMPAT_JOURNAL_DEV;
		param.s_feature_compat = 0;
		param.s_feature_ro_compat = 0;
	}
	if (param.s_rev_level == EXT2_GOOD_OLD_REV &&
	    (param.s_feature_compat || param.s_feature_ro_compat ||
	     param.s_feature_incompat))
		param.s_rev_level = 1;	/* Create a revision 1 filesystem */

	check_plausibility(device_name , force);
	check_mount(device_name, force, "filesystem");

	param.s_log_frag_size = param.s_log_block_size;

	if (noaction && param.s_blocks_count) {
		dev_size = param.s_blocks_count;
		retval = 0;
	} else {
	retry:
		retval = ext2fs_get_device_size(device_name,
						EXT2_BLOCK_SIZE(&param),
						&dev_size);
		if ((retval == EFBIG) &&
		    (blocksize == 0) &&
		    (param.s_log_block_size == 0)) {
			param.s_log_block_size = 2;
			blocksize = 4096;
			goto retry;
		}
	}

	mke2fs_error_msg_and_die((retval && (retval != EXT2_ET_UNIMPLEMENTED)),"determine filesystem size");

	if (!param.s_blocks_count) {
		if (retval == EXT2_ET_UNIMPLEMENTED) {
			mke2fs_error_msg_and_die(1,
				"determine device size; you "
				"must specify\nthe size of the "
				"filesystem");
		} else {
			if (dev_size == 0) {
				bb_error_msg_and_die(
					"Device size reported to be zero.  "
					"Invalid partition specified, or\n\t"
					"partition table wasn't reread "
					"after running fdisk, due to\n\t"
					"a modified partition being busy "
					"and in use.  You may need to reboot\n\t"
					"to re-read your partition table.\n"
				);
			}
			param.s_blocks_count = dev_size;
			if (sys_page_size > EXT2_BLOCK_SIZE(&param))
				param.s_blocks_count &= ~((sys_page_size /
							   EXT2_BLOCK_SIZE(&param))-1);
		}

	} else if (!force && (param.s_blocks_count > dev_size)) {
		bb_error_msg("Filesystem larger than apparent device size");
		proceed_question();
	}

	/*
	 * If the user asked for HAS_JOURNAL, then make sure a journal
	 * gets created.
	 */
	if ((param.s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL) &&
	    !journal_size)
		journal_size = -1;

	/* Set first meta blockgroup via an environment variable */
	/* (this is mostly for debugging purposes) */
	if ((param.s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG) &&
	    ((tmp = getenv("MKE2FS_FIRST_META_BG"))))
		param.s_first_meta_bg = atoi(tmp);

	/* Get the hardware sector size, if available */
	retval = ext2fs_get_device_sectsize(device_name, &sector_size);
	mke2fs_error_msg_and_die(retval, "determine hardware sector size");

	if ((tmp = getenv("MKE2FS_DEVICE_SECTSIZE")) != NULL)
		sector_size = atoi(tmp);

	set_fs_defaults(fs_type, &param, blocksize, sector_size, &inode_ratio);
	blocksize = EXT2_BLOCK_SIZE(&param);

	if (extended_opts)
		parse_extended_opts(&param, extended_opts);

	/* Since sparse_super is the default, we would only have a problem
	 * here if it was explicitly disabled.
	 */
	if ((param.s_feature_compat & EXT2_FEATURE_COMPAT_RESIZE_INO) &&
	    !(param.s_feature_ro_compat&EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		bb_error_msg_and_die("reserved online resize blocks not supported "
			  "on non-sparse filesystem");
	}

	if (param.s_blocks_per_group) {
		if (param.s_blocks_per_group < 256 ||
		    param.s_blocks_per_group > 8 * (unsigned) blocksize) {
			bb_error_msg_and_die("blocks per group count out of range");
		}
	}

	if (!force && param.s_blocks_count >= (1 << 31)) {
		bb_error_msg_and_die("Filesystem too large.  No more than 2**31-1 blocks\n"
			"\t (8TB using a blocksize of 4k) are currently supported.");
	}

	if (inode_size) {
		if (inode_size < EXT2_GOOD_OLD_INODE_SIZE ||
		    inode_size > EXT2_BLOCK_SIZE(&param) ||
		    inode_size & (inode_size - 1)) {
			bb_error_msg_and_die("invalid inode size %d (min %d/max %d)",
				inode_size, EXT2_GOOD_OLD_INODE_SIZE,
				blocksize);
		}
		mke2fs_warning_msg((inode_size != EXT2_GOOD_OLD_INODE_SIZE),
			"%d-byte inodes not usable on most systems",
			inode_size);
		param.s_inode_size = inode_size;
	}

	/*
	 * Calculate number of inodes based on the inode ratio
	 */
	param.s_inodes_count = num_inodes ? num_inodes :
		((__u64) param.s_blocks_count * blocksize)
			/ inode_ratio;

	/*
	 * Calculate number of blocks to reserve
	 */
	param.s_r_blocks_count = (param.s_blocks_count * reserved_ratio) / 100;
	return 1;
}

static void mke2fs_clean_up(void)
{
	if (ENABLE_FEATURE_CLEAN_UP && journal_device) free(journal_device);
}

int mke2fs_main (int argc, char **argv);
int mke2fs_main (int argc, char **argv)
{
	errcode_t	retval;
	ext2_filsys	fs;
	badblocks_list	bb_list = 0;
	unsigned int	i;
	int		val;
	io_manager	io_ptr;

	if (ENABLE_FEATURE_CLEAN_UP)
		atexit(mke2fs_clean_up);
	if (!PRS(argc, argv))
		return 0;

#ifdef CONFIG_TESTIO_DEBUG
	io_ptr = test_io_manager;
	test_io_backing_manager = unix_io_manager;
#else
	io_ptr = unix_io_manager;
#endif

	/*
	 * Initialize the superblock....
	 */
	retval = ext2fs_initialize(device_name, 0, &param,
				   io_ptr, &fs);
	mke2fs_error_msg_and_die(retval, "set up superblock");

	/*
	 * Wipe out the old on-disk superblock
	 */
	if (!noaction)
		zap_sector(fs, 2, 6);

	/*
	 * Generate a UUID for it...
	 */
	uuid_generate(fs->super->s_uuid);

	/*
	 * Initialize the directory index variables
	 */
	fs->super->s_def_hash_version = EXT2_HASH_TEA;
	uuid_generate((unsigned char *) fs->super->s_hash_seed);

	/*
	 * Add "jitter" to the superblock's check interval so that we
	 * don't check all the filesystems at the same time.  We use a
	 * kludgy hack of using the UUID to derive a random jitter value.
	 */
	for (i = 0, val = 0; i < sizeof(fs->super->s_uuid); i++)
		val += fs->super->s_uuid[i];
	fs->super->s_max_mnt_count += val % EXT2_DFL_MAX_MNT_COUNT;

	/*
	 * Override the creator OS, if applicable
	 */
	if (creator_os && !set_os(fs->super, creator_os)) {
		bb_error_msg_and_die("unknown os - %s", creator_os);
	}

	/*
	 * For the Hurd, we will turn off filetype since it doesn't
	 * support it.
	 */
	if (fs->super->s_creator_os == EXT2_OS_HURD)
		fs->super->s_feature_incompat &=
			~EXT2_FEATURE_INCOMPAT_FILETYPE;

	/*
	 * Set the volume label...
	 */
	if (volume_label) {
		snprintf(fs->super->s_volume_name, sizeof(fs->super->s_volume_name), "%s", volume_label);
	}

	/*
	 * Set the last mount directory
	 */
	if (mount_dir) {
		snprintf(fs->super->s_last_mounted, sizeof(fs->super->s_last_mounted), "%s", mount_dir);
	}

	if (!quiet || noaction)
		show_stats(fs);

	if (noaction)
		return 0;

	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		create_journal_dev(fs);
		return (ext2fs_close(fs) ? 1 : 0);
	}

	if (bad_blocks_filename)
		read_bb_file(fs, &bb_list, bad_blocks_filename);
	if (cflag)
		test_disk(fs, &bb_list);

	handle_bad_blocks(fs, bb_list);
	fs->stride = fs_stride;
	retval = ext2fs_allocate_tables(fs);
	mke2fs_error_msg_and_die(retval, "allocate filesystem tables");
	if (super_only) {
		fs->super->s_state |= EXT2_ERROR_FS;
		fs->flags &= ~(EXT2_FLAG_IB_DIRTY|EXT2_FLAG_BB_DIRTY);
	} else {
		/* rsv must be a power of two (64kB is MD RAID sb alignment) */
		unsigned int rsv = 65536 / fs->blocksize;
		unsigned long blocks = fs->super->s_blocks_count;
		unsigned long start;
		blk_t ret_blk;

#ifdef ZAP_BOOTBLOCK
		zap_sector(fs, 0, 2);
#endif

		/*
		 * Wipe out any old MD RAID (or other) metadata at the end
		 * of the device.  This will also verify that the device is
		 * as large as we think.  Be careful with very small devices.
		 */
		start = (blocks & ~(rsv - 1));
		if (start > rsv)
			start -= rsv;
		if (start > 0)
			retval = zero_blocks(fs, start, blocks - start,
					     NULL, &ret_blk, NULL);

		mke2fs_warning_msg(retval, "can't zero block %u at end of filesystem", ret_blk);
		write_inode_tables(fs);
		create_root_dir(fs);
		create_lost_and_found(fs);
		reserve_inodes(fs);
		create_bad_block_inode(fs, bb_list);
		if (fs->super->s_feature_compat &
		    EXT2_FEATURE_COMPAT_RESIZE_INO) {
			retval = ext2fs_create_resize_inode(fs);
			mke2fs_error_msg_and_die(retval, "reserve blocks for online resize");
		}
	}

	if (journal_device) {
		make_journal_device(journal_device, fs, quiet, force);
	} else if (journal_size) {
		make_journal_blocks(fs, journal_size, journal_flags, quiet);
	}

	mke2fs_verbose("Writing superblocks and filesystem accounting information: ");
	retval = ext2fs_flush(fs);
	mke2fs_warning_msg(retval, "had trouble writing out superblocks");
	mke2fs_verbose_done();
	if (!quiet && !getenv("MKE2FS_SKIP_CHECK_MSG"))
		print_check_message(fs);
	val = ext2fs_close(fs);
	return (retval || val) ? 1 : 0;
}
