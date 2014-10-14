/*
 * Copyright (c) 2000,2002,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __XFS_BIT_H__
#define	__XFS_BIT_H__

/*
 * XFS bit manipulation routines.
 */

/*
 * masks with n high/low bits set, 32-bit values & 64-bit values
 */
#define	XFS_MASK32HI(n)		xfs_mask32hi(n)
static inline __uint32_t xfs_mask32hi(int n)
{
	return (__uint32_t)-1 << (32 - (n));
}
#define	XFS_MASK64HI(n)		xfs_mask64hi(n)
static inline __uint64_t xfs_mask64hi(int n)
{
	return (__uint64_t)-1 << (64 - (n));
}
#define	XFS_MASK32LO(n)		xfs_mask32lo(n)
static inline __uint32_t xfs_mask32lo(int n)
{
	return ((__uint32_t)1 << (n)) - 1;
}
#define	XFS_MASK64LO(n)		xfs_mask64lo(n)
static inline __uint64_t xfs_mask64lo(int n)
{
	return ((__uint64_t)1 << (n)) - 1;
}

/* Get high bit set out of 32-bit argument, -1 if none set */
extern int xfs_highbit32(__uint32_t v);

/* Get low bit set out of 64-bit argument, -1 if none set */
extern int xfs_lowbit64(__uint64_t v);

/* Get high bit set out of 64-bit argument, -1 if none set */
extern int xfs_highbit64(__uint64_t);

/* Count set bits in map starting with start_bit */
extern int xfs_count_bits(uint *map, uint size, uint start_bit);

/* Count continuous one bits in map starting with start_bit */
extern int xfs_contig_bits(uint *map, uint size, uint start_bit);

/* Find next set bit in map */
extern int xfs_next_bit(uint *map, uint size, uint start_bit);

#endif	/* __XFS_BIT_H__ */
