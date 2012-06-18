/**
 * efs.c - Limited processing of encrypted files
 *
 *	This module is part of ntfs-3g library
 *
 * Copyright (c)      2009 Martin Bene
 * Copyright (c)      2009-2010 Jean-Pierre Andre
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#include "types.h"
#include "debug.h"
#include "attrib.h"
#include "inode.h"
#include "dir.h"
#include "efs.h"
#include "index.h"
#include "logging.h"
#include "misc.h"
#include "efs.h"

#ifdef HAVE_SETXATTR	/* extended attributes interface required */

static ntfschar logged_utility_stream_name[] = {
	const_cpu_to_le16('$'),
	const_cpu_to_le16('E'),
	const_cpu_to_le16('F'),
	const_cpu_to_le16('S'),
	const_cpu_to_le16(0)
} ;


/*
 *		Get the ntfs EFS info into an extended attribute
 */

int ntfs_get_efs_info(ntfs_inode *ni, char *value, size_t size)
{
	EFS_ATTR_HEADER *efs_info;
	s64 attr_size = 0;

	if (ni) {
		if (ni->flags & FILE_ATTR_ENCRYPTED) {
			efs_info = (EFS_ATTR_HEADER*)ntfs_attr_readall(ni,
				AT_LOGGED_UTILITY_STREAM,(ntfschar*)NULL, 0,
				&attr_size);
			if (efs_info
			    && (le32_to_cpu(efs_info->length) == attr_size)) {
				if (attr_size <= (s64)size) {
					if (value)
						memcpy(value,efs_info,attr_size);
					else {
						errno = EFAULT;
						attr_size = 0;
					}
				} else
					if (size) {
						errno = ERANGE;
						attr_size = 0;
					}
				free (efs_info);
			} else {
				if (efs_info) {
					free(efs_info);
					ntfs_log_error("Bad efs_info for inode %lld\n",
						(long long)ni->mft_no);
				} else {
					ntfs_log_error("Could not get efsinfo"
						" for inode %lld\n",
						(long long)ni->mft_no);
				}
				errno = EIO;
				attr_size = 0;
			}
		} else {
			errno = ENODATA;
			ntfs_log_trace("Inode %lld is not encrypted\n",
				(long long)ni->mft_no); 
		}
	}
	return (attr_size ? (int)attr_size : -errno);
}

/*
 *		Fix all encrypted AT_DATA attributes of an inode
 *
 *	The fix may require making an attribute non resident, which
 *	requires more space in the MFT record, and may cause some
 *	attribute to be expelled and the full record to be reorganized.
 *	When this happens, the search for data attributes has to be
 *	reinitialized.
 *
 *	Returns zero if successful.
 *		-1 if there is a problem.
 */

static int fixup_loop(ntfs_inode *ni)
{
	ntfs_attr_search_ctx *ctx;
	ntfs_attr *na;
	ATTR_RECORD *a;
	BOOL restart;
	BOOL first;
	int cnt;
	int maxcnt;
	int res = 0;

	maxcnt = 0;
	do {
		restart = FALSE;
		ctx = ntfs_attr_get_search_ctx(ni, NULL);
		if (!ctx) {
			ntfs_log_error("Failed to get ctx for efs\n");
			res = -1;
		}
		cnt = 0;
		while (!restart && !res
			&& !ntfs_attr_lookup(AT_DATA, NULL, 0, 
				   CASE_SENSITIVE, 0, NULL, 0, ctx)) {
			cnt++;
			a = ctx->attr;
			na = ntfs_attr_open(ctx->ntfs_ino, AT_DATA,
				(ntfschar*)((u8*)a + le16_to_cpu(a->name_offset)),
				a->name_length);
			if (!na) {
				ntfs_log_error("can't open DATA Attribute\n");
				res = -1;
			}
			if (na && !(ctx->attr->flags & ATTR_IS_ENCRYPTED)) {
				if (!NAttrNonResident(na)
				   && ntfs_attr_make_non_resident(na, ctx)) {
				/*
				 * ntfs_attr_make_non_resident fails if there
				 * is not enough space in the MFT record.
				 * When this happens, force making non-resident
				 * so that some other attribute is expelled.
				 */
					if (ntfs_attr_force_non_resident(na)) {
						res = -1;
					} else {
					/* make sure there is some progress */
						if (cnt <= maxcnt) {
							errno = EIO;
							ntfs_log_error("Multiple failure"
								" making non resident\n");
							res = -1;
						} else {
							ntfs_attr_put_search_ctx(ctx);
							ctx = (ntfs_attr_search_ctx*)NULL;
							restart = TRUE;
							maxcnt = cnt;
						}
					}
				}
				if (!restart && !res
				    && ntfs_efs_fixup_attribute(ctx, na)) {
					ntfs_log_error("Error in efs fixup of AT_DATA Attribute\n");
					res = -1;
				}
			}
		if (na)
			ntfs_attr_close(na);
		}
		first = FALSE;
	} while (restart && !res);
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	return (res);
}

/*
 *		Set the efs data from an extended attribute
 *	Warning : the new data is not checked
 *	Returns 0, or -1 if there is a problem
 */

int ntfs_set_efs_info(ntfs_inode *ni, const char *value, size_t size,
			int flags)
			
{
	int res;
	int written;
	ntfs_attr *na;
	const EFS_ATTR_HEADER *info_header;

	res = 0;
	if (ni && value && size) {
		if (ni->flags & (FILE_ATTR_ENCRYPTED | FILE_ATTR_COMPRESSED)) {
			if (ni->flags & FILE_ATTR_ENCRYPTED) {
				ntfs_log_trace("Inode %lld already encrypted\n",
						(long long)ni->mft_no);
				errno = EEXIST;
			} else {
				/*
				 * Possible problem : if encrypted file was
				 * restored in a compressed directory, it was
				 * restored as compressed.
				 * TODO : decompress first.
				 */
				ntfs_log_error("Inode %lld cannot be encrypted and compressed\n",
					(long long)ni->mft_no);
				errno = EIO;
			}
			return -1;
		}
		info_header = (const EFS_ATTR_HEADER*)value;
			/* make sure we get a likely efsinfo */
		if (le32_to_cpu(info_header->length) != size) {
			errno = EINVAL;
			return (-1);
		}
		if (!ntfs_attr_exist(ni,AT_LOGGED_UTILITY_STREAM,
				(ntfschar*)NULL,0)) {
			if (!(flags & XATTR_REPLACE)) {
			/*
			 * no logged_utility_stream attribute : add one,
			 * apparently, this does not feed the new value in
			 */
				res = ntfs_attr_add(ni,AT_LOGGED_UTILITY_STREAM,
					logged_utility_stream_name,4,
					(u8*)NULL,(s64)size);
			} else {
				errno = ENODATA;
				res = -1;
			}
		} else {
			errno = EEXIST;
			res = -1;
		}
		if (!res) {
			/*
			 * open and update the existing efs data
			 */
			na = ntfs_attr_open(ni, AT_LOGGED_UTILITY_STREAM,
				logged_utility_stream_name, 4);
			if (na) {
				/* resize attribute */
				res = ntfs_attr_truncate(na, (s64)size);
				/* overwrite value if any */
				if (!res && value) {
					written = (int)ntfs_attr_pwrite(na,
						 (s64)0, (s64)size, value);
					if (written != (s64)size) {
						ntfs_log_error("Failed to "
							"update efs data\n");
						errno = EIO;
						res = -1;
					}
				}
				ntfs_attr_close(na);
			} else
				res = -1;
		}
		if (!res) {
			/* Don't handle AT_DATA Attribute(s) if inode is a directory */
			if (!(ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)) {
				/* iterate over AT_DATA attributes */
                        	/* set encrypted flag, truncate attribute to match padding bytes */
			
			if (fixup_loop(ni))
				return -1;
			}
			ni->flags |= FILE_ATTR_ENCRYPTED;
			NInoSetDirty(ni);
			NInoFileNameSetDirty(ni);
		}
	} else {
		errno = EINVAL;
		res = -1;
	}
	return (res ? -1 : 0);
}

/*
 *              Fixup raw encrypted AT_DATA Attribute
 *     read padding length from last two bytes
 *     truncate attribute, make non-resident,
 *     set data size to match padding length
 *     set ATTR_IS_ENCRYPTED flag on attribute 
 *
 *	Return 0 if successful
 *		-1 if failed (errno tells why)
 */

int ntfs_efs_fixup_attribute(ntfs_attr_search_ctx *ctx, ntfs_attr *na) 
{
	u64 newsize;
	u64 oldsize;
	le16 appended_bytes;
	u16 padding_length;
	ntfs_inode *ni;
	BOOL close_ctx = FALSE;

	if (!na) {
		ntfs_log_error("no na specified for efs_fixup_attribute\n");
		goto err_out;
	}
	if (!ctx) {
		ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
		if (!ctx) {
			ntfs_log_error("Failed to get ctx for efs\n");
			goto err_out;
		}
		close_ctx = TRUE;
		if (ntfs_attr_lookup(AT_DATA, na->name, na->name_len, 
				CASE_SENSITIVE, 0, NULL, 0, ctx)) {
			ntfs_log_error("attr lookup for AT_DATA attribute failed in efs fixup\n");
			goto err_out;
		}
	} else {
		if (!NAttrNonResident(na)) {
			ntfs_log_error("Cannot make non resident"
				" when a context has been allocated\n");
			goto err_out;
		}
	}

		/* no extra bytes are added to void attributes */
	oldsize = na->data_size;
	if (oldsize) {
		/* make sure size is valid for a raw encrypted stream */
		if ((oldsize & 511) != 2) {
			ntfs_log_error("Bad raw encrypted stream\n");
			goto err_out;
		}
		/* read padding length from last two bytes of attribute */
		if (ntfs_attr_pread(na, oldsize - 2, 2, &appended_bytes) != 2) {
			ntfs_log_error("Error reading padding length\n");
			goto err_out;
		}
		padding_length = le16_to_cpu(appended_bytes);
		if (padding_length > 511 || padding_length > na->data_size-2) {
			errno = EINVAL;
			ntfs_log_error("invalid padding length %d for data_size %lld\n",
				 padding_length, (long long)oldsize);
			goto err_out;
		}
		newsize = oldsize - padding_length - 2;
		/*
		 * truncate attribute to possibly free clusters allocated 
		 * for the last two bytes, but do not truncate to new size
		 * to avoid losing useful data
		 */
		if (ntfs_attr_truncate(na, oldsize - 2)) {
			ntfs_log_error("Error truncating attribute\n");
			goto err_out;
		}
	} else
		newsize = 0;

	/*
	 * Encrypted AT_DATA Attributes MUST be non-resident
	 * This has to be done after the attribute is resized, as
	 * resizing down to zero may cause the attribute to be made
	 * resident.
	 */
	if (!NAttrNonResident(na)
	    && ntfs_attr_make_non_resident(na, ctx)) {
		if (!close_ctx
		    || ntfs_attr_force_non_resident(na)) {
			ntfs_log_error("Error making DATA attribute non-resident\n");
			goto err_out;
		} else {
			/*
			 * must reinitialize context after forcing
			 * non-resident. We need a context for updating
			 * the state, and at this point, we are sure
			 * the context is not used elsewhere.
			 */
			ntfs_attr_reinit_search_ctx(ctx);
			if (ntfs_attr_lookup(AT_DATA, na->name, na->name_len, 
					CASE_SENSITIVE, 0, NULL, 0, ctx)) {
				ntfs_log_error("attr lookup for AT_DATA attribute failed in efs fixup\n");
				goto err_out;
			}
		}
	}
	ni = na->ni;
	if (!na->name_len) {
		ni->data_size = newsize;
		ni->allocated_size = na->allocated_size;
	}
	NInoSetDirty(ni);
	NInoFileNameSetDirty(ni);

	ctx->attr->data_size = cpu_to_le64(newsize);
	if (le64_to_cpu(ctx->attr->initialized_size) > newsize)
		ctx->attr->initialized_size = ctx->attr->data_size;
	ctx->attr->flags |= ATTR_IS_ENCRYPTED;
	if (close_ctx)
		ntfs_attr_put_search_ctx(ctx);
		
	return (0);
err_out:
	if (close_ctx && ctx)
		ntfs_attr_put_search_ctx(ctx);
	return (-1);
}

#endif /* HAVE_SETXATTR */
