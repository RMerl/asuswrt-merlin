/**
 * bitmap.c - Bitmap handling code. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2006 Anton Altaparmakov
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
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "attrib.h"
#include "bitmap.h"
#include "debug.h"
#include "logging.h"

/**
 * ntfs_bitmap_set_bits_in_run - set a run of bits in a bitmap to a value
 * @na:		attribute containing the bitmap
 * @start_bit:	first bit to set
 * @count:	number of bits to set
 * @value:	value to set the bits to (i.e. 0 or 1)
 *
 * Set @count bits starting at bit @start_bit in the bitmap described by the
 * attribute @na to @value, where @value is either 0 or 1.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
static int ntfs_bitmap_set_bits_in_run(ntfs_attr *na, s64 start_bit,
		s64 count, int value)
{
	ntfs_volume *vol = na->ni->vol;
	s64 bufsize, br, left = count;
	u8 *buf, *lastbyte_buf;
	int bit, firstbyte, lastbyte, lastbyte_pos, tmp, err;

	if (!na || start_bit < 0 || count < 0) {
		errno = EINVAL;
		return -1;
	}

	bit = start_bit & 7;
	if (bit)
		firstbyte = 1;
	else
		firstbyte = 0;

	/* Calculate the required buffer size in bytes, capping it at 8kiB. */
	bufsize = ((count - (bit ? 8 - bit : 0) + 7) >> 3) + firstbyte;
	if (bufsize > 8192)
		bufsize = 8192;

	buf = (u8*)ntfs_malloc(bufsize);
	if (!buf)
		return -1;

	/* Depending on @value, zero or set all bits in the allocated buffer. */
	memset(buf, value ? 0xff : 0, bufsize);

	/* If there is a first partial byte... */
	if (bit) {
		/* read it in... */
		br = ntfs_attr_pread(na, start_bit >> 3, 1, buf);
		if (br != 1) {
			free(buf);
			errno = EIO;
			return -1;
		}
		/* and set or clear the appropriate bits in it. */
		while ((bit & 7) && left--) {
			if (value)
				*buf |= 1 << bit++;
			else
				*buf &= ~(1 << bit++);
		}
		/* Update @start_bit to the new position. */
		start_bit = (start_bit + 7) & ~7;
	}

	/* Loop until @left reaches zero. */
	lastbyte = 0;
	lastbyte_buf = NULL;
	bit = left & 7;
	do {
		/* If there is a last partial byte... */
		if (left > 0 && bit) {
			lastbyte_pos = ((left + 7) >> 3) + firstbyte;
			if (!lastbyte_pos) {
				// FIXME: Eeek! BUG!
				ntfs_log_trace("lastbyte is zero. Leaving "
						"inconsistent metadata.\n");
				err = EIO;
				goto free_err_out;
			}
			/* and it is in the currently loaded bitmap window... */
			if (lastbyte_pos <= bufsize) {
				lastbyte_buf = buf + lastbyte_pos - 1;

				/* read the byte in... */
				br = ntfs_attr_pread(na, (start_bit + left) >>
						3, 1, lastbyte_buf);
				if (br != 1) {
					// FIXME: Eeek! We need rollback! (AIA)
					ntfs_log_trace("Read of last byte "
							"failed. Leaving "
							"inconsistent "
							"metadata.\n");
					err = EIO;
					goto free_err_out;
				}
				/* and set/clear the appropriate bits in it. */
				while (bit && left--) {
					if (value)
						*lastbyte_buf |= 1 << --bit;
					else
						*lastbyte_buf &= ~(1 << --bit);
				}
				/* We don't want to come back here... */
				bit = 0;
				/* We have a last byte that we have handled. */
				lastbyte = 1;
			}
		}

		/* Write the prepared buffer to disk. */
		tmp = (start_bit >> 3) - firstbyte;
		br = ntfs_attr_pwrite(na, tmp, bufsize, buf);
		if (br != bufsize) {
			// FIXME: Eeek! We need rollback! (AIA)
			ntfs_log_trace("Failed to write buffer to bitmap. "
					"Leaving inconsistent metadata.\n");
			err = EIO;
			goto free_err_out;
		}

		/* Update counters. */
		tmp = (bufsize - firstbyte - lastbyte) << 3;
		if (firstbyte) {
			firstbyte = 0;
			/*
			 * Re-set the partial first byte so a subsequent write
			 * of the buffer does not have stale, incorrect bits.
			 */
			*buf = value ? 0xff : 0;
		}
		start_bit += tmp;
		left -= tmp;
		if (bufsize > (tmp = (left + 7) >> 3))
			bufsize = tmp;

		if (lastbyte && left != 0) {
			// FIXME: Eeek! BUG!
			ntfs_log_trace("Last buffer but count is not zero (= "
					"%lli). Leaving inconsistent metadata."
					"\n", (long long)left);
			err = EIO;
			goto free_err_out;
		}
	} while (left > 0);

	/* Update free clusters and MFT records. */
	if (na == vol->mftbmp_na) {
		if (value)
			vol->nr_free_mft_records -= count;
		else
			vol->nr_free_mft_records += count;
	}
	if (na == vol->lcnbmp_na) {
		if (value)
			vol->nr_free_clusters -= count;
		else
			vol->nr_free_clusters += count;
	}

	/* Done! */
	free(buf);
	return 0;

free_err_out:
	free(buf);
	errno = err;
	return -1;
}

/**
 * ntfs_bitmap_set_run - set a run of bits in a bitmap
 * @na:		attribute containing the bitmap
 * @start_bit:	first bit to set
 * @count:	number of bits to set
 *
 * Set @count bits starting at bit @start_bit in the bitmap described by the
 * attribute @na.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_bitmap_set_run(ntfs_attr *na, s64 start_bit, s64 count)
{
	return ntfs_bitmap_set_bits_in_run(na, start_bit, count, 1);
}

/**
 * ntfs_bitmap_clear_run - clear a run of bits in a bitmap
 * @na:		attribute containing the bitmap
 * @start_bit:	first bit to clear
 * @count:	number of bits to clear
 *
 * Clear @count bits starting at bit @start_bit in the bitmap described by the
 * attribute @na.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_bitmap_clear_run(ntfs_attr *na, s64 start_bit, s64 count)
{
	ntfs_log_trace("Dealloc from bit 0x%llx, count 0x%llx.\n",
		       (long long)start_bit, (long long)count);

	return ntfs_bitmap_set_bits_in_run(na, start_bit, count, 0);
}

