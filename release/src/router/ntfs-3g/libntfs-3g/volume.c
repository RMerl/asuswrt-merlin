/**
 * volume.c - NTFS volume handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2002-2009 Szabolcs Szakacsits
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2010      Jean-Pierre Andre
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include "compat.h"
#include "volume.h"
#include "attrib.h"
#include "mft.h"
#include "bootsect.h"
#include "device.h"
#include "debug.h"
#include "inode.h"
#include "runlist.h"
#include "logfile.h"
#include "dir.h"
#include "logging.h"
#include "cache.h"
#include "misc.h"

const char *ntfs_home = 
"Ntfs-3g news, support and information:  http://ntfs-3g.org\n";

static const char *invalid_ntfs_msg =
"The device '%s' doesn't seem to have a valid NTFS.\n"
"Maybe the wrong device is used? Or the whole disk instead of a\n"
"partition (e.g. /dev/sda, not /dev/sda1)? Or the other way around?\n";

static const char *corrupt_volume_msg =
"NTFS is either inconsistent, or there is a hardware fault, or it's a\n"
"SoftRAID/FakeRAID hardware. In the first case run chkdsk /f on Windows\n"
"then reboot into Windows twice. The usage of the /f parameter is very\n"
"important! If the device is a SoftRAID/FakeRAID then first activate\n"
"it and mount a different device under the /dev/mapper/ directory, (e.g.\n"
"/dev/mapper/nvidia_eahaabcc1). Please see the 'dmraid' documentation\n"
"for more details.\n";

static const char *hibernated_volume_msg =
"The NTFS partition is hibernated. Please resume and shutdown Windows\n"
"properly, or mount the volume read-only with the 'ro' mount option, or\n"
"mount the volume read-write with the 'remove_hiberfile' mount option.\n"
"For example type on the command line:\n"
"\n"
"            mount -t ntfs-3g -o remove_hiberfile %s %s\n"
"\n";

static const char *unclean_journal_msg =
"Write access is denied because the disk wasn't safely powered\n"
"off and the 'norecover' mount option was specified.\n";

static const char *opened_volume_msg =
"Mount is denied because the NTFS volume is already exclusively opened.\n"
"The volume may be already mounted, or another software may use it which\n"
"could be identified for example by the help of the 'fuser' command.\n";

static const char *fakeraid_msg =
"Either the device is missing or it's powered down, or you have\n"
"SoftRAID hardware and must use an activated, different device under\n" 
"/dev/mapper/, (e.g. /dev/mapper/nvidia_eahaabcc1) to mount NTFS.\n"
"Please see the 'dmraid' documentation for help.\n";

static const char *access_denied_msg =
"Please check '%s' and the ntfs-3g binary permissions,\n"
"and the mounting user ID. More explanation is provided at\n"
"http://ntfs-3g.org/support.html#unprivileged\n";

/**
 * ntfs_volume_alloc - Create an NTFS volume object and initialise it
 *
 * Description...
 *
 * Returns:
 */
ntfs_volume *ntfs_volume_alloc(void)
{
	return ntfs_calloc(sizeof(ntfs_volume));
}

static void ntfs_attr_free(ntfs_attr **na)
{
	if (na && *na) {
		ntfs_attr_close(*na);
		*na = NULL;
	}
}

static int ntfs_inode_free(ntfs_inode **ni)
{
	int ret = -1;

	if (ni && *ni) {
		ret = ntfs_inode_close(*ni);
		*ni = NULL;
	} 
	
	return ret;
}

static void ntfs_error_set(int *err)
{
	if (!*err)
		*err = errno;
}

/**
 * __ntfs_volume_release - Destroy an NTFS volume object
 * @v:
 *
 * Description...
 *
 * Returns:
 */
static int __ntfs_volume_release(ntfs_volume *v)
{
	int err = 0;

	if (ntfs_inode_free(&v->vol_ni))
		ntfs_error_set(&err);
	/* 
	 * FIXME: Inodes must be synced before closing
	 * attributes, otherwise unmount could fail.
	 */
	if (v->lcnbmp_ni && NInoDirty(v->lcnbmp_ni))
		ntfs_inode_sync(v->lcnbmp_ni);
	ntfs_attr_free(&v->lcnbmp_na);
	if (ntfs_inode_free(&v->lcnbmp_ni))
		ntfs_error_set(&err);
	
	if (v->mft_ni && NInoDirty(v->mft_ni))
		ntfs_inode_sync(v->mft_ni);
	ntfs_attr_free(&v->mftbmp_na);
	ntfs_attr_free(&v->mft_na);
	if (ntfs_inode_free(&v->mft_ni))
		ntfs_error_set(&err);
	
	if (v->mftmirr_ni && NInoDirty(v->mftmirr_ni))
		ntfs_inode_sync(v->mftmirr_ni);
	ntfs_attr_free(&v->mftmirr_na);
	if (ntfs_inode_free(&v->mftmirr_ni))
		ntfs_error_set(&err);
	
	if (v->dev) {
		struct ntfs_device *dev = v->dev;

		if (dev->d_ops->sync(dev))
			ntfs_error_set(&err);
		if (dev->d_ops->close(dev))
			ntfs_error_set(&err);
	}

	ntfs_free_lru_caches(v);
	free(v->vol_name);
	free(v->upcase);
	if (v->locase) free(v->locase);
	free(v->attrdef);
	free(v);

	errno = err;
	return errno ? -1 : 0;
}

static void ntfs_attr_setup_flag(ntfs_inode *ni)
{
	STANDARD_INFORMATION *si;

	si = ntfs_attr_readall(ni, AT_STANDARD_INFORMATION, AT_UNNAMED, 0, NULL);
	if (si) {
		ni->flags = si->file_attributes;
		free(si);
	}
}

/**
 * ntfs_mft_load - load the $MFT and setup the ntfs volume with it
 * @vol:	ntfs volume whose $MFT to load
 *
 * Load $MFT from @vol and setup @vol with it. After calling this function the
 * volume @vol is ready for use by all read access functions provided by the
 * ntfs library.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
static int ntfs_mft_load(ntfs_volume *vol)
{
	VCN next_vcn, last_vcn, highest_vcn;
	s64 l;
	MFT_RECORD *mb = NULL;
	ntfs_attr_search_ctx *ctx = NULL;
	ATTR_RECORD *a;
	int eo;

	/* Manually setup an ntfs_inode. */
	vol->mft_ni = ntfs_inode_allocate(vol);
	mb = ntfs_malloc(vol->mft_record_size);
	if (!vol->mft_ni || !mb) {
		ntfs_log_perror("Error allocating memory for $MFT");
		goto error_exit;
	}
	vol->mft_ni->mft_no = 0;
	vol->mft_ni->mrec = mb;
	/* Can't use any of the higher level functions yet! */
	l = ntfs_mst_pread(vol->dev, vol->mft_lcn << vol->cluster_size_bits, 1,
			vol->mft_record_size, mb);
	if (l != 1) {
		if (l != -1)
			errno = EIO;
		ntfs_log_perror("Error reading $MFT");
		goto error_exit;
	}
	
	if (ntfs_mft_record_check(vol, 0, mb))
		goto error_exit;
	
	ctx = ntfs_attr_get_search_ctx(vol->mft_ni, NULL);
	if (!ctx)
		goto error_exit;

	/* Find the $ATTRIBUTE_LIST attribute in $MFT if present. */
	if (ntfs_attr_lookup(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0, 0, 0, NULL, 0,
			ctx)) {
		if (errno != ENOENT) {
			ntfs_log_error("$MFT has corrupt attribute list.\n");
			goto io_error_exit;
		}
		goto mft_has_no_attr_list;
	}
	NInoSetAttrList(vol->mft_ni);
	l = ntfs_get_attribute_value_length(ctx->attr);
	if (l <= 0 || l > 0x40000) {
		ntfs_log_error("$MFT/$ATTR_LIST invalid length (%lld).\n",
			       (long long)l);
		goto io_error_exit;
	}
	vol->mft_ni->attr_list_size = l;
	vol->mft_ni->attr_list = ntfs_malloc(l);
	if (!vol->mft_ni->attr_list)
		goto error_exit;
	
	l = ntfs_get_attribute_value(vol, ctx->attr, vol->mft_ni->attr_list);
	if (!l) {
		ntfs_log_error("Failed to get value of $MFT/$ATTR_LIST.\n");
		goto io_error_exit;
	}
	if (l != vol->mft_ni->attr_list_size) {
		ntfs_log_error("Partial read of $MFT/$ATTR_LIST (%lld != "
			       "%u).\n", (long long)l,
			       vol->mft_ni->attr_list_size);
		goto io_error_exit;
	}

mft_has_no_attr_list:

	ntfs_attr_setup_flag(vol->mft_ni);
	
	/* We now have a fully setup ntfs inode for $MFT in vol->mft_ni. */
	
	/* Get an ntfs attribute for $MFT/$DATA and set it up, too. */
	vol->mft_na = ntfs_attr_open(vol->mft_ni, AT_DATA, AT_UNNAMED, 0);
	if (!vol->mft_na) {
		ntfs_log_perror("Failed to open ntfs attribute");
		goto error_exit;
	}
	/* Read all extents from the $DATA attribute in $MFT. */
	ntfs_attr_reinit_search_ctx(ctx);
	last_vcn = vol->mft_na->allocated_size >> vol->cluster_size_bits;
	highest_vcn = next_vcn = 0;
	a = NULL;
	while (!ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, next_vcn, NULL, 0,
			ctx)) {
		runlist_element *nrl;

		a = ctx->attr;
		/* $MFT must be non-resident. */
		if (!a->non_resident) {
			ntfs_log_error("$MFT must be non-resident.\n");
			goto io_error_exit;
		}
		/* $MFT must be uncompressed and unencrypted. */
		if (a->flags & ATTR_COMPRESSION_MASK ||
				a->flags & ATTR_IS_ENCRYPTED) {
			ntfs_log_error("$MFT must be uncompressed and "
				       "unencrypted.\n");
			goto io_error_exit;
		}
		/*
		 * Decompress the mapping pairs array of this extent and merge
		 * the result into the existing runlist. No need for locking
		 * as we have exclusive access to the inode at this time and we
		 * are a mount in progress task, too.
		 */
		nrl = ntfs_mapping_pairs_decompress(vol, a, vol->mft_na->rl);
		if (!nrl) {
			ntfs_log_perror("ntfs_mapping_pairs_decompress() failed");
			goto error_exit;
		}
		vol->mft_na->rl = nrl;

		/* Get the lowest vcn for the next extent. */
		highest_vcn = sle64_to_cpu(a->highest_vcn);
		next_vcn = highest_vcn + 1;

		/* Only one extent or error, which we catch below. */
		if (next_vcn <= 0)
			break;

		/* Avoid endless loops due to corruption. */
		if (next_vcn < sle64_to_cpu(a->lowest_vcn)) {
			ntfs_log_error("$MFT has corrupt attribute list.\n");
			goto io_error_exit;
		}
	}
	if (!a) {
		ntfs_log_error("$MFT/$DATA attribute not found.\n");
		goto io_error_exit;
	}
	if (highest_vcn && highest_vcn != last_vcn - 1) {
		ntfs_log_error("Failed to load runlist for $MFT/$DATA.\n");
		ntfs_log_error("highest_vcn = 0x%llx, last_vcn - 1 = 0x%llx\n",
			       (long long)highest_vcn, (long long)last_vcn - 1);
		goto io_error_exit;
	}
	/* Done with the $Mft mft record. */
	ntfs_attr_put_search_ctx(ctx);
	ctx = NULL;
	/*
	 * The volume is now setup so we can use all read access functions.
	 */
	vol->mftbmp_na = ntfs_attr_open(vol->mft_ni, AT_BITMAP, AT_UNNAMED, 0);
	if (!vol->mftbmp_na) {
		ntfs_log_perror("Failed to open $MFT/$BITMAP");
		goto error_exit;
	}
	return 0;
io_error_exit:
	errno = EIO;
error_exit:
	eo = errno;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (vol->mft_na) {
		ntfs_attr_close(vol->mft_na);
		vol->mft_na = NULL;
	}
	if (vol->mft_ni) {
		ntfs_inode_close(vol->mft_ni);
		vol->mft_ni = NULL;
	}
	errno = eo;
	return -1;
}

/**
 * ntfs_mftmirr_load - load the $MFTMirr and setup the ntfs volume with it
 * @vol:	ntfs volume whose $MFTMirr to load
 *
 * Load $MFTMirr from @vol and setup @vol with it. After calling this function
 * the volume @vol is ready for use by all write access functions provided by
 * the ntfs library (assuming ntfs_mft_load() has been called successfully
 * beforehand).
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
static int ntfs_mftmirr_load(ntfs_volume *vol)
{
	int err;

	vol->mftmirr_ni = ntfs_inode_open(vol, FILE_MFTMirr);
	if (!vol->mftmirr_ni) {
		ntfs_log_perror("Failed to open inode $MFTMirr");
		return -1;
	}
	
	vol->mftmirr_na = ntfs_attr_open(vol->mftmirr_ni, AT_DATA, AT_UNNAMED, 0);
	if (!vol->mftmirr_na) {
		ntfs_log_perror("Failed to open $MFTMirr/$DATA");
		goto error_exit;
	}
	
	if (ntfs_attr_map_runlist(vol->mftmirr_na, 0) < 0) {
		ntfs_log_perror("Failed to map runlist of $MFTMirr/$DATA");
		goto error_exit;
	}
	
	return 0;

error_exit:
	err = errno;
	if (vol->mftmirr_na) {
		ntfs_attr_close(vol->mftmirr_na);
		vol->mftmirr_na = NULL;
	}
	ntfs_inode_close(vol->mftmirr_ni);
	vol->mftmirr_ni = NULL;
	errno = err;
	return -1;
}

/**
 * ntfs_volume_startup - allocate and setup an ntfs volume
 * @dev:	device to open
 * @flags:	optional mount flags
 *
 * Load, verify, and parse bootsector; load and setup $MFT and $MFTMirr. After
 * calling this function, the volume is setup sufficiently to call all read
 * and write access functions provided by the library.
 *
 * Return the allocated volume structure on success and NULL on error with
 * errno set to the error code.
 */
ntfs_volume *ntfs_volume_startup(struct ntfs_device *dev, unsigned long flags)
{
	LCN mft_zone_size, mft_lcn;
	s64 br;
	ntfs_volume *vol;
	NTFS_BOOT_SECTOR *bs;
	int eo;

	if (!dev || !dev->d_ops || !dev->d_name) {
		errno = EINVAL;
		ntfs_log_perror("%s: dev = %p", __FUNCTION__, dev);
		return NULL;
	}

	bs = ntfs_malloc(sizeof(NTFS_BOOT_SECTOR));
	if (!bs)
		return NULL;
	
	/* Allocate the volume structure. */
	vol = ntfs_volume_alloc();
	if (!vol)
		goto error_exit;
	
	/* Create the default upcase table. */
	vol->upcase_len = 65536;
	vol->upcase = ntfs_malloc(vol->upcase_len * sizeof(ntfschar));
	if (!vol->upcase)
		goto error_exit;
	
	ntfs_upcase_table_build(vol->upcase,
			vol->upcase_len * sizeof(ntfschar));
	/* Default with no locase table and case sensitive file names */
	vol->locase = (ntfschar*)NULL;
	NVolSetCaseSensitive(vol);
	
		/* by default, all files are shown and not marked hidden */
	NVolSetShowSysFiles(vol);
	NVolSetShowHidFiles(vol);
	NVolClearHideDotFiles(vol);
	if (flags & MS_RDONLY)
		NVolSetReadOnly(vol);
	
	/* ...->open needs bracketing to compile with glibc 2.7 */
	if ((dev->d_ops->open)(dev, NVolReadOnly(vol) ? O_RDONLY: O_RDWR)) {
		ntfs_log_perror("Error opening '%s'", dev->d_name);
		goto error_exit;
	}
	/* Attach the device to the volume. */
	vol->dev = dev;
	
	/* Now read the bootsector. */
	br = ntfs_pread(dev, 0, sizeof(NTFS_BOOT_SECTOR), bs);
	if (br != sizeof(NTFS_BOOT_SECTOR)) {
		if (br != -1)
			errno = EINVAL;
		if (!br)
			ntfs_log_error("Failed to read bootsector (size=0)\n");
		else
			ntfs_log_perror("Error reading bootsector");
		goto error_exit;
	}
	if (!ntfs_boot_sector_is_ntfs(bs)) {
		errno = EINVAL;
		goto error_exit;
	}
	if (ntfs_boot_sector_parse(vol, bs) < 0)
		goto error_exit;
	
	free(bs);
	bs = NULL;
	/* Now set the device block size to the sector size. */
	if (ntfs_device_block_size_set(vol->dev, vol->sector_size))
		ntfs_log_debug("Failed to set the device block size to the "
				"sector size.  This may affect performance "
				"but should be harmless otherwise.  Error: "
				"%s\n", strerror(errno));
	
	/* We now initialize the cluster allocator. */
	vol->full_zones = 0;
	mft_zone_size = vol->nr_clusters >> 3;      /* 12.5% */

	/* Setup the mft zone. */
	vol->mft_zone_start = vol->mft_zone_pos = vol->mft_lcn;
	ntfs_log_debug("mft_zone_pos = 0x%llx\n", (long long)vol->mft_zone_pos);

	/*
	 * Calculate the mft_lcn for an unmodified NTFS volume (see mkntfs
	 * source) and if the actual mft_lcn is in the expected place or even
	 * further to the front of the volume, extend the mft_zone to cover the
	 * beginning of the volume as well. This is in order to protect the
	 * area reserved for the mft bitmap as well within the mft_zone itself.
	 * On non-standard volumes we don't protect it as the overhead would be
	 * higher than the speed increase we would get by doing it.
	 */
	mft_lcn = (8192 + 2 * vol->cluster_size - 1) / vol->cluster_size;
	if (mft_lcn * vol->cluster_size < 16 * 1024)
		mft_lcn = (16 * 1024 + vol->cluster_size - 1) /
				vol->cluster_size;
	if (vol->mft_zone_start <= mft_lcn)
		vol->mft_zone_start = 0;
	ntfs_log_debug("mft_zone_start = 0x%llx\n", (long long)vol->mft_zone_start);

	/*
	 * Need to cap the mft zone on non-standard volumes so that it does
	 * not point outside the boundaries of the volume. We do this by
	 * halving the zone size until we are inside the volume.
	 */
	vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	while (vol->mft_zone_end >= vol->nr_clusters) {
		mft_zone_size >>= 1;
		vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	}
	ntfs_log_debug("mft_zone_end = 0x%llx\n", (long long)vol->mft_zone_end);

	/*
	 * Set the current position within each data zone to the start of the
	 * respective zone.
	 */
	vol->data1_zone_pos = vol->mft_zone_end;
	ntfs_log_debug("data1_zone_pos = %lld\n", (long long)vol->data1_zone_pos);
	vol->data2_zone_pos = 0;
	ntfs_log_debug("data2_zone_pos = %lld\n", (long long)vol->data2_zone_pos);

	/* Set the mft data allocation position to mft record 24. */
	vol->mft_data_pos = 24;

	/*
	 * The cluster allocator is now fully operational.
	 */

	/* Need to setup $MFT so we can use the library read functions. */
	if (ntfs_mft_load(vol) < 0) {
		ntfs_log_perror("Failed to load $MFT");
		goto error_exit;
	}

	/* Need to setup $MFTMirr so we can use the write functions, too. */
	if (ntfs_mftmirr_load(vol) < 0) {
		ntfs_log_perror("Failed to load $MFTMirr");
		goto error_exit;
	}
	return vol;
error_exit:
	eo = errno;
	free(bs);
	if (vol)
		__ntfs_volume_release(vol);
	errno = eo;
	return NULL;
}

/**
 * ntfs_volume_check_logfile - check logfile on target volume
 * @vol:	volume on which to check logfile
 *
 * Return 0 on success and -1 on error with errno set error code.
 */
static int ntfs_volume_check_logfile(ntfs_volume *vol)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	RESTART_PAGE_HEADER *rp = NULL;
	int err = 0;

	ni = ntfs_inode_open(vol, FILE_LogFile);
	if (!ni) {
		ntfs_log_perror("Failed to open inode FILE_LogFile");
		errno = EIO;
		return -1;
	}
	
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ntfs_log_perror("Failed to open $FILE_LogFile/$DATA");
		err = EIO;
		goto out;
	}
	
	if (!ntfs_check_logfile(na, &rp) || !ntfs_is_logfile_clean(na, rp))
		err = EOPNOTSUPP;
	free(rp);
	ntfs_attr_close(na);
out:	
	if (ntfs_inode_close(ni))
		ntfs_error_set(&err);
	if (err) {
		errno = err;
		return -1;
	}
	return 0;
}

/**
 * ntfs_hiberfile_open - Find and open '/hiberfil.sys'
 * @vol:    An ntfs volume obtained from ntfs_mount
 *
 * Return:  inode  Success, hiberfil.sys is valid
 *	    NULL   hiberfil.sys doesn't exist or some other error occurred
 */
static ntfs_inode *ntfs_hiberfile_open(ntfs_volume *vol)
{
	u64 inode;
	ntfs_inode *ni_root;
	ntfs_inode *ni_hibr = NULL;
	ntfschar   *unicode = NULL;
	int unicode_len;
	const char *hiberfile = "hiberfil.sys";

	if (!vol) {
		errno = EINVAL;
		return NULL;
	}

	ni_root = ntfs_inode_open(vol, FILE_root);
	if (!ni_root) {
		ntfs_log_debug("Couldn't open the root directory.\n");
		return NULL;
	}

	unicode_len = ntfs_mbstoucs(hiberfile, &unicode);
	if (unicode_len < 0) {
		ntfs_log_perror("Couldn't convert 'hiberfil.sys' to Unicode");
		goto out;
	}

	inode = ntfs_inode_lookup_by_name(ni_root, unicode, unicode_len);
	if (inode == (u64)-1) {
		ntfs_log_debug("Couldn't find file '%s'.\n", hiberfile);
		goto out;
	}

	inode = MREF(inode);
	ni_hibr = ntfs_inode_open(vol, inode);
	if (!ni_hibr) {
		ntfs_log_debug("Couldn't open inode %lld.\n", (long long)inode);
		goto out;
	}
out:
	if (ntfs_inode_close(ni_root)) {
		ntfs_inode_close(ni_hibr);
		ni_hibr = NULL;
	}
	free(unicode);
	return ni_hibr;
}


#define NTFS_HIBERFILE_HEADER_SIZE	4096

/**
 * ntfs_volume_check_hiberfile - check hiberfil.sys whether Windows is
 *                               hibernated on the target volume
 * @vol:    volume on which to check hiberfil.sys
 *
 * Return:  0 if Windows isn't hibernated for sure
 *         -1 otherwise and errno is set to the appropriate value
 */
int ntfs_volume_check_hiberfile(ntfs_volume *vol, int verbose)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	int bytes_read, err;
	char *buf = NULL;

	ni = ntfs_hiberfile_open(vol);
	if (!ni) {
		if (errno == ENOENT)
			return 0;
		return -1;
	}

	buf = ntfs_malloc(NTFS_HIBERFILE_HEADER_SIZE);
	if (!buf)
		goto out;

	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ntfs_log_perror("Failed to open hiberfil.sys data attribute");
		goto out;
	}

	bytes_read = ntfs_attr_pread(na, 0, NTFS_HIBERFILE_HEADER_SIZE, buf);
	if (bytes_read == -1) {
		ntfs_log_perror("Failed to read hiberfil.sys");
		goto out;
	}
	if (bytes_read < NTFS_HIBERFILE_HEADER_SIZE) {
		if (verbose)
			ntfs_log_error("Hibernated non-system partition, "
				       "refused to mount.\n");
		errno = EPERM;
		goto out;
	}
	if (memcmp(buf, "hibr", 4) == 0) {
		if (verbose)
			ntfs_log_error("Windows is hibernated, refused to mount.\n");
		errno = EPERM;
		goto out;
	}
        /* All right, all header bytes are zero */
	errno = 0;
out:
	if (na)
		ntfs_attr_close(na);
	free(buf);
	err = errno;
	if (ntfs_inode_close(ni))
		ntfs_error_set(&err);
	errno = err;
	return errno ? -1 : 0;
}

/*
 *		Make sure a LOGGED_UTILITY_STREAM attribute named "$TXF_DATA"
 *	on the root directory is resident.
 *	When it is non-resident, the partition cannot be mounted on Vista
 *	(see http://support.microsoft.com/kb/974729)
 *
 *	We take care to avoid this situation, however this can be a
 *	consequence of having used an older version (including older
 *	Windows version), so we had better fix it.
 *
 *	Returns 0 if unneeded or successful
 *		-1 if there was an error, explained by errno
 */

static int fix_txf_data(ntfs_volume *vol)
{
	void *txf_data;
	s64 txf_data_size;
	ntfs_inode *ni;
	ntfs_attr *na;
	int res;

	res = 0;
	ntfs_log_debug("Loading root directory\n");
	ni = ntfs_inode_open(vol, FILE_root);
	if (!ni) {
		ntfs_log_perror("Failed to open root directory");
		res = -1;
	} else {
		/* Get the $TXF_DATA attribute */
		na = ntfs_attr_open(ni, AT_LOGGED_UTILITY_STREAM, TXF_DATA, 9);
		if (na) {
			if (NAttrNonResident(na)) {
				/*
				 * Fix the attribute by truncating, then
				 * rewriting it.
				 */
				ntfs_log_debug("Making $TXF_DATA resident\n");
				txf_data = ntfs_attr_readall(ni,
						AT_LOGGED_UTILITY_STREAM,
						TXF_DATA, 9, &txf_data_size);
				if (txf_data) {
					if (ntfs_attr_truncate(na, 0)
					    || (ntfs_attr_pwrite(na, 0,
						 txf_data_size, txf_data)
							!= txf_data_size))
						res = -1;
					free(txf_data);
				}
			if (res)
				ntfs_log_error("Failed to make $TXF_DATA resident\n");
			else
				ntfs_log_error("$TXF_DATA made resident\n");
			}
			ntfs_attr_close(na);
		}
		if (ntfs_inode_close(ni)) {
			ntfs_log_perror("Failed to close root");
			res = -1;
		}
	}
	return (res);
}

/**
 * ntfs_device_mount - open ntfs volume
 * @dev:	device to open
 * @flags:	optional mount flags
 *
 * This function mounts an ntfs volume. @dev should describe the device which
 * to mount as the ntfs volume.
 *
 * @flags is an optional second parameter. The same flags are used as for
 * the mount system call (man 2 mount). Currently only the following flag
 * is implemented:
 *	MS_RDONLY	- mount volume read-only
 *
 * The function opens the device @dev and verifies that it contains a valid
 * bootsector. Then, it allocates an ntfs_volume structure and initializes
 * some of the values inside the structure from the information stored in the
 * bootsector. It proceeds to load the necessary system files and completes
 * setting up the structure.
 *
 * Return the allocated volume structure on success and NULL on error with
 * errno set to the error code.
 */
ntfs_volume *ntfs_device_mount(struct ntfs_device *dev, unsigned long flags)
{
	s64 l;
	ntfs_volume *vol;
	u8 *m = NULL, *m2 = NULL;
	ntfs_attr_search_ctx *ctx = NULL;
	ntfs_inode *ni;
	ntfs_attr *na;
	ATTR_RECORD *a;
	VOLUME_INFORMATION *vinf;
	ntfschar *vname;
	int i, j, eo;
	u32 u;

	vol = ntfs_volume_startup(dev, flags);
	if (!vol)
		return NULL;

	/* Load data from $MFT and $MFTMirr and compare the contents. */
	m  = ntfs_malloc(vol->mftmirr_size << vol->mft_record_size_bits);
	m2 = ntfs_malloc(vol->mftmirr_size << vol->mft_record_size_bits);
	if (!m || !m2)
		goto error_exit;

	l = ntfs_attr_mst_pread(vol->mft_na, 0, vol->mftmirr_size,
			vol->mft_record_size, m);
	if (l != vol->mftmirr_size) {
		if (l == -1)
			ntfs_log_perror("Failed to read $MFT");
		else {
			ntfs_log_error("Failed to read $MFT, unexpected length "
				       "(%lld != %d).\n", (long long)l,
				       vol->mftmirr_size);
			errno = EIO;
		}
		goto error_exit;
	}
	l = ntfs_attr_mst_pread(vol->mftmirr_na, 0, vol->mftmirr_size,
			vol->mft_record_size, m2);
	if (l != vol->mftmirr_size) {
		if (l == -1) {
			ntfs_log_perror("Failed to read $MFTMirr");
			goto error_exit;
		}
		vol->mftmirr_size = l;
	}
	ntfs_log_debug("Comparing $MFTMirr to $MFT...\n");
	for (i = 0; i < vol->mftmirr_size; ++i) {
		MFT_RECORD *mrec, *mrec2;
		const char *ESTR[12] = { "$MFT", "$MFTMirr", "$LogFile",
			"$Volume", "$AttrDef", "root directory", "$Bitmap",
			"$Boot", "$BadClus", "$Secure", "$UpCase", "$Extend" };
		const char *s;

		if (i < 12)
			s = ESTR[i];
		else if (i < 16)
			s = "system file";
		else
			s = "mft record";

		mrec = (MFT_RECORD*)(m + i * vol->mft_record_size);
		if (mrec->flags & MFT_RECORD_IN_USE) {
			if (ntfs_is_baad_recordp(mrec)) {
				ntfs_log_error("$MFT error: Incomplete multi "
					       "sector transfer detected in "
					       "'%s'.\n", s);
				goto io_error_exit;
			}
			if (!ntfs_is_mft_recordp(mrec)) {
				ntfs_log_error("$MFT error: Invalid mft "
						"record for '%s'.\n", s);
				goto io_error_exit;
			}
		}
		mrec2 = (MFT_RECORD*)(m2 + i * vol->mft_record_size);
		if (mrec2->flags & MFT_RECORD_IN_USE) {
			if (ntfs_is_baad_recordp(mrec2)) {
				ntfs_log_error("$MFTMirr error: Incomplete "
						"multi sector transfer "
						"detected in '%s'.\n", s);
				goto io_error_exit;
			}
			if (!ntfs_is_mft_recordp(mrec2)) {
				ntfs_log_error("$MFTMirr error: Invalid mft "
						"record for '%s'.\n", s);
				goto io_error_exit;
			}
		}
		if (memcmp(mrec, mrec2, ntfs_mft_record_get_data_size(mrec))) {
			ntfs_log_error("$MFTMirr does not match $MFT (record "
				       "%d).\n", i);
			goto io_error_exit;
		}
	}

	free(m2);
	free(m);
	m = m2 = NULL;

	/* Now load the bitmap from $Bitmap. */
	ntfs_log_debug("Loading $Bitmap...\n");
	vol->lcnbmp_ni = ntfs_inode_open(vol, FILE_Bitmap);
	if (!vol->lcnbmp_ni) {
		ntfs_log_perror("Failed to open inode FILE_Bitmap");
		goto error_exit;
	}
	
	vol->lcnbmp_na = ntfs_attr_open(vol->lcnbmp_ni, AT_DATA, AT_UNNAMED, 0);
	if (!vol->lcnbmp_na) {
		ntfs_log_perror("Failed to open ntfs attribute");
		goto error_exit;
	}
	
	if (vol->lcnbmp_na->data_size > vol->lcnbmp_na->allocated_size) {
		ntfs_log_error("Corrupt cluster map size (%lld > %lld)\n",
				(long long)vol->lcnbmp_na->data_size, 
				(long long)vol->lcnbmp_na->allocated_size);
		goto io_error_exit;
	}

	/* Now load the upcase table from $UpCase. */
	ntfs_log_debug("Loading $UpCase...\n");
	ni = ntfs_inode_open(vol, FILE_UpCase);
	if (!ni) {
		ntfs_log_perror("Failed to open inode FILE_UpCase");
		goto error_exit;
	}
	/* Get an ntfs attribute for $UpCase/$DATA. */
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ntfs_log_perror("Failed to open ntfs attribute");
		goto error_exit;
	}
	/*
	 * Note: Normally, the upcase table has a length equal to 65536
	 * 2-byte Unicode characters but allow for different cases, so no
	 * checks done. Just check we don't overflow 32-bits worth of Unicode
	 * characters.
	 */
	if (na->data_size & ~0x1ffffffffULL) {
		ntfs_log_error("Error: Upcase table is too big (max 32-bit "
				"allowed).\n");
		errno = EINVAL;
		goto error_exit;
	}
	if (vol->upcase_len != na->data_size >> 1) {
		vol->upcase_len = na->data_size >> 1;
		/* Throw away default table. */
		free(vol->upcase);
		vol->upcase = ntfs_malloc(na->data_size);
		if (!vol->upcase)
			goto error_exit;
	}
	/* Read in the $DATA attribute value into the buffer. */
	l = ntfs_attr_pread(na, 0, na->data_size, vol->upcase);
	if (l != na->data_size) {
		ntfs_log_error("Failed to read $UpCase, unexpected length "
			       "(%lld != %lld).\n", (long long)l,
			       (long long)na->data_size);
		errno = EIO;
		goto error_exit;
	}
	/* Done with the $UpCase mft record. */
	ntfs_attr_close(na);
	if (ntfs_inode_close(ni)) {
		ntfs_log_perror("Failed to close $UpCase");
		goto error_exit;
	}

	/*
	 * Now load $Volume and set the version information and flags in the
	 * vol structure accordingly.
	 */
	ntfs_log_debug("Loading $Volume...\n");
	vol->vol_ni = ntfs_inode_open(vol, FILE_Volume);
	if (!vol->vol_ni) {
		ntfs_log_perror("Failed to open inode FILE_Volume");
		goto error_exit;
	}
	/* Get a search context for the $Volume/$VOLUME_INFORMATION lookup. */
	ctx = ntfs_attr_get_search_ctx(vol->vol_ni, NULL);
	if (!ctx)
		goto error_exit;

	/* Find the $VOLUME_INFORMATION attribute. */
	if (ntfs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, 0, NULL,
			0, ctx)) {
		ntfs_log_perror("$VOLUME_INFORMATION attribute not found in "
				"$Volume");
		goto error_exit;
	}
	a = ctx->attr;
	/* Has to be resident. */
	if (a->non_resident) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION must be "
			       "resident but it isn't.\n");
		errno = EIO;
		goto error_exit;
	}
	/* Get a pointer to the value of the attribute. */
	vinf = (VOLUME_INFORMATION*)(le16_to_cpu(a->value_offset) + (char*)a);
	/* Sanity checks. */
	if ((char*)vinf + le32_to_cpu(a->value_length) > (char*)ctx->mrec +
			le32_to_cpu(ctx->mrec->bytes_in_use) ||
			le16_to_cpu(a->value_offset) + le32_to_cpu(
			a->value_length) > le32_to_cpu(a->length)) {
		ntfs_log_error("$VOLUME_INFORMATION in $Volume is corrupt.\n");
		errno = EIO;
		goto error_exit;
	}
	/* Setup vol from the volume information attribute value. */
	vol->major_ver = vinf->major_ver;
	vol->minor_ver = vinf->minor_ver;
	/* Do not use le16_to_cpu() macro here as our VOLUME_FLAGS are
	   defined using cpu_to_le16() macro and hence are consistent. */
	vol->flags = vinf->flags;
	/*
	 * Reinitialize the search context for the $Volume/$VOLUME_NAME lookup.
	 */
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(AT_VOLUME_NAME, AT_UNNAMED, 0, 0, 0, NULL, 0,
			ctx)) {
		if (errno != ENOENT) {
			ntfs_log_perror("Failed to lookup of $VOLUME_NAME in "
					"$Volume failed");
			goto error_exit;
		}
		/*
		 * Attribute not present.  This has been seen in the field.
		 * Treat this the same way as if the attribute was present but
		 * had zero length.
		 */
		vol->vol_name = ntfs_malloc(1);
		if (!vol->vol_name)
			goto error_exit;
		vol->vol_name[0] = '\0';
	} else {
		a = ctx->attr;
		/* Has to be resident. */
		if (a->non_resident) {
			ntfs_log_error("$VOLUME_NAME must be resident.\n");
			errno = EIO;
			goto error_exit;
		}
		/* Get a pointer to the value of the attribute. */
		vname = (ntfschar*)(le16_to_cpu(a->value_offset) + (char*)a);
		u = le32_to_cpu(a->value_length) / 2;
		/*
		 * Convert Unicode volume name to current locale multibyte
		 * format.
		 */
		vol->vol_name = NULL;
		if (ntfs_ucstombs(vname, u, &vol->vol_name, 0) == -1) {
			ntfs_log_perror("Volume name could not be converted "
					"to current locale");
			ntfs_log_debug("Forcing name into ASCII by replacing "
				"non-ASCII characters with underscores.\n");
			vol->vol_name = ntfs_malloc(u + 1);
			if (!vol->vol_name)
				goto error_exit;
			
			for (j = 0; j < (s32)u; j++) {
				u16 uc = le16_to_cpu(vname[j]);
				if (uc > 0xff)
					uc = (u16)'_';
				vol->vol_name[j] = (char)uc;
			}
			vol->vol_name[u] = '\0';
		}
	}
	ntfs_attr_put_search_ctx(ctx);
	ctx = NULL;
	/* Now load the attribute definitions from $AttrDef. */
	ntfs_log_debug("Loading $AttrDef...\n");
	ni = ntfs_inode_open(vol, FILE_AttrDef);
	if (!ni) {
		ntfs_log_perror("Failed to open $AttrDef");
		goto error_exit;
	}
	/* Get an ntfs attribute for $AttrDef/$DATA. */
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ntfs_log_perror("Failed to open ntfs attribute");
		goto error_exit;
	}
	/* Check we don't overflow 32-bits. */
	if (na->data_size > 0xffffffffLL) {
		ntfs_log_error("Attribute definition table is too big (max "
			       "32-bit allowed).\n");
		errno = EINVAL;
		goto error_exit;
	}
	vol->attrdef_len = na->data_size;
	vol->attrdef = ntfs_malloc(na->data_size);
	if (!vol->attrdef)
		goto error_exit;
	/* Read in the $DATA attribute value into the buffer. */
	l = ntfs_attr_pread(na, 0, na->data_size, vol->attrdef);
	if (l != na->data_size) {
		ntfs_log_error("Failed to read $AttrDef, unexpected length "
			       "(%lld != %lld).\n", (long long)l,
			       (long long)na->data_size);
		errno = EIO;
		goto error_exit;
	}
	/* Done with the $AttrDef mft record. */
	ntfs_attr_close(na);
	if (ntfs_inode_close(ni)) {
		ntfs_log_perror("Failed to close $AttrDef");
		goto error_exit;
	}
	/*
	 * Check for dirty logfile and hibernated Windows.
	 * We care only about read-write mounts.
	 */
	if (!(flags & MS_RDONLY)) {
		if (!(flags & MS_IGNORE_HIBERFILE) && 
		    ntfs_volume_check_hiberfile(vol, 1) < 0)
			goto error_exit;
		if (ntfs_volume_check_logfile(vol) < 0) {
			if (!(flags & MS_RECOVER))
				goto error_exit;
			ntfs_log_info("The file system wasn't safely "
				      "closed on Windows. Fixing.\n");
			if (ntfs_logfile_reset(vol))
				goto error_exit;
		}
	}
		/* make $TXF_DATA resident if present on the root directory */
	if (!NVolReadOnly(vol) && fix_txf_data(vol))
		goto error_exit;

	return vol;
io_error_exit:
	errno = EIO;
error_exit:
	eo = errno;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	free(m);
	free(m2);
	__ntfs_volume_release(vol);
	errno = eo;
	return NULL;
}

/*
 *		Set appropriate flags for showing NTFS metafiles
 *	or files marked as hidden.
 *	Not set in ntfs_mount() to avoid breaking existing tools.
 */

int ntfs_set_shown_files(ntfs_volume *vol,
			BOOL show_sys_files, BOOL show_hid_files,
			BOOL hide_dot_files)
{
	int res;

	res = -1;
	if (vol) {
		NVolClearShowSysFiles(vol);
		NVolClearShowHidFiles(vol);
		NVolClearHideDotFiles(vol);
		if (show_sys_files)
			NVolSetShowSysFiles(vol);
		if (show_hid_files)
			NVolSetShowHidFiles(vol);
		if (hide_dot_files)
			NVolSetHideDotFiles(vol);
		res = 0;
	}
	if (res)
		ntfs_log_error("Failed to set file visibility\n");
	return (res);
}

/*
 *		Set ignore case mode
 */

int ntfs_set_ignore_case(ntfs_volume *vol)
{
	int res;

	res = -1;
	if (vol && vol->upcase) {
		vol->locase = ntfs_locase_table_build(vol->upcase,
					vol->upcase_len);
		if (vol->locase) {
			NVolClearCaseSensitive(vol);
			res = 0;
		}
	}
	if (res)
		ntfs_log_error("Failed to set ignore_case mode\n");
	return (res);
}

/**
 * ntfs_mount - open ntfs volume
 * @name:	name of device/file to open
 * @flags:	optional mount flags
 *
 * This function mounts an ntfs volume. @name should contain the name of the
 * device/file to mount as the ntfs volume.
 *
 * @flags is an optional second parameter. The same flags are used as for
 * the mount system call (man 2 mount). Currently only the following flags
 * is implemented:
 *	MS_RDONLY	- mount volume read-only
 *
 * The function opens the device or file @name and verifies that it contains a
 * valid bootsector. Then, it allocates an ntfs_volume structure and initializes
 * some of the values inside the structure from the information stored in the
 * bootsector. It proceeds to load the necessary system files and completes
 * setting up the structure.
 *
 * Return the allocated volume structure on success and NULL on error with
 * errno set to the error code.
 *
 * Note, that a copy is made of @name, and hence it can be discarded as
 * soon as the function returns.
 */
ntfs_volume *ntfs_mount(const char *name __attribute__((unused)),
		unsigned long flags __attribute__((unused)))
{
#ifndef NO_NTFS_DEVICE_DEFAULT_IO_OPS
	struct ntfs_device *dev;
	ntfs_volume *vol;

	/* Allocate an ntfs_device structure. */
	dev = ntfs_device_alloc(name, 0, &ntfs_device_default_io_ops, NULL);
	if (!dev)
		return NULL;
	/* Call ntfs_device_mount() to do the actual mount. */
	vol = ntfs_device_mount(dev, flags);
	if (!vol) {
		int eo = errno;
		ntfs_device_free(dev);
		errno = eo;
	} else
		ntfs_create_lru_caches(vol);
	return vol;
#else
	/*
	 * ntfs_mount() makes no sense if NO_NTFS_DEVICE_DEFAULT_IO_OPS is
	 * defined as there are no device operations available in libntfs in
	 * this case.
	 */
	errno = EOPNOTSUPP;
	return NULL;
#endif
}

/**
 * ntfs_umount - close ntfs volume
 * @vol: address of ntfs_volume structure of volume to close
 * @force: if true force close the volume even if it is busy
 *
 * Deallocate all structures (including @vol itself) associated with the ntfs
 * volume @vol.
 *
 * Return 0 on success. On error return -1 with errno set appropriately
 * (most likely to one of EAGAIN, EBUSY or EINVAL). The EAGAIN error means that
 * an operation is in progress and if you try the close later the operation
 * might be completed and the close succeed.
 *
 * If @force is true (i.e. not zero) this function will close the volume even
 * if this means that data might be lost.
 *
 * @vol must have previously been returned by a call to ntfs_mount().
 *
 * @vol itself is deallocated and should no longer be dereferenced after this
 * function returns success. If it returns an error then nothing has been done
 * so it is safe to continue using @vol.
 */
int ntfs_umount(ntfs_volume *vol, const BOOL force __attribute__((unused)))
{
	struct ntfs_device *dev;
	int ret;

	if (!vol) {
		errno = EINVAL;
		return -1;
	}
	dev = vol->dev;
	ret = __ntfs_volume_release(vol);
	ntfs_device_free(dev);
	return ret;
}

#ifdef HAVE_MNTENT_H

#ifndef HAVE_REALPATH
/**
 * realpath - If there is no realpath on the system
 */
static char *realpath(const char *path, char *resolved_path)
{
	strncpy(resolved_path, path, PATH_MAX);
	resolved_path[PATH_MAX] = '\0';
	return resolved_path;
}
#endif

/**
 * ntfs_mntent_check - desc
 *
 * If you are wanting to use this, you actually wanted to use
 * ntfs_check_if_mounted(), you just didn't realize. (-:
 *
 * See description of ntfs_check_if_mounted(), below.
 */
static int ntfs_mntent_check(const char *file, unsigned long *mnt_flags)
{
	struct mntent *mnt;
	char *real_file = NULL, *real_fsname = NULL;
	FILE *f;
	int err = 0;

	real_file = ntfs_malloc(PATH_MAX + 1);
	if (!real_file)
		return -1;
	real_fsname = ntfs_malloc(PATH_MAX + 1);
	if (!real_fsname) {
		err = errno;
		goto exit;
	}
	if (!realpath(file, real_file)) {
		err = errno;
		goto exit;
	}
	if (!(f = setmntent(MOUNTED, "r"))) {
		err = errno;
		goto exit;
	}
	while ((mnt = getmntent(f))) {
		if (!realpath(mnt->mnt_fsname, real_fsname))
			continue;
		if (!strcmp(real_file, real_fsname))
			break;
	}
	endmntent(f);
	if (!mnt)
		goto exit;
	*mnt_flags = NTFS_MF_MOUNTED;
	if (!strcmp(mnt->mnt_dir, "/"))
		*mnt_flags |= NTFS_MF_ISROOT;
#ifdef HAVE_HASMNTOPT
	if (hasmntopt(mnt, "ro") && !hasmntopt(mnt, "rw"))
		*mnt_flags |= NTFS_MF_READONLY;
#endif
exit:
	free(real_file);
	free(real_fsname);
	if (err) {
		errno = err;
		return -1;
	}
	return 0;
}
#endif /* HAVE_MNTENT_H */

/**
 * ntfs_check_if_mounted - check if an ntfs volume is currently mounted
 * @file:	device file to check
 * @mnt_flags:	pointer into which to return the ntfs mount flags (see volume.h)
 *
 * If the running system does not support the {set,get,end}mntent() calls,
 * just return 0 and set *@mnt_flags to zero.
 *
 * When the system does support the calls, ntfs_check_if_mounted() first tries
 * to find the device @file in /etc/mtab (or wherever this is kept on the
 * running system). If it is not found, assume the device is not mounted and
 * return 0 and set *@mnt_flags to zero.
 *
 * If the device @file is found, set the NTFS_MF_MOUNTED flags in *@mnt_flags.
 *
 * Further if @file is mounted as the file system root ("/"), set the flag
 * NTFS_MF_ISROOT in *@mnt_flags.
 *
 * Finally, check if the file system is mounted read-only, and if so set the
 * NTFS_MF_READONLY flag in *@mnt_flags.
 *
 * On success return 0 with *@mnt_flags set to the ntfs mount flags.
 *
 * On error return -1 with errno set to the error code.
 */
int ntfs_check_if_mounted(const char *file __attribute__((unused)),
		unsigned long *mnt_flags)
{
	*mnt_flags = 0;
#ifdef HAVE_MNTENT_H
	return ntfs_mntent_check(file, mnt_flags);
#else
	return 0;
#endif
}

/**
 * ntfs_version_is_supported - check if NTFS version is supported.
 * @vol:	ntfs volume whose version we're interested in.
 *
 * The function checks if the NTFS volume version is known or not.
 * Version 1.1 and 1.2 are used by Windows NT3.x and NT4.
 * Version 2.x is used by Windows 2000 Betas.
 * Version 3.0 is used by Windows 2000.
 * Version 3.1 is used by Windows XP, Windows Server 2003 and Longhorn.
 *
 * Return 0 if NTFS version is supported otherwise -1 with errno set.
 *
 * The following error codes are defined:
 *	EOPNOTSUPP - Unknown NTFS version
 *	EINVAL	   - Invalid argument
 */
int ntfs_version_is_supported(ntfs_volume *vol)
{
	u8 major, minor;

	if (!vol) {
		errno = EINVAL;
		return -1;
	}

	major = vol->major_ver;
	minor = vol->minor_ver;

	if (NTFS_V1_1(major, minor) || NTFS_V1_2(major, minor))
		return 0;

	if (NTFS_V2_X(major, minor))
		return 0;

	if (NTFS_V3_0(major, minor) || NTFS_V3_1(major, minor))
		return 0;

	errno = EOPNOTSUPP;
	return -1;
}

/**
 * ntfs_logfile_reset - "empty" $LogFile data attribute value
 * @vol:	ntfs volume whose $LogFile we intend to reset.
 *
 * Fill the value of the $LogFile data attribute, i.e. the contents of
 * the file, with 0xff's, thus marking the journal as empty.
 *
 * FIXME(?): We might need to zero the LSN field of every single mft
 * record as well. (But, first try without doing that and see what
 * happens, since chkdsk might pickup the pieces and do it for us...)
 *
 * On success return 0.
 *
 * On error return -1 with errno set to the error code.
 */
int ntfs_logfile_reset(ntfs_volume *vol)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	int eo;

	if (!vol) {
		errno = EINVAL;
		return -1;
	}

	ni = ntfs_inode_open(vol, FILE_LogFile);
	if (!ni) {
		ntfs_log_perror("Failed to open inode FILE_LogFile");
		return -1;
	}

	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		eo = errno;
		ntfs_log_perror("Failed to open $FILE_LogFile/$DATA");
		goto error_exit;
	}

	if (ntfs_empty_logfile(na)) {
		eo = errno;
		ntfs_attr_close(na);
		goto error_exit;
	}
	
	ntfs_attr_close(na);
	return ntfs_inode_close(ni);

error_exit:
	ntfs_inode_close(ni);
	errno = eo;
	return -1;
}

/**
 * ntfs_volume_write_flags - set the flags of an ntfs volume
 * @vol:	ntfs volume where we set the volume flags
 * @flags:	new flags
 *
 * Set the on-disk volume flags in the mft record of $Volume and
 * on volume @vol to @flags.
 *
 * Return 0 if successful and -1 if not with errno set to the error code.
 */
int ntfs_volume_write_flags(ntfs_volume *vol, const le16 flags)
{
	ATTR_RECORD *a;
	VOLUME_INFORMATION *c;
	ntfs_attr_search_ctx *ctx;
	int ret = -1;	/* failure */

	if (!vol || !vol->vol_ni) {
		errno = EINVAL;
		return -1;
	}
	/* Get a pointer to the volume information attribute. */
	ctx = ntfs_attr_get_search_ctx(vol->vol_ni, NULL);
	if (!ctx)
		return -1;

	if (ntfs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, 0, NULL,
			0, ctx)) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION was not found "
			       "in $Volume!\n");
		goto err_out;
	}
	a = ctx->attr;
	/* Sanity check. */
	if (a->non_resident) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION must be resident "
			       "but it isn't.\n");
		errno = EIO;
		goto err_out;
	}
	/* Get a pointer to the value of the attribute. */
	c = (VOLUME_INFORMATION*)(le16_to_cpu(a->value_offset) + (char*)a);
	/* Sanity checks. */
	if ((char*)c + le32_to_cpu(a->value_length) > (char*)ctx->mrec +
			le32_to_cpu(ctx->mrec->bytes_in_use) ||
			le16_to_cpu(a->value_offset) +
			le32_to_cpu(a->value_length) > le32_to_cpu(a->length)) {
		ntfs_log_error("Attribute $VOLUME_INFORMATION in $Volume is "
			       "corrupt!\n");
		errno = EIO;
		goto err_out;
	}
	/* Set the volume flags. */
	vol->flags = c->flags = flags & VOLUME_FLAGS_MASK;
	/* Write them to disk. */
	ntfs_inode_mark_dirty(vol->vol_ni);
	if (ntfs_inode_sync(vol->vol_ni))
		goto err_out;

	ret = 0; /* success */
err_out:
	ntfs_attr_put_search_ctx(ctx);
	return ret;
}

int ntfs_volume_error(int err)
{
	int ret;

	switch (err) {
		case 0:
			ret = NTFS_VOLUME_OK;
			break;
		case EINVAL:
			ret = NTFS_VOLUME_NOT_NTFS;
			break;
		case EIO:
			ret = NTFS_VOLUME_CORRUPT;
			break;
		case EPERM:
			ret = NTFS_VOLUME_HIBERNATED;
			break;
		case EOPNOTSUPP:
			ret = NTFS_VOLUME_UNCLEAN_UNMOUNT;
			break;
		case EBUSY:
			ret = NTFS_VOLUME_LOCKED;
			break;
		case ENXIO:
			ret = NTFS_VOLUME_RAID;
			break;
		case EACCES:
			ret = NTFS_VOLUME_NO_PRIVILEGE;
			break;
		default:
			ret = NTFS_VOLUME_UNKNOWN_REASON;
			break;
	}
	return ret;
}


void ntfs_mount_error(const char *volume, const char *mntpoint, int err)
{
	switch (err) {
		case NTFS_VOLUME_NOT_NTFS:
			ntfs_log_error(invalid_ntfs_msg, volume);
			break;
		case NTFS_VOLUME_CORRUPT:
			ntfs_log_error("%s", corrupt_volume_msg);
			break;
		case NTFS_VOLUME_HIBERNATED:
			ntfs_log_error(hibernated_volume_msg, volume, mntpoint);
			break;
		case NTFS_VOLUME_UNCLEAN_UNMOUNT:
			ntfs_log_error("%s", unclean_journal_msg);
			break;
		case NTFS_VOLUME_LOCKED:
			ntfs_log_error("%s", opened_volume_msg);
			break;
		case NTFS_VOLUME_RAID:
			ntfs_log_error("%s", fakeraid_msg);
			break;
		case NTFS_VOLUME_NO_PRIVILEGE:
			ntfs_log_error(access_denied_msg, volume);
			break;
	}
}

int ntfs_set_locale(void)
{
	const char *locale;

	locale = setlocale(LC_ALL, "");
	if (!locale) {
		locale = setlocale(LC_ALL, NULL);
		ntfs_log_error("Couldn't set local environment, using default "
			       "'%s'.\n", locale);
		return 1;
	}
	return 0;
}

/*
 *		Feed the counts of free clusters and free mft records
 */

int ntfs_volume_get_free_space(ntfs_volume *vol)
{
	ntfs_attr *na;
	int ret;

	ret = -1; /* default return */
	vol->free_clusters = ntfs_attr_get_free_bits(vol->lcnbmp_na);
	if (vol->free_clusters < 0) {
		ntfs_log_perror("Failed to read NTFS $Bitmap");
	} else {
		na = vol->mftbmp_na;
		vol->free_mft_records = ntfs_attr_get_free_bits(na);

		if (vol->free_mft_records >= 0)
			vol->free_mft_records += (na->allocated_size - na->data_size) << 3;

		if (vol->free_mft_records < 0)
			ntfs_log_perror("Failed to calculate free MFT records");
		else
			ret = 0;
	}
	return (ret);
}
