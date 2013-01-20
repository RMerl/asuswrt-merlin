/*
 * main.c --- ext2 resizer main program
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <unistd.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "e2p/e2p.h"

#include "resize2fs.h"

#include "../version.h"

char *program_name, *device_name, *io_options;

static void usage (char *prog)
{
	fprintf (stderr, _("Usage: %s [-d debug_flags] [-f] [-F] [-M] [-P] "
			   "[-p] device [new_size]\n\n"), prog);

	exit (1);
}

static errcode_t resize_progress_func(ext2_resize_t rfs, int pass,
				      unsigned long cur, unsigned long max)
{
	ext2_sim_progmeter progress;
	const char	*label;
	errcode_t	retval;

	progress = (ext2_sim_progmeter) rfs->prog_data;
	if (max == 0)
		return 0;
	if (cur == 0) {
		if (progress)
			ext2fs_progress_close(progress);
		progress = 0;
		switch (pass) {
		case E2_RSZ_EXTEND_ITABLE_PASS:
			label = _("Extending the inode table");
			break;
		case E2_RSZ_BLOCK_RELOC_PASS:
			label = _("Relocating blocks");
			break;
		case E2_RSZ_INODE_SCAN_PASS:
			label = _("Scanning inode table");
			break;
		case E2_RSZ_INODE_REF_UPD_PASS:
			label = _("Updating inode references");
			break;
		case E2_RSZ_MOVE_ITABLE_PASS:
			label = _("Moving inode table");
			break;
		default:
			label = _("Unknown pass?!?");
			break;
		}
		printf(_("Begin pass %d (max = %lu)\n"), pass, max);
		retval = ext2fs_progress_init(&progress, label, 30,
					      40, max, 0);
		if (retval)
			progress = 0;
		rfs->prog_data = (void *) progress;
	}
	if (progress)
		ext2fs_progress_update(progress, cur);
	if (cur >= max) {
		if (progress)
			ext2fs_progress_close(progress);
		progress = 0;
		rfs->prog_data = 0;
	}
	return 0;
}

static void determine_fs_stride(ext2_filsys fs)
{
	unsigned int	group;
	unsigned long long sum;
	unsigned int	has_sb, prev_has_sb, num;
	int		i_stride, b_stride;

	if (fs->stride)
		return;
	num = 0; sum = 0;
	for (group = 0; group < fs->group_desc_count; group++) {
		has_sb = ext2fs_bg_has_super(fs, group);
		if (group == 0 || has_sb != prev_has_sb)
			goto next;
		b_stride = fs->group_desc[group].bg_block_bitmap -
			fs->group_desc[group-1].bg_block_bitmap -
			fs->super->s_blocks_per_group;
		i_stride = fs->group_desc[group].bg_inode_bitmap -
			fs->group_desc[group-1].bg_inode_bitmap -
			fs->super->s_blocks_per_group;
		if (b_stride != i_stride ||
		    b_stride < 0)
			goto next;

		/* printf("group %d has stride %d\n", group, b_stride); */
		sum += b_stride;
		num++;

	next:
		prev_has_sb = has_sb;
	}

	if (fs->group_desc_count > 12 && num < 3)
		sum = 0;

	if (num)
		fs->stride = sum / num;
	else
		fs->stride = 0;

	fs->super->s_raid_stride = fs->stride;
	ext2fs_mark_super_dirty(fs);

#if 0
	if (fs->stride)
		printf("Using RAID stride of %d\n", fs->stride);
#endif
}

int main (int argc, char ** argv)
{
	errcode_t	retval;
	ext2_filsys	fs;
	int		c;
	int		flags = 0;
	int		flush = 0;
	int		force = 0;
	int		io_flags = 0;
	int		force_min_size = 0;
	int		print_min_size = 0;
	int		fd, ret;
	blk_t		new_size = 0;
	blk64_t		max_size = 0;
	blk_t		min_size = 0;
	io_manager	io_ptr;
	char		*new_size_str = 0;
	int		use_stride = -1;
#ifdef HAVE_FSTAT64
	struct stat64	st_buf;
#else
	struct stat	st_buf;
#endif
	__s64		new_file_size;
	unsigned int	sys_page_size = 4096;
	long		sysval;
	int		len, mount_flags;
	char		*mtpt;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif

	add_error_table(&et_ext2_error_table);

	fprintf (stderr, "resize2fs %s (%s)\n",
		 E2FSPROGS_VERSION, E2FSPROGS_DATE);
	if (argc && *argv)
		program_name = *argv;

	while ((c = getopt (argc, argv, "d:fFhMPpS:")) != EOF) {
		switch (c) {
		case 'h':
			usage(program_name);
			break;
		case 'f':
			force = 1;
			break;
		case 'F':
			flush = 1;
			break;
		case 'M':
			force_min_size = 1;
			break;
		case 'P':
			print_min_size = 1;
			break;
		case 'd':
			flags |= atoi(optarg);
			break;
		case 'p':
			flags |= RESIZE_PERCENT_COMPLETE;
			break;
		case 'S':
			use_stride = atoi(optarg);
			break;
		default:
			usage(program_name);
		}
	}
	if (optind == argc)
		usage(program_name);

	device_name = argv[optind++];
	if (optind < argc)
		new_size_str = argv[optind++];
	if (optind < argc)
		usage(program_name);

	io_options = strchr(device_name, '?');
	if (io_options)
		*io_options++ = 0;

	/*
	 * Figure out whether or not the device is mounted, and if it is
	 * where it is mounted.
	 */
	len=80;
	while (1) {
		mtpt = malloc(len);
		if (!mtpt)
			return ENOMEM;
		mtpt[len-1] = 0;
		retval = ext2fs_check_mount_point(device_name, &mount_flags,
						  mtpt, len);
		if (retval) {
			com_err("ext2fs_check_mount_point", retval,
				_("while determining whether %s is mounted."),
				device_name);
			exit(1);
		}
		if (!(mount_flags & EXT2_MF_MOUNTED) || (mtpt[len-1] == 0))
			break;
		free(mtpt);
		len = 2 * len;
	}

#ifdef HAVE_OPEN64
	fd = open64(device_name, O_RDWR);
#else
	fd = open(device_name, O_RDWR);
#endif
	if (fd < 0) {
		com_err("open", errno, _("while opening %s"),
			device_name);
		exit(1);
	}

#ifdef HAVE_FSTAT64
	ret = fstat64(fd, &st_buf);
#else
	ret = fstat(fd, &st_buf);
#endif
	if (ret < 0) {
		com_err("open", errno,
			_("while getting stat information for %s"),
			device_name);
		exit(1);
	}

	if (flush) {
		retval = ext2fs_sync_device(fd, 1);
		if (retval) {
			com_err(argv[0], retval,
				_("while trying to flush %s"),
				device_name);
			exit(1);
		}
	}

	if (!S_ISREG(st_buf.st_mode )) {
		close(fd);
		fd = -1;
	}

#ifdef CONFIG_TESTIO_DEBUG
	if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
	} else
#endif
		io_ptr = unix_io_manager;

	if (!(mount_flags & EXT2_MF_MOUNTED))
		io_flags = EXT2_FLAG_RW | EXT2_FLAG_EXCLUSIVE;
	retval = ext2fs_open2(device_name, io_options, io_flags,
			      0, 0, io_ptr, &fs);
	if (retval) {
		com_err (program_name, retval, _("while trying to open %s"),
			 device_name);
		printf (_("Couldn't find valid filesystem superblock.\n"));
		exit (1);
	}

	/*
	 * Check for compatibility with the feature sets.  We need to
	 * be more stringent than ext2fs_open().
	 */
	if (fs->super->s_feature_compat & ~EXT2_LIB_FEATURE_COMPAT_SUPP) {
		com_err(program_name, EXT2_ET_UNSUPP_FEATURE,
			"(%s)", device_name);
		exit(1);
	}

	/*
	 * XXXX   The combination of flex_bg and !resize_inode causes
	 * major problems for resize2fs, since when the group descriptors
	 * grow in size this can potentially require multiple inode
	 * tables to be moved aside to make room, and resize2fs chokes
	 * rather badly in this scenario.  It's a rare combination,
	 * except when a filesystem is expanded more than a certain
	 * size, so for now, we'll just prohibit that combination.
	 * This is something we should fix eventually, though.
	 */
	if ((fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_FLEX_BG) &&
	    !(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_RESIZE_INODE)) {
		com_err(program_name, 0, _("%s: The combination of flex_bg "
					   "and\n\t!resize_inode features "
					   "is not supported by resize2fs.\n"),
			device_name);
		exit(1);
	}

	min_size = calculate_minimum_resize_size(fs);

	if (print_min_size) {
		if (!force && ((fs->super->s_state & EXT2_ERROR_FS) ||
			       ((fs->super->s_state & EXT2_VALID_FS) == 0))) {
			fprintf(stderr,
				_("Please run 'e2fsck -f %s' first.\n\n"),
				device_name);
			exit(1);
		}
		printf(_("Estimated minimum size of the filesystem: %u\n"),
		       min_size);
		exit(0);
	}

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

	/*
	 * Get the size of the containing partition, and use this for
	 * defaults and for making sure the new filesystem doesn't
	 * exceed the partition size.
	 */
	retval = ext2fs_get_device_size2(device_name, fs->blocksize,
					&max_size);
	if (retval) {
		com_err(program_name, retval,
			_("while trying to determine filesystem size"));
		exit(1);
	}
	if (force_min_size)
		new_size = min_size;
	else if (new_size_str) {
		new_size = parse_num_blocks(new_size_str,
					    fs->super->s_log_block_size);
		if (new_size == 0) {
			com_err(program_name, 0,
				_("Invalid new size: %s\n"), new_size_str);
			exit(1);
		}
	} else {
		/* Take down devices exactly 16T to 2^32-1 blocks */
		if (max_size == (1ULL << 32))
			max_size--;
		else if (max_size > (1ULL << 32)) {
			com_err(program_name, 0, _("New size too large to be "
				"expressed in 32 bits\n"));
			exit(1);
		}
		new_size = max_size;
		/* Round down to an even multiple of a pagesize */
		if (sys_page_size > fs->blocksize)
			new_size &= ~((sys_page_size / fs->blocksize)-1);
	}

	if (!force && new_size < min_size) {
		com_err(program_name, 0,
			_("New size smaller than minimum (%u)\n"), min_size);
		exit(1);
	}
	if (use_stride >= 0) {
		if (use_stride >= (int) fs->super->s_blocks_per_group) {
			com_err(program_name, 0,
				_("Invalid stride length"));
			exit(1);
		}
		fs->stride = fs->super->s_raid_stride = use_stride;
		ext2fs_mark_super_dirty(fs);
	} else
		  determine_fs_stride(fs);

	/*
	 * If we are resizing a plain file, and it's not big enough,
	 * automatically extend it in a sparse fashion by writing the
	 * last requested block.
	 */
	new_file_size = ((__u64) new_size) * fs->blocksize;
	if ((__u64) new_file_size >
	    (((__u64) 1) << (sizeof(st_buf.st_size)*8 - 1)) - 1)
		fd = -1;
	if ((new_file_size > st_buf.st_size) &&
	    (fd > 0)) {
		if ((ext2fs_llseek(fd, new_file_size-1, SEEK_SET) >= 0) &&
		    (write(fd, "0", 1) == 1))
			max_size = new_size;
	}
	if (!force && (new_size > max_size)) {
		fprintf(stderr, _("The containing partition (or device)"
			" is only %llu (%dk) blocks.\nYou requested a new size"
			" of %u blocks.\n\n"), max_size,
			fs->blocksize / 1024, new_size);
		exit(1);
	}
	if (new_size == fs->super->s_blocks_count) {
		fprintf(stderr, _("The filesystem is already %u blocks "
			"long.  Nothing to do!\n\n"), new_size);
		exit(0);
	}
	if (mount_flags & EXT2_MF_MOUNTED) {
		retval = online_resize_fs(fs, mtpt, &new_size, flags);
	} else {
		if (!force && ((fs->super->s_lastcheck < fs->super->s_mtime) ||
			       (fs->super->s_state & EXT2_ERROR_FS) ||
			       ((fs->super->s_state & EXT2_VALID_FS) == 0))) {
			fprintf(stderr,
				_("Please run 'e2fsck -f %s' first.\n\n"),
				device_name);
			exit(1);
		}
		printf(_("Resizing the filesystem on "
			 "%s to %u (%dk) blocks.\n"),
		       device_name, new_size, fs->blocksize / 1024);
		retval = resize_fs(fs, &new_size, flags,
				   ((flags & RESIZE_PERCENT_COMPLETE) ?
				    resize_progress_func : 0));
	}
	free(mtpt);
	if (retval) {
		com_err(program_name, retval, _("while trying to resize %s"),
			device_name);
		fprintf(stderr,
			_("Please run 'e2fsck -fy %s' to fix the filesystem\n"
			  "after the aborted resize operation.\n"),
			device_name);
		ext2fs_close(fs);
		exit(1);
	}
	printf(_("The filesystem on %s is now %u blocks long.\n\n"),
	       device_name, new_size);

	if ((st_buf.st_size > new_file_size) &&
	    (fd > 0)) {
#ifdef HAVE_FTRUNCATE64
		retval = ftruncate64(fd, new_file_size);
#else
		retval = 0;
		/* Only truncate if new_file_size doesn't overflow off_t */
		if (((off_t) new_file_size) == new_file_size)
			retval = ftruncate(fd, (off_t) new_file_size);
#endif
		if (retval)
			com_err(program_name, retval,
				_("while trying to truncate %s"),
				device_name);
	}
	if (fd > 0)
		close(fd);
	remove_error_table(&et_ext2_error_table);
	return (0);
}
