/**
 * inode.c - Inode handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Anton Altaparmakov
 * Copyright (c) 2002-2008 Szabolcs Szakacsits
 * Copyright (c) 2004-2007 Yura Pakhuchiy
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2009-2010 Jean-Pierre Andre
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "param.h"
#include "compat.h"
#include "types.h"
#include "volume.h"
#include "cache.h"
#include "inode.h"
#include "attrib.h"
#include "debug.h"
#include "mft.h"
#include "attrlist.h"
#include "runlist.h"
#include "lcnalloc.h"
#include "index.h"
#include "dir.h"
#include "ntfstime.h"
#include "logging.h"
#include "misc.h"

ntfs_inode *ntfs_inode_base(ntfs_inode *ni)
{
	if (ni->nr_extents == -1)
		return ni->base_ni;
	return ni;
}

/**
 * ntfs_inode_mark_dirty - set the inode (and its base inode if it exists) dirty
 * @ni:		ntfs inode to set dirty
 *
 * Set the inode @ni dirty so it is written out later (at the latest at
 * ntfs_inode_close() time). If @ni is an extent inode, set the base inode
 * dirty, too.
 *
 * This function cannot fail.
 */
void ntfs_inode_mark_dirty(ntfs_inode *ni)
{
	NInoSetDirty(ni);
	if (ni->nr_extents == -1)
		NInoSetDirty(ni->base_ni);
}

/**
 * __ntfs_inode_allocate - Create and initialise an NTFS inode object
 * @vol:
 *
 * Description...
 *
 * Returns:
 */
static ntfs_inode *__ntfs_inode_allocate(ntfs_volume *vol)
{
	ntfs_inode *ni;

	ni = (ntfs_inode*)ntfs_calloc(sizeof(ntfs_inode));
	if (ni)
		ni->vol = vol;
	return ni;
}

/**
 * ntfs_inode_allocate - Create an NTFS inode object
 * @vol:
 *
 * Description...
 *
 * Returns:
 */
ntfs_inode *ntfs_inode_allocate(ntfs_volume *vol)
{
	return __ntfs_inode_allocate(vol);
}

/**
 * __ntfs_inode_release - Destroy an NTFS inode object
 * @ni:
 *
 * Description...
 *
 * Returns:
 */
static void __ntfs_inode_release(ntfs_inode *ni)
{
	if (NInoDirty(ni))
		ntfs_log_error("Releasing dirty inode %lld!\n", 
			       (long long)ni->mft_no);
	if (NInoAttrList(ni) && ni->attr_list)
		free(ni->attr_list);
	free(ni->mrec);
	free(ni);
	return;
}

/**
 * ntfs_inode_open - open an inode ready for access
 * @vol:	volume to get the inode from
 * @mref:	inode number / mft record number to open
 *
 * Allocate an ntfs_inode structure and initialize it for the given inode
 * specified by @mref. @mref specifies the inode number / mft record to read,
 * including the sequence number, which can be 0 if no sequence number checking
 * is to be performed.
 *
 * Then, allocate a buffer for the mft record, read the mft record from the
 * volume @vol, and attach it to the ntfs_inode structure (->mrec). The
 * mft record is mst deprotected and sanity checked for validity and we abort
 * if deprotection or checks fail.
 *
 * Finally, search for an attribute list attribute in the mft record and if one
 * is found, load the attribute list attribute value and attach it to the
 * ntfs_inode structure (->attr_list). Also set the NI_AttrList bit to indicate
 * this.
 *
 * Return a pointer to the ntfs_inode structure on success or NULL on error,
 * with errno set to the error code.
 */
static ntfs_inode *ntfs_inode_real_open(ntfs_volume *vol, const MFT_REF mref)
{
	s64 l;
	ntfs_inode *ni = NULL;
	ntfs_attr_search_ctx *ctx;
	STANDARD_INFORMATION *std_info;
	le32 lthle;
	int olderrno;

	ntfs_log_enter("Entering for inode %lld\n", (long long)MREF(mref));
	if (!vol) {
		errno = EINVAL;
		goto out;
	}
	ni = __ntfs_inode_allocate(vol);
	if (!ni)
		goto out;
	if (ntfs_file_record_read(vol, mref, &ni->mrec, NULL))
		goto err_out;
	if (!(ni->mrec->flags & MFT_RECORD_IN_USE)) {
		errno = ENOENT;
		goto err_out;
	}
	ni->mft_no = MREF(mref);
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		goto err_out;
	/* Receive some basic information about inode. */
	if (ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED,
				0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		if (!ni->mrec->base_mft_record)
			ntfs_log_perror("No STANDARD_INFORMATION in base record"
					" %lld", (long long)MREF(mref));
		goto put_err_out;
	}
	std_info = (STANDARD_INFORMATION *)((u8 *)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	ni->flags = std_info->file_attributes;
	ni->creation_time = std_info->creation_time;
	ni->last_data_change_time = std_info->last_data_change_time;
	ni->last_mft_change_time = std_info->last_mft_change_time;
	ni->last_access_time = std_info->last_access_time;
  		/* JPA insert v3 extensions if present */
                /* length may be seen as 72 (v1.x) or 96 (v3.x) */
	lthle = ctx->attr->length;
	if (le32_to_cpu(lthle) > sizeof(STANDARD_INFORMATION)) {
		set_nino_flag(ni, v3_Extensions);
		ni->owner_id = std_info->owner_id;
		ni->security_id = std_info->security_id;
		ni->quota_charged = std_info->quota_charged;
		ni->usn = std_info->usn;
	} else {
		clear_nino_flag(ni, v3_Extensions);
		ni->owner_id = const_cpu_to_le32(0);
		ni->security_id = const_cpu_to_le32(0);
	}
	/* Set attribute list information. */
	olderrno = errno;
	if (ntfs_attr_lookup(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		if (errno != ENOENT)
			goto put_err_out;
		/* Attribute list attribute does not present. */
		/* restore previous errno to avoid misinterpretation */
		errno = olderrno;
		goto get_size;
	}
	NInoSetAttrList(ni);
	l = ntfs_get_attribute_value_length(ctx->attr);
	if (!l)
		goto put_err_out;
	if (l > 0x40000) {
		errno = EIO;
		ntfs_log_perror("Too large attrlist attribute (%lld), inode "
				"%lld", (long long)l, (long long)MREF(mref));
		goto put_err_out;
	}
	ni->attr_list_size = l;
	ni->attr_list = ntfs_malloc(ni->attr_list_size);
	if (!ni->attr_list)
		goto put_err_out;
	l = ntfs_get_attribute_value(vol, ctx->attr, ni->attr_list);
	if (!l)
		goto put_err_out;
	if (l != ni->attr_list_size) {
		errno = EIO;
		ntfs_log_perror("Unexpected attrlist size (%lld <> %u), inode "
				"%lld", (long long)l, ni->attr_list_size, 
				(long long)MREF(mref));
		goto put_err_out;
	}
get_size:
	olderrno = errno;
	if (ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
		if (errno != ENOENT)
			goto put_err_out;
		/* Directory or special file. */
		/* restore previous errno to avoid misinterpretation */
		errno = olderrno;
		ni->data_size = ni->allocated_size = 0;
	} else {
		if (ctx->attr->non_resident) {
			ni->data_size = sle64_to_cpu(ctx->attr->data_size);
			if (ctx->attr->flags &
					(ATTR_IS_COMPRESSED | ATTR_IS_SPARSE))
				ni->allocated_size = sle64_to_cpu(
						ctx->attr->compressed_size);
			else
				ni->allocated_size = sle64_to_cpu(
						ctx->attr->allocated_size);
		} else {
			ni->data_size = le32_to_cpu(ctx->attr->value_length);
			ni->allocated_size = (ni->data_size + 7) & ~7;
		}
		set_nino_flag(ni,KnownSize);
	}
	ntfs_attr_put_search_ctx(ctx);
out:	
	ntfs_log_leave("\n");
	return ni;

put_err_out:
	ntfs_attr_put_search_ctx(ctx);
err_out:
	__ntfs_inode_release(ni);
	ni = NULL;
	goto out;
}

/**
 * ntfs_inode_close - close an ntfs inode and free all associated memory
 * @ni:		ntfs inode to close
 *
 * Make sure the ntfs inode @ni is clean.
 *
 * If the ntfs inode @ni is a base inode, close all associated extent inodes,
 * then deallocate all memory attached to it, and finally free the ntfs inode
 * structure itself.
 *
 * If it is an extent inode, we disconnect it from its base inode before we
 * destroy it.
 *
 * It is OK to pass NULL to this function, it is just noop in this case.
 *
 * Return 0 on success or -1 on error with errno set to the error code. On
 * error, @ni has not been freed. The user should attempt to handle the error
 * and call ntfs_inode_close() again. The following error codes are defined:
 *
 *	EBUSY	@ni and/or its attribute list runlist is/are dirty and the
 *		attempt to write it/them to disk failed.
 *	EINVAL	@ni is invalid (probably it is an extent inode).
 *	EIO	I/O error while trying to write inode to disk.
 */

int ntfs_inode_real_close(ntfs_inode *ni)
{
	int ret = -1;
	
	if (!ni)
		return 0;

	ntfs_log_enter("Entering for inode %lld\n", (long long)ni->mft_no);

	/* If we have dirty metadata, write it out. */
	if (NInoDirty(ni) || NInoAttrListDirty(ni)) {
		if (ntfs_inode_sync(ni)) {
			if (errno != EIO)
				errno = EBUSY;
			goto err;
		}
	}
	/* Is this a base inode with mapped extent inodes? */
	if (ni->nr_extents > 0) {
		while (ni->nr_extents > 0) {
			if (ntfs_inode_real_close(ni->extent_nis[0])) {
				if (errno != EIO)
					errno = EBUSY;
				goto err;
			}
		}
	} else if (ni->nr_extents == -1) {
		ntfs_inode **tmp_nis;
		ntfs_inode *base_ni;
		s32 i;

		/*
		 * If the inode is an extent inode, disconnect it from the
		 * base inode before destroying it.
		 */
		base_ni = ni->base_ni;
		for (i = 0; i < base_ni->nr_extents; ++i) {
			tmp_nis = base_ni->extent_nis;
			if (tmp_nis[i] != ni)
				continue;
			/* Found it. Disconnect. */
			memmove(tmp_nis + i, tmp_nis + i + 1,
					(base_ni->nr_extents - i - 1) *
					sizeof(ntfs_inode *));
			/* Buffer should be for multiple of four extents. */
			if ((--base_ni->nr_extents) & 3) {
				i = -1;
				break;
			}
			/*
			 * ElectricFence is unhappy with realloc(x,0) as free(x)
			 * thus we explicitly separate these two cases.
			 */
			if (base_ni->nr_extents) {
				/* Resize the memory buffer. */
				tmp_nis = realloc(tmp_nis, base_ni->nr_extents *
						  sizeof(ntfs_inode *));
				/* Ignore errors, they don't really matter. */
				if (tmp_nis)
					base_ni->extent_nis = tmp_nis;
			} else if (tmp_nis) {
				free(tmp_nis);
				base_ni->extent_nis = (ntfs_inode**)NULL;
			}
			/* Allow for error checking. */
			i = -1;
			break;
		}
		
		/* 
		 *  We could successfully sync, so only log this error
		 *  and try to sync other inode extents too.
		 */
		if (i != -1)
			ntfs_log_error("Extent inode %lld was not found\n",
				       (long long)ni->mft_no);
	}
	
	__ntfs_inode_release(ni);
	ret = 0;
err:
	ntfs_log_leave("\n");
	return ret;
}

#if CACHE_NIDATA_SIZE

/*
 *		Free an inode structure when there is not more space
 *	in the cache
 */

void ntfs_inode_nidata_free(const struct CACHED_GENERIC *cached)
{
        ntfs_inode_real_close(((const struct CACHED_NIDATA*)cached)->ni);
}

/*
 *		Compute a hash value for an inode entry
 */

int ntfs_inode_nidata_hash(const struct CACHED_GENERIC *item)
{
	return (((const struct CACHED_NIDATA*)item)->inum
			% (2*CACHE_NIDATA_SIZE));
}

/*
 *		inum comparing for entering/fetching from cache
 */

static int idata_cache_compare(const struct CACHED_GENERIC *cached,
			const struct CACHED_GENERIC *wanted)
{
	return (((const struct CACHED_NIDATA*)cached)->inum
			!= ((const struct CACHED_NIDATA*)wanted)->inum);
}

/*
 *		Invalidate an inode entry when not needed anymore.
 *	The entry should have been synced, it may be reused later,
 *	if it is requested before it is dropped from cache.
 */

void ntfs_inode_invalidate(ntfs_volume *vol, const MFT_REF mref)
{
	struct CACHED_NIDATA item;
	int count;

	item.inum = MREF(mref);
	item.ni = (ntfs_inode*)NULL;
	item.pathname = (const char*)NULL;
	item.varsize = 0;
	count = ntfs_invalidate_cache(vol->nidata_cache,
				GENERIC(&item),idata_cache_compare,CACHE_FREE);
}

#endif

/*
 *		Open an inode
 *
 *	When possible, an entry recorded in the cache is reused
 *
 *	**NEVER REOPEN** an inode, this can lead to a duplicated
 * 	cache entry (hard to detect), and to an obsolete one being
 *	reused. System files are however protected from being cached.
 */

ntfs_inode *ntfs_inode_open(ntfs_volume *vol, const MFT_REF mref)
{
	ntfs_inode *ni;
#if CACHE_NIDATA_SIZE
	struct CACHED_NIDATA item;
	struct CACHED_NIDATA *cached;

		/* fetch idata from cache */
	item.inum = MREF(mref);
	debug_double_inode(item.inum,1);
	item.pathname = (const char*)NULL;
	item.varsize = 0;
	cached = (struct CACHED_NIDATA*)ntfs_fetch_cache(vol->nidata_cache,
				GENERIC(&item),idata_cache_compare);
	if (cached) {
		ni = cached->ni;
		/* do not keep open entries in cache */
		ntfs_remove_cache(vol->nidata_cache,
				(struct CACHED_GENERIC*)cached,0);
	} else {
		ni = ntfs_inode_real_open(vol, mref);
	}
#else
	ni = ntfs_inode_real_open(vol, mref);
#endif
	return (ni);
}

/*
 *		Close an inode entry
 *
 *	If cacheing is in use, the entry is synced and kept available
 *	in cache for further use.
 *
 *	System files (inode < 16 or having the IS_4 flag) are protected
 *	against being cached.
 */

int ntfs_inode_close(ntfs_inode *ni)
{
	int res;
#if CACHE_NIDATA_SIZE
	BOOL dirty;
	struct CACHED_NIDATA item;

	if (ni) {
		debug_double_inode(ni->mft_no,0);
		/* do not cache system files : could lead to double entries */
		if (ni->vol && ni->vol->nidata_cache
			&& ((ni->mft_no == FILE_root)
			    || ((ni->mft_no >= FILE_first_user)
				&& !(ni->mrec->flags & MFT_RECORD_IS_4)))) {
			/* If we have dirty metadata, write it out. */
			dirty = NInoDirty(ni) || NInoAttrListDirty(ni);
			if (dirty) {
				res = ntfs_inode_sync(ni);
					/* do a real close if sync failed */
				if (res)
					ntfs_inode_real_close(ni);
			} else
				res = 0;

			if (!res) {
					/* feed idata into cache */
				item.inum = ni->mft_no;
				item.ni = ni;
				item.pathname = (const char*)NULL;
				item.varsize = 0;
				debug_cached_inode(ni);
				ntfs_enter_cache(ni->vol->nidata_cache,
					GENERIC(&item), idata_cache_compare);
			}
		} else {
			/* cache not ready or system file, really close */
			res = ntfs_inode_real_close(ni);
		}
	} else
		res = 0;
#else
	res = ntfs_inode_real_close(ni);
#endif
	return (res);
}

/**
 * ntfs_extent_inode_open - load an extent inode and attach it to its base
 * @base_ni:	base ntfs inode
 * @mref:	mft reference of the extent inode to load (in little endian)
 *
 * First check if the extent inode @mref is already attached to the base ntfs
 * inode @base_ni, and if so, return a pointer to the attached extent inode.
 *
 * If the extent inode is not already attached to the base inode, allocate an
 * ntfs_inode structure and initialize it for the given inode @mref. @mref
 * specifies the inode number / mft record to read, including the sequence
 * number, which can be 0 if no sequence number checking is to be performed.
 *
 * Then, allocate a buffer for the mft record, read the mft record from the
 * volume @base_ni->vol, and attach it to the ntfs_inode structure (->mrec).
 * The mft record is mst deprotected and sanity checked for validity and we
 * abort if deprotection or checks fail.
 *
 * Finally attach the ntfs inode to its base inode @base_ni and return a
 * pointer to the ntfs_inode structure on success or NULL on error, with errno
 * set to the error code.
 *
 * Note, extent inodes are never closed directly. They are automatically
 * disposed off by the closing of the base inode.
 */
ntfs_inode *ntfs_extent_inode_open(ntfs_inode *base_ni, const MFT_REF mref)
{
	u64 mft_no = MREF_LE(mref);
	ntfs_inode *ni = NULL;
	ntfs_inode **extent_nis;
	int i;

	if (!base_ni) {
		errno = EINVAL;
		ntfs_log_perror("%s", __FUNCTION__);
		return NULL;
	}
	
	ntfs_log_enter("Opening extent inode %lld (base mft record %lld).\n",
			(unsigned long long)mft_no,
			(unsigned long long)base_ni->mft_no);
	
	/* Is the extent inode already open and attached to the base inode? */
	if (base_ni->nr_extents > 0) {
		extent_nis = base_ni->extent_nis;
		for (i = 0; i < base_ni->nr_extents; i++) {
			u16 seq_no;

			ni = extent_nis[i];
			if (mft_no != ni->mft_no)
				continue;
			/* Verify the sequence number if given. */
			seq_no = MSEQNO_LE(mref);
			if (seq_no && seq_no != le16_to_cpu(
					ni->mrec->sequence_number)) {
				errno = EIO;
				ntfs_log_perror("Found stale extent mft "
					"reference mft=%lld",
					(long long)ni->mft_no);
				goto out;
			}
			goto out;
		}
	}
	/* Wasn't there, we need to load the extent inode. */
	ni = __ntfs_inode_allocate(base_ni->vol);
	if (!ni)
		goto out;
	if (ntfs_file_record_read(base_ni->vol, le64_to_cpu(mref), &ni->mrec, NULL))
		goto err_out;
	ni->mft_no = mft_no;
	ni->nr_extents = -1;
	ni->base_ni = base_ni;
	/* Attach extent inode to base inode, reallocating memory if needed. */
	if (!(base_ni->nr_extents & 3)) {
		i = (base_ni->nr_extents + 4) * sizeof(ntfs_inode *);

		extent_nis = ntfs_malloc(i);
		if (!extent_nis)
			goto err_out;
		if (base_ni->nr_extents) {
			memcpy(extent_nis, base_ni->extent_nis,
					i - 4 * sizeof(ntfs_inode *));
			free(base_ni->extent_nis);
		}
		base_ni->extent_nis = extent_nis;
	}
	base_ni->extent_nis[base_ni->nr_extents++] = ni;
out:
	ntfs_log_leave("\n");
	return ni;
err_out:
	__ntfs_inode_release(ni);
	ni = NULL;
	goto out;
}

/**
 * ntfs_inode_attach_all_extents - attach all extents for target inode
 * @ni:		opened ntfs inode for which perform attach
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_inode_attach_all_extents(ntfs_inode *ni)
{
	ATTR_LIST_ENTRY *ale;
	u64 prev_attached = 0;

	if (!ni) {
		ntfs_log_trace("Invalid arguments.\n");
		errno = EINVAL;
		return -1;
	}

	if (ni->nr_extents == -1)
		ni = ni->base_ni;

	ntfs_log_trace("Entering for inode 0x%llx.\n", (long long) ni->mft_no);

	/* Inode haven't got attribute list, thus nothing to attach. */
	if (!NInoAttrList(ni))
		return 0;

	if (!ni->attr_list) {
		ntfs_log_trace("Corrupt in-memory struct.\n");
		errno = EINVAL;
		return -1;
	}

	/* Walk through attribute list and attach all extents. */
	errno = 0;
	ale = (ATTR_LIST_ENTRY *)ni->attr_list;
	while ((u8*)ale < ni->attr_list + ni->attr_list_size) {
		if (ni->mft_no != MREF_LE(ale->mft_reference) &&
				prev_attached != MREF_LE(ale->mft_reference)) {
			if (!ntfs_extent_inode_open(ni, ale->mft_reference)) {
				ntfs_log_trace("Couldn't attach extent inode.\n");
				return -1;
			}
			prev_attached = MREF_LE(ale->mft_reference);
		}
		ale = (ATTR_LIST_ENTRY *)((u8*)ale + le16_to_cpu(ale->length));
	}
	return 0;
}

/**
 * ntfs_inode_sync_standard_information - update standard information attribute
 * @ni:		ntfs inode to update standard information
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
static int ntfs_inode_sync_standard_information(ntfs_inode *ni)
{
	ntfs_attr_search_ctx *ctx;
	STANDARD_INFORMATION *std_info;
	u32 lth;
	le32 lthle;

	ntfs_log_trace("Entering for inode %lld\n", (long long)ni->mft_no);

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;
	if (ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED,
			     0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		ntfs_log_perror("Failed to sync standard info (inode %lld)",
				(long long)ni->mft_no);
		ntfs_attr_put_search_ctx(ctx);
		return -1;
	}
	std_info = (STANDARD_INFORMATION *)((u8 *)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	std_info->file_attributes = ni->flags;
	if (!test_nino_flag(ni, TimesSet)) {
		std_info->creation_time = ni->creation_time;
		std_info->last_data_change_time = ni->last_data_change_time;
		std_info->last_mft_change_time = ni->last_mft_change_time;
		std_info->last_access_time = ni->last_access_time;
	}

		/* JPA update v3.x extensions, ensuring consistency */

	lthle = ctx->attr->length;
	lth = le32_to_cpu(lthle);
	if (test_nino_flag(ni, v3_Extensions)
	    && (lth <= sizeof(STANDARD_INFORMATION)))
		ntfs_log_error("bad sync of standard information\n");

	if (lth > sizeof(STANDARD_INFORMATION)) {
		std_info->owner_id = ni->owner_id;
		std_info->security_id = ni->security_id;
		std_info->quota_charged = ni->quota_charged;
		std_info->usn = ni->usn;
	}
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	return 0;
}

/**
 * ntfs_inode_sync_file_name - update FILE_NAME attributes
 * @ni:		ntfs inode to update FILE_NAME attributes
 *
 * Update all FILE_NAME attributes for inode @ni in the index.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
static int ntfs_inode_sync_file_name(ntfs_inode *ni, ntfs_inode *dir_ni)
{
	ntfs_attr_search_ctx *ctx = NULL;
	ntfs_index_context *ictx;
	ntfs_inode *index_ni;
	FILE_NAME_ATTR *fn;
	FILE_NAME_ATTR *fnx;
	REPARSE_POINT *rpp;
	le32 reparse_tag;
	int err = 0;

	ntfs_log_trace("Entering for inode %lld\n", (long long)ni->mft_no);

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx) {
		err = errno;
		goto err_out;
	}
	/* Collect the reparse tag, if any */
	reparse_tag = cpu_to_le32(0);
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
		if (!ntfs_attr_lookup(AT_REPARSE_POINT, NULL,
				0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
			rpp = (REPARSE_POINT*)((u8 *)ctx->attr +
					le16_to_cpu(ctx->attr->value_offset));
			reparse_tag = rpp->reparse_tag;
		}
		ntfs_attr_reinit_search_ctx(ctx);
	}
	/* Walk through all FILE_NAME attributes and update them. */
	while (!ntfs_attr_lookup(AT_FILE_NAME, NULL, 0, 0, 0, NULL, 0, ctx)) {
		fn = (FILE_NAME_ATTR *)((u8 *)ctx->attr +
				le16_to_cpu(ctx->attr->value_offset));
		if (MREF_LE(fn->parent_directory) == ni->mft_no) {
			/*
			 * WARNING: We cheat here and obtain 2 attribute
			 * search contexts for one inode (first we obtained
			 * above, second will be obtained inside
			 * ntfs_index_lookup), it's acceptable for library,
			 * but will deadlock in the kernel.
			 */
			index_ni = ni;
		} else
			if (dir_ni)
				index_ni = dir_ni;
			else
				index_ni = ntfs_inode_open(ni->vol, 
					le64_to_cpu(fn->parent_directory));
		if (!index_ni) {
			if (!err)
				err = errno;
			ntfs_log_perror("Failed to open inode %lld with index",
				(long long)le64_to_cpu(fn->parent_directory));
			continue;
		}
		ictx = ntfs_index_ctx_get(index_ni, NTFS_INDEX_I30, 4);
		if (!ictx) {
			if (!err)
				err = errno;
			ntfs_log_perror("Failed to get index ctx, inode %lld",
					(long long)index_ni->mft_no);
			if ((ni != index_ni) && !dir_ni
			    && ntfs_inode_close(index_ni) && !err)
				err = errno;
			continue;
		}
		if (ntfs_index_lookup(fn, sizeof(FILE_NAME_ATTR), ictx)) {
			if (!err) {
				if (errno == ENOENT)
					err = EIO;
				else
					err = errno;
			}
			ntfs_log_perror("Index lookup failed, inode %lld",
					(long long)index_ni->mft_no);
			ntfs_index_ctx_put(ictx);
			if (ni != index_ni && ntfs_inode_close(index_ni) && !err)
				err = errno;
			continue;
		}
		/* Update flags and file size. */
		fnx = (FILE_NAME_ATTR *)ictx->data;
		fnx->file_attributes =
				(fnx->file_attributes & ~FILE_ATTR_VALID_FLAGS) |
				(ni->flags & FILE_ATTR_VALID_FLAGS);
		if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
			fnx->data_size = fnx->allocated_size
				= const_cpu_to_le64(0);
		else {
			fnx->allocated_size = cpu_to_sle64(ni->allocated_size);
			fnx->data_size = cpu_to_sle64(ni->data_size);
		}
			/* update or clear the reparse tag in the index */
		fnx->reparse_point_tag = reparse_tag;
		if (!test_nino_flag(ni, TimesSet)) {
			fnx->creation_time = ni->creation_time;
			fnx->last_data_change_time = ni->last_data_change_time;
			fnx->last_mft_change_time = ni->last_mft_change_time;
			fnx->last_access_time = ni->last_access_time;
		} else {
			fnx->creation_time = fn->creation_time;
			fnx->last_data_change_time = fn->last_data_change_time;
			fnx->last_mft_change_time = fn->last_mft_change_time;
			fnx->last_access_time = fn->last_access_time;
		}
		ntfs_index_entry_mark_dirty(ictx);
		ntfs_index_ctx_put(ictx);
		if ((ni != index_ni) && !dir_ni
		    && ntfs_inode_close(index_ni) && !err)
			err = errno;
	}
	/* Check for real error occurred. */
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_perror("Attribute lookup failed, inode %lld",
				(long long)ni->mft_no);
		goto err_out;
	}
	ntfs_attr_put_search_ctx(ctx);
	if (err) {
		errno = err;
		return -1;
	}
	return 0;
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}

/**
 * ntfs_inode_sync - write the inode (and its dirty extents) to disk
 * @ni:		ntfs inode to write
 *
 * Write the inode @ni to disk as well as its dirty extent inodes if such
 * exist and @ni is a base inode. If @ni is an extent inode, only @ni is
 * written completely disregarding its base inode and any other extent inodes.
 *
 * For a base inode with dirty extent inodes if any writes fail for whatever
 * reason, the failing inode is skipped and the sync process is continued. At
 * the end the error condition that brought about the failure is returned. Thus
 * the smallest amount of data loss possible occurs.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 * The following error codes are defined:
 *	EINVAL	- Invalid arguments were passed to the function.
 *	EBUSY	- Inode and/or one of its extents is busy, try again later.
 *	EIO	- I/O error while writing the inode (or one of its extents).
 */
static int ntfs_inode_sync_in_dir(ntfs_inode *ni, ntfs_inode *dir_ni)
{
	int ret = 0;
	int err = 0;
	if (!ni) {
		errno = EINVAL;
		ntfs_log_error("Failed to sync NULL inode\n");
		return -1;
	}

	ntfs_log_enter("Entering for inode %lld\n", (long long)ni->mft_no);

	/* Update STANDARD_INFORMATION. */
	if ((ni->mrec->flags & MFT_RECORD_IN_USE) && ni->nr_extents != -1 &&
			ntfs_inode_sync_standard_information(ni)) {
		if (!err || errno == EIO) {
			err = errno;
			if (err != EIO)
				err = EBUSY;
		}
	}

	/* Update FILE_NAME's in the index. */
	if ((ni->mrec->flags & MFT_RECORD_IN_USE) && ni->nr_extents != -1 &&
			NInoFileNameTestAndClearDirty(ni) &&
			ntfs_inode_sync_file_name(ni, dir_ni)) {
		if (!err || errno == EIO) {
			err = errno;
			if (err != EIO)
				err = EBUSY;
		}
		ntfs_log_perror("Failed to sync FILE_NAME (inode %lld)",
				(long long)ni->mft_no);
		NInoFileNameSetDirty(ni);
	}

	/* Write out attribute list from cache to disk. */
	if ((ni->mrec->flags & MFT_RECORD_IN_USE) && ni->nr_extents != -1 &&
			NInoAttrList(ni) && NInoAttrListTestAndClearDirty(ni)) {
		ntfs_attr *na;

		na = ntfs_attr_open(ni, AT_ATTRIBUTE_LIST, AT_UNNAMED, 0);
		if (!na) {
			if (!err || errno == EIO) {
				err = errno;
				if (err != EIO)
					err = EBUSY;
				ntfs_log_perror("Attribute list sync failed "
						"(open, inode %lld)",
						(long long)ni->mft_no);
			}
			NInoAttrListSetDirty(ni);
			goto sync_inode;
		} 
		
		if (na->data_size == ni->attr_list_size) {
			if (ntfs_attr_pwrite(na, 0, ni->attr_list_size,
				        ni->attr_list) != ni->attr_list_size) {
				if (!err || errno == EIO) {
					err = errno;
					if (err != EIO)
						err = EBUSY;
					ntfs_log_perror("Attribute list sync "
						"failed (write, inode %lld)",
						(long long)ni->mft_no);
				}
				NInoAttrListSetDirty(ni);
			}
		} else {
			err = EIO;
			ntfs_log_error("Attribute list sync failed (bad size, "
				       "inode %lld)\n", (long long)ni->mft_no);
			NInoAttrListSetDirty(ni);
		}
		ntfs_attr_close(na);
	}
	
sync_inode:
	/* Write this inode out to the $MFT (and $MFTMirr if applicable). */
	if (NInoTestAndClearDirty(ni)) {
		if (ntfs_mft_record_write(ni->vol, ni->mft_no, ni->mrec)) {
			if (!err || errno == EIO) {
				err = errno;
				if (err != EIO)
					err = EBUSY;
			}
			NInoSetDirty(ni);
			ntfs_log_perror("MFT record sync failed, inode %lld",
					(long long)ni->mft_no);
		}
	}

	/* If this is a base inode with extents write all dirty extents, too. */
	if (ni->nr_extents > 0) {
		s32 i;

		for (i = 0; i < ni->nr_extents; ++i) {
			ntfs_inode *eni;

			eni = ni->extent_nis[i];
			if (!NInoTestAndClearDirty(eni))
				continue;
			
			if (ntfs_mft_record_write(eni->vol, eni->mft_no, 
						  eni->mrec)) {
				if (!err || errno == EIO) {
					err = errno;
					if (err != EIO)
						err = EBUSY;
				}
				NInoSetDirty(eni);
				ntfs_log_perror("Extent MFT record sync failed,"
						" inode %lld/%lld",
						(long long)ni->mft_no,
						(long long)eni->mft_no);
			}
		}
	}

	if (err) {
		errno = err;
		ret = -1;
	}
	
	ntfs_log_leave("\n");
	return ret;
}

int ntfs_inode_sync(ntfs_inode *ni)
{
	return (ntfs_inode_sync_in_dir(ni, (ntfs_inode*)NULL));
}

/*
 *		Close an inode with an open parent inode
 */

int ntfs_inode_close_in_dir(ntfs_inode *ni, ntfs_inode *dir_ni)
{
	int res;

	res = ntfs_inode_sync_in_dir(ni, dir_ni);
	if (res) {
		if (errno != EIO)
			errno = EBUSY;
	} else
		res = ntfs_inode_close(ni);
	return (res);
}

/**
 * ntfs_inode_add_attrlist - add attribute list to inode and fill it
 * @ni: opened ntfs inode to which add attribute list
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 * The following error codes are defined:
 *	EINVAL	- Invalid arguments were passed to the function.
 *	EEXIST	- Attribute list already exist.
 *	EIO	- Input/Ouput error occurred.
 *	ENOMEM	- Not enough memory to perform add.
 */
int ntfs_inode_add_attrlist(ntfs_inode *ni)
{
	int err;
	ntfs_attr_search_ctx *ctx;
	u8 *al = NULL, *aln;
	int al_len = 0;
	ATTR_LIST_ENTRY *ale = NULL;
	ntfs_attr *na;

	if (!ni) {
		errno = EINVAL;
		ntfs_log_perror("%s", __FUNCTION__);
		return -1;
	}

	ntfs_log_trace("inode %llu\n", (unsigned long long) ni->mft_no);

	if (NInoAttrList(ni) || ni->nr_extents) {
		errno = EEXIST;
		ntfs_log_perror("Inode already has attribute list");
		return -1;
	}

	/* Form attribute list. */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx) {
		err = errno;
		goto err_out;
	}
	/* Walk through all attributes. */
	while (!ntfs_attr_lookup(AT_UNUSED, NULL, 0, 0, 0, NULL, 0, ctx)) {
		
		int ale_size;
		
		if (ctx->attr->type == AT_ATTRIBUTE_LIST) {
			err = EIO;
			ntfs_log_perror("Attribute list already present");
			goto put_err_out;
		}
		
		ale_size = (sizeof(ATTR_LIST_ENTRY) + sizeof(ntfschar) *
					ctx->attr->name_length + 7) & ~7;
		al_len += ale_size;
		
		aln = realloc(al, al_len);
		if (!aln) {
			err = errno;
			ntfs_log_perror("Failed to realloc %d bytes", al_len);
			goto put_err_out;
		}
		ale = (ATTR_LIST_ENTRY *)(aln + ((u8 *)ale - al));
		al = aln;
		
		memset(ale, 0, ale_size);
		
		/* Add attribute to attribute list. */
		ale->type = ctx->attr->type;
		ale->length = cpu_to_le16((sizeof(ATTR_LIST_ENTRY) +
			sizeof(ntfschar) * ctx->attr->name_length + 7) & ~7);
		ale->name_length = ctx->attr->name_length;
		ale->name_offset = (u8 *)ale->name - (u8 *)ale;
		if (ctx->attr->non_resident)
			ale->lowest_vcn = ctx->attr->lowest_vcn;
		else
			ale->lowest_vcn = 0;
		ale->mft_reference = MK_LE_MREF(ni->mft_no,
			le16_to_cpu(ni->mrec->sequence_number));
		ale->instance = ctx->attr->instance;
		memcpy(ale->name, (u8 *)ctx->attr +
				le16_to_cpu(ctx->attr->name_offset),
				ctx->attr->name_length * sizeof(ntfschar));
		ale = (ATTR_LIST_ENTRY *)(al + al_len);
	}
	/* Check for real error occurred. */
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_perror("%s: Attribute lookup failed, inode %lld",
				__FUNCTION__, (long long)ni->mft_no);
		goto put_err_out;
	}

	/* Set in-memory attribute list. */
	ni->attr_list = al;
	ni->attr_list_size = al_len;
	NInoSetAttrList(ni);
	NInoAttrListSetDirty(ni);

	/* Free space if there is not enough it for $ATTRIBUTE_LIST. */
	if (le32_to_cpu(ni->mrec->bytes_allocated) -
			le32_to_cpu(ni->mrec->bytes_in_use) <
			offsetof(ATTR_RECORD, resident_end)) {
		if (ntfs_inode_free_space(ni,
				offsetof(ATTR_RECORD, resident_end))) {
			/* Failed to free space. */
			err = errno;
			ntfs_log_perror("Failed to free space for attrlist");
			goto rollback;
		}
	}

	/* Add $ATTRIBUTE_LIST to mft record. */
	if (ntfs_resident_attr_record_add(ni,
				AT_ATTRIBUTE_LIST, NULL, 0, NULL, 0, 0) < 0) {
		err = errno;
		ntfs_log_perror("Couldn't add $ATTRIBUTE_LIST to MFT");
		goto rollback;
	}

	/* Resize it. */
	na = ntfs_attr_open(ni, AT_ATTRIBUTE_LIST, AT_UNNAMED, 0);
	if (!na) {
		err = errno;
		ntfs_log_perror("Failed to open just added $ATTRIBUTE_LIST");
		goto remove_attrlist_record;
	}
	if (ntfs_attr_truncate(na, al_len)) {
		err = errno;
		ntfs_log_perror("Failed to resize just added $ATTRIBUTE_LIST");
		ntfs_attr_close(na);
		goto remove_attrlist_record;;
	}
	
	ntfs_attr_put_search_ctx(ctx);
	ntfs_attr_close(na);
	return 0;

remove_attrlist_record:
	/* Prevent ntfs_attr_recorm_rm from freeing attribute list. */
	ni->attr_list = NULL;
	NInoClearAttrList(ni);
	/* Remove $ATTRIBUTE_LIST record. */
	ntfs_attr_reinit_search_ctx(ctx);
	if (!ntfs_attr_lookup(AT_ATTRIBUTE_LIST, NULL, 0,
				CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		if (ntfs_attr_record_rm(ctx))
			ntfs_log_perror("Rollback failed to remove attrlist");
	} else
		ntfs_log_perror("Rollback failed to find attrlist");
	/* Setup back in-memory runlist. */
	ni->attr_list = al;
	ni->attr_list_size = al_len;
	NInoSetAttrList(ni);
rollback:
	/*
	 * Scan attribute list for attributes that placed not in the base MFT
	 * record and move them to it.
	 */
	ntfs_attr_reinit_search_ctx(ctx);
	ale = (ATTR_LIST_ENTRY*)al;
	while ((u8*)ale < al + al_len) {
		if (MREF_LE(ale->mft_reference) != ni->mft_no) {
			if (!ntfs_attr_lookup(ale->type, ale->name,
						ale->name_length,
						CASE_SENSITIVE,
						sle64_to_cpu(ale->lowest_vcn),
						NULL, 0, ctx)) {
				if (ntfs_attr_record_move_to(ctx, ni))
					ntfs_log_perror("Rollback failed to "
							"move attribute");
			} else
				ntfs_log_perror("Rollback failed to find attr");
			ntfs_attr_reinit_search_ctx(ctx);
		}
		ale = (ATTR_LIST_ENTRY*)((u8*)ale + le16_to_cpu(ale->length));
	}
	/* Remove in-memory attribute list. */
	ni->attr_list = NULL;
	ni->attr_list_size = 0;
	NInoClearAttrList(ni);
	NInoAttrListClearDirty(ni);
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
err_out:
	free(al);
	errno = err;
	return -1;
}

/**
 * ntfs_inode_free_space - free space in the MFT record of an inode
 * @ni:		ntfs inode in which MFT record needs more free space
 * @size:	amount of space needed to free
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
int ntfs_inode_free_space(ntfs_inode *ni, int size)
{
	ntfs_attr_search_ctx *ctx;
	int freed;

	if (!ni || size < 0) {
		errno = EINVAL;
		ntfs_log_perror("%s: ni=%p size=%d", __FUNCTION__, ni, size);
		return -1;
	}

	ntfs_log_trace("Entering for inode %lld, size %d\n",
		       (unsigned long long)ni->mft_no, size);

	freed = (le32_to_cpu(ni->mrec->bytes_allocated) -
				le32_to_cpu(ni->mrec->bytes_in_use));

	if (size <= freed)
		return 0;

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;
	/*
	 * $STANDARD_INFORMATION and $ATTRIBUTE_LIST must stay in the base MFT
	 * record, so position search context on the first attribute after them.
	 */
	if (ntfs_attr_position(AT_FILE_NAME, ctx))
		goto put_err_out;

	while (1) {
		int record_size;
		/*
		 * Check whether attribute is from different MFT record. If so,
		 * find next, because we don't need such.
		 */
		while (ctx->ntfs_ino->mft_no != ni->mft_no) {
retry:			
			if (ntfs_attr_position(AT_UNUSED, ctx))
				goto put_err_out;
		}

		if (ntfs_inode_base(ctx->ntfs_ino)->mft_no == FILE_MFT && 
		    ctx->attr->type == AT_DATA)
			goto retry;

		if (ctx->attr->type == AT_INDEX_ROOT)
			goto retry;

		record_size = le32_to_cpu(ctx->attr->length);

		if (ntfs_attr_record_move_away(ctx, 0)) {
			ntfs_log_perror("Failed to move out attribute #2");
			break;
		}
		freed += record_size;

		/* Check whether we are done. */
		if (size <= freed) {
			ntfs_attr_put_search_ctx(ctx);
			return 0;
		}
		/*
		 * Reposition to first attribute after $STANDARD_INFORMATION 
		 * and $ATTRIBUTE_LIST instead of simply skipping this attribute 
		 * because in the case when we have got only in-memory attribute 
		 * list then ntfs_attr_lookup will fail when it tries to find 
		 * $ATTRIBUTE_LIST.
		 */
		ntfs_attr_reinit_search_ctx(ctx);
		if (ntfs_attr_position(AT_FILE_NAME, ctx))
			break;
	}
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	if (errno == ENOSPC)
		ntfs_log_trace("No attributes left that could be moved out.\n");
	return -1;
}

/**
 * ntfs_inode_update_times - update selected time fields for ntfs inode
 * @ni:		ntfs inode for which update time fields
 * @mask:	select which time fields should be updated
 *
 * This function updates time fields to current time. Fields to update are
 * selected using @mask (see enum @ntfs_time_update_flags for posssible values).
 */
void ntfs_inode_update_times(ntfs_inode *ni, ntfs_time_update_flags mask)
{
	ntfs_time now;

	if (!ni) {
		ntfs_log_error("%s(): Invalid arguments.\n", __FUNCTION__);
		return;
	}

	if ((ni->mft_no < FILE_first_user && ni->mft_no != FILE_root) ||
			NVolReadOnly(ni->vol) || !mask)
		return;

	now = ntfs_current_time();
	if (mask & NTFS_UPDATE_ATIME)
		ni->last_access_time = now;
	if (mask & NTFS_UPDATE_MTIME)
		ni->last_data_change_time = now;
	if (mask & NTFS_UPDATE_CTIME)
		ni->last_mft_change_time = now;
	
	NInoFileNameSetDirty(ni);
	NInoSetDirty(ni);
}

/**
 * ntfs_inode_badclus_bad - check for $Badclus:$Bad data attribute
 * @mft_no:		mft record number where @attr is present
 * @attr:		attribute record used to check for the $Bad attribute
 *
 * Check if the mft record given by @mft_no and @attr contains the bad sector
 * list. Please note that mft record numbers describing $Badclus extent inodes
 * will not match the current $Badclus:$Bad check.
 * 
 * On success return 1 if the file is $Badclus:$Bad, otherwise return 0.
 * On error return -1 with errno set to the error code.
 */
int ntfs_inode_badclus_bad(u64 mft_no, ATTR_RECORD *attr)
{
	int len, ret = 0;
	ntfschar *ustr;

	if (!attr) {
		ntfs_log_error("Invalid argument.\n");
		errno = EINVAL;
		return -1;
	}
	
	if (mft_no != FILE_BadClus)
	       	return 0;

	if (attr->type != AT_DATA)
	       	return 0;

	if ((ustr = ntfs_str2ucs("$Bad", &len)) == NULL) {
		ntfs_log_perror("Couldn't convert '$Bad' to Unicode");
		return -1;
	}

	if (ustr && ntfs_names_are_equal(ustr, len,
			(ntfschar *)((u8 *)attr + le16_to_cpu(attr->name_offset)),
			attr->name_length, 0, NULL, 0))
		ret = 1;

	ntfs_ucsfree(ustr);

	return ret;
}

#ifdef HAVE_SETXATTR	/* extended attributes interface required */

/*
 *		Get high precision NTFS times
 *
 *	They are returned in following order : create, update, access, change
 *	provided they fit in requested size.
 *
 *	Returns the modified size if successfull (or 32 if buffer size is null)
 *		-errno if failed
 */

int ntfs_inode_get_times(ntfs_inode *ni, char *value, size_t size)
{
	ntfs_attr_search_ctx *ctx;
	STANDARD_INFORMATION *std_info;
	u64 *times;
	int ret;

	ret = 0;
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (ctx) {
		if (ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED,
				     0, CASE_SENSITIVE, 0, NULL, 0, ctx)) {
			ntfs_log_perror("Failed to get standard info (inode %lld)",
					(long long)ni->mft_no);
		} else {
			std_info = (STANDARD_INFORMATION *)((u8 *)ctx->attr +
					le16_to_cpu(ctx->attr->value_offset));
			if (value && (size >= 8)) {
				times = (u64*)value;
				times[0] = le64_to_cpu(std_info->creation_time);
				ret = 8;
				if (size >= 16) {
					times[1] = le64_to_cpu(std_info->last_data_change_time);
					ret = 16;
				}
				if (size >= 24) {
					times[2] = le64_to_cpu(std_info->last_access_time);
					ret = 24;
				}
				if (size >= 32) {
					times[3] = le64_to_cpu(std_info->last_mft_change_time);
					ret = 32;
				}
			} else
				if (!size)
					ret = 32;
				else
					ret = -ERANGE;
			}
		ntfs_attr_put_search_ctx(ctx);
	}		
	return (ret ? ret : -errno);
}

/*
 *		Set high precision NTFS times
 *
 *	They are expected in this order : create, update, access
 *	provided they are present in input. The change time is set to
 *	current time.
 *
 *	The times are inserted directly in the standard_information and
 *	file names attributes to avoid manipulating low precision times
 *
 *	Returns 0 if success
 *		-1 if there were an error (described by errno)
 */

int ntfs_inode_set_times(ntfs_inode *ni, const char *value, size_t size,
			int flags)
{
	ntfs_attr_search_ctx *ctx;
	STANDARD_INFORMATION *std_info;
	FILE_NAME_ATTR *fn;
	const u64 *times;
	ntfs_time now;
	int cnt;
	int ret;

	ret = -1;
	if ((size >= 8) && !(flags & XATTR_CREATE)) {
		times = (const u64*)value;
		now = ntfs_current_time();
			/* update the standard information attribute */
		ctx = ntfs_attr_get_search_ctx(ni, NULL);
		if (ctx) {
			if (ntfs_attr_lookup(AT_STANDARD_INFORMATION,
					AT_UNNAMED, 0, CASE_SENSITIVE,
					0, NULL, 0, ctx)) {
				ntfs_log_perror("Failed to get standard info (inode %lld)",
						(long long)ni->mft_no);
			} else {
				std_info = (STANDARD_INFORMATION *)((u8 *)ctx->attr +
					le16_to_cpu(ctx->attr->value_offset));
				/*
				 * Mark times set to avoid overwriting
				 * them when the inode is closed.
				 * The inode structure must also be updated
				 * (with loss of precision) because of cacheing.
				 * TODO : use NTFS precision in inode, and
				 * return sub-second times in getattr()
				 */
				set_nino_flag(ni, TimesSet);
				std_info->creation_time = cpu_to_le64(times[0]);
				ni->creation_time
					= std_info->creation_time;
				if (size >= 16) {
					std_info->last_data_change_time = cpu_to_le64(times[1]);
					ni->last_data_change_time
						= std_info->last_data_change_time;
				}
				if (size >= 24) {
					std_info->last_access_time = cpu_to_le64(times[2]);
					ni->last_access_time
						= std_info->last_access_time;
				}
				std_info->last_mft_change_time = now;
				ni->last_mft_change_time = now;
				ntfs_inode_mark_dirty(ctx->ntfs_ino);
				NInoFileNameSetDirty(ni);

				/* update the file names attributes */
				ntfs_attr_reinit_search_ctx(ctx);
				cnt = 0;
				while (!ntfs_attr_lookup(AT_FILE_NAME,
						AT_UNNAMED, 0, CASE_SENSITIVE,
						0, NULL, 0, ctx)) {
					fn = (FILE_NAME_ATTR*)((u8 *)ctx->attr +
						le16_to_cpu(ctx->attr->value_offset));
					fn->creation_time
						= cpu_to_le64(times[0]);
					if (size >= 16)
						fn->last_data_change_time
							= cpu_to_le64(times[1]);
					if (size >= 24)
						fn->last_access_time
							= cpu_to_le64(times[2]);
					fn->last_mft_change_time = now;
					cnt++;
				}
				if (cnt)
					ret = 0;
				else {
					ntfs_log_perror("Failed to get file names (inode %lld)",
						(long long)ni->mft_no);
				}
			}
			ntfs_attr_put_search_ctx(ctx);
		}
	} else
		if (size < 8)
			errno = ERANGE;
		else
			errno = EEXIST;
	return (ret);
}

#endif /* HAVE_SETXATTR */
