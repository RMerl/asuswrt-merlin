/**
 * dir.c - Directory handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2004-2008 Szabolcs Szakacsits
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 * Copyright (c) 2008-2010 Jean-Pierre Andre
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

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#include "param.h"
#include "types.h"
#include "debug.h"
#include "attrib.h"
#include "inode.h"
#include "dir.h"
#include "volume.h"
#include "mft.h"
#include "index.h"
#include "ntfstime.h"
#include "lcnalloc.h"
#include "logging.h"
#include "cache.h"
#include "misc.h"
#include "security.h"
#include "reparse.h"
#include "object_id.h"

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

/*
 * The little endian Unicode strings "$I30", "$SII", "$SDH", "$O"
 *  and "$Q" as global constants.
 */
ntfschar NTFS_INDEX_I30[5] = { const_cpu_to_le16('$'), const_cpu_to_le16('I'),
		const_cpu_to_le16('3'), const_cpu_to_le16('0'),
		const_cpu_to_le16('\0') };
ntfschar NTFS_INDEX_SII[5] = { const_cpu_to_le16('$'), const_cpu_to_le16('S'),
		const_cpu_to_le16('I'), const_cpu_to_le16('I'),
		const_cpu_to_le16('\0') };
ntfschar NTFS_INDEX_SDH[5] = { const_cpu_to_le16('$'), const_cpu_to_le16('S'),
		const_cpu_to_le16('D'), const_cpu_to_le16('H'),
		const_cpu_to_le16('\0') };
ntfschar NTFS_INDEX_O[3] = { const_cpu_to_le16('$'), const_cpu_to_le16('O'),
		const_cpu_to_le16('\0') };
ntfschar NTFS_INDEX_Q[3] = { const_cpu_to_le16('$'), const_cpu_to_le16('Q'),
		const_cpu_to_le16('\0') };
ntfschar NTFS_INDEX_R[3] = { const_cpu_to_le16('$'), const_cpu_to_le16('R'),
		const_cpu_to_le16('\0') };

#if CACHE_INODE_SIZE

/*
 *		Pathname hashing
 *
 *	Based on first char and second char (which may be '\0')
 */

int ntfs_dir_inode_hash(const struct CACHED_GENERIC *cached)
{
	const char *path;
	const unsigned char *name;

	path = (const char*)cached->variable;
	if (!path) {
		ntfs_log_error("Bad inode cache entry\n");
		return (-1);
	}
	name = (const unsigned char*)strrchr(path,'/');
	if (!name)
		name = (const unsigned char*)path;
	return (((name[0] << 1) + name[1] + strlen((const char*)name))
				% (2*CACHE_INODE_SIZE));
}

/*
 *		Pathname comparing for entering/fetching from cache
 */

static int inode_cache_compare(const struct CACHED_GENERIC *cached,
			const struct CACHED_GENERIC *wanted)
{
	return (!cached->variable
		    || strcmp(cached->variable, wanted->variable));
}

/*
 *		Pathname comparing for invalidating entries in cache
 *
 *	A partial path is compared in order to invalidate all paths
 *	related to a renamed directory
 *	inode numbers are also checked, as deleting a long name may
 *	imply deleting a short name and conversely
 *
 *	Only use associated with a CACHE_NOHASH flag
 */

static int inode_cache_inv_compare(const struct CACHED_GENERIC *cached,
			const struct CACHED_GENERIC *wanted)
{
	int len;
	BOOL different;
	const struct CACHED_INODE *w;
	const struct CACHED_INODE *c;

	w = (const struct CACHED_INODE*)wanted;
	c = (const struct CACHED_INODE*)cached;
	if (w->pathname) {
		len = strlen(w->pathname);
		different = !cached->variable
			|| ((w->inum != MREF(c->inum))
			   && (strncmp(c->pathname, w->pathname, len)
				|| ((c->pathname[len] != '\0')
				   && (c->pathname[len] != '/'))));
	} else
		different = !c->pathname
			|| (w->inum != MREF(c->inum));
	return (different);
}

#endif

#if CACHE_LOOKUP_SIZE

/*
 *		File name comparing for entering/fetching from lookup cache
 */

static int lookup_cache_compare(const struct CACHED_GENERIC *cached,
			const struct CACHED_GENERIC *wanted)
{
	const struct CACHED_LOOKUP *c = (const struct CACHED_LOOKUP*) cached;
	const struct CACHED_LOOKUP *w = (const struct CACHED_LOOKUP*) wanted;
	return (!c->name
		    || (c->parent != w->parent)
		    || (c->namesize != w->namesize)
		    || memcmp(c->name, w->name, c->namesize));
}

/*
 *		Inode number comparing for invalidating lookup cache
 *
 *	All entries with designated inode number are invalidated
 *
 *	Only use associated with a CACHE_NOHASH flag
 */

static int lookup_cache_inv_compare(const struct CACHED_GENERIC *cached,
			const struct CACHED_GENERIC *wanted)
{
	const struct CACHED_LOOKUP *c = (const struct CACHED_LOOKUP*) cached;
	const struct CACHED_LOOKUP *w = (const struct CACHED_LOOKUP*) wanted;
	return (!c->name
		    || (c->parent != w->parent)
		    || (MREF(c->inum) != MREF(w->inum)));
}

/*
 *		Lookup hashing
 *
 *	Based on first, second and and last char
 */

int ntfs_dir_lookup_hash(const struct CACHED_GENERIC *cached)
{
	const unsigned char *name;
	int count;
	unsigned int val;

	name = (const unsigned char*)cached->variable;
	count = cached->varsize;
	if (!name || !count) {
		ntfs_log_error("Bad lookup cache entry\n");
		return (-1);
	}
	val = (name[0] << 2) + (name[1] << 1) + name[count - 1] + count;
	return (val % (2*CACHE_LOOKUP_SIZE));
}

#endif

/**
 * ntfs_inode_lookup_by_name - find an inode in a directory given its name
 * @dir_ni:	ntfs inode of the directory in which to search for the name
 * @uname:	Unicode name for which to search in the directory
 * @uname_len:	length of the name @uname in Unicode characters
 *
 * Look for an inode with name @uname in the directory with inode @dir_ni.
 * ntfs_inode_lookup_by_name() walks the contents of the directory looking for
 * the Unicode name. If the name is found in the directory, the corresponding
 * inode number (>= 0) is returned as a mft reference in cpu format, i.e. it
 * is a 64-bit number containing the sequence number.
 *
 * On error, return -1 with errno set to the error code. If the inode is is not
 * found errno is ENOENT.
 *
 * Note, @uname_len does not include the (optional) terminating NULL character.
 *
 * Note, we look for a case sensitive match first but we also look for a case
 * insensitive match at the same time. If we find a case insensitive match, we
 * save that for the case that we don't find an exact match, where we return
 * the mft reference of the case insensitive match.
 *
 * If the volume is mounted with the case sensitive flag set, then we only
 * allow exact matches.
 */
u64 ntfs_inode_lookup_by_name(ntfs_inode *dir_ni,
		const ntfschar *uname, const int uname_len)
{
	VCN vcn;
	u64 mref = 0;
	s64 br;
	ntfs_volume *vol = dir_ni->vol;
	ntfs_attr_search_ctx *ctx;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia;
	IGNORE_CASE_BOOL case_sensitivity;
	u8 *index_end;
	ntfs_attr *ia_na;
	int eo, rc;
	u32 index_block_size, index_vcn_size;
	u8 index_vcn_size_bits;

	ntfs_log_trace("Entering\n");

	if (!dir_ni || !dir_ni->mrec || !uname || uname_len <= 0) {
		errno = EINVAL;
		return -1;
	}

	ctx = ntfs_attr_get_search_ctx(dir_ni, NULL);
	if (!ctx)
		return -1;

	/* Find the index root attribute in the mft record. */
	if (ntfs_attr_lookup(AT_INDEX_ROOT, NTFS_INDEX_I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx)) {
		ntfs_log_perror("Index root attribute missing in directory inode "
				"%lld", (unsigned long long)dir_ni->mft_no);
		goto put_err_out;
	}
	case_sensitivity = (NVolCaseSensitive(vol) ? CASE_SENSITIVE : IGNORE_CASE);
	/* Get to the index root value. */
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	index_block_size = le32_to_cpu(ir->index_block_size);
	if (index_block_size < NTFS_BLOCK_SIZE ||
			index_block_size & (index_block_size - 1)) {
		ntfs_log_error("Index block size %u is invalid.\n",
				(unsigned)index_block_size);
		goto put_err_out;
	}
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		/* Bounds checks. */
		if ((u8*)ie < (u8*)ctx->mrec || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end) {
			ntfs_log_error("Index entry out of bounds in inode %lld"
				       "\n", (unsigned long long)dir_ni->mft_no);
			goto put_err_out;
		}
		/*
		 * The last entry cannot contain a name. It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->ie_flags & INDEX_ENTRY_END)
			break;
		
		if (!le16_to_cpu(ie->length)) {
			ntfs_log_error("Zero length index entry in inode %lld"
				       "\n", (unsigned long long)dir_ni->mft_no);
			goto put_err_out;
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_names_full_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				case_sensitivity, vol->upcase, vol->upcase_len);
		/*
		 * If uname collates before the name of the current entry, there
		 * is definitely no such name in this index but we might need to
		 * descend into the B+tree so we just break out of the loop.
		 */
		if (rc == -1)
			break;
		/* The names are not equal, continue the search. */
		if (rc)
			continue;
		/*
		 * Perfect match, this will never happen as the
		 * ntfs_are_names_equal() call will have gotten a match but we
		 * still treat it correctly.
		 */
		mref = le64_to_cpu(ie->indexed_file);
		ntfs_attr_put_search_ctx(ctx);
		return mref;
	}
	/*
	 * We have finished with this index without success. Check for the
	 * presence of a child node and if not present return error code
	 * ENOENT, unless we have got the mft reference of a matching name
	 * cached in mref in which case return mref.
	 */
	if (!(ie->ie_flags & INDEX_ENTRY_NODE)) {
		ntfs_attr_put_search_ctx(ctx);
		if (mref)
			return mref;
		ntfs_log_debug("Entry not found.\n");
		errno = ENOENT;
		return -1;
	} /* Child node present, descend into it. */

	/* Open the index allocation attribute. */
	ia_na = ntfs_attr_open(dir_ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
	if (!ia_na) {
		ntfs_log_perror("Failed to open index allocation (inode %lld)",
				(unsigned long long)dir_ni->mft_no);
		goto put_err_out;
	}

	/* Allocate a buffer for the current index block. */
	ia = ntfs_malloc(index_block_size);
	if (!ia) {
		ntfs_attr_close(ia_na);
		goto put_err_out;
	}

	/* Determine the size of a vcn in the directory index. */
	if (vol->cluster_size <= index_block_size) {
		index_vcn_size = vol->cluster_size;
		index_vcn_size_bits = vol->cluster_size_bits;
	} else {
		index_vcn_size = vol->sector_size;
		index_vcn_size_bits = vol->sector_size_bits;
	}

	/* Get the starting vcn of the index_block holding the child node. */
	vcn = sle64_to_cpup((u8*)ie + le16_to_cpu(ie->length) - 8);

descend_into_child_node:

	/* Read the index block starting at vcn. */
	br = ntfs_attr_mst_pread(ia_na, vcn << index_vcn_size_bits, 1,
			index_block_size, ia);
	if (br != 1) {
		if (br != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read vcn 0x%llx",
			       	(unsigned long long)vcn);
		goto close_err_out;
	}

	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_log_error("Actual VCN (0x%llx) of index buffer is different "
				"from expected VCN (0x%llx).\n",
				(long long)sle64_to_cpu(ia->index_block_vcn),
				(long long)vcn);
		errno = EIO;
		goto close_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 != index_block_size) {
		ntfs_log_error("Index buffer (VCN 0x%llx) of directory inode 0x%llx "
				"has a size (%u) differing from the directory "
				"specified size (%u).\n", (long long)vcn,
				(unsigned long long)dir_ni->mft_no,
				(unsigned) le32_to_cpu(ia->index.allocated_size) + 0x18,
				(unsigned)index_block_size);
		errno = EIO;
		goto close_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + index_block_size) {
		ntfs_log_error("Size of index buffer (VCN 0x%llx) of directory inode "
				"0x%llx exceeds maximum size.\n",
				(long long)vcn, (unsigned long long)dir_ni->mft_no);
		errno = EIO;
		goto close_err_out;
	}

	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/*
	 * Iterate similar to above big loop but applied to index buffer, thus
	 * loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		/* Bounds check. */
		if ((u8*)ie < (u8*)ia || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end) {
			ntfs_log_error("Index entry out of bounds in directory "
				       "inode %lld.\n", 
				       (unsigned long long)dir_ni->mft_no);
			errno = EIO;
			goto close_err_out;
		}
		/*
		 * The last entry cannot contain a name. It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->ie_flags & INDEX_ENTRY_END)
			break;
		
		if (!le16_to_cpu(ie->length)) {
			errno = EIO;
			ntfs_log_error("Zero length index entry in inode %lld"
				       "\n", (unsigned long long)dir_ni->mft_no);
			goto close_err_out;
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_names_full_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				case_sensitivity, vol->upcase, vol->upcase_len);
		/*
		 * If uname collates before the name of the current entry, there
		 * is definitely no such name in this index but we might need to
		 * descend into the B+tree so we just break out of the loop.
		 */
		if (rc == -1)
			break;
		/* The names are not equal, continue the search. */
		if (rc)
			continue;
		mref = le64_to_cpu(ie->indexed_file);
		free(ia);
		ntfs_attr_close(ia_na);
		ntfs_attr_put_search_ctx(ctx);
		return mref;
	}
	/*
	 * We have finished with this index buffer without success. Check for
	 * the presence of a child node.
	 */
	if (ie->ie_flags & INDEX_ENTRY_NODE) {
		if ((ia->index.ih_flags & NODE_MASK) == LEAF_NODE) {
			ntfs_log_error("Index entry with child node found in a leaf "
					"node in directory inode %lld.\n",
					(unsigned long long)dir_ni->mft_no);
			errno = EIO;
			goto close_err_out;
		}
		/* Child node present, descend into it. */
		vcn = sle64_to_cpup((u8*)ie + le16_to_cpu(ie->length) - 8);
		if (vcn >= 0)
			goto descend_into_child_node;
		ntfs_log_error("Negative child node vcn in directory inode "
			       "0x%llx.\n", (unsigned long long)dir_ni->mft_no);
		errno = EIO;
		goto close_err_out;
	}
	free(ia);
	ntfs_attr_close(ia_na);
	ntfs_attr_put_search_ctx(ctx);
	/*
	 * No child node present, return error code ENOENT, unless we have got
	 * the mft reference of a matching name cached in mref in which case
	 * return mref.
	 */
	if (mref)
		return mref;
	ntfs_log_debug("Entry not found.\n");
	errno = ENOENT;
	return -1;
put_err_out:
	eo = EIO;
	ntfs_log_debug("Corrupt directory. Aborting lookup.\n");
eo_put_err_out:
	ntfs_attr_put_search_ctx(ctx);
	errno = eo;
	return -1;
close_err_out:
	eo = errno;
	free(ia);
	ntfs_attr_close(ia_na);
	goto eo_put_err_out;
}

/*
 *		Lookup a file in a directory from its UTF-8 name
 *
 *	The name is first fetched from cache if one is defined
 *
 *	Returns the inode number
 *		or -1 if not possible (errno tells why)
 */

u64 ntfs_inode_lookup_by_mbsname(ntfs_inode *dir_ni, const char *name)
{
	int uname_len;
	ntfschar *uname = (ntfschar*)NULL;
	u64 inum;
	char *cached_name;
	const char *const_name;

	if (!NVolCaseSensitive(dir_ni->vol)) {
		cached_name = ntfs_uppercase_mbs(name,
			dir_ni->vol->upcase, dir_ni->vol->upcase_len);
		const_name = cached_name;
	} else {
		cached_name = (char*)NULL;
		const_name = name;
	}
	if (const_name) {
#if CACHE_LOOKUP_SIZE

		/*
		 * fetch inode from cache
		 */

		if (dir_ni->vol->lookup_cache) {
			struct CACHED_LOOKUP item;
			struct CACHED_LOOKUP *cached;

			item.name = const_name;
			item.namesize = strlen(const_name) + 1;
			item.parent = dir_ni->mft_no;
			cached = (struct CACHED_LOOKUP*)ntfs_fetch_cache(
					dir_ni->vol->lookup_cache,
					GENERIC(&item), lookup_cache_compare);
			if (cached) {
				inum = cached->inum;
				if (inum == (u64)-1)
					errno = ENOENT;
			} else {
				/* Generate unicode name. */
				uname_len = ntfs_mbstoucs(name, &uname);
				if (uname_len >= 0) {
					inum = ntfs_inode_lookup_by_name(dir_ni,
							uname, uname_len);
					item.inum = inum;
				/* enter into cache, even if not found */
					ntfs_enter_cache(dir_ni->vol->lookup_cache,
							GENERIC(&item),
							lookup_cache_compare);
					free(uname);
				} else
					inum = (s64)-1;
			}
		} else
#endif
			{
				/* Generate unicode name. */
			uname_len = ntfs_mbstoucs(cached_name, &uname);
			if (uname_len >= 0)
				inum = ntfs_inode_lookup_by_name(dir_ni,
						uname, uname_len);
			else
				inum = (s64)-1;
		}
		if (cached_name)
			free(cached_name);
	} else
		inum = (s64)-1;
	return (inum);
}

/*
 *		Update a cache lookup record when a name has been defined
 *
 *	The UTF-8 name is required
 */

void ntfs_inode_update_mbsname(ntfs_inode *dir_ni, const char *name, u64 inum)
{
#if CACHE_LOOKUP_SIZE
	struct CACHED_LOOKUP item;
	struct CACHED_LOOKUP *cached;
	char *cached_name;

	if (dir_ni->vol->lookup_cache) {
		if (!NVolCaseSensitive(dir_ni->vol)) {
			cached_name = ntfs_uppercase_mbs(name,
				dir_ni->vol->upcase, dir_ni->vol->upcase_len);
			item.name = cached_name;
		} else {
			cached_name = (char*)NULL;
			item.name = name;
		}
		if (item.name) {
			item.namesize = strlen(item.name) + 1;
			item.parent = dir_ni->mft_no;
			item.inum = inum;
			cached = (struct CACHED_LOOKUP*)ntfs_enter_cache(
					dir_ni->vol->lookup_cache,
					GENERIC(&item), lookup_cache_compare);
			if (cached)
				cached->inum = inum;
			if (cached_name)
				free(cached_name);
		}
	}
#endif
}

/**
 * ntfs_pathname_to_inode - Find the inode which represents the given pathname
 * @vol:       An ntfs volume obtained from ntfs_mount
 * @parent:    A directory inode to begin the search (may be NULL)
 * @pathname:  Pathname to be located
 *
 * Take an ASCII pathname and find the inode that represents it.  The function
 * splits the path and then descends the directory tree.  If @parent is NULL,
 * then the root directory '.' will be used as the base for the search.
 *
 * Return:  inode  Success, the pathname was valid
 *	    NULL   Error, the pathname was invalid, or some other error occurred
 */
ntfs_inode *ntfs_pathname_to_inode(ntfs_volume *vol, ntfs_inode *parent,
		const char *pathname)
{
	u64 inum;
	int len, err = 0;
	char *p, *q;
	ntfs_inode *ni;
	ntfs_inode *result = NULL;
	ntfschar *unicode = NULL;
	char *ascii = NULL;
#if CACHE_INODE_SIZE
	struct CACHED_INODE item;
	struct CACHED_INODE *cached;
	char *fullname;
#endif

	if (!vol || !pathname) {
		errno = EINVAL;
		return NULL;
	}
	
	ntfs_log_trace("path: '%s'\n", pathname);
	
	ascii = strdup(pathname);
	if (!ascii) {
		ntfs_log_error("Out of memory.\n");
		err = ENOMEM;
		goto out;
	}

	p = ascii;
	/* Remove leading /'s. */
	while (p && *p && *p == PATH_SEP)
		p++;
#if CACHE_INODE_SIZE
	fullname = p;
	if (p[0] && (p[strlen(p)-1] == PATH_SEP))
		ntfs_log_error("Unnormalized path %s\n",ascii);
#endif
	if (parent) {
		ni = parent;
	} else {
#if CACHE_INODE_SIZE
			/*
			 * fetch inode for full path from cache
			 */
		if (*fullname) {
			item.pathname = fullname;
			item.varsize = strlen(fullname) + 1;
			cached = (struct CACHED_INODE*)ntfs_fetch_cache(
				vol->xinode_cache, GENERIC(&item),
				inode_cache_compare);
		} else
			cached = (struct CACHED_INODE*)NULL;
		if (cached) {
			/*
			 * return opened inode if found in cache
			 */
			inum = MREF(cached->inum);
			ni = ntfs_inode_open(vol, inum);
			if (!ni) {
				ntfs_log_debug("Cannot open inode %llu: %s.\n",
						(unsigned long long)inum, p);
				err = EIO;
			}
			result = ni;
			goto out;
		}
#endif
		ni = ntfs_inode_open(vol, FILE_root);
		if (!ni) {
			ntfs_log_debug("Couldn't open the inode of the root "
					"directory.\n");
			err = EIO;
			result = (ntfs_inode*)NULL;
			goto out;
		}
	}

	while (p && *p) {
		/* Find the end of the first token. */
		q = strchr(p, PATH_SEP);
		if (q != NULL) {
			*q = '\0';
		}
#if CACHE_INODE_SIZE
			/*
			 * fetch inode for partial path from cache
			 */
		cached = (struct CACHED_INODE*)NULL;
		if (!parent) {
			item.pathname = fullname;
			item.varsize = strlen(fullname) + 1;
			cached = (struct CACHED_INODE*)ntfs_fetch_cache(
					vol->xinode_cache, GENERIC(&item),
					inode_cache_compare);
			if (cached) {
				inum = cached->inum;
			}
		}
			/*
			 * if not in cache, translate, search, then
			 * insert into cache if found
			 */
		if (!cached) {
			len = ntfs_mbstoucs(p, &unicode);
			if (len < 0) {
				ntfs_log_perror("Could not convert filename to Unicode:"
					" '%s'", p);
				err = errno;
				goto close;
			} else if (len > NTFS_MAX_NAME_LEN) {
				err = ENAMETOOLONG;
				goto close;
			}
			inum = ntfs_inode_lookup_by_name(ni, unicode, len);
			if (!parent && (inum != (u64) -1)) {
				item.inum = inum;
				ntfs_enter_cache(vol->xinode_cache,
						GENERIC(&item),
						inode_cache_compare);
			}
		}
#else
		len = ntfs_mbstoucs(p, &unicode);
		if (len < 0) {
			ntfs_log_perror("Could not convert filename to Unicode:"
					" '%s'", p);
			err = errno;
			goto close;
		} else if (len > NTFS_MAX_NAME_LEN) {
			err = ENAMETOOLONG;
			goto close;
		}
		inum = ntfs_inode_lookup_by_name(ni, unicode, len);
#endif
		if (inum == (u64) -1) {
			ntfs_log_debug("Couldn't find name '%s' in pathname "
					"'%s'.\n", p, pathname);
			err = ENOENT;
			goto close;
		}

		if (ni != parent)
			if (ntfs_inode_close(ni)) {
				err = errno;
				goto out;
			}

		inum = MREF(inum);
		ni = ntfs_inode_open(vol, inum);
		if (!ni) {
			ntfs_log_debug("Cannot open inode %llu: %s.\n",
					(unsigned long long)inum, p);
			err = EIO;
			goto close;
		}
	
		free(unicode);
		unicode = NULL;

		if (q) *q++ = PATH_SEP; /* JPA */
		p = q;
		while (p && *p && *p == PATH_SEP)
			p++;
	}

	result = ni;
	ni = NULL;
close:
	if (ni && (ni != parent))
		if (ntfs_inode_close(ni) && !err)
			err = errno;
out:
	free(ascii);
	free(unicode);
	if (err)
		errno = err;
	return result;
}

/*
 * The little endian Unicode string ".." for ntfs_readdir().
 */
static const ntfschar dotdot[3] = { const_cpu_to_le16('.'),
				   const_cpu_to_le16('.'),
				   const_cpu_to_le16('\0') };

/*
 * union index_union -
 * More helpers for ntfs_readdir().
 */
typedef union {
	INDEX_ROOT *ir;
	INDEX_ALLOCATION *ia;
} index_union __attribute__((__transparent_union__));

/**
 * enum INDEX_TYPE -
 * More helpers for ntfs_readdir().
 */
typedef enum {
	INDEX_TYPE_ROOT,	/* index root */
	INDEX_TYPE_ALLOCATION,	/* index allocation */
} INDEX_TYPE;

/**
 * ntfs_filldir - ntfs specific filldir method
 * @dir_ni:	ntfs inode of current directory
 * @pos:	current position in directory
 * @ivcn_bits:	log(2) of index vcn size
 * @index_type:	specifies whether @iu is an index root or an index allocation
 * @iu:		index root or index block to which @ie belongs
 * @ie:		current index entry
 * @dirent:	context for filldir callback supplied by the caller
 * @filldir:	filldir callback supplied by the caller
 *
 * Pass information specifying the current directory entry @ie to the @filldir
 * callback.
 */
static int ntfs_filldir(ntfs_inode *dir_ni, s64 *pos, u8 ivcn_bits,
		const INDEX_TYPE index_type, index_union iu, INDEX_ENTRY *ie,
		void *dirent, ntfs_filldir_t filldir)
{
	FILE_NAME_ATTR *fn = &ie->key.file_name;
	unsigned dt_type;
	BOOL metadata;
	ntfschar *loname;
	int res;
	MFT_REF mref;

	ntfs_log_trace("Entering.\n");
	
	/* Advance the position even if going to skip the entry. */
	if (index_type == INDEX_TYPE_ALLOCATION)
		*pos = (u8*)ie - (u8*)iu.ia + (sle64_to_cpu(
				iu.ia->index_block_vcn) << ivcn_bits) +
				dir_ni->vol->mft_record_size;
	else /* if (index_type == INDEX_TYPE_ROOT) */
		*pos = (u8*)ie - (u8*)iu.ir;
	/* Skip root directory self reference entry. */
	if (MREF_LE(ie->indexed_file) == FILE_root)
		return 0;
	if (ie->key.file_name.file_attributes & FILE_ATTR_I30_INDEX_PRESENT)
		dt_type = NTFS_DT_DIR;
	else if (fn->file_attributes & FILE_ATTR_SYSTEM)
		dt_type = NTFS_DT_UNKNOWN;
	else
		dt_type = NTFS_DT_REG;

		/* return metadata files and hidden files if requested */
	mref = le64_to_cpu(ie->indexed_file);
        metadata = (MREF(mref) != FILE_root) && (MREF(mref) < FILE_first_user);
        if ((!metadata && (NVolShowHidFiles(dir_ni->vol)
				|| !(fn->file_attributes & FILE_ATTR_HIDDEN)))
            || (NVolShowSysFiles(dir_ni->vol) && (NVolShowHidFiles(dir_ni->vol)
				|| metadata))) {
		if (NVolCaseSensitive(dir_ni->vol)) {
			res = filldir(dirent, fn->file_name,
					fn->file_name_length,
					fn->file_name_type, *pos,
					mref, dt_type);
		} else {
			loname = (ntfschar*)ntfs_malloc(2*fn->file_name_length);
			if (loname) {
				memcpy(loname, fn->file_name,
					2*fn->file_name_length);
				ntfs_name_locase(loname, fn->file_name_length,
					dir_ni->vol->locase,
					dir_ni->vol->upcase_len);
				res = filldir(dirent, loname,
					fn->file_name_length,
					fn->file_name_type, *pos,
					mref, dt_type);
				free(loname);
			} else
				res = -1;
		}
	} else
		res = 0;
	return (res);
}

/**
 * ntfs_mft_get_parent_ref - find mft reference of parent directory of an inode
 * @ni:		ntfs inode whose parent directory to find
 *
 * Find the parent directory of the ntfs inode @ni. To do this, find the first
 * file name attribute in the mft record of @ni and return the parent mft
 * reference from that.
 *
 * Note this only makes sense for directories, since files can be hard linked
 * from multiple directories and there is no way for us to tell which one is
 * being looked for.
 *
 * Technically directories can have hard links, too, but we consider that as
 * illegal as Linux/UNIX do not support directory hard links.
 *
 * Return the mft reference of the parent directory on success or -1 on error
 * with errno set to the error code.
 */
static MFT_REF ntfs_mft_get_parent_ref(ntfs_inode *ni)
{
	MFT_REF mref;
	ntfs_attr_search_ctx *ctx;
	FILE_NAME_ATTR *fn;
	int eo;

	ntfs_log_trace("Entering.\n");
	
	if (!ni) {
		errno = EINVAL;
		return ERR_MREF(-1);
	}

	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return ERR_MREF(-1);
	if (ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, 0, 0, NULL, 0, ctx)) {
		ntfs_log_error("No file name found in inode %lld\n", 
			       (unsigned long long)ni->mft_no);
		goto err_out;
	}
	if (ctx->attr->non_resident) {
		ntfs_log_error("File name attribute must be resident (inode "
			       "%lld)\n", (unsigned long long)ni->mft_no);
		goto io_err_out;
	}
	fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	if ((u8*)fn +	le32_to_cpu(ctx->attr->value_length) >
			(u8*)ctx->attr + le32_to_cpu(ctx->attr->length)) {
		ntfs_log_error("Corrupt file name attribute in inode %lld.\n",
			       (unsigned long long)ni->mft_no);
		goto io_err_out;
	}
	mref = le64_to_cpu(fn->parent_directory);
	ntfs_attr_put_search_ctx(ctx);
	return mref;
io_err_out:
	errno = EIO;
err_out:
	eo = errno;
	ntfs_attr_put_search_ctx(ctx);
	errno = eo;
	return ERR_MREF(-1);
}

/**
 * ntfs_readdir - read the contents of an ntfs directory
 * @dir_ni:	ntfs inode of current directory
 * @pos:	current position in directory
 * @dirent:	context for filldir callback supplied by the caller
 * @filldir:	filldir callback supplied by the caller
 *
 * Parse the index root and the index blocks that are marked in use in the
 * index bitmap and hand each found directory entry to the @filldir callback
 * supplied by the caller.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 *
 * Note: Index blocks are parsed in ascending vcn order, from which follows
 * that the directory entries are not returned sorted.
 */
int ntfs_readdir(ntfs_inode *dir_ni, s64 *pos,
		void *dirent, ntfs_filldir_t filldir)
{
	s64 i_size, br, ia_pos, bmp_pos, ia_start;
	ntfs_volume *vol;
	ntfs_attr *ia_na, *bmp_na = NULL;
	ntfs_attr_search_ctx *ctx = NULL;
	u8 *index_end, *bmp = NULL;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia = NULL;
	int rc, ir_pos, bmp_buf_size, bmp_buf_pos, eo;
	u32 index_block_size, index_vcn_size;
	u8 index_block_size_bits, index_vcn_size_bits;

	ntfs_log_trace("Entering.\n");
	
	if (!dir_ni || !pos || !filldir) {
		errno = EINVAL;
		return -1;
	}

	if (!(dir_ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)) {
		errno = ENOTDIR;
		return -1;
	}

	vol = dir_ni->vol;

	ntfs_log_trace("Entering for inode %lld, *pos 0x%llx.\n",
			(unsigned long long)dir_ni->mft_no, (long long)*pos);

	/* Open the index allocation attribute. */
	ia_na = ntfs_attr_open(dir_ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
	if (!ia_na) {
		if (errno != ENOENT) {
			ntfs_log_perror("Failed to open index allocation attribute. "
				"Directory inode %lld is corrupt or bug",
				(unsigned long long)dir_ni->mft_no);
			return -1;
		}
		i_size = 0;
	} else
		i_size = ia_na->data_size;

	rc = 0;

	/* Are we at end of dir yet? */
	if (*pos >= i_size + vol->mft_record_size)
		goto done;

	/* Emulate . and .. for all directories. */
	if (!*pos) {
		rc = filldir(dirent, dotdot, 1, FILE_NAME_POSIX, *pos,
				MK_MREF(dir_ni->mft_no,
				le16_to_cpu(dir_ni->mrec->sequence_number)),
				NTFS_DT_DIR);
		if (rc)
			goto err_out;
		++*pos;
	}
	if (*pos == 1) {
		MFT_REF parent_mref;

		parent_mref = ntfs_mft_get_parent_ref(dir_ni);
		if (parent_mref == ERR_MREF(-1)) {
			ntfs_log_perror("Parent directory not found");
			goto dir_err_out;
		}

		rc = filldir(dirent, dotdot, 2, FILE_NAME_POSIX, *pos,
				parent_mref, NTFS_DT_DIR);
		if (rc)
			goto err_out;
		++*pos;
	}

	ctx = ntfs_attr_get_search_ctx(dir_ni, NULL);
	if (!ctx)
		goto err_out;

	/* Get the offset into the index root attribute. */
	ir_pos = (int)*pos;
	/* Find the index root attribute in the mft record. */
	if (ntfs_attr_lookup(AT_INDEX_ROOT, NTFS_INDEX_I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx)) {
		ntfs_log_perror("Index root attribute missing in directory inode "
				"%lld", (unsigned long long)dir_ni->mft_no);
		goto dir_err_out;
	}
	/* Get to the index root value. */
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));

	/* Determine the size of a vcn in the directory index. */
	index_block_size = le32_to_cpu(ir->index_block_size);
	if (index_block_size < NTFS_BLOCK_SIZE ||
			index_block_size & (index_block_size - 1)) {
		ntfs_log_error("Index block size %u is invalid.\n",
				(unsigned)index_block_size);
		goto dir_err_out;
	}
	index_block_size_bits = ffs(index_block_size) - 1;
	if (vol->cluster_size <= index_block_size) {
		index_vcn_size = vol->cluster_size;
		index_vcn_size_bits = vol->cluster_size_bits;
	} else {
		index_vcn_size = vol->sector_size;
		index_vcn_size_bits = vol->sector_size_bits;
	}

	/* Are we jumping straight into the index allocation attribute? */
	if (*pos >= vol->mft_record_size) {
		ntfs_attr_put_search_ctx(ctx);
		ctx = NULL;
		goto skip_index_root;
	}

	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry or until filldir tells us it has had enough
	 * or signals an error (both covered by the rc test).
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		ntfs_log_debug("In index root, offset %d.\n", (int)((u8*)ie - (u8*)ir));
		/* Bounds checks. */
		if ((u8*)ie < (u8*)ctx->mrec || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err_out;
		/* The last entry cannot contain a name. */
		if (ie->ie_flags & INDEX_ENTRY_END)
			break;
		
		if (!le16_to_cpu(ie->length))
			goto dir_err_out;
		
		/* Skip index root entry if continuing previous readdir. */
		if (ir_pos > (u8*)ie - (u8*)ir)
			continue;
		/*
		 * Submit the directory entry to ntfs_filldir(), which will
		 * invoke the filldir() callback as appropriate.
		 */
		rc = ntfs_filldir(dir_ni, pos, index_vcn_size_bits,
				INDEX_TYPE_ROOT, ir, ie, dirent, filldir);
		if (rc) {
			ntfs_attr_put_search_ctx(ctx);
			ctx = NULL;
			goto err_out;
		}
	}
	ntfs_attr_put_search_ctx(ctx);
	ctx = NULL;

	/* If there is no index allocation attribute we are finished. */
	if (!ia_na)
		goto EOD;

	/* Advance *pos to the beginning of the index allocation. */
	*pos = vol->mft_record_size;

skip_index_root:

	if (!ia_na)
		goto done;

	/* Allocate a buffer for the current index block. */
	ia = ntfs_malloc(index_block_size);
	if (!ia)
		goto err_out;

	bmp_na = ntfs_attr_open(dir_ni, AT_BITMAP, NTFS_INDEX_I30, 4);
	if (!bmp_na) {
		ntfs_log_perror("Failed to open index bitmap attribute");
		goto dir_err_out;
	}

	/* Get the offset into the index allocation attribute. */
	ia_pos = *pos - vol->mft_record_size;

	bmp_pos = ia_pos >> index_block_size_bits;
	if (bmp_pos >> 3 >= bmp_na->data_size) {
		ntfs_log_error("Current index position exceeds index bitmap "
				"size.\n");
		goto dir_err_out;
	}

	bmp_buf_size = min(bmp_na->data_size - (bmp_pos >> 3), 4096);
	bmp = ntfs_malloc(bmp_buf_size);
	if (!bmp)
		goto err_out;

	br = ntfs_attr_pread(bmp_na, bmp_pos >> 3, bmp_buf_size, bmp);
	if (br != bmp_buf_size) {
		if (br != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read from index bitmap attribute");
		goto err_out;
	}

	bmp_buf_pos = 0;
	/* If the index block is not in use find the next one that is. */
	while (!(bmp[bmp_buf_pos >> 3] & (1 << (bmp_buf_pos & 7)))) {
find_next_index_buffer:
		bmp_pos++;
		bmp_buf_pos++;
		/* If we have reached the end of the bitmap, we are done. */
		if (bmp_pos >> 3 >= bmp_na->data_size)
			goto EOD;
		ia_pos = bmp_pos << index_block_size_bits;
		if (bmp_buf_pos >> 3 < bmp_buf_size)
			continue;
		/* Read next chunk from the index bitmap. */
		bmp_buf_pos = 0;
		if ((bmp_pos >> 3) + bmp_buf_size > bmp_na->data_size)
			bmp_buf_size = bmp_na->data_size - (bmp_pos >> 3);
		br = ntfs_attr_pread(bmp_na, bmp_pos >> 3, bmp_buf_size, bmp);
		if (br != bmp_buf_size) {
			if (br != -1)
				errno = EIO;
			ntfs_log_perror("Failed to read from index bitmap attribute");
			goto err_out;
		}
	}

	ntfs_log_debug("Handling index block 0x%llx.\n", (long long)bmp_pos);

	/* Read the index block starting at bmp_pos. */
	br = ntfs_attr_mst_pread(ia_na, bmp_pos << index_block_size_bits, 1,
			index_block_size, ia);
	if (br != 1) {
		if (br != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read index block");
		goto err_out;
	}

	ia_start = ia_pos & ~(s64)(index_block_size - 1);
	if (sle64_to_cpu(ia->index_block_vcn) != ia_start >>
			index_vcn_size_bits) {
		ntfs_log_error("Actual VCN (0x%llx) of index buffer is different "
				"from expected VCN (0x%llx) in inode 0x%llx.\n",
				(long long)sle64_to_cpu(ia->index_block_vcn),
				(long long)ia_start >> index_vcn_size_bits,
				(unsigned long long)dir_ni->mft_no);
		goto dir_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 != index_block_size) {
		ntfs_log_error("Index buffer (VCN 0x%llx) of directory inode %lld "
				"has a size (%u) differing from the directory "
				"specified size (%u).\n", (long long)ia_start >>
				index_vcn_size_bits,
				(unsigned long long)dir_ni->mft_no,
				(unsigned) le32_to_cpu(ia->index.allocated_size)
				+ 0x18, (unsigned)index_block_size);
		goto dir_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + index_block_size) {
		ntfs_log_error("Size of index buffer (VCN 0x%llx) of directory inode "
				"%lld exceeds maximum size.\n",
				(long long)ia_start >> index_vcn_size_bits,
				(unsigned long long)dir_ni->mft_no);
		goto dir_err_out;
	}
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry or until ntfs_filldir tells us it has had
	 * enough or signals an error (both covered by the rc test).
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		ntfs_log_debug("In index allocation, offset 0x%llx.\n",
				(long long)ia_start + ((u8*)ie - (u8*)ia));
		/* Bounds checks. */
		if ((u8*)ie < (u8*)ia || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end) {
			ntfs_log_error("Index entry out of bounds in directory inode "
				"%lld.\n", (unsigned long long)dir_ni->mft_no);
			goto dir_err_out;
		}
		/* The last entry cannot contain a name. */
		if (ie->ie_flags & INDEX_ENTRY_END)
			break;
		
		if (!le16_to_cpu(ie->length))
			goto dir_err_out;
		
		/* Skip index entry if continuing previous readdir. */
		if (ia_pos - ia_start > (u8*)ie - (u8*)ia)
			continue;
		/*
		 * Submit the directory entry to ntfs_filldir(), which will
		 * invoke the filldir() callback as appropriate.
		 */
		rc = ntfs_filldir(dir_ni, pos, index_vcn_size_bits,
				INDEX_TYPE_ALLOCATION, ia, ie, dirent, filldir);
		if (rc)
			goto err_out;
	}
	goto find_next_index_buffer;
EOD:
	/* We are finished, set *pos to EOD. */
	*pos = i_size + vol->mft_record_size;
done:
	free(ia);
	free(bmp);
	if (bmp_na)
		ntfs_attr_close(bmp_na);
	if (ia_na)
		ntfs_attr_close(ia_na);
	ntfs_log_debug("EOD, *pos 0x%llx, returning 0.\n", (long long)*pos);
	return 0;
dir_err_out:
	errno = EIO;
err_out:
	eo = errno;
	ntfs_log_trace("failed.\n");
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	free(ia);
	free(bmp);
	if (bmp_na)
		ntfs_attr_close(bmp_na);
	if (ia_na)
		ntfs_attr_close(ia_na);
	errno = eo;
	return -1;
}


/**
 * __ntfs_create - create object on ntfs volume
 * @dir_ni:	ntfs inode for directory in which create new object
 * @securid:	id of inheritable security descriptor, 0 if none
 * @name:	unicode name of new object
 * @name_len:	length of the name in unicode characters
 * @type:	type of the object to create
 * @dev:	major and minor device numbers (obtained from makedev())
 * @target:	target in unicode (only for symlinks)
 * @target_len:	length of target in unicode characters
 *
 * Internal, use ntfs_create{,_device,_symlink} wrappers instead.
 *
 * @type can be:
 *	S_IFREG		to create regular file
 *	S_IFDIR		to create directory
 *	S_IFBLK		to create block device
 *	S_IFCHR		to create character device
 *	S_IFLNK		to create symbolic link
 *	S_IFIFO		to create FIFO
 *	S_IFSOCK	to create socket
 * other values are invalid.
 *
 * @dev is used only if @type is S_IFBLK or S_IFCHR, in other cases its value
 * ignored.
 *
 * @target and @target_len are used only if @type is S_IFLNK, in other cases
 * their value ignored.
 *
 * Return opened ntfs inode that describes created object on success or NULL
 * on error with errno set to the error code.
 */
static ntfs_inode *__ntfs_create(ntfs_inode *dir_ni, le32 securid,
		ntfschar *name, u8 name_len, mode_t type, dev_t dev,
		ntfschar *target, int target_len)
{
	ntfs_inode *ni;
	int rollback_data = 0, rollback_sd = 0;
	FILE_NAME_ATTR *fn = NULL;
	STANDARD_INFORMATION *si = NULL;
	int err, fn_len, si_len;

	ntfs_log_trace("Entering.\n");
	
	/* Sanity checks. */
	if (!dir_ni || !name || !name_len) {
		ntfs_log_error("Invalid arguments.\n");
		errno = EINVAL;
		return NULL;
	}
	
	if (dir_ni->flags & FILE_ATTR_REPARSE_POINT) {
		errno = EOPNOTSUPP;
		return NULL;
	}
	
	ni = ntfs_mft_record_alloc(dir_ni->vol, NULL);
	if (!ni)
		return NULL;
#if CACHE_NIDATA_SIZE
	ntfs_inode_invalidate(dir_ni->vol, ni->mft_no);
#endif
	/*
	 * Create STANDARD_INFORMATION attribute.
	 * JPA Depending on available inherited security descriptor,
	 * Write STANDARD_INFORMATION v1.2 (no inheritance) or v3
	 */
	if (securid)
		si_len = sizeof(STANDARD_INFORMATION);
	else
		si_len = offsetof(STANDARD_INFORMATION, v1_end);
	si = ntfs_calloc(si_len);
	if (!si) {
		err = errno;
		goto err_out;
	}
	si->creation_time = ni->creation_time;
	si->last_data_change_time = ni->last_data_change_time;
	si->last_mft_change_time = ni->last_mft_change_time;
	si->last_access_time = ni->last_access_time;
	if (securid) {
		set_nino_flag(ni, v3_Extensions);
		ni->owner_id = si->owner_id = 0;
		ni->security_id = si->security_id = securid;
		ni->quota_charged = si->quota_charged = const_cpu_to_le64(0);
		ni->usn = si->usn = const_cpu_to_le64(0);
	} else
		clear_nino_flag(ni, v3_Extensions);
	if (!S_ISREG(type) && !S_ISDIR(type)) {
		si->file_attributes = FILE_ATTR_SYSTEM;
		ni->flags = FILE_ATTR_SYSTEM;
	}
	ni->flags |= FILE_ATTR_ARCHIVE;
	if (NVolHideDotFiles(dir_ni->vol)
	    && (name_len > 1)
	    && (name[0] == const_cpu_to_le16('.'))
	    && (name[1] != const_cpu_to_le16('.')))
		ni->flags |= FILE_ATTR_HIDDEN;
		/*
		 * Set compression flag according to parent directory
		 * unless NTFS version < 3.0 or cluster size > 4K
		 * or compression has been disabled
		 */
	if ((dir_ni->flags & FILE_ATTR_COMPRESSED)
	   && (dir_ni->vol->major_ver >= 3)
	   && NVolCompression(dir_ni->vol)
	   && (dir_ni->vol->cluster_size <= MAX_COMPRESSION_CLUSTER_SIZE)
	   && (S_ISREG(type) || S_ISDIR(type)))
		ni->flags |= FILE_ATTR_COMPRESSED;
	/* Add STANDARD_INFORMATION to inode. */
	if (ntfs_attr_add(ni, AT_STANDARD_INFORMATION, AT_UNNAMED, 0,
			(u8*)si, si_len)) {
		err = errno;
		ntfs_log_error("Failed to add STANDARD_INFORMATION "
				"attribute.\n");
		goto err_out;
	}

	if (!securid) {
		if (ntfs_sd_add_everyone(ni)) {
			err = errno;
			goto err_out;
		}
	}
	rollback_sd = 1;

	if (S_ISDIR(type)) {
		INDEX_ROOT *ir = NULL;
		INDEX_ENTRY *ie;
		int ir_len, index_len;

		/* Create INDEX_ROOT attribute. */
		index_len = sizeof(INDEX_HEADER) + sizeof(INDEX_ENTRY_HEADER);
		ir_len = offsetof(INDEX_ROOT, index) + index_len;
		ir = ntfs_calloc(ir_len);
		if (!ir) {
			err = errno;
			goto err_out;
		}
		ir->type = AT_FILE_NAME;
		ir->collation_rule = COLLATION_FILE_NAME;
		ir->index_block_size = cpu_to_le32(ni->vol->indx_record_size);
		if (ni->vol->cluster_size <= ni->vol->indx_record_size)
			ir->clusters_per_index_block =
					ni->vol->indx_record_size >>
					ni->vol->cluster_size_bits;
		else
			ir->clusters_per_index_block = 
					ni->vol->indx_record_size >>
					ni->vol->sector_size_bits;
		ir->index.entries_offset = cpu_to_le32(sizeof(INDEX_HEADER));
		ir->index.index_length = cpu_to_le32(index_len);
		ir->index.allocated_size = cpu_to_le32(index_len);
		ie = (INDEX_ENTRY*)((u8*)ir + sizeof(INDEX_ROOT));
		ie->length = cpu_to_le16(sizeof(INDEX_ENTRY_HEADER));
		ie->key_length = 0;
		ie->ie_flags = INDEX_ENTRY_END;
		/* Add INDEX_ROOT attribute to inode. */
		if (ntfs_attr_add(ni, AT_INDEX_ROOT, NTFS_INDEX_I30, 4,
				(u8*)ir, ir_len)) {
			err = errno;
			free(ir);
			ntfs_log_error("Failed to add INDEX_ROOT attribute.\n");
			goto err_out;
		}
		free(ir);
	} else {
		INTX_FILE *data;
		int data_len;

		switch (type) {
			case S_IFBLK:
			case S_IFCHR:
				data_len = offsetof(INTX_FILE, device_end);
				data = ntfs_malloc(data_len);
				if (!data) {
					err = errno;
					goto err_out;
				}
				data->major = cpu_to_le64(major(dev));
				data->minor = cpu_to_le64(minor(dev));
				if (type == S_IFBLK)
					data->magic = INTX_BLOCK_DEVICE;
				if (type == S_IFCHR)
					data->magic = INTX_CHARACTER_DEVICE;
				break;
			case S_IFLNK:
				data_len = sizeof(INTX_FILE_TYPES) +
						target_len * sizeof(ntfschar);
				data = ntfs_malloc(data_len);
				if (!data) {
					err = errno;
					goto err_out;
				}
				data->magic = INTX_SYMBOLIC_LINK;
				memcpy(data->target, target,
						target_len * sizeof(ntfschar));
				break;
			case S_IFSOCK:
				data = NULL;
				data_len = 1;
				break;
			default: /* FIFO or regular file. */
				data = NULL;
				data_len = 0;
				break;
		}
		/* Add DATA attribute to inode. */
		if (ntfs_attr_add(ni, AT_DATA, AT_UNNAMED, 0, (u8*)data,
				data_len)) {
			err = errno;
			ntfs_log_error("Failed to add DATA attribute.\n");
			free(data);
			goto err_out;
		}
		rollback_data = 1;
		free(data);
	}
	/* Create FILE_NAME attribute. */
	fn_len = sizeof(FILE_NAME_ATTR) + name_len * sizeof(ntfschar);
	fn = ntfs_calloc(fn_len);
	if (!fn) {
		err = errno;
		goto err_out;
	}
	fn->parent_directory = MK_LE_MREF(dir_ni->mft_no,
			le16_to_cpu(dir_ni->mrec->sequence_number));
	fn->file_name_length = name_len;
	fn->file_name_type = FILE_NAME_POSIX;
	if (S_ISDIR(type))
		fn->file_attributes = FILE_ATTR_I30_INDEX_PRESENT;
	if (!S_ISREG(type) && !S_ISDIR(type))
		fn->file_attributes = FILE_ATTR_SYSTEM;
	else
		fn->file_attributes |= ni->flags & FILE_ATTR_COMPRESSED;
	fn->file_attributes |= FILE_ATTR_ARCHIVE;
	fn->file_attributes |= ni->flags & FILE_ATTR_HIDDEN;
	fn->creation_time = ni->creation_time;
	fn->last_data_change_time = ni->last_data_change_time;
	fn->last_mft_change_time = ni->last_mft_change_time;
	fn->last_access_time = ni->last_access_time;
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
		fn->data_size = fn->allocated_size = const_cpu_to_le64(0);
	else {
		fn->data_size = cpu_to_sle64(ni->data_size);
		fn->allocated_size = cpu_to_sle64(ni->allocated_size);
	}
	memcpy(fn->file_name, name, name_len * sizeof(ntfschar));
	/* Add FILE_NAME attribute to inode. */
	if (ntfs_attr_add(ni, AT_FILE_NAME, AT_UNNAMED, 0, (u8*)fn, fn_len)) {
		err = errno;
		ntfs_log_error("Failed to add FILE_NAME attribute.\n");
		goto err_out;
	}
	/* Add FILE_NAME attribute to index. */
	if (ntfs_index_add_filename(dir_ni, fn, MK_MREF(ni->mft_no,
			le16_to_cpu(ni->mrec->sequence_number)))) {
		err = errno;
		ntfs_log_perror("Failed to add entry to the index");
		goto err_out;
	}
	/* Set hard links count and directory flag. */
	ni->mrec->link_count = cpu_to_le16(1);
	if (S_ISDIR(type))
		ni->mrec->flags |= MFT_RECORD_IS_DIRECTORY;
	ntfs_inode_mark_dirty(ni);
	/* Done! */
	free(fn);
	free(si);
	ntfs_log_trace("Done.\n");
	return ni;
err_out:
	ntfs_log_trace("Failed.\n");

	if (rollback_sd)
		ntfs_attr_remove(ni, AT_SECURITY_DESCRIPTOR, AT_UNNAMED, 0);
	
	if (rollback_data)
		ntfs_attr_remove(ni, AT_DATA, AT_UNNAMED, 0);
	/*
	 * Free extent MFT records (should not exist any with current
	 * ntfs_create implementation, but for any case if something will be
	 * changed in the future).
	 */
	while (ni->nr_extents)
		if (ntfs_mft_record_free(ni->vol, *(ni->extent_nis))) {
			err = errno;
			ntfs_log_error("Failed to free extent MFT record.  "
					"Leaving inconsistent metadata.\n");
		}
	if (ntfs_mft_record_free(ni->vol, ni))
		ntfs_log_error("Failed to free MFT record.  "
				"Leaving inconsistent metadata. Run chkdsk.\n");
	free(fn);
	free(si);
	errno = err;
	return NULL;
}

/**
 * Some wrappers around __ntfs_create() ...
 */

ntfs_inode *ntfs_create(ntfs_inode *dir_ni, le32 securid, ntfschar *name,
		u8 name_len, mode_t type)
{
	if (type != S_IFREG && type != S_IFDIR && type != S_IFIFO &&
			type != S_IFSOCK) {
		ntfs_log_error("Invalid arguments.\n");
		return NULL;
	}
	return __ntfs_create(dir_ni, securid, name, name_len, type, 0, NULL, 0);
}

ntfs_inode *ntfs_create_device(ntfs_inode *dir_ni, le32 securid,
		ntfschar *name, u8 name_len, mode_t type, dev_t dev)
{
	if (type != S_IFCHR && type != S_IFBLK) {
		ntfs_log_error("Invalid arguments.\n");
		return NULL;
	}
	return __ntfs_create(dir_ni, securid, name, name_len, type, dev, NULL, 0);
}

ntfs_inode *ntfs_create_symlink(ntfs_inode *dir_ni, le32 securid,
		ntfschar *name, u8 name_len, ntfschar *target, int target_len)
{
	if (!target || !target_len) {
		ntfs_log_error("%s: Invalid argument (%p, %d)\n", __FUNCTION__,
			       target, target_len);
		return NULL;
	}
	return __ntfs_create(dir_ni, securid, name, name_len, S_IFLNK, 0,
			target, target_len);
}

int ntfs_check_empty_dir(ntfs_inode *ni)
{
	ntfs_attr *na;
	int ret = 0;
	
	if (!(ni->mrec->flags & MFT_RECORD_IS_DIRECTORY))
		return 0;

	na = ntfs_attr_open(ni, AT_INDEX_ROOT, NTFS_INDEX_I30, 4);
	if (!na) {
		errno = EIO;
		ntfs_log_perror("Failed to open directory");
		return -1;
	}
	
	/* Non-empty directory? */
	if ((na->data_size != sizeof(INDEX_ROOT) + sizeof(INDEX_ENTRY_HEADER))){
		/* Both ENOTEMPTY and EEXIST are ok. We use the more common. */
		errno = ENOTEMPTY;
		ntfs_log_debug("Directory is not empty\n");
		ret = -1;
	}
	
	ntfs_attr_close(na);
	return ret;
}

static int ntfs_check_unlinkable_dir(ntfs_inode *ni, FILE_NAME_ATTR *fn)
{
	int link_count = le16_to_cpu(ni->mrec->link_count);
	int ret;
	
	ret = ntfs_check_empty_dir(ni);
	if (!ret || errno != ENOTEMPTY)
		return ret;
	/* 
	 * Directory is non-empty, so we can unlink only if there is more than
	 * one "real" hard link, i.e. links aren't different DOS and WIN32 names
	 */
	if ((link_count == 1) || 
	    (link_count == 2 && fn->file_name_type == FILE_NAME_DOS)) {
		errno = ENOTEMPTY;
		ntfs_log_debug("Non-empty directory without hard links\n");
		goto no_hardlink;
	}
	
	ret = 0;
no_hardlink:	
	return ret;
}

/**
 * ntfs_delete - delete file or directory from ntfs volume
 * @ni:		ntfs inode for object to delte
 * @dir_ni:	ntfs inode for directory in which delete object
 * @name:	unicode name of the object to delete
 * @name_len:	length of the name in unicode characters
 *
 * @ni is always closed after the call to this function (even if it failed),
 * user does not need to call ntfs_inode_close himself.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
int ntfs_delete(ntfs_volume *vol, const char *pathname,
		ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name, u8 name_len)
{
	ntfs_attr_search_ctx *actx = NULL;
	FILE_NAME_ATTR *fn = NULL;
	BOOL looking_for_dos_name = FALSE, looking_for_win32_name = FALSE;
	BOOL case_sensitive_match = TRUE;
	int err = 0;
#if CACHE_NIDATA_SIZE
	int i;
#endif
#if CACHE_INODE_SIZE
	struct CACHED_INODE item;
	const char *p;
	u64 inum = (u64)-1;
	int count;
#endif
#if CACHE_LOOKUP_SIZE
	struct CACHED_LOOKUP lkitem;
#endif

	ntfs_log_trace("Entering.\n");
	
	if (!ni || !dir_ni || !name || !name_len) {
		ntfs_log_error("Invalid arguments.\n");
		errno = EINVAL;
		goto err_out;
	}
	if (ni->nr_extents == -1)
		ni = ni->base_ni;
	if (dir_ni->nr_extents == -1)
		dir_ni = dir_ni->base_ni;
	/*
	 * Search for FILE_NAME attribute with such name. If it's in POSIX or
	 * WIN32_AND_DOS namespace, then simply remove it from index and inode.
	 * If filename in DOS or in WIN32 namespace, then remove DOS name first,
	 * only then remove WIN32 name.
	 */
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx)
		goto err_out;
search:
	while (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE,
			0, NULL, 0, actx)) {
		char *s;
		BOOL case_sensitive = IGNORE_CASE;

		errno = 0;
		fn = (FILE_NAME_ATTR*)((u8*)actx->attr +
				le16_to_cpu(actx->attr->value_offset));
		s = ntfs_attr_name_get(fn->file_name, fn->file_name_length);
		ntfs_log_trace("name: '%s'  type: %d  dos: %d  win32: %d  "
			       "case: %d\n", s, fn->file_name_type,
			       looking_for_dos_name, looking_for_win32_name,
			       case_sensitive_match);
		ntfs_attr_name_free(&s);
		if (looking_for_dos_name) {
			if (fn->file_name_type == FILE_NAME_DOS)
				break;
			else
				continue;
		}
		if (looking_for_win32_name) {
			if  (fn->file_name_type == FILE_NAME_WIN32)
				break;
			else
				continue;
		}
		
		/* Ignore hard links from other directories */
		if (dir_ni->mft_no != MREF_LE(fn->parent_directory)) {
			ntfs_log_debug("MFT record numbers don't match "
				       "(%llu != %llu)\n", 
				       (long long unsigned)dir_ni->mft_no, 
				       (long long unsigned)MREF_LE(fn->parent_directory));
			continue;
		}
		     
		if (fn->file_name_type == FILE_NAME_POSIX || case_sensitive_match)
			case_sensitive = CASE_SENSITIVE;
		
		if (ntfs_names_are_equal(fn->file_name, fn->file_name_length,
					 name, name_len, case_sensitive, 
					 ni->vol->upcase, ni->vol->upcase_len)){
			
			if (fn->file_name_type == FILE_NAME_WIN32) {
				looking_for_dos_name = TRUE;
				ntfs_attr_reinit_search_ctx(actx);
				continue;
			}
			if (fn->file_name_type == FILE_NAME_DOS)
				looking_for_dos_name = TRUE;
			break;
		}
	}
	if (errno) {
		/*
		 * If case sensitive search failed, then try once again
		 * ignoring case.
		 */
		if (errno == ENOENT && case_sensitive_match) {
			case_sensitive_match = FALSE;
			ntfs_attr_reinit_search_ctx(actx);
			goto search;
		}
		goto err_out;
	}
	
	if (ntfs_check_unlinkable_dir(ni, fn) < 0)
		goto err_out;
		
	if (ntfs_index_remove(dir_ni, ni, fn, le32_to_cpu(actx->attr->value_length)))
		goto err_out;
	
	if (ntfs_attr_record_rm(actx))
		goto err_out;
	
	ni->mrec->link_count = cpu_to_le16(le16_to_cpu(
			ni->mrec->link_count) - 1);
	
	ntfs_inode_mark_dirty(ni);
	if (looking_for_dos_name) {
		looking_for_dos_name = FALSE;
		looking_for_win32_name = TRUE;
		ntfs_attr_reinit_search_ctx(actx);
		goto search;
	}
	/* TODO: Update object id, quota and securiry indexes if required. */
	/*
	 * If hard link count is not equal to zero then we are done. In other
	 * case there are no reference to this inode left, so we should free all
	 * non-resident attributes and mark all MFT record as not in use.
	 */
#if CACHE_LOOKUP_SIZE
			/* invalidate entry in lookup cache */
	lkitem.name = (const char*)NULL;
	lkitem.namesize = 0;
	lkitem.inum = ni->mft_no;
	lkitem.parent = dir_ni->mft_no;
	ntfs_invalidate_cache(vol->lookup_cache, GENERIC(&lkitem),
			lookup_cache_inv_compare, CACHE_NOHASH);
#endif
#if CACHE_INODE_SIZE
	inum = ni->mft_no;
	if (pathname) {
			/* invalide cache entry, even if there was an error */
		/* Remove leading /'s. */
		p = pathname;
		while (*p == PATH_SEP)
			p++;
		if (p[0] && (p[strlen(p)-1] == PATH_SEP))
			ntfs_log_error("Unnormalized path %s\n",pathname);
		item.pathname = p;
		item.varsize = strlen(p);
	} else {
		item.pathname = (const char*)NULL;
		item.varsize = 0;
	}
	item.inum = inum;
	count = ntfs_invalidate_cache(vol->xinode_cache, GENERIC(&item),
				inode_cache_inv_compare, CACHE_NOHASH);
	if (pathname && !count)
		ntfs_log_error("Could not delete inode cache entry for %s\n",
			pathname);
#endif
	if (ni->mrec->link_count) {
		ntfs_inode_update_times(ni, NTFS_UPDATE_CTIME);
		goto ok;
	}
	if (ntfs_delete_reparse_index(ni)) {
		/*
		 * Failed to remove the reparse index : proceed anyway
		 * This is not a critical error, the entry is useless
		 * because of sequence_number, and stopping file deletion
		 * would be much worse as the file is not referenced now.
		 */
		err = errno;
	}
	if (ntfs_delete_object_id_index(ni)) {
		/*
		 * Failed to remove the object id index : proceed anyway
		 * This is not a critical error.
		 */
		err = errno;
	}
	ntfs_attr_reinit_search_ctx(actx);
	while (!ntfs_attrs_walk(actx)) {
		if (actx->attr->non_resident) {
			runlist *rl;

			rl = ntfs_mapping_pairs_decompress(ni->vol, actx->attr,
					NULL);
			if (!rl) {
				err = errno;
				ntfs_log_error("Failed to decompress runlist.  "
						"Leaving inconsistent metadata.\n");
				continue;
			}
			if (ntfs_cluster_free_from_rl(ni->vol, rl)) {
				err = errno;
				ntfs_log_error("Failed to free clusters.  "
						"Leaving inconsistent metadata.\n");
				continue;
			}
			free(rl);
		}
	}
	if (errno != ENOENT) {
		err = errno;
		ntfs_log_error("Attribute enumeration failed.  "
				"Probably leaving inconsistent metadata.\n");
	}
	/* All extents should be attached after attribute walk. */
#if CACHE_NIDATA_SIZE
		/*
		 * Disconnect extents before deleting them, so they are
		 * not wrongly moved to cache through the chainings
		 */
	for (i=ni->nr_extents-1; i>=0; i--) {
		ni->extent_nis[i]->base_ni = (ntfs_inode*)NULL;
		ni->extent_nis[i]->nr_extents = 0;
		if (ntfs_mft_record_free(ni->vol, ni->extent_nis[i])) {
			err = errno;
			ntfs_log_error("Failed to free extent MFT record.  "
					"Leaving inconsistent metadata.\n");
		}
	}
	free(ni->extent_nis);
	ni->nr_extents = 0;
	ni->extent_nis = (ntfs_inode**)NULL;
#else
	while (ni->nr_extents)
		if (ntfs_mft_record_free(ni->vol, *(ni->extent_nis))) {
			err = errno;
			ntfs_log_error("Failed to free extent MFT record.  "
					"Leaving inconsistent metadata.\n");
		}
#endif
	if (ntfs_mft_record_free(ni->vol, ni)) {
		err = errno;
		ntfs_log_error("Failed to free base MFT record.  "
				"Leaving inconsistent metadata.\n");
	}
	ni = NULL;
ok:	
	ntfs_inode_update_times(dir_ni, NTFS_UPDATE_MCTIME);
out:
	if (actx)
		ntfs_attr_put_search_ctx(actx);
	if (ntfs_inode_close(dir_ni) && !err)
		err = errno;
	if (ntfs_inode_close(ni) && !err)
		err = errno;
	if (err) {
		errno = err;
		ntfs_log_debug("Could not delete file: %s\n", strerror(errno));
		return -1;
	}
	ntfs_log_trace("Done.\n");
	return 0;
err_out:
	err = errno;
	goto out;
}

/**
 * ntfs_link - create hard link for file or directory
 * @ni:		ntfs inode for object to create hard link
 * @dir_ni:	ntfs inode for directory in which new link should be placed
 * @name:	unicode name of the new link
 * @name_len:	length of the name in unicode characters
 *
 * NOTE: At present we allow creating hardlinks to directories, we use them
 * in a temporary state during rename. But it's defenitely bad idea to have
 * hard links to directories as a result of operation.
 * FIXME: Create internal  __ntfs_link that allows hard links to a directories
 * and external ntfs_link that do not. Write ntfs_rename that uses __ntfs_link.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
static int ntfs_link_i(ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name,
			 u8 name_len, FILE_NAME_TYPE_FLAGS nametype)
{
	FILE_NAME_ATTR *fn = NULL;
	int fn_len, err;

	ntfs_log_trace("Entering.\n");
	
	if (!ni || !dir_ni || !name || !name_len || 
			ni->mft_no == dir_ni->mft_no) {
		err = EINVAL;
		ntfs_log_perror("ntfs_link wrong arguments");
		goto err_out;
	}
	
	if ((ni->flags & FILE_ATTR_REPARSE_POINT)
	   && !ntfs_possible_symlink(ni)) {
		err = EOPNOTSUPP;
		goto err_out;
	}
	
	/* Create FILE_NAME attribute. */
	fn_len = sizeof(FILE_NAME_ATTR) + name_len * sizeof(ntfschar);
	fn = ntfs_calloc(fn_len);
	if (!fn) {
		err = errno;
		goto err_out;
	}
	fn->parent_directory = MK_LE_MREF(dir_ni->mft_no,
			le16_to_cpu(dir_ni->mrec->sequence_number));
	fn->file_name_length = name_len;
	fn->file_name_type = nametype;
	fn->file_attributes = ni->flags;
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
		fn->file_attributes |= FILE_ATTR_I30_INDEX_PRESENT;
		fn->data_size = fn->allocated_size = const_cpu_to_le64(0);
	} else {
		fn->allocated_size = cpu_to_sle64(ni->allocated_size);
		fn->data_size = cpu_to_sle64(ni->data_size);
	}
	fn->creation_time = ni->creation_time;
	fn->last_data_change_time = ni->last_data_change_time;
	fn->last_mft_change_time = ni->last_mft_change_time;
	fn->last_access_time = ni->last_access_time;
	memcpy(fn->file_name, name, name_len * sizeof(ntfschar));
	/* Add FILE_NAME attribute to index. */
	if (ntfs_index_add_filename(dir_ni, fn, MK_MREF(ni->mft_no,
			le16_to_cpu(ni->mrec->sequence_number)))) {
		err = errno;
		ntfs_log_perror("Failed to add filename to the index");
		goto err_out;
	}
	/* Add FILE_NAME attribute to inode. */
	if (ntfs_attr_add(ni, AT_FILE_NAME, AT_UNNAMED, 0, (u8*)fn, fn_len)) {
		ntfs_log_error("Failed to add FILE_NAME attribute.\n");
		err = errno;
		/* Try to remove just added attribute from index. */
		if (ntfs_index_remove(dir_ni, ni, fn, fn_len))
			goto rollback_failed;
		goto err_out;
	}
	/* Increment hard links count. */
	ni->mrec->link_count = cpu_to_le16(le16_to_cpu(
			ni->mrec->link_count) + 1);
	/* Done! */
	ntfs_inode_mark_dirty(ni);
	free(fn);
	ntfs_log_trace("Done.\n");
	return 0;
rollback_failed:
	ntfs_log_error("Rollback failed. Leaving inconsistent metadata.\n");
err_out:
	free(fn);
	errno = err;
	return -1;
}

int ntfs_link(ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name, u8 name_len)
{
	return (ntfs_link_i(ni, dir_ni, name, name_len, FILE_NAME_POSIX));
}

/*
 *		Get a parent directory from an inode entry
 *
 *	This is only used in situations where the path used to access
 *	the current file is not known for sure. The result may be different
 *	from the path when the file is linked in several parent directories.
 *
 *	Currently this is only used for translating ".." in the target
 *	of a Vista relative symbolic link
 */

ntfs_inode *ntfs_dir_parent_inode(ntfs_inode *ni)
{
	ntfs_inode *dir_ni = (ntfs_inode*)NULL;
	u64 inum;
	FILE_NAME_ATTR *fn;
	ntfs_attr_search_ctx *ctx;

	if (ni->mft_no != FILE_root) {
			/* find the name in the attributes */
		ctx = ntfs_attr_get_search_ctx(ni, NULL);
		if (!ctx)
			return ((ntfs_inode*)NULL);

		if (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0,
				CASE_SENSITIVE,	0, NULL, 0, ctx)) {
			/* We know this will always be resident. */
			fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
					le16_to_cpu(ctx->attr->value_offset));
			inum = le64_to_cpu(fn->parent_directory);
			if (inum != (u64)-1) {
				dir_ni = ntfs_inode_open(ni->vol, MREF(inum));
			}
		}
		ntfs_attr_put_search_ctx(ctx);
	}
	return (dir_ni);
}

#ifdef HAVE_SETXATTR

#define MAX_DOS_NAME_LENGTH	 12

/*
 *		Get a DOS name for a file in designated directory
 *
 *	Returns size if found
 *		0 if not found
 *		-1 if there was an error (described by errno)
 */

static int get_dos_name(ntfs_inode *ni, u64 dnum, ntfschar *dosname)
{
	size_t outsize = 0;
	FILE_NAME_ATTR *fn;
	ntfs_attr_search_ctx *ctx;

		/* find the name in the attributes */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;

	while (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		/* We know this will always be resident. */
		fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
				le16_to_cpu(ctx->attr->value_offset));

		if ((fn->file_name_type & FILE_NAME_DOS)
		    && (MREF_LE(fn->parent_directory) == dnum)) {
				/*
				 * Found a DOS or WIN32+DOS name for the entry
				 * copy name, after truncation for safety
				 */
			outsize = fn->file_name_length;
/* TODO : reject if name is too long ? */
			if (outsize > MAX_DOS_NAME_LENGTH)
				outsize = MAX_DOS_NAME_LENGTH;
			memcpy(dosname,fn->file_name,outsize*sizeof(ntfschar));
		}
	}
	ntfs_attr_put_search_ctx(ctx);
	return (outsize);
}


/*
 *		Get a long name for a file in designated directory
 *
 *	Returns size if found
 *		0 if not found
 *		-1 if there was an error (described by errno)
 */

static int get_long_name(ntfs_inode *ni, u64 dnum, ntfschar *longname)
{
	size_t outsize = 0;
	FILE_NAME_ATTR *fn;
	ntfs_attr_search_ctx *ctx;

		/* find the name in the attributes */
	ctx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!ctx)
		return -1;

		/* first search for WIN32 or DOS+WIN32 names */
	while (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		/* We know this will always be resident. */
		fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
				le16_to_cpu(ctx->attr->value_offset));

		if ((fn->file_name_type & FILE_NAME_WIN32)
		    && (MREF_LE(fn->parent_directory) == dnum)) {
				/*
				 * Found a WIN32 or WIN32+DOS name for the entry
				 * copy name
				 */
			outsize = fn->file_name_length;
			memcpy(longname,fn->file_name,outsize*sizeof(ntfschar));
		}
	}
		/* if not found search for POSIX names */
	if (!outsize) {
		ntfs_attr_reinit_search_ctx(ctx);
	while (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE,
			0, NULL, 0, ctx)) {
		/* We know this will always be resident. */
		fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
				le16_to_cpu(ctx->attr->value_offset));

		if ((fn->file_name_type == FILE_NAME_POSIX)
		    && (MREF_LE(fn->parent_directory) == dnum)) {
				/*
				 * Found a POSIX name for the entry
				 * copy name
				 */
			outsize = fn->file_name_length;
			memcpy(longname,fn->file_name,outsize*sizeof(ntfschar));
		}
	}
	}
	ntfs_attr_put_search_ctx(ctx);
	return (outsize);
}


/*
 *		Get the ntfs DOS name into an extended attribute
 */

int ntfs_get_ntfs_dos_name(ntfs_inode *ni, ntfs_inode *dir_ni,
			char *value, size_t size)
{
	int outsize = 0;
	char *outname = (char*)NULL;
	u64 dnum;
	int doslen;
	ntfschar dosname[MAX_DOS_NAME_LENGTH];

	dnum = dir_ni->mft_no;
	doslen = get_dos_name(ni, dnum, dosname);
	if (doslen > 0) {
			/*
			 * Found a DOS name for the entry, make
			 * uppercase and encode into the buffer
			 * if there is enough space
			 */
		ntfs_name_upcase(dosname, doslen,
				ni->vol->upcase, ni->vol->upcase_len);
		if (ntfs_ucstombs(dosname, doslen, &outname, size) < 0) {
			ntfs_log_error("Cannot represent dosname in current locale.\n");
			outsize = -errno;
		} else {
			outsize = strlen(outname);
			if (value && (outsize <= (int)size))
				memcpy(value, outname, outsize);
			else
				if (size && (outsize > (int)size))
					outsize = -ERANGE;
			free(outname);
		}
	} else {
		if (doslen == 0)
			errno = ENODATA;
		outsize = -errno;
	}
	return (outsize);
}

/*
 *		Change the name space of an existing file or directory
 *
 *	Returns the old namespace if successful
 *		-1 if an error occurred (described by errno)
 */

static int set_namespace(ntfs_inode *ni, ntfs_inode *dir_ni,
			ntfschar *name, int len,
			FILE_NAME_TYPE_FLAGS nametype)
{
	ntfs_attr_search_ctx *actx;
	ntfs_index_context *icx;
	FILE_NAME_ATTR *fnx;
	FILE_NAME_ATTR *fn = NULL;
	BOOL found;
	int lkup;
	int ret;

	ret = -1;
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (actx) {
		found = FALSE;
		do {
			lkup = ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0,
	                        CASE_SENSITIVE, 0, NULL, 0, actx);
			if (!lkup) {
				fn = (FILE_NAME_ATTR*)((u8*)actx->attr +
				     le16_to_cpu(actx->attr->value_offset));
				found = (MREF_LE(fn->parent_directory)
						== dir_ni->mft_no)
					&& !memcmp(fn->file_name, name,
						len*sizeof(ntfschar));
			}
		} while (!lkup && !found);
		if (found) {
			icx = ntfs_index_ctx_get(dir_ni, NTFS_INDEX_I30, 4);
			if (icx) {
				lkup = ntfs_index_lookup((char*)fn, len, icx);
				if (!lkup && icx->data && icx->data_len) {
					fnx = (FILE_NAME_ATTR*)icx->data;
					ret = fn->file_name_type;
					fn->file_name_type = nametype;
					fnx->file_name_type = nametype;
					ntfs_inode_mark_dirty(ni);
					ntfs_index_entry_mark_dirty(icx);
				}
			ntfs_index_ctx_put(icx);
			}
		}
		ntfs_attr_put_search_ctx(actx);
	}
	return (ret);
}

/*
 *		Set a DOS name to a file and adjust name spaces
 *
 *	If the new names are collapsible (same uppercased chars) :
 *
 * - the existing DOS name or DOS+Win32 name is made Posix
 * - if it was a real DOS name, the existing long name is made DOS+Win32
 *        and the existing DOS name is deleted
 * - finally the existing long name is made DOS+Win32 unless already done
 *
 *	If the new names are not collapsible :
 *
 * - insert the short name as a DOS name
 * - delete the old long name or existing short name
 * - insert the new long name (as a Win32 or DOS+Win32 name)
 *
 * Deleting the old long name will not delete the file
 * provided the old name was in the Posix name space,
 * because the alternate name has been set before.
 *
 * The inodes of file and parent directory are always closed
 *
 * Returns 0 if successful
 *	   -1 if failed
 */

static int set_dos_name(ntfs_inode *ni, ntfs_inode *dir_ni,
			ntfschar *shortname, int shortlen,
			ntfschar *longname, int longlen,
			ntfschar *deletename, int deletelen, BOOL existed)
{
	unsigned int linkcount;
	ntfs_volume *vol;
	BOOL collapsible;
	BOOL deleted;
	BOOL done;
	FILE_NAME_TYPE_FLAGS oldnametype;
	u64 dnum;
	u64 fnum;
	int res;

	res = -1;
	vol = ni->vol;
	dnum = dir_ni->mft_no;
	fnum = ni->mft_no;
				/* save initial link count */
	linkcount = le16_to_cpu(ni->mrec->link_count);

		/* check whether the same name may be used as DOS and WIN32 */
	collapsible = ntfs_collapsible_chars(ni->vol, shortname, shortlen,
						longname, longlen);
	if (collapsible) {
		deleted = FALSE;
		done = FALSE;
		if (existed) {
			oldnametype = set_namespace(ni, dir_ni, deletename,
					deletelen, FILE_NAME_POSIX);
			if (oldnametype == FILE_NAME_DOS) {
				if (set_namespace(ni, dir_ni, longname, longlen,
						FILE_NAME_WIN32_AND_DOS) >= 0) {
					if (!ntfs_delete(vol,
						(const char*)NULL, ni, dir_ni,  
						deletename, deletelen))
						res = 0;
					deleted = TRUE;
				} else
					done = TRUE;
			}
		}
		if (!deleted) {
			if (!done && (set_namespace(ni, dir_ni,
					longname, longlen,
					FILE_NAME_WIN32_AND_DOS) >= 0))
				res = 0;
			ntfs_inode_update_times(ni, NTFS_UPDATE_CTIME);
			ntfs_inode_update_times(dir_ni, NTFS_UPDATE_MCTIME);
			if (ntfs_inode_close_in_dir(ni,dir_ni) && !res)
				res = -1;
			if (ntfs_inode_close(dir_ni) && !res)
				res = -1;
		}
	} else {
		if (!ntfs_link_i(ni, dir_ni, shortname, shortlen, 
				FILE_NAME_DOS)
			/* make sure a new link was recorded */
		    && (le16_to_cpu(ni->mrec->link_count) > linkcount)) {
			/* delete the existing long name or short name */
// is it ok to not provide the path ?
			if (!ntfs_delete(vol, (char*)NULL, ni, dir_ni,
				 deletename, deletelen)) {
			/* delete closes the inodes, so have to open again */
				dir_ni = ntfs_inode_open(vol, dnum);
				if (dir_ni) {
					ni = ntfs_inode_open(vol, fnum);
					if (ni) {
						if (!ntfs_link_i(ni, dir_ni,
							longname, longlen,
							FILE_NAME_WIN32))
							res = 0;
						if (ntfs_inode_close_in_dir(ni,
							dir_ni)
						    && !res)
							res = -1;
					}
				if (ntfs_inode_close(dir_ni) && !res)
					res = -1;
				}
			}
		} else {
			ntfs_inode_close_in_dir(ni,dir_ni);
			ntfs_inode_close(dir_ni);
		}
	}
	return (res);
}


/*
 *		Set the ntfs DOS name into an extended attribute
 *
 *  The DOS name will be added as another file name attribute
 *  using the existing file name information from the original
 *  name or overwriting the DOS Name if one exists.
 *
 *  	The inode of the file is always closed
 */

int ntfs_set_ntfs_dos_name(ntfs_inode *ni, ntfs_inode *dir_ni,
			const char *value, size_t size,	int flags)
{
	int res = 0;
	int longlen = 0;
	int shortlen = 0;
	char newname[MAX_DOS_NAME_LENGTH + 1];
	ntfschar oldname[MAX_DOS_NAME_LENGTH];
	int oldlen;
	ntfs_volume *vol;
	u64 fnum;
	u64 dnum;
	BOOL closed = FALSE;
	ntfschar *shortname = NULL;
	ntfschar longname[NTFS_MAX_NAME_LEN];

	vol = ni->vol;
	fnum = ni->mft_no;
		/* convert the string to the NTFS wide chars */
	if (size > MAX_DOS_NAME_LENGTH)
		size = MAX_DOS_NAME_LENGTH;
	strncpy(newname, value, size);
	newname[size] = 0;
	shortlen = ntfs_mbstoucs(newname, &shortname);
			/* make sure the short name has valid chars */
	if ((shortlen < 0) || ntfs_forbidden_chars(shortname,shortlen)) {
		ntfs_inode_close_in_dir(ni,dir_ni);
		ntfs_inode_close(dir_ni);
		res = -errno;
		return res;
	}
	dnum = dir_ni->mft_no;
	longlen = get_long_name(ni, dnum, longname);
	if (longlen > 0) {
		oldlen = get_dos_name(ni, dnum, oldname);
		if ((oldlen >= 0)
		    && !ntfs_forbidden_chars(longname, longlen)) {
			if (oldlen > 0) {
				if (flags & XATTR_CREATE) {
					res = -1;
					errno = EEXIST;
				} else
					if ((shortlen == oldlen)
					    && !memcmp(shortname,oldname,
						     oldlen*sizeof(ntfschar)))
						/* already set, done */
						res = 0;
					else {
						res = set_dos_name(ni, dir_ni,
							shortname, shortlen,
							longname, longlen,
							oldname, oldlen, TRUE);
						closed = TRUE;
					}
			} else {
				if (flags & XATTR_REPLACE) {
					res = -1;
					errno = ENODATA;
				} else {
					res = set_dos_name(ni, dir_ni,
						shortname, shortlen,
						longname, longlen,
						longname, longlen, FALSE);
					closed = TRUE;
				}
			}
		} else
			res = -1;
	} else {
		res = -1;
		errno = ENOENT;
	}
	free(shortname);
	if (!closed) {
		ntfs_inode_close_in_dir(ni,dir_ni);
		ntfs_inode_close(dir_ni);
	}
	return (res ? -1 : 0);
}

/*
 *		Delete the ntfs DOS name
 */

int ntfs_remove_ntfs_dos_name(ntfs_inode *ni, ntfs_inode *dir_ni)
{
	int res;
	int oldnametype;
	int longlen = 0;
	int shortlen;
	u64 dnum;
	ntfs_volume *vol;
	BOOL deleted = FALSE;
	ntfschar shortname[MAX_DOS_NAME_LENGTH];
	ntfschar longname[NTFS_MAX_NAME_LEN];

	res = -1;
	vol = ni->vol;
	dnum = dir_ni->mft_no;
	longlen = get_long_name(ni, dnum, longname);
	if (longlen > 0) {
		shortlen = get_dos_name(ni, dnum, shortname);
		if (shortlen >= 0) {
				/* migrate the long name as Posix */
			oldnametype = set_namespace(ni,dir_ni,longname,longlen,
					FILE_NAME_POSIX);
			switch (oldnametype) {
			case FILE_NAME_WIN32_AND_DOS :
				/* name was Win32+DOS : done */
				res = 0;
				break;
			case FILE_NAME_DOS :
				/* name was DOS, make it back to DOS */
				set_namespace(ni,dir_ni,longname,longlen,
						FILE_NAME_DOS);
				errno = ENOENT;
				break;
			case FILE_NAME_WIN32 :
				/* name was Win32, make it Posix and delete */
				if (set_namespace(ni,dir_ni,shortname,shortlen,
						FILE_NAME_POSIX) >= 0) {
					if (!ntfs_delete(vol,
							(const char*)NULL, ni,
							dir_ni, shortname,
							shortlen))
						res = 0;
					deleted = TRUE;
				} else {
					/*
					 * DOS name has been found, but cannot
					 * migrate to Posix : something bad 
					 * has happened
					 */
					errno = EIO;
					ntfs_log_error("Could not change"
						" DOS name of inode %lld to Posix\n",
						(long long)ni->mft_no);
				}
				break;
			default :
				/* name was Posix or not found : error */
				errno = ENOENT;
				break;
			}
		}
	} else {
		errno = ENOENT;
		res = -1;
	}
	if (!deleted) {
		ntfs_inode_close_in_dir(ni,dir_ni);
		ntfs_inode_close(dir_ni);
	}
	return (res);
}

#endif
