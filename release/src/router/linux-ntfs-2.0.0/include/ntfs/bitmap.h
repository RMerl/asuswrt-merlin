/*
 * bitmap.h - Exports for bitmap handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
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

#ifndef _NTFS_BITMAP_H
#define _NTFS_BITMAP_H

#include "types.h"
#include "attrib.h"

/*
 * NOTES:
 *
 * - Operations are 8-bit only to ensure the functions work both on little
 *   and big endian machines! So don't make them 32-bit ops!
 * - bitmap starts at bit = 0 and ends at bit = bitmap size - 1.
 * - _Caller_ has to make sure that the bit to operate on is less than the
 *   size of the bitmap.
 */

/**
 * ntfs_bit_set - set a bit in a field of bits
 * @bitmap:	field of bits
 * @bit:	bit to set
 * @new_value:	value to set bit to (0 or 1)
 *
 * Set the bit @bit in the @bitmap to @new_value. Ignore all errors.
 */
static __inline__ void ntfs_bit_set(u8 *bitmap, const u64 bit,
		const u8 new_value)
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
static __inline__ char ntfs_bit_get(const u8 *bitmap, const u64 bit)
{
	if (!bitmap)
		return -1;
	return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

static __inline__ void ntfs_bit_change(u8 *bitmap, const u64 bit)
{
	if (!bitmap)
		return;
	bitmap[bit >> 3] ^= 1 << (bit & 7);
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
static __inline__ char ntfs_bit_get_and_set(u8 *bitmap, const u64 bit,
		const u8 new_value)
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

extern int ntfs_bitmap_set_run(ntfs_attr *na, s64 start_bit, s64 count);
extern int ntfs_bitmap_clear_run(ntfs_attr *na, s64 start_bit, s64 count);

/**
 * ntfs_bitmap_set_bit - set a bit in a bitmap
 * @na:		attribute containing the bitmap
 * @bit:	bit to set
 *
 * Set the @bit in the bitmap described by the attribute @na.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
static __inline__ int ntfs_bitmap_set_bit(ntfs_attr *na, s64 bit)
{
	return ntfs_bitmap_set_run(na, bit, 1);
}

/**
 * ntfs_bitmap_clear_bit - clear a bit in a bitmap
 * @na:		attribute containing the bitmap
 * @bit:	bit to clear
 *
 * Clear @bit in the bitmap described by the attribute @na.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
static __inline__ int ntfs_bitmap_clear_bit(ntfs_attr *na, s64 bit)
{
	return ntfs_bitmap_clear_run(na, bit, 1);
}

#endif /* defined _NTFS_BITMAP_H */
