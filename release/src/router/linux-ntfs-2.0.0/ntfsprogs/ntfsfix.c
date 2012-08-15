/**
 * ntfsfix - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2002-2006 Szabolcs Szakacsits
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This utility fixes some common NTFS problems, resets the NTFS journal file
 * and schedules an NTFS consistency check for the first boot into Windows.
 *
 *	Anton Altaparmakov <aia21@cantab.net>
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

/*
 * WARNING: This program might not work on architectures which do not allow
 * unaligned access. For those, the program would need to start using
 * get/put_unaligned macros (#include <asm/unaligned.h>), but not doing it yet,
 * since NTFS really mostly applies to ia32 only, which does allow unaligned
 * accesses. We might not actually have a problem though, since the structs are
 * defined as being packed so that might be enough for gcc to insert the
 * correct code.
 *
 * If anyone using a non-little endian and/or an aligned access only CPU tries
 * this program please let me know whether it works or not!
 *
 *	Anton Altaparmakov <aia21@cantab.net>
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "types.h"
#include "attrib.h"
#include "mft.h"
#include "device.h"
#include "logfile.h"
#include "utils.h"
#include "version.h"
#include "logging.h"

#ifdef NO_NTFS_DEVICE_DEFAULT_IO_OPS
#	error "No default device io operations!  Cannot build ntfsfix.  \
You need to run ./configure without the --disable-default-device-io-ops \
switch if you want to be able to build the NTFS utilities."
#endif

static const char *EXEC_NAME = "ntfsfix";
static const char *OK        = "OK\n";
static const char *FAILED    = "FAILED\n";

static struct {
	char *volume;
} opt;

/**
 * usage
 */
__attribute__((noreturn))
static int usage(void)
{
	ntfs_log_info("%s v%s (libntfs %s)\n"
		   "\n"
		   "Usage: %s [options] device\n"
		   "    Attempt to fix an NTFS partition.\n"
		   "\n"
		   "    -h, --help             Display this help\n"
		   "    -V, --version          Display version information\n"
		   "\n"
		   "For example: %s /dev/hda6\n\n",
		   EXEC_NAME, VERSION, ntfs_libntfs_version(), EXEC_NAME,
		   EXEC_NAME);
	ntfs_log_info("%s%s", ntfs_bugs, ntfs_home);
	exit(1);
}

/**
 * version
 */
__attribute__((noreturn))
static void version(void)
{
	ntfs_log_info("%s v%s\n\n"
		   "Attempt to fix an NTFS partition.\n\n"
		   "Copyright (c) 2000-2006 Anton Altaparmakov\n"
		   "Copyright (c) 2002-2006 Szabolcs Szakacsits\n"
		   "Copyright (c) 2007      Yura Pakhuchiy\n\n",
		   EXEC_NAME, VERSION);
	ntfs_log_info("%s\n%s%s", ntfs_gpl, ntfs_bugs, ntfs_home);
	exit(1);
}

/**
 * parse_options
 */
static void parse_options(int argc, char **argv)
{
	int c;
	static const char *sopt = "-hV";
	static const struct option lopt[] = {
		{ "help",	no_argument,	NULL, 'h' },
		{ "version",	no_argument,	NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	memset(&opt, 0, sizeof(opt));

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opt.volume)
				opt.volume = argv[optind - 1];
			else {
				ntfs_log_info("ERROR: Too many arguments.\n");
				usage();
			}
			break;
		case 'h':
		case '?':
			usage();
		case 'V':
			version();
		default:
			ntfs_log_info("ERROR: Unknown option '%s'.\n", argv[optind - 1]);
			usage();
		}
	}

	if (opt.volume == NULL) {
		ntfs_log_info("ERROR: You must specify a device.\n");
		usage();
	}
}

/**
 * OLD_ntfs_volume_set_flags
 */
static int OLD_ntfs_volume_set_flags(ntfs_volume *vol, const le16 flags)
{
	MFT_RECORD *m = NULL;
	ATTR_RECORD *a;
	VOLUME_INFORMATION *c;
	ntfs_attr_search_ctx *ctx;
	int ret = -1;   /* failure */

	if (!vol) {
		errno = EINVAL;
		return -1;
	}
	if (ntfs_file_record_read(vol, FILE_Volume, &m, NULL)) {
		ntfs_log_perror("Failed to read $Volume");
		return -1;
	}
	/* Sanity check */
	if (!(m->flags & MFT_RECORD_IN_USE)) {
		ntfs_log_error("$Volume has been deleted. Cannot handle this "
				"yet. Run chkdsk to fix this.\n");
		errno = EIO;
		goto err_exit;
	}
	/* Get a pointer to the volume information attribute. */
	ctx = ntfs_attr_get_search_ctx(NULL, m);
	if (!ctx) {
		ntfs_log_debug("Failed to allocate attribute search "
				"context.\n");
		goto err_exit;
	}
	if (ntfs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, 0, NULL,
			0, ctx)) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION was not found in "
				"$Volume!\n");
		goto err_out;
	}
	a = ctx->attr;
	/* Sanity check. */
	if (a->non_resident) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION must be resident "
				"(and it isn't)!\n");
		errno = EIO;
		goto err_out;
	}
	/* Get a pointer to the value of the attribute. */
	c = (VOLUME_INFORMATION*)(le16_to_cpu(a->value_offset) + (char*)a);
	/* Sanity checks. */
	if ((char*)c + le32_to_cpu(a->value_length) >
			(char*)m + le32_to_cpu(m->bytes_in_use) ||
			le16_to_cpu(a->value_offset) +
			le32_to_cpu(a->value_length) > le32_to_cpu(a->length)) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION in $Volume is "
				"corrupt!\n");
		errno = EIO;
		goto err_out;
	}
	/* Set the volume flags. */
	vol->flags = c->flags = flags;
	if (ntfs_mft_record_write(vol, FILE_Volume, m)) {
		ntfs_log_perror("Error writing $Volume");
		goto err_out;
	}
	ret = 0; /* success */
err_out:
	ntfs_attr_put_search_ctx(ctx);
err_exit:
	free(m);
	return ret;
}

/**
 * set_dirty_flag
 */
static int set_dirty_flag(ntfs_volume *vol)
{
	le16 flags;

	if (NVolWasDirty(vol))
		return 0;
	ntfs_log_info("Setting required flags on partition... ");
	/*
	 * Set chkdsk flag, i.e. mark the partition dirty so chkdsk will run
	 * and fix it for us.
	 */
	flags = vol->flags | VOLUME_IS_DIRTY;
	if (OLD_ntfs_volume_set_flags(vol, flags)) {
		ntfs_log_info(FAILED);
		ntfs_log_error("Error setting volume flags.\n");
		return -1;
	}
	vol->flags = flags;
	NVolSetWasDirty(vol);
	ntfs_log_info(OK);
	return 0;
}

/**
 * empty_journal
 */
static int empty_journal(ntfs_volume *vol)
{
	if (NVolLogFileEmpty(vol))
		return 0;
	ntfs_log_info("Going to empty the journal ($LogFile)... ");
	if (ntfs_logfile_reset(vol)) {
		ntfs_log_info(FAILED);
		ntfs_log_perror("Failed to reset $LogFile");
		return -1;
	}
	ntfs_log_info(OK);
	return 0;
}

/**
 * fix_mftmirr
 */
static int fix_mftmirr(ntfs_volume *vol)
{
	s64 l, br;
	unsigned char *m, *m2;
	int i, ret = -1; /* failure */
	BOOL done;

	ntfs_log_info("\nProcessing $MFT and $MFTMirr...\n");

	/* Load data from $MFT and $MFTMirr and compare the contents. */
	m = (u8*)malloc(vol->mftmirr_size << vol->mft_record_size_bits);
	if (!m) {
		ntfs_log_perror("Failed to allocate memory");
		return -1;
	}
	m2 = (u8*)malloc(vol->mftmirr_size << vol->mft_record_size_bits);
	if (!m2) {
		ntfs_log_perror("Failed to allocate memory");
		free(m);
		return -1;
	}

	ntfs_log_info("Reading $MFT... ");
	l = ntfs_attr_mst_pread(vol->mft_na, 0, vol->mftmirr_size,
			vol->mft_record_size, m);
	if (l != vol->mftmirr_size) {
		ntfs_log_info(FAILED);
		if (l != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read $MFT");
		goto error_exit;
	}
	ntfs_log_info(OK);

	ntfs_log_info("Reading $MFTMirr... ");
	l = ntfs_attr_mst_pread(vol->mftmirr_na, 0, vol->mftmirr_size,
			vol->mft_record_size, m2);
	if (l != vol->mftmirr_size) {
		ntfs_log_info(FAILED);
		if (l != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read $MFTMirr");
		goto error_exit;
	}
	ntfs_log_info(OK);

	/*
	 * FIXME: Need to actually check the $MFTMirr for being real. Otherwise
	 * we might corrupt the partition if someone is experimenting with
	 * software RAID and the $MFTMirr is not actually in the position we
	 * expect it to be... )-:
	 * FIXME: We should emit a warning it $MFTMirr is damaged and ask
	 * user whether to recreate it from $MFT or whether to abort. - The
	 * warning needs to include the danger of software RAID arrays.
	 * Maybe we should go as far as to detect whether we are running on a
	 * MD disk and if yes then bomb out right at the start of the program?
	 */

	ntfs_log_info("Comparing $MFTMirr to $MFT... ");
	done = FALSE;
	for (i = 0; i < vol->mftmirr_size; ++i) {
		MFT_RECORD *mrec, *mrec2;
		const char *ESTR[12] = { "$MFT", "$MFTMirr", "$LogFile",
			"$Volume", "$AttrDef", "root directory", "$Bitmap",
			"$Boot", "$BadClus", "$Secure", "$UpCase", "$Extend" };
		const char *s;
		BOOL use_mirr;

		if (i < 12)
			s = ESTR[i];
		else if (i < 16)
			s = "system file";
		else
			s = "mft record";

		use_mirr = FALSE;
		mrec = (MFT_RECORD*)(m + i * vol->mft_record_size);
		if (mrec->flags & MFT_RECORD_IN_USE) {
			if (ntfs_is_baad_record(mrec->magic)) {
				ntfs_log_info(FAILED);
				ntfs_log_error("$MFT error: Incomplete multi "
						"sector transfer detected in "
						"%s.\nCannot handle this yet. "
						")-:\n", s);
				goto error_exit;
			}
			if (!ntfs_is_mft_record(mrec->magic)) {
				ntfs_log_info(FAILED);
				ntfs_log_error("$MFT error: Invalid mft "
						"record for %s.\nCannot "
						"handle this yet. )-:\n", s);
				goto error_exit;
			}
		}
		mrec2 = (MFT_RECORD*)(m2 + i * vol->mft_record_size);
		if (mrec2->flags & MFT_RECORD_IN_USE) {
			if (ntfs_is_baad_record(mrec2->magic)) {
				ntfs_log_info(FAILED);
				ntfs_log_error("$MFTMirr error: Incomplete "
						"multi sector transfer "
						"detected in %s.\n", s);
				goto error_exit;
			}
			if (!ntfs_is_mft_record(mrec2->magic)) {
				ntfs_log_info(FAILED);
				ntfs_log_error("$MFTMirr error: Invalid mft "
						"record for %s.\n", s);
				goto error_exit;
			}
			/* $MFT is corrupt but $MFTMirr is ok, use $MFTMirr. */
			if (!(mrec->flags & MFT_RECORD_IN_USE) &&
					!ntfs_is_mft_record(mrec->magic))
				use_mirr = TRUE;
		}
		if (memcmp(mrec, mrec2, ntfs_mft_record_get_data_size(mrec))) {
			if (!done) {
				done = TRUE;
				ntfs_log_info(FAILED);
			}
			ntfs_log_info("Correcting differences in $MFT%s "
					"record %d...", use_mirr ? "" : "Mirr",
					i);
			br = ntfs_mft_record_write(vol, i,
					use_mirr ? mrec2 : mrec);
			if (br) {
				ntfs_log_info(FAILED);
				ntfs_log_perror("Error correcting $MFT%s",
						use_mirr ? "" : "Mirr");
				goto error_exit;
			}
			ntfs_log_info(OK);
		}
	}
	if (!done)
		ntfs_log_info(OK);
	ntfs_log_info("Processing of $MFT and $MFTMirr completed "
			"successfully.\n");
	ret = 0;
error_exit:
	free(m);
	free(m2);
	return ret;
}

/**
 * fix_mount
 */
static int fix_mount(void)
{
	int ret = -1; /* failure */
	ntfs_volume *vol;
	struct ntfs_device *dev;

	ntfs_log_info("Attempting to correct errors... ");

	dev = ntfs_device_alloc(opt.volume, 0, &ntfs_device_default_io_ops,
			NULL);
	if (!dev) {
		ntfs_log_info(FAILED);
		ntfs_log_perror("Failed to allocate device");
		return -1;
	}
	vol = ntfs_volume_startup(dev, 0);
	if (!vol) {
		ntfs_log_info(FAILED);
		ntfs_log_perror("Failed to startup volume");
		ntfs_log_error("Volume is corrupt. You should run chkdsk.\n");
		ntfs_device_free(dev);
		return -1;
	}
	if (fix_mftmirr(vol) < 0)
		goto error_exit;
	if (set_dirty_flag(vol) < 0)
		goto error_exit;
	if (empty_journal(vol) < 0)
		goto error_exit;
	ret = 0;
error_exit:
	/* ntfs_umount() will invoke ntfs_device_free() for us. */
	if (ntfs_umount(vol, 0))
		ntfs_umount(vol, 1);
	return ret;
}

/**
 * main
 */
int main(int argc, char **argv)
{
	ntfs_volume *vol;
	unsigned long mnt_flags;
	int ret = 1; /* failure */
	BOOL force = FALSE;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	parse_options(argc, argv);

	if (!ntfs_check_if_mounted(opt.volume, &mnt_flags)) {
		if ((mnt_flags & NTFS_MF_MOUNTED) &&
				!(mnt_flags & NTFS_MF_READONLY) && !force) {
			ntfs_log_error("Refusing to operate on read-write "
					"mounted device %s.\n", opt.volume);
			exit(1);
		}
	} else
		ntfs_log_perror("Failed to determine whether %s is mounted",
				opt.volume);
	/* Attempt a full mount first. */
	ntfs_log_info("Mounting volume... ");
	vol = ntfs_mount(opt.volume, 0);
	if (vol) {
		ntfs_log_info(OK);
		ntfs_log_info("Processing of $MFT and $MFTMirr completed "
				"successfully.\n");
	} else {
		ntfs_log_info(FAILED);
		if (fix_mount() < 0)
			exit(1);
		vol = ntfs_mount(opt.volume, 0);
		if (!vol) {
			ntfs_log_perror("Remount failed");
			exit(1);
		}
	}
	/* So the unmount does not clear it again. */
	NVolSetWasDirty(vol);
	/* Check NTFS version is ok for us (in $Volume) */
	ntfs_log_info("NTFS volume version is %i.%i.\n", vol->major_ver,
			vol->minor_ver);
	if (ntfs_version_is_supported(vol)) {
		ntfs_log_error("Error: Unknown NTFS version.\n");
		goto error_exit;
	}
	if (vol->major_ver >= 3) {
		/*
		 * FIXME: If on NTFS 3.0+, check for presence of the usn
		 * journal and stamp it if present.
		 */
	}
	/* FIXME: We should be marking the quota out of date, too. */
	/* That's all for now! */
	ntfs_log_info("NTFS partition %s was processed successfully.\n",
			vol->dev->d_name);
	/* Set return code to 0. */
	ret = 0;
error_exit:
	if (ntfs_umount(vol, 0))
		ntfs_umount(vol, 1);
	return ret;
}

