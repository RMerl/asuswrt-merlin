/**
 * ntfsmftalloc - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Anton Altaparmakov
 *
 * This utility will allocate and initialize an mft record.
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

#include "config.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#	include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#	include <stdio.h>
#endif
#ifdef HAVE_STDARG_H
#	include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#	include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#	include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_GETOPT_H
#	include <getopt.h>
#else
	extern int optind;
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef LLONG_MAX
#	define LLONG_MAX 9223372036854775807LL
#endif

#include "types.h"
#include "attrib.h"
#include "inode.h"
#include "layout.h"
#include "volume.h"
#include "mft.h"
#include "utils.h"
#include "version.h"
#include "logging.h"

static const char *EXEC_NAME = "ntfsmftalloc";

/* Need these global so ntfsmftalloc_exit can access them. */
static BOOL success = FALSE;

static char *dev_name;

static ntfs_volume *vol;
static ntfs_inode *ni = NULL;
static ntfs_inode *base_ni = NULL;
static s64 base_mft_no = -1;

static struct {
				/* -h, print usage and exit. */
	int no_action;		/* -n, do not write to device, only display
				       what would be done. */
	int quiet;		/* -q, quiet execution. */
	int verbose;		/* -v, verbose execution, given twice, really
				       verbose execution (debug mode). */
	int force;		/* -f, force allocation. */
				/* -V, print version and exit. */
} opts;

/**
 * err_exit - error output and terminate; ignores quiet (-q)
 */
__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
static void err_exit(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "ERROR: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "Aborting...\n");
	exit(1);
}

/**
 * copyright - print copyright statements
 */
static void copyright(void)
{
	ntfs_log_info("Copyright (c) 2004-2005 Anton Altaparmakov\n"
			"Allocate and initialize a base or an extent mft "
			"record.  If a base mft record\nis not specified, a "
			"base mft record is allocated and initialized.  "
			"Otherwise,\nan extent mft record is allocated and "
			"initialized to point to the specified\nbase mft "
			"record.\n");
}

/**
 * license - print license statement
 */
static void license(void)
{
	ntfs_log_info("%s", ntfs_gpl);
}

/**
 * usage - print a list of the parameters to the program
 */
__attribute__((noreturn))
static void usage(void)
{
	copyright();
	ntfs_log_info("Usage: %s [options] device [base-mft-record]\n"
			"    -n    Do not write to disk\n"
			"    -f    Force execution despite errors\n"
			"    -q    Quiet execution\n"
			"    -v    Verbose execution\n"
			"    -vv   Very verbose execution\n"
			"    -V    Display version information\n"
			"    -l    Display licensing information\n"
			"    -h    Display this help\n", EXEC_NAME);
	ntfs_log_info("%s%s", ntfs_bugs, ntfs_home);
	exit(1);
}

/**
 * parse_options
 */
static void parse_options(int argc, char *argv[])
{
	long long ll;
	char *s;
	int c;

	if (argc && *argv)
		EXEC_NAME = *argv;
	ntfs_log_info("%s v%s (libntfs %s)\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	while ((c = getopt(argc, argv, "fh?nqvVl")) != EOF) {
		switch (c) {
		case 'f':
			opts.force = 1;
			break;
		case 'n':
			opts.no_action = 1;
			break;
		case 'q':
			opts.quiet = 1;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'V':
			/* Version number already printed, so just exit. */
			exit(0);
		case 'l':
			copyright();
			license();
			exit(0);
		case 'h':
		case '?':
		default:
			usage();
		}
	}

	if (opts.verbose > 1)
		ntfs_log_set_levels(NTFS_LOG_LEVEL_DEBUG | NTFS_LOG_LEVEL_TRACE |
			NTFS_LOG_LEVEL_VERBOSE | NTFS_LOG_LEVEL_QUIET);

	if (optind == argc)
		usage();
	/* Get the device. */
	dev_name = argv[optind++];
	ntfs_log_verbose("device name = %s\n", dev_name);
	if (optind != argc) {
		/* Get the base mft record number. */
		ll = strtoll(argv[optind++], &s, 0);
		if (*s || !ll || (ll >= LLONG_MAX && errno == ERANGE))
			err_exit("Invalid base mft record number: %s\n",
					argv[optind - 1]);
		base_mft_no = ll;
		ntfs_log_verbose("base mft record number = 0x%llx\n", (long long)ll);
	}
	if (optind != argc)
		usage();
}

/**
 * dump_mft_record
 */
static void dump_mft_record(MFT_RECORD *m)
{
	ATTR_RECORD *a;
	unsigned int u;
	MFT_REF r;

	ntfs_log_info("-- Beginning dump of mft record. --\n");
	u = le32_to_cpu(m->magic);
	ntfs_log_info("Mft record signature (magic) = %c%c%c%c\n", u & 0xff,
			u >> 8 & 0xff, u >> 16 & 0xff, u >> 24 & 0xff);
	u = le16_to_cpu(m->usa_ofs);
	ntfs_log_info("Update sequence array offset = %u (0x%x)\n", u, u);
	ntfs_log_info("Update sequence array size = %u\n", le16_to_cpu(m->usa_count));
	ntfs_log_info("$LogFile sequence number (lsn) = %llu\n",
			(unsigned long long)le64_to_cpu(m->lsn));
	ntfs_log_info("Sequence number = %u\n", le16_to_cpu(m->sequence_number));
	ntfs_log_info("Reference (hard link) count = %u\n",
						le16_to_cpu(m->link_count));
	u = le16_to_cpu(m->attrs_offset);
	ntfs_log_info("First attribute offset = %u (0x%x)\n", u, u);
	ntfs_log_info("Flags = %u: ", le16_to_cpu(m->flags));
	if (m->flags & MFT_RECORD_IN_USE)
		ntfs_log_info("MFT_RECORD_IN_USE");
	else
		ntfs_log_info("MFT_RECORD_NOT_IN_USE");
	if (m->flags & MFT_RECORD_IS_DIRECTORY)
		ntfs_log_info(" | MFT_RECORD_IS_DIRECTORY");
	ntfs_log_info("\n");
	u = le32_to_cpu(m->bytes_in_use);
	ntfs_log_info("Bytes in use = %u (0x%x)\n", u, u);
	u = le32_to_cpu(m->bytes_allocated);
	ntfs_log_info("Bytes allocated = %u (0x%x)\n", u, u);
	r = le64_to_cpu(m->base_mft_record);
	ntfs_log_info("Base mft record reference:\n\tMft record number = %llu\n\t"
			"Sequence number = %u\n",
			(unsigned long long)MREF(r), MSEQNO(r));
	ntfs_log_info("Next attribute instance = %u\n",
			le16_to_cpu(m->next_attr_instance));
	a = (ATTR_RECORD*)((char*)m + le16_to_cpu(m->attrs_offset));
	ntfs_log_info("-- Beginning dump of attributes within mft record. --\n");
	while ((char*)a < (char*)m + le32_to_cpu(m->bytes_in_use)) {
		if (a->type == AT_END)
			break;
		a = (ATTR_RECORD*)((char*)a + le32_to_cpu(a->length));
	};
	ntfs_log_info("-- End of attributes. --\n");
}

/**
 * ntfsmftalloc_exit
 */
static void ntfsmftalloc_exit(void)
{
	if (success)
		return;
	/* If there is a base inode, close that instead of the extent inode. */
	if (base_ni)
		ni = base_ni;
	/* Close the inode. */
	if (ni && ntfs_inode_close(ni)) {
		ntfs_log_perror("Warning: Failed to close inode 0x%llx",
				(long long)ni->mft_no);
	}
	/* Unmount the volume. */
	if (ntfs_umount(vol, 0) == -1)
		ntfs_log_perror("Warning: Could not umount %s", dev_name);
}

/**
 * main
 */
int main(int argc, char **argv)
{
	unsigned long mnt_flags, ul;
	int err;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	/* Initialize opts to zero / required values. */
	memset(&opts, 0, sizeof(opts));
	/* Parse command line options. */
	parse_options(argc, argv);
	utils_set_locale();
	/* Make sure the file system is not mounted. */
	if (ntfs_check_if_mounted(dev_name, &mnt_flags))
		ntfs_log_error("Failed to determine whether %s is mounted: %s\n",
				dev_name, strerror(errno));
	else if (mnt_flags & NTFS_MF_MOUNTED) {
		ntfs_log_error("%s is mounted.\n", dev_name);
		if (!opts.force)
			err_exit("Refusing to run!\n");
		ntfs_log_error("ntfsmftalloc forced anyway. Hope /etc/mtab "
				"is incorrect.\n");
	}
	/* Mount the device. */
	if (opts.no_action) {
		ntfs_log_quiet("Running in READ-ONLY mode!\n");
		ul = NTFS_MNT_RDONLY;
	} else
		ul = 0;
	vol = ntfs_mount(dev_name, ul);
	if (!vol)
		err_exit("Failed to mount %s: %s\n", dev_name, strerror(errno));
	/* Register our exit function which will unlock and close the device. */
	err = atexit(&ntfsmftalloc_exit);
	if (err == -1) {
		ntfs_log_error("Could not set up exit() function because atexit() "
				"failed: %s Aborting...\n", strerror(errno));
		ntfsmftalloc_exit();
		exit(1);
	}
	if (base_mft_no != -1) {
		base_ni = ntfs_inode_open(vol, base_mft_no);
		if (!base_ni)
			err_exit("Failed to open base inode 0x%llx: %s\n",
					(long long)base_mft_no,
					strerror(errno));
	}
	/* Open the specified inode. */
	ni = ntfs_mft_record_alloc(vol, base_ni);
	if (!ni)
		err_exit("Failed to allocate mft record: %s\n",
				strerror(errno));
	ntfs_log_info("Allocated %s mft record 0x%llx", base_ni ? "extent" : "base",
			(long long)ni->mft_no);
	if (base_ni)
		ntfs_log_info(" with base mft record 0x%llx",
				(long long)base_mft_no);
	ntfs_log_info(".\n");
	if (!opts.quiet && opts.verbose > 1) {
		ntfs_log_verbose("Dumping allocated mft record 0x%llx:\n",
				(long long)ni->mft_no);
		dump_mft_record(ni->mrec);
	}
	/* Close the (base) inode. */
	if (base_ni)
		ni = base_ni;
	err = ntfs_inode_close(ni);
	if (err)
		err_exit("Failed to close inode 0x%llx: %s\n",
				(long long)ni->mft_no, strerror(errno));
	/* Unmount the volume. */
	err = ntfs_umount(vol, 0);
	/* Disable our ntfsmftalloc_exit() handler. */
	success = TRUE;
	if (err == -1)
		ntfs_log_perror("Warning: Failed to umount %s", dev_name);
	else
		ntfs_log_quiet("ntfsmftalloc completed successfully.\n");
	return 0;
}
