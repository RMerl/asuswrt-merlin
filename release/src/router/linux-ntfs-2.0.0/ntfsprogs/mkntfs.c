/**
 * mkntfs - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2007 Anton Altaparmakov
 * Copyright (c) 2001-2005 Richard Russon
 * Copyright (c) 2002-2006 Szabolcs Szakacsits
 * Copyright (c) 2005      Erik Sornes
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This utility will create an NTFS 1.2 or 3.1 volume on a user
 * specified (block) device.
 *
 * Some things (option handling and determination of mount status) have been
 * adapted from e2fsprogs-1.19 and lib/ext2fs/ismounted.c and misc/mke2fs.c in
 * particular.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS source
 * in the file COPYING); if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef ENABLE_UUID
#include <uuid/uuid.h>
#endif


#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
	extern char *optarg;
	extern int optind;
#endif

#ifdef HAVE_LINUX_MAJOR_H
#	include <linux/major.h>
#	ifndef MAJOR
#		define MAJOR(dev)	((dev) >> 8)
#		define MINOR(dev)	((dev) & 0xff)
#	endif
#	ifndef IDE_DISK_MAJOR
#		ifndef IDE0_MAJOR
#			define IDE0_MAJOR	3
#			define IDE1_MAJOR	22
#			define IDE2_MAJOR	33
#			define IDE3_MAJOR	34
#			define IDE4_MAJOR	56
#			define IDE5_MAJOR	57
#			define IDE6_MAJOR	88
#			define IDE7_MAJOR	89
#			define IDE8_MAJOR	90
#			define IDE9_MAJOR	91
#		endif
#		define IDE_DISK_MAJOR(M) \
				((M) == IDE0_MAJOR || (M) == IDE1_MAJOR || \
				(M) == IDE2_MAJOR || (M) == IDE3_MAJOR || \
				(M) == IDE4_MAJOR || (M) == IDE5_MAJOR || \
				(M) == IDE6_MAJOR || (M) == IDE7_MAJOR || \
				(M) == IDE8_MAJOR || (M) == IDE9_MAJOR)
#	endif
#	ifndef SCSI_DISK_MAJOR
#		ifndef SCSI_DISK0_MAJOR
#			define SCSI_DISK0_MAJOR	8
#			define SCSI_DISK1_MAJOR	65
#			define SCSI_DISK7_MAJOR	71
#		endif
#		define SCSI_DISK_MAJOR(M) \
				((M) == SCSI_DISK0_MAJOR || \
				((M) >= SCSI_DISK1_MAJOR && \
				(M) <= SCSI_DISK7_MAJOR))
#	endif
#endif

#include "security.h"
#include "types.h"
#include "attrib.h"
#include "bitmap.h"
#include "bootsect.h"
#include "device.h"
#include "dir.h"
#include "mft.h"
#include "mst.h"
#include "runlist.h"
#include "utils.h"
#include "ntfstime.h"
#include "sd.h"
#include "boot.h"
#include "attrdef.h"
#include "version.h"
#include "logging.h"
#include "support.h"
#include "unistr.h"

#ifdef NO_NTFS_DEVICE_DEFAULT_IO_OPS
#error "No default device io operations!  Cannot build mkntfs.  \
You need to run ./configure without the --disable-default-device-io-ops \
switch if you want to be able to build the NTFS utilities."
#endif

/* Page size on ia32. Can change to 8192 on Alpha. */
#define NTFS_PAGE_SIZE	4096

static char EXEC_NAME[] = "mkntfs";

/**
 * global variables
 */
static u8		  *g_buf		  = NULL;
static int		   g_mft_bitmap_byte_size = 0;
static u8		  *g_mft_bitmap		  = NULL;
static int		   g_lcn_bitmap_byte_size = 0;
static u8		  *g_lcn_bitmap		  = NULL;
static runlist		  *g_rl_mft		  = NULL;
static runlist		  *g_rl_mft_bmp		  = NULL;
static runlist		  *g_rl_mftmirr		  = NULL;
static runlist		  *g_rl_logfile		  = NULL;
static runlist		  *g_rl_boot		  = NULL;
static runlist		  *g_rl_bad		  = NULL;
static INDEX_ALLOCATION  *g_index_block	  = NULL;
static ntfs_volume	  *g_vol		  = NULL;
static int		   g_mft_size		  = 0;
static long long	   g_mft_lcn		  = 0;		/* lcn of $MFT, $DATA attribute */
static long long	   g_mftmirr_lcn	  = 0;		/* lcn of $MFTMirr, $DATA */
static long long	   g_logfile_lcn	  = 0;		/* lcn of $LogFile, $DATA */
static int		   g_logfile_size	  = 0;		/* in bytes, determined from volume_size */
static long long	   g_mft_zone_end	  = 0;		/* Determined from volume_size and mft_zone_multiplier, in clusters */
static long long	   g_num_bad_blocks	  = 0;		/* Number of bad clusters */
static long long	  *g_bad_blocks		  = NULL;	/* Array of bad clusters */

/**
 * struct mkntfs_options
 */
static struct mkntfs_options {
	char *dev_name;			/* Name of the device, or file, to use */
	BOOL enable_compression;	/* -C, enables compression of all files on the volume by default. */
	BOOL quick_format;		/* -f or -Q, fast format, don't zero the volume first. */
	BOOL force;			/* -F, force fs creation. */
	long heads;			/* -H, number of heads on device */
	BOOL disable_indexing;		/* -I, disables indexing of file contents on the volume by default. */
	BOOL no_action;			/* -n, do not write to device, only display what would be done. */
	long long part_start_sect;	/* -p, start sector of partition on parent device */
	long sector_size;		/* -s, in bytes, power of 2, default is 512 bytes. */
	long sectors_per_track;		/* -S, number of sectors per track on device */
	BOOL use_epoch_time;		/* -T, fake the time to be 00:00:00 UTC, Jan 1, 1970. */
	long mft_zone_multiplier;	/* -z, value from 1 to 4. Default is 1. */
	long long num_sectors;		/* size of device in sectors */
	long cluster_size;		/* -c, format with this cluster-size */
	char *label;			/* -L, volume label */
} opts;


/**
 * mkntfs_license
 */
static void mkntfs_license(void)
{
	ntfs_log_info("%s", ntfs_gpl);
}

/**
 * mkntfs_usage
 */
static void mkntfs_usage(void)
{
	ntfs_log_info("\nUsage: %s [options] device [number-of-sectors]\n"
"\n"
"Basic options:\n"
"    -f, --fast                      Perform a quick format\n"
"    -Q, --quick                     Perform a quick format\n"
"    -L, --label STRING              Set the volume label\n"
"    -C, --enable-compression        Enable compression on the volume\n"
"    -I, --no-indexing               Disable indexing on the volume\n"
"    -n, --no-action                 Do not write to disk\n"
"\n"
"Advanced options:\n"
"    -c, --cluster-size BYTES        Specify the cluster size for the volume\n"
"    -s, --sector-size BYTES         Specify the sector size for the device\n"
"    -p, --partition-start SECTOR    Specify the partition start sector\n"
"    -H, --heads NUM                 Specify the number of heads\n"
"    -S, --sectors-per-track NUM     Specify the number of sectors per track\n"
"    -z, --mft-zone-multiplier NUM   Set the MFT zone multiplier\n"
"    -T, --zero-time                 Fake the time to be 00:00 UTC, Jan 1, 1970\n"
"    -F, --force                     Force execution despite errors\n"
"\n"
"Output options:\n"
"    -q, --quiet                     Quiet execution\n"
"    -v, --verbose                   Verbose execution\n"
"        --debug                     Very verbose execution\n"
"\n"
"Help options:\n"
"    -V, --version                   Display version\n"
"    -l, --license                   Display licensing information\n"
"    -h, --help                      Display this help\n"
"\n", basename(EXEC_NAME));
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * mkntfs_version
 */
static void mkntfs_version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s)\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Create an NTFS volume on a user specified (block) "
			"device.\n\n");
	ntfs_log_info("Copyright (c) 2000-2007 Anton Altaparmakov\n");
	ntfs_log_info("Copyright (c) 2001-2005 Richard Russon\n");
	ntfs_log_info("Copyright (c) 2002-2006 Szabolcs Szakacsits\n");
	ntfs_log_info("Copyright (c) 2005      Erik Sornes\n");
	ntfs_log_info("Copyright (c) 2007      Yura Pakhuchiy\n");
	ntfs_log_info("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
}


/**
 * mkntfs_parse_long
 */
static BOOL mkntfs_parse_long(const char *string, const char *name, long *num)
{
	char *end = NULL;
	long tmp;

	if (!string || !name || !num)
		return FALSE;

	if (*num >= 0) {
		ntfs_log_error("You may only specify the %s once.\n", name);
		return FALSE;
	}

	tmp = strtol(string, &end, 0);
	if (end && *end) {
		ntfs_log_error("Cannot understand the %s '%s'.\n", name, string);
		return FALSE;
	} else {
		*num = tmp;
		return TRUE;
	}
}

/**
 * mkntfs_parse_llong
 */
static BOOL mkntfs_parse_llong(const char *string, const char *name,
		long long *num)
{
	char *end = NULL;
	long long tmp;

	if (!string || !name || !num)
		return FALSE;

	if (*num >= 0) {
		ntfs_log_error("You may only specify the %s once.\n", name);
		return FALSE;
	}

	tmp = strtoll(string, &end, 0);
	if (end && *end) {
		ntfs_log_error("Cannot understand the %s '%s'.\n", name,
				string);
		return FALSE;
	} else {
		*num = tmp;
		return TRUE;
	}
}

/**
 * mkntfs_init_options
 */
static void mkntfs_init_options(struct mkntfs_options *opts2)
{
	if (!opts2)
		return;

	memset(opts2, 0, sizeof(*opts2));

	/* Mark all the numeric options as "unset". */
	opts2->cluster_size		= -1;
	opts2->heads			= -1;
	opts2->mft_zone_multiplier	= -1;
	opts2->num_sectors		= -1;
	opts2->part_start_sect		= -1;
	opts2->sector_size		= -1;
	opts2->sectors_per_track	= -1;
}

/**
 * mkntfs_parse_options
 */
static BOOL mkntfs_parse_options(int argc, char *argv[], struct mkntfs_options *opts2)
{
	static const char *sopt = "-c:CfFhH:IlL:np:qQs:S:TvVz:";
	static const struct option lopt[] = {
		{ "cluster-size",	required_argument,	NULL, 'c' },
		{ "debug",		no_argument,		NULL, 'Z' },
		{ "enable-compression",	no_argument,		NULL, 'C' },
		{ "fast",		no_argument,		NULL, 'f' },
		{ "force",		no_argument,		NULL, 'F' },
		{ "heads",		required_argument,	NULL, 'H' },
		{ "help",		no_argument,		NULL, 'h' },
		{ "label",		required_argument,	NULL, 'L' },
		{ "license",		no_argument,		NULL, 'l' },
		{ "mft-zone-multiplier",required_argument,	NULL, 'z' },
		{ "no-action",		no_argument,		NULL, 'n' },
		{ "no-indexing",	no_argument,		NULL, 'I' },
		{ "partition-start",	required_argument,	NULL, 'p' },
		{ "quick",		no_argument,		NULL, 'Q' },
		{ "quiet",		no_argument,		NULL, 'q' },
		{ "sector-size",	required_argument,	NULL, 's' },
		{ "sectors-per-track",	required_argument,	NULL, 'S' },
		{ "verbose",		no_argument,		NULL, 'v' },
		{ "version",		no_argument,		NULL, 'V' },
		{ "zero-time",		no_argument,		NULL, 'T' },
		{ NULL, 0, NULL, 0 }
	};

	int c = -1;
	int lic = 0;
	int err = 0;
	int ver = 0;

	if (!argv || !opts2) {
		ntfs_log_error("Internal error: invalid parameters to "
				"mkntfs_options.\n");
		return FALSE;
	}

	opterr = 0; /* We'll handle the errors, thank you. */

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:		/* A device, or a number of sectors */
			if (!opts2->dev_name)
				opts2->dev_name = argv[optind - 1];
			else if (!mkntfs_parse_llong(optarg,
					"number of sectors",
					&opts2->num_sectors))
				err++;
			break;
		case 'C':
			opts2->enable_compression = TRUE;
			break;
		case 'c':
			if (!mkntfs_parse_long(optarg, "cluster size",
					&opts2->cluster_size))
				err++;
			break;
		case 'F':
			opts2->force = TRUE;
			break;
		case 'f':	/* fast */
		case 'Q':	/* quick */
			opts2->quick_format = TRUE;
			break;
		case 'H':
			if (!mkntfs_parse_long(optarg, "heads", &opts2->heads))
				err++;
			break;
		case 'h':
			err++;	/* display help */
			break;
		case 'I':
			opts2->disable_indexing = TRUE;
			break;
		case 'L':
			if (!opts2->label) {
				opts2->label = argv[optind-1];
			} else {
				ntfs_log_error("You may only specify the label "
						"once.\n");
				err++;
			}
			break;
		case 'l':
			lic++;	/* display the license */
			break;
		case 'n':
			opts2->no_action = TRUE;
			break;
		case 'p':
			if (!mkntfs_parse_llong(optarg, "partition start",
						&opts2->part_start_sect))
				err++;
			break;
		case 'q':
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET |
					NTFS_LOG_LEVEL_VERBOSE |
					NTFS_LOG_LEVEL_PROGRESS);
			break;
		case 's':
			if (!mkntfs_parse_long(optarg, "sector size",
						&opts2->sector_size))
				err++;
			break;
		case 'S':
			if (!mkntfs_parse_long(optarg, "sectors per track",
						&opts2->sectors_per_track))
				err++;
			break;
		case 'T':
			opts2->use_epoch_time = TRUE;
			break;
		case 'v':
			ntfs_log_set_levels(NTFS_LOG_LEVEL_QUIET |
					NTFS_LOG_LEVEL_VERBOSE |
					NTFS_LOG_LEVEL_PROGRESS);
			break;
		case 'V':
			ver++;	/* display version info */
			break;
		case 'Z':	/* debug - turn on everything */
			ntfs_log_set_levels(NTFS_LOG_LEVEL_DEBUG |
					NTFS_LOG_LEVEL_TRACE |
					NTFS_LOG_LEVEL_VERBOSE |
					NTFS_LOG_LEVEL_QUIET);
			break;
		case 'z':
			if (!mkntfs_parse_long(optarg, "mft zone multiplier",
						&opts2->mft_zone_multiplier))
				err++;
			break;
		default:
			if (ntfs_log_parse_option (argv[optind-1]))
				break;
			if (((optopt == 'c') || (optopt == 'H') ||
			     (optopt == 'L') || (optopt == 'p') ||
			     (optopt == 's') || (optopt == 'S') ||
			     (optopt == 'N') || (optopt == 'z')) &&
			     (!optarg)) {
				ntfs_log_error("Option '%s' requires an "
						"argument.\n", argv[optind-1]);
			} else if (optopt != '?') {
				ntfs_log_error("Unknown option '%s'.\n",
						argv[optind - 1]);
			}
			err++;
			break;
		}
	}

	if (!err && !ver && !lic) {
		if (opts2->dev_name == NULL) {
			if (argc > 1)
				ntfs_log_error("You must specify a device.\n");
			err++;
		}
	}

	if (ver)
		mkntfs_version();
	if (lic)
		mkntfs_license();
	if (err)
		mkntfs_usage();

	return (!err && !ver && !lic);
}


/**
 * mkntfs_time
 */
static time_t mkntfs_time(void)
{
	if (!opts.use_epoch_time)
		return time(NULL);
	return 0;
}

/**
 * append_to_bad_blocks
 */
static BOOL append_to_bad_blocks(unsigned long long block)
{
	long long *new_buf;

	if (!(g_num_bad_blocks & 15)) {
		new_buf = realloc(g_bad_blocks, (g_num_bad_blocks + 16) *
							sizeof(long long));
		if (!new_buf) {
			ntfs_log_perror("Reallocating memory for bad blocks "
				"list failed");
			return FALSE;
		}
		g_bad_blocks = new_buf;
	}
	g_bad_blocks[g_num_bad_blocks++] = block;
	return TRUE;
}

/**
 * mkntfs_write
 */
static long long mkntfs_write(struct ntfs_device *dev,
		const void *b, long long count)
{
	long long bytes_written, total;
	int retry;

	if (opts.no_action)
		return count;
	total = 0LL;
	retry = 0;
	do {
		bytes_written = dev->d_ops->write(dev, b, count);
		if (bytes_written == -1LL) {
			retry = errno;
			ntfs_log_perror("Error writing to %s", dev->d_name);
			errno = retry;
			return bytes_written;
		} else if (!bytes_written) {
			retry++;
		} else {
			count -= bytes_written;
			total += bytes_written;
		}
	} while (count && retry < 3);
	if (count)
		ntfs_log_error("Failed to complete writing to %s after three retries."
			"\n", dev->d_name);
	return total;
}

/**
 * ntfs_rlwrite - Write data to disk on clusters found in a runlist.
 *
 * Write to disk the clusters contained in the runlist @rl taking the data
 * from @val.  Take @val_len bytes from @val and pad the rest with zeroes.
 *
 * If the @rl specifies a completely sparse file, @val is allowed to be NULL.
 *
 * @inited_size if not NULL points to an output variable which will contain
 * the actual number of bytes written to disk. I.e. this will not include
 * sparse bytes for example.
 *
 * Return the number of bytes written (minus padding) or -1 on error. Errno
 * will be set to the error code.
 */
static s64 ntfs_rlwrite(struct ntfs_device *dev, const runlist *rl,
		const u8 *val, const s64 val_len, s64 *inited_size)
{
	s64 bytes_written, total, length, delta;
	int retry, i;

	if (inited_size)
		*inited_size = 0LL;
	if (opts.no_action)
		return val_len;
	total = 0LL;
	delta = 0LL;
	for (i = 0; rl[i].length; i++) {
		length = rl[i].length * g_vol->cluster_size;
		/* Don't write sparse runs. */
		if (rl[i].lcn == -1) {
			total += length;
			if (!val)
				continue;
			/* TODO: Check that *val is really zero at pos and len. */
			continue;
		}
		/*
		 * Break up the write into the real data write and then a write
		 * of zeroes between the end of the real data and the end of
		 * the (last) run.
		 */
		if (total + length > val_len) {
			delta = length;
			length = val_len - total;
			delta -= length;
		}
		if (dev->d_ops->seek(dev, rl[i].lcn * g_vol->cluster_size,
				SEEK_SET) == (off_t)-1)
			return -1LL;
		retry = 0;
		do {
			bytes_written = dev->d_ops->write(dev, val + total,
					length);
			if (bytes_written == -1LL) {
				retry = errno;
				ntfs_log_perror("Error writing to %s",
					dev->d_name);
				errno = retry;
				return bytes_written;
			}
			if (bytes_written) {
				length -= bytes_written;
				total += bytes_written;
				if (inited_size)
					*inited_size += bytes_written;
			} else {
				retry++;
			}
		} while (length && retry < 3);
		if (length) {
			ntfs_log_error("Failed to complete writing to %s after three "
					"retries.\n", dev->d_name);
			return total;
		}
	}
	if (delta) {
		int eo;
		char *b = ntfs_calloc(delta);
		if (!b)
			return -1;
		bytes_written = mkntfs_write(dev, b, delta);
		eo = errno;
		free(b);
		errno = eo;
		if (bytes_written == -1LL)
			return bytes_written;
	}
	return total;
}

/**
 * make_room_for_attribute - make room for an attribute inside an mft record
 * @m:		mft record
 * @pos:	position at which to make space
 * @size:	byte size to make available at this position
 *
 * @pos points to the attribute in front of which we want to make space.
 *
 * Return 0 on success or -errno on error. Possible error codes are:
 *
 *	-ENOSPC		There is not enough space available to complete
 *			operation. The caller has to make space before calling
 *			this.
 *	-EINVAL		Can only occur if mkntfs was compiled with -DDEBUG. Means
 *			the input parameters were faulty.
 */
static int make_room_for_attribute(MFT_RECORD *m, char *pos, const u32 size)
{
	u32 biu;

	if (!size)
		return 0;
#ifdef DEBUG
	/*
	 * Rigorous consistency checks. Always return -EINVAL even if more
	 * appropriate codes exist for simplicity of parsing the return value.
	 */
	if (size != ((size + 7) & ~7)) {
		ntfs_log_error("make_room_for_attribute() received non 8-byte aligned "
				"size.\n");
		return -EINVAL;
	}
	if (!m || !pos)
		return -EINVAL;
	if (pos < (char*)m || pos + size < (char*)m ||
			pos > (char*)m + le32_to_cpu(m->bytes_allocated) ||
			pos + size > (char*)m + le32_to_cpu(m->bytes_allocated))
		return -EINVAL;
	/* The -8 is for the attribute terminator. */
	if (pos - (char*)m > (int)le32_to_cpu(m->bytes_in_use) - 8)
		return -EINVAL;
#endif
	biu = le32_to_cpu(m->bytes_in_use);
	/* Do we have enough space? */
	if (biu + size > le32_to_cpu(m->bytes_allocated))
		return -ENOSPC;
	/* Move everything after pos to pos + size. */
	memmove(pos + size, pos, biu - (pos - (char*)m));
	/* Update mft record. */
	m->bytes_in_use = cpu_to_le32(biu + size);
	return 0;
}

/**
 * deallocate_scattered_clusters
 */
static void deallocate_scattered_clusters(const runlist *rl)
{
	LCN j;
	int i;

	if (!rl)
		return;
	/* Iterate over all runs in the runlist @rl. */
	for (i = 0; rl[i].length; i++) {
		/* Skip sparse runs. */
		if (rl[i].lcn == -1LL)
			continue;
		/* Deallocate the current run. */
		for (j = rl[i].lcn; j < rl[i].lcn + rl[i].length; j++)
			ntfs_bit_set(g_lcn_bitmap, j, 0);
	}
}

/**
 * allocate_scattered_clusters
 * @clusters: Amount of clusters to allocate.
 *
 * Allocate @clusters and create a runlist of the allocated clusters.
 *
 * Return the allocated runlist. Caller has to free the runlist when finished
 * with it.
 *
 * On error return NULL and errno is set to the error code.
 *
 * TODO: We should be returning the size as well, but for mkntfs this is not
 * necessary.
 */
static runlist * allocate_scattered_clusters(s64 clusters)
{
	runlist *rl = NULL, *rlt;
	VCN vcn = 0LL;
	LCN lcn, end, prev_lcn = 0LL;
	int rlpos = 0;
	int rlsize = 0;
	s64 prev_run_len = 0LL;
	char bit;

	end = g_vol->nr_clusters;
	/* Loop until all clusters are allocated. */
	while (clusters) {
		/* Loop in current zone until we run out of free clusters. */
		for (lcn = g_mft_zone_end; lcn < end; lcn++) {
			bit = ntfs_bit_get_and_set(g_lcn_bitmap, lcn, 1);
			if (bit)
				continue;
			/*
			 * Reallocate memory if necessary. Make sure we have
			 * enough for the terminator entry as well.
			 */
			if ((rlpos + 2) * (int)sizeof(runlist) >= rlsize) {
				rlsize += 4096; /* PAGE_SIZE */
				rlt = realloc(rl, rlsize);
				if (!rlt)
					goto err_end;
				rl = rlt;
			}
			/* Coalesce with previous run if adjacent LCNs. */
			if (prev_lcn == lcn - prev_run_len) {
				rl[rlpos - 1].length = ++prev_run_len;
				vcn++;
			} else {
				rl[rlpos].vcn = vcn++;
				rl[rlpos].lcn = lcn;
				prev_lcn = lcn;
				rl[rlpos].length = 1LL;
				prev_run_len = 1LL;
				rlpos++;
			}
			/* Done? */
			if (!--clusters) {
				/* Add terminator element and return. */
				rl[rlpos].vcn = vcn;
				rl[rlpos].lcn = 0LL;
				rl[rlpos].length = 0LL;
				return rl;
			}

		}
		/* Switch to next zone, decreasing mft zone by factor 2. */
		end = g_mft_zone_end;
		g_mft_zone_end >>= 1;
		/* Have we run out of space on the volume? */
		if (g_mft_zone_end <= 0)
			goto err_end;
	}
	return rl;
err_end:
	if (rl) {
		/* Add terminator element. */
		rl[rlpos].vcn = vcn;
		rl[rlpos].lcn = -1LL;
		rl[rlpos].length = 0LL;
		/* Deallocate all allocated clusters. */
		deallocate_scattered_clusters(rl);
		/* Free the runlist. */
		free(rl);
	}
	return NULL;
}

/**
 * ntfs_attr_find - find (next) attribute in mft record
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means don't care)
 * @name_len:	attribute name length (only needed if @name present)
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length
 * @ctx:	search context with mft record and attribute to search from
 *
 * You shouldn't need to call this function directly. Use lookup_attr() instead.
 *
 * ntfs_attr_find() takes a search context @ctx as parameter and searches the
 * mft record specified by @ctx->mrec, beginning at @ctx->attr, for an
 * attribute of @type, optionally @name and @val. If found, ntfs_attr_find()
 * returns 0 and @ctx->attr will point to the found attribute.
 *
 * If not found, ntfs_attr_find() returns -1, with errno set to ENOENT and
 * @ctx->attr will point to the attribute before which the attribute being
 * searched for would need to be inserted if such an action were to be desired.
 *
 * On actual error, ntfs_attr_find() returns -1 with errno set to the error
 * code but not to ENOENT.  In this case @ctx->attr is undefined and in
 * particular do not rely on it not changing.
 *
 * If @ctx->is_first is TRUE, the search begins with @ctx->attr itself. If it
 * is FALSE, the search begins after @ctx->attr.
 *
 * If @type is AT_UNUSED, return the first found attribute, i.e. one can
 * enumerate all attributes by setting @type to AT_UNUSED and then calling
 * ntfs_attr_find() repeatedly until it returns -1 with errno set to ENOENT to
 * indicate that there are no more entries. During the enumeration, each
 * successful call of ntfs_attr_find() will return the next attribute in the
 * mft record @ctx->mrec.
 *
 * If @type is AT_END, seek to the end and return -1 with errno set to ENOENT.
 * AT_END is not a valid attribute, its length is zero for example, thus it is
 * safer to return error instead of success in this case. This also allows us
 * to interoperate cleanly with ntfs_external_attr_find().
 *
 * If @name is AT_UNNAMED search for an unnamed attribute. If @name is present
 * but not AT_UNNAMED search for a named attribute matching @name. Otherwise,
 * match both named and unnamed attributes.
 *
 * If @ic is IGNORE_CASE, the @name comparison is not case sensitive and
 * @ctx->ntfs_ino must be set to the ntfs inode to which the mft record
 * @ctx->mrec belongs. This is so we can get at the ntfs volume and hence at
 * the upcase table. If @ic is CASE_SENSITIVE, the comparison is case
 * sensitive. When @name is present, @name_len is the @name length in Unicode
 * characters.
 *
 * If @name is not present (NULL), we assume that the unnamed attribute is
 * being searched for.
 *
 * Finally, the resident attribute value @val is looked for, if present.
 * If @val is not present (NULL), @val_len is ignored.
 *
 * ntfs_attr_find() only searches the specified mft record and it ignores the
 * presence of an attribute list attribute (unless it is the one being searched
 * for, obviously). If you need to take attribute lists into consideration, use
 * ntfs_attr_lookup() instead (see below). This also means that you cannot use
 * ntfs_attr_find() to search for extent records of non-resident attributes, as
 * extents with lowest_vcn != 0 are usually described by the attribute list
 * attribute only. - Note that it is possible that the first extent is only in
 * the attribute list while the last extent is in the base mft record, so don't
 * rely on being able to find the first extent in the base mft record.
 *
 * Warning: Never use @val when looking for attribute types which can be
 *	    non-resident as this most likely will result in a crash!
 */
static int mkntfs_attr_find(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	ntfschar *upcase = g_vol->upcase;
	u32 upcase_len = g_vol->upcase_len;

	/*
	 * Iterate over attributes in mft record starting at @ctx->attr, or the
	 * attribute following that, if @ctx->is_first is TRUE.
	 */
	if (ctx->is_first) {
		a = ctx->attr;
		ctx->is_first = FALSE;
	} else {
		a = (ATTR_RECORD*)((char*)ctx->attr +
				le32_to_cpu(ctx->attr->length));
	}
	for (;;	a = (ATTR_RECORD*)((char*)a + le32_to_cpu(a->length))) {
		if (p2n(a) < p2n(ctx->mrec) || (char*)a > (char*)ctx->mrec +
				le32_to_cpu(ctx->mrec->bytes_allocated))
			break;
		ctx->attr = a;
		if (((type != AT_UNUSED) && (le32_to_cpu(a->type) >
				le32_to_cpu(type))) ||
				(a->type == AT_END)) {
			errno = ENOENT;
			return -1;
		}
		if (!a->length)
			break;
		/* If this is an enumeration return this attribute. */
		if (type == AT_UNUSED)
			return 0;
		if (a->type != type)
			continue;
		/*
		 * If @name is AT_UNNAMED we want an unnamed attribute.
		 * If @name is present, compare the two names.
		 * Otherwise, match any attribute.
		 */
		if (name == AT_UNNAMED) {
			/* The search failed if the found attribute is named. */
			if (a->name_length) {
				errno = ENOENT;
				return -1;
			}
		} else if (name && !ntfs_names_are_equal(name, name_len,
				(ntfschar*)((char*)a + le16_to_cpu(a->name_offset)),
				a->name_length, ic, upcase, upcase_len)) {
			int rc;

			rc = ntfs_names_collate(name, name_len,
					(ntfschar*)((char*)a +
					le16_to_cpu(a->name_offset)),
					a->name_length, 1, IGNORE_CASE,
					upcase, upcase_len);
			/*
			 * If @name collates before a->name, there is no
			 * matching attribute.
			 */
			if (rc == -1) {
				errno = ENOENT;
				return -1;
			}
			/* If the strings are not equal, continue search. */
			if (rc)
				continue;
			rc = ntfs_names_collate(name, name_len,
					(ntfschar*)((char*)a +
					le16_to_cpu(a->name_offset)),
					a->name_length, 1, CASE_SENSITIVE,
					upcase, upcase_len);
			if (rc == -1) {
				errno = ENOENT;
				return -1;
			}
			if (rc)
				continue;
		}
		/*
		 * The names match or @name not present and attribute is
		 * unnamed. If no @val specified, we have found the attribute
		 * and are done.
		 */
		if (!val) {
			return 0;
		/* @val is present; compare values. */
		} else {
			int rc;

			rc = memcmp(val, (char*)a +le16_to_cpu(a->value_offset),
					min(val_len,
					le32_to_cpu(a->value_length)));
			/*
			 * If @val collates before the current attribute's
			 * value, there is no matching attribute.
			 */
			if (!rc) {
				u32 avl;
				avl = le32_to_cpu(a->value_length);
				if (val_len == avl)
					return 0;
				if (val_len < avl) {
					errno = ENOENT;
					return -1;
				}
			} else if (rc < 0) {
				errno = ENOENT;
				return -1;
			}
		}
	}
	ntfs_log_trace("File is corrupt. Run chkdsk.\n");
	errno = EIO;
	return -1;
}

/**
 * ntfs_attr_lookup - find an attribute in an ntfs inode
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means don't care)
 * @name_len:	attribute name length (only needed if @name present)
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
 * @lowest_vcn:	lowest vcn to find (optional, non-resident attributes only)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length
 * @ctx:	search context with mft record and attribute to search from
 *
 * Find an attribute in an ntfs inode. On first search @ctx->ntfs_ino must
 * be the base mft record and @ctx must have been obtained from a call to
 * ntfs_attr_get_search_ctx().
 *
 * This function transparently handles attribute lists and @ctx is used to
 * continue searches where they were left off at.
 *
 * If @type is AT_UNUSED, return the first found attribute, i.e. one can
 * enumerate all attributes by setting @type to AT_UNUSED and then calling
 * ntfs_attr_lookup() repeatedly until it returns -1 with errno set to ENOENT
 * to indicate that there are no more entries. During the enumeration, each
 * successful call of ntfs_attr_lookup() will return the next attribute, with
 * the current attribute being described by the search context @ctx.
 *
 * If @type is AT_END, seek to the end of the base mft record ignoring the
 * attribute list completely and return -1 with errno set to ENOENT.  AT_END is
 * not a valid attribute, its length is zero for example, thus it is safer to
 * return error instead of success in this case.  It should never be needed to
 * do this, but we implement the functionality because it allows for simpler
 * code inside ntfs_external_attr_find().
 *
 * If @name is AT_UNNAMED search for an unnamed attribute. If @name is present
 * but not AT_UNNAMED search for a named attribute matching @name. Otherwise,
 * match both named and unnamed attributes.
 *
 * After finishing with the attribute/mft record you need to call
 * ntfs_attr_put_search_ctx() to cleanup the search context (unmapping any
 * mapped extent inodes, etc).
 *
 * Return 0 if the search was successful and -1 if not, with errno set to the
 * error code.
 *
 * On success, @ctx->attr is the found attribute, it is in mft record
 * @ctx->mrec, and @ctx->al_entry is the attribute list entry for this
 * attribute with @ctx->base_* being the base mft record to which @ctx->attr
 * belongs.  If no attribute list attribute is present @ctx->al_entry and
 * @ctx->base_* are NULL.
 *
 * On error ENOENT, i.e. attribute not found, @ctx->attr is set to the
 * attribute which collates just after the attribute being searched for in the
 * base ntfs inode, i.e. if one wants to add the attribute to the mft record
 * this is the correct place to insert it into, and if there is not enough
 * space, the attribute should be placed in an extent mft record.
 * @ctx->al_entry points to the position within @ctx->base_ntfs_ino->attr_list
 * at which the new attribute's attribute list entry should be inserted.  The
 * other @ctx fields, base_ntfs_ino, base_mrec, and base_attr are set to NULL.
 * The only exception to this is when @type is AT_END, in which case
 * @ctx->al_entry is set to NULL also (see above).
 *
 * The following error codes are defined:
 *	ENOENT	Attribute not found, not an error as such.
 *	EINVAL	Invalid arguments.
 *	EIO	I/O error or corrupt data structures found.
 *	ENOMEM	Not enough memory to allocate necessary buffers.
 */
static int mkntfs_attr_lookup(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn __attribute__((unused)), const u8 *val,
		const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni;

	if (!ctx || !ctx->mrec || !ctx->attr) {
		errno = EINVAL;
		return -1;
	}
	if (ctx->base_ntfs_ino)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;
	if (!base_ni || !NInoAttrList(base_ni) || type == AT_ATTRIBUTE_LIST)
		return mkntfs_attr_find(type, name, name_len, ic, val, val_len,
				ctx);
	errno = EOPNOTSUPP;
	return -1;
}

/**
 * insert_positioned_attr_in_mft_record
 *
 * Create a non-resident attribute with a predefined on disk location
 * specified by the runlist @rl. The clusters specified by @rl are assumed to
 * be allocated already.
 *
 * Return 0 on success and -errno on error.
 */
static int insert_positioned_attr_in_mft_record(MFT_RECORD *m,
		const ATTR_TYPES type, const char *name, u32 name_len,
		const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags,
		const runlist *rl, const u8 *val, const s64 val_len)
{
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	u16 hdr_size;
	int asize, mpa_size, err, i;
	s64 bw = 0, inited_size;
	VCN highest_vcn;
	ntfschar *uname = NULL;
	int uname_len = 0;
	/*
	if (base record)
		attr_lookup();
	else
	*/

	uname = ntfs_str2ucs(name, &uname_len);
	if (!uname)
		return -errno;

	/* Check if the attribute is already there. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to allocate attribute search context.\n");
		err = -ENOMEM;
		goto err_out;
	}
	if (ic == IGNORE_CASE) {
		ntfs_log_error("FIXME: Hit unimplemented code path #1.\n");
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
		err = -EEXIST;
		goto err_out;
	}
	if (errno != ENOENT) {
		ntfs_log_error("Corrupt inode.\n");
		err = -errno;
		goto err_out;
	}
	a = ctx->attr;
	if (flags & ATTR_COMPRESSION_MASK) {
		ntfs_log_error("Compressed attributes not supported yet.\n");
		/* FIXME: Compress attribute into a temporary buffer, set */
		/* val accordingly and save the compressed size. */
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
		ntfs_log_error("Encrypted/sparse attributes not supported.\n");
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (flags & ATTR_COMPRESSION_MASK) {
		hdr_size = 72;
		/* FIXME: This compression stuff is all wrong. Never mind for */
		/* now. (AIA) */
		if (val_len)
			mpa_size = 0; /* get_size_for_compressed_mapping_pairs(rl); */
		else
			mpa_size = 0;
	} else {
		hdr_size = 64;
		if (val_len) {
			mpa_size = ntfs_get_size_for_mapping_pairs(g_vol, rl, 0);
			if (mpa_size < 0) {
				err = -errno;
				ntfs_log_error("Failed to get size for mapping "
						"pairs.\n");
				goto err_out;
			}
		} else {
			mpa_size = 0;
		}
	}
	/* Mapping pairs array and next attribute must be 8-byte aligned. */
	asize = (((int)hdr_size + ((name_len + 7) & ~7) + mpa_size) + 7) & ~7;
	/* Get the highest vcn. */
	for (i = 0, highest_vcn = 0LL; rl[i].length; i++)
		highest_vcn += rl[i].length;
	/* Does the value fit inside the allocated size? */
	if (highest_vcn * g_vol->cluster_size < val_len) {
		ntfs_log_error("BUG: Allocated size is smaller than data size!\n");
		err = -EINVAL;
		goto err_out;
	}
	err = make_room_for_attribute(m, (char*)a, asize);
	if (err == -ENOSPC) {
		/*
		 * FIXME: Make space! (AIA)
		 * can we make it non-resident? if yes, do that.
		 *	does it fit now? yes -> do it.
		 * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
		 * yes -> make non-resident
		 *	does it fit now? yes -> do it.
		 * make all attributes non-resident
		 *	does it fit now? yes -> do it.
		 * m is a base record? yes -> allocate extension record
		 *	does the new attribute fit in there? yes -> do it.
		 * split up runlist into extents and place each in an extension
		 * record.
		 * FIXME: the check for needing extension records should be
		 * earlier on as it is very quick: asize > m->bytes_allocated?
		 */
		err = -EOPNOTSUPP;
		goto err_out;
#ifdef DEBUG
	} else if (err == -EINVAL) {
		ntfs_log_error("BUG(): in insert_positioned_attribute_in_mft_"
				"record(): make_room_for_attribute() returned "
				"error: EINVAL!\n");
		goto err_out;
#endif
	}
	a->type = type;
	a->length = cpu_to_le32(asize);
	a->non_resident = 1;
	a->name_length = name_len;
	a->name_offset = cpu_to_le16(hdr_size);
	a->flags = flags;
	a->instance = m->next_attr_instance;
	m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance)
			+ 1) & 0xffff);
	a->lowest_vcn = 0;
	a->highest_vcn = cpu_to_sle64(highest_vcn - 1LL);
	a->mapping_pairs_offset = cpu_to_le16(hdr_size + ((name_len + 7) & ~7));
	memset(a->reserved1, 0, sizeof(a->reserved1));
	/* FIXME: Allocated size depends on compression. */
	a->allocated_size = cpu_to_sle64(highest_vcn * g_vol->cluster_size);
	a->data_size = cpu_to_sle64(val_len);
	if (name_len)
		memcpy((char*)a + hdr_size, uname, name_len << 1);
	if (flags & ATTR_COMPRESSION_MASK) {
		if (flags & ATTR_COMPRESSION_MASK & ~ATTR_IS_COMPRESSED) {
			ntfs_log_error("Unknown compression format. Reverting "
					"to standard compression.\n");
			a->flags &= ~ATTR_COMPRESSION_MASK;
			a->flags |= ATTR_IS_COMPRESSED;
		}
		a->compression_unit = 4;
		inited_size = val_len;
		/* FIXME: Set the compressed size. */
		a->compressed_size = 0;
		/* FIXME: Write out the compressed data. */
		/* FIXME: err = build_mapping_pairs_compressed(); */
		err = -EOPNOTSUPP;
	} else {
		a->compression_unit = 0;
		bw = ntfs_rlwrite(g_vol->dev, rl, val, val_len, &inited_size);
		if (bw != val_len) {
			ntfs_log_error("Error writing non-resident attribute "
					"value.\n");
			return -errno;
		}
		err = ntfs_mapping_pairs_build(g_vol, (u8*)a + hdr_size +
				((name_len + 7) & ~7), mpa_size, rl, 0, NULL);
	}
	a->initialized_size = cpu_to_sle64(inited_size);
	if (err < 0 || bw != val_len) {
		/* FIXME: Handle error. */
		/* deallocate clusters */
		/* remove attribute */
		if (err >= 0)
			err = -EIO;
		ntfs_log_error("insert_positioned_attr_in_mft_record failed "
				"with error %i.\n", err < 0 ? err : (int)bw);
	}
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	ntfs_ucsfree(uname);
	return err;
}

/**
 * insert_non_resident_attr_in_mft_record
 *
 * Return 0 on success and -errno on error.
 */
static int insert_non_resident_attr_in_mft_record(MFT_RECORD *m,
		const ATTR_TYPES type, const char *name, u32 name_len,
		const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags,
		const u8 *val, const s64 val_len)
{
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	u16 hdr_size;
	int asize, mpa_size, err, i;
	runlist *rl = NULL;
	s64 bw = 0;
	ntfschar *uname = NULL;
	int uname_len = 0;
	/*
	if (base record)
		attr_lookup();
	else
	*/

	uname = ntfs_str2ucs(name, &uname_len);
	if (!uname)
		return -errno;

	/* Check if the attribute is already there. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to allocate attribute search context.\n");
		err = -ENOMEM;
		goto err_out;
	}
	if (ic == IGNORE_CASE) {
		ntfs_log_error("FIXME: Hit unimplemented code path #2.\n");
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, NULL, 0, ctx)) {
		err = -EEXIST;
		goto err_out;
	}
	if (errno != ENOENT) {
		ntfs_log_error("Corrupt inode.\n");
		err = -errno;
		goto err_out;
	}
	a = ctx->attr;
	if (flags & ATTR_COMPRESSION_MASK) {
		ntfs_log_error("Compressed attributes not supported yet.\n");
		/* FIXME: Compress attribute into a temporary buffer, set */
		/* val accordingly and save the compressed size. */
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
		ntfs_log_error("Encrypted/sparse attributes not supported.\n");
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (val_len) {
		rl = allocate_scattered_clusters((val_len +
				g_vol->cluster_size - 1) / g_vol->cluster_size);
		if (!rl) {
			err = -errno;
			ntfs_log_perror("Failed to allocate scattered clusters");
			goto err_out;
		}
	} else {
		rl = NULL;
	}
	if (flags & ATTR_COMPRESSION_MASK) {
		hdr_size = 72;
		/* FIXME: This compression stuff is all wrong. Never mind for */
		/* now. (AIA) */
		if (val_len)
			mpa_size = 0; /* get_size_for_compressed_mapping_pairs(rl); */
		else
			mpa_size = 0;
	} else {
		hdr_size = 64;
		if (val_len) {
			mpa_size = ntfs_get_size_for_mapping_pairs(g_vol, rl, 0);
			if (mpa_size < 0) {
				err = -errno;
				ntfs_log_error("Failed to get size for mapping "
						"pairs.\n");
				goto err_out;
			}
		} else {
			mpa_size = 0;
		}
	}
	/* Mapping pairs array and next attribute must be 8-byte aligned. */
	asize = (((int)hdr_size + ((name_len + 7) & ~7) + mpa_size) + 7) & ~7;
	err = make_room_for_attribute(m, (char*)a, asize);
	if (err == -ENOSPC) {
		/*
		 * FIXME: Make space! (AIA)
		 * can we make it non-resident? if yes, do that.
		 *	does it fit now? yes -> do it.
		 * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
		 * yes -> make non-resident
		 *	does it fit now? yes -> do it.
		 * make all attributes non-resident
		 *	does it fit now? yes -> do it.
		 * m is a base record? yes -> allocate extension record
		 *	does the new attribute fit in there? yes -> do it.
		 * split up runlist into extents and place each in an extension
		 * record.
		 * FIXME: the check for needing extension records should be
		 * earlier on as it is very quick: asize > m->bytes_allocated?
		 */
		err = -EOPNOTSUPP;
		goto err_out;
#ifdef DEBUG
	} else if (err == -EINVAL) {
		ntfs_log_error("BUG(): in insert_non_resident_attribute_in_"
				"mft_record(): make_room_for_attribute() "
				"returned error: EINVAL!\n");
		goto err_out;
#endif
	}
	a->type = type;
	a->length = cpu_to_le32(asize);
	a->non_resident = 1;
	a->name_length = name_len;
	a->name_offset = cpu_to_le16(hdr_size);
	a->flags = flags;
	a->instance = m->next_attr_instance;
	m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance)
			+ 1) & 0xffff);
	a->lowest_vcn = 0;
	for (i = 0; rl[i].length; i++)
		;
	a->highest_vcn = cpu_to_sle64(rl[i].vcn - 1);
	a->mapping_pairs_offset = cpu_to_le16(hdr_size + ((name_len + 7) & ~7));
	memset(a->reserved1, 0, sizeof(a->reserved1));
	/* FIXME: Allocated size depends on compression. */
	a->allocated_size = cpu_to_sle64((val_len + (g_vol->cluster_size - 1)) &
			~(g_vol->cluster_size - 1));
	a->data_size = cpu_to_sle64(val_len);
	a->initialized_size = cpu_to_sle64(val_len);
	if (name_len)
		memcpy((char*)a + hdr_size, uname, name_len << 1);
	if (flags & ATTR_COMPRESSION_MASK) {
		if (flags & ATTR_COMPRESSION_MASK & ~ATTR_IS_COMPRESSED) {
			ntfs_log_error("Unknown compression format. Reverting "
					"to standard compression.\n");
			a->flags &= ~ATTR_COMPRESSION_MASK;
			a->flags |= ATTR_IS_COMPRESSED;
		}
		a->compression_unit = 4;
		/* FIXME: Set the compressed size. */
		a->compressed_size = 0;
		/* FIXME: Write out the compressed data. */
		/* FIXME: err = build_mapping_pairs_compressed(); */
		err = -EOPNOTSUPP;
	} else {
		a->compression_unit = 0;
		bw = ntfs_rlwrite(g_vol->dev, rl, val, val_len, NULL);
		if (bw != val_len) {
			ntfs_log_error("Error writing non-resident attribute "
					"value.\n");
			return -errno;
		}
		err = ntfs_mapping_pairs_build(g_vol, (u8*)a + hdr_size +
				((name_len + 7) & ~7), mpa_size, rl, 0, NULL);
	}
	if (err < 0 || bw != val_len) {
		/* FIXME: Handle error. */
		/* deallocate clusters */
		/* remove attribute */
		if (err >= 0)
			err = -EIO;
		ntfs_log_error("insert_non_resident_attr_in_mft_record failed with "
			"error %lld.\n", (long long) (err < 0 ? err : bw));
	}
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	ntfs_ucsfree(uname);
	free(rl);
	return err;
}

/**
 * insert_resident_attr_in_mft_record
 *
 * Return 0 on success and -errno on error.
 */
static int insert_resident_attr_in_mft_record(MFT_RECORD *m,
		const ATTR_TYPES type, const char *name, u32 name_len,
		const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags,
		const RESIDENT_ATTR_FLAGS res_flags,
		const u8 *val, const u32 val_len)
{
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	int asize, err;
	ntfschar *uname = NULL;
	int uname_len = 0;
	/*
	if (base record)
		mkntfs_attr_lookup();
	else
	*/

	uname = ntfs_str2ucs(name, &uname_len);
	if (!uname)
		return -errno;

	/* Check if the attribute is already there. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to allocate attribute search context.\n");
		err = -ENOMEM;
		goto err_out;
	}
	if (ic == IGNORE_CASE) {
		ntfs_log_error("FIXME: Hit unimplemented code path #3.\n");
		err = -EOPNOTSUPP;
		goto err_out;
	}
	if (!mkntfs_attr_lookup(type, uname, uname_len, ic, 0, val, val_len,
			ctx)) {
		err = -EEXIST;
		goto err_out;
	}
	if (errno != ENOENT) {
		ntfs_log_error("Corrupt inode.\n");
		err = -errno;
		goto err_out;
	}
	a = ctx->attr;
	/* sizeof(resident attribute record header) == 24 */
	asize = ((24 + ((name_len + 7) & ~7) + val_len) + 7) & ~7;
	err = make_room_for_attribute(m, (char*)a, asize);
	if (err == -ENOSPC) {
		/*
		 * FIXME: Make space! (AIA)
		 * can we make it non-resident? if yes, do that.
		 *	does it fit now? yes -> do it.
		 * m's $DATA or $BITMAP+$INDEX_ALLOCATION resident?
		 * yes -> make non-resident
		 *	does it fit now? yes -> do it.
		 * make all attributes non-resident
		 *	does it fit now? yes -> do it.
		 * m is a base record? yes -> allocate extension record
		 *	does the new attribute fit in there? yes -> do it.
		 * split up runlist into extents and place each in an extension
		 * record.
		 * FIXME: the check for needing extension records should be
		 * earlier on as it is very quick: asize > m->bytes_allocated?
		 */
		err = -EOPNOTSUPP;
		goto err_out;
	}
#ifdef DEBUG
	if (err == -EINVAL) {
		ntfs_log_error("BUG(): in insert_resident_attribute_in_mft_"
				"record(): make_room_for_attribute() returned "
				"error: EINVAL!\n");
		goto err_out;
	}
#endif
	a->type = type;
	a->length = cpu_to_le32(asize);
	a->non_resident = 0;
	a->name_length = name_len;
	if (type == AT_OBJECT_ID)
		a->name_offset = const_cpu_to_le16(0);
	else
		a->name_offset = const_cpu_to_le16(24);
	a->flags = flags;
	a->instance = m->next_attr_instance;
	m->next_attr_instance = cpu_to_le16((le16_to_cpu(m->next_attr_instance)
			+ 1) & 0xffff);
	a->value_length = cpu_to_le32(val_len);
	a->value_offset = cpu_to_le16(24 + ((name_len + 7) & ~7));
	a->resident_flags = res_flags;
	a->reservedR = 0;
	if (name_len)
		memcpy((char*)a + 24, uname, name_len << 1);
	if (val_len)
		memcpy((char*)a + le16_to_cpu(a->value_offset), val, val_len);
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	ntfs_ucsfree(uname);
	return err;
}


/**
 * add_attr_std_info
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_std_info(MFT_RECORD *m, const FILE_ATTR_FLAGS flags,
		le32 security_id)
{
	STANDARD_INFORMATION si;
	int err, sd_size;

	sd_size = 48;

	si.creation_time = utc2ntfs(mkntfs_time());
	si.last_data_change_time = si.creation_time;
	si.last_mft_change_time = si.creation_time;
	si.last_access_time = si.creation_time;
	si.file_attributes = flags; /* already LE */
	si.maximum_versions = cpu_to_le32(0);
	si.version_number = cpu_to_le32(0);
	si.class_id = cpu_to_le32(0);
	si.security_id = security_id;
	if (si.security_id != 0)
		sd_size = 72;
	/* FIXME: $Quota support... */
	si.owner_id = cpu_to_le32(0);
	si.quota_charged = cpu_to_le64(0ULL);
	/* FIXME: $UsnJrnl support... Not needed on fresh w2k3-volume */
	si.usn = cpu_to_le64(0ULL);
	/* NTFS 1.2: size of si = 48, NTFS 3.[01]: size of si = 72 */
	err = insert_resident_attr_in_mft_record(m, AT_STANDARD_INFORMATION,
			NULL, 0, 0, 0, 0, (u8*)&si, sd_size);
	if (err < 0)
		ntfs_log_perror("add_attr_std_info failed");
	return err;
}

/**
 * add_attr_file_name
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_file_name(MFT_RECORD *m, const leMFT_REF parent_dir,
		const s64 allocated_size, const s64 data_size,
		const FILE_ATTR_FLAGS flags, const u16 packed_ea_size,
		const u32 reparse_point_tag, const char *file_name,
		const FILE_NAME_TYPE_FLAGS file_name_type)
{
	ntfs_attr_search_ctx *ctx;
	STANDARD_INFORMATION *si;
	FILE_NAME_ATTR *fn;
	int i, fn_size;
	ntfschar *uname;

	/* Check if the attribute is already there. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to get attribute search context.\n");
		return -ENOMEM;
	}
	if (mkntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0, 0, 0,
				NULL, 0, ctx)) {
		int eo = errno;
		ntfs_log_error("BUG: Standard information attribute not "
				"present in file record.\n");
		ntfs_attr_put_search_ctx(ctx);
		return -eo;
	}
	si = (STANDARD_INFORMATION*)((char*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	i = (strlen(file_name) + 1) * sizeof(ntfschar);
	fn_size = sizeof(FILE_NAME_ATTR) + i;
	fn = ntfs_malloc(fn_size);
	if (!fn) {
		ntfs_attr_put_search_ctx(ctx);
		return -errno;
	}
	fn->parent_directory = parent_dir;

	fn->creation_time = si->creation_time;
	fn->last_data_change_time = si->last_data_change_time;
	fn->last_mft_change_time = si->last_mft_change_time;
	fn->last_access_time = si->last_access_time;
	ntfs_attr_put_search_ctx(ctx);

	fn->allocated_size = cpu_to_sle64(allocated_size);
	fn->data_size = cpu_to_sle64(data_size);
	fn->file_attributes = flags;
	/* These are in a union so can't have both. */
	if (packed_ea_size && reparse_point_tag) {
		free(fn);
		return -EINVAL;
	}
	if (packed_ea_size) {
		fn->packed_ea_size = cpu_to_le16(packed_ea_size);
		fn->reserved = cpu_to_le16(0);
	} else {
		fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
	}
	fn->file_name_type = file_name_type;
	uname = fn->file_name;
	i = ntfs_mbstoucs(file_name, &uname, i);
	if (i < 1) {
		free(fn);
		return -EINVAL;
	}
	if (i > 0xff) {
		free(fn);
		return -ENAMETOOLONG;
	}
	/* No terminating null in file names. */
	fn->file_name_length = i;
	fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
	i = insert_resident_attr_in_mft_record(m, AT_FILE_NAME, NULL, 0, 0,
			0, RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
	free(fn);
	if (i < 0)
		ntfs_log_error("add_attr_file_name failed: %s\n", strerror(-i));
	return i;
}

#ifdef ENABLE_UUID

/**
 * add_attr_object_id -
 *
 * Note we insert only a basic object id which only has the GUID and none of
 * the extended fields.  This is because we currently only use this function
 * when creating the object id for the volume.
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_object_id(MFT_RECORD *m, const GUID *object_id)
{
	OBJECT_ID_ATTR oi;
	int err;

	oi = (OBJECT_ID_ATTR) {
		.object_id = *object_id,
	};
	err = insert_resident_attr_in_mft_record(m, AT_OBJECT_ID, NULL,
			0, 0, 0, 0, (u8*)&oi, sizeof(oi.object_id));
	if (err < 0)
		ntfs_log_error("add_attr_vol_info failed: %s\n", strerror(-err));
	return err;
}

#endif

/**
 * add_attr_sd
 *
 * Create the security descriptor attribute adding the security descriptor @sd
 * of length @sd_len to the mft record @m.
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_sd(MFT_RECORD *m, const u8 *sd, const s64 sd_len)
{
	int err;

	/* Does it fit? NO: create non-resident. YES: create resident. */
	if (le32_to_cpu(m->bytes_in_use) + 24 + sd_len >
						le32_to_cpu(m->bytes_allocated))
		err = insert_non_resident_attr_in_mft_record(m,
				AT_SECURITY_DESCRIPTOR, NULL, 0, 0, 0, sd,
				sd_len);
	else
		err = insert_resident_attr_in_mft_record(m,
				AT_SECURITY_DESCRIPTOR, NULL, 0, 0, 0, 0, sd,
				sd_len);
	if (err < 0)
		ntfs_log_error("add_attr_sd failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_data
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_data(MFT_RECORD *m, const char *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const ATTR_FLAGS flags,
		const u8 *val, const s64 val_len)
{
	int err;

	/*
	 * Does it fit? NO: create non-resident. YES: create resident.
	 *
	 * FIXME: Introduced arbitrary limit of mft record allocated size - 512.
	 * This is to get around the problem that if $Bitmap/$DATA becomes too
	 * big, but is just small enough to be resident, we would make it
	 * resident, and later run out of space when creating the other
	 * attributes and this would cause us to abort as making resident
	 * attributes non-resident is not supported yet.
	 * The proper fix is to support making resident attribute non-resident.
	 */
	if (le32_to_cpu(m->bytes_in_use) + 24 + val_len >
			min(le32_to_cpu(m->bytes_allocated),
			le32_to_cpu(m->bytes_allocated) - 512))
		err = insert_non_resident_attr_in_mft_record(m, AT_DATA, name,
				name_len, ic, flags, val, val_len);
	else
		err = insert_resident_attr_in_mft_record(m, AT_DATA, name,
				name_len, ic, flags, 0, val, val_len);

	if (err < 0)
		ntfs_log_error("add_attr_data failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_data_positioned
 *
 * Create a non-resident data attribute with a predefined on disk location
 * specified by the runlist @rl. The clusters specified by @rl are assumed to
 * be allocated already.
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_data_positioned(MFT_RECORD *m, const char *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const ATTR_FLAGS flags, const runlist *rl,
		const u8 *val, const s64 val_len)
{
	int err;

	err = insert_positioned_attr_in_mft_record(m, AT_DATA, name, name_len,
			ic, flags, rl, val, val_len);
	if (err < 0)
		ntfs_log_error("add_attr_data_positioned failed: %s\n",
				strerror(-err));
	return err;
}

/**
 * add_attr_vol_name
 *
 * Create volume name attribute specifying the volume name @vol_name as a null
 * terminated char string of length @vol_name_len (number of characters not
 * including the terminating null), which is converted internally to a little
 * endian ntfschar string. The name is at least 1 character long and at most
 * 0xff characters long (not counting the terminating null).
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_vol_name(MFT_RECORD *m, const char *vol_name,
		const int vol_name_len __attribute__((unused)))
{
	ntfschar *uname = NULL;
	int uname_len = 0;
	int i;

	if (vol_name) {
		uname_len = ntfs_mbstoucs(vol_name, &uname, 0);
		if (uname_len < 0)
			return -errno;
		if (uname_len > 0xff) {
			free(uname);
			return -ENAMETOOLONG;
		}
	}
	i = insert_resident_attr_in_mft_record(m, AT_VOLUME_NAME, NULL, 0, 0,
			0, 0, (u8*)uname, uname_len*sizeof(ntfschar));
	free(uname);
	if (i < 0)
		ntfs_log_error("add_attr_vol_name failed: %s\n", strerror(-i));
	return i;
}

/**
 * add_attr_vol_info
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_vol_info(MFT_RECORD *m, const VOLUME_FLAGS flags,
		const u8 major_ver, const u8 minor_ver)
{
	VOLUME_INFORMATION vi;
	int err;

	memset(&vi, 0, sizeof(vi));
	vi.major_ver = major_ver;
	vi.minor_ver = minor_ver;
	vi.flags = flags & VOLUME_FLAGS_MASK;
	err = insert_resident_attr_in_mft_record(m, AT_VOLUME_INFORMATION, NULL,
			0, 0, 0, 0, (u8*)&vi, sizeof(vi));
	if (err < 0)
		ntfs_log_error("add_attr_vol_info failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_index_root
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_index_root(MFT_RECORD *m, const char *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const ATTR_TYPES indexed_attr_type,
		const COLLATION_RULES collation_rule,
		const u32 index_block_size)
{
	INDEX_ROOT *r;
	INDEX_ENTRY_HEADER *e;
	int err, val_len;

	val_len = sizeof(INDEX_ROOT) + sizeof(INDEX_ENTRY_HEADER);
	r = ntfs_malloc(val_len);
	if (!r)
		return -errno;
	r->type = (indexed_attr_type == AT_FILE_NAME) ? AT_FILE_NAME : 0;
	if (indexed_attr_type == AT_FILE_NAME &&
			collation_rule != COLLATION_FILE_NAME) {
		free(r);
		ntfs_log_error("add_attr_index_root: indexed attribute is $FILE_NAME "
			"but collation rule is not COLLATION_FILE_NAME.\n");
		return -EINVAL;
	}
	r->collation_rule = collation_rule;
	r->index_block_size = cpu_to_le32(index_block_size);
	if (index_block_size >= g_vol->cluster_size) {
		if (index_block_size % g_vol->cluster_size) {
			ntfs_log_error("add_attr_index_root: index block size is not "
					"a multiple of the cluster size.\n");
			free(r);
			return -EINVAL;
		}
		r->clusters_per_index_block = index_block_size /
				g_vol->cluster_size;
	} else { /* if (g_vol->cluster_size > index_block_size) */
		if (index_block_size & (index_block_size - 1)) {
			ntfs_log_error("add_attr_index_root: index block size is not "
					"a power of 2.\n");
			free(r);
			return -EINVAL;
		}
		if (index_block_size < (u32)opts.sector_size) {
			 ntfs_log_error("add_attr_index_root: index block size "
					 "is smaller than the sector size.\n");
			 free(r);
			 return -EINVAL;
		}
		r->clusters_per_index_block = index_block_size /
				opts.sector_size;
	}
	memset(&r->reserved, 0, sizeof(r->reserved));
	r->index.entries_offset = const_cpu_to_le32(sizeof(INDEX_HEADER));
	r->index.index_length = const_cpu_to_le32(sizeof(INDEX_HEADER) +
			sizeof(INDEX_ENTRY_HEADER));
	r->index.allocated_size = r->index.index_length;
	r->index.flags = SMALL_INDEX;
	memset(&r->index.reserved, 0, sizeof(r->index.reserved));
	e = (INDEX_ENTRY_HEADER*)((u8*)&r->index +
			le32_to_cpu(r->index.entries_offset));
	/*
	 * No matter whether this is a file index or a view as this is a
	 * termination entry, hence no key value / data is associated with it
	 * at all. Thus, we just need the union to be all zero.
	 */
	e->indexed_file = const_cpu_to_le64(0LL);
	e->length = const_cpu_to_le16(sizeof(INDEX_ENTRY_HEADER));
	e->key_length = const_cpu_to_le16(0);
	e->flags = INDEX_ENTRY_END;
	e->reserved = const_cpu_to_le16(0);
	err = insert_resident_attr_in_mft_record(m, AT_INDEX_ROOT, name,
				name_len, ic, 0, 0, (u8*)r, val_len);
	free(r);
	if (err < 0)
		ntfs_log_error("add_attr_index_root failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_index_alloc
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_index_alloc(MFT_RECORD *m, const char *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const u8 *index_alloc_val, const u32 index_alloc_val_len)
{
	int err;

	err = insert_non_resident_attr_in_mft_record(m, AT_INDEX_ALLOCATION,
			name, name_len, ic, 0, index_alloc_val,
			index_alloc_val_len);
	if (err < 0)
		ntfs_log_error("add_attr_index_alloc failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_bitmap
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_bitmap(MFT_RECORD *m, const char *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const u8 *bitmap,
		const u32 bitmap_len)
{
	int err;

	/* Does it fit? NO: create non-resident. YES: create resident. */
	if (le32_to_cpu(m->bytes_in_use) + 24 + bitmap_len >
						le32_to_cpu(m->bytes_allocated))
		err = insert_non_resident_attr_in_mft_record(m, AT_BITMAP, name,
				name_len, ic, 0, bitmap, bitmap_len);
	else
		err = insert_resident_attr_in_mft_record(m, AT_BITMAP, name,
				name_len, ic, 0, 0, bitmap, bitmap_len);

	if (err < 0)
		ntfs_log_error("add_attr_bitmap failed: %s\n", strerror(-err));
	return err;
}

/**
 * add_attr_bitmap_positioned
 *
 * Create a non-resident bitmap attribute with a predefined on disk location
 * specified by the runlist @rl. The clusters specified by @rl are assumed to
 * be allocated already.
 *
 * Return 0 on success or -errno on error.
 */
static int add_attr_bitmap_positioned(MFT_RECORD *m, const char *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const runlist *rl, const u8 *bitmap, const u32 bitmap_len)
{
	int err;

	err = insert_positioned_attr_in_mft_record(m, AT_BITMAP, name, name_len,
			ic, 0, rl, bitmap, bitmap_len);
	if (err < 0)
		ntfs_log_error("add_attr_bitmap_positioned failed: %s\n",
				strerror(-err));
	return err;
}


/**
 * upgrade_to_large_index
 *
 * Create bitmap and index allocation attributes, modify index root
 * attribute accordingly and move all of the index entries from the index root
 * into the index allocation.
 *
 * Return 0 on success or -errno on error.
 */
static int upgrade_to_large_index(MFT_RECORD *m, const char *name,
		u32 name_len, const IGNORE_CASE_BOOL ic,
		INDEX_ALLOCATION **idx)
{
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	INDEX_ROOT *r;
	INDEX_ENTRY *re;
	INDEX_ALLOCATION *ia_val = NULL;
	ntfschar *uname = NULL;
	int uname_len = 0;
	u8 bmp[8];
	char *re_start, *re_end;
	int i, err, index_block_size;

	uname = ntfs_str2ucs(name, &uname_len);
	if (!uname)
		return -errno;

	/* Find the index root attribute. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to allocate attribute search context.\n");
		ntfs_ucsfree(uname);
		return -ENOMEM;
	}
	if (ic == IGNORE_CASE) {
		ntfs_log_error("FIXME: Hit unimplemented code path #4.\n");
		err = -EOPNOTSUPP;
		ntfs_ucsfree(uname);
		goto err_out;
	}
	err = mkntfs_attr_lookup(AT_INDEX_ROOT, uname, uname_len, ic, 0, NULL, 0,
			ctx);
	ntfs_ucsfree(uname);
	if (err) {
		err = -ENOTDIR;
		goto err_out;
	}
	a = ctx->attr;
	if (a->non_resident || a->flags) {
		err = -EINVAL;
		goto err_out;
	}
	r = (INDEX_ROOT*)((char*)a + le16_to_cpu(a->value_offset));
	re_end = (char*)r + le32_to_cpu(a->value_length);
	re_start = (char*)&r->index + le32_to_cpu(r->index.entries_offset);
	re = (INDEX_ENTRY*)re_start;
	index_block_size = le32_to_cpu(r->index_block_size);
	memset(bmp, 0, sizeof(bmp));
	ntfs_bit_set(bmp, 0ULL, 1);
	/* Bitmap has to be at least 8 bytes in size. */
	err = add_attr_bitmap(m, name, name_len, ic, bmp, sizeof(bmp));
	if (err)
		goto err_out;
	ia_val = ntfs_calloc(index_block_size);
	if (!ia_val) {
		err = -errno;
		goto err_out;
	}
	/* Setup header. */
	ia_val->magic = magic_INDX;
	ia_val->usa_ofs = cpu_to_le16(sizeof(INDEX_ALLOCATION));
	if (index_block_size >= NTFS_BLOCK_SIZE) {
		ia_val->usa_count = cpu_to_le16(index_block_size /
				NTFS_BLOCK_SIZE + 1);
	} else {
		ia_val->usa_count = cpu_to_le16(1);
		ntfs_log_error("Sector size is bigger than index block size. "
				"Setting usa_count to 1. If Windows chkdsk "
				"reports this as corruption, please email %s "
				"stating that you saw this message and that "
				"the filesystem created was corrupt.  "
				"Thank you.", NTFS_DEV_LIST);
	}
	/* Set USN to 1. */
	*(le16*)((char*)ia_val + le16_to_cpu(ia_val->usa_ofs)) =
			cpu_to_le16(1);
	ia_val->lsn = 0;
	ia_val->index_block_vcn = 0;
	ia_val->index.flags = LEAF_NODE;
	/* Align to 8-byte boundary. */
	ia_val->index.entries_offset = cpu_to_le32((sizeof(INDEX_HEADER) +
			le16_to_cpu(ia_val->usa_count) * 2 + 7) & ~7);
	ia_val->index.allocated_size = cpu_to_le32(index_block_size -
			(sizeof(INDEX_ALLOCATION) - sizeof(INDEX_HEADER)));
	/* Find the last entry in the index root and save it in re. */
	while ((char*)re < re_end && !(re->flags & INDEX_ENTRY_END)) {
		/* Next entry in index root. */
		re = (INDEX_ENTRY*)((char*)re + le16_to_cpu(re->length));
	}
	/* Copy all the entries including the termination entry. */
	i = (char*)re - re_start + le16_to_cpu(re->length);
	memcpy((char*)&ia_val->index +
			le32_to_cpu(ia_val->index.entries_offset), re_start, i);
	/* Finish setting up index allocation. */
	ia_val->index.index_length = cpu_to_le32(i +
			le32_to_cpu(ia_val->index.entries_offset));
	/* Move the termination entry forward to the beginning if necessary. */
	if ((char*)re > re_start) {
		memmove(re_start, (char*)re, le16_to_cpu(re->length));
		re = (INDEX_ENTRY*)re_start;
	}
	/* Now fixup empty index root with pointer to index allocation VCN 0. */
	r->index.flags = LARGE_INDEX;
	re->flags |= INDEX_ENTRY_NODE;
	if (le16_to_cpu(re->length) < sizeof(INDEX_ENTRY_HEADER) + sizeof(VCN))
		re->length = cpu_to_le16(le16_to_cpu(re->length) + sizeof(VCN));
	r->index.index_length = cpu_to_le32(le32_to_cpu(r->index.entries_offset)
			+ le16_to_cpu(re->length));
	r->index.allocated_size = r->index.index_length;
	/* Resize index root attribute. */
	if (ntfs_resident_attr_value_resize(m, a, sizeof(INDEX_ROOT) -
			sizeof(INDEX_HEADER) +
			le32_to_cpu(r->index.allocated_size))) {
		/* TODO: Remove the added bitmap! */
		/* Revert index root from index allocation. */
		err = -errno;
		goto err_out;
	}
	/* Set VCN pointer to 0LL. */
	*(leVCN*)((char*)re + le16_to_cpu(re->length) - sizeof(VCN)) = 0;
	err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)ia_val, index_block_size);
	if (err) {
		err = -errno;
		ntfs_log_error("ntfs_mst_pre_write_fixup() failed in "
				"upgrade_to_large_index.\n");
		goto err_out;
	}
	err = add_attr_index_alloc(m, name, name_len, ic, (u8*)ia_val,
			index_block_size);
	ntfs_mst_post_write_fixup((NTFS_RECORD*)ia_val);
	if (err) {
		/* TODO: Remove the added bitmap! */
		/* Revert index root from index allocation. */
		goto err_out;
	}
	*idx = ia_val;
	ntfs_attr_put_search_ctx(ctx);
	return 0;
err_out:
	ntfs_attr_put_search_ctx(ctx);
	free(ia_val);
	return err;
}

/**
 * make_room_for_index_entry_in_index_block
 *
 * Create space of @size bytes at position @pos inside the index block @idx.
 *
 * Return 0 on success or -errno on error.
 */
static int make_room_for_index_entry_in_index_block(INDEX_BLOCK *idx,
		INDEX_ENTRY *pos, u32 size)
{
	u32 biu;

	if (!size)
		return 0;
#ifdef DEBUG
	/*
	 * Rigorous consistency checks. Always return -EINVAL even if more
	 * appropriate codes exist for simplicity of parsing the return value.
	 */
	if (size != ((size + 7) & ~7)) {
		ntfs_log_error("make_room_for_index_entry_in_index_block() received "
				"non 8-byte aligned size.\n");
		return -EINVAL;
	}
	if (!idx || !pos)
		return -EINVAL;
	if ((char*)pos < (char*)idx || (char*)pos + size < (char*)idx ||
			(char*)pos > (char*)idx + sizeof(INDEX_BLOCK) -
				sizeof(INDEX_HEADER) +
				le32_to_cpu(idx->index.allocated_size) ||
			(char*)pos + size > (char*)idx + sizeof(INDEX_BLOCK) -
				sizeof(INDEX_HEADER) +
				le32_to_cpu(idx->index.allocated_size))
		return -EINVAL;
	/* The - sizeof(INDEX_ENTRY_HEADER) is for the index terminator. */
	if ((char*)pos - (char*)&idx->index >
			(int)le32_to_cpu(idx->index.index_length)
			- (int)sizeof(INDEX_ENTRY_HEADER))
		return -EINVAL;
#endif
	biu = le32_to_cpu(idx->index.index_length);
	/* Do we have enough space? */
	if (biu + size > le32_to_cpu(idx->index.allocated_size))
		return -ENOSPC;
	/* Move everything after pos to pos + size. */
	memmove((char*)pos + size, (char*)pos, biu - ((char*)pos -
			(char*)&idx->index));
	/* Update index block. */
	idx->index.index_length = cpu_to_le32(biu + size);
	return 0;
}

/**
 * ntfs_index_keys_compare
 *
 * not all types of COLLATION_RULES supported yet...
 * added as needed.. (remove this comment when all are added)
 */
static int ntfs_index_keys_compare(u8 *key1, u8 *key2, int key1_length,
		int key2_length, COLLATION_RULES collation_rule)
{
	u32 u1, u2;
	int i;

	if (collation_rule == COLLATION_NTOFS_ULONG) {
		/* i.e. $SII or $QUOTA-$Q */
		u1 = le32_to_cpup(key1);
		u2 = le32_to_cpup(key2);
		if (u1 < u2)
			return -1;
		if (u1 > u2)
			return 1;
		/* u1 == u2 */
		return 0;
	}
	if (collation_rule == COLLATION_NTOFS_ULONGS) {
		/* i.e $OBJID-$O */
		i = 0;
		while (i < min(key1_length, key2_length)) {
			u1 = le32_to_cpup(key1 + i);
			u2 = le32_to_cpup(key2 + i);
			if (u1 < u2)
				return -1;
			if (u1 > u2)
				return 1;
			/* u1 == u2 */
			i += sizeof(u32);
		}
		if (key1_length < key2_length)
			return -1;
		if (key1_length > key2_length)
			return 1;
		return 0;
	}
	if (collation_rule == COLLATION_NTOFS_SECURITY_HASH) {
		/* i.e. $SDH */
		u1 = le32_to_cpu(((SDH_INDEX_KEY*)key1)->hash);
		u2 = le32_to_cpu(((SDH_INDEX_KEY*)key2)->hash);
		if (u1 < u2)
			return -1;
		if (u1 > u2)
			return 1;
		/* u1 == u2 */
		u1 = le32_to_cpu(((SDH_INDEX_KEY*)key1)->security_id);
		u2 = le32_to_cpu(((SDH_INDEX_KEY*)key2)->security_id);
		if (u1 < u2)
			return -1;
		if (u1 > u2)
			return 1;
		return 0;
	}
	if (collation_rule == COLLATION_NTOFS_SID) {
		/* i.e. $QUOTA-O */
		i = memcmp(key1, key2, min(key1_length, key2_length));
		if (!i) {
			if (key1_length < key2_length)
				return -1;
			if (key1_length > key2_length)
				return 1;
		}
		return i;
	}
	ntfs_log_critical("ntfs_index_keys_compare called without supported "
			"collation rule.\n");
	return 0;	/* Claim they're equal.  What else can we do? */
}

/**
 * insert_index_entry_in_res_dir_index
 *
 * i.e. insert an index_entry in some named index_root
 * simplified search method, works for mkntfs
 */
static int insert_index_entry_in_res_dir_index(INDEX_ENTRY *idx, u32 idx_size,
		MFT_RECORD *m, ntfschar *name, u32 name_size, ATTR_TYPES type)
{
	ntfs_attr_search_ctx *ctx;
	INDEX_HEADER *idx_header;
	INDEX_ENTRY *idx_entry, *idx_end;
	ATTR_RECORD *a;
	COLLATION_RULES collation_rule;
	int err, i;

	err = 0;
	/* does it fit ?*/
	if (g_vol->mft_record_size > idx_size + le32_to_cpu(m->bytes_allocated))
		return -ENOSPC;
	/* find the INDEX_ROOT attribute:*/
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_error("Failed to allocate attribute search "
				"context.\n");
		err = -ENOMEM;
		goto err_out;
	}
	if (mkntfs_attr_lookup(AT_INDEX_ROOT, name, name_size, 0, 0, NULL, 0,
			ctx)) {
		err = -EEXIST;
		goto err_out;
	}
	/* found attribute */
	a = (ATTR_RECORD*)ctx->attr;
	collation_rule = ((INDEX_ROOT*)((u8*)a +
			le16_to_cpu(a->value_offset)))->collation_rule;
	idx_header = (INDEX_HEADER*)((u8*)a + le16_to_cpu(a->value_offset)
			+ 0x10);
	idx_entry = (INDEX_ENTRY*)((u8*)idx_header +
			le32_to_cpu(idx_header->entries_offset));
	idx_end = (INDEX_ENTRY*)((u8*)idx_entry +
			le32_to_cpu(idx_header->index_length));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	if (type == AT_FILE_NAME) {
		while (((u8*)idx_entry < (u8*)idx_end) &&
				!(idx_entry->flags & INDEX_ENTRY_END)) {
			i = ntfs_file_values_compare(&idx->key.file_name,
					&idx_entry->key.file_name, 1,
					IGNORE_CASE, g_vol->upcase,
					g_vol->upcase_len);
			/*
			 * If @file_name collates before ie->key.file_name,
			 * there is no matching index entry.
			 */
			if (i == -1)
				break;
			/* If file names are not equal, continue search. */
			if (i)
				goto do_next;
			if (idx->key.file_name.file_name_type !=
					FILE_NAME_POSIX ||
					idx_entry->key.file_name.file_name_type
					!= FILE_NAME_POSIX)
				return -EEXIST;
			i = ntfs_file_values_compare(&idx->key.file_name,
					&idx_entry->key.file_name, 1,
					CASE_SENSITIVE, g_vol->upcase,
					g_vol->upcase_len);
			if (!i)
				return -EEXIST;
			if (i == -1)
				break;
do_next:
			idx_entry = (INDEX_ENTRY*)((u8*)idx_entry +
					le16_to_cpu(idx_entry->length));
		}
	} else if (type == AT_UNUSED) {  /* case view */
		while (((u8*)idx_entry < (u8*)idx_end) &&
				!(idx_entry->flags & INDEX_ENTRY_END)) {
			i = ntfs_index_keys_compare((u8*)idx + 0x10,
					(u8*)idx_entry + 0x10,
					le16_to_cpu(idx->key_length),
					le16_to_cpu(idx_entry->key_length),
					collation_rule);
			if (!i)
				return -EEXIST;
			if (i == -1)
				break;
			idx_entry = (INDEX_ENTRY*)((u8*)idx_entry +
					le16_to_cpu(idx_entry->length));
		}
	} else
		return -EINVAL;
	memmove((u8*)idx_entry + idx_size, (u8*)idx_entry,
			le32_to_cpu(m->bytes_in_use) -
			((u8*)idx_entry - (u8*)m));
	memcpy((u8*)idx_entry, (u8*)idx, idx_size);
	/* Adjust various offsets, etc... */
	m->bytes_in_use = cpu_to_le32(le32_to_cpu(m->bytes_in_use) + idx_size);
	a->length = cpu_to_le32(le32_to_cpu(a->length) + idx_size);
	a->value_length = cpu_to_le32(le32_to_cpu(a->value_length) + idx_size);
	idx_header->index_length = cpu_to_le32(
			le32_to_cpu(idx_header->index_length) + idx_size);
	idx_header->allocated_size = cpu_to_le32(
			le32_to_cpu(idx_header->allocated_size) + idx_size);
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	return err;
}

/**
 * initialize_secure
 *
 * initializes $Secure's $SDH and $SII indexes from $SDS datastream
 */
static int initialize_secure(char *sds, u32 sds_size, MFT_RECORD *m)
{
	int err, sdh_size, sii_size;
	SECURITY_DESCRIPTOR_HEADER *sds_header;
	INDEX_ENTRY *idx_entry_sdh, *idx_entry_sii;
	SDH_INDEX_DATA *sdh_data;
	SII_INDEX_DATA *sii_data;

	sds_header = (SECURITY_DESCRIPTOR_HEADER*)sds;
	sdh_size  = sizeof(INDEX_ENTRY_HEADER);
	sdh_size += sizeof(SDH_INDEX_KEY) + sizeof(SDH_INDEX_DATA);
	sii_size  = sizeof(INDEX_ENTRY_HEADER);
	sii_size += sizeof(SII_INDEX_KEY) + sizeof(SII_INDEX_DATA);
	idx_entry_sdh = ntfs_calloc(sizeof(INDEX_ENTRY));
	if (!idx_entry_sdh)
		return -errno;
	idx_entry_sii = ntfs_calloc(sizeof(INDEX_ENTRY));
	if (!idx_entry_sii) {
		free(idx_entry_sdh);
		return -errno;
	}
	err = 0;

	while ((char*)sds_header < (char*)sds + sds_size) {
		if (!sds_header->length)
			break;
		/* SDH index entry */
		idx_entry_sdh->data_offset = const_cpu_to_le16(0x18);
		idx_entry_sdh->data_length = const_cpu_to_le16(0x14);
		idx_entry_sdh->reservedV = const_cpu_to_le32(0x00);
		idx_entry_sdh->length = const_cpu_to_le16(0x30);
		idx_entry_sdh->key_length = const_cpu_to_le16(0x08);
		idx_entry_sdh->flags = const_cpu_to_le16(0x00);
		idx_entry_sdh->reserved = const_cpu_to_le16(0x00);
		idx_entry_sdh->key.sdh.hash = sds_header->hash;
		idx_entry_sdh->key.sdh.security_id = sds_header->security_id;
		sdh_data = (SDH_INDEX_DATA*)((u8*)idx_entry_sdh +
				le16_to_cpu(idx_entry_sdh->data_offset));
		sdh_data->hash = sds_header->hash;
		sdh_data->security_id = sds_header->security_id;
		sdh_data->offset = sds_header->offset;
		sdh_data->length = sds_header->length;
		sdh_data->reserved_II = const_cpu_to_le32(0x00490049);

		/* SII index entry */
		idx_entry_sii->data_offset = const_cpu_to_le16(0x14);
		idx_entry_sii->data_length = const_cpu_to_le16(0x14);
		idx_entry_sii->reservedV = const_cpu_to_le32(0x00);
		idx_entry_sii->length = const_cpu_to_le16(0x28);
		idx_entry_sii->key_length = const_cpu_to_le16(0x04);
		idx_entry_sii->flags = const_cpu_to_le16(0x00);
		idx_entry_sii->reserved = const_cpu_to_le16(0x00);
		idx_entry_sii->key.sii.security_id = sds_header->security_id;
		sii_data = (SII_INDEX_DATA*)((u8*)idx_entry_sii +
				le16_to_cpu(idx_entry_sii->data_offset));
		sii_data->hash = sds_header->hash;
		sii_data->security_id = sds_header->security_id;
		sii_data->offset = sds_header->offset;
		sii_data->length = sds_header->length;
		if ((err = insert_index_entry_in_res_dir_index(idx_entry_sdh,
				sdh_size, m, NTFS_INDEX_SDH, 4, AT_UNUSED)))
			break;
		if ((err = insert_index_entry_in_res_dir_index(idx_entry_sii,
				sii_size, m, NTFS_INDEX_SII, 4, AT_UNUSED)))
			break;
		sds_header = (SECURITY_DESCRIPTOR_HEADER*)((u8*)sds_header +
				((le32_to_cpu(sds_header->length) + 15) & ~15));
	}
	free(idx_entry_sdh);
	free(idx_entry_sii);
	return err;
}

/**
 * initialize_quota
 *
 * initialize $Quota with the default quota index-entries.
 */
static int initialize_quota(MFT_RECORD *m)
{
	int o_size, q1_size, q2_size, err, i;
	INDEX_ENTRY *idx_entry_o, *idx_entry_q1, *idx_entry_q2;
	QUOTA_O_INDEX_DATA *idx_entry_o_data;
	QUOTA_CONTROL_ENTRY *idx_entry_q1_data, *idx_entry_q2_data;

	err = 0;
	/* q index entry num 1 */
	q1_size = 0x48;
	idx_entry_q1 = ntfs_calloc(q1_size);
	if (!idx_entry_q1)
		return errno;
	idx_entry_q1->data_offset = const_cpu_to_le16(0x14);
	idx_entry_q1->data_length = const_cpu_to_le16(0x30);
	idx_entry_q1->reservedV = const_cpu_to_le32(0x00);
	idx_entry_q1->length = const_cpu_to_le16(0x48);
	idx_entry_q1->key_length = const_cpu_to_le16(0x04);
	idx_entry_q1->flags = const_cpu_to_le16(0x00);
	idx_entry_q1->reserved = const_cpu_to_le16(0x00);
	idx_entry_q1->key.owner_id = const_cpu_to_le32(0x01);
	idx_entry_q1_data = (QUOTA_CONTROL_ENTRY*)((char*)idx_entry_q1
			+ le16_to_cpu(idx_entry_q1->data_offset));
	idx_entry_q1_data->version = const_cpu_to_le32(0x02);
	idx_entry_q1_data->flags = QUOTA_FLAG_DEFAULT_LIMITS;
	idx_entry_q1_data->bytes_used = const_cpu_to_le64(0x00);
	idx_entry_q1_data->change_time = utc2ntfs(mkntfs_time());
	idx_entry_q1_data->threshold = cpu_to_sle64(-1);
	idx_entry_q1_data->limit = cpu_to_sle64(-1);
	idx_entry_q1_data->exceeded_time = 0;
	err = insert_index_entry_in_res_dir_index(idx_entry_q1, q1_size, m,
			NTFS_INDEX_Q, 2, AT_UNUSED);
	free(idx_entry_q1);
	if (err)
		return err;
	/* q index entry num 2 */
	q2_size = 0x58;
	idx_entry_q2 = ntfs_calloc(q2_size);
	if (!idx_entry_q2)
		return errno;
	idx_entry_q2->data_offset = const_cpu_to_le16(0x14);
	idx_entry_q2->data_length = const_cpu_to_le16(0x40);
	idx_entry_q2->reservedV = const_cpu_to_le32(0x00);
	idx_entry_q2->length = const_cpu_to_le16(0x58);
	idx_entry_q2->key_length = const_cpu_to_le16(0x04);
	idx_entry_q2->flags = const_cpu_to_le16(0x00);
	idx_entry_q2->reserved = const_cpu_to_le16(0x00);
	idx_entry_q2->key.owner_id = QUOTA_FIRST_USER_ID;
	idx_entry_q2_data = (QUOTA_CONTROL_ENTRY*)((char*)idx_entry_q2
			+ le16_to_cpu(idx_entry_q2->data_offset));
	idx_entry_q2_data->version = const_cpu_to_le32(0x02);
	idx_entry_q2_data->flags = QUOTA_FLAG_DEFAULT_LIMITS;
	idx_entry_q2_data->bytes_used = const_cpu_to_le64(0x00);
	idx_entry_q2_data->change_time = utc2ntfs(mkntfs_time());;
	idx_entry_q2_data->threshold = cpu_to_sle64(-1);
	idx_entry_q2_data->limit = cpu_to_sle64(-1);
	idx_entry_q2_data->exceeded_time = 0;
	idx_entry_q2_data->sid.revision = 1;
	idx_entry_q2_data->sid.sub_authority_count = 2;
	for (i = 0; i < 5; i++)
		idx_entry_q2_data->sid.identifier_authority.value[i] = 0;
	idx_entry_q2_data->sid.identifier_authority.value[5] = 0x05;
	idx_entry_q2_data->sid.sub_authority[0] =
			const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
	idx_entry_q2_data->sid.sub_authority[1] =
			const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
	err = insert_index_entry_in_res_dir_index(idx_entry_q2, q2_size, m,
			NTFS_INDEX_Q, 2, AT_UNUSED);
	free(idx_entry_q2);
	if (err)
		return err;
	o_size = 0x28;
	idx_entry_o = ntfs_calloc(o_size);
	if (!idx_entry_o)
		return errno;
	idx_entry_o->data_offset = const_cpu_to_le16(0x20);
	idx_entry_o->data_length = const_cpu_to_le16(0x04);
	idx_entry_o->reservedV = const_cpu_to_le32(0x00);
	idx_entry_o->length = const_cpu_to_le16(0x28);
	idx_entry_o->key_length = const_cpu_to_le16(0x10);
	idx_entry_o->flags = const_cpu_to_le16(0x00);
	idx_entry_o->reserved = const_cpu_to_le16(0x00);
	idx_entry_o->key.sid.revision = 0x01;
	idx_entry_o->key.sid.sub_authority_count = 0x02;
	for (i = 0; i < 5; i++)
		idx_entry_o->key.sid.identifier_authority.value[i] = 0;
	idx_entry_o->key.sid.identifier_authority.value[5] = 0x05;
	idx_entry_o->key.sid.sub_authority[0] =
			const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
	idx_entry_o->key.sid.sub_authority[1] =
			const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
	idx_entry_o_data = (QUOTA_O_INDEX_DATA*)((char*)idx_entry_o
			+ le16_to_cpu(idx_entry_o->data_offset));
	idx_entry_o_data->owner_id  = QUOTA_FIRST_USER_ID;
	/* 20 00 00 00 padding after here on ntfs 3.1. 3.0 is unchecked. */
	idx_entry_o_data->unknown = const_cpu_to_le32(32);
	err = insert_index_entry_in_res_dir_index(idx_entry_o, o_size, m,
			NTFS_INDEX_O, 2, AT_UNUSED);
	free(idx_entry_o);

	return err;
}

/**
 * insert_file_link_in_dir_index
 *
 * Insert the fully completed FILE_NAME_ATTR @file_name which is inside
 * the file with mft reference @file_ref into the index (allocation) block
 * @idx (which belongs to @file_ref's parent directory).
 *
 * Return 0 on success or -errno on error.
 */
static int insert_file_link_in_dir_index(INDEX_BLOCK *idx, leMFT_REF file_ref,
		FILE_NAME_ATTR *file_name, u32 file_name_size)
{
	int err, i;
	INDEX_ENTRY *ie;
	char *index_end;

	/*
	 * Lookup dir entry @file_name in dir @idx to determine correct
	 * insertion location. FIXME: Using a very oversimplified lookup
	 * method which is sufficient for mkntfs but no good whatsoever in
	 * real world scenario. (AIA)
	 */

	index_end = (char*)&idx->index + le32_to_cpu(idx->index.index_length);
	ie = (INDEX_ENTRY*)((char*)&idx->index +
			le32_to_cpu(idx->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	while ((char*)ie < index_end && !(ie->flags & INDEX_ENTRY_END)) {
#if 0
#ifdef DEBUG
		ntfs_log_debug("file_name_attr1->file_name_length = %i\n",
				file_name->file_name_length);
		if (file_name->file_name_length) {
			char *__buf = NULL;
			i = ntfs_ucstombs((ntfschar*)&file_name->file_name,
				file_name->file_name_length, &__buf, 0);
			if (i < 0)
				ntfs_log_debug("Name contains non-displayable "
						"Unicode characters.\n");
			ntfs_log_debug("file_name_attr1->file_name = %s\n",
					__buf);
			free(__buf);
		}
		ntfs_log_debug("file_name_attr2->file_name_length = %i\n",
				ie->key.file_name.file_name_length);
		if (ie->key.file_name.file_name_length) {
			char *__buf = NULL;
			i = ntfs_ucstombs(ie->key.file_name.file_name,
				ie->key.file_name.file_name_length + 1, &__buf,
				0);
			if (i < 0)
				ntfs_log_debug("Name contains non-displayable "
						"Unicode characters.\n");
			ntfs_log_debug("file_name_attr2->file_name = %s\n",
					__buf);
			free(__buf);
		}
#endif
#endif
		i = ntfs_file_values_compare(file_name,
				(FILE_NAME_ATTR*)&ie->key.file_name, 1,
				IGNORE_CASE, g_vol->upcase, g_vol->upcase_len);
		/*
		 * If @file_name collates before ie->key.file_name, there is no
		 * matching index entry.
		 */
		if (i == -1)
			break;
		/* If file names are not equal, continue search. */
		if (i)
			goto do_next;
		/* File names are equal when compared ignoring case. */
		/*
		 * If BOTH file names are in the POSIX namespace, do a case
		 * sensitive comparison as well. Otherwise the names match so
		 * we return -EEXIST. FIXME: There are problems with this in a
		 * real world scenario, when one is POSIX and one isn't, but
		 * fine for mkntfs where we don't use POSIX namespace at all
		 * and hence this following code is luxury. (AIA)
		 */
		if (file_name->file_name_type != FILE_NAME_POSIX ||
		    ie->key.file_name.file_name_type != FILE_NAME_POSIX)
			return -EEXIST;
		i = ntfs_file_values_compare(file_name,
				(FILE_NAME_ATTR*)&ie->key.file_name, 1,
				CASE_SENSITIVE, g_vol->upcase,
				g_vol->upcase_len);
		if (i == -1)
			break;
		/* Complete match. Bugger. Can't insert. */
		if (!i)
			return -EEXIST;
do_next:
#ifdef DEBUG
		/* Next entry. */
		if (!ie->length) {
			ntfs_log_debug("BUG: ie->length is zero, breaking out "
					"of loop.\n");
			break;
		}
#endif
		ie = (INDEX_ENTRY*)((char*)ie + le16_to_cpu(ie->length));
	};
	i = (sizeof(INDEX_ENTRY_HEADER) + file_name_size + 7) & ~7;
	err = make_room_for_index_entry_in_index_block(idx, ie, i);
	if (err) {
		ntfs_log_error("make_room_for_index_entry_in_index_block "
				"failed: %s\n", strerror(-err));
		return err;
	}
	/* Create entry in place and copy file name attribute value. */
	ie->indexed_file = file_ref;
	ie->length = cpu_to_le16(i);
	ie->key_length = cpu_to_le16(file_name_size);
	ie->flags = cpu_to_le16(0);
	ie->reserved = cpu_to_le16(0);
	memcpy((char*)&ie->key.file_name, (char*)file_name, file_name_size);
	return 0;
}

/**
 * create_hardlink_res
 *
 * Create a file_name_attribute in the mft record @m_file which points to the
 * parent directory with mft reference @ref_parent.
 *
 * Then, insert an index entry with this file_name_attribute in the index
 * root @idx of the index_root attribute of the parent directory.
 *
 * @ref_file is the mft reference of @m_file.
 *
 * Return 0 on success or -errno on error.
 */
static int create_hardlink_res(MFT_RECORD *m_parent, const leMFT_REF ref_parent,
		MFT_RECORD *m_file, const leMFT_REF ref_file,
		const s64 allocated_size, const s64 data_size,
		const FILE_ATTR_FLAGS flags, const u16 packed_ea_size,
		const u32 reparse_point_tag, const char *file_name,
		const FILE_NAME_TYPE_FLAGS file_name_type)
{
	FILE_NAME_ATTR *fn;
	int i, fn_size, idx_size;
	INDEX_ENTRY *idx_entry_new;
	ntfschar *uname;

	/* Create the file_name attribute. */
	i = (strlen(file_name) + 1) * sizeof(ntfschar);
	fn_size = sizeof(FILE_NAME_ATTR) + i;
	fn = ntfs_malloc(fn_size);
	if (!fn)
		return -errno;
	fn->parent_directory = ref_parent;
	/* FIXME: copy the creation_time from the std info */
	fn->creation_time = utc2ntfs(mkntfs_time());
	fn->last_data_change_time = fn->creation_time;
	fn->last_mft_change_time = fn->creation_time;
	fn->last_access_time = fn->creation_time;
	fn->allocated_size = cpu_to_sle64(allocated_size);
	fn->data_size = cpu_to_sle64(data_size);
	fn->file_attributes = flags;
	/* These are in a union so can't have both. */
	if (packed_ea_size && reparse_point_tag) {
		free(fn);
		return -EINVAL;
	}
	if (packed_ea_size) {
		free(fn);
		return -EINVAL;
	}
	if (packed_ea_size) {
		fn->packed_ea_size = cpu_to_le16(packed_ea_size);
		fn->reserved = cpu_to_le16(0);
	} else {
		fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
	}
	fn->file_name_type = file_name_type;
	uname = fn->file_name;
	i = ntfs_mbstoucs(file_name, &uname, i);
	if (i < 1) {
		free(fn);
		return -EINVAL;
	}
	if (i > 0xff) {
		free(fn);
		return -ENAMETOOLONG;
	}
	/* No terminating null in file names. */
	fn->file_name_length = i;
	fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
	/* Increment the link count of @m_file. */
	i = le16_to_cpu(m_file->link_count);
	if (i == 0xffff) {
		ntfs_log_error("Too many hardlinks present already.\n");
		free(fn);
		return -EINVAL;
	}
	m_file->link_count = cpu_to_le16(i + 1);
	/* Add the file_name to @m_file. */
	i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0, 0,
			0, RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
	if (i < 0) {
		ntfs_log_error("create_hardlink failed adding file name "
				"attribute: %s\n", strerror(-i));
		free(fn);
		/* Undo link count increment. */
		m_file->link_count = cpu_to_le16(
				le16_to_cpu(m_file->link_count) - 1);
		return i;
	}
	/* Insert the index entry for file_name in @idx. */
	idx_size = (fn_size + 7)  & ~7;
	idx_entry_new = ntfs_calloc(idx_size + 0x10);
	if (!idx_entry_new)
		return -errno;
	idx_entry_new->indexed_file = ref_file;
	idx_entry_new->length = cpu_to_le16(idx_size + 0x10);
	idx_entry_new->key_length = cpu_to_le16(fn_size);
	memcpy((u8*)idx_entry_new + 0x10, (u8*)fn, fn_size);
	i = insert_index_entry_in_res_dir_index(idx_entry_new, idx_size + 0x10,
			m_parent, NTFS_INDEX_I30, 4, AT_FILE_NAME);
	if (i < 0) {
		ntfs_log_error("create_hardlink failed inserting index entry: "
				"%s\n", strerror(-i));
		/* FIXME: Remove the file name attribute from @m_file. */
		free(idx_entry_new);
		free(fn);
		/* Undo link count increment. */
		m_file->link_count = cpu_to_le16(
				le16_to_cpu(m_file->link_count) - 1);
		return i;
	}
	free(idx_entry_new);
	free(fn);
	return 0;
}

/**
 * create_hardlink
 *
 * Create a file_name_attribute in the mft record @m_file which points to the
 * parent directory with mft reference @ref_parent.
 *
 * Then, insert an index entry with this file_name_attribute in the index
 * block @idx of the index allocation attribute of the parent directory.
 *
 * @ref_file is the mft reference of @m_file.
 *
 * Return 0 on success or -errno on error.
 */
static int create_hardlink(INDEX_BLOCK *idx, const leMFT_REF ref_parent,
		MFT_RECORD *m_file, const leMFT_REF ref_file,
		const s64 allocated_size, const s64 data_size,
		const FILE_ATTR_FLAGS flags, const u16 packed_ea_size,
		const u32 reparse_point_tag, const char *file_name,
		const FILE_NAME_TYPE_FLAGS file_name_type)
{
	FILE_NAME_ATTR *fn;
	int i, fn_size;
	ntfschar *uname;

	/* Create the file_name attribute. */
	i = (strlen(file_name) + 1) * sizeof(ntfschar);
	fn_size = sizeof(FILE_NAME_ATTR) + i;
	fn = ntfs_malloc(fn_size);
	if (!fn)
		return -errno;
	fn->parent_directory = ref_parent;
	/* FIXME: Is this correct? Or do we have to copy the creation_time */
	/* from the std info? */
	fn->creation_time = utc2ntfs(mkntfs_time());
	fn->last_data_change_time = fn->creation_time;
	fn->last_mft_change_time = fn->creation_time;
	fn->last_access_time = fn->creation_time;
	fn->allocated_size = cpu_to_sle64(allocated_size);
	fn->data_size = cpu_to_sle64(data_size);
	fn->file_attributes = flags;
	/* These are in a union so can't have both. */
	if (packed_ea_size && reparse_point_tag) {
		free(fn);
		return -EINVAL;
	}
	if (packed_ea_size) {
		fn->packed_ea_size = cpu_to_le16(packed_ea_size);
		fn->reserved = cpu_to_le16(0);
	} else {
		fn->reparse_point_tag = cpu_to_le32(reparse_point_tag);
	}
	fn->file_name_type = file_name_type;
	uname = fn->file_name;
	i = ntfs_mbstoucs(file_name, &uname, i);
	if (i < 1) {
		free(fn);
		return -EINVAL;
	}
	if (i > 0xff) {
		free(fn);
		return -ENAMETOOLONG;
	}
	/* No terminating null in file names. */
	fn->file_name_length = i;
	fn_size = sizeof(FILE_NAME_ATTR) + i * sizeof(ntfschar);
	/* Increment the link count of @m_file. */
	i = le16_to_cpu(m_file->link_count);
	if (i == 0xffff) {
		ntfs_log_error("Too many hardlinks present already.\n");
		free(fn);
		return -EINVAL;
	}
	m_file->link_count = cpu_to_le16(i + 1);
	/* Add the file_name to @m_file. */
	i = insert_resident_attr_in_mft_record(m_file, AT_FILE_NAME, NULL, 0, 0,
			0, RESIDENT_ATTR_IS_INDEXED, (u8*)fn, fn_size);
	if (i < 0) {
		ntfs_log_error("create_hardlink failed adding file name attribute: "
				"%s\n", strerror(-i));
		free(fn);
		/* Undo link count increment. */
		m_file->link_count = cpu_to_le16(
				le16_to_cpu(m_file->link_count) - 1);
		return i;
	}
	/* Insert the index entry for file_name in @idx. */
	i = insert_file_link_in_dir_index(idx, ref_file, fn, fn_size);
	if (i < 0) {
		ntfs_log_error("create_hardlink failed inserting index entry: %s\n",
				strerror(-i));
		/* FIXME: Remove the file name attribute from @m_file. */
		free(fn);
		/* Undo link count increment. */
		m_file->link_count = cpu_to_le16(
				le16_to_cpu(m_file->link_count) - 1);
		return i;
	}
	free(fn);
	return 0;
}

#ifdef ENABLE_UUID

/**
 * index_obj_id_insert
 *
 * Insert an index entry with the key @guid and data pointing to the mft record
 * @ref in the $O index root of the mft record @m (which must be the mft record
 * for $ObjId).
 *
 * Return 0 on success or -errno on error.
 */
static int index_obj_id_insert(MFT_RECORD *m, const GUID *guid,
		const leMFT_REF ref)
{
	INDEX_ENTRY *idx_entry_new;
	int data_ofs, idx_size, err;
	OBJ_ID_INDEX_DATA *oi;

	/*
	 * Insert the index entry for the object id in the index.
	 *
	 * First determine the size of the index entry to be inserted.  This
	 * consists of the index entry header, followed by the index key, i.e.
	 * the GUID, followed by the index data, i.e. OBJ_ID_INDEX_DATA.
	 */
	data_ofs = (sizeof(INDEX_ENTRY_HEADER) + sizeof(GUID) + 7) & ~7;
	idx_size = (data_ofs + sizeof(OBJ_ID_INDEX_DATA) + 7) & ~7;
	idx_entry_new = ntfs_calloc(idx_size);
	if (!idx_entry_new)
		return -errno;
	idx_entry_new->data_offset = cpu_to_le16(data_ofs);
	idx_entry_new->data_length = cpu_to_le16(sizeof(OBJ_ID_INDEX_DATA));
	idx_entry_new->length = cpu_to_le16(idx_size);
	idx_entry_new->key_length = cpu_to_le16(sizeof(GUID));
	idx_entry_new->key.object_id = *guid;
	oi = (OBJ_ID_INDEX_DATA*)((u8*)idx_entry_new + data_ofs);
	oi->mft_reference = ref;
	err = insert_index_entry_in_res_dir_index(idx_entry_new, idx_size, m,
			NTFS_INDEX_O, 2, AT_UNUSED);
	free(idx_entry_new);
	if (err < 0) {
		ntfs_log_error("index_obj_id_insert failed inserting index "
				"entry: %s\n", strerror(-err));
		return err;
	}
	return 0;
}

#endif

/**
 * mkntfs_cleanup
 */
static void mkntfs_cleanup(void)
{
	/* Close the volume */
	if (g_vol) {
		if (g_vol->dev) {
			if (NDevOpen(g_vol->dev) && g_vol->dev->d_ops->close(g_vol->dev))
				ntfs_log_perror("Warning: Could not close %s", g_vol->dev->d_name);
			ntfs_device_free(g_vol->dev);
		}
		free(g_vol->vol_name);
		free(g_vol->attrdef);
		free(g_vol->upcase);
		free(g_vol);
		g_vol = NULL;
	}

	/* Free any memory we've used */
	free(g_bad_blocks);	g_bad_blocks	= NULL;
	free(g_buf);		g_buf		= NULL;
	free(g_index_block);	g_index_block	= NULL;
	free(g_lcn_bitmap);	g_lcn_bitmap	= NULL;
	free(g_mft_bitmap);	g_mft_bitmap	= NULL;
	free(g_rl_bad);		g_rl_bad	= NULL;
	free(g_rl_boot);	g_rl_boot	= NULL;
	free(g_rl_logfile);	g_rl_logfile	= NULL;
	free(g_rl_mft);		g_rl_mft	= NULL;
	free(g_rl_mft_bmp);	g_rl_mft_bmp	= NULL;
	free(g_rl_mftmirr);	g_rl_mftmirr	= NULL;
}


/**
 * mkntfs_open_partition -
 */
static BOOL mkntfs_open_partition(ntfs_volume *vol)
{
	BOOL result = FALSE;
	int i;
	struct stat sbuf;
	unsigned long mnt_flags;

	/*
	 * Allocate and initialize an ntfs device structure and attach it to
	 * the volume.
	 */
	vol->dev = ntfs_device_alloc(opts.dev_name, 0, &ntfs_device_default_io_ops, NULL);
	if (!vol->dev) {
		ntfs_log_perror("Could not create device");
		goto done;
	}

	/* Open the device for reading or reading and writing. */
	if (opts.no_action) {
		ntfs_log_quiet("Running in READ-ONLY mode!\n");
		i = O_RDONLY;
	} else {
		i = O_RDWR;
	}
	if (vol->dev->d_ops->open(vol->dev, i)) {
		if (errno == ENOENT)
			ntfs_log_error("The device doesn't exist; did you specify it correctly?\n");
		else
			ntfs_log_perror("Could not open %s", vol->dev->d_name);
		goto done;
	}
	/* Verify we are dealing with a block device. */
	if (vol->dev->d_ops->stat(vol->dev, &sbuf)) {
		ntfs_log_perror("Error getting information about %s", vol->dev->d_name);
		goto done;
	}

	if (!S_ISBLK(sbuf.st_mode)) {
		ntfs_log_error("%s is not a block device.\n", vol->dev->d_name);
		if (!opts.force) {
			ntfs_log_error("Refusing to make a filesystem here!\n");
			goto done;
		}
		if (!opts.num_sectors) {
			if (!sbuf.st_size && !sbuf.st_blocks) {
				ntfs_log_error("You must specify the number of sectors.\n");
				goto done;
			}
			if (opts.sector_size) {
				if (sbuf.st_size)
					opts.num_sectors = sbuf.st_size / opts.sector_size;
				else
					opts.num_sectors = ((s64)sbuf.st_blocks << 9) / opts.sector_size;
			} else {
				if (sbuf.st_size)
					opts.num_sectors = sbuf.st_size / 512;
				else
					opts.num_sectors = sbuf.st_blocks;
				opts.sector_size = 512;
			}
		}
		ntfs_log_warning("mkntfs forced anyway.\n");
#ifdef HAVE_LINUX_MAJOR_H
	} else if ((IDE_DISK_MAJOR(MAJOR(sbuf.st_rdev)) &&
			MINOR(sbuf.st_rdev) % 64 == 0) ||
			(SCSI_DISK_MAJOR(MAJOR(sbuf.st_rdev)) &&
			MINOR(sbuf.st_rdev) % 16 == 0)) {
		ntfs_log_error("%s is entire device, not just one partition.\n", vol->dev->d_name);
		if (!opts.force) {
			ntfs_log_error("Refusing to make a filesystem here!\n");
			goto done;
		}
		ntfs_log_warning("mkntfs forced anyway.\n");
#endif
	}
	/* Make sure the file system is not mounted. */
	if (ntfs_check_if_mounted(vol->dev->d_name, &mnt_flags)) {
		ntfs_log_perror("Failed to determine whether %s is mounted", vol->dev->d_name);
	} else if (mnt_flags & NTFS_MF_MOUNTED) {
		ntfs_log_error("%s is mounted.\n", vol->dev->d_name);
		if (!opts.force) {
			ntfs_log_error("Refusing to make a filesystem here!\n");
			goto done;
		}
		ntfs_log_warning("mkntfs forced anyway. Hope /etc/mtab is incorrect.\n");
	}
	result = TRUE;
done:
	return result;
}

/**
 * mkntfs_get_page_size - detect the system's memory page size.
 */
static long mkntfs_get_page_size(void)
{
	long page_size;
#ifdef _SC_PAGESIZE
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0)
#endif
#ifdef _SC_PAGE_SIZE
		page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size < 0)
#endif
	{
		ntfs_log_warning("Failed to determine system page size.  "
				"Assuming safe default of 4096 bytes.\n");
		return 4096;
	}
	ntfs_log_debug("System page size is %li bytes.\n", page_size);
	return page_size;
}

/**
 * mkntfs_override_vol_params -
 */
static BOOL mkntfs_override_vol_params(ntfs_volume *vol)
{
	s64 volume_size;
	long page_size;
	int i;
	BOOL winboot = TRUE;

	/* If user didn't specify the sector size, determine it now. */
	if (opts.sector_size < 0) {
		opts.sector_size = ntfs_device_sector_size_get(vol->dev);
		if (opts.sector_size < 0) {
			ntfs_log_warning("The sector size was not specified "
				"for %s and it could not be obtained "
				"automatically.  It has been set to 512 "
				"bytes.\n", vol->dev->d_name);
			opts.sector_size = 512;
		}
	}
	/* Validate sector size. */
	if ((opts.sector_size - 1) & opts.sector_size) {
		ntfs_log_error("The sector size is invalid.  It must be a "
			"power of two, e.g. 512, 1024.\n");
		return FALSE;
	}
	if (opts.sector_size < 256 || opts.sector_size > 4096) {
		ntfs_log_error("The sector size is invalid.  The minimum size "
			"is 256 bytes and the maximum is 4096 bytes.\n");
		return FALSE;
	}
	ntfs_log_debug("sector size = %ld bytes\n", opts.sector_size);
	/* Now set the device block size to the sector size. */
	if (ntfs_device_block_size_set(vol->dev, opts.sector_size))
		ntfs_log_debug("Failed to set the device block size to the "
				"sector size.  This may cause problems when "
				"creating the backup boot sector and also may "
				"affect performance but should be harmless "
				"otherwise.  Error: %s\n", strerror(errno));
	/* If user didn't specify the number of sectors, determine it now. */
	if (opts.num_sectors < 0) {
		opts.num_sectors = ntfs_device_size_get(vol->dev,
				opts.sector_size);
		if (opts.num_sectors <= 0) {
			ntfs_log_error("Couldn't determine the size of %s.  "
				"Please specify the number of sectors "
				"manually.\n", vol->dev->d_name);
			return FALSE;
		}
	}
	ntfs_log_debug("number of sectors = %lld (0x%llx)\n", opts.num_sectors,
			opts.num_sectors);
	/*
	 * Reserve the last sector for the backup boot sector unless the
	 * sector size is less than 512 bytes in which case reserve 512 bytes
	 * worth of sectors.
	 */
	i = 1;
	if (opts.sector_size < 512)
		i = 512 / opts.sector_size;
	opts.num_sectors -= i;
	/* If user didn't specify the partition start sector, determine it. */
	if (opts.part_start_sect < 0) {
		opts.part_start_sect = ntfs_device_partition_start_sector_get(
				vol->dev);
		if (opts.part_start_sect < 0) {
			ntfs_log_warning("The partition start sector was not "
				"specified for %s and it could not be obtained "
				"automatically.  It has been set to 0.\n",
				vol->dev->d_name);
			opts.part_start_sect = 0;
			winboot = FALSE;
		} else if (opts.part_start_sect >> 32) {
			ntfs_log_warning("The partition start sector specified "
				"for %s and the automatically determined value "
				"is too large.  It has been set to 0.\n",
				vol->dev->d_name);
			opts.part_start_sect = 0;
			winboot = FALSE;
		}
	} else if (opts.part_start_sect >> 32) {
		ntfs_log_error("Invalid partition start sector.  Maximum is "
			"4294967295 (2^32-1).\n");
		return FALSE;
	}
	/* If user didn't specify the sectors per track, determine it now. */
	if (opts.sectors_per_track < 0) {
		opts.sectors_per_track = ntfs_device_sectors_per_track_get(
				vol->dev);
		if (opts.sectors_per_track < 0) {
			ntfs_log_warning("The number of sectors per track was "
				"not specified for %s and it could not be "
				"obtained automatically.  It has been set to "
				"0.\n", vol->dev->d_name);
			opts.sectors_per_track = 0;
			winboot = FALSE;
		} else if (opts.sectors_per_track > 65535) {
			ntfs_log_warning("The number of sectors per track was "
				"not specified for %s and the automatically "
				"determined value is too large.  It has been "
				"set to 0.\n", vol->dev->d_name);
			opts.sectors_per_track = 0;
			winboot = FALSE;
		}
	} else if (opts.sectors_per_track > 65535) {
		ntfs_log_error("Invalid number of sectors per track.  Maximum "
			"is 65535.\n");
		return FALSE;
	}
	/* If user didn't specify the number of heads, determine it now. */
	if (opts.heads < 0) {
		opts.heads = ntfs_device_heads_get(vol->dev);
		if (opts.heads < 0) {
			ntfs_log_warning("The number of heads was not "
				"specified for %s and it could not be obtained "
				"automatically.  It has been set to 0.\n",
				vol->dev->d_name);
			opts.heads = 0;
			winboot = FALSE;
		} else if (opts.heads > 65535) {
			ntfs_log_warning("The number of heads was not "
				"specified for %s and the automatically "
				"determined value is too large.  It has been "
				"set to 0.\n", vol->dev->d_name);
			opts.heads = 0;
			winboot = FALSE;
		}
	} else if (opts.heads > 65535) {
		ntfs_log_error("Invalid number of heads.  Maximum is 65535.\n");
		return FALSE;
	}
	volume_size = opts.num_sectors * opts.sector_size;
	/* Validate volume size. */
	if (volume_size < (1 << 20)) {			/* 1MiB */
		ntfs_log_error("Device is too small (%llikiB).  Minimum NTFS "
				"volume size is 1MiB.\n", volume_size / 1024);
		return FALSE;
	}
	ntfs_log_debug("volume size = %llikiB\n", volume_size / 1024);
	/* If user didn't specify the cluster size, determine it now. */
	if (!vol->cluster_size) {
		/*
		 * Windows Vista always uses 4096 bytes as the default cluster
		 * size regardless of the volume size so we do it, too.
		 */
		vol->cluster_size = 4096;
		/* For small volumes on devices with large sector sizes. */
		if (vol->cluster_size < (u32)opts.sector_size)
			vol->cluster_size = opts.sector_size;
		/*
		 * For huge volumes, grow the cluster size until the number of
		 * clusters fits into 32 bits or the cluster size exceeds the
		 * maximum limit of 64kiB.
		 */
		while (volume_size >> (ffs(vol->cluster_size) - 1 + 32)) {
			vol->cluster_size <<= 1;
			if (vol->cluster_size > 65535) {
				ntfs_log_error("Device is too large to hold an "
						"NTFS volume (maximum size is "
						"256TiB).\n");
				return FALSE;
			}
		}
		ntfs_log_quiet("Cluster size has been automatically set to %u "
				"bytes.\n", (unsigned)vol->cluster_size);
	}
	/* Validate cluster size. */
	if (vol->cluster_size & (vol->cluster_size - 1)) {
		ntfs_log_error("The cluster size is invalid.  It must be a "
				"power of two, e.g. 1024, 4096.\n");
		return FALSE;
	}
	if (vol->cluster_size < (u32)opts.sector_size) {
		ntfs_log_error("The cluster size is invalid.  It must be equal "
				"to, or larger than, the sector size.\n");
		return FALSE;
	}
	if (vol->cluster_size > 128 * (u32)opts.sector_size) {
		ntfs_log_error("The cluster size is invalid.  It cannot be "
				"more that 128 times the size of the sector "
				"size.\n");
		return FALSE;
	}
	if (vol->cluster_size > 65536) {
		ntfs_log_error("The cluster size is invalid.  The maximum "
			"cluster size is 65536 bytes (64kiB).\n");
		return FALSE;
	}
	vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
	ntfs_log_debug("cluster size = %u bytes\n",
			(unsigned int)vol->cluster_size);
	if (vol->cluster_size > 4096) {
		if (opts.enable_compression) {
			if (!opts.force) {
				ntfs_log_error("Windows cannot use compression "
						"when the cluster size is "
						"larger than 4096 bytes.\n");
				return FALSE;
			}
			opts.enable_compression = 0;
		}
		ntfs_log_warning("Windows cannot use compression when the "
				"cluster size is larger than 4096 bytes.  "
				"Compression has been disabled for this "
				"volume.\n");
	}
	vol->nr_clusters = volume_size / vol->cluster_size;
	/*
	 * Check the cluster_size and num_sectors for consistency with
	 * sector_size and num_sectors. And check both of these for consistency
	 * with volume_size.
	 */
	if ((vol->nr_clusters != ((opts.num_sectors * opts.sector_size) /
			vol->cluster_size) ||
			(volume_size / opts.sector_size) != opts.num_sectors ||
			(volume_size / vol->cluster_size) !=
			vol->nr_clusters)) {
		/* XXX is this code reachable? */
		ntfs_log_error("Illegal combination of volume/cluster/sector "
				"size and/or cluster/sector number.\n");
		return FALSE;
	}
	ntfs_log_debug("number of clusters = %llu (0x%llx)\n",
			vol->nr_clusters, vol->nr_clusters);
	/* Number of clusters must fit within 32 bits (Win2k limitation). */
	if (vol->nr_clusters >> 32) {
		if (vol->cluster_size >= 65536) {
			ntfs_log_error("Device is too large to hold an NTFS "
					"volume (maximum size is 256TiB).\n");
			return FALSE;
		}
		ntfs_log_error("Number of clusters exceeds 32 bits.  Please "
				"try again with a larger\ncluster size or "
				"leave the cluster size unspecified and the "
				"smallest possible cluster size for the size "
				"of the device will be used.\n");
		return FALSE;
	}
	page_size = mkntfs_get_page_size();
	/*
	 * Set the mft record size.  By default this is 1024 but it has to be
	 * at least as big as a sector and not bigger than a page on the system
	 * or the NTFS kernel driver will not be able to mount the volume.
	 * TODO: The mft record size should be user specifiable just like the
	 * "inode size" can be specified on other Linux/Unix file systems.
	 */
	vol->mft_record_size = 1024;
	if (vol->mft_record_size < (u32)opts.sector_size)
		vol->mft_record_size = opts.sector_size;
	if (vol->mft_record_size > (unsigned long)page_size)
		ntfs_log_warning("Mft record size (%u bytes) exceeds system "
				"page size (%li bytes).  You will not be able "
				"to mount this volume using the NTFS kernel "
				"driver.\n", (unsigned)vol->mft_record_size,
				page_size);
	vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
	ntfs_log_debug("mft record size = %u bytes\n",
			(unsigned)vol->mft_record_size);
	/*
	 * Set the index record size.  By default this is 4096 but it has to be
	 * at least as big as a sector and not bigger than a page on the system
	 * or the NTFS kernel driver will not be able to mount the volume.
	 * FIXME: Should we make the index record size to be user specifiable?
	 */
	vol->indx_record_size = 4096;
	if (vol->indx_record_size < (u32)opts.sector_size)
		vol->indx_record_size = opts.sector_size;
	if (vol->indx_record_size > (unsigned long)page_size)
		ntfs_log_warning("Index record size (%u bytes) exceeds system "
				"page size (%li bytes).  You will not be able "
				"to mount this volume using the NTFS kernel "
				"driver.\n", (unsigned)vol->indx_record_size,
				page_size);
	vol->indx_record_size_bits = ffs(vol->indx_record_size) - 1;
	ntfs_log_debug("index record size = %u bytes\n",
			(unsigned)vol->indx_record_size);
	if (!winboot) {
		ntfs_log_warning("To boot from a device, Windows needs the "
				"'partition start sector', the 'sectors per "
				"track' and the 'number of heads' to be "
				"set.\n");
		ntfs_log_warning("Windows will not be able to boot from this "
				"device.\n");
	}
	return TRUE;
}

/**
 * mkntfs_initialize_bitmaps -
 */
static BOOL mkntfs_initialize_bitmaps(void)
{
	u64 i;
	int mft_bitmap_size;

	/* Determine lcn bitmap byte size and allocate it. */
	g_lcn_bitmap_byte_size = (g_vol->nr_clusters + 7) >> 3;
	/* Needs to be multiple of 8 bytes. */
	g_lcn_bitmap_byte_size = (g_lcn_bitmap_byte_size + 7) & ~7;
	i = (g_lcn_bitmap_byte_size + g_vol->cluster_size - 1) &
			~(g_vol->cluster_size - 1);
	ntfs_log_debug("g_lcn_bitmap_byte_size = %i, allocated = %llu\n",
			g_lcn_bitmap_byte_size, i);
	g_lcn_bitmap = ntfs_calloc(g_lcn_bitmap_byte_size);
	if (!g_lcn_bitmap)
		return FALSE;
	/*
	 * $Bitmap can overlap the end of the volume. Any bits in this region
	 * must be set. This region also encompasses the backup boot sector.
	 */
	for (i = g_vol->nr_clusters; i < (u64)g_lcn_bitmap_byte_size << 3; i++)
		ntfs_bit_set(g_lcn_bitmap, i, 1);
	/*
	 * Mft size is 27 (NTFS 3.0+) mft records or one cluster, whichever is
	 * bigger.
	 */
	g_mft_size = 27;
	g_mft_size *= g_vol->mft_record_size;
	if (g_mft_size < (s32)g_vol->cluster_size)
		g_mft_size = g_vol->cluster_size;
	ntfs_log_debug("MFT size = %i (0x%x) bytes\n", g_mft_size, g_mft_size);
	/* Determine mft bitmap size and allocate it. */
	mft_bitmap_size = g_mft_size / g_vol->mft_record_size;
	/* Convert to bytes, at least one. */
	g_mft_bitmap_byte_size = (mft_bitmap_size + 7) >> 3;
	/* Mft bitmap is allocated in multiples of 8 bytes. */
	g_mft_bitmap_byte_size = (g_mft_bitmap_byte_size + 7) & ~7;
	ntfs_log_debug("mft_bitmap_size = %i, g_mft_bitmap_byte_size = %i\n",
			mft_bitmap_size, g_mft_bitmap_byte_size);
	g_mft_bitmap = ntfs_calloc(g_mft_bitmap_byte_size);
	if (!g_mft_bitmap)
		return FALSE;
	/* Create runlist for mft bitmap. */
	g_rl_mft_bmp = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_mft_bmp)
		return FALSE;

	g_rl_mft_bmp[0].vcn = 0LL;
	/* Mft bitmap is right after $Boot's data. */
	i = (8192 + g_vol->cluster_size - 1) / g_vol->cluster_size;
	g_rl_mft_bmp[0].lcn = i;
	/*
	 * Size is always one cluster, even though valid data size and
	 * initialized data size are only 8 bytes.
	 */
	g_rl_mft_bmp[1].vcn = 1LL;
	g_rl_mft_bmp[0].length = 1LL;
	g_rl_mft_bmp[1].lcn = -1LL;
	g_rl_mft_bmp[1].length = 0LL;
	/* Allocate cluster for mft bitmap. */
	ntfs_bit_set(g_lcn_bitmap, i, 1);
	return TRUE;
}

/**
 * mkntfs_initialize_rl_mft -
 */
static BOOL mkntfs_initialize_rl_mft(void)
{
	int i, j;

	/* If user didn't specify the mft lcn, determine it now. */
	if (!g_mft_lcn) {
		/*
		 * We start at the higher value out of 16kiB and just after the
		 * mft bitmap.
		 */
		g_mft_lcn = g_rl_mft_bmp[0].lcn + g_rl_mft_bmp[0].length;
		if (g_mft_lcn * g_vol->cluster_size < 16 * 1024)
			g_mft_lcn = (16 * 1024 + g_vol->cluster_size - 1) /
					g_vol->cluster_size;
	}
	ntfs_log_debug("$MFT logical cluster number = 0x%llx\n", g_mft_lcn);
	/* Determine MFT zone size. */
	g_mft_zone_end = g_vol->nr_clusters;
	switch (opts.mft_zone_multiplier) {  /* % of volume size in clusters */
	case 4:
		g_mft_zone_end = g_mft_zone_end >> 1;	/* 50%   */
		break;
	case 3:
		g_mft_zone_end = g_mft_zone_end * 3 >> 3;/* 37.5% */
		break;
	case 2:
		g_mft_zone_end = g_mft_zone_end >> 2;	/* 25%   */
		break;
	case 1:
	default:
		g_mft_zone_end = g_mft_zone_end >> 3;	/* 12.5% */
		break;
	}
	ntfs_log_debug("MFT zone size = %lldkiB\n", g_mft_zone_end <<
			g_vol->cluster_size_bits >> 10 /* >> 10 == / 1024 */);
	/*
	 * The mft zone begins with the mft data attribute, not at the beginning
	 * of the device.
	 */
	g_mft_zone_end += g_mft_lcn;
	/* Create runlist for mft. */
	g_rl_mft = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_mft)
		return FALSE;

	g_rl_mft[0].vcn = 0LL;
	g_rl_mft[0].lcn = g_mft_lcn;
	/* rounded up division by cluster size */
	j = (g_mft_size + g_vol->cluster_size - 1) / g_vol->cluster_size;
	g_rl_mft[1].vcn = j;
	g_rl_mft[0].length = j;
	g_rl_mft[1].lcn = -1LL;
	g_rl_mft[1].length = 0LL;
	/* Allocate clusters for mft. */
	for (i = 0; i < j; i++)
		ntfs_bit_set(g_lcn_bitmap, g_mft_lcn + i, 1);
	/* Determine mftmirr_lcn (middle of volume). */
	g_mftmirr_lcn = (opts.num_sectors * opts.sector_size >> 1)
			/ g_vol->cluster_size;
	ntfs_log_debug("$MFTMirr logical cluster number = 0x%llx\n",
			g_mftmirr_lcn);
	/* Create runlist for mft mirror. */
	g_rl_mftmirr = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_mftmirr)
		return FALSE;

	g_rl_mftmirr[0].vcn = 0LL;
	g_rl_mftmirr[0].lcn = g_mftmirr_lcn;
	/*
	 * The mft mirror is either 4kb (the first four records) or one cluster
	 * in size, which ever is bigger. In either case, it contains a
	 * byte-for-byte identical copy of the beginning of the mft (i.e. either
	 * the first four records (4kb) or the first cluster worth of records,
	 * whichever is bigger).
	 */
	j = (4 * g_vol->mft_record_size + g_vol->cluster_size - 1) / g_vol->cluster_size;
	g_rl_mftmirr[1].vcn = j;
	g_rl_mftmirr[0].length = j;
	g_rl_mftmirr[1].lcn = -1LL;
	g_rl_mftmirr[1].length = 0LL;
	/* Allocate clusters for mft mirror. */
	for (i = 0; i < j; i++)
		ntfs_bit_set(g_lcn_bitmap, g_mftmirr_lcn + i, 1);
	g_logfile_lcn = g_mftmirr_lcn + j;
	ntfs_log_debug("$LogFile logical cluster number = 0x%llx\n",
			g_logfile_lcn);
	return TRUE;
}

/**
 * mkntfs_initialize_rl_logfile -
 */
static BOOL mkntfs_initialize_rl_logfile(void)
{
	int i, j;
	u64 volume_size;

	/* Create runlist for log file. */
	g_rl_logfile = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_logfile)
		return FALSE;


	volume_size = g_vol->nr_clusters << g_vol->cluster_size_bits;

	g_rl_logfile[0].vcn = 0LL;
	g_rl_logfile[0].lcn = g_logfile_lcn;
	/*
	 * Determine logfile_size from volume_size (rounded up to a cluster),
	 * making sure it does not overflow the end of the volume.
	 */
	if (volume_size < 2048LL * 1024)		/* < 2MiB	*/
		g_logfile_size = 256LL * 1024;		/*   -> 256kiB	*/
	else if (volume_size < 4000000LL)		/* < 4MB	*/
		g_logfile_size = 512LL * 1024;		/*   -> 512kiB	*/
	else if (volume_size <= 200LL * 1024 * 1024)	/* < 200MiB	*/
		g_logfile_size = 2048LL * 1024;		/*   -> 2MiB	*/
	else	{
		/*
		 * FIXME: The $LogFile size is 64 MiB upwards from 12GiB but
		 * the "200" divider below apparently approximates "100" or
		 * some other value as the volume size decreases. For example:
		 *      Volume size   LogFile size    Ratio
		 *	  8799808        46048       191.100
		 *	  8603248        45072       190.877
		 *	  7341704        38768       189.375
		 *	  6144828        32784       187.433
		 *	  4192932        23024       182.111
		 */
		if (volume_size >= 12LL << 30)		/* > 12GiB	*/
			g_logfile_size = 64 << 20;	/*   -> 64MiB	*/
		else
			g_logfile_size = (volume_size / 200) &
					~(g_vol->cluster_size - 1);
	}
	j = g_logfile_size / g_vol->cluster_size;
	while (g_rl_logfile[0].lcn + j >= g_vol->nr_clusters) {
		/*
		 * $Logfile would overflow volume. Need to make it smaller than
		 * the standard size. It's ok as we are creating a non-standard
		 * volume anyway if it is that small.
		 */
		g_logfile_size >>= 1;
		j = g_logfile_size / g_vol->cluster_size;
	}
	g_logfile_size = (g_logfile_size + g_vol->cluster_size - 1) &
			~(g_vol->cluster_size - 1);
	ntfs_log_debug("$LogFile (journal) size = %ikiB\n",
			g_logfile_size / 1024);
	/*
	 * FIXME: The 256kiB limit is arbitrary. Should find out what the real
	 * minimum requirement for Windows is so it doesn't blue screen.
	 */
	if (g_logfile_size < 256 << 10) {
		ntfs_log_error("$LogFile would be created with invalid size. "
				"This is not allowed as it would cause Windows "
				"to blue screen and during boot.\n");
		return FALSE;
	}
	g_rl_logfile[1].vcn = j;
	g_rl_logfile[0].length = j;
	g_rl_logfile[1].lcn = -1LL;
	g_rl_logfile[1].length = 0LL;
	/* Allocate clusters for log file. */
	for (i = 0; i < j; i++)
		ntfs_bit_set(g_lcn_bitmap, g_logfile_lcn + i, 1);
	return TRUE;
}

/**
 * mkntfs_initialize_rl_boot -
 */
static BOOL mkntfs_initialize_rl_boot(void)
{
	int i, j;
	/* Create runlist for $Boot. */
	g_rl_boot = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_boot)
		return FALSE;

	g_rl_boot[0].vcn = 0LL;
	g_rl_boot[0].lcn = 0LL;
	/*
	 * $Boot is always 8192 (0x2000) bytes or 1 cluster, whichever is
	 * bigger.
	 */
	j = (8192 + g_vol->cluster_size - 1) / g_vol->cluster_size;
	g_rl_boot[1].vcn = j;
	g_rl_boot[0].length = j;
	g_rl_boot[1].lcn = -1LL;
	g_rl_boot[1].length = 0LL;
	/* Allocate clusters for $Boot. */
	for (i = 0; i < j; i++)
		ntfs_bit_set(g_lcn_bitmap, 0LL + i, 1);
	return TRUE;
}

/**
 * mkntfs_initialize_rl_bad -
 */
static BOOL mkntfs_initialize_rl_bad(void)
{
	/* Create runlist for $BadClus, $DATA named stream $Bad. */
	g_rl_bad = ntfs_malloc(2 * sizeof(runlist));
	if (!g_rl_bad)
		return FALSE;

	g_rl_bad[0].vcn = 0LL;
	g_rl_bad[0].lcn = -1LL;
	/*
	 * $BadClus named stream $Bad contains the whole volume as a single
	 * sparse runlist entry.
	 */
	g_rl_bad[1].vcn = g_vol->nr_clusters;
	g_rl_bad[0].length = g_vol->nr_clusters;
	g_rl_bad[1].lcn = -1LL;
	g_rl_bad[1].length = 0LL;

	/* TODO: Mark bad blocks as such. */
	return TRUE;
}

/**
 * mkntfs_fill_device_with_zeroes -
 */
static BOOL mkntfs_fill_device_with_zeroes(void)
{
	/*
	 * If not quick format, fill the device with 0s.
	 * FIXME: Except bad blocks! (AIA)
	 */
	int i;
	ssize_t bw;
	unsigned long long position;
	float progress_inc = (float)g_vol->nr_clusters / 100;
	u64 volume_size;

	volume_size = g_vol->nr_clusters << g_vol->cluster_size_bits;

	ntfs_log_progress("Initializing device with zeroes:   0%%");
	for (position = 0; position < (unsigned long long)g_vol->nr_clusters;
			position++) {
		if (!(position % (int)(progress_inc+1))) {
			ntfs_log_progress("\b\b\b\b%3.0f%%", position /
					progress_inc);
		}
		bw = mkntfs_write(g_vol->dev, g_buf, g_vol->cluster_size);
		if (bw != (ssize_t)g_vol->cluster_size) {
			if (bw != -1 || errno != EIO) {
				ntfs_log_error("This should not happen.\n");
				return FALSE;
			}
			if (!position) {
				ntfs_log_error("Error: Cluster zero is bad. "
					"Cannot create NTFS file "
					"system.\n");
				return FALSE;
			}
			/* Add the baddie to our bad blocks list. */
			if (!append_to_bad_blocks(position))
				return FALSE;
			ntfs_log_quiet("\nFound bad cluster (%lld). Adding to "
				"list of bad blocks.\nInitializing "
				"device with zeroes: %3.0f%%", position,
				position / progress_inc);
			/* Seek to next cluster. */
			g_vol->dev->d_ops->seek(g_vol->dev,
					((off_t)position + 1) *
					g_vol->cluster_size, SEEK_SET);
		}
	}
	ntfs_log_progress("\b\b\b\b100%%");
	position = (volume_size & (g_vol->cluster_size - 1)) /
			opts.sector_size;
	for (i = 0; (unsigned long)i < position; i++) {
		bw = mkntfs_write(g_vol->dev, g_buf, opts.sector_size);
		if (bw != opts.sector_size) {
			if (bw != -1 || errno != EIO) {
				ntfs_log_error("This should not happen.\n");
				return FALSE;
			} else if (i + 1ull == position) {
				ntfs_log_error("Error: Bad cluster found in "
					"location reserved for system "
					"file $Boot.\n");
				return FALSE;
			}
			/* Seek to next sector. */
			g_vol->dev->d_ops->seek(g_vol->dev,
					opts.sector_size, SEEK_CUR);
		}
	}
	ntfs_log_progress(" - Done.\n");
	return TRUE;
}

/**
 * mkntfs_sync_index_record
 *
 * (ERSO) made a function out of this, but the reason for doing that
 * disappeared during coding....
 */
static BOOL mkntfs_sync_index_record(INDEX_ALLOCATION* idx, MFT_RECORD* m,
		ntfschar* name, u32 name_len)
{
	int i, err;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	long long lw;
	runlist	*rl_index = NULL;

	i = 5 * sizeof(ntfschar);
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_perror("Failed to allocate attribute search context");
		return FALSE;
	}
	/* FIXME: This should be IGNORE_CASE! */
	if (mkntfs_attr_lookup(AT_INDEX_ALLOCATION, name, name_len, 0, 0,
			NULL, 0, ctx)) {
		ntfs_attr_put_search_ctx(ctx);
		ntfs_log_error("BUG: $INDEX_ALLOCATION attribute not found.\n");
		return FALSE;
	}
	a = ctx->attr;
	rl_index = ntfs_mapping_pairs_decompress(g_vol, a, NULL);
	if (!rl_index) {
		ntfs_attr_put_search_ctx(ctx);
		ntfs_log_error("Failed to decompress runlist of $INDEX_ALLOCATION "
				"attribute.\n");
		return FALSE;
	}
	if (sle64_to_cpu(a->initialized_size) < i) {
		ntfs_attr_put_search_ctx(ctx);
		free(rl_index);
		ntfs_log_error("BUG: $INDEX_ALLOCATION attribute too short.\n");
		return FALSE;
	}
	ntfs_attr_put_search_ctx(ctx);
	i = sizeof(INDEX_BLOCK) - sizeof(INDEX_HEADER) +
			le32_to_cpu(idx->index.allocated_size);
	err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)idx, i);
	if (err) {
		free(rl_index);
		ntfs_log_error("ntfs_mst_pre_write_fixup() failed while "
			"syncing index block.\n");
		return FALSE;
	}
	lw = ntfs_rlwrite(g_vol->dev, rl_index, (u8*)idx, i, NULL);
	free(rl_index);
	if (lw != i) {
		ntfs_log_error("Error writing $INDEX_ALLOCATION.\n");
		return FALSE;
	}
	/* No more changes to @idx below here so no need for fixup: */
	/* ntfs_mst_post_write_fixup((NTFS_RECORD*)idx); */
	return TRUE;
}

/**
 * create_file_volume -
 */
static BOOL create_file_volume(MFT_RECORD *m, leMFT_REF root_ref,
		VOLUME_FLAGS fl, const GUID *volume_guid
#ifndef ENABLE_UUID
		__attribute__((unused))
#endif
		)
{
	int i, err;
	u8 *sd;

	ntfs_log_verbose("Creating $Volume (mft record 3)\n");
	m = (MFT_RECORD*)(g_buf + 3 * g_vol->mft_record_size);
	err = create_hardlink(g_index_block, root_ref, m,
			MK_LE_MREF(FILE_Volume, FILE_Volume), 0LL, 0LL,
			FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
			"$Volume", FILE_NAME_WIN32_AND_DOS);
	if (!err) {
		init_system_file_sd(FILE_Volume, &sd, &i);
		err = add_attr_sd(m, sd, i);
	}
	if (!err)
		err = add_attr_data(m, NULL, 0, 0, 0, NULL, 0);
	if (!err)
		err = add_attr_vol_name(m, g_vol->vol_name, g_vol->vol_name ?
				strlen(g_vol->vol_name) : 0);
	if (!err) {
		if (fl & VOLUME_IS_DIRTY)
			ntfs_log_quiet("Setting the volume dirty so check "
					"disk runs on next reboot into "
					"Windows.\n");
		err = add_attr_vol_info(m, fl, g_vol->major_ver,
				g_vol->minor_ver);
	}
#ifdef ENABLE_UUID
	if (!err)
		err = add_attr_object_id(m, volume_guid);
#endif
	if (err < 0) {
		ntfs_log_error("Couldn't create $Volume: %s\n",
				strerror(-err));
		return FALSE;
	}
	return TRUE;
}

/**
 * create_backup_boot_sector
 *
 * Return 0 on success or -1 if it couldn't be created.
 */
static int create_backup_boot_sector(u8 *buff)
{
	const char *s;
	ssize_t bw;
	int size, e;

	ntfs_log_verbose("Creating backup boot sector.\n");
	/*
	 * Write the first max(512, opts.sector_size) bytes from buf to the
	 * last sector, but limit that to 8192 bytes of written data since that
	 * is how big $Boot is (and how big our buffer is)..
	 */
	size = 512;
	if (size < opts.sector_size)
		size = opts.sector_size;
	if (size < opts.cluster_size)
		size = opts.cluster_size;
	if (g_vol->dev->d_ops->seek(g_vol->dev, (opts.num_sectors + 1) *
			opts.sector_size - size, SEEK_SET) == (off_t)-1) {
		ntfs_log_perror("Seek failed");
		goto bb_err;
	}
	if (size > 8192)
		size = 8192;
	bw = mkntfs_write(g_vol->dev, buff, size);
	if (bw == size)
		return 0;
	e = errno;
	if (bw == -1LL)
		s = strerror(e);
	else
		s = "unknown error";
	/* At least some 2.4 kernels return EIO instead of ENOSPC. */
	if (bw != -1LL || (bw == -1LL && e != ENOSPC && e != EIO)) {
		ntfs_log_critical("Couldn't write backup boot sector: %s\n", s);
		return -1;
	}
bb_err:
	ntfs_log_error("Couldn't write backup boot sector. This is due to a "
			"limitation in the\nLinux kernel. This is not a major "
			"problem as Windows check disk will create the\n"
			"backup boot sector when it is run on your next boot "
			"into Windows.\n");
	return -1;
}

/**
 * mkntfs_create_root_structures -
 */
static BOOL mkntfs_create_root_structures(void)
{
	NTFS_BOOT_SECTOR *bs;
	MFT_RECORD *m;
	leMFT_REF root_ref;
	leMFT_REF extend_ref;
	int i;
	int j;
	int err;
	u8 *sd;
	FILE_ATTR_FLAGS extend_flags;
	VOLUME_FLAGS volume_flags = 0;
	int nr_sysfiles;
	u8 *buf_log = NULL;
	int buf_sds_first_size;
	char *buf_sds;

	ntfs_log_quiet("Creating NTFS volume structures.\n");
	nr_sysfiles = 27;
	/*
	 * Setup an empty mft record.  Note, we can just give 0 as the mft
	 * reference as we are creating an NTFS 1.2 volume for which the mft
	 * reference is ignored by ntfs_mft_record_layout().
	 *
	 * Copy the mft record onto all 16 records in the buffer and setup the
	 * sequence numbers of each system file to equal the mft record number
	 * of that file (only for $MFT is the sequence number 1 rather than 0).
	 */
	for (i = 0; i < nr_sysfiles; i++) {
		if (ntfs_mft_record_layout(g_vol, 0, m = (MFT_RECORD *)(g_buf +
				i * g_vol->mft_record_size))) {
			ntfs_log_error("Failed to layout system mft records."
					"\n");
			return FALSE;
		}
		if (i == 0 || i > 23)
			m->sequence_number = cpu_to_le16(1);
		else
			m->sequence_number = cpu_to_le16(i);
	}
	/*
	 * If only one cluster contains all system files then
	 * fill the rest of it with empty, formatted records.
	 */
	if (nr_sysfiles * (s32)g_vol->mft_record_size < g_mft_size) {
		for (i = nr_sysfiles;
		      i * (s32)g_vol->mft_record_size < g_mft_size; i++) {
			m = (MFT_RECORD *)(g_buf + i * g_vol->mft_record_size);
			if (ntfs_mft_record_layout(g_vol, 0, m)) {
				ntfs_log_error("Failed to layout mft record."
						"\n");
				return FALSE;
			}
			m->flags = cpu_to_le16(0);
			m->sequence_number = cpu_to_le16(i);
		}
	}
	/*
	 * Create the 16 system files, adding the system information attribute
	 * to each as well as marking them in use in the mft bitmap.
	 */
	for (i = 0; i < nr_sysfiles; i++) {
		le32 file_attrs;

		m = (MFT_RECORD*)(g_buf + i * g_vol->mft_record_size);
		if (i < 16 || i > 23) {
			m->mft_record_number = cpu_to_le32(i);
			m->flags |= MFT_RECORD_IN_USE;
			ntfs_bit_set(g_mft_bitmap, 0LL + i, 1);
		}
		file_attrs = FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM;
		if (i == FILE_root) {
			file_attrs |= FILE_ATTR_ARCHIVE;
			if (opts.disable_indexing)
				file_attrs |= FILE_ATTR_NOT_CONTENT_INDEXED;
			if (opts.enable_compression)
				file_attrs |= FILE_ATTR_COMPRESSED;
		}
		/* setting specific security_id flag and */
		/* file permissions for ntfs 3.x */
		if (i == 0 || i == 1 || i == 2 || i == 6 || i == 8 ||
				i == 10) {
			add_attr_std_info(m, file_attrs,
				cpu_to_le32(0x0100));
		} else if (i == 9) {
			file_attrs |= FILE_ATTR_VIEW_INDEX_PRESENT;
			add_attr_std_info(m, file_attrs,
				cpu_to_le32(0x0101));
		} else if (i == 11) {
			add_attr_std_info(m, file_attrs,
				cpu_to_le32(0x0101));
		} else if (i == 24 || i == 25 || i == 26) {
			file_attrs |= FILE_ATTR_ARCHIVE;
			file_attrs |= FILE_ATTR_VIEW_INDEX_PRESENT;
			add_attr_std_info(m, file_attrs,
				cpu_to_le32(0x0101));
		} else {
			add_attr_std_info(m, file_attrs,
				cpu_to_le32(0x00));
		}
	}
	/* The root directory mft reference. */
	root_ref = MK_LE_MREF(FILE_root, FILE_root);
	extend_ref = MK_LE_MREF(11,11);
	ntfs_log_verbose("Creating root directory (mft record 5)\n");
	m = (MFT_RECORD*)(g_buf + 5 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_DIRECTORY;
	m->link_count = cpu_to_le16(le16_to_cpu(m->link_count) + 1);
	err = add_attr_file_name(m, root_ref, 0LL, 0LL,
			FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
			FILE_ATTR_I30_INDEX_PRESENT, 0, 0, ".",
			FILE_NAME_WIN32_AND_DOS);
	if (!err) {
		init_root_sd(&sd, &i);
		err = add_attr_sd(m, sd, i);
	}
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$I30", 4, 0, AT_FILE_NAME,
				COLLATION_FILE_NAME, g_vol->indx_record_size);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = upgrade_to_large_index(m, "$I30", 4, 0, &g_index_block);
	if (!err) {
		ntfs_attr_search_ctx *ctx;
		ATTR_RECORD *a;
		ctx = ntfs_attr_get_search_ctx(NULL, m);
		if (!ctx) {
			ntfs_log_perror("Failed to allocate attribute search "
					"context");
			return FALSE;
		}
		/* There is exactly one file name so this is ok. */
		if (mkntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, 0, 0, NULL,
				0, ctx)) {
			ntfs_attr_put_search_ctx(ctx);
			ntfs_log_error("BUG: $FILE_NAME attribute not found."
					"\n");
			return FALSE;
		}
		a = ctx->attr;
		err = insert_file_link_in_dir_index(g_index_block, root_ref,
				(FILE_NAME_ATTR*)((char*)a +
				le16_to_cpu(a->value_offset)),
				le32_to_cpu(a->value_length));
		ntfs_attr_put_search_ctx(ctx);
	}
	if (err) {
		ntfs_log_error("Couldn't create root directory: %s\n",
			strerror(-err));
		return FALSE;
	}
	/* Add all other attributes, on a per-file basis for clarity. */
	ntfs_log_verbose("Creating $MFT (mft record 0)\n");
	m = (MFT_RECORD*)g_buf;
	err = add_attr_data_positioned(m, NULL, 0, 0, 0, g_rl_mft, g_buf,
			g_mft_size);
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_MFT, 1), g_mft_size,
				g_mft_size, FILE_ATTR_HIDDEN |
				FILE_ATTR_SYSTEM, 0, 0, "$MFT",
				FILE_NAME_WIN32_AND_DOS);
	/* mft_bitmap is not modified in mkntfs; no need to sync it later. */
	if (!err)
		err = add_attr_bitmap_positioned(m, NULL, 0, 0, g_rl_mft_bmp,
				g_mft_bitmap, g_mft_bitmap_byte_size);
	if (err < 0) {
		ntfs_log_error("Couldn't create $MFT: %s\n", strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $MFTMirr (mft record 1)\n");
	m = (MFT_RECORD*)(g_buf + 1 * g_vol->mft_record_size);
	err = add_attr_data_positioned(m, NULL, 0, 0, 0, g_rl_mftmirr, g_buf,
			g_rl_mftmirr[0].length * g_vol->cluster_size);
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_MFTMirr, FILE_MFTMirr),
				g_rl_mftmirr[0].length * g_vol->cluster_size,
				g_rl_mftmirr[0].length * g_vol->cluster_size,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$MFTMirr", FILE_NAME_WIN32_AND_DOS);
	if (err < 0) {
		ntfs_log_error("Couldn't create $MFTMirr: %s\n",
				strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $LogFile (mft record 2)\n");
	m = (MFT_RECORD*)(g_buf + 2 * g_vol->mft_record_size);
	buf_log = ntfs_malloc(g_logfile_size);
	if (!buf_log)
		return FALSE;
	memset(buf_log, -1, g_logfile_size);
	err = add_attr_data_positioned(m, NULL, 0, 0, 0, g_rl_logfile, buf_log,
			g_logfile_size);
	free(buf_log);
	buf_log = NULL;
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_LogFile, FILE_LogFile),
				g_logfile_size, g_logfile_size,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$LogFile", FILE_NAME_WIN32_AND_DOS);
	if (err < 0) {
		ntfs_log_error("Couldn't create $LogFile: %s\n",
				strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $AttrDef (mft record 4)\n");
	m = (MFT_RECORD*)(g_buf + 4 * g_vol->mft_record_size);
	err = add_attr_data(m, NULL, 0, 0, 0, (u8*)g_vol->attrdef,
			g_vol->attrdef_len);
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_AttrDef, FILE_AttrDef),
				(g_vol->attrdef_len + g_vol->cluster_size - 1) &
				~(g_vol->cluster_size - 1), g_vol->attrdef_len,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$AttrDef", FILE_NAME_WIN32_AND_DOS);
	if (!err) {
		init_system_file_sd(FILE_AttrDef, &sd, &i);
		err = add_attr_sd(m, sd, i);
	}
	if (err < 0) {
		ntfs_log_error("Couldn't create $AttrDef: %s\n",
				strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $Bitmap (mft record 6)\n");
	m = (MFT_RECORD*)(g_buf + 6 * g_vol->mft_record_size);
	/* the data attribute of $Bitmap must be non-resident or otherwise */
	/* windows 2003 will regard the volume as corrupt (ERSO) */
	if (!err)
		err = insert_non_resident_attr_in_mft_record(m,
			AT_DATA,  NULL, 0, 0, 0,
			g_lcn_bitmap, g_lcn_bitmap_byte_size);


	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_Bitmap, FILE_Bitmap),
				(g_lcn_bitmap_byte_size + g_vol->cluster_size -
				1) & ~(g_vol->cluster_size - 1),
				g_lcn_bitmap_byte_size,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$Bitmap", FILE_NAME_WIN32_AND_DOS);
	if (err < 0) {
		ntfs_log_error("Couldn't create $Bitmap: %s\n", strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $Boot (mft record 7)\n");
	m = (MFT_RECORD*)(g_buf + 7 * g_vol->mft_record_size);
	bs = ntfs_calloc(8192);
	if (!bs)
		return FALSE;
	memcpy(bs, boot_array, sizeof(boot_array));
	/*
	 * Create the boot sector in bs. Note, that bs is already zeroed
	 * in the boot sector section and that it has the NTFS OEM id/magic
	 * already inserted, so no need to worry about these things.
	 */
	bs->bpb.bytes_per_sector = cpu_to_le16(opts.sector_size);
	bs->bpb.sectors_per_cluster = (u8)(g_vol->cluster_size /
			opts.sector_size);
	bs->bpb.media_type = 0xf8; /* hard disk */
	bs->bpb.sectors_per_track = cpu_to_le16(opts.sectors_per_track);
	ntfs_log_debug("sectors per track = %ld (0x%lx)\n",
			opts.sectors_per_track, opts.sectors_per_track);
	bs->bpb.heads = cpu_to_le16(opts.heads);
	ntfs_log_debug("heads = %ld (0x%lx)\n", opts.heads, opts.heads);
	bs->bpb.hidden_sectors = cpu_to_le32(opts.part_start_sect);
	ntfs_log_debug("hidden sectors = %llu (0x%llx)\n", opts.part_start_sect,
			opts.part_start_sect);
	bs->physical_drive = 0x80;  	    /* boot from hard disk */
	bs->extended_boot_signature = 0x80; /* everybody sets this, so we do */
	bs->number_of_sectors = cpu_to_sle64(opts.num_sectors);
	bs->mft_lcn = cpu_to_sle64(g_mft_lcn);
	bs->mftmirr_lcn = cpu_to_sle64(g_mftmirr_lcn);
	if (g_vol->mft_record_size >= g_vol->cluster_size) {
		bs->clusters_per_mft_record = g_vol->mft_record_size /
			g_vol->cluster_size;
	} else {
		bs->clusters_per_mft_record = -(ffs(g_vol->mft_record_size) -
				1);
		if ((u32)(1 << -bs->clusters_per_mft_record) !=
				g_vol->mft_record_size) {
			free(bs);
			ntfs_log_error("BUG: calculated clusters_per_mft_record"
					" is wrong (= 0x%x)\n",
					bs->clusters_per_mft_record);
			return FALSE;
		}
	}
	ntfs_log_debug("clusters per mft record = %i (0x%x)\n",
			bs->clusters_per_mft_record,
			bs->clusters_per_mft_record);
	if (g_vol->indx_record_size >= g_vol->cluster_size) {
		bs->clusters_per_index_record = g_vol->indx_record_size /
			g_vol->cluster_size;
	} else {
		bs->clusters_per_index_record = -g_vol->indx_record_size_bits;
		if ((1 << -bs->clusters_per_index_record) !=
				(s32)g_vol->indx_record_size) {
			free(bs);
			ntfs_log_error("BUG: calculated "
					"clusters_per_index_record is wrong "
					"(= 0x%x)\n",
					bs->clusters_per_index_record);
			return FALSE;
		}
	}
	ntfs_log_debug("clusters per index block = %i (0x%x)\n",
			bs->clusters_per_index_record,
			bs->clusters_per_index_record);
	/* Generate a 64-bit random number for the serial number. */
	bs->volume_serial_number = cpu_to_le64(((u64)random() << 32) |
			((u64)random() & 0xffffffff));
	/*
	 * Leave zero for now as NT4 leaves it zero, too. If want it later, see
	 * ../libntfs/bootsect.c for how to calculate it.
	 */
	bs->checksum = cpu_to_le32(0);
	/* Make sure the bootsector is ok. */
	if (!ntfs_boot_sector_is_ntfs(bs, TRUE)) {
		free(bs);
		ntfs_log_error("FATAL: Generated boot sector is invalid!\n");
		return FALSE;
	}
	err = add_attr_data_positioned(m, NULL, 0, 0, 0, g_rl_boot, (u8*)bs,
			8192);
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_Boot, FILE_Boot),
				(8192 + g_vol->cluster_size - 1) &
				~(g_vol->cluster_size - 1), 8192,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$Boot", FILE_NAME_WIN32_AND_DOS);
	if (!err) {
		init_system_file_sd(FILE_Boot, &sd, &i);
		err = add_attr_sd(m, sd, i);
	}
	if (err < 0) {
		free(bs);
		ntfs_log_error("Couldn't create $Boot: %s\n", strerror(-err));
		return FALSE;
	}
	if (create_backup_boot_sector((u8*)bs)) {
		/*
		 * Pre-2.6 kernels couldn't access the last sector if it was
		 * odd and we failed to set the device block size to the sector
		 * size, hence we schedule chkdsk to create it.
		 */
		volume_flags |= VOLUME_IS_DIRTY;
	}
	free(bs);
#ifdef ENABLE_UUID
	/*
	 * We cheat a little here and if the user has requested all times to be
	 * set to zero then we set the GUID to zero as well.  This options is
	 * only used for development purposes so that should be fine.
	 */
	if (!opts.use_epoch_time) {
		/* Generate a GUID for the volume. */
		uuid_generate((void*)&g_vol->guid);
	} else
		memset(&g_vol->guid, 0, sizeof(g_vol->guid));
#endif
	if (!create_file_volume(m, root_ref, volume_flags, &g_vol->guid))
		return FALSE;
	ntfs_log_verbose("Creating $BadClus (mft record 8)\n");
	m = (MFT_RECORD*)(g_buf + 8 * g_vol->mft_record_size);
	/* FIXME: This should be IGNORE_CASE */
	/* Create a sparse named stream of size equal to the volume size. */
	err = add_attr_data_positioned(m, "$Bad", 4, 0, 0, g_rl_bad, NULL,
			g_vol->nr_clusters * g_vol->cluster_size);
	if (!err) {
		err = add_attr_data(m, NULL, 0, 0, 0, NULL, 0);
	}
	if (!err) {
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_BadClus, FILE_BadClus),
				0LL, 0LL, FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM,
				0, 0, "$BadClus", FILE_NAME_WIN32_AND_DOS);
	}
	if (err < 0) {
		ntfs_log_error("Couldn't create $BadClus: %s\n",
				strerror(-err));
		return FALSE;
	}
	/* create $Secure (NTFS 3.0+) */
	ntfs_log_verbose("Creating $Secure (mft record 9)\n");
	m = (MFT_RECORD*)(g_buf + 9 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_VIEW_INDEX;
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(9, 9), 0LL, 0LL,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
				FILE_ATTR_VIEW_INDEX_PRESENT, 0, 0,
				"$Secure", FILE_NAME_WIN32_AND_DOS);
	buf_sds = NULL;
	buf_sds_first_size = 0;
	if (!err) {
		int buf_sds_size;

		buf_sds_first_size = 0xfc;
		buf_sds_size = 0x40000 + buf_sds_first_size;
		buf_sds = ntfs_calloc(buf_sds_size);
		if (!buf_sds)
			return FALSE;
		init_secure_sds(buf_sds);
		memcpy(buf_sds + 0x40000, buf_sds, buf_sds_first_size);
		err = add_attr_data(m, "$SDS", 4, 0, 0, (u8*)buf_sds,
				buf_sds_size);
	}
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$SDH", 4, 0, AT_UNUSED,
			COLLATION_NTOFS_SECURITY_HASH,
			g_vol->indx_record_size);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$SII", 4, 0, AT_UNUSED,
			COLLATION_NTOFS_ULONG, g_vol->indx_record_size);
	if (!err)
		err = initialize_secure(buf_sds, buf_sds_first_size, m);
	free(buf_sds);
	if (err < 0) {
		ntfs_log_error("Couldn't create $Secure: %s\n",
			strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $UpCase (mft record 0xa)\n");
	m = (MFT_RECORD*)(g_buf + 0xa * g_vol->mft_record_size);
	err = add_attr_data(m, NULL, 0, 0, 0, (u8*)g_vol->upcase,
			g_vol->upcase_len << 1);
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(FILE_UpCase, FILE_UpCase),
				((g_vol->upcase_len << 1) +
				g_vol->cluster_size - 1) &
				~(g_vol->cluster_size - 1),
				g_vol->upcase_len << 1,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM, 0, 0,
				"$UpCase", FILE_NAME_WIN32_AND_DOS);
	if (err < 0) {
		ntfs_log_error("Couldn't create $UpCase: %s\n", strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $Extend (mft record 11)\n");
	/*
	 * $Extend index must be resident.  Otherwise, w2k3 will regard the
	 * volume as corrupt. (ERSO)
	 */
	m = (MFT_RECORD*)(g_buf + 11 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_DIRECTORY;
	if (!err)
		err = create_hardlink(g_index_block, root_ref, m,
				MK_LE_MREF(11, 11), 0LL, 0LL,
				FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
				FILE_ATTR_I30_INDEX_PRESENT, 0, 0,
				"$Extend", FILE_NAME_WIN32_AND_DOS);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$I30", 4, 0, AT_FILE_NAME,
			COLLATION_FILE_NAME, g_vol->indx_record_size);
	if (err < 0) {
		ntfs_log_error("Couldn't create $Extend: %s\n",
			strerror(-err));
		return FALSE;
	}
	/* NTFS reserved system files (mft records 0xc-0xf) */
	for (i = 0xc; i < 0x10; i++) {
		ntfs_log_verbose("Creating system file (mft record 0x%x)\n", i);
		m = (MFT_RECORD*)(g_buf + i * g_vol->mft_record_size);
		err = add_attr_data(m, NULL, 0, 0, 0, NULL, 0);
		if (!err) {
			init_system_file_sd(i, &sd, &j);
			err = add_attr_sd(m, sd, j);
		}
		if (err < 0) {
			ntfs_log_error("Couldn't create system file %i (0x%x): "
					"%s\n", i, i, strerror(-err));
			return FALSE;
		}
	}
	/* create systemfiles for ntfs volumes (3.1) */
	/* starting with file 24 (ignoring file 16-23) */
	extend_flags = FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM |
		FILE_ATTR_ARCHIVE | FILE_ATTR_VIEW_INDEX_PRESENT;
	ntfs_log_verbose("Creating $Quota (mft record 24)\n");
	m = (MFT_RECORD*)(g_buf + 24 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_4;
	m->flags |= MFT_RECORD_IS_VIEW_INDEX;
	if (!err)
		err = create_hardlink_res((MFT_RECORD*)(g_buf +
			11 * g_vol->mft_record_size), extend_ref, m,
			MK_LE_MREF(24, 1), 0LL, 0LL, extend_flags,
			0, 0, "$Quota", FILE_NAME_WIN32_AND_DOS);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$Q", 2, 0, AT_UNUSED,
			COLLATION_NTOFS_ULONG, g_vol->indx_record_size);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$O", 2, 0, AT_UNUSED,
			COLLATION_NTOFS_SID, g_vol->indx_record_size);
	if (!err)
		err = initialize_quota(m);
	if (err < 0) {
		ntfs_log_error("Couldn't create $Quota: %s\n", strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $ObjId (mft record 25)\n");
	m = (MFT_RECORD*)(g_buf + 25 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_4;
	m->flags |= MFT_RECORD_IS_VIEW_INDEX;
	if (!err)
		err = create_hardlink_res((MFT_RECORD*)(g_buf +
				11 * g_vol->mft_record_size), extend_ref,
				m, MK_LE_MREF(25, 1), 0LL, 0LL,
				extend_flags, 0, 0, "$ObjId",
				FILE_NAME_WIN32_AND_DOS);

	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$O", 2, 0, AT_UNUSED,
			COLLATION_NTOFS_ULONGS,
			g_vol->indx_record_size);
#ifdef ENABLE_UUID
	if (!err)
		err = index_obj_id_insert(m, &g_vol->guid,
				MK_LE_MREF(FILE_Volume, FILE_Volume));
#endif
	if (err < 0) {
		ntfs_log_error("Couldn't create $ObjId: %s\n",
				strerror(-err));
		return FALSE;
	}
	ntfs_log_verbose("Creating $Reparse (mft record 26)\n");
	m = (MFT_RECORD*)(g_buf + 26 * g_vol->mft_record_size);
	m->flags |= MFT_RECORD_IS_4;
	m->flags |= MFT_RECORD_IS_VIEW_INDEX;
	if (!err)
		err = create_hardlink_res((MFT_RECORD*)(g_buf +
				11 * g_vol->mft_record_size),
				extend_ref, m, MK_LE_MREF(26, 1),
				0LL, 0LL, extend_flags, 0, 0,
				"$Reparse", FILE_NAME_WIN32_AND_DOS);
	/* FIXME: This should be IGNORE_CASE */
	if (!err)
		err = add_attr_index_root(m, "$R", 2, 0, AT_UNUSED,
			COLLATION_NTOFS_ULONGS, g_vol->indx_record_size);
	if (err < 0) {
		ntfs_log_error("Couldn't create $Reparse: %s\n",
			strerror(-err));
		return FALSE;
	}
	return TRUE;
}

/**
 * mkntfs_redirect
 */
static int mkntfs_redirect(struct mkntfs_options *opts2)
{
	int result = 1;
	ntfs_attr_search_ctx *ctx = NULL;
	long long lw, pos;
	ATTR_RECORD *a;
	MFT_RECORD *m;
	int i, err;

	if (!opts2) {
		ntfs_log_error("Internal error: invalid parameters to mkntfs_options.\n");
		goto done;
	}
	/* Initialize the random number generator with the current time. */
	srandom(mkntfs_time());
	/* Allocate and initialize ntfs_volume structure g_vol. */
	g_vol = ntfs_volume_alloc();
	if (!g_vol) {
		ntfs_log_perror("Could not create volume");
		goto done;
	}
	/* Create NTFS 3.1 (Windows XP/Vista) volumes. */
	g_vol->major_ver = 3;
	g_vol->minor_ver = 1;
	/* Transfer some options to the volume. */
	if (opts.label) {
		g_vol->vol_name = strdup(opts.label);
		if (!g_vol->vol_name) {
			ntfs_log_perror("Could not copy volume name");
			goto done;
		}
	}
	if (opts.cluster_size >= 0)
		g_vol->cluster_size = opts.cluster_size;
	/* Length is in unicode characters. */
	g_vol->upcase_len = 65536;
	g_vol->upcase = ntfs_malloc(g_vol->upcase_len * sizeof(ntfschar));
	if (!g_vol->upcase)
		goto done;
	ntfs_upcase_table_build(g_vol->upcase,
			g_vol->upcase_len * sizeof(ntfschar));
	g_vol->attrdef = ntfs_malloc(sizeof(attrdef_ntfs3x_array));
	if (!g_vol->attrdef) {
		ntfs_log_perror("Could not create attrdef structure");
		goto done;
	}
	memcpy(g_vol->attrdef, attrdef_ntfs3x_array,
			sizeof(attrdef_ntfs3x_array));
	g_vol->attrdef_len = sizeof(attrdef_ntfs3x_array);
	/* Open the partition. */
	if (!mkntfs_open_partition(g_vol))
		goto done;
	/*
	 * Decide on the sector size, cluster size, mft record and index record
	 * sizes as well as the number of sectors/tracks/heads/size, etc.
	 */
	if (!mkntfs_override_vol_params(g_vol))
		goto done;
	/* Initialize $Bitmap and $MFT/$BITMAP related stuff. */
	if (!mkntfs_initialize_bitmaps())
		goto done;
	/* Initialize MFT & set g_logfile_lcn. */
	if (!mkntfs_initialize_rl_mft())
		goto done;
	/* Initialize $LogFile. */
	if (!mkntfs_initialize_rl_logfile())
		goto done;
	/* Initialize $Boot. */
	if (!mkntfs_initialize_rl_boot())
		goto done;
	/* Allocate a buffer large enough to hold the mft. */
	g_buf = ntfs_calloc(g_mft_size);
	if (!g_buf)
		goto done;
	/* Create runlist for $BadClus, $DATA named stream $Bad. */
	if (!mkntfs_initialize_rl_bad())
		goto done;
	/* If not quick format, fill the device with 0s. */
	if (!opts.quick_format) {
		if (!mkntfs_fill_device_with_zeroes())
			goto done;
	}
	/* Create NTFS volume structures. */
	if (!mkntfs_create_root_structures())
		goto done;
	/*
	 * - Do not step onto bad blocks!!!
	 * - If any bad blocks were specified or found, modify $BadClus,
	 *   allocating the bad clusters in $Bitmap.
	 * - C&w bootsector backup bootsector (backup in last sector of the
	 *   partition).
	 * - If NTFS 3.0+, c&w $Secure file and $Extend directory with the
	 *   corresponding special files in it, i.e. $ObjId, $Quota, $Reparse,
	 *   and $UsnJrnl. And others? Or not all necessary?
	 * - RE: Populate $root with the system files (and $Extend directory if
	 *   applicable). Possibly should move this as far to the top as
	 *   possible and update during each subsequent c&w of each system file.
	 */
	ntfs_log_verbose("Syncing root directory index record.\n");
	if (!mkntfs_sync_index_record(g_index_block, (MFT_RECORD*)(g_buf + 5 *
			g_vol->mft_record_size), NTFS_INDEX_I30, 4))
		goto done;

	ntfs_log_verbose("Syncing $Bitmap.\n");
	m = (MFT_RECORD*)(g_buf + 6 * g_vol->mft_record_size);

	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_perror("Could not create an attribute search context");
		goto done;
	}

	if (mkntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
		ntfs_log_error("BUG: $DATA attribute not found.\n");
		goto done;
	}

	a = ctx->attr;
	if (a->non_resident) {
		runlist *rl = ntfs_mapping_pairs_decompress(g_vol, a, NULL);
		if (!rl) {
			ntfs_log_error("ntfs_mapping_pairs_decompress() failed\n");
			goto done;
		}
		lw = ntfs_rlwrite(g_vol->dev, rl, g_lcn_bitmap, g_lcn_bitmap_byte_size, NULL);
		err = errno;
		free(rl);
		if (lw != g_lcn_bitmap_byte_size) {
			ntfs_log_error("ntfs_rlwrite: %s\n", lw == -1 ?
				       strerror(err) : "unknown error");
			goto done;
		}
	} else {
		memcpy((char*)a + le16_to_cpu(a->value_offset), g_lcn_bitmap, le32_to_cpu(a->value_length));
	}

	/*
	 * No need to sync $MFT/$BITMAP as that has never been modified since
	 * its creation.
	 */
	ntfs_log_verbose("Syncing $MFT.\n");
	pos = g_mft_lcn * g_vol->cluster_size;
	lw = 1;
	for (i = 0; i < g_mft_size / (s32)g_vol->mft_record_size; i++) {
		if (!opts.no_action)
			lw = ntfs_mst_pwrite(g_vol->dev, pos, 1, g_vol->mft_record_size, g_buf + i * g_vol->mft_record_size);
		if (lw != 1) {
			ntfs_log_error("ntfs_mst_pwrite: %s\n", lw == -1 ?
				       strerror(errno) : "unknown error");
			goto done;
		}
		pos += g_vol->mft_record_size;
	}
	ntfs_log_verbose("Updating $MFTMirr.\n");
	pos = g_mftmirr_lcn * g_vol->cluster_size;
	lw = 1;
	for (i = 0; i < g_rl_mftmirr[0].length * g_vol->cluster_size / g_vol->mft_record_size; i++) {
		m = (MFT_RECORD*)(g_buf + i * g_vol->mft_record_size);
		/*
		 * Decrement the usn by one, so it becomes the same as the one
		 * in $MFT once it is mst protected. - This is as we need the
		 * $MFTMirr to have the exact same byte by byte content as
		 * $MFT, rather than just equivalent meaning content.
		 */
		if (ntfs_mft_usn_dec(m)) {
			ntfs_log_error("ntfs_mft_usn_dec");
			goto done;
		}
		if (!opts.no_action)
			lw = ntfs_mst_pwrite(g_vol->dev, pos, 1, g_vol->mft_record_size, g_buf + i * g_vol->mft_record_size);
		if (lw != 1) {
			ntfs_log_error("ntfs_mst_pwrite: %s\n", lw == -1 ?
				       strerror(errno) : "unknown error");
			goto done;
		}
		pos += g_vol->mft_record_size;
	}
	ntfs_log_verbose("Syncing device.\n");
	if (g_vol->dev->d_ops->sync(g_vol->dev)) {
		ntfs_log_error("Syncing device. FAILED");
		goto done;
	}
	ntfs_log_quiet("mkntfs completed successfully. Have a nice day.\n");
	result = 0;
done:
	ntfs_attr_put_search_ctx(ctx);
	mkntfs_cleanup();	/* Device is unlocked and closed here */
	return result;
}


/**
 * main - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, something went wrong
 */
int main(int argc, char *argv[])
{
	int result = 1;

	ntfs_log_set_handler(ntfs_log_handler_outerr);
	utils_set_locale();

	mkntfs_init_options(&opts);			/* Set up the options */

	if (!mkntfs_parse_options(argc, argv, &opts))	/* Read the command line options */
		goto done;

	result = mkntfs_redirect(&opts);
done:
	return result;
}
