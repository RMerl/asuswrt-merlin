/**
 * bitmap.c - Bitmap handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2006 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2004-2008 Szabolcs Szakacsits
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "attrib.h"
#include "bitmap.h"
#include "debug.h"
#include "logging.h"
#include "misc.h"

/**
 * ntfs_bit_set - set a bit in a field of bits
 * @bitmap:	field of bits
 * @bit:	bit to set
 * @new_value:	value to set bit to (0 or 1)
 *
 * Set the bit @bit in the @bitmap to @new_value. Ignore all errors.
 */
void ntfs_bit_set(u8 *bitmap, const u64 bit, const u8 new_value)
{
	if (!bitmap || new_value > 1)
		return;
	if (!new_value)
		bitmap[bit >> 3] &= ~(1 << (bit & 7));
	else
		bitmap[bit >> 3] |= (1 << (bit & 7));
}

/**
 * ntfs_bit_get - get value of a bit in a field of bits
 * @bitmap:	field of bits
 * @bit:	bit to get
 *
 * Get and return the value of the bit @bit in @bitmap (0 or 1).
 * Return -1 on error.
 */
char ntfs_bit_get(const u8 *bitmap, const u64 bit)
{
	if (!bitmap)
		return -1;
	return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

/**
 * ntfs_bit_get_and_set - get value of a bit in a field of bits and set it
 * @bitmap:	field of bits
 * @bit:	bit to get/set
 * @new_value:	value to set bit to (0 or 1)
 *
 * Return the value of the bit @bit and set it to @new_value (0 or 1).
 * Return -1 on error.
 */
char ntfs_bit_get_and_set(u8 *bitmap, const u64 bit, const u8 new_value)
{
	register u8 old_bit, shift;

	if (!bitmap || new_value > 1)
		return -1;
	shift = bit & 7;
	old_bit = (bitmap[bit >> 3] >> shift) & 1;
	if (new_value != old_bit)
		bitmap[bit >> 3] ^= 1 << shift;
	return old_bit;
}

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
	s64 bufsize, br;
	u8 *buf, *lastbyte_buf;
	int bit, firstbyte, lastbyte, lastbyte_pos, tmp, ret = -1;

	if (!na || start_bit < 0 || count < 0) {
		errno = EINVAL;
		ntfs_log_perror("%s: Invalid argument (%p, %lld, %lld)",
			__FUNCTION__, na, (long long)start_bit, (long long)count);
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

	buf = ntfs_malloc(bufsize);
	if (!buf)
		return -1;
	
	/* Depending on @value, zero or set all bits in the allocated buffer. */
	memset(buf, value ? 0xff : 0, bufsize);

	/* If there is a first partial byte... */
	if (bit) {
		/* read it in... */
		br = ntfs_attr_pread(na, start_bit >> 3, 1, buf);
		if (br != 1) {
			if (br >= 0)
				errno = EIO;
			goto free_err_out;
		}
		/* and set or clear the appropriate bits in it. */
		while ((bit & 7) && count--) {
			if (value)
				*buf |= 1 << bit++;
			else
				*buf &= ~(1 << bit++);
		}
		/* Update @start_bit to the new position. */
		start_bit = (start_bit + 7) & ~7;
	}

	/* Loop until @count reaches zero. */
	lastbyte = 0;
	lastbyte_buf = NULL;
	bit = count & 7;
	do {
		/* If there is a last partial byte... */
		if (count > 0 && bit) {
			lastbyte_pos = ((count + 7) >> 3) + firstbyte;
			if (!lastbyte_pos) {
				// FIXME: Eeek! BUG!
				ntfs_log_error("Lastbyte is zero. Leaving "
						"inconsistent metadata.\n");
				errno = EIO;
				goto free_err_out;
			}
			/* and it is in the currently loaded bitmap window... */
			if (lastbyte_pos <= bufsize) {
				lastbyte_buf = buf + lastbyte_pos - 1;

				/* read the byte in... */
				br = ntfs_attr_pread(na, (start_bit + count) >>
						3, 1, lastbyte_buf);
				if (br != 1) {
					// FIXME: Eeek! We need rollback! (AIA)
					if (br >= 0)
						errno = EIO;
					ntfs_log_perror("Reading of last byte "
						"failed (%lld). Leaving inconsistent "
						"metadata", (long long)br);
					goto free_err_out;
				}
				/* and set/clear the appropriate bits in it. */
				while (bit && count--) {
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
			if (br >= 0)
				errno = EIO;
			ntfs_log_perror("Failed to write buffer to bitmap "
				"(%lld != %lld). Leaving inconsistent metadata",
				(long long)br, (long long)bufsize);
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
		count -= tmp;
		if (bufsize > (tmp = (count + 7) >> 3))
			bufsize = tmp;

		if (lastbyte && count != 0) {
			// FIXME: Eeek! BUG!
			ntfs_log_error("Last buffer but count is not zero "
				       "(%lld). Leaving inconsistent metadata.\n",
				       (long long)count);
			errno = EIO;
			goto free_err_out;
		}
	} while (count > 0);
	
	ret = 0;
	
free_err_out:
	free(buf);
	return ret;
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
	int ret; 
	
	ntfs_log_enter("Set from bit %lld, count %lld\n",
		       (long long)start_bit, (long long)count);
	ret = ntfs_bitmap_set_bits_in_run(na, start_bit, count, 1);
	ntfs_log_leave("\n");
	return ret;
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
	int ret; 
	
	ntfs_log_enter("Clear from bit %lld, count %lld\n",
		       (long long)start_bit, (long long)count);
	ret = ntfs_bitmap_set_bits_in_run(na, start_bit, count, 0);
	ntfs_log_leave("\n");
	return ret;
}

