/**
 * compress.c - Compressed attribute handling code.  Part of the Linux-NTFS
 *		project.
 *
 * Copyright (c) 2004-2005 Anton Altaparmakov
 * Copyright (c)      2005 Yura Pakhuchiy
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

#include "attrib.h"
#include "debug.h"
#include "volume.h"
#include "types.h"
#include "layout.h"
#include "runlist.h"
#include "compress.h"
#include "logging.h"

/**
 * enum ntfs_compression_constants - constants used in the compression code
 */
typedef enum {
	/* Token types and access mask. */
	NTFS_SYMBOL_TOKEN	=	0,
	NTFS_PHRASE_TOKEN	=	1,
	NTFS_TOKEN_MASK		=	1,

	/* Compression sub-block constants. */
	NTFS_SB_SIZE_MASK	=	0x0fff,
	NTFS_SB_SIZE		=	0x1000,
	NTFS_SB_IS_COMPRESSED	=	0x8000,
} ntfs_compression_constants;

/**
 * ntfs_decompress - decompress a compression block into an array of pages
 * @dest:	buffer to which to write the decompressed data
 * @dest_size:	size of buffer @dest in bytes
 * @cb_start:	compression block to decompress
 * @cb_size:	size of compression block @cb_start in bytes
 *
 * This decompresses the compression block @cb_start into the destination
 * buffer @dest.
 *
 * @cb_start is a pointer to the compression block which needs decompressing
 * and @cb_size is the size of @cb_start in bytes (8-64kiB).
 *
 * Return 0 if success or -EOVERFLOW on error in the compressed stream.
 */
static int ntfs_decompress(u8 *dest, const u32 dest_size,
		u8 *const cb_start, const u32 cb_size)
{
	/*
	 * Pointers into the compressed data, i.e. the compression block (cb),
	 * and the therein contained sub-blocks (sb).
	 */
	u8 *cb_end = cb_start + cb_size; /* End of cb. */
	u8 *cb = cb_start;	/* Current position in cb. */
	u8 *cb_sb_start = cb;	/* Beginning of the current sb in the cb. */
	u8 *cb_sb_end;		/* End of current sb / beginning of next sb. */
	/* Variables for uncompressed data / destination. */
	u8 *dest_end = dest + dest_size;	/* End of dest buffer. */
	u8 *dest_sb_start;	/* Start of current sub-block in dest. */
	u8 *dest_sb_end;	/* End of current sb in dest. */
	/* Variables for tag and token parsing. */
	u8 tag;			/* Current tag. */
	int token;		/* Loop counter for the eight tokens in tag. */

	ntfs_log_trace("Entering, cb_size = 0x%x.\n", (unsigned)cb_size);
do_next_sb:
	ntfs_log_debug("Beginning sub-block at offset = 0x%x in the cb.\n",
			cb - cb_start);
	/*
	 * Have we reached the end of the compression block or the end of the
	 * decompressed data?  The latter can happen for example if the current
	 * position in the compression block is one byte before its end so the
	 * first two checks do not detect it.
	 */
	if (cb == cb_end || !le16_to_cpup((u16*)cb) || dest == dest_end) {
		ntfs_log_debug("Completed. Returning success (0).\n");
		return 0;
	}
	/* Setup offset for the current sub-block destination. */
	dest_sb_start = dest;
	dest_sb_end = dest + NTFS_SB_SIZE;
	/* Check that we are still within allowed boundaries. */
	if (dest_sb_end > dest_end)
		goto return_overflow;
	/* Does the minimum size of a compressed sb overflow valid range? */
	if (cb + 6 > cb_end)
		goto return_overflow;
	/* Setup the current sub-block source pointers and validate range. */
	cb_sb_start = cb;
	cb_sb_end = cb_sb_start + (le16_to_cpup((u16*)cb) & NTFS_SB_SIZE_MASK)
			+ 3;
	if (cb_sb_end > cb_end)
		goto return_overflow;
	/* Now, we are ready to process the current sub-block (sb). */
	if (!(le16_to_cpup((u16*)cb) & NTFS_SB_IS_COMPRESSED)) {
		ntfs_log_debug("Found uncompressed sub-block.\n");
		/* This sb is not compressed, just copy it into destination. */
		/* Advance source position to first data byte. */
		cb += 2;
		/* An uncompressed sb must be full size. */
		if (cb_sb_end - cb != NTFS_SB_SIZE)
			goto return_overflow;
		/* Copy the block and advance the source position. */
		memcpy(dest, cb, NTFS_SB_SIZE);
		cb += NTFS_SB_SIZE;
		/* Advance destination position to next sub-block. */
		dest += NTFS_SB_SIZE;
		goto do_next_sb;
	}
	ntfs_log_debug("Found compressed sub-block.\n");
	/* This sb is compressed, decompress it into destination. */
	/* Forward to the first tag in the sub-block. */
	cb += 2;
do_next_tag:
	if (cb == cb_sb_end) {
		/* Check if the decompressed sub-block was not full-length. */
		if (dest < dest_sb_end) {
			int nr_bytes = dest_sb_end - dest;

			ntfs_log_debug("Filling incomplete sub-block with zeroes.\n");
			/* Zero remainder and update destination position. */
			memset(dest, 0, nr_bytes);
			dest += nr_bytes;
		}
		/* We have finished the current sub-block. */
		goto do_next_sb;
	}
	/* Check we are still in range. */
	if (cb > cb_sb_end || dest > dest_sb_end)
		goto return_overflow;
	/* Get the next tag and advance to first token. */
	tag = *cb++;
	/* Parse the eight tokens described by the tag. */
	for (token = 0; token < 8; token++, tag >>= 1) {
		u16 lg, pt, length, max_non_overlap;
		register u16 i;
		u8 *dest_back_addr;

		/* Check if we are done / still in range. */
		if (cb >= cb_sb_end || dest > dest_sb_end)
			break;
		/* Determine token type and parse appropriately.*/
		if ((tag & NTFS_TOKEN_MASK) == NTFS_SYMBOL_TOKEN) {
			/*
			 * We have a symbol token, copy the symbol across, and
			 * advance the source and destination positions.
			 */
			*dest++ = *cb++;
			/* Continue with the next token. */
			continue;
		}
		/*
		 * We have a phrase token. Make sure it is not the first tag in
		 * the sb as this is illegal and would confuse the code below.
		 */
		if (dest == dest_sb_start)
			goto return_overflow;
		/*
		 * Determine the number of bytes to go back (p) and the number
		 * of bytes to copy (l). We use an optimized algorithm in which
		 * we first calculate log2(current destination position in sb),
		 * which allows determination of l and p in O(1) rather than
		 * O(n). We just need an arch-optimized log2() function now.
		 */
		lg = 0;
		for (i = dest - dest_sb_start - 1; i >= 0x10; i >>= 1)
			lg++;
		/* Get the phrase token into i. */
		pt = le16_to_cpup((u16*)cb);
		/*
		 * Calculate starting position of the byte sequence in
		 * the destination using the fact that p = (pt >> (12 - lg)) + 1
		 * and make sure we don't go too far back.
		 */
		dest_back_addr = dest - (pt >> (12 - lg)) - 1;
		if (dest_back_addr < dest_sb_start)
			goto return_overflow;
		/* Now calculate the length of the byte sequence. */
		length = (pt & (0xfff >> lg)) + 3;
		/* Verify destination is in range. */
		if (dest + length > dest_sb_end)
			goto return_overflow;
		/* The number of non-overlapping bytes. */
		max_non_overlap = dest - dest_back_addr;
		if (length <= max_non_overlap) {
			/* The byte sequence doesn't overlap, just copy it. */
			memcpy(dest, dest_back_addr, length);
			/* Advance destination pointer. */
			dest += length;
		} else {
			/*
			 * The byte sequence does overlap, copy non-overlapping
			 * part and then do a slow byte by byte copy for the
			 * overlapping part. Also, advance the destination
			 * pointer.
			 */
			memcpy(dest, dest_back_addr, max_non_overlap);
			dest += max_non_overlap;
			dest_back_addr += max_non_overlap;
			length -= max_non_overlap;
			while (length--)
				*dest++ = *dest_back_addr++;
		}
		/* Advance source position and continue with the next token. */
		cb += 2;
	}
	/* No tokens left in the current tag. Continue with the next tag. */
	goto do_next_tag;
return_overflow:
	ntfs_log_debug("Failed. Returning -EOVERFLOW.\n");
	errno = EOVERFLOW;
	return -1;
}

/**
 * ntfs_is_cb_compressed - internal function, do not use
 *
 * This is a very specialised function determining if a cb is compressed or
 * uncompressed.  It is assumed that checking for a sparse cb has already been
 * performed and that the cb is not sparse.  It makes all sorts of other
 * assumptions as well and hence it is not useful anywhere other than where it
 * is used at the moment.  Please, do not make this function available for use
 * outside of compress.c as it is bound to confuse people and not do what they
 * want.
 *
 * Return TRUE on errors so that the error will be detected later on in the
 * code.  Might be a bit confusing to debug but there really should never be
 * errors coming from here.
 */
static BOOL ntfs_is_cb_compressed(ntfs_attr *na,
		runlist_element *rl, VCN cb_start_vcn, int cb_clusters)
{
	/*
	 * The simplest case: the run starting at @cb_start_vcn contains
	 * @cb_clusters clusters which are all not sparse, thus the cb is not
	 * compressed.
	 */
restart:
	cb_clusters -= rl->length - (cb_start_vcn - rl->vcn);
	while (cb_clusters > 0) {
		/* Go to the next run. */
		rl++;
		/* Map the next runlist fragment if it is not mapped. */
		if (rl->lcn < LCN_HOLE || !rl->length) {
			cb_start_vcn = rl->vcn;
			rl = ntfs_attr_find_vcn(na, rl->vcn);
			if (!rl || rl->lcn < LCN_HOLE || !rl->length)
				return TRUE;
			/*
			 * If the runs were merged need to deal with the
			 * resulting partial run so simply restart.
			 */
			if (rl->vcn < cb_start_vcn)
				goto restart;
		}
		/* If the current run is sparse, the cb is compressed. */
		if (rl->lcn == LCN_HOLE)
			return TRUE;
		/* If the whole cb is not sparse, it is not compressed. */
		if (rl->length >= cb_clusters)
			return FALSE;
		cb_clusters -= rl->length;
	};
	/* All cb_clusters were not sparse thus the cb is not compressed. */
	return FALSE;
}

/**
 * ntfs_compressed_attr_pread - read from a compressed attribute
 * @na:		ntfs attribute to read from
 * @pos:	byte position in the attribute to begin reading from
 * @count:	number of bytes to read
 * @b:		output data buffer
 *
 * NOTE:  You probably want to be using attrib.c::ntfs_attr_pread() instead.
 *
 * This function will read @count bytes starting at offset @pos from the
 * compressed ntfs attribute @na into the data buffer @b.
 *
 * On success, return the number of successfully read bytes.  If this number
 * is lower than @count this means that the read reached end of file or that
 * an error was encountered during the read so that the read is partial.
 * 0 means end of file or nothing was read (also return 0 when @count is 0).
 *
 * On error and nothing has been read, return -1 with errno set appropriately
 * to the return code of ntfs_pread(), or to EINVAL in case of invalid
 * arguments.
 */
s64 ntfs_compressed_attr_pread(ntfs_attr *na, s64 pos, s64 count, void *b)
{
	s64 br, to_read, ofs, total, total2;
	u64 cb_size_mask;
	VCN start_vcn, vcn, end_vcn;
	ntfs_volume *vol;
	runlist_element *rl;
	u8 *dest, *cb, *cb_pos, *cb_end;
	u32 cb_size;
	int err;
	unsigned int nr_cbs, cb_clusters;

	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, pos 0x%llx, count 0x%llx.\n",
			(unsigned long long)na->ni->mft_no, na->type,
			(long long)pos, (long long)count);
	if (!na || !NAttrCompressed(na) || !na->ni || !na->ni->vol || !b ||
			pos < 0 || count < 0) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * Encrypted attributes are not supported.  We return access denied,
	 * which is what Windows NT4 does, too.
	 */
	if (NAttrEncrypted(na)) {
		errno = EACCES;
		return -1;
	}
	if (!count)
		return 0;
	/* Truncate reads beyond end of attribute. */
	if (pos + count > na->data_size) {
		if (pos >= na->data_size) {
			return 0;
		}
		count = na->data_size - pos;
	}
	/* If it is a resident attribute, simply use ntfs_attr_pread(). */
	if (!NAttrNonResident(na))
		return ntfs_attr_pread(na, pos, count, b);
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
	vol = na->ni->vol;
	cb_size = na->compression_block_size;
	cb_size_mask = cb_size - 1UL;
	cb_clusters = na->compression_block_clusters;

	/* Need a temporary buffer for each loaded compression block. */
	cb = ntfs_malloc(cb_size);
	if (!cb)
		return -1;

	/* Need a temporary buffer for each uncompressed block. */
	dest = ntfs_malloc(cb_size);
	if (!dest) {
		err = errno;
		free(cb);
		errno = err;
		return -1;
	}
	/*
	 * The first vcn in the first compression block (cb) which we need to
	 * decompress.
	 */
	start_vcn = (pos & ~cb_size_mask) >> vol->cluster_size_bits;
	/* Offset in the uncompressed cb at which to start reading data. */
	ofs = pos & cb_size_mask;
	/*
	 * The first vcn in the cb after the last cb which we need to
	 * decompress.
	 */
	end_vcn = ((pos + count + cb_size - 1) & ~cb_size_mask) >>
			vol->cluster_size_bits;
	/* Number of compression blocks (cbs) in the wanted vcn range. */
	nr_cbs = (end_vcn - start_vcn) << vol->cluster_size_bits >>
			na->compression_block_size_bits;
	cb_end = cb + cb_size;
do_next_cb:
	nr_cbs--;
	cb_pos = cb;
	vcn = start_vcn;
	start_vcn += cb_clusters;

	/* Check whether the compression block is sparse. */
	rl = ntfs_attr_find_vcn(na, vcn);
	if (!rl || rl->lcn < LCN_HOLE) {
		free(cb);
		free(dest);
		if (total)
			return total;
		/* FIXME: Do we want EIO or the error code? (AIA) */
		errno = EIO;
		return -1;
	}
	if (rl->lcn == LCN_HOLE) {
		/* Sparse cb, zero out destination range overlapping the cb. */
		ntfs_log_debug("Found sparse compression block.\n");
		to_read = min(count, cb_size - ofs);
		memset(b, 0, to_read);
		ofs = 0;
		total += to_read;
		count -= to_read;
		b = (u8*)b + to_read;
	} else if (!ntfs_is_cb_compressed(na, rl, vcn, cb_clusters)) {
		s64 tdata_size, tinitialized_size;
		/*
		 * Uncompressed cb, read it straight into the destination range
		 * overlapping the cb.
		 */
		ntfs_log_debug("Found uncompressed compression block.\n");
		/*
		 * Read the uncompressed data into the destination buffer.
		 * NOTE: We cheat a little bit here by marking the attribute as
		 * not compressed in the ntfs_attr structure so that we can
		 * read the data by simply using ntfs_attr_pread().  (-8
		 * NOTE: we have to modify data_size and initialized_size
		 * temporarily as well...
		 */
		to_read = min(count, cb_size - ofs);
		ofs += vcn << vol->cluster_size_bits;
		NAttrClearCompressed(na);
		tdata_size = na->data_size;
		tinitialized_size = na->initialized_size;
		na->data_size = na->initialized_size = na->allocated_size;
		do {
			br = ntfs_attr_pread(na, ofs, to_read, b);
			if (br < 0) {
				err = errno;
				na->data_size = tdata_size;
				na->initialized_size = tinitialized_size;
				NAttrSetCompressed(na);
				free(cb);
				free(dest);
				if (total)
					return total;
				errno = err;
				return br;
			}
			total += br;
			count -= br;
			b = (u8*)b + br;
			to_read -= br;
			ofs += br;
		} while (to_read > 0);
		na->data_size = tdata_size;
		na->initialized_size = tinitialized_size;
		NAttrSetCompressed(na);
		ofs = 0;
	} else {
		s64 tdata_size, tinitialized_size;

		/*
		 * Compressed cb, decompress it into the temporary buffer, then
		 * copy the data to the destination range overlapping the cb.
		 */
		ntfs_log_debug("Found compressed compression block.\n");
		/*
		 * Read the compressed data into the temporary buffer.
		 * NOTE: We cheat a little bit here by marking the attribute as
		 * not compressed in the ntfs_attr structure so that we can
		 * read the raw, compressed data by simply using
		 * ntfs_attr_pread().  (-8
		 * NOTE: We have to modify data_size and initialized_size
		 * temporarily as well...
		 */
		to_read = cb_size;
		NAttrClearCompressed(na);
		tdata_size = na->data_size;
		tinitialized_size = na->initialized_size;
		na->data_size = na->initialized_size = na->allocated_size;
		do {
			br = ntfs_attr_pread(na,
					(vcn << vol->cluster_size_bits) +
					(cb_pos - cb), to_read, cb_pos);
			if (br < 0) {
				err = errno;
				na->data_size = tdata_size;
				na->initialized_size = tinitialized_size;
				NAttrSetCompressed(na);
				free(cb);
				free(dest);
				if (total)
					return total;
				errno = err;
				return br;
			}
			cb_pos += br;
			to_read -= br;
		} while (to_read > 0);
		na->data_size = tdata_size;
		na->initialized_size = tinitialized_size;
		NAttrSetCompressed(na);
		/* Just a precaution. */
		if (cb_pos + 2 <= cb_end)
			*(u16*)cb_pos = 0;
		ntfs_log_debug("Successfully read the compression block.\n");
		if (ntfs_decompress(dest, cb_size, cb, cb_size) < 0) {
			err = errno;
			free(cb);
			free(dest);
			if (total)
				return total;
			errno = err;
			return -1;
		}
		to_read = min(count, cb_size - ofs);
		memcpy(b, dest + ofs, to_read);
		total += to_read;
		count -= to_read;
		b = (u8*)b + to_read;
		ofs = 0;
	}
	/* Do we have more work to do? */
	if (nr_cbs)
		goto do_next_cb;
	/* We no longer need the buffers. */
	free(cb);
	free(dest);
	/* Return number of bytes read. */
	return total + total2;
}
