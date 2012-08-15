/**
 * utils.c - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2003-2006 Anton Altaparmakov
 * Copyright (c) 2003 Lode Leroy
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 *
 * A set of shared functions for ntfs utilities
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "utils.h"
#include "types.h"
#include "volume.h"
#include "debug.h"
#include "dir.h"
#include "version.h"
#include "logging.h"

const char *ntfs_bugs = "Developers' email address: "NTFS_DEV_LIST"\n";
const char *ntfs_home = "Linux NTFS homepage: http://www.linux-ntfs.org\n";
const char *ntfs_gpl = "This program is free software, released under the GNU "
	"General Public License\nand you are welcome to redistribute it under "
	"certain conditions.  It comes with\nABSOLUTELY NO WARRANTY; for "
	"details read the GNU General Public License to be\nfound in the file "
	"\"COPYING\" distributed with this program, or online at:\n"
	"http://www.gnu.org/copyleft/gpl.html\n";

static const char *invalid_ntfs_msg =
"The device '%s' doesn't have a valid NTFS.\n"
"Maybe you selected the wrong device? Or the whole disk instead of a\n"
"partition (e.g. /dev/hda, not /dev/hda1)? Or the other way around?\n";

static const char *corrupt_volume_msg =
"NTFS is inconsistent. Run chkdsk /f on Windows then reboot it TWICE!\n"
"The usage of the /f parameter is very IMPORTANT! No modification was\n"
"made to NTFS by this software.\n";

static const char *hibernated_volume_msg =
"The NTFS partition is hibernated. Please resume Windows and turned it \n"
"off properly, so mounting could be done safely.\n";

static const char *unclean_journal_msg =
"Access is denied because the NTFS journal file is unclean. Choices are:\n"
" A) Shutdown Windows properly.\n"
" B) Click the 'Safely Remove Hardware' icon in the Windows taskbar\n"
"    notification area before disconnecting the device.\n"
" C) Use 'Eject' from Windows Explorer to safely remove the device.\n"
" D) If you ran chkdsk previously then boot Windows again which will\n"
"    automatically initialize the journal.\n"
" E) Submit 'force' option (WARNING: This solution it not recommended).\n"
" F) ntfsmount: Mount the volume read-only by using the 'ro' mount option.\n";

static const char *opened_volume_msg =
"Access is denied because the NTFS volume is already exclusively opened.\n"
"The volume may be already mounted, or another software may use it which\n"
"could be identified for example by the help of the 'fuser' command.\n";

static const char *dirty_volume_msg =
"Volume is scheduled for check.\n"
"Please boot into Windows TWICE, or use the 'force' option.\n"
"NOTE: If you had not scheduled check and last time accessed this volume\n"
"using ntfsmount and shutdown system properly, then init scripts in your\n"
"distribution are broken. Please report to your distribution developers\n"
"(NOT to us!) that init scripts kill ntfsmount or mount.ntfs-fuse during\n"
"shutdown instead of proper umount.\n";

static const char *fakeraid_msg =
"You seem to have a SoftRAID/FakeRAID hardware and must use an activated,\n"
"different device under /dev/mapper, (e.g. /dev/mapper/nvidia_eahaabcc1)\n"
"to mount NTFS. Please see the 'dmraid' documentation for help.\n";

/**
 * utils_set_locale
 */
int utils_set_locale(void)
{
	const char *locale;

	locale = setlocale(LC_ALL, "");
	if (!locale) {
		locale = setlocale(LC_ALL, NULL);
		ntfs_log_error("Failed to set locale, using default '%s'.\n",
				locale);
		return 1;
	} else {
		return 0;
	}
}

/**
 * utils_valid_device - Perform some safety checks on the device, before start
 * @name:   Full pathname of the device/file to work with
 * @force:  Continue regardless of problems
 *
 * Check that the name refers to a device and that is isn't already mounted.
 * These checks can be overridden by using the force option.
 *
 * Return:  1  Success, we can continue
 *	    0  Error, we cannot use this device
 */
int utils_valid_device(const char *name, int force)
{
	unsigned long mnt_flags = 0;
	struct stat st;

#ifdef __CYGWIN32__
	/* FIXME: This doesn't work for Cygwin, so just return success. */
	return 1;
#endif
	if (!name) {
		errno = EINVAL;
		return 0;
	}

	if (stat(name, &st) == -1) {
		if (errno == ENOENT)
			ntfs_log_error("The device %s doesn't exist\n", name);
		else
			ntfs_log_perror("Error getting information about %s",
					name);
		return 0;
	}

	/* Make sure the file system is not mounted. */
	if (ntfs_check_if_mounted(name, &mnt_flags)) {
		ntfs_log_perror("Failed to determine whether %s is mounted",
				name);
		if (!force) {
			ntfs_log_error("Use the force option to ignore this "
					"error.\n");
			return 0;
		}
		ntfs_log_warning("Forced to continue.\n");
	} else if (mnt_flags & NTFS_MF_MOUNTED) {
		if (!force) {
			ntfs_log_error("%s", opened_volume_msg);
			ntfs_log_error("You can use force option to avoid this "
					"check, but this is not recommended\n"
					"and may lead to data corruption.\n");
			return 0;
		}
		ntfs_log_warning("Forced to continue.\n");
	}

	return 1;
}

/**
 * utils_mount_volume - Mount an NTFS volume
 */
ntfs_volume * utils_mount_volume(const char *device, ntfs_mount_flags flags)
{
	ntfs_volume *vol;

	if (!device) {
		errno = EINVAL;
		return NULL;
	}

	if (!utils_valid_device(device, flags & NTFS_MNT_FORCE))
		return NULL;

	vol = ntfs_mount(device, flags);
	if (!vol) {
		ntfs_log_perror("Failed to mount '%s'", device);
		if (errno == EINVAL)
			ntfs_log_error(invalid_ntfs_msg, device);
		else if (errno == EIO)
			ntfs_log_error("%s", corrupt_volume_msg);
		else if (errno == EPERM)
			ntfs_log_error("%s", hibernated_volume_msg);
		else if (errno == EOPNOTSUPP)
			ntfs_log_error("%s", unclean_journal_msg);
		else if (errno == EBUSY)
			ntfs_log_error("%s", opened_volume_msg);
		else if (errno == ENXIO)
			ntfs_log_error("%s", fakeraid_msg);
		return NULL;
	}

	if (NVolWasDirty(vol)) {
		if (!(flags & NTFS_MNT_FORCE)) {
			ntfs_log_error("%s", dirty_volume_msg);
			ntfs_umount(vol, FALSE);
			return NULL;
		}
		ntfs_log_error("WARNING: Dirty volume mount was forced by the "
				"'force' mount option.\n");
	}
	return vol;
}

/**
 * utils_parse_size - Convert a string representing a size
 * @value:  String to be parsed
 * @size:   Parsed size
 * @scale:  Whether or not to allow a suffix to scale the value
 *
 * Read a string and convert it to a number.  Strings may be suffixed to scale
 * them.  Any number without a suffix is assumed to be in bytes.
 *
 * Suffix  Description  Multiple
 *  [tT]    Terabytes     10^12
 *  [gG]    Gigabytes     10^9
 *  [mM]    Megabytes     10^6
 *  [kK]    Kilobytes     10^3
 *
 * Notes:
 *     Only the first character of the suffix is read.
 *     The multipliers are decimal thousands, not binary: 1000, not 1024.
 *     If parse_size fails, @size will not be changed
 *
 * Return:  1  Success
 *	    0  Error, the string was malformed
 */
int utils_parse_size(const char *value, s64 *size, BOOL scale)
{
	long long result;
	char *suffix = NULL;

	if (!value || !size) {
		errno = EINVAL;
		return 0;
	}

	ntfs_log_debug("Parsing size '%s'.\n", value);

	result = strtoll(value, &suffix, 0);
	if (result < 0 || errno == ERANGE) {
		ntfs_log_error("Invalid size '%s'.\n", value);
		return 0;
	}

	if (!suffix) {
		ntfs_log_error("Internal error, strtoll didn't return a suffix.\n");
		return 0;
	}

	if (scale) {
		switch (suffix[0]) {
			case 't': case 'T': result *= 1000;
			case 'g': case 'G': result *= 1000;
			case 'm': case 'M': result *= 1000;
			case 'k': case 'K': result *= 1000;
			case '-': case 0:
				break;
			default:
				ntfs_log_error("Invalid size suffix '%s'.  Use T, G, M, or K.\n", suffix);
				return 0;
		}
	} else {
		if ((suffix[0] != '-') && (suffix[0] != 0)) {
			ntfs_log_error("Invalid number '%.*s'.\n", (int)(suffix - value + 1), value);
			return 0;
		}
	}

	ntfs_log_debug("Parsed size = %lld.\n", result);
	*size = result;
	return 1;
}

/**
 * utils_parse_range - Convert a string representing a range of numbers
 * @string:  The string to be parsed
 * @start:   The beginning of the range will be stored here
 * @finish:  The end of the range will be stored here
 *
 * Read a string of the form n-m.  If the lower end is missing, zero will be
 * substituted.  If the upper end is missing LONG_MAX will be used.  If the
 * string cannot be parsed correctly, @start and @finish will not be changed.
 *
 * Return:  1  Success, a valid string was found
 *	    0  Error, the string was not a valid range
 */
int utils_parse_range(const char *string, s64 *start, s64 *finish, BOOL scale)
{
	s64 a, b;
	char *middle;

	if (!string || !start || !finish) {
		errno = EINVAL;
		return 0;
	}

	middle = strchr(string, '-');
	if (string == middle) {
		ntfs_log_debug("Range has no beginning, defaulting to 0.\n");
		a = 0;
	} else {
		if (!utils_parse_size(string, &a, scale))
			return 0;
	}

	if (middle) {
		if (middle[1] == 0) {
			b = LONG_MAX;		// XXX ULLONG_MAX
			ntfs_log_debug("Range has no end, defaulting to %lld.\n", b);
		} else {
			if (!utils_parse_size(middle+1, &b, scale))
				return 0;
		}
	} else {
		b = a;
	}

	ntfs_log_debug("Range '%s' = %lld - %lld\n", string, a, b);

	*start  = a;
	*finish = b;
	return 1;
}

/**
 * find_attribute - Find an attribute of the given type
 * @type:  An attribute type, e.g. AT_FILE_NAME
 * @ctx:   A search context, created using ntfs_get_attr_search_ctx
 *
 * Using the search context to keep track, find the first/next occurrence of a
 * given attribute type.
 *
 * N.B.  This will return a pointer into @mft.  As long as the search context
 *       has been created without an inode, it won't overflow the buffer.
 *
 * Return:  Pointer  Success, an attribute was found
 *	    NULL     Error, no matching attributes were found
 */
ATTR_RECORD * find_attribute(const ATTR_TYPES type, ntfs_attr_search_ctx *ctx)
{
	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}

	if (ntfs_attr_lookup(type, NULL, 0, 0, 0, NULL, 0, ctx) != 0) {
		ntfs_log_debug("find_attribute didn't find an attribute of type: 0x%02x.\n", type);
		return NULL;	/* None / no more of that type */
	}

	ntfs_log_debug("find_attribute found an attribute of type: 0x%02x.\n", type);
	return ctx->attr;
}

/**
 * find_first_attribute - Find the first attribute of a given type
 * @type:  An attribute type, e.g. AT_FILE_NAME
 * @mft:   A buffer containing a raw MFT record
 *
 * Search through a raw MFT record for an attribute of a given type.
 * The return value is a pointer into the MFT record that was supplied.
 *
 * N.B.  This will return a pointer into @mft.  The pointer won't stray outside
 *       the buffer, since we created the search context without an inode.
 *
 * Return:  Pointer  Success, an attribute was found
 *	    NULL     Error, no matching attributes were found
 */
ATTR_RECORD * find_first_attribute(const ATTR_TYPES type, MFT_RECORD *mft)
{
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *rec;

	if (!mft) {
		errno = EINVAL;
		return NULL;
	}

	ctx = ntfs_attr_get_search_ctx(NULL, mft);
	if (!ctx) {
		ntfs_log_error("Couldn't create a search context.\n");
		return NULL;
	}

	rec = find_attribute(type, ctx);
	ntfs_attr_put_search_ctx(ctx);
	if (rec)
		ntfs_log_debug("find_first_attribute: found attr of type 0x%02x.\n", type);
	else
		ntfs_log_debug("find_first_attribute: didn't find attr of type 0x%02x.\n", type);
	return rec;
}

/**
 * utils_inode_get_name
 *
 * using inode
 * get filename
 * add name to list
 * get parent
 * if parent is 5 (/) stop
 * get inode of parent
 */
#define max_path 20
int utils_inode_get_name(ntfs_inode *inode, char *buffer, int bufsize)
{
	// XXX option: names = posix/win32 or dos
	// flags: path, filename, or both


	ntfs_volume *vol;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *rec;
	FILE_NAME_ATTR *attr;
	int name_space;
	MFT_REF parent = FILE_root;
	char *names[max_path + 1];// XXX ntfs_malloc? and make max bigger?
	int i, len, offset = 0;

	if (!inode || !buffer) {
		errno = EINVAL;
		return 0;
	}

	vol = inode->vol;

	//ntfs_log_debug("sizeof(char*) = %d, sizeof(names) = %d\n", sizeof(char*), sizeof(names));
	memset(names, 0, sizeof(names));

	for (i = 0; i < max_path; i++) {

		ctx = ntfs_attr_get_search_ctx(inode, NULL);
		if (!ctx) {
			ntfs_log_error("Couldn't create a search context.\n");
			return 0;
		}

		//ntfs_log_debug("i = %d, inode = %p (%lld)\n", i, inode, inode->mft_no);

		name_space = 4;
		while ((rec = find_attribute(AT_FILE_NAME, ctx))) {
			/* We know this will always be resident. */
			attr = (FILE_NAME_ATTR *) ((char *) rec + le16_to_cpu(rec->value_offset));

			if (attr->file_name_type > name_space) { //XXX find the ...
				continue;
			}

			name_space = attr->file_name_type;
			parent     = le64_to_cpu(attr->parent_directory);

			if (names[i]) {
				free(names[i]);
				names[i] = NULL;
			}

			if (ntfs_ucstombs(attr->file_name, attr->file_name_length,
			    &names[i], 0) < 0) {
				char *temp;
				ntfs_log_error("Couldn't translate filename to current locale.\n");
				temp = ntfs_malloc(30);
				if (!temp)
					return 0;
				snprintf(temp, 30, "<MFT%llu>", (unsigned
						long long)inode->mft_no);
				names[i] = temp;
			}

			//ntfs_log_debug("names[%d] %s\n", i, names[i]);
			//ntfs_log_debug("parent = %lld\n", MREF(parent));
		}

		ntfs_attr_put_search_ctx(ctx);

		if (i > 0)			/* Don't close the original inode */
			ntfs_inode_close(inode);

		if (MREF(parent) == FILE_root) {	/* The root directory, stop. */
			//ntfs_log_debug("inode 5\n");
			break;
		}

		inode = ntfs_inode_open(vol, parent);
		if (!inode) {
			ntfs_log_error("Couldn't open inode %llu.\n",
					(unsigned long long)MREF(parent));
			break;
		}
	}

	if (i >= max_path) {
		/* If we get into an infinite loop, we'll end up here. */
		ntfs_log_error("The directory structure is too deep (over %d) nested directories.\n", max_path);
		return 0;
	}

	/* Assemble the names in the correct order. */
	for (i = max_path; i >= 0; i--) {
		if (!names[i])
			continue;

		len = snprintf(buffer + offset, bufsize - offset, "%c%s", PATH_SEP, names[i]);
		if (len >= (bufsize - offset)) {
			ntfs_log_error("Pathname was truncated.\n");
			break;
		}

		offset += len;
	}

	/* Free all the allocated memory */
	for (i = 0; i < max_path; i++)
		free(names[i]);

	ntfs_log_debug("Pathname: %s\n", buffer);

	return 1;
}
#undef max_path

/**
 * utils_attr_get_name
 */
int utils_attr_get_name(ntfs_volume *vol, ATTR_RECORD *attr, char *buffer, int bufsize)
{
	int len, namelen;
	char *name;
	ATTR_DEF *attrdef;

	// flags: attr, name, or both
	if (!attr || !buffer) {
		errno = EINVAL;
		return 0;
	}

	attrdef = ntfs_attr_find_in_attrdef(vol, attr->type);
	if (attrdef) {
		name    = NULL;
		namelen = ntfs_ucsnlen(attrdef->name, sizeof(attrdef->name));
		if (ntfs_ucstombs(attrdef->name, namelen, &name, 0) < 0) {
			ntfs_log_error("Couldn't translate attribute type to "
					"current locale.\n");
			// <UNKNOWN>?
			return 0;
		}
		len = snprintf(buffer, bufsize, "%s", name);
	} else {
		ntfs_log_error("Unknown attribute type 0x%02x\n", attr->type);
		len = snprintf(buffer, bufsize, "<UNKNOWN>");
	}

	if (len >= bufsize) {
		ntfs_log_error("Attribute type was truncated.\n");
		return 0;
	}

	if (!attr->name_length) {
		return 0;
	}

	buffer  += len;
	bufsize -= len;

	name    = NULL;
	namelen = attr->name_length;
	if (ntfs_ucstombs((ntfschar *)((char *)attr + le16_to_cpu(
			attr->name_offset)), namelen, &name, 0) < 0) {
		ntfs_log_error("Couldn't translate attribute name to current "
				"locale.\n");
		// <UNKNOWN>?
		len = snprintf(buffer, bufsize, "<UNKNOWN>");
		return 0;
	}

	len = snprintf(buffer, bufsize, "(%s)", name);
	free(name);

	if (len >= bufsize) {
		ntfs_log_error("Attribute name was truncated.\n");
		return 0;
	}

	return 0;
}

/**
 * utils_cluster_in_use - Determine if a cluster is in use
 * @vol:  An ntfs volume obtained from ntfs_mount
 * @lcn:  The Logical Cluster Number to test
 *
 * The metadata file $Bitmap has one binary bit representing each cluster on
 * disk.  The bit will be set for each cluster that is in use.  The function
 * reads the relevant part of $Bitmap into a buffer and tests the bit.
 *
 * This function has a static buffer in which it caches a section of $Bitmap.
 * If the lcn, being tested, lies outside the range, the buffer will be
 * refreshed. @bmplcn stores offset to the first bit (in bits) stored in the
 * buffer.
 *
 * NOTE: Be very carefull with shifts by 3 everywhere in this function.
 *
 * Return:  1  Cluster is in use
 *	    0  Cluster is free space
 *	   -1  Error occurred
 */
int utils_cluster_in_use(ntfs_volume *vol, long long lcn)
{
	static unsigned char buffer[512];
	static long long bmplcn = -(sizeof(buffer) << 3);
	int byte, bit;
	ntfs_attr *attr;

	if (!vol) {
		errno = EINVAL;
		return -1;
	}

	/* Does lcn lie in the section of $Bitmap we already have cached? */
	if ((lcn < bmplcn) || (lcn >= (bmplcn + (sizeof(buffer) << 3)))) {
		ntfs_log_debug("Bit lies outside cache.\n");
		attr = ntfs_attr_open(vol->lcnbmp_ni, AT_DATA, AT_UNNAMED, 0);
		if (!attr) {
			ntfs_log_perror("Couldn't open $Bitmap");
			return -1;
		}

		/* Mark the buffer as in use, in case the read is shorter. */
		memset(buffer, 0xFF, sizeof(buffer));
		bmplcn = lcn & (~((sizeof(buffer) << 3) - 1));

		if (ntfs_attr_pread(attr, (bmplcn >> 3), sizeof(buffer),
					buffer) < 0) {
			ntfs_log_perror("Couldn't read $Bitmap");
			ntfs_attr_close(attr);
			return -1;
		}

		ntfs_log_debug("Reloaded bitmap buffer.\n");
		ntfs_attr_close(attr);
	}

	bit  = 1 << (lcn & 7);
	byte = (lcn >> 3) & (sizeof(buffer) - 1);
	ntfs_log_debug("cluster = %lld, bmplcn = %lld, byte = %d, bit = %d, "
			"in use %d\n", lcn, bmplcn, byte, bit, buffer[byte] &
			bit);

	return (buffer[byte] & bit);
}

/**
 * utils_mftrec_in_use - Determine if a MFT Record is in use
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @mref:  MFT Reference (inode number)
 *
 * The metadata file $BITMAP has one binary bit representing each record in the
 * MFT.  The bit will be set for each record that is in use.  The function
 * reads the relevant part of $BITMAP into a buffer and tests the bit.
 *
 * This function has a static buffer in which it caches a section of $BITMAP.
 * If the mref, being tested, lies outside the range, the buffer will be
 * refreshed.
 *
 * Return:  1  MFT Record is in use
 *	    0  MFT Record is unused
 *	   -1  Error occurred
 */
int utils_mftrec_in_use(ntfs_volume *vol, MFT_REF mref)
{
	static u8 buffer[512];
	static s64 bmpmref = -sizeof(buffer) - 1; /* Which bit of $BITMAP is in the buffer */
	int byte, bit;

	ntfs_log_trace("Entering.\n");

	if (!vol) {
		errno = EINVAL;
		return -1;
	}

	/* Does mref lie in the section of $Bitmap we already have cached? */
	if (((s64)MREF(mref) < bmpmref) || ((s64)MREF(mref) >= (bmpmref +
			(sizeof(buffer) << 3)))) {
		ntfs_log_debug("Bit lies outside cache.\n");

		/* Mark the buffer as not in use, in case the read is shorter. */
		memset(buffer, 0, sizeof(buffer));
		bmpmref = mref & (~((sizeof(buffer) << 3) - 1));

		if (ntfs_attr_pread(vol->mftbmp_na, (bmpmref>>3), sizeof(buffer), buffer) < 0) {
			ntfs_log_perror("Couldn't read $MFT/$BITMAP");
			return -1;
		}

		ntfs_log_debug("Reloaded bitmap buffer.\n");
	}

	bit  = 1 << (mref & 7);
	byte = (mref >> 3) & (sizeof(buffer) - 1);
	ntfs_log_debug("cluster = %lld, bmpmref = %lld, byte = %d, bit = %d, in use %d\n", mref, bmpmref, byte, bit, buffer[byte] & bit);

	return (buffer[byte] & bit);
}

/**
 * __metadata
 */
static int __metadata(ntfs_volume *vol, u64 num)
{
	if (num <= FILE_UpCase)
		return 1;
	if (!vol)
		return -1;
	if ((vol->major_ver == 3) && (num == FILE_Extend))
		return 1;

	return 0;
}

/**
 * utils_is_metadata - Determine if an inode represents a metadata file
 * @inode:  An ntfs inode to be tested
 *
 * A handful of files in the volume contain filesystem data - metadata.
 * They can be identified by their inode number (offset in MFT/$DATA) or by
 * their parent.
 *
 * Return:  1  inode is a metadata file
 *	    0  inode is not a metadata file
 *	   -1  Error occurred
 */
int utils_is_metadata(ntfs_inode *inode)
{
	ntfs_volume *vol;
	ATTR_RECORD *rec;
	FILE_NAME_ATTR *attr;
	MFT_RECORD *file;
	u64 num;

	if (!inode) {
		errno = EINVAL;
		return -1;
	}

	vol = inode->vol;
	if (!vol)
		return -1;

	num = inode->mft_no;
	if (__metadata(vol, num) == 1)
		return 1;

	file = inode->mrec;
	if (file && (file->base_mft_record != 0)) {
		num = MREF_LE(file->base_mft_record);
		if (__metadata(vol, num) == 1)
			return 1;
	}
	file = inode->mrec;

	rec = find_first_attribute(AT_FILE_NAME, inode->mrec);
	if (!rec)
		return -1;

	/* We know this will always be resident. */
	attr = (FILE_NAME_ATTR *)((char *)rec + le16_to_cpu(rec->value_offset));

	num = MREF_LE(attr->parent_directory);
	if ((num != FILE_root) && (__metadata(vol, num) == 1))
		return 1;

	return 0;
}

/**
 * utils_dump_mem - Display a block of memory in hex and ascii
 * @buf:     Buffer to be displayed
 * @start:   Offset into @buf to start from
 * @length:  Number of bytes to display
 * @flags:   Options to change the style of the output
 *
 * Display a block of memory in a tradition hex-dump manner.
 * Optionally the ascii part can be turned off.
 *
 * The flags, described fully in utils.h, default to 0 (DM_DEFAULTS).
 * Examples are: DM_INDENT (indent the output by one tab); DM_RED (colour the
 * output); DM_NO_ASCII (only print the hex values).
 */
void utils_dump_mem(void *buf, int start, int length, int flags)
{
	int off, i, s, e, col;
	u8 *mem = buf;

	s =  start                & ~15;	// round down
	e = (start + length + 15) & ~15;	// round up

	for (off = s; off < e; off += 16) {
		col = 30;
		if (flags & DM_RED)
			col += 1;
		if (flags & DM_GREEN)
			col += 2;
		if (flags & DM_BLUE)
			col += 4;
		if (flags & DM_INDENT)
			ntfs_log_debug("\t");
		if (flags & DM_BOLD)
			ntfs_log_debug("\e[01m");
		if (flags & (DM_RED | DM_BLUE | DM_GREEN | DM_BOLD))
			ntfs_log_debug("\e[%dm", col);
		if (off == s)
			ntfs_log_debug("%6.6x ", start);
		else
			ntfs_log_debug("%6.6x ", off);

		for (i = 0; i < 16; i++) {
			if ((i == 8) && (!(flags & DM_NO_DIVIDER)))
				ntfs_log_debug(" -");
			if (((off+i) >= start) && ((off+i) < (start+length)))
				ntfs_log_debug(" %02X", mem[off+i]);
			else
				ntfs_log_debug("   ");
		}
		if (!(flags & DM_NO_ASCII)) {
			ntfs_log_debug("  ");
			for (i = 0; i < 16; i++) {
				if (((off+i) < start) || ((off+i) >= (start+length)))
					ntfs_log_debug(" ");
				else if (isprint(mem[off + i]))
					ntfs_log_debug("%c", mem[off + i]);
				else
					ntfs_log_debug(".");
			}
		}
		if (flags & (DM_RED | DM_BLUE | DM_GREEN | DM_BOLD))
			ntfs_log_debug("\e[0m");
		ntfs_log_debug("\n");
	}
}


/**
 * mft_get_search_ctx
 */
struct mft_search_ctx * mft_get_search_ctx(ntfs_volume *vol)
{
	struct mft_search_ctx *ctx;

	if (!vol) {
		errno = EINVAL;
		return NULL;
	}

	ctx = calloc(1, sizeof *ctx);

	ctx->mft_num = -1;
	ctx->vol = vol;

	return ctx;
}

/**
 * mft_put_search_ctx
 */
void mft_put_search_ctx(struct mft_search_ctx *ctx)
{
	if (!ctx)
		return;
	if (ctx->inode)
		ntfs_inode_close(ctx->inode);
	free(ctx);
}

/**
 * mft_next_record
 */
int mft_next_record(struct mft_search_ctx *ctx)
{
	s64 nr_mft_records;
	ATTR_RECORD *attr10 = NULL;
	ATTR_RECORD *attr20 = NULL;
	ATTR_RECORD *attr80 = NULL;
	ntfs_attr_search_ctx *attr_ctx;

	if (!ctx) {
		errno = EINVAL;
		return -1;
	}

	if (ctx->inode) {
		ntfs_inode_close(ctx->inode);
		ctx->inode = NULL;
	}

	nr_mft_records = ctx->vol->mft_na->initialized_size >>
			ctx->vol->mft_record_size_bits;

	for (ctx->mft_num++; (s64)ctx->mft_num < nr_mft_records; ctx->mft_num++) {
		int in_use;

		ctx->flags_match = 0;
		in_use = utils_mftrec_in_use(ctx->vol, (MFT_REF) ctx->mft_num);
		if (in_use == -1) {
			ntfs_log_error("Error reading inode %llu.  Aborting.\n",
					(unsigned long long)ctx->mft_num);
			return -1;
		}

		if (in_use) {
			ctx->flags_match |= FEMR_IN_USE;

			ctx->inode = ntfs_inode_open(ctx->vol, (MFT_REF) ctx->mft_num);
			if (ctx->inode == NULL) {
				ntfs_log_error("Error reading inode %llu.\n", (unsigned
						long long) ctx->mft_num);
				continue;
			}

			attr10 = find_first_attribute(AT_STANDARD_INFORMATION, ctx->inode->mrec);
			attr20 = find_first_attribute(AT_ATTRIBUTE_LIST,       ctx->inode->mrec);
			attr80 = find_first_attribute(AT_DATA, ctx->inode->mrec);

			if (attr10)
				ctx->flags_match |= FEMR_BASE_RECORD;
			else
				ctx->flags_match |= FEMR_NOT_BASE_RECORD;

			if (attr20)
				ctx->flags_match |= FEMR_BASE_RECORD;

			if (attr80)
				ctx->flags_match |= FEMR_FILE;

			if (ctx->flags_search & FEMR_DIR) {
				attr_ctx = ntfs_attr_get_search_ctx(ctx->inode, NULL);
				if (attr_ctx) {
					if (ntfs_attr_lookup(AT_INDEX_ROOT, NTFS_INDEX_I30, 4, 0, 0, NULL, 0, attr_ctx) == 0)
						ctx->flags_match |= FEMR_DIR;

					ntfs_attr_put_search_ctx(attr_ctx);
				} else {
					ntfs_log_error("Couldn't create a search context.\n");
					return -1;
				}
			}

			switch (utils_is_metadata(ctx->inode)) {
				case 1: ctx->flags_match |= FEMR_METADATA;     break;
				case 0: ctx->flags_match |= FEMR_NOT_METADATA; break;
				default:
					ctx->flags_match |= FEMR_NOT_METADATA; break;
					//ntfs_log_error("Error reading inode %lld.\n", ctx->mft_num);
					//return -1;
			}

		} else {		// !in_use
			ntfs_attr *mft;

			ctx->flags_match |= FEMR_NOT_IN_USE;

			ctx->inode = calloc(1, sizeof(*ctx->inode));
			if (!ctx->inode) {
				ntfs_log_error("Out of memory.  Aborting.\n");
				return -1;
			}

			ctx->inode->mft_no = ctx->mft_num;
			ctx->inode->vol    = ctx->vol;
			ctx->inode->mrec   = ntfs_malloc(ctx->vol->mft_record_size);
			if (!ctx->inode->mrec) {
				free(ctx->inode); // == ntfs_inode_close
				return -1;
			}

			mft = ntfs_attr_open(ctx->vol->mft_ni, AT_DATA,
					AT_UNNAMED, 0);
			if (!mft) {
				ntfs_log_perror("Couldn't open $MFT/$DATA");
				// free / close
				return -1;
			}

			if (ntfs_attr_pread(mft, ctx->vol->mft_record_size * ctx->mft_num, ctx->vol->mft_record_size, ctx->inode->mrec) < ctx->vol->mft_record_size) {
				ntfs_log_perror("Couldn't read MFT Record %llu",
					(unsigned long long) ctx->mft_num);
				// free / close
				ntfs_attr_close(mft);
				return -1;
			}

			ntfs_attr_close(mft);
		}

		if (ctx->flags_match & ctx->flags_search) {
			break;
		}

		if (ntfs_inode_close(ctx->inode)) {
			ntfs_log_error("Error closing inode %llu.\n",
					(unsigned long long)ctx->mft_num);
			return -errno;
		}

		ctx->inode = NULL;
	}

	return (ctx->inode == NULL);
}


