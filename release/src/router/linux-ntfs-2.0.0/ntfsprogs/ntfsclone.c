/**
 * ntfsclone - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003-2006 Szabolcs Szakacsits
 * Copyright (c) 2004-2006 Anton Altaparmakov
 * Special image format support copyright (c) 2004 Per Olofsson
 *
 * Clone NTFS data and/or metadata to a sparse file, image, device or stdout.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/*
 * FIXME: ntfsclone do bad things about endians handling. Fix it and remove
 * this note and define.
 */
#define NTFS_DO_NOT_CHECK_ENDIANS

#include "debug.h"
#include "types.h"
#include "support.h"
#include "endians.h"
#include "bootsect.h"
#include "device.h"
#include "attrib.h"
#include "mst.h"
#include "volume.h"
#include "mft.h"
#include "bitmap.h"
#include "inode.h"
#include "index.h"
#include "dir.h"
#include "runlist.h"
#include "ntfstime.h"
#include "utils.h"
#include "version.h"

#if defined(linux) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE	_IO(0x12,96)  /* Get device size in 512-byte blocks. */
#endif
#if defined(linux) && defined(_IOR) && !defined(BLKGETSIZE64)
#define BLKGETSIZE64	_IOR(0x12,114,size_t)	/* Get device size in bytes. */
#endif

static const char *EXEC_NAME = "ntfsclone";

static const char *bad_sectors_warning_msg =
"*************************************************************************\n"
"* WARNING: The disk has bad sector. This means physical damage on the   *\n"
"* disk surface caused by deterioration, manufacturing faults or other   *\n"
"* reason. The reliability of the disk may stay stable or degrade fast.  *\n"
"* Use the --rescue option to efficiently save as much data as possible! *\n"
"*************************************************************************\n";

static const char *dirty_volume_msg =
"Volume '%s' is scheduled for a check or it was shutdown \n"
"uncleanly. Please boot Windows or use the --force option to progress.\n";

static struct {
	int verbose;
	int quiet;
	int debug;
	int force;
	int overwrite;
	int std_out;
	int blkdev_out;		/* output file is block device */
	int metadata;		/* metadata only cloning */
	int ignore_fs_check;
	int rescue;
	int save_image;
	int restore_image;
	char *output;
	char *volume;
	struct statfs stfs;
} opt;

struct bitmap {
	s64 size;
	u8 *bm;
};

struct progress_bar {
	u64 start;
	u64 stop;
	int resolution;
	float unit;
};

typedef struct {
	ntfs_inode *ni;			/* inode being processed */
	ntfs_attr_search_ctx *ctx;	/* inode attribute being processed */
	s64 inuse;			/* number of clusters in use */
} ntfs_walk_clusters_ctx;

typedef int (ntfs_walk_op)(ntfs_inode *ni, void *data);

struct ntfs_walk_cluster {
	ntfs_walk_op *inode_op;		/* not implemented yet */
	ntfs_walk_clusters_ctx *image;
};


static ntfs_volume *vol = NULL;
static struct bitmap lcn_bitmap;

static int fd_in;
static int fd_out;
static FILE *msg_out = NULL;

static int wipe = 0;
static unsigned int nr_used_mft_records   = 0;
static unsigned int wiped_unused_mft_data = 0;
static unsigned int wiped_unused_mft      = 0;
static unsigned int wiped_resident_data   = 0;
static unsigned int wiped_timestamp_data  = 0;

static BOOL image_is_host_endian = FALSE;

#define IMAGE_MAGIC "\0ntfsclone-image"
#define IMAGE_MAGIC_SIZE 16

/* This is the first endianness safe format version. */
#define NTFSCLONE_IMG_VER_MAJOR_ENDIANNESS_SAFE	10
#define NTFSCLONE_IMG_VER_MINOR_ENDIANNESS_SAFE	0

/*
 * Set the version to 10.0 to avoid colisions with old ntfsclone which
 * stupidly used the volume version as the image version...  )-:  I hope NTFS
 * never reaches version 10.0 and if it does one day I hope no-one is using
 * such an old ntfsclone by then...
 *
 * NOTE: Only bump the minor version if the image format and header are still
 * backwards compatible.  Otherwise always bump the major version.  If in
 * doubt, bump the major version.
 */
#define NTFSCLONE_IMG_VER_MAJOR	10
#define NTFSCLONE_IMG_VER_MINOR	0

/* All values are in little endian. */
static struct {
	char magic[IMAGE_MAGIC_SIZE];
	u8 major_ver;
	u8 minor_ver;
	u32 cluster_size;
	s64 device_size;
	s64 nr_clusters;
	s64 inuse;
	u32 offset_to_image_data;	/* From start of image_hdr. */
} __attribute__((__packed__)) image_hdr;

#define NTFSCLONE_IMG_HEADER_SIZE_OLD	\
		(offsetof(typeof(image_hdr), offset_to_image_data))

#define NTFS_MBYTE (1000 * 1000)

#define ERR_PREFIX   "ERROR"
#define PERR_PREFIX  ERR_PREFIX "(%d): "
#define NERR_PREFIX  ERR_PREFIX ": "

#define LAST_METADATA_INODE	11

#define NTFS_MAX_CLUSTER_SIZE	65536
#define NTFS_SECTOR_SIZE	  512

#define rounded_up_division(a, b) (((a) + (b - 1)) / (b))

#define read_all(f, p, n)  io_all((f), (p), (n), 0)
#define write_all(f, p, n) io_all((f), (p), (n), 1)

__attribute__((format(printf, 1, 2)))
static void Printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(msg_out, fmt, ap);
	va_end(ap);
	fflush(msg_out);
}

__attribute__((format(printf, 1, 2)))
static void perr_printf(const char *fmt, ...)
{
	va_list ap;
	int eo = errno;

	Printf(PERR_PREFIX, eo);
	va_start(ap, fmt);
	vfprintf(msg_out, fmt, ap);
	va_end(ap);
	Printf(": %s\n", strerror(eo));
	fflush(msg_out);
}

__attribute__((format(printf, 1, 2)))
static void err_printf(const char *fmt, ...)
{
	va_list ap;

	Printf(NERR_PREFIX);
	va_start(ap, fmt);
	vfprintf(msg_out, fmt, ap);
	va_end(ap);
	fflush(msg_out);
}

__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
static int err_exit(const char *fmt, ...)
{
	va_list ap;

	Printf(NERR_PREFIX);
	va_start(ap, fmt);
	vfprintf(msg_out, fmt, ap);
	va_end(ap);
	fflush(msg_out);
	exit(1);
}

__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
static int perr_exit(const char *fmt, ...)
{
	va_list ap;
	int eo = errno;

	Printf(PERR_PREFIX, eo);
	va_start(ap, fmt);
	vfprintf(msg_out, fmt, ap);
	va_end(ap);
	Printf(": %s\n", strerror(eo));
	fflush(msg_out);
	exit(1);
}


__attribute__((noreturn))
static void usage(void)
{
	fprintf(stderr, "\nUsage: %s [OPTIONS] SOURCE\n"
		"    Efficiently clone NTFS to a sparse file, image, device or standard output.\n"
		"\n"
		"    -o, --output FILE      Clone NTFS to the non-existent FILE\n"
		"    -O, --overwrite FILE   Clone NTFS to FILE, overwriting if exists\n"
		"    -s, --save-image       Save to the special image format\n"
		"    -r, --restore-image    Restore from the special image format\n"
		"        --rescue           Continue after disk read errors\n"
		"    -m, --metadata         Clone *only* metadata (for NTFS experts)\n"
		"        --ignore-fs-check  Ignore the filesystem check result\n"
		"    -f, --force            Force to progress (DANGEROUS)\n"
		"    -h, --help             Display this help\n"
#ifdef DEBUG
		"    -d, --debug            Show debug information\n"
#endif
		"\n"
		"    If FILE is '-' then send the image to the standard output. If SOURCE is '-'\n"
		"    and --restore-image is used then read the image from the standard input.\n"
		"\n", EXEC_NAME);
	fprintf(stderr, "%s%s", ntfs_bugs, ntfs_home);
	exit(1);
}


static void parse_options(int argc, char **argv)
{
	static const char *sopt = "-dfhmo:O:rs";
	static const struct option lopt[] = {
#ifdef DEBUG
		{ "debug",	      no_argument,	 NULL, 'd' },
#endif
		{ "force",	      no_argument,	 NULL, 'f' },
		{ "help",	      no_argument,	 NULL, 'h' },
		{ "metadata",	      no_argument,	 NULL, 'm' },
		{ "output",	      required_argument, NULL, 'o' },
		{ "overwrite",	      required_argument, NULL, 'O' },
		{ "restore-image",    no_argument,	 NULL, 'r' },
		{ "ignore-fs-check",  no_argument,	 NULL, 'C' },
		{ "rescue",           no_argument,	 NULL, 'R' },
		{ "save-image",	      no_argument,	 NULL, 's' },
		{ NULL, 0, NULL, 0 }
	};

	int c;

	memset(&opt, 0, sizeof(opt));

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (opt.volume)
				usage();
			opt.volume = argv[optind-1];
			break;
		case 'd':
			opt.debug++;
			break;
		case 'f':
			opt.force++;
			break;
		case 'h':
		case '?':
			usage();
		case 'm':
			opt.metadata++;
			break;
		case 'O':
			opt.overwrite++;
		case 'o':
			if (opt.output)
				usage();
			opt.output = optarg;
			break;
		case 'r':
			opt.restore_image++;
			break;
		case 'C':
			opt.ignore_fs_check++;
			break;
		case 'R':
			opt.rescue++;
			break;
		case 's':
			opt.save_image++;
			break;
		default:
			err_printf("Unknown option '%s'.\n", argv[optind-1]);
			usage();
		}
	}

	if (opt.output == NULL) {
		err_printf("You must specify an output file.\n");
		usage();
	}

	if (strcmp(opt.output, "-") == 0)
		opt.std_out++;

	if (opt.volume == NULL) {
		err_printf("You must specify a device file.\n");
		usage();
	}

	if (opt.metadata && opt.save_image)
		err_exit("Saving only metadata to an image is not "
			 "supported!\n");

	if (opt.metadata && opt.restore_image)
		err_exit("Restoring only metadata from an image is not "
			 "supported!\n");

	if (opt.metadata && opt.std_out)
		err_exit("Cloning only metadata to stdout isn't supported!\n");

	if (opt.ignore_fs_check && !opt.metadata)
		err_exit("Filesystem check can be ignored only for metadata "
			 "cloning!\n");

	if (opt.save_image && opt.restore_image)
		err_exit("Saving and restoring an image at the same time "
			 "is not supported!\n");

	if (!opt.std_out) {
		struct stat st;

		if (stat(opt.output, &st) == -1) {
			if (errno != ENOENT)
				perr_exit("Couldn't access '%s'", opt.output);
		} else {
			if (!opt.overwrite)
				err_exit("Output file '%s' already exists.\n"
					 "Use option --overwrite if you want to"
					 " replace its content.\n", opt.output);

			if (S_ISBLK(st.st_mode)) {
				opt.blkdev_out = 1;
				if (opt.metadata)
					err_exit("Cloning only metadata to a "
					     "block device isn't supported!\n");
			}
		}
	}

	msg_out = stdout;

	/* FIXME: this is a workaround for losing debug info if stdout != stderr
	   and for the uncontrollable verbose messages in libntfs. Ughhh. */
	if (opt.std_out)
		msg_out = stderr;
	else if (opt.debug) {
		/* Redirect stderr to stdout, note fflush()es are essential! */
		fflush(stdout);
		fflush(stderr);
		if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
			perror("Failed to redirect stderr to stdout");
			exit(1);
		}
		fflush(stdout);
		fflush(stderr);
	} else {
		fflush(stderr);
		if (!freopen("/dev/null", "w", stderr))
			perr_exit("Failed to redirect stderr to /dev/null");
	}
}

static void progress_init(struct progress_bar *p, u64 start, u64 stop, int res)
{
	p->start = start;
	p->stop = stop;
	p->unit = 100.0 / (stop - start);
	p->resolution = res;
}


static void progress_update(struct progress_bar *p, u64 current)
{
	float percent = p->unit * current;

	if (current != p->stop) {
		if ((current - p->start) % p->resolution)
			return;
		Printf("%6.2f percent completed\r", percent);
	} else
		Printf("100.00 percent completed\n");
	fflush(msg_out);
}

static s64 is_critical_metadata(ntfs_walk_clusters_ctx *image, runlist *rl)
{
	s64 inode = image->ni->mft_no;

	if (inode <= LAST_METADATA_INODE) {

		/* Don't save bad sectors (both $Bad and unnamed are ignored */
		if (inode == FILE_BadClus && image->ctx->attr->type == AT_DATA)
			return 0;

		if (inode != FILE_LogFile)
			return rl->length;

		if (image->ctx->attr->type == AT_DATA) {

			/* Save at least the first 16 KiB of FILE_LogFile */
			s64 s = (s64)16384 - rl->vcn * vol->cluster_size;
			if (s > 0) {
				s = rounded_up_division(s, vol->cluster_size);
				if (rl->length < s)
					s = rl->length;
				return s;
			}
			return 0;
		}
	}

	if (image->ctx->attr->type != AT_DATA)
		return rl->length;

	return 0;
}


static int io_all(void *fd, void *buf, int count, int do_write)
{
	int i;
	struct ntfs_device *dev = fd;

	while (count > 0) {
		if (do_write)
			i = write(*(int *)fd, buf, count);
		else if (opt.restore_image)
			i = read(*(int *)fd, buf, count);
		else
			i = dev->d_ops->read(dev, buf, count);
		if (i < 0) {
			if (errno != EAGAIN && errno != EINTR)
				return -1;
		} else {
			count -= i;
			buf = i + (char *) buf;
		}
	}
	return 0;
}


static void rescue_sector(void *fd, off_t pos, void *buff)
{
	const char *badsector_magic = "BadSectoR\0";
	struct ntfs_device *dev = fd;

	if (opt.restore_image) {
		if (lseek(*(int *)fd, pos, SEEK_SET) == (off_t)-1)
			perr_exit("lseek");
	} else {
		if (vol->dev->d_ops->seek(dev, pos, SEEK_SET) == (off_t)-1)
			perr_exit("seek input");
	}

	if (read_all(fd, buff, NTFS_SECTOR_SIZE) == -1) {
		Printf("WARNING: Can't read sector at %llu, lost data.\n",
			(unsigned long long)pos);
		memset(buff, '?', NTFS_SECTOR_SIZE);
		memmove(buff, badsector_magic, sizeof(badsector_magic));
	}
}


static void copy_cluster(int rescue, u64 rescue_lcn)
{
	char buff[NTFS_MAX_CLUSTER_SIZE]; /* overflow checked at mount time */
	/* vol is NULL if opt.restore_image is set */
	u32 csize = le32_to_cpu(image_hdr.cluster_size);
	void *fd = (void *)&fd_in;
	off_t rescue_pos;

	if (!opt.restore_image) {
		csize = vol->cluster_size;
		fd = vol->dev;
	}

	rescue_pos = (off_t)(rescue_lcn * csize);

	if (read_all(fd, buff, csize) == -1) {

		if (errno != EIO)
			perr_exit("read_all");
		else if (rescue){
			u32 i;
			for (i = 0; i < csize; i += NTFS_SECTOR_SIZE)
				rescue_sector(fd, rescue_pos + i, buff + i);
		} else {
			Printf("%s", bad_sectors_warning_msg);
			err_exit("Disk is faulty, can't make full backup!");
		}
	}

	if (opt.save_image) {
		char cmd = 1;
		if (write_all(&fd_out, &cmd, sizeof(cmd)) == -1)
			perr_exit("write_all");
	}

	if (write_all(&fd_out, buff, csize) == -1) {
		int err = errno;
		perr_printf("Write failed");
		if (err == EIO && opt.stfs.f_type == 0x517b)
			Printf("Apparently you tried to clone to a remote "
			       "Windows computer but they don't\nhave "
			       "efficient sparse file handling by default. "
			       "Please try a different method.\n");
		exit(1);
	}
}

static void lseek_to_cluster(s64 lcn)
{
	off_t pos;

	pos = (off_t)(lcn * vol->cluster_size);

	if (vol->dev->d_ops->seek(vol->dev, pos, SEEK_SET) == (off_t)-1)
		perr_exit("lseek input");

	if (opt.std_out || opt.save_image)
		return;

	if (lseek(fd_out, pos, SEEK_SET) == (off_t)-1)
		perr_exit("lseek output");
}

static void image_skip_clusters(s64 count)
{
	if (opt.save_image && count > 0) {
		typeof(count) count_buf;
		char buff[1 + sizeof(count)];

		buff[0] = 0;
		count_buf = cpu_to_sle64(count);
		memcpy(buff + 1, &count_buf, sizeof(count_buf));

		if (write_all(&fd_out, buff, sizeof(buff)) == -1)
			perr_exit("write_all");
	}
}

static void dump_clusters(ntfs_walk_clusters_ctx *image, runlist *rl)
{
	s64 i, len; /* number of clusters to copy */

	if (opt.std_out || !opt.metadata)
		return;

	if (!(len = is_critical_metadata(image, rl)))
		return;

	lseek_to_cluster(rl->lcn);

	/* FIXME: this could give pretty suboptimal performance */
	for (i = 0; i < len; i++)
		copy_cluster(opt.rescue, rl->lcn + i);
}

static void clone_ntfs(u64 nr_clusters)
{
	u64 cl, last_cl;  /* current and last used cluster */
	void *buf;
	u32 csize = vol->cluster_size;
	u64 p_counter = 0;
	struct progress_bar progress;

	if (opt.save_image)
		Printf("Saving NTFS to image ...\n");
	else
		Printf("Cloning NTFS ...\n");

	buf = ntfs_calloc(csize);
	if (!buf)
		perr_exit("clone_ntfs");

	progress_init(&progress, p_counter, nr_clusters, 100);

	if (opt.save_image) {
		if (write_all(&fd_out, &image_hdr,
				image_hdr.offset_to_image_data) == -1)
			perr_exit("write_all");
	}

	for (last_cl = cl = 0; cl < (u64)vol->nr_clusters; cl++) {

		if (ntfs_bit_get(lcn_bitmap.bm, cl)) {
			progress_update(&progress, ++p_counter);
			lseek_to_cluster(cl);
			image_skip_clusters(cl - last_cl - 1);

			copy_cluster(opt.rescue, cl);
			last_cl = cl;
			continue;
		}

		if (opt.std_out && !opt.save_image) {
			progress_update(&progress, ++p_counter);
			if (write_all(&fd_out, buf, csize) == -1)
				perr_exit("write_all");
		}
	}
	image_skip_clusters(cl - last_cl - 1);
}

static void write_empty_clusters(s32 csize, s64 count,
				 struct progress_bar *progress, u64 *p_counter)
{
	s64 i;
	char buff[NTFS_MAX_CLUSTER_SIZE];

	memset(buff, 0, csize);

	for (i = 0; i < count; i++) {
		if (write_all(&fd_out, buff, csize) == -1)
			perr_exit("write_all");
		progress_update(progress, ++(*p_counter));
	}
}

static void restore_image(void)
{
	s64 pos = 0, count;
	s32 csize = le32_to_cpu(image_hdr.cluster_size);
	char cmd;
	u64 p_counter = 0;
	struct progress_bar progress;

	Printf("Restoring NTFS from image ...\n");

	progress_init(&progress, p_counter, opt.std_out ?
		      sle64_to_cpu(image_hdr.nr_clusters) :
		      sle64_to_cpu(image_hdr.inuse),
		      100);

	while (pos < sle64_to_cpu(image_hdr.nr_clusters)) {
		if (read_all(&fd_in, &cmd, sizeof(cmd)) == -1)
			perr_exit("read_all");

		if (cmd == 0) {
			if (read_all(&fd_in, &count, sizeof(count)) == -1)
				perr_exit("read_all");
			if (!image_is_host_endian)
				count = sle64_to_cpu(count);
			if (opt.std_out)
				write_empty_clusters(csize, count,
						     &progress, &p_counter);
			else {
				if (lseek(fd_out, count * csize, SEEK_CUR) ==
						(off_t)-1)
					perr_exit("restore_image: lseek");
			}
			pos += count;
		} else if (cmd == 1) {
			copy_cluster(0, 0);
			pos++;
			progress_update(&progress, ++p_counter);
		} else
			err_exit("Invalid command code in image\n");
	}
}

static void wipe_index_entry_timestams(INDEX_ENTRY *e)
{
	s64 timestamp = utc2ntfs(0);

	/* FIXME: can fall into infinite loop if corrupted */
	while (!(e->flags & INDEX_ENTRY_END)) {

		e->key.file_name.creation_time = timestamp;
		e->key.file_name.last_data_change_time = timestamp;
		e->key.file_name.last_mft_change_time = timestamp;
		e->key.file_name.last_access_time = timestamp;

		wiped_timestamp_data += 32;

		e = (INDEX_ENTRY *)((u8 *)e + le16_to_cpu(e->length));
	}
}

static void wipe_index_allocation_timestamps(ntfs_inode *ni, ATTR_RECORD *attr)
{
	INDEX_ALLOCATION *indexa, *tmp_indexa;
	INDEX_ENTRY *entry;
	INDEX_ROOT *indexr;
	u8 *bitmap, *byte;
	int bit;
	ntfs_attr *na;
	ntfschar *name;
	u32 name_len;

	indexr = ntfs_index_root_get(ni, attr);
	if (!indexr) {
		perr_printf("Failed to read $INDEX_ROOT attribute of inode "
			    "%lld", ni->mft_no);
		return;
	}

	if (indexr->type != AT_FILE_NAME)
		goto out_indexr;

	name = (ntfschar *)((u8 *)attr + le16_to_cpu(attr->name_offset));
	name_len = attr->name_length;

	byte = bitmap = ntfs_attr_readall(ni, AT_BITMAP, name, name_len, NULL);
	if (!byte) {
		perr_printf("Failed to read $BITMAP attribute");
		goto out_indexr;
	}

	na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, name, name_len);
	if (!na) {
		perr_printf("Failed to open $INDEX_ALLOCATION attribute");
		goto out_bitmap;
	}

	if (!na->data_size)
		goto out_na;

	tmp_indexa = indexa = ntfs_malloc(na->data_size);
	if (!tmp_indexa)
		goto out_na;

	if (ntfs_attr_pread(na, 0, na->data_size, indexa) != na->data_size) {
		perr_printf("Failed to read $INDEX_ALLOCATION attribute");
		goto out_indexa;
	}

	bit = 0;
	while ((u8 *)tmp_indexa < (u8 *)indexa + na->data_size) {
		if (*byte & (1 << bit)) {
			if (ntfs_mst_post_read_fixup((NTFS_RECORD *)tmp_indexa,
					le32_to_cpu(
					indexr->index_block_size))) {
				perr_printf("Damaged INDX record");
				goto out_indexa;
			}
			entry = (INDEX_ENTRY *)((u8 *)tmp_indexa + le32_to_cpu(
				tmp_indexa->index.entries_offset) + 0x18);

			wipe_index_entry_timestams(entry);

			if (ntfs_mft_usn_dec((MFT_RECORD *)tmp_indexa))
				perr_exit("ntfs_mft_usn_dec");

			if (ntfs_mst_pre_write_fixup((NTFS_RECORD *)tmp_indexa,
					le32_to_cpu(
					indexr->index_block_size))) {
				perr_printf("INDX write fixup failed");
				goto out_indexa;
			}
		}
		tmp_indexa = (INDEX_ALLOCATION *)((u8 *)tmp_indexa +
				le32_to_cpu(indexr->index_block_size));
		bit++;
		if (bit > 7) {
			bit = 0;
			byte++;
		}
	}

	if (ntfs_rl_pwrite(vol, na->rl, 0, na->data_size, indexa) != na->data_size)
		perr_printf("ntfs_rl_pwrite failed for inode %lld", ni->mft_no);
out_indexa:
	free(indexa);
out_na:
	ntfs_attr_close(na);
out_bitmap:
	free(bitmap);
out_indexr:
	free(indexr);
}

static void wipe_index_root_timestamps(ATTR_RECORD *attr, s64 timestamp)
{
	INDEX_ENTRY *entry;
	INDEX_ROOT *iroot;

	iroot = (INDEX_ROOT *)((u8 *)attr + le16_to_cpu(attr->value_offset));
	entry = (INDEX_ENTRY *)((u8 *)iroot +
			le32_to_cpu(iroot->index.entries_offset) + 0x10);

	while (!(entry->flags & INDEX_ENTRY_END)) {

		if (iroot->type == AT_FILE_NAME) {

			entry->key.file_name.creation_time = timestamp;
			entry->key.file_name.last_access_time = timestamp;
			entry->key.file_name.last_data_change_time = timestamp;
			entry->key.file_name.last_mft_change_time = timestamp;

			wiped_timestamp_data += 32;

		} else if (ntfs_names_are_equal(NTFS_INDEX_Q,
				sizeof(NTFS_INDEX_Q) / 2 - 1,
				(ntfschar *)((char *)attr +
					    le16_to_cpu(attr->name_offset)),
				attr->name_length, 0, NULL, 0)) {

			QUOTA_CONTROL_ENTRY *quota_q;

			quota_q = (QUOTA_CONTROL_ENTRY *)((u8 *)entry +
					le16_to_cpu(entry->data_offset));
			/*
			 *  FIXME: no guarantee it's indeed /$Extend/$Quota:$Q.
			 *  For now, as a minimal safeguard, we check only for
			 *  quota version 2 ...
			 */
			if (le32_to_cpu(quota_q->version) == 2) {
				quota_q->change_time = timestamp;
				wiped_timestamp_data += 4;
			}
		}

		entry = (INDEX_ENTRY*)((u8*)entry + le16_to_cpu(entry->length));
	}
}

#define WIPE_TIMESTAMPS(atype, attr, timestamp)			\
do {								\
	atype *ats;						\
	ats = (atype *)((char *)(attr) + le16_to_cpu((attr)->value_offset)); \
								\
	ats->creation_time = (timestamp);	       		\
	ats->last_data_change_time = (timestamp);		\
	ats->last_mft_change_time= (timestamp);			\
	ats->last_access_time = (timestamp);			\
								\
	wiped_timestamp_data += 32;				\
								\
} while (0)

static void wipe_timestamps(ntfs_walk_clusters_ctx *image)
{
	ATTR_RECORD *a = image->ctx->attr;
	s64 timestamp = utc2ntfs(0);

	if (a->type == AT_FILE_NAME)
		WIPE_TIMESTAMPS(FILE_NAME_ATTR, a, timestamp);

	else if (a->type == AT_STANDARD_INFORMATION)
		WIPE_TIMESTAMPS(STANDARD_INFORMATION, a, timestamp);

	else if (a->type == AT_INDEX_ROOT)
		wipe_index_root_timestamps(a, timestamp);
}

static void wipe_resident_data(ntfs_walk_clusters_ctx *image)
{
	ATTR_RECORD *a;
	u32 i;
	int n = 0;
	u8 *p;

	a = image->ctx->attr;
	p = (u8*)a + le16_to_cpu(a->value_offset);

	if (image->ni->mft_no <= LAST_METADATA_INODE)
		return;

	if (a->type != AT_DATA)
		return;

	for (i = 0; i < le32_to_cpu(a->value_length); i++) {
		if (p[i]) {
			p[i] = 0;
			n++;
		}
	}

	wiped_resident_data += n;
}

static void clone_logfile_parts(ntfs_walk_clusters_ctx *image, runlist *rl)
{
	s64 offset = 0, lcn, vcn;

	while (1) {

		vcn = offset / image->ni->vol->cluster_size;
		lcn = ntfs_rl_vcn_to_lcn(rl, vcn);
		if (lcn < 0)
			break;

		lseek_to_cluster(lcn);
		copy_cluster(opt.rescue, lcn);

		if (offset == 0)
			offset = NTFS_BLOCK_SIZE >> 1;
		else
			offset <<= 1;
	}
}

static void walk_runs(struct ntfs_walk_cluster *walk)
{
	int i, j;
	runlist *rl;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;

	ctx = walk->image->ctx;
	a = ctx->attr;

	if (!a->non_resident) {
		if (wipe) {
			wipe_resident_data(walk->image);
			wipe_timestamps(walk->image);
		}
		return;
	}

	if (wipe && walk->image->ctx->attr->type == AT_INDEX_ALLOCATION)
		wipe_index_allocation_timestamps(walk->image->ni, a);

	if (!(rl = ntfs_mapping_pairs_decompress(vol, a, NULL)))
		perr_exit("ntfs_decompress_mapping_pairs");

	for (i = 0; rl[i].length; i++) {
		s64 lcn = rl[i].lcn;
		s64 lcn_length = rl[i].length;

		if (lcn == LCN_HOLE || lcn == LCN_RL_NOT_MAPPED)
			continue;

		/* FIXME: ntfs_mapping_pairs_decompress should return error */
		if (lcn < 0 || lcn_length < 0)
			err_exit("Corrupt runlist in inode %lld attr %x LCN "
				 "%llx length %llx\n", ctx->ntfs_ino->mft_no,
				 (unsigned int)le32_to_cpu(a->type), lcn,
				 lcn_length);

		if (!wipe)
			dump_clusters(walk->image, rl + i);

		for (j = 0; j < lcn_length; j++) {
			u64 k = (u64)lcn + j;
			if (ntfs_bit_get_and_set(lcn_bitmap.bm, k, 1))
				err_exit("Cluster %llu referenced twice!\n"
					 "You didn't shutdown your Windows"
					 "properly?\n", (unsigned long long)k);
		}

		walk->image->inuse += lcn_length;
	}
	if (!wipe && !opt.std_out && opt.metadata &&
	    walk->image->ni->mft_no == FILE_LogFile &&
	    walk->image->ctx->attr->type == AT_DATA)
		clone_logfile_parts(walk->image, rl);

	free(rl);
}


static void walk_attributes(struct ntfs_walk_cluster *walk)
{
	ntfs_attr_search_ctx *ctx;

	if (!(ctx = ntfs_attr_get_search_ctx(walk->image->ni, NULL)))
		perr_exit("ntfs_get_attr_search_ctx");

	while (!ntfs_attrs_walk(ctx)) {
		if (ctx->attr->type == AT_END)
			break;

		walk->image->ctx = ctx;
		walk_runs(walk);
	}

	ntfs_attr_put_search_ctx(ctx);
}



static void compare_bitmaps(struct bitmap *a)
{
	s64 i, pos, count;
	int mismatch = 0;
	u8 bm[NTFS_BUF_SIZE];

	Printf("Accounting clusters ...\n");

	pos = 0;
	while (1) {
		count = ntfs_attr_pread(vol->lcnbmp_na, pos, NTFS_BUF_SIZE, bm);
		if (count == -1)
			perr_exit("Couldn't get $Bitmap $DATA");

		if (count == 0) {
			if (a->size > pos)
				err_exit("$Bitmap size is smaller than expected"
					 " (%lld != %lld)\n", a->size, pos);
			break;
		}

		for (i = 0; i < count; i++, pos++) {
			s64 cl;  /* current cluster */

			if (a->size <= pos)
				goto done;

			if (a->bm[pos] == bm[i])
				continue;

			for (cl = pos * 8; cl < (pos + 1) * 8; cl++) {
				char bit;

				bit = ntfs_bit_get(a->bm, cl);
				if (bit == ntfs_bit_get(bm, i * 8 + cl % 8))
					continue;

				if (opt.ignore_fs_check) {
					lseek_to_cluster(cl);
					copy_cluster(opt.rescue, cl);
				}

				if (++mismatch > 10)
					continue;

				Printf("Cluster accounting failed at %lld "
				       "(0x%llx): %s cluster in $Bitmap\n",
				       (long long)cl, (unsigned long long)cl,
				       bit ? "missing" : "extra");
			}
		}
	}
done:
	if (mismatch) {
		Printf("Totally %d cluster accounting mismatches.\n", mismatch);
		if (opt.ignore_fs_check) {
			Printf("WARNING: The NTFS inconsistency was overruled "
			       "by the --ignore-fs-check option.\n");
			return;
		}
		err_exit("Filesystem check failed! Windows wasn't shutdown "
			 "properly or inconsistent\nfilesystem. Please run "
			 "chkdsk /f on Windows then reboot it TWICE.\n");
	}
}


static int wipe_data(char *p, int pos, int len)
{
	int wiped = 0;

	for (p += pos; --len >= 0;) {
		if (p[len]) {
			p[len] = 0;
			wiped++;
		}
	}

	return wiped;
}

static void wipe_unused_mft_data(ntfs_inode *ni)
{
	int unused;
	MFT_RECORD *m = ni->mrec;

	/* FIXME: broken MFTMirr update was fixed in libntfs, check if OK now */
	if (ni->mft_no <= LAST_METADATA_INODE)
		return;

	unused = le32_to_cpu(m->bytes_allocated) - le32_to_cpu(m->bytes_in_use);
	wiped_unused_mft_data += wipe_data((char *)m,
			le32_to_cpu(m->bytes_in_use), unused);
}

static void wipe_unused_mft(ntfs_inode *ni)
{
	int unused;
	MFT_RECORD *m = ni->mrec;

	/* FIXME: broken MFTMirr update was fixed in libntfs, check if OK now */
	if (ni->mft_no <= LAST_METADATA_INODE)
		return;

	unused = le32_to_cpu(m->bytes_in_use) - sizeof(MFT_RECORD);
	wiped_unused_mft += wipe_data((char *)m, sizeof(MFT_RECORD), unused);
}

static void mft_record_write_with_same_usn(ntfs_volume *volume, ntfs_inode *ni)
{
	if (ntfs_mft_usn_dec(ni->mrec))
		perr_exit("ntfs_mft_usn_dec");

	if (ntfs_mft_record_write(volume, ni->mft_no, ni->mrec))
		perr_exit("ntfs_mft_record_write");
}

static void mft_inode_write_with_same_usn(ntfs_volume *volume, ntfs_inode *ni)
{
	s32 i;

	mft_record_write_with_same_usn(volume, ni);

	if (ni->nr_extents <= 0)
		return;

	for (i = 0; i < ni->nr_extents; ++i) {
		ntfs_inode *eni = ni->extent_nis[i];
		mft_record_write_with_same_usn(volume, eni);
	}
}

static int walk_clusters(ntfs_volume *volume, struct ntfs_walk_cluster *walk)
{
	s64 inode = 0;
	s64 last_mft_rec;
	ntfs_inode *ni;
	struct progress_bar progress;

	Printf("Scanning volume ...\n");

	last_mft_rec = (volume->mft_na->initialized_size >>
			volume->mft_record_size_bits) - 1;
	progress_init(&progress, inode, last_mft_rec, 100);

	for (; inode <= last_mft_rec; inode++) {

		int err, deleted_inode;
		MFT_REF mref = (MFT_REF)inode;

		progress_update(&progress, inode);

		/* FIXME: Terrible kludge for libntfs not being able to return
		   a deleted MFT record as inode */
		ni = ntfs_calloc(sizeof(ntfs_inode));
		if (!ni)
			perr_exit("walk_clusters");

		ni->vol = volume;

		err = ntfs_file_record_read(volume, mref, &ni->mrec, NULL);
		if (err == -1) {
			free(ni);
			continue;
		}

		deleted_inode = !(ni->mrec->flags & MFT_RECORD_IN_USE);

		if (deleted_inode) {

			ni->mft_no = MREF(mref);
			if (wipe) {
				wipe_unused_mft(ni);
				wipe_unused_mft_data(ni);
				mft_record_write_with_same_usn(volume, ni);
			}
		}

		free(ni->mrec);
		free(ni);

		if (deleted_inode)
			continue;

		if ((ni = ntfs_inode_open(volume, mref)) == NULL) {
			/* FIXME: continue only if it make sense, e.g.
			   MFT record not in use based on $MFT bitmap */
			if (errno == EIO || errno == ENOENT)
				continue;
			perr_exit("Reading inode %lld failed", inode);
		}

		if (wipe)
			nr_used_mft_records++;

		if (ni->mrec->base_mft_record)
			goto out;

		walk->image->ni = ni;
		walk_attributes(walk);
out:
		if (wipe) {
			wipe_unused_mft_data(ni);
			mft_inode_write_with_same_usn(volume, ni);
		}

		if (ntfs_inode_close(ni))
			perr_exit("ntfs_inode_close for inode %lld", inode);
	}

	return 0;
}


/*
 * $Bitmap can overlap the end of the volume. Any bits in this region
 * must be set. This region also encompasses the backup boot sector.
 */
static void bitmap_file_data_fixup(s64 cluster, struct bitmap *bm)
{
	for (; cluster < bm->size << 3; cluster++)
		ntfs_bit_set(bm->bm, (u64)cluster, 1);
}


/*
 * Allocate a block of memory with one bit for each cluster of the disk.
 * All the bits are set to 0, except those representing the region beyond the
 * end of the disk.
 */
static void setup_lcn_bitmap(void)
{
	/* Determine lcn bitmap byte size and allocate it. */
	lcn_bitmap.size = rounded_up_division(vol->nr_clusters, 8);

	lcn_bitmap.bm = ntfs_calloc(lcn_bitmap.size);
	if (!lcn_bitmap.bm)
		perr_exit("Failed to allocate internal buffer");

	bitmap_file_data_fixup(vol->nr_clusters, &lcn_bitmap);
}


static s64 volume_size(ntfs_volume *volume, s64 nr_clusters)
{
	return nr_clusters * volume->cluster_size;
}


static void print_volume_size(const char *str, s64 bytes)
{
	Printf("%s: %lld bytes (%lld MB)\n", str, (long long)bytes,
			(long long)rounded_up_division(bytes, NTFS_MBYTE));
}


static void print_disk_usage(const char *spacer, u32 cluster_size,
		s64 nr_clusters, s64 inuse)
{
	s64 total, used;

	total = nr_clusters * cluster_size;
	used = inuse * cluster_size;

	Printf("Space in use       %s: %lld MB (%.1f%%)   ", spacer,
			(long long)rounded_up_division(used, NTFS_MBYTE),
			100.0 * ((float)used / total));

	Printf("\n");
}

static void print_image_info(void)
{
	Printf("Ntfsclone image version: %d.%d\n",
			image_hdr.major_ver, image_hdr.minor_ver);
	Printf("Cluster size           : %u bytes\n",
			(unsigned)le32_to_cpu(image_hdr.cluster_size));
	print_volume_size("Image volume size      ",
			sle64_to_cpu(image_hdr.nr_clusters) *
			le32_to_cpu(image_hdr.cluster_size));
	Printf("Image device size      : %lld bytes\n",
			sle64_to_cpu(image_hdr.device_size));
	print_disk_usage("    ", le32_to_cpu(image_hdr.cluster_size),
			sle64_to_cpu(image_hdr.nr_clusters),
			sle64_to_cpu(image_hdr.inuse));
	Printf("Offset to image data   : %u (0x%x) bytes\n",
			(unsigned)le32_to_cpu(image_hdr.offset_to_image_data),
			(unsigned)le32_to_cpu(image_hdr.offset_to_image_data));
}

static void check_if_mounted(const char *device, unsigned long new_mntflag)
{
	unsigned long mntflag;

	if (ntfs_check_if_mounted(device, &mntflag))
		perr_exit("Failed to check '%s' mount state", device);

	if (mntflag & NTFS_MF_MOUNTED) {
		if (!(mntflag & NTFS_MF_READONLY))
			err_exit("Device '%s' is mounted read-write. "
				 "You must 'umount' it first.\n", device);
		if (!new_mntflag)
			err_exit("Device '%s' is mounted. "
				 "You must 'umount' it first.\n", device);
	}
}

/**
 * mount_volume -
 *
 * First perform some checks to determine if the volume is already mounted, or
 * is dirty (Windows wasn't shutdown properly).  If everything is OK, then mount
 * the volume (load the metadata into memory).
 */
static void mount_volume(unsigned long new_mntflag)
{
	check_if_mounted(opt.volume, new_mntflag);

	if (!(vol = ntfs_mount(opt.volume, new_mntflag))) {

		int err = errno;

		perr_printf("Opening '%s' as NTFS failed", opt.volume);
		if (err == EINVAL) {
			Printf("Apparently device '%s' doesn't have a "
			       "valid NTFS. Maybe you selected\nthe whole "
			       "disk instead of a partition (e.g. /dev/hda, "
			       "not /dev/hda1)?\n", opt.volume);
		}
		exit(1);
	}

	if (NVolWasDirty(vol))
		if (opt.force-- <= 0)
			err_exit(dirty_volume_msg, opt.volume);

	if (NTFS_MAX_CLUSTER_SIZE < vol->cluster_size)
		err_exit("Cluster size %u is too large!\n",
				(unsigned int)vol->cluster_size);

	Printf("NTFS volume version: %d.%d\n", vol->major_ver, vol->minor_ver);
	if (ntfs_version_is_supported(vol))
		perr_exit("Unknown NTFS version");

	Printf("Cluster size       : %u bytes\n",
			(unsigned int)vol->cluster_size);
	print_volume_size("Current volume size",
			  volume_size(vol, vol->nr_clusters));
}

static struct ntfs_walk_cluster backup_clusters = { NULL, NULL };

static int device_offset_valid(int fd, s64 ofs)
{
	char ch;

	if (lseek(fd, ofs, SEEK_SET) >= 0 && read(fd, &ch, 1) == 1)
		return 0;
	return -1;
}

static s64 device_size_get(int fd)
{
	s64 high, low;
#ifdef BLKGETSIZE64
	{	u64 size;

		if (ioctl(fd, BLKGETSIZE64, &size) >= 0) {
			ntfs_log_debug("BLKGETSIZE64 nr bytes = %llu "
				"(0x%llx).\n", (unsigned long long)size,
				(unsigned long long)size);
			return (s64)size;
		}
	}
#endif
#ifdef BLKGETSIZE
	{	unsigned long size;

		if (ioctl(fd, BLKGETSIZE, &size) >= 0) {
			ntfs_log_debug("BLKGETSIZE nr 512 byte blocks = %lu "
				"(0x%lx).\n", size, size);
			return (s64)size * 512;
		}
	}
#endif
#ifdef FDGETPRM
	{       struct floppy_struct this_floppy;

		if (ioctl(fd, FDGETPRM, &this_floppy) >= 0) {
			ntfs_log_debug("FDGETPRM nr 512 byte blocks = %lu "
				"(0x%lx).\n", this_floppy.size,
				this_floppy.size);
			return (s64)this_floppy.size * 512;
		}
	}
#endif
	/*
	 * We couldn't figure it out by using a specialized ioctl,
	 * so do binary search to find the size of the device.
	 */
	low = 0LL;
	for (high = 1024LL; !device_offset_valid(fd, high); high <<= 1)
		low = high;
	while (low < high - 1LL) {
		const s64 mid = (low + high) / 2;

		if (!device_offset_valid(fd, mid))
			low = mid;
		else
			high = mid;
	}
	lseek(fd, 0LL, SEEK_SET);
	return (low + 1LL);
}

static void fsync_clone(int fd)
{
	Printf("Syncing ...\n");
	if (fsync(fd) && errno != EINVAL)
		perr_exit("fsync");
}

static void set_filesize(s64 filesize)
{
	long fs_type = 0; /* Unknown filesystem type */

	if (fstatfs(fd_out, &opt.stfs) == -1)
		Printf("WARNING: Couldn't get filesystem type: "
		       "%s\n", strerror(errno));
	else
		fs_type = opt.stfs.f_type;

	if (fs_type == 0x52654973)
		Printf("WARNING: You're using ReiserFS, it has very poor "
		       "performance creating\nlarge sparse files. The next "
		       "operation might take a very long time!\n"
		       "Creating sparse output file ...\n");
	else if (fs_type == 0x517b)
		Printf("WARNING: You're using SMBFS and if the remote share "
		       "isn't Samba but a Windows\ncomputer then the clone "
		       "operation will be very inefficient and may fail!\n");

	if (ftruncate(fd_out, filesize) == -1) {
		int err = errno;
		perr_printf("ftruncate failed for file '%s'", opt.output);
		if (fs_type)
			Printf("Destination filesystem type is 0x%lx.\n",
			       (unsigned long)fs_type);
		if (err == E2BIG) {
			Printf("Your system or the destination filesystem "
			       "doesn't support large files.\n");
			if (fs_type == 0x517b) {
				Printf("SMBFS needs minimum Linux kernel "
				       "version 2.4.25 and\n the 'lfs' option"
				       "\nfor smbmount to have large "
				       "file support.\n");
			}
		} else if (err == EPERM) {
			Printf("Apparently the destination filesystem doesn't "
			       "support sparse files.\nYou can overcome this "
			       "by using the more efficient --save-image "
			       "option\nof ntfsclone. Use the --restore-image "
			       "option to restore the image.\n");
		}
		exit(1);
	}
}

static s64 open_image(void)
{
	if (strcmp(opt.volume, "-") == 0) {
		if ((fd_in = fileno(stdin)) == -1)
			perr_exit("fileno for stdout failed");
	} else {
		if ((fd_in = open(opt.volume, O_RDONLY)) == -1)
			perr_exit("failed to open image");
	}
	if (read_all(&fd_in, &image_hdr, NTFSCLONE_IMG_HEADER_SIZE_OLD) == -1)
		perr_exit("read_all");
	if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0)
		err_exit("Input file is not an image! (invalid magic)\n");
	if (image_hdr.major_ver < NTFSCLONE_IMG_VER_MAJOR_ENDIANNESS_SAFE) {
		image_hdr.major_ver = NTFSCLONE_IMG_VER_MAJOR;
		image_hdr.minor_ver = NTFSCLONE_IMG_VER_MINOR;
#if (__BYTE_ORDER == __BIG_ENDIAN)
		Printf("Old image format detected.  If the image was created "
				"on a little endian architecture it will not "
				"work.  Use a more recent version of "
				"ntfsclone to recreate the image.\n");
		image_hdr.cluster_size = cpu_to_le32(image_hdr.cluster_size);
		image_hdr.device_size = cpu_to_sle64(image_hdr.device_size);
		image_hdr.nr_clusters = cpu_to_sle64(image_hdr.nr_clusters);
		image_hdr.inuse = cpu_to_sle64(image_hdr.inuse);
#endif
		image_hdr.offset_to_image_data =
				const_cpu_to_le32((sizeof(image_hdr) + 7) & ~7);
		image_is_host_endian = TRUE;
	} else {
		typeof(image_hdr.offset_to_image_data) offset_to_image_data;
		int delta;

		if (image_hdr.major_ver > NTFSCLONE_IMG_VER_MAJOR)
			err_exit("Do not know how to handle image format "
					"version %d.%d.  Please obtain a "
					"newer version of ntfsclone.\n",
					image_hdr.major_ver,
					image_hdr.minor_ver);
		/* Read the image header data offset. */
		if (read_all(&fd_in, &offset_to_image_data,
				sizeof(offset_to_image_data)) == -1)
			perr_exit("read_all");
		image_hdr.offset_to_image_data =
				le32_to_cpu(offset_to_image_data);
		/*
		 * Read any fields from the header that we have not read yet so
		 * that the input stream is positioned correctly.  This means
		 * we can support future minor versions that just extend the
		 * header in a backwards compatible way.
		 */
		delta = offset_to_image_data - (NTFSCLONE_IMG_HEADER_SIZE_OLD +
				sizeof(image_hdr.offset_to_image_data));
		if (delta > 0) {
			char *dummy_buf;

			dummy_buf = malloc(delta);
			if (!dummy_buf)
				perr_exit("malloc dummy_buffer");
			if (read_all(&fd_in, dummy_buf, delta) == -1)
				perr_exit("read_all");
		}
	}
	return sle64_to_cpu(image_hdr.device_size);
}

static s64 open_volume(void)
{
	s64 device_size;

	mount_volume(NTFS_MNT_RDONLY);

	device_size = ntfs_device_size_get(vol->dev, 1);
	if (device_size <= 0)
		err_exit("Couldn't get device size (%lld)!\n", device_size);

	print_volume_size("Current device size", device_size);

	if (device_size < vol->nr_clusters * vol->cluster_size)
		err_exit("Current NTFS volume size is bigger than the device "
			 "size (%lld)!\nCorrupt partition table or incorrect "
			 "device partitioning?\n", device_size);

	return device_size;
}

static void initialise_image_hdr(s64 device_size, s64 inuse)
{
	memcpy(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
	image_hdr.major_ver = NTFSCLONE_IMG_VER_MAJOR;
	image_hdr.minor_ver = NTFSCLONE_IMG_VER_MINOR;
	image_hdr.cluster_size = cpu_to_le32(vol->cluster_size);
	image_hdr.device_size = cpu_to_sle64(device_size);
	image_hdr.nr_clusters = cpu_to_sle64(vol->nr_clusters);
	image_hdr.inuse = cpu_to_sle64(inuse);
	image_hdr.offset_to_image_data = cpu_to_le32((sizeof(image_hdr) + 7) &
			~7);
}

static void check_output_device(s64 input_size)
{
	if (opt.blkdev_out) {
		s64 dest_size = device_size_get(fd_out);
		if (dest_size < input_size)
			err_exit("Output device is too small (%lld) to fit the "
				 "NTFS image (%lld).\n", dest_size, input_size);

		check_if_mounted(opt.output, 0);
	} else
		set_filesize(input_size);
}

static ntfs_attr_search_ctx *attr_get_search_ctx(ntfs_inode *ni)
{
	ntfs_attr_search_ctx *ret;

	if ((ret = ntfs_attr_get_search_ctx(ni, NULL)) == NULL)
		perr_printf("ntfs_attr_get_search_ctx");

	return ret;
}

/**
 * lookup_data_attr
 *
 * Find the $DATA attribute (with or without a name) for the given ntfs inode.
 */
static ntfs_attr_search_ctx *lookup_data_attr(ntfs_inode *ni, const char *aname)
{
	ntfs_attr_search_ctx *ctx;
	ntfschar *ustr;
	int len = 0;

	if ((ctx = attr_get_search_ctx(ni)) == NULL)
		return NULL;

	if ((ustr = ntfs_str2ucs(aname, &len)) == NULL) {
		perr_printf("Couldn't convert '%s' to Unicode", aname);
		goto error_out;
	}

	if (ntfs_attr_lookup(AT_DATA, ustr, len, 0, 0, NULL, 0, ctx)) {
		perr_printf("ntfs_attr_lookup");
		goto error_out;
	}
	ntfs_ucsfree(ustr);
	return ctx;
error_out:
	ntfs_attr_put_search_ctx(ctx);
	return NULL;
}

static void ignore_bad_clusters(ntfs_walk_clusters_ctx *image)
{
	ntfs_inode *ni;
	ntfs_attr_search_ctx *ctx = NULL;
	runlist *rl, *rl_bad;
	s64 nr_bad_clusters = 0;

	if (!(ni = ntfs_inode_open(vol, FILE_BadClus)))
		perr_exit("ntfs_open_inode");

	if ((ctx = lookup_data_attr(ni, "$Bad")) == NULL)
		exit(1);

	if (!(rl_bad = ntfs_mapping_pairs_decompress(vol, ctx->attr, NULL)))
		perr_exit("ntfs_mapping_pairs_decompress");

	for (rl = rl_bad; rl->length; rl++) {
		s64 lcn = rl->lcn;

		if (lcn == LCN_HOLE || lcn < 0)
			continue;

		for (; lcn < rl->lcn + rl->length; lcn++, nr_bad_clusters++) {
			if (ntfs_bit_get_and_set(lcn_bitmap.bm, lcn, 0))
				image->inuse--;
		}
	}
	if (nr_bad_clusters)
		Printf("WARNING: The disk has %lld or more bad sectors"
		       " (hardware faults).\n", nr_bad_clusters);
	free(rl_bad);

	ntfs_attr_put_search_ctx(ctx);
	if (ntfs_inode_close(ni))
		perr_exit("ntfs_inode_close failed for $BadClus");
}

static void check_dest_free_space(u64 src_bytes)
{
	u64 dest_bytes;
	struct statvfs stvfs;
	struct stat st;

	if (opt.metadata || opt.blkdev_out || opt.std_out)
		return;
	/*
	 * TODO: save_image needs a bit more space than src_bytes
	 * due to the free space encoding overhead.
	 */
	if (fstatvfs(fd_out, &stvfs) == -1) {
		Printf("WARNING: Unknown free space on the destination: %s\n",
		       strerror(errno));
		return;
	}
	
	/* If file is a FIFO then there is no point in checking the size. */
	if (!fstat(fd_out, &st)) {
		if (S_ISFIFO(st.st_mode))
			return;
	} else
		Printf("WARNING: fstat failed: %s\n", strerror(errno));

	dest_bytes = (u64)stvfs.f_frsize * stvfs.f_bfree;
	if (!dest_bytes)
		dest_bytes = (u64)stvfs.f_bsize * stvfs.f_bfree;

	if (dest_bytes < src_bytes)
		err_exit("Destination doesn't have enough free space: "
			 "%llu MB < %llu MB\n",
			 rounded_up_division(dest_bytes, NTFS_MBYTE),
			 rounded_up_division(src_bytes,  NTFS_MBYTE));
}

int main(int argc, char **argv)
{
	ntfs_walk_clusters_ctx image;
	s64 device_size;        /* input device size in bytes */
	s64 ntfs_size;
	unsigned int wiped_total = 0;

	/* print to stderr, stdout can be an NTFS image ... */
	fprintf(stderr, "%s v%s (libntfs %s)\n", EXEC_NAME, VERSION,
		ntfs_libntfs_version());
	msg_out = stderr;

	parse_options(argc, argv);

	utils_set_locale();

	if (opt.restore_image) {
		device_size = open_image();
		ntfs_size = sle64_to_cpu(image_hdr.nr_clusters) *
				le32_to_cpu(image_hdr.cluster_size);
	} else {
		device_size = open_volume();
		ntfs_size = vol->nr_clusters * vol->cluster_size;
	}
	// FIXME: This needs to be the cluster size...
	ntfs_size += 512; /* add backup boot sector */

	if (opt.std_out) {
		if ((fd_out = fileno(stdout)) == -1)
			perr_exit("fileno for stdout failed");
	} else {
		/* device_size_get() might need to read() */
		int flags = O_RDWR;

		if (!opt.blkdev_out) {
			flags |= O_CREAT | O_TRUNC;
			if (!opt.overwrite)
				flags |= O_EXCL;
		}

		if ((fd_out = open(opt.output, flags, S_IRUSR | S_IWUSR)) == -1)
			perr_exit("Opening file '%s' failed", opt.output);

		if (!opt.save_image)
			check_output_device(ntfs_size);
	}

	if (opt.restore_image) {
		print_image_info();
		restore_image();
		fsync_clone(fd_out);
		exit(0);
	}

	setup_lcn_bitmap();
	memset(&image, 0, sizeof(image));
	backup_clusters.image = &image;

	walk_clusters(vol, &backup_clusters);
	compare_bitmaps(&lcn_bitmap);
	print_disk_usage("", vol->cluster_size, vol->nr_clusters, image.inuse);

	check_dest_free_space(vol->cluster_size * image.inuse);

	ignore_bad_clusters(&image);

	if (opt.save_image)
		initialise_image_hdr(device_size, image.inuse);

	/* FIXME: save backup boot sector */

	if (opt.std_out || !opt.metadata) {
		s64 nr_clusters_to_save = image.inuse;
		if (opt.std_out && !opt.save_image)
			nr_clusters_to_save = vol->nr_clusters;

		clone_ntfs(nr_clusters_to_save);
		fsync_clone(fd_out);
		exit(0);
	}

	wipe = 1;
	opt.volume = opt.output;
	/* 'force' again mount for dirty volumes (e.g. after resize).
	   FIXME: use mount flags to avoid potential side-effects in future */
	opt.force++;
	mount_volume(0);

	free(lcn_bitmap.bm);
	setup_lcn_bitmap();
	memset(&image, 0, sizeof(image));
	backup_clusters.image = &image;

	walk_clusters(vol, &backup_clusters);

	Printf("Num of MFT records       = %10lld\n",
			(long long)vol->mft_na->initialized_size >>
			vol->mft_record_size_bits);
	Printf("Num of used MFT records  = %10u\n", nr_used_mft_records);

	Printf("Wiped unused MFT data    = %10u\n", wiped_unused_mft_data);
	Printf("Wiped deleted MFT data   = %10u\n", wiped_unused_mft);
	Printf("Wiped resident user data = %10u\n", wiped_resident_data);
	Printf("Wiped timestamp data     = %10u\n", wiped_timestamp_data);

	wiped_total += wiped_unused_mft_data;
	wiped_total += wiped_unused_mft;
	wiped_total += wiped_resident_data;
	wiped_total += wiped_timestamp_data;
	Printf("Wiped totally            = %10u\n", wiped_total);

	fsync_clone(fd_out);
	exit(0);
}
