/**
 * attrib.c - Attribute handling code. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2002-2006 Szabolcs Szakacsits
 * Copyright (c) 2004-2007 Yura Pakhuchiy
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
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

#include "compat.h"
#include "attrib.h"
#include "attrlist.h"
#include "device.h"
#include "mft.h"
#include "debug.h"
#include "mst.h"
#include "volume.h"
#include "types.h"
#include "layout.h"
#include "inode.h"
#include "runlist.h"
#include "lcnalloc.h"
#include "dir.h"
#include "compress.h"
#include "bitmap.h"
#include "logging.h"
#include "support.h"
#include "crypto.h"

ntfschar AT_UNNAMED[] = { const_cpu_to_le16('\0') };

/**
 * ntfs_get_attribute_value_length - Find the length of an attribute
 * @a:
 *
 * Description...
 *
 * Returns:
 */
s64 ntfs_get_attribute_value_length(const ATTR_RECORD *a)
{
	if (!a) {
		errno = EINVAL;
		return 0;
	}
	errno = 0;
	if (a->non_resident)
		return sle64_to_cpu(a->data_size);
	return (s64)le32_to_cpu(a->value_length);
}

/**
 * ntfs_get_attribute_value - Get a copy of an attribute
 * @vol:	
 * @a:	
 * @b:	
 *
 * Description...
 *
 * Returns:
 */
s64 ntfs_get_attribute_value(const ntfs_volume *vol,
		const ATTR_RECORD *a, u8 *b)
{
	runlist *rl;
	s64 total, r;
	int i;

	/* Sanity checks. */
	if (!vol || !a || !b) {
		errno = EINVAL;
		return 0;
	}
	/* Complex attribute? */
	/*
	 * Ignore the flags in case they are not zero for an attribute list
	 * attribute.  Windows does not complain about invalid flags and chkdsk
	 * does not detect or fix them so we need to cope with it, too.
	 */
	if (a->type != AT_ATTRIBUTE_LIST && a->flags) {
		ntfs_log_error("Non-zero (%04x) attribute flags. Cannot handle "
			       "this yet.\n", le16_to_cpu(a->flags));
		errno = EOPNOTSUPP;
		return 0;
	}
	if (!a->non_resident) {
		/* Attribute is resident. */

		/* Sanity check. */
		if (le32_to_cpu(a->value_length) + le16_to_cpu(a->value_offset)
				> le32_to_cpu(a->length)) {
			return 0;
		}

		memcpy(b, (const char*)a + le16_to_cpu(a->value_offset),
				le32_to_cpu(a->value_length));
		errno = 0;
		return (s64)le32_to_cpu(a->value_length);
	}

	/* Attribute is not resident. */

	/* If no data, return 0. */
	if (!(a->data_size)) {
		errno = 0;
		return 0;
	}
	/*
	 * FIXME: What about attribute lists?!? (AIA)
	 */
	/* Decompress the mapping pairs array into a runlist. */
	rl = ntfs_mapping_pairs_decompress(vol, a, NULL);
	if (!rl) {
		errno = EINVAL;
		return 0;
	}
	/*
	 * FIXED: We were overflowing here in a nasty fashion when we
	 * reach the last cluster in the runlist as the buffer will
	 * only be big enough to hold data_size bytes while we are
	 * reading in allocated_size bytes which is usually larger
	 * than data_size, since the actual data is unlikely to have a
	 * size equal to a multiple of the cluster size!
	 * FIXED2:  We were also overflowing here in the same fashion
	 * when the data_size was more than one run smaller than the
	 * allocated size which happens with Windows XP sometimes.
	 */
	/* Now load all clusters in the runlist into b. */
	for (i = 0, total = 0; rl[i].length; i++) {
		if (total + (rl[i].length << vol->cluster_size_bits) >=
				sle64_to_cpu(a->data_size)) {
			unsigned char *intbuf = NULL;
			/*
			 * We have reached the last run so we were going to
			 * overflow when executing the ntfs_pread() which is
			 * BAAAAAAAD!
			 * Temporary fix:
			 *	Allocate a new buffer with size:
			 *	rl[i].length << vol->cluster_size_bits, do the
			 *	read into our buffer, then memcpy the correct
			 *	amount of data into the caller supplied buffer,
			 *	free our buffer, and continue.
			 * We have reached the end of data size so we were
			 * going to overflow in the same fashion.
			 * Temporary fix:  same as above.
			 */
			intbuf = ntfs_malloc(rl[i].length <<
					vol->cluster_size_bits);
			if (!intbuf) {
				int eo = errno;
				free(rl);
				errno = eo;
				return 0;
			}
			/*
			 * FIXME: If compressed file: Only read if lcn != -1.
			 * Otherwise, we are dealing with a sparse run and we
			 * just memset the user buffer to 0 for the length of
			 * the run, which should be 16 (= compression unit
			 * size).
			 * FIXME: Really only when file is compressed, or can
			 * we have sparse runs in uncompressed files as well?
			 * - Yes we can, in sparse files! But not necessarily
			 * size of 16, just run length.
			 */
			r = ntfs_pread(vol->dev, rl[i].lcn <<
					vol->cluster_size_bits, rl[i].length <<
					vol->cluster_size_bits, intbuf);
			if (r != rl[i].length << vol->cluster_size_bits) {
#define ESTR "Error reading attribute value"
				if (r == -1) {
					int eo = errno;
					ntfs_log_perror(ESTR);
					errno = eo;
				} else if (r < rl[i].length <<
						vol->cluster_size_bits) {
					ntfs_log_debug(ESTR": Ran out of "
							"input data.\n");
					errno = EIO;
				} else {
					ntfs_log_debug(ESTR": unknown error\n");
					errno = EIO;
				}
#undef ESTR
				free(rl);
				free(intbuf);
				return 0;
			}
			memcpy(b + total, intbuf, sle64_to_cpu(a->data_size) -
					total);
			free(intbuf);
			total = sle64_to_cpu(a->data_size);
			break;
		}
		/*
		 * FIXME: If compressed file: Only read if lcn != -1.
		 * Otherwise, we are dealing with a sparse run and we just
		 * memset the user buffer to 0 for the length of the run, which
		 * should be 16 (= compression unit size).
		 * FIXME: Really only when file is compressed, or can
		 * we have sparse runs in uncompressed files as well?
		 * - Yes we can, in sparse files! But not necessarily size of
		 * 16, just run length.
		 */
		r = ntfs_pread(vol->dev, rl[i].lcn << vol->cluster_size_bits,
				rl[i].length << vol->cluster_size_bits,
				b + total);
		if (r != rl[i].length << vol->cluster_size_bits) {
#define ESTR "Error reading attribute value"
			if (r == -1) {
				int eo = errno;
				ntfs_log_perror(ESTR);
				errno = eo;
			} else if (r < rl[i].length << vol->cluster_size_bits) {
				ntfs_log_debug(ESTR ": Ran out of "
						"input data.\n");
				errno = EIO;
			} else {
				ntfs_log_debug(ESTR ": unknown error\n");
				errno = EIO;
			}
#undef ESTR
			free(rl);
			return 0;
		}
		total += r;
	}
	free(rl);
	return total;
}

/* Already cleaned up code below, but still look for FIXME:... */

/**
 * __ntfs_attr_init - primary initialization of an ntfs attribute structure
 * @na:		ntfs attribute to initialize
 * @ni:		ntfs inode with which to initialize the ntfs attribute
 * @type:	attribute type
 * @name:	attribute name in little endian Unicode or NULL
 * @name_len:	length of attribute @name in Unicode characters (if @name given)
 *
 * Initialize the ntfs attribute @na with @ni, @type, @name, and @name_len.
 */
static void __ntfs_attr_init(ntfs_attr *na, ntfs_inode *ni,
		const ATTR_TYPES type, ntfschar *name, const u32 name_len)
{
	na->rl = NULL;
	na->ni = ni;
	na->type = type;
	na->name = name;
	if (name)
		na->name_len = name_len;
	else
		na->name_len = 0;
}

/**
 * ntfs_attr_init - initialize an ntfs_attr with data sizes and status
 * @na:
 * @non_resident:
 * @compressed:
 * @encrypted:
 * @sparse:
 * @allocated_size:
 * @data_size:
 * @initialized_size:
 * @compressed_size:
 * @compression_unit:
 *
 * Final initialization for an ntfs attribute.
 */
void ntfs_attr_init(ntfs_attr *na, const BOOL non_resident,
		const BOOL compressed, const BOOL encrypted, const BOOL sparse,
		const s64 allocated_size, const s64 data_size,
		const s64 initialized_size, const s64 compressed_size,
		const u8 compression_unit)
{
	if (!NAttrInitialized(na)) {
		if (non_resident)
			NAttrSetNonResident(na);
		if (compressed)
			NAttrSetCompressed(na);
		if (encrypted)
			NAttrSetEncrypted(na);
		if (sparse)
			NAttrSetSparse(na);
		na->allocated_size = allocated_size;
		na->data_size = data_size;
		na->initialized_size = initialized_size;
		if (compressed || sparse) {
			ntfs_volume *vol = na->ni->vol;

			na->compressed_size = compressed_size;
			na->compression_block_clusters = 1 << compression_unit;
			na->compression_block_size = 1 << (compression_unit +
					vol->cluster_size_bits);
			na->compression_block_size_bits = ffs(
					na->compression_block_size) - 1;
		}
		NAttrSetInitialized(na);
	}
}

/**
 * ntfs_attr_open - open an ntfs attribute for access
 * @ni:		open ntfs inode in which the ntfs attribute resides
 * @type:	attribute type
 * @name:	attribute name in little endian Unicode or AT_UNNAMED or NULL
 * @name_len:	length of attribute @name in Unicode characters (if @name given)
 *
 * Allocate a new ntfs attribute structure, initialize it with @ni, @type,
 * @name, and @name_len, then return it. Return NULL on error with
 * errno set to the error code.
 *
 * If @name is AT_UNNAMED look specifically for an unnamed attribute.  If you
 * do not care whether the attribute is named or not set @name to NULL.  In
 * both those cases @name_len is not used at all.
 */
ntfs_attr *ntfs_attr_open(ntfs_inode *ni, const ATTR_TYPES type,
		ntfschar *name, u32 name_len)
{
	ntfs_attr_search_ctx *ctx;
	ntfs_attr *na;
	ATTR_RECORD *a;
	struct list_head *pos;
	int err;
	BOOL cs;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n",
		(unsigned long long)ni->mft_no, type);
	if (!ni || !ni->vol || !ni->mrec) {
		errno = EINVAL;
		return NULL;
	}
	/* Check cache, maybe this attribute already opened? */
	list_for_each(pos, &ni->attr_cache) {
		ntfs_attr *tmp_na;

		tmp_na = list_entry(pos, ntfs_attr, list_entry);
		if (tmp_na->type == type && tmp_na->name_len == name_len &&
				!ntfs_ucsncmp(tmp_na->name, name, name_len)) {
			ntfs_log_trace("Found this attribute in cache, "
					"increment reference count and "
					"return it.\n");
			tmp_na->nr_references++;
			return tmp_na;
		}
	}
	/* Search failed. Properly open attrbute. */
	na = calloc(sizeof(ntfs_attr), 1);
	if (!na)
		return NULL;
	if (name && name != AT_UNNAMED && name != NTFS_INDEX_I30) {
		name = ntfs_ucsndup(name, name_len);
		if (!name) {
			err = errno;
			free(na);
			errno = err;
			return NULL;
		}
	}

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx) {
		err = errno;
		goto err_out;
	}
	if (ntfs_attr_lookup(type, name, name_len, 0, 0, NULL, 0, ctx)) {
		err = errno;
		goto put_err_out;
	}

	a = ctx->attr;
	/*
	 * Wipe the flags in case they are not zero for an attribute list
	 * attribute.  Windows does not complain about invalid flags and chkdsk
	 * does not detect or fix them so we need to cope with it, too.
	 */
	if (type == AT_ATTRIBUTE_LIST)
		a->flags = 0;
	cs = (a->flags & (ATTR_IS_COMPRESSED | ATTR_IS_SPARSE)) ? 1 : 0;
	if (!name) {
		if (a->name_length) {
			name = ntfs_ucsndup((ntfschar*)((u8*)a + le16_to_cpu(
					a->name_offset)), a->name_length);
			if (!name) {
				err = errno;
				goto put_err_out;
			}
			name_len = a->name_length;
		} else {
			name = AT_UNNAMED;
			name_len = 0;
		}
	}
	__ntfs_attr_init(na, ni, type, name, name_len);
	if (a->non_resident) {
		ntfs_attr_init(na, TRUE, (a->flags & ATTR_IS_COMPRESSED)? 1 : 0,
				(a->flags & ATTR_IS_ENCRYPTED) ? 1 : 0,
				(a->flags & ATTR_IS_SPARSE) ? 1 : 0,
				sle64_to_cpu(a->allocated_size),
				sle64_to_cpu(a->data_size),
				sle64_to_cpu(a->initialized_size),
				cs ? sle64_to_cpu(a->compressed_size) : 0,
				cs ? a->compression_unit : 0);
	} else {
		s64 l = le32_to_cpu(a->value_length);
		ntfs_attr_init(na, FALSE, (a->flags & ATTR_IS_COMPRESSED) ? 1:0,
				(a->flags & ATTR_IS_ENCRYPTED) ? 1 : 0,
				(a->flags & ATTR_IS_SPARSE) ? 1 : 0,
				(l + 7) & ~7, l, l, cs ? (l + 7) & ~7 : 0, 0);
	}
	ntfs_attr_put_search_ctx(ctx);
	if (NAttrEncrypted(na))
		ntfs_crypto_attr_open(na);
	list_add_tail(&na->list_entry, &ni->attr_cache);
	na->nr_references = 1;
	return na;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
err_out:
	free(na);
	errno = err;
	return NULL;
}

/**
 * ntfs_attr_close - free an ntfs attribute structure
 * @na:		ntfs attribute structure to free
 *
 * Release all memory associated with the ntfs attribute @na and then release
 * @na itself.
 */
void ntfs_attr_close(ntfs_attr *na)
{
	if (!na)
		return;
	na->nr_references--;
	if (na->nr_references) {
		ntfs_log_trace("There are %d more references left to "
				"this attribute.\n", na->nr_references);
		return;
	}
	ntfs_log_trace("There are no more references left to this attribute\n");
	list_del(&na->list_entry);
	if (NAttrEncrypted(na))
		ntfs_crypto_attr_close(na);
	if (NAttrNonResident(na) && na->rl)
		free(na->rl);
	/* Don't release if using an internal constant. */
	if (na->name != AT_UNNAMED && na->name != NTFS_INDEX_I30)
		free(na->name);
	free(na);
}

/**
 * ntfs_attr_map_runlist - map (a part of) a runlist of an ntfs attribute
 * @na:		ntfs attribute for which to map (part of) a runlist
 * @vcn:	map runlist part containing this vcn
 *
 * Map the part of a runlist containing the @vcn of the ntfs attribute @na.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attr_map_runlist(ntfs_attr *na, VCN vcn)
{
	LCN lcn;
	ntfs_attr_search_ctx *ctx;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, vcn 0x%llx.\n",
		(unsigned long long)na->ni->mft_no, na->type, (long long)vcn);

	lcn = ntfs_rl_vcn_to_lcn(na->rl, vcn);
	if (lcn >= 0 || lcn == LCN_HOLE || lcn == LCN_ENOENT)
		return 0;

	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx)
		return -1;

	/* Find the attribute in the mft record. */
	if (!ntfs_attr_lookup(na->type, na->name, na->name_len, CASE_SENSITIVE,
			vcn, NULL, 0, ctx)) {
		runlist_element *rl;

		/* Decode the runlist. */
		rl = ntfs_mapping_pairs_decompress(na->ni->vol, ctx->attr,
				na->rl);
		if (rl) {
			na->rl = rl;
			ntfs_attr_put_search_ctx(ctx);
			return 0;
		}
	}
	ntfs_attr_put_search_ctx(ctx);
	return -1;
}

/**
 * ntfs_attr_map_runlist_range - map (a part of) a runlist of an ntfs attribute
 * @na:		ntfs attribute for which to map (part of) a runlist
 * @from_vcn:	map runlist part starting this vcn
 * @to_vcn:	map runlist part ending this vcn
 *
 * Map the part of a runlist from containing the @from_vcn to containing the
 * @to_vcn of an ntfs attribute @na. It is OK for @to_vcn to be beyond last run.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attr_map_runlist_range(ntfs_attr *na, VCN from_vcn, VCN to_vcn)
{
	ntfs_attr_search_ctx *ctx = NULL;
	runlist *rl;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, "
			"from_vcn 0x%llx, to_vcn 0x%llx.\n",
		(unsigned long long)na->ni->mft_no, na->type,
		(long long)from_vcn, (long long)to_vcn);

	/* Map extent with @from_vcn. */
	if (ntfs_attr_map_runlist(na, from_vcn))
		goto err_out;

	for (rl = na->rl; rl->vcn <= to_vcn;) {
		/* Skip not interesting to us runs. */
		if (rl->lcn >= 0 || rl->lcn == LCN_HOLE || (rl->vcn +
					rl->length < from_vcn &&
					rl->lcn == LCN_RL_NOT_MAPPED)) {
			rl++;
			continue;
		}

		/* We reached the end of runlist, just exit. */
		if (rl->lcn == LCN_ENOENT)
			break;

		/* Check for errors. */
		if (rl->lcn < 0 && rl->lcn != LCN_RL_NOT_MAPPED) {
			errno = EIO;
			goto err_out;
		}

		/* Runlist is not mapped here. */
		if (!ctx) {
			ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
			if (!ctx)
				goto err_out;
		}
		/* Find the attribute in the mft record. */
		if (ntfs_attr_lookup(na->type, na->name, na->name_len,
					CASE_SENSITIVE, rl->vcn, NULL, 0,
					ctx))
			goto err_out;

		/* Decode the runlist. */
		rl = ntfs_mapping_pairs_decompress(na->ni->vol, ctx->attr,
				na->rl);
		if (!rl)
			goto err_out;
		na->rl = rl;
	}

	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_trace("Done.\n");
	return 0;
err_out:
	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_trace("Failed.\n");
	return -1;
}

/**
 * ntfs_attr_map_whole_runlist - map the whole runlist of an ntfs attribute
 * @na:		ntfs attribute for which to map the runlist
 *
 * Map the whole runlist of the ntfs attribute @na.  For an attribute made up
 * of only one attribute extent this is the same as calling
 * ntfs_attr_map_runlist(na, 0) but for an attribute with multiple extents this
 * will map the runlist fragments from each of the extents thus giving access
 * to the entirety of the disk allocation of an attribute.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attr_map_whole_runlist(ntfs_attr *na)
{
	VCN next_vcn, last_vcn, highest_vcn;
	ntfs_attr_search_ctx *ctx;
	ntfs_volume *vol = na->ni->vol;
	ATTR_RECORD *a;
	int err;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n",
			(unsigned long long)na->ni->mft_no, na->type);

	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx)
		return -1;

	/* Map all attribute extents one by one. */
	next_vcn = last_vcn = highest_vcn = 0;
	a = NULL;
	while (1) {
		runlist_element *rl;

		int not_mapped = 0;
		if (ntfs_rl_vcn_to_lcn(na->rl, next_vcn) == LCN_RL_NOT_MAPPED)
			not_mapped = 1;

		if (ntfs_attr_lookup(na->type, na->name, na->name_len,
				CASE_SENSITIVE, next_vcn, NULL, 0, ctx))
			break;

		a = ctx->attr;

		if (not_mapped) {
			/* Decode the runlist. */
			rl = ntfs_mapping_pairs_decompress(na->ni->vol,
								a, na->rl);
			if (!rl)
				goto err_out;
			na->rl = rl;
		}

		/* Are we in the first extent? */
		if (!next_vcn) {
			 if (a->lowest_vcn) {
				ntfs_log_trace("First extent of attribute has "
						"non zero lowest_vcn. "
						"Inode is corrupt.\n");
				errno = EIO;
				goto err_out;
			}
			/* Get the last vcn in the attribute. */
			last_vcn = sle64_to_cpu(a->allocated_size) >>
					vol->cluster_size_bits;
		}

		/* Get the lowest vcn for the next extent. */
		highest_vcn = sle64_to_cpu(a->highest_vcn);
		next_vcn = highest_vcn + 1;

		/* Only one extent or error, which we catch below. */
		if (next_vcn <= 0) {
			errno = ENOENT;
			break;
		}

		/* Avoid endless loops due to corruption. */
		if (next_vcn < sle64_to_cpu(a->lowest_vcn)) {
			ntfs_log_trace("Inode has corrupt attribute list "
					"attribute.\n");
			errno = EIO;
			goto err_out;
		}
	}
	if (!a) {
		if (errno == ENOENT)
			ntfs_log_trace("Attribute not found. "
					"Inode is corrupt.\n");
		else
			ntfs_log_trace("Inode is corrupt.\n");
		goto err_out;
	}
	if (highest_vcn && highest_vcn != last_vcn - 1) {
		ntfs_log_trace("Failed to load the complete run list for the "
				"attribute. Bug or corrupt inode.\n");
		ntfs_log_trace("highest_vcn = 0x%llx, last_vcn - 1 = 0x%llx\n",
				(long long)highest_vcn,
				(long long)last_vcn - 1);
		errno = EIO;
		goto err_out;
	}
	err = errno;
	ntfs_attr_put_search_ctx(ctx);
	if (err == ENOENT)
		return 0;
out_now:
	errno = err;
	return -1;
err_out:
	err = errno;
	ntfs_attr_put_search_ctx(ctx);
	goto out_now;
}

/**
 * ntfs_attr_vcn_to_lcn - convert a vcn into a lcn given an ntfs attribute
 * @na:		ntfs attribute whose runlist to use for conversion
 * @vcn:	vcn to convert
 *
 * Convert the virtual cluster number @vcn of an attribute into a logical
 * cluster number (lcn) of a device using the runlist @na->rl to map vcns to
 * their corresponding lcns.
 *
 * If the @vcn is not mapped yet, attempt to map the attribute extent
 * containing the @vcn and retry the vcn to lcn conversion.
 *
 * Since lcns must be >= 0, we use negative return values with special meaning:
 *
 * Return value		Meaning / Description
 * ==========================================
 *  -1 = LCN_HOLE	Hole / not allocated on disk.
 *  -3 = LCN_ENOENT	There is no such vcn in the attribute.
 *  -4 = LCN_EINVAL	Input parameter error.
 *  -5 = LCN_EIO	Corrupt fs, disk i/o error, or not enough memory.
 */
LCN ntfs_attr_vcn_to_lcn(ntfs_attr *na, const VCN vcn)
{
	LCN lcn;
	BOOL is_retry = FALSE;

	if (!na || !NAttrNonResident(na) || vcn < 0)
		return (LCN)LCN_EINVAL;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n", (unsigned long
			long)na->ni->mft_no, na->type);
retry:
	/* Convert vcn to lcn. If that fails map the runlist and retry once. */
	lcn = ntfs_rl_vcn_to_lcn(na->rl, vcn);
	if (lcn >= 0)
		return lcn;
	if (!is_retry && !ntfs_attr_map_runlist(na, vcn)) {
		is_retry = TRUE;
		goto retry;
	}
	/*
	 * If the attempt to map the runlist failed, or we are getting
	 * LCN_RL_NOT_MAPPED despite having mapped the attribute extent
	 * successfully, something is really badly wrong...
	 */
	if (!is_retry || lcn == (LCN)LCN_RL_NOT_MAPPED)
		return (LCN)LCN_EIO;
	/* lcn contains the appropriate error code. */
	return lcn;
}

/**
 * ntfs_attr_find_vcn - find a vcn in the runlist of an ntfs attribute
 * @na:		ntfs attribute whose runlist to search
 * @vcn:	vcn to find
 *
 * Find the virtual cluster number @vcn in the runlist of the ntfs attribute
 * @na and return the the address of the runlist element containing the @vcn.
 *
 * Note you need to distinguish between the lcn of the returned runlist
 * element being >= 0 and LCN_HOLE. In the later case you have to return zeroes
 * on read and allocate clusters on write. You need to update the runlist, the
 * attribute itself as well as write the modified mft record to disk.
 *
 * If there is an error return NULL with errno set to the error code. The
 * following error codes are defined:
 *	EINVAL		Input parameter error.
 *	ENOENT		There is no such vcn in the runlist.
 *	ENOMEM		Not enough memory.
 *	EIO		I/O error or corrupt metadata.
 */
runlist_element *ntfs_attr_find_vcn(ntfs_attr *na, const VCN vcn)
{
	runlist_element *rl;
	BOOL is_retry = FALSE;

	if (!na || !NAttrNonResident(na) || vcn < 0) {
		errno = EINVAL;
		return NULL;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, vcn %llx\n",
		       (unsigned long long)na->ni->mft_no, na->type,
		       (long long)vcn);
retry:
	rl = na->rl;
	if (!rl)
		goto map_rl;
	if (vcn < rl[0].vcn)
		goto map_rl;
	while (rl->length) {
		if (vcn < rl[1].vcn) {
			if (rl->lcn >= (LCN)LCN_HOLE)
				return rl;
			break;
		}
		rl++;
	}
	switch (rl->lcn) {
	case (LCN)LCN_RL_NOT_MAPPED:
		goto map_rl;
	case (LCN)LCN_ENOENT:
		errno = ENOENT;
		break;
	case (LCN)LCN_EINVAL:
		errno = EINVAL;
		break;
	default:
		errno = EIO;
		break;
	}
	return NULL;
map_rl:
	/* The @vcn is in an unmapped region, map the runlist and retry. */
	if (!is_retry && !ntfs_attr_map_runlist(na, vcn)) {
		is_retry = TRUE;
		goto retry;
	}
	/*
	 * If we already retried or the mapping attempt failed something has
	 * gone badly wrong. EINVAL and ENOENT coming from a failed mapping
	 * attempt are equivalent to errors for us as they should not happen
	 * in our code paths.
	 */
	if (is_retry || errno == EINVAL || errno == ENOENT)
		errno = EIO;
	return NULL;
}

/**
 * ntfs_attr_pread - read from an attribute specified by an ntfs_attr structure
 * @na:		ntfs attribute to read from
 * @pos:	byte position in the attribute to begin reading from
 * @count:	number of bytes to read
 * @b:		output data buffer
 *
 * This function will read @count bytes starting at offset @pos from the ntfs
 * attribute @na into the data buffer @b.
 *
 * On success, return the number of successfully read bytes. If this number is
 * lower than @count this means that the read reached end of file or that an
 * error was encountered during the read so that the read is partial. 0 means
 * end of file or nothing was read (also return 0 when @count is 0).
 *
 * On error and nothing has been read, return -1 with errno set appropriately
 * to the return code of ntfs_pread(), or to EINVAL in case of invalid
 * arguments.
 */
s64 ntfs_attr_pread(ntfs_attr *na, const s64 pos, s64 count, void *b)
{
	s64 br, to_read, ofs, total, total2;
	ntfs_volume *vol;
	runlist_element *rl;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, pos 0x%llx, "
			"count 0x%llx.\n", (unsigned long long)na->ni->mft_no,
			na->type, (long long)pos, (long long)count);
	if (!na || !na->ni || !na->ni->vol || !b || pos < 0 || count < 0) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * If this is a compressed attribute it needs special treatment, but
	 * only if it is non-resident.
	 */
	if (NAttrCompressed(na) && NAttrNonResident(na))
		return ntfs_compressed_attr_pread(na, pos, count, b);
	/*
	 * Encrypted non-resident attributes are not supported.  We return
	 * access denied, which is what Windows NT4 does, too.
	 */
	if (NAttrEncrypted(na) && NAttrNonResident(na))
		return ntfs_crypto_attr_pread(na, pos, count, b);

	vol = na->ni->vol;
	if (!count)
		return 0;
	/* Truncate reads beyond end of attribute. */
	if (pos + count > na->data_size) {
		if (pos >= na->data_size)
			return 0;
		count = na->data_size - pos;
	}
	/* If it is a resident attribute, get the value from the mft record. */
	if (!NAttrNonResident(na)) {
		ntfs_attr_search_ctx *ctx;
		char *val;

		ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
		if (!ctx)
			return -1;
		if (ntfs_attr_lookup(na->type, na->name, na->name_len, 0,
				0, NULL, 0, ctx)) {
			int eo;
res_err_out:
			eo = errno;
			ntfs_attr_put_search_ctx(ctx);
			errno = eo;
			return -1;
		}
		val = (char*)ctx->attr + le16_to_cpu(ctx->attr->value_offset);
		if (val < (char*)ctx->attr || val +
				le32_to_cpu(ctx->attr->value_length) >
				(char*)ctx->mrec + vol->mft_record_size) {
			errno = EIO;
			goto res_err_out;
		}
		memcpy(b, val + pos, count);
		ntfs_attr_put_search_ctx(ctx);
		return count;
	}
	total = total2 = 0;
	/* Zero out reads beyond initialized size. */
	if (pos + count > na->initialized_size) {
		if (pos >= na->initialized_size) {
			memset(b, 0, count);
			return count;
		}
		total2 = pos + count - na->initialized_size;
		count -= total2;
		memset((u8*)b + count, 0, total2);
	}
	/* Find the runlist element containing the vcn. */
	rl = ntfs_attr_find_vcn(na, pos >> vol->cluster_size_bits);
	if (!rl) {
		/*
		 * If the vcn is not present it is an out of bounds read.
		 * However, we already truncated the read to the data_size,
		 * so getting this here is an error.
		 */
		if (errno == ENOENT)
			errno = EIO;
		return -1;
	}
	/*
	 * Gather the requested data into the linear destination buffer. Note,
	 * a partial final vcn is taken care of by the @count capping of read
	 * length.
	 */
	ofs = pos - (rl->vcn << vol->cluster_size_bits);
	for (; count; rl++, ofs = 0) {
		if (rl->lcn == LCN_RL_NOT_MAPPED) {
			rl = ntfs_attr_find_vcn(na, rl->vcn);
			if (!rl) {
				if (errno == ENOENT)
					errno = EIO;
				goto rl_err_out;
			}
			/* Needed for case when runs merged. */
			ofs = pos + total - (rl->vcn << vol->cluster_size_bits);
		}
		if (!rl->length)
			goto rl_err_out;
		if (rl->lcn < (LCN)0) {
			if (rl->lcn != (LCN)LCN_HOLE)
				goto rl_err_out;
			/* It is a hole, just zero the matching @b range. */
			to_read = min(count, (rl->length <<
					vol->cluster_size_bits) - ofs);
			memset(b, 0, to_read);
			/* Update progress counters. */
			total += to_read;
			count -= to_read;
			b = (u8*)b + to_read;
			continue;
		}
		/* It is a real lcn, read it into @dst. */
		to_read = min(count, (rl->length << vol->cluster_size_bits) -
				ofs);
retry:
		ntfs_log_trace("Reading 0x%llx bytes from vcn 0x%llx, "
				"lcn 0x%llx, ofs 0x%llx.\n", to_read, rl->vcn,
				rl->lcn, ofs);
		br = ntfs_pread(vol->dev, (rl->lcn << vol->cluster_size_bits) +
				ofs, to_read, b);
		/* If everything ok, update progress counters and continue. */
		if (br > 0) {
			total += br;
			count -= br;
			b = (u8*)b + br;
			continue;
		}
		/* If the syscall was interrupted, try again. */
		if (br == (s64)-1 && errno == EINTR)
			goto retry;
		if (total)
			return total;
		if (!br)
			errno = EIO;
		return -1;
	}
	/* Finally, return the number of bytes read. */
	return total + total2;
rl_err_out:
	if (total)
		return total;
	errno = EIO;
	return -1;
}

/**
 * ntfs_attr_pwrite - positioned write to an ntfs attribute
 * @na:		ntfs attribute to write to
 * @pos:	position in the attribute to write to
 * @count:	number of bytes to write
 * @b:		data buffer to write to disk
 *
 * This function will write @count bytes from data buffer @b to ntfs attribute
 * @na at position @pos.
 *
 * On success, return the number of successfully written bytes. If this number
 * is lower than @count this means that an error was encountered during the
 * write so that the write is partial. 0 means nothing was written (also return
 * 0 when @count is 0).
 *
 * On error and nothing has been written, return -1 with errno set
 * appropriately to the return code of ntfs_pwrite(), or to EINVAL in case of
 * invalid arguments.
 */
s64 ntfs_attr_pwrite(ntfs_attr *na, const s64 pos, s64 count, const void *b)
{
	s64 written, to_write, ofs, total, old_initialized_size, old_data_size;
	VCN update_from = -1;
	ntfs_volume *vol;
	ntfs_attr_search_ctx *ctx = NULL;
	runlist_element *rl;
	int eo;
	struct {
		unsigned int undo_initialized_size	: 1;
		unsigned int undo_data_size		: 1;
		unsigned int update_mapping_pairs	: 1;
	} need_to = { 0, 0, 0 };

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, pos 0x%llx, "
			"count 0x%llx.\n", na->ni->mft_no, na->type,
			(long long)pos, (long long)count);
	if (!na || !na->ni || !na->ni->vol || !b || pos < 0 || count < 0) {
		errno = EINVAL;
		return -1;
	}
	vol = na->ni->vol;
	/*
	 * Encrypted non-resident attributes are not supported.  We return
	 * access denied, which is what Windows NT4 does, too.
	 */
	if (NAttrEncrypted(na) && NAttrNonResident(na)) {
		errno = EACCES;
		return -1;
	}
	/* If this is a compressed attribute it needs special treatment. */
	if (NAttrCompressed(na)) {
		// TODO: Implement writing compressed attributes! (AIA)
		// return ntfs_attr_pwrite_compressed(ntfs_attr *na,
		//		const s64 pos, s64 count, void *b);
		errno = EOPNOTSUPP;
		return -1;
	}
	if (!count)
		return 0;
	/* If the write reaches beyond the end, extend the attribute. */
	old_data_size = na->data_size;
	if (pos + count > na->data_size) {
		if (__ntfs_attr_truncate(na, pos + count, FALSE)) {
			eo = errno;
			ntfs_log_trace("Attribute extend failed.\n");
			errno = eo;
			return -1;
		}
		need_to.undo_data_size = 1;
	}
	old_initialized_size = na->initialized_size;
	/* If it is a resident attribute, write the data to the mft record. */
	if (!NAttrNonResident(na)) {
		char *val;

		ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
		if (!ctx)
			goto err_out;
		if (ntfs_attr_lookup(na->type, na->name, na->name_len, 0,
				0, NULL, 0, ctx))
			goto err_out;
		val = (char*)ctx->attr + le16_to_cpu(ctx->attr->value_offset);
		if (val < (char*)ctx->attr || val +
				le32_to_cpu(ctx->attr->value_length) >
				(char*)ctx->mrec + vol->mft_record_size) {
			errno = EIO;
			goto err_out;
		}
		memcpy(val + pos, b, count);
		if (ntfs_mft_record_write(vol, ctx->ntfs_ino->mft_no,
				ctx->mrec)) {
			/*
			 * NOTE: We are in a bad state at this moment. We have
			 * dirtied the mft record but we failed to commit it to
			 * disk. Since we have read the mft record ok before,
			 * it is unlikely to fail writing it, so is ok to just
			 * return error here... (AIA)
			 */
			goto err_out;
		}
		ntfs_attr_put_search_ctx(ctx);
		return count;
	}
	total = 0;
	/* Handle writes beyond initialized_size. */
	if (pos + count > na->initialized_size) {
		/*
		 * Map runlist between initialized size and place we start
		 * writing at.
		 */
		if (ntfs_attr_map_runlist_range(na, na->initialized_size >>
					vol->cluster_size_bits,
					pos >> vol->cluster_size_bits))
			goto err_out;
		/* Set initialized_size to @pos + @count. */
		ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
		if (!ctx)
			goto err_out;
		if (ntfs_attr_lookup(na->type, na->name, na->name_len, 0,
				0, NULL, 0, ctx))
			goto err_out;
		/* If write starts beyond initialized_size, zero the gap. */
		if (pos > na->initialized_size && ntfs_rl_fill_zero(vol,
				na->rl, na->initialized_size,
				pos - na->initialized_size))
			goto err_out;

		ctx->attr->initialized_size = cpu_to_sle64(pos + count);
		if (ntfs_mft_record_write(vol, ctx->ntfs_ino->mft_no,
				ctx->mrec)) {
			/*
			 * Undo the change in the in-memory copy and send it
			 * back for writing.
			 */
			ctx->attr->initialized_size =
					cpu_to_sle64(old_initialized_size);
			ntfs_mft_record_write(vol, ctx->ntfs_ino->mft_no,
					ctx->mrec);
			goto err_out;
		}
		na->initialized_size = pos + count;
		ntfs_attr_put_search_ctx(ctx);
		ctx = NULL;
		/*
		 * NOTE: At this point the initialized_size in the mft record
		 * has been updated BUT there is random data on disk thus if
		 * we decide to abort, we MUST change the initialized_size
		 * again.
		 */
		need_to.undo_initialized_size = 1;
	}
	/* Find the runlist element containing the vcn. */
	rl = ntfs_attr_find_vcn(na, pos >> vol->cluster_size_bits);
	if (!rl) {
		/*
		 * If the vcn is not present it is an out of bounds write.
		 * However, we already extended the size of the attribute,
		 * so getting this here must be an error of some kind.
		 */
		if (errno == ENOENT)
			errno = EIO;
		goto err_out;
	}
	/*
	 * Scatter the data from the linear data buffer to the volume. Note, a
	 * partial final vcn is taken care of by the @count capping of write
	 * length.
	 */
	ofs = pos - (rl->vcn << vol->cluster_size_bits);
	for (; count; rl++, ofs = 0) {
		if (rl->lcn == LCN_RL_NOT_MAPPED) {
			rl = ntfs_attr_find_vcn(na, rl->vcn);
			if (!rl) {
				if (errno == ENOENT)
					errno = EIO;
				goto rl_err_out;
			}
			/* Needed for case when runs merged. */
			ofs = pos + total - (rl->vcn << vol->cluster_size_bits);
		}
		if (!rl->length) {
			errno = EIO;
			goto rl_err_out;
		}
		if (rl->lcn < (LCN)0) {
			LCN lcn_seek_from = -1;
			runlist *rlc;
			VCN cur_vcn, from_vcn;

			if (rl->lcn != (LCN)LCN_HOLE) {
				errno = EIO;
				goto rl_err_out;
			}

			to_write = min(count, (rl->length <<
					vol->cluster_size_bits) - ofs);

			/* Instantiate the hole. */
			cur_vcn = rl->vcn;
			from_vcn = rl->vcn + (ofs >> vol->cluster_size_bits);
			ntfs_log_trace("Instantiate hole with vcn 0x%llx.\n",
					cur_vcn);
			/*
			 * Map whole runlist to be able update mapping pairs
			 * later.
			 */
			if (ntfs_attr_map_whole_runlist(na))
				goto err_out;
			/*
			 * Restore @rl, it probably get lost during runlist
			 * mapping.
			 */
			rl = ntfs_attr_find_vcn(na, cur_vcn);
			if (!rl) {
				ntfs_log_error("BUG! Failed to find run after "
						"mapping whole runlist. Please "
						"report to the %s.\n",
						NTFS_DEV_LIST);
				errno = EIO;
				goto err_out;
			}
			/*
			 * Search backwards to find the best lcn to start
			 * seek from.
			 */
			rlc = rl;
			while (rlc->vcn) {
				rlc--;
				if (rlc->lcn >= 0) {
					lcn_seek_from = rlc->lcn +
							(from_vcn - rlc->vcn);
					break;
				}
			}
			if (lcn_seek_from == -1) {
				/* Backwards search failed, search forwards. */
				rlc = rl;
				while (rlc->length) {
					rlc++;
					if (rlc->lcn >= 0) {
						lcn_seek_from = rlc->lcn -
							(rlc->vcn - from_vcn);
						if (lcn_seek_from < -1)
							lcn_seek_from = -1;
						break;
					}
				}
			}
			/* Allocate clusters to instantiate the hole. */
			rlc = ntfs_cluster_alloc(vol, from_vcn,
						((ofs + to_write - 1) >>
						vol->cluster_size_bits) + 1 +
						rl->vcn - from_vcn,
						lcn_seek_from, DATA_ZONE);
			if (!rlc) {
				eo = errno;
				ntfs_log_trace("Failed to allocate clusters "
						"for hole instantiating.\n");
				errno = eo;
				goto err_out;
			}
			/* Merge runlists. */
			rl = ntfs_runlists_merge(na->rl, rlc);
			if (!rl) {
				eo = errno;
				ntfs_log_trace("Failed to merge runlists.\n");
				if (ntfs_cluster_free_from_rl(vol, rlc)) {
					ntfs_log_trace("Failed to free just "
						"allocated clusters. Leaving "
						"inconsistent metadata. "
						"Run chkdsk\n");
				}
				errno = eo;
				goto err_out;
			}
			na->rl = rl;
			need_to.update_mapping_pairs = 1;
			if (update_from == -1)
				update_from = from_vcn;
			rl = ntfs_attr_find_vcn(na, cur_vcn);
			if (!rl) {
				/*
				 * It's definitely a BUG, if we failed to find
				 * @cur_vcn, because we missed it during
				 * instantiating of the hole.
				 */
				ntfs_log_error("BUG! Failed to find run after "
						"instantiating. Please report "
						"to the %s.\n", NTFS_DEV_LIST);
				errno = EIO;
				goto err_out;
			}
			/* If leaved part of the hole go to the next run. */
			if (rl->lcn < 0)
				rl++;
			/* Now LCN shoudn't be less than 0. */
			if (rl->lcn < 0) {
				ntfs_log_error("BUG! LCN is lesser than 0. "
						"Please report to the %s.\n",
						NTFS_DEV_LIST);
				errno = EIO;
				goto err_out;
			}
			if (rl->vcn < cur_vcn) {
				/*
				 * Clusters that replaced hole are merged with
				 * previous run, so we need to update offset.
				 */
				ofs += (cur_vcn - rl->vcn) <<
					vol->cluster_size_bits;
			}
			if (rl->vcn > cur_vcn) {
				/*
				 * We left part of the hole, so update we need
				 * to update offset
				 */
				ofs -= (rl->vcn - cur_vcn) <<
					vol->cluster_size_bits;
			}
			/*
			 * Clear region between start of @rl->vcn cluster and
			 * @ofs if necessary.
			 */
			if (ofs && ntfs_rl_fill_zero(vol, na->rl, rl->vcn <<
					vol->cluster_size_bits, ofs))
				goto err_out;
		}
		/* It is a real lcn, write it to the volume. */
		to_write = min(count, (rl->length << vol->cluster_size_bits) -
				ofs);
retry:
		ntfs_log_trace("Writing 0x%llx bytes to vcn 0x%llx, lcn 0x%llx,"
				" ofs 0x%llx.\n", to_write, rl->vcn, rl->lcn,
				ofs);
		if (!NVolReadOnly(vol)) {
			s64 pos = (rl->lcn << vol->cluster_size_bits) + ofs;
			int bsize = 4096; /* FIXME: Test whether we need
					     PAGE_SIZE here. Eg., on IA64. */
			/*
			 * Write 4096 size blocks if it's possible. This will
			 * cause the kernel not to seek and read disk blocks for
			 * filling the end of the buffer which increases write
			 * speed.
			 */
			if (vol->cluster_size >= bsize && !(ofs % bsize) &&
					(to_write % bsize) && ofs + to_write ==
					na->initialized_size) {
				char *cb;
				s64 rounded = (to_write + bsize - 1) &
					~(bsize - 1);

				cb = ntfs_malloc(rounded);
				if (!cb)
					goto err_out;
				memcpy(cb, b, to_write);
				memset(cb + to_write, 0, rounded - to_write);
				written = ntfs_pwrite(vol->dev, pos, rounded,
						cb);
				if (written > to_write)
					written = to_write;
				free(cb);
			} else
				written = ntfs_pwrite(vol->dev, pos, to_write,
						b);
		} else
			written = to_write;
		/* If everything ok, update progress counters and continue. */
		if (written > 0) {
			total += written;
			count -= written;
			b = (const u8*)b + written;
			continue;
		}
		/* If the syscall was interrupted, try again. */
		if (written == (s64)-1 && errno == EINTR)
			goto retry;
		if (!written)
			errno = EIO;
		goto rl_err_out;
	}
done:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	/* Update mapping pairs if needed. */
	if (need_to.update_mapping_pairs) {
		if (ntfs_attr_update_mapping_pairs(na, update_from)) {
			/* FIXME: We want rollback here. */
			ntfs_log_perror("%s(): Failed to update mapping pairs. "
					"Leaving inconsistent metadata. "
					"Run chkdsk!", __FUNCTION__);
			errno = EIO;
			return -1;
		}
	}
	/* Finally, return the number of bytes written. */
	return total;
rl_err_out:
	eo = errno;
	if (total) {
		if (need_to.undo_initialized_size) {
			if (pos + total > na->initialized_size)
				goto done;
			/*
			 * TODO: Need to try to change initialized_size. If it
			 * succeeds goto done, otherwise goto err_out. (AIA)
			 */
			errno = EOPNOTSUPP;
			goto err_out;
		}
		goto done;
	}
	errno = eo;
err_out:
	eo = errno;
	if (need_to.undo_initialized_size) {
		int err;

		err = 0;
		if (!ctx) {
			ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
			if (!ctx)
				err = 1;
		} else
			ntfs_attr_reinit_search_ctx(ctx);
		if (ctx) {
			err = ntfs_attr_lookup(na->type, na->name,
					na->name_len, 0, 0, NULL, 0, ctx);
			if (!err) {
				na->initialized_size = old_initialized_size;
				ctx->attr->initialized_size = cpu_to_sle64(
						old_initialized_size);
				err = ntfs_mft_record_write(vol,
						ctx->ntfs_ino->mft_no,
						ctx->mrec);
			}
		}
		if (err) {
			/*
			 * FIXME: At this stage could try to recover by filling
			 * old_initialized_size -> new_initialized_size with
			 * data or at least zeroes. (AIA)
			 */
			ntfs_log_error("Eeek! Failed to recover from error. "
					"Leaving metadata in inconsistent "
					"state! Run chkdsk!\n");
		}
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	/* Update mapping pairs if needed. */
	if (need_to.update_mapping_pairs)
		ntfs_attr_update_mapping_pairs(na, update_from);
	/* Restore original data_size if needed. */
	if (need_to.undo_data_size && ntfs_attr_truncate(na, old_data_size))
		ntfs_log_trace("Failed to restore data_size.\n");
	errno = eo;
	return -1;
}

/**
 * ntfs_attr_mst_pread - multi sector transfer protected ntfs attribute read
 * @na:		multi sector transfer protected ntfs attribute to read from
 * @pos:	byte position in the attribute to begin reading from
 * @bk_cnt:	number of mst protected blocks to read
 * @bk_size:	size of each mst protected block in bytes
 * @dst:	output data buffer
 *
 * This function will read @bk_cnt blocks of size @bk_size bytes each starting
 * at offset @pos from the ntfs attribute @na into the data buffer @b.
 *
 * On success, the multi sector transfer fixups are applied and the number of
 * read blocks is returned. If this number is lower than @bk_cnt this means
 * that the read has either reached end of attribute or that an error was
 * encountered during the read so that the read is partial. 0 means end of
 * attribute or nothing to read (also return 0 when @bk_cnt or @bk_size are 0).
 *
 * On error and nothing has been read, return -1 with errno set appropriately
 * to the return code of ntfs_attr_pread() or to EINVAL in case of invalid
 * arguments.
 *
 * NOTE: If an incomplete multi sector transfer is detected the magic is
 * changed to BAAD but no error is returned, i.e. it is possible that any of
 * the returned blocks have multi sector transfer errors. This should be
 * detected by the caller by checking each block with is_baad_recordp(&block).
 * The reasoning is that we want to fixup as many blocks as possible and we
 * want to return even bad ones to the caller so, e.g. in case of ntfsck, the
 * errors can be repaired.
 */
s64 ntfs_attr_mst_pread(ntfs_attr *na, const s64 pos, const s64 bk_cnt,
		const u32 bk_size, void *dst)
{
	s64 br;
	u8 *end;

	ntfs_log_trace("Entering for inode 0x%llx, attr type 0x%x, "
			"pos 0x%llx.\n", (unsigned long long)na->ni->mft_no,
			na->type, (long long)pos);
	if (bk_cnt < 0 || bk_size % NTFS_BLOCK_SIZE) {
		errno = EINVAL;
		return -1;
	}
	br = ntfs_attr_pread(na, pos, bk_cnt * bk_size, dst);
	if (br <= 0)
		return br;
	br /= bk_size;
	for (end = (u8*)dst + br * bk_size; (u8*)dst < end; dst = (u8*)dst +
			bk_size)
		ntfs_mst_post_read_fixup((NTFS_RECORD*)dst, bk_size);
	/* Finally, return the number of blocks read. */
	return br;
}

/**
 * ntfs_attr_mst_pwrite - multi sector transfer protected ntfs attribute write
 * @na:		multi sector transfer protected ntfs attribute to write to
 * @pos:	position in the attribute to write to
 * @bk_cnt:	number of mst protected blocks to write
 * @bk_size:	size of each mst protected block in bytes
 * @src:	data buffer to write to disk
 *
 * This function will write @bk_cnt blocks of size @bk_size bytes each from
 * data buffer @b to multi sector transfer (mst) protected ntfs attribute @na
 * at position @pos.
 *
 * On success, return the number of successfully written blocks. If this number
 * is lower than @bk_cnt this means that an error was encountered during the
 * write so that the write is partial. 0 means nothing was written (also
 * return 0 when @bk_cnt or @bk_size are 0).
 *
 * On error and nothing has been written, return -1 with errno set
 * appropriately to the return code of ntfs_attr_pwrite(), or to EINVAL in case
 * of invalid arguments.
 *
 * NOTE: We mst protect the data, write it, then mst deprotect it using a quick
 * deprotect algorithm (no checking). This saves us from making a copy before
 * the write and at the same time causes the usn to be incremented in the
 * buffer. This conceptually fits in better with the idea that cached data is
 * always deprotected and protection is performed when the data is actually
 * going to hit the disk and the cache is immediately deprotected again
 * simulating an mst read on the written data. This way cache coherency is
 * achieved.
 */
s64 ntfs_attr_mst_pwrite(ntfs_attr *na, const s64 pos, s64 bk_cnt,
		const u32 bk_size, void *src)
{
	s64 written, i;

	ntfs_log_trace("Entering for inode 0x%llx, attr type 0x%x, "
			"pos 0x%llx.\n", (unsigned long long)na->ni->mft_no,
			na->type, (long long)pos);
	if (bk_cnt < 0 || bk_size % NTFS_BLOCK_SIZE) {
		errno = EINVAL;
		return -1;
	}
	if (!bk_cnt)
		return 0;
	/* Prepare data for writing. */
	for (i = 0; i < bk_cnt; ++i) {
		int err;

		err = ntfs_mst_pre_write_fixup((NTFS_RECORD*)
				((u8*)src + i * bk_size), bk_size);
		if (err < 0) {
			/* Abort write at this position. */
			if (!i)
				return err;
			bk_cnt = i;
			break;
		}
	}
	/* Write the prepared data. */
	written = ntfs_attr_pwrite(na, pos, bk_cnt * bk_size, src);
	/* Quickly deprotect the data again. */
	for (i = 0; i < bk_cnt; ++i)
		ntfs_mst_post_write_fixup((NTFS_RECORD*)((u8*)src + i *
				bk_size));
	if (written <= 0)
		return written;
	/* Finally, return the number of complete blocks written. */
	return written / bk_size;
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
static int ntfs_attr_find(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	ntfs_volume *vol;
	ntfschar *upcase;
	u32 upcase_len;

	ntfs_log_trace("Entering for attribute type 0x%x.\n", type);

	if (ctx->ntfs_ino) {
		vol = ctx->ntfs_ino->vol;
		upcase = vol->upcase;
		upcase_len = vol->upcase_len;
	} else {
		if (name && name != AT_UNNAMED) {
			errno = EINVAL;
			return -1;
		}
		vol = NULL;
		upcase = NULL;
		upcase_len = 0;
	}
	/*
	 * Iterate over attributes in mft record starting at @ctx->attr, or the
	 * attribute following that, if @ctx->is_first is TRUE.
	 */
	if (ctx->is_first) {
		a = ctx->attr;
		ctx->is_first = FALSE;
	} else
		a = (ATTR_RECORD*)((char*)ctx->attr +
				le32_to_cpu(ctx->attr->length));
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
			register int rc;

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
		if (!val)
			return 0;
		/* @val is present; compare values. */
		else {
			register int rc;

			rc = memcmp(val, (char*)a +le16_to_cpu(a->value_offset),
					min(val_len,
					le32_to_cpu(a->value_length)));
			/*
			 * If @val collates before the current attribute's
			 * value, there is no matching attribute.
			 */
			if (!rc) {
				register u32 avl;
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
	ntfs_log_debug("ntfs_attr_find(): File is corrupt. Run chkdsk.\n");
	errno = EIO;
	return -1;
}

/**
 * ntfs_external_attr_find - find an attribute in the attribute list of an inode
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means don't care)
 * @name_len:	attribute name length (only needed if @name present)
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
 * @lowest_vcn:	lowest vcn to find (optional, non-resident attributes only)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length
 * @ctx:	search context with mft record and attribute to search from
 *
 * You shouldn't need to call this function directly. Use ntfs_attr_lookup()
 * instead.
 *
 * Find an attribute by searching the attribute list for the corresponding
 * attribute list entry. Having found the entry, map the mft record for read
 * if the attribute is in a different mft record/inode, find the attribute in
 * there and return it.
 *
 * If @type is AT_UNUSED, return the first found attribute, i.e. one can
 * enumerate all attributes by setting @type to AT_UNUSED and then calling
 * ntfs_external_attr_find() repeatedly until it returns -1 with errno set to
 * ENOENT to indicate that there are no more entries. During the enumeration,
 * each successful call of ntfs_external_attr_find() will return the next
 * attribute described by the attribute list of the base mft record described
 * by the search context @ctx.
 *
 * If @type is AT_END, seek to the end of the base mft record ignoring the
 * attribute list completely and return -1 with errno set to ENOENT.  AT_END is
 * not a valid attribute, its length is zero for example, thus it is safer to
 * return error instead of success in this case.
 *
 * If @name is AT_UNNAMED search for an unnamed attribute. If @name is present
 * but not AT_UNNAMED search for a named attribute matching @name. Otherwise,
 * match both named and unnamed attributes.
 *
 * On first search @ctx->ntfs_ino must be the inode of the base mft record and
 * @ctx must have been obtained from a call to ntfs_attr_get_search_ctx().
 * On subsequent calls, @ctx->ntfs_ino can be any extent inode, too
 * (@ctx->base_ntfs_ino is then the base inode).
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
 * belongs.
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
static int ntfs_external_attr_find(ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni;
	ntfs_volume *vol;
	ATTR_LIST_ENTRY *al_entry, *next_al_entry;
	u8 *al_start, *al_end;
	ATTR_RECORD *a;
	ntfschar *al_name;
	u32 al_name_len;
	BOOL is_first_search = FALSE;

	ni = ctx->ntfs_ino;
	base_ni = ctx->base_ntfs_ino;
	ntfs_log_trace("Entering for inode 0x%llx, attribute type 0x%x.\n",
			(unsigned long long)ni->mft_no, type);
	if (!base_ni) {
		/* First call happens with the base mft record. */
		base_ni = ctx->base_ntfs_ino = ctx->ntfs_ino;
		ctx->base_mrec = ctx->mrec;
	}
	if (ni == base_ni)
		ctx->base_attr = ctx->attr;
	if (type == AT_END)
		goto not_found;
	vol = base_ni->vol;
	al_start = base_ni->attr_list;
	al_end = al_start + base_ni->attr_list_size;
	if (!ctx->al_entry) {
		ctx->al_entry = (ATTR_LIST_ENTRY*)al_start;
		is_first_search = TRUE;
	}
	/*
	 * Iterate over entries in attribute list starting at @ctx->al_entry,
	 * or the entry following that, if @ctx->is_first is TRUE.
	 */
	if (ctx->is_first) {
		al_entry = ctx->al_entry;
		ctx->is_first = FALSE;
		/*
		 * If an enumeration and the first attribute is higher than
		 * the attribute list itself, need to return the attribute list
		 * attribute.
		 */
		if ((type == AT_UNUSED) && is_first_search &&
				le32_to_cpu(al_entry->type) >
				le32_to_cpu(AT_ATTRIBUTE_LIST))
			goto find_attr_list_attr;
	} else {
		al_entry = (ATTR_LIST_ENTRY*)((char*)ctx->al_entry +
				le16_to_cpu(ctx->al_entry->length));
		/*
		 * If this is an enumeration and the attribute list attribute
		 * is the next one in the enumeration sequence, just return the
		 * attribute list attribute from the base mft record as it is
		 * not listed in the attribute list itself.
		 */
		if ((type == AT_UNUSED) && le32_to_cpu(ctx->al_entry->type) <
				le32_to_cpu(AT_ATTRIBUTE_LIST) &&
				le32_to_cpu(al_entry->type) >
				le32_to_cpu(AT_ATTRIBUTE_LIST)) {
			int rc;
find_attr_list_attr:

			/* Check for bogus calls. */
			if (name || name_len || val || val_len || lowest_vcn) {
				errno = EINVAL;
				return -1;
			}

			/* We want the base record. */
			ctx->ntfs_ino = base_ni;
			ctx->mrec = ctx->base_mrec;
			ctx->is_first = TRUE;
			/* Sanity checks are performed elsewhere. */
			ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
					le16_to_cpu(ctx->mrec->attrs_offset));

			/* Find the attribute list attribute. */
			rc = ntfs_attr_find(AT_ATTRIBUTE_LIST, NULL, 0,
					IGNORE_CASE, NULL, 0, ctx);

			/*
			 * Setup the search context so the correct
			 * attribute is returned next time round.
			 */
			ctx->al_entry = al_entry;
			ctx->is_first = TRUE;

			/* Got it. Done. */
			if (!rc)
				return 0;

			/* Error! If other than not found return it. */
			if (errno != ENOENT)
				return rc;

			/* Not found?!? Absurd! Must be a bug... )-: */
			ntfs_log_trace("BUG! Attribute list attribute not "
					"found but it exists! "
					"Returning error (EINVAL).\n");
			errno = EINVAL;
			return -1;
		}
	}
	for (;; al_entry = next_al_entry) {
		/* Out of bounds check. */
		if ((u8*)al_entry < base_ni->attr_list ||
				(u8*)al_entry > al_end)
			break;	/* Inode is corrupt. */
		ctx->al_entry = al_entry;
		/* Catch the end of the attribute list. */
		if ((u8*)al_entry == al_end)
			goto not_found;
		if (!al_entry->length)
			break;
		if ((u8*)al_entry + 6 > al_end || (u8*)al_entry +
				le16_to_cpu(al_entry->length) > al_end)
			break;
		next_al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
				le16_to_cpu(al_entry->length));
		if (type != AT_UNUSED) {
			if (le32_to_cpu(al_entry->type) > le32_to_cpu(type))
				goto not_found;
			if (type != al_entry->type)
				continue;
		}
		al_name_len = al_entry->name_length;
		al_name = (ntfschar*)((u8*)al_entry + al_entry->name_offset);
		/*
		 * If !@type we want the attribute represented by this
		 * attribute list entry.
		 */
		if (type == AT_UNUSED)
			goto is_enumeration;
		/*
		 * If @name is AT_UNNAMED we want an unnamed attribute.
		 * If @name is present, compare the two names.
		 * Otherwise, match any attribute.
		 */
		if (name == AT_UNNAMED) {
			if (al_name_len)
				goto not_found;
		} else if (name && !ntfs_names_are_equal(al_name, al_name_len,
				name, name_len, ic, vol->upcase,
				vol->upcase_len)) {
			register int rc;

			rc = ntfs_names_collate(name, name_len, al_name,
					al_name_len, 1, IGNORE_CASE,
					vol->upcase, vol->upcase_len);
			/*
			 * If @name collates before al_name, there is no
			 * matching attribute.
			 */
			if (rc == -1)
				goto not_found;
			/* If the strings are not equal, continue search. */
			if (rc)
				continue;
			/*
			 * FIXME: Reverse engineering showed 0, IGNORE_CASE but
			 * that is inconsistent with ntfs_attr_find(). The
			 * subsequent rc checks were also different. Perhaps I
			 * made a mistake in one of the two. Need to recheck
			 * which is correct or at least see what is going
			 * on... (AIA)
			 */
			rc = ntfs_names_collate(name, name_len, al_name,
					al_name_len, 1, CASE_SENSITIVE,
					vol->upcase, vol->upcase_len);
			if (rc == -1)
				goto not_found;
			if (rc)
				continue;
		}
		/*
		 * The names match or @name not present and attribute is
		 * unnamed. Now check @lowest_vcn. Continue search if the
		 * next attribute list entry still fits @lowest_vcn. Otherwise
		 * we have reached the right one or the search has failed.
		 */
		if (lowest_vcn && (u8*)next_al_entry >= al_start	    &&
				(u8*)next_al_entry + 6 < al_end	    &&
				(u8*)next_al_entry + le16_to_cpu(
					next_al_entry->length) <= al_end    &&
				sle64_to_cpu(next_al_entry->lowest_vcn) <=
					lowest_vcn			    &&
				next_al_entry->type == al_entry->type	    &&
				next_al_entry->name_length == al_name_len   &&
				ntfs_names_are_equal((ntfschar*)((char*)
					next_al_entry +
					next_al_entry->name_offset),
					next_al_entry->name_length,
					al_name, al_name_len, CASE_SENSITIVE,
					vol->upcase, vol->upcase_len))
			continue;
is_enumeration:
		if (MREF_LE(al_entry->mft_reference) == ni->mft_no) {
			if (MSEQNO_LE(al_entry->mft_reference) !=
					le16_to_cpu(
					ni->mrec->sequence_number)) {
				ntfs_log_debug("Found stale mft reference in "
						"attribute list!\n");
				break;
			}
		} else { /* Mft references do not match. */
			/* Do we want the base record back? */
			if (MREF_LE(al_entry->mft_reference) ==
					base_ni->mft_no) {
				ni = ctx->ntfs_ino = base_ni;
				ctx->mrec = ctx->base_mrec;
			} else {
				/* We want an extent record. */
				ni = ntfs_extent_inode_open(base_ni,
						al_entry->mft_reference);
				if (!ni) {
					ntfs_log_perror("Failed to map extent "
							"inode");
					break;
				}
				ctx->ntfs_ino = ni;
				ctx->mrec = ni->mrec;
			}
		}
		a = ctx->attr = (ATTR_RECORD*)((char*)ctx->mrec +
				le16_to_cpu(ctx->mrec->attrs_offset));
		/*
		 * ctx->ntfs_ino, ctx->mrec, and ctx->attr now point to the
		 * mft record containing the attribute represented by the
		 * current al_entry.
		 *
		 * We could call into ntfs_attr_find() to find the right
		 * attribute in this mft record but this would be less
		 * efficient and not quite accurate as ntfs_attr_find() ignores
		 * the attribute instance numbers for example which become
		 * important when one plays with attribute lists. Also, because
		 * a proper match has been found in the attribute list entry
		 * above, the comparison can now be optimized. So it is worth
		 * re-implementing a simplified ntfs_attr_find() here.
		 *
		 * Use a manual loop so we can still use break and continue
		 * with the same meanings as above.
		 */
do_next_attr_loop:
		if ((char*)a < (char*)ctx->mrec || (char*)a > (char*)ctx->mrec +
				le32_to_cpu(ctx->mrec->bytes_allocated))
			break;
		if (a->type == AT_END)
			continue;
		if (!a->length)
			break;
		if (al_entry->instance != a->instance)
			goto do_next_attr;
		/*
		 * If the type and/or the name are/is mismatched between the
		 * attribute list entry and the attribute record, there is
		 * corruption so we break and return error EIO.
		 */
		if (al_entry->type != a->type)
			break;
		if (!ntfs_names_are_equal((ntfschar*)((char*)a +
				le16_to_cpu(a->name_offset)),
				a->name_length, al_name,
				al_name_len, CASE_SENSITIVE,
				vol->upcase, vol->upcase_len))
			break;
		ctx->attr = a;
		/*
		 * If no @val specified or @val specified and it matches, we
		 * have found it! Also, if !@type, it is an enumeration, so we
		 * want the current attribute.
		 */
		if ((type == AT_UNUSED) || !val || (!a->non_resident &&
				le32_to_cpu(a->value_length) == val_len &&
				!memcmp((char*)a + le16_to_cpu(a->value_offset),
				val, val_len))) {
			return 0;
		}
do_next_attr:
		/* Proceed to the next attribute in the current mft record. */
		a = (ATTR_RECORD*)((char*)a + le32_to_cpu(a->length));
		goto do_next_attr_loop;
	}
	if (ni != base_ni) {
		ctx->ntfs_ino = base_ni;
		ctx->mrec = ctx->base_mrec;
		ctx->attr = ctx->base_attr;
	}
	ntfs_log_debug("Inode is corrupt.\n");
	errno = EIO;
	return -1;
not_found:
	/*
	 * If we were looking for AT_END or we were enumerating and reached the
	 * end, we reset the search context @ctx and use ntfs_attr_find() to
	 * seek to the end of the base mft record.
	 */
	if (type == AT_UNUSED || type == AT_END) {
		ntfs_attr_reinit_search_ctx(ctx);
		return ntfs_attr_find(AT_END, name, name_len, ic, val, val_len,
				ctx);
	}
	/*
	 * The attribute wasn't found.  Before we return, we want to ensure
	 * @ctx->mrec and @ctx->attr indicate the position at which the
	 * attribute should be inserted in the base mft record.  Since we also
	 * want to preserve @ctx->al_entry we cannot reinitialize the search
	 * context using ntfs_attr_reinit_search_ctx() as this would set
	 * @ctx->al_entry to NULL.  Thus we do the necessary bits manually (see
	 * ntfs_attr_init_search_ctx() below).  Note, we _only_ preserve
	 * @ctx->al_entry as the remaining fields (base_*) are identical to
	 * their non base_ counterparts and we cannot set @ctx->base_attr
	 * correctly yet as we do not know what @ctx->attr will be set to by
	 * the call to ntfs_attr_find() below.
	 */
	ctx->mrec = ctx->base_mrec;
	ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
			le16_to_cpu(ctx->mrec->attrs_offset));
	ctx->is_first = TRUE;
	ctx->ntfs_ino = ctx->base_ntfs_ino;
	ctx->base_ntfs_ino = NULL;
	ctx->base_mrec = NULL;
	ctx->base_attr = NULL;
	/*
	 * In case there are multiple matches in the base mft record, need to
	 * keep enumerating until we get an attribute not found response (or
	 * another error), otherwise we would keep returning the same attribute
	 * over and over again and all programs using us for enumeration would
	 * lock up in a tight loop.
	 */
	{
		int ret;

		do {
			ret = ntfs_attr_find(type, name, name_len, ic, val,
					val_len, ctx);
		} while (!ret);
		return ret;
	}
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
 *
 * The following error codes are defined:
 *	ENOENT	Attribute not found, not an error as such.
 *	EINVAL	Invalid arguments.
 *	EIO	I/O error or corrupt data structures found.
 *	ENOMEM	Not enough memory to allocate necessary buffers.
 */
int ntfs_attr_lookup(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx)
{
	ntfs_volume *vol;
	ntfs_inode *base_ni;

	if (!ctx || !ctx->mrec || !ctx->attr || (name && name != AT_UNNAMED &&
			(!ctx->ntfs_ino || !(vol = ctx->ntfs_ino->vol) ||
			!vol->upcase || !vol->upcase_len))) {
		errno = EINVAL;
		return -1;
	}
	if (ctx->base_ntfs_ino)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;
	if (!base_ni || !NInoAttrList(base_ni) || type == AT_ATTRIBUTE_LIST)
		return ntfs_attr_find(type, name, name_len, ic, val, val_len,
				ctx);
	return ntfs_external_attr_find(type, name, name_len, ic, lowest_vcn,
			val, val_len, ctx);
}

/**
 * ntfs_attr_init_search_ctx - initialize an attribute search context
 * @ctx:	attribute search context to initialize
 * @ni:		ntfs inode with which to initialize the search context
 * @mrec:	mft record with which to initialize the search context
 *
 * Initialize the attribute search context @ctx with @ni and @mrec.
 */
static void ntfs_attr_init_search_ctx(ntfs_attr_search_ctx *ctx,
		ntfs_inode *ni, MFT_RECORD *mrec)
{
	if (!mrec)
		mrec = ni->mrec;
	ctx->mrec = mrec;
	/* Sanity checks are performed elsewhere. */
	ctx->attr = (ATTR_RECORD*)((u8*)mrec + le16_to_cpu(mrec->attrs_offset));
	ctx->is_first = TRUE;
	ctx->ntfs_ino = ni;
	ctx->al_entry = NULL;
	ctx->base_ntfs_ino = NULL;
	ctx->base_mrec = NULL;
	ctx->base_attr = NULL;
}

/**
 * ntfs_attr_reinit_search_ctx - reinitialize an attribute search context
 * @ctx:	attribute search context to reinitialize
 *
 * Reinitialize the attribute search context @ctx.
 *
 * This is used when a search for a new attribute is being started to reset
 * the search context to the beginning.
 */
void ntfs_attr_reinit_search_ctx(ntfs_attr_search_ctx *ctx)
{
	if (!ctx->base_ntfs_ino) {
		/* No attribute list. */
		ctx->is_first = TRUE;
		/* Sanity checks are performed elsewhere. */
		ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
				le16_to_cpu(ctx->mrec->attrs_offset));
		/*
		 * This needs resetting due to ntfs_external_attr_find() which
		 * can leave it set despite having zeroed ctx->base_ntfs_ino.
		 */
		ctx->al_entry = NULL;
		return;
	} /* Attribute list. */
	ntfs_attr_init_search_ctx(ctx, ctx->base_ntfs_ino, ctx->base_mrec);
	return;
}

/**
 * ntfs_attr_get_search_ctx - allocate/initialize a new attribute search context
 * @ni:		ntfs inode with which to initialize the search context
 * @mrec:	mft record with which to initialize the search context
 *
 * Allocate a new attribute search context, initialize it with @ni and @mrec,
 * and return it. Return NULL on error with errno set to ENOMEM.
 *
 * @mrec can be NULL, in which case the mft record is taken from @ni.
 *
 * Note: For low level utilities which know what they are doing we allow @ni to
 * be NULL and @mrec to be set.  Do NOT do this unless you understand the
 * implications!!!  For example it is no longer safe to call ntfs_attr_lookup()
 * if you
 */
ntfs_attr_search_ctx *ntfs_attr_get_search_ctx(ntfs_inode *ni, MFT_RECORD *mrec)
{
	ntfs_attr_search_ctx *ctx;

	if (!ni && !mrec) {
		errno = EINVAL;
		return NULL;
	}
	ctx = ntfs_malloc(sizeof(ntfs_attr_search_ctx));
	if (ctx)
		ntfs_attr_init_search_ctx(ctx, ni, mrec);
	return ctx;
}

/**
 * ntfs_attr_put_search_ctx - release an attribute search context
 * @ctx:	attribute search context to free
 *
 * Release the attribute search context @ctx. This function does not change
 * errno and doing nothing if NULL passed to it.
 */
void ntfs_attr_put_search_ctx(ntfs_attr_search_ctx *ctx)
{
	free(ctx);
}

/**
 * ntfs_attr_find_in_attrdef - find an attribute in the $AttrDef system file
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to find
 *
 * Search for the attribute definition record corresponding to the attribute
 * @type in the $AttrDef system file.
 *
 * Return the attribute type definition record if found and NULL if not found
 * or an error occurred. On error the error code is stored in errno. The
 * following error codes are defined:
 *	ENOENT	- The attribute @type is not specified in $AttrDef.
 *	EINVAL	- Invalid parameters (e.g. @vol is not valid).
 */
ATTR_DEF *ntfs_attr_find_in_attrdef(const ntfs_volume *vol,
		const ATTR_TYPES type)
{
	ATTR_DEF *ad;

	if (!vol || !vol->attrdef || !type) {
		errno = EINVAL;
		return NULL;
	}
	for (ad = vol->attrdef; (u8*)ad - (u8*)vol->attrdef <
			vol->attrdef_len && ad->type; ++ad) {
		/* We haven't found it yet, carry on searching. */
		if (le32_to_cpu(ad->type) < le32_to_cpu(type))
			continue;
		/* We found the attribute; return it. */
		if (ad->type == type)
			return ad;
		/* We have gone too far already. No point in continuing. */
		break;
	}
	/* Attribute not found?!? */
	errno = ENOENT;
	return NULL;
}

/**
 * ntfs_attr_size_bounds_check - check a size of an attribute type for validity
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 * @size:	size which to check
 *
 * Check whether the @size in bytes is valid for an attribute of @type on the
 * ntfs volume @vol. This information is obtained from $AttrDef system file.
 *
 * Return 0 if valid and -1 if not valid or an error occurred. On error the
 * error code is stored in errno. The following error codes are defined:
 *	ERANGE	- @size is not valid for the attribute @type.
 *	ENOENT	- The attribute @type is not specified in $AttrDef.
 *	EINVAL	- Invalid parameters (e.g. @size is < 0 or @vol is not valid).
 */
int ntfs_attr_size_bounds_check(const ntfs_volume *vol, const ATTR_TYPES type,
		const s64 size)
{
	ATTR_DEF *ad;

	if (size < 0) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * $ATTRIBUTE_LIST should be not greater than 0x40000, but this is not
	 * listed in the AttrDef.
	 */
	if (type == AT_ATTRIBUTE_LIST && size > 0x40000) {
		errno = ERANGE;
		return -1;
	}

	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (!ad)
		return -1;
	/* We found the attribute. - Do the bounds check. */
	if ((sle64_to_cpu(ad->min_size) && size <
			sle64_to_cpu(ad->min_size)) ||
			((sle64_to_cpu(ad->max_size) > 0) && size >
			sle64_to_cpu(ad->max_size))) {
		/* @size is out of range! */
		errno = ERANGE;
		return -1;
	}
	return 0;
}

/**
 * ntfs_attr_can_be_non_resident - check if an attribute can be non-resident
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 *
 * Check whether the attribute of @type on the ntfs volume @vol is allowed to
 * be non-resident. This information is obtained from $AttrDef system file.
 *
 * Return 0 if the attribute is allowed to be non-resident and -1 if not or an
 * error occurred. On error the error code is stored in errno. The following
 * error codes are defined:
 *	EPERM	- The attribute is not allowed to be non-resident.
 *	ENOENT	- The attribute @type is not specified in $AttrDef.
 *	EINVAL	- Invalid parameters (e.g. @vol is not valid).
 */
int ntfs_attr_can_be_non_resident(const ntfs_volume *vol, const ATTR_TYPES type)
{
	ATTR_DEF *ad;

	/* Find the attribute definition record in $AttrDef. */
	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (!ad)
		return -1;
	/* Check the flags and return the result. */
	if (ad->flags & ATTR_DEF_RESIDENT) {
		ntfs_log_trace("Attribute can't be non-resident\n");
		errno = EPERM;
		return -1;
	}
	return 0;
}

/**
 * ntfs_attr_can_be_resident - check if an attribute can be resident
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 *
 * Check whether the attribute of @type on the ntfs volume @vol is allowed to
 * be resident. This information is derived from our ntfs knowledge and may
 * not be completely accurate, especially when user defined attributes are
 * present. Basically we allow everything to be resident except for index
 * allocation and extended attribute attributes.
 *
 * Return 0 if the attribute is allowed to be resident and -1 if not or an
 * error occurred. On error the error code is stored in errno. The following
 * error codes are defined:
 *	EPERM	- The attribute is not allowed to be resident.
 *	EINVAL	- Invalid parameters (e.g. @vol is not valid).
 *
 * Warning: In the system file $MFT the attribute $Bitmap must be non-resident
 *	    otherwise windows will not boot (blue screen of death)!  We cannot
 *	    check for this here as we don't know which inode's $Bitmap is being
 *	    asked about so the caller needs to special case this.
 */
int ntfs_attr_can_be_resident(const ntfs_volume *vol, const ATTR_TYPES type)
{
	if (!vol || !vol->attrdef || !type) {
		errno = EINVAL;
		return -1;
	}
	if (type != AT_INDEX_ALLOCATION)
		return 0;

	ntfs_log_trace("Attribute can't be resident\n");
	errno = EPERM;
	return -1;
}

/**
 * ntfs_make_room_for_attr - make room for an attribute inside an mft record
 * @m:		mft record
 * @pos:	position at which to make space
 * @size:	byte size to make available at this position
 *
 * @pos points to the attribute in front of which we want to make space.
 *
 * Return 0 on success or -1 on error. On error the error code is stored in
 * errno. Possible error codes are:
 *	ENOSPC	- There is not enough space available to complete operation. The
 *		  caller has to make space before calling this.
 *	EINVAL	- Input parameters were faulty.
 */
int ntfs_make_room_for_attr(MFT_RECORD *m, u8 *pos, u32 size)
{
	u32 biu;

	ntfs_log_trace("Entering for pos 0x%d, size %u.\n",
		(int)(pos - (u8*)m), (unsigned) size);

	/* Make size 8-byte alignment. */
	size = (size + 7) & ~7;

	/* Rigorous consistency checks. */
	if (!m || !pos || pos < (u8*)m || pos + size >
			(u8*)m + le32_to_cpu(m->bytes_allocated)) {
		errno = EINVAL;
		return -1;
	}
	/* The -8 is for the attribute terminator. */
	if (pos - (u8*)m > (int)le32_to_cpu(m->bytes_in_use) - 8) {
		errno = EINVAL;
		return -1;
	}
	/* Nothing to do. */
	if (!size)
		return 0;

	biu = le32_to_cpu(m->bytes_in_use);
	/* Do we have enough space? */
	if (biu + size > le32_to_cpu(m->bytes_allocated)) {
		ntfs_log_trace("Not enough space in the MFT record\n");
		errno = ENOSPC;
		return -1;
	}
	/* Move everything after pos to pos + size. */
	memmove(pos + size, pos, biu - (pos - (u8*)m));
	/* Update mft record. */
	m->bytes_in_use = cpu_to_le32(biu + size);
	return 0;
}

/**
 * ntfs_resident_attr_record_add - add resident attribute to inode
 * @ni:		opened ntfs inode to which MFT record add attribute
 * @type:	type of the new attribute
 * @name:	name of the new attribute
 * @name_len:	name length of the new attribute
 * @val:	value of the new attribute
 * @size:	size of new attribute (length of @val, if @val != NULL)
 * @flags:	flags of the new attribute
 *
 * Return offset to attribute from the beginning of the mft record on success
 * and -1 on error. On error the error code is stored in errno.
 * Possible error codes are:
 *	EINVAL	- Invalid arguments passed to function.
 *	EEXIST	- Attribute of such type and with same name already exists.
 *	EIO	- I/O error occurred or damaged filesystem.
 */
int ntfs_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
			ntfschar *name, u8 name_len, u8 *val, u32 size,
			ATTR_FLAGS flags)
{
	ntfs_attr_search_ctx *ctx;
	u32 length;
	ATTR_RECORD *a;
	MFT_RECORD *m;
	int err, offset;
	ntfs_inode *base_ni;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, flags 0x%x.\n",
		(long long) ni->mft_no, le32_to_cpu(type), le16_to_cpu(flags));

	if (!ni || (!name && name_len)) {
		errno = EINVAL;
		return -1;
	}

	if (ntfs_attr_can_be_resident(ni->vol, type)) {
		if (errno == EPERM)
			ntfs_log_trace("Attribute can't be resident.\n");
		else
			ntfs_log_trace("ntfs_attr_can_be_resident failed.\n");
		return -1;
	}

	/* Locate place where record should be. */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;
	/*
	 * Use ntfs_attr_find instead of ntfs_attr_lookup to find place for
	 * attribute in @ni->mrec, not any extent inode in case if @ni is base
	 * file record.
	 */
	if (!ntfs_attr_find(type, name, name_len, CASE_SENSITIVE, val, size,
			ctx)) {
		err = EEXIST;
		ntfs_log_trace("Attribute already present.\n");
		goto put_err_out;
	}
	if (errno != ENOENT) {
		err = EIO;
		goto put_err_out;
	}
	a = ctx->attr;
	m = ctx->mrec;

	/* Make room for attribute. */
	length = offsetof(ATTR_RECORD, resident_end) +
				((name_len * sizeof(ntfschar) + 7) & ~7) +
				((size + 7) & ~7);
	if (ntfs_make_room_for_attr(ctx->mrec, (u8*) ctx->attr, length)) {
		err = errno;
		ntfs_log_trace("Failed to make room for attribute.\n");
		goto put_err_out;
	}

	/* Setup record fields. */
	offset = ((u8*)a - (u8*)m);
	a->type = type;
	a->length = cpu_to_le32(length);
	a->non_resident = 0;
	a->name_length = name_len;
	a->name_offset = cpu_to_le16(offsetof(ATTR_RECORD, resident_end));
	a->flags = flags;
	a->instance = m->next_attr_instance;
	a->value_length = cpu_to_le32(size);
	a->value_offset = cpu_to_le16(length - ((size + 7) & ~7));
	if (val)
		memcpy((u8*)a + le16_to_cpu(a->value_offset), val, size);
	else
		memset((u8*)a + le16_to_cpu(a->value_offset), 0, size);
	if (type == AT_FILE_NAME)
		a->resident_flags = RESIDENT_ATTR_IS_INDEXED;
	else
		a->resident_flags = 0;
	if (name_len)
		memcpy((u8*)a + le16_to_cpu(a->name_offset),
			name, sizeof(ntfschar) * name_len);
	m->next_attr_instance =
		cpu_to_le16((le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	if (ni->nr_extents == -1)
		base_ni = ni->base_ni;
	else
		base_ni = ni;
	if (type != AT_ATTRIBUTE_LIST && NInoAttrList(base_ni)) {
		if (ntfs_attrlist_entry_add(ni, a)) {
			err = errno;
			ntfs_attr_record_resize(m, a, 0);
			ntfs_log_trace("Failed add attribute entry to "
					"ATTRIBUTE_LIST.\n");
			goto put_err_out;
		}
	}
	ntfs_inode_mark_dirty(ni);
	ntfs_attr_put_search_ctx(ctx);
	return offset;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}

/**
 * ntfs_non_resident_attr_record_add - add extent of non-resident attribute
 * @ni:			opened ntfs inode to which MFT record add attribute
 * @type:		type of the new attribute extent
 * @name:		name of the new attribute extent
 * @name_len:		name length of the new attribute extent
 * @lowest_vcn:		lowest vcn of the new attribute extent
 * @dataruns_size:	dataruns size of the new attribute extent
 * @flags:		flags of the new attribute extent
 *
 * Return offset to attribute from the beginning of the mft record on success
 * and -1 on error. On error the error code is stored in errno.
 * Possible error codes are:
 *	EINVAL	- Invalid arguments passed to function.
 *	EEXIST	- Attribute of such type, with same lowest vcn and with same
 *		  name already exists.
 *	EIO	- I/O error occurred or damaged filesystem.
 */
int ntfs_non_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, VCN lowest_vcn, int dataruns_size,
		ATTR_FLAGS flags)
{
	ntfs_attr_search_ctx *ctx;
	u32 length;
	ATTR_RECORD *a;
	MFT_RECORD *m;
	ntfs_inode *base_ni;
	int err, offset;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, lowest_vcn %lld, "
			"dataruns_size %d, flags 0x%x.\n",
			(long long) ni->mft_no, le32_to_cpu(type),
			(long long) lowest_vcn, dataruns_size,
			le16_to_cpu(flags));

	if (!ni || dataruns_size <= 0 || (!name && name_len)) {
		errno = EINVAL;
		return -1;
	}

	if (ntfs_attr_can_be_non_resident(ni->vol, type)) {
		if (errno == EPERM)
			ntfs_log_trace("Attribute can't be non resident.\n");
		else
			ntfs_log_trace("ntfs_attr_can_be_non_resident() "
					"failed.\n");
		return -1;
	}

	/* Locate place where record should be. */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;
	/*
	 * Use ntfs_attr_find instead of ntfs_attr_lookup to find place for
	 * attribute in @ni->mrec, not any extent inode in case if @ni is base
	 * file record.
	 */
	if (!ntfs_attr_find(type, name, name_len, CASE_SENSITIVE, NULL, 0,
			ctx)) {
		err = EEXIST;
		ntfs_log_trace("Attribute already present.\n");
		goto put_err_out;
	}
	if (errno != ENOENT) {
		err = EIO;
		goto put_err_out;
	}
	a = ctx->attr;
	m = ctx->mrec;

	/* Make room for attribute. */
	dataruns_size = (dataruns_size + 7) & ~7;
	length = offsetof(ATTR_RECORD, compressed_size) + ((sizeof(ntfschar) *
			name_len + 7) & ~7) + dataruns_size +
			((flags & (ATTR_IS_COMPRESSED | ATTR_IS_SPARSE)) ?
			sizeof(a->compressed_size) : 0);
	if (ntfs_make_room_for_attr(ctx->mrec, (u8*) ctx->attr, length)) {
		err = errno;
		ntfs_log_trace("Failed to make room for attribute.\n");
		goto put_err_out;
	}

	/* Setup record fields. */
	a->type = type;
	a->length = cpu_to_le32(length);
	a->non_resident = 1;
	a->name_length = name_len;
	a->name_offset = cpu_to_le16(offsetof(ATTR_RECORD, compressed_size) +
			((flags & (ATTR_IS_COMPRESSED | ATTR_IS_SPARSE)) ?
			sizeof(a->compressed_size) : 0));
	a->flags = flags;
	a->instance = m->next_attr_instance;
	a->lowest_vcn = cpu_to_sle64(lowest_vcn);
	a->mapping_pairs_offset = cpu_to_le16(length - dataruns_size);
	a->compression_unit = (flags & ATTR_IS_COMPRESSED) ? 4 : 0;
	/* If @lowest_vcn == 0, than setup empty attribute. */
	if (!lowest_vcn) {
		a->highest_vcn = cpu_to_sle64(-1);
		a->allocated_size = 0;
		a->data_size = 0;
		a->initialized_size = 0;
		/* Set empty mapping pairs. */
		*((u8*)a + le16_to_cpu(a->mapping_pairs_offset)) = 0;
	}
	if (name_len)
		memcpy((u8*)a + le16_to_cpu(a->name_offset),
			name, sizeof(ntfschar) * name_len);
	m->next_attr_instance =
		cpu_to_le16((le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	if (ni->nr_extents == -1)
		base_ni = ni->base_ni;
	else
		base_ni = ni;
	if (type != AT_ATTRIBUTE_LIST && NInoAttrList(base_ni)) {
		if (ntfs_attrlist_entry_add(ni, a)) {
			err = errno;
			ntfs_attr_record_resize(m, a, 0);
			ntfs_log_trace("Failed add attribute entry to "
					"ATTRIBUTE_LIST.\n");
			goto put_err_out;
		}
	}
	ntfs_inode_mark_dirty(ni);
	/*
	 * Locate offset from start of the MFT record where new attribute is
	 * placed. We need relookup it, because record maybe moved during
	 * update of attribute list.
	 */
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(type, name, name_len, CASE_SENSITIVE,
					lowest_vcn, NULL, 0, ctx)) {
		err = errno;
		ntfs_log_trace("Attribute lookup failed. Probably leaving "
				"inconsistent metadata.\n");
		ntfs_attr_put_search_ctx(ctx);
		errno = err;
		return -1;
	}
	offset = (u8*)ctx->attr - (u8*)ctx->mrec;
	ntfs_attr_put_search_ctx(ctx);
	return offset;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}

/**
 * ntfs_attr_record_rm - remove attribute extent
 * @ctx:	search context describing the attribute which should be removed
 *
 * If this function succeed, user should reinit search context if he/she wants
 * use it anymore.
 *
 * Return 0 on success and -1 on error. On error the error code is stored in
 * errno. Possible error codes are:
 *	EINVAL	- Invalid arguments passed to function.
 *	EIO	- I/O error occurred or damaged filesystem.
 */
int ntfs_attr_record_rm(ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni;
	ATTR_TYPES type;
	int err;

	if (!ctx || !ctx->ntfs_ino || !ctx->mrec || !ctx->attr) {
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n",
			(long long) ctx->ntfs_ino->mft_no,
			(unsigned) le32_to_cpu(ctx->attr->type));
	type = ctx->attr->type;
	ni = ctx->ntfs_ino;
	if (ctx->base_ntfs_ino)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;

	/* Remove attribute itself. */
	if (ntfs_attr_record_resize(ctx->mrec, ctx->attr, 0)) {
		ntfs_log_trace("Couldn't remove attribute record. "
				"Bug or damaged MFT record.\n");
		if (NInoAttrList(base_ni) && type != AT_ATTRIBUTE_LIST)
			if (ntfs_attrlist_entry_add(ni, ctx->attr))
				ntfs_log_trace("Rollback failed. Leaving "
						"inconsistent metadata.\n");
		err = EIO;
		return -1;
	}
	ntfs_inode_mark_dirty(ni);

	/*
	 * Remove record from $ATTRIBUTE_LIST if present and we don't want
	 * delete $ATTRIBUTE_LIST itself.
	 */
	if (NInoAttrList(base_ni) && type != AT_ATTRIBUTE_LIST) {
		if (ntfs_attrlist_entry_rm(ctx)) {
			ntfs_log_trace("Couldn't delete record from "
					"$ATTRIBUTE_LIST.\n");
			return -1;
		}
	}

	/* Post $ATTRIBUTE_LIST delete setup. */
	if (type == AT_ATTRIBUTE_LIST) {
		if (NInoAttrList(base_ni) && base_ni->attr_list)
			free(base_ni->attr_list);
		base_ni->attr_list = NULL;
		NInoClearAttrList(base_ni);
		NInoAttrListClearDirty(base_ni);
	}

	/* Free MFT record, if it isn't contain attributes. */
	if (le32_to_cpu(ctx->mrec->bytes_in_use) -
			le16_to_cpu(ctx->mrec->attrs_offset) == 8) {
		if (ntfs_mft_record_free(ni->vol, ni)) {
			// FIXME: We need rollback here.
			ntfs_log_trace("Couldn't free MFT record.\n");
			errno = EIO;
			return -1;
		}
		/* Remove done if we freed base inode. */
		if (ni == base_ni)
			return 0;
	}

	if (type == AT_ATTRIBUTE_LIST || !NInoAttrList(base_ni))
		return 0;

	/* Remove attribute list if we don't need it any more. */
	if (!ntfs_attrlist_need(base_ni)) {
		ntfs_attr_reinit_search_ctx(ctx);
		if (ntfs_attr_lookup(AT_ATTRIBUTE_LIST, NULL, 0, CASE_SENSITIVE,
				0, NULL, 0, ctx)) {
			/*
			 * FIXME: Should we succeed here? Definitely something
			 * goes wrong because NInoAttrList(base_ni) returned
			 * that we have got attribute list.
			 */
			ntfs_log_trace("Couldn't find attribute list. Succeed "
					"anyway.\n");
			return 0;
		}
		/* Deallocate clusters. */
		if (ctx->attr->non_resident) {
			runlist *al_rl;

			al_rl = ntfs_mapping_pairs_decompress(base_ni->vol,
					ctx->attr, NULL);
			if (!al_rl) {
				ntfs_log_trace("Couldn't decompress attribute "
						"list runlist. Succeed "
						"anyway.\n");
				return 0;
			}
			if (ntfs_cluster_free_from_rl(base_ni->vol, al_rl)) {
				ntfs_log_trace("Leaking clusters! Run chkdsk. "
						"Couldn't free clusters from "
						"attribute list runlist.\n");
			}
			free(al_rl);
		}
		/* Remove attribute record itself. */
		if (ntfs_attr_record_rm(ctx)) {
			/*
			 * FIXME: Should we succeed here? BTW, chkdsk doesn't
			 * complain if it find MFT record with attribute list,
			 * but without extents.
			 */
			ntfs_log_trace("Couldn't remove attribute list. "
					"Succeed anyway.\n");
			return 0;
		}
	}
	return 0;
}

/**
 * ntfs_attr_add - add attribute to inode
 * @ni:		opened ntfs inode to which add attribute
 * @type:	type of the new attribute
 * @name:	name in unicode of the new attribute
 * @name_len:	name length in unicode characters of the new attribute
 * @val:	value of new attribute
 * @size:	size of the new attribute / length of @val (if specified)
 *
 * @val should always be specified for always resident attributes (eg. FILE_NAME
 * attribute), for attributes that can become non-resident @val can be NULL
 * (eg. DATA attribute). @size can be specified even if @val is NULL, in this
 * case data size will be equal to @size and initialized size will be equal
 * to 0.
 *
 * If inode haven't got enough space to add attribute, add attribute to one of
 * it extents, if no extents present or no one of them have enough space, than
 * allocate new extent and add attribute to it.
 *
 * If on one of this steps attribute list is needed but not present, than it is
 * added transparently to caller. So, this function should not be called with
 * @type == AT_ATTRIBUTE_LIST, if you really need to add attribute list call
 * ntfs_inode_add_attrlist instead.
 *
 * On success return 0. On error return -1 with errno set to the error code.
 */
int ntfs_attr_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, u8 *val, s64 size)
{
	u32 attr_rec_size;
	int err, i, offset;
	BOOL is_resident = TRUE;
	BOOL always_non_resident = FALSE, always_resident = FALSE;
	ntfs_inode *attr_ni;
	ntfs_attr *na;

	if (!ni || size < 0 || type == AT_ATTRIBUTE_LIST) {
		ntfs_log_trace("Invalid arguments passed.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr %x, size %lld.\n",
			(long long) ni->mft_no, type, size);

	if (ni->nr_extents == -1)
		ni = ni->base_ni;

	/* Check the attribute type and the size. */
	if (ntfs_attr_size_bounds_check(ni->vol, type, size)) {
		if (errno == ERANGE) {
			ntfs_log_trace("Size bounds check failed.\n");
		} else if (errno == ENOENT) {
			ntfs_log_trace("Invalid attribute type. Aborting...\n");
			errno = EIO;
		}
		return -1;
	}

	/* Sanity checks for always resident attributes. */
	if (ntfs_attr_can_be_non_resident(ni->vol, type)) {
		if (errno != EPERM) {
			err = errno;
			ntfs_log_trace("ntfs_attr_can_be_non_resident() "
					"failed.\n");
			goto err_out;
		}
		/* @val is mandatory. */
		if (!val) {
			ntfs_log_trace("@val is mandatory for always resident "
					"attributes.\n");
			errno = EINVAL;
			return -1;
		}
		if (size > ni->vol->mft_record_size) {
			ntfs_log_trace("Attribute is too big.\n");
			errno = ERANGE;
			return -1;
		}
		always_resident = TRUE;
	}

	/* Check whether attribute can be resident. */
	if (ntfs_attr_can_be_resident(ni->vol, type)) {
		if (errno != EPERM) {
			err = errno;
			ntfs_log_trace("ntfs_attr_can_be_resident() failed.\n");
			goto err_out;
		}
		is_resident = FALSE;
		always_non_resident = TRUE;
	}

retry:
	/* Calculate attribute record size. */
	if (is_resident)
		attr_rec_size = offsetof(ATTR_RECORD, resident_end) +
				ROUND_UP(name_len * sizeof(ntfschar), 3) +
				ROUND_UP(size, 3);
	else /* We add 8 for space for mapping pairs. */
		attr_rec_size = offsetof(ATTR_RECORD, non_resident_end) +
				ROUND_UP(name_len * sizeof(ntfschar), 3) + 8;

	/*
	 * If we have enough free space for the new attribute in the base MFT
	 * record, then add attribute to it.
	 */
	if (le32_to_cpu(ni->mrec->bytes_allocated) -
			le32_to_cpu(ni->mrec->bytes_in_use) >= attr_rec_size) {
		attr_ni = ni;
		goto add_attr_record;
	}

	/* Try to add to extent inodes. */
	if (ntfs_inode_attach_all_extents(ni)) {
		err = errno;
		ntfs_log_trace("Failed to attach all extents to inode.\n");
		goto err_out;
	}
	for (i = 0; i < ni->nr_extents; i++) {
		attr_ni = ni->extent_nis[i];
		if (le32_to_cpu(attr_ni->mrec->bytes_allocated) -
				le32_to_cpu(attr_ni->mrec->bytes_in_use) >=
				attr_rec_size)
			goto add_attr_record;
	}

	/* 
	 * If failed to find space for resident attribute, then try to find
	 * space for non resident one.
	 */
	if (is_resident && !always_resident) {
		is_resident = FALSE;
		goto retry;
	}

	/*
	 * FIXME: Try to make other attributes non-resident here. Factor out
	 * code from ntfs_resident_attr_resize.
	 */

	/* There is no extent that contain enough space for new attribute. */
	if (!NInoAttrList(ni)) {
		/* Add attribute list not present, add it and retry. */
		if (ntfs_inode_add_attrlist(ni)) {
			err = errno;
			ntfs_log_trace("Failed to add attribute list.\n");
			goto err_out;
		}
		return ntfs_attr_add(ni, type, name, name_len, val, size);
	}
	/* Allocate new extent for attribute. */
	attr_ni = ntfs_mft_record_alloc(ni->vol, ni);
	if (!attr_ni) {
		err = errno;
		ntfs_log_trace("Failed to allocate extent record.\n");
		goto err_out;
	}

	/*
	 * Determine resident or not will be attribute using heuristics and
	 * calculate attribute record size. FIXME: small code duplication here.
	 */
	if (always_resident || (!always_non_resident && size < 256)) {
		is_resident = TRUE;
		attr_rec_size = offsetof(ATTR_RECORD, resident_end) +
				ROUND_UP(name_len * sizeof(ntfschar), 3) +
				ROUND_UP(size, 3);
	} else { /* We add 8 for space for mapping pairs. */
		is_resident = FALSE;
		attr_rec_size = offsetof(ATTR_RECORD, non_resident_end) +
				ROUND_UP(name_len * sizeof(ntfschar), 3) + 8;
	}

add_attr_record:
	if (is_resident) {
		/* Add resident attribute. */
		offset = ntfs_resident_attr_record_add(attr_ni, type, name,
				name_len, val, size, 0);
		if (offset < 0) {
			err = errno;
			ntfs_log_trace("Failed to add resident attribute.\n");
			goto free_err_out;
		}
		return 0;
	}

	/* Add non resident attribute. */
	offset = ntfs_non_resident_attr_record_add(attr_ni, type, name,
				name_len, 0, 8, 0);
	if (offset < 0) {
		err = errno;
		ntfs_log_trace("Failed to add non resident attribute.\n");
		goto free_err_out;
	}

	/* If @size == 0, we are done. */
	if (!size)
		return 0;

	/* Open new attribute and resize it. */
	na = ntfs_attr_open(ni, type, name, name_len);
	if (!na) {
		err = errno;
		ntfs_log_trace("Failed to open just added attribute.\n");
		goto rm_attr_err_out;
	}
	/* Resize and set attribute value. */
	if (ntfs_attr_truncate(na, size) ||
			(val && (ntfs_attr_pwrite(na, 0, size, val) != size))) {
		err = errno;
		ntfs_log_trace("Failed to initialize just added attribute.\n");
		if (ntfs_attr_rm(na))
			ntfs_log_trace("Failed to remove just added attribute. "
					"Probably leaving inconsistent "
					"metadata.\n");
		goto err_out;
	}
	ntfs_attr_close(na);
	/* Done !*/
	return 0;

rm_attr_err_out:
	/* Remove just added attribute. */
	if (ntfs_attr_record_resize(attr_ni->mrec,
			(ATTR_RECORD*)((u8*)attr_ni->mrec + offset), 0)) {
		ntfs_log_trace("Failed to remove just added attribute.\n");
	}
free_err_out:
	/* Free MFT record, if it isn't contain attributes. */
	if (le32_to_cpu(attr_ni->mrec->bytes_in_use) -
			le16_to_cpu(attr_ni->mrec->attrs_offset) == 8) {
		if (ntfs_mft_record_free(attr_ni->vol, attr_ni)) {
			ntfs_log_trace("Failed to free MFT record. Leaving "
					"inconsistent metadata.\n");
		}
	}
err_out:
	errno = err;
	return -1;
}

/**
 * ntfs_attr_rm - remove attribute from ntfs inode
 * @na:		opened ntfs attribute to delete
 *
 * Remove attribute and all it's extents from ntfs inode. If attribute was non
 * resident also free all clusters allocated by attribute. This function always 
 * closes @na upon exit (both on success and failure).
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
int ntfs_attr_rm(ntfs_attr *na)
{
	ntfs_attr_search_ctx *ctx;
	int ret = 0;

	if (!na) {
		ntfs_log_trace("Invalid arguments passed.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n",
		(long long) na->ni->mft_no, na->type);

	/* Free cluster allocation. */
	if (NAttrNonResident(na)) {
		if (ntfs_attr_map_whole_runlist(na)) {
			ntfs_attr_close(na);
			return -1;
		}
		if (ntfs_cluster_free(na->ni->vol, na, 0, -1) < 0) {
			ntfs_log_trace("Failed to free cluster allocation. "
					"Leaving inconsistent metadata.\n");
			ret = -1;
		}
	}

	/* Search for attribute extents and remove them all. */
	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx) {
		ntfs_attr_close(na);
		return -1;
	}
	while (!ntfs_attr_lookup(na->type, na->name, na->name_len,
				CASE_SENSITIVE, 0, NULL, 0, ctx)) {
		if (ntfs_attr_record_rm(ctx)) {
			ntfs_log_trace("Failed to remove attribute extent. "
					"Leaving inconsistent metadata.\n");
			ret = -1;
		}
		ntfs_attr_reinit_search_ctx(ctx);
	}
	if (errno != ENOENT) {
		ntfs_log_trace("Attribute lookup failed. "
				"Probably leaving inconsistent metadata.\n");
		ret = -1;
	}

	/* Throw away now non-exist attribute. */
	ntfs_attr_close(na);
	/* Done. */
	return ret;
}

/**
 * ntfs_attr_record_resize - resize an attribute record
 * @m:		mft record containing attribute record
 * @a:		attribute record to resize
 * @new_size:	new size in bytes to which to resize the attribute record @a
 *
 * Resize the attribute record @a, i.e. the resident part of the attribute, in
 * the mft record @m to @new_size bytes.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 * The following error codes are defined:
 *	ENOSPC	- Not enough space in the mft record @m to perform the resize.
 * Note that on error no modifications have been performed whatsoever.
 *
 * Warning: If you make a record smaller without having copied all the data you
 *	    are interested in the data may be overwritten!
 */
int ntfs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size)
{
	ntfs_log_trace("Entering for new_size %u.\n", (unsigned) new_size);
	/* Align to 8 bytes, just in case the caller hasn't. */
	new_size = (new_size + 7) & ~7;
	/* If the actual attribute length has changed, move things around. */
	if (new_size != le32_to_cpu(a->length)) {
		u32 new_muse = le32_to_cpu(m->bytes_in_use) -
				le32_to_cpu(a->length) + new_size;
		/* Not enough space in this mft record. */
		if (new_muse > le32_to_cpu(m->bytes_allocated)) {
			errno = ENOSPC;
			return -1;
		}
		/* Move attributes following @a to their new location. */
		memmove((u8*)a + new_size, (u8*)a + le32_to_cpu(a->length),
				le32_to_cpu(m->bytes_in_use) - ((u8*)a -
				(u8*)m) - le32_to_cpu(a->length));
		/* Adjust @m to reflect the change in used space. */
		m->bytes_in_use = cpu_to_le32(new_muse);
		/* Adjust @a to reflect the new size. */
		if (new_size >= offsetof(ATTR_REC, length) + sizeof(a->length))
			a->length = cpu_to_le32(new_size);
	}
	return 0;
}

/**
 * ntfs_resident_attr_value_resize - resize the value of a resident attribute
 * @m:		mft record containing attribute record
 * @a:		attribute record whose value to resize
 * @new_size:	new size in bytes to which to resize the attribute value of @a
 *
 * Resize the value of the attribute @a in the mft record @m to @new_size bytes.
 * If the value is made bigger, the newly "allocated" space is cleared.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 * The following error codes are defined:
 *	ENOSPC	- Not enough space in the mft record @m to perform the resize.
 * Note that on error no modifications have been performed whatsoever.
 */
int ntfs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a,
		const u32 new_size)
{
	ntfs_log_trace("Entering for new size %u.\n", (unsigned)new_size);

	/*
	 * Check that the attribute name hasn't been placed after the
	 * attribute value. Chkdsk treat this as corruption.
	 */
	if (a->name_length && le16_to_cpu(a->name_offset) >=
			le16_to_cpu(a->value_offset)) {
		ntfs_log_trace("Name is placed after the attribute value. "
				"Corrupted inode. Run chkdsk.  Aborting...\n");
		errno = EIO;
		return -1;
	}
	/* Resize the resident part of the attribute record. */
	if (ntfs_attr_record_resize(m, a, (le16_to_cpu(a->value_offset) +
			new_size + 7) & ~7) < 0) {
		if (errno != ENOSPC) {
			int eo = errno;
			ntfs_log_trace("Attribute record resize failed.  "
					"Aborting...\n");
			errno = eo;
		}
		return -1;
	}
	/*
	 * If we made the attribute value bigger, clear the area between the
	 * old size and @new_size.
	 */
	if (new_size > le32_to_cpu(a->value_length))
		memset((u8*)a + le16_to_cpu(a->value_offset) +
				le32_to_cpu(a->value_length), 0, new_size -
				le32_to_cpu(a->value_length));
	/* Finally update the length of the attribute value. */
	a->value_length = cpu_to_le32(new_size);
	return 0;
}

/**
 * ntfs_attr_record_move_to - move attribute record to target inode
 * @ctx:	attribute search context describing the attribute record
 * @ni:		opened ntfs inode to which move attribute record
 *
 * If this function succeed, user should reinit search context if he/she wants
 * use it anymore.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attr_record_move_to(ntfs_attr_search_ctx *ctx, ntfs_inode *ni)
{
	ntfs_attr_search_ctx *nctx;
	ATTR_RECORD *a;
	int err;

	if (!ctx || !ctx->attr || !ctx->ntfs_ino || !ni) {
		ntfs_log_trace("Invalid arguments passed.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for ctx->attr->type 0x%x, "
			"ctx->ntfs_ino->mft_no 0x%llx, ni->mft_no 0x%llx.\n",
			(unsigned) le32_to_cpu(ctx->attr->type),
			(long long) ctx->ntfs_ino->mft_no,
			(long long) ni->mft_no);

	if (ctx->ntfs_ino == ni)
		return 0;

	if (!ctx->al_entry) {
		ntfs_log_trace("Inode should contain attribute list to use "
				"this function.\n");
		errno = EINVAL;
		return -1;
	}

	/* Find place in MFT record where attribute will be moved. */
	a = ctx->attr;
	nctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!nctx) {
		ntfs_log_trace("Couldn't obtain search context.\n");
		return -1;
	}
	/*
	 * Use ntfs_attr_find instead of ntfs_attr_lookup to find place for
	 * attribute in @ni->mrec, not any extent inode in case if @ni is base
	 * file record.
	 */
	if (!ntfs_attr_find(a->type, (ntfschar*)((u8*)a + le16_to_cpu(
			a->name_offset)), a->name_length, CASE_SENSITIVE, NULL,
			0, nctx)) {
		ntfs_log_trace("Attribute of such type, with same name already "
				"present in this MFT record.\n");
		err = EEXIST;
		goto put_err_out;
	}
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_debug("Attribute lookup failed.\n");
		goto put_err_out;
	}

	/* Make space and move attribute. */
	if (ntfs_make_room_for_attr(ni->mrec, (u8*) nctx->attr,
					le32_to_cpu(a->length))) {
		err = errno;
		ntfs_log_trace("Couldn't make space for attribute.\n");
		goto put_err_out;
	}
	memcpy(nctx->attr, a, le32_to_cpu(a->length));
	nctx->attr->instance = nctx->mrec->next_attr_instance;
	nctx->mrec->next_attr_instance = cpu_to_le16(
		(le16_to_cpu(nctx->mrec->next_attr_instance) + 1) & 0xffff);
	ntfs_attr_record_resize(ctx->mrec, a, 0);
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_inode_mark_dirty(ni);

	/* Update attribute list. */
	ctx->al_entry->mft_reference =
		MK_LE_MREF(ni->mft_no, le16_to_cpu(ni->mrec->sequence_number));
	ctx->al_entry->instance = nctx->attr->instance;
	ntfs_attrlist_mark_dirty(ni);

	ntfs_attr_put_search_ctx(nctx);
	return 0;
put_err_out:
	ntfs_attr_put_search_ctx(nctx);
	errno = err;
	return -1;
}

/**
 * ntfs_attr_record_move_away - move away attribute record from it's mft record
 * @ctx:	attribute search context describing the attribute record
 * @extra:	minimum amount of free space in the new holder of record
 *
 * New attribute record holder must have free @extra bytes after moving
 * attribute record to it.
 *
 * If this function succeed, user should reinit search context if he/she wants
 * use it anymore.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_attr_record_move_away(ntfs_attr_search_ctx *ctx, int extra)
{
	ntfs_inode *base_ni, *ni;
	MFT_RECORD *m;
	int i;

	if (!ctx || !ctx->attr || !ctx->ntfs_ino || extra < 0) {
		ntfs_log_trace("Invalid arguments passed.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for attr 0x%x, inode 0x%llx.\n",
			(unsigned) le32_to_cpu(ctx->attr->type),
			(long long) ctx->ntfs_ino->mft_no);

	if (ctx->ntfs_ino->nr_extents == -1)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;

	if (!NInoAttrList(base_ni)) {
		ntfs_log_trace("Inode should contain attribute list to use "
				"this function.\n");
		errno = EINVAL;
		return -1;
	}

	if (ntfs_inode_attach_all_extents(ctx->ntfs_ino)) {
		ntfs_log_trace("Couldn't attach extent inode.\n");
		return -1;
	}

	/* Walk through all extents and try to move attribute to them. */
	for (i = 0; i < base_ni->nr_extents; i++) {
		ni = base_ni->extent_nis[i];
		m = ni->mrec;

		if (ctx->ntfs_ino->mft_no == ni->mft_no)
			continue;

		if (le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use) <
				le32_to_cpu(ctx->attr->length) + extra)
			continue;

		/*
		 * ntfs_attr_record_move_to can fail if extent with other lowest
		 * VCN already present in inode we trying move record to. So,
		 * do not return error.
		 */
		if (!ntfs_attr_record_move_to(ctx, ni))
			return 0;
	}

	/*
	 * Failed to move attribute to one of the current extents, so allocate
	 * new extent and move attribute to it.
	 */
	ni = ntfs_mft_record_alloc(base_ni->vol, base_ni);
	if (!ni) {
		ntfs_log_trace("Couldn't allocate new MFT record.\n");
		return -1;
	}
	if (ntfs_attr_record_move_to(ctx, ni)) {
		ntfs_log_trace("Couldn't move attribute to new MFT record.\n");
		return -1;
	}
	return 0;
}

/**
 * ntfs_attr_make_non_resident - convert a resident to a non-resident attribute
 * @na:		open ntfs attribute to make non-resident
 * @ctx:	ntfs search context describing the attribute
 *
 * Convert a resident ntfs attribute to a non-resident one.
 *
 * Return 0 on success and -1 on error with errno set to the error code. The
 * following error codes are defined:
 *	EPERM	- The attribute is not allowed to be non-resident.
 *	TODO: others...
 *
 * NOTE to self: No changes in the attribute list are required to move from
 *		 a resident to a non-resident attribute.
 *
 * Warning: We do not set the inode dirty and we do not write out anything!
 *	    We expect the caller to do this as this is a fairly low level
 *	    function and it is likely there will be further changes made.
 */
static int ntfs_attr_make_non_resident(ntfs_attr *na,
		ntfs_attr_search_ctx *ctx)
{
	s64 new_allocated_size, bw;
	ntfs_volume *vol = na->ni->vol;
	ATTR_REC *a = ctx->attr;
	runlist *rl;
	int mp_size, mp_ofs, name_ofs, arec_size;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n", (unsigned long
			long)na->ni->mft_no, na->type);

	/* Some preliminary sanity checking. */
	if (NAttrNonResident(na)) {
		ntfs_log_trace("Eeek!  Trying to make non-resident attribute "
				"non-resident.  Aborting...\n");
		errno = EINVAL;
		return -1;
	}

	/* Check that the attribute is allowed to be non-resident. */
	if (ntfs_attr_can_be_non_resident(vol, na->type))
		return -1;

	/*
	 * Check that the attribute name hasn't been placed after the
	 * attribute value. Chkdsk treat this as corruption.
	 */
	if (a->name_length && le16_to_cpu(a->name_offset) >=
			le16_to_cpu(a->value_offset)) {
		ntfs_log_trace("Name is placed after the attribute value. "
				"Corrupted inode. Run chkdsk.  Aborting...\n");
		errno = EIO;
		return -1;
	}

	new_allocated_size = (le32_to_cpu(a->value_length) + vol->cluster_size
			- 1) & ~(vol->cluster_size - 1);

	if (new_allocated_size > 0) {
		/* Start by allocating clusters to hold the attribute value. */
		rl = ntfs_cluster_alloc(vol, 0, new_allocated_size >>
				vol->cluster_size_bits, -1, DATA_ZONE);
		if (!rl) {
			if (errno != ENOSPC) {
				ntfs_log_trace("Eeek!  Failed to allocate "
					"cluster(s).  Aborting...\n");
			}
			return -1;
		}
	} else
		rl = NULL;
	/*
	 * Setup the in-memory attribute structure to be non-resident so that
	 * we can use ntfs_attr_pwrite().
	 */
	NAttrSetNonResident(na);
	na->rl = rl;
	na->allocated_size = new_allocated_size;
	na->data_size = na->initialized_size = le32_to_cpu(a->value_length);
	/*
	 * FIXME: For now just clear all of these as we don't support them when
	 * writing.
	 */
	NAttrClearCompressed(na);
	NAttrClearSparse(na);
	NAttrClearEncrypted(na);

	if (rl) {
		/* Now copy the attribute value to the allocated cluster(s). */
		bw = ntfs_attr_pwrite(na, 0, le32_to_cpu(a->value_length),
				(u8*)a + le16_to_cpu(a->value_offset));
		if (bw != le32_to_cpu(a->value_length)) {
			ntfs_log_debug("Failed to write out attribute value "
					"(bw = %lli, errno = %i).  "
					"Aborting...\n", (long long)bw, errno);
			if (bw >= 0)
				errno = EIO;
			goto cluster_free_err_out;
		}
	}
	/* Determine the size of the mapping pairs array. */
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl, 0);
	if (mp_size < 0) {
		ntfs_log_debug("Failed to get size for mapping pairs array.  "
				"Aborting...\n");
		goto cluster_free_err_out;
	}
	/* Calculate new offsets for the name and the mapping pairs array. */
	name_ofs = (sizeof(ATTR_REC) - sizeof(a->compressed_size) + 7) & ~7;
	mp_ofs = (name_ofs + a->name_length * sizeof(ntfschar) + 7) & ~7;
	/*
	 * Determine the size of the resident part of the non-resident
	 * attribute record. (Not compressed thus no compressed_size element
	 * present.)
	 */
	arec_size = (mp_ofs + mp_size + 7) & ~7;

	/* Resize the resident part of the attribute record. */
	if (ntfs_attr_record_resize(ctx->mrec, a, arec_size) < 0) {
		if (errno != ENOSPC) {
			ntfs_log_trace("Failed to resize attribute record.  "
					"Aborting...\n");
		}
		goto cluster_free_err_out;
	}

	/*
	 * Convert the resident part of the attribute record to describe a
	 * non-resident attribute.
	 */
	a->non_resident = 1;

	/* Move the attribute name if it exists and update the offset. */
	if (a->name_length)
		memmove((u8*)a + name_ofs, (u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	a->name_offset = cpu_to_le16(name_ofs);

	/* Update the flags to match the in-memory ones. */
	a->flags &= ~(ATTR_IS_SPARSE | ATTR_IS_ENCRYPTED |
			ATTR_COMPRESSION_MASK);

	/* Setup the fields specific to non-resident attributes. */
	a->lowest_vcn = cpu_to_sle64(0);
	a->highest_vcn = cpu_to_sle64((new_allocated_size - 1) >>
						vol->cluster_size_bits);

	a->mapping_pairs_offset = cpu_to_le16(mp_ofs);

	a->compression_unit = 0;

	memset(&a->reserved1, 0, sizeof(a->reserved1));

	a->allocated_size = cpu_to_sle64(new_allocated_size);
	a->data_size = a->initialized_size = cpu_to_sle64(na->data_size);

	/* Generate the mapping pairs array in the attribute record. */
	if (ntfs_mapping_pairs_build(vol, (u8*)a + mp_ofs, arec_size - mp_ofs,
			rl, 0, NULL) < 0) {
		// FIXME: Eeek! We need rollback! (AIA)
		ntfs_log_trace("Eeek!  Failed to build mapping pairs.  Leaving "
				"corrupt attribute record on disk.  In memory "
				"runlist is still intact!  Error code is %i.  "
				"FIXME:  Need to rollback instead!\n", errno);
		return -1;
	}

	/* Done! */
	return 0;

cluster_free_err_out:
	if (rl && ntfs_cluster_free(vol, na, 0, -1) < 0)
		ntfs_log_trace("Failed to release allocated clusters in error "
				"code path.  Leaving inconsistent metadata...\n");
	NAttrClearNonResident(na);
	na->allocated_size = na->data_size;
	na->rl = NULL;
	free(rl);
	return -1;
}

/**
 * ntfs_resident_attr_resize - resize a resident, open ntfs attribute
 * @na:		resident ntfs attribute to resize
 * @newsize:	new size (in bytes) to which to resize the attribute
 *
 * Change the size of a resident, open ntfs attribute @na to @newsize bytes.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 * The following error codes are defined:
 *	ENOMEM		- Not enough memory to complete operation.
 *	ERANGE		- @newsize is not valid for the attribute type of @na.
 *	ENOSPC		- There is no enough space on the volume to allocate
 *			  new clusters or in base mft to resize $ATTRIBUTE_LIST.
 *	EOVERFLOW	- Resident attribute can not become non resident and
 *			  already filled whole MFT record, but had not reached
 *			  @newsize bytes length.
 */
static int ntfs_resident_attr_resize(ntfs_attr *na, const s64 newsize)
{
	ntfs_attr_search_ctx *ctx;
	ntfs_volume *vol;
	ntfs_inode *ni;
	int err;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, new size %lld.\n",
			(unsigned long long)na->ni->mft_no, na->type,
			(long long)newsize);

	/* Get the attribute record that needs modification. */
	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx)
		return -1;
	if (ntfs_attr_lookup(na->type, na->name, na->name_len, 0, 0, NULL, 0,
			ctx)) {
		err = errno;
		goto put_err_out;
	}
	vol = na->ni->vol;
	/*
	 * Check the attribute type and the corresponding minimum and maximum
	 * sizes against @newsize and fail if @newsize is out of bounds.
	 */
	if (ntfs_attr_size_bounds_check(vol, na->type, newsize) < 0) {
		err = errno;
		if (err == ERANGE) {
			ntfs_log_trace("Size bounds check failed.  "
					"Aborting...\n");
		} else if (err == ENOENT)
			err = EIO;
		goto put_err_out;
	}
	/*
	 * If @newsize is bigger than the MFT record we need to make the
	 * attribute non-resident if the attribute type supports it. If it is
	 * smaller we can go ahead and attempt the resize.
	 */
	if (newsize < vol->mft_record_size) {
		/* Perform the resize of the attribute record. */
		if (!ntfs_resident_attr_value_resize(ctx->mrec, ctx->attr,
				newsize)) {
			/* Update attribute size everywhere. */
			na->data_size = na->initialized_size = newsize;
			na->allocated_size = ROUND_UP(newsize, 3);
			if (NAttrCompressed(na) || NAttrSparse(na))
				na->compressed_size = na->allocated_size;
			if (na->type == AT_DATA && na->name == AT_UNNAMED) {
				na->ni->data_size = na->data_size;
				na->ni->allocated_size = na->allocated_size;
				NInoFileNameSetDirty(na->ni);
			}
			goto resize_done;
		}
		/* Error! If not enough space, just continue. */
		if (errno != ENOSPC) {
			err = errno;
			ntfs_log_trace("Failed to resize resident part "
					"of attribute.  Aborting...\n");
			goto put_err_out;
		}
	}
	/* There is not enough space in the MFT record to perform the resize. */

	/* Make the attribute non-resident if possible. */
	if (!ntfs_attr_make_non_resident(na, ctx)) {
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
		ntfs_attr_put_search_ctx(ctx);
		/* Resize non-resident attribute */
		return ntfs_attr_truncate(na, newsize);
	} else if (errno != ENOSPC && errno != EPERM) {
		err = errno;
		ntfs_log_trace("Failed to make attribute non-resident.  "
				"Aborting...\n");
		goto put_err_out;
	}

	/* Try to make other attributes non-resident and retry each time. */
	ntfs_attr_init_search_ctx(ctx, NULL, na->ni->mrec);
	while (!ntfs_attr_lookup(AT_UNUSED, NULL, 0, 0, 0, NULL, 0, ctx)) {
		ntfs_attr *tna;
		ATTR_RECORD *a;

		a = ctx->attr;
		if (a->non_resident)
			continue;

		/*
		 * Check out whether convert is reasonable. Assume that mapping
		 * pairs will take 8 bytes.
		 */
		if (le32_to_cpu(a->length) <= offsetof(ATTR_RECORD,
				compressed_size) + ROUND_UP(a->name_length *
				sizeof(ntfschar), 3) + 8)
			continue;

		tna = ntfs_attr_open(na->ni, a->type, (ntfschar*)((u8*)a +
				le16_to_cpu(a->name_offset)), a->name_length);
		if (!tna) {
			err = errno;
			ntfs_log_trace("Couldn't open attribute.\n");
			goto put_err_out;
		}
		if (ntfs_attr_make_non_resident(tna, ctx)) {
			ntfs_attr_close(tna);
			continue;
		}
		ntfs_inode_mark_dirty(tna->ni);
		ntfs_attr_close(tna);
		ntfs_attr_put_search_ctx(ctx);
		return ntfs_resident_attr_resize(na, newsize);
	}
	/* Check whether error occurred. */
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_trace("Attribute lookup failed.\n");
		goto put_err_out;
	}

	/* We can't move out attribute list, thus move out others. */
	if (na->type == AT_ATTRIBUTE_LIST) {
		ntfs_attr_put_search_ctx(ctx);
		if (ntfs_inode_free_space(na->ni, offsetof(ATTR_RECORD,
				non_resident_end) + 8)) {
			ntfs_log_trace("Couldn't free space in the MFT record "
					"to make attribute list non "
					"resident.\n");
			return -1;
		}
		return ntfs_resident_attr_resize(na, newsize);
	}

	/*
	 * Move the attribute to a new MFT record, creating an attribute list
	 * attribute or modifying it if it is already present.
	 */

	/* Point search context back to attribute which we need resize. */
	ntfs_attr_init_search_ctx(ctx, na->ni, NULL);
	if (ntfs_attr_lookup(na->type, na->name, na->name_len, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		ntfs_log_trace("Attribute lookup failed.\n");
		err = errno;
		goto put_err_out;
	}

	/*
	 * Force index allocation creation instead of moving out index root
	 * from the base MFT record.
	 */
	if (na->type == AT_INDEX_ROOT && na->data_size > sizeof(INDEX_ROOT) +
			sizeof(INDEX_ENTRY_HEADER) + sizeof(VCN)) {
		INDEX_ROOT *ir;

		ir = (INDEX_ROOT*)((u8*)ctx->attr +
				le16_to_cpu(ctx->attr->value_offset));
		if (!(ir->index.flags & LARGE_INDEX)) {
			err = EOVERFLOW;
			goto put_err_out;
		}
	}

	/*
	 * Check whether attribute is already single in the this MFT record.
	 * 8 added for the attribute terminator.
	 */
	if (le32_to_cpu(ctx->mrec->bytes_in_use) ==
			le16_to_cpu(ctx->mrec->attrs_offset) +
			le32_to_cpu(ctx->attr->length) + 8) {
		err = EOVERFLOW;
		goto put_err_out;
	}

	/* Add attribute list if not present. */
	if (na->ni->nr_extents == -1)
		ni = na->ni->base_ni;
	else
		ni = na->ni;
	if (!NInoAttrList(ni)) {
		ntfs_attr_put_search_ctx(ctx);
		if (ntfs_inode_add_attrlist(ni))
			return -1;
		return ntfs_resident_attr_resize(na, newsize);
	}
	/* Allocate new MFT record. */
	ni = ntfs_mft_record_alloc(vol, ni);
	if (!ni) {
		err = errno;
		ntfs_log_trace("Couldn't allocate new MFT record.\n");
		goto put_err_out;
	}
	/* Move attribute to it. */
	if (ntfs_attr_record_move_to(ctx, ni)) {
		err = errno;
		ntfs_log_trace("Couldn't move attribute to new MFT record.\n");
		goto put_err_out;
	}
	/* Update ntfs attribute. */
	if (na->ni->nr_extents == -1)
		na->ni = ni;

	ntfs_attr_put_search_ctx(ctx);
	/* Try to perform resize once again. */
	return ntfs_resident_attr_resize(na, newsize);

resize_done:
	/*
	 * Set the inode (and its base inode if it exists) dirty so it is
	 * written out later.
	 */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	/* Done! */
	ntfs_attr_put_search_ctx(ctx);
	return 0;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}

/**
 * ntfs_attr_make_resident - convert a non-resident to a resident attribute
 * @na:		open ntfs attribute to make resident
 * @ctx:	ntfs search context describing the attribute
 *
 * Convert a non-resident ntfs attribute to a resident one.
 *
 * Return 0 on success and -1 on error with errno set to the error code. The
 * following error codes are defined:
 *	EINVAL	   - Invalid arguments passed.
 *	EPERM	   - The attribute is not allowed to be resident.
 *	EIO	   - I/O error, damaged inode or bug.
 *	ENOSPC	   - There is no enough space to perform conversion.
 *	EOPNOTSUPP - Requested conversion is not supported yet.
 *
 * Warning: We do not set the inode dirty and we do not write out anything!
 *	    We expect the caller to do this as this is a fairly low level
 *	    function and it is likely there will be further changes made.
 */
static int ntfs_attr_make_resident(ntfs_attr *na, ntfs_attr_search_ctx *ctx)
{
	ntfs_volume *vol = na->ni->vol;
	ATTR_REC *a = ctx->attr;
	int name_ofs, val_ofs;
	s64 arec_size, bytes_read;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n", (unsigned long
			long)na->ni->mft_no, na->type);

	/* Should be called for the first extent of the attribute. */
	if (sle64_to_cpu(a->lowest_vcn)) {
		ntfs_log_trace("Should be called for the first extent of the "
				"attribute.  Aborting...\n");
		errno = EINVAL;
		return -1;
	}

	/* Some preliminary sanity checking. */
	if (!NAttrNonResident(na)) {
		ntfs_log_trace("Trying to make resident attribute resident.  "
				"Aborting...\n");
		errno = EINVAL;
		return -1;
	}

	/* Make sure this is not $MFT/$BITMAP or Windows will not boot! */
	if (na->type == AT_BITMAP && na->ni->mft_no == FILE_MFT) {
		errno = EPERM;
		return -1;
	}

	/* Check that the attribute is allowed to be resident. */
	if (ntfs_attr_can_be_resident(vol, na->type))
		return -1;

	/*
	 * Check that the attribute name hasn't been placed after the
	 * mapping pairs array. Chkdsk treat this as corruption.
	 */
	if (a->name_length && le16_to_cpu(a->name_offset) >=
			le16_to_cpu(a->mapping_pairs_offset)) {
		ntfs_log_trace("Damaged attribute. Name is placed after the "
				"mapping pairs array. Run chkdsk. Aborting.\n");
		errno = EIO;
		return -1;
	}

	if (NAttrCompressed(na) || NAttrEncrypted(na)) {
		ntfs_log_trace("Making compressed or encrypted files resident "
				"is not implemented yet.\n");
		errno = EOPNOTSUPP;
		return -1;
	}

	/* Work out offsets into and size of the resident attribute. */
	name_ofs = 24; /* = sizeof(resident_ATTR_REC); */
	val_ofs = (name_ofs + a->name_length * sizeof(ntfschar) + 7) & ~7;
	arec_size = (val_ofs + na->data_size + 7) & ~7;

	/* Sanity check the size before we start modifying the attribute. */
	if (le32_to_cpu(ctx->mrec->bytes_in_use) - le32_to_cpu(a->length) +
			arec_size > le32_to_cpu(ctx->mrec->bytes_allocated)) {
		ntfs_log_trace("Not enough space to make attribute resident\n");
		errno = ENOSPC;
		return -1;
	}

	/* Read and cache the whole runlist if not already done. */
	if (ntfs_attr_map_whole_runlist(na))
		return -1;

	/* Move the attribute name if it exists and update the offset. */
	if (a->name_length) {
		memmove((u8*)a + name_ofs, (u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	}
	a->name_offset = cpu_to_le16(name_ofs);

	/* Resize the resident part of the attribute record. */
	if (ntfs_attr_record_resize(ctx->mrec, a, arec_size) < 0) {
		/*
		 * Bug, because ntfs_attr_record_resize should not fail (we
		 * already checked that attribute fits MFT record).
		 */
		ntfs_log_error("BUG! Failed to resize attribute record. "
				"Please report to the %s.  Aborting...\n",
				NTFS_DEV_LIST);
		errno = EIO;
		return -1;
	}

	/* Convert the attribute record to describe a resident attribute. */
	a->non_resident = 0;
	a->flags = 0;
	a->value_length = cpu_to_le32(na->data_size);
	a->value_offset = cpu_to_le16(val_ofs);
	/*
	 * File names cannot be non-resident so we would never see this here
	 * but at least it serves as a reminder that there may be attributes
	 * for which we do need to set this flag. (AIA)
	 */
	if (a->type == AT_FILE_NAME)
		a->resident_flags = RESIDENT_ATTR_IS_INDEXED;
	else
		a->resident_flags = 0;
	a->reservedR = 0;

	/* Sanity fixup...  Shouldn't really happen. (AIA) */
	if (na->initialized_size > na->data_size)
		na->initialized_size = na->data_size;

	/* Copy data from run list to resident attribute value. */
	bytes_read = ntfs_rl_pread(vol, na->rl, 0, na->initialized_size,
			(u8*)a + val_ofs);
	if (bytes_read != na->initialized_size) {
		if (bytes_read >= 0)
			errno = EIO;
		ntfs_log_trace("Eeek! Failed to read attribute data. Leaving "
				"inconsistent metadata. Run chkdsk.  "
				"Aborting...\n");
		return -1;
	}

	/* Clear memory in gap between initialized_size and data_size. */
	if (na->initialized_size < na->data_size)
		memset((u8*)a + val_ofs + na->initialized_size, 0,
				na->data_size - na->initialized_size);

	/*
	 * Deallocate clusters from the runlist.
	 *
	 * NOTE: We can use ntfs_cluster_free() because we have already mapped
	 * the whole run list and thus it doesn't matter that the attribute
	 * record is in a transiently corrupted state at this moment in time.
	 */
	if (ntfs_cluster_free(vol, na, 0, -1) < 0) {
		ntfs_log_perror("Eeek! Failed to release allocated clusters");
		ntfs_log_trace("Ignoring error and leaving behind wasted "
				"clusters.\n");
	}

	/* Throw away the now unused runlist. */
	free(na->rl);
	na->rl = NULL;

	/* Update in-memory struct ntfs_attr. */
	NAttrClearNonResident(na);
	NAttrClearCompressed(na);
	NAttrClearSparse(na);
	NAttrClearEncrypted(na);
	na->initialized_size = na->data_size;
	na->allocated_size = na->compressed_size = (na->data_size + 7) & ~7;
	na->compression_block_size = 0;
	na->compression_block_size_bits = na->compression_block_clusters = 0;
	return 0;
}

#define NTFS_VCN_DELETE_MARK -2
/**
 * ntfs_attr_update_mapping_pairs - update mapping pairs for ntfs attribute
 * @na:		non-resident ntfs open attribute for which we need update
 * @from_vcn:	update runlist starting this VCN
 *
 * Build mapping pairs from @na->rl and write them to the disk. Also, this
 * function updates sparse bit, allocated and compressed size (allocates/frees
 * space for this field if required).
 *
 * @na->allocated_size should be set to correct value for the new runlist before
 * call to this function. Vice-versa @na->compressed_size will be calculated and
 * set to correct value during this function.
 *
 * New runlist should be fully formed starting @from_vcn. Runs before @from_vcn
 * can be mapped or not, but on-disk structures should not be modified before
 * call to this function so they can be mapped if necessary.
 *
 * FIXME: Make it O(1) for sparse files too, not only for normal.
 *
 * FIXME: Rewrite without using NTFS_VCN_DELETE_MARK define.
 *
 * NOTE: Be careful in the future with updating bits on compressed files (at
 * present assumed that on-disk flag is already set/cleared before call to
 * this function).
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 * The following error codes are defined:
 *	EINVAL - Invalid arguments passed.
 *	ENOMEM - Not enough memory to complete operation.
 *	ENOSPC - There is no enough space in base mft to resize $ATTRIBUTE_LIST
 *		 or there is no free MFT records left to allocate.
 */
int ntfs_attr_update_mapping_pairs(ntfs_attr *na, VCN from_vcn)
{
	ntfs_attr_search_ctx *ctx;
	ntfs_inode *ni, *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	VCN stop_vcn;
	int err, mp_size, cur_max_mp_size, exp_max_mp_size;
	BOOL finished_build;

retry:
	if (!na || !na->rl) {
		ntfs_log_trace("Invalid parameters passed.\n");
		errno = EINVAL;
		return -1;
	}

	if (!NAttrNonResident(na)) {
		ntfs_log_trace("Attribute should be non resident.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, from vcn 0x%lld."
			"\n", (unsigned long long)na->ni->mft_no, na->type,
			from_vcn);

	if (na->ni->nr_extents == -1)
		base_ni = na->ni->base_ni;
	else
		base_ni = na->ni;

	ctx = ntfs_attr_get_search_ctx(base_ni, NULL);
	if (!ctx) {
		ntfs_log_trace("Couldn't get search context.\n");
		return -1;
	}

	/* Fill attribute records with new mapping pairs. */
	stop_vcn = 0;
	finished_build = FALSE;
	while (!ntfs_attr_lookup(na->type, na->name, na->name_len,
				CASE_SENSITIVE, ctx->is_first ? 0 : from_vcn,
				NULL, 0, ctx)) {
		a = ctx->attr;
		m = ctx->mrec;
		/*
		 * If runlist is updating not from the beginning, then set
		 * @stop_vcn properly, i.e. to the lowest vcn of record that
		 * contain @from_vcn. Also we do not need @from_vcn anymore,
		 * set it to 0 to make ntfs_attr_lookup enumerate attributes.
		 */
		if (from_vcn && a->lowest_vcn) {
			LCN first_lcn;

			stop_vcn = sle64_to_cpu(a->lowest_vcn);
			from_vcn = 0;
			/*
			 * Check whether the first run we need to update is
			 * the last run in runlist, if so, then deallocate
			 * all attribute extents starting this one.
			 */
			first_lcn = ntfs_rl_vcn_to_lcn(na->rl, stop_vcn);
			if (first_lcn == LCN_EINVAL) {
				ntfs_log_trace("BUG! Incorrect runlist.\n");
				err = EIO;
				goto put_err_out;
			}
			if (first_lcn == LCN_ENOENT ||
					first_lcn == LCN_RL_NOT_MAPPED)
				finished_build = TRUE;
		}

		/*
		 * Check whether we finished mapping pairs build, if so mark
		 * extent as need to delete (by setting highest vcn to
		 * NTFS_VCN_DELETE_MARK (-2), we shall check it later and
		 * delete extent) and continue search.
		 */
		if (finished_build) {
			ntfs_log_trace("Mark attr 0x%x for delete in inode "
					"0x%llx.\n", (unsigned)le32_to_cpu(
					a->type), ctx->ntfs_ino->mft_no);
			a->highest_vcn = cpu_to_sle64(NTFS_VCN_DELETE_MARK);
			ntfs_inode_mark_dirty(ctx->ntfs_ino);
			continue;
		}

		/*
		 * Check that the attribute name hasn't been placed after the
		 * mapping pairs array. Windows treat this as a corruption.
		 */
		if (a->name_length) {
			if (le16_to_cpu(a->name_offset) >=
					le16_to_cpu(a->mapping_pairs_offset)) {
				ntfs_log_error("Damaged attribute. Name is "
						"placed after the mapping "
						"pairs array. Run chkdsk.\n");
				err = EIO;
				goto put_err_out;
			}
		}
		/*
		 * If we in the first extent, then set/clean sparse bit,
		 * update allocated and compressed size.
		 */
		if (!a->lowest_vcn) {
			int sparse;

			/* Update allocated size. */
			a->allocated_size = cpu_to_sle64(na->allocated_size);
			/*
			 * Check whether part of runlist we are updating is
			 * sparse.
			 */
			sparse = ntfs_rl_sparse(na->rl);
			if (sparse == -1) {
				ntfs_log_trace("Bad runlist.\n");
				err = errno;
				goto put_err_out;
			}
			/*
			 * If new part or on-disk attribute is not sparse, then
			 * we should fully map runlist to make final decision.
			 */
			if (sparse || (a->flags & ATTR_IS_SPARSE)) {
				if (from_vcn && ntfs_attr_map_runlist_range(na,
						0, from_vcn - 1)) {
					ntfs_log_trace("Failed to map runlist "
							"before @from_vcn.\n");
					err = errno;
					goto put_err_out;
				}
				/*
				 * Reconsider whether whole runlist is sparse
				 * if new part is not.
				 */
				if (!sparse) {
					sparse = ntfs_rl_sparse(na->rl);
					if (sparse == -1) {
						ntfs_log_trace("Bad "
								"runlist.\n");
						err = errno;
						goto put_err_out;
					}
				}
			}
			/* Attribute becomes sparse/compressed. */
			if (sparse && !(a->flags & (ATTR_IS_SPARSE |
							ATTR_IS_COMPRESSED))) {
				/*
				 * We need to move attribute to another mft
				 * record, if attribute is to small to add
				 * compressed_size field to it and we have no
				 * free space in the current mft record.
				 */
				if ((le32_to_cpu(a->length) - le16_to_cpu(
						a->mapping_pairs_offset)
						== 8) && !(le32_to_cpu(
						m->bytes_allocated) -
						le32_to_cpu(m->bytes_in_use))) {
					if (!NInoAttrList(na->ni)) {
						ntfs_attr_put_search_ctx(ctx);
						if (ntfs_inode_add_attrlist(
									na->ni))
							return -1;
						goto retry;
					}
					if (ntfs_attr_record_move_away(ctx,
								8)) {
						ntfs_log_trace("Failed to move "
							"attribute to another "
							"extent. Aborting..\n");
						err = errno;
						goto put_err_out;
					}
					ntfs_attr_put_search_ctx(ctx);
					goto retry;
				}
				if (!(le32_to_cpu(a->length) - le16_to_cpu(
						a->mapping_pairs_offset))) {
					ntfs_log_trace("Size of the space "
							"allocated for mapping "
							"pairs should not be 0."
							"  Aborting ...\n");
					err = EIO;
					goto put_err_out;
				}
				NAttrSetSparse(na);
				a->flags |= ATTR_IS_SPARSE;
				a->compression_unit = 4; /* Windows set it so,
							    even if attribute
							    is not actually
							    compressed. */
				memmove((u8*)a + le16_to_cpu(a->name_offset) +
					8, (u8*)a + le16_to_cpu(a->name_offset),
					a->name_length * sizeof(ntfschar));
				a->name_offset = cpu_to_le16(le16_to_cpu(
							a->name_offset) + 8);
				a->mapping_pairs_offset =
						cpu_to_le16(le16_to_cpu(
						a->mapping_pairs_offset) + 8);
				/*
				 * We should update all mapping pairs, because
				 * we shifted their starting position.
				 */
				from_vcn = 0;
			}
			/* Attribute becomes normal. */
			if (!sparse && (a->flags & ATTR_IS_SPARSE) &&
					!(a->flags & ATTR_IS_COMPRESSED)) {
				NAttrClearSparse(na);
				a->flags &= ~ATTR_IS_SPARSE;
				a->compression_unit = 0;
				memmove((u8*)a + le16_to_cpu(a->name_offset) -
					8, (u8*)a + le16_to_cpu(a->name_offset),
					a->name_length * sizeof(ntfschar));
				/*
				 * Windows defragmentation tool do not update
				 * name offset correctly for unnamed
				 * attributes, but chkdsk do not like when it
				 * negative, so do not change it at all if it
				 * would become negative.
				 */
				if (le16_to_cpu(a->name_offset) >= 8)
					a->name_offset = cpu_to_le16(
							le16_to_cpu(
							a->name_offset) - 8);
				a->mapping_pairs_offset =
						cpu_to_le16(le16_to_cpu(
						a->mapping_pairs_offset) - 8);
				/*
				 * We should update all mapping pairs, because
				 * we shifted their starting position.
				 */
				from_vcn = 0;
			}
			/* Update compressed size if required. */
			if (sparse || (a->flags & ATTR_IS_COMPRESSED)) {
				s64 new_compr_size;

				new_compr_size = ntfs_rl_get_compressed_size(
						na->ni->vol, na->rl);
				if (new_compr_size == -1) {
					err = errno;
					ntfs_log_trace("BUG! Leaving "
							"inconsistent "
							"metadata.\n");
					goto put_err_out;
				}
				na->compressed_size = new_compr_size;
				a->compressed_size = cpu_to_sle64(
						new_compr_size);
			}
			/*
			 * Set FILE_NAME dirty flag, to update sparse bit and
			 * allocated size in the index.
			 */
			if (na->type == AT_DATA && na->name == AT_UNNAMED) {
				if (sparse)
					na->ni->allocated_size =
							na->compressed_size;
				else
					na->ni->allocated_size =
							na->allocated_size;
				NInoFileNameSetDirty(na->ni);
			}

			/*
			 * We do want to do anything for the first extent in
			 * case we are updating mapping pairs not from the
			 * begging.
			 */
			if (!a->highest_vcn || from_vcn <=
					sle64_to_cpu(a->highest_vcn) + 1)
				from_vcn = 0;
			else {
				if (from_vcn)
					continue;
			}
		}

		/* Get the size for the rest of mapping pairs array. */
		mp_size = ntfs_get_size_for_mapping_pairs(na->ni->vol, na->rl,
								stop_vcn);
		if (mp_size <= 0) {
			err = errno;
			ntfs_log_trace("Get size for mapping pairs failed.\n");
			goto put_err_out;
		}
		/*
		 * Determine maximum possible length of mapping pairs,
		 * if we shall *not* expand space for mapping pairs.
		 */
		cur_max_mp_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->mapping_pairs_offset);
		/*
		 * Determine maximum possible length of mapping pairs in the
		 * current mft record, if we shall expand space for mapping
		 * pairs.
		 */
		exp_max_mp_size = le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use) + cur_max_mp_size;
		/* Test mapping pairs for fitting in the current mft record. */
		if (mp_size > exp_max_mp_size) {
			/*
			 * Mapping pairs of $ATTRIBUTE_LIST attribute must fit
			 * in the base mft record. Try to move out other
			 * attributes and try again.
			 */
			if (na->type == AT_ATTRIBUTE_LIST) {
				ntfs_attr_put_search_ctx(ctx);
				if (ntfs_inode_free_space(na->ni, mp_size -
							cur_max_mp_size)) {
					if (errno != ENOSPC)
						return -1;
					ntfs_log_error("Attribute list mapping "
							"pairs size to big, "
							"can't fit them in the "
							"base MFT record. "
							"Defragment volume and "
							"try once again.\n");
					errno = ENOSPC;
					return -1;
				}
				goto retry;
			}

			/* Add attribute list if it isn't present, and retry. */
			if (!NInoAttrList(base_ni)) {
				ntfs_attr_put_search_ctx(ctx);
				if (ntfs_inode_add_attrlist(base_ni)) {
					ntfs_log_trace("Couldn't add attribute "
							"list.\n");
					return -1;
				}
				goto retry;
			}

			/*
			 * Set mapping pairs size to maximum possible for this
			 * mft record. We shall write the rest of mapping pairs
			 * to another MFT records.
			 */
			mp_size = exp_max_mp_size;
		}

		/* Change space for mapping pairs if we need it. */
		if (((mp_size + 7) & ~7) != cur_max_mp_size) {
			if (ntfs_attr_record_resize(m, a,
					le16_to_cpu(a->mapping_pairs_offset) +
					mp_size)) {
				ntfs_log_error("BUG! Ran out of space in mft "
						"record. Please run chkdsk and "
						"if that doesn't find any "
						"errors please report you saw "
						"this message to %s.\n",
						NTFS_DEV_LIST);
				err = EIO;
				goto put_err_out;
			}
		}

		/* Update lowest vcn. */
		a->lowest_vcn = cpu_to_sle64(stop_vcn);
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
		if ((ctx->ntfs_ino->nr_extents == -1 ||
					NInoAttrList(ctx->ntfs_ino)) &&
					ctx->attr->type != AT_ATTRIBUTE_LIST) {
			ctx->al_entry->lowest_vcn = cpu_to_sle64(stop_vcn);
			ntfs_attrlist_mark_dirty(ctx->ntfs_ino);
		}

		/*
		 * Generate the new mapping pairs array directly into the
		 * correct destination, i.e. the attribute record itself.
		 */
		if (!ntfs_mapping_pairs_build(na->ni->vol, (u8*)a + le16_to_cpu(
				a->mapping_pairs_offset), mp_size, na->rl,
				stop_vcn, &stop_vcn))
			finished_build = TRUE;
		if (!finished_build && errno != ENOSPC) {
			err = errno;
			ntfs_log_error("BUG!  Mapping pairs build failed.  "
					"Please run chkdsk and if that doesn't "
					"find any errors please report you saw "
					"this message to %s.\n", NTFS_DEV_LIST);
			goto put_err_out;
		}
		a->highest_vcn = cpu_to_sle64(stop_vcn - 1);
	}
	/* Check whether error occurred. */
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_trace("Attribute lookup failed.\n");
		goto put_err_out;
	}
	/* Sanity check. */
	if (from_vcn) {
		err = ENOMSG;
		ntfs_log_error("Library BUG! @from_vcn is nonzero, please "
				"report to %s.\n", NTFS_DEV_LIST);
		goto put_err_out;
	}

	/* Deallocate not used attribute extents and return with success. */
	if (finished_build) {
		ntfs_attr_reinit_search_ctx(ctx);
		ntfs_log_trace("Deallocate marked extents.\n");
		while (!ntfs_attr_lookup(na->type, na->name, na->name_len,
				CASE_SENSITIVE, 0, NULL, 0, ctx)) {
			if (sle64_to_cpu(ctx->attr->highest_vcn) !=
							NTFS_VCN_DELETE_MARK)
				continue;
			/* Remove unused attribute record. */
			if (ntfs_attr_record_rm(ctx)) {
				err = errno;
				ntfs_log_trace("Couldn't remove unused "
						"attribute record.\n");
				goto put_err_out;
			}
			ntfs_attr_reinit_search_ctx(ctx);
		}
		if (errno != ENOENT) {
			err = errno;
			ntfs_log_trace("Attribute lookup failed.\n");
			goto put_err_out;
		}
		ntfs_log_trace("Deallocate done.\n");
		ntfs_attr_put_search_ctx(ctx);
		ntfs_log_trace("Done!");
		return 0;
	}
	ntfs_attr_put_search_ctx(ctx);
	ctx = NULL;

	/* Allocate new MFT records for the rest of mapping pairs. */
	while (1) {
		/* Calculate size of rest mapping pairs. */
		mp_size = ntfs_get_size_for_mapping_pairs(na->ni->vol,
						na->rl, stop_vcn);
		if (mp_size <= 0) {
			err = errno;
			ntfs_log_trace("Get size for mapping pairs failed.\n");
			goto put_err_out;
		}
		/* Allocate new mft record. */
		ni = ntfs_mft_record_alloc(na->ni->vol, base_ni);
		if (!ni) {
			err = errno;
			ntfs_log_trace("Couldn't allocate new MFT record.\n");
			goto put_err_out;
		}
		m = ni->mrec;
		/*
		 * If mapping size exceed available space, set them to
		 * possible maximum.
		 */
		cur_max_mp_size = le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use) -
				(offsetof(ATTR_RECORD, compressed_size) +
				((NAttrCompressed(na) || NAttrSparse(na)) ?
				sizeof(a->compressed_size) : 0)) -
				((sizeof(ntfschar) * na->name_len + 7) & ~7);
		if (mp_size > cur_max_mp_size)
			mp_size = cur_max_mp_size;
		/* Add attribute extent to new record. */
		err = ntfs_non_resident_attr_record_add(ni, na->type,
			 na->name, na->name_len, stop_vcn, mp_size, 0);
		if (err == -1) {
			err = errno;
			ntfs_log_trace("Couldn't add attribute extent into the "
					"MFT record.\n");
			if (ntfs_mft_record_free(na->ni->vol, ni)) {
				ntfs_log_trace("Couldn't free MFT record.\n");
			}
			goto put_err_out;
		}
		a = (ATTR_RECORD*)((u8*)m + err);

		err = ntfs_mapping_pairs_build(na->ni->vol, (u8*)a +
			le16_to_cpu(a->mapping_pairs_offset), mp_size, na->rl,
			stop_vcn, &stop_vcn);
		if (err < 0 && errno != ENOSPC) {
			err = errno;
			ntfs_log_error("BUG!  Mapping pairs build failed.  "
					"Please run chkdsk and if that doesn't "
					"find any errors please report you saw "
					"this message to %s.\n", NTFS_DEV_LIST);
			if (ntfs_mft_record_free(na->ni->vol, ni))
				ntfs_log_trace("Couldn't free MFT record.\n");
			goto put_err_out;
		}
		a->highest_vcn = cpu_to_sle64(stop_vcn - 1);
		ntfs_inode_mark_dirty(ni);
		/* All mapping pairs has been written. */
		if (!err)
			break;
	}
	ntfs_log_trace("Done!\n");
	return 0;
put_err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}
#undef NTFS_VCN_DELETE_MARK

/**
 * ntfs_non_resident_attr_shrink - shrink a non-resident, open ntfs attribute
 * @na:		non-resident ntfs attribute to shrink
 * @newsize:	new size (in bytes) to which to shrink the attribute
 *
 * Reduce the size of a non-resident, open ntfs attribute @na to @newsize bytes.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 * The following error codes are defined:
 *	ENOMEM	- Not enough memory to complete operation.
 *	ERANGE	- @newsize is not valid for the attribute type of @na.
 */
static int ntfs_non_resident_attr_shrink(ntfs_attr *na, const s64 newsize)
{
	ntfs_volume *vol;
	ntfs_attr_search_ctx *ctx;
	VCN first_free_vcn;
	s64 nr_freed_clusters;
	int err;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, newsize %lld.\n",
			(unsigned long long)na->ni->mft_no, na->type,
			(long long)newsize);

	vol = na->ni->vol;

	/*
	 * Check the attribute type and the corresponding minimum size
	 * against @newsize and fail if @newsize is too small.
	 */
	if (ntfs_attr_size_bounds_check(vol, na->type, newsize) < 0) {
		if (errno == ERANGE) {
			ntfs_log_trace("Eeek! Size bounds check failed. "
					"Aborting...\n");
		} else if (errno == ENOENT)
			errno = EIO;
		return -1;
	}

	/* The first cluster outside the new allocation. */
	first_free_vcn = (newsize + vol->cluster_size - 1) >>
			vol->cluster_size_bits;
	/*
	 * Compare the new allocation with the old one and only deallocate
	 * clusters if there is a change.
	 */
	if ((na->allocated_size >> vol->cluster_size_bits) != first_free_vcn) {
		if (ntfs_attr_map_whole_runlist(na)) {
			ntfs_log_trace("Eeek! ntfs_attr_map_whole_runlist "
					"failed.\n");
			return -1;
		}
		/* Deallocate all clusters starting with the first free one. */
		nr_freed_clusters = ntfs_cluster_free(vol, na, first_free_vcn,
				-1);
		if (nr_freed_clusters < 0) {
			ntfs_log_trace("Eeek! Freeing of clusters failed. "
					"Aborting...\n");
			return -1;
		}

		/* Truncate the runlist itself. */
		if (ntfs_rl_truncate(&na->rl, first_free_vcn)) {
			err = errno;
			/*
			 * Failed to truncate the runlist, so just throw it
			 * away, it will be mapped afresh on next use.
			 */
			free(na->rl);
			na->rl = NULL;
			ntfs_log_trace("Eeek! Run list truncation failed.\n");
			errno = err;
			return -1;
		}

		/* Prepare to mapping pairs update. */
		na->allocated_size = first_free_vcn << vol->cluster_size_bits;
		/* Write mapping pairs for new runlist. */
		if (ntfs_attr_update_mapping_pairs(na, first_free_vcn)) {
			ntfs_log_trace("Eeek! Mapping pairs update failed. "
					"Leaving inconsistent metadata. "
					"Run chkdsk.\n");
			return -1;
		}
	}

	/* Get the first attribute record. */
	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx) {
		ntfs_log_trace("Couldn't get attribute search context.\n");
		return -1;
	}
	if (ntfs_attr_lookup(na->type, na->name, na->name_len, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		err = errno;
		if (err == ENOENT)
			err = EIO;
		ntfs_log_trace("Eeek! Lookup of first attribute extent failed. "
				"Leaving inconsistent metadata.\n");
		goto put_err_out;
	}

	/* Update data and initialized size. */
	na->data_size = newsize;
	ctx->attr->data_size = cpu_to_sle64(newsize);
	if (newsize < na->initialized_size) {
		na->initialized_size = newsize;
		ctx->attr->initialized_size = cpu_to_sle64(newsize);
	}
	/* Update data size in the index. */
	if (na->type == AT_DATA && na->name == AT_UNNAMED) {
		na->ni->data_size = na->data_size;
		NInoFileNameSetDirty(na->ni);
	}

	/* If the attribute now has zero size, make it resident. */
	if (!newsize) {
		if (ntfs_attr_make_resident(na, ctx)) {
			/* If couldn't make resident, just continue. */
			if (errno != EPERM)
				ntfs_log_error("Failed to make attribute "
						"resident. Leaving as is...\n");
		}
	}

	/* Set the inode dirty so it is written out later. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	/* Done! */
	ntfs_attr_put_search_ctx(ctx);
	return 0;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}

/**
 * ntfs_non_resident_attr_expand - expand a non-resident, open ntfs attribute
 * @na:		non-resident ntfs attribute to expand
 * @newsize:	new size (in bytes) to which to expand the attribute
 * @sparse:	if TRUE then will create hole if possible
 *
 * Expand the size of a non-resident, open ntfs attribute @na to @newsize bytes,
 * by allocating new clusters.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 * The following error codes are defined:
 *	ENOMEM - Not enough memory to complete operation.
 *	ERANGE - @newsize is not valid for the attribute type of @na.
 *	ENOSPC - There is no enough space in base mft to resize $ATTRIBUTE_LIST.
 */
static int ntfs_non_resident_attr_expand(ntfs_attr *na, const s64 newsize,
		BOOL sparse)
{
	VCN first_free_vcn;
	ntfs_volume *vol;
	ntfs_attr_search_ctx *ctx;
	runlist *rl, *rln;
	s64 org_alloc_size;
	int err;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, new size %lld, "
			"current size %lld.\n",
			(unsigned long long)na->ni->mft_no, na->type,
			(long long)newsize, (long long)na->data_size);

	vol = na->ni->vol;

	/*
	 * Check the attribute type and the corresponding maximum size
	 * against @newsize and fail if @newsize is too big.
	 */
	if (ntfs_attr_size_bounds_check(vol, na->type, newsize) < 0) {
		if (errno == ERANGE) {
			ntfs_log_trace("Eeek! Size bounds check failed. "
					"Aborting...\n");
		} else if (errno == ENOENT)
			errno = EIO;
		return -1;
	}

	/* Save for future use. */
	org_alloc_size = na->allocated_size;
	/* The first cluster outside the new allocation. */
	first_free_vcn = (newsize + vol->cluster_size - 1) >>
			vol->cluster_size_bits;
	/*
	 * Compare the new allocation with the old one and only allocate
	 * clusters if there is a change.
	 */
	if ((na->allocated_size >> vol->cluster_size_bits) < first_free_vcn) {
		/* Map required part of runlist. */
		if (ntfs_attr_map_runlist(na, na->allocated_size >>
					vol->cluster_size_bits)) {
			ntfs_log_error("Failed to map runlist.\n");
			return -1;
		}

		/*
		 * If we extend $DATA attribute on NTFS 3+ volume, we can add
		 * sparse runs instead of real allocation of clusters.
		 */
		if (na->type == AT_DATA && vol->major_ver >= 3 && sparse) {
			rl = ntfs_malloc(0x1000);
			if (!rl)
				return -1;

			rl[0].vcn = (na->allocated_size >>
					vol->cluster_size_bits);
			rl[0].lcn = LCN_HOLE;
			rl[0].length = first_free_vcn -
				(na->allocated_size >> vol->cluster_size_bits);
			rl[1].vcn = first_free_vcn;
			rl[1].lcn = LCN_ENOENT;
			rl[1].length = 0;
		} else {
			/*
			 * Determine first after last LCN of attribute.
			 * We will start seek clusters from this LCN to avoid
			 * fragmentation.  If there are no valid LCNs in the
			 * attribute let the cluster allocator choose the
			 * starting LCN.
			 */
			LCN lcn_seek_from;

			lcn_seek_from = -1;
			if (na->rl->length) {
				/* Seek to the last run list element. */
				for (rl = na->rl; (rl + 1)->length; rl++)
					;
				/*
				 * If the last LCN is a hole or similar seek
				 * back to last valid LCN.
				 */
				while (rl->lcn < 0 && rl != na->rl)
					rl--;
				/*
				 * Only set lcn_seek_from it the LCN is valid.
				 */
				if (rl->lcn >= 0)
					lcn_seek_from = rl->lcn + rl->length;
			}

			rl = ntfs_cluster_alloc(vol, na->allocated_size >>
					vol->cluster_size_bits, first_free_vcn -
					(na->allocated_size >>
					vol->cluster_size_bits), lcn_seek_from,
					DATA_ZONE);
			if (!rl) {
				ntfs_log_trace("Cluster allocation failed.\n");
				return -1;
			}
		}

		/* Append new clusters to attribute runlist. */
		rln = ntfs_runlists_merge(na->rl, rl);
		if (!rln) {
			/* Failed, free just allocated clusters. */
			err = errno;
			ntfs_log_trace("Run list merge failed.\n");
			ntfs_cluster_free_from_rl(vol, rl);
			free(rl);
			errno = err;
			return -1;
		}
		na->rl = rln;

		/* Prepare to mapping pairs update. */
		na->allocated_size = first_free_vcn << vol->cluster_size_bits;
		/* Write mapping pairs for new runlist. */
		if (ntfs_attr_update_mapping_pairs(na, org_alloc_size >>
				vol->cluster_size_bits)) {
			err = errno;
			ntfs_log_trace("Mapping pairs update failed.\n");
			goto rollback;
		}
	}

	ctx = ntfs_attr_get_search_ctx(na->ni, NULL);
	if (!ctx) {
		ntfs_log_trace("Failed to get search context.\n");
		if (na->allocated_size == org_alloc_size) {
			return -1;
		}
		err = errno;
		goto rollback;
	}

	if (ntfs_attr_lookup(na->type, na->name, na->name_len, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		err = errno;
		ntfs_log_trace("Lookup of first attribute extent failed.\n");
		if (err == ENOENT)
			err = EIO;
		if (na->allocated_size != org_alloc_size) {
			ntfs_attr_put_search_ctx(ctx);
			goto rollback;
		} else
			goto put_err_out;
	}

	/* Update data size. */
	na->data_size = newsize;
	ctx->attr->data_size = cpu_to_sle64(newsize);
	/* Update data size in the index. */
	if (na->type == AT_DATA && na->name == AT_UNNAMED) {
		na->ni->data_size = na->data_size;
		NInoFileNameSetDirty(na->ni);
	}
	/* Set the inode dirty so it is written out later. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	/* Done! */
	ntfs_attr_put_search_ctx(ctx);
	return 0;
rollback:
	/* Free allocated clusters. */
	if (ntfs_cluster_free(vol, na, org_alloc_size >>
			vol->cluster_size_bits, -1) < 0) {
		ntfs_log_trace("Eeek!  Leaking clusters.  Run chkdsk!\n");
		err = EIO;
	}
	/* Now, truncate the runlist itself. */
	if (ntfs_rl_truncate(&na->rl, org_alloc_size >>
			vol->cluster_size_bits)) {
		/*
		 * Failed to truncate the runlist, so just throw it away, it
		 * will be mapped afresh on next use.
		 */
		free(na->rl);
		na->rl = NULL;
		ntfs_log_trace("Couldn't truncate runlist. Rollback failed.\n");
	} else {
		/* Prepare to mapping pairs update. */
		na->allocated_size = org_alloc_size;
		/* Restore mapping pairs. */
		if (ntfs_attr_update_mapping_pairs(na, na->allocated_size >>
					vol->cluster_size_bits)) {
			ntfs_log_trace("Failed to restore old mapping pairs. "
					"Rollback failed.\n");
		}
	}
	errno = err;
	return -1;
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return -1;
}


/**
 * __ntfs_attr_truncate - resize an ntfs attribute
 * @na:		open ntfs attribute to resize
 * @newsize:	new size (in bytes) to which to resize the attribute
 * @sparse:	if TRUE then will create hole if possible
 *
 * Change the size of an open ntfs attribute @na to @newsize bytes. If the
 * attribute is made bigger and the attribute is resident the newly
 * "allocated" space is cleared and if the attribute is non-resident the
 * newly allocated space is marked as not initialised and no real allocation
 * on disk is performed.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 * The following error codes are defined:
 *	EINVAL	   	- Invalid arguments were passed to the function.
 *	EACCES		- Attribute is encrypted.
 *	ERANGE		- @newsize is not valid for the attribute type of @na.
 *	ENOSPC		- There is no enough space on the volume to allocate
 *			  new clusters or in base mft to resize $ATTRIBUTE_LIST.
 *	EOVERFLOW	- Resident attribute can not become non resident and
 *			  already filled whole MFT record, but had not reached
 *			  @newsize bytes length.
 *	EOPNOTSUPP	- The desired resize is not implemented yet.
 */
int __ntfs_attr_truncate(ntfs_attr *na, const s64 newsize, BOOL sparse)
{
	int ret;

	if (!na || newsize < 0 ||
			(na->ni->mft_no == FILE_MFT && na->type == AT_DATA)) {
		ntfs_log_trace("Invalid arguments passed.\n");
		errno = EINVAL;
		return -1;
	}

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x.\n", (unsigned long
			long)na->ni->mft_no, na->type);

	if (na->data_size == newsize)
		return 0;
	/*
	 * Encrypted attributes are not supported. We return access denied,
	 * which is what Windows NT4 does, too.
	 */
	if (NAttrEncrypted(na)) {
		errno = EACCES;
		ntfs_log_trace("Failed (encrypted).\n");
		return -1;
	}
	/*
	 * TODO: Implement making handling of compressed attributes.
	 */
	if (NAttrCompressed(na)) {
		errno = EOPNOTSUPP;
		ntfs_log_trace("Failed (compressed).\n");
		return -1;
	}
	if (NAttrNonResident(na)) {
		if (newsize > na->data_size)
			ret = ntfs_non_resident_attr_expand(na, newsize,
					sparse);
		else
			ret = ntfs_non_resident_attr_shrink(na, newsize);
	} else
		ret = ntfs_resident_attr_resize(na, newsize);
	if (!ret)
		ntfs_log_trace("Done!\n");
	else
		ntfs_log_trace("Failed.\n");
	return ret;
}


/**
 * Wrapper around __ntfs_attr_truncate that always tries to creates hole
 */
int ntfs_attr_truncate(ntfs_attr *na, const s64 newsize)
{
	return __ntfs_attr_truncate(na, newsize, TRUE);
}


/**
 * ntfs_attr_readall - read the entire data from an ntfs attribute
 * @ni:		open ntfs inode in which the ntfs attribute resides
 * @type:	attribute type
 * @name:	attribute name in little endian Unicode or AT_UNNAMED or NULL
 * @name_len:	length of attribute @name in Unicode characters (if @name given)
 * @data_size:	if non-NULL then store here the data size
 *
 * This function will read the entire content of an ntfs attribute.
 * If @name is AT_UNNAMED then look specifically for an unnamed attribute.
 * If @name is NULL then the attribute could be either named or not.
 * In both those cases @name_len is not used at all.
 *
 * On success a buffer is allocated with the content of the attribute
 * and which needs to be freed when it's not needed anymore. If the
 * @data_size parameter is non-NULL then the data size is set there.
 *
 * On error NULL is returned with errno set to the error code.
 */
void *ntfs_attr_readall(ntfs_inode *ni, const ATTR_TYPES type,
			ntfschar *name, u32 name_len, s64 *data_size)
{
	ntfs_attr *na;
	void *data, *ret = NULL;
	s64 size;

	na = ntfs_attr_open(ni, type, name, name_len);
	if (!na) {
		ntfs_log_perror("ntfs_attr_open failed");
		return NULL;
	}
	data = ntfs_malloc(na->data_size);
	if (!data)
		goto out;

	size = ntfs_attr_pread(na, 0, na->data_size, data);
	if (size != na->data_size) {
		ntfs_log_perror("ntfs_attr_pread failed");
		free(data);
		goto out;
	}
	ret = data;
	if (data_size)
		*data_size = size;
out:
	ntfs_attr_close(na);
	return ret;
}

/**
 * ntfs_attr_exist - FIXME: description
 */
int ntfs_attr_exist(ntfs_inode *ni, const ATTR_TYPES type, ntfschar *name,
		u32 name_len)
{
	ntfs_attr_search_ctx *ctx;
	int ret;

	ntfs_log_trace("Entering.\n");

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return 0;

	ret = ntfs_attr_lookup(type, name, name_len, CASE_SENSITIVE, 0, NULL, 0,
			       ctx);

	ntfs_attr_put_search_ctx(ctx);
	return !ret;
}
