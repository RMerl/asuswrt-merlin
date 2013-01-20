/*
 * mke2fs.c - Make a ext2fs filesystem.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
 * 	2003, 2004, 2005 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* Usage: mke2fs [options] device
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-).
 */

#define _XOPEN_SOURCE 600 /* for inclusion of PATH_MAX in Solaris */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>
#include <blkid/blkid.h>

#include "ext2fs/ext2_fs.h"
#include "et/com_err.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"
#include "ext2fs/ext2fs.h"
#include "util.h"
#include "profile.h"
#include "prof_err.h"
#include "../version.h"
#include "nls-enable.h"

#define STRIDE_LENGTH 8

#ifndef __sparc__
#define ZAP_BOOTBLOCK
#endif

extern int isatty(int);
extern FILE *fpopen(const char *cmd, const char *mode);

const char * program_name = "mke2fs";
const char * device_name /* = NULL */;

/* Command line options */
int	cflag;
int	verbose;
int	quiet;
int	super_only;
int	discard = 1;	/* attempt to discard device before fs creation */
int	force;
int	noaction;
int	journal_size;
int	journal_flags;
int	lazy_itable_init;
char	*bad_blocks_filename;
__u32	fs_stride;

struct ext2_super_block fs_param;
char *fs_uuid = NULL;
char *creator_os;
char *volume_label;
char *mount_dir;
char *journal_device;
int sync_kludge;	/* Set using the MKE2FS_SYNC env. option */
char **fs_types;

profile_t	profile;

int sys_page_size = 4096;
int linux_version_code = 0;

static void usage(void)
{
	fprintf(stderr, _("Usage: %s [-c|-l filename] [-b block-size] "
	"[-f fragment-size]\n\t[-i bytes-per-inode] [-I inode-size] "
	"[-J journal-options]\n"
	"\t[-G meta group size] [-N number-of-inodes]\n"
	"\t[-m reserved-blocks-percentage] [-o creator-os]\n"
	"\t[-g blocks-per-group] [-L volume-label] "
	"[-M last-mounted-directory]\n\t[-O feature[,...]] "
	"[-r fs-revision] [-E extended-option[,...]]\n"
	"\t[-T fs-type] [-U UUID] [-jnqvFKSV] device [blocks-count]\n"),
		program_name);
	exit(1);
}

static int int_log2(int arg)
{
	int	l = 0;

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

	for (l=0; arg ; l++)
		arg = arg / 10;
	return l;
}

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

/*
 * Helper function for read_bb_file and test_disk
 */
static void invalid_block(ext2_filsys fs EXT2FS_ATTR((unused)), blk_t blk)
{
	fprintf(stderr, _("Bad block %u out of range; ignored.\n"), blk);
	return;
}

/*
 * Reads the bad blocks list from a file
 */
static void read_bb_file(ext2_filsys fs, badblocks_list *bb_list,
			 const char *bad_blocks_file)
{
	FILE		*f;
	errcode_t	retval;

	f = fopen(bad_blocks_file, "r");
	if (!f) {
		com_err("read_bad_blocks_file", errno,
			_("while trying to open %s"), bad_blocks_file);
		exit(1);
	}
	retval = ext2fs_read_bb_FILE(fs, f, bb_list, invalid_block);
	fclose (f);
	if (retval) {
		com_err("ext2fs_read_bb_FILE", retval,
			_("while reading in list of bad blocks from file"));
		exit(1);
	}
}

/*
 * Runs the badblocks program to test the disk
 */
static void test_disk(ext2_filsys fs, badblocks_list *bb_list)
{
	FILE		*f;
	errcode_t	retval;
	char		buf[1024];

	sprintf(buf, "badblocks -b %d -X %s%s%s %u", fs->blocksize,
		quiet ? "" : "-s ", (cflag > 1) ? "-w " : "",
		fs->device_name, fs->super->s_blocks_count-1);
	if (verbose)
		printf(_("Running command: %s\n"), buf);
	f = popen(buf, "r");
	if (!f) {
		com_err("popen", errno,
			_("while trying to run '%s'"), buf);
		exit(1);
	}
	retval = ext2fs_read_bb_FILE(fs, f, bb_list, invalid_block);
	pclose(f);
	if (retval) {
		com_err("ext2fs_read_bb_FILE", retval,
			_("while processing list of bad blocks from program"));
		exit(1);
	}
}

static void handle_bad_blocks(ext2_filsys fs, badblocks_list bb_list)
{
	dgrp_t			i;
	blk_t			j;
	unsigned 		must_be_good;
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
			fprintf(stderr, _("Block %d in primary "
				"superblock/group descriptor area bad.\n"), i);
			fprintf(stderr, _("Blocks %u through %u must be good "
				"in order to build a filesystem.\n"),
				fs->super->s_first_data_block, must_be_good);
			fputs(_("Aborting....\n"), stderr);
			exit(1);
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
				if (!group_bad)
					fprintf(stderr,
_("Warning: the backup superblock/group descriptors at block %u contain\n"
"	bad blocks.\n\n"),
						group_block);
				group_bad++;
				group = ext2fs_group_of_blk(fs, group_block+j);
				fs->group_desc[group].bg_free_blocks_count++;
				ext2fs_group_desc_csum_set(fs, group);
				fs->super->s_free_blocks_count++;
			}
		}
		group_block += fs->super->s_blocks_per_group;
	}

	/*
	 * Mark all the bad blocks as used...
	 */
	retval = ext2fs_badblocks_list_iterate_begin(bb_list, &bb_iter);
	if (retval) {
		com_err("ext2fs_badblocks_list_iterate_begin", retval,
			_("while marking bad blocks as used"));
		exit(1);
	}
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
	fputs(_("done                            \n"), stdout);
}

static void write_inode_tables(ext2_filsys fs, int lazy_flag, int itable_zeroed)
{
	errcode_t	retval;
	blk_t		blk;
	dgrp_t		i;
	int		num, ipb;
	struct progress_struct progress;

	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, _("Writing inode tables: "),
			      fs->group_desc_count);

	for (i = 0; i < fs->group_desc_count; i++) {
		progress_update(&progress, i);

		blk = fs->group_desc[i].bg_inode_table;
		num = fs->inode_blocks_per_group;

		if (lazy_flag) {
			ipb = fs->blocksize / EXT2_INODE_SIZE(fs->super);
			num = ((((fs->super->s_inodes_per_group -
				  fs->group_desc[i].bg_itable_unused) *
				 EXT2_INODE_SIZE(fs->super)) +
				EXT2_BLOCK_SIZE(fs->super) - 1) /
			       EXT2_BLOCK_SIZE(fs->super));
		}
		if (!lazy_flag || itable_zeroed) {
			/* The kernel doesn't need to zero the itable blocks */
			fs->group_desc[i].bg_flags |= EXT2_BG_INODE_ZEROED;
			ext2fs_group_desc_csum_set(fs, i);
		}
		retval = ext2fs_zero_blocks(fs, blk, num, &blk, &num);
		if (retval) {
			fprintf(stderr, _("\nCould not write %d "
				  "blocks in inode table starting at %u: %s\n"),
				num, blk, error_message(retval));
			exit(1);
		}
		if (sync_kludge) {
			if (sync_kludge == 1)
				sync();
			else if ((i % sync_kludge) == 0)
				sync();
		}
	}
	ext2fs_zero_blocks(0, 0, 0, 0, 0);
	progress_close(&progress);
}

static void create_root_dir(ext2_filsys fs)
{
	errcode_t		retval;
	struct ext2_inode	inode;
	__u32			uid, gid;

	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, 0);
	if (retval) {
		com_err("ext2fs_mkdir", retval, _("while creating root dir"));
		exit(1);
	}
	if (geteuid()) {
		retval = ext2fs_read_inode(fs, EXT2_ROOT_INO, &inode);
		if (retval) {
			com_err("ext2fs_read_inode", retval,
				_("while reading root inode"));
			exit(1);
		}
		uid = getuid();
		inode.i_uid = uid;
		ext2fs_set_i_uid_high(inode, uid >> 16);
		if (uid) {
			gid = getgid();
			inode.i_gid = gid;
			ext2fs_set_i_gid_high(inode, gid >> 16);
		}
		retval = ext2fs_write_new_inode(fs, EXT2_ROOT_INO, &inode);
		if (retval) {
			com_err("ext2fs_write_inode", retval,
				_("while setting root inode ownership"));
			exit(1);
		}
	}
}

static void create_lost_and_found(ext2_filsys fs)
{
	unsigned int		lpf_size = 0;
	errcode_t		retval;
	ext2_ino_t		ino;
	const char		*name = "lost+found";
	int			i;

	fs->umask = 077;
	retval = ext2fs_mkdir(fs, EXT2_ROOT_INO, 0, name);
	if (retval) {
		com_err("ext2fs_mkdir", retval,
			_("while creating /lost+found"));
		exit(1);
	}

	retval = ext2fs_lookup(fs, EXT2_ROOT_INO, name, strlen(name), 0, &ino);
	if (retval) {
		com_err("ext2_lookup", retval,
			_("while looking up /lost+found"));
		exit(1);
	}

	for (i=1; i < EXT2_NDIR_BLOCKS; i++) {
		/* Ensure that lost+found is at least 2 blocks, so we always
		 * test large empty blocks for big-block filesystems.  */
		if ((lpf_size += fs->blocksize) >= 16*1024 &&
		    lpf_size >= 2 * fs->blocksize)
			break;
		retval = ext2fs_expand_dir(fs, ino);
		if (retval) {
			com_err("ext2fs_expand_dir", retval,
				_("while expanding /lost+found"));
			exit(1);
		}
	}
}

static void create_bad_block_inode(ext2_filsys fs, badblocks_list bb_list)
{
	errcode_t	retval;

	ext2fs_mark_inode_bitmap(fs->inode_map, EXT2_BAD_INO);
	ext2fs_inode_alloc_stats2(fs, EXT2_BAD_INO, +1, 0);
	retval = ext2fs_update_bb_inode(fs, bb_list);
	if (retval) {
		com_err("ext2fs_update_bb_inode", retval,
			_("while setting bad block inode"));
		exit(1);
	}

}

static void reserve_inodes(ext2_filsys fs)
{
	ext2_ino_t	i;

	for (i = EXT2_ROOT_INO + 1; i < EXT2_FIRST_INODE(fs->super); i++)
		ext2fs_inode_alloc_stats2(fs, i, +1, 0);
	ext2fs_mark_ib_dirty(fs);
}

#define BSD_DISKMAGIC   (0x82564557UL)  /* The disk magic number */
#define BSD_MAGICDISK   (0x57455682UL)  /* The disk magic number reversed */
#define BSD_LABEL_OFFSET        64

static void zap_sector(ext2_filsys fs, int sect, int nsect)
{
	char *buf;
	int retval;
	unsigned int *magic;

	buf = malloc(512*nsect);
	if (!buf) {
		printf(_("Out of memory erasing sectors %d-%d\n"),
		       sect, sect + nsect - 1);
		exit(1);
	}

	if (sect == 0) {
		/* Check for a BSD disklabel, and don't erase it if so */
		retval = io_channel_read_blk(fs->io, 0, -512, buf);
		if (retval)
			fprintf(stderr,
				_("Warning: could not read block 0: %s\n"),
				error_message(retval));
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
	if (retval)
		fprintf(stderr, _("Warning: could not erase sector %d: %s\n"),
			sect, error_message(retval));
}

static void create_journal_dev(ext2_filsys fs)
{
	struct progress_struct progress;
	errcode_t		retval;
	char			*buf;
	blk_t			blk, err_blk;
	int			c, count, err_count;

	retval = ext2fs_create_journal_superblock(fs,
				  fs->super->s_blocks_count, 0, &buf);
	if (retval) {
		com_err("create_journal_dev", retval,
			_("while initializing journal superblock"));
		exit(1);
	}
	if (quiet)
		memset(&progress, 0, sizeof(progress));
	else
		progress_init(&progress, _("Zeroing journal device: "),
			      fs->super->s_blocks_count);

	blk = 0;
	count = fs->super->s_blocks_count;
	while (count > 0) {
		if (count > 1024)
			c = 1024;
		else
			c = count;
		retval = ext2fs_zero_blocks(fs, blk, c, &err_blk, &err_count);
		if (retval) {
			com_err("create_journal_dev", retval,
				_("while zeroing journal device "
				  "(block %u, count %d)"),
				err_blk, err_count);
			exit(1);
		}
		blk += c;
		count -= c;
		progress_update(&progress, blk);
	}
	ext2fs_zero_blocks(0, 0, 0, 0, 0);

	retval = io_channel_write_blk(fs->io,
				      fs->super->s_first_data_block+1,
				      1, buf);
	if (retval) {
		com_err("create_journal_dev", retval,
			_("while writing journal superblock"));
		exit(1);
	}
	progress_close(&progress);
}

static void show_stats(ext2_filsys fs)
{
	struct ext2_super_block *s = fs->super;
	char 			buf[80];
        char                    *os;
	blk_t			group_block;
	dgrp_t			i;
	int			need, col_left;

	if (fs_param.s_blocks_count != s->s_blocks_count)
		fprintf(stderr, _("warning: %u blocks unused.\n\n"),
		       fs_param.s_blocks_count - s->s_blocks_count);

	memset(buf, 0, sizeof(buf));
	strncpy(buf, s->s_volume_name, sizeof(s->s_volume_name));
	printf(_("Filesystem label=%s\n"), buf);
	fputs(_("OS type: "), stdout);
        os = e2p_os2string(fs->super->s_creator_os);
	fputs(os, stdout);
	free(os);
	printf("\n");
	printf(_("Block size=%u (log=%u)\n"), fs->blocksize,
		s->s_log_block_size);
	printf(_("Fragment size=%u (log=%u)\n"), fs->fragsize,
		s->s_log_frag_size);
	printf(_("Stride=%u blocks, Stripe width=%u blocks\n"),
	       s->s_raid_stride, s->s_raid_stripe_width);
	printf(_("%u inodes, %u blocks\n"), s->s_inodes_count,
	       s->s_blocks_count);
	printf(_("%u blocks (%2.2f%%) reserved for the super user\n"),
		s->s_r_blocks_count,
	       100.0 * s->s_r_blocks_count / s->s_blocks_count);
	printf(_("First data block=%u\n"), s->s_first_data_block);
	if (s->s_reserved_gdt_blocks)
		printf(_("Maximum filesystem blocks=%lu\n"),
		       (s->s_reserved_gdt_blocks + fs->desc_blocks) *
		       EXT2_DESC_PER_BLOCK(s) * s->s_blocks_per_group);
	if (fs->group_desc_count > 1)
		printf(_("%u block groups\n"), fs->group_desc_count);
	else
		printf(_("%u block group\n"), fs->group_desc_count);
	printf(_("%u blocks per group, %u fragments per group\n"),
	       s->s_blocks_per_group, s->s_frags_per_group);
	printf(_("%u inodes per group\n"), s->s_inodes_per_group);

	if (fs->group_desc_count == 1) {
		printf("\n");
		return;
	}

	printf(_("Superblock backups stored on blocks: "));
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
	printf("\n\n");
}

/*
 * Set the S_CREATOR_OS field.  Return true if OS is known,
 * otherwise, 0.
 */
static int set_os(struct ext2_super_block *sb, char *os)
{
	if (isdigit (*os))
		sb->s_creator_os = atoi (os);
	else if (strcasecmp(os, "linux") == 0)
		sb->s_creator_os = EXT2_OS_LINUX;
	else if (strcasecmp(os, "GNU") == 0 || strcasecmp(os, "hurd") == 0)
		sb->s_creator_os = EXT2_OS_HURD;
	else if (strcasecmp(os, "freebsd") == 0)
		sb->s_creator_os = EXT2_OS_FREEBSD;
	else if (strcasecmp(os, "lites") == 0)
		sb->s_creator_os = EXT2_OS_LITES;
	else
		return 0;
	return 1;
}

#define PATH_SET "PATH=/sbin"

static void parse_extended_opts(struct ext2_super_block *param,
				const char *opts)
{
	char	*buf, *token, *next, *p, *arg, *badopt = 0;
	int	len;
	int	r_usage = 0;

	len = strlen(opts);
	buf = malloc(len+1);
	if (!buf) {
		fprintf(stderr,
			_("Couldn't allocate memory to parse options!\n"));
		exit(1);
	}
	strcpy(buf, opts);
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
				badopt = token;
				continue;
			}
			param->s_raid_stride = strtoul(arg, &p, 0);
			if (*p || (param->s_raid_stride == 0)) {
				fprintf(stderr,
					_("Invalid stride parameter: %s\n"),
					arg);
				r_usage++;
				continue;
			}
		} else if (strcmp(token, "stripe-width") == 0 ||
			   strcmp(token, "stripe_width") == 0) {
			if (!arg) {
				r_usage++;
				badopt = token;
				continue;
			}
			param->s_raid_stripe_width = strtoul(arg, &p, 0);
			if (*p || (param->s_raid_stripe_width == 0)) {
				fprintf(stderr,
					_("Invalid stripe-width parameter: %s\n"),
					arg);
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
				badopt = token;
				continue;
			}

			resize = parse_num_blocks(arg,
						  param->s_log_block_size);

			if (resize == 0) {
				fprintf(stderr,
					_("Invalid resize parameter: %s\n"),
					arg);
				r_usage++;
				continue;
			}
			if (resize <= param->s_blocks_count) {
				fprintf(stderr,
					_("The resize maximum must be greater "
					  "than the filesystem size.\n"));
				r_usage++;
				continue;
			}

			blocksize = EXT2_BLOCK_SIZE(param);
			bpg = param->s_blocks_per_group;
			if (!bpg)
				bpg = blocksize * 8;
			gdpb = EXT2_DESC_PER_BLOCK(param);
			group_desc_count =
				ext2fs_div_ceil(param->s_blocks_count, bpg);
			desc_blocks = (group_desc_count +
				       gdpb - 1) / gdpb;
			rsv_groups = ext2fs_div_ceil(resize, bpg);
			rsv_gdb = ext2fs_div_ceil(rsv_groups, gdpb) -
				desc_blocks;
			if (rsv_gdb > (int) EXT2_ADDR_PER_BLOCK(param))
				rsv_gdb = EXT2_ADDR_PER_BLOCK(param);

			if (rsv_gdb > 0) {
				if (param->s_rev_level == EXT2_GOOD_OLD_REV) {
					fprintf(stderr,
	_("On-line resizing not supported with revision 0 filesystems\n"));
					free(buf);
					exit(1);
				}
				param->s_feature_compat |=
					EXT2_FEATURE_COMPAT_RESIZE_INODE;

				param->s_reserved_gdt_blocks = rsv_gdb;
			}
		} else if (!strcmp(token, "test_fs")) {
			param->s_flags |= EXT2_FLAGS_TEST_FILESYS;
		} else if (!strcmp(token, "lazy_itable_init")) {
			if (arg)
				lazy_itable_init = strtoul(arg, &p, 0);
			else
				lazy_itable_init = 1;
		} else if (!strcmp(token, "discard")) {
			discard = 1;
		} else if (!strcmp(token, "nodiscard")) {
			discard = 0;
		} else {
			r_usage++;
			badopt = token;
		}
	}
	if (r_usage) {
		fprintf(stderr, _("\nBad option(s) specified: %s\n\n"
			"Extended options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid extended options are:\n"
			"\tstride=<RAID per-disk data chunk in blocks>\n"
			"\tstripe-width=<RAID stride * data disks in blocks>\n"
			"\tresize=<resize maximum size in blocks>\n"
			"\tlazy_itable_init=<0 to disable, 1 to enable>\n"
			"\ttest_fs\n"
			"\tdiscard\n"
			"\tnodiscard\n\n"),
			badopt ? badopt : "");
		free(buf);
		exit(1);
	}
	if (param->s_raid_stride &&
	    (param->s_raid_stripe_width % param->s_raid_stride) != 0)
		fprintf(stderr, _("\nWarning: RAID stripe-width %u not an even "
				  "multiple of stride %u.\n\n"),
			param->s_raid_stripe_width, param->s_raid_stride);

	free(buf);
}

static __u32 ok_features[3] = {
	/* Compat */
	EXT3_FEATURE_COMPAT_HAS_JOURNAL |
		EXT2_FEATURE_COMPAT_RESIZE_INODE |
		EXT2_FEATURE_COMPAT_DIR_INDEX |
		EXT2_FEATURE_COMPAT_EXT_ATTR,
	/* Incompat */
	EXT2_FEATURE_INCOMPAT_FILETYPE|
		EXT3_FEATURE_INCOMPAT_EXTENTS|
		EXT3_FEATURE_INCOMPAT_JOURNAL_DEV|
		EXT2_FEATURE_INCOMPAT_META_BG|
		EXT4_FEATURE_INCOMPAT_FLEX_BG,
	/* R/O compat */
	EXT2_FEATURE_RO_COMPAT_LARGE_FILE|
		EXT4_FEATURE_RO_COMPAT_HUGE_FILE|
		EXT4_FEATURE_RO_COMPAT_DIR_NLINK|
		EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE|
		EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER|
		EXT4_FEATURE_RO_COMPAT_GDT_CSUM
};


static void syntax_err_report(const char *filename, long err, int line_num)
{
	fprintf(stderr,
		_("Syntax error in mke2fs config file (%s, line #%d)\n\t%s\n"),
		filename, line_num, error_message(err));
	exit(1);
}

static const char *config_fn[] = { ROOT_SYSCONFDIR "/mke2fs.conf", 0 };

static void edit_feature(const char *str, __u32 *compat_array)
{
	if (!str)
		return;

	if (e2p_edit_feature(str, compat_array, ok_features)) {
		fprintf(stderr, _("Invalid filesystem option set: %s\n"),
			str);
		exit(1);
	}
}

struct str_list {
	char **list;
	int num;
	int max;
};

static errcode_t init_list(struct str_list *sl)
{
	sl->num = 0;
	sl->max = 0;
	sl->list = malloc((sl->max+1) * sizeof(char *));
	if (!sl->list)
		return ENOMEM;
	sl->list[0] = 0;
	return 0;
}

static errcode_t push_string(struct str_list *sl, const char *str)
{
	char **new_list;

	if (sl->num >= sl->max) {
		sl->max += 2;
		new_list = realloc(sl->list, (sl->max+1) * sizeof(char *));
		if (!new_list)
			return ENOMEM;
		sl->list = new_list;
	}
	sl->list[sl->num] = malloc(strlen(str)+1);
	if (sl->list[sl->num] == 0)
		return ENOMEM;
	strcpy(sl->list[sl->num], str);
	sl->num++;
	sl->list[sl->num] = 0;
	return 0;
}

static void print_str_list(char **list)
{
	char **cpp;

	for (cpp = list; *cpp; cpp++) {
		printf("'%s'", *cpp);
		if (cpp[1])
			fputs(", ", stdout);
	}
	fputc('\n', stdout);
}

/*
 * Return TRUE if the profile has the given subsection
 */
static int profile_has_subsection(profile_t profile, const char *section,
				  const char *subsection)
{
	void			*state;
	const char		*names[4];
	char			*name;
	int			ret = 0;

	names[0] = section;
	names[1] = subsection;
	names[2] = 0;

	if (profile_iterator_create(profile, names,
				    PROFILE_ITER_LIST_SECTION |
				    PROFILE_ITER_RELATIONS_ONLY, &state))
		return 0;

	if ((profile_iterator(&state, &name, 0) == 0) && name) {
		free(name);
		ret = 1;
	}

	profile_iterator_free(&state);
	return ret;
}

static char **parse_fs_type(const char *fs_type,
			    const char *usage_types,
			    struct ext2_super_block *fs_param,
			    char *progname)
{
	const char	*ext_type = 0;
	char		*parse_str;
	char		*profile_type = 0;
	char		*cp, *t;
	const char	*size_type;
	struct str_list	list;
	unsigned long	meg;
	int		is_hurd = 0;

	if (init_list(&list))
		return 0;

	if (creator_os && (!strcasecmp(creator_os, "GNU") ||
			   !strcasecmp(creator_os, "hurd")))
		is_hurd = 1;

	if (fs_type)
		ext_type = fs_type;
	else if (is_hurd)
		ext_type = "ext2";
	else if (!strcmp(program_name, "mke3fs"))
		ext_type = "ext3";
	else if (progname) {
		ext_type = strrchr(progname, '/');
		if (ext_type)
			ext_type++;
		else
			ext_type = progname;

		if (!strncmp(ext_type, "mkfs.", 5)) {
			ext_type += 5;
			if (ext_type[0] == 0)
				ext_type = 0;
		} else
			ext_type = 0;
	}

	if (!ext_type) {
		profile_get_string(profile, "defaults", "fs_type", 0,
				   "ext2", &profile_type);
		ext_type = profile_type;
		if (!strcmp(ext_type, "ext2") && (journal_size != 0))
			ext_type = "ext3";
	}


	if (!profile_has_subsection(profile, "fs_types", ext_type) &&
	    strcmp(ext_type, "ext2")) {
		printf(_("\nYour mke2fs.conf file does not define the "
			 "%s filesystem type.\n"), ext_type);
		if (!strcmp(ext_type, "ext3") || !strcmp(ext_type, "ext4") ||
		    !strcmp(ext_type, "ext4dev")) {
			printf(_("You probably need to install an updated "
				 "mke2fs.conf file.\n\n"));
		}
		if (!force) {
			printf(_("Aborting...\n"));
			exit(1);
		}
	}

	meg = (1024 * 1024) / EXT2_BLOCK_SIZE(fs_param);
	if (fs_param->s_blocks_count < 3 * meg)
		size_type = "floppy";
	else if (fs_param->s_blocks_count < 512 * meg)
		size_type = "small";
	else
		size_type = "default";

	if (!usage_types)
		usage_types = size_type;

	parse_str = malloc(usage_types ? strlen(usage_types)+1 : 1);
	if (!parse_str) {
		free(list.list);
		return 0;
	}
	if (usage_types)
		strcpy(parse_str, usage_types);
	else
		*parse_str = '\0';

	if (ext_type)
		push_string(&list, ext_type);
	cp = parse_str;
	while (1) {
		t = strchr(cp, ',');
		if (t)
			*t = '\0';

		if (*cp) {
			if (profile_has_subsection(profile, "fs_types", cp))
				push_string(&list, cp);
			else if (strcmp(cp, "default") != 0)
				fprintf(stderr,
					_("\nWarning: the fs_type %s is not "
					  "defined in mke2fs.conf\n\n"),
					cp);
		}
		if (t)
			cp = t+1;
		else {
			cp = "";
			break;
		}
	}
	free(parse_str);
	free(profile_type);
	if (is_hurd)
		push_string(&list, "hurd");
	return (list.list);
}

static char *get_string_from_profile(char **fs_types, const char *opt,
				     const char *def_val)
{
	char *ret = 0;
	int i;

	for (i=0; fs_types[i]; i++);
	for (i-=1; i >=0 ; i--) {
		profile_get_string(profile, "fs_types", fs_types[i],
				   opt, 0, &ret);
		if (ret)
			return ret;
	}
	profile_get_string(profile, "defaults", opt, 0, def_val, &ret);
	return (ret);
}

static int get_int_from_profile(char **fs_types, const char *opt, int def_val)
{
	int ret;
	char **cpp;

	profile_get_integer(profile, "defaults", opt, 0, def_val, &ret);
	for (cpp = fs_types; *cpp; cpp++)
		profile_get_integer(profile, "fs_types", *cpp, opt, ret, &ret);
	return ret;
}

static int get_bool_from_profile(char **fs_types, const char *opt, int def_val)
{
	int ret;
	char **cpp;

	profile_get_boolean(profile, "defaults", opt, 0, def_val, &ret);
	for (cpp = fs_types; *cpp; cpp++)
		profile_get_boolean(profile, "fs_types", *cpp, opt, ret, &ret);
	return ret;
}

extern const char *mke2fs_default_profile;
static const char *default_files[] = { "<default>", 0 };

#ifdef HAVE_BLKID_PROBE_GET_TOPOLOGY
/*
 * Sets the geometry of a device (stripe/stride), and returns the
 * device's alignment offset, if any, or a negative error.
 */
static int get_device_geometry(const char *file,
			       struct ext2_super_block *fs_param,
			       int psector_size)
{
	int rc = -1;
	int blocksize;
	blkid_probe pr;
	blkid_topology tp;
	unsigned long min_io;
	unsigned long opt_io;
	struct stat statbuf;

	/* Nothing to do for a regular file */
	if (!stat(file, &statbuf) && S_ISREG(statbuf.st_mode))
		return 0;

	pr = blkid_new_probe_from_filename(file);
	if (!pr)
		goto out;

	tp = blkid_probe_get_topology(pr);
	if (!tp)
		goto out;

	min_io = blkid_topology_get_minimum_io_size(tp);
	opt_io = blkid_topology_get_optimal_io_size(tp);
	blocksize = EXT2_BLOCK_SIZE(fs_param);
	if ((min_io == 0) && (psector_size > blocksize))
		min_io = psector_size;
	if ((opt_io == 0) && min_io)
		opt_io = min_io;
	if ((opt_io == 0) && (psector_size > blocksize))
		opt_io = psector_size;

	fs_param->s_raid_stride = min_io / blocksize;
	fs_param->s_raid_stripe_width = opt_io / blocksize;

	rc = blkid_topology_get_alignment_offset(tp);
out:
	blkid_free_probe(pr);
	return rc;
}
#endif

static void PRS(int argc, char *argv[])
{
	int		b, c;
	int		size;
	char 		*tmp, **cpp;
	int		blocksize = 0;
	int		inode_ratio = 0;
	int		inode_size = 0;
	unsigned long	flex_bg_size = 0;
	double		reserved_ratio = 5.0;
	int		lsector_size = 0, psector_size = 0;
	int		show_version_only = 0;
	unsigned long long num_inodes = 0; /* unsigned long long to catch too-large input */
	errcode_t	retval;
	char *		oldpath = getenv("PATH");
	char *		extended_opts = 0;
	const char *	fs_type = 0;
	const char *	usage_types = 0;
	blk_t		dev_size;
#ifdef __linux__
	struct 		utsname ut;
#endif
	long		sysval;
	int		s_opt = -1, r_opt = -1;
	char		*fs_features = 0;
	int		use_bsize;
	char		*newpath;
	int		pathlen = sizeof(PATH_SET) + 1;

	if (oldpath)
		pathlen += strlen(oldpath);
	newpath = malloc(pathlen);
	strcpy(newpath, PATH_SET);

	/* Update our PATH to include /sbin  */
	if (oldpath) {
		strcat (newpath, ":");
		strcat (newpath, oldpath);
	}
	putenv (newpath);

	tmp = getenv("MKE2FS_SYNC");
	if (tmp)
		sync_kludge = atoi(tmp);

	/* Determine the system page size if possible */
#ifdef HAVE_SYSCONF
#if (!defined(_SC_PAGESIZE) && defined(_SC_PAGE_SIZE))
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif
#ifdef _SC_PAGESIZE
	sysval = sysconf(_SC_PAGESIZE);
	if (sysval > 0)
		sys_page_size = sysval;
#endif /* _SC_PAGESIZE */
#endif /* HAVE_SYSCONF */

	if ((tmp = getenv("MKE2FS_CONFIG")) != NULL)
		config_fn[0] = tmp;
	profile_set_syntax_err_cb(syntax_err_report);
	retval = profile_init(config_fn, &profile);
	if (retval == ENOENT) {
		profile_init(default_files, &profile);
		profile_set_default(profile, mke2fs_default_profile);
	}

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	add_error_table(&et_ext2_error_table);
	add_error_table(&et_prof_error_table);
	memset(&fs_param, 0, sizeof(struct ext2_super_block));
	fs_param.s_rev_level = 1;  /* Create revision 1 filesystems now */

#ifdef __linux__
	if (uname(&ut)) {
		perror("uname");
		exit(1);
	}
	linux_version_code = parse_version_number(ut.release);
	if (linux_version_code && linux_version_code < (2*65536 + 2*256))
		fs_param.s_rev_level = 0;
#endif

	if (argc && *argv) {
		program_name = get_progname(*argv);

		/* If called as mkfs.ext3, create a journal inode */
		if (!strcmp(program_name, "mkfs.ext3") ||
		    !strcmp(program_name, "mke3fs"))
			journal_size = -1;
	}

	while ((c = getopt (argc, argv,
		    "b:cf:g:G:i:jl:m:no:qr:s:t:vE:FI:J:KL:M:N:O:R:ST:U:V")) != EOF) {
		switch (c) {
		case 'b':
			blocksize = strtol(optarg, &tmp, 0);
			b = (blocksize > 0) ? blocksize : -blocksize;
			if (b < EXT2_MIN_BLOCK_SIZE ||
			    b > EXT2_MAX_BLOCK_SIZE || *tmp) {
				com_err(program_name, 0,
					_("invalid block size - %s"), optarg);
				exit(1);
			}
			if (blocksize > 4096)
				fprintf(stderr, _("Warning: blocksize %d not "
						  "usable on most systems.\n"),
					blocksize);
			if (blocksize > 0)
				fs_param.s_log_block_size =
					int_log2(blocksize >>
						 EXT2_MIN_BLOCK_LOG_SIZE);
			break;
		case 'c':	/* Check for bad blocks */
			cflag++;
			break;
		case 'f':
			size = strtoul(optarg, &tmp, 0);
			if (size < EXT2_MIN_BLOCK_SIZE ||
			    size > EXT2_MAX_BLOCK_SIZE || *tmp) {
				com_err(program_name, 0,
					_("invalid fragment size - %s"),
					optarg);
				exit(1);
			}
			fs_param.s_log_frag_size =
				int_log2(size >> EXT2_MIN_BLOCK_LOG_SIZE);
			fprintf(stderr, _("Warning: fragments not supported.  "
			       "Ignoring -f option\n"));
			break;
		case 'g':
			fs_param.s_blocks_per_group = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(program_name, 0,
					_("Illegal number for blocks per group"));
				exit(1);
			}
			if ((fs_param.s_blocks_per_group % 8) != 0) {
				com_err(program_name, 0,
				_("blocks per group must be multiple of 8"));
				exit(1);
			}
			break;
		case 'G':
			flex_bg_size = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(program_name, 0,
					_("Illegal number for flex_bg size"));
				exit(1);
			}
			if (flex_bg_size < 1 ||
			    (flex_bg_size & (flex_bg_size-1)) != 0) {
				com_err(program_name, 0,
					_("flex_bg size must be a power of 2"));
				exit(1);
			}
			break;
		case 'i':
			inode_ratio = strtoul(optarg, &tmp, 0);
			if (inode_ratio < EXT2_MIN_BLOCK_SIZE ||
			    inode_ratio > EXT2_MAX_BLOCK_SIZE * 1024 ||
			    *tmp) {
				com_err(program_name, 0,
					_("invalid inode ratio %s (min %d/max %d)"),
					optarg, EXT2_MIN_BLOCK_SIZE,
					EXT2_MAX_BLOCK_SIZE * 1024);
				exit(1);
			}
			break;
		case 'J':
			parse_journal_opts(optarg);
			break;
		case 'K':
			fprintf(stderr, _("Warning: -K option is deprecated and "
					  "should not be used anymore. Use "
					  "\'-E nodiscard\' extended option "
					  "instead!\n"));
			discard = 0;
			break;
		case 'j':
			if (!journal_size)
				journal_size = -1;
			break;
		case 'l':
			bad_blocks_filename = malloc(strlen(optarg)+1);
			if (!bad_blocks_filename) {
				com_err(program_name, ENOMEM,
					_("in malloc for bad_blocks_filename"));
				exit(1);
			}
			strcpy(bad_blocks_filename, optarg);
			break;
		case 'm':
			reserved_ratio = strtod(optarg, &tmp);
			if ( *tmp || reserved_ratio > 50 ||
			     reserved_ratio < 0) {
				com_err(program_name, 0,
					_("invalid reserved blocks percent - %s"),
					optarg);
				exit(1);
			}
			break;
		case 'n':
			noaction++;
			break;
		case 'o':
			creator_os = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			r_opt = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(program_name, 0,
					_("bad revision level - %s"), optarg);
				exit(1);
			}
			fs_param.s_rev_level = r_opt;
			break;
		case 's':	/* deprecated */
			s_opt = atoi(optarg);
			break;
		case 'I':
			inode_size = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(program_name, 0,
					_("invalid inode size - %s"), optarg);
				exit(1);
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case 'F':
			force++;
			break;
		case 'L':
			volume_label = optarg;
			break;
		case 'M':
			mount_dir = optarg;
			break;
		case 'N':
			num_inodes = strtoul(optarg, &tmp, 0);
			if (*tmp) {
				com_err(program_name, 0,
					_("bad num inodes - %s"), optarg);
					exit(1);
			}
			break;
		case 'O':
			fs_features = optarg;
			break;
		case 'E':
		case 'R':
			extended_opts = optarg;
			break;
		case 'S':
			super_only = 1;
			break;
		case 't':
			fs_type = optarg;
			break;
		case 'T':
			usage_types = optarg;
			break;
		case 'U':
			fs_uuid = optarg;
			break;
		case 'V':
			/* Print version number and exit */
			show_version_only++;
			break;
		default:
			usage();
		}
	}
	if ((optind == argc) && !show_version_only)
		usage();
	device_name = argv[optind++];

	if (!quiet || show_version_only)
		fprintf (stderr, "mke2fs %s (%s)\n", E2FSPROGS_VERSION,
			 E2FSPROGS_DATE);

	if (show_version_only) {
		fprintf(stderr, _("\tUsing %s\n"),
			error_message(EXT2_ET_BASE));
		exit(0);
	}

	/*
	 * If there's no blocksize specified and there is a journal
	 * device, use it to figure out the blocksize
	 */
	if (blocksize <= 0 && journal_device) {
		ext2_filsys	jfs;
		io_manager	io_ptr;

#ifdef CONFIG_TESTIO_DEBUG
		if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
			io_ptr = test_io_manager;
			test_io_backing_manager = unix_io_manager;
		} else
#endif
			io_ptr = unix_io_manager;
		retval = ext2fs_open(journal_device,
				     EXT2_FLAG_JOURNAL_DEV_OK, 0,
				     0, io_ptr, &jfs);
		if (retval) {
			com_err(program_name, retval,
				_("while trying to open journal device %s\n"),
				journal_device);
			exit(1);
		}
		if ((blocksize < 0) && (jfs->blocksize < (unsigned) (-blocksize))) {
			com_err(program_name, 0,
				_("Journal dev blocksize (%d) smaller than "
				  "minimum blocksize %d\n"), jfs->blocksize,
				-blocksize);
			exit(1);
		}
		blocksize = jfs->blocksize;
		printf(_("Using journal device's blocksize: %d\n"), blocksize);
		fs_param.s_log_block_size =
			int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);
		ext2fs_close(jfs);
	}

	if (blocksize > sys_page_size) {
		if (!force) {
			com_err(program_name, 0,
				_("%d-byte blocks too big for system (max %d)"),
				blocksize, sys_page_size);
			proceed_question();
		}
		fprintf(stderr, _("Warning: %d-byte blocks too big for system "
				  "(max %d), forced to continue\n"),
			blocksize, sys_page_size);
	}
	if (optind < argc) {
		fs_param.s_blocks_count = parse_num_blocks(argv[optind++],
				fs_param.s_log_block_size);
		if (!fs_param.s_blocks_count) {
			com_err(program_name, 0,
				_("invalid blocks count '%s' on device '%s'"),
				argv[optind - 1], device_name);
			exit(1);
		}
	}
	if (optind < argc)
		usage();

	if (!force)
		check_plausibility(device_name);
	check_mount(device_name, force, _("filesystem"));

	fs_param.s_log_frag_size = fs_param.s_log_block_size;

	if (noaction && fs_param.s_blocks_count) {
		dev_size = fs_param.s_blocks_count;
		retval = 0;
	} else {
	retry:
		retval = ext2fs_get_device_size(device_name,
						EXT2_BLOCK_SIZE(&fs_param),
						&dev_size);
		if ((retval == EFBIG) &&
		    (blocksize == 0) &&
		    (fs_param.s_log_block_size == 0)) {
			fs_param.s_log_block_size = 2;
			blocksize = 4096;
			goto retry;
		}
	}

	if (retval == EFBIG) {
		blk64_t	big_dev_size;

		if (blocksize < 4096) {
			fs_param.s_log_block_size = 2;
			blocksize = 4096;
		}
		retval = ext2fs_get_device_size2(device_name,
				 EXT2_BLOCK_SIZE(&fs_param), &big_dev_size);
		if (retval)
			goto get_size_failure;
		if (big_dev_size == (1ULL << 32)) {
			dev_size = (blk_t) (big_dev_size - 1);
			goto got_size;
		}
		fprintf(stderr, _("%s: Size of device %s too big "
				  "to be expressed in 32 bits\n\t"
				  "using a blocksize of %d.\n"),
			program_name, device_name, EXT2_BLOCK_SIZE(&fs_param));
		exit(1);
	}
get_size_failure:
	if (retval && (retval != EXT2_ET_UNIMPLEMENTED)) {
		com_err(program_name, retval,
			_("while trying to determine filesystem size"));
		exit(1);
	}
got_size:
	if (!fs_param.s_blocks_count) {
		if (retval == EXT2_ET_UNIMPLEMENTED) {
			com_err(program_name, 0,
				_("Couldn't determine device size; you "
				"must specify\nthe size of the "
				"filesystem\n"));
			exit(1);
		} else {
			if (dev_size == 0) {
				com_err(program_name, 0,
				_("Device size reported to be zero.  "
				  "Invalid partition specified, or\n\t"
				  "partition table wasn't reread "
				  "after running fdisk, due to\n\t"
				  "a modified partition being busy "
				  "and in use.  You may need to reboot\n\t"
				  "to re-read your partition table.\n"
				  ));
				exit(1);
			}
			fs_param.s_blocks_count = dev_size;
			if (sys_page_size > EXT2_BLOCK_SIZE(&fs_param))
				fs_param.s_blocks_count &= ~((sys_page_size /
					   EXT2_BLOCK_SIZE(&fs_param))-1);
		}

	} else if (!force && (fs_param.s_blocks_count > dev_size)) {
		com_err(program_name, 0,
			_("Filesystem larger than apparent device size."));
		proceed_question();
	}

	fs_types = parse_fs_type(fs_type, usage_types, &fs_param, argv[0]);
	if (!fs_types) {
		fprintf(stderr, _("Failed to parse fs types list\n"));
		exit(1);
	}

	/* Figure out what features should be enabled */

	tmp = NULL;
	if (fs_param.s_rev_level != EXT2_GOOD_OLD_REV) {
		tmp = get_string_from_profile(fs_types, "base_features",
		      "sparse_super,filetype,resize_inode,dir_index");
		edit_feature(tmp, &fs_param.s_feature_compat);
		free(tmp);

		for (cpp = fs_types; *cpp; cpp++) {
			tmp = NULL;
			profile_get_string(profile, "fs_types", *cpp,
					   "features", "", &tmp);
			if (tmp && *tmp)
				edit_feature(tmp, &fs_param.s_feature_compat);
			free(tmp);
		}
		tmp = get_string_from_profile(fs_types, "default_features",
					      "");
	}
	edit_feature(fs_features ? fs_features : tmp,
		     &fs_param.s_feature_compat);
	free(tmp);

	if (fs_param.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		fs_types[0] = strdup("journal");
		fs_types[1] = 0;
	}

	if (verbose) {
		fputs(_("fs_types for mke2fs.conf resolution: "), stdout);
		print_str_list(fs_types);
	}

	if (r_opt == EXT2_GOOD_OLD_REV &&
	    (fs_param.s_feature_compat || fs_param.s_feature_incompat ||
	     fs_param.s_feature_ro_compat)) {
		fprintf(stderr, _("Filesystem features not supported "
				  "with revision 0 filesystems\n"));
		exit(1);
	}

	if (s_opt > 0) {
		if (r_opt == EXT2_GOOD_OLD_REV) {
			fprintf(stderr, _("Sparse superblocks not supported "
				  "with revision 0 filesystems\n"));
			exit(1);
		}
		fs_param.s_feature_ro_compat |=
			EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
	} else if (s_opt == 0)
		fs_param.s_feature_ro_compat &=
			~EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;

	if (journal_size != 0) {
		if (r_opt == EXT2_GOOD_OLD_REV) {
			fprintf(stderr, _("Journals not supported "
				  "with revision 0 filesystems\n"));
			exit(1);
		}
		fs_param.s_feature_compat |=
			EXT3_FEATURE_COMPAT_HAS_JOURNAL;
	}

	if (fs_param.s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		reserved_ratio = 0;
		fs_param.s_feature_incompat = EXT3_FEATURE_INCOMPAT_JOURNAL_DEV;
		fs_param.s_feature_compat = 0;
		fs_param.s_feature_ro_compat = 0;
 	}

	if ((fs_param.s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG) &&
	    (fs_param.s_feature_compat & EXT2_FEATURE_COMPAT_RESIZE_INODE)) {
		fprintf(stderr, _("The resize_inode and meta_bg features "
				  "are not compatible.\n"
				  "They can not be both enabled "
				  "simultaneously.\n"));
		exit(1);
	}

	/* Set first meta blockgroup via an environment variable */
	/* (this is mostly for debugging purposes) */
	if ((fs_param.s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG) &&
	    ((tmp = getenv("MKE2FS_FIRST_META_BG"))))
		fs_param.s_first_meta_bg = atoi(tmp);

	/* Get the hardware sector sizes, if available */
	retval = ext2fs_get_device_sectsize(device_name, &lsector_size);
	if (retval) {
		com_err(program_name, retval,
			_("while trying to determine hardware sector size"));
		exit(1);
	}
	retval = ext2fs_get_device_phys_sectsize(device_name, &psector_size);
	if (retval) {
		com_err(program_name, retval,
			_("while trying to determine physical sector size"));
		exit(1);
	}

	if ((tmp = getenv("MKE2FS_DEVICE_SECTSIZE")) != NULL)
		lsector_size = atoi(tmp);
	if ((tmp = getenv("MKE2FS_DEVICE_PHYS_SECTSIZE")) != NULL)
		psector_size = atoi(tmp);

	/* Older kernels may not have physical/logical distinction */
	if (!psector_size)
		psector_size = lsector_size;

	if (blocksize <= 0) {
		use_bsize = get_int_from_profile(fs_types, "blocksize", 4096);

		if (use_bsize == -1) {
			use_bsize = sys_page_size;
			if ((linux_version_code < (2*65536 + 6*256)) &&
			    (use_bsize > 4096))
				use_bsize = 4096;
		}
		if (lsector_size && use_bsize < lsector_size)
			use_bsize = lsector_size;
		if ((blocksize < 0) && (use_bsize < (-blocksize)))
			use_bsize = -blocksize;
		blocksize = use_bsize;
		fs_param.s_blocks_count /= blocksize / 1024;
	} else {
		if (blocksize < lsector_size) {			/* Impossible */
			com_err(program_name, EINVAL,
				_("while setting blocksize; too small "
				  "for device\n"));
			exit(1);
		} else if ((blocksize < psector_size) &&
			   (psector_size <= sys_page_size)) {	/* Suboptimal */
			fprintf(stderr, _("Warning: specified blocksize %d is "
				"less than device physical sectorsize %d\n"),
				blocksize, psector_size);
		}
	}

	if (inode_ratio == 0) {
		inode_ratio = get_int_from_profile(fs_types, "inode_ratio",
						   8192);
		if (inode_ratio < blocksize)
			inode_ratio = blocksize;
	}

	fs_param.s_log_frag_size = fs_param.s_log_block_size =
		int_log2(blocksize >> EXT2_MIN_BLOCK_LOG_SIZE);

#ifdef HAVE_BLKID_PROBE_GET_TOPOLOGY
	retval = get_device_geometry(device_name, &fs_param, psector_size);
	if (retval < 0) {
		fprintf(stderr,
			_("warning: Unable to get device geometry for %s\n"),
			device_name);
	} else if (retval) {
		printf(_("%s alignment is offset by %lu bytes.\n"),
		       device_name, retval);
		printf(_("This may result in very poor performance, "
			  "(re)-partitioning suggested.\n"));
	}
#endif

	blocksize = EXT2_BLOCK_SIZE(&fs_param);

	lazy_itable_init = 0;
	if (access("/sys/fs/ext4/features/lazy_itable_init", R_OK) == 0)
		lazy_itable_init = 1;

	lazy_itable_init = get_bool_from_profile(fs_types,
						 "lazy_itable_init",
						 lazy_itable_init);
	discard = get_bool_from_profile(fs_types, "discard" , discard);

	/* Get options from profile */
	for (cpp = fs_types; *cpp; cpp++) {
		tmp = NULL;
		profile_get_string(profile, "fs_types", *cpp, "options", "", &tmp);
			if (tmp && *tmp)
				parse_extended_opts(&fs_param, tmp);
			free(tmp);
	}

	if (extended_opts)
		parse_extended_opts(&fs_param, extended_opts);

	/* Since sparse_super is the default, we would only have a problem
	 * here if it was explicitly disabled.
	 */
	if ((fs_param.s_feature_compat & EXT2_FEATURE_COMPAT_RESIZE_INODE) &&
	    !(fs_param.s_feature_ro_compat&EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		com_err(program_name, 0,
			_("reserved online resize blocks not supported "
			  "on non-sparse filesystem"));
		exit(1);
	}

	if (fs_param.s_blocks_per_group) {
		if (fs_param.s_blocks_per_group < 256 ||
		    fs_param.s_blocks_per_group > 8 * (unsigned) blocksize) {
			com_err(program_name, 0,
				_("blocks per group count out of range"));
			exit(1);
		}
	}

	if (inode_size == 0)
		inode_size = get_int_from_profile(fs_types, "inode_size", 0);
	if (!flex_bg_size && (fs_param.s_feature_incompat &
			      EXT4_FEATURE_INCOMPAT_FLEX_BG))
		flex_bg_size = get_int_from_profile(fs_types,
						    "flex_bg_size", 16);
	if (flex_bg_size) {
		if (!(fs_param.s_feature_incompat &
		      EXT4_FEATURE_INCOMPAT_FLEX_BG)) {
			com_err(program_name, 0,
				_("Flex_bg feature not enabled, so "
				  "flex_bg size may not be specified"));
			exit(1);
		}
		fs_param.s_log_groups_per_flex = int_log2(flex_bg_size);
	}

	if (inode_size && fs_param.s_rev_level >= EXT2_DYNAMIC_REV) {
		if (inode_size < EXT2_GOOD_OLD_INODE_SIZE ||
		    inode_size > EXT2_BLOCK_SIZE(&fs_param) ||
		    inode_size & (inode_size - 1)) {
			com_err(program_name, 0,
				_("invalid inode size %d (min %d/max %d)"),
				inode_size, EXT2_GOOD_OLD_INODE_SIZE,
				blocksize);
			exit(1);
		}
		fs_param.s_inode_size = inode_size;
	}

	/* Make sure number of inodes specified will fit in 32 bits */
	if (num_inodes == 0) {
		unsigned long long n;
		n = (unsigned long long) fs_param.s_blocks_count * blocksize / inode_ratio;
		if (n > ~0U) {
			com_err(program_name, 0,
			    _("too many inodes (%llu), raise inode ratio?"), n);
			exit(1);
		}
	} else if (num_inodes > ~0U) {
		com_err(program_name, 0,
			_("too many inodes (%llu), specify < 2^32 inodes"),
			  num_inodes);
		exit(1);
	}
	/*
	 * Calculate number of inodes based on the inode ratio
	 */
	fs_param.s_inodes_count = num_inodes ? num_inodes :
		((__u64) fs_param.s_blocks_count * blocksize)
			/ inode_ratio;

	if ((((long long)fs_param.s_inodes_count) *
	     (inode_size ? inode_size : EXT2_GOOD_OLD_INODE_SIZE)) >=
	    (((long long)fs_param.s_blocks_count) *
	     EXT2_BLOCK_SIZE(&fs_param))) {
		com_err(program_name, 0, _("inode_size (%u) * inodes_count "
					  "(%u) too big for a\n\t"
					  "filesystem with %lu blocks, "
					  "specify higher inode_ratio (-i)\n\t"
					  "or lower inode count (-N).\n"),
			inode_size ? inode_size : EXT2_GOOD_OLD_INODE_SIZE,
			fs_param.s_inodes_count,
			(unsigned long) fs_param.s_blocks_count);
		exit(1);
	}

	/*
	 * Calculate number of blocks to reserve
	 */
	fs_param.s_r_blocks_count = (unsigned int) (reserved_ratio *
					fs_param.s_blocks_count / 100.0);
}

static int should_do_undo(const char *name)
{
	errcode_t retval;
	io_channel channel;
	__u16	s_magic;
	struct ext2_super_block super;
	io_manager manager = unix_io_manager;
	int csum_flag, force_undo;

	csum_flag = EXT2_HAS_RO_COMPAT_FEATURE(&fs_param,
					       EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
	force_undo = get_int_from_profile(fs_types, "force_undo", 0);
	if (!force_undo && (!csum_flag || !lazy_itable_init))
		return 0;

	retval = manager->open(name, IO_FLAG_EXCLUSIVE,  &channel);
	if (retval) {
		/*
		 * We don't handle error cases instead we
		 * declare that the file system doesn't exist
		 * and let the rest of mke2fs take care of
		 * error
		 */
		retval = 0;
		goto open_err_out;
	}

	io_channel_set_blksize(channel, SUPERBLOCK_OFFSET);
	retval = io_channel_read_blk(channel, 1, -SUPERBLOCK_SIZE, &super);
	if (retval) {
		retval = 0;
		goto err_out;
	}

#if defined(WORDS_BIGENDIAN)
	s_magic = ext2fs_swab16(super.s_magic);
#else
	s_magic = super.s_magic;
#endif

	if (s_magic == EXT2_SUPER_MAGIC)
		retval = 1;

err_out:
	io_channel_close(channel);

open_err_out:

	return retval;
}

static int mke2fs_setup_tdb(const char *name, io_manager *io_ptr)
{
	errcode_t retval = 0;
	char *tdb_dir, *tdb_file;
	char *device_name, *tmp_name;

	/*
	 * Configuration via a conf file would be
	 * nice
	 */
	tdb_dir = getenv("E2FSPROGS_UNDO_DIR");
	if (!tdb_dir)
		profile_get_string(profile, "defaults",
				   "undo_dir", 0, "/var/lib/e2fsprogs",
				   &tdb_dir);

	if (!strcmp(tdb_dir, "none") || (tdb_dir[0] == 0) ||
	    access(tdb_dir, W_OK))
		return 0;

	tmp_name = strdup(name);
	if (!tmp_name) {
	alloc_fn_fail:
		com_err(program_name, ENOMEM, 
			_("Couldn't allocate memory for tdb filename\n"));
		return ENOMEM;
	}
	device_name = basename(tmp_name);
	tdb_file = malloc(strlen(tdb_dir) + 8 + strlen(device_name) + 7 + 1);
	if (!tdb_file)
		goto alloc_fn_fail;
	sprintf(tdb_file, "%s/mke2fs-%s.e2undo", tdb_dir, device_name);

	if (!access(tdb_file, F_OK)) {
		if (unlink(tdb_file) < 0) {
			retval = errno;
			com_err(program_name, retval,
				_("while trying to delete %s"),
				tdb_file);
			free(tdb_file);
			return retval;
		}
	}

	set_undo_io_backing_manager(*io_ptr);
	*io_ptr = undo_io_manager;
	set_undo_io_backup_file(tdb_file);
	printf(_("Overwriting existing filesystem; this can be undone "
		 "using the command:\n"
		 "    e2undo %s %s\n\n"), tdb_file, name);

	free(tdb_file);
	free(tmp_name);
	return retval;
}

#ifdef __linux__

#ifndef BLKDISCARD
#define BLKDISCARD	_IO(0x12,119)
#endif

#ifndef BLKDISCARDZEROES
#define BLKDISCARDZEROES _IO(0x12,124)
#endif

/*
 * Return zero if the discard succeeds, and -1 if the discard fails.
 */
static int mke2fs_discard_blocks(ext2_filsys fs)
{
	int fd;
	int ret;
	int blocksize;
	__u64 blocks;
	__uint64_t range[2];

	blocks = fs->super->s_blocks_count;
	blocksize = EXT2_BLOCK_SIZE(fs->super);
	range[0] = 0;
	range[1] = blocks * blocksize;

	fd = open64(fs->device_name, O_RDWR);

	if (fd > 0) {
		ret = ioctl(fd, BLKDISCARD, &range);
		if (verbose) {
			printf(_("Calling BLKDISCARD from %llu to %llu "),
			       (unsigned long long) range[0],
			       (unsigned long long) range[1]);
			if (ret)
				printf(_("failed.\n"));
			else
				printf(_("succeeded.\n"));
		}
		close(fd);
	}
	return ret;
}

static int mke2fs_discard_zeroes_data(ext2_filsys fs)
{
	int fd;
	int ret;
	int discard_zeroes_data = 0;

	fd = open64(fs->device_name, O_RDWR);

	if (fd > 0) {
		ioctl(fd, BLKDISCARDZEROES, &discard_zeroes_data);
		close(fd);
	}
	return discard_zeroes_data;
}
#else
#define mke2fs_discard_blocks(fs)	1
#define mke2fs_discard_zeroes_data(fs)	0
#endif

int main (int argc, char *argv[])
{
	errcode_t	retval = 0;
	ext2_filsys	fs;
	badblocks_list	bb_list = 0;
	unsigned int	journal_blocks;
	unsigned int	i;
	int		val, hash_alg;
	io_manager	io_ptr;
	char		tdb_string[40];
	char		*hash_alg_str;
	int		itable_zeroed = 0;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	PRS(argc, argv);

#ifdef CONFIG_TESTIO_DEBUG
	if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
	} else
#endif
		io_ptr = unix_io_manager;

	if (should_do_undo(device_name)) {
		retval = mke2fs_setup_tdb(device_name, &io_ptr);
		if (retval)
			exit(1);
	}

	/*
	 * Initialize the superblock....
	 */
	retval = ext2fs_initialize(device_name, EXT2_FLAG_EXCLUSIVE, &fs_param,
				   io_ptr, &fs);
	if (retval) {
		com_err(device_name, retval, _("while setting up superblock"));
		exit(1);
	}

	/* Can't undo discard ... */
	if (discard && (io_ptr != undo_io_manager)) {
		retval = mke2fs_discard_blocks(fs);

		if (!retval && mke2fs_discard_zeroes_data(fs)) {
			if (verbose)
				printf(_("Discard succeeded and will return 0s "
					 " - skipping inode table wipe\n"));
			lazy_itable_init = 1;
			itable_zeroed = 1;
		}
	}

	sprintf(tdb_string, "tdb_data_size=%d", fs->blocksize <= 4096 ?
		32768 : fs->blocksize * 8);
	io_channel_set_options(fs->io, tdb_string);

	if (fs_param.s_flags & EXT2_FLAGS_TEST_FILESYS)
		fs->super->s_flags |= EXT2_FLAGS_TEST_FILESYS;

	if ((fs_param.s_feature_incompat &
	     (EXT3_FEATURE_INCOMPAT_EXTENTS|EXT4_FEATURE_INCOMPAT_FLEX_BG)) ||
	    (fs_param.s_feature_ro_compat &
	     (EXT4_FEATURE_RO_COMPAT_HUGE_FILE|EXT4_FEATURE_RO_COMPAT_GDT_CSUM|
	      EXT4_FEATURE_RO_COMPAT_DIR_NLINK|
	      EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE)))
		fs->super->s_kbytes_written = 1;

	/*
	 * Wipe out the old on-disk superblock
	 */
	if (!noaction)
		zap_sector(fs, 2, 6);

	/*
	 * Parse or generate a UUID for the filesystem
	 */
	if (fs_uuid) {
		if (uuid_parse(fs_uuid, fs->super->s_uuid) !=0) {
			com_err(device_name, 0, "could not parse UUID: %s\n",
				fs_uuid);
			exit(1);
		}
	} else
		uuid_generate(fs->super->s_uuid);

	/*
	 * Initialize the directory index variables
	 */
	hash_alg_str = get_string_from_profile(fs_types, "hash_alg",
					       "half_md4");
	hash_alg = e2p_string2hash(hash_alg_str);
	free(hash_alg_str);
	fs->super->s_def_hash_version = (hash_alg >= 0) ? hash_alg :
		EXT2_HASH_HALF_MD4;
	uuid_generate((unsigned char *) fs->super->s_hash_seed);

	/*
	 * Add "jitter" to the superblock's check interval so that we
	 * don't check all the filesystems at the same time.  We use a
	 * kludgy hack of using the UUID to derive a random jitter value.
	 */
	for (i = 0, val = 0 ; i < sizeof(fs->super->s_uuid); i++)
		val += fs->super->s_uuid[i];
	fs->super->s_max_mnt_count += val % EXT2_DFL_MAX_MNT_COUNT;

	/*
	 * Override the creator OS, if applicable
	 */
	if (creator_os && !set_os(fs->super, creator_os)) {
		com_err (program_name, 0, _("unknown os - %s"), creator_os);
		exit(1);
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
		memset(fs->super->s_volume_name, 0,
		       sizeof(fs->super->s_volume_name));
		strncpy(fs->super->s_volume_name, volume_label,
			sizeof(fs->super->s_volume_name));
	}

	/*
	 * Set the last mount directory
	 */
	if (mount_dir) {
		memset(fs->super->s_last_mounted, 0,
		       sizeof(fs->super->s_last_mounted));
		strncpy(fs->super->s_last_mounted, mount_dir,
			sizeof(fs->super->s_last_mounted));
	}

	if (!quiet || noaction)
		show_stats(fs);

	if (noaction)
		exit(0);

	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
		create_journal_dev(fs);
		exit(ext2fs_close(fs) ? 1 : 0);
	}

	if (bad_blocks_filename)
		read_bb_file(fs, &bb_list, bad_blocks_filename);
	if (cflag)
		test_disk(fs, &bb_list);

	handle_bad_blocks(fs, bb_list);
	fs->stride = fs_stride = fs->super->s_raid_stride;
	retval = ext2fs_allocate_tables(fs);
	if (retval) {
		com_err(program_name, retval,
			_("while trying to allocate filesystem tables"));
		exit(1);
	}
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
			retval = ext2fs_zero_blocks(fs, start, blocks - start,
						    &ret_blk, NULL);

		if (retval) {
			com_err(program_name, retval,
				_("while zeroing block %u at end of filesystem"),
				ret_blk);
		}
		write_inode_tables(fs, lazy_itable_init, itable_zeroed);
		create_root_dir(fs);
		create_lost_and_found(fs);
		reserve_inodes(fs);
		create_bad_block_inode(fs, bb_list);
		if (fs->super->s_feature_compat &
		    EXT2_FEATURE_COMPAT_RESIZE_INODE) {
			retval = ext2fs_create_resize_inode(fs);
			if (retval) {
				com_err("ext2fs_create_resize_inode", retval,
				_("while reserving blocks for online resize"));
				exit(1);
			}
		}
	}

	if (journal_device) {
		ext2_filsys	jfs;

		if (!force)
			check_plausibility(journal_device);
		check_mount(journal_device, force, _("journal"));

		retval = ext2fs_open(journal_device, EXT2_FLAG_RW|
				     EXT2_FLAG_JOURNAL_DEV_OK, 0,
				     fs->blocksize, unix_io_manager, &jfs);
		if (retval) {
			com_err(program_name, retval,
				_("while trying to open journal device %s\n"),
				journal_device);
			exit(1);
		}
		if (!quiet) {
			printf(_("Adding journal to device %s: "),
			       journal_device);
			fflush(stdout);
		}
		retval = ext2fs_add_journal_device(fs, jfs);
		if(retval) {
			com_err (program_name, retval,
				 _("\n\twhile trying to add journal to device %s"),
				 journal_device);
			exit(1);
		}
		if (!quiet)
			printf(_("done\n"));
		ext2fs_close(jfs);
		free(journal_device);
	} else if ((journal_size) ||
		   (fs_param.s_feature_compat &
		    EXT3_FEATURE_COMPAT_HAS_JOURNAL)) {
		journal_blocks = figure_journal_size(journal_size, fs);

		if (super_only) {
			printf(_("Skipping journal creation in super-only mode\n"));
			fs->super->s_journal_inum = EXT2_JOURNAL_INO;
			goto no_journal;
		}

		if (!journal_blocks) {
			fs->super->s_feature_compat &=
				~EXT3_FEATURE_COMPAT_HAS_JOURNAL;
			goto no_journal;
		}
		if (!quiet) {
			printf(_("Creating journal (%u blocks): "),
			       journal_blocks);
			fflush(stdout);
		}
		retval = ext2fs_add_journal_inode(fs, journal_blocks,
						  journal_flags);
		if (retval) {
			com_err (program_name, retval,
				 _("\n\twhile trying to create journal"));
			exit(1);
		}
		if (!quiet)
			printf(_("done\n"));
	}
no_journal:

	if (!quiet)
		printf(_("Writing superblocks and "
		       "filesystem accounting information: "));
	retval = ext2fs_flush(fs);
	if (retval) {
		fprintf(stderr,
			_("\nWarning, had trouble writing out superblocks."));
	}
	if (!quiet) {
		printf(_("done\n\n"));
		if (!getenv("MKE2FS_SKIP_CHECK_MSG"))
			print_check_message(fs);
	}
	val = ext2fs_close(fs);
	remove_error_table(&et_ext2_error_table);
	remove_error_table(&et_prof_error_table);
	profile_release(profile);
	for (i=0; fs_types[i]; i++)
		free(fs_types[i]);
	free(fs_types);
	return (retval || val) ? 1 : 0;
}
