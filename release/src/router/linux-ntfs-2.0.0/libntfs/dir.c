/**
 * dir.c - Directory handling code. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Anton Altaparmakov
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 * Copyright (c) 2004-2005 Richard Russon
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
u64 ntfs_inode_lookup_by_name(ntfs_inode *dir_ni, const ntfschar *uname,
		const int uname_len)
{
	VCN vcn;
	u64 mref = 0;
	s64 br;
	ntfs_volume *vol = dir_ni->vol;
	ntfs_attr_search_ctx *ctx;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia;
	u8 *index_end;
	ntfs_attr *ia_na;
	int eo, rc;
	u32 index_block_size, index_vcn_size;
	u8 index_vcn_size_bits;

	if (!dir_ni || !dir_ni->mrec || !uname || uname_len <= 0) {
		errno = EINVAL;
		return -1;
	}

	ctx = ntfs_attr_get_search_ctx(dir_ni, NULL);
	if (!ctx)
		return -1;

	/* Find the index root attribute in the mft record. */
	if (ntfs_attr_lookup(AT_INDEX_ROOT, NTFS_INDEX_I30, 4, CASE_SENSITIVE,
				0, NULL, 0, ctx)) {
		ntfs_log_perror("Index root attribute missing in directory "
				"inode 0x%llx", (unsigned long long)dir_ni->
				mft_no);
		goto put_err_out;
	}
	/* Get to the index root value. */
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	index_block_size = le32_to_cpu(ir->index_block_size);
	if (index_block_size < NTFS_BLOCK_SIZE ||
			index_block_size & (index_block_size - 1)) {
		ntfs_log_debug("Index block size %u is invalid.\n",
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
				index_end)
			goto put_err_out;
		/*
		 * The last entry cannot contain a name. It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/*
		 * We perform a case sensitive comparison and if that matches
		 * we are done and return the mft reference of the inode (i.e.
		 * the inode number together with the sequence number for
		 * consistency checking). We convert it to cpu format before
		 * returning.
		 */
		if (ntfs_names_are_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len)) {
found_it:
			/*
			 * We have a perfect match, so we don't need to care
			 * about having matched imperfectly before.
			 */
			mref = le64_to_cpu(ie->indexed_file);
			ntfs_attr_put_search_ctx(ctx);
			return mref;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison. If the comparison matches, we cache
		 * the mft reference in mref. Use first case insensitive match
		 * in case if no name matches case sensitive, but several names
		 * matches case insensitive.
		 */
		if (!mref && !NVolCaseSensitive(vol) &&
				ntfs_names_are_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len))
			mref = le64_to_cpu(ie->indexed_file);
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_names_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
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
		 * Names match with case insensitive comparison, now try the
		 * case sensitive comparison, which is required for proper
		 * collation.
		 */
		rc = ntfs_names_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		/*
		 * Perfect match, this will never happen as the
		 * ntfs_are_names_equal() call will have gotten a match but we
		 * still treat it correctly.
		 */
		goto found_it;
	}
	/*
	 * We have finished with this index without success. Check for the
	 * presence of a child node and if not present return error code
	 * ENOENT, unless we have got the mft reference of a matching name
	 * cached in mref in which case return mref.
	 */
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
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
		ntfs_log_perror("Failed to open index allocation attribute. "
				"Directory inode 0x%llx is corrupt or driver "
				"bug", (unsigned long long)dir_ni->mft_no);
		goto put_err_out;
	}

	/* Allocate a buffer for the current index block. */
	ia = (INDEX_ALLOCATION*)malloc(index_block_size);
	if (!ia) {
		ntfs_log_perror("Failed to allocate buffer for index block");
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
		ntfs_log_debug("Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx).\n",
				(long long)sle64_to_cpu(ia->index_block_vcn),
				(long long)vcn);
		errno = EIO;
		goto close_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 != index_block_size) {
		ntfs_log_debug("Index buffer (VCN 0x%llx) of directory inode "
				"0x%llx has a size (%u) differing from the "
				"directory specified size (%u).\n",
				(long long)vcn, (unsigned long long)dir_ni->
				mft_no, (unsigned)le32_to_cpu(ia->index.
				allocated_size) + 0x18, (unsigned)
				index_block_size);
		errno = EIO;
		goto close_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + index_block_size) {
		ntfs_log_debug("Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%llx exceeds maximum size.\n",
				(long long)vcn, (unsigned long long)dir_ni->
				mft_no);
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
			ntfs_log_debug("Index entry out of bounds in directory "
					"inode 0x%llx.\n",
					(unsigned long long)dir_ni->mft_no);
			errno = EIO;
			goto close_err_out;
		}
		/*
		 * The last entry cannot contain a name. It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/*
		 * We perform a case sensitive comparison and if that matches
		 * we are done and return the mft reference of the inode (i.e.
		 * the inode number together with the sequence number for
		 * consistency checking). We convert it to cpu format before
		 * returning.
		 */
		if (ntfs_names_are_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len)) {
found_it2:
			/*
			 * We have a perfect match, so we don't need to care
			 * about having matched imperfectly before.
			 */
			mref = le64_to_cpu(ie->indexed_file);
			free(ia);
			ntfs_attr_close(ia_na);
			ntfs_attr_put_search_ctx(ctx);
			return mref;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison. If the comparison matches, we cache
		 * the mft reference in mref. Use first case insensitive match
		 * in case if no name matches case sensitive, but several names
		 * matches case insensitive.
		 */
		if (!mref && !NVolCaseSensitive(vol) &&
				ntfs_names_are_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len))
			mref = le64_to_cpu(ie->indexed_file);
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_names_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
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
		 * Names match with case insensitive comparison, now try the
		 * case sensitive comparison, which is required for proper
		 * collation.
		 */
		rc = ntfs_names_collate(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		/*
		 * Perfect match, this will never happen as the
		 * ntfs_are_names_equal() call will have gotten a match but we
		 * still treat it correctly.
		 */
		goto found_it2;
	}
	/*
	 * We have finished with this index buffer without success. Check for
	 * the presence of a child node.
	 */
	if (ie->flags & INDEX_ENTRY_NODE) {
		if ((ia->index.flags & NODE_MASK) == LEAF_NODE) {
			ntfs_log_debug("Index entry with child node found in a "
					"leaf node in directory inode "
					"0x%llx.\n",
					(unsigned long long)dir_ni->mft_no);
			errno = EIO;
			goto close_err_out;
		}
		/* Child node present, descend into it. */
		vcn = sle64_to_cpup((u8*)ie + le16_to_cpu(ie->length) - 8);
		if (vcn >= 0)
			goto descend_into_child_node;
		ntfs_log_debug("Negative child node vcn in directory inode "
				"0x%llx.\n", (unsigned long long)dir_ni->
				mft_no);
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

/**
 * ntfs_pathname_to_inode_num - find the inode number which represents the
 * 				given pathname
 * @vol:       An ntfs volume obtained from ntfs_mount
 * @parent:    A directory inode to begin the search (may be NULL)
 * @pathname:  Pathname to be located
 *
 * Take an ASCII pathname and find the inode that represents it.  The function
 * splits the path and then descends the directory tree.  If @parent is NULL,
 * then the root directory '.' will be used as the base for the search.
 *
 * Return:  -1    Error, the pathname was invalid, or some other error occurred
 *          else  Success, the pathname was valid
 */
u64 ntfs_pathname_to_inode_num(ntfs_volume *vol, ntfs_inode *parent,
		const char *pathname)
{
	u64 inum, result;
	int len, err = 0;
	char *p, *q;
	ntfs_inode *ni = NULL;
	ntfschar *unicode = NULL;
	char *ascii = NULL;

	inum = result = (u64)-1;
	if (!vol || !pathname) {
		err = EINVAL;
		goto close;
	}
	ntfs_log_trace("Path: '%s'\n", pathname);
	if (parent) {
		ni = parent;
	} else
		inum = FILE_root;
	unicode = calloc(1, MAX_PATH);
	ascii = strdup(pathname);
	if (!unicode || !ascii) {
		ntfs_log_error("Out of memory.\n");
		err = ENOMEM;
		goto close;
	}
	p = ascii;
	/* Remove leading /'s. */
	while (p && *p == PATH_SEP)
		p++;
	while (p && *p) {
		if (!ni) {
			ni = ntfs_inode_open(vol, inum);
			if (!ni) {
				ntfs_log_debug("Cannot open inode %llu.\n",
						(unsigned long long)inum);
				err = EIO;
				goto close;
			}
		}
		/* Find the end of the first token. */
		q = strchr(p, PATH_SEP);
		if (q != NULL) {
			*q = 0;
			q++;
		}
		len = ntfs_mbstoucs(p, &unicode, MAX_PATH);
		if (len < 0) {
			ntfs_log_debug("Couldn't convert name to Unicode: "
					"%s.\n", p);
			err = EILSEQ;
			goto close;
		}
		inum = ntfs_inode_lookup_by_name(ni, unicode, len);
		if (inum == (u64)-1) {
			ntfs_log_debug("Couldn't find name '%s' in pathname "
					"'%s'.\n", p, pathname);
			err = ENOENT;
			goto close;
		}
		inum = MREF(inum);
		if (ni != parent)
			ntfs_inode_close(ni);
		ni = NULL;
		p = q;
		while (p && *p == PATH_SEP)
			p++;
	}
	result = inum;
close:
	if (ni && (ni != parent))
		ntfs_inode_close(ni);
	free(ascii);
	free(unicode);
	if (err)
		errno = err;
	return result;
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

	inum = ntfs_pathname_to_inode_num(vol, parent, pathname);
	if (inum == (u64)-1)
		return NULL;
	return ntfs_inode_open(vol, inum);
}

/*
 * The little endian Unicode string ".." for ntfs_readdir().
 */
static const ntfschar dotdot[3] = { const_cpu_to_le16('.'),
				   const_cpu_to_le16('.'),
				   const_cpu_to_le16('\0') };

/**
 * ntfs_filldir - ntfs specific filldir method
 * @vol:	ntfs volume with wjich we are working
 * @pos:	current position in directory
 * @ie:		current index entry
 * @dirent:	context for filldir callback supplied by the caller
 * @filldir:	filldir callback supplied by the caller
 *
 * Pass information specifying the current directory entry @ie to the @filldir
 * callback.
 */
static int ntfs_filldir(ntfs_volume *vol, s64 *pos, INDEX_ENTRY *ie,
		void *dirent, ntfs_filldir_t filldir)
{
	FILE_NAME_ATTR *fn = &ie->key.file_name;
	unsigned dt_type;

	ntfs_log_trace("Entering.\n");

	/* Skip root directory self reference entry. */
	if (MREF_LE(ie->indexed_file) == FILE_root)
		return 0;
	if (ie->key.file_name.file_attributes & FILE_ATTR_I30_INDEX_PRESENT)
		dt_type = NTFS_DT_DIR;
	else {
		if (NVolInterix(vol) && fn->file_attributes & FILE_ATTR_SYSTEM)
			dt_type = NTFS_DT_UNKNOWN;
		else
			dt_type = NTFS_DT_REG;
	}
	return filldir(dirent, fn->file_name, fn->file_name_length,
			fn->file_name_type, *pos,
			le64_to_cpu(ie->indexed_file), dt_type);
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
		ntfs_log_debug("No file name found in inode 0x%llx. Corrupt "
				"inode.\n", (unsigned long long)ni->mft_no);
		goto err_out;
	}
	if (ctx->attr->non_resident) {
		ntfs_log_debug("File name attribute must be resident. "
				"Corrupt inode 0x%llx.\n",
				(unsigned long long)ni->mft_no);
		goto io_err_out;
	}
	fn = (FILE_NAME_ATTR*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));
	if ((u8*)fn +	le32_to_cpu(ctx->attr->value_length) >
			(u8*)ctx->attr + le32_to_cpu(ctx->attr->length)) {
		ntfs_log_debug("Corrupt file name attribute in inode 0x%llx.\n",
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

	ntfs_log_trace("Entering for inode 0x%llx, *pos 0x%llx.\n",
			(unsigned long long)dir_ni->mft_no, (long long)*pos);

	/* Open the index allocation attribute. */
	ia_na = ntfs_attr_open(dir_ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
	if (!ia_na) {
		if (errno != ENOENT) {
			ntfs_log_perror("Failed to open index allocation "
					"attribute. Directory inode 0x%llx is "
					"corrupt or bug", (unsigned long long)
					dir_ni->mft_no);
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
	if (ntfs_attr_lookup(AT_INDEX_ROOT, NTFS_INDEX_I30, 4, CASE_SENSITIVE,
				0, NULL, 0, ctx)) {
		ntfs_log_debug("Index root attribute missing in directory "
				"inode 0x%llx.\n", (unsigned long long)dir_ni->
				mft_no);
		goto dir_err_out;
	}
	/* Get to the index root value. */
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->value_offset));

	/* Determine the size of a vcn in the directory index. */
	index_block_size = le32_to_cpu(ir->index_block_size);
	if (index_block_size < NTFS_BLOCK_SIZE ||
			index_block_size & (index_block_size - 1)) {
		ntfs_log_debug("Index block size %u is invalid.\n",
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
		ntfs_log_debug("In index root, offset 0x%x.\n",
				(u8*)ie - (u8*)ir);
		/* Bounds checks. */
		if ((u8*)ie < (u8*)ctx->mrec || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err_out;
		/* The last entry cannot contain a name. */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/* Skip index root entry if continuing previous readdir. */
		if (ir_pos > (u8*)ie - (u8*)ir)
			continue;
		/* Advance the position even if going to skip the entry. */
		*pos = (u8*)ie - (u8*)ir;
		/*
		 * Submit the directory entry to ntfs_filldir(), which will
		 * invoke the filldir() callback as appropriate.
		 */
		rc = ntfs_filldir(vol, pos, ie, dirent, filldir);
		if (rc)
			goto err_out;
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
	ia = (INDEX_ALLOCATION*)malloc(index_block_size);
	if (!ia) {
		ntfs_log_perror("Failed to allocate buffer for index block");
		goto err_out;
	}

	bmp_na = ntfs_attr_open(dir_ni, AT_BITMAP, NTFS_INDEX_I30, 4);
	if (!bmp_na) {
		ntfs_log_perror("Failed to open index bitmap attribute");
		goto dir_err_out;
	}

	/* Get the offset into the index allocation attribute. */
	ia_pos = *pos - vol->mft_record_size;

	bmp_pos = ia_pos >> index_block_size_bits;
	if (bmp_pos >> 3 >= bmp_na->data_size) {
		ntfs_log_debug("Current index position exceeds index bitmap "
				"size.\n");
		goto dir_err_out;
	}

	bmp_buf_size = min(bmp_na->data_size - (bmp_pos >> 3), 4096);
	bmp = (u8*)malloc(bmp_buf_size);
	if (!bmp) {
		ntfs_log_perror("Failed to allocate bitmap buffer");
		goto err_out;
	}

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
		if ((bmp_pos >> 3) + bmp_buf_size > bmp_na->data_size)
			bmp_buf_size = bmp_na->data_size - (bmp_pos >> 3);
		br = ntfs_attr_pread(bmp_na, bmp_pos >> 3, bmp_buf_size, bmp);
		if (br != bmp_buf_size) {
			if (br != -1)
				errno = EIO;
			ntfs_log_perror("Failed to read from index bitmap "
					"attribute");
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
		ntfs_log_debug("Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx) in "
				"inode 0x%llx.\n",
				(long long)sle64_to_cpu(ia->index_block_vcn),
				(long long)ia_start >> index_vcn_size_bits,
				(unsigned long long)dir_ni->mft_no);
		goto dir_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 != index_block_size) {
		ntfs_log_debug("Index buffer (VCN 0x%llx) of directory inode "
				"0x%llx has a size (%u) differing from the "
				"directory specified size (%u).\n",
				(long long)ia_start >> index_vcn_size_bits,
				(unsigned long long)dir_ni->mft_no,
				(unsigned) le32_to_cpu(ia->index.allocated_size)
				+ 0x18, (unsigned)index_block_size);
		goto dir_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + index_block_size) {
		ntfs_log_debug("Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%llx exceeds maximum size.\n",
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
			ntfs_log_debug("Index entry out of bounds in directory "
					"inode 0x%llx.\n", (unsigned long long)
					dir_ni->mft_no);
			goto dir_err_out;
		}
		/* The last entry cannot contain a name. */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/* Skip index entry if continuing previous readdir. */
		if (ia_pos - ia_start > (u8*)ie - (u8*)ia)
			continue;
		/* Advance the position even if going to skip the entry. */
		*pos = (u8*)ie - (u8*)ia + (sle64_to_cpu(
				ia->index_block_vcn) << index_vcn_size_bits) +
				dir_ni->vol->mft_record_size;
		/*
		 * Submit the directory entry to ntfs_filldir(), which will
		 * invoke the filldir() callback as appropriate.
		 */
		rc = ntfs_filldir(vol, pos, ie, dirent, filldir);
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
	if (rc)
		ntfs_log_trace("filldir returned %i, *pos 0x%llx.", rc,
				(long long)*pos);
	ntfs_log_trace("Failed.\n");
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
static ntfs_inode *__ntfs_create(ntfs_inode *dir_ni,
		ntfschar *name, u8 name_len, dev_t type, dev_t dev,
		ntfschar *target, u8 target_len)
{
	ntfs_inode *ni;
	int rollback_data = 0, rollback_sd = 0;
	FILE_NAME_ATTR *fn = NULL;
	STANDARD_INFORMATION *si = NULL;
	SECURITY_DESCRIPTOR_ATTR *sd = NULL;
	ACL *acl;
	ACCESS_ALLOWED_ACE *ace;
	SID *sid;
	int err, fn_len, si_len, sd_len;

	ntfs_log_trace("Entering.\n");

	/* Sanity checks. */
	if (!dir_ni || !name || !name_len) {
		ntfs_log_error("Invalid arguments.\n");
		errno = EINVAL;
		return NULL;
	}
	/* FIXME: Reparse points requires special handling. */
	if (dir_ni->flags & FILE_ATTR_REPARSE_POINT) {
		errno = EOPNOTSUPP;
		return NULL;
	}
	/* Allocate MFT record for new file. */
	ni = ntfs_mft_record_alloc(dir_ni->vol, NULL);
	if (!ni) {
		ntfs_log_error("Failed to allocate new MFT record: %s.\n",
				strerror(errno));
		return NULL;
	}
	/*
	 * Create STANDARD_INFORMATION attribute. Write STANDARD_INFORMATION
	 * version 1.2, windows will upgrade it to version 3 if needed.
	 */
	si_len = offsetof(STANDARD_INFORMATION, v1_end);
	si = calloc(1, si_len);
	if (!si) {
		err = errno;
		ntfs_log_error("Not enough memory.\n");
		goto err_out;
	}
	si->creation_time = utc2ntfs(ni->creation_time);
	si->last_data_change_time = utc2ntfs(ni->last_data_change_time);
	si->last_mft_change_time = utc2ntfs(ni->last_mft_change_time);
	si->last_access_time = utc2ntfs(ni->last_access_time);
	if (!S_ISREG(type) && !S_ISDIR(type)) {
		si->file_attributes = FILE_ATTR_SYSTEM;
		ni->flags = FILE_ATTR_SYSTEM;
	}
	/* Add STANDARD_INFORMATION to inode. */
	if (ntfs_attr_add(ni, AT_STANDARD_INFORMATION, AT_UNNAMED, 0,
			(u8*)si, si_len)) {
		err = errno;
		ntfs_log_error("Failed to add STANDARD_INFORMATION "
				"attribute.\n");
		goto err_out;
	}
	/* Create SECURITY_DESCRIPTOR attribute (everyone has full access). */
	/*
	 * Calculate security descriptor length. We have 2 sub-authorities in
	 * owner and group SIDs, but structure SID contain only one, so add
	 * 4 bytes to every SID.
	 */
	sd_len = sizeof(SECURITY_DESCRIPTOR_ATTR) + 2 * (sizeof(SID) + 4) +
		sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE);
	sd = calloc(1, sd_len);
	if (!sd) {
		err = errno;
		ntfs_log_error("Not enough memory.\n");
		goto err_out;
	}
	sd->revision = 1;
	sd->control = SE_DACL_PRESENT | SE_SELF_RELATIVE;
	sid = (SID*)((u8*)sd + sizeof(SECURITY_DESCRIPTOR_ATTR));
	sd->owner = cpu_to_le32((u8*)sid - (u8*)sd);
	sid->revision = 1;
	sid->sub_authority_count = 2;
	sid->sub_authority[0] = cpu_to_le32(32);
	sid->sub_authority[1] = cpu_to_le32(544);
	sid->identifier_authority.value[5] = 5;
	sid = (SID*)((u8*)sid + sizeof(SID) + 4);
	sd->group = cpu_to_le32((u8*)sid - (u8*)sd);
	sid->revision = 1;
	sid->sub_authority_count = 2;
	sid->sub_authority[0] = cpu_to_le32(32);
	sid->sub_authority[1] = cpu_to_le32(544);
	sid->identifier_authority.value[5] = 5;
	acl = (ACL*)((u8*)sid + sizeof(SID) + 4);
	sd->dacl = cpu_to_le32((u8*)acl - (u8*)sd);
	acl->revision = 2;
	acl->size = cpu_to_le16(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE));
	acl->ace_count = cpu_to_le16(1);
	ace = (ACCESS_ALLOWED_ACE*)((u8*)acl + sizeof(ACL));
	ace->type = ACCESS_ALLOWED_ACE_TYPE;
	ace->flags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
	ace->size = cpu_to_le16(sizeof(ACCESS_ALLOWED_ACE));
	ace->mask = cpu_to_le32(0x1f01ff); /* FIXME */
	ace->sid.revision = 1;
	ace->sid.sub_authority_count = 1;
	ace->sid.sub_authority[0] = 0;
	ace->sid.identifier_authority.value[5] = 1;
	/* Add SECURITY_DESCRIPTOR attribute to inode. */
	if (ntfs_attr_add(ni, AT_SECURITY_DESCRIPTOR, AT_UNNAMED, 0,
			(u8*)sd, sd_len)) {
		err = errno;
		ntfs_log_error("Failed to add SECURITY_DESCRIPTOR "
				"attribute.\n");
		goto err_out;
	}
	rollback_sd = 1;
	/* Add DATA/INDEX_ROOT attribute. */
	if (S_ISDIR(type)) {
		INDEX_ROOT *ir = NULL;
		INDEX_ENTRY *ie;
		int ir_len, index_len;

		/* Create INDEX_ROOT attribute. */
		index_len = sizeof(INDEX_HEADER) + sizeof(INDEX_ENTRY_HEADER);
		ir_len = offsetof(INDEX_ROOT, index) + index_len;
		ir = calloc(1, ir_len);
		if (!ir) {
			err = errno;
			ntfs_log_error("Not enough memory.\n");
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
		ie->flags = INDEX_ENTRY_END;
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
			free(data);
			ntfs_log_error("Failed to add DATA attribute.\n");
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
	fn->creation_time = utc2ntfs(ni->creation_time);
	fn->last_data_change_time = utc2ntfs(ni->last_data_change_time);
	fn->last_mft_change_time = utc2ntfs(ni->last_mft_change_time);
	fn->last_access_time = utc2ntfs(ni->last_access_time);
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
	free(sd);
	ntfs_log_trace("Done.\n");
	return ni;
err_out:
	ntfs_log_trace("Failed.\n");
	if (rollback_sd) {
		ntfs_attr *na;

		na = ntfs_attr_open(ni, AT_SECURITY_DESCRIPTOR, AT_UNNAMED, 0);
		if (!na)
			ntfs_log_perror("Failed to open SD (0x50) attribute of "
					" inode 0x%llx. Run chkdsk.\n",
					(unsigned long long)ni->mft_no);
		else if (ntfs_attr_rm(na))
			ntfs_log_perror("Failed to remove SD (0x50) attribute "
					"of inode 0x%llx. Run chkdsk.\n",
					(unsigned long long)ni->mft_no);
	}
	if (rollback_data) {
		ntfs_attr *na;

		na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
		if (!na)
			ntfs_log_perror("Failed to open data attribute of "
					" inode 0x%llx. Run chkdsk.\n",
					(unsigned long long)ni->mft_no);
		else if (ntfs_attr_rm(na))
			ntfs_log_perror("Failed to remove data attribute of "
					"inode 0x%llx. Run chkdsk.\n",
					(unsigned long long)ni->mft_no);
	}
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
	free(sd);
	errno = err;
	return NULL;
}

/**
 * Some wrappers around __ntfs_create() ...
 */

ntfs_inode *ntfs_create(ntfs_inode *dir_ni, ntfschar *name, u8 name_len,
		dev_t type)
{
	if (type != S_IFREG && type != S_IFDIR && type != S_IFIFO &&
			type != S_IFSOCK) {
		ntfs_log_error("Invalid arguments.\n");
		return NULL;
	}
	return __ntfs_create(dir_ni, name, name_len, type, 0, NULL, 0);
}

ntfs_inode *ntfs_create_device(ntfs_inode *dir_ni, ntfschar *name, u8 name_len,
		dev_t type, dev_t dev)
{
	if (type != S_IFCHR && type != S_IFBLK) {
		ntfs_log_error("Invalid arguments.\n");
		return NULL;
	}
	return __ntfs_create(dir_ni, name, name_len, type, dev, NULL, 0);
}

ntfs_inode *ntfs_create_symlink(ntfs_inode *dir_ni, ntfschar *name, u8 name_len,
		ntfschar *target, u8 target_len)
{
	if (!target || !target_len) {
		ntfs_log_error("Invalid arguments.\n");
		return NULL;
	}
	return __ntfs_create(dir_ni, name, name_len, S_IFLNK, 0,
			target, target_len);
}

/**
 * ntfs_delete - delete file or directory from ntfs volume
 * @pni:	ntfs inode for object to delete
 * @dir_ni:	ntfs inode for directory in which delete object
 * @name:	unicode name of the object to delete
 * @name_len:	length of the name in unicode characters
 *
 * @pni is pointer to pointer to ntfs_inode structure. Upon successful
 * completion and if inode is really deleted (there are no more links left to
 * it) this function will close @*pni and set it to NULL, in the other cases
 * @*pni will stay opened.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
int ntfs_delete(ntfs_inode **pni, ntfs_inode *dir_ni, ntfschar *name,
		u8 name_len)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_index_context *ictx = NULL;
	ntfs_inode *ni;
	FILE_NAME_ATTR *fn = NULL;
	BOOL looking_for_dos_name = FALSE, looking_for_win32_name = FALSE;
	BOOL case_sensitive_match = TRUE;
	int err = 0;

	ntfs_log_trace("Entering.\n");

	if (!pni || !(ni = *pni) || !dir_ni || !name || !name_len ||
			ni->nr_extents == -1 || dir_ni->nr_extents == -1) {
		ntfs_log_error("Invalid arguments.\n");
		errno = EINVAL;
		goto err_out;
	}
	if (ni->nr_references > 1 && le16_to_cpu(ni->mrec->link_count) == 1) {
		ntfs_log_error("Trying to deleting inode with left "
				"references.\n");
		errno = EINVAL;
		goto err_out;
	}
	/*
	 * Search for FILE_NAME attribute with such name. If it's in POSIX or
	 * WIN32_AND_DOS namespace, then simply remove it from index and inode.
	 * If filename in DOS or in WIN32 namespace, then remove DOS name first,
	 * only then remove WIN32 name. Mark WIN32 name as POSIX name to prevent
	 * chkdsk to complain about DOS name absence in case if DOS name had
	 * been successfully deleted, but WIN32 name remove failed.
	 */
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx)
		goto err_out;
search:
	while (!ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, CASE_SENSITIVE,
			0, NULL, 0, actx)) {
		errno = 0;
		fn = (FILE_NAME_ATTR*)((u8*)actx->attr +
				le16_to_cpu(actx->attr->value_offset));
		ntfs_log_trace("Found filename with instance number %d.\n",
				le16_to_cpu(actx->attr->instance));
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
		if (dir_ni->mft_no == MREF_LE(fn->parent_directory) &&
				ntfs_names_are_equal(fn->file_name,
				fn->file_name_length, name,
				name_len, case_sensitive_match ?
				CASE_SENSITIVE : IGNORE_CASE, ni->vol->upcase,
				ni->vol->upcase_len)) {
			if (fn->file_name_type == FILE_NAME_WIN32) {
				looking_for_dos_name = TRUE;
				ntfs_attr_reinit_search_ctx(actx);
				ntfs_log_trace("Restart search. "
						"Looking for DOS name.\n");
				continue;
			}
			if (fn->file_name_type == FILE_NAME_DOS)
				looking_for_dos_name = TRUE;
			break;
		}
	}
	if (errno) {
		/*
		 * If case sensitive search failed and volume mounted case
		 * insensitive, then try once again ignoring case.
		 */
		if (errno == ENOENT && !NVolCaseSensitive(ni->vol) &&
				case_sensitive_match) {
			case_sensitive_match = FALSE;
			ntfs_attr_reinit_search_ctx(actx);
			ntfs_log_trace("Restart search. Ignore case.");
			goto search;
		}
		ntfs_log_error("Failed to find requested filename in FILE_NAME "
				"attributes that belong to this inode.\n");
		goto err_out;
	}
	/* If deleting directory check it to be empty. */
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
		ntfs_attr *na;

		na = ntfs_attr_open(ni, AT_INDEX_ROOT, NTFS_INDEX_I30, 4);
		if (!na) {
			ntfs_log_error("Corrupt directory or library bug.\n");
			errno = EIO;
			goto err_out;
		}
		/*
		 * Do not allow non-empty directory deletion if hard links count
		 * is 1 (always) or 2 (in case if filename in DOS namespace,
		 * because we delete it first in file which have both WIN32 and
		 * DOS names).
		 */
		if ((na->data_size != sizeof(INDEX_ROOT) + sizeof(
				INDEX_ENTRY_HEADER)) && (le16_to_cpu(
				ni->mrec->link_count) == 1 ||
				(le16_to_cpu(ni->mrec->link_count) == 2 &&
				fn->file_name_type == FILE_NAME_DOS))) {
			ntfs_attr_close(na);
			ntfs_log_error("Directory is not empty.\n");
			errno = ENOTEMPTY;
			goto err_out;
		}
		ntfs_attr_close(na);
	}
	/* One more sanity check. */
	if (ni->nr_references > 1 && looking_for_dos_name &&
			le16_to_cpu(ni->mrec->link_count) == 2) {
		ntfs_log_error("Trying to deleting inode with left "
				"references.\n");
		errno = EINVAL;
		goto err_out;
	}
	ntfs_log_trace("Found!\n");
	/* Search for such FILE_NAME in index. */
	ictx = ntfs_index_ctx_get(dir_ni, NTFS_INDEX_I30, 4);
	if (!ictx)
		goto err_out;
	if (ntfs_index_lookup(fn, le32_to_cpu(actx->attr->value_length), ictx))
		goto err_out;
	/* Set namespace to POSIX for WIN32 name. */
	if (fn->file_name_type == FILE_NAME_WIN32) {
		fn->file_name_type = FILE_NAME_POSIX;
		ntfs_inode_mark_dirty(actx->ntfs_ino);
		((FILE_NAME_ATTR*)ictx->data)->file_name_type = FILE_NAME_POSIX;
		ntfs_index_entry_mark_dirty(ictx);
	}
	/* Do not support reparse point deletion yet. */
	if (((FILE_NAME_ATTR*)ictx->data)->file_attributes &
			FILE_ATTR_REPARSE_POINT) {
		errno = EOPNOTSUPP;
		goto err_out;
	}
	/* Remove FILE_NAME from index. */
	if (ntfs_index_rm(ictx))
		goto err_out;

	/* Remove FILE_NAME from inode. */
	if (ntfs_attr_record_rm(actx))
		goto err_out;
	/* Decrement hard link count. */
	ni->mrec->link_count = cpu_to_le16(le16_to_cpu(
			ni->mrec->link_count) - 1);
	ntfs_inode_mark_dirty(ni);
	if (looking_for_dos_name) {
		looking_for_dos_name = FALSE;
		looking_for_win32_name = TRUE;
		ntfs_attr_reinit_search_ctx(actx);
		ntfs_log_trace("DOS name deleted. "
				"Now search for WIN32 name.\n");
		goto search;
	} else
		ntfs_log_trace("Deleted.\n");
	/* TODO: Update object id, quota and security indexes if required. */
	/*
	 * If hard link count is not equal to zero then we are done. In other
	 * case there are no reference to this inode left, so we should free all
	 * non-resident attributes and mark all MFT record as not in use.
	 */
	if (ni->mrec->link_count)
		goto out;
	ntfs_attr_reinit_search_ctx(actx);
	while (!ntfs_attrs_walk(actx)) {
		if (actx->attr->non_resident) {
			runlist *rl;

			rl = ntfs_mapping_pairs_decompress(ni->vol, actx->attr,
					NULL);
			if (!rl) {
				err = errno;
				ntfs_log_error("Failed to decompress runlist.  "
						"Leaving inconsistent "
						"metadata.\n");
				continue;
			}
			if (ntfs_cluster_free_from_rl(ni->vol, rl)) {
				err = errno;
				ntfs_log_error("Failed to free clusters.  "
						"Leaving inconsistent "
						"metadata.\n");
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
	while (ni->nr_extents)
		if (ntfs_mft_record_free(ni->vol, *(ni->extent_nis))) {
			err = errno;
			ntfs_log_error("Failed to free extent MFT record.  "
					"Leaving inconsistent metadata.\n");
		}
	if (ntfs_mft_record_free(ni->vol, ni)) {
		err = errno;
		ntfs_log_error("Failed to free base MFT record.  "
				"Leaving inconsistent metadata.\n");
	}
	*pni = NULL;
out:
	if (actx)
		ntfs_attr_put_search_ctx(actx);
	if (ictx)
		ntfs_index_ctx_put(ictx);
	if (err) {
		ntfs_log_error("%s(): Failed.\n", __FUNCTION__);
		errno = err;
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
 * NOTE: At present we allow creating hard links to directories, we use them
 * in a temporary state during rename. But it's definitely bad idea to have
 * hard links to directories as a result of operation.
 * FIXME: Create internal  __ntfs_link that allows hard links to a directories
 * and external ntfs_link that do not. Write ntfs_rename that uses __ntfs_link.
 *
 * Return 0 on success or -1 on error with errno set to the error code.
 */
int ntfs_link(ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name, u8 name_len)
{
	FILE_NAME_ATTR *fn = NULL;
	int fn_len, err;

	ntfs_log_trace("Entering.\n");

	if (!ni || !dir_ni || !name || !name_len ||
			ni->mft_no == dir_ni->mft_no) {
		err = EINVAL;
		ntfs_log_error("Invalid arguments.");
		goto err_out;
	}
	/* FIXME: Reparse points requires special handling. */
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
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
	fn->file_name_type = FILE_NAME_POSIX;
	fn->file_attributes = ni->flags;
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
		fn->file_attributes |= FILE_ATTR_I30_INDEX_PRESENT;
	fn->allocated_size = cpu_to_sle64(ni->allocated_size);
	fn->data_size = cpu_to_sle64(ni->data_size);
	fn->creation_time = utc2ntfs(ni->creation_time);
	fn->last_data_change_time = utc2ntfs(ni->last_data_change_time);
	fn->last_mft_change_time = utc2ntfs(ni->last_mft_change_time);
	fn->last_access_time = utc2ntfs(ni->last_access_time);
	memcpy(fn->file_name, name, name_len * sizeof(ntfschar));
	/* Add FILE_NAME attribute to index. */
	if (ntfs_index_add_filename(dir_ni, fn, MK_MREF(ni->mft_no,
			le16_to_cpu(ni->mrec->sequence_number)))) {
		err = errno;
		ntfs_log_error("Failed to add entry to the index.\n");
		goto err_out;
	}
	/* Add FILE_NAME attribute to inode. */
	if (ntfs_attr_add(ni, AT_FILE_NAME, AT_UNNAMED, 0, (u8*)fn, fn_len)) {
		ntfs_index_context *ictx;

		err = errno;
		ntfs_log_error("Failed to add FILE_NAME attribute.\n");
		/* Try to remove just added attribute from index. */
		ictx = ntfs_index_ctx_get(dir_ni, NTFS_INDEX_I30, 4);
		if (!ictx)
			goto rollback_failed;
		if (ntfs_index_lookup(fn, fn_len, ictx)) {
			ntfs_index_ctx_put(ictx);
			goto rollback_failed;
		}
		if (ntfs_index_rm(ictx)) {
			ntfs_index_ctx_put(ictx);
			goto rollback_failed;
		}
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
	ntfs_log_error("%s(): Failed.\n", __FUNCTION__);
	free(fn);
	errno = err;
	return -1;
}

