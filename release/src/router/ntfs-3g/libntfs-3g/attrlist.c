/**
 * attrlist.c - Attribute list attribute handling code.  Originated from the Linux-NTFS
 *		project.
 *
 * Copyright (c) 2004-2005 Anton Altaparmakov
 * Copyright (c) 2004-2005 Yura Pakhuchiy
 * Copyright (c)      2006 Szabolcs Szakacsits
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "layout.h"
#include "attrib.h"
#include "attrlist.h"
#include "debug.h"
#include "unistr.h"
#include "logging.h"
#include "misc.h"

/**
 * ntfs_attrlist_need - check whether inode need attribute list
 * @ni:		opened ntfs inode for which perform check
 *
 * Check whether all are attributes belong to one MFT record, in that case
 * attribute list is not needed.
 *
 * Return 1 if inode need attribute list, 0 if not, -1 on error with errno set
 * to the error code. If function succeed errno set to 0. The following error
 * codes are defined:
 *	EINVAL	- Invalid arguments passed to function or attribute haven't got
 *		  attribute list.
 */
int ntfs_attrlist_need(ntfs_inode *ni)
{
	ATTR_LIST_ENTRY *ale;

	if (!ni) {
		ntfs_log_trace("Invalid arguments.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx.\n", (long long) ni->mft_no);

	if (!NInoAttrList(ni)) {
		ntfs_log_trace("Inode haven't got attribute list.\n");
		errno = EINVAL;
		return -1;
	}

	if (!ni->attr_list) {
		ntfs_log_trace("Corrupt in-memory struct.\n");
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	ale = (ATTR_LIST_ENTRY *)ni->attr_list;
	while ((u8*)ale < ni->attr_list + ni->attr_list_size) {
		if (MREF_LE(ale->mft_reference) != ni->mft_no)
			return 1;
		ale = (ATTR_LIST_ENTRY *)((u8*)ale + le16_to_cpu(ale->length));
	}
	return 0;
}

/**
 * ntfs_attrlist_entry_add - add an attribute list attribute entry
 * @ni:		opened ntfs inode, which contains that attribute
 * @attr:	attribute record to add to attribute list
 *
 * Return 0 on success and -1 on error with errno set to the error code. The
 * following error codes are defined:
 *	EINVAL	- Invalid arguments passed to function.
 *	ENOMEM	- Not enough memory to allocate necessary buffers.
 *	EIO	- I/O error occurred or damaged filesystem.
 *	EEXIST	- Such attribute already present in attribute list.
 */
int ntfs_attrlist_entry_add(ntfs_inode *ni, ATTR_RECORD *attr)
{
	ATTR_LIST_ENTRY *ale;
	MFT_REF mref;
	ntfs_attr *na = NULL;
	ntfs_attr_search_ctx *ctx;
	u8 *new_al;
	int entry_len, entry_offset, err;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n",
			(long long) ni->mft_no,
			(unsigned) le32_to_cpu(attr->type));

	if (!ni || !attr) {
		ntfs_log_trace("Invalid arguments.\n");
		errno = EINVAL;
		return -1;
	}

	mref = MK_LE_MREF(ni->mft_no, le16_to_cpu(ni->mrec->sequence_number));

	if (ni->nr_extents == -1)
		ni = ni->base_ni;

	if (!NInoAttrList(ni)) {
		ntfs_log_trace("Attribute list isn't present.\n");
		errno = ENOENT;
		return -1;
	}

	/* Determine size and allocate memory for new attribute list. */
	entry_len = (sizeof(ATTR_LIST_ENTRY) + sizeof(ntfschar) *
			attr->name_length + 7) & ~7;
	new_al = ntfs_calloc(ni->attr_list_size + entry_len);
	if (!new_al)
		return -1;

	/* Find place for the new entry. */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx) {
		err = errno;
		goto err_out;
	}
	if (!ntfs_attr_lookup(attr->type, (attr->name_length) ? (ntfschar*)
			((u8*)attr + le16_to_cpu(attr->name_offset)) :
			AT_UNNAMED, attr->name_length, CASE_SENSITIVE,
			(attr->non_resident) ? le64_to_cpu(attr->lowest_vcn) :
			0, (attr->non_resident) ? NULL : ((u8*)attr +
			le16_to_cpu(attr->value_offset)), (attr->non_resident) ?
			0 : le32_to_cpu(attr->value_length), ctx)) {
		/* Found some extent, check it to be before new extent. */
		if (ctx->al_entry->lowest_vcn == attr->lowest_vcn) {
			err = EEXIST;
			ntfs_log_trace("Such attribute already present in the "
					"attribute list.\n");
			ntfs_attr_put_search_ctx(ctx);
			goto err_out;
		}
		/* Add new entry after this extent. */
		ale = (ATTR_LIST_ENTRY*)((u8*)ctx->al_entry +
				le16_to_cpu(ctx->al_entry->length));
	} else {
		/* Check for real errors. */
		if (errno != ENOENT) {
			err = errno;
			ntfs_log_trace("Attribute lookup failed.\n");
			ntfs_attr_put_search_ctx(ctx);
			goto err_out;
		}
		/* No previous extents found. */
		ale = ctx->al_entry;
	}
	/* Don't need it anymore, @ctx->al_entry points to @ni->attr_list. */
	ntfs_attr_put_search_ctx(ctx);

	/* Determine new entry offset. */
	entry_offset = ((u8 *)ale - ni->attr_list);
	/* Set pointer to new entry. */
	ale = (ATTR_LIST_ENTRY *)(new_al + entry_offset);
	/* Zero it to fix valgrind warning. */
	memset(ale, 0, entry_len);
	/* Form new entry. */
	ale->type = attr->type;
	ale->length = cpu_to_le16(entry_len);
	ale->name_length = attr->name_length;
	ale->name_offset = offsetof(ATTR_LIST_ENTRY, name);
	if (attr->non_resident)
		ale->lowest_vcn = attr->lowest_vcn;
	else
		ale->lowest_vcn = 0;
	ale->mft_reference = mref;
	ale->instance = attr->instance;
	memcpy(ale->name, (u8 *)attr + le16_to_cpu(attr->name_offset),
			attr->name_length * sizeof(ntfschar));

	/* Resize $ATTRIBUTE_LIST to new length. */
	na = ntfs_attr_open(ni, AT_ATTRIBUTE_LIST, AT_UNNAMED, 0);
	if (!na) {
		err = errno;
		ntfs_log_trace("Failed to open $ATTRIBUTE_LIST attribute.\n");
		goto err_out;
	}
	if (ntfs_attr_truncate(na, ni->attr_list_size + entry_len)) {
		err = errno;
		ntfs_log_trace("$ATTRIBUTE_LIST resize failed.\n");
		goto err_out;
	}

	/* Copy entries from old attribute list to new. */
	memcpy(new_al, ni->attr_list, entry_offset);
	memcpy(new_al + entry_offset + entry_len, ni->attr_list +
			entry_offset, ni->attr_list_size - entry_offset);

	/* Set new runlist. */
	free(ni->attr_list);
	ni->attr_list = new_al;
	ni->attr_list_size = ni->attr_list_size + entry_len;
	NInoAttrListSetDirty(ni);
	/* Done! */
	ntfs_attr_close(na);
	return 0;
err_out:
	if (na)
		ntfs_attr_close(na);
	free(new_al);
	errno = err;
	return -1;
}

/**
 * ntfs_attrlist_entry_rm - remove an attribute list attribute entry
 * @ctx:	attribute search context describing the attribute list entry
 *
 * Remove the attribute list entry @ctx->al_entry from the attribute list.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attrlist_entry_rm(ntfs_attr_search_ctx *ctx)
{
	u8 *new_al;
	int new_al_len;
	ntfs_inode *base_ni;
	ntfs_attr *na;
	ATTR_LIST_ENTRY *ale;
	int err;

	if (!ctx || !ctx->ntfs_ino || !ctx->al_entry) {
		ntfs_log_trace("Invalid arguments.\n");
		errno = EINVAL;
		return -1;
	}

	if (ctx->base_ntfs_ino)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;
	ale = ctx->al_entry;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, lowest_vcn %lld.\n",
			(long long) ctx->ntfs_ino->mft_no,
			(unsigned) le32_to_cpu(ctx->al_entry->type),
			(long long) le64_to_cpu(ctx->al_entry->lowest_vcn));

	if (!NInoAttrList(base_ni)) {
		ntfs_log_trace("Attribute list isn't present.\n");
		errno = ENOENT;
		return -1;
	}

	/* Allocate memory for new attribute list. */
	new_al_len = base_ni->attr_list_size - le16_to_cpu(ale->length);
	new_al = ntfs_calloc(new_al_len);
	if (!new_al)
		return -1;

	/* Reisze $ATTRIBUTE_LIST to new length. */
	na = ntfs_attr_open(base_ni, AT_ATTRIBUTE_LIST, AT_UNNAMED, 0);
	if (!na) {
		err = errno;
		ntfs_log_trace("Failed to open $ATTRIBUTE_LIST attribute.\n");
		goto err_out;
	}
	if (ntfs_attr_truncate(na, new_al_len)) {
		err = errno;
		ntfs_log_trace("$ATTRIBUTE_LIST resize failed.\n");
		goto err_out;
	}

	/* Copy entries from old attribute list to new. */
	memcpy(new_al, base_ni->attr_list, (u8*)ale - base_ni->attr_list);
	memcpy(new_al + ((u8*)ale - base_ni->attr_list), (u8*)ale + le16_to_cpu(
		ale->length), new_al_len - ((u8*)ale - base_ni->attr_list));

	/* Set new runlist. */
	free(base_ni->attr_list);
	base_ni->attr_list = new_al;
	base_ni->attr_list_size = new_al_len;
	NInoAttrListSetDirty(base_ni);
	/* Done! */
	ntfs_attr_close(na);
	return 0;
err_out:
	if (na)
		ntfs_attr_close(na);
	free(new_al);
	errno = err;
	return -1;
}
