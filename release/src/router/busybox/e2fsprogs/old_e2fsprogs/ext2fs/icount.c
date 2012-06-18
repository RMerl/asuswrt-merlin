/* vi: set sw=4 ts=4: */
/*
 * icount.c --- an efficient inode count abstraction
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * The data storage strategy used by icount relies on the observation
 * that most inode counts are either zero (for non-allocated inodes),
 * one (for most files), and only a few that are two or more
 * (directories and files that are linked to more than one directory).
 *
 * Also, e2fsck tends to load the icount data sequentially.
 *
 * So, we use an inode bitmap to indicate which inodes have a count of
 * one, and then use a sorted list to store the counts for inodes
 * which are greater than one.
 *
 * We also use an optional bitmap to indicate which inodes are already
 * in the sorted list, to speed up the use of this abstraction by
 * e2fsck's pass 2.  Pass 2 increments inode counts as it finds them,
 * so this extra bitmap avoids searching the sorted list to see if a
 * particular inode is on the sorted list already.
 */

struct ext2_icount_el {
	ext2_ino_t	ino;
	__u16	count;
};

struct ext2_icount {
	errcode_t		magic;
	ext2fs_inode_bitmap	single;
	ext2fs_inode_bitmap	multiple;
	ext2_ino_t		count;
	ext2_ino_t		size;
	ext2_ino_t		num_inodes;
	ext2_ino_t		cursor;
	struct ext2_icount_el	*list;
};

void ext2fs_free_icount(ext2_icount_t icount)
{
	if (!icount)
		return;

	icount->magic = 0;
	ext2fs_free_mem(&icount->list);
	ext2fs_free_inode_bitmap(icount->single);
	ext2fs_free_inode_bitmap(icount->multiple);
	ext2fs_free_mem(&icount);
}

errcode_t ext2fs_create_icount2(ext2_filsys fs, int flags, unsigned int size,
				ext2_icount_t hint, ext2_icount_t *ret)
{
	ext2_icount_t	icount;
	errcode_t	retval;
	size_t		bytes;
	ext2_ino_t	i;

	if (hint) {
		EXT2_CHECK_MAGIC(hint, EXT2_ET_MAGIC_ICOUNT);
		if (hint->size > size)
			size = (size_t) hint->size;
	}

	retval = ext2fs_get_mem(sizeof(struct ext2_icount), &icount);
	if (retval)
		return retval;
	memset(icount, 0, sizeof(struct ext2_icount));

	retval = ext2fs_allocate_inode_bitmap(fs, 0,
					      &icount->single);
	if (retval)
		goto errout;

	if (flags & EXT2_ICOUNT_OPT_INCREMENT) {
		retval = ext2fs_allocate_inode_bitmap(fs, 0,
						      &icount->multiple);
		if (retval)
			goto errout;
	} else
		icount->multiple = 0;

	if (size) {
		icount->size = size;
	} else {
		/*
		 * Figure out how many special case inode counts we will
		 * have.  We know we will need one for each directory;
		 * we also need to reserve some extra room for file links
		 */
		retval = ext2fs_get_num_dirs(fs, &icount->size);
		if (retval)
			goto errout;
		icount->size += fs->super->s_inodes_count / 50;
	}

	bytes = (size_t) (icount->size * sizeof(struct ext2_icount_el));
	retval = ext2fs_get_mem(bytes, &icount->list);
	if (retval)
		goto errout;
	memset(icount->list, 0, bytes);

	icount->magic = EXT2_ET_MAGIC_ICOUNT;
	icount->count = 0;
	icount->cursor = 0;
	icount->num_inodes = fs->super->s_inodes_count;

	/*
	 * Populate the sorted list with those entries which were
	 * found in the hint icount (since those are ones which will
	 * likely need to be in the sorted list this time around).
	 */
	if (hint) {
		for (i=0; i < hint->count; i++)
			icount->list[i].ino = hint->list[i].ino;
		icount->count = hint->count;
	}

	*ret = icount;
	return 0;

errout:
	ext2fs_free_icount(icount);
	return retval;
}

errcode_t ext2fs_create_icount(ext2_filsys fs, int flags,
			       unsigned int size,
			       ext2_icount_t *ret)
{
	return ext2fs_create_icount2(fs, flags, size, 0, ret);
}

/*
 * insert_icount_el() --- Insert a new entry into the sorted list at a
 *	specified position.
 */
static struct ext2_icount_el *insert_icount_el(ext2_icount_t icount,
					    ext2_ino_t ino, int pos)
{
	struct ext2_icount_el	*el;
	errcode_t		retval;
	ext2_ino_t			new_size = 0;
	int			num;

	if (icount->count >= icount->size) {
		if (icount->count) {
			new_size = icount->list[(unsigned)icount->count-1].ino;
			new_size = (ext2_ino_t) (icount->count *
				((float) icount->num_inodes / new_size));
		}
		if (new_size < (icount->size + 100))
			new_size = icount->size + 100;
		retval = ext2fs_resize_mem((size_t) icount->size *
					   sizeof(struct ext2_icount_el),
					   (size_t) new_size *
					   sizeof(struct ext2_icount_el),
					   &icount->list);
		if (retval)
			return 0;
		icount->size = new_size;
	}
	num = (int) icount->count - pos;
	if (num < 0)
		return 0;	/* should never happen */
	if (num) {
		memmove(&icount->list[pos+1], &icount->list[pos],
			sizeof(struct ext2_icount_el) * num);
	}
	icount->count++;
	el = &icount->list[pos];
	el->count = 0;
	el->ino = ino;
	return el;
}

/*
 * get_icount_el() --- given an inode number, try to find icount
 *	information in the sorted list.  If the create flag is set,
 *	and we can't find an entry, create one in the sorted list.
 */
static struct ext2_icount_el *get_icount_el(ext2_icount_t icount,
					    ext2_ino_t ino, int create)
{
	float	range;
	int	low, high, mid;
	ext2_ino_t	lowval, highval;

	if (!icount || !icount->list)
		return 0;

	if (create && ((icount->count == 0) ||
		       (ino > icount->list[(unsigned)icount->count-1].ino))) {
		return insert_icount_el(icount, ino, (unsigned) icount->count);
	}
	if (icount->count == 0)
		return 0;

	if (icount->cursor >= icount->count)
		icount->cursor = 0;
	if (ino == icount->list[icount->cursor].ino)
		return &icount->list[icount->cursor++];
	low = 0;
	high = (int) icount->count-1;
	while (low <= high) {
		if (low == high)
			mid = low;
		else {
			/* Interpolate for efficiency */
			lowval = icount->list[low].ino;
			highval = icount->list[high].ino;

			if (ino < lowval)
				range = 0;
			else if (ino > highval)
				range = 1;
			else
				range = ((float) (ino - lowval)) /
					(highval - lowval);
			mid = low + ((int) (range * (high-low)));
		}
		if (ino == icount->list[mid].ino) {
			icount->cursor = mid+1;
			return &icount->list[mid];
		}
		if (ino < icount->list[mid].ino)
			high = mid-1;
		else
			low = mid+1;
	}
	/*
	 * If we need to create a new entry, it should be right at
	 * low (where high will be left at low-1).
	 */
	if (create)
		return insert_icount_el(icount, ino, low);
	return 0;
}

errcode_t ext2fs_icount_validate(ext2_icount_t icount, FILE *out)
{
	errcode_t	ret = 0;
	unsigned int	i;
	const char *bad = "bad icount";

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (icount->count > icount->size) {
		fprintf(out, "%s: count > size\n", bad);
		return EXT2_ET_INVALID_ARGUMENT;
	}
	for (i=1; i < icount->count; i++) {
		if (icount->list[i-1].ino >= icount->list[i].ino) {
			fprintf(out, "%s: list[%d].ino=%u, list[%d].ino=%u\n",
				bad, i-1, icount->list[i-1].ino,
				i, icount->list[i].ino);
			ret = EXT2_ET_INVALID_ARGUMENT;
		}
	}
	return ret;
}

errcode_t ext2fs_icount_fetch(ext2_icount_t icount, ext2_ino_t ino, __u16 *ret)
{
	struct ext2_icount_el	*el;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		*ret = 1;
		return 0;
	}
	if (icount->multiple &&
	    !ext2fs_test_inode_bitmap(icount->multiple, ino)) {
		*ret = 0;
		return 0;
	}
	el = get_icount_el(icount, ino, 0);
	if (!el) {
		*ret = 0;
		return 0;
	}
	*ret = el->count;
	return 0;
}

errcode_t ext2fs_icount_increment(ext2_icount_t icount, ext2_ino_t ino,
				  __u16 *ret)
{
	struct ext2_icount_el	*el;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		/*
		 * If the existing count is 1, then we know there is
		 * no entry in the list.
		 */
		el = get_icount_el(icount, ino, 1);
		if (!el)
			return EXT2_ET_NO_MEMORY;
		ext2fs_unmark_inode_bitmap(icount->single, ino);
		el->count = 2;
	} else if (icount->multiple) {
		/*
		 * The count is either zero or greater than 1; if the
		 * inode is set in icount->multiple, then there should
		 * be an entry in the list, so find it using
		 * get_icount_el().
		 */
		if (ext2fs_test_inode_bitmap(icount->multiple, ino)) {
			el = get_icount_el(icount, ino, 1);
			if (!el)
				return EXT2_ET_NO_MEMORY;
			el->count++;
		} else {
			/*
			 * The count was zero; mark the single bitmap
			 * and return.
			 */
		zero_count:
			ext2fs_mark_inode_bitmap(icount->single, ino);
			if (ret)
				*ret = 1;
			return 0;
		}
	} else {
		/*
		 * The count is either zero or greater than 1; try to
		 * find an entry in the list to determine which.
		 */
		el = get_icount_el(icount, ino, 0);
		if (!el) {
			/* No entry means the count was zero */
			goto zero_count;
		}
		el = get_icount_el(icount, ino, 1);
		if (!el)
			return EXT2_ET_NO_MEMORY;
		el->count++;
	}
	if (icount->multiple)
		ext2fs_mark_inode_bitmap(icount->multiple, ino);
	if (ret)
		*ret = el->count;
	return 0;
}

errcode_t ext2fs_icount_decrement(ext2_icount_t icount, ext2_ino_t ino,
				  __u16 *ret)
{
	struct ext2_icount_el	*el;

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (ext2fs_test_inode_bitmap(icount->single, ino)) {
		ext2fs_unmark_inode_bitmap(icount->single, ino);
		if (icount->multiple)
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		else {
			el = get_icount_el(icount, ino, 0);
			if (el)
				el->count = 0;
		}
		if (ret)
			*ret = 0;
		return 0;
	}

	if (icount->multiple &&
	    !ext2fs_test_inode_bitmap(icount->multiple, ino))
		return EXT2_ET_INVALID_ARGUMENT;

	el = get_icount_el(icount, ino, 0);
	if (!el || el->count == 0)
		return EXT2_ET_INVALID_ARGUMENT;

	el->count--;
	if (el->count == 1)
		ext2fs_mark_inode_bitmap(icount->single, ino);
	if ((el->count == 0) && icount->multiple)
		ext2fs_unmark_inode_bitmap(icount->multiple, ino);

	if (ret)
		*ret = el->count;
	return 0;
}

errcode_t ext2fs_icount_store(ext2_icount_t icount, ext2_ino_t ino,
			      __u16 count)
{
	struct ext2_icount_el	*el;

	if (!ino || (ino > icount->num_inodes))
		return EXT2_ET_INVALID_ARGUMENT;

	EXT2_CHECK_MAGIC(icount, EXT2_ET_MAGIC_ICOUNT);

	if (count == 1) {
		ext2fs_mark_inode_bitmap(icount->single, ino);
		if (icount->multiple)
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		return 0;
	}
	if (count == 0) {
		ext2fs_unmark_inode_bitmap(icount->single, ino);
		if (icount->multiple) {
			/*
			 * If the icount->multiple bitmap is enabled,
			 * we can just clear both bitmaps and we're done
			 */
			ext2fs_unmark_inode_bitmap(icount->multiple, ino);
		} else {
			el = get_icount_el(icount, ino, 0);
			if (el)
				el->count = 0;
		}
		return 0;
	}

	/*
	 * Get the icount element
	 */
	el = get_icount_el(icount, ino, 1);
	if (!el)
		return EXT2_ET_NO_MEMORY;
	el->count = count;
	ext2fs_unmark_inode_bitmap(icount->single, ino);
	if (icount->multiple)
		ext2fs_mark_inode_bitmap(icount->multiple, ino);
	return 0;
}

ext2_ino_t ext2fs_get_icount_size(ext2_icount_t icount)
{
	if (!icount || icount->magic != EXT2_ET_MAGIC_ICOUNT)
		return 0;

	return icount->size;
}
