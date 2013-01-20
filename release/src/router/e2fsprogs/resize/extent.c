/*
 * extent.c --- ext2 extent abstraction
 *
 * This abstraction is used to provide a compact way of representing a
 * translation table, for moving multiple contiguous ranges (extents)
 * of blocks or inodes.
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000 by Theosore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "resize2fs.h"

struct ext2_extent_entry {
	__u32	old_loc, new_loc;
	int	size;
};

struct _ext2_extent {
	struct ext2_extent_entry *list;
	int	cursor;
	int	size;
	int	num;
	int	sorted;
};

/*
 * Create an extent table
 */
errcode_t ext2fs_create_extent_table(ext2_extent *ret_extent, int size)
{
	ext2_extent	extent;
	errcode_t	retval;

	retval = ext2fs_get_mem(sizeof(struct _ext2_extent), &extent);
	if (retval)
		return retval;
	memset(extent, 0, sizeof(struct _ext2_extent));

	extent->size = size ? size : 50;
	extent->cursor = 0;
	extent->num = 0;
	extent->sorted = 1;

	retval = ext2fs_get_array(sizeof(struct ext2_extent_entry),
				extent->size, &extent->list);
	if (retval) {
		ext2fs_free_mem(&extent);
		return retval;
	}
	memset(extent->list, 0,
	       sizeof(struct ext2_extent_entry) * extent->size);
	*ret_extent = extent;
	return 0;
}

/*
 * Free an extent table
 */
void ext2fs_free_extent_table(ext2_extent extent)
{
	if (extent->list)
		ext2fs_free_mem(&extent->list);
	extent->list = 0;
	extent->size = 0;
	extent->num = 0;
	ext2fs_free_mem(&extent);
}

/*
 * Add an entry to the extent table
 */
errcode_t ext2fs_add_extent_entry(ext2_extent extent, __u32 old_loc, __u32 new_loc)
{
	struct	ext2_extent_entry	*ent;
	errcode_t			retval;
	int				newsize;
	int				curr;

	if (extent->num >= extent->size) {
		newsize = extent->size + 100;
		retval = ext2fs_resize_mem(sizeof(struct ext2_extent_entry) *
					   extent->size,
					   sizeof(struct ext2_extent_entry) *
					   newsize, &extent->list);
		if (retval)
			return retval;
		extent->size = newsize;
	}
	curr = extent->num;
	ent = extent->list + curr;
	if (curr) {
		/*
		 * Check to see if this can be coalesced with the last
		 * extent
		 */
		ent--;
		if ((ent->old_loc + ent->size == old_loc) &&
		    (ent->new_loc + ent->size == new_loc)) {
			ent->size++;
			return 0;
		}
		/*
		 * Now see if we're going to ruin the sorting
		 */
		if (ent->old_loc + ent->size > old_loc)
			extent->sorted = 0;
		ent++;
	}
	ent->old_loc = old_loc;
	ent->new_loc = new_loc;
	ent->size = 1;
	extent->num++;
	return 0;
}

/*
 * Helper function for qsort
 */
static EXT2_QSORT_TYPE extent_cmp(const void *a, const void *b)
{
	const struct ext2_extent_entry *db_a;
	const struct ext2_extent_entry *db_b;

	db_a = (const struct ext2_extent_entry *) a;
	db_b = (const struct ext2_extent_entry *) b;

	return (db_a->old_loc - db_b->old_loc);
}

/*
 * Given an inode map and inode number, look up the old inode number
 * and return the new inode number.
 */
__u32 ext2fs_extent_translate(ext2_extent extent, __u32 old_loc)
{
	int	low, high, mid;
	__u32	lowval, highval;
	float	range;

	if (!extent->sorted) {
		qsort(extent->list, extent->num,
		      sizeof(struct ext2_extent_entry), extent_cmp);
		extent->sorted = 1;
	}
	low = 0;
	high = extent->num-1;
	while (low <= high) {
#if 0
		mid = (low+high)/2;
#else
		if (low == high)
			mid = low;
		else {
			/* Interpolate for efficiency */
			lowval = extent->list[low].old_loc;
			highval = extent->list[high].old_loc;

			if (old_loc < lowval)
				range = 0;
			else if (old_loc > highval)
				range = 1;
			else {
				range = ((float) (old_loc - lowval)) /
					(highval - lowval);
				if (range > 0.9)
					range = 0.9;
				if (range < 0.1)
					range = 0.1;
			}
			mid = low + ((int) (range * (high-low)));
		}
#endif
		if ((old_loc >= extent->list[mid].old_loc) &&
		    (old_loc < extent->list[mid].old_loc + extent->list[mid].size))
			return (extent->list[mid].new_loc +
				(old_loc - extent->list[mid].old_loc));
		if (old_loc < extent->list[mid].old_loc)
			high = mid-1;
		else
			low = mid+1;
	}
	return 0;
}

/*
 * For debugging only
 */
void ext2fs_extent_dump(ext2_extent extent, FILE *out)
{
	int	i;
	struct ext2_extent_entry *ent;

	fputs(_("# Extent dump:\n"), out);
	fprintf(out, _("#\tNum=%d, Size=%d, Cursor=%d, Sorted=%d\n"),
	       extent->num, extent->size, extent->cursor, extent->sorted);
	for (i=0, ent=extent->list; i < extent->num; i++, ent++) {
		fprintf(out, _("#\t\t %u -> %u (%d)\n"), ent->old_loc,
			ent->new_loc, ent->size);
	}
}

/*
 * Iterate over the contents of the extent table
 */
errcode_t ext2fs_iterate_extent(ext2_extent extent, __u32 *old_loc,
				__u32 *new_loc, int *size)
{
	struct ext2_extent_entry *ent;

	if (!old_loc) {
		extent->cursor = 0;
		return 0;
	}

	if (extent->cursor >= extent->num) {
		*old_loc = 0;
		*new_loc = 0;
		*size = 0;
		return 0;
	}

	ent = extent->list + extent->cursor++;

	*old_loc = ent->old_loc;
	*new_loc = ent->new_loc;
	*size = ent->size;
	return 0;
}




